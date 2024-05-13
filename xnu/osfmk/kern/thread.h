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
 * @OSF_FREE_COPYRIGHT@
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
 *	File:	thread.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	This file contains the structure definitions for threads.
 *
 */
/*
 * Copyright (c) 1993 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 */

#ifndef _KERN_THREAD_H_
#define _KERN_THREAD_H_

#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <mach/mach_param.h>
#include <mach/message.h>
#include <mach/boolean.h>
#include <mach/vm_param.h>
#include <mach/thread_info.h>
#include <mach/thread_status.h>
#include <mach/exception_types.h>

#include <kern/kern_types.h>
#include <vm/vm_kern.h>
#include <sys/cdefs.h>
#include <sys/_types/_size_t.h>

#ifdef MACH_KERNEL_PRIVATE
#include <mach_assert.h>
#include <mach_ldebug.h>

#include <ipc/ipc_types.h>

#include <mach/port.h>
#include <kern/cpu_number.h>
#include <kern/smp.h>
#include <kern/queue.h>

#include <kern/timer.h>
#include <kern/simple_lock.h>
#include <kern/locks.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <mach/sfi_class.h>
#include <kern/thread_call.h>
#include <kern/thread_group.h>
#include <kern/timer_call.h>
#include <kern/task.h>
#include <kern/exception.h>
#include <kern/affinity.h>
#include <kern/debug.h>
#include <kern/block_hint.h>
#include <kern/recount.h>
#include <kern/turnstile.h>
#include <kern/mpsc_queue.h>

#include <kern/waitq.h>
#include <san/kasan.h>
#include <san/kcov_data.h>
#include <os/refcnt.h>

#include <ipc/ipc_kmsg.h>

#include <machine/atomic.h>
#include <machine/cpu_data.h>
#include <machine/thread.h>

#endif  /* MACH_KERNEL_PRIVATE */
#ifdef XNU_KERNEL_PRIVATE
/* priority queue static asserts fail for __ARM64_ARCH_8_32__ kext builds */
#include <kern/priority_queue.h>
#endif /* XNU_KERNEL_PRIVATE */

__BEGIN_DECLS

#ifdef XNU_KERNEL_PRIVATE
#if CONFIG_TASKWATCH
/* Taskwatch related. TODO: find this a better home */
typedef struct task_watcher task_watch_t;
#endif /* CONFIG_TASKWATCH */

/* Thread tags; for easy identification. */
__options_closed_decl(thread_tag_t, uint16_t, {
	THREAD_TAG_MAINTHREAD   = 0x01,
	THREAD_TAG_CALLOUT      = 0x02,
	THREAD_TAG_IOWORKLOOP   = 0x04,
	THREAD_TAG_PTHREAD      = 0x10,
	THREAD_TAG_WORKQUEUE    = 0x20,
	THREAD_TAG_USER_JOIN    = 0x40,
});

typedef struct thread_ro *thread_ro_t;

/*!
 * @struct thread_ro
 *
 * @brief
 * A structure allocated in a read only zone that safely
 * represents the linkages of a thread to its cred, proc, task, ...
 *
 * @discussion
 * The lifetime of a @c thread_ro structure is 1:1 with that
 * of a @c thread_t or a @c uthread_t and holding a thread reference
 * always allows to dereference this structure safely.
 */
struct thread_ro {
	struct thread              *tro_owner;
#if MACH_BSD
	__xnu_struct_group(thread_ro_creds, tro_creds, {
		/*
		 * @c tro_cred holds the current thread credentials.
		 *
		 * For most threads, this is a cache of the proc's
		 * credentials that has been updated at the last
		 * syscall boundary via current_cached_proc_cred_update().
		 *
		 * If the thread assumed a different identity using settid(),
		 * then the proc cached credential lives in @c tro_realcred
		 * instead.
		 */
		struct ucred       *tro_cred;
		struct ucred       *tro_realcred;
	});
	struct proc                *tro_proc;
	struct proc_ro             *tro_proc_ro;
#endif
	struct task                *tro_task;

	struct ipc_port            *tro_self_port;
	struct ipc_port            *tro_settable_self_port;             /* send right */
	struct ipc_port            *tro_ports[THREAD_SELF_PORT_COUNT];  /* no right */

	struct exception_action    *tro_exc_actions;
};

/*
 * Flags for `thread set status`.
 */
__options_decl(thread_set_status_flags_t, uint32_t, {
	TSSF_FLAGS_NONE = 0,

	/* Translate the state to user. */
	TSSF_TRANSLATE_TO_USER = 0x01,

	/* Translate the state to user. Preserve flags */
	TSSF_PRESERVE_FLAGS = 0x02,

	/* Check kernel signed flag */
	TSSF_CHECK_USER_FLAGS = 0x04,

	/* Allow only user state PTRS */
	TSSF_ALLOW_ONLY_USER_PTRS = 0x08,

	/* Generate random diversifier and stash it */
	TSSF_RANDOM_USER_DIV = 0x10,

	/* Stash sigreturn token */
	TSSF_STASH_SIGRETURN_TOKEN = 0x20,

	/* Check sigreturn token */
	TSSF_CHECK_SIGRETURN_TOKEN = 0x40,

	/* Allow only matching sigreturn token */
	TSSF_ALLOW_ONLY_MATCHING_TOKEN = 0x80,

	/* Stash diversifier from thread */
	TSSF_THREAD_USER_DIV = 0x100,

	/* Check for entitlement */
	TSSF_CHECK_ENTITLEMENT = 0x200,
});

/*
 * Size in bits of compact thread id (ctid).
 */
#define CTID_SIZE_BIT 20
typedef uint32_t ctid_t;

#endif /* XNU_KERNEL_PRIVATE */
#ifdef MACH_KERNEL_PRIVATE

extern zone_t thread_ro_zone;

__options_decl(thread_work_interval_flags_t, uint32_t, {
	TH_WORK_INTERVAL_FLAGS_NONE            = 0x0,
#if CONFIG_SCHED_AUTO_JOIN
	/* Flags to indicate status about work interval thread is currently part of */
	TH_WORK_INTERVAL_FLAGS_AUTO_JOIN_LEAK  = 0x1,
#endif /* CONFIG_SCHED_AUTO_JOIN */
	TH_WORK_INTERVAL_FLAGS_HAS_WORKLOAD_ID = 0x2,
	TH_WORK_INTERVAL_FLAGS_RT_ALLOWED      = 0x4,
});

#if CONFIG_EXCLAVES
/* Thread exclaves interrupt-safe state bits (ORd) */
__options_decl(thread_exclaves_intstate_flags_t, uint32_t, {
	/* Thread is currently executing in secure kernel or exclaves userspace
	 * or was interrupted/preempted while doing so. */
	TH_EXCLAVES_EXECUTION                  = 0x1,
});

__options_decl(thread_exclaves_state_flags_t, uint16_t, {
/* Thread exclaves state bits (ORd) */
	/* Thread is handling RPC from a client in xnu or Darwin userspace (but
	 * may have returned to xnu due to an exclaves scheduler request or having
	 * upcalled). Must not re-enter exclaves via RPC or return to Darwin
	 * userspace. */
	TH_EXCLAVES_RPC                        = 0x1,
	/* Thread has made an upcall RPC request back into xnu while handling RPC
	 * into exclaves from a client in xnu or Darwin userspace. Must not
	 * re-enter exclaves via RPC or return to Darwin userspace. */
	TH_EXCLAVES_UPCALL                     = 0x2,
	/* Thread has made an exclaves scheduler request (such as a wait or wake)
	 * from the xnu scheduler while handling RPC into exclaves from a client in
	 * xnu or Darwin userspace. Must not re-enter exclaves via RPC or return to
	 * Darwin userspace. */
	TH_EXCLAVES_SCHEDULER_REQUEST          = 0x4,
	/* Thread is calling into xnu proxy server directly (but may have
	 * returned to xnu due to an exclaves scheduler request or having
	 * upcalled). Must not re-enter exclaves or return to Darwin userspace.
	 */
	TH_EXCLAVES_XNUPROXY                   = 0x8,
	/* Thread is calling into the exclaves scheduler directly.
	 * Must not re-enter exclaves or return to Darwin userspace.
	 */
	TH_EXCLAVES_SCHEDULER_CALL             = 0x10,
	/* Thread has called the stop upcall and once the thread returns from
	 * downcall, exit_with_reason needs to be called on the task.
	 */
	TH_EXCLAVES_STOP_UPCALL_PENDING        = 0x20,
});
#define TH_EXCLAVES_STATE_ANY           ( \
    TH_EXCLAVES_RPC               | \
    TH_EXCLAVES_UPCALL            | \
    TH_EXCLAVES_SCHEDULER_REQUEST | \
    TH_EXCLAVES_XNUPROXY          | \
    TH_EXCLAVES_SCHEDULER_CALL)

__options_decl(thread_exclaves_inspection_flags_t, uint16_t, {
	/* Thread is on Stackshot's inspection queue */
	TH_EXCLAVES_INSPECTION_STACKSHOT       = 0x1,
	/* Thread is on Kperf's inspection queue */
	TH_EXCLAVES_INSPECTION_KPERF           = 0x2,
	/* Thread must not be inspected (may deadlock, etc.) - set by collector thread*/
	TH_EXCLAVES_INSPECTION_NOINSPECT       = 0x8000,
});

#endif /* CONFIG_EXCLAVES */

