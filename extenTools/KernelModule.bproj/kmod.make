#
# kmod.make
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
# Variable definitions and rules for building kernel relocatable projects.  A
# kmod is a fragment of code that can be loaded into the kernel
# as part of a IOKit project.
#
# PUBLIC TARGETS
#    kmod: synonymous with all
#
# IMPORTED VARIABLES
#    none
#
# EXPORTED VARIABLES
#    none
#

.PHONY: kmodule all

kmodule: all
PROJTYPE = KMOD

ifeq "" "$(PRODUCT)"
    PRODUCT = $(PRODUCT_DIR)/$(NAME)$(EXECUTABLE_EXT)
endif

PRODUCTS = $(PRODUCT)
STRIPPED_PRODUCTS = $(PRODUCT)

CREATE_KMOD_INFO = CreateKModInfo.perl

ifneq "NO" "$(GEN_KMOD_INFO)"
    KMOD_INFO = $(SFILE_DIR)/$(NAME)_info.c
    OTHER_GENERATED_SRCFILES += $(KMOD_INFO)
    OTHER_GENERATED_OFILES += $(NAME)_info.o
endif


KINCVERS = A
KERNEL_FRAMEWORK = $(SYSTEM_LIBRARY_DIR)/Frameworks/Kernel.framework
KERNEL_HEADERS = $(NEXT_ROOT)$(KERNEL_FRAMEWORK)/Headers
KERNEL_HDR_INSTALLDIR = $(KERNEL_FRAMEWORK)/Versions/$(KINCVERS)/Headers

ifneq "" "$(wildcard $(KERNEL_HEADERS))"
    KERNEL_HEADERS_INCLUDES = -I$(KERNEL_HEADERS) -I$(KERNEL_HEADERS)/bsd
endif

#
# Find the Resource root.
#
PROJTYPE_RESOURCES = ProjectTypes/KernelModule.projectType/Resources

ifneq "" "$(wildcard $(LOCAL_DEVELOPER_DIR)/$(PROJTYPE_RESOURCES))"
KM_RESOURCE_ROOT=$(NEXT_ROOT)$(LOCAL_DEVELOPER_DIR)/$(PROJTYPE_RESOURCES)
else

ifneq "" "$(wildcard $(HOME)/Library/$(PROJTYPE_RESOURCES))"
KM_RESOURCE_ROOT=$(HOME)/Library/$(PROJTYPE_RESOURCES)
else
KM_RESOURCE_ROOT=$(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/$(PROJTYPE_RESOURCES)
endif
endif

i386_PROJTYPE_CFLAGS = 
ppc_PROJTYPE_CFLAGS = -finline -fno-keep-inline-functions \
		      -force_cpusubtype_ALL \
		      -msoft-float -mcpu=604 -mlong-branch 
ARCH_PROJTYPE_FLAGS = $($(CURRENT_ARCH)_PROJTYPE_CFLAGS) \
		      $(OTHER_$(CURRENT_ARCH)_CFLAGS) 

PROJTYPE_CFLAGS = -nostdinc $(KERNEL_HEADERS_INCLUDES) \
		  $(ARCH_PROJTYPE_FLAGS) -static \
		  -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE \
		  -DAPPLE -DNeXT 

PROJTYPE_CCFLAGS = -x c++ -fno-rtti -fno-exceptions -fcheck-new -fvtable-thunks

PROJTYPE_LDFLAGS = -r -static

KEXT_LIB_CC = -lcc_kext

ifndef LOCAL_MAKEFILEDIR
    LOCAL_MAKEFILEDIR = $(LOCAL_DEVELOPER_DIR)/Makefiles/pb_makefiles
endif
-include $(LOCAL_MAKEFILEDIR)/kmod.make.preamble

include $(MAKEFILEDIR)/common.make

#
# Subtle combination of files and libraries make up the C++ runtime system for
# kernel modules.  We are dependant on the KernelModule kmod.make and
# CreateKModInfo.perl scripts to be exactly instep with both this library
# module and the libkmod module as well.
#
# If you do any maintenance on any of the following files make sure great
# care is taken to keep them in Sync.
#    extenTools/KernelModule.bproj/kmod.make
#    extenTools/KernelModule.bproj/CreateKModInfo.perl
#    xnu/libkern/kmod/cplus_start.c
#    xnu/libkern/kmod/cplus_start.c
#    xnu/libkern/kmod/c_start.c
#    xnu/libkern/kmod/c_stop.c
#
# The trick is that the linkline links all of the developers modules.
# If any static constructors are used .constructors_used will be left as
# an undefined symbol.  This symbol is exported by the cplus_start.c routine
# which automatically brings in the appropriate C++ _start routine.  However
# the actual _start symbol is only required by the kmod_info structure that
# is created and initialized by the CreateKModInfo.perl script.  If no C++
# was used the _start will be an undefined symbol that is finally satisfied
# by the c_start module in the kmod library.
# 
# The linkline must look like this.
#    *.o -lkmodc++ kmod_info.o -lkmod
#
KMOD_INFO_OFILE = $(OFILE_DIR)/$(NAME)_info.o
NON_INFO_OFILES = $(filter-out %/$(NAME)_info.o, $(LOADABLES))

$(PRODUCT): $(DEPENDENCIES)
	$(SILENT) non_info=`$(ECHO) '$(LOADABLES)' \
	    | $(SED) -e 's|$(KMOD_INFO_OFILE)||'`; \
	cmd="$(LD) -o $(PRODUCT) -nostdlib $(ALL_LDFLAGS) $(ARCHITECTURE_FLAGS)\
	    $$non_info -lkmodc++ $(KMOD_INFO_OFILE) -lkmod $(KEXT_LIB_CC)"; \
	$(ECHO) $$cmd; eval "$$cmd"

#
# Make the global kmod_info structure.
# Run CreateKModInfo script to generate the info file.
#
$(KMOD_INFO):
	$(SILENT) $(ECHO) Creating $@ from `pwd`/CustomInfo.xml; \
	$(KM_RESOURCE_ROOT)/$(CREATE_KMOD_INFO) > $(KMOD_INFO) \
            || ($(RM) $(KMOD_INFO); false)

-include $(LOCAL_MAKEFILEDIR)/kmod.make.postamble
