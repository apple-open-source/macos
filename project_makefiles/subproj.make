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
#                               subproj.make
#


all::
	@echo Sorry, you must run make from the top-level project.

RESOURCES_ROOT = $(PRODUCT_ROOT)

include $(MAKEFILEDIR)/common.make
-include $(LOCAL_MAKEFILEDIR)/subproj.make.preamble

PRODUCT = $(PRODUCT_PREFIX:.subproj=_subproj.o)
ENABLE_INFO_DICTIONARY = NO

HELP_OUTPUT_FILE_DIR = $(DERIVED_SRC_DIR)

projectType_specific_exported_vars = \
	"DEV_HEADER_DIR_BASE = $$header_base" \
	"BUILD_OFILES_LIST_ONLY = $(BUILD_OFILES_LIST_ONLY)" \
	"CODE_GEN_STYLE = $(CODE_GEN_STYLE)"

.PHONY : project build_project ofileList link_subproject

project::
	@(arch=$(TARGET_ARCH) ; \
	$(set_should_build) ; \
	if [ "$$should_build" = "no" ] ; then \
	   $(ECHO) " ..... $(NAME) not built for architecture $(TARGET_ARCH), platform $(PLATFORM_OS)" ; \
           $(build_empty) ; \
	else \
	   $(MAKE) build_project \
       		   "OFILE_DIR = $(OFILE_DIR)" \
       		   "SYM_DIR = $(SYM_DIR)" \
		   "PROPOGATED_CFLAGS = $(PROPOGATED_CFLAGS)" \
		   "BUILD_TYPE_SUFFIX = $(BUILD_TYPE_SUFFIX)" \
       		   "DEV_HEADER_DIR_BASE = $(DEV_HEADER_DIR_BASE)" \
		   "DEV_PROJECT_HEADER_DIR_BASE = $(DEV_PROJECT_HEADER_DIR_BASE)" \
       		   "BUILD_OFILES_LIST_ONLY = $(BUILD_OFILES_LIST_ONLY)" \
       		   "MAKEFILEDIR = $(MAKEFILEDIR)" \
       		   "SRCROOT = $(SRCROOT)" \
       		   "OBJROOT = $(OBJROOT)" \
       		   "SYMROOT = $(SYMROOT)" $(stop_if_error_in_name) ; \
	   $(MAKE) ofileList \
	           "MY_OFILES = $(NON_SUBPROJ_OFILES)" \
       		   "OFILE_DIR = $(OFILE_DIR)" \
       		   "SYM_DIR = $(SYM_DIR)" \
       		   "PRODUCT_PREFIX = $(PRODUCT_PREFIX)" \
       		   "VPATH = " \
       		   "MAKEFILEDIR = $(MAKEFILEDIR)" \
       		   "SRCROOT = $(SRCROOT)" \
       		   "OBJROOT = $(OBJROOT)" \
       		   "SYMROOT = $(SYMROOT)" $(stop_if_error_in_name) ; \
	fi)

build_project: initial_targets all_subprojects resources $(PRODUCT)

ofileList:
	@(cd $(OFILE_DIR) ; \
	if [ "$(MAKE_SINGLE_MODULE)" = "YES" ] ; then \
		$(OFILE_LIST_TOOL) -removePrefix ../ $(PRODUCT) $(OTHER_OFILES) $(SUBPROJ_OFILELISTS) -o ../$(NAME)_subproj.ofileList ; \
	else \
		$(OFILE_LIST_TOOL) -removePrefix ../ $(MY_OFILES) $(OTHER_OFILES) $(SUBPROJ_OFILELISTS) -inDirectory $(OFILE_DIR) -o ../$(NAME)_subproj.ofileList ; \
	fi)

$(PRODUCT): $(PRODUCT_DEPENDS)
	@(if [ \( "$(MAKE_SINGLE_MODULE)" != "YES" \
                  -a "$(BUILD_OFILES_LIST_ONLY)" = "YES" \) \
	       -o  "$(PLATFORM_OS)" = "winnt" ] ; then \
             $(build_empty) ; \
        else \
	     if [ "`$(ECHO) $(OFILES) | wc -w`" = "       0" ] ; then \
	        $(ECHO) Warning: Subproject $(NAME) is empty. ; \
                $(build_empty) ; \
	     else \
	        $(MAKE) link_subproject \
       		   "OFILE_DIR = $(OFILE_DIR)" \
       		   "SYM_DIR = $(SYM_DIR)" \
		   "PROPOGATED_CFLAGS = $(PROPOGATED_CFLAGS)" \
       		   "MAKEFILEDIR = $(MAKEFILEDIR)" \
       		   "SRCROOT = $(SRCROOT)" \
       		   "OBJROOT = $(OBJROOT)" \
       		   "SYMROOT = $(SYMROOT)" $(stop_if_error_in_name) ; \
             fi ; \
        fi)

link_subproject: $(OFILES) $(OTHER_OFILES)
	$(CC) $(ALL_CFLAGS) -nostdlib $(OFILES) $(OTHER_OFILES) -r -o $(PRODUCT)

build_empty = \
 	     $(RM) -f $(TEMP_C_FILE) ; \
	     $(TOUCH) $(TEMP_C_FILE) ; \
	     $(CC) $(ALL_CFLAGS) -c $(TEMP_C_FILE) -o  $(PRODUCT_PREFIX:.subproj=_subproj.o) ; \
	     $(RM) -f $(TEMP_C_FILE)

TEMP_C_FILE = $(PRODUCT_PREFIX:.subproj=)_empty.c

PROJECT_TYPE_SPECIFIC_GARBAGE = 

-include $(LOCAL_MAKEFILEDIR)/subproj.make.postamble
