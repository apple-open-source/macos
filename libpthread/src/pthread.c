/*
 * Copyright (c) 2000-2013 Apple Inc. All rights reserved.
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

#include "internal.h"
#include "private.h"
#include "workqueue_private.h"
#include "introspection_private.h"
#include "qos_private.h"

#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <machine/vmparam.h>
#define	__APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>
#include <libkern/OSAtomic.h>

#include <_simple.h>
#include <platform/string.h>
#include <platform/compat.h>

extern int __sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
                    void *newp, size_t newlen);
extern void __exit(int) __attribute__((noreturn));

static void (*exitf)(int) = __exit;
__private_extern__ void* (*_pthread_malloc)(size_t) = NULL;
__private_extern__ void (*_pthread_free)(void *) = NULL;

//
// Global variables
//

// This global should be used (carefully) by anyone needing to know if a
// pthread (other than the main thread) has been created.
int __is_threaded = 0;

int __unix_conforming = 0;

// _pthread_list_lock protects _pthread_count, access to the __pthread_head
// list, and the parentcheck, childrun and childexit flags of the pthread
// structure. Externally imported by pthread_cancelable.c.
__private_extern__ pthread_lock_t _pthread_list_lock = LOCK_INITIALIZER;
__private_extern__ struct __pthread_list __pthread_head = TAILQ_HEAD_INITIALIZER(__pthread_head);
static int _pthread_count = 1;

#if PTHREAD_LAYOUT_SPI

const struct pthread_layout_offsets_s pthread_layout_offsets = {
	.plo_version = 1,
	.plo_pthread_tsd_base_offset = offsetof(struct _pthread, tsd),
	.plo_pthread_tsd_base_address_offset = 0,
	.plo_pthread_tsd_entry_size = sizeof(((struct _pthread *)NULL)->tsd[0]),
};

#endif // PTHREAD_LAYOUT_SPI

//
// Static variables
//

// Mach message notification that a thread needs to be recycled.
typedef struct _pthread_reap_msg_t {
	mach_msg_header_t header;
	pthread_t thread;
	mach_msg_trailer_t trailer;
} pthread_reap_msg_t;

#define pthreadsize ((size_t)mach_vm_round_page(sizeof(struct _pthread)))
static pthread_attr_t _pthread_attr_default = {0};
static struct _pthread _thread = {0};

static int default_priority;
static int max_priority;
static int min_priority;
static int pthread_concurrency;

// work queue support data
static void (*__libdispatch_workerfunction)(pthread_priority_t) = NULL;
static int __libdispatch_offset;

// supported feature set
int __pthread_supported_features;

//
// Function prototypes
//

// pthread primitives
static int _pthread_allocate(pthread_t *thread, const pthread_attr_t *attrs, void **stack);
static int _pthread_deallocate(pthread_t t);

static void _pthread_terminate(pthread_t t);

static void _pthread_struct_init(pthread_t t,
	const pthread_attr_t *attrs,
	void *stack,
	size_t stacksize,
	int kernalloc);

extern void _pthread_set_self(pthread_t);

static void _pthread_dealloc_reply_port(pthread_t t);

static inline void __pthread_add_thread(pthread_t t, bool parent);
static inline int __pthread_remove_thread(pthread_t t, bool child, bool *should_exit);

static int _pthread_find_thread(pthread_t thread);

static void _pthread_exit(pthread_t self, void *value_ptr) __dead2;
static void _pthread_setcancelstate_exit(pthread_t self, void  *value_ptr, int conforming);

static inline void _pthread_introspection_thread_create(pthread_t t, bool destroy);
static inline void _pthread_introspection_thread_start(pthread_t t);
static inline void _pthread_introspection_thread_terminate(pthread_t t, void *freeaddr, size_t freesize, bool destroy);
static inline void _pthread_introspection_thread_destroy(pthread_t t);

extern void start_wqthread(pthread_t self, mach_port_t kport, void *stackaddr, void *unused, int reuse);
extern void thread_start(pthread_t self, mach_port_t kport, void *(*fun)(void *), void * funarg, size_t stacksize, unsigned int flags);

void pthread_workqueue_atfork_child(void);

static bool __workq_newapi;

/* Compatibility: previous pthread API used WORKQUEUE_OVERCOMMIT to request overcommit threads from
 * the kernel. This definition is kept here, in userspace only, to perform the compatibility shimm
 * from old API requests to the new kext conventions.
 */
#define WORKQUEUE_OVERCOMMIT 0x10000

/*
 * Flags filed passed to bsdthread_create and back in pthread_start 
31  <---------------------------------> 0
_________________________________________
| flags(8) | policy(8) | importance(16) |
-----------------------------------------
*/

#define PTHREAD_START_CUSTOM	0x01000000
#define PTHREAD_START_SETSCHED	0x02000000
#define PTHREAD_START_DETACHED	0x04000000
#define PTHREAD_START_QOSCLASS	0x08000000
#define PTHREAD_START_QOSCLASS_MASK 0xffffff
#define PTHREAD_START_POLICY_BITSHIFT 16
#define PTHREAD_START_POLICY_MASK 0xff
#define PTHREAD_START_IMPORTANCE_MASK 0xffff

static int pthread_setschedparam_internal(pthread_t, mach_port_t, int, const struct sched_param *);
extern pthread_t __bsdthread_create(void *(*func)(void *), void * func_arg, void * stack, pthread_t  thread, unsigned int flags);
extern int __bsdthread_register(void (*)(pthread_t, mach_port_t, void *(*)(void *), void *, size_t, unsigned int), void (*)(pthread_t, mach_port_t, void *, void *, int), int,void (*)(pthread_t, mach_port_t, void *(*)(void *), void *, size_t, unsigned int), int32_t *,__uint64_t);
extern int __bsdthread_terminate(void * freeaddr, size_t freesize, mach_port_t kport, mach_port_t joinsem);
extern __uint64_t __thread_selfid( void );
extern int __pthread_canceled(int);
extern int __pthread_kill(mach_port_t, int);

extern int __workq_open(void);
extern int __workq_kernreturn(int, void *, int, int);

#if defined(__i386__) || defined(__x86_64__)
static const mach_vm_address_t PTHREAD_STACK_HINT = 0xB0000000;
#else
#error no PTHREAD_STACK_HINT for this architecture
#endif

#ifdef __i386__
// Check for regression of <rdar://problem/13249323>
struct rdar_13249323_regression_static_assert { unsigned a[offsetof(struct _pthread, err_no) == 68 ? 1 : -1]; };
#endif

// Allocate a thread structure, stack and guard page.
//
// The thread structure may optionally be placed in the same allocation as the
// stack, residing above the top of the stack. This cannot be done if a
// custom stack address is provided.
//
// Similarly the guard page cannot be allocated if a custom stack address is
// provided.
//
// The allocated thread structure is initialized with values that indicate how
// it should be freed.

