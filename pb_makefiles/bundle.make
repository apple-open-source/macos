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
# bundle.make
#
# Variable definitions and rules for building bundle projects.  A bundle
# is a directory which contains a dynamically-loadable executable and any resources
# that executable requires.  See wrapped.make for more information about
# projects whose product is a directory.
#
# PUBLIC TARGETS
#    bundle: synonymous with all
#
# IMPORTED VARIABLES
#    none
#
# EXPORTED VARIABLES
#    none
#

bundle: all

ifndef BUNDLE_EXTENSION
BUNDLE_EXTENSION = bundle
endif

PRODUCT = $(PRODUCT_DIR)/$(NAME).$(BUNDLE_EXTENSION)
PRODUCTS = $(PRODUCT)
INNER_PRODUCT = $(PRODUCT)$(VERSION_SUFFIX)/$(NAME)$(BUILD_TYPE_SUFFIX)$(DLL_EXT)
INNER_PROFILED_PRODUCT = $(PRODUCT)$(VERSION_SUFFIX)/$(NAME)$(PROFILE_SUFFIX)$(DLL_EXT)
STRIPPED_PRODUCTS = $(INNER_PRODUCT)
STRIPPED_PROFILED_PRODUCTS = $(INNER_PROFILED_PRODUCT)

PROJTYPE_MFLAGS = -F$(PRODUCT_DIR)
ifeq "$(OS)" "MACOS"
PROJTYPE_LDFLAGS = $($(OS)_PROJTYPE_LDFLAGS) -bundle
PROJTYPE_CONVERT_BUNDLE = YES
endif
ifeq "$(OS)" "NEXTSTEP"
PROJTYPE_LDFLAGS = $($(OS)_PROJTYPE_LDFLAGS) -bundle -undefined suppress
endif
ifeq "$(OS)" "WINDOWS"
PROJTYPE_LDFLAGS = $($(OS)_PROJTYPE_LDFLAGS) -bundle
endif

include $(MAKEFILEDIR)/wrapped-common.make
-include $(LOCAL_MAKEFILEDIR)/bundle.make.preamble

# Turn java compression back on (off by default in flags.make)
#JAVA_ZIP_COMPRESSION_LEVEL =
#JAVA_JAR_COMPRESSION_LEVEL =

$(PRODUCT): $(INNER_PRODUCT)

$(INNER_PRODUCT): $(DEPENDENCIES)
	$(SILENT) $(MKDIRS) $(PRODUCT)
ifeq "$(OS)" "SOLARIS"
	$(LIBTOOL) $(ALL_LIBTOOL_FLAGS) -o $(INNER_PRODUCT) $(LOADABLES)
else
	$(LD) $(ALL_LDFLAGS) $(ARCHITECTURE_FLAGS) -o $(INNER_PRODUCT) $(LOADABLES)
endif

-include $(LOCAL_MAKEFILEDIR)/bundle.make.postamble


