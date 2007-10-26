# $FreeBSD: src/usr.sbin/cron/cron/Makefile,v 1.15 2001/07/20 06:19:40 obrien Exp $

PROG=	cron
SRCS=	cron.c database.c do_command.c job.c user.c popen.c
#CFLAGS+=-DLOGIN_CAP
DPADD=	${LIBCRON} ${LIBUTIL}
LDADD=	${LIBCRON} -framework IOKit -framework CoreFoundation -lutil
MAN8=	cron.8
BINDIR=/usr/sbin

.include <bsd.prog.mk>
