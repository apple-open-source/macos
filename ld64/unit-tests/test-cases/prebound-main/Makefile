
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

SHELL = bash # use bash shell so we can redirect just stderr

#
# Verify -prebind for 10.3 make ppc prebound and all others not prebound
#

ifeq (,${findstring 64,$(ARCH)})
	ifeq (${ARCH},i386)
		KEYWORD = NOUNDEFS
	else
		KEYWORD = PREBOUND
	endif
else
	KEYWORD = NOUNDEFS
endif


run: all

all: 
	# SnowLeopard is missing libmx.dylib which gcc thinks it needs
	${CC} ${CCFLAGS} main.c -o main -prebind -mmacosx-version-min=10.3 -nostdlib -lcrt1.o -lSystem
	otool -hv main | grep ${KEYWORD} | ${FAIL_IF_EMPTY}
	${PASS_IFF_GOOD_MACHO} main

clean:
	rm main 
