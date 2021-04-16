prefix=/usr
sysconfdir=/etc/mpdev
shareddir=/usr/share/mpdev
libexecdir=/usr/libexec/mpdev
mandir=/usr/share/man
logdir=/var/log/svc
servicedir=/service
version=$(shell cat ../conf-version)
release=$(shell cat ../conf-release)
email=$(shell cat ../conf-email)

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
	-e 's,@email\@,$(email),g' \
	-e 's,@DATE\@,$(DATE),g'

all: rules changelog preinst postinst prerm \
	mpdev.dsc mpdev.install \
	debian.tar.gz

preinst: preinst.in
	$(edit) $@.in > $@

postinst: postinst.in
	$(edit) $@.in > $@

prerm: prerm.in
	$(edit) $@.in > $@

rules: rules.in Makefile ../conf-version ../conf-release ../conf-email
	$(edit) $@.in > $@;chmod +x rules

mpdev.install: mpdev.install.in
	$(edit) $@.in > $@

changelog: ../doc/ChangeLog Makefile ../conf-version ../conf-release \
../conf-email
	../catChangeLog --debian --name=mpdev --state=unstable \
		--urgency=low ../doc/ChangeLog > $@
control: control.in ../conf-email
	$(edit) $@.in > $@
copyright: copyright.in ../conf-email
	$(edit) $@.in > $@

mpdev.dsc: mpdev.dsc.in Makefile ../conf-version ../conf-release ../conf-email
	$(edit) $@.in > $@

debian.tar.gz: copyright preinst postinst prerm rules changelog compat \
Makefile control mpdev.install docs
	tar cf - copyright preinst postinst prerm rules changelog \
	compat control mpdev.install docs | gzip > $@

clean:
	/bin/rm -f rules prerm preinst postinst *.dsc changelog \
	mpdev.install debian.tar.gz