static int
_pthread_allocate(pthread_t *thread, const pthread_attr_t *attrs, void **stack)
{
	int res;
	kern_return_t kr;
	pthread_t t = NULL;
	mach_vm_address_t allocaddr = PTHREAD_STACK_HINT;
	size_t allocsize = 0;
	size_t guardsize = 0;
	size_t stacksize = 0;
	
	PTHREAD_ASSERT(attrs->stacksize >= PTHREAD_STACK_MIN);

	*thread = NULL;
	*stack = NULL;
	
	// Allocate a pthread structure if necessary
	
	if (attrs->stackaddr != NULL) {
		PTHREAD_ASSERT(((uintptr_t)attrs->stackaddr % vm_page_size) == 0);
		*stack = attrs->stackaddr;
		allocsize = pthreadsize;
	} else {
		guardsize = attrs->guardsize;
		stacksize = attrs->stacksize;
		allocsize = stacksize + guardsize + pthreadsize;
	}
	
	kr = mach_vm_map(mach_task_self(),
			 &allocaddr,
			 allocsize,
			 vm_page_size - 1,
			 VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE,
			 MEMORY_OBJECT_NULL,
			 0,
			 FALSE,
			 VM_PROT_DEFAULT,
			 VM_PROT_ALL,
			 VM_INHERIT_DEFAULT);

	if (kr != KERN_SUCCESS) {
		kr = mach_vm_allocate(mach_task_self(),
				 &allocaddr,
				 allocsize,
				 VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE);
	}

	if (kr == KERN_SUCCESS) {
		// The stack grows down.
		// Set the guard page at the lowest address of the
		// newly allocated stack. Return the highest address
		// of the stack.
		if (guardsize) {
			(void)mach_vm_protect(mach_task_self(), allocaddr, guardsize, FALSE, VM_PROT_NONE);
		}

		// Thread structure resides at the top of the stack.
		t = (void *)(allocaddr + stacksize + guardsize);
		if (stacksize) {
			// Returns the top of the stack.
			*stack = t;
		}
	}
	
	if (t != NULL) {
		_pthread_struct_init(t, attrs, *stack, 0, 0);
		t->freeaddr = (void *)allocaddr;
		t->freesize = allocsize;
		*thread = t;
		res = 0;
	} else {
		res = EAGAIN;
	}
        return res;
}

static int
_pthread_deallocate(pthread_t t)
{
	// Don't free the main thread.
	if (t != &_thread) {
		(void)mach_vm_deallocate(mach_task_self(), t->freeaddr, t->freesize);
	}
	return 0;
}

// Terminates the thread if called from the currently running thread.
PTHREAD_NORETURN
static void
_pthread_terminate(pthread_t t)
{
	PTHREAD_ASSERT(t == pthread_self());
	
	uintptr_t freeaddr = (uintptr_t)t->freeaddr;
	size_t freesize = t->freesize - pthreadsize;

	mach_port_t kport = _pthread_kernel_thread(t);
	semaphore_t joinsem = t->joiner_notify;

	_pthread_dealloc_reply_port(t);

	// Shrink the pthread_t so that it does not include the stack
	// so that we're always responsible for deallocating the stack.
	t->freeaddr += freesize;
	t->freesize = pthreadsize;

	// After the call to __pthread_remove_thread, it is only safe to
	// dereference the pthread_t structure if EBUSY has been returned.

	bool destroy, should_exit;
	destroy = (__pthread_remove_thread(t, true, &should_exit) != EBUSY);

	if (t == &_thread) {
		// Don't free the main thread.
		freesize = 0;
	} else if (destroy) {
		// We were told not to keep the pthread_t structure around, so
		// instead of just deallocating the stack, we should deallocate
		// the entire structure.
		freesize += pthreadsize;
	}
	if (freesize == 0) {
		freeaddr = 0;
	}
	_pthread_introspection_thread_terminate(t, freeaddr, freesize, destroy);
	if (should_exit) {
		exitf(0);
	}

	__bsdthread_terminate((void *)freeaddr, freesize, kport, joinsem);
	PTHREAD_ABORT("thread %p didn't terminate", t);
}

int       
pthread_attr_destroy(pthread_attr_t *attr)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		attr->sig = 0;
		ret = 0;
	}
	return ret;
}

int       
pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		*detachstate = attr->detached;
		ret = 0;
	}
	return ret;
}

int       
pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inheritsched)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		*inheritsched = attr->inherit;
		ret = 0;
	}
	return ret;
}

int       
pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		*param = attr->param;
		ret = 0;
	}
	return ret;
}

int       
pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		*policy = attr->policy;
		ret = 0;
	}
	return ret;
}

// Default stack size is 512KB; independent of the main thread's stack size.
static const size_t DEFAULT_STACK_SIZE = 512 * 1024;

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
	attr->fastpath = 1;
	attr->schedset = 0;
	attr->guardsize = vm_page_size;
	attr->qosclass = _pthread_priority_make_newest(QOS_CLASS_DEFAULT, 0, 0);
	return 0;
}

int       
pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG &&
	    (detachstate == PTHREAD_CREATE_JOINABLE ||
	     detachstate == PTHREAD_CREATE_DETACHED)) {
		attr->detached = detachstate;
		ret = 0;
	}
	return ret;
}

int       
pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG &&
	    (inheritsched == PTHREAD_INHERIT_SCHED ||
	     inheritsched == PTHREAD_EXPLICIT_SCHED)) {
		attr->inherit = inheritsched;
		ret = 0;
	}
	return ret;
}

int       
pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		/* TODO: Validate sched_param fields */
		attr->param = *param;
		attr->schedset = 1;
		ret = 0;
	}
	return ret;
}

int       
pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG &&
	    (policy == SCHED_OTHER ||
	     policy == SCHED_RR ||
	     policy == SCHED_FIFO)) {
		attr->policy = policy;
		attr->schedset = 1;
		ret = 0;
	}
	return ret;
}

int
pthread_attr_setscope(pthread_attr_t *attr, int scope)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		if (scope == PTHREAD_SCOPE_SYSTEM) {
			// No attribute yet for the scope.
			ret = 0;
		} else if (scope == PTHREAD_SCOPE_PROCESS) {
			ret = ENOTSUP;
		}
	}
	return ret;
}

int
pthread_attr_getscope(const pthread_attr_t *attr, int *scope)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		*scope = PTHREAD_SCOPE_SYSTEM;
		ret = 0;
	}
	return ret;
}

int
pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		*stackaddr = attr->stackaddr;
		ret = 0;
	}
	return ret;
}

int
pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG &&
	    ((uintptr_t)stackaddr % vm_page_size) == 0) {
		attr->stackaddr = stackaddr;
		attr->fastpath = 0;
		attr->guardsize = 0;
		ret = 0;
	}
	return ret;
}

int
pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		*stacksize = attr->stacksize;
		ret = 0;
	}
	return ret;
}

int
pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG &&
	    (stacksize % vm_page_size) == 0 &&
	    stacksize >= PTHREAD_STACK_MIN) {
		attr->stacksize = stacksize;
		ret = 0;
	}
	return ret;
}

int
pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t * stacksize)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		*stackaddr = (void *)((uintptr_t)attr->stackaddr - attr->stacksize);
		*stacksize = attr->stacksize;
		ret = 0;
	}
	return ret;
}

// Per SUSv3, the stackaddr is the base address, the lowest addressable byte
// address. This is not the same as in pthread_attr_setstackaddr.
int
pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG &&
	    ((uintptr_t)stackaddr % vm_page_size) == 0 &&
	    (stacksize % vm_page_size) == 0 &&
	    stacksize >= PTHREAD_STACK_MIN) {
		attr->stackaddr = (void *)((uintptr_t)stackaddr + stacksize);
        	attr->stacksize = stacksize;
		attr->fastpath = 0;
		ret = 0;
	}
	return ret;
}

int
pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		/* Guardsize of 0 is valid, ot means no guard */
		if ((guardsize % vm_page_size) == 0) {
			attr->guardsize = guardsize;
			attr->fastpath = 0;
			ret = 0;
		}
	}
	return ret;
}

