#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/hash/Makefile.inc,v 1.5 2002/11/18 09:50:54 ru Exp $

.PATH: ${.CURDIR}/db/hash
CWD := ${.CURDIR}/db/hash

.include "Makefile.fbsd_begin"
FBSDMISRCS= hash.c hash_bigkey.c hash_buf.c hash_func.c hash_log2.c \
	hash_page.c ndbm.c
.for _src in ${FBSDMISRCS}
CFLAGS-${_src:R}-fbsd.${_src:E} += -D__DBINTERFACE_PRIVATE
.endfor
.for _src in hash.c hash_bigkey.c hash_buf.c hash_page.c
CFLAGS-${_src:R}-fbsd.${_src:E} += -UDEBUG
.endfor
FBSDHDRS= hash.h page.h
.include "Makefile.fbsd_end"

# need to rename extern.h to make it unique
.ifmake autopatch

# This .for statement forces evaluation of ${CWD}
.for _cwd in ${CWD}
${_cwd}/hash_extern.h: ${_cwd}/FreeBSD/extern.h _AUTOPATCH
AUTOPATCHHDRS+= ${_cwd}/hash_extern.h
.endfor # _cwd

.endif # autopatch
