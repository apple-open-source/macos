/*
 * Copyright (c) 2000-2012 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 * 
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1995-2005 Apple Computer, Inc. All Rights Reserved */
/*
 *	pthread_synch.c
 */

#pragma mark - Front Matter

#define  _PTHREAD_CONDATTR_T
#define  _PTHREAD_COND_T
#define _PTHREAD_MUTEXATTR_T
#define _PTHREAD_MUTEX_T
#define _PTHREAD_RWLOCKATTR_T
#define _PTHREAD_RWLOCK_T

#undef pthread_mutexattr_t
#undef pthread_mutex_t
#undef pthread_condattr_t
#undef pthread_cond_t
#undef pthread_rwlockattr_t
#undef pthread_rwlock_t

#include <sys/cdefs.h>

// <rdar://problem/26158937> panic() should be marked noreturn
extern void panic(const char *string, ...) __printflike(1,2) __dead2;

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/resourcevar.h>
//#include <sys/proc_internal.h>
#include <sys/kauth.h>
#include <sys/systm.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/wait.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <sys/lock.h>
#include <sys/kdebug.h>
//#include <sys/sysproto.h>
#include <sys/vm.h>
#include <sys/user.h>		/* for coredump */
#include <sys/proc_info.h>	/* for fill_procworkqueue */

#include <mach/mach_port.h>
#include <mach/mach_types.h>
#include <mach/semaphore.h>
#include <mach/sync_policy.h>
#include <mach/task.h>
#include <mach/vm_prot.h>
#include <kern/kern_types.h>
#include <kern/task.h>
#include <kern/clock.h>
#include <mach/kern_return.h>
#include <kern/thread.h>
#include <kern/sched_prim.h>
#include <kern/kalloc.h>
#include <kern/sched_prim.h>	/* for thread_exception_return */
#include <kern/processor.h>
#include <kern/assert.h>
#include <mach/mach_vm.h>
#include <mach/mach_param.h>
#include <mach/thread_status.h>
#include <mach/thread_policy.h>
#include <mach/message.h>
#include <mach/port.h>
//#include <vm/vm_protos.h>
#include <vm/vm_fault.h>
#include <vm/vm_map.h>
#include <mach/thread_act.h> /* for thread_resume */
#include <machine/machine_routines.h>
#include <mach/shared_region.h>

#include <libkern/OSAtomic.h>
#include <libkern/libkern.h>

#include <sys/pthread_shims.h>
#include "kern_internal.h"

// XXX: Dirty import for sys/signarvar.h that's wrapped in BSD_KERNEL_PRIVATE
#define sigcantmask (sigmask(SIGKILL) | sigmask(SIGSTOP))

// XXX: Ditto for thread tags from kern/thread.h
#define	THREAD_TAG_MAINTHREAD 0x1
#define	THREAD_TAG_PTHREAD 0x10
#define	THREAD_TAG_WORKQUEUE 0x20

lck_grp_attr_t   *pthread_lck_grp_attr;
lck_grp_t    *pthread_lck_grp;
lck_attr_t   *pthread_lck_attr;

extern void thread_set_cthreadself(thread_t thread, uint64_t pself, int isLP64);
extern void workqueue_thread_yielded(void);

enum run_nextreq_mode {
	RUN_NEXTREQ_DEFAULT,
	RUN_NEXTREQ_DEFAULT_KEVENT,
	RUN_NEXTREQ_OVERCOMMIT,
	RUN_NEXTREQ_OVERCOMMIT_KEVENT,
	RUN_NEXTREQ_DEFERRED_OVERCOMMIT,
	RUN_NEXTREQ_UNCONSTRAINED,
	RUN_NEXTREQ_EVENT_MANAGER,
	RUN_NEXTREQ_ADD_TIMER
};
static thread_t workqueue_run_nextreq(proc_t p, struct workqueue *wq, thread_t th,
		enum run_nextreq_mode mode, pthread_priority_t prio,
		bool kevent_bind_via_return);

static boolean_t workqueue_run_one(proc_t p, struct workqueue *wq, boolean_t overcommit, pthread_priority_t priority);

static void wq_runreq(proc_t p, thread_t th, struct workqueue *wq,
		struct threadlist *tl, boolean_t return_directly, boolean_t deferred_kevent);

static void _setup_wqthread(proc_t p, thread_t th, struct workqueue *wq, struct threadlist *tl, bool first_use);

static void reset_priority(struct threadlist *tl, pthread_priority_t pri);
static pthread_priority_t pthread_priority_from_wq_class_index(struct workqueue *wq, int index);

static void wq_unpark_continue(void* ptr, wait_result_t wait_result) __dead2;

static boolean_t workqueue_addnewthread(struct workqueue *wq, boolean_t ignore_constrained_thread_limit);

static void workqueue_removethread(struct threadlist *tl, bool fromexit, bool first_use);
static void workqueue_lock_spin(struct workqueue *);
static void workqueue_unlock(struct workqueue *);

static boolean_t may_start_constrained_thread(struct workqueue *wq, uint32_t at_priclass, uint32_t my_priclass, boolean_t *start_timer);

static mach_vm_offset_t stack_addr_hint(proc_t p, vm_map_t vmap);

int proc_settargetconc(pid_t pid, int queuenum, int32_t targetconc);
int proc_setalltargetconc(pid_t pid, int32_t * targetconcp);

#define WQ_MAXPRI_MIN	0	/* low prio queue num */
#define WQ_MAXPRI_MAX	2	/* max  prio queuenum */
#define WQ_PRI_NUM	3	/* number of prio work queues */

#define C_32_STK_ALIGN          16
#define C_64_STK_ALIGN          16
#define C_64_REDZONE_LEN        128

#define PTHREAD_T_OFFSET 0

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
#define PTHREAD_START_QOSCLASS_MASK	0x00ffffff
#define PTHREAD_START_POLICY_BITSHIFT 16
#define PTHREAD_START_POLICY_MASK 0xff
#define PTHREAD_START_IMPORTANCE_MASK 0xffff

#define SCHED_OTHER      POLICY_TIMESHARE
#define SCHED_FIFO       POLICY_FIFO
#define SCHED_RR         POLICY_RR

#define BASEPRI_DEFAULT 31

#pragma mark sysctls

uint32_t wq_yielded_threshold		= WQ_YIELDED_THRESHOLD;
uint32_t wq_yielded_window_usecs	= WQ_YIELDED_WINDOW_USECS;
uint32_t wq_stalled_window_usecs	= WQ_STALLED_WINDOW_USECS;
uint32_t wq_reduce_pool_window_usecs	= WQ_REDUCE_POOL_WINDOW_USECS;
uint32_t wq_max_timer_interval_usecs	= WQ_MAX_TIMER_INTERVAL_USECS;
uint32_t wq_max_threads			= WORKQUEUE_MAXTHREADS;
uint32_t wq_max_constrained_threads	= WORKQUEUE_MAXTHREADS / 8;
uint32_t wq_max_concurrency = 1; // set to ncpus on load

SYSCTL_INT(_kern, OID_AUTO, wq_yielded_threshold, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_yielded_threshold, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_yielded_window_usecs, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_yielded_window_usecs, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_stalled_window_usecs, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_stalled_window_usecs, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_reduce_pool_window_usecs, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_reduce_pool_window_usecs, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_max_timer_interval_usecs, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_max_timer_interval_usecs, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_max_threads, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_max_threads, 0, "");

SYSCTL_INT(_kern, OID_AUTO, wq_max_constrained_threads, CTLFLAG_RW | CTLFLAG_LOCKED,
	   &wq_max_constrained_threads, 0, "");

#ifdef DEBUG
SYSCTL_INT(_kern, OID_AUTO, wq_max_concurrency, CTLFLAG_RW | CTLFLAG_LOCKED,
		   &wq_max_concurrency, 0, "");

static int wq_kevent_test SYSCTL_HANDLER_ARGS;
SYSCTL_PROC(_debug, OID_AUTO, wq_kevent_test, CTLFLAG_MASKED | CTLFLAG_RW | CTLFLAG_LOCKED | CTLFLAG_ANYBODY | CTLTYPE_OPAQUE, NULL, 0, wq_kevent_test, 0, "-");
#endif

static uint32_t wq_init_constrained_limit = 1;

uint32_t pthread_debug_tracing = 1;

SYSCTL_INT(_kern, OID_AUTO, pthread_debug_tracing, CTLFLAG_RW | CTLFLAG_LOCKED,
		   &pthread_debug_tracing, 0, "")


#pragma mark - Process/Thread Setup/Teardown syscalls

static mach_vm_offset_t
stack_addr_hint(proc_t p, vm_map_t vmap)
{
	mach_vm_offset_t stackaddr;
	mach_vm_offset_t aslr_offset;
	bool proc64bit = proc_is64bit(p);

	// We can't safely take random values % something unless its a power-of-two
	_Static_assert(powerof2(PTH_DEFAULT_STACKSIZE), "PTH_DEFAULT_STACKSIZE is a power-of-two");

#if defined(__i386__) || defined(__x86_64__)
	if (proc64bit) {
		// Matches vm_map_get_max_aslr_slide_pages's image shift in xnu
		aslr_offset = random() % (1 << 28); // about 512 stacks
	} else {
		// Actually bigger than the image shift, we've got ~256MB to work with
		aslr_offset = random() % (16 * PTH_DEFAULT_STACKSIZE);
	}
	aslr_offset = vm_map_trunc_page_mask(aslr_offset, vm_map_page_mask(vmap));
	if (proc64bit) {
		// Above nanomalloc range (see NANOZONE_SIGNATURE)
		stackaddr = 0x700000000000 + aslr_offset;
	} else {
		stackaddr = SHARED_REGION_BASE_I386 + SHARED_REGION_SIZE_I386 + aslr_offset;
	}
#elif defined(__arm__) || defined(__arm64__)
	// vm_map_get_max_aslr_slide_pages ensures 1MB of slide, we do better
	aslr_offset = random() % ((proc64bit ? 4 : 2) * PTH_DEFAULT_STACKSIZE);
	aslr_offset = vm_map_trunc_page_mask((vm_map_offset_t)aslr_offset, vm_map_page_mask(vmap));
	if (proc64bit) {
		// 64 stacks below nanomalloc (see NANOZONE_SIGNATURE)
		stackaddr = 0x170000000 - 64 * PTH_DEFAULT_STACKSIZE - aslr_offset;
	} else {
		// If you try to slide down from this point, you risk ending up in memory consumed by malloc
		stackaddr = SHARED_REGION_BASE_ARM - 32 * PTH_DEFAULT_STACKSIZE + aslr_offset;
	}
#else
#error Need to define a stack address hint for this architecture
#endif
	return stackaddr;
}

/**
 * bsdthread_create system call.  Used by pthread_create.
 */
int
_bsdthread_create(struct proc *p, user_addr_t user_func, user_addr_t user_funcarg, user_addr_t user_stack, user_addr_t user_pthread, uint32_t flags, user_addr_t *retval)
{
	kern_return_t kret;
	void * sright;
	int error = 0;
	int allocated = 0;
	mach_vm_offset_t stackaddr;
	mach_vm_size_t th_allocsize = 0;
	mach_vm_size_t th_guardsize;
	mach_vm_offset_t th_stack;
	mach_vm_offset_t th_pthread;
	mach_vm_offset_t th_tsd_base;
	mach_port_name_t th_thport;
	thread_t th;
	vm_map_t vmap = pthread_kern->current_map();
	task_t ctask = current_task();
	unsigned int policy, importance;
	uint32_t tsd_offset;

	int isLP64 = 0;

	if (pthread_kern->proc_get_register(p) == 0) {
		return EINVAL;
	}

	PTHREAD_TRACE(TRACE_pthread_thread_create | DBG_FUNC_START, flags, 0, 0, 0, 0);

	isLP64 = proc_is64bit(p);
	th_guardsize = vm_map_page_size(vmap);

	stackaddr = pthread_kern->proc_get_stack_addr_hint(p);
	kret = pthread_kern->thread_create(ctask, &th);
	if (kret != KERN_SUCCESS)
		return(ENOMEM);
	thread_reference(th);

	pthread_kern->thread_set_tag(th, THREAD_TAG_PTHREAD);

	sright = (void *)pthread_kern->convert_thread_to_port(th);
	th_thport = pthread_kern->ipc_port_copyout_send(sright, pthread_kern->task_get_ipcspace(ctask));

	if ((flags & PTHREAD_START_CUSTOM) == 0) {
		mach_vm_size_t pthread_size =
			vm_map_round_page_mask(pthread_kern->proc_get_pthsize(p) + PTHREAD_T_OFFSET, vm_map_page_mask(vmap));
		th_allocsize = th_guardsize + user_stack + pthread_size;
		user_stack += PTHREAD_T_OFFSET;

		kret = mach_vm_map(vmap, &stackaddr,
				th_allocsize,
				page_size-1,
				VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE , NULL,
				0, FALSE, VM_PROT_DEFAULT, VM_PROT_ALL,
				VM_INHERIT_DEFAULT);
		if (kret != KERN_SUCCESS){
			kret = mach_vm_allocate(vmap,
					&stackaddr, th_allocsize,
					VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE);
		}
		if (kret != KERN_SUCCESS) {
			error = ENOMEM;
			goto out;
		}

		PTHREAD_TRACE(TRACE_pthread_thread_create|DBG_FUNC_NONE, th_allocsize, stackaddr, 0, 2, 0);

		allocated = 1;
		/*
		 * The guard page is at the lowest address
		 * The stack base is the highest address
		 */
		kret = mach_vm_protect(vmap,  stackaddr, th_guardsize, FALSE, VM_PROT_NONE);

		if (kret != KERN_SUCCESS) { 
			error = ENOMEM;
			goto out1;
		}

		th_pthread = stackaddr + th_guardsize + user_stack;
		th_stack = th_pthread;
		
		/*
		* Pre-fault the first page of the new thread's stack and the page that will
		* contain the pthread_t structure.
		*/	
		if (vm_map_trunc_page_mask((vm_map_offset_t)(th_stack - C_64_REDZONE_LEN), vm_map_page_mask(vmap)) != 
				vm_map_trunc_page_mask((vm_map_offset_t)th_pthread, vm_map_page_mask(vmap))){
			vm_fault( vmap,
					vm_map_trunc_page_mask((vm_map_offset_t)(th_stack - C_64_REDZONE_LEN), vm_map_page_mask(vmap)),
					VM_PROT_READ | VM_PROT_WRITE,
					FALSE, 
					THREAD_UNINT, NULL, 0);
		}
		
		vm_fault( vmap,
				vm_map_trunc_page_mask((vm_map_offset_t)th_pthread, vm_map_page_mask(vmap)),
				VM_PROT_READ | VM_PROT_WRITE,
				FALSE, 
				THREAD_UNINT, NULL, 0);

	} else {
		th_stack = user_stack;
		th_pthread = user_pthread;

		PTHREAD_TRACE(TRACE_pthread_thread_create|DBG_FUNC_NONE, 0, 0, 0, 3, 0);
	}

	tsd_offset = pthread_kern->proc_get_pthread_tsd_offset(p);
	if (tsd_offset) {
		th_tsd_base = th_pthread + tsd_offset;
		kret = pthread_kern->thread_set_tsd_base(th, th_tsd_base);
		if (kret == KERN_SUCCESS) {
			flags |= PTHREAD_START_TSD_BASE_SET;
		}
	}

#if defined(__i386__) || defined(__x86_64__)
	/*
	 * Set up i386 registers & function call.
	 */
	if (isLP64 == 0) {
		x86_thread_state32_t state = {
			.eip = (unsigned int)pthread_kern->proc_get_threadstart(p),
			.eax = (unsigned int)th_pthread,
			.ebx = (unsigned int)th_thport,
			.ecx = (unsigned int)user_func,
			.edx = (unsigned int)user_funcarg,
			.edi = (unsigned int)user_stack,
			.esi = (unsigned int)flags,
			/*
			 * set stack pointer
			 */
			.esp = (int)((vm_offset_t)(th_stack-C_32_STK_ALIGN))
		};

		error = pthread_kern->thread_set_wq_state32(th, (thread_state_t)&state);
		if (error != KERN_SUCCESS) {
			error = EINVAL;
			goto out;
		}
	} else {
		x86_thread_state64_t state64 = {
			.rip = (uint64_t)pthread_kern->proc_get_threadstart(p),
			.rdi = (uint64_t)th_pthread,
			.rsi = (uint64_t)(th_thport),
			.rdx = (uint64_t)user_func,
			.rcx = (uint64_t)user_funcarg,
			.r8 = (uint64_t)user_stack,
			.r9 = (uint64_t)flags,
			/*
			 * set stack pointer aligned to 16 byte boundary
			 */
			.rsp = (uint64_t)(th_stack - C_64_REDZONE_LEN)
		};

		error = pthread_kern->thread_set_wq_state64(th, (thread_state_t)&state64);
		if (error != KERN_SUCCESS) {
			error = EINVAL;
			goto out;
		}
		
	}
#elif defined(__arm__)
	arm_thread_state_t state = {
		.pc = (int)pthread_kern->proc_get_threadstart(p),
		.r[0] = (unsigned int)th_pthread,
		.r[1] = (unsigned int)th_thport,
		.r[2] = (unsigned int)user_func,
		.r[3] = (unsigned int)user_funcarg,
		.r[4] = (unsigned int)user_stack,
		.r[5] = (unsigned int)flags,

		/* Set r7 & lr to 0 for better back tracing */
		.r[7] = 0,
		.lr = 0,

		/*	
		 * set stack pointer
		 */
		.sp = (int)((vm_offset_t)(th_stack-C_32_STK_ALIGN))
	};

	(void) pthread_kern->thread_set_wq_state32(th, (thread_state_t)&state);

#else
#error bsdthread_create  not defined for this architecture
#endif

	if ((flags & PTHREAD_START_SETSCHED) != 0) {
		/* Set scheduling parameters if needed */
		thread_extended_policy_data_t    extinfo;
		thread_precedence_policy_data_t   precedinfo;

		importance = (flags & PTHREAD_START_IMPORTANCE_MASK);
		policy = (flags >> PTHREAD_START_POLICY_BITSHIFT) & PTHREAD_START_POLICY_MASK;

		if (policy == SCHED_OTHER) {
			extinfo.timeshare = 1;
		} else {
			extinfo.timeshare = 0;
		}

		thread_policy_set(th, THREAD_EXTENDED_POLICY, (thread_policy_t)&extinfo, THREAD_EXTENDED_POLICY_COUNT);

		precedinfo.importance = (importance - BASEPRI_DEFAULT);
		thread_policy_set(th, THREAD_PRECEDENCE_POLICY, (thread_policy_t)&precedinfo, THREAD_PRECEDENCE_POLICY_COUNT);
	} else if ((flags & PTHREAD_START_QOSCLASS) != 0) {
		/* Set thread QoS class if requested. */
		pthread_priority_t priority = (pthread_priority_t)(flags & PTHREAD_START_QOSCLASS_MASK);

		thread_qos_policy_data_t qos;
		qos.qos_tier = pthread_priority_get_thread_qos(priority);
		qos.tier_importance = (qos.qos_tier == QOS_CLASS_UNSPECIFIED) ? 0 :
				_pthread_priority_get_relpri(priority);

		pthread_kern->thread_policy_set_internal(th, THREAD_QOS_POLICY, (thread_policy_t)&qos, THREAD_QOS_POLICY_COUNT);
	}

	kret = pthread_kern->thread_resume(th);
	if (kret != KERN_SUCCESS) {
		error = EINVAL;
		goto out1;
	}
	thread_deallocate(th);	/* drop the creator reference */

	PTHREAD_TRACE(TRACE_pthread_thread_create|DBG_FUNC_END, error, th_pthread, 0, 0, 0);

	// cast required as mach_vm_offset_t is always 64 bits even on 32-bit platforms
	*retval = (user_addr_t)th_pthread;

	return(0);

out1:
	if (allocated != 0) {
		(void)mach_vm_deallocate(vmap, stackaddr, th_allocsize);
	}
out:
	(void)pthread_kern->mach_port_deallocate(pthread_kern->task_get_ipcspace(ctask), th_thport);
	(void)thread_terminate(th);
	(void)thread_deallocate(th);
	return(error);
}

