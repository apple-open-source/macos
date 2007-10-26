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

SHELL = bash # use bash shell so we can redirect just stderr

#
# Validate that the linker catches illegal combinations of .o files 
# compiled with different GC settings. 
#

test:
	${CC} ${CCFLAGS} foo.m -c -o foo.o
	${FAIL_IF_BAD_OBJ} foo.o

	${CC} ${CCFLAGS} foo.m -c -o foo-gc.o -fobjc-gc
	${FAIL_IF_BAD_OBJ} foo-gc.o

	${CC} ${CCFLAGS} foo.m -c -o foo-gc-only.o -fobjc-gc-only
	${FAIL_IF_BAD_OBJ} foo-gc-only.o

	${CC} ${CCFLAGS} bar.m -c -o bar.o
	${FAIL_IF_BAD_OBJ} bar.o

	${CC} ${CCFLAGS} bar.m -c -o bar-gc.o -fobjc-gc
	${FAIL_IF_BAD_OBJ} bar-gc.o

	${CC} ${CCFLAGS} bar.m -c -o bar-gc-only.o -fobjc-gc-only
	${FAIL_IF_BAD_OBJ} bar-gc-only.o

	# check RR + RR -> RR
	${CC} foo.o bar.o -dynamiclib -o libfoobar.dylib -framework Foundation
	${FAIL_IF_BAD_MACHO} libfoobar.dylib

	# check GC/RR + GC/RR -> GC/RR
	${CC} foo-gc.o bar-gc.o -dynamiclib -o libfoobar.dylib -framework Foundation
	${FAIL_IF_BAD_MACHO} libfoobar.dylib

	# check GC + GC -> GC
	${CC} foo-gc-only.o bar-gc-only.o -dynamiclib -o libfoobar.dylib -framework Foundation
	${FAIL_IF_BAD_MACHO} libfoobar.dylib

	# check RR + GC/RR -> error
	${FAIL_IF_SUCCESS} ${CC} foo.o bar-gc.o -dynamiclib -o libfoobar.dylib -framework Foundation 2> fail.log

	# check RR + GC -> error
	${FAIL_IF_SUCCESS} ${CC} foo.o bar-gc-only.o -dynamiclib -o libfoobar.dylib -framework Foundation 2> fail.log

	# check GC + GC/RR -> error
	${FAIL_IF_SUCCESS} ${CC} foo-gc-only.o bar-gc.o -dynamiclib -o libfoobar.dylib -framework Foundation 2> fail.log
	${PASS_IFF} true

clean:
	rm -rf foo*.o bar*.o libfoobar.dylib fail.log
