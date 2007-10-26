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
# commands-MACOS.make
#
# commands needed by the makefiles on MACOS
#

ifneq "" "$(wildcard /usr/bin/javaconfig)"
JAVA_HOME := $(shell javaconfig Home)
endif
ifeq "" "$(JAVA_HOME)"
JAVA_HOME = $(NEXT_ROOT)$(SYSTEM_LIBRARY_DIR)/Frameworks/JavaVM.framework/Home
endif

# If JAVA_HOME is the default, use /usr/bin for JDKBINDIR since javacSpec.plist
# is full of references to /usr/bin/javac. By the way, it should contain
# references to $(JAVAC) to be portable!

JDKDIR = $(JAVA_HOME)
ifeq "$(NEXT_ROOT)$(SYSTEM_LIBRARY_DIR)/Frameworks/JavaVM.framework/Home" "$(JAVA_HOME)"
JDKBINDIR = /usr/bin
else
JDKBINDIR = $(JDKDIR)/bin
endif
JDKLIBDIR = $(JDKDIR)/lib

ECHO = echo
NULL = /dev/null

CD = cd
RM = /bin/rm
LN = /bin/ln
SYMLINK = /bin/ln -s
CP = /bin/cp
MV = /bin/mv
FASTCP = /usr/lib/fastcp
TAR = /usr/bin/gnutar
ifneq "" "$(wildcard /bin/mkdirs)"
  MKDIRS = /bin/mkdirs
else
  MKDIRS = /bin/mkdir -p
endif
CAT = /bin/cat
TAIL = /usr/bin/tail
TOUCH = /usr/bin/touch
FIND = /usr/bin/find
GREP = /bin/grep
STRIP = /usr/bin/strip
CHMOD = /bin/chmod
ifneq "" "$(wildcard /etc/chown)"
  CHOWN = /etc/chown
else
  CHOWN = /usr/sbin/chown
endif
ifneq "" "$(wildcard /bin/chgrp)"
  CHGRP = /bin/chgrp
else
  CHGRP = /usr/bin/chgrp
endif
COMPRESSMANPAGES = $(MAKEFILEPATH)/bin/compress-man-pages.pl -d $(DSTROOT)

AWK = /usr/bin/awk
TR = /usr/bin/tr

ARCH_CMD = /usr/bin/arch
VERS_STRING = /usr/bin/vers_string
FIXPRECOMPS = /usr/bin/fixPrecomps
MERGEINFO = /usr/lib/mergeInfo
COMPILEHELP = /usr/bin/compileHelp
OFILE_LIST_TOOL = /usr/lib/ofileListTool
FRAMEWORK_TOOL = /usr/lib/frameworkFlags
NEWER = $(MAKEFILEDIR)/newer
DOTDOTIFY = $(MAKEFILEDIR)/dotdotify
CLONEHDRS = $(MAKEFILEDIR)/clonehdrs
MIG = $(NEXT_ROOT)/usr/bin/mig
MSGWRAP = /usr/bin/msgwrap
PSWRAP = /usr/bin/pswrap
ifneq "" "$(wildcard /usr/bin/rpcgen)"
  RPCGEN = /usr/bin/rpcgen
else
  RPCGEN = /usr/sbin/rpcgen
endif
YACC = /usr/bin/yacc
LEX = /usr/bin/lex
ifneq "" "$(wildcard /bin/sed)"
  SED = /bin/sed
else
  SED = /usr/bin/sed
endif
CC = /usr/bin/cc
LD = $(CC)
AR = /usr/bin/ar
RANLIB = /usr/bin/ranlib
LIPO = /usr/bin/lipo

ifeq "$(LIBRARY_STYLE)" "STATIC"
LIBTOOL = /usr/bin/libtool
else
LIBTOOL = $(CC) -dynamiclib $(ARCHITECTURE_FLAGS)
endif

JAVATOOL = /usr/bin/javatool
BRIDGET = /usr/bin/bridget
BUILDFILTER = /usr/lib/BuildFilter
EOPREINDEX = $(NEXT_ROOT)/usr/bin/eopreindex

GENFORCELOAD = /usr/lib/genforceload
GENCLASSPATH = /usr/lib/genclasspath

JAVA = $(JDKBINDIR)/java
JAVAH = $(JDKBINDIR)/javah
JAVAP = $(JDKBINDIR)/javap
JAVADOC = $(JDKBINDIR)/javadoc
RMIC = $(JDKBINDIR)/rmic
RMIREGISTRY = $(JDKBINDIR)/rmiregistry
SERIALVER = $(JDKBINDIR)/serialver
JAR = $(JDKBINDIR)/jar

MKZIP = /usr/bin/zip

# The following is not a command but it is platform-specific.

JAVA_PATH_SEPARATOR=:

PLISTREAD = /usr/lib/plistread
