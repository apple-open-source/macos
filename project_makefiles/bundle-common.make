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
#                              bundle-common.make
#

RESOURCES_ROOT = $(PRODUCT_ROOT)/Resources

include $(MAKEFILEDIR)/common.make

# Override common.make
DEV_HEADER_DIR_BASE = $(TOP_PRODUCT_ROOT)

DYNAMIC_BUILD_TYPE_SUFFIX = ""
DEBUG_BUILD_TYPE_SUFFIX = ""
PROFILE_BUILD_TYPE_SUFFIX = "_profile"

set_header_dirs = \
	$(set_product_root) ; \
        public_header_dir="$(INSTALLDIR)/$$product_root/Headers" ;\
	 private_header_dir="$(INSTALLDIR)/$$product_root/PrivateHeaders"

.PHONY : framework bundle palette debug profile static

framework bundle palette debug profile static kernel::
	@($(check_for_gnumake) ; \
	$(process_target_archs) ; \
	$(set_dynamic_flags) ; \
	$(set_objdir) ; \
	$(set_product_root) ; \
	for arch in $$archs ; do \
	   $(ECHO) == Making target $@ for $(NAME) \($$arch\) == ; \
	   ofile_dir="$(OBJROOT)/`$(ECHO) $$buildtype`_$$objdir/$$arch" ; \
           intermediate_product_name="$(NAME).$$arch" ; \
	   $(MAKE) project \
		"SYM_DIR = $(SYMROOT)/$(DERIVED_SRC_DIR_NAME)" \
		"OFILE_DIR = $$ofile_dir" \
		"BUILD_TYPE_CFLAGS = $($@_target_CFLAGS) $$dynamic_cflags" \
		"BUILD_TYPE_LDFLAGS = $($@_target_LDFLAGS)" \
		"RC_CFLAGS = -arch $$arch $$archless_rcflags" \
		"RC_ARCHS = $$archs" \
		"TARGET_ARCH = $$arch" \
		"ALL_ARCH_FLAGS = $$arch_flags" \
		"BUILD_TARGET = $@" \
		"BUILD_TYPE_SUFFIX = $$build_type_suffix" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" \
		"ONLY_SUBPROJECT = $(ONLY_SUBPROJECT)" \
		"PRODUCT_ROOT = $(SYMROOT)" \
		"TOP_PRODUCT_ROOT = $(SYMROOT)/$$product_root" \
		"IS_TOPLEVEL = YES" \
		"LINK_STYLE = $(LINK_STYLE)" \
		"INSTALLDIR = $(INSTALLDIR)" \
		"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" \
		"DSTROOT = $(DSTROOT)" \
		"SRCROOT = $(SRCROOT)" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" \
              $(stop_if_error_in_arch) ; \
	   if [ -n "$$last_arch" ] ; then \
		multiple_archs=yes ; \
	   fi ; \
	   last_arch=$$arch ; \
	   lists="$$lists $$ofile_dir/$(NAME).ofileList" ; \
        done ; \
	$(MAKE) configure_for_target_archs \
		"TOP_PRODUCT_ROOT = $(SYMROOT)/$$product_root" \
		"PRODUCT_ROOT = $(SYMROOT)" \
		"REL_PRODUCT_ROOT = $$product_root" \
		"OFILE_DIR = $(OBJROOT)/`$(ECHO) $$buildtype`_$$objdir" \
		"BUILD_TYPE_SUFFIX = $$build_type_suffix" \
		"MULTIPLE_ARCHS = $$multiple_archs" \
		"SINGLE_ARCH = $$last_arch" \
		"RC_ARCHS = $$archs" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" \
		"ARCH_SPECIFIC_OFILELISTS = $$lists" \
		"DELETE_THIN_RESULTS = $(DELETE_THIN_RESULTS)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)")

# Current limitation: Nested frameworks and palettes are unsupported.
#    Nested bundles are supported but must have BUNDLE_EXTENSION set.

