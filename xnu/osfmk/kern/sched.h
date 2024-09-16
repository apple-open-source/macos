/*
 * Copyright (c) 2000-2019 Apple Inc. All rights reserved.
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
/*
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	sched.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1985
 *
 *	Header file for scheduler.
 *
 */

#ifndef _KERN_SCHED_H_
#define _KERN_SCHED_H_

#include <mach/policy.h>
#include <kern/kern_types.h>
#include <kern/smp.h>
#include <kern/circle_queue.h>
#include <kern/macro_help.h>
#include <kern/timer_call.h>
#include <kern/ast.h>
#include <kern/bits.h>

#define NRQS_MAX        (128)                           /* maximum number of priority levels */

#define MAXPRI          (NRQS_MAX-1)
#define MINPRI          0                               /* lowest legal priority schedulable */
#define IDLEPRI         MINPRI                          /* idle thread priority */
#define NOPRI           -1

/*
 *	High-level priority assignments
 *
 *************************************************************************
 * 127		Reserved (real-time)
 *				A
 *				+
 *			(32 levels)
 *				+
 *				V
 * 96		Reserved (real-time)
 * 95		Kernel mode only
 *				A
 *				+
 *			(16 levels)
 *				+
 *				V
 * 80		Kernel mode only
 * 79		System high priority
 *				A
 *				+
 *			(16 levels)
 *				+
 *				V
 * 64		System high priority
 * 63		Elevated priorities
 *				A
 *				+
 *			(12 levels)
 *				+
 *				V
 * 52		Elevated priorities
 * 51		Elevated priorities (incl. BSD +nice)
 *				A
 *				+
 *			(20 levels)
 *				+
 *				V
 * 32		Elevated priorities (incl. BSD +nice)
 * 31		Default (default base for threads)
 * 30		Lowered priorities (incl. BSD -nice)
 *				A
 *				+
 *			(20 levels)
 *				+
 *				V
 * 11		Lowered priorities (incl. BSD -nice)
 * 10		Lowered priorities (aged pri's)
 *				A
 *				+
 *			(11 levels)
 *				+
 *				V
 * 0		Lowered priorities (aged pri's / idle)
 *************************************************************************
 */

#define BASEPRI_RTQUEUES        (BASEPRI_REALTIME + 1)                          /* 97 */
#define BASEPRI_REALTIME        (MAXPRI - (NRQS_MAX / 4) + 1)                   /* 96 */

#define MAXPRI_KERNEL           (BASEPRI_REALTIME - 1)                          /* 95 */
#define BASEPRI_PREEMPT_HIGH    (BASEPRI_PREEMPT + 1)                           /* 93 */
#define BASEPRI_PREEMPT         (MAXPRI_KERNEL - 3)                             /* 92 */
#define BASEPRI_VM              (BASEPRI_PREEMPT - 1)                           /* 91 */

#define BASEPRI_KERNEL          (MINPRI_KERNEL + 1)                             /* 81 */
#define MINPRI_KERNEL           (MAXPRI_KERNEL - (NRQS_MAX / 8) + 1)            /* 80 */

#define MAXPRI_RESERVED         (MINPRI_KERNEL - 1)                             /* 79 */
#define BASEPRI_GRAPHICS        (MAXPRI_RESERVED - 3)                           /* 76 */
#define MINPRI_RESERVED         (MAXPRI_RESERVED - (NRQS_MAX / 8) + 1)          /* 64 */

