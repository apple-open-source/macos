/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
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
 * POSIX Pthread Library
 */

#include "pthread_internals.h"
#include "pthread_workqueue.h"

#include <assert.h>
#include <stdio.h>	/* For printf(). */
#include <stdlib.h>
#include <errno.h>	/* For __mach_errno_addr() prototype. */
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <machine/vmparam.h>
#include <mach/vm_statistics.h>
#define	__APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>
#include <libkern/OSAtomic.h>
#if defined(__ppc__)
#include <libkern/OSCrossEndian.h>
#endif


#ifndef BUILDING_VARIANT /* [ */

__private_extern__ struct __pthread_list __pthread_head = TAILQ_HEAD_INITIALIZER(__pthread_head);



/* Per-thread kernel support */
extern void _pthread_set_self(pthread_t);
extern void mig_init(int);
static int _pthread_create_pthread_onstack(pthread_attr_t *attrs, void **stack, pthread_t *thread);
static kern_return_t _pthread_free_pthread_onstack(pthread_t t, int freestruct, int termthread);
void _pthread_struct_init(pthread_t t, const pthread_attr_t *attrs, void * stack, size_t stacksize, int kernalloc, int nozero);
static void _pthread_tsd_reinit(pthread_t t);
static int  _new_pthread_create_suspended(pthread_t *thread, 
	       const pthread_attr_t *attr,
	       void *(*start_routine)(void *), 
	       void *arg,
		int create_susp);

/* Get CPU capabilities from the kernel */
__private_extern__ void _init_cpu_capabilities(void);

/* Needed to tell the malloc subsystem we're going multithreaded */
extern void set_malloc_singlethreaded(int);

/* Used when we need to call into the kernel with no reply port */
extern pthread_lock_t reply_port_lock;
int _pthread_find_thread(pthread_t thread);

/* Mach message used to notify that a thread needs to be reaped */

typedef struct _pthread_reap_msg_t {
	mach_msg_header_t header;
	pthread_t thread;
	mach_msg_trailer_t trailer;
} pthread_reap_msg_t;

/* We'll implement this when the main thread is a pthread */
/* Use the local _pthread struct to avoid malloc before our MiG reply port is set */
static struct _pthread _thread = {0};

/* This global should be used (carefully) by anyone needing to know if a 
** pthread has been created.
*/
int __is_threaded = 0;
/* _pthread_count is protected by _pthread_list_lock */
static int _pthread_count = 1;
int __unix_conforming = 0;
__private_extern__ size_t pthreadsize = 0;

/* under rosetta we will use old style creation of threads */
static int __oldstyle = 0;

__private_extern__ pthread_lock_t _pthread_list_lock = LOCK_INITIALIZER;

/* Same implementation as LOCK, but without the __is_threaded check */
int _spin_tries = 0;
__private_extern__ void _spin_lock_retry(pthread_lock_t *lock)
{ 
	int tries = _spin_tries;
	do {
		if (tries-- > 0)
			continue;
		syscall_thread_switch(THREAD_NULL, SWITCH_OPTION_DEPRESS, 1);
		tries = _spin_tries;
	} while(!_spin_lock_try(lock));
}

extern mach_port_t thread_recycle_port;

/* These are used to keep track of a semaphore pool shared by mutexes and condition
** variables.
*/

static semaphore_t *sem_pool = NULL;
static int sem_pool_count = 0;
static int sem_pool_current = 0;
static pthread_lock_t sem_pool_lock = LOCK_INITIALIZER;

static int default_priority;
static int max_priority;
static int min_priority;
static int pthread_concurrency;

static OSSpinLock __workqueue_list_lock = OS_SPINLOCK_INIT;

static void _pthread_exit(pthread_t self, void *value_ptr);
int _pthread_setcancelstate_internal(int state, int *oldstate, int conforming);
static void _pthread_setcancelstate_exit(pthread_t self, void  *value_ptr, int conforming);
static pthread_attr_t _pthread_attr_default = {0};
static void _pthread_workq_init(pthread_workqueue_t wq, const pthread_workqueue_attr_t * attr);
static int handle_removeitem(pthread_workqueue_t workq, pthread_workitem_t item);
static int kernel_workq_setup = 0;
static volatile int32_t kernel_workq_count = 0;
static volatile unsigned int user_workq_count = 0; 
#define KERNEL_WORKQ_ELEM_MAX  	64		/* Max number of elements in the kerrel */
static int wqreadyprio = 0;	/* current highest prio queue ready with items */

__private_extern__ struct __pthread_workitem_pool __pthread_workitem_pool_head = TAILQ_HEAD_INITIALIZER(__pthread_workitem_pool_head);
__private_extern__ struct __pthread_workqueue_pool __pthread_workqueue_pool_head = TAILQ_HEAD_INITIALIZER(__pthread_workqueue_pool_head);

struct _pthread_workqueue_head __pthread_workq0_head;
struct _pthread_workqueue_head __pthread_workq1_head;
struct _pthread_workqueue_head __pthread_workq2_head;
struct _pthread_workqueue_head __pthread_workq3_head;
struct _pthread_workqueue_head __pthread_workq4_head;
pthread_workqueue_head_t __pthread_wq_head_tbl[WQ_NUM_PRIO_QS] = {&__pthread_workq0_head, &__pthread_workq1_head, &__pthread_workq2_head, &__pthread_workq3_head, &__pthread_workq4_head};

static void workqueue_list_lock(void);
static void workqueue_list_unlock(void);
static int valid_workq(pthread_workqueue_t);
static void pick_nextworkqueue_droplock(void);
static int post_nextworkitem(pthread_workqueue_t workq);
static void _pthread_workq_return(pthread_t self);
static pthread_workqueue_attr_t _pthread_wq_attr_default = {0};
void _pthread_wqthread(pthread_t self, mach_port_t kport, void * stackaddr, pthread_workitem_t item, int reuse);
extern void start_wqthread(pthread_t self, mach_port_t kport, void * stackaddr, pthread_workitem_t item, int reuse);
extern void thread_start(pthread_t self, mach_port_t kport, void *(*fun)(void *), void * funarg, size_t stacksize, int flags);
static pthread_workitem_t alloc_workitem(void);
static void free_workitem(pthread_workitem_t);
static pthread_workqueue_t alloc_workqueue(void);
static void free_workqueue(pthread_workqueue_t);
static int _pthread_work_internal_init(void);
static void workqueue_exit(pthread_t self, pthread_workqueue_t workq, pthread_workitem_t item);

/* workq_ops commands */
#define WQOPS_QUEUE_ADD 1
#define WQOPS_QUEUE_REMOVE 2
#define WQOPS_THREAD_RETURN 4

/*
 * Flags filed passed to bsdthread_create and back in pthread_start 
31  <---------------------------------> 0
_________________________________________
| flags(8) | policy(8) | importance(16) |
-----------------------------------------
*/
void _pthread_start(pthread_t self, mach_port_t kport, void *(*fun)(void *), void * funarg, size_t stacksize, unsigned int flags);

#define PTHREAD_START_CUSTOM	0x01000000
#define PTHREAD_START_SETSCHED	0x02000000
#define PTHREAD_START_DETACHED	0x04000000
#define PTHREAD_START_POLICY_BITSHIFT 16
#define PTHREAD_START_POLICY_MASK 0xff
#define PTHREAD_START_IMPORTANCE_MASK 0xffff

static int pthread_setschedparam_internal(pthread_t, mach_port_t, int, const struct sched_param *);
extern pthread_t __bsdthread_create(void (*func)(void *), void * func_arg, void * stack, pthread_t  thread, unsigned int flags);
extern int __bsdthread_terminate(void * freeaddr, size_t freesize, mach_port_t kport, mach_port_t joinsem);

#if defined(__ppc__) || defined(__ppc64__)
static const vm_address_t PTHREAD_STACK_HINT = 0xF0000000;
#elif defined(__i386__) || defined(__x86_64__)
static const vm_address_t PTHREAD_STACK_HINT = 0xB0000000;
#elif defined(__arm__)
static const vm_address_t PTHREAD_STACK_HINT = 0x30000000;
#else
#error Need to define a stack address hint for this architecture
#endif

/* Set the base address to use as the stack pointer, before adjusting due to the ABI
 * The guardpages for stackoverflow protection is also allocated here
 * If the stack was already allocated(stackaddr in attr)  then there are no guardpages 
 * set up for the thread
 */

static int
_pthread_allocate_stack(pthread_attr_t *attrs, void **stack)
{
    kern_return_t kr;
    vm_address_t stackaddr;
    size_t guardsize;

    assert(attrs->stacksize >= PTHREAD_STACK_MIN);
    if (attrs->stackaddr != NULL) {
	/* No guard pages setup in this case */
        assert(((uintptr_t)attrs->stackaddr % vm_page_size) == 0);
        *stack = attrs->stackaddr;
         return 0;
    }

    guardsize = attrs->guardsize;
    stackaddr = PTHREAD_STACK_HINT;
    kr = vm_map(mach_task_self(), &stackaddr,
    			attrs->stacksize + guardsize,
    			vm_page_size-1,
    			VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE , MEMORY_OBJECT_NULL,
    			0, FALSE, VM_PROT_DEFAULT, VM_PROT_ALL,
    			VM_INHERIT_DEFAULT);
    if (kr != KERN_SUCCESS)
    	kr = vm_allocate(mach_task_self(),
    					&stackaddr, attrs->stacksize + guardsize,
    					VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        return EAGAIN;
    }
     /* The guard page is at the lowest address */
     /* The stack base is the highest address */
    if (guardsize)
    	kr = vm_protect(mach_task_self(), stackaddr, guardsize, FALSE, VM_PROT_NONE);
    *stack = (void *)(stackaddr + attrs->stacksize + guardsize);
        return 0;
}

static int
_pthread_create_pthread_onstack(pthread_attr_t *attrs, void **stack, pthread_t *thread)
{
    kern_return_t kr;
	pthread_t t;
    vm_address_t stackaddr;
    size_t guardsize, allocsize;

	assert(attrs->stacksize >= PTHREAD_STACK_MIN);

    if (attrs->stackaddr != NULL) {
	/* No guard pages setup in this case */
		assert(((uintptr_t)attrs->stackaddr % vm_page_size) == 0);
		*stack = attrs->stackaddr;
		t = (pthread_t)malloc(pthreadsize);
		_pthread_struct_init(t, attrs, attrs->stackaddr, 0, 0, 0);
		t->freeStackOnExit = 0;
		t->freeaddr = 0;
		t->freesize = 0;
		*thread = t;
 		return 0;
	}

    guardsize = attrs->guardsize;
	allocsize = attrs->stacksize + guardsize + pthreadsize;
    stackaddr = PTHREAD_STACK_HINT;
    kr = vm_map(mach_task_self(), &stackaddr,
    			allocsize,
    			vm_page_size-1,
    			VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE , MEMORY_OBJECT_NULL,
    			0, FALSE, VM_PROT_DEFAULT, VM_PROT_ALL,
    			VM_INHERIT_DEFAULT);
    if (kr != KERN_SUCCESS)
    	kr = vm_allocate(mach_task_self(),
    					&stackaddr, allocsize,
    					VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        return EAGAIN;
    }
     /* The guard page is at the lowest address */
     /* The stack base is the highest address */
    if (guardsize)
    	kr = vm_protect(mach_task_self(), stackaddr, guardsize, FALSE, VM_PROT_NONE);


	*stack = (void *)(stackaddr + attrs->stacksize + guardsize);

	t = (pthread_t)(stackaddr + attrs->stacksize + guardsize);
	_pthread_struct_init(t, attrs, *stack, 0, 0, 1);
	t->kernalloc = 0;
	t->freesize = allocsize;
	t->freeaddr = (void *)stackaddr;
	t->freeStackOnExit = 1;
	*thread = t;

    return 0;
}

static kern_return_t
_pthread_free_pthread_onstack(pthread_t t, int freestruct, int termthread)
{
	kern_return_t res = 0;
    	vm_address_t freeaddr;
    	size_t freesize;
	task_t self = mach_task_self();
	int thread_count;
	mach_port_t kport;
	semaphore_t joinsem = SEMAPHORE_NULL;

#if WQ_TRACE
	__kdebug_trace(0x900001c, freestruct, termthread, 0, 0, 0);
#endif
	kport = t->kernel_thread;
	joinsem = t->joiner_notify;

	if (t->freeStackOnExit) {
		freeaddr = (vm_address_t)t->freeaddr;
		if (freestruct)
			freesize = t->stacksize + t->guardsize + pthreadsize;
		else
			freesize = t->stacksize + t->guardsize;
		if (termthread) {
			mig_dealloc_reply_port(MACH_PORT_NULL);
			LOCK(_pthread_list_lock);
			if (freestruct != 0) {
				TAILQ_REMOVE(&__pthread_head, t, plist);
				/* if parent has not returned from create yet keep pthread_t */
#if WQ_TRACE
				__kdebug_trace(0x9000010, t, 0, 0, 1, 0);
#endif
				if (t->parentcheck == 0)
					freesize -= pthreadsize;
			}
			t->childexit = 1;
			thread_count = --_pthread_count;
			UNLOCK(_pthread_list_lock);

#if WQ_TRACE
		__kdebug_trace(0x9000020, freeaddr, freesize, kport, 1, 0);
#endif
			if (thread_count <=0)
				exit(0);
			else
				__bsdthread_terminate(freeaddr, freesize, kport, joinsem);
			abort();
		} else  {
#if WQ_TRACE
			__kdebug_trace(0x9000024, freeaddr, freesize, 0, 1, 0);
#endif
			res = vm_deallocate(mach_task_self(), freeaddr, freesize);
		}
	} else {
		if (termthread) {
			mig_dealloc_reply_port(MACH_PORT_NULL);
			LOCK(_pthread_list_lock);
			if (freestruct != 0) {
				TAILQ_REMOVE(&__pthread_head, t, plist);
#if WQ_TRACE
				__kdebug_trace(0x9000010, t, 0, 0, 2, 0);
#endif
			}
			thread_count = --_pthread_count;
			t->childexit = 1;
			UNLOCK(_pthread_list_lock);

			if (freestruct)  {
#if WQ_TRACE
		__kdebug_trace(0x9000008, t, 0, 0, 2, 0);
#endif
				free(t);
			}

			freeaddr = 0;
			freesize = 0;
#if WQ_TRACE
		__kdebug_trace(0x9000020, 0, 0, kport, 2, 0);
#endif

			if (thread_count <=0)
				exit(0);
			else
				__bsdthread_terminate(NULL, 0, kport, joinsem);
			abort();
		} else if (freestruct) {
		       t->sig = _PTHREAD_NO_SIG;
#if WQ_TRACE
			__kdebug_trace(0x9000024, t, 0, 0, 2, 0);
#endif
			free(t);
		}
	}
	return(res);
} 



