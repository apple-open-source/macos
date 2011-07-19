#
# xbs-compatible wrapper Makefile for postfix
#
# WARNING: B&I overrides the perms, owner, and group for everything
# under /Library; see <rdar://problem/8389433>

PROJECT=postfix

SHELL := /bin/sh

# Sane defaults, which are typically overridden on the command line.
SRCROOT=
OBJROOT=$(SRCROOT)
SYMROOT=$(OBJROOT)
DSTROOT=/usr/local
RC_ARCHS=
CFLAGS=-mdynamic-no-pic -Os $(RC_CFLAGS)
COMPRESS_PL=/Developer/Makefiles/bin/compress-man-pages.pl

clean : ;

installhdrs : ;

installsrc :
	[ ! -d $(SRCROOT)/$(PROJECT) ] && mkdir -p $(SRCROOT)/$(PROJECT)
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	find $(SRCROOT) -type d -name CVS -print0 | xargs -0 rm -rf

build :
	echo "ENV = $(ENV)"
	cd $(SRCROOT)/$(PROJECT) && patch -p1 < $(SRCROOT)/patches/postfix-2.8-patch01
	cd $(SRCROOT)/$(PROJECT) && patch -p1 < $(SRCROOT)/patches/postfix-2.8-patch02
	cd $(SRCROOT)/$(PROJECT) && patch -p1 < $(SRCROOT)/patches/postfix-2.8-patch03
	$(ENV) $(MAKE) -C $(SRCROOT)/$(PROJECT) makefiles OPT="-DNO_NETINFO -DUSE_TLS -DUSE_CYRUS_SASL -DUSE_SASL_AUTH -D__APPLE_OS_X_SERVER__ -DEVENTS_STYLE=EVENTS_STYLE_KQUEUE\
			-DHAS_DEV_URANDOM -DUSE_SYSV_POLL\
			-DHAS_PCRE \
			-DHAS_LDAP \
			-I/usr/include/sasl -F/System/Library/PrivateFrameworks -framework CoreDaemon -framework OpenDirectory $(CFLAGS)" \
			AUXLIBS="-L/usr/lib -lssl -lsasl2.2.0.1 -lgssapi_krb5 -lldap"
	$(ENV) $(MAKE) -C $(SRCROOT)/$(PROJECT)
	cd $(SRCROOT)/postfix/src/smtpstone && make all

