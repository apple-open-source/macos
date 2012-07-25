#
# Apple wrapper Makefile for PostgreSQL
# Copyright (c) 2009-2011 Apple Inc. All Rights Reserved.
#

# General project info for use with RC/GNUsource.make makefile
Project         = postgresql
ProjectName     = PostgreSQL
UserType        = Administrator
ToolType        = Commands
Submission      = 56

# Include common server build variables
-include /AppleInternal/ServerTools/ServerBuildVariables.xcconfig

# Variables only used by project
SocketDir = $(if $(SERVER_INSTALL_PATH_PREFIX),$(VARDIR)/pgsql_socket,$(VARDIR)/pgsql_socket_alt)
ServicesDir = $(NSLIBRARYSUBDIR)/Server
ServiceDir = $(ServicesDir)/$(ProjectName)
DocDir = $(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)/postgresql
DBDir = $(if $(SERVER_INSTALL_PATH_PREFIX),$(ServiceDir)/Data,$(VARDIR)/pgsql)
LogDir = /Library/Logs/PostgreSQL

# Environment is passed to BOTH configure AND make, which can cause problems if these
# variables are intended to help configure, but not override the result.
Extra_CC_Flags	= -Os -g -Wall -Wno-deprecated-declarations
# This allows extra variables to be passed _just_ to configure.
Extra_Configure_Environment	= CFLAGS="$$RC_CFLAGS $(Extra_CC_Flags)" \
					LDFLAGS="$$RC_CFLAGS $(Extra_CC_Flags)" \
					LDFLAGS_EX="-mdynamic-no-pic" \
					EXTRA_LDFLAGS_PROGRAM="-mdynamic-no-pic"

# The configure flags are ordered to match current output of ./configure --help.
# Extra indentation represents suboptions.
Extra_Configure_Flags	= --prefix=$(SERVER_INSTALL_PATH_PREFIX)$(USRDIR) --sbindir=$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR) \
			--sysconfdir=$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR) --mandir=$(SERVER_INSTALL_PATH_PREFIX)$(MANDIR)\
			--localstatedir=$(DBDir) \
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
AEP_Version	= 9.1.4
AEP_LicenseFile	= $(Sources)/COPYRIGHT
AEP_Patches	= arches.patch pg_config_manual_h.patch \
			radar7687126.patch radar7756388.patch radar8304089.patch \
			initdb.patch _int_bool.c.patch prep_buildtree.patch
AEP_LaunchdConfigs	= $(if $(SERVER_INSTALL_PATH_PREFIX),org.postgresql.postgres.plist,org.postgresql.postgres_alt.plist)
AEP_Binaries	= $(SERVER_INSTALL_PATH_PREFIX)$(USRBINDIR)/* $(SERVER_INSTALL_PATH_PREFIX)$/$(USRLIBDIR)/lib*.dylib $(SERVER_INSTALL_PATH_PREFIX)$/$(USRLIBDIR)/$(Project)/* $(SERVER_INSTALL_PATH_PREFIX)/$(USRLIBDIR)/postgresql/pgxs/src/test/regress/pg_regress

Configure_Products = config.log src/include/pg_config.h
GnuAfterInstall	= $(if $(SERVER_INSTALL_PATH_PREFIX), install-docs install-contrib \
			install-macosx install-backup install-wrapper archive-strip-binaries \
			install-postgres-for-migration cleanup-dst-root, \
			install-docs install-contrib \
			install-macosx install-wrapper archive-strip-binaries \
			cleanup-dst-root)

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
	@echo "Creating service directories..."
	$(INSTALL) -m 0750 -o _postgres -g _postgres -d $(DSTROOT)$(SocketDir)
	$(INSTALL) -m 0755 -o _postgres -g _postgres -d $(DSTROOT)$(LogDir)
	@echo "Installing man pages..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	$(INSTALL) Support/man/pltcl_delmod.1 $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	$(INSTALL) Support/man/pltcl_listmod.1 $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	$(INSTALL) Support/man/pltcl_loadmod.1 $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/share/man/man1
	@echo "Installing template config file..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/Library/Preferences
	$(INSTALL) -m 644 Support/org.postgresql.postgres.plist.dist $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/Library/Preferences/org.postgresql.postgres.plist
	@echo "Installing initialization script..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	$(INSTALL) -m 755 Support/copy_postgresql_config_files.sh $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/libexec
	@echo "Done."

install-backup: install-macosx
	@echo "Installing backup / Time Machine support..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/server_backup
	$(INSTALL_SCRIPT) Support/sysexits.rb $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/server_backup
	$(INSTALL_SCRIPT) Support/backuptool.rb $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/server_backup
	$(INSTALL_SCRIPT) Support/postgresql_backup.rb $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/server_backup
	$(INSTALL_DIRECTORY) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR)/server_backup
	$(INSTALL_FILE) Support/46-postgresql.plist $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR)/server_backup
	@echo "Done."

install-wrapper:
	@echo "Installing wrapper"
	$(MV) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/bin/postgres $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/bin/postgres_real
	$(SED) 's|@PATH_TEMPLATE@|$(SERVER_INSTALL_PATH_PREFIX)/usr/bin/postgres_real|' Support/postgres > Support/postgres_patched
	$(INSTALL) -m 755 Support/postgres_patched $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/bin/postgres
	@echo "Done."

install-postgres-for-migration:
	@echo "Installing older PostgreSQL binaries to be used for migration"
	$(MKDIR) $(SYMROOT)/Migration_9.0_to_9.1
	$(MAKE) -C Support/Migration_9.0_to_9.1 install \
			SRCROOT=$(SRCROOT)/Support/Migration_9.0_to_9.1 \
			OBJROOT=$(OBJROOT)/Migration_9.0_to_9.1 \
			SYMROOT=$(SYMROOT)/Migration_9.0_to_9.1 \
			DSTROOT=$(DSTROOT) \
			BuildDirectory=$(OBJROOT)/Migration_9.0_to_9.1/Build \
			Sources=$(OBJROOT)/Migration_9.0_to_9.1/postgres \
			CoreOSMakefiles=$(CoreOSMakefiles)
	@echo "Done installing PostgreSQL 9.0"

cleanup-dst-root:
	@echo "Removing unwanted files from DSTROOT"
	$(RM) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/lib/*.a
	$(RM) $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)/usr/lib/postgresql9.0/*.a
	@echo "Done."
