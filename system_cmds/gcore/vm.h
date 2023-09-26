/*
 * Copyright (c) 2021 Apple Inc.  All rights reserved.
 */

#include <mach/mach.h>
#include <mach/mach_port.h>
#include <mach/task.h>
#include <mach/mach_vm.h>
#include <stdbool.h>

#include "corefile.h"
#include "region.h"

#ifndef _VM_H
#define _VM_H

extern void setpageshift(void);
extern int pageshift_host;
extern int pageshift_app;

struct region;
struct regionhead;

extern void rop_fileref_delete(struct region *);
extern void rop_zfod_delete(struct region *);
extern void rop_sparse_delete(struct region *);
extern void rop_vanilla_delete(struct region *);

extern struct regionhead *build_region_list(task_t);
extern int walk_region_list(struct regionhead *, walk_region_cbfn_t, void *);
extern void del_region_list(struct regionhead *);

extern void print_memory_region_header(void);
extern void print_memory_region(const struct region *);
extern void print_one_memory_region(const struct region *);

extern walk_region_cbfn_t region_print_memory;
extern walk_region_cbfn_t makeheader_memory_region;
extern walk_region_cbfn_t stream_memory_region;
extern walk_region_cbfn_t pwrite_memory_region;
extern walk_region_cbfn_t size_memory_region;

extern int is_tagged(task_t, mach_vm_offset_t, mach_vm_offset_t, unsigned);

#ifdef RDAR_23744374
extern bool is_actual_size(const task_t, const struct region *, mach_vm_size_t *);
#endif

static __inline boolean_t
in_zfod_region(const vm_region_submap_info_data_64_t *info)
{
    return info->share_mode == SM_EMPTY && !info->is_submap &&
        0 == info->object_id && !info->external_pager &&
        0 == info->pages_dirtied + info->pages_resident + info->pages_swapped_out;
}

#endif /* _VM_H */
