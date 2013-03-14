##
# Makefile for Apple Release Control (Archive Extraction & Patch)
#
# Copyright (c) 2005-2010 Apple Inc.
#
# @APPLE_LICENSE_HEADER_START@
# 
# Portions Copyright (c) 2005-2009 Apple Inc.  All Rights Reserved.
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
#
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
# @APPLE_LICENSE_HEADER_END@
##
# This header must be included after GNUsource.make or Common.make
#
# Set these variables as needed before including this file:
#  GnuAfterInstall - project-specific post-install targets. This MUST be defined
#                    for the AEP post-install helper targets to be invoked.
#  AEP_Version - open source project version, used in archive name and
#                extracted directory
#  AEP_Patches - list of file names (from patches subdirectory) to
#                run thru patch and apply to extracted sources
#  AEP_LicenseFile - full path to license file
#  AEP_BuildInSources - override default BuildDirectory and set to Sources;
#                       this would be necessary if the project should be built
#                       in the extracted source directory because the real
#                       sources (with configure) are located there.
#  Extra_Configure_Environment - additional environment variables only needed for
#                                the invocation of configure; this would be
#                                necessary if the project requires extra build flags
#                                but doesn't want to override everything defined by
#                                configure.
#
# The following variables will be defined if empty:
#  AEP_Project           [ $(Project)                                ]
#  AEP_ProjVers          [ $(AEP_Project)-$(AEP_Version)             ]
#  AEP_Filename          [ $(AEP_ProjVers).tar.[bg]z*                ]
#  AEP_ExtractDir        [ $(AEP_ProjVers)                           ]
#  AEP_LicenseFile       [ $(SRCROOT)/$(ProjectName).txt             ]
#  AEP_ConfigDir         [ $(ETCDIR)                                 ]
#
# Additionally, the following variables may also be defined before
# including this file:
#  AEP_Binaries - list of binaries to archive to SYMROOT and strip in DSTROOT
#  AEP_LaunchdConfigs - launchd config files in SRCROOT to be installed
#                       into LAUNCHDDIR
#  AEP_StartupItem - startup items name to be installed into
#                    SYSTEM_STARTUP_DIR; assumes existence of
#                    StartupParameters.plist and Localizable.strings
#  AEP_ManPages - man pages provided outside the extracted project
#  AEP_ConfigFiles - standard set of configuration files; ".default" versions
#                    will be created as well
#  Dependencies - list of subprojects which should be built before main project.
#
# Finally, a new target, archive-strip-binaries, is made available to copy
# AEP_Binaries to SYMROOT and strip the versions in DSTROOT. To leverage this
# target, it should be defined as the first item in GnuAfterInstall.
##

ifdef AEP_Binaries
GnuAfterInstall += archive-strip-binaries
endif
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
	   cd $(Sources) && $(PATCH) -p1 -F0 < $(SRCROOT)/patches/$$patchfile || exit 1; \
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
		$(MAKE) -C $${Dependency} $(TARGET)			\
			SRCROOT=$(SRCROOT)/$${Dependency}		\
			OBJROOT=$(OBJROOT)				\
			SYMROOT=$(SYMROOT)				\
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
		$(CP) $${file} $(SYMROOT);				\
		$(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file};\
		$(STRIP) -S $${file};					\
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
