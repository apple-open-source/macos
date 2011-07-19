.PATH: ${.CURDIR}/i386/sys

AINC+= -I${.CURDIR}/i386/sys

MDSRCS+= OSAtomic.s \
	atomic.c \
	i386_gettimeofday_asm.s \
	mach_absolute_time.c \
	mach_absolute_time_asm.s \
	_setjmp.s \
	setjmp.s \
	_sigtramp.s \
	spinlocks_asm.s \
	spinlocks.s

DYLDSRCS += \
	OSAtomic.s \
	i386_gettimeofday_asm.s \
	mach_absolute_time_asm.s \
	spinlocks_asm.s
