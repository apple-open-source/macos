#include "Block_private.h"

#if HAVE_UNWIND

typedef struct Block_layout Block_layout;

// These functions were split out from runtime.cpp to avoid having to link to
// libc++ as doing so would create a circular dependencies between dylibs.
// Compiling this file with -fexceptions requires linking to libunwind and
// libcompiler_rt, both of which are in libSystem.


void _call_copy_helpers_excp(Block_layout *dstbl, Block_layout *srcbl,
                             HelperBaseData *helper) {
    ExcpCleanupInfo __attribute__((cleanup(_cleanup_generic_captures)))
    info = {EXCP_NONE, dstbl, helper};
    // helper is null if generic helpers aren't used.
    if (helper) {
        info.state = EXCP_COPY_GENERIC;
        _call_generic_copy_helper(dstbl, srcbl, helper);
    }
    info.state = EXCP_COPY_CUSTOM;
    _call_custom_copy_helper(dstbl, srcbl);
    info.state = EXCP_NONE;
}

void _call_dispose_helpers_excp(Block_layout *bl, HelperBaseData *helper) {
    ExcpCleanupInfo __attribute__((cleanup(_cleanup_generic_captures)))
    info = {EXCP_DESTROY_CUSTOM, bl, helper};
    _call_custom_dispose_helper(bl);
    // helper is null if generic helpers aren't used.
    if (helper) {
        info.state = EXCP_DESTROY_GENERIC;
        _call_generic_destroy_helper(bl, helper);
    }
    info.state = EXCP_NONE;
}

#endif
