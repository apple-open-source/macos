#
# Copyright (c) 2001-2005 Apple Computer, Inc.
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

PROJECT_NAME	= MySQL
MYSQL_VERSION	= mysql-4.1.13a
BUILD_DIR	= /usr
STAGING_DIR 	:= $(shell mktemp -d /tmp/mysql-tmp-XXXXXX)
SHARE_DIR	= /usr/share
MYSQL_SHARE_DIR = $(SHARE_DIR)/mysql
VERSIONS_DIR=/usr/local/OpenSourceVersions
LICENSE_DIR=/usr/local/OpenSourceLicenses
LIBEXEC_DIR=/usr/libexec
INSTALL		=/usr/bin/install
DITTO		=/usr/bin/ditto
CHOWN		=/usr/sbin/chown
PATCH		=/usr/bin/patch
MKDIR		=/bin/mkdir

FILES		= $(MYSQL_VERSION).tar.gz mysqlman.1 Makefile \
MySQL.plist MySQL.txt config.h.sed applemysqlcheckcnf \
my-huge.cnf.patch my-large.cnf.patch mysqld_safe.patch \
applemysqlcheckcnf.8

FILES_TO_REMOVE = \
/usr/share/info/dir \
$(MYSQL_SHARE_DIR)/Info.plist \
$(MYSQL_SHARE_DIR)/Makefile \
$(MYSQL_SHARE_DIR)/ReadMe.txt \
$(MYSQL_SHARE_DIR)/StartupParameters.plist \
$(MYSQL_SHARE_DIR)/postinstall \
$(MYSQL_SHARE_DIR)/preinstall \
$(MYSQL_SHARE_DIR)/Description.plist \
$(MYSQL_SHARE_DIR)/make_win_src_distribution \
/usr/bin/make_win_binary_distribution \
/usr/bin/make_win_src_distribution

FILES_TO_LINK = \
client_test \
comp_err \
msql2mysql \
my_print_defaults \
myisam_ftdump \
myisamchk \
myisamlog \
myisampack \
mysql_client_test \
mysql_config \
mysql_convert_table_format \
mysql_create_system_tables \
mysql_explain_log \
mysql_find_rows \
mysql_fix_extensions \
mysql_install_db \
mysql_secure_installation \
mysql_setpermission \
mysql_tableinfo \
mysql_tzinfo_to_sql \
mysql_waitpid \
mysqlbinlog \
mysqlbug \
mysqlcheck \
mysqldumpslow \
mysqlhotcopy \
mysqlimport \
mysqlmanager \
mysqlmanager-pwgen \
mysqlmanagerc \
mysqltest \
pack_isam \
resolve_stack_dump \
resolveip 

default: build

clean:
	$(SILENT) $(ECHO) "Cleaning $(PROJECT_NAME)..."
	$(SILENT) -$(RM) -rf mysql

installhdrs:
	$(SILENT) $(ECHO) "$(PROJECT_NAME) has no headers to install in $(SRCROOT)..."

installsrc:
	$(SILENT) $(ECHO) "Installing $(PROJECT_NAME) sources in $(SRCROOT)..."
	$(SILENT) -$(RM) -rf $(SRCROOT)
	$(SILENT) $(MKDIRS) $(SRCROOT)
	$(SILENT) $(CP) $(FILES) $(SRCROOT)

mysql: $(OBJROOT)
	$(SILENT) -$(RM) -rf $(MYSQL_VERSION) mysql
	$(SILENT) $(TAR) -xzf $(MYSQL_VERSION).tar.gz
	$(SILENT) $(MV) $(MYSQL_VERSION) mysql

untar: mysql 

mysql/config.status: untar
	$(SILENT) $(ECHO) "Configuring mysql..."
	$(SILENT) $(CD) mysql;\
	CFLAGS="-O3 -fno-omit-frame-pointer $$RC_CFLAGS" \
	CXXFLAGS="-O3 -fno-omit-frame-pointer -felide-constructors -fno-exceptions -fno-rtti $$RC_CFLAGS" \
	LDFLAGS="$$RC_CFLAGS" \
	ac_cv_c_bigendian=yes \
	./configure --infodir=/usr/share/info \
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
	$(SILENT) $(ECHO) "Patching mysql/config.h..."
	$(SILENT) $(CD) mysql; $(SED) -f ../config.h.sed config.h > config.h.tmp; $(MV) config.h config.h.bak; $(MV) config.h.tmp config.h
	$(SILENT) $(ECHO) "Patching mysql/innobase/ib_config.h..."
	$(SILENT) $(CD) mysql/innobase; $(SED) -f ../../config.h.sed ib_config.h > ib_config.h.tmp; $(MV) ib_config.h ib_config.h.bak; $(MV) ib_config.h.tmp ib_config.h

configure: mysql/config.status

