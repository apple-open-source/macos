#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/recno/Makefile.inc,v 1.4 2002/11/18 09:50:55 ru Exp $

.PATH: ${.CURDIR}/db/recno
CWD := ${.CURDIR}/db/recno

.include "Makefile.fbsd_begin"
FBSDMISRCS= rec_close.c rec_delete.c rec_get.c rec_open.c rec_put.c rec_search.c \
	rec_seq.c rec_utils.c
.for _src in ${FBSDMISRCS}
CFLAGS-${_src:R}-fbsd.${_src:E} += -D__DBINTERFACE_PRIVATE
.endfor
FBSDHDRS= recno.h
.include "Makefile.fbsd_end"

# need to rename extern.h to make it unique
.ifmake autopatch

# This .for statement forces evaluation of ${CWD}
.for _cwd in ${CWD}
${_cwd}/rec_extern.h: ${_cwd}/FreeBSD/extern.h _AUTOPATCH
AUTOPATCHHDRS+= ${_cwd}/rec_extern.h
.endfor # _cwd

.endif # autopatch
