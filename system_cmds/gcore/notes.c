/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "corefile.h"
#include "utils.h"
#include "vm.h"
#include "notes.h"
#include "portable_task_crash_info_t.h"
#include "portable_region_infos_t.h"

#include <errno.h>
#include <libproc.h>
#include <Kernel/kern/ledger.h>
#include <kern/kcdata.h>
#include <System/sys/persona.h>
#include <sys/sysctl.h>
#include <sys/codesign.h>
#include <sys/kern_sysctl.h>
#include <mach/mach_time.h>
#include <mach-o/dyld_introspection.h>

#if TARGET_OS_OSX
#include <responsibility.h>
#endif

#pragma mark -- Fetching data for notes --

extern int ledger(int cmd, caddr_t arg1, caddr_t arg2, caddr_t arg3);

static int
get_ledgers(int pid, struct task_crashinfo_note_data *tci)
{
    /* we use ledger(LEDGER_ENTRY_INFO) for everything.
     * this will basically get us a big array of `struct ledger_entry_info_t`s.
     * we need to get the index from a different call
     * ledger(LEDGER_TEMPLATE_INFO) which gives us a bunch of
     * ledger_template_info, then we take those and compare the names to find
     * the ledger entry we care about, and then get the proper ledger entry
     * based on name/index. then we can calculate the balance.
     */

    /* grab ledger entries */
    struct ledger_info li = {0};
    {
        const int ledger_err = ledger(LEDGER_INFO, (caddr_t)(intptr_t)pid, (caddr_t)&li, NULL);
        if (ledger_err < 0) {
            return ledger_err;
        }
    }
        
    const int64_t template_cnt = li.li_entries;
    
    struct ledger_template_info *templateInfo = malloc((size_t)template_cnt * sizeof(struct ledger_template_info));
    struct ledger_entry_info *lei = malloc((size_t)template_cnt * sizeof(*lei));
    if (templateInfo == NULL || lei == NULL) {
        free(templateInfo);
        free(lei);
        return -1;
    }

#define IF_LEDGER_CASE(name) \
if (strncmp(templateInfo[i].lti_name, #name, strlen(#name)) == 0) { \
    tci->ledger_##name = lei[i].lei_balance; \
}
            
#define ELIF_LEDGER_CASE(name)  else IF_LEDGER_CASE(name)
    
    uint64_t count = template_cnt;
    if (ledger(LEDGER_TEMPLATE_INFO, (caddr_t)templateInfo, (caddr_t)&template_cnt, NULL) >= 0
        && ledger(LEDGER_ENTRY_INFO, (caddr_t)(intptr_t)pid, (caddr_t)lei, (caddr_t)&count) >= 0) {
        
        for (int i = 0; i < template_cnt; i++) {
              IF_LEDGER_CASE(internal)
            ELIF_LEDGER_CASE(internal_compressed)
            ELIF_LEDGER_CASE(iokit_mapped)
            ELIF_LEDGER_CASE(alternate_accounting)
            ELIF_LEDGER_CASE(alternate_compressed)
            ELIF_LEDGER_CASE(purgable_nonvolatile)
            ELIF_LEDGER_CASE(purgable_nonvolatile_compressed)
            ELIF_LEDGER_CASE(page_table)
            ELIF_LEDGER_CASE(phys_footprint)
            ELIF_LEDGER_CASE(phys_footprint_lifetime_max)
            ELIF_LEDGER_CASE(network_nonvolatile)
            ELIF_LEDGER_CASE(network_nonvolatile_compressed)
            ELIF_LEDGER_CASE(wired_mem)
            ELIF_LEDGER_CASE(tagged_footprint)
            ELIF_LEDGER_CASE(tagged_footprint_compressed)
            ELIF_LEDGER_CASE(media_footprint)
            ELIF_LEDGER_CASE(media_footprint_compressed)
            ELIF_LEDGER_CASE(graphics_footprint)
            ELIF_LEDGER_CASE(graphics_footprint_compressed)
            ELIF_LEDGER_CASE(neural_footprint)
            ELIF_LEDGER_CASE(neural_footprint_compressed)
        }
    } else {
        free(templateInfo);
        free(lei);
        return -1;
    }
    
    free(templateInfo);
    free(lei);
    return 0;
}

static void
tci_copy_ti(struct task_crashinfo_note_data *tci, const task_vm_info_data_t *ti)
{
    tci->ledger_internal                        = ti->internal;
    tci->ledger_purgable_nonvolatile            = ti->ledger_purgeable_nonvolatile;
    tci->ledger_purgable_nonvolatile_compressed = ti->ledger_purgeable_novolatile_compressed;
    tci->ledger_phys_footprint                  = ti->phys_footprint;
    tci->ledger_phys_footprint_lifetime_max     = ti->ledger_phys_footprint_peak;
    tci->ledger_network_nonvolatile             = ti->ledger_tag_network_nonvolatile;
    tci->ledger_network_nonvolatile_compressed  = ti->ledger_tag_network_nonvolatile_compressed;
    tci->ledger_media_footprint                 = ti->ledger_tag_media_footprint;
    tci->ledger_media_footprint_compressed      = ti->ledger_tag_media_footprint_compressed;
    tci->ledger_graphics_footprint              = ti->ledger_tag_graphics_footprint;
    tci->ledger_graphics_footprint_compressed   = ti->ledger_tag_graphics_footprint_compressed;
    tci->ledger_neural_footprint                = ti->ledger_tag_neural_footprint;
    tci->ledger_neural_footprint_compressed     = ti->ledger_tag_neural_footprint_compressed;
}

static void
copy_pwqinfo(struct task_crashinfo_note_data *tci, const struct proc_workqueueinfo *pwqinfo)
{
    tci->pwq_nthreads       = pwqinfo->pwq_nthreads;
    tci->pwq_runthreads     = pwqinfo->pwq_runthreads;
    tci->pwq_blockedthreads = pwqinfo->pwq_blockedthreads;
    tci->pwq_state          = pwqinfo->pwq_state;
}

static void
copy_rusage_info(struct task_crashinfo_note_data *tci, const struct rusage_info_v3 *rui)
{
    memcpy(tci->ri_uuid, rui->ri_uuid, sizeof tci->ri_uuid);
    
    tci->ri_user_time                     = rui->ri_user_time;
    tci->ri_system_time                   = rui->ri_system_time;
    tci->ri_pkg_idle_wkups                = rui->ri_pkg_idle_wkups;
    tci->ri_interrupt_wkups               = rui->ri_interrupt_wkups;
    tci->ri_pageins                       = rui->ri_pageins;
    tci->ri_wired_size                    = rui->ri_wired_size;
    tci->ri_resident_size                 = rui->ri_resident_size;
    tci->ri_proc_start_abstime            = rui->ri_proc_start_abstime;
    tci->ri_proc_exit_abstime             = rui->ri_proc_exit_abstime;
    tci->ri_child_user_time               = rui->ri_child_user_time;
    tci->ri_child_system_time             = rui->ri_child_system_time;
    tci->ri_child_pkg_idle_wkups          = rui->ri_child_pkg_idle_wkups;
    tci->ri_child_interrupt_wkups         = rui->ri_child_interrupt_wkups;
    tci->ri_child_pageins                 = rui->ri_child_pageins;
    tci->ri_child_elapsed_abstime         = rui->ri_child_elapsed_abstime;
    tci->ri_diskio_bytesread              = rui->ri_diskio_bytesread;
    tci->ri_diskio_byteswritten           = rui->ri_diskio_byteswritten;
    tci->ri_cpu_time_qos_default          = rui->ri_cpu_time_qos_default;
    tci->ri_cpu_time_qos_maintenance      = rui->ri_cpu_time_qos_maintenance;
    tci->ri_cpu_time_qos_background       = rui->ri_cpu_time_qos_background;
    tci->ri_cpu_time_qos_utility          = rui->ri_cpu_time_qos_utility;
    tci->ri_cpu_time_qos_legacy           = rui->ri_cpu_time_qos_legacy;
    tci->ri_cpu_time_qos_user_initiated   = rui->ri_cpu_time_qos_user_initiated;
    tci->ri_cpu_time_qos_user_interactive = rui->ri_cpu_time_qos_user_interactive;
    tci->ri_billed_system_time            = rui->ri_billed_system_time;
    tci->ri_serviced_system_time          = rui->ri_serviced_system_time;
}

typedef struct portable_iosurface_debug_description_pid_properties_list_t
    portable_iosurface_info;
extern int proc_list_uptrs(pid_t pid, uint64_t *buffer, uint32_t buffersize);

static const char *
strdup_nul_terminated_string(const char *string_buffer, int buffer_size, int string_length)
{
    if (0 < string_length && string_length < buffer_size) {
        return strndup(string_buffer, string_length);
    } else {
        return NULL;
    }
}

static const char *
strdup_nul_terminated_proc_name(pid_t pid)
{
    char name_buf[2 * MAXCOMLEN + 1] = "";
    const int name_length = proc_name(pid, name_buf, sizeof(name_buf));
    return strdup_nul_terminated_string(name_buf, sizeof(name_buf), name_length);
}

static const char *
strdup_nul_terminated_proc_path(pid_t pid)
{
    char path_buf[MAXPATHLEN + 1] = "";
    const int path_length = proc_pidpath(pid, path_buf, sizeof(path_buf));
    return strdup_nul_terminated_string(path_buf, sizeof(path_buf), path_length);
}

static void
populate_task_crash_info_for_live_task(struct task_crashinfo_note_data *tci, task_t task)
{
    tci->task_type = kTASK_CRASHINFO_TASK_TYPE_LIVE;

    pid_t pid = -1;
    {
        const kern_return_t pid_for_task_err = pid_for_task(task, &pid);
        if (pid_for_task_err != KERN_SUCCESS) {
            errx(EX_OSERR, "failed to get pid for task with error %s", mach_error_string(pid_for_task_err));
        }
    }
    
    /* proc_pidinfo and proc names/paths */
    {
        tci->proc_name = strdup_nul_terminated_proc_name(pid);
        tci->proc_path = strdup_nul_terminated_proc_path(pid);
        
        struct proc_bsdinfo bsdInfo = {0};
        const int proc_pidinfo_err = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsdInfo, sizeof(bsdInfo));
        
        if (PROC_PIDTBSDINFO_SIZE != proc_pidinfo_err) {
            errx(EX_OSERR, "proc_pidinfo failed");
        }
        
        assert((pid_t)bsdInfo.pbi_pid == pid);
        
        tci->pid                 = bsdInfo.pbi_pid;
        tci->ppid                = bsdInfo.pbi_ppid;
        tci->uid                 = bsdInfo.pbi_uid;
        tci->gid                 = bsdInfo.pbi_gid;
        tci->proc_starttime_sec  = bsdInfo.pbi_start_tvsec;
        tci->proc_starttime_usec = bsdInfo.pbi_start_tvusec;
        
        tci->parent_proc_name = strdup_nul_terminated_proc_name(bsdInfo.pbi_ppid);
        tci->parent_proc_path = strdup_nul_terminated_proc_path(bsdInfo.pbi_ppid);
    }
    
    /* responsible_pid */
    {
        pid_t responsible_pid = -1;
        
        /* responsibility only exists on Mac */
#if TARGET_OS_OSX
        const kern_return_t get_responsible_pid_err = responsibility_get_responsible_for_pid(pid, &responsible_pid, NULL, NULL, NULL);
        if (KERN_SUCCESS != get_responsible_pid_err) {
            errx(EX_OSERR, "failed to get reponsible pid with error %s", mach_error_string(get_responsible_pid_err));
        }
#endif
        
        tci->responsible_pid = responsible_pid;
    }
    
    /* proc_persona_id */
    {
        /*
         * struct kpersona_info kpi = {
         *     .persona_info_version = PERSONA_INFO_V1,
         * };
         *
         * const int kpersona_pidinfo_err = kpersona_pidinfo(pid, &kpi);
         * if (kpersona_pidinfo_err == 0) {
         *     tci->proc_persona_id = kpi.persona_id;
         * } else if (kpersona_pidinfo_err == ESRCH) {
         *     tci->proc_persona_id = PERSONA_ID_NONE;
         * } else {
         *     errx(EX_OSERR, "kpersona_pidinfo failed with the error %s", strerror(errno));
         * }
         */
        
        /* kpersona_pidinfo() always fails because we change_credentials().  for now, don't fill
         * out the persona id for suspended targets - nothing uses it yet anyway.
         */
        tci->proc_persona_id = PERSONA_ID_NONE;
    }
    
    /* userstack and proc_flags */
    {
        int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
        struct kinfo_proc processInfo;
        size_t bufsize = sizeof(processInfo);

        const int kern_proc_pid_err = sysctl(mib, (unsigned)(sizeof(mib) / sizeof(int)), &processInfo,
                                             &bufsize, NULL, 0);
        if (kern_proc_pid_err < 0 && bufsize <= 0) {
            errx(EX_OSERR, "KERN_PROC_PID failed");
        }
        
        tci->userstack = (uint64_t)processInfo.kp_proc.user_stack;
        tci->proc_flags = processInfo.kp_proc.p_flag;
    }
    
    /* KERN_PROCARGS2 */
    {
        /* first get the maximum arguments size, to determine the necessary
         * buffer size
         */
        int argmax_mib[] = { CTL_KERN, KERN_ARGMAX };
        size_t argmax_buf_size = 0;
        size_t size = sizeof(argmax_buf_size);
        const int kern_argmax_err = sysctl(argmax_mib, 2, &argmax_buf_size, &size, NULL, 0);
        
        if (kern_argmax_err != 0) {
            errx(EX_OSERR, "KERN_ARGMAX failed");
        }
        
        tci->argslen = (int32_t)argmax_buf_size;

        void *argbuf = malloc(argmax_buf_size);
        if (NULL == argbuf) {
            errx(EX_OSERR, "failed to allocate argument buffer");
        }

        /* the older KERN_PROCARGS is deprecated */
        int procargs2_mib[] = { CTL_KERN, KERN_PROCARGS2, pid };
        const int kern_procargs2_err = sysctl(procargs2_mib, 3, argbuf, &argmax_buf_size, NULL, 0);
                
        if (kern_procargs2_err != 0) {
            errx(EX_OSERR, "KERN_PROCARGS2 failed");
        }
        
        /* argc is the first int32_t of the arguments buffer */
        tci->proc_argc = *(int32_t *)argbuf;
                
        free(argbuf);
    }
    
    /* dirty flags */
    {
        uint32_t dirty_flags = 0;
        const int proc_get_diry_err = proc_get_dirty(pid, &dirty_flags);
        if (proc_get_diry_err < 0) {
            errx(EX_OSERR, "proc_get_dirty failed");
        }
        
        tci->dirty_flags = *(int32_t *)&dirty_flags;
    }
    
    /* csflags */
    {
        const int csops_err = csops(pid, CS_OPS_STATUS, &tci->proc_csflags, sizeof(tci->proc_csflags));
        if (csops_err < 0) {
            errx(EX_OSERR, "csops failed");
        }
    }
    
    /* exception codes */
    {
        /* only corpses have non-zero exception codes */
        tci->exception_code_code = 0;
        tci->exception_code_subcode = 0;
    }
    
    /* coalitions */
    {
        struct proc_pidcoalitioninfo pci = {0};
        const int proc_pidinfo_size = proc_pidinfo(pid, PROC_PIDCOALITIONINFO, 1, &pci, sizeof(pci));
        if (proc_pidinfo_size != PROC_PIDCOALITIONINFO_SIZE) {
            errx(EX_OSERR, "proc_pidinfo failed");
        }
        
        tci->coalition_resource_id = pci.coalition_id[COALITION_TYPE_RESOURCE];
        tci->coalition_jetsam_id   = pci.coalition_id[COALITION_TYPE_JETSAM];
    }
    
    /* uptrs */
    {
        const int uptrs_count = proc_list_uptrs(pid, NULL, 0);
        if (uptrs_count > 0) {
            uint32_t bufsize = sizeof(uint64_t) * uptrs_count;
            uint64_t *uptrs = malloc(bufsize);
            if (NULL == uptrs) {
                errx(EX_OSERR, "failed to allocate space for uptrs");
            }
            
            __unused const int fetched_uptrs_count = proc_list_uptrs(pid, uptrs, bufsize);
            assert(fetched_uptrs_count == uptrs_count);
            
            tci->udata_ptrs_count = uptrs_count;
            tci->udata_ptrs = uptrs;
        }
    }
    
    /* workqueue info */
    {
        extern int __proc_info(int32_t callnum, int32_t pid, uint32_t flavor, uint64_t arg, user_addr_t buffer, int32_t buffersize);
        
        struct proc_workqueueinfo pwqinfo = {0};
        int err = __proc_info(PROC_INFO_CALL_PIDINFO, pid, PROC_PIDWORKQUEUEINFO,
                          (uint64_t)0, (user_addr_t)&pwqinfo,
                          (uint32_t)sizeof(pwqinfo));
        if (PROC_PIDWORKQUEUEINFO_SIZE != err) {
            errx(EX_OSERR, "PROC_PIDWORKQUEUEINFO failed");
        }
        
        copy_pwqinfo(tci, &pwqinfo);
    }
    
    /* rusage */
    {
        struct rusage_info_v3 rui = {0};
        static_assert(sizeof(rui.ri_uuid) == sizeof(tci->ri_uuid), "UUID sizes mismatch");
        
        const int rusage_err = proc_pid_rusage(pid, RUSAGE_INFO_V3, (rusage_info_t *)&rui);
        if (rusage_err != 0) {
            errx(EX_OSERR, "proc_pid_rusage failed");
        }
        
        copy_rusage_info(tci, &rui);
    }
    
    /* task_vm_info_data_t */
    {
        task_vm_info_data_t ti = {0};
        mach_msg_type_number_t task_info_count = TASK_VM_INFO_REV3_COUNT;
        const kern_return_t task_info_err = task_info(task, TASK_VM_INFO_PURGEABLE, (task_info_t)&ti, &task_info_count);

        if (KERN_SUCCESS != task_info_err) {
            errx(EX_OSERR, "task_info failed with the error %s", mach_error_string(task_info_err));
        }
        
        /* fill the parts of the struct that aren't filled via `task_info` */
        int get_ledgers_err = get_ledgers(pid, tci);
        if (0 != get_ledgers_err) {
            errx(EX_OSERR, "get_ledgers failed");
        }
        
        tci_copy_ti(tci, &ti);
    }
}

