##
# Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# The contents of this file constitute Original Code as defined in and
# are subject to the Apple Public Source License Version 1.1 (the
# "License").  You may not use this file except in compliance with the
# License.  Please obtain a copy of the License at
# http://www.apple.com/publicsource and read it before using this file.
# 
# This Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
#
# reinstall.make
#
# Rules for rebuilding an installed set of products for Windows, were we can
# not strip binaries after they are linked.  Instead, we clone all the object
# files and strip them individually, creating a new set of directories to link from.
#


STRIPNAME = STRIPPED
GARBAGE += $(OBJROOT)/$(STRIPNAME) $(SYMROOT)/$(STRIPNAME)

ifeq "$(REINSTALLING)" "YES"
ifneq "$(STRIP_ON_INSTALL)" "NO"

STRIPO = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Libraries/gcc-lib/$(ARCH)/StabsToCodeview.exe
STRIPDIRS = $(OBJROOT)/$(STRIPNAME) $(SYMROOT)/$(STRIPNAME)

CLONE_AND_STRIP = $(FIND) . '(' -name $(STRIPNAME) -prune ')' \
  -o '(' -name lastbuildtimes -prune ')' \
  -o '(' -type d -exec $(MKDIRS) $(STRIPNAME)/'{}' ';' ')' \
  -o '(' -name '*.exe' -o -name '*.dll' -o -name '*.lib' -name 'lib*.a' ')' \
  -o '(' -name '*.EXE' -o -name '*.DLL' -o -name '*.LIB' -name 'LIB*.A' ')' \
  -o '(' -name '*.ofileList' -o -name '*.ofilelist' ')' \
  -o '(' -name '*.o' -exec $(STRIPO) -g0 '{}' -o $(STRIPNAME)/'{}' ';' ')' \
  -o -exec $(CP) -p '{}' $(STRIPNAME)/'{}' ';'

.PHONY: clone_and_strip reinstall-stripped

clone_and_strip:
	$(MKDIRS) $(OBJROOT)/$(STRIPNAME) $(SYMROOT)/$(STRIPNAME)
	cd $(OBJROOT) && $(CLONE_AND_STRIP)
	cd $(SYMROOT) && $(CLONE_AND_STRIP)

reinstall-stripped: clone_and_strip 
	$(SILENT)if $(ECHO) $(OBJROOT) | $(GREP) -v $(STRIPNAME) $(BURY_STDERR) ; then \
	   cmd='$(MAKE) install OBJROOT=$(OBJROOT)/$(STRIPNAME) SYMROOT=$(SYMROOT)/$(STRIPNAME) DEBUG_SYMBOLS_CFLAG= SKIP_EXPORTING_HEADERS=YES' ; \
	   $(ECHO) $$cmd ; $$cmd ; \
	fi
endif
endif


