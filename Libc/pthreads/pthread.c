/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <assert.h>
#include <stdio.h>	/* For printf(). */
#include <stdlib.h>
#include <errno.h>	/* For __mach_errno_addr() prototype. */
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/syscall.h>
#include <machine/vmparam.h>
#include <mach/vm_statistics.h>
#define	__APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>

__private_extern__ struct __pthread_list __pthread_head = LIST_HEAD_INITIALIZER(&__pthread_head);

/* Per-thread kernel support */
extern void _pthread_set_self(pthread_t);
extern void mig_init(int);

/* Get CPU capabilities from the kernel */
__private_extern__ void _init_cpu_capabilities(void);

/* Needed to tell the malloc subsystem we're going multithreaded */
extern void set_malloc_singlethreaded(int);

/* Used when we need to call into the kernel with no reply port */
extern pthread_lock_t reply_port_lock;

/* We'll implement this when the main thread is a pthread */
/* Use the local _pthread struct to avoid malloc before our MiG reply port is set */
static struct _pthread _thread = {0};

/* This global should be used (carefully) by anyone needing to know if a 
** pthread has been created.
*/
int __is_threaded = 0;
/* _pthread_count is protected by _pthread_list_lock */
static int _pthread_count = 1;

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

/*
 * [Internal] stack support
 */
size_t _pthread_stack_size = 0;
#define STACK_LOWEST(sp)	((sp) & ~__pthread_stack_mask)
#define STACK_RESERVED		(sizeof (struct _pthread))


/* The stack grows towards lower addresses:
   |<----------------user stack|struct _pthread|
   ^STACK_LOWEST               ^STACK_START    ^STACK_BASE
			       ^STACK_SELF  */

#define STACK_BASE(sp)		(((sp) | __pthread_stack_mask) + 1)
#define STACK_START(stack_low)	(STACK_BASE(stack_low) - STACK_RESERVED)
#define STACK_SELF(sp)		STACK_START(sp)

#if defined(__ppc__)
static const vm_address_t PTHREAD_STACK_HINT = 0xF0000000;
#elif defined(__i386__)
static const vm_address_t PTHREAD_STACK_HINT = 0xB0000000;
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
    size_t guardsize;
#if 1
    assert(attrs->stacksize >= PTHREAD_STACK_MIN);
    if (attrs->stackaddr != NULL) {
	/* No guard pages setup in this case */
        assert(((vm_address_t)(attrs->stackaddr) & (vm_page_size - 1)) == 0);
        *stack = attrs->stackaddr;
         return 0;
    }

   guardsize = attrs->guardsize;
    *((vm_address_t *)stack) = PTHREAD_STACK_HINT;
    kr = vm_map(mach_task_self(), (vm_address_t *)stack,
    			attrs->stacksize + guardsize,
    			vm_page_size-1,
    			VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE , MEMORY_OBJECT_NULL,
    			0, FALSE, VM_PROT_DEFAULT, VM_PROT_ALL,
    			VM_INHERIT_DEFAULT);
    if (kr != KERN_SUCCESS)
    	kr = vm_allocate(mach_task_self(),
    					(vm_address_t *)stack, attrs->stacksize + guardsize,
    					VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) {
        return EAGAIN;
    }
     /* The guard page is at the lowest address */
     /* The stack base is the highest address */
    if (guardsize)
    	kr = vm_protect(mach_task_self(), (vm_address_t)*stack, guardsize, FALSE, VM_PROT_NONE);
    *stack += attrs->stacksize + guardsize;

