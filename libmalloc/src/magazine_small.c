/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "internal.h"


/*********************	SMALL FREE LIST UTILITIES	************************/

#pragma mark meta header helpers

/*
 * Mark a block as free.  Only the first quantum of a block is marked thusly,
 * the remainder are marked "middle".
 */
static MALLOC_INLINE void
small_meta_header_set_is_free(msize_t *meta_headers, msize_t index, msize_t msize)
{
	meta_headers[index] = msize | SMALL_IS_FREE;
}

/*
 * Mark a block as not free, preserving its size.
 */
static MALLOC_INLINE void
small_meta_header_set_not_free(msize_t *meta_headers, msize_t index)
{
	meta_headers[index] &= ~SMALL_IS_FREE;
}

/*
 * Mark a block as in use.  Only the first quantum of a block is marked thusly,
 * the remainder are marked "middle".
 */
static MALLOC_INLINE void
small_meta_header_set_in_use(msize_t *meta_headers, msize_t index, msize_t msize)
{
	meta_headers[index] = msize;
}

/*
 * Mark a quantum as being the second or later in a block.
 */
static MALLOC_INLINE void
small_meta_header_set_middle(msize_t *meta_headers, msize_t index)
{
	meta_headers[index] = 0;
}

#pragma mark in-place free list

static MALLOC_INLINE void
small_inplace_checksum_ptr(rack_t *rack, inplace_linkage_s *linkage, void *ptr)
{
	uintptr_t checksum = free_list_gen_checksum((uintptr_t)ptr ^ rack->cookie ^ (uintptr_t)rack);
	linkage->checksum = checksum;
	linkage->ptr = ptr;
}

static MALLOC_INLINE free_list_t
small_inplace_unchecksum_ptr(rack_t *rack, inplace_linkage_s *linkage)
{
	if (linkage->checksum != (uint8_t)free_list_gen_checksum((uintptr_t)linkage->ptr ^ rack->cookie ^ (uintptr_t)rack)) {
		free_list_checksum_botch(rack, linkage, linkage->ptr);
		__builtin_trap();
	}

	return (free_list_t){ .p = linkage->ptr };
}

static MALLOC_INLINE free_list_t
small_inplace_unchecksum_ptr_task(task_t task, memory_reader_t reader,
		print_task_printer_t printer, rack_t *rack, inplace_linkage_s *linkage)
{
	inplace_linkage_s *mapped_linkage;
	rack_t *mapped_rack;
	if (reader(task, (vm_address_t)linkage, sizeof(*linkage),
			(void **)&mapped_linkage)) {
		printer("Unable to map small linkage pointer %p\n", linkage);
		return (free_list_t){ .p = NULL };
	}

	if (reader(task, (vm_address_t)rack,
			sizeof(struct rack_s), (void **)&mapped_rack)) {
		printer("Failed to map small rack\n");
		return (free_list_t){ .p = NULL };
	}

	if (mapped_linkage->checksum != (uint8_t)free_list_gen_checksum(
			(uintptr_t)mapped_linkage->ptr ^ mapped_rack->cookie ^ (uintptr_t)rack)) {
		free_list_checksum_botch(rack, linkage, mapped_linkage->ptr);
		__builtin_trap();
	}

	return (free_list_t){ .p = mapped_linkage->ptr };
}

static MALLOC_INLINE free_list_t
small_inplace_free_entry_get_previous(rack_t *rack, small_inplace_free_entry_t ptr)
{
	return small_inplace_unchecksum_ptr(rack, &ptr->previous);
}

static MALLOC_INLINE void
small_inplace_free_entry_set_previous(rack_t *rack, small_inplace_free_entry_t entry, free_list_t previous)
{
	small_inplace_checksum_ptr(rack, &entry->previous, previous.p);
}

static MALLOC_INLINE free_list_t
small_inplace_free_entry_get_next(rack_t *rack, small_inplace_free_entry_t ptr)
{
	return small_inplace_unchecksum_ptr(rack, &ptr->next);
}

static MALLOC_INLINE free_list_t
small_inplace_free_entry_get_next_task(task_t task, memory_reader_t reader,
		print_task_printer_t printer, rack_t *rack,
		small_inplace_free_entry_t ptr)
{
	return small_inplace_unchecksum_ptr_task(task, reader, printer, rack,
			&ptr->next);
}

static MALLOC_INLINE void
small_inplace_free_entry_set_next(rack_t *rack, small_inplace_free_entry_t entry, free_list_t next)
{
	small_inplace_checksum_ptr(rack, &entry->next, next.p);
}

#pragma mark OOB free list

// Returns true if the address and size of the free list entry would result
// in the free entry being the only data on a given page.
static MALLOC_INLINE boolean_t
small_needs_oob_free_entry(void *ptr, msize_t msize)
{
	return ((trunc_page_quanta((uintptr_t)ptr) == (uintptr_t)ptr) && (SMALL_BYTES_FOR_MSIZE(msize) >= vm_kernel_page_size));
}

// Returns true if the address given lies within the region's OOB free
// list entries, rather than a free_list_t in the region's heap space.
static MALLOC_INLINE boolean_t
small_is_oob_free_entry(free_list_t ptr)
{
	small_region_t region = SMALL_REGION_FOR_PTR(ptr.p);
	return (((uintptr_t)ptr.p >= (uintptr_t)&region->small_oob_free_entries[0]) &&
			((uintptr_t)ptr.p < (uintptr_t)&region->small_oob_free_entries[SMALL_OOB_COUNT]));
}

static MALLOC_INLINE void
small_oob_free_entry_set_previous(oob_free_entry_t oobe, free_list_t previous)
{
	oobe->prev = (uintptr_t)previous.p;
}

static MALLOC_INLINE free_list_t
small_oob_free_entry_get_previous(oob_free_entry_t oobe)
{
	return (free_list_t){ .p = (void *)oobe->prev };
}

static MALLOC_INLINE void
small_oob_free_entry_set_next(oob_free_entry_t oobe, free_list_t next)
{
	oobe->next = (uintptr_t)next.p;
}

static MALLOC_INLINE free_list_t
small_oob_free_entry_get_next(oob_free_entry_t oobe)
{
	return (free_list_t){ .p = (void *)oobe->next };
}

static MALLOC_INLINE free_list_t
small_oob_free_entry_get_next_task(task_t task, memory_reader_t reader,
		print_task_printer_t printer, oob_free_entry_t oobe)
{
	oob_free_entry_t mapped_oobe;
	if (reader(task, (vm_address_t)oobe, sizeof(*oobe),
			(void **)&mapped_oobe)) {
		printer("Failed to map small oobe pointer\n");
		return (free_list_t){ .p = NULL };
	}
	return (free_list_t){ .p = (void *)mapped_oobe->next };
}

static MALLOC_INLINE void *
small_oob_free_entry_get_ptr(oob_free_entry_t oobe)
{
	if (!(oobe->ptr & SMALL_IS_OOB)) {
		return NULL;
	}
	small_region_t region = SMALL_REGION_FOR_PTR(oobe);
	uint16_t block = oobe->ptr & ~SMALL_IS_OOB;
	return (void *)((uintptr_t)region + (block << SHIFT_SMALL_QUANTUM));
}

static MALLOC_INLINE void *
small_oob_free_entry_get_ptr_task(task_t task, memory_reader_t reader,
		print_task_printer_t printer, oob_free_entry_t oobe)
{
	// We need to map the oob_free_entry_t to read the pointer value.
	oob_free_entry_t mapped_oobe;
	if (reader(task, (vm_address_t)oobe, sizeof(*oobe),
			(void **)&mapped_oobe)) {
		printer("Failed to map small oobe pointer\n");
		return NULL;
	}

	if (!(mapped_oobe->ptr & SMALL_IS_OOB)) {
		return NULL;
	}

	// The rest of this code works with target process addresses and returns an
	// address in the target process.
	small_region_t region = SMALL_REGION_FOR_PTR(oobe);
	uint16_t block = mapped_oobe->ptr & ~SMALL_IS_OOB;
	return (void *)((uintptr_t)region + (block << SHIFT_SMALL_QUANTUM));
}

static MALLOC_INLINE void
small_oob_free_entry_set_ptr(oob_free_entry_t oobe, void *ptr)
{
	oobe->ptr = SMALL_IS_OOB | (SMALL_REGION_OFFSET_FOR_PTR(ptr) >> SHIFT_SMALL_QUANTUM);
}

static MALLOC_INLINE void
small_oob_free_entry_set_free(oob_free_entry_t oobe)
{
	oobe->prev = ~0;
	oobe->next = ~0;
	oobe->ptr = 0;
}

// Finds the first unused OOB free list entry in the pointer's region.
// Returns NULL if all of the OOB entries are used.
static MALLOC_INLINE oob_free_entry_t
small_oob_free_find_empty(void *ptr, msize_t msize)
{
	small_region_t region = SMALL_REGION_FOR_PTR(ptr);

	// There are 61 of these entries at the end of a small region.
	// If this changes, then a linear search through the list may
	// become an unsuitable choice.
	for (int i=0; i < SMALL_OOB_COUNT; i++) {
		if (region->small_oob_free_entries[i].ptr == 0) {
			return &region->small_oob_free_entries[i];
		}
	}

#if DEBUG_MALLOC
	malloc_report(ASL_LEVEL_INFO, "used all slots of OOB entries\n");
#endif
	return NULL;
}

static MALLOC_INLINE oob_free_entry_t
small_oob_free_find_ptr(void *ptr, msize_t msize)
{
	small_region_t region = SMALL_REGION_FOR_PTR(ptr);

	// There are 61 of these entries at the end of a small region.
	// If this changes, then a linear search through the list may
	// become an unsuitable choice.
	for (int i=0; i < SMALL_OOB_COUNT; i++) {
		oob_free_entry_t oob = &region->small_oob_free_entries[i];
		if (small_oob_free_entry_get_ptr(oob) == ptr &&
				oob->ptr & SMALL_IS_OOB) {
			return &region->small_oob_free_entries[i];
		}
	}

	return NULL;
}

#pragma mark generic free list

static MALLOC_INLINE void
small_free_list_set_previous(rack_t *rack, free_list_t entry, free_list_t previous)
{
	if (small_is_oob_free_entry(entry)) {
		small_oob_free_entry_set_previous(entry.oob, previous);
	} else {
		small_inplace_free_entry_set_previous(rack, entry.small_inplace, previous);
	}
}

static MALLOC_INLINE free_list_t
small_free_list_get_previous(rack_t *rack, free_list_t ptr)
{
	MALLOC_ASSERT(ptr.p);
	if (small_is_oob_free_entry(ptr)) {
		return small_oob_free_entry_get_previous(ptr.oob);
	} else {
		return small_inplace_free_entry_get_previous(rack, ptr.small_inplace);
	}
}

static MALLOC_INLINE void
small_free_list_set_next(rack_t *rack, free_list_t entry, free_list_t next)
{
	if (small_is_oob_free_entry(entry)) {
		small_oob_free_entry_set_next(entry.oob, next);
	} else {
		small_inplace_free_entry_set_next(rack, entry.small_inplace, next);
	}
}

static MALLOC_INLINE free_list_t
small_free_list_get_next(rack_t *rack, free_list_t ptr)
{
	MALLOC_ASSERT(ptr.p);
	if (small_is_oob_free_entry(ptr)) {
		return small_oob_free_entry_get_next(ptr.oob);
	} else {
		return small_inplace_free_entry_get_next(rack, ptr.small_inplace);
	}
}

static MALLOC_INLINE free_list_t
small_free_list_get_next_task(task_t task, memory_reader_t reader,
		print_task_printer_t printer, rack_t *rack, free_list_t ptr)
{
	MALLOC_ASSERT(ptr.p);
	if (small_is_oob_free_entry(ptr)) {
		return small_oob_free_entry_get_next_task(task, reader, printer, ptr.oob);
	} else {
		return small_inplace_free_entry_get_next_task(task, reader, printer,
				rack, ptr.small_inplace);
	}
}

static MALLOC_INLINE void *
small_free_list_get_ptr(free_list_t ptr)
{
	if (!ptr.p) {
		return NULL;
	} else if (small_is_oob_free_entry(ptr)) {
		return small_oob_free_entry_get_ptr(ptr.oob);
	} else {
		return (void *)ptr.p;
	}
}

static MALLOC_INLINE void *
small_free_list_get_ptr_task(task_t task, memory_reader_t reader,
		print_task_printer_t printer, free_list_t ptr)
{
	if (!ptr.p) {
		return NULL;
	} else if (small_is_oob_free_entry(ptr)) {
		return small_oob_free_entry_get_ptr_task(task, reader, printer, ptr.oob);
	} else {
		return (void *)ptr.p;
	}
}

// Returns a free_list_t that is either inline or not based on the
// pointer and msize.
static MALLOC_INLINE free_list_t
small_free_list_from_ptr(rack_t *rack, void *ptr, msize_t msize)
{
	MALLOC_ASSERT(msize);

	// The default is to put the free_list_t in the memory that
	// the pointer leads to.
	free_list_t entry;
	entry.p = ptr;

	// If the pointer is page aligned, and the msize is greater
	// than a whole page, then we try and put the entry in
	// the out-of-band area instead.
	if (small_needs_oob_free_entry(ptr, msize)) {
		oob_free_entry_t oobe = small_oob_free_find_empty(ptr, msize);
		if (oobe) {
			small_oob_free_entry_set_ptr(oobe, ptr);
			entry.oob = oobe;
		}
	}

	return entry;
}

