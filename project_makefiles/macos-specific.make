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
# macos-specific.make
#

OS_PREFIX = NEXTSTEP_

BE_PARANOID = YES

SHELL  = /bin/sh
CC = /usr/bin/cc
FASTCP = /usr/lib/fastcp
CLONEHDRS = /usr/lib/clonehdrs
CHANGES = /usr/lib/changes
ARCH_TOOL = /usr/lib/arch_tool
OFILE_LIST_TOOL = /usr/lib/ofileListTool 
DEARCHIFY = $(ARCH_TOOL) -dearchify
ARCHIFY = $(ARCH_TOOL) -archify_list
ifneq "" "$(wildcard /usr/etc/chown)"
  CHOWN = /usr/etc/chown -f
else
  CHOWN = /usr/sbin/chown -f
endif
ifneq "" "$(wildcard /bin/chgrp)"
  CHGRP = /bin/chgrp -f
else
  CHGRP = /usr/bin/chgrp -f
endif
CHMOD  = /bin/chmod -f
TAR    = /usr/bin/gnutar

ifeq "$(LIBRARY_STYLE)" "STATIC"
LIBTOOL = /usr/bin/libtool
else
LIBTOOL = $(CC) -dynamiclib
endif

STRIP  = /usr/bin/strip
RM     = /bin/rm
SYMLINK = /bin/ln -s
CP     = /bin/cp
INSTALL = /usr/bin/install
INSTALL_HEADERS_CMD = $(CP) -p
ECHO   = echo
ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif
TOUCH  = /usr/bin/touch
AWK    = /usr/bin/awk
PSWRAP = /usr/bin/pswrap
MSGWRAP = /usr/bin/msgwrap -n
MIG    = $(NEXT_ROOT)/usr/bin/mig
ifneq "" "$(wildcard /usr/bin/rpcgen)"
  RPCGEN = /usr/bin/rpcgen
else
  RPCGEN = /usr/sbin/rpcgen
endif
FIXPRECOMPS = /usr/bin/fixPrecomps
LIPO = /usr/bin/lipo
LN = $(SYMLINK)
MERGEINFO = /usr/lib/mergeInfo
VERS_STRING = /usr/bin/vers_string
ARCH_CMD = /usr/bin/arch
TRANSMOGRIFY = /bin/ln
SEARCH = /usr/bin/fgrep
FIND = /usr/bin/find
CAT = /bin/cat
MV = /bin/mv
TR = /usr/bin/tr
MKDIR = /bin/mkdir
ifneq "" "$(wildcard /bin/sed)"
  SED = /bin/sed
else
  SED = /usr/bin/sed
endif
LEX = /usr/bin/lex
YACC = /usr/bin/yacc
BASENAME = /usr/bin/basename

COMPILEHELP = /usr/bin/compileHelp

FRAMEWORK_TOOL = /usr/lib/frameworkFlags

BURY_STDERR = 2> /dev/null
DOTDOTIFY_PATH = $(SED) '/^[^\/]/s:^:../:'
DOTDOTIFY_IPATHS = $(SED) 's:-I\.\./:-I../../:g'

NUMBER_OF_OBJECT_FILES = "`$(ECHO) $(OFILES) $(OTHER_OFILES) | wc -w`"

OBJCPLUS_FLAG = -ObjC++

PIPE_CFLAG = -pipe

# To handle workspace, which requires icons in sections:
PLATFORM_APP_LDFLAGS = $(APPICONFLAGS)

DYNAMIC_BUNDLE_UNDEFINED_FLAGS = -undefined suppress

DYNAMIC_CODE_GEN_CFLAG = -dynamic
STATIC_CODE_GEN_CFLAG = -static
DYNAMIC_LIBTOOL_FLAGS = -dynamic -install_name $(DYLIB_INSTALL_DIR)/$(DYLIB_INSTALL_NAME)
STATIC_LIBTOOL_FLAGS = -static

DYNALIB_EXT = .dylib
STATICLIB_EXT = .a
LIBRARY_PREF = lib
BUNDLE_BINARY_EXT =

# Default strip options
DYLD_EXEC_STRIP_OPTS = -S
APP_STRIP_OPTS = $(DYLD_EXEC_STRIP_OPTS)
TOOL_STRIP_OPTS =  $(DYLD_EXEC_STRIP_OPTS) 
LIBRARY_STRIP_OPTS = -S   # Note: -S strips debugging symbols
LIBRARY_INSTALL_OPTS = -c -S -S
DYNAMIC_STRIP_OPTS = -S

# Defaults for who to chown executables to when installing
INSTALL_AS_USER = root
INSTALL_AS_GROUP = wheel