/**
 * bsdthread_terminate system call.  Used by pthread_terminate
 */
int
_bsdthread_terminate(__unused struct proc *p,
		     user_addr_t stackaddr,
		     size_t size,
		     uint32_t kthport,
		     uint32_t sem,
		     __unused int32_t *retval)
{
	mach_vm_offset_t freeaddr;
	mach_vm_size_t freesize;
	kern_return_t kret;
	thread_t th = current_thread();

	freeaddr = (mach_vm_offset_t)stackaddr;
	freesize = size;

	PTHREAD_TRACE(TRACE_pthread_thread_terminate|DBG_FUNC_START, freeaddr, freesize, kthport, 0xff, 0);

	if ((freesize != (mach_vm_size_t)0) && (freeaddr != (mach_vm_offset_t)0)) {
		if (pthread_kern->thread_get_tag(th) & THREAD_TAG_MAINTHREAD){
			vm_map_t user_map = pthread_kern->current_map();
			freesize = vm_map_trunc_page_mask((vm_map_offset_t)freesize - 1, vm_map_page_mask(user_map));
			kret = mach_vm_behavior_set(user_map, freeaddr, freesize, VM_BEHAVIOR_REUSABLE);
			assert(kret == KERN_SUCCESS || kret == KERN_INVALID_ADDRESS);
			kret = kret ? kret : mach_vm_protect(user_map, freeaddr, freesize, FALSE, VM_PROT_NONE);
			assert(kret == KERN_SUCCESS || kret == KERN_INVALID_ADDRESS);
		} else {
			kret = mach_vm_deallocate(pthread_kern->current_map(), freeaddr, freesize);
			if (kret != KERN_SUCCESS) {
				PTHREAD_TRACE(TRACE_pthread_thread_terminate|DBG_FUNC_END, kret, 0, 0, 0, 0);
				return(EINVAL);
			}
		}
	}
	
	(void) thread_terminate(th);
	if (sem != MACH_PORT_NULL) {
		 kret = pthread_kern->semaphore_signal_internal_trap(sem);
		if (kret != KERN_SUCCESS) {
			PTHREAD_TRACE(TRACE_pthread_thread_terminate|DBG_FUNC_END, kret, 0, 0, 0, 0);
			return(EINVAL);
		}
	}
	
	if (kthport != MACH_PORT_NULL) {
		pthread_kern->mach_port_deallocate(pthread_kern->task_get_ipcspace(current_task()), kthport);
	}

	PTHREAD_TRACE(TRACE_pthread_thread_terminate|DBG_FUNC_END, 0, 0, 0, 0, 0);

	pthread_kern->thread_exception_return();
	panic("bsdthread_terminate: still running\n");

	PTHREAD_TRACE(TRACE_pthread_thread_terminate|DBG_FUNC_END, 0, 0xff, 0, 0, 0);

	return(0);
}

/**
 * bsdthread_register system call.  Performs per-process setup.  Responsible for
 * returning capabilitiy bits to userspace and receiving userspace function addresses.
 */
int
_bsdthread_register(struct proc *p,
		    user_addr_t threadstart,
		    user_addr_t wqthread,
		    int pthsize,
		    user_addr_t pthread_init_data,
		    user_addr_t pthread_init_data_size,
		    uint64_t dispatchqueue_offset,
		    int32_t *retval)
{
	/* We have to do this first so that it resets after fork */
	pthread_kern->proc_set_stack_addr_hint(p, (user_addr_t)stack_addr_hint(p, pthread_kern->current_map()));

	/* prevent multiple registrations */
	if (pthread_kern->proc_get_register(p) != 0) {
		return(EINVAL);
	}
	/* syscall randomizer test can pass bogus values */
	if (pthsize < 0 || pthsize > MAX_PTHREAD_SIZE) {
		return(EINVAL);
	}
	pthread_kern->proc_set_threadstart(p, threadstart);
	pthread_kern->proc_set_wqthread(p, wqthread);
	pthread_kern->proc_set_pthsize(p, pthsize);
	pthread_kern->proc_set_register(p);

	/* if we have pthread_init_data, then we use that and target_concptr (which is an offset) get data. */
	if (pthread_init_data != 0) {
		thread_qos_policy_data_t qos;

		struct _pthread_registration_data data = {};
		size_t pthread_init_sz = MIN(sizeof(struct _pthread_registration_data), (size_t)pthread_init_data_size);

		kern_return_t kr = copyin(pthread_init_data, &data, pthread_init_sz);
		if (kr != KERN_SUCCESS) {
			return EINVAL;
		}

		/* Incoming data from the data structure */
		pthread_kern->proc_set_dispatchqueue_offset(p, data.dispatch_queue_offset);
		if (data.version > offsetof(struct _pthread_registration_data, tsd_offset)
			&& data.tsd_offset < (uint32_t)pthsize) {
			pthread_kern->proc_set_pthread_tsd_offset(p, data.tsd_offset);
		}

		/* Outgoing data that userspace expects as a reply */
		data.version = sizeof(struct _pthread_registration_data);
		if (pthread_kern->qos_main_thread_active()) {
			mach_msg_type_number_t nqos = THREAD_QOS_POLICY_COUNT;
			boolean_t gd = FALSE;

			kr = pthread_kern->thread_policy_get(current_thread(), THREAD_QOS_POLICY, (thread_policy_t)&qos, &nqos, &gd);
			if (kr != KERN_SUCCESS || qos.qos_tier == THREAD_QOS_UNSPECIFIED) {
				/* Unspecified threads means the kernel wants us to impose legacy upon the thread. */
				qos.qos_tier = THREAD_QOS_LEGACY;
				qos.tier_importance = 0;

				kr = pthread_kern->thread_policy_set_internal(current_thread(), THREAD_QOS_POLICY, (thread_policy_t)&qos, THREAD_QOS_POLICY_COUNT);
			}

			if (kr == KERN_SUCCESS) {
				data.main_qos = thread_qos_get_pthread_priority(qos.qos_tier);
			} else {
				data.main_qos = _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0);
			}
		} else {
			data.main_qos = _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0);
		}

		kr = copyout(&data, pthread_init_data, pthread_init_sz);
		if (kr != KERN_SUCCESS) {
			return EINVAL;
		}
	} else {
		pthread_kern->proc_set_dispatchqueue_offset(p, dispatchqueue_offset);
	}

	/* return the supported feature set as the return value. */
	*retval = PTHREAD_FEATURE_SUPPORTED;

	return(0);
}

#pragma mark - QoS Manipulation

int
_bsdthread_ctl_set_qos(struct proc *p, user_addr_t __unused cmd, mach_port_name_t kport, user_addr_t tsd_priority_addr, user_addr_t arg3, int *retval)
{
 	kern_return_t kr;
	thread_t th;

	pthread_priority_t priority;

	/* Unused parameters must be zero. */
	if (arg3 != 0) {
		return EINVAL;
	}

	/* QoS is stored in a given slot in the pthread TSD. We need to copy that in and set our QoS based on it. */
	if (proc_is64bit(p)) {
		uint64_t v;
		kr = copyin(tsd_priority_addr, &v, sizeof(v));
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		priority = (int)(v & 0xffffffff);
	} else {
		uint32_t v;
		kr = copyin(tsd_priority_addr, &v, sizeof(v));
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		priority = v;
	}

	if ((th = port_name_to_thread(kport)) == THREAD_NULL) {
		return ESRCH;
	}

	/* <rdar://problem/16211829> Disable pthread_set_qos_class_np() on threads other than pthread_self */
	if (th != current_thread()) {
		thread_deallocate(th);
		return EPERM;
	}

	int rv = _bsdthread_ctl_set_self(p, 0, priority, 0, _PTHREAD_SET_SELF_QOS_FLAG, retval);

	/* Static param the thread, we just set QoS on it, so its stuck in QoS land now. */
	/* pthread_kern->thread_static_param(th, TRUE); */ // see <rdar://problem/16433744>, for details

	thread_deallocate(th);

	return rv;
}

static inline struct threadlist *
util_get_thread_threadlist_entry(thread_t th)
{
	struct uthread *uth = pthread_kern->get_bsdthread_info(th);
	if (uth) {
		struct threadlist *tl = pthread_kern->uthread_get_threadlist(uth);
		return tl;
	}
	return NULL;
}

int
_bsdthread_ctl_set_self(struct proc *p, user_addr_t __unused cmd, pthread_priority_t priority, mach_port_name_t voucher, _pthread_set_flags_t flags, int __unused *retval)
{
	thread_qos_policy_data_t qos;
	mach_msg_type_number_t nqos = THREAD_QOS_POLICY_COUNT;
	boolean_t gd = FALSE;
	bool was_manager_thread = false;
	thread_t th = current_thread();
	struct workqueue *wq = NULL;
	struct threadlist *tl = NULL;

	kern_return_t kr;
	int qos_rv = 0, voucher_rv = 0, fixedpri_rv = 0;

	if ((flags & _PTHREAD_SET_SELF_WQ_KEVENT_UNBIND) != 0) {
		tl = util_get_thread_threadlist_entry(th);
		if (tl) {
			wq = tl->th_workq;
		} else {
			goto qos;
		}

		workqueue_lock_spin(wq);
		if (tl->th_flags & TH_LIST_KEVENT_BOUND) {
			tl->th_flags &= ~TH_LIST_KEVENT_BOUND;
			unsigned int kevent_flags = KEVENT_FLAG_WORKQ;
			if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET) {
				kevent_flags |= KEVENT_FLAG_WORKQ_MANAGER;
			}

			workqueue_unlock(wq);
			kevent_qos_internal_unbind(p, class_index_get_thread_qos(tl->th_priority), th, kevent_flags);
		} else {
			workqueue_unlock(wq);
		}
	}

qos:
	if ((flags & _PTHREAD_SET_SELF_QOS_FLAG) != 0) {
		kr = pthread_kern->thread_policy_get(th, THREAD_QOS_POLICY, (thread_policy_t)&qos, &nqos, &gd);
		if (kr != KERN_SUCCESS) {
			qos_rv = EINVAL;
			goto voucher;
		}

		/* If we have main-thread QoS then we don't allow a thread to come out of QOS_CLASS_UNSPECIFIED. */
		if (pthread_kern->qos_main_thread_active() && qos.qos_tier == THREAD_QOS_UNSPECIFIED) {
			qos_rv = EPERM;
			goto voucher;
		}

		/* Get the work queue for tracing, also the threadlist for bucket manipluation. */
		if (!tl) {
			tl = util_get_thread_threadlist_entry(th);
			if (tl) wq = tl->th_workq;
		}

		PTHREAD_TRACE_WQ(TRACE_pthread_set_qos_self | DBG_FUNC_START, wq, qos.qos_tier, qos.tier_importance, 0, 0);

		qos.qos_tier = pthread_priority_get_thread_qos(priority);
		qos.tier_importance = (qos.qos_tier == QOS_CLASS_UNSPECIFIED) ? 0 : _pthread_priority_get_relpri(priority);

		if (qos.qos_tier == QOS_CLASS_UNSPECIFIED) {
			qos_rv = EINVAL;
			goto voucher;
		}

		/* If we're a workqueue, the threadlist item priority needs adjusting, along with the bucket we were running in. */
		if (tl) {
			workqueue_lock_spin(wq);
			bool now_under_constrained_limit = false;

			assert(!(tl->th_flags & TH_LIST_KEVENT_BOUND));

			kr = pthread_kern->thread_set_workq_qos(th, qos.qos_tier, qos.tier_importance);
			assert(kr == KERN_SUCCESS || kr == KERN_TERMINATED);

			/* Fix up counters. */
			uint8_t old_bucket = tl->th_priority;
			uint8_t new_bucket = pthread_priority_get_class_index(priority);
			if (old_bucket == WORKQUEUE_EVENT_MANAGER_BUCKET) {
				was_manager_thread = true;
			}

			uint32_t old_active = OSAddAtomic(-1, &wq->wq_thactive_count[old_bucket]);
			OSAddAtomic(1, &wq->wq_thactive_count[new_bucket]);

			wq->wq_thscheduled_count[old_bucket]--;
			wq->wq_thscheduled_count[new_bucket]++;

			bool old_overcommit = !(tl->th_flags & TH_LIST_CONSTRAINED);
			bool new_overcommit = priority & _PTHREAD_PRIORITY_OVERCOMMIT_FLAG;
			if (!old_overcommit && new_overcommit) {
				wq->wq_constrained_threads_scheduled--;
				tl->th_flags &= ~TH_LIST_CONSTRAINED;
				if (wq->wq_constrained_threads_scheduled == wq_max_constrained_threads - 1) {
					now_under_constrained_limit = true;
				}
			} else if (old_overcommit && !new_overcommit) {
				wq->wq_constrained_threads_scheduled++;
				tl->th_flags |= TH_LIST_CONSTRAINED;
			}

			tl->th_priority = new_bucket;

			/* If we were at the ceiling of threads for a given bucket, we have
			 * to reevaluate whether we should start more work.
			 */
			if (old_active == wq->wq_reqconc[old_bucket] || now_under_constrained_limit) {
				/* workqueue_run_nextreq will drop the workqueue lock in all exit paths. */
				(void)workqueue_run_nextreq(p, wq, THREAD_NULL, RUN_NEXTREQ_DEFAULT, 0, false);
			} else {
				workqueue_unlock(wq);
			}
		} else {
			kr = pthread_kern->thread_policy_set_internal(th, THREAD_QOS_POLICY, (thread_policy_t)&qos, THREAD_QOS_POLICY_COUNT);
			if (kr != KERN_SUCCESS) {
				qos_rv = EINVAL;
			}
		}

		PTHREAD_TRACE_WQ(TRACE_pthread_set_qos_self | DBG_FUNC_END, wq, qos.qos_tier, qos.tier_importance, 0, 0);
	}

voucher:
	if ((flags & _PTHREAD_SET_SELF_VOUCHER_FLAG) != 0) {
		kr = pthread_kern->thread_set_voucher_name(voucher);
		if (kr != KERN_SUCCESS) {
			voucher_rv = ENOENT;
			goto fixedpri;
		}
	}

fixedpri:
	if (qos_rv) goto done;
	if ((flags & _PTHREAD_SET_SELF_FIXEDPRIORITY_FLAG) != 0) {
		thread_extended_policy_data_t extpol = {.timeshare = 0};

		if (!tl) tl  = util_get_thread_threadlist_entry(th);
		if (tl) {
			/* Not allowed on workqueue threads */
			fixedpri_rv = ENOTSUP;
			goto done;
		}

		kr = pthread_kern->thread_policy_set_internal(th, THREAD_EXTENDED_POLICY, (thread_policy_t)&extpol, THREAD_EXTENDED_POLICY_COUNT);
		if (kr != KERN_SUCCESS) {
			fixedpri_rv = EINVAL;
			goto done;
		}
	} else if ((flags & _PTHREAD_SET_SELF_TIMESHARE_FLAG) != 0) {
		thread_extended_policy_data_t extpol = {.timeshare = 1};

		if (!tl) tl = util_get_thread_threadlist_entry(th);
		if (tl) {
			/* Not allowed on workqueue threads */
			fixedpri_rv = ENOTSUP;
			goto done;
		}

		kr = pthread_kern->thread_policy_set_internal(th, THREAD_EXTENDED_POLICY, (thread_policy_t)&extpol, THREAD_EXTENDED_POLICY_COUNT);
		if (kr != KERN_SUCCESS) {
			fixedpri_rv = EINVAL;
			goto done;
		}
	}

