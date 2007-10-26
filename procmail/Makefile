##
# Makefile for procmail
##

# Project info
Project           = procmail
UserType          = Administration
ToolType          = Services
Extra_CC_Flags    = -mdynamic-no-pic
Extra_Environment = LDFLAGS0="" LOCKINGTEST="/tmp" \
		    BASENAME="$(USRDIR)" MANDIR=$(MANDIR)

# It's a 3rd Party Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

Install_Flags = BASENAME="$(DSTROOT)$(USRDIR)" MANDIR=$(DSTROOT)$(MANDIR)

lazy_install_source:: shadow_source

build::
	@echo "Building $(Project)..."
	$(_v) $(MAKE) -C $(BuildDirectory)/$(Project) $(Environment)

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install::
	@echo "Installing $(Project)..."
	$(_v) $(MAKE) -C $(BuildDirectory)/$(Project) $(Environment) $(Install_Flags) install install-suid
	$(_v) cd $(DSTROOT)$(USRBINDIR) && strip *
	chgrp mail $(DSTROOT)/usr/bin/procmail
	chmod g+s $(DSTROOT)/usr/bin/procmail
	chgrp mail $(DSTROOT)/usr/bin/lockfile
	chmod g+s $(DSTROOT)/usr/bin/lockfile
	$(INSTALL_FILE) $(SRCROOT)/mailstat.1 $(DSTROOT)/usr/share/man/man1
# COMPRESSMANPAGES use DSTROOT automatically
	$(COMPRESSMANPAGES) /usr/share/man
# Install plist
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/LICENSE $(OSL)/$(Project).txt

