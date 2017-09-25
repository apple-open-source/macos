/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
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

extern void del_fileref_region(struct region *);
extern void del_zfod_region(struct region *);
extern void del_sparse_region(struct region *);
extern void del_vanilla_region(struct region *);

extern struct regionhead *build_region_list(task_t);
extern int walk_region_list(struct regionhead *, walk_region_cbfn_t, void *);
extern void del_region_list(struct regionhead *);

extern void print_memory_region_header(void);
extern void print_memory_region(const struct region *);
extern void print_one_memory_region(const struct region *);

extern walk_region_cbfn_t region_print_memory;
extern walk_region_cbfn_t region_write_memory;
extern walk_region_cbfn_t region_size_memory;

extern int is_tagged(task_t, mach_vm_offset_t, mach_vm_offset_t, unsigned);

#ifdef RDAR_23744374
extern bool is_actual_size(const task_t, const struct region *, mach_vm_size_t *);
#endif

#endif /* _VM_H */
