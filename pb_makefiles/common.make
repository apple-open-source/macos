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



# Turn on the use of the NeXT/Apple make hacks to support the pb_makefiles.
USE_APPLE_PB_SUPPORT = all
export USE_APPLE_PB_SUPPORT


#
# Top-Level Rule
#

.PHONY: all

ifeq "AGGREGATE" "$(PROJTYPE)"
all:
else
all: build
endif

#
# Local site-wide Makefile customization
#

ifndef LOCAL_MAKEFILEDIR
        LOCAL_MAKEFILEDIR = $(NEXT_ROOT)$(LOCAL_DEVELOPER_DIR)/Makefiles/pb_makefiles
endif

-include $(LOCAL_MAKEFILEDIR)/common.make.preamble

#
# merging OS-specific variables
#

ifndef INSTALLDIR
INSTALLDIR = $($(PLATFORM_TYPE)_INSTALLDIR)
endif

ifndef DYLIB_ROOT
ifeq "SOLARIS" "$(OS)"
DYLIB_ROOT = /opt/Apple
endif
endif

#
# Localization variables
#
# Under the old localization scheme, a project defined LANGUAGE to name the
# language that was being built and LOCAL_RESOURCES and HELP_FILES to name the 
# resources used by that language.  The new scheme builds all languages
# simultaneously, and requires that the project define LANGUAGES to name all
# languages and English_RESOURCES, English_HELP_FILES, French_RESOURCES
# French_HELP_FILES, etc. to define the resources needed by each language.
#
# The following variable definitions allow projects that have not yet been
# updated to the new scheme to continue to build.
#

ifndef $(LANGUAGE)_HELP_FILES
$(LANGUAGE)_HELP_FILES = $(HELP_FILES)
endif
ifndef $(LANGUAGE)_RESOURCES
$(LANGUAGE)_RESOURCES = $(LOCAL_RESOURCES)
endif
ifndef LANGUAGES
LANGUAGES = $(LANGUAGE)
endif

#
# Miscellaneous variables
#

SILENT = @
ARCH = $(shell $(ARCH_CMD))
ifndef RC_ARCHS
RC_ARCHS = $(ARCH)
endif
TARGET_ARCHS = $(RC_ARCHS)
MAKEFILE_SOURCE = pb_makefiles

ifeq "WINDOWS" "$(OS)"

ifndef LINK_SUBPROJECTS
LINK_SUBPROJECTS = NO
endif
DISABLE_OBJCPLUSPLUS = NO
EXECUTABLE_EXT = .exe
ifeq "STATIC" "$(LIBRARY_STYLE)"
LIBRARY_EXT = .lib
else
LIBRARY_EXT = .dll
endif
LIBRARY_PREFIX =
DLL_EXT = .dll
EXP_EXT = .exp
NULL = NUL
ifneq "$(LIBRARY_STYLE)" "STATIC"
WINDOWS_DEF_FILE = $(NAME)$(BUILD_TYPE_SUFFIX).def 
endif

else

ifndef LINK_SUBPROJECTS
LINK_SUBPROJECTS = YES
endif
EXECUTABLE_EXT = 
LIBRARY_PREFIX = lib

ifeq "SOLARIS" "$(OS)"
ifeq "STATIC" "$(LIBRARY_STYLE)"
LIBRARY_EXT = .a
else
LIBRARY_EXT = .so
endif
else
ifeq "HPUX" "$(OS)"
ifeq "STATIC" "$(LIBRARY_STYLE)"
LIBRARY_EXT = .a
else
LIBRARY_EXT = .sl
endif
else
ifeq "STATIC" "$(LIBRARY_STYLE)"
LIBRARY_EXT = .a
else
LIBRARY_EXT = .dylib
endif
endif
endif

DLL_EXT =
NULL = /dev/null

endif

PROFILE_SUFFIX = _profile
ifndef DEBUG_SUFFIX
DEBUG_SUFFIX = _debug
endif

ifneq "NO" "$(INCLUDE_COMPATIBILITY_MAKEFILE)"
include $(MAKEFILEDIR)/compatibility.make
endif

#
# Roots
#

SRCROOT := $(shell pwd)
# BUILD_SYMROOT is defined below under Directories for derived files
ifndef OBJROOT
OBJROOT = $(BUILD_SYMROOT)
endif
# BUILD_OUTPUT_DIR is defined below under Directories
ifndef SYMROOT
SYMROOT = $(BUILD_OUTPUT_DIR)
endif

#
# Directories
#

ifneq "" "$($(PLATFORM_TYPE)_BUILD_OUTPUT_DIR)"
BUILD_OUTPUT_DIR = $($(PLATFORM_TYPE)_BUILD_OUTPUT_DIR)
endif
ifeq "" "$(BUILD_OUTPUT_DIR)"
BUILD_OUTPUT_DIR = $(SRCROOT)
endif

#
# Java transformations
#
## NOTE: This is OFF by default so OpenStep apps aren't affected.
ifeq "$(JAVA_IS_CLIENT_SIDE)" "YES"
JAVA_CLASSES_CLIENT := $(JAVA_CLASSES)
endif

