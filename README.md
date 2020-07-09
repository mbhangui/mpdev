# mpdev
music player daemon events daemon

**mpdev** is a music player daemon event watcher. It connects to the mpd socket and uses mpd's idle command to listen for player events. Whenever an event occurs, mpdev can carry out various activities using user defined hooks. The idea for doing mpdev comes from [mpdcron](https://alip.github.io/mpdcron/). `mpdev` is work in progress. It doesn't yet have a script to update the `stats` sqlite3 status database.

You can create scripts in $HOME/.mpdev directory. The default installation installs $HOME/.mpdev/player for the uid 1000. The script does the following

1. scrobbles titles to last.fm and libre.fm. You have to create API keys by running lastfm-scrobbler and librefm-scrobbler one time
2. updates play counts in the sqlite stats.db
3. Synchronizes the ratings in the sticker, rompr and the stats db. It also initializes the rating to 3 when you play an unrated song
```

## Environment Variables available to hooks

```
SONG_URI
SONG_LAST_MODIFIED
SONG_ALBUM
SONG_ARTIST
SONG_DATE
SONG_GENRE
SONG_TITLE
SONG_TRACK
SONG_DURATION
SONG_POSITION
SONG_ID
```

If you create the `stats` database, mpdev will update the last\_played field in the stats db. It will also update the song rating that you choose for the song. The ability to rate songs in mpd can be enabled by having the `sticker_file` keyword uncommented in `/etc/mpd.conf`. You will also need a mpd client that uses the mpd sticker command. One such player is `cantata`, which is available for all linux distros and Mac OSX.

```
#
# The location of the sticker database.  This is a database which
# manages dynamic information attached to songs.
#
sticker_file                    "/var/lib/mpd/sticker.db"
#
```

The stats database can be created running the `update_stats` program

```
CREATE TABLE song(
        id              INTEGER PRIMARY KEY,
        play_count      INTEGER,
        love            INTEGER,
        kill            INTEGER,
        rating          INTEGER,
        tags            TEXT NOT NULL,
        uri             TEXT UNIQUE NOT NULL,
        duration        INTEGER,
        last_modified   INTEGER,
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
        mb_artistid     TEXT,
        mb_albumid      TEXT,
        mb_trackid      TEXT,
        last_played     INTEGER,
        karma           INTEGER NOT NULL CONSTRAINT karma_percent CHECK (karma >= 0 AND karma <= 100) DEFAULT 50
);

CREATE TABLE artist(
        id              INTEGER PRIMARY KEY,
        play_count      INTEGER,
        tags            TEXT NOT NULL,
        name            TEXT UNIQUE NOT NULL,
        love            INTEGER,
        kill            INTEGER,
        rating          INTEGER);

CREATE TABLE album(
        id              INTEGER PRIMARY KEY,
        play_count      INTEGER,
        tags            TEXT NOT NULL,
        artist          TEXT,
        name            TEXT UNIQUE NOT NULL,
        love            INTEGER,
        kill            INTEGER,
        rating          INTEGER);

CREATE TABLE genre(
        id              INTEGER PRIMARY KEY,
        play_count      INTEGER,
        tags            TEXT NOT NULL,
        name            TEXT UNIQUE NOT NULL,
        love            INTEGER,
        kill            INTEGER,
        rating          INTEGER);
CREATE INDEX rating on song(rating);
CREATE INDEX uri on song(uri);
CREATE INDEX last_played on song(last_played);
PRAGMA user_version=11
```

The scrobbler modules lastfm-scrobbler, librefm-scrobbler enables scrobbling to last.fm and libre.fm.

By default mpdev will get the host name and port for mpd from MPD\_HOST and MPD\_PORT environment variables. MPD\_PASSWORD environment variable can be set to make mpdev connect to a password-protected mpd. If these environment variables aren’t set, mpdev connects to localhost on port 6600.

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

* Debian (including arm images for Debian 10 which work (and tested) for RaspberryPI (model 2,3 & 4) and Allo Sparky)
* Fedora

Use the below url for installation

https://software.opensuse.org//download.html?project=home%3Ambhangui%3Adietpi&package=mpdev
