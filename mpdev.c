/*
 * $Log: mpdev.c,v $
 * Revision 1.4  2020-07-14 14:19:00+05:30  Cprogrammer
 * handle empty playlist with no currentsong playing
 *
 * Revision 1.3  2020-07-13 22:33:38+05:30  Cprogrammer
 * fixed usage string
 *
 * Revision 1.2  2020-07-13 01:04:17+05:30  Cprogrammer
 * set SONG_DURATION as seconds since epoch
 *
 * Revision 1.1  2020-07-08 14:30:59+05:30  Cprogrammer
 * Initial revision
 *
 *
 * mpdev - watch mpd events
 *
 * Full documentation of mpd commands at https://www.musicpd.org/doc/html/protocol.html
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
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <timeoutread.h>
#include <timeoutwrite.h>
#include <substdio.h>
#include <getln.h>
#include <stralloc.h>
#include <str.h>
#include <scan.h>
#include <env.h>
#include <fmt.h>
#include <sgetopt.h>
#include <strerr.h>
#include <error.h>
#include <wait.h>
#include "pathexec.h"
#include "tcpopen.h"

#ifndef	lint
static char     sccsid[] = "$Id: mpdev.c,v 1.4 2020-07-14 14:19:00+05:30 Cprogrammer Exp mbhangui $";
#endif

extern char    *strptime(const char *, const char *, struct tm *);
ssize_t         safewrite(int, char *, int);
ssize_t         saferead(int, char *, int);

substdio        mpdin, mpdout, ssout, sserr;
stralloc        line = {0};
char            strnum[FMT_ULONG];
/*
 * verbose = 0 - silent
 *         = 1 - output from script run -v
 *         = 2 - output mpd events      -vv
 *         = 3 - high verbose output    -vvv
 */
int             timeout = 1200, verbose;
char           *player_cmd[3] = {0, 0, 0};

char           *usage =
				"usage: mpdev [-i IP/Host | -s unix_socket] [-p port] [-r retry_interval]\n"
				" -i IP    - IP address of MPD host. default 127.0.0.1\n"
				" -p port  - MPD listening port. default 6600\n"
				" -s unix  - domain socket path\n"
				" -r retry - retry interval if mpd host is down\n"
				" -v       - output from executed scripts\n"
				" -vv      - output from mpd events\n"
				" -vvv     - high verbose output\n";

void
die_write()
{
	substdio_puts(&sserr, "Requested action aborted: write error\n");
	substdio_flush(&sserr);
	_exit(111);
}

void
flush()
{
	if (substdio_flush(&ssout) == -1)
		die_write();
}

void
flush_err()
{
	if (substdio_flush(&sserr) == -1)
		die_write();
}

void
out(char *s)
{
	if (substdio_puts(&ssout, s) == -1)
		die_write();
}

void
err_out(char *s)
{
	if (substdio_puts(&sserr, s) == -1 || substdio_flush(&sserr) == -1)
		die_write();
}

