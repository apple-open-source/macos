#!/bin/sh

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
# SearchForOldDirs
#
# Searches all files (other than those ending in ~) in a directory
# hierarchy for references to the old Rhapsody DR1 / OpenStep system 
# directory layout.
#
# Copyright (c) 1998, Apple Computer, Inc.
#  All rights reserved.
#   


# Assume that the user wants only the filenames that contain the target
# strings, unless the -v flag is given.

ARG="-l"

if [ "x$1" = "x-v" ]; then
	ARG=""
	shift
fi

find . -type f \! -name "*~" | \
    xargs grep $ARG -e "LocalLibrary\|NextDeveloper\|LocalDeveloper\|NextApps\|LocalApps\|NextAdmin\|LocalAdmin\|NextPrinter\|NextStep\|NextCD\|NextLibrary"

exit 0