typedef union thread_rr_state {
	uint32_t trr_value;
	struct {
#define TRR_FAULT_NONE     0
#define TRR_FAULT_PENDING  1
#define TRR_FAULT_OBSERVED 2
		/*
		 * Set to TRR_FAULT_PENDING with interrupts disabled
		 * by the thread when it is entering a user fault codepath.
		 *
		 * Moved to TRR_FAULT_OBSERVED from TRR_FAULT_PENDING:
		 * - by the thread if at IPI time,
		 * - or by task_restartable_ranges_synchronize() if the thread
		 *   is interrupted (under the thread lock)
		 *
		 * Cleared by the thread when returning from a user fault
		 * codepath.
		 */
		uint8_t  trr_fault_state;

		/*
		 * Set by task_restartable_ranges_synchronize()
		 * if trr_fault_state is TRR_FAULT_OBSERVED
		 * and a rendez vous at the AST is required.
		 *
		 * Set atomically if trr_fault_state == TRR_FAULT_OBSERVED,
		 * and trr_ipi_ack_pending == 0
		 */
		uint8_t  trr_sync_waiting;

		/*
		 * Updated under the thread_lock(),
		 * set by task_restartable_ranges_synchronize()
		 * when the thread was IPIed and the caller is waiting
		 * for an ACK.
		 */
		uint16_t trr_ipi_ack_pending;
	};
} thread_rr_state_t;

struct thread {
#if MACH_ASSERT
#define THREAD_MAGIC 0x1234ABCDDCBA4321ULL
	/* Ensure nothing uses &thread as a queue entry */
	uint64_t                thread_magic;
#endif /* MACH_ASSERT */

	/*
	 *	NOTE:	The runq field in the thread structure has an unusual
	 *	locking protocol.  If its value is PROCESSOR_NULL, then it is
	 *	locked by the thread_lock, but if its value is something else
	 *	then it is locked by the associated run queue lock. It is
	 *	set to PROCESSOR_NULL without holding the thread lock, but the
	 *	transition from PROCESSOR_NULL to non-null must be done
	 *	under the thread lock and the run queue lock. To enforce the
	 *	protocol, runq should only be accessed using the
	 *	thread_get/set/clear_runq functions and locked variants below.
	 *
	 *	New waitq APIs allow the 'links' and '__runq' fields to be
	 *	anywhere in the thread structure.
	 */
	union {
		queue_chain_t                   runq_links;             /* run queue links */
		queue_chain_t                   wait_links;             /* wait queue links */
		struct mpsc_queue_chain         mpsc_links;             /* thread daemon mpsc links */
		struct priority_queue_entry_sched wait_prioq_links;       /* priority ordered waitq links */
	};

	event64_t               wait_event;     /* wait queue event */
	struct { processor_t    runq; } __runq; /* internally managed run queue assignment, see above comment */
	waitq_t                 waitq;          /* wait queue this thread is enqueued on */
	struct turnstile       *turnstile;      /* thread's turnstile, protected by primitives interlock */
	void                   *inheritor;      /* inheritor of the primitive the thread will block on */
	struct priority_queue_sched_max sched_inheritor_queue; /* Inheritor queue for kernel promotion */
	struct priority_queue_sched_max base_inheritor_queue; /* Inheritor queue for user promotion */

#if CONFIG_SCHED_EDGE
	bool            th_bound_cluster_enqueued;
	bool            th_shared_rsrc_enqueued[CLUSTER_SHARED_RSRC_TYPE_COUNT];
	bool            th_shared_rsrc_heavy_user[CLUSTER_SHARED_RSRC_TYPE_COUNT];
	bool            th_shared_rsrc_heavy_perf_control[CLUSTER_SHARED_RSRC_TYPE_COUNT];
#endif /* CONFIG_SCHED_EDGE */

#if CONFIG_SCHED_CLUTCH
	/*
	 * In the clutch scheduler, the threads are maintained in runqs at the clutch_bucket
	 * level (clutch_bucket defines a unique thread group and scheduling bucket pair). The
	 * thread is linked via a couple of linkages in the clutch bucket:
	 *
	 * - A stable priority queue linkage which is the main runqueue (based on sched_pri) for the clutch bucket
	 * - A regular priority queue linkage which is based on thread's base/promoted pri (used for clutch bucket priority calculation)
	 * - A queue linkage used for timesharing operations of threads at the scheduler tick
	 */
	struct priority_queue_entry_stable      th_clutch_runq_link;
	struct priority_queue_entry_sched       th_clutch_pri_link;
	queue_chain_t                           th_clutch_timeshare_link;
#endif /* CONFIG_SCHED_CLUTCH */

	/* Data updated during assert_wait/thread_wakeup */
	decl_simple_lock_data(, sched_lock);     /* scheduling lock (thread_lock()) */
	decl_simple_lock_data(, wake_lock);      /* for thread stop / wait (wake_lock()) */
	uint16_t                options;        /* options set by thread itself */
#define TH_OPT_INTMASK          0x0003          /* interrupt / abort level */
#define TH_OPT_VMPRIV           0x0004          /* may allocate reserved memory */
#define TH_OPT_SYSTEM_CRITICAL  0x0010          /* Thread must always be allowed to run - even under heavy load */
#define TH_OPT_PROC_CPULIMIT    0x0020          /* Thread has a task-wide CPU limit applied to it */
#define TH_OPT_PRVT_CPULIMIT    0x0040          /* Thread has a thread-private CPU limit applied to it */
#define TH_OPT_IDLE_THREAD      0x0080          /* Thread is a per-processor idle thread */
#define TH_OPT_GLOBAL_FORCED_IDLE       0x0100  /* Thread performs forced idle for thermal control */
#define TH_OPT_SCHED_VM_GROUP   0x0200          /* Thread belongs to special scheduler VM group */
#define TH_OPT_HONOR_QLIMIT     0x0400          /* Thread will honor qlimit while sending mach_msg, regardless of MACH_SEND_ALWAYS */
#define TH_OPT_SEND_IMPORTANCE  0x0800          /* Thread will allow importance donation from kernel rpc */
#define TH_OPT_ZONE_PRIV        0x1000          /* Thread may use the zone replenish reserve */
#define TH_OPT_IPC_TG_BLOCKED   0x2000          /* Thread blocked in sync IPC and has made the thread group blocked callout */
#define TH_OPT_FORCED_LEDGER    0x4000          /* Thread has a forced CPU limit  */
#define TH_IN_MACH_EXCEPTION    0x8000          /* Thread is currently handling a mach exception */

	bool                    wake_active;    /* wake event on stop */
	bool                    at_safe_point;  /* thread_abort_safely allowed */
	uint8_t                 sched_saved_run_weight;
#if DEVELOPMENT || DEBUG
	bool                    pmap_footprint_suspended;
#endif /* DEVELOPMENT || DEBUG */


	ast_t                   reason;         /* why we blocked */
	uint32_t                quantum_remaining;
	wait_result_t           wait_result;    /* outcome of wait -
	                                        * may be examined by this thread
	                                        * WITHOUT locking */
	thread_rr_state_t       t_rr_state;     /* state for restartable ranges */
	thread_continue_t       continuation;   /* continue here next dispatch */
	void                   *parameter;      /* continuation parameter */

	/* Data updated/used in thread_invoke */
	vm_offset_t             kernel_stack;   /* current kernel stack */
	vm_offset_t             reserved_stack; /* reserved kernel stack */

	/*** Machine-dependent state ***/
	struct machine_thread   machine;

#if KASAN
	struct kasan_thread_data kasan_data;
#endif
#if CONFIG_KCOV
	kcov_thread_data_t       kcov_data;
#endif

	/* Thread state: */
	int                     state;
/*
 *	Thread states [bits or'ed]
 * All but TH_WAIT_REPORT are encoded in SS_TH_FLAGS
 * All are encoded in kcdata.py ('ths_state')
 */
#define TH_WAIT                 0x01            /* queued for waiting */
#define TH_SUSP                 0x02            /* stopped or requested to stop */
#define TH_RUN                  0x04            /* running or on runq */
#define TH_UNINT                0x08            /* waiting uninteruptibly */
#define TH_TERMINATE            0x10            /* halted at termination */
#define TH_TERMINATE2           0x20            /* added to termination queue */
#define TH_WAIT_REPORT          0x40            /* the wait is using the sched_call,
	                                        * only set if TH_WAIT is also set */
#define TH_IDLE                 0x80            /* idling processor */
#define TH_WAKING              0x100            /* between waitq remove and thread_go */

	/* Scheduling information */
	sched_mode_t            sched_mode;     /* scheduling mode */
	sched_mode_t            saved_mode;     /* saved mode during forced mode demotion */

	/* This thread's contribution to global sched counters */
	sched_bucket_t          th_sched_bucket;

	sfi_class_id_t          sfi_class;      /* SFI class (XXX Updated on CSW/QE/AST) */
	sfi_class_id_t          sfi_wait_class; /* Currently in SFI wait for this class, protected by sfi_lock */

