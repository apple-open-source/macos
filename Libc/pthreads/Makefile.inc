.if exists(${.CURDIR}/${MACHINE_ARCH}/pthreads/Makefile.inc)
.include "${.CURDIR}/${MACHINE_ARCH}/pthreads/Makefile.inc"
.endif

.PATH: ${.CURDIR}/pthreads

MISRCS +=                  pthread_cond.c pthread_tsd.c pthread.c \
        pthread_mutex.c    thread_setup.c lock.s   stack.s pthread_rwlock.c

PTHREADS_INSTHDRS += pthread.h pthread_impl.h sched.h
PTHREADS_INSTHDRS := ${PTHREADS_INSTHDRS:S/^/${.CURDIR}\/pthreads\//}
INSTHDRS += ${PTHREADS_INSTHDRS}

UNIX03SRCS += pthread.c pthread_cond.c

.if ${LIB} == "c"
MAN3+=	pthread.3 pthread_atfork.3 pthread_attr.3  pthread_cancel.3 \
	pthread_cleanup_pop.3 pthread_cleanup_push.3 \
	pthread_cond_broadcast.3 pthread_cond_destroy.3 \
	pthread_cond_init.3 pthread_cond_signal.3 \
	pthread_cond_timedwait.3 pthread_cond_wait.3 \
	pthread_create.3 pthread_detach.3 \
	pthread_equal.3 pthread_exit.3 \
	pthread_getschedparam.3 pthread_getspecific.3 \
	pthread_join.3 pthread_key_create.3 \
	pthread_key_delete.3 pthread_mutex_destroy.3 \
	pthread_mutex_init.3 pthread_mutex_lock.3 \
	pthread_mutex_trylock.3 pthread_mutex_unlock.3 \
	pthread_mutexattr.3 pthread_once.3 \
	pthread_self.3 pthread_setcancelstate.3 \
	pthread_setspecific.3 pthread_condattr.3 \
	pthread_rwlock_destroy.3 pthread_rwlock_init.3 \
	pthread_rwlock_rdlock.3 pthread_rwlock_unlock.3 \
	pthread_rwlock_wrlock.3 pthread_rwlockattr_destroy.3 \
	pthread_rwlockattr_getpshared.3 pthread_rwlockattr_init.3 \
	pthread_rwlockattr_setpshared.3

MLINKS+=pthread_attr.3 pthread_attr_destroy.3 \
	pthread_attr.3 pthread_attr_getdetachstate.3 \
	pthread_attr.3 pthread_attr_getinheritsched.3 \
	pthread_attr.3 pthread_attr_getschedparam.3 \
	pthread_attr.3 pthread_attr_getschedpolicy.3 \
	pthread_attr.3 pthread_attr_getscope.3 \
	pthread_attr.3 pthread_attr_getstackaddr.3 \
	pthread_attr.3 pthread_attr_getstacksize.3 \
	pthread_attr.3 pthread_attr_init.3 \
	pthread_attr.3 pthread_attr_setdetachstate.3 \
	pthread_attr.3 pthread_attr_setinheritsched.3 \
	pthread_attr.3 pthread_attr_setschedparam.3 \
	pthread_attr.3 pthread_attr_setschedpolicy.3 \
	pthread_attr.3 pthread_attr_setscope.3 \
	pthread_attr.3 pthread_attr_setstackaddr.3 \
	pthread_attr.3 pthread_attr_setstacksize.3
	
MLINKS+=pthread_getschedparam.3 pthread_setschedparam.3

MLINKS+=pthread_mutexattr.3 pthread_mutexattr_destroy.3 \
	pthread_mutexattr.3 pthread_mutexattr_getprioceiling.3 \
	pthread_mutexattr.3 pthread_mutexattr_getprotocol.3 \
	pthread_mutexattr.3 pthread_mutexattr_gettype.3 \
	pthread_mutexattr.3 pthread_mutexattr_init.3 \
	pthread_mutexattr.3 pthread_mutexattr_setprioceiling.3 \
	pthread_mutexattr.3 pthread_mutexattr_setprotocol.3 \
	pthread_mutexattr.3 pthread_mutexattr_settype.3

MLINKS+=pthread_setcancelstate.3 pthread_setcanceltype.3 \
	pthread_setcancelstate.3 pthread_testcancel.3

MLINKS+=pthread_condattr.3 pthread_condattr_init.3 \
	pthread_condattr.3 pthread_condattr_destroy.3

MLINKS+=pthread_rwlock_rdlock.3 pthread_rwlock_tryrdlock.3 

MLINKS+=pthread_rwlock_wrlock.3 pthread_rwlock_trywrlock.3 

.endif


