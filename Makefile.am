@SET_MAKE@

COFLAGS=-z+05:30
ACLOCAL_AMFLAGS=-I m4

DEFS=@DEFS@ -Wall -Dlint -fno-strict-aliasing -I/usr/include/qmail -I.

INSTALL_PROGRAM = ${INSTALL}          -c
install_sh_PROGRAM = $(install_sh)    -c
INSTALL_SCRIPT = ${INSTALL}           -c
install_sh_SCRIPT = $(install_sh)     -c
INSTALL_STRIP_PROGRAM = $(install_sh) -c -s

mpdevbindir=@prefix@/bin
mpdevsbindir=@prefix@/sbin

mpdevbin_PROGRAMS = mpdev
mpdevbin_SCRIPTS = librefm-scrobbler lastfm-scrobbler mpdplaylist romprhist \
				   transfer_play
libexec_PROGRAMS = mpdev_cleanup mpdev_update
libexec_SCRIPTS = player playpause mpdev_rename create_service
noinst_DATA = mpdev.spec PKGBUILD
man_MANS = mpdplaylist.1 mpdev.1 lastfm-scrobbler.1 librefm-scrobbler.1 \
		 mpdev_update.1 mpdev_cleanup.1 transfer_play.1 romprhist.1
SYSCONFDIR=@sysconfdir@
sysconf_DATA=mpdev-release
doc_DATA=LICENSE moc-scrobbler-license mpdev.changes README.md

MOSTLYCLEANFILES = mpdev player playpause librefm-scrobbler \
				   lastfm-scrobbler mpdplaylist choose haveip6.h \
				   create_service romprhost

mpdev_SOURCES = mpdev.c tcpopen.c pathexec_env.c pathexec_run.c
mpdev_LDADD = $(LIB_QMAIL)

mpdev_update_SOURCES = mpdev_update.c tcpopen.c
mpdev_update_LDADD = $(LIB_QMAIL) $(LIB_SQLITE3)

mpdev_cleanup_SOURCES = mpdev_cleanup.c replacestr.c tcpopen.c
mpdev_cleanup_LDADD = $(LIB_QMAIL) $(LIB_SQLITE3)

edit = sed \
	-e 's}@PACKAGE\@}$(PACKAGE)}g' \
	-e 's}@PACKAGE_VERSION\@}$(PACKAGE_VERSION)}g' \
	-e 's}@version\@}$(VERSION)}g' \
	-e "s|@release\@|"`cat conf-release`"|g" \
	-e 's|@email\@|'"`cat conf-email`"'|g' \
	-e 's}@pkgconfigdir\@}$(pkgconfigdir)}g' \
	-e 's}@VERSION\@}$(VERSION)}g' \
	-e 's}@sysconfdir\@}$(sysconfdir)}g' \
	-e 's}@prefix\@}$(prefix)}g'

editlastfm = sed \
	-e 's}@name\@}lastfm}g' \
	-e 's}@auth_url\@}"www.last.fm/api/auth"}g' \
	-e 's}@api_url\@}"ws.audioscrobbler.com/2.0"}g' \
	-e 's}@host_url\@}"post.audioscrobbler.com"}g'

editlibrefm = sed \
	-e 's}@name\@}librefm}g' \
	-e 's}@auth_url\@}"libre.fm/api/auth/"}g' \
	-e 's}@api_url\@}"libre.fm/2.0/"}g' \
	-e 's}@host_url\@}"http://turtle.libre.fm"}g'

choose: choose.sh warn-auto.sh
	/bin/rm -f choose
	cat warn-auto.sh choose.sh > choose
	chmod 555 choose

haveip6.h: \
choose tryip6.c haveip6.h1 haveip6.h2 conf-ip
	$(COMPILE) -c `grep -h -v "^#" conf-ip` tryip6.c; \
	$(COMPILE)  tryip6.o -o tryip6; \
	./choose r tryip6 haveip6.h1 haveip6.h2 > haveip6.h

tcpopen.o: tcpopen.c tcpopen.h haveip6.h conf-ip
	$(COMPILE) -c $(DEFS) `grep -h -v "^#" conf-ip` tcpopen.c

librefm-scrobbler: moc-scrobbler.in
	$(editlibrefm) moc-scrobbler.in > $@
	chmod +x $@

lastfm-scrobbler: moc-scrobbler.in
	$(editlastfm) moc-scrobbler.in > $@
	chmod +x $@

mpdplaylist: mpdplaylist.in
	$(edit) $@.in > $@
	chmod +x $@

romprhist: romprhist.in
	$(edit) $@.in > $@
	chmod +x $@

create_service: create_service.in
	$(edit) $@.in > $@
	chmod +x $@

mpdev_rename: mpdev_rename.in
	$(edit) $@.in > $@
	chmod +x $@

create_stats: create_stats.in
	$(edit) $@.in > $@
	chmod +x $@

player: player.in
	$(edit) $@.in > $@
	chmod +x $@

playpause: playpause.in
	$(edit) $@.in > $@
	chmod +x $@

transfer_play: transfer_play.in
	$(edit) $@.in > $@
	chmod +x $@

mpdev.spec: mpdev.spec.in catChangeLog doc/ChangeLog conf-version \
conf-release conf-email
	(cat $@.in;./catChangeLog) | $(edit) > $@
mpdev.changes: doc/ChangeLog conf-version conf-release conf-email
	./catChangeLog --changes doc/ChangeLog > $@
PKGBUILD: PKGBUILD.in conf-email \
conf-version conf-release mpdev.changes
	cat $@.in | $(edit) > $@