void
die_read(char *arg)
{
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
mpd_flush_io()
{
	mpdin.p = 0;
	if (substdio_flush(&mpdout) == -1)
		die_write();
}

void
print_time(substdio *ss)
{
	struct timeval  tm;
	int             i;

	if (verbose < 2)
		return;
	if (gettimeofday(&tm, 0))
		strerr_die1sys(111, "mpdev: gettimeofday: ");
	strnum[i = fmt_ulong(strnum, tm.tv_sec)] = 0;
	if (substdio_put(ss, strnum, i) == -1)
		die_write();
	if (substdio_put(ss, ".", 1) == -1)
		die_write();
	strnum[i = fmt_ulong(strnum, tm.tv_usec)] = 0;
	if (substdio_put(ss, strnum, i) == -1)
		die_write();
	if (substdio_put(ss, " ", 1) == -1)
		die_write();
}

void
mpd_out(char *s)
{
	if (verbose > 1) {
		print_time(&ssout);
		out("mpd command [");
		out(s);
		out("]\n");
		flush();
	}
	if (substdio_puts(&mpdout, s) == -1)
		die_write();
	if (substdio_puts(&mpdout, "\n") == -1)
		die_write();
	if (substdio_flush(&mpdout) == -1)
		die_write();
}

void
die_alarm()
{
	substdio_puts(&sserr, "Requested action aborted: timeout\n");
	substdio_flush(&sserr);
	_exit(111);
}

void
die_nomem()
{
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
		die_write();
	return r;
}

/*-
 * file: 96kHz/Minnesota Orchestra - Eiji Oue - Nikolai Rimsky-Korsakov- The Snow Maiden - Dance of the Tumblers.flac
 * Last-Modified: 2020-02-04T14:40:15Z
 * Album: HDtracks Ultimate Download Experience
 * Artist: Minnesota Orchestra / Eiji Oue
 * Date: 2009
 * Genre: Classical
 * Title: Nikolai Rimsky-Korsakov: The Snow Maiden - Dance of the Tumblers
 * Track: 1
 * Time: 235
 * duration: 234.645
 * Pos: 0
 * Id: 1
 * OK
 */
static stralloc uri_s = {0}, last_modified_s = {0}, album_s = {0},
	artist_s = {0}, date_s = {0}, genre_s = {0}, title_s = {0}, id_s = {0},
	track_s = {0}, duration_s = {0}, position_s = {0}, response_s = {0};

int
get_song_details(char **uri, char **last_modified, char **album, char **artist,
	char **date, char **genre, char **title, char **track, char **duration,
	time_t *duration_i, int *pos, unsigned long *id, char **response)
{
	int             match, flag;

	if (uri)
		*uri = 0;
	if (last_modified)
		*last_modified = 0;
	if (album)
		*album = 0;
	if (artist)
		*artist = 0;
	if (date)
		*date = 0;
	if (genre)
		*genre = 0;
	if (title)
		*title = 0;
	if (track)
		*track = 0;
	if (duration)
		*duration = 0;
	if (duration_i)
		*duration_i = -1;
	if (pos)
		*pos = -1;
	if (id)
		*id = 0;
	if (response)
		*response = 0;
	mpd_out("currentsong");
	for (flag = 1;;) {
		if (getln(&mpdin, &line, &match, '\n') == -1)
			die_read("currentsong");
		if (!match && line.len == 0) {
			if (verbose) {
				out("EOF current song\n");
				flush();
			}
			return 0;
		}
		if (verbose > 1) {
			if (substdio_put(&ssout, line.s, line.len) == -1)
				die_write();
		}
		if (!str_diffn(line.s, "OK MPD ", 7))
			continue;
		if (!str_diffn(line.s, "OK\n", 3)) {
			flag = 2;
			break;
		}
		line.len--;
		line.s[line.len] = 0;
		if (uri && !str_diffn(line.s, "file: ", 6)) {
			if (!stralloc_copyb(&uri_s, line.s + 6, line.len - 6) || !stralloc_0(&uri_s))
				die_nomem();
			*uri = uri_s.s;
		} else
		if (last_modified && !str_diffn(line.s, "Last-Modified: ", 15)) {
			if (!stralloc_copyb(&last_modified_s, line.s + 15, line.len - 15) || !stralloc_0(&last_modified_s))
				die_nomem();
			*last_modified = last_modified_s.s;
		} else
		if (album && !str_diffn(line.s, "Album: ", 7)) {
			if (!stralloc_copyb(&album_s, line.s + 7, line.len - 7) || !stralloc_0(&album_s))
				die_nomem();
			*album = album_s.s;
		} else
		if (artist && !str_diffn(line.s, "Artist: ", 8)) {
			if (!stralloc_copyb(&artist_s, line.s + 8, line.len - 8) || !stralloc_0(&artist_s))
				die_nomem();
			*artist = artist_s.s;
		} else
		if (date && !str_diffn(line.s, "Date: ", 6)) {
			if (!stralloc_copyb(&date_s, line.s + 6, line.len - 6) || !stralloc_0(&date_s))
				die_nomem();
			*date = date_s.s;
		} else
		if (genre && !str_diffn(line.s, "Genre: ", 7)) {
			if (!stralloc_copyb(&genre_s, line.s + 7, line.len - 7) || !stralloc_0(&genre_s))
				die_nomem();
			*genre = genre_s.s;
		} else
		if (title && !str_diffn(line.s, "Title: ", 7)) {
			if (!stralloc_copyb(&title_s, line.s + 7, line.len - 7) || !stralloc_0(&title_s))
				die_nomem();
			*title = title_s.s;
		} else
		if (track && !str_diffn(line.s, "Track: ", 7)) {
			if (!stralloc_copyb(&track_s, line.s + 7, line.len - 7) || !stralloc_0(&track_s))
				die_nomem();
			*track = track_s.s;
		} else
		if (duration && !str_diffn(line.s, "Time: ", 6)) {
			if (!stralloc_copyb(&duration_s, line.s + 6, line.len - 6) || !stralloc_0(&duration_s))
				die_nomem();
			*duration = duration_s.s;
		} else
		if (duration_i && !str_diffn(line.s, "duration: ", 10)) {
			scan_ulong(line.s + 10, (unsigned long *) duration_i);
		} else
		if (pos && !str_diffn(line.s, "Pos: ", 5)) {
			scan_int(line.s + 5, pos);
			if (!stralloc_copyb(&position_s, line.s + 5, line.len - 5) || !stralloc_0(&position_s))
				die_nomem();
		} else
		if (id && !str_diffn(line.s, "Id: ", 4)) {
			scan_ulong(line.s + 4, id);
			if (!stralloc_copyb(&id_s, line.s + 4, line.len - 4) || !stralloc_0(&id_s))
				die_nomem();
		} else
		if (response && !str_diffn(line.s, "OK", 2)) {
			if (!stralloc_copyb(&response_s, line.s + 10, line.len - 10) || !stralloc_0(&response_s))
				die_nomem();
			*response = response_s.s;
		}
	}
	return (flag);
}

#define DATABASE_EVENT        1
#define UPDATE_EVENT          2
#define STORED_PLAYLIST_EVENT 3
#define PLAYLIST_EVENT        4
#define PLAYER_EVENT          5
#define MIXER_EVENT           6
#define OUTPUT_EVENT          7
#define OPTIONS_EVENT         8
#define PARTITION_EVENT       9
#define STICKER_EVENT         10
#define SUBSCRIPTION_EVENT    11
#define MESSAGE_EVENT         12
#define NEIGHBOUR_EVENT       13
#define MOUNT_EVENT           14
#define CUSTOM_EVENT          15

int
run_command(int status, char *arg)
{
	int             i, wstat, childrc, fd;
	pid_t           child;

	if (!status) {
		player_cmd[0] = ".mpdev/player";
	} else
	switch (status)
	{
		case STICKER_EVENT:
			player_cmd[0] = ".mpdev/sticker";
			break;
		case MIXER_EVENT:
			player_cmd[0] = ".mpdev/mixer";
			break;
		case OPTIONS_EVENT:
			player_cmd[0] = ".mpdev/options";
			break;
		case OUTPUT_EVENT:
			player_cmd[0] = ".mpdev/output";
			break;
		case UPDATE_EVENT:
			player_cmd[0] = ".mpdev/update";
			break;
		case DATABASE_EVENT:
			player_cmd[0] = ".mpdev/database";
			break;
		case STORED_PLAYLIST_EVENT:
			player_cmd[0] = ".mpdev/stored_playlist";
			break;
		case PARTITION_EVENT:
			player_cmd[0] = ".mpdev/partition";
			break;
		case SUBSCRIPTION_EVENT:
			player_cmd[0] = ".mpdev/subscription";
			break;
		case MESSAGE_EVENT:
			player_cmd[0] = ".mpdev/message";
			break;
		case MOUNT_EVENT:
			player_cmd[0] = ".mpdev/mount";
			break;
		case NEIGHBOUR_EVENT:
			player_cmd[0] = ".mpdev/neighbour";
			break;
		case CUSTOM_EVENT:
			player_cmd[0] = ".mpdev/custom";
			break;
	}
	if (access(player_cmd[0], F_OK))
		return 0;
	player_cmd[1] = arg;
	if (verbose == 3) {
		print_time(&ssout);
		out("mpdev: running command ");
		out(player_cmd[0]);
		out(" ");
		out(arg);
		out("\n");
		flush();
	}
	switch ((child = fork()))
	{
	case -1:
		strerr_die1sys(111, "fork: ");
		_exit(111); /*- not reached */
	case 0:
		if (!verbose) {
			if ((fd = open("/dev/null", O_WRONLY)) == -1)
				strerr_die1sys(111, "open: /dev/null: ");
			if (dup2(fd, 1) == -1)
				strerr_die1sys(111, "open: /dev/null: ");
			if (fd != 1)
				close(fd);
		}
		pathexec_run(player_cmd[0], player_cmd, environ);
		_exit(111);
	default:
		break;
	}
	if (wait_pid(&wstat, child) == -1)
		strerr_die1sys(111, "wait_pid: ");
	if (wait_crashed(wstat))
		strerr_die1sys(111, "wait crashed: ");
	childrc = wait_exitcode(wstat);
	if (verbose == 3) {
		print_time(&ssout);
		out("mpdev: command ");
		out(player_cmd[0]);
		out(" ");
		out(arg);
		out(" exit_status=[");
		strnum[i = fmt_int(strnum, childrc)] = 0;
		out(strnum);
		out("]\n");
		flush();
	}
	return (childrc);
}

/*
 * database: the song database has been modified after update.
 * update: a database update has started or finished. If the database was modified during the update, the database event is also emitted.
 * stored_playlist: a stored playlist has been modified, renamed, created or deleted
 * playlist: the queue (i.e. the current playlist) has been modified
 * player: the player has been started, stopped or seeked
 * mixer: the volume has been changed
 * output: an audio output has been added, removed or modified (e.g. renamed, enabled or disabled)
 * options: options like repeat, random, crossfade, replay gain
 * partition: a partition was added, removed or changed
 * sticker: the sticker database has been modified.
 * subscription: a client has subscribed or unsubscribed to a channel
 * message: a message was received on a channel this client is subscribed to; this event is only emitted when the queue is empty
 * neighbor: a neighbor was found or lost
 * mount: the mount list has changed
 */

int
do_idle()
{
	int             match, status;

	mpd_out("idle");
	status = 0;
	for (;;) {
		if (getln(&mpdin, &line, &match, '\n') == -1)
			die_read("idle");
		if (!match && line.len == 0) {
			if (verbose) {
				out("EOF idle\n");
				flush();
			}
			return (0);
		}
		if (verbose > 1) {
			if (substdio_put(&ssout, line.s, line.len) == -1)
				die_write();
			flush();
		}
		if (!str_diffn(line.s, "changed: playlist\n", 18)) {
			status = PLAYLIST_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: player\n", 16)) {
			status = PLAYER_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: sticker\n", 17)) {
			status = STICKER_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: mixer\n", 15)) {
			status = MIXER_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: options\n", 17)) {
			status = OPTIONS_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: output\n", 16)) {
			status = OUTPUT_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: update\n", 16)) {
			status = UPDATE_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: database\n", 18)) {
			status = DATABASE_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: stored_playlist\n", 25)) {
			status = STORED_PLAYLIST_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: partition\n", 19)) {
			status = PARTITION_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: subscription\n", 22)) {
			status = SUBSCRIPTION_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: message\n", 17)) {
			status = MESSAGE_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: mount\n", 15)) {
			status = MOUNT_EVENT;
			continue;
		} else
		if (!str_diffn(line.s, "changed: neighbor\n", 18)) {
			status = NEIGHBOUR_EVENT;
			continue;
		}
		if (!str_diffn(line.s, "OK\n", 3)) {
			switch(status)
			{
			case PLAYER_EVENT:
			case PLAYLIST_EVENT:
				break;
			default:
				run_command(status, "mpd-event");
				status = 0;
				mpd_out("idle");
				continue;
			}
			break; /*- break out of loop */
		}
		if (verbose) {
			out("idle: unexpected output: ");
			if (!stralloc_0(&line))
				die_nomem();
			out(line.s);
			flush();
		}
	} /*- for (;;) { */
	return status;
}

void
print_song_details(char *uri, char *last_modified, char *album, char *artist,
	char *date, char *genre, char *title, char *track, char *duration,
	time_t duration_i, int pos, unsigned long id, char *response)
{
	int             flag;

	flag = 0;
	if (id > 0) {
		if (!flag++)
			print_time(&ssout);
		out("Id: ");
		strnum[fmt_ulong(strnum, id)] = 0;
		out(strnum);
		out("\n");
	}
	if (uri) {
		if (!flag++)
			print_time(&ssout);
		out("file: ");
		out(uri);
		out("\n");
	}
	if (last_modified) {
		if (!flag++)
			print_time(&ssout);
		out("last-Modified: ");
		out(last_modified);
		out("\n");
	}
	if (album) {
		if (!flag++)
			print_time(&ssout);
		out("Album: ");
		out(album);
		out("\n");
	}
	if (artist) {
		if (!flag++)
			print_time(&ssout);
		out("Artist: ");
		out(artist);
		out("\n");
	}
	if (date) {
		if (!flag++)
			print_time(&ssout);
		out("Date: ");
		out(date);
		out("\n");
	}
	if (genre) {
		if (!flag++)
			print_time(&ssout);
		out("Genre: ");
		out(genre);
		out("\n");
	}
	if (title) {
		if (!flag++)
			print_time(&ssout);
		out("Title: ");
		out(title);
		out("\n");
	}
	if (track) {
		if (!flag++)
			print_time(&ssout);
		out("Track: ");
		out(track);
		out("\n");
	}
	if (duration) {
		if (!flag++)
			print_time(&ssout);
		out("Duration: ");
		out(duration);
		out("\n");
	}
	if (duration_i > 0) {
		if (!flag++)
			print_time(&ssout);
		out("Length: ");
		strnum[fmt_ulong(strnum, duration_i)] = 0;
		out(strnum);
		out("\n");
	}
	if (!pos || pos > 0) {
		if (!flag++)
			print_time(&ssout);
		out("Pos: ");
		strnum[fmt_ulong(strnum, pos)] = 0;
		out(strnum);
		out("\n");
	}
}

void
submit_song(int verbose, char *cmmd)
{
	int             i;

	strnum[i = fmt_int(strnum, verbose)] = 0;
	if (!env_put2("VERBOSE", strnum))
		die_nomem();
	run_command(0, cmmd);
	return;
}

void
set_environ()
{
	struct tm       tm = {0};
	time_t          mod_time;
	int             i;

	if (uri_s.len && !env_put2("SONG_URI", uri_s.s))
		die_nomem();
	if (last_modified_s.len) {
		if (!strptime(last_modified_s.s, "%Y-%m-%dT%H:%M:%SZ", &tm)) {
			strerr_warn4("uri: ", uri_s.s, ": invalid timestamp: ", last_modified_s.s, 0);
			mod_time = time(0);
		} else
		if (!(mod_time = mktime(&tm))) {
			strerr_warn4("uri: ", uri_s.s, ": invalid timestamp: ", last_modified_s.s, 0);
			mod_time = time(0);
		}
		strnum[i = fmt_ulong(strnum, mod_time)] = 0;
	} else
		i = 0;
	if (i && !env_put2("SONG_LAST_MODIFIED", strnum))
		die_nomem();
	if (album_s.len && !env_put2("SONG_ALBUM", album_s.s))
		die_nomem();
	if (artist_s.len && !env_put2("SONG_ARTIST", artist_s.s))
		die_nomem();
	if (date_s.len && !env_put2("SONG_DATE", date_s.s))
		die_nomem();
	if (genre_s.len && !env_put2("SONG_GENRE", genre_s.s))
		die_nomem();
	if (title_s.len && !env_put2("SONG_TITLE", title_s.s))
		die_nomem();
	if (track_s.len && !env_put2("SONG_TRACK", track_s.s))
		die_nomem();
	if (duration_s.len && !env_put2("SONG_DURATION", duration_s.s))
		die_nomem();
	if (position_s.len && !env_put2("SONG_POSITION", position_s.s))
		die_nomem();
	if (id_s.len && !env_put2("SONG_ID", id_s.s))
		die_nomem();
}

int
main(int argc, char **argv)
{
	int             i, opt, pos, sock, port_num = 6600, retry_interval = 60, connection_num;
	char           *mpd_socket, *uri, *last_modified, *album, *artist,
				   *date, *genre, *title, *track, *duration, *response, *mpd_host, *ptr;
	char            port[FMT_ULONG], mpdinbuf[1024], mpdoutbuf[512], ssoutbuf[512], sserrbuf[512];
	unsigned long   id, prev_id1 = 0, prev_id2 = 0;
	time_t          duration_i;

	substdio_fdbuf(&ssout, write, 1, ssoutbuf, sizeof(sserrbuf));
	substdio_fdbuf(&sserr, write, 2, sserrbuf, sizeof(sserrbuf));
	if (!(mpd_host = env_get("MPD_HOST")))
		mpd_host = "127.0.0.1";
	mpd_socket = env_get("MPD_SOCKET");
	if (!(ptr = env_get("MPD_PORT")))
		port_num = 6600;
	else
		scan_uint(ptr, (unsigned int *) &port_num);
	while ((opt = getopt(argc, argv, "vh:p:s:r:")) != opteof) {
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
		case 'r':
			scan_uint(optarg, (unsigned int *) &retry_interval);
			break;
		case 'v':
			verbose++;
			break;
		default:
			strerr_die1x(100, usage);
		}
	}
	if (mpd_socket && port_num != 6600)
		strerr_die1x(100, "you can't specify both socket & port");
	if (mpd_socket && str_diff(mpd_host, "127.0.0.1"))
		strerr_die1x(100, "you can't specify both socket & IP");
	port[fmt_ulong(port, port_num)] = 0;
	if (!(ptr = env_get("HOME")))
		strerr_die1x(100, "HOME not set");
	if (chdir(ptr))
		strerr_die3sys(111, "unable to chdir to ", ptr, ": ");
	for (connection_num = 1;;connection_num++) {
		if (verbose == 3) {
			print_time(&ssout);
			strnum[i = fmt_ulong(strnum, connection_num)] = 0;
			out("connection ");
			out(strnum);
			out(": ");
			out(mpd_socket ? "socket [" : "host [");
			out(mpd_socket ? mpd_socket : mpd_host);
			out("]");
			if (!mpd_socket) {
				out(" port [");
				out(port);
				out("]\n");
			} else {
				out("\n");
			}
			flush();
		}
		if ((sock = tcpopen(mpd_socket ? mpd_socket : mpd_host, 0, port_num)) == -1) {
			print_time(&sserr);
			flush_err();
			strnum[i = fmt_ulong(strnum, connection_num)] = 0;
			if (mpd_socket)
				strerr_warn6("mpdev: connection ", strnum, ": tcpopen: ", "socket [", mpd_socket, "]", &strerr_sys);
			else
				strerr_warn8("mpdev: connection ", strnum, ": tcpopen: ", "host [", mpd_host, "] port [", port, "]", &strerr_sys);
			sleep(retry_interval);
			continue;
		}
		if (verbose) {
			print_time(&ssout);
			strnum[i = fmt_ulong(strnum, connection_num)] = 0;
			out("connection ");
			out(strnum);
			out(": ");
			out(mpd_socket ? "socket [" : "host [");
			out(mpd_socket ? mpd_socket : mpd_host);
			out("]");
			if (!mpd_socket) {
				out(" port [");
				out(port);
				out("] connected\n");
			} else {
				out(" connected\n");
			}
			flush();
		}
		substdio_fdbuf(&mpdin, saferead, sock, mpdinbuf, sizeof mpdinbuf);
		substdio_fdbuf(&mpdout, safewrite, sock, mpdoutbuf, sizeof mpdoutbuf);
		prev_id1 = prev_id2 = 0;
		for (;;) {
			if ((i = get_song_details(&uri, &last_modified, &album, &artist, &date, &genre,
				&title, &track, &duration, &duration_i, &pos, &id, &response)) == 1) {
				print_time(&sserr);
				err_out("failed to get song details\n");
				sleep(1);
				continue;
			} else
			if (!i) {
				close(sock);
				submit_song(verbose, "end-song");
				break;
			}
			if (prev_id1 && prev_id1 != id) {
				submit_song(verbose, "end-song");
				prev_id1 = id;
			}
			if (id) {
				set_environ();
				prev_id1 = id;
				if (!prev_id2 || (id && prev_id2 != id)) {
					if (verbose == 3) {
						print_song_details(uri, last_modified, album, artist,
							date, genre, title, track, duration, duration_i,
							pos, id, response);
						flush();
					}
					submit_song(verbose, "now-playing");
					prev_id2 = id;
				}
			} else
				run_command(CUSTOM_EVENT, "mpd-event");
			if (!do_idle()) {
				close(sock);
				submit_song(verbose, "end-song");
				break;
			}
		}
	}
	_exit(0);
}

void
getversion_mpdev_C()
{
	static char    *x = "$Id: mpdev.c,v 1.4 2020-07-14 14:19:00+05:30 Cprogrammer Exp mbhangui $";

	x++;
}