#define copy_corpse_crashinfo_data(dest) \
  do {                                   \
    assert(data_size == sizeof(dest));   \
    dest = *(typeof(dest) *)data;        \
  } while (0);

static void populate_task_crash_info_for_corpse(struct task_crashinfo_note_data *tcip, mach_vm_address_t kcd_addr_begin, mach_vm_size_t kcd_size)
{
    kcdata_iter_t iter = kcdata_iter((void *)kcd_addr_begin, (unsigned long)kcd_size);
    
    KCDATA_ITER_FOREACH(iter) {
        const uint32_t data_type = kcdata_iter_type(iter);
        const uint32_t data_size = kcdata_iter_size(iter);
        const void *data = kcdata_iter_payload(iter);
        
        switch (data_type) {
            case TASK_CRASHINFO_TASK_IS_CORPSE_FORK:
                assert(data_size == sizeof(boolean_t));
                if (*(boolean_t *)data) {
                    tcip->task_type = kTASK_CRASHINFO_TASK_TYPE_CORPSE_FORK;
                } else {
                    tcip->task_type = kTASK_CRASHINFO_TASK_TYPE_CORPSE;
                }
                break;
            case TASK_CRASHINFO_PID:
                copy_corpse_crashinfo_data(tcip->pid);
                break;
            case TASK_CRASHINFO_PPID:
                copy_corpse_crashinfo_data(tcip->ppid);
                break;
            case TASK_CRASHINFO_UID:
                copy_corpse_crashinfo_data(tcip->uid);
                break;
            case TASK_CRASHINFO_GID:
                copy_corpse_crashinfo_data(tcip->gid);
                break;
            case TASK_CRASHINFO_PROC_STARTTIME: {
                assert(data_size == sizeof(struct timeval64));
                const struct timeval64 tv64 = *(struct timeval64 *)data;
                tcip->proc_starttime_sec = tv64.tv_sec;
                tcip->proc_starttime_usec = tv64.tv_usec;
                break;
            }
                
            case TASK_CRASHINFO_PROC_NAME: {
                char nul_terminated_proc_name[2 * MAXCOMLEN + 1] = "";
                strncpy(nul_terminated_proc_name, (char *)data,
                        sizeof(nul_terminated_proc_name) - 1);
                nul_terminated_proc_name[sizeof(nul_terminated_proc_name) - 1] = '\0';
                
                tcip->proc_name = strdup(nul_terminated_proc_name);
                break;
            }
                
            case TASK_CRASHINFO_PROC_PATH: {
                char nul_terminated_proc_path[MAXPATHLEN + 1] = "";
                strncpy(nul_terminated_proc_path, (char *)data,
                        sizeof(nul_terminated_proc_path) - 1);
                nul_terminated_proc_path[sizeof(nul_terminated_proc_path) - 1] = '\0';
                
                tcip->proc_path = strdup(nul_terminated_proc_path);
                break;
            }
                
            case TASK_CRASHINFO_RESPONSIBLE_PID:
                copy_corpse_crashinfo_data(tcip->responsible_pid);
                break;
                
            case TASK_CRASHINFO_PROC_PERSONA_ID:
                copy_corpse_crashinfo_data(tcip->proc_persona_id);
                break;
                
            case TASK_CRASHINFO_USERSTACK:
                copy_corpse_crashinfo_data(tcip->userstack);
                break;
            case TASK_CRASHINFO_PROC_FLAGS:
                copy_corpse_crashinfo_data(tcip->proc_flags);
                break;
                
            case TASK_CRASHINFO_ARGSLEN:
                copy_corpse_crashinfo_data(tcip->argslen);
                break;
            case TASK_CRASHINFO_PROC_ARGC:
                copy_corpse_crashinfo_data(tcip->proc_argc);
                break;
                
            case TASK_CRASHINFO_DIRTY_FLAGS:
                copy_corpse_crashinfo_data(tcip->dirty_flags);
                break;
                
            case TASK_CRASHINFO_PROC_CSFLAGS:
                copy_corpse_crashinfo_data(tcip->proc_csflags);
                break;
                
            case TASK_CRASHINFO_EXCEPTION_CODES: {
                assert(data_size == 2 * sizeof(mach_exception_data_t));
                const mach_exception_data_t exc_data = (mach_exception_data_t)data;
                tcip->exception_code_code = exc_data[0];
                tcip->exception_code_subcode = exc_data[1];
                break;
            }
                
            case TASK_CRASHINFO_CRASHED_THREADID:
                copy_corpse_crashinfo_data(tcip->crashed_threadid);
                break;
            case TASK_CRASHINFO_EXCEPTION_TYPE:
                copy_corpse_crashinfo_data(tcip->crashinfo_exception_type);
                break;
                
            case KCDATA_TYPE_ARRAY:
                switch (kcdata_iter_array_elem_type(iter))  {
                    case TASK_CRASHINFO_UDATA_PTRS: {
                        /* note that the task crashinfo won't include TASK_CRASHINFO_UDATA_PTRS if
                         * there are many udata pointers (rdar://122580096) */
                        
                        assert(kcdata_iter_array_elem_size(iter) == sizeof(uint64_t));
                        
                        uint32_t element_count = kcdata_iter_array_elem_count(iter);
                        uint32_t bufsize = element_count * sizeof(uint64_t);
                        
                        /* KCDATA_TYPE_ARRAY pads its contents to 16 bytes */
                        assert(data_size == roundup(bufsize, 16));
                        
                        const uint64_t *pointers_from_kernel = (uint64_t *)data;
                        
                        tcip->udata_ptrs = malloc(sizeof(*pointers_from_kernel) * element_count);
                        memcpy(tcip->udata_ptrs, pointers_from_kernel, element_count * sizeof(*pointers_from_kernel));
                        tcip->udata_ptrs_count = element_count;
                        
                        break;
                    }
                        
                    case TASK_CRASHINFO_COALITION_ID:
                        assert(kcdata_iter_array_elem_count(iter) == COALITION_NUM_TYPES);
                        assert(kcdata_iter_array_elem_size(iter) == sizeof(uint64_t));
                        
                        tcip->coalition_resource_id = ((uint64_t *)data)[COALITION_TYPE_RESOURCE];
                        tcip->coalition_jetsam_id = ((uint64_t *)data)[COALITION_TYPE_JETSAM];
                        
                        break;
                }
                break;
                
            case TASK_CRASHINFO_MEMORY_LIMIT:
                copy_corpse_crashinfo_data(tcip->memory_limit);
                break;
            case TASK_CRASHINFO_MEMORY_LIMIT_INCREASE:
                copy_corpse_crashinfo_data(tcip->memory_limit_increase);
                break;
            case TASK_CRASHINFO_MEMORYSTATUS_EFFECTIVE_PRIORITY:
                copy_corpse_crashinfo_data(tcip->memorystatus_effective_priority);
                break;
                
            case TASK_CRASHINFO_WORKQUEUEINFO: {
                assert(data_size == sizeof(struct proc_workqueueinfo));
                const struct proc_workqueueinfo pwqinfo = *(struct proc_workqueueinfo *)data;
                copy_pwqinfo(tcip, &pwqinfo);
                break;
            }
                
            case TASK_CRASHINFO_RUSAGE_INFO: {
                assert(data_size >= sizeof(struct rusage_info_v3));
                const struct rusage_info_v3 rui = *(struct rusage_info_v3 *)data;
                copy_rusage_info(tcip, &rui);
                break;
            }
                
            case TASK_CRASHINFO_LEDGER_INTERNAL:
                copy_corpse_crashinfo_data(tcip->ledger_internal);
                break;
            case TASK_CRASHINFO_LEDGER_INTERNAL_COMPRESSED:
                copy_corpse_crashinfo_data(tcip->ledger_internal_compressed);
                break;
            case TASK_CRASHINFO_LEDGER_IOKIT_MAPPED:
                copy_corpse_crashinfo_data(tcip->ledger_iokit_mapped);
                break;
            case TASK_CRASHINFO_LEDGER_ALTERNATE_ACCOUNTING:
                copy_corpse_crashinfo_data(tcip->ledger_alternate_accounting);
                break;
            case TASK_CRASHINFO_LEDGER_ALTERNATE_ACCOUNTING_COMPRESSED:
                copy_corpse_crashinfo_data(tcip->ledger_alternate_compressed);
                break;
            case TASK_CRASHINFO_LEDGER_PURGEABLE_NONVOLATILE:
                copy_corpse_crashinfo_data(tcip->ledger_purgable_nonvolatile);
                break;
            case TASK_CRASHINFO_LEDGER_PURGEABLE_NONVOLATILE_COMPRESSED:
                copy_corpse_crashinfo_data(tcip->ledger_purgable_nonvolatile_compressed);
                break;
            case TASK_CRASHINFO_LEDGER_PAGE_TABLE:
                copy_corpse_crashinfo_data(tcip->ledger_page_table);
                break;
            case TASK_CRASHINFO_LEDGER_PHYS_FOOTPRINT:
                copy_corpse_crashinfo_data(tcip->ledger_phys_footprint);
                break;
            case TASK_CRASHINFO_LEDGER_PHYS_FOOTPRINT_LIFETIME_MAX:
                copy_corpse_crashinfo_data(tcip->ledger_phys_footprint_lifetime_max);
                break;
            case TASK_CRASHINFO_LEDGER_NETWORK_NONVOLATILE:
                copy_corpse_crashinfo_data(tcip->ledger_network_nonvolatile);
                break;
            case TASK_CRASHINFO_LEDGER_NETWORK_NONVOLATILE_COMPRESSED:
                copy_corpse_crashinfo_data(tcip->ledger_network_nonvolatile_compressed);
                break;
            case TASK_CRASHINFO_LEDGER_WIRED_MEM:
                copy_corpse_crashinfo_data(tcip->ledger_wired_mem);
                break;
            case TASK_CRASHINFO_LEDGER_TAGGED_FOOTPRINT:
                copy_corpse_crashinfo_data(tcip->ledger_tagged_footprint);
                break;
            case TASK_CRASHINFO_LEDGER_TAGGED_FOOTPRINT_COMPRESSED:
                copy_corpse_crashinfo_data(tcip->ledger_tagged_footprint_compressed);
                break;
            case TASK_CRASHINFO_LEDGER_MEDIA_FOOTPRINT:
                copy_corpse_crashinfo_data(tcip->ledger_media_footprint);
                break;
            case TASK_CRASHINFO_LEDGER_MEDIA_FOOTPRINT_COMPRESSED:
                copy_corpse_crashinfo_data(tcip->ledger_media_footprint_compressed);
                break;
            case TASK_CRASHINFO_LEDGER_GRAPHICS_FOOTPRINT:
                copy_corpse_crashinfo_data(tcip->ledger_graphics_footprint);
                break;
            case TASK_CRASHINFO_LEDGER_GRAPHICS_FOOTPRINT_COMPRESSED:
                copy_corpse_crashinfo_data(tcip->ledger_graphics_footprint_compressed);
                break;
            case TASK_CRASHINFO_LEDGER_NEURAL_FOOTPRINT:
                copy_corpse_crashinfo_data(tcip->ledger_neural_footprint);
                break;
            case TASK_CRASHINFO_LEDGER_NEURAL_FOOTPRINT_COMPRESSED:
                copy_corpse_crashinfo_data(tcip->ledger_neural_footprint_compressed);
                break;
        }
    }
    
    if (KCDATA_ITER_FOREACH_FAILED(iter)) {
        errx(EX_OSERR, "failed to iterate kcdata for corpse");
    }
}

