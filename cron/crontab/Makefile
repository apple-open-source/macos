# $FreeBSD: src/usr.sbin/cron/crontab/Makefile,v 1.11 2001/03/26 14:39:47 ru Exp $

PROG=	crontab
CFLAGS+=-I${.CURDIR}/../cron
DPADD=	${LIBCRON} 
LDADD=	${LIBCRON}
MAN1=	crontab.1 
MAN5=   crontab.5

BINDIR=	/usr/bin
BINOWN=	root
BINMODE=4555
#INSTALLFLAGS=-fschg

.include <bsd.prog.mk>
