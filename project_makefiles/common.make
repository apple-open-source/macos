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
###############################################################################
#  NeXT common.make
#  Copyright 1992, NeXT Computer, Inc.
#
#  This makefile is common to all project-types (apps, subprojects,
#  bundles, and palettes).  It can also prove useful to custom Makefiles
#  needing generic project-building functionality, but users should be aware
#  that interfaces supported at this level are private to the makefiles
#  and may change from release to release.
#  
###############################################################################

# Turn on the use of the NeXT/Apple make hacks to support the pb_makefiles.
USE_APPLE_PB_SUPPORT = all
export USE_APPLE_PB_SUPPORT

ifndef LOCAL_MAKEFILEDIR
	LOCAL_MAKEFILEDIR = $(NEXT_ROOT)$(LOCAL_DEVELOPER_DIR)/Makefiles/project
endif

# extremely large projects may have to override this name to something shorter
STRIPNAME = STRIPPED

-include $(LOCAL_MAKEFILEDIR)/common.make.preamble

# Non-optional source files for the project.
SRCFILES = PB.project $(CLASSES) $(MFILES) $(CFILES) \
	$(CCFILES) $(CAPCFILES) $(CAPMFILES) $(CXXFILES) $(CPPFILES) \
	$(HFILES) $(PSWFILES) $(PSWMFILES) $(DBMODELAFILES) \
	$(GLOBAL_RESOURCES) $(LOCAL_RESOURCES) $(HELP_FILES) \
	$(OTHERSRCS) $(OTHERLINKED) $(OTHER_SOURCEFILES)
# Optional source files for the project.
SUPPORTFILES = Makefile $(ICONHEADER) $(APPICON) $(DOCICONS) 

# What OS are we running on?
-include $(MAKEFILEDIR)/platform.make

include $(MAKEFILEDIR)/$(PLATFORM_OS)-specific.make

# Use the OS-specific versions of some variables, if they exist.

OS_SPECIFIC_INSTALLDIR = $($(OS_PREFIX)INSTALLDIR)
ifneq ("$(OS_SPECIFIC_INSTALLDIR)", "")
INSTALLDIR := $(OS_SPECIFIC_INSTALLDIR)
endif

OS_SPECIFIC_BUILDDIR = $($(OS_PREFIX)BUILD_OUTPUT_DIR)
ifneq ("$(OS_SPECIFIC_BUILDDIR)", "")
BUILD_OUTPUT_DIR := $(OS_SPECIFIC_BUILDDIR)
endif

OS_SPECIFIC_PB_CFLAGS = $($(OS_PREFIX)PB_CFLAGS)
ifneq ("$(OS_SPECIFIC_PB_CFLAGS)", "")
PB_CFLAGS := $(OS_SPECIFIC_PB_CFLAGS)
endif

OS_SPECIFIC_PB_LDFLAGS = $($(OS_PREFIX)PB_LDFLAGS)
ifneq ("$(OS_SPECIFIC_PB_LDFLAGS)", "")
PB_LDFLAGS := $(OS_SPECIFIC_PB_LDFLAGS)
endif

OS_SPECIFIC_PUBLIC_HEADER_DIR = $($(OS_PREFIX)PUBLIC_HEADERS_DIR)
ifneq ("$(OS_SPECIFIC_PUBLIC_HEADER_DIR)", "")
PUBLIC_HEADER_DIR := $(OS_SPECIFIC_PUBLIC_HEADER_DIR)
endif

ifndef BUILD_OUTPUT_DIR
 	BUILD_OUTPUT_DIR = .
endif
ifndef SYMROOT
	SYMROOT = $(BUILD_OUTPUT_DIR)
endif
ifndef OBJROOT
	OBJROOT = $(BUILD_OUTPUT_DIR)
endif

### Compute all the possible derived files and directories for them:

SRCROOT = `pwd`
DERIVED_SRC_DIR_NAME = derived_src

# Directory for .o files (can be thrown away) 
OFILE_DIR = $(OBJROOT)/obj
# Directory for all other derived files (contains symbol info. for debugging)
SYM_DIR = $(SYMROOT)/$(DERIVED_SRC_DIR_NAME)
# Directory for all public headers of the entire project tree
LOCAL_HEADER_DIR = $(SYMROOT)/Headers/$(NAME)

# For compatibility:
DERIVED_DIR = $(OFILE_DIR)
DERIVED_SRC_DIR = $(SYM_DIR)

