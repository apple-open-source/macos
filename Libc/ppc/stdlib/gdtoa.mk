# Long double is head-tail pair of doubles
FBSDSRCS+= gdtoa-strtopdd.c machdep_ldisdd.c _ldbl_util.c

# also build a 64-bit long double version (ppc only)
LDBLSRCS += machdep_ldisdd.c
