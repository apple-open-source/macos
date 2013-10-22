#
# Apple wrapper Makefile for PostgreSQL
# Copyright (c) 2012-2013 Apple Inc. All Rights Reserved.
#
# This build of PostgreSQLQ 9.1.x is only intended for data migration purposes.

# General project info for use with RC/GNUsource.make makefile
Project         = postgresql
ProjectName     = PostgreSQL
UserType        = Administrator
ToolType        = Commands
Submission      = 64.2

# Include common server build variables
-include /AppleInternal/ServerTools/ServerBuildVariables.xcconfig

# Variables only used by project
ServicesDir = $(NSLIBRARYSUBDIR)/Server
ServiceDir = $(ServicesDir)/$(ProjectName)
DBDir = $(ServiceDir)/postgresql9.1/pgsql

# Environment is passed to BOTH configure AND make, which can cause problems if these
# variables are intended to help configure, but not override the result.
Extra_CC_Flags	= -Os -g -Wall -Wno-deprecated-declarations
# This allows extra variables to be passed _just_ to configure.
Extra_Configure_Environment	= CFLAGS="$$RC_CFLAGS $(Extra_CC_Flags)" \
					LDFLAGS="$$RC_CFLAGS $(Extra_CC_Flags)" \
					LDFLAGS_EX="-mdynamic-no-pic" \
					EXTRA_LDFLAGS_PROGRAM="-mdynamic-no-pic" \
					LIBS="-L$(SDKROOT)/usr/lib" \
					INCLUDES="-I$(SDKROOT)/usr/include/libxml2"

# The configure flags are ordered to match current output of ./configure --help.
# Extra indentation represents suboptions.
Extra_Configure_Flags	= --prefix=$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/postgresql9.1 \
			--sbindir=$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/postgresql9.1 \
			--bindir=$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/postgresql9.1 \
			--sysconfdir=$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR) \
			--localstatedir=$(DBDir) \
			--libdir=$(SERVER_INSTALL_PATH_PREFIX)$(USRLIBDIR)/postgresql9.1 \
			--datarootdir=$(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)/postgresql9.1 \
			--includedir=$(ServiceDir)/pgbuild_tmp \
			--enable-thread-safety \
			--enable-dtrace \
			--with-tcl \
			--with-perl \
			--with-python \
			--with-gssapi \
			--with-krb5 \
			--with-pam \
			--with-ldap \
			--with-bonjour \
			--with-openssl \
			--with-libxml \
			--with-libxslt \
			--with-system-tzdata=$(SHAREDIR)/zoneinfo \
			--with-pgport=5433

Extra_Make_Flags	=

# Additional project info used with AEP
AEP		= YES
AEP_Version	= 9.1.9
AEP_LicenseFile	= $(Sources)/COPYRIGHT
AEP_Patches     = arches.patch pg_config_manual_h.patch \
					radar7687126.patch radar7756388.patch radar8304089.patch \
					initdb.patch _int_bool.c.patch prep_buildtree.patch

AEP_Binaries	= $(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/postgresql9.1/* $(SERVER_INSTALL_PATH_PREFIX)$(USRLIBDIR)/postgresql9.1/* $(SERVER_INSTALL_PATH_PREFIX)$(USRLIBDIR)/postgresql9.1/pgxs/src/test/regress/pg_regress

Configure_Products = config.log src/include/pg_config.h
GnuAfterInstall	= install-contrib install-macosx cleanup-temp archive-strip-binaries
ContribTools	= hstore intarray pg_upgrade pg_upgrade_support

# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: $(GnuAfterInstall)

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
#include $(MAKEFILEPATH)/CoreOS/ReleaseControl/AEP.make
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

###### End AEP.Make

# Override settings from above includes
BuildDirectory	= $(OBJROOT)/Build
Install_Target	= install
# This needs to be overridden because the project properly uses DESTDIR and
# INSTALL_ROOT (which is included in Environment).
Install_Flags	= DESTDIR="$(DSTROOT)"


# Build rules
install-contrib:
	@echo "Installing specific tools from contrib:"
	for tool in $(ContribTools); do \
		echo "...Installing $${tool}...";	\
		$(MAKE) -C $(BuildDirectory)/contrib/$${tool} $(Install_Flags) $(Install_Target); \
	done

install-macosx:
	@echo "Creating service directories..."
	@echo "Done."

cleanup-temp:
	@echo "Removing unnecessary files from DSTROOT..."
	$(RM) -rvf $(DSTROOT)$(NSLIBRARYSUBDIR)
	@echo "Done."
