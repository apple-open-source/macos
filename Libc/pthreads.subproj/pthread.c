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

#define __POSIX_LIB__
#include <assert.h>
#include <stdio.h>	/* For printf(). */
#include <stdlib.h>
#include <errno.h>	/* For __mach_errno_addr() prototype. */
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <machine/vmparam.h>
#include <mach/vm_statistics.h>

#include "pthread_internals.h"

/* Per-thread kernel support */
extern void _pthread_set_self(pthread_t);
extern void mig_init(int);

/* Needed to tell the malloc subsystem we're going multithreaded */
extern void set_malloc_singlethreaded(int);

/* Used when we need to call into the kernel with no reply port */
extern pthread_lock_t reply_port_lock;

/*
 * [Internal] stack support
 */

size_t _pthread_stack_size = 0;
int _spin_tries = 0;
#if !defined(__ppc__)
int _cpu_has_altivec = 0;
#endif

/* This global should be used (carefully) by anyone needing to know if a pthread has been
** created.
*/
int __is_threaded = 0;

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

extern mach_port_t thread_recycle_port;

#define STACK_LOWEST(sp)	((sp) & ~__pthread_stack_mask)
#define STACK_RESERVED		(sizeof (struct _pthread))

#ifdef STACK_GROWS_UP

/* The stack grows towards higher addresses:
   |struct _pthread|user stack---------------->|
   ^STACK_BASE     ^STACK_START
   ^STACK_SELF
   ^STACK_LOWEST  */
#define STACK_BASE(sp)		STACK_LOWEST(sp)
#define STACK_START(stack_low)	(STACK_BASE(stack_low) + STACK_RESERVED)
#define STACK_SELF(sp)		STACK_BASE(sp)

#else

/* The stack grows towards lower addresses:
   |<----------------user stack|struct _pthread|
   ^STACK_LOWEST               ^STACK_START    ^STACK_BASE
			       ^STACK_SELF  */

#define STACK_BASE(sp)		(((sp) | __pthread_stack_mask) + 1)
#define STACK_START(stack_low)	(STACK_BASE(stack_low) - STACK_RESERVED)
#define STACK_SELF(sp)		STACK_START(sp)

#endif

/* Set the base address to use as the stack pointer, before adjusting due to the ABI */

