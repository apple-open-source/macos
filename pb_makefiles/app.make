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
# app.make
#
# Variable definitions and rules for building application projects.  An
# application is a directory which contains an executable and any resources
# that executable requires.  See wrapped.make for more information about
# projects whose product is a directory.
#
# PUBLIC TARGETS
#    app: synonymous with all
#
# IMPORTED VARIABLES
#    APP_WRAPPER_EXTENSION:  the extension for the app wrapper.  Defaults to ".app"
#
# EXPORTED VARIABLES
#    none
#

.PHONY: app all
app: all
PROJTYPE = APP

ifeq "" "$(APP_WRAPPER_EXTENSION)"
APP_WRAPPER_EXTENSION = .app
endif

PRODUCT = $(PRODUCT_DIR)/$(NAME)$(APP_WRAPPER_EXTENSION)
PRODUCTS = $(PRODUCT)
PROJTYPE_GARBAGE = $(PRODUCT_DIR)/$(NAME).debug $(PRODUCT_DIR)/$(NAME).profile
INNER_PRODUCT = $(PRODUCT)/$(NAME)$(EXECUTABLE_EXT)
STRIPPED_PRODUCTS = $(INNER_PRODUCT)

ifeq "WINDOWS" "$(OS)"
REG_FILE = appResources.reg
RESOURCE_OFILE = appResources.o
PROJTYPE_LDFLAGS = -win
PROJTYPE_OFILES = $(RESOURCE_OFILE)
PROJTYPE_LIBS = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Libraries/libNSWinMain.a
endif
ifeq "MACOS" "$(OS)"
PROJTYPE_LDFLAGS = -sectcreate __ICON __header $(NAME).iconheader -segprot __ICON r r $(ICONSECTIONS)
PROJTYPE_CONVERT_BUNDLE = YES
endif
ifeq "NEXTSTEP" "$(OS)"
PROJTYPE_LDFLAGS = -sectcreate __ICON __header $(NAME).iconheader -segprot __ICON r r $(ICONSECTIONS)
endif

include $(MAKEFILEDIR)/wrapped-common.make
-include $(LOCAL_MAKEFILEDIR)/app.make.preamble

IMPLICIT_SOURCE_FILES += $(NAME).iconheader

$(PRODUCT): $(INNER_PRODUCT)

$(INNER_PRODUCT): $(DEPENDENCIES)
	$(SILENT) $(MKDIRS) $(PRODUCT)
	$(CC) $(ALL_LDFLAGS) $(ARCHITECTURE_FLAGS) -o $(INNER_PRODUCT) $(LOADABLES)

-include $(LOCAL_MAKEFILEDIR)/app.make.postamble

