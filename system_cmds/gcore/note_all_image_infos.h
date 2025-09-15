/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#ifndef _NOTE_ALL_IMAGE_INFOS_H
#define _NOTE_ALL_IMAGE_INFOS_H

#include <stdint.h>

// This note, its semantics, and how to construct it is documented at:
// https://stashweb.sd.apple.com/users/jmolenda/repos/coreutils/browse

#define NOTE_ALL_IMAGE_INFOS_CURRENT_VERSION    1

struct note_all_image_infos {
    uint32_t version;           // Currently 1
    uint32_t imgcount;          // Number of binary images
    uint64_t entries_fileoff;   // file offset of array of image_entries
    uint32_t entry_size;        // sizeof 'struct image_entry'
    uint32_t unused0;           // set to zero
} __attribute__((__packed__));

struct note_aii_image_entry {
    uint64_t filepath_offset;   // corefile offset of the c-string filepath
                                // if available, else UINT64_MAX
    uuid_t uuid;                // uint8_t[16]. Set to all zeroes if no uuid
    uint64_t load_address;      // UINT64_MAX if unknown
    uint64_t seg_addrs_offset;  // offset to array of 'struct segment_vmaddr's
                                // UINT64_MAX if unknown
    uint32_t segment_count;     // The number of segments for this binary,
                                // 0 if none.
    uint32_t executing;         // Set to 0 if executing status is unknown by
                                // corefile creator.
                                // Set to 1 if this binary was executing on
                                // any thread so it can be force-loaded by the
                                // corefile reader.
                                // Set to 2 if this binary was not executing
                                // on any thread.
} __attribute__((__packed__));

struct note_aii_segment_vmaddr {
    char segname[16];
    uint64_t vmaddr;
    uint64_t unused;
} __attribute__((__packed__));

#endif /* _NOTE_ALL_IMAGE_INFOS_H */
