#	from @(#)Makefile.inc	8.3 (Berkeley) 2/4/95
# $FreeBSD: src/lib/libc/stdlib/Makefile.inc,v 1.32 2001/09/07 13:03:16 phantom Exp $

# machine-independent stdlib sources
.PATH: ${.CURDIR}/${MACHINE_ARCH}/stdlib ${.CURDIR}/stdlib

MISRCS+=abort.c  bsearch.c getsubopt.c  putenv.c     setenv.c     strtouq.c \
	atexit.c div.c     heapsort.c   qsort.c      strtod.c     system.c \
	atof.c   exit.c    labs.c       radixsort.c  strtol.c \
	atoi.c   getenv.c  ldiv.c       rand.c       strtoq.c \
	atol.c   getopt.c  merge.c      random.c     strtoul.c \
	reallocf.c a64l.c l64a.c strtoll.c strtoull.c

.if ${MACHINE_ARCH} == "alpha"
#  XXX Temporary until the assumption that a long is 32-bits is resolved
#  XXX FreeBSD's code. NetBSD kludged this with Long = int32_t and
#  XXX ULong = u_int32_t
SRCS+=	netbsd_strtod.c
.else
SRCS+=	strtod.c
.endif

# machine-dependent stdlib sources
.if exists(${.CURDIR}/${MACHINE_ARCH}/stdlib/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/stdlib/Makefile.inc"
.endif

.if ${LIB} == "c"
MAN3+=	abort.3 abs.3 alloca.3 atexit.3 atof.3 atoi.3 atol.3 bsearch.3 \
	div.3 exit.3 getenv.3 getopt.3 getsubopt.3 hcreate.3 labs.3 \
	ldiv.3 malloc.3 memory.3 qsort.3 radixsort.3 rand.3 random.3 \
	realpath.3 strfmon.3 strtod.3 strtol.3 strtoul.3 system.3 tsearch.3 \
	a64l.3

MLINKS+=getenv.3 putenv.3 getenv.3 setenv.3 getenv.3 unsetenv.3
MLINKS+=hcreate.3 hdestroy.3 hcreate.3 hsearch.3
MLINKS+=qsort.3 heapsort.3 qsort.3 mergesort.3
MLINKS+=rand.3 rand_r.3 rand.3 srand.3 rand.3 sranddev.3
MLINKS+=random.3 initstate.3 random.3 setstate.3 random.3 srandom.3 \
	random.3 srandomdev.3
MLINKS+=strtol.3 strtoll.3 strtol.3 strtoq.3
MLINKS+=strtoul.3 strtoull.3 strtoul.3 strtouq.3
MLINKS+=malloc.3 calloc.3 malloc.3 free.3 malloc.3 realloc.3 malloc.3 reallocf.3
MLINKS+=tsearch.3 tdelete.3 tsearch.3 tfind.3 tsearch.3 twalk.3 a64l.3 l64a.3
.endif
