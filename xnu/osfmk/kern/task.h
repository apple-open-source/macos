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
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
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
 *	File:	task.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	This file contains the structure definitions for tasks.
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
/*
 * NOTICE: This file was modified by McAfee Research in 2004 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 * Copyright (c) 2005 SPARTA, Inc.
 */

#ifndef _KERN_TASK_H_
#define _KERN_TASK_H_

#include <kern/kern_types.h>
#include <kern/task_ref.h>
#include <mach/mach_types.h>
#include <sys/cdefs.h>

#ifdef XNU_KERNEL_PRIVATE
#include <kern/btlog.h>
#include <kern/kern_cdata.h>
#include <mach/sfi_class.h>
#include <kern/counter.h>
#include <kern/cs_blobs.h>
#include <kern/queue.h>
#include <kern/recount.h>
#include <sys/kern_sysctl.h>
#if CONFIG_EXCLAVES
#include <mach/exclaves.h>
#endif /* CONFIG_EXCLAVES */
#endif /* XNU_KERNEL_PRIVATE */

#ifdef  MACH_KERNEL_PRIVATE
#include <mach/boolean.h>
#include <mach/port.h>
#include <mach/time_value.h>
#include <mach/message.h>
#include <mach/mach_param.h>
#include <mach/task_info.h>
#include <mach/exception_types.h>
#include <mach/vm_statistics.h>
#include <machine/task.h>

#include <kern/cpu_data.h>
#include <kern/queue.h>
#include <kern/exception.h>
#include <kern/locks.h>
#include <security/_label.h>
#include <ipc/ipc_port.h>

#include <kern/thread.h>
#include <mach/coalition.h>
#include <stdatomic.h>
#include <os/refcnt.h>

#if CONFIG_DEFERRED_RECLAIM
typedef struct vm_deferred_reclamation_metadata_s *vm_deferred_reclamation_metadata_t;
#endif /* CONFIG_DEFFERED_RECLAIM */

struct _cpu_time_qos_stats {
	uint64_t cpu_time_qos_default;
	uint64_t cpu_time_qos_maintenance;
	uint64_t cpu_time_qos_background;
	uint64_t cpu_time_qos_utility;
	uint64_t cpu_time_qos_legacy;
	uint64_t cpu_time_qos_user_initiated;
	uint64_t cpu_time_qos_user_interactive;
};

struct task_writes_counters {
	uint64_t task_immediate_writes;
	uint64_t task_deferred_writes;
	uint64_t task_invalidated_writes;
	uint64_t task_metadata_writes;
};

struct task_watchports;
#include <bank/bank_internal.h>

#ifdef MACH_BSD
struct proc;
struct proc_ro;
#endif

struct task {
	/* Synchronization/destruction information */
	decl_lck_mtx_data(, lock);      /* Task's lock */
	os_refcnt_t     ref_count;      /* Number of references to me */

#if DEVELOPMENT || DEBUG
	struct os_refgrp *ref_group;
	lck_spin_t        ref_group_lock;
#endif /* DEVELOPMENT || DEBUG */

	bool            active;         /* Task has not been terminated */
	bool            ipc_active;     /* IPC with the task ports is allowed */
	bool            halting;        /* Task is being halted */
	bool            message_app_suspended;  /* Let iokit know when pidsuspended */

	/* Virtual timers */
	uint32_t        vtimers;
	uint32_t loadTag; /* dext ID used for logging identity */

	/* Globally uniqueid to identify tasks and corpses */
	uint64_t        task_uniqueid;

	/* Miscellaneous */
	vm_map_t        XNU_PTRAUTH_SIGNED_PTR("task.map") map; /* Address space description */
	queue_chain_t   tasks;  /* global list of tasks */
	struct task_watchports *watchports; /* watchports passed in spawn */
	turnstile_inheritor_t returnwait_inheritor; /* inheritor for task_wait */

#if defined(CONFIG_SCHED_MULTIQ)
	sched_group_t sched_group;
#endif /* defined(CONFIG_SCHED_MULTIQ) */

	/* Threads in this task */
	queue_head_t            threads;
	struct restartable_ranges *t_rr_ranges;

	processor_set_t         pset_hint;
	struct affinity_space   *affinity_space;

	int                     thread_count;
	uint32_t                active_thread_count;
	int                     suspend_count;  /* Internal scheduling only */
#ifdef CONFIG_TASK_SUSPEND_STATS
	struct task_suspend_stats_s t_suspend_stats; /* suspension statistics for this task */
	task_suspend_source_array_t t_suspend_sources; /* array of suspender debug info for this task */
#endif /* CONFIG_TASK_SUSPEND_STATS */

	/* User-visible scheduling information */
	integer_t               user_stop_count;        /* outstanding stops */
	integer_t               legacy_stop_count;      /* outstanding legacy stops */

	int16_t                 priority;               /* base priority for threads */
	int16_t                 max_priority;           /* maximum priority for threads */

	integer_t               importance;             /* priority offset (BSD 'nice' value) */

#define task_is_immovable(task) \
	!!(task_get_control_port_options(task) & TASK_CONTROL_PORT_IMMOVABLE)
#define task_is_pinned(task) \
	!!(task_get_control_port_options(task) & TASK_CONTROL_PORT_PINNED)

	/* Statistics */
	uint64_t                total_runnable_time;

	struct recount_task     tk_recount;

	/* IPC structures */
	decl_lck_mtx_data(, itk_lock_data);
	/*
	 * Different flavors of task port.
	 * These flavors TASK_FLAVOR_* are defined in mach_types.h
	 */
	struct ipc_port * XNU_PTRAUTH_SIGNED_PTR("task.itk_task_ports") itk_task_ports[TASK_SELF_PORT_COUNT];
	struct ipc_port * XNU_PTRAUTH_SIGNED_PTR("task.itk_settable_self") itk_settable_self;   /* a send right */
	struct ipc_port * XNU_PTRAUTH_SIGNED_PTR("task.itk_self") itk_self;                     /* immovable/pinned task port, does not hold right */
	struct exception_action exc_actions[EXC_TYPES_COUNT];
	/* a send right each valid element  */
	struct ipc_port * XNU_PTRAUTH_SIGNED_PTR("task.itk_host") itk_host;                     /* a send right */
	struct ipc_port * XNU_PTRAUTH_SIGNED_PTR("task.itk_bootstrap") itk_bootstrap;           /* a send right */
	struct ipc_port * XNU_PTRAUTH_SIGNED_PTR("task.itk_debug_control") itk_debug_control;   /* send right for debugmode communications */
	struct ipc_port * XNU_PTRAUTH_SIGNED_PTR("task.itk_task_access") itk_task_access;       /* and another send right */
	struct ipc_port * XNU_PTRAUTH_SIGNED_PTR("task.itk_resume") itk_resume;                 /* a receive right to resume this task */
	struct ipc_port * XNU_PTRAUTH_SIGNED_PTR("task.itk_registered") itk_registered[TASK_PORT_REGISTER_MAX];
	/* all send rights */
	ipc_port_t * XNU_PTRAUTH_SIGNED_PTR("task.itk_dyld_notify") itk_dyld_notify; /* lazy send rights array of size DYLD_MAX_PROCESS_INFO_NOTIFY_COUNT */
#if CONFIG_PROC_RESOURCE_LIMITS
	struct ipc_port * XNU_PTRAUTH_SIGNED_PTR("task.itk_resource_notify") itk_resource_notify; /* a send right to the resource notify port */
#endif /* CONFIG_PROC_RESOURCE_LIMITS */
	struct ipc_space * XNU_PTRAUTH_SIGNED_PTR("task.itk_space") itk_space;

