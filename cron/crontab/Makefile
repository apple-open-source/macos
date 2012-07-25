# $FreeBSD: src/usr.sbin/cron/crontab/Makefile,v 1.14 2004/11/03 18:01:21 ru Exp $

BINDIR= /usr/bin

PROG=	crontab
BINOWN=	root
BINMODE=4555
CFLAGS+=-I${.CURDIR}/../cron
DPADD=	${LIBCRON}
LDADD=	${LIBCRON}
MAN1=	crontab.1 
MAN5=   crontab.5

.include <bsd.prog.mk>
