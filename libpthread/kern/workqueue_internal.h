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
#define WQOPS_THREAD_RETURN 4
#define WQOPS_QUEUE_NEWSPISUPP  0x10	/* this is to check for newer SPI support */
#define WQOPS_QUEUE_REQTHREADS  0x20	/* request number of threads of a prio */
#define WQOPS_QUEUE_REQTHREADS2 0x30	/* request a number of threads in a given priority bucket */

/* flag values for reuse field in the libc side _pthread_wqthread */
#define	WQ_FLAG_THREAD_PRIOMASK		0x0000ffff
#define WQ_FLAG_THREAD_PRIOSHIFT	(8ull)
#define	WQ_FLAG_THREAD_OVERCOMMIT	0x00010000	/* thread is with overcommit prio */
#define	WQ_FLAG_THREAD_REUSE		0x00020000	/* thread is being reused */
#define	WQ_FLAG_THREAD_NEWSPI		0x00040000	/* the call is with new SPIs */

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

#define WORKQUEUE_NUM_BUCKETS 6

/* wq_max_constrained_threads = max(64, N_CPU * WORKQUEUE_CONSTRAINED_FACTOR)
 * This used to be WORKQUEUE_NUM_BUCKETS + 1 when NUM_BUCKETS was 4, yielding
 * N_CPU * 5. When NUM_BUCKETS changed, we decided that the limit should
 * not change. So the factor is now always 5.
 */
#define WORKQUEUE_CONSTRAINED_FACTOR 5

#define WORKQUEUE_OVERCOMMIT	0x10000

struct threadlist {
	TAILQ_ENTRY(threadlist) th_entry;
	thread_t th_thread;
	int	 th_flags;
	uint8_t	 th_priority;
	uint8_t  th_policy;
	struct workqueue *th_workq;
	mach_vm_size_t th_stacksize;
	mach_vm_size_t th_allocsize;
	mach_vm_offset_t th_stackaddr;
	mach_port_name_t th_thport;
	uint32_t th_override_count;
	uint32_t th_dispatch_override_count;
};
#define TH_LIST_INITED 		0x01
#define TH_LIST_RUNNING 	0x02
#define TH_LIST_BLOCKED 	0x04
#define TH_LIST_SUSPENDED 	0x08
#define TH_LIST_BUSY		0x10
#define TH_LIST_NEED_WAKEUP	0x20
#define TH_LIST_CONSTRAINED	0x40


struct workqueue {
	proc_t		wq_proc;
	vm_map_t	wq_map;
	task_t		wq_task;
	thread_call_t	wq_atimer_call;
	int 		wq_flags;
	int			wq_lflags;
	uint64_t 	wq_thread_yielded_timestamp;
	uint32_t	wq_thread_yielded_count;
	uint32_t	wq_timer_interval;
	uint32_t	wq_max_concurrency;
	uint32_t	wq_threads_scheduled;
	uint32_t	wq_constrained_threads_scheduled;
	uint32_t	wq_nthreads;
	uint32_t	wq_thidlecount;
	uint32_t	wq_reqcount;
	TAILQ_HEAD(, threadlist) wq_thrunlist;
	TAILQ_HEAD(, threadlist) wq_thidlelist;
	uint16_t	wq_requests[WORKQUEUE_NUM_BUCKETS];
	uint16_t	wq_ocrequests[WORKQUEUE_NUM_BUCKETS];
	uint16_t	wq_reqconc[WORKQUEUE_NUM_BUCKETS];	  		/* requested concurrency for each priority level */
	uint16_t	wq_thscheduled_count[WORKQUEUE_NUM_BUCKETS];
	uint32_t	wq_thactive_count[WORKQUEUE_NUM_BUCKETS] __attribute__((aligned(4))); /* must be uint32_t since we OSAddAtomic on these */
	uint64_t	wq_lastblocked_ts[WORKQUEUE_NUM_BUCKETS] __attribute__((aligned(8)));
};
#define WQ_LIST_INITED		0x01
#define WQ_ATIMER_RUNNING	0x02
#define WQ_EXITING		0x04

#define WQL_ATIMER_BUSY		0x01
#define WQL_ATIMER_WAITING	0x02
#define WQL_EXCEEDED_CONSTRAINED_THREAD_LIMIT    0x04
#define WQL_EXCEEDED_TOTAL_THREAD_LIMIT          0x08


#define WQ_VECT_SET_BIT(vector, bit)	\
	vector[(bit) / 32] |= (1 << ((bit) % 32))

#define WQ_VECT_CLEAR_BIT(vector, bit)	\
	vector[(bit) / 32] &= ~(1 << ((bit) % 32))

#define WQ_VECT_TEST_BIT(vector, bit)	\
	vector[(bit) / 32] & (1 << ((bit) % 32))

#define WORKQUEUE_MAXTHREADS		512
#define WQ_YIELDED_THRESHOLD		2000
#define WQ_YIELDED_WINDOW_USECS		30000
#define WQ_STALLED_WINDOW_USECS		200
#define WQ_REDUCE_POOL_WINDOW_USECS	5000000
#define	WQ_MAX_TIMER_INTERVAL_USECS	50000

#endif // KERNEL

#endif // _WORKQUEUE_INTERNAL_H_
