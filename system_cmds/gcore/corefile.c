/*
 * Copyright (c) 2021 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "corefile.h"
#include "sparse.h"
#include "utils.h"
#include "vm.h"
#include "notes.h"
#include "portable_task_crash_info_t.h"
#include "portable_region_infos_t.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <compression.h>
#include <sys/param.h>
#include <libgen.h>
#include <sys/stat.h>

native_mach_header_t *
make_corefile_mach_header(void *data)
{
    native_mach_header_t *mh = data;
    mh->magic = NATIVE_MH_MAGIC;
    mh->filetype = MH_CORE;
#if defined(__LP64__)
    const int is64 = 1;
#else
    const int is64 = 0;
#endif
#if defined(__i386__) || defined(__x86_64__)
    mh->cputype = is64 ? CPU_TYPE_X86_64 : CPU_TYPE_I386;
    mh->cpusubtype = is64 ? CPU_SUBTYPE_X86_64_ALL : CPU_SUBTYPE_I386_ALL;
#elif defined(__arm__) || defined(__arm64__)
    mh->cputype = is64 ? CPU_TYPE_ARM64 : CPU_TYPE_ARM;
    mh->cpusubtype = is64 ? CPU_SUBTYPE_ARM64_ALL : CPU_SUBTYPE_ARM_ALL;
#else
#error undefined
#endif
    return mh;
}

struct proto_coreinfo_command *
make_coreinfo_command(native_mach_header_t *mh, void *data, const uuid_t aoutid, uint64_t address, uint64_t dyninfo)
{
    struct proto_coreinfo_command *cc = data;
    cc->cmd = proto_LC_COREINFO;
    cc->cmdsize = sizeof (*cc);
    cc->version = 1;
    cc->type = proto_CORETYPE_USER;
	cc->pageshift = (uint16_t)pageshift_host;
    cc->address = address;
    uuid_copy(cc->uuid, aoutid);
    cc->dyninfo = dyninfo;
    mach_header_inc_ncmds(mh, 1);
    mach_header_inc_sizeofcmds(mh, cc->cmdsize);
    return cc;
}

native_segment_command_t *
make_native_segment_command(void *data, const struct vm_range *vr, const struct file_range *fr, vm_prot_t maxprot, vm_prot_t initprot)
{
    native_segment_command_t *sc = data;
    sc->cmd = NATIVE_LC_SEGMENT;
    sc->cmdsize = sizeof (*sc);
    assert(V_SIZE(vr));
	sc->vmaddr = (unsigned long)V_ADDR(vr);
	sc->vmsize = (unsigned long)V_SIZE(vr);
	sc->fileoff = (unsigned long)F_OFF(fr);
	sc->filesize = (unsigned long)F_SIZE(fr);
    sc->maxprot = maxprot;
    sc->initprot = initprot;
    sc->nsects = 0;
    sc->flags = 0;
    return sc;
}

static struct proto_coredata_command *
make_coredata_command(void *data, const struct vm_range *vr, const struct file_range *fr, const vm_region_submap_info_data_64_t *info, unsigned comptype, unsigned purgable)
{
	struct proto_coredata_command *cc = data;
	cc->cmd = proto_LC_COREDATA;
	cc->cmdsize = sizeof (*cc);
	assert(V_SIZE(vr));
	cc->vmaddr = V_ADDR(vr);
	cc->vmsize = V_SIZE(vr);
	cc->fileoff = F_OFF(fr);
	cc->filesize = F_SIZE(fr);
	cc->maxprot = info->max_protection;
	cc->prot = info->protection;
	cc->flags = COMP_MAKE_FLAGS(comptype);
	cc->share_mode = info->share_mode;
	assert(purgable <= UINT8_MAX);
	cc->purgable = (uint8_t)purgable;
	assert(info->user_tag <= UINT8_MAX);
	cc->tag = (uint8_t)info->user_tag;
	cc->extp = info->external_pager;
	return cc;
}

#pragma mark -- Write LC_NOTEs for memory analysis tools --

static walk_return_t
pwrite_memory(struct write_segment_data *, const void *, size_t, const struct vm_range *);

static struct file_range 
note_write_memory(struct write_segment_data *wsd, const void *addr, size_t size)
{
    const struct vm_range vr = {
        .addr = (mach_vm_offset_t)addr,
        .size = size,
    };
    struct file_range fr = {
        .off = wsd->wsd_foffset,
        .size = size
    };
    if (WALK_CONTINUE != pwrite_memory(wsd, addr, size, &vr)) {
        return (struct file_range){0, 0};
    }
    return fr;
}

static uint64_t 
write_string_to_wsda(struct write_segment_data *wsda, const char *string)
{
    if (NULL == string) {
        return UINT64_MAX;
    }
    
    const struct file_range fr = note_write_memory(wsda, string, strlen(string) + 1);
    return fr.off;
}

static uint64_t
write_uint16_array_to_wsda(struct write_segment_data *wsda, const uint16_t *array, size_t array_count)
{
    if (NULL == wsda) {
        return UINT64_MAX;
    }
    
    const struct file_range fr = note_write_memory(wsda, array, array_count * sizeof(*array));
    return fr.off;
}

struct note_command *
make_region_infos_note(native_mach_header_t *mh, struct note_command *nc, struct write_segment_data *wsda, const struct region_infos_note_data *region_infos_note)
{
    nc->cmd = LC_NOTE;
    nc->cmdsize = sizeof(struct note_command);
    strlcpy(nc->data_owner, "vm info", sizeof(nc->data_owner));
    
    const size_t portable_region_infos_size = sizeof(struct portable_region_infos_t) + (sizeof(struct portable_region_info_t) * (size_t)region_infos_note->regions_count);
    struct portable_region_infos_t *portable_reigon_infos = calloc(1, portable_region_infos_size);
    portable_reigon_infos->major_version = PORTABLE_REGION_INFOS_CURRENT_MAJOR_VERSION;
    portable_reigon_infos->minor_version = PORTABLE_REGION_INFOS_CURRENT_MINOR_VERSION;
    portable_reigon_infos->regions_count = region_infos_note->regions_count;
    
    for (uint64_t region_index = 0; region_index < region_infos_note->regions_count; region_index++) {
        const struct region_infos_note_region_data *region_data = &region_infos_note->regions[region_index];
        
        uint64_t mapped_file_path_file_offset = UINT64_MAX;
        if (region_data->mapped_file_path) {
            mapped_file_path_file_offset = write_string_to_wsda(wsda, region_data->mapped_file_path);
            if (mapped_file_path_file_offset == 0) {
                return NULL;
            }
        }
        
        uint64_t phys_footprint_dispositions_file_offset = UINT64_MAX;
        if (NULL != region_data->phys_footprint_dispositions) {
            phys_footprint_dispositions_file_offset = write_uint16_array_to_wsda(wsda, region_data->phys_footprint_dispositions, (size_t)region_data->phys_footprint_disposition_count);
            if (phys_footprint_dispositions_file_offset == 0) {
                return NULL;
            }
        }
        
        uint64_t non_phys_footprint_dispositions_file_offset = UINT64_MAX;
        if (NULL != region_data->non_phys_footprint_dispositions) {
            non_phys_footprint_dispositions_file_offset = write_uint16_array_to_wsda(wsda, region_data->non_phys_footprint_dispositions, (size_t)region_data->non_phys_footprint_disposition_count);
            if (non_phys_footprint_dispositions_file_offset == 0) {
                return NULL;
            }
        }
        
        portable_reigon_infos->regions[region_index] = (struct portable_region_info_t){
            .vmaddr = region_data->vmaddr,
            .vmsize = region_data->vmsize,

            .nesting_depth = region_data->nesting_depth,

            .protection               = region_data->protection,
            .max_protection           = region_data->max_protection,
            .inheritance              = region_data->inheritance,
            .offset                   = region_data->offset,
            .user_tag                 = region_data->user_tag,
            .pages_resident           = region_data->pages_resident,
            .pages_shared_now_private = region_data->pages_shared_now_private,
            .pages_swapped_out        = region_data->pages_swapped_out,
            .pages_dirtied            = region_data->pages_dirtied,
            .ref_count                = region_data->ref_count,
            .shadow_depth             = region_data->shadow_depth,
            .external_pager           = region_data->external_pager,
            .share_mode               = region_data->share_mode,
            .is_submap                = region_data->is_submap,
            .behavior                 = region_data->behavior,
            .object_id                = region_data->object_id,
            .user_wired_count         = region_data->user_wired_count,
            .pages_reusable           = region_data->pages_reusable,
            .object_id_full           = region_data->object_id_full,

            .purgeability = region_data->purgeability,

            .mapped_file_path_offset = mapped_file_path_file_offset,

            .phys_footprint_disposition_count   = region_data->phys_footprint_disposition_count,
            .phys_footprint_dispositions_offset = phys_footprint_dispositions_file_offset,

            .non_phys_footprint_disposition_count   = region_data->non_phys_footprint_disposition_count,
            .non_phys_footprint_dispositions_offset = non_phys_footprint_dispositions_file_offset,
        };
    }
    
    const struct file_range portable_reigon_infos_file_range = note_write_memory(wsda, portable_reigon_infos, portable_region_infos_size);
    if (portable_reigon_infos_file_range.off == 0) {
        return NULL;
    }
    
    nc->offset = F_OFF(&portable_reigon_infos_file_range);
    nc->size = F_SIZE(&portable_reigon_infos_file_range);
    
    mach_header_inc_ncmds(mh, 1);
    mach_header_inc_sizeofcmds(mh, nc->cmdsize);

    return nc;
}

struct note_command *
make_task_crashinfo_note(native_mach_header_t *mh, struct note_command *nc, struct write_segment_data *wsda, const struct task_crashinfo_note_data *task_crashinfo_note)
{
    nc->cmd = LC_NOTE;
    nc->cmdsize = sizeof(struct note_command);
    strlcpy(nc->data_owner, "task crashinfo", sizeof(nc->data_owner));
    
    const uint64_t proc_name_file_offset = write_string_to_wsda(wsda, task_crashinfo_note->proc_name);
    if (proc_name_file_offset == 0) {
        return NULL;
    }
    const uint64_t proc_path_file_offset = write_string_to_wsda(wsda, task_crashinfo_note->proc_path);
    if (proc_path_file_offset == 0) {
        return NULL;
    }
    const uint64_t parent_proc_name_file_offset = write_string_to_wsda(wsda, task_crashinfo_note->parent_proc_name);
    if (parent_proc_name_file_offset == 0) {
        return NULL;
    }
    const uint64_t parent_proc_path_file_offset = write_string_to_wsda(wsda, task_crashinfo_note->parent_proc_path);
    if (parent_proc_path_file_offset == 0) {
        return NULL;
    }
    
    assert((NULL == task_crashinfo_note->udata_ptrs && 0 == task_crashinfo_note->udata_ptrs_count) || (NULL != task_crashinfo_note->udata_ptrs && task_crashinfo_note->udata_ptrs_count > 0));
    uint64_t udata_ptrs_file_offset = UINT64_MAX;
    if (task_crashinfo_note->udata_ptrs) {
        udata_ptrs_file_offset = note_write_memory(wsda, task_crashinfo_note->udata_ptrs, sizeof(*task_crashinfo_note->udata_ptrs) * (size_t)task_crashinfo_note->udata_ptrs_count).off;
        if (udata_ptrs_file_offset == 0) {
            return NULL;
        }
    }
    
    assert((NULL == task_crashinfo_note->vm_object_query_datas && 0 == task_crashinfo_note->vm_object_query_datas_count) || (NULL != task_crashinfo_note->vm_object_query_datas && task_crashinfo_note->vm_object_query_datas_count > 0));
    uint64_t vm_object_query_datas_file_offset = UINT64_MAX;
    if (task_crashinfo_note->vm_object_query_datas) {
        vm_object_query_datas_file_offset = note_write_memory(wsda, task_crashinfo_note->vm_object_query_datas, sizeof(*task_crashinfo_note->vm_object_query_datas) * (size_t)task_crashinfo_note->vm_object_query_datas_count).off;
        if (vm_object_query_datas_file_offset == 0) {
            return NULL;
        }
    }
    
    struct portable_task_crash_info_t portable_task_crash_info = {
        .major_version = PORTABLE_TASK_CRASH_INFO_CURRENT_MAJOR_VERSION,
        .minor_version = PORTABLE_TASK_CRASH_INFO_CURRENT_MINOR_VERSION,

        .task_type = task_crashinfo_note->task_type,

        .pid                 = task_crashinfo_note->pid,
        .ppid                = task_crashinfo_note->ppid,
        .uid                 = task_crashinfo_note->uid,
        .gid                 = task_crashinfo_note->gid,
        .proc_starttime_sec  = task_crashinfo_note->proc_starttime_sec,
        .proc_starttime_usec = task_crashinfo_note->proc_starttime_usec,

        .proc_name_offset        = proc_name_file_offset,
        .proc_path_offset        = proc_path_file_offset,
        .parent_proc_name_offset = parent_proc_name_file_offset,
        .parent_proc_path_offset = parent_proc_path_file_offset,

        .responsible_pid = task_crashinfo_note->responsible_pid,

        .proc_persona_id = task_crashinfo_note->proc_persona_id,

        .userstack  = task_crashinfo_note->userstack,
        .proc_flags = task_crashinfo_note->proc_flags,

        .argslen   = task_crashinfo_note->argslen,
        .proc_argc = task_crashinfo_note->proc_argc,

        .dirty_flags = task_crashinfo_note->dirty_flags,

        .proc_csflags = task_crashinfo_note->proc_csflags,

        .exception_code_code    = task_crashinfo_note->exception_code_code,
        .exception_code_subcode = task_crashinfo_note->exception_code_subcode,

        .crashed_threadid         = task_crashinfo_note->crashed_threadid,
        .crashinfo_exception_type = task_crashinfo_note->crashinfo_exception_type,

        .coalition_resource_id = task_crashinfo_note->coalition_resource_id,
        .coalition_jetsam_id   = task_crashinfo_note->coalition_jetsam_id,

        .dyld_all_image_infos_addr = task_crashinfo_note->dyld_all_image_infos_addr,
        .dyld_all_image_infos_size = task_crashinfo_note->dyld_all_image_infos_size,

        .dyld_shared_cache_base_addr = task_crashinfo_note->dyld_shared_cache_base_addr,
        .dyld_shared_cache_size      = task_crashinfo_note->dyld_shared_cache_size,

        .udata_ptrs_offset = udata_ptrs_file_offset,
        .udata_ptrs_count  = task_crashinfo_note->udata_ptrs_count,

        .vm_object_query_datas_offset = vm_object_query_datas_file_offset,
        .vm_object_query_datas_count  = task_crashinfo_note->vm_object_query_datas_count,

        .memory_limit                    = task_crashinfo_note->memory_limit,
        .memory_limit_increase           = task_crashinfo_note->memory_limit_increase,
        .memorystatus_effective_priority = task_crashinfo_note->memorystatus_effective_priority,

        .pwq_nthreads       = task_crashinfo_note->pwq_nthreads,
        .pwq_runthreads     = task_crashinfo_note->pwq_runthreads,
        .pwq_blockedthreads = task_crashinfo_note->pwq_blockedthreads,
        .pwq_state          = task_crashinfo_note->pwq_state,

        .mach_timebase_info_numer = task_crashinfo_note->mach_timebase_info_numer,
        .mach_timebase_info_denom = task_crashinfo_note->mach_timebase_info_denom,

        /* ri_uuid populated below */
        .ri_user_time                     = task_crashinfo_note->ri_user_time,
        .ri_system_time                   = task_crashinfo_note->ri_system_time,
        .ri_pkg_idle_wkups                = task_crashinfo_note->ri_pkg_idle_wkups,
        .ri_interrupt_wkups               = task_crashinfo_note->ri_interrupt_wkups,
        .ri_pageins                       = task_crashinfo_note->ri_pageins,
        .ri_wired_size                    = task_crashinfo_note->ri_wired_size,
        .ri_resident_size                 = task_crashinfo_note->ri_resident_size,
        .ri_proc_start_abstime            = task_crashinfo_note->ri_proc_start_abstime,
        .ri_proc_exit_abstime             = task_crashinfo_note->ri_proc_exit_abstime,
        .ri_child_user_time               = task_crashinfo_note->ri_child_user_time,
        .ri_child_system_time             = task_crashinfo_note->ri_child_system_time,
        .ri_child_pkg_idle_wkups          = task_crashinfo_note->ri_child_pkg_idle_wkups,
        .ri_child_interrupt_wkups         = task_crashinfo_note->ri_child_interrupt_wkups,
        .ri_child_pageins                 = task_crashinfo_note->ri_child_pageins,
        .ri_child_elapsed_abstime         = task_crashinfo_note->ri_child_elapsed_abstime,
        .ri_diskio_bytesread              = task_crashinfo_note->ri_diskio_bytesread,
        .ri_diskio_byteswritten           = task_crashinfo_note->ri_diskio_byteswritten,
        .ri_cpu_time_qos_default          = task_crashinfo_note->ri_cpu_time_qos_default,
        .ri_cpu_time_qos_maintenance      = task_crashinfo_note->ri_cpu_time_qos_maintenance,
        .ri_cpu_time_qos_background       = task_crashinfo_note->ri_cpu_time_qos_background,
        .ri_cpu_time_qos_utility          = task_crashinfo_note->ri_cpu_time_qos_utility,
        .ri_cpu_time_qos_legacy           = task_crashinfo_note->ri_cpu_time_qos_legacy,
        .ri_cpu_time_qos_user_initiated   = task_crashinfo_note->ri_cpu_time_qos_user_initiated,
        .ri_cpu_time_qos_user_interactive = task_crashinfo_note->ri_cpu_time_qos_user_interactive,
        .ri_billed_system_time            = task_crashinfo_note->ri_billed_system_time,
        .ri_serviced_system_time          = task_crashinfo_note->ri_serviced_system_time,

        .ledger_internal                        = task_crashinfo_note->ledger_internal,
        .ledger_internal_compressed             = task_crashinfo_note->ledger_internal_compressed,
        .ledger_iokit_mapped                    = task_crashinfo_note->ledger_iokit_mapped,
        .ledger_alternate_accounting            = task_crashinfo_note->ledger_alternate_accounting,
        .ledger_alternate_compressed            = task_crashinfo_note->ledger_alternate_compressed,
        .ledger_purgable_nonvolatile            = task_crashinfo_note->ledger_purgable_nonvolatile,
        .ledger_purgable_nonvolatile_compressed = task_crashinfo_note->ledger_purgable_nonvolatile_compressed,
        .ledger_page_table                      = task_crashinfo_note->ledger_page_table,
        .ledger_phys_footprint                  = task_crashinfo_note->ledger_phys_footprint,
        .ledger_phys_footprint_lifetime_max     = task_crashinfo_note->ledger_phys_footprint_lifetime_max,
        .ledger_network_nonvolatile             = task_crashinfo_note->ledger_network_nonvolatile,
        .ledger_network_nonvolatile_compressed  = task_crashinfo_note->ledger_network_nonvolatile_compressed,
        .ledger_wired_mem                       = task_crashinfo_note->ledger_wired_mem,
        .ledger_tagged_footprint                = task_crashinfo_note->ledger_tagged_footprint,
        .ledger_tagged_footprint_compressed     = task_crashinfo_note->ledger_tagged_footprint_compressed,
        .ledger_media_footprint                 = task_crashinfo_note->ledger_media_footprint,
        .ledger_media_footprint_compressed      = task_crashinfo_note->ledger_media_footprint_compressed,
        .ledger_graphics_footprint              = task_crashinfo_note->ledger_graphics_footprint,
        .ledger_graphics_footprint_compressed   = task_crashinfo_note->ledger_graphics_footprint_compressed,
        .ledger_neural_footprint                = task_crashinfo_note->ledger_neural_footprint,
        .ledger_neural_footprint_compressed     = task_crashinfo_note->ledger_neural_footprint_compressed,
    };
    
    memcpy(&portable_task_crash_info.ri_uuid, task_crashinfo_note->ri_uuid, sizeof(uuid_t));
    
    const struct file_range portable_task_crash_info_file_range = note_write_memory(wsda, &portable_task_crash_info, sizeof(portable_task_crash_info));
    if (portable_task_crash_info_file_range.off == 0) {
        return NULL;
    }
    
    nc->offset = F_OFF(&portable_task_crash_info_file_range);
    nc->size = F_SIZE(&portable_task_crash_info_file_range);
    
    mach_header_inc_ncmds(mh, 1);
    mach_header_inc_sizeofcmds(mh, nc->cmdsize);
    
    return nc;
}

