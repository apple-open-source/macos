.PATH: ${.CURDIR}/${MACHINE_ARCH}/threads ${.CURDIR}/threads

.sinclude "${.CURDIR}/${MACHINE_ARCH}/threads/Makefile.inc"

MISRCS += cprocs.c       cthreads.c     mig_support.c

DYLDSRCS += mig_support.c
