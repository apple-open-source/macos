#
# xbs-compatible wrapper Makefile for dovecot 
#
# WARNING: B&I overrides the perms, owner, and group for everything
# under /Library; see <rdar://problem/8389433>

Project			= dovecot
DELIVERABLE		= dovecot
PROJECT_VERSION		= $(Project)-2.0.13apple1
PigeonholeProject		= $(Project)-pigeonhole
PIGEONHOLE_VERSION		= $(Project)-2.0-pigeonhole-0.2.3

# Configuration values we customize
#
DOVECOT_PUSH_NOTIFY	= $(Project).push-notify
TOOL_DIR		= $(Project).Tools
MIGRATION_TOOL		= cyrus_to_$(Project)
SKVIEW_TOOL		= skview

# Environment is passed to BOTH configure AND make, which can cause problems if these
# variables are intended to help configure, but not override the result.
Environment	= MAKEOBJDIR="$(BuildDirectory)" \
            INSTALL_ROOT="$(DSTROOT)" \
            TMPDIR="$(TMPDIR)" TEMPDIR="$(TMPDIR)"
# This allows extra variables to be passed _just_ to configure.
Extra_Configure_Environment =
# The following were experiments used to avoid having to copy the sources into place.
# They didn't work.
#	EXTRA_CFLAGS="-Wcomment $(addprefix -I,$(shell find $(Sources)/$(Project)/src/lib* -type d -print) $(Sources)/$(Project)/src/deliver $(Sources)/$(Project)/src/mail-common) -Wformat-y2k"
#	EXTRA_LDFLAGS_PROGRAM="-mdynamic-no-pic"

Make_Flags	= -j

ProjectConfig		= $(DSTROOT)$(USRLIBDIR)/$(DELIVERABLE)/$(Project)-config

Common_Configure_Flags	= \
	--prefix=$(USRDIR) \
	--sbindir=$(USRSBINDIR) \
	--libexecdir=$(LIBEXECDIR) \
	--sysconfdir=/etc \
	--datarootdir=$(SHAREDIR) \
	--localstatedir=/var/$(DELIVERABLE) \
	--disable-dependency-tracking \
	--disable-static
Project_Configure_Flags	=	\
	--with-rundir=/var/run/$(DELIVERABLE) \
	--with-moduledir=$(USRLIBDIR)/$(DELIVERABLE) \
	--with-ssl=openssl \
	--with-gssapi=yes
Pigeonhole_Configure_Flags	= \
	--with-dovecot=../$(Project)

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/Common.make

XCODEBUILD	= $(USRBINDIR)/xcodebuild
# Override settings from above includes
BuildDirectory	= $(OBJROOT)/Build
Install_Target	= install
TMPDIR		= $(OBJROOT)/Build/tmp
# This needs to be overridden because the project properly uses DESTDIR and
# INSTALL_ROOT (which is included in Environment).
Install_Flags   = DESTDIR="$(DSTROOT)"
# Typically defined in GNUSource.make; duplicated here to effect similar functionality.
Sources			= $(SRCROOT)
Configure		= ./configure
ConfigureProject	= $(Configure)
ConfigurePigeonhole		= $(Configure)
ProjectConfigStamp	= $(BuildDirectory)/$(Project)/configure-stamp
PigeonholeConfigStamp	= $(BuildDirectory)/$(PigeonholeProject)/configure-stamp

#STRIP_FLAGS=-s
#SO_STRIPFLAGS=-rx


.PHONY: build_dovecot build_pigeonhole build_daemon build_tools
.PHONY: archive-strip-binaries install-extras install-man install-startup-files
.PHONY: install-open-source-files

default : clean configure_dovecot build_dovecot

install :: build_dovecot build_pigeonhole \
		build_daemon build_tools archive-strip-binaries \
		install-extras install-man install-startup-files

install-no-clean :: build_dovecot build_pigeonhole


$(BuildDirectory)/$(Project)/$(Configure):
	$(_v) cd "$(BuildDirectory)/$(Project)" && autoconf

$(ProjectConfigStamp) :
	@echo "***** Configuring $(Project), version $(PROJECT_VERSION)"
	$(_v) $(MKDIR) $(BuildDirectory)/$(Project) $(TMPDIR)
	$(_v) $(CP) $(Sources)/$(Project) $(BuildDirectory)
	$(_v) cd $(BuildDirectory)/$(Project) && $(Environment) $(Extra_Configure_Environment) $(ConfigureProject) $(Common_Configure_Flags) $(Project_Configure_Flags)
	$(_v) touch $@
	@echo "***** Configuring $(Project) complete."

