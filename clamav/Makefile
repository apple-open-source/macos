# xbs-compatible wrapper Makefile for Clam AV
#

Project			= clamav
OPEN_SOURCE_VER	= 0.97.1
PROJECT_VERSION	= $(Project)-$(OPEN_SOURCE_VER)

# Configuration values we customize
#
CLAMAV_OWNER		= _clamav
CLAMAV_GROUP		= _clamav
CLAMAV_TAR_GZ		= $(PROJECT_VERSION).tar.gz

PROJECT_BIN_DIR		= $(Project).Bin
PROJECT_CONF_DIR	= $(Project).Conf
PROJECT_SETUP_DIR	= $(Project).SetupExtras
PROJECT_LD_DIR		= $(Project).LaunchDaemons
PROJECT_OS_DIR		= $(Project).OpenSourceInfo
PROJECT_PATCH		= $(Project)-patch-7054720.diff

CONFIG_ENV	= MAKEOBJDIR="$(BuildDirectory)" \
            INSTALL_ROOT="$(DSTROOT)" \
            TMPDIR="$(TMPDIR)" TEMPDIR="$(TMPDIR)"

CFLAGS		= -O0 $(RC_CFLAGS)

# Environment is passed to BOTH configure AND make, which can cause problems if these
# variables are intended to help configure, but not override the result.
Environment	= MAKEOBJDIR="$(BuildDirectory)" \
            INSTALL_ROOT="$(DSTROOT)" \
            TMPDIR="$(TMPDIR)" TEMPDIR="$(TMPDIR)"
# This allows extra variables to be passed _just_ to configure.
Extra_Configure_Environment =

Make_Flags	=

ProjectConfig		= $(DSTROOT)$(USRINCLUDEDIR)/$(Project)/$(Project)-config

Common_Configure_Flags	= \
	--prefix=/ \
	--mandir=$(MANDIR) \
	--bindir=$(USRBINDIR) \
	--libdir=$(USRLIBDIR) \
	--sbindir=$(USRSBINDIR) \
	--datadir=$(SHAREDIR)/$(Project) \
	--exec-prefix=$(USRDIR) \
	--libexecdir=$(LIBEXECDIR) \
	--sysconfdir=/etc \
	--datarootdir=$(SHAREDIR) \
	--sharedstatedir=/var/$(Project)/share \
	--localstatedir=/var/$(Project)/state \
	--disable-dependency-tracking
Project_Configure_Flags	=	\
	--includedir=$(SHAREDIR)/$(Project)/include \
	--oldincludedir=$(SHAREDIR)/$(Project)/include \
	--with-dbdir=/var/$(Project) \
	--with-user=$(CLAMAV_OWNER) \
	--with-group=$(CLAMAV_GROUP) \
	--with-gnu-ld
Stataic_Configure_Flags	=	\
	--disable-shared \
	--enable-static

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

# Override settings from above includes
BuildDirectory	= $(OBJROOT)/Build
Install_Target	= install
TMPDIR			= $(OBJROOT)/Build/tmp

# This needs to be overridden because the project properly uses DESTDIR and
# INSTALL_ROOT (which is included in Environment).
Install_Flags	= DESTDIR="$(DSTROOT)"

# Typically defined in GNUSource.make; duplicated here to effect similar functionality.
Sources				= $(SRCROOT)
Configure			= ./configure
ConfigureProject	= $(Configure)
ProjectConfigStamp	= $(BuildDirectory)/$(Project)/configure-stamp

LIB_TOOL			= $(BuildDirectory)/$(Project)/libtool

.PHONY: build-clamav
.PHONY: archive-strip-binaries install-extras install-man install-startup-files
.PHONY: install-open-source-files

default : clean build-clamav

install :: build-clamav archive-strip-binaries \
		install-extras install-startup-files \
		lib_cleanup

install-no-clean :: build-clamav

build-clamav :: extract-sources make-clamav

###################

