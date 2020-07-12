/*
 * $Log: update_stats.c,v $
 * Revision 1.2  2020-07-13 01:05:17+05:30  Cprogrammer
 * massive performance improvement
 *
 * Revision 1.1  2020-07-09 13:46:22+05:30  Cprogrammer
 * Initial revision
 *
 *
 * mpdev - watch mpd events
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
#include <stdio.h>
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
#include "tcpopen.h"

extern char    *strptime(const char *, const char *, struct tm *);
ssize_t         safewrite(int, char *, int);
ssize_t         saferead(int, char *, int);

substdio        mpdin, mpdout, ssout, sserr;
stralloc        line = {0};
char            strnum[FMT_ULONG];
int             timeout = 1200, verbose;
static sqlite3 *db;
char           *usage =
				"usage: mpdev [-i IP/Host | -s unix_socket] [-p port] [-r retry_interval]\n"
				"        -i   IP   (IP address of MPD host. default 127.0.0.1)\n"
				"        -p   port (MPD listening port. default 6600)\n"
				"        -s   unix domain socket path\n"
				"        -d   sqlite3 dbfile path\n"
				"        -j   Enable Journal in Memory\n"
				"        -S   Enable Synch Mode\n"
				"        -t   Enabel Transaction Mode\n"
				"        -v   verbose output";

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
	substdio_puts(&sserr, "mpdev: out of memory\n");
	substdio_flush(&sserr);
	_exit(111);
}

ssize_t
saferead(int fd, char *buf, int len)
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
safewrite(int fd, char *buf, int len)
{
	int             r;

	if ((r = timeoutwrite(timeout, fd, buf, len)) <= 0)
		die_write(0);
	return r;
}
/*
 * file: 50 Cent - 2017 - Best of (ALAC)/01 In Da Club.m4a
 * Last-Modified: 2019-07-24T13:10:35Z
 * Format: 44100:16:2
 * Time: 193
 * duration: 193.467
 * Artist: 50 Cent
 * Album: Best of
 * Title: In Da Club
 * Track: 1
 * Genre: Hip-Hop
 * Date: 2017
 */

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

static stralloc uri = {0}, last_modified = {0}, album = {0},
	artist = {0}, date = {0}, genre = {0}, title = {0},
	track = {0}, duration = {0}, sql_str = {0};

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

void
prepare_stmt()
{
	if (!stralloc_copyb(&sql_str, "INSERT or IGNORE into song (uri, artist, album, title, track, genre, date, last_modified, duration)", 99))
		die_nomem();
	else
	if (!stralloc_catb(&sql_str, " values (@uri, @artist, @album, @title, @track, @genre, @date, @last_modified, @duration)", 89))
		die_nomem();
	else
	if (!stralloc_0(&sql_str))
		die_nomem();
	return;
}

void
insert_data(sqlite3_stmt *res, unsigned long *processed, unsigned long *failure, char **ptr)
{
	struct tm       tm = {0};
	time_t          mod_time;
	int             i;

	if (!strptime(last_modified.s, "%Y-%m-%dT%H:%M:%SZ", &tm)) {
		strerr_warn4("uri: ", uri.s, ": invalid timestamp: ", last_modified.s, 0);
		return;
	} else
	if (!(mod_time = mktime(&tm))) {
		strerr_warn4("uri: ", uri.s, ": invalid timestamp: ", last_modified.s, 0);
		return;
	}
	strnum[i = fmt_ulong(strnum, mod_time)] = 0;
	sqlite3_bind_text(res, 1, uri.len ? uri.s : "", -1, SQLITE_STATIC);
	sqlite3_bind_text(res, 2, artist.len ? artist.s : "", -1, SQLITE_STATIC);
	sqlite3_bind_text(res, 3, album.len ? album.s : "", -1, SQLITE_STATIC);
	sqlite3_bind_text(res, 4, title.len ? title.s : "", -1, SQLITE_STATIC);
	sqlite3_bind_text(res, 5, track.len ? track.s : "", -1, SQLITE_STATIC);
	sqlite3_bind_text(res, 6, genre.len ? genre.s : "", -1, SQLITE_STATIC);
	sqlite3_bind_text(res, 7, date.len ? date.s : "", -1, SQLITE_STATIC);
	sqlite3_bind_int(res, 8, mod_time ? mod_time : 0);
	sqlite3_bind_text(res, 9, duration.s, -1, SQLITE_STATIC);
	if (ptr)
		*ptr = sqlite3_expanded_sql(res);
	if (sqlite3_step(res) != SQLITE_DONE) {
		*failure += 1;
	} else
		*processed += 1;
	sqlite3_clear_bindings(res);
	sqlite3_reset(res);
}

