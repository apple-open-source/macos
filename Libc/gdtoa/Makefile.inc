.PATH: ${.CURDIR}/gdtoa

CFLAGS += -I${.CURDIR}/gdtoa

.ifmake autopatch
ALL_ARCHS ?= i386 ppc ppc64 x86_64
.for a in $(ALL_ARCHS)
.if exists(../${a}/stdlib/gdtoa.mk)
.include "../${a}/stdlib/gdtoa.mk"
.endif
.endfor
.else # !autopatch
.if exists(${.CURDIR}/${MACHINE_ARCH}/stdlib/gdtoa.mk)
.include "${.CURDIR}/${MACHINE_ARCH}/stdlib/gdtoa.mk"
.endif
.endif # autopatch
GDTOA_UNIQUE_SRCS != perl -e '@z = split(" ", "$(GDTOA_FBSDSRCS)"); $$, = "\n"; print @z' | sort -u

.include "Makefile.fbsd_begin"
FBSDMISRCS= $(GDTOA_UNIQUE_SRCS) _hdtoa.c _ldtoa.c glue.c \
    gdtoa-dmisc.c gdtoa-dtoa.c gdtoa-gdtoa.c gdtoa-gethex.c gdtoa-gmisc.c \
    gdtoa-hd_init.c gdtoa-hexnan.c gdtoa-misc.c gdtoa-smisc.c \
    gdtoa-strtod.c gdtoa-strtodg.c gdtoa-strtof.c gdtoa-strtord.c \
    gdtoa-sum.c gdtoa-ulp.c
FBSDHDRS= gdtoa.h gdtoaimp.h 
.include "Makefile.fbsd_end"

LEGACYSRCS += gdtoa-strtof.c gdtoa-strtod.c gdtoa-strtodg.c
