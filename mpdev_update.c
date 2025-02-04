/*
 * $Log: mpdev_update.c,v $
 * Revision 1.10  2025-01-26 16:45:59+05:30  Cprogrammer
 * fix gcc14 errors
 *
 * Revision 1.9  2022-06-20 01:04:16+05:30  Cprogrammer
 * remove extra arguments passed to print_song
 *
 * Revision 1.8  2022-05-10 21:31:50+05:30  Cprogrammer
 * use tcpopen from standard include path
 *
 * Revision 1.7  2021-10-01 12:05:27+05:30  Cprogrammer
 * removed name column from song table in stats db
 *
 * Revision 1.6  2021-09-29 19:16:58+05:30  Cprogrammer
 * added last_modified column to sticker table
 *
 * Revision 1.5  2020-08-11 14:02:11+05:30  Cprogrammer
 * adjust date_added for localtime
 *
 * Revision 1.4  2020-08-02 09:11:14+05:30  Cprogrammer
 * set default values for play_count, rating, date_added fields.
 * use %mtime% tag from mpd datatabase for date_added filed instead of using stat(2) call
 *
 * Revision 1.3  2020-07-28 12:42:03+05:30  Cprogrammer
 * added updation of Disc tag
 *
 * Revision 1.2  2020-07-24 09:41:15+05:30  Cprogrammer
 * removed requirement of -m option for update mode
 *
 * Revision 1.1  2020-07-19 18:15:46+05:30  Cprogrammer
 * Initial revision
 *
 *
 *
 * mpdev_update - utility for maintaining stats/sticker sqlite db
 *
 * Full documentation of mpd commands at https://www.musicpd.org/doc/html/protocol.html
 *
 * Optimization tips from
 * https://www.whoishostingthis.com/compare/sqlite/optimize/
 * https://www.wassen.net/sqlite-c.html
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#endif
#include <timeoutread.h>
#include <timeoutwrite.h>
#include <substdio.h>
#include <getln.h>
#include <stralloc.h>
#include <fmt.h>
#include <env.h>
#include <error.h>
#include <strerr.h>
#include <sgetopt.h>
#include <scan.h>
#include <str.h>
#include <env.h>
#include <tcpopen.h>

#ifndef	lint
static char     sccsid[] = "$Id: mpdev_update.c,v 1.10 2025-01-26 16:45:59+05:30 Cprogrammer Exp mbhangui $";
#endif

extern char    *strptime(const char *, const char *, struct tm *);
ssize_t         safewrite(int, char *, size_t);
ssize_t         saferead(int, char *, size_t);

substdio        mpdin, mpdout, ssout, sserr;
static stralloc uri = {0}, last_modified = {0}, album = {0}, artist = {0},
				date = {0}, genre = {0}, title = {0}, track = {0},
				disc = {0}, duration = {0}, line = {0};
char            strnum[FMT_ULONG];
int             timeout = 1200, verbose, db_type = -1, do_update = 0;
static sqlite3 *db;
char           *usage =
				"usage: mpdev_update [-i IP/Host | -s unix_socket] [-p port]\n"
				" -i IP     - IP address of MPD host. default 127.0.0.1\n"
				" -p port   - MPD listening port. default 6600\n"
				" -s socket - unix domain socket path\n"
				" -d        - sqlite3 dbfile path\n"
				" -j        - Enable Journal in Memory\n"
				" -S        - Enable Synch Mode\n"
				" -P        - Print sql statment\n"
				" -t        - Enable Transaction Mode\n"
				" -D 0|1    - insert stats | insert sticker (insert new records)\n"
				" -U        - update stats (insert new records + update old records)\n"
				" -v        - verbose output";

void
die_write(char *arg)
{
	if (db)
		sqlite3_close(db);
	substdio_puts(&sserr, "Requested action aborted: write error");
	if (arg) {
		substdio_put(&sserr, ": ", 2);
		substdio_puts(&sserr, arg);
	}
	substdio_put(&sserr, "\n", 1);
	substdio_flush(&sserr);
	_exit(111);
}

void
out(char *s)
{
	if (substdio_puts(&ssout, s) == -1)
		die_write(0);
}

void
flush()
{
	if (substdio_flush(&ssout) == -1)
		die_write("stdout");
}

void
err_out(char *s)
{
	if (substdio_puts(&sserr, s) == -1)
		die_write(0);
}

void
err_flush()
{
	if (substdio_flush(&sserr) == -1)
		die_write("sserr");
}

void
die_read(char *arg)
{
	if (db)
		sqlite3_close(db);
	if (arg) {
		substdio_put(&sserr, "Requested action aborted: ", 26);
		substdio_puts(&sserr, arg);
		substdio_put(&sserr, ": read error\n", 13);
	} else
		substdio_puts(&sserr, "Requested action aborted: read error\n");
	substdio_flush(&sserr);
	_exit(111);
}

void
die_alarm()
{
	if (db)
		sqlite3_close(db);
	substdio_puts(&sserr, "Requested action aborted: timeout\n");
	substdio_flush(&sserr);
	_exit(111);
}

void
die_nomem()
{
	if (db)
		sqlite3_close(db);
	substdio_puts(&sserr, "mpdev_update: out of memory\n");
	substdio_flush(&sserr);
	_exit(111);
}

ssize_t
saferead(int fd, char *buf, size_t len)
{
	int             r;

	if ((r = timeoutread(timeout, fd, buf, len)) == -1) {
		if (errno == error_timeout)
			die_alarm();
	}
	if (r < 0)
		die_read(0);
	return r;
}

ssize_t
safewrite(int fd, char *buf, size_t len)
{
	int             r;

	if ((r = timeoutwrite(timeout, fd, buf, len)) <= 0)
		die_write(0);
	return r;
}

void
print_missing(char *uri, char *what)
{
	err_out("missing information in uri: ");
	err_out(uri);
	err_out(": ");
	err_out(what);
	err_out("\n");
	err_flush();
}

void
print_song()
{
	if (verbose) {
		out("uri: [");
		out(uri.s);
	}
	if (last_modified.len) {
		if (verbose) {
			out("], ");
			out("Last-Modified: [");
			out(last_modified.s);
		}
	} else
		print_missing(uri.s, "Last-Modified");
	if (duration.len) {
		if (verbose) {
			out("], ");
			out("Duration: [");
			out(duration.s);
		}
	} else
		print_missing(uri.s, "Duration");
	if (title.len) {
		if (verbose) {
			out("], ");
			out("Title: [");
			out(title.s);
		}
	} else
		print_missing(uri.s, "Title");
	if (artist.len) {
		if (verbose) {
			out("], ");
			out("Artist: [");
			out(artist.s);
		}
	} else
		print_missing(uri.s, "Artist");
	if (album.len) {
		if (verbose) {
			out("], ");
			out("Album: [");
			out(album.s);
		}
	} /*else
		print_missing(uri.s, "Album"); */
	if (track.len) {
		if (verbose) {
			out("], ");
			out("Track: [");
			out(track.s);
		}
	} /*else
		print_missing(uri.s, "Track"); */
	if (disc.len) {
		if (verbose) {
			out("], ");
			out("Disc: [");
			out(disc.s);
		}
	} /*else
		print_missing(uri.s, "Disc"); */
	if (genre.len) {
		if (verbose) {
			out("], ");
			out("Genre: [");
			out(genre.s);
		}
	} /*else
		print_missing(uri.s, "Genre"); */
	if (date.len) {
		if (verbose) {
			out("], ");
			out("Date: [");
			out(date.s);
		}
	} /*else
		print_missing(uri.s, "Date"); */
	if (verbose) {
		out("]\n");
		flush();
	}
	return;
}

