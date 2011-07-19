# $Version$
#
# x86-64-optimised string functions.
#
#
#
.PATH: ${.CURDIR}/x86_64/string

MDSRCS += \
	bcopy.c \
	bcopy_sse3x.s \
	bcopy_sse42.s \
	bzero.c \
	bzero_sse2.s \
	bzero_sse42.s \
	__bzero.s \
	longcopy_sse3x.s \
	memcpy.c \
	memmove.c \
	strlcat.s \
	strlcpy.s \
	strlen.s \
	strcpy.s \
	strcmp.s \
	strncpy.s \
	strncmp.s \
	memcmp.s \
	memset.s \
	ffs.s

SUPPRESSSRCS += bcmp.c

DYLDSRCS += \
	__bzero.s \
	bcopy_sse3x.s \
	bzero_sse2.s \
	ffs.s \
	longcopy_sse3x.s \
	strcmp.s \
	strlen.s