extract-sources : $(TMPDIR)
	@echo "***** Extracting sources from: $(CLAMAV_TAR_GZ)"
	$(_v) cd $(BuildDirectory) && gnutar -xzpf $(Sources)/$(PROJECT_BIN_DIR)/$(CLAMAV_TAR_GZ)
	$(_v) $(MV) $(BuildDirectory)/$(PROJECT_VERSION) $(BuildDirectory)/$(Project)
	@echo "***** Extracting sources complete."

make-clamav : $(TMPDIR)
	@echo "***** Building $(Project)"
	@echo "***** Applying project patches: $(PROJECT_PATCH)"
	$(_v) if [ -e "$(SRCROOT)/$(PROJECT_BIN_DIR)/$(PROJECT_PATCH)" ]; then\
		(cd "$(BuildDirectory)/$(Project)" && patch -p1 < "$(SRCROOT)/$(PROJECT_BIN_DIR)/$(PROJECT_PATCH)") ; \
	fi
	@echo "***** Applying project patches complete."
	@echo "***** Configuring $(Project) shared, version $(PROJECT_VERSION)"
	$(_v) cd $(BuildDirectory)/$(Project) && $(CONFIG_ENV) $(ConfigureProject) $(Common_Configure_Flags) $(Project_Configure_Flags)
	$(_v) touch $@
	@echo "***** Configuring $(Project) shared complete."
	@echo "***** Patching $(LIB_TOOL)"
	$(_v) cd $(BuildDirectory)/$(Project)
	if grep -qs 'LTCFLAGS=\"-g -O2\"' $(LIB_TOOL) ; then \
		mv $(LIB_TOOL) $(LIB_TOOL).bak ; \
		sed -e 's/LTCFLAGS=\"-g -O2\"/LTCFLAGS=\"$(CFLAGS)\"/g' $(LIB_TOOL).bak > $(LIB_TOOL) ; \
	fi
	@echo "***** Patching $(LIB_TOOL) complete."
	@echo "***** Making $(Project)"
	$(_v) $(MAKE) -C $(BuildDirectory)/$(Project) CFLAGS="$(CFLAGS)" CXXFLAGS="$(CFLAGS)" $(Make_Flags) $(Install_Flags) $(Install_Target)
	@echo "***** Making $(Project) complete."
	@echo "***** Cleaning sources for: $(Project)"
	$(_v) cd $(BuildDirectory)/$(Project) && make distclean
	@echo "***** Cleaning sources complete."
	@echo "***** Configuring $(Project) static, version $(PROJECT_VERSION)"
	$(_v) cd $(BuildDirectory)/$(Project) && $(CONFIG_ENV) $(ConfigureProject) $(Common_Configure_Flags) $(Project_Configure_Flags) $(Stataic_Configure_Flags)
	$(_v) touch $@
	@echo "***** Configuring $(Project) static complete."
	@echo "***** Patching $(LIB_TOOL)"
	$(_v) cd $(BuildDirectory)/$(Project)
	if grep -qs 'LTCFLAGS=\"-g -O2\"' $(LIB_TOOL) ; then \
		mv $(LIB_TOOL) $(LIB_TOOL).bak ; \
		sed -e 's/LTCFLAGS=\"-g -O2\"/LTCFLAGS=\"$(CFLAGS)\"/g' $(LIB_TOOL).bak > $(LIB_TOOL) ; \
	fi
	@echo "***** Patching $(LIB_TOOL) complete."
	@echo "***** Making $(Project)"
	$(_v) $(MAKE) -C $(BuildDirectory)/$(Project) CFLAGS="$(CFLAGS)" CXXFLAGS="$(CFLAGS)" $(Make_Flags) $(Install_Flags) $(Install_Target)
	@echo "***** Making $(Project) complete."
	@echo "***** Building $(Project) complete."

$(ProjectConfig): $(DSTROOT)$(USRLIBDIR)/$(Project)/$(Project)-config
	$(_v) $(CP) "$(DSTROOT)$(USRLIBDIR)/$(Project)/$(Project)-config" $@