install : pre-install
	install -d -m 755 $(DSTROOT)/System/Library/LaunchDaemons
	install -d -m 755 $(DSTROOT)/System/Library/ServerSetup/CleanInstallExtras
	install -d -m 755 $(DSTROOT)/System/Library/ServerSetup/MigrationExtras
	install -d -m 755 $(DSTROOT)/System/Library/ServerSetup/PromotionExtras
	install -d -m 755 $(DSTROOT)/usr/libexec/postfix/scripts
	install -d -m 755 $(DSTROOT)/usr/share/man/man1
	install -d -m 755 $(DSTROOT)/usr/local/OpenSourceVersions
	install -d -m 755 $(DSTROOT)/usr/local/OpenSourceLicenses
	install -d -m 755 $(DSTROOT)/usr/share/doc/postfix/html
	install -d -m 755 $(DSTROOT)/usr/share/doc/postfix/examples
	install -d -m 755 $(DSTROOT)/Library/Server/Mail/Data
	@echo "WARNING: B&I overrides the perms, owner, and group for everything under /Library; see <rdar://problem/8389433>"
	install -d -m 770 -o _postfix -g mail $(DSTROOT)/Library/Server/Mail/Data/mta
	install -d -m 755 $(DSTROOT)/private/var/lib
	install -d -m 755 $(DSTROOT)/private/var/spool
	ln -s ../../../Library/Server/Mail/Data/mta $(DSTROOT)/private/var/lib/postfix
	ln -s ../../../Library/Server/Mail/Data/spool $(DSTROOT)/private/var/spool/postfix
	ln -s postfix/aliases $(DSTROOT)/private/etc
	rm  $(DSTROOT)/private/etc/postfix/master.cf
	install -m 0644 $(SRCROOT)/Postfix.Config/main.cf.default $(DSTROOT)/private/etc/postfix
	install -m 0644 $(SRCROOT)/Postfix.Config/main.cf.default $(DSTROOT)/private/etc/postfix/main.cf
	install -m 0644 $(SRCROOT)/Postfix.Config/master.cf.default $(DSTROOT)/private/etc/postfix
	install -m 0644 $(SRCROOT)/Postfix.Config/master.cf.default $(DSTROOT)/private/etc/postfix/master.cf
	install -m 0644 $(SRCROOT)/Postfix.LaunchDaemons/org.postfix.master.plist \
			$(DSTROOT)/System/Library/LaunchDaemons/org.postfix.master.plist
	install -m 0755 $(SRCROOT)/Postfix.ServerSetup/postfix_config \
			$(DSTROOT)/System/Library/ServerSetup/CleanInstallExtras/SetupPostfix.sh
	install -m 0755 $(SRCROOT)/Postfix.ServerSetup/postfix_upgrade \
			$(DSTROOT)/System/Library/ServerSetup/MigrationExtras/64_postfix_migrator
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/qmqp-sink $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/smtp-sink $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/qmqp-source $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/smtp-source $(DSTROOT)/usr/libexec/postfix
	install -m 0444 $(SRCROOT)/Postfix.OpenSourceInfo/postfix.plist $(DSTROOT)/usr/local/OpenSourceVersions
	install -m 0444 $(SRCROOT)/Postfix.OpenSourceInfo/postfix.txt $(DSTROOT)/usr/local/OpenSourceLicenses
	install -m 644 $(SRCROOT)/Postfix.Config/postfix-files $(DSTROOT)/private/etc/postfix/postfix-files
	rm $(DSTROOT)/usr/libexec/postfix/postfix-files
	ln -s ../../../etc/postfix/postfix-files $(DSTROOT)/usr/libexec/postfix/postfix-files
	install -m 755 $(SRCROOT)/postfix/examples/smtpd-policy/greylist.pl $(DSTROOT)/usr/libexec/postfix
	install -m 0644 $(SRCROOT)/Postfix.Config/custom_header_checks $(DSTROOT)/private/etc/postfix
	cd $(DSTROOT)/usr/bin && ln -s ../sbin/sendmail newaliases
	cd $(DSTROOT)/usr/bin && ln -s ../sbin/sendmail mailq
	install -m 640 $(SRCROOT)/Postfix.Config/aliases.db $(DSTROOT)/private/etc/aliases.db
	cp $(SRCROOT)/postfix/conf/sample-*.cf $(DSTROOT)/usr/share/doc/postfix/examples
	/usr/bin/strip -S $(DSTROOT)/usr/libexec/postfix/nqmgr
	find $(DSTROOT)/usr/share/doc $(DSTROOT)/usr/share/man -type f -print | while read f; do \
		perl -p -i -e 's,(/private)?/var/spool/postfix,/Library/Server/Mail/Data/spool,g; \
			       s,(/private)?/var/lib/postfix,/Library/Server/Mail/Data/mta,g;' "$$f"; \
	done
	if [ -e $(COMPRESS_PL) ]; then\
		$(COMPRESS_PL) $(DSTROOT)/usr/share/man;\
	fi
	echo "------- setting defaults -------"
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e mail_owner=_postfix
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e setgid_group=_postdrop
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e mydomain_fallback=localhost
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e queue_directory=/Library/Server/Mail/Data/spool
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
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e mynetworks=127.0.0.0/8
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e smtpd_client_restrictions='permit_mynetworks permit_sasl_authenticated permit'
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e recipient_delimiter=+
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e tls_random_source=dev:/dev/urandom
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e smtpd_tls_ciphers=medium
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e inet_protocols=all
	# desktop -> server promotion
	cd $(DSTROOT)/System/Library/ServerSetup/PromotionExtras && ln -s ../CleanInstallExtras/SetupPostfix.sh
	rm $(DSTROOT)/private/etc/postfix/makedefs.out
	cd $(SRCROOT)/$(PROJECT) && make tidy

pre-install : build
	cd $(PROJECT)/$(SRCDIR) && \
	$(SHELL) -x postfix-install -non-interactive \
		install_root=$(DSTROOT) \
		tempdir=$(OBJROOT) \
		mail_owner=postfix \
		setgid_group=postdrop \
		config_directory=/private/etc/postfix \
		daemon_directory=/usr/libexec/postfix \
		command_directory=/usr/sbin \
		queue_directory=/Library/Server/Mail/Data/spool \
		sendmail_path=/usr/sbin/sendmail \
		newaliases_path=/usr/bin/newaliases \
		mailq_path=/usr/bin/mailq \
		manpage_directory=/usr/share/man \
		sample_directory=/usr/share/doc/postfix/examples \
		html_directory=/usr/share/doc/postfix/html \
		data_directory=/Library/Server/Mail/Data/mta \
		readme_directory=/usr/share/doc/postfix && \
	for F in $(DSTROOT)/usr/{{s,}bin,libexec/postfix}/*; do \
		echo "$$F" ; cp "$$F" $(SYMROOT); \
		[ -f "$$F" -a -x "$$F" ] && strip -x "$$F"; \
	done && \
	$(SHELL) -x conf/post-install set-permissions \
		mail_owner=postfix \
		setgid_group=postdrop \
		install_root=$(DSTROOT) \
		tempdir=$(OBJROOT) \
		config_directory=$(DSTROOT)/private/etc/postfix \
		daemon_directory=$(DSTROOT)/usr/libexec/postfix \
		command_directory=$(DSTROOT)/usr/sbin \
		queue_directory=$(DSTROOT)/Library/Server/Mail/Data/spool \
		sendmail_path=$(DSTROOT)/usr/sbin/sendmail \
		newaliases_path=$(DSTROOT)/usr/bin/newaliases \
		mailq_path=$(DSTROOT)/usr/bin/mailq \
		manpage_directory=$(DSTROOT)/usr/share/man \
		sample_directory=$(DSTROOT)/usr/share/doc/postfix/examples \
		html_directory=$(DSTROOT)/usr/share/doc/postfix/html \
		data_directory=$(DSTROOT)/Library/Server/Mail/Data/mta \
		readme_directory=$(DSTROOT)/usr/share/doc/postfix

.PHONY: clean installhdrs installsrc build install pre-install
