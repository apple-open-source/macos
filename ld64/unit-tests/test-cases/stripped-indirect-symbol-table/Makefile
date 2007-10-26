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

ifeq "${ARCH}"  "i386"
	POINTER_SEGMENT = __IMPORT
	POINTER_SECTION  = __pointers
else
	POINTER_SEGMENT = __DATA
	POINTER_SECTION  = __nl_symbol_ptr
endif


#
# Test that using strip -R to selectively strip symbol names
# of of a .o file still works with ld.
#

run: all

all:
	${CC} ${CCFLAGS} a.c -c -o a.o
	${CC} ${CCFLAGS} b.c -c -o b.o
	${CC} ${CCFLAGS} c.c -c -o c.o
	${CC} ${CCFLAGS} func.c -c -o func.o
	${LD} -arch ${ARCH} -r a.o b.o c.o -o most.o
	strip -x -R strip.list most.o -o most.stripped.o
	${CC} ${CCFLAGS} most.stripped.o func.o -dynamiclib -o dylib1
	${LD} -arch ${ARCH} -r most.stripped.o func.o -o all.o
	${CC} ${CCFLAGS} all.o  -dynamiclib -o dylib2	
	otool -X -s ${POINTER_SEGMENT} ${POINTER_SECTION} dylib1 >dylib1.pointers
	otool -X -s ${POINTER_SEGMENT} ${POINTER_SECTION} dylib2 >dylib2.pointers
	${PASS_IFF} diff dylib1.pointers dylib2.pointers

clean:
	rm -rf *.o dylib1 dylib2 *.pointers
