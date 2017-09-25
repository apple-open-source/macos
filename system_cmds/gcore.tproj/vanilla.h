/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "vm.h"

#ifndef _VANILLA_H
#define _VANILLA_H

struct proc_bsdinfo;

extern void validate_core_header(const native_mach_header_t *, off_t);
extern int coredump(task_t, int, const struct proc_bsdinfo *);
extern int coredump_write(task_t, int, struct regionhead *, const uuid_t, mach_vm_offset_t, mach_vm_offset_t);
extern struct regionhead *coredump_prepare(task_t, uuid_t);

#endif /* _VANILLA_H */
