# $Id: Makefile,v 1.2 2004/05/27 08:07:49 pwd Exp $

PROG=uid_driver

VPATH=../driver
CFLAGS+=-Wmost -Wno-four-char-constants -Wno-unknown-pragmas -Werror -g
SRCS=sysctl.c uid_driver.c
NOMAN=YES
CFLAGS+= -I../kext -I../driver

.include <bsd.prog.mk>
