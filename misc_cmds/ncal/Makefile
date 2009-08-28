# $FreeBSD: src/usr.bin/ncal/Makefile,v 1.8 2005/01/27 14:52:45 delphij Exp $

PROG=	ncal
SRCS=	calendar.c easter.c ncal.c
INCS=	calendar.h

WARNS?=	1

CFLAGS+=	-I${.CURDIR}

LINKS= ${BINDIR}/ncal ${BINDIR}/cal
MLINKS= ncal.1 cal.1
.include <bsd.prog.mk>