## NOTE: This is ON by default so OpenStep apps do compile for server-side.
ifeq "$(JAVA_IS_SERVER_SIDE)" "NO"
override JAVA_CLASSES = 
endif

# Directories for derived files
PRODUCT_DIR = $(SYMROOT)
ifeq "" "$(TOPLEVEL_NAME)"
TOPLEVEL_NAME = $(NAME)
endif
export TOPLEVEL_NAME

BUILD_SYMROOT = $(SYMROOT)/$(TOPLEVEL_NAME).build

OFILE_DIR = $(OBJROOT)/objects$(OFILE_DIR_SUFFIX)
SFILE_DIR = $(BUILD_SYMROOT)/derived_src

JAVA_SRC_DIR = $(BUILD_SYMROOT)/derived_src/java
JAVA_SRC_DIR_CLIENT = $(JAVA_SRC_DIR)_client
JAVA_OBJ_DIR = $(OBJROOT)/java_classes
JAVA_OBJ_DIR_CLIENT = $(JAVA_OBJ_DIR)_client

PROJECT_HDR_DIR = $(BUILD_SYMROOT)/ProjectHeaders
ifneq "" "$(PUBLIC_HDR_DIR)"
   PUBLIC_HDR_DIR = $(BUILD_SYMROOT)/Headers/$(notdir $(PUBLIC_HDR_INSTALLDIR))
endif
ifeq "YES" "$(WRAPPED)"
ifeq "APP" "$(PROJTYPE)"
   PUBLIC_HDR_DIR = $(BUILD_SYMROOT)/Headers
else
   PUBLIC_HDR_DIR = $(PRODUCT)/Headers
endif
endif
ifneq "" "$(PRIVATE_HDR_DIR)"
   PRIVATE_HDR_DIR = $(BUILD_SYMROOT)/PrivateHeaders/$(notdir $(PRIVATE_HDR_INSTALLDIR))
endif
ifeq "YES" "$(WRAPPED)"
   PRIVATE_HDR_DIR = $(PRODUCT)/PrivateHeaders
endif
ifeq "" "$(PRECOMP_TRUSTFILE)"
PRECOMP_TRUSTFILE = $(SFILE_DIR)/TrustedPrecomps.txt
endif
export PUBLIC_HDR_DIR
export PRIVATE_HDR_DIR
export PUBLIC_HDR_INSTALLDIR
export PRIVATE_HDR_INSTALLDIR
export PRECOMP_TRUSTFILE

#
# File Lists
#

VPATH = $(OFILE_DIR) $(SFILE_DIR) $(LANGUAGE).lproj
GENERATED_SRCFILES = $(MSGFILES:%.msg=%Speaker.h) $(MSGFILES:%.msg=%Speaker.m) $(MSGFILES:%.msg=%Listener.h) $(MSGFILES:%.msg=%Listener.m) $(DEFSFILES:%.defs=%.h) $(DEFSFILES:%.defs=%User.c) $(DEFSFILES:%.defs=%Server.c) $(MIGFILES:%.mig=%.h) $(MIGFILES:%.mig=%User.c) $(MIGFILES:%.mig=%Server.c) $(PSWFILES:.psw=.h) $(PSWFILES:.psw=.c) $(PSWMFILES:.pswm=.h) $(PSWMFILES:.pswm=.m) $(subst .x_svc,.c, $(subst .x_clnt,.c, $(subst .x_xdr,.c, $(subst .x,.h, $(RPCFILES))))) $(YFILES:.y=.c) $(YFILES:.y=.h) $(LFILES:.l=.c) $(YMFILES:.ym=.m) $(YMFILES:.ym=.h) $(LMFILES:.lm=.m) $(OTHER_GENERATED_SRCFILES)

SRCFILES = PB.project $(JAVA_CLASSES) $(JAVA_CLASSES_CLIENT) $(OTHERSRCS) $(HFILES) $(CLASSES) $(MFILES) $(CFILES) $(CAPCFILES) $(CAPMFILES) $(CCFILES) $(CPPFILES) $(CXXFILES) $(YFILES) $(LFILES) $(YMFILES) $(LMFILES) $(PSWFILES) $(PSWMFILES) $(WOSFILES) $(foreach L,$(LANGUAGES),$(addprefix $(L).lproj/,$($(L)_RESOURCES))) $(GLOBAL_RESOURCES) $(foreach L,$(LANGUAGES),$(addprefix $(L).lproj/,$($(L)_HELP_FILES))) $(OTHERLINKED) $(OTHER_SOURCEFILES)

LOCAL_OFILES = $(addsuffix .o, $(basename $(CLASSES) $(MFILES) $(CFILES) $(CAPCFILES) $(CAPMFILES) $(CCFILES) $(CPPFILES) $(CXXFILES) $(PSWFILES) $(PSWMFILES))) $(PROJTYPE_OFILES) $(OTHERLINKEDOFILES) $(OTHER_OFILES) $(OTHER_GENERATED_OFILES)

