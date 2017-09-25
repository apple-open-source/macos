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
#include <kern/zalloc.h>
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

zone_t pthread_zone_workqueue;
zone_t pthread_zone_threadlist;
zone_t pthread_zone_threadreq;

extern void thread_set_cthreadself(thread_t thread, uint64_t pself, int isLP64);
extern void workqueue_thread_yielded(void);

#define WQ_SETUP_FIRST_USE  1
#define WQ_SETUP_CLEAR_VOUCHER  2
static void _setup_wqthread(proc_t p, thread_t th, struct workqueue *wq,
		struct threadlist *tl, int flags);

static void reset_priority(struct threadlist *tl, pthread_priority_t pri);
static pthread_priority_t pthread_priority_from_wq_class_index(struct workqueue *wq, int index);

static void wq_unpark_continue(void* ptr, wait_result_t wait_result) __dead2;

static bool workqueue_addnewthread(proc_t p, struct workqueue *wq);
static void workqueue_removethread(struct threadlist *tl, bool fromexit, bool first_use);
static void workqueue_lock_spin(struct workqueue *);
static void workqueue_unlock(struct workqueue *);

#define WQ_RUN_TR_THROTTLED 0
#define WQ_RUN_TR_THREAD_NEEDED 1
#define WQ_RUN_TR_THREAD_STARTED 2
#define WQ_RUN_TR_EXITING 3
static int workqueue_run_threadreq_and_unlock(proc_t p, struct workqueue *wq,
		struct threadlist *tl, struct threadreq *req, bool may_add_new_thread);

static bool may_start_constrained_thread(struct workqueue *wq,
		uint32_t at_priclass, struct threadlist *tl, bool may_start_timer);

static mach_vm_offset_t stack_addr_hint(proc_t p, vm_map_t vmap);
static boolean_t wq_thread_is_busy(uint64_t cur_ts,
		_Atomic uint64_t *lastblocked_tsp);

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

static uint32_t wq_stalled_window_usecs	= WQ_STALLED_WINDOW_USECS;
static uint32_t wq_reduce_pool_window_usecs	= WQ_REDUCE_POOL_WINDOW_USECS;
static uint32_t wq_max_timer_interval_usecs	= WQ_MAX_TIMER_INTERVAL_USECS;
static uint32_t wq_max_threads			= WORKQUEUE_MAXTHREADS;
static uint32_t wq_max_constrained_threads	= WORKQUEUE_MAXTHREADS / 8;
static uint32_t wq_max_concurrency[WORKQUEUE_NUM_BUCKETS + 1]; // set to ncpus on load

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
static int wq_kevent_test SYSCTL_HANDLER_ARGS;
SYSCTL_PROC(_debug, OID_AUTO, wq_kevent_test, CTLFLAG_MASKED | CTLFLAG_RW | CTLFLAG_LOCKED | CTLFLAG_ANYBODY | CTLTYPE_OPAQUE, NULL, 0, wq_kevent_test, 0, "-");
#endif

static uint32_t wq_init_constrained_limit = 1;

uint32_t pthread_debug_tracing = 1;

SYSCTL_INT(_kern, OID_AUTO, pthread_debug_tracing, CTLFLAG_RW | CTLFLAG_LOCKED,
		   &pthread_debug_tracing, 0, "")

/*
 *       +-----+-----+-----+-----+-----+-----+-----+
 *       | MT  | BG  | UT  | DE  | IN  | UN  | mgr |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 * | pri |  5  |  4  |  3  |  2  |  1  |  0  |  6  |
 * | qos |  1  |  2  |  3  |  4  |  5  |  6  |  7  |
 * +-----+-----+-----+-----+-----+-----+-----+-----+
 */
static inline uint32_t
_wq_bucket_to_thread_qos(int pri)
{
	if (pri == WORKQUEUE_EVENT_MANAGER_BUCKET) {
		return WORKQUEUE_EVENT_MANAGER_BUCKET + 1;
	}
	return WORKQUEUE_EVENT_MANAGER_BUCKET - pri;
}

#pragma mark wq_thactive

#if defined(__LP64__)
// Layout is:
//   7 * 16 bits for each QoS bucket request count (including manager)
//   3 bits of best QoS among all pending constrained requests
//   13 bits of zeroes
#define WQ_THACTIVE_BUCKET_WIDTH 16
#define WQ_THACTIVE_QOS_SHIFT    (7 * WQ_THACTIVE_BUCKET_WIDTH)
#else
// Layout is:
//   6 * 10 bits for each QoS bucket request count (except manager)
//   1 bit for the manager bucket
//   3 bits of best QoS among all pending constrained requests
#define WQ_THACTIVE_BUCKET_WIDTH 10
#define WQ_THACTIVE_QOS_SHIFT    (6 * WQ_THACTIVE_BUCKET_WIDTH + 1)
#endif
#define WQ_THACTIVE_BUCKET_MASK  ((1U << WQ_THACTIVE_BUCKET_WIDTH) - 1)
#define WQ_THACTIVE_BUCKET_HALF  (1U << (WQ_THACTIVE_BUCKET_WIDTH - 1))
#define WQ_THACTIVE_NO_PENDING_REQUEST 6

_Static_assert(sizeof(wq_thactive_t) * CHAR_BIT - WQ_THACTIVE_QOS_SHIFT >= 3,
		"Make sure we have space to encode a QoS");

static inline wq_thactive_t
_wq_thactive_fetch_and_add(struct workqueue *wq, wq_thactive_t offset)
{
#if PTHREAD_INLINE_RMW_ATOMICS || !defined(__LP64__)
	return atomic_fetch_add_explicit(&wq->wq_thactive, offset,
			memory_order_relaxed);
#else
	return pthread_kern->atomic_fetch_add_128_relaxed(&wq->wq_thactive, offset);
#endif
}

static inline wq_thactive_t
_wq_thactive(struct workqueue *wq)
{
#if PTHREAD_INLINE_RMW_ATOMICS || !defined(__LP64__)
	return atomic_load_explicit(&wq->wq_thactive, memory_order_relaxed);
#else
	return pthread_kern->atomic_load_128_relaxed(&wq->wq_thactive);
#endif
}

#define WQ_THACTIVE_BEST_CONSTRAINED_REQ_QOS(tha) \
		((tha) >> WQ_THACTIVE_QOS_SHIFT)

static inline uint32_t
_wq_thactive_best_constrained_req_qos(struct workqueue *wq)
{
	// Avoid expensive atomic operations: the three bits we're loading are in
	// a single byte, and always updated under the workqueue lock
	wq_thactive_t v = *(wq_thactive_t *)&wq->wq_thactive;
	return WQ_THACTIVE_BEST_CONSTRAINED_REQ_QOS(v);
}

static inline wq_thactive_t
_wq_thactive_set_best_constrained_req_qos(struct workqueue *wq,
		uint32_t orig_qos, uint32_t new_qos)
{
	wq_thactive_t v;
	v = (wq_thactive_t)(new_qos - orig_qos) << WQ_THACTIVE_QOS_SHIFT;
	/*
	 * We can do an atomic add relative to the initial load because updates
	 * to this qos are always serialized under the workqueue lock.
	 */
	return _wq_thactive_fetch_and_add(wq, v) + v;
}

static inline wq_thactive_t
_wq_thactive_offset_for_qos(int qos)
{
	return (wq_thactive_t)1 << (qos * WQ_THACTIVE_BUCKET_WIDTH);
}

static inline wq_thactive_t
_wq_thactive_inc(struct workqueue *wq, int qos)
{
	return _wq_thactive_fetch_and_add(wq, _wq_thactive_offset_for_qos(qos));
}

static inline wq_thactive_t
_wq_thactive_dec(struct workqueue *wq, int qos)
{
	return _wq_thactive_fetch_and_add(wq, -_wq_thactive_offset_for_qos(qos));
}

static inline wq_thactive_t
_wq_thactive_move(struct workqueue *wq, int oldqos, int newqos)
{
	return _wq_thactive_fetch_and_add(wq, _wq_thactive_offset_for_qos(newqos) -
			_wq_thactive_offset_for_qos(oldqos));
}

