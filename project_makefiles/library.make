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
#                                 library.make
#

library:: all

include $(MAKEFILEDIR)/common.make
-include $(LOCAL_MAKEFILEDIR)/library.make.preamble

DYLIB_SYMLINK_NAME = $(LIBRARY_PREF)$(NAME)$(BUILD_TYPE_SUFFIX).dylib
DYLIB_INSTALL_NAME = $(LIBRARY_PREF)$(NAME)$(BUILD_TYPE_SUFFIX).$(DEPLOY_WITH_VERSION_NAME).dylib
DYLIB_INSTALL_DIR = $(INSTALLDIR)

ENABLE_INFO_DICTIONARY = NO

RESOURCES_ROOT = $(PRODUCT_ROOT)

DYNAMIC_BUILD_TYPE_SUFFIX = ""
DEBUG_BUILD_TYPE_SUFFIX = "_g"
PROFILE_BUILD_TYPE_SUFFIX = "_p"

all debug profile posix shlib:: 
	@($(check_for_gnumake) ; \
	$(process_target_archs) ; \
	$(set_dynamic_flags) ; \
	$(set_objdir) ; \
	for arch in $$archs ; do \
           $(set_should_build) ; \
	   if [ "$$should_build" = "no" ] ; then continue ; fi ; \
	   echo == Making target $@ for $(NAME) \($$arch\) == ; \
	   ofile_dir="$(OBJROOT)/`echo $$buildtype`_$$objdir/$$arch" ; \
	   $(MAKE) project \
		"SYM_DIR = $(SYMROOT)/$(DERIVED_SRC_DIR_NAME)" \
		"OFILE_DIR = $$ofile_dir" \
		"BUILD_TYPE_CFLAGS = $($@_target_CFLAGS) $$dynamic_cflags" \
		"BUILD_TYPE_LDFLAGS = $($@_target_LDFLAGS)" \
		"RC_CFLAGS = -arch $$arch $$archless_rcflags" \
		"ALL_ARCH_FLAGS = $$arch_flags" \
		"RC_ARCHS = $$archs" \
		"TARGET_ARCH = $$arch" \
		"BUILD_TARGET = $@" \
		"BUILD_TYPE_SUFFIX = $$build_type_suffix" \
		"DEVROOT = $(DEVROOT)" \
		"BUILD_OFILES_LIST_ONLY = $(BUILD_OFILES_LIST_ONLY)" \
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
	    lists="$$lists $$ofile_dir/$(NAME).ofileList" ; \
        done ; \
	$(MAKE) configure_for_target_archs \
		"TOP_PRODUCT_ROOT = $(SYMROOT)" \
		"PRODUCT_ROOT = $(SYMROOT)" \
		"BUILD_TYPE_SUFFIX = $$build_type_suffix" \
		"BAD_PREFIX = `echo $$buildtype`_$$objdir/" \
		"OFILE_DIR = $(OBJROOT)/`echo $$buildtype`_$$objdir" \
		"MULTIPLE_ARCHS = $$multiple_archs" \
		"SINGLE_ARCH = $$last_arch" \
		"RC_ARCHS = $$archs" \
		"ARCH_SPECIFIC_OFILELISTS = $$lists" \
		"BUILD_OFILES_LIST_ONLY = $(BUILD_OFILES_LIST_ONLY)" \
		"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)")

configure_for_target_archs::
	@($(set_dynamic_flags) ; \
	for arch in $(RC_ARCHS) ; do \
	    dependencies="$$dependencies $(SYMROOT)/$(LIBRARY_PREF)$(NAME)$(BUILD_TYPE_SUFFIX).$$arch$$library_ext" ;\
	    lipo_args="$$lipo_args -arch $$arch $(SYMROOT)/$(LIBRARY_PREF)$(NAME)$(BUILD_TYPE_SUFFIX).$$arch$$library_ext" ; \
	done ; \
	$(set_should_build) ; \
	if [ "$$should_build" = "yes"  \
	   -a "$(BUILD_OFILES_LIST_ONLY)" != "YES" ] ; then \
	   $(MAKE) final \
		"SYMROOT = $(SYMROOT)" \
		"LIB_NAME = $(LIBRARY_PREF)$(NAME)$(BUILD_TYPE_SUFFIX)$$library_ext" \
		"DEPENDENCIES = $$dependencies" \
		"LIPO_ARGS = $$lipo_args" \
		"MULTIPLE_ARCHS = $(MULTIPLE_ARCHS)" \
		"SINGLE_ARCH = $(SINGLE_ARCH)" \
		"RC_ARCHS = $(RC_ARCHS)" \
		"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" ; \
	else \
	    $(ECHO) Not configuring library $(NAME). ; \
	fi)

.PHONY : final

final: $(SYMROOT)/$(LIB_NAME)
		
