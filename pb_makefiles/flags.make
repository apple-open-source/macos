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
# flags.make
#
# This makefile defines the flags which are passed to the compiler, linker,
# and other build tools.  With the exception of the LDFLAGS, all flags found
# here are used by the commands in implicitrules.make
#
# IMPORTED VARIABLES
#    You may define the following variables in your Makefile.preamble
#
#    OTHER_CFLAGS, LOCAL_CFLAGS:  additional flags to pass to the compiler
#	Note that $(OTHER_CFLAGS) and $(LOCAL_CFLAGS) are used for .h, .c, .m,
#	.cc, .cxx, .C, .mm, and .M files.  There is no need to respecify the
#	flags in OTHER_MFLAGS, etc.
#	Note also that OTHER_... flags are inherited by subprojects, while 
#	LOCAL_... flags are only used by the current project.
#    OTHER_MFLAGS, LOCAL_MFLAGS:  additional flags for .m files
#    OTHER_CCFLAGS, LOCAL_CCFLAGS:  additional flags for .cc, .cxx, and .C files
#    OTHER_MMFLAGS, LOCAL_MMFLAGS:  additional flags for .mm and .M files
#    OTHER_PRECOMPFLAGS, LOCAL_PRECOMPFLAGS:  additional flags used when
#	precompiling header files
#    OTHER_LDFLAGS, LOCAL_LDFLAGS:  additional flags passed to ld and libtool
#    OTHER_PSWFLAGS, LOCAL_PSWFLAGS:  additional flags passed to pswrap
#    OTHER_RPCFLAGS, LOCAL_RPCFLAGS:  additional flags passed to rpcgen
#    OTHER_YFLAGS, LOCAL_YFLAGS:  additional flags passed to yacc
#    OTHER_LFLAGS, LOCAL_LFLAGS:  additional flags passed to lex
#
# EXPORTED VARIABLES
#    You may use the following variables in your rules.
#
#    ALL_CFLAGS:  flags to pass when compiling .c files
#    ALL_MFLAGS:  flags to pass when compiling .m files
#    ALL_CCFLAGS:  flags to pass when compiling .cc, .cxx, and .C files
#    ALL_MMFLAGS:  flags to pass when compiling .mm, .mxx, and .M files
#    ALL_PRECOMPFLAGS:  flags to pass when precompiling .h files
#    ALL_LDFLAGS:  flags to pass when linking object files
#    ALL_LIBTOOL_FLAGS:  flags to pass when libtooling object files
#    ALL_PSWFLAGS:  flags to pass when processing .psw and .pswm (pswrap) files
#    ALL_RPCFLAGS:  flags to pass when processing .rpc (rpcgen) files
#    ALL_YFLAGS:  flags to pass when processing .y (yacc) files
#    ALL_LFLAGS:  flags to pass when processing .l (lex) files
#
# OVERRIDABLE VARIBLES
#    The following variables are defined by flags.make and may be overridden
#    in your makefile.postamble.
#
#    WARNING_CFLAGS:  flag used to set warning level (defaults to -Wmost)
#    DEBUG_SYMBOLS_CFLAGS:  debug-symbol flag passed to all builds (defaults
#	to -g)
#    DEBUG_BUILD_CFLAGS:  flags passed during debug builds (defaults to -DDEBUG)
#    OPTIMIZE_BUILD_CFLAGS:  flags passed during optimized builds (defaults
#	to -Os)
#    PROFILE_BUILD_CFLAGS:  flags passed during profile builds (defaults
#	to -pg -DPROFILE)
#    LOCAL_DIR_INCLUDE_DIRECTIVE:  flag used to add current directory to
#	the include path (defaults to -I.)
#    DEBUG_BUILD_LDFLAGS, OPTIMIZE_BUILD_LDFLAGS, PROFILE_BUILD_LDFLAGS: flags
#	passed to ld/libtool (default to nothing)
#

#
# OS-specific flags
#

ifeq "MACOS" "$(OS)"
OS_CFLAGS = -pipe
OS_LDFLAGS = $(SECTORDER_FLAGS)
endif
ifeq "NEXTSTEP" "$(OS)"
OS_CFLAGS = -pipe
OS_LDFLAGS = $(SECTORDER_FLAGS)
endif
ifeq "HPUX" "$(OS)"
OS_CFLAGS = -dynamic
endif

#
# flags defined in Makefile by ProjectBuilder
#