GENERATED_JAVA_CLASSES += $(OTHER_GENERATED_JAVA_CLASSES)
GENERATED_JAVA_CLASSES_CLIENT += $(OTHER_GENERATED_JAVA_CLASSES_CLIENT)

JAVAFILES = $(strip $(JAVA_CLASSES) $(GENERATED_JAVA_CLASSES))
JAVAFILES_CLIENT = $(strip $(JAVA_CLASSES_CLIENT) $(GENERATED_JAVA_CLASSES_CLIENT))

ifeq "YES" "$(LINK_SUBPROJECTS)"
 SUBPROJ_OFILES = $(addsuffix _subproj.o,$(SUBPROJECTS))
 OFILES = $(LOCAL_OFILES) $(SUBPROJ_OFILES)
 LOCAL_OFILELISTS =
else
 OFILES = $(LOCAL_OFILES)
 SUBPROJ_OFILELISTS = $(addsuffix _subproj.ofileList,$(SUBPROJECTS))
 LOCAL_OFILELISTS = $(SUBPROJ_OFILELISTS)
endif

LOCAL_OFILELISTS += $(OTHER_OFILELISTS)
ifneq "" "$(strip $(LOCAL_OFILELISTS))"
 OFILELISTS = $(NAME).ofileList
endif

ALL_PRECOMPS = $(PRECOMPILED_HEADERS:.h=.p) $(PRECOMPS)
PRECOMPILED_PUBLIC_HEADERS = $(foreach X, $(PRECOMPILED_HEADERS), $(findstring $(X), $(PUBLIC_HEADERS)))
PRECOMPILED_PRIVATE_HEADERS = $(foreach X, $(PRECOMPILED_HEADERS), $(findstring $(X), $(PRIVATE_HEADERS)))
PRECOMPILED_PROJECT_HEADERS = $(foreach X, $(PRECOMPILED_HEADERS), $(findstring $(X), $(PROJECT_HEADERS)))


ifeq "$(BUILD_TYPE)" "normal"

FRAMEWORK_FLAGS = $(FRAMEWORKS)
PROJTYPE_FRAMEWORK_FLAGS = $(PROJTYPE_FRAMEWORKS)
OTHER_FRAMEWORK_FLAGS = $(OTHER_FRAMEWORKS)
OPTIONAL_FRAMEWORK_FLAGS = $(OPTIONAL_FRAMEWORKS)

else

FRAMEWORK_FLAGS = $(shell $(FRAMEWORK_TOOL) $(FRAMEWORKS) $(BUILD_TYPE))
PROJTYPE_FRAMEWORK_FLAGS = $(shell $(FRAMEWORK_TOOL) $(PROJTYPE_FRAMEWORKS) $(BUILD_TYPE))
OTHER_FRAMEWORK_FLAGS = $(shell $(FRAMEWORK_TOOL) $(OTHER_FRAMEWORKS) $(BUILD_TYPE))
OPTIONAL_FRAMEWORK_FLAGS = $(shell $(FRAMEWORK_TOOL) $(OPTIONAL_FRAMEWORKS) $(BUILD_TYPE))

endif

DEPENDENCIES = $(ARCH_SPECIFIC_OFILES) $(OFILES) $(OFILELISTS) $(OTHER_PRODUCT_DEPENDS)

ifeq "$(LIBRARY_STYLE)" "STATIC"
LOADABLES = $(OFILELISTS:%=-filelist %) $(OFILES) $(LIBS) $(PROJTYPE_LIBS) $(OTHER_LIBS) $(OPTIONAL_LIBS)
else
LOADABLES = $(OFILELISTS:%=-filelist %) $(OFILES) $(LIBS) $(FRAMEWORK_FLAGS) $(PROJTYPE_LIBS) $(PROJTYPE_FRAMEWORK_FLAGS) $(OTHER_LIBS) $(OTHER_FRAMEWORK_FLAGS) $(OPTIONAL_LIBS) $(OPTIONAL_FRAMEWORK_FLAGS)
endif

GARBAGE = $(OBJROOT)/objects-* $(OBJROOT)/obj-* $(SFILE_DIR) $(PROJECT_HDR_DIR) $(PROJTYPE_GARBAGE) $(OTHER_GARBAGE) $(PRECOMPILED_HEADERS:.h=.p) $(PRECOMPS) $(JAVA_SRC_DIR) $(JAVA_SRC_DIR_CLIENT) $(JAVA_OBJ_DIR) $(JAVA_OBJ_DIR_CLIENT)

include $(MAKEFILEDIR)/versions.make
include $(MAKEFILEDIR)/commands-$(OS).make

# if the Makefile has defined a compiler for a specific platform,
# use that one instead of the one found in commands-$(OS).make
ifneq "$($(PLATFORM_TYPE)_OBJCPLUS_COMPILER)" ""
CC = $($(PLATFORM_TYPE)_OBJCPLUS_COMPILER)
endif
ifneq "$($(PLATFORM_TYPE)_OBJCPLUS_COMPILER)" ""
JAVAC = $($(PLATFORM_TYPE)_JAVA_COMPILER)
endif

