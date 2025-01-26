/*
 * $Log: mpdev_cleanup.c,v $
 * Revision 1.5  2025-01-26 16:45:47+05:30  Cprogrammer
 * fix gcc14 errors
 *
 * Revision 1.4  2022-05-10 21:31:23+05:30  Cprogrammer
 * use tcpopen from standard include path
 *
 * Revision 1.3  2020-08-08 11:07:46+05:30  Cprogrammer
 * fixed usage, error messages
 *
 * Revision 1.2  2020-07-28 12:40:52+05:30  Cprogrammer
 * made -m optional
 *
 * Revision 1.1  2020-07-19 18:16:35+05:30  Cprogrammer
 * Initial revision
 *
 *
 * mpdev_cleanup - utility to clean entries from stats/sticker sqlite db
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <timeoutread.h>
#include <timeoutwrite.h>
#include <substdio.h>
#include <stralloc.h>
#include <error.h>
#include <getln.h>
#include <strerr.h>
#include <sgetopt.h>
#include <fmt.h>
#include <env.h>
#include <scan.h>
#include <str.h>
#include <tcpopen.h>
#include "replacestr.h"

#ifndef	lint
static char     sccsid[] = "$Id: mpdev_cleanup.c,v 1.5 2025-01-26 16:45:47+05:30 Cprogrammer Exp mbhangui $";
#endif

ssize_t         safewrite(int, char *, size_t);
ssize_t         saferead(int, char *, size_t);

substdio        mpdin, mpdout, ssout, sserr;
static stralloc line = {0}, tmp = {0};
int             timeout = 1200, disk_mode = 0, verbose;
char            method = 0;
static sqlite3 *db, *memdb;
char           *usage =
				"usage: mpdev_cleanup [-i IP/Host | -s unix_socket] [-p port]\n"
				"  -h IP   - IP address of MPD host. default 127.0.0.1\n"
				"  -p port - MPD listening port. default 6600\n"
				"  -s unix - domain socket path\n"
				"  -m path - music directory path\n"
				"  -d path - sqlite3 dbfile path\n"
				"  -D      - Use disk mode\n"
				"  -c      - Clean stats db\n"
				"  -C      - Clean sticker db\n"
				"  -v      - verbose output";

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

int
database_init(int transaction_mode, sqlite3_stmt **r_res, sqlite3_stmt **w_res)
{
	char           *sql, *err_msg;

	*r_res = *w_res = (sqlite3_stmt *) 0;
	if (sqlite3_open(":memory:", &memdb) != SQLITE_OK)
		strerr_die2x(111, "sqlite3_open: memdb: ", (char *) sqlite3_errmsg(memdb));
	if (transaction_mode) {
		if (sqlite3_exec(memdb, "BEGIN TRANSACTION", NULL, NULL, &err_msg) != SQLITE_OK) {
			sqlite3_close(memdb);
			strerr_die2x(111, "BEGIN TRANSACTION: ", err_msg);
		}
		if (err_msg)
			sqlite3_free(err_msg);
	}
	sql = "CREATE TABLE IF NOT EXISTS temp(\n"
        "id  INTEGER PRIMARY KEY,\n"
        "uri TEXT UNIQUE NOT NULL\n"
		")";
	if (sqlite3_exec(memdb, sql, 0, 0, &err_msg) != SQLITE_OK) {
		sqlite3_close(memdb);
		strerr_die4x(111, "sqlite3_exec: ", sql, ": ", err_msg);
	}
	if (err_msg)
		sqlite3_free(err_msg);
	sql = "INSERT or IGNORE into temp (uri) values (@uri)";
	if (sqlite3_prepare_v2(memdb, sql, -1, w_res, 0) != SQLITE_OK) {
		sqlite3_close(memdb);
		strerr_die4x(111, "sqlite3_prepare_v2: ", sql, ": ", (char *) sqlite3_errmsg(db));
	}
	sql = "SELECT uri from temp where uri=@uri";
	if (sqlite3_prepare_v2(memdb, sql, -1, r_res, 0) != SQLITE_OK) {
		sqlite3_close(memdb);
		strerr_die4x(111, "sqlite3_prepare_v2: ", sql, ": ", (char *) sqlite3_errmsg(db));
	}
	return (0);
}

void
insert_data(sqlite3_stmt *res, char *stmt, unsigned long *processed, unsigned long *failure)
{
	sqlite3_bind_text(res, 1, stmt, -1, SQLITE_STATIC);
	if (sqlite3_step(res) != SQLITE_DONE)
		*failure += 1;
	else
		*processed += 1;
	sqlite3_clear_bindings(res);
	sqlite3_reset(res);
}

void
database_end(int transaction_mode)
{
	char           *sql, *err_msg;

	sql = "CREATE INDEX IF NOT EXISTS uri on temp(uri);";
	if (sqlite3_exec(memdb, sql, 0, 0, &err_msg) != SQLITE_OK) {
		sqlite3_close(memdb);
		strerr_die4x(111, "sqlite3_exiec: ", sql, ": ", err_msg);
	}
	if (err_msg)
		sqlite3_free(err_msg);
	if (transaction_mode) {
		if (sqlite3_exec(memdb, "END TRANSACTION", 0, 0, &err_msg) != SQLITE_OK) {
			sqlite3_close(memdb);
			strerr_die2x(111, "sqlite3_exec: END TRANSACTION: ", err_msg);
		}
		if (err_msg)
			sqlite3_free(err_msg);
	}
	return;
}

sqlite3_stmt   *
dump_mpd_into_mem(int sock)
{
	int             match;
	char            mpdinbuf[1024], mpdoutbuf[512];
	unsigned long   processed, failure;
	sqlite3_stmt   *r_res, *w_res;

	substdio_fdbuf(&mpdin, saferead, sock, mpdinbuf, sizeof mpdinbuf);
	substdio_fdbuf(&mpdout, safewrite, sock, mpdoutbuf, sizeof mpdoutbuf);
	if (substdio_put(&mpdout, "listallinfo\n", 12) || substdio_flush(&mpdout) == -1)
		die_write("unable to write to mpd");
	database_init(1, &r_res, &w_res);
	for (processed = failure = 0;;) {
		if (getln(&mpdin, &line, &match, '\n') == -1)
			die_read("getln");
		if (!match && line.len == 0)
			break;
		if (!str_diffn(line.s, "OK\n", 3))
			break;
		line.s[--line.len] = 0; /*- remove newline */
		if (!str_diffn(line.s, "file: ", 6))
			insert_data(w_res, line.s + 6, &processed, &failure);
	}
	database_end(1);
	return r_res;
}

