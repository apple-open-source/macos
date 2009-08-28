##
# Makefile for gperf
##

# Project info
Project		= gperf
UserType	= Developer
ToolType	= Commands
Extra_CC_Flags	= -mdynamic-no-pic
GnuAfterInstall	= post-install install-plist
ifeq ($(shell tconf --test TARGET_OS_EMBEDDED),YES)
Extra_Configure_Flags = --exec-prefix=$(DSTROOT)/usr/local
Extra_Install_Flags = TARGETPROG=pgperf
endif
# It's a GNU Source project
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target	= install

post-install:
ifeq ($(shell tconf --test TARGET_OS_EMBEDDED),YES)
	strip $(DSTROOT)/usr/local/bin/pgperf
else
	strip $(DSTROOT)/usr/bin/gperf
endif

OSV = $(DSTROOT)/usr/local/OpenSourceVersions
OSL = $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(Sources)/COPYING $(OSL)/$(Project).txt

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 3.0.3
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = offsetof.patch

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif
