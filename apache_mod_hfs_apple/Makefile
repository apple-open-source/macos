# Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
# The contents of this file constitute Original Code as defined in and are 
# subject to the Apple Public Source License Version 1.2 (the 'License'). You 
# may not use this file except in compliance with the License. Please obtain a 
# copy of the License at http://www.apple.com/publicsource and read it before 
# using this file.
# This Original Code and all software distributed under the License are 
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS 
# OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT 
# LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, 
# QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the specific 
# language governing rights and limitations under the License.


MODULE_NAME = mod_hfs_apple
MODULE_SRC = $(MODULE_NAME).c
MODULE_SRC2 = $(MODULE_NAME)2.c
MODULE = $(MODULE_NAME).so
OTHER_SRC = 
HEADERS =
APXS2=/usr/sbin/apxs
APXS1=/usr/sbin/apxs-1.3
SRCFILES = Makefile $(MODULE_SRC) $(MODULE_SRC2) $(OTHER_SRC) $(HEADERS)
INSTALLDIR1 := $(shell $(APXS1) -q LIBEXECDIR)
INSTALLDIR2 := $(shell $(APXS2) -q LIBEXECDIR)

export MACOSX_DEPLOYMENT_TARGET=10.4
MORE_FLAGS = -DUSE_CHKUSRNAMPASSWD=1 
MORE_FLAGS += -Wc,"$(RC_CFLAGS) -Wall -W -g"
MORE_FLAGS += -Wl,"$(RC_CFLAGS)"

MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
include $(MAKEFILEDIR)/platform.make
include $(MAKEFILEDIR)/commands-$(OS).make

all build $(MODULE): $(MODULE_SRC) $(OTHER_SRC)
	$(APXS1) -c $(MORE_FLAGS) -o $(MODULE) $(MODULE_SRC) $(OTHER_SRC)
	$(APXS2) -c $(MORE_FLAGS) -o $(MODULE) $(MODULE_SRC2) $(OTHER_SRC)
 
installsrc:
	@echo "Installing source files..."
	-$(RM) -rf $(SRCROOT)$(SRCPATH)
	$(MKDIRS) $(SRCROOT)$(SRCPATH)
	$(TAR) cf - $(SRCFILES) | (cd $(SRCROOT)$(SRCPATH) && $(TAR) xf -)

installhdrs:
	@echo "Installing header files..."

install: $(MODULE)
	@echo "Installing Apache 1.3 module..."
	$(MKDIRS) $(SYMROOT)$(INSTALLDIR1)
	$(CP) $(MODULE) $(SYMROOT)$(INSTALLDIR1)
	$(CHMOD) 755 $(SYMROOT)$(INSTALLDIR1)/$(MODULE)
	$(MKDIRS) $(DSTROOT)$(INSTALLDIR1)
	$(STRIP) -x $(SYMROOT)$(INSTALLDIR1)/$(MODULE) -o $(DSTROOT)$(INSTALLDIR1)/$(MODULE)
	@echo "Installing Apache 2.2 module..."
	$(MKDIRS) $(SYMROOT)$(INSTALLDIR2)
	$(CP) .libs/$(MODULE) $(SYMROOT)$(INSTALLDIR2)
	$(CHMOD) 755 $(SYMROOT)$(INSTALLDIR2)/$(MODULE)
	$(MKDIRS) $(DSTROOT)$(INSTALLDIR2)
	$(STRIP) -x $(SYMROOT)$(INSTALLDIR2)/$(MODULE) -o $(DSTROOT)$(INSTALLDIR2)/$(MODULE)

clean:
	@echo "== Cleaning $(MODULE_NAME) =="
	-$(RM) -r -f .libs $(MODULE_NAME).la $(MODULE_NAME).lo $(MODULE_NAME).slo $(MODULE_NAME).o $(MODULE)

