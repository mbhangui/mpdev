# mpdev
music player daemon events daemon

**mpdev** is a music player daemon event watcher. It connects to the mpd socket and uses mpd's idle command to listen for player events. Whenever an event occurs, mpdev can carry out various activities using user defined hooks.

You can create scripts in $HOME/.mpdev directory. You can have the below shell script as $HOME/.mpdev/player. It will get on a song change.

```
#!/bin/sh

get_mpd_conf_value()
{
	grep "^$1" /etc/mpd.conf|awk '{print $2}' | \
		sed -e 's{"{{g'
}

get_mpd_rating()
{
	uri=`echo $1|sed -e "s{'{''{g"`
	echo "select value from sticker where type='song' and uri='$uri';" | sqlite3 $sticker_file
	if [ $? -ne 0 ] ; then
		echo "error: select value from sticker where type='song' and uri=\"$uri\";" 1>&2
		exit 1
	fi
}

update_stats()
{
	uri=`echo $2|sed -e "s{'{''{g"`
	echo "update song set last_played=$1,rating=$3 where uri='$uri';" | sqlite3 $music_dir/.mpd/stats_dietpi.db
	if [ $? -ne 0 ] ; then
		echo "error: update song set last_played=$1,rating=$3 where uri='$uri';" 1>&2
		exit 1
	fi
}
music_dir=`get_mpd_conf_value music_directory`
if [ $? -ne 0 ] ; then
	echo "could not get music directory from mpd.conf" 1>&2
	exit 1
fi
sticker_file=`get_mpd_conf_value sticker_file`
if [ $? -ne 0 ] ; then
	echo "could not get sticker database from mpd.conf" 1>&2
	exit 1
fi

if [ -n "$MPD_SONG_URI" ] ; then
	tmval=`date +'%s'`
	rating=`get_mpd_rating "$MPD_SONG_URI"`
	if [ -z "$rating" ] ; then
		rating=0
	fi
	update_stats $tmval "$MPD_SONG_URI" "$rating"
fi
exit 0
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

If you enable the `stats` database, mpdev will update the last\_played field in the stats db. It will also update the song rating that you choose for the song. The ability to rate songs in mpd can be enabled by having the `sticker_file` keyword uncommented in `/etc/mpd.conf`. You will also need a mpd client that uses the mpd sticker command. One such player is `cantata`, which is available for all linux distros and Mac OSX.

```
#
# The location of the sticker database.  This is a database which
# manages dynamic information attached to songs.
#
sticker_file                    "/var/lib/mpd/sticker.db"
#
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
/usr/local/src/mpdev-0.1
$ ./create_rpm
$ ls -l $HOME/rpmbuild/RPMS/x86_64/mpdev\*
-rw-rw-r--. 1 mbhangui mbhangui  290567 Feb  8 09:05 mpdev-0.1-1.1.fc31.x86_64.rpm
```

## Create debian package
```
$ pwd
/usr/local/src/mpdev-0.1
$ ./create_debian
$ ls -l $HOME/stage/mpdev*
-rw-r--r--  1 mbhangui mbhangui  558 Jul  2 19:30 mpdev.1-1.1_amd64.buildinfo
-rw-r--r--  1 mbhangui mbhangui  981 Jul  2 19:30 mpdev.1-1.1_amd64.changes
-rw-r--r--  1 mbhangui mbhangui 2916 Jul  2 19:30 mpdev.1-1.1_amd64.deb
```

## Prebuilt Binaries

Prebuilt binaries using openSUSE Build Service are available for mpdev for

* Debian (including arm images for Debian 10 which work for RaspberryPI and Allo Sparky)
* Fedora
* Ubuntu

Use the below url for installation

https://software.opensuse.org//download.html?project=home%3Ambhangui&package=mpdev
