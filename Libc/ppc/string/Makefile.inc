# $Version$
#
# PPC-optimised string functions.
#
MDSRCS +=	\
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

