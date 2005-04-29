.PATH: ${.CURDIR}/ppc/gen

MDSRCS +=	\
     abs.s	\
     ffs.s	\
     fp.h	\
     icacheinval.s \
     mcount.s	\
     setjmperr.c

SUPPRESSSRCS += memcpy.c memmove.c memset.c
