#
# Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
#
# kext.make
#
# Variable definitions and rules for building kext projects.  A kext
# is a directory which contains a dynamically-loadable executable and any
# resources that executable requires.  See wrapped.make for more information
# about projects whose product is a directory.
#
# PUBLIC TARGETS
#    extension: synonymous with all
#
# IMPORTED VARIABLES
#    none
#
# EXPORTED VARIABLES
#    none
#

.PHONY: extension all create-xmlinfo-file

extension: all

ifndef BUNDLE_EXTENSION
BUNDLE_EXTENSION = kext
endif

PRODUCT = $(PRODUCT_DIR)/$(NAME).$(BUNDLE_EXTENSION)
PRODUCTS = $(PRODUCT)

PROJTYPE_RESOURCES = ProjectTypes/KernelExtension.projectType/Resources

XMLINFO_FILE = $(GLOBAL_RESOURCE_DIR)/Info-$(PLATFORM_OS).xml
XMLINFO_FILES = CustomInfo.xml $(wildcard *.kmodproj/CustomInfo.xml)
BEFORE_BUILD += create-xmlinfo-file
ENABLE_INFO_DICTIONARY = NO

include $(MAKEFILEDIR)/wrapped-common.make
export GLOBAL_RESOURCE_DIR = $(PRODUCT)$(VERSION_SUFFIX)

ifndef LOCAL_MAKEFILEDIR
    LOCAL_MAKEFILEDIR = $(LOCAL_DEVELOPER_DIR)/Makefiles/pb_makefiles
endif

-include $(LOCAL_MAKEFILEDIR)/kext.make.preamble

#
# Try to find the Driver.projectType root directory
#
ifneq "" "$(wildcard $(NEXT_ROOT)$(LOCAL_DEVELOPER_DIR)/$(PROJTYPE_RESOURCES))"
KEXT_TOOLS_ROOT=$(NEXT_ROOT)$(LOCAL_DEVELOPER_DIR)/$(PROJTYPE_RESOURCES)
else

ifneq "" "$(wildcard $(HOME)/Library/$(PROJTYPE_RESOURCES))"
KEXT_TOOLS_ROOT=$(HOME)/Library/$(PROJTYPE_RESOURCES)
else
KEXT_TOOLS_ROOT=$(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/$(PROJTYPE_RESOURCES)
endif
endif

EXPR = /bin/expr

#
# prebuild rules
#

create-xmlinfo-file: $(GLOBAL_RESOURCE_DIR) $(XMLINFO_FILE)

XMLMERGEINFO = $(KEXT_TOOLS_ROOT)/KExtMergeInfo

ifneq "" "$(wildcard $(XMLINFO_FILES))"
OPTIONAL_XMLINFO_FILES += $(XMLINFO_FILES)
endif

$(XMLINFO_FILE): $(GLOBAL_RESOURCE_DIR) PB.project $(OTHER_XMLINFO_FILES) $(OPTIONAL_XMLINFO_FILES)
ifneq "$(ENABLE_XMLINFO_DICTIONARY)" "NO"
	$(SILENT) mergeInfoArgs=""; \
	for file in $(OPTIONAL_XMLINFO_FILES); do \
	    if [ -r "$$file" ] ; then \
		mergeInfoArgs="$$mergeInfoArgs $$file" ; \
	    fi ; \
	done; \
	if [ -x "$(XMLMERGEINFO)$(EXECUTABLE_EXT)" ] ; then \
	   $(RM) -f $@ ; \
	   cmd="$(XMLMERGEINFO) -o $@ PB.project $$mergeInfoArgs $(OTHER_XMLINFO_FILES)" ; \
	   $(ECHO) $$cmd ; eval $$cmd ; \
	fi
endif

$(PRODUCT):

-include $(LOCAL_MAKEFILEDIR)/kext.make.postamble

