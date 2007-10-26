# $Version$
#
# i386-optimised string functions.
#
#
#
.PATH: ${.CURDIR}/i386/string
MDSRCS += bcopy.s \
	bzero.s \
	ffs.s \
	memcpy.s \
	memmove.s \
	strlcat.s \
	strlcpy.s \
	strlen.s \
	strcpy.s \
	strcmp.s \
	strncpy.s \
	strncmp.s \
	memcmp.s \
	bcmp.s \
	memset.s
