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
#                                  tool.make
#

tool:: all

PRODUCT = $(PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX).$(TARGET_ARCH)$(EXECUTABLE_EXT)

include $(MAKEFILEDIR)/common.make
-include $(LOCAL_MAKEFILEDIR)/tool.make.preamble

ENABLE_INFO_DICTIONARY = NO

projectType_specific_exported_vars = \
	"DEV_HEADER_DIR_BASE = $$header_base" \
	"BUILD_OFILES_LIST_ONLY = $(BUILD_OFILES_LIST_ONLY)" \
	"CODE_GEN_STYLE = $(CODE_GEN_STYLE)"

.PHONY : tool debug profile

all debug profile::
	@($(check_for_gnumake) ; \
	$(process_target_archs) ; \
	$(set_dynamic_flags) ; \
	$(set_objdir) ; \
	for arch in $$archs ; do \
	   $(ECHO) == Making $(NAME) for $$arch == ; \
	   ofile_dir="$(OBJROOT)/`echo $$buildtype`_$$objdir/$$arch" ; \
	   $(MAKE) project \
		"SYM_DIR = $(SYMROOT)/$(DERIVED_SRC_DIR_NAME)" \
		"OFILE_DIR = $$ofile_dir" \
		"BUILD_TYPE_CFLAGS = $($@_target_CFLAGS) $$dynamic_cflags" \
		"BUILD_TYPE_LDFLAGS = $($@_target_LDFLAGS)" \
		"RC_CFLAGS = -arch $$arch $$archless_rcflags" \
		"RC_ARCHS = $$archs" \
		"ALL_ARCH_FLAGS = $$arch_flags" \
		"TARGET_ARCH = $$arch" \
		"BUILD_TARGET = $@" \
		"BUILD_TYPE_SUFFIX = $$build_type_suffix" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" \
		"DEVROOT = $(DEVROOT)" \
		"PRODUCT_ROOT = $(SYMROOT)" \
		"TOP_PRODUCT_ROOT = $(SYMROOT)" \
		"IS_TOPLEVEL = YES" \
		"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
		"INSTALLDIR = $(INSTALLDIR)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" \
		"DSTROOT = $(DSTROOT)" \
		"SRCROOT = $(SRCROOT)" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" ; \
	   if [ -n "$$last_arch" ] ; then \
		multiple_archs=yes ; \
	   fi ; \
	   last_arch=$$arch ; \
        done ; \
	$(MAKE) configure_for_target_archs \
		"TOP_PRODUCT_ROOT = $(SYMROOT)" \
		"PRODUCT_ROOT = $(SYMROOT)" \
		"BUILD_TYPE_SUFFIX = $$build_type_suffix" \
		"MULTIPLE_ARCHS = $$multiple_archs" \
		"SINGLE_ARCH = $$last_arch" \
		"RC_ARCHS = $$archs" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" 	)

configure_for_target_archs:: 
	@(for arch in $(RC_ARCHS) ; do \
	    dependencies="$$dependencies $(PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX).$$arch$(EXECUTABLE_EXT)" ;\
	    lipo_args="$$lipo_args -arch $$arch $(PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX).$$arch$(EXECUTABLE_EXT)" ; \
	done ; \
	$(set_should_build) ; \
	if [ "$$should_build" = "yes" ] ; then \
	    $(MAKE) final_tool \
		"TOP_PRODUCT_ROOT = $(TOP_PRODUCT_ROOT)" \
		"PRODUCT_ROOT = $(PRODUCT_ROOT)" \
		"DEPENDENCIES = $$dependencies" \
		"LIPO_ARGS = $$lipo_args" \
		"BUILD_TYPE_SUFFIX = $(BUILD_TYPE_SUFFIX)" \
		"MULTIPLE_ARCHS = $(MULTIPLE_ARCHS)" \
		"SINGLE_ARCH = $(SINGLE_ARCH)" \
		"RC_ARCHS = $(RC_ARCHS)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" ; \
	else \
	    $(ECHO) Not configuring tool $(NAME). ; \
	fi)

