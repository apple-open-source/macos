.PATH: ${.CURDIR}/${MACHINE_ARCH}/threads ${.CURDIR}/threads

.ifnmake autopatch
.if exists(${.CURDIR}/${MACHINE_ARCH}/threads/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/threads/Makefile.inc"
.endif
.endif # !autopatch

MISRCS += cprocs.c       cthreads.c     lu_utils.c     mig_support.c
