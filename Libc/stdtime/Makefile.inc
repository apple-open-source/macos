#	Makefile.inc,v 1.2 1994/09/13 21:26:01 wollman Exp
# $FreeBSD: src/lib/libc/stdtime/Makefile.inc,v 1.14 2002/11/18 09:50:56 ru Exp $

.PATH: ${.CURDIR}/stdtime
CWD := ${.CURDIR}/stdtime

CFLAGS += -I${.CURDIR}/stdtime

LEGACYSRCS += localtime.c strftime.c strptime.c

MISRCS += getdate.c timezone_unix03.c

# set the LIBC_ALIAS_* macros so we can decorate the symbol independent
# of other macro settings
CFLAGS-localtime-fbsd.c += -DLIBC_ALIAS_MKTIME
CFLAGS-strftime-fbsd.c += -DLIBC_ALIAS_STRFTIME -DLIBC_ALIAS_STRFTIME_L
CFLAGS-strptime-fbsd.c += -DLIBC_ALIAS_STRPTIME -DLIBC_ALIAS_STRPTIME_L

.include "Makefile.fbsd_begin"
FBSDMISRCS= asctime.c difftime.c ftime.c localtime.c \
	strftime.c strptime.c timelocal.c time32.c
FBSDHDRS= private.h timelocal.h tzfile.h
.include "Makefile.fbsd_end"
INSTHDRS += ${.CURDIR}/stdtime/tzfile.h

.if ${LIB} == "c"
.include "Makefile.fbsd_begin"
FBSDMAN3= ctime.3 ftime.3 strftime.3 strptime.3 time2posix.3
FBSDMAN5= tzfile.5
.include "Makefile.fbsd_end"

MAN3   += getdate.3 timegm.3
MLINKS += timegm.3 timelocal.3

MLINKS+=ctime.3 asctime.3 ctime.3 difftime.3 ctime.3 gmtime.3 \
	ctime.3 localtime.3 ctime.3 mktime.3 \
	ctime.3 ctime_r.3 ctime.3 localtime_r.3 ctime.3 gmtime_r.3 \
	ctime.3 asctime_r.3
MLINKS+=time2posix.3 posix2time.3
MLINKS+=strftime.3 strftime_l.3
MLINKS+=strptime.3 strptime_l.3
.endif