/*
 * Destroy a thread attribute structure
 */
int       
pthread_attr_destroy(pthread_attr_t *attr)
{
	if (attr->sig == _PTHREAD_ATTR_SIG)
	{
		attr->sig = 0;
		return (0);
	} else
	{
		return (EINVAL); /* Not an attribute structure! */
	}
}

/*
 * Get the 'detach' state from a thread attribute structure.
 * Note: written as a helper function for info hiding
 */
int       
pthread_attr_getdetachstate(const pthread_attr_t *attr, 
			    int *detachstate)
{
	if (attr->sig == _PTHREAD_ATTR_SIG)
	{
		*detachstate = attr->detached;
		return (0);
	} else
	{
		return (EINVAL); /* Not an attribute structure! */
	}
}

/*
 * Get the 'inherit scheduling' info from a thread attribute structure.
 * Note: written as a helper function for info hiding
 */
int       
pthread_attr_getinheritsched(const pthread_attr_t *attr, 
			     int *inheritsched)
{
	if (attr->sig == _PTHREAD_ATTR_SIG)
	{
		*inheritsched = attr->inherit;
		return (0);
	} else
	{
		return (EINVAL); /* Not an attribute structure! */
	}
}

/*
 * Get the scheduling parameters from a thread attribute structure.
 * Note: written as a helper function for info hiding
 */
int       
pthread_attr_getschedparam(const pthread_attr_t *attr, 
			   struct sched_param *param)
{
	if (attr->sig == _PTHREAD_ATTR_SIG)
	{
		*param = attr->param;
		return (0);
	} else
	{
		return (EINVAL); /* Not an attribute structure! */
	}
}

/*
 * Get the scheduling policy from a thread attribute structure.
 * Note: written as a helper function for info hiding
 */
int       
pthread_attr_getschedpolicy(const pthread_attr_t *attr, 
			    int *policy)
{
	if (attr->sig == _PTHREAD_ATTR_SIG)
	{
		*policy = attr->policy;
		return (0);
	} else
	{
		return (EINVAL); /* Not an attribute structure! */
	}
}

/* Retain the existing stack size of 512K and not depend on Main thread default stack size */
static const size_t DEFAULT_STACK_SIZE = (512*1024);
/*
 * Initialize a thread attribute structure to default values.
 */
int       
pthread_attr_init(pthread_attr_t *attr)
{
	attr->stacksize = DEFAULT_STACK_SIZE;
	attr->stackaddr = NULL;
	attr->sig = _PTHREAD_ATTR_SIG;
	attr->param.sched_priority = default_priority;
	attr->param.quantum = 10; /* quantum isn't public yet */
	attr->detached = PTHREAD_CREATE_JOINABLE;
	attr->inherit = _PTHREAD_DEFAULT_INHERITSCHED;
	attr->policy = _PTHREAD_DEFAULT_POLICY;
	attr->freeStackOnExit = 1;
	attr->fastpath = 1;
	attr->schedset = 0;
	attr->guardsize = vm_page_size;
	return (0);
}

/*
 * Set the 'detach' state in a thread attribute structure.
 * Note: written as a helper function for info hiding
 */
int       
pthread_attr_setdetachstate(pthread_attr_t *attr, 
			    int detachstate)
{
	if (attr->sig == _PTHREAD_ATTR_SIG)
	{
		if ((detachstate == PTHREAD_CREATE_JOINABLE) ||
		    (detachstate == PTHREAD_CREATE_DETACHED))
		{
			attr->detached = detachstate;
			return (0);
		} else
		{
			return (EINVAL);
		}
	} else
	{
		return (EINVAL); /* Not an attribute structure! */
	}
}

/*
 * Set the 'inherit scheduling' state in a thread attribute structure.
 * Note: written as a helper function for info hiding
 */
int       
pthread_attr_setinheritsched(pthread_attr_t *attr, 
			     int inheritsched)
{
	if (attr->sig == _PTHREAD_ATTR_SIG)
	{
		if ((inheritsched == PTHREAD_INHERIT_SCHED) ||
		    (inheritsched == PTHREAD_EXPLICIT_SCHED))
		{
			attr->inherit = inheritsched;
			return (0);
		} else
		{
			return (EINVAL);
		}
	} else
	{
		return (EINVAL); /* Not an attribute structure! */
	}
}

/*
 * Set the scheduling paramters in a thread attribute structure.
 * Note: written as a helper function for info hiding
 */
int       
pthread_attr_setschedparam(pthread_attr_t *attr, 
			   const struct sched_param *param)
{
	if (attr->sig == _PTHREAD_ATTR_SIG)
	{
		/* TODO: Validate sched_param fields */
		attr->param = *param;
		attr->schedset = 1;
		return (0);
	} else
	{
		return (EINVAL); /* Not an attribute structure! */
	}
}

/*
 * Set the scheduling policy in a thread attribute structure.
 * Note: written as a helper function for info hiding
 */
int       
pthread_attr_setschedpolicy(pthread_attr_t *attr, 
			    int policy)
{
	if (attr->sig == _PTHREAD_ATTR_SIG)
	{
		if ((policy == SCHED_OTHER) ||
		    (policy == SCHED_RR) ||
		    (policy == SCHED_FIFO))
		{
			attr->policy = policy;
			attr->schedset = 1;
			return (0);
		} else
		{
			return (EINVAL);
		}
	} else
	{
		return (EINVAL); /* Not an attribute structure! */
	}
}

/*
 * Set the scope for the thread.
 * We currently only provide PTHREAD_SCOPE_SYSTEM
 */
int
pthread_attr_setscope(pthread_attr_t *attr,
                            int scope)
{
    if (attr->sig == _PTHREAD_ATTR_SIG) {
        if (scope == PTHREAD_SCOPE_SYSTEM) {
            /* No attribute yet for the scope */
            return (0);
        } else if (scope == PTHREAD_SCOPE_PROCESS) {
            return (ENOTSUP);
        }
    }
    return (EINVAL); /* Not an attribute structure! */
}

/*
 * Get the scope for the thread.
 * We currently only provide PTHREAD_SCOPE_SYSTEM
 */
int
pthread_attr_getscope(const pthread_attr_t *attr,
                            int *scope)
{
    if (attr->sig == _PTHREAD_ATTR_SIG) {
        *scope = PTHREAD_SCOPE_SYSTEM;
        return (0);
    }
    return (EINVAL); /* Not an attribute structure! */
}