struct task_crashinfo_note_data *
prepare_task_crashinfo_note(task_t task)
{
    struct task_crashinfo_note_data *tci = calloc(1, sizeof(*tci));
    
    /* will be set only for corpses */
    tci->crashinfo_exception_type = INT32_MAX;
    
    /* mach timebase info */
    {
        mach_timebase_info_data_t info = {0};
        const kern_return_t mach_timebase_err = mach_timebase_info(&info);
        if (KERN_SUCCESS != mach_timebase_err) {
            err_mach(mach_timebase_err, NULL, "mach_timebase_info");
            errx(EX_OSERR, "Failed to get mach timebase info");
        }
        
        tci->mach_timebase_info_denom = info.denom;
        tci->mach_timebase_info_numer = info.numer;
    }
    
    /* all image infos range */
    {
        task_dyld_info_data_t local_dyld_info = {0};
        mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
        const kern_return_t task_info_err = task_info(task, TASK_DYLD_INFO, (task_info_t)&local_dyld_info, &count);
        if (KERN_SUCCESS != task_info_err) {
            err_mach(task_info_err, NULL, "task_info");
            errx(EX_OSERR, "Failed to get task info");
        }
        
        tci->dyld_all_image_infos_addr = (uint64_t)local_dyld_info.all_image_info_addr;
        tci->dyld_all_image_infos_size = (uint64_t)local_dyld_info.all_image_info_size;
    }
    
    /* dyld shared cache range
     *
     * equivalent to CSRangeOfDyldSharedCacheInTask()
     */
    {
        tci->dyld_shared_cache_base_addr = UINT64_MAX;
        tci->dyld_shared_cache_size = UINT64_MAX;
        
        kern_return_t dyld_err = KERN_FAILURE;
        
        dyld_process_t process = dyld_process_create_for_task(task, &dyld_err);
        if (!process) {
            errx(EX_OSERR, "Failed to create dyld process info with error %s", mach_error_string(dyld_err));
        }
        
        dyld_process_snapshot_t snapshot = dyld_process_snapshot_create_for_process(process, &dyld_err);
        if (!snapshot) {
            errx(EX_OSERR, "Failed to create dyld snapshot with error %s", mach_error_string(dyld_err));
        }
        
        dyld_shared_cache_t cache = dyld_process_snapshot_get_shared_cache(snapshot);
        if (!cache) {
            errx(EX_OSERR, "Failed to get dyld shared cache info");
        }
        
        if (!dyld_shared_cache_is_mapped_private(cache)) {
            /* ignore private shared caches */
            tci->dyld_shared_cache_base_addr = dyld_shared_cache_get_base_address(cache);
            tci->dyld_shared_cache_size = dyld_shared_cache_get_mapped_size(cache);
        }
        
        dyld_process_snapshot_dispose(snapshot);
        dyld_process_dispose(process);
    }
    
    /* owned vm objects */
    {
        tci->vm_object_query_datas = NULL;
        tci->vm_object_query_datas_count = 0;

        const char *get_owned_objects_sysctl_name = "vm.get_owned_vmobjects";

        // First query for the size of the buffer that we should attempt to allocate.
        size_t buflen = 0;
        int sysctl_err = sysctlbyname(get_owned_objects_sysctl_name, NULL, &buflen, (void*)&task, sizeof(task));
        if (sysctl_err != 0 && errno != ENOENT) {
            errx(EX_OSERR, "sysctl vm.get_owned_vmobjects failed");
        }

        if (buflen != 0) {
            /* there exists a small race where the number of owned VM objects could increase between the time
             * we ask for the size and when we do the actual query. the sysctl does not indicate to us
             * when it is unable to return all of the owned vm objects in this case, so to be slightly more
             * safe we always allocate an additional small amount for the buffer.
             *
             * in the rare case that even this extra safety is not sufficient, we retry with a larger size.
             * the retries occur when we notice that the provided buffer was entirely filled, which may indicate
             * there were more entries that did not fit.
             */
            const size_t buflenSafetySize = sizeof(vm_object_query_data_t) * 16;
            size_t requestedBuflen = buflen;
            size_t returnedBuflen = 0;
            vmobject_list_output_t buf = NULL;

            do {
                requestedBuflen += buflenSafetySize;
                buf = realloc(buf, requestedBuflen);
                if (NULL == buf) {
                    errx(EX_OSERR, "failed to allocate space for VM object query datas");
                }

                returnedBuflen = requestedBuflen;
                sysctl_err = sysctlbyname(get_owned_objects_sysctl_name, buf, &returnedBuflen, (void*)&task, sizeof(task));
                if (sysctl_err != 0 && errno != ENOENT) {
                    errx(EX_OSERR, "sysctl vm.get_owned_vmobjects failed");
                }
            } while (returnedBuflen == requestedBuflen);

            if (returnedBuflen != 0) {
                /* buffer should always at least be large enough to tell us how many entries exist
                 * except in the case where buflen is 0, handled above
                 */
                assert(returnedBuflen >= sizeof(buf->entries));

                /* buffer should be large enough to have the data for all the entries */
                assert((returnedBuflen - sizeof(buf->entries)) >= buf->entries * sizeof(vm_object_query_data_t));

                struct portable_vm_object_query_data_t *portable_result = malloc((size_t)buf->entries * sizeof(struct portable_vm_object_query_data_t));
                for (uint64_t i = 0; i < buf->entries; i++) {
                    vm_object_query_t query_data = &(buf->data[i]);
                    portable_result[i] = (struct portable_vm_object_query_data_t) {
                        .object_id       = query_data->object_id,
                        .virtual_size    = query_data->virtual_size,
                        .resident_size   = query_data->resident_size,
                        .wired_size      = query_data->wired_size,
                        .reusable_size   = query_data->reusable_size,
                        .compressed_size = query_data->compressed_size,
                        .vo_no_footprint = query_data->vo_no_footprint,
                        .vo_ledger_tag   = query_data->vo_ledger_tag,
                        .purgable        = query_data->purgable,
                    };
                }
                
                tci->vm_object_query_datas = portable_result;
                tci->vm_object_query_datas_count = buf->entries;
            }

            free(buf);
        }
    }
    
    /* determine if this is a live task or a corpse */
    mach_vm_address_t kcd_addr_begin = 0;
    mach_vm_size_t kcd_size = 0;
    const kern_return_t map_corpse_info_err = task_map_corpse_info_64(mach_task_self(), task, &kcd_addr_begin, &kcd_size);
    if (KERN_SUCCESS == map_corpse_info_err) {
        /* corpse */
        populate_task_crash_info_for_corpse(tci, kcd_addr_begin, kcd_size);
        
        mach_vm_deallocate(mach_task_self(), kcd_addr_begin, kcd_size);
    } else {
        /* live task */
        populate_task_crash_info_for_live_task(tci, task);
    }
    
    return tci;
}

