# searching i386 directory as a fallback to avoid unnecessary code duplication
.PATH: ${.CURDIR}/x86_64/pthreads ${.CURDIR}/i386/pthreads

MDSRCS += \
       init_cpu_capabilities.c \
       get_cpu_capabilities.s \
       pthread_set_self.s \
       pthread_self.s \
       pthread_getspecific.s \
       start_wqthread.s \
       thread_start.s

