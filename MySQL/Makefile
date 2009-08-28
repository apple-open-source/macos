#
# Copyright (c) 2001-2009 Apple Computer, Inc.
#
# Starting with MySQL 3.23.54, the source patch to handle installation
# directories with tildes no longer works. Going forward, this Makefile
# takes the simpler approach of installing into a staging directory in /tmp 
# and then dittoing that into DSTROOT.
#
# The patch for config.h.in is needed regardless of MySQL version; it
# makes MySQL generate correct code for PPC  but not i386 when building fat.
#

# These includes provide the proper paths to system utilities

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

PROJECT_NAME	  = MySQL
MYSQL_VERSION	  = 5.0.82
MYSQL_BASE_DIR	  = mysql-$(MYSQL_VERSION)
BUILD_DIR         = /usr
MYSQL_BUILD_DIR   = build
MYSQL_SRC_DIR     = $(MYSQL_BUILD_DIR)/src
MYSQL_STAGING_DIR = $(MYSQL_BUILD_DIR)/staging
SHARE_DIR         = usr/share
VERSIONS_DIR      = usr/local/OpenSourceVersions
LICENSE_DIR       = usr/local/OpenSourceLicenses
LIBEXEC_DIR       = usr/libexec

# Tool paths

INSTALL           = /usr/bin/install
DITTO             = /usr/bin/ditto
CHOWN             = /usr/sbin/chown
PATCH             = /usr/bin/patch
MKDIR             = /bin/mkdir

# Build paths

ifndef SRCROOT
SRCROOT		:= /tmp/MySQL.build/MySQL
endif
ifndef OBJROOT
OBJROOT		:= /tmp/MySQL.build/MySQL~obj
endif
ifndef DSTROOT
DSTROOT		:= /tmp/MySQL.build/MySQL~dst
endif
ifndef SYMROOT
SYMROOT		:= /tmp/MySQL.build/MySQL~sym
endif

#
# Architectures to be build (ppc i386 ppc64 x86_64)
#

ifdef RC_ARCHS
MYSQL_ARCHS		:= $(RC_ARCHS)
else
MYSQL_ARCHS		:= i386
endif

# Helper macros for using MYSQL_ARCHS
NUM_ARCHS       = $(words $(MYSQL_ARCHS))

ONE_WAY_BUILD   = $(filter $(NUM_ARCHS),1)
TWO_WAY_BUILD   = $(filter $(NUM_ARCHS),2)
THREE_WAY_BUILD = $(filter $(NUM_ARCHS),3)
FOUR_WAY_BUILD  = $(filter $(NUM_ARCHS),4)

FIRST_ARCH      = $(firstword $(MYSQL_ARCHS))
SECOND_ARCH     = $(word 2,$(MYSQL_ARCHS))
THIRD_ARCH      = $(word 3,$(MYSQL_ARCHS))
FOURTH_ARCH     = $(word 4,$(MYSQL_ARCHS))

SRC_BASE        = $(OBJROOT)/$(MYSQL_SRC_DIR)
CUR_ARCH_SRC    = $(SRC_BASE)/$(cur_arch)

STAGING_BASE       = $(OBJROOT)/$(MYSQL_STAGING_DIR)
FIRST_ARCH_STAGING = $(STAGING_BASE)/$(FIRST_ARCH)
CUR_ARCH_STAGING   = $(STAGING_BASE)/$(cur_arch)

#
# Special file/direcotry lists for various targets
#

FILES_TO_INSTALL		= \
$(MYSQL_BASE_DIR).tar.gz \
Makefile \
MySQL.plist \
MySQL.txt \
applemysqlcheckcnf \
applemysqlcheckcnf.8 \
mysqlman.1 \
patch

FILES_TO_REMOVE = \
usr/bin/make_win_binary_distribution \
usr/bin/make_win_src_distribution \
usr/share/info/dir \
usr/share/mysql/Info.plist \
usr/share/mysql/Makefile \
usr/share/mysql/ReadMe.txt \
usr/share/mysql/StartupParameters.plist \
usr/share/mysql/postinstall \
usr/share/mysql/preinstall \
usr/share/mysql/Description.plist \
usr/share/mysql/make_win_src_distribution \
usr/share/mysql/mysql-test/r/fix_priv_tabs.result