ifneq "$(findstring java,$(JAVAFILES)$(JAVAFILES_CLIENT))" ""
JAVA_ENABLED = YES
JAVA_USED = YES
endif

ifndef JAVA_NEEDED
JAVA_NEEDED = $(JAVA_AWARE)
endif

ifeq "YES" "$(JAVA_USED)"
ifeq "" "$(JAVA_NEEDED)"
JAVA_NEEDED = YES
endif
endif

include $(MAKEFILEDIR)/flags.make
include $(MAKEFILEDIR)/recursion.make
include $(MAKEFILEDIR)/implicitrules.make

#
# In general, no rules should be placed above the include lines above, to
# ensure that when the rule is stored, any variables in that rule or its
# dependencies have been fully defined.  For example, OFILE_DIR may change
# in flags.make.
#

$(OFILELISTS): $(LOCAL_OFILELISTS) Makefile
	$(OFILE_LIST_TOOL) -o $(OFILE_DIR)/$(NAME).ofileList $(LOCAL_OFILELISTS)


#
# Rules for creating help files
#
# Note that the LOCALIZED_HELP_PLIST, LOCALIZED_HELP_FILES, and 
# LOCALIZED_SUBPROJECT_HELP_PLISTS variables are defined in terms of the 
# current target ($*) and therefore are only meaningful in the context of 
# the {Language}.create-help-file targets.
#
# The ifneq "$(HELP_FILES)" "$($(LANGUAGE)_HELP_FILES)" statement below may be
# a little unclear, so here's the explanation:
# It is an error to have non-localized help files.  This is because help files
# are always written in some human language, and therefore implicitly local.
# The ifneq enforces this.  There are three possible states of the variables:
# 1. Still using LANGUAGE, etc. instead of LANGUAGES, etc.
#    In this case, $(LANGUAGE)_HELP_FILES was assigned to $(HELP_FILES) earlier
#    in common.make, so the test is guaranteed to fail and the message is not reported
# 2. Using the new variables.
#    In this case, $(LANGUAGE)_HELP_FILES is _HELP_FILES, which is undefined.
#    HELP_FILES is also undefined, and so "" != "" fails and hence no message
# 3. Using the new variables, but have non-localized help files
#    $(LANGUAGE)_HELP_FILES is _HELP_FILES, which is undefined, but HELP_FILES
#    is defined.  Therefore the ifneq succeeds, the message is reported, and the
#    build stops.
#

HELP_FILE_NAME = Help.plist
ifeq "YES" "$(WRAPPED)"
LOCALIZED_HELP_PLIST = $(GLOBAL_RESOURCE_DIR)/$*.lproj/$(HELP_FILE_NAME)
else
LOCALIZED_HELP_PLIST = $(SFILE_DIR)/$*$(HELP_FILE_NAME)
endif
LOCALIZED_HELP_FILES = $(addprefix $*.lproj/, $($*_HELP_FILES))
LOCALIZED_SUBPROJECT_HELP_PLISTS = $(wildcard $(foreach SUBPROJ, $(SUBPROJECTS), $(SFILE_DIR)/$(SUBPROJ)/$*$(HELP_FILE_NAME)))
ALL_LOCALIZED_HELP_CONTENT = $(strip $(LOCALIZED_HELP_FILES) $(LOCALIZED_SUBPROJECT_HELP_PLISTS))

.SUFFIXES: .create-help-file

create-help-file: $(addsuffix .create-help-file, $(LANGUAGES))

$(addsuffix .create-help-file, $(LANGUAGES)):
ifneq "$(HELP_FILES)" "$($(LANGUAGE)_HELP_FILES)"
	$(SILENT) $(ECHO) common.make: localization error: Non-localized help files: $(HELP_FILES)
	$(SILENT) exit 1
endif
	$(SILENT) if [ ! -n "$(ALL_LOCALIZED_HELP_CONTENT)" ] || $(NEWER) -s -n $(LOCALIZED_HELP_PLIST) $(ALL_LOCALIZED_HELP_CONTENT); \
	then \
		$(ECHO) -n ; \
	else \
		$(ECHO) Processing $* help files... ; \
		cmd="$(MKDIRS) $(dir $(LOCALIZED_HELP_PLIST))" ; \
		$(ECHO) $$cmd ; $$cmd ; \
		cmd="$(COMPILEHELP) $(LOCALIZED_HELP_FILES) $(addprefix -m , $(LOCALIZED_SUBPROJECT_HELP_PLISTS)) -o $(LOCALIZED_HELP_PLIST)" ; \
		$(ECHO) $$cmd ; $$cmd ; \
	fi

# "Versioning Systems" adapt these makefiles to an SCM system in the following 2 ways:
#  1. Provide a rule for an object file that can be linked into binaries produced here
#      (e.g. $(VERS_OFILE) could then be included in OTHER_GENERATED_OFILES).
#  2. Provide a decimal number that can be used as the current project version to be
#      stored in a Mach dylib (i.e. $(CURRENT_PROJECT_VERSION)).

