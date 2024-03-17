# mpdev

[![mpdev C/C++ CI - Linux, Mac OSX](https://github.com/mbhangui/mpdev/actions/workflows/mpdev-c-cpp.yml/badge.svg)](https://github.com/mbhangui/mpdev/actions/workflows/mpdev-c-cpp.yml)

**mpdev** is a music player daemon event watcher. It connects to the mpd socket and uses mpd's idle command to listen for player events. Whenever an event occurs, mpdev can carry out various activities using user defined hooks. The idea for doing mpdev comes from [mpdcron](https://alip.github.io/mpdcron/). Currently mpdev runs on linux and Mac OSX. For Linux you have binary package which mostly automates the installation and setup. For Mac OSX, one needs to manually compile, install and configure things like startup, configure environment variables, creating the sqlite3 database.


mpdev helps in bulding a database of your played tracks. Along with a script `mpdplaylist`, it can generate a playlist for mpd as per your taste and mood.

You can create scripts in $HOME/.mpdev directory. The default installation installs two scripts player and playpause in $HOME/.mpdev directory for uid 1000. The scripts are adequate for most use cases. The player script does the following

1. scrobbles titles to last.fm and libre.fm. You have to create API keys by running lastfm-scrobbler and librefm-scrobbler one time. You can disable scrobbling by setting DISABLE\_SCROBBLE environment variable.
2. updates play counts in the sqlite stats.db. You could write your own script and update any external database.
3. Synchronizes the ratings in the sticker (sqlite). You could write your own script and update any external database. You can also automatically rate songs to some default value by setting an environment variable AUTO\_RATING by creating a file $HOME/.mpdev/AUTO\_RATING. If you are using supervise from deamontools (more on that below), creating an environment variable is very easy. e.g. To have an environment variable AUTO\_RATING with value 6, you just need to have a file name AUTO\_RATING in /service/mpdev/variables. The file should just have 6 as the content.
4. Update the song's karma. Karma is a number ranging from 0 to 100. If a song is skipped, it's karma is downgraded by 1. Karma can be downgraded only for songs rated less than 6 and played 5 times or less and whose karma is 50 or less. If a song is played twice within a day, it's karma is upgraded by 4. If a song is played twice within a week, it's karma is upgraded by 3. If a song is played twice within 14 days, its karma is upgraded by 2. If a song is played twice within a month, it's karma is upgraded by 1. All songs start with a default karma of 50. A song earns a permanent karma if any of the below happen

	  * it's karma becomes 60 or more.
	  * has been rated 6 or greater.
	  * has been played from beginning to end for more than 5 times.
	
The above four are actually done by running a hook, a script named `player` in $HOME/.mpdev directory. You can put your own script named `player` in this directory. In fact mpdev can run specific hooks for specific types of mpd events. A hook can be any executable program or script. It will be passed arguments and have certain environment variables related to the song being played, available to it. Below is a list of of events and corresponding hooks that will be executed if available.

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
mpd-event      - Passed when the above events listed (apart from SONG_CHANGE) happen.
player-event   - Passed when you play or pause song
playlist-event - Passed when the playlist changes
now-playing    - Passed when a song starts playing
end-song       - Passed when a song finishes playing
```

The default installation makes a copy of `/usr/libexec/mpdev/player` and `/usr/libexec/mpdev/playpause` in `$HOME/.mpdev` for the user with uid `1000`. The player script inserts or updates songs in the stat.db, sticker.db sqlite database when a song gets played. For bulk inserts, updates, deletion, the **mpdev** package also comes with `mpdev_update(1)` and `mpdev_cleanup(1)` programs that help in maintaining `stats.db`, `sticker.db` sqlite databases. This two programs use SQL Transactions and are extremely fast and takes just a few seconds to create/update the stats.db, sticker.db sqlite databases. You can disable all database updates by the player script, by setting **NO_DB_UPDATE** environment variable. In such a case, the player script will just print information about the song being played and actions like pause, stop, play. The default installation also makes a copy of `/usr/libexec/mpdev/output` and `/usr/libexec/mpdev/mixer` in `$HOME/mpdev`. At the moment, these scripts just print information of the state of output devices and the volume level. You can edit them if you can think of something useful. If you are installing mpdev from the [Open Build Service repository](https://software.opensuse.org//download.html?project=home%3Ambhangui%3Araspi&package=mpdev), the mpdev sevice will automatically update these scripts if they change in the package. If you don't want auto-udpate you can create an override file. e.g. If you create a file $HOME/.mpdev/.player.nooverwrite, the script $HOME/.mpdev/player will never be replaced with a newer script from the binary package.

Example 1. create stats.db in the current directory

```
$ time mpdev_update -S -j -t -D 0 -d stats.db
Processed 42630 rows, Failures 0 rows, Updated 42636 rows

real    0m0.830s
user    0m0.405s
sys     0m0.096s
```

Example 2. Update stats.db in the current directory to add new songs

```
$ time mpdev_update -S -j -t -D 0 -d stats.db
Processed 42636 rows, Failures 0 rows, Updated 6 rows

real    0m0.725s
user    0m0.353s
sys     0m0.067s
```

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
SONG_PLAYED_DURATION|Set to the duration for which the song was played
ELAPSED_TIME|Total time elapsed within the current song in seconds, but with higher resolution
SONG_POSITION|Set to position of the current song
SONG_LAST_MODIFIED|Set to the last modified time of the song
START_TIME|Time at which the song play started
END_TIME|Time at which the song ended
PLAYER_STATE|Set when you pause/resume player
DURATION|Set to song duration as a floating point number
SCROBBLER_LASTFM|Set to 1 if tracks are getting scrobbled to [lastfm](https://www.last.fm/). Will not be set if DISABLE_SCROBBLE environment variable is set.
SCROBBLER_LIBREFM|Set to 1 if tracks are getting scrobbled to [librefm](https://libre.fm/). Will not be set if DISABLE_SCROBBLE environment variable is set.
OUTPUT_CHANGED|Set when you enable or disable any output devices. This indicates the new state of an output device.
VOLUME|Set during startup and when you change mixer volume. This indicates the volume level as a percentage.
VERBOSE|Verbose level of mpdev

Apart from the above environment variables, the state of all output device are available as environment variable to the script ~/.mpdev/output. Below example is for a case where three devices listed in /etc/mpd.conf - (Piano DAC Plus 2.1 with ID 0, Xonar EssenceOne with ID 1 and Scarlett 2i2 USB with ID 2). e.g. To check if the first device is enabled, you can just query the environment variable **OUTPUT_0_STATE**. Whenever the state of any output device is changed (enabled or disabled), the script ~/.mpdev/output gets called with access to the following environment variables.

```
OUTPUT_0_ID=0
OUTPUT_0_NAME=Piano DAC Plus 2.1
OUTPUT_0_STATE=enabled
OUTPUT_1_ID=1
OUTPUT_1_NAME=Xonar EssenceOne
OUTPUT_1_STATE=disabled
OUTPUT_2_ID=2
OUTPUT_2_NAME=Scarlett 2i2 USB
OUTPUT_2_STATE=disabled
```

If you create the `stats` database, mpdev will update the last\_played field, play\_count fields in stats db. It will also update the song rating that you choose for the song. The ability to rate songs in mpd can be enabled by having the `sticker_file` keyword uncommented in `/etc/mpd.conf`. You will also need a mpd client that uses the mpd sticker command. One such player is `cantata`, which is available for all linux distros and Mac OSX.

The sticker database can be enabled by having the followinng entry in `/etc/mpd.conf`

```
#
# The location of the sticker database.  This is a database which
# manages dynamic information attached to songs.
#
sticker_file                    "/var/lib/mpd/sticker.db"
#
```

You need to restart mpdev for the new environment variables to be available to mpdev. To restart the mpdev daemon, you then just need to run the following command `svc -r /service/mpdev`. Below are few operations to stop, start and restart mpdev.

```
$ sudo svc -d /service/mpdev # this stops  the mpdev daemon
$ sudo svc -u /service/mpdev # this starts the mpdev daemon
$ sudo svc -r /service/mpdev # this stops and restarts the mpdev daemon
```

If you do a source install and want to have mpdev started by supervise you need to do the following

```
$ sudo apt-get install daemontools
$ sudo ./create_service --servicedir=/service --servicedir=/service \
    --user=1000 --add-service

If you want mpdev to print song information to a LCD display on a remote
host running lcdDaemon
$ sudo ./create_service --servicedir=/service --servicedir=/service \
    --user=1000 --lcdhost=IP --lcdport=1806 --add-service
```

If you want to do a source install and not have the supervise to run mpdev daemon, you could write a simple script and call it in a rc script during boot. If you don't use supervise, you need some knowledge of shell scripting. A very simple example of such a script is below. Another problem of not using supervise will be that you will have to enable your script to be called in some rc or systemd script whenever your machine is started.

```
#!/bin/sh
while true
do
    env HOME=/home/pi \
        MPDEV_TMPDIR=/tmp/mpdev \
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

To use lastfm-scrobbler, librefm-scrobbler, you need to have your own [last.fm API keys](https://www.last.fm/api/account/create). Once you have created your API keys, you can view the same at the [api accounts page](https://www.last.fm/api/accounts).

The scrobbler modules lastfm-scrobbler, librefm-scrobbler enables scrobbling to last.fm and libre.fm and can be enabled by running lastfm-scrobbler and librefm-scrobbler once as shown below

```
$ lastfm-scrobbler  --connect --api_key="abc123" --api_sec="xyz890"
$ librefm-scrobbler --connect --api_key="abc123" --api_sec="xyz890"
```

As you can see above, the same key generated for lastfm can be used for librefm.

After you have added the connection you should have lastfm-scrobbler.conf file in ~/.config/lastfm-scrobbler and librefm-scrobbler.conf file in ~/.config/librefm-scrobbler. These two files will have your `SESSION_KEY`, `API_KEY` and `API_SEC`. lastfm-scrobbler and librefm-scrobbler are a copy of moc-scrobbler from https://gitlab.com/pachanka/moc-scrobbler. moc-scrobbler is distributed under the terms of [MIT License](https://gitlab.com/pachanka/moc-scrobbler/-/blob/master/license.md). mpdev will not do scrobbling if the `SESSION_KEY`, `API_KEY` and `API_SEC` haven't been setup, or if the environment variable DISABLE\_SCROBBLE is set

By default mpdev will get the host name and port for mpd from MPD\_HOST and MPD\_PORT environment variables. MPD\_PASSWORD environment variable can be set to make mpdev connect to a password-protected mpd. If these environment variables aren’t set, mpdev connects to localhost on port 6600.. You can look at the logs while your songs are playing using `tail -f /var/log/svc/mpdev/current`

# Installation

## Source Installation

Take a look at BuildRequires field in mpdev.spec and Build-Depends field in debian/mpdev.dsc

mpdev uses library from [libqmail](https://github.com/mbhangui/libqmail). If you are building mpdev from source, you need to install libqmail.

```
$ cd /usr/local/src
$ git clone --no-tags --no-recurse-submodules --depth=1 https://github.com/mbhangui/libqmail
$ cd libqmail
$ ./default.configure
$ make && sudo make install-strip
```

```
$ ./configure --prefix=$prefix --libexecdir=$prefix/libexec/mpdev
$ make
$ sudo make install
```
## Create RPM / Debian packages

This is done by running the create\_rpm / create\_debian scripts. (Here `version` refers to the existing version of mpdev package)

### Create debian package

```
$ pwd
/usr/local/src/mpdev
$ ./create_rpm
$ ls -l $HOME/rpmbuild/RPMS/x86_64/mpdev\*
-rw-rw-r--. 1 mbhangui mbhangui  290567 Feb  8 09:05 mpdev-0.1-1.1.fc31.x86_64.rpm
```

### Create debian package
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

[![mpdev](https://build.opensuse.org/projects/home:mbhangui:raspi/packages/mpdev/badge.svg?type=percent)](https://build.opensuse.org/package/show/home:mbhangui:raspi/mpdev)

Prebuilt binaries using openSUSE Build Service are available for mpdev for

* Debian (including ARM images for Debian 10 which work (and tested) for RaspberryPI (model 2,3 & 4) and Debian 9 for Allo Sparky)
* Raspbian 10 and 11 for RaspberryPI (ARM images)
* openSUSE Tumbleweed (x86\_64)
* Arch Linux (x86\_64)
* Fedora (x86\_64)
* Ubuntu (x86\_64 and ARM images for BananaPI)

Use the below url for installation

https://software.opensuse.org//download.html?project=home%3Ambhangui%3Araspi&package=mpdev

## IMPORTANT NOTE for debian if you are going to use supervise from daemontools package

debian/ubuntu repositories already has daemontools and ucspi-tcp which are far behind in terms of feature list that the indimail-mta repo provides. When you install mpdev, apt-get may pull the wrong version with limited features. Also `apt-get install mpdev` will get installed with errors, leading to an incomplete setup. You need to ensure that the two packages get installed from the indimail-mta repository instead of the debian repository. If you don't do this, mpdev will not function correctly as it depends on setting of proper global envirnoment variables. Global environment variabels are not supported by daemontools from the official debian repository. Additionally, the official ucspi-tcp package from the debian repository doesn't support TLS, which will result in services that depend on TLS not functioning.

All you need to do is set a higher preference for the indimail-mta repository by creating /etc/apt/preferences.d/preferences with the following contents

```
$ sudo /bin/bash
# cat > /etc/apt/preferences.d/preferences <<EOF
Package: *
Pin: origin download.opensuse.org
Pin-Priority: 1001
EOF
```

You can verify this by doing

```
$ apt policy daemontools ucspi-tcp
daemontools:
  Installed: 2.11-1.1+1.1
  Candidate: 2.11-1.1+1.1
  Version table:
     1:0.76-7 500
        500 http://raspbian.raspberrypi.org/raspbian buster/main armhf Packages
 *** 2.11-1.1+1.1 1001
       1001 http://download.opensuse.org/repositories/home:/indimail/Debian_10  Packages
        100 /var/lib/dpkg/status
ucspi-tcp:
  Installed: 2.11-1.1+1.1
  Candidate: 2.11-1.1+1.1
  Version table:
     1:0.88-6 500
        500 http://raspbian.raspberrypi.org/raspbian buster/main armhf Packages
 *** 2.11-1.1+1.1 1001
       1001 http://download.opensuse.org/repositories/home:/indimail/Debian_10/ Packages
        100 /var/lib/dpkg/status
```

# Building your own mpd music server

Take a look at [pistop](https://github.com/mbhangui/pistop) and this [README](https://github.com/mbhangui/pistop/blob/master/example_config/README.md).