int
pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize)
{
	int ret = EINVAL;
	if (attr->sig == _PTHREAD_ATTR_SIG) {
		*guardsize = attr->guardsize;
		ret = 0;
	}
	return ret;
}


/*
 * Create and start execution of a new thread.
 */

static void
_pthread_body(pthread_t self)
{
	_pthread_set_self(self);
	__pthread_add_thread(self, false);
	_pthread_exit(self, (self->fun)(self->arg));
}

void
_pthread_start(pthread_t self, mach_port_t kport, void *(*fun)(void *), void *arg, size_t stacksize, unsigned int pflags)
{
	if ((pflags & PTHREAD_START_CUSTOM) == 0) {
		void *stackaddr = self;
		_pthread_struct_init(self, &_pthread_attr_default, stackaddr, stacksize, 1);

		if (pflags & PTHREAD_START_SETSCHED) {
			self->policy = ((pflags >> PTHREAD_START_POLICY_BITSHIFT) & PTHREAD_START_POLICY_MASK);
			self->param.sched_priority = (pflags & PTHREAD_START_IMPORTANCE_MASK);
		}

		if ((pflags & PTHREAD_START_DETACHED) == PTHREAD_START_DETACHED)  {
			self->detached &= ~PTHREAD_CREATE_JOINABLE;
			self->detached |= PTHREAD_CREATE_DETACHED;
		}
	}

	if ((pflags & PTHREAD_START_QOSCLASS) != 0) {
		/* The QoS class is cached in the TSD of the pthread, so to reflect the
		 * class that the kernel brought us up at, the TSD must be primed from the
		 * flags parameter.
		 */
		self->tsd[_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS] = (pflags & PTHREAD_START_QOSCLASS_MASK);
	} else {
		/* Give the thread a default QoS tier, of zero. */
		self->tsd[_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS] = _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0);
	}

	_pthread_set_kernel_thread(self, kport);
	self->fun = fun;
	self->arg = arg;
	
	_pthread_body(self);
}

static void
_pthread_struct_init(pthread_t t,
		     const pthread_attr_t *attrs,
		     void *stack,
		     size_t stacksize,
		     int kernalloc)
{
	t->sig = _PTHREAD_SIG;
	t->tsd[_PTHREAD_TSD_SLOT_PTHREAD_SELF] = t;
	t->tsd[_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS] = _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0);
	LOCK_INIT(t->lock);
	t->kernalloc = kernalloc;
	if (kernalloc != 0) {
		uintptr_t stackaddr = (uintptr_t)t;
		t->stacksize = stacksize;
		t->stackaddr = (void *)stackaddr;
		t->freeaddr = (void *)(uintptr_t)(stackaddr - stacksize - vm_page_size);
		t->freesize = pthreadsize + stacksize + vm_page_size;
	} else {
		t->stacksize = attrs->stacksize;
		t->stackaddr = (void *)stack;
	}
	t->guardsize = attrs->guardsize;
	t->detached = attrs->detached;
	t->inherit = attrs->inherit;
	t->policy = attrs->policy;
	t->schedset = attrs->schedset;
	t->param = attrs->param;
	t->cancel_state = PTHREAD_CANCEL_ENABLE | PTHREAD_CANCEL_DEFERRED;
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
	return __is_threaded;
}

mach_port_t
pthread_mach_thread_np(pthread_t t)
{
	mach_port_t kport = MACH_PORT_NULL;

	if (t == pthread_self()) {
		/*
		 * If the call is on self, return the kernel port. We cannot
		 * add this bypass for main thread as it might have exited,
		 * and we should not return stale port info.
		 */
		kport = _pthread_kernel_thread(t);
	} else {
		(void)_pthread_lookup_thread(t, &kport, 0);
	}

	return kport;
}

pthread_t
pthread_from_mach_thread_np(mach_port_t kernel_thread)
{
	struct _pthread *p = NULL;

	/* No need to wait as mach port is already known */
	LOCK(_pthread_list_lock);

	TAILQ_FOREACH(p, &__pthread_head, plist) {
		if (_pthread_kernel_thread(p) == kernel_thread) {
			break;
		}
	}

	UNLOCK(_pthread_list_lock);

	return p;
}

size_t
pthread_get_stacksize_np(pthread_t t)
{
	int ret;
	size_t size = 0;

	if (t == NULL) {
		return ESRCH; // XXX bug?
	}
	
	// since the main thread will not get de-allocated from underneath us
	if (t == pthread_self() || t == &_thread) {
		return t->stacksize;
	}

	LOCK(_pthread_list_lock);

	ret = _pthread_find_thread(t);
	if (ret == 0) {
		size = t->stacksize;
	} else {
		size = ret; // XXX bug?
	}

	UNLOCK(_pthread_list_lock);

	return size;
}

void *
pthread_get_stackaddr_np(pthread_t t)
{
	int ret;
	void *addr = NULL;

	if (t == NULL) {
		return (void *)(uintptr_t)ESRCH; // XXX bug?
	}
	
	// since the main thread will not get de-allocated from underneath us
	if (t == pthread_self() || t == &_thread) {
		return t->stackaddr;
	}

	LOCK(_pthread_list_lock);

	ret = _pthread_find_thread(t);
	if (ret == 0) {
		addr = t->stackaddr;
	} else {
		addr = (void *)(uintptr_t)ret; // XXX bug?
	}

	UNLOCK(_pthread_list_lock);

	return addr;
}

static mach_port_t
_pthread_reply_port(pthread_t t)
{
	void *p;
	if (t == NULL) {
		p = _pthread_getspecific_direct(_PTHREAD_TSD_SLOT_MIG_REPLY);
	} else {
		p = t->tsd[_PTHREAD_TSD_SLOT_MIG_REPLY];
	}
	return (mach_port_t)(uintptr_t)p;
}

static void
_pthread_set_reply_port(pthread_t t, mach_port_t reply_port)
{
	void *p = (void *)(uintptr_t)reply_port;
	if (t == NULL) {
		_pthread_setspecific_direct(_PTHREAD_TSD_SLOT_MIG_REPLY, p);
	} else {
		t->tsd[_PTHREAD_TSD_SLOT_MIG_REPLY] = p;
	}
}

static void
_pthread_dealloc_reply_port(pthread_t t)
{
	mach_port_t reply_port = _pthread_reply_port(t);
	if (reply_port != MACH_PORT_NULL) {
		mig_dealloc_reply_port(reply_port);
	}
}

pthread_t
pthread_main_thread_np(void)
{
	return &_thread;
}

/* returns non-zero if the current thread is the main thread */
int
pthread_main_np(void)
{
	pthread_t self = pthread_self();

	return ((self->detached & _PTHREAD_CREATE_PARENT) == _PTHREAD_CREATE_PARENT);
}


/* if we are passed in a pthread_t that is NULL, then we return
   the current thread's thread_id. So folks don't have to call
   pthread_self, in addition to us doing it, if they just want 
   their thread_id.
*/
int
pthread_threadid_np(pthread_t thread, uint64_t *thread_id)
{
	int res = 0;
	pthread_t self = pthread_self();

	if (thread_id == NULL) {
		return EINVAL;
	}

	if (thread == NULL || thread == self) {
		*thread_id = self->thread_id;
	} else {
		LOCK(_pthread_list_lock);
		res = _pthread_find_thread(thread);
		if (res == 0) {
			*thread_id = thread->thread_id;
		}
		UNLOCK(_pthread_list_lock);
	}
	return res;
}

