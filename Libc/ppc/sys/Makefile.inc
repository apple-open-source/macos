.PATH: ${.CURDIR}/ppc/sys

MDSRCS+= OSAtomic.s \
	ldbl64.s \
	_longjmp.s \
	longjmp.s \
	ppc_gettimeofday.s \
	_setjmp.s \
	setjmp.s \
	_sigtramp.s

MDCOPYFILES+= ${.CURDIR}/Platforms/${RC_TARGET_CONFIG}/ppc/libc.syscall.ppc
