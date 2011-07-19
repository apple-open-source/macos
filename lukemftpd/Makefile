#
# Apple wrapper Makefile for tnftpd
# Copyright (c) 2008-2010 Apple Inc. All Rights Reserved.
##
#

# General project info for use with RC/GNUsource.make makefile
Project         = tnftpd
ProjectName     = lukemftpd
UserType        = Administrator
ToolType        = Commands
Submission      = 45

# Environment is passed to BOTH configure AND make, which can cause problems if these
# variables are intended to help configure, but not override the result.
Environment	= MAKEOBJDIR="$(BuildDirectory)" \
			INSTALL_ROOT="$(DSTROOT)"
Extra_CC_Flags	= -Os -mdynamic-no-pic -Wall -Wno-deprecated-declarations
# This allows extra variables to be passed _just_ to configure.
Extra_Configure_Environment	= CFLAGS="$$RC_CFLAGS $(Extra_CC_Flags)" \
					LDFLAGS="$$RC_CFLAGS $(Extra_CC_Flags)" \
					EXTRA_LIBS="-lpam"

# The configure flags are ordered to match current output of ./configure --help.
# Extra indentation represents suboptions.
Extra_Configure_Flags	= --prefix=$(USRDIR) --sbindir=$(LIBEXECDIR) \
			--sysconfdir=$(ETCDIR) \
			--enable-ipv6 --with-pam --with-gssapi

# Additional project info used with AEP
AEP		= YES
AEP_Version	= 20100324
AEP_LicenseFile	= $(Sources)/COPYING
AEP_Patches	= manpages.patch \
		PR-3795936.long-username.patch \
	        PR-3886477.ftpd.c.patch \
		PR-4608716.ls.c.patch \
		PR-4581099.ftpd.c.patch \
		PR-4616924.ftpd.c.patch \
		PR-5815072.ftpd.c.patch \
		print.c.patch \
		sacl.patch \
		gss.patch
AEP_ConfigDir	= $(ETCDIR)
AEP_ConfigFiles	= ftpd.conf
AEP_LaunchdConfigs	= ftp.plist
AEP_Binaries	= $(LIBEXECDIR)/*
GnuAfterInstall	= install-macosx archive-strip-binaries

# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: $(GnuAfterInstall)

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include AEP.make

# Override settings from above includes
BuildDirectory	= $(OBJROOT)/Build/$(AEP_Project)
Install_Target	= install
# This needs to be overridden because the project properly uses DESTDIR and
# INSTALL_ROOT (which is included in Environment).
Install_Flags	= DESTDIR="$(DSTROOT)"


# Build rules
install-macosx:
	@echo "Cleaning up install for Mac OS X..."
	$(MV) $(DSTROOT)$(LIBEXECDIR)/tnftpd $(DSTROOT)$(LIBEXECDIR)/ftpd
	$(LN) $(DSTROOT)$(MANDIR)/man5/ftpusers.5 $(DSTROOT)$(MANDIR)/man5/ftpchroot.5
	$(LN) $(DSTROOT)$(MANDIR)/man8/tnftpd.8 $(DSTROOT)$(MANDIR)/man8/ftpd.8
	@echo "Installing PAM configuration file..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(ETCDIR)/pam.d/
	$(INSTALL_FILE) -m 0644 ftpd $(DSTROOT)$(ETCDIR)/pam.d/
	@echo "Installing sample configuration files..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SHAREDIR)/ftpd/examples
	$(INSTALL_FILE) -c -m 0644  $(Sources)/examples/ftpd.conf $(DSTROOT)$(SHAREDIR)/ftpd/examples
	$(INSTALL_FILE) -c -m 0644  $(Sources)/examples/ftpusers $(DSTROOT)$(SHAREDIR)/ftpd/examples
	$(CHOWN) -R root:wheel $(DSTROOT)/
	@echo "Mac OS X-specific cleanup complete."
