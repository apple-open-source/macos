#
# Apple wrapper Makefile for PHP
# Copyright (c) 2008-2014 Apple Inc. All Rights Reserved.
##
#

# General project info for use with RC/GNUsource.make makefile
Project         = php
ProjectName     = apache_mod_php
UserType        = Developer
ToolType        = Commands
Submission      = 93

# Environment is passed to BOTH configure AND make, which can cause problems if these
# variables are intended to help configure, but not override the result.
Environment	= YACC=/usr/local/bin/bison-1.28 \
			php_cv_bison_version=1.28 \
			LEX=/usr/local/bin/lex-2.5.4 \
			MAKEOBJDIR="$(BuildDirectory)" \
			INSTALL_ROOT="$(DSTROOT)" \
			TMPDIR="$(TMPDIR)" TEMPDIR="$(TMPDIR)" 
# This allows extra variables to be passed _just_ to configure.
Extra_Configure_Environment	= CFLAGS="$$RC_CFLAGS -Os -g" \
					LDFLAGS="$$RC_CFLAGS -Os -g" \
					EXTRA_LIBS="-lresolv" \
					EXTRA_LDFLAGS_PROGRAM="-mdynamic-no-pic"

build_host_target_alias=`$SHELL "$(Sources)/config.guess"`
# The configure flags are ordered to match current output of ./configure --help.
# Extra indentation represents suboptions.
Extra_Configure_Flags	= --sysconfdir=$(ETCDIR) \
			--with-apxs2=$(USRSBINDIR)/apxs \
			--enable-cli \
			--with-config-file-path=/etc \
			--with-config-file-scan-dir=/Library/Server/Web/Config/php \
			--with-libxml-dir=$(USRDIR) \
			--with-openssl=$(USRDIR) \
			--with-kerberos=$(USRDIR) \
			--with-zlib=$(USRDIR) \
			--enable-bcmath \
			--with-bz2=$(USRDIR) \
			--enable-calendar \
			--disable-cgi \
			--with-curl=$(USRDIR) \
			--enable-dba \
			--with-ndbm=$(USRDIR) \
			--enable-exif \
			--enable-fpm \
			--enable-ftp \
			--with-png-dir=no \
			--with-gd \
			--with-jpeg-dir=$(DSTROOT)$(USRDIR)/local \
			--enable-gd-native-ttf \
			--with-icu-dir=$(USRDIR) \
			--with-ldap=$(USRDIR)\
			--with-ldap-sasl=$(USRDIR) \
			--with-libedit=$(USRDIR) \
			--enable-mbstring \
			--enable-mbregex \
			--with-mysql=mysqlnd \
			--with-mysqli=mysqlnd \
			--without-pear \
			--with-pear=no\
			--with-pdo-mysql=mysqlnd \
			--with-mysql-sock=/var/mysql/mysql.sock \
			--with-readline=$(USRDIR) \
			--enable-shmop \
			--with-snmp=$(USRDIR) \
			--enable-soap \
			--enable-sockets \
			--enable-sysvmsg --enable-sysvsem --enable-sysvshm \
			--with-tidy \
			--enable-wddx \
			--with-xmlrpc \
			--with-iconv-dir=$(USRDIR) \
			--with-xsl=$(USRDIR) \
			--enable-zend-multibyte \
			--enable-zip


# Additional project info used with AEP
AEP		= YES
AEP_Version	= 5.5.14
AEP_LicenseFile	= $(Sources)/LICENSE
AEP_Patches	=  \
			MacOSX_build.patch \
			iconv.patch pear.patch phar.patch
