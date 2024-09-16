/*
 * Copyright (c) 2006-2018 Apple Computer, Inc. All rights reserved.
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

#ifndef SYS_MEMORYSTATUS_H
#define SYS_MEMORYSTATUS_H

#include <sys/time.h>
#include <mach_debug/zone_info.h>
#include <sys/proc.h>
#include <sys/reason.h>
#include <stdbool.h>

#define MEMORYSTATUS_ENTITLEMENT "com.apple.private.memorystatus"

#define JETSAM_PRIORITY_REVISION                  2

#define JETSAM_PRIORITY_IDLE_HEAD                -2
/* The value -1 is an alias to JETSAM_PRIORITY_DEFAULT */
#define JETSAM_PRIORITY_IDLE                      0
#define JETSAM_PRIORITY_ENTITLED_MAX              9 /* Entitled processes may use bands 1-9 for experimentation */
#define JETSAM_PRIORITY_IDLE_DEFERRED             10 /* Keeping this around till all xnu_quick_tests can be moved away from it.*/
#define JETSAM_PRIORITY_AGING_BAND1               JETSAM_PRIORITY_IDLE_DEFERRED
#define JETSAM_PRIORITY_AGING_BAND1_STUCK         15 /* Sysprocs stuck in JETSAM_PRIORITY_IDLE_DEFERRED will be moved to this band */
#define JETSAM_PRIORITY_BACKGROUND_OPPORTUNISTIC  20
#define JETSAM_PRIORITY_AGING_BAND2               JETSAM_PRIORITY_BACKGROUND_OPPORTUNISTIC
#define JETSAM_PRIORITY_BACKGROUND                30
/*
 * NB: This band is no longer used by mail, but IS used by many active
 * processes doing background work.
 */
#define JETSAM_PRIORITY_MAIL                      40
#define JETSAM_PRIORITY_ELEVATED_INACTIVE         JETSAM_PRIORITY_MAIL
#define JETSAM_PRIORITY_PHONE                     50
#define JETSAM_PRIORITY_FREEZER                   75
#define JETSAM_PRIORITY_UI_SUPPORT                80
#define JETSAM_PRIORITY_FOREGROUND_SUPPORT        90
#define JETSAM_PRIORITY_FOREGROUND               100
#define JETSAM_PRIORITY_AUDIO_AND_ACCESSORY      120
#define JETSAM_PRIORITY_CONDUCTOR                130
#define JETSAM_PRIORITY_DRIVER_APPLE             150
#define JETSAM_PRIORITY_HOME                     160
#define JETSAM_PRIORITY_EXECUTIVE                170
#define JETSAM_PRIORITY_IMPORTANT                180
#define JETSAM_PRIORITY_CRITICAL                 190

#define JETSAM_PRIORITY_MAX                      210

/*
 * Used to show that a process is so high priority it is not processed by jetsam at all.
 * This is set on launchd and kernel_task.
 */
#define JETSAM_PRIORITY_INTERNAL                 999

/* TODO - tune. This should probably be lower priority */
#define JETSAM_PRIORITY_DEFAULT                  180
#define JETSAM_PRIORITY_TELEPHONY                190

/* Compatibility */
#define DEFAULT_JETSAM_PRIORITY                  180

/*
 * The deferral time used by default for apps and daemons in all aging
 * policies except kJetsamAgingPolicySysProcsReclaimedFirst is
 * DEFERRED_IDLE_EXIT_TIME_SECS.
 *
 * For kJetsamAgingPolicySysProcsReclaimedFirst,
 *
 * Daemons: The actual idle deferred time for the daemon is based on
 * the relaunch behavior of the daemon. The relaunch behavior determines
 * the scaling factor applied to DEFERRED_IDLE_EXIT_TIME_SECS. See
 * kJetsamSysProcsIdleDelayTime* ratios defined in kern_memorystatus.c
 *
 * Apps: The apps are aged for DEFERRED_IDLE_EXIT_TIME_SECS factored
 * by kJetsamAppsIdleDelayTimeRatio.
 */
#define DEFERRED_IDLE_EXIT_TIME_SECS             10

#define KEV_MEMORYSTATUS_SUBCLASS                 3

enum {
	kMemorystatusLevelNote = 1,
	kMemorystatusSnapshotNote = 2,
	kMemorystatusFreezeNote = 3,
	kMemorystatusPressureNote = 4
};

enum {
	kMemorystatusLevelAny = -1,
	kMemorystatusLevelNormal = 0,
	kMemorystatusLevelWarning = 1,
	kMemorystatusLevelUrgent = 2,
	kMemorystatusLevelCritical = 3
};

typedef struct memorystatus_priority_entry {
	pid_t pid;
	int32_t priority;
	uint64_t user_data;
	int32_t limit;  /* MB */
	uint32_t state;
} memorystatus_priority_entry_t;

/*
 * This should be the structure to specify different properties
 * for processes (group or single) from user-space. Unfortunately,
 * we can't move to it completely because the priority_entry structure
 * above has been in use for a while now. We'll have to deprecate it.
 *
 * To support new fields/properties, we will add a new structure with a
 * new version and a new size.
 */
