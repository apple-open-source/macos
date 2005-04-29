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
# prebuild.make
#
# Rules for preparing for a build.  This preparation includes exporting
# any public, private, or project header files and ensuring that the
# object and generated-source directories exist.
#
# PUBLIC TARGETS
#    prebuild: copies public and private header files to their
#        final destination and refreshes precomps
#
# IMPORTED VARIABLES
#    BEFORE_PREBUILD: targets to build before installing headers for a subproject
#    AFTER_PREBUILD: targets to build after installing headers for a subproject
#
# EXPORTED VARIABLES
#

.PHONY: prebuild announce-prebuild local-prebuild recursive-prebuild
.PHONY: prebuild-directories export-java-classes
.PHONY: export-project-headers export-public-headers export-private-headers refresh-precomps

#
# Variable definitions
#

ACTUAL_PREBUILD = prebuild-directories export-java-classes export-project-headers export-public-headers export-private-headers delete-precomp-trustfile

#
# First we do local prebuild, then we recurse, then we refresh precomps
#

ifneq "YES" "$(SUPPRESS_BUILD)"
prebuild: local-prebuild recursive-prebuild refresh-precomps
recursive-prebuild: $(ALL_SUBPROJECTS:%=prebuild@%) local-prebuild
$(ALL_SUBPROJECTS:%=prebuild@%): local-prebuild
endif

#
# Local prebuild
#

local-prebuild: announce-prebuild $(BEFORE_PREBUILD) $(ACTUAL_PREBUILD) $(AFTER_PREBUILD)
$(AFTER_PREBUILD): announce-prebuild $(BEFORE_PREBUILD) $(ACTUAL_PREBUILD)
$(ACTUAL_PREBUILD): announce-prebuild $(BEFORE_PREBUILD)
$(BEFORE_PREBUILD): announce-prebuild

#
# before we do anything we announce our intentions
#

announce-prebuild:
ifndef RECURSING
	$(SILENT) $(ECHO) Pre-build setup...
else
	$(SILENT) $(ECHO) $(RECURSIVE_ELLIPSIS)in $(NAME)
endif

#
# Ensure that important directories exist
#

PREBUILD_DIRECTORIES = $(OFILE_DIR) $(SFILE_DIR) $(PRODUCT_DIR)
prebuild-directories: $(PREBUILD_DIRECTORIES)

#
# make sure everyone can see our .java's
#

ifeq "$(JAVA_ENABLED)" "YES"

# Put all the file mappings for all subprojects into a single common
# file mappings file.
JAVA_FILE_MAP = $(JAVA_SRC_DIR)/FileMappings.plist
JAVA_FILE_MAP_CLIENT = $(JAVA_SRC_DIR_CLIENT)/FileMappingsClient.plist

export-java-classes: export-java-classes-server export-java-classes-client

ifneq "" "$(JAVAFILES)"
export-java-classes-server: $(JAVA_SRC_DIR) $(JAVA_OBJ_DIR)
	$(JAVATOOL) $(JAVATOOL_ARGS) $(PB_JAVATOOL_FLAGS) $(OTHER_JAVATOOL_FLAGS) -newer -copy -file_map $(JAVA_FILE_MAP) -java_src $(JAVA_SRC_DIR) -java_obj $(JAVA_OBJ_DIR) $(JAVAFILES)
else
export-java-classes-server:
endif

ifneq "" "$(JAVAFILES_CLIENT)"
export-java-classes-client: $(JAVA_SRC_DIR_CLIENT) $(JAVA_OBJ_DIR_CLIENT)
	$(JAVATOOL) $(JAVATOOL_ARGS_CLIENT) $(PB_JAVATOOL_FLAGS) $(OTHER_JAVATOOL_FLAGS) -newer -copy -file_map $(JAVA_FILE_MAP_CLIENT) -java_src $(JAVA_SRC_DIR_CLIENT) -java_obj $(JAVA_OBJ_DIR_CLIENT) $(JAVAFILES_CLIENT)
else
export-java-classes-client:
endif

else

export-java-classes:

endif

#
# if there are any project headers we must export them
#

ifneq "" "$(PROJECT_HEADERS)$(OTHER_PROJECT_HEADERS)"
export-project-headers: $(PROJECT_HDR_DIR)$(PROJECT_HEADER_DIR_SUFFIX) $(PROJECT_HEADERS) $(OTHER_PROJECT_HEADERS)
	$(SILENT) $(CLONEHDRS) $(PROJECT_HEADERS) $(OTHER_PROJECT_HEADERS) $(PROJECT_HDR_DIR)$(PROJECT_HEADER_DIR_SUFFIX)