static inline uint32_t
_wq_thactive_aggregate_downto_qos(struct workqueue *wq, wq_thactive_t v,
		int qos, uint32_t *busycount, uint32_t *max_busycount)
{
	uint32_t count = 0, active;
	uint64_t curtime;

#ifndef __LP64__
	/*
	 * on 32bits the manager bucket is a single bit and the best constrained
	 * request QoS 3 bits are where the 10 bits of a regular QoS bucket count
	 * would be. Mask them out.
	 */
	v &= ~(~0ull << WQ_THACTIVE_QOS_SHIFT);
#endif
	if (busycount) {
		curtime = mach_absolute_time();
		*busycount = 0;
	}
	if (max_busycount) {
		*max_busycount = qos + 1;
	}
	for (int i = 0; i <= qos; i++, v >>= WQ_THACTIVE_BUCKET_WIDTH) {
		active = v & WQ_THACTIVE_BUCKET_MASK;
		count += active;
		if (busycount && wq->wq_thscheduled_count[i] > active) {
			if (wq_thread_is_busy(curtime, &wq->wq_lastblocked_ts[i])) {
				/*
				 * We only consider the last blocked thread for a given bucket
				 * as busy because we don't want to take the list lock in each
				 * sched callback. However this is an approximation that could
				 * contribute to thread creation storms.
				 */
				(*busycount)++;
			}
		}
	}
	return count;
}

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
	user_addr_t main_thread_stack_top = 0;
	if (pthread_kern->proc_get_user_stack) {
		main_thread_stack_top = pthread_kern->proc_get_user_stack(p);
	}
	if (proc64bit && main_thread_stack_top) {
		// The main thread stack position is randomly slid by xnu (c.f.
		// load_main() in mach_loader.c), so basing pthread stack allocations
		// where the main thread stack ends is already ASLRd and doing so
		// avoids creating a gap in the process address space that may cause
		// extra PTE memory usage. rdar://problem/33328206
		stackaddr = vm_map_trunc_page_mask((vm_map_offset_t)main_thread_stack_top,
				vm_map_page_mask(vmap));
	} else {
		// vm_map_get_max_aslr_slide_pages ensures 1MB of slide, we do better
		aslr_offset = random() % ((proc64bit ? 4 : 2) * PTH_DEFAULT_STACKSIZE);
		aslr_offset = vm_map_trunc_page_mask((vm_map_offset_t)aslr_offset,
				vm_map_page_mask(vmap));
		if (proc64bit) {
			// 64 stacks below shared region
			stackaddr = SHARED_REGION_BASE_ARM64 - 64 * PTH_DEFAULT_STACKSIZE - aslr_offset;
		} else {
			// If you try to slide down from this point, you risk ending up in memory consumed by malloc
			stackaddr = SHARED_REGION_BASE_ARM - 32 * PTH_DEFAULT_STACKSIZE + aslr_offset;
		}
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
	if (!MACH_PORT_VALID(th_thport)) {
		error = EMFILE; // userland will convert this into a crash
		goto out;
	}

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

	if (pthread_kern->proc_get_mach_thread_self_tsd_offset) {
		uint64_t mach_thread_self_offset =
				pthread_kern->proc_get_mach_thread_self_tsd_offset(p);
		if (mach_thread_self_offset && tsd_offset) {
			bool proc64bit = proc_is64bit(p);
			if (proc64bit) {
				uint64_t th_thport_tsd = (uint64_t)th_thport;
				error = copyout(&th_thport_tsd, th_pthread + tsd_offset +
						mach_thread_self_offset, sizeof(th_thport_tsd));
			} else {
				uint32_t th_thport_tsd = (uint32_t)th_thport;
				error = copyout(&th_thport_tsd, th_pthread + tsd_offset +
						mach_thread_self_offset, sizeof(th_thport_tsd));
			}
			if (error) {
				goto out1;
			}
		}
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
	if (pthread_kern->thread_will_park_or_terminate) {
		pthread_kern->thread_will_park_or_terminate(th);
	}
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

	if (pthread_kern->thread_will_park_or_terminate) {
		pthread_kern->thread_will_park_or_terminate(th);
	}
	(void)thread_terminate(th);
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
	struct _pthread_registration_data data = {};
	uint32_t max_tsd_offset;
	kern_return_t kr;
	size_t pthread_init_sz = 0;

	/* syscall randomizer test can pass bogus values */
	if (pthsize < 0 || pthsize > MAX_PTHREAD_SIZE) {
		return(EINVAL);
	}
	/*
	 * if we have pthread_init_data, then we use that and target_concptr
	 * (which is an offset) get data.
	 */
	if (pthread_init_data != 0) {
		if (pthread_init_data_size < sizeof(data.version)) {
			return EINVAL;
		}
		pthread_init_sz = MIN(sizeof(data), (size_t)pthread_init_data_size);
		int ret = copyin(pthread_init_data, &data, pthread_init_sz);
		if (ret) {
			return ret;
		}
		if (data.version != (size_t)pthread_init_data_size) {
			return EINVAL;
		}
	} else {
		data.dispatch_queue_offset = dispatchqueue_offset;
	}

	/* We have to do this before proc_get_register so that it resets after fork */
	mach_vm_offset_t stackaddr = stack_addr_hint(p, pthread_kern->current_map());
	pthread_kern->proc_set_stack_addr_hint(p, (user_addr_t)stackaddr);

	/* prevent multiple registrations */
	if (pthread_kern->proc_get_register(p) != 0) {
		return(EINVAL);
	}

	pthread_kern->proc_set_threadstart(p, threadstart);
	pthread_kern->proc_set_wqthread(p, wqthread);
	pthread_kern->proc_set_pthsize(p, pthsize);
	pthread_kern->proc_set_register(p);

	uint32_t tsd_slot_sz = proc_is64bit(p) ? sizeof(uint64_t) : sizeof(uint32_t);
	if ((uint32_t)pthsize >= tsd_slot_sz &&
			data.tsd_offset <= (uint32_t)(pthsize - tsd_slot_sz)) {
		max_tsd_offset = ((uint32_t)pthsize - data.tsd_offset - tsd_slot_sz);
	} else {
		data.tsd_offset = 0;
		max_tsd_offset = 0;
	}
	pthread_kern->proc_set_pthread_tsd_offset(p, data.tsd_offset);

	if (data.dispatch_queue_offset > max_tsd_offset) {
		data.dispatch_queue_offset = 0;
	}
	pthread_kern->proc_set_dispatchqueue_offset(p, data.dispatch_queue_offset);

	if (pthread_kern->proc_set_return_to_kernel_offset) {
		if (data.return_to_kernel_offset > max_tsd_offset) {
			data.return_to_kernel_offset = 0;
		}
		pthread_kern->proc_set_return_to_kernel_offset(p,
				data.return_to_kernel_offset);
	}

	if (pthread_kern->proc_set_mach_thread_self_tsd_offset) {
		if (data.mach_thread_self_offset > max_tsd_offset) {
			data.mach_thread_self_offset = 0;
		}
		pthread_kern->proc_set_mach_thread_self_tsd_offset(p,
				data.mach_thread_self_offset);
	}

	if (pthread_init_data != 0) {
		/* Outgoing data that userspace expects as a reply */
		data.version = sizeof(struct _pthread_registration_data);
		if (pthread_kern->qos_main_thread_active()) {
			mach_msg_type_number_t nqos = THREAD_QOS_POLICY_COUNT;
			thread_qos_policy_data_t qos;
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
	}

	/* return the supported feature set as the return value. */
	*retval = PTHREAD_FEATURE_SUPPORTED;

	return(0);
}

#pragma mark - QoS Manipulation

int
_bsdthread_ctl_set_qos(struct proc *p, user_addr_t __unused cmd, mach_port_name_t kport, user_addr_t tsd_priority_addr, user_addr_t arg3, int *retval)
{
 	int rv;
	thread_t th;

	pthread_priority_t priority;

	/* Unused parameters must be zero. */
	if (arg3 != 0) {
		return EINVAL;
	}

	/* QoS is stored in a given slot in the pthread TSD. We need to copy that in and set our QoS based on it. */
	if (proc_is64bit(p)) {
		uint64_t v;
		rv = copyin(tsd_priority_addr, &v, sizeof(v));
		if (rv) goto out;
		priority = (int)(v & 0xffffffff);
	} else {
		uint32_t v;
		rv = copyin(tsd_priority_addr, &v, sizeof(v));
		if (rv) goto out;
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

	rv = _bsdthread_ctl_set_self(p, 0, priority, 0, _PTHREAD_SET_SELF_QOS_FLAG, retval);

	/* Static param the thread, we just set QoS on it, so its stuck in QoS land now. */
	/* pthread_kern->thread_static_param(th, TRUE); */ // see <rdar://problem/16433744>, for details

	thread_deallocate(th);

out:
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

boolean_t
_workq_thread_has_been_unbound(thread_t th, int qos_class)
{
	struct threadlist *tl = util_get_thread_threadlist_entry(th);
	if (!tl) {
		return FALSE;
	}

	struct workqueue *wq = tl->th_workq;
	workqueue_lock_spin(wq);

	if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET) {
		goto failure;
	} else if (qos_class != class_index_get_thread_qos(tl->th_priority)) {
		goto failure;
	}

	if ((tl->th_flags & TH_LIST_KEVENT_BOUND)){
		goto failure;
	}
	tl->th_flags &= ~TH_LIST_KEVENT_BOUND;

	workqueue_unlock(wq);
	return TRUE;

failure:
	workqueue_unlock(wq);
	return FALSE;
}

int
_bsdthread_ctl_set_self(struct proc *p, user_addr_t __unused cmd, pthread_priority_t priority, mach_port_name_t voucher, _pthread_set_flags_t flags, int __unused *retval)
{
	thread_qos_policy_data_t qos;
	mach_msg_type_number_t nqos = THREAD_QOS_POLICY_COUNT;
	boolean_t gd = FALSE;
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
			unsigned int kevent_flags = KEVENT_FLAG_WORKQ | KEVENT_FLAG_UNBIND_CHECK_FLAGS;
			if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET) {
				kevent_flags |= KEVENT_FLAG_WORKQ_MANAGER;
			}

			workqueue_unlock(wq);
			__assert_only int ret = kevent_qos_internal_unbind(p, class_index_get_thread_qos(tl->th_priority), th, kevent_flags);
			assert(ret == 0);
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

		/*
		 * If we have main-thread QoS then we don't allow a thread to come out
		 * of QOS_CLASS_UNSPECIFIED.
		 */
		if (pthread_kern->qos_main_thread_active() && qos.qos_tier ==
				THREAD_QOS_UNSPECIFIED) {
			qos_rv = EPERM;
			goto voucher;
		}

		if (!tl) {
			tl = util_get_thread_threadlist_entry(th);
			if (tl) wq = tl->th_workq;
		}

		PTHREAD_TRACE_WQ(TRACE_pthread_set_qos_self | DBG_FUNC_START, wq, qos.qos_tier, qos.tier_importance, 0, 0);

		qos.qos_tier = pthread_priority_get_thread_qos(priority);
		qos.tier_importance = (qos.qos_tier == QOS_CLASS_UNSPECIFIED) ? 0 : _pthread_priority_get_relpri(priority);

		if (qos.qos_tier == QOS_CLASS_UNSPECIFIED ||
				qos.tier_importance > 0 || qos.tier_importance < THREAD_QOS_MIN_TIER_IMPORTANCE) {
			qos_rv = EINVAL;
			goto voucher;
		}

		/*
		 * If we're a workqueue, the threadlist item priority needs adjusting,
		 * along with the bucket we were running in.
		 */
		if (tl) {
			bool try_run_threadreq = false;

			workqueue_lock_spin(wq);
			kr = pthread_kern->thread_set_workq_qos(th, qos.qos_tier, qos.tier_importance);
			assert(kr == KERN_SUCCESS || kr == KERN_TERMINATED);

			/* Fix up counters. */
			uint8_t old_bucket = tl->th_priority;
			uint8_t new_bucket = pthread_priority_get_class_index(priority);

			if (old_bucket != new_bucket) {
				_wq_thactive_move(wq, old_bucket, new_bucket);
				wq->wq_thscheduled_count[old_bucket]--;
				wq->wq_thscheduled_count[new_bucket]++;
				if (old_bucket == WORKQUEUE_EVENT_MANAGER_BUCKET ||
						old_bucket < new_bucket) {
					/*
					 * if the QoS of the thread was lowered, then this could
					 * allow for a higher QoS thread request to run, so we need
					 * to reevaluate.
					 */
					try_run_threadreq = true;
				}
				tl->th_priority = new_bucket;
			}

			bool old_overcommit = !(tl->th_flags & TH_LIST_CONSTRAINED);
			bool new_overcommit = priority & _PTHREAD_PRIORITY_OVERCOMMIT_FLAG;
			if (!old_overcommit && new_overcommit) {
				if (wq->wq_constrained_threads_scheduled-- ==
						wq_max_constrained_threads) {
					try_run_threadreq = true;
				}
				tl->th_flags &= ~TH_LIST_CONSTRAINED;
			} else if (old_overcommit && !new_overcommit) {
				wq->wq_constrained_threads_scheduled++;
				tl->th_flags |= TH_LIST_CONSTRAINED;
			}

			if (try_run_threadreq) {
				workqueue_run_threadreq_and_unlock(p, wq, NULL, NULL, true);
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

static int
_bsdthread_ctl_max_parallelism(struct proc __unused *p, user_addr_t __unused cmd,
		int qos, unsigned long flags, int *retval)
{
	_Static_assert(QOS_PARALLELISM_COUNT_LOGICAL ==
			_PTHREAD_QOS_PARALLELISM_COUNT_LOGICAL, "logical");
	_Static_assert(QOS_PARALLELISM_REALTIME ==
			_PTHREAD_QOS_PARALLELISM_REALTIME, "realtime");

	if (flags & ~(QOS_PARALLELISM_REALTIME | QOS_PARALLELISM_COUNT_LOGICAL)) {
		return EINVAL;
	}

	if (flags & QOS_PARALLELISM_REALTIME) {
		if (qos) {
			return EINVAL;
		}
	} else if (qos == THREAD_QOS_UNSPECIFIED || qos >= THREAD_QOS_LAST) {
		return EINVAL;
	}

	*retval = pthread_kern->qos_max_parallelism(qos, flags);
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
	case BSDTHREAD_CTL_QOS_MAX_PARALLELISM:
		return _bsdthread_ctl_max_parallelism(p, cmd, (int)arg1, (unsigned long)arg2, retval);
	default:
		return EINVAL;
	}
}

#pragma mark - Workqueue Implementation

#pragma mark wq_flags

static inline uint32_t
_wq_flags(struct workqueue *wq)
{
	return atomic_load_explicit(&wq->wq_flags, memory_order_relaxed);
}

static inline bool
_wq_exiting(struct workqueue *wq)
{
	return _wq_flags(wq) & WQ_EXITING;
}

static inline uint32_t
_wq_flags_or_orig(struct workqueue *wq, uint32_t v)
{
#if PTHREAD_INLINE_RMW_ATOMICS
	uint32_t state;
	do {
		state = _wq_flags(wq);
	} while (!OSCompareAndSwap(state, state | v, &wq->wq_flags));
	return state;
#else
	return atomic_fetch_or_explicit(&wq->wq_flags, v, memory_order_relaxed);
#endif
}

static inline uint32_t
_wq_flags_and_orig(struct workqueue *wq, uint32_t v)
{
#if PTHREAD_INLINE_RMW_ATOMICS
	uint32_t state;
	do {
		state = _wq_flags(wq);
	} while (!OSCompareAndSwap(state, state & v, &wq->wq_flags));
	return state;
#else
	return atomic_fetch_and_explicit(&wq->wq_flags, v, memory_order_relaxed);
#endif
}

static inline bool
WQ_TIMER_DELAYED_NEEDED(struct workqueue *wq)
{
	uint32_t oldflags, newflags;
	do {
		oldflags = _wq_flags(wq);
		if (oldflags & (WQ_EXITING | WQ_ATIMER_DELAYED_RUNNING)) {
			return false;
		}
		newflags = oldflags | WQ_ATIMER_DELAYED_RUNNING;
	} while (!OSCompareAndSwap(oldflags, newflags, &wq->wq_flags));
	return true;
}

static inline bool
WQ_TIMER_IMMEDIATE_NEEDED(struct workqueue *wq)
{
	uint32_t oldflags, newflags;
	do {
		oldflags = _wq_flags(wq);
		if (oldflags & (WQ_EXITING | WQ_ATIMER_IMMEDIATE_RUNNING)) {
			return false;
		}
		newflags = oldflags | WQ_ATIMER_IMMEDIATE_RUNNING;
	} while (!OSCompareAndSwap(oldflags, newflags, &wq->wq_flags));
	return true;
}

#pragma mark thread requests pacing

static inline uint32_t
_wq_pacing_shift_for_pri(int pri)
{
	return _wq_bucket_to_thread_qos(pri) - 1;
}

static inline int
_wq_highest_paced_priority(struct workqueue *wq)
{
	uint8_t paced = wq->wq_paced;
	int msb = paced ? 32 - __builtin_clz(paced) : 0; // fls(paced) == bit + 1
	return WORKQUEUE_EVENT_MANAGER_BUCKET - msb;
}

static inline uint8_t
_wq_pacing_bit_for_pri(int pri)
{
	return 1u << _wq_pacing_shift_for_pri(pri);
}

static inline bool
_wq_should_pace_priority(struct workqueue *wq, int pri)
{
	return wq->wq_paced >= _wq_pacing_bit_for_pri(pri);
}

static inline void
_wq_pacing_start(struct workqueue *wq, struct threadlist *tl)
{
	uint8_t bit = _wq_pacing_bit_for_pri(tl->th_priority);
	assert((tl->th_flags & TH_LIST_PACING) == 0);
	assert((wq->wq_paced & bit) == 0);
	wq->wq_paced |= bit;
	tl->th_flags |= TH_LIST_PACING;
}

static inline bool
_wq_pacing_end(struct workqueue *wq, struct threadlist *tl)
{
	if (tl->th_flags & TH_LIST_PACING) {
		uint8_t bit = _wq_pacing_bit_for_pri(tl->th_priority);
		assert((wq->wq_paced & bit) != 0);
		wq->wq_paced ^= bit;
		tl->th_flags &= ~TH_LIST_PACING;
		return wq->wq_paced < bit; // !_wq_should_pace_priority
	}
	return false;
}

#pragma mark thread requests

static void
_threadreq_init_alloced(struct threadreq *req, int priority, int flags)
{
	assert((flags & TR_FLAG_ONSTACK) == 0);
	req->tr_state = TR_STATE_NEW;
	req->tr_priority = priority;
	req->tr_flags = flags;
}

static void
_threadreq_init_stack(struct threadreq *req, int priority, int flags)
{
	req->tr_state = TR_STATE_NEW;
	req->tr_priority = priority;
	req->tr_flags = flags | TR_FLAG_ONSTACK;
}

static void
_threadreq_copy_prepare(struct workqueue *wq)
{
again:
	if (wq->wq_cached_threadreq) {
		return;
	}

	workqueue_unlock(wq);
	struct threadreq *req = zalloc(pthread_zone_threadreq);
	workqueue_lock_spin(wq);

	if (wq->wq_cached_threadreq) {
		/*
		 * We lost the race and someone left behind an extra threadreq for us
		 * to use.  Throw away our request and retry.
		 */
		workqueue_unlock(wq);
		zfree(pthread_zone_threadreq, req);
		workqueue_lock_spin(wq);
		goto again;
	} else {
		wq->wq_cached_threadreq = req;
	}

	assert(wq->wq_cached_threadreq);
}

static bool
_threadreq_copy_prepare_noblock(struct workqueue *wq)
{
	if (wq->wq_cached_threadreq) {
		return true;
	}

	wq->wq_cached_threadreq = zalloc_noblock(pthread_zone_threadreq);

	return wq->wq_cached_threadreq != NULL;
}

static inline struct threadreq_head *
_threadreq_list_for_req(struct workqueue *wq, const struct threadreq *req)
{
	if (req->tr_flags & TR_FLAG_OVERCOMMIT) {
		return &wq->wq_overcommit_reqlist[req->tr_priority];
	} else {
		return &wq->wq_reqlist[req->tr_priority];
	}
}

static void
_threadreq_enqueue(struct workqueue *wq, struct threadreq *req)
{
	assert(req && req->tr_state == TR_STATE_NEW);
	if (req->tr_priority == WORKQUEUE_EVENT_MANAGER_BUCKET) {
		assert(wq->wq_event_manager_threadreq.tr_state != TR_STATE_WAITING);
		memcpy(&wq->wq_event_manager_threadreq, req, sizeof(struct threadreq));
		req = &wq->wq_event_manager_threadreq;
		req->tr_flags &= ~(TR_FLAG_ONSTACK | TR_FLAG_NO_PACING);
	} else {
		if (req->tr_flags & TR_FLAG_ONSTACK) {
			assert(wq->wq_cached_threadreq);
			struct threadreq *newreq = wq->wq_cached_threadreq;
			wq->wq_cached_threadreq = NULL;

			memcpy(newreq, req, sizeof(struct threadreq));
			newreq->tr_flags &= ~(TR_FLAG_ONSTACK | TR_FLAG_NO_PACING);
			req->tr_state = TR_STATE_DEAD;
			req = newreq;
		}
		TAILQ_INSERT_TAIL(_threadreq_list_for_req(wq, req), req, tr_entry);
	}
	req->tr_state = TR_STATE_WAITING;
	wq->wq_reqcount++;
}

static void
_threadreq_dequeue(struct workqueue *wq, struct threadreq *req)
{
	if (req->tr_priority != WORKQUEUE_EVENT_MANAGER_BUCKET) {
		struct threadreq_head *req_list = _threadreq_list_for_req(wq, req);
#if DEBUG
		struct threadreq *cursor = NULL;
		TAILQ_FOREACH(cursor, req_list, tr_entry) {
			if (cursor == req) break;
		}
		assert(cursor == req);
#endif
		TAILQ_REMOVE(req_list, req, tr_entry);
	}
	wq->wq_reqcount--;
}

/*
 * Mark a thread request as complete.  At this point, it is treated as owned by
 * the submitting subsystem and you should assume it could be freed.
 *
 * Called with the workqueue lock held.
 */
static int
_threadreq_complete_and_unlock(proc_t p, struct workqueue *wq,
		struct threadreq *req, struct threadlist *tl)
{
	struct threadreq *req_tofree = NULL;
	bool sync = (req->tr_state == TR_STATE_NEW);
	bool workloop = req->tr_flags & TR_FLAG_WORKLOOP;
	bool onstack = req->tr_flags & TR_FLAG_ONSTACK;
	bool kevent = req->tr_flags & TR_FLAG_KEVENT;
	bool unbinding = tl->th_flags & TH_LIST_UNBINDING;
	bool locked = true;
	bool waking_parked_thread = (tl->th_flags & TH_LIST_BUSY);
	int ret;

	req->tr_state = TR_STATE_COMPLETE;

	if (!workloop && !onstack && req != &wq->wq_event_manager_threadreq) {
		if (wq->wq_cached_threadreq) {
			req_tofree = req;
		} else {
			wq->wq_cached_threadreq = req;
		}
	}

	if (tl->th_flags & TH_LIST_UNBINDING) {
		tl->th_flags &= ~TH_LIST_UNBINDING;
		assert((tl->th_flags & TH_LIST_KEVENT_BOUND));
	} else if (workloop || kevent) {
		assert((tl->th_flags & TH_LIST_KEVENT_BOUND) == 0);
		tl->th_flags |= TH_LIST_KEVENT_BOUND;
	}

	if (workloop) {
		workqueue_unlock(wq);
		ret = pthread_kern->workloop_fulfill_threadreq(wq->wq_proc, (void*)req,
				tl->th_thread, sync ? WORKLOOP_FULFILL_THREADREQ_SYNC : 0);
		assert(ret == 0);
		locked = false;
	} else if (kevent) {
		unsigned int kevent_flags = KEVENT_FLAG_WORKQ;
		if (sync) {
			kevent_flags |= KEVENT_FLAG_SYNCHRONOUS_BIND;
		}
		if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET) {
			kevent_flags |= KEVENT_FLAG_WORKQ_MANAGER;
		}
		workqueue_unlock(wq);
		ret = kevent_qos_internal_bind(wq->wq_proc,
				class_index_get_thread_qos(tl->th_priority), tl->th_thread,
				kevent_flags);
		if (ret != 0) {
			workqueue_lock_spin(wq);
			tl->th_flags &= ~TH_LIST_KEVENT_BOUND;
			locked = true;
		} else {
			locked = false;
		}
	}

	/*
	 * Run Thread, Run!
	 */
	PTHREAD_TRACE_WQ(TRACE_wq_run_threadreq | DBG_FUNC_END, wq, 0, 0, 0, 0);
	PTHREAD_TRACE_WQ_REQ(TRACE_wq_runitem | DBG_FUNC_START, wq, req, tl->th_priority,
			thread_tid(current_thread()), thread_tid(tl->th_thread));

	if (waking_parked_thread) {
		if (!locked) {
			workqueue_lock_spin(wq);
		}
		tl->th_flags &= ~(TH_LIST_BUSY);
		if ((tl->th_flags & TH_LIST_REMOVING_VOUCHER) == 0) {
			/*
			 * If the thread is in the process of removing its voucher, then it
			 * isn't actually in the wait event yet and we don't need to wake
			 * it up.  Save the trouble (and potential lock-ordering issues
			 * (see 30617015)).
			 */
			thread_wakeup_thread(tl, tl->th_thread);
		}
		workqueue_unlock(wq);

		if (req_tofree) zfree(pthread_zone_threadreq, req_tofree);
		return WQ_RUN_TR_THREAD_STARTED;
	}

	assert ((tl->th_flags & TH_LIST_PACING) == 0);
	if (locked) {
		workqueue_unlock(wq);
	}
	if (req_tofree) zfree(pthread_zone_threadreq, req_tofree);
	if (unbinding) {
		return WQ_RUN_TR_THREAD_STARTED;
	}
	_setup_wqthread(p, tl->th_thread, wq, tl, WQ_SETUP_CLEAR_VOUCHER);
	pthread_kern->unix_syscall_return(EJUSTRETURN);
	__builtin_unreachable();
}

/*
 * Mark a thread request as cancelled.  Has similar ownership semantics to the
 * complete call above.
 */
static void
_threadreq_cancel(struct workqueue *wq, struct threadreq *req)
{
	assert(req->tr_state == TR_STATE_WAITING);
	req->tr_state = TR_STATE_DEAD;

	assert((req->tr_flags & TR_FLAG_ONSTACK) == 0);
	if (req->tr_flags & TR_FLAG_WORKLOOP) {
		__assert_only int ret;
		ret = pthread_kern->workloop_fulfill_threadreq(wq->wq_proc, (void*)req,
				THREAD_NULL, WORKLOOP_FULFILL_THREADREQ_CANCEL);
		assert(ret == 0 || ret == ECANCELED);
	} else if (req != &wq->wq_event_manager_threadreq) {
		zfree(pthread_zone_threadreq, req);
	}
}

#pragma mark workqueue lock

static boolean_t workqueue_lock_spin_is_acquired_kdp(struct workqueue *wq) {
  return kdp_lck_spin_is_acquired(&wq->wq_lock);
}

static void
workqueue_lock_spin(struct workqueue *wq)
{
	assert(ml_get_interrupts_enabled() == TRUE);
	lck_spin_lock(&wq->wq_lock);
}

static bool
workqueue_lock_try(struct workqueue *wq)
{
	return lck_spin_try_lock(&wq->wq_lock);
}

static void
workqueue_unlock(struct workqueue *wq)
{
	lck_spin_unlock(&wq->wq_lock);
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

	PTHREAD_TRACE_WQ(TRACE_wq_start_add_timer, wq, wq->wq_reqcount,
			_wq_flags(wq), wq->wq_timer_interval, 0);

	thread_call_t call = wq->wq_atimer_delayed_call;
	if (thread_call_enter1_delayed(call, call, deadline)) {
		panic("delayed_call was already enqueued");
	}
}

/**
 * Immediately trigger the workqueue_add_timer
 */
static void
workqueue_interval_timer_trigger(struct workqueue *wq)
{
	PTHREAD_TRACE_WQ(TRACE_wq_start_add_timer, wq, wq->wq_reqcount,
			_wq_flags(wq), 0, 0);

	thread_call_t call = wq->wq_atimer_immediate_call;
	if (thread_call_enter1(call, call)) {
		panic("immediate_call was already enqueued");
	}
}

/**
 * returns whether lastblocked_tsp is within wq_stalled_window_usecs of cur_ts
 */
static boolean_t
wq_thread_is_busy(uint64_t cur_ts, _Atomic uint64_t *lastblocked_tsp)
{
	clock_sec_t	secs;
	clock_usec_t	usecs;
	uint64_t lastblocked_ts;
	uint64_t elapsed;

	lastblocked_ts = atomic_load_explicit(lastblocked_tsp, memory_order_relaxed);
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

	return (secs == 0 && usecs < wq_stalled_window_usecs);
}

/**
 * handler function for the timer
 */
static void
workqueue_add_timer(struct workqueue *wq, thread_call_t thread_call_self)
{
	proc_t p = wq->wq_proc;

	workqueue_lock_spin(wq);

	PTHREAD_TRACE_WQ(TRACE_wq_add_timer | DBG_FUNC_START, wq,
			_wq_flags(wq), wq->wq_nthreads, wq->wq_thidlecount, 0);

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
	/*
	 * Prevent _workqueue_mark_exiting() from going away
	 */
	wq->wq_lflags |= WQL_ATIMER_BUSY;

	/*
	 * Decide which timer we are and remove the RUNNING flag.
	 */
	if (thread_call_self == wq->wq_atimer_delayed_call) {
		uint64_t wq_flags = _wq_flags_and_orig(wq, ~WQ_ATIMER_DELAYED_RUNNING);
		if ((wq_flags & WQ_ATIMER_DELAYED_RUNNING) == 0) {
			panic("workqueue_add_timer(delayed) w/o WQ_ATIMER_DELAYED_RUNNING");
		}
	} else if (thread_call_self == wq->wq_atimer_immediate_call) {
		uint64_t wq_flags = _wq_flags_and_orig(wq, ~WQ_ATIMER_IMMEDIATE_RUNNING);
		if ((wq_flags & WQ_ATIMER_IMMEDIATE_RUNNING) == 0) {
			panic("workqueue_add_timer(immediate) w/o WQ_ATIMER_IMMEDIATE_RUNNING");
		}
	} else {
		panic("workqueue_add_timer can't figure out which timer it is");
	}

	int ret = WQ_RUN_TR_THREAD_STARTED;
	while (ret == WQ_RUN_TR_THREAD_STARTED && wq->wq_reqcount) {
		ret = workqueue_run_threadreq_and_unlock(p, wq, NULL, NULL, true);

		workqueue_lock_spin(wq);
	}
	_threadreq_copy_prepare(wq);

	/*
	 * If we called WQ_TIMER_NEEDED above, then this flag will be set if that
	 * call marked the timer running.  If so, we let the timer interval grow.
	 * Otherwise, we reset it back to 0.
	 */
	uint32_t wq_flags = _wq_flags(wq);
	if (!(wq_flags & WQ_ATIMER_DELAYED_RUNNING)) {
		wq->wq_timer_interval = 0;
	}

	wq->wq_lflags &= ~WQL_ATIMER_BUSY;

	if ((wq_flags & WQ_EXITING) || (wq->wq_lflags & WQL_ATIMER_WAITING)) {
		/*
		 * wakeup the thread hung up in _workqueue_mark_exiting or
		 * workqueue_add_timer waiting for this timer to finish getting out of
		 * the way
		 */
		wq->wq_lflags &= ~WQL_ATIMER_WAITING;
		wakeup(wq);
	}

	PTHREAD_TRACE_WQ(TRACE_wq_add_timer | DBG_FUNC_END, wq, 0, wq->wq_nthreads, wq->wq_thidlecount, 0);

	workqueue_unlock(wq);
}

#pragma mark thread state tracking

// called by spinlock code when trying to yield to lock owner
void
_workqueue_thread_yielded(void)
{
}

static void
workqueue_callback(int type, thread_t thread)
{
	struct uthread *uth = pthread_kern->get_bsdthread_info(thread);
	struct threadlist *tl = pthread_kern->uthread_get_threadlist(uth);
	struct workqueue *wq = tl->th_workq;
	uint32_t old_count, req_qos, qos = tl->th_priority;
	wq_thactive_t old_thactive;

	switch (type) {
	case SCHED_CALL_BLOCK: {
		bool start_timer = false;

		old_thactive = _wq_thactive_dec(wq, tl->th_priority);
		req_qos = WQ_THACTIVE_BEST_CONSTRAINED_REQ_QOS(old_thactive);
		old_count = _wq_thactive_aggregate_downto_qos(wq, old_thactive,
				qos, NULL, NULL);

		if (old_count == wq_max_concurrency[tl->th_priority]) {
			/*
			 * The number of active threads at this priority has fallen below
			 * the maximum number of concurrent threads that are allowed to run
			 *
			 * if we collide with another thread trying to update the
			 * last_blocked (really unlikely since another thread would have to
			 * get scheduled and then block after we start down this path), it's
			 * not a problem.  Either timestamp is adequate, so no need to retry
			 */
			atomic_store_explicit(&wq->wq_lastblocked_ts[qos],
					mach_absolute_time(), memory_order_relaxed);
		}

		if (req_qos == WORKQUEUE_EVENT_MANAGER_BUCKET || qos > req_qos) {
			/*
			 * The blocking thread is at a lower QoS than the highest currently
			 * pending constrained request, nothing has to be redriven
			 */
		} else {
			uint32_t max_busycount, old_req_count;
			old_req_count = _wq_thactive_aggregate_downto_qos(wq, old_thactive,
					req_qos, NULL, &max_busycount);
			/*
			 * If it is possible that may_start_constrained_thread had refused
			 * admission due to being over the max concurrency, we may need to
			 * spin up a new thread.
			 *
			 * We take into account the maximum number of busy threads
			 * that can affect may_start_constrained_thread as looking at the
			 * actual number may_start_constrained_thread will see is racy.
			 *
			 * IOW at NCPU = 4, for IN (req_qos = 1), if the old req count is
			 * between NCPU (4) and NCPU - 2 (2) we need to redrive.
			 */
			if (wq_max_concurrency[req_qos] <= old_req_count + max_busycount &&
					old_req_count <= wq_max_concurrency[req_qos]) {
				if (WQ_TIMER_DELAYED_NEEDED(wq)) {
					start_timer = true;
					workqueue_interval_timer_start(wq);
				}
			}
		}

		PTHREAD_TRACE_WQ(TRACE_wq_thread_block | DBG_FUNC_START, wq,
				old_count - 1, qos | (req_qos << 8),
				wq->wq_reqcount << 1 | start_timer, 0);
		break;
	}
	case SCHED_CALL_UNBLOCK: {
		/*
		 * we cannot take the workqueue_lock here...
		 * an UNBLOCK can occur from a timer event which
		 * is run from an interrupt context... if the workqueue_lock
		 * is already held by this processor, we'll deadlock...
		 * the thread lock for the thread being UNBLOCKED
		 * is also held
		 */
		old_thactive = _wq_thactive_inc(wq, qos);
		if (pthread_debug_tracing) {
			req_qos = WQ_THACTIVE_BEST_CONSTRAINED_REQ_QOS(old_thactive);
			old_count = _wq_thactive_aggregate_downto_qos(wq, old_thactive,
					qos, NULL, NULL);
			PTHREAD_TRACE_WQ(TRACE_wq_thread_block | DBG_FUNC_END, wq,
					old_count + 1, qos | (req_qos << 8),
					wq->wq_threads_scheduled, 0);
		}
		break;
	}
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
	}
	/*
	 * drop our ref on the thread
	 */
	thread_deallocate(tl->th_thread);

	zfree(pthread_zone_threadlist, tl);
}


/**
 * Try to add a new workqueue thread.
 *
 * - called with workq lock held
 * - dropped and retaken around thread creation
 * - return with workq lock held
 */
static bool
workqueue_addnewthread(proc_t p, struct workqueue *wq)
{
	kern_return_t kret;

	wq->wq_nthreads++;

	workqueue_unlock(wq);

	struct threadlist *tl = zalloc(pthread_zone_threadlist);
	bzero(tl, sizeof(struct threadlist));

	thread_t th;
	kret = pthread_kern->thread_create_workq_waiting(wq->wq_task, wq_unpark_continue, tl, &th);
 	if (kret != KERN_SUCCESS) {
		PTHREAD_TRACE_WQ(TRACE_wq_thread_create_failed | DBG_FUNC_NONE, wq, kret, 0, 0, 0);
		goto fail_free;
	}

	mach_vm_offset_t stackaddr = pthread_kern->proc_get_stack_addr_hint(p);

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
		kret = mach_vm_allocate(wq->wq_map,
				&stackaddr, th_allocsize,
				VM_MAKE_TAG(VM_MEMORY_STACK) | VM_FLAGS_ANYWHERE);
	}

	if (kret != KERN_SUCCESS) {
		PTHREAD_TRACE_WQ(TRACE_wq_thread_create_failed | DBG_FUNC_NONE, wq, kret, 1, 0, 0);
		goto fail_terminate;
	}

	/*
	 * The guard page is at the lowest address
	 * The stack base is the highest address
	 */
	kret = mach_vm_protect(wq->wq_map, stackaddr, guardsize, FALSE, VM_PROT_NONE);
	if (kret != KERN_SUCCESS) {
		PTHREAD_TRACE_WQ(TRACE_wq_thread_create_failed | DBG_FUNC_NONE, wq, kret, 2, 0, 0);
		goto fail_vm_deallocate;
	}


	pthread_kern->thread_set_tag(th, THREAD_TAG_PTHREAD | THREAD_TAG_WORKQUEUE);
	pthread_kern->thread_static_param(th, TRUE);

	/*
	 * convert_thread_to_port() consumes a reference
	 */
	thread_reference(th);
	void *sright = (void *)pthread_kern->convert_thread_to_port(th);
	tl->th_thport = pthread_kern->ipc_port_copyout_send(sright,
			pthread_kern->task_get_ipcspace(wq->wq_task));

	tl->th_flags = TH_LIST_INITED | TH_LIST_NEW;
	tl->th_thread = th;
	tl->th_workq = wq;
	tl->th_stackaddr = stackaddr;
	tl->th_priority = WORKQUEUE_NUM_BUCKETS;

	struct uthread *uth;
	uth = pthread_kern->get_bsdthread_info(tl->th_thread);

	workqueue_lock_spin(wq);

	void *current_tl = pthread_kern->uthread_get_threadlist(uth);
	if (current_tl == NULL) {
		pthread_kern->uthread_set_threadlist(uth, tl);
		TAILQ_INSERT_TAIL(&wq->wq_thidlelist, tl, th_entry);
		wq->wq_thidlecount++;
	} else if (current_tl == WQ_THREADLIST_EXITING_POISON) {
		/*
		 * Failed thread creation race: The thread already woke up and has exited.
		 */
		PTHREAD_TRACE_WQ(TRACE_wq_thread_create_failed | DBG_FUNC_NONE, wq, kret, 3, 0, 0);
		goto fail_unlock;
	} else {
		panic("Unexpected initial threadlist value");
	}

	PTHREAD_TRACE_WQ(TRACE_wq_thread_create | DBG_FUNC_NONE, wq, 0, 0, 0, 0);

	return (TRUE);

fail_unlock:
	workqueue_unlock(wq);
	(void)pthread_kern->mach_port_deallocate(pthread_kern->task_get_ipcspace(wq->wq_task),
			tl->th_thport);

fail_vm_deallocate:
	(void) mach_vm_deallocate(wq->wq_map, stackaddr, th_allocsize);

fail_terminate:
	if (pthread_kern->thread_will_park_or_terminate) {
		pthread_kern->thread_will_park_or_terminate(th);
	}
	(void)thread_terminate(th);
	thread_deallocate(th);

fail_free:
	zfree(pthread_zone_threadlist, tl);

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
	char * ptr;
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

		if (wq_max_threads > WQ_THACTIVE_BUCKET_HALF) {
			wq_max_threads = WQ_THACTIVE_BUCKET_HALF;
		}
		if (wq_max_threads > pthread_kern->config_thread_max - 20) {
			wq_max_threads = pthread_kern->config_thread_max - 20;
		}
	}

	if (pthread_kern->proc_get_wqptr(p) == NULL) {
		if (pthread_kern->proc_init_wqptr_or_wait(p) == FALSE) {
			assert(pthread_kern->proc_get_wqptr(p) != NULL);
			goto out;
		}

		ptr = (char *)zalloc(pthread_zone_workqueue);
		bzero(ptr, sizeof(struct workqueue));

		wq = (struct workqueue *)ptr;
		wq->wq_proc = p;
		wq->wq_task = current_task();
		wq->wq_map  = pthread_kern->current_map();

		// Start the event manager at the priority hinted at by the policy engine
		int mgr_priority_hint = pthread_kern->task_get_default_manager_qos(current_task());
		wq->wq_event_manager_priority = (uint32_t)thread_qos_get_pthread_priority(mgr_priority_hint) | _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;

		TAILQ_INIT(&wq->wq_thrunlist);
		TAILQ_INIT(&wq->wq_thidlelist);
		for (int i = 0; i < WORKQUEUE_EVENT_MANAGER_BUCKET; i++) {
			TAILQ_INIT(&wq->wq_overcommit_reqlist[i]);
			TAILQ_INIT(&wq->wq_reqlist[i]);
		}

		wq->wq_atimer_delayed_call =
				thread_call_allocate_with_priority((thread_call_func_t)workqueue_add_timer,
						(thread_call_param_t)wq, THREAD_CALL_PRIORITY_KERNEL);
		wq->wq_atimer_immediate_call =
				thread_call_allocate_with_priority((thread_call_func_t)workqueue_add_timer,
						(thread_call_param_t)wq, THREAD_CALL_PRIORITY_KERNEL);

		lck_spin_init(&wq->wq_lock, pthread_lck_grp, pthread_lck_attr);

		wq->wq_cached_threadreq = zalloc(pthread_zone_threadreq);
		*(wq_thactive_t *)&wq->wq_thactive =
				(wq_thactive_t)WQ_THACTIVE_NO_PENDING_REQUEST <<
				WQ_THACTIVE_QOS_SHIFT;

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
	if (!wq) return;

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
	uint64_t wq_flags = _wq_flags_or_orig(wq, WQ_EXITING);

	if (wq_flags & WQ_ATIMER_DELAYED_RUNNING) {
		if (thread_call_cancel(wq->wq_atimer_delayed_call) == TRUE) {
			wq_flags = _wq_flags_and_orig(wq, ~WQ_ATIMER_DELAYED_RUNNING);
		}
	}
	if (wq_flags & WQ_ATIMER_IMMEDIATE_RUNNING) {
		if (thread_call_cancel(wq->wq_atimer_immediate_call) == TRUE) {
			wq_flags = _wq_flags_and_orig(wq, ~WQ_ATIMER_IMMEDIATE_RUNNING);
		}
	}
	while ((_wq_flags(wq) & (WQ_ATIMER_DELAYED_RUNNING | WQ_ATIMER_IMMEDIATE_RUNNING)) ||
			(wq->wq_lflags & WQL_ATIMER_BUSY)) {
		assert_wait((caddr_t)wq, (THREAD_UNINT));
		workqueue_unlock(wq);

		thread_block(THREAD_CONTINUE_NULL);

		workqueue_lock_spin(wq);
	}

	/*
	 * Save off pending requests, will complete/free them below after unlocking
	 */
	TAILQ_HEAD(, threadreq) local_list = TAILQ_HEAD_INITIALIZER(local_list);

	for (int i = 0; i < WORKQUEUE_EVENT_MANAGER_BUCKET; i++) {
		TAILQ_CONCAT(&local_list, &wq->wq_overcommit_reqlist[i], tr_entry);
		TAILQ_CONCAT(&local_list, &wq->wq_reqlist[i], tr_entry);
	}

	/*
	 * XXX: Can't deferred cancel the event manager request, so just smash it.
	 */
	assert((wq->wq_event_manager_threadreq.tr_flags & TR_FLAG_WORKLOOP) == 0);
	wq->wq_event_manager_threadreq.tr_state = TR_STATE_DEAD;

	workqueue_unlock(wq);

	struct threadreq *tr, *tr_temp;
	TAILQ_FOREACH_SAFE(tr, &local_list, tr_entry, tr_temp) {
		_threadreq_cancel(wq, tr);
	}
	PTHREAD_TRACE(TRACE_wq_pthread_exit|DBG_FUNC_END, 0, 0, 0, 0, 0);
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

			zfree(pthread_zone_threadlist, tl);
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
		if (wq->wq_cached_threadreq) {
			zfree(pthread_zone_threadreq, wq->wq_cached_threadreq);
		}
		thread_call_free(wq->wq_atimer_delayed_call);
		thread_call_free(wq->wq_atimer_immediate_call);
		lck_spin_destroy(&wq->wq_lock, pthread_lck_grp);

		for (int i = 0; i < WORKQUEUE_EVENT_MANAGER_BUCKET; i++) {
			assert(TAILQ_EMPTY(&wq->wq_overcommit_reqlist[i]));
			assert(TAILQ_EMPTY(&wq->wq_reqlist[i]));
		}

		zfree(pthread_zone_workqueue, wq);

		PTHREAD_TRACE(TRACE_wq_workqueue_exit|DBG_FUNC_END, 0, 0, 0, 0, 0);
	}
}


#pragma mark workqueue thread manipulation


/**
 * Entry point for libdispatch to ask for threads
 */
static int
wqops_queue_reqthreads(struct proc *p, int reqcount,
		pthread_priority_t priority)
{
	bool overcommit = _pthread_priority_get_flags(priority) & _PTHREAD_PRIORITY_OVERCOMMIT_FLAG;
	bool event_manager = _pthread_priority_get_flags(priority) & _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;
	int class = event_manager ? WORKQUEUE_EVENT_MANAGER_BUCKET :
			pthread_priority_get_class_index(priority);

	if ((reqcount <= 0) || (class < 0) || (class >= WORKQUEUE_NUM_BUCKETS) ||
			(overcommit && event_manager)) {
		return EINVAL;
	}

	struct workqueue *wq;
	if ((wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p)) == NULL) {
		return EINVAL;
	}

	workqueue_lock_spin(wq);
	_threadreq_copy_prepare(wq);

	PTHREAD_TRACE_WQ(TRACE_wq_wqops_reqthreads | DBG_FUNC_NONE, wq, reqcount, priority, 0, 0);

	int tr_flags = 0;
	if (overcommit) tr_flags |= TR_FLAG_OVERCOMMIT;
	if (reqcount > 1) {
		/*
		 * when libdispatch asks for more than one thread, it wants to achieve
		 * parallelism. Pacing would be detrimental to this ask, so treat
		 * these specially to not do the pacing admission check
		 */
		tr_flags |= TR_FLAG_NO_PACING;
	}

	while (reqcount-- && !_wq_exiting(wq)) {
		struct threadreq req;
		_threadreq_init_stack(&req, class, tr_flags);

		workqueue_run_threadreq_and_unlock(p, wq, NULL, &req, true);

		workqueue_lock_spin(wq); /* reacquire */
		_threadreq_copy_prepare(wq);
	}

	workqueue_unlock(wq);

	return 0;
}

