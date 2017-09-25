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

#include "resolver.h"
#include "internal.h"
#include "private.h"
#include "workqueue_private.h"
#include "introspection_private.h"
#include "qos_private.h"
#include "tsd_private.h"

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

#include <_simple.h>
#include <platform/string.h>
#include <platform/compat.h>

extern int __sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
                    void *newp, size_t newlen);
extern void __exit(int) __attribute__((noreturn));
extern int __pthread_kill(mach_port_t, int);

extern struct _pthread _thread;
extern int default_priority;


//
// Global variables
//

static void (*exitf)(int) = __exit;
PTHREAD_NOEXPORT void* (*_pthread_malloc)(size_t) = NULL;
PTHREAD_NOEXPORT void (*_pthread_free)(void *) = NULL;

#if PTHREAD_DEBUG_LOG
#include <fcntl.h>
int _pthread_debuglog;
uint64_t _pthread_debugstart;
#endif

// This global should be used (carefully) by anyone needing to know if a
// pthread (other than the main thread) has been created.
int __is_threaded = 0;

int __unix_conforming = 0;

// _pthread_list_lock protects _pthread_count, access to the __pthread_head
// list, and the parentcheck, childrun and childexit flags of the pthread
// structure. Externally imported by pthread_cancelable.c.
PTHREAD_NOEXPORT _pthread_lock _pthread_list_lock = _PTHREAD_LOCK_INITIALIZER;
PTHREAD_NOEXPORT struct __pthread_list __pthread_head = TAILQ_HEAD_INITIALIZER(__pthread_head);
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

/*
 * The pthread may be offset into a page.  In that event, by contract
 * with the kernel, the allocation will extend PTHREAD_SIZE from the
 * start of the next page.  There's also one page worth of allocation
 * below stacksize for the guard page. <rdar://problem/19941744>
 */
#define PTHREAD_SIZE ((size_t)mach_vm_round_page(sizeof(struct _pthread)))
#define PTHREAD_ALLOCADDR(stackaddr, stacksize) ((stackaddr - stacksize) - vm_page_size)
#define PTHREAD_ALLOCSIZE(stackaddr, stacksize) ((round_page((uintptr_t)stackaddr) + PTHREAD_SIZE) - (uintptr_t)PTHREAD_ALLOCADDR(stackaddr, stacksize))

static pthread_attr_t _pthread_attr_default = { };

// The main thread's pthread_t
PTHREAD_NOEXPORT struct _pthread _thread __attribute__((aligned(64))) = { };

PTHREAD_NOEXPORT int default_priority;
static int max_priority;
static int min_priority;
static int pthread_concurrency;

// work queue support data
static void (*__libdispatch_workerfunction)(pthread_priority_t) = NULL;
static void (*__libdispatch_keventfunction)(void **events, int *nevents) = NULL;
static void (*__libdispatch_workloopfunction)(uint64_t *workloop_id, void **events, int *nevents) = NULL;
static int __libdispatch_offset;

// supported feature set
int __pthread_supported_features;
static bool __workq_newapi;

//
// Function prototypes
//

// pthread primitives
static int _pthread_allocate(pthread_t *thread, const pthread_attr_t *attrs, void **stack);
static int _pthread_deallocate(pthread_t t);

static void _pthread_terminate_invoke(pthread_t t);

static inline void _pthread_struct_init(pthread_t t,
	const pthread_attr_t *attrs,
	void *stack,
	size_t stacksize,
	void *freeaddr,
	size_t freesize);

static inline void _pthread_set_self_internal(pthread_t, bool needs_tsd_base_set);

static void _pthread_dealloc_reply_port(pthread_t t);
static void _pthread_dealloc_special_reply_port(pthread_t t);

static inline void __pthread_add_thread(pthread_t t, const pthread_attr_t *attr, bool parent, bool from_mach_thread);
static inline int __pthread_remove_thread(pthread_t t, bool child, bool *should_exit);

static void _pthread_exit(pthread_t self, void *value_ptr) __dead2;

static inline void _pthread_introspection_thread_create(pthread_t t, bool destroy);
static inline void _pthread_introspection_thread_start(pthread_t t);
static inline void _pthread_introspection_thread_terminate(pthread_t t, void *freeaddr, size_t freesize, bool destroy);
static inline void _pthread_introspection_thread_destroy(pthread_t t);

extern void _pthread_set_self(pthread_t);
extern void start_wqthread(pthread_t self, mach_port_t kport, void *stackaddr, void *unused, int reuse); // trampoline into _pthread_wqthread
extern void thread_start(pthread_t self, mach_port_t kport, void *(*fun)(void *), void * funarg, size_t stacksize, unsigned int flags); // trampoline into _pthread_start

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

#define PTHREAD_START_CUSTOM		0x01000000
#define PTHREAD_START_SETSCHED		0x02000000
#define PTHREAD_START_DETACHED		0x04000000
#define PTHREAD_START_QOSCLASS		0x08000000
#define PTHREAD_START_TSD_BASE_SET	0x10000000
#define PTHREAD_START_QOSCLASS_MASK 0x00ffffff
#define PTHREAD_START_POLICY_BITSHIFT 16
#define PTHREAD_START_POLICY_MASK 0xff
#define PTHREAD_START_IMPORTANCE_MASK 0xffff

static int pthread_setschedparam_internal(pthread_t, mach_port_t, int, const struct sched_param *);
extern pthread_t __bsdthread_create(void *(*func)(void *), void * func_arg, void * stack, pthread_t  thread, unsigned int flags);
extern int __bsdthread_register(void (*)(pthread_t, mach_port_t, void *(*)(void *), void *, size_t, unsigned int), void (*)(pthread_t, mach_port_t, void *, void *, int), int,void (*)(pthread_t, mach_port_t, void *(*)(void *), void *, size_t, unsigned int), int32_t *,__uint64_t);
extern int __bsdthread_terminate(void * freeaddr, size_t freesize, mach_port_t kport, mach_port_t joinsem);
extern __uint64_t __thread_selfid( void );