	uint32_t                sched_flags;            /* current flag bits */
#define TH_SFLAG_NO_SMT                 0x0001          /* On an SMT CPU, this thread must be scheduled alone */
#define TH_SFLAG_FAILSAFE               0x0002          /* fail-safe has tripped */
#define TH_SFLAG_THROTTLED              0x0004          /* throttled thread forced to timeshare mode (may be applied in addition to failsafe) */

#define TH_SFLAG_PROMOTED               0x0008          /* sched pri has been promoted by kernel mutex priority promotion */
#define TH_SFLAG_ABORT                  0x0010          /* abort interruptible waits */
#define TH_SFLAG_ABORTSAFELY            0x0020          /* ... but only those at safe point */
#define TH_SFLAG_ABORTED_MASK           (TH_SFLAG_ABORT | TH_SFLAG_ABORTSAFELY)
#define TH_SFLAG_DEPRESS                0x0040          /* normal depress yield */
#define TH_SFLAG_POLLDEPRESS            0x0080          /* polled depress yield */
#define TH_SFLAG_DEPRESSED_MASK         (TH_SFLAG_DEPRESS | TH_SFLAG_POLLDEPRESS)
/* unused TH_SFLAG_PRI_UPDATE           0x0100 */
#define TH_SFLAG_EAGERPREEMPT           0x0200          /* Any preemption of this thread should be treated as if AST_URGENT applied */
#define TH_SFLAG_RW_PROMOTED            0x0400          /* promote reason: blocking with RW lock held */
#define TH_SFLAG_BASE_PRI_FROZEN        0x0800          /* (effective) base_pri is frozen */
#define TH_SFLAG_WAITQ_PROMOTED         0x1000          /* promote reason: waitq wakeup (generally for IPC receive) */

#if __AMP__
#define TH_SFLAG_ECORE_ONLY             0x2000          /* (unused) Bind thread to E core processor set */
#define TH_SFLAG_PCORE_ONLY             0x4000          /* (unused) Bind thread to P core processor set */
#endif

#define TH_SFLAG_EXEC_PROMOTED          0x8000          /* promote reason: thread is in an exec */

#define TH_SFLAG_THREAD_GROUP_AUTO_JOIN 0x10000         /* thread has been auto-joined to thread group */
#if __AMP__
#define TH_SFLAG_BOUND_SOFT             0x20000         /* thread is soft bound to a cluster; can run anywhere if bound cluster unavailable */
#endif /* __AMP__ */

#if CONFIG_PREADOPT_TG
#define TH_SFLAG_REEVALUTE_TG_HIERARCHY_LATER 0x40000   /* thread needs to reevaluate its TG hierarchy */
#endif

#define TH_SFLAG_FLOOR_PROMOTED               0x80000   /* promote reason: boost requested */

/* 'promote reasons' that request a priority floor only, not a custom priority */
#define TH_SFLAG_PROMOTE_REASON_MASK    (TH_SFLAG_RW_PROMOTED | TH_SFLAG_WAITQ_PROMOTED | TH_SFLAG_EXEC_PROMOTED | TH_SFLAG_FLOOR_PROMOTED)

#define TH_SFLAG_RT_DISALLOWED         0x100000         /* thread wants RT but may not have joined a work interval that allows it */
#define TH_SFLAG_DEMOTED_MASK      (TH_SFLAG_THROTTLED | TH_SFLAG_FAILSAFE | TH_SFLAG_RT_DISALLOWED)     /* saved_mode contains previous sched_mode */
#define TH_SFLAG_RT_CPULIMIT         0x200000           /* thread should have a CPU limit applied. */

	int16_t                 sched_pri;              /* scheduled (current) priority */
	int16_t                 base_pri;               /* effective base priority (equal to req_base_pri unless TH_SFLAG_BASE_PRI_FROZEN) */
	int16_t                 req_base_pri;           /* requested base priority */
	int16_t                 max_priority;           /* copy of max base priority */
	int16_t                 task_priority;          /* copy of task base priority */
	int16_t                 promotion_priority;     /* priority thread is currently promoted to */
	uint16_t                priority_floor_count;   /* number of push to boost the floor priority */
	int16_t                 suspend_count;          /* Kernel holds on this thread  */

	int                     iotier_override;        /* atomic operations to set, cleared on ret to user */
	os_ref_atomic_t         ref_count;              /* number of references to me */

	uint32_t                rwlock_count;           /* Number of lck_rw_t locks held by thread */
	struct smrq_slist_head  smr_stack;
#ifdef DEBUG_RW
	rw_lock_debug_t         rw_lock_held;           /* rw_locks currently held by the thread */
#endif /* DEBUG_RW */

	integer_t               importance;             /* task-relative importance */

	/* Priority depression expiration */
	integer_t               depress_timer_active;
	timer_call_t            depress_timer;

	/* real-time parameters */
	struct {                                        /* see mach/thread_policy.h */
		uint32_t            period;
		uint32_t            computation;
		uint32_t            constraint;
		bool                preemptible;
		uint8_t             priority_offset;   /* base_pri = BASEPRI_RTQUEUES + priority_offset */
		uint64_t            deadline;
	}                       realtime;

	uint64_t                last_run_time;          /* time when thread was switched away from */
	uint64_t                last_made_runnable_time;        /* time when thread was unblocked or preempted */
	uint64_t                last_basepri_change_time;       /* time when thread was last changed in basepri while runnable */
	uint64_t                same_pri_latency;
	/*
	 * workq_quantum_deadline is the workq thread's next runtime deadline. This
	 * value is set to 0 if the thread has no such deadline applicable to it.
	 *
	 * The synchronization for this field is due to how this field is modified
	 * 1) This field is always modified on the thread by itself or on the thread
	 * when it is not running/runnable
	 * 2) Change of this field is immediately followed by a
	 * corresponding change to the AST_KEVENT to either set or clear the
	 * AST_KEVENT_WORKQ_QUANTUM_EXPIRED bit
	 *
	 * workq_quantum_deadline can be modified by the thread on itself during
	 * interrupt context. However, due to (2) and due to the fact that the
	 * change to the AST_KEVENT is volatile, this forces the compiler to
	 * guarantee the order between the write to workq_quantum_deadline and the
	 * kevent field and therefore guarantees the correct synchronization.
	 */
	uint64_t                workq_quantum_deadline;

#if WORKQ_QUANTUM_HISTORY_DEBUG

#define WORKQ_QUANTUM_HISTORY_COUNT 16
	struct workq_quantum_history {
		uint64_t time;
		uint64_t deadline;
		bool arm;
	} workq_quantum_history[WORKQ_QUANTUM_HISTORY_COUNT];
	uint64_t workq_quantum_history_index;

#define WORKQ_QUANTUM_HISTORY_WRITE_ENTRY(thread, ...)  ({\
	        thread_t __th = (thread); \
	        uint64_t __index = os_atomic_inc_orig(&thread->workq_quantum_history_index, relaxed); \
	        struct workq_quantum_history _wq_quantum_history = { mach_approximate_time(), __VA_ARGS__}; \
	        __th->workq_quantum_history[__index % WORKQ_QUANTUM_HISTORY_COUNT] = \
	                        (struct workq_quantum_history) _wq_quantum_history; \
	})
#else /* WORKQ_QUANTUM_HISTORY_DEBUG */
#define WORKQ_QUANTUM_HISTORY_WRITE_ENTRY(thread, ...)
#endif /* WORKQ_QUANTUM_HISTORY_DEBUG */

#define THREAD_NOT_RUNNABLE (~0ULL)

#if CONFIG_THREAD_GROUPS
	struct thread_group     *thread_group;
#endif

#if defined(CONFIG_SCHED_MULTIQ)
	sched_group_t           sched_group;
#endif /* defined(CONFIG_SCHED_MULTIQ) */

	/* Data used during setrun/dispatch */
	processor_t             bound_processor;        /* bound to a processor? */
	processor_t             last_processor;         /* processor last dispatched on */
	processor_t             chosen_processor;       /* Where we want to run this thread */

	/* Fail-safe computation since last unblock or qualifying yield */
	uint64_t                computation_metered;
	uint64_t                computation_epoch;
	uint64_t                computation_interrupt_epoch;
	uint64_t                safe_release;           /* when to release fail-safe */

	/* Call out from scheduler */
	void                  (*sched_call)(int type, thread_t thread);

#if defined(CONFIG_SCHED_PROTO)
	uint32_t                runqueue_generation;    /* last time runqueue was drained */
#endif

	/* Statistics and timesharing calculations */
#if defined(CONFIG_SCHED_TIMESHARE_CORE)
	natural_t               sched_stamp;            /* last scheduler tick */
	natural_t               sched_usage;            /* timesharing cpu usage [sched] */
	natural_t               pri_shift;              /* usage -> priority from pset */
	natural_t               cpu_usage;              /* instrumented cpu usage [%cpu] */
	natural_t               cpu_delta;              /* accumulated cpu_usage delta */
#endif /* CONFIG_SCHED_TIMESHARE_CORE */

	uint32_t                c_switch;               /* total context switches */
	uint32_t                p_switch;               /* total processor switches */
	uint32_t                ps_switch;              /* total pset switches */

	/* Timing data structures */
	uint64_t                sched_time_save;        /* saved time for scheduler tick */
	uint64_t                vtimer_user_save;       /* saved values for vtimers */
	uint64_t                vtimer_prof_save;
	uint64_t                vtimer_rlim_save;
	uint64_t                vtimer_qos_save;

	timer_data_t            runnable_timer;         /* time the thread is runnable (including running) */

	struct recount_thread   th_recount;             /* resource accounting */

#if CONFIG_SCHED_SFI
	/* Timing for wait state */
	uint64_t                wait_sfi_begin_time;    /* start time for thread waiting in SFI */
#endif

	/*
	 * Processor/cache affinity
	 * - affinity_threads links task threads with the same affinity set
	 */
	queue_chain_t           affinity_threads;
	affinity_set_t          affinity_set;

#if CONFIG_TASKWATCH
	task_watch_t           *taskwatch;              /* task watch */
