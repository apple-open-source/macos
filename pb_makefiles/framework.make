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
#
# IMPORTED VARIABLES
#    FRAMEWORK_DLL_INSTALLDIR:  On Windows platforms, this variable indicates
#	where to put the framework's DLL.  This variable defaults to 
#	$(INSTALLDIR)/../Executables
#	
# OVERRIDABLE VARIABLES
#    INSTALL_NAME_DIRECTIVE:  This directive ensures that executables linked
#	against the framework will run against the correct version even if
#	the current version of the framework changes.  You may override this
#	to "" as an alternative to using the DYLD_LIBRARY_PATH during your
#	development cycle, but be sure to restore it before installing.
#

.PHONY: framework all generate-dummy-file
framework: all
PROJTYPE = FRAMEWORK

ifeq "WINDOWS" "$(OS)"
DUMMY_SYMBOL = NSFramework_$(NAME)
DUMMY_SYMBOL_FILE = $(SFILE_DIR)/$(DUMMY_SYMBOL).m

FRAMEWORK_GENERATED_SRCFILES = $(notdir $(DUMMY_SYMBOL_FILE))

OTHER_GENERATED_SRCFILES += $(FRAMEWORK_GENERATED_SRCFILES)
OTHER_GENERATED_OFILES += $(FRAMEWORK_GENERATED_SRCFILES:.m=.o)

DUMMY_SYMBOL_INFO_FILE = $(SFILE_DIR)/NSFrameworkSymbol_$(NAME).plist
OTHER_INFO_FILES += $(DUMMY_SYMBOL_INFO_FILE)
endif

PRODUCT = $(PRODUCT_DIR)/$(NAME).framework
PRODUCTS = $(PRODUCT)
INNER_PRODUCT = $(PRODUCT)/$(NAME)$(BUILD_TYPE_SUFFIX)$(DLL_EXT)
INNER_PROFILED_PRODUCT = $(PRODUCT)/$(NAME)$(PROFILE_SUFFIX)$(DLL_EXT)
STRIPPED_PRODUCTS = $(INNER_PRODUCT)
STRIPPED_PROFILED_PRODUCTS = $(INNER_PROFILED_PRODUCT)

DYLIB_INSTALL_DIR = $(DYLIB_ROOT)$(INSTALLDIR)/$(NAME).framework
DYLIB_INSTALL_NAME = $(NAME)$(BUILD_TYPE_SUFFIX)$(DLL_EXT)
INSTALL_NAME_DIRECTIVE = -install_name `echo $(DYLIB_INSTALL_DIR)/$(DYLIB_INSTALL_NAME) | sed 's/\/\//\//g'`

PROJTYPE_LDFLAGS = -dynamic -compatibility_version $(COMPATIBILITY_PROJECT_VERSION) -current_version $(CURRENT_PROJECT_VERSION) $(INSTALL_NAME_DIRECTIVE)

PUBLIC_HDR_INSTALLDIR = $(INSTALLDIR)/$(NAME).framework/Headers
PRIVATE_HDR_INSTALLDIR = $(INSTALLDIR)/$(NAME).framework/PrivateHeaders

ifeq "WINDOWS" "$(OS)"
BEFORE_PREBUILD += generate-dummy-file
endif
BEFORE_INSTALL += verify-install-name-directive
BEFORE_INSTALL += preindex-installed-framework

ifeq "WINDOWS" "$(OS)"
ifneq "$(LIBRARY_STYLE)" "STATIC"

AFTER_INSTALL += install-dll
OS_LDFLAGS += -def $(WINDOWS_DEF_FILE)
INNER_PRODUCT_CRUFT = $(PRODUCT)/$(NAME)$(BUILD_TYPE_SUFFIX)$(EXP_EXT)

endif
endif

# frameworks are not yet converted to the new bundle layout
#ifeq "$(OS)" "MACOS"
#PROJTYPE_CONVERT_BUNDLE = YES
#endif

# This -F directive must come before any others to ensure that the
# local header files will be used instead of the header files from
# an already-installed version of the project.  Thus, we modify
# RECURSIVE_CFLAGS before including wrapped-common.make
RECURSIVE_CFLAGS += -F$(PRODUCT_DIR)

include $(MAKEFILEDIR)/wrapped-common.make
-include $(LOCAL_MAKEFILEDIR)/framework.make.preamble

ifeq "" "$(wildcard $(EOPREINDEX))"
PREINDEX_FRAMEWORK = NO
endif

RECURSIVE_FLAGS += PUBLIC_HDR_DIR=$(PUBLIC_HDR_DIR)
RECURSIVE_FLAGS += PRIVATE_HDR_DIR=$(PRIVATE_HDR_DIR)

$(PRODUCT): $(INNER_PRODUCT)

$(INNER_PRODUCT): $(DEPENDENCIES) $(WINDOWS_DEF_FILE)
	$(LIBTOOL) $(ALL_LIBTOOL_FLAGS) -o $(INNER_PRODUCT) $(LOADABLES)
ifneq "$(INNER_PRODUCT_CRUFT)" ""
	$(RM) -f $(INNER_PRODUCT_CRUFT)
endif

ifndef FRAMEWORK_DLL_INSTALLDIR
FRAMEWORK_DLL_INSTALLDIR = $(INSTALLDIR)/../Executables
endif

#
# install customization
#

verify-install-name-directive:
ifeq "" "$(INSTALL_NAME_DIRECTIVE)"
	$(SILENT) $(ECHO) You must restore the INSTALL_NAME_DIRECTIVE variable
	$(SILENT) $(ECHO) before installing a framework.
	$(SILENT) exit 1
endif

install-dll: $(DSTROOT)$(FRAMEWORK_DLL_INSTALLDIR)
	$(RM) -f $(DSTROOT)$(FRAMEWORK_DLL_INSTALLDIR)/$(NAME)$(DLL_EXT)
	$(MV) $(DSTROOT)$(INSTALLDIR)/$(NAME).framework/$(NAME)$(DLL_EXT) $(DSTROOT)$(FRAMEWORK_DLL_INSTALLDIR)

preindex-installed-framework:
ifneq "$(PREINDEX_FRAMEWORK)" "NO"
	$(EOPREINDEX) -o  $(GLOBAL_RESOURCE_DIR)
endif

preindex:
	$(SILENT) $(ECHO) -n preindexing $(NAME)...
	$(SILENT) $(EOPREINDEX)
	$(SILENT) $(ECHO) done

#
# creating directories
#

$(DSTROOT)$(FRAMEWORK_DLL_INSTALLDIR):
	$(MKDIRS) $@

ifeq "WINDOWS" "$(OS)"
generate-dummy-file: $(DUMMY_SYMBOL_FILE)
$(DUMMY_SYMBOL_FILE):
	$(SILENT) $(ECHO) "Creating... $@"
	$(SILENT) $(MKDIRS) $(SFILE_DIR)
	$(SILENT) $(ECHO) "@interface $(DUMMY_SYMBOL)" > $@
	$(SILENT) $(ECHO) "@end" >> $@
	$(SILENT) $(ECHO) "@implementation $(DUMMY_SYMBOL)" >> $@
	$(SILENT) $(ECHO) "@end" >> $@

$(DUMMY_SYMBOL_INFO_FILE):
	$(SILENT) $(ECHO) "{NSFrameworkSymbol=\"$(DUMMY_SYMBOL)\";}" > $(DUMMY_SYMBOL_INFO_FILE)
endif

-include $(LOCAL_MAKEFILEDIR)/framework.make.postamble