static size_t
sizeof_segment_command(void) 
{
	return opt->extended ?
		sizeof(struct proto_coredata_command) : sizeof(native_segment_command_t);
}

static struct load_command *
make_segment_command(void *data, const struct vm_range *vr, const struct file_range *fr, const vm_region_submap_info_data_64_t *info, unsigned comptype, int purgable)
{
	if (opt->extended)
		make_coredata_command(data, vr, fr, info, comptype, purgable);
	else
		make_native_segment_command(data, vr, fr, info->max_protection, info->protection);
	return data;
}

/*
 * Increment the mach-o header data when we succeed
 */
static void
commit_load_command(struct write_segment_data *wsd, const struct load_command *lc)
{
    wsd->wsd_lc = (caddr_t)lc + lc->cmdsize;
    native_mach_header_t *mh = wsd->wsd_mh;
    mach_header_inc_ncmds(mh, 1);
    mach_header_inc_sizeofcmds(mh, lc->cmdsize);
}

#pragma mark -- Regions written as "file references" --

static size_t
cmdsize_fileref_command(const char *nm)
{
    size_t cmdsize = sizeof (struct proto_fileref_command);
    size_t len;
    if (0 != (len = strlen(nm))) {
        len++; // NUL-terminated for mmap sanity
        cmdsize += roundup(len, sizeof (long));
    }
    return cmdsize;
}