#endif /* CONFIG_TASKWATCH */

	/* Various bits of state to stash across a continuation, exclusive to the current thread block point */
	union {
		struct {
			mach_msg_return_t       state;          /* receive state */
			mach_port_seqno_t       seqno;          /* seqno of recvd message */
			ipc_object_t            object;         /* object received on */
			mach_vm_address_t       msg_addr;       /* receive msg buffer pointer */
			mach_vm_address_t       aux_addr;       /* receive aux buffer pointer */
			mach_msg_size_t         max_msize;      /* max rcv size for msg */
			mach_msg_size_t         max_asize;      /* max rcv size for aux data */
			mach_msg_size_t         msize;          /* actual size for the msg */
			mach_msg_size_t         asize;          /* actual size for aux data */
			mach_msg_option64_t     option;         /* 64 bits options for receive */
			mach_port_name_t        receiver_name;  /* the receive port name */
			union {
				struct ipc_kmsg   *XNU_PTRAUTH_SIGNED_PTR("thread.ith_kmsg")  kmsg;  /* received message */
#if MACH_FLIPC
				struct ipc_mqueue *XNU_PTRAUTH_SIGNED_PTR("thread.ith_peekq") peekq; /* mqueue to peek at */
#endif /* MACH_FLIPC */
			};
		} receive;
		struct {
			struct semaphore        *waitsemaphore;         /* semaphore ref */
			struct semaphore        *signalsemaphore;       /* semaphore ref */
			int                     options;                /* semaphore options */
			kern_return_t           result;                 /* primary result */
			mach_msg_continue_t continuation;
		} sema;
		struct {
#define THREAD_SAVE_IOKIT_TLS_COUNT     8
			void                    *tls[THREAD_SAVE_IOKIT_TLS_COUNT];
		} iokit;
	} saved;

	/* Only user threads can cause guard exceptions, only kernel threads can be thread call threads */
	union {
		/* Thread call thread's state structure, stored on its stack */
		struct thread_call_thread_state *thc_state;

		/* Structure to save information about guard exception */
		struct {
			mach_exception_code_t           code;
			mach_exception_subcode_t        subcode;
		} guard_exc_info;
	};

	/* User level suspensions */
	int32_t                 user_stop_count;

	/* IPC data structures */
#if IMPORTANCE_INHERITANCE
	natural_t ith_assertions;                       /* assertions pending drop */
#endif
	circle_queue_head_t     ith_messages;           /* messages to reap */
	mach_port_t             ith_kernel_reply_port;  /* reply port for kernel RPCs */

	/* Ast/Halt data structures */
	vm_offset_t             recover;                /* page fault recover(copyin/out) */

	queue_chain_t           threads;                /* global list of all threads */

	/* Activation */
	queue_chain_t           task_threads;

	/* Task membership */
#if __x86_64__ || __arm__
	struct task            *t_task;
#endif
	struct thread_ro       *t_tro;
	vm_map_t                map;
	thread_t                handoff_thread;

	/* Timed wait expiration */
	timer_call_t            wait_timer;
	uint16_t                wait_timer_active; /* is the call running */
	bool                    wait_timer_armed; /* should the wait be cleared */

	/* Miscellaneous bits guarded by mutex */
	uint32_t
	    active:1,           /* Thread is active and has not been terminated */
	    ipc_active:1,       /* IPC with the thread ports is allowed */
	    started:1,          /* Thread has been started after creation */
	    static_param:1,     /* Disallow policy parameter changes */
	    inspection:1,       /* TRUE when task is being inspected by crash reporter */
	    policy_reset:1,     /* Disallow policy parameter changes on terminating threads */
	    suspend_parked:1,   /* thread parked in thread_suspended */
	    corpse_dup:1,       /* TRUE when thread is an inactive duplicate in a corpse */
	:0;

	/* Pending thread ast(s) */
	os_atomic(ast_t)        ast;

	decl_lck_mtx_data(, mutex);

	struct ipc_port         *ith_special_reply_port;   /* ref to special reply port */

#if CONFIG_DTRACE
	uint16_t                t_dtrace_flags;         /* DTrace thread states */
#define TH_DTRACE_EXECSUCCESS   0x01
	uint16_t                t_dtrace_inprobe;       /* Executing under dtrace_probe */
	uint32_t                t_dtrace_predcache;     /* DTrace per thread predicate value hint */
	int64_t                 t_dtrace_tracing;       /* Thread time under dtrace_probe() */
	int64_t                 t_dtrace_vtime;
#endif

	clock_sec_t             t_page_creation_time;
	uint32_t                t_page_creation_count;
	uint32_t                t_page_creation_throttled;
#if (DEVELOPMENT || DEBUG)
	uint64_t                t_page_creation_throttled_hard;
	uint64_t                t_page_creation_throttled_soft;
#endif /* DEVELOPMENT || DEBUG */
	int                     t_pagein_error;         /* for vm_fault(), holds error from vnop_pagein() */

	mach_port_name_t        ith_voucher_name;
	ipc_voucher_t           ith_voucher;

#ifdef KPERF
/* The high 8 bits are the number of frames to sample of a user callstack. */
#define T_KPERF_CALLSTACK_DEPTH_OFFSET     (24)
#define T_KPERF_SET_CALLSTACK_DEPTH(DEPTH) (((uint32_t)(DEPTH)) << T_KPERF_CALLSTACK_DEPTH_OFFSET)
#define T_KPERF_GET_CALLSTACK_DEPTH(FLAGS) ((FLAGS) >> T_KPERF_CALLSTACK_DEPTH_OFFSET)
#define T_KPERF_ACTIONID_OFFSET            (18)
#define T_KPERF_SET_ACTIONID(AID)          (((uint32_t)(AID)) << T_KPERF_ACTIONID_OFFSET)
#define T_KPERF_GET_ACTIONID(FLAGS)        ((FLAGS) >> T_KPERF_ACTIONID_OFFSET)
#endif

#define T_KPERF_AST_CALLSTACK 0x1 /* dump a callstack on thread's next AST */
#define T_KPERF_AST_DISPATCH  0x2 /* dump a name on thread's next AST */
#define T_KPC_ALLOC           0x4 /* thread needs a kpc_buf allocated */

#define T_KPERF_AST_ALL \
    (T_KPERF_AST_CALLSTACK | T_KPERF_AST_DISPATCH | T_KPC_ALLOC)
/* only go up to T_KPERF_ACTIONID_OFFSET - 1 */

#ifdef KPERF
	uint32_t                kperf_ast;
	uint32_t                kperf_pet_gen;  /* last generation of PET that sampled this thread*/
	uint32_t                kperf_c_switch; /* last dispatch detection */
	uint32_t                kperf_pet_cnt;  /* how many times a thread has been sampled by PET */
#if CONFIG_EXCLAVES
	uint32_t                kperf_exclaves_ast;
#endif
#endif

#ifdef CONFIG_CPU_COUNTERS
	/* accumulated performance counters for this thread */
	uint64_t               *kpc_buf;
#endif /* CONFIG_CPU_COUNTERS */

#if HYPERVISOR
	/* hypervisor virtual CPU object associated with this thread */
	void                   *hv_thread_target;
#endif /* HYPERVISOR */

	/* Statistics accumulated per-thread and aggregated per-task */
	uint32_t                syscalls_unix;
	uint32_t                syscalls_mach;
	ledger_t                t_ledger;
	ledger_t                t_threadledger; /* per thread ledger */
	ledger_t                t_bankledger;                /* ledger to charge someone */
	uint64_t                t_deduct_bank_ledger_time;   /* cpu time to be deducted from bank ledger */
	uint64_t                t_deduct_bank_ledger_energy; /* energy to be deducted from bank ledger */

	uint64_t                thread_id;             /* system wide unique thread-id */
	uint32_t                ctid;                  /* system wide compact thread-id */
	uint32_t                ctsid;                 /* this thread ts ID */

	/* policy is protected by the thread mutex */
	struct thread_requested_policy  requested_policy;
	struct thread_effective_policy  effective_policy;

	/* usynch override is protected by the task lock, eventually will be thread mutex */
	struct thread_qos_override {
		struct thread_qos_override      *override_next;
		uint32_t        override_contended_resource_count;
		int16_t         override_qos;
		int16_t         override_resource_type;
		user_addr_t     override_resource;
	} *overrides;

	uint32_t                kevent_overrides;
	uint8_t                 user_promotion_basepri;
	uint8_t                 kern_promotion_schedpri;
	_Atomic uint16_t        kevent_ast_bits;

	io_stat_info_t          thread_io_stats; /* per-thread I/O statistics */

	uint32_t                thread_callout_interrupt_wakeups;
	uint32_t                thread_callout_platform_idle_wakeups;
	uint32_t                thread_timer_wakeups_bin_1;
	uint32_t                thread_timer_wakeups_bin_2;
	thread_tag_t            thread_tag;

	/*
	 * callout_* fields are only set for thread call threads whereas guard_exc_fatal is set
	 * by user threads on themselves while taking a guard exception. So it's okay for them to
	 * share this bitfield.
	 */
	uint16_t
	    callout_woken_from_icontext:1,
	    callout_woken_from_platform_idle:1,
	    callout_woke_thread:1,
	    guard_exc_fatal:1,
	    thread_bitfield_unused:12;

#define THREAD_BOUND_CLUSTER_NONE       (UINT32_MAX)
	uint32_t                 th_bound_cluster_id;

