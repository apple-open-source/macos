.PATH: ${.CURDIR}/arm/sys

MDSRCS+= \
	OSAtomic-v4.c	\
	OSAtomic.s	\
	_longjmp.s	\
	_setjmp.s	\
	arm_commpage_gettimeofday.c	\
	longjmp.s	\
	setjmp.s

MDCOPYFILES+= ${.CURDIR}/Platforms/${RC_TARGET_CONFIG}/arm/libc.syscall.arm
