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
# palette.make
#
# Variable definitions and rules for building palette projects.  A
# palette is a directory which contains a loadable executable and any resources
# that executable requires.  See wrapped.make for more information about
# projects whose product is a directory.
#
# PUBLIC TARGETS
#    palette: synonymous with all
#
# IMPORTED VARIABLES
#    none
#
# EXPORTED VARIABLES
#    none
#

.PHONY: palette all
palette: all

PRODUCT = $(PRODUCT_DIR)/$(NAME).palette
PRODUCTS = $(PRODUCT)
INNER_PRODUCT = $(PRODUCT)$(VERSION_SUFFIX)/$(NAME)$(DLL_EXT)
STRIPPED_PRODUCTS = $(INNER_PRODUCT)

PROJTYPE_MFLAGS = -F$(PRODUCT_DIR)
ifeq "$(OS)" "MACOS"
PROJTYPE_LDFLAGS = $($(OS)_PROJTYPE_LDFLAGS) -bundle
PROJTYPE_CONVERT_BUNDLE = YES
else
ifeq "$(OS)" "NEXTSTEP"
PROJTYPE_LDFLAGS = $($(OS)_PROJTYPE_LDFLAGS) -bundle -undefined suppress
else
PROJTYPE_LDFLAGS = $($(OS)_PROJTYPE_LDFLAGS) -bundle
endif
endif


include $(MAKEFILEDIR)/wrapped-common.make
-include $(LOCAL_MAKEFILEDIR)/palette.make.preamble

$(PRODUCT): $(INNER_PRODUCT)

$(INNER_PRODUCT): $(DEPENDENCIES)
	$(SILENT) $(MKDIRS) $(PRODUCT)
	$(LD) $(ALL_LDFLAGS) $(ARCHITECTURE_FLAGS) -o $(INNER_PRODUCT) $(LOADABLES)

-include $(LOCAL_MAKEFILEDIR)/palette.make.postamble