#if CONFIG_THREAD_GROUPS
#if CONFIG_PREADOPT_TG
	/* The preadopt thread group is set on the thread
	 *
	 *   a) By another thread when it is a creator and it is scheduled with the
	 *   thread group on the TR
	 *   b) On itself when it binds a thread request and becomes a
	 *   servicer or when it rebinds to the thread request
	 *   c) On itself when it processes knotes and finds the first
	 *   EVFILT_MACHPORT event to deliver to userspace
	 *
	 * Note that this is a full reference owned by the thread_t and not a
	 * borrowed reference.
	 *
	 * This reference is cleared from the thread_t by the thread itself at the
	 * following times:
	 *   a) When it explicitly adopts a work interval or a bank voucher
	 *   b) If it still exists on the thread, after it has unbound and is about
	 *   to park
	 *   c) During thread termination if one still exists
	 *   d) When a different preadoption thread group is set on the thread
	 *
	 * It is modified under the thread lock.
	 */
	struct thread_group     *preadopt_thread_group;

	/* This field here is present in order to make sure that the t->thread_group
	 * is always pointing to a valid thread group and isn't a dangling pointer.
	 *
	 * Consider the following scenario:
	 *	a) t->thread_group points to the preadoption thread group
	 *	b) The preadoption thread group is modified on the thread but we are
	 *	unable to resolve the hierarchy immediately due to the current state of
	 *	the thread
	 *
	 *	In order to make sure that t->thread_group points to a valid thread
	 *	group until we can resolve the hierarchy again, we save the existing
	 *	thread_group it points to in old_preadopt_thread_group. The next time a
	 *	hierarchy resolution is done, we know that t->thread_group will not point
	 *	to this field anymore so we can clear it.
	 *
	 *	 This field is always going to take the reference that was previously in
	 *	 preadopt_thread_group so it will have a full +1
	 */
	struct thread_group     *old_preadopt_thread_group;
#endif /* CONFIG_PREADOPT_TG */

	/* This is a borrowed reference to the TG from the ith_voucher and is saved
	 * here since we may not always be in the right context to able to do the
	 * lookups.
	 *
	 * It is set always set on self under the thread lock */
	struct thread_group     *bank_thread_group;

	/*  Whether this is the autojoin thread group or the work interval thread
	 *  group depends on whether the thread's sched_flags has the
	 *  TH_SFLAG_THREAD_GROUP_AUTO_JOIN bit set */
	union {
		/* This is a borrowed reference to the auto join thread group from the
		 * work_interval. It is set with the thread lock held */
		struct thread_group             *auto_join_thread_group;
		/* This is a borrowed reference to the explicit work_interval thread group
		 * and is always set on self */
		struct thread_group             *work_interval_thread_group;
	};
#endif /* CONFIG_THREAD_GROUPS */

	/* work interval (if any) associated with the thread. Only modified by
	 * current thread on itself or when another thread when the thread is held
	 * off of runq */
	struct work_interval            *th_work_interval;
	thread_work_interval_flags_t    th_work_interval_flags;

#if SCHED_TRACE_THREAD_WAKEUPS
	uintptr_t               thread_wakeup_bt[64];
#endif
	turnstile_update_flags_t inheritor_flags; /* inheritor flags for inheritor field */
	block_hint_t            pending_block_hint;
	block_hint_t            block_hint;      /* What type of primitive last caused us to block. */
	uint32_t                decompressions;  /* Per-thread decompressions counter to be added to per-task decompressions counter */
	int                     thread_region_page_shift; /* Page shift that this thread would like to use when */
	                                                  /* introspecting a task. This is currently being used */
	                                                  /* by footprint which uses a thread for each task being inspected. */
#if CONFIG_SCHED_RT_ALLOW
	/* Used when a thread is requested to set/clear its own CPU limit */
	uint32_t
	    t_ledger_req_action:2,
	    t_ledger_req_percentage:7,
	    t_ledger_req_interval_ms:16,
	:0;
#endif /* CONFIG_SCHED_RT_ALLOW */

#if CONFIG_IOSCHED
	void                   *decmp_upl;
#endif /* CONFIG_IOSCHED */
	struct knote            *ith_knote;         /* knote fired for rcv */

#if CONFIG_SPTM
	/* TXM thread stack associated with this thread */
	uintptr_t               txm_thread_stack;
#endif

#if CONFIG_EXCLAVES
	/* Per-thread IPC buffer for exclaves communication. Only modified by the
	 * current thread on itself. */
	void                    *th_exclaves_ipc_buffer;
	/* Exclaves scheduling context ID corresponding to IPC buffer, communicated
	 * to the exclaves scheduler component. Only modified by the current
	 * thread on itself. */
	uint64_t                th_exclaves_scheduling_context_id;
	/* Thread exclaves interrupt-safe state. Only mutated by the current thread
	 * on itself with interrupts disabled, and only ever read by the current
	 * thread (with no locking), including from interrupt context, or during
	 * debug/stackshot. */
	thread_exclaves_intstate_flags_t th_exclaves_intstate;
	/* Thread exclaves state. Only mutated by the current thread on itself, and
	 * only ever read by the current thread (with no locking). Unsafe to read
	 * from interrupt context. */
	thread_exclaves_state_flags_t th_exclaves_state;
	/* Thread stackshot state. Prevents returning to Exclave world until after
	 * an external agent has triggered inspection (likely via Exclave stackshot),
	 * and woken this thread. */
	thread_exclaves_inspection_flags_t _Atomic th_exclaves_inspection_state;
	/* Task for which conclave teardown is being called by this thread. Used
	 * for context by conclave crash info upcall to find the task for appending
	 * the conclave crash info. */
	task_t conclave_stop_task;
	/* Queue of threads being inspected by Stackshot.
	 * Modified under exclaves_collect_mtx. */
	queue_chain_t th_exclaves_inspection_queue_stackshot;
	/* Queue of threads being inspected by kperf.
	 * Modified under exclaves_collect_mtx. */
	queue_chain_t th_exclaves_inspection_queue_kperf;
#endif /* CONFIG_EXCLAVES */
};

#define ith_state           saved.receive.state
#define ith_seqno           saved.receive.seqno
#define ith_object          saved.receive.object
#define ith_msg_addr        saved.receive.msg_addr
#define ith_aux_addr        saved.receive.aux_addr
#define ith_max_msize       saved.receive.max_msize
#define ith_max_asize       saved.receive.max_asize
#define ith_msize           saved.receive.msize
#define ith_asize           saved.receive.asize
#define ith_option          saved.receive.option
#define ith_receiver_name   saved.receive.receiver_name
#define ith_kmsg            saved.receive.kmsg
#if MACH_FLIPC
#define ith_peekq           saved.receive.peekq
#endif /* MACH_FLIPC */

#define sth_waitsemaphore   saved.sema.waitsemaphore
#define sth_signalsemaphore saved.sema.signalsemaphore
#define sth_options         saved.sema.options
#define sth_result          saved.sema.result
#define sth_continuation    saved.sema.continuation

#define ITH_KNOTE_NULL      ((void *)NULL)
#define ITH_KNOTE_PSEUDO    ((void *)0xdeadbeef)
/*
 * The ith_knote is used during message delivery, and can safely be interpreted
 * only when used for one of these codepaths, which the test for the msgt_name
 * being RECEIVE or SEND_ONCE is about.
 */
#define ITH_KNOTE_VALID(kn, msgt_name) \
	        (((kn) != ITH_KNOTE_NULL && (kn) != ITH_KNOTE_PSEUDO) && \
	         ((msgt_name) == MACH_MSG_TYPE_PORT_RECEIVE || \
	         (msgt_name) == MACH_MSG_TYPE_PORT_SEND_ONCE))

#if MACH_ASSERT
#define assert_thread_magic(thread) assertf((thread)->thread_magic == THREAD_MAGIC, \
	                                    "bad thread magic 0x%llx for thread %p, expected 0x%llx", \
	                                    (thread)->thread_magic, (thread), THREAD_MAGIC)
#else
#define assert_thread_magic(thread) do { (void)(thread); } while (0)
#endif

extern thread_t         thread_bootstrap(void);

extern void             thread_machine_init_template(void);

extern void             thread_init(void);

extern void             thread_daemon_init(void);

extern void             thread_reference(
	thread_t                thread);

extern void             thread_deallocate(
	thread_t                thread);

extern void             thread_inspect_deallocate(
	thread_inspect_t        thread);

extern void             thread_read_deallocate(
	thread_read_t           thread);

extern void             thread_terminate_self(void);

extern kern_return_t    thread_terminate_internal(
	thread_t                thread);

extern void             thread_start(
	thread_t                thread) __attribute__ ((noinline));

extern void             thread_start_in_assert_wait(
	thread_t                thread,
	struct waitq           *waitq,
	event64_t               event,
	wait_interrupt_t        interruptible) __attribute__ ((noinline));

extern void             thread_terminate_enqueue(
	thread_t                thread);

extern void             thread_exception_enqueue(
	task_t                  task,
	thread_t                thread,
	exception_type_t        etype);

extern void             thread_backtrace_enqueue(
	kcdata_object_t         obj,
	exception_port_t        ports[static BT_EXC_PORTS_COUNT],
	exception_type_t        etype);

extern void                     thread_copy_resource_info(
	thread_t dst_thread,
	thread_t src_thread);

extern void                     thread_terminate_crashed_threads(void);

extern void                     thread_stack_enqueue(
	thread_t                thread);

extern void                     thread_hold(
	thread_t        thread);

extern void                     thread_release(
	thread_t        thread);

extern void                     thread_corpse_continue(void) __dead2;

extern boolean_t                thread_is_active(thread_t thread);

extern lck_grp_t                thread_lck_grp;

/* Locking for scheduler state, always acquired with interrupts disabled (splsched()) */
#define thread_lock_init(th)    simple_lock_init(&(th)->sched_lock, 0)
#define thread_lock(th)                 simple_lock(&(th)->sched_lock, &thread_lck_grp)
#define thread_unlock(th)               simple_unlock(&(th)->sched_lock)
#define thread_lock_assert(th, x)       simple_lock_assert(&(th)->sched_lock, (x))

#define wake_lock_init(th)              simple_lock_init(&(th)->wake_lock, 0)
#define wake_lock(th)                   simple_lock(&(th)->wake_lock, &thread_lck_grp)
#define wake_unlock(th)                 simple_unlock(&(th)->wake_lock)

#define thread_should_halt_fast(thread)         (!(thread)->active)

extern void                             stack_alloc(
	thread_t                thread);

extern void                     stack_handoff(
	thread_t                from,
	thread_t                to);

