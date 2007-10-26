# Long double is head-tail pair of doubles
GDTOA_FBSDSRCS+= gdtoa-strtopdd.c machdep_ldisdd.c
MISRCS+= _ldbl_util.c

CFLAGS-_ldbl_util.c += -I${.CURDIR}/fbsdcompat

# also build a 64-bit long double version (ppc only)
LDBLSRCS += machdep_ldisdd.c
