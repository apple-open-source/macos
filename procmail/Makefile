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

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 3.22
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = Makefile.1.diff \
                recommend.c.diff \
                PR-3076981.diff \
                PR-7556883.diff

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -zxf $(SRCROOT)/$(AEP_Filename)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		(cd $(SRCROOT)/$(Project) && patch -lp1 < $(SRCROOT)/patches/$$patchfile) || exit 1; \
	done
endif