static void
size_fileref_subregion(const struct subregion *s, struct size_core *sc)
{
    assert(S_LIBENT(s));

    size_t cmdsize = cmdsize_fileref_command(S_PATHNAME(s));
    sc->headersize += cmdsize;
    sc->count++;
    sc->memsize += S_SIZE(s);
}

static void
size_fileref_region(const struct region *r, struct size_core *sc)
{
    assert(0 == r->r_nsubregions);
    assert(!r->r_inzfodregion);

    size_t cmdsize = cmdsize_fileref_command(r->r_fileref->fr_pathname);
    sc->headersize += cmdsize;
    sc->count++;
    sc->memsize += R_SIZE(r);
}

static struct proto_fileref_command *
make_fileref_command(void *data, const char *pathname, const uuid_t uuid,
    const struct vm_range *vr, const struct file_range *fr,
	const vm_region_submap_info_data_64_t *info, unsigned purgable)
{
    struct proto_fileref_command *fc = data;
    size_t len;

    fc->cmd = proto_LC_FILEREF;
    fc->cmdsize = sizeof (*fc);
    if (0 != (len = strlen(pathname))) {
        /*
         * Strings live immediately after the
         * command, and are included in the cmdsize
         */
        fc->filename.offset = sizeof (*fc);
        void *s = fc + 1;
        strlcpy(s, pathname, ++len); // NUL-terminated for mmap sanity
        fc->cmdsize += roundup(len, sizeof (long));
        assert(cmdsize_fileref_command(pathname) == fc->cmdsize);
    }

	/*
	 * A file reference allows different kinds of identifiers for
	 * the reference to be reconstructed.
	 */
	assert(info->external_pager);

	if (!uuid_is_null(uuid)) {
		uuid_copy(fc->id, uuid);
		fc->flags = FREF_MAKE_FLAGS(kFREF_ID_UUID);
	} else {
		struct stat st;
		if (-1 != stat(pathname, &st) && 0 != st.st_mtimespec.tv_sec) {
			/* "little-endian format timespec structure" */
			struct timespec ts = st.st_mtimespec;
			ts.tv_nsec = 0;	// allow touch(1) to fix things
			memset(fc->id, 0, sizeof(fc->id));
			memcpy(fc->id, &ts, sizeof(ts));
			fc->flags = FREF_MAKE_FLAGS(kFREF_ID_MTIMESPEC_LE);
		} else
			fc->flags = FREF_MAKE_FLAGS(kFREF_ID_NONE);
	}

	fc->vmaddr = V_ADDR(vr);
	assert(V_SIZE(vr));
	fc->vmsize = V_SIZE(vr);

	assert(F_OFF(fr) >= 0);
	fc->fileoff = F_OFF(fr);
	fc->filesize = F_SIZE(fr);

    assert(info->max_protection & VM_PROT_READ);
    fc->maxprot = info->max_protection;
    fc->prot = info->protection;

    fc->share_mode = info->share_mode;
    assert(purgable <= UINT8_MAX);
    fc->purgable = (uint8_t)purgable;
    assert(info->user_tag <= UINT8_MAX);
    fc->tag = (uint8_t)info->user_tag;
    fc->extp = info->external_pager;
    return fc;
}