static MALLOC_INLINE void
small_free_mark_free(rack_t *rack, free_list_t entry, msize_t msize)
{
	// Marks both the start and end block of a free-list entry as free.
	void *ptr = small_free_list_get_ptr(entry);
	msize_t *meta_headers = SMALL_META_HEADER_FOR_PTR(ptr);
	uintptr_t start_index = SMALL_META_INDEX_FOR_PTR(ptr);
	uintptr_t end_index = SMALL_META_INDEX_FOR_PTR(ptr + SMALL_BYTES_FOR_MSIZE(msize) - 1);
	MALLOC_ASSERT(start_index <= end_index);

	small_meta_header_set_is_free(meta_headers, start_index, msize);
	small_meta_header_set_is_free(meta_headers, end_index, msize);
}

static MALLOC_INLINE void
small_free_mark_middle(rack_t *rack, free_list_t entry, msize_t msize)
{
	// Marks both the start and end block of a free-list entry as "middle" (unfree).
	void *ptr = small_free_list_get_ptr(entry);
	msize_t *meta_headers = SMALL_META_HEADER_FOR_PTR(ptr);
	uintptr_t start_index = SMALL_META_INDEX_FOR_PTR(ptr);
	uintptr_t end_index = SMALL_META_INDEX_FOR_PTR(ptr + SMALL_BYTES_FOR_MSIZE(msize) - 1);
	MALLOC_ASSERT(start_index <= end_index);
	MALLOC_ASSERT((meta_headers[start_index] & ~SMALL_IS_FREE) == msize);

	small_meta_header_set_middle(meta_headers, start_index);
	small_meta_header_set_middle(meta_headers, end_index);
}

static MALLOC_INLINE void
small_free_mark_unfree(rack_t *rack, free_list_t entry, msize_t msize)
{
	// Marks both the start and end block of a free-list entry as not free.
	void *ptr = small_free_list_get_ptr(entry);
	msize_t *meta_headers = SMALL_META_HEADER_FOR_PTR(ptr);
	uintptr_t start_index = SMALL_META_INDEX_FOR_PTR(ptr);
	uintptr_t end_index = SMALL_META_INDEX_FOR_PTR(ptr + SMALL_BYTES_FOR_MSIZE(msize) - 1);
	MALLOC_ASSERT(start_index <= end_index);

	small_meta_header_set_not_free(meta_headers, start_index);
	small_meta_header_set_not_free(meta_headers, end_index);
}

static MALLOC_INLINE unsigned int
small_free_list_count(task_t task, memory_reader_t reader,
		print_task_printer_t printer, rack_t *rack, free_list_t ptr)
{
	unsigned int count = 0;
	while (ptr.p) {
		count++;
		ptr = small_free_list_get_next_task(task, reader, printer, rack, ptr);
	}
	return count;
}

/*
 * Adds an item to the proper free list, and also marks the meta-header of the
 * block properly.
 * Assumes szone has been locked
 */
static free_list_t
small_free_list_add_ptr(rack_t *rack, magazine_t *small_mag_ptr, void *ptr, msize_t msize)
{
	grain_t slot = SMALL_FREE_SLOT_FOR_MSIZE(rack, msize);
	free_list_t free_head = small_mag_ptr->mag_free_list[slot];

	// This will either return the free_list_t for the current pointer, or attempt
	// to reserve an OOB entry for us.
	free_list_t free_ptr = small_free_list_from_ptr(rack, ptr, msize);

#if DEBUG_MALLOC
	if (LOG(szone, ptr)) {
		malloc_report(ASL_LEVEL_INFO, "in %s, ptr=%p, msize=%d\n", __FUNCTION__, ptr, msize);
	}
	if (((uintptr_t)ptr) & (SMALL_QUANTUM - 1)) {
		malloc_zone_error(rack->debug_flags, true, "small_free_list_add_ptr: Unaligned ptr %p\n", ptr);
	}
#endif

	small_free_list_set_previous(rack, free_ptr, (free_list_t){ .p = NULL });
	small_free_list_set_next(rack, free_ptr, free_head);

	// Set the start and end blocks of the meta header as "free". Marking the last block
	// allows coalescing the regions when we free adjacent regions.
	small_free_mark_free(rack, free_ptr, msize);

	if (small_free_list_get_ptr(free_head)) {
#if DEBUG_MALLOC
		if (small_free_list_get_previous(szone, free_head)) {
			malloc_zone_error(rack->debug_flags, true, "small_free_list_add_ptr: Internal invariant broken (free_head->previous != NULL)\n"
						"ptr=%p slot=%d free_head=%p previous=%p\n", ptr, slot, (void *)free_head, free_head->previous.p);
		}
		if (!SMALL_PTR_IS_FREE(small_free_list_get_ptr(free_head))) {
			malloc_zone_error(rack->debug_flags, true, "small_free_list_add_ptr: Internal invariant broken (free_head is not a free pointer)\n"
						"ptr=%p slot=%d free_head=%p\n", ptr, slot, (void *)small_free_list_get_ptr(free_head));
		}
#endif
		small_free_list_set_previous(rack, free_head, free_ptr);
	} else {
		BITMAPN_SET(small_mag_ptr->mag_bitmap, slot);
	}

	small_mag_ptr->mag_free_list[slot] = free_ptr;
	return free_ptr;
}

/*
 * Removes the item pointed to by ptr in the proper free list.
 * Assumes szone has been locked
 */
static void
small_free_list_remove_ptr_no_clear(rack_t *rack, magazine_t *small_mag_ptr, free_list_t entry, msize_t msize)
{
	grain_t slot = SMALL_FREE_SLOT_FOR_MSIZE(rack, msize);
	free_list_t next, previous;

#if DEBUG_MALLOC
	if (LOG(szone, ptr)) {
		malloc_report(ASL_LEVEL_INFO, "In %s, ptr=%p, msize=%d\n", __FUNCTION__, ptr, msize);
	}
#endif

	previous = small_free_list_get_previous(rack, entry);
	next = small_free_list_get_next(rack, entry);

	if (!small_free_list_get_ptr(previous)) {
		// The block to remove is the head of the free list
#if DEBUG_MALLOC
		if (small_mag_ptr->mag_free_list[slot] != ptr) {
			malloc_zone_error(rack->debug_flags, true,
					"small_free_list_remove_ptr_no_clear: Internal invariant broken (small_mag_ptr->mag_free_list[slot])\n"
					"ptr=%p slot=%d msize=%d small_mag_ptr->mag_free_list[slot]=%p\n", ptr, slot, msize,
					(void *)small_mag_ptr->mag_free_list[slot]);
			return;
		}
#endif
		small_mag_ptr->mag_free_list[slot] = next;
		if (!small_free_list_get_ptr(next)) {
			BITMAPN_CLR(small_mag_ptr->mag_bitmap, slot);
		}
	} else {
		// Check that the next pointer of "previous" points to "entry".
		free_list_t prev_next = small_free_list_get_next(rack, previous);
		if (small_free_list_get_ptr(prev_next) != small_free_list_get_ptr(entry)) {
			malloc_zone_error(rack->debug_flags, true,
					"small_free_list_remove_ptr_no_clear: Internal invariant broken (next ptr of prev) for %p, prev_next=%p\n",
					small_free_list_get_ptr(entry), small_free_list_get_ptr(prev_next));
			__builtin_unreachable(); // Always crashes in malloc_zone_error().
		}
		small_free_list_set_next(rack, previous, next);
	}

	if (small_free_list_get_ptr(next)) {
		// Check that the previous pointer of "next" points to "entry".
		free_list_t next_prev = small_free_list_get_previous(rack, next);
		if (small_free_list_get_ptr(next_prev) != small_free_list_get_ptr(entry)) {
			malloc_zone_error(rack->debug_flags, true,
					"small_free_list_remove_ptr_no_clear: Internal invariant broken (prev ptr of next) for %p, next_prev=%p\n",
					small_free_list_get_ptr(entry), small_free_list_get_ptr(next_prev));
			__builtin_unreachable(); // Always crashes in malloc_zone_error().
		}
		small_free_list_set_previous(rack, next, previous);
	}

	if (small_is_oob_free_entry(entry)) {
		small_oob_free_entry_set_free(entry.oob);
	}
}

static void
small_free_list_remove_ptr(rack_t *rack, magazine_t *small_mag_ptr, free_list_t entry, msize_t msize)
{
	// In the general case we want to ensure we marked these entries as "middle"
	// while we are in this function. However, when we're moving free list entries
	// from/to the recirc depot we rely on the metadata bits being intact to
	// reconstruct the free list. In that case we have to be able to skip this
	// metadata manipulation.
	small_free_mark_middle(rack, entry, msize);
	small_free_list_remove_ptr_no_clear(rack, small_mag_ptr, entry, msize);
}

// Find a free list entry by its pointer address. This should only really be used
// by small_finalize_region, or similar, where the free_list_t entry of a known
// pointer is desired. Otherwise it is cheaper to always pull off the free lists.
static free_list_t
small_free_list_find_by_ptr(rack_t *rack, magazine_t *small_mag_ptr, void *ptr, msize_t msize)
{
	if (*SMALL_METADATA_FOR_PTR(ptr) == (SMALL_IS_FREE | msize)) {
		// If the block is marked free, and of size `msize`, then we first must check
		// if the alignment+size is such that we could have use an OOB-entry.
		if (small_needs_oob_free_entry(ptr, msize)) {
			// Scan the OOB entries looking for this address.
			small_region_t region = SMALL_REGION_FOR_PTR(ptr);
			for (int i=0; i<SMALL_OOB_COUNT; i++) {
				if (!region->small_oob_free_entries[i].ptr) {
					continue;
				}
				if (small_oob_free_entry_get_ptr(&region->small_oob_free_entries[i]) == ptr) {
					return (free_list_t){ .oob = &region->small_oob_free_entries[i] };
				}
			}
		}

		// Otherwise, the freed pointer will be in place.
		return (free_list_t){ .p = ptr };
	}

	malloc_zone_error(rack->debug_flags, true,
			"small_free_list_find_by_ptr: ptr is not free (ptr metadata !SMALL_IS_FREE), "
			"ptr=%p msize=%d metadata=0x%x\n", ptr, msize, *SMALL_METADATA_FOR_PTR(ptr));
	__builtin_trap();
}

void
small_finalize_region(rack_t *rack, magazine_t *small_mag_ptr)
{
	void *last_block, *previous_block;
	msize_t last_msize, previous_msize, last_index;
	free_list_t previous;

	// It is possible that the block prior to the last block in the region has
	// been free'd, but was not coalesced with the free bytes at the end of the
	// block, since we treat the bytes at the end of the region as "in use" in
	// the meta headers. Attempt to coalesce the last block with the previous
	// block, so we don't violate the "no consecutive free blocks" invariant.
	//
	// FIXME: If we could calculate the previous small free size in the same
	//        manner as tiny_previous_preceding_free, it would eliminate the
	//        index & previous msize checks, which are a guard against reading
	//        bogus data out of in-use or written-on-freed memory.
	//
	// FIXME: Need to investigate how much work would be required to increase
	//        'mag_bytes_free_at_end' when freeing the preceding block, rather
	//        than performing this workaround.
	//
	if (small_mag_ptr->mag_bytes_free_at_end) {
		last_block = SMALL_REGION_HEAP_END(small_mag_ptr->mag_last_region) - small_mag_ptr->mag_bytes_free_at_end;
		last_msize = SMALL_MSIZE_FOR_BYTES(small_mag_ptr->mag_bytes_free_at_end);

		last_index = SMALL_META_INDEX_FOR_PTR(last_block);
		previous_msize = SMALL_PREVIOUS_MSIZE(last_block);

		if (last_index && (previous_msize <= last_index)) {
			previous_block = (void *)((uintptr_t)last_block - SMALL_BYTES_FOR_MSIZE(previous_msize));

			if (SMALL_PTR_IS_FREE(previous_block)) {
				previous = small_free_list_find_by_ptr(rack, small_mag_ptr, previous_block, previous_msize);
				small_free_list_remove_ptr(rack, small_mag_ptr, previous, previous_msize);
				last_block = previous_block;
				last_msize += previous_msize;
			}
		}

		// splice last_block into the free list
		small_free_list_add_ptr(rack, small_mag_ptr, last_block, last_msize);
		small_mag_ptr->mag_bytes_free_at_end = 0;
	}

#if CONFIG_ASLR_INTERNAL
	free_list_t next;

	if (small_mag_ptr->mag_bytes_free_at_start) {
		last_block = SMALL_REGION_HEAP_BASE(small_mag_ptr->mag_last_region);
		last_msize = SMALL_MSIZE_FOR_BYTES(small_mag_ptr->mag_bytes_free_at_start);

		void *next_block = (void *)((uintptr_t)last_block + small_mag_ptr->mag_bytes_free_at_start);
		if (SMALL_PTR_IS_FREE(next_block)) {
			msize_t next_msize = SMALL_PTR_SIZE(next_block);
			next = small_free_list_find_by_ptr(rack, small_mag_ptr, next_block, next_msize);
			small_free_list_remove_ptr(rack, small_mag_ptr, next, next_msize);
			last_msize += next_msize;
		}

		// splice last_block into the free list
		small_free_list_add_ptr(rack, small_mag_ptr, last_block, last_msize);
		small_mag_ptr->mag_bytes_free_at_start = 0;
	}
#endif

	// TODO: Will we ever need to coalesce the blocks at the beginning and end when we finalize?
	small_mag_ptr->mag_last_region = NULL;
}