configure_for_target_archs:: 
	@($(set_product_root) ; \
	complete_prod_root="$(PRODUCT_ROOT)/$$product_root" ; \
	for arch in $(RC_ARCHS) ; do \
	    dependencies="$$dependencies $$complete_prod_root/$(NAME)$(BUILD_TYPE_SUFFIX).$$arch$(BUNDLE_BINARY_EXT)" ;\
	    lipo_args="$$lipo_args -arch $$arch $$complete_prod_root/$(NAME)$(BUILD_TYPE_SUFFIX).$$arch$(BUNDLE_BINARY_EXT)" ; \
	done ; \
	$(set_should_build) ; \
	if [ "$$should_build" = "yes" ] ; then \
	   $(MAKE) final \
		"TOP_PRODUCT_ROOT = $(TOP_PRODUCT_ROOT)" \
		"DEPENDENCIES = $$dependencies" \
		"LIPO_ARGS = $$lipo_args" \
		"COMPLETE_PRODUCT_ROOT = $$complete_prod_root" \
		"BUILD_TYPE_SUFFIX = $(BUILD_TYPE_SUFFIX)" \
		"DELETE_THIN_RESULTS = $(DELETE_THIN_RESULTS)" \
		"MULTIPLE_ARCHS = $(MULTIPLE_ARCHS)" \
		"SINGLE_ARCH = $(SINGLE_ARCH)" \
		"RC_ARCHS = $(RC_ARCHS)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" ; \
	else \
	    $(ECHO) Not configuring $(NAME). ; \
	fi)


.PHONY : final

final: $(COMPLETE_PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX)$(LIBRARY_EXT)

$(COMPLETE_PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX)$(LIBRARY_EXT): $(DEPENDENCIES)
	@(if [ -n "$(MULTIPLE_ARCHS)" ] ; then \
	      cmd="$(LIPO) -create $(LIPO_ARGS) -o $@" ; \
	      $(ECHO) $$cmd ; $$cmd ; \
	  else \
	      $(RM) -f $@ ; \
	      if [ "$(PLATFORM_OS)" = "winnt" ] ; then \
	         $(RM) -f $(COMPLETE_PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX).$(SINGLE_ARCH).exp ; \
	         cmd="$(TRANSMOGRIFY) $(COMPLETE_PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX).$(SINGLE_ARCH)$(BUNDLE_BINARY_EXT) $(COMPLETE_PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX)$(BUNDLE_BINARY_EXT)" ; \
	         $(ECHO) $$cmd ; $$cmd ; \
	      fi ; \
	      if [ -r $(COMPLETE_PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX).$(SINGLE_ARCH)$(LIBRARY_EXT) ] ; then \
		 cmd="$(TRANSMOGRIFY) $(COMPLETE_PRODUCT_ROOT)/$(NAME)$(BUILD_TYPE_SUFFIX).$(SINGLE_ARCH)$(LIBRARY_EXT) $@" ; \
		$(ECHO) $$cmd ; $$cmd ; \
	      fi ; \
	fi ; \
	if [ "$(DELETE_THIN_RESULTS)" = "YES" ] ; then \
	     cmd="$(RM) -f $(DEPENDENCIES)" ; \
	     $(ECHO) $$cmd ; $$cmd ; \
	fi)

.PHONY : project actual_project

project::  
	@($(set_product_root) ; \
	$(set_should_build) ; \
	if [ "$$should_build" = "yes" ] ; then \
	   $(MAKE) actual_project \
		"PRODUCT_ROOT = $(PRODUCT_ROOT)/$$product_root" \
       		"OFILE_DIR = $(OFILE_DIR)" \
       		"SYM_DIR = $(SYM_DIR)" \
		"BUILD_TYPE_CFLAGS = $(BUILD_TYPE_CFLAGS)" \
		"IS_TOPLEVEL = $(IS_TOPLEVEL)" \
		"LINK_STYLE = $(LINK_STYLE)" \
		"BUILD_TARGET = $(BUILD_TARGET)" \
		"ALL_ARCH_FLAGS = $(ALL_ARCH_FLAGS)" \
		"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
       		"DEV_HEADER_DIR_BASE = $(DEV_HEADER_DIR_BASE)" \
		"DEV_PROJECT_HEADER_DIR_BASE = $(DEV_PROJECT_HEADER_DIR_BASE)"\
       		"BUILD_OFILES_LIST_ONLY = $(BUILD_OFILES_LIST_ONLY)" \
		"BUILD_TYPE_SUFFIX = $(BUILD_TYPE_SUFFIX)" \
       		"MAKEFILEDIR = $(MAKEFILEDIR)" \
       		"DSTROOT = $(DSTROOT)" \
       		"SRCROOT = $(SRCROOT)" \
       		"OBJROOT = $(OBJROOT)" \
       		"SYMROOT = $(SYMROOT)" \
		$(extra_actual_project_exported_vars) ; \
	else \
	   $(ECHO) " ..... $(NAME) not built for architecture $(TARGET_ARCH), platform $(PLATFORM_OS)" ; \
	fi)


actual_project:: initial_targets $(PRODUCT_ROOT) $(RESOURCES_ROOT) all_subprojects resources $(PRODUCT)



