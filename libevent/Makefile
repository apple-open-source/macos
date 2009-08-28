##
# Makefile for libevent
##

# Project info
Project	    = libevent
ProjectName = libevent
UserType    = Developer
ToolType    = Library

Configure = $(BuildDirectory)/$(Project)/configure

# Private API only
Install_Prefix = $(USRDIR)/local/$(Project)

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make

#
# Automatic Extract & Patch
#

AEP	       = YES
AEP_ProjVers   = $(Project)-1.4.4-stable
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = 

lazy_install_source::
	$(_v) if [ ! -f "$(SRCROOT)/$(AEP_ProjVers)" ]; then $(MAKE) extract_source; fi

extract_source::
ifeq ($(AEP),YES)
	@echo "Extracting source for $(Project)...";
	$(_v) $(MKDIR) -p "$(BuildDirectory)";
	$(_v) $(TAR) -C "$(BuildDirectory)" -xzf "$(SRCROOT)/$(AEP_Filename)";
	$(_v) $(RMDIR) "$(BuildDirectory)/$(Project)";
	$(_v) $(MV) "$(BuildDirectory)/$(AEP_ExtractDir)" "$(BuildDirectory)/$(Project)";
	$(_v) for patchfile in $(AEP_Patches); do \
	   cd "$(BuildDirectory)/$(Project)" && patch -lp0 < "$(SRCROOT)/patches/$${patchfile}"; \
	done;
endif

#
# Open Source Hooey
#

OSV = /usr/local/OpenSourceVersions
OSL = /usr/local/OpenSourceLicenses

install:: install-ossfiles

install-ossfiles::
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(OSV)";
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(ProjectName).plist" "$(DSTROOT)/$(OSV)/$(ProjectName).plist";
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(OSL)";
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/license.txt" "$(DSTROOT)/$(OSL)/$(ProjectName).txt";
