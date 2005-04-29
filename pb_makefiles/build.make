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
# build.make
#
# Rules for building the product.
#
# PUBLIC TARGETS
#    build: copies public and private header files to their
#        final destination
#
# IMPORTED VARIABLES
#    BEFORE_BUILD_RECURSION: targets to make before building subprojects
#    BEFORE_BUILD: targets to make before a build, but after subprojects
#    AFTER_BUILD: targets to make after a build
#
# EXPORTED VARIABLES
#

.PHONY: build announce-build before-build-recursion recursive-build local-build build-directories
.PHONY: copy-local-resources copy-global-resources copy-java-resources build-products build-java-classes

#
# Variable definitions
#

BEFORE_BUILD_RECURSION += $(GENERATED_SRCFILES)
ACTUAL_BUILD = build-directories build-java-classes copy-local-resources copy-global-resources copy-java-resources build-products

#
# First we recurse, then we do local build
#

ifneq "YES" "$(SUPPRESS_BUILD)"
build: announce-build before-build-recursion recursive-build local-build
recursive-build: $(ALL_SUBPROJECTS:%=build@%) announce-build before-build-recursion
local-build: recursive-build $(ALL_SUBPROJECTS:%=build@%) announce-build before-build-recursion
$(ALL_SUBPROJECTS:%=build@%): announce-build before-build-recursion
before-build-recursion: announce-build $(BEFORE_BUILD_RECURSION)
endif

#
# Local build
#

local-build:  $(BEFORE_BUILD) $(ACTUAL_BUILD) $(AFTER_BUILD)
$(AFTER_BUILD): $(BEFORE_BUILD) $(ACTUAL_BUILD)
$(ACTUAL_BUILD): $(BEFORE_BUILD)

#
# before we do anything we announce our intentions
#

announce-build:
ifndef RECURSING
	$(SILENT) $(ECHO) Building...
else
	$(SILENT) $(ECHO) $(RECURSIVE_ELLIPSIS)in $(NAME)
endif

#
# Ensure that important directories exist
#

build-directories: $(OFILE_DIR) $(SFILE_DIR) $(PRODUCT_DIR)

#
# Compile java code
#

ifeq "$(JAVA_ENABLED)" "YES"

JAVAC_SPECFILE = $(MAKEFILEPATH)/Resources/$(basename $(notdir $(JAVAC)))Spec.plist

ALL_JAVAC_FLAGS = $(OPTIONAL_JFLAGS) $(OTHER_JAVAC_FLAGS)

build-java-classes: build-java-classes-server build-java-classes-client

ifneq "" "$(JAVAFILES)"
build-java-classes-server: $(JAVA_SRC_DIR)  $(JAVA_OBJ_DIR)
	$(JAVATOOL) $(JAVATOOL_ARGS) $(PB_JAVATOOL_FLAGS) $(OTHER_JAVATOOL_FLAGS) $(ALL_JAVAC_FLAGS) -newer -build -java_src $(JAVA_SRC_DIR) -java_obj $(JAVA_OBJ_DIR) $(JAVAFILES)
else
build-java-classes-server:
endif

ifneq "" "$(JAVAFILES_CLIENT)"
build-java-classes-client: $(JAVA_SRC_DIR_CLIENT) $(JAVA_OBJ_DIR_CLIENT)
	$(JAVATOOL) $(JAVATOOL_ARGS_CLIENT) $(PB_JAVATOOL_FLAGS) $(OTHER_JAVATOOL_FLAGS) $(ALL_JAVAC_FLAGS) -newer -build -java_src $(JAVA_SRC_DIR_CLIENT) -java_obj $(JAVA_OBJ_DIR_CLIENT) $(JAVAFILES_CLIENT)
else
build-java-classes-client:
endif

else

build-java-classes:
copy-java-resources:

endif

#
# Copying local resources
#
# Note that the LOCALIZED_RESOURCE_DIR and LOCALIZED_RESOURCES variables are defined
# in terms of the current target ($*) and therefore are only meaningful in
# the context of the {Language}.copy-local-resources targets.
#

ifneq "$(DISABLE_RESOURCE_COPYING)" "YES"

LOCALIZED_RESOURCE_DIR = $(GLOBAL_RESOURCE_DIR)/$*.lproj
LOCALIZED_RESOURCES = $(addprefix $*.lproj/, $($*_RESOURCES))

