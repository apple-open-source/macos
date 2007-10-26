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

# Test with dwarf-2
	${CC} ${CCFLAGS} foo.c -o foo-${ARCH} -gdwarf-2
	${FAIL_IF_BAD_MACHO} foo-${ARCH}
	${OTOOL} -hlv foo-${ARCH} | grep LC_UUID | ${FAIL_IF_EMPTY}
	rm -f foo-${ARCH}

# Test with stabs
	${CC} ${CCFLAGS} foo.c -o foo-${ARCH} -gfull -gstabs+
	${FAIL_IF_BAD_MACHO} foo-${ARCH}
	${OTOOL} -hlv foo-${ARCH} | grep LC_UUID | ${FAIL_IF_STDIN}

# Test with two files one with UUID
	${CC} ${CCFLAGS} bar.c -c -gdwarf-2
	${FAIL_IF_BAD_OBJ} bar.o

	${LD} -arch ${ARCH} bar.o -r -o bar-${ARCH}.o -no_uuid
	${FAIL_IF_BAD_OBJ} bar-${ARCH}.o

	${CC} ${CCFLAGS} foo.c -c -gdwarf-2
	${FAIL_IF_BAD_OBJ} foo.o

	${LD} -arch ${ARCH} foo.o -r -o foo-${ARCH}.o
	${FAIL_IF_BAD_OBJ} foo-${ARCH}.o

	${CC} ${CCFLAGS} foo-${ARCH}.o bar-${ARCH}.o -o temp-${ARCH}
	${FAIL_IF_BAD_MACHO} temp-${ARCH}

	${OTOOL} -hlv temp-${ARCH} | grep LC_UUID | ${FAIL_IF_EMPTY}
	rm -f foo.o bar.o foo-${ARCH}.o bar-${ARCH}.o temp-${ARCH}

# Test with with the flag disabling uuid
	${CC} ${CCFLAGS} foo.c -o foo-${ARCH} -Wl,-no_uuid -gdwarf-2
	${FAIL_IF_BAD_MACHO} foo-${ARCH}
	${OTOOL} -hlv foo-${ARCH} | grep LC_UUID | ${PASS_IFF_EMPTY}

clean:
	rm -rf foo-*