	ledger_t        ledger;
	/* Synchronizer ownership information */
	queue_head_t    semaphore_list;         /* list of owned semaphores   */
	int             semaphores_owned;       /* number of semaphores owned */

	unsigned int    priv_flags;                     /* privilege resource flags */
#define VM_BACKING_STORE_PRIV   0x1

	MACHINE_TASK

	counter_t faults;             /* faults counter */
	counter_t pageins;            /* pageins counter */
	counter_t cow_faults;         /* copy on write fault counter */
	counter_t messages_sent;      /* messages sent counter */
	counter_t messages_received;  /* messages received counter */
	uint32_t decompressions;      /* decompression counter */
	uint32_t syscalls_mach;       /* mach system call counter */
	uint32_t syscalls_unix;       /* unix system call counter */
	uint32_t c_switch;            /* total context switches */
	uint32_t p_switch;            /* total processor switches */
	uint32_t ps_switch;           /* total pset switches */

#ifdef  MACH_BSD
	struct proc_ro *                bsd_info_ro;
#endif
	kcdata_descriptor_t             corpse_info;
	uint64_t                        crashed_thread_id;
	queue_chain_t                   corpse_tasks;
#ifdef CONFIG_MACF
	struct label *                  crash_label;
#endif
	volatile uint32_t t_flags;                                      /* general-purpose task flags protected by task_lock (TL) */
#define TF_NONE                 0
#define TF_64B_ADDR             0x00000001                              /* task has 64-bit addressing */
#define TF_64B_DATA             0x00000002                              /* task has 64-bit data registers */
#define TF_CPUMON_WARNING       0x00000004                              /* task has at least one thread in CPU usage warning zone */
#define TF_WAKEMON_WARNING      0x00000008                              /* task is in wakeups monitor warning zone */
#define TF_TELEMETRY            (TF_CPUMON_WARNING | TF_WAKEMON_WARNING) /* task is a telemetry participant */
#define TF_GPU_DENIED           0x00000010                              /* task is not allowed to access the GPU */
#define TF_PENDING_CORPSE       0x00000040                              /* task corpse has not been reported yet */
#define TF_CORPSE_FORK          0x00000080                              /* task is a forked corpse */
#define TF_CA_CLIENT_WI         0x00000800                              /* task has CA_CLIENT work interval */
#define TF_DARKWAKE_MODE        0x00001000                              /* task is in darkwake mode */
#define TF_NO_SMT               0x00002000                              /* task threads must not be paired with SMT threads */
#define TF_SYS_VERSION_COMPAT   0x00008000                              /* shim task accesses to OS version data (macOS - app compatibility) */
#define TF_TECS                 0x00020000                              /* task threads must enable CPU security */
#if defined(__x86_64__)
#define TF_INSN_COPY_OPTOUT     0x00040000                              /* task threads opt out of unhandled-fault instruction stream collection */
#endif
#define TF_COALITION_MEMBER     0x00080000                              /* task is a member of a coalition */
#define TF_NO_CORPSE_FORKING    0x00100000                              /* do not fork a corpse for this task */
#define TF_USE_PSET_HINT_CLUSTER_TYPE 0x00200000                        /* bind task to task->pset_hint->pset_cluster_type */
#define TF_DYLD_ALL_IMAGE_FINAL   0x00400000                            /* all_image_info_addr can no longer be changed */
#define TF_HASPROC              0x00800000                              /* task points to a proc */
#define TF_HAS_REPLY_PORT_TELEMETRY 0x10000000                          /* Rate limit telemetry for reply port security semantics violations rdar://100244531 */
#define TF_HAS_EXCEPTION_TELEMETRY  0x20000000                          /* Rate limit telemetry for exception identity violations rdar://100729339 */
#define TF_GAME_MODE            0x40000000                              /* Set the game mode bit for CLPC */

/*
 * WARNING: These TF_ and TFRO_ flags are NOT automatically inherited by a child of fork
 * If you believe something should be inherited, you must manually inherit the flags in `task_create_internal`
 */

/*
 * RO-protected flags:
 */
#define TFRO_CORPSE                     0x00000020                      /* task is a corpse */
#define TFRO_HARDENED                   0x00000100                      /* task is a hardened runtime binary */
#if XNU_TARGET_OS_OSX
#define TFRO_MACH_HARDENING_OPT_OUT     0x00000200                      /* task might load third party plugins on macOS and should be opted out of mach hardening */
#endif /* XNU_TARGET_OS_OSX */
#define TFRO_PLATFORM                   0x00000400                      /* task is a platform binary */
#define TFRO_FILTER_MSG                 0x00004000                      /* task calls into message filter callback before sending a message */
#define TFRO_PAC_EXC_FATAL              0x00010000                      /* task is marked a corpse if a PAC exception occurs */
#define TFRO_JIT_EXC_FATAL              0x00020000                      /* kill the task on access violations from privileged JIT code */
#define TFRO_PAC_ENFORCE_USER_STATE     0x01000000                      /* Enforce user and kernel signed thread state */
#if CONFIG_EXCLAVES
#define TFRO_HAS_KD_ACCESS              0x02000000                      /* Access to the kernel exclave resource domain  */
#endif /* CONFIG_EXCLAVES */

/*
 * Task is running within a 64-bit address space.
 */
#define task_has_64Bit_addr(task)       \
	(((task)->t_flags & TF_64B_ADDR) != 0)
#define task_set_64Bit_addr(task)       \
	((task)->t_flags |= TF_64B_ADDR)
#define task_clear_64Bit_addr(task)     \
	((task)->t_flags &= ~TF_64B_ADDR)

/*
 * Task is using 64-bit machine state.
 */
#define task_has_64Bit_data(task)       \
	(((task)->t_flags & TF_64B_DATA) != 0)
#define task_set_64Bit_data(task)       \
	((task)->t_flags |= TF_64B_DATA)
#define task_clear_64Bit_data(task)     \
	((task)->t_flags &= ~TF_64B_DATA)

#define task_corpse_pending_report(task)        \
	 (((task)->t_flags & TF_PENDING_CORPSE) != 0)

#define task_set_corpse_pending_report(task)       \
	 ((task)->t_flags |= TF_PENDING_CORPSE)

#define task_clear_corpse_pending_report(task)       \
	 ((task)->t_flags &= ~TF_PENDING_CORPSE)

#define task_is_a_corpse_fork(task)     \
	(((task)->t_flags & TF_CORPSE_FORK) != 0)

#define task_set_coalition_member(task)      \
	((task)->t_flags |= TF_COALITION_MEMBER)

#define task_clear_coalition_member(task)    \
	((task)->t_flags &= ~TF_COALITION_MEMBER)

#define task_is_coalition_member(task)       \
	(((task)->t_flags & TF_COALITION_MEMBER) != 0)

#define task_has_proc(task) \
	(((task)->t_flags & TF_HASPROC) != 0)

#define task_set_has_proc(task) \
	((task)->t_flags |= TF_HASPROC)

#define task_clear_has_proc(task) \
	((task)->t_flags &= ~TF_HASPROC)

#define task_has_reply_port_telemetry(task) \
	(((task)->t_flags & TF_HAS_REPLY_PORT_TELEMETRY) != 0)

#define task_set_reply_port_telemetry(task) \
	((task)->t_flags |= TF_HAS_REPLY_PORT_TELEMETRY)

#define task_has_exception_telemetry(task) \
	(((task)->t_flags & TF_HAS_EXCEPTION_TELEMETRY) != 0)

#define task_set_exception_telemetry(task) \
	((task)->t_flags |= TF_HAS_EXCEPTION_TELEMETRY)

