#
# Apple wrapper Makefile for PostgreSQL
# Copyright (c) 2009-2013,2016-2018 Apple Inc. All Rights Reserved.
#

# General project info for use with RC/GNUsource.make makefile
Project         = postgresql
UserType        = Administrator
ToolType        = Commands
Submission      = 207.6

# Include common server build variables
-include /AppleInternal/ServerTools/ServerBuildVariables.xcconfig

# Variables only used by project

ServicesDir = $(NSLIBRARYSUBDIR)/Server
ServiceDirCustomer = $(ServicesDir)/PostgreSQL
DocDir = $(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)/postgresql
DBDirCustomer = $(ServiceDirCustomer)/Data

AppleInternalDir = $(DSTROOT)/AppleInternal/ServerTools/PostgreSQL
ifndef DITTO
	DITTO=/usr/bin/ditto
endif
REALSDKROOT = $(shell echo $(SDKROOT) | sed  s/\\/BuildRoot//)

# Environment is passed to BOTH configure AND make, which can cause problems if these
# variables are intended to help configure, but not override the result.
Extra_CC_Flags	= -Os -g -Wall -Wno-deprecated-declarations
# This allows extra variables to be passed _just_ to configure.
Extra_Configure_Environment	= CFLAGS="$$RC_CFLAGS $(Extra_CC_Flags)" \
					LDFLAGS="$$RC_CFLAGS $(Extra_CC_Flags)" \
					LDFLAGS_EX="-mdynamic-no-pic" \
					EXTRA_LDFLAGS_PROGRAM="-mdynamic-no-pic" \
					LIBS="-L$(SDKROOT)/usr/lib" \
					INCLUDES="-I$(SDKROOT)/usr/include/libxml2 -I$(SDKROOT)/usr/include"

# The configure flags are ordered to match current output of ./configure --help.
# Extra indentation represents suboptions.
Extra_Configure_Flags	= --prefix=$(SERVER_INSTALL_PATH_PREFIX)$(USRDIR) --sbindir=$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR) \
	--sysconfdir=$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR) --mandir=$(SERVER_INSTALL_PATH_PREFIX)$(MANDIR) \
	--localstatedir=$(DBDirCustomer) \
	--htmldir=$(DocDir) \
	--enable-thread-safety \
	--includedir=$(REALSDKROOT)/usr/local/include \
	--enable-dtrace \
	--with-python \
	--with-gssapi \
	--with-krb5 \
	--with-pam \
	--with-bonjour \
	--with-openssl=no \
	--with-libxml \
	--with-libxslt \
	--with-system-tzdata=$(SHAREDIR)/zoneinfo \
	--with-tcl=yes \
	--with-tclconfig=$(SDKROOT)/System/Library/Frameworks/Tcl.framework/Versions/Current

Extra_Make_Flags	=

# Additional project info used with AEP
AEP		= YES
AEP_Version	= 9.3.7
AEP_LicenseFile	= $(Sources)/COPYRIGHT
AEP_Patches	= arches.patch pg_config_manual_h.patch \
			radar7687126.patch radar7756388.patch radar8304089.patch \
			initdb.patch _int_bool.c.patch prep_buildtree.patch radar13777485.patch

