# $Version$
#
# ARM-optimised string functions.
#
.PATH: ${.CURDIR}/arm/string

MDSRCS +=	\
	bcopy.s \
	bzero.s \
	ffs.s \
	strcmp.s \
	strlen.s

SUPPRESSSRCS += memcpy.c memmove.c memset.c strlen.c