#define MEMORYSTATUS_MPE_VERSION_1              1

#define MEMORYSTATUS_MPE_VERSION_1_SIZE         sizeof(struct memorystatus_properties_entry_v1)

typedef struct memorystatus_properties_entry_v1 {
	int version;
	pid_t pid;
	int32_t priority;
	int use_probability;
	uint64_t user_data;
	int32_t limit;  /* MB */
	uint32_t state;
	char proc_name[MAXCOMLEN + 1];
	char __pad1[3];
} memorystatus_properties_entry_v1_t;

/*
 * Represents a freeze or demotion candidate.
 */
typedef struct memorystatus_properties_freeze_entry_v1 {
	int version;
	pid_t pid;
	uint32_t priority;
	char proc_name[(2 * MAXCOMLEN) + 1];
	char __pad1[3];
} memorystatus_properties_freeze_entry_v1;

typedef struct memorystatus_kernel_stats {
	uint32_t free_pages;
	uint32_t active_pages;
	uint32_t inactive_pages;
	uint32_t throttled_pages;
	uint32_t purgeable_pages;
	uint32_t wired_pages;
	uint32_t speculative_pages;
	uint32_t filebacked_pages;
	uint32_t anonymous_pages;
	uint32_t compressor_pages;
	uint64_t compressions;
	uint64_t decompressions;
	uint64_t total_uncompressed_pages_in_compressor;
	uint64_t zone_map_size;
	uint64_t zone_map_capacity;
	uint64_t largest_zone_size;
	char     largest_zone_name[MACH_ZONE_NAME_MAX_LEN];
} memorystatus_kernel_stats_t;

typedef enum memorystatus_freeze_skip_reason {
	kMemorystatusFreezeSkipReasonNone = 0,
	kMemorystatusFreezeSkipReasonExcessSharedMemory = 1,
	kMemorystatusFreezeSkipReasonLowPrivateSharedRatio = 2,
	kMemorystatusFreezeSkipReasonNoCompressorSpace = 3,
	kMemorystatusFreezeSkipReasonNoSwapSpace = 4,
	kMemorystatusFreezeSkipReasonBelowMinPages = 5,
	kMemorystatusFreezeSkipReasonLowProbOfUse = 6,
	kMemorystatusFreezeSkipReasonOther = 7,
	kMemorystatusFreezeSkipReasonOutOfBudget = 8,
	kMemorystatusFreezeSkipReasonOutOfSlots = 9,
	kMemorystatusFreezeSkipReasonDisabled = 10,
	kMemorystatusFreezeSkipReasonElevated = 11,
	_kMemorystatusFreezeSkipReasonMax
} memorystatus_freeze_skip_reason_t;
/*
** This is a variable-length struct.
** Allocate a buffer of the size returned by the sysctl, cast to a memorystatus_snapshot_t *
*/

typedef struct jetsam_snapshot_entry {
	pid_t    pid;
	char     name[(2 * MAXCOMLEN) + 1];
	int32_t  priority;
	uint32_t state;
	uint32_t fds;
	memorystatus_freeze_skip_reason_t jse_freeze_skip_reason; /* why wasn't this process frozen? */
	uint8_t  uuid[16];
	uint64_t user_data;
	uint64_t killed;
	uint64_t pages;
	uint64_t max_pages_lifetime;
	uint64_t purgeable_pages;
	uint64_t jse_internal_pages;
	uint64_t jse_internal_compressed_pages;
	uint64_t jse_purgeable_nonvolatile_pages;
	uint64_t jse_purgeable_nonvolatile_compressed_pages;
	uint64_t jse_alternate_accounting_pages;
	uint64_t jse_alternate_accounting_compressed_pages;
	uint64_t jse_iokit_mapped_pages;
	uint64_t jse_page_table_pages;
	uint64_t jse_memory_region_count;
	uint64_t jse_gencount;                  /* memorystatus_thread generation counter */
	uint64_t jse_starttime;                 /* absolute time when process starts */
	uint64_t jse_killtime;                  /* absolute time when jetsam chooses to kill a process */
	uint64_t jse_idle_delta;                /* time spent in idle band */
	uint64_t jse_coalition_jetsam_id;       /* we only expose coalition id for COALITION_TYPE_JETSAM */
	struct timeval64 cpu_time;
	uint64_t jse_thaw_count;
	uint64_t jse_frozen_to_swap_pages;
	uint64_t csflags;
	uint32_t cs_trust_level;
	uint64_t jse_neural_nofootprint_total_pages;
} memorystatus_jetsam_snapshot_entry_t;

typedef struct jetsam_snapshot {
	uint64_t snapshot_time;                 /* absolute time snapshot was initialized */
	uint64_t notification_time;             /* absolute time snapshot was consumed */
	uint64_t js_gencount;                   /* memorystatus_thread generation counter */
	memorystatus_kernel_stats_t stats;      /* system stat when snapshot is initialized */
	size_t entry_count;
	memorystatus_jetsam_snapshot_entry_t entries[];
} memorystatus_jetsam_snapshot_t;

