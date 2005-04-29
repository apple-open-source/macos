/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1996 1995 by Open Software Foundation, Inc. 1997 1996 1995 1994 1993 1992 1991  
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * MkLinux
 */

/*
 * POSIX Threads - IEEE 1003.1c
 */

#ifndef _POSIX_PTHREAD_INTERNALS_H
#define _POSIX_PTHREAD_INTERNALS_H

// suppress pthread_attr_t typedef in sys/signal.h
#define _PTHREAD_ATTR_T
struct _pthread_attr_t; /* forward reference */
typedef struct _pthread_attr_t pthread_attr_t;

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <mach/mach.h>
#include <mach/mach_error.h>


#ifndef __POSIX_LIB__
#define __POSIX_LIB__
#endif

#include "posix_sched.h"		/* For POSIX scheduling policy & parameter */
#include <sys/queue.h>		/* For POSIX scheduling policy & parameter */
#include "pthread_machdep.h"		/* Machine-dependent definitions. */
#include "pthread_spinlock.h"		/* spinlock definitions. */

LIST_HEAD(__pthread_list, _pthread);
extern struct __pthread_list __pthread_head;        /* head of list of open files */
extern pthread_lock_t _pthread_list_lock;
/*
 * Compiled-in limits
 */
#undef _POSIX_THREAD_KEYS_MAX
#define _POSIX_THREAD_KEYS_MAX	      128

/*
 * Threads
 */
#define _PTHREAD_T
typedef struct _pthread
{
	long	       sig;	      /* Unique signature for this structure */
	struct __darwin_pthread_handler_rec *__cleanup_stack;
	pthread_lock_t lock;	      /* Used for internal mutex on structure */
	u_int32_t	detached:8,
			inherit:8,
			policy:8,
			pad:8;
	size_t	       guardsize;	/* size in bytes to guard stack overflow */
#if  !defined(__LP64__)
	int	       pad0;		/* for backwards compatibility */
#endif
	struct sched_param param;
	struct _pthread_mutex *mutexes;
	struct _pthread *joiner;
#if !defined(__LP64__)
	int		pad1;		/* for backwards compatibility */
#endif
	void           *exit_value;
	semaphore_t    death;		/* pthread_join() uses this to wait for death's call */
	mach_port_t    kernel_thread; /* kernel thread this thread is bound to */
	void	       *(*fun)(void*);/* Thread start routine */
        void	       *arg;	      /* Argment for thread start routine */
	int	       cancel_state;  /* Whether thread can be cancelled */
	int	       err_no;		/* thread-local errno */
	void	       *tsd[_POSIX_THREAD_KEYS_MAX];  /* Thread specific data */
        void           *stackaddr;     /* Base of the stack (is aligned on vm_page_size boundary */
        size_t         stacksize;      /* Size of the stack (is a multiple of vm_page_size and >= PTHREAD_STACK_MIN) */
	mach_port_t    reply_port;     /* Cached MiG reply port */
#if defined(__LP64__)
        int		pad2;		/* for natural alignment */
#endif
	void           *cthread_self;  /* cthread_self() if somebody calls cthread_set_self() */
        boolean_t      freeStackOnExit; /* Should we free the stack when we're done? */
#if defined(__LP64__)
	int		pad3;		/* for natural alignment */
#endif
	LIST_ENTRY(_pthread) plist;
} *pthread_t;

/*
 * This will cause a compile-time failure if someone moved the tsd field
 * and we need to change _PTHREAD_TSD_OFFSET in pthread_machdep.h
 */
typedef char _need_to_change_PTHREAD_TSD_OFFSET[(_PTHREAD_TSD_OFFSET == offsetof(struct _pthread, tsd[0])) ? 0 : -1] ;

/*
 * Thread attributes
 */
struct _pthread_attr_t
{
	long	       sig;	      /* Unique signature for this structure */
	pthread_lock_t lock;	      /* Used for internal mutex on structure */
	u_int32_t	detached:8,
			inherit:8,
			policy:8,
			reserved1:8;
	size_t	       guardsize;	/* size in bytes to guard stack overflow */
	int	       reserved2;
	struct sched_param param;
        void           *stackaddr;     /* Base of the stack (is aligned on vm_page_size boundary */
        size_t         stacksize;      /* Size of the stack (is a multiple of vm_page_size and >= PTHREAD_STACK_MIN) */
	boolean_t      freeStackOnExit;/* Should we free the stack when we exit? */
};

/*
 * Mutex attributes
 */
#define _PTHREAD_MUTEXATTR_T
typedef struct 
{
	long sig;		     /* Unique signature for this structure */
	int prioceiling;
	u_int32_t protocol:2,		/* protocol attribute */
		type:2,			/* mutex type */
		rfu:28;
} pthread_mutexattr_t;

/*
 * Mutex variables
 */
#define _PTHREAD_MUTEX_T
typedef struct _pthread_mutex
{
	long	       sig;	      /* Unique signature for this structure */
	pthread_lock_t lock;	      /* Used for internal mutex on structure */
	u_int32_t      waiters;       /* Count of threads waiting for this mutex */
	pthread_t      owner;	      /* Which thread has this mutex locked */
	semaphore_t    sem;	      /* Semaphore used for waiting */
	u_int32_t	 protocol:2,		/* protocol */
		type:2,			/* mutex type */
		rfu:12,
		lock_count:16;
	struct _pthread_mutex *next, *prev;  /* List of other mutexes he owns */
	struct _pthread_cond *busy;   /* List of condition variables using this mutex */
	int16_t       prioceiling;
	int16_t	       priority;      /* Priority to restore when mutex unlocked */
	semaphore_t	order;
} pthread_mutex_t;

/*
 * Condition variable attributes
 */
