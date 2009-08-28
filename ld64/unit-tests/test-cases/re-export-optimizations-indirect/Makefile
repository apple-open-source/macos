##
# Copyright (c) 2009 Apple Inc. All rights reserved.
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
# Test that a public re-exported library is automatically added as a dependent
# unless nothing is used from it.
#


run: all

all:
	
# -sub_library for 10.4
	${CC} ${CCFLAGS} -dynamiclib bar.c -o libbar.dylib  -install_name /usr/lib/libbar.dylib -mmacosx-version-min=10.4
	${FAIL_IF_BAD_MACHO} libbar.dylib
	${CC} ${CCFLAGS} -dynamiclib middle.c -o libmiddle.dylib -lbar -L. -sub_library libbar -install_name /mid/libmiddle.dylib -mmacosx-version-min=10.4
	${FAIL_IF_BAD_MACHO} libmiddle.dylib
	${CC} ${CCFLAGS} -dynamiclib foo.c -o libfoo.dylib -lmiddle -L. -sub_library libmiddle -install_name /usr/lib/libfoo.dylib -mmacosx-version-min=10.4
	${FAIL_IF_BAD_MACHO} libfoo.dylib
	${CC} ${CCFLAGS} -dynamiclib other.c -o libother.dylib -lbar -L. -sub_library libbar -install_name /usr/lib/libother.dylib -mmacosx-version-min=10.4
	${FAIL_IF_BAD_MACHO} libother.dylib
	${CC} ${CCFLAGS} main.c -DCALL_BAR libfoo.dylib libother.dylib -o main -L. -mmacosx-version-min=10.4
	nm -m main | grep _bar | grep libbar | ${FAIL_IF_EMPTY}
	${PASS_IFF_GOOD_MACHO} main
		
# -sub_library for 10.5
	${CC} ${CCFLAGS} -dynamiclib bar.c -o libbar.dylib  -install_name /usr/lib/libbar.dylib -mmacosx-version-min=10.5
	${FAIL_IF_BAD_MACHO} libbar.dylib
	${CC} ${CCFLAGS} -dynamiclib middle.c -o libmiddle.dylib -lbar -L. -sub_library libbar -install_name /mid/libmiddle.dylib -mmacosx-version-min=10.5
	${FAIL_IF_BAD_MACHO} libmiddle.dylib
	${CC} ${CCFLAGS} -dynamiclib foo.c -o libfoo.dylib -lmiddle -L. -sub_library libmiddle -install_name /usr/lib/libfoo.dylib -mmacosx-version-min=10.5
	${FAIL_IF_BAD_MACHO} libfoo.dylib
	${CC} ${CCFLAGS} -dynamiclib other.c -o libother.dylib -lbar -L. -sub_library libbar -install_name /usr/lib/libother.dylib -mmacosx-version-min=10.5
	${FAIL_IF_BAD_MACHO} libother.dylib
	${CC} ${CCFLAGS} main.c -DCALL_BAR libfoo.dylib libother.dylib -o main -L. -mmacosx-version-min=10.5
	nm -m main | grep _bar | grep libbar | ${FAIL_IF_EMPTY}
	${PASS_IFF_GOOD_MACHO} main
	

clean:
	rm -rf libbar.dylib libfoo.dylib libmiddle.dylib libother.dylib main 
