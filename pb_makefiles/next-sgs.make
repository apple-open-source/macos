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
# next-sgs.make 
#
# Hacks to support use of NeXT's internal SGS tool set.
#
# If you use this, you're pretty much on your own.  If you must, however:
# 1) Put your source code in a directory named $(NAME).%d[.%d][.%d][%s]
# 2) Uncomment the preamble boilerplate:
#      OTHER_GENERATED_OFILES = $(VERS_OFILE)
# 3) Build your source "from clean" if you expect VERS_OFILE to be correct.
#

VERS_FILE = $(NAME)_vers.c
VERS_OFILE = $(NAME)_vers.o

$(VERS_OFILE): $(VERS_FILE)

$(VERS_FILE): 
	(cname=`$(ECHO) $(NAME) | $(SED) 's/[^0-9A-Za-z]/_/g'`; \
        $(VERS_STRING) -c $(NAME) \
                | $(SED) s/SGS_VERS/$${cname}_VERS_STRING/ \
                | $(SED) s/VERS_NUM/$${cname}_VERS_NUM/ > $(SFILE_DIR)/$(VERS_FILE))


# In an SGS world, generation of a version string for a project depends upon
# the project living in a directory with a name of the correct format.
CURRENT_PROJECT_VERSION = $(shell echo $(SRCROOT) | sed -n 's%^.*-\([0-9][0-9]*\.*[0-9]*\.*[0-9]*\).*$$%\1%p' | sed 's%\.$$%%')


