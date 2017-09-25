/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include <sys/queue.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <uuid/uuid.h>

#ifndef _REGION_H
#define _REGION_H

/*
 * A range of virtual memory
 */
struct vm_range {
	mach_vm_offset_t addr;
	mach_vm_offset_t size;
};

#define _V_ADDR(g)		((g)->addr)
#define _V_SIZE(g)		((g)->size)
#define V_SETADDR(g, a)	((g)->addr = (a))
#define V_SETSIZE(g, z)	((g)->size = (z))
#define V_ENDADDR(g)	(_V_ADDR(g) + _V_SIZE(g))

static __inline const mach_vm_offset_t V_ADDR(const struct vm_range *vr) {
	return _V_ADDR(vr);
}
static __inline const mach_vm_offset_t V_SIZE(const struct vm_range *vr) {
	return _V_SIZE(vr);
}
static __inline const size_t V_SIZEOF(const struct vm_range *vr) {
    assert((typeof (vr->size))(size_t)_V_SIZE(vr) == _V_SIZE(vr));
    return (size_t)_V_SIZE(vr);
}

/*
 * A range of offsets into a file
 */
struct file_range {
	off_t off;
	off_t size;
};

#define F_OFF(f)		((f)->off)
#define F_SIZE(f)		((f)->size)

struct regionop;
struct subregion;

struct region {
    STAILQ_ENTRY(region) r_linkage;

	struct vm_range r_range;

#define R_RANGE(r)		(&(r)->r_range)
#define _R_ADDR(r)		_V_ADDR(R_RANGE(r))
#define _R_SIZE(r)		_V_SIZE(R_RANGE(r))
#define R_SIZEOF(r)     V_SIZEOF(R_RANGE(r))
#define R_SETADDR(r, a)	V_SETADDR(R_RANGE(r), (a))
#define R_SETSIZE(r, z)	V_SETSIZE(R_RANGE(r), (z))
#define R_ENDADDR(r)	(_R_ADDR(r) + _R_SIZE(r))

    vm_region_submap_info_data_64_t r_info;
    vm_page_info_basic_data_t r_pageinfo;

    int r_purgable;

#ifdef CONFIG_SUBMAP
    int r_depth;
#endif
    boolean_t r_insharedregion, r_inzfodregion, r_incommregion;

    /*
     * This field may be non-NULL if the region is a read-only part
     * of a mapped file (i.e. the shared cache) and thus
     * doesn't need to be copied.
     */
    struct {
        const struct libent *fr_libent;
		const char *fr_pathname;
        off_t fr_offset;
    } *r_fileref;

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
extern const struct regionop fileref_ops;

struct regionhead;

#endif /* _REGION_H */