sqlite3_stmt   *
stats_database_init(char *database, int synch_mode, int journal_in_memory, int transaction_mode)
{
	char           *sql, *err_msg;
	sqlite3_stmt   *res;

	/*----------------------- sqlite ----------------------*/
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
		sqlite3_free(err_msg);
	}
	sql = "DROP INDEX IF EXISTS rating;\n"
		  "DROP INDEX IF EXISTS uri;\n"
		  "DROP INDEX IF EXISTS last_played;\n"
		  "DROP INDEX IF EXISTS last_modified;";
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
	sqlite3_free(err_msg);
	sql = "CREATE TABLE IF NOT EXISTS song(\n"
        "id              INTEGER PRIMARY KEY,\n"
        "play_count      INTEGER,\n"
#if 0
        "love            INTEGER,\n"
        "kill            INTEGER,\n"
#endif
        "rating          INTEGER,\n"
#if 0
        "tags            TEXT NOT NULL,\n"
#endif
        "uri             TEXT UNIQUE NOT NULL,\n"
        "duration        INTEGER,\n"
        "last_modified   INTEGER,\n"
        "artist          TEXT,\n"
        "album           TEXT,\n"
        "title           TEXT,\n"
        "track           TEXT,\n"
        "name            TEXT,\n"
        "genre           TEXT,\n"
        "date            TEXT,\n"
        "composer        TEXT,\n"
        "performer       TEXT,\n"
        "disc            TEXT,\n"
        "last_played     INTEGER,\n"
#if 0
        "mb_artistid     TEXT,\n"
        "mb_albumid      TEXT,\n"
        "mb_trackid      TEXT,\n"
#endif
        "karma           INTEGER NOT NULL CONSTRAINT karma_percent CHECK (karma >= 0 AND karma <= 100) DEFAULT 50\n"
	");";
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
	sqlite3_free(err_msg);
	prepare_stmt();
	if (sqlite3_prepare_v2(db, sql_str.s, -1, &res, 0) != SQLITE_OK) {
		sqlite3_close(db);
		strerr_die3x(111, database, ": sqlite3_prepare_v2: ", (char *) sqlite3_errmsg(db));
	}
	/*----------------------- sqlite ----------------------*/
	return (res);
}

int
stats_database_end(char *database, int transaction_mode)
{
	char           *sql, *err_msg;

	/*----------------------- sqlite ----------------------*/
	sql = "CREATE INDEX IF NOT EXISTS rating        on song(rating);\n"
		  "CREATE INDEX IF NOT EXISTS uri           on song(uri);\n"
		  "CREATE INDEX IF NOT EXISTS last_played   on song(last_played);\n"
		  "CREATE INDEX IF NOT EXISTS last_modified on song(last_modified);";
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
		sqlite3_free(err_msg);
	}
	return(sqlite3_total_changes(db));
	sqlite3_close(db);
	/*----------------------- sqlite ----------------------*/
}

int
main(int argc, char **argv)
{
	int             opt, match, sock, port_num = 6600;
	char            synch_mode = 1, journal_in_memory = 0, transaction_mode = 0, print_sql = 0;
	unsigned long   processed, failure, row_count;
	time_t          t;
	struct tm       tm = {0};
	char            port[FMT_ULONG], mpdinbuf[1024], mpdoutbuf[512], ssoutbuf[512], sserrbuf[512];
	char           *mpd_socket, *mpd_host, *ptr, *database;
	sqlite3_stmt   *res;

	substdio_fdbuf(&ssout, write, 1, ssoutbuf, sizeof(sserrbuf));
	substdio_fdbuf(&sserr, write, 2, sserrbuf, sizeof(sserrbuf));
	if (!(mpd_host = env_get("MPD_HOST")))
		mpd_host = "127.0.0.1";
	mpd_socket = env_get("MPD_SOCKET");
	if (!(ptr = env_get("MPD_PORT")))
		port_num = 6600;
	else
		scan_uint(ptr, (unsigned int *) &port_num);
	database = (char *) 0;
	while ((opt = getopt(argc, argv, "vjSPth:p:s:d:")) != opteof) {
		switch (opt) {
		case 'h':
			mpd_host = optarg;
			break;
		case 'd':
			database = optarg;
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
	if (mpd_socket && port_num != 6600) {
		strerr_die1x(100, "you can't specify both socket & port");
	}
	if (mpd_socket && str_diff(mpd_host, "127.0.0.1")) {
		strerr_die1x(100, "you can't specify both socket & IP");
	}
	port[fmt_ulong(port, port_num)] = 0;
	if ((sock = tcpopen(mpd_socket ? mpd_socket : mpd_host, 0, port_num)) == -1) {
		if (mpd_socket)
			strerr_die4sys(111, "update_stats: tcpopen: ", "socket [", mpd_socket, "]");
		else
			strerr_die6sys(111, "update_stats: tcpopen: ", "host [", mpd_host, "] port [", port, "]");
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
		if (!str_diffn(line.s, "file: ", 6)) {
			if (uri.len) {
				insert_data(res, &processed, &failure, print_sql ? &ptr : 0);
				if (sqlite3_changes(db) == 1) {
					print_song(uri.s, last_modified.s, duration.s, artist.s, album.s, title.s, track.s, genre.s, date.s);
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
		} else
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
	row_count = stats_database_end(database, transaction_mode);
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
