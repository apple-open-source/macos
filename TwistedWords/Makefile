##
# Makefile for TwistedWords
##

# Project info
Project	    = TwistedWords
ProjectName = TwistedWords
UserType    = Developer
ToolType    = Library

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

PYTHON = /usr/bin/python
EXTRAS := $(shell $(PYTHON) -c 'import sys; print sys.prefix')/Extras

build:: extract_source
	$(_v) cd $(OBJROOT)/$(Project) && $(Environment) $(PYTHON) setup.py build

install::
	$(_v) cd $(OBJROOT)/$(Project) && $(Environment) $(PYTHON) setup.py install --home="$(EXTRAS)" --root="$(DSTROOT)";
ifdef RC_JASPER
	$(_v) for so in $$(find "$(DSTROOT)$(EXTRAS)" -type f -name '*.so'); do $(STRIP) -Sx "$${so}"; done
endif

#
# Automatic Extract & Patch
#

AEP	       = YES
AEP_ProjVers   = $(Project)-0.4.0
AEP_Filename   = $(AEP_ProjVers).tar.gz
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = 

extract_source::
ifeq ($(AEP),YES)
	@echo "Extracting source for $(Project)..."
	$(_v) $(MKDIR) -p $(OBJROOT)
	$(_v) $(TAR) -C $(OBJROOT) -xzf $(SRCROOT)/$(AEP_Filename)
	$(_v) $(RMDIR) $(OBJROOT)/$(Project)
	$(_v) $(MV) $(OBJROOT)/$(AEP_ExtractDir) $(OBJROOT)/$(Project)
	$(_v) for patchfile in $(AEP_Patches); do \
	   cd $(OBJROOT)/$(Project) && patch -lp0 < $(SRCROOT)/patches/$$patchfile; \
	done
endif

#
# Open Source Hooey
#

OSV = /usr/local/OpenSourceVersions
OSL = /usr/local/OpenSourceLicenses

install:: install-ossfiles

install-ossfiles::
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/$(OSV)
	$(_v) $(INSTALL_FILE) $(SRCROOT)/$(ProjectName).plist $(DSTROOT)/$(OSV)/$(ProjectName).plist
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)/$(OSL)
	$(_v) $(INSTALL_FILE) $(OBJROOT)/$(Project)/LICENSE $(DSTROOT)/$(OSL)/$(ProjectName).txt
