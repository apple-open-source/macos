/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "region.h"
#include "dyld.h"

#ifndef _SPARSE_H
#define _SPARSE_H

struct subregion {
	struct vm_range s_range;
    native_segment_command_t s_segcmd;
    const struct libent *s_libent;
    bool s_isuuidref;
};

#define S_RANGE(s)	(&(s)->s_range)

static __inline void S_SETADDR(struct subregion *s, mach_vm_offset_t a) {
    V_SETADDR(S_RANGE(s), a);
}

static __inline void S_SETSIZE(struct subregion *s, mach_vm_offset_t sz) {
    V_SETSIZE(S_RANGE(s), sz);
}

static __inline const mach_vm_offset_t S_ADDR(const struct subregion *s) {
    return V_ADDR(S_RANGE(s));
}

static __inline const mach_vm_offset_t S_SIZE(const struct subregion *s) {
    return V_SIZE(S_RANGE(s));
}

static __inline const mach_vm_offset_t S_ENDADDR(const struct subregion *s) {
    return S_ADDR(s) + S_SIZE(s);
}

static __inline const char *S_MACHO_TYPE(const struct subregion *s) {
    return s->s_segcmd.segname;
}

static __inline off_t S_MACHO_FILEOFF(const struct subregion *s) {
     return s->s_segcmd.fileoff;
}

static __inline off_t S_MACHO_FILESIZE(const struct subregion *s) {
    return s->s_segcmd.filesize;
}

static __inline const struct libent *S_LIBENT(const struct subregion *s) {
    return s->s_libent;
}

static __inline const char *S_PATHNAME(const struct subregion *s) {
    const struct libent *le = S_LIBENT(s);
    return le ? le->le_pathname : "(unknown)";
}

static __inline const char *S_FILENAME(const struct subregion *s) {
    const struct libent *le = S_LIBENT(s);
    return le ? le->le_filename : S_PATHNAME(s);
}

extern bool issubregiontype(const struct subregion *, const char *);

extern walk_region_cbfn_t decorate_memory_region;
extern walk_region_cbfn_t undecorate_memory_region;
extern walk_region_cbfn_t sparse_region_optimization;

#endif /* _SPARSE_H */