.SUFFIXES: .copy-local-resources

copy-local-resources: $(addsuffix .copy-local-resources, $(LANGUAGES))

$(addsuffix .copy-local-resources, $(LANGUAGES)):
	$(SILENT) if [ -n "$(LOCALIZED_RESOURCES)" ]; then \
	$(ECHO) Copying $* resources... ; \
	$(MKDIRS) $(LOCALIZED_RESOURCE_DIR) ; \
	$(FASTCP) $(LOCALIZED_RESOURCES) $(LOCALIZED_RESOURCE_DIR) ; \
	fi

#
# Copying global resources
#

ifneq "$(GLOBAL_RESOURCES) $(OTHER_RESOURCES)" " "
copy-global-resources: $(GLOBAL_RESOURCE_DIR) $(GLOBAL_RESOURCES) $(OTHER_RESOURCES)
ifneq "$(GLOBAL_RESOURCE_DIR)" ""
	$(SILENT) $(FASTCP) $(GLOBAL_RESOURCES) $(OTHER_RESOURCES) $(GLOBAL_RESOURCE_DIR)
else
	$(SILENT) $(ECHO) Error: Only wrapper-style projects can have resources
	$(SILENT) exit 2
endif
endif
endif

#
# Building products
#

ifeq "YES" "$(BUILD_OFILES_LIST_ONLY)"
build-products: $(DEPENDENCIES)
else
build-products: $(PRODUCTS)
endif

#
# DEF-file generation (used on Windows only)
#

ifeq "WINDOWS" "$(OS)"
TMPFILE = $(OFILE_DIR)/nmtmpfile
GENERATED_DEF_FILE = $(OFILE_DIR)/$(WINDOWS_DEF_FILE)

# We have to strip not only leading underbars but also the @ specifications
# we got when __stdcall was used.  The meaning of @ in the .o and the .def
# are not hte same and if we keep these @ the linker will be confused.

ifneq "$(ENABLE_DEF_FILE_GENERATION)" "NO"
$(WINDOWS_DEF_FILE): $(OFILELISTS) $(OFILES)
	$(SILENT) if [ ! -r "./$(WINDOWS_DEF_FILE)" ] ; then \
		$(ECHO) -n "Generating $(notdir $@)...." ; \
		(cd $(OFILE_DIR); $(DUMP_SYMBOLS) $(OFILES) $(addprefix @,$(OFILELISTS)) | $(EGREP) "(SECT).*(External).*(\|)" | $(EGREP) -v "(__GLOBAL_$I)|(_OBJC_)" | $(SED) "s/ _/ /; s/@[0-9][0-9]*//" > $(TMPFILE)) ; \
		$(ECHO) "LIBRARY $(NAME)$(BUILD_TYPE_SUFFIX).dll" > $(GENERATED_DEF_FILE) ; \
		$(ECHO) "EXPORTS" >> $(GENERATED_DEF_FILE) ; \
		$(AWK) '$$3 == "SECT2" || $$NF ~ /^.objc_c/ {printf "\t%s CONSTANT\n", $$NF; next} $$3 == "SECT1" {printf "\t%s\n", $$NF}' $(TMPFILE) >> $(GENERATED_DEF_FILE) ; \
		$(ECHO) "done"; \
	fi
endif

.PHONY: clean_deffile

clean_deffile:
	$(RM) -f $(GENERATED_DEF_FILE)
endif


#
# Rules for creating directories
#

ifeq "PDO_UNIX" "$(PLATFORM_TYPE)" 
$(GLOBAL_RESOURCE_DIR) $(JAVA_RESOURCE_DIR) $(JAVA_RESOURCE_DIR_CLIENT):
else
$(OFILE_DIR) $(SFILE_DIR) $(JAVA_SRC_DIR) $(JAVA_SRC_DIR_CLIENT) $(JAVA_OBJ_DIR) $(JAVA_OBJ_DIR_CLIENT) $(PRODUCT_DIR) $(GLOBAL_RESOURCE_DIR) $(JAVA_RESOURCE_DIR) $(JAVA_RESOURCE_DIR_CLIENT):
endif
	$(SILENT) $(MKDIRS) $@