USRBIN_FILES_TO_RENAME = \
comp_err \
innochecksum \
msql2mysql \
my_print_defaults \
myisam_ftdump \
myisamchk \
myisamlog \
myisampack \
mysql \
mysql_client_test \
mysql_config \
mysql_convert_table_format \
mysql_create_system_tables \
mysql_explain_log \
mysql_find_rows \
mysql_fix_extensions \
mysql_fix_privilege_tables \
mysql_install_db \
mysql_secure_installation \
mysql_setpermission \
mysql_tableinfo \
mysql_tzinfo_to_sql \
mysql_upgrade \
mysql_upgrade_shell \
mysql_waitpid \
mysql_zap \
mysqlaccess \
mysqladmin \
mysqlbinlog \
mysqlbug \
mysqlcheck \
mysqld_multi \
mysqld_safe \
mysqldump \
mysqldumpslow \
mysqlhotcopy \
mysqlimport \
mysqlshow \
mysqltest \
mysqltestmanager \
mysqltestmanager-pwgen \
mysqltestmanagerc \
perror \
replace \
resolve_stack_dump \
resolveip

USRLIBEXEC_FILES_TO_RENAME = \
mysqld \
mysqlmanager

USRMYSQLTEST_FILES_TO_RENAME = \
install_test_db \
mtr \
mysql-stress-test.pl \
mysql-test-run \
mysql-test-run-shell \
mysql-test-run.pl

USRSHAREMAN1_FILES_TO_RENAME = \
comp_err.1 \
innochecksum.1 \
make_win_bin_dist.1 \
make_win_src_distribution.1 \
msql2mysql.1 \
my_print_defaults.1 \
myisam_ftdump.1 \
myisamchk.1 \
myisamlog.1 \
myisampack.1 \
mysql-stress-test.pl.1 \
mysql-test-run.pl.1 \
mysql.1 \
mysql.server.1 \
mysql_client_test.1 \
mysql_config.1 \
mysql_convert_table_format.1 \
mysql_create_system_tables.1 \
mysql_explain_log.1 \
mysql_find_rows.1 \
mysql_fix_extensions.1 \
mysql_fix_privilege_tables.1 \
mysql_install_db.1 \
mysql_secure_installation.1 \
mysql_setpermission.1 \
mysql_tableinfo.1 \
mysql_tzinfo_to_sql.1 \
mysql_upgrade.1 \
mysql_upgrade_shell.1 \
mysql_waitpid.1 \
mysql_zap.1 \
mysqlaccess.1 \
mysqladmin.1 \
mysqlbinlog.1 \
mysqlbug.1 \
mysqlcheck.1 \
mysqld_multi.1 \
mysqld_safe.1 \
mysqldump.1 \
mysqldumpslow.1 \
mysqlhotcopy.1 \
mysqlimport.1 \
mysqlman.1 \
mysqlshow.1 \
mysqltest.1 \
mysqltestmanager-pwgen.1 \
mysqltestmanager.1 \
mysqltestmanagerc.1 \
perror.1 \
replace.1 \
resolve_stack_dump.1 \
resolveip.1 \
safe_mysqld.1

USRSHAREMAN8_FILES_TO_RENAME = \
mysqld.8 \
mysqlmanager.8

USRSHAREMYSQL_FILES_TO_RENAME = \
mysql.server \
mysqld_multi.server

USRBIN_FILES_TO_LIPO = \
comp_err \
innochecksum \
my_print_defaults \
myisam_ftdump \
myisamchk \
myisamlog \
myisampack \
mysql \
mysql_client_test \
mysql_tzinfo_to_sql \
mysql_upgrade \
mysql_waitpid \
mysqladmin \
mysqlbinlog \
mysqlcheck \
mysqldump \
mysqlimport \
mysqlshow \
mysqltest \
mysqltestmanager \
mysqltestmanager-pwgen \
mysqltestmanagerc \
perror \
replace \
resolve_stack_dump \
resolveip

