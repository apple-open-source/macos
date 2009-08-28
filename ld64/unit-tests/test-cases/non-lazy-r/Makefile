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
# Check that non-lazy-pointers are properly handled by -r
#


all: all-${ARCH}

all-ppc: hasnl

all-ppc64: hasnl

all-i386: hasnl

all-armv6: hasnl

all-x86_64: all-true

all-true:
	${PASS_IFF} true


hasnl:
	${CC} ${CCFLAGS} -c foo.c -o foo.o
	${CC} ${CCFLAGS} -c other.c -o other.o
	${LD} -r -arch ${ARCH} foo.o other.o -o fooall.o -exported_symbol _foo
	# make sure there  are two indirect symbols: _foo and LOCAL
	otool -Iv fooall.o | grep "3 entries" | ${FAIL_IF_EMPTY}
	otool -Iv fooall.o | grep _foo   | ${FAIL_IF_EMPTY}
	otool -Iv fooall.o | grep _tent  | ${FAIL_IF_EMPTY}
	otool -Iv fooall.o | grep _other | ${FAIL_IF_STDIN}
	# make sure re-parsed correctly
	${OBJECTDUMP} fooall.o | grep name: | grep '_foo$$non_lazy_ptr' | ${FAIL_IF_EMPTY} 
	${OBJECTDUMP} fooall.o | grep name: | grep '_other$$non_lazy_ptr' | ${FAIL_IF_EMPTY}
	${OBJECTDUMP} fooall.o | grep name: | grep '_tent$$non_lazy_ptr' | ${FAIL_IF_EMPTY}
	${PASS_IFF} true
	
clean:
	rm -rf *.o