extern void                             stack_free(
	thread_t                thread);

extern void                             stack_free_reserved(
	thread_t                thread);

extern boolean_t                stack_alloc_try(
	thread_t            thread);

extern void                             stack_collect(void);

extern kern_return_t    thread_info_internal(
	thread_t                                thread,
	thread_flavor_t                 flavor,
	thread_info_t                   thread_info_out,
	mach_msg_type_number_t  *thread_info_count);

extern kern_return_t    kernel_thread_create(
	thread_continue_t       continuation,
	void                            *parameter,
	integer_t                       priority,
	thread_t                        *new_thread);

extern kern_return_t    kernel_thread_start_priority(
	thread_continue_t       continuation,
	void                            *parameter,
	integer_t                       priority,
	thread_t                        *new_thread);

extern void                             machine_stack_attach(
	thread_t                thread,
	vm_offset_t             stack);

extern vm_offset_t              machine_stack_detach(
	thread_t                thread);

extern void                             machine_stack_handoff(
	thread_t                old,
	thread_t                new);

extern thread_t                 machine_switch_context(
	thread_t                        old_thread,
	thread_continue_t       continuation,
	thread_t                        new_thread);

extern void                             machine_load_context(
	thread_t                thread) __attribute__((noreturn));

extern void             machine_thread_state_initialize(
	thread_t                                thread);

extern kern_return_t    machine_thread_set_state(
	thread_t                                thread,
	thread_flavor_t                 flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  count);

extern mach_vm_address_t machine_thread_pc(
	thread_t                thread);

extern void machine_thread_reset_pc(
	thread_t                thread,
	mach_vm_address_t       pc);

extern boolean_t        machine_thread_on_core(
	thread_t                thread);

extern boolean_t        machine_thread_on_core_allow_invalid(
	thread_t                thread);

extern kern_return_t    machine_thread_get_state(
	thread_t                                thread,
	thread_flavor_t                 flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  *count);

extern kern_return_t    machine_thread_state_convert_from_user(
	thread_t                                thread,
	thread_flavor_t                 flavor,
	thread_state_t                  tstate,
	mach_msg_type_number_t  count,
	thread_state_t old_tstate,
	mach_msg_type_number_t old_count,
	thread_set_status_flags_t tssf_flags);

extern kern_return_t    machine_thread_state_convert_to_user(
	thread_t                                thread,
	thread_flavor_t                 flavor,
	thread_state_t                  tstate,
	mach_msg_type_number_t  *count,
	thread_set_status_flags_t tssf_flags);

extern kern_return_t    machine_thread_dup(
	thread_t                self,
	thread_t                target,
	boolean_t               is_corpse);

extern void             machine_thread_init(void);

extern void             machine_thread_template_init(thread_t thr_template);

#if __has_feature(ptrauth_calls)
extern bool             machine_thread_state_is_debug_flavor(int flavor);
#endif /* __has_feature(ptrauth_calls) */


extern void             machine_thread_create(
	thread_t                thread,
	task_t                  task,
	bool                    first_thread);

extern kern_return_t    machine_thread_process_signature(
	thread_t                thread,
	task_t                  task);

extern void             machine_thread_switch_addrmode(
	thread_t                 thread);

extern void                 machine_thread_destroy(
	thread_t                thread);

extern void                             machine_set_current_thread(
	thread_t                        thread);

extern kern_return_t    machine_thread_get_kern_state(
	thread_t                                thread,
	thread_flavor_t                 flavor,
	thread_state_t                  tstate,
	mach_msg_type_number_t  *count);

extern kern_return_t    machine_thread_inherit_taskwide(
	thread_t                thread,
	task_t                  parent_task);

extern kern_return_t    machine_thread_set_tsd_base(
	thread_t                                thread,
	mach_vm_offset_t                tsd_base);

#define thread_mtx_try(thread)                  lck_mtx_try_lock(&(thread)->mutex)
#define thread_mtx_held(thread)                 lck_mtx_assert(&(thread)->mutex, LCK_MTX_ASSERT_OWNED)

extern void thread_apc_ast(thread_t thread);

extern void thread_update_qos_cpu_time(thread_t thread);

void act_machine_sv_free(thread_t, int);

vm_offset_t                     min_valid_stack_address(void);
vm_offset_t                     max_valid_stack_address(void);

extern bool thread_no_smt(thread_t thread);
extern bool processor_active_thread_no_smt(processor_t processor);

extern void thread_set_options(uint32_t thopt);

#if CONFIG_THREAD_GROUPS
struct thread_group *thread_get_current_voucher_thread_group(thread_t thread);
#endif /* CONFIG_THREAD_GROUPS */

#if CONFIG_COALITIONS
uint64_t thread_get_current_voucher_resource_coalition_id(thread_t thread);
#endif /* CONFIG_COALITIONS */

#endif  /* MACH_KERNEL_PRIVATE */
#if BSD_KERNEL_PRIVATE

/* Duplicated from osfmk/kern/ipc_tt.h */
__options_decl(port_intrans_options_t, uint32_t, {
	PORT_INTRANS_OPTIONS_NONE              = 0x0000,
	PORT_INTRANS_THREAD_IN_CURRENT_TASK    = 0x0001,
	PORT_INTRANS_THREAD_NOT_CURRENT_THREAD = 0x0002,

	PORT_INTRANS_SKIP_TASK_EVAL            = 0x0004,
	PORT_INTRANS_ALLOW_CORPSE_TASK         = 0x0008,
});

extern thread_t port_name_to_thread(
	mach_port_name_t            port_name,
	port_intrans_options_t    options);

#endif /* BSD_KERNEL_PRIVATE */
#ifdef XNU_KERNEL_PRIVATE

extern void                     thread_require(
	thread_t        thread);

extern void                     thread_deallocate_safe(
	thread_t                thread);

extern uint64_t                 thread_rettokern_addr(
	thread_t thread);

extern uint64_t                 thread_wqquantum_addr(
	thread_t thread);

extern integer_t        thread_kern_get_pri(thread_t thr) __pure2;

extern void             thread_kern_set_pri(thread_t thr, integer_t pri);

extern integer_t        thread_kern_get_kernel_maxpri(void) __pure2;

uint16_t        thread_set_tag(thread_t thread, uint16_t tag);
uint16_t        thread_get_tag(thread_t thread);

__options_decl(shared_rsrc_policy_agent_t, uint32_t, {
	SHARED_RSRC_POLICY_AGENT_DISPATCH = 0,
	SHARED_RSRC_POLICY_AGENT_SYSCTL = 1,
	SHARED_RSRC_POLICY_AGENT_PERFCTL_CSW = 2,
	SHARED_RSRC_POLICY_AGENT_PERFCTL_QUANTUM = 3,
});

boolean_t       thread_shared_rsrc_policy_get(thread_t thread, cluster_shared_rsrc_type_t type);
kern_return_t   thread_shared_rsrc_policy_set(thread_t thread, uint32_t index, cluster_shared_rsrc_type_t type, shared_rsrc_policy_agent_t agent);
kern_return_t   thread_shared_rsrc_policy_clear(thread_t thread, cluster_shared_rsrc_type_t type, shared_rsrc_policy_agent_t agent);

#ifdef MACH_KERNEL_PRIVATE
static inline thread_tag_t
thread_set_tag_internal(thread_t thread, thread_tag_t tag)
{
	return os_atomic_or_orig(&thread->thread_tag, tag, relaxed);
}

static inline thread_tag_t
thread_get_tag_internal(thread_t thread)
{
	return thread->thread_tag;
}
#endif /* MACH_KERNEL_PRIVATE */

uint64_t        thread_last_run_time(thread_t thread);

extern kern_return_t    thread_state_initialize(
	thread_t                                thread);

extern kern_return_t    thread_setstatus(
	thread_t                                thread,
	int                                             flavor,
	thread_state_t                  tstate,
	mach_msg_type_number_t  count);

extern kern_return_t    thread_setstatus_from_user(
	thread_t                                thread,
	int                                             flavor,
	thread_state_t                  tstate,
	mach_msg_type_number_t  count,
	thread_state_t                  old_tstate,
	mach_msg_type_number_t  old_count,
	thread_set_status_flags_t flags);

extern kern_return_t    thread_getstatus(
	thread_t                                thread,
	int                                             flavor,
	thread_state_t                  tstate,
	mach_msg_type_number_t  *count);

extern void main_thread_set_immovable_pinned(thread_t thread);

extern kern_return_t    thread_getstatus_to_user(
	thread_t                                thread,
	int                                             flavor,
	thread_state_t                  tstate,
	mach_msg_type_number_t  *count,
	thread_set_status_flags_t flags);

extern kern_return_t    thread_create_with_continuation(
	task_t task,
	thread_t *new_thread,
	thread_continue_t continuation);

extern kern_return_t main_thread_create_waiting(task_t    task,
    thread_continue_t              continuation,
    event_t                        event,
    thread_t                       *new_thread);

extern kern_return_t    thread_create_workq_waiting(
	task_t                  task,
	thread_continue_t       thread_return,
	thread_t                *new_thread);

extern  void    thread_yield_internal(
	mach_msg_timeout_t      interval);

extern void thread_yield_to_preemption(void);

extern void thread_depress_timer_setup(thread_t self);

/*
 * Thread-private CPU limits: apply a private CPU limit to this thread only. Available actions are:
 *
 * 1) Block. Prevent CPU consumption of the thread from exceeding the limit.
 * 2) Exception. Generate a resource consumption exception when the limit is exceeded.
 * 3) Disable. Remove any existing CPU limit.
 */
#define THREAD_CPULIMIT_BLOCK           0x1
#define THREAD_CPULIMIT_EXCEPTION       0x2
#define THREAD_CPULIMIT_DISABLE         0x3

