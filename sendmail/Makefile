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
	$(_v) $(MAKE) -C $(Sources)/$(Project)	\
		Extra_CC_Flags="$(CFLAGS)"	\
		Extra_LD_Flags="$(LDFLAGS)"	\
		OPTIONS='-O "$(OBJROOT)"'

install:: install-sendmail install-cf install-doc install-contrib

install-sendmail:
	$(_v) umask $(Install_Mask) ;				\
	      $(MAKE) -C $(Sources)/$(Project)			\
		install-strip					\
		DESTDIR="$(DSTROOT)"				\
		STDIR="/private/var/log"			\
		OPTIONS='-O"$(OBJROOT)"'
	$(_v) umask $(Install_Mask) ;				\
	      $(MAKE) -C $(Sources)/$(Project)/mail.local	\
		force-install strip				\
		DESTDIR="$(DSTROOT)"				\
		STDIR="/private/var/log"			\
		OPTIONS='-O"$(OBJROOT)"'
	$(_v) umask $(Install_Mask) ;				\
	      $(MAKE) -C $(Sources)/$(Project)/rmail		\
		force-install strip				\
		DESTDIR="$(DSTROOT)"				\
		STDIR="/private/var/log"			\
		OPTIONS='-O"$(OBJROOT)"'

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

install-contrib:
	@echo "Installing contrib files..."
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(USRBINDIR)
	umask $(Install_Mask) ; $(INSTALL_DIRECTORY) $(DSTROOT)$(MANDIR)/man1
	$(INSTALL_SCRIPT) -c $(Project)/contrib/expn.pl $(DSTROOT)$(USRBINDIR)/expn
	$(LN) -fs $(USRBINDIR)/expn $(DSTROOT)$(MANDIR)/man1/expn.1