$(SYMROOT)/$(LIB_NAME): $(DEPENDENCIES)
	@($(set_dynamic_flags) ; \
	if [ -n "$(MULTIPLE_ARCHS)" ] ; then \
	    cmd="$(LIPO) -create $(LIPO_ARGS) -o $@" ; \
	else \
	    $(RM) -f $@ ; \
	    cmd="$(TRANSMOGRIFY)  $(SYMROOT)/$(LIBRARY_PREF)$(NAME)$(BUILD_TYPE_SUFFIX).$(SINGLE_ARCH)$$library_ext $@" ; \
	fi ; \
	echo $$cmd ; $$cmd ; \
        if [ "$(PLATFORM_OS)" = "winnt" -a -r $(NAME).def ] ; then \
	   $(RM) -f $(PRODUCT_ROOT)/$(LIBRARY_PREF)$(NAME)$(BUILD_TYPE_SUFFIX).$(SINGLE_ARCH).exp ; \
	   cmd="$(TRANSMOGRIFY) $(SYMROOT)/$(LIBRARY_PREF)$(NAME)$(BUILD_TYPE_SUFFIX).$(SINGLE_ARCH).lib $(SYMROOT)/$(LIBRARY_PREF)$(NAME)$(BUILD_TYPE_SUFFIX).lib" ; \
	   $(ECHO) $$cmd ; $$cmd ; \
        fi ; \
	if [ "$(DELETE_THIN_RESULTS)" = "YES" ] ; then \
	     cmd="$(RM) -f $(DEPENDENCIES)" ; \
	     echo $$cmd ; $$cmd ; \
	fi)


MY_OFILE_LIST = $(OFILE_DIR)/$(NAME).ofileList

# The following needs to detect when the project rule has not been invoked 
# from above as a top-level library project.

project:: initial_targets all_subprojects resources
	@($(set_dynamic_flags) ; \
	$(MAKE) build_product \
		   "PRODUCT = $(SYMROOT)/$(LIBRARY_PREF)$(NAME)$(BUILD_TYPE_SUFFIX).$(TARGET_ARCH)$$library_ext" \
       		   "OFILE_DIR = $(OFILE_DIR)" \
       		   "SYM_DIR = $(SYM_DIR)" \
       		   "DEV_HEADER_DIR_BASE = $(DEV_HEADER_DIR_BASE)" \
		   "CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
		   "BUILD_OFILES_LIST_ONLY = $(BUILD_OFILES_LIST_ONLY)" \
       		   "MAKEFILEDIR = $(MAKEFILEDIR)" \
       		   "DSTROOT = $(DSTROOT)" \
       		   "SRCROOT = $(SRCROOT)" \
       		   "OBJROOT = $(OBJROOT)" \
       		   "SYMROOT = $(SYMROOT)" )

build_product: $(PRODUCT) 

set_extra_libtool_flags = \
	ofileList_flags="-filelist $(MY_OFILE_LIST),$(OFILE_DIR)"

$(PRODUCT): $(PRODUCT_DEPENDS)
	@((cd $(OFILE_DIR) ; $(OFILE_LIST_TOOL) -removePrefix $(OFILE_DIR)/ -removePrefix ../ $(NON_SUBPROJ_OFILES) $(SUBPROJ_OFILELISTS) $(OTHER_OFILES) $(OTHER_GENERATED_OFILES) -o $(NAME).ofileList) ; \
	  $(set_dynamic_link_flags) ; \
	  $(set_extra_libtool_flags) ; \
	  frameworks=`$(FRAMEWORK_TOOL) $(FRAMEWORKS) $(BUILD_TARGET)` ; \
	  if [ "$(BUILD_OFILES_LIST_ONLY)" != "YES" ] ; then \
	     cmd="$(LIBTOOL) $$dynamic_libtool_flags $(OTHER_LIBTOOL_FLAGS) $(PLATFORM_SPECIFIC_LIBTOOL_FLAGS) $(ALL_LDFLAGS) -arch_only $(TARGET_ARCH) -o $@ $$ofileList_flags $$frameworks $(LIBS) $(OTHER_LIBS)" ; \
  	     echo $$cmd ; $$cmd ; \
	  fi)

PROJECT_TYPE_SPECIFIC_GARBAGE = $(SYMROOT)/$(LIBRARY_PREF)$(NAME)*.a $(SYMROOT)/$(LIBRARY_PREF)$(NAME)*.dylib 

projectType_specific_exported_vars = \
	"BUILD_OFILES_LIST_ONLY = $(BUILD_OFILES_LIST_ONLY)" \
 	"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
	"DEV_HEADER_DIR_BASE = $$header_base" \
	"BUILD_OFILES_LIST_ONLY = YES"


ifeq ($(PLATFORM_OS)-$(REINSTALLING), winnt-)
install:: all
	      $(MAKE) reinstall_stripped REINSTALLING=YES
