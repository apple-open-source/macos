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
# platform.make
#
# IMPORTANT NOTE:  In a normal full build, this file does not become the
# built platform.make.  Instead, the built platform.make gets generated
# under the control of the install-resources rule in Makefile.postamble.
# This file is here in this form because it gets included by Makefile.preamble
# when building the pb_makefiles project itself.

ARCH := $(shell arch)

ifeq "i386-nextpdo-winnt3.5" "$(ARCH)"
OS = WINDOWS
PLATFORM_OS = winnt
endif

ifeq "hppa1.1-nextpdo-hpux" "$(ARCH)"
OS = HPUX
PLATFORM_OS = hpux
endif

ifeq "hppa-hpux" "$(ARCH)"
OS = HPUX
PLATFORM_OS = hpux
endif

ifeq "sparc-nextpdo-solaris2" "$(ARCH)"
OS = SOLARIS
PLATFORM_OS = solaris
endif

ifeq "" "$(OS)"
ifeq "macos" "$(RC_OS)"
OS = MACOS
PLATFORM_OS = macos
else
OS = NEXTSTEP
PLATFORM_OS = nextstep
endif
endif

include $(MAKEFILEDIR)/platform-variables.make
