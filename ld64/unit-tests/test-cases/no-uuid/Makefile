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

#
# Test the we set emit LC_UUID correctly
#

run: all

all:

# Test main executable built with dwarf has uuid
	${CC} ${CCFLAGS} foo.c -o foo -gdwarf-2
	${FAIL_IF_BAD_MACHO} foo
	${OTOOL} -hlv foo | grep LC_UUID | ${FAIL_IF_EMPTY}
	rm -f foo

# Test main executable built with stabs has uuid
	${CC} ${CCFLAGS} foo.c -o foo -gfull -gstabs+
	${FAIL_IF_BAD_MACHO} foo
	${OTOOL} -hlv foo | grep LC_UUID | ${FAIL_IF_EMPTY}

# Test main executable built with dwarf and -no_uuid does not have uuid
	${CC} ${CCFLAGS} foo.c -o foo -Wl,-no_uuid -gdwarf-2
	${FAIL_IF_BAD_MACHO} foo
	${OTOOL} -hlv foo | grep LC_UUID | ${FAIL_IF_STDIN}

# Test ld -r of stabs file has no uuid
	${CC} ${CCFLAGS} foo.c -c -o foo.o -gfull -gstabs+
	${LD} -arch ${ARCH} foo.o -r -o foo2.o
	${OTOOL} -hlv foo2.o | grep LC_UUID | ${FAIL_IF_STDIN}

# Test ld -r of two files one with uuid produces a uuid
	${CC} ${CCFLAGS} foo.c -c -o foo.o -gdwarf-2
	${LD} -arch ${ARCH} foo.o -r -o foo2.o
	${CC} ${CCFLAGS} bar.c -c -gstabs+ -o bar.o
	${LD} -arch ${ARCH} -r foo.o bar.o -o foobar.o
	${OTOOL} -hlv foobar.o | grep LC_UUID | ${PASS_IFF_STDIN}

clean:
	rm -rf foo foo.o foo2.o bar.o foobar.o foo.dSYM
