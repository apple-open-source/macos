/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
 */

#ifndef _PORTABLE_REGION_INFOS_H
#define _PORTABLE_REGION_INFOS_H

// Encodes all info returned by the following each submap and vm object in the target:
// * mach_vm_region_recurse(..., VM_REGION_SUBMAP_INFO_COUNT_64),
// * mach_vm_region(..., VM_REGION_BASIC_INFO_64, ...)
// * mach_vm_purgeable_control(..., VM_PURGEABLE_GET_STATE, ...)
// * mach_vm_page_range_query()
struct __attribute__((__packed__)) portable_region_info_t {
    uint64_t vmaddr; // Start vmaddr of submap/vm object.
    uint64_t vmsize; // Size of submap/vm object as returned by mach_vm_region_recurse().

    uint32_t nesting_depth; // As returned by mach_vm_region_recurse().

    // The contents of a vm_region_submap_info_data_64_t struct.
    int32_t  protection;               // present access protection
    int32_t  max_protection;           // max avail through vm_prot
    uint32_t inheritance;              // behavior of map/obj on fork
    uint64_t offset;                   // offset into object/map
    uint32_t user_tag;                 // user tag on map entry
    uint32_t pages_resident;           // only valid for objects
    uint32_t pages_shared_now_private; // only for objects
    uint32_t pages_swapped_out;        // only for objects
    uint32_t pages_dirtied;            // only for objects
    uint32_t ref_count;                // obj/map mappers, etc
    uint16_t shadow_depth;             // only for obj
    uint8_t  external_pager;           // only for obj
    uint8_t  share_mode;               // see enumeration
    uint8_t  is_submap;                // submap vs obj
    int32_t  behavior;                 // access behavior hint
    uint32_t object_id;                // obj/map name, not a handle
    uint16_t user_wired_count;
    uint32_t pages_reusable;
    uint64_t object_id_full;

    // Only set for non-submaps.
    // INT32_MAX if this is a submap, or if mach_vm_purgable_control() does not return KERN_SUCCESS.
    //
    // This is the state returned by mach_vm_purgable_control(..., VM_PURGEABLE_GET_STATE, ...).
    int32_t purgeability;

    // Only set for non-submaps where external_pager is also set.
    // UINT64_MAX if unavailable.
    //
    // Fetch in userspace using proc_regionfilename().
    uint64_t mapped_file_path_offset;

    // Only set for non-submaps.
    //
    // Dispositions should be captured both with the "vm.self_region_footprint" sysctl enabled and
    // disabled.  Symbolication.framework does this in recordRegions() using the utility functions
    // collectPhysFootprint() and setCollectPhysFootprint().
    //
    // Stored as an array of uint16_ts b/c the highest `VM_PAGE_QUERY_PAGE_*` macro value is
    // currently 0x800.
    // This array won't be very large. It only takes 512 KiB to store dispositions for 4 GiB of VM on a
    // system with a 16 KiB page size.
    //
    // Fetch in userspace using mach_vm_page_range_query().
    uint64_t phys_footprint_disposition_count;   // 0 if no dispositions.
    uint64_t phys_footprint_dispositions_offset; // File offset of start of dispositions array.

    uint64_t non_phys_footprint_disposition_count;   // 0 if no dispositions.
    uint64_t non_phys_footprint_dispositions_offset; // File offset of start of dispositions array.
};

#define PORTABLE_REGION_INFOS_CURRENT_MAJOR_VERSION 2
#define PORTABLE_REGION_INFOS_CURRENT_MINOR_VERSION 0

// Store a portable_region_info_t for all submaps and vm objects in the target.
// Saved in an "region infos" LC_NOTE.
struct __attribute__((__packed__)) portable_region_infos_t {
    uint32_t major_version; // Currently 2.
    uint32_t minor_version; // Currently 0.

    // Tail-allocated array of `portable_region_info_t`s.
    uint64_t regions_count;
    struct portable_region_info_t regions[0];
};

#endif /* _PORTABLE_REGION_INFOS_H */
