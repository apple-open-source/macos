##
# Implicit rules
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

##
# Variables
##

# CC Options
CC_Archs    = 
CC_Debug    = -g
ifeq ($(DEBUG),YES)
CC_Optimize = 
LD_Prebind  = 
else
CC_Optimize = -Os
endif
CC_Other    = -pipe

# OpenStep Frameworks
Frameworks = $(Extra_Frameworks)

# C Preprocessor Options
CPP_Defines  = $(Extra_CPP_Defines)
CPP_Includes = $(Extra_CPP_Includes)

# Linker Options
LD_Libraries = $(Extra_LD_Libraries)

# CC/CPP/LD Flags
CPP_Flags = $(CPP_Defines)            $(CPP_Includes)          $(Extra_CPP_Flags)
LD_Flags  = $(CC_Archs) $(Frameworks) $(LD_Libraries)          $(Extra_LD_Flags)
CC_Flags  = $(CC_Archs) $(CC_Debug) $(CC_Optimize) $(CC_Other) $(Extra_CC_Flags)
Cxx_Flags = $(CC_Archs) $(CC_Debug) $(CC_Optimize) $(CC_Other) $(Extra_Cxx_Flags)

# This is for compatibility with standard implicit rules
CPPFLAGS = $(CPP_Flags)
CFLAGS   = $(CC_Flags)
CXXFLAGS = $(Cxx_Flags)
LDFLAGS  = $(LD_Flags)

##
# Targets
##

# C / Objective-C / C++ Source

%.o: %.c
	@echo "Compiling "$@"..."
	$(_v) $(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BuildDirectory)/%.o: %.c
	@echo "Compiling "$@"..."
	$(_v) $(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(CC_Excecutable): $(CC_Objects)
	@echo "Linking "$@"..."
	$(_v) $(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

%: %.o
	@echo "Linking "$@"..."
	$(_v) $(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

# Shell scripts

%: %.sh
	@echo "Copying shell script "$@"..."
	$(_v) $(CP) $< $@
	$(_v) $(CHMOD) ugo+x $@

$(BuildDirectory)/%: %.sh
	@echo "Copying shell script "$@"..."
	$(_v) $(CP) $< $@
	$(_v) $(CHMOD) ugo+x $@
