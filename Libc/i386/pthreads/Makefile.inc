.PATH: ${.CURDIR}/i386/pthreads
MDSRCS += \
       init_cpu_capabilities.c \
       get_cpu_capabilities.s \
       preempt.s \
       pthread_mutex_lock.s \
       pthread_set_self.s \
       pthread_self.s \
       pthread_getspecific.s \
       start_wqthread.s \
       thread_start.s

DYLDSRCS += \
	preempt.s \
	pthread_set_self.s \
	pthread_self.s \
	pthread_getspecific.s