$(DSTROOT)$(USRLIBDIR)/$(Project)/$(Project)-config:
	$(_v) $(MAKE) build-clamav

# Custom configuration

lib_cleanup :
	@echo "***** Cleaning up files not intended for installation"
	$(_v) $(RMDIR) $(DSTROOT)$(USRLIBDIR)/libclamav.6.dylib
	$(_v) $(RMDIR) $(DSTROOT)$(USRLIBDIR)/libclamav.a
	$(_v) $(RMDIR) $(DSTROOT)$(USRLIBDIR)/libclamav.dylib
	$(_v) $(RMDIR) $(DSTROOT)$(USRLIBDIR)/libclamav.la
	$(_v) $(RMDIR) $(DSTROOT)$(USRLIBDIR)/libclamunrar.a
	$(_v) $(RMDIR) $(DSTROOT)$(USRLIBDIR)/libclamunrar_iface.a
	$(_v) $(RMDIR) $(DSTROOT)$(USRLIBDIR)/libclamunrar.la
	$(_v) $(RMDIR) $(DSTROOT)$(USRLIBDIR)/libclamunrar_iface.la
	$(_v) $(RMDIR) $(SYMROOT)/$(SHAREDIR)/$(Project)/include
	$(_v) $(RMDIR) $(DSTROOT)/$(SHAREDIR)/$(Project)/include
	$(_v) $(RMDIR) $(SYMROOT)$(USRBINDIR)/clamav-config
	$(_v) $(RMDIR) $(DSTROOT)$(USRBINDIR)/clamav-config
	$(_v) $(RMDIR) $(SYMROOT)$(USRLIBDIR)/pkgconfig
	$(_v) $(RMDIR) $(DSTROOT)$(USRLIBDIR)/pkgconfig
	$(_v) $(RMDIR) $(SYMROOT)$(ETCDIR)
	$(_v) $(RMDIR) $(SYMROOT)$(SHAREDIR)
	@echo "***** Cleaning up complete."

archive-strip-binaries: $(SYMROOT)
	@echo "***** Archiving, dSYMing and stripping binaries..."
	$(_v) for file in $(DSTROOT)$(USRBINDIR)/* $(DSTROOT)$(USRSBINDIR)/*;\
	do \
		echo "Processing $${file##*/} (from $${file})";	\
		if [ ! -e "$(SYMROOT)/$${file##*/}" ]; then	\
			echo "  $(CP) $${file} $(SYMROOT)";	\
			$(CP) $${file} $(SYMROOT);	\
		fi;	\
		if [ -e "$(SYMROOT)/$${file##*/}.dSYM" ]; then	\
			echo "...odd, dSYM already exists.";	\
		else	\
			echo "  $(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file}";\
			$(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file};\
		fi;	\
		$(STRIP) -S $${file};	\
	done
	$(_v) for file in $$( $(FIND) $(DSTROOT)$(USRLIBDIR) -type f \( -name '*.so' -o -name '*.dylib' \) );\
	do \
		$(STRIP) -rx $${file};\
	done
	@echo "***** Archiving, dSYMing and stripping binaries complete."

install-open-source-files:
	@echo "***** Installing open source configuration files..."
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRDIR)/local/OpenSourceVersions
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRDIR)/local/OpenSourceLicenses
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_OS_DIR)/$(Project).plist" \
				  "$(DSTROOT)$(USRDIR)/local/OpenSourceVersions"
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_OS_DIR)/$(Project).txt" \
				  "$(DSTROOT)$(USRDIR)/local/OpenSourceLicenses"
	@echo "***** Installing open source configuration files complete."