int
pthread_getname_np(pthread_t thread, char *threadname, size_t len)
{
	int res;

	if (thread == NULL) {
		return ESRCH;
	}

	LOCK(_pthread_list_lock);
	res = _pthread_find_thread(thread);
	if (res == 0) {
		strlcpy(threadname, thread->pthread_name, len);
	}
	UNLOCK(_pthread_list_lock);
	return res;
}

int
pthread_setname_np(const char *name)
{
	int res;
	pthread_t self = pthread_self();

	size_t len = 0;
	if (name != NULL) {
		len = strlen(name);
	}

	/* protytype is in pthread_internals.h */
	res = __proc_info(5, getpid(), 2, (uint64_t)0, (void*)name, (int)len);
	if (res == 0) {
		if (len > 0) {
			strlcpy(self->pthread_name, name, MAXTHREADNAMESIZE);
		} else {
			bzero(self->pthread_name, MAXTHREADNAMESIZE);
		}
	}
	return res;

}

PTHREAD_ALWAYS_INLINE
static inline void
__pthread_add_thread(pthread_t t, bool parent)
{
	bool should_deallocate = false;
	bool should_add = true;

	LOCK(_pthread_list_lock);

	// The parent and child threads race to add the thread to the list.
	// When called by the parent:
	//  - set parentcheck to true
	//  - back off if childrun is true
	// When called by the child:
	//  - set childrun to true
	//  - back off if parentcheck is true
	if (parent) {
		t->parentcheck = 1;
		if (t->childrun) {
			// child got here first, don't add.
			should_add = false;
		}
		
		// If the child exits before we check in then it has to keep
		// the thread structure memory alive so our dereferences above
		// are valid. If it's a detached thread, then no joiner will
		// deallocate the thread structure itself. So we do it here.
		if (t->childexit) {
			should_add = false;
			should_deallocate = ((t->detached & PTHREAD_CREATE_DETACHED) == PTHREAD_CREATE_DETACHED);
		}
	} else {
		t->childrun = 1;
		if (t->parentcheck) {
			// Parent got here first, don't add.
			should_add = false;
		}
		if (t->wqthread) {
			// Work queue threads have no parent. Simulate.
			t->parentcheck = 1;
		}
	}

	if (should_add) {
		TAILQ_INSERT_TAIL(&__pthread_head, t, plist);
		_pthread_count++;
	}

	UNLOCK(_pthread_list_lock);

	if (parent) {
		_pthread_introspection_thread_create(t, should_deallocate);
		if (should_deallocate) {
			_pthread_deallocate(t);
		}
	} else {
		_pthread_introspection_thread_start(t);
	}
}

// <rdar://problem/12544957> must always inline this function to avoid epilogues
// Returns EBUSY if the thread structure should be kept alive (is joinable).
// Returns ESRCH if the thread structure is no longer valid (was detached).
PTHREAD_ALWAYS_INLINE
static inline int
__pthread_remove_thread(pthread_t t, bool child, bool *should_exit)
{
	int ret = 0;
	
	bool should_remove = true;

	LOCK(_pthread_list_lock);

	// When a thread removes itself:
	//  - Set the childexit flag indicating that the thread has exited.
	//  - Return false if parentcheck is zero (must keep structure)
	//  - If the thread is joinable, keep it on the list so that
	//    the join operation succeeds. Still decrement the running
	//    thread count so that we exit if no threads are running.
	//  - Update the running thread count.
	// When another thread removes a joinable thread:
	//  - CAREFUL not to dereference the thread before verifying that the
	//    reference is still valid using _pthread_find_thread().
	//  - Remove the thread from the list.

	if (child) {
		t->childexit = 1;
		if (t->parentcheck == 0) {
			ret = EBUSY;
		}
		if ((t->detached & PTHREAD_CREATE_JOINABLE) != 0) {
			ret = EBUSY;
			should_remove = false;
		}
		*should_exit = (--_pthread_count <= 0);
	} else {
		ret = _pthread_find_thread(t);
		if (ret == 0) {
			// If we found a thread but it's not joinable, bail.
			if ((t->detached & PTHREAD_CREATE_JOINABLE) == 0) {
				should_remove = false;
				ret = ESRCH;
			}
		}
	}
	if (should_remove) {
		TAILQ_REMOVE(&__pthread_head, t, plist);
	}

	UNLOCK(_pthread_list_lock);
	
	return ret;
}

int
pthread_create(pthread_t *thread,
	const pthread_attr_t *attr,
	void *(*start_routine)(void *),
	void *arg)
{	
	pthread_t t = NULL;
	unsigned int flags = 0;

	pthread_attr_t *attrs = (pthread_attr_t *)attr;
	if (attrs == NULL) {
		attrs = &_pthread_attr_default;
	} else if (attrs->sig != _PTHREAD_ATTR_SIG) {
		return EINVAL;
	}

	if (attrs->detached == PTHREAD_CREATE_DETACHED) {
		flags |= PTHREAD_START_DETACHED;
	}

	if (attrs->schedset != 0) {
		flags |= PTHREAD_START_SETSCHED;
		flags |= ((attrs->policy & PTHREAD_START_POLICY_MASK) << PTHREAD_START_POLICY_BITSHIFT);
		flags |= (attrs->param.sched_priority & PTHREAD_START_IMPORTANCE_MASK);
	} else if (attrs->qosclass != 0) {
		flags |= PTHREAD_START_QOSCLASS;
		flags |= (attrs->qosclass & PTHREAD_START_QOSCLASS_MASK);
	}

	__is_threaded = 1;

	void *stack;
	
	if (attrs->fastpath) {
		// kernel will allocate thread and stack, pass stacksize.
		stack = (void *)attrs->stacksize;
	} else {
		// allocate the thread and its stack
		flags |= PTHREAD_START_CUSTOM;

		int res;
		res = _pthread_allocate(&t, attrs, &stack);
		if (res) {
			return res;
		}

		t->arg = arg;
		t->fun = start_routine;
	}

	pthread_t t2;
	t2 = __bsdthread_create(start_routine, arg, stack, t, flags);
	if (t2 == (pthread_t)-1) {
		if (flags & PTHREAD_START_CUSTOM) {
			// free the thread and stack if we allocated it
			_pthread_deallocate(t);
		}
		return EAGAIN;
	}
	if (t == NULL) {
		t = t2;
	}

	__pthread_add_thread(t, true);
	
	// XXX if a thread is created detached and exits, t will be invalid
	*thread = t;
	return 0;
}

int
pthread_create_suspended_np(pthread_t *thread,
	const pthread_attr_t *attr,
	void *(*start_routine)(void *),
	void *arg)
{
	int res;
	void *stack;
	mach_port_t kernel_thread = MACH_PORT_NULL;

	const pthread_attr_t *attrs = attr;
	if (attrs == NULL) {
		attrs = &_pthread_attr_default;
	} else if (attrs->sig != _PTHREAD_ATTR_SIG) {
		return EINVAL;
	}

	pthread_t t;
	res = _pthread_allocate(&t, attrs, &stack);
	if (res) {
		return res;
	}
		
	*thread = t;

	kern_return_t kr;
	kr = thread_create(mach_task_self(), &kernel_thread);
	if (kr != KERN_SUCCESS) {
		//PTHREAD_ABORT("thread_create() failed: %d", kern_res);
		return EINVAL; /* Need better error here? */
	}

	_pthread_set_kernel_thread(t, kernel_thread);
	(void)pthread_setschedparam_internal(t, kernel_thread, t->policy, &t->param);
		
	__is_threaded = 1;

	t->arg = arg;
	t->fun = start_routine;

	__pthread_add_thread(t, true);

	// Set up a suspended thread.
	_pthread_setup(t, _pthread_body, stack, 1, 0);
	return res;
}