$(PigeonholeConfigStamp) : $(ProjectConfig)
	@echo "***** Configuring $(PigeonholeProject): Version: $(PIGEONHOLE_VERSION)"
	$(_v) $(MKDIR) $(BuildDirectory)/$(PigeonholeProject)
	$(_v) $(CP) $(Sources)/$(PigeonholeProject) $(BuildDirectory)
	$(_v) cd $(BuildDirectory)/$(PigeonholeProject) && $(Environment) $(Extra_Configure_Environment) $(ConfigurePigeonhole) $(Common_Configure_Flags) $(Pigeonhole_Configure_Flags)
	$(_v) touch $@
	@echo "***** Configuring $(PigeonholeProject) complete."

build_dovecot : $(ProjectConfigStamp) $(TMPDIR)
	@echo "***** Building $(Project)"
	$(_v) $(MAKE) -C $(BuildDirectory)/$(Project) $(Make_Flags) $(Install_Flags) $(Install_Target)
	$(_v) $(MV) "$(DSTROOT)$(USRSBINDIR)/$(DELIVERABLE)" "$(DSTROOT)$(USRSBINDIR)/$(DELIVERABLE)d"
	@echo "***** Building $(Project) complete."

$(DSTROOT)$(USRLIBDIR)/$(DELIVERABLE)/$(Project)-config:
	$(_v) $(MAKE) build_dovecot

build_pigeonhole : $(PigeonholeConfigStamp) $(ProjectConfig)
	@echo "***** Building $(PigeonholeProject)"
	$(_v) $(MAKE) -C $(BuildDirectory)/$(PigeonholeProject) $(Make_Flags) $(Install_Flags) $(Install_Target)
	$(_v) $(CP) $(BuildDirectory)/$(PigeonholeProject)/src/plugins/lda-sieve/.libs/lib90_sieve_plugin.so* \
		$(SYMROOT)
	@echo "***** Building $(PigeonholeProject) complete."

build_daemon : $(ProjectConfig)
	@echo "***** Building $(DOVECOT_PUSH_NOTIFY)"
	$(_v) cd "$(SRCROOT)/$(DOVECOT_PUSH_NOTIFY)/daemon" \
		&& $(XCODEBUILD) $(Install_Target)	\
			SRCROOT="$(SRCROOT)/$(DOVECOT_PUSH_NOTIFY)/daemon"	\
			OBJROOT=$(OBJROOT)		\
			SYMROOT=$(SYMROOT)		\
			DSTROOT="$(DSTROOT)"		\
			RC_CFLAGS="$(RC_CFLAGS)"	\
			RC_ARCHS="$(RC_ARCHS)"
	@echo "***** Building $(DOVECOT_PUSH_NOTIFY) complete."

build_tools :
	@echo "***** Building $(MIGRATION_TOOL)"
	$(_v) cd "$(SRCROOT)/$(TOOL_DIR)/$(MIGRATION_TOOL)" \
		&& $(XCODEBUILD) $(Install_Target)	\
			SRCROOT="$(SRCROOT)/$(TOOL_DIR)/$(MIGRATION_TOOL)"	\
			OBJROOT=$(OBJROOT)		\
			SYMROOT=$(SYMROOT)		\
			DSTROOT="$(DSTROOT)"		\
			RC_CFLAGS="$(RC_CFLAGS)"	\
			RC_ARCHS="$(RC_ARCHS)"
	@echo "***** Building $(MIGRATION_TOOL) complete."
	@echo "***** Building $(SKVIEW_TOOL)"
	$(_v) cd "$(SRCROOT)/$(TOOL_DIR)/$(SKVIEW_TOOL)" \
		&& $(XCODEBUILD) $(Install_Target)	\
			SRCROOT="$(SRCROOT)/$(TOOL_DIR)/$(SKVIEW_TOOL)"	\
			OBJROOT=$(OBJROOT)		\
			SYMROOT=$(SYMROOT)		\
			DSTROOT="$(DSTROOT)"		\
			RC_CFLAGS="$(RC_CFLAGS)"	\
			RC_ARCHS="$(RC_ARCHS)"
	@echo "***** Building $(SKVIEW_TOOL) complete."

# Custom configuration:
#
#

lib_cleanup :
	@echo "***** Cleaning up files not intended for installation"
	$(_v) $(RMDIR) $(SYMROOT)/usr/include
	$(_v) $(RMDIR) $(DSTROOT)/usr/include
	$(_v) $(RMDIR) "$(SYMROOT)$(RUNDIR)"
	$(_v) $(RMDIR) "$(DSTROOT)$(RUNDIR)"
	$(_v) $(RMDIR) "$(SYMROOT)$(ETCDIR)" "$(SYMROOT)$(SHAREDIR)"
	@echo "***** Cleaning up complete."

