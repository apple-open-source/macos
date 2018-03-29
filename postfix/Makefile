#
# xbs-compatible wrapper Makefile for postfix
#

PROJECT=postfix
OPEN_SOURCE_VERSION=3.2.2

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
SDKROOT=
RC_ARCHS=
CFLAGS=$(RC_CFLAGS)

WARN = -Wall -Wno-comment -Wformat -Wimplicit -Wmissing-prototypes \
	-Wparentheses -Wstrict-prototypes -Wswitch -Wuninitialized \
	-Wunused -Wno-missing-braces -Wno-deprecated

BuildDirectory	= $(OBJROOT)/Build
TMPDIR		= $(OBJROOT)/Build/tmp
Sources		= $(SRCROOT)

SMTPSTONE	= $(BuildDirectory)/$(PROJECT)/src/smtpstone/qmqp-sink $(BuildDirectory)/$(PROJECT)/src/smtpstone/smtp-sink \
			$(BuildDirectory)/$(PROJECT)/src/smtpstone/qmqp-source $(BuildDirectory)/$(PROJECT)/src/smtpstone/smtp-source

ifneq "" "$(SDKROOT)"
  CC_PATH = $(shell xcrun -find -sdk $(SDKROOT) cc)
else
  CC_PATH = $(shell xcrun -find cc)
endif

install :: copy-src apply-patches build-postfix install-postfix archive-strip-binaries \
		post-install install-extras

clean : ;

installhdrs : ;

installsrc :
	@echo "***** installsrc"
	[ ! -d $(SRCROOT)/$(PROJECT) ] && mkdir -p $(SRCROOT)/$(PROJECT)
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	find $(SRCROOT) -type d -name CVS -print0 | xargs -0 rm -rf
	@echo "***** installsrc complete"

copy-src :
	@echo "***** copy source"
	$(_v) /bin/mkdir -p $(BuildDirectory)/$(PROJECT)
	$(_v) /bin/mkdir -p $(TMPDIR)
	$(_v) /bin/cp -rpf $(Sources)/$(PROJECT) $(BuildDirectory)
	@echo "***** copy source complete"

apply-patches :
	@echo "***** applying patches "
	$(_v) cd $(BuildDirectory)/$(PROJECT) && patch -p1 < "$(SRCROOT)/patches/server-config.patch"
	$(_v) cd $(BuildDirectory)/$(PROJECT) && patch -p1 < "$(SRCROOT)/patches/libressl.patch"
	$(_v) cd $(BuildDirectory)/$(PROJECT) && patch -p1 < "$(SRCROOT)/patches/build.patch"
	@echo "***** creating MIG API files "
	$(_v) cd $(BuildDirectory)/$(PROJECT)/src/global && mig -v "$(SDKROOT)/usr/local/include/opendirectory/DSlibinfoMIG.defs"
	@echo "***** creating MIG API files complete"

build-postfix :
	@echo "***** building $(PROJECT)"
	@echo "*** build environment = $(ENV)"
	$(ENV) $(MAKE) -C $(BuildDirectory)/$(PROJECT) makefiles CC="$(CC_PATH)" \
		OPT=" -g -Os" \
		CCARGS=" -DNO_NETINFO \
			 -DUSE_TLS -I$(SDKROOT)/usr/local/libressl/include \
			 -DUSE_CYRUS_SASL -DUSE_SASL_AUTH -I$(SDKROOT)/usr/include/sasl \
			 -DEVENTS_STYLE=EVENTS_STYLE_KQUEUE \
			 -DHAS_DEV_URANDOM -DUSE_SYSV_POLL -DHAS_PCRE -DHAS_LDAP \
			 -F$(SDKROOT)/System/Library/Frameworks \
			 -F$(SDKROOT)/System/Library/PrivateFrameworks \
			 $(CFLAGS) $(WARN)" \
		AUXLIBS="-lssl.35 -lcrypto.35 -lpcre -lsasl2.2.0.1 -lgssapi_krb5 -lldap -licucore"
	$(ENV) $(MAKE) -C $(BuildDirectory)/$(PROJECT)
	@echo "*** building: smtpstone"
	cd $(BuildDirectory)/postfix/src/smtpstone && make all
	@echo "***** building $(PROJECT) complete"

