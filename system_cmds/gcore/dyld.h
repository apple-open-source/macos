/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "corefile.h"
#include "utils.h"

#include <mach-o/dyld_images.h>
#include <mach-o/dyld_process_info.h>
#include <uuid/uuid.h>

#ifndef _DYLD_H
#define _DYLD_H

struct libent {
    const char *le_filename;            // (points into le_pathname!)
    char *le_pathname;
    uuid_t le_uuid;
    uint64_t le_mhaddr;                 // address in target process
    const native_mach_header_t *le_mh;  // copy mapped into this address space
	struct vm_range le_vr;				// vmaddr, vmsize bounds in target process
	mach_vm_offset_t le_objoff;			// offset from le_mhaddr to first __TEXT seg
};

extern const struct libent *libent_lookup_byuuid(const uuid_t);
extern const struct libent *libent_lookup_first_bytype(uint32_t);
extern const struct libent *libent_insert(const char *, const uuid_t, uint64_t, const native_mach_header_t *, const struct vm_range *, mach_vm_offset_t);
extern bool libent_build_nametable(task_t, dyld_process_info);

extern dyld_process_info get_task_dyld_info(task_t);
extern bool get_sc_uuid(dyld_process_info, uuid_t);
extern void free_task_dyld_info(dyld_process_info);

#endif /* _DYLD_H */
