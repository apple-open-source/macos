# $Version$
#
# i386-optimised string functions.
#
#
#
.PATH: ${.CURDIR}/i386/string
MDSRCS += \
	bcopy.c \
	bcopy_scalar.s \
	bcopy_sse2.s \
	bcopy_sse3x.s \
	bcopy_sse42.s \
	bzero.c \
	bzero_scalar.s \
	bzero_sse2.s \
	bzero_sse42.s \
	__bzero.s \
	longcopy_sse3x.s \
	memset_pattern_sse2.s \
	ffs.s \
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
	memset.s

SUPPRESSSRCS += bcmp.c

DYLDSRCS += \
	__bzero.s \
	bcopy_scalar.s \
	bzero_scalar.s \
	ffs.s \
	memcmp.s \
	memset_pattern_sse2.s \
	strcmp.s \
	strlen.s