#define MAXPRI_USER             (MINPRI_RESERVED - 1)                           /* 63 */
#define BASEPRI_CONTROL         (BASEPRI_DEFAULT + 17)                          /* 48 */
#define BASEPRI_FOREGROUND      (BASEPRI_DEFAULT + 16)                          /* 47 */
#define BASEPRI_BACKGROUND      (BASEPRI_DEFAULT + 15)                          /* 46 */
#define BASEPRI_USER_INITIATED  (BASEPRI_DEFAULT +  6)                          /* 37 */
#define BASEPRI_DEFAULT         (MAXPRI_USER - (NRQS_MAX / 4))                  /* 31 */
#define MAXPRI_SUPPRESSED       (BASEPRI_DEFAULT - 3)                           /* 28 */
#define BASEPRI_UTILITY         (BASEPRI_DEFAULT - 11)                          /* 20 */
#define MAXPRI_THROTTLE         (MINPRI + 4)                                    /*  4 */
#define MINPRI_USER             MINPRI                                          /*  0 */

#define DEPRESSPRI              (MINPRI)                /* depress priority */

#define MAXPRI_PROMOTE          (MAXPRI_KERNEL)         /* ceiling for mutex promotion */
#define MINPRI_RWLOCK           (BASEPRI_BACKGROUND)    /* floor when holding rwlock count */
#define MINPRI_EXEC             (BASEPRI_DEFAULT)       /* floor when in exec state */
#define MINPRI_WAITQ            (BASEPRI_DEFAULT)       /* floor when in waitq handover state */
#define MINPRI_FLOOR            (BASEPRI_BACKGROUND)    /* floor when boost requested */

#define NRQS                    (BASEPRI_REALTIME)      /* Non-realtime levels for runqs */
#define NRTQS                   (MAXPRI - BASEPRI_REALTIME) /* Realtime levels for runqs */

/* Ensure that NRQS is large enough to represent all non-realtime threads; even promoted ones */
_Static_assert((NRQS == (MAXPRI_PROMOTE + 1)), "Runqueues are too small to hold all non-realtime threads");

/* Type used for thread->sched_mode and saved_mode */
typedef enum {
	TH_MODE_NONE = 0,                                       /* unassigned, usually for saved_mode only */
	TH_MODE_REALTIME,                                       /* time constraints supplied */
	TH_MODE_FIXED,                                          /* use fixed priorities, no decay */
	TH_MODE_TIMESHARE,                                      /* use timesharing algorithm */
} sched_mode_t;

/*
 * Determine whether the target platform should run the Clutch/Edge Scheduler.
 * All arm64 platforms are eligible to do so.
 */
#if defined(__arm64__) && CONFIG_CLUTCH && !CONFIG_SCHED_EDGE_OPT_OUT

/*
 * Single-cluster, symmetric (SMP) systems can run with just the Clutch policy, but
 * multi-cluster, asymmetric (AMP) systems must further enable the Edge policy
 * extension to Clutch in order to manage scheduling across the multiple CPU clusters.
 */
#define CONFIG_SCHED_CLUTCH 1
#if __AMP__
#define CONFIG_SCHED_EDGE   1
#endif /* __AMP__ */

#endif /* defined(__arm64__) && CONFIG_CLUTCH && !CONFIG_SCHED_EDGE_OPT_OUT */

/*
 * Since the clutch scheduler organizes threads based on the thread group
 * and the scheduling bucket, its important to not mix threads from multiple
 * priority bands into the same bucket. To achieve that, in the clutch bucket
 * world, there is a scheduling bucket per QoS effectively.
 */

/* Buckets used for load calculation */
typedef enum {
	TH_BUCKET_FIXPRI = 0,                   /* Fixed-priority */
	TH_BUCKET_SHARE_FG,                     /* Timeshare thread above BASEPRI_DEFAULT */
#if CONFIG_SCHED_CLUTCH
	TH_BUCKET_SHARE_IN,                     /* Timeshare thread between BASEPRI_USER_INITIATED and BASEPRI_DEFAULT */
#endif /* CONFIG_SCHED_CLUTCH */
	TH_BUCKET_SHARE_DF,                     /* Timeshare thread between BASEPRI_DEFAULT and BASEPRI_UTILITY */
	TH_BUCKET_SHARE_UT,                     /* Timeshare thread between BASEPRI_UTILITY and MAXPRI_THROTTLE */
	TH_BUCKET_SHARE_BG,                     /* Timeshare thread between MAXPRI_THROTTLE and MINPRI */
	TH_BUCKET_RUN,                          /* All runnable threads */
	TH_BUCKET_SCHED_MAX = TH_BUCKET_RUN,    /* Maximum schedulable buckets */
	TH_BUCKET_MAX,
} sched_bucket_t;

