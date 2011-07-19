.PATH: ${.CURDIR}/x86_64/sys ${.CURDIR}/i386/sys

AINC+= -I${.CURDIR}/x86_64/sys

MDSRCS+= OSAtomic.s \
	atomic.c \
	spinlocks.c \
	spinlocks_asm.s \
	i386_gettimeofday_asm.s \
	_setjmp.s \
	setjmp.s \
	_sigtramp.s \
	nanotime.s

DYLDSRCS += \
	OSAtomic.s \
	i386_gettimeofday_asm.s \
	spinlocks_asm.s \
	nanotime.s
