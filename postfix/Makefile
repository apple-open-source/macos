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
CFLAGS=-Os $(RC_CFLAGS)

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
	$(ENV) $(MAKE) -C $(SRCROOT)/$(PROJECT) makefiles OPT="-DBIND_8_COMPAT -DHAS_SSL -DUSE_SASL_AUTH -I/AppleInternal/Developer/Headers/sasl -framework DirectoryService $(CFLAGS)" AUXLIBS="-L/usr/lib -lssl -lsasl2.2.0.1 -lgssapi_krb5"
	$(ENV) $(MAKE) -C $(SRCROOT)/$(PROJECT)
	cc $(CFLAGS) watchpostfix.c -o postfix-watch

install : pre-install
	install -d -m 755 $(DSTROOT)/System/Library/StartupItems/Postfix
	install -d -m 755 $(DSTROOT)/usr/libexec/postfix/scripts
	install -s -m 755 postfix-watch $(DSTROOT)/usr/sbin/postfix-watch
	rsync -a $(SRCROOT)/Postfix.StartupItem/ \
		 $(DSTROOT)/System/Library/StartupItems/Postfix
	cp $(SRCROOT)/mta_select $(DSTROOT)/usr/libexec/postfix
	cp $(SRCROOT)/aliases.db $(DSTROOT)/private/etc
	ln -s postfix/aliases $(DSTROOT)/private/etc
	install -m 0444 $(SRCROOT)/master.cf.defaultserver $(DSTROOT)/private/etc/postfix

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