/*
 *	Macro to check for invalid priorities.
 */
#define invalid_pri(pri) ((pri) < MINPRI || (pri) > MAXPRI)

struct runq_stats {
	uint64_t                count_sum;
	uint64_t                last_change_timestamp;
};

#if defined(CONFIG_SCHED_TIMESHARE_CORE)

struct run_queue {
	int                     highq;                          /* highest runnable queue */
	bitmap_t                bitmap[BITMAP_LEN(NRQS)];       /* run queue bitmap array */
	int                     count;                          /* # of threads total */
	int                     urgency;                        /* level of preemption urgency */
	circle_queue_head_t     queues[NRQS];           /* one for each priority */

	struct runq_stats       runq_stats;
};

inline static void
rq_bitmap_set(bitmap_t *__header_indexable map, u_int n)
{
	assert(n < NRQS);
	bitmap_set(map, n);
}

inline static void
rq_bitmap_clear(bitmap_t *__header_indexable map, u_int n)
{
	assert(n < NRQS);
	bitmap_clear(map, n);
}

#endif /* defined(CONFIG_SCHED_TIMESHARE_CORE) */

typedef struct {
	queue_head_t            pri_queue;                      /* runnable RT threads for this priority */
	uint64_t                pri_earliest_deadline;          /* earliest deadline for this priority */
	int                     pri_count;                      /* # of threads for this priority */
	uint32_t                pri_constraint;                 /* constraint of earliest deadline thread for this priority */
} rt_queue_pri_t;

struct rt_queue {
	_Atomic uint64_t        earliest_deadline;              /* earliest deadline */
	_Atomic int             count;                          /* # of threads total */
	_Atomic uint32_t        constraint;                     /* constraint of earliest deadline thread */
	_Atomic int             ed_index;                       /* index of earliest deadline thread */

	bitmap_t                bitmap[BITMAP_LEN(NRTQS)];

	rt_queue_pri_t          rt_queue_pri[NRTQS];

	struct runq_stats       runq_stats;
};
typedef struct rt_queue *rt_queue_t;

#define RT_CONSTRAINT_NONE              UINT32_MAX
#define RT_DEADLINE_NONE                UINT64_MAX
#define RT_DEADLINE_QUANTUM_EXPIRED     (UINT64_MAX - 1)

extern int rt_runq_count(processor_set_t);
extern uint64_t rt_runq_earliest_deadline(processor_set_t);



/*
 *	Scheduler routines.
 */

/* Handle quantum expiration for an executing thread */
extern void             thread_quantum_expire(
	timer_call_param_t      processor,
	timer_call_param_t      thread);

/* Handle preemption timer expiration for an executing thread */
extern void             thread_preempt_expire(
	timer_call_param_t      processor,
	timer_call_param_t      thread);

/* Context switch check for current processor */
extern ast_t    csw_check(
	thread_t      thread,
	processor_t   processor,
	ast_t         check_reason);

/* Check for pending ASTs */
extern void ast_check(processor_t processor);

extern ast_t update_pending_nonurgent_preemption(processor_t processor, ast_t reason);
extern void clear_pending_nonurgent_preemption(processor_t processor);

extern void sched_update_generation_count(void);

#if defined(CONFIG_SCHED_TIMESHARE_CORE)
extern uint32_t std_quantum, min_std_quantum;
extern uint32_t std_quantum_us;
#endif /* CONFIG_SCHED_TIMESHARE_CORE */