/*
 * Used by the kevent system to request threads.
 *
 * Currently count is ignored and we always return one thread per invocation.
 */
static thread_t
_workq_kevent_reqthreads(struct proc *p, pthread_priority_t priority,
		bool no_emergency)
{
	int wq_run_tr = WQ_RUN_TR_THROTTLED;
	bool emergency_thread = false;
	struct threadreq req;


	struct workqueue *wq;
	if ((wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p)) == NULL) {
		return THREAD_NULL;
	}

	int class = pthread_priority_get_class_index(priority);

	workqueue_lock_spin(wq);
	bool has_threadreq = _threadreq_copy_prepare_noblock(wq);

	PTHREAD_TRACE_WQ_REQ(TRACE_wq_kevent_reqthreads | DBG_FUNC_NONE, wq, NULL, priority, 0, 0);

	/*
	 * Skip straight to event manager if that's what was requested
	 */
	if ((_pthread_priority_get_qos_newest(priority) == QOS_CLASS_UNSPECIFIED) ||
			(_pthread_priority_get_flags(priority) & _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG)){
		goto event_manager;
	}

	bool will_pace = _wq_should_pace_priority(wq, class);
	if ((wq->wq_thidlecount == 0 || will_pace) && has_threadreq == false) {
		/*
		 * We'll need to persist the request and can't, so return the emergency
		 * thread instead, which has a persistent request object.
		 */
		emergency_thread = true;
		goto event_manager;
	}

	/*
	 * Handle overcommit requests
	 */
	if ((_pthread_priority_get_flags(priority) & _PTHREAD_PRIORITY_OVERCOMMIT_FLAG) != 0){
		_threadreq_init_stack(&req, class, TR_FLAG_KEVENT | TR_FLAG_OVERCOMMIT);
		wq_run_tr = workqueue_run_threadreq_and_unlock(p, wq, NULL, &req, false);
		goto done;
	}

	/*
	 * Handle constrained requests
	 */
	boolean_t may_start = may_start_constrained_thread(wq, class, NULL, false);
	if (may_start || no_emergency) {
		_threadreq_init_stack(&req, class, TR_FLAG_KEVENT);
		wq_run_tr = workqueue_run_threadreq_and_unlock(p, wq, NULL, &req, false);
		goto done;
	} else {
		emergency_thread = true;
	}