.PHONY : final_tool

final_tool: $(PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX)$(EXECUTABLE_EXT)
		
$(PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX)$(EXECUTABLE_EXT): $(DEPENDENCIES)
	@(if [ -n "$(MULTIPLE_ARCHS)" ] ; then \
	    cmd="$(LIPO) -create $(LIPO_ARGS) -o $@" ; \
	else \
	    $(RM) -f $@ ; \
	    cmd="$(TRANSMOGRIFY) $(PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX).$(SINGLE_ARCH)$(EXECUTABLE_EXT) $@" ; \
	fi ; \
	$(ECHO) $$cmd ; $$cmd ; \
	if [ "$(DELETE_THIN_RESULTS)" = "YES" -a "$(IS_TOPLEVEL)" = "YES" ] ; then \
	     cmd="$(RM) -f $(DEPENDENCIES)" ; \
	     $(ECHO) $$cmd ; $$cmd ; \
	fi)


.PHONY : project actual_project

project::
	@($(set_should_build) ; \
	if [ "$$should_build" = "yes" ] ; then \
	   $(MAKE) actual_project \
		"PRODUCT_ROOT = $(PRODUCT_ROOT)" \
       		"OFILE_DIR = $(OFILE_DIR)" \
       		"SYM_DIR = $(SYM_DIR)" \
		"BUILD_TYPE_CFLAGS = $(BUILD_TYPE_CFLAGS)" \
		"IS_TOPLEVEL = $(IS_TOPLEVEL)" \
		"BUILD_TARGET = $(BUILD_TARGET)" \
		"ALL_ARCH_FLAGS = $(ALL_ARCH_FLAGS)" \
		"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
       		"DEV_HEADER_DIR_BASE = $(DEV_HEADER_DIR_BASE)" \
		"DEV_PROJECT_HEADER_DIR_BASE = $(DEV_PROJECT_HEADER_DIR_BASE)"\
       		"BUILD_OFILES_LIST_ONLY = $(BUILD_OFILES_LIST_ONLY)" \
       		"MAKEFILEDIR = $(MAKEFILEDIR)" \
       		"DSTROOT = $(DSTROOT)" \
       		"SRCROOT = $(SRCROOT)" \
       		"OBJROOT = $(OBJROOT)" \
       		"SYMROOT = $(SYMROOT)" \
		$(extra_actual_project_exported_vars) ; \
	else \
	   $(ECHO) " ..... $(NAME) not built for architecture $(TARGET_ARCH), platform $(PLATFORM_OS)" ; \
	fi)

actual_project: initial_targets all_subprojects resources $(PRODUCT_ROOT) $(PRODUCT)


$(PRODUCT): $(PRODUCT_DEPENDS)
	@(if [ "`$(ECHO) $(OFILES) $(OTHER_OFILES) | wc -w`" != "       0" ] ; \
	then \
            $(set_dynamic_link_flags) ; \
	    frameworks=`$(FRAMEWORK_TOOL) $(FRAMEWORKS) $(BUILD_TARGET)` ; \
	    if [ "$(ALWAYS_USE_OFILELISTS)" = "YES" ] ; then \
	       (cd $(OFILE_DIR) ; $(OFILE_LIST_TOOL) -removePrefix $(OFILE_DIR)/ \
		 $(NON_SUBPROJ_OFILES) $(SUBPROJ_OFILELISTS) $(OTHER_OFILES) \
		 $(OTHER_GENERATED_OFILES) -o $(NAME).ofileList) ; \
  	       cmd="$(CC) $$dynamic_ldflags $(ALL_CFLAGS) $(OBJCFLAG) \
		   $(ALL_LDFLAGS) -o $@ -filelist \
		   $(OFILE_DIR)/$(NAME).ofileList,$(OFILE_DIR) \
		   $$frameworks $(LIBS) $(OTHER_LIBS)" ; \
	    else \
	       cmd="$(CC) $$dynamic_ldflags $(ALL_CFLAGS) $(OBJCFLAG) \
		   $(ALL_LDFLAGS) -o $@ $(OFILES) $(OTHER_OFILES) \
		   $(OTHER_GENERATED_OFILES) $$frameworks $(LIBS) $(OTHER_LIBS)" ; \
	    fi ; \
	    $(ECHO) $$cmd ; \
	    $$cmd ; \
        fi)