extern uint32_t thread_depress_time;
extern uint32_t default_timeshare_computation;
extern uint32_t default_timeshare_constraint;

extern uint32_t max_rt_quantum, min_rt_quantum;

extern int default_preemption_rate;

#if defined(CONFIG_SCHED_TIMESHARE_CORE)

/*
 *	Age usage  at approximately (1 << SCHED_TICK_SHIFT) times per second
 *	Aging may be deferred during periods where all processors are idle
 *	and cumulatively applied during periods of activity.
 */
#define SCHED_TICK_SHIFT        3
#define SCHED_TICK_MAX_DELTA    (8)

extern unsigned         sched_tick;
extern uint32_t         sched_tick_interval;

#endif /* CONFIG_SCHED_TIMESHARE_CORE */

extern uint64_t         sched_one_second_interval;

/* Periodic computation of various averages */
extern void            compute_sched_load(void);

extern void             compute_averages(uint64_t);

extern void             compute_averunnable(
	void                    *nrun);

extern void             compute_stack_target(
	void                    *arg);

extern void             compute_pageout_gc_throttle(
	void                    *arg);

extern void             compute_pmap_gc_throttle(
	void                    *arg);

/*
 *	Conversion factor from usage
 *	to priority.
 */
#if defined(CONFIG_SCHED_TIMESHARE_CORE)

#define MAX_LOAD (NRQS - 1)
#define SCHED_PRI_SHIFT_MAX ((8 * sizeof(uint32_t)) - 1)
extern uint32_t         sched_pri_shifts[TH_BUCKET_MAX];
extern uint32_t         sched_fixed_shift;
extern int8_t           sched_load_shifts[NRQS];
extern uint32_t         sched_decay_usage_age_factor;
void sched_timeshare_consider_maintenance(uint64_t ctime, bool safe_point);
#endif /* CONFIG_SCHED_TIMESHARE_CORE */

void sched_consider_recommended_cores(uint64_t ctime, thread_t thread);

extern int32_t          sched_poll_yield_shift;
extern uint64_t         sched_safe_rt_duration;
extern uint64_t         sched_safe_fixed_duration;

extern uint32_t         sched_load_average, sched_mach_factor;

extern uint32_t         avenrun[3], mach_factor[3];

extern uint64_t         max_unsafe_rt_computation;
extern uint64_t         max_unsafe_fixed_computation;
extern uint64_t         max_poll_computation;

extern uint32_t         sched_run_buckets[TH_BUCKET_MAX];

extern uint32_t sched_run_incr(thread_t thread);
extern uint32_t sched_run_decr(thread_t thread);
extern void sched_update_thread_bucket(thread_t thread);

extern uint32_t sched_smt_run_incr(thread_t thread);
extern uint32_t sched_smt_run_decr(thread_t thread);
extern void sched_smt_update_thread_bucket(thread_t thread);

#define SCHED_DECAY_TICKS       32
struct shift_data {
	int     shift1;
	int     shift2;
};

/*
 * Save the current thread time and compute a delta since the last call for the
 * scheduler tick.
 */
#define sched_tick_delta(thread, delta) \
MACRO_BEGIN \
    uint64_t _total = recount_thread_time_mach(thread); \
    (delta) = (typeof(delta))(_total - thread->sched_time_save); \
    thread->sched_time_save = _total; \
MACRO_END

#define SCHED_MAX_BACKUP_PROCESSORS             7
#if defined(__x86_64__)
#define SCHED_DEFAULT_BACKUP_PROCESSORS         1
#define SCHED_DEFAULT_BACKUP_PROCESSORS_SMT     2
#else
#define SCHED_DEFAULT_BACKUP_PROCESSORS         0
#define SCHED_DEFAULT_BACKUP_PROCESSORS_SMT     0
#endif
extern int sched_rt_n_backup_processors;

extern bool system_is_SMT;

#endif  /* _KERN_SCHED_H_ */