extern int __workq_open(void);
extern int __workq_kernreturn(int, void *, int, int);

#if defined(__i386__) || defined(__x86_64__)
static const mach_vm_address_t PTHREAD_STACK_HINT = 0xB0000000;
#else
#error no PTHREAD_STACK_HINT for this architecture
#endif

// Check that offsets of _PTHREAD_STRUCT_DIRECT_*_OFFSET values hasn't changed
_Static_assert(offsetof(struct _pthread, tsd) + _PTHREAD_STRUCT_DIRECT_THREADID_OFFSET
		== offsetof(struct _pthread, thread_id),
		"_PTHREAD_STRUCT_DIRECT_THREADID_OFFSET is correct");

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
		allocsize = PTHREAD_SIZE;
	} else {
		guardsize = attrs->guardsize;
		stacksize = attrs->stacksize;
		allocsize = stacksize + guardsize + PTHREAD_SIZE;
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
		_pthread_struct_init(t, attrs,
				     *stack, attrs->stacksize,
				     allocaddr, allocsize);
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
		kern_return_t ret;
		ret = mach_vm_deallocate(mach_task_self(), t->freeaddr, t->freesize);
		PTHREAD_ASSERT(ret == KERN_SUCCESS);
	}
	return 0;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"

PTHREAD_NOINLINE
static void*
_pthread_current_stack_address(void)
{
	int a;
	return &a;
}

#pragma clang diagnostic pop