int
small_free_detach_region(rack_t *rack, magazine_t *small_mag_ptr, region_t r)
{
	uintptr_t start = (uintptr_t)SMALL_REGION_HEAP_BASE(r);
	uintptr_t current = start;
	uintptr_t limit = (uintptr_t)SMALL_REGION_HEAP_END(r);
	int total_alloc = 0;
	msize_t *meta_headers = SMALL_META_HEADER_FOR_PTR(start);

	while (current < limit) {
		unsigned index = SMALL_META_INDEX_FOR_PTR(current);
		msize_t msize_and_free = meta_headers[index];
		boolean_t is_free = msize_and_free & SMALL_IS_FREE;
		msize_t msize = msize_and_free & ~SMALL_IS_FREE;

		if (!msize) {
#if DEBUG_MALLOC
			boolean_t is_free = msize_and_free & SMALL_IS_FREE;
			malloc_report(ASL_LEVEL_ERR, "*** small_free_detach_region error with %p: msize=%d is_free=%d\n", (void *)current, msize, is_free);
#endif
			break;
		}

		if (is_free) {
			free_list_t entry = small_free_list_find_by_ptr(rack, small_mag_ptr, (void *)current, msize);
			small_free_list_remove_ptr_no_clear(rack, small_mag_ptr, entry, msize);
		} else {
			total_alloc++;
		}
		current += SMALL_BYTES_FOR_MSIZE(msize);
	}
	return total_alloc;
}

size_t
small_free_reattach_region(rack_t *rack, magazine_t *small_mag_ptr, region_t r)
{
	uintptr_t start = (uintptr_t)SMALL_REGION_HEAP_BASE(r);
	uintptr_t current = start;
	uintptr_t limit = (uintptr_t)SMALL_REGION_HEAP_END(r);
	size_t total_alloc = 0;
	msize_t *meta_headers = SMALL_META_HEADER_FOR_PTR(start);

	while (current < limit) {
		unsigned index = SMALL_META_INDEX_FOR_PTR(current);
		msize_t msize_and_free = meta_headers[index];
		boolean_t is_free = msize_and_free & SMALL_IS_FREE;
		msize_t msize = msize_and_free & ~SMALL_IS_FREE;

		if (!msize) {
#if DEBUG_MALLOC
			malloc_report(ASL_LEVEL_ERR, "*** small_free_reattach_region error with %p: msize=%d is_free=%d\n", (void *)current, msize, is_free);
#endif
			break;
		}
		if (is_free) {
			small_free_list_add_ptr(rack, small_mag_ptr, (void *)current, msize);
		} else {
			total_alloc += SMALL_BYTES_FOR_MSIZE(msize);
		}
		current += SMALL_BYTES_FOR_MSIZE(msize);
	}
	return total_alloc;
}

typedef struct {
	uint16_t pnum, size;
} small_pg_pair_t;

void
small_free_scan_madvise_free(rack_t *rack, magazine_t *depot_ptr, region_t r)
{
	uintptr_t start = (uintptr_t)SMALL_REGION_HEAP_BASE(r);
	uintptr_t current = start;
	uintptr_t limit = (uintptr_t)SMALL_REGION_HEAP_END(r);
	msize_t *meta_headers = SMALL_META_HEADER_FOR_PTR(start);
	small_pg_pair_t advisory[((SMALL_HEAP_SIZE + vm_kernel_page_size - 1) >> vm_kernel_page_shift) >>
							 1]; // 4096bytes stack allocated
	int advisories = 0;

	// Scan the metadata identifying blocks which span one or more pages. Mark the pages MADV_FREE taking care to preserve free list
	// management data.
	while (current < limit) {
		unsigned index = SMALL_META_INDEX_FOR_PTR(current);
		msize_t msize_and_free = meta_headers[index];
		boolean_t is_free = msize_and_free & SMALL_IS_FREE;
		msize_t msize = msize_and_free & ~SMALL_IS_FREE;

		if (is_free && !msize && (current == start)) {
#if DEBUG_MALLOC
			// first block is all free
			malloc_report(ASL_LEVEL_ERR, "*** small_free_scan_madvise_free first block is all free! %p: msize=%d is_free=%d\n", (void *)current,
						  msize, is_free);
#endif
			uintptr_t pgLo = round_page_kernel(start + sizeof(free_list_t) + sizeof(msize_t));
			uintptr_t pgHi = trunc_page_kernel(start + SMALL_REGION_SIZE - sizeof(msize_t));

			if (pgLo < pgHi) {
				advisory[advisories].pnum = (pgLo - start) >> vm_kernel_page_shift;
				advisory[advisories].size = (pgHi - pgLo) >> vm_kernel_page_shift;
				advisories++;
			}
			break;
		}
		if (!msize) {
#if DEBUG_MALLOC
			malloc_report(ASL_LEVEL_ERR,
						  "*** small_free_scan_madvise_free error with %p: msize=%d is_free=%d\n", (void *)current, msize, is_free);
#endif
			break;
		}
		if (is_free) {
			uintptr_t pgLo = round_page_kernel(current + sizeof(free_list_t) + sizeof(msize_t));
			uintptr_t pgHi = trunc_page_kernel(current + SMALL_BYTES_FOR_MSIZE(msize) - sizeof(msize_t));

			if (pgLo < pgHi) {
				advisory[advisories].pnum = (pgLo - (uintptr_t)r) >> vm_kernel_page_shift;
				advisory[advisories].size = (pgHi - pgLo) >> vm_kernel_page_shift;
				advisories++;
			}
		}
		current += SMALL_BYTES_FOR_MSIZE(msize);
	}

	if (advisories > 0) {
		int i;

		OSAtomicIncrement32Barrier(&(REGION_TRAILER_FOR_SMALL_REGION(r)->pinned_to_depot));
		SZONE_MAGAZINE_PTR_UNLOCK(depot_ptr);
		for (i = 0; i < advisories; ++i) {
			uintptr_t addr = (advisory[i].pnum << vm_kernel_page_shift) + (uintptr_t)r;
			size_t size = advisory[i].size << vm_kernel_page_shift;

			mvm_madvise_free(rack, r, addr, addr + size, NULL, rack->debug_flags & MALLOC_DO_SCRIBBLE);
		}
		SZONE_MAGAZINE_PTR_LOCK(depot_ptr);
		OSAtomicDecrement32Barrier(&(REGION_TRAILER_FOR_SMALL_REGION(r)->pinned_to_depot));
	}
}

#if CONFIG_RECIRC_DEPOT
static region_t
small_find_msize_region(rack_t *rack, magazine_t *small_mag_ptr, mag_index_t mag_index, msize_t msize)
{
	void *ptr;
	grain_t slot = SMALL_FREE_SLOT_FOR_MSIZE(rack, msize);
	free_list_t *free_list = small_mag_ptr->mag_free_list;
	free_list_t *the_slot = free_list + slot;
	free_list_t *limit;
	unsigned bitmap;

	// Assumes we've locked the magazine
	CHECK_MAGAZINE_PTR_LOCKED(szone, small_mag_ptr, __PRETTY_FUNCTION__);

	// Look for an exact match by checking the freelist for this msize.
	ptr = small_free_list_get_ptr(*the_slot);
	if (ptr) {
		return SMALL_REGION_FOR_PTR(ptr);
	}

	// Mask off the bits representing slots holding free blocks smaller than
	// the size we need.
	//
	// BITMAPN_CTZ implementation
	unsigned idx = slot >> 5;
	bitmap = 0;
	unsigned mask = ~((1 << (slot & 31)) - 1);
	for (; idx < SMALL_FREELIST_BITMAP_WORDS(rack); ++idx) {
		bitmap = small_mag_ptr->mag_bitmap[idx] & mask;
		if (bitmap != 0) {
			break;
		}
		mask = ~0U;
	}
	// Check for fallthrough: No bits set in bitmap
	if ((bitmap == 0) && (idx == SMALL_FREELIST_BITMAP_WORDS(rack))) {
		return NULL;
	}

	// Start looking at the first set bit, plus 32 bits for every word of
	// zeroes or entries that were too small.
	slot = BITMAP32_CTZ((&bitmap)) + (idx * 32);

	limit = free_list + SMALL_FREE_SLOT_COUNT(rack) - 1;
	free_list += slot;

	if (free_list < limit) {
		ptr = small_free_list_get_ptr(*free_list);
		if (ptr) {
			return SMALL_REGION_FOR_PTR(ptr);
		} else {
			/* Shouldn't happen. Fall through to look at last slot. */
#if DEBUG_MALLOC
			malloc_report(ASL_LEVEL_ERR, "in small_malloc_from_free_list(), mag_bitmap out of sync, slot=%d\n", slot);
#endif
		}
	}

	// We are now looking at the last slot, which contains blocks equal to, or
	// due to coalescing of free blocks, larger than (num_small_slots - 1) * (small quantum size).
	ptr = small_free_list_get_ptr(*limit);
	if (ptr) {
		return SMALL_REGION_FOR_PTR(ptr);
	}

	return NULL;
}

static boolean_t
small_get_region_from_depot(rack_t *rack, magazine_t *small_mag_ptr, mag_index_t mag_index, msize_t msize)
{
	magazine_t *depot_ptr = &(rack->magazines[DEPOT_MAGAZINE_INDEX]);

#if DEBUG_MALLOC
	if (DEPOT_MAGAZINE_INDEX == mag_index) {
		malloc_zone_error(rack->debug_flags, true, "small_get_region_from_depot called for magazine index -1\n", NULL, NULL);
		return 0;
	}
#endif

	SZONE_MAGAZINE_PTR_LOCK(depot_ptr);

	// Appropriate a Depot'd region that can satisfy requested msize.
	region_trailer_t *node;
	region_t sparse_region;
	msize_t try_msize = msize;

	while (1) {
		sparse_region = small_find_msize_region(rack, depot_ptr, DEPOT_MAGAZINE_INDEX, try_msize);
		if (NULL == sparse_region) { // Depot empty?
			SZONE_MAGAZINE_PTR_UNLOCK(depot_ptr);
			return 0;
		}

		node = REGION_TRAILER_FOR_SMALL_REGION(sparse_region);
		if (0 == node->pinned_to_depot) {
			// Found one!
			break;
		}

		// Try the next msize up - maybe the head of its free list will be in
		// a region we can use. Once we get the region we'll still allocate the
		// original msize.
		try_msize++;

		if (try_msize > NUM_SMALL_SLOTS) {
			// Tried all the msizes but couldn't get a usable region. Let's
			// give up for now and we'll allocate a new region from the kernel.
			SZONE_MAGAZINE_PTR_UNLOCK(depot_ptr);
			return 0;
		}
	}

	// disconnect node from Depot
	recirc_list_extract(rack, depot_ptr, node);

	// Iterate the region pulling its free entries off the (locked) Depot's free list
	int objects_in_use = small_free_detach_region(rack, depot_ptr, sparse_region);

	// Transfer ownership of the region
	MAGAZINE_INDEX_FOR_SMALL_REGION(sparse_region) = mag_index;
	MALLOC_ASSERT(node->pinned_to_depot == 0);

	// Iterate the region putting its free entries on its new (locked) magazine's free list
	size_t bytes_inplay = small_free_reattach_region(rack, small_mag_ptr, sparse_region);

	depot_ptr->mag_num_bytes_in_objects -= bytes_inplay;
	depot_ptr->num_bytes_in_magazine -= SMALL_HEAP_SIZE;
	depot_ptr->mag_num_objects -= objects_in_use;

	small_mag_ptr->mag_num_bytes_in_objects += bytes_inplay;
	small_mag_ptr->num_bytes_in_magazine += SMALL_HEAP_SIZE;
	small_mag_ptr->mag_num_objects += objects_in_use;

	// connect to magazine as last node
	recirc_list_splice_last(rack, small_mag_ptr, node);

	SZONE_MAGAZINE_PTR_UNLOCK(depot_ptr);

	MAGMALLOC_DEPOTREGION(SMALL_SZONE_FROM_RACK(rack), (int)mag_index, (void *)sparse_region, SMALL_REGION_SIZE,
						  (int)BYTES_USED_FOR_SMALL_REGION(sparse_region)); // DTrace USDT Probe

	return 1;
}
#endif // CONFIG_RECIRC_DEPOT

