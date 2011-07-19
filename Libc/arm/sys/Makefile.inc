.PATH: ${.CURDIR}/arm/sys

MDSRCS+= \
	OSAtomic.s	\
	OSAtomic_resolvers.c	\
	gcc_atomic.c	\
	_longjmp.s	\
	_setjmp.s	\
	arm_commpage_gettimeofday.c	\
	longjmp.s	\
	setjmp.s \
	mach_absolute_time.s

DYLDSRCS += OSAtomic.s arm_commpage_gettimeofday.c mach_absolute_time.s

.if !defined(FEATURE_ARM_ARCH_6)
MDSRCS+= OSAtomic-v4.c
DYLDSRCS+= OSAtomic-v4.c
.endif
