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
# library.make
#
# Variable definitions and rules for building library projects.  An
# application is a directory which contains an executable and any resources
# that executable requires.  See wrapped.make for more information about
# projects whose product is a directory.
#
# PUBLIC TARGETS
#    library: synonymous with all
#
# IMPORTED VARIABLES
#    PUBLIC_HEADER_DIR:  Determines where public exported header files
#	should be installed.  Do not include $(DSTROOT) in this value --
#	it is prefixed automatically.
#    PRIVATE_HEADER_DIR:  Determines where private exported header files
#  	should be installed.  Do not include $(DSTROOT) in this value --
#	it is prefixed automatically.
#    LIBRARY_STYLE:  This may be either STATIC or DYNAMIC, and determines
#  	whether the libraries produces are statically linked when they
#	are used or if they are dynamically loadable.
#    LIBRARY_DLL_INSTALLDIR:  On Windows platforms, this variable indicates
#	where to put the library's DLL.  This variable defaults to 
#	$(INSTALLDIR)/../Executables
#
# OVERRIDABLE VARIABLES
#    INSTALL_NAME_DIRECTIVE:  This directive ensures that executables linked
#	against the shlib will run against the correct version even if
#	the current version of the shlib changes.  You may override this
#	to "" as an alternative to using the DYLD_LIBRARY_PATH during your
#	development cycle, but be sure to restore it before installing.
#
# EXPORTED VARIABLES
#    none
#

.PHONY: library all
library: all
PROJTYPE = LIBRARY

PRODUCT = $(PRODUCT_DIR)/$(LIBRARY_PREFIX)$(NAME)$(BUILD_TYPE_SUFFIX)$(LIBRARY_EXT)
PRODUCTS = $(PRODUCT)
STRIPPED_PRODUCTS = $(PRODUCT)

# libraries do not have a debug suffix, but java packages do...
ifeq "library.make" "$(MAKEFILE)"
override DEBUG_SUFFIX =
endif

ifndef LIBRARY_DLL_INSTALLDIR
LIBRARY_DLL_INSTALLDIR = $(INSTALLDIR)/../Executables
endif

DYLIB_INSTALL_DIR = $(DYLIB_ROOT)$(INSTALLDIR)
DYLIB_INSTALL_NAME = $(LIBRARY_PREFIX)$(NAME)$(LIBRARY_EXT)
INSTALL_NAME_DIRECTIVE = -install_name `echo $(DYLIB_INSTALL_DIR)/$(DYLIB_INSTALL_NAME) | sed 's/\/\//\//g'`

ifneq "STATIC" "$(LIBRARY_STYLE)"
PROJTYPE_LDFLAGS = -dynamic -compatibility_version $(COMPATIBILITY_PROJECT_VERSION) -current_version $(CURRENT_PROJECT_VERSION) $(INSTALL_NAME_DIRECTIVE)
else
PROJTYPE_LDFLAGS = -static
endif

BEFORE_INSTALL += verify-install-name-directive

ifeq "WINDOWS" "$(OS)"

ifneq "STATIC" "$(LIBRARY_STYLE)"

PRODUCT_CRUFT = $(PRODUCT_DIR)/$(LIBRARY_PREFIX)$(NAME)$(EXP_EXT)
AFTER_INSTALL += install-dll
OS_LDFLAGS += -def $(WINDOWS_DEF_FILE)

endif
endif

include $(MAKEFILEDIR)/common.make
-include $(LOCAL_MAKEFILEDIR)/library.make.preamble

ifeq "STATIC" "$(LIBRARY_STYLE)"

$(PRODUCT): $(DEPENDENCIES)
ifeq "$(USE_AR)" "YES"
	$(AR) ru $(PRODUCT) $(LOADABLES)
	$(RANLIB) $(PRODUCT)
else
	$(LIBTOOL) $(ALL_LIBTOOL_FLAGS) -o $(PRODUCT) $(LOADABLES)
endif

else

$(PRODUCT): $(DEPENDENCIES) $(WINDOWS_DEF_FILE)
	$(LIBTOOL) $(filter-out -g, $(ALL_LDFLAGS)) -o $(PRODUCT) $(LOADABLES)
ifneq "$(PRODUCT_CRUFT)" ""
	$(RM) -f $(PRODUCT_CRUFT)
endif
endif

verify-install-name-directive:
ifeq "" "$(INSTALL_NAME_DIRECTIVE)"
	$(SILENT) $(ECHO) You must restore the INSTALL_NAME_DIRECTIVE variable
	$(SILENT) $(ECHO) before installing a framework.
	$(SILENT) exit 1
endif

install-dll: $(DSTROOT)$(LIBRARY_DLL_INSTALLDIR)
	$(RM) -f $(DSTROOT)$(LIBRARY_DLL_INSTALLDIR)/$(NAME)$(DLL_EXT)
	$(MV) $(DSTROOT)$(INSTALLDIR)/$(NAME)$(BUILD_TYPE_SUFFIX)$(DLL_EXT) $(DSTROOT)$(LIBRARY_DLL_INSTALLDIR)

#
# creating directories
#

$(DSTROOT)$(LIBRARY_DLL_INSTALLDIR):
	$(MKDIRS) $@

-include $(LOCAL_MAKEFILEDIR)/library.make.postamble