#if CONFIG_MADVISE_PRESSURE_RELIEF
void
small_madvise_pressure_relief(rack_t *rack)
{
	mag_index_t mag_index;
	magazine_t *small_depot_ptr = &rack->magazines[DEPOT_MAGAZINE_INDEX];

	for (mag_index = 0; mag_index < rack->num_magazines; mag_index++) {
		size_t index;
		for (index = 0; index < rack->region_generation->num_regions_allocated; ++index) {
			rack_region_lock(rack);

			region_t small = rack->region_generation->hashed_regions[index];
			if (!small || small == HASHRING_REGION_DEALLOCATED) {
				rack_region_unlock(rack);
				continue;
			}

			region_trailer_t *trailer = REGION_TRAILER_FOR_SMALL_REGION(small);
			// Make sure that the owning magazine doesn't try and take this out
			// from under our feet.
			trailer->dispose_flags |= RACK_DISPOSE_DELAY;
			rack_region_unlock(rack);

			magazine_t *mag_ptr = mag_lock_zine_for_region_trailer(rack->magazines,
					trailer, MAGAZINE_INDEX_FOR_SMALL_REGION(small));

			// If acquiring the region lock was enough to prevent the owning
			// magazine from deallocating the region, free it now so we don't
			// do wasted work.
			if (rack_region_maybe_dispose(rack, small, SMALL_REGION_SIZE, trailer)) {
				SZONE_MAGAZINE_PTR_UNLOCK(mag_ptr);
				continue;
			}

			/* Ordering is important here, the magazine of a region may potentially change
			 * during mag_lock_zine_for_region_trailer, so src_mag_index must be taken
			 * after we've obtained the lock.
			 */
			mag_index_t src_mag_index = MAGAZINE_INDEX_FOR_SMALL_REGION(small);

			/* We can (and must) ignore magazines that are already in the recirc depot. */
			if (src_mag_index == DEPOT_MAGAZINE_INDEX) {
				SZONE_MAGAZINE_PTR_UNLOCK(mag_ptr);
				continue;
			}

			if (REGION_TRAILER_FOR_SMALL_REGION(small)->pinned_to_depot > 0) {
				SZONE_MAGAZINE_PTR_UNLOCK(mag_ptr);
				continue;
			}

			if (small == mag_ptr->mag_last_region && (mag_ptr->mag_bytes_free_at_end || mag_ptr->mag_bytes_free_at_start)) {
				small_finalize_region(rack, mag_ptr);
			}

			/* Because this region is currently in use, we can't safely madvise it while
			 * it's attached to the magazine. For this operation we have to remove it from
			 * the current mag, attach it to the depot and then madvise.
			 */

			recirc_list_extract(rack, mag_ptr, REGION_TRAILER_FOR_SMALL_REGION(small));
			int objects_in_use = small_free_detach_region(rack, mag_ptr, small);

			SZONE_MAGAZINE_PTR_LOCK(small_depot_ptr);
			MAGAZINE_INDEX_FOR_SMALL_REGION(small) = DEPOT_MAGAZINE_INDEX;
			MALLOC_ASSERT(REGION_TRAILER_FOR_SMALL_REGION(small)->pinned_to_depot == 0);

			size_t bytes_inplay = small_free_reattach_region(rack, small_depot_ptr, small);

			/* Fix up the metadata of the target magazine while the region is in the depot. */
			mag_ptr->mag_num_bytes_in_objects -= bytes_inplay;
			mag_ptr->num_bytes_in_magazine -= SMALL_HEAP_SIZE;
			mag_ptr->mag_num_objects -= objects_in_use;

			/* Now we can drop the magazine lock of the source mag. */
			SZONE_MAGAZINE_PTR_UNLOCK(mag_ptr);

			small_depot_ptr->mag_num_bytes_in_objects += bytes_inplay;
			small_depot_ptr->num_bytes_in_magazine += SMALL_HEAP_SIZE;
			small_depot_ptr->mag_num_objects -= objects_in_use;

			recirc_list_splice_last(rack, small_depot_ptr, REGION_TRAILER_FOR_SMALL_REGION(small));

			/* Actually do the scan, done holding the depot lock, the call will drop the lock
			 * around the actual madvise syscalls.
			 */
			small_free_scan_madvise_free(rack, small_depot_ptr, small);

			/* Now the region is in the recirc depot, the next allocations to require more
			 * blocks will come along and take one of these regions back out of the depot.
			 * As OS X madvise's reuse on an per-region basis, we leave as many of these
			 * regions in the depot as possible after memory pressure.
			 */
			SZONE_MAGAZINE_PTR_UNLOCK(small_depot_ptr);
		}
	}
}
#endif // CONFIG_MADVISE_PRESSURE_RELIEF

#if CONFIG_AGGRESSIVE_MADVISE || CONFIG_RECIRC_DEPOT
static MALLOC_INLINE void
small_madvise_free_range_no_lock(rack_t *rack,
								 magazine_t *small_mag_ptr,
								 region_t region,
								 free_list_t freee,
								 msize_t fmsize,
								 void *headptr,
								 size_t headsize)
{
	void *ptr = small_free_list_get_ptr(freee);
	region_trailer_t *node = REGION_TRAILER_FOR_SMALL_REGION(region);

	// Lock on small_magazines[mag_index] is already held here.
	// Calculate the first page in the coalesced block that would be safe to mark MADV_FREE
	size_t free_header_size = sizeof(free_list_t) + sizeof(msize_t);

	// If the free_list_t entry is out-of-line then we don't need to reserve any space
	// at the start of the region.
	if (small_is_oob_free_entry(freee)) {
		free_header_size = 0;
	}

	uintptr_t safe_ptr = (uintptr_t)ptr + free_header_size;
	uintptr_t round_safe = round_page_kernel(safe_ptr);

	// Calculate the last page in the coalesced block that would be safe to mark MADV_FREE
	uintptr_t safe_extent = (uintptr_t)ptr + SMALL_BYTES_FOR_MSIZE(fmsize);
	uintptr_t trunc_extent = trunc_page_kernel(safe_extent);

	// The newly freed block may complete a span of bytes that cover one or more pages. Mark the span with MADV_FREE.
	if (round_safe < trunc_extent) { // Coalesced area covers a page (perhaps many)
		// Extend the freed block by the free region header and tail sizes to include pages
		// we may have coalesced that no longer host free region tails and headers.
		// This may extend over in-use ranges, but the MIN/MAX clamping below will fix that up.
		uintptr_t lo = trunc_page_kernel((uintptr_t)headptr);
		uintptr_t hi = round_page_kernel((uintptr_t)headptr + headsize + free_header_size);

		uintptr_t free_lo = MAX(round_safe, lo);
		uintptr_t free_hi = MIN(trunc_extent, hi);

		if (free_lo < free_hi) {
			// Before unlocking, ensure that the metadata for the freed region
			// makes it look not free but includes the length. This ensures that
			// any code that inspects the metadata while we are unlocked sees
			// a valid state and will not try to use or coalesce freed memory
			// into it.
			small_free_mark_unfree(rack, freee, fmsize);
			small_free_list_remove_ptr_no_clear(rack, small_mag_ptr, freee, fmsize);
			OSAtomicIncrement32Barrier(&(node->pinned_to_depot));
			SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
			mvm_madvise_free(rack, region, free_lo, free_hi, &rack->last_madvise, rack->debug_flags & MALLOC_DO_SCRIBBLE);
			SZONE_MAGAZINE_PTR_LOCK(small_mag_ptr);
			OSAtomicDecrement32Barrier(&(node->pinned_to_depot));
			small_free_list_add_ptr(rack, small_mag_ptr, ptr, fmsize);
		}
	}
}
#endif // CONFIG_AGGRESSIVE_MADVISE || CONFIG_RECIRC_DEPOT

#if CONFIG_RECIRC_DEPOT
static region_t
small_free_try_depot_unmap_no_lock(rack_t *rack, magazine_t *depot_ptr, region_trailer_t *node)
{
	if (0 < node->bytes_used || 0 < node->pinned_to_depot || depot_ptr->recirculation_entries < recirc_retained_regions) {
		return NULL;
	}

	// disconnect first node from Depot
	recirc_list_extract(rack, depot_ptr, node);

	// Iterate the region pulling its free entries off the (locked) Depot's free list
	region_t sparse_region = SMALL_REGION_FOR_PTR(node);
	int objects_in_use = small_free_detach_region(rack, depot_ptr, sparse_region);

	if (0 == objects_in_use) {
		if (!rack_region_remove(rack, sparse_region, node)) {
			return NULL;
		}
		depot_ptr->num_bytes_in_magazine -= SMALL_HEAP_SIZE;

		// Caller will transfer ownership of the region back to the OS with no locks held
		MAGMALLOC_DEALLOCREGION(SMALL_SZONE_FROM_RACK(rack), (void *)sparse_region, (int)SMALL_REGION_SIZE); // DTrace USDT Probe
		return sparse_region;

	} else {
		malloc_zone_error(rack->debug_flags, true,
				"small_free_try_depot_unmap_no_lock objects_in_use not zero: %d\n", objects_in_use);
		return NULL;
	}
}

static boolean_t
small_free_do_recirc_to_depot(rack_t *rack, magazine_t *small_mag_ptr, mag_index_t mag_index)
{
	// The entire magazine crossed the "emptiness threshold". Transfer a region
	// from this magazine to the Depot. Choose a region that itself has crossed the emptiness threshold (i.e
	// is at least fraction "f" empty.) Such a region will be marked "suitable" on the recirculation list.
	region_trailer_t *node = small_mag_ptr->firstNode;

	while (node && (!node->recirc_suitable || node->pinned_to_depot)) {
		node = node->next;
	}

	if (NULL == node) {
#if DEBUG_MALLOC
		malloc_report(ASL_LEVEL_ERR, "*** small_free_do_recirc_to_depot end of list\n");
#endif
		return TRUE; // Caller must SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
	}

	region_t sparse_region = SMALL_REGION_FOR_PTR(node);

	// Deal with unclaimed memory -- mag_bytes_free_at_end or mag_bytes_free_at start
	if (sparse_region == small_mag_ptr->mag_last_region &&
		(small_mag_ptr->mag_bytes_free_at_end || small_mag_ptr->mag_bytes_free_at_start)) {
		small_finalize_region(rack, small_mag_ptr);
	}

	// disconnect "suitable" node from magazine
	recirc_list_extract(rack, small_mag_ptr, node);

	// Iterate the region pulling its free entries off its (locked) magazine's free list
	int objects_in_use = small_free_detach_region(rack, small_mag_ptr, sparse_region);
	magazine_t *depot_ptr = &(rack->magazines[DEPOT_MAGAZINE_INDEX]);

	// hand over the region to the (locked) Depot
	SZONE_MAGAZINE_PTR_LOCK(depot_ptr);
	// this will cause small_free_list_add_ptr called by small_free_reattach_region to use
	// the depot as its target magazine, rather than magazine formerly associated with sparse_region
	MAGAZINE_INDEX_FOR_SMALL_REGION(sparse_region) = DEPOT_MAGAZINE_INDEX;
	node->pinned_to_depot = 0;

	// Iterate the region putting its free entries on Depot's free list
	size_t bytes_inplay = small_free_reattach_region(rack, depot_ptr, sparse_region);

	small_mag_ptr->mag_num_bytes_in_objects -= bytes_inplay;
	small_mag_ptr->num_bytes_in_magazine -= SMALL_HEAP_SIZE;
	small_mag_ptr->mag_num_objects -= objects_in_use;

	SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr); // Unlock the originating magazine

	depot_ptr->mag_num_bytes_in_objects += bytes_inplay;
	depot_ptr->num_bytes_in_magazine += SMALL_HEAP_SIZE;
	depot_ptr->mag_num_objects += objects_in_use;

	// connect to Depot as last node
	recirc_list_splice_last(rack, depot_ptr, node);

	MAGMALLOC_RECIRCREGION(SMALL_SZONE_FROM_RACK(rack), (int)mag_index, (void *)sparse_region, SMALL_REGION_SIZE,
						   (int)BYTES_USED_FOR_SMALL_REGION(sparse_region)); // DTrace USDT Probe

#if CONFIG_AGGRESSIVE_MADVISE
	if (!aggressive_madvise_enabled)
#endif
	{
		// Mark free'd dirty pages with MADV_FREE to reduce memory pressure
		small_free_scan_madvise_free(rack, depot_ptr, sparse_region);
	}

	// If the region is entirely empty vm_deallocate() it outside the depot lock
	region_t r_dealloc = small_free_try_depot_unmap_no_lock(rack, depot_ptr, node);
	SZONE_MAGAZINE_PTR_UNLOCK(depot_ptr);
	if (r_dealloc) {
		mvm_deallocate_pages(r_dealloc, SMALL_REGION_SIZE,
				MALLOC_FIX_GUARD_PAGE_FLAGS(rack->debug_flags));
	}
	return FALSE; // Caller need not unlock the originating magazine
}

static MALLOC_INLINE boolean_t
small_free_try_recirc_to_depot(rack_t *rack,
							   magazine_t *small_mag_ptr,
							   mag_index_t mag_index,
							   region_t region,
							   free_list_t freee,
							   msize_t msize,
							   void *headptr,
							   size_t headsize)
{
	region_trailer_t *node = REGION_TRAILER_FOR_SMALL_REGION(region);
	size_t bytes_used = node->bytes_used;

	if (DEPOT_MAGAZINE_INDEX != mag_index) {
		// Emptiness discriminant
		if (small_region_below_recirc_threshold(region)) {
			/* Region has crossed threshold from density to sparsity. Mark it "suitable" on the
			 * recirculation candidates list. */
			node->recirc_suitable = TRUE;
		} else {
			/* After this free, we've found the region is still dense, so it must have been even more so before
			 * the free. That implies the region is already correctly marked. Do nothing. */
		}

		// Has the entire magazine crossed the "emptiness threshold"? If so, transfer a region
		// from this magazine to the Depot. Choose a region that itself has crossed the emptiness threshold (i.e
		// is at least fraction "f" empty.) Such a region will be marked "suitable" on the recirculation list.
		if (small_magazine_below_recirc_threshold(small_mag_ptr)) {
			return small_free_do_recirc_to_depot(rack, small_mag_ptr, mag_index);
		}
	} else {
#if CONFIG_AGGRESSIVE_MADVISE
		if (!aggressive_madvise_enabled)
#endif
		{
			// We are free'ing into the depot, so madvise as we do so unless we were madvising every incoming
			// allocation anyway.
			small_madvise_free_range_no_lock(rack, small_mag_ptr, region, freee, msize, headptr, headsize);
		}

		if (0 < bytes_used || 0 < node->pinned_to_depot) {
			/* Depot'd region is still live. Leave it in place on the Depot's recirculation list
			 * so as to avoid thrashing between the Depot's free list and a magazines's free list
			 * with detach_region/reattach_region */
		} else {
			/* Depot'd region is just now empty. Consider return to OS. */
			region_t r_dealloc = small_free_try_depot_unmap_no_lock(rack, small_mag_ptr, node);
			SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
			if (r_dealloc) {
				mvm_deallocate_pages(r_dealloc, SMALL_REGION_SIZE, MALLOC_FIX_GUARD_PAGE_FLAGS(rack->debug_flags));
			}
			return FALSE; // Caller need not unlock
		}
	}
	return TRUE; // Caller must do SZONE_MAGAZINE_PTR_UNLOCK(tiny_mag_ptr)
}
#endif // CONFIG_RECIRC_DEPOT

