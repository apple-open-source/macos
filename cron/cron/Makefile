# $FreeBSD: src/usr.sbin/cron/cron/Makefile,v 1.14 2001/03/26 14:39:47 ru Exp $

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
#CFLAGS+=-DLOGIN_CAP
DPADD=	${LIBCRON} 
LDADD=	${LIBCRON} -framework IOKit -framework CoreFoundation
MAN8=	cron.8
BINDIR=/usr/sbin

.include <bsd.prog.mk>
