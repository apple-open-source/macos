#
# Apple wrapper Makefile for PostgreSQL
# Copyright (c) 2009-2011 Apple Inc. All Rights Reserved.
#
# This build of PostgreSQLQ 9.0.x is only intended for data migration purposes.

# General project info for use with RC/GNUsource.make makefile
Project         = postgresql
ProjectName     = PostgreSQL
UserType        = Administrator
ToolType        = Commands
Submission      = 26.3

# Include common server build variables
-include /AppleInternal/ServerTools/ServerBuildVariables.xcconfig

# Variables only used by project
ServicesDir = $(NSLIBRARYSUBDIR)/Server
ServiceDir = $(ServicesDir)/$(ProjectName)
DBDir = $(ServiceDir)/postgresql9.0/pgsql

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
Extra_Configure_Flags	= --prefix=$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/postgresql9.0 \
			--sbindir=$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/postgresql9.0 \
			--bindir=$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/postgresql9.0 \
			--sysconfdir=$(SERVER_INSTALL_PATH_PREFIX)$(ETCDIR) \
			--localstatedir=$(DBDir) \
			--libdir=$(SERVER_INSTALL_PATH_PREFIX)$(USRLIBDIR)/postgresql9.0 \
			--datarootdir=$(SERVER_INSTALL_PATH_PREFIX)$(SHAREDIR)/postgresql9.0 \
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
AEP_Version	= 9.0.8
AEP_LicenseFile	= $(Sources)/COPYRIGHT
AEP_Patches	= arches.patch pg_config_manual_h.patch \
			radar7687126.patch radar7756388.patch radar8304089.patch \
			initdb.patch _int_bool.c.patch
AEP_Binaries	= $(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/postgresql9.0/* $(SERVER_INSTALL_PATH_PREFIX)$(USRLIBDIR)/postgresql9.0/* $(SERVER_INSTALL_PATH_PREFIX)$(USRLIBDIR)/postgresql9.0/pgxs/src/test/regress/pg_regress

Configure_Products = config.log src/include/pg_config.h
GnuAfterInstall	= install-contrib install-macosx install-wrapper cleanup-temp archive-strip-binaries
ContribTools	= hstore intarray pg_upgrade pg_upgrade_support

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


# Build rules
install-contrib:
	@echo "Installing specific tools from contrib:"
	for tool in $(ContribTools); do \
		echo "...Installing $${tool}...";	\
		$(MAKE) -C $(BuildDirectory)/contrib/$${tool} $(Install_Flags) $(Install_Target); \
	done

install-macosx:
	@echo "Creating service directories..."
	$(INSTALL_DIRECTORY) $(DSTROOT)$(ServicesDir)
	$(INSTALL) -m 0700 -o _postgres -g _postgres -d $(DSTROOT)$(ServiceDir)
	$(INSTALL) -m 0700 -o _postgres -g _postgres -d $(DSTROOT)$(DBDir)
	@echo "Done."

install-wrapper: 
	@echo "Installing wrapper" 
	$(MV) -v $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/postgresql9.0/postgres $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/postgresql9.0/postgres_real 
	$(INSTALL) -m 755 Support/postgres $(DSTROOT)$(SERVER_INSTALL_PATH_PREFIX)$(LIBEXECDIR)/postgresql9.0/postgres 
	@echo "Done."

cleanup-temp:
	@echo "Removing unnecessary files from DSTROOT..."
	$(RM) -rvf $(DSTROOT)$(ServiceDir)/pgbuild_tmp
	@echo "Done."
