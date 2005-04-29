# $Version$
#
# PPC-optimised string functions.
#
.PATH: ${.CURDIR}/ppc/string

MDSRCS +=	\
     bcopy.s	\
     bzero.s	\
     memset.s   \
     memcmp.s	\
     strcat.s	\
     strcmp.s	\
     strcpy.s	\
     strlcat.s	\
     strlcpy.s	\
     strlen.s	\
     strncat.s	\
     strncmp.s	\
     strncpy.s

SUPPRESSSRCS += bcmp.c