done:
	if (qos_rv && voucher_rv) {
		/* Both failed, give that a unique error. */
		return EBADMSG;
	}

	if (qos_rv) {
		return qos_rv;
	}

	if (voucher_rv) {
		return voucher_rv;
	}

	if (fixedpri_rv) {
		return fixedpri_rv;
	}
	
	return 0;
}

int
_bsdthread_ctl_qos_override_start(struct proc __unused *p, user_addr_t __unused cmd, mach_port_name_t kport, pthread_priority_t priority, user_addr_t resource, int __unused *retval)
{
	thread_t th;
	int rv = 0;

	if ((th = port_name_to_thread(kport)) == THREAD_NULL) {
		return ESRCH;
	}

	int override_qos = pthread_priority_get_thread_qos(priority);

	struct threadlist *tl = util_get_thread_threadlist_entry(th);
	if (tl) {
		PTHREAD_TRACE_WQ(TRACE_wq_override_start | DBG_FUNC_NONE, tl->th_workq, thread_tid(th), 1, priority, 0);
	}

	/* The only failure case here is if we pass a tid and have it lookup the thread, we pass the uthread, so this all always succeeds. */
	pthread_kern->proc_usynch_thread_qos_add_override_for_resource_check_owner(th, override_qos, TRUE,
			resource, THREAD_QOS_OVERRIDE_TYPE_PTHREAD_EXPLICIT_OVERRIDE, USER_ADDR_NULL, MACH_PORT_NULL);
	thread_deallocate(th);
	return rv;
}

int
_bsdthread_ctl_qos_override_end(struct proc __unused *p, user_addr_t __unused cmd, mach_port_name_t kport, user_addr_t resource, user_addr_t arg3, int __unused *retval)
{
	thread_t th;
	int rv = 0;

	if (arg3 != 0) {
		return EINVAL;
	}

	if ((th = port_name_to_thread(kport)) == THREAD_NULL) {
		return ESRCH;
	}

	struct uthread *uth = pthread_kern->get_bsdthread_info(th);

	struct threadlist *tl = util_get_thread_threadlist_entry(th);
	if (tl) {
		PTHREAD_TRACE_WQ(TRACE_wq_override_end | DBG_FUNC_NONE, tl->th_workq, thread_tid(th), 0, 0, 0);
	}

	pthread_kern->proc_usynch_thread_qos_remove_override_for_resource(current_task(), uth, 0, resource, THREAD_QOS_OVERRIDE_TYPE_PTHREAD_EXPLICIT_OVERRIDE);

	thread_deallocate(th);
	return rv;
}

static int
_bsdthread_ctl_qos_dispatch_asynchronous_override_add_internal(mach_port_name_t kport, pthread_priority_t priority, user_addr_t resource, user_addr_t ulock_addr)
{
	thread_t th;
	int rv = 0;

	if ((th = port_name_to_thread(kport)) == THREAD_NULL) {
		return ESRCH;
	}

	int override_qos = pthread_priority_get_thread_qos(priority);

	struct threadlist *tl = util_get_thread_threadlist_entry(th);
	if (!tl) {
		thread_deallocate(th);
		return EPERM;
	}

	PTHREAD_TRACE_WQ(TRACE_wq_override_dispatch | DBG_FUNC_NONE, tl->th_workq, thread_tid(th), 1, priority, 0);

	rv = pthread_kern->proc_usynch_thread_qos_add_override_for_resource_check_owner(th, override_qos, TRUE,
			resource, THREAD_QOS_OVERRIDE_TYPE_DISPATCH_ASYNCHRONOUS_OVERRIDE, ulock_addr, kport);

	thread_deallocate(th);
	return rv;
}

int _bsdthread_ctl_qos_dispatch_asynchronous_override_add(struct proc __unused *p, user_addr_t __unused cmd,
		mach_port_name_t kport, pthread_priority_t priority, user_addr_t resource, int __unused *retval)
{
	return _bsdthread_ctl_qos_dispatch_asynchronous_override_add_internal(kport, priority, resource, USER_ADDR_NULL);
}

int
_bsdthread_ctl_qos_override_dispatch(struct proc *p __unused, user_addr_t cmd __unused, mach_port_name_t kport, pthread_priority_t priority, user_addr_t ulock_addr, int __unused *retval)
{
	return _bsdthread_ctl_qos_dispatch_asynchronous_override_add_internal(kport, priority, USER_ADDR_NULL, ulock_addr);
}

int
_bsdthread_ctl_qos_override_reset(struct proc *p, user_addr_t cmd, user_addr_t arg1, user_addr_t arg2, user_addr_t arg3, int *retval)
{
	if (arg1 != 0 || arg2 != 0 || arg3 != 0) {
		return EINVAL;
	}

	return _bsdthread_ctl_qos_dispatch_asynchronous_override_reset(p, cmd, 1 /* reset_all */, 0, 0, retval);
}

int
_bsdthread_ctl_qos_dispatch_asynchronous_override_reset(struct proc __unused *p, user_addr_t __unused cmd, int reset_all, user_addr_t resource, user_addr_t arg3, int __unused *retval)
{
	if ((reset_all && (resource != 0)) || arg3 != 0) {
		return EINVAL;
	}

	thread_t th = current_thread();
	struct uthread *uth = pthread_kern->get_bsdthread_info(th);
	struct threadlist *tl = pthread_kern->uthread_get_threadlist(uth);

	if (!tl) {
		return EPERM;
	}

	PTHREAD_TRACE_WQ(TRACE_wq_override_reset | DBG_FUNC_NONE, tl->th_workq, 0, 0, 0, 0);

	resource = reset_all ? THREAD_QOS_OVERRIDE_RESOURCE_WILDCARD : resource;
	pthread_kern->proc_usynch_thread_qos_reset_override_for_resource(current_task(), uth, 0, resource, THREAD_QOS_OVERRIDE_TYPE_DISPATCH_ASYNCHRONOUS_OVERRIDE);

	return 0;
}

int
_bsdthread_ctl(struct proc *p, user_addr_t cmd, user_addr_t arg1, user_addr_t arg2, user_addr_t arg3, int *retval)
{
	switch (cmd) {
	case BSDTHREAD_CTL_SET_QOS:
		return _bsdthread_ctl_set_qos(p, cmd, (mach_port_name_t)arg1, arg2, arg3, retval);
	case BSDTHREAD_CTL_QOS_OVERRIDE_START:
		return _bsdthread_ctl_qos_override_start(p, cmd, (mach_port_name_t)arg1, (pthread_priority_t)arg2, arg3, retval);
	case BSDTHREAD_CTL_QOS_OVERRIDE_END:
		return _bsdthread_ctl_qos_override_end(p, cmd, (mach_port_name_t)arg1, arg2, arg3, retval);
	case BSDTHREAD_CTL_QOS_OVERRIDE_RESET:
		return _bsdthread_ctl_qos_override_reset(p, cmd, arg1, arg2, arg3, retval);
	case BSDTHREAD_CTL_QOS_OVERRIDE_DISPATCH:
		return _bsdthread_ctl_qos_override_dispatch(p, cmd, (mach_port_name_t)arg1, (pthread_priority_t)arg2, arg3, retval);
	case BSDTHREAD_CTL_QOS_DISPATCH_ASYNCHRONOUS_OVERRIDE_ADD:
		return _bsdthread_ctl_qos_dispatch_asynchronous_override_add(p, cmd, (mach_port_name_t)arg1, (pthread_priority_t)arg2, arg3, retval);
	case BSDTHREAD_CTL_QOS_DISPATCH_ASYNCHRONOUS_OVERRIDE_RESET:
		return _bsdthread_ctl_qos_dispatch_asynchronous_override_reset(p, cmd, (int)arg1, arg2, arg3, retval);
	case BSDTHREAD_CTL_SET_SELF:
		return _bsdthread_ctl_set_self(p, cmd, (pthread_priority_t)arg1, (mach_port_name_t)arg2, (_pthread_set_flags_t)arg3, retval);
	default:
		return EINVAL;
	}
}

#pragma mark - Workqueue Implementation
#pragma mark workqueue lock

static boolean_t workqueue_lock_spin_is_acquired_kdp(struct workqueue *wq) {
  return kdp_lck_spin_is_acquired(&wq->wq_lock);
}

static void
workqueue_lock_spin(struct workqueue *wq)
{
	boolean_t interrupt_state = ml_set_interrupts_enabled(FALSE);
	lck_spin_lock(&wq->wq_lock);
	wq->wq_interrupt_state = interrupt_state;
}

static void
workqueue_unlock(struct workqueue *wq)
{
	boolean_t interrupt_state = wq->wq_interrupt_state;
	lck_spin_unlock(&wq->wq_lock);
	ml_set_interrupts_enabled(interrupt_state);
}

#pragma mark workqueue add timer

/**
 * Sets up the timer which will call out to workqueue_add_timer
 */
static void
workqueue_interval_timer_start(struct workqueue *wq)
{
	uint64_t deadline;

	/* n.b. wq_timer_interval is reset to 0 in workqueue_add_timer if the
	 ATIMER_RUNNING flag is not present.  The net effect here is that if a
	 sequence of threads is required, we'll double the time before we give out
	 the next one. */
	if (wq->wq_timer_interval == 0) {
		wq->wq_timer_interval = wq_stalled_window_usecs;

	} else {
		wq->wq_timer_interval = wq->wq_timer_interval * 2;

		if (wq->wq_timer_interval > wq_max_timer_interval_usecs) {
			wq->wq_timer_interval = wq_max_timer_interval_usecs;
		}
	}
	clock_interval_to_deadline(wq->wq_timer_interval, 1000, &deadline);

	PTHREAD_TRACE_WQ(TRACE_wq_start_add_timer, wq, wq->wq_reqcount, wq->wq_flags, wq->wq_timer_interval, 0);

	boolean_t ret = thread_call_enter1_delayed(wq->wq_atimer_delayed_call, wq->wq_atimer_delayed_call, deadline);
	if (ret) {
		panic("delayed_call was already enqueued");
	}
}

/**
 * Immediately trigger the workqueue_add_timer
 */
static void
workqueue_interval_timer_trigger(struct workqueue *wq)
{
	PTHREAD_TRACE_WQ(TRACE_wq_start_add_timer, wq, wq->wq_reqcount, wq->wq_flags, 0, 0);

	boolean_t ret = thread_call_enter1(wq->wq_atimer_immediate_call, wq->wq_atimer_immediate_call);
	if (ret) {
		panic("immediate_call was already enqueued");
	}
}

/**
 * returns whether lastblocked_tsp is within wq_stalled_window_usecs of cur_ts
 */
static boolean_t
wq_thread_is_busy(uint64_t cur_ts, uint64_t *lastblocked_tsp)
{
	clock_sec_t	secs;
	clock_usec_t	usecs;
	uint64_t lastblocked_ts;
	uint64_t elapsed;

	/*
	 * the timestamp is updated atomically w/o holding the workqueue lock
	 * so we need to do an atomic read of the 64 bits so that we don't see
	 * a mismatched pair of 32 bit reads... we accomplish this in an architecturally
	 * independent fashion by using OSCompareAndSwap64 to write back the
	 * value we grabbed... if it succeeds, then we have a good timestamp to
	 * evaluate... if it fails, we straddled grabbing the timestamp while it
	 * was being updated... treat a failed update as a busy thread since
	 * it implies we are about to see a really fresh timestamp anyway
	 */
	lastblocked_ts = *lastblocked_tsp;

	if ( !OSCompareAndSwap64((UInt64)lastblocked_ts, (UInt64)lastblocked_ts, lastblocked_tsp))
		return (TRUE);

	if (lastblocked_ts >= cur_ts) {
		/*
		 * because the update of the timestamp when a thread blocks isn't
		 * serialized against us looking at it (i.e. we don't hold the workq lock)
		 * it's possible to have a timestamp that matches the current time or
		 * that even looks to be in the future relative to when we grabbed the current
		 * time... just treat this as a busy thread since it must have just blocked.
		 */
		return (TRUE);
	}
	elapsed = cur_ts - lastblocked_ts;

	pthread_kern->absolutetime_to_microtime(elapsed, &secs, &usecs);

	if (secs == 0 && usecs < wq_stalled_window_usecs)
		return (TRUE);
	return (FALSE);
}

static inline bool
WQ_TIMER_DELAYED_NEEDED(struct workqueue *wq)
{
	int oldflags;
retry:
	oldflags = wq->wq_flags;
	if ( !(oldflags & (WQ_EXITING | WQ_ATIMER_DELAYED_RUNNING))) {
		if (OSCompareAndSwap(oldflags, oldflags | WQ_ATIMER_DELAYED_RUNNING, (UInt32 *)&wq->wq_flags)) {
			return true;
		} else {
			goto retry;
		}
	}
	return false;
}

static inline bool
WQ_TIMER_IMMEDIATE_NEEDED(struct workqueue *wq)
{
	int oldflags;
retry:
	oldflags = wq->wq_flags;
	if ( !(oldflags & (WQ_EXITING | WQ_ATIMER_IMMEDIATE_RUNNING))) {
		if (OSCompareAndSwap(oldflags, oldflags | WQ_ATIMER_IMMEDIATE_RUNNING, (UInt32 *)&wq->wq_flags)) {
			return true;
		} else {
			goto retry;
		}
	}
	return false;
}

/**
 * handler function for the timer
 */