sqlite3_stmt   *
stats_database_init(char *database, int synch_mode, int journal_in_memory, int transaction_mode)
{
	char           *sql, *err_msg;
	sqlite3_stmt   *res;

	if (sqlite3_open(database, &db) != SQLITE_OK)
		strerr_die3x(111, database, ": ", (char *) sqlite3_errmsg(db));
	if (!synch_mode) {
		if (verbose) {
			out("PRAGMA synchronous  = OFF\n");
			flush();
		}
		if (sqlite3_exec(db, "PRAGMA synchronous = OFF", NULL, NULL, &err_msg) != SQLITE_OK) {
			sqlite3_close(db);
			strerr_die3x(111, database, ": PRAGMA synchronous = OFF: ", err_msg);
		}
		if (err_msg)
			sqlite3_free(err_msg);
	} else
	if (verbose) {
		out("PRAGMA synchronous  = NORMAL\n");
		flush();
	}
	if (journal_in_memory) {
		if (verbose) {
			out("PRAGMA journal_mode = MEMORY\n");
			flush();
		}
		if (sqlite3_exec(db, "PRAGMA journal_mode = MEMORY", NULL, NULL, &err_msg) != SQLITE_OK) {
			sqlite3_close(db);
			strerr_die3x(111, database, ": PRAGMA journal_mode = MEMORY: ", err_msg);
		}
		if (err_msg)
			sqlite3_free(err_msg);
	} else
	if (verbose) {
		out("PRAGMA journal_mode = DELETE\n");
		flush();
	}
	if (transaction_mode) {
		if (verbose) {
			out("BEGIN TRANSACTION\n");
			flush();
		}
		if (sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, &err_msg) != SQLITE_OK) {
			sqlite3_close(db);
			strerr_die3x(111, database, ": BEGIN TRANSACTION: ", err_msg);
		}
		if (err_msg)
			sqlite3_free(err_msg);
	}
	if (db_type == 0) {
		sql = "DROP INDEX IF EXISTS rating;\n"
			  "DROP INDEX IF EXISTS uri;\n"
			  "DROP INDEX IF EXISTS last_played;\n"
			  "DROP INDEX IF EXISTS date_added;\n"
			  "DROP INDEX IF EXISTS last_modified;";
	} else {
		sql = "DROP INDEX IF EXISTS rating;\n"
			  "DROP INDEX IF EXISTS uri;\n";
	}
	if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
		sqlite3_close(db);
		strerr_die5x(111, database, ": ", sql, ": ", err_msg);
	}
	if (verbose) {
		if (sqlite3_changes(db) == 1) {
			out(sql);
			out("\n");
			flush();
		}
	}
	if (err_msg)
		sqlite3_free(err_msg);
	if (db_type == 0) {
		sql = "CREATE TABLE IF NOT EXISTS song(\n"
        	"id              INTEGER PRIMARY KEY,\n"
			"play_count      INTEGER DEFAULT 0,\n"
			"rating          INTEGER DEFAULT 0,\n"
        	"uri             TEXT UNIQUE NOT NULL,\n"
        	"duration        INTEGER,\n"
        	"last_modified   INTEGER,\n"
			"date_added      INTEGER DEFAULT (strftime('%s','now')),\n"
        	"artist          TEXT,\n"
        	"album           TEXT,\n"
        	"title           TEXT,\n"
        	"track           TEXT,\n"
        	"genre           TEXT,\n"
        	"date            TEXT,\n"
        	"composer        TEXT,\n"
        	"performer       TEXT,\n"
        	"disc            TEXT,\n"
        	"last_played     INTEGER,\n"
        	"karma           INTEGER NOT NULL CONSTRAINT karma_percent CHECK (karma >= 0 AND karma <= 100) DEFAULT 50\n"
		");";
	} else {
		sql = "CREATE TABLE IF NOT EXISTS sticker(\n"
        	"type  VARCHAR NOT NULL,\n"
        	"uri   VARCHAR NOT NULL,\n"
        	"name  VARCHAR NOT NULL,\n"
        	"value VARCHAR NOT NULL,\n"
			"last_modified INTEGER DEFAULT (strftime('%Y-%m-%d %H:%M:%S:%s','now', 'localtime'))\n"
		");";
	}
	if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
		sqlite3_close(db);
		strerr_die5x(111, database, ": ", sql, ": ", err_msg);
	}
	if (verbose) {
		if (sqlite3_changes(db) == 1) {
			out(sql);
			out("\n");
			flush();
		}
	}
	if (err_msg)
		sqlite3_free(err_msg);
	if (db_type == 0) {
		if (do_update == 0) /*- insert */
			sql = "INSERT or IGNORE into song (uri, artist, album, title, track, "
				"genre, date, date_added, last_modified, duration) "
			  	"values (@uri, @artist, @album, @title, @track, @genre, @date, "
				"@added, @modified, @duration)";
		else /*- update stats for any metadata updation */
			sql = "UPDATE song set artist=@artist, album=@album, title=@title, "
				"track=@track, genre=@genre, date=@date, last_modified=@modified, "
				"duration=@duration where uri=@uri";
	} else
		sql = "INSERT or IGNORE into sticker (type, uri, name, value)"
			  " values ('song', @uri, 'rating', 0)";
	if (sqlite3_prepare_v2(db, sql, -1, &res, 0) != SQLITE_OK) {
		sqlite3_close(db);
		strerr_die5x(111, database, ": sqlite3_prepare_v2: ", sql, ": ", (char *) sqlite3_errmsg(db));
	}
	return (res);
}