AEP_ConfigDir	= $(ETCDIR)
AEP_Binaries	= $(shell $(USRSBINDIR)/apxs -q LIBEXECDIR)/*.so $(USRBINDIR)/php $(USRSBINDIR)/php-fpm
AEP_ManPages	= pear.1 phar.1 phar.phar.1
Dependencies	= libjpeg 
GnuAfterInstall = archive-strip-binaries install-macosx install-xdebug install-open-source-files # needs a path adjustment


# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: post-extract-source $(GnuAfterInstall)

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
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

#include $(MAKEFILEPATH)/CoreOS/ReleaseControl/AEP.make

# Override settings from above includes
BuildDirectory	= $(OBJROOT)/Build/$(AEP_Project)
Install_Target	= install
TMPDIR		= $(OBJROOT)/Build/tmp
# This needs to be overridden because the project properly uses DESTDIR and
# INSTALL_ROOT (which is included in Environment).
Install_Flags	= DESTDIR="$(DSTROOT)"

#
# "ifeq / ifneq" are processed at read time; since these use variables defined
# by the included files above, this must be performed after the includes.
#
# The PCRE library is only installed on Snow Leopard and later.
ifneq ($(strip $(wildcard $(USRDIR)/local/include/pcre.*)),)
Extra_Configure_Flags	+= --with-pcre-regex=$(USRDIR)
endif

# Build rules
# Dependency info necessary to ensure temp directory and cleanup are performed.
lazy_install_source:: $(TMPDIR)
$(GNUConfigStamp): post-extract-source $(TMPDIR)

# Post-extract target
post-extract-source: extract-source
	@echo "Executing extra patch after extraction..."
	$(PERL) -i -pe 's|-i -a -n php5|-i -n php5|g' $(Sources)/configure
	$(PERL) -i -pe 's|rm -f conftest|rm -rf conftest|g' $(Sources)/configure

# Invoke pearcmd.php manually (instead of via /usr/bin/pear) so we can force
# lookups from DSTROOT instead of final install location.
PEAR		= $(DSTROOT)$(USRBINDIR)/php -C -q \
		-n -d include_path=$(DSTROOT)$(USRLIBDIR)/php $(PEAR_Cmd)
PEAR_Cmd	= $(TMPDIR)/pearcmd.php

install-macosx:
	@echo "Cleaning up install for Mac OS X..."
	-$(RMDIR) $(DSTROOT)$(ETCDIR)/apache2
	$(CHOWN) -R root:wheel $(DSTROOT)/
	$(INSTALL_FILE) $(Sources)/php.ini-production $(DSTROOT)$(AEP_ConfigDir)/php.ini.default
	$(PERL) -i -pe 's|^extension_dir =.*|extension_dir = $(USRLIBDIR)/php/extensions/no-debug-non-zts-20121212|' $(DSTROOT)$(AEP_ConfigDir)/php.ini.default
	$(INSTALL_DIRECTORY) $(DSTROOT)$(USRLIBDIR)/php/extensions/no-debug-non-zts-20121212
	@echo "Removing references to DSTROOT in php-config and include files..."
	$(CP) $(DSTROOT)$(USRBINDIR)/php-config $(SYMROOT)/php-config \
		&& $(SED) -e 's=-L$(DSTROOT)$(USRDIR)/local/lib==' $(SYMROOT)/php-config \
		| $(SED) -e 's@$(DSTROOT)@@g' > $(DSTROOT)$(USRBINDIR)/php-config
	$(CP) $(DSTROOT)$(USRINCLUDEDIR)/$(Project)/main/build-defs.h $(SYMROOT) \
		&& LANG=C $(SED) -e 's@$(DSTROOT)@@g' $(SYMROOT)/build-defs.h \
			> $(DSTROOT)$(USRINCLUDEDIR)/$(Project)/main/build-defs.h
	@echo "Archiving private static libraries..."
	-$(MV) $(DSTROOT)$(USRDIR)/local/lib/* $(SYMROOT)
	@echo "Deleting private dependencies..."
	-$(RMDIR) $(DSTROOT)$(USRDIR)/local/lib
	-$(RMDIR) $(DSTROOT)$(USRDIR)/local/include
	-$(RMDIR) $(DSTROOT)$(USRDIR)/local/bin
	-$(RMDIR) $(DSTROOT)$(USRDIR)/local/share
	@echo "Installing PEAR phar for installation at setup time."
	$(INSTALL_FILE) $(SRCROOT)/install-pear-nozlib.phar $(DSTROOT)$(USRLIBDIR)/php
	@echo "Fixing PEAR configuration file..."
	if [ -e $(DSTROOT)/$(USRLIBDIR)/php/pearcmd.php ]; then	\
		$(CP) $(DSTROOT)/$(USRLIBDIR)/php/pearcmd.php $(PEAR_Cmd);	\
		$(PATCH) -l $(PEAR_Cmd) $(SRCROOT)/patches/pearcmd.patch;	\
		$(PEAR) -C $(DSTROOT)$(ETCDIR)/pear.conf config-set \
			cache_dir /tmp/pear/cache system;	\
		$(PEAR) -C $(DSTROOT)$(ETCDIR)/pear.conf config-set \
			download_dir /tmp/pear/download system;	\
		$(PEAR) -C $(DSTROOT)$(ETCDIR)/pear.conf config-set \
			temp_dir /tmp/pear/temp system;		\
	fi
	@echo "Cleaning up PEAR junk files..."
	-$(RMDIR) $(DSTROOT)$(USRLIBDIR)/php/test
	-$(RM) -rf $(DSTROOT)/.channels \
		$(DSTROOT)/.depdb \
		$(DSTROOT)/.depdblock \
		$(DSTROOT)/.filemap \
		$(DSTROOT)/.lock \
		$(DSTROOT)/.registry \
		$(DSTROOT)$(USRLIBDIR)/php/.lock \
		$(DSTROOT)$(USRLIBDIR)/php/.depdblock
	-$(STRIP) -x $(DSTROOT)/usr/bin/php $(DSTROOT)/usr/sbin/php-fpm
	-$(RM) -rf $(DSTROOT)/usr/var
	-$(MV) $(DSTROOT)/usr/php $(DSTROOT)/usr/share
	@echo "Mac OS X-specific cleanup complete."

install-xdebug:
	@echo "Installing XDebug extension..."
	$(MAKE) -C xdebug $(TARGET)				\
			SRCROOT=$(SRCROOT)/xdebug		\
			OBJROOT=$(OBJROOT)			\
			SYMROOT=$(SYMROOT)			\
			DSTROOT=$(DSTROOT)			\
			BuildDirectory=$(OBJROOT)/Build/xdebug	\
			Sources=$(OBJROOT)/xdebug		\
			CoreOSMakefiles=$(CoreOSMakefiles)
	@echo "XDebug extension installed."

$(DSTROOT)$(LIBEXECDIR)/apache2 $(TMPDIR):
	$(MKDIR) $@

refresh-pear:
	curl -RO# http://pear.php.net/install-pear-nozlib.phar
	curl -RO# http://pear2.php.net/pyrus.phar
