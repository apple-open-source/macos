#	@(#)Makefile.inc	8.1 (Berkeley) 6/2/93
# $FreeBSD: src/lib/libc/compat-43/Makefile.inc,v 1.11 2001/03/27 17:26:46 ru Exp $

# compat-43 sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/compat-43 ${.CURDIR}/compat-43

SRCS+= creat.c  getwd.c send.c       setpgrp.c    setreuid.c   setruid.c \
       gethostid.c killpg.c sethostid.c  setregid.c   setrgid.c    sigcompat.c


.if ${LIB} == "c"
MAN2+=	creat.2 killpg.2 sigpause.2 sigvec.2
MAN3+=	gethostid.3 setruid.3

MLINKS+=gethostid.3 sethostid.3
MLINKS+=setruid.3 setrgid.3

.endif