void
insert_update_data(sqlite3_stmt *res, unsigned long *processed, unsigned long *failure, char **ptr)
{
	struct tm       tm = {0};
	int             i;
	time_t          mod_time;

	if (db_type == 0) {
		if (!strptime(last_modified.s, "%Y-%m-%dT%H:%M:%SZ", &tm)) {
			strerr_warn4("uri: ", uri.s, ": invalid timestamp: ", last_modified.s, 0);
			return;
		} else
		if (!(mod_time = mktime(&tm))) {
			strerr_warn4("uri: ", uri.s, ": invalid timestamp: ", last_modified.s, 0);
			return;
		}
		if (do_update == 0) { /*- insert */
			sqlite3_bind_text(res,  1, uri.len ? uri.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(res,  2, artist.len ? artist.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(res,  3, album.len ? album.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(res,  4, title.len ? title.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(res,  5, track.len ? track.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(res,  6, genre.len ? genre.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(res,  7, date.len ? date.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_int(res,   8, mod_time - timezone); /*- mod_time is in GMT */
			sqlite3_bind_int(res,   9, mod_time);
			sqlite3_bind_text(res, 10, duration.s, -1, SQLITE_STATIC);
		} else { /*- update stats for any metadata updation */
			i = 1;
			sqlite3_bind_text(res, i++, artist.len ? artist.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(res, i++, album.len ? album.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(res, i++, title.len ? title.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(res, i++, track.len ? track.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(res, i++, genre.len ? genre.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(res, i++, date.len ? date.s : "", -1, SQLITE_STATIC);
			sqlite3_bind_int(res,  i++, mod_time);
			sqlite3_bind_text(res, i++, duration.s, -1, SQLITE_STATIC);
			sqlite3_bind_text(res, i++, uri.len ? uri.s : "", -1, SQLITE_STATIC);
			/*-
			sql = "UPDATE song set artist=@artist, album=@album, title=@title, track=@track, "
				"genre=@genre, date=@date, date_added=@added, last_modified=@modified, "
				"duration=@duration where uri=@uri";
			*/
		}
	} else
		sqlite3_bind_text(res, 1, uri.len ? uri.s : "", -1, SQLITE_STATIC);
	if (ptr)
		*ptr = sqlite3_expanded_sql(res);
	if (sqlite3_step(res) != SQLITE_DONE) {
		*failure += 1;
	} else
		*processed += 1;
	sqlite3_clear_bindings(res);
	sqlite3_reset(res);
}

int
stats_database_end(char *database, sqlite3_stmt *res, int transaction_mode)
{
	char           *sql, *err_msg;
	int             i;

	if (db_type == 0) {
		sql = "CREATE INDEX IF NOT EXISTS rating        on song(rating);\n"
			  "CREATE INDEX IF NOT EXISTS uri           on song(uri);\n"
			  "CREATE INDEX IF NOT EXISTS last_played   on song(last_played);\n"
			  "CREATE INDEX IF NOT EXISTS date_added    on song(date_added);\n"
			  "CREATE INDEX IF NOT EXISTS last_modified on song(last_modified);";
	} else {
		sql = "CREATE INDEX IF NOT EXISTS rating        on sticker(value);\n"
			  "CREATE INDEX IF NOT EXISTS uri           on sticker(uri);\n"
			  "CREATE INDEX IF NOT EXISTS last          on sticker(last_modified);\n";
	}
	if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
		sqlite3_close(db);
		strerr_die5x(111, database, ": ", sql, ": ", err_msg);
	}
	if (verbose) {
		if (sqlite3_changes(db) == 1) {
			out(sql);
			out("\n");
			flush();
		}
	}
	if (err_msg)
		sqlite3_free(err_msg);
	if (transaction_mode) {
		if (verbose) {
			out("END TRANSACTION\n");
			flush();
		}
		if (sqlite3_exec(db, "END TRANSACTION", 0, 0, &err_msg) != SQLITE_OK) {
			sqlite3_close(db);
			strerr_die3x(111, database, ": end transaction: ", err_msg);
		}
		if (err_msg)
			sqlite3_free(err_msg);
	}
	i = sqlite3_total_changes(db);
	if (sqlite3_finalize(res) != SQLITE_OK)
		strerr_warn2("uri: ", "sqlite3_finalize: ", (char *) sqlite3_errmsg(db));
	sqlite3_close(db);
	return (i);
}

int
main(int argc, char **argv)
{
	int             opt, match, sock, port_num = 6600, is_directory = 0;
	char            synch_mode = 1, journal_in_memory = 0, transaction_mode = 0, print_sql = 0;
	unsigned long   processed, failure, row_count;
	time_t          t;
	struct tm       tm = {0};
	char            port[FMT_ULONG], mpdinbuf[1024], mpdoutbuf[512], ssoutbuf[512], sserrbuf[512];
	char           *mpd_socket, *mpd_host, *ptr, *database;
	sqlite3_stmt   *res;

	substdio_fdbuf(&ssout, (ssize_t (*)(int,  char *, size_t)) write, 1, ssoutbuf, sizeof(sserrbuf));
	substdio_fdbuf(&sserr, (ssize_t (*)(int,  char *, size_t)) write, 2, sserrbuf, sizeof(sserrbuf));
	if (!(mpd_host = env_get("MPD_HOST")))
		mpd_host = "127.0.0.1";
	mpd_socket = env_get("MPD_SOCKET");
	if (!(ptr = env_get("MPD_PORT")))
		port_num = 6600;
	else
		scan_uint(ptr, (unsigned int *) &port_num);
	database = (char *) 0;
	while ((opt = getopt(argc, argv, "vjSUPth:p:s:d:D:")) != opteof) {
		switch (opt) {
		case 'd':
			database = optarg;
			break;
		case 'h':
			mpd_host = optarg;
			break;
		case 's':
			mpd_socket = optarg;
			break;
		case 'p':
			scan_uint(optarg, (unsigned int *) &port_num);
			break;
		case 'j':
			journal_in_memory = 1;
			break;
		case 'S':
			synch_mode = 0;
			break;
		case 't':
			transaction_mode = 1;
			break;
		case 'P':
			print_sql = 1;
			break;
		case 'D':
			scan_uint(optarg, (unsigned int *) &db_type);
			break;
		case 'U':
			do_update = 1;
			break;
		case 'v':
			verbose++;
			break;
		default:
			strerr_die1x(100, usage);
		}
	}
	t = time(0);
	localtime_r(&t, &tm);
	if (!env_put2("TZ", (char *) tm.tm_zone))
		die_nomem();
	if (mpd_socket && port_num != 6600)
		strerr_die1x(100, "you can't specify both socket & port");
	if (mpd_socket && str_diff(mpd_host, "127.0.0.1"))
		strerr_die1x(100, "you can't specify both socket & IP");
	if (!database) {
		strerr_warn1("mpdev_update: database (-d option) not specified", 0);
		strerr_die1x(100, usage);
	}
	if (db_type == -1) {
		strerr_warn1("mpdev_update: db type (-D option) not specified", 0);
		strerr_die1x(100, usage);
	}
	port[fmt_ulong(port, port_num)] = 0;
	if ((sock = tcpopen(mpd_socket ? mpd_socket : mpd_host, 0, port_num)) == -1) {
		if (mpd_socket)
			strerr_die4sys(111, "mpdev_update: tcpopen: ", "socket [", mpd_socket, "]");
		else
			strerr_die6sys(111, "mpdev_update: tcpopen: ", "host [", mpd_host, "] port [", port, "]");
	}
	substdio_fdbuf(&mpdin, saferead, sock, mpdinbuf, sizeof mpdinbuf);
	substdio_fdbuf(&mpdout, safewrite, sock, mpdoutbuf, sizeof mpdoutbuf);
	if (substdio_put(&mpdout, "listallinfo\n", 12) || substdio_flush(&mpdout) == -1)
		die_write("unable to write to mpd");
	res = stats_database_init(database, synch_mode, journal_in_memory, transaction_mode);
	uri.len = last_modified.len = duration.len = artist.len = album.len = title.len = track.len = genre.len = date.len = 0;
	for (processed = failure = 0;;) {
		if (getln(&mpdin, &line, &match, '\n') == -1)
			die_read("getln");
		if (!match && line.len == 0)
			break;
		if (!str_diffn(line.s, "OK\n", 3))
			break;
		line.s[--line.len] = 0; /*- remove newline */
		if (!str_diffn(line.s, "directory: ", 11))
			is_directory = 1;
		if (!str_diffn(line.s, "file: ", 6)) {
			is_directory = 0;
			if (uri.len) {
				insert_update_data(res, &processed, &failure, print_sql ? &ptr : 0);
				if (sqlite3_changes(db) == 1) {
					print_song();
					if (print_sql && ptr) {
						out(ptr);
						sqlite3_free(ptr);
						out("\n");
						flush();
					}
				}
				uri.len = last_modified.len = duration.len = artist.len = album.len = title.len = track.len = genre.len = date.len = 0;
			}
			if (!stralloc_copyb(&uri, line.s + 6, line.len - 5))
				die_nomem();
			uri.len--;
		}
		if (is_directory)
			continue;
		if (!str_diffn(line.s, "Last-Modified: ", 15)) {
			if (!stralloc_copyb(&last_modified, line.s + 15, line.len - 14))
				die_nomem();
			last_modified.len--;
		} else
		if (!str_diffn(line.s, "Time: ", 6)) {
			if (!stralloc_copyb(&duration, line.s + 6, line.len - 5))
				die_nomem();
			duration.len--;
		} else
		if (!str_diffn(line.s, "Title: ", 7)) {
			if (!stralloc_copyb(&title, line.s + 7, line.len - 6))
				die_nomem();
			title.len--;
		} else
		if (!str_diffn(line.s, "Artist: ", 8)) {
			if (!stralloc_copyb(&artist, line.s + 8, line.len - 7))
				die_nomem();
			artist.len--;
		} else
		if (!str_diffn(line.s, "Album: ", 7)) {
			if (!stralloc_copyb(&album, line.s + 7, line.len - 6))
				die_nomem();
			album.len--;
		} else
		if (!str_diffn(line.s, "Track: ", 7)) {
			if (!stralloc_copyb(&track, line.s + 7, line.len - 6))
				die_nomem();
			track.len--;
		} else
		if (!str_diffn(line.s, "Disc: ", 6)) {
			if (!stralloc_copyb(&disc, line.s + 6, line.len - 5))
				die_nomem();
			disc.len--;
		} else
		if (!str_diffn(line.s, "Genre: ", 7)) {
			if (!stralloc_copyb(&genre, line.s + 7, line.len - 6))
				die_nomem();
			genre.len--;
		} else
		if (!str_diffn(line.s, "Date: ", 6)) {
			if (!stralloc_copyb(&date, line.s + 6, line.len - 5))
				die_nomem();
			date.len--;
		}
	}
	row_count = stats_database_end(database, res, transaction_mode);
	out("Processed ");
	strnum[fmt_ulong(strnum, processed)] = 0;
	out(strnum);
	strnum[fmt_ulong(strnum, failure)] = 0;
	out(" rows, Failures ");
	out(strnum);
	strnum[fmt_ulong(strnum, row_count)] = 0;
	out(" rows, Updated ");
	out(strnum);
	out(" rows\n");
	flush();
	close(sock);
	return 0;
}