int       
pthread_detach(pthread_t thread)
{
	int res;
	bool join = false;
	semaphore_t sema = SEMAPHORE_NULL;

	res = _pthread_lookup_thread(thread, NULL, 1);
	if (res) {
		return res; // Not a valid thread to detach.
	}

	LOCK(thread->lock);
	if (thread->detached & PTHREAD_CREATE_JOINABLE) {
		if (thread->detached & _PTHREAD_EXITED) {
			// Join the thread if it's already exited.
			join = true;
		} else {
			thread->detached &= ~PTHREAD_CREATE_JOINABLE;
			thread->detached |= PTHREAD_CREATE_DETACHED;
			sema = thread->joiner_notify;
		}
	} else {
		res = EINVAL;
	}
	UNLOCK(thread->lock);

	if (join) {
		pthread_join(thread, NULL);
	} else if (sema) {
		semaphore_signal(sema);
	}

	return res;
}

int   
pthread_kill(pthread_t th, int sig)
{	
	if (sig < 0 || sig > NSIG) {
		return EINVAL;
	}

	mach_port_t kport = MACH_PORT_NULL;
	if (_pthread_lookup_thread(th, &kport, 0) != 0) {
		return ESRCH; // Not a valid thread.
	}

	// Don't signal workqueue threads.
	if (th->wqthread != 0 && th->wqkillset == 0) {
		return ENOTSUP;
	}

	int ret = __pthread_kill(kport, sig);

	if (ret == -1) {
		ret = errno;
	}
	return ret;
}

int 
__pthread_workqueue_setkill(int enable)
{
	pthread_t self = pthread_self();

	LOCK(self->lock);
	self->wqkillset = enable ? 1 : 0;
	UNLOCK(self->lock);

	return 0;
}

static void *
__pthread_get_exit_value(pthread_t t, int conforming)
{
	const int flags = (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING);
	void *value = t->exit_value;
	if (conforming) {
		if ((t->cancel_state & flags) == flags) {
			value = PTHREAD_CANCELED;
		}
	}
	return value;
}

/* For compatibility... */

pthread_t
_pthread_self(void) {
	return pthread_self();
}

/*
 * Terminate a thread.
 */
int __disable_threadsignal(int);

PTHREAD_NORETURN
static void 
_pthread_exit(pthread_t self, void *value_ptr)
{
	struct __darwin_pthread_handler_rec *handler;

	// Disable signal delivery while we clean up
	__disable_threadsignal(1);

	// Set cancel state to disable and type to deferred
	_pthread_setcancelstate_exit(self, value_ptr, __unix_conforming);

	while ((handler = self->__cleanup_stack) != 0) {
		(handler->__routine)(handler->__arg);
		self->__cleanup_stack = handler->__next;
	}
	_pthread_tsd_cleanup(self);

	LOCK(self->lock);
	self->detached |= _PTHREAD_EXITED;
	self->exit_value = value_ptr;

	if ((self->detached & PTHREAD_CREATE_JOINABLE) &&
			self->joiner_notify == SEMAPHORE_NULL) {
		self->joiner_notify = (semaphore_t)os_get_cached_semaphore();
	}
	UNLOCK(self->lock);

	// Clear per-thread semaphore cache
	os_put_cached_semaphore(SEMAPHORE_NULL);

	_pthread_terminate(self);
}

void
pthread_exit(void *value_ptr)
{
	pthread_t self = pthread_self();
	if (self->wqthread == 0) {
		_pthread_exit(self, value_ptr);
	} else {
		PTHREAD_ABORT("pthread_exit() may only be called against threads created via pthread_create()");
	}
}

int       
pthread_getschedparam(pthread_t thread, 
		      int *policy,
		      struct sched_param *param)
{
	int ret;

	if (thread == NULL) {
		return ESRCH;
	}
	
	LOCK(_pthread_list_lock);

	ret = _pthread_find_thread(thread);
	if (ret == 0) {
		if (policy) {
			*policy = thread->policy;
		}
		if (param) {
			*param = thread->param;
		}
	}

	UNLOCK(_pthread_list_lock);

	return ret;
}

static int       
pthread_setschedparam_internal(pthread_t thread, 
		      mach_port_t kport,
		      int policy,
		      const struct sched_param *param)
{
	policy_base_data_t bases;
	policy_base_t base;
	mach_msg_type_number_t count;
	kern_return_t ret;

	switch (policy) {
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
			return EINVAL;
	}
	ret = thread_policy(kport, policy, base, count, TRUE);
	return (ret != KERN_SUCCESS) ? EINVAL : 0;
}

int       
pthread_setschedparam(pthread_t t, int policy, const struct sched_param *param)
{
	mach_port_t kport = MACH_PORT_NULL;
	int res;
	int bypass = 1;

	// since the main thread will not get de-allocated from underneath us
	if (t == pthread_self() || t == &_thread ) {
		kport = _pthread_kernel_thread(t);
	} else {
		bypass = 0;
		(void)_pthread_lookup_thread(t, &kport, 0);
	}
	
	res = pthread_setschedparam_internal(t, kport, policy, param);
	if (res == 0) {
		if (bypass == 0) {
			// Ensure the thread is still valid.
			LOCK(_pthread_list_lock);
			res = _pthread_find_thread(t);
			if (res == 0) {
				t->policy = policy;
				t->param = *param;
			}
			UNLOCK(_pthread_list_lock);
		}  else {
			t->policy = policy;
			t->param = *param;
		}
	}
	return res;
}

int
sched_get_priority_min(int policy)
{
	return default_priority - 16;
}

int
sched_get_priority_max(int policy)
{
	return default_priority + 16;
}

int       
pthread_equal(pthread_t t1, pthread_t t2)
{
	return (t1 == t2);
}

// Force LLVM not to optimise this to a call to __pthread_set_self, if it does
// then _pthread_set_self won't be bound when secondary threads try and start up.
PTHREAD_NOINLINE
void
_pthread_set_self(pthread_t p)
{
	extern void __pthread_set_self(void *);

	if (p == NULL) {
		p = &_thread;
	}
	
	uint64_t tid = __thread_selfid();
	if (tid == -1ull) {
		PTHREAD_ABORT("failed to set thread_id");
	}

	p->tsd[_PTHREAD_TSD_SLOT_PTHREAD_SELF] = p;
	p->tsd[_PTHREAD_TSD_SLOT_ERRNO] = &p->err_no;
	p->thread_id = tid;
	__pthread_set_self(&p->tsd[0]);
}

struct _pthread_once_context {
	pthread_once_t *pthread_once;
	void (*routine)(void);
};

static void
__pthread_once_handler(void *context)
{
	struct _pthread_once_context *ctx = context;
	pthread_cleanup_push((void*)__os_once_reset, &ctx->pthread_once->once);
	ctx->routine();
	pthread_cleanup_pop(0);
	ctx->pthread_once->sig = _PTHREAD_ONCE_SIG;
}