/*
 * It's almost always more efficient to write out a reference to the
 * data than write out the data itself.
 */
static walk_return_t
make_fileref_subregion_command(const struct region *r, const struct subregion *s, struct write_segment_data *wsd)
{
    assert(S_LIBENT(s));
    if (OPTIONS_DEBUG(opt, 1) && !issubregiontype(s, SEG_TEXT) && !issubregiontype(s, SEG_LINKEDIT))
        printf("%s: unusual segment type %s from %s\n", __func__, S_MACHO_TYPE(s), S_FILENAME(s));
    assert((r->r_info.max_protection & VM_PROT_READ) == VM_PROT_READ);
    assert((r->r_info.protection & VM_PROT_WRITE) == 0);

    const struct libent *le = S_LIBENT(s);
	const struct file_range fr = {
		.off = S_MACHO_FILEOFF(s),
		.size = S_SIZE(s),
	};
    const struct proto_fileref_command *fc = make_fileref_command(wsd->wsd_lc, le->le_pathname, le->le_uuid, S_RANGE(s), &fr, &r->r_info, r->r_purgable);

    commit_load_command(wsd, (const void *)fc);
    if (OPTIONS_DEBUG(opt, 3)) {
        hsize_str_t hstr;
        printr(r, "ref '%s' %s (vm %llx-%llx, file offset %lld for %s)\n", S_FILENAME(s), S_MACHO_TYPE(s), (uint64_t)fc->vmaddr, (uint64_t)fc->vmaddr + fc->vmsize, (int64_t)fc->fileoff, str_hsize(hstr, fc->filesize));
    }
    return WALK_CONTINUE;
}

/*
 * Note that we may be asked to write reference segments whose protections
 * are rw- -- this -should- be ok as we don't convert the region to a file
 * reference unless we know it hasn't been modified.
 */
static walk_return_t
rop_fileref_makeheader(const struct region *r, struct write_segment_data *wsd)
{
    assert(0 == r->r_nsubregions);
    assert(r->r_info.user_tag != VM_MEMORY_IOKIT);
    assert((r->r_info.max_protection & VM_PROT_READ) == VM_PROT_READ);
    assert(!r->r_inzfodregion);

	const struct libent *le = r->r_fileref->fr_libent;
	const char *pathname = r->r_fileref->fr_pathname;
	const struct file_range fr = {
		.off = r->r_fileref->fr_offset,
		.size = R_SIZE(r),
	};
	const struct proto_fileref_command *fc = make_fileref_command(wsd->wsd_lc, pathname, le ? le->le_uuid : UUID_NULL, R_RANGE(r), &fr, &r->r_info, r->r_purgable);

    commit_load_command(wsd, (const void *)fc);
    if (OPTIONS_DEBUG(opt, 3)) {
        hsize_str_t hstr;
		printr(r, "ref '%s' %s (vm %llx-%llx, file offset %lld for %s)\n", pathname, "(type?)", (uint64_t)fc->vmaddr, (uint64_t)fc->vmaddr + fc->vmsize, (int64_t)fc->fileoff, str_hsize(hstr, fc->filesize));
    }
    return WALK_CONTINUE;
}

static walk_return_t
rop_noop(const struct region *__unused r,
    struct write_segment_data *__unused wsd)
{
    return WALK_CONTINUE;
}

/*
 * fileref regions only update the mach header, and so the header op
 * and the write op are the same, while the stream op is a no-op
 */
const struct regionop fileref_ops = {
    .rop_header = rop_fileref_makeheader,
    .rop_stream = rop_noop,
    .rop_pwrite = rop_fileref_makeheader,
    .rop_delete = rop_fileref_delete,
};