ifndef VERSIONING_SYSTEM_MAKEFILEDIR
VERSIONING_SYSTEM_MAKEFILEDIR = $(MAKEFILEPATH)/VersioningSystems
endif

ifndef LOCAL_VERSIONING_SYSTEM_MAKEFILEDIR
LOCAL_VERSIONING_SYSTEM_MAKEFILEDIR = $(NEXT_ROOT)$(LOCAL_DEVELOPER_DIR)/Makefiles/VersioningSystems
endif

-include $(VERSIONING_SYSTEM_MAKEFILEDIR)/$(VERSIONING_SYSTEM).make
-include $(LOCAL_VERSIONING_SYSTEM_MAKEFILEDIR)/$(VERSIONING_SYSTEM).make

# In case $(VERSIONING_SYSTEM).make didn't set CURRENT_PROJECT_VERSION...

ifeq "" "$(CURRENT_PROJECT_VERSION)"
CURRENT_PROJECT_VERSION = 1
endif
ifeq "" "$(COMPATIBILITY_PROJECT_VERSION)"
COMPATIBILITY_PROJECT_VERSION = 1
endif

# STRIP_ON_INSTALL must have a value
ifneq "$(STRIP_ON_INSTALL)" "NO"
STRIP_ON_INSTALL = YES
endif

# On Windows, we want to force the loading of framework DLL's.

ifeq "WINDOWS" "$(OS)"
ifneq "" "$(FRAMEWORKS)"

FORCELOAD_FILENAME = NSFrameworkForceLoad_$(NAME).m
FORCELOAD_FILEPATH = $(SFILE_DIR)/$(FORCELOAD_FILENAME)
OTHER_GENERATED_SRCFILES += $(FORCELOAD_FILENAME)
OTHER_GENERATED_OFILES += $(FORCELOAD_FILENAME:.m=.o)
BEFORE_BUILD_RECURSION += generate-forceload-file

STD_FRAMEWORK_SEARCHPATHS = -F$(NEXT_ROOT)$(SYSTEM_LIBRARY_DIR)/Frameworks -F$(NEXT_ROOT)$(SYSTEM_LIBRARY_DIR)/PrivateFrameworks
FRAMEWORK_ARGS = $(ALL_LDFLAGS) $(FRAMEWORKS) $(STD_FRAMEWORK_SEARCHPATHS)

generate-forceload-file: $(FORCELOAD_FILEPATH)
$(FORCELOAD_FILEPATH): Makefile
	$(SILENT) $(MKDIRS) $(SFILE_DIR)
	$(SILENT) $(RM) -f $(FORCELOAD_FILEPATH)
	$(GENFORCELOAD) $(FRAMEWORK_ARGS) > $(FORCELOAD_FILEPATH)

$(FORCELOAD_FILENAME:.m=.o): $(FORCELOAD_FILEPATH)
	$(CC) -c $(filter-out -W%, $(ALL_CFLAGS)) -o $(OFILE_DIR)/$(FORCELOAD_FILENAME:.m=.o) $<

endif
endif

# Back to Java...

ifeq "YES" "$(JAVA_USED)"

ifeq "$(patsubst $(PRODUCT)%,,$(JAVA_INSTALL_DIR))" "$(JAVA_INSTALL_DIR)"
JAVA_DSTROOT = $(DSTROOT)
endif
ifeq "$(patsubst $(PRODUCT)%,,$(JAVA_INSTALL_DIR_CLIENT))" "$(JAVA_INSTALL_DIR_CLIENT)"
JAVA_DSTROOT_CLIENT = $(DSTROOT)
endif

endif

ifneq "AGGREGATE" "$(PROJTYPE)"
# 04/14/1999 RDW -- Excluding depend.make for stage 1 builds on PDO
#                   (when we're bootstrapping everything).  Should
#                   be on for all other cases.  This was causing
#                   problems in stage 1 builds on HP-UX.
ifndef PDO_STAGE1BUILD
include $(MAKEFILEDIR)/depend.make
endif
include $(MAKEFILEDIR)/prebuild.make
include $(MAKEFILEDIR)/build.make
include $(MAKEFILEDIR)/installhdrs.make
include $(MAKEFILEDIR)/install.make
endif
include $(MAKEFILEDIR)/installsrc.make

always:


#
# architecture-specific .o files get combined into fat .o files
#

ifneq "$(LIPO)" ""

ARCH_SPECIFIC_OFILES = $(foreach OFILE, $(LOCAL_OFILES), $(foreach ARCH, $(ADJUSTED_TARGET_ARCHS), $(OFILE_DIR)/$(notdir $(basename $(OFILE))).$(ARCH).o))

$(OFILES): $(ARCH_SPECIFIC_OFILES)

endif

#
# Sequencing
#

ifneq "YES" "$(RECURSING)"
announce-prebuild announce-build announce-installhdrs announce-install: display-masthead
endif

ifneq "AGGREGATE" "$(PROJTYPE)"
ifneq "YES" "$(RECURSING)"
ifneq "YES" "$(SKIP_BUILD)"
announce-install: build
endif
ifneq "YES" "$(SKIP_PREBUILD)"
announce-build: prebuild
endif
endif
endif

