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
# depend.make
#
# Rules for updating dependencies
#
# PUBLIC TARGETS
#	depend: builds individual dependency files and consolidates them into
#	  Makefile.dependencies.
#
# IMPORTED VARIABLES
#	AUTOMATIC_DEPENDENCY_INFO: if YES, then the dependency file is
#	  updated every time the project is built.  If NO, the dependency
#	  file is only built when the depend target is invoked.
#	  This flag is set to YES by Makefile.dependencies, so once you
#	  have done a make depend, the dependency file will always remain
#	  up to date.
#	BEFORE_DEPEND: targets to build before building dependencies for a
#	  subproject
#	AFTER_DEPEND: targets to build after building dependencies for a
#	  subproject
#

-include $(SFILE_DIR)/Makefile.dependencies

#
# Implicit rule that creates a dependency file from a source file
#
.SUFFIXES: .d .dd

.c.d:
	$(UNDEPENDED_CC) $(ALL_CFLAGS) -MM $< > $(SFILE_DIR)/$(*F).d

.m.d:
	$(UNDEPENDED_CC) $(ALL_MFLAGS) -MM $< > $(SFILE_DIR)/$(*F).d

.C.d .cc.d .cxx.d .cpp.d:
	$(UNDEPENDED_CC) $(ALL_CCFLAGS) -MM $< > $(SFILE_DIR)/$(*F).d

.M.d:
	$(UNDEPENDED_CC) $(ALL_MMFLAGS) -MM $< > $(SFILE_DIR)/$(*F).d

.d.dd:
	$(SED) 's/\.o[ :][ :]*/.d : /' < $< > $(SFILE_DIR)/$(*F).dd

ifeq "YES" "$(AUTOMATIC_DEPENDENCY_INFO)"
AFTER_BUILD += update-dependencies
endif

.PHONY: local-build depend local-depend recursive-depend announce-depend \
	update-dependencies recompute-dependencies remove-Makefile.dependencies

#
# Variable definitions
#

ACTUAL_DEPEND = recompute-dependencies
UNDEPENDED_CC := $(CC)

# By asking gcc to generate dependency info as it compiles, we avoid the
# need to do this explicitly using the .m.d rule above.  This optimization
# only works if the .d file already exists when the build first kicks off.
# Thus, the .d file will require two compiler invocations the first time 
# the file is compiled after a clean, but we can piggypack on the .o rule
# thereafter.
ifeq "YES" "$(AUTOMATIC_DEPENDENCY_INFO)"
CC = $(RM) -rf $(SFILE_DIR)/$*.d && DEPENDENCIES_OUTPUT=$(SFILE_DIR)/$*.d $(UNDEPENDED_CC)
endif

DFILES = $(CLASSES:.m=.d) $(MFILES:.m=.d) $(CFILES:.c=.d) $(CAPCFILES:.C=.d)  $(CXXFILES:.cxx=.d) $(CPPFILES:.cpp=.d) $(CAPMFILES:.M=.d) $(CCFILES:.cc=.d)

DDFILES = $(DFILES:.d=.dd)

#
# First we update local dependencies, then we recurse.
#

depend: prebuild local-depend recursive-depend
recursive-depend: $(ALL_SUBPROJECTS:%=depend@%) local-depend
$(ALL_SUBPROJECTS:%=depend@%): local-depend

#
# Local dependencies update
#

local-depend: announce-depend $(BEFORE_DEPEND) $(ACTUAL_DEPEND) $(AFTER_DEPEND)
$(BEFORE_DEPEND): announce-depend
$(ACTUAL_DEPEND): announce-depend $(BEFORE_DEPEND)
$(AFTER_DEPEND): announce-depend $(BEFORE_DEPEND) $(ACTUAL_DEPEND)

#
# Before we do anything we must announce our intentions.
# We don't announce our intentions if we were hooked in from the
# post-depend recursion, since the post-depend has already been announced.
#

announce-depend:
ifndef RECURSING
	$(SILENT) $(ECHO) Updating Makefile.dependencies...
else
	$(SILENT) $(ECHO) $(RECURSIVE_ELLIPSIS)in $(NAME)
endif

#
# We may want to delete the old dependencies file before creating a new
# one in case some dependencies in the old file must be deleted.
#

recompute-dependencies: remove-Makefile.dependencies Makefile.dependencies
update-dependencies: Makefile.dependencies

#
# Remove Makefile dependencies
#

remove-Makefile.dependencies:
	$(RM) -f $(SFILE_DIR)/Makefile.dependencies

remove-dependency-info:
	$(CD) $(SFILE_DIR) && $(RM) -f $(DFILES) $(DDFILES)

#
# Update Makefile.dependencies
#

Makefile.dependencies: $(DFILES) $(DDFILES)
ifneq "$(words $(DFILES) $(DDFILES))" "0"
	$(ECHO) AUTOMATIC_DEPENDENCY_INFO=YES > $(SFILE_DIR)/$(@F)
	$(CAT) $(DFILES) $(DDFILES) | $(SED) /=/d >> $(SFILE_DIR)/$(@F)
endif

