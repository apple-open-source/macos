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
LLVMGCC = /usr/local/bin/llvm-gcc
LLVMGXX = /usr/local/bin/llvm-g++
#
# Test the we set the stack execution bit properly.

run:
	@if [ -f /usr/local/bin/llvm-gcc ] ; then \
	  $(MAKE) all ; \
	else \
	  ${PASS_IFF} /usr/bin/true ; \
	fi

all: zero one two three four five six seven eight nine ten eleven twelve thirteen


zero:
	#
	# llvm  : a.c : Dfoo3
	# llvm  : b.c : Dfoo2
	# MachO : main.c : Ufoo2, Ufoo3
	#
	#echo "Zero..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a.c -c -o a-${ARCH}.o  ${OTHER}
	${LLVMGCC} ${CCFLAGS} --emit-llvm b.c -c -o b-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main.c -c -o main-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a-${ARCH}.o b-${ARCH}.o main-${ARCH}.o -o main-${ARCH}
	${PASS_IFF_GOOD_MACHO} main-${ARCH}
	${PASS_IFF} ./main-${ARCH}

one:
	#
	# llvm  : a1.c : Dfoo3, Ufoo4
	# llvm  : b1.c : Dfoo2, Ufoo4
	# MachO : main1.c : Dfoo4, Ufoo2, Ufoo3
	#
	#echo "One..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a1.c -c -o a1-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} --emit-llvm b1.c -c -o b1-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main1.c -c -o main1-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a1-${ARCH}.o b1-${ARCH}.o main1-${ARCH}.o -o main1-${ARCH}
	${PASS_IFF_GOOD_MACHO} main1-${ARCH}
	${PASS_IFF} ./main1-${ARCH}

two:
	#
	# llvm  : a2.c : Dfoo3, Ufoo4
	# llvm  : b2.c : Dfoo2, Dfoo4
	# MachO : main2.c : Ufoo2, Ufoo3
	#
	#echo "Two..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a2.c -c -o a2-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} --emit-llvm b2.c -c -o b2-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main2.c -c -o main2-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a2-${ARCH}.o b2-${ARCH}.o main2-${ARCH}.o -o main2-${ARCH}
	${PASS_IFF_GOOD_MACHO} main2-${ARCH}
	${PASS_IFF} ./main2-${ARCH}

three:
	#
	# llvm  : a3.c : Dfoo1, Dbar
	# llvm  : b3.c : Dfoo2, Ubar
	# MachO : main3.c : Ufoo1, Ufoo2, Ubar
	#
	#echo "Three..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a3.c -c -o a3-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} --emit-llvm b3.c -c -o b3-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main3.c -c -o main3-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a3-${ARCH}.o b3-${ARCH}.o main3-${ARCH}.o -o main3-${ARCH}
	${PASS_IFF_GOOD_MACHO} main3-${ARCH}
	${PASS_IFF} ./main3-${ARCH}

four:
	#
	# llvm  : a4.c : Dfoo3, Ufoo4
	# llvm  : b4.c : Dfoo2, DLmyfoo, Ufoo4
	# MachO : main4.c : Dfoo4, Ufoo2, Ufoo3
	#
	#echo "Four..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a4.c -c -o a4-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} --emit-llvm b4.c -c -o b4-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main4.c -c -o main4-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a4-${ARCH}.o b4-${ARCH}.o main4-${ARCH}.o -o main4-${ARCH}
	${PASS_IFF_GOOD_MACHO} main4-${ARCH}
	${PASS_IFF} ./main4-${ARCH}

five:
	#
	# llvm  : a5.c : Dfoo1, Ufoo2, Ufoo3
	# llvm  : b5.c : Dfoo2
	# MachO : main5.c : Dfoo3, Ufoo1
	#
	#echo "Five..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a5.c -c -o a5-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} --emit-llvm b5.c -c -o b5-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main5.c -c -o main5-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a5-${ARCH}.o b5-${ARCH}.o main5-${ARCH}.o -o main5-${ARCH}  -Wl,-dead_strip
	${PASS_IFF_GOOD_MACHO} main5-${ARCH}
	${PASS_IFF} ./main5-${ARCH}
	${OTOOL} -tV main5-${ARCH} | grep foo3 | ${PASS_IFF_EMPTY}