static MALLOC_INLINE boolean_t
small_free_no_lock(rack_t *rack, magazine_t *small_mag_ptr, mag_index_t mag_index, region_t region, void *ptr, msize_t msize)
{
	msize_t *meta_headers = SMALL_META_HEADER_FOR_PTR(ptr);
	unsigned index = SMALL_META_INDEX_FOR_PTR(ptr);
	size_t original_size = SMALL_BYTES_FOR_MSIZE(msize);
	void *next_block = ptr + original_size;
	msize_t next_index = index + msize;

	MALLOC_TRACE(TRACE_small_free, (uintptr_t)rack, (uintptr_t)small_mag_ptr, (uintptr_t)ptr, SMALL_BYTES_FOR_MSIZE(msize));

#if CONFIG_AGGRESSIVE_MADVISE || CONFIG_RECIRC_DEPOT
	void *original_ptr = ptr;
#endif // CONFIG_AGGRESSIVE_MADVISE || CONFIG_RECIRC_DEPOT

#if DEBUG_MALLOC
	if (LOG(szone, ptr)) {
		malloc_report(ASL_LEVEL_ERR, "in small_free_no_lock(), ptr=%p, msize=%d\n", ptr, msize);
	}
	if (!msize) {
		malloc_zone_error(rack->debug_flags, true, "trying to free small block that is too small in small_free_no_lock(), ptr=%p, msize=%d\n",
				ptr, msize);
	}
#endif

	// Check that the region cookie is intact.
	region_check_cookie(region, &REGION_COOKIE_FOR_SMALL_REGION(region));

	// We try to coalesce this block with the preceeding one
	if (index > 0 && (meta_headers[index - 1] & SMALL_IS_FREE)) {
		msize_t previous_msize = meta_headers[index - 1] & ~SMALL_IS_FREE;
		grain_t previous_index = index - previous_msize;

		// Check if the metadata for the start of the block is also free.
		if (meta_headers[previous_index] == (previous_msize | SMALL_IS_FREE)) {
			void *previous_ptr = (void *)((uintptr_t)ptr - SMALL_BYTES_FOR_MSIZE(previous_msize));
			free_list_t previous = small_free_list_find_by_ptr(rack, small_mag_ptr, previous_ptr, previous_msize);
			small_free_list_remove_ptr(rack, small_mag_ptr, previous, previous_msize);
			ptr = previous_ptr;
			small_meta_header_set_middle(meta_headers, index); // This block is now a middle block.
			msize += previous_msize;
			index -= previous_msize;
		} else {
			_os_set_crash_log_message("small free list metadata inconsistency (headers[previous] != previous size)");
			__builtin_trap();
		}
	}

	// Try to coalesce with this block with the next block
	if ((next_block < SMALL_REGION_HEAP_END(region)) && (meta_headers[next_index] & SMALL_IS_FREE)) {
		msize_t next_msize = meta_headers[next_index] & ~SMALL_IS_FREE;
		free_list_t next = small_free_list_find_by_ptr(rack, small_mag_ptr, next_block, next_msize);
		small_free_list_remove_ptr(rack, small_mag_ptr, next, next_msize);
		msize += next_msize;
	}

	if (rack->debug_flags & MALLOC_DO_SCRIBBLE) {
		if (!msize) {
			malloc_zone_error(rack->debug_flags, true, "incorrect size information for %p - block header was damaged\n", ptr);
		} else {
			memset(ptr, SCRABBLE_BYTE, SMALL_BYTES_FOR_MSIZE(msize));
		}
	}

	free_list_t freee = small_free_list_add_ptr(rack, small_mag_ptr, ptr, msize);

	// use original_size and not msize to avoid double counting the coalesced blocks
	small_mag_ptr->mag_num_bytes_in_objects -= original_size;

	// Update this region's bytes in use count
	region_trailer_t *trailer = REGION_TRAILER_FOR_SMALL_REGION(region);
	size_t bytes_used = trailer->bytes_used - original_size;
	trailer->bytes_used = (unsigned int)bytes_used;

#if CONFIG_AGGRESSIVE_MADVISE
	if (aggressive_madvise_enabled) {
		small_madvise_free_range_no_lock(rack, small_mag_ptr, region, freee, msize, original_ptr, original_size);
	}
#endif

	// Caller must do SZONE_MAGAZINE_PTR_UNLOCK(tiny_mag_ptr) if this function
	// returns TRUE.
	boolean_t needs_unlock = TRUE;

#if CONFIG_RECIRC_DEPOT
	needs_unlock = small_free_try_recirc_to_depot(rack, small_mag_ptr, mag_index, region, freee, msize, original_ptr, original_size);
#endif // CONFIG_RECIRC_DEPOT
	return needs_unlock;
}

// Allocates from the last region or a freshly allocated region
static void *
small_malloc_from_region_no_lock(rack_t *rack,
								 magazine_t *small_mag_ptr,
								 mag_index_t mag_index,
								 msize_t msize,
								 void *aligned_address)
{
	void *ptr;

	// Before anything we transform the mag_bytes_free_at_end or mag_bytes_free_at_start - if any - to a regular free block
	/* FIXME: last_block needs to be coalesced with previous entry if free, <rdar://5462322> */
	if (small_mag_ptr->mag_bytes_free_at_end || small_mag_ptr->mag_bytes_free_at_start) {
		small_finalize_region(rack, small_mag_ptr);
	}

	small_region_t region = (small_region_t)aligned_address;

	// Tag the region at "aligned_address" as belonging to us,
	// and so put it under the protection of the magazine lock we are holding.
	// Do this before advertising "aligned_address" on the hash ring(!)
	MAGAZINE_INDEX_FOR_SMALL_REGION(region) = mag_index;

	// Insert the new region into the hash ring
	rack_region_insert(rack, region);

	small_mag_ptr->mag_last_region = region;
	BYTES_USED_FOR_SMALL_REGION(region) = SMALL_BYTES_FOR_MSIZE(msize);

#if CONFIG_ASLR_INTERNAL
	int offset_msize = malloc_entropy[1] & SMALL_ENTROPY_MASK;
#if DEBUG_MALLOC
	if (getenv("MallocASLRForce")) {
		offset_msize = strtol(getenv("MallocASLRForce"), NULL, 0) & SMALL_ENTROPY_MASK;
	}
	if (getenv("MallocASLRPrint")) {
		malloc_report(ASL_LEVEL_INFO, "Region: %p offset: %d\n", region, offset_msize);
	}
#endif
#else
	int offset_msize = 0;
#endif
	ptr = (void *)(SMALL_REGION_HEAP_BASE(region) + SMALL_BYTES_FOR_MSIZE(offset_msize));
	small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(ptr), offset_msize, msize);
	small_mag_ptr->mag_num_objects++;
	small_mag_ptr->mag_num_bytes_in_objects += SMALL_BYTES_FOR_MSIZE(msize);
	small_mag_ptr->num_bytes_in_magazine += SMALL_HEAP_SIZE;

	// add a big free block at the end
	small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(ptr), offset_msize + msize, NUM_SMALL_BLOCKS - msize - offset_msize);
	small_mag_ptr->mag_bytes_free_at_end = SMALL_BYTES_FOR_MSIZE(NUM_SMALL_BLOCKS - msize - offset_msize);

#if CONFIG_ASLR_INTERNAL
	// add a big free block at the start
	small_mag_ptr->mag_bytes_free_at_start = SMALL_BYTES_FOR_MSIZE(offset_msize);
	if (offset_msize) {
		small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(ptr), 0, offset_msize);
	}
#else
	small_mag_ptr->mag_bytes_free_at_start = 0;
#endif

	// connect to magazine as last node
	recirc_list_splice_last(rack, small_mag_ptr, REGION_TRAILER_FOR_SMALL_REGION(region));

	return ptr;
}

void *
small_memalign(szone_t *szone, size_t alignment, size_t size, size_t span)
{
	msize_t mspan = SMALL_MSIZE_FOR_BYTES(span + SMALL_QUANTUM - 1);
	void *p = small_malloc_should_clear(&szone->small_rack, mspan, 0);

	if (NULL == p) {
		return NULL;
	}

	size_t offset = ((uintptr_t)p) & (alignment - 1);	// p % alignment
	size_t pad = (0 == offset) ? 0 : alignment - offset; // p + pad achieves desired alignment

	msize_t msize = SMALL_MSIZE_FOR_BYTES(size + SMALL_QUANTUM - 1);
	msize_t mpad = SMALL_MSIZE_FOR_BYTES(pad + SMALL_QUANTUM - 1);
	msize_t mwaste = mspan - msize - mpad; // excess blocks

	if (mpad > 0) {
		void *q = (void *)(((uintptr_t)p) + pad);

		// Mark q as block header and in-use, thus creating two blocks.
		magazine_t *small_mag_ptr = mag_lock_zine_for_region_trailer(szone->small_rack.magazines,
				REGION_TRAILER_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(p)),
				MAGAZINE_INDEX_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(p)));
		small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(p), SMALL_META_INDEX_FOR_PTR(p), mpad);
		small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(q), SMALL_META_INDEX_FOR_PTR(q), msize + mwaste);
		small_mag_ptr->mag_num_objects++;
		SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);

		// Give up mpad blocks beginning at p to the small free list
		free_small(&szone->small_rack, p, SMALL_REGION_FOR_PTR(p), SMALL_BYTES_FOR_MSIZE(mpad));

		p = q; // advance p to the desired alignment
	}
	if (mwaste > 0) {
		void *q = (void *)(((uintptr_t)p) + SMALL_BYTES_FOR_MSIZE(msize));
		// Mark q as block header and in-use, thus creating two blocks.
		magazine_t *small_mag_ptr = mag_lock_zine_for_region_trailer(szone->small_rack.magazines,
				REGION_TRAILER_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(p)),
				MAGAZINE_INDEX_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(p)));
		small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(p), SMALL_META_INDEX_FOR_PTR(p), msize);
		small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(q), SMALL_META_INDEX_FOR_PTR(q), mwaste);
		small_mag_ptr->mag_num_objects++;
		SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);

		// Give up mwaste blocks beginning at q to the small free list
		free_small(&szone->small_rack, q, SMALL_REGION_FOR_PTR(q), SMALL_BYTES_FOR_MSIZE(mwaste));
	}

	return p; // p has the desired size and alignment, and can later be free()'d
}

boolean_t
small_claimed_address(rack_t *rack, void *ptr)
{
	region_t r = small_region_for_ptr_no_lock(rack, ptr);
	return r && ptr >= SMALL_REGION_HEAP_BASE(r)
			&& ptr < SMALL_REGION_HEAP_END(r);
}

void *
small_try_shrink_in_place(rack_t *rack, void *ptr, size_t old_size, size_t new_good_size)
{
	msize_t new_msize = SMALL_MSIZE_FOR_BYTES(new_good_size);
	msize_t mshrinkage = SMALL_MSIZE_FOR_BYTES(old_size) - new_msize;

	if (mshrinkage) {
		void *q = (void *)((uintptr_t)ptr + SMALL_BYTES_FOR_MSIZE(new_msize));
		magazine_t *small_mag_ptr = mag_lock_zine_for_region_trailer(rack->magazines,
				REGION_TRAILER_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(ptr)),
				MAGAZINE_INDEX_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(ptr)));

		// Mark q as block header and in-use, thus creating two blocks.
		small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(ptr), SMALL_META_INDEX_FOR_PTR(ptr), new_msize);
		small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(q), SMALL_META_INDEX_FOR_PTR(q), mshrinkage);
		small_mag_ptr->mag_num_objects++;

		SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
		free_small(rack, q, SMALL_REGION_FOR_PTR(q), 0);
	}

	return ptr;
}

