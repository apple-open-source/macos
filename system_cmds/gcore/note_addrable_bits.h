/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#ifndef _NOTE_ADDRABLE_BITS_H
#define _NOTE_ADDRABLE_BITS_H

#include <stdint.h>

// This note, its semantics, and how to construct it is documented at:
// https://stashweb.sd.apple.com/users/jmolenda/repos/coreutils/browse

/*
 * Specifies how many bits in pointers are actually used for addressing.
 * The bits above these may be used for additional information (tagged
 * pointers, pointer authentication bits), and the debugger may need to
 * enforce an all-0 or all-1 clearing of these bits when computing what
 * the pointers are pointing to.
 */

#define NOTE_ADDRABLE_BITS_CURRENT_VERSION  4

struct note_addrable_bits {
    uint32_t version;                       // Currently 4
    uint32_t low_memory_addressing_bits;    // 64 - T0SZ
    uint32_t high_memory_addressing_bits;   // 64 - T1SZ
    uint32_t reserved0;                     // = zero
} __attribute__((__packed__));
                                        
#endif /* _NOTE_ADDRABLE_BITS_H */