six:
	#
	# llvm  : a6.c : Dfoo1, Dfoo2
	# MachO : main6.c : Ufoo1
	#
	#echo "Six..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a6.c -c -o a6-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main6.c -c -o main6-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a6-${ARCH}.o  main6-${ARCH}.o -o main6-${ARCH}  -Wl,-dead_strip
	${PASS_IFF_GOOD_MACHO} main6-${ARCH}
	${PASS_IFF} ./main6-${ARCH}
	${OTOOL} -tV main6-${ARCH} | grep foo2 | ${PASS_IFF_EMPTY}

seven:
	#
	# llvm  : a7.c : Dfoo1, Dfoo2, Ufoo3
	# llvm  : b7.c : Dfoo3, ufoo2
	# MachO : main7.c : Ufoo1
	#
	#echo "Seven..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a7.c -c -o a7-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} --emit-llvm b7.c -c -o b7-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main7.c -c -o main7-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a7-${ARCH}.o b7-${ARCH}.o  main7-${ARCH}.o -o main7-${ARCH}
	${PASS_IFF_GOOD_MACHO} main7-${ARCH}
	${PASS_IFF} ./main7-${ARCH}

eight:
	#
	# llvm  : a8.c : Dfoo1, Dfoo2
	# MachO : main8.c : Ufoo1
	#
	#echo "Eight..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a8.c -c -o a8-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main8.c -c -o main8-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a8-${ARCH}.o main8-${ARCH}.o -o main8-${ARCH}   -Wl,-dead_strip
	${PASS_IFF} ./main8-${ARCH}
	${OTOOL} -tV main8-${ARCH} | grep foo2 | ${PASS_IFF_EMPTY}
	${OTOOL} -tV main8-${ARCH} | grep unnamed_2_1 | ${PASS_IFF_EMPTY}

nine:
	#
	# llvm  : a9.c : Dfoo1, Dfoo2, Dfoo3, Ufoo3, Ufoo4
	# MachO : main9.c : Ufoo1, Dfoo4
	#
	#echo "Nine..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a9.c -c -o a9-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main9.c -c -o main9-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a9-${ARCH}.o main9-${ARCH}.o -o main9-${ARCH}   -Wl,-dead_strip
	${PASS_IFF} ./main9-${ARCH}
	${OTOOL} -tV main9-${ARCH} | grep foo2 | ${PASS_IFF_EMPTY}
	${OTOOL} -tV main9-${ARCH} | grep foo4 | ${PASS_IFF_EMPTY}
	${OTOOL} -tV main9-${ARCH} | grep unnamed_2_1 | ${PASS_IFF_EMPTY}

ten:
	#
	# llvm  : a10.c
	# llvm  : b10.c
	# MachO : main10.c
	#
	#echo "Ten..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a10.c -c -o a10-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} --emit-llvm b10.c -c -o b10-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main10.c -c -o main10-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a10-${ARCH}.o b10-${ARCH}.o main10-${ARCH}.o -o main10-${ARCH}
	${PASS_IFF_GOOD_MACHO} main10-${ARCH}

eleven:
	#
	# llvm  : a11.c
	# MachO : main11.c
	#
	#echo "Eleven..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a11.c -c -o a11-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main11.c -c -o main11-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a11-${ARCH}.o main11-${ARCH}.o -o main11-${ARCH}
	${PASS_IFF_GOOD_MACHO} main11-${ARCH}
	./main11-${ARCH} >& /dev/null

twelve:
	#
	# llvm  : a12.c
	# MachO : main12.c
	#
	#echo "Tweleve..."
	${LLVMGCC} ${CCFLAGS} --emit-llvm a12.c -c -o a12-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} main12.c -c -o main12-${ARCH}.o
	${LLVMGCC} ${CCFLAGS} a12-${ARCH}.o main12-${ARCH}.o -o main12-${ARCH}
	${PASS_IFF_GOOD_MACHO} main12-${ARCH}
	./main12-${ARCH}

thirteen:
	#
	# llvm : a13.cc
	# MachO : main13.cc
	#
	# echo "Thirteen..."
	${LLVMGCC} ${CXXFLAGS} --emit-llvm a13.cc -c -o a13-${ARCH}.o
	${LLVMGCC} ${CXXFLAGS} main13.cc -c -o main13-${ARCH}.o
	${LLVMGXX} a13-${ARCH}.o main13-${ARCH}.o -o main13-${ARCH}
	${PASS_IFF_GOOD_MACHO} main13-${ARCH}

	
clean:
	rm -rf *.o main*-* big.*
