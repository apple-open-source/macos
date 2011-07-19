#
# Copyright (c) 2005-2006 Apple Computer, Inc.
# Wrapper Makefile for mod_encoding
#

# These includes provide the proper paths to system utilities

include $(MAKEFILEPATH)/pb_makefiles/platform.make
include $(MAKEFILEPATH)/pb_makefiles/commands-$(OS).make

PROJECT_NAME	= apache_mod_encoding
MOD_ENCODING_TGZ= mod_encoding-20021209.tar.gz
SRC_DIR		= mod_encoding-20021209
VERSIONS_DIR	= /usr/local/OpenSourceVersions
LICENSE_DIR	= /usr/local/OpenSourceLicenses
APXS		= /usr/sbin/apxs
MODULE_DIR      = $(shell $(APXS) -q LIBEXECDIR)
BIN_FILE	= mod_encoding.so
SRC_FILE	= mod_encoding.c
#SRC_UPDATE_FILE = mod_encoding.c.apache2.20040616
SRC_UPDATE_FILE = mod_encoding.c-patched
PATCH		= /usr/bin/patch
MORE_FLAGS = -Wc,"$(RC_CFLAGS) -I lib -g -Wall -W -Wconversion"
#MORE_FLAGS += -Wl,"$(RC_CFLAGS) -liconv lib/.libs/libiconv_hook.a"
MORE_FLAGS += -Wl,"$(RC_CFLAGS) -liconv lib/iconv_hook.o lib/iconv_hook_default.o lib/iconv_hook_eucjp.o lib/iconv_hook_ja_auto.o lib/iconv_hook_mssjis.o lib/iconv_hook_ucs2_cp932.o lib/iconv_hook_utf8_cp932.o lib/iconv_hook_utf8_eucjp.o lib/identify_encoding.o"

SRC_FILES 	= $(MOD_ENCODING_TGZ) Makefile apache_mod_encoding2.plist apache_mod_encoding2.txt $(SRC_UPDATE_FILE)


build all default: $(SRC_DIR)/$(BIN_FILE)

clean:
	$(SILENT) $(ECHO) "Cleaning..."
	$(SILENT) $(RM) -r -f $(SRC_DIR) 

installhdrs:
	$(SILENT) $(ECHO) "$(PROJECT_NAME) has no headers to install in $(SRCROOT)..."

installsrc:
	$(SILENT) $(ECHO) "Installing $(PROJECT_NAME) sources in $(SRCROOT)..."
	$(SILENT) -$(RM) -rf $(SRCROOT)
	$(SILENT) $(MKDIRS) $(SRCROOT)
	$(SILENT) $(CP) -r $(SRC_FILES) $(SRCROOT)

untar: $(OBJROOT)
	$(SILENT) -$(RM) -rf $(SRC_DIR) 
	$(SILENT) $(TAR) -xzf $(MOD_ENCODING_TGZ)
	$(SILENT) $(CD) $(SRC_DIR); $(MV) $(SRC_FILE) $(SRC_FILE)-orig
	$(SILENT) $(CD) $(SRC_DIR); $(CP) ../$(SRC_UPDATE_FILE) $(SRC_FILE)

$(SRC_DIR)/$(SRC_FILE): untar

$(SRC_DIR)/$(BIN_FILE): $(SRC_DIR)/$(SRC_FILE)
	$(SILENT) $(ECHO) "Building module..."
	$(SILENT) $(CD) $(SRC_DIR)/lib; LDFLAGS="$$RC_CFLAGS -Os" \
			CFLAGS="$$RC_CFLAGS -Os" \
			./configure --prefix=/usr; make
#	$(SILENT) $(CD) $(SRC_DIR); ./configure --with-iconv-hook=lib --with-apxs=$(APXS); make
	$(SILENT) $(CD) $(SRC_DIR); $(APXS) -c $(MORE_FLAGS) $(SRC_FILE)

install: build $(SRC_DIR)/$(BIN_FILE) $(DSTROOT)$(NEXTSTEP_INSTALLDIR)/$(MODULE_DIR) $(DSTROOT)$(NEXTSTEP_INSTALLDIR)$(VERSIONS_DIR) $(DSTROOT)$(NEXTSTEP_INSTALLDIR)$(LICENSE_DIR)
	$(SILENT) $(ECHO) "Installing module..."
	$(SILENT) $(MKDIRS) $(SYMROOT)$(NEXTSTEP_INSTALLDIR)
	$(SILENT) $(CP) $(SRC_DIR)/.libs/$(BIN_FILE) $(SYMROOT)$(NEXTSTEP_INSTALLDIR)
	$(SILENT) $(CHMOD) 755 $(SYMROOT)$(NEXTSTEP_INSTALLDIR)/$(BIN_FILE)
	$(SILENT) $(MKDIRS) $(DSTROOT)$(NEXTSTEP_INSTALLDIR)
	$(SILENT) $(STRIP) -x $(SYMROOT)$(NEXTSTEP_INSTALLDIR)/$(BIN_FILE) -o $(DSTROOT)$(NEXTSTEP_INSTALLDIR)/$(MODULE_DIR)/$(BIN_FILE)
	$(SILENT) $(CP) apache_mod_encoding2.plist $(DSTROOT)$(NEXTSTEP_INSTALLDIR)$(VERSIONS_DIR)
	$(SILENT) $(CP) apache_mod_encoding2.txt $(DSTROOT)$(NEXTSTEP_INSTALLDIR)$(LICENSE_DIR)

$(DSTROOT)$(NEXTSTEP_INSTALLDIR)/$(MODULE_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT):
	$(SILENT) $(MKDIRS) $@

$(OBJROOT):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(NEXTSTEP_INSTALLDIR)$(VERSIONS_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(NEXTSTEP_INSTALLDIR)$(LICENSE_DIR):
	$(SILENT) $(MKDIRS) $@
