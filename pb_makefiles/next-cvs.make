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
# next-cvs.make 
#
# Hacks to support use of NeXT's particular use of CVS.
#
# To use this:
# 1) Uncomment the preamble boilerplate:
#      OTHER_GENERATED_OFILES = $(VERS_OFILE)
# 2) You may want to change CVS_VERS_FILE   If the CVSVersionInfo.txt file maintained by 
#    "checkpoint" is not in the current directory (e.g. your project is an aggregate)
#    you must change CVS_VERS_FILE; eg.
#	CVS_VERS_FILE = ../CVSVersionInfo.txt
#    In many projects, the default below will suffice.
#
ifndef CVS_VERS_FILE
CVS_VERS_FILE = CVSVersionInfo.txt
endif

CVS_PROJECT_VERS_CMD = $(VERSIONING_SYSTEM_MAKEFILEDIR)/next-cvs_project_version.sh

VERS_FILE = $(NAME)_vers.c
VERS_OFILE = $(NAME)_vers.o
CVS_PROJECT_VERS_CMD_ARGS = -V $(CVS_VERS_FILE)

$(VERS_OFILE): $(VERS_FILE)

$(VERS_FILE): $(CVS_VERS_FILE)
	$(CVS_PROJECT_VERS_CMD) $(CVS_PROJECT_VERS_CMD_ARGS) -c $(NAME) > $(SFILE_DIR)/$(VERS_FILE)

CURRENT_PROJECT_VERSION = $(shell $(CVS_PROJECT_VERS_CMD) $(CVS_PROJECT_VERS_CMD_ARGS) -n)

# By default, this looks for the version info in
# CVSVersionInfo.txt in the current source directory.  If this is not 
# case, override $(CVS_VERS_FILE)