# Auto-created directories
$(SYMROOT) $(OFILE_DIR) $(SYM_DIR) $(PUBLIC_HEADER_DIR) $(DSTROOT)$(INSTALLDIR) $(PRODUCT_ROOT) $(RESOURCES_ROOT):
	@$(MKDIRS) $@ $(BURY_STDERR) || ($(ECHO) \'$(MKDIRS) $@\' failed in `pwd` ; exit 1)

# Versioning of wrapper directories
CURRENTLY_ACTIVE_VERSION = YES
DEPLOY_WITH_VERSION_NAME = A
COMPATIBILITY_PROJECT_VERSION = 1

MSGOFILES = $(MSGFILES:.msg=Speaker.o) $(MSGFILES:.msg=Listener.o)
MSGDERIVEDMFILES = $(MSGFILES:.msg=Speaker.m) $(MSGFILES:.msg=Listener.m)

ALLMIGFILES = $(MIGFILES) $(DEFSFILES)

MIGOFILES = $(MIGFILES:.mig=User.o) $(MIGFILES:.mig=Server.o)
DEFSOFILES = $(DEFSFILES:.defs=User.o) $(DEFSFILES:.defs=Server.o)
ALLMIGOFILES = $(MIGOFILES) $(DEFSOFILES)

MIGDERIVEDCFILES = $(MIGFILES:.mig=User.c) $(MIGFILES:.mig=Server.c)
MIGDERIVEDHFILES = $(MIGFILES:.mig=.h)
DEFSDERIVEDCFILES = $(DEFSFILES:.defs=User.c) $(DEFSFILES:.defs=Server.c)
DEFSDERIVEDHFILES = $(DEFSFILES:.defs=.h)
ALLMIGDERIVEDCFILES = $(MIGDERIVEDCFILES) $(DEFSDERIVEDCFILES)
ALLMIGDERIVEDSRCFILES = $(ALLMIGDERIVEDCFILES) $(MIGDERIVEDHFILES) $(DEFSDERIVEDHFILES)

RPCOFILES = $(RPCDERIVEDCFILES:.c=.o)
RPCDERIVEDCFILES = $(RPCFILES:.x=_clnt.c) \
                   $(RPCFILES:.x=_svc.c) \
                   $(RPCFILES:.x=_xdr.c)
ALLRPCDERIVEDSRCFILES = $(RPCFILES:.x=.h) $(RPCDERIVEDCFILES)

EARLY_HFILES = $(PSWFILES:.psw=.h) $(PSWMFILES:.pswm=.h) $(RPCFILES:.x=.h)
EARLY_OFILES = $(PSWFILES:.psw=.o) $(PSWMFILES:.pswm=.o)

ALL_PRECOMPS = $(PRECOMPILED_HEADERS:.h=.p) $(PRECOMPS)

INITIAL_FILES = $(INITIAL_INITIAL_TARGETS) $(PROJECTTYPE_SPECIFIC_INITIAL_TARGETS) $(EARLY_HFILES) $(MSGOFILES) $(ALLMIGOFILES) export_headers refresh_precomps $(OTHER_INITIAL_TARGETS)

# Creating initial files and directories is tricky in the parallel build case

.PHONY : all initial_targets initial_dirs

initial_targets: $(INITIAL_FILES)
INITIAL_DIRS = $(OFILE_DIR)/.initial_dirs 
INITIAL_DIRECTORIES = $(OFILE_DIR) $(SYM_DIR) 
$(INITIAL_FILES): $(INITIAL_DIRS)

$(INITIAL_DIRS):
	@$(MKDIRS) $(INITIAL_DIRECTORIES)
	@$(TOUCH) $(INITIAL_DIRS)

SUBPROJ_OFILES = $(SUBPROJECTS:.subproj=_subproj.o)
SUBPROJ_OFILELISTS = $(SUBPROJECTS:.subproj=_subproj.ofileList)
NON_SUBPROJ_OFILES = $(CLASSES:.m=.o) $(MFILES:.m=.o) $(CFILES:.c=.o) \
	$(CCFILES:.cc=.o) $(CAPCFILES:.C=.o) $(CAPMFILES:.M=.o) \
	$(CXXFILES:.cxx=.o) $(CPPFILES:.cpp=.o) $(EARLY_OFILES) \
	$(OTHERLINKEDOFILES) 
OFILES = $(NON_SUBPROJ_OFILES) $(SUBPROJ_OFILES)
 
#    Note: It would be nice to put $(OTHERRELOCATABLES) in this list someday
#          when PB provides full paths for the contents of this variable.

# Derived resources:
DBMODELS = $(DBMODELAFILES:.dbmodela=.dbmodel)

PROJECT_INFO_FILE = Info-$(PLATFORM_OS).plist
USER_INFO_FILE = CustomInfo.plist
# Note: OTHER_INFO_FILES can be used for platform-specific Info.plist hackery

HELP_OUTPUT_FILE = Help.plist
HELP_OUTPUT_FILE_DIR = $(RESOURCES_ROOT)

### Set defaults for many values used throughout the Makefiles

PB_MAKEFILES = Makefile
PROJECT_HEADERS_DIR_NAME = ProjectHeaders

PRODUCT_DEPENDS = $(OFILES) $(OTHER_OFILES) $(DBMODELS) \
	$(ICONHEADER) $(APPICON) $(DOCICONS) $(PB_MAKEFILES) \
	$(OTHER_GENERATED_OFILES) $(OTHER_PRODUCT_DEPENDS)

GARBAGE = $(PROJECT_TYPE_SPECIFIC_GARBAGE) \
	$(OBJROOT)/*_obj $(OBJROOT)/obj $(OFILE_DIR) \
	$(SYMROOT)/$(DERIVED_SRC_DIR_NAME) $(SYMROOT)/$(PROJECT_HEADERS_DIR_NAME) \
	$(SYMROOT)/sym *~ $(LANGUAGE).lproj/*~ $(VERS_FILE) \
	Makefile.dependencies $(SYMROOT)/$(CHANGES_FILE_BASE)* gmon.out \
	$(ALL_PRECOMPS) $(OTHER_INITIAL_TARGETS) $(OTHER_GARBAGE)

# Default name for file to use as "reference time of last build"
CHANGES_FILE_BASE = .lastBuildTime

# Compiler flags that may be overridden in Makefile.postamble
OPTIMIZATION_CFLAG = -O
DEBUG_SYMBOLS_CFLAG = -g
WARNING_CFLAGS = -Wmost
DEBUG_BUILD_CFLAGS = -DDEBUG
PROFILE_BUILD_CFLAGS = -pg -g -DPROFILE
POSIX_BUILD_CFLAGS = -D_POSIX_LIB
SHLIB_BUILD_CFLAGS =  -I$(LOCAL_DEVELOPER_DIR)/Headers/libsys -i$(LOCAL_DEVELOPER_DIR)/Headers/libsys/shlib.h -DSHLIB
KERNEL_BUILD_CFLAGS =  -DKERNEL

# Default compiler options
ALL_FRAMEWORK_CFLAGS = $(FRAMEWORK_PATHS) $(PROPOGATED_FRAMEWORK_CFLAGS)
PROJECT_SPECIFIC_CFLAGS = $(CFLAGS) $(OTHER_CFLAGS) $(HEADER_PATHS) $(PB_CFLAGS) 
COMMON_CFLAGS = $(DEBUG_SYMBOLS_CFLAG) $(WARNING_CFLAGS) $(PIPE_CFLAG)    
all_target_CFLAGS = $(COMMON_CFLAGS) $(OPTIMIZATION_CFLAG)
debug_target_CFLAGS = $(COMMON_CFLAGS) $(DEBUG_BUILD_CFLAGS)
profile_target_CFLAGS = $(COMMON_CFLAGS) $(PROFILE_BUILD_CFLAGS) $(OPTIMIZATION_CFLAG) 
app_target_CFLAGS = $(all_target_CFLAGS)
library_target_CFLAGS = $(all_target_CFLAGS)
framework_target_CFLAGS = $(all_target_CFLAGS)
bundle_target_CFLAGS = $(all_target_CFLAGS)
palette_target_CFLAGS = $(all_target_CFLAGS)
posix_target_CFLAGS = $(COMMON_CFLAGS) $(POSIX_BUILD_CFLAGS) $(OPTIMIZATION_CFLAG) 
shlib_target_CFLAGS = $(COMMON_CFLAGS) $(SHLIB_BUILD_CFLAGS) $(OPTIMIZATION_CFLAG) 
kernel_target_CFLAGS = $(COMMON_CFLAGS) $(KERNEL_BUILD_CFLAGS) $(OPTIMIZATION_CFLAG) 
OBJCFLAG = -ObjC

# ...and the actual flags used in compilation (see basicrules.make)
ALL_CFLAGS = $(BUILD_TYPE_CFLAGS) $(ALL_FRAMEWORK_CFLAGS) $(PROPOGATED_CFLAGS) $(PROJECT_SPECIFIC_CFLAGS) -I$(DEV_PROJECT_HEADER_DIR) -I$(SYM_DIR) $(RC_CFLAGS) $(EXTRA_CFLAGS)
ALL_PRECOMP_CFLAGS = $(BUILD_TYPE_CFLAGS) $(ALL_FRAMEWORK_CFLAGS) $(PROPOGATED_CFLAGS) $(PROJECT_SPECIFIC_CFLAGS) -I$(DEV_PROJECT_HEADER_DIR) -I$(SYM_DIR) $(RC_CFLAGS) $(ALL_ARCH_FLAGS) $(EXTRA_CFLAGS)

# Link editor options:
all_target_LDFLAGS = $(SECTORDER_FLAGS)
debug_target_LDFLAGS =
profile_target_LDFLAGS = -pg
app_target_LDFLAGS = $(all_target_LDFLAGS)
library_target_LDFLAGS = $(all_target_LDFLAGS)
framework_target_LDFLAGS = $(all_target_LDFLAGS)
bundle_target_LDFLAGS = $(all_target_LDFLAGS)
ALL_LDFLAGS = $(LIBRARY_PATHS) $(BUILD_TYPE_LDFLAGS) $(PB_LDFLAGS) $(LDFLAGS) $(OTHER_LDFLAGS) $(EXTRA_LDFLAGS)

# Yacc options
YFLAGS = -d


PUSHD = pushed_dir=`pwd` ; cd 
POPD = cd $$pushed_dir

DEFAULT_BUNDLE_EXTENSION = bundle

# Set VPATH via a variable so clients of common.make can reuse it when overriding VPATH
NORMAL_VPATH = $(OFILE_DIR):$(SYM_DIR):$(LANGUAGE).lproj:$(PRODUCT_ROOT):$(PRODUCT_ROOT)/$(LANGUAGE).lproj
VPATH = $(VPATH_PREAMBLE)$(NORMAL_VPATH)$(VPATH_POSTAMBLE)


# Generation of a version string if project lives in correct directory name
# To activate this feature, put your source code in a directory named 
# $(NAME).%d[.%d][.%d] and set OTHER_GENERATED_OFILES = $(VERS_OFILE).
VERS_FILE = $(SYM_DIR)/$(NAME)_vers.c
VERS_OFILE = $(OFILE_DIR)/$(NAME)_vers.o

$(VERS_OFILE): $(VERS_FILE)

$(VERS_FILE): 
	@($(RM) -f $(VERS_FILE) ; \
	cname=`echo $(NAME) | sed 's/[^0-9A-Za-z]/_/g'`; \
        $(VERS_STRING) -c $(NAME) \
                | sed s/SGS_VERS/$${cname}_VERS_STRING/ \
                | sed s/VERS_NUM/$${cname}_VERS_NUM/ > $@)

### Use a set of basic suffix-style rules:

include $(MAKEFILEDIR)/basicrules.make

### Some utility definitions used throughout the PB Makefiles

process_target_archs = \
	if [ -n "$(RC_ARCHS)" ] ; then \
		archs="$(RC_ARCHS)" ; \
	else \
	    if [ -n "$(TARGET_ARCHS)" ] ; then \
		archs="$(TARGET_ARCHS)" ; \
	    else \
		archs=`$(ARCH_CMD)` ; \
	    fi ; \
	fi ; \
	if [ -z "$$archs" ] ; then \
		archs=`$(ARCH_CMD)` ; \
	fi ; \
	archless_rcflags=`$(DEARCHIFY) $(RC_CFLAGS)` ; \
	arch_flags=`$(ARCHIFY) $$archs` 

set_build_output_dirs = true


# Various little pieces of shared logic...

set_bundle_ext = \
	if [ -z "$(BUNDLE_EXTENSION)" ] ; then \
	   bundle_ext=$(DEFAULT_BUNDLE_EXTENSION) ; \
	else \
	   bundle_ext=$(BUNDLE_EXTENSION) ; \
	fi 

DYNAMIC_CFLAGS = -fno-common

set_dynamic_flags = \
	if [ "$@" = "shlib" ] ; then \
           dynamic_cflags="$(STATIC_CODE_GEN_CFLAG)" ; \
	else \
           if [ "$(CODE_GEN_STYLE)" = "DYNAMIC" ] ; then \
                buildtype="dynamic" ; \
                dynamic_cflags="$(DYNAMIC_CODE_GEN_CFLAG) $(DYNAMIC_CFLAGS)" ; \
		  library_ext="$(DYNALIB_EXT)" ; \
           else \
                buildtype="static" ; \
                dynamic_cflags="$(STATIC_CODE_GEN_CFLAG)" ; \
		  library_ext="$(STATICLIB_EXT)" ; \
           fi ; \
           if [ "$(LIBRARY_STYLE)" = "STATIC" ] ; then \
		  library_ext="$(STATICLIB_EXT)" ; \
           fi ; \
        fi ; \
        if [ "$@" = "shlib" -o "$@" = "posix" -o "$@" = "kernel" ] ; then \
            buildtype="$@" ; \
        fi

# Use of $(AWK) is temporary workaround until vers_string gets fixed.

CURRENT_PROJECT_VERSION = `$(VERS_STRING) -n $(BURY_STDERR) | $(AWK) -F. '$$3 ~ /^[0-9][0-9]*$$/ && $$2 ~ /^[0-9][0-9]*$$/ && $$1 ~ /^[0-9][0-9]*$$/ {print $$1"."$$2"."$$3; next} $$2 ~ /^[0-9][0-9]*$$/ && $$1 ~ /^[0-9][0-9]*$$/ {print $$1"."$$2; next} $$1 ~ /^[0-9][0-9]*$$/ {print $$1}'`


set_dynamic_link_flags = \
        if [ "$(CODE_GEN_STYLE)" = "DYNAMIC" ]; then \
	    if [ "$(LIBRARY_STYLE)" != "STATIC" ] ; then \
                dynamic_libtool_flags="$(DYNAMIC_LIBTOOL_FLAGS)" ; \
		if [ -n "$(COMPATIBILITY_PROJECT_VERSION)" \
		    -a -n "$(CURRENT_PROJECT_VERSION)" ] ; then \
		   dynamic_libtool_flags="$$dynamic_libtool_flags -compatibility_version $(COMPATIBILITY_PROJECT_VERSION) -current_version $(CURRENT_PROJECT_VERSION)" ; \
		fi ; \
	     else \
                dynamic_libtool_flags="$(STATIC_LIBTOOL_FLAGS)" ; \
	     fi ; \
             dynamic_ldflags="$(DYNAMIC_LDFLAGS)" ; \
             dynamic_bundle_flags="-bundle $(DYNAMIC_BUNDLE_UNDEFINED_FLAGS)" ; \
        else \
             dynamic_libtool_flags="$(STATIC_LIBTOOL_FLAGS)" ; \
             dynamic_bundle_flags="$(STATIC_CODE_GEN_CFLAG) -nostdlib -r" ; \
        fi

DYNAMIC_BUILD_TYPE_SUFFIX = ""
DEBUG_BUILD_TYPE_SUFFIX = ""
PROFILE_BUILD_TYPE_SUFFIX = "_profile"

set_objdir = \
   case "$@" in                                               		\
      debug | common_debug_install)                                     \
       	objdir="debug_obj" ;                                            \
	build_type_suffix=$(DEBUG_BUILD_TYPE_SUFFIX) ;;			\
      profile | common_profile_install)					\
	objdir="profile_obj" ;						\
	build_type_suffix=$(PROFILE_BUILD_TYPE_SUFFIX) ;;	    	\
      *)								\
        objdir="obj" ;							\
        build_type_suffix=$(DYNAMIC_BUILD_TYPE_SUFFIX) ;;         	\
   esac

set_should_build = \
	should_build=yes; \
	for excluded_platform in $(EXCLUDED_PLATFORMS) none ; do \
	    if [ "$(PLATFORM_OS)" = "$$excluded_platform" ] ; then \
	        should_build=no; \
	    fi ; \
	done ; \
	if [ -n "$(INCLUDED_PLATFORMS)" ] ; then \
	   should_build=no; \
	   for included_platform in $(INCLUDED_PLATFORMS) none ; do \
	       if [ "$(PLATFORM_OS)" = "$$included_platform" ] ; then \
	          should_build=yes; \
	       fi ; \
	   done ; \
	fi ; \
	if [ -z "$$arch" ] ; then \
	   arch="$(TARGET_ARCH)" ; \
	fi ; \
	if [ "$$should_build" = "yes" -a -n "$$arch" ] ; then \
	   for excluded_arch in $(EXCLUDED_ARCHS) none ; do \
	      if [ "$$arch" = "$$excluded_arch" ] ; then \
	         should_build=no; \
	      fi ; \
	   done ; \
	   if [ -n "$(INCLUDED_ARCHS)" ] ; then \
	      should_build=no; \
	      for included_arch in $(INCLUDED_ARCHS) none ; do \
	         if [ "$$arch" = "$$included_arch" ] ; then \
	            should_build=yes; \
	         fi ; \
	      done ; \
	   fi ; \
	fi


### Define all the targets necessary at every level of the project-hierarchy:

# The following rules and rule fragments do the recursion into "sub" projects
# of this project and does a 'make project' for each one in its
# respective directories.  This insures that we do not rely on the directory
# timestamp or "hack" file to know whether or not something has changed.  

CHANGES_COMMAND = $(CHANGES) $(TOP_PRODUCT_ROOT)/$(CHANGES_FILE_BASE).$(TARGET_ARCH) $(BUILD_TARGET)

use_default_directory_args = \
	top_prod_root=`$(ECHO) $(TOP_PRODUCT_ROOT) | $(DOTDOTIFY_PATH)` ; \
	prod_root=`$(ECHO) $(PRODUCT_ROOT) | $(DOTDOTIFY_PATH)` ; \
	ofile_dir=`$(ECHO) $(OFILE_DIR) | $(DOTDOTIFY_PATH)` ; \
	sym_dir=`$(ECHO) $(SYM_DIR) | $(DOTDOTIFY_PATH)` ; \
	header_base=`$(ECHO) $(DEV_HEADER_DIR_BASE) | $(DOTDOTIFY_PATH)` ; \
	project_header_base=`$(ECHO) $(DEV_PROJECT_HEADER_DIR_BASE) | $(DOTDOTIFY_PATH)` ; \
        propogated_cflags=`$(ECHO) $(PROPOGATED_CFLAGS) | $(DOTDOTIFY_IPATHS)`

use_absolute_directory_args = \
      if [ -d "$(PRODUCT_ROOT)" ] ; then \
	   $(PUSHD) $(PRODUCT_ROOT) ; abs_prod_root=`pwd` ; $(POPD) ; \
           prod_root=$$abs_prod_root ; \
      fi ; \
      if [ -d "$(TOP_PRODUCT_ROOT)" ] ; then \
          $(PUSHD) $(TOP_PRODUCT_ROOT) ; abs_top_prod_root=`pwd` ; $(POPD) ; \
          top_prod_root=$$abs_top_prod_root ; \
      fi ; \
      if [ -d "$(OFILE_DIR)" ] ; then \
          $(PUSHD) $(OFILE_DIR) ; abs_ofile_dir=`pwd` ; $(POPD) ; \
          ofile_dir=$$abs_ofile_dir ; \
      fi ; \
      if [ -d "$(SYM_DIR)" ] ; then \
          $(PUSHD) $(SYM_DIR) ; abs_sym_dir=`pwd` ; $(POPD) ; \
          sym_dir=$$abs_sym_dir ; \
      fi ; \
      if [ -d "$(DEV_HEADER_DIR_BASE)" ] ; then \
          $(PUSHD) $(DEV_HEADER_DIR_BASE) ; abs_header_base=`pwd` ; $(POPD) ; \
          header_base=$$abs_header_base ; \
      fi ; \
      if [ -d "$(DEV_PROJECT_HEADER_DIR_BASE)" ] ; then \
          $(PUSHD) $(DEV_PROJECT_HEADER_DIR_BASE) ; abs_project_header_base=`pwd` ; $(POPD) ; \
          project_header_base=$$abs_project_header_base ; \
      fi ; \
      propogated_cflags="$(PROPOGATED_CFLAGS)"


ALL_SUBPROJECTS = $(BUILD_TOOLS) $(SUBPROJECTS) $(BUNDLES) $(FRAMEWORK_SUBPROJECTS) $(LIBRARIES) $(TOOLS) $(LEGACIES) $(AGGREGATES) $(PALETTES)
IS_TOPLEVEL = 

# may not be right if tool is renamed without renaming directory name
TOOL_NAMES = $(TOOLS=.tproj=)

.PHONY : all_subprojects

all_subprojects: $(TOP_PRODUCT_ROOT)
	@(if [ -z "$(ONLY_SUBPROJECT)" ] ; then \
	    if [ -n "$(BUILD_ALL_SUBPROJECTS)" ] ; then \
		subdirectories="$(ALL_SUBPROJECTS)" ; \
	    else \
	        subdirectories=`$(CHANGES_COMMAND) $(ALL_SUBPROJECTS)` ; \
	    fi ; \
	else \
	    subdirectories="$(ONLY_SUBPROJECT)" ; \
	fi ; \
	target=project ; \
	beginning_msg="Making" ; ending_msg="Finished making" ; \
	$(recurse_on_subdirectories))
	@(if [ -n "$(RESOURCES_ROOT)" ] ; then \
           actual_prod_root=$(RESOURCES_ROOT) ; \
        else \
           actual_prod_root=$(PRODUCT_ROOT) ; \
        fi ; \
	$(check_tools))
	@(if [ "$(IS_TOPLEVEL)" = "YES" -a -z "$(ONLY_SUBPROJECT)" ] ; then \
	   $(RM) -f $(TOP_PRODUCT_ROOT)/$(CHANGES_FILE_BASE).$(TARGET_ARCH) ; \
	   $(ECHO) $(BUILD_TARGET) > $(TOP_PRODUCT_ROOT)/$(CHANGES_FILE_BASE).$(TARGET_ARCH) ; \
	fi)

$(SUBPROJ_OFILES):
	@(if [ "$(BE_PARANOID)" = "YES" -a ! -r "$(OFILE_DIR)/$@" ] ; then \
	   $(ECHO) $(OFILE_DIR)/$@ does not exist....rebuilding. ; \
	   subdirectories="`$(BASENAME) $@ | $(SED) s/_subproj.o/.subproj/`"; \
	   target=project; \
	   beginning_msg="Making" ; ending_msg="Finished making" ; \
  	   $(recurse_on_subdirectories) ; \
	fi)

TOOL_NAMES = $(TOOLS:.tproj=)
check_tools = \
      if [ "$(BE_PARANOID)" = "YES" ] ; then \
	for tool in $(TOOL_NAMES) none ; do \
	   if [ $$tool = "none" ] ; then break; fi ; \
	   tool_file="$$actual_prod_root/$${tool}$(BUILD_TYPE_SUFFIX).$(TARGET_ARCH)$(EXECUTABLE_EXT)" ; \
	   if [ ! -s $$tool_file ] ; then \
	      $(ECHO) $$tool_file does not exist...; \
	      subdirectories=$$tool.tproj; \
	      target=project; \
	      beginning_msg="Making" ; ending_msg="Finished making" ; \
  	      $(recurse_on_subdirectories) ; \
	   fi ; \
	done ; \
      fi

recurse_on_subdirectories = \
      	$(use_default_directory_args) ; \
	for sub in $$subdirectories none ; do \
	   if [ "$$sub" = "none" ] ; then break; fi ; \
	   if [ -h "$$sub" ] ; then \
		$(use_absolute_directory_args) ; \
	   fi ; \
           $(PUSHD) $$sub; \
	   if [ -n "$$beginning_msg" ] ; then \
		$(ECHO) $$beginning_msg $$sub; \
	   fi ; \
	   $(MAKE) $$target $(exported_vars) $(stop_if_error_in_sub) ; \
	   $(POPD) ; \
	   if [ -n "$$ending_msg" ] ; then \
		$(ECHO) $$ending_msg $$sub ; \
	   fi ; \
	done


exported_vars = \
	"PRODUCT_ROOT = $$prod_root" \
	"TOP_PRODUCT_ROOT = $$top_prod_root" \
	"OFILE_DIR = $$ofile_dir/$$sub" \
	"PRODUCT_PREFIX = $$ofile_dir/$$sub" \
	"BUNDLE_DIR = $$prod_root/$$sub" \
	"REL_BUNDLE_DIR = $$sub" \
	"SYM_DIR = $$sym_dir/$$sub" \
	"TARGET_ARCH = $(TARGET_ARCH)" \
	"BUILD_TARGET = $(BUILD_TARGET)" \
	"MAKEFILEDIR = $(MAKEFILEDIR)" \
	"DEST = $(DEST)/$$sub" \
	"SRCROOT = $(SRCROOT)" \
	"OBJROOT = $(OBJROOT)" \
	"SYMROOT = $(SYMROOT)" \
	"DSTROOT = $(DSTROOT)" \
	"IS_TOPLEVEL = NO" \
	"PROPOGATED_CFLAGS = $(PROJECT_SPECIFIC_CFLAGS) $$propogated_cflags -I$$sym_dir" \
	"PROPOGATED_FRAMEWORK_CFLAGS = $(ALL_FRAMEWORK_CFLAGS)" \
	"DEV_PROJECT_HEADER_DIR_BASE = $$project_header_base" \
	"PROJECT_HEADERS_DIR_NAME = $(PROJECT_HEADERS_DIR_NAME)" \
	"MULTIPLE_ARCHS = $(MULTIPLE_ARCHS)" \
	"SINGLE_ARCH = $(SINGLE_ARCH)" \
	"DEPENDENCIES =" \
	"ONLY_SUBPROJECT =" \
	"RC_ARCHS = $(RC_ARCHS)" \
	"RC_CFLAGS = $(RC_CFLAGS)" \
	"ALL_ARCH_FLAGS = $(ALL_ARCH_FLAGS)" \
	$(projectType_specific_exported_vars)


stop_if_error_in_sub = \
        || ($(ECHO) ---- Stopping build due to failure in $$sub ; exit 1)

stop_if_error_in_arch = \
        || ($(ECHO) ---- Stopping build due to failure in build of architecture $$arch ; exit 1) 

stop_if_error_in_name = \
        || ($(ECHO) ---- Stopping build due to failure in $(NAME) ; exit 1) 

continuep = \
        || ($(ECHO) $(MFLAGS) | $(SEARCH) "k") 

# Finalizing build (e.g. lipo/ln the arch-specific binaries into place)

.PHONY : configure_for_target_archs

configure_for_target_archs_exported_vars = \
	$(exported_vars) \
	$(extra_configure_for_target_archs_exported_vars)

configure_for_target_archs::
	@($(set_should_build) ; \
	if [ "$$should_build" != "no" ] ; then \
	   subdirectories="$(ALL_SUBPROJECTS)"; \
	   target=configure_for_target_archs ; \
      	   $(use_default_directory_args) ; \
	   for sub in $$subdirectories none ; do \
	      if [ $$sub = "none" ] ; then break; fi ; \
	      if [ -h $$sub ] ; then \
		  $(use_absolute_directory_args) ; \
	      fi ; \
              $(PUSHD) $$sub; \
	      $(MAKE) $$target $(configure_for_target_archs_exported_vars) ; \
	      $(POPD) ; \
	   done ; \
	fi)


# Finalizing installation

STRIP_ON_INSTALL = YES

finalize_install_exported_vars = \
	"DSTROOT = $(DSTROOT)" \
	"SRCROOT = $(SRCROOT)" \
	"OBJROOT = $(OBJROOT)" \
	"SYMROOT = $(SYMROOT)" \
	"SYM_DIR = $(SYM_DIR)" \
	"DEVROOT = $(DEVROOT)" \
	"INSTALLDIR = $(INSTALLDIR)" \
	"PRODUCT_ROOT = $(PRODUCT_ROOT)" \
	"RESOURCES_ROOT = $(RESOURCES_ROOT)" \
	"PRODUCT = $(PRODUCT)" \
	"OFILE_DIR = $(OFILE_DIR)" \
	"MAKEFILEDIR = $(MAKEFILEDIR)" \
	"RC_CFLAGS = $(RC_CFLAGS)" \
	"RC_ARCHS = $(RC_ARCHS)" \
	$(extra_finalize_install_exported_vars)

# Finalizing build (e.g. lipo/ln the arch-specific binaries into place)

finalize_install:: strip_myself after_install
	@(subdirectories="$(ALL_SUBPROJECTS)"; \
	target=finalize_install ; \
	for sub in $$subdirectories none ; do \
	   if [ $$sub = "none" ] ; then break; fi ; \
           $(PUSHD) $$sub; \
	   $(MAKE) $$target $(finalize_install_exported_vars) ; \
	   $(POPD) ; \
	done)

strip_myself::

after_install::

.PHONY : before_install install after_install strip_myself finalize_install

# These Makefiles are now strictly dependent on GNU make functionality (plus 
# a couple of simple NEXT-specific additions that will be eventually not be 
# required).  To insure we are running on a proper GNU make implementation,
# the built-in variable GNUMAKE is examined.

check_for_gnumake = \
	if [ "$(GNUMAKE)" != "YES" ] ; then \
		$(ECHO) These Makefiles only work with GNU make. ; \
		$(ECHO) Please do not attempt to use an improper make tool. ; \
		exit 1 ; \
	fi

# Resources stuff:

ENABLE_INFO_DICTIONARY = YES


# The following rule insures that resources for this particular level in the project hierarchy get copied over to the appropriate place.  Note that we depend on VPATH including $(LANGUAGE).lproj so that the LOCAL_RESOURCES are found correctly.  FASTCP is used to minimize the copying of files, since most resources are likely to be up to date most of the time.

.PHONY : resources

resources: $(RESOURCES_ROOT) $(RESOURCE_OFILE) $(OTHER_RESOURCES) $(HELP_OUTPUT_FILE_DIR)/$(HELP_OUTPUT_FILE) 
	@(if [ -n "$(RESOURCES_ROOT)" ] ; then \
 	    if [ -n "$(LOCAL_RESOURCES)" ] ; then \
	       $(MKDIRS) $(RESOURCES_ROOT)/$(LANGUAGE).lproj ; \
	       relative_resources_dir=`$(ECHO) $(RESOURCES_ROOT)/$(LANGUAGE).lproj | $(DOTDOTIFY_PATH)` ; \
	       (cd $(LANGUAGE).lproj; $(FASTCP) $(LOCAL_RESOURCES) $$relative_resources_dir) ; \
	    fi ; \
 	    if [ -n "$(GLOBAL_RESOURCES)" -o -n "$(OTHER_RESOURCES)" ] ; then \
	        $(FASTCP) $(GLOBAL_RESOURCES) $(OTHER_RESOURCES) $(RESOURCES_ROOT) ; \
	    fi ; \
	    if [ -r "$(USER_INFO_FILE)" ] ; then \
	        mergeInfoArgs="$(USER_INFO_FILE)" ; \
	    fi ; \
	    if [ -n "$(REG_FILE)" -a -r "$(RESOURCES_ROOT)/$(REG_FILE)" ] ; then \
	        mergeInfoArgs="$$mergeInfoArgs -regFile $(REG_FILE)" ; \
	    fi ; \
	    if [ -x $(MERGEINFO) -a "$(ENABLE_INFO_DICTIONARY)" = "YES" ] ; then \
	       $(RM) -f $(RESOURCES_ROOT)/$(PROJECT_INFO_FILE) ; \
	       cmd="$(MERGEINFO) PB.project $$mergeInfoArgs $(OTHER_INFO_FILES) -o $(RESOURCES_ROOT)/$(PROJECT_INFO_FILE)" ; \
		$$cmd ; \
	    fi ; \
	fi)

$(RESOURCE_OFILE):
	@(cmd="$(REGGEN) -o $(OFILE_DIR)/$(RESOURCE_OFILE) -project PB.project -rc $(RC_CMD) -regFile $(RESOURCES_ROOT)/$(REG_FILE)" ; \
       	echo $$cmd ; $$cmd)

$(HELP_OUTPUT_FILE_DIR)/$(HELP_OUTPUT_FILE): $(HELP_FILES)
	@(if [ -x $(COMPILEHELP) ] ; then \
	    $(RM) -f $(RESOURCES_ROOT)/$(HELP_OUTPUT_FILE) ; \
	    for sub in $(SUBPROJECTS) none ; do \
		if [ "$$sub" = "none" ] ; then break ; fi ; \
		help_plist="$(DERIVED_SRC_DIR)/$$sub/$(HELP_OUTPUT_FILE)" ; \
		if [ -r $$help_plist ] ; then \
		   subproj_help_plists="$$subproj_help_plists -m $$help_plist" ; \
		fi ; \
	    done ; \
	    if [ -n "$(HELP_FILES)" -o -n "$$subproj_help_plists" ] ; then \
	        $(MKDIRS) $(HELP_OUTPUT_FILE_DIR) ; \
	        cmd="$(COMPILEHELP) -f $(HELP_FILES) $$subproj_help_plists -o $(HELP_OUTPUT_FILE_DIR)/$(HELP_OUTPUT_FILE)" ; \
		 $(ECHO) $$cmd ; $$cmd ; \
	    fi ; \
	 fi)

# rules for copying, cleaning and making dependencies

.PHONY : installsrc copy 

installsrc:: SRCROOT
	@($(MAKE) copy "DEST = $(SRCROOT)" \
		       "SRCROOT = $(SRCROOT)" \
			"OBJROOT = $(OBJROOT)" \
			"SYMROOT = $(SYMROOT)" \
		       "MAKEFILEDIR = $(MAKEFILEDIR)" )

copy:: DEST $(DEST) $(SRCFILES)
	@($(ECHO) "$(TAR) cf - $(SRCFILES) | (cd $(DEST); $(TAR) xf -)"; \
	$(TAR) cf - $(SRCFILES) | (cd $(DEST); $(TAR) xf -) ; \
	for i in $(SUPPORTFILES) none ; do \
	    if [ -r $$i -a ! -r $(DEST)/$$i ] ; then \
		supportfiles="$$supportfiles $$i" ; \
	    fi ; \
	done ; \
	if [ -n "$$supportfiles" ] ; then \
	   $(ECHO) "$(TAR) cf - $$supportfiles | (cd $(DEST); $(TAR) xf -)" ; \
	   $(TAR) cf - $$supportfiles | (cd $(DEST); $(TAR) xf -) ; \
	fi)
	@(subdirectories="$(ALL_SUBPROJECTS)" ; \
	target=copy; \
	beginning_msg="Copying" ; ending_msg="Finished copying" ; \
	$(recurse_on_subdirectories))

SRCROOT DEST:
	@if [ -n "${$@}" ]; then exit 0; \
	else $(ECHO) Must define $@; exit 1; fi

$(DEST):
	@$(MKDIRS) $(DEST)

# Build a set of dependencies for current level into Makefile.depndencies 

Makefile.dependencies::
	@echo Warning: 'depend' target currently non-functional.

   
.broken.Makefile.dependencies:: $(CLASSES) $(MFILES) $(CFILES) $(CCFILES) $(CAPCFILES) $(CAPMFILES) $(CXXFILES) $(CPPFILES) $(INITIAL_TARGETS)
	@($(RM) -f Makefile.dependencies ; \
	if [ "`$(ECHO) $(CLASSES) $(MFILES) $(CFILES) $(CCFILES) $(CAPCFILES) $(CAPMFILES) $(CXXFILES) $(CPPFILES) | wc -w`" != "       0" ] ; then \
		cmd="$(CC) -MM $(PROJECT_SPECIFIC_CFLAGS) $(PROPOGATED_CFLAGS) -I$(SYM_DIR)  $(CLASSES) $(MFILES) $(CFILES) $(CCFILES) $(CAPCFILES) $(CAPMFILES) $(CXXFILES) $(CPPFILES) > Makefile.dependencies" ; \
		echo $$cmd ; $$cmd || ($(RM) -f Makefile.dependencies ; exit 1) ; \
	fi)

		
# Header stuff:

DEV_HEADER_DIR_BASE = $(SYMROOT)
DEV_PROJECT_HEADER_DIR_BASE = $(SYMROOT)
DEV_PUBLIC_HEADER_DIR  = $(DEV_HEADER_DIR_BASE)/Headers
DEV_PROJECT_HEADER_DIR = $(DEV_PROJECT_HEADER_DIR_BASE)/$(PROJECT_HEADERS_DIR_NAME)
DEV_PRIVATE_HEADER_DIR = $(DEV_HEADER_DIR_BASE)/PrivateHeaders

set_header_dirs = \
	public_header_dir="$(PUBLIC_HEADER_DIR)" ; \
	private_header_dir="$(PRIVATE_HEADER_DIR)"

.PHONY : installhdrs installhdrs_recursively copy_my_headers after_installhdrs export_headers export_headers_recursively

installhdrs:: $(OTHER_INSTALLHDRS_DEPENDS)
	@(if [ "$(PROJECT_TYPE)" != "Aggregate" ] ; then \
	    $(ECHO) Making installhdrs for $(NAME) ; \
	    $(set_header_dirs) ; \
	    $(MAKE) installhdrs_recursively \
		"PUBLIC_HEADER_DEST_DIR = $(DSTROOT)$$public_header_dir" \
		"PROJECT_HEADER_DEST_DIR =" \
		"PRIVATE_HEADER_DEST_DIR = $(DSTROOT)$$private_header_dir" \
		"HEADER_COPY_CMD = $(FASTCP)" \
		"BUILD_TARGET = $@" \
	       "MAKEFILEDIR = $(MAKEFILEDIR)" \
	       "RC_CFLAGS = $(RC_CFLAGS)" \
	       "SRCROOT = $(SRCROOT)" \
	       "SYMROOT = $(SYMROOT)" \
	       "OBJROOT = $(OBJROOT)" \
	       "DSTROOT = $(DSTROOT)" ; \
	    $(ECHO) Finished installhdrs for $(NAME) ; \
	fi)

installhdrs_recursively::
	@($(set_should_build) ; \
	if [ "$$should_build" != "no" ] ; then \
	    $(ECHO) ...for $(NAME) ; \
	    $(MAKE) copy_my_headers after_installhdrs \
		"PUBLIC_HEADER_DEST_DIR = $(PUBLIC_HEADER_DEST_DIR)" \
		"PROJECT_HEADER_DEST_DIR = $(PROJECT_HEADER_DEST_DIR)" \
		"PRIVATE_HEADER_DEST_DIR = $(PRIVATE_HEADER_DEST_DIR)" \
		"HEADER_COPY_CMD = $(HEADER_COPY_CMD)" \
		"BUILD_TARGET = $(BUILD_TARGET)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" \
		"DSTROOT = $(DSTROOT)" \
		"SRCROOT = $(SRCROOT)" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" \
		"SYM_DIR = $(SYM_DIR)" ; \
       	    subdirectories="$(ALL_SUBPROJECTS)" ; \
	    target=installhdrs_recursively ; \
  	    $(recurse_on_subdirectories) ; \
	fi)

export_headers:
	@(if [ "$(SKIP_EXPORTING_HEADERS)" != "YES" ] ; then \
	    if [ "$(IS_TOPLEVEL)" = "YES" ] ; then \
		$(ECHO) Exporting headers... ; \
		$(MAKE) export_headers_recursively \
			"TOP_PRODUCT_ROOT = $(TOP_PRODUCT_ROOT)" \
			"BUILD_TARGET = $(BUILD_TARGET)" \
			"MAKEFILEDIR = $(MAKEFILEDIR)" \
			"DSTROOT = $(DSTROOT)" \
			"SRCROOT = $(SRCROOT)" \
			"OBJROOT = $(OBJROOT)" \
			"SYMROOT = $(SYMROOT)" \
			"SYM_DIR = $(SYM_DIR)" ; \
		$(ECHO) Done exporting headers. ; \
	    fi ; \
	fi)

export_headers_recursively:
	@($(set_should_build) ; \
	if [ "$$should_build" != "no" ] ; then \
	    $(ECHO) ...for $(NAME) ; \
	    $(MAKE) copy_my_headers \
		"PUBLIC_HEADER_DEST_DIR = $(DEV_PUBLIC_HEADER_DIR)" \
		"PROJECT_HEADER_DEST_DIR = $(DEV_PROJECT_HEADER_DIR)" \
		"PRIVATE_HEADER_DEST_DIR = $(DEV_PRIVATE_HEADER_DIR)" \
		"HEADER_COPY_CMD = $(CLONEHDRS)" \
		"BUILD_TARGET = $(BUILD_TARGET)" \
		"MAKEFILEDIR = $(MAKEFILEDIR)" \
		"DSTROOT = $(DSTROOT)" \
		"SRCROOT = $(SRCROOT)" \
		"OBJROOT = $(OBJROOT)" \
		"SYMROOT = $(SYMROOT)" \
		"SYM_DIR = $(SYM_DIR)" ; \
	   subdirectories=`$(CHANGES_COMMAND) $(ALL_SUBPROJECTS)` ; \
	   target=export_headers_recursively ; \
  	   $(recurse_on_subdirectories) ;\
	fi)

copy_my_headers:: $(SYM_DIR) $(PUBLIC_HEADERS) $(OTHER_PUBLIC_HEADERS) $(PROJECT_HEADERS) $(OTHER_PROJECT_HEADERS) $(PRIVATE_HEADERS) $(OTHER_PRIVATE_HEADERS)
	@(if [ \( -n "$(PUBLIC_HEADERS)" -o \
	       -n "$(OTHER_PUBLIC_HEADERS)" \) -a \
	       -n "$(PUBLIC_HEADER_DEST_DIR)" ] ; then \
	   header_dir="`$(ECHO) $(PUBLIC_HEADER_DEST_DIR)$(PUBLIC_HEADER_DIR_SUFFIX) | $(TR) -d ' '`" ; \
	   $(MKDIRS) $$header_dir ; \
	   $(HEADER_COPY_CMD) $(PUBLIC_HEADERS) $(OTHER_PUBLIC_HEADERS) $$header_dir ; \
	fi ; \
	if [ \( -n "$(PROJECT_HEADERS)" -o \
	     -n "$(OTHER_PROJECT_HEADERS)" \) -a \
	     -n "$(PROJECT_HEADER_DEST_DIR)" ] ; then \
	   $(MKDIRS) $(PROJECT_HEADER_DEST_DIR) ; \
	   $(HEADER_COPY_CMD) $(PROJECT_HEADERS) $(OTHER_PROJECT_HEADERS) $(PROJECT_HEADER_DEST_DIR) ; \
	fi ; \
	if [ \( -n "$(PRIVATE_HEADERS)" -o \
	     -n "$(OTHER_PRIVATE_HEADERS)" \) -a \
	     -n "$(PRIVATE_HEADER_DEST_DIR)" ] ; then \
	   header_dir="`$(ECHO) $(PRIVATE_HEADER_DEST_DIR)$(PRIVATE_HEADER_DIR_SUFFIX) | $(TR) -d ' '`" ; \
	   $(MKDIRS) $$header_dir ; \
	   $(HEADER_COPY_CMD) $(PRIVATE_HEADERS) $(OTHER_PRIVATE_HEADERS) $$header_dir ; \
	fi)

# precompiled headers are only supported on NEXTSTEP right now

after_installhdrs::
	@(if [ -n "$(PUBLIC_PRECOMPILED_HEADERS)" \
            -a -n "$(PUBLIC_HEADER_DEST_DIR)" \
            -a -z "$(DISABLE_PRECOMPS)" ] ; then \
	   cd $(PUBLIC_HEADER_DEST_DIR) ; \
	   for header in $(PUBLIC_PRECOMPILED_HEADERS) none ; do \
	      if [ $$header = "none" ] ; then break; fi ; \
	      cmd="$(CC) -precomp $(PUBLIC_PRECOMPILED_HEADERS_CFLAGS) $(RC_CFLAGS) $$header";\
	      $(ECHO) $$cmd ; $$cmd ; \
	   done ; \
	fi)


# making the stripped target

ifeq ("$(PLATFORM_OS)", "winnt")

STRIPO = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Libraries/gcc-lib/`$(ARCH_CMD)`/StabsToCodeview.exe
STRIPDIRS = $(OBJROOT)/$(STRIPNAME) $(SYMROOT)/$(STRIPNAME)
GARBAGE += $(STRIPDIRS)

CLONE_AND_STRIP = $(FIND) . '(' -name $(STRIPNAME) -prune ')' \
  -o '(' -type d -exec $(MKDIRS) $(STRIPNAME)/'{}' ';' ')' \
  -o '(' -name '*.exe' -o -name '*.dll' -o -name '*.lib' -name 'lib*.a' ')' \
  -o '(' -name '*.EXE' -o -name '*.DLL' -o -name '*.LIB' -name 'LIB*.A' ')' \
  -o '(' -name '*.ofileList' -o -name '*.ofilelist' -o -name '$(CHANGES_FILE_BASE)*' ')' \
  -o '(' -name '*.o' -exec $(STRIPO) -g0 '{}' -o $(STRIPNAME)/'{}' ';' ')' \
  -o -exec $(CP) -p '{}' $(STRIPNAME)/'{}' ';'

.PHONY: clone_and_strip reinstall_stripped

clone_and_strip:
	$(MKDIRS) $(OBJROOT)/$(STRIPNAME) $(SYMROOT)/$(STRIPNAME)
	cd $(OBJROOT) && $(CLONE_AND_STRIP)
	cd $(SYMROOT) && $(CLONE_AND_STRIP)

reinstall_stripped: clone_and_strip 
	@if $(ECHO) $(OBJROOT) | $(GREP) -v $(STRIPNAME) $(BURY_STDERR) ; then \
	   cmd='$(MAKE) install OBJROOT=$(OBJROOT)/$(STRIPNAME) SYMROOT=$(SYMROOT)/$(STRIPNAME) DEBUG_SYMBOLS_CFLAG= SKIP_EXPORTING_HEADERS=YES' ; \
	   $(ECHO) $$cmd ; $$cmd ; \
	fi

endif

# Cleaning stuff:	

.PHONY : clean actual_clean really_clean

clean:: 
	@(echo == Making clean for $(NAME) == ; \
	$(set_bundle_ext) ; \
	$(process_target_archs) ; \
	if [ -n "$(CLEAN_ALL_SUBPROJECTS)" ] ; then \
	   $(MAKE) actual_clean really_clean \
		   "DEV_HEADER_DIR_BASE = $(DEV_HEADER_DIR_BASE)" \
	          "BUNDLE_EXTENSION = $$bundle_ext" \
		   "SRCROOT = $(SRCROOT)" \
                 "SYMROOT = $(SYMROOT)" \
		   "OBJROOT = $(OBJROOT)" ; \
	else \
	   $(MAKE) actual_clean \
		   "BUNDLE_EXTENSION = $$bundle_ext" \
		   "DEV_HEADER_DIR_BASE = $(DEV_HEADER_DIR_BASE)" \
		   "SRCROOT = $(SRCROOT)" \
		   "SYMROOT = $(SYMROOT)" \
		   "OBJROOT = $(OBJROOT)" ; \
	fi )
	
actual_clean:: 
	@if [ ! -w . ] ; then $(ECHO) '***' project write-protected; exit 1 ; fi
	$(RM) -rf $(GARBAGE)

really_clean::
	@(subdirectories="$(ALL_SUBPROJECTS)" ;\
	target="actual_clean really_clean"; \
	beginning_msg="Cleaning" ; ending_msg="Finished cleaning" ; \
	$(recurse_on_subdirectories))


# Indexing support:
#  - through the use of conditional gnumake makefile assignments context
#    can be set up in preambles and postambles

.PHONY : echo_makefile_variable

set_additional_makefile_context = true

echo_makefile_variable:
	@($(set_additional_makefile_context) ; \
	$(ECHO) $($(VAR_NAME)))

echo_makefile_expression:
	@($(set_additional_makefile_context) ; \
	$(ECHO) $(EXPR_STRING))

-include $(LOCAL_MAKEFILEDIR)/common.make.postamble