int       
pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
	struct _pthread_once_context ctx = { once_control, init_routine };
	do {
		os_once(&once_control->once, &ctx, __pthread_once_handler);
	} while (once_control->sig == _PTHREAD_ONCE_SIG_init);
	return 0;
}

void
_pthread_testcancel(pthread_t thread, int isconforming)
{
	const int flags = (PTHREAD_CANCEL_ENABLE|_PTHREAD_CANCEL_PENDING);

	LOCK(thread->lock);
	bool canceled = ((thread->cancel_state & flags) == flags);
	UNLOCK(thread->lock);
	
	if (canceled) {
		pthread_exit(isconforming ? PTHREAD_CANCELED : 0);
	}
}

void
_pthread_exit_if_canceled(int error)
{
	if (__unix_conforming && ((error & 0xff) == EINTR) && (__pthread_canceled(0) == 0)) {
		pthread_t self = pthread_self();
		if (self != NULL) {
			self->cancel_error = error;
		}
		pthread_exit(PTHREAD_CANCELED);
	}
}

int
pthread_getconcurrency(void)
{
	return pthread_concurrency;
}

int
pthread_setconcurrency(int new_level)
{
	if (new_level < 0) {
		return EINVAL;
	}
	pthread_concurrency = new_level;
	return 0;
}

void
_pthread_set_pfz(uintptr_t address)
{
}

#if !defined(PTHREAD_TARGET_EOS) && !defined(VARIANT_DYLD)
void *
malloc(size_t sz)
{
	if (_pthread_malloc) {
		return _pthread_malloc(sz);
	} else {
		return NULL;
	}
}

void
free(void *p)
{
	if (_pthread_free) {
		_pthread_free(p);
	}
}
#endif

/*
 * Perform package initialization - called automatically when application starts
 */
struct ProgramVars; /* forward reference */

int
__pthread_init(const struct _libpthread_functions *pthread_funcs, const char *envp[] __unused,
               const char *apple[] __unused, const struct ProgramVars *vars __unused)
{
	// Save our provided pushed-down functions
	if (pthread_funcs) {
		exitf = pthread_funcs->exit;

		if (pthread_funcs->version >= 2) {
			_pthread_malloc = pthread_funcs->malloc;
			_pthread_free = pthread_funcs->free;
		}
	}

	//
	// Get host information
	//

	kern_return_t kr;
	host_flavor_t flavor = HOST_PRIORITY_INFO;
	mach_msg_type_number_t count = HOST_PRIORITY_INFO_COUNT;
	host_priority_info_data_t priority_info;
	host_t host = mach_host_self();
	kr = host_info(host, flavor, (host_info_t)&priority_info, &count);
	if (kr != KERN_SUCCESS) {
		PTHREAD_ABORT("host_info(mach_host_self(), ...) failed: %s", mach_error_string(kr));
	} else {
		default_priority = priority_info.user_priority;
		min_priority = priority_info.minimum_priority;
		max_priority = priority_info.maximum_priority;
	}
	mach_port_deallocate(mach_task_self(), host);

	//
	// Set up the main thread structure
	//

	void *stackaddr;
	size_t stacksize = DFLSSIZ;
    	size_t len = sizeof(stackaddr);
    	int mib[] = { CTL_KERN, KERN_USRSTACK };
    	if (__sysctl(mib, 2, &stackaddr, &len, NULL, 0) != 0) {
       		stackaddr = (void *)USRSTACK;
	}

	pthread_t thread = &_thread;
	pthread_attr_init(&_pthread_attr_default);
	_pthread_struct_init(thread, &_pthread_attr_default, stackaddr, stacksize, 0);
	thread->detached = PTHREAD_CREATE_JOINABLE;

	// Finish initialization with common code that is reinvoked on the
	// child side of a fork.

	// Finishes initialization of main thread attributes.
	// Initializes the thread list and add the main thread.
	// Calls _pthread_set_self() to prepare the main thread for execution.
	__pthread_fork_child_internal(thread);
	
	// Set up kernel entry points with __bsdthread_register.
	pthread_workqueue_atfork_child();

	return 0;
}

int
sched_yield(void)
{
    swtch_pri(0);
    return 0;
}

PTHREAD_NOEXPORT void
__pthread_fork_child_internal(pthread_t p)
{
	TAILQ_INIT(&__pthread_head);
	LOCK_INIT(_pthread_list_lock);

	// Re-use the main thread's static storage if no thread was provided.
	if (p == NULL) {
		if (_thread.tsd[0] != 0) {
			bzero(&_thread, sizeof(struct _pthread));
		}
		p = &_thread;
	}

	LOCK_INIT(p->lock);
	_pthread_set_kernel_thread(p, mach_thread_self());
	_pthread_set_reply_port(p, mach_reply_port());
	p->__cleanup_stack = NULL;
	p->joiner_notify = SEMAPHORE_NULL;
	p->joiner = MACH_PORT_NULL;
	p->detached |= _PTHREAD_CREATE_PARENT;
	p->tsd[__TSD_SEMAPHORE_CACHE] = SEMAPHORE_NULL;

	// Initialize the list of threads with the new main thread.
	TAILQ_INSERT_HEAD(&__pthread_head, p, plist);
	_pthread_count = 1;

	_pthread_set_self(p);
	_pthread_introspection_thread_start(p);
}

/*
 * Query/update the cancelability 'state' of a thread
 */
PTHREAD_NOEXPORT int
_pthread_setcancelstate_internal(int state, int *oldstate, int conforming)
{
	pthread_t self;

	switch (state) {
		case PTHREAD_CANCEL_ENABLE:
			if (conforming) {
				__pthread_canceled(1);
			}
			break;
		case PTHREAD_CANCEL_DISABLE:
			if (conforming) {
				__pthread_canceled(2);
			}
			break;
		default:
			return EINVAL;
	}

	self = pthread_self();
	LOCK(self->lock);
	if (oldstate) {
		*oldstate = self->cancel_state & _PTHREAD_CANCEL_STATE_MASK;
	}
	self->cancel_state &= ~_PTHREAD_CANCEL_STATE_MASK;
	self->cancel_state |= state;
	UNLOCK(self->lock);
	if (!conforming) {
		_pthread_testcancel(self, 0);  /* See if we need to 'die' now... */
	}
	return 0;
}

/* When a thread exits set the cancellation state to DISABLE and DEFERRED */
static void
_pthread_setcancelstate_exit(pthread_t self, void * value_ptr, int conforming)
{
	LOCK(self->lock);
	self->cancel_state &= ~(_PTHREAD_CANCEL_STATE_MASK | _PTHREAD_CANCEL_TYPE_MASK);
	self->cancel_state |= (PTHREAD_CANCEL_DISABLE | PTHREAD_CANCEL_DEFERRED);
	if (value_ptr == PTHREAD_CANCELED) {
// 4597450: begin
		self->detached |= _PTHREAD_WASCANCEL;
// 4597450: end
	}
	UNLOCK(self->lock);
}

int
_pthread_join_cleanup(pthread_t thread, void ** value_ptr, int conforming)
{
	// Returns ESRCH if the thread was not created joinable.
	int ret = __pthread_remove_thread(thread, false, NULL);
	if (ret != 0) {
		return ret;
	}
	
	if (value_ptr) {
		*value_ptr = __pthread_get_exit_value(thread, conforming);
	}
	_pthread_introspection_thread_destroy(thread);
	_pthread_deallocate(thread);
	return 0;
}