/* TODO - deprecate; see <rdar://problem/12969599> */
#define kMaxSnapshotEntries 192

/*
 * default jetsam snapshot support
 */
extern memorystatus_jetsam_snapshot_t *memorystatus_jetsam_snapshot;
extern memorystatus_jetsam_snapshot_t *memorystatus_jetsam_snapshot_copy;
#if CONFIG_FREEZE
extern memorystatus_jetsam_snapshot_t *memorystatus_jetsam_snapshot_freezer;
extern unsigned int memorystatus_jetsam_snapshot_freezer_max;
extern unsigned int memorystatus_jetsam_snapshot_freezer_size;
#endif /* CONFIG_FREEZE */
extern unsigned int memorystatus_jetsam_snapshot_count;
extern unsigned int memorystatus_jetsam_snapshot_copy_count;
extern unsigned int memorystatus_jetsam_snapshot_max;
extern unsigned int memorystatus_jetsam_snapshot_size;
extern uint64_t memorystatus_jetsam_snapshot_last_timestamp;
extern uint64_t memorystatus_jetsam_snapshot_timeout;
#define memorystatus_jetsam_snapshot_list memorystatus_jetsam_snapshot->entries
#define JETSAM_SNAPSHOT_TIMEOUT_SECS 30

/* General memorystatus stuff */

extern uint64_t memorystatus_sysprocs_idle_delay_time;
extern uint64_t memorystatus_apps_idle_delay_time;

/* State */
#define kMemorystatusSuspended        0x01
#define kMemorystatusFrozen           0x02
#define kMemorystatusWasThawed        0x04
#define kMemorystatusTracked          0x08
#define kMemorystatusSupportsIdleExit 0x10
#define kMemorystatusDirty            0x20
#define kMemorystatusAssertion        0x40

/*
 * Jetsam exit reason definitions - related to memorystatus
 *
 * When adding new exit reasons also update:
 *	JETSAM_REASON_MEMORYSTATUS_MAX
 *	kMemorystatusKilled... Cause enum
 *	memorystatus_kill_cause_name[]
 */
#define JETSAM_REASON_INVALID                            0
#define JETSAM_REASON_GENERIC                            1
#define JETSAM_REASON_MEMORY_HIGHWATER                   2
#define JETSAM_REASON_VNODE                              3
#define JETSAM_REASON_MEMORY_VMPAGESHORTAGE              4
#define JETSAM_REASON_MEMORY_PROCTHRASHING               5
#define JETSAM_REASON_MEMORY_FCTHRASHING                 6
#define JETSAM_REASON_MEMORY_PERPROCESSLIMIT             7
#define JETSAM_REASON_MEMORY_DISK_SPACE_SHORTAGE         8
#define JETSAM_REASON_MEMORY_IDLE_EXIT                   9
#define JETSAM_REASON_ZONE_MAP_EXHAUSTION                10
#define JETSAM_REASON_MEMORY_VMCOMPRESSOR_THRASHING      11
#define JETSAM_REASON_MEMORY_VMCOMPRESSOR_SPACE_SHORTAGE 12
#define JETSAM_REASON_LOWSWAP                            13
#define JETSAM_REASON_MEMORY_SUSTAINED_PRESSURE          14
#define JETSAM_REASON_MEMORY_VMPAGEOUT_STARVATION        15
#define JETSAM_REASON_MEMORYSTATUS_MAX  JETSAM_REASON_MEMORY_VMPAGEOUT_STARVATION
/* non-memorystatus jetsam reasons */
#define JETSAM_REASON_CPULIMIT                           100
typedef uint64_t jetsam_reason_t;

/* memorystatus kill cause */
typedef enum {
	kMemorystatusInvalid = JETSAM_REASON_INVALID,
	kMemorystatusKilled = JETSAM_REASON_GENERIC,
	kMemorystatusKilledHiwat = JETSAM_REASON_MEMORY_HIGHWATER,
	kMemorystatusKilledVnodes = JETSAM_REASON_VNODE,
	kMemorystatusKilledVMPageShortage = JETSAM_REASON_MEMORY_VMPAGESHORTAGE,
	kMemorystatusKilledProcThrashing = JETSAM_REASON_MEMORY_PROCTHRASHING,
	kMemorystatusKilledFCThrashing = JETSAM_REASON_MEMORY_FCTHRASHING,
	kMemorystatusKilledPerProcessLimit = JETSAM_REASON_MEMORY_PERPROCESSLIMIT,
	kMemorystatusKilledDiskSpaceShortage = JETSAM_REASON_MEMORY_DISK_SPACE_SHORTAGE,
	kMemorystatusKilledIdleExit = JETSAM_REASON_MEMORY_IDLE_EXIT,
	kMemorystatusKilledZoneMapExhaustion = JETSAM_REASON_ZONE_MAP_EXHAUSTION,
	kMemorystatusKilledVMCompressorThrashing = JETSAM_REASON_MEMORY_VMCOMPRESSOR_THRASHING,
	kMemorystatusKilledVMCompressorSpaceShortage = JETSAM_REASON_MEMORY_VMCOMPRESSOR_SPACE_SHORTAGE,
	kMemorystatusKilledLowSwap = JETSAM_REASON_LOWSWAP,
	kMemorystatusKilledSustainedPressure = JETSAM_REASON_MEMORY_SUSTAINED_PRESSURE,
	kMemorystatusKilledVMPageoutStarvation = JETSAM_REASON_MEMORY_VMPAGEOUT_STARVATION,
} memorystatus_kill_cause_t;