#pragma mark -- ZFOD segments written only to the header --

static void
size_zfod_region(const struct region *r, struct size_core *sc)
{
    assert(0 == r->r_nsubregions);
    assert(r->r_inzfodregion);
    sc->headersize += sizeof_segment_command();
    sc->count++;
    sc->memsize += R_SIZE(r);
}

static walk_return_t
rop_zfod_makeheader(const struct region *r, struct write_segment_data *wsd)
{
    assert(r->r_info.user_tag != VM_MEMORY_IOKIT);
    assert((r->r_info.max_protection & VM_PROT_READ) == VM_PROT_READ);

	const struct file_range fr = {
		.off = wsd->wsd_foffset,
		.size = 0,
	};
    make_segment_command(wsd->wsd_lc, R_RANGE(r), &fr, &r->r_info, 0, VM_PURGABLE_EMPTY);
    commit_load_command(wsd, wsd->wsd_lc);
    return WALK_CONTINUE;
}

/*
 * zfod regions only update the mach header, and so the header op
 * and the write op are the same, while the stream op is a no-op
 */
const struct regionop zfod_ops = {
    .rop_header = rop_zfod_makeheader,
    .rop_stream = rop_noop,
    .rop_pwrite = rop_zfod_makeheader,
    .rop_delete = rop_zfod_delete,
};

#pragma mark -- Regions containing data written at offsets beyond the header --

static void
report_write_memory(const struct vm_range *vr, int error, size_t size, off_t foffset, ssize_t nwritten)
{
    hsize_str_t hsz;
    printvr(vr, "writing %ld bytes at offset %lld -> ", size, foffset);
    if (error)
        printf("err #%d - %s ", error, strerror(error));
    else {
        printf("%s ", str_hsize(hsz, nwritten));
        if (size != (size_t)nwritten)
            printf("[%zd - incomplete write!] ", nwritten);
        else if (size != V_SIZE(vr))
            printf("(%s in memory) ",
                   str_hsize(hsz, V_SIZE(vr)));
    }
    printf("\n");
}

static walk_return_t
result_write_memory(int error, size_t size, ssize_t nwritten,
    struct write_segment_data *wsd)
{
    walk_return_t step = WALK_CONTINUE;
    switch (error) {
        case 0:
            if (size != (size_t)nwritten)
                step = WALK_ERROR;
            else {
                wsd->wsd_foffset += nwritten;
                wsd->wsd_nwritten += nwritten;
            }
            break;
        case EFAULT:    // transient mapping failure?
            break;
        default:        // EROFS, ENOSPC, EFBIG etc. */
            step = WALK_ERROR;
            break;
    }
    return step;
}

static walk_return_t
pwrite_memory(struct write_segment_data *wsd, const void *addr, size_t size, const struct vm_range *vr)
{
    assert(size);
    ssize_t nwritten;
	const int error = bounded_pwrite(wsd->wsd_fd, addr, size, wsd->wsd_foffset, &wsd->wsd_nocache, &nwritten);
    if (error || OPTIONS_DEBUG(opt, 3)) {
        report_write_memory(vr, error, size, wsd->wsd_foffset, nwritten);
    }
    return result_write_memory(error, size, nwritten, wsd);
}

static walk_return_t
stream_memory(struct write_segment_data *wsd, const void *addr, size_t size, const struct vm_range *vr)
{
    assert(size);
    ssize_t nwritten;
    const int error = bounded_write(wsd->wsd_fd, addr, size, &nwritten);
    if (error || OPTIONS_DEBUG(opt, 3)) {
        report_write_memory(vr, error, size, wsd->wsd_foffset, nwritten);
    }
    return result_write_memory(error, size, nwritten, wsd);
}

/*
 * Write a contiguous range of memory into the core file.
 * Apply compression, and chunk if necessary.
 */
static int
segment_compflags(compression_algorithm ca, unsigned *algnum)
{
    switch (ca) {
        case COMPRESSION_LZ4:
            *algnum = kCOMP_LZ4;
            break;
        case COMPRESSION_ZLIB:
            *algnum = kCOMP_ZLIB;
            break;
        case COMPRESSION_LZMA:
            *algnum = kCOMP_LZMA;
            break;
        case COMPRESSION_LZFSE:
            *algnum = kCOMP_LZFSE;
            break;
        default:
            err(EX_SOFTWARE, "unsupported compression algorithm %x", ca);
    }
    return 0;
}

static bool
is_file_mapped_shared(const struct region *r)
{
    if (r->r_info.external_pager)
        switch (r->r_info.share_mode) {
            case SM_TRUESHARED:     // sm=shm
            case SM_SHARED:         // sm=ali
            case SM_SHARED_ALIASED: // sm=s/a
                return true;
            default:
                break;
        }
    return false;
}