/* ALWAYS called with list lock and return with list lock */
int
_pthread_find_thread(pthread_t thread)
{
	if (thread != NULL) {
		pthread_t p;
loop:
		TAILQ_FOREACH(p, &__pthread_head, plist) {
			if (p == thread) {
				if (_pthread_kernel_thread(thread) == MACH_PORT_NULL) {
					UNLOCK(_pthread_list_lock);
					sched_yield();
					LOCK(_pthread_list_lock);
					goto loop;
				} 
				return 0;
			}
		}
	}
	return ESRCH;
}

int
_pthread_lookup_thread(pthread_t thread, mach_port_t *portp, int only_joinable)
{
	mach_port_t kport = MACH_PORT_NULL;
	int ret;

	if (thread == NULL) {
		return ESRCH;
	}
	
	LOCK(_pthread_list_lock);
	
	ret = _pthread_find_thread(thread);
	if (ret == 0) {
		// Fail if we only want joinable threads and the thread found is
		// not in the detached state.
		if (only_joinable != 0 && (thread->detached & PTHREAD_CREATE_DETACHED) != 0) {
			ret = EINVAL;
		} else {
			kport = _pthread_kernel_thread(thread);
		}
	}
	
	UNLOCK(_pthread_list_lock);
	
	if (portp != NULL) {
		*portp = kport;
	}

	return ret;
}

void
_pthread_clear_qos_tsd(mach_port_t thread_port)
{
	if (thread_port == MACH_PORT_NULL || (uintptr_t)_pthread_getspecific_direct(_PTHREAD_TSD_SLOT_MACH_THREAD_SELF) == thread_port) {
		/* Clear the current thread's TSD, that can be done inline. */
		_pthread_setspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS, _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0));
	} else {
		pthread_t p;

		LOCK(_pthread_list_lock);

		TAILQ_FOREACH(p, &__pthread_head, plist) {
			mach_port_t kp;
			while ((kp = _pthread_kernel_thread(p)) == MACH_PORT_NULL) {
				UNLOCK(_pthread_list_lock);
				sched_yield();
				LOCK(_pthread_list_lock);
			}
			if (thread_port == kp) {
				p->tsd[_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS] = _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0);
				break;
			}
		}

		UNLOCK(_pthread_list_lock);
	}
}

/***** pthread workqueue support routines *****/

PTHREAD_NOEXPORT void
pthread_workqueue_atfork_child(void)
{
	struct _pthread_registration_data data = {
		.dispatch_queue_offset = __PTK_LIBDISPATCH_KEY0 * sizeof(void *),
	};

	int rv = __bsdthread_register(thread_start,
			     start_wqthread,
			     (int)pthreadsize,
			     (void*)&data,
			     (uintptr_t)sizeof(data),
			     data.dispatch_queue_offset);

	if (rv > 0) {
		__pthread_supported_features = rv;
	}

	if (_pthread_priority_get_qos_newest(data.main_qos) != QOS_CLASS_UNSPECIFIED) {
		_pthread_set_main_qos(data.main_qos);
		_thread.tsd[_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS] = data.main_qos;
	}

	if (__libdispatch_workerfunction != NULL) {
		// prepare the kernel for workq action
		(void)__workq_open();
	}
}

void
_pthread_wqthread(pthread_t self, mach_port_t kport, void *stackaddr, void *unused, int flags)
{
	PTHREAD_ASSERT(flags & WQ_FLAG_THREAD_NEWSPI);

	int thread_reuse = flags & WQ_FLAG_THREAD_REUSE;
	int thread_class = flags & WQ_FLAG_THREAD_PRIOMASK;
	int overcommit = (flags & WQ_FLAG_THREAD_OVERCOMMIT) != 0;

	pthread_priority_t priority;

	if ((__pthread_supported_features & PTHREAD_FEATURE_QOS_MAINTENANCE) == 0) {
		priority = _pthread_priority_make_version2(thread_class, 0, (overcommit ? _PTHREAD_PRIORITY_OVERCOMMIT_FLAG : 0));
	} else {
		priority = _pthread_priority_make_newest(thread_class, 0, (overcommit ? _PTHREAD_PRIORITY_OVERCOMMIT_FLAG : 0));
	}

	if (thread_reuse == 0) {
		// New thread created by kernel, needs initialization.
		_pthread_struct_init(self, &_pthread_attr_default, stackaddr, DEFAULT_STACK_SIZE, 1);
		_pthread_set_kernel_thread(self, kport);
		self->wqthread = 1;
		self->wqkillset = 0;

		// Not a joinable thread.
		self->detached &= ~PTHREAD_CREATE_JOINABLE;
		self->detached |= PTHREAD_CREATE_DETACHED;

		// Update the running thread count and set childrun bit.
		// XXX this should be consolidated with pthread_body().
		_pthread_set_self(self);
		_pthread_introspection_thread_create(self, false);
		__pthread_add_thread(self, false);

		// If we're running with fine-grained priority, we also need to
		// set this thread to have the QoS class provided to use by the kernel
		if (__pthread_supported_features & PTHREAD_FEATURE_FINEPRIO) {
			_pthread_setspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS, _pthread_priority_make_newest(thread_class, 0, 0));
		}
	}

#if WQ_DEBUG
	PTHREAD_ASSERT(self);
	PTHREAD_ASSERT(self == pthread_self());
#endif // WQ_DEBUG

	self->fun = (void *(*)(void *))__libdispatch_workerfunction;
	self->arg = (void *)(uintptr_t)thread_class;

	if (__pthread_supported_features & PTHREAD_FEATURE_FINEPRIO) {
		if (!__workq_newapi) {
			/* Old thread priorities are inverted from where we have them in
			 * the new flexible priority scheme. The highest priority is zero,
			 * up to 2, with background at 3.
			 */
			pthread_workqueue_function_t func = (pthread_workqueue_function_t)__libdispatch_workerfunction;

			int opts = overcommit ? WORKQ_ADDTHREADS_OPTION_OVERCOMMIT : 0;

			if ((__pthread_supported_features & PTHREAD_FEATURE_QOS_DEFAULT) == 0) {
				/* Dirty hack to support kernels that don't have QOS_CLASS_DEFAULT. */
				switch (thread_class) {
					case QOS_CLASS_USER_INTERACTIVE:
						thread_class = QOS_CLASS_USER_INITIATED;
						break;
					case QOS_CLASS_USER_INITIATED:
						thread_class = QOS_CLASS_DEFAULT;
						break;
					default:
						break;
				}
			}

			switch (thread_class) {
				/* QOS_CLASS_USER_INTERACTIVE is not currently requested by for old dispatch priority compatibility */
				case QOS_CLASS_USER_INITIATED:
					(*func)(WORKQ_HIGH_PRIOQUEUE, opts, NULL);
					break;

				case QOS_CLASS_DEFAULT:
					/* B&I builders can't pass a QOS_CLASS_DEFAULT thread to dispatch, for fear of the QoS being
					 * picked up by NSThread (et al) and transported around the system. So change the TSD to
					 * make this thread look like QOS_CLASS_USER_INITIATED even though it will still run as legacy.
					 */
					_pthread_setspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS, _pthread_priority_make_newest(QOS_CLASS_USER_INITIATED, 0, 0));
					(*func)(WORKQ_DEFAULT_PRIOQUEUE, opts, NULL);
					break;

				case QOS_CLASS_UTILITY:
					(*func)(WORKQ_LOW_PRIOQUEUE, opts, NULL);
					break;

				case QOS_CLASS_BACKGROUND:
					(*func)(WORKQ_BG_PRIOQUEUE, opts, NULL);
					break;

				/* Legacy dispatch does not use QOS_CLASS_MAINTENANCE, so no need to handle it here */
			}

		} else {
			/* "New" API, where dispatch is expecting to be given the thread priority */
			(*__libdispatch_workerfunction)(priority);
		}
	} else {
		/* We're the new library running on an old kext, so thread_class is really the workq priority. */
		pthread_workqueue_function_t func = (pthread_workqueue_function_t)__libdispatch_workerfunction;
		int options = overcommit ? WORKQ_ADDTHREADS_OPTION_OVERCOMMIT : 0;
		(*func)(thread_class, options, NULL);
	}

	__workq_kernreturn(WQOPS_THREAD_RETURN, NULL, 0, 0);
	_pthread_exit(self, NULL);
}