archive-strip-binaries: $(SYMROOT)
	@echo "***** Archiving, dSYMing and stripping binaries..."
	$(_v) $(FIND) $(OBJROOT) -name '*.dSYM' -exec $(CP) {} $(SYMROOT) \;
	$(_v) for file in $(DSTROOT)$(USRBINDIR)/* $(DSTROOT)$(USRSBINDIR)/* $(DSTROOT)$(LIBEXECDIR)/$(DELIVERABLE)/*;\
	do \
		if test -L $${file}; then \
			echo "Skipping symlink $${file}"; \
			continue; \
		fi; \
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
	$(_v) for file in $$( $(FIND) $(DSTROOT)$(USRLIBDIR)/$(DELIVERABLE) -type f \( -name '*.so' -o -name '*.dylib' \) );\
	do \
		if test -L $${file}; then \
			echo "Skipping symlink $${file}"; \
			continue; \
		fi; \
		echo "Processing $${file##*/} (from $${file})";	\
		if [ ! -e "$(SYMROOT)/$${file##*/}" ]; then	\
			echo "  $(CP) $${file} $(SYMROOT)";	\
			$(CP) $${file} $(SYMROOT);	\
		fi;	\
		if [ -e "$(SYMROOT)/$${file##*/}.dSYM" ]; then	\
			echo "...dSYM already copied.";	\
		else	\
			echo "  $(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file}";\
			$(DSYMUTIL) --out=$(SYMROOT)/$${file##*/}.dSYM $${file};\
		fi;	\
		$(STRIP) -rx $${file};	\
	done
	@echo "***** Archiving, dSYMing and stripping binaries complete."

install-strip :
	@echo "***** Stripping language libraries..."
	@echo "***** Stripping language libraries complete."

install-open-source-files:
	@echo "***** Installing open source configuration files..."
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRDIR)/local/OpenSourceVersions
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(USRDIR)/local/OpenSourceLicenses
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/dovecot.OpenSourceInfo/$(DELIVERABLE).plist" \
				  "$(DSTROOT)$(USRDIR)/local/OpenSourceVersions"
	$(_v) $(INSTALL_FILE) "$(SRCROOT)/dovecot.OpenSourceInfo/$(DELIVERABLE).txt" \
				  "$(DSTROOT)$(USRDIR)/local/OpenSourceLicenses"
	@echo "***** Installing open source configuration files complete."

install-extras : install-open-source-files lib_cleanup
	@echo "***** Installing extras..."
	$(_v) if [ ! -d "$(DSTROOT)$(ETCDIR)" ]; then	\
		echo "$(MKDIR) $(DSTROOT)$(ETCDIR)";		\
		$(MKDIR) "$(DSTROOT)$(ETCDIR)";		\
	fi
	$(_v) if [ -e "$(DSTROOT)/etc" -a "$(ETCDIR)" != "/etc" ]; then	\
		echo "$(MV) $(DSTROOT)/etc/* $(DSTROOT)$(ETCDIR)";	\
		$(MV) "$(DSTROOT)/etc/*" "$(DSTROOT)$(ETCDIR)";		\
		echo "$(RMDIR) $(DSTROOT)/etc";				\
		$(RMDIR) "$(DSTROOT)/etc";				\
	fi
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(ETCDIR)/$(DELIVERABLE)" \
			"$(DSTROOT)$(ETCDIR)/$(DELIVERABLE)/conf.d" \
			"$(DSTROOT)$(ETCDIR)/$(DELIVERABLE)/example-config" \
			"$(DSTROOT)$(ETCDIR)/$(DELIVERABLE)/example-config/conf.d" \
			"$(DSTROOT)$(ETCDIR)/$(DELIVERABLE)/default" \
			"$(DSTROOT)$(ETCDIR)/$(DELIVERABLE)/default/conf.d"
	$(_v) $(INSTALL_FILE) $(BuildDirectory)/$(Project)/doc/example-config/*.conf* \
			"$(DSTROOT)$(ETCDIR)/$(DELIVERABLE)/example-config"
	$(_v) $(INSTALL_FILE) $(BuildDirectory)/$(Project)/doc/example-config/conf.d/*.conf* \
			"$(DSTROOT)$(ETCDIR)/$(DELIVERABLE)/example-config/conf.d"
	$(_v) $(INSTALL_FILE) $(SRCROOT)/dovecot.Config/*.conf* \
			"$(DSTROOT)$(ETCDIR)/$(DELIVERABLE)/default"
	$(_v) $(INSTALL_FILE) $(SRCROOT)/dovecot.Config/conf.d/*.conf* \
			"$(DSTROOT)$(ETCDIR)/$(DELIVERABLE)/default/conf.d"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/CleanInstallExtras"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/MigrationExtras"
	$(_v) $(INSTALL_DIRECTORY) "$(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/PromotionExtras"
	$(_v) $(INSTALL_SCRIPT) "$(SRCROOT)/dovecot.Config/65_mail_migrator.pl" \
			"$(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/MigrationExtras/65_mail_migrator.pl"
	$(_v) $(INSTALL_SCRIPT) "$(SRCROOT)/dovecot.Config/SetupDovecot.sh" \
			"$(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/CleanInstallExtras/SetupDovecot.sh"
	$(_v) cd "$(DSTROOT)$(NSLIBRARYDIR)/ServerSetup/PromotionExtras" && \
			ln -s ../CleanInstallExtras/SetupDovecot.sh
	$(_v) $(INSTALL_SCRIPT) "$(SRCROOT)/dovecot.Config/migrate_partition_mail_data" \
			"$(DSTROOT)$(LIBEXECDIR)/$(DELIVERABLE)/migrate_partition_mail_data.sh"
	$(_v) $(INSTALL_SCRIPT) "$(SRCROOT)/dovecot.Config/migrate_mail_data.pl" \
			"$(DSTROOT)$(LIBEXECDIR)/$(DELIVERABLE)/migrate_mail_data.pl"
	$(_v) $(INSTALL_SCRIPT) "$(SRCROOT)/dovecot.Config/mail_data_migrator.pl" \
			"$(DSTROOT)$(LIBEXECDIR)/$(DELIVERABLE)/mail_data_migrator.pl"
	$(_v) $(INSTALL_SCRIPT) "$(SRCROOT)/dovecot.Config/migrate_single_user_mail_data" \
			"$(DSTROOT)$(LIBEXECDIR)/$(DELIVERABLE)/migrate_single_user_mail_data.sh"
	$(_v) $(INSTALL_SCRIPT) "$(SRCROOT)/dovecot.Config/quota-warning.sh" \
			"$(DSTROOT)$(LIBEXECDIR)/$(DELIVERABLE)/quota-warning.sh"
	$(_v) $(INSTALL_SCRIPT) "$(SRCROOT)/dovecot.Config/quota-exceeded.sh" \
			"$(DSTROOT)$(LIBEXECDIR)/$(DELIVERABLE)/quota-exceeded.sh"
	$(_v) $(CHMOD) u+w $(DSTROOT)$(LIBEXECDIR)/$(DELIVERABLE)/quota-*.sh
	$(_v) $(CHOWN) root:mail "$(DSTROOT)$(LIBEXECDIR)/$(DELIVERABLE)/deliver"
	$(_v) $(CHMOD) 04750 "$(DSTROOT)$(LIBEXECDIR)/$(DELIVERABLE)/deliver"
	$(_v) $(INSTALL_SCRIPT) "$(SRCROOT)/$(TOOL_DIR)/update-fts-index.pl" \
			"$(DSTROOT)$(LIBEXECDIR)/$(DELIVERABLE)/update-fts-index.pl"
	@echo "WARNING: B&I overrides the perms, owner, and group for everything under /Library; see <rdar://problem/8389433>"
	$(_v) $(INSTALL) -d -m 775 -o _dovecot -g mail $(DSTROOT)/Library/Server/Mail/Data/mail
	$(_v) $(INSTALL) -d -m 775 -o _dovecot -g mail $(DSTROOT)/Library/Server/Mail/Data/rules
	$(_v) (cd "$(DSTROOT)$(USRBINDIR)" && $(LN) -s cvt_mail_data set_user_mail_opts)
	@echo "***** Installing extras complete."

install-man :
	@echo "***** Installing man pages..."
	$(_v) perl -p -i -e '$$pass = 1 if /FILES/; s/dovecot/dovecotd/g unless $$pass;' "$(DSTROOT)$(SHAREDIR)/man/man1/dovecot.1"
	$(_v) (cd "$(DSTROOT)$(SHAREDIR)/man" && $(LN) -s ../man1/dovecot.1 man8/dovecotd.8)
	@echo "***** Installing man pages complete."

install-startup-files :
	@echo "***** Installing Startup Item..."
	$(_v) $(INSTALL_DIRECTORY) $(DSTROOT)$(NSLIBRARYDIR)/LaunchDaemons
	$(_v) $(INSTALL_FILE) \
		$(SRCROOT)/dovecot.LaunchDaemons/org.dovecot.$(DELIVERABLE)d.plist \
		$(SRCROOT)/dovecot.LaunchDaemons/com.apple.mail_migration.plist \
		$(SRCROOT)/dovecot.LaunchDaemons/org.dovecot.fts.update.plist \
		$(DSTROOT)$(NSLIBRARYDIR)/LaunchDaemons
	@echo "***** Installing Startup Item complete."

$(DSTROOT) $(TMPDIR) :
	$(_v) if [ ! -d $@ ]; then	\
		$(MKDIR) $@;	\
	fi
