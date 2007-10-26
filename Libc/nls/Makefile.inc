#	from $NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $
# $FreeBSD: src/lib/libc/nls/Makefile.inc,v 1.5.2.2 2001/04/25 10:04:07 ru Exp $

.PATH: ${.CURDIR}/nls

.include "Makefile.fbsd_begin"
FBSDMISRCS= msgcat.c
FBSDHDRS= msgcat.h
.include "Makefile.fbsd_end"

# Install msgcat.h for usage by gencat (in adv_cmds)
LOCALHDRS+=	${.CURDIR}/nls/msgcat.h

.if ${LIB} == "c"
.include "Makefile.fbsd_begin"
FBSDMAN3= catclose.3 catgets.3 catopen.3
.include "Makefile.fbsd_end"
.endif