event_manager:
	_threadreq_init_stack(&req, WORKQUEUE_EVENT_MANAGER_BUCKET, TR_FLAG_KEVENT);
	wq_run_tr = workqueue_run_threadreq_and_unlock(p, wq, NULL, &req, false);

done:
	if (wq_run_tr == WQ_RUN_TR_THREAD_NEEDED && WQ_TIMER_IMMEDIATE_NEEDED(wq)) {
		workqueue_interval_timer_trigger(wq);
	}
	return emergency_thread ? (void*)-1 : 0;
}

thread_t
_workq_reqthreads(struct proc *p, __assert_only int requests_count,
		workq_reqthreads_req_t request)
{
	assert(requests_count == 1);

	pthread_priority_t priority = request->priority;
	bool no_emergency = request->count & WORKQ_REQTHREADS_NOEMERGENCY;

	return _workq_kevent_reqthreads(p, priority, no_emergency);
}


int
workq_kern_threadreq(struct proc *p, workq_threadreq_t _req,
		enum workq_threadreq_type type, unsigned long priority, int flags)
{
	struct workqueue *wq;
	int ret;

	if ((wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p)) == NULL) {
		return EINVAL;
	}

	switch (type) {
	case WORKQ_THREADREQ_KEVENT: {
		bool no_emergency = flags & WORKQ_THREADREQ_FLAG_NOEMERGENCY;
		(void)_workq_kevent_reqthreads(p, priority, no_emergency);
		return 0;
	}
	case WORKQ_THREADREQ_WORKLOOP:
	case WORKQ_THREADREQ_WORKLOOP_NO_THREAD_CALL: {
		struct threadreq *req = (struct threadreq *)_req;
		int req_class = pthread_priority_get_class_index(priority);
		int req_flags = TR_FLAG_WORKLOOP;
		if ((_pthread_priority_get_flags(priority) &
				_PTHREAD_PRIORITY_OVERCOMMIT_FLAG) != 0){
			req_flags |= TR_FLAG_OVERCOMMIT;
		}

		thread_t thread = current_thread();
		struct threadlist *tl = util_get_thread_threadlist_entry(thread);

		if (tl && tl != WQ_THREADLIST_EXITING_POISON &&
				(tl->th_flags & TH_LIST_UNBINDING)) {
			/*
			 * we're called back synchronously from the context of
			 * kevent_qos_internal_unbind from within wqops_thread_return()
			 * we can try to match up this thread with this request !
			 */
		} else {
			tl = NULL;
		}

		_threadreq_init_alloced(req, req_class, req_flags);
		workqueue_lock_spin(wq);
		PTHREAD_TRACE_WQ_REQ(TRACE_wq_kevent_reqthreads | DBG_FUNC_NONE, wq, req, priority, 1, 0);
		ret = workqueue_run_threadreq_and_unlock(p, wq, tl, req, false);
		if (ret == WQ_RUN_TR_EXITING) {
			return ECANCELED;
		}
		if (ret == WQ_RUN_TR_THREAD_NEEDED) {
			if (type == WORKQ_THREADREQ_WORKLOOP_NO_THREAD_CALL) {
				return EAGAIN;
			}
			if (WQ_TIMER_IMMEDIATE_NEEDED(wq)) {
				workqueue_interval_timer_trigger(wq);
			}
		}
		return 0;
	}
	case WORKQ_THREADREQ_REDRIVE:
		PTHREAD_TRACE_WQ_REQ(TRACE_wq_kevent_reqthreads | DBG_FUNC_NONE, wq, 0, 0, 4, 0);
		workqueue_lock_spin(wq);
		ret = workqueue_run_threadreq_and_unlock(p, wq, NULL, NULL, true);
		if (ret == WQ_RUN_TR_EXITING) {
			return ECANCELED;
		}
		return 0;
	default:
		return ENOTSUP;
	}
}

