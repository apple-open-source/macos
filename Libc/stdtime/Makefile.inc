#	Makefile.inc,v 1.2 1994/09/13 21:26:01 wollman Exp
# $FreeBSD: src/lib/libc/stdtime/Makefile.inc,v 1.14 2002/11/18 09:50:56 ru Exp $

.PATH:	${.CURDIR}/stdtime

CFLAGS += -DNOTIFY_TZ

.include "Makefile.fbsd_begin"
FBSDSRCS= asctime.c difftime.c localtime.c strftime.c strptime.c timelocal.c \
	time32.c
FBSDORIGHDRS= private.h timelocal.h tzfile.h
.include "Makefile.fbsd_end"

.if ${LIB} == "c"
.include "Makefile.fbsd_begin"
FBSDMAN3= ctime.3 strftime.3 strptime.3 time2posix.3
FBSDMAN5= tzfile.5
.include "Makefile.fbsd_end"

MLINKS+=ctime.3 asctime.3 ctime.3 difftime.3 ctime.3 gmtime.3 \
	ctime.3 localtime.3 ctime.3 mktime.3 ctime.3 timegm.3 \
	ctime.3 ctime_r.3 ctime.3 localtime_r.3 ctime.3 gmtime_r.3 \
	ctime.3 asctime_r.3
MLINKS+=time2posix.3 posix2time.3
.endif