#define minimum_page_size (MIN(vm_kernel_page_size, vm_page_size))
#define minimum_page_mask (MIN(vm_kernel_page_mask, vm_page_mask))

static mach_vm_address_t
trunc_minimum_page(mach_vm_address_t x)
{
    mach_vm_size_t mask = minimum_page_mask;
    return (x & (~mask));
}

static mach_vm_address_t
round_minimum_page(mach_vm_address_t x)
{
    mach_vm_size_t mask = minimum_page_mask;
    return trunc_minimum_page(x + mask);
}

void
set_collect_phys_footprint(bool collect)
{
    static const char *kVM_SELF_FOOTPRINT_SYSCTL = "vm.self_region_footprint";
    
    int collect_as_int = collect;
    int sysctl_ret = sysctlbyname(kVM_SELF_FOOTPRINT_SYSCTL, NULL, 0, (void *)&collect_as_int, sizeof(collect_as_int));
    if (sysctl_ret != 0 && errno != ENOENT) {
        fprintf(stderr, "Error setting sysctl %s: %s (%d)\n", kVM_SELF_FOOTPRINT_SYSCTL, strerror(errno), errno);
    }
}


static void
populate_region_dispositions(task_t task, struct region_infos_note_data *region_infos_note, bool use_phys_footprint)
{
    set_collect_phys_footprint(use_phys_footprint);
    
    for (uint64_t region_index = 0; region_index < region_infos_note->regions_count; region_index++) {
        struct region_infos_note_region_data *region_info = &region_infos_note->regions[region_index];
        
        /* SM_EMPTY regions are always 0's; safely ignore to save space */
        if (!region_info->is_submap && region_info->share_mode != SM_EMPTY) {
            const mach_vm_address_t page_aligned_start_addr = trunc_minimum_page(region_info->vmaddr);
            const mach_vm_address_t page_aligned_end_addr   = round_minimum_page(region_info->vmaddr + region_info->vmsize);
            
            const mach_vm_size_t pages_spanned_by_region = (page_aligned_end_addr - page_aligned_start_addr) / minimum_page_size;
            
            size_t num_bytes = (size_t)pages_spanned_by_region * sizeof(int);
            int *raw_dispositions = malloc(num_bytes);
            if (NULL == raw_dispositions) {
                errx(EX_OSERR, "out of memory for dispositions");
            }
            
            mach_vm_size_t found_pages_count = pages_spanned_by_region;
            const kern_return_t vm_page_range_query_err = mach_vm_page_range_query(task,
                                                                                   page_aligned_start_addr,
                                                                                   page_aligned_end_addr - page_aligned_start_addr,
                                                                                   (mach_vm_address_t)raw_dispositions,
                                                                                   &found_pages_count);
            if (KERN_SUCCESS != vm_page_range_query_err) {
                errx(EX_OSERR, "couldn't fetch page dispositions");
            }
            
            const size_t converted_dispositions_byte_count = (size_t)found_pages_count * sizeof(uint16_t);
            uint16_t *converted_dispositions = malloc(converted_dispositions_byte_count);
            if (NULL == converted_dispositions) {
                errx(EX_OSERR, "out of memory for dispositions");
            }
            
            for (uint64_t disposition_index = 0; disposition_index < found_pages_count; disposition_index++) {
                const int      raw_disposition      = raw_dispositions[disposition_index];
                const unsigned unsigned_disposition = *(unsigned *)&raw_disposition;
                const uint16_t uint16_disposition   = (uint16_t)unsigned_disposition;
                
                converted_dispositions[disposition_index] = uint16_disposition;
            }
            
            if (use_phys_footprint) {
                region_info->phys_footprint_dispositions = converted_dispositions;
                region_info->phys_footprint_disposition_count = found_pages_count;
            } else {
                region_info->non_phys_footprint_dispositions = converted_dispositions;
                region_info->non_phys_footprint_disposition_count = found_pages_count;
            }
            
            free(raw_dispositions);
        }
    }
}