static void
workqueue_add_timer(struct workqueue *wq, thread_call_t thread_call_self)
{
	proc_t		p;
	boolean_t	start_timer = FALSE;
	boolean_t	retval;

	PTHREAD_TRACE_WQ(TRACE_wq_add_timer | DBG_FUNC_START, wq, wq->wq_flags, wq->wq_nthreads, wq->wq_thidlecount, 0);

	p = wq->wq_proc;

	workqueue_lock_spin(wq);

	/*
	 * There's two tricky issues here.
	 *
	 * First issue: we start the thread_call's that invoke this routine without
	 * the workqueue lock held.  The scheduler callback needs to trigger
	 * reevaluation of the number of running threads but shouldn't take that
	 * lock, so we can't use it to synchronize state around the thread_call.
	 * As a result, it might re-enter the thread_call while this routine is
	 * already running.  This could cause it to fire a second time and we'll
	 * have two add_timers running at once.  Obviously, we don't want that to
	 * keep stacking, so we need to keep it at two timers.
	 *
	 * Solution: use wq_flags (accessed via atomic CAS) to synchronize the
	 * enqueue of the thread_call itself.  When a thread needs to trigger the
	 * add_timer, it checks for ATIMER_DELAYED_RUNNING and, when not set, sets
	 * the flag then does a thread_call_enter.  We'll then remove that flag
	 * only once we've got the lock and it's safe for the thread_call to be
	 * entered again.
	 *
	 * Second issue: we need to make sure that the two timers don't execute this
	 * routine concurrently.  We can't use the workqueue lock for this because
	 * we'll need to drop it during our execution.
	 *
	 * Solution: use WQL_ATIMER_BUSY as a condition variable to indicate that
	 * we are currently executing the routine and the next thread should wait.
	 *
	 * After all that, we arrive at the following four possible states:
	 * !WQ_ATIMER_DELAYED_RUNNING && !WQL_ATIMER_BUSY       no pending timer, no active timer
	 * !WQ_ATIMER_DELAYED_RUNNING &&  WQL_ATIMER_BUSY       no pending timer,  1 active timer
	 *  WQ_ATIMER_DELAYED_RUNNING && !WQL_ATIMER_BUSY        1 pending timer, no active timer
	 *  WQ_ATIMER_DELAYED_RUNNING &&  WQL_ATIMER_BUSY        1 pending timer,  1 active timer
	 *
	 * Further complication sometimes we need to trigger this function to run
	 * without delay.  Because we aren't under a lock between setting
	 * WQ_ATIMER_DELAYED_RUNNING and calling thread_call_enter, we can't simply
	 * re-enter the thread call: if thread_call_enter() returned false, we
	 * wouldn't be able to distinguish the case where the thread_call had
	 * already fired from the case where it hadn't been entered yet from the
	 * other thread.  So, we use a separate thread_call for immediate
	 * invocations, and a separate RUNNING flag, WQ_ATIMER_IMMEDIATE_RUNNING.
	 */

	while (wq->wq_lflags & WQL_ATIMER_BUSY) {
		wq->wq_lflags |= WQL_ATIMER_WAITING;

		assert_wait((caddr_t)wq, (THREAD_UNINT));
		workqueue_unlock(wq);

		thread_block(THREAD_CONTINUE_NULL);

		workqueue_lock_spin(wq);
	}
	wq->wq_lflags |= WQL_ATIMER_BUSY;

	/*
	 * Decide which timer we are and remove the RUNNING flag.
	 */
	if (thread_call_self == wq->wq_atimer_delayed_call) {
		if ((wq->wq_flags & WQ_ATIMER_DELAYED_RUNNING) == 0) {
			panic("workqueue_add_timer is the delayed timer but the delayed running flag isn't set");
		}
		WQ_UNSETFLAG(wq, WQ_ATIMER_DELAYED_RUNNING);
	} else if (thread_call_self == wq->wq_atimer_immediate_call) {
		if ((wq->wq_flags & WQ_ATIMER_IMMEDIATE_RUNNING) == 0) {
			panic("workqueue_add_timer is the immediate timer but the immediate running flag isn't set");
		}
		WQ_UNSETFLAG(wq, WQ_ATIMER_IMMEDIATE_RUNNING);
	} else {
		panic("workqueue_add_timer can't figure out which timer it is");
	}

again:
	retval = TRUE;
	if ( !(wq->wq_flags & WQ_EXITING)) {
		boolean_t add_thread = FALSE;
		/*
		 * check to see if the stall frequency was beyond our tolerance
		 * or we have work on the queue, but haven't scheduled any
		 * new work within our acceptable time interval because
		 * there were no idle threads left to schedule
		 */
		if (wq->wq_reqcount) {
			uint32_t	priclass = 0;
			uint32_t	thactive_count = 0;
			uint64_t	curtime = mach_absolute_time();
			uint64_t	busycount = 0;

			if (wq->wq_requests[WORKQUEUE_EVENT_MANAGER_BUCKET] &&
				wq->wq_thscheduled_count[WORKQUEUE_EVENT_MANAGER_BUCKET] == 0){
				priclass = WORKQUEUE_EVENT_MANAGER_BUCKET;
			} else {
				for (priclass = 0; priclass < WORKQUEUE_NUM_BUCKETS; priclass++) {
					if (wq->wq_requests[priclass])
						break;
				}
			}

			if (priclass < WORKQUEUE_EVENT_MANAGER_BUCKET){
				/*
				 * Compute a metric for many how many threads are active.  We
				 * find the highest priority request outstanding and then add up
				 * the number of active threads in that and all higher-priority
				 * buckets.  We'll also add any "busy" threads which are not
				 * active but blocked recently enough that we can't be sure
				 * they've gone idle yet.  We'll then compare this metric to our
				 * max concurrency to decide whether to add a new thread.
				 */
				for (uint32_t i = 0; i <= priclass; i++) {
					thactive_count += wq->wq_thactive_count[i];

					if (wq->wq_thscheduled_count[i] < wq->wq_thactive_count[i]) {
						if (wq_thread_is_busy(curtime, &wq->wq_lastblocked_ts[i]))
							busycount++;
					}
				}
			}

			if (thactive_count + busycount < wq->wq_max_concurrency ||
				priclass == WORKQUEUE_EVENT_MANAGER_BUCKET) {

				if (wq->wq_thidlecount == 0) {
					/*
					 * if we have no idle threads, try to add one
					 */
					retval = workqueue_addnewthread(wq, priclass == WORKQUEUE_EVENT_MANAGER_BUCKET);
				}
				add_thread = TRUE;
			}

			if (wq->wq_reqcount) {
				/*
				 * as long as we have threads to schedule, and we successfully
				 * scheduled new work, keep trying
				 */
				while (wq->wq_thidlecount && !(wq->wq_flags & WQ_EXITING)) {
					/*
					 * workqueue_run_nextreq is responsible for
					 * dropping the workqueue lock in all cases
					 */
					retval = (workqueue_run_nextreq(p, wq, THREAD_NULL, RUN_NEXTREQ_ADD_TIMER, 0, false) != THREAD_NULL);
					workqueue_lock_spin(wq);

					if (retval == FALSE)
						break;
				}
				if ( !(wq->wq_flags & WQ_EXITING) && wq->wq_reqcount) {

					if (wq->wq_thidlecount == 0 && retval == TRUE && add_thread == TRUE)
						goto again;

					if (wq->wq_thidlecount == 0 || busycount) {
						start_timer = WQ_TIMER_DELAYED_NEEDED(wq);
					}

					PTHREAD_TRACE_WQ(TRACE_wq_add_timer | DBG_FUNC_NONE, wq, wq->wq_reqcount, wq->wq_thidlecount, busycount, 0);
				}
			}
		}
	}

	/*
	 * If we called WQ_TIMER_NEEDED above, then this flag will be set if that
	 * call marked the timer running.  If so, we let the timer interval grow.
	 * Otherwise, we reset it back to 0.
	 */
	if (!(wq->wq_flags & WQ_ATIMER_DELAYED_RUNNING)) {
		wq->wq_timer_interval = 0;
	}

	wq->wq_lflags &= ~WQL_ATIMER_BUSY;

	if ((wq->wq_flags & WQ_EXITING) || (wq->wq_lflags & WQL_ATIMER_WAITING)) {
		/*
		 * wakeup the thread hung up in _workqueue_mark_exiting or workqueue_add_timer waiting for this timer
		 * to finish getting out of the way
		 */
		wq->wq_lflags &= ~WQL_ATIMER_WAITING;
		wakeup(wq);
	}

	PTHREAD_TRACE_WQ(TRACE_wq_add_timer | DBG_FUNC_END, wq, start_timer, wq->wq_nthreads, wq->wq_thidlecount, 0);

	workqueue_unlock(wq);

	if (start_timer == TRUE)
		workqueue_interval_timer_start(wq);
}

#pragma mark thread state tracking

// called by spinlock code when trying to yield to lock owner
void
_workqueue_thread_yielded(void)
{
	struct workqueue *wq;
	proc_t p;

	p = current_proc();

	if ((wq = pthread_kern->proc_get_wqptr(p)) == NULL || wq->wq_reqcount == 0)
		return;

	workqueue_lock_spin(wq);

	if (wq->wq_reqcount) {
		uint64_t	curtime;
		uint64_t	elapsed;
		clock_sec_t	secs;
		clock_usec_t	usecs;

		if (wq->wq_thread_yielded_count++ == 0)
			wq->wq_thread_yielded_timestamp = mach_absolute_time();

		if (wq->wq_thread_yielded_count < wq_yielded_threshold) {
			workqueue_unlock(wq);
			return;
		}

		PTHREAD_TRACE_WQ(TRACE_wq_thread_yielded | DBG_FUNC_START, wq, wq->wq_thread_yielded_count, wq->wq_reqcount, 0, 0);

		wq->wq_thread_yielded_count = 0;

		curtime = mach_absolute_time();
		elapsed = curtime - wq->wq_thread_yielded_timestamp;
		pthread_kern->absolutetime_to_microtime(elapsed, &secs, &usecs);

		if (secs == 0 && usecs < wq_yielded_window_usecs) {

			if (wq->wq_thidlecount == 0) {
				workqueue_addnewthread(wq, TRUE);
				/*
				 * 'workqueue_addnewthread' drops the workqueue lock
				 * when creating the new thread and then retakes it before
				 * returning... this window allows other threads to process
				 * requests, so we need to recheck for available work
				 * if none found, we just return...  the newly created thread
				 * will eventually get used (if it hasn't already)...
				 */
				if (wq->wq_reqcount == 0) {
					workqueue_unlock(wq);
					return;
				}
			}
			if (wq->wq_thidlecount) {
				(void)workqueue_run_nextreq(p, wq, THREAD_NULL, RUN_NEXTREQ_UNCONSTRAINED, 0, false);
				/*
				 * workqueue_run_nextreq is responsible for
				 * dropping the workqueue lock in all cases
				 */
				PTHREAD_TRACE_WQ(TRACE_wq_thread_yielded | DBG_FUNC_END, wq, wq->wq_thread_yielded_count, wq->wq_reqcount, 1, 0);

				return;
			}
		}
		PTHREAD_TRACE_WQ(TRACE_wq_thread_yielded | DBG_FUNC_END, wq, wq->wq_thread_yielded_count, wq->wq_reqcount, 2, 0);
	}
	workqueue_unlock(wq);
}

static void
workqueue_callback(int type, thread_t thread)
{
	struct uthread    *uth;
	struct threadlist *tl;
	struct workqueue  *wq;

	uth = pthread_kern->get_bsdthread_info(thread);
	tl = pthread_kern->uthread_get_threadlist(uth);
	wq = tl->th_workq;

	switch (type) {
	case SCHED_CALL_BLOCK: {
		uint32_t	old_activecount;
		boolean_t	start_timer = FALSE;

		old_activecount = OSAddAtomic(-1, &wq->wq_thactive_count[tl->th_priority]);

		/*
		 * If we blocked and were at the requested concurrency previously, we may
		 * need to spin up a new thread.  Of course, if it's the event manager
		 * then that's moot, so ignore that case.
		 */
		if (old_activecount == wq->wq_reqconc[tl->th_priority] &&
			tl->th_priority != WORKQUEUE_EVENT_MANAGER_BUCKET) {
			uint64_t	curtime;
			UInt64		*lastblocked_ptr;

			/*
			 * the number of active threads at this priority
			 * has fallen below the maximum number of concurrent
			 * threads that we're allowed to run
			 */
			lastblocked_ptr = (UInt64 *)&wq->wq_lastblocked_ts[tl->th_priority];
			curtime = mach_absolute_time();

			/*
			 * if we collide with another thread trying to update the last_blocked (really unlikely
			 * since another thread would have to get scheduled and then block after we start down
			 * this path), it's not a problem.  Either timestamp is adequate, so no need to retry
			 */

			OSCompareAndSwap64(*lastblocked_ptr, (UInt64)curtime, lastblocked_ptr);

			if (wq->wq_reqcount) {
				/*
				 * We have work to do so start up the timer if it's not
				 * running; it'll sort out whether we need to start another
				 * thread
				 */
				start_timer = WQ_TIMER_DELAYED_NEEDED(wq);
			}

			if (start_timer == TRUE) {
				workqueue_interval_timer_start(wq);
			}
		}
		PTHREAD_TRACE1_WQ(TRACE_wq_thread_block | DBG_FUNC_START, wq, old_activecount, tl->th_priority, start_timer, thread_tid(thread));
		break;
	}
	case SCHED_CALL_UNBLOCK:
		/*
		 * we cannot take the workqueue_lock here...
		 * an UNBLOCK can occur from a timer event which
		 * is run from an interrupt context... if the workqueue_lock
		 * is already held by this processor, we'll deadlock...
		 * the thread lock for the thread being UNBLOCKED
		 * is also held
		 */
		OSAddAtomic(1, &wq->wq_thactive_count[tl->th_priority]);

		PTHREAD_TRACE1_WQ(TRACE_wq_thread_block | DBG_FUNC_END, wq, wq->wq_threads_scheduled, tl->th_priority, 0, thread_tid(thread));

		break;
	}
}

sched_call_t
_workqueue_get_sched_callback(void)
{
	return workqueue_callback;
}

#pragma mark thread addition/removal

static mach_vm_size_t
_workqueue_allocsize(struct workqueue *wq)
{
	proc_t p = wq->wq_proc;
	mach_vm_size_t guardsize = vm_map_page_size(wq->wq_map);
	mach_vm_size_t pthread_size =
		vm_map_round_page_mask(pthread_kern->proc_get_pthsize(p) + PTHREAD_T_OFFSET, vm_map_page_mask(wq->wq_map));
	return guardsize + PTH_DEFAULT_STACKSIZE + pthread_size;
}

/**
 * pop goes the thread
 *
 * If fromexit is set, the call is from workqueue_exit(,
 * so some cleanups are to be avoided.
 */
static void
workqueue_removethread(struct threadlist *tl, bool fromexit, bool first_use)
{
	struct uthread * uth;
	struct workqueue * wq = tl->th_workq;

	if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET){
		TAILQ_REMOVE(&wq->wq_thidlemgrlist, tl, th_entry);
	} else {
		TAILQ_REMOVE(&wq->wq_thidlelist, tl, th_entry);
	}

	if (fromexit == 0) {
		assert(wq->wq_nthreads && wq->wq_thidlecount);
		wq->wq_nthreads--;
		wq->wq_thidlecount--;
	}

	/*
	 * Clear the threadlist pointer in uthread so
	 * blocked thread on wakeup for termination will
	 * not access the thread list as it is going to be
	 * freed.
	 */
	pthread_kern->thread_sched_call(tl->th_thread, NULL);

	uth = pthread_kern->get_bsdthread_info(tl->th_thread);
	if (uth != (struct uthread *)0) {
		pthread_kern->uthread_set_threadlist(uth, NULL);
	}
	if (fromexit == 0) {
		/* during exit the lock is not held */
		workqueue_unlock(wq);
	}

	if ( (tl->th_flags & TH_LIST_NEW) || first_use ) {
		/*
		 * thread was created, but never used...
		 * need to clean up the stack and port ourselves
		 * since we're not going to spin up through the
		 * normal exit path triggered from Libc
		 */
		if (fromexit == 0) {
			/* vm map is already deallocated when this is called from exit */
			(void)mach_vm_deallocate(wq->wq_map, tl->th_stackaddr, _workqueue_allocsize(wq));
		}
		(void)pthread_kern->mach_port_deallocate(pthread_kern->task_get_ipcspace(wq->wq_task), tl->th_thport);

	} else {

		PTHREAD_TRACE1_WQ(TRACE_wq_thread_park | DBG_FUNC_END, wq, (uintptr_t)thread_tid(current_thread()), wq->wq_nthreads, 0xdead, thread_tid(tl->th_thread));
	}
	/*
	 * drop our ref on the thread
	 */
	thread_deallocate(tl->th_thread);

	kfree(tl, sizeof(struct threadlist));
}


/**
 * Try to add a new workqueue thread.
 *
 * - called with workq lock held
 * - dropped and retaken around thread creation
 * - return with workq lock held
 */
static boolean_t
workqueue_addnewthread(struct workqueue *wq, boolean_t ignore_constrained_thread_limit)
{
	struct threadlist *tl;
	struct uthread	*uth;
	kern_return_t	kret;
	thread_t	th;
	proc_t		p;
	void 	 	*sright;
	mach_vm_offset_t stackaddr;

	if ((wq->wq_flags & WQ_EXITING) == WQ_EXITING) {
		PTHREAD_TRACE_WQ(TRACE_wq_thread_add_during_exit | DBG_FUNC_NONE, wq, 0, 0, 0, 0);
		return (FALSE);
	}

	if (wq->wq_nthreads >= wq_max_threads) {
		PTHREAD_TRACE_WQ(TRACE_wq_thread_limit_exceeded | DBG_FUNC_NONE, wq, wq->wq_nthreads, wq_max_threads, 0, 0);
		return (FALSE);
	}

	if (ignore_constrained_thread_limit == FALSE &&
		wq->wq_constrained_threads_scheduled >= wq_max_constrained_threads) {
		/*
		 * If we're not creating this thread to service an overcommit or
		 * event manager request, then we check to see if we are over our
		 * constrained thread limit, in which case we error out.
		 */
		PTHREAD_TRACE_WQ(TRACE_wq_thread_constrained_maxed | DBG_FUNC_NONE, wq, wq->wq_constrained_threads_scheduled,
				wq_max_constrained_threads, 0, 0);
		return (FALSE);
	}

	wq->wq_nthreads++;

	p = wq->wq_proc;
	workqueue_unlock(wq);

	tl = kalloc(sizeof(struct threadlist));
	bzero(tl, sizeof(struct threadlist));

	kret = pthread_kern->thread_create_workq_waiting(wq->wq_task, wq_unpark_continue, tl, &th);
 	if (kret != KERN_SUCCESS) {
		PTHREAD_TRACE_WQ(TRACE_wq_thread_create_failed | DBG_FUNC_NONE, wq, kret, 0, 0, 0);
		kfree(tl, sizeof(struct threadlist));
		goto failed;
	}

	stackaddr = pthread_kern->proc_get_stack_addr_hint(p);

	mach_vm_size_t guardsize = vm_map_page_size(wq->wq_map);
	mach_vm_size_t pthread_size =
		vm_map_round_page_mask(pthread_kern->proc_get_pthsize(p) + PTHREAD_T_OFFSET, vm_map_page_mask(wq->wq_map));
	mach_vm_size_t th_allocsize = guardsize + PTH_DEFAULT_STACKSIZE + pthread_size;

	kret = mach_vm_map(wq->wq_map, &stackaddr,
			th_allocsize, page_size-1,
			VM_MAKE_TAG(VM_MEMORY_STACK)| VM_FLAGS_ANYWHERE, NULL,
			0, FALSE, VM_PROT_DEFAULT, VM_PROT_ALL,
			VM_INHERIT_DEFAULT);

	if (kret != KERN_SUCCESS) {
		PTHREAD_TRACE_WQ(TRACE_wq_thread_create_failed | DBG_FUNC_NONE, wq, kret, 1, 0, 0);

		kret = mach_vm_allocate(wq->wq_map,
				&stackaddr, th_allocsize,
				VM_MAKE_TAG(VM_MEMORY_STACK) | VM_FLAGS_ANYWHERE);
	}
	if (kret == KERN_SUCCESS) {
		/*
		 * The guard page is at the lowest address
		 * The stack base is the highest address
		 */
		kret = mach_vm_protect(wq->wq_map, stackaddr, guardsize, FALSE, VM_PROT_NONE);

		if (kret != KERN_SUCCESS) {
			(void) mach_vm_deallocate(wq->wq_map, stackaddr, th_allocsize);
			PTHREAD_TRACE_WQ(TRACE_wq_thread_create_failed | DBG_FUNC_NONE, wq, kret, 2, 0, 0);
		}
	}
	if (kret != KERN_SUCCESS) {
		(void) thread_terminate(th);
		thread_deallocate(th);

		kfree(tl, sizeof(struct threadlist));
		goto failed;
	}
	thread_reference(th);

	pthread_kern->thread_set_tag(th, THREAD_TAG_PTHREAD | THREAD_TAG_WORKQUEUE);

	sright = (void *)pthread_kern->convert_thread_to_port(th);
	tl->th_thport = pthread_kern->ipc_port_copyout_send(sright, pthread_kern->task_get_ipcspace(wq->wq_task));

	pthread_kern->thread_static_param(th, TRUE);

	tl->th_flags = TH_LIST_INITED | TH_LIST_NEW;

	tl->th_thread = th;
	tl->th_workq = wq;
	tl->th_stackaddr = stackaddr;
	tl->th_priority = WORKQUEUE_NUM_BUCKETS;

	uth = pthread_kern->get_bsdthread_info(tl->th_thread);

	workqueue_lock_spin(wq);

	pthread_kern->uthread_set_threadlist(uth, tl);
	TAILQ_INSERT_TAIL(&wq->wq_thidlelist, tl, th_entry);

	wq->wq_thidlecount++;

	PTHREAD_TRACE_WQ(TRACE_wq_thread_create | DBG_FUNC_NONE, wq, 0, 0, 0, 0);

	return (TRUE);

failed:
	workqueue_lock_spin(wq);
	wq->wq_nthreads--;

	return (FALSE);
}

