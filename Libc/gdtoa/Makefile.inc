.PATH: ${.CURDIR}/gdtoa

CFLAGS += -I${.CURDIR}/gdtoa

.include "Makefile.fbsd_begin"
FBSDSRCS= _ldtoa.c glue.c \
    gdtoa-dmisc.c gdtoa-dtoa.c gdtoa-gdtoa.c gdtoa-gethex.c gdtoa-gmisc.c \
    gdtoa-hd_init.c gdtoa-hexnan.c gdtoa-misc.c gdtoa-smisc.c gdtoa-strtoIg.c \
    gdtoa-strtod.c gdtoa-strtodg.c gdtoa-strtof.c gdtoa-strtord.c gdtoa-sum.c \
    gdtoa-ulp.c
.if exists(${.CURDIR}/${MACHINE_ARCH}/stdlib/gdtoa.mk)
.include "${.CURDIR}/${MACHINE_ARCH}/stdlib/gdtoa.mk"
.endif
FBSDORIGHDRS= gdtoa.h gdtoaimp.h 
.include "Makefile.fbsd_end"