static int
callback(void *data, int argc, char **argv, char **column_name)
{
	int             i, j, notfound = 0;
	char           *ptr;

	for(i = 0; i < argc; i++) {
#if 0
		out(column_name[i]);
		out(" ");
		out(argv[i] ? argv[i] : "NULL");
		out("\n");
#endif
		if (i || !argv[i])
			continue;
		switch (disk_mode)
		{
			case 1:
				notfound = access(argv[i], F_OK);
				break;
			default:
				ptr = sqlite3_expanded_sql((sqlite3_stmt *) data);
				tmp.len = 0;
				if (ptr && (!stralloc_copys(&tmp, ptr) || !stralloc_0(&tmp)))
					die_nomem();
				if (sqlite3_bind_text((sqlite3_stmt *) data, 1, argv[i], -1, SQLITE_STATIC) != SQLITE_OK) {
					strerr_warn4("sqlite3_bind_text: ", argv[i], ": ", (char *) sqlite3_errmsg(memdb), 0);
					strerr_warn4("sqlite3_bind_text: ", tmp.len ? tmp.s : "unknown statement", ": ", (char *) sqlite3_errmsg(memdb), 0);
					return 1;
				}
				j = sqlite3_step((sqlite3_stmt *) data);
				notfound = (j == SQLITE_ROW ? 0 : 1);
				sqlite3_clear_bindings((sqlite3_stmt *) data);
				sqlite3_reset((sqlite3_stmt *) data);
				if (j == SQLITE_ERROR) {
					strerr_warn3("sqlite3_step: ", tmp.len ? tmp.s : "unknown statement", ": ", 0);
					return 1;
				}
				break;
		}
		if (notfound) {
			out(method == 1 ? "DELETE from song where uri='" : "DELETE from sticker where uri='");
			if ((j = replacestr(argv[i], "'", "''", &tmp)))
				out(tmp.s);
			else
				out(argv[i]);
			out("';\n");
		}
	}
	return 0;
}

