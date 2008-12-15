.PATH: ${.CURDIR}/${MACHINE_ARCH}/util ${.CURDIR}/util

MISRCS += login_tty.c logwtmp.c pty.c fparseln.c opendev.c

.ifdef FEATURE_LEGACY_UTMP_APIS
MISRCS += login.c logout.c
.endif

.for _src in fparseln.c
CFLAGS-${_src} += -D__FBSDID=__RCSID
.endfor

.if ${LIB} == "c"
MAN3 += fparseln.3 opendev.3 openpty.3

.ifdef FEATURE_LEGACY_UTMP_APIS
MAN3 += login.3
.endif

MLINKS += openpty.3 forkpty.3
MLINKS += openpty.3 login_tty.3
.ifdef FEATURE_LEGACY_UTMP_APIS
MLINKS += login.3 logwtmp.3
MLINKS += login.3 logout.3
.endif
.endif