/**
 * Setup per-process state for the workqueue.
 */
int
_workq_open(struct proc *p, __unused int32_t *retval)
{
	struct workqueue * wq;
	int wq_size;
	char * ptr;
	uint32_t i;
	uint32_t num_cpus;
	int error = 0;

	if (pthread_kern->proc_get_register(p) == 0) {
		return EINVAL;
	}

	num_cpus = pthread_kern->ml_get_max_cpus();

	if (wq_init_constrained_limit) {
		uint32_t limit;
		/*
		 * set up the limit for the constrained pool
		 * this is a virtual pool in that we don't
		 * maintain it on a separate idle and run list
		 */
		limit = num_cpus * WORKQUEUE_CONSTRAINED_FACTOR;

		if (limit > wq_max_constrained_threads)
			wq_max_constrained_threads = limit;

		wq_init_constrained_limit = 0;

		if (wq_max_threads > pthread_kern->config_thread_max - 20) {
			wq_max_threads = pthread_kern->config_thread_max - 20;
		}
	}

	if (pthread_kern->proc_get_wqptr(p) == NULL) {
		if (pthread_kern->proc_init_wqptr_or_wait(p) == FALSE) {
			assert(pthread_kern->proc_get_wqptr(p) != NULL);
			goto out;
		}

		wq_size = sizeof(struct workqueue);

		ptr = (char *)kalloc(wq_size);
		bzero(ptr, wq_size);

		wq = (struct workqueue *)ptr;
		wq->wq_flags = WQ_LIST_INITED;
		wq->wq_proc = p;
		wq->wq_max_concurrency = wq_max_concurrency;
		wq->wq_task = current_task();
		wq->wq_map  = pthread_kern->current_map();

		for (i = 0; i < WORKQUEUE_NUM_BUCKETS; i++)
			wq->wq_reqconc[i] = (uint16_t)wq->wq_max_concurrency;

		// The event manager bucket is special, so its gets a concurrency of 1
		// though we shouldn't ever read this value for that bucket
		wq->wq_reqconc[WORKQUEUE_EVENT_MANAGER_BUCKET] = 1;

		// Start the event manager at the priority hinted at by the policy engine
		int mgr_priority_hint = pthread_kern->task_get_default_manager_qos(current_task());
		wq->wq_event_manager_priority = (uint32_t)thread_qos_get_pthread_priority(mgr_priority_hint) | _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;

		TAILQ_INIT(&wq->wq_thrunlist);
		TAILQ_INIT(&wq->wq_thidlelist);

		wq->wq_atimer_delayed_call =
				thread_call_allocate_with_priority((thread_call_func_t)workqueue_add_timer,
						(thread_call_param_t)wq, THREAD_CALL_PRIORITY_KERNEL);
		wq->wq_atimer_immediate_call =
				thread_call_allocate_with_priority((thread_call_func_t)workqueue_add_timer,
						(thread_call_param_t)wq, THREAD_CALL_PRIORITY_KERNEL);

		lck_spin_init(&wq->wq_lock, pthread_lck_grp, pthread_lck_attr);

		pthread_kern->proc_set_wqptr(p, wq);

	}
out:

	return(error);
}

/*
 * Routine:	workqueue_mark_exiting
 *
 * Function:	Mark the work queue such that new threads will not be added to the
 *		work queue after we return.
 *
 * Conditions:	Called against the current process.
 */
void
_workqueue_mark_exiting(struct proc *p)
{
	struct workqueue *wq = pthread_kern->proc_get_wqptr(p);

	if (wq != NULL) {

		PTHREAD_TRACE_WQ(TRACE_wq_pthread_exit|DBG_FUNC_START, wq, 0, 0, 0, 0);

		workqueue_lock_spin(wq);

		/*
		 * We arm the add timer without holding the workqueue lock so we need
		 * to synchronize with any running or soon to be running timers.
		 *
		 * Threads that intend to arm the timer atomically OR
		 * WQ_ATIMER_{DELAYED,IMMEDIATE}_RUNNING into the wq_flags, only if
		 * WQ_EXITING is not present.  So, once we have set WQ_EXITING, we can
		 * be sure that no new RUNNING flags will be set, but still need to
		 * wait for the already running timers to complete.
		 *
		 * We always hold the workq lock when dropping WQ_ATIMER_RUNNING, so
		 * the check for and sleep until clear is protected.
		 */
		WQ_SETFLAG(wq, WQ_EXITING);

		if (wq->wq_flags & WQ_ATIMER_DELAYED_RUNNING) {
			if (thread_call_cancel(wq->wq_atimer_delayed_call) == TRUE) {
				WQ_UNSETFLAG(wq, WQ_ATIMER_DELAYED_RUNNING);
			}
		}
		if (wq->wq_flags & WQ_ATIMER_IMMEDIATE_RUNNING) {
			if (thread_call_cancel(wq->wq_atimer_immediate_call) == TRUE) {
				WQ_UNSETFLAG(wq, WQ_ATIMER_IMMEDIATE_RUNNING);
			}
		}
		while (wq->wq_flags & (WQ_ATIMER_DELAYED_RUNNING | WQ_ATIMER_IMMEDIATE_RUNNING) ||
				(wq->wq_lflags & WQL_ATIMER_BUSY)) {
			assert_wait((caddr_t)wq, (THREAD_UNINT));
			workqueue_unlock(wq);

			thread_block(THREAD_CONTINUE_NULL);

			workqueue_lock_spin(wq);
		}
		workqueue_unlock(wq);

		PTHREAD_TRACE(TRACE_wq_pthread_exit|DBG_FUNC_END, 0, 0, 0, 0, 0);
	}
}

/*
 * Routine:	workqueue_exit
 *
 * Function:	clean up the work queue structure(s) now that there are no threads
 *		left running inside the work queue (except possibly current_thread).
 *
 * Conditions:	Called by the last thread in the process.
 *		Called against current process.
 */
void
_workqueue_exit(struct proc *p)
{
	struct workqueue  * wq;
	struct threadlist  * tl, *tlist;
	struct uthread	*uth;
	size_t wq_size = sizeof(struct workqueue);

	wq = pthread_kern->proc_get_wqptr(p);
	if (wq != NULL) {

		PTHREAD_TRACE_WQ(TRACE_wq_workqueue_exit|DBG_FUNC_START, wq, 0, 0, 0, 0);

		pthread_kern->proc_set_wqptr(p, NULL);

		/*
		 * Clean up workqueue data structures for threads that exited and
		 * didn't get a chance to clean up after themselves.
		 */
		TAILQ_FOREACH_SAFE(tl, &wq->wq_thrunlist, th_entry, tlist) {
			assert((tl->th_flags & TH_LIST_RUNNING) != 0);

			pthread_kern->thread_sched_call(tl->th_thread, NULL);

			uth = pthread_kern->get_bsdthread_info(tl->th_thread);
			if (uth != (struct uthread *)0) {
				pthread_kern->uthread_set_threadlist(uth, NULL);
			}
			TAILQ_REMOVE(&wq->wq_thrunlist, tl, th_entry);

			/*
			 * drop our last ref on the thread
			 */
			thread_deallocate(tl->th_thread);

			kfree(tl, sizeof(struct threadlist));
		}
		TAILQ_FOREACH_SAFE(tl, &wq->wq_thidlelist, th_entry, tlist) {
			assert((tl->th_flags & TH_LIST_RUNNING) == 0);
			assert(tl->th_priority != WORKQUEUE_EVENT_MANAGER_BUCKET);
			workqueue_removethread(tl, true, false);
		}
		TAILQ_FOREACH_SAFE(tl, &wq->wq_thidlemgrlist, th_entry, tlist) {
			assert((tl->th_flags & TH_LIST_RUNNING) == 0);
			assert(tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET);
			workqueue_removethread(tl, true, false);
		}
		thread_call_free(wq->wq_atimer_delayed_call);
		thread_call_free(wq->wq_atimer_immediate_call);
		lck_spin_destroy(&wq->wq_lock, pthread_lck_grp);

		kfree(wq, wq_size);

		PTHREAD_TRACE(TRACE_wq_workqueue_exit|DBG_FUNC_END, 0, 0, 0, 0, 0);
	}
}


#pragma mark workqueue thread manipulation

/**
 * Entry point for libdispatch to ask for threads
 */
static int wqops_queue_reqthreads(struct proc *p, int reqcount, pthread_priority_t priority){
	struct workqueue *wq;
	boolean_t start_timer = FALSE;

	boolean_t overcommit = (_pthread_priority_get_flags(priority) & _PTHREAD_PRIORITY_OVERCOMMIT_FLAG) != 0;
	int class = pthread_priority_get_class_index(priority);

	boolean_t event_manager = (_pthread_priority_get_flags(priority) & _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG) != 0;
	if (event_manager){
		class = WORKQUEUE_EVENT_MANAGER_BUCKET;
	}

	if ((reqcount <= 0) || (class < 0) || (class >= WORKQUEUE_NUM_BUCKETS) || (overcommit && event_manager)) {
		return EINVAL;
	}
	

	if ((wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p)) == NULL) {
		return EINVAL;
	}

	workqueue_lock_spin(wq);
	
	if (overcommit == 0 && event_manager == 0) {
		wq->wq_reqcount += reqcount;
		wq->wq_requests[class] += reqcount;
		
		PTHREAD_TRACE_WQ(TRACE_wq_req_threads | DBG_FUNC_NONE, wq, priority, wq->wq_requests[class], reqcount, 0);
		
		while (wq->wq_reqcount) {
			if (!workqueue_run_one(p, wq, overcommit, 0))
				break;
		}
	} else if (overcommit) {
		PTHREAD_TRACE_WQ(TRACE_wq_req_octhreads | DBG_FUNC_NONE, wq, priority, wq->wq_ocrequests[class], reqcount, 0);
		
		while (reqcount) {
			if (!workqueue_run_one(p, wq, overcommit, priority))
				break;
			reqcount--;
		}
		if (reqcount) {
			/*
			 * We need to delay starting some of the overcommit requests.
			 * We'll record the request here and as existing threads return to
			 * the kernel, we'll notice the ocrequests and spin them back to
			 * user space as the overcommit variety.
			 */
			wq->wq_reqcount += reqcount;
			wq->wq_requests[class] += reqcount;
			wq->wq_ocrequests[class] += reqcount;
			
			PTHREAD_TRACE_WQ(TRACE_wq_delay_octhreads | DBG_FUNC_NONE, wq, priority, wq->wq_ocrequests[class], reqcount, 0);

			/*
			 * If we delayed this thread coming up but we're not constrained
			 * or at max threads then we need to start the timer so we don't
			 * risk dropping this request on the floor.
			 */
			if ((wq->wq_constrained_threads_scheduled < wq_max_constrained_threads) &&
					(wq->wq_nthreads < wq_max_threads)){
				start_timer = WQ_TIMER_DELAYED_NEEDED(wq);
			}
		}
	} else if (event_manager) {
		PTHREAD_TRACE_WQ(TRACE_wq_req_event_manager | DBG_FUNC_NONE, wq, wq->wq_event_manager_priority, wq->wq_requests[WORKQUEUE_EVENT_MANAGER_BUCKET], wq->wq_thscheduled_count[WORKQUEUE_EVENT_MANAGER_BUCKET], 0);

		if (wq->wq_requests[WORKQUEUE_EVENT_MANAGER_BUCKET] == 0){
			wq->wq_reqcount += 1;
			wq->wq_requests[WORKQUEUE_EVENT_MANAGER_BUCKET] = 1;
		}

		// We've recorded the request for an event manager thread above.  We'll
		// let the timer pick it up as we would for a kernel callout.  We can
		// do a direct add/wakeup when that support is added for the kevent path.
		if (wq->wq_thscheduled_count[WORKQUEUE_EVENT_MANAGER_BUCKET] == 0){
			start_timer = WQ_TIMER_DELAYED_NEEDED(wq);
		}
	}

	if (start_timer) {
		workqueue_interval_timer_start(wq);
	}

	workqueue_unlock(wq);

	return 0;
}

/*
 * Used by the kevent system to request threads.
 *
 * Currently count is ignored and we always return one thread per invocation.
 */
thread_t _workq_reqthreads(struct proc *p, int requests_count, workq_reqthreads_req_t requests){
	thread_t th = THREAD_NULL;
	boolean_t do_thread_call = FALSE;
	boolean_t emergency_thread = FALSE;
	assert(requests_count > 0);

#if DEBUG
	// Make sure that the requests array is sorted, highest priority first
	if (requests_count > 1){
		__assert_only qos_class_t priority = _pthread_priority_get_qos_newest(requests[0].priority);
		__assert_only unsigned long flags = ((_pthread_priority_get_flags(requests[0].priority) & (_PTHREAD_PRIORITY_OVERCOMMIT_FLAG|_PTHREAD_PRIORITY_EVENT_MANAGER_FLAG)) != 0);
		for (int i = 1; i < requests_count; i++){
			if (requests[i].count == 0) continue;
			__assert_only qos_class_t next_priority = _pthread_priority_get_qos_newest(requests[i].priority);
			__assert_only unsigned long next_flags = ((_pthread_priority_get_flags(requests[i].priority) & (_PTHREAD_PRIORITY_OVERCOMMIT_FLAG|_PTHREAD_PRIORITY_EVENT_MANAGER_FLAG)) != 0);
			if (next_flags != flags){
				flags = next_flags;
				priority = next_priority;
			} else {
				assert(next_priority <= priority);
			}
		}
	}
#endif // DEBUG

	struct workqueue *wq;
	if ((wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p)) == NULL) {
		return THREAD_NULL;
	}

	workqueue_lock_spin(wq);

	PTHREAD_TRACE_WQ(TRACE_wq_kevent_req_threads | DBG_FUNC_START, wq, requests_count, 0, 0, 0);

	// Look for overcommit or event-manager-only requests.
	boolean_t have_overcommit = FALSE;
	pthread_priority_t priority = 0;
	for (int i = 0; i < requests_count; i++){
		if (requests[i].count == 0)
			continue;
		priority = requests[i].priority;
		if (_pthread_priority_get_qos_newest(priority) == QOS_CLASS_UNSPECIFIED){
			priority |= _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;
		}
		if ((_pthread_priority_get_flags(priority) & _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG) != 0){
			goto event_manager;
		}
		if ((_pthread_priority_get_flags(priority) & _PTHREAD_PRIORITY_OVERCOMMIT_FLAG) != 0){
			have_overcommit = TRUE;
			break;
		}
	}

	if (have_overcommit){
		if (wq->wq_thidlecount){
			th = workqueue_run_nextreq(p, wq, THREAD_NULL, RUN_NEXTREQ_OVERCOMMIT_KEVENT, priority, true);
			if (th != THREAD_NULL){
				goto out;
			} else {
				workqueue_lock_spin(wq); // reacquire lock
			}
		}

		int class = pthread_priority_get_class_index(priority);
		wq->wq_reqcount += 1;
		wq->wq_requests[class] += 1;
		wq->wq_kevent_ocrequests[class] += 1;

		do_thread_call = WQ_TIMER_IMMEDIATE_NEEDED(wq);
		goto deferred;
	}

	// Having no overcommit requests, try to find any request that can start
	// There's no TOCTTOU since we hold the workqueue lock
	for (int i = 0; i < requests_count; i++){
		workq_reqthreads_req_t req = requests + i;
		priority = req->priority;
		int class = pthread_priority_get_class_index(priority);

		if (req->count == 0)
			continue;

		if (!may_start_constrained_thread(wq, class, WORKQUEUE_NUM_BUCKETS, NULL))
			continue;

		wq->wq_reqcount += 1;
		wq->wq_requests[class] += 1;
		wq->wq_kevent_requests[class] += 1;

		PTHREAD_TRACE_WQ(TRACE_wq_req_kevent_threads | DBG_FUNC_NONE, wq, priority, wq->wq_kevent_requests[class], 1, 0);

		if (wq->wq_thidlecount){
			th = workqueue_run_nextreq(p, wq, THREAD_NULL, RUN_NEXTREQ_DEFAULT_KEVENT, priority, true);
			goto out;
		} else {
			do_thread_call = WQ_TIMER_IMMEDIATE_NEEDED(wq);
			goto deferred;
		}
	}

	// Okay, here's the fun case: we can't spin up any of the non-overcommit threads
	// that we've seen a request for, so we kick this over to the event manager thread
	emergency_thread = TRUE;