	uint32_t t_procflags;                                            /* general-purpose task flags protected by proc_lock (PL) */
#define TPF_NONE                 0
#define TPF_DID_EXEC             0x00000001                              /* task has been execed to a new task */
#define TPF_EXEC_COPY            0x00000002                              /* task is the new copy of an exec */

#define task_did_exec_internal(task)            \
	(((task)->t_procflags & TPF_DID_EXEC) != 0)

#define task_is_exec_copy_internal(task)        \
	(((task)->t_procflags & TPF_EXEC_COPY) != 0)

	mach_vm_address_t       all_image_info_addr; /* dyld __all_image_info     */
	mach_vm_size_t          all_image_info_size; /* section location and size */

#if CONFIG_CPU_COUNTERS
#define TASK_KPC_FORCED_ALL_CTRS        0x2     /* Bit in "t_kpc" signifying this task forced all counters */
	uint32_t t_kpc; /* kpc flags */
#endif /* CONFIG_CPU_COUNTERS */

	bool pidsuspended; /* pid_suspend called; no threads can execute */
	bool frozen;       /* frozen; private resident pages committed to swap */
	bool changing_freeze_state;        /* in the process of freezing or thawing */
	bool     is_large_corpse;
	uint16_t policy_ru_cpu          :4,
	    policy_ru_cpu_ext      :4,
	    applied_ru_cpu         :4,
	    applied_ru_cpu_ext     :4;
	uint8_t  rusage_cpu_flags;
	uint8_t  rusage_cpu_percentage;         /* Task-wide CPU limit percentage */
	uint8_t  rusage_cpu_perthr_percentage;  /* Per-thread CPU limit percentage */
#if MACH_ASSERT
	int8_t          suspends_outstanding;   /* suspends this task performed in excess of resumes */
#endif
	uint8_t                  t_returnwaitflags;
#define TWF_NONE                 0
#define TRW_LRETURNWAIT          0x01           /* task is waiting for fork/posix_spawn/exec to complete */
#define TRW_LRETURNWAITER        0x02           /* task is waiting for TRW_LRETURNWAIT to get cleared */
#define TRW_LEXEC_COMPLETE       0x04           /* thread should call exec complete */

#if CONFIG_EXCLAVES
	uint8_t                  t_exclave_state;
#define TES_NONE                 0
#define TES_CONCLAVE_TAINTED     0x01           /* Task has talked to conclave, xnu has tainted the process */
#define TES_CONCLAVE_UNTAINTABLE 0x02           /* Task can not be tainted by xnu when it talks to conclave */
#endif /* CONFIG_EXCLAVES */

#if __has_feature(ptrauth_calls)
	bool                            shared_region_auth_remapped;    /* authenticated sections ready for use */
	char                            *shared_region_id;              /* determines which ptr auth key to use */
#endif /* __has_feature(ptrauth_calls) */
	struct vm_shared_region         *shared_region;

	uint64_t rusage_cpu_interval;           /* Task-wide CPU limit interval */
	uint64_t rusage_cpu_perthr_interval;    /* Per-thread CPU limit interval */
	uint64_t rusage_cpu_deadline;
	thread_call_t rusage_cpu_callt;
#if CONFIG_TASKWATCH
	queue_head_t    task_watchers;          /* app state watcher threads */
	int     num_taskwatchers;
	int             watchapplying;
#endif /* CONFIG_TASKWATCH */

	struct bank_task *bank_context;  /* pointer to per task bank structure */

#if IMPORTANCE_INHERITANCE
	struct ipc_importance_task  *task_imp_base;     /* Base of IPC importance chain */
#endif /* IMPORTANCE_INHERITANCE */

	vm_extmod_statistics_data_t     extmod_statistics;

	struct task_requested_policy requested_policy;
	struct task_effective_policy effective_policy;

	/*
	 * Can be merged with imp_donor bits, once the IMPORTANCE_INHERITANCE macro goes away.
	 */
	uint32_t        low_mem_notified_warn           :1,     /* warning low memory notification is sent to the task */
	    low_mem_notified_critical       :1,                 /* critical low memory notification is sent to the task */
	    purged_memory_warn              :1,                 /* purgeable memory of the task is purged for warning level pressure */
	    purged_memory_critical          :1,                 /* purgeable memory of the task is purged for critical level pressure */
	    low_mem_privileged_listener     :1,                 /* if set, task would like to know about pressure changes before other tasks on the system */
	    mem_notify_reserved             :27;                /* reserved for future use */

	uint32_t memlimit_is_active                 :1, /* if set, use active attributes, otherwise use inactive attributes */
	    memlimit_is_fatal                   :1,     /* if set, exceeding current memlimit will prove fatal to the task */
	    memlimit_active_exc_resource        :1,     /* if set, suppress exc_resource exception when task exceeds active memory limit */
	    memlimit_inactive_exc_resource      :1,     /* if set, suppress exc_resource exception when task exceeds inactive memory limit */
	    memlimit_attrs_reserved             :28;     /* reserved for future use */

	io_stat_info_t          task_io_stats;

	struct task_writes_counters task_writes_counters_internal;
	struct task_writes_counters task_writes_counters_external;

	/*
	 * The cpu_time_qos_stats fields are protected by the task lock
	 */
	struct _cpu_time_qos_stats      cpu_time_eqos_stats;
	struct _cpu_time_qos_stats      cpu_time_rqos_stats;

	/* Statistics accumulated for terminated threads from this task */
	uint32_t        task_timer_wakeups_bin_1;
	uint32_t        task_timer_wakeups_bin_2;
	uint64_t        task_gpu_ns;

	uint8_t         task_can_transfer_memory_ownership;
#if DEVELOPMENT || DEBUG
	uint8_t         task_no_footprint_for_debug;
#endif
	uint8_t         task_objects_disowning;
	uint8_t         task_objects_disowned;
	/* # of purgeable volatile VM objects owned by this task: */
	int             task_volatile_objects;
	/* # of purgeable but not volatile VM objects owned by this task: */
	int             task_nonvolatile_objects;
	int             task_owned_objects;
	queue_head_t    task_objq;
	decl_lck_mtx_data(, task_objq_lock); /* protects "task_objq" */

	unsigned int    task_thread_limit:16;
#if __arm64__
	unsigned int    task_legacy_footprint:1;
	unsigned int    task_extra_footprint_limit:1;
	unsigned int    task_ios13extended_footprint_limit:1;
#endif /* __arm64__ */
	unsigned int    task_region_footprint:1;
	unsigned int    task_has_crossed_thread_limit:1;
	unsigned int    task_rr_in_flight:1; /* a t_rr_synchronzie() is in flight */
	/*
	 * A task's coalition set is "adopted" in task_create_internal
	 * and unset in task_deallocate_internal, so each array member
	 * can be referenced without the task lock.
	 * Note: these fields are protected by coalition->lock,
	 *       not the task lock.
	 */
	coalition_t     coalition[COALITION_NUM_TYPES];
	queue_chain_t   task_coalition[COALITION_NUM_TYPES];
	uint64_t        dispatchqueue_offset;

#if DEVELOPMENT || DEBUG
	boolean_t       task_unnested;
	int             task_disconnected_count;
#endif

#if HYPERVISOR
	void * XNU_PTRAUTH_SIGNED_PTR("task.hv_task_target") hv_task_target; /* hypervisor virtual machine object associated with this task */
#endif /* HYPERVISOR */

#if CONFIG_SECLUDED_MEMORY
	uint8_t task_can_use_secluded_mem;
	uint8_t task_could_use_secluded_mem;
	uint8_t task_could_also_use_secluded_mem;
	uint8_t task_suppressed_secluded;
#endif /* CONFIG_SECLUDED_MEMORY */