USRLIBMYSQL_FILES_TO_LIPO = \
libdbug.a \
libheap.a \
libmyisam.a \
libmyisammrg.a \
libmysqlclient.a \
libmysqlclient_r.a \
libmystrings.a \
libmysys.a \
libvio.a

USRLIBEXEC_FILES_TO_LIPO = \
mysqld \
mysqlmanager

MAN1_FILES_TO_LINK = \
mysqltestmanager.1 \
mysqltestmanager-pwgen.1 \
mysqltestmanagerc.1 \
mysql_upgrade.1 \
mysql_upgrade_shell.1


#================================================================================
#
# DEFAULT MAKE TARGET
#
#================================================================================

default: build

#================================================================================
#
# EXPANSION PHASE
#
#================================================================================

#
# Expand the source tarball into individual source directories for each architecture
#
mysql: $(OBJROOT)
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] Extracting sources from archive ($(MYSQL_ARCHS))..."
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	$(SILENT) -$(RM) -rf $(MYSQL_BASE_DIR) mysql $(SRC_BASE)
	$(SILENT) $(MKDIRS) $(SRC_BASE); \
	$(foreach cur_arch, $(MYSQL_ARCHS), \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * expanding sources for arch=$(cur_arch)"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(SRC_BASE); $(TAR) -xzf $(SRCROOT)/$(MYSQL_BASE_DIR).tar.gz; \
		$(MV) $(MYSQL_BASE_DIR) $(cur_arch); \
	)

untar: mysql 

#================================================================================
#
# CONFIGURE PHASE
#
#================================================================================

.PHONY: configure-do-all configure-banner configure-do-config configure-do-patch

CONFIG_STD_OPTS	= \
		--infodir=/usr/share/info \
		--with-extra-charsets=complex \
		--with-low-memory \
		--enable-thread-safe-client \
		--enable-local-infile \
		--mandir=/usr/share/man \
		--localstatedir=/var/mysql  \
		--sysconfdir=/etc \
		--with-mysqld-user=mysql \
		--without-bench \
		--without-debug \
		--disable-shared \
		--disable-dependency-tracking \
		--with-unix-socket-path=/var/mysql/mysql.sock \
		--prefix=$(BUILD_DIR)

GOOGLE_PRECONFIG_FILES_TO_PATCH	= \
innobase/configure \
innobase/configure.in \
innobase/ib_config.h \
innobase/ib_config.h.in \
innobase/btr/btr0cur.c \
innobase/btr/btr0sea.c \
innobase/buf/buf0buf.c \
innobase/include/buf0buf.ic \
innobase/include/os0sync.h \
innobase/include/os0sync.ic \
innobase/include/srv0srv.h \
innobase/include/sync0rw.h \
innobase/include/sync0rw.ic \
innobase/include/sync0sync.h \
innobase/include/sync0sync.ic \
innobase/include/univ.i \
innobase/mem/mem0pool.c \
innobase/row/row0sel.c \
innobase/srv/srv0srv.c \
innobase/srv/srv0start.c \
innobase/sync/sync0arr.c \
innobase/sync/sync0rw.c \
innobase/sync/sync0sync.c \
libmysqld/ha_innodb.cc \
sql/ha_innodb.cc

APPLE_PATCH_BASE_DIR	= $(SRCROOT)/patch/apple
GOOGLE_PATCH_BASE_DIR	= $(SRCROOT)/patch/google/5.0.67

configure-do-all: configure-banner configure-do-pre-patch configure-do-config configure-do-post-patch

configure-banner:
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] Configuring sources ($(MYSQL_ARCHS))..."
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"

