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
#                              framework.make
#

all:: framework

PRODUCT = $(PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX).$(TARGET_ARCH)$(BUNDLE_BINARY_EXT)

PROJECTTYPE_SPECIFIC_INITIAL_TARGETS = framework_initial_symlinks

include $(MAKEFILEDIR)/bundle-common.make
-include $(LOCAL_MAKEFILEDIR)/framework.make.preamble

# Framework symlink-based versioning support:

VIRTUAL_PRODUCT = $(VIRTUAL_PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX)$(BUNDLE_BINARY_EXT)
HEADERS_ROOT = $(PRODUCT_ROOT)/Headers
PRIVATEHEADERS_ROOT = $(PRODUCT_ROOT)/PrivateHeaders
VIRTUAL_RESOURCES_ROOT = $(VIRTUAL_PRODUCT_ROOT)/Resources
VIRTUAL_HEADERS_ROOT = $(VIRTUAL_PRODUCT_ROOT)/Headers
VIRTUAL_PRIVATEHEADERS_ROOT = $(VIRTUAL_PRODUCT_ROOT)/PrivateHeaders
CURRENT_VERSION_ROOT = $(VIRTUAL_PRODUCT_ROOT)/Versions/Current
VERSION_ROOT = $(VIRTUAL_PRODUCT_ROOT)/Versions

$(VERSION_ROOT):
	@$(MKDIRS) $@

$(VIRTUAL_PRODUCT):
	@($(RM) -f $@ ; \
	cmd="$(SYMLINK) Versions/Current/$(NAME)$(BUILD_TYPE_SUFFIX) $@" ; \
	$(ECHO) $$cmd; $$cmd)

$(VIRTUAL_RESOURCES_ROOT):
	@($(RM) -f $@ ; \
	cmd="$(SYMLINK) Versions/Current/Resources $@" ; \
	$(ECHO) $$cmd; $$cmd)

$(VIRTUAL_HEADERS_ROOT):
	@($(RM) -f $@ ; \
	cmd="$(SYMLINK) Versions/Current/Headers $@" ; \
	$(ECHO) $$cmd; $$cmd)

$(VIRTUAL_PRIVATEHEADERS_ROOT):
	@($(RM) -f $@ ; \
	cmd="$(SYMLINK) Versions/Current/PrivateHeaders $@" ; \
	$(ECHO) $$cmd; $$cmd)

$(CURRENT_VERSION_ROOT):
	@($(RM) -f $@ ; \
	cmd="$(SYMLINK) $(DEPLOY_WITH_VERSION_NAME) $@" ; \
	$(ECHO) $$cmd; $$cmd)

.PHONY : framework_symlinks framework_initial_symlinks after_installhdrs

framework_symlinks: $(VERSION_ROOT) $(VIRTUAL_PRODUCT) $(VIRTUAL_RESOURCES_ROOT) $(VIRTUAL_HEADERS_ROOT) $(VIRTUAL_PRIVATEHEADERS_ROOT) $(CURRENT_VERSION_ROOT)

framework_initial_symlinks:
	@(if [ "$(CURRENTLY_ACTIVE_VERSION)" = "YES" -a \
	       "$(DISABLE_VERSIONING)" != "YES" ] ; then \
	    $(MAKE) framework_symlinks \
		"VIRTUAL_PRODUCT_ROOT = $(SYMROOT)/$(NAME).framework" \
		"BUILD_TYPE_SUFFIX = $(BUILD_TYPE_SUFFIX)" ; \
	fi)

after_installhdrs::
	@(if [ "$(CURRENTLY_ACTIVE_VERSION)" = "YES" -a \
	       "$(DISABLE_VERSIONING)" != "YES" ] ; then \
	    $(MAKE) framework_symlinks \
		"VIRTUAL_PRODUCT_ROOT = $(DSTROOT)$(INSTALLDIR)/$(NAME).framework" \
		"BUILD_TYPE_SUFFIX = $(BUILD_TYPE_SUFFIX)" ; \
	fi)

# Framework-specific attributes:

PROJECT_TYPE_SPECIFIC_GARBAGE = $(SYMROOT)/$(NAME).framework \
				   $(SYMROOT)/$(NAME).debug \
				   $(SYMROOT)/$(NAME).profile

DYLIB_INSTALL_NAME = Versions/$(DEPLOY_WITH_VERSION_NAME)/$(NAME)$(BUILD_TYPE_SUFFIX)
DYLIB_INSTALL_DIR = $(INSTALLDIR)/$(NAME).framework

projectType_specific_exported_vars = \
	"BUILD_OFILES_LIST_ONLY = YES" \
 	"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
	"DEV_HEADER_DIR_BASE = $$header_base" \
	"PRODUCT_ROOT = $$prod_root/Resources"

extra_configure_for_target_archs_exported_vars = \
	"PRODUCT_ROOT = $$prod_root/$(REL_PRODUCT_ROOT)/Resources"

extra_finalize_install_exported_vars = \
	"PRODUCT_ROOT = $(PRODUCT_ROOT)/Resources"

set_product_root = \
	framework_ext="framework" ; \
	if [ "$(DISABLE_VERSIONING)" = "YES" ] ; then \
	    product_root="$(NAME).framework" ; \
	else \
	    product_root="$(NAME).framework/Versions/$(DEPLOY_WITH_VERSION_NAME)" ; \
	fi

LINK_STYLE = DYLIB

DLL_DIR_NAME = Executables

before_install::
	@(if [ "$(PLATFORM_OS)" = "winnt" ] ; then \
		$(RM) -rf $(DSTROOT)$(INSTALLDIR)/../$(DLL_DIR_NAME)/$(NAME)*.dll ; \
	fi)

finalize_install::
	@(if [ "$(PLATFORM_OS)" = "winnt" ] ; then \
		$(MKDIRS) $(DSTROOT)$(INSTALLDIR)/../$(DLL_DIR_NAME) ; \
	    cmd="$(MV) $(PRODUCT_ROOT)/$(NAME).dll $(DSTROOT)$(INSTALLDIR)/../$(DLL_DIR_NAME)/" ; \
	    echo $$cmd ; $$cmd ; \
    	    if [ -r $(PRODUCT_ROOT)/$(NAME)$(DEBUG_BUILD_TYPE_SUFFIX).dll ] ; then \
		cmd="$(MV) $(PRODUCT_ROOT)/$(NAME)$(DEBUG_BUILD_TYPE_SUFFIX).dll $(DSTROOT)$(INSTALLDIR)/../$(DLL_DIR_NAME)/" ; \
		echo $$cmd ; $$cmd ; \
	    fi ; \
    	    if [ -r $(PRODUCT_ROOT)/$(NAME)$(PROFILE_BUILD_TYPE_SUFFIX).dll ] ; then \
		cmd="$(MV) $(PRODUCT_ROOT)/$(NAME)$(PROFILE_BUILD_TYPE_SUFFIX).dll $(DSTROOT)$(INSTALLDIR)/../$(DLL_DIR_NAME)/" ; \
		echo $$cmd ; $$cmd ; \
	    fi ; \
	fi)

-include $(LOCAL_MAKEFILEDIR)/framework.make.postamble
