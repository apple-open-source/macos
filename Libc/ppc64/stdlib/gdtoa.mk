# Long double is head-tail pair of doubles
GDTOA_FBSDSRCS+= gdtoa-strtopdd.c machdep_ldisdd.c
MISRCS+= _ldbl_util.c

CFLAGS-_ldbl_util.c += -I${.CURDIR}/fbsdcompat
