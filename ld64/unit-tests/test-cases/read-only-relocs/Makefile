##
# Copyright (c) 2006-2007 Apple Inc. All rights reserved.
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
# Test that ld can linke a dylib built with -mdynamic-no-pic
#


SHELL = bash # use bash shell so we can redirect just stderr

NO_PIC =  
STATIC =  

ifeq (${ARCH},i386)
	NO_PIC =  -mdynamic-no-pic
	STATIC =  -static
else 
	ifeq (${ARCH},ppc)
		NO_PIC =  -mdynamic-no-pic
		STATIC =  -mdynamic-no-pic
	endif
endif



all:
	# build libfoo.dylib as regular dylib
	${CC} ${CCFLAGS} foo.c -dynamiclib -o libfoo.dylib
	# build libtest.dylib using -mdynamic-no-pic and -read_only_relocs suppress
	${CC} ${CCFLAGS} test.c -c ${NO_PIC}
	${CC} ${CCFLAGS} test.o libfoo.dylib -dynamiclib -o libtest.dylib -read_only_relocs suppress -Wl,-w
	# build libtest.dylib using -static and -read_only_relocs suppress
	${CC} ${CCFLAGS} test.c -c ${STATIC}
	${CC} ${CCFLAGS} test.o libfoo.dylib -dynamiclib -o libtest.dylib -read_only_relocs suppress -Wl,-w
	# build main using -static and -read_only_relocs suppress
	${CC} ${CCFLAGS} test.c -c ${STATIC}
	${CC} ${CCFLAGS} test.o libfoo.dylib -o foo -read_only_relocs suppress -Wl,-w
	${PASS_IFF_GOOD_MACHO} foo

clean:
	rm -rf test.o libfoo.dylib libtest.dylib foo
