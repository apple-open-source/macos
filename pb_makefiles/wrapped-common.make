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
# wrapped-common.make
#
# Rules and variables common to all projects with wrapped-style products
# (i.e. applications, bundles, etc.).  Projects which include this file
# are assumed to have defined the PRODUCT variable to be the path to the
# directory and the INNER_PRODUCT to be the path to the executable within
# the directory.  Wrapped products consist of a directory containing the
# executable, a Resources, Headers, and PrivateHeaders directories.
#
# IMPORTED VARIABLES
#    none
#
# EXPORTED VARIABLES
#    none
#

WRAPPED = YES

ifeq "YES" "$(JAVA_USED)"

JAVA_INSTALL_DIR = $(subst $(PRODUCT_DIR),$(INSTALLDIR),$(JAVA_RESOURCE_DIR))
JAVA_INSTALL_DIR_CLIENT = $(subst $(PRODUCT_DIR),$(INSTALLDIR),$(JAVA_RESOURCE_DIR_CLIENT))

copy-java-resources: copy-java-classes

ANNOUNCE_COPY_JAVA =
COPY_JAVA_FS = copy-java-fs-wfastcp

endif


# The resource directories are exported so that non-wrapped
# subprojects can copy their resources.
export GLOBAL_RESOURCE_DIR = $(PRODUCT)$(VERSION_SUFFIX)/Resources
export WEBSERVER_RESOURCE_DIR = $(PRODUCT)$(VERSION_SUFFIX)/WebServerResources
export JAVA_RESOURCE_DIR = $(GLOBAL_RESOURCE_DIR)/Java
export JAVA_RESOURCE_DIR_CLIENT = $(WEBSERVER_RESOURCE_DIR)/Java
INFO_FILE = $(GLOBAL_RESOURCE_DIR)/Info-$(PLATFORM_OS).plist
USER_INFO_FILE_NAME = CustomInfo.plist
JAVA_TEMP_FILE = $(SFILE_DIR)/Java.plist
BEFORE_BUILD += create-info-file create-help-file

include $(MAKEFILEDIR)/common.make
-include $(LOCAL_MAKEFILEDIR)/wrapped-common.make.preamble

#
# prebuild rules
#

.PHONY: create-info-file

create-info-file: $(GLOBAL_RESOURCE_DIR) $(INFO_FILE)

ifneq "$(REG_APPEND_FILE)" ""
REG_APPEND_FLAGS = -append $(REG_APPEND_FILE)
endif

ifneq "$(REG_FILE)" ""
$(RESOURCE_OFILE) $(GLOBAL_RESOURCE_DIR)/$(REG_FILE): $(GLOBAL_RESOURCE_DIR) $(OFILE_DIR) PB.project $(WINDOWS_APPICON) $(WINDOWS_DOCICONS)
	$(REGGEN) -o $(OFILE_DIR)/$(RESOURCE_OFILE) $(REG_APPEND_FLAGS) -project PB.project -rc $(RC_CMD) -regFile $(GLOBAL_RESOURCE_DIR)/$(REG_FILE)
endif

# only depend on CustomInfo.plist if it exists
ifneq "" "$(wildcard $(USER_INFO_FILE_NAME))"
OPTIONAL_INFO_FILES += $(USER_INFO_FILE_NAME)
endif

$(INFO_FILE): $(GLOBAL_RESOURCE_DIR) PB.project $(OTHER_INFO_FILES) $(OPTIONAL_INFO_FILES)
ifneq "$(ENABLE_INFO_DICTIONARY)" "NO"
	-$(SILENT) $(RM) -f $(JAVA_TEMP_FILE)
	$(SILENT) $(MKDIRS) $(dir $(JAVA_TEMP_FILE))
	$(SILENT) $(ECHO) "{" >>$(JAVA_TEMP_FILE)
ifeq "YES" "$(JAVA_NEEDED)"
	$(SILENT) $(ECHO) "    NSJavaNeeded = Yes;" >>$(JAVA_TEMP_FILE)
endif
ifneq "" "$(JAVA_USED)"
	$(SILENT) $(ECHO) "    NSJavaRoot = \"$(subst $(PRODUCT)$(VERSION_SUFFIX)/,,$(JAVA_RESOURCE_DIR))\";" >>$(JAVA_TEMP_FILE)
endif
ifneq "" "$(JAVA_PRODUCT)"
	$(SILENT) $(ECHO) "    NSJavaPath = ($(JAVA_PRODUCTS_PATH));" >>$(JAVA_TEMP_FILE)
endif
ifeq "WebObjects" "$(findstring WebObjects, $(PROJECT_TYPE))"
	$(SILENT) $(ECHO) "    NSJavaRootClient = \"$(subst $(PRODUCT)$(VERSION_SUFFIX)/,,$(JAVA_RESOURCE_DIR_CLIENT))\";" >>$(JAVA_TEMP_FILE)
ifeq "JAR" "$(JAVA_ARCHIVE_METHOD)"
	$(SILENT) $(ECHO) "    NSJavaPathClient = \"$(JAVA_JAR_NAME_CLIENT)\";" >>$(JAVA_TEMP_FILE)
endif
endif
	$(SILENT) $(ECHO) "}" >>$(JAVA_TEMP_FILE)
	$(SILENT) if [ -r "$(USER_INFO_FILE_NAME)" ] ; then \
	   mergeInfoArgs="$(USER_INFO_FILE_NAME)" ; \
	fi ; \
	if [ -r "$(JAVA_TEMP_FILE)" ] ; then \
	   mergeInfoArgs="$$mergeInfoArgs $(JAVA_TEMP_FILE)" ; \
	fi ; \
	if [ -n "$(REG_FILE)" ] ; then \
	   mergeInfoArgs="$$mergeInfoArgs -regFile $(REG_FILE)" ; \
	fi ; \
	if [ -x "$(MERGEINFO)$(EXECUTABLE_EXT)" ] ; then \
	   $(RM) -f $(INFO_FILE) ; \
	   cmd="$(MERGEINFO) PB.project $$mergeInfoArgs $(OTHER_INFO_FILES) -o $(INFO_FILE)" ; \
	   $(ECHO) $$cmd ; eval $$cmd ; \
	elif [ -f "$(USER_INFO_FILE_NAME)" ]; then \
       $(RM) -f $(INFO_FILE) ; \
       cmd="$(CP) $(USER_INFO_FILE_NAME) $(INFO_FILE)" ; \
       $(ECHO) $$cmd ; eval $$cmd ; \
 	fi
endif

-include $(LOCAL_MAKEFILEDIR)/wrapped-common.make.postamble