PB_CFLAGS += $($(PLATFORM_TYPE)_PB_CFLAGS)
PB_MFLAGS += $($(PLATFORM_TYPE)_PB_MFLAGS)
PB_CCFLAGS += $($(PLATFORM_TYPE)_PB_CCFLAGS)
PB_MMFLAGS += $($(PLATFORM_TYPE)_PB_MMFLAGS)
PB_PRECOMPFLAGS += $($(PLATFORM_TYPE)_PRECOMPFLAGS)
PB_LDFLAGS += $($(PLATFORM_TYPE)_PB_LDFLAGS)
PB_PSWFLAGS += $($(PLATFORM_TYPE)_PB_PSWFLAGS)
PB_RPCFLAGS += $($(PLATFORM_TYPE)_PB_RPCFLAGS)
PB_YFLAGS += $($(PLATFORM_TYPE)_PB_YFLAGS)
PB_LFLAGS += $($(PLATFORM_TYPE)_PB_LFLAGS)

#
# optional flags -- note that optimization is on by default
#

export OPTIMIZE
export DEBUG
export PROFILE

ifndef OPTIMIZE
OPTIMIZE = YES
endif

DEBUG_SYMBOLS_CFLAG = -g # this flag is passed to all builds, not just debug builds
DEBUG_BUILD_CFLAGS = -DDEBUG # this flag is only passed to debug builds
DEBUG_BUILD_JFLAGS = -g # this flag is only passed to debug builds
DEBUG_BUILD_LDFLAGS =
OPTIMIZE_BUILD_CFLAGS = -Os
OPTIMIZE_BUILD_LDFLAGS =
PROFILE_BUILD_CFLAGS = -pg -DPROFILE
ifeq "$(OS)" "SOLARIS"
PROFILE_BUILD_LDFLAGS = -pg -ldl
else
PROFILE_BUILD_LDFLAGS = -pg
endif

# the ifndef is an optimization -- OFILE_DIR is specified on the command line
# for recursion, so there's no need to spend time invoking the shell.
# ifndef RECURSING
# OFILE_DIR_SUFFIX := $(OFILE_DIR_SUFFIX)-$(shell echo $(TARGET_ARCHS) | tr " " "-")
# endif

ifeq "YES" "$(OPTIMIZE)"
OPTIONAL_CFLAGS += $(OPTIMIZE_BUILD_CFLAGS)
OPTIONAL_LDFLAGS += $(OPTIMIZE_BUILD_LDFLAGS)
OPTIONAL_LIBS += $(OPTIMIZE_BUILD_LIBS)
OPTIONAL_FRAMEWORKS += $(OPTIMIZE_BUILD_FRAMEWORKS)
OFILE_DIR_SUFFIX := $(OFILE_DIR_SUFFIX)-optimized
endif

ifeq "YES" "$(DEBUG)"
OPTIONAL_CFLAGS += $(DEBUG_BUILD_CFLAGS)
OPTIONAL_JFLAGS += $(DEBUG_BUILD_JFLAGS)
OPTIONAL_LDFLAGS += $(DEBUG_BUILD_LDFLAGS)
OPTIONAL_LIBS += $(DEBUG_BUILD_LIBS)
OPTIONAL_FRAMEWORKS += $(DEBUG_BUILD_FRAMEWORKS)
OFILE_DIR_SUFFIX := $(OFILE_DIR_SUFFIX)-debug
endif

ifeq "YES" "$(PROFILE)"
OPTIONAL_CFLAGS += $(PROFILE_BUILD_CFLAGS)
OPTIONAL_LDFLAGS += $(PROFILE_BUILD_LDFLAGS)
OPTIONAL_LIBS += $(PROFILE_BUILD_LIBS)
OPTIONAL_FRAMEWORKS += $(PROFILE_BUILD_FRAMEWORKS)
OFILE_DIR_SUFFIX := $(OFILE_DIR_SUFFIX)-profile
endif

#
# recursive flags
#

export RECURSIVE_CFLAGS += $(HEADER_PATHS) $(FRAMEWORK_PATHS) $(PB_CFLAGS) $(OTHER_CFLAGS)
export RECURSIVE_MFLAGS += $(OTHER_MFLAGS)
export RECURSIVE_CCFLAGS += $(OTHER_CCFLAGS)
export RECURSIVE_MMFLAGS += $(OTHER_MMFLAGS)
export RECURSIVE_PRECOMPFLAGS += $(OTHER_PRECOMPFLAGS)
export RECURSIVE_LDFLAGS += $(FRAMEWORK_PATHS) $(LIBRARY_PATHS) $(OTHER_LDFLAGS)
export RECURSIVE_PSWFLAGS += $(OTHER_PSWFLAGS)
export RECURSIVE_RPCFLAGS += $(OTHER_RPCFLAGS)
export RECURSIVE_YFLAGS  += $(OTHER_YFLAGS)
export RECURSIVE_LFLAGS  += $(OTHER_LFLAGS)

