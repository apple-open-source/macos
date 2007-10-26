##
# Copyright (c) 2007 Apple Inc. All rights reserved.
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
# The point of this test is validate cpu subtypes processing
#

test: test-${ARCH}

test-ppc64:
	${PASS_IFF} true

test-i386:
	${PASS_IFF} true

test-x86_64:
	${PASS_IFF} true

test-ppc:
	gcc foo.c -arch ppc     -mmacosx-version-min=10.4 -c -o foo.o
	${FAIL_IF_BAD_OBJ} foo.o
	gcc foo.c -arch ppc750  -mmacosx-version-min=10.4 -c -o foo-G3.o
	${FAIL_IF_BAD_OBJ} foo-G3.o
	gcc foo.c -arch ppc7400 -mmacosx-version-min=10.4 -c -o foo-G4.o
	${FAIL_IF_BAD_OBJ} foo-G4.o
	gcc foo.c -arch ppc970  -mmacosx-version-min=10.4 -c -o foo-G5.o
	${FAIL_IF_BAD_OBJ} foo-G5.o
	gcc main.c -arch ppc    -mmacosx-version-min=10.4 -c -o main.o
	${FAIL_IF_BAD_OBJ} main.o
	gcc main.c -arch ppc970 -mmacosx-version-min=10.4 -c -o main-G5.o
	${FAIL_IF_BAD_OBJ} main-G5.o

	# check ALL+ALL -> ALL
	${LD} -r main.o foo.o -o main-r.o
	${FAIL_IF_BAD_OBJ} main-r.o
	otool -hv main-r.o | grep ALL | ${FAIL_IF_EMPTY}

	# check G3+ALL -> G3
	${LD} -r main.o foo-G3.o -o main-r.o
	${FAIL_IF_BAD_OBJ} main-r.o
	otool -hv main-r.o | grep ppc750 | ${FAIL_IF_EMPTY}

	# check G4+ALL -> G4
	${LD} -r main.o foo-G4.o -o main-r.o
	${FAIL_IF_BAD_OBJ} main-r.o
	otool -hv main-r.o | grep ppc7400 | ${FAIL_IF_EMPTY}

	# check G5+ALL -> G5
	${LD} -r main.o foo-G5.o -o main-r.o
	${FAIL_IF_BAD_OBJ} main-r.o
	otool -hv main-r.o | grep ppc970 | ${FAIL_IF_EMPTY}

	# check G5+G4 -> G5
	${LD} -r main-G5.o foo-G4.o -o main-r.o
	${FAIL_IF_BAD_OBJ} main-r.o
	otool -hv main-r.o | grep ppc970 | ${FAIL_IF_EMPTY}

	# check G4+G5 -> G5
	${LD} -r foo-G4.o main-G5.o -o main-r.o
	${FAIL_IF_BAD_OBJ} main-r.o
	otool -hv main-r.o | grep ppc970 | ${FAIL_IF_EMPTY}

	# check -force_cpusubtype_ALL
	${LD} -r main.o foo-G5.o -o main-r.o -force_cpusubtype_ALL
	${FAIL_IF_BAD_OBJ} main-r.o
	otool -hv main-r.o | grep ALL | ${PASS_IFF_STDIN}

clean:
	rm -f *.o
