/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#ifndef _PORTABLE_TASK_CRASH_INFO_H
#define _PORTABLE_TASK_CRASH_INFO_H

#include <stdint.h>

// A portable copy of vm_object_query_data_t.
struct __attribute__((__packed__)) portable_vm_object_query_data_t {
    uint64_t object_id;
    uint64_t virtual_size;
    uint64_t resident_size;
    uint64_t wired_size;
    uint64_t reusable_size;
    uint64_t compressed_size;
    struct {
        uint64_t vo_no_footprint : 1; /* object not included in footprint */
        uint64_t vo_ledger_tag   : 3; /* object ledger tag */
        uint64_t purgable        : 2; /* object "purgable" state #defines */
    };
};

#define PORTABLE_TASK_CRASH_INFO_CURRENT_MAJOR_VERSION 2
#define PORTABLE_TASK_CRASH_INFO_CURRENT_MINOR_VERSION 0

// This struct mostly contains the TASK_CRASHINFO_* data the kernel saves for corpses and makes
// available to userspace via task_map_corpse_info_64().
// It also contains a few additional entries marked with `(new)` which aren't included that data.
//
// The kernel gathers the TASK_CRASHINFO_* information in gather_populate_corpse_crashinfo() in
// kern_exit.c.
// I've attempted to give notes about how to fetch these values in userspace for live processes
// using existing APIs, but I wasn't able to find existing methods for all of these.  I may have
// missed some.
//
// It should be possible to read most of these in userspace directly from the TASK_CRASHINFO_*
// data for corpses.
//
// TASK_CRASHINFO_* entries not included in this struct:
// * TASK_CRASHINFO_UUID
//   * Duplicate info - UUID is included in TASK_CRASHINFO_RUSAGE_INFO.
// * TASK_CRASHINFO_CPUTYPE
//   * Duplicate info - target arch is included in core header.
// * TASK_CRASHINFO_EXTMODINFO
//   * Kernel no longer includes in corpse blobs.
// * TASK_CRASHINFO_PROC_STATUS
//   * Kernel no longer includes in corpse blobs.
// * TASK_CRASHINFO_BSDINFOWITHUNIQID
//   * Still included by kernel in corpse blobs, but only includes duplicate uuid or irrelevant
//     unique process IDs.
// * TASK_CRASHINFO_RUSAGE
//   * Kernel deprecated this in favor of TASK_CRASHINFO_RUSAGE_INFO.
// * TASK_CRASHINFO_KERNEL_TRIAGE_INFO_V1
//   * No obvious use.
struct __attribute__((__packed__)) portable_task_crash_info_t {
    uint32_t major_version; // Currently 2.
    uint32_t minor_version; // Currently 0.

    // Analogous to TASK_CRASHINFO_TASK_IS_CORPSE_FORK.
    // 0 for live task, 1 for corpse, 2, for corpse fork.
    // Fetch using task_map_corpse_info_64().
    //  * If the call fails, it's a live task.
    //  * If the call succeeds, set based on value of TASK_CRASHINFO_TASK_IS_CORPSE_FORK.
    uint32_t task_type;

    // Fetch using proc_pidinfo(..., PROC_PIDTBSDINFO, ...) like Symbolication.framework does in
    // get_process_description().
    int32_t  pid;                // TASK_CRASHINFO_PID            - Target pid.
    int32_t  ppid;               // TASK_CRASHINFO_PPID           - Target ppid.
    uint32_t uid;                // TASK_CRASHINFO_UID            - Target UID (user ID).
    uint32_t gid;                // TASK_CRASHINFO_GID            - Target GID (process and user IDs).
    uint64_t proc_starttime_sec; // TASK_CRASHINFO_PROC_STARTTIME - A struct timeval64.
    uint64_t proc_starttime_usec;

    // Offsets into core file of nul-terminated char arrays, or UINT64_MAX if not set.
    //
    // Fetch using `proc_name()` and `proc_pidpath()`.
    uint64_t proc_name_offset;        // TASK_CRASHINFO_PROC_NAME - Target executable name.
    uint64_t proc_path_offset;        // TASK_CRASHINFO_PROC_PATH - Target executable path.
    uint64_t parent_proc_name_offset; // (new) - Target's parent's executable name.
    uint64_t parent_proc_path_offset; // (new) - Target's parent's executable path.

    // TASK_CRASHINFO_RESPONSIBLE_PID.
    //
    // Fetch using responsibility_get_responsible_for_pid().
    int32_t  responsible_pid;

    // TASK_CRASHINFO_PROC_PERSONA_ID.
    // Unsure how to fetch from userspace. Maybe we can somehow get proc_persona_info.persona_id?
    uint32_t proc_persona_id; // kpersona_pidinfo()

    // Fetch from `int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid }` like CoreSymbolication does in CSTaskIsTranslated().
    uint64_t userstack;  // TASK_CRASHINFO_USERSTACK.  extern_proc.user_stack.
    uint32_t proc_flags; // TASK_CRASHINFO_PROC_FLAGS. extern_proc.p_flag.