CUMULATIVE_VARIABLES += RECURSIVE_CFLAGS RECURSIVE_MFLAGS RECURSIVE_CCFLAGS RECURSIVE_MMFLAGS RECURSIVE_PRECOMPFLAGS RECURSIVE_LDFLAGS RECURSIVE_PSWFLAGS RECURSIVE_RPCFLAGS RECURSIVE_YFLAGS RECURSIVE_LFLAGS RECURSIVE_VARIABLES

#
# nonrecursive flags
#

ARCHFULL_RC_CFLAGS = $(foreach X, $(RC_ARCHS),$(addprefix -arch , $(X)))
ARCHLESS_RC_CFLAGS = $(filter-out $(ARCHFULL_RC_CFLAGS), $(RC_CFLAGS))

# All the requested archs
# RDW 04/15/1999 -- Don't pass '-arch foo' on PDO since the
#                   compiler will just complain
ifeq ($(PLATFORM_TYPE), PDO_UNIX)
  ARCHITECTURE_FLAGS =
else
  ARCHITECTURE_FLAGS = $(addprefix -arch ,$(ADJUSTED_TARGET_ARCHS))
endif

ifeq "$(JAVAC)" ""
ifneq "" "$(wildcard /usr/bin/javaconfig)"
JAVAC = $(shell javaconfig Compiler)
endif
endif

ifeq "WINDOWS" "$(OS)"
CLASSPATH_DELIMITER = ;
else
CLASSPATH_DELIMITER = :
endif

ifeq "$(CLASSPATH)" ""
CLASSPATH = $(shell javaconfig DefaultClasspath)
else
CLASSPATH := $(CLASSPATH)$(CLASSPATH_DELIMITER)$(shell javaconfig DefaultClasspath)
endif

CLASSPATH_CLIENT = $(CLASSPATH)

#
# Turning off multi-file compile
#

JAVATOOL_ARGS = -javac $(JAVAC) -classpath "$(CLASSPATH)"
JAVATOOL_ARGS_CLIENT = -javac $(JAVAC) -classpath "$(CLASSPATH_CLIENT)"

LOCAL_DIR_INCLUDE_DIRECTIVE = -I.
WARNING_CFLAGS = -Wmost
ifneq "mwcc" "$(notdir $(CC))"
ifeq "MACOS" "$(OS)"
PRECOMP_CFLAGS = 
endif
ifeq "NEXTSTEP" "$(OS)"
PRECOMP_CFLAGS = -precomp-trustfile $(PRECOMP_TRUSTFILE)
endif
endif

NONRECURSIVE_CFLAGS = $(ARCHLESS_RC_CFLAGS) $(WARNING_CFLAGS) $(PRECOMP_CFLAGS) $(DEBUG_SYMBOLS_CFLAG) -fno-common -I$(PROJECT_HDR_DIR) -I$(PRIVATE_HDR_DIR) -I$(PUBLIC_HDR_DIR) -I$(SFILE_DIR) $(LOCAL_DIR_INCLUDE_DIRECTIVE) $(OS_CFLAGS) $(PROJTYPE_CFLAGS) $(LOCAL_CFLAGS)
NONRECURSIVE_MFLAGS  = -ObjC $(OS_MFLAGS) $(PROJTYPE_MFLAGS) $(PB_MFLAGS) $(LOCAL_MFLAGS)
NONRECURSIVE_CCFLAGS = $(OS_CCFLAGS) $(PROJTYPE_CCFLAGS) $(PB_CCFLAGS) $(LOCAL_CCFLAGS)
NONRECURSIVE_MMFLAGS = -ObjC++ $(OS_MMFLAGS) $(PROJTYPE_MMFLAGS) $(PB_MMFLAGS) $(LOCAL_MMFLAGS)
NONRECURSIVE_PRECOMPFLAGS = $(OS_PRECOMPFLAGS) $(PROJTYPE_PRECOMPFLAGS) $(PB_PRECOMPFLAGS) $(LOCAL_PRECOMPFLAGS)
NONRECURSIVE_LDFLAGS = -L$(OFILE_DIR) $(OS_LDFLAGS) $(PROJTYPE_LDFLAGS) $(PB_LDFLAGS) $(LOCAL_LDFLAGS)
NONRECURSIVE_PSWFLAGS = -H AppKit $(PROJTYPE_PSWFLAGS) $(PB_PSWFLAGS) $(LOCAL_PSWFLAGS)
NONRECURSIVE_RPCFLAGS = $(PROJTYPE_RPCFLAGS) $(PB_RPCFLAGS)
NONRECURSIVE_YFLAGS  = -d $(OS_YFLAGS) $(PROJTYPE_YFLAGS) $(PB_YFLAGS) $(LOCAL_YFLAGS)
NONRECURSIVE_LFLAGS  = $(OS_LFLAGS) $(PROJTYPE_LFLAGS) $(PB_LFLAGS) $(LOCAL_LFLAGS)