	task_exc_guard_behavior_t task_exc_guard;
	mach_vm_address_t mach_header_vm_address;

	queue_head_t    io_user_clients;

#if CONFIG_FREEZE
	queue_head_t   task_frozen_cseg_q;  /* queue of csegs frozen to NAND */
#endif /* CONFIG_FREEZE */
	boolean_t       donates_own_pages; /* pages land on the special Q (only swappable pages on iPadOS, early swap on macOS) */
	uint32_t task_shared_region_slide;   /* cached here to avoid locking during telemetry */
#if CONFIG_PHYS_WRITE_ACCT
	uint64_t        task_fs_metadata_writes;
#endif /* CONFIG_PHYS_WRITE_ACCT */
	uuid_t   task_shared_region_uuid;
#if CONFIG_MEMORYSTATUS
	uint64_t        memstat_dirty_start; /* last abstime transition into the dirty band or last call to task_ledger_settle_dirty_time  while dirty */
#endif /* CONFIG_MEMORYSTATUS */
	vmobject_list_output_t corpse_vmobject_list;
	uint64_t corpse_vmobject_list_size;
#if CONFIG_DEFERRED_RECLAIM
	vm_deferred_reclamation_metadata_t deferred_reclamation_metadata; /* Protected by the task lock */
#endif /* CONFIG_DEFERRED_RECLAIM */

#if CONFIG_EXCLAVES
	void * XNU_PTRAUTH_SIGNED_PTR("task.conclave") conclave;
	void * XNU_PTRAUTH_SIGNED_PTR("task.exclave_crash_info") exclave_crash_info;
	uint32_t exclave_crash_info_length;
#endif /* CONFIG_EXCLAVES */
};

ZONE_DECLARE_ID(ZONE_ID_PROC_TASK, void *);
extern zone_t proc_task_zone;

extern task_control_port_options_t task_get_control_port_options(task_t task);
extern void task_set_control_port_options(task_t task, task_control_port_options_t opts);

/*
 * EXC_GUARD default delivery behavior for optional Mach port and VM guards.
 * Applied to new tasks at creation time.
 */
extern task_exc_guard_behavior_t task_exc_guard_default;
extern size_t proc_and_task_size;
extern void  *get_bsdtask_info(task_t t);
extern void *task_get_proc_raw(task_t task);
static inline void
task_require(struct task *task)
{
	zone_id_require(ZONE_ID_PROC_TASK, proc_and_task_size, task_get_proc_raw(task));
}

/*
 * task_lock() and task_unlock() need to be callable from the `bsd/` tree of
 * XNU and are therefore promoted to full functions instead of macros so that
 * they can be linked against.
 *
 * We provide `extern` declarations here for consumers of `task.h` in `osfmk/`,
 * then separately provide `inline` definitions in `task.c`. Together with the
 * `BUILD_LTO=1` build argument, this guarantees these functions are always
 * inlined regardless of whether called from the `osfmk/` tree or `bsd/` tree.
 */
extern void task_lock(task_t);
extern void task_unlock(task_t);

#define task_lock_assert_owned(task)    LCK_MTX_ASSERT(&(task)->lock, LCK_MTX_ASSERT_OWNED)
#define task_lock_try(task)             lck_mtx_try_lock(&(task)->lock)

#define task_objq_lock_init(task)       lck_mtx_init(&(task)->task_objq_lock, &vm_object_lck_grp, &vm_object_lck_attr)
#define task_objq_lock_destroy(task)    lck_mtx_destroy(&(task)->task_objq_lock, &vm_object_lck_grp)
#define task_objq_lock(task)            lck_mtx_lock(&(task)->task_objq_lock)
#define task_objq_lock_assert_owned(task)       LCK_MTX_ASSERT(&(task)->task_objq_lock, LCK_MTX_ASSERT_OWNED)
#define task_objq_lock_try(task)        lck_mtx_try_lock(&(task)->task_objq_lock)
#define task_objq_unlock(task)          lck_mtx_unlock(&(task)->task_objq_lock)

#define itk_lock_init(task)     lck_mtx_init(&(task)->itk_lock_data, &ipc_lck_grp, &ipc_lck_attr)
#define itk_lock_destroy(task)  lck_mtx_destroy(&(task)->itk_lock_data, &ipc_lck_grp)
#define itk_lock(task)          lck_mtx_lock(&(task)->itk_lock_data)
#define itk_unlock(task)        lck_mtx_unlock(&(task)->itk_lock_data)

/* task clear return wait flags */
#define TCRW_CLEAR_INITIAL_WAIT   0x1
#define TCRW_CLEAR_FINAL_WAIT     0x2
#define TCRW_CLEAR_EXEC_COMPLETE  0x4
#define TCRW_CLEAR_ALL_WAIT       (TCRW_CLEAR_INITIAL_WAIT | TCRW_CLEAR_FINAL_WAIT)

/* Initialize task module */
extern void             task_init(void);

/* coalition_init() calls this to initialize ledgers before task_init() */
extern void             init_task_ledgers(void);

extern task_t   current_task(void) __pure2;

extern bool task_is_driver(task_t task);
extern uint32_t task_ro_flags_get(task_t task);
extern void task_ro_flags_set(task_t task, uint32_t flags);
extern void task_ro_flags_clear(task_t task, uint32_t flags);

extern lck_attr_t      task_lck_attr;
extern lck_grp_t       task_lck_grp;

struct task_watchport_elem {
	task_t                          twe_task;
	ipc_port_t                      twe_port;     /* (Space lock) */
	ipc_port_t XNU_PTRAUTH_SIGNED_PTR("twe_pdrequest") twe_pdrequest;
};

struct task_watchports {
	os_refcnt_t                     tw_refcount;           /* (Space lock) */
	task_t                          tw_task;               /* (Space lock) & tw_refcount == 0 */
	thread_t                        tw_thread;             /* (Space lock) & tw_refcount == 0 */
	uint32_t                        tw_elem_array_count;   /* (Space lock) */
	struct task_watchport_elem      tw_elem[];             /* (Space lock) & (Portlock) & (mq lock) */
};

#define task_watchports_retain(x)   (os_ref_retain(&(x)->tw_refcount))
#define task_watchports_release(x)  (os_ref_release(&(x)->tw_refcount))

#define task_watchport_elem_init(elem, task, port) \
do {                                               \
	(elem)->twe_task = (task);                 \
	(elem)->twe_port = (port);                 \
	(elem)->twe_pdrequest = IP_NULL;           \
} while(0)

#define task_watchport_elem_clear(elem) task_watchport_elem_init((elem), NULL, NULL)

extern void
task_add_turnstile_watchports(
	task_t          task,
	thread_t        thread,
	ipc_port_t      *portwatch_ports,
	uint32_t        portwatch_count);

extern void
task_watchport_elem_deallocate(
	struct          task_watchport_elem *watchport_elem);

extern boolean_t
task_has_watchports(task_t task);

void
task_dyld_process_info_update_helper(
	task_t                  task,
	size_t                  active_count,
	vm_map_address_t        magic_addr,
	ipc_port_t             *release_ports,
	size_t                  release_count);

extern kern_return_t
task_suspend2_mig(
	task_t                  task,
	task_suspension_token_t *suspend_token);

extern kern_return_t
task_suspend2_external(
	task_t                  task,
	task_suspension_token_t *suspend_token);

