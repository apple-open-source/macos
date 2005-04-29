.PATH: ${.CURDIR}/${MACHINE_ARCH}/util ${.CURDIR}/util

MISRCS += login.c      login_tty.c  logout.c     logwtmp.c    pty.c fparseln.c \
	opendev.c

.if ${LIB} == "c"
MAN3 += fparseln.3 opendev.3
.endif
