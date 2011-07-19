#
# Copyright (c) 2008-2010 Apple Inc.  All Rights Reserved.
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

MODULE_NAME = mod_python
MODULE_SRC = $(MODULE_NAME).c
MODULE = $(MODULE_NAME).so
VERSION = 3.3.1
DISTRO_FILE = $(MODULE_NAME)-$(VERSION).tgz
MODULE_DIR = $(MODULE_NAME)-$(VERSION)
PATCHFILES = connobject.c.diff Makefile.in.diff
OTHER_SRC = apache_$(MODULE_NAME).txt apache_$(MODULE_NAME).plist $(PATCHFILES)
HEADERS =
APXS=/usr/sbin/apxs
SRCFILES = Makefile $(DISTRO_FILE) $(OTHER_SRC) $(HEADERS)
INSTALLDIR := $(shell $(APXS) -q LIBEXECDIR)
VERSIONS_DIR    = /usr/local/OpenSourceVersions
LICENSE_DIR     = /usr/local/OpenSourceLicenses
INSTALL = /usr/bin/install
PYTHON = /usr/bin/python
SITE_PACKAGES_LIB_DIR = /Library/Python/2.7/site-packages
SITE_PACKAGES_SYS_DIR = $(shell $(PYTHON) -c 'import sys; print sys.prefix')/Extras/lib/python
#/System/Library/Frameworks/Python.framework/Versions/2.7/Extras/lib/python/mod_python
EGG_THING = mod_python-3.3.1-py2.7.egg-info

MORE_FLAGS += -Wc,"$(RC_CFLAGS) -Wall -W -g"

MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
include $(MAKEFILEDIR)/platform.make
include $(MAKEFILEDIR)/commands-$(OS).make

all build $(MODULE): $(MODULE_DIR)/Makefile 
	$(SILENT) $(CD) $(MODULE_DIR);\
	make

$(MODULE_DIR)/Makefile: $(MODULE_DIR)/configure
	$(SILENT) $(CD) $(MODULE_DIR);\
	CXX=gcc \
	LDFLAGS="$$RC_CFLAGS" \
	CFLAGS="$$RC_CFLAGS" \
	./configure --with-apxs=$(APXS) 

 
installsrc: $(MODULE_DISTRO)
	@echo "Installing source files..."
	-$(RM) -rf $(SRCROOT)$(SRCPATH)
	$(MKDIRS) $(SRCROOT)$(SRCPATH)
	$(TAR) cf - $(SRCFILES) | (cd $(SRCROOT)$(SRCPATH) && $(TAR) xf -)

$(MODULE_DIR)/configure:
	$(SILENT) $(TAR) -xzf $(DISTRO_FILE)
	cd $(MODULE_DIR) && for patchfile in $(PATCHFILES); do \
		patch -p0 < ../$$patchfile; \
	done

installhdrs:
	@echo "Installing header files..."

install: $(MODULE) $(DSTROOT)/$(LICENSE_DIR) $(DSTROOT)/$(VERSIONS_DIR) $(SYMROOT)$(INSTALLDIR) $(DSTROOT)$(INSTALLDIR) $(DSTROOT)/$(SITE_PACKAGES_SYS_DIR)
	@echo "Installing site-package..."
	cd $(MODULE_DIR) && DESTDIR=$(DSTROOT) make install_py_lib
	mv $(DSTROOT)/$(SITE_PACKAGES_LIB_DIR)/mod_python $(DSTROOT)/$(SITE_PACKAGES_SYS_DIR)
	mv $(DSTROOT)/$(SITE_PACKAGES_LIB_DIR)/$(EGG_THING) $(DSTROOT)/$(SITE_PACKAGES_SYS_DIR)
	rm -rf $(DSTROOT)/Library
	$(STRIP) -x $(DSTROOT)/$(SITE_PACKAGES_SYS_DIR)/mod_python/_psp.so
	@echo "Installing Apache 2.2 module..."
	$(CP) $(MODULE_DIR)/src/$(MODULE) $(SYMROOT)$(INSTALLDIR)
	$(CHMOD) 755 $(SYMROOT)$(INSTALLDIR)/$(MODULE)
	$(STRIP) -x $(SYMROOT)$(INSTALLDIR)/$(MODULE) -o $(DSTROOT)$(INSTALLDIR)/$(MODULE)
	$(INSTALL) apache_$(MODULE_NAME).txt $(DSTROOT)/$(LICENSE_DIR)
	$(INSTALL) apache_$(MODULE_NAME).plist $(DSTROOT)/$(VERSIONS_DIR)


clean:
	@echo "== Cleaning $(MODULE_NAME) =="
	-$(RM) -r -f .libs *.loT *.la *.lo *.slo *.o $(MODULE_DIR)

$(SYMROOT)$(INSTALLDIR):
	$(SILENT) $(MKDIRS) -m 755 $@

$(DSTROOT)$(INSTALLDIR):
	$(SILENT) $(MKDIRS) -m 755 $@

$(DSTROOT)/$(LICENSE_DIR):
	$(SILENT) $(MKDIRS) -m 755 $@

$(DSTROOT)/$(VERSIONS_DIR):
	$(SILENT) $(MKDIRS) -m 755 $@

$(DSTROOT)/$(SITE_PACKAGES_SYS_DIR):
	$(SILENT) $(MKDIRS) -m 755 $@


