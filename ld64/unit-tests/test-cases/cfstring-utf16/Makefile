##
# Copyright (c) 2008 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

#
# Test that utf16 cfstring literals are not coalesced.
# There is 3 CFSTR in foo.m and 1 CFSTR in bar.m
# After coalescing and dead stripping there should be only one CFSTR in the output
#

ifeq (,${findstring 64,$(ARCH)})
	CFSTRING_SIZE = 48
else
	CFSTRING_SIZE = 96
endif
	USTRING_SIZE = 37



run: all

all:
	${CC} ${CCFLAGS} foo.m bar.m -o foo -framework CoreFoundation -dead_strip
	size -l foo | grep "__cfstring: ${CFSTRING_SIZE}" | ${FAIL_IF_EMPTY}
	size -l foo | grep "__ustring: ${USTRING_SIZE}" | ${FAIL_IF_EMPTY}
	${PASS_IFF_GOOD_MACHO} foo

clean:
	rm -rf foo 