/***** pthread workqueue API for libdispatch *****/

int
_pthread_workqueue_init(pthread_workqueue_function2_t func, int offset, int flags)
{
	if (flags != 0) {
		return ENOTSUP;
	}

	__workq_newapi = true;
	__libdispatch_offset = offset;

	int rv = pthread_workqueue_setdispatch_np((pthread_workqueue_function_t)func);
	return rv;
}

void
pthread_workqueue_setdispatchoffset_np(int offset)
{
	__libdispatch_offset = offset;
}

int
pthread_workqueue_setdispatch_np(pthread_workqueue_function_t worker_func)
{
	int res = EBUSY;
	if (__libdispatch_workerfunction == NULL) {
		// Check whether the kernel supports new SPIs
		res = __workq_kernreturn(WQOPS_QUEUE_NEWSPISUPP, NULL, __libdispatch_offset, 0);
		if (res == -1){
			res = ENOTSUP;
		} else {
			__libdispatch_workerfunction = (pthread_workqueue_function2_t)worker_func;

			// Prepare the kernel for workq action
			(void)__workq_open();
			if (__is_threaded == 0) {
				__is_threaded = 1;
			}
		}
	}
	return res;
}

int
_pthread_workqueue_supported(void)
{
	return __pthread_supported_features;
}

int
pthread_workqueue_addthreads_np(int queue_priority, int options, int numthreads)
{
	int res = 0;

	// Cannot add threads without a worker function registered.
	if (__libdispatch_workerfunction == NULL) {
		return EPERM;
	}

	pthread_priority_t kp = 0;

	if (__pthread_supported_features & PTHREAD_FEATURE_FINEPRIO) {
		/* The new kernel API takes the new QoS class + relative priority style of
		 * priority. This entry point is here for compatibility with old libdispatch
		 * versions (ie. the simulator). We request the corresponding new bracket
		 * from the kernel, then on the way out run all dispatch queues that were
		 * requested.
		 */

		int compat_priority = queue_priority & WQ_FLAG_THREAD_PRIOMASK;
		int flags = 0;

		/* To make sure the library does not issue more threads to dispatch than
		 * were requested, the total number of active requests is recorded in
		 * __workq_requests.
		 */
		if (options & WORKQ_ADDTHREADS_OPTION_OVERCOMMIT) {
			flags = _PTHREAD_PRIORITY_OVERCOMMIT_FLAG;
		}

		kp = _pthread_qos_class_encode_workqueue(compat_priority, flags);

	} else {
		/* Running on the old kernel, queue_priority is what we pass directly to
		 * the syscall.
		 */
		kp = queue_priority & WQ_FLAG_THREAD_PRIOMASK;

		if (options & WORKQ_ADDTHREADS_OPTION_OVERCOMMIT) {
			kp |= WORKQUEUE_OVERCOMMIT;
		}
	}

	res = __workq_kernreturn(WQOPS_QUEUE_REQTHREADS, NULL, numthreads, (int)kp);
	if (res == -1) {
		res = errno;
	}
	return res;
}

int
_pthread_workqueue_addthreads(int numthreads, pthread_priority_t priority)
{
	int res = 0;

	if (__libdispatch_workerfunction == NULL) {
		return EPERM;
	}

	if ((__pthread_supported_features & PTHREAD_FEATURE_FINEPRIO) == 0) {
		return ENOTSUP;
	}

	res = __workq_kernreturn(WQOPS_QUEUE_REQTHREADS, NULL, numthreads, (int)priority);
	if (res == -1) {
		res = errno;
	}
	return res;
}

/*
 * Introspection SPI for libpthread.
 */

static pthread_introspection_hook_t _pthread_introspection_hook;

pthread_introspection_hook_t
pthread_introspection_hook_install(pthread_introspection_hook_t hook)
{
	if (os_slowpath(!hook)) {
		PTHREAD_ABORT("pthread_introspection_hook_install was passed NULL");
	}
	pthread_introspection_hook_t prev;
	prev = __sync_swap(&_pthread_introspection_hook, hook);
	return prev;
}

PTHREAD_NOINLINE
static void
_pthread_introspection_hook_callout_thread_create(pthread_t t, bool destroy)
{
	_pthread_introspection_hook(PTHREAD_INTROSPECTION_THREAD_CREATE, t, t,
			pthreadsize);
	if (!destroy) return;
	_pthread_introspection_thread_destroy(t);
}

static inline void
_pthread_introspection_thread_create(pthread_t t, bool destroy)
{
	if (os_fastpath(!_pthread_introspection_hook)) return;
	_pthread_introspection_hook_callout_thread_create(t, destroy);
}

PTHREAD_NOINLINE
static void
_pthread_introspection_hook_callout_thread_start(pthread_t t)
{
	size_t freesize;
	void *freeaddr;
	if (t == &_thread) {
		freesize = t->stacksize + t->guardsize;
		freeaddr = t->stackaddr - freesize;
	} else {
		freesize = t->freesize - pthreadsize;
		freeaddr = t->freeaddr;
	}
	_pthread_introspection_hook(PTHREAD_INTROSPECTION_THREAD_START, t,
			freeaddr, freesize);
}

static inline void
_pthread_introspection_thread_start(pthread_t t)
{
	if (os_fastpath(!_pthread_introspection_hook)) return;
	_pthread_introspection_hook_callout_thread_start(t);
}

PTHREAD_NOINLINE
static void
_pthread_introspection_hook_callout_thread_terminate(pthread_t t,
		void *freeaddr, size_t freesize, bool destroy)
{
	if (destroy && freesize) {
		freesize -= pthreadsize;
	}
	_pthread_introspection_hook(PTHREAD_INTROSPECTION_THREAD_TERMINATE, t,
			freeaddr, freesize);
	if (!destroy) return;
	_pthread_introspection_thread_destroy(t);
}

static inline void
_pthread_introspection_thread_terminate(pthread_t t, void *freeaddr,
		size_t freesize, bool destroy)
{
	if (os_fastpath(!_pthread_introspection_hook)) return;
	_pthread_introspection_hook_callout_thread_terminate(t, freeaddr, freesize,
			destroy);
}

PTHREAD_NOINLINE
static void
_pthread_introspection_hook_callout_thread_destroy(pthread_t t)
{
	if (t == &_thread) return;
	_pthread_introspection_hook(PTHREAD_INTROSPECTION_THREAD_DESTROY, t, t,
			pthreadsize);
}

static inline void
_pthread_introspection_thread_destroy(pthread_t t)
{
	if (os_fastpath(!_pthread_introspection_hook)) return;
	_pthread_introspection_hook_callout_thread_destroy(t);
}

