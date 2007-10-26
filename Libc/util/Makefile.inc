.PATH: ${.CURDIR}/${MACHINE_ARCH}/util ${.CURDIR}/util

MISRCS += login.c      login_tty.c  logout.c     logwtmp.c    pty.c fparseln.c \
	opendev.c

.if ${LIB} == "c"
MAN3 += fparseln.3 login.3 opendev.3 openpty.3

MLINKS += openpty.3 forkpty.3
MLINKS += openpty.3 login_tty.3
MLINKS += login.3 logwtmp.3
MLINKS += login.3 logout.3
.endif