extern kern_return_t
task_resume2_mig(
	task_suspension_token_t suspend_token);

extern kern_return_t
task_resume2_external(
	task_suspension_token_t suspend_token);

extern void
task_suspension_token_deallocate_grp(
	task_suspension_token_t suspend_token,
	task_grp_t              grp);

extern ipc_port_t
convert_task_to_port_with_flavor(
	task_t                  task,
	mach_task_flavor_t      flavor,
	task_grp_t              grp);

extern task_t   current_task_early(void) __pure2;

#else   /* MACH_KERNEL_PRIVATE */

__BEGIN_DECLS

extern task_t   current_task(void) __pure2;

extern bool task_is_driver(task_t task);

#define TF_NONE                 0

#define TWF_NONE                 0
#define TRW_LRETURNWAIT          0x01           /* task is waiting for fork/posix_spawn/exec to complete */
#define TRW_LRETURNWAITER        0x02           /* task is waiting for TRW_LRETURNWAIT to get cleared */
#define TRW_LEXEC_COMPLETE       0x04           /* thread should call exec complete */

/* task clear return wait flags */
#define TCRW_CLEAR_INITIAL_WAIT   0x1
#define TCRW_CLEAR_FINAL_WAIT     0x2
#define TCRW_CLEAR_EXEC_COMPLETE  0x4
#define TCRW_CLEAR_ALL_WAIT       (TCRW_CLEAR_INITIAL_WAIT | TCRW_CLEAR_FINAL_WAIT)


#define TPF_NONE                0
#define TPF_EXEC_COPY           0x00000002                              /* task is the new copy of an exec */


__END_DECLS

#endif  /* MACH_KERNEL_PRIVATE */

__BEGIN_DECLS

#ifdef KERNEL_PRIVATE
extern boolean_t                task_is_app_suspended(task_t task);
extern bool task_is_exotic(task_t task);
extern bool task_is_alien(task_t task);
#endif /* KERNEL_PRIVATE */

#ifdef  XNU_KERNEL_PRIVATE

/* Hold all threads in a task, Wait for task to stop running, just to get off CPU */
extern kern_return_t task_hold_and_wait(
	task_t          task);

/* Release hold on all threads in a task */
extern kern_return_t    task_release(
	task_t          task);

/* Suspend/resume a task where the kernel owns the suspend count */
extern kern_return_t    task_suspend_internal_locked(   task_t          task);
extern kern_return_t    task_suspend_internal(          task_t          task);
extern kern_return_t    task_resume_internal_locked(    task_t          task);
extern kern_return_t    task_resume_internal(           task_t          task);

/* Suspends a task by placing a hold on its threads */
extern kern_return_t    task_pidsuspend(
	task_t          task);

/* Resumes a previously paused task */
extern kern_return_t    task_pidresume(
	task_t          task);

extern kern_return_t    task_send_trace_memory(
	task_t          task,
	uint32_t        pid,
	uint64_t        uniqueid);

extern void             task_remove_turnstile_watchports(
	task_t          task);

extern void             task_transfer_turnstile_watchports(
	task_t          old_task,
	task_t          new_task,
	thread_t        new_thread);

extern kern_return_t
    task_violated_guard(mach_exception_code_t, mach_exception_subcode_t, void *, bool);

#if DEVELOPMENT || DEBUG

extern kern_return_t    task_disconnect_page_mappings(
	task_t          task);
#endif /* DEVELOPMENT || DEBUG */

extern void                     tasks_system_suspend(boolean_t suspend);

#if CONFIG_FREEZE

/* Freeze a task's resident pages */
extern kern_return_t    task_freeze(
	task_t          task,
	uint32_t        *purgeable_count,
	uint32_t        *wired_count,
	uint32_t        *clean_count,
	uint32_t        *dirty_count,
	uint32_t        dirty_budget,
	uint32_t        *shared_count,
	int             *freezer_error_code,
	boolean_t       eval_only);

/* Thaw a currently frozen task */
extern kern_return_t    task_thaw(
	task_t          task);

typedef enum {
	CREDIT_TO_SWAP = 1,
	DEBIT_FROM_SWAP = 2
} freezer_acct_op_t;

extern void task_update_frozen_to_swap_acct(
	task_t  task,
	int64_t amount,
	freezer_acct_op_t op);

#endif /* CONFIG_FREEZE */

/* Halt all other threads in the current task */
extern kern_return_t    task_start_halt(
	task_t          task);

/* Wait for other threads to halt and free halting task resources */
extern void             task_complete_halt(
	task_t          task);

extern kern_return_t    task_terminate_internal(
	task_t                  task);

struct proc_ro;
typedef struct proc_ro *proc_ro_t;

extern kern_return_t    task_create_internal(
	task_t          parent_task,
	proc_ro_t       proc_ro,
	coalition_t     *parent_coalitions,
	boolean_t       inherit_memory,
	boolean_t       is_64bit,
	boolean_t       is_64bit_data,
	uint32_t        t_flags,
	uint32_t        t_flags_ro,
	uint32_t        procflags,
	uint8_t         t_returnwaitflags,
	task_t          child_task);

extern kern_return_t    task_set_special_port_internal(
	task_t                  task,
	int                     which,
	ipc_port_t              port);

extern kern_return_t task_set_security_tokens(
	task_t                  task,
	security_token_t        sec_token,
	audit_token_t           audit_token,
	host_priv_t             host_priv);

extern kern_return_t    task_info(
	task_t                  task,
	task_flavor_t           flavor,
	task_info_t             task_info_out,
	mach_msg_type_number_t  *task_info_count);

/*
 * Additional fields that aren't exposed through `task_power_info` but needed
 * by clients of `task_power_info_locked`.
 */
struct task_power_info_extra {
	uint64_t cycles;
	uint64_t instructions;
	uint64_t pcycles;
	uint64_t pinstructions;
	uint64_t user_ptime;
	uint64_t system_ptime;
	uint64_t runnable_time;
	uint64_t energy;
	uint64_t penergy;
	uint64_t secure_time;
	uint64_t secure_ptime;
};

void task_power_info_locked(
	task_t                        task,
	task_power_info_t             info,
	gpu_energy_data_t             gpu_energy,
	task_power_info_v2_t          infov2,
	struct task_power_info_extra *extra_info);

extern uint64_t         task_gpu_utilisation(
	task_t   task);

extern void             task_update_cpu_time_qos_stats(
	task_t   task,
	uint64_t *eqos_stats,
	uint64_t *rqos_stats);

extern void             task_vtimer_set(
	task_t          task,
	integer_t       which);

extern void             task_vtimer_clear(
	task_t          task,
	integer_t       which);

extern void             task_vtimer_update(
	task_t          task,
	integer_t       which,
	uint32_t        *microsecs);

#define TASK_VTIMER_USER                0x01
#define TASK_VTIMER_PROF                0x02
#define TASK_VTIMER_RLIM                0x04

extern void             task_set_64bit(
	task_t          task,
	boolean_t       is_64bit,
	boolean_t       is_64bit_data);

extern bool             task_get_64bit_addr(
	task_t task);

extern bool             task_get_64bit_data(
	task_t task);

extern void     task_set_platform_binary(
	task_t task,
	boolean_t is_platform);

#if XNU_TARGET_OS_OSX
#if DEVELOPMENT || DEBUG
/* Disables task identity security hardening (*_set_exception_ports policy)
 * for all tasks if amfi_get_out_of_my_way is set. */