boolean_t
small_try_realloc_in_place(rack_t *rack, void *ptr, size_t old_size, size_t new_size)
{
	// returns 1 on success
	msize_t *meta_headers = SMALL_META_HEADER_FOR_PTR(ptr);
	unsigned index;
	msize_t old_msize, new_msize;
	unsigned next_index;
	void *next_block;
	msize_t next_msize_and_free;
	boolean_t is_free;
	msize_t next_msize, leftover_msize;
	void *leftover;

	index = SMALL_META_INDEX_FOR_PTR(ptr);
	old_msize = SMALL_MSIZE_FOR_BYTES(old_size);
	new_msize = SMALL_MSIZE_FOR_BYTES(new_size + SMALL_QUANTUM - 1);
	next_index = index + old_msize;

	if (next_index >= NUM_SMALL_BLOCKS) {
		return 0;
	}
	next_block = (char *)ptr + old_size;

#if DEBUG_MALLOC
	if ((uintptr_t)next_block & (SMALL_QUANTUM - 1)) {
		malloc_zone_error(rack->debug_flags, true, "internal invariant broken in realloc(next_block) for %p\n", next_block);
	}
	if (meta_headers[index] != old_msize) {
		malloc_report(ASL_LEVEL_ERR, "*** small_try_realloc_in_place incorrect old %d %d\n", meta_headers[index], old_msize);
	}
#endif

	magazine_t *small_mag_ptr = mag_lock_zine_for_region_trailer(rack->magazines,
			REGION_TRAILER_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(ptr)),
			MAGAZINE_INDEX_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(ptr)));
	if (DEPOT_MAGAZINE_INDEX == MAGAZINE_INDEX_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(ptr))) {
		SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
		return 0;
	}

	msize_t coalesced_msize = new_msize - old_msize;
#if CONFIG_SMALL_CACHE
	void *last_free_ptr = small_mag_ptr->mag_last_free;
	msize_t last_free_msize = small_mag_ptr->mag_last_free_msize;
	if (last_free_ptr == next_block && old_msize + last_free_msize >= new_msize) {
		/*
		 * There is a block in mag_last_free and it's immediately after
		 * this block and it's large enough. We can use some or all of it.
		 */
		leftover_msize = last_free_msize - coalesced_msize;
		if (leftover_msize) {
			small_mag_ptr->mag_last_free_msize -= coalesced_msize;
			small_mag_ptr->mag_last_free += new_size - old_size;
			// The block in mag_last_free is still marked as header and in-use, so copy that
			// state to the block that remains. The state for the block that we're going to
			// use is adjusted by the small_meta_header_set_middle() call below.
			small_meta_header_set_in_use(meta_headers, index + new_msize, leftover_msize);
		} else {
			// Using the whole block.
			small_mag_ptr->mag_last_free = NULL;
			small_mag_ptr->mag_last_free_msize = 0;
			small_mag_ptr->mag_last_free_rgn = NULL;
		}
		small_meta_header_set_in_use(meta_headers, index, new_msize);
		small_meta_header_set_middle(meta_headers, next_index);
	} else {
#endif // CONFIG_SMALL_CACHE
		/*
		 * Try to expand into unused space immediately after this block.
		 */
		msize_t unused_msize = SMALL_MSIZE_FOR_BYTES(small_mag_ptr->mag_bytes_free_at_end);
		void *unused_start = SMALL_REGION_HEAP_END(SMALL_REGION_FOR_PTR(ptr)) - small_mag_ptr->mag_bytes_free_at_end;
		if (small_mag_ptr->mag_last_region == SMALL_REGION_FOR_PTR(ptr)
				&& coalesced_msize < unused_msize && unused_start == ptr + old_size) {
			// Extend the in-use for this block to the new size
			small_meta_header_set_in_use(meta_headers, index, new_msize);

			// Clear the in-use size for the start of the area we extended into
			small_meta_header_set_middle(meta_headers, next_index);

			// Reduce mag_bytes_free_at_end and update its in-use size.
			small_mag_ptr->mag_bytes_free_at_end -= SMALL_BYTES_FOR_MSIZE(coalesced_msize);
			small_meta_header_set_in_use(meta_headers, index + new_msize, SMALL_MSIZE_FOR_BYTES(small_mag_ptr->mag_bytes_free_at_end));
		} else {
			/*
			 * Look for a free block immediately afterwards.  If it's large enough, we can consume (part of)
			 * it.
			 */
			next_msize_and_free = meta_headers[next_index];
			is_free = next_msize_and_free & SMALL_IS_FREE;
			if (!is_free) {
				SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
				return 0; // next_block is in use;
			}

			next_msize = next_msize_and_free & ~SMALL_IS_FREE;
			if (old_msize + next_msize < new_msize) {
				SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
				return 0; // even with next block, not enough
			}

			// The following block is big enough; pull it from its freelist and chop off enough to satisfy
			// our needs.
			free_list_t freee = small_free_list_find_by_ptr(rack, small_mag_ptr, next_block, next_msize);
			small_free_list_remove_ptr(rack, small_mag_ptr, freee, next_msize);
			small_meta_header_set_middle(meta_headers, next_index);
			leftover_msize = old_msize + next_msize - new_msize;
			if (leftover_msize) {
				/* there's some left, so put the remainder back */
				leftover = (unsigned char *)ptr + SMALL_BYTES_FOR_MSIZE(new_msize);
				small_free_list_add_ptr(rack, small_mag_ptr, leftover, leftover_msize);
			}
			small_meta_header_set_in_use(meta_headers, index, new_msize);
		}
#if CONFIG_SMALL_CACHE
	}
#endif // CONFIG_SMALL_CACHE
#if DEBUG_MALLOC
	if (SMALL_BYTES_FOR_MSIZE(new_msize) > szone->large_threshold) {
		malloc_report(ASL_LEVEL_ERR, "*** realloc in place for %p exceeded msize=%d\n", new_msize);
	}

	if (LOG(szone, ptr)) {
		malloc_report(ASL_LEVEL_INFO, "in small_try_realloc_in_place(), ptr=%p, msize=%d\n", ptr, *SMALL_METADATA_FOR_PTR(ptr));
	}
#endif
	small_mag_ptr->mag_num_bytes_in_objects += SMALL_BYTES_FOR_MSIZE(new_msize - old_msize);

	// Update this region's bytes in use count
	region_trailer_t *node = REGION_TRAILER_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(ptr));
	size_t bytes_used = node->bytes_used + SMALL_BYTES_FOR_MSIZE(new_msize - old_msize);
	node->bytes_used = (unsigned int)bytes_used;

	// Emptiness discriminant
	if (bytes_used < DENSITY_THRESHOLD(SMALL_HEAP_SIZE)) {
		/* After this reallocation the region is still sparse, so it must have been even more so before
		 * the reallocation. That implies the region is already correctly marked. Do nothing. */
	} else {
		/* Region has crossed threshold from sparsity to density. Mark it not "suitable" on the
		 * recirculation candidates list. */
		node->recirc_suitable = FALSE;
	}

	SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
	CHECK(szone, __PRETTY_FUNCTION__);
	return 1;
}

static char *small_check_fail_msg = "check: incorrect small region ";

#define SMALL_CHECK_FAIL(fmt, ...) \
	malloc_zone_check_fail(small_check_fail_msg, \
			"%ld, counter=%d\n" fmt,  region_index, counter, __VA_ARGS__);

boolean_t
small_check_region(rack_t *rack, region_t region, size_t region_index,
		unsigned counter)
{
	unsigned char *ptr = SMALL_REGION_HEAP_BASE(region);
	msize_t *meta_headers = SMALL_META_HEADER_FOR_PTR(ptr);
	unsigned char *region_end = SMALL_REGION_HEAP_END(region);
	msize_t prev_free = 0;
	unsigned index;
	msize_t msize_and_free;
	msize_t msize;
	free_list_t free_head, previous, next;
	msize_t *follower;
	mag_index_t mag_index = MAGAZINE_INDEX_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(ptr));
	magazine_t *small_mag_ptr = &(rack->magazines[mag_index]);

	// Assumes locked
	CHECK_MAGAZINE_PTR_LOCKED(szone, small_mag_ptr, __PRETTY_FUNCTION__);

	if (region == small_mag_ptr->mag_last_region) {
		ptr += small_mag_ptr->mag_bytes_free_at_start;
		region_end -= small_mag_ptr->mag_bytes_free_at_end;
	}

	while (ptr < region_end) {
		index = SMALL_META_INDEX_FOR_PTR(ptr);
		msize_and_free = meta_headers[index];
		if (!(msize_and_free & SMALL_IS_FREE)) {
			// block is in use
			msize = msize_and_free;
			if (!msize) {
				SMALL_CHECK_FAIL("*** invariant broken: null msize ptr=%p num_small_regions=%d end=%p\n", ptr,
							  (int)rack->num_regions, region_end);
				return 0;
			}
#if !CONFIG_RELAXED_INVARIANT_CHECKS
			if (SMALL_BYTES_FOR_MSIZE(msize) > szone->large_threshold) {
				SMALL_CHECK_FAIL("*** invariant broken for %p this small msize=%d - size is too large\n", ptr, msize_and_free);
				return 0;
			}
#endif // CONFIG_RELAXED_INVARIANT_CHECKS
			ptr += SMALL_BYTES_FOR_MSIZE(msize);
			prev_free = 0;
		} else {
			// free pointer
			msize = msize_and_free & ~SMALL_IS_FREE;
			free_head = (free_list_t){ .p = ptr };
			follower = (msize_t *)FOLLOWING_SMALL_PTR(ptr, msize);
			if (!msize) {
				SMALL_CHECK_FAIL("*** invariant broken for free block %p this msize=%d\n", ptr, msize);
				return 0;
			}
#if !CONFIG_RELAXED_INVARIANT_CHECKS
			if (prev_free) {
				SMALL_CHECK_FAIL("*** invariant broken for %p (2 free in a row)\n", ptr);
				return 0;
			}
#endif

			// <rdar://problem/24680189> check for possible OOB entry if needed
			if (small_needs_oob_free_entry(ptr, msize)) {
				oob_free_entry_t oob = small_oob_free_find_ptr(ptr, msize);
				if (oob) {
					free_head.oob = oob;
				}
			}

			previous = small_free_list_get_previous(rack, free_head);
			next = small_free_list_get_next(rack, free_head);
			if (previous.p && !SMALL_PTR_IS_FREE(small_free_list_get_ptr(previous))) {
				SMALL_CHECK_FAIL("*** invariant broken for %p (previous %p is not a free pointer)\n", ptr, small_free_list_get_ptr(previous));
				return 0;
			}
			if (next.p && !SMALL_PTR_IS_FREE(small_free_list_get_ptr(next))) {
				SMALL_CHECK_FAIL("*** invariant broken for %p (next %p is not a free pointer)\n", ptr, small_free_list_get_ptr(next));
				return 0;
			}
			if (SMALL_PREVIOUS_MSIZE(follower) != msize) {
				SMALL_CHECK_FAIL("*** invariant broken for small free %p followed by %p in region [%p-%p] "
						"(end marker incorrect) should be %d; in fact %d\n",
						ptr, follower, SMALL_REGION_HEAP_BASE(region), region_end, msize, SMALL_PREVIOUS_MSIZE(follower));
				return 0;
			}
			ptr = (unsigned char *)follower;
			prev_free = SMALL_IS_FREE;
		}
	}
	return 1;
}