int
workq_kern_threadreq_modify(struct proc *p, workq_threadreq_t _req,
		enum workq_threadreq_op operation, unsigned long arg1,
		unsigned long __unused arg2)
{
	struct threadreq *req = (struct threadreq *)_req;
	struct workqueue *wq;
	int priclass, ret = 0, wq_tr_rc = WQ_RUN_TR_THROTTLED;

	if (req == NULL || (wq = pthread_kern->proc_get_wqptr(p)) == NULL) {
		return EINVAL;
	}

	workqueue_lock_spin(wq);

	if (_wq_exiting(wq)) {
		ret = ECANCELED;
		goto out_unlock;
	}

	/*
	 * Find/validate the referenced request structure
	 */
	if (req->tr_state != TR_STATE_WAITING) {
		ret = EINVAL;
		goto out_unlock;
	}
	assert(req->tr_priority < WORKQUEUE_EVENT_MANAGER_BUCKET);
	assert(req->tr_flags & TR_FLAG_WORKLOOP);

	switch (operation) {
	case WORKQ_THREADREQ_CHANGE_PRI:
	case WORKQ_THREADREQ_CHANGE_PRI_NO_THREAD_CALL:
		priclass = pthread_priority_get_class_index(arg1);
		PTHREAD_TRACE_WQ_REQ(TRACE_wq_kevent_reqthreads | DBG_FUNC_NONE, wq, req, arg1, 2, 0);
		if (req->tr_priority == priclass) {
			goto out_unlock;
		}
		_threadreq_dequeue(wq, req);
		req->tr_priority = priclass;
		req->tr_state = TR_STATE_NEW; // what was old is new again
		wq_tr_rc = workqueue_run_threadreq_and_unlock(p, wq, NULL, req, false);
		goto out;

	case WORKQ_THREADREQ_CANCEL:
		PTHREAD_TRACE_WQ_REQ(TRACE_wq_kevent_reqthreads | DBG_FUNC_NONE, wq, req, 0, 3, 0);
		_threadreq_dequeue(wq, req);
		req->tr_state = TR_STATE_DEAD;
		break;

	default:
		ret = ENOTSUP;
		break;
	}

out_unlock:
	workqueue_unlock(wq);
out:
	if (wq_tr_rc == WQ_RUN_TR_THREAD_NEEDED) {
		if (operation == WORKQ_THREADREQ_CHANGE_PRI_NO_THREAD_CALL) {
			ret = EAGAIN;
		} else if (WQ_TIMER_IMMEDIATE_NEEDED(wq)) {
			workqueue_interval_timer_trigger(wq);
		}
	}
	return ret;
}


