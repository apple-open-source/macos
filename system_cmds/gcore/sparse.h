/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#include "region.h"

#ifndef _SPARSE_H
#define _SPARSE_H

typedef enum subregiontype : uint8_t {
    SR_DIRTY = 1,
    SR_CLEAN = 2,
} subregiontype;

struct subregion {
	struct vm_range s_range;
    enum subregiontype s_type;
};

#define S_RANGE(s)	(&(s)->s_range)

static __inline void
S_SETADDR(struct subregion *s, mach_vm_offset_t a) {
    V_SETADDR(S_RANGE(s), a);
}

static __inline void
S_SETSIZE(struct subregion *s, mach_vm_offset_t sz) {
    V_SETSIZE(S_RANGE(s), sz);
}

static __inline void
S_SETTYPE(struct subregion *s, subregiontype t) {
    assert(t == SR_DIRTY || t == SR_CLEAN);
    s->s_type = t;
}

static __inline const mach_vm_offset_t
S_ADDR(const struct subregion *s) {
    return V_ADDR(S_RANGE(s));
}

static __inline const mach_vm_offset_t
S_SIZE(const struct subregion *s) {
    return V_SIZE(S_RANGE(s));
}

static __inline const mach_vm_offset_t
S_ENDADDR(const struct subregion *s) {
    return S_ADDR(s) + S_SIZE(s);
}

static __inline const subregiontype
S_TYPE(const struct subregion *s) {
    return s->s_type;
}

extern void simple_regionlist_optimization(struct regionhead *);
extern void retain_only_stack(struct regionhead *);
extern void retain_only_modified(struct regionhead *, task_t);

#endif /* _SPARSE_H */