install-extras : install-open-source-files
	@echo "***** Installing extras..."
	# Create /private/etc
	$(_v) if [ ! -d "$(DSTROOT)$(ETCDIR)" ]; then	\
		echo "$(MKDIR) $(DSTROOT)$(ETCDIR)";		\
		$(MKDIR) "$(DSTROOT)$(ETCDIR)";	\
	fi
	$(_v) if [ -e "$(DSTROOT)/etc" -a "$(ETCDIR)" != "/etc" ]; then	\
		echo "$(MV) $(DSTROOT)/etc/* $(DSTROOT)$(ETCDIR)";	\
		$(MV) "$(DSTROOT)/etc/"* "$(DSTROOT)$(ETCDIR)/";	\
		echo "$(RMDIR) $(DSTROOT)/etc";	\
		$(RMDIR) "$(DSTROOT)/etc";	\
	fi

	# Create /private/var
	$(_v) if [ ! -d "$(DSTROOT)$(VARDIR)" ]; then	\
		echo "$(MKDIR) $(DSTROOT)$(VARDIR)";		\
		$(MKDIR) "$(DSTROOT)$(VARDIR)";		\
	fi
	$(_v) if [ -e "$(DSTROOT)/var" -a "$(VARDIR)" != "/var" ]; then	\
		echo "$(MV) $(DSTROOT)/var/* $(DSTROOT)$(VARDIR)";	\
		$(MV) "$(DSTROOT)/var/"* "$(DSTROOT)$(VARDIR)/";		\
		echo "$(RMDIR) $(DSTROOT)/var";				\
		$(RMDIR) "$(DSTROOT)/var";				\
	fi

	# install directories
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(SHAREDIR)/sandbox"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(VARDIR)/$(Project)/share"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(VARDIR)/$(Project)/share"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/CommonExtras"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/MigrationExtras"

	# Service configuration files
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_CONF_DIR)/clamd.conf.default" \
			"$(DSTROOT)$(ETCDIR)/clamd.conf.default"
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_CONF_DIR)/freshclam.conf.default" \
			"$(DSTROOT)$(ETCDIR)/freshclam.conf.default"

	# Cleanup default installed config files
	$(_v) $(RM) "$(DSTROOT)$(ETCDIR)/clamd.conf"
	$(_v) $(RM) "$(DSTROOT)$(ETCDIR)/freshclam.conf"

	# Service setup script
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_SETUP_DIR)/SetupClamAV.sh" \
			"$(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/CommonExtras/SetupClamAV.sh"
	$(_v) $(CHMOD) 0755 "$(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/CommonExtras/SetupClamAV.sh"

	# Sandbox setup
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_SETUP_DIR)/clamd.sb" \
			"$(DSTROOT)$(SHAREDIR)/sandbox/clamd.sb"

	# Install missing man page
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/$(PROJECT_CONF_DIR)/clambc.1" \
			"$(DSTROOT)$(MANDIR)/man1/clambc.1"

	# Set ownership & permissions
	$(_v) $(CHOWN) -R _clamav:_clamav "$(DSTROOT)$(VARDIR)/$(Project)"
	$(_v) $(CHMOD) 0755 "$(DSTROOT)$(VARDIR)/$(Project)"

	# Don't install databases
	$(_v) $(RM) "$(DSTROOT)$(VARDIR)/$(Project)/daily.cvd"
	$(_v) $(RM) "$(DSTROOT)$(VARDIR)/$(Project)/main.cvd"
	@echo "***** Installing extras complete."

install-startup-files :
	@echo "***** Installing Startup Item..."
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(NSLIBRARYDIR)/LaunchDaemons
	$(_v) $(INSTALL_FILE) \
		$(SRCROOT)/$(PROJECT_LD_DIR)/org.clamav.clamd.plist \
		$(SRCROOT)/$(PROJECT_LD_DIR)/org.clamav.freshclam.plist \
		$(SRCROOT)/$(PROJECT_LD_DIR)/org.clamav.freshclam-init.plist \
		$(DSTROOT)$(NSLIBRARYDIR)/LaunchDaemons
	@echo "***** Installing Startup Item complete."

$(DSTROOT) $(TMPDIR) :
	$(_v) if [ ! -d $@ ]; then	\
		$(MKDIR) $@;	\
	fi