/*
 * For backwards compatibility
 * Keeping these around for external users (e.g. ReportCrash, Ariadne).
 * TODO: Remove once they stop using these.
 */
#define kMemorystatusKilledDiagnostic           kMemorystatusKilledDiskSpaceShortage
#define kMemorystatusKilledVMThrashing          kMemorystatusKilledVMCompressorThrashing
#define JETSAM_REASON_MEMORY_VMTHRASHING        JETSAM_REASON_MEMORY_VMCOMPRESSOR_THRASHING

/* Memorystatus control */
#define MEMORYSTATUS_BUFFERSIZE_MAX 65536

#ifndef KERNEL
__BEGIN_DECLS
int memorystatus_get_level(user_addr_t level);
int memorystatus_control(uint32_t command, int32_t pid, uint32_t flags, void *buffer, size_t buffersize);
__END_DECLS
#endif

/* Commands */
#define MEMORYSTATUS_CMD_GET_PRIORITY_LIST            1
#define MEMORYSTATUS_CMD_SET_PRIORITY_PROPERTIES      2
#define MEMORYSTATUS_CMD_GET_JETSAM_SNAPSHOT          3
#define MEMORYSTATUS_CMD_GET_PRESSURE_STATUS          4
#define MEMORYSTATUS_CMD_SET_JETSAM_HIGH_WATER_MARK   5    /* Set active memory limit = inactive memory limit, both non-fatal	*/
#define MEMORYSTATUS_CMD_SET_JETSAM_TASK_LIMIT        6    /* Set active memory limit = inactive memory limit, both fatal	*/
#define MEMORYSTATUS_CMD_SET_MEMLIMIT_PROPERTIES      7    /* Set memory limits plus attributes independently			*/
#define MEMORYSTATUS_CMD_GET_MEMLIMIT_PROPERTIES      8    /* Get memory limits plus attributes					*/
#define MEMORYSTATUS_CMD_PRIVILEGED_LISTENER_ENABLE   9    /* Set the task's status as a privileged listener w.r.t memory notifications  */
#define MEMORYSTATUS_CMD_PRIVILEGED_LISTENER_DISABLE  10   /* Reset the task's status as a privileged listener w.r.t memory notifications  */
#define MEMORYSTATUS_CMD_AGGRESSIVE_JETSAM_LENIENT_MODE_ENABLE  11   /* Enable the 'lenient' mode for aggressive jetsam. See comments in kern_memorystatus.c near the top. */
#define MEMORYSTATUS_CMD_AGGRESSIVE_JETSAM_LENIENT_MODE_DISABLE 12   /* Disable the 'lenient' mode for aggressive jetsam. */
#define MEMORYSTATUS_CMD_GET_MEMLIMIT_EXCESS          13   /* Compute how much a process's phys_footprint exceeds inactive memory limit */
#define MEMORYSTATUS_CMD_ELEVATED_INACTIVEJETSAMPRIORITY_ENABLE         14 /* Set the inactive jetsam band for a process to JETSAM_PRIORITY_ELEVATED_INACTIVE */
#define MEMORYSTATUS_CMD_ELEVATED_INACTIVEJETSAMPRIORITY_DISABLE        15 /* Reset the inactive jetsam band for a process to the default band (0)*/
#define MEMORYSTATUS_CMD_SET_PROCESS_IS_MANAGED       16   /* (Re-)Set state on a process that marks it as (un-)managed by a system entity e.g. assertiond */
#define MEMORYSTATUS_CMD_GET_PROCESS_IS_MANAGED       17   /* Return the 'managed' status of a process */
#define MEMORYSTATUS_CMD_SET_PROCESS_IS_FREEZABLE     18   /* Is the process eligible for freezing? Apps and extensions can pass in FALSE to opt out of freezing, i.e.,
	                                                    *  if they would prefer being jetsam'ed in the idle band to being frozen in an elevated band. */
#define MEMORYSTATUS_CMD_GET_PROCESS_IS_FREEZABLE     19   /* Return the freezable state of a process. */

#define MEMORYSTATUS_CMD_FREEZER_CONTROL              20

#define MEMORYSTATUS_CMD_GET_AGGRESSIVE_JETSAM_LENIENT_MODE      21   /* Query if the lenient mode for aggressive jetsam is enabled. */

#define MEMORYSTATUS_CMD_INCREASE_JETSAM_TASK_LIMIT   22   /* Used by DYLD to increase the jetsam active and inactive limits, when using roots */

#if PRIVATE
#define MEMORYSTATUS_CMD_SET_TESTING_PID 23 /* Used by unit tests in the development kernel only. */
#endif /* PRIVATE */

#define MEMORYSTATUS_CMD_GET_PROCESS_IS_FROZEN 24 /* Check if the process is frozen. */

#define MEMORYSTATUS_CMD_MARK_PROCESS_COALITION_SWAPPABLE 25 /* Set the coalition led by this process as swappable. This is a one-way transition. Swappable coalitions can never be made non-swappable. */
#define MEMORYSTATUS_CMD_GET_PROCESS_COALITION_IS_SWAPPABLE 26 /* Get the swappable status for this process' coalition. */

#define MEMORYSTATUS_CMD_CONVERT_MEMLIMIT_MB 28 /* Given a memlimit value (which may be 0 or -1), convert it to an actual limit in megabytes. */

#define MEMORYSTATUS_CMD_SET_DIAG_LIMIT               30 /* Set the diagnostics memory limit */
#define MEMORYSTATUS_CMD_GET_DIAG_LIMIT               31 /* Get the diagnostics memory limit */

#define MEMORYSTATUS_CMD_GET_JETSAM_ZPRINT_NAMES      32   /* Get jetsam snapshot zprint names array */
#define MEMORYSTATUS_CMD_GET_JETSAM_ZPRINT_INFO       33   /* Get jetsam snapshot zprint zone info */
#define MEMORYSTATUS_CMD_GET_JETSAM_ZPRINT_MEMINFO    34   /* Get jetsam snapshot zprint wired memory info */

/* Commands that act on a group of processes */
#define MEMORYSTATUS_CMD_GRP_SET_PROPERTIES           100

#if PRIVATE
/* Test commands */

/* Trigger forced jetsam */
#define MEMORYSTATUS_CMD_TEST_JETSAM            1000
#define MEMORYSTATUS_CMD_TEST_JETSAM_SORT       1001

/* Select priority band sort order */
#define JETSAM_SORT_NOSORT      0
#define JETSAM_SORT_DEFAULT     1

#endif /* PRIVATE */

/* memorystatus_control() flags */

#define MEMORYSTATUS_FLAGS_SNAPSHOT_ON_DEMAND           0x1     /* A populated snapshot buffer is returned on demand */
#define MEMORYSTATUS_FLAGS_SNAPSHOT_AT_BOOT             0x2     /* Returns a snapshot with memstats collected at boot */
#define MEMORYSTATUS_FLAGS_SNAPSHOT_COPY                0x4     /* No longer supported. Used to return the previously populated snapshot created by the system */
#define MEMORYSTATUS_FLAGS_GRP_SET_PRIORITY             0x8     /* Set jetsam priorities for a group of pids */
#define MEMORYSTATUS_FLAGS_GRP_SET_PROBABILITY          0x10    /* Set probability of use for a group of processes */

#if PRIVATE
#define MEMORYSTATUS_FLAGS_SET_TESTING_PID     0x20 /* Only used by xnu unit tests. */
#define MEMORYSTATUS_FLAGS_UNSET_TESTING_PID   0x40 /* Only used by xnu unit tests. */
#endif /* PRIVATE */

#define MEMORYSTATUS_FLAGS_SNAPSHOT_FREEZER             0x80    /* A snapshot buffer containing app kills since last consumption */

#define MEMORYSTATUS_FLAGS_GRP_SET_FREEZE_PRIORITY      0x100   /* Set a new ordered list of freeze candidates */
#define MEMORYSTATUS_FLAGS_GRP_SET_DEMOTE_PRIORITY      0x200   /* Set a new ordered list of demote candidates */
/*
 * For use with memorystatus_control:
 * MEMORYSTATUS_CMD_GET_JETSAM_SNAPSHOT
 *
 * A jetsam snapshot is initialized when a non-idle
 * jetsam event occurs.  The data is held in the
 * buffer until it is reaped. This is the default
 * behavior.
 *
 * Flags change the default behavior:
 *	Demand mode - this is an on_demand snapshot,
 *	meaning data is populated upon request.
 *
 *	Boot mode - this is a snapshot of
 *	memstats collected before loading the
 *	init program.  Once collected, these
 *	stats do not change.  In this mode,
 *	the snapshot entry_count is always 0.
 *
 *	Copy mode - this returns the previous snapshot
 *      collected by the system. The current snaphshot
 *	might be only half populated.
 *
 * Snapshots are inherently racey between request
 * for buffer size and actual data compilation.
 */

/* These definitions are required for backwards compatibility */
#define MEMORYSTATUS_SNAPSHOT_ON_DEMAND         MEMORYSTATUS_FLAGS_SNAPSHOT_ON_DEMAND
#define MEMORYSTATUS_SNAPSHOT_AT_BOOT           MEMORYSTATUS_FLAGS_SNAPSHOT_AT_BOOT
#define MEMORYSTATUS_SNAPSHOT_COPY              MEMORYSTATUS_FLAGS_SNAPSHOT_COPY

