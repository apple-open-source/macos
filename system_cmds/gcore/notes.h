/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#ifndef _NOTES_H
#define _NOTES_H

#include <stdint.h>
#include <stdlib.h>

typedef enum {
    kTASK_CRASHINFO_TASK_TYPE_LIVE        = 0,
    kTASK_CRASHINFO_TASK_TYPE_CORPSE      = 1,
    kTASK_CRASHINFO_TASK_TYPE_CORPSE_FORK = 2
} task_crashinfo_task_type_t;

struct task_crashinfo_note_data {
    task_crashinfo_task_type_t task_type;

    int32_t  pid;
    int32_t  ppid;
    uint32_t uid;
    uint32_t gid;
    uint64_t proc_starttime_sec;
    uint64_t proc_starttime_usec;

    const char *proc_name;
    const char *proc_path;
    const char *parent_proc_name;
    const char *parent_proc_path;

    int32_t  responsible_pid;

    uint32_t proc_persona_id;

    uint64_t userstack;
    uint32_t proc_flags;

    int32_t  argslen;
    int32_t  proc_argc;

    int32_t  dirty_flags;

    uint32_t proc_csflags;

    uint64_t exception_code_code;
    uint64_t exception_code_subcode;

    uint64_t crashed_threadid;
    int32_t  crashinfo_exception_type;
                                       

    uint64_t coalition_resource_id;
    uint64_t coalition_jetsam_id;

    uint64_t dyld_all_image_infos_addr;
    uint64_t dyld_all_image_infos_size;
    
    uint64_t dyld_shared_cache_base_addr;
    uint64_t dyld_shared_cache_size;

    uint64_t *udata_ptrs;
    uint64_t udata_ptrs_count;

    struct portable_vm_object_query_data_t *vm_object_query_datas;
    uint64_t vm_object_query_datas_count;

    uint64_t memory_limit;
    uint32_t memory_limit_increase;
    int32_t  memorystatus_effective_priority;

    uint32_t pwq_nthreads;
    uint32_t pwq_runthreads;
    uint32_t pwq_blockedthreads;
    uint32_t pwq_state;

    uint32_t mach_timebase_info_numer;
    uint32_t mach_timebase_info_denom;

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

    uint64_t ledger_internal;
    uint64_t ledger_internal_compressed;
    uint64_t ledger_iokit_mapped;
    uint64_t ledger_alternate_accounting;
    uint64_t ledger_alternate_compressed;
    uint64_t ledger_purgable_nonvolatile;
    uint64_t ledger_purgable_nonvolatile_compressed;
    uint64_t ledger_page_table;
    uint64_t ledger_phys_footprint;
    uint64_t ledger_phys_footprint_lifetime_max;
    uint64_t ledger_network_nonvolatile;
    uint64_t ledger_network_nonvolatile_compressed;
    uint64_t ledger_wired_mem;
    uint64_t ledger_tagged_footprint;
    uint64_t ledger_tagged_footprint_compressed;
    uint64_t ledger_media_footprint;
    uint64_t ledger_media_footprint_compressed;
    uint64_t ledger_graphics_footprint;
    uint64_t ledger_graphics_footprint_compressed;
    uint64_t ledger_neural_footprint;
    uint64_t ledger_neural_footprint_compressed;
};

struct region_infos_note_region_data {
    uint64_t vmaddr;
    uint64_t vmsize;

    uint32_t nesting_depth;

    int32_t  protection;
    int32_t  max_protection;
    uint32_t inheritance;
    uint64_t offset;
    uint32_t user_tag;
    uint32_t pages_resident;
    uint32_t pages_shared_now_private;
    uint32_t pages_swapped_out;
    uint32_t pages_dirtied;
    uint32_t ref_count;
    uint16_t shadow_depth;
    uint8_t  external_pager;
    uint8_t  share_mode;
    uint8_t  is_submap;
    int32_t  behavior;
    uint32_t object_id;
    uint16_t user_wired_count;
    uint32_t pages_reusable;
    uint64_t object_id_full;

    int32_t purgeability;

    const char *mapped_file_path;

    uint64_t phys_footprint_disposition_count;
    const uint16_t *phys_footprint_dispositions;

    uint64_t non_phys_footprint_disposition_count;
    const uint16_t *non_phys_footprint_dispositions;
};

struct region_infos_note_data {
    uint64_t regions_count;
    struct region_infos_note_region_data regions[0];
};

struct task_crashinfo_note_data *prepare_task_crashinfo_note(task_t task);
struct region_infos_note_data *prepare_region_infos_note(task_t task);

void destroy_task_crash_info_note_data(struct task_crashinfo_note_data *);
void destroy_region_infos_note_data(struct region_infos_note_data *);

#endif /* _NOTES_H */