AEP_Binaries	= $(SERVER_INSTALL_PATH_PREFIX)$(USRBINDIR)/* $(SERVER_INSTALL_PATH_PREFIX)$/$(USRLIBDIR)/lib*.dylib $(SERVER_INSTALL_PATH_PREFIX)$/$(USRLIBDIR)/$(Project)/* $(SERVER_INSTALL_PATH_PREFIX)/$(USRLIBDIR)/postgresql/pgxs/src/test/regress/pg_regress

Configure_Products = config.log src/include/pg_config.h

ifdef SERVER_INSTALL_PATH_PREFIX
	GnuAfterInstall	= install-docs install-contribs install-internal-contribs \
			install-macosx install-backup install-wrapper archive-strip-binaries \
			install-postgres9.0-for-migration install-postgres9.1-for-migration \
			install-postgres9.2-for-migration cleanup-dst-root
else 
	GnuAfterInstall = install-docs install-contribs \
			install-macosx archive-strip-binaries \
			cleanup-dst-root
endif

# Note that pldebugger must have been patched into the source in order to use it.
ifdef DEBUG
	ContribTools	= hstore intarray pg_upgrade pg_upgrade_support pldebugger
else
	ContribTools	= hstore intarray pg_upgrade pg_upgrade_support
endif

# Extensions useful for internal use (i.e., debugging, profiling, etc.)
InternalContribTools = btree_gist pg_stat_statements pg_test_timing
InternalPGXSContribTools = plpgsql_check

# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: $(GnuAfterInstall)

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/AEP.make

# Override settings from above includes
BuildDirectory	= $(OBJROOT)/Build
Install_Target	= install
# This needs to be overridden because the project properly uses DESTDIR and
# INSTALL_ROOT (which is included in Environment).
Install_Flags	= DESTDIR="$(DSTROOT)"
Install_Flags_PGXS = PG_CONFIG="$(DSTROOT)/Applications/Server.app/Contents/ServerRoot/usr/bin/pg_config" INSTALL_ROOT="$(SERVER_INSTALL_PATH_PREFIX) -I$(OBJROOT)/postgresql/src/pl/plpgsql/src"

# Build rules

# The touch is necessary to prevent unnecessary regeneration of all documentation.
install-docs:
	@echo "Installing documentation..."
	$(TOUCH) -r $(Sources)/Makefile $(Sources)/configure
	$(MAKE) -C $(BuildDirectory) $(Install_Flags) $@


install_source::
	if [ -z "$(SERVER_INSTALL_PATH_PREFIX)" ]; then \
		echo "Stripping archives from SRCROOT"; \
		find $(SRCROOT)/Support -name \*.tar.gz -delete; \
		rm $(SRCROOT)/plpgsql_check.tgz; \
	fi

.PHONY: prepare-contribs
prepare-contribs:
	@echo "Installing specific tools from contrib:"

.PHONY: install-contribs
install-contribs: prepare-contribs ${ContribTools}
	@echo "Installed specific tools from contrib!"

.PHONY: ${ContribTools}
${ContribTools}:
	echo "...Installing $@..."
	$(MAKE) -C $(BuildDirectory)/contrib/$@ $(Install_Flags) $(Install_Target)

.PHONY: prepare-internal-contribs
prepare-internal-contribs:
	@echo "Installing specific tools from contrib for AppleInternal:"
	$(MKDIR) $(AppleInternalDir)/ServerRoot
	# Expand the plpgsql_check sources into the postgres contrib directory, as that's where it's happiest building
	$(TAR) xvf $(SRCROOT)/plpgsql_check.tgz -C $(BuildDirectory)/contrib/

.PHONY: install-internal-contribs
install-internal-contribs: prepare-internal-contribs ${InternalPGXSContribTools} ${InternalContribTools}
	@echo "Installed specific tools from contrib for AppleInternal!"

.PHONY: ${InternalContribTools}
${InternalContribTools}:
	echo "...Installing $@..."
	$(MAKE) -C $(BuildDirectory)/contrib/$@ $(Install_Flags) $(Install_Target)
	cd $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/.. && $(FIND) ServerRoot -name $@* -exec $(DITTO) {} $(AppleInternalDir)/{} \; -exec rm -rf {} \;

.PHONY: ${InternalPGXSContribTools}
${InternalPGXSContribTools}:
	echo "...Installing $@..."
	$(MAKE) -C $(BuildDirectory)/contrib/$@ $(Install_Flags_PGXS)  $(Install_Target)
	cd $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/.. && $(FIND) ServerRoot -name $@* -exec $(DITTO) {} $(AppleInternalDir)/{} \; -exec rm -rf {} \;


install-macosx:
	@echo "Installing man pages..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	$(INSTALL) Support/man/pltcl_delmod.1 $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	$(INSTALL) Support/man/pltcl_listmod.1 $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	$(INSTALL) Support/man/pltcl_loadmod.1 $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	@echo "Installing service database cluster relocation script..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/System/Library/ServerSetup/Configurations
	$(INSTALL) -m 755 Support/relocate_postgres_service_cluster $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	@echo "Installing configuration tools..."
	$(INSTALL_SCRIPT) Support/postgres_restore_tool $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	$(INSTALL_SCRIPT) Support/postgres_migration_tool $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	$(INSTALL_SCRIPT) Support/postgres_promotion_tool $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	$(INSTALL_SCRIPT) Support/postgres_postSetup_tool $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	$(INSTALL_FILE) Support/PostgreSQL.plist $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/System/Library/ServerSetup/Configurations
	@echo "Done."

install-backup: install-macosx
	@echo "Installing backup / Time Machine support..."
	$(INSTALL_SCRIPT) Support/backup_restore/xpostgres.py $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/bin/xpostgres
	$(LN) -s xpostgres $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/bin/xpg_ctl
	@echo "Done."

install-wrapper:
	@echo "Installing wrapper"
	$(MV) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/bin/postgres $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/bin/postgres_real
	$(SED) 's|@PATH_TEMPLATE@|$(SERVER_INSTALL_PATH_PREFIX)/usr/bin/postgres_real|' Support/postgres > Support/postgres_patched
	$(INSTALL) -m 755 Support/postgres_patched $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/bin/postgres
	@echo "Done."

install-postgres9.0-for-migration:
	@echo "Installing older PostgreSQL binaries to be used for migration"
	$(MKDIR) $(SYMROOT)/Migration_9.0_to_9.2
	$(MAKE) -C Support/Migration_9.0_to_9.2 install \
			SRCROOT=$(SRCROOT)/Support/Migration_9.0_to_9.2 \
			OBJROOT=$(OBJROOT)/Migration_9.0_to_9.2 \
			SYMROOT=$(SYMROOT)/Migration_9.0_to_9.2 \
			DSTROOT=$(DSTROOT) \
			BuildDirectory=$(OBJROOT)/Migration_9.0_to_9.2/Build \
			Sources=$(OBJROOT)/Migration_9.0_to_9.2/postgres \
			CoreOSMakefiles=$(CoreOSMakefiles)
	@echo "Done installing PostgreSQL 9.0"

install-postgres9.1-for-migration:
	@echo "Installing older PostgreSQL binaries to be used for migration"
	$(MKDIR) $(SYMROOT)/Migration_9.1_to_9.2
	$(MAKE) -C Support/Migration_9.1_to_9.2 install \
			SRCROOT=$(SRCROOT)/Support/Migration_9.1_to_9.2 \
			OBJROOT=$(OBJROOT)/Migration_9.1_to_9.2 \
			SYMROOT=$(SYMROOT)/Migration_9.1_to_9.2 \
			DSTROOT=$(DSTROOT) \
			BuildDirectory=$(OBJROOT)/Migration_9.1_to_9.2/Build \
			Sources=$(OBJROOT)/Migration_9.1_to_9.2/postgres \
			CoreOSMakefiles=$(CoreOSMakefiles)
	@echo "Done installing PostgreSQL 9.1"

install-postgres9.2-for-migration:
	@echo "Installing older PostgreSQL binaries to be used for migration"
	$(MKDIR) $(SYMROOT)/Migration_9.2_to_9.3
	$(MAKE) -C Support/Migration_9.2_to_9.3 install \
			SRCROOT=$(SRCROOT)/Support/Migration_9.2_to_9.3 \
			OBJROOT=$(OBJROOT)/Migration_9.2_to_9.3 \
			SYMROOT=$(SYMROOT)/Migration_9.2_to_9.3 \
			DSTROOT=$(DSTROOT) \
			BuildDirectory=$(OBJROOT)/Migration_9.2_to_9.3/Build \
			Sources=$(OBJROOT)/Migration_9.2_to_9.3/postgres \
			CoreOSMakefiles=$(CoreOSMakefiles)
	@echo "Done installing PostgreSQL 9.2"

cleanup-dst-root:
	@echo "Removing unwanted files from DSTROOT"
	$(RM) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/lib/*.a
	$(RM) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/lib/postgresql9.0/*.a
	$(RM) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/lib/postgresql9.1/*.a
	$(RM) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/lib/postgresql9.2/*.a
	if [ -z $(SERVER_INSTALL_PATH_PREFIX) ]; then \
		echo "We only want libraries installed (and headers and pg_config for internal use), so remove everything else."; \
		$(MKDIR) $(DSTROOT)/usr/local/bin; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/System; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/private; \
		$(MKDIR) $(DSTROOT)$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin; \
		$(CP) $(DSTROOT)/usr/bin/pg_config $(DSTROOT)$(TOOLCHAIN_INSTALL_DIR)/usr/local/bin; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/bin; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/libexec; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/share; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/lib/postgresql9.0; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/lib/postgresql9.1; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/lib/postgresql9.2; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/lib/postgresql/pgxs; \
		$(SILENT) $(MV) $(DSTROOT)$(REALSDKROOT)/usr/local/include $(DSTROOT)/usr/local/include; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/Applications/Xcode.app/Contents/Developer/Platforms; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/BuildRoot; \
	fi
	@echo "Done."
