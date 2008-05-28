##
# Makefile for Twisted
##

# Project info
Project	    = Twisted
ProjectName = Twisted
UserType    = Developer
ToolType    = Library

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

PYTHON = /usr/bin/python
EXTRAS := $(shell $(PYTHON) -c 'import sys; print sys.prefix')/Extras
CORE_SCRIPTS = trial twistd

build:: extract_source
	$(_v) cd "$(OBJROOT)/$(Project)" && $(Environment) $(PYTHON) setup.py build;

install::
	$(_v) cd "$(OBJROOT)/$(Project)" && $(Environment) $(PYTHON) setup.py install --home="$(EXTRAS)" --root="$(DSTROOT)";
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(USRBINDIR)";
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(MANDIR)/man1";
	$(_v) for script in $(CORE_SCRIPTS); do \
	    $(LN) -s "$(EXTRAS)/bin/$${script}" "$(DSTROOT)$(USRBINDIR)/$${script}"; \
	    $(INSTALL_FILE) "$(OBJROOT)/$(Project)/TwistedCore-"*"/doc/man/$${script}.1" "$(DSTROOT)$(MANDIR)/man1/"; \
	done;
ifdef RC_JASPER
	$(_v) for so in $$(find "$(DSTROOT)$(EXTRAS)" -type f -name '*.so'); do \
	    $(STRIP) -Sx "$${so}"; \
	done;
	$(_v) for zsh_turd in "$(DSTROOT)$(EXTRAS)/lib/python/twisted/python/zsh/"*; do \
	    if [ ! -s "$${zsh_turd}" ]; then rm -f "$${zsh_turd}"; fi; \
	done;
endif

#
# Automatic Extract & Patch
#

AEP	       = YES
AEP_ProjVers   = $(Project)-2.5.0
AEP_Filename   = $(AEP_ProjVers).tar.bz2
AEP_ExtractDir = $(AEP_ProjVers)
AEP_Patches    = 

extract_source::
ifeq ($(AEP),YES)
	@echo "Extracting source for $(Project)...";
	$(_v) $(MKDIR) -p "$(OBJROOT)";
	$(_v) $(TAR) -C "$(OBJROOT)" -xjf "$(SRCROOT)/$(AEP_Filename)";
	$(_v) $(RMDIR) "$(OBJROOT)/$(Project)";
	$(_v) $(MV) "$(OBJROOT)/$(AEP_ExtractDir)" "$(OBJROOT)/$(Project)";
	$(_v) for patchfile in $(AEP_Patches); do \
	   cd "$(OBJROOT)/$(Project)" && patch -lp0 < "$(SRCROOT)/patches/$${patchfile}"; \
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
	$(_v) $(INSTALL_FILE) "$(OBJROOT)/$(Project)/LICENSE" "$(DSTROOT)/$(OSL)/$(ProjectName).txt";