static int
wqops_thread_return(struct proc *p, struct workqueue *wq)
{
	thread_t th = current_thread();
	struct uthread *uth = pthread_kern->get_bsdthread_info(th);
	struct threadlist *tl = pthread_kern->uthread_get_threadlist(uth);

	/* reset signal mask on the workqueue thread to default state */
	if (pthread_kern->uthread_get_sigmask(uth) != (sigset_t)(~workq_threadmask)) {
		pthread_kern->proc_lock(p);
		pthread_kern->uthread_set_sigmask(uth, ~workq_threadmask);
		pthread_kern->proc_unlock(p);
	}

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
	bool was_manager = (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET);
	int new_qos;

	if (!was_manager) {
		new_qos = pthread_kern->proc_usynch_thread_qos_squash_override_for_resource(th,
				THREAD_QOS_OVERRIDE_RESOURCE_WILDCARD,
				THREAD_QOS_OVERRIDE_TYPE_DISPATCH_ASYNCHRONOUS_OVERRIDE);
	}

	PTHREAD_TRACE_WQ(TRACE_wq_runitem | DBG_FUNC_END, wq, tl->th_priority, 0, 0, 0);

	workqueue_lock_spin(wq);

	if (tl->th_flags & TH_LIST_KEVENT_BOUND) {
		unsigned int flags = KEVENT_FLAG_WORKQ;
		if (was_manager) {
			flags |= KEVENT_FLAG_WORKQ_MANAGER;
		}

		tl->th_flags |= TH_LIST_UNBINDING;
		workqueue_unlock(wq);
		kevent_qos_internal_unbind(p, class_index_get_thread_qos(tl->th_priority), th, flags);
		if (!(tl->th_flags & TH_LIST_UNBINDING)) {
			_setup_wqthread(p, th, wq, tl, WQ_SETUP_CLEAR_VOUCHER);
			pthread_kern->unix_syscall_return(EJUSTRETURN);
			__builtin_unreachable();
		}
		workqueue_lock_spin(wq);
		tl->th_flags &= ~(TH_LIST_KEVENT_BOUND | TH_LIST_UNBINDING);
	}

	if (!was_manager) {
		/* Fix up counters from the squash operation. */
		uint8_t old_bucket = tl->th_priority;
		uint8_t new_bucket = thread_qos_get_class_index(new_qos);

		if (old_bucket != new_bucket) {
			_wq_thactive_move(wq, old_bucket, new_bucket);
			wq->wq_thscheduled_count[old_bucket]--;
			wq->wq_thscheduled_count[new_bucket]++;

			PTHREAD_TRACE_WQ(TRACE_wq_thread_squash | DBG_FUNC_NONE, wq, tl->th_priority, new_bucket, 0, 0);
			tl->th_priority = new_bucket;
			PTHREAD_TRACE_WQ(TRACE_wq_override_reset | DBG_FUNC_END, tl->th_workq, new_qos, 0, 0, 0);
		}
	}

	workqueue_run_threadreq_and_unlock(p, wq, tl, NULL, false);
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
	struct workqueue *wq;
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

		wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p);
		if (wq == NULL) {
			error = EINVAL;
			break;
		}
		workqueue_lock_spin(wq);
		if (pri & _PTHREAD_PRIORITY_SCHED_PRI_FLAG){
			/*
			 * If userspace passes a scheduling priority, that takes precidence
			 * over any QoS.  (So, userspace should take care not to accidenatally
			 * lower the priority this way.)
			 */
			uint32_t sched_pri = pri & _PTHREAD_PRIORITY_SCHED_PRI_MASK;
			if (wq->wq_event_manager_priority & _PTHREAD_PRIORITY_SCHED_PRI_FLAG){
				wq->wq_event_manager_priority = MAX(sched_pri, wq->wq_event_manager_priority & _PTHREAD_PRIORITY_SCHED_PRI_MASK)
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
	case WQOPS_THREAD_WORKLOOP_RETURN:
		wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p);
		PTHREAD_TRACE_WQ(TRACE_wq_runthread | DBG_FUNC_END, wq, options, 0, 0, 0);
		if (item != 0 && arg2 != 0) {
			int32_t kevent_retval;
			int ret;
			if (options == WQOPS_THREAD_KEVENT_RETURN) {
				ret = kevent_qos_internal(p, -1, item, arg2, item, arg2, NULL, NULL,
						KEVENT_FLAG_WORKQ | KEVENT_FLAG_IMMEDIATE | KEVENT_FLAG_ERROR_EVENTS,
						&kevent_retval);
			} else /* options == WQOPS_THREAD_WORKLOOP_RETURN */ {
				kqueue_id_t kevent_id = -1;
				ret = kevent_id_internal(p, &kevent_id, item, arg2, item, arg2,
						NULL, NULL,
						KEVENT_FLAG_WORKLOOP | KEVENT_FLAG_IMMEDIATE | KEVENT_FLAG_ERROR_EVENTS,
						&kevent_retval);
			}
			/*
			 * We shouldn't be getting more errors out than events we put in, so
			 * reusing the input buffer should always provide enough space.  But,
			 * the assert is commented out since we get errors in edge cases in the
			 * process lifecycle.
			 */
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
		goto thread_return;

	case WQOPS_THREAD_RETURN:
		wq = (struct workqueue *)pthread_kern->proc_get_wqptr(p);
		PTHREAD_TRACE_WQ(TRACE_wq_runthread | DBG_FUNC_END, wq, options, 0, 0, 0);
	thread_return:
		error = wqops_thread_return(p, wq);
		// NOT REACHED except in case of error
		assert(error);
		break;

	case WQOPS_SHOULD_NARROW: {
		/*
		 * arg2 = priority to test
		 * arg3 = unused
		 */
		pthread_priority_t priority = arg2;
		thread_t th = current_thread();
		struct threadlist *tl = util_get_thread_threadlist_entry(th);

		if (tl == NULL || (tl->th_flags & TH_LIST_CONSTRAINED) == 0) {
			error = EINVAL;
			break;
		}

		int class = pthread_priority_get_class_index(priority);
		wq = tl->th_workq;
		workqueue_lock_spin(wq);
		bool should_narrow = !may_start_constrained_thread(wq, class, tl, false);
		workqueue_unlock(wq);

		*retval = should_narrow;
		break;
	}
	default:
		error = EINVAL;
		break;
	}

	switch (options) {
	case WQOPS_THREAD_KEVENT_RETURN:
	case WQOPS_THREAD_WORKLOOP_RETURN:
	case WQOPS_THREAD_RETURN:
		PTHREAD_TRACE_WQ(TRACE_wq_runthread | DBG_FUNC_START, wq, options, 0, 0, 0);
		break;
	}
	return (error);
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

	PTHREAD_TRACE_WQ(TRACE_wq_thread_park | DBG_FUNC_START, wq, 0, 0, 0, 0);

	uint32_t us_to_wait = 0;

	TAILQ_REMOVE(&wq->wq_thrunlist, tl, th_entry);

	tl->th_flags &= ~TH_LIST_RUNNING;
	tl->th_flags &= ~TH_LIST_KEVENT;
	assert((tl->th_flags & TH_LIST_KEVENT_BOUND) == 0);

	if (tl->th_flags & TH_LIST_CONSTRAINED) {
		wq->wq_constrained_threads_scheduled--;
		tl->th_flags &= ~TH_LIST_CONSTRAINED;
	}

	_wq_thactive_dec(wq, tl->th_priority);
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
		PTHREAD_TRACE_WQ(TRACE_wq_thread_reset_priority | DBG_FUNC_NONE,
					wq, thread_tid(thread),
					(tl->th_priority << 16) | WORKQUEUE_EVENT_MANAGER_BUCKET, 2, 0);
		reset_priority(tl, pthread_priority_from_wq_class_index(wq, WORKQUEUE_EVENT_MANAGER_BUCKET));
		tl->th_priority = WORKQUEUE_EVENT_MANAGER_BUCKET;
	}

	if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET){
		TAILQ_INSERT_HEAD(&wq->wq_thidlemgrlist, tl, th_entry);
	} else {
		TAILQ_INSERT_HEAD(&wq->wq_thidlelist, tl, th_entry);
	}

	/*
	 * When we remove the voucher from the thread, we may lose our importance
	 * causing us to get preempted, so we do this after putting the thread on
	 * the idle list.  That when, when we get our importance back we'll be able
	 * to use this thread from e.g. the kevent call out to deliver a boosting
	 * message.
	 */
	tl->th_flags |= TH_LIST_REMOVING_VOUCHER;
	workqueue_unlock(wq);
	if (pthread_kern->thread_will_park_or_terminate) {
		pthread_kern->thread_will_park_or_terminate(tl->th_thread);
	}
	__assert_only kern_return_t kr;
	kr = pthread_kern->thread_set_voucher_name(MACH_PORT_NULL);
	assert(kr == KERN_SUCCESS);
	workqueue_lock_spin(wq);
	tl->th_flags &= ~(TH_LIST_REMOVING_VOUCHER);

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

static bool
may_start_constrained_thread(struct workqueue *wq, uint32_t at_priclass,
		struct threadlist *tl, bool may_start_timer)
{
	uint32_t req_qos = _wq_thactive_best_constrained_req_qos(wq);
	wq_thactive_t thactive;

	if (may_start_timer && at_priclass < req_qos) {
		/*
		 * When called from workqueue_run_threadreq_and_unlock() pre-post newest
		 * higher priorities into the thactive state so that
		 * workqueue_callback() takes the right decision.
		 *
		 * If the admission check passes, workqueue_run_threadreq_and_unlock
		 * will reset this value before running the request.
		 */
		thactive = _wq_thactive_set_best_constrained_req_qos(wq, req_qos,
				at_priclass);
#ifdef __LP64__
		PTHREAD_TRACE_WQ(TRACE_wq_thactive_update, 1, (uint64_t)thactive,
				(uint64_t)(thactive >> 64), 0, 0);
#endif
	} else {
		thactive = _wq_thactive(wq);
	}

	uint32_t constrained_threads = wq->wq_constrained_threads_scheduled;
	if (tl && (tl->th_flags & TH_LIST_CONSTRAINED)) {
		/*
		 * don't count the current thread as scheduled
		 */
		constrained_threads--;
	}
	if (constrained_threads >= wq_max_constrained_threads) {
		PTHREAD_TRACE_WQ(TRACE_wq_constrained_admission | DBG_FUNC_NONE, wq, 1,
				wq->wq_constrained_threads_scheduled,
				wq_max_constrained_threads, 0);
		/*
		 * we need 1 or more constrained threads to return to the kernel before
		 * we can dispatch additional work
		 */
		return false;
	}