event_manager:
	if (wq->wq_requests[WORKQUEUE_EVENT_MANAGER_BUCKET] == 0){
		wq->wq_reqcount += 1;
		wq->wq_requests[WORKQUEUE_EVENT_MANAGER_BUCKET] = 1;
		PTHREAD_TRACE_WQ(TRACE_wq_req_event_manager | DBG_FUNC_NONE, wq, 0, wq->wq_kevent_requests[WORKQUEUE_EVENT_MANAGER_BUCKET], 1, 0);
	} else {
		PTHREAD_TRACE_WQ(TRACE_wq_req_event_manager | DBG_FUNC_NONE, wq, 0, wq->wq_kevent_requests[WORKQUEUE_EVENT_MANAGER_BUCKET], 0, 0);
	}
	wq->wq_kevent_requests[WORKQUEUE_EVENT_MANAGER_BUCKET] = 1;

	if (wq->wq_thidlecount && wq->wq_thscheduled_count[WORKQUEUE_EVENT_MANAGER_BUCKET] == 0){
		th = workqueue_run_nextreq(p, wq, THREAD_NULL, RUN_NEXTREQ_EVENT_MANAGER, 0, true);
		assert(th != THREAD_NULL);
		goto out;
	}
	do_thread_call = WQ_TIMER_IMMEDIATE_NEEDED(wq);

deferred:
	workqueue_unlock(wq);

	if (do_thread_call == TRUE){
		workqueue_interval_timer_trigger(wq);
	}

out:
	PTHREAD_TRACE_WQ(TRACE_wq_kevent_req_threads | DBG_FUNC_END, wq, do_thread_call, 0, 0, 0);

	return emergency_thread ? (void*)-1 : th;
}


static int wqops_thread_return(struct proc *p){
	thread_t th = current_thread();
	struct uthread *uth = pthread_kern->get_bsdthread_info(th);
	struct threadlist *tl = pthread_kern->uthread_get_threadlist(uth);

	/* reset signal mask on the workqueue thread to default state */
	if (pthread_kern->uthread_get_sigmask(uth) != (sigset_t)(~workq_threadmask)) {
		pthread_kern->proc_lock(p);
		pthread_kern->uthread_set_sigmask(uth, ~workq_threadmask);
		pthread_kern->proc_unlock(p);
	}

	struct workqueue *wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p);
	if (wq == NULL || !tl) {
		return EINVAL;
	}

	PTHREAD_TRACE_WQ(TRACE_wq_override_reset | DBG_FUNC_START, tl->th_workq, 0, 0, 0, 0);

	/*
	 * This squash call has neat semantics: it removes the specified overrides,
	 * replacing the current requested QoS with the previous effective QoS from
	 * those overrides.  This means we won't be preempted due to having our QoS
	 * lowered.  Of course, now our understanding of the thread's QoS is wrong,
	 * so we'll adjust below.
	 */
	int new_qos =
	pthread_kern->proc_usynch_thread_qos_squash_override_for_resource(th,
			THREAD_QOS_OVERRIDE_RESOURCE_WILDCARD,
			THREAD_QOS_OVERRIDE_TYPE_DISPATCH_ASYNCHRONOUS_OVERRIDE);

	workqueue_lock_spin(wq);

	if (tl->th_flags & TH_LIST_KEVENT_BOUND) {
		unsigned int flags = KEVENT_FLAG_WORKQ;
		if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET) {
			flags |= KEVENT_FLAG_WORKQ_MANAGER;
		}

		workqueue_unlock(wq);
		kevent_qos_internal_unbind(p, class_index_get_thread_qos(tl->th_priority), th, flags);
		workqueue_lock_spin(wq);

		tl->th_flags &= ~TH_LIST_KEVENT_BOUND;
	}

	/* Fix up counters from the squash operation. */
	uint8_t old_bucket = tl->th_priority;
	uint8_t new_bucket = thread_qos_get_class_index(new_qos);

	if (old_bucket != new_bucket) {
		OSAddAtomic(-1, &wq->wq_thactive_count[old_bucket]);
		OSAddAtomic(1, &wq->wq_thactive_count[new_bucket]);

		wq->wq_thscheduled_count[old_bucket]--;
		wq->wq_thscheduled_count[new_bucket]++;

		tl->th_priority = new_bucket;
	}

	PTHREAD_TRACE_WQ(TRACE_wq_override_reset | DBG_FUNC_END, tl->th_workq, new_qos, 0, 0, 0);

	PTHREAD_TRACE_WQ(TRACE_wq_runitem | DBG_FUNC_END, wq, 0, 0, 0, 0);

	(void)workqueue_run_nextreq(p, wq, th, RUN_NEXTREQ_DEFAULT, 0, false);
	/*
	 * workqueue_run_nextreq is responsible for
	 * dropping the workqueue lock in all cases
	 */
	return 0;
}

/**
 * Multiplexed call to interact with the workqueue mechanism
 */
int
_workq_kernreturn(struct proc *p,
		  int options,
		  user_addr_t item,
		  int arg2,
		  int arg3,
		  int32_t *retval)
{
	int error = 0;

	if (pthread_kern->proc_get_register(p) == 0) {
		return EINVAL;
	}

	switch (options) {
	case WQOPS_QUEUE_NEWSPISUPP: {
		/*
		 * arg2 = offset of serialno into dispatch queue
		 * arg3 = kevent support
		 */
		int offset = arg2;
		if (arg3 & 0x01){
			// If we get here, then userspace has indicated support for kevent delivery.
		}

		pthread_kern->proc_set_dispatchqueue_serialno_offset(p, (uint64_t)offset);
		break;
	}
	case WQOPS_QUEUE_REQTHREADS: {
		/*
		 * arg2 = number of threads to start
		 * arg3 = priority
		 */
		error = wqops_queue_reqthreads(p, arg2, arg3);
		break;
	}
	case WQOPS_SET_EVENT_MANAGER_PRIORITY: {
		/*
		 * arg2 = priority for the manager thread
		 *
		 * if _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG is set, the
		 * ~_PTHREAD_PRIORITY_FLAGS_MASK contains a scheduling priority instead
		 * of a QOS value
		 */
		pthread_priority_t pri = arg2;

		struct workqueue *wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p);
		if (wq == NULL) {
			error = EINVAL;
			break;
		}
		workqueue_lock_spin(wq);
		if (pri & _PTHREAD_PRIORITY_SCHED_PRI_FLAG){
			// If userspace passes a scheduling priority, that takes precidence
			// over any QoS.  (So, userspace should take care not to accidenatally
			// lower the priority this way.)
			uint32_t sched_pri = pri & (~_PTHREAD_PRIORITY_FLAGS_MASK);
			if (wq->wq_event_manager_priority & _PTHREAD_PRIORITY_SCHED_PRI_FLAG){
				wq->wq_event_manager_priority = MAX(sched_pri, wq->wq_event_manager_priority & (~_PTHREAD_PRIORITY_FLAGS_MASK))
						| _PTHREAD_PRIORITY_SCHED_PRI_FLAG | _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;
			} else {
				wq->wq_event_manager_priority = sched_pri
						| _PTHREAD_PRIORITY_SCHED_PRI_FLAG | _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;
			}
		} else if ((wq->wq_event_manager_priority & _PTHREAD_PRIORITY_SCHED_PRI_FLAG) == 0){
			int cur_qos = pthread_priority_get_thread_qos(wq->wq_event_manager_priority);
			int new_qos = pthread_priority_get_thread_qos(pri);
			wq->wq_event_manager_priority = (uint32_t)thread_qos_get_pthread_priority(MAX(cur_qos, new_qos)) | _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;
		}
		workqueue_unlock(wq);
		break;
	}
	case WQOPS_THREAD_KEVENT_RETURN:
		if (item != 0) {
			int32_t kevent_retval;
			int ret = kevent_qos_internal(p, -1, item, arg2, item, arg2, NULL, NULL, KEVENT_FLAG_WORKQ | KEVENT_FLAG_IMMEDIATE | KEVENT_FLAG_ERROR_EVENTS, &kevent_retval);
			// We shouldn't be getting more errors out than events we put in, so
			// reusing the input buffer should always provide enough space.  But,
			// the assert is commented out since we get errors in edge cases in the
			// process lifecycle.
			//assert(ret == KERN_SUCCESS && kevent_retval >= 0);
			if (ret != KERN_SUCCESS){
				error = ret;
				break;
			} else if (kevent_retval > 0){
				assert(kevent_retval <= arg2);
				*retval = kevent_retval;
				error = 0;
				break;
			}
		}
		// FALLTHRU
	case WQOPS_THREAD_RETURN:
		error = wqops_thread_return(p);
		// NOT REACHED except in case of error
		assert(error);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}


static boolean_t
workqueue_run_one(proc_t p, struct workqueue *wq, boolean_t overcommit, pthread_priority_t priority)
{
	boolean_t	ran_one;

	if (wq->wq_thidlecount == 0) {
		if (overcommit == FALSE) {
			if (wq->wq_constrained_threads_scheduled < wq->wq_max_concurrency)
				workqueue_addnewthread(wq, overcommit);
		} else {
			workqueue_addnewthread(wq, overcommit);

			if (wq->wq_thidlecount == 0)
				return (FALSE);
		}
	}
	ran_one = (workqueue_run_nextreq(p, wq, THREAD_NULL, overcommit ? RUN_NEXTREQ_OVERCOMMIT : RUN_NEXTREQ_DEFAULT, priority, false) != THREAD_NULL);
	/*
	 * workqueue_run_nextreq is responsible for
	 * dropping the workqueue lock in all cases
	 */
	workqueue_lock_spin(wq);

	return (ran_one);
}

/*
 * We have no work to do, park ourselves on the idle list.
 *
 * Consumes the workqueue lock and does not return.
 */
static void __dead2
parkit(struct workqueue *wq, struct threadlist *tl, thread_t thread)
{
	assert(thread == tl->th_thread);
	assert(thread == current_thread());

	uint32_t us_to_wait = 0;

	TAILQ_REMOVE(&wq->wq_thrunlist, tl, th_entry);

	tl->th_flags &= ~TH_LIST_RUNNING;
	tl->th_flags &= ~TH_LIST_KEVENT;
	assert((tl->th_flags & TH_LIST_KEVENT_BOUND) == 0);

	if (tl->th_flags & TH_LIST_CONSTRAINED) {
		wq->wq_constrained_threads_scheduled--;
		tl->th_flags &= ~TH_LIST_CONSTRAINED;
	}

	OSAddAtomic(-1, &wq->wq_thactive_count[tl->th_priority]);
	wq->wq_thscheduled_count[tl->th_priority]--;
	wq->wq_threads_scheduled--;
	uint32_t thidlecount = ++wq->wq_thidlecount;

	pthread_kern->thread_sched_call(thread, NULL);

	/*
	 * We'd like to always have one manager thread parked so that we can have
	 * low latency when we need to bring a manager thread up.  If that idle
	 * thread list is empty, make this thread a manager thread.
	 *
	 * XXX: This doesn't check that there's not a manager thread outstanding,
	 * so it's based on the assumption that most manager callouts will change
	 * their QoS before parking.  If that stops being true, this may end up
	 * costing us more than we gain.
	 */
	if (TAILQ_EMPTY(&wq->wq_thidlemgrlist) &&
			tl->th_priority != WORKQUEUE_EVENT_MANAGER_BUCKET){
		reset_priority(tl, pthread_priority_from_wq_class_index(wq, WORKQUEUE_EVENT_MANAGER_BUCKET));
		tl->th_priority = WORKQUEUE_EVENT_MANAGER_BUCKET;
	}

	if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET){
		TAILQ_INSERT_HEAD(&wq->wq_thidlemgrlist, tl, th_entry);
	} else {
		TAILQ_INSERT_HEAD(&wq->wq_thidlelist, tl, th_entry);
	}

	PTHREAD_TRACE_WQ(TRACE_wq_thread_park | DBG_FUNC_START, wq,
			wq->wq_threads_scheduled, wq->wq_thidlecount, us_to_wait, 0);

	/*
	 * When we remove the voucher from the thread, we may lose our importance
	 * causing us to get preempted, so we do this after putting the thread on
	 * the idle list.  That when, when we get our importance back we'll be able
	 * to use this thread from e.g. the kevent call out to deliver a boosting
	 * message.
	 */
	workqueue_unlock(wq);
	kern_return_t kr = pthread_kern->thread_set_voucher_name(MACH_PORT_NULL);
	assert(kr == KERN_SUCCESS);
	workqueue_lock_spin(wq);

	if ((tl->th_flags & TH_LIST_RUNNING) == 0) {
		if (thidlecount < 101) {
			us_to_wait = wq_reduce_pool_window_usecs - ((thidlecount-2) * (wq_reduce_pool_window_usecs / 100));
		} else {
			us_to_wait = wq_reduce_pool_window_usecs / 100;
		}

		thread_set_pending_block_hint(thread, kThreadWaitParkedWorkQueue);
		assert_wait_timeout_with_leeway((caddr_t)tl, (THREAD_INTERRUPTIBLE),
				TIMEOUT_URGENCY_SYS_BACKGROUND|TIMEOUT_URGENCY_LEEWAY, us_to_wait,
				wq_reduce_pool_window_usecs/10, NSEC_PER_USEC);

		workqueue_unlock(wq);

		thread_block(wq_unpark_continue);
		panic("thread_block(wq_unpark_continue) returned!");
	} else {
		workqueue_unlock(wq);

		/*
		 * While we'd dropped the lock to unset our voucher, someone came
		 * around and made us runnable.  But because we weren't waiting on the
		 * event their wakeup() was ineffectual.  To correct for that, we just
		 * run the continuation ourselves.
		 */
		wq_unpark_continue(NULL, THREAD_AWAKENED);
	}
}

static boolean_t may_start_constrained_thread(struct workqueue *wq, uint32_t at_priclass, uint32_t my_priclass, boolean_t *start_timer){
	if (wq->wq_constrained_threads_scheduled >= wq_max_constrained_threads) {
		/*
		 * we need 1 or more constrained threads to return to the kernel before
		 * we can dispatch additional work
		 */
		return FALSE;
	}

	uint32_t busycount = 0;
	uint32_t thactive_count = wq->wq_thactive_count[at_priclass];

	// Has our most recently blocked thread blocked recently enough that we
	// should still consider it busy?
	if (wq->wq_thscheduled_count[at_priclass] > wq->wq_thactive_count[at_priclass]) {
		if (wq_thread_is_busy(mach_absolute_time(), &wq->wq_lastblocked_ts[at_priclass])) {
			busycount++;
		}
	}

	if (my_priclass < WORKQUEUE_NUM_BUCKETS && my_priclass == at_priclass){
		/*
		 * don't count this thread as currently active
		 */
		thactive_count--;
	}

	if (thactive_count + busycount >= wq->wq_max_concurrency) {
		if (busycount && start_timer) {
				/*
				 * we found at least 1 thread in the
				 * 'busy' state... make sure we start
				 * the timer because if they are the only
				 * threads keeping us from scheduling
				 * this work request, we won't get a callback
				 * to kick off the timer... we need to
				 * start it now...
				 */
				*start_timer = WQ_TIMER_DELAYED_NEEDED(wq);
		}

		PTHREAD_TRACE_WQ(TRACE_wq_overcommitted|DBG_FUNC_NONE, wq, ((start_timer && *start_timer) ? 1 << _PTHREAD_PRIORITY_FLAGS_SHIFT : 0) | class_index_get_pthread_priority(at_priclass), thactive_count, busycount, 0);

		return FALSE;
	}
	return TRUE;
}

static struct threadlist *
pop_from_thidlelist(struct workqueue *wq, uint32_t priclass)
{
	assert(wq->wq_thidlecount);

	struct threadlist *tl = NULL;

	if (!TAILQ_EMPTY(&wq->wq_thidlemgrlist) && 
			(priclass == WORKQUEUE_EVENT_MANAGER_BUCKET || TAILQ_EMPTY(&wq->wq_thidlelist))){
		tl = TAILQ_FIRST(&wq->wq_thidlemgrlist);
		TAILQ_REMOVE(&wq->wq_thidlemgrlist, tl, th_entry);
		assert(tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET);
	} else if (!TAILQ_EMPTY(&wq->wq_thidlelist) &&
			(priclass != WORKQUEUE_EVENT_MANAGER_BUCKET || TAILQ_EMPTY(&wq->wq_thidlemgrlist))){
		tl = TAILQ_FIRST(&wq->wq_thidlelist);
		TAILQ_REMOVE(&wq->wq_thidlelist, tl, th_entry);
		assert(tl->th_priority != WORKQUEUE_EVENT_MANAGER_BUCKET);
	} else {
		panic("pop_from_thidlelist called with no threads available");
	}
	assert((tl->th_flags & TH_LIST_RUNNING) == 0);

	assert(wq->wq_thidlecount);
	wq->wq_thidlecount--;

	TAILQ_INSERT_TAIL(&wq->wq_thrunlist, tl, th_entry);

	tl->th_flags |= TH_LIST_RUNNING | TH_LIST_BUSY;

	wq->wq_threads_scheduled++;
	wq->wq_thscheduled_count[priclass]++;
	OSAddAtomic(1, &wq->wq_thactive_count[priclass]);

	return tl;
}

static pthread_priority_t
pthread_priority_from_wq_class_index(struct workqueue *wq, int index){
	if (index == WORKQUEUE_EVENT_MANAGER_BUCKET){
		return wq->wq_event_manager_priority;
	} else {
		return class_index_get_pthread_priority(index);
	}
}

