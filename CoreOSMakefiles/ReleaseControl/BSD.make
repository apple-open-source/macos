##
# Makefile for Apple Release Control (BSD projects)
#
# Wilfredo Sanchez | wsanchez@apple.com
# Copyright (c) 1997-1999 Apple Computer, Inc.
#
# @APPLE_LICENSE_HEADER_START@
# 
# Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
# Reserved.  This file contains Original Code and/or Modifications of
# Original Code as defined in and that are subject to the Apple Public
# Source License Version 1.1 (the "License").  You may not use this file
# except in compliance with the License.  Please obtain a copy of the
# License at http://www.apple.com/publicsource and read it before using
# this file.
# 
# The Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##

ifndef CoreOSMakefiles
CoreOSMakefiles = $(MAKEFILEPATH)/CoreOS
endif

include $(CoreOSMakefiles)/ReleaseControl/Common.make

##
# My variables
##

CC_Debug += -Wall

Extra_CC_Flags += -no-cpp-precomp

Environment = ARCH_FLAGS="$(CC_Archs)"	\
	           COPTS="$(CFLAGS)"	\
	      MAKEOBJDIR="$(OBJROOT)"	\
	      $(Extra_Environment)

ifndef BSD_Executable_Path
BSD_Executable_Path = $(USRBINDIR)
endif

Extra_Environment += BINDIR="$(BSD_Executable_Path)"

Install_Environment = DESTDIR=$(DSTROOT)		\
		      $(Extra_Install_Environment)

Install_Target = install

##
# Targets
##

.PHONY: bsd_install

BSD_Install_Targets = $(BSD_Before_Install) BSD_install_dirs BSD_install BSD_clean_dirs $(BSD_After_Install) compress_man_pages

install:: $(BSD_Install_Targets)

BSD_install_dirs::
	$(_v) $(MKDIR) $(DSTROOT)
	$(_v) mtree -f $(CoreOSMakefiles)/ReleaseControl/mtree/Darwin.root.dist     -U -p $(DSTROOT)
	$(_v) mtree -f $(CoreOSMakefiles)/ReleaseControl/mtree/Darwin.usr.dist      -U -p $(DSTROOT)$(USRDIR)
	$(_v) mtree -f $(CoreOSMakefiles)/ReleaseControl/mtree/Darwin.var.dist      -U -p $(DSTROOT)$(VARDIR)

BSD_install:: build
	@echo "Installing $(Project)..."
	$(_v) umask $(Install_Mask) ; $(Environment) $(BSDMAKE) $(Install_Environment) $(Install_Target)

BSD_clean_dirs::
	-$(_v) $(FIND) -d $(DSTROOT) -type d -exec rmdir "{}" \; 2> /dev/null

BSD_Build_Targets = $(BSD_Before_Build) BSD_build $(BSD_After_Build)

build:: $(BSD_Build_Targets)

BSD_build::
	@echo "Building $(Project)..."
	$(_v) $(MKDIR) $(OBJROOT)
	$(_v) $(Environment) $(BSDMAKE)

depend::
	@echo "Making dependancy file"
	$(_v) $(Environment) $(BSDMAKE) depend