kern_return_t
small_in_use_enumerator(task_t task,
						void *context,
						unsigned type_mask,
						szone_t *szone,
						memory_reader_t reader,
						vm_range_recorder_t recorder)
{
	size_t num_regions;
	size_t index;
	region_t *regions;
	vm_range_t buffer[MAX_RECORDER_BUFFER];
	unsigned count = 0;
	kern_return_t err;
	region_t region;
	vm_range_t range;
	vm_range_t admin_range;
	vm_range_t ptr_range;
	unsigned char *mapped_region;
	msize_t *block_header;
	unsigned block_index;
	unsigned block_limit;
	msize_t msize_and_free;
	msize_t msize;
	magazine_t *small_mag_base = NULL;

	region_hash_generation_t *srg_ptr;
	err = reader(task, (vm_address_t)szone->small_rack.region_generation, sizeof(region_hash_generation_t), (void **)&srg_ptr);
	if (err) {
		return err;
	}

	num_regions = srg_ptr->num_regions_allocated;
	err = reader(task, (vm_address_t)srg_ptr->hashed_regions, sizeof(region_t) * num_regions, (void **)&regions);
	if (err) {
		return err;
	}

	if (type_mask & MALLOC_PTR_IN_USE_RANGE_TYPE) {
		// Map in all active magazines. Do this outside the iteration over regions.
		err = reader(task, (vm_address_t)(szone->small_rack.magazines), szone->small_rack.num_magazines * sizeof(magazine_t),
					 (void **)&small_mag_base);
		if (err) {
			return err;
		}
	}

	for (index = 0; index < num_regions; ++index) {
		region = regions[index];
		if (HASHRING_OPEN_ENTRY != region && HASHRING_REGION_DEALLOCATED != region) {
			range.address = (vm_address_t)SMALL_REGION_HEAP_BASE(region);
			range.size = SMALL_HEAP_SIZE;
			if (type_mask & MALLOC_ADMIN_REGION_RANGE_TYPE) {
				admin_range.address = SMALL_REGION_METADATA(region);
				admin_range.size = SMALL_METADATA_SIZE;
				recorder(task, context, MALLOC_ADMIN_REGION_RANGE_TYPE, &admin_range, 1);
			}
			if (type_mask & (MALLOC_PTR_REGION_RANGE_TYPE | MALLOC_ADMIN_REGION_RANGE_TYPE)) {
				ptr_range.address = range.address;
				ptr_range.size = SMALL_HEAP_SIZE;
				recorder(task, context, MALLOC_PTR_REGION_RANGE_TYPE, &ptr_range, 1);
			}
			if (type_mask & MALLOC_PTR_IN_USE_RANGE_TYPE) {
				err = reader(task, (vm_address_t)region, (vm_size_t)SMALL_REGION_SIZE, (void **)&mapped_region);
				if (err) {
					return err;
				}

				mag_index_t mag_index = MAGAZINE_INDEX_FOR_SMALL_REGION(mapped_region);
				magazine_t *small_mag_ptr = small_mag_base + mag_index;

				int cached_free_blocks = 0;
#if CONFIG_SMALL_CACHE
				// Each magazine could have a pointer to a cached free block from
				// this region. Count the regions that have such a pointer.
				for (mag_index = 0; mag_index < szone->small_rack.num_magazines; mag_index++) {
					if (region == (small_mag_base + mag_index)->mag_last_free_rgn) {
						cached_free_blocks++;
					}
				}
#endif // CONFIG_SMALL_CACHE

				block_header = SMALL_META_HEADER_FOR_REGION(mapped_region);
				block_index = 0;
				block_limit = NUM_SMALL_BLOCKS;
				if (region == small_mag_ptr->mag_last_region) {
					block_index += SMALL_MSIZE_FOR_BYTES(small_mag_ptr->mag_bytes_free_at_start);
					block_limit -= SMALL_MSIZE_FOR_BYTES(small_mag_ptr->mag_bytes_free_at_end);
				}

				for (;block_index < block_limit; block_index += msize) {
					msize_and_free = block_header[block_index];
					msize = msize_and_free & ~SMALL_IS_FREE;
					if (!msize) {
						return KERN_FAILURE; // Somethings amiss. Avoid looping at this block_index.
					}
					if (!(msize_and_free & SMALL_IS_FREE)) {
						void *ptr = SMALL_REGION_HEAP_BASE(region) + SMALL_BYTES_FOR_MSIZE(block_index);
#if CONFIG_SMALL_CACHE
						// If there are still magazines that have cached free
						// blocks in this region, check whether this is one of
						// them and don't return the block pointer if it is.
						boolean_t block_cached = false;
						if (cached_free_blocks) {
							for (mag_index = 0; mag_index < szone->small_rack.num_magazines; mag_index++) {
								if (ptr == (small_mag_base + mag_index)->mag_last_free) {
									block_cached = true;
									cached_free_blocks--;
									break;
								}
							}
						}
						if (block_cached) {
							continue;
						}
#endif // CONFIG_SMALL_CACHE
						// Block in use
						buffer[count].address = (vm_address_t)ptr;
						buffer[count].size = SMALL_BYTES_FOR_MSIZE(msize);
						count++;
						if (count >= MAX_RECORDER_BUFFER) {
							recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE, buffer, count);
							count = 0;
						}
					}
				}
				if (count) {
					recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE, buffer, count);
					count = 0;
				}
			}
		}
	}
	return 0;
}

static void *
small_malloc_from_free_list(rack_t *rack, magazine_t *small_mag_ptr, mag_index_t mag_index, msize_t msize)
{
	msize_t this_msize;
	grain_t slot = SMALL_FREE_SLOT_FOR_MSIZE(rack, msize);
	free_list_t *free_list = small_mag_ptr->mag_free_list;
	free_list_t *the_slot = free_list + slot;
	free_list_t *limit;
	unsigned bitmap;
	msize_t leftover_msize;
	void *leftover_ptr;
	void *ptr;

	// Assumes we've locked the region
	CHECK_MAGAZINE_PTR_LOCKED(szone, small_mag_ptr, __PRETTY_FUNCTION__);

	// Look for an exact match by checking the freelist for this msize.
	if (small_free_list_get_ptr(*the_slot)) {
		ptr = small_free_list_get_ptr(*the_slot);
		this_msize = msize;
		small_free_list_remove_ptr(rack, small_mag_ptr, *the_slot, msize);
		goto return_small_alloc;
	}

	// Mask off the bits representing slots holding free blocks smaller than
	// the size we need.  If there are no larger free blocks, try allocating
	// from the free space at the end of the small region.
	//
	// BITMAPN_CTZ implementation
	unsigned idx = slot >> 5;
	bitmap = 0;
	unsigned mask = ~((1 << (slot & 31)) - 1);
	for (; idx < SMALL_FREELIST_BITMAP_WORDS(rack); ++idx) {
		bitmap = small_mag_ptr->mag_bitmap[idx] & mask;
		if (bitmap != 0) {
			break;
		}
		mask = ~0U;
	}
	// Check for fallthrough: No bits set in bitmap
	if ((bitmap == 0) && (idx == SMALL_FREELIST_BITMAP_WORDS(rack))) {
		goto try_small_from_end;
	}

	// Start looking at the first set bit, plus 32 bits for every word of
	// zeroes or entries that were too small.
	slot = BITMAP32_CTZ((&bitmap)) + (idx * 32);

	// FIXME: Explain use of - 1 here, last slot has special meaning
	limit = free_list + SMALL_FREE_SLOT_COUNT(rack) - 1;
	free_list += slot;

	// Attempt to pull off the free_list slot that we now think is full.
	if ((ptr = small_free_list_get_ptr(*free_list))) {
		this_msize = SMALL_PTR_SIZE(ptr);
		small_free_list_remove_ptr(rack, small_mag_ptr, *free_list, this_msize);
		goto add_leftover_and_proceed;
	}

#if DEBUG_MALLOC
	malloc_report(ASL_LEVEL_ERR, "in small_malloc_from_free_list(), mag_bitmap out of sync, slot=%d\n", slot);
#endif

try_small_from_end:
	// Let's see if we can use small_mag_ptr->mag_bytes_free_at_end
	if (small_mag_ptr->mag_bytes_free_at_end >= SMALL_BYTES_FOR_MSIZE(msize)) {
		ptr = SMALL_REGION_HEAP_END(small_mag_ptr->mag_last_region) - small_mag_ptr->mag_bytes_free_at_end;
		small_mag_ptr->mag_bytes_free_at_end -= SMALL_BYTES_FOR_MSIZE(msize);
		if (small_mag_ptr->mag_bytes_free_at_end) {
			// let's mark this block as in use to serve as boundary
			small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(ptr),
										 SMALL_META_INDEX_FOR_PTR((unsigned char *)ptr + SMALL_BYTES_FOR_MSIZE(msize)),
										 SMALL_MSIZE_FOR_BYTES(small_mag_ptr->mag_bytes_free_at_end));
		}
		this_msize = msize;
		goto return_small_alloc;
	}
#if CONFIG_ASLR_INTERNAL
	// Try from start if nothing left at end
	if (small_mag_ptr->mag_bytes_free_at_start >= SMALL_BYTES_FOR_MSIZE(msize)) {
		ptr = SMALL_REGION_HEAP_BASE(small_mag_ptr->mag_last_region) + small_mag_ptr->mag_bytes_free_at_start -
				SMALL_BYTES_FOR_MSIZE(msize);
		small_mag_ptr->mag_bytes_free_at_start -= SMALL_BYTES_FOR_MSIZE(msize);
		if (small_mag_ptr->mag_bytes_free_at_start) {
			// let's mark this block as in use to serve as boundary
			small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(ptr), 0,
					SMALL_MSIZE_FOR_BYTES(small_mag_ptr->mag_bytes_free_at_start));
		}
		this_msize = msize;
		goto return_small_alloc;
	}
#endif
	return NULL;

add_leftover_and_proceed:
	if (this_msize > msize) {
		leftover_msize = this_msize - msize;
		leftover_ptr = (unsigned char *)ptr + SMALL_BYTES_FOR_MSIZE(msize);
#if DEBUG_MALLOC
		if (LOG(szone, ptr)) {
			malloc_report(ASL_LEVEL_INFO, "in small_malloc_from_free_list(), adding leftover ptr=%p, this_msize=%d\n", ptr, this_msize);
		}
#endif
		small_free_list_add_ptr(rack, small_mag_ptr, leftover_ptr, leftover_msize);
		this_msize = msize;
	}

return_small_alloc:
	small_mag_ptr->mag_num_objects++;
	small_mag_ptr->mag_num_bytes_in_objects += SMALL_BYTES_FOR_MSIZE(this_msize);

	// Check that the region cookie is intact and update the region's bytes in use count
	small_region_t region = SMALL_REGION_FOR_PTR(ptr);
	region_check_cookie(region, &REGION_COOKIE_FOR_SMALL_REGION(region));

	region_trailer_t *trailer = REGION_TRAILER_FOR_SMALL_REGION(region);
	size_t bytes_used = trailer->bytes_used + SMALL_BYTES_FOR_MSIZE(this_msize);
	trailer->bytes_used = (unsigned int)bytes_used;

	// Emptiness discriminant
	if (bytes_used < DENSITY_THRESHOLD(SMALL_HEAP_SIZE)) {
		/* After this allocation the region is still sparse, so it must have been even more so before
		 * the allocation. That implies the region is already correctly marked. Do nothing. */
	} else {
		/* Region has crossed threshold from sparsity to density. Mark in not "suitable" on the
		 * recirculation candidates list. */
		trailer->recirc_suitable = FALSE;
	}
#if DEBUG_MALLOC
	if (LOG(szone, ptr)) {
		malloc_report(ASL_LEVEL_INFO, "in small_malloc_from_free_list(), ptr=%p, this_msize=%d, msize=%d\n", ptr, this_msize, msize);
	}
#endif
	small_meta_header_set_in_use(SMALL_META_HEADER_FOR_PTR(ptr), SMALL_META_INDEX_FOR_PTR(ptr), this_msize);
	return ptr;
}

void *
small_malloc_should_clear(rack_t *rack, msize_t msize, boolean_t cleared_requested)
{
	void *ptr;
	mag_index_t mag_index = rack_get_thread_index(rack) % rack->num_magazines;
	magazine_t *small_mag_ptr = &(rack->magazines[mag_index]);

	MALLOC_TRACE(TRACE_small_malloc, (uintptr_t)rack, SMALL_BYTES_FOR_MSIZE(msize), (uintptr_t)small_mag_ptr, cleared_requested);

	SZONE_MAGAZINE_PTR_LOCK(small_mag_ptr);

#if CONFIG_SMALL_CACHE
	ptr = small_mag_ptr->mag_last_free;

	if (small_mag_ptr->mag_last_free_msize == msize) {
		// we have a winner
		small_mag_ptr->mag_last_free = NULL;
		small_mag_ptr->mag_last_free_msize = 0;
		small_mag_ptr->mag_last_free_rgn = NULL;
		SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
		CHECK(szone, __PRETTY_FUNCTION__);
		if (cleared_requested) {
			memset(ptr, 0, SMALL_BYTES_FOR_MSIZE(msize));
		}
		return ptr;
	}
#endif /* CONFIG_SMALL_CACHE */

	while (1) {
		ptr = small_malloc_from_free_list(rack, small_mag_ptr, mag_index, msize);
		if (ptr) {
			SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
			CHECK(szone, __PRETTY_FUNCTION__);
			if (cleared_requested) {
				memset(ptr, 0, SMALL_BYTES_FOR_MSIZE(msize));
			}
			return ptr;
		}

#if CONFIG_RECIRC_DEPOT
		if (small_get_region_from_depot(rack, small_mag_ptr, mag_index, msize)) {
			ptr = small_malloc_from_free_list(rack, small_mag_ptr, mag_index, msize);
			if (ptr) {
				SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
				CHECK(szone, __PRETTY_FUNCTION__);
				if (cleared_requested) {
					memset(ptr, 0, SMALL_BYTES_FOR_MSIZE(msize));
				}
				return ptr;
			}
		}
#endif // CONFIG_RECIRC_DEPOT

		// The magazine is exhausted. A new region (heap) must be allocated to satisfy this call to malloc().
		// The allocation, an mmap() system call, will be performed outside the magazine spin locks by the first
		// thread that suffers the exhaustion. That thread accquires the magazine_alloc_lock, then drops the
		// magazine lock to allow freeing threads to proceed. Allocating thrads that arrive later  are excluded
		// from the critial section by the alloc lock. When those are unblocked, they succeed in the code above.
		//
		// Note that we need to trylock the alloc lock to avoid a deadlock, since we can't block on the alloc
		// lock while holding the magazine lock
		if (os_likely(_malloc_lock_trylock(&small_mag_ptr->magazine_alloc_lock))) {
			// We got the alloc lock, so we are the thread that should allocate a new region
			void *fresh_region;

			// time to create a new region (do this outside the magazine lock)
			SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
			fresh_region = mvm_allocate_pages(SMALL_REGION_SIZE,
					SMALL_BLOCKS_ALIGN,
					MALLOC_FIX_GUARD_PAGE_FLAGS(rack->debug_flags),
					VM_MEMORY_MALLOC_SMALL);
			SZONE_MAGAZINE_PTR_LOCK(small_mag_ptr);

			// DTrace USDT Probe
			MAGMALLOC_ALLOCREGION(SMALL_SZONE_FROM_RACK(rack), (int)mag_index, fresh_region, SMALL_REGION_SIZE);

			if (!fresh_region) { // out of memory!
				SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
				_malloc_lock_unlock(&small_mag_ptr->magazine_alloc_lock);
				return NULL;
			}

			region_set_cookie(&REGION_COOKIE_FOR_SMALL_REGION(fresh_region));
			ptr = small_malloc_from_region_no_lock(rack, small_mag_ptr, mag_index, msize, fresh_region);

			// we don't clear because this freshly allocated space is pristine
			SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
			_malloc_lock_unlock(&small_mag_ptr->magazine_alloc_lock);
			CHECK(szone, __PRETTY_FUNCTION__);
			return ptr;
		} else {
			// We failed to get the alloc lock, so someone else is allocating.
			// Drop the magazine lock...
			SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);

			// Wait for the other thread on the alloc lock
			_malloc_lock_lock(&small_mag_ptr->magazine_alloc_lock);
			_malloc_lock_unlock(&small_mag_ptr->magazine_alloc_lock);

			// Reacquire the magazine lock to go around the loop again
			SZONE_MAGAZINE_PTR_LOCK(small_mag_ptr);
		}
	}
	/* NOTREACHED */
}