#
# Conveniences
#

profile:
	$(SILENT) unset $(CUMULATIVE_VARIABLES) ||: ; \
            $(MAKE) PROFILE=YES \
		BUILD_TYPE=$@ \
		BUILD_TYPE_SUFFIX=$(PROFILE_SUFFIX) \
		APP_WRAPPER_EXTENSION=.profile

debug:
	$(SILENT) unset $(CUMULATIVE_VARIABLES) ||: ; \
            $(MAKE) DEBUG=YES PROFILE=NO OPTIMIZE=NO \
		BUILD_TYPE=$@ \
		BUILD_TYPE_SUFFIX=$(DEBUG_SUFFIX) \
		APP_WRAPPER_EXTENSION=.debug

BUILD_TYPE = normal

#
# Masthead
#

display-masthead:
ifeq "$(BUILD_TYPE)" "normal"
	$(SILENT) $(ECHO) == Making $(NAME) for $(ADJUSTED_TARGET_ARCHS) ==
else
	$(SILENT) $(ECHO) == Making $(BUILD_TYPE) on $(NAME) for $(ADJUSTED_TARGET_ARCHS) ==
endif

#
# Makefile-Debugging and Variable-Access Rules
#

.PHONY: show-variable sv show-expression se

VARIABLE=$(V)
show-variables sv:
	$(SILENT) $(foreach X, $(VARIABLE), $(ECHO) "$($(X))";)
EXPRESSION=$(E)
show-expression se:
	$(SILENT) $(foreach X, $(EXPRESSION), $(ECHO) "$(X)";)

echo_makefile_variable:
	$(SILENT) $(foreach X, $(VAR_NAME), $(ECHO) "$($(X))";)

echo_makefile_expression:
	$(SILENT) $(foreach X, $(EXPR_STRING), $(ECHO) "$(X)";)

#
# Cleaning Rules
#

.PHONY: clean mostlyclean announce-clean

clean: announce-clean
	$(RM) -rf $(GARBAGE) $(PRODUCTS)
ifneq "$(SYMROOT)" "$(BUILD_SYMROOT)"
	$(RM) -rf $(BUILD_SYMROOT)
endif

mostlyclean: announce-clean
	$(RM) -rf $(GARBAGE)
ifneq "$(SYMROOT)" "$(BUILD_SYMROOT)"
	$(RM) -rf $(BUILD_SYMROOT)
endif
	
announce-clean:
	$(SILENT) $(ECHO) == Cleaning $(NAME) ==


#
# Some install conveniences that apply to all projects
#
# The install_java_debug target creates the _g libraries that
# Sun's VM needs to run native method implementations under jdb
#

install_java_debug:
	$(MAKE) install DEBUG=YES PROFILE=NO OPTIMIZE=NO STRIP_ON_INSTALL=NO \
            BUILD_TYPE=debug BUILD_TYPE_SUFFIX=$(DEBUG_SUFFIX) \
            SRCROOT=$(SRCROOT) SYMROOT=$(SYMROOT) OBJROOT=$(OBJROOT)

install_debug:
	$(MAKE) install DEBUG=YES PROFILE=NO OPTIMIZE=NO STRIP_ON_INSTALL=NO

install_unstripped:
	$(MAKE) install DEBUG=NO PROFILE=NO OPTIMIZE=YES STRIP_ON_INSTALL=NO

install_profile:
	$(MAKE) install DEBUG=YES PROFILE=YES OPTIMIZE=YES STRIP_ON_INSTALL=NO

ifeq "YES" "$(JAVA_USED)"

COPY_JAVA_FS = copy-java-fs-wtar

# ZIP_JAVA_CLASSES is deprecated but it meant no archiving, so we'll
# honour it here if possible.

ifeq "NO" "$(ZIP_JAVA_CLASSES)"
ARCHIVE_JAVA_CLASSES = NO
endif

# Find the appropriate way to archive the Java classes.

ifneq "NO" "$(ARCHIVE_JAVA_CLASSES)"

ifndef JAVA_ARCHIVE_METHOD
JAVA_ARCHIVE_METHOD = ZIP
endif

ifeq "JAR" "$(JAVA_ARCHIVE_METHOD)"
ifndef JAVA_JAR_NAME
JAVA_JAR_NAME := $(shell $(SHELL) -c 'JJAR=`$(ECHO) $(NAME) | $(TR) "[A-Z]" "[a-z]"`; $(ECHO) $${JJAR:=$(NAME)}').jar
endif
JAVA_JAR_NAME_CLIENT = $(NAME).jar

COPY_JAVA_CLASSES = copy-java-jar
JAVA_PRODUCT = $(JAVA_JAR_NAME)
JAVA_PRODUCT_CLIENT = $(JAVA_JAR_NAME)
endif

ifeq "ZIP" "$(JAVA_ARCHIVE_METHOD)"
ifndef JAVA_ZIP_NAME
JAVA_ZIP_NAME := $(shell $(SHELL) -c 'JZIP=`$(ECHO) $(NAME) | $(TR) "[A-Z]" "[a-z]"`; $(ECHO) $${JZIP:=$(NAME)}').zip
endif