struct _thread_ledger_indices {
	int cpu_time;
};

extern struct _thread_ledger_indices thread_ledgers;

extern int thread_get_cpulimit(int *action, uint8_t *percentage, uint64_t *interval_ns);
extern int thread_set_cpulimit(int action, uint8_t percentage, uint64_t interval_ns);

extern uint64_t thread_cpulimit_remaining(uint64_t now);
extern bool thread_cpulimit_interval_has_expired(uint64_t now);
extern void thread_cpulimit_restart(uint64_t now);

extern void thread_read_times(
	thread_t         thread,
	time_value_t    *user_time,
	time_value_t    *system_time,
	time_value_t    *runnable_time);

extern void thread_read_times_unsafe(
	thread_t         thread,
	time_value_t    *user_time,
	time_value_t    *system_time,
	time_value_t    *runnable_time);

extern uint64_t         thread_get_runtime_self(void);

extern void                     thread_setuserstack(
	thread_t                thread,
	mach_vm_offset_t        user_stack);

extern user_addr_t         thread_adjuserstack(
	thread_t                thread,
	int                             adjust);


extern void                     thread_setentrypoint(
	thread_t                thread,
	mach_vm_offset_t        entry);

extern kern_return_t    thread_set_tsd_base(
	thread_t        thread,
	mach_vm_offset_t tsd_base);

extern kern_return_t    thread_setsinglestep(
	thread_t                thread,
	int                     on);

extern kern_return_t    thread_userstack(
	thread_t,
	int,
	thread_state_t,
	unsigned int,
	mach_vm_offset_t *,
	int *,
	boolean_t);

extern kern_return_t    thread_entrypoint(
	thread_t,
	int,
	thread_state_t,
	unsigned int,
	mach_vm_offset_t *);

extern kern_return_t    thread_userstackdefault(
	mach_vm_offset_t *,
	boolean_t);

extern kern_return_t    thread_wire_internal(
	host_priv_t             host_priv,
	thread_t                thread,
	boolean_t               wired,
	boolean_t               *prev_state);


extern kern_return_t    thread_dup(thread_t);

extern kern_return_t thread_dup2(thread_t, thread_t);

#if !defined(_SCHED_CALL_T_DEFINED)
#define _SCHED_CALL_T_DEFINED
typedef void    (*sched_call_t)(
	int                             type,
	thread_t                thread);
#endif

#define SCHED_CALL_BLOCK                0x1
#define SCHED_CALL_UNBLOCK              0x2

extern void             thread_sched_call(
	thread_t                thread,
	sched_call_t    call);

extern boolean_t        thread_is_static_param(
	thread_t                thread);

extern task_t   get_threadtask(thread_t) __pure2;

extern task_t   get_threadtask_early(thread_t) __pure2;

/*
 * Thread is running within a 64-bit address space.
 */
#define thread_is_64bit_addr(thd)       \
	task_has_64Bit_addr(get_threadtask(thd))

/*
 * Thread is using 64-bit machine state.
 */
#define thread_is_64bit_data(thd)       \
	task_has_64Bit_data(get_threadtask(thd))

struct uthread;

#if defined(__x86_64__)
extern int              thread_task_has_ldt(thread_t);
#endif
extern void             set_thread_pagein_error(thread_t, int);
extern event_t          workq_thread_init_and_wq_lock(task_t, thread_t); // bsd/pthread/

struct proc;
struct uthread;
struct image_params;
extern const size_t     uthread_size;
extern thread_ro_t      get_thread_ro_unchecked(thread_t) __pure2;
extern thread_ro_t      get_thread_ro(thread_t) __pure2;
extern thread_ro_t      current_thread_ro_unchecked(void) __pure2;
extern thread_ro_t      current_thread_ro(void) __pure2;
extern void             clear_thread_ro_proc(thread_t);
extern struct uthread  *get_bsdthread_info(thread_t) __pure2;
extern thread_t         get_machthread(struct uthread *) __pure2;
extern uint64_t         uthread_tid(struct uthread *) __pure2;
extern user_addr_t      thread_get_sigreturn_token(thread_t thread);
extern uint32_t         thread_get_sigreturn_diversifier(thread_t thread);
extern void             uthread_init(task_t, struct uthread *, thread_ro_t, int);
extern void             uthread_cleanup_name(struct uthread *uthread);
extern void             uthread_cleanup(struct uthread *, thread_ro_t);
extern void             uthread_cred_ref(struct ucred *);
extern void             uthread_cred_free(struct ucred *);
extern void             uthread_destroy(struct uthread *);
extern void             uthread_reset_proc_refcount(struct uthread *);

extern void             uthread_set_exec_data(struct uthread *uth, struct image_params *imgp);
extern bool             uthread_is64bit(struct uthread *uth) __pure2;
#if PROC_REF_DEBUG
extern void             uthread_init_proc_refcount(struct uthread *);
extern void             uthread_destroy_proc_refcount(struct uthread *);
extern void             uthread_assert_zero_proc_refcount(struct uthread *);
#else
#define                 uthread_init_proc_refcount(uth)        ((void)(uth))
#define                 uthread_destroy_proc_refcount(uth)     ((void)(uth))
#define                 uthread_assert_zero_proc_refcount(uth) ((void)(uth))
#endif
#if CONFIG_DEBUG_SYSCALL_REJECTION
extern uint64_t         uthread_get_syscall_rejection_flags(void *);
extern uint64_t         *uthread_get_syscall_rejection_mask(void *);
extern uint64_t         *uthread_get_syscall_rejection_once_mask(void *);
extern bool             uthread_syscall_rejection_is_enabled(void *);
#endif /* CONFIG_DEBUG_SYSCALL_REJECTION */
extern mach_port_name_t  uthread_joiner_port(struct uthread *);
extern user_addr_t       uthread_joiner_address(struct uthread *);
extern void              uthread_joiner_wake(task_t task, struct uthread *);

extern boolean_t        thread_should_halt(
	thread_t                thread);

extern boolean_t        thread_should_abort(
	thread_t);

extern int is_64signalregset(void);

extern void act_set_kperf(thread_t);
extern void act_set_astledger(thread_t thread);
extern void act_set_astledger_async(thread_t thread);
extern void act_set_io_telemetry_ast(thread_t);
extern void act_set_macf_telemetry_ast(thread_t);
extern void act_set_astproc_resource(thread_t);

extern vm_offset_t thread_get_kernel_stack(thread_t);

extern kern_return_t thread_process_signature(thread_t thread, task_t task);

extern uint32_t dtrace_get_thread_predcache(thread_t);
extern int64_t dtrace_get_thread_vtime(thread_t);
extern int64_t dtrace_get_thread_tracing(thread_t);
extern uint16_t dtrace_get_thread_inprobe(thread_t);
extern int dtrace_get_thread_last_cpu_id(thread_t);
extern vm_offset_t dtrace_get_kernel_stack(thread_t);
#define dtrace_get_kernel_stack thread_get_kernel_stack
extern void dtrace_set_thread_predcache(thread_t, uint32_t);
extern void dtrace_set_thread_vtime(thread_t, int64_t);
extern void dtrace_set_thread_tracing(thread_t, int64_t);
extern void dtrace_set_thread_inprobe(thread_t, uint16_t);
extern void dtrace_thread_bootstrap(void);
extern void dtrace_thread_didexec(thread_t);

extern int64_t dtrace_calc_thread_recent_vtime(thread_t);


extern kern_return_t    thread_set_wq_state32(
	thread_t          thread,
	thread_state_t    tstate);

extern kern_return_t    thread_set_wq_state64(
	thread_t          thread,
	thread_state_t    tstate);

extern vm_offset_t      kernel_stack_mask;
extern vm_offset_t      kernel_stack_size;
extern vm_offset_t      kernel_stack_depth_max;

extern void guard_ast(thread_t);
extern void fd_guard_ast(thread_t,
    mach_exception_code_t, mach_exception_subcode_t);
#if CONFIG_VNGUARD
extern void vn_guard_ast(thread_t,
    mach_exception_code_t, mach_exception_subcode_t);
#endif
extern void mach_port_guard_ast(thread_t,
    mach_exception_code_t, mach_exception_subcode_t);
extern void virt_memory_guard_ast(thread_t,
    mach_exception_code_t, mach_exception_subcode_t);
extern void thread_guard_violation(thread_t,
    mach_exception_code_t, mach_exception_subcode_t, boolean_t);
extern void thread_update_io_stats(thread_t, int size, int io_flags);

extern kern_return_t    thread_set_voucher_name(mach_port_name_t name);
extern kern_return_t thread_get_voucher_origin_pid(thread_t thread, int32_t *pid);
extern kern_return_t thread_get_voucher_origin_proximate_pid(thread_t thread,
    int32_t *origin_pid, int32_t *proximate_pid);
extern kern_return_t thread_get_current_voucher_origin_pid(int32_t *pid);

extern void thread_enable_send_importance(thread_t thread, boolean_t enable);

/*
 * Translate signal context data pointer to userspace representation
 */

extern kern_return_t    machine_thread_siguctx_pointer_convert_to_user(
	thread_t thread,
	user_addr_t *uctxp);

extern void machine_tecs(thread_t thr);

typedef enum cpuvn {
	CPUVN_CI = 1
} cpuvn_e;

extern int machine_csv(cpuvn_e cve);
#if defined(__x86_64__)
extern void machine_thread_set_insn_copy_optout(thread_t thr);
#endif

/*
 * Translate array of function pointer syscall arguments from userspace representation
 */

extern kern_return_t    machine_thread_function_pointers_convert_from_user(
	thread_t thread,
	user_addr_t *fptrs,
	uint32_t count);

