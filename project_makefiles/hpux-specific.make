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
# hpux-specific.make
#

#DISABLE_VERSIONING = YES
DISABLE_FAT_BUILDS = YES
DISABLE_PRECOMPS   = YES

OS_PREFIX = PDO_UNIX_

NEXTDEV_BIN = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Executables
HPDEV_BIN = /usr/ccs/bin
LOCAL_BIN = $(NEXT_ROOT)/local/bin

# OS-supplied tools
AWK     = /bin/awk
BASENAME = /usr/ucb/basename
CAT     = /bin/cat
CHGRP   = /bin/chgrp -h
CHMOD   = /bin/chmod
CHOWN   = /bin/chown -h
CP      = /bin/cp
ECHO    = /bin/echo
INSTALL_HEADERS_CMD = $(CP) -p
LN      = /bin/ln -s
MKDIR	= /usr/bin/mkdir
MKDIRS  = /usr/bin/mkdir -p
MV	= /bin/mv
RM      = /bin/rm
SEARCH	= /bin/fgrep
SED	= /bin/sed
SHELL   = /bin/sh
SYMLINK = /bin/ln -s
TOUCH   = /bin/touch
TR	= /bin/tr
TRANSMOGRIFY = /bin/ln
STRIP  = $(HPDEV_BIN)/strip
LEX = $(HPDEV_BIN)/lex
YACC = $(HPDEV_BIN)/yacc

# NeXT-supplied tools
ARCH_CMD = $(NEXTDEV_BIN)/arch
CC	= $(NEXTDEV_BIN)/gcc
TAR	= $(NEXTDEV_BIN)/gnutar
INSTALL	= $(NEXTDEV_BIN)/installtool
VERS_STRING = $(LOCAL_BIN)/vers_string

FASTCP = $(NEXTDEV_BIN)/fastcp
CLONEHDRS = $(NEXTDEV_BIN)/clonehdrs
CHANGES = $(NEXTDEV_BIN)/changes
ARCH_TOOL = $(NEXTDEV_BIN)/arch_tool
OFILE_LIST_TOOL = $(NEXTDEV_BIN)/ofileListTool 
DEARCHIFY = $(ARCH_TOOL) -dearchify
ARCHIFY = $(ARCH_TOOL) -archify_list
LIBTOOL = $(NEXTDEV_BIN)/libtool
FRAMEWORK_TOOL = $(NEXTDEV_BIN)/frameworkFlags
MERGEINFO = $(NEXTDEV_BIN)/mergeInfo
COMPILEHELP = $(NEXTDEV_BIN)/compileHelp

BURY_STDERR = 2> /dev/null
DOTDOTIFY_PATH = $(SED) '/^[^/]/s:^:../:'
DOTDOTIFY_IPATHS = $(SED) 's:-I\.\./:-I../../:g'

OBJCPLUS_FLAG = -ObjC++
PIPE_CFLAG = -pipe

DYNAMIC_LIBTOOL_FLAGS = -dynamic
STATIC_LIBTOOL_FLAGS = -static
DYNAMIC_STRIP_OPTS = -r

# This will need to be changed, I suspect...
DYNALIB_EXT = .a
STATICLIB_EXT = .a
LIBRARY_PREF = lib
BUNDLE_BINARY_EXT =

# Install options
#LIBRARY_INSTALL_OPTS = -striplib
LIBRARY_INSTALL_OPTS =

# Defaults for who to chown executables to when installing
INSTALL_AS_USER = root
INSTALL_AS_GROUP = bin

