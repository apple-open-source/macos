.PATH: ${.CURDIR}/gdtoa
CWD := ${.CURDIR}/gdtoa

CFLAGS += -I${.CURDIR}/gdtoa

.sinclude "${.CURDIR}/${MACHINE_ARCH}/stdlib/gdtoa.mk"

GDTOA_UNIQUE_SRCS != ${PERL} -e '@z = split(" ", "$(GDTOA_FBSDSRCS)"); $$, = "\n"; print @z' | sort -u

.include "Makefile.fbsd_begin"
FBSDMISRCS= $(GDTOA_UNIQUE_SRCS) _hdtoa.c glue.c \
    gdtoa-dmisc.c gdtoa-dtoa.c gdtoa-gdtoa.c gdtoa-gethex.c gdtoa-gmisc.c \
    gdtoa-hd_init.c gdtoa-hexnan.c gdtoa-misc.c gdtoa-smisc.c \
    gdtoa-strtod.c gdtoa-strtodg.c gdtoa-strtof.c gdtoa-strtord.c \
    gdtoa-sum.c gdtoa-ulp.c

.ifndef FEATURE_ONLY_LONG_DOUBLE_IS_DOUBLE
FBSDMISRCS+= _ldtoa.c
.endif

FBSDHDRS= gdtoa.h gdtoaimp.h gdtoa_fltrnds.h

.include "Makefile.fbsd_end"

LEGACYSRCS += gdtoa-strtof.c gdtoa-strtod.c gdtoa-strtodg.c
