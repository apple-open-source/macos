.PATH: ${.CURDIR}/ppc/gen

MDSRCS +=	\
     abs.s	\
     fp.h	\
     icacheinval.s \
     mcount.s	\
     setjmperr.c \
     _ctx_start.S \
     getcontext.S \
     getmcontext.c \
     makecontext.c \
     _setcontext.S \
     setcontext.c \
     swapcontext.c

SUPPRESSSRCS += memcpy.c memmove.c memset.c

# makecontext.c can only compile with __DARWIN_UNIX03=0 because the structure
# field names are renamed with __ prefix when __DARWIN_UNIX03=1.  If
# makecontext.c ever needs to build variant, this will have to be fix properly
CFLAGS-makecontext.c = -U__DARWIN_UNIX03 -D__DARWIN_UNIX03=0
