# searching ppc directory as a fallback to avoid unnecessary code duplication
.PATH: ${.CURDIR}/ppc64/pthreads ${.CURDIR}/ppc/pthreads

MDSRCS += \
       init_cpu_capabilities.c \
       get_cpu_capabilities.s \
       pthread_set_self_64.s \
       pthread_self_64.s \
       pthread_getspecific_64.s