/*
 * Get the duration of the given thread's last wait.
 */
uint64_t thread_get_last_wait_duration(thread_t thread);

extern bool thread_get_no_smt(void);
#if defined(__x86_64__)
extern bool curtask_get_insn_copy_optout(void);
extern void curtask_set_insn_copy_optout(void);
#endif /* defined(__x86_64__) */

/*! @function ctid_get_thread
 *  @abstract translates a ctid_t to thread_t
 *  @discussion ctid are system wide compact thread-id
 *              associated to thread_t at thread creation
 *              and recycled at thread termination. If a ctid is
 *              referenced past the corresponding thread termination,
 *              it is considered stale, and the behavior is not defined.
 *              Note that this call does not acquire a reference on the thread,
 *              so as soon as the matching thread terminates, the ctid
 *              will become stale, and it could be re-used and associated with
 *              another thread. You must externally guarantee that the thread
 *              will not exit while you are using its ctid.
 *  @result   thread_t corresponding to ctid
 */
extern thread_t ctid_get_thread(ctid_t ctid);

/*! @function ctid_get_thread
 *  @abstract translates a ctid_t to thread_t
 *  @discussion Unsafe variant of ctid_get_thread() to be used
 *              when the caller can't guarantee the liveness of this ctid_t.
 *              may return NULL or a freed thread_t.
 */
extern thread_t ctid_get_thread_unsafe(ctid_t ctid);

/*!
 *   @function thread_get_ctid
 *   @abstract returns the ctid of thread.
 *   @param thread to find the corresponding ctid.
 *   @discussion the ctid provided will become stale after the matching thread
 *               terminates.
 *   @result uint32_t ctid.
 */
extern ctid_t thread_get_ctid(thread_t thread);

#endif  /* XNU_KERNEL_PRIVATE */
#ifdef KERNEL_PRIVATE

typedef struct thread_pri_floor {
	thread_t thread;
} thread_pri_floor_t;

#ifdef MACH_KERNEL_PRIVATE
extern void thread_floor_boost_ast(thread_t thread);
extern void thread_floor_boost_set_promotion_locked(thread_t thread);
#endif /* MACH_KERNEL_PRIVATE */

/*!  @function thread_priority_floor_start
 *   @abstract boost the current thread priority to floor.
 *   @discussion Increase the priority of the current thread to at least MINPRI_FLOOR.
 *       The boost will be mantained until a corresponding thread_priority_floor_end()
 *       is called. Every call of thread_priority_floor_start() needs to have a corresponding
 *       call to thread_priority_floor_end() from the same thread.
 *       No thread can return to userspace before calling thread_priority_floor_end().
 *
 *       NOTE: avoid to use this function. Try to use gate_t or sleep_with_inheritor()
 *       instead.
 *   @result a token to be given to the corresponding thread_priority_floor_end()
 */
extern thread_pri_floor_t thread_priority_floor_start(void);
/*!  @function thread_priority_floor_end
 *   @abstract ends the floor boost.
 *   @param token the token obtained from thread_priority_floor_start()
 *   @discussion ends the priority floor boost started with thread_priority_floor_start()
 */
extern void thread_priority_floor_end(thread_pri_floor_t *token);

extern void thread_set_no_smt(bool set);

extern void thread_mtx_lock(thread_t thread);

extern void thread_mtx_unlock(thread_t thread);

extern uint64_t thread_dispatchqaddr(
	thread_t thread);

bool thread_is_eager_preempt(thread_t thread);
void thread_set_eager_preempt(thread_t thread);
void thread_clear_eager_preempt(thread_t thread);
void thread_set_honor_qlimit(thread_t thread);
void thread_clear_honor_qlimit(thread_t thread);
extern ipc_port_t convert_thread_to_port(thread_t);
extern ipc_port_t convert_thread_to_port_pinned(thread_t);
extern ipc_port_t convert_thread_inspect_to_port(thread_inspect_t);
extern ipc_port_t convert_thread_read_to_port(thread_read_t);
extern boolean_t is_external_pageout_thread(void);
extern boolean_t is_vm_privileged(void);
extern boolean_t set_vm_privilege(boolean_t);
extern kern_allocation_name_t thread_set_allocation_name(kern_allocation_name_t new_name);
extern void *thread_iokit_tls_get(uint32_t index);
extern void thread_iokit_tls_set(uint32_t index, void * data);
extern int thread_self_region_page_shift(void);
extern void thread_self_region_page_shift_set(int pgshift);
extern kern_return_t thread_create_immovable(task_t task, thread_t *new_thread);
extern kern_return_t thread_terminate_pinned(thread_t thread);

struct thread_attr_for_ipc_propagation;
extern kern_return_t thread_get_ipc_propagate_attr(thread_t thread, struct thread_attr_for_ipc_propagation *attr);
extern size_t thread_get_current_exec_path(char *path, size_t size);
#endif /* KERNEL_PRIVATE */
#ifdef XNU_KERNEL_PRIVATE

extern void
thread_get_thread_name(thread_t th, char* name);

/* Read the runq assignment, under the thread lock. */
extern processor_t thread_get_runq(thread_t thread);

/*
 * Read the runq assignment, under both the thread lock and
 * the pset lock corresponding to the last non-null assignment.
 */
extern processor_t thread_get_runq_locked(thread_t thread);

/*
 * Set the runq assignment to a non-null value, under both the
 * thread lock and the pset lock corresponding to the new
 * assignment.
 */
extern void thread_set_runq_locked(thread_t thread, processor_t new_runq);

/*
 * Set the runq assignment to PROCESSOR_NULL, under the pset
 * lock corresponding to the current non-null assignment.
 */
extern void thread_clear_runq(thread_t thread);

/*
 * Set the runq assignment to PROCESSOR_NULL, under both the
 * thread lock and the pset lock corresponding to the current
 * non-null assignment.
 */
extern void thread_clear_runq_locked(thread_t thread);

/*
 * Assert the runq assignment to be PROCESSOR_NULL, under
 * some guarantee that the runq will not change from null to
 * non-null, such as holding the thread lock.
 */
extern void thread_assert_runq_null(thread_t thread);

/*
 * Assert the runq assignment to be non-null, under the pset
 * lock corresponding to the current non-null assignment.
 */
extern void thread_assert_runq_nonnull(thread_t thread);

extern bool thread_supports_cooperative_workqueue(thread_t thread);
extern void thread_arm_workqueue_quantum(thread_t thread);
extern void thread_disarm_workqueue_quantum(thread_t thread);

extern void thread_evaluate_workqueue_quantum_expiry(thread_t thread);
extern bool thread_has_expired_workqueue_quantum(thread_t thread, bool should_trace);

#if CONFIG_SPTM

extern void
thread_associate_txm_thread_stack(uintptr_t thread_stack);

extern void
thread_disassociate_txm_thread_stack(uintptr_t thread_stack);

extern uintptr_t
thread_get_txm_thread_stack(void);

#endif /* CONFIG_SPTM */

/* Kernel side prototypes for MIG routines */
extern kern_return_t thread_get_exception_ports(
	thread_t                        thread,
	exception_mask_t                exception_mask,
	exception_mask_array_t          masks,
	mach_msg_type_number_t          *CountCnt,
	exception_port_array_t          ports,
	exception_behavior_array_t      behaviors,
	thread_state_flavor_array_t     flavors);

extern kern_return_t thread_get_special_port(
	thread_inspect_t         thread,
	int                      which,
	ipc_port_t              *portp);

#endif /* XNU_KERNEL_PRIVATE */

/*! @function thread_has_thread_name
 *   @abstract Checks if a thread has a name.
 *   @discussion This function takes one input, a thread, and returns
 *       a boolean value indicating if that thread already has a name associated
 *       with it.
 *   @param th The thread to inspect.
 *   @result TRUE if the thread has a name, FALSE otherwise.
 */
extern boolean_t thread_has_thread_name(thread_t th);

/*! @function thread_set_thread_name
 *   @abstract Set a thread's name.
 *   @discussion This function takes two input parameters: a thread to name,
 *       and the name to apply to the thread.  The name will be copied over to
 *       the thread in order to better identify the thread.  If the name is
 *       longer than MAXTHREADNAMESIZE - 1, it will be truncated.
 *   @param th The thread to be named.
 *   @param name The name to apply to the thread.
 */
extern void thread_set_thread_name(thread_t th, const char* name);

#if !MACH_KERNEL_PRIVATE || !defined(current_thread)
extern thread_t current_thread(void) __pure2;
#endif

extern uint64_t thread_tid(thread_t thread) __pure2;

extern void thread_reference(
	thread_t        thread);

extern void thread_deallocate(
	thread_t        thread);

/*! @function kernel_thread_start
 *   @abstract Create a kernel thread.
 *   @discussion This function takes three input parameters, namely reference
 *       to the function that the thread should execute, caller specified data
 *       and a reference which is used to return the newly created kernel
 *       thread. The function returns KERN_SUCCESS on success or an appropriate
 *       kernel code type indicating the error. It may be noted that the caller
 *       is responsible for explicitly releasing the reference to the created
 *       thread when no longer needed. This should be done by calling
 *       thread_deallocate(new_thread).
 *   @param continuation A C-function pointer where the thread will begin execution.
 *   @param parameter Caller specified data to be passed to the new thread.
 *   @param new_thread Reference to the new thread is returned in this parameter.
 *   @result Returns KERN_SUCCESS on success or an appropriate kernel code type.
 */

extern kern_return_t    kernel_thread_start(
	thread_continue_t       continuation,
	void                    *parameter,
	thread_t                *new_thread);

__END_DECLS

#endif  /* _KERN_THREAD_H_ */