/*
 * For use with memorystatus_control:
 * MEMORYSTATUS_CMD_SET_PRIORITY_PROPERTIES
 */
typedef struct memorystatus_priority_properties {
	int32_t  priority;
	uint64_t user_data;
} memorystatus_priority_properties_t;

/*
 * Inform the kernel that setting the priority property is driven by assertions.
 */
#define MEMORYSTATUS_SET_PRIORITY_ASSERTION     0x1

/*
 * For use with memorystatus_control:
 * MEMORYSTATUS_CMD_SET_MEMLIMIT_PROPERTIES
 * MEMORYSTATUS_CMD_GET_MEMLIMIT_PROPERTIES
 */
typedef struct memorystatus_memlimit_properties {
	int32_t memlimit_active;                /* jetsam memory limit (in MB) when process is active */
	uint32_t memlimit_active_attr;
	int32_t memlimit_inactive;              /* jetsam memory limit (in MB) when process is inactive */
	uint32_t memlimit_inactive_attr;
} memorystatus_memlimit_properties_t;

typedef struct memorystatus_memlimit_properties2 {
	memorystatus_memlimit_properties_t v1;
	uint32_t memlimit_increase;             /* jetsam memory limit increase (in MB) for active and inactive states */
	uint32_t memlimit_increase_bytes;       /* bytes used to determine the jetsam memory limit increase, for active and inactive states */
} memorystatus_memlimit_properties2_t;

#define MEMORYSTATUS_MEMLIMIT_ATTR_FATAL        0x1     /* if set, exceeding the memlimit is fatal */
/*
 * For use with diagnostics memorystatus_control:
 * MEMORYSTATUS_CMD_SET_DIAG_LIMIT
 */
typedef struct memorystatus_diag_memlimit_properties {
	uint64_t memlimit;                       /* max limit for memory usage before an exception is thrown expressed in bytes, but rounded internally to Mbytes */
	bool     threshold_enabled;              /* Current status of the diagnostics threshold, only for get operations, not consider in set threshold */
} memorystatus_diag_memlimit_properties_t;


#ifdef XNU_KERNEL_PRIVATE

/*
 * A process will be killed immediately if it crosses a memory limit marked as fatal.
 * Fatal limit types are the
 *	- default system-wide task limit
 *	- per-task custom memory limit
 *
 * A process with a non-fatal memory limit can exceed that limit, but becomes an early
 * candidate for jetsam when the device is under memory pressure.
 * Non-fatal limit types are the
 *	- high-water-mark limit
 *
 * Processes that opt into dirty tracking are evaluated
 * based on clean vs dirty state.
 *      dirty ==> active
 *      clean ==> inactive
 *
 * Processes that do not opt into dirty tracking are
 * evalulated based on priority level.
 *      Foreground or above ==> active
 *      Below Foreground    ==> inactive
 */

/*
 * p_memstat_state flag holds
 *	- in kernel process state and memlimit state
 */

#define P_MEMSTAT_SUSPENDED            0x00000001 /* Process is suspended and likely in the IDLE band */
#define P_MEMSTAT_FROZEN               0x00000002 /* Process has some state on disk. It should be suspended */
#define P_MEMSTAT_FREEZE_DISABLED      0x00000004 /* Process isn't freeze-eligible and will not be frozen */
#define P_MEMSTAT_ERROR                0x00000008 /* Process couldn't be jetsammed for some reason. Transient state so jetsam can skip it next time it sees it */
#define P_MEMSTAT_LOCKED               0x00000010 /* Process is being actively worked on behind the proc_list_lock */
#define P_MEMSTAT_TERMINATED           0x00000020 /* Process is exiting */
#define P_MEMSTAT_FREEZE_IGNORE        0x00000040 /* Process was evaluated by freezer and will be ignored till the next time it goes active and does something */
#define P_MEMSTAT_PRIORITYUPDATED      0x00000080 /* Process had its jetsam priority updated */
#define P_MEMSTAT_FOREGROUND           0x00000100 /* Process is in the FG jetsam band...unused??? */
#define P_MEMSTAT_REFREEZE_ELIGIBLE    0x00000400 /* Process was once thawed i.e. its state was brought back from disk. It is now refreeze eligible.*/
#define P_MEMSTAT_MANAGED              0x00000800 /* Process is managed by RunningBoard i.e. is either application or extension */
#define P_MEMSTAT_INTERNAL             0x00001000 /* Process is a system-critical-not-be-jetsammed process i.e. launchd */
#define P_MEMSTAT_FATAL_MEMLIMIT                  0x00002000   /* current fatal state of the process's memlimit */
#define P_MEMSTAT_MEMLIMIT_ACTIVE_FATAL           0x00004000   /* if set, exceeding limit is fatal when the process is active   */
#define P_MEMSTAT_MEMLIMIT_INACTIVE_FATAL         0x00008000   /* if set, exceeding limit is fatal when the process is inactive */
#define P_MEMSTAT_USE_ELEVATED_INACTIVE_BAND      0x00010000   /* if set, the process will go into this band & stay there when in the background instead
	                                                        *  of the aging bands and/or the IDLE band. */