TEMP_C_FILE = $(OFILE_DIR)/$(NAME)_empty.c
TEMP_O_FILE = $(OFILE_DIR)/$(NAME)_empty.o
SINGLE_O_FILE = $(OFILE_DIR)/$(NAME)_everything.o
MY_OFILE_LIST = $(OFILE_DIR)/$(NAME).ofileList

set_extra_libtool_flags = \
	ofileList_flags="-filelist $(MY_OFILE_LIST),$(OFILE_DIR)"

ofilelist:
	@(cd $(OFILE_DIR) ; $(OFILE_LIST_TOOL) -removePrefix $(OFILE_DIR)/ \
	   -removePrefix ../ $(NON_SUBPROJ_OFILES) $(SUBPROJ_OFILELISTS) \
	   $(OTHER_OFILES) $(OTHER_GENERATED_OFILES) -o $(NAME).ofileList)

$(PRODUCT): $(PRODUCT_DEPENDS) ofilelist $(PRELINK_DEPENDS)
	@($(set_dynamic_link_flags) ; \
	$(set_extra_libtool_flags) ; \
	$(set_product_root); \
	(cd $(OFILE_DIR) ; $(OFILE_LIST_TOOL) -removePrefix $(OFILE_DIR)/ \
	   -removePrefix ../ $(NON_SUBPROJ_OFILES) $(SUBPROJ_OFILELISTS) \
	   $(OTHER_OFILES) $(OTHER_GENERATED_OFILES) -o $(NAME).ofileList) ; \
	if [ "$(OS_PREFIX)" != "PDO_UNIX_" ] ; then \
	    frameworks=`$(FRAMEWORK_TOOL) $(FRAMEWORKS) $(BUILD_TARGET)` ; \
	fi ; \
	libs="$(LIBS:-lsys_s=)" ; \
	if [ "$(LINK_STYLE)" = "ONE_MODULE" ] ; then \
	      num_ofiles=$(NUMBER_OF_OBJECT_FILES) ; \
       	      if [ "$$num_ofiles" != "       0" \
       	         -a "$$num_ofiles" != "0" ] ; then \
	         if [ "$(PLATFORM_OS)" = "winnt" ] ; then \
                    contents=$$ofileList_flags ;\
		 else \
                    contents="$(OFILES) $(OTHER_OFILES) $(OTHER_GENERATED_OFILES)" ; \
		 fi ; \
	      else \
	         $(ECHO) Warning: No object files for $(NAME). ; \
	         $(RM) -f $(TEMP_C_FILE) ; \
	         $(TOUCH) $(TEMP_C_FILE) ; \
	         $(CC) $(ALL_CFLAGS) -c $(TEMP_C_FILE) -o $(TEMP_O_FILE) ; \
	         $(RM) -f $(TEMP_C_FILE) ; \
	         contents="$(TEMP_O_FILE)" ; \
	       fi ; \
	       cmd="$(CC) $$dynamic_bundle_flags -o $@ $(ALL_CFLAGS) $(ALL_LDFLAGS) \
			$$contents $$frameworks $$libs $(OTHER_LIBS)" ; \
   	       $(ECHO) $$cmd ; $$cmd ; \
	else \
	      cmd="$(LIBTOOL) $$dynamic_libtool_flags \
		$(PLATFORM_SPECIFIC_LIBTOOL_FLAGS) $(OTHER_LIBTOOL_FLAGS) \
		$(ALL_LDFLAGS) $(ALL_FRAMEWORK_CFLAGS) -arch_only $(TARGET_ARCH) \
		-o $(@:.lib=.dll) $$ofileList_flags $$frameworks $$libs \
		$(OTHER_LIBS)" ; \
	      $(ECHO) $$cmd ; $$cmd ; \
        fi)
	

before_install:: $(DSTROOT)$(INSTALLDIR)
	@($(set_product_root) ; \
	cmd="$(CHMOD) -R a+w $(DSTROOT)$(INSTALLDIR)/$(NAME).$$framework_ext || true" ; \
	$(ECHO) $$cmd ; eval $$cmd ; \
	cmd="$(RM) -rf $(DSTROOT)$(INSTALLDIR)/$(NAME).$$framework_ext" ; \
	$(ECHO) $$cmd ; $$cmd)

ifeq ($(PLATFORM_OS)-$(REINSTALLING), winnt-)
install:: all
	      $(MAKE) reinstall_stripped REINSTALLING=YES
