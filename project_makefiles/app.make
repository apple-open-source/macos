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
#                                  app.make
#

all:: app

PRODUCT = $(PRODUCT_ROOT)/$(PRODUCT_NAME)
PROJECT_TYPE_SPECIFIC_GARBAGE = $(SYMROOT)/$(NAME).app \
				$(SYMROOT)/$(NAME).service \
				$(SYMROOT)/$(NAME).debug \
				$(SYMROOT)/$(NAME).profile

RESOURCES_ROOT = $(PRODUCT_ROOT)/Resources
ifeq ("$(PLATFORM_OS)", "winnt")
RESOURCE_OFILE = appResources.o
REG_FILE = appResources.reg
endif

projectType_specific_exported_vars = \
	"BUILD_OFILES_LIST_ONLY = $(BUILD_OFILES_LIST_ONLY)" \
 	"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
	"DEV_HEADER_DIR_BASE = $$header_base" \
	"PRODUCT_ROOT=$$prod_root/Resources"

ICONHEADER = $(NAME).iconheader
APPICONFLAGS =  -sectcreate __ICON __header $(ICONHEADER) \
		-segprot __ICON r r $(ICONSECTIONS) \
                $(OTHER_ICONSECTIONS)

include $(MAKEFILEDIR)/common.make
-include $(LOCAL_MAKEFILEDIR)/app.make.preamble

# Override this for other types of wrappers, like .service wrappers
APP_WRAPPER_EXTENSION = $@


.PHONY : app debug profile

app debug profile::
	@($(check_for_gnumake) ; \
	$(process_target_archs) ; \
	$(set_dynamic_flags) ; \
	$(set_objdir) ; \
	$(set_should_build) ; \
	for arch in $$archs ; do \
           $(set_should_build) ; \
	   if [ "$$should_build" = "no" ] ; then continue ; fi ; \
	   $(ECHO) == Making $(NAME).$(APP_WRAPPER_EXTENSION) for $$arch == ; \
	   ofile_dir="$(OBJROOT)/`$(ECHO) $$buildtype`_$$objdir/$$arch" ; \
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
		"DEVROOT = $(DEVROOT)" \
		"EXECUTABLE_DIR = $(SYMROOT)/$(DERIVED_SRC_DIR_NAME)" \
		"PRODUCT_NAME = $(NAME).$$arch$(EXECUTABLE_EXT)" \
		"PRODUCT_ROOT = $(SYMROOT)/$(NAME).$(APP_WRAPPER_EXTENSION)" \
		"TOP_PRODUCT_ROOT = $(SYMROOT)/$(NAME).$(APP_WRAPPER_EXTENSION)" \
		"IS_TOPLEVEL = YES" \
		"CODE_GEN_STYLE = $(CODE_GEN_STYLE)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" \
		"DSTROOT = $(DSTROOT)" \
		"SRCROOT = $(SRCROOT)" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" ; \
	   if [ -n "$$lipo_args" ] ; then \
		multiple_archs=yes ; \
	   fi ; \
	   last_arch=$$arch ; \
           lipo_args="$$lipo_args -arch $$arch $(SYMROOT)/$(NAME).$(APP_WRAPPER_EXTENSION)/$(NAME).$$arch$(EXECUTABLE_EXT)";\
	   dependencies="$$dependencies $(SYMROOT)/$(NAME).$(APP_WRAPPER_EXTENSION)/$(NAME).$$arch$(EXECUTABLE_EXT)" ; \
        done ; \
        $(set_should_build) ; \
	 if [ "$$should_build" != "no" ] ; then \
	   $(MAKE) configure_for_target_archs \
		"TOP_PRODUCT_ROOT = $(SYMROOT)/$(NAME).$(APP_WRAPPER_EXTENSION)" \
		"PRODUCT_ROOT = $(SYMROOT)/$(NAME).$(APP_WRAPPER_EXTENSION)" \
		"DEPENDENCIES = $$dependencies" \
		"BUILD_TYPE_SUFFIX = $$build_type_suffix" \
		"OFILE_DIR = $(OBJROOT)/`$(ECHO) $$buildtype`_$$objdir" \
		"MULTIPLE_ARCHS = $$multiple_archs" \
		"SINGLE_ARCH = $$last_arch" \
		"RC_ARCHS = $$archs" \
		"ARCH_SPECIFIC_OFILELISTS = $$lists" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" ; \
	fi)

