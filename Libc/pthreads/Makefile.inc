.sinclude "${.CURDIR}/${MACHINE_ARCH}/pthreads/Makefile.inc"

.PATH: ${.CURDIR}/pthreads

CFLAGS += -I${.CURDIR}/pthreads

MISRCS += pthread_cancelable.c pthread_cond.c pthread_tsd.c pthread.c \
        pthread_mutex.c    thread_setup.c   stack.s pthread_rwlock.c

DYLDSRCS += pthread.c pthread_mutex.c pthread_tsd.c

.ifdef FEATURE_PLOCKSTAT
${SYMROOTINC}/plockstat.h: ${.CURDIR}/pthreads/plockstat.d
	${MKDIR} ${SYMROOTINC}
	${DTRACE} -o ${.TARGET} -C -h -s ${.ALLSRC}

.for _src in pthread_cond.c pthread_mutex.c pthread_rwlock.c
${_src:R}.${OBJSUFFIX}: ${SYMROOTINC}/plockstat.h
.endfor
.endif # FEATURE_PLOCKSTAT

PTHREADS_INSTHDRS += pthread.h pthread_spis.h pthread_impl.h sched.h
PTHREADS_INSTHDRS := ${PTHREADS_INSTHDRS:S/^/${.CURDIR}\/pthreads\//}
INSTHDRS += ${PTHREADS_INSTHDRS}

PRIV_INSTHDRS += ${.CURDIR}/pthreads/pthread_machdep.h
LOCALHDRS += ${.CURDIR}/pthreads/pthread_workqueue.h

LEGACYSRCS += pthread.c pthread_cancelable.c pthread_cond.c pthread_mutex.c pthread_rwlock.c
CANCELABLESRCS += pthread_cancelable.c

# thread_setup.c can only compile with __DARWIN_UNIX03=0 (for ppc64) because
# the structure field names are renamed with __ prefix when __DARWIN_UNIX03=1.
# If thread_setup.c ever needs to build variant, this will have to be fix
# properly
CFLAGS-thread_setup.c = -U__DARWIN_UNIX03 -D__DARWIN_UNIX03=0

# set the LIBC_ALIAS_* macros so we can decorate the symbol independent
# of other macro settings
CFLAGS-pthread.c += -DLIBC_ALIAS_PTHREAD_CANCEL -DLIBC_ALIAS_PTHREAD_SETCANCELSTATE -DLIBC_ALIAS_PTHREAD_SETCANCELTYPE -DLIBC_ALIAS_PTHREAD_SIGMASK -DLIBC_ALIAS_PTHREAD_TESTCANCEL
CFLAGS-pthread_cancelable.c += -DLIBC_ALIAS_PTHREAD_COND_TIMEDWAIT -DLIBC_ALIAS_PTHREAD_COND_WAIT -DLIBC_ALIAS_PTHREAD_JOIN -DLIBC_ALIAS_SIGWAIT
CFLAGS-pthread_cond.c += -DLIBC_ALIAS_PTHREAD_COND_INIT
CFLAGS-pthread_mutex.c += -DLIBC_ALIAS_PTHREAD_MUTEXATTR_DESTROY
CFLAGS-pthread_rwlock.c += -DLIBC_ALIAS_PTHREAD_RWLOCK_DESTROY -DLIBC_ALIAS_PTHREAD_RWLOCK_INIT -DLIBC_ALIAS_PTHREAD_RWLOCK_RDLOCK -DLIBC_ALIAS_PTHREAD_RWLOCK_TRYRDLOCK -DLIBC_ALIAS_PTHREAD_RWLOCK_TRYWRLOCK -DLIBC_ALIAS_PTHREAD_RWLOCK_UNLOCK -DLIBC_ALIAS_PTHREAD_RWLOCK_WRLOCK

.if defined(CCARCH) && ${CCARCH} == armv6
CFLAGS-pthread.c += -mno-thumb
CFLAGS-pthread_cancelable.c += -mno-thumb
CFLAGS-pthread_cond.c += -mno-thumb
CFLAGS-pthread_mutex.c += -mno-thumb
CFLAGS-pthread_rwlock.c += -mno-thumb
CFLAGS-pthread_tsd.c += -mno-thumb
.endif

STRIP_HDRS += pthread.h

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

MLINKS+= pthread_attr.3 pthread_attr_destroy.3				\
         pthread_attr.3 pthread_attr_getdetachstate.3			\
         pthread_attr.3 pthread_attr_getinheritsched.3			\
         pthread_attr.3 pthread_attr_getschedparam.3			\
         pthread_attr.3 pthread_attr_getschedpolicy.3			\
         pthread_attr.3 pthread_attr_getscope.3				\
         pthread_attr.3 pthread_attr_getstackaddr.3			\
         pthread_attr.3 pthread_attr_getstacksize.3			\
         pthread_attr.3 pthread_attr_init.3				\
         pthread_attr.3 pthread_attr_setdetachstate.3			\
         pthread_attr.3 pthread_attr_setinheritsched.3			\
         pthread_attr.3 pthread_attr_setschedparam.3			\
         pthread_attr.3 pthread_attr_setschedpolicy.3			\
         pthread_attr.3 pthread_attr_setscope.3				\
         pthread_attr.3 pthread_attr_setstackaddr.3			\
         pthread_attr.3 pthread_attr_setstacksize.3

MLINKS+= pthread_getschedparam.3 pthread_setschedparam.3

MLINKS+= pthread_mutexattr.3 pthread_mutexattr_destroy.3		\
         pthread_mutexattr.3 pthread_mutexattr_getprioceiling.3		\
         pthread_mutexattr.3 pthread_mutexattr_getprotocol.3		\
         pthread_mutexattr.3 pthread_mutexattr_gettype.3		\
         pthread_mutexattr.3 pthread_mutexattr_init.3			\
         pthread_mutexattr.3 pthread_mutexattr_setprioceiling.3		\
         pthread_mutexattr.3 pthread_mutexattr_setprotocol.3		\
         pthread_mutexattr.3 pthread_mutexattr_settype.3

MLINKS+= pthread_setcancelstate.3 pthread_setcanceltype.3		\
         pthread_setcancelstate.3 pthread_testcancel.3

MLINKS+= pthread_condattr.3 pthread_condattr_destroy.3			\
         pthread_condattr.3 pthread_condattr_init.3

MLINKS+= pthread_rwlock_rdlock.3 pthread_rwlock_tryrdlock.3 

MLINKS+= pthread_rwlock_wrlock.3 pthread_rwlock_trywrlock.3 

.endif