static walk_return_t
map_memory_range(struct write_segment_data *wsd, const struct region *r, const struct vm_range *vr, struct vm_range *dp)
{
    if (r->r_incommregion) {
        /*
         * Special case: for commpage access, copy from our own address space.
         */
        V_SETADDR(dp, 0);
        V_SETSIZE(dp, V_SIZE(vr));

        kern_return_t kr = mach_vm_allocate(mach_task_self(), &dp->addr, dp->size, VM_FLAGS_ANYWHERE);
        if (KERN_SUCCESS != kr || 0 == dp->addr) {
            err_mach(kr, r, "mach_vm_allocate c %llx-%llx", V_ADDR(vr), V_ENDADDR(vr));
            print_one_memory_region(r);
            return WALK_ERROR;
        }
        if (OPTIONS_DEBUG(opt, 3))
            printr(r, "copying from self %llx-%llx\n", V_ADDR(vr), V_ENDADDR(vr));
        memcpy((void *)dp->addr, (const void *)V_ADDR(vr), V_SIZE(vr));
        return WALK_CONTINUE;
    }

    if (!r->r_insharedregion && 0 == (r->r_info.protection & VM_PROT_READ)) {
        assert(0 != (r->r_info.max_protection & VM_PROT_READ)); // simple_region_optimization()

        /*
         * Special case: region that doesn't currently have read permission.
         * (e.g. --x/r-x permissions with tag 64 - JS JIT generated code
         * from com.apple.WebKit.WebContent)
         */
        const mach_vm_offset_t pagesize_host = 1u << pageshift_host;
        if (OPTIONS_DEBUG(opt, 3))
            printr(r, "unreadable (%s/%s), remap with read permission\n",
                str_prot(r->r_info.protection), str_prot(r->r_info.max_protection));
        V_SETADDR(dp, 0);
        V_SETSIZE(dp, V_SIZE(vr));
        vm_prot_t cprot, mprot;
        kern_return_t kr = mach_vm_remap(mach_task_self(), &dp->addr, V_SIZE(dp), pagesize_host - 1, true, wsd->wsd_task, V_ADDR(vr), true, &cprot, &mprot, VM_INHERIT_NONE);
            if (KERN_SUCCESS != kr) {
                err_mach(kr, r, "mach_vm_remap() %llx-%llx", V_ADDR(vr), V_ENDADDR(vr));
                return WALK_ERROR;
            }
            assert(r->r_info.protection == cprot && r->r_info.max_protection == mprot);
            kr = mach_vm_protect(mach_task_self(), V_ADDR(dp), V_SIZE(dp), false, VM_PROT_READ);
            if (KERN_SUCCESS != kr) {
                err_mach(kr, r, "mach_vm_protect() %llx-%llx", V_ADDR(vr), V_ENDADDR(vr));
                mach_vm_deallocate(mach_task_self(), V_ADDR(dp), V_SIZE(dp));
                return WALK_ERROR;
            }
        return WALK_CONTINUE;
    }

    /*
     * Most segments with data are read here
     */
    vm_offset_t data32 = 0;
    mach_msg_type_number_t data32_count;
    kern_return_t kr = mach_vm_read(wsd->wsd_task, V_ADDR(vr), V_SIZE(vr), &data32, &data32_count);
    switch (kr) {
        case KERN_SUCCESS:
            V_SETADDR(dp, data32);
            V_SETSIZE(dp, data32_count);
            break;
        case KERN_INVALID_ADDRESS:
            if (!r->r_insharedregion &&
                (VM_MEMORY_SKYWALK == r->r_info.user_tag || is_file_mapped_shared(r))) {
                if (OPTIONS_DEBUG(opt, 1)) {
                    /* not necessarily an error: mitigation below */
                    tag_str_t tstr;
                    printr(r, "mach_vm_read() failed (%s) -- substituting zeroed region\n", str_tagr(tstr, r));
                    if (OPTIONS_DEBUG(opt, 2))
                        print_one_memory_region(r);
                }
                V_SETSIZE(dp, V_SIZE(vr));
                kr = mach_vm_allocate(mach_task_self(), &dp->addr, V_SIZE(dp), VM_FLAGS_ANYWHERE);
                if (KERN_SUCCESS != kr || 0 == V_ADDR(dp))
                    err_mach(kr, r, "mach_vm_allocate() z %llx-%llx", V_ADDR(vr), V_ENDADDR(vr));
                break;
            }
            /*FALLTHROUGH*/
        default:
            err_mach(kr, r, "mach_vm_read() %llx-%llx", V_ADDR(vr), V_SIZE(vr));
            if (OPTIONS_DEBUG(opt, 1))
                print_one_memory_region(r);
            break;
    }
    if (kr != KERN_SUCCESS) {
        V_SETADDR(dp, 0);
        return WALK_ERROR;
    }

    /*
     * Sometimes (e.g. searchd) we may not be able to fetch all the pages
     * from the underlying mapped file, in which case replace those pages
     * with zfod pages (at least they compress efficiently) rather than
     * taking a SIGBUS when compressing them.
     *
     * XXX Perhaps we should just catch the SIGBUS, and if the faulting address
     * is in the right range, substitute zfod pages and rerun region compression?
     * Complex though, because the compression code may be multithreaded.
     */
    if (!r->r_insharedregion && is_file_mapped_shared(r)) {
        const mach_vm_offset_t pagesize_host = 1u << pageshift_host;

        if (r->r_info.pages_resident * pagesize_host == V_SIZE(dp))
            return WALK_CONTINUE;   // all pages resident, so skip ..

        if (OPTIONS_DEBUG(opt, 2))
            printr(r, "probing %llu pages in mapped-shared file\n", V_SIZE(dp) / pagesize_host);

        kr = KERN_SUCCESS;
        for (mach_vm_offset_t a = V_ADDR(dp); a < V_ENDADDR(dp); a += pagesize_host) {

            mach_msg_type_number_t pCount = VM_PAGE_INFO_BASIC_COUNT;
            vm_page_info_basic_data_t pInfo;

            kr = mach_vm_page_info(mach_task_self(), a, VM_PAGE_INFO_BASIC, (vm_page_info_t)&pInfo, &pCount);
            if (KERN_SUCCESS != kr) {
                err_mach(kr, NULL, "mach_vm_page_info() at %llx", a);
                break;
            }
            /* If the VM has the page somewhere, assume we can bring it back */
            if (pInfo.disposition & (VM_PAGE_QUERY_PAGE_PRESENT | VM_PAGE_QUERY_PAGE_REF | VM_PAGE_QUERY_PAGE_DIRTY))
                continue;

            /* Force the page to be fetched to see if it faults */
            mach_vm_size_t tsize = pagesize_host;
            void *tmp = valloc((size_t)tsize);
            const mach_vm_address_t vtmp = (mach_vm_address_t)tmp;

            switch (kr = mach_vm_read_overwrite(mach_task_self(), a, tsize, vtmp, &tsize)) {
                case KERN_SUCCESS:
                    break;
                case KERN_INVALID_ADDRESS: {
                    /* Content can't be found: replace it and the rest of the region with zero-fill pages */
                    if (OPTIONS_DEBUG(opt, 2)) {
                        printr(r, "mach_vm_read_overwrite() failed after %llu pages -- substituting zfod\n", (a - V_ADDR(dp)) / pagesize_host);
                        print_one_memory_region(r);
                    }
                    mach_vm_address_t va = a;
                    kr = mach_vm_allocate(mach_task_self(), &va, V_ENDADDR(dp) - va, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE);
                    if (KERN_SUCCESS != kr) {
                        err_mach(kr, r, "mach_vm_allocate() %llx", a);
                    } else {
                        assert(a == va);
                        a = V_ENDADDR(dp);  // no need to look any further
                    }
                    break;
                }
                default:
                    err_mach(kr, r, "mach_vm_overwrite() %llx", a);
                    break;
            }
            free(tmp);
            if (KERN_SUCCESS != kr)
                break;
        }
        if (KERN_SUCCESS != kr) {
            kr = mach_vm_deallocate(mach_task_self(), V_ADDR(dp), V_SIZE(dp));
            if (KERN_SUCCESS != kr && OPTIONS_DEBUG(opt, 1))
                err_mach(kr, r, "mach_vm_deallocate() pre %llx-%llx", V_ADDR(dp), V_ENDADDR(dp));
            V_SETADDR(dp, 0);
            return WALK_ERROR;
        }
    }

    return WALK_CONTINUE;
}

static void
unmap_memory_range(const struct region *r, const struct vm_range *dp)
{
    if (V_ADDR(dp)) {
        const kern_return_t kr = mach_vm_deallocate(mach_task_self(),
            V_ADDR(dp), V_SIZE(dp));
        if (KERN_SUCCESS != kr && OPTIONS_DEBUG(opt, 1)) {
            err_mach(kr, r, "mach_vm_deallocate() post %llx-%llx",
            V_ADDR(dp), V_SIZE(dp));
        }
    }
}

/*
 * pwrite a memory range and update the mach header to reflect it
 * Note that we can do per-segment compression here, which makes
 * things extra complicated.
 *
 * Since some regions can be inconveniently large,
 * chop them into multiple chunks as we compress them.
 * (mach_vm_read has 32-bit limitations too).
 */
