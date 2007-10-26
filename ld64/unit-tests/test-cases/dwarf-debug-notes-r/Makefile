##
# Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
# The point of this test is a sanity check that ld
# produces good "debug notes" stabs from dwarf .o files after
# some of the .o files are merged with ld -r.
# Running nm through stabs-filter.pl produces connonical stabs
# that can be diffed against a checked in know good set of stabs
#

run: all

all: foobar.o main.o 
	${CXX} ${CCXXFLAGS} foobar.o main.o -o dwarf-test-${ARCH}
	${FAIL_IF_BAD_MACHO} dwarf-test-${ARCH}
	nm -ap dwarf-test-${ARCH} | ./stabs-filter.pl > dwarf-test-${ARCH}.stabs
	${PASS_IFF} diff dwarf-test-${ARCH}.stabs expected-stabs

foobar.o : foo.o bar.o
	${LD} -r -arch ${ARCH} foo.o bar.o -o foobar.o
	${FAIL_IF_BAD_OBJ} foobar.o

foo.o : foo.cxx
	${CXX} ${CCXXFLAGS} -gdwarf-2 foo.cxx -c -o $@ -mdynamic-no-pic
	${FAIL_IF_BAD_OBJ} $@

bar.o : bar.cxx
	${CXX} ${CCXXFLAGS} -gdwarf-2 bar.cxx -c -o $@  -mdynamic-no-pic
	${FAIL_IF_BAD_OBJ} $@

main.o : main.cxx
	${CXX} ${CCXXFLAGS} -gdwarf-2 main.cxx -c -o $@ -mdynamic-no-pic
	${FAIL_IF_BAD_OBJ} $@

clean:
	rm -rf dwarf-test-*  *.o *.stabs
