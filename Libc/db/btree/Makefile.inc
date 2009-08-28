#	from @(#)Makefile.inc	8.2 (Berkeley) 7/14/94
# $FreeBSD: src/lib/libc/db/btree/Makefile.inc,v 1.4 2002/11/18 09:50:54 ru Exp $

.PATH: ${.CURDIR}/db/btree
CWD := ${.CURDIR}/db/btree

.include "Makefile.fbsd_begin"
FBSDMISRCS= bt_close.c bt_conv.c bt_delete.c bt_get.c bt_open.c \
	bt_overflow.c bt_page.c bt_put.c bt_search.c bt_seq.c bt_split.c \
	bt_utils.c 
.for _src in ${FBSDMISRCS}
CFLAGS-${_src:R}-fbsd.${_src:E} += -D__DBINTERFACE_PRIVATE
.endfor
.for _src in bt_open.c bt_overflow.c
CFLAGS-${_src:R}-fbsd.${_src:E} += -UDEBUG
.endfor
FBSDHDRS= btree.h
.include "Makefile.fbsd_end"

# This .for statement forces evaluation of ${CWD}
.for _cwd in ${CWD}

# need to rename extern.h to make it unique
.ifmake autopatch
${_cwd}/bt_extern.h: ${_cwd}/FreeBSD/extern.h _AUTOPATCH
AUTOPATCHHDRS+= ${_cwd}/bt_extern.h
.endif # autopatch

INSTBTREEPRIVHDRS_AUTOPATCH+= ${_cwd}/bt_extern.h ${_cwd}/btree.h

.endfor # _cwd