static walk_return_t
pwrite_memory_range(struct write_segment_data *wsd,
    const struct region *r, mach_vm_offset_t vmaddr, mach_vm_offset_t vmsize)
{
    assert(R_ADDR(r) <= vmaddr && R_ENDADDR(r) >= vmaddr + vmsize);

    mach_vm_offset_t resid = vmsize;
    walk_return_t step = WALK_CONTINUE;

    do {
        vmsize = resid;
        vmsize = vmsize > INT32_MAX ? INT32_MAX : vmsize;
        if (opt->chunksize > 0 && vmsize > opt->chunksize)
            vmsize = opt->chunksize;
        assert(vmsize <= INT32_MAX);

        const struct vm_range vr = {
            .addr = vmaddr,
            .size = vmsize,
        };
        struct vm_range d, *dp = &d;

        step = map_memory_range(wsd, r, &vr, dp);
        if (WALK_CONTINUE != step)
            break;
        assert(0 != V_ADDR(dp) && 0 != V_SIZE(dp));
        const void *srcaddr = (const void *)V_ADDR(dp);

        mach_vm_behavior_set(mach_task_self(), V_ADDR(dp), V_SIZE(dp), VM_BEHAVIOR_SEQUENTIAL);

        void *dstbuf = NULL;
        unsigned algorithm = 0;
        size_t filesize;

		if (opt->extended) {
			dstbuf = malloc(V_SIZEOF(dp));
			if (dstbuf) {
				filesize = compression_encode_buffer(dstbuf, V_SIZEOF(dp), srcaddr, V_SIZEOF(dp), NULL, opt->calgorithm);
				if (filesize > 0 && filesize < V_SIZEOF(dp)) {
					srcaddr = dstbuf;	/* the data source is now heap, compressed */
                    mach_vm_deallocate(mach_task_self(), V_ADDR(dp), V_SIZE(dp));
                    V_SETADDR(dp, 0);
					if (segment_compflags(opt->calgorithm, &algorithm) != 0) {
						free(dstbuf);
                        mach_vm_deallocate(mach_task_self(), V_ADDR(dp), V_SIZE(dp));
                        V_SETADDR(dp, 0);
						step = WALK_ERROR;
                        break;
					}
				} else {
					free(dstbuf);
					dstbuf = NULL;
					filesize = V_SIZEOF(dp);
				}
			} else
				filesize = V_SIZEOF(dp);
			assert(filesize <= V_SIZEOF(dp));
		} else
			filesize = V_SIZEOF(dp);

        assert(filesize);

        /* Align the written memory to a 16K offset so it can be cleanly mmap()ped by tools */
        wsd->wsd_foffset = roundup(wsd->wsd_foffset, 0x4000);

		const struct file_range fr = {
			.off = wsd->wsd_foffset,
			.size = filesize,
		};
		make_segment_command(wsd->wsd_lc, &vr, &fr, &r->r_info, algorithm, r->r_purgable);
		step = pwrite_memory(wsd, srcaddr, filesize, &vr);
		if (dstbuf)
			free(dstbuf);

        unmap_memory_range(r, dp);

		if (WALK_ERROR == step)
			break;
		commit_load_command(wsd, wsd->wsd_lc);
		resid -= vmsize;
		vmaddr += vmsize;
	} while (resid);

    return step;
}

/*
 * A simpler version of pwrite_memory_range() (i.e. no compression)
 * used for streaming writes that just computes the segment header(s)
 * update(s) for this range of addresses.
 */
static walk_return_t
makeheader_memory_range(struct write_segment_data *wsd,
    const struct region *r, mach_vm_offset_t vmaddr, mach_vm_offset_t vmsize)
{
    assert(R_ADDR(r) <= vmaddr && R_ENDADDR(r) >= vmaddr + vmsize);
    assert(!opt->extended);

    mach_vm_offset_t resid = vmsize;
    walk_return_t step = WALK_CONTINUE;

    do {
        vmsize = resid;
        vmsize = vmsize > INT32_MAX ? INT32_MAX : vmsize;
        assert(vmsize <= INT32_MAX);

        const struct vm_range vr = {
            .addr = vmaddr,
            .size = vmsize,
        };
        struct vm_range d, *dp = &d;

        step = map_memory_range(wsd, r, &vr, dp);
        if (WALK_CONTINUE != step) {
            break;
        }
        assert(0 != V_ADDR(dp) && 0 != V_SIZE(dp));

        const size_t filesize = V_SIZEOF(dp);
        assert(filesize);

        const struct file_range fr = {
            .off = wsd->wsd_foffset,
            .size = filesize,
        };
        make_segment_command(wsd->wsd_lc, &vr, &fr, &r->r_info, 0, r->r_purgable);
        // predict what will be written
        wsd->wsd_foffset += filesize;
        wsd->wsd_nwritten += filesize;

        unmap_memory_range(r, dp);
        commit_load_command(wsd, wsd->wsd_lc);
        resid -= vmsize;
        vmaddr += vmsize;
    } while (resid);

    return step;
}

/*
 * A simpler version of pwrite_memory_range() (i.e. no compression) used for
 * streaming that just writes the segment data for this range of addresses.
 */
static walk_return_t
stream_memory_range(struct write_segment_data *wsd,
    const struct region *r, mach_vm_offset_t vmaddr, mach_vm_offset_t vmsize)
{
    assert(R_ADDR(r) <= vmaddr && R_ENDADDR(r) >= vmaddr + vmsize);
    assert(!opt->extended);

    mach_vm_offset_t resid = vmsize;
    walk_return_t step = WALK_CONTINUE;

    do {
        vmsize = resid;
        vmsize = vmsize > INT32_MAX ? INT32_MAX : vmsize;
        assert(vmsize <= INT32_MAX);

        const struct vm_range vr = {
            .addr = vmaddr,
            .size = vmsize,
        };
        struct vm_range d, *dp = &d;

        step = map_memory_range(wsd, r, &vr, dp);
        if (WALK_CONTINUE != step) {
            break;
        }
        assert(0 != V_ADDR(dp) && 0 != V_SIZE(dp));
        const void *srcaddr = (const void *)V_ADDR(dp);

        mach_vm_behavior_set(mach_task_self(), V_ADDR(dp), V_SIZE(dp), VM_BEHAVIOR_SEQUENTIAL);

        const size_t filesize = V_SIZEOF(dp);
        assert(filesize);

        step = stream_memory(wsd, srcaddr, filesize, &vr);
        unmap_memory_range(r, dp);
        if (WALK_ERROR == step) {
            break;
        }

        resid -= vmsize;
        vmaddr += vmsize;
    } while (resid);

    return step;
}

#ifdef RDAR_23744374
/*
 * Sigh. This is a workaround.
 * Find the vmsize as if the VM system manages ranges in host pagesize units
 * rather than application pagesize units.
 */
static mach_vm_size_t
getvmsize_host(const task_t task, const struct region *r)
{
    mach_vm_size_t vmsize_host = R_SIZE(r);

    if (pageshift_host != pageshift_app) {
        is_actual_size(task, r, &vmsize_host);
        if (OPTIONS_DEBUG(opt, 1) && R_SIZE(r) != vmsize_host)
            printr(r, "(region size tweak: was %llx, is %llx)\n", R_SIZE(r), vmsize_host);
    }
    return vmsize_host;
}
#else
static __inline mach_vm_size_t
getvmsize_host(__unused const task_t task, const struct region *r)
{
    return R_SIZE(r);
}
#endif

static walk_return_t
rop_sparse_nomakeheader(const struct region *__unused r,
    struct write_segment_data *__unused wsd)
{
    printf("%s: sparse regions not supported\n", __func__);
    return WALK_ERROR;
}