PROJECT_TYPE_SPECIFIC_GARBAGE = $(SYMROOT)/$(NAME)$(EXECUTABLE_EXT)

.PHONY : before_install install 

before_install:: $(DSTROOT)$(INSTALLDIR)
	$(RM) -rf $(DSTROOT)$(INSTALLDIR)/$(NAME)$(EXECUTABLE_EXT)

ifeq ($(PLATFORM_OS)-$(REINSTALLING), winnt-)
install:: all
	      $(MAKE) reinstall_stripped REINSTALLING=YES
else
install:: all before_install installhdrs $(OTHER_INSTALL_DEPENDS)
	@($(set_should_build) ; \
	if [ "$$should_build" = "yes" ] ; then \
	   $(process_target_archs) ; \
	   cmd="$(CP) $(SYMROOT)/$(NAME)$(EXECUTABLE_EXT) $(DSTROOT)$(INSTALLDIR)" ; \
	   $(ECHO) $$cmd ; $$cmd ; \
	   product="$(DSTROOT)$(INSTALLDIR)/$(NAME)$(EXECUTABLE_EXT)"; \
	   $(MAKE) finalize_install \
		"DSTROOT = $(DSTROOT)" \
		"SRCROOT = $(SRCROOT)" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" \
		"DEVROOT = $(DEVROOT)" \
		"BUILD_TARGET = $@" \
		"INSTALLDIR = $(INSTALLDIR)" \
		"PRODUCT_ROOT = $(DSTROOT)$(INSTALLDIR)" \
		"PRODUCT = $$product" \
		"SYM_DIR = $(SYMROOT)/$(DERIVED_SRC_DIR_NAME)" \
		"OFILE_DIR = $(OBJROOT)/$$objdir/$$arch" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" \
		"RC_CFLAGS = $$arch_cflags" \
		"RC_ARCHS = $$archs" ; \
	   cmd="$(CHMOD) -R a-w $$product" ; \
	   $(ECHO) $$cmd ; $$cmd || true ; \
	   if [ -n "$(INSTALL_AS_USER)" ] ; then \
	      cmd="$(CHOWN) -R $(INSTALL_AS_USER) $$product" ; \
	      $(ECHO) $$cmd ; $$cmd || true ; \
	   fi ; \
	   if [ "$(INSTALL_AS_GROUP)" != "" ] ; then \
	      cmd="$(CHGRP) -R $(INSTALL_AS_GROUP) $$product" ; \
	      $(ECHO) $$cmd ; $$cmd || true ; \
	   fi ; \
	   if [ "$(INSTALL_PERMISSIONS)" != "" ] ; then \
	      cmd="$(CHMOD) $(INSTALL_PERMISSIONS) $$product" ; \
	      $(ECHO) $$cmd ; $$cmd || true ; \
	   fi ; \
	fi)
endif

strip_myself::
	@($(set_should_build) ; \
	if [ "$$should_build" = "yes" ] ; then \
	   if [ "$(BUILD_OFILES_LIST_ONLY)" != "YES" \
	        -a "$(STRIP_ON_INSTALL)" = "YES" ] ; then \
	      cmd="$(STRIP) $(TOOL_STRIP_OPTS) $(PRODUCT_ROOT)/$(NAME)*$(EXECUTABLE_EXT)" ; \
	      $(ECHO) $$cmd ; $$cmd ; \
	   fi ; \
	   for arch in $(RC_ARCHS) ; do \
	       cmd="$(RM) -f $(PRODUCT_ROOT)/$(NAME)*.$$arch$(EXECUTABLE_EXT)" ; \
	       $(ECHO) $$cmd ; $$cmd ; \
	   done ; \
	fi)

-include $(LOCAL_MAKEFILEDIR)/tool.make.postamble
