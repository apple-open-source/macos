.PATH: ${.CURDIR}/arm/pthreads
MDSRCS += \
       get_cpu_capabilities.s \
       pthread_set_self.s \
       pthread_self.s \
       pthread_getspecific.s \
       init_cpu_capabilities.c