struct region_info_args {
    struct region_infos_note_data *region_infos_note;
    task_t task;
    pid_t pid_for_live_task; /* -1 for corpses */
};

static walk_return_t
populate_region_infos_excluding_dispositions(struct region *region, void *arg)
{
    const struct region_info_args *args = (struct region_info_args *)arg;
    
    const char *region_filename = NULL;
    int32_t purgeability = INT32_MAX;
    if (!region->r_info.is_submap) {
        if (region->r_purgable != VM_PURGABLE_DENY) {
            /* VM_PURGABLE_DENY implies mach_vm_purgable_control() did not return success */
            purgeability = region->r_purgable;
        }
        
        /* if this region is a mapped file and we're targeting a live task, use the pid-based API
         * proc_regionfilename() to capture the mapped file's path.  rdar://123653830 requests an
         * API which would also work for corpses, but nothing like that exists yet.
         */
        if (region->r_info.external_pager && args->pid_for_live_task > 0) {
            char path[MAXPATHLEN + 1] = "";
            const int path_length = proc_regionfilename(args->pid_for_live_task, region->r_range.addr, path, sizeof(path));
            if (0 < path_length && (size_t)path_length < sizeof(path)) {
                path[path_length] = '\0';
                
                region_filename = strdup(path);
            }
        }
    }
    
    struct region_infos_note_region_data *region_info = &args->region_infos_note->regions[args->region_infos_note->regions_count];
    
    *region_info = (struct region_infos_note_region_data) {
        .vmaddr = region->r_range.addr,
        .vmsize = region->r_range.size,
        
#ifdef CONFIG_SUBMAP
        .nesting_depth = region->r_depth,
#endif
        
        .protection               = region->r_info.protection,
        .max_protection           = region->r_info.max_protection,
        .inheritance              = region->r_info.inheritance,
        .offset                   = region->r_info.offset,
        .user_tag                 = region->r_info.user_tag,
        .pages_resident           = region->r_info.pages_resident,
        .pages_shared_now_private = region->r_info.pages_shared_now_private,
        .pages_swapped_out        = region->r_info.pages_swapped_out,
        .pages_dirtied            = region->r_info.pages_dirtied,
        .ref_count                = region->r_info.ref_count,
        .shadow_depth             = region->r_info.shadow_depth,
        .external_pager           = region->r_info.external_pager,
        .share_mode               = region->r_info.share_mode,
        .is_submap                = !!(region->r_info.is_submap),
        .behavior                 = region->r_info.behavior,
        .object_id                = region->r_info.object_id,
        .user_wired_count         = region->r_info.user_wired_count,
        .pages_reusable           = region->r_info.pages_reusable,
        .object_id_full           = region->r_info.object_id_full,
        
        .purgeability = purgeability,
        
        .mapped_file_path = region_filename,
        
        /* dispositions will be populated by populate_region_dispositions */
        
        .phys_footprint_dispositions      = NULL,
        .phys_footprint_disposition_count = 0,
        
        .non_phys_footprint_dispositions      = NULL,
        .non_phys_footprint_disposition_count = 0,
    };
    
    args->region_infos_note->regions_count += 1;
    
    return WALK_CONTINUE;
}