	/*
	 * Compute a metric for many how many threads are active.  We find the
	 * highest priority request outstanding and then add up the number of
	 * active threads in that and all higher-priority buckets.  We'll also add
	 * any "busy" threads which are not active but blocked recently enough that
	 * we can't be sure they've gone idle yet.  We'll then compare this metric
	 * to our max concurrency to decide whether to add a new thread.
	 */

	uint32_t busycount, thactive_count;

	thactive_count = _wq_thactive_aggregate_downto_qos(wq, thactive,
			at_priclass, &busycount, NULL);

	if (tl && tl->th_priority <= at_priclass) {
		/*
		 * don't count this thread as currently active
		 */
		assert(thactive_count > 0);
		thactive_count--;
	}

	if (thactive_count + busycount < wq_max_concurrency[at_priclass]) {
		PTHREAD_TRACE_WQ(TRACE_wq_constrained_admission | DBG_FUNC_NONE, wq, 2,
				thactive_count, busycount, 0);
		return true;
	} else {
		PTHREAD_TRACE_WQ(TRACE_wq_constrained_admission | DBG_FUNC_NONE, wq, 3,
				thactive_count, busycount, 0);
	}

	if (busycount && may_start_timer) {
		/*
		 * If this is called from the add timer, we won't have another timer
		 * fire when the thread exits the "busy" state, so rearm the timer.
		 */
		if (WQ_TIMER_DELAYED_NEEDED(wq)) {
			workqueue_interval_timer_start(wq);
		}
	}

	return false;
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
	_wq_thactive_inc(wq, priclass);
	return tl;
}

static pthread_priority_t
pthread_priority_from_wq_class_index(struct workqueue *wq, int index)
{
	if (index == WORKQUEUE_EVENT_MANAGER_BUCKET){
		return wq->wq_event_manager_priority;
	} else {
		return class_index_get_pthread_priority(index);
	}
}

static void
reset_priority(struct threadlist *tl, pthread_priority_t pri)
{
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

/*
 * Picks the best request to run, and returns the best overcommit fallback
 * if the best pick is non overcommit and risks failing its admission check.
 */
static struct threadreq *
workqueue_best_threadreqs(struct workqueue *wq, struct threadlist *tl,
		struct threadreq **fallback)
{
	struct threadreq *req, *best_req = NULL;
	int priclass, prilimit;

	if ((wq->wq_event_manager_threadreq.tr_state == TR_STATE_WAITING) &&
			((wq->wq_thscheduled_count[WORKQUEUE_EVENT_MANAGER_BUCKET] == 0) ||
			(tl && tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET))) {
		/*
		 * There's an event manager request and either:
		 *   - no event manager currently running
		 *   - we are re-using the event manager
		 */
		req = &wq->wq_event_manager_threadreq;
		PTHREAD_TRACE_WQ_REQ(TRACE_wq_run_threadreq_req_select | DBG_FUNC_NONE, wq, req, 1, 0, 0);
		return req;
	}

	if (tl) {
		prilimit = WORKQUEUE_EVENT_MANAGER_BUCKET;
	} else {
		prilimit = _wq_highest_paced_priority(wq);
	}
	for (priclass = 0; priclass < prilimit; priclass++) {
		req = TAILQ_FIRST(&wq->wq_overcommit_reqlist[priclass]);
		if (req) {
			PTHREAD_TRACE_WQ_REQ(TRACE_wq_run_threadreq_req_select | DBG_FUNC_NONE, wq, req, 2, 0, 0);
			if (best_req) {
				*fallback = req;
			} else {
				best_req = req;
			}
			break;
		}
		if (!best_req) {
			best_req = TAILQ_FIRST(&wq->wq_reqlist[priclass]);
			if (best_req) {
				PTHREAD_TRACE_WQ_REQ(TRACE_wq_run_threadreq_req_select | DBG_FUNC_NONE, wq, best_req, 3, 0, 0);
			}
		}
	}
	return best_req;
}

/**
 * Runs a thread request on a thread
 *
 * - if thread is THREAD_NULL, will find a thread and run the request there.
 *   Otherwise, the thread must be the current thread.
 *
 * - if req is NULL, will find the highest priority request and run that.  If
 *   it is not NULL, it must be a threadreq object in state NEW.  If it can not
 *   be run immediately, it will be enqueued and moved to state WAITING.
 *
 *   Either way, the thread request object serviced will be moved to state
 *   PENDING and attached to the threadlist.
 *
 *   Should be called with the workqueue lock held.  Will drop it.
 *
 *   WARNING: _workq_kevent_reqthreads needs to be able to preflight any
 *   admission checks in this function.  If you are changing this function,
 *   keep that one up-to-date.
 *
 * - if parking_tl is non NULL, then the current thread is parking. This will
 *   try to reuse this thread for a request. If no match is found, it will be
 *   parked.
 */
static int
workqueue_run_threadreq_and_unlock(proc_t p, struct workqueue *wq,
		struct threadlist *parking_tl, struct threadreq *req,
		bool may_add_new_thread)
{
	struct threadreq *incoming_req = req;

	struct threadlist *tl = parking_tl;
	int rc = WQ_RUN_TR_THROTTLED;

	assert(tl == NULL || tl->th_thread == current_thread());
	assert(req == NULL || req->tr_state == TR_STATE_NEW);
	assert(!may_add_new_thread || !tl);

	PTHREAD_TRACE_WQ_REQ(TRACE_wq_run_threadreq | DBG_FUNC_START, wq, req,
			tl ? thread_tid(tl->th_thread) : 0,
			req ? (req->tr_priority << 16 | req->tr_flags) : 0, 0);

	/*
	 * Special cases when provided an event manager request
	 */
	if (req && req->tr_priority == WORKQUEUE_EVENT_MANAGER_BUCKET) {
		// Clients must not rely on identity of event manager requests
		assert(req->tr_flags & TR_FLAG_ONSTACK);
		// You can't be both overcommit and event manager
		assert((req->tr_flags & TR_FLAG_OVERCOMMIT) == 0);

		/*
		 * We can only ever have one event manager request, so coalesce them if
		 * there's already one outstanding.
		 */
		if (wq->wq_event_manager_threadreq.tr_state == TR_STATE_WAITING) {
			PTHREAD_TRACE_WQ_REQ(TRACE_wq_run_threadreq_mgr_merge | DBG_FUNC_NONE, wq, req, 0, 0, 0);

			struct threadreq *existing_req = &wq->wq_event_manager_threadreq;
			if (req->tr_flags & TR_FLAG_KEVENT) {
				existing_req->tr_flags |= TR_FLAG_KEVENT;
			}

			req = existing_req;
			incoming_req = NULL;
		}

		if (wq->wq_thscheduled_count[WORKQUEUE_EVENT_MANAGER_BUCKET] &&
				(!tl || tl->th_priority != WORKQUEUE_EVENT_MANAGER_BUCKET)){
			/*
			 * There can only be one event manager running at a time.
			 */
			PTHREAD_TRACE_WQ(TRACE_wq_run_threadreq | DBG_FUNC_END, wq, 1, 0, 0, 0);
			goto done;
		}
	}

again: // Start again after creating a thread

	if (_wq_exiting(wq)) {
		rc = WQ_RUN_TR_EXITING;
		goto exiting;
	}

	/*
	 * Thread request selection and admission control
	 */
	struct threadreq *fallback = NULL;
	if (req) {
		if ((req->tr_flags & TR_FLAG_NO_PACING) == 0 &&
				_wq_should_pace_priority(wq, req->tr_priority)) {
			/*
			 * If a request fails the pacing admission check, then thread
			 * requests are redriven when the pacing thread is finally scheduled
			 * when it calls _wq_pacing_end() in wq_unpark_continue().
			 */
			goto done;
		}
	} else if (wq->wq_reqcount == 0) {
		PTHREAD_TRACE_WQ(TRACE_wq_run_threadreq | DBG_FUNC_END, wq, 2, 0, 0, 0);
		goto done;
	} else if ((req = workqueue_best_threadreqs(wq, tl, &fallback)) == NULL) {
		PTHREAD_TRACE_WQ(TRACE_wq_run_threadreq | DBG_FUNC_END, wq, 3, 0, 0, 0);
		goto done;
	}

	if ((req->tr_flags & TR_FLAG_OVERCOMMIT) == 0 &&
			(req->tr_priority < WORKQUEUE_EVENT_MANAGER_BUCKET)) {
		if (!may_start_constrained_thread(wq, req->tr_priority, parking_tl, true)) {
			if (!fallback) {
				PTHREAD_TRACE_WQ(TRACE_wq_run_threadreq | DBG_FUNC_END, wq, 4, 0, 0, 0);
				goto done;
			}
			assert(req->tr_state == TR_STATE_WAITING);
			req = fallback;
		}
	}

	/*
	 * Thread selection.
	 */
	if (parking_tl) {
		if (tl->th_priority != req->tr_priority) {
			_wq_thactive_move(wq, tl->th_priority, req->tr_priority);
			wq->wq_thscheduled_count[tl->th_priority]--;
			wq->wq_thscheduled_count[req->tr_priority]++;
		}
		PTHREAD_TRACE_WQ(TRACE_wq_run_threadreq_thread_select | DBG_FUNC_NONE,
				wq, 1, thread_tid(tl->th_thread), 0, 0);
	} else if (wq->wq_thidlecount) {
		tl = pop_from_thidlelist(wq, req->tr_priority);
		/*
		 * This call will update wq_thscheduled_count and wq_thactive_count for
		 * the provided priority.  It will not set the returned thread to that
		 * priority.  This matches the behavior of the parking_tl clause above.
		 */
		PTHREAD_TRACE_WQ(TRACE_wq_run_threadreq_thread_select | DBG_FUNC_NONE,
				wq, 2, thread_tid(tl->th_thread), 0, 0);
	} else /* no idle threads */ {
		if (!may_add_new_thread || wq->wq_nthreads >= wq_max_threads) {
			PTHREAD_TRACE_WQ(TRACE_wq_run_threadreq | DBG_FUNC_END, wq, 5,
					may_add_new_thread, wq->wq_nthreads, 0);
			if (wq->wq_nthreads < wq_max_threads) {
				rc = WQ_RUN_TR_THREAD_NEEDED;
			}
			goto done;
		}

		bool added_thread = workqueue_addnewthread(p, wq);
		/*
		 * workqueue_addnewthread will drop and re-take the lock, so we
		 * need to ensure we still have a cached request.
		 *
		 * It also means we have to pick a new request, since our old pick may
		 * not be valid anymore.
		 */
		req = incoming_req;
		if (req && (req->tr_flags & TR_FLAG_ONSTACK)) {
			_threadreq_copy_prepare(wq);
		}

		if (added_thread) {
			PTHREAD_TRACE_WQ(TRACE_wq_run_threadreq_thread_select | DBG_FUNC_NONE,
					wq, 3, 0, 0, 0);
			goto again;
		} else if (_wq_exiting(wq)) {
			rc = WQ_RUN_TR_EXITING;
			goto exiting;
		} else {
			PTHREAD_TRACE_WQ(TRACE_wq_run_threadreq | DBG_FUNC_END, wq, 6, 0, 0, 0);
			/*
			 * Something caused thread creation to fail.  Kick off the timer in
			 * the hope that it'll succeed next time.
			 */
			if (WQ_TIMER_DELAYED_NEEDED(wq)) {
				workqueue_interval_timer_start(wq);
			}
			goto done;
		}
	}

	/*
	 * Setup thread, mark request as complete and run with it.
	 */
	if (req->tr_state == TR_STATE_WAITING) {
		_threadreq_dequeue(wq, req);
	}
	if (tl->th_priority != req->tr_priority) {
		PTHREAD_TRACE_WQ(TRACE_wq_thread_reset_priority | DBG_FUNC_NONE,
					wq, thread_tid(tl->th_thread),
					(tl->th_priority << 16) | req->tr_priority, 1, 0);
		reset_priority(tl, pthread_priority_from_wq_class_index(wq, req->tr_priority));
		tl->th_priority = (uint8_t)req->tr_priority;
	}
	if (req->tr_flags & TR_FLAG_OVERCOMMIT) {
		if ((tl->th_flags & TH_LIST_CONSTRAINED) != 0) {
			tl->th_flags &= ~TH_LIST_CONSTRAINED;
			wq->wq_constrained_threads_scheduled--;
		}
	} else {
		if ((tl->th_flags & TH_LIST_CONSTRAINED) == 0) {
			tl->th_flags |= TH_LIST_CONSTRAINED;
			wq->wq_constrained_threads_scheduled++;
		}
	}

	if (!parking_tl && !(req->tr_flags & TR_FLAG_NO_PACING)) {
		_wq_pacing_start(wq, tl);
	}
	if ((req->tr_flags & TR_FLAG_OVERCOMMIT) == 0) {
		uint32_t old_qos, new_qos;

		/*
		 * If we are scheduling a constrained thread request, we may need to
		 * update the best constrained qos in the thactive atomic state.
		 */
		for (new_qos = 0; new_qos < WQ_THACTIVE_NO_PENDING_REQUEST; new_qos++) {
			if (TAILQ_FIRST(&wq->wq_reqlist[new_qos]))
				break;
		}
		old_qos = _wq_thactive_best_constrained_req_qos(wq);
		if (old_qos != new_qos) {
			wq_thactive_t v = _wq_thactive_set_best_constrained_req_qos(wq,
					old_qos, new_qos);
#ifdef __LP64__
			PTHREAD_TRACE_WQ(TRACE_wq_thactive_update, 2, (uint64_t)v,
					(uint64_t)(v >> 64), 0, 0);
#else
			PTHREAD_TRACE_WQ(TRACE_wq_thactive_update, 2, v, 0, 0, 0);
#endif
		}
	}
	{
		uint32_t upcall_flags = WQ_FLAG_THREAD_NEWSPI;
		if (req->tr_flags & TR_FLAG_OVERCOMMIT)
			upcall_flags |= WQ_FLAG_THREAD_OVERCOMMIT;
		if (req->tr_flags & TR_FLAG_KEVENT)
			upcall_flags |= WQ_FLAG_THREAD_KEVENT;
		if (req->tr_flags & TR_FLAG_WORKLOOP)
			upcall_flags |= WQ_FLAG_THREAD_WORKLOOP | WQ_FLAG_THREAD_KEVENT;
		if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET)
			upcall_flags |= WQ_FLAG_THREAD_EVENT_MANAGER;
		tl->th_upcall_flags = upcall_flags >> WQ_FLAG_THREAD_PRIOSHIFT;
	}
	if (req->tr_flags & TR_FLAG_KEVENT) {
		tl->th_flags |= TH_LIST_KEVENT;
	} else {
		tl->th_flags &= ~TH_LIST_KEVENT;
	}
	return _threadreq_complete_and_unlock(p, wq, req, tl);

done:
	if (incoming_req) {
		_threadreq_enqueue(wq, incoming_req);
	}

exiting:

	if (parking_tl && !(parking_tl->th_flags & TH_LIST_UNBINDING)) {
		parkit(wq, parking_tl, parking_tl->th_thread);
		__builtin_unreachable();
	}

	workqueue_unlock(wq);

	return rc;
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

	struct workqueue *wq = pthread_kern->proc_get_wqptr(p);
	if (wq == NULL) goto done;

	workqueue_lock_spin(wq);

	struct threadlist *tl = pthread_kern->uthread_get_threadlist(uth);
	assert(tl != WQ_THREADLIST_EXITING_POISON);
	if (tl == NULL) {
		/*
		 * We woke up before addnewthread() was finished setting us up.  Go
		 * ahead and exit, but before we do poison the threadlist variable so
		 * that addnewthread() doesn't think we are valid still.
		 */
		pthread_kern->uthread_set_threadlist(uth, WQ_THREADLIST_EXITING_POISON);
		workqueue_unlock(wq);
		goto done;
	}

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
			__builtin_unreachable();
		}
	}

	if ((tl->th_flags & TH_LIST_RUNNING) == 0) {
		assert((tl->th_flags & TH_LIST_BUSY) == 0);
		if (!first_use) {
			PTHREAD_TRACE_WQ(TRACE_wq_thread_park | DBG_FUNC_END, wq, 0, 0, 0, 0);
		}
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
			PTHREAD_TRACE_WQ(TRACE_wq_thread_reset_priority | DBG_FUNC_NONE,
						wq, thread_tid(th),
						(tl->th_priority << 16) | qos_class_get_class_index(WQ_THREAD_CLEANUP_QOS), 3, 0);
			pthread_priority_t cleanup_pri = _pthread_priority_make_newest(WQ_THREAD_CLEANUP_QOS, 0, 0);
			reset_priority(tl, cleanup_pri);
		}

		workqueue_removethread(tl, 0, first_use);

		if (first_use){
			pthread_kern->thread_bootstrap_return();
		} else {
			pthread_kern->unix_syscall_return(0);
		}
		__builtin_unreachable();
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
	if (!first_use) {
		PTHREAD_TRACE_WQ(TRACE_wq_thread_park | DBG_FUNC_END, wq, 0, 0, 0, 0);
	}
	if (_wq_pacing_end(wq, tl) && wq->wq_reqcount) {
		workqueue_run_threadreq_and_unlock(p, wq, NULL, NULL, true);
	} else {
		workqueue_unlock(wq);
	}
	_setup_wqthread(p, th, wq, tl, first_use ? WQ_SETUP_FIRST_USE : 0);
	pthread_kern->thread_sched_call(th, workqueue_callback);