#else
    vm_address_t cur_stack = (vm_address_t)0;
	if (free_stacks == 0)
	{
	    /* Allocating guard pages is done by doubling
	     * the actual stack size, since STACK_BASE() needs
	     * to have stacks aligned on stack_size. Allocating just 
	     * one page takes as much memory as allocating more pages
	     * since it will remain one entry in the vm map.
	     * Besides, allocating more than one page allows tracking the
	     * overflow pattern when the overflow is bigger than one page.
	     */
#ifndef	NO_GUARD_PAGES
# define	GUARD_SIZE(a)	(2*(a))
# define	GUARD_MASK(a)	(((a)<<1) | 1)
#else
# define	GUARD_SIZE(a)	(a)
# define	GUARD_MASK(a)	(a)
#endif
		while (lowest_stack > GUARD_SIZE(__pthread_stack_size))
		{
			lowest_stack -= GUARD_SIZE(__pthread_stack_size);
			/* Ensure stack is there */
			kr = vm_allocate(mach_task_self(),
					 &lowest_stack,
					 GUARD_SIZE(__pthread_stack_size),
					 FALSE);
#ifndef	NO_GUARD_PAGES
			if (kr == KERN_SUCCESS) {
			    kr = vm_protect(mach_task_self(),
					    lowest_stack,
					    __pthread_stack_size,
					    FALSE, VM_PROT_NONE);
			    lowest_stack += __pthread_stack_size;
			    if (kr == KERN_SUCCESS)
				break;
			}
#else
			if (kr == KERN_SUCCESS)
			    break;
#endif
		}
		if (lowest_stack > 0)
			free_stacks = (vm_address_t *)lowest_stack;
		else
		{
			/* Too bad.  We'll just have to take what comes.
			   Use vm_map instead of vm_allocate so we can
			   specify alignment.  */
			kr = vm_map(mach_task_self(), &lowest_stack,
				    GUARD_SIZE(__pthread_stack_size),
				    GUARD_MASK(__pthread_stack_mask),
				    TRUE /* anywhere */, MEMORY_OBJECT_NULL,
				    0, FALSE, VM_PROT_DEFAULT, VM_PROT_ALL,
				    VM_INHERIT_DEFAULT);
			/* This really shouldn't fail and if it does I don't
			   know what to do.  */
#ifndef	NO_GUARD_PAGES
			if (kr == KERN_SUCCESS) {
			    kr = vm_protect(mach_task_self(),
					    lowest_stack,
					    __pthread_stack_size,
					    FALSE, VM_PROT_NONE);
			    lowest_stack += __pthread_stack_size;
			}
#endif
			free_stacks = (vm_address_t *)lowest_stack;
			lowest_stack = 0;
		}
		*free_stacks = 0; /* No other free stacks */
	}
	cur_stack = STACK_START((vm_address_t) free_stacks);
	free_stacks = (vm_address_t *)*free_stacks;
	cur_stack = _adjust_sp(cur_stack); /* Machine dependent stack fudging */
#endif
        return 0;
}

static pthread_attr_t _pthread_attr_default = {0};

/*
 * Destroy a thread attribute structure
 */
