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
# commands-SOLARIS.make
#
# commands needed by the makefiles on SOLARIS
#

NEXTLIB_BIN = $(NEXT_ROOT)$(SYSTEM_LIBRARY_EXECUTABLES_DIR)
NEXTDEV_BIN = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_DIR)/Executables
SUNDEV_BIN = /usr/ccs/bin
#
# RDW 04/21/1999 -- Changed LOCAL_BIN to the new location.
#
LOCAL_BIN = $(NEXT_ROOT)/Local/Executables

JAVA_HOME := $(shell javaconfig Home)
ifeq "" "$(JAVA_HOME)"
JAVA_HOME = $(NEXT_ROOT)$(SYSTEM_LIBRARY_DIR)/JDK
endif

JDKDIR = $(JAVA_HOME)
JDKBINDIR = $(JDKDIR)/bin
JDKLIBDIR = $(JDKDIR)/lib

ECHO    = /usr/ucb/echo
NULL = /dev/null

CD = cd
RM = /bin/rm
LN = /bin/ln
SYMLINK = /bin/ln -s
CP = /bin/cp
MV = /bin/mv
FASTCP = $(NEXTDEV_BIN)/fastcp
TAR	= $(NEXTLIB_BIN)/gnutar
MKDIRS	= /usr/bin/mkdir -p
CAT = /bin/cat
TAIL = /usr/bin/tail
TOUCH   = /usr/ucb/touch
FIND = /usr/bin/find
GREP = /bin/grep
STRIP  = $(SUNDEV_BIN)/strip
CHGRP   = /bin/chgrp -f
CHMOD   = /bin/chmod -f
CHOWN   = /bin/chown -f

ARCH_CMD = $(NEXTDEV_BIN)/arch
VERS_STRING = $(LOCAL_BIN)/vers_string
MERGEINFO = $(NEXTDEV_BIN)/mergeInfo
OFILE_LIST_TOOL = $(NEXTDEV_BIN)/ofileListTool 
FRAMEWORK_TOOL = $(NEXTDEV_BIN)/frameworkFlags
NEWER = $(MAKEFILEDIR)/newer
DOTDOTIFY = $(MAKEFILEDIR)/dotdotify
CLONEHDRS = $(MAKEFILEDIR)/clonehdrs
MIG = $(NEXTDEV_BIN)/mig
MSGWRAP = $(NEXTDEV_BIN)/msgwrap
PSWRAP = $(NEXTDEV_BIN)/pswrap
RPCGEN = /bin/rpcgen
LEX = $(SUNDEV_BIN)/lex
YACC = $(SUNDEV_BIN)/yacc
SED	= /usr/ucb/sed
TR = /usr/ucb/tr
CC	= $(NEXTDEV_BIN)/gcc
LD = $(CC)
AR = /usr/ccs/bin/ar
RANLIB = $(SUNDEV_BIN)/ranlib
LIBTOOL = $(NEXTDEV_BIN)/libtool

JAVATOOL = $(NEXTDEV_BIN)/javatool
BRIDGET = $(NEXTDEV_BIN)/bridget
GENFORCELOAD = $(NEXTDEV_BIN)/genforceload
GENCLASSPATH = $(NEXTDEV_BIN)/genclasspath
PLISTREAD = $(NEXTDEV_BIN)/plistread
BUILDFILTER = $(NEXTDEV_BIN)/BuildFilter

JAVA = $(JDKBINDIR)/java
JAVAH = $(JDKBINDIR)/javah
JAVAP = $(JDKBINDIR)/javap
JAVADOC = $(JDKBINDIR)/javadoc
RMIC = $(JDKBINDIR)/rmic
RMIREGISTRY = $(JDKBINDIR)/rmiregistry
SERIALVER = $(JDKBINDIR)/serialver
JAR = $(JDKBINDIR)/jar
MKZIP = $(NEXTDEV_BIN)/zip


# The following is not a command but it is platform-specific.

JAVA_PATH_SEPARATOR=:

