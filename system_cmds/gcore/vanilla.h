/*
 * Copyright (c) 2021 Apple Inc.  All rights reserved.
 */

#include "vm.h"
#include "notes.h"

#ifndef _VANILLA_H
#define _VANILLA_H

struct proc_bsdinfo;

extern void validate_core_header(const native_mach_header_t *, off_t);
extern int coredump(task_t, int, const struct proc_bsdinfo *);
extern int coredump_pwrite(task_t, int, struct regionhead *, const uuid_t, mach_vm_offset_t, mach_vm_offset_t, const struct task_crashinfo_note_data *, const struct region_infos_note_data *);
extern int coredump_stream(task_t, int, struct regionhead *);
extern struct regionhead *coredump_prepare(task_t, uuid_t);

#endif /* _VANILLA_H */
