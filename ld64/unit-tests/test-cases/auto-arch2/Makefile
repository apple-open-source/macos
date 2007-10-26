##
# Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

SHELL = bash # use bash shell so we can redirect just stderr

#
# The point of this test is a check that ld
# can figure out which architecture is needed 
# by looking at the .o files when -arch is not used.
#

run: all


all:
	${CC} ${CCFLAGS} hello.c -c -o hello-${ARCH}.o -mmacosx-version-min=10.4
	${FAIL_IF_BAD_OBJ} hello-${ARCH}.o
	${FAIL_IF_ERROR} ${LD} -r -lcrt1.o hello-${ARCH}.o -o hello-r.o -lSystem
	${FAIL_IF_ERROR} ObjectDump hello-r.o >/dev/null
	file hello-r.o | grep ${ARCH} | ${PASS_IFF_STDIN}

clean:
	rm -f *.o