static walk_return_t
count_elems(struct region *r, void *arg)
{
#pragma unused(r)
    size_t *count = (size_t *)arg;
    *count = *count + 1;

    return WALK_CONTINUE;
}

struct region_infos_note_data *
prepare_region_infos_note(const task_t task)
{
    /* rebuild a region_list that contains *all* of the regions. don't optimize away any regions
     * (via simple_region_optimization) and explicitly include the IOKIT regions.
     */
    struct regionhead *rhead = build_region_list(task, true);
    
    size_t region_count = 0;
    walk_region_list(rhead, count_elems, &region_count);
    
    const size_t region_infos_note_size = sizeof(struct region_infos_note_data) + (region_count * sizeof(struct region_infos_note_region_data));
    struct region_infos_note_data *region_infos_note = calloc(1, region_infos_note_size);
    if (NULL == region_infos_note) {
        errx(EX_OSERR, "failed to allocate space for vm info LC_NOTE");
    }

    region_infos_note->regions_count = 0;
    
    pid_t pid_for_live_task = -1;
    {
        /* determine if this is a live task or a corpse */
        mach_vm_address_t kcd_addr_begin = 0;
        mach_vm_size_t kcd_size = 0;
        const kern_return_t map_corpse_info_err = task_map_corpse_info_64(mach_task_self(), task, &kcd_addr_begin, &kcd_size);
        if (KERN_SUCCESS == map_corpse_info_err) {
            /* corpse */
            mach_vm_deallocate(mach_task_self(), kcd_addr_begin, kcd_size);
        } else {
            /* live task */
            const kern_return_t pid_for_task_err = pid_for_task(task, &pid_for_live_task);
            if (KERN_SUCCESS != pid_for_task_err) {
                errx(EX_OSERR, "failed to get pid for task with error %s", mach_error_string(pid_for_task_err));
            }
        }
    }
    
    struct region_info_args args = {
        .region_infos_note = region_infos_note,
        .task = task,
        .pid_for_live_task = pid_for_live_task,
    };
    walk_region_list(rhead, populate_region_infos_excluding_dispositions, &args);
    
    populate_region_dispositions(task, region_infos_note, true);
    populate_region_dispositions(task, region_infos_note, false);
    
    del_region_list(rhead);

    return region_infos_note;
}

#pragma mark -- Tearing down note data --

void destroy_task_crash_info_note_data(struct task_crashinfo_note_data *task_crashinfo_note)
{
    free((void*)task_crashinfo_note->proc_name);
    free((void*)task_crashinfo_note->proc_path);
    free((void*)task_crashinfo_note->parent_proc_name);
    free((void*)task_crashinfo_note->parent_proc_path);
    
    free((void*)task_crashinfo_note->udata_ptrs);

    free(task_crashinfo_note->vm_object_query_datas);
    
    free(task_crashinfo_note);
}

void destroy_region_infos_note_data(struct region_infos_note_data *region_infos_note)
{
    for (uint64_t region_index = 0; region_index < region_infos_note->regions_count; region_index++) {
        const struct region_infos_note_region_data *region = &region_infos_note->regions[region_index];
        
        free((void*)region->mapped_file_path);
        free((void*)region->phys_footprint_dispositions);
        free((void*)region->non_phys_footprint_dispositions);
    }
    
    free(region_infos_note);
}
