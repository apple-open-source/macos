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
# commands-WINDOWS.make
#
# commands needed by the makefiles on Windows
#

ECHO = echo
NULL = NUL

UTILDIR = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_EXECUTABLES_DIR)/Utilities
DEVDIR = $(NEXT_ROOT)$(SYSTEM_DEVELOPER_EXECUTABLES_DIR)

JAVA_HOME := $(shell javaconfig Home)
ifeq "" "$(JAVA_HOME)"
JAVA_HOME = $(NEXT_ROOT)$(SYSTEM_LIBRARY_DIR)/Jdk
endif

JDKDIR = $(JAVA_HOME)
JDKBINDIR = $(JDKDIR)/bin
JDKLIBDIR = $(JDKDIR)/lib

CD = cd
RM = $(UTILDIR)/rm
LN = $(UTILDIR)/ln
SYMLINK = $(UTILDIR)/cp -rfp
CP = $(UTILDIR)/cp
MV = $(UTILDIR)/mv
FASTCP = $(DEVDIR)/fastcp
TAR = $(UTILDIR)/tar
MKDIRS = $(UTILDIR)/mkdirs
CAT = $(UTILDIR)/cat
TAIL = $(UTILDIR)/tail
TOUCH = $(UTILDIR)/touch
FIND = $(UTILDIR)/find
GREP = $(UTILDIR)/grep
CHMOD = $(UTILDIR)/chmod

ARCH_CMD = $(DEVDIR)/arch
VERS_STRING = $(DEVDIR)/vers_string
MERGEINFO = $(DEVDIR)/mergeInfo
COMPILEHELP = $(DEVDIR)/compileHelp
OFILE_LIST_TOOL = $(DEVDIR)/ofileListTool
FRAMEWORK_TOOL = $(DEVDIR)/frameworkFlags
NEWER = $(MAKEFILEDIR)/newer
DOTDOTIFY = $(MAKEFILEDIR)/dotdotify
CLONEHDRS = $(MAKEFILEDIR)/clonehdrs
MIG =
PSWRAP = $(DEVDIR)/pswrap
YACC = $(UTILDIR)/bison -y
LEX = $(UTILDIR)/flex
SED = $(UTILDIR)/sed
EGREP = $(UTILDIR)/egrep
AWK = $(UTILDIR)/gawk
TR = $(UTILDIR)/tr
CC = $(DEVDIR)/gcc
LD = $(CC)
AR =
NM = $(DEVDIR)/nm
RANLIB =
LIBTOOL = $(DEVDIR)/libtool
DUMP_SYMBOLS = $(DEVDIR)/link -dump -symbols

RC_CMD = $(DEVDIR)/rc.exe
REGGEN = $(DEVDIR)/regGen.exe

JAVATOOL = $(DEVDIR)/javatool.exe
BRIDGET = $(DEVDIR)/bridget.exe
BUILDFILTER = $(DEVDIR)/BuildFilter.exe
EOPREINDEX = $(NEXT_ROOT)$(SYSTEM_LIBRARY_EXECUTABLES_DIR)/eopreindex.exe

GENFORCELOAD = $(DEVDIR)/genforceload.exe
GENCLASSPATH = $(DEVDIR)/genclasspath.exe
PLISTREAD = $(DEVDIR)/plistread.exe
JAVA = $(JDKBINDIR)/java.exe
JAVAH = $(JDKBINDIR)/javah.exe
JAVAP = $(JDKBINDIR)/javap.exe
JAVADOC = $(JDKBINDIR)/javadoc.exe
RMIC = $(JDKBINDIR)/rmic.exe
RMIREGISTRY = $(JDKBINDIR)/rmiregistry.exe
SERIALVER = $(JDKBINDIR)/serialver.exe


JAR = $(JDKBINDIR)/jar.exe

# The following is not a command but it is platform-specific.

JAVA_PATH_SEPARATOR=;

MKZIP = $(DEVDIR)/zip.exe