#define P_MEMSTAT_PRIORITY_ASSERTION              0x00020000   /* jetsam priority is being driven by an assertion */
#define P_MEMSTAT_FREEZE_CONSIDERED               0x00040000   /* This process has been considered for the freezer. */
#define P_MEMSTAT_SKIP                            0x00080000   /* Process is temporarily ineligible for memory pressure kills. Used only on development & debug kernels to make corpses of buggy processes */
#define P_MEMSTAT_FROZEN_XPC_SERVICE              0x00100000   /* Process is an XPC service. Only used for freezer telemetry. */
#define P_MEMSTAT_FROZEN_FOCAL_THAW               0x00200000 /* Process has been thawed while focal in the current freezer interval. Only used for freezer telemetry. */

/*
 * p_memstat_relaunch_flags holds
 *      - relaunch behavior when jetsammed
 */
#define P_MEMSTAT_RELAUNCH_UNKNOWN      0x0
#define P_MEMSTAT_RELAUNCH_LOW          0x1
#define P_MEMSTAT_RELAUNCH_MED          0x2
#define P_MEMSTAT_RELAUNCH_HIGH         0x4

/*
 * Checking the p_memstat_state almost always requires the proc_list_lock
 * because the jetsam thread could be on the other core changing the state.
 *
 * App -- almost always managed by a system process. Always have dirty tracking OFF. Can include extensions too.
 * System Processes -- not managed by anybody. Always have dirty tracking ON. Can include extensions (here) too.
 */
#define isApp(p)            ((p->p_memstat_state & P_MEMSTAT_MANAGED) || ! (p->p_memstat_dirty & P_DIRTY_TRACK))
#define isSysProc(p)            ( ! (p->p_memstat_state & P_MEMSTAT_MANAGED) || (p->p_memstat_dirty & P_DIRTY_TRACK))

#define MEMSTAT_BUCKET_COUNT (JETSAM_PRIORITY_MAX + 1)

typedef struct memstat_bucket {
	TAILQ_HEAD(, proc) list;
	uint32_t count;
	uint32_t relaunch_high_count;
} memstat_bucket_t;

extern memstat_bucket_t memstat_bucket[MEMSTAT_BUCKET_COUNT];

/*
 * Table that expresses the probability of a process
 * being used in the next hour.
 */
typedef struct memorystatus_internal_probabilities {
	char proc_name[MAXCOMLEN + 1];
	int use_probability;
} memorystatus_internal_probabilities_t;

extern memorystatus_internal_probabilities_t *memorystatus_global_probabilities_table;
extern size_t memorystatus_global_probabilities_size;

extern void memorystatus_post_snapshot(void);

extern void memorystatus_init(void);

extern void memorystatus_init_at_boot_snapshot(void);

extern int memorystatus_add(proc_t p, boolean_t locked);

__options_closed_decl(memstat_priority_options_t, uint8_t, {
	MEMSTAT_PRIORITY_OPTIONS_NONE   = 0x00,
	/* Priority is driven by a RB assertion */
	MEMSTAT_PRIORITY_IS_ASSERTION   = 0x01,
	MEMSTAT_PRIORITY_IS_EFFECTIVE   = 0x02,
	/* Insert at the head of the corresponding band */
	MEMSTAT_PRIORITY_INSERT_HEAD    = 0x04,
	/* Do not consider aging the process (used to send directly to IDLE) */
	MEMSTAT_PRIORITY_NO_AGING = 0x08,
});

extern int memorystatus_set_priority(proc_t p, int priority, uint64_t user_data,
    memstat_priority_options_t options);

__options_decl(memlimit_options_t, uint8_t, {
	MEMLIMIT_OPTIONS_NONE = 0x00,
	MEMLIMIT_ACTIVE_FATAL = 0x01,
	MEMLIMIT_INACTIVE_FATAL = 0x02,
});
extern int memorystatus_set_memlimits(proc_t p, int32_t active_limit, int32_t inactive_limit, memlimit_options_t options);

/* Remove this process from jetsam bands for killing or freezing.
 * The proc_list_lock is held by the caller.
 * @param p: The process to remove.
 * @return: 0 if successful. EAGAIN if the process can't be removed right now (because it's being frozen) or ESRCH.
 */
extern int memorystatus_remove(proc_t p);

int memorystatus_update_inactive_jetsam_priority_band(pid_t pid, uint32_t opflags, int priority, boolean_t effective_now);
int memorystatus_relaunch_flags_update(proc_t p, int relaunch_flags);

extern int memorystatus_dirty_track(proc_t p, uint32_t pcontrol);
extern int memorystatus_dirty_set(proc_t p, boolean_t self, uint32_t pcontrol);
extern int memorystatus_dirty_get(proc_t p, boolean_t locked);
extern int memorystatus_dirty_clear(proc_t p, uint32_t pcontrol);

