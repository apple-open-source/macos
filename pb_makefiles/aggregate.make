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
# aggregate.make
#
# Variable definitions and rules for building aggregate projects.  An
# aggregate is a project which does not itself contain any code or
# resources, but instead contains a number of related subprojects.  For
# example, if you have a framework, a palette which palettizes the framework
# classes, and some applications which use the palette, then you would
# create an aggregate which contains all of them.
#
# PUBLIC TARGETS
#    aggregate: synonymous with all
#
# IMPORTED VARIABLES
#    none
#
# EXPORTED VARIABLES
#    none
#

.PHONY: aggregate all
aggregate: all
PROJTYPE = AGGREGATE

include $(MAKEFILEDIR)/common.make

-include $(LOCAL_MAKEFILEDIR)/aggregate.make.preamble

#
# ensure that subprojects will have access to each other's products
#

RECURSIVE_CFLAGS += -I$(BUILD_SYMROOT)/Headers -I$(BUILD_SYMROOT)/PrivateHeaders -F$(SYMROOT)
RECURSIVE_LDFLAGS += -F$(SYMROOT) -L$(SYMROOT)

#
# unlike most project types, the aggregate does no processing on its
# own and simply recursively invokes the given target on all subprojects
#

ifneq "YES" "$(SUPPRESS_BUILD)"
all: banner-for-all $(RECURSABLE_DIRS:%=all@%)
clean: banner-for-clean $(RECURSABLE_DIRS:%=clean@%)
mostlyclean: banner-for-mostlyclean $(RECURSABLE_DIRS:%=mostlyclean@%)
depend: banner-for-depend $(RECURSABLE_DIRS:%=depend@%)
prebuild: banner-for-prebuild $(RECURSABLE_DIRS:%=prebuild@%)
build: banner-for-build $(RECURSABLE_DIRS:%=build@%)
installhdrs: banner-for-installhdrs $(RECURSABLE_DIRS:%=installhdrs@%)
ifeq ($(OS)-$(REINSTALLING)-$(STRIP_ON_INSTALL), WINDOWS--YES)
install: all
	$(MAKE) reinstall-stripped REINSTALLING=YES
else
install: banner-for-install $(BEFORE_INSTALL) $(RECURSABLE_DIRS:%=install@%) $(AFTER_INSTALL)
endif
else
all clean mostlyclean depend prebuild build installhdrs install:
endif


$(RECURSABLE_DIRS:%=all@%): banner-for-all
$(RECURSABLE_DIRS:%=clean@%): banner-for-clean
$(RECURSABLE_DIRS:%=mostlyclean@%): banner-for-mostlyclean
$(RECURSABLE_DIRS:%=depend@%): banner-for-depend
$(RECURSABLE_DIRS:%=prebuild@%): banner-for-prebuild
$(RECURSABLE_DIRS:%=build@%): banner-for-build
$(RECURSABLE_DIRS:%=installhdrs@%): banner-for-installhdrs
$(RECURSABLE_DIRS:%=install@%): banner-for-install

BANNER_TARGET = $(subst banner-for-,,$@)
banner-for-%:
	$(SILENT) $(ECHO) == Making $(BANNER_TARGET) for $(TARGET_ARCHS) in $(ALL_SUBPROJECTS) ==

# 
# Special rules for making stripped targets on NT
# Note that this section is duplicated in install.make, and any
# changes made here must be reflected there.  
#

ifeq "$(OS)" "WINDOWS"
include $(MAKEFILEDIR)/reinstall.make
endif

-include $(LOCAL_MAKEFILEDIR)/aggregate.make.postamble