extern bool AMFI_bootarg_disable_mach_hardening;
#endif /* DEVELOPMENT || DEBUG */
extern void             task_disable_mach_hardening(
	task_t task);

extern bool     task_opted_out_mach_hardening(
	task_t task);
#endif /* XNU_TARGET_OS_OSX */

extern boolean_t task_get_platform_binary(
	task_t task);

extern void
task_set_hardened_runtime(
	task_t task,
	bool is_hardened);

extern boolean_t
task_is_hardened_binary(
	task_t task);

extern boolean_t task_is_a_corpse(
	task_t task);

extern boolean_t task_is_ipc_active(
	task_t task);

extern void task_set_corpse(
	task_t task);

extern void     task_set_exc_guard_ctrl_port_default(
	task_t task,
	thread_t main_thread,
	const char *name,
	unsigned int namelen,
	boolean_t is_simulated,
	uint32_t platform,
	uint32_t sdk);

extern void task_set_immovable_pinned(task_t task);

extern bool     task_set_ca_client_wi(
	task_t task,
	boolean_t ca_client_wi);

extern kern_return_t task_set_dyld_info(
	task_t            task,
	mach_vm_address_t addr,
	mach_vm_size_t    size);

extern void task_set_mach_header_address(
	task_t task,
	mach_vm_address_t addr);

extern void task_set_uniqueid(task_t task);

/* Get number of activations in a task */
extern int              get_task_numacts(
	task_t          task);

extern bool task_donates_own_pages(
	task_t task);

struct label;
extern kern_return_t task_collect_crash_info(
	task_t task,
#if CONFIG_MACF
	struct label *crash_label,
#endif
	int is_corpse_fork);
void task_wait_till_threads_terminate_locked(task_t task);

/* JMM - should just be temporary (implementation in bsd_kern still) */
extern void     set_bsdtask_info(task_t, void *);
extern uint32_t set_task_loadTag(task_t task, uint32_t loadTag);
extern vm_map_t get_task_map_reference(task_t);
extern vm_map_t swap_task_map(task_t, thread_t, vm_map_t);
extern pmap_t   get_task_pmap(task_t);
extern uint64_t get_task_resident_size(task_t);
extern uint64_t get_task_compressed(task_t);
extern uint64_t get_task_resident_max(task_t);
extern uint64_t get_task_phys_footprint(task_t);
#if CONFIG_LEDGER_INTERVAL_MAX
extern uint64_t get_task_phys_footprint_interval_max(task_t, int reset);
#endif /* CONFIG_FOOTPRINT_INTERVAL_MAX */
extern uint64_t get_task_phys_footprint_lifetime_max(task_t);
extern uint64_t get_task_phys_footprint_limit(task_t);
extern uint64_t get_task_purgeable_size(task_t);
extern uint64_t get_task_cpu_time(task_t);
extern uint64_t get_task_dispatchqueue_offset(task_t);
extern uint64_t get_task_dispatchqueue_serialno_offset(task_t);
extern uint64_t get_task_dispatchqueue_label_offset(task_t);
extern uint64_t get_task_uniqueid(task_t task);
extern int      get_task_version(task_t task);

extern uint64_t get_task_internal(task_t);
extern uint64_t get_task_internal_compressed(task_t);
extern uint64_t get_task_purgeable_nonvolatile(task_t);
extern uint64_t get_task_purgeable_nonvolatile_compressed(task_t);
extern uint64_t get_task_iokit_mapped(task_t);
extern uint64_t get_task_alternate_accounting(task_t);
extern uint64_t get_task_alternate_accounting_compressed(task_t);
extern uint64_t get_task_memory_region_count(task_t);
extern uint64_t get_task_page_table(task_t);
#if CONFIG_FREEZE
extern uint64_t get_task_frozen_to_swap(task_t);
#endif
extern uint64_t get_task_network_nonvolatile(task_t);
extern uint64_t get_task_network_nonvolatile_compressed(task_t);
extern uint64_t get_task_wired_mem(task_t);
extern uint32_t get_task_loadTag(task_t task);

extern uint64_t get_task_tagged_footprint(task_t task);
extern uint64_t get_task_tagged_footprint_compressed(task_t task);
extern uint64_t get_task_media_footprint(task_t task);
extern uint64_t get_task_media_footprint_compressed(task_t task);
extern uint64_t get_task_graphics_footprint(task_t task);
extern uint64_t get_task_graphics_footprint_compressed(task_t task);
extern uint64_t get_task_neural_footprint(task_t task);
extern uint64_t get_task_neural_footprint_compressed(task_t task);

extern kern_return_t task_convert_phys_footprint_limit(int, int *);
extern kern_return_t task_set_phys_footprint_limit_internal(task_t, int, int *, boolean_t, boolean_t);
extern kern_return_t task_get_phys_footprint_limit(task_t task, int *limit_mb);
#if DEBUG || DEVELOPMENT
#if CONFIG_MEMORYSTATUS
extern kern_return_t task_set_diag_footprint_limit_internal(task_t, uint64_t, uint64_t *);
extern kern_return_t task_get_diag_footprint_limit_internal(task_t, uint64_t *, bool *);
extern kern_return_t task_set_diag_footprint_limit(task_t task, uint64_t new_limit_mb, uint64_t *old_limit_mb);
#endif /* CONFIG_MEMORYSTATUS */
#endif /* DEBUG || DEVELOPMENT */

extern security_token_t *task_get_sec_token(task_t task);
extern void task_set_sec_token(task_t task, security_token_t *token);
extern audit_token_t *task_get_audit_token(task_t task);
extern void task_set_audit_token(task_t task, audit_token_t *token);
extern void task_set_tokens(task_t task, security_token_t *sec_token, audit_token_t *audit_token);
extern boolean_t task_is_privileged(task_t task);
extern uint8_t *task_get_mach_trap_filter_mask(task_t task);
extern void task_set_mach_trap_filter_mask(task_t task, uint8_t *mask);
extern uint8_t *task_get_mach_kobj_filter_mask(task_t task);
extern void task_set_mach_kobj_filter_mask(task_t task, uint8_t *mask);
extern mach_vm_address_t task_get_all_image_info_addr(task_t task);

/* Jetsam memlimit attributes */
extern boolean_t task_get_memlimit_is_active(task_t task);
extern boolean_t task_get_memlimit_is_fatal(task_t task);
extern void task_set_memlimit_is_active(task_t task, boolean_t memlimit_is_active);
extern void task_set_memlimit_is_fatal(task_t task, boolean_t memlimit_is_fatal);
extern boolean_t task_has_triggered_exc_resource(task_t task, boolean_t memlimit_is_active);
extern void task_mark_has_triggered_exc_resource(task_t task, boolean_t memlimit_is_active);

extern uint64_t task_get_dirty_start(task_t task);
extern void task_set_dirty_start(task_t task, uint64_t start);

extern void task_set_thread_limit(task_t task, uint16_t thread_limit);
#if CONFIG_PROC_RESOURCE_LIMITS
extern kern_return_t task_set_port_space_limits(task_t task, uint32_t soft_limit, uint32_t hard_limit);
#endif /* CONFIG_PROC_RESOURCE_LIMITS */
extern void task_port_space_ast(task_t task);

#if XNU_TARGET_OS_OSX
extern boolean_t task_has_system_version_compat_enabled(task_t task);
extern void task_set_system_version_compat_enabled(task_t task, boolean_t enable_system_version_compat);
#endif

extern boolean_t        is_kerneltask(task_t task);
extern boolean_t        is_corpsefork(task_t task);

extern kern_return_t check_actforsig(task_t task, thread_t thread, int setast);