ifeq "YES" "$(JAVA_ENABLED)"
NONRECURSIVE_JAVA_CFLAGS = -DJAVA_OPENSTEP_ENABLED
endif

ifeq "YES" "$(JAVA_USED)"
RECURSIVE_JAVA_CFLAGS = -DJAVA_OPENSTEP

# Flags used to build a zip archive of Java classes. This archive should not
# be compressed!

JAVA_ZIP_COMPRESSION_LEVEL = 0
JAVA_JAR_COMPRESSION_LEVEL = 0

ifneq "" "$(JAVA_JAR_PARTIAL_MANIFEST)"
JAVA_JAR_MANIFEST_FLAGS = m
endif
ifneq "" "$(JAVA_JAR_PARTIAL_MANIFEST_CLIENT)"
JAVA_JAR_MANIFEST_FLAGS_CLIENT = m
endif

# Note, the order given in the JAVA_JAR_FLAGS appears to be important.
# If a manifest file is specified is should come before the 'f' 
# flag and the manifest file should be specified as the first argument.

JAVA_ZIP_FLAGS = -ug$(JAVA_ZIP_COMPRESSION_LEVEL)
JAVA_JAR_FLAGS = -c$(JAVA_JAR_MANIFEST_FLAGS)f$(JAVA_JAR_COMPRESSION_LEVEL)
JAVA_JAR_FLAGS_CLIENT = -c$(JAVA_JAR_MANIFEST_FLAGS_CLIENT)f$(JAVA_JAR_COMPRESSION_LEVEL)
endif

#
# build flags
#

ALL_CFLAGS = $(OPTIONAL_CFLAGS) $(NONRECURSIVE_CFLAGS) $(NONRECURSIVE_JAVA_CFLAGS) $(RECURSIVE_CFLAGS) $(RECURSIVE_JAVA_CFLAGS)
ALL_MFLAGS =  $(ALL_CFLAGS) $(NONRECURSIVE_MFLAGS) $(RECURSIVE_MFLAGS)
ALL_CCFLAGS = $(ALL_CFLAGS) $(NONRECURSIVE_CCFLAGS) $(RECURSIVE_CCFLAGS)
ALL_MMFLAGS = $(ALL_CFLAGS) $(NONRECURSIVE_MMFLAGS) $(RECURSIVE_MMFLAGS)
ALL_PRECOMPFLAGS = $(ARCHITECTURE_FLAGS) $(ALL_CFLAGS) $(NONRECURSIVE_PRECOMPFLAGS) $(RECURSIVE_PRECOMPFLAGS)
ALL_LDFLAGS = $(ARCHLESS_RC_CFLAGS) $(OPTIONAL_LDFLAGS) $(NONRECURSIVE_LDFLAGS) $(RECURSIVE_LDFLAGS)
ifeq "$(LIBRARY_STYLE)" "STATIC"
ALL_LIBTOOL_FLAGS = $(filter-out -pg, $(filter-out -g, $(OPTIONAL_LDFLAGS) $(NONRECURSIVE_LDFLAGS) $(RECURSIVE_LDFLAGS)))
else
ALL_LIBTOOL_FLAGS = $(ALL_LDFLAGS)
endif
ALL_PSWFLAGS = $(NONRECURSIVE_PSWFLAGS) $(RECURSIVE_PSWFLAGS)
ALL_RPCFLAGS = $(NONRECURSIVE_RPCFLAGS) $(RECURSIVE_RPCFLAGS)
ALL_YFLAGS = $(NONRECURSIVE_YFLAGS) $(RECURSIVE_YFLAGS)
ALL_LFLAGS = $(NONRECURSIVE_LFLAGS) $(RECURSIVE_LFLAGS)
