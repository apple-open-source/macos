.PATH: ${.CURDIR}/i386/sys

AINC+= -I${.CURDIR}/i386/sys

MDSRCS+= OSAtomic.s \
	i386_gettimeofday.s \
	_setjmp.s \
	setjmp.s \
	_sigtramp.s

MDCOPYFILES+= ${.CURDIR}/Platforms/${RC_TARGET_CONFIG}/i386/libc.syscall.i386
