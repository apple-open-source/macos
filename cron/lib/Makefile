# $FreeBSD: src/usr.sbin/cron/lib/Makefile,v 1.7 2004/10/24 15:33:02 ru Exp $

LIB=	cron

SRCS= entry.c env.c misc.c

CFLAGS+= -g -I${.CURDIR}/../cron
#CFLAGS+=-DLOGIN_CAP
NO_PIC=		yes
NO_PROFILE=	yes

install:
	@echo -n

.include <bsd.lib.mk>