build: configure
	$(SILENT) $(ECHO) "Building mysql..."
	$(SILENT) $(CD) mysql;make
	$(SILENT) $(ECHO) "Patching mysql/support-files/my-huge.cnf..."
	$(SILENT) $(CD) mysql/support-files; $(PATCH) -u my-huge.cnf ../../my-huge.cnf.patch
	$(SILENT) $(ECHO) "Patching mysql/support-files/my-large.cnf..."
	$(SILENT) $(CD) mysql/support-files; $(PATCH) -u my-large.cnf ../../my-large.cnf.patch
	$(SILENT) $(ECHO) "Patching mysql/scripts/mysqld_safe..."
	$(SILENT) $(CD) mysql/scripts; $(PATCH) -u mysqld_safe ../../mysqld_safe.patch

# Must set DESTDIR to shadow directory
install: build $(STAGING_DIR)$(VERSIONS_DIR) $(STAGING_DIR)$(LICENSE_DIR) $(STAGING_DIR)$(LIBEXEC_DIR)
	$(SILENT) $(ECHO) "Installing mysql..."
	$(SILENT) $(CD) mysql;make install DESTDIR=$(STAGING_DIR)
	$(SILENT) $(CP) mysqlman.1 $(STAGING_DIR)/usr/share/man/man1
	$(SILENT) $(MKDIR) -p -m 755 $(STAGING_DIR)/usr/share/man/man8
	$(SILENT) $(CHOWN) root:wheel $(STAGING_DIR)/usr/share/man/man8
	$(SILENT) $(CP) applemysqlcheckcnf.8 $(STAGING_DIR)/usr/share/man/man8
	$(SILENT) $(INSTALL) -m 444 -o root -g wheel MySQL.plist $(STAGING_DIR)$(VERSIONS_DIR)
	$(SILENT) $(INSTALL) -m 444 -o root -g wheel MySQL.txt $(STAGING_DIR)$(LICENSE_DIR)
	$(SILENT) $(INSTALL) -m 755 -o root -g wheel applemysqlcheckcnf $(STAGING_DIR)$(LIBEXEC_DIR)
	$(SILENT) $(ECHO) "Fixing up $(PROJECT_NAME), staging from $(STAGING_DIR)..."
	$(SILENT) -$(MV) $(STAGING_DIR)/$(BUILD_DIR)/mysql-test $(STAGING_DIR)/$(MYSQL_SHARE_DIR)
	for i in $(FILES_TO_REMOVE); do \
		rm -r -f $(STAGING_DIR)/$$i; \
	done 
	$(SILENT) -$(STRIP) $(STAGING_DIR)/usr/libexec/* > /dev/null 2>&1
	$(SILENT) -$(STRIP) $(STAGING_DIR)/usr/lib/mysql/* > /dev/null 2>&1
	$(SILENT) -$(STRIP) $(STAGING_DIR)/usr/bin/* > /dev/null 2>&1
	for i in $(FILES_TO_LINK); do \
		ln  $(STAGING_DIR)/$(SHARE_DIR)/man/man1/mysqlman.1 $(STAGING_DIR)/$(SHARE_DIR)/man/man1/$$i.1; \
	done
	$(SILENT) $(CP) $(STAGING_DIR)/usr/bin/mysql_config $(STAGING_DIR)/usr/bin/mysql_config-tmp
	$(SILENT) $(SED) < $(STAGING_DIR)/usr/bin/mysql_config-tmp > $(STAGING_DIR)/usr/bin/mysql_config -e 's%-arch i386%%' -e 's%-arch ppc%%'
	$(SILENT) $(RM) -r -f $(STAGING_DIR)/usr/bin/mysql_config-tmp
	$(SILENT) $(CHOWN) -R root:wheel $(STAGING_DIR)/usr/
	$(SILENT) $(DITTO) $(STAGING_DIR) $(DSTROOT)
	$(SILENT) $(RM) -r -f $(STAGING_DIR)
	$(SILENT) $(ECHO) "# The latest information about MySQL is available on the web at http://www.mysql.com."
	$(SILENT) $(ECHO) "# Use MySQL Manager app to initialize MySQL database."

$(BUILD_DIR):
	$(SILENT) $(MKDIRS) $@

$(STAGING_DIR)$(BUILD_DIR):
	$(SILENT) $(MKDIRS) $@

$(STAGING_DIR)$(VERSIONS_DIR):
	$(SILENT) $(MKDIRS) $@

$(STAGING_DIR)$(LICENSE_DIR):
	$(SILENT) $(MKDIRS) $@

$(STAGING_DIR)$(LIBEXEC_DIR):
	$(SILENT) $(MKDIRS) $@

$(STAGING_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT):
	$(SILENT) $(MKDIRS) $@

$(OBJROOT):
	$(SILENT) $(MKDIRS) $@

