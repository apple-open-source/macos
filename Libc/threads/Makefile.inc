.PATH: ${.CURDIR}/${MACHINE_ARCH}/threads ${.CURDIR}/threads

.if exists(${.CURDIR}/${MACHINE_ARCH}/threads/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/threads/Makefile.inc"
.endif

MISRCS += cprocs.c       cthreads.c     lu_utils.c     mig_support.c
