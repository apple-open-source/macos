##
# Makefile for diffstat
##

# Project info
Project           = diffstat
UserType          = Developer
ToolType          = Commands
GnuAfterInstall   = install-plist install-strip
Extra_CC_Flags    = -mdynamic-no-pic

install:: shadow_source

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

Install_Target = install

# Automatic Extract & Patch
AEP            = YES
AEP_Project    = $(Project)
AEP_Version    = 1.41
AEP_ProjVers   = $(AEP_Project)-$(AEP_Version)
AEP_Filename   = $(Project).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    =

ifeq ($(suffix $(AEP_Filename)),.bz2)
AEP_ExtractOption = j
else
AEP_ExtractOption = z
endif

# Extract the source.
install_source::
ifeq ($(AEP),YES)
	$(TAR) -C $(SRCROOT) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(SRCROOT)/$(AEP_Project)
	$(MV) $(SRCROOT)/$(AEP_ExtractDir) $(SRCROOT)/$(AEP_Project)
	for patchfile in $(AEP_Patches); do \
		cd $(SRCROOT)/$(Project) && patch -p0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
	done
endif

OSV	= $(DSTROOT)/usr/local/OpenSourceVersions
OSL	= $(DSTROOT)/usr/local/OpenSourceLicenses

install-plist:
	$(MKDIR) $(OSV)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).plist $(OSV)/$(Project).plist
	$(MKDIR) $(OSL)
	$(INSTALL_FILE) $(SRCROOT)/$(Project).txt $(OSL)/$(Project).txt

install-strip:
	$(CP) $(DSTROOT)/usr/bin/diffstat $(SYMROOT)
	$(STRIP) -x $(DSTROOT)/usr/bin/diffstat