configure_for_target_archs:: $(TOP_PRODUCT_ROOT)/$(NAME)$(EXECUTABLE_EXT)

$(TOP_PRODUCT_ROOT)/$(NAME)$(EXECUTABLE_EXT): $(DEPENDENCIES)
	@(if [ -n "$(MULTIPLE_ARCHS)" ] ; then \
	    for arch in $(RC_ARCHS) ; do \
		lipo_args="$$lipo_args -arch $$arch $(TOP_PRODUCT_ROOT)/$(NAME).$$arch$(EXECUTABLE_EXT)" ; \
	    done ; \
	    cmd="$(LIPO) -create $$lipo_args -o $@" ; \
	else \
	    $(RM) -f $@ ; \
	    cmd="$(TRANSMOGRIFY) $(TOP_PRODUCT_ROOT)/$(NAME).$(SINGLE_ARCH)$(EXECUTABLE_EXT) $@" ; \
	fi ; \
	$(ECHO) $$cmd ; $$cmd ; \
	if [ "$(DELETE_THIN_RESULTS)" = "YES" ] ; then \
	     cmd="$(RM) -f $(DEPENDENCIES)" ; \
	     $(ECHO) $$cmd ; $$cmd ; \
	fi)

.PHONY : project

project:: initial_targets all_subprojects resources $(PRODUCT)

$(PRODUCT): $(PRODUCT_DEPENDS) $(RESOURCE_OFILE)
	@($(set_dynamic_link_flags) ; \
	frameworks=`$(FRAMEWORK_TOOL) $(FRAMEWORKS) $(BUILD_TARGET)` ; \
	if [ "$(PLATFORM_OS)" = "winnt" ] ; then \
	   (cd $(OFILE_DIR) ; $(OFILE_LIST_TOOL) -removePrefix $(OFILE_DIR)/ -removePrefix ../ $(NON_SUBPROJ_OFILES) $(SUBPROJ_OFILELISTS) $(RESOURCE_OFILE) $(OTHER_OFILES) $(OTHER_GENERATED_OFILES) $$adaptors -o $(NAME).ofileList) ; \
	   cmd="$(CC) $$dynamic_ldflags $(ALL_CFLAGS) $(OBJCFLAG) $(ALL_LDFLAGS) \
		-win -o $(PRODUCT) -filelist $(OFILE_DIR)/$(NAME).ofileList,$(OFILE_DIR)\
		$$adaptors $$frameworks $(LIBS) $(OTHER_LIBS) $(WINDOWS_ENTRY_POINT_LIB)" ; \
	else \
	   cmd="$(CC) $$dynamic_ldflags $(ALL_CFLAGS) $(OBJCFLAG) $(ALL_LDFLAGS) \
		$(PLATFORM_APP_LDFLAGS) -o $(PRODUCT) $(OFILES) $(OTHER_OFILES) $(OTHER_GENERATED_OFILES) \
		$$adaptors $$frameworks $(LIBS) $(OTHER_LIBS)" ; \
        fi ; \
	$(ECHO) $$cmd ; \
	$$cmd)
	

.PHONY : before_install install

before_install:: $(DSTROOT)$(INSTALLDIR)
	@(if [ "$(APP_WRAPPER_EXTENSION)" != "$@" ] ; then \
	     wrapper_ext=$(APP_WRAPPER_EXTENSION) ; \
	else \
	     wrapper_ext=app ; \
	fi ; \
	cmd="$(CHMOD) -R u+w $(DSTROOT)$(INSTALLDIR)/$(NAME).$$wrapper_ext || true" ; \
	echo $$cmd ; eval $$cmd ; \
	cmd="$(RM) -rf $(DSTROOT)$(INSTALLDIR)/$(NAME).$$wrapper_ext" ; \
	echo $$cmd ; $$cmd)

