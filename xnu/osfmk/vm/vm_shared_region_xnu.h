/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef _VM_SHARED_REGION_XNU_H_
#define _VM_SHARED_REGION_XNU_H_

#include <sys/cdefs.h>
#include <vm/vm_shared_region.h>

__BEGIN_DECLS

#ifdef MACH_KERNEL_PRIVATE

#include <kern/queue.h>
#include <vm/vm_object_xnu.h>
#include <vm/memory_object.h>

#define PAGE_SIZE_FOR_SR_SLIDE          4096
#define PAGE_SIZE_FOR_SR_SLIDE_16KB     16384

/*
 * Documentation for the slide info format can be found in the dyld project in
 * the file 'launch-cache/dyld_cache_format.h'.
 */

typedef struct vm_shared_region_slide_info_entry_v1 *vm_shared_region_slide_info_entry_v1_t;
struct vm_shared_region_slide_info_entry_v1 {
	uint32_t        version;
	uint32_t        toc_offset;     // offset from start of header to table-of-contents
	uint32_t        toc_count;      // number of entries in toc (same as number of pages in r/w mapping)
	uint32_t        entry_offset;
	uint32_t        entry_count;
	uint32_t        entries_size;
	// uint16_t	toc[toc_count];
	// entrybitmap	entries[entries_count];
};

#define NUM_SLIDING_BITMAPS_PER_PAGE    (0x1000/sizeof(int)/8) /*128*/
typedef struct slide_info_entry_toc     *slide_info_entry_toc_t;
struct slide_info_entry_toc {
	uint8_t entry[NUM_SLIDING_BITMAPS_PER_PAGE];
};

typedef struct vm_shared_region_slide_info_entry_v2 *vm_shared_region_slide_info_entry_v2_t;
struct vm_shared_region_slide_info_entry_v2 {
	uint32_t        version;
	uint32_t        page_size;
	uint32_t        page_starts_offset;
	uint32_t        page_starts_count;
	uint32_t        page_extras_offset;
	uint32_t        page_extras_count;
	uint64_t        delta_mask;             // which (contiguous) set of bits contains the delta to the next rebase location
	uint64_t        value_add;
	// uint16_t	page_starts[page_starts_count];
	// uint16_t	page_extras[page_extras_count];
};

#define DYLD_CACHE_SLIDE_PAGE_ATTRS             0xC000  // high bits of uint16_t are flags
#define DYLD_CACHE_SLIDE_PAGE_ATTR_EXTRA        0x8000  // index is into extras array (not starts array)
#define DYLD_CACHE_SLIDE_PAGE_ATTR_NO_REBASE    0x4000  // page has no rebasing
#define DYLD_CACHE_SLIDE_PAGE_ATTR_END          0x8000  // last chain entry for page
#define DYLD_CACHE_SLIDE_PAGE_VALUE             0x3FFF  // bitwise negation of DYLD_CACHE_SLIDE_PAGE_ATTRS
#define DYLD_CACHE_SLIDE_PAGE_OFFSET_SHIFT      2

typedef struct vm_shared_region_slide_info_entry_v3 *vm_shared_region_slide_info_entry_v3_t;
struct vm_shared_region_slide_info_entry_v3 {
	uint32_t        version;                        // currently 3
	uint32_t        page_size;                      // currently 4096 (may also be 16384)
	uint32_t        page_starts_count;
	uint64_t        value_add;
	uint16_t        page_starts[] /* page_starts_count */;
};

#define DYLD_CACHE_SLIDE_V3_PAGE_ATTR_NO_REBASE 0xFFFF  // page has no rebasing


typedef struct vm_shared_region_slide_info_entry_v4 *vm_shared_region_slide_info_entry_v4_t;
struct vm_shared_region_slide_info_entry_v4 {
	uint32_t    version;        // currently 4
	uint32_t    page_size;      // currently 4096 (may also be 16384)
	uint32_t    page_starts_offset;
	uint32_t    page_starts_count;
	uint32_t    page_extras_offset;
	uint32_t    page_extras_count;
	uint64_t    delta_mask;    // which (contiguous) set of bits contains the delta to the next rebase location (0xC0000000)
	uint64_t    value_add;     // base address of cache
	// uint16_t    page_starts[page_starts_count];
	// uint16_t    page_extras[page_extras_count];
};

#define DYLD_CACHE_SLIDE4_PAGE_NO_REBASE           0xFFFF  // page has no rebasing
#define DYLD_CACHE_SLIDE4_PAGE_INDEX               0x7FFF  // index into starts or extras
#define DYLD_CACHE_SLIDE4_PAGE_USE_EXTRA           0x8000  // index is into extras array (not starts array)
#define DYLD_CACHE_SLIDE4_PAGE_EXTRA_END           0x8000  // last chain entry for page


typedef struct vm_shared_region_slide_info_entry_v5 *vm_shared_region_slide_info_entry_v5_t;
struct vm_shared_region_slide_info_entry_v5 {
	uint32_t    version;        // currently 5
	uint32_t    page_size;      // 16384
	uint32_t    page_starts_count;
	uint64_t    value_add;
	uint16_t    page_starts[] /* page_starts_count */;
};

#define DYLD_CACHE_SLIDE_V5_PAGE_ATTR_NO_REBASE    0xFFFF    // page has no rebasing


