#
# Copyright (c) 2001-2003 Apple Computer, Inc.
#
# Starting with MySQL 3.23.54, the source patch to handle installation
# directories with tildes no longer works. Going forward, this Makefile
# takes the simpler approach of installing into a staging directory in /tmp 
# and then dittoing that into DSTROOT.
#
# This Makefile applies patches in configure.patch and configure.in.patch 
# that are only valid for MySQL 4.0.18.
# The patch for config.h.in is needed regardless of MySQL version; it
# makes MySQL generate correct code for PPC when building fat.
#

# These includes provide the proper paths to system utilities

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

PROJECT_NAME	= MySQL
MYSQL_VERSION	= mysql-4.0.18
BUILD_DIR	= /usr
STAGING_DIR 	:= $(shell mktemp -d /tmp/mysql-tmp-XXXXXX)
SHARE_DIR	= /usr/share
MYSQL_SHARE_DIR = $(SHARE_DIR)/mysql
DITTO		= /usr/bin/ditto
FILES_TO_REMOVE = info/dir mysql/Info.plist mysql/Makefile mysql/ReadMe.txt mysql/StartupParameters.plist mysql/postinstall mysql/preinstall mysql/Description.plist

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
	$(SILENT) $(CP) Makefile $(SRCROOT)
	$(SILENT) $(CP) $(MYSQL_VERSION).tar.gz $(SRCROOT)
	$(SILENT) $(CP) configure.patch configure.in.patch config.h.in.patch $(SRCROOT)

mysql: $(OBJROOT)
	$(SILENT) -$(RM) -rf $(MYSQL_VERSION) mysql
	$(SILENT) $(TAR) -xzf $(MYSQL_VERSION).tar.gz
	$(SILENT) $(MV) $(MYSQL_VERSION) mysql

untar: mysql 

mysql/config.status: untar
	$(SILENT) $(ECHO) "Patching configure script..."
	$(SILENT) $(CD) mysql; patch -i ../configure.patch
	$(SILENT) $(CD) mysql; patch -i ../configure.in.patch
	$(SILENT) $(CD) mysql; patch -i ../config.h.in.patch
	$(SILENT) $(ECHO) "Configuring mysql..."
	$(SILENT) $(CD) mysql;\
	CFLAGS="-O3 -fno-omit-frame-pointer $$RC_CFLAGS" \
	CXX=gcc \
	CXXFLAGS="-O3 -fno-omit-frame-pointer -felide-constructors -fno-exceptions -fno-rtti $$RC_CFLAGS" \
	LDFLAGS="$$RC_CFLAGS" \
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
		--prefix=$(BUILD_DIR)

configure: mysql/config.status

build: configure
	$(SILENT) $(ECHO) "Building mysql..."
	$(SILENT) $(CD) mysql;make

# Must set DESTDIR to shadow directory
install: build
	$(SILENT) $(ECHO) "Installing mysql..."
	$(SILENT) $(CD) mysql;make install DESTDIR=$(STAGING_DIR)
	$(SILENT) $(ECHO) "Fixing up $(PROJECT_NAME), staging from $(STAGING_DIR)..."
	$(SILENT) -$(MV) $(STAGING_DIR)/$(BUILD_DIR)/mysql-test $(STAGING_DIR)/$(MYSQL_SHARE_DIR)
	for i in $(FILES_TO_REMOVE); do \
		rm -r -f $(STAGING_DIR)/$(SHARE_DIR)/$$i; \
	done 
	$(SILENT) -$(STRIP) $(STAGING_DIR)/usr/libexec/* > /dev/null 2>&1
	$(SILENT) -$(STRIP) $(STAGING_DIR)/usr/lib/mysql/* > /dev/null 2>&1
	$(SILENT) -$(STRIP) $(STAGING_DIR)/usr/bin/* > /dev/null 2>&1
	$(SILENT) $(DITTO) $(STAGING_DIR) $(DSTROOT)
	$(SILENT) $(RM) -r -f $(STAGING_DIR)
	$(SILENT) $(ECHO) "# The latest information about MySQL is available on the web at http://www.mysql.com."
	$(SILENT) $(ECHO) "# Use MySQL Manager app to initialize MySQL database."

$(BUILD_DIR):
	$(SILENT) $(MKDIRS) $@

$(STAGING_DIR)$(BUILD_DIR):
	$(SILENT) $(MKDIRS) $@

$(STAGING_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT):
	$(SILENT) $(MKDIRS) $@

$(OBJROOT):
	$(SILENT) $(MKDIRS) $@