done:
	if (first_use){
		pthread_kern->thread_bootstrap_return();
	} else {
		pthread_kern->unix_syscall_return(EJUSTRETURN);
	}
	panic("Our attempt to return to userspace failed...");
}

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
 * |kevent list| optionally - at most WQ_KEVENT_LIST_LEN events
 * |kevent data| optionally - at most WQ_KEVENT_DATA_SIZE bytes
 * |stack gap  | bottom aligned to 16 bytes, and at least as big as stack_gap_min
 * |   STACK   |
 * |     ⇓     |
 * |           |
 * |guard page | guardsize
 * |-----------| th_stackaddr
 */
void
_setup_wqthread(proc_t p, thread_t th, struct workqueue *wq,
		struct threadlist *tl, int setup_flags)
{
	int error;
	if (setup_flags & WQ_SETUP_CLEAR_VOUCHER) {
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
		__assert_only kern_return_t kr;
		kr = pthread_kern->thread_set_voucher_name(MACH_PORT_NULL);
		assert(kr == KERN_SUCCESS);
	}

	uint32_t upcall_flags = tl->th_upcall_flags << WQ_FLAG_THREAD_PRIOSHIFT;
	if (!(setup_flags & WQ_SETUP_FIRST_USE)) {
		upcall_flags |= WQ_FLAG_THREAD_REUSE;
	}

	/*
	 * Put the QoS class value into the lower bits of the reuse_thread register, this is where
	 * the thread priority used to be stored anyway.
	 */
	pthread_priority_t priority = pthread_priority_from_wq_class_index(wq, tl->th_priority);
	upcall_flags |= (_pthread_priority_get_qos_newest(priority) & WQ_FLAG_THREAD_PRIOMASK);

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

	if (setup_flags & WQ_SETUP_FIRST_USE) {
		uint32_t tsd_offset = pthread_kern->proc_get_pthread_tsd_offset(p);
		if (tsd_offset) {
			mach_vm_offset_t th_tsd_base = (mach_vm_offset_t)pthread_self_addr + tsd_offset;
			kern_return_t kret = pthread_kern->thread_set_tsd_base(th, th_tsd_base);
			if (kret == KERN_SUCCESS) {
				upcall_flags |= WQ_FLAG_THREAD_TSD_BASE_SET;
			}
		}

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
	}

	user_addr_t kevent_list = NULL;
	int kevent_count = 0;
	if (upcall_flags & WQ_FLAG_THREAD_KEVENT){
		bool workloop = upcall_flags & WQ_FLAG_THREAD_WORKLOOP;

		kevent_list = pthread_self_addr - WQ_KEVENT_LIST_LEN * sizeof(struct kevent_qos_s);
		kevent_count = WQ_KEVENT_LIST_LEN;

		user_addr_t kevent_id_addr = kevent_list;
		if (workloop) {
			/*
			 * The kevent ID goes just below the kevent list.  Sufficiently new
			 * userspace will know to look there.  Old userspace will just
			 * ignore it.
			 */
			kevent_id_addr -= sizeof(kqueue_id_t);
		}

		user_addr_t kevent_data_buf = kevent_id_addr - WQ_KEVENT_DATA_SIZE;
		user_size_t kevent_data_available = WQ_KEVENT_DATA_SIZE;

		int32_t events_out = 0;

		assert(tl->th_flags | TH_LIST_KEVENT_BOUND);
		unsigned int flags = KEVENT_FLAG_STACK_DATA | KEVENT_FLAG_IMMEDIATE;
		if (tl->th_priority == WORKQUEUE_EVENT_MANAGER_BUCKET) {
			flags |= KEVENT_FLAG_WORKQ_MANAGER;
		}
		int ret = 0;
		if (workloop) {
			flags |= KEVENT_FLAG_WORKLOOP;
			kqueue_id_t kevent_id = -1;
			ret = kevent_id_internal(p, &kevent_id,
					NULL, 0, kevent_list, kevent_count,
					kevent_data_buf, &kevent_data_available,
					flags, &events_out);
			copyout(&kevent_id, kevent_id_addr, sizeof(kevent_id));
		} else {
			flags |= KEVENT_FLAG_WORKQ;
			ret = kevent_qos_internal(p,
					class_index_get_thread_qos(tl->th_priority),
					NULL, 0, kevent_list, kevent_count,
					kevent_data_buf, &kevent_data_available,
					flags, &events_out);
		}

		// squash any errors into just empty output
		if (ret != KERN_SUCCESS || events_out == -1){
			events_out = 0;
			kevent_data_available = WQ_KEVENT_DATA_SIZE;
		}

		// We shouldn't get data out if there aren't events available
		assert(events_out != 0 || kevent_data_available == WQ_KEVENT_DATA_SIZE);

		if (events_out > 0){
			if (kevent_data_available == WQ_KEVENT_DATA_SIZE){
				stack_top_addr = (kevent_id_addr - stack_gap_min) & -stack_align_min;
			} else {
				stack_top_addr = (kevent_data_buf + kevent_data_available - stack_gap_min) & -stack_align_min;
			}

			kevent_count = events_out;
		} else {
			kevent_list = NULL;
			kevent_count = 0;
		}
	}

	PTHREAD_TRACE_WQ(TRACE_wq_runthread | DBG_FUNC_START, wq, 0, 0, 0, 0);

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

	if ((wq = pthread_kern->proc_get_wqptr(p)) == NULL) {
		return EINVAL;
	}

	/*
	 * This is sometimes called from interrupt context by the kperf sampler.
	 * In that case, it's not safe to spin trying to take the lock since we
	 * might already hold it.  So, we just try-lock it and error out if it's
	 * already held.  Since this is just a debugging aid, and all our callers
	 * are able to handle an error, that's fine.
	 */
	bool locked = workqueue_lock_try(wq);
	if (!locked) {
		return EBUSY;
	}

	activecount = _wq_thactive_aggregate_downto_qos(wq, _wq_thactive(wq),
			WORKQUEUE_NUM_BUCKETS - 1, NULL, NULL);
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

	pthread_zone_workqueue = zinit(sizeof(struct workqueue),
			1024 * sizeof(struct workqueue), 8192, "pthread.workqueue");
	pthread_zone_threadlist = zinit(sizeof(struct threadlist),
			1024 * sizeof(struct threadlist), 8192, "pthread.threadlist");
	pthread_zone_threadreq = zinit(sizeof(struct threadreq),
			1024 * sizeof(struct threadreq), 8192, "pthread.threadreq");

	/*
	 * register sysctls
	 */
	sysctl_register_oid(&sysctl__kern_wq_stalled_window_usecs);
	sysctl_register_oid(&sysctl__kern_wq_reduce_pool_window_usecs);
	sysctl_register_oid(&sysctl__kern_wq_max_timer_interval_usecs);
	sysctl_register_oid(&sysctl__kern_wq_max_threads);
	sysctl_register_oid(&sysctl__kern_wq_max_constrained_threads);
	sysctl_register_oid(&sysctl__kern_pthread_debug_tracing);

#if DEBUG
	sysctl_register_oid(&sysctl__debug_wq_kevent_test);
#endif

	for (int i = 0; i < WORKQUEUE_NUM_BUCKETS; i++) {
		uint32_t thread_qos = _wq_bucket_to_thread_qos(i);
		wq_max_concurrency[i] = pthread_kern->qos_max_parallelism(thread_qos,
				QOS_PARALLELISM_COUNT_LOGICAL);
	}
	wq_max_concurrency[WORKQUEUE_EVENT_MANAGER_BUCKET] = 1;
}