size_t
small_size(rack_t *rack, const void *ptr)
{
	if (small_region_for_ptr_no_lock(rack, ptr)) {
		if (SMALL_META_INDEX_FOR_PTR(ptr) >= NUM_SMALL_BLOCKS) {
			return 0;
		}
		msize_t msize_and_free = *SMALL_METADATA_FOR_PTR(ptr);
		if (msize_and_free & SMALL_IS_FREE) {
			return 0;
		}
#if CONFIG_SMALL_CACHE
		{
			mag_index_t mag_index = MAGAZINE_INDEX_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(ptr));
			if (DEPOT_MAGAZINE_INDEX != mag_index) {
				magazine_t *small_mag_ptr = &(rack->magazines[mag_index]);

				if (ptr == small_mag_ptr->mag_last_free) {
					return 0;
				}
			} else {
				for (mag_index = 0; mag_index < rack->num_magazines; mag_index++) {
					magazine_t *small_mag_ptr = &(rack->magazines[mag_index]);

					if (ptr == small_mag_ptr->mag_last_free) {
						return 0;
					}
				}
			}
		}
#endif
		return SMALL_BYTES_FOR_MSIZE(msize_and_free);
	}

	return 0;
}

static MALLOC_NOINLINE void
free_small_botch(rack_t *rack, void *ptr)
{
	malloc_zone_error(rack->debug_flags, true, "double free for ptr %p\n", ptr);
}

void
free_small(rack_t *rack, void *ptr, region_t small_region, size_t known_size)
{
	msize_t msize;
	mag_index_t mag_index = MAGAZINE_INDEX_FOR_SMALL_REGION(SMALL_REGION_FOR_PTR(ptr));
	magazine_t *small_mag_ptr = &(rack->magazines[mag_index]);

	// ptr is known to be in small_region
	if (known_size) {
		msize = SMALL_MSIZE_FOR_BYTES(known_size + SMALL_QUANTUM - 1);
	} else {
		msize = SMALL_PTR_SIZE(ptr);
		if (SMALL_PTR_IS_FREE(ptr)) {
			free_small_botch(rack, ptr);
			return;
		}
	}

	SZONE_MAGAZINE_PTR_LOCK(small_mag_ptr);

#if CONFIG_SMALL_CACHE
	// Depot does not participate in CONFIG_SMALL_CACHE since it can't be directly malloc()'d
	if (DEPOT_MAGAZINE_INDEX != mag_index) {
		void *ptr2 = small_mag_ptr->mag_last_free; // Might be NULL
		msize_t msize2 = small_mag_ptr->mag_last_free_msize;
		region_t rgn2 = small_mag_ptr->mag_last_free_rgn;

		/* check that we don't already have this pointer in the cache */
		if (ptr == ptr2) {
			SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
			free_small_botch(rack, ptr);
			return;
		}

		if ((rack->debug_flags & MALLOC_DO_SCRIBBLE) && msize) {
			memset(ptr, SCRABBLE_BYTE, SMALL_BYTES_FOR_MSIZE(msize));
		}

		small_mag_ptr->mag_last_free = ptr;
		small_mag_ptr->mag_last_free_msize = msize;
		small_mag_ptr->mag_last_free_rgn = small_region;

		if (!ptr2) {
			SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
			CHECK(szone, __PRETTY_FUNCTION__);
			return;
		}

		msize = msize2;
		ptr = ptr2;
		small_region = rgn2;
	}
#endif /* CONFIG_SMALL_CACHE */

	// Now in the time it took to acquire the lock, the region may have migrated
	// from one magazine to another. I.e. trailer->mag_index is volatile.
	// In which case the magazine lock we obtained (namely magazines[mag_index].mag_lock)
	// is stale. If so, keep on tryin' ...
	region_trailer_t *trailer = REGION_TRAILER_FOR_SMALL_REGION(small_region);
	mag_index_t refreshed_index;

	while (mag_index != (refreshed_index = trailer->mag_index)) { // Note assignment

		SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);

		mag_index = refreshed_index;
		small_mag_ptr = &(rack->magazines[mag_index]);
		SZONE_MAGAZINE_PTR_LOCK(small_mag_ptr);
	}

	if (small_free_no_lock(rack, small_mag_ptr, mag_index, small_region, ptr, msize)) {
		SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
	}

	CHECK(szone, __PRETTY_FUNCTION__);
}

void
print_small_free_list(task_t task, memory_reader_t reader,
		print_task_printer_t printer, rack_t *rack)
{
	free_list_t ptr;
	_SIMPLE_STRING b = _simple_salloc();
	mag_index_t mag_index;

	if (b) {
		rack_t *mapped_rack;
		magazine_t *mapped_magazines;
		if (reader(task, (vm_address_t)rack, sizeof(struct rack_s),
				(void **)&mapped_rack)) {
			printer("Failed to map small rack\n");
			return;
		}
		if (reader(task, (vm_address_t)mapped_rack->magazines,
				mapped_rack->num_magazines * sizeof(magazine_t),
				(void **)&mapped_magazines)) {
			printer("Failed to map small rack magazines\n");
			return;
		}

        _simple_sappend(b, "small free sizes:\n");
		grain_t free_slots = SMALL_FREE_SLOT_COUNT(mapped_rack);
		for (mag_index = -1; mag_index < mapped_rack->num_magazines;
				mag_index++) {
			grain_t slot = 0;
			if (mag_index == -1) {
				_simple_sprintf(b, "\tRecirc depot: ");
			} else {
				_simple_sprintf(b, "\tMagazine %d: ", mag_index);
			}
			while (slot < free_slots) {
				ptr = mapped_magazines[mag_index].mag_free_list[slot];
				if (small_free_list_get_ptr_task(task, reader, printer, ptr)) {
					_simple_sprintf(b, "%s%y[%d]; ", (slot == free_slots - 1) ?
							">=" : "", (slot + 1) * SMALL_QUANTUM,
							small_free_list_count(task, reader, printer,
									rack, ptr));
				}
				slot++;
			}
			_simple_sappend(b, "\n");
		}
		printer("%s\n", _simple_string(b));
		_simple_sfree(b);
	}
}

void
print_small_region(task_t task, memory_reader_t reader,
		print_task_printer_t printer, szone_t *szone, int level,
		region_t region, size_t bytes_at_start, size_t bytes_at_end)
{
	unsigned counts[1024];
	unsigned in_use = 0;
	uintptr_t start = (uintptr_t)SMALL_REGION_HEAP_BASE(region);
	uintptr_t current = start + bytes_at_start;
	uintptr_t limit = (uintptr_t)SMALL_REGION_HEAP_END(region) - bytes_at_end;
	uintptr_t mapped_start;
	msize_t msize_and_free;
	msize_t msize;
	unsigned ci;
	_SIMPLE_STRING b;
	uintptr_t pgTot = 0;

	if (reader(task, (vm_address_t)start, SMALL_REGION_SIZE,
			(void **)&mapped_start)) {
		printer("Failed to map small region at %p\n", start);
		return;
	}
	off_t start_offset = mapped_start - start;
	region_t mapped_region = (region_t)mapped_start;

	if (region == HASHRING_REGION_DEALLOCATED) {
		if ((b = _simple_salloc()) != NULL) {
			_simple_sprintf(b, "Small region [unknown address] was returned to the OS\n");
			printer("%s\n", _simple_string(b));
			_simple_sfree(b);
		}
		return;
	}

	memset(counts, 0, sizeof(counts));
	while (current < limit) {
		msize_and_free = *(uintptr_t *)((char *)SMALL_METADATA_FOR_PTR(current) + start_offset);
		msize = msize_and_free & ~SMALL_IS_FREE;
		if (!msize) {
			printer("*** error with %p: msize=%d, free: %x\n", (void *)current,
					(unsigned)msize, msize_and_free & SMALL_IS_FREE);
			break;
		}
		if (!(msize_and_free & SMALL_IS_FREE)) {
			// block in use
			if (msize < 1024) {
				counts[msize]++;
			}
			in_use++;
		} else {
			uintptr_t pgLo = round_page_quanta(current +
					sizeof(free_list_t) + sizeof(msize_t));
			uintptr_t pgHi = trunc_page_quanta(current +
					SMALL_BYTES_FOR_MSIZE(msize) - sizeof(msize_t));

			if (pgLo < pgHi) {
				pgTot += (pgHi - pgLo);
			}
		}
		current += SMALL_BYTES_FOR_MSIZE(msize);
	}

	if ((b = _simple_salloc()) != NULL) {
		mag_index_t mag_index = MAGAZINE_INDEX_FOR_SMALL_REGION(mapped_region);
		_simple_sprintf(b, "Small region %p [%p-%p, %y] \t", region, (void *)start,
				SMALL_REGION_HEAP_END(region), (int)SMALL_REGION_SIZE);
		if (mag_index == DEPOT_MAGAZINE_INDEX) {
			_simple_sprintf(b, "Recirc depot \t");
		} else {
		    _simple_sprintf(b, "Magazine=%d \t", mag_index);
		}
		_simple_sprintf(b, "Allocations in use=%d \t Bytes in use=%ly (%d%%) \t",
				in_use, BYTES_USED_FOR_SMALL_REGION(mapped_region),
				(int)(100.0F * BYTES_USED_FOR_SMALL_REGION(mapped_region))/SMALL_REGION_SIZE);
		if (bytes_at_end || bytes_at_start) {
			_simple_sprintf(b, "Untouched=%ly ", bytes_at_end + bytes_at_start);
		}
		if (mag_index == DEPOT_MAGAZINE_INDEX) {
			_simple_sprintf(b, "Advised MADV_FREE=%ly", pgTot);
		} else {
			_simple_sprintf(b, "Fragments subject to reclamation=%ly", pgTot);
#if CONFIG_RECIRC_DEPOT
			_simple_sprintf(b, small_region_below_recirc_threshold(mapped_region) ?
					"\tEmpty enough to be moved to recirc depot" :
					"\tNot empty enough to be moved to recirc depot");
#endif // CONFIG_RECIRC_DEPOT
		}
		if (level >= MALLOC_VERBOSE_PRINT_LEVEL && in_use) {
			_simple_sappend(b, "\n\tSizes in use: ");
			for (ci = 0; ci < 1024; ci++) {
				if (counts[ci]) {
					_simple_sprintf(b, "%y[%d] ", SMALL_BYTES_FOR_MSIZE(ci),
							counts[ci]);
				}
			}
		}
		printer("%s\n", _simple_string(b));
		_simple_sfree(b);
	}
}

static char *small_freelist_fail_msg = "check: small free list incorrect";

#define SMALL_FREELIST_FAIL(fmt, ...) \
	malloc_zone_check_fail(small_freelist_fail_msg, \
			" (slot=%u), counter=%d\n" fmt,  slot, counter, __VA_ARGS__);

boolean_t
small_free_list_check(rack_t *rack, grain_t slot, unsigned counter)
{
	mag_index_t mag_index;

	for (mag_index = -1; mag_index < rack->num_magazines; mag_index++) {
		magazine_t *small_mag_ptr = &(rack->magazines[mag_index]);
		SZONE_MAGAZINE_PTR_LOCK(small_mag_ptr);

		unsigned count = 0;
		free_list_t current = rack->magazines[mag_index].mag_free_list[slot];
		free_list_t previous = (free_list_t){ .p = NULL };
		msize_t msize_and_free;
		void *ptr = NULL;

		while ((ptr = small_free_list_get_ptr(current))) {
			msize_and_free = *SMALL_METADATA_FOR_PTR(ptr);
			if (!(msize_and_free & SMALL_IS_FREE)) {
				SMALL_FREELIST_FAIL("*** in-use ptr in free list slot=%u count=%d ptr=%p\n", slot, count, ptr);
				SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
				return 0;
			}
			if (((uintptr_t)ptr) & (SMALL_QUANTUM - 1)) {
				SMALL_FREELIST_FAIL("*** unaligned ptr in free list slot=%u count=%d ptr=%p\n", slot, count, ptr);
				SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
				return 0;
			}
			if (!small_region_for_ptr_no_lock(rack, ptr)) {
				SMALL_FREELIST_FAIL("*** ptr not in szone slot=%d count=%d ptr=%p\n", slot, count, ptr);
				SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
				return 0;
			}
			if (small_free_list_get_previous(rack, current).p != previous.p) {
				SMALL_FREELIST_FAIL("*** previous incorrectly set slot=%u count=%d ptr=%p\n", slot, count, ptr);
				SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
				return 0;
			}
			previous = current;
			current = small_free_list_get_next(rack, current);
			count++;
		}

		SZONE_MAGAZINE_PTR_UNLOCK(small_mag_ptr);
	}
	return 1;
}
