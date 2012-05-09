#
# Apple wrapper Makefile for PostgreSQL
# Copyright (c) 2009-2011 Apple Inc. All Rights Reserved.
#

# General project info for use with RC/GNUsource.make makefile
Project         = postgresql
ProjectName     = PostgreSQL
UserType        = Administrator
ToolType        = Commands
Submission      = 26.5

# Variables only used by project
DBDir = $(VARDIR)/pgsql
SocketDir = $(VARDIR)/pgsql_socket
DocDir = $(NSLIBRARYSUBDIR)/WebServer/Documents
ServicesDir = $(NSLIBRARYSUBDIR)/Server
ServiceDir = $(ServicesDir)/$(ProjectName)
BackupDir = $(ServiceDir)/Backup
LogDir = /Library/Logs/PostgreSQL

RUBY = $(shell which ruby)
RubyInstallDir = $(shell $(RUBY) -rrbconfig -e 'p Config::CONFIG["vendorarchdir"]')

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
Extra_Configure_Flags	= --prefix=$(USRDIR) --sbindir=$(LIBEXECDIR) \
			--sysconfdir=$(ETCDIR) \
			--localstatedir=/var/pgsql \
			--htmldir=$(NSLIBRARYSUBDIR)/WebServer/Documents/$(Project) \
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
AEP_Version	= 9.0.5
AEP_LicenseFile	= $(Sources)/COPYRIGHT
AEP_Patches	= arches.patch pg_config_manual_h.patch \
			radar7687126.patch radar7756388.patch radar8304089.patch \
			initdb.patch _int_bool.c.patch
AEP_LaunchdConfigs	= org.postgresql.postgres.plist
#AEP_Binaries	= $(USRBINDIR)/* $(USRLIBDIR)/lib*.dylib $(USRLIBDIR)/$(Project)/*
AEP_Binaries	= $(addprefix /,$(shell cd $(DSTROOT) && $(FIND) usr -type f -perm +0111 -exec $(SHELL) -c 'test \"`file -b --mime-type {}`\" = \"application/octet-stream\"' \; -print))

Configure_Products = config.log src/include/pg_config.h
GnuAfterInstall	= install-docs install-contrib \
			install-macosx install-backup install-wrapper archive-strip-binaries 
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
	$(INSTALL) -m 0700 -o _postgres -g _postgres -d $(DSTROOT)$(DBDir)
	$(INSTALL) -m 0750 -o _postgres -g _postgres -d $(DSTROOT)$(SocketDir)
	$(INSTALL_DIRECTORY) $(DSTROOT)$(ServicesDir)
	$(INSTALL) -m 0700 -o _postgres -g _postgres -d $(DSTROOT)$(ServiceDir)
	$(INSTALL) -m 0755 -o _postgres -g _postgres -d $(DSTROOT)$(LogDir)
	@echo "Done."

install-backup: install-macosx
	@echo "Installing backup / Time Machine support..."
	$(INSTALL) -m 0700 -o _postgres -g _postgres -d $(DSTROOT)$(BackupDir)
	$(INSTALL_DIRECTORY) $(DSTROOT)$(RubyInstallDir)
	$(INSTALL_SCRIPT) Support/sysexits.rb $(DSTROOT)$(RubyInstallDir)
	$(INSTALL_SCRIPT) Support/backuptool.rb $(DSTROOT)$(RubyInstallDir)
	$(INSTALL_DIRECTORY) $(DSTROOT)$(LIBEXECDIR)/server_backup
	$(INSTALL_SCRIPT) Support/postgresql_backup.rb $(DSTROOT)$(LIBEXECDIR)/server_backup
	$(INSTALL_DIRECTORY) $(DSTROOT)$(ETCDIR)/server_backup
	$(INSTALL_FILE) Support/46-postgresql.plist $(DSTROOT)$(ETCDIR)/server_backup
	@echo "Done."

install-wrapper: 
	@echo "Installing wrapper" 
	$(MV) $(DSTROOT)/usr/bin/postgres $(DSTROOT)/usr/bin/postgres_real 
	$(INSTALL) -m 755 Support/postgres $(DSTROOT)/usr/bin/postgres 
	@echo "Done." 
