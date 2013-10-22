#
# xbs-compatible wrapper Makefile for postfix
#

PROJECT=postfix

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
SDKROOT=
RC_ARCHS=
CFLAGS=-g -Os $(RC_CFLAGS)


BuildDirectory	= $(OBJROOT)/Build
TMPDIR		= $(OBJROOT)/Build/tmp
Sources		= $(SRCROOT)

SMTPSTONE	= $(BuildDirectory)/$(PROJECT)/src/smtpstone/qmqp-sink $(BuildDirectory)/$(PROJECT)/src/smtpstone/smtp-sink \
			$(BuildDirectory)/$(PROJECT)/src/smtpstone/qmqp-source $(BuildDirectory)/$(PROJECT)/src/smtpstone/smtp-source


install :: copy-src apply-patches build-postfix install-postfix archive-strip-binaries \
		post-install install-extras set-defaults

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
	@echo "***** applying patches"
	cd "$(BuildDirectory)/$(PROJECT)" && patch -p1 < "$(SRCROOT)/patches/postfix-2.9-patch03.txt"
	cd "$(BuildDirectory)/$(PROJECT)" && patch -p1 < "$(SRCROOT)/patches/postfix-2.9-patch04.txt"
	@echo "***** applying patches complete"
	@echo "***** creating MIG API files "
	$(_v) cd $(BuildDirectory)/$(PROJECT)/src/global && mig -v "$(SDKROOT)/usr/local/include/opendirectory/DSlibinfoMIG.defs"
	@echo "***** creating MIG API files complete"

build-postfix :
	@echo "***** building $(PROJECT)"
	@echo "*** build environment = $(ENV)"
	$(ENV) $(MAKE) -C $(BuildDirectory)/$(PROJECT) makefiles OPT="-DNO_NETINFO -DUSE_TLS -DUSE_CYRUS_SASL -DUSE_SASL_AUTH -D__APPLE_OS_X_SERVER__ \
			-DEVENTS_STYLE=EVENTS_STYLE_KQUEUE \
			-DHAS_DEV_URANDOM -DUSE_SYSV_POLL -DHAS_PCRE -DHAS_LDAP \
			-I$(SDKROOT)/usr/include \
			-I$(SDKROOT)/usr/include/sasl \
			-I$(SDKROOT)/usr/local/include \
			-F$(SDKROOT)/System/Library/Frameworks \
			-F$(SDKROOT)/System/Library/PrivateFrameworks \
			$(CFLAGS)" AUXLIBS="-L$(SDKROOT)/usr/lib -lssl -lsasl2.2.0.1 -lgssapi_krb5 -lldap"
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
		sample_directory=$(DSTROOT)/usr/share/doc/postfix/examples \
		html_directory=$(DSTROOT)/usr/share/doc/postfix/html \
		data_directory=$(DSTROOT)/private/var/lib/postfix \
		readme_directory=$(DSTROOT)/usr/share/doc/postfix
	@echo "*****  post-install $(PROJECT) complete"

install-extras : 
	@echo "***** installing extras"
	@echo "*** installing directories"
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
	install -m 0644 $(SRCROOT)/Postfix.LaunchDaemons/org.postfix.master.plist \
			$(DSTROOT)/System/Library/LaunchDaemons/org.postfix.master.plist
	install -m 0755 $(SRCROOT)/Postfix.ServerSetup/set_credentials.sh $(DSTROOT)/usr/libexec/postfix/set_credentials.sh
	@echo "*** Installing smtpstone binaries"
	install -s -m 0755 $(BuildDirectory)/$(PROJECT)/src/smtpstone/qmqp-sink $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(BuildDirectory)/$(PROJECT)/src/smtpstone/smtp-sink $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(BuildDirectory)/$(PROJECT)/src/smtpstone/qmqp-source $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(BuildDirectory)/$(PROJECT)/src/smtpstone/smtp-source $(DSTROOT)/usr/libexec/postfix
	@echo "*** Installing open source version files"
	install -m 0444 $(SRCROOT)/Postfix.OpenSourceInfo/postfix.plist $(DSTROOT)/usr/local/OpenSourceVersions
	install -m 0444 $(SRCROOT)/Postfix.OpenSourceInfo/postfix.txt $(DSTROOT)/usr/local/OpenSourceLicenses
	@echo "*** installing custome runtime files"
	install -m 644 $(SRCROOT)/Postfix.Config/postfix-files $(DSTROOT)/private/etc/postfix/postfix-files
	rm $(DSTROOT)/usr/libexec/postfix/postfix-files
	ln -s ../../../etc/postfix/postfix-files $(DSTROOT)/usr/libexec/postfix/postfix-files
	install -m 755 $(SRCROOT)/postfix/examples/smtpd-policy/greylist.pl $(DSTROOT)/usr/libexec/postfix
	install -m 0644 $(SRCROOT)/Postfix.Config/custom_header_checks $(DSTROOT)/private/etc/postfix
	cd $(DSTROOT)/usr/bin && ln -s ../sbin/sendmail newaliases
	cd $(DSTROOT)/usr/bin && ln -s ../sbin/sendmail mailq
	install -m 640 $(SRCROOT)/Postfix.Config/aliases.db $(DSTROOT)/private/etc/aliases.db
	@echo "***** installing extras complete"

set-defaults :
	@echo "***** setting default cofig values"
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e mail_owner=_postfix
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e setgid_group=_postdrop
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e mydomain_fallback=localhost
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e queue_directory=/private/var/spool/postfix
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e command_directory=/usr/sbin
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e daemon_directory=/usr/libexec/postfix
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e sendmail_path=/usr/sbin/sendmail
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e newaliases_path=/usr/bin/newaliases
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e mailq_path=/usr/bin/mailq
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e manpage_directory=/usr/share/man
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e sample_directory=/usr/share/doc/postfix/examples
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e html_directory=/usr/share/doc/postfix/html
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e readme_directory=/usr/share/doc/postfix
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e message_size_limit=10485760
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e mailbox_size_limit=0
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e biff=no
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e mynetworks='127.0.0.0/8, [::1]/128'
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e smtpd_client_restrictions='permit_mynetworks permit_sasl_authenticated permit'
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e recipient_delimiter=+
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e tls_random_source=dev:/dev/urandom
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e smtpd_tls_ciphers=medium
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e inet_protocols=all
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e inet_interfaces=loopback-only
	@echo "****** setting default cofig values complete"

.PHONY: clean installhdrs installsrc build install pre-install
