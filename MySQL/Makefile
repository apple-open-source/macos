#
# Legacy Makefile created by JFA
# Copyright (c) 2001 by Apple Computer, Inc.
#
# Installation occurs in /usr. Build
# and install targets do the same work! 
#
#

PROJECT_NAME = MySQL

MYSQL_VERSION=mysql-3.23.51

# These includes provide the proper paths to system utilities

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

# Set up our variables

BUILD_DIR=/usr
SOURCE1_TO_PATCH=mysql/configure.in
SOURCE2_TO_PATCH=mysql/ltconfig
SOURCE3_TO_PATCH=mysql/ltmain.sh
SHARE_DIR=/usr/share
MYSQL_SHARE_DIR=$(SHARE_DIR)/mysql
TMP_FILE=$(OBJROOT)/tmp-file
SED=sed
SILENT=@
GNUTAR=gnutar
AUTOCONF=/usr/bin/autoconf

# Build rules

all build $(PROJECT_NAME):: do_clean do_untar do_prepare do_build

install:: do_clean do_untar do_prepare do_install do_deploy

clean:: do_clean

installhdrs:: do_installhdrs

installsrc:: do_installsrc

do_untar:
	$(SILENT) if [ ! -e mysql/README ]; then\
		$(GNUTAR) -xzf $(MYSQL_VERSION).tar.gz;\
		$(MV) $(MYSQL_VERSION) mysql;\
	fi

do_prepare:
	$(SILENT) $(ECHO) "Patching $(SOURCE1_TO_PATCH) with curses fix"
	$(SILENT) \
		$(CP) $(SOURCE1_TO_PATCH) $(SOURCE1_TO_PATCH).bak;\
		$(SED) <$(SOURCE1_TO_PATCH) >$(TMP_FILE) \
			-e 's%with_named_curses=""%true%g';\
		$(MV) $(TMP_FILE) $(SOURCE1_TO_PATCH)
	$(SILENT) $(ECHO) "Patching $(SOURCE2_TO_PATCH) with tilde fix"
	$(SILENT) \
		$(CP) $(SOURCE2_TO_PATCH) $(SOURCE2_TO_PATCH).bak;\
		$(SED) <$(SOURCE2_TO_PATCH) >$(TMP_FILE) \
			-e 's%~%^%g';\
		$(MV) $(TMP_FILE) $(SOURCE2_TO_PATCH)
	$(SILENT) $(ECHO) "Patching $(SOURCE3_TO_PATCH) with tilde fix"
	$(SILENT) \
		$(CP) $(SOURCE3_TO_PATCH) $(SOURCE3_TO_PATCH).bak;\
		$(SED) <$(SOURCE3_TO_PATCH) >$(TMP_FILE) \
			-e "s%IFS='~'%IFS='^'%g";\
		$(MV) $(TMP_FILE) $(SOURCE3_TO_PATCH)
	$(SILENT) $(ECHO) "Configuring mysql..."
	$(SILENT) if [ ! -e mysql/Makefile ]; then\
		$(CD) mysql;\
		$(AUTOCONF);\
		CFLAGS="-Os" CXXFLAGS="-Os" \
		./configure --infodir=/usr/share/info \
			--mandir=/usr/share/man \
			--localstatedir=/var/mysql  \
			--sysconfdir=/etc \
			--with-mysqld-user=mysql \
			--without-bench \
			--with-named-curses-libs=/usr/lib/libcurses.dylib \
			--with-low-memory \
			--without-debug \
			--with-lib-ccflags=-Os \
			--prefix=$(BUILD_DIR);\
	fi

do_build: $(DSTROOT)
	$(SILENT) $(ECHO) "Building mysql..."
	$(SILENT) $(CD) mysql;make

# Must set DEST_DIR=$(DSTROOT) to build in shadow directory (whitmore)
do_install: $(DSTROOT)
	$(SILENT) $(ECHO) "Installing mysql..."
	$(SILENT) $(CD) mysql;make;make install-strip DESTDIR=$(DSTROOT)
	$(SILENT) $(ECHO) "# The latest information about MySQL is available on the web at http://www.mysql.com"
	$(SILENT) $(ECHO) "# Run mysql_install_db --user=mysql and then safe_mysqld &"

do_installhdrs:
	$(SILENT) $(ECHO) "Installing $(PROJECT_NAME) headers in $(SRCROOT)..."

do_installsrc:
	$(SILENT) $(ECHO) "Installing $(PROJECT_NAME) sources in $(SRCROOT)..."
	$(SILENT) -$(RM) -rf $(SRCROOT)
	$(SILENT) $(MKDIRS) $(SRCROOT)
	$(SILENT) $(CP) Makefile $(SRCROOT)
	$(SILENT) $(CP) $(MYSQL_VERSION).tar.gz $(SRCROOT)

do_clean:
	$(SILENT) $(ECHO) "Cleaning $(PROJECT_NAME)..."
	$(SILENT) -$(RM) -rf mysql

do_deploy:
	$(SILENT) $(ECHO) "Fixing up $(PROJECT_NAME)..."
	$(SILENT) $(RM) $(DSTROOT)/$(SHARE_DIR)/info/dir
	$(SILENT) $(MV) $(DSTROOT)/$(BUILD_DIR)/mysql-test $(DSTROOT)/$(MYSQL_SHARE_DIR)

$(BUILD_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(BUILD_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT):
	$(SILENT) $(MKDIRS) $@