install-postfix :
	@echo "***** installing $(PROJECT)"
	@echo "*** executing $(SHELL) -x postfix-install"
	cd $(BuildDirectory)/$(PROJECT) && \
	$(SHELL) -x postfix-install -non-interactive \
		install_root=$(DSTROOT) \
		tempdir=$(OBJROOT) \
		mail_owner=_postfix \
		setgid_group=_postdrop \
		config_directory=/private/etc/postfix \
		daemon_directory=/usr/libexec/postfix \
		command_directory=/usr/sbin \
		queue_directory=/private/var/spool/postfix \
		sendmail_path=/usr/sbin/sendmail \
		newaliases_path=/usr/bin/newaliases \
		mailq_path=/usr/bin/mailq \
		manpage_directory=/usr/share/man \
		meta_directory=/private/etc/postfix \
		sample_directory=/usr/share/doc/postfix/examples \
		html_directory=/usr/share/doc/postfix/html \
		data_directory=/private/var/lib/postfix \
		readme_directory=/usr/share/doc/postfix
	@echo "****** installing $(PROJECT) complete"

archive-strip-binaries :
	@echo "****** archiving & stripping binaries"
	rm -r $(BuildDirectory)/$(PROJECT)/src/local/local.dSYM
	for file in $(DSTROOT)/usr/{{s,}bin,libexec/postfix}/* $(SMTPSTONE); do \
		echo "*** Processing $${file##*/} (from $${file})"; \
		if [ ! -e "$(SYMROOT)/$${file##*/}" ]; then \
			echo "** /bin/cp $${file} $(SYMROOT)"; \
			/bin/cp $${file} $(SYMROOT); \
		fi; \
		if [ -e "$(SYMROOT)/$${file##*/}.dSYM" ]; then \
			echo "** ...odd, dSYM already exists."; \
		else \
			echo "** /usr/bin/dsymutil --out=$(SYMROOT)/$${file##*/}.dSYM $${file}"; \
			/usr/bin/dsymutil --out=$(SYMROOT)/$${file##*/}.dSYM $${file}; \
		fi; \
		strip -x $${file}; \
	done
	@echo "***** archiving & stripping binaries complete"

post-install :
	@echo "***** post-install $(PROJECT)"
	@echo "*** executing $(SHELL) -x post-install"
	cd $(BuildDirectory)/$(PROJECT) && \
	$(SHELL) -x conf/post-install set-permissions \
		mail_owner=_postfix \
		setgid_group=_postdrop \
		install_root=$(DSTROOT) \
		tempdir=$(OBJROOT) \
		config_directory=$(DSTROOT)/private/etc/postfix \
		daemon_directory=$(DSTROOT)/usr/libexec/postfix \
		command_directory=$(DSTROOT)/usr/sbin \
		queue_directory=$(DSTROOT)/private/var/spool/postfix \
		sendmail_path=$(DSTROOT)/usr/sbin/sendmail \
		newaliases_path=$(DSTROOT)/usr/bin/newaliases \
		mailq_path=$(DSTROOT)/usr/bin/mailq \
		manpage_directory=$(DSTROOT)/usr/share/man \
		meta_directory=$(DSTROOT)/private/etc/postfix \
		sample_directory=$(DSTROOT)/usr/share/doc/postfix/examples \
		html_directory=$(DSTROOT)/usr/share/doc/postfix/html \
		data_directory=$(DSTROOT)/private/var/lib/postfix \
		readme_directory=$(DSTROOT)/usr/share/doc/postfix
	@echo "*****  post-install $(PROJECT) complete"

install-extras : 
	@echo "***** installing extras"
	@echo "*** installing directories"
	install -d -m 755 $(DSTROOT)/Library/LaunchDaemons
	install -d -m 755 $(DSTROOT)/System/Library/LaunchDaemons
	install -d -m 755 $(DSTROOT)/usr/libexec/postfix/scripts
	install -d -m 755 $(DSTROOT)/usr/share/man/man1
	install -d -m 755 $(DSTROOT)/usr/local/OpenSourceVersions
	install -d -m 755 $(DSTROOT)/usr/local/OpenSourceLicenses
	install -d -m 755 $(DSTROOT)/usr/share/doc/postfix/html
	install -d -m 755 $(DSTROOT)/usr/share/doc/postfix/examples
	install -d -m 755 $(DSTROOT)/private/var/db
	install -d -m 755 $(DSTROOT)/private/var/lib
	install -d -m 755 $(DSTROOT)/private/var/spool
	ln -s postfix/aliases $(DSTROOT)/private/etc
	@echo "*** installing default config files"
	rm  $(DSTROOT)/private/etc/postfix/master.cf
	install -m 0644 $(SRCROOT)/Postfix.Config/main.cf.default $(DSTROOT)/private/etc/postfix
	install -m 0644 $(SRCROOT)/Postfix.Config/main.cf.default $(DSTROOT)/private/etc/postfix/main.cf
	install -m 0644 $(SRCROOT)/Postfix.Config/master.cf.default $(DSTROOT)/private/etc/postfix
	install -m 0644 $(SRCROOT)/Postfix.Config/master.cf.default $(DSTROOT)/private/etc/postfix/master.cf
	install -m 0644 $(SRCROOT)/Postfix.LaunchDaemons/com.apple.postfix.master.plist \
			$(DSTROOT)/System/Library/LaunchDaemons/com.apple.postfix.master.plist
	install -m 0644 $(SRCROOT)/Postfix.LaunchDaemons/com.apple.postfix.newaliases.plist \
			$(DSTROOT)/System/Library/LaunchDaemons/com.apple.postfix.newaliases.plist
	install -m 0755 $(SRCROOT)/Postfix.ServerSetup/set_credentials.sh $(DSTROOT)/usr/libexec/postfix/set_credentials.sh
	install -m 0755 -o 0 -g 0 $(SRCROOT)/Postfix.ServerSetup/mk_postfix_spool.sh $(DSTROOT)/usr/libexec/postfix/mk_postfix_spool.sh
	install -m 0755 -o 0 -g 0 $(SRCROOT)/Postfix.ServerSetup/postfixsetup $(DSTROOT)/usr/libexec/postfix/postfixsetup
	install -m 0644 -o 0 -g 0 $(SRCROOT)/Postfix.LaunchDaemons/com.apple.postfixsetup.plist $(DSTROOT)/Library/LaunchDaemons/com.apple.postfixsetup.plist
	@echo "*** Installing smtpstone binaries"
	install -s -m 0755 $(BuildDirectory)/$(PROJECT)/src/smtpstone/qmqp-sink $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(BuildDirectory)/$(PROJECT)/src/smtpstone/smtp-sink $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(BuildDirectory)/$(PROJECT)/src/smtpstone/qmqp-source $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(BuildDirectory)/$(PROJECT)/src/smtpstone/smtp-source $(DSTROOT)/usr/libexec/postfix
	@echo "*** Installing open source version files"
	install -m 0444 $(SRCROOT)/Postfix.OpenSourceInfo/Postfix.plist $(DSTROOT)/usr/local/OpenSourceVersions
	install -m 0444 $(SRCROOT)/Postfix.OpenSourceInfo/Postfix.txt $(DSTROOT)/usr/local/OpenSourceLicenses
	@echo "*** installing custome runtime files"
	#install -m 644 $(SRCROOT)/Postfix.Config/postfix-files $(DSTROOT)/private/etc/postfix/postfix-files
	#rm $(DSTROOT)/usr/libexec/postfix/postfix-files
	#ln -s ../../../etc/postfix/postfix-files $(DSTROOT)/usr/libexec/postfix/postfix-files
	install -m 755 $(SRCROOT)/postfix/examples/smtpd-policy/greylist.pl $(DSTROOT)/usr/libexec/postfix
	install -m 0644 $(SRCROOT)/Postfix.Config/custom_header_checks $(DSTROOT)/private/etc/postfix
	cd $(DSTROOT)/usr/bin && ln -s ../sbin/sendmail newaliases
	cd $(DSTROOT)/usr/bin && ln -s ../sbin/sendmail mailq
	install -m 640 $(SRCROOT)/Postfix.Config/aliases.db $(DSTROOT)/private/etc/aliases.db
	@echo "***** installing extras complete"

.PHONY: clean installhdrs installsrc build install pre-install
