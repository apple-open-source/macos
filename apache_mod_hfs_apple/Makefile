# Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
MODULE = $(MODULE_NAME).so
OTHER_SRC =
HEADERS =
SRCFILES = Makefile $(MODULE_SRC) $(OTHER_SRC) $(HEADERS)
NEXTSTEP_INSTALLDIR := $(shell /usr/sbin/apxs -q LIBEXECDIR)

#MORE_FLAGS = -Wc,"-DDEBUG=1 -traditional-cpp -Wno-four-char-constants"

MAKEFILEDIR = $(MAKEFILEPATH)/pb_makefiles
include $(MAKEFILEDIR)/platform.make
include $(MAKEFILEDIR)/commands-$(OS).make

all build $(MODULE): $(MODULE_SRC) $(OTHER_SRC)
	ln -sf $(SRCROOT)$(SRCPATH)/Makefile $(OBJROOT)/Makefile
	ln -sf $(SRCROOT)$(SRCPATH)/mod_hfs_apple.c $(OBJROOT)/mod_hfs_apple.c
	cd $(OBJROOT) ; /usr/sbin/apxs -c -o $(OBJROOT)/$(MODULE) $(OBJROOT)/$(MODULE_SRC) $(OTHER_SRC)
	#$(CC) -DDARWIN -DUSE_HSREGEX -DUSE_EXPAT -I../lib/expat-lite -g -O3 -pipe -DHARD_SERVER_LIMIT=1024 -DEAPI -DSHARED_MODULE -I /usr/include/httpd -traditional-cpp -Wno-four-char-constants -F$(NEXT_ROOT)$(SYSTEM_LIBRARY_DIR)/PrivateFrameworks -c $(MODULE_SRC)
	#$(CC) -bundle -undefined error -o $(MODULE) $(MODULE_NAME).o -bundle_loader /usr/sbin/httpd
 
installsrc:
	@echo "Installing source files..."
	-$(RM) -rf $(SRCROOT)$(SRCPATH)
	$(MKDIRS) $(SRCROOT)$(SRCPATH)
	$(TAR) cf - $(SRCFILES) | (cd $(SRCROOT)$(SRCPATH) && $(TAR) xf -)

installhdrs:
	@echo "Installing header files..."

install: $(MODULE)
	@echo "Installing product..."
	$(MKDIRS) $(DSTROOT)$(NEXTSTEP_INSTALLDIR)
	$(CP) $(OBJROOT)/$(MODULE) $(DSTROOT)$(NEXTSTEP_INSTALLDIR)
	$(CHMOD) 755 $(DSTROOT)$(NEXTSTEP_INSTALLDIR)/$(MODULE)
	$(STRIP) -x $(DSTROOT)$(NEXTSTEP_INSTALLDIR)/$(MODULE)
	#/usr/sbin/apxs -i -a -n hfs_apple $(MODULE)

clean:
	@echo "== Cleaning $(MODULE_NAME) =="
	-$(RM) $(MODULE) *.o