else
install:: all before_install $(OTHER_INSTALL_DEPENDS)
	@($(set_should_build) ; \
	if [ "$$should_build" = "yes" ] ; then \
	    $(ECHO) == Making install for $(NAME) == ; \
	    $(set_product_root) ; \
	    $(process_target_archs) ; \
	    $(set_dynamic_flags) ; \
	    $(set_objdir) ; \
	    ofile_dir="$(OBJROOT)/`$(ECHO) $$buildtype`_$$objdir" ; \
	    installed_product="$(DSTROOT)$(INSTALLDIR)/$(NAME).$$framework_ext" ;\
	    $(ECHO) "Copying $(NAME).$$framework_ext" ; \
	    (cd $(SYMROOT); $(TAR) cf - $(NAME).$$framework_ext) | (cd     $(DSTROOT)$(INSTALLDIR); $(TAR) xf -) ; \
	    $(MAKE) installhdrs finalize_install \
		"DSTROOT = $(DSTROOT)" \
		"SRCROOT = $(SRCROOT)" \
	       	"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" \
		"SYM_DIR = $(SYMROOT)/$(DERIVED_SRC_DIR_NAME)" \
		"DEVROOT = $(DEVROOT)" \
		"INSTALLDIR = $(INSTALLDIR)" \
		"PRODUCT_ROOT = $(DSTROOT)$(INSTALLDIR)/$$product_root" \
		"PRODUCT = $(DSTROOT)$(INSTALLDIR)/$$product_root/$(NAME)" \
		"OFILE_DIR = $$ofile_dir" \
		"BUILD_TYPE_CFLAGS = $(framework_target_CFLAGS) $$dynamic_cflags" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" \
		"RC_CFLAGS = $$arch_cflags" \
		"RC_ARCHS = $$archs" ; \
	    $(CHMOD) -R a-w $$installed_product || true ; \
	    $(CHOWN) -R $(INSTALL_AS_USER) $$installed_product || true ; \
	    $(CHGRP) -R $(INSTALL_AS_GROUP) $$installed_product || true ; \
	fi)
endif

strip_myself::
	@($(set_product_root) ; \
       	product_root="$(PRODUCT_ROOT)" ; \
	product="$(PRODUCT_ROOT)/$(NAME)$(BUNDLE_BINARY_EXT)" ; \
	$(set_should_build) ; \
	if [ "$$should_build" = "yes" ] ; then \
	   if [ ! -f $$product ] ; then \
       	        product_root="$(PRODUCT_ROOT)/$(NAME).$$framework_ext" ; \
       	        product="$$product_root/$(NAME)$(BUNDLE_BINARY_EXT)" ; \
	   fi ; \
	   $(MAKE) strip_bundle_binary $(finalize_install_exported_vars) \
				       "PRODUCT_ROOT = $$product_root" \
				       "PRODUCT = $$product" ; \
	   product="$$product_root)/$(NAME)$(DEBUG_BUILD_TYPE_SUFFIX)$(BUNDLE_BINARY_EXT)" ; \
	   if [ -f $$product ] ; then \
	       $(MAKE) strip_bundle_binary $(finalize_install_exported_vars) \
				       "PRODUCT_ROOT = $$product_root" \
				       "PRODUCT = $$product" ; \
	   fi ; \
	   product="$$product_root/$(NAME)$(PROFILE_BUILD_TYPE_SUFFIX)$(BUNDLE_BINARY_EXT)" ; \
	   if [ -f $$product ] ; then \
	       $(MAKE) strip_bundle_binary $(finalize_install_exported_vars) \
				       "PRODUCT_ROOT = $$product_root" \
				       "PRODUCT = $$product" ; \
	   fi ; \
	fi)

strip_bundle_binary::
	@(if [ "$(BUILD_OFILES_LIST_ONLY)" != "YES" \
	     -a "$(STRIP_ON_INSTALL)" = "YES" ] ; then \
		if [ "$(CODE_GEN_STYLE)" = "DYNAMIC" ] ; then \
			strip_opts="$(DYNAMIC_STRIP_OPTS)" ; \
		else \
			strip_opts="$(LIBRARY_STRIP_OPTS)" ; \
		fi ; \
		cmd="$(STRIP) $$strip_opts $(PRODUCT)" ; \
	       	$(ECHO) $$cmd ; $$cmd ; \
	fi ; \
	$(CHMOD) -R ugo-w $(PRODUCT) || true ; \
	$(RM) -f $(PRODUCT_ROOT)/$(CHANGES_FILE_BASE).* ; \
	for arch in $(RC_ARCHS) ; do \
	    cmd="$(RM) -f $(PRODUCT_ROOT)/$(NAME)*.$$arch$(BUNDLE_BINARY_EXT) $(PRODUCT_ROOT)/$(NAME)*.$$arch$(LIBRARY_EXT)" ;\
	    $(ECHO) $$cmd ; $$cmd ; \
	done)