int
main(int argc, char **argv)
{
	int             opt, sock, port_num = 6600;
	char            port[FMT_ULONG], ssoutbuf[512], sserrbuf[512];
	char           *sql, *database = 0, *err_msg, *music_directory = 0,
				   *mpd_socket, *mpd_host, *ptr;
	sqlite3_stmt   *r_res;

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
	while ((opt = getopt(argc, argv, "vcCDd:m:h:s:p:")) != opteof) {
		switch (opt) {
		case 'h':
			mpd_host = optarg;
			break;
		case 's':
			mpd_socket = optarg;
			break;
		case 'p':
			scan_uint(optarg, (unsigned int *) &port_num);
			break;
		case 'm':
			music_directory = optarg;
			break;
		case 'd':
			database = optarg;
			break;
		case 'c': /*- clean stats */
			if (!method)
				method = 1;
			else
				method = 3;
			break;
		case 'C': /*- clean sticker */
			if (!method)
				method = 2;
			else
				method = 3;
			break;
		case 'D':
			disk_mode = 1;
			break;
		case 'v':
			verbose++;
			break;
		default:
			strerr_die1x(100, usage);
		}
	}

	if (!database || !method) {
		err_out("You have to specify -d and either -c or -C options\n");
		err_flush();
		strerr_die1x(100, usage);
	}
	if (method == 3) {
		err_out("You cannot specify both -c and -C options\n");
		err_flush();
		strerr_die1x(100, usage);
	}

	if (!disk_mode) {
		if (mpd_socket && port_num != 6600)
			strerr_die1x(100, "you can't specify both socket & port");
		if (mpd_socket && str_diff(mpd_host, "127.0.0.1"))
			strerr_die1x(100, "you can't specify both socket & IP");
		port[fmt_ulong(port, port_num)] = 0;
		if ((sock = tcpopen(mpd_socket ? mpd_socket : mpd_host, 0, port_num)) == -1) {
			if (mpd_socket)
				strerr_die4sys(111, "mpdev_cleanup: tcpopen: ", "socket [", mpd_socket, "]: ");
			else
				strerr_die6sys(111, "mpdev_cleanup: tcpopen: ", "host [", mpd_host, "] port [", port, "]: ");
		}
		r_res = dump_mpd_into_mem(sock);
	} else
		r_res = (sqlite3_stmt *) 0;
	if (sqlite3_open(database, &db) != SQLITE_OK)
		strerr_die3x(111, database, ": ", (char *) sqlite3_errmsg(db));
	if (music_directory && chdir(music_directory))
		strerr_die3sys(111, "unable to chdir to ", music_directory, ": ");
	sql = (method == 1 ? "SELECT uri from song" : "SELECT uri from sticker");
	if (sqlite3_exec(db, sql, callback, r_res, &err_msg) != SQLITE_OK) {
		sqlite3_close(db);
		strerr_die5x(111, database, ": ", sql, ": ", err_msg);
	}
	if (err_msg)
		sqlite3_free(err_msg);
	sqlite3_close(db);
	out("VACUUM;\n");
	out(".quit\n");
	flush();
}
