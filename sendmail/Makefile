##
# Makefile for sendmail
##

# Project info
Project  = sendmail
UserType = Administration
ToolType = Services

# It's a 3rd Party Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

build::
	mkdir -p $(DSTROOT)/usr/share/man/man1
	mkdir -p $(DSTROOT)/usr/share/man/man5
	mkdir -p $(DSTROOT)/usr/share/man/man8
	mkdir -p $(DSTROOT)/usr/bin
	mkdir -p $(DSTROOT)/usr/sbin
	mkdir -p $(DSTROOT)/usr/libexec
	mkdir -p $(DSTROOT)/private/etc/mail
	mkdir -p $(DSTROOT)/private/var
	ln -sf private/etc $(DSTROOT)/etc
	ln -sf private/var $(DSTROOT)/var
	$(_v) $(MAKE) -C $(Sources)/$(Project)	\
		Extra_CC_Flags="$(CFLAGS) -DBIND_8_COMPAT"	\
		Extra_LD_Flags="$(LDFLAGS) -bind_at_load -force_flat_namespace"	\
		OPTIONS='-O "$(OBJROOT)"'

install:: install-sendmail install-cf install-doc

install-sendmail:
	$(_v) umask $(Install_Mask) ;				\
	      $(MAKE) -C $(Sources)/$(Project)			\
		install						\
		DESTDIR="$(DSTROOT)"				\
		STDIR="/private/var/log"			\
		OPTIONS='-O"$(OBJROOT)"'
	$(_v) umask $(Install_Mask) ;				\
	      $(MAKE) -C $(Sources)/$(Project)/mail.local	\
		force-install 					\
		DESTDIR="$(DSTROOT)"				\
		STDIR="/private/var/log"			\
		OPTIONS='-O"$(OBJROOT)"'
	strip -x $(DSTROOT)/usr/bin/vacation
	strip -x $(DSTROOT)/usr/libexec/mail.local
	strip -x $(DSTROOT)/usr/libexec/smrsh
	strip -x $(DSTROOT)/usr/sbin/editmap
	strip -x $(DSTROOT)/usr/sbin/mailstats
	strip -x $(DSTROOT)/usr/sbin/makemap
	strip -x $(DSTROOT)/usr/sbin/praliases
	strip -x $(DSTROOT)/usr/sbin/sendmail
	chown root.smmsp $(DSTROOT)/usr/sbin/sendmail
	chmod 4555 $(DSTROOT)/usr/sbin/sendmail
	echo "# sample access file" > $(DSTROOT)/private/etc/mail/access
	makemap hash $(DSTROOT)/private/etc/mail/access < $(DSTROOT)/private/etc/mail/access
	rm -f $(DSTROOT)/etc $(DSTROOT)/var


DATADIR       = $(SHAREDIR)/sendmail
CONFDIR       = $(DATADIR)/conf
CFDIR         = $(CONFDIR)/cf
DOMAINDIR     = $(CONFDIR)/domain
FEATUREDIR    = $(CONFDIR)/feature
HACKDIR       = $(CONFDIR)/hack
M4DIR         = $(CONFDIR)/m4
MAILERDIR     = $(CONFDIR)/mailer
OSTYPEDIR     = $(CONFDIR)/ostype
SHDIR         = $(CONFDIR)/sh
SITECONFIGDIR = $(CONFDIR)/siteconfig

install-cf:
	@echo "Installing supporting files..."
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(DATADIR)
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(CONFDIR)
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(CFDIR)
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(DOMAINDIR)
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(FEATUREDIR)
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(HACKDIR)
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(M4DIR)
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(MAILERDIR)
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(OSTYPEDIR)
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(SHDIR)
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(SITECONFIGDIR)
	$(INSTALL_FILE) -c $(Project)/cf/README          $(DSTROOT)$(CONFDIR)
	$(INSTALL_FILE) -c $(Project)/cf/cf/*.mc         $(DSTROOT)$(CFDIR)
	$(INSTALL_FILE) -c $(Project)/cf/domain/*.m4     $(DSTROOT)$(DOMAINDIR)
	$(INSTALL_FILE) -c $(Project)/cf/feature/*.m4    $(DSTROOT)$(FEATUREDIR)
	$(INSTALL_FILE) -c $(Project)/cf/hack/*.m4       $(DSTROOT)$(HACKDIR)
	$(INSTALL_FILE) -c $(Project)/cf/m4/*.m4         $(DSTROOT)$(M4DIR)
	$(INSTALL_FILE) -c $(Project)/cf/mailer/*.m4     $(DSTROOT)$(MAILERDIR)
	$(INSTALL_FILE) -c $(Project)/cf/ostype/*.m4     $(DSTROOT)$(OSTYPEDIR)
	$(INSTALL_FILE) -c $(Project)/cf/sh/*.sh         $(DSTROOT)$(SHDIR)
	$(INSTALL_FILE) -c $(Project)/cf/siteconfig/*.m4 $(DSTROOT)$(SITECONFIGDIR)
	$(INSTALL_FILE) -c $(SRCROOT)/README $(DSTROOT)/$(ETCDIR)/mail/
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(ETCDIR)/mail
	$(M4) -D_CF_DIR_=$(DSTROOT)$(CONFDIR)/ $(DSTROOT)$(M4DIR)/cf.m4                         \
		$(DSTROOT)$(CFDIR)/generic-darwin.mc > $(DSTROOT)$(ETCDIR)/mail/sendmail.cf
	$(CHMOD) 644 $(DSTROOT)$(ETCDIR)/mail/sendmail.cf
	$(INSTALL_FILE) -c /dev/null $(DSTROOT)$(ETCDIR)/mail/local-host-names

DOCSDIR = $(NSDOCUMENTATIONDIR)/$(UserType)/$(ToolType)/$(ProjectName)

install-doc:
	@echo "Installing documentation..."
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(DOCSDIR)
	$(INSTALL_FILE) -c $(Project)/FAQ		$(DSTROOT)$(DOCSDIR)
	$(INSTALL_FILE) -c $(Project)/KNOWNBUGS		$(DSTROOT)$(DOCSDIR)
	$(INSTALL_FILE) -c $(Project)/LICENSE		$(DSTROOT)$(DOCSDIR)
	$(INSTALL_FILE) -c $(Project)/README		$(DSTROOT)$(DOCSDIR)
	$(INSTALL_FILE) -c $(Project)/RELEASE_NOTES	$(DSTROOT)$(DOCSDIR)
	$(INSTALL_FILE) -c $(Project)/doc/op/op.ps	$(DSTROOT)$(DOCSDIR)