configure-do-pre-patch:
	$(foreach cur_arch, $(MYSQL_ARCHS), \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * applying Google pre-configure patches; arch=$(cur_arch)"; \
		$(foreach cur_file, $(GOOGLE_PRECONFIG_FILES_TO_PATCH), \
			$(SILENT) $(CD) $(CUR_ARCH_SRC); $(PATCH) -u $(cur_file) $(GOOGLE_PATCH_BASE_DIR)/$(cur_file).patch; \
		) \
	)

configure-do-config:
	$(foreach cur_arch, $(MYSQL_ARCHS), \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * configuring arch=$(cur_arch)..."; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(CUR_ARCH_SRC); \
		CFLAGS="-O3 -fno-omit-frame-pointer $(RC_CFLAGS)" \
		CXXFLAGS="-O3 -fno-omit-frame-pointer -felide-constructors -fno-exceptions -fno-rtti $(RC_CFLAGS)" \
		LDFLAGS="$(RC_CFLAGS)" \
		./configure --target=$(cur_arch)-apple-darwin $(CONFIG_STD_OPTS); \
	)

configure-do-post-patch:
	$(foreach cur_arch, $(MYSQL_ARCHS), \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * patching sql/mysqld.cc; arch=$(cur_arch)..."; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(CUR_ARCH_SRC)/sql; $(PATCH) -u mysqld.cc $(APPLE_PATCH_BASE_DIR)/mysqld.cc.patch; \
	)

configure: untar configure-do-all


#================================================================================
#
# BUILD PHASE
#
#================================================================================

.PHONY: build-do-all build-banner build-make build-patch-support build-patch-script

build-do-all: build-banner build-make build-patch-support build-patch-script

build-banner:
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] Building ($(MYSQL_ARCHS))..."
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"

build-make:
	$(foreach cur_arch, $(MYSQL_ARCHS), \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * building; arch=$(cur_arch)..."; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(CUR_ARCH_SRC); RC_ARCHS="$(cur_arch)" RC_CFLAGS="-arch $(cur_arch) $(RC_NONARCH_CFLAGS)" make; \
	)

build-patch-support:
	$(foreach cur_arch, $(MYSQL_ARCHS), \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * patching support-files; arch=$(cur_arch)..."; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(CUR_ARCH_SRC)/support-files; $(PATCH) -u my-huge.cnf $(APPLE_PATCH_BASE_DIR)/my-huge.cnf.patch; \
		$(SILENT) $(CD) $(CUR_ARCH_SRC)/support-files; $(PATCH) -u my-large.cnf $(APPLE_PATCH_BASE_DIR)/my-large.cnf.patch; \
	)

build-patch-script:
	$(foreach cur_arch, $(MYSQL_ARCHS), \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * patching scripts/mysqld_safe; arch=$(cur_arch)..."; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(CUR_ARCH_SRC)/scripts; $(PATCH) -u mysqld_safe $(APPLE_PATCH_BASE_DIR)/mysqld_safe.patch; \
	)

build: configure build-do-all

#================================================================================
#
# INSTALL PHASE
# 
# NOTE: Due to compiler changes introduced in Mac OS X 10.5, executables built 
#       with -arch ppc generate a new architecture, "ppc7400".  When using the
#       lipo tool to operate on these binaries, the "-arch ppc7400" flag is 
#       substituted to access that architecture.  However, when using lipo to 
#       operate on static library (.a) files, the "-arch ppc" flag continues to 
#       be used to access ppc or ppc7400 architectures contained in those files. 
#       This make file has been updated to automatically make these substitutions 
#       as needed.
# 
#================================================================================

# For make install, must set DESTDIR to shadow directory

# NOTE: Non-architecture-specific files are copied from the PPC staging dir

#
# Function: sub_create_thin_binary
# Descriptions: 
#	Creates thin binaries for each target architecture from the fat binaries 
#   created in the install-make target.  This is needed because the RC_*
#   environment variables will cause mysql/Makefile to create universal 
#   binaries, but due to limitations in the mysql/configure script, the 
#   configured sources are only valid for one of the built architectures.  
#   To get around this, multiple mysql/configure passes are made for each 
#   architecture with the configured sources placed in separate build 
#   directories for each architecture. Then mysql/Makefile (install-make) is 
#   invoked for each build directory, and the valid (thin) architecture from 
#   each directory is extracted and later combined into a single universal 
#   containing the correct build results for all requested architectures.  The
#   final re-combining is performed in the sub_create_install_binary funtion.
# Inputs:
#   $1 staging directory path
#   $2 architecture name (e.g. ppc, i386, etc.)
#   $3 path to file (e.g. usr/bin)
#   $4 base file name (e.g. comp_err)
#   $5 "-thin" name for ppc (e.g. ppc or ppc7400)
# Notes:
#   1. The function creates 2 files in each architecture staging directory:
#      <filename>.thin.<arch> and <filename>.univ.  These files are removed
#      from the FIRST_ARCH staging directory in sub_create_install_binary prior
#      to the final installation in DSTROOT.
#   2. This function is not called for ONE_WAY_BUILD makes.
#
LIPO_THIN_ARCH    = $(patsubst ppc,$5,$2)
LIPO_THIN_INFILE  = $1/$2/$3/$4
LIPO_THIN_OUTFILE = $1/$2/$3/$4.thin.$2
define sub_create_thin_binary
	$(MV) $(LIPO_THIN_INFILE) $(LIPO_THIN_INFILE).univ; \
	$(LIPO) -thin $(LIPO_THIN_ARCH) $(LIPO_THIN_INFILE).univ -output $(LIPO_THIN_OUTFILE)
endef

#
# Function: sub_create_install_binary
# Descriptions: 
#	Creates a the final binary file for installation. For multi-arch builds, 
#   the thin binaries created in sub_create_thin_binary are combined to create
#   a single universal binary.  An unstripped copy of the resulting binary 
#   (thin or universal) is  placed in SYMROOT for the B&I archive. The binary
#   to be installed is stripped of debugging symbols. Addditionally, the thin 
#   binaries created by sub_create_thin_binary are removed here if needed.
# Inputs:
#   $1 staging directory path 
#   $2 path to file (e.g. usr/bin)
#   $3 base file name (e.g. comp_err)
#   $4 "-arch" name for ppc (e.g. ppc or ppc7400)
#
LIPO_INFILE_ARCH_1  = -arch $(patsubst ppc,$4,$(FIRST_ARCH)) $1/$(FIRST_ARCH)/$2/$3.thin.$(FIRST_ARCH)
LIPO_INFILE_ARCH_2  = -arch $(patsubst ppc,$4,$(SECOND_ARCH)) $1/$(SECOND_ARCH)/$2/$3.thin.$(SECOND_ARCH)
LIPO_INFILE_ARCH_3  = -arch $(patsubst ppc,$4,$(THIRD_ARCH)) $1/$(THIRD_ARCH)/$2/$3.thin.$(THIRD_ARCH)
LIPO_INFILE_ARCH_4  = -arch $(patsubst ppc,$4,$(FOURTH_ARCH)) $1/$(FOURTH_ARCH)/$2/$3.thin.$(FOURTH_ARCH)
LIPO_OUTFILE = $1/$(FIRST_ARCH)/$2/$3
define sub_create_install_binary
	$(if $(FOUR_WAY_BUILD),$(LIPO) -create $(LIPO_INFILE_ARCH_1) $(LIPO_INFILE_ARCH_2) $(LIPO_INFILE_ARCH_3) $(LIPO_INFILE_ARCH_4) -output $(LIPO_OUTFILE);,
		$(if $(THREE_WAY_BUILD),$(LIPO) -create $(LIPO_INFILE_ARCH_1) $(LIPO_INFILE_ARCH_2) $(LIPO_INFILE_ARCH_3) -output $(LIPO_OUTFILE);,
			$(if $(TWO_WAY_BUILD),$(LIPO) -create $(LIPO_INFILE_ARCH_1) $(LIPO_INFILE_ARCH_2) -output $(LIPO_OUTFILE);,
			) \
		) \
	) \
	$(CP) $(LIPO_OUTFILE) $(SYMROOT)/; \
	-$(STRIP) -S $(LIPO_OUTFILE) > /dev/null 2>&1; \
	$(RM) -rf $(LIPO_OUTFILE).univ $(LIPO_OUTFILE).thin.$(FIRST_ARCH); \
	$(LIPO) -info $(LIPO_OUTFILE)
endef

.PHONY: install-do-all install-disabled install-banner install-clean \
        install-make install-archive install-rename install-man install-info \
		install-scripts install-unused install-symlink install-thin install-lipo \
		install-config install-copy install-patch-test install-finish

install-do-all: install-banner install-clean install-make \
				install-rename install-man install-info install-scripts \
				install-unused install-symlink install-thin install-lipo \
				install-config install-copy install-patch-test install-finish

install-disabled: 

install-banner:
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] Creating installation ($(MYSQL_ARCHS))..."
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"

install-clean:
	$(SILENT) $(RM) -rf $(OBJROOT)/$(MYSQL_STAGING_DIR)
	$(SILENT) $(MKDIRS) $(OBJROOT)/$(MYSQL_STAGING_DIR)

install-make:
	$(foreach cur_arch, $(MYSQL_ARCHS), \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * make-installing; arch=$(cur_arch)"; \
		$(SILENT) $(ECHO) "#"; \
		$(SILENT) $(CD) $(CUR_ARCH_SRC); make install DESTDIR=$(CUR_ARCH_STAGING); \
	)

install-archive:
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * archiving staging dirs for checkpoint"
	@$(ECHO) "#"
	$(SILENT) $(CD) $(OBJROOT)/$(MYSQL_BUILD_DIR); $(TAR) cvzf /Users/Shared/staging.tgz staging

install-rename:
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * removing arch prefixes from installed files"
	@$(ECHO) "#"
	$(foreach cur_arch, $(MYSQL_ARCHS), \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * removing arch prefixes in /usr/bin; arch=$(cur_arch)"; \
		$(foreach cur_file, $(USRBIN_FILES_TO_RENAME), \
			$(SILENT) $(CD) $(CUR_ARCH_STAGING)/usr/bin; $(MV) $(cur_arch)-apple-darwin-$(cur_file) $(cur_file); \
		) \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * removing arch prefixes in /usr/libexec; arch=$(cur_arch)"; \
		$(foreach cur_file, $(USRLIBEXEC_FILES_TO_RENAME), \
			$(SILENT) $(CD) $(CUR_ARCH_STAGING)/usr/libexec; $(MV) $(cur_arch)-apple-darwin-$(cur_file) $(cur_file); \
		) \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * removing arch prefixes in /usr/mysql-test; arch=$(cur_arch)"; \
		$(foreach cur_file, $(USRMYSQLTEST_FILES_TO_RENAME), \
			$(SILENT) $(CD) $(CUR_ARCH_STAGING)/usr/mysql-test; $(MV) $(cur_arch)-apple-darwin-$(cur_file) $(cur_file); \
		) \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * removing arch prefixes in /usr/share/man/man1; arch=$(cur_arch)"; \
		$(foreach cur_file, $(USRSHAREMAN1_FILES_TO_RENAME), \
			$(SILENT) $(CD) $(CUR_ARCH_STAGING)/usr/share/man/man1; $(MV) $(cur_arch)-apple-darwin-$(cur_file) $(cur_file); \
		) \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * removing arch prefixes in /usr/share/man/man8; arch=$(cur_arch)"; \
		$(foreach cur_file, $(USRSHAREMAN8_FILES_TO_RENAME), \
			$(SILENT) $(CD) $(CUR_ARCH_STAGING)/usr/share/man/man8; $(MV) $(cur_arch)-apple-darwin-$(cur_file) $(cur_file); \
		) \
		$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * removing arch prefixes in /usr/share/mysql; arch=$(cur_arch)"; \
		$(foreach cur_file, $(USRSHAREMYSQL_FILES_TO_RENAME), \
			$(SILENT) $(CD) $(CUR_ARCH_STAGING)/usr/share/mysql; $(MV) $(cur_arch)-apple-darwin-$(cur_file) $(cur_file); \
		) \
	)

install-man:
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * installing Apple man pages"
	@$(ECHO) "#"
	$(SILENT) $(CP) $(SRCROOT)/mysqlman.1 $(FIRST_ARCH_STAGING)/usr/share/man/man1
	$(SILENT) $(MKDIR) -p -m 755 $(FIRST_ARCH_STAGING)/usr/share/man/man8
	$(SILENT) $(CHOWN) root:wheel $(FIRST_ARCH_STAGING)/usr/share/man/man8
	$(SILENT) $(CP) $(SRCROOT)/applemysqlcheckcnf.8 $(FIRST_ARCH_STAGING)/usr/share/man/man8
	$(SILENT) $(CHMOD) 644 $(FIRST_ARCH_STAGING)/usr/share/man/man8/applemysqlcheckcnf.8

install-info:
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * installing version and license info"
	@$(ECHO) "#"
	$(SILENT) $(MKDIRS) $(FIRST_ARCH_STAGING)/$(VERSIONS_DIR)
	$(SILENT) $(INSTALL) -m 444 -o root -g wheel $(SRCROOT)/MySQL.plist $(FIRST_ARCH_STAGING)/$(VERSIONS_DIR)
	$(SILENT) $(MKDIRS) $(FIRST_ARCH_STAGING)/$(LICENSE_DIR)
	$(SILENT) $(INSTALL) -m 444 -o root -g wheel $(SRCROOT)/MySQL.txt $(FIRST_ARCH_STAGING)/$(LICENSE_DIR)

install-scripts:
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * installing Apple support scripts"
	@$(ECHO) "#"
	$(SILENT) $(MKDIRS) $(FIRST_ARCH_STAGING)/$(LIBEXEC_DIR)
	$(SILENT) $(INSTALL) -m 755 -o root -g wheel $(SRCROOT)/applemysqlcheckcnf $(FIRST_ARCH_STAGING)/$(LIBEXEC_DIR)

install-unused:
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * removing unused files"
	@$(ECHO) "#"
	$(SILENT) -$(MV) $(FIRST_ARCH_STAGING)/usr/mysql-test $(FIRST_ARCH_STAGING)/usr/share/mysql
	$(foreach cur_file, $(FILES_TO_REMOVE), \
		$(SILENT) $(RM) -rf $(FIRST_ARCH_STAGING)/$(cur_file); \
	)

install-symlink:
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * creating symlinks for missing man pages"
	@$(ECHO) "#"
	$(foreach cur_file, $(MAN1_FILES_TO_LINK), \
		$(SILENT) $(CD) $(FIRST_ARCH_STAGING)/usr/share/man/man1; $(LN) mysqlman.1 $(cur_file); \
	)

install-thin:
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * creating thin binaries"
	@$(ECHO) "#"
	$(if $(ONE_WAY_BUILD), \
		@$(ECHO) "Skipping install-thin for single arch build ($(MYSQL_ARCHS))."; \
	, \
		$(foreach cur_arch, $(MYSQL_ARCHS), \
			$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * creating thin binaries in /usr/bin; arch=$(cur_arch)"; \
			$(foreach cur_file, $(USRBIN_FILES_TO_LIPO), \
				$(SILENT) $(call sub_create_thin_binary,$(OBJROOT)/$(MYSQL_STAGING_DIR),$(cur_arch),usr/bin,$(cur_file),ppc7400); \
			) \
			$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * creating thin binaries in /usr/lib/mysql; arch=$(cur_arch)"; \
			$(foreach cur_file, $(USRLIBMYSQL_FILES_TO_LIPO), \
				$(SILENT) $(call sub_create_thin_binary,$(OBJROOT)/$(MYSQL_STAGING_DIR),$(cur_arch),usr/lib/mysql,$(cur_file),ppc); \
			) \
			$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * creating thin binaries in /usr/libexec; arch=$(cur_arch)"; \
			$(foreach cur_file, $(USRLIBEXEC_FILES_TO_LIPO), \
				$(SILENT) $(call sub_create_thin_binary,$(OBJROOT)/$(MYSQL_STAGING_DIR),$(cur_arch),usr/libexec,$(cur_file),ppc7400); \
			) \
		) \
	)

install-lipo: $(SYMROOT)
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * creating universal binaries"
	@$(ECHO) "#"
	$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * creating universal binaries in /usr/bin"
	$(foreach cur_file, $(USRBIN_FILES_TO_LIPO), \
		$(SILENT) $(call sub_create_install_binary,$(OBJROOT)/$(MYSQL_STAGING_DIR),usr/bin,$(cur_file),ppc7400); \
	)
	$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * creating universal binaries in /usr/lib/mysql"
	$(foreach cur_file, $(USRLIBMYSQL_FILES_TO_LIPO), \
		$(SILENT) $(call sub_create_install_binary,$(OBJROOT)/$(MYSQL_STAGING_DIR),usr/lib/mysql,$(cur_file),ppc); \
	)
	$(SILENT) $(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * creating universal binaries in /usr/libexec"
	$(foreach cur_file, $(USRLIBEXEC_FILES_TO_LIPO), \
		$(SILENT) $(call sub_create_install_binary,$(OBJROOT)/$(MYSQL_STAGING_DIR),usr/libexec,$(cur_file),ppc7400); \
	)

install-config:
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * fixing mysql_config script"
	@$(ECHO) "#"
	$(SILENT) $(CP) $(FIRST_ARCH_STAGING)/usr/bin/mysql_config $(FIRST_ARCH_STAGING)/usr/bin/mysql_config-tmp
	$(SILENT) $(SED) < $(FIRST_ARCH_STAGING)/usr/bin/mysql_config-tmp > $(FIRST_ARCH_STAGING)/usr/bin/mysql_config \
		$(if $(FOUR_WAY_BUILD),-e 's%-arch $(FIRST_ARCH)%%' -e 's%-arch $(SECOND_ARCH)%%' -e 's%-arch $(THIRD_ARCH)%%' -e 's%-arch $(FOURTH_ARCH)%%', \
			$(if $(THREE_WAY_BUILD),-e 's%-arch $(FIRST_ARCH)%%' -e 's%-arch $(SECOND_ARCH)%%' -e 's%-arch $(THIRD_ARCH)%%', \
				$(if $(TWO_WAY_BUILD),-e 's%-arch $(FIRST_ARCH)%%' -e 's%-arch $(SECOND_ARCH)%%', \
					-e 's%-arch $(FIRST_ARCH)%%' \
				) \
			) \
		)
	$(SILENT) $(RM) -r -f $(FIRST_ARCH_STAGING)/usr/bin/mysql_config-tmp

install-copy:
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * copying staging dir to DSTROOT"
	@$(ECHO) "#"
	$(SILENT) $(CHOWN) -R root:wheel $(FIRST_ARCH_STAGING)/usr/
	$(SILENT) $(DITTO) $(FIRST_ARCH_STAGING) $(DSTROOT)

install-patch-test:
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] * patching mysql-test script sources"
	@$(ECHO) "#"
	$(SILENT) $(CD) $(DSTROOT)/usr/share/mysql/mysql-test/lib; $(PATCH) -u mtr_timer.pl $(APPLE_PATCH_BASE_DIR)/mtr_timer_pl.patch

install-finish:
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	@$(ECHO) "### The latest information about MySQL is available"
	@$(ECHO) "### on the web at http://www.mysql.org."
	@$(ECHO) "###"
	@$(ECHO) "### Use Server Admin or /usr/sbin/serveradmin to start MySQL. The "
	@$(ECHO) "### MySQL database will be automatically initialized at service start."
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] BUILD COMPLETE."
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="

install: build install-do-all

#================================================================================
#
# STANDARD BUILD TARGETS
#
#================================================================================

installsrc:
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] Installing sources in $(SRCROOT)..."
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	$(SILENT) -$(RM) -rf $(SRCROOT)
	$(SILENT) $(MKDIRS) $(SRCROOT)
	$(SILENT) $(CP) -R $(FILES_TO_INSTALL) $(SRCROOT)

installhdrs:
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] Install headers in $(SRCROOT)..."
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] $(PROJECT_NAME) has no headers to install."

clean:
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	@$(ECHO) "# `date +%Y/%m/%d\ %H:%M:%S` [MySQL] Cleaning project..."
	@$(ECHO) "#"
	@$(ECHO) "# = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ="
	@$(ECHO) "#"
	$(SILENT) $(RM) -rf $(MYSQL_BASE_DIR) mysql

#================================================================================
#
# DIRECTORY TARGETS
#
#================================================================================

$(BUILD_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT):
	$(SILENT) $(MKDIRS) $@

$(OBJROOT):
	$(SILENT) $(MKDIRS) $@

$(SYMROOT):
	$(SILENT) $(MKDIRS) $@

