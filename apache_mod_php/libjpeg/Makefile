##
# Copyright (c) 2008-2010 Apple Inc. All Rights Reserved.
##
#

# General project info
Project         = libjpeg
UserType        = Developer
ToolType        = Library

# Some Makefile features are available only on Snow Leopard and later.
_OS_VERSION = $(shell sw_vers -productVersion | cut -d. -f1-2)

# Project-specific additions.
#Extra_Environment	= 
Extra_Configure_Flags	= --enable-static --disable-shared
Extra_Make_Flags		=
# Required or AEP parts won't be picked up.
GnuAfterInstall		= install-macosx

# Override defaults because we do want this in /usr/local...
Install_Prefix	= $(USRDIR)/local
Install_Man	= $(Install_Prefix)/share/man
Install_Info	= $(Install_Prefix)/info

# Automatic Extract & Patch
AEP		= YES
AEP_Version	= 9
AEP_Filename	= jpegsrc.v$(AEP_Version).tar.gz
AEP_ExtractDir	= jpeg-$(AEP_Version)
AEP_Patches	=
# License file must be empty as this is a subproject.
AEP_LicenseFile	=


# Defines only used here.

# Local targets that must be defined before including the following
# files to get the dependency order correct
install::

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make


#GnuAfterInstall += archive-strip-binaries
GnuAfterInstall += install-startup-files install-open-source-files
GnuAfterInstall += install-top-level-man-pages install-configuration-files


#
# Define AEP variables
#
ifndef AEP_Project
    AEP_Project		= $(Project)
endif

ifndef AEP_ProjVers
  ifdef AEP_Version
    AEP_ProjVers	= $(AEP_Project)-$(AEP_Version)
  else
    AEP_ProjVers	= $(AEP_Project)
  endif
endif

ifndef AEP_Filename
    AEP_Filename	= $(wildcard $(AEP_ProjVers).tar.gz $(AEP_ProjVers).tar.bz2)
endif
ifeq ($(suffix $(AEP_Filename)),.bz2)
    AEP_ExtractOption	= j
else
    AEP_ExtractOption	= z
endif

ifndef AEP_ExtractDir
    AEP_ExtractDir	= $(AEP_ProjVers)
endif

ifndef AEP_LicenseFile
    AEP_LicenseFile	= $(SRCROOT)/$(ProjectName).txt
endif

