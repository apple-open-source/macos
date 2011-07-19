# $Version$
#
# ARM-optimised string functions.
#
.PATH: ${.CURDIR}/arm/string

MDSRCS +=	  \
	bcopy_Generic.s  \
	bcopy_CortexA8.s \
	bcopy_CortexA9.s \
	bzero_Generic.s  \
	bzero_CortexA8.s \
	bzero_CortexA9.s \
	dyld_resolvers.c \
	ffs.s     \
	memcmp.s  \
	strcmp.s  \
	strncmp.s \
	strlen.s  \
	strchr.s  \
	strstr.s  \
	strcpy.s  \
	strlcpy.s \
	strncpy.s \
	strnlen.s
    
DYLDSRCS +=	 \
	bcopy_Generic.s \
	bzero_Generic.s \
	ffs.s \
	strcmp.s \
	strlen.s

.if defined(FEATURE_ARM_ARCH_6)
MDSRCS += memset_pattern.s
.endif

SUPPRESSSRCS += \
	bcmp.c    \
	memcpy.c  \
	memmove.c \
	memset.c  \
	strncmp.c \
	strlen.c  \
	strchr.c  \
	strstr.c  \
	strcpy.c  \
	strlcpy.c \
	strncpy.c \
	strnlen.c

