@SET_MAKE@

COFLAGS=-z+05:30
ACLOCAL_AMFLAGS=-I m4

DEFS=@DEFS@ -Wall -Dlint -fno-strict-aliasing -I/usr/include/qmail -I.

INSTALL_PROGRAM = ${INSTALL}          -c
install_sh_PROGRAM = $(install_sh)    -c
INSTALL_SCRIPT = ${INSTALL}           -c
install_sh_SCRIPT = $(install_sh)     -c
INSTALL_STRIP_PROGRAM = $(install_sh) -c -s

mpdevsysconfdir=@sysconfdir@/mpdev
mpdevbindir=@prefix@/bin
mpdevsbindir=@prefix@/sbin

mpdevbin_PROGRAMS = mpdev
mpdevbin_SCRIPTS=librefm-scrobbler lastfm-scrobbler mpdplaylist
libexec_SCRIPTS=mpdev_maintenance player create_service
noinst_DATA=mpdev.spec
man_MANS=mpdplaylist.1 mpdev.1 lastfm-scrobbler.1 librefm-scrobbler.1

MOSTLYCLEANFILES = mpdev player mpdev_maintenance librefm-scrobbler \
				   lastfm-scrobbler mpdplaylist choose haveip6.h

mpdev_SOURCES = mpdev.c tcpopen.c pathexec_env.c pathexec_run.c
mpdev_LDADD = $(LIB_QMAIL)

edit = sed \
	-e 's}@PACKAGE\@}$(PACKAGE)}g' \
	-e 's}@PACKAGE_VERSION\@}$(PACKAGE_VERSION)}g' \
	-e 's}@version\@}$(VERSION)}g' \
	-e "s|@release\@|"`cat conf-release`"|g" \
	-e 's}@pkgconfigdir\@}$(pkgconfigdir)}g' \
	-e 's}@VERSION\@}$(VERSION)}g' \
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

mpdev_maintenance: mpdev_maintenance.in
	$(edit) $@.in > $@
	chmod +x $@

mpdplaylist: mpdplaylist.in
	$(edit) $@.in > $@
	chmod +x $@

create_service: create_service.in
	$(edit) $@.in > $@
	chmod +x $@

player: player.in
	$(edit) $@.in > $@
	chmod +x $@

mpdev.spec: mpdev.spec.in catChangeLog ChangeLog conf-version conf-release
	(cat $@.in;./catChangeLog) | $(edit) > $@