ifndef AEP_ManPages
    AEP_ManPages := $(wildcard *.[1-9] man/*.[1-9])
endif

ifndef AEP_ConfigDir
    AEP_ConfigDir	= $(ETCDIR)
endif

ifndef AEP_Binaries
    AEP_Binaries	= $(subst ./,/,$(shell cd $(DSTROOT) && $(FIND) . -type f -perm +0111 -exec $(SHELL) -c 'test \"`file -b --mime-type {}`\" = \"application/octet-stream\"' \; -print))
endif

#AEP_ExtractRoot		= $(SRCROOT)
AEP_ExtractRoot		= $(OBJROOT)

# Redefine the Sources directory defined elsewhere
# ...but save the version of ConfigStamp (based on Sources)
# GNUSource.make uses.
GNUConfigStamp		:= $(ConfigStamp)
Sources			= $(AEP_ExtractRoot)/$(AEP_Project)

# Redefine Configure to allow extra "helper" environment variables.
# This logic was moved to GNUSource.make in 10A251, so only override the setting
# if building on an earlier system. (Make_Flags is only defined with that patch.)
ifndef Make_Flags
ifdef Extra_Configure_Environment
      Configure		:= $(Extra_Configure_Environment) $(Configure)
endif
endif


# Open Source configuration directories
OSVDIR	= $(USRDIR)/local/OpenSourceVersions
OSLDIR	= $(USRDIR)/local/OpenSourceLicenses

# Launchd / startup item paths
LAUNCHDDIR		= $(NSSYSTEMDIR)$(NSLIBRARYSUBDIR)/LaunchDaemons
SYSTEM_STARTUP_DIR	= $(NSSYSTEMDIR)$(NSLIBRARYSUBDIR)/StartupItems


#
# AEP targets
#
.PHONY: extract-source build-dependencies archive-strip-binaries
.PHONY: install-open-source-files install-startup-files
.PHONY: install-top-level-man-pages install-configuration-files

ifdef ConfigStamp
$(GNUConfigStamp): extract-source build-dependencies
else
build:: extract-source build-dependencies
endif

# Because GNUSource's ConfigStamp's rules are processed before this file is included,
# it's easier to copy the sources to the build directory and work from there.
extract-source::
ifeq ($(AEP),YES)
	@echo "Extracting source for $(Project)..."
	$(MKDIR) $(AEP_ExtractRoot)
	$(TAR) -C $(AEP_ExtractRoot) -$(AEP_ExtractOption)xf $(SRCROOT)/$(AEP_Filename)
	$(RMDIR) $(Sources)
	$(_v) $(RM) $(GNUConfigStamp)
	$(MV) $(AEP_ExtractRoot)/$(AEP_ExtractDir) $(Sources)
ifdef AEP_Patches
	for patchfile in $(AEP_Patches); do \
	   echo "Applying $$patchfile..."; \
	   cd $(Sources) && $(PATCH) -lp1 < $(SRCROOT)/patches/$$patchfile; \
	done
endif
ifeq ($(AEP_BuildInSources),YES)
ifneq ($(Sources),$(BuildDirectory))
	@echo "Copying sources to build directory..."
	$(_v) $(CP) $(Sources) $(BuildDirectory)
endif
endif
else
	@echo "Source extraction for $(Project) skipped!"
endif

# Common.make's recurse doesn't reset SRCROOT and misdefines Sources
build-dependencies:
ifdef Dependencies
	$(_v) for Dependency in $(Dependencies); do			\
		$(MKDIR) $(SYMROOT)/$${Dependency};			\
		$(MAKE) -C $${Dependency} $(TARGET)			\
			SRCROOT=$(SRCROOT)/$${Dependency}		\
			OBJROOT=$(OBJROOT)				\
			SYMROOT=$(SYMROOT)/$${Dependency}		\
			DSTROOT=$(DSTROOT)				\
			BuildDirectory=$(OBJROOT)/Build/$${Dependency}	\
			Sources=$(OBJROOT)/$${Dependency}		\
			CoreOSMakefiles=$(CoreOSMakefiles)		\
			$(Extra_Dependency_Flags);			\
		done
endif

archive-strip-binaries:: $(SYMROOT)
ifdef AEP_Binaries
	@echo "Archiving and stripping binaries..."
	$(_v) for file in $(addprefix $(DSTROOT),$(AEP_Binaries));	\
	do \
		_type=`file -b --mime-type $${file}`;			\
		if [ "$${_type}" = "application/octet-stream" ]; then	\
			echo "\tProcessing $${file}...";			\
			$(CP) $${file} $(SYMROOT);				\
			$(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file};\
			$(STRIP) -S $${file};					\
		else	\
			echo "\tSkipped non-binary $${file}; type is $${_type}";\
		fi	\
	done
endif

install-startup-files::
ifdef AEP_LaunchdConfigs
	@echo "Installing launchd configuration files..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(LAUNCHDDIR)
	$(INSTALL_FILE) $(AEP_LaunchdConfigs) $(DSTROOT)$(LAUNCHDDIR)
endif
ifdef AEP_StartupItem
	@echo "Installing StartupItem..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SYSTEM_STARTUP_DIR)/$(AEP_StartupItem)
	$(INSTALL_SCRIPT) $(StartupItem) $(DSTROOT)$(SYSTEM_STARTUP_DIR)/$(AEP_StartupItem)
	$(INSTALL_FILE) StartupParameters.plist $(DSTROOT)$(SYSTEM_STARTUP_DIR)/$(AEP_StartupItem)
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SYSTEM_STARTUP_DIR)/$(AEP_StartupItem)/Resources/English.lproj
	$(INSTALL_FILE) Localizable.strings $(DSTROOT)$(SYSTEM_STARTUP_DIR)/$(AEP_StartupItem)/Resources/English.lproj
endif

install-open-source-files::
	@echo "Installing Apple-internal open-source documentation..."
	if [ -e $(SRCROOT)/$(ProjectName).plist ]; then	\
		$(MKDIR) $(DSTROOT)/$(OSVDIR);	   	\
		$(INSTALL_FILE) $(SRCROOT)/$(ProjectName).plist $(DSTROOT)/$(OSVDIR)/$(ProjectName).plist;	\
	else	\
		echo "WARNING: No open-source file for this project!";	\
	fi
	if [ -e $(AEP_LicenseFile) ]; then	\
		$(MKDIR) $(DSTROOT)/$(OSLDIR);	\
		$(INSTALL_FILE) $(AEP_LicenseFile) $(DSTROOT)/$(OSLDIR)/$(ProjectName).txt;	\
	else	\
		echo "WARNING: No open-source file for this project!";	\
	fi


#
# Install any man pages at the top-level directory or its "man" subdirectory
#
install-top-level-man-pages::
ifdef AEP_ManPages
	@echo "Installing top-level man pages..."
	for _page in $(AEP_ManPages); do				\
		_section_dir=$(Install_Man)/man$${_page##*\.};		\
		$(INSTALL_DIRECTORY) $(DSTROOT)$${_section_dir};	\
		$(INSTALL_FILE) $${_page} $(DSTROOT)$${_section_dir};	\
	done
endif


#
# Install configuration files and their corresponding ".default" files
# to one standard location.
#
install-configuration-files::
ifdef AEP_ConfigFiles
	@echo "Installing configuration files..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(AEP_ConfigDir)
	for file in $(AEP_ConfigFiles); \
	do \
		$(INSTALL_FILE) $${file} $(DSTROOT)$(AEP_ConfigDir); \
		$(CHMOD) u+w $(DSTROOT)$(AEP_ConfigDir)/$${file}; \
		if [ "${file##*.}" != "default" ]; then \
			$(INSTALL_FILE) $${file} $(DSTROOT)$(AEP_ConfigDir)/$${file}.default; \
		fi; \
	done
endif


clean::
	$(_v) if [ -d $(Sources) ]; then \
	    cd $(Sources) && make clean; \
	fi

$(DSTROOT) $(DSTROOT)$(AEP_ConfigDir) $(SYMROOT) $(TMPDIR):
	$(MKDIR) $@


#
# Override settings from above include
#
DESTDIR	= $(DSTROOT)

# This needs to be overridden because the project properly uses DESTDIR.
Install_Flags	= DESTDIR="$(DSTROOT)"
# This project does not support the default "install-strip" target.
Install_Target	= install

# Done with overrides

install-macosx:
	@echo "No additional work necessary for Mac OS X."
