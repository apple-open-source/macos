/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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
 * Timer queue support routines.
 */

#ifndef _KERN_TIMER_QUEUE_H_
#define _KERN_TIMER_QUEUE_H_

#include <mach/mach_types.h>

#ifdef MACH_KERNEL_PRIVATE

#include <kern/mpqueue.h>
#include <kern/queue.h>

/*
 *	Invoked by kernel, implemented by platform.
 */

/* Request an expiration deadline, returns queue association */
extern mpqueue_head_t * timer_queue_assign(
	uint64_t                deadline);

extern uint64_t         timer_call_slop(
	uint64_t                deadline,
	uint64_t                armtime,
	uint32_t                urgency,
	thread_t                arming_thread,
	boolean_t               *rlimited);
extern boolean_t        timer_resort_threshold(uint64_t);

/* Cancel an associated expiration deadline and specify new deadline */
extern void             timer_queue_cancel(
	mpqueue_head_t          *queue,
	uint64_t                deadline,
	uint64_t                new_deadline);

/* Return a pointer to the local timer queue for a given cpu */
extern mpqueue_head_t * timer_queue_cpu(
	int                     cpu);

/* Call a function with argument on a cpu */
extern void             timer_call_cpu(
	int                     cpu,
	void                    (*fn)(void *),
	void                    *arg);

/* Queue a function to be called with argument on a cpu */
extern void             timer_call_nosync_cpu(
	int                     cpu,
	void                    (*fn)(void *),
	void                    *arg);

/*
 *	Invoked by platform, implemented by kernel.
 */

/*
 *	Invoked by kernel, implemented by platform.
 */

#define NUM_LATENCY_QOS_TIERS (6)

typedef struct {
	uint32_t idle_entry_timer_processing_hdeadline_threshold_ns;
	uint32_t interrupt_timer_coalescing_ilat_threshold_ns;
	uint32_t timer_resort_threshold_ns;

	int32_t timer_coalesce_rt_shift;
	int32_t timer_coalesce_bg_shift;
	int32_t timer_coalesce_kt_shift;
	int32_t timer_coalesce_fp_shift;
	int32_t timer_coalesce_ts_shift;

	uint64_t timer_coalesce_rt_ns_max;
	uint64_t timer_coalesce_bg_ns_max;
	uint64_t timer_coalesce_kt_ns_max;
	uint64_t timer_coalesce_fp_ns_max;
	uint64_t timer_coalesce_ts_ns_max;

	uint32_t latency_qos_scale[NUM_LATENCY_QOS_TIERS];
	uint64_t latency_qos_ns_max[NUM_LATENCY_QOS_TIERS];
	boolean_t latency_tier_rate_limited[NUM_LATENCY_QOS_TIERS];
} timer_coalescing_priority_params_ns_t;

extern timer_coalescing_priority_params_ns_t * timer_call_get_priority_params(void);


extern uint64_t         timer_call_slop(
	uint64_t                deadline,
	uint64_t                armtime,
	uint32_t                urgency,
	thread_t                arming_thread,
	boolean_t               *rlimited);

/* Process deadline expiration for queue, returns new deadline */
extern uint64_t         timer_queue_expire(
	mpqueue_head_t          *queue,
	uint64_t                deadline);

extern uint64_t         timer_queue_expire_with_options(
	mpqueue_head_t *,
	uint64_t,
	boolean_t);

/* Shutdown a timer queue and reassign existing activities */
extern void             timer_queue_shutdown(int target_cpu,
    mpqueue_head_t          *queue,
    mpqueue_head_t          *new_queue);

/* Move timer requests from one queue to another */
extern int              timer_queue_migrate(
	mpqueue_head_t          *from,
	mpqueue_head_t          *to);

/*
 *	Invoked by platform, implemented by platfrom.
 */

extern void             timer_intr(int inuser, uint64_t iaddr);

#if defined(i386) || defined(x86_64)
extern uint64_t         setPop(uint64_t time);
#else
extern int              setPop(uint64_t time);
#endif

extern void             timer_resync_deadlines(void);

extern void             timer_queue_expire_local(void *arg);

extern void             timer_set_deadline(uint64_t deadline);

/* Migrate the local timer queue of a given cpu to the master cpu */
extern uint32_t         timer_queue_migrate_cpu(int target_cpu);

extern void             timer_queue_trace(
	mpqueue_head_t          *queue);
extern void             timer_queue_trace_cpu(int cpu);

extern uint64_t         timer_sysctl_get(int oid);
extern kern_return_t    timer_sysctl_set(int oid, uint64_t value);

#endif  /* MACH_KERNEL_PRIVATE */

#endif  /* _KERN_TIMER_QUEUE_H_ */