COPY_JAVA_CLASSES = copy-java-zip
JAVA_PRODUCT = $(JAVA_ZIP_NAME)
endif

endif

ifneq "" "$(JAVA_PRODUCT)"
JAVA_PRODUCTS_PATH = \"$(JAVA_PRODUCT)\"
endif

#ifndef JAVA_PRODUCT
#JAVA_PRODUCT = "`$(SHELL) -c '$(CD) $(JAVA_OBJ_DIR) && $(FIND) * -print'`"
#endif

#ifndef JAVA_PRODUCT_CLIENT
#JAVA_PRODUCT_CLIENT = "`$(SHELL) -c '$(CD) $(JAVA_OBJ_DIR_CLIENT) && $(FIND) * -print'`"
#endif

ifneq "" "$(JAVA_PRODUCT)"
GARBAGE += $(JAVA_RESOURCE_DIR)/$(JAVA_PRODUCT)
endif
ifneq "" "$(JAVA_PRODUCT_CLIENT)"
GARBAGE += $(JAVA_RESOURCE_DIR_CLIENT)/$(JAVA_PRODUCT_CLIENT)
endif

ifeq "" "$(COPY_JAVA_CLASSES)"
COPY_JAVA_CLASSES = $(COPY_JAVA_FS)
endif

ANNOUNCE_COPY_JAVA = announce-copy-java

copy-java-classes: $(ANNOUNCE_COPY_JAVA) relax-java-permissions $(COPY_JAVA_CLASSES)

ifneq "YES" "$(RECURSING)"
announce-copy-java: display-masthead
endif

announce-copy-java:
	$(SILENT) echo Copying Java classes...

relax-java-permissions: relax-java-perms relax-java-perms-client

relax-java-perms:
ifdef CHMOD
	-$(SILENT) if [ ! -z "$(JAVA_OBJ_DIR)" -a -d "$(JAVA_OBJ_DIR)" ]; then \
	    $(ECHO) $(CHMOD) -R a+rX $(JAVA_OBJ_DIR); \
	    $(CHMOD) -R a+rX $(JAVA_OBJ_DIR); \
        fi
endif

relax-java-perms-client:
ifdef CHMOD
	-$(SILENT) if [ ! -z "$(JAVA_OBJ_DIR_CLIENT)" -a -d "$(JAVA_OBJ_DIR_CLIENT)" ]; then \
	    $(ECHO) $(CHMOD) -R a+rX $(JAVA_OBJ_DIR_CLIENT); \
	    $(CHMOD) -R a+rX $(JAVA_OBJ_DIR_CLIENT); \
        fi
endif