ifeq ($(PLATFORM_OS)-$(REINSTALLING), winnt-)
install:: all
	      $(MAKE) reinstall_stripped REINSTALLING=YES
else
install:: all before_install installhdrs $(OTHER_INSTALL_DEPENDS)
	@($(set_should_build) ; \
	if [ "$$should_build" = "yes" ] ; then \
	    if [ "$(APP_WRAPPER_EXTENSION)" != "install" ] ; then \
	      wrapper_ext=$(APP_WRAPPER_EXTENSION) ; \
	    else \
	      wrapper_ext=app ; \
	    fi ; \
	    $(ECHO) Copying $(NAME).$$wrapper_ext to $(DSTROOT)$(INSTALLDIR) ; \
	    (cd $(SYMROOT); $(TAR) chf - $(NAME).$$wrapper_ext) | (cd $(DSTROOT)$(INSTALLDIR); $(TAR) xf -) ; \
	    if [ "$(INSTALL_PERMISSIONS)" != "" ] ; then \
		cmd="$(CHMOD) $(INSTALL_PERMISSIONS) $(DSTROOT)$(INSTALLDIR)/$(NAME).$$wrapper_ext/$(NAME)$(EXECUTABLE_EXT)" ; \
		$(ECHO) $$cmd ; $$cmd ; \
	    fi ; \
	    $(process_target_archs) ; \
	    $(MAKE) finalize_install \
		"DSTROOT = $(DSTROOT)" \
		"SRCROOT = $(SRCROOT)" \
	       	"OBJROOT = $(OBJROOT)" \
	       	"SYMROOT = $(SYMROOT)" \
		"SYM_DIR = $(SYMROOT)/$(DERIVED_SRC_DIR_NAME)" \
		"DEVROOT = $(DEVROOT)" \
		"INSTALLDIR = $(INSTALLDIR)" \
		"PRODUCT_ROOT = $(DSTROOT)$(INSTALLDIR)/$(NAME).$$wrapper_ext" \
		"PRODUCT = $(DSTROOT)$(INSTALLDIR)/$(NAME).$$wrapper_ext/$(NAME)" \
		"OFILE_DIR = $(OBJROOT)/$$obj_dir" \
		"PROJECT_SPECIFIC_CFLAGS = $(NORMAL_CFLAGS)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" \
		"RC_CFLAGS = $$arch_cflags" \
		"RC_ARCHS = $$archs" ; \
	    $(CHOWN) -R $(INSTALL_AS_USER) $(DSTROOT)$(INSTALLDIR)/$(NAME).$$wrapper_ext || true; \
	    $(CHGRP) -R $(INSTALL_AS_GROUP) $(DSTROOT)$(INSTALLDIR)/$(NAME).$$wrapper_ext || true; \
	    $(CHMOD) -R a-w $(DSTROOT)$(INSTALLDIR)/$(NAME).$$wrapper_ext || true ;\
	fi)
endif


extra_finalize_install_exported_vars = \
	"PRODUCT_ROOT = $(PRODUCT_ROOT)/Resources"

strip_myself::
	@(if [ "$(BUILD_OFILES_LIST_ONLY)" != "YES" \
	     -a "$(STRIP_ON_INSTALL)" = "YES" ] ; then \
	    cmd="$(STRIP) $(APP_STRIP_OPTS) $(PRODUCT)$(EXECUTABLE_EXT)"; \
	    $(ECHO) $$cmd ; $$cmd ; \
	fi ; \
	$(RM) -f $(PRODUCT_ROOT)/$(CHANGES_FILE_BASE).* ; \
	for arch in $(RC_ARCHS) ; do \
	    cmd="$(RM) -f $(PRODUCT).$$arch$(EXECUTABLE_EXT)" ; \
	    $(ECHO) $$cmd ; $$cmd ; \
	done )

-include $(LOCAL_MAKEFILEDIR)/app.make.postamble
