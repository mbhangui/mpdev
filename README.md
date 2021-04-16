# mpdev

[![mpdev C/C++ CI](https://github.com/mbhangui/mpdev/actions/workflows/mpdev-c-cpp.yml/badge.svg)](https://github.com/mbhangui/mpdev/actions/workflows/mpdev-c-cpp.yml)

**mpdev** is a music player daemon event watcher. It connects to the mpd socket and uses mpd's idle command to listen for player events. Whenever an event occurs, mpdev can carry out various activities using user defined hooks. The idea for doing mpdev comes from [mpdcron](https://alip.github.io/mpdcron/).


mpdev helps in bulding a database of your played tracks. Along with a script `mpdplaylist`, it can generate a playlist for mpd as per your taste and mood.

You can create scripts in $HOME/.mpdev directory. The default installation installs a script named $HOME/.mpdev/player for the uid 1000. The script is adequate for most use cases. It additionally updates RompR database. The script does the following

1. scrobbles titles to last.fm and libre.fm. You have to create API keys by running lastfm-scrobbler and librefm-scrobbler one time
2. updates play counts in the sqlite stats.db. You can write your own script and update any external database. An example of this is in the player script which updates the Playcounttable for [RompR](https://fatg3erman.github.io/RompR/).
3. Synchronizes the ratings in the sticker (sqlite). You can write your own script and update any external database. An exampe of this is in the player script which updates the Ratingtable for [RompR](https://fatg3erman.github.io/RompR/) (MySQL). You can also do automatic rating to some default value by setting an environment variable AUTO\_RATING. If you are using supervise from deamontools (more on that below), creating an environment variable is very easy. e.g. To have an environment variable AUTO\_RATING with vvalue 6, you just need to have a file name AUTO\_RATING in /service/mpdev/variables. The file should just have 6 as the content.
4. Update the song's karma. If a song is skipped, it's karma is downgraded by 1. If a song is played twice withing a day, it's karma is upgraded by 4. If a song is played twice within a week, it's karma is upgraded by 3. If a song is played twice within 14 days, its karma is upgraded by 2. If a song is played twice within a month, it's karma is upgraded by 1. All songs start with a default karma of 50.

The above three are actually done by running a hook, a script named `player` in $HOME/.mpdev directory. You can put your own script named `player` in this directory. In fact mpdev can run specific hooks for specific types of mpd events. A hook can be any executable program or script. It will be passed arguments and have certain environment variables related to the song playing, available to it. Below is a list of of events and corresponding hooks that will be executed if available.

MPD EVENT|Hook script
---------|-----------
SONG_CHANGE|~/.mpdev/player
PLAY/PAUSE|~/.mpdev/playpause
STICKER_EVENT|~/.mpdev/sticker
MIXER_EVENT|~/.mpdev/mixer
OPTIONS_EVENT|~/.mpdev/options
OUTPUT_EVENT|~/.mpdev/output
UPDATE_EVENT|~/.mpdev/update
DATABASE_EVENT|~/.mpdev/database
PLAYLIST_EVENT|~/.mpdev/playlist
STORED_PLAYLIST_EVENT|~/.mpdev/stored_playlist
PARTITION_EVENT|~/.mpdev/partition
SUBSCRIPTION_EVENT|~/.mpdev/subscription
MESSAGE_EVENT|~/.mpdev/message
MOUNT_EVENT|~/.mpdev/mount
NEIGHBOUR_EVENT|~/.mpdev/neighbour
CUSTOM_EVENT|~/.mpdev/custom

The hooks are passed the following arguments

```
mpd-event      - Passed when the above events listed, apart from SONG_CHANGE happen.
player-event   - Passed when you play/pause player
playlist-event - Passed when the playlist changes
now-playing    - Passed when a song starts playing
end-song       - Passed when a song finishes playing
```

The default installation makes a copy of `/usr/libexec/mpdev/player` in `$HOME/.mpdev` for the user with uid `1000`.

The **mpdev** package also comes with `mpdev_update` and `mpdev_cleanup` programs that help in maintaining the sqlite databases `stats.db` and `sticker.db`.

## Environment Variables available to hooks

Environment variable|Description
--------------------|-----------
SONG_ID|Set to the ID of song in mpd database
SONG_URI|Set to the full path of the music file
SONG_TITLE|Set to the title of the song
SONG_ARTIST|Set to the song artist
SONG_ALBUM|Set to the song album
SONG_DATE|Set to the Date tag of the song
SONG_GENRE|Set to the Genre tag of the song
SONG_TRACK|Set to the track number of the song
SONG_DURATION|Set to the song duration
SONG_PLAYED|Set to the duration for which the song was played
SONG_POSITION|Set to position of the current song
SONG_LAST_MODIFIED|Set to the last modified time of the song
START_TIME|Time at which the song play started
END_TIME|Time at which the song ended
ELAPSED_TIME|Set to time since player was paused
PLAYER_STATE|Set when you pause/resume player
DURATION|Set to song duration as a floating point number
SCROBBLER_LASTFM|Set to 1 if tracks are getting scrobbled to [lastfm](https://www.last.fm/)
SCROBBLER_LIBREFM|Set to 1 if tracks are getting scrobbled to [librefm](https://libre.fm/)
VERBOSE|Verbose level of mpdev

If you create the `stats` database, mpdev will update the last\_played field, play\_count fields in stats db. It will also update the song rating that you choose for the song. The ability to rate songs in mpd can be enabled by having the `sticker_file` keyword uncommented in `/etc/mpd.conf`. You will also need a mpd client that uses the mpd sticker command. One such player is `cantata`, which is available for all linux distros and Mac OSX. mpdev can also update [RompR](https://fatg3erman.github.io/RompR/) ratings and play counts and synchronize the ratings between [RompR](https://fatg3erman.github.io/RompR/) MySQL and sticker sqlite databases. Since mpdev runs in the background, it can keep updating [RompR](https://fatg3erman.github.io/RompR/), stats db play counts and history without you having to keep [RompR Web Frontend](https://fatg3erman.github.io/RompR/) running.

The sticker database can be enabled by having the followinng entry in `/etc/mpd.conf`

```
#
# The location of the sticker database.  This is a database which
# manages dynamic information attached to songs.
#
sticker_file                    "/var/lib/mpd/sticker.db"
#
```

If you want to use mpdev to update the [RompR](https://fatg3erman.github.io/RompR/) db, you need to set the following variables

```
ROMPR          - set this to any non-empty string (e.g. rompr)
MYSQL_HOST     - set this to the host on which rompr's MySQL database is running
MYSQL_PORT     - Port on which MySQL is running (usually 3306)
MYSQL_USER     - User for connecting to MySQL (e.g. rompr)
MYSQL_PASS     - Password for MYSQL_USER (e.g. romprdbpass)
MYSQL_DATABASE - Database name for rompr db (e.g. romprdb)
```

One can use [supervise](https://en.wikipedia.org/wiki/Daemontools) from the indimail-mta package to run this. The default rpm/debian installation of mpdev will that for you. But it will not enable rompr. To do that you need to run the following commands to create the above environment variables

```
$ sudo /bin/bash
# echo rompr         > /service/mpdev/variables/ROMPR
# echo 192.168.1.100 > /service/mpdev/variables/MYSQL_HOST # assuming you have MySQL database installed on 191.168.1.100
# echo 3306          > /service/mpdev/variables/MYSQL_PORT
# echo rompr         > /service/mpdev/variables/MYSQL_USER
# echo romprdbpas    > /service/mpdev/variables/MYSQL_PASS
# echo romprdb       > /service/mpdev/variables/MYSQL_DATABASE
# echo /home/pi      > /service/mpdev/variables/HOME
# echo /bin:/usr/bin > /service/mpdev/variables/PATH
# echo 6             > /service/mpdev/variables/AUTO_RATING
```

You need to restart mpdev for the new environment variables to be available to mpdev. To restart the mpdev daemon, you then just need to run the following command. `svc -d` stops the daemon, `svc -u` starts the daemon.

```
$ sudo svc -d /service/mpdev # this stops  the mpdev daemon
$ sudo svc -u /service/mpdev # this starts the mpdev daemon
```

If you want to do a source install and not have the supervise to run mpdev daemon, you could write a simple script and call it in a rc script during boot. If you don't use supervise, you need some knowledge of shell scripting. A very simple example of such a script is below. Another problem of not using supervise will be that you will have to enable your script to be called in some rc or systemd script whenever your machine is started.

```
#!/bin/sh
while true
do
    env ROMPR=rompr \
        MYSQL_HOST=192.168.1.100
        MYSQL_PORT=3306
        MYSQL_USER=rompr
        MYSQL_PASS=romprdbpass
        MYSQL_DATABASE=romprdb \
        HOME=/home/pi \
        XDG_RUNTIME_DIR=/home/pi \
        PATH=\$PATH:/usr/bin:/bin \
        AUTO_RATING=6 \
    /usr/bin/mpdev -v
	sleep 1
done
```

The stats database can be created running the `mpdev_update` program or by running sqlite3 program using the following SQL script.

```
CREATE TABLE IF NOT EXISTS song(
        id              INTEGER PRIMARY KEY,
        play_count      INTEGER DEFAULT 0,
        rating          INTEGER DEFAULT 0,
        uri             TEXT UNIQUE NOT NULL,
        duration        INTEGER,
        last_modified   INTEGER,
        date_added      INTEGER DEFAULT (strftime('%s','now')),
        artist          TEXT,
        album           TEXT,
        title           TEXT,
        track           TEXT,
        name            TEXT,
        genre           TEXT,
        date            TEXT,
        composer        TEXT,
        performer       TEXT,
        disc            TEXT,
        last_played     INTEGER,
        karma           INTEGER NOT NULL CONSTRAINT karma_percent CHECK (karma >= 0 AND karma <= 100) DEFAULT 50
);

CREATE INDEX rating on song(rating);
CREATE INDEX uri on song(uri);
CREATE INDEX last_played on song(last_played);
CREATE INDEX date_added on song(date_added);
CREATE INDEX last_modified on song(last_modified);
```

The scrobbler modules lastfm-scrobbler, librefm-scrobbler enables scrobbling to last.fm and libre.fm and can be enabled by running lastfm-scrobbler and librefm-scrobbler once as shown below

```
$ lastfm-scrobbler  --connect # for setting up lastfm
$ librefm-scrobbler --connect # for setting up librefm
```

After you have added the connection you should have lastfm-scrobbler.conf file in ~/.config/lastfm-scrobbler and librefm-scrobbler.conf file in ~/.config/librefm-scrobbler. These two files will have your `SESSION_KEY` and `API_KEY`. lastfm-scrobbler and librefm-scrobbler are a copy of moc-scrobbler from https://gitlab.com/pachanka/moc-scrobbler. mpdev will not do scrobbling if the `SESSION_KEY` and `API_KEY` haven't been setup.

By default mpdev will get the host name and port for mpd from MPD\_HOST and MPD\_PORT environment variables. MPD\_PASSWORD environment variable can be set to make mpdev connect to a password-protected mpd. If these environment variables aren’t set, mpdev connects to localhost on port 6600.. You can look at the logs while your songs are playing using `tail -f /var/log/svc/mpdev/current`

# Installation

## Source Installation

Take a look at BuildRequires field in mpdev.spec and Build-Depends field in debian/mpdev.dsc

```
$ ./configure --prefix=$prefix --libexecdir=$prefix/libexec/mpdev
$ make
$ sudo make install
```
## Create RPM / DEB

This is done by running the create\_rpm / create\_debian scripts. (Here `version` refers to the existing version of mpdev package)

```
$ pwd
/usr/local/src/mpdev
$ ./create_rpm
$ ls -l $HOME/rpmbuild/RPMS/x86_64/mpdev\*
-rw-rw-r--. 1 mbhangui mbhangui  290567 Feb  8 09:05 mpdev-0.1-1.1.fc31.x86_64.rpm
```

## Create debian package
```
$ pwd
/usr/local/src/mpdev
$ ./create_debian
$ ls -l $HOME/stage/mpdev*
-rw-r--r--  1 mbhangui mbhangui  558 Jul  2 19:30 mpdev.1-1.1_amd64.buildinfo
-rw-r--r--  1 mbhangui mbhangui  981 Jul  2 19:30 mpdev.1-1.1_amd64.changes
-rw-r--r--  1 mbhangui mbhangui 2916 Jul  2 19:30 mpdev.1-1.1_amd64.deb
```

## Prebuilt Binaries

Prebuilt binaries using openSUSE Build Service are available for mpdev for

* Debian (including ARM images for Debian 10 which work (and tested) for RaspberryPI (model 2,3 & 4) and Allo Sparky)
* openSUSE Tumbleweed
* Arch Linux
* Fedora
* Ubuntu

Use the below url for installation

https://software.opensuse.org//download.html?project=home%3Ambhangui%3Adietpi&package=mpdev