// Terminates the thread if called from the currently running thread.
PTHREAD_NORETURN PTHREAD_NOINLINE PTHREAD_NOT_TAIL_CALLED
static void
_pthread_terminate(pthread_t t)
{
	PTHREAD_ASSERT(t == pthread_self());

	uintptr_t freeaddr = (uintptr_t)t->freeaddr;
	size_t freesize = t->freesize;

	// the size of just the stack
	size_t freesize_stack = t->freesize;

	// We usually pass our structure+stack to bsdthread_terminate to free, but
	// if we get told to keep the pthread_t structure around then we need to
	// adjust the free size and addr in the pthread_t to just refer to the
	// structure and not the stack.  If we do end up deallocating the
	// structure, this is useless work since no one can read the result, but we
	// can't do it after the call to pthread_remove_thread because it isn't
	// safe to dereference t after that.
	if ((void*)t > t->freeaddr && (void*)t < t->freeaddr + t->freesize){
		// Check to ensure the pthread structure itself is part of the
		// allocation described by freeaddr/freesize, in which case we split and
		// only deallocate the area below the pthread structure.  In the event of a
		// custom stack, the freeaddr/size will be the pthread structure itself, in
		// which case we shouldn't free anything (the final else case).
		freesize_stack = trunc_page((uintptr_t)t - (uintptr_t)freeaddr);

		// describe just the remainder for deallocation when the pthread_t goes away
		t->freeaddr += freesize_stack;
		t->freesize -= freesize_stack;
	} else if (t == &_thread){
		freeaddr = t->stackaddr - pthread_get_stacksize_np(t);
		uintptr_t stackborder = trunc_page((uintptr_t)_pthread_current_stack_address());
		freesize_stack = stackborder - freeaddr;
	} else {
		freesize_stack = 0;
	}

	mach_port_t kport = _pthread_kernel_thread(t);
	semaphore_t joinsem = t->joiner_notify;

	_pthread_dealloc_special_reply_port(t);
	_pthread_dealloc_reply_port(t);

	// After the call to __pthread_remove_thread, it is not safe to
	// dereference the pthread_t structure.

	bool destroy, should_exit;
	destroy = (__pthread_remove_thread(t, true, &should_exit) != EBUSY);

	if (!destroy || t == &_thread) {
		// Use the adjusted freesize of just the stack that we computed above.
		freesize = freesize_stack;
	}

	// Check if there is nothing to free because the thread has a custom
	// stack allocation and is joinable.
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

PTHREAD_NORETURN
static void
_pthread_terminate_invoke(pthread_t t)
{
	_pthread_terminate(t);
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
PTHREAD_NOINLINE PTHREAD_NORETURN
static void
_pthread_body(pthread_t self, bool needs_tsd_base_set)
{
	_pthread_set_self_internal(self, needs_tsd_base_set);
	__pthread_add_thread(self, NULL, false, false);
	void *result = (self->fun)(self->arg);

	_pthread_exit(self, result);
}

PTHREAD_NORETURN
void
_pthread_start(pthread_t self,
	       mach_port_t kport,
	       void *(*fun)(void *),
	       void *arg,
	       size_t stacksize,
	       unsigned int pflags)
{
	if ((pflags & PTHREAD_START_CUSTOM) == 0) {
		void *stackaddr = self;
		_pthread_struct_init(self, &_pthread_attr_default,
				stackaddr, stacksize,
				PTHREAD_ALLOCADDR(stackaddr, stacksize), PTHREAD_ALLOCSIZE(stackaddr, stacksize));

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

	bool thread_tsd_bsd_set = (bool)(pflags & PTHREAD_START_TSD_BASE_SET);

#if DEBUG
	PTHREAD_ASSERT(MACH_PORT_VALID(kport));
	PTHREAD_ASSERT(_pthread_kernel_thread(self) == kport);
#endif
	// will mark the thread initialized
	_pthread_markcancel_if_canceled(self, kport);

	self->fun = fun;
	self->arg = arg;

	_pthread_body(self, !thread_tsd_bsd_set);
}

PTHREAD_ALWAYS_INLINE
static inline void
_pthread_struct_init(pthread_t t,
		     const pthread_attr_t *attrs,
		     void *stackaddr,
		     size_t stacksize,
		     void *freeaddr,
		     size_t freesize)
{
#if DEBUG
	PTHREAD_ASSERT(t->sig != _PTHREAD_SIG);
#endif

	t->sig = _PTHREAD_SIG;
	t->tsd[_PTHREAD_TSD_SLOT_PTHREAD_SELF] = t;
	t->tsd[_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS] = _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0);
	_PTHREAD_LOCK_INIT(t->lock);

	t->stackaddr = stackaddr;
	t->stacksize = stacksize;
	t->freeaddr = freeaddr;
	t->freesize = freesize;

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


PTHREAD_NOEXPORT_VARIANT
mach_port_t
pthread_mach_thread_np(pthread_t t)
{
	mach_port_t kport = MACH_PORT_NULL;
	(void)_pthread_is_valid(t, 0, &kport);
	return kport;
}

PTHREAD_NOEXPORT_VARIANT
pthread_t
pthread_from_mach_thread_np(mach_port_t kernel_thread)
{
	struct _pthread *p = NULL;

	/* No need to wait as mach port is already known */
	_PTHREAD_LOCK(_pthread_list_lock);

	TAILQ_FOREACH(p, &__pthread_head, plist) {
		if (_pthread_kernel_thread(p) == kernel_thread) {
			break;
		}
	}

	_PTHREAD_UNLOCK(_pthread_list_lock);

	return p;
}

PTHREAD_NOEXPORT_VARIANT
size_t
pthread_get_stacksize_np(pthread_t t)
{
	size_t size = 0;

	if (t == NULL) {
		return ESRCH; // XXX bug?
	}

#if !defined(__arm__) && !defined(__arm64__)
	// The default rlimit based allocations will be provided with a stacksize
	// of the current limit and a freesize of the max.  However, custom
	// allocations will just have the guard page to free.  If we aren't in the
	// latter case, call into rlimit to determine the current stack size.  In
	// the event that the current limit == max limit then we'll fall down the
	// fast path, but since it's unlikely that the limit is going to be lowered
	// after it's been change to the max, we should be fine.
	//
	// Of course, on arm rlim_cur == rlim_max and there's only the one guard
	// page.  So, we can skip all this there.
	if (t == &_thread && t->stacksize + vm_page_size != t->freesize) {
		// We want to call getrlimit() just once, as it's relatively expensive
		static size_t rlimit_stack;

		if (rlimit_stack == 0) {
			struct rlimit limit;
			int ret = getrlimit(RLIMIT_STACK, &limit);

			if (ret == 0) {
				rlimit_stack = (size_t) limit.rlim_cur;
			}
		}

		if (rlimit_stack == 0 || rlimit_stack > t->freesize) {
			return t->stacksize;
		} else {
			return rlimit_stack;
		}
	}
#endif /* !defined(__arm__) && !defined(__arm64__) */

	if (t == pthread_self() || t == &_thread) {
		return t->stacksize;
	}

	_PTHREAD_LOCK(_pthread_list_lock);

	if (_pthread_is_valid_locked(t)) {
		size = t->stacksize;
	} else {
		size = ESRCH; // XXX bug?
	}

	_PTHREAD_UNLOCK(_pthread_list_lock);

	return size;
}

PTHREAD_NOEXPORT_VARIANT
void *
pthread_get_stackaddr_np(pthread_t t)
{
	void *addr = NULL;

	if (t == NULL) {
		return (void *)(uintptr_t)ESRCH; // XXX bug?
	}

	// since the main thread will not get de-allocated from underneath us
	if (t == pthread_self() || t == &_thread) {
		return t->stackaddr;
	}

	_PTHREAD_LOCK(_pthread_list_lock);

	if (_pthread_is_valid_locked(t)) {
		addr = t->stackaddr;
	} else {
		addr = (void *)(uintptr_t)ESRCH; // XXX bug?
	}

	_PTHREAD_UNLOCK(_pthread_list_lock);

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

static mach_port_t
_pthread_special_reply_port(pthread_t t)
{
	void *p;
	if (t == NULL) {
		p = _pthread_getspecific_direct(_PTHREAD_TSD_SLOT_MACH_SPECIAL_REPLY);
	} else {
		p = t->tsd[_PTHREAD_TSD_SLOT_MACH_SPECIAL_REPLY];
	}
	return (mach_port_t)(uintptr_t)p;
}

static void
_pthread_dealloc_special_reply_port(pthread_t t)
{
	mach_port_t special_reply_port = _pthread_special_reply_port(t);
	if (special_reply_port != MACH_PORT_NULL) {
		mach_port_mod_refs(mach_task_self(), special_reply_port,
				MACH_PORT_RIGHT_RECEIVE, -1);
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
PTHREAD_NOEXPORT_VARIANT
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
		_PTHREAD_LOCK(_pthread_list_lock);
		if (!_pthread_is_valid_locked(thread)) {
			res = ESRCH;
		} else if (thread->thread_id == 0) {
			res = EINVAL;
		} else {
			*thread_id = thread->thread_id;
		}
		_PTHREAD_UNLOCK(_pthread_list_lock);
	}
	return res;
}

PTHREAD_NOEXPORT_VARIANT
int
pthread_getname_np(pthread_t thread, char *threadname, size_t len)
{
	int res = 0;

	if (thread == NULL) {
		return ESRCH;
	}

	_PTHREAD_LOCK(_pthread_list_lock);
	if (_pthread_is_valid_locked(thread)) {
		strlcpy(threadname, thread->pthread_name, len);
	} else {
		res = ESRCH;
	}
	_PTHREAD_UNLOCK(_pthread_list_lock);
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
__pthread_add_thread(pthread_t t, const pthread_attr_t *attrs,
		bool parent, bool from_mach_thread)
{
	bool should_deallocate = false;
	bool should_add = true;

	mach_port_t kport = _pthread_kernel_thread(t);
	if (os_slowpath(!MACH_PORT_VALID(kport))) {
		PTHREAD_CLIENT_CRASH(kport,
				"Unable to allocate thread port, possible port leak");
	}

	if (from_mach_thread) {
		_PTHREAD_LOCK_FROM_MACH_THREAD(_pthread_list_lock);
	} else {
		_PTHREAD_LOCK(_pthread_list_lock);
	}

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

		/*
		 * Set some initial values which we know in the pthread structure in
		 * case folks try to get the values before the thread can set them.
		 */
		if (parent && attrs && attrs->schedset == 0) {
			t->tsd[_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS] = attrs->qosclass;
		}
	}

	if (from_mach_thread){
		_PTHREAD_UNLOCK_FROM_MACH_THREAD(_pthread_list_lock);
	} else {
		_PTHREAD_UNLOCK(_pthread_list_lock);
	}

	if (parent) {
		if (!from_mach_thread) {
			// PR-26275485: Mach threads will likely crash trying to run
			// introspection code.  Since the fall out from the introspection
			// code not seeing the injected thread is likely less than crashing
			// in the introspection code, just don't make the call.
			_pthread_introspection_thread_create(t, should_deallocate);
		}
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

	_PTHREAD_LOCK(_pthread_list_lock);

	// When a thread removes itself:
	//  - Set the childexit flag indicating that the thread has exited.
	//  - Return false if parentcheck is zero (must keep structure)
	//  - If the thread is joinable, keep it on the list so that
	//    the join operation succeeds. Still decrement the running
	//    thread count so that we exit if no threads are running.
	//  - Update the running thread count.
	// When another thread removes a joinable thread:
	//  - CAREFUL not to dereference the thread before verifying that the
	//    reference is still valid using _pthread_is_valid_locked().
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
	} else if (!_pthread_is_valid_locked(t)) {
		ret = ESRCH;
		should_remove = false;
	} else if ((t->detached & PTHREAD_CREATE_JOINABLE) == 0) {
		// If we found a thread but it's not joinable, bail.
		ret = ESRCH;
		should_remove = false;
	} else if (t->parentcheck == 0) {
		// If we're not the child thread *and* the parent has not finished
		// creating the thread yet, then we are another thread that's joining
		// and we cannot deallocate the pthread.
		ret = EBUSY;
	}
	if (should_remove) {
		TAILQ_REMOVE(&__pthread_head, t, plist);
	}

	_PTHREAD_UNLOCK(_pthread_list_lock);

	return ret;
}

static int
_pthread_create(pthread_t *thread,
	const pthread_attr_t *attr,
	void *(*start_routine)(void *),
	void *arg,
	bool from_mach_thread)
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
		if (errno == EMFILE) {
			PTHREAD_CLIENT_CRASH(0,
					"Unable to allocate thread port, possible port leak");
		}
		if (flags & PTHREAD_START_CUSTOM) {
			// free the thread and stack if we allocated it
			_pthread_deallocate(t);
		}
		return EAGAIN;
	}
	if (t == NULL) {
		t = t2;
	}

	__pthread_add_thread(t, attrs, true, from_mach_thread);

	// n.b. if a thread is created detached and exits, t will be invalid
	*thread = t;
	return 0;
}

int
pthread_create(pthread_t *thread,
	const pthread_attr_t *attr,
	void *(*start_routine)(void *),
	void *arg)
{
	return _pthread_create(thread, attr, start_routine, arg, false);
}

int
pthread_create_from_mach_thread(pthread_t *thread,
	const pthread_attr_t *attr,
	void *(*start_routine)(void *),
	void *arg)
{
	return _pthread_create(thread, attr, start_routine, arg, true);
}

PTHREAD_NORETURN
static void
_pthread_suspended_body(pthread_t self)
{
	_pthread_set_self(self);
	__pthread_add_thread(self, NULL, false, false);
	_pthread_exit(self, (self->fun)(self->arg));
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

	t->cancel_state |= _PTHREAD_CANCEL_INITIALIZED;
	__pthread_add_thread(t, NULL, true, false);

	// Set up a suspended thread.
	_pthread_setup(t, _pthread_suspended_body, stack, 1, 0);
	return res;
}


PTHREAD_NOEXPORT_VARIANT
int
pthread_detach(pthread_t thread)
{
	int res = 0;
	bool join = false;
	semaphore_t sema = SEMAPHORE_NULL;

	if (!_pthread_is_valid(thread, PTHREAD_IS_VALID_LOCK_THREAD, NULL)) {
		return ESRCH; // Not a valid thread to detach.
	}

	if ((thread->detached & PTHREAD_CREATE_DETACHED) ||
			!(thread->detached & PTHREAD_CREATE_JOINABLE)) {
		res = EINVAL;
	} else if (thread->detached & _PTHREAD_EXITED) {
		// Join the thread if it's already exited.
		join = true;
	} else {
		thread->detached &= ~PTHREAD_CREATE_JOINABLE;
		thread->detached |= PTHREAD_CREATE_DETACHED;
		sema = thread->joiner_notify;
	}

	_PTHREAD_UNLOCK(thread->lock);

	if (join) {
		pthread_join(thread, NULL);
	} else if (sema) {
		semaphore_signal(sema);
	}

	return res;
}

PTHREAD_NOEXPORT_VARIANT
int
pthread_kill(pthread_t th, int sig)
{
	if (sig < 0 || sig > NSIG) {
		return EINVAL;
	}

	mach_port_t kport = MACH_PORT_NULL;
	if (!_pthread_is_valid(th, 0, &kport)) {
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

PTHREAD_NOEXPORT_VARIANT
int
__pthread_workqueue_setkill(int enable)
{
	pthread_t self = pthread_self();

	_PTHREAD_LOCK(self->lock);
	self->wqkillset = enable ? 1 : 0;
	_PTHREAD_UNLOCK(self->lock);

	return 0;
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

	_PTHREAD_LOCK(self->lock);
	self->detached |= _PTHREAD_EXITED;
	self->exit_value = value_ptr;

	if ((self->detached & PTHREAD_CREATE_JOINABLE) &&
			self->joiner_notify == SEMAPHORE_NULL) {
		self->joiner_notify = (semaphore_t)os_get_cached_semaphore();
	}
	_PTHREAD_UNLOCK(self->lock);

	// Clear per-thread semaphore cache
	os_put_cached_semaphore(SEMAPHORE_NULL);

	_pthread_terminate_invoke(self);
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


PTHREAD_NOEXPORT_VARIANT
int
pthread_getschedparam(pthread_t thread,
		      int *policy,
		      struct sched_param *param)
{
	int ret = 0;

	if (thread == NULL) {
		return ESRCH;
	}

	_PTHREAD_LOCK(_pthread_list_lock);

	if (_pthread_is_valid_locked(thread)) {
		if (policy) {
			*policy = thread->policy;
		}
		if (param) {
			*param = thread->param;
		}
	} else {
		ret = ESRCH;
	}

	_PTHREAD_UNLOCK(_pthread_list_lock);

	return ret;
}


PTHREAD_ALWAYS_INLINE
static inline int
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


PTHREAD_NOEXPORT_VARIANT
int
pthread_setschedparam(pthread_t t, int policy, const struct sched_param *param)
{
	mach_port_t kport = MACH_PORT_NULL;
	int res;
	int bypass = 1;

	// since the main thread will not get de-allocated from underneath us
	if (t == pthread_self() || t == &_thread) {
		kport = _pthread_kernel_thread(t);
	} else {
		bypass = 0;
		(void)_pthread_is_valid(t, 0, &kport);
	}

	res = pthread_setschedparam_internal(t, kport, policy, param);
	if (res == 0) {
		if (bypass == 0) {
			// Ensure the thread is still valid.
			_PTHREAD_LOCK(_pthread_list_lock);
			if (_pthread_is_valid_locked(t)) {
				t->policy = policy;
				t->param = *param;
			} else {
				res = ESRCH;
			}
			_PTHREAD_UNLOCK(_pthread_list_lock);
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

/*
 * Force LLVM not to optimise this to a call to __pthread_set_self, if it does
 * then _pthread_set_self won't be bound when secondary threads try and start up.
 */
PTHREAD_NOINLINE
void
_pthread_set_self(pthread_t p)
{
	return _pthread_set_self_internal(p, true);
}

PTHREAD_ALWAYS_INLINE
static inline void
_pthread_set_self_internal(pthread_t p, bool needs_tsd_base_set)
{
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

	if (needs_tsd_base_set) {
		_thread_set_tsd_base(&p->tsd[0]);
	}
}


// <rdar://problem/28984807> pthread_once should have an acquire barrier
PTHREAD_ALWAYS_INLINE
static inline void
_os_once_acquire(os_once_t *predicate, void *context, os_function_t function)
{
	if (OS_EXPECT(os_atomic_load(predicate, acquire), ~0l) != ~0l) {
		_os_once(predicate, context, function);
		OS_COMPILER_CAN_ASSUME(*predicate == ~0l);
	}
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

PTHREAD_NOEXPORT_VARIANT
int
pthread_once(pthread_once_t *once_control, void (*init_routine)(void))
{
	struct _pthread_once_context ctx = { once_control, init_routine };
	do {
		_os_once_acquire(&once_control->once, &ctx, __pthread_once_handler);
	} while (once_control->sig == _PTHREAD_ONCE_SIG_init);
	return 0;
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

static unsigned long
_pthread_strtoul(const char *p, const char **endptr, int base)
{
	uintptr_t val = 0;

	// Expect hex string starting with "0x"
	if ((base == 16 || base == 0) && p && p[0] == '0' && p[1] == 'x') {
		p += 2;
		while (1) {
			char c = *p;
			if ('0' <= c && c <= '9') {
				val = (val << 4) + (c - '0');
			} else if ('a' <= c && c <= 'f') {
				val = (val << 4) + (c - 'a' + 10);
			} else if ('A' <= c && c <= 'F') {
				val = (val << 4) + (c - 'A' + 10);
			} else {
				break;
			}
			++p;
		}
	}

	*endptr = (char *)p;
	return val;
}

static int
parse_main_stack_params(const char *apple[],
			void **stackaddr,
			size_t *stacksize,
			void **allocaddr,
			size_t *allocsize)
{
	const char *p = _simple_getenv(apple, "main_stack");
	if (!p) return 0;

	int ret = 0;
	const char *s = p;

	*stackaddr = _pthread_strtoul(s, &s, 16);
	if (*s != ',') goto out;

	*stacksize = _pthread_strtoul(s + 1, &s, 16);
	if (*s != ',') goto out;

	*allocaddr = _pthread_strtoul(s + 1, &s, 16);
	if (*s != ',') goto out;

	*allocsize = _pthread_strtoul(s + 1, &s, 16);
	if (*s != ',' && *s != 0) goto out;

	ret = 1;
out:
	bzero((char *)p, strlen(p));
	return ret;
}

#if !defined(VARIANT_STATIC)
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
#endif // VARIANT_STATIC

/*
 * Perform package initialization - called automatically when application starts
 */
struct ProgramVars; /* forward reference */

int
__pthread_init(const struct _libpthread_functions *pthread_funcs,
	       const char *envp[] __unused,
	       const char *apple[],
	       const struct ProgramVars *vars __unused)
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

	// Get the address and size of the main thread's stack from the kernel.
	void *stackaddr = 0;
	size_t stacksize = 0;
	void *allocaddr = 0;
	size_t allocsize = 0;
	if (!parse_main_stack_params(apple, &stackaddr, &stacksize, &allocaddr, &allocsize) ||
		stackaddr == NULL || stacksize == 0) {
		// Fall back to previous bevhaior.
		size_t len = sizeof(stackaddr);
		int mib[] = { CTL_KERN, KERN_USRSTACK };
		if (__sysctl(mib, 2, &stackaddr, &len, NULL, 0) != 0) {
#if defined(__LP64__)
			stackaddr = (void *)USRSTACK64;
#else
			stackaddr = (void *)USRSTACK;
#endif
		}
		stacksize = DFLSSIZ;
		allocaddr = 0;
		allocsize = 0;
	}

	pthread_t thread = &_thread;
	pthread_attr_init(&_pthread_attr_default);
	_pthread_struct_init(thread, &_pthread_attr_default,
			     stackaddr, stacksize,
			     allocaddr, allocsize);
	thread->detached = PTHREAD_CREATE_JOINABLE;

	// Finish initialization with common code that is reinvoked on the
	// child side of a fork.

	// Finishes initialization of main thread attributes.
	// Initializes the thread list and add the main thread.
	// Calls _pthread_set_self() to prepare the main thread for execution.
	_pthread_main_thread_init(thread);

	// Set up kernel entry points with __bsdthread_register.
	_pthread_bsdthread_init();

	// Have pthread_key do its init envvar checks.
	_pthread_key_global_init(envp);

#if PTHREAD_DEBUG_LOG
	_SIMPLE_STRING path = _simple_salloc();
	_simple_sprintf(path, "/var/tmp/libpthread.%d.log", getpid());
	_pthread_debuglog = open(_simple_string(path),
			O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0666);
	_simple_sfree(path);
	_pthread_debugstart = mach_absolute_time();
#endif

	return 0;
}

PTHREAD_NOEXPORT void
_pthread_main_thread_init(pthread_t p)
{
	TAILQ_INIT(&__pthread_head);
	_PTHREAD_LOCK_INIT(_pthread_list_lock);

	// Re-use the main thread's static storage if no thread was provided.
	if (p == NULL) {
		if (_thread.tsd[0] != 0) {
			bzero(&_thread, sizeof(struct _pthread));
		}
		p = &_thread;
	}

	_PTHREAD_LOCK_INIT(p->lock);
	_pthread_set_kernel_thread(p, mach_thread_self());
	_pthread_set_reply_port(p, mach_reply_port());
	p->__cleanup_stack = NULL;
	p->joiner_notify = SEMAPHORE_NULL;
	p->joiner = MACH_PORT_NULL;
	p->detached |= _PTHREAD_CREATE_PARENT;
	p->tsd[__TSD_SEMAPHORE_CACHE] = (void*)SEMAPHORE_NULL;
	p->cancel_state |= _PTHREAD_CANCEL_INITIALIZED;

	// Initialize the list of threads with the new main thread.
	TAILQ_INSERT_HEAD(&__pthread_head, p, plist);
	_pthread_count = 1;

	_pthread_set_self(p);
	_pthread_introspection_thread_start(p);
}

int
_pthread_join_cleanup(pthread_t thread, void ** value_ptr, int conforming)
{
	int ret = __pthread_remove_thread(thread, false, NULL);
	if (ret != 0 && ret != EBUSY) {
		// Returns ESRCH if the thread was not created joinable.
		return ret;
	}

	if (value_ptr) {
		*value_ptr = _pthread_get_exit_value(thread, conforming);
	}
	_pthread_introspection_thread_destroy(thread);
	if (ret != EBUSY) {
		// __pthread_remove_thread returns EBUSY if the parent has not
		// finished creating the thread (and is still expecting the pthread_t
		// to be alive).
		_pthread_deallocate(thread);
	}
	return 0;
}

int
sched_yield(void)
{
    swtch_pri(0);
    return 0;
}

// XXX remove
void
cthread_yield(void)
{
	sched_yield();
}

void
pthread_yield_np(void)
{
	sched_yield();
}



PTHREAD_NOEXPORT_VARIANT
void
_pthread_clear_qos_tsd(mach_port_t thread_port)
{
	if (thread_port == MACH_PORT_NULL || (uintptr_t)_pthread_getspecific_direct(_PTHREAD_TSD_SLOT_MACH_THREAD_SELF) == thread_port) {
		/* Clear the current thread's TSD, that can be done inline. */
		_pthread_setspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS, _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0));
	} else {
		pthread_t p;

		_PTHREAD_LOCK(_pthread_list_lock);

		TAILQ_FOREACH(p, &__pthread_head, plist) {
			mach_port_t kp = _pthread_kernel_thread(p);
			if (thread_port == kp) {
				p->tsd[_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS] = _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0);
				break;
			}
		}

		_PTHREAD_UNLOCK(_pthread_list_lock);
	}
}


/***** pthread workqueue support routines *****/

PTHREAD_NOEXPORT void
_pthread_bsdthread_init(void)
{
	struct _pthread_registration_data data = {};
	data.version = sizeof(struct _pthread_registration_data);
	data.dispatch_queue_offset = __PTK_LIBDISPATCH_KEY0 * sizeof(void *);
	data.return_to_kernel_offset = __TSD_RETURN_TO_KERNEL * sizeof(void *);
	data.tsd_offset = offsetof(struct _pthread, tsd);
	data.mach_thread_self_offset = __TSD_MACH_THREAD_SELF * sizeof(void *);

	int rv = __bsdthread_register(thread_start,
			start_wqthread, (int)PTHREAD_SIZE,
			(void*)&data, (uintptr_t)sizeof(data),
			data.dispatch_queue_offset);

	if (rv > 0) {
		if ((rv & PTHREAD_FEATURE_QOS_DEFAULT) == 0) {
			PTHREAD_INTERNAL_CRASH(rv,
					"Missing required support for QOS_CLASS_DEFAULT");
		}
		if ((rv & PTHREAD_FEATURE_QOS_MAINTENANCE) == 0) {
			PTHREAD_INTERNAL_CRASH(rv,
					"Missing required support for QOS_CLASS_MAINTENANCE");
		}
		__pthread_supported_features = rv;
	}

	pthread_priority_t main_qos = (pthread_priority_t)data.main_qos;

	if (_pthread_priority_get_qos_newest(main_qos) != QOS_CLASS_UNSPECIFIED) {
		_pthread_set_main_qos(main_qos);
		_thread.tsd[_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS] = main_qos;
	}

	if (__libdispatch_workerfunction != NULL) {
		// prepare the kernel for workq action
		(void)__workq_open();
	}
}

// workqueue entry point from kernel
PTHREAD_NORETURN
void
_pthread_wqthread(pthread_t self, mach_port_t kport, void *stacklowaddr, void *keventlist, int flags, int nkevents)
{
	PTHREAD_ASSERT(flags & WQ_FLAG_THREAD_NEWSPI);

	bool thread_reuse = flags & WQ_FLAG_THREAD_REUSE;
	bool overcommit = flags & WQ_FLAG_THREAD_OVERCOMMIT;
	bool kevent = flags & WQ_FLAG_THREAD_KEVENT;
	bool workloop = (flags & WQ_FLAG_THREAD_WORKLOOP) &&
			__libdispatch_workloopfunction != NULL;
	PTHREAD_ASSERT((!kevent) || (__libdispatch_keventfunction != NULL));
	PTHREAD_ASSERT(!workloop || kevent);

	pthread_priority_t priority = 0;
	unsigned long priority_flags = 0;

	if (overcommit)
		priority_flags |= _PTHREAD_PRIORITY_OVERCOMMIT_FLAG;
	if (flags & WQ_FLAG_THREAD_EVENT_MANAGER)
		priority_flags |= _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;
	if (kevent)
		priority_flags |= _PTHREAD_PRIORITY_NEEDS_UNBIND_FLAG;

	int thread_class = flags & WQ_FLAG_THREAD_PRIOMASK;
	priority = _pthread_priority_make_newest(thread_class, 0, priority_flags);

	if (!thread_reuse) {
		// New thread created by kernel, needs initialization.
		void *stackaddr = self;
		size_t stacksize = (uintptr_t)self - (uintptr_t)stacklowaddr;

		_pthread_struct_init(self, &_pthread_attr_default,
							 stackaddr, stacksize,
							 PTHREAD_ALLOCADDR(stackaddr, stacksize), PTHREAD_ALLOCSIZE(stackaddr, stacksize));

		_pthread_set_kernel_thread(self, kport);
		self->wqthread = 1;
		self->wqkillset = 0;
		self->cancel_state |= _PTHREAD_CANCEL_INITIALIZED;

		// Not a joinable thread.
		self->detached &= ~PTHREAD_CREATE_JOINABLE;
		self->detached |= PTHREAD_CREATE_DETACHED;

		// Update the running thread count and set childrun bit.
		bool thread_tsd_base_set = (bool)(flags & WQ_FLAG_THREAD_TSD_BASE_SET);
		_pthread_set_self_internal(self, !thread_tsd_base_set);
		_pthread_introspection_thread_create(self, false);
		__pthread_add_thread(self, NULL, false, false);
	}

	// If we're running with fine-grained priority, we also need to
	// set this thread to have the QoS class provided to use by the kernel
	if (__pthread_supported_features & PTHREAD_FEATURE_FINEPRIO) {
		_pthread_setspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS, _pthread_priority_make_newest(thread_class, 0, priority_flags));
	}

#if WQ_DEBUG
	PTHREAD_ASSERT(self);
	PTHREAD_ASSERT(self == pthread_self());
#endif // WQ_DEBUG

	if (workloop) {
		self->fun = (void *(*)(void*))__libdispatch_workloopfunction;
	} else if (kevent){
		self->fun = (void *(*)(void*))__libdispatch_keventfunction;
	} else {
		self->fun = (void *(*)(void*))__libdispatch_workerfunction;
	}
	self->arg = (void *)(uintptr_t)thread_class;

	if (kevent && keventlist && nkevents > 0){
		int errors_out;
	kevent_errors_retry:

		if (workloop) {
			kqueue_id_t kevent_id = *(kqueue_id_t*)((char*)keventlist - sizeof(kqueue_id_t));
			kqueue_id_t kevent_id_in = kevent_id;
			(__libdispatch_workloopfunction)(&kevent_id, &keventlist, &nkevents);
			PTHREAD_ASSERT(kevent_id == kevent_id_in || nkevents == 0);
			errors_out = __workq_kernreturn(WQOPS_THREAD_WORKLOOP_RETURN, keventlist, nkevents, 0);
		} else {
			(__libdispatch_keventfunction)(&keventlist, &nkevents);
			errors_out = __workq_kernreturn(WQOPS_THREAD_KEVENT_RETURN, keventlist, nkevents, 0);
		}

		if (errors_out > 0){
			nkevents = errors_out;
			goto kevent_errors_retry;
		} else if (errors_out < 0){
			PTHREAD_ABORT("kevent return produced an error: %d", errno);
		}
		goto thexit;
    } else if (kevent){
		if (workloop) {
			(__libdispatch_workloopfunction)(0, NULL, NULL);
			__workq_kernreturn(WQOPS_THREAD_WORKLOOP_RETURN, NULL, 0, -1);
		} else {
			(__libdispatch_keventfunction)(NULL, NULL);
			__workq_kernreturn(WQOPS_THREAD_KEVENT_RETURN, NULL, 0, 0);
		}

		goto thexit;
    }

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

thexit:
	{
		pthread_priority_t current_priority = _pthread_getspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS);
		if ((current_priority & _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG) ||
			(_pthread_priority_get_qos_newest(current_priority) > WQ_THREAD_CLEANUP_QOS)) {
			// Reset QoS to something low for the cleanup process
			priority = _pthread_priority_make_newest(WQ_THREAD_CLEANUP_QOS, 0, 0);
			_pthread_setspecific_direct(_PTHREAD_TSD_SLOT_PTHREAD_QOS_CLASS, priority);
		}
	}

	_pthread_exit(self, NULL);
}

/***** pthread workqueue API for libdispatch *****/

_Static_assert(WORKQ_KEVENT_EVENT_BUFFER_LEN == WQ_KEVENT_LIST_LEN,
		"Kernel and userland should agree on the event list size");

void
pthread_workqueue_setdispatchoffset_np(int offset)
{
	__libdispatch_offset = offset;
}

static int
pthread_workqueue_setdispatch_with_workloop_np(pthread_workqueue_function2_t queue_func,
		pthread_workqueue_function_kevent_t kevent_func,
		pthread_workqueue_function_workloop_t workloop_func)
{
	int res = EBUSY;
	if (__libdispatch_workerfunction == NULL) {
		// Check whether the kernel supports new SPIs
		res = __workq_kernreturn(WQOPS_QUEUE_NEWSPISUPP, NULL, __libdispatch_offset, kevent_func != NULL ? 0x01 : 0x00);
		if (res == -1){
			res = ENOTSUP;
		} else {
			__libdispatch_workerfunction = queue_func;
			__libdispatch_keventfunction = kevent_func;
			__libdispatch_workloopfunction = workloop_func;

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
_pthread_workqueue_init_with_workloop(pthread_workqueue_function2_t queue_func,
		pthread_workqueue_function_kevent_t kevent_func,
		pthread_workqueue_function_workloop_t workloop_func,
		int offset, int flags)
{
	if (flags != 0) {
		return ENOTSUP;
	}

	__workq_newapi = true;
	__libdispatch_offset = offset;

	int rv = pthread_workqueue_setdispatch_with_workloop_np(queue_func, kevent_func, workloop_func);
	return rv;
}

int
_pthread_workqueue_init_with_kevent(pthread_workqueue_function2_t queue_func,
		pthread_workqueue_function_kevent_t kevent_func,
		int offset, int flags)
{
	return _pthread_workqueue_init_with_workloop(queue_func, kevent_func, NULL, offset, flags);
}

int
_pthread_workqueue_init(pthread_workqueue_function2_t func, int offset, int flags)
{
	return _pthread_workqueue_init_with_kevent(func, NULL, offset, flags);
}

int
pthread_workqueue_setdispatch_np(pthread_workqueue_function_t worker_func)
{
	return pthread_workqueue_setdispatch_with_workloop_np((pthread_workqueue_function2_t)worker_func, NULL, NULL);
}

int
_pthread_workqueue_supported(void)
{
	if (os_unlikely(!__pthread_supported_features)) {
		PTHREAD_INTERNAL_CRASH(0, "libpthread has not been initialized");
	}

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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		kp = _pthread_qos_class_encode_workqueue(compat_priority, flags);
#pragma clang diagnostic pop

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

bool
_pthread_workqueue_should_narrow(pthread_priority_t pri)
{
	int res = __workq_kernreturn(WQOPS_SHOULD_NARROW, NULL, (int)pri, 0);
	if (res == -1) {
		return false;
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

int
_pthread_workqueue_set_event_manager_priority(pthread_priority_t priority)
{
	int res = __workq_kernreturn(WQOPS_SET_EVENT_MANAGER_PRIORITY, NULL, (int)priority, 0);
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
	pthread_introspection_hook_t prev;
	prev = _pthread_atomic_xchg_ptr((void**)&_pthread_introspection_hook, hook);
	return prev;
}

PTHREAD_NOINLINE
static void
_pthread_introspection_hook_callout_thread_create(pthread_t t, bool destroy)
{
	_pthread_introspection_hook(PTHREAD_INTROSPECTION_THREAD_CREATE, t, t,
			PTHREAD_SIZE);
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
		freesize = t->freesize - PTHREAD_SIZE;
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
		freesize -= PTHREAD_SIZE;
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
			PTHREAD_SIZE);
}

static inline void
_pthread_introspection_thread_destroy(pthread_t t)
{
	if (os_fastpath(!_pthread_introspection_hook)) return;
	_pthread_introspection_hook_callout_thread_destroy(t);
}

