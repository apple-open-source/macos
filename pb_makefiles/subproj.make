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
# subproj.make
#
# Variable definitions and rules for building component subprojects.  A
# component subproject contains code and resources which are needed by the
# parent project, but which have been split off to simplify project management.
#
# PUBLIC TARGETS
#    subproj: synonymous with all
#
# IMPORTED VARIABLES
#    none
#
# EXPORTED VARIABLES
#    none
#

.PHONY: subproj all
subproj: all

# Unlike most project types, the subproject can have two different
# products.  On platforms which support the merging of .o files,
# we will generate a .o file which contains the previous .o files.
# On platforms where this is not supported we will generate an
# ofilelist.  We must specify PRODUCT before we include common.make,
# but will not know which product we are building until after we
# have included common.make.  The solution to the dilemma is to
# define PRODUCT to be a phony target which will build the actual
# product

.PHONY: subproject_product
PRODUCTS = subproject_product
BEFORE_BUILD += create-help-file

include $(MAKEFILEDIR)/common.make
-include $(LOCAL_MAKEFILEDIR)/subproj.make.preamble

O_PRODUCT = $(OFILE_DIR)/../$(SUBDIRECTORY_NAME)_subproj.o
OFILELIST_PRODUCT = $(OFILE_DIR)/../$(SUBDIRECTORY_NAME)_subproj.ofileList
ifeq "YES" "$(LINK_SUBPROJECTS)"
ACTUAL_PRODUCT = $(O_PRODUCT)
else
ACTUAL_PRODUCT = $(OFILELIST_PRODUCT)
endif

subproject_product: $(ACTUAL_PRODUCT)

# unlike other project types, a subproject must
# generate its result even if the build is
# suppressed or if there are no source files

# also unlike other project types, a subproject
# is not built from all $(LOADABLES), just from
# $(OFILES) and $(OFILELISTS)

ifeq "YES" "$(SUPPRESS_BUILD)"
ARTIFICIAL_SUBPROJECT = YES
OFILELISTS = 
endif
ifeq "" "$(filter %.o, $(OFILES))"
ARTIFICIAL_SUBPROJECT = YES
endif

ifeq "YES" "$(ARTIFICIAL_SUBPROJECT)"

build: $(ACTUAL_PRODUCT)

$(O_PRODUCT): $(OFILE_DIR) $(SFILE_DIR)/subproj_scratch_file.c
	$(CC) -c $(SFILE_DIR)/subproj_scratch_file.c -o $(O_PRODUCT)

$(SFILE_DIR)/subproj_scratch_file.c: $(SFILE_DIR)
	$(ECHO) static int x';' > $*.c

$(OFILELIST_PRODUCT): $(OFILE_DIR) $(SFILE_DIR)/subproj_scratch_file.c $(OFILELISTS)
	$(CC) -c $(SFILE_DIR)/subproj_scratch_file.c -o $(OFILE_DIR)/subproj_scratch_file.o
	$(OFILE_LIST_TOOL) $(OFILE_DIR)/subproj_scratch_file.o $(OFILELISTS) -o $(OFILELIST_PRODUCT)

else

$(O_PRODUCT): $(DEPENDENCIES) Makefile
	$(CC) $(ARCHITECTURE_FLAGS) $(ALL_CFLAGS) -nostdlib $(OFILES) $(OFILELISTS) -r -o $(O_PRODUCT)
$(OFILELIST_PRODUCT): $(DEPENDENCIES) Makefile
	$(OFILE_LIST_TOOL) $(OFILES) $(OFILELISTS) -o $(OFILELIST_PRODUCT)

endif

-include $(LOCAL_MAKEFILEDIR)/subproj.make.postamble
