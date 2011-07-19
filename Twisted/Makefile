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

PYTHON_VERSIONS = $(shell \
  for python in /usr/bin/python2.*[0-9]; do \
    "$${python}" -c 'import sys; print "%d.%d" % tuple(sys.version_info[0:2])'; \
  done; \
)

PYTHON = /usr/bin/python
EXTRAS := $(shell "$(PYTHON)" -c 'import sys; print sys.prefix')/Extras
CORE_SCRIPTS = trial twistd

build:: extract_source
	$(_v) for version in $(PYTHON_VERSIONS); do \
	        echo "Building for Python $${version}..."; \
	        cd $(OBJROOT)/$(Project) && $(Environment) "$(PYTHON)$${version}" setup.py build; \
	      done;

install::
	$(_v) for version in $(PYTHON_VERSIONS); do \
	        extras="$$("$(PYTHON)$${version}" -c 'import sys; print sys.prefix')/Extras"; \
	        echo "Installing for Python $${version}..."; \
	        cd $(OBJROOT)/$(Project) && $(Environment) "$(PYTHON)$${version}" setup.py install --home="$${extras}" --root="$(DSTROOT)"; \
	        for so in $$(find "$(DSTROOT)$${extras}" -type f -name '*.so'); do $(STRIP) -Sx "$${so}"; done; \
	        echo "Removing turds for Python $${version}..."; \
	        for turd in "$(DSTROOT)$${extras}/lib/python/twisted/python/zsh/"* "$(DSTROOT)$${extras}/lib/python/twisted/topfiles/"*.misc; do \
	          if [ ! -s "$${turd}" ]; then rm -f "$${turd}"; fi; \
	        done; \
	        echo "Removing empty files for Python $${version}..."; \
	        find "$(DSTROOT)$${extras}/lib/python" -name '*.py' -empty -exec cp "$(SRCROOT)/empty.py" '{}' ";" -print; \
	        echo "Initializing dropin.cache for Python $${version}..."; \
	        PYTHONPATH="$(DSTROOT)$${extras}/lib/python" "$(DSTROOT)$${extras}/bin/twistd" --help 2>&1 >/dev/null; \
	      done;
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(USRBINDIR)";
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)/$(MANDIR)/man1";
	$(_v) for script in $(CORE_SCRIPTS); do \
	    $(LN) -s "$(EXTRAS)/bin/$${script}" "$(DSTROOT)$(USRBINDIR)/$${script}"; \
	    $(INSTALL_FILE) "$(OBJROOT)/$(Project)/doc/core/man/$${script}.1" "$(DSTROOT)$(MANDIR)/man1/"; \
	done;

#
# Automatic Extract & Patch
#

AEP	       = YES
AEP_ProjVers   = $(Project)-11.0.0
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
