
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

#
# Sanity check that ld can link a kext
#

ifeq (${ARCH},x86_64)
	FILE_TYPE = KEXTBUNDLE
else
	FILE_TYPE = OBJECT
endif


run: all

all:
	${CC} ${CCFLAGS} -static -fno-common -mkernel -c mykext.c -o mykext.o
	${CC} ${CCFLAGS} -static -fno-common -mkernel -c mykextinfo.c -o mykextinfo.o
	${CC} ${CCFLAGS} -Wl,-kext mykext.o mykextinfo.o -nostdlib -lkmodc++ -lkmod -lcc_kext -o mykext
	otool -hv mykext | grep ${FILE_TYPE} | ${FAIL_IF_EMPTY}
	nm -nm mykext | grep '(undefined) external _extern_global' | ${FAIL_IF_EMPTY}
	nm -nm mykext | grep '(__DATA,__data) external _my_global' | ${FAIL_IF_EMPTY}
	otool -rv mykext | grep '_extern_global' | ${FAIL_IF_EMPTY}
	${PASS_IFF} true

clean:
	rm -f mykext.o mykextinfo.o mykext
