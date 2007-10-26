#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/mpool/Makefile.inc,v 1.4 2002/11/18 09:50:55 ru Exp $

.PATH: ${.CURDIR}/db/mpool

.include "Makefile.fbsd_begin"
FBSDMISRCS= mpool.c
.for _src in ${FBSDMISRCS}
CFLAGS-${_src:R}-fbsd.${_src:E} += -D__DBINTERFACE_PRIVATE
.endfor
.for _src in mpool.c
CFLAGS-${_src:R}-fbsd.${_src:E} += -UDEBUG
.endfor
.include "Makefile.fbsd_end"
