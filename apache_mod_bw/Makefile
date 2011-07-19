#
# Copyright (c) 2008 Apple Inc.  All Rights Reserved.
# 
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
#
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
#

MODULE_NAME = mod_bw
MODULE_SRC = $(MODULE_NAME).c
MODULE = $(MODULE_NAME).so
MODULE_DIR = mod_bw
VERSION=0.8
DISTRO_FILE = $(MODULE_NAME)-$(VERSION).tgz
OTHER_SRC = apache_mod_bw.plist apache_mod_bw.txt $(DISTRO_FILE)
HEADERS =
APXS=/usr/sbin/apxs
SRCFILES = Makefile $(OTHER_SRC) $(HEADERS)
INSTALL=/usr/bin/install
INSTALLDIR := $(shell $(APXS) -q LIBEXECDIR)
VERSIONS_DIR=/usr/local/OpenSourceVersions
LICENSE_DIR=/usr/local/OpenSourceLicenses

MORE_FLAGS += -Wc,"$(RC_CFLAGS) -Wall -W -g"
MORE_FLAGS += -Wl,"$(RC_CFLAGS)"

MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
include $(MAKEFILEDIR)/platform.make
include $(MAKEFILEDIR)/commands-$(OS).make

all build $(MODULE): $(MODULE_SRC) $(OTHER_SRC)
	$(APXS) -c $(MORE_FLAGS) -o $(MODULE) $(MODULE_SRC) $(OTHER_SRC)

$(MODULE_SRC): 
	$(SILENT) $(TAR) -xzf $(DISTRO_FILE)
	$(SILENT) $(CP) $(MODULE_DIR)/$(MODULE_SRC) .

installsrc:
	@echo "Installing source files..."
	-$(RM) -rf $(SRCROOT)$(SRCPATH)
	$(MKDIRS) $(SRCROOT)$(SRCPATH)
	$(TAR) cf - $(SRCFILES) | (cd $(SRCROOT)$(SRCPATH) && $(TAR) xf -)

installhdrs:
	@echo "Installing header files..."

install: $(MODULE) $(DSTROOT)$(INSTALLDIR) $(DSTROOT)$(LIBEXECDIR) $(DSTROOT)$(VERSIONS_DIR) $(DSTROOT)$(LICENSE_DIR)
	@echo "Installing Apache 2.2 module..."
	$(MKDIRS) $(SYMROOT)$(INSTALLDIR)
	$(CP) .libs/$(MODULE) $(SYMROOT)$(INSTALLDIR)
	$(CHMOD) 755 $(SYMROOT)$(INSTALLDIR)/$(MODULE)
	$(STRIP) -x $(SYMROOT)$(INSTALLDIR)/$(MODULE) -o $(DSTROOT)$(INSTALLDIR)/$(MODULE)
	$(INSTALL) -m 444 -o root -g wheel apache_mod_bw.plist $(DSTROOT)$(VERSIONS_DIR)
	$(INSTALL) -m 444 -o root -g wheel apache_mod_bw.txt $(DSTROOT)$(LICENSE_DIR)

clean:
	@echo "== Cleaning $(MODULE_NAME) =="
	-$(RM) -r -f .libs *.o *.lo *.slo *.la $(MODULE_SRC) $(MODULE_DIR)

$(DSTROOT)$(INSTALLDIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(VERSIONS_DIR):
	$(SILENT) $(MKDIRS) $@

$(DSTROOT)$(LICENSE_DIR):
	$(SILENT) $(MKDIRS) $@

