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

#ENV = \
#        CFLAGS="-no-cpp-precomp $(RC_CFLAGS)" \
#        XCFLAGS="-no-cpp-precomp $(RC_CFLAGS)" \
#        RC_ARCHS="$(RC_ARCHS)" \
#        DYLD_LIBRARY_PATH="$(DSTROOT)/usr/lib"

clean : ;

installhdrs : ;

installsrc :
	[ ! -d $(SRCROOT)/$(PROJECT) ] && mkdir -p $(SRCROOT)/$(PROJECT)
	tar cf - . | (cd $(SRCROOT) ; tar xfp -)
	find $(SRCROOT) -type d -name CVS -print0 | xargs -0 rm -rf

build :
	echo "ENV = $(ENV)"
	$(ENV) $(MAKE) -C $(SRCROOT)/$(PROJECT) makefiles OPT="-DBIND_8_COMPAT -DHAS_SSL -DUSE_SASL_AUTH -D__APPLE__ \
			-I/AppleInternal/Developer/Headers/sasl -framework DirectoryService $(CFLAGS)" \
			AUXLIBS="-L/usr/lib -lssl -lsasl2.2.0.1 -lgssapi_krb5"
	$(ENV) $(MAKE) -C $(SRCROOT)/$(PROJECT)
	cd $(SRCROOT)/postfix/src/smtpstone && make all

install : pre-install
	install -d -m 755 $(DSTROOT)/System/Library/LaunchDaemons
	install -d -m 755 $(DSTROOT)/System/Library/ServerSetup/SetupExtras
	install -d -m 755 $(DSTROOT)/usr/libexec/postfix/scripts
	install -d -m 755 $(DSTROOT)/usr/share/man/man1
	install -d -m 755 $(DSTROOT)/usr/local/OpenSourceVersions
	install -d -m 755 $(DSTROOT)/usr/local/OpenSourceLicenses
	cp $(SRCROOT)/aliases.db $(DSTROOT)/private/etc
	ln -s postfix/aliases $(DSTROOT)/private/etc
	install -d -m 755 $(DSTROOT)/private/etc/postfix/sample/
	cp $(SRCROOT)/postfix/conf/sample/sample* $(DSTROOT)/private/etc/postfix/sample/
	install -m 0444 $(SRCROOT)/master.cf.defaultserver $(DSTROOT)/private/etc/postfix
	install -m 0644 $(SRCROOT)/Postfix.LaunchDaemons/org.postfix.master.plist \
			$(DSTROOT)/System/Library/LaunchDaemons/org.postfix.master.plist
	install -m 0755 $(SRCROOT)/Postfix.ServerSetup/toggle_on_demand \
			$(DSTROOT)/System/Library/ServerSetup/SetupExtras/toggle_on_demand
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/qmqp-sink $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/smtp-sink $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/qmqp-source $(DSTROOT)/usr/libexec/postfix
	install -s -m 0755 $(SRCROOT)/postfix/src/smtpstone/smtp-source $(DSTROOT)/usr/libexec/postfix
	install -m 0444 $(SRCROOT)/Postfix.OpenSourceInfo/postfix.plist $(DSTROOT)/usr/local/OpenSourceVersions
	install -m 0444 $(SRCROOT)/Postfix.OpenSourceInfo/postfix.txt $(DSTROOT)/usr/local/OpenSourceLicenses
	install -s -m 0755 $(DSTROOT)/usr/sbin/sendmail $(DSTROOT)/usr/bin/newaliases
	install -s -m 0755 $(DSTROOT)/usr/sbin/sendmail $(DSTROOT)/usr/bin/mailq
	rm $(DSTROOT)/private/etc/postfix/makedefs.out

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