extern kern_return_t machine_task_get_state(
	task_t task,
	int flavor,
	thread_state_t state,
	mach_msg_type_number_t *state_count);

extern kern_return_t machine_task_set_state(
	task_t task,
	int flavor,
	thread_state_t state,
	mach_msg_type_number_t state_count);

extern void machine_task_terminate(task_t task);

extern kern_return_t machine_task_process_signature(task_t task, uint32_t platform, uint32_t sdk, char const **error_msg);

struct _task_ledger_indices {
	int cpu_time;
	int tkm_private;
	int tkm_shared;
	int phys_mem;
	int wired_mem;
	int conclave_mem;
	int internal;
	int iokit_mapped;
	int external;
	int reusable;
	int alternate_accounting;
	int alternate_accounting_compressed;
	int page_table;
	int phys_footprint;
	int internal_compressed;
	int purgeable_volatile;
	int purgeable_nonvolatile;
	int purgeable_volatile_compressed;
	int purgeable_nonvolatile_compressed;
	int tagged_nofootprint;
	int tagged_footprint;
	int tagged_nofootprint_compressed;
	int tagged_footprint_compressed;
	int network_volatile;
	int network_nonvolatile;
	int network_volatile_compressed;
	int network_nonvolatile_compressed;
	int media_nofootprint;
	int media_footprint;
	int media_nofootprint_compressed;
	int media_footprint_compressed;
	int graphics_nofootprint;
	int graphics_footprint;
	int graphics_nofootprint_compressed;
	int graphics_footprint_compressed;
	int neural_nofootprint;
	int neural_footprint;
	int neural_nofootprint_compressed;
	int neural_footprint_compressed;
	int platform_idle_wakeups;
	int interrupt_wakeups;
#if CONFIG_SCHED_SFI
	int sfi_wait_times[MAX_SFI_CLASS_ID];
#endif /* CONFIG_SCHED_SFI */
	int cpu_time_billed_to_me;
	int cpu_time_billed_to_others;
	int physical_writes;
	int logical_writes;
	int logical_writes_to_external;
	int energy_billed_to_me;
	int energy_billed_to_others;
#if CONFIG_MEMORYSTATUS
	int memorystatus_dirty_time;
#endif /* CONFIG_MEMORYSTATUS */
#if DEBUG || DEVELOPMENT
	int pages_grabbed;
	int pages_grabbed_kern;
	int pages_grabbed_iopl;
	int pages_grabbed_upl;
#endif
#if CONFIG_FREEZE
	int frozen_to_swap;
#endif /* CONFIG_FREEZE */
#if CONFIG_PHYS_WRITE_ACCT
	int fs_metadata_writes;
#endif /* CONFIG_PHYS_WRITE_ACCT */
	int swapins;
};

/*
 * Many of the task ledger entries use a reduced feature set
 * (specifically they just use LEDGER_ENTRY_ALLOW_PANIC_ON_NEGATIVE)
 * and are stored in a smaller entry structure.
 * That structure is an implementation detail of the ledger.
 * But on PPL systems, the task ledger's memory is managed by the PPL
 * and it has to determine the size of the task ledger at compile time.
 * This define specifies the number of small entries so the PPL can
 * properly determine the ledger's size.
 *
 * If you add a new entry with only the
 * LEDGER_ENTRY_ALLOW_PANIC_ON_NEGATIVE | LEDGER_ENTRY_ALLOW_INACTIVE
 * flags, you need to increment this count.
 * Otherwise, PPL systems will panic at boot.
 */
#if DEVELOPMENT || DEBUG
#define TASK_LEDGER_NUM_SMALL_INDICES 33
#else
#define TASK_LEDGER_NUM_SMALL_INDICES 29
#endif /* DEVELOPMENT || DEBUG */
extern struct _task_ledger_indices task_ledgers;

/* requires task to be unlocked, returns a referenced thread */
thread_t task_findtid(task_t task, uint64_t tid);
int pid_from_task(task_t task);

extern kern_return_t task_wakeups_monitor_ctl(task_t task, uint32_t *rate_hz, int32_t *flags);
extern kern_return_t task_cpu_usage_monitor_ctl(task_t task, uint32_t *flags);
extern void task_rollup_accounting_info(task_t new_task, task_t parent_task);
extern kern_return_t task_io_monitor_ctl(task_t task, uint32_t *flags);
extern void task_set_did_exec_flag(task_t task);
extern void task_clear_exec_copy_flag(task_t task);
extern boolean_t task_is_exec_copy(task_t);
extern boolean_t task_did_exec(task_t task);
extern boolean_t task_is_active(task_t task);
extern boolean_t task_is_halting(task_t task);
extern void task_clear_return_wait(task_t task, uint32_t flags);
extern void task_wait_to_return(void) __attribute__((noreturn));
extern event_t task_get_return_wait_event(task_t task);

extern void task_bank_reset(task_t task);
extern void task_bank_init(task_t task);

#if CONFIG_MEMORYSTATUS
extern void task_ledger_settle_dirty_time(task_t t);
#endif /* CONFIG_MEMORYSTATUS */

#if CONFIG_ARCADE
extern void task_prep_arcade(task_t task, thread_t thread);
#endif /* CONFIG_ARCADE */

extern int task_pid(task_t task);

#if __has_feature(ptrauth_calls)
char *task_get_vm_shared_region_id_and_jop_pid(task_t task, uint64_t *);
void task_set_shared_region_id(task_t task, char *id);
#endif /* __has_feature(ptrauth_calls) */

extern boolean_t task_has_assertions(task_t task);
/* End task_policy */

extern void      task_set_gpu_denied(task_t task, boolean_t denied);
extern boolean_t task_is_gpu_denied(task_t task);

extern void task_set_game_mode(task_t task, bool enabled);
/* returns true if update must be pushed to coalition (Automatically handled by task_set_game_mode) */
extern bool task_set_game_mode_locked(task_t task, bool enabled);
extern bool task_get_game_mode(task_t task);

extern queue_head_t * task_io_user_clients(task_t task);
extern void     task_set_message_app_suspended(task_t task, boolean_t enable);

extern void task_copy_fields_for_exec(task_t dst_task, task_t src_task);

extern void task_copy_vmobjects(task_t task, vm_object_query_t query, size_t len, size_t *num);
extern void task_get_owned_vmobjects(task_t task, size_t buffer_size, vmobject_list_output_t buffer, size_t* output_size, size_t* entries);
extern void task_store_owned_vmobject_info(task_t to_task, task_t from_task);

extern void task_set_filter_msg_flag(task_t task, boolean_t flag);
extern boolean_t task_get_filter_msg_flag(task_t task);

#if __has_feature(ptrauth_calls)
extern bool task_is_pac_exception_fatal(task_t task);
extern void task_set_pac_exception_fatal_flag(task_t task);
#endif /*__has_feature(ptrauth_calls)*/

extern bool task_is_jit_exception_fatal(task_t task);
extern void task_set_jit_exception_fatal_flag(task_t task);

extern bool task_needs_user_signed_thread_state(task_t task);
extern void task_set_tecs(task_t task);
extern void task_get_corpse_vmobject_list(task_t task, vmobject_list_output_t* list, size_t* list_size);

extern boolean_t task_corpse_forking_disabled(task_t task);

void __attribute__((noinline)) SENDING_NOTIFICATION__THIS_PROCESS_HAS_TOO_MANY_MACH_PORTS(task_t task,
    uint32_t current_size, uint32_t soft_limit, uint32_t hard_limit);

extern int get_task_cdhash(task_t task, char cdhash[CS_CDHASH_LEN]);