#define _PTHREAD_CONDATTR_T
typedef struct 
{
	long	       sig;	     /* Unique signature for this structure */
	int unsupported;
} pthread_condattr_t;

/*
 * Condition variables
 */
#define _PTHREAD_COND_T
typedef struct _pthread_cond
{
	long	       sig;	     /* Unique signature for this structure */
	pthread_lock_t lock;	     /* Used for internal mutex on structure */
	semaphore_t    sem;	     /* Kernel semaphore */
	struct _pthread_cond *next, *prev;  /* List of condition variables using mutex */
	struct _pthread_mutex *busy; /* mutex associated with variable */
	u_int32_t	waiters:16,	/* Number of threads waiting */
		   sigspending:16;	/* Number of outstanding signals */
} pthread_cond_t;

/*
 * Initialization control (once) variables
 */
#define _PTHREAD_ONCE_T
typedef struct 
{
	long	       sig;	      /* Unique signature for this structure */
	pthread_lock_t lock;	      /* Used for internal mutex on structure */
} pthread_once_t;

#define _PTHREAD_RWLOCKATTR_T
typedef struct {
	long	       sig;	      /* Unique signature for this structure */
	int             pshared;
	int		rfu[2];		/* reserved for future use */
} pthread_rwlockattr_t;

#define _PTHREAD_RWLOCK_T
typedef struct {
	long 		sig;
        pthread_mutex_t lock;   /* monitor lock */
	int             state;
	pthread_cond_t  read_signal;
	pthread_cond_t  write_signal;
	int             blocked_writers;
	int             pshared;
	int		rfu[3];
} pthread_rwlock_t;

#include "pthread.h"

#if defined(__i386__) || defined(__ppc64__)
/*
 * Inside libSystem, we can use r13 or %gs directly to get access to the
 * thread-specific data area. The current thread is in the first slot.
 */
inline static pthread_t __attribute__((__pure__))
_pthread_self_direct(void)
{
       pthread_t ret;
#if defined(__i386__)
       asm("movl %%gs:%P1, %0" : "=r" (ret) : "i" (offsetof(struct _pthread, tsd[0])));
#elif defined(__ppc64__)
	register const pthread_t __pthread_self asm ("r13");
	ret = __pthread_self;
#endif
       return ret;
}
#define pthread_self() _pthread_self_direct()
#endif

#define _PTHREAD_DEFAULT_INHERITSCHED	PTHREAD_INHERIT_SCHED
#define _PTHREAD_DEFAULT_PROTOCOL	PTHREAD_PRIO_NONE
#define _PTHREAD_DEFAULT_PRIOCEILING	0
#define _PTHREAD_DEFAULT_POLICY		SCHED_OTHER
#define _PTHREAD_DEFAULT_STACKSIZE	0x80000	  /* 512K */

#define _PTHREAD_NO_SIG			0x00000000
#define _PTHREAD_MUTEX_ATTR_SIG		0x4D545841  /* 'MTXA' */
#define _PTHREAD_MUTEX_SIG		0x4D555458  /* 'MUTX' */
#define _PTHREAD_MUTEX_SIG_init		0x32AAABA7  /* [almost] ~'MUTX' */
#define _PTHREAD_COND_ATTR_SIG		0x434E4441  /* 'CNDA' */
#define _PTHREAD_COND_SIG		0x434F4E44  /* 'COND' */
#define _PTHREAD_COND_SIG_init		0x3CB0B1BB  /* [almost] ~'COND' */
#define _PTHREAD_ATTR_SIG		0x54484441  /* 'THDA' */
#define _PTHREAD_ONCE_SIG		0x4F4E4345  /* 'ONCE' */
#define _PTHREAD_ONCE_SIG_init		0x30B1BCBA  /* [almost] ~'ONCE' */
#define _PTHREAD_SIG			0x54485244  /* 'THRD' */
#define _PTHREAD_RWLOCK_ATTR_SIG	0x52574C41  /* 'RWLA' */
#define _PTHREAD_RWLOCK_SIG		0x52574C4B  /* 'RWLK' */
#define _PTHREAD_RWLOCK_SIG_init	0x2DA8B3B4  /* [almost] ~'RWLK' */

#define _PTHREAD_CREATE_PARENT	     4
#define _PTHREAD_EXITED		     8

#if defined(DEBUG)
#define _PTHREAD_MUTEX_OWNER_SELF	pthread_self()
#else
#define _PTHREAD_MUTEX_OWNER_SELF	(pthread_t)0x12141968
#endif
#define _PTHREAD_MUTEX_OWNER_SWITCHING	(pthread_t)(~0)

#define _PTHREAD_CANCEL_STATE_MASK   0x01
#define _PTHREAD_CANCEL_TYPE_MASK    0x02
#define _PTHREAD_CANCEL_PENDING	     0x10  /* pthread_cancel() has been called for this thread */

extern boolean_t swtch_pri(int);

#ifndef ESUCCESS
#define ESUCCESS 0
#endif

#ifndef PTHREAD_MACH_CALL
#define	PTHREAD_MACH_CALL(expr, ret) (ret) = (expr)
#endif

/* Prototypes. */

/* Functions defined in machine-dependent files. */
extern vm_address_t _sp(void);
extern vm_address_t _adjust_sp(vm_address_t sp);
extern void _pthread_setup(pthread_t th, void (*f)(pthread_t), void *sp, int suspended, int needresume);

extern void _pthread_tsd_cleanup(pthread_t self);

__private_extern__ semaphore_t new_sem_from_pool(void);
__private_extern__ void restore_sem_to_pool(semaphore_t);
__private_extern__ void _pthread_atfork_queue_init(void);
#endif /* _POSIX_PTHREAD_INTERNALS_H */
