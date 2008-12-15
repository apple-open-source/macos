#       from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/db/man/Makefile.inc,v 1.11 2002/11/18 09:50:54 ru Exp $

.PATH: ${.CURDIR}/db/man
CWD := ${.CURDIR}/db/man

.if ${LIB} == "c"
.include "Makefile.fbsd_begin"
FBSDMAN3= btree.3 dbm.3 dbopen.3 hash.3 mpool.3 recno.3
.include "Makefile.fbsd_end"

MLINKS+= dbm.3 dbm_clearerr.3		\
         dbm.3 dbm_close.3		\
         dbm.3 dbm_delete.3		\
         dbm.3 dbm_dirfno.3		\
         dbm.3 dbm_error.3		\
         dbm.3 dbm_fetch.3		\
         dbm.3 dbm_firstkey.3		\
         dbm.3 dbm_nextkey.3		\
         dbm.3 dbm_open.3		\
         dbm.3 dbm_store.3

MLINKS+= dbopen.3 db.3

MLINKS+= mpool.3 mpool_close.3		\
         mpool.3 mpool_filter.3		\
         mpool.3 mpool_get.3		\
         mpool.3 mpool_new.3		\
         mpool.3 mpool_open.3		\
         mpool.3 mpool_put.3		\
         mpool.3 mpool_sync.3

.endif
