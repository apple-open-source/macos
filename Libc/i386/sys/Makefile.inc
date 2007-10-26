.PATH: ${.CURDIR}/i386/sys

AINC+= -I${.CURDIR}/i386/sys

MDSRCS+= OSAtomic.s \
	i386_gettimeofday.s \
	_setjmp.s \
	setjmp.s \
	_sigtramp.s

MDCOPYFILES+= ${.CURDIR}/i386/sys/libc.syscall.i386