    // Fetch using KERN_PROCARGS2 like Symbolication does in copy_remote_env_vars_with_argbuf()
    int32_t  argslen;   // TASK_CRASHINFO_ARGSLEN.   The returned-via-reference argbufSize.
    int32_t  proc_argc; // TASK_CRASHINFO_PROC_ARGC. The first 4 bytes of the mapped args.

    // Fetch using proc_get_dirty().
    int32_t  dirty_flags; // TASK_CRASHINFO_DIRTY_FLAGS.

    // Fetch using csops_audittoken() like Symbolication does in _is_process_debuggable().
    uint32_t proc_csflags; // TASK_CRASHINFO_PROC_CSFLAGS.

    // TASK_CRASHINFO_EXCEPTION_CODES - Exception code and subcode from
    // <mach/exception_types.h>.
    // Both 0 if no exception occurred.
    // Unsure how to fetch from userspace.
    uint64_t exception_code_code;
    uint64_t exception_code_subcode;

    // Unsure how to fetch from userspace.
    uint64_t crashed_threadid;         // TASK_CRASHINFO_CRASHED_THREADID
    int32_t  crashinfo_exception_type; // TASK_CRASHINFO_EXCEPTION_TYPE
                                       // UINT32_MAX if no exception.

    // TASK_CRASHINFO_COALITION_ID
    //
    // Fetch using proc_pidinfo(..., PROC_PIDCOALITIONINFO, ...) like DVTInstrumentsFrameworks does
    // in -[IDEDataProvider_procinfo captureAttributes:toDictionary:forPID:].
    uint64_t coalition_resource_id;
    uint64_t coalition_jetsam_id;

    // TASK_CRASHINFO_TASKDYLD_INFO
    // A struct task_dyld_info_data_t, minus flavor (which is implicit in core file arch). Allows
    // us to instantly find "all image infos" in the core.
    // The kernel no longer includes this in corpse blobs as TASK_CRASHINFO_TASKDYLD_INFO, but
    // CoreSymbolication (and potentially lldb) will use it.
    //
    // Fetch using task_info(..., TASK_DYLD_INFO, ...) like CoreSymbolication.framework does in
    // CSCppTaskMemory::CSCppTaskMemory().
    //
    // Values should be UINT64_MAX if unavailable (dyld plans to remove "all image infos" in the
    // future).
    uint64_t dyld_all_image_infos_addr;
    uint64_t dyld_all_image_infos_size;
    
    // The base and total size of the dyld shared cache as reported by
    // dyld_shared_cache_get_base_address() and dyld_shared_cache_get_mapped_size().
    //
    // Values should be UINT64_MAX if unavailable.
    uint64_t dyld_shared_cache_base_addr;
    uint64_t dyld_shared_cache_size;

    // TASK_CRASHINFO_UDATA_PTRS - Array of pointers from kernel into userspace.
    // udata_ptrs_count is 0 if not set.
    //
    // Fetch using `extern int proc_list_uptrs(pid_t pid, uint64_t *buffer, uint32_t buffersize)`
    // like Symbolication does in
    // -[VMUTaskMemoryScanner _orderedScanWithScanner:recorder:keepMapped:actions:].
    uint64_t udata_ptrs_offset;
    uint64_t udata_ptrs_count;

    // Array of portable_vm_object_query_data_t structs.
    // Fetch using sysctlbyname("vm.get_owned_vmobjects", ...).
    uint64_t vm_object_query_datas_offset;
    uint64_t vm_object_query_datas_count;

    // how to fetch from userspace.
    uint64_t memory_limit;                    // TASK_CRASHINFO_MEMORY_LIMIT get_task_phys_footprint_limit()
    uint32_t memory_limit_increase;           // TASK_CRASHINFO_MEMORY_LIMIT_INCREASE memorystatus_cmd_get_memlimit_properties() via memorystatus_control()
    int32_t  memorystatus_effective_priority; // TASK_CRASHINFO_MEMORYSTATUS_EFFECTIVE_PRIORITY memorystatus_get_priority_pid()

    // TASK_CRASHINFO_WORKQUEUEINFO
    // A struct proc_workqueueinfo.
    //
    // Fetch using `proc_pidinfo(..., PROC_PIDWORKQUEUEINFO, ...).
    uint32_t pwq_nthreads;       // Total number of workqueue threads.
    uint32_t pwq_runthreads;     // Total number of running workqueue threads.
    uint32_t pwq_blockedthreads; // Total number of blocked workqueue threads.
    uint32_t pwq_state;

    // (new)
    // A mach_timebase_info_data_t.
    // Allows converting between mach time units and epoch time for `*_abstime` values below.
    //
    // Fetch using mach_timebase_info().
    uint32_t mach_timebase_info_numer;
    uint32_t mach_timebase_info_denom;

