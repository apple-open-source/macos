#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: src/lib/libc/string/Makefile.inc,v 1.25 2001/10/10 02:17:35 ache Exp $

.PATH: ${.CURDIR}/${MACHINE_ARCH}/string ${.CURDIR}/string

CFLAGS+= -I${.CURDIR}/locale

# machine-independent string sources
MISRCS+=index.c       rindex.c      strcspn.c     strpbrk.c     strxfrm.c \
	memccpy.c     rindix.c      strdup.c      strsep.c      swab.c \
	memchr.c      strcasecmp.c  strerror.c    strspn.c \
	memcmp.c      strchr.c      strftime.c    strstr.c \
	memset.c      strcoll.c     strmode.c     strtok.c \
	strlcat.c     strlcpy.c

# machine-dependent string sources
.if exists(${.CURDIR}/${MACHINE_ARCH}/string/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/string/Makefile.inc"
.endif

.if ${LIB} == "c"
MAN3+=	bcmp.3 bcopy.3 bstring.3 bzero.3 ffs.3 index.3 memccpy.3 memchr.3 \
	memcmp.3 memcpy.3 memmove.3 memset.3 rindex.3 strcasecmp.3 strcat.3 \
	strchr.3 strcmp.3 strcoll.3 strcpy.3 strcspn.3 strdup.3 strerror.3 \
	string.3 strlcpy.3 strlen.3 strmode.3 strpbrk.3 strrchr.3 strsep.3 \
	strspn.3 strstr.3 strtok.3 strxfrm.3 swab.3 wmemchr.3

MLINKS+=strcasecmp.3 strncasecmp.3
MLINKS+=strcat.3 strncat.3
MLINKS+=strcmp.3 strncmp.3
MLINKS+=strcpy.3 strncpy.3
MLINKS+=strerror.3 perror.3 strerror.3 sys_errlist.3 strerror.3 sys_nerr.3
MLINKS+=strlcpy.3 strlcat.3
MLINKS+=strtok.3 strtok_r.3
MLINKS+=strstr.3 strcasestr.3
MLINKS+=strstr.3 strnstr.3
MLINKS+=wmemchr.3 wmemcmp.3 wmemchr.3 wmemcpy.3 \
	wmemchr.3 wmemmove.3 wmemchr.3 wmemset.3 \
	wmemchr.3 wcscat.3 wmemchr.3 wcschr.3 \
	wmemchr.3 wcscmp.3 wmemchr.3 wcscpy.3 \
	wmemchr.3 wcscspn.3 wmemchr.3 wcslcat.3 \
	wmemchr.3 wcslcpy.3 wmemchr.3 wcslen.3 \
	wmemchr.3 wcsncat.3 wmemchr.3 wcsncmp.3 \
	wmemchr.3 wcsncpy.3 wmemchr.3 wcspbrk.3 \
	wmemchr.3 wcsrchr.3 wmemchr.3 wcsspn.3 \
	wmemchr.3 wcsstr.3
.endif
