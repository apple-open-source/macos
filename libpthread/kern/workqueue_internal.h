/*
 * Copyright (c) 2014 Apple Computer, Inc. All rights reserved.
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

#ifndef _WORKQUEUE_INTERNAL_H_
#define _WORKQUEUE_INTERNAL_H_

/* These definitions are shared between the kext and userspace inside the pthread project. Consolidating
 * duplicate definitions that used to exist in both projects, when separate.
 */

/* workq_kernreturn commands */
#define WQOPS_THREAD_RETURN        0x04	/* parks the thread back into the kernel */
#define WQOPS_QUEUE_NEWSPISUPP     0x10	/* this is to check for newer SPI support */
#define WQOPS_QUEUE_REQTHREADS     0x20	/* request number of threads of a prio */
#define WQOPS_QUEUE_REQTHREADS2    0x30	/* request a number of threads in a given priority bucket */
#define WQOPS_THREAD_KEVENT_RETURN 0x40	/* parks the thread after delivering the passed kevent array */
#define WQOPS_SET_EVENT_MANAGER_PRIORITY 0x80	/* max() in the provided priority in the the priority of the event manager */

/* flag values for upcall flags field, only 8 bits per struct threadlist */
#define	WQ_FLAG_THREAD_PRIOMASK			0x0000ffff
#define WQ_FLAG_THREAD_PRIOSHIFT		16
#define	WQ_FLAG_THREAD_OVERCOMMIT		0x00010000	/* thread is with overcommit prio */
#define	WQ_FLAG_THREAD_REUSE			0x00020000	/* thread is being reused */
#define	WQ_FLAG_THREAD_NEWSPI			0x00040000	/* the call is with new SPIs */
#define WQ_FLAG_THREAD_KEVENT			0x00080000  /* thread is response to kevent req */
#define WQ_FLAG_THREAD_EVENT_MANAGER	0x00100000  /* event manager thread */
#define WQ_FLAG_THREAD_TSD_BASE_SET		0x00200000  /* tsd base has already been set */

#define WQ_THREAD_CLEANUP_QOS QOS_CLASS_UTILITY

/* These definitions are only available to the kext, to avoid bleeding constants and types across the boundary to
 * the userspace library.
 */
#ifdef KERNEL

/* These defines come from kern/thread.h but are XNU_KERNEL_PRIVATE so do not get
 * exported to kernel extensions.
 */
#define SCHED_CALL_BLOCK 0x1
#define SCHED_CALL_UNBLOCK 0x2

// kwe_state
enum {
	KWE_THREAD_INWAIT = 1,
	KWE_THREAD_PREPOST,
	KWE_THREAD_BROADCAST,
};

/* old workq priority scheme */

#define WORKQUEUE_HIGH_PRIOQUEUE    0       /* high priority queue */
#define WORKQUEUE_DEFAULT_PRIOQUEUE 1       /* default priority queue */
#define WORKQUEUE_LOW_PRIOQUEUE     2       /* low priority queue */
#define WORKQUEUE_BG_PRIOQUEUE      3       /* background priority queue */

#define WORKQUEUE_NUM_BUCKETS 7

// Sometimes something gets passed a bucket number and we need a way to express
// that it's actually the event manager.  Use the (n+1)th bucket for that.
#define WORKQUEUE_EVENT_MANAGER_BUCKET (WORKQUEUE_NUM_BUCKETS-1)

/* wq_max_constrained_threads = max(64, N_CPU * WORKQUEUE_CONSTRAINED_FACTOR)
 * This used to be WORKQUEUE_NUM_BUCKETS + 1 when NUM_BUCKETS was 4, yielding
 * N_CPU * 5. When NUM_BUCKETS changed, we decided that the limit should
 * not change. So the factor is now always 5.
 */
#define WORKQUEUE_CONSTRAINED_FACTOR 5

#define WORKQUEUE_OVERCOMMIT	0x10000

/*
 * A thread which is scheduled may read its own th_priority field without
 * taking the workqueue lock.  Other fields should be assumed to require the
 * lock.
 */
struct threadlist {
	TAILQ_ENTRY(threadlist) th_entry;
	thread_t th_thread;
	struct workqueue *th_workq;
	mach_vm_offset_t th_stackaddr;
	mach_port_name_t th_thport;
	uint16_t th_flags;
	uint8_t th_upcall_flags;
	uint8_t th_priority;
};

