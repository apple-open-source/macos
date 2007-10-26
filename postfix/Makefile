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
	$(ENV) $(MAKE) -C $(SRCROOT)/$(PROJECT) makefiles OPT="-DBIND_8_COMPAT -DUSE_TLS -DUSE_CYRUS_SASL -DUSE_SASL_AUTH -D__APPLE_OS_X_SERVER__\
			-I/usr/include/sasl -framework DirectoryService $(CFLAGS)" AUXLIBS="-L/usr/lib -lssl -lsasl2.2.0.1 -lgssapi_krb5"
	$(ENV) $(MAKE) -C $(SRCROOT)/$(PROJECT)
	cd $(SRCROOT)/postfix/src/smtpstone && make all

install : pre-install
	install -d -m 755 $(DSTROOT)/System/Library/LaunchDaemons
	install -d -m 755 $(DSTROOT)/System/Library/ServerSetup/SetupExtras
	install -d -m 755 $(DSTROOT)/System/Library/ServerSetup/MigrationExtras
	install -d -m 755 $(DSTROOT)/usr/libexec/postfix/scripts
	install -d -m 755 $(DSTROOT)/usr/share/man/man1
	install -d -m 755 $(DSTROOT)/usr/local/OpenSourceVersions
	install -d -m 755 $(DSTROOT)/usr/local/OpenSourceLicenses
	ln -s postfix/aliases $(DSTROOT)/private/etc
	rm  $(DSTROOT)/private/etc/postfix/master.cf
	install -m 0444 $(SRCROOT)/Postfix.Config/main.cf.default $(DSTROOT)/private/etc/postfix
	install -m 0444 $(SRCROOT)/Postfix.Config/master.cf.default $(DSTROOT)/private/etc/postfix/master.cf
	install -m 0444 $(SRCROOT)/Postfix.Config/master.cf.default $(DSTROOT)/private/etc/postfix
	install -m 0644 $(SRCROOT)/Postfix.LaunchDaemons/org.postfix.master.plist \
			$(DSTROOT)/System/Library/LaunchDaemons/org.postfix.master.plist
	install -m 0755 $(SRCROOT)/Postfix.ServerSetup/postfix_config \
			$(DSTROOT)/System/Library/ServerSetup/SetupExtras/postfix_config
	install -m 0755 $(SRCROOT)/Postfix.ServerSetup/postfix_upgrade \
			$(DSTROOT)/System/Library/ServerSetup/MigrationExtras/postfix_upgrade
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/qmqp-sink $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/smtp-sink $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/qmqp-source $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/smtp-source $(DSTROOT)/usr/libexec/postfix
	install -m 0444 $(SRCROOT)/Postfix.OpenSourceInfo/postfix.plist $(DSTROOT)/usr/local/OpenSourceVersions
	install -m 0444 $(SRCROOT)/Postfix.OpenSourceInfo/postfix.txt $(DSTROOT)/usr/local/OpenSourceLicenses
	cd $(DSTROOT)/usr/bin && ln -s ../sbin/sendmail newaliases
	cd $(DSTROOT)/usr/bin && ln -s ../sbin/sendmail mailq
	install -m 644 $(SRCROOT)/Postfix.Config/aliases.db $(DSTROOT)/private/etc/aliases.db
	/usr/bin/strip -S $(DSTROOT)/usr/libexec/postfix/nqmgr
	if [ -e $(COMPRESS_PL) ]; then\
		$(COMPRESS_PL) $(DSTROOT)/usr/share/man;\
	fi
	echo "------- setting defaults -------"
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
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e readme_directory=/usr/share/doc/postfix
	$(DSTROOT)/usr/sbin/postconf -c $(DSTROOT)/private/etc/postfix -e message_size_limit=10485760
	mkfifo -m 622 $(DSTROOT)/private/var/spool/postfix/public/pickup
	chown _postfix:_postdrop $(DSTROOT)/private/var/spool/postfix/public/pickup
 	#cd $(DSTROOT)/private/etc && $(DSTROOT)/usr/sbin/postmap aliases
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
		queue_directory=/private/var/spool/postfix \
		sendmail_path=/usr/sbin/sendmail \
		newaliases_path=/usr/bin/newaliases \
		mailq_path=/usr/bin/mailq \
		manpage_directory=/usr/share/man \
		sample_directory=/usr/share/doc/postfix/examples \
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
		queue_directory=$(DSTROOT)/private/var/spool/postfix \
		sendmail_path=$(DSTROOT)/usr/sbin/sendmail \
		newaliases_path=$(DSTROOT)/usr/bin/newaliases \
		mailq_path=$(DSTROOT)/usr/bin/mailq \
		manpage_directory=$(DSTROOT)/usr/share/man \
		sample_directory=$(DSTROOT)/usr/share/doc/postfix/examples \
		readme_directory=$(DSTROOT)/usr/share/doc/postfix

.PHONY: clean installhdrs installsrc build install pre-install
