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
# winnt-specific.make
#

OS_PREFIX = WINDOWS_

DISABLE_VERSIONING = YES
DISABLE_FAT_BUILDS = YES
DISABLE_PRECOMPS   = YES
DISABLE_OBJCPLUSPLUS = YES
ALWAYS_USE_OFILELISTS = YES

EXECUTABLE_EXT = .exe
DYNALIB_EXT = .dll
BUNDLE_BINARY_EXT = .dll
STATICLIB_EXT = .lib
LIBRARY_EXT = .lib
LIBRARY_PREF = 

# Tool directories
NEXTDEV_BIN = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_EXECUTABLES_DIR)
NEXTDEV_LIB = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Libraries
NEXT_BIN = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_EXECUTABLES_DIR)/Utilities
BUILD_TOOL_DIR = $(NEXTDEV_BIN)

# Ported tools:
SHELL   = $(NEXT_ROOT)$(SYSTEM_LIBRARY_EXECUTABLES_DIR)/sh
CC      = $(NEXTDEV_BIN)/gcc
TAR     = $(NEXT_BIN)/gnutar
MV      = $(NEXT_BIN)/mv
LS      = $(NEXT_BIN)/ls
RM      = $(NEXT_BIN)/rm
SYMLINK = $(NEXT_BIN)/ln -s
CP      = $(NEXT_BIN)/cp
ECHO    = echo
MKDIRS  = $(NEXT_BIN)/mkdirs
MKDIR   = $(NEXT_BIN)/mkdir
TOUCH   = $(NEXT_BIN)/touch
AWK     = $(NEXT_BIN)/gawk
ARCH_CMD = $(NEXTDEV_BIN)/arch
VERS_STRING = $(NEXTDEV_BIN)/vers_string
LN = $(SYMLINK)
INSTALL = $(NEXTDEV_BIN)/install
INSTALL_HEADERS_CMD = $(CP) -p
TRANSMOGRIFY = $(NEXT_BIN)/ln
SEARCH = $(NEXT_BIN)/fgrep -s
GREP = $(NEXT_BIN)/grep
FIND = $(NEXT_BIN)/find
TR = $(NEXT_BIN)/tr
SED = $(NEXT_BIN)/sed
LIBTOOL = $(NEXTDEV_BIN)/libtool
PSWRAP = $(NEXTDEV_BIN)/pswrap
BASENAME = $(NEXT_BIN)/basename
CAT     = $(NEXT_BIN)/cat
CHMOD   = $(NEXT_BIN)/chmod -f
# The extension must be present on compileHelp
COMPILEHELP = $(NEXTDEV_BIN)/compileHelp.exe
REGGEN = $(NEXTDEV_BIN)/regGen
LEX = $(NEXT_BIN)/flex
YACC = $(NEXT_BIN)/bison

BURY_STDERR = 2> NUL
DOTDOTIFY_PATH = $(SED) -e '/^[^/][^:]/s:^:../:' -e '/^\.$$/s:^:../:'
DOTDOTIFY_IPATHS = $(SED) 's:-I\.\./:-I../../:g'

NUMBER_OF_OBJECT_FILES = "`$(ECHO) $(OFILES) $(OTHER_OFILES) | wc -w`"

# DEFFILE = $(DERIVED_SRC_DIR)/$(NAME).def 
# PRODUCT_DEPENDS += $(DEFFILE)

PLATFORM_SPECIFIC_LIBTOOL_FLAGS = $(DEBUG_SYMBOLS_CFLAG) 
# PLATFORM_SPECIFIC_LIBTOOL_FLAGS += -def $(DEFFILE)

WINDOWS_ENTRY_POINT_LIB = $(NEXTDEV_LIB)/libNSWinMain.a

# The following doesn't do any good with the MS linker.
#DYNAMIC_BUNDLE_UNDEFINED_FLAGS = -undefined suppress

# Makefiles tools
FASTCP = fastcp
CLONEHDRS = $(BUILD_TOOL_DIR)/clonehdrs
CHANGES = $(BUILD_TOOL_DIR)/changes
OFILE_LIST_TOOL = $(BUILD_TOOL_DIR)/ofileListTool 
MERGEINFO = $(BUILD_TOOL_DIR)/mergeInfo.exe
FRAMEWORK_TOOL = $(BUILD_TOOL_DIR)/frameworkFlags
RC_CMD = $(BUILD_TOOL_DIR)/rc.exe

# Important non-ported tools:
STRIP  = $(ECHO) Warning! Not stripping
CHOWN  = $(ECHO) Warning! Not chowning
CHGRP  = $(ECHO) Warning! Not chgrping

# Non-existent tools on winnt - invocation of these will fail

LIPO = $(BUILD_TOOL_DIR)/lipo
ARCH_TOOL = $(BUILD_TOOL_DIR)/arch_tool
DEARCHIFY = $(ARCH_TOOL) -dearchify
ARCHIFY = $(ARCH_TOOL) -archify_list
FIXPRECOMPS = /usr/bin/fixPrecomps
MSGWRAP = /usr/bin/msgwrap -n
MIG    = /usr/bin/mig
RPCGEN = /usr/bin/rpcgen

DYNAMIC_CODE_GEN_CFLAG = -dynamic
STATIC_CODE_GEN_CFLAG = -static
DYNAMIC_LIBTOOL_FLAGS = -dynamic
STATIC_LIBTOOL_FLAGS = -static

# Install options (strip is temporarily disabled)
# LIBRARY_INSTALL_OPTS = -s

# Defaults for who to chown executables to when installing
INSTALL_AS_USER = root
INSTALL_AS_GROUP = wheel