/* Get the base stack address of the given thread */
int
pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
    if (attr->sig == _PTHREAD_ATTR_SIG) {
        *stackaddr = attr->stackaddr;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int
pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
    if ((attr->sig == _PTHREAD_ATTR_SIG) && (((uintptr_t)stackaddr % vm_page_size) == 0)) {
        attr->stackaddr = stackaddr;
        attr->freeStackOnExit = 0;
	attr->fastpath = 0;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int
pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    if (attr->sig == _PTHREAD_ATTR_SIG) {
        *stacksize = attr->stacksize;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int
pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    if ((attr->sig == _PTHREAD_ATTR_SIG) && ((stacksize % vm_page_size) == 0) && (stacksize >= PTHREAD_STACK_MIN)) {
        attr->stacksize = stacksize;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int
pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t * stacksize)
{
    if (attr->sig == _PTHREAD_ATTR_SIG) {
	*stackaddr = (void *)((uintptr_t)attr->stackaddr - attr->stacksize);
        *stacksize = attr->stacksize;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

/* By SUSV spec, the stackaddr is the base address, the lowest addressable 
 * byte address. This is not the same as in pthread_attr_setstackaddr.
 */
int
pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize)
{
    if ((attr->sig == _PTHREAD_ATTR_SIG) && 
	(((uintptr_t)stackaddr % vm_page_size) == 0) && 
	 ((stacksize % vm_page_size) == 0) && (stacksize >= PTHREAD_STACK_MIN)) {
		attr->stackaddr = (void *)((uintptr_t)stackaddr + stacksize);
        	attr->stacksize = stacksize;
        	attr->freeStackOnExit = 0;
		attr->fastpath = 0;
        	return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}


/*
 * Set the guardsize attribute in the attr.
 */
int
pthread_attr_setguardsize(pthread_attr_t *attr,
                            size_t guardsize)
{
    if (attr->sig == _PTHREAD_ATTR_SIG) {
	/* Guardsize of 0 is valid, ot means no guard */
        if ((guardsize % vm_page_size) == 0) {
		attr->guardsize = guardsize;
		attr->fastpath = 0;
            return (0);
	} else
		return(EINVAL);
    }
    return (EINVAL); /* Not an attribute structure! */
}

/*
 * Get the guardsize attribute in the attr.
 */
int
pthread_attr_getguardsize(const pthread_attr_t *attr,
                            size_t *guardsize)
{
    if (attr->sig == _PTHREAD_ATTR_SIG) {
        *guardsize = attr->guardsize;
        return (0);
    }
    return (EINVAL); /* Not an attribute structure! */
}


/*
 * Create and start execution of a new thread.
 */

static void
_pthread_body(pthread_t self)
{
    _pthread_set_self(self);
    _pthread_exit(self, (self->fun)(self->arg));
}

void
_pthread_start(pthread_t self, mach_port_t kport, void *(*fun)(void *), void * funarg, size_t stacksize, unsigned int pflags)
{
	int ret;
#if WQ_DEBUG
	pthread_t pself;
#endif
	pthread_attr_t *attrs = &_pthread_attr_default;
	char * stackaddr;

	if ((pflags & PTHREAD_START_CUSTOM) == 0) {
		stackaddr = self;
		_pthread_struct_init(self, attrs, stackaddr,  stacksize, 1, 1);
		LOCK(_pthread_list_lock);
		if (pflags & PTHREAD_START_SETSCHED) {
			self->policy = ((pflags >> PTHREAD_START_POLICY_BITSHIFT) & PTHREAD_START_POLICY_MASK);
			self->param.sched_priority = (pflags & PTHREAD_START_IMPORTANCE_MASK);
		}
		/* These are not joinable threads */
		if ((pflags & PTHREAD_START_DETACHED) == PTHREAD_START_DETACHED)  {
			self->detached &= ~PTHREAD_CREATE_JOINABLE;
			self->detached |= PTHREAD_CREATE_DETACHED;
		}
	}  else 
		LOCK(_pthread_list_lock);
	self->kernel_thread = kport;
	self->fun = fun;
	self->arg = funarg;
	
	/* Add to the pthread list */
	if (self->parentcheck == 0) {
		TAILQ_INSERT_TAIL(&__pthread_head, self, plist);
#if WQ_TRACE
		__kdebug_trace(0x900000c, self, 0, 0, 3, 0);
#endif
		_pthread_count++;
	}
	self->childrun = 1;
	UNLOCK(_pthread_list_lock);
#if defined(__i386__) || defined(__x86_64__)
	_pthread_set_self(self);
#endif

#if WQ_DEBUG
	pself = pthread_self();
	if (self != pself)
		abort();
#endif
#if WQ_TRACE
	__kdebug_trace(0x9000030, self, pflags, 0, 0, 0);
#endif

	_pthread_exit(self, (self->fun)(self->arg));
}

int
_pthread_create(pthread_t t,
		const pthread_attr_t *attrs,
                void *stack,
		const mach_port_t kernel_thread)
{
	int res;
	res = 0;
	
	do
	{
		memset(t, 0, sizeof(*t));
		t->newstyle = 0;
		t->schedset = 0;
		t->kernalloc = 0;
		t->tsd[0] = t;
		t->max_tsd_key = 0;
		t->wqthread = 0;
		t->cur_workq = 0;
		t->cur_workitem = 0;
		t->stacksize = attrs->stacksize;
                t->stackaddr = (void *)stack;
		t->guardsize = attrs->guardsize;
                t->kernel_thread = kernel_thread;
		t->detached = attrs->detached;
		t->inherit = attrs->inherit;
		t->policy = attrs->policy;
		t->param = attrs->param;
		t->freeStackOnExit = attrs->freeStackOnExit;
		t->mutexes = (struct _pthread_mutex *)NULL;
		t->sig = _PTHREAD_SIG;
                t->reply_port = MACH_PORT_NULL;
                t->cthread_self = NULL;
		LOCK_INIT(t->lock);
		t->plist.tqe_next = (struct _pthread *)0;
		t->plist.tqe_prev = (struct _pthread **)0;
		t->cancel_state = PTHREAD_CANCEL_ENABLE | PTHREAD_CANCEL_DEFERRED;
		t->__cleanup_stack = (struct __darwin_pthread_handler_rec *)NULL;
		t->death = SEMAPHORE_NULL;

		if (kernel_thread != MACH_PORT_NULL)
			(void)pthread_setschedparam_internal(t, kernel_thread, t->policy, &t->param);
	} while (0);
	return (res);
}

void
_pthread_struct_init(pthread_t t, const pthread_attr_t *attrs, void * stack, size_t stacksize, int kernalloc, int nozero)
{
	mach_vm_offset_t stackaddr = (mach_vm_offset_t)stack;

		if (nozero == 0) {
			memset(t, 0, sizeof(*t));
			t->plist.tqe_next = (struct _pthread *)0;
			t->plist.tqe_prev = (struct _pthread **)0;
		}
		t->schedset = attrs->schedset;
		t->tsd[0] = t;
		if (kernalloc != 0) {
			stackaddr = (mach_vm_offset_t)t;

			/* if allocated from kernel set values appropriately */
			t->stacksize = stacksize;
       			t->stackaddr = stackaddr;
			t->freeStackOnExit = 1;
			t->freeaddr = stackaddr - stacksize - vm_page_size;
			t->freesize = pthreadsize + stacksize + vm_page_size;
		} else {
			t->stacksize = attrs->stacksize;
			t->stackaddr = (void *)stack;
		}
		t->guardsize = attrs->guardsize;
		t->detached = attrs->detached;
		t->inherit = attrs->inherit;
		t->policy = attrs->policy;
		t->param = attrs->param;
		t->mutexes = (struct _pthread_mutex *)NULL;
		t->sig = _PTHREAD_SIG;
		t->reply_port = MACH_PORT_NULL;
		t->cthread_self = NULL;
		LOCK_INIT(t->lock);
		t->cancel_state = PTHREAD_CANCEL_ENABLE | PTHREAD_CANCEL_DEFERRED;
		t->__cleanup_stack = (struct __darwin_pthread_handler_rec *)NULL;
		t->death = SEMAPHORE_NULL;
		t->newstyle = 1;
		t->kernalloc = kernalloc;
		t->wqthread = 0;
		t->cur_workq = 0;
		t->cur_workitem = 0;
		t->max_tsd_key = 0;
}

static void
_pthread_tsd_reinit(pthread_t t)
{
	bzero(&t->tsd[1], (_INTERNAL_POSIX_THREAD_KEYS_END-1) * sizeof(void *));
}


/* Need to deprecate this in future */
int
_pthread_is_threaded(void)
{
    return __is_threaded;
}

/* Non portable public api to know whether this process has(had) atleast one thread 
 * apart from main thread. There could be race if there is a thread in the process of
 * creation at the time of call . It does not tell whether there are more than one thread
 * at this point of time.
 */
int
pthread_is_threaded_np(void)
{
    return (__is_threaded);
}

mach_port_t
pthread_mach_thread_np(pthread_t t)
{
	mach_port_t kport = MACH_PORT_NULL;

	if (_pthread_lookup_thread(t, &kport, 0) != 0)
		return(NULL);

	return(kport);
}

pthread_t pthread_from_mach_thread_np(mach_port_t kernel_thread) 
{
    struct _pthread * p = NULL;

	/* No need to wait as mach port is already known */
    LOCK(_pthread_list_lock);
    TAILQ_FOREACH(p, &__pthread_head, plist) {
        if (p->kernel_thread == kernel_thread) 
			break;
    }
    UNLOCK(_pthread_list_lock);     
    return p;
}

size_t
pthread_get_stacksize_np(pthread_t t)
{
	int ret;
	size_t size = 0;

	if (t == NULL)
		return(ESRCH);
	
	LOCK(_pthread_list_lock);

	if ((ret = _pthread_find_thread(t)) != 0) {
		UNLOCK(_pthread_list_lock);
		return(ret);
	}
	size = t->stacksize;
	UNLOCK(_pthread_list_lock);
	return(size);
}

void *
pthread_get_stackaddr_np(pthread_t t)
{
	int ret;
	void * addr = NULL;

	if (t == NULL)
		return(ESRCH);
	
	LOCK(_pthread_list_lock);

	if ((ret = _pthread_find_thread(t)) != 0) {
		UNLOCK(_pthread_list_lock);
		return(ret);
	}
	addr = t->stackaddr;
	UNLOCK(_pthread_list_lock);

	return(addr);
}

mach_port_t
_pthread_reply_port(pthread_t t)
{
    return t->reply_port;
}


/* returns non-zero if the current thread is the main thread */
int
pthread_main_np(void)
{
	pthread_t self = pthread_self();

	return ((self->detached & _PTHREAD_CREATE_PARENT) == _PTHREAD_CREATE_PARENT);
}

static int       
_new_pthread_create_suspended(pthread_t *thread, 
	       const pthread_attr_t *attr,
	       void *(*start_routine)(void *), 
	       void *arg,
		int create_susp)
{
	pthread_attr_t *attrs;
	void *stack;
	int error;
	unsigned int flags;
	pthread_t t;
	kern_return_t kern_res;
	mach_port_t kernel_thread = MACH_PORT_NULL;
	int needresume;
	task_t self = mach_task_self();
	int kernalloc = 0;
	int susp = create_susp;

	if ((attrs = (pthread_attr_t *)attr) == (pthread_attr_t *)NULL)
	{			/* Set up default paramters */
		attrs = &_pthread_attr_default;
	} else if (attrs->sig != _PTHREAD_ATTR_SIG) {
            return EINVAL;
	}
	error = 0;

	if (((attrs->policy != _PTHREAD_DEFAULT_POLICY)  || 
		(attrs->param.sched_priority != default_priority)) && (create_susp == 0)) {
		needresume = 1;	
		susp = 1;	
	} else 
		needresume = 0;

	/* In default policy (ie SCHED_OTHER) only sched_priority is used. Check for
	 * any change in priority or policy is needed here.
	 */
	if ((__oldstyle == 1) || (create_susp != 0)) {
		/* Rosetta or pthread_create_suspended() */
		/* running under rosetta */
		/* Allocate a stack for the thread */
#if WQ_TRACE
		__kdebug_trace(0x9000000, create_susp, 0, 0, 0, 0);
#endif
                if ((error = _pthread_allocate_stack(attrs, &stack)) != 0) {
			return(error);
                }
		t = (pthread_t)malloc(sizeof(struct _pthread));
		*thread = t;
		if (susp) {
			/* Create the Mach thread for this thread */
			PTHREAD_MACH_CALL(thread_create(self, &kernel_thread), kern_res);
			if (kern_res != KERN_SUCCESS)
			{
				printf("Can't create thread: %d\n", kern_res);
				return(EINVAL);
			}
		}
                if ((error = _pthread_create(t, attrs, stack, kernel_thread)) != 0)
		{
			return(error);
		}
		set_malloc_singlethreaded(0);
		__is_threaded = 1;

		/* Send it on it's way */
		t->arg = arg;
		t->fun = start_routine;
		t->newstyle = 0;
		/* Now set it up to execute */
		LOCK(_pthread_list_lock);
		TAILQ_INSERT_TAIL(&__pthread_head, t, plist);
#if WQ_TRACE
		__kdebug_trace(0x900000c, t, 0, 0, 4, 0);
#endif
		_pthread_count++;
		UNLOCK(_pthread_list_lock);
		_pthread_setup(t, _pthread_body, stack, susp, needresume);
		return(0);
	} else {

		flags = 0;
		if (attrs->fastpath == 1)
			kernalloc = 1;

		if (attrs->detached == PTHREAD_CREATE_DETACHED)
				flags |= PTHREAD_START_DETACHED;
		if (attrs->schedset != 0) {
			flags |= PTHREAD_START_SETSCHED;
			flags |= ((attrs->policy & PTHREAD_START_POLICY_MASK) << PTHREAD_START_POLICY_BITSHIFT);
			flags |= (attrs->param.sched_priority & PTHREAD_START_IMPORTANCE_MASK);
		} 

		set_malloc_singlethreaded(0);
		__is_threaded = 1;

		if (kernalloc == 0) {
			/* Allocate a stack for the thread */
			flags |= PTHREAD_START_CUSTOM;
			if ((error = _pthread_create_pthread_onstack(attrs, &stack, &t)) != 0) {
				return(error);
			}
			/* Send it on it's way */
			t->arg = arg;
			t->fun = start_routine;
			t->newstyle = 1;

#if WQ_TRACE
		__kdebug_trace(0x9000004, t, flags, 0, 0, 0);
#endif
			
			if ((t = __bsdthread_create(start_routine, arg, stack, t, flags)) == -1) {
				_pthread_free_pthread_onstack(t, 1, 0);
				return (EAGAIN);
			}
			LOCK(_pthread_list_lock);
			t->parentcheck = 1;
			if ((t->childexit != 0) && ((t->detached & PTHREAD_CREATE_DETACHED) == PTHREAD_CREATE_DETACHED)) {
				/* detached child exited, mop up */
				UNLOCK(_pthread_list_lock);
#if WQ_TRACE
			__kdebug_trace(0x9000008, t, 0, 0, 1, 0);
#endif
				free(t);
			} else if (t->childrun == 0) {
				TAILQ_INSERT_TAIL(&__pthread_head, t, plist);
				_pthread_count++;
#if WQ_TRACE
				__kdebug_trace(0x900000c, t, 0, 0, 1, 0);
#endif
				UNLOCK(_pthread_list_lock);
			} else 
				UNLOCK(_pthread_list_lock);

			*thread = t;

#if WQ_TRACE
		__kdebug_trace(0x9000014, t, 0, 0, 1, 0);
#endif
			return (0);

		} else {
			/* kernel allocation */
#if WQ_TRACE
		__kdebug_trace(0x9000018, flags, 0, 0, 0, 0);
#endif
			if ((t = __bsdthread_create(start_routine, arg, attrs->stacksize, NULL, flags)) == -1)
				return (EAGAIN);
			/* Now set it up to execute */
			LOCK(_pthread_list_lock);
			t->parentcheck = 1;
			if ((t->childexit != 0) && ((t->detached & PTHREAD_CREATE_DETACHED) == PTHREAD_CREATE_DETACHED)) {
				/* detached child exited, mop up */
				UNLOCK(_pthread_list_lock);
#if WQ_TRACE
			__kdebug_trace(0x9000008, t, pthreadsize, 0, 2, 0);
#endif
				vm_deallocate(self, t, pthreadsize);
			} else if (t->childrun == 0) {
				TAILQ_INSERT_TAIL(&__pthread_head, t, plist);
				_pthread_count++;
#if WQ_TRACE
				__kdebug_trace(0x900000c, t, 0, 0, 2, 0);
#endif
				UNLOCK(_pthread_list_lock);
			} else 
				UNLOCK(_pthread_list_lock);

			*thread = t;

#if WQ_TRACE
			__kdebug_trace(0x9000014, t, 0, 0, 2, 0);
#endif
			return(0);
		}
	}
}

static int       
_pthread_create_suspended(pthread_t *thread, 
	       const pthread_attr_t *attr,
	       void *(*start_routine)(void *), 
	       void *arg,
           int suspended)
{
	pthread_attr_t *attrs;
	void *stack;
	int res;
	pthread_t t;
	kern_return_t kern_res;
	mach_port_t kernel_thread = MACH_PORT_NULL;
	int needresume;

	if ((attrs = (pthread_attr_t *)attr) == (pthread_attr_t *)NULL)
	{			/* Set up default paramters */
		attrs = &_pthread_attr_default;
	} else if (attrs->sig != _PTHREAD_ATTR_SIG) {
            return EINVAL;
	}
	res = 0;

	/* In default policy (ie SCHED_OTHER) only sched_priority is used. Check for
	 * any change in priority or policy is needed here.
	 */
	if (((attrs->policy != _PTHREAD_DEFAULT_POLICY)  || 
		(attrs->param.sched_priority != default_priority)) && (suspended == 0)) {
		needresume = 1;	
		suspended = 1;	
	} else 
		needresume = 0;

	do
	{
		/* Allocate a stack for the thread */
                if ((res = _pthread_allocate_stack(attrs, &stack)) != 0) {
                    break;
                }
		t = (pthread_t)malloc(sizeof(struct _pthread));
		*thread = t;
		if (suspended) {
			/* Create the Mach thread for this thread */
			PTHREAD_MACH_CALL(thread_create(mach_task_self(), &kernel_thread), kern_res);
			if (kern_res != KERN_SUCCESS)
			{
				printf("Can't create thread: %d\n", kern_res);
				res = EINVAL; /* Need better error here? */
				break;
			}
		}
                if ((res = _pthread_create(t, attrs, stack, kernel_thread)) != 0)
		{
			break;
		}
		set_malloc_singlethreaded(0);
		__is_threaded = 1;

		/* Send it on it's way */
		t->arg = arg;
		t->fun = start_routine;
		/* Now set it up to execute */
		LOCK(_pthread_list_lock);
		TAILQ_INSERT_TAIL(&__pthread_head, t, plist);
#if WQ_TRACE
		__kdebug_trace(0x900000c, t, 0, 0, 5, 0);
#endif
		_pthread_count++;
		UNLOCK(_pthread_list_lock);
		_pthread_setup(t, _pthread_body, stack, suspended, needresume);
	} while (0);
	return (res);
}

int
pthread_create(pthread_t *thread,
           const pthread_attr_t *attr,
           void *(*start_routine)(void *),
           void *arg)
{
    return _new_pthread_create_suspended(thread, attr, start_routine, arg, 0);
}

int
pthread_create_suspended_np(pthread_t *thread,
           const pthread_attr_t *attr,
           void *(*start_routine)(void *),
           void *arg)
{
    return _pthread_create_suspended(thread, attr, start_routine, arg, 1);
}

/*
 * Make a thread 'undetached' - no longer 'joinable' with other threads.
 */
int       
pthread_detach(pthread_t thread)
{
	int newstyle = 0;
	int ret;

	if ((ret = _pthread_lookup_thread(thread, NULL, 1)) != 0)
		return (ret); /* Not a valid thread */
		
	LOCK(thread->lock);
	newstyle = thread->newstyle;
	if (thread->detached & PTHREAD_CREATE_JOINABLE)
	{
		if (thread->detached & _PTHREAD_EXITED) {
			UNLOCK(thread->lock);
			pthread_join(thread, NULL);
			return 0;
		} else {
			if (newstyle == 0) {
				semaphore_t death = thread->death;

				thread->detached &= ~PTHREAD_CREATE_JOINABLE;
				thread->detached |= PTHREAD_CREATE_DETACHED;
				UNLOCK(thread->lock);
				if (death)
					(void) semaphore_signal(death);
			} else {
				mach_port_t joinport = thread->joiner_notify;
	
				thread->detached &= ~PTHREAD_CREATE_JOINABLE;
				thread->detached |= PTHREAD_CREATE_DETACHED;
				
				UNLOCK(thread->lock);
				if (joinport) {
					semaphore_signal(joinport);
				}
			}
			return(0);
		}
	} else {
		UNLOCK(thread->lock);
		return (EINVAL);
	}
}


/* 
 * pthread_kill call to system call
 */

extern int __pthread_kill(mach_port_t, int);

int   
pthread_kill (
        pthread_t th,
        int sig)
{
	int error = 0;
	mach_port_t kport = MACH_PORT_NULL;
	
	if ((sig < 0) || (sig > NSIG))
		return(EINVAL);

	if (_pthread_lookup_thread(th, &kport, 0) != 0)
		return (ESRCH); /* Not a valid thread */

	error = __pthread_kill(kport, sig);

	if (error == -1)
		error = errno;
	return(error);
}

/* Announce that there are pthread resources ready to be reclaimed in a */
/* subsequent pthread_exit or reaped by pthread_join. In either case, the Mach */
/* thread underneath is terminated right away. */
static
void _pthread_become_available(pthread_t thread, mach_port_t kernel_thread) {
	pthread_reap_msg_t msg;
	kern_return_t ret;

	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND,
					      MACH_MSG_TYPE_MOVE_SEND);
	msg.header.msgh_size = sizeof msg - sizeof msg.trailer;
	msg.header.msgh_remote_port = thread_recycle_port;
	msg.header.msgh_local_port = kernel_thread; 
	msg.header.msgh_id = 0x44454144; /* 'DEAD' */
	msg.thread = thread;
	ret = mach_msg_send(&msg.header);
	assert(ret == MACH_MSG_SUCCESS);
}

/* Reap the resources for available threads */
__private_extern__
int _pthread_reap_thread(pthread_t th, mach_port_t kernel_thread, void **value_ptr, int conforming) {
	mach_port_type_t ptype;
	kern_return_t ret;
	task_t self;

	self = mach_task_self();
	if (kernel_thread != MACH_PORT_DEAD) {
		ret = mach_port_type(self, kernel_thread, &ptype);
		if (ret == KERN_SUCCESS && ptype != MACH_PORT_TYPE_DEAD_NAME) {
			/* not quite dead yet... */
			return EAGAIN;
		}
		ret = mach_port_deallocate(self, kernel_thread);
		if (ret != KERN_SUCCESS) {
			fprintf(stderr,
				  "mach_port_deallocate(kernel_thread) failed: %s\n",
				  mach_error_string(ret));
		}
	}

	if (th->reply_port != MACH_PORT_NULL) {
		ret = mach_port_mod_refs(self, th->reply_port,
						 MACH_PORT_RIGHT_RECEIVE, -1);
		if (ret != KERN_SUCCESS) {
			fprintf(stderr,
				  "mach_port_mod_refs(reply_port) failed: %s\n",
				  mach_error_string(ret));
		}
	}

	if (th->freeStackOnExit) {
		vm_address_t addr = (vm_address_t)th->stackaddr;
		vm_size_t size;

		size = (vm_size_t)th->stacksize + th->guardsize;
		
		addr -= size;
		ret = vm_deallocate(self, addr, size);
		if (ret != KERN_SUCCESS) {
		  fprintf(stderr,
				    "vm_deallocate(stack) failed: %s\n",
				    mach_error_string(ret));
		}
	}


	if (value_ptr) 
		*value_ptr = th->exit_value;
	if (conforming) {
		if ((th->cancel_state & (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING)) ==
            (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING)) 
			*value_ptr = PTHREAD_CANCELED;
		th->sig = _PTHREAD_NO_SIG;
	}


	if (th != &_thread)
		free(th);

	return 0;
}

static
void _pthread_reap_threads(void)
{
	pthread_reap_msg_t msg;
	kern_return_t ret;

	ret = mach_msg(&msg.header, MACH_RCV_MSG|MACH_RCV_TIMEOUT, 0,
			sizeof msg, thread_recycle_port, 
			MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	while (ret == MACH_MSG_SUCCESS) {
		mach_port_t kernel_thread = msg.header.msgh_remote_port;
		pthread_t thread = msg.thread;

		if (_pthread_reap_thread(thread, kernel_thread, (void **)0, 0) == EAGAIN)
		{
			/* not dead yet, put it back for someone else to reap, stop here */
			_pthread_become_available(thread, kernel_thread);
			return;
		}
		ret = mach_msg(&msg.header, MACH_RCV_MSG|MACH_RCV_TIMEOUT, 0,
				sizeof msg, thread_recycle_port, 
				MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	}
}

/* For compatibility... */

pthread_t
_pthread_self() {
	return pthread_self();
}

/*
 * Terminate a thread.
 */
int __disable_threadsignal(int);

static void 
_pthread_exit(pthread_t self, void *value_ptr)
{
	struct __darwin_pthread_handler_rec *handler;
	kern_return_t kern_res;
	int thread_count;
	int newstyle = self->newstyle;

	/* Make this thread not to receive any signals */
	__disable_threadsignal(1);

#if WQ_TRACE
	__kdebug_trace(0x900001c, self, newstyle, 0, 0, 0);
#endif

	/* set cancel state to disable and type to deferred */
	_pthread_setcancelstate_exit(self, value_ptr, __unix_conforming);

	while ((handler = self->__cleanup_stack) != 0)
	{
		(handler->__routine)(handler->__arg);
		self->__cleanup_stack = handler->__next;
	}
	_pthread_tsd_cleanup(self);

	if (newstyle == 0) {
		_pthread_reap_threads();

		LOCK(self->lock);
		self->detached |= _PTHREAD_EXITED;

		if (self->detached & PTHREAD_CREATE_JOINABLE) {
			mach_port_t death = self->death;
			self->exit_value = value_ptr;
			UNLOCK(self->lock);
			/* the joiner will need a kernel thread reference, leave ours for it */
			if (death) {
				PTHREAD_MACH_CALL(semaphore_signal(death), kern_res);
				if (kern_res != KERN_SUCCESS)
					fprintf(stderr,
						"semaphore_signal(death) failed: %s\n",
						mach_error_string(kern_res));
			}
			LOCK(_pthread_list_lock);
			thread_count = --_pthread_count;
			UNLOCK(_pthread_list_lock);
		} else {
			UNLOCK(self->lock);
			LOCK(_pthread_list_lock);
			TAILQ_REMOVE(&__pthread_head, self, plist);
#if WQ_TRACE
			__kdebug_trace(0x9000010, self, 0, 0, 5, 0);
#endif
			thread_count = --_pthread_count;
			UNLOCK(_pthread_list_lock);
			/* with no joiner, we let become available consume our cached ref */
			_pthread_become_available(self, self->kernel_thread);
		}

		if (thread_count <= 0)
			exit(0);

		/* Use a new reference to terminate ourselves. Should never return. */
		PTHREAD_MACH_CALL(thread_terminate(mach_thread_self()), kern_res);
		fprintf(stderr, "thread_terminate(mach_thread_self()) failed: %s\n",
				mach_error_string(kern_res));
	} else {
		semaphore_t joinsem = SEMAPHORE_NULL;

		if ((self->joiner_notify == NULL) && (self->detached & PTHREAD_CREATE_JOINABLE))
			joinsem = new_sem_from_pool();
		LOCK(self->lock);
		self->detached |= _PTHREAD_EXITED;

		self->exit_value = value_ptr;
		if (self->detached & PTHREAD_CREATE_JOINABLE) {
			if (self->joiner_notify == NULL) {
				self->joiner_notify = joinsem;
				joinsem = SEMAPHORE_NULL;
			}
			UNLOCK(self->lock);
			if (joinsem != SEMAPHORE_NULL)
				restore_sem_to_pool(joinsem);
			_pthread_free_pthread_onstack(self, 0, 1);
		} else {
			UNLOCK(self->lock);
			/* with no joiner, we let become available consume our cached ref */
			if (joinsem != SEMAPHORE_NULL)
				restore_sem_to_pool(joinsem);
			_pthread_free_pthread_onstack(self, 1, 1);
		}
	}
	abort();
}

void
pthread_exit(void *value_ptr)
{
	pthread_t self = pthread_self();
	if (self->wqthread != 0)
		workqueue_exit(self, self->cur_workq, self->cur_workitem);
	else
		_pthread_exit(self, value_ptr);
}

/*
 * Get the scheduling policy and scheduling paramters for a thread.
 */
int       
pthread_getschedparam(pthread_t thread, 
		      int *policy,
		      struct sched_param *param)
{
	int ret;

	if (thread == NULL)
		return(ESRCH);
	
	LOCK(_pthread_list_lock);

	if ((ret = _pthread_find_thread(thread)) != 0) {
		UNLOCK(_pthread_list_lock);
		return(ret);
	}
	if (policy != 0)
		*policy = thread->policy;
	if (param != 0)
		*param = thread->param;
	UNLOCK(_pthread_list_lock);

	return(0);
}

/*
 * Set the scheduling policy and scheduling paramters for a thread.
 */
static int       
pthread_setschedparam_internal(pthread_t thread, 
		      mach_port_t  kport,
		      int policy,
		      const struct sched_param *param)
{
	policy_base_data_t bases;
	policy_base_t base;
	mach_msg_type_number_t count;
	kern_return_t ret;

	switch (policy)
	{
		case SCHED_OTHER:
			bases.ts.base_priority = param->sched_priority;
			base = (policy_base_t)&bases.ts;
			count = POLICY_TIMESHARE_BASE_COUNT;
			break;
		case SCHED_FIFO:
			bases.fifo.base_priority = param->sched_priority;
			base = (policy_base_t)&bases.fifo;
			count = POLICY_FIFO_BASE_COUNT;
			break;
		case SCHED_RR:
			bases.rr.base_priority = param->sched_priority;
			/* quantum isn't public yet */
			bases.rr.quantum = param->quantum;
			base = (policy_base_t)&bases.rr;
			count = POLICY_RR_BASE_COUNT;
			break;
		default:
			return (EINVAL);
	}
	ret = thread_policy(kport, policy, base, count, TRUE);
	if (ret != KERN_SUCCESS)
			return (EINVAL);
	return (0);
}

int       
pthread_setschedparam(pthread_t t, 
		      int policy,
		      const struct sched_param *param)
{
	mach_port_t kport = MACH_PORT_NULL;
	int error;
	int bypass = 1;

	if (t != pthread_self() && t != &_thread ) { //since the main thread will not get de-allocated from underneath us
		bypass = 0;
		if (_pthread_lookup_thread(t, &kport, 0) != 0)
			return(ESRCH);
	} else
		kport = t->kernel_thread;

	error = pthread_setschedparam_internal(t, kport, policy, param);
	if (error == 0) {
		if (bypass == 0) {
			/* ensure the thread is still valid */
			LOCK(_pthread_list_lock);
			if ((error = _pthread_find_thread(t)) != 0) {
				UNLOCK(_pthread_list_lock);
				return(error);
			}
			t->policy = policy;
			t->param = *param;
			UNLOCK(_pthread_list_lock);
		}  else {
			t->policy = policy;
			t->param = *param;
		}
	}
	return(error);
}

/*
 * Get the minimum priority for the given policy
 */
int
sched_get_priority_min(int policy)
{
    return default_priority - 16;
}

/*
 * Get the maximum priority for the given policy
 */
int
sched_get_priority_max(int policy)
{
    return default_priority + 16;
}

/*
 * Determine if two thread identifiers represent the same thread.
 */
int       
pthread_equal(pthread_t t1, 
	      pthread_t t2)
{
	return (t1 == t2);
}

__private_extern__ void
_pthread_set_self(pthread_t p)
{
	extern void __pthread_set_self(pthread_t);
        if (p == 0) {
                bzero(&_thread, sizeof(struct _pthread));
                p = &_thread;
        }
        p->tsd[0] = p;
	__pthread_set_self(p);
}

void 
cthread_set_self(void *cself)
{
    pthread_t self = pthread_self();
    if ((self == (pthread_t)NULL) || (self->sig != _PTHREAD_SIG)) {
        _pthread_set_self(cself);
        return;
    }
    self->cthread_self = cself;
}

void *
ur_cthread_self(void) {
    pthread_t self = pthread_self();
    if ((self == (pthread_t)NULL) || (self->sig != _PTHREAD_SIG)) {
        return (void *)self;
    }
    return self->cthread_self;
}

/*
 * cancellation handler for pthread once as the init routine can have a
 * cancellation point. In that case we need to restore the spin unlock 
 */
void
__pthread_once_cancel_handler(pthread_once_t *once_control)
{
		_spin_unlock(&once_control->lock);
}


/*
 * Execute a function exactly one time in a thread-safe fashion.
 */
int       
pthread_once(pthread_once_t *once_control, 
	     void (*init_routine)(void))
{
	_spin_lock(&once_control->lock);
	if (once_control->sig == _PTHREAD_ONCE_SIG_init)
	{
		pthread_cleanup_push(__pthread_once_cancel_handler, once_control);
		(*init_routine)();
		pthread_cleanup_pop(0);
		once_control->sig = _PTHREAD_ONCE_SIG;
	}
	_spin_unlock(&once_control->lock);
	return (0);  /* Spec defines no possible errors! */
}

/*
 * Insert a cancellation point in a thread.
 */
__private_extern__ void
_pthread_testcancel(pthread_t thread, int isconforming)
{
	LOCK(thread->lock);
	if ((thread->cancel_state & (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING)) == 
	    (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING))
	{
		UNLOCK(thread->lock);
		if (isconforming)
			pthread_exit(PTHREAD_CANCELED);
		else
			pthread_exit(0);
	}
	UNLOCK(thread->lock);
}



int
pthread_getconcurrency(void)
{
	return(pthread_concurrency);
}

int
pthread_setconcurrency(int new_level)
{
	if (new_level < 0)
		return EINVAL;
	pthread_concurrency = new_level;
	return(0);
}

/*
 * Perform package initialization - called automatically when application starts
 */

__private_extern__ int
pthread_init(void)
{
	pthread_attr_t *attrs;
        pthread_t thread;
	kern_return_t kr;
	host_priority_info_data_t priority_info;
	host_info_t info;
	host_flavor_t flavor;
	host_t host;
	mach_msg_type_number_t count;
	int mib[2];
	size_t len;
	void *stackaddr;

	pthreadsize = round_page(sizeof (struct _pthread));
	count = HOST_PRIORITY_INFO_COUNT;
	info = (host_info_t)&priority_info;
	flavor = HOST_PRIORITY_INFO;
	host = mach_host_self();
	kr = host_info(host, flavor, info, &count);
        if (kr != KERN_SUCCESS)
                printf("host_info failed (%d); probably need privilege.\n", kr);
        else {
		default_priority = priority_info.user_priority;
		min_priority = priority_info.minimum_priority;
		max_priority = priority_info.maximum_priority;
	}
	attrs = &_pthread_attr_default;
	pthread_attr_init(attrs);

	TAILQ_INIT(&__pthread_head);
	LOCK_INIT(_pthread_list_lock);
	thread = &_thread;
	TAILQ_INSERT_HEAD(&__pthread_head, thread, plist);
	_pthread_set_self(thread);

	/* In case of dyld reset the tsd keys from 1 - 10 */
	_pthread_keys_init();

    	mib[0] = CTL_KERN;
    	mib[1] = KERN_USRSTACK;
    	len = sizeof (stackaddr);
    	if (sysctl (mib, 2, &stackaddr, &len, NULL, 0) != 0)
       		stackaddr = (void *)USRSTACK;
	_pthread_create(thread, attrs, stackaddr, mach_thread_self());
	thread->detached = PTHREAD_CREATE_JOINABLE|_PTHREAD_CREATE_PARENT;

	_init_cpu_capabilities();
	if (_NumCPUs() > 1)
		_spin_tries = MP_SPIN_TRIES;

	mach_port_deallocate(mach_task_self(), host);
    
#if defined(__ppc__)
	IF_ROSETTA() {
		__oldstyle = 1;
	}
#endif
#if defined(__arm__)
	__oldstyle = 1;
#endif

#if defined(_OBJC_PAGE_BASE_ADDRESS)
{
        vm_address_t objcRTPage = (vm_address_t)_OBJC_PAGE_BASE_ADDRESS;
        kr = vm_map(mach_task_self(),
                 &objcRTPage, vm_page_size * 4, vm_page_size - 1,
                 VM_FLAGS_FIXED | VM_MAKE_TAG(0), // Which tag to use?
                 MACH_PORT_NULL,
                 (vm_address_t)0, FALSE,
                 (vm_prot_t)0, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE,
                 VM_INHERIT_DEFAULT);
		/* We ignore the return result here. The ObjC runtime will just have to deal. */
}
#endif

	mig_init(1);		/* enable multi-threaded mig interfaces */
	if (__oldstyle == 0) {
#if defined(__i386__) || defined(__x86_64__)
		__bsdthread_register(thread_start, start_wqthread, round_page(sizeof(struct _pthread)));
#else
		__bsdthread_register(_pthread_start, _pthread_wqthread, round_page(sizeof(struct _pthread)));
#endif
	}
	return 0;
}

int sched_yield(void)
{
    swtch_pri(0);
    return 0;
}

/* This used to be the "magic" that gets the initialization routine called when the application starts */
static int _do_nothing(void) { return 0; }
int (*_cthread_init_routine)(void) = _do_nothing;

/* Get a semaphore from the pool, growing it if necessary */

__private_extern__ semaphore_t new_sem_from_pool(void) {
	kern_return_t res;
	semaphore_t sem;
        int i;
        
	LOCK(sem_pool_lock);
	if (sem_pool_current == sem_pool_count) {
		sem_pool_count += 16;
		sem_pool = realloc(sem_pool, sem_pool_count * sizeof(semaphore_t));
		for (i = sem_pool_current; i < sem_pool_count; i++) {
			PTHREAD_MACH_CALL(semaphore_create(mach_task_self(), &sem_pool[i], SYNC_POLICY_FIFO, 0), res);
		}
	}
	sem = sem_pool[sem_pool_current++];
	UNLOCK(sem_pool_lock);
	return sem;
}

/* Put a semaphore back into the pool */
__private_extern__ void restore_sem_to_pool(semaphore_t sem) {
	LOCK(sem_pool_lock);
	sem_pool[--sem_pool_current] = sem;
	UNLOCK(sem_pool_lock);
}

static void sem_pool_reset(void) {
	LOCK(sem_pool_lock);
	sem_pool_count = 0;
	sem_pool_current = 0;
	sem_pool = NULL; 
	UNLOCK(sem_pool_lock);
}

__private_extern__ void _pthread_fork_child(pthread_t p) {
	/* Just in case somebody had it locked... */
	UNLOCK(sem_pool_lock);
	sem_pool_reset();
	/* No need to hold the pthread_list_lock as no one other than this 
	 * thread is present at this time
	 */
	TAILQ_INIT(&__pthread_head);
	LOCK_INIT(_pthread_list_lock);
	TAILQ_INSERT_HEAD(&__pthread_head, p, plist);
	_pthread_count = 1;
}

/*
 * Query/update the cancelability 'state' of a thread
 */
int
_pthread_setcancelstate_internal(int state, int *oldstate, int conforming)
{
	pthread_t self = pthread_self();


	switch (state) {
		case PTHREAD_CANCEL_ENABLE:
		if (conforming)
			__pthread_canceled(1);
			break;
		case PTHREAD_CANCEL_DISABLE:
		if (conforming)
			__pthread_canceled(2);
			break;
		default:
			return EINVAL;
	}

	self = pthread_self();
	LOCK(self->lock);
	if (oldstate)
		*oldstate = self->cancel_state & _PTHREAD_CANCEL_STATE_MASK;
	self->cancel_state &= ~_PTHREAD_CANCEL_STATE_MASK;
	self->cancel_state |= state;
	UNLOCK(self->lock);
	if (!conforming)
		_pthread_testcancel(self, 0);  /* See if we need to 'die' now... */
	return (0);
}

/* When a thread exits set the cancellation state to DISABLE and DEFERRED */
static void
_pthread_setcancelstate_exit(pthread_t self, void * value_ptr, int conforming)
{
	LOCK(self->lock);
	self->cancel_state &= ~(_PTHREAD_CANCEL_STATE_MASK | _PTHREAD_CANCEL_TYPE_MASK);
	self->cancel_state |= (PTHREAD_CANCEL_DISABLE | PTHREAD_CANCEL_DEFERRED);
	if ((value_ptr == PTHREAD_CANCELED)) {
// 4597450: begin
		self->detached |= _PTHREAD_WASCANCEL;
// 4597450: end
	}
	UNLOCK(self->lock);
}

int
_pthread_join_cleanup(pthread_t thread, void ** value_ptr, int conforming)
{
	kern_return_t res;
	int detached = 0, ret;

#if WQ_TRACE
	__kdebug_trace(0x9000028, thread, 0, 0, 1, 0);
#endif
	/* The scenario where the joiner was waiting for the thread and
	 * the pthread detach happened on that thread. Then the semaphore
	 * will trigger but by the time joiner runs, the target thread could be
	 * freed. So we need to make sure that the thread is still in the list 
	 * and is joinable  before we continue with the join.
	 */
	LOCK(_pthread_list_lock);
	if ((ret = _pthread_find_thread(thread)) != 0) {
		UNLOCK(_pthread_list_lock);
		/* returns ESRCH */
		return(ret);
	}
	if ((thread->detached & PTHREAD_CREATE_JOINABLE) == 0) {
		/* the thread might be a detached thread */
		UNLOCK(_pthread_list_lock);
		return(ESRCH);

	}
	/* It is still a joinable thread and needs to be reaped */
	TAILQ_REMOVE(&__pthread_head, thread, plist);
#if WQ_TRACE
	__kdebug_trace(0x9000010, thread, 0, 0, 3, 0);
#endif
	UNLOCK(_pthread_list_lock);

	if (value_ptr)
		*value_ptr = thread->exit_value;
	if (conforming) {
		if ((thread->cancel_state & (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING)) ==
			(PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING)) {
			*value_ptr = PTHREAD_CANCELED;
		}
	}
	if (thread->reply_port != MACH_PORT_NULL) {
		res = mach_port_mod_refs(mach_task_self(), thread->reply_port, MACH_PORT_RIGHT_RECEIVE, -1);
		if (res != KERN_SUCCESS) 
			fprintf(stderr,"mach_port_mod_refs(reply_port) failed: %s\n",mach_error_string(res));
		thread->reply_port = MACH_PORT_NULL;
	}
	if (thread->freeStackOnExit) {
		thread->sig = _PTHREAD_NO_SIG;
#if WQ_TRACE
	__kdebug_trace(0x9000028, thread, 0, 0, 2, 0);
#endif
		vm_deallocate(mach_task_self(), thread, pthreadsize);
	} else {
		thread->sig = _PTHREAD_NO_SIG;
#if WQ_TRACE
	__kdebug_trace(0x9000028, thread, 0, 0, 3, 0);
#endif
		free(thread);
	}
	return(0);
}

/* ALWAYS called with list lock and return with list lock */
int
_pthread_find_thread(pthread_t thread)
{
	pthread_t p;

loop:
	TAILQ_FOREACH(p, &__pthread_head, plist) {
		if (p == thread) {
			if (thread->kernel_thread == MACH_PORT_NULL) {
				UNLOCK(_pthread_list_lock);
				sched_yield();
				LOCK(_pthread_list_lock);
				goto loop;
			} 
			return(0);
		}
	}
	return(ESRCH);
}

int
_pthread_lookup_thread(pthread_t thread, mach_port_t * portp, int only_joinable)
{
	mach_port_t kport;
	int ret = 0;

	if (thread == NULL)
		return(ESRCH);
	
	LOCK(_pthread_list_lock);

	if ((ret = _pthread_find_thread(thread)) != 0) {
		UNLOCK(_pthread_list_lock);
		return(ret);
	}
	if ((only_joinable != 0) && ((thread->detached & PTHREAD_CREATE_DETACHED) != 0)) {
		UNLOCK(_pthread_list_lock);
		return(EINVAL);
	} 
	kport = thread->kernel_thread;
	UNLOCK(_pthread_list_lock);
	if (portp != NULL)
		*portp = kport;
	return(0);
}

/* XXXXXXXXXXXXX Pthread Workqueue Attributes XXXXXXXXXXXXXXXXXX */
int 
pthread_workqueue_attr_init_np(pthread_workqueue_attr_t * attrp)
{
	attrp->stacksize = DEFAULT_STACK_SIZE;
	attrp->istimeshare = 1;
	attrp->importance = 0;
	attrp->affinity = 0;
	attrp->queueprio = WORK_QUEUE_NORMALIZER;
	attrp->sig = PTHEAD_WRKQUEUE_ATTR_SIG;
	return(0);
}

int 
pthread_workqueue_attr_destroy_np(pthread_workqueue_attr_t * attr)
{
	if (attr->sig == PTHEAD_WRKQUEUE_ATTR_SIG)
	{
		return (0);
	} else
	{
		return (EINVAL); /* Not an attribute structure! */
	}
}

#ifdef NOTYET /* [ */
int 
pthread_workqueue_attr_getstacksize_np(const pthread_workqueue_attr_t * attr, size_t * stacksizep)
{
    if (attr->sig == PTHEAD_WRKQUEUE_ATTR_SIG) {
        *stacksizep = attr->stacksize;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int 
pthread_workqueue_attr_setstacksize_np(pthread_workqueue_attr_t * attr, size_t stacksize)
{
    if ((attr->sig == PTHEAD_WRKQUEUE_ATTR_SIG) && ((stacksize % vm_page_size) == 0) && (stacksize >= PTHREAD_STACK_MIN)) {
        attr->stacksize = stacksize;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}


int 
pthread_workqueue_attr_getthreadtimeshare_np(const pthread_workqueue_attr_t * attr, int * istimesahrep)
{
    if (attr->sig == PTHEAD_WRKQUEUE_ATTR_SIG) {
        *istimesahrep = attr->istimeshare;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int 
pthread_workqueue_attr_settthreadtimeshare_np(pthread_workqueue_attr_t * attr, int istimeshare)
{
    if (attr->sig == PTHEAD_WRKQUEUE_ATTR_SIG) {
	if (istimeshare != 0)
        	attr->istimeshare = istimeshare;
	else
        	attr->istimeshare = 0;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int 
pthread_workqueue_attr_getthreadimportance_np(const pthread_workqueue_attr_t * attr, int * importancep)
{
    if (attr->sig == PTHEAD_WRKQUEUE_ATTR_SIG) {
        *importancep = attr->importance;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int 
pthread_workqueue_attr_settthreadimportance_np(pthread_workqueue_attr_t * attr, int importance)
{
    if (attr->sig == PTHEAD_WRKQUEUE_ATTR_SIG){
        attr->importance = importance;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int 
pthread_workqueue_attr_getthreadaffinity_np(const pthread_workqueue_attr_t * attr, int * affinityp)
{
    if (attr->sig == PTHEAD_WRKQUEUE_ATTR_SIG) {
        *affinityp = attr->affinity;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int 
pthread_workqueue_attr_settthreadaffinity_np(pthread_workqueue_attr_t * attr, int affinity)
{
    if (attr->sig == PTHEAD_WRKQUEUE_ATTR_SIG){
        attr->affinity = affinity;
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

#endif /* NOTYET  ] */

int 
pthread_workqueue_attr_getqueuepriority_np(const pthread_workqueue_attr_t * attr, int * qpriop)
{
    if (attr->sig == PTHEAD_WRKQUEUE_ATTR_SIG) {
        *qpriop = (attr->queueprio - WORK_QUEUE_NORMALIZER);
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int 
pthread_workqueue_attr_setqueuepriority_np(pthread_workqueue_attr_t * attr, int qprio)
{
	/* only -2 to +2 is valid */
    if ((attr->sig == PTHEAD_WRKQUEUE_ATTR_SIG) && (qprio <= 2) && (qprio >= -2)) {
        attr->queueprio = (qprio + WORK_QUEUE_NORMALIZER);
        return (0);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

/* XXXXXXXXXXXXX Pthread Workqueue support routines XXXXXXXXXXXXXXXXXX */

static void
workqueue_list_lock()
{
	OSSpinLockLock(&__workqueue_list_lock);
}

static void
workqueue_list_unlock()
{
	OSSpinLockUnlock(&__workqueue_list_lock);
}

int
pthread_workqueue_init_np()
{
	int ret;

	workqueue_list_lock();
	ret =_pthread_work_internal_init();
	workqueue_list_unlock();

	return(ret);
}

static int
_pthread_work_internal_init(void)
{
	int i, error;
	pthread_workqueue_head_t headp;
	pthread_workitem_t witemp;
	pthread_workqueue_t wq;

	if (kernel_workq_setup == 0) {
#if defined(__i386__) || defined(__x86_64__)
		__bsdthread_register(thread_start, start_wqthread, round_page(sizeof(struct _pthread)));
#else
		__bsdthread_register(_pthread_start, _pthread_wqthread, round_page(sizeof(struct _pthread)));
#endif

		_pthread_wq_attr_default.stacksize = DEFAULT_STACK_SIZE;
		_pthread_wq_attr_default.istimeshare = 1;
		_pthread_wq_attr_default.importance = 0;
		_pthread_wq_attr_default.affinity = 0;
		_pthread_wq_attr_default.queueprio = WORK_QUEUE_NORMALIZER;
		_pthread_wq_attr_default.sig = PTHEAD_WRKQUEUE_ATTR_SIG;

		for( i = 0; i< WQ_NUM_PRIO_QS; i++) {
			headp = __pthread_wq_head_tbl[i];
			TAILQ_INIT(&headp->wqhead);
			headp->next_workq = 0;
		}

		/* create work item and workqueue pools */
		witemp = (struct _pthread_workitem *)malloc(sizeof(struct _pthread_workitem) * WORKITEM_POOL_SIZE);
		bzero(witemp, (sizeof(struct _pthread_workitem) * WORKITEM_POOL_SIZE));
		for (i = 0; i < WORKITEM_POOL_SIZE; i++) {
			TAILQ_INSERT_TAIL(&__pthread_workitem_pool_head, &witemp[i], item_entry);
		}
		wq = (struct _pthread_workqueue *)malloc(sizeof(struct _pthread_workqueue) * WORKQUEUE_POOL_SIZE);
		bzero(wq, (sizeof(struct _pthread_workqueue) * WORKQUEUE_POOL_SIZE));
		for (i = 0; i < WORKQUEUE_POOL_SIZE; i++) {
			TAILQ_INSERT_TAIL(&__pthread_workqueue_pool_head, &wq[i], wq_list);
		}

		if (error = __workq_open()) {
			TAILQ_INIT(&__pthread_workitem_pool_head);
			TAILQ_INIT(&__pthread_workqueue_pool_head);
			free(witemp);
			free(wq);
			return(ENOMEM);
		}
		kernel_workq_setup = 1;
	}
	return(0);
}


/* This routine is called with list lock held */
static pthread_workitem_t
alloc_workitem(void)
{
	pthread_workitem_t witem;

	if (TAILQ_EMPTY(&__pthread_workitem_pool_head)) {
		workqueue_list_unlock();
		witem = malloc(sizeof(struct _pthread_workitem));
		workqueue_list_lock();
	} else {
		witem = TAILQ_FIRST(&__pthread_workitem_pool_head);
		TAILQ_REMOVE(&__pthread_workitem_pool_head, witem, item_entry);
	}
	return(witem);
}

/* This routine is called with list lock held */
static void
free_workitem(pthread_workitem_t witem) 
{
	TAILQ_INSERT_TAIL(&__pthread_workitem_pool_head, witem, item_entry);
}

/* This routine is called with list lock held */
static pthread_workqueue_t
alloc_workqueue(void)
{
	pthread_workqueue_t wq;

	if (TAILQ_EMPTY(&__pthread_workqueue_pool_head)) {
		workqueue_list_unlock();
		wq = malloc(sizeof(struct _pthread_workqueue));
		workqueue_list_lock();
	} else {
		wq = TAILQ_FIRST(&__pthread_workqueue_pool_head);
		TAILQ_REMOVE(&__pthread_workqueue_pool_head, wq, wq_list);
	}
	user_workq_count++;
	return(wq);
}

/* This routine is called with list lock held */
static void
free_workqueue(pthread_workqueue_t wq) 
{
	user_workq_count--;
	TAILQ_INSERT_TAIL(&__pthread_workqueue_pool_head, wq, wq_list);
}

static void
_pthread_workq_init(pthread_workqueue_t wq, const pthread_workqueue_attr_t * attr)
{
	bzero(wq, sizeof(struct _pthread_workqueue));
	if (attr != NULL) {
		wq->stacksize = attr->stacksize;
		wq->istimeshare = attr->istimeshare;
		wq->importance = attr->importance;
		wq->affinity = attr->affinity;
		wq->queueprio = attr->queueprio;
	 } else {
		wq->stacksize = DEFAULT_STACK_SIZE;
		wq->istimeshare = 1;
		wq->importance = 0;
		wq->affinity = 0;
		wq->queueprio = WORK_QUEUE_NORMALIZER;
	}
	LOCK_INIT(wq->lock);
	wq->flags = 0;
	TAILQ_INIT(&wq->item_listhead);
	TAILQ_INIT(&wq->item_kernhead);
	wq->wq_list.tqe_next = 0;
	wq->wq_list.tqe_prev = 0;
	wq->sig = PTHEAD_WRKQUEUE_SIG;
	wq->headp = __pthread_wq_head_tbl[wq->queueprio];
}

int 
valid_workq(pthread_workqueue_t workq)
{
	if (workq->sig == PTHEAD_WRKQUEUE_SIG) 
		return(1);
	else
		return(0);
}


/* called with list lock */
static void
pick_nextworkqueue_droplock()
{
	int i, curwqprio, val, found;
	pthread_workqueue_head_t headp;
	pthread_workqueue_t workq;
	pthread_workqueue_t nworkq = NULL;

loop:
	while (kernel_workq_count < KERNEL_WORKQ_ELEM_MAX) {
		found = 0;
		for (i = 0; i < WQ_NUM_PRIO_QS; i++)  {
			wqreadyprio = i;	/* because there is nothing else higher to run */
			headp = __pthread_wq_head_tbl[i];

			if (TAILQ_EMPTY(&headp->wqhead))
				continue;
			workq = headp->next_workq;
			if (workq == NULL)
				workq = TAILQ_FIRST(&headp->wqhead);
			curwqprio = workq->queueprio;
			nworkq = workq;	/* starting pt */
			while (kernel_workq_count < KERNEL_WORKQ_ELEM_MAX) {
				headp->next_workq = TAILQ_NEXT(workq, wq_list);
				if (headp->next_workq == NULL)
					headp->next_workq = TAILQ_FIRST(&headp->wqhead);
				val = post_nextworkitem(workq);

				if (val != 0) {
					/* things could have changed so reasses */
					/* If kernel queue is full , skip */
					if (kernel_workq_count >= KERNEL_WORKQ_ELEM_MAX)
						break;
					/* If anything with higher prio arrived, then reevaluate */
					if (wqreadyprio < curwqprio)
						goto loop;	/* we need re evaluate again */
					/* we can post some more work items */
					found = 1;
				}

				/* cannot use workq here as it could be freed */
				if (TAILQ_EMPTY(&headp->wqhead))
					break;
				/* if we found nothing to run and only one workqueue in the list, skip */
				if ((val == 0) && (workq == headp->next_workq))
					break;
				workq = headp->next_workq;
				if (workq == NULL)
					workq = TAILQ_FIRST(&headp->wqhead);
				if (val != 0)
					nworkq = workq;
				/* if we found nothing to run and back to workq where we started */
				if ((val == 0) && (workq == nworkq))
					break;
			}
			if (kernel_workq_count >= KERNEL_WORKQ_ELEM_MAX)
				break;
		}
		/* nothing found to run?  */
		if (found == 0)
			break;
	}
	workqueue_list_unlock();
}

static int
post_nextworkitem(pthread_workqueue_t workq)
{
	int error;
	pthread_workitem_t witem;
	pthread_workqueue_head_t headp;
	void (*func)(pthread_workqueue_t, void *);

		if ((workq->flags & PTHREAD_WORKQ_SUSPEND) == PTHREAD_WORKQ_SUSPEND) {
			return(0);
		}
		if (TAILQ_EMPTY(&workq->item_listhead)) {
			return(0);
		}
		witem = TAILQ_FIRST(&workq->item_listhead);
		headp = workq->headp;
		if ((witem->flags & PTH_WQITEM_BARRIER) == PTH_WQITEM_BARRIER) {
			
			if ((witem->flags & PTH_WQITEM_APPLIED) != 0) {
				return(0);
			}
			/* Also barrier when nothing is there needs to be handled */
			/* Nothing to wait for */
			if (workq->kq_count != 0) {
				witem->flags |= PTH_WQITEM_APPLIED;
				workq->flags |= PTHREAD_WORKQ_BARRIER_ON;
				workq->barrier_count = workq->kq_count;
#if WQ_TRACE
			__kdebug_trace(0x9000064, 1, workq->barrier_count, 0, 0, 0);
#endif
				return(1);
			} else {
#if WQ_TRACE
			__kdebug_trace(0x9000064, 2, workq->barrier_count, 0, 0, 0);
#endif
				if (witem->func != NULL) {
				        workqueue_list_unlock();
					func = witem->func;
					(*func)(workq, witem->func_arg);
					workqueue_list_lock();
				}
				TAILQ_REMOVE(&workq->item_listhead, witem, item_entry);
				witem->flags = 0;
				free_workitem(witem);
				return(1);
			}
		} else if ((witem->flags & PTH_WQITEM_DESTROY) == PTH_WQITEM_DESTROY) {
#if WQ_TRACE
			__kdebug_trace(0x9000068, 1, workq->kq_count, 0, 0, 0);
#endif
			if ((witem->flags & PTH_WQITEM_APPLIED) != 0) {
				return(0);
			}
			witem->flags |= PTH_WQITEM_APPLIED;
			workq->flags |= (PTHREAD_WORKQ_BARRIER_ON | PTHREAD_WORKQ_TERM_ON);
			workq->barrier_count = workq->kq_count;
			workq->term_callback = witem->func;
			workq->term_callarg = witem->func_arg;
			TAILQ_REMOVE(&workq->item_listhead, witem, item_entry);
			if ((TAILQ_EMPTY(&workq->item_listhead)) && (workq->kq_count == 0)) {
				if (!(TAILQ_EMPTY(&workq->item_kernhead))) {
#if WQ_TRACE
				__kdebug_trace(0x900006c, workq, workq->kq_count, 0, 0xff, 0);
#endif
				}
				witem->flags = 0;
				free_workitem(witem);
				workq->flags |= PTHREAD_WORKQ_DESTROYED;
#if WQ_TRACE
			__kdebug_trace(0x900006c, workq, workq->kq_count, 0, 1, 0);
#endif
				headp = __pthread_wq_head_tbl[workq->queueprio];
				if (headp->next_workq == workq) {
					headp->next_workq = TAILQ_NEXT(workq, wq_list);
					if (headp->next_workq == NULL) {
						headp->next_workq = TAILQ_FIRST(&headp->wqhead);
						if (headp->next_workq == workq)
							headp->next_workq = NULL;
					}
				}
				workq->sig = 0;
				TAILQ_REMOVE(&headp->wqhead, workq, wq_list);
				if (workq->term_callback != NULL) {
				        workqueue_list_unlock();
					(*workq->term_callback)(workq, workq->term_callarg);
					workqueue_list_lock();
				}
				free_workqueue(workq);
				return(1);
			} else 
				TAILQ_INSERT_HEAD(&workq->item_listhead, witem, item_entry);
#if WQ_TRACE
			__kdebug_trace(0x9000068, 2, workq->barrier_count, 0, 0, 0);
#endif
			return(1);
		}  else {
#if WQ_TRACE
			__kdebug_trace(0x9000060, witem, workq, witem->func_arg, 0xfff, 0);
#endif
			TAILQ_REMOVE(&workq->item_listhead, witem, item_entry);
			TAILQ_INSERT_TAIL(&workq->item_kernhead, witem, item_entry);
			if ((witem->flags & PTH_WQITEM_KERN_COUNT) == 0) {
				workq->kq_count++;
				witem->flags |= PTH_WQITEM_KERN_COUNT;
			}
			OSAtomicIncrement32(&kernel_workq_count);
			workqueue_list_unlock();
			if (( error =__workq_ops(WQOPS_QUEUE_ADD, witem, 0)) == -1) {
				OSAtomicDecrement32(&kernel_workq_count);
				workqueue_list_lock();
#if WQ_TRACE
			__kdebug_trace(0x900007c, witem, workq, witem->func_arg, workq->kq_count, 0);
#endif
				TAILQ_REMOVE(&workq->item_kernhead, witem, item_entry);
				TAILQ_INSERT_HEAD(&workq->item_listhead, witem, item_entry);
				if ((workq->flags & (PTHREAD_WORKQ_BARRIER_ON | PTHREAD_WORKQ_TERM_ON)) != 0)
					workq->flags |= PTHREAD_WORKQ_REQUEUED;
			} else
				workqueue_list_lock();
#if WQ_TRACE
			__kdebug_trace(0x9000060, witem, workq, witem->func_arg, workq->kq_count, 0);
#endif
			return(1);
		}
		/* noone should come here */
#if 1
		printf("error in logic for next workitem\n");
		abort();
#endif
		return(0);
}

void
_pthread_wqthread(pthread_t self, mach_port_t kport, void * stackaddr, pthread_workitem_t item, int reuse)
{
	int ret;
	pthread_attr_t *attrs = &_pthread_attr_default;
	pthread_workqueue_t workq;
	pthread_t pself;


	workq = item->workq;
	if (reuse == 0) {
		/* reuse is set to 0, when a thread is newly created to run a workitem */
		_pthread_struct_init(self, attrs, stackaddr,  DEFAULT_STACK_SIZE, 1, 1);
		self->wqthread = 1;
		self->parentcheck = 1;

		/* These are not joinable threads */
		self->detached &= ~PTHREAD_CREATE_JOINABLE;
		self->detached |= PTHREAD_CREATE_DETACHED;
#if defined(__i386__) || defined(__x86_64__)
		_pthread_set_self(self);
#endif
#if WQ_TRACE
		__kdebug_trace(0x9000050, self, item, item->func_arg, 0, 0);
#endif
		self->kernel_thread = kport;
		self->fun = item->func;
		self->arg = item->func_arg;
		/* Add to the pthread list */
		LOCK(_pthread_list_lock);
		TAILQ_INSERT_TAIL(&__pthread_head, self, plist);
#if WQ_TRACE
		__kdebug_trace(0x900000c, self, 0, 0, 10, 0);
#endif
		_pthread_count++;
		UNLOCK(_pthread_list_lock);
	} else  {
		/* reuse is set to 1, when a thread is resued to run another work item */
#if WQ_TRACE
		__kdebug_trace(0x9000054, self, item, item->func_arg, 0, 0);
#endif
		/* reset all tsd from 1 to KEYS_MAX */
		 _pthread_tsd_reinit(self);

		self->fun = item->func;
		self->arg = item->func_arg;
	}

#if WQ_DEBUG
	if (reuse == 0) {
		pself = pthread_self();
		if (self != pself) {
#if WQ_TRACE
		__kdebug_trace(0x9000078, self, pself, item->func_arg, 0, 0);
#endif
			printf("pthread_self not set: pself %p, passed in %p\n", pself, self);
    			_pthread_set_self(self);
			pself = pthread_self();
			if (self != pself)
				printf("(2)pthread_self not set: pself %p, passed in %p\n", pself, self);
			pself = self;
		}
	} else {
		pself = pthread_self();
		if (self != pself) {
			printf("(3)pthread_self not set in reuse: pself %p, passed in %p\n", pself, self);
			abort();
		}
	}
#endif /* WQ_DEBUG */

	self->cur_workq = workq;
	self->cur_workitem = item;
	OSAtomicDecrement32(&kernel_workq_count);

	ret = (*self->fun)(self->arg);

	workqueue_exit(self, workq, item);

}

static void
workqueue_exit(pthread_t self, pthread_workqueue_t workq, pthread_workitem_t item)
{
	pthread_attr_t *attrs = &_pthread_attr_default;
	pthread_workitem_t baritem;
	pthread_workqueue_head_t headp;
	void (*func)(pthread_workqueue_t, void *);
	
	workqueue_list_lock();

	TAILQ_REMOVE(&workq->item_kernhead, item, item_entry);
	workq->kq_count--;
#if WQ_TRACE
	__kdebug_trace(0x9000070, self, 1, item->func_arg, workq->kq_count, 0);
#endif
	item->flags = 0;
	free_workitem(item);

	if ((workq->flags & PTHREAD_WORKQ_BARRIER_ON) == PTHREAD_WORKQ_BARRIER_ON) {
		workq->barrier_count--;
#if WQ_TRACE
		__kdebug_trace(0x9000084, self, workq->barrier_count, workq->kq_count, 1, 0);
#endif
		if (workq->barrier_count <= 0 ) {
			/* Need to remove barrier item from the list */
			baritem = TAILQ_FIRST(&workq->item_listhead);
#if WQ_DEBUG
			if ((baritem->flags & (PTH_WQITEM_BARRIER | PTH_WQITEM_DESTROY| PTH_WQITEM_APPLIED)) == 0)
				printf("Incorect bar item being removed in barrier processing\n");
#endif /* WQ_DEBUG */
			/* if the front item is a barrier and call back is registered, run that */
			 if (((baritem->flags & PTH_WQITEM_BARRIER) == PTH_WQITEM_BARRIER) && (baritem->func != NULL)) {
				workqueue_list_unlock();
				func = baritem->func;
				(*func)(workq, baritem->func_arg);
				workqueue_list_lock();
			}
			TAILQ_REMOVE(&workq->item_listhead, baritem, item_entry);
			baritem->flags = 0;
			free_workitem(baritem);
			workq->flags &= ~PTHREAD_WORKQ_BARRIER_ON;
#if WQ_TRACE
			__kdebug_trace(0x9000058, self, item, item->func_arg, 0, 0);
#endif
			if ((workq->flags & PTHREAD_WORKQ_TERM_ON) != 0) {
				headp = __pthread_wq_head_tbl[workq->queueprio];
				workq->flags |= PTHREAD_WORKQ_DESTROYED;
#if WQ_TRACE
			__kdebug_trace(0x900006c, workq, workq->kq_count, 0, 2, 0);
#endif
				if (headp->next_workq == workq) {
					headp->next_workq = TAILQ_NEXT(workq, wq_list);
					if (headp->next_workq == NULL) {
						headp->next_workq = TAILQ_FIRST(&headp->wqhead);
						if (headp->next_workq == workq)
							headp->next_workq = NULL;
					}
				}
				TAILQ_REMOVE(&headp->wqhead, workq, wq_list);
				workq->sig = 0;
				if (workq->term_callback != NULL) {
				        workqueue_list_unlock();
					(*workq->term_callback)(workq, workq->term_callarg);
					workqueue_list_lock();
				}
				free_workqueue(workq);
			} else {
			/* if there are higher prio schedulabel item reset to wqreadyprio */
				if ((workq->queueprio < wqreadyprio) && (!(TAILQ_EMPTY(&workq->item_listhead))))
						wqreadyprio = workq->queueprio;
			}
		}
	} 
#if WQ_TRACE
	else {
		__kdebug_trace(0x9000070, self, 2, item->func_arg, workq->barrier_count, 0);
	}
	
	__kdebug_trace(0x900005c, self, item, 0, 0, 0);
#endif
	pick_nextworkqueue_droplock();
	_pthread_workq_return(self);
}

static void 
_pthread_workq_return(pthread_t self)
{
	struct __darwin_pthread_handler_rec *handler;
	int value = 0;
	int * value_ptr=&value;

	/* set cancel state to disable and type to deferred */
	_pthread_setcancelstate_exit(self, value_ptr, __unix_conforming);

	/* Make this thread not to receive any signals */
	__disable_threadsignal(1);

	while ((handler = self->__cleanup_stack) != 0)
	{
		(handler->__routine)(handler->__arg);
		self->__cleanup_stack = handler->__next;
	}
	_pthread_tsd_cleanup(self);

	__workq_ops(WQOPS_THREAD_RETURN, NULL, 0);

	/* This is the way to terminate the thread */
	_pthread_exit(self, NULL);
}


/* returns 0 if it handles it, otherwise 1 */
static int
handle_removeitem(pthread_workqueue_t workq, pthread_workitem_t item)
{
	pthread_workitem_t baritem;
	pthread_workqueue_head_t headp;
	void (*func)(pthread_workqueue_t, void *);

	if ((workq->flags & PTHREAD_WORKQ_BARRIER_ON) == PTHREAD_WORKQ_BARRIER_ON) {
		workq->barrier_count--;
		if (workq->barrier_count <= 0 ) {
			/* Need to remove barrier item from the list */
			baritem = TAILQ_FIRST(&workq->item_listhead);
#if WQ_DEBUG
			if ((baritem->flags & (PTH_WQITEM_BARRIER | PTH_WQITEM_DESTROY| PTH_WQITEM_APPLIED)) == 0)
				printf("Incorect bar item being removed in barrier processing\n");
#endif /* WQ_DEBUG */
			/* if the front item is a barrier and call back is registered, run that */
			if (((baritem->flags & PTH_WQITEM_BARRIER) == PTH_WQITEM_BARRIER) 
			 	&& (baritem->func != NULL)) {
				workqueue_list_unlock();
				func = baritem->func;
				(*func)(workq, baritem->func_arg);
				workqueue_list_lock();
			}
			TAILQ_REMOVE(&workq->item_listhead, baritem, item_entry);
			baritem->flags = 0;
			free_workitem(baritem);
			item->flags = 0;
			free_workitem(item);
			workq->flags &= ~PTHREAD_WORKQ_BARRIER_ON;
#if WQ_TRACE
			__kdebug_trace(0x9000058, pthread_self(), item, item->func_arg, 0, 0);
#endif
			if ((workq->flags & PTHREAD_WORKQ_TERM_ON) != 0) {
				headp = __pthread_wq_head_tbl[workq->queueprio];
				workq->flags |= PTHREAD_WORKQ_DESTROYED;
#if WQ_TRACE
				__kdebug_trace(0x900006c, workq, workq->kq_count, 0, 2, 0);
#endif
				if (headp->next_workq == workq) {
					headp->next_workq = TAILQ_NEXT(workq, wq_list);
					if (headp->next_workq == NULL) {
						headp->next_workq = TAILQ_FIRST(&headp->wqhead);
						if (headp->next_workq == workq)
							headp->next_workq = NULL;
					}
				}
				TAILQ_REMOVE(&headp->wqhead, workq, wq_list);
				workq->sig = 0;
				if (workq->term_callback != NULL) {
				        workqueue_list_unlock();
					(*workq->term_callback)(workq, workq->term_callarg);
					workqueue_list_lock();
				}
				free_workqueue(workq);
				pick_nextworkqueue_droplock();
				return(0);
			} else {
			/* if there are higher prio schedulabel item reset to wqreadyprio */
				if ((workq->queueprio < wqreadyprio) && (!(TAILQ_EMPTY(&workq->item_listhead))))
						wqreadyprio = workq->queueprio;
				free_workitem(item);
				pick_nextworkqueue_droplock();	
				return(0);
			}
		}
	}
	return(1);
}
/* XXXXXXXXXXXXX Pthread Workqueue functions XXXXXXXXXXXXXXXXXX */

int 
pthread_workqueue_create_np(pthread_workqueue_t * workqp, const pthread_workqueue_attr_t * attr)
{
	pthread_workqueue_t wq;
	pthread_workqueue_head_t headp;

	if ((attr != NULL) && (attr->sig != PTHEAD_WRKQUEUE_ATTR_SIG)) {
		return(EINVAL);
	}

	if (__is_threaded == 0)
		__is_threaded = 1;

	workqueue_list_lock();
	if (kernel_workq_setup == 0) {
		int ret = _pthread_work_internal_init();
		if (ret != 0) {
			workqueue_list_unlock();
			return(ret);
		}
	}
	
	wq = alloc_workqueue();
	
	_pthread_workq_init(wq, attr);

	headp = __pthread_wq_head_tbl[wq->queueprio];
	TAILQ_INSERT_TAIL(&headp->wqhead, wq, wq_list);
	if (headp->next_workq == NULL) {
		headp->next_workq = TAILQ_FIRST(&headp->wqhead);
	}

	workqueue_list_unlock();
	
	*workqp = wq;

	return(0);
}

int 
pthread_workqueue_destroy_np(pthread_workqueue_t workq, void (* callback_func)(pthread_workqueue_t, void *), void * callback_arg)
{
	pthread_workitem_t witem;
	pthread_workqueue_head_t headp;

	if (valid_workq(workq) == 0) {
		return(EINVAL);
	}

	workqueue_list_lock();

	/*
	 * Allocate the workitem here as it can drop the lock.
	 * Also we can evaluate the workqueue state only once.
	 */
	witem = alloc_workitem();
	witem->item_entry.tqe_next = 0;
	witem->item_entry.tqe_prev = 0;
	witem->func = callback_func;
	witem->func_arg = callback_arg;
	witem->flags = PTH_WQITEM_DESTROY;

	if ((workq->flags & (PTHREAD_WORKQ_IN_TERMINATE | PTHREAD_WORKQ_TERM_ON | PTHREAD_WORKQ_DESTROYED)) == 0) {
		workq->flags |= PTHREAD_WORKQ_IN_TERMINATE;
		/* If nothing queued or running, destroy now */
		if ((TAILQ_EMPTY(&workq->item_listhead)) && (TAILQ_EMPTY(&workq->item_kernhead))) {
			workq->flags |= (PTHREAD_WORKQ_TERM_ON | PTHREAD_WORKQ_DESTROYED);
			headp = __pthread_wq_head_tbl[workq->queueprio];
			workq->term_callback = callback_func;
			workq->term_callarg = callback_arg;
			if (headp->next_workq == workq) {
				headp->next_workq = TAILQ_NEXT(workq, wq_list);
				if (headp->next_workq == NULL) {
					headp->next_workq = TAILQ_FIRST(&headp->wqhead);
					if (headp->next_workq == workq)
						headp->next_workq = NULL;
				}
			}
			TAILQ_REMOVE(&headp->wqhead, workq, wq_list);
			workq->sig = 0;
			free_workitem(witem);
			if (workq->term_callback != NULL) {
			        workqueue_list_unlock();
				(*workq->term_callback)(workq, workq->term_callarg);
				workqueue_list_lock();
			}
#if WQ_TRACE
			__kdebug_trace(0x900006c, workq, workq->kq_count, 0, 3, 0);
#endif
			free_workqueue(workq);
			workqueue_list_unlock();
			return(0);
		}
		TAILQ_INSERT_TAIL(&workq->item_listhead, witem, item_entry);
	} else {
		free_workitem(witem);
		workqueue_list_unlock();
		return(EINPROGRESS);
	}
	workqueue_list_unlock();
	return(0);
}


int 
pthread_workqueue_additem_np(pthread_workqueue_t workq, void ( *workitem_func)(void *), void * workitem_arg, pthread_workitem_handle_t * itemhandlep)
{
	pthread_workitem_t witem;

	if (valid_workq(workq) == 0) {
		return(EINVAL);
	}

	workqueue_list_lock();

	/*
	 * Allocate the workitem here as it can drop the lock.
	 * Also we can evaluate the workqueue state only once.
	 */
	witem = alloc_workitem();
	witem->func = workitem_func;
	witem->func_arg = workitem_arg;
	witem->flags = 0;
	witem->workq = workq;
	witem->item_entry.tqe_next = 0;
	witem->item_entry.tqe_prev = 0;

	/* alloc workitem can drop the lock, check the state  */
	if ((workq->flags & (PTHREAD_WORKQ_IN_TERMINATE | PTHREAD_WORKQ_DESTROYED)) != 0) {
		free_workitem(witem);
		workqueue_list_unlock();
		*itemhandlep = 0;
		return(ESRCH);
	}

	if (itemhandlep != NULL)
		*itemhandlep = (pthread_workitem_handle_t *)witem;
	TAILQ_INSERT_TAIL(&workq->item_listhead, witem, item_entry);
	if (((workq->flags & PTHREAD_WORKQ_BARRIER_ON) == 0) && (workq->queueprio < wqreadyprio))
			wqreadyprio = workq->queueprio;

	pick_nextworkqueue_droplock();

	return(0);
}

int 
pthread_workqueue_removeitem_np(pthread_workqueue_t workq, pthread_workitem_handle_t itemhandle)
{
	pthread_workitem_t item, baritem;
	pthread_workqueue_head_t headp;
	int error;
	
	if (valid_workq(workq) == 0) {
		return(EINVAL);
	}

	workqueue_list_lock();
	if ((workq->flags & (PTHREAD_WORKQ_IN_TERMINATE | PTHREAD_WORKQ_DESTROYED)) != 0) {
		workqueue_list_unlock();
		return(ESRCH);
	}

	TAILQ_FOREACH(item, &workq->item_listhead, item_entry) {
		if (item == (pthread_workitem_t)itemhandle) {
			TAILQ_REMOVE(&workq->item_listhead, item, item_entry);
			if ((item->flags & (PTH_WQITEM_BARRIER | PTH_WQITEM_APPLIED)) == (PTH_WQITEM_BARRIER | PTH_WQITEM_APPLIED)) {
				workq->flags &= ~PTHREAD_WORKQ_BARRIER_ON;
				workq->barrier_count = 0;
				if ((workq->queueprio < wqreadyprio) && (!(TAILQ_EMPTY(&workq->item_listhead)))) {
						wqreadyprio = workq->queueprio;
				}
			} else if ((item->flags & PTH_WQITEM_KERN_COUNT) == PTH_WQITEM_KERN_COUNT) {
				workq->kq_count--;
				item->flags |= PTH_WQITEM_REMOVED;	
				if (handle_removeitem(workq, item) == 0)
					return(0);
			}
			item->flags |= PTH_WQITEM_NOTINLIST;
			free_workitem(item);
			workqueue_list_unlock();
			return(0);
		}
	}
			
	TAILQ_FOREACH(item, &workq->item_kernhead, item_entry) {
		if (item == (pthread_workitem_t)itemhandle) {
				workqueue_list_unlock();
				if ((error = __workq_ops(WQOPS_QUEUE_REMOVE, item, 0)) == 0) {
					workqueue_list_lock();
					TAILQ_REMOVE(&workq->item_kernhead, item, item_entry);
					OSAtomicDecrement32(&kernel_workq_count);
					workq->kq_count--; 
					item->flags |= PTH_WQITEM_REMOVED;	
					if (handle_removeitem(workq, item) != 0) {
						free_workitem(item);
						pick_nextworkqueue_droplock();
					}
					return(0);
				} else {
					return(EBUSY);
				}
		} 
	}
	workqueue_list_unlock();
	return(EINVAL);
}


int 
pthread_workqueue_addbarrier_np(pthread_workqueue_t workq, void (* callback_func)(pthread_workqueue_t, void *), void * callback_arg, __unused int waitforcallback, pthread_workitem_handle_t *itemhandlep)
{
	pthread_workitem_t witem;

	if (valid_workq(workq) == 0) {
		return(EINVAL);
	}

	workqueue_list_lock();

	/*
	 * Allocate the workitem here as it can drop the lock.
	 * Also we can evaluate the workqueue state only once.
	 */
	witem = alloc_workitem();
	witem->item_entry.tqe_next = 0;
	witem->item_entry.tqe_prev = 0;
	witem->func = callback_func;
	witem->func_arg = callback_arg;
	witem->flags = PTH_WQITEM_BARRIER;

	/* alloc workitem can drop the lock, check the state  */
	if ((workq->flags & (PTHREAD_WORKQ_IN_TERMINATE | PTHREAD_WORKQ_DESTROYED)) != 0) {
		free_workitem(witem);
		workqueue_list_unlock();
		return(ESRCH);
	}

	if (itemhandlep != NULL)
		*itemhandlep = (pthread_workitem_handle_t *)witem;

	TAILQ_INSERT_TAIL(&workq->item_listhead, witem, item_entry);
	if (((workq->flags & PTHREAD_WORKQ_BARRIER_ON) == 0) && (workq->queueprio < wqreadyprio))
			wqreadyprio = workq->queueprio;

	pick_nextworkqueue_droplock();

	return(0);
}

int 
pthread_workqueue_suspend_np(pthread_workqueue_t workq)
{
	if (valid_workq(workq) == 0) {
		return(EINVAL);
	}
	workqueue_list_lock();
	if ((workq->flags & (PTHREAD_WORKQ_IN_TERMINATE | PTHREAD_WORKQ_DESTROYED)) != 0) {
		workqueue_list_unlock();
		return(ESRCH);
	}

	workq->flags |= PTHREAD_WORKQ_SUSPEND;
	workq->suspend_count++;
	workqueue_list_unlock();
	return(0);
}

int 
pthread_workqueue_resume_np(pthread_workqueue_t workq)
{
	if (valid_workq(workq) == 0) {
		return(EINVAL);
	}
	workqueue_list_lock();
	if ((workq->flags & (PTHREAD_WORKQ_IN_TERMINATE | PTHREAD_WORKQ_DESTROYED)) != 0) {
		workqueue_list_unlock();
		return(ESRCH);
	}

	workq->suspend_count--;
	if (workq->suspend_count <= 0) {
		workq->flags &= ~PTHREAD_WORKQ_SUSPEND;
		if (((workq->flags & PTHREAD_WORKQ_BARRIER_ON) == 0) && (workq->queueprio < wqreadyprio))
				wqreadyprio = workq->queueprio;

			pick_nextworkqueue_droplock();
	} else
		workqueue_list_unlock();


	return(0);
}

#else /* !BUILDING_VARIANT ] [ */
extern int __unix_conforming;
extern int _pthread_count;
extern pthread_lock_t _pthread_list_lock;
extern void _pthread_testcancel(pthread_t thread, int isconforming);
extern int _pthread_reap_thread(pthread_t th, mach_port_t kernel_thread, void **value_ptr, int conforming);

#endif /* !BUILDING_VARIANT ] */

#if __DARWIN_UNIX03

__private_extern__ void 
__posix_join_cleanup(void *arg)
{
	pthread_t thread = (pthread_t)arg;
	int already_exited, res;
	void * dummy;
	semaphore_t death;
	mach_port_t joinport;
	int newstyle = 0;

	LOCK(thread->lock);
	already_exited = (thread->detached & _PTHREAD_EXITED);

	newstyle = thread->newstyle;
	
#if WQ_TRACE
	__kdebug_trace(0x900002c, thread, newstyle, 0, 0, 0);
#endif
	if (newstyle = 0) {
		death = thread->death;
		if (!already_exited){
			thread->joiner = (struct _pthread *)NULL;
			UNLOCK(thread->lock);
			restore_sem_to_pool(death);
		} else {
			UNLOCK(thread->lock);
			while ((res = _pthread_reap_thread(thread,
					thread->kernel_thread,
					&dummy, 1)) == EAGAIN)
				{
					sched_yield();
				}
			restore_sem_to_pool(death);

		}

	} else {
		/* leave another thread to join */
		thread->joiner = (struct _pthread *)NULL;
		UNLOCK(thread->lock);
	}
}

#endif /* __DARWIN_UNIX03 */


/*
 * Wait for a thread to terminate and obtain its exit value.
 */
/*
int       
pthread_join(pthread_t thread, 
	     void **value_ptr)

moved to pthread_cancelable.c */

/*
 * Cancel a thread
 */
int
pthread_cancel(pthread_t thread)
{
#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;
#endif /* __DARWIN_UNIX03 */

	if (_pthread_lookup_thread(thread, NULL, 0) != 0)
		return(ESRCH);

#if __DARWIN_UNIX03
	int state;

	LOCK(thread->lock);
	state = thread->cancel_state |= _PTHREAD_CANCEL_PENDING;
	UNLOCK(thread->lock);
	if (state & PTHREAD_CANCEL_ENABLE)
		__pthread_markcancel(thread->kernel_thread);
#else /* __DARWIN_UNIX03 */
	thread->cancel_state |= _PTHREAD_CANCEL_PENDING;
#endif /* __DARWIN_UNIX03 */
	return (0);
}

void
pthread_testcancel(void)
{
	pthread_t self = pthread_self();

#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;
	_pthread_testcancel(self, 1);
#else /* __DARWIN_UNIX03 */
	_pthread_testcancel(self, 0);
#endif /* __DARWIN_UNIX03 */

}


/*
 * Query/update the cancelability 'state' of a thread
 */
int
pthread_setcancelstate(int state, int *oldstate)
{
#if __DARWIN_UNIX03
	if (__unix_conforming == 0) {
		__unix_conforming = 1;
	}
	return (_pthread_setcancelstate_internal(state, oldstate, 1));
#else /* __DARWIN_UNIX03 */
	return (_pthread_setcancelstate_internal(state, oldstate, 0));
#endif /* __DARWIN_UNIX03 */

}



/*
 * Query/update the cancelability 'type' of a thread
 */
int
pthread_setcanceltype(int type, int *oldtype)
{
	pthread_t self = pthread_self();
	
#if __DARWIN_UNIX03
	if (__unix_conforming == 0)
		__unix_conforming = 1;
#endif /* __DARWIN_UNIX03 */

	if ((type != PTHREAD_CANCEL_DEFERRED) && 
	    (type != PTHREAD_CANCEL_ASYNCHRONOUS))
		return EINVAL;
	self = pthread_self();
	LOCK(self->lock);
	if (oldtype)
		*oldtype = self->cancel_state & _PTHREAD_CANCEL_TYPE_MASK;
	self->cancel_state &= ~_PTHREAD_CANCEL_TYPE_MASK;
	self->cancel_state |= type;
	UNLOCK(self->lock);
#if !__DARWIN_UNIX03
	_pthread_testcancel(self, 0);  /* See if we need to 'die' now... */
#endif /* __DARWIN_UNIX03 */
	return (0);
}

int 
pthread_sigmask(int how, const sigset_t * set, sigset_t * oset)
{
#if __DARWIN_UNIX03
	int err = 0;

	if (__pthread_sigmask(how, set, oset) == -1) {
		err = errno;
	} 
	return(err);
#else /* __DARWIN_UNIX03 */
	return(__pthread_sigmask(how, set, oset));
#endif /* __DARWIN_UNIX03 */
}

/*
int
sigwait(const sigset_t * set, int * sig)

moved to pthread_cancelable.c */