extern boolean_t kdp_task_is_locked(task_t task);

/* Kernel side prototypes for MIG routines */
extern kern_return_t task_get_exception_ports(
	task_t                          task,
	exception_mask_t                exception_mask,
	exception_mask_array_t          masks,
	mach_msg_type_number_t          *CountCnt,
	exception_port_array_t          ports,
	exception_behavior_array_t      behaviors,
	thread_state_flavor_array_t     flavors);

#if CONFIG_EXCLAVES
int task_add_conclave(task_t task, void *, int64_t, const char *task_conclave_id);
kern_return_t task_inherit_conclave(task_t old_task, task_t new_task, void *vnode, int64_t off);
kern_return_t task_launch_conclave(mach_port_name_t port);
void task_clear_conclave(task_t task);
void task_stop_conclave(task_t task, bool gather_crash_bt);
kern_return_t task_stop_conclave_upcall(void);
kern_return_t task_stop_conclave_upcall_complete(void);
kern_return_t task_suspend_conclave_upcall(uint64_t *, size_t);
struct xnuupcalls_conclavesharedbuffer_s;
kern_return_t task_crash_info_conclave_upcall(task_t task,
    const struct xnuupcalls_conclavesharedbuffer_s *shared_buf, uint32_t length);
typedef struct exclaves_resource exclaves_resource_t;
exclaves_resource_t *task_get_conclave(task_t task);
void task_set_conclave_untaintable(task_t task);
void task_add_conclave_crash_info(task_t task, void *crash_info_ptr);
//Changing this would also warrant a change in ConclaveSharedBuffer
#define CONCLAVE_CRASH_BUFFER_PAGECOUNT 2

#endif /* CONFIG_EXCLAVES */

#endif  /* XNU_KERNEL_PRIVATE */
#ifdef  KERNEL_PRIVATE

extern void     *get_bsdtask_info(task_t);
extern void     *get_bsdthreadtask_info(thread_t);
extern void task_bsdtask_kill(task_t);
extern vm_map_t get_task_map(task_t);
extern ledger_t get_task_ledger(task_t);

extern boolean_t get_task_pidsuspended(task_t);
extern boolean_t get_task_suspended(task_t);
extern boolean_t get_task_frozen(task_t);

/*
 * Flavors of convert_task_to_port. XNU callers get convert_task_to_port_kernel,
 * external callers get convert_task_to_port_external.
 */
extern ipc_port_t convert_task_to_port(task_t);
extern ipc_port_t convert_task_to_port_kernel(task_t);
extern ipc_port_t convert_task_to_port_external(task_t);
extern ipc_port_t convert_task_to_port_pinned(task_t);

extern ipc_port_t convert_task_read_to_port(task_t);
extern ipc_port_t convert_task_read_to_port_kernel(task_read_t);
extern ipc_port_t convert_task_read_to_port_external(task_t);

extern ipc_port_t convert_task_inspect_to_port(task_inspect_t);
extern ipc_port_t convert_task_name_to_port(task_name_t);

extern ipc_port_t convert_corpse_to_port_and_nsrequest(task_t task);

extern ipc_port_t convert_task_suspension_token_to_port(task_suspension_token_t task);
/* Convert from a port (in this case, an SO right to a task's resume port) to a task. */
extern task_suspension_token_t convert_port_to_task_suspension_token(ipc_port_t port);

extern void task_suspension_send_once(ipc_port_t port);

#define TASK_WRITE_IMMEDIATE                 0x1
#define TASK_WRITE_DEFERRED                  0x2
#define TASK_WRITE_INVALIDATED               0x4
#define TASK_WRITE_METADATA                  0x8
extern void     task_update_logical_writes(task_t task, uint32_t io_size, int flags, void *vp);

__enum_decl(task_balance_flags_t, uint8_t, {
	TASK_BALANCE_CREDIT                 = 0x1,
	TASK_BALANCE_DEBIT                  = 0x2,
});

__enum_decl(task_physical_write_flavor_t, uint8_t, {
	TASK_PHYSICAL_WRITE_METADATA        = 0x1,
});
extern void     task_update_physical_writes(task_t task, task_physical_write_flavor_t flavor,
    uint64_t io_size, task_balance_flags_t flags);

#if CONFIG_SECLUDED_MEMORY
extern void task_set_can_use_secluded_mem(
	task_t task,
	boolean_t can_use_secluded_mem);
extern void task_set_could_use_secluded_mem(
	task_t task,
	boolean_t could_use_secluded_mem);
extern void task_set_could_also_use_secluded_mem(
	task_t task,
	boolean_t could_also_use_secluded_mem);
extern boolean_t task_can_use_secluded_mem(
	task_t task,
	boolean_t is_allocate);
extern boolean_t task_could_use_secluded_mem(task_t task);
extern boolean_t task_could_also_use_secluded_mem(task_t task);
#endif /* CONFIG_SECLUDED_MEMORY */

extern void task_set_darkwake_mode(task_t, boolean_t);
extern boolean_t task_get_darkwake_mode(task_t);

#if __arm64__
extern void task_set_legacy_footprint(task_t task);
extern void task_set_extra_footprint_limit(task_t task);
extern void task_set_ios13extended_footprint_limit(task_t task);
#endif /* __arm64__ */

#if CONFIG_MACF
extern struct label *get_task_crash_label(task_t task);
extern void set_task_crash_label(task_t task, struct label *label);
#endif /* CONFIG_MACF */

#endif  /* KERNEL_PRIVATE */

extern task_t   kernel_task;

extern void             task_name_deallocate_mig(
	task_name_t             task_name);

extern void             task_policy_set_deallocate_mig(
	task_policy_set_t       task_policy_set);

extern void             task_policy_get_deallocate_mig(
	task_policy_get_t       task_policy_get);

extern void             task_inspect_deallocate_mig(
	task_inspect_t          task_inspect);

extern void             task_read_deallocate_mig(
	task_read_t          task_read);

extern void             task_suspension_token_deallocate(
	task_suspension_token_t token);

extern boolean_t task_self_region_footprint(void);
extern void task_self_region_footprint_set(boolean_t newval);
extern void task_ledgers_footprint(ledger_t ledger,
    ledger_amount_t *ledger_resident,
    ledger_amount_t *ledger_compressed);
extern void task_set_memory_ownership_transfer(
	task_t task,
	boolean_t value);

#if DEVELOPMENT || DEBUG
extern void task_set_no_footprint_for_debug(
	task_t task,
	boolean_t value);
extern int task_get_no_footprint_for_debug(
	task_t task);
#endif /* DEVELOPMENT || DEBUG */

#ifdef KERNEL_PRIVATE
extern kern_return_t task_get_suspend_stats(task_t task, task_suspend_stats_t stats);
extern kern_return_t task_get_suspend_stats_kdp(task_t task, task_suspend_stats_t stats);
#endif /* KERNEL_PRIVATE*/

#ifdef XNU_KERNEL_PRIVATE
extern kern_return_t task_get_suspend_sources(task_t task, task_suspend_source_array_t sources);
extern kern_return_t task_get_suspend_sources_kdp(task_t task, task_suspend_source_array_t sources);
#endif /* XNU_KERNEL_PRIVATE */

#if CONFIG_ROSETTA
extern bool task_is_translated(task_t task);
#endif



#ifdef MACH_KERNEL_PRIVATE
void task_procname(task_t task, char *buf, int size);
void task_best_name(task_t task, char *buf, size_t size);
#endif /* MACH_KERNEL_PRIVATE */

__END_DECLS

#endif  /* _KERN_TASK_H_ */
