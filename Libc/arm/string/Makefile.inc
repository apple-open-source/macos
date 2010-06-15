# $Version$
#
# ARM-optimised string functions.
#
.PATH: ${.CURDIR}/arm/string

MDSRCS +=	 \
	bcopy.s  \
	bzero.s  \
	ffs.s    \
	memcmp.s \
	memset_pattern.s \
	strcmp.s \
	strlen.s

SUPPRESSSRCS += bcmp.c memcpy.c memmove.c memset.c strlen.c
