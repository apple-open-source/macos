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
#				    bundle.make
#

all:: bundle

PRODUCT = $(PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX).$(TARGET_ARCH)$(BUNDLE_BINARY_EXT)

include $(MAKEFILEDIR)/bundle-common.make 
-include $(LOCAL_MAKEFILEDIR)/bundle.make.preamble

PROJECT_TYPE_SPECIFIC_GARBAGE = $(SYMROOT)/$(NAME).$(BUNDLE_EXTENSION) \
				$(SYMROOT)/$(NAME).palette

extra_configure_for_target_archs_exported_vars = \
	"PRODUCT_ROOT = $$prod_root/$(NAME).$(BUNDLE_EXTENSION)/Resources"

extra_finalize_install_exported_vars = \
	"PRODUCT_ROOT = $(PRODUCT_ROOT)/$(NAME).$(BUNDLE_EXTENSION)/Resources"

extra_actual_project_exported_vars = \
	"LINK_STYLE = ONE_MODULE"

projectType_specific_exported_vars = \
	"BUILD_OFILES_LIST_ONLY = $(BUILD_OFILES_LIST_ONLY)" \
 	"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
	"DEV_HEADER_DIR_BASE = $$header_base" \
	"PRODUCT_ROOT = $$prod_root/Resources"

set_product_root = \
	if [ "$(PROJECT_TYPE)" = "Palette" ] ; then \
		framework_ext="palette" ; \
	else \
		framework_ext=$(BUNDLE_EXTENSION) ; \
	fi ; \
	product_root="$(NAME).$$framework_ext"

LINK_STYLE = ONE_MODULE

-include $(LOCAL_MAKEFILEDIR)/bundle.make.postamble