static int
_pthread_allocate_stack(pthread_attr_t *attrs, vm_address_t *stack)
{
    kern_return_t kr;
#if 1
    assert(attrs->stacksize >= PTHREAD_STACK_MIN);
    if (attrs->stackaddr != NULL) {
        assert(((vm_offset_t)(attrs->stackaddr) & (vm_page_size - 1)) == 0);
        *stack = (vm_address_t)attrs->stackaddr;
        return 0;
    }
    kr = vm_allocate(mach_task_self(), stack, attrs->stacksize + vm_page_size, VM_MAKE_TAG(VM_MEMORY_STACK)| TRUE);
    if (kr != KERN_SUCCESS) {
        return EAGAIN;
    }
#ifdef STACK_GROWS_UP
    /* The guard page is the page one higher than the stack */
    /* The stack base is at the lowest address */
    kr = vm_protect(mach_task_self(), *stack + attrs->stacksize, vm_page_size, FALSE, VM_PROT_NONE);
#else
    /* The guard page is at the lowest address */
    /* The stack base is the highest address */
    kr = vm_protect(mach_task_self(), *stack, vm_page_size, FALSE, VM_PROT_NONE);
    *stack += attrs->stacksize + vm_page_size;
#endif

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
# ifdef	STACK_GROWS_UP
			    kr = vm_protect(mach_task_self(),
					    lowest_stack+__pthread_stack_size,
					    __pthread_stack_size,
					    FALSE, VM_PROT_NONE);
# else	/* STACK_GROWS_UP */
			    kr = vm_protect(mach_task_self(),
					    lowest_stack,
					    __pthread_stack_size,
					    FALSE, VM_PROT_NONE);
			    lowest_stack += __pthread_stack_size;
# endif	/* STACK_GROWS_UP */
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
# ifdef	STACK_GROWS_UP
			    kr = vm_protect(mach_task_self(),
					    lowest_stack+__pthread_stack_size,
					    __pthread_stack_size,
					    FALSE, VM_PROT_NONE);
# else	/* STACK_GROWS_UP */
			    kr = vm_protect(mach_task_self(),
					    lowest_stack,
					    __pthread_stack_size,
					    FALSE, VM_PROT_NONE);
			    lowest_stack += __pthread_stack_size;
# endif	/* STACK_GROWS_UP */
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

static const size_t DEFAULT_STACK_SIZE = DFLSSIZ;
/*
 * Initialize a thread attribute structure to default values.
 */
int       
pthread_attr_init(pthread_attr_t *attr)
{
        attr->stacksize = DEFAULT_STACK_SIZE;
        attr->stackaddr = NULL;
	attr->sig = _PTHREAD_ATTR_SIG;
	attr->policy = _PTHREAD_DEFAULT_POLICY;
	attr->param.sched_priority = default_priority;
	attr->param.quantum = 10; /* quantum isn't public yet */
	attr->inherit = _PTHREAD_DEFAULT_INHERITSCHED;
	attr->detached = PTHREAD_CREATE_JOINABLE;
        attr->freeStackOnExit = TRUE;
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
                vm_address_t stack,
		const mach_port_t kernel_thread)
{
	int res;
	kern_return_t kern_res;
	res = ESUCCESS;
	do
	{
		memset(t, 0, sizeof(*t));
		t->stacksize = attrs->stacksize;
                t->stackaddr = (void *)stack;
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
		t->cancel_state = PTHREAD_CANCEL_ENABLE | PTHREAD_CANCEL_DEFERRED;
		t->cleanup_stack = (struct _pthread_handler_rec *)NULL;
		pthread_setschedparam(t, t->policy, &t->param);
		/* Create control semaphores */
		if (t->detached == PTHREAD_CREATE_JOINABLE)
		{
			PTHREAD_MACH_CALL(semaphore_create(mach_task_self(), 
						   &t->death, 
						   SYNC_POLICY_FIFO, 
						   0), kern_res);
			if (kern_res != KERN_SUCCESS)
			{
				printf("Can't create 'death' semaphore: %d\n", kern_res);
				res = EINVAL; /* Need better error here? */
				break;
			}
			PTHREAD_MACH_CALL(semaphore_create(mach_task_self(), 
						   &t->joiners, 
						   SYNC_POLICY_FIFO, 
						   0), kern_res);
			if (kern_res != KERN_SUCCESS)
			{
				printf("Can't create 'joiners' semaphore: %d\n", kern_res);
				res = EINVAL; /* Need better error here? */
				break;
			}
			t->num_joiners = 0;
		} else
		{
			t->death = MACH_PORT_NULL;
		}
	} while (0);
	return (res);
}

int
_pthread_is_threaded(void)
{
    return __is_threaded;
}

mach_port_t
pthread_mach_thread_np(pthread_t t)
{
    return t->kernel_thread;
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

static int       
_pthread_create_suspended(pthread_t *thread, 
	       const pthread_attr_t *attr,
	       void *(*start_routine)(void *), 
	       void *arg,
           int suspended)
{
	pthread_attr_t _attr, *attrs;
	vm_address_t stack;
	int res;
	pthread_t t;
	kern_return_t kern_res;
	mach_port_t kernel_thread;
	if ((attrs = (pthread_attr_t *)attr) == (pthread_attr_t *)NULL)
	{			/* Set up default paramters */
		attrs = &_attr;
		pthread_attr_init(attrs);
        } else if (attrs->sig != _PTHREAD_ATTR_SIG) {
            return EINVAL;
        }
	res = ESUCCESS;
	do
	{
		/* Allocate a stack for the thread */
                if ((res = _pthread_allocate_stack(attrs, &stack)) != 0) {
                    break;
                }
		t = (pthread_t)malloc(sizeof(struct _pthread));
		*thread = t;
		/* Create the Mach thread for this thread */
		PTHREAD_MACH_CALL(thread_create(mach_task_self(), &kernel_thread), kern_res);
		if (kern_res != KERN_SUCCESS)
		{
			printf("Can't create thread: %d\n", kern_res);
			res = EINVAL; /* Need better error here? */
			break;
		}
                if ((res = _pthread_create(t, attrs, stack, kernel_thread)) != 0)
		{
			break;
		}
		t->arg = arg;
		t->fun = start_routine;
		/* Now set it up to execute */
		_pthread_setup(t, _pthread_body, stack);
		/* Send it on it's way */
                set_malloc_singlethreaded(0);
		__is_threaded = 1;
        if (suspended == 0) {
            PTHREAD_MACH_CALL(thread_resume(kernel_thread), kern_res);
        }
		if (kern_res != KERN_SUCCESS)
		{
			printf("Can't resume thread: %d\n", kern_res);
			res = EINVAL; /* Need better error here? */
			break;
		}
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
	kern_return_t kern_res;
	int num_joiners;
	mach_port_t death;
	if (thread->sig == _PTHREAD_SIG)
	{
		LOCK(thread->lock);
		if (thread->detached == PTHREAD_CREATE_JOINABLE)
		{
			thread->detached = PTHREAD_CREATE_DETACHED;
			num_joiners = thread->num_joiners;
			death = thread->death;
			thread->death = MACH_PORT_NULL;
			UNLOCK(thread->lock);
			if (num_joiners > 0)
			{
				/* Wake up a joiner */
				PTHREAD_MACH_CALL(semaphore_signal(thread->joiners), kern_res);
			}
			/* Destroy 'control' semaphores */
			PTHREAD_MACH_CALL(semaphore_destroy(mach_task_self(),
						    thread->joiners), kern_res);
			PTHREAD_MACH_CALL(semaphore_destroy(mach_task_self(),
						    death), kern_res);
			return (ESUCCESS);
		} else if (thread->detached == _PTHREAD_EXITED) {
			UNLOCK(thread->lock);
			pthread_join(thread, NULL);
			return ESUCCESS;
		} else
		{
			UNLOCK(thread->lock);
			return (EINVAL);
		}
	} else
	{
		return (ESRCH); /* Not a valid thread */
	}
}

/* Announce that there is a thread ready to be reclaimed for pthread_create */
/* or terminated by pthread_exit. If the thread is reused, it will have its */
/* thread state set and will continue in the thread body function. If it is */
/* terminated, it will be yanked out from under the mach_msg() call. */

static void _pthread_become_available(pthread_t thread) {
	mach_msg_empty_rcv_t msg = { { 0 } };
	kern_return_t ret;

	if (thread->reply_port == MACH_PORT_NULL) {
		thread->reply_port = mach_reply_port();
	}
	msg.header.msgh_size = sizeof msg - sizeof msg.trailer;
	msg.header.msgh_remote_port = thread_recycle_port;
	msg.header.msgh_local_port = MACH_PORT_NULL; 
	msg.header.msgh_id = (int)thread;
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
	ret = mach_msg(&msg.header, MACH_SEND_MSG | MACH_RCV_MSG,
			msg.header.msgh_size, sizeof msg,
			thread->reply_port, MACH_MSG_TIMEOUT_NONE,
			MACH_PORT_NULL);
	while (1) {
		ret = thread_suspend(thread->kernel_thread);
	}
	/* We should never get here */
}

/* Check to see if any threads are available. Return immediately */

static kern_return_t _pthread_check_for_available_threads(mach_msg_empty_rcv_t *msg) {
	return mach_msg(&msg->header, MACH_RCV_MSG|MACH_RCV_TIMEOUT, 0,
			sizeof(mach_msg_empty_rcv_t), thread_recycle_port, 0,
			MACH_PORT_NULL);
}

/* Terminate all available threads and deallocate their stacks */
static void _pthread_reap_threads(void) {
	kern_return_t ret;
	mach_msg_empty_rcv_t msg = { { 0 } };
	while((ret = _pthread_check_for_available_threads(&msg)) == KERN_SUCCESS) {
		pthread_t th = (pthread_t)msg.header.msgh_id;
		mach_port_t kernel_thread = th->kernel_thread;
		mach_port_t reply_port = th->reply_port; 
		vm_size_t size = (vm_size_t)th->stacksize + vm_page_size;
		vm_address_t addr = (vm_address_t)th->stackaddr;
#if !defined(STACK_GROWS_UP)
		addr -= size;
#endif
		ret = thread_terminate(kernel_thread);
		if (ret != KERN_SUCCESS) {
			fprintf(stderr, "thread_terminate() failed: %s\n",
				mach_error_string(ret));
		}
		ret = mach_port_destroy(mach_task_self(), reply_port);
		if (ret != KERN_SUCCESS) {
			fprintf(stderr,
				"mach_port_destroy(thread_reply) failed: %s\n",
				mach_error_string(ret));
		}
		if (th->freeStackOnExit) {
			ret = vm_deallocate(mach_task_self(), addr, size);
			if (ret != KERN_SUCCESS) {
				fprintf(stderr,
					"vm_deallocate(stack) failed: %s\n",
					mach_error_string(ret));
			}
		}
		free(th);
	}
	assert(ret == MACH_RCV_TIMED_OUT);
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
	pthread_t self = pthread_self();
        struct _pthread_handler_rec *handler;
	kern_return_t kern_res;
	int num_joiners;
	while ((handler = self->cleanup_stack) != 0)
	{
		(handler->routine)(handler->arg);
		self->cleanup_stack = handler->next;
	}
	_pthread_tsd_cleanup(self);
	LOCK(self->lock);
	if (self->detached == PTHREAD_CREATE_JOINABLE)
	{
		self->detached = _PTHREAD_EXITED;
		self->exit_value = value_ptr;
		num_joiners = self->num_joiners;
		UNLOCK(self->lock);
		if (num_joiners > 0)
		{
			/* POSIX says that multiple pthread_join() calls on */
			/* the same thread are undefined so we just wake up */
			/* the first one to join */
			PTHREAD_MACH_CALL(semaphore_signal(self->joiners), kern_res);
		}
		do {
			PTHREAD_MACH_CALL(semaphore_wait(self->death), kern_res);
		} while (kern_res == KERN_ABORTED);
	} else
		UNLOCK(self->lock);
	/* Destroy thread & reclaim resources */
	if (self->death)
	{
		PTHREAD_MACH_CALL(semaphore_destroy(mach_task_self(), self->joiners), kern_res);
		PTHREAD_MACH_CALL(semaphore_destroy(mach_task_self(), self->death), kern_res);
	}
        if (self->detached == _PTHREAD_CREATE_PARENT) {
		exit((int)(self->exit_value));
	}

	_pthread_reap_threads();
        
	_pthread_become_available(self);
}

/*
 * Wait for a thread to terminate and obtain its exit value.
 */
int       
pthread_join(pthread_t thread, 
	     void **value_ptr)
{
	kern_return_t kern_res;
	if (thread->sig == _PTHREAD_SIG)
	{
		LOCK(thread->lock);
		if (thread->detached == PTHREAD_CREATE_JOINABLE)
		{
			thread->num_joiners++;
			UNLOCK(thread->lock);
			do {
				PTHREAD_MACH_CALL(semaphore_wait(thread->joiners), kern_res);
			 } while (kern_res == KERN_ABORTED);
			LOCK(thread->lock);
			thread->num_joiners--;
		}
		if (thread->detached == _PTHREAD_EXITED)
		{
			if (thread->num_joiners == 0)
			{	/* Give the result to this thread */
				if (value_ptr)
				{
					*value_ptr = thread->exit_value;
				}
				UNLOCK(thread->lock);
				PTHREAD_MACH_CALL(semaphore_signal(thread->death), kern_res);
				return (ESUCCESS);
			} else
			{	/* This 'joiner' missed the catch! */
				UNLOCK(thread->lock);
				return (ESRCH);
			}
		} else
		{		/* The thread has become anti-social! */
			UNLOCK(thread->lock);
			return (EINVAL);
		}
	} else
	{
		return (ESRCH); /* Not a valid thread */
	}
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
            thread->policy = policy;
            thread->param = *param;
		ret = thread_policy(thread->kernel_thread, policy, base, count, TRUE);
		if (ret != KERN_SUCCESS)
		{
			return (EINVAL);
		}
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
	LOCK(once_control->lock);
	if (once_control->sig == _PTHREAD_ONCE_SIG_init)
	{
		(*init_routine)();
		once_control->sig = _PTHREAD_ONCE_SIG;
	}
	UNLOCK(once_control->lock);
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
	*oldstate = self->cancel_state & _PTHREAD_CANCEL_STATE_MASK;
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
	*oldtype = self->cancel_state & _PTHREAD_CANCEL_TYPE_MASK;
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

/*
 * Perform package initialization - called automatically when application starts
 */

/* We'll implement this when the main thread is a pthread */
/* Use the local _pthread struct to avoid malloc before our MiG reply port is set */

static struct _pthread _thread = {0};

static int
pthread_init(void)
{
	pthread_attr_t _attr, *attrs;
        pthread_t thread;
	kern_return_t kr;
	host_basic_info_data_t basic_info;
	host_priority_info_data_t priority_info;
	host_info_t info;
	host_flavor_t flavor;
	mach_msg_type_number_t count;
	int mib[2];
	size_t len;
	int hasvectorunit, numcpus;

        count = HOST_PRIORITY_INFO_COUNT;
	info = (host_info_t)&priority_info;
	flavor = HOST_PRIORITY_INFO;
	kr = host_info(mach_host_self(), flavor, info, &count);
        if (kr != KERN_SUCCESS)
                printf("host_info failed (%d); probably need privilege.\n", kr);
        else {
		default_priority = priority_info.user_priority;
            min_priority = priority_info.minimum_priority;
            max_priority = priority_info.maximum_priority;
	}
	attrs = &_attr;
	pthread_attr_init(attrs);
	_pthread_set_self(&_thread);

        _pthread_create(&_thread, attrs, USRSTACK, mach_thread_self());
        thread = &_thread;
        thread->detached = _PTHREAD_CREATE_PARENT;

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
		kr = host_info(mach_host_self(), flavor, info, &count);
		if (kr != KERN_SUCCESS)
			printf("host_info failed (%d)\n", kr);
		else {
			if (basic_info.avail_cpus > 1)
				_spin_tries = MP_SPIN_TRIES;
			/* This is a crude test */
			if (basic_info.cpu_subtype >= CPU_SUBTYPE_POWERPC_7400) 
				_cpu_has_altivec = 1;
		}
	}
	mib[0] = CTL_HW;
	mib[1] = HW_VECTORUNIT;
	len = sizeof(hasvectorunit);
	if (sysctl(mib, 2, &hasvectorunit, &len, NULL, 0) == 0) {
		_cpu_has_altivec = hasvectorunit;
	}
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

__private_extern__ void _pthread_fork_child(void) {
	/* Just in case somebody had it locked... */
	UNLOCK(sem_pool_lock);
	sem_pool_reset();
}

