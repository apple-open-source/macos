#
# Apple wrapper Makefile for PostgreSQL
# Copyright (c) 2009-2013 Apple Inc. All Rights Reserved.
#

# General project info for use with RC/GNUsource.make makefile
Project         = postgresql
UserType        = Administrator
ToolType        = Commands
Submission      = 97

# Include common server build variables
-include /AppleInternal/ServerTools/ServerBuildVariables.xcconfig

# Variables only used by project


ServicesDir = $(NSLIBRARYSUBDIR)/Server
ServiceDirCustomer = $(ServicesDir)/PostgreSQL
DocDir = $(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)/postgresql
DBDirCustomer = $(ServiceDirCustomer)/Data

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
Extra_Configure_Flags	= --prefix=$(SERVER_INSTALL_PATH_PREFIX)$(USRDIR) --sbindir=$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR) \
			--sysconfdir=$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR) --mandir=$(SERVER_INSTALL_PATH_PREFIX)$(MANDIR) \
			--localstatedir=$(DBDirCustomer) \
			--htmldir=$(DocDir) \
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
			--with-system-tzdata=$(SHAREDIR)/zoneinfo

Extra_Make_Flags	=

# Additional project info used with AEP
AEP		= YES
AEP_Version	= 9.2.4
AEP_LicenseFile	= $(Sources)/COPYRIGHT
AEP_Patches	= arches.patch pg_config_manual_h.patch \
			radar7687126.patch radar7756388.patch radar8304089.patch \
			initdb.patch _int_bool.c.patch prep_buildtree.patch
AEP_LaunchdConfigs	= org.postgresql.postgres.plist com.apple.postgres.plist
AEP_Binaries	= $(SERVER_INSTALL_PATH_PREFIX)$(USRBINDIR)/* $(SERVER_INSTALL_PATH_PREFIX)$/$(USRLIBDIR)/lib*.dylib $(SERVER_INSTALL_PATH_PREFIX)$/$(USRLIBDIR)/$(Project)/* $(SERVER_INSTALL_PATH_PREFIX)/$(USRLIBDIR)/postgresql/pgxs/src/test/regress/pg_regress

Configure_Products = config.log src/include/pg_config.h
GnuAfterInstall	= install-docs install-contrib \
			install-macosx install-backup install-wrapper archive-strip-binaries \
			install-postgres9.0-for-migration install-postgres9.1-for-migration cleanup-dst-root

ContribTools	= hstore intarray pg_upgrade pg_upgrade_support

# Local targets that must be defined before including the following
# files to get the dependency order correct
.PHONY: $(GnuAfterInstall)

# Include common makefile targets for B&I
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/GNUSource.make
include $(MAKEFILEPATH)/CoreOS/ReleaseControl/AEP.make

# Override settings from above includes
LAUNCHDDIR := $(SERVER_INSTALL_PATH_PREFIX)$(LAUNCHDDIR)
BuildDirectory	= $(OBJROOT)/Build
Install_Target	= install
# This needs to be overridden because the project properly uses DESTDIR and
# INSTALL_ROOT (which is included in Environment).
Install_Flags	= DESTDIR="$(DSTROOT)"


# Build rules

# The touch is necessary to prevent unnecessary regeneration of all documentation.
install-docs:
	@echo "Installing documentation..."
	$(TOUCH) -r $(Sources)/Makefile $(Sources)/configure
	$(MAKE) -C $(BuildDirectory) $(Install_Flags) $@

install-contrib:
	@echo "Installing specific tools from contrib:"
	for tool in $(ContribTools); do \
		echo "...Installing $${tool}...";	\
		$(MAKE) -C $(BuildDirectory)/contrib/$${tool} $(Install_Flags) $(Install_Target); \
	done

install-macosx:
	@echo "Installing man pages..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	$(INSTALL) Support/man/pltcl_delmod.1 $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	$(INSTALL) Support/man/pltcl_listmod.1 $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	$(INSTALL) Support/man/pltcl_loadmod.1 $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	@echo "Installing template config files..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/Library/Preferences
	$(INSTALL) -m 644 Support/org.postgresql.postgres.plist.dist $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/Library/Preferences/org.postgresql.postgres.plist
	$(INSTALL) -m 644 Support/com.apple.postgres.plist.dist $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/Library/Preferences/com.apple.postgres.plist
	@echo "Installing webapp plist files..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc/apache2/webapps
	$(INSTALL) -m 644 Support/webapp_org.postgresql.postgres.plist.dist $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc/apache2/webapps/org.postgresql.postgres.plist
	$(CP) -p $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc/apache2/webapps/org.postgresql.postgres.plist $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/private/etc/apache2/webapps/org.postgresql.postgres.plist.default
	@echo "Installing initialization script..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	$(INSTALL) -m 755 Support/copy_postgresql_config_files.sh $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	@echo "Installing Extras scripts..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/System/Library/ServerSetup/CommonExtras/PostgreSQLExtras
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/System/Library/ServerSetup/RestoreExtras
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/System/Library/ServerSetup/MigrationExtras
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/System/Library/ServerSetup/PromotionExtras
	$(INSTALL_SCRIPT) Support/05_PostgresRestoreExtra.pl $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/System/Library/ServerSetup/RestoreExtras
	$(INSTALL_SCRIPT) Support/05_postgresmigrator.rb $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/System/Library/ServerSetup/MigrationExtras
	$(INSTALL_SCRIPT) Support/58_postgres_setup.rb $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/System/Library/ServerSetup/PromotionExtras
	@echo "Done."

install-backup: install-macosx
	@echo "Installing backup / Time Machine support..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR)/server_backup
	$(INSTALL_FILE) Support/backup_restore/46-postgresql.plist $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR)/server_backup
	$(INSTALL_SCRIPT) Support/backup_restore/xpg_archive_command $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	$(INSTALL_SCRIPT) Support/backup_restore/xpostgres $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/bin
	$(INSTALL_SCRIPT) Support/backup_restore/xpg_ctl $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/bin
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

cleanup-dst-root:
	@echo "Removing unwanted files from DSTROOT"
	$(RM) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/lib/*.a
	$(RM) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/lib/postgresql9.0/*.a
	$(RM) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/lib/postgresql9.1/*.a
	if [ -z $(SERVER_INSTALL_PATH_PREFIX) ]; then \
		echo "We only want libraries installed, so remove everything else."; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/System; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/private; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/bin; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/libexec; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/include; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/share; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/lib/postgresql9.0; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/lib/postgresql9.1; \
		$(SILENT) $(RM) -Rf $(DSTROOT)/usr/lib/postgresql/pgxs; \
	fi
	@echo "Done."