static void
reset_priority(struct threadlist *tl, pthread_priority_t pri){
	kern_return_t ret;
	thread_t th = tl->th_thread;

	if ((pri & _PTHREAD_PRIORITY_SCHED_PRI_FLAG) == 0){
		ret = pthread_kern->thread_set_workq_qos(th, pthread_priority_get_thread_qos(pri), 0);
		assert(ret == KERN_SUCCESS || ret == KERN_TERMINATED);

		if (tl->th_flags & TH_LIST_EVENT_MGR_SCHED_PRI) {

			/* Reset priority to default (masked by QoS) */

			ret = pthread_kern->thread_set_workq_pri(th, 31, POLICY_TIMESHARE);
			assert(ret == KERN_SUCCESS || ret == KERN_TERMINATED);

			tl->th_flags &= ~TH_LIST_EVENT_MGR_SCHED_PRI;
		}
	} else {
		ret = pthread_kern->thread_set_workq_qos(th, THREAD_QOS_UNSPECIFIED, 0);
		assert(ret == KERN_SUCCESS || ret == KERN_TERMINATED);
		ret = pthread_kern->thread_set_workq_pri(th, (pri & (~_PTHREAD_PRIORITY_FLAGS_MASK)), POLICY_TIMESHARE);
		assert(ret == KERN_SUCCESS || ret == KERN_TERMINATED);

		tl->th_flags |= TH_LIST_EVENT_MGR_SCHED_PRI;
	}
}

/**
 * grabs a thread for a request
 *
 *  - called with the workqueue lock held...
 *  - responsible for dropping it in all cases
 *  - if provided mode is for overcommit, doesn't consume a reqcount
 *
 */
static thread_t
workqueue_run_nextreq(proc_t p, struct workqueue *wq, thread_t thread,
		enum run_nextreq_mode mode, pthread_priority_t prio,
		bool kevent_bind_via_return)
{
	thread_t th_to_run = THREAD_NULL;
	uint32_t upcall_flags = 0;
	uint32_t priclass;
	struct threadlist *tl = NULL;
	struct uthread *uth = NULL;
	boolean_t start_timer = FALSE;

	if (mode == RUN_NEXTREQ_ADD_TIMER) {
		mode = RUN_NEXTREQ_DEFAULT;
	}

	// valid modes to call this function with
	assert(mode == RUN_NEXTREQ_DEFAULT || mode == RUN_NEXTREQ_DEFAULT_KEVENT ||
			mode == RUN_NEXTREQ_OVERCOMMIT || mode == RUN_NEXTREQ_UNCONSTRAINED ||
			mode == RUN_NEXTREQ_EVENT_MANAGER || mode == RUN_NEXTREQ_OVERCOMMIT_KEVENT);
	// may only have a priority if in OVERCOMMIT or DEFAULT_KEVENT mode
	assert(mode == RUN_NEXTREQ_OVERCOMMIT || mode == RUN_NEXTREQ_OVERCOMMIT_KEVENT ||
			mode == RUN_NEXTREQ_DEFAULT_KEVENT || prio == 0);
	// thread == thread_null means "please spin up a new workqueue thread, we can't reuse this"
	// thread != thread_null is thread reuse, and must be the current thread
	assert(thread == THREAD_NULL || thread == current_thread());

	PTHREAD_TRACE_WQ(TRACE_wq_run_nextitem|DBG_FUNC_START, wq, thread_tid(thread), wq->wq_thidlecount, wq->wq_reqcount, 0);

	if (thread != THREAD_NULL) {
		uth = pthread_kern->get_bsdthread_info(thread);

		if ((tl = pthread_kern->uthread_get_threadlist(uth)) == NULL) {
			panic("wq thread with no threadlist");
		}
	}

	/*
	 * from here until we drop the workq lock we can't be pre-empted since we
	 * hold the lock in spin mode... this is important since we have to
	 * independently update the priority that the thread is associated with and
	 * the priorty based counters that "workqueue_callback" also changes and
	 * bases decisions on.
	 */

	/*
	 * This giant monstrosity does three things:
	 *
	 *   - adjusts the mode, if required
	 *   - selects the priclass that we'll be servicing
	 *   - sets any mode-specific upcall flags
	 *
	 * When possible special-cases should be handled here and converted into
	 * non-special cases.
	 */
	if (mode == RUN_NEXTREQ_OVERCOMMIT) {
		priclass = pthread_priority_get_class_index(prio);
		upcall_flags |= WQ_FLAG_THREAD_OVERCOMMIT;
	} else if (mode == RUN_NEXTREQ_OVERCOMMIT_KEVENT){
		priclass = pthread_priority_get_class_index(prio);
		upcall_flags |= WQ_FLAG_THREAD_KEVENT;
	} else if (mode == RUN_NEXTREQ_EVENT_MANAGER){
		assert(wq->wq_thscheduled_count[WORKQUEUE_EVENT_MANAGER_BUCKET] == 0);
		priclass = WORKQUEUE_EVENT_MANAGER_BUCKET;
		upcall_flags |= WQ_FLAG_THREAD_EVENT_MANAGER;
		if (wq->wq_kevent_requests[WORKQUEUE_EVENT_MANAGER_BUCKET]){
			upcall_flags |= WQ_FLAG_THREAD_KEVENT;
		}
	} else if (wq->wq_reqcount == 0){
		// no work to do.  we'll check again when new work arrives.
		goto done;
	} else if (mode == RUN_NEXTREQ_DEFAULT_KEVENT) {
		assert(kevent_bind_via_return);

		priclass = pthread_priority_get_class_index(prio);
		assert(priclass < WORKQUEUE_EVENT_MANAGER_BUCKET);
		assert(wq->wq_kevent_requests[priclass] > 0);

		upcall_flags |= WQ_FLAG_THREAD_KEVENT;
		mode = RUN_NEXTREQ_DEFAULT;
	} else if (wq->wq_requests[WORKQUEUE_EVENT_MANAGER_BUCKET] &&
			   ((wq->wq_thscheduled_count[WORKQUEUE_EVENT_MANAGER_BUCKET] == 0) ||
				(thread != THREAD_NULL && tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET))){
		// There's an event manager request and either:
		//   - no event manager currently running
		//   - we are re-using the event manager
		mode = RUN_NEXTREQ_EVENT_MANAGER;
		priclass = WORKQUEUE_EVENT_MANAGER_BUCKET;
		upcall_flags |= WQ_FLAG_THREAD_EVENT_MANAGER;
		if (wq->wq_kevent_requests[WORKQUEUE_EVENT_MANAGER_BUCKET]){
			upcall_flags |= WQ_FLAG_THREAD_KEVENT;
		}
	} else {
		// Find highest priority and check for special request types
		for (priclass = 0; priclass < WORKQUEUE_EVENT_MANAGER_BUCKET; priclass++) {
			if (wq->wq_requests[priclass])
				break;
		}
		if (priclass == WORKQUEUE_EVENT_MANAGER_BUCKET){
			// only request should have been event manager since it's not in a bucket,
			// but we weren't able to handle it since there's already an event manager running,
			// so we fell into this case
			assert(wq->wq_requests[WORKQUEUE_EVENT_MANAGER_BUCKET] == 1 &&
				   wq->wq_thscheduled_count[WORKQUEUE_EVENT_MANAGER_BUCKET] == 1 &&
				   wq->wq_reqcount == 1);
			goto done;
		}

		if (wq->wq_kevent_ocrequests[priclass]){
			mode = RUN_NEXTREQ_DEFERRED_OVERCOMMIT;
			upcall_flags |= WQ_FLAG_THREAD_KEVENT;
			upcall_flags |= WQ_FLAG_THREAD_OVERCOMMIT;
		} else if (wq->wq_ocrequests[priclass]){
			mode = RUN_NEXTREQ_DEFERRED_OVERCOMMIT;
			upcall_flags |= WQ_FLAG_THREAD_OVERCOMMIT;
		} else if (wq->wq_kevent_requests[priclass]){
			upcall_flags |= WQ_FLAG_THREAD_KEVENT;
		}
	}

	assert(mode != RUN_NEXTREQ_EVENT_MANAGER || priclass == WORKQUEUE_EVENT_MANAGER_BUCKET);
	assert(mode == RUN_NEXTREQ_EVENT_MANAGER || priclass != WORKQUEUE_EVENT_MANAGER_BUCKET);

	if (mode == RUN_NEXTREQ_DEFAULT /* non-overcommit */){
		uint32_t my_priclass = (thread != THREAD_NULL) ? tl->th_priority : WORKQUEUE_NUM_BUCKETS;
		if (may_start_constrained_thread(wq, priclass, my_priclass, &start_timer) == FALSE){
			// per policy, we won't start another constrained thread
			goto done;
		}
	}

	if (thread != THREAD_NULL) {
		/*
		 * thread is non-NULL here when we return from userspace
		 * in workq_kernreturn, rather than trying to find a thread
		 * we pick up new work for this specific thread.
		 */
		th_to_run = thread;
		upcall_flags |= WQ_FLAG_THREAD_REUSE;
	} else if (wq->wq_thidlecount == 0) {
		/*
		 * we have no additional threads waiting to pick up
		 * work, however, there is additional work to do.
		 */
		start_timer = WQ_TIMER_DELAYED_NEEDED(wq);

		PTHREAD_TRACE_WQ(TRACE_wq_stalled, wq, wq->wq_nthreads, start_timer, 0, 0);

		goto done;
	} else {
		// there is both work available and an idle thread, so activate a thread
		tl = pop_from_thidlelist(wq, priclass);
		th_to_run = tl->th_thread;
	}

	// Adjust counters and thread flags AKA consume the request
	// TODO: It would be lovely if OVERCOMMIT consumed reqcount
	switch (mode) {
		case RUN_NEXTREQ_DEFAULT:
		case RUN_NEXTREQ_DEFAULT_KEVENT: /* actually mapped to DEFAULT above */
		case RUN_NEXTREQ_ADD_TIMER: /* actually mapped to DEFAULT above */
		case RUN_NEXTREQ_UNCONSTRAINED:
			wq->wq_reqcount--;
			wq->wq_requests[priclass]--;

			if (mode == RUN_NEXTREQ_DEFAULT){
				if (!(tl->th_flags & TH_LIST_CONSTRAINED)) {
					wq->wq_constrained_threads_scheduled++;
					tl->th_flags |= TH_LIST_CONSTRAINED;
				}
			} else if (mode == RUN_NEXTREQ_UNCONSTRAINED){
				if (tl->th_flags & TH_LIST_CONSTRAINED) {
					wq->wq_constrained_threads_scheduled--;
					tl->th_flags &= ~TH_LIST_CONSTRAINED;
				}
			}
			if (upcall_flags & WQ_FLAG_THREAD_KEVENT){
				wq->wq_kevent_requests[priclass]--;
			}
			break;

		case RUN_NEXTREQ_EVENT_MANAGER:
			wq->wq_reqcount--;
			wq->wq_requests[priclass]--;

			if (tl->th_flags & TH_LIST_CONSTRAINED) {
				wq->wq_constrained_threads_scheduled--;
				tl->th_flags &= ~TH_LIST_CONSTRAINED;
			}
			if (upcall_flags & WQ_FLAG_THREAD_KEVENT){
				wq->wq_kevent_requests[priclass]--;
			}
			break;

		case RUN_NEXTREQ_DEFERRED_OVERCOMMIT:
			wq->wq_reqcount--;
			wq->wq_requests[priclass]--;
			if (upcall_flags & WQ_FLAG_THREAD_KEVENT){
				wq->wq_kevent_ocrequests[priclass]--;
			} else {
    			wq->wq_ocrequests[priclass]--;
			}
			/* FALLTHROUGH */
		case RUN_NEXTREQ_OVERCOMMIT:
		case RUN_NEXTREQ_OVERCOMMIT_KEVENT:
			if (tl->th_flags & TH_LIST_CONSTRAINED) {
				wq->wq_constrained_threads_scheduled--;
				tl->th_flags &= ~TH_LIST_CONSTRAINED;
			}
			break;
	}

	// Confirm we've maintained our counter invariants
	assert(wq->wq_requests[priclass] < UINT16_MAX);
	assert(wq->wq_ocrequests[priclass] < UINT16_MAX);
	assert(wq->wq_kevent_requests[priclass] < UINT16_MAX);
	assert(wq->wq_kevent_ocrequests[priclass] < UINT16_MAX);
	assert(wq->wq_ocrequests[priclass] + wq->wq_kevent_requests[priclass] +
			wq->wq_kevent_ocrequests[priclass] <=
			wq->wq_requests[priclass]);

	assert((tl->th_flags & TH_LIST_KEVENT_BOUND) == 0);
	if (upcall_flags & WQ_FLAG_THREAD_KEVENT) {
		tl->th_flags |= TH_LIST_KEVENT;
	} else {
		tl->th_flags &= ~TH_LIST_KEVENT;
	}

	uint32_t orig_class = tl->th_priority;
	tl->th_priority = (uint8_t)priclass;

	if ((thread != THREAD_NULL) && (orig_class != priclass)) {
		/*
		 * we need to adjust these counters based on this
		 * thread's new disposition w/r to priority
		 */
		OSAddAtomic(-1, &wq->wq_thactive_count[orig_class]);
		OSAddAtomic(1, &wq->wq_thactive_count[priclass]);

		wq->wq_thscheduled_count[orig_class]--;
		wq->wq_thscheduled_count[priclass]++;
	}
	wq->wq_thread_yielded_count = 0;

	pthread_priority_t outgoing_priority = pthread_priority_from_wq_class_index(wq, tl->th_priority);
	PTHREAD_TRACE_WQ(TRACE_wq_reset_priority | DBG_FUNC_START, wq, thread_tid(tl->th_thread), outgoing_priority, 0, 0);
	reset_priority(tl, outgoing_priority);
	PTHREAD_TRACE_WQ(TRACE_wq_reset_priority | DBG_FUNC_END, wq, thread_tid(tl->th_thread), outgoing_priority, 0, 0);

	/*
	 * persist upcall_flags so that in can be retrieved in setup_wqthread
	 */
	tl->th_upcall_flags = upcall_flags >> WQ_FLAG_THREAD_PRIOSHIFT;

	/*
	 * if current thread is reused for work request, does not return via unix_syscall
	 */
	wq_runreq(p, th_to_run, wq, tl, (thread == th_to_run),
			(upcall_flags & WQ_FLAG_THREAD_KEVENT) && !kevent_bind_via_return);

	PTHREAD_TRACE_WQ(TRACE_wq_run_nextitem|DBG_FUNC_END, wq, thread_tid(th_to_run), mode == RUN_NEXTREQ_OVERCOMMIT, 1, 0);

	assert(!kevent_bind_via_return || (upcall_flags & WQ_FLAG_THREAD_KEVENT));
	if (kevent_bind_via_return && (upcall_flags & WQ_FLAG_THREAD_KEVENT)) {
		tl->th_flags |= TH_LIST_KEVENT_BOUND;
	}

	workqueue_unlock(wq);

	return th_to_run;

done:
	if (start_timer)
		workqueue_interval_timer_start(wq);

	PTHREAD_TRACE_WQ(TRACE_wq_run_nextitem | DBG_FUNC_END, wq, thread_tid(thread), start_timer, 3, 0);

	if (thread != THREAD_NULL){
		parkit(wq, tl, thread);
		/* NOT REACHED */
	}

	workqueue_unlock(wq);

	return THREAD_NULL;
}

/**
 * parked thread wakes up
 */
