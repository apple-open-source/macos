# $Version$
#
# i386-optimised string functions.
#
#
#
.PATH: ${.CURDIR}/i386/string
MDSRCS += bcopy.s \
	bzero.s \
	memcpy.s \
	memmove.s \
	strcmp.s
