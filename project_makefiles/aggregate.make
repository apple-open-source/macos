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
#                              aggregate.make
#

default_rule: all

include $(MAKEFILEDIR)/common.make
-include $(LOCAL_MAKEFILEDIR)/aggregate.make.preamble

recurse_vars = \
	"OBJROOT = $(OBJROOT)" \
	"SYMROOT = $(SYMROOT)" \
	"DSTROOT = $(DSTROOT)" \
	"SRCROOT = $(SRCROOT)" \
	"PROJECT_HEADERS_DIR_NAME = $(PROJECT_HEADERS_DIR_NAME)" \
	"SKIP_EXPORTING_HEADERS = $(SKIP_EXPORTING_HEADERS)" \
	"TOP_PRODUCT_ROOT = $(SYMROOT)" \
	"MAKEFILEDIR = $(MAKEFILEDIR)" \
	"RC_CFLAGS = $(RC_CFLAGS)" \
	"RC_ARCHS = $$archs" \
	"TARGET_ARCHS = $$archs" \
	$(aggregate_recursion_exported_vars)

TARGET_ARCH = all

all debug profile::
	@$(process_target_archs) ; \
	 $(MAKE) recurse_for_subprojects \
		"BUILD_TARGET = $@" \
		"ONLY_SUBPROJECTS = `$(CHANGES) $(SYMROOT)/$(CHANGES_FILE_BASE).$(TARGET_ARCH) $@ $(ALL_SUBPROJECTS)`" \
		$(recurse_vars)

install installhdrs::
	@($(set_should_build) ; \
	if [ "$$should_build" = "yes" ] ; then \
	   $(process_target_archs) ; \
	   $(MAKE) recurse_for_subprojects \
		"BUILD_TARGET = $@" \
		"ONLY_SUBPROJECTS = $(ALL_SUBPROJECTS)" \
		$(recurse_vars) ; \
	fi)

recurse_for_subprojects::
	@(subdirectories="$(ONLY_SUBPROJECTS)" ; \
	target="$(BUILD_TARGET)"; \
	beginning_msg="Making $(BUILD_TARGET) in" ; \
	ending_msg="Finished $(BUILD_TARGET) in" ; \
	$(recurse_on_subdirectories))


projectType_specific_exported_vars = \
	"SYMROOT = $(SYMROOT)/$$sub.derived"   \
	"OBJROOT = $(OBJROOT)/$$sub.derived"   \
	"SRCROOT = $(SRCROOT)/$$sub"

.PHONY : always

$(ALL_SUBPROJECTS): always
	@($(process_target_archs) ; \
	if [ -n "$(BUILD_TARGET)" ] ; then \
	    build_target=$(BUILD_TARGET); \
	else \
	    build_target=all; \
	fi; \
	$(MAKE) recurse_for_subprojects \
	    "BUILD_TARGET = $$build_target" \
	    "ONLY_SUBPROJECTS = $@" \
	    $(recurse_vars))

always:

-include $(LOCAL_MAKEFILEDIR)/aggregate.make.postamble
