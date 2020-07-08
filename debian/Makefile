prefix=/usr
sysconfdir=/etc/mpdev
shareddir=/usr/share/mpdev
libexecdir=/usr/libexec/mpdev
mandir=/usr/share/man
logdir=/var/log/svc
servicedir=/service
version=$(shell cat ../conf-version)
release=$(shell cat ../conf-release)
edit = sed \
	-e 's,@prefix\@,$(prefix),g' \
	-e 's,@sysconfdir\@,$(sysconfdir),g' \
	-e 's,@shareddir\@,$(shareddir),g' \
	-e 's,@libexecdir\@,$(libexecdir),g' \
	-e 's,@mandir\@,$(mandir),g' \
	-e 's,@logdir\@,$(logdir),g' \
	-e 's,@servicedir\@,$(servicedir),g' \
	-e 's,@version\@,$(version),g' \
	-e 's,@release\@,$(release),g' \
	-e 's,@DATE\@,$(DATE),g'

all: rules changelog postinst prerm \
	mpdev.dsc mpdev.install \
	debian.tar.gz

postinst: postinst.in
	$(edit) $@.in > $@

prerm: prerm.in
	$(edit) $@.in > $@

rules: rules.in Makefile ../conf-version ../conf-release
	$(edit) $@.in > $@

mpdev.install: mpdev.install.in
	$(edit) $@.in > $@

changelog: changelog.in ../conf-version ../conf-release
	(cat $@.in; echo ""; \
	echo " -- Manvendra Bhangui <mpdev@indimail.org>  `date +'%a, %d %b %Y %H:%M:%S %z'`" ) \
	| $(edit) > $@

mpdev.dsc: mpdev.dsc.in Makefile ../conf-version ../conf-release
	$(edit) $@.in > $@

debian.tar.gz: copyright postinst prerm rules changelog compat \
Makefile control mpdev.install
	tar cf - copyright postinst prerm rules changelog \
	compat control mpdev.install | gzip > $@

clean:
	/bin/rm -f rules prerm postinst *.dsc changelog \
	mpdev.install debian.tar.gz