    // TASK_CRASHINFO_RUSAGE_INFO
    // A struct rusage_info_v3, minus phys_footprint which is also present in ledger info below.
    //
    // Fetch using proc_pid_rusage().
    uint8_t  ri_uuid[16];
    uint64_t ri_user_time;
    uint64_t ri_system_time;
    uint64_t ri_pkg_idle_wkups;
    uint64_t ri_interrupt_wkups;
    uint64_t ri_pageins;
    uint64_t ri_wired_size;
    uint64_t ri_resident_size;
    uint64_t ri_proc_start_abstime;
    uint64_t ri_proc_exit_abstime;
    uint64_t ri_child_user_time;
    uint64_t ri_child_system_time;
    uint64_t ri_child_pkg_idle_wkups;
    uint64_t ri_child_interrupt_wkups;
    uint64_t ri_child_pageins;
    uint64_t ri_child_elapsed_abstime;
    uint64_t ri_diskio_bytesread;
    uint64_t ri_diskio_byteswritten;
    uint64_t ri_cpu_time_qos_default;
    uint64_t ri_cpu_time_qos_maintenance;
    uint64_t ri_cpu_time_qos_background;
    uint64_t ri_cpu_time_qos_utility;
    uint64_t ri_cpu_time_qos_legacy;
    uint64_t ri_cpu_time_qos_user_initiated;
    uint64_t ri_cpu_time_qos_user_interactive;
    uint64_t ri_billed_system_time;
    uint64_t ri_serviced_system_time;

    // TASK_CRASHINFO_LEDGER_*
    // Ledger data.
    //
    // Fetch using `extern int ledger(int cmd, caddr_t arg1, caddr_t arg2, caddr_t arg3)`, like
    // footprint does in FPLedgerIndexForLedger() and -[FPUserProcess _gatherLedgers].
    uint64_t ledger_internal;                        // TASK_CRASHINFO_LEDGER_INTERNAL
    uint64_t ledger_internal_compressed;             // TASK_CRASHINFO_LEDGER_INTERNAL_COMPRESSED
    uint64_t ledger_iokit_mapped;                    // TASK_CRASHINFO_LEDGER_IOKIT_MAPPED
    uint64_t ledger_alternate_accounting;            // TASK_CRASHINFO_LEDGER_ALTERNATE_ACCOUNTING
    uint64_t ledger_alternate_compressed;            // TASK_CRASHINFO_LEDGER_ALTERNATE_ACCOUNTING_COMPRESSED
    uint64_t ledger_purgable_nonvolatile;            // TASK_CRASHINFO_LEDGER_PURGEABLE_NONVOLATILE
    uint64_t ledger_purgable_nonvolatile_compressed; // TASK_CRASHINFO_LEDGER_PURGEABLE_NONVOLATILE_COMPRESSED
    uint64_t ledger_page_table;                      // TASK_CRASHINFO_LEDGER_PAGE_TABLE
    uint64_t ledger_phys_footprint;                  // TASK_CRASHINFO_LEDGER_PHYS_FOOTPRINT
    uint64_t ledger_phys_footprint_lifetime_max;     // TASK_CRASHINFO_LEDGER_PHYS_FOOTPRINT_LIFETIME_MAX
    uint64_t ledger_network_nonvolatile;             // TASK_CRASHINFO_LEDGER_NETWORK_NONVOLATILE
    uint64_t ledger_network_nonvolatile_compressed;  // TASK_CRASHINFO_LEDGER_NETWORK_NONVOLATILE_COMPRESSED
    uint64_t ledger_wired_mem;                       // TASK_CRASHINFO_LEDGER_WIRED_MEM
    uint64_t ledger_tagged_footprint;                // TASK_CRASHINFO_LEDGER_TAGGED_FOOTPRINT
    uint64_t ledger_tagged_footprint_compressed;     // TASK_CRASHINFO_LEDGER_TAGGED_FOOTPRINT_COMPRESSED
    uint64_t ledger_media_footprint;                 // TASK_CRASHINFO_LEDGER_MEDIA_FOOTPRINT
    uint64_t ledger_media_footprint_compressed;      // TASK_CRASHINFO_LEDGER_MEDIA_FOOTPRINT_COMPRESSED
    uint64_t ledger_graphics_footprint;              // TASK_CRASHINFO_LEDGER_GRAPHICS_FOOTPRINT
    uint64_t ledger_graphics_footprint_compressed;   // TASK_CRASHINFO_LEDGER_GRAPHICS_FOOTPRINT_COMPRESSED
    uint64_t ledger_neural_footprint;                // TASK_CRASHINFO_LEDGER_NEURAL_FOOTPRINT
    uint64_t ledger_neural_footprint_compressed;     // TASK_CRASHINFO_LEDGER_NEURAL_FOOTPRINT_COMPRESSED
};

#endif /* _PORTABLE_TASK_CRASH_INFO_H */
