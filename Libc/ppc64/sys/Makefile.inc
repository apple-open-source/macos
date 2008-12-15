# searching ppc directory as a fallback to avoid unnecessary code duplication
.PATH: ${.CURDIR}/ppc/sys ${.CURDIR}/ppc64/sys

MDSRCS+= OSAtomic.s \
	_longjmp.s \
	longjmp.s \
	ppc_gettimeofday.s \
	_setjmp.s \
	setjmp.s \
	_sigtramp.s

MDCOPYFILES+= ${.CURDIR}/Platforms/${RC_TARGET_CONFIG}/ppc64/libc.syscall.ppc64