extern int memorystatus_on_terminate(proc_t p);

extern void memorystatus_on_suspend(proc_t p);
extern void memorystatus_on_resume(proc_t p);
extern void memorystatus_on_inactivity(proc_t p);

extern void memorystatus_on_pageout_scan_end(void);

/* Memorystatus kevent */

void memorystatus_kevent_init(lck_grp_t *grp, lck_attr_t *attr);

int memorystatus_knote_register(struct knote *kn);
void memorystatus_knote_unregister(struct knote *kn);

#if CONFIG_MEMORYSTATUS
void memorystatus_log_exception(const int max_footprint_mb, boolean_t memlimit_is_active, boolean_t memlimit_is_fatal);
void memorystatus_log_diag_threshold_exception(const int diag_threshold_value);
void memorystatus_on_ledger_footprint_exceeded(int warning, boolean_t memlimit_is_active, boolean_t memlimit_is_fatal);
void proc_memstat_skip(proc_t p, boolean_t set);
void memorystatus_proc_flags_unsafe(void * v, boolean_t *is_dirty, boolean_t *is_dirty_tracked, boolean_t *allow_idle_exit);

#if __arm64__
void memorystatus_act_on_legacy_footprint_entitlement(proc_t p, boolean_t footprint_increase);
void memorystatus_act_on_ios13extended_footprint_entitlement(proc_t p);
void memorystatus_act_on_entitled_task_limit(proc_t p);
void memorystatus_act_on_entitled_developer_task_limit(proc_t p);
#endif /* __arm64__ */

#endif /* CONFIG_MEMORYSTATUS */

int memorystatus_get_pressure_status_kdp(void);
int  memorystatus_get_proccnt_upto_priority(int32_t max_bucket_index);

#if CONFIG_JETSAM

typedef enum memorystatus_policy {
	kPolicyDefault        = 0x0,
	kPolicyMoreFree       = 0x1,
} memorystatus_policy_t;
extern unsigned int memorystatus_swap_all_apps;

/*
 * Synchronous memorystatus kill calls.
 */

boolean_t memorystatus_kill_on_VM_page_shortage(void);
boolean_t memorystatus_kill_on_vnode_limit(void);
boolean_t memorystatus_kill_on_sustained_pressure(void);

/*
 * Wake up the memorystatus thread so it can do async kills.
 * The memorystatus thread will keep killing until the system is
 * considered healthy.
 */
void memorystatus_thread_wake(void);

/*
 * Attempt to kill the specified pid with the given reason.
 * Consumes a reference on the jetsam_reason.
 */
boolean_t memorystatus_kill_with_jetsam_reason_sync(pid_t pid, os_reason_t jetsam_reason);

void jetsam_on_ledger_cpulimit_exceeded(void);
void memorystatus_fast_jetsam_override(boolean_t enable_override);
/*
 * Disable memorystatus_swap_all_apps.
 * Used by vm_pageout at boot if the swap volume is too small to support app swap.
 * Returns true iff app swap is now disabled.
 * On development or debug kernels, app swap can be enabled via a boot-arg and in
 * that case can not be disabled.
 */
bool memorystatus_disable_swap(void);

#endif /* CONFIG_JETSAM */

boolean_t memorystatus_kill_on_zone_map_exhaustion(pid_t pid);
boolean_t memorystatus_kill_on_VM_compressor_space_shortage(boolean_t async);
void memorystatus_kill_on_vps_starvation(void);
void memorystatus_pages_update(unsigned int pages_avail);
boolean_t memorystatus_idle_exit_from_VM(void);
proc_t memorystatus_get_first_proc_locked(unsigned int *bucket_index, boolean_t search);
proc_t memorystatus_get_next_proc_locked(unsigned int *bucket_index, proc_t p, boolean_t search);
void memorystatus_get_task_page_counts(task_t task, uint32_t *footprint, uint32_t *max_footprint_lifetime, uint32_t *purgeable_pages);

bool memorystatus_task_has_increased_memory_limit_entitlement(task_t task);
bool memorystatus_task_has_increased_debugging_memory_limit_entitlement(task_t task);
bool memorystatus_task_has_legacy_footprint_entitlement(task_t task);
bool memorystatus_task_has_ios13extended_footprint_limit(task_t task);

#if VM_PRESSURE_EVENTS

extern kern_return_t memorystatus_update_vm_pressure(boolean_t);

#if CONFIG_MEMORYSTATUS
/* Flags */
extern int memorystatus_low_mem_privileged_listener(uint32_t op_flags);
extern int memorystatus_send_pressure_note(int pid);
extern boolean_t memorystatus_is_foreground_locked(proc_t p);
extern boolean_t memorystatus_bg_pressure_eligible(proc_t p);
#endif /* CONFIG_MEMORYSTATUS */

#endif /* VM_PRESSURE_EVENTS */

#endif /* XNU_KERNEL_PRIVATE */

#endif /* SYS_MEMORYSTATUS_H */