endif

#
# if there are any public headers we must export them
#

ifneq "" "$(PUBLIC_HEADERS)$(OTHER_PUBLIC_HEADERS)"
ifneq "" "$(PUBLIC_HDR_DIR)"
export-public-headers: $(PUBLIC_HDR_DIR)$(PUBLIC_HEADER_DIR_SUFFIX) $(PUBLIC_HEADERS) $(OTHER_PUBLIC_HEADERS)
	$(SILENT) $(CLONEHDRS) $(PUBLIC_HEADERS) $(OTHER_PUBLIC_HEADERS) $(PUBLIC_HDR_DIR)$(PUBLIC_HEADER_DIR_SUFFIX)
endif
endif

#
# if there are any private headers we must export them
#

ifneq "" "$(PRIVATE_HEADERS)$(OTHER_PRIVATE_HEADERS)"
ifneq "" "$(PRIVATE_HDR_DIR)"
export-private-headers: $(PRIVATE_HDR_DIR)$(PRIVATE_HEADER_DIR_SUFFIX) $(PRIVATE_HEADERS) $(OTHER_PRIVATE_HEADERS)
	$(SILENT) $(CLONEHDRS) $(PRIVATE_HEADERS)  $(OTHER_PRIVATE_HEADERS) $(PRIVATE_HDR_DIR)$(PRIVATE_HEADER_DIR_SUFFIX)
endif
endif

#
# we delete the trustfile every build in case an external framework's headers
# have been edited since the last build
#

delete-precomp-trustfile:
ifneq "YES" "$(RECURSING)"
	$(RM) -f $(PRECOMP_TRUSTFILE)
endif

#
# Existing precomps must be kept up to date in case a contributing header file changes.
#

refresh-precomps:
ifeq "$(OS)" "MACOS"
ifneq "" "$(strip $(ALL_PRECOMPS))"
endif
ifneq "" "$(strip $(PRECOMPILED_PUBLIC_HEADERS))"
endif
ifneq "" "$(strip $(PRECOMPILED_PRIVATE_HEADERS))"
endif
ifneq "" "$(strip $(PRECOMPILED_PROJECT_HEADERS))"
endif
endif
ifeq "$(OS)" "NEXTSTEP"
ifneq "" "$(strip $(ALL_PRECOMPS))"
	$(SILENT) $(ECHO) refreshing local precomps
	$(SILENT) $(FIXPRECOMPS) -precomps $(ALL_PRECOMPS) -update $(ALL_PRECOMPFLAGS)
endif
ifneq "" "$(strip $(PRECOMPILED_PUBLIC_HEADERS))"
	$(SILENT) $(ECHO) refreshing public precomps
	$(SILENT) $(CD) $(PUBLIC_HDR_DIR) && $(FIXPRECOMPS) -precomps $(PRECOMPILED_PUBLIC_HEADERS:.h=.p) -update $(ALL_PRECOMPFLAGS)
endif
ifneq "" "$(strip $(PRECOMPILED_PRIVATE_HEADERS))"
	$(SILENT) $(ECHO) refreshing private precomps
	$(SILENT) $(CD) $(PRIVATE_HDR_DIR) && $(FIXPRECOMPS) -precomps $(PRECOMPILED_PRIVATE_HEADERS:.h=.p) -update $(ALL_PRECOMPFLAGS)
endif
ifneq "" "$(strip $(PRECOMPILED_PROJECT_HEADERS))"
	$(SILENT) $(ECHO) refreshing project precomps
	$(SILENT) $(CD) $(PROJECT_HDR_DIR) && $(FIXPRECOMPS) -precomps $(PRECOMPILED_PROJECT_HEADERS:.h=.p) -update $(ALL_PRECOMPFLAGS)
endif
endif


#
# rule for making various directories
#

$(OFILE_DIR) $(SFILE_DIR) $(JAVA_OBJ_DIR) $(JAVA_OBJ_DIR_CLIENT) $(JAVA_SRC_DIR) $(JAVA_SRC_DIR_CLIENT) $(PRODUCT_DIR) $(PROJECT_HDR_DIR)$(PROJECT_HEADER_DIR_SUFFIX) $(PUBLIC_HDR_DIR)$(PUBLIC_HEADER_DIR_SUFFIX) $(PRIVATE_HDR_DIR)$(PRIVATE_HEADER_DIR_SUFFIX):
	$(SILENT) $(MKDIRS) $@
