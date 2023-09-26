/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "loader_additions.h"
#include "dyld_shared_cache.h"
#include "region.h"

#include <mach-o/loader.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/types.h>

#ifndef _COREFILE_H
#define _COREFILE_H

#if defined(__LP64__)
typedef struct mach_header_64 native_mach_header_t;
typedef struct segment_command_64 native_segment_command_t;
#define NATIVE_MH_MAGIC		MH_MAGIC_64
#define NATIVE_LC_SEGMENT	LC_SEGMENT_64
#else
typedef struct mach_header native_mach_header_t;
typedef struct segment_command native_segment_command_t;
#define NATIVE_MH_MAGIC		MH_MAGIC
#define NATIVE_LC_SEGMENT	LC_SEGMENT
#endif

static __inline const struct load_command *next_lc(const struct load_command *lc) {
	if (lc->cmdsize && (lc->cmdsize & 3) == 0)
		return (const void *)((caddr_t)lc + lc->cmdsize);
	return NULL;
}

extern native_segment_command_t *make_native_segment_command(void *, const struct vm_range *, const struct file_range *, vm_prot_t, vm_prot_t);

extern native_mach_header_t *make_corefile_mach_header(void *);
extern struct proto_coreinfo_command *make_coreinfo_command(native_mach_header_t *, void *, const uuid_t, uint64_t, uint64_t);

static __inline void mach_header_inc_ncmds(native_mach_header_t *mh, uint32_t inc) {
    mh->ncmds += inc;
}

static __inline void mach_header_inc_sizeofcmds(native_mach_header_t *mh, uint32_t inc) {
    mh->sizeofcmds += inc;
}

struct size_core {
    unsigned long count; /* number-of-objects */
    size_t headersize;   /* size in mach header */
    mach_vm_offset_t memsize;     /* size in memory */
};

struct size_segment_data {
    struct size_core ssd_vanilla;  /* full segments with data */
    struct size_core ssd_sparse;   /* sparse segments with data */
    struct size_core ssd_fileref;  /* full & sparse segments with uuid file references */
    struct size_core ssd_zfod;     /* full segments with zfod pages */
};

struct write_segment_data {
    task_t wsd_task;
    native_mach_header_t *wsd_mh;
    void *wsd_lc;
    int wsd_fd;
	bool wsd_nocache;
    off_t wsd_foffset;
    off_t wsd_nwritten;
};

#endif /* _COREFILE_H */
