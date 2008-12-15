.PATH: ${.CURDIR}/x86_64/sys ${.CURDIR}/i386/sys

AINC+= -I${.CURDIR}/x86_64/sys

MDSRCS+= OSAtomic.s \
	i386_gettimeofday.s \
	_setjmp.s \
	setjmp.s \
	_sigtramp.s

MDCOPYFILES+= ${.CURDIR}/Platforms/${RC_TARGET_CONFIG}/x86_64/libc.syscall.x86_64