copy-java-fs-wfastcp:
	$(SILENT) if [ -d "$(JAVA_OBJ_DIR)" ]; then \
	    $(CD) $(JAVA_OBJ_DIR); if [ "*" != "`$(ECHO) *`" ]; then \
	        $(MKDIRS) $(JAVA_RESOURCE_DIR); \
	        $(FASTCP) $(JAVA_OBJ_DIR)/* $(JAVA_RESOURCE_DIR); \
	    fi; \
	fi
	$(SILENT) if [ -d "$(JAVA_OBJ_DIR_CLIENT)" ]; then \
	    $(CD) $(JAVA_OBJ_DIR_CLIENT); if [ "*" != "`$(ECHO) *`" ]; then \
	    $(MKDIRS) $(JAVA_RESOURCE_DIR_CLIENT); \
	    $(FASTCP) $(JAVA_OBJ_DIR_CLIENT)/* $(JAVA_RESOURCE_DIR_CLIENT); \
	    fi; \
	fi

copy-java-fs-wtar:
	$(SILENT) if [ -d "$(JAVA_OBJ_DIR)" ]; then \
	    $(CD) $(JAVA_OBJ_DIR); if [ "*" != "`$(ECHO) *`" ]; then \
	        $(MKDIRS) $(JAVA_RESOURCE_DIR); \
	        ($(CD) $(JAVA_OBJ_DIR) && $(TAR) cf - *) | ($(CD) $(JAVA_RESOURCE_DIR) && $(TAR) xpf -) && echo Copied `($(CD) $(JAVA_OBJ_DIR) && ls -d *)`; \
	    fi; \
	fi
	$(SILENT) if [ -d "$(JAVA_OBJ_DIR_CLIENT)" ]; then \
	    $(CD) $(JAVA_OBJ_DIR_CLIENT); if [ "*" != "`$(ECHO) *`" ]; then \
	    $(MKDIRS) $(JAVA_RESOURCE_DIR_CLIENT); \
	    ($(CD) $(JAVA_OBJ_DIR_CLIENT) && $(TAR) cf - *) | ($(CD) $(JAVA_RESOURCE_DIR_CLIENT) && $(TAR) xpf -) && echo Copied `($(CD) $(JAVA_OBJ_DIR_CLIENT) && ls -d *)`; \
	    fi; \
	fi

# Note: Info-ZIP's zip recursively exploration of directories looks broken
# so we have to find the .class files ourselves in order to have a correct
# archive of classes.

# As a matter of fact, we do not want to zip the client-side Java: hey, how
# will it be served by an HTTP server, then? So we just use the fastcp copy.

copy-java-zip:
	$(SILENT) if [ -d "$(JAVA_OBJ_DIR)" -a "$(JAVA_CLASSES)" ]; then \
	    $(CD) $(JAVA_OBJ_DIR); if [ "*" != "`$(ECHO) *`" ]; then \
	        $(MKDIRS) $(JAVA_RESOURCE_DIR); \
		if [ "$(ARCHIVE_ALL_JAVA_CLASSES)" = "NO" ]; then \
		    $(CD) $(JAVA_OBJ_DIR) && $(MKZIP) $(JAVA_ZIP_FLAGS) $(JAVA_RESOURCE_DIR)/$(JAVA_ZIP_NAME) `$(ECHO) "$(JAVA_CLASSES)" | $(SED) -e "s/\.java/\.class/g"` && $(ECHO) Zipped `$(ECHO) "$(JAVA_CLASSES)" | $(SED) -e "s/\.java/\.class/g"`; \
		else \
		    $(CD) $(JAVA_OBJ_DIR) && $(MKZIP) $(JAVA_ZIP_FLAGS) $(JAVA_RESOURCE_DIR)/$(JAVA_ZIP_NAME) `$(FIND) . \( -type d -o -name \*.class \) -print | $(SED) 's,^\./,,'` && echo Zipped `ls -d *`; \
		fi; \
	    fi; \
	fi
	$(SILENT) if [ -d "$(JAVA_OBJ_DIR_CLIENT)" ]; then \
	    $(CD) $(JAVA_OBJ_DIR_CLIENT); if [ "*" != "`$(ECHO) *`" ]; then \
	    $(MKDIRS) $(JAVA_RESOURCE_DIR_CLIENT); \
	    $(FASTCP) $(JAVA_OBJ_DIR_CLIENT)/* $(JAVA_RESOURCE_DIR_CLIENT); \
	    fi; \
	fi

# If we have jar, we certainly will use this rule rather than the zip one.

copy-java-jar-resources: $(JAVA_OBJ_DIR)
ifneq "$(OTHER_JAR_RESOURCES)" ""
	$(SILENT) $(FASTCP) $(OTHER_JAR_RESOURCES) $(JAVA_OBJ_DIR)
endif
ifneq "$(OTHER_JAR_RESOURCES_CLIENT)" ""
	$(SILENT) $(FASTCP) $(OTHER_JAR_RESOURCES_CLIENT) $(JAVA_OBJ_DIR_CLIENT)
endif

copy-java-jar: copy-java-jar-resources
	$(SILENT) if [ -d "$(JAVA_OBJ_DIR)" -a "$(JAVA_CLASSES)" ]; then \
	    $(CD) $(JAVA_OBJ_DIR); if [ "*" != "`$(ECHO) *`" ]; then \
	        $(MKDIRS) $(JAVA_RESOURCE_DIR); \
		if [ "$(ARCHIVE_ALL_JAVA_CLASSES)" = "NO" ]; then \
	            $(CD) $(JAVA_OBJ_DIR) && $(JAR) $(JAVA_JAR_FLAGS) $(JAVA_JAR_PARTIAL_MANIFEST) $(JAVA_RESOURCE_DIR)/$(JAVA_JAR_NAME) `$(ECHO) "$(JAVA_CLASSES)" | $(SED) -e "s/\.java/\.class/g"` && $(ECHO) Jarred `$(ECHO) "$(JAVA_CLASSES)" | $(SED) -e "s/\.java/\.class/g"`; \
		else \
		    $(CD) $(JAVA_OBJ_DIR) && $(JAR) $(JAVA_JAR_FLAGS) $(JAVA_JAR_PARTIAL_MANIFEST) $(JAVA_RESOURCE_DIR)/$(JAVA_JAR_NAME) *  && echo Jarred `ls -d *`; \
		fi; \
	    fi; \
	fi
	$(SILENT) if [ -d "$(JAVA_OBJ_DIR_CLIENT)" ]; then \
	    $(CD) $(JAVA_OBJ_DIR_CLIENT); if [ "*" != "`$(ECHO) *`" ]; then \
	    $(MKDIRS) $(JAVA_RESOURCE_DIR_CLIENT); \
	    $(CD) $(JAVA_OBJ_DIR_CLIENT) && $(JAR) $(JAVA_JAR_FLAGS_CLIENT) $(JAVA_JAR_PARTIAL_MANIFEST_CLIENT) $(JAVA_RESOURCE_DIR_CLIENT)/$(JAVA_JAR_NAME_CLIENT) * && $(ECHO) Jarred `ls -d *`; \
	    $(FASTCP) $(JAVA_OBJ_DIR_CLIENT)/* $(JAVA_RESOURCE_DIR_CLIENT); \
	    fi; \
	fi

endif

-include $(LOCAL_MAKEFILEDIR)/common.make.postamble