static walk_return_t
rop_sparse_nostream(const struct region *__unused r,
    struct write_segment_data *__unused wsd)
{
    printf("%s: sparse regions not supported", __func__);
    return WALK_ERROR;
}

static walk_return_t
rop_sparse_pwrite(const struct region *r, struct write_segment_data *wsd)
{
    assert(r->r_nsubregions);
    assert(!r->r_inzfodregion);
    assert(NULL == r->r_fileref);

    const mach_vm_size_t vmsize_host = getvmsize_host(wsd->wsd_task, r);
    walk_return_t step = WALK_CONTINUE;

    for (unsigned i = 0; i < r->r_nsubregions; i++) {
        const struct subregion *s = r->r_subregions[i];

        if (s->s_isuuidref)
            step = make_fileref_subregion_command(r, s, wsd);
        else {
            /* Write this one out as real data */
            mach_vm_size_t vmsize = S_SIZE(s);
            if (R_SIZE(r) != vmsize_host) {
                if (S_ADDR(s) + vmsize > R_ADDR(r) + vmsize_host) {
                    vmsize = R_ADDR(r) + vmsize_host - S_ADDR(s);
                    if (OPTIONS_DEBUG(opt, 3))
                        printr(r, "(subregion size tweak: was %llx, is %llx)\n",
                               S_SIZE(s), vmsize);
                }
            }
            step = pwrite_memory_range(wsd, r, S_ADDR(s), vmsize);
        }
        if (WALK_ERROR == step)
            break;
    }
    return step;
}

static walk_return_t
rop_vanilla_pwrite(const struct region *r, struct write_segment_data *wsd)
{
    assert(0 == r->r_nsubregions);
    assert(!r->r_inzfodregion);
    assert(NULL == r->r_fileref);

    const mach_vm_size_t vmsize_host = getvmsize_host(wsd->wsd_task, r);
    return pwrite_memory_range(wsd, r, R_ADDR(r), vmsize_host);
}

static walk_return_t
rop_vanilla_makeheader(const struct region *r, struct write_segment_data *wsd)
{
    assert(0 == r->r_nsubregions);
    assert(!r->r_inzfodregion);
    assert(NULL == r->r_fileref);

    const mach_vm_size_t vmsize_host = getvmsize_host(wsd->wsd_task, r);
    return makeheader_memory_range(wsd, r, R_ADDR(r), vmsize_host);
}

static walk_return_t
rop_vanilla_stream(const struct region *r, struct write_segment_data *wsd)
{
    assert(0 == r->r_nsubregions);
    assert(!r->r_inzfodregion);
    assert(NULL == r->r_fileref);

    const mach_vm_size_t vmsize_host = getvmsize_host(wsd->wsd_task, r);
    return stream_memory_range(wsd, r, R_ADDR(r), vmsize_host);
}

walk_return_t
pwrite_memory_region(struct region *r, void *arg)
{
    assert(r->r_info.user_tag != VM_MEMORY_IOKIT); // elided in walk_regions()
    assert((r->r_info.max_protection & VM_PROT_READ) == VM_PROT_READ);
    return ROP_PWRITE(r, arg);
}

walk_return_t
makeheader_memory_region(struct region *r, void *arg)
{
    assert(r->r_info.user_tag != VM_MEMORY_IOKIT); // elided in walk_regions()
    assert((r->r_info.max_protection & VM_PROT_READ) == VM_PROT_READ);
    return ROP_HEADER(r, arg);
}

walk_return_t
stream_memory_region(struct region *r, void *arg)
{
    assert(r->r_info.user_tag != VM_MEMORY_IOKIT); // elided in walk_regions()
    assert((r->r_info.max_protection & VM_PROT_READ) == VM_PROT_READ);
    return ROP_STREAM(r, arg);
}

/*
 * Handles the cases where segments are broken into chunks i.e. when
 * writing compressed segments.  Maximum segment size is 2GiB.
 */
static unsigned long
count_memory_range(mach_vm_offset_t vmsize)
{
    const mach_vm_offset_t chunksize =
        (opt->chunksize > 0 && opt->chunksize < INT32_MAX) ?
        opt->chunksize : INT32_MAX;
    return (unsigned long)((vmsize + chunksize - 1) / chunksize);
}

/*
 * A sparse region is likely a writable data segment described by
 * native_segment_command_t somewhere in the address space.
 */
static void
size_sparse_subregion(const struct subregion *s, struct size_core *sc)
{
    const unsigned long count = count_memory_range(S_SIZE(s));
    sc->headersize += sizeof_segment_command() * count;
    sc->count += count;
    sc->memsize += S_SIZE(s);
}

static void
size_sparse_region(const struct region *r, struct size_core *sc_sparse, struct size_core *sc_fileref)
{
    assert(0 != r->r_nsubregions);

    unsigned long entry_total = sc_sparse->count + sc_fileref->count;
    for (unsigned i = 0; i < r->r_nsubregions; i++) {
        const struct subregion *s = r->r_subregions[i];
        if (s->s_isuuidref)
            size_fileref_subregion(s, sc_fileref);
        else
            size_sparse_subregion(s, sc_sparse);
    }
    if (OPTIONS_DEBUG(opt, 3)) {
        /* caused by compression breaking a large region into chunks */
        entry_total = (sc_fileref->count + sc_sparse->count) - entry_total;
        if (entry_total > r->r_nsubregions)
            printr(r, "range contains %u subregions requires %lu segment commands\n",
               r->r_nsubregions, entry_total);
    }
}

/*
 * Sparse regions are complex, experimental, and thus
 * streaming is currently not supported.
 */

const struct regionop sparse_ops = {
    .rop_header = rop_sparse_nomakeheader,
    .rop_stream = rop_sparse_nostream,
    .rop_pwrite = rop_sparse_pwrite,
    .rop_delete = rop_sparse_delete,
};

static void
size_vanilla_region(const struct region *r, struct size_core *sc)
{
    assert(0 == r->r_nsubregions);

    const unsigned long count = count_memory_range(R_SIZE(r));
    sc->headersize += sizeof_segment_command() * count;
    sc->count += count;
    sc->memsize += R_SIZE(r);

    if (OPTIONS_DEBUG(opt, 3) && count != 1)
        printr(r, "range with 1 region, but requires %lu segment commands\n", count);
}

const struct regionop vanilla_ops = {
    .rop_header = rop_vanilla_makeheader,
    .rop_stream = rop_vanilla_stream,
    .rop_pwrite = rop_vanilla_pwrite,
    .rop_delete = rop_vanilla_delete,
};

walk_return_t
size_memory_region(struct region *r, void *arg)
{
    struct size_segment_data *ssd = arg;

    if (&zfod_ops == r->r_op)
        size_zfod_region(r, &ssd->ssd_zfod);
    else if (&fileref_ops == r->r_op)
        size_fileref_region(r, &ssd->ssd_fileref);
    else if (&sparse_ops == r->r_op)
        size_sparse_region(r, &ssd->ssd_sparse, &ssd->ssd_fileref);
    else if (&vanilla_ops == r->r_op)
        size_vanilla_region(r, &ssd->ssd_vanilla);
    else
        errx(EX_SOFTWARE, "%s: bad op", __func__);

    return WALK_CONTINUE;
}
