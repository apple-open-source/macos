#	from $NetBSD: Makefile.inc,v 1.7 1995/02/27 13:06:20 cgd Exp $
# $FreeBSD: src/lib/libc/nls/Makefile.inc,v 1.7 2001/03/27 17:26:48 ru Exp $

.PATH: ${.CURDIR}/../libc/nls

SRCS+=	msgcat.c

.if ${LIB} == "c"
MAN+=	catclose.3 catgets.3 catopen.3
.endif
