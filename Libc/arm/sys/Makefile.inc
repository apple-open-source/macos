.PATH: ${.CURDIR}/arm/sys

MDSRCS+= \
	OSAtomic.s	\
	gcc_atomic.c	\
	_longjmp.s	\
	_setjmp.s	\
	arm_commpage_gettimeofday.c	\
	longjmp.s	\
	setjmp.s

.if !defined(FEATURE_ARM_ARCH_6)
MDSRCS+= OSAtomic-v4.c
.endif

MDCOPYFILES+= ${.CURDIR}/Platforms/${RC_TARGET_CONFIG}/arm/libc.syscall.arm
