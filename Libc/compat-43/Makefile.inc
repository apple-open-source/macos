#	@(#)Makefile.inc	8.1 (Berkeley) 6/2/93
# $FreeBSD: src/lib/libc/compat-43/Makefile.inc,v 1.11 2001/03/27 17:26:46 ru Exp $

# compat-43 sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/compat-43 ${.CURDIR}/compat-43

MISRCS+= sigaltstk.c sigcompat.c
.include "Makefile.fbsd_begin"
FBSDMISRCS= creat.c gethostid.c getwd.c killpg.c sethostid.c setpgrp.c \
	setrgid.c setruid.c
.include "Makefile.fbsd_end"

LEGACYSRCS += creat.c setregid.c setreuid.c sigcompat.c killpg.c
CANCELABLESRCS += creat.c sigcompat.c

# set the LIBC_ALIAS_* macros so we can decorate the symbol independent
# of other macro settings
CFLAGS-creat-fbsd.c += -DLIBC_ALIAS_CREAT
CFLAGS-setregid.c += -DLIBC_ALIAS_SETREGID
CFLAGS-setreuid.c += -DLIBC_ALIAS_SETREUID
CFLAGS-setpgrp-fbsd.c += -DLIBC_ALIAS_SETPGRP
CFLAGS-sigcompat.c += -DLIBC_ALIAS_SIGPAUSE
CFLAGS-killpg-fbsd.c += -DLIBC_ALIAS_KILLPG
CFLAGS-sigaltstk.c += -DLIBC_ALIAS_SIGALTSTACK

.if ${LIB} == "c"
MAN2+=	sigblock.2 sigpause.2 sigsetmask.2 sigvec.2

.include "Makefile.fbsd_begin"
FBSDMAN2= creat.2 killpg.2
FBSDMAN3= gethostid.3 setruid.3
.include "Makefile.fbsd_end"

MLINKS+= gethostid.3 sethostid.3

MLINKS+= setruid.3 setrgid.3

MLINKS+= sigsetmask.2 sigmask.2

.endif