else
install:: all before_install $(OTHER_INSTALL_DEPENDS) common_install
endif

debug_install:: debug before_install $(OTHER_DEBUG_INSTALL_DEPENDS) common_debug_install
profile_install:: profile before_install $(OTHER_PROFILE_INSTALL_DEPENDS) common_profile_install

before_install::

ifeq ("$(PLATFORM_OS)", "winnt")
$(DSTROOT)$(IMPORT_LIBRARY_DIR):
	$(MKDIRS) $(DSTROOT)$(IMPORT_LIBRARY_DIR)

common_install common_debug_install common_profile_install:: installhdrs $(DSTROOT)$(IMPORT_LIBRARY_DIR)
else
common_install common_debug_install common_profile_install:: installhdrs
endif
	@($(process_target_archs) ; \
	if [ "$(BUILD_OFILES_LIST_ONLY)" != "YES" ] ; then \
	    $(set_dynamic_flags) ; \
	    $(set_objdir) ; \
	    libname="$(LIBRARY_PREF)$(NAME)$$build_type_suffix$$library_ext" ; \
	    import_libname="$(LIBRARY_PREF)$(NAME)$$build_type_suffix.lib" ; \
	    $(MKDIRS) $(DSTROOT)$(INSTALLDIR) ; \
	    if [ '(' "$(PLATFORM_OS)" = "nextstep" -o "$(PLATFORM_OS)" = "macos" ')' -a \
		  "$(LIBRARY_STYLE)" != "STATIC" -a \
		  "$(CODE_GEN_STYLE)" = "DYNAMIC" ] ; then \
              dylib_install_name=$(LIBRARY_PREF)$(NAME)$$build_type_suffix.$(DEPLOY_WITH_VERSION_NAME)$$library_ext ; \
 	        installed_library="$(DSTROOT)$(INSTALLDIR)/$$dylib_install_name" ; \
		$(RM) -f $(DSTROOT)$(INSTALLDIR)/$$libname ; \
	        cmd="$(SYMLINK) $$dylib_install_name $(DSTROOT)$(INSTALLDIR)/$$libname" ; \
	        $(ECHO) $$cmd ; $$cmd ; \
	    else \
	        installed_library="$(DSTROOT)$(INSTALLDIR)/$$libname" ; \
	        installed_import_library="$(DSTROOT)$(IMPORT_LIBRARY_DIR)/$$import_libname" ; \
	    fi ; \
	    if [ "$(STRIP_ON_INSTALL)" = "YES" ] ; then \
		strip_install_flags="$(LIBRARY_INSTALL_OPTS)" ;\
	    fi ; \
	    $(RM) -f $$installed_library ; \
	    cmd="$(INSTALL) $$strip_install_flags -m 555 -o $(INSTALL_AS_USER) -g $(INSTALL_AS_GROUP) $(SYMROOT)/$$libname $$installed_library" ; \
	    $(ECHO) $$cmd ; $$cmd || exit 1; \
	    if [ "$(INSTALL_PERMISSIONS)" != "" ] ; then \
		$(ECHO) $(CHMOD) $(INSTALL_PERMISSIONS) $$installed_library ; \
		$(CHMOD) $(INSTALL_PERMISSIONS) $$installed_library ; \
	    fi ; \
	    if [ "$(PLATFORM_OS)" = "winnt" ] ; then \
	      $(RM) -f $$installed_import_library ; \
	      cmd="$(INSTALL) $$strip_install_flags -m 555 -o $(INSTALL_AS_USER) -g $(INSTALL_AS_GROUP) $(SYMROOT)/$$import_libname $$installed_import_library" ; \
	      $(ECHO) $$cmd ; $$cmd || exit 1; \
	      if [ "$(INSTALL_PERMISSIONS)" != "" ] ; then \
		  $(ECHO) $(CHMOD) $(INSTALL_PERMISSIONS) $$installed_import_library ; \
		  $(CHMOD) $(INSTALL_PERMISSIONS) $$installed_import_library ; \
	      fi ; \
	    fi ; \
	fi ; \
	$(MAKE) finalize_install \
		"DSTROOT = $(DSTROOT)" \
		"SRCROOT = $(SRCROOT)" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" \
		"DEVROOT = $(DEVROOT)" \
		"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
		"INSTALLDIR = $(INSTALLDIR)" \
		"PRODUCT_ROOT = $(DSTROOT)$(INSTALLDIR)" \
		"PRODUCT = $$installed_library" \
		"SYM_DIR = $(SYMROOT)/$(DERIVED_SRC_DIR_NAME)" \
		"OFILE_DIR = $(OBJROOT)/$$objdir/$$arch" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" \
		"RC_CFLAGS = $(RC_CFLAGS)" \
		"RC_ARCHS = $$archs" )

-include $(LOCAL_MAKEFILEDIR)/library.make.postamble