static void __dead2
wq_unpark_continue(void* __unused ptr, wait_result_t wait_result)
{
	boolean_t first_use = false;
	thread_t th = current_thread();
	proc_t p = current_proc();

	struct uthread *uth = pthread_kern->get_bsdthread_info(th);
	if (uth == NULL) goto done;

	struct threadlist *tl = pthread_kern->uthread_get_threadlist(uth);
	if (tl == NULL) goto done;

	struct workqueue *wq = tl->th_workq;

	workqueue_lock_spin(wq);

	assert(tl->th_flags & TH_LIST_INITED);

	if ((tl->th_flags & TH_LIST_NEW)){
		tl->th_flags &= ~(TH_LIST_NEW);
		first_use = true;
	}

	if ((tl->th_flags & (TH_LIST_RUNNING | TH_LIST_BUSY)) == TH_LIST_RUNNING) {
		/*
		 * The normal wakeup path.
		 */
		goto return_to_user;
	}

	if ((tl->th_flags & TH_LIST_RUNNING) == 0 &&
			wait_result == THREAD_TIMED_OUT &&
			tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET &&
			TAILQ_FIRST(&wq->wq_thidlemgrlist) == tl &&
			TAILQ_NEXT(tl, th_entry) == NULL){
		/*
		 * If we are the only idle manager and we pop'ed for self-destruction,
		 * then don't actually exit.  Instead, free our stack to save some
		 * memory and re-park.
		 */

		workqueue_unlock(wq);

		vm_map_t vmap = wq->wq_map;

		// Keep this in sync with _setup_wqthread()
		const vm_size_t       guardsize = vm_map_page_size(vmap);
		const user_addr_t     freeaddr = (user_addr_t)tl->th_stackaddr + guardsize;
		const vm_map_offset_t freesize = vm_map_trunc_page_mask((PTH_DEFAULT_STACKSIZE + guardsize + PTHREAD_T_OFFSET) - 1, vm_map_page_mask(vmap)) - guardsize;

		int kr;
		kr = mach_vm_behavior_set(vmap, freeaddr, freesize, VM_BEHAVIOR_REUSABLE);
		assert(kr == KERN_SUCCESS || kr == KERN_INVALID_ADDRESS);

		workqueue_lock_spin(wq);

		if ( !(tl->th_flags & TH_LIST_RUNNING)) {
			thread_set_pending_block_hint(th, kThreadWaitParkedWorkQueue);
			assert_wait((caddr_t)tl, (THREAD_INTERRUPTIBLE));

			workqueue_unlock(wq);

			thread_block(wq_unpark_continue);
			/* NOT REACHED */
		}
	}

	if ((tl->th_flags & TH_LIST_RUNNING) == 0) {
		assert((tl->th_flags & TH_LIST_BUSY) == 0);
		/*
		 * We were set running, but not for the purposes of actually running.
		 * This could be because the timer elapsed.  Or it could be because the
		 * thread aborted.  Either way, we need to return to userspace to exit.
		 *
		 * The call to workqueue_removethread will consume the lock.
		 */

		if (!first_use &&
				(tl->th_priority < qos_class_get_class_index(WQ_THREAD_CLEANUP_QOS) ||
				(tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET))) {
			// Reset the QoS to something low for the pthread cleanup
			pthread_priority_t cleanup_pri = _pthread_priority_make_newest(WQ_THREAD_CLEANUP_QOS, 0, 0);
			reset_priority(tl, cleanup_pri);
		}

		workqueue_removethread(tl, 0, first_use);

		if (first_use){
			pthread_kern->thread_bootstrap_return();
		} else {
			pthread_kern->unix_syscall_return(0);
		}
		/* NOT REACHED */
	}

	/*
	 * The timer woke us up or the thread was aborted.  However, we have
	 * already started to make this a runnable thread.  Wait for that to
	 * finish, then continue to userspace.
	 */
	while ((tl->th_flags & TH_LIST_BUSY)) {
		assert_wait((caddr_t)tl, (THREAD_UNINT));

		workqueue_unlock(wq);

		thread_block(THREAD_CONTINUE_NULL);

		workqueue_lock_spin(wq);
	}

return_to_user:
	workqueue_unlock(wq);
	_setup_wqthread(p, th, wq, tl, first_use);
	pthread_kern->thread_sched_call(th, workqueue_callback);
done:
	if (first_use){
		pthread_kern->thread_bootstrap_return();
	} else {
		pthread_kern->unix_syscall_return(EJUSTRETURN);
	}
	panic("Our attempt to return to userspace failed...");
}

/* called with workqueue lock held */
static void
wq_runreq(proc_t p, thread_t th, struct workqueue *wq, struct threadlist *tl,
		  boolean_t return_directly, boolean_t needs_kevent_bind)
{
	PTHREAD_TRACE1_WQ(TRACE_wq_runitem | DBG_FUNC_START, tl->th_workq, 0, 0, thread_tid(current_thread()), thread_tid(th));

	unsigned int kevent_flags = KEVENT_FLAG_WORKQ;
	if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET) {
		kevent_flags |= KEVENT_FLAG_WORKQ_MANAGER;
	}

	if (return_directly) {
		if (needs_kevent_bind) {
			assert((tl->th_flags & TH_LIST_KEVENT_BOUND) == 0);
			tl->th_flags |= TH_LIST_KEVENT_BOUND;
		}

		workqueue_unlock(wq);

		if (needs_kevent_bind) {
			kevent_qos_internal_bind(p, class_index_get_thread_qos(tl->th_priority), th, kevent_flags);
		}

		/*
		 * For preemption reasons, we want to reset the voucher as late as
		 * possible, so we do it in two places:
		 *   - Just before parking (i.e. in parkit())
		 *   - Prior to doing the setup for the next workitem (i.e. here)
		 *
		 * Those two places are sufficient to ensure we always reset it before
		 * it goes back out to user space, but be careful to not break that
		 * guarantee.
		 */
		kern_return_t kr = pthread_kern->thread_set_voucher_name(MACH_PORT_NULL);
		assert(kr == KERN_SUCCESS);

		_setup_wqthread(p, th, wq, tl, false);

		PTHREAD_TRACE_WQ(TRACE_wq_run_nextitem|DBG_FUNC_END, tl->th_workq, 0, 0, 4, 0);

		pthread_kern->unix_syscall_return(EJUSTRETURN);
		/* NOT REACHED */
	}

	if (needs_kevent_bind) {
		// Leave TH_LIST_BUSY set so that the thread can't beat us to calling kevent
		workqueue_unlock(wq);
		assert((tl->th_flags & TH_LIST_KEVENT_BOUND) == 0);
		kevent_qos_internal_bind(p, class_index_get_thread_qos(tl->th_priority), th, kevent_flags);
		tl->th_flags |= TH_LIST_KEVENT_BOUND;
		workqueue_lock_spin(wq);
	}
	tl->th_flags &= ~(TH_LIST_BUSY);
	thread_wakeup_thread(tl,th);
}

#define KEVENT_LIST_LEN 16 // WORKQ_KEVENT_EVENT_BUFFER_LEN
#define KEVENT_DATA_SIZE (32 * 1024)

/**
 * configures initial thread stack/registers to jump into:
 * _pthread_wqthread(pthread_t self, mach_port_t kport, void *stackaddr, void *keventlist, int upcall_flags, int nkevents);
 * to get there we jump through assembily stubs in pthread_asm.s.  Those
 * routines setup a stack frame, using the current stack pointer, and marshall
 * arguments from registers to the stack as required by the ABI.
 *
 * One odd thing we do here is to start the pthread_t 4k below what would be the
 * top of the stack otherwise.  This is because usually only the first 4k of the
 * pthread_t will be used and so we want to put it on the same 16k page as the
 * top of the stack to save memory.
 *
 * When we are done the stack will look like:
 * |-----------| th_stackaddr + th_allocsize
 * |pthread_t  | th_stackaddr + DEFAULT_STACKSIZE + guardsize + PTHREAD_STACK_OFFSET
 * |kevent list| optionally - at most KEVENT_LIST_LEN events
 * |kevent data| optionally - at most KEVENT_DATA_SIZE bytes
 * |stack gap  | bottom aligned to 16 bytes, and at least as big as stack_gap_min
 * |   STACK   |
 * |     ⇓     |
 * |           |
 * |guard page | guardsize
 * |-----------| th_stackaddr
 */
void
_setup_wqthread(proc_t p, thread_t th, struct workqueue *wq, struct threadlist *tl,
		bool first_use)
{
	int error;
	uint32_t upcall_flags;

	pthread_priority_t priority = pthread_priority_from_wq_class_index(wq, tl->th_priority);

	const vm_size_t guardsize = vm_map_page_size(tl->th_workq->wq_map);
	const vm_size_t stack_gap_min = (proc_is64bit(p) == 0) ? C_32_STK_ALIGN : C_64_REDZONE_LEN;
	const vm_size_t stack_align_min = (proc_is64bit(p) == 0) ? C_32_STK_ALIGN : C_64_STK_ALIGN;

	user_addr_t pthread_self_addr = (user_addr_t)(tl->th_stackaddr + PTH_DEFAULT_STACKSIZE + guardsize + PTHREAD_T_OFFSET);
	user_addr_t stack_top_addr = (user_addr_t)((pthread_self_addr - stack_gap_min) & -stack_align_min);
	user_addr_t stack_bottom_addr = (user_addr_t)(tl->th_stackaddr + guardsize);

	user_addr_t wqstart_fnptr = pthread_kern->proc_get_wqthread(p);
	if (!wqstart_fnptr) {
		panic("workqueue thread start function pointer is NULL");
	}

	/* Put the QoS class value into the lower bits of the reuse_thread register, this is where
	 * the thread priority used to be stored anyway.
	 */
	upcall_flags  = tl->th_upcall_flags << WQ_FLAG_THREAD_PRIOSHIFT;
	upcall_flags |= (_pthread_priority_get_qos_newest(priority) & WQ_FLAG_THREAD_PRIOMASK);

	upcall_flags |= WQ_FLAG_THREAD_NEWSPI;

	uint32_t tsd_offset = pthread_kern->proc_get_pthread_tsd_offset(p);
	if (tsd_offset) {
		mach_vm_offset_t th_tsd_base = (mach_vm_offset_t)pthread_self_addr + tsd_offset;
		kern_return_t kret = pthread_kern->thread_set_tsd_base(th, th_tsd_base);
		if (kret == KERN_SUCCESS) {
			upcall_flags |= WQ_FLAG_THREAD_TSD_BASE_SET;
		}
	}

	if (first_use) {
		/*
		* Pre-fault the first page of the new thread's stack and the page that will
		* contain the pthread_t structure.
		*/
		vm_map_t vmap = pthread_kern->current_map();
		if (vm_map_trunc_page_mask((vm_map_offset_t)(stack_top_addr - C_64_REDZONE_LEN), vm_map_page_mask(vmap)) !=
				vm_map_trunc_page_mask((vm_map_offset_t)pthread_self_addr, vm_map_page_mask(vmap))){
			vm_fault( vmap,
					vm_map_trunc_page_mask((vm_map_offset_t)(stack_top_addr - C_64_REDZONE_LEN), vm_map_page_mask(vmap)),
					VM_PROT_READ | VM_PROT_WRITE,
					FALSE,
					THREAD_UNINT, NULL, 0);
		}
		vm_fault( vmap,
				vm_map_trunc_page_mask((vm_map_offset_t)pthread_self_addr, vm_map_page_mask(vmap)),
				VM_PROT_READ | VM_PROT_WRITE,
				FALSE,
				THREAD_UNINT, NULL, 0);
	} else {
		upcall_flags |= WQ_FLAG_THREAD_REUSE;
	}

	user_addr_t kevent_list = NULL;
	int kevent_count = 0;
	if (upcall_flags & WQ_FLAG_THREAD_KEVENT){
		kevent_list = pthread_self_addr - KEVENT_LIST_LEN * sizeof(struct kevent_qos_s);
		kevent_count = KEVENT_LIST_LEN;

		user_addr_t kevent_data_buf = kevent_list - KEVENT_DATA_SIZE;
		user_size_t kevent_data_available = KEVENT_DATA_SIZE;

		int32_t events_out = 0;

		assert(tl->th_flags | TH_LIST_KEVENT_BOUND);
		unsigned int flags = KEVENT_FLAG_WORKQ | KEVENT_FLAG_STACK_DATA | KEVENT_FLAG_IMMEDIATE;
		if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET) {
			flags |= KEVENT_FLAG_WORKQ_MANAGER;
		}
		int ret = kevent_qos_internal(p, class_index_get_thread_qos(tl->th_priority), NULL, 0, kevent_list, kevent_count,
									  kevent_data_buf, &kevent_data_available,
									  flags, &events_out);

		// turns out there are a lot of edge cases where this will fail, so not enabled by default
		//assert((ret == KERN_SUCCESS && events_out != -1) || ret == KERN_ABORTED);

		// squash any errors into just empty output on
		if (ret != KERN_SUCCESS || events_out == -1){
			events_out = 0;
			kevent_data_available = KEVENT_DATA_SIZE;
		}

		// We shouldn't get data out if there aren't events available
		assert(events_out != 0 || kevent_data_available == KEVENT_DATA_SIZE);

		if (events_out > 0){
			if (kevent_data_available == KEVENT_DATA_SIZE){
				stack_top_addr = (kevent_list - stack_gap_min) & -stack_align_min;
			} else {
				stack_top_addr = (kevent_data_buf + kevent_data_available - stack_gap_min) & -stack_align_min;
			}

			kevent_count = events_out;
		} else {
			kevent_list = NULL;
			kevent_count = 0;
		}
	}

#if defined(__i386__) || defined(__x86_64__)
	if (proc_is64bit(p) == 0) {
		x86_thread_state32_t state = {
			.eip = (unsigned int)wqstart_fnptr,
			.eax = /* arg0 */ (unsigned int)pthread_self_addr,
			.ebx = /* arg1 */ (unsigned int)tl->th_thport,
			.ecx = /* arg2 */ (unsigned int)stack_bottom_addr,
			.edx = /* arg3 */ (unsigned int)kevent_list,
			.edi = /* arg4 */ (unsigned int)upcall_flags,
			.esi = /* arg5 */ (unsigned int)kevent_count,

			.esp = (int)((vm_offset_t)stack_top_addr),
		};

		error = pthread_kern->thread_set_wq_state32(th, (thread_state_t)&state);
		if (error != KERN_SUCCESS) {
			panic(__func__ ": thread_set_wq_state failed: %d", error);
		}
	} else {
		x86_thread_state64_t state64 = {
			// x86-64 already passes all the arguments in registers, so we just put them in their final place here
			.rip = (uint64_t)wqstart_fnptr,
			.rdi = (uint64_t)pthread_self_addr,
			.rsi = (uint64_t)tl->th_thport,
			.rdx = (uint64_t)stack_bottom_addr,
			.rcx = (uint64_t)kevent_list,
			.r8  = (uint64_t)upcall_flags,
			.r9  = (uint64_t)kevent_count,

			.rsp = (uint64_t)(stack_top_addr)
		};

		error = pthread_kern->thread_set_wq_state64(th, (thread_state_t)&state64);
		if (error != KERN_SUCCESS) {
			panic(__func__ ": thread_set_wq_state failed: %d", error);
		}
	}
#else
#error setup_wqthread  not defined for this architecture
#endif
}

#if DEBUG
static int wq_kevent_test SYSCTL_HANDLER_ARGS {
	//(struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req)
#pragma unused(oidp, arg1, arg2)
	int error;
	struct workq_reqthreads_req_s requests[64] = {};

	if (req->newlen > sizeof(requests) || req->newlen < sizeof(struct workq_reqthreads_req_s))
		return EINVAL;

	error = copyin(req->newptr, requests, req->newlen);
	if (error) return error;

	_workq_reqthreads(req->p, (int)(req->newlen / sizeof(struct workq_reqthreads_req_s)), requests);
	
	return 0;
}
#endif // DEBUG
	
#pragma mark - Misc

int 
_fill_procworkqueue(proc_t p, struct proc_workqueueinfo * pwqinfo)
{
	struct workqueue * wq;
	int error = 0;
	int	activecount;
	uint32_t pri;

	if ((wq = pthread_kern->proc_get_wqptr(p)) == NULL) {
		return EINVAL;
	}

	workqueue_lock_spin(wq);
	activecount = 0;

	for (pri = 0; pri < WORKQUEUE_NUM_BUCKETS; pri++) {
		activecount += wq->wq_thactive_count[pri];
	}
	pwqinfo->pwq_nthreads = wq->wq_nthreads;
	pwqinfo->pwq_runthreads = activecount;
	pwqinfo->pwq_blockedthreads = wq->wq_threads_scheduled - activecount;
	pwqinfo->pwq_state = 0;

	if (wq->wq_constrained_threads_scheduled >= wq_max_constrained_threads) {
		pwqinfo->pwq_state |= WQ_EXCEEDED_CONSTRAINED_THREAD_LIMIT;
	}

	if (wq->wq_nthreads >= wq_max_threads) {
		pwqinfo->pwq_state |= WQ_EXCEEDED_TOTAL_THREAD_LIMIT;
	}

	workqueue_unlock(wq);
	return(error);
}

uint32_t
_get_pwq_state_kdp(proc_t p)
{
	if (p == NULL) {
		return 0;
	}

	struct workqueue *wq = pthread_kern->proc_get_wqptr(p);

	if (wq == NULL || workqueue_lock_spin_is_acquired_kdp(wq)) {
		return 0;
	}

	uint32_t pwq_state = WQ_FLAGS_AVAILABLE;

	if (wq->wq_constrained_threads_scheduled >= wq_max_constrained_threads) {
		pwq_state |= WQ_EXCEEDED_CONSTRAINED_THREAD_LIMIT;
	}

	if (wq->wq_nthreads >= wq_max_threads) {
		pwq_state |= WQ_EXCEEDED_TOTAL_THREAD_LIMIT;
	}

	return pwq_state;
}

int 
_thread_selfid(__unused struct proc *p, uint64_t *retval)
{
	thread_t thread = current_thread();
	*retval = thread_tid(thread);
	return KERN_SUCCESS;
}

void
_pthread_init(void)
{
	pthread_lck_grp_attr = lck_grp_attr_alloc_init();
	pthread_lck_grp = lck_grp_alloc_init("pthread", pthread_lck_grp_attr);
	
	/*
	 * allocate the lock attribute for pthread synchronizers
	 */
	pthread_lck_attr = lck_attr_alloc_init();

	pthread_list_mlock = lck_mtx_alloc_init(pthread_lck_grp, pthread_lck_attr);
	
	pth_global_hashinit();
	psynch_thcall = thread_call_allocate(psynch_wq_cleanup, NULL);
	psynch_zoneinit();

	/*
	 * register sysctls
	 */
	sysctl_register_oid(&sysctl__kern_wq_yielded_threshold);
	sysctl_register_oid(&sysctl__kern_wq_yielded_window_usecs);
	sysctl_register_oid(&sysctl__kern_wq_stalled_window_usecs);
	sysctl_register_oid(&sysctl__kern_wq_reduce_pool_window_usecs);
	sysctl_register_oid(&sysctl__kern_wq_max_timer_interval_usecs);
	sysctl_register_oid(&sysctl__kern_wq_max_threads);
	sysctl_register_oid(&sysctl__kern_wq_max_constrained_threads);
	sysctl_register_oid(&sysctl__kern_pthread_debug_tracing);

#if DEBUG
	sysctl_register_oid(&sysctl__kern_wq_max_concurrency);
	sysctl_register_oid(&sysctl__debug_wq_kevent_test);
#endif

	wq_max_concurrency = pthread_kern->ml_get_max_cpus();

}
