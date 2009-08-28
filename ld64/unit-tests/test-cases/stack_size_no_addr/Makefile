##
# Copyright (c) 2005-2008 Apple Inc. All rights reserved.
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

# Test the ld option -stack_size adds a custom stack segment

ifeq ($(ARCH),armv6)
	STACK_ADDR = 0x30000000
	STACK_SIZE = 0x05000000
	STACK_TOP  = 0x2b000000
else
ifeq (,${findstring 64,$(ARCH)})
	STACK_ADDR = 0xC0000000
	STACK_SIZE = 0x05000000
	STACK_TOP  = 0xbb000000
else
	STACK_ADDR = 0x0007fff5fc000000
	STACK_TOP  = 0x00007fff57000000
	STACK_SIZE = 0x0000000005000000
endif
endif


run: all

all:
	${CC} ${CCFLAGS} main.c -o main -Wl,-stack_size,${STACK_SIZE}
	# Check for __UNIXSTACK section in object, check that it has the correct value
	otool -l main | grep -A6 __UNIXSTACK > main.otool
	grep " vmsize[ ]*${STACK_SIZE}" main.otool | ${FAIL_IF_EMPTY}
	grep " vmaddr[ ]*${STACK_TOP}" main.otool | ${FAIL_IF_EMPTY}
	${PASS_IFF_GOOD_MACHO} main

clean:
	rm -rf main main.otool
