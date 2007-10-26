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
# produces good "debug notes" stabs from dwarf .o files
# Running nm through stabs-filter.pl produces connonical stabs
# that can be diffed against a checked in know good set of stabs
#

run: all

all: hello.o other.o 
	${CXX} ${CCXXFLAGS} -gdwarf-2 hello.o other.o -o dwarf-hello-${ARCH}
	${FAIL_IF_BAD_MACHO} dwarf-hello-${ARCH}
	nm -ap dwarf-hello-${ARCH} | ./stabs-filter.pl > dwarf-hello-${ARCH}.stabs
	${PASS_IFF} diff dwarf-hello-${ARCH}.stabs expected-stabs

hello.o : hello.cxx
	${CXX} ${CCXXFLAGS} -gdwarf-2 hello.cxx -c -o $@ -mdynamic-no-pic
	${FAIL_IF_BAD_OBJ} $@

other.o : other.cxx
	${CXX} ${CCXXFLAGS} -gdwarf-2 other.cxx -c -o $@ -mdynamic-no-pic
	${FAIL_IF_BAD_OBJ} $@

clean:
	rm -rf dwarf-hello-* *.o *.stabs