#define TH_LIST_INITED 		0x01 /* Set at thread creation. */
#define TH_LIST_RUNNING 	0x02 /* On thrunlist, not parked. */
#define TH_LIST_KEVENT		0x04 /* Thread requested by kevent */
#define TH_LIST_NEW			0x08 /* First return to userspace */
#define TH_LIST_BUSY		0x10 /* Removed from idle list but not ready yet. */
#define TH_LIST_KEVENT_BOUND	0x20 /* Thread bound to kqueues */
#define TH_LIST_CONSTRAINED	0x40 /* Non-overcommit thread. */
#define TH_LIST_EVENT_MGR_SCHED_PRI	0x80 /* Non-QoS Event Manager */

struct workqueue {
	proc_t		wq_proc;
	vm_map_t	wq_map;
	task_t		wq_task;

	_Atomic uint32_t	wq_flags;  // updated atomically
	uint32_t	wq_lflags; // protected by wqueue lock

	lck_spin_t	wq_lock;
	boolean_t	wq_interrupt_state;

	thread_call_t	wq_atimer_delayed_call;
	thread_call_t	wq_atimer_immediate_call;

	uint64_t	wq_thread_yielded_timestamp;
	uint32_t	wq_thread_yielded_count;
	uint32_t	wq_timer_interval;
	uint32_t	wq_max_concurrency;
	uint32_t	wq_threads_scheduled;
	uint32_t	wq_constrained_threads_scheduled;
	uint32_t	wq_nthreads;
	uint32_t	wq_thidlecount;

	TAILQ_HEAD(, threadlist) wq_thrunlist;
	TAILQ_HEAD(, threadlist) wq_thidlelist;
	TAILQ_HEAD(, threadlist) wq_thidlemgrlist;

	/* Counters for how many requests we have outstanding.  The invariants here:
	 *   - reqcount == SUM(requests) + (event manager ? 1 : 0)
	 *   - SUM(ocrequests) + SUM(kevent_requests) + SUM(kevent_ocrequests) <= SUM(requests)
	 *   - # of constrained requests is difference between quantities above
	 * i.e. a kevent+overcommit request will increment reqcount, requests and
	 * kevent_ocrequests only.
	 */
	uint32_t	wq_reqcount;
	uint16_t	wq_requests[WORKQUEUE_NUM_BUCKETS];
	uint16_t	wq_ocrequests[WORKQUEUE_NUM_BUCKETS];
	uint16_t	wq_kevent_requests[WORKQUEUE_NUM_BUCKETS];
	uint16_t	wq_kevent_ocrequests[WORKQUEUE_NUM_BUCKETS];

	uint16_t	wq_reqconc[WORKQUEUE_NUM_BUCKETS];	  		/* requested concurrency for each priority level */
	uint16_t	wq_thscheduled_count[WORKQUEUE_NUM_BUCKETS];
	uint32_t	wq_thactive_count[WORKQUEUE_NUM_BUCKETS] __attribute__((aligned(4))); /* must be uint32_t since we OSAddAtomic on these */
	uint64_t	wq_lastblocked_ts[WORKQUEUE_NUM_BUCKETS] __attribute__((aligned(8))); /* XXX: why per bucket? */

	uint32_t	wq_event_manager_priority;
};
#define WQ_LIST_INITED		0x01
#define WQ_EXITING		0x02
#define WQ_ATIMER_DELAYED_RUNNING	0x04
#define WQ_ATIMER_IMMEDIATE_RUNNING	0x08

#define WQ_SETFLAG(wq, flag) __c11_atomic_fetch_or(&wq->wq_flags, flag, __ATOMIC_SEQ_CST)
#define WQ_UNSETFLAG(wq, flag) __c11_atomic_fetch_and(&wq->wq_flags, ~flag, __ATOMIC_SEQ_CST)

#define WQL_ATIMER_BUSY		0x01
#define WQL_ATIMER_WAITING	0x02

#define WORKQUEUE_MAXTHREADS		512
#define WQ_YIELDED_THRESHOLD		2000
#define WQ_YIELDED_WINDOW_USECS		30000
#define WQ_STALLED_WINDOW_USECS		200
#define WQ_REDUCE_POOL_WINDOW_USECS	5000000
#define	WQ_MAX_TIMER_INTERVAL_USECS	50000

#endif // KERNEL

#endif // _WORKQUEUE_INTERNAL_H_