int       
pthread_attr_destroy(pthread_attr_t *attr)
{
	if (attr->sig == _PTHREAD_ATTR_SIG)
	{
		return (ESUCCESS);
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
		return (ESUCCESS);
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
		return (ESUCCESS);
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
		return (ESUCCESS);
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
		return (ESUCCESS);
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
        attr->freeStackOnExit = TRUE;
	attr->guardsize = vm_page_size;
	return (ESUCCESS);
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
			return (ESUCCESS);
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
			return (ESUCCESS);
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
		return (ESUCCESS);
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
			return (ESUCCESS);
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
            return (ESUCCESS);
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
pthread_attr_getscope(pthread_attr_t *attr,
                            int *scope)
{
    if (attr->sig == _PTHREAD_ATTR_SIG) {
        *scope = PTHREAD_SCOPE_SYSTEM;
        return (ESUCCESS);
    }
    return (EINVAL); /* Not an attribute structure! */
}

/* Get the base stack address of the given thread */
int
pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
    if (attr->sig == _PTHREAD_ATTR_SIG) {
        *stackaddr = attr->stackaddr;
        return (ESUCCESS);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int
pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
    if ((attr->sig == _PTHREAD_ATTR_SIG) && (((vm_offset_t)stackaddr & (vm_page_size - 1)) == 0)) {
        attr->stackaddr = stackaddr;
        attr->freeStackOnExit = FALSE;
        return (ESUCCESS);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int
pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
    if (attr->sig == _PTHREAD_ATTR_SIG) {
        *stacksize = attr->stacksize;
        return (ESUCCESS);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int
pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
    if ((attr->sig == _PTHREAD_ATTR_SIG) && ((stacksize % vm_page_size) == 0) && (stacksize >= PTHREAD_STACK_MIN)) {
        attr->stacksize = stacksize;
        return (ESUCCESS);
    } else {
        return (EINVAL); /* Not an attribute structure! */
    }
}

int
pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t * stacksize)
{
    if (attr->sig == _PTHREAD_ATTR_SIG) {
	u_int32_t addr = (u_int32_t)attr->stackaddr;

	addr -= attr->stacksize;
	*stackaddr = (void *)addr;
        *stacksize = attr->stacksize;
        return (ESUCCESS);
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
	(((vm_offset_t)stackaddr & (vm_page_size - 1)) == 0) && 
	((stacksize % vm_page_size) == 0) && (stacksize >= PTHREAD_STACK_MIN)) {
		u_int32_t addr = (u_int32_t)stackaddr;

		addr += stacksize;
		attr->stackaddr = (void *)addr;
        	attr->stacksize = stacksize;
        	attr->freeStackOnExit = FALSE;
        	return (ESUCCESS);
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
            return (ESUCCESS);
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
        return (ESUCCESS);
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
    pthread_exit((self->fun)(self->arg));
}

int
_pthread_create(pthread_t t,
		const pthread_attr_t *attrs,
                void *stack,
		const mach_port_t kernel_thread)
{
	int res;
	res = ESUCCESS;
	
	do
	{
		memset(t, 0, sizeof(*t));
		t->tsd[0] = t;
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
		t->plist.le_next = (struct _pthread *)0;
		t->plist.le_prev = (struct _pthread **)0;
		t->cancel_state = PTHREAD_CANCEL_ENABLE | PTHREAD_CANCEL_DEFERRED;
		t->cleanup_stack = (struct _pthread_handler_rec *)NULL;
		t->death = SEMAPHORE_NULL;

		if (kernel_thread != MACH_PORT_NULL)
			pthread_setschedparam(t, t->policy, &t->param);
	} while (0);
	return (res);
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
    thread_t kernel_thread;

    /* Wait for the creator to initialize it */
    while ((kernel_thread = t->kernel_thread) == MACH_PORT_NULL)
	sched_yield();

    return kernel_thread;
}

size_t
pthread_get_stacksize_np(pthread_t t)
{
    return t->stacksize;
}

void *
pthread_get_stackaddr_np(pthread_t t)
{
    return t->stackaddr;
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
	res = ESUCCESS;

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
		LIST_INSERT_HEAD(&__pthread_head, t, plist);
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
    return _pthread_create_suspended(thread, attr, start_routine, arg, 0);
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
	if (thread->sig == _PTHREAD_SIG)
	{
		LOCK(thread->lock);
		if (thread->detached & PTHREAD_CREATE_JOINABLE)
		{
			if (thread->detached & _PTHREAD_EXITED) {
				UNLOCK(thread->lock);
				pthread_join(thread, NULL);
				return ESUCCESS;
			} else {
				semaphore_t death = thread->death;

				thread->detached &= ~PTHREAD_CREATE_JOINABLE;
				thread->detached |= PTHREAD_CREATE_DETACHED;
				UNLOCK(thread->lock);
				if (death)
					(void) semaphore_signal(death);
				return (ESUCCESS);
			}
		} else {
			UNLOCK(thread->lock);
			return (EINVAL);
		}
	} else {
		return (ESRCH); /* Not a valid thread */
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
	
	if ((sig < 0) || (sig > NSIG))
		return(EINVAL);

	if (th && (th->sig == _PTHREAD_SIG)) {
		error = __pthread_kill(pthread_mach_thread_np(th), sig);
		if (error == -1)
			error = errno;
		return(error);
	}
	else
		return(ESRCH);
}

/* Announce that there are pthread resources ready to be reclaimed in a */
/* subsequent pthread_exit or reaped by pthread_join. In either case, the Mach */
/* thread underneath is terminated right away. */
static
void _pthread_become_available(pthread_t thread, mach_port_t kernel_thread) {
	mach_msg_empty_rcv_t msg;
	kern_return_t ret;

	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND,
					      MACH_MSG_TYPE_MOVE_SEND);
	msg.header.msgh_size = sizeof msg - sizeof msg.trailer;
	msg.header.msgh_remote_port = thread_recycle_port;
	msg.header.msgh_local_port = kernel_thread; 
	msg.header.msgh_id = (int)thread;
	ret = mach_msg_send(&msg.header);
	assert(ret == MACH_MSG_SUCCESS);
}

/* Reap the resources for available threads */
static
int _pthread_reap_thread(pthread_t th, mach_port_t kernel_thread, void **value_ptr) {
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

	if (th != &_thread)
		free(th);

	return ESUCCESS;
}

static
void _pthread_reap_threads(void)
{
	mach_msg_empty_rcv_t msg;
	kern_return_t ret;

	ret = mach_msg(&msg.header, MACH_RCV_MSG|MACH_RCV_TIMEOUT, 0,
			sizeof(mach_msg_empty_rcv_t), thread_recycle_port, 
			MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	while (ret == MACH_MSG_SUCCESS) {
		mach_port_t kernel_thread = msg.header.msgh_remote_port;
		pthread_t thread = (pthread_t)msg.header.msgh_id;

		if (_pthread_reap_thread(thread, kernel_thread, (void **)0) == EAGAIN)
		{
			/* not dead yet, put it back for someone else to reap, stop here */
			_pthread_become_available(thread, kernel_thread);
			return;
		}
		ret = mach_msg(&msg.header, MACH_RCV_MSG|MACH_RCV_TIMEOUT, 0,
				sizeof(mach_msg_empty_rcv_t), thread_recycle_port, 
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
void 
pthread_exit(void *value_ptr)
{
	struct _pthread_handler_rec *handler;
	pthread_t self = pthread_self();
	kern_return_t kern_res;
	int thread_count;

	/* Make this thread not to receive any signals */
	syscall(331,1);

	while ((handler = self->cleanup_stack) != 0)
	{
		(handler->routine)(handler->arg);
		self->cleanup_stack = handler->next;
	}
	_pthread_tsd_cleanup(self);

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
		LIST_REMOVE(self, plist);
		thread_count = --_pthread_count;
		UNLOCK(_pthread_list_lock);
		/* with no joiner, we let become available consume our cached ref */
		_pthread_become_available(self, pthread_mach_thread_np(self));
	}

	if (thread_count <= 0)
		exit(0);

	/* Use a new reference to terminate ourselves. Should never return. */
	PTHREAD_MACH_CALL(thread_terminate(mach_thread_self()), kern_res);
	fprintf(stderr, "thread_terminate(mach_thread_self()) failed: %s\n",
			mach_error_string(kern_res));
	abort();
}

/*
 * Wait for a thread to terminate and obtain its exit value.
 */
int       
pthread_join(pthread_t thread, 
	     void **value_ptr)
{
	kern_return_t kern_res;
	int res = ESUCCESS;

	if (thread->sig == _PTHREAD_SIG)
	{
		semaphore_t death = new_sem_from_pool(); /* in case we need it */

		LOCK(thread->lock);
		if ((thread->detached & PTHREAD_CREATE_JOINABLE) &&
			thread->death == SEMAPHORE_NULL)
		{
			pthread_t self = pthread_self();

			assert(thread->joiner == NULL);
			if (thread != self && (self == NULL || self->joiner != thread))
			{
				int already_exited = (thread->detached & _PTHREAD_EXITED);

				thread->death = death;
				thread->joiner = self;
				UNLOCK(thread->lock);

				if (!already_exited)
				{
					/* Wait for it to signal... */ 
					do {
						PTHREAD_MACH_CALL(semaphore_wait(death), kern_res);
					} while (kern_res != KERN_SUCCESS);
				}

				LOCK(_pthread_list_lock);
				LIST_REMOVE(thread, plist);
				UNLOCK(_pthread_list_lock);
				/* ... and wait for it to really be dead */
				while ((res = _pthread_reap_thread(thread,
							thread->kernel_thread,
							value_ptr)) == EAGAIN)
				{
					sched_yield();
				}
			} else {
				UNLOCK(thread->lock);
				res = EDEADLK;
			}
		} else {
			UNLOCK(thread->lock);
			res = EINVAL;
		}
		restore_sem_to_pool(death);
		return res;
	}
	return ESRCH;
}

/*
 * Get the scheduling policy and scheduling paramters for a thread.
 */
int       
pthread_getschedparam(pthread_t thread, 
		      int *policy,
		      struct sched_param *param)
{
	if (thread->sig == _PTHREAD_SIG)
	{
            *policy = thread->policy;
            *param = thread->param;
            return (ESUCCESS);
	} else
	{
		return (ESRCH);  /* Not a valid thread structure */
	}
}

/*
 * Set the scheduling policy and scheduling paramters for a thread.
 */
int       
pthread_setschedparam(pthread_t thread, 
		      int policy,
		      const struct sched_param *param)
{
	policy_base_data_t bases;
	policy_base_t base;
	mach_msg_type_number_t count;
	kern_return_t ret;

	if (thread->sig == _PTHREAD_SIG)
	{
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
		ret = thread_policy(pthread_mach_thread_np(thread), policy, base, count, TRUE);
		if (ret != KERN_SUCCESS)
		{
			return (EINVAL);
		}
            	thread->policy = policy;
            	thread->param = *param;
		return (ESUCCESS);
	} else
	{
		return (ESRCH);  /* Not a valid thread structure */
	}
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
 * Execute a function exactly one time in a thread-safe fashion.
 */
int       
pthread_once(pthread_once_t *once_control, 
	     void (*init_routine)(void))
{
	_spin_lock(&once_control->lock);
	if (once_control->sig == _PTHREAD_ONCE_SIG_init)
	{
		(*init_routine)();
		once_control->sig = _PTHREAD_ONCE_SIG;
	}
	_spin_unlock(&once_control->lock);
	return (ESUCCESS);  /* Spec defines no possible errors! */
}

/*
 * Cancel a thread
 */
int
pthread_cancel(pthread_t thread)
{
	if (thread->sig == _PTHREAD_SIG)
	{
		thread->cancel_state |= _PTHREAD_CANCEL_PENDING;
		return (ESUCCESS);
	} else
	{
		return (ESRCH);
	}
}

/*
 * Insert a cancellation point in a thread.
 */
static void
_pthread_testcancel(pthread_t thread)
{
	LOCK(thread->lock);
	if ((thread->cancel_state & (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING)) == 
	    (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING))
	{
		UNLOCK(thread->lock);
		pthread_exit(0);
	}
	UNLOCK(thread->lock);
}

void
pthread_testcancel(void)
{
	pthread_t self = pthread_self();
	_pthread_testcancel(self);
}

/*
 * Query/update the cancelability 'state' of a thread
 */
int
pthread_setcancelstate(int state, int *oldstate)
{
	pthread_t self = pthread_self();
	int err = ESUCCESS;
	LOCK(self->lock);
	if (oldstate)
		*oldstate = self->cancel_state & ~_PTHREAD_CANCEL_STATE_MASK;
	if ((state == PTHREAD_CANCEL_ENABLE) || (state == PTHREAD_CANCEL_DISABLE))
	{
		self->cancel_state = (self->cancel_state & _PTHREAD_CANCEL_STATE_MASK) | state;
	} else
	{
		err = EINVAL;
	}
	UNLOCK(self->lock);
	_pthread_testcancel(self);  /* See if we need to 'die' now... */
	return (err);
}

/*
 * Query/update the cancelability 'type' of a thread
 */
int
pthread_setcanceltype(int type, int *oldtype)
{
	pthread_t self = pthread_self();
	int err = ESUCCESS;
	LOCK(self->lock);
	if (oldtype)
		*oldtype = self->cancel_state & ~_PTHREAD_CANCEL_TYPE_MASK;
	if ((type == PTHREAD_CANCEL_DEFERRED) || (type == PTHREAD_CANCEL_ASYNCHRONOUS))
	{
		self->cancel_state = (self->cancel_state & _PTHREAD_CANCEL_TYPE_MASK) | type;
	} else
	{
		err = EINVAL;
	}
	UNLOCK(self->lock);
	_pthread_testcancel(self);  /* See if we need to 'die' now... */
	return (err);
}

int
pthread_getconcurrency(void)
{
	return(pthread_concurrency);
}

int
pthread_setconcurrency(int new_level)
{
	pthread_concurrency = new_level;
	return(ESUCCESS);
}

/*
 * Perform package initialization - called automatically when application starts
 */

static int
pthread_init(void)
{
	pthread_attr_t *attrs;
        pthread_t thread;
	kern_return_t kr;
	host_basic_info_data_t basic_info;
	host_priority_info_data_t priority_info;
	host_info_t info;
	host_flavor_t flavor;
	host_t host;
	mach_msg_type_number_t count;
	int mib[2];
	size_t len;
	int numcpus;

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

	LIST_INIT(&__pthread_head);
	LOCK_INIT(_pthread_list_lock);
	thread = &_thread;
	LIST_INSERT_HEAD(&__pthread_head, thread, plist);
	_pthread_set_self(thread);
	_pthread_create(thread, attrs, (void *)USRSTACK, mach_thread_self());
	thread->detached = PTHREAD_CREATE_JOINABLE|_PTHREAD_CREATE_PARENT;

        /* See if we're on a multiprocessor and set _spin_tries if so.  */
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	len = sizeof(numcpus);
	if (sysctl(mib, 2, &numcpus, &len, NULL, 0) == 0) {
		if (numcpus > 1) {
			_spin_tries = MP_SPIN_TRIES;
		}
	} else {
		count = HOST_BASIC_INFO_COUNT;
		info = (host_info_t)&basic_info;
		flavor = HOST_BASIC_INFO;
		kr = host_info(host, flavor, info, &count);
		if (kr != KERN_SUCCESS)
			printf("host_info failed (%d)\n", kr);
		else {
			if (basic_info.avail_cpus > 1)
				_spin_tries = MP_SPIN_TRIES;
		}
	}

	mach_port_deallocate(mach_task_self(), host);
    
	_init_cpu_capabilities();

#if defined(__ppc__)
	
	/* Use fsqrt instruction in sqrt() if available. */
    if (_cpu_capabilities & kHasFsqrt) {
        extern size_t hw_sqrt_len;
        extern double sqrt( double );
        extern double hw_sqrt( double );
        extern void sys_icache_invalidate(void *, size_t);

        memcpy ( (void *)sqrt, (void *)hw_sqrt, hw_sqrt_len );
        sys_icache_invalidate((void *)sqrt, hw_sqrt_len);
    }
#endif
    
	mig_init(1);		/* enable multi-threaded mig interfaces */
	return 0;
}

int sched_yield(void)
{
    swtch_pri(0);
    return 0;
}

/* This is the "magic" that gets the initialization routine called when the application starts */
int (*_cthread_init_routine)(void) = pthread_init;

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
	LIST_INIT(&__pthread_head);
	LOCK_INIT(_pthread_list_lock);
	LIST_INSERT_HEAD(&__pthread_head, p, plist);
	_pthread_count = 1;
}

