#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/db/Makefile.inc,v 1.4 2002/11/18 09:50:54 ru Exp $

.PATH: ${.CURDIR}/db/db

.include "Makefile.fbsd_begin"
FBSDMISRCS= db.c
.for _src in ${FBSDMISRCS}
CFLAGS-${_src:R}-fbsd.${_src:E} += -D__DBINTERFACE_PRIVATE
.endfor
.include "Makefile.fbsd_end"
