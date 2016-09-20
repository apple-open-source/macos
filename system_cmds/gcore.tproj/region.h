/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include <sys/queue.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <uuid/uuid.h>

#ifndef _REGION_H
#define _REGION_H

struct regionop;
struct subregion;

struct region {
    STAILQ_ENTRY(region) r_linkage;

    mach_vm_offset_t r_address;
    mach_vm_offset_t r_size;

#define _R_ADDR(r)      ((r)->r_address)
#define _R_SIZE(r)      ((r)->r_size)
#define R_SETADDR(r, a)	((r)->r_address = (a))
#define R_SETSIZE(r, z)	((r)->r_size = (z))
#define R_ENDADDR(r)	(_R_ADDR(r) + _R_SIZE(r))

    vm_region_submap_info_data_64_t r_info;
    vm_page_info_basic_data_t r_pageinfo;

#ifdef CONFIG_PURGABLE
    int r_purgable;
#endif
#ifdef CONFIG_SUBMAP
    int r_depth;
#endif
    boolean_t
    r_insharedregion,
    r_inzfodregion,
    r_incommregion;

#ifdef CONFIG_REFSC
    /*
     * This field may be non-NULL if the region is a read-only part
     * of a mapped file (i.e. the shared cache) and thus
     * doesn't need to be copied.
     */
    struct {
        const struct libent *fr_libent;
        off_t fr_offset;
    } *r_fileref;
#endif

    /*
     * These (optional) fields are filled in after we parse the information
     * about the dylibs we've mapped, as provided by dyld.
     */
    struct subregion **r_subregions;
    unsigned r_nsubregions;

    const struct regionop *r_op;
};

static __inline const mach_vm_offset_t R_ADDR(const struct region *r) {
    return _R_ADDR(r);
}

static __inline const mach_vm_offset_t R_SIZE(const struct region *r) {
    return _R_SIZE(r);
}

/*
 * Describes the disposition of the region after a walker returns
 */
typedef enum {
    WALK_CONTINUE,		// press on ..
    WALK_DELETE_REGION,	// discard this region, then continue
    WALK_TERMINATE,		// early termination, no error
    WALK_ERROR,		// early termination, error
} walk_return_t;

struct size_core;
struct write_segment_data;

typedef walk_return_t walk_region_cbfn_t(struct region *, void *);

struct regionop {
    void (*rop_print)(const struct region *);
    walk_return_t (*rop_write)(const struct region *, struct write_segment_data *);
    void (*rop_delete)(struct region *);
};

#define ROP_PRINT(r)    (((r)->r_op->rop_print)(r))
#define ROP_WRITE(r, w) (((r)->r_op->rop_write)(r, w))
#define ROP_DELETE(r)   (((r)->r_op->rop_delete)(r))

extern const struct regionop vanilla_ops, sparse_ops, zfod_ops;
#ifdef CONFIG_REFSC
extern const struct regionop fileref_ops;
#endif

struct regionhead;

#endif /* _REGION_H */
