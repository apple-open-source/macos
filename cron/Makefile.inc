# $FreeBSD: src/usr.sbin/cron/Makefile.inc,v 1.4 2005/06/10 06:12:52 des Exp $

.if exists(${.OBJDIR}/../lib)
LIBCRON=${.OBJDIR}/../lib/libcron.a
.else
LIBCRON=${.CURDIR}/../lib/libcron.a
.endif
LIBCRON=${.OBJDIR}/libcron.a

#.include "../Makefile.inc"
