##
# Apple wrapper Makefile for mailman list server
# Copyright (c) 2003 by Apple Computer, Inc.
##
# Although it is a GNU-like project, it does not come with a Makefile,
# and the configure script requires user interaction. This Makefile just provides
# the targets required by Apple's build system, and creates a config file
# config.php by modifying the default config file config_default.php
#
# This Makefile tries to conform to hier(7) by moving config, data, attachments
# to appropriate places in the file system, and makes symlinks where necessary.

Project		= mailman
UserType	= Administrator
ToolType	= Services
Install_Prefix	= "$(SHAREDIR)/$(Project)"
Extra_Configure_Flags = --localstatedir="$(VARDIR)/$(Project)" \
	--with-var-prefix="$(VARDIR)/$(Project)" \
	--with-mail-gid=mailman \
	--with-cgi-gid=www \
	--without-permcheck

GnuNoChown = YES
GnuAfterInstall	= install-strip install-extras install-readmes install-startup

SILENT=$(_v)
ECHO=echo
SO_STRIPFLAGS=-rx

HTTPDIR=/usr/share/httpd/icons
SADIR=$(NSSYSTEMDIR)$(NSLIBRARYSUBDIR)/ServerSetup/SetupExtras

SIDIR=$(NSSYSTEMDIR)$(NSLIBRARYSUBDIR)/StartupItems/Mailman
SI_SCRIPT=Mailman.StartupItem/Mailman
SI_PLIST=Mailman.StartupItem/StartupParameters.plist

READMEFILES=FAQ NEWS README README-I18N.en README.CONTRIB README.EXIM README.MACOSX README.NETSCAPE README.POSTFIX README.QMAIL README.SENDMAIL README.USERAGENT

# Configuration values we customize
#

# These includes provide the proper paths to system utilities
#
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

DESTDIR="$(DSTROOT)"

# These need to be overridden to match the project's use of DESTDIR.
Environment	=
Install_Flags	= DESTDIR="$(DSTROOT)"
Install_Target	= install


# Build rules

# Custom configuration:
#

install-strip:
	$(SILENT) $(ECHO) "Stripping language libraries..."
	$(SILENT) -$(STRIP) $(SO_STRIPFLAGS) \
		$(DSTROOT)$(Install_Prefix)/pythonlib/japanese/c/_japanese_codecs.so \
		$(DSTROOT)$(Install_Prefix)/pythonlib/korean/c/_koco.so \
		$(DSTROOT)$(Install_Prefix)/pythonlib/korean/c/hangul.so
	$(SILENT) $(ECHO) "done."

install-extras:
	$(SILENT) $(ECHO) "Installing extras..."
	$(MKDIR) "$(DSTROOT)/$(HTTPDIR)"
	$(SILENT) $(CP) -p "$(DSTROOT)$(Install_Prefix)"/icons/* \
		"$(DSTROOT)$(HTTPDIR)"
	$(MKDIR) "$(DSTROOT)$(SADIR)"
	$(INSTALL_SCRIPT) SetupScript "$(DSTROOT)$(SADIR)/Mailman"
	$(SILENT) $(ECHO) "MTA = 'Postfix'" \
		>> "$(DSTROOT)$(Install_Prefix)/Mailman/mm_cfg.py.dist"
	$(RM) "$(DSTROOT)$(Install_Prefix)/Mailman/mm_cfg.py"
	$(FIND) "$(DSTROOT)$(Install_Prefix)" -type f -name '*.py' \
		-exec $(CHMOD) g-w {} \;
	$(FIND) "$(DSTROOT)$(Install_Prefix)" -perm +02000 \
		-exec $(CHGRP) mailman {} \;
	$(CHGRP) -R mailman "$(DSTROOT)$(VARDIR)/$(Project)"
	$(SILENT) $(ECHO) "done."

install-readmes:
	$(SILENT) $(ECHO) "Installing Read Me files..."
	$(MKDIR) "$(RC_Install_HTML)"
	for file in $(READMEFILES); \
	do \
		$(INSTALL_FILE) "$(SRCROOT)/$(Project)/$$file" "$(RC_Install_HTML)"; \
	done
	$(SILENT) $(ECHO) "done."

install-startup:
	$(SILENT) $(ECHO) "Installing Startup Item..."
	$(MKDIR) "$(DSTROOT)$(SIDIR)"
	$(INSTALL_SCRIPT) "$(SRCROOT)/$(SI_SCRIPT)" "$(DSTROOT)$(SIDIR)"
	$(INSTALL_FILE) "$(SRCROOT)/$(SI_PLIST)" "$(DSTROOT)$(SIDIR)"
	$(SILENT) $(ECHO) "done."
