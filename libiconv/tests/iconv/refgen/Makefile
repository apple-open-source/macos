# $FreeBSD$

.PATH: ${.CURDIR}/../tablegen

PROG=	refgen
MAN=
SRCS=	tablegen.c

WARNS?=	3
CFLAGS+= -L/usr/local/lib -I/usr/local/include

LDFLAGS+=	-liconv

.include <bsd.prog.mk>