typedef union vm_shared_region_slide_info_entry *vm_shared_region_slide_info_entry_t;
union vm_shared_region_slide_info_entry {
	struct {
		uint32_t version;
		uint32_t page_size; // only valid to use this on version >= 2
	};
	struct vm_shared_region_slide_info_entry_v1 v1;
	struct vm_shared_region_slide_info_entry_v2 v2;
	struct vm_shared_region_slide_info_entry_v3 v3;
	struct vm_shared_region_slide_info_entry_v4 v4;
	struct vm_shared_region_slide_info_entry_v5 v5;
};

#define MIN_SLIDE_INFO_SIZE \
    MIN(sizeof(struct vm_shared_region_slide_info_entry_v1), \
    MIN(sizeof(struct vm_shared_region_slide_info_entry_v2), \
    MIN(sizeof(struct vm_shared_region_slide_info_entry_v3), \
    MIN(sizeof(struct vm_shared_region_slide_info_entry_v4), \
    sizeof(struct vm_shared_region_slide_info_entry_v5)))))

/*
 * This is the information used by the shared cache pager for sub-sections
 * which must be modified for relocations and/or pointer authentications
 * before it can be used. The shared_region_pager gets source pages from
 * the shared cache file and modifies them -- see shared_region_pager_data_request().
 *
 * A single pager may be used from multiple shared regions provided:
 * - same si_slide_object, si_start, si_end, si_slide, si_ptrauth and si_jop_key
 * - The size and contents of si_slide_info_entry are the same.
 */
typedef struct vm_shared_region_slide_info {
	uint32_t                si_slide;           /* the distance that the file data is relocated */
	bool                    si_slid;
#if __has_feature(ptrauth_calls)
	bool                    si_ptrauth;
	uint64_t                si_jop_key;
	struct vm_shared_region *si_shared_region; /* so we can ref/dealloc for authenticated slide info */
#endif /* __has_feature(ptrauth_calls) */
	mach_vm_address_t       si_slid_address __kernel_data_semantics;
	mach_vm_offset_t        si_start __kernel_data_semantics; /* start offset in si_slide_object */
	mach_vm_offset_t        si_end __kernel_data_semantics;
	vm_object_t             si_slide_object;    /* The source object for the pages to be modified */
	mach_vm_size_t          si_slide_info_size; /* size of dyld provided relocation information */
	vm_shared_region_slide_info_entry_t si_slide_info_entry; /* dyld provided relocation information */
} *vm_shared_region_slide_info_t;

/*
 * Data structure that represents a unique shared cache region.
 */
struct vm_shared_region {
	uint32_t                sr_ref_count;
	uint32_t                sr_slide;
	queue_chain_t           sr_q;
	void                    *sr_root_dir;
	cpu_type_t              sr_cpu_type;
	cpu_subtype_t           sr_cpu_subtype;
	ipc_port_t              sr_mem_entry;
	mach_vm_offset_t        sr_first_mapping;
	mach_vm_offset_t        sr_base_address;
	mach_vm_size_t          sr_size;
	mach_vm_offset_t        sr_pmap_nesting_start;
	mach_vm_size_t          sr_pmap_nesting_size;
	thread_call_t           sr_timer_call;
	uuid_t                  sr_uuid;

#if __ARM_MIXED_PAGE_SIZE__
	uint8_t                 sr_page_shift;
#endif /* __ARM_MIXED_PAGE_SIZE__ */
	thread_t                sr_mapping_in_progress; /* sr_first_mapping will be != -1 when done */
	thread_t                sr_slide_in_progress;
	bool                    sr_64bit;
	bool                    sr_persists;
	bool                    sr_uuid_copied;
	bool                    sr_stale;              /* This region should never be used again. */
	bool                    sr_driverkit;

#if __has_feature(ptrauth_calls)
	bool                    sr_reslide;            /* Special shared region for suspected attacked processes */
	uint_t                  sr_num_auth_section;  /* num entries in sr_auth_section */
	uint_t                  sr_next_auth_section; /* used while filling in sr_auth_section */
	vm_shared_region_slide_info_t *sr_auth_section;
#endif /* __has_feature(ptrauth_calls) */

	uint32_t                sr_rsr_version;

	uint64_t                sr_install_time; /* mach_absolute_time() of installation into global list */
	uint32_t                sr_id; /* identifier for shared cache */
	uint32_t                sr_images_count;
	struct dyld_uuid_info_64 *sr_images;
};

extern uint64_t shared_region_find_key(char *shared_region_id);

#endif /* MACH_KERNEL_PRIVATE */

#ifdef XNU_KERNEL_PRIVATE

extern vm_shared_region_t vm_shared_region_get(
	struct task             *task);
extern vm_shared_region_t vm_shared_region_trim_and_get(
	struct task             *task);
extern void vm_shared_region_deallocate(
	struct vm_shared_region *shared_region);
extern void vm_shared_region_set(
	struct task             *task,
	struct vm_shared_region *new_shared_region);
extern kern_return_t vm_shared_region_sliding_valid(uint32_t slide);
extern void vm_commpage_init(void);
extern void vm_commpage_text_init(void);
extern void vm_shared_region_reslide_stale(boolean_t driverkit);
#endif /* XNU_KERNEL_PRIVATE */

__END_DECLS

#endif  /* _VM_SHARED_REGION_XNU_H_ */
