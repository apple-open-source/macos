.PATH: ${.CURDIR}/${MACHINE_ARCH}/threads ${.CURDIR}/threads

.sinclude "${.CURDIR}/${MACHINE_ARCH}/threads/Makefile.inc"

MISRCS += cprocs.c       cthreads.c     lu_utils.c     mig_support.c
