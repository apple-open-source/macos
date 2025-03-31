/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include "pgm_malloc.h"

#include <TargetConditionals.h>
#include <mach/mach_time.h>  // mach_absolute_time()

#include "internal.h"


#pragma mark -
#pragma mark Types and Structures

static const char * const slot_state_labels[] = {
	"unused", "allocated", "freed"
};

typedef enum {
	ss_unused,
	ss_allocated,
	ss_freed
} slot_state_t;

MALLOC_STATIC_ASSERT(ss_unused == 0, "unused encoded with 0");
MALLOC_STATIC_ASSERT(ss_freed < (1 << 2), "enum encodable with 2 bits");

typedef struct {
	slot_state_t state : 2;
	uint32_t metadata : 30; // metadata << slots, so borrowing 2 bits here is okay.
	uint16_t size;
	uint16_t offset;
} slot_t;

MALLOC_STATIC_ASSERT(PAGE_MAX_SIZE <= UINT16_MAX, "16 bits for page offsets");
MALLOC_STATIC_ASSERT(sizeof(slot_t) == 8, "slot_t size");

typedef struct MALLOC_PACKED {
	uint64_t thread_id;
	uint64_t time;
	uint16_t trace_size;
} alloc_info_t;

MALLOC_STATIC_ASSERT(sizeof(alloc_info_t) == 18, "alloc_info_t size (packed)");

typedef struct {
	uint32_t slot;
	alloc_info_t alloc;
	alloc_info_t dealloc;
	uint8_t trace_buffer[216];  // ~62 frames (~3.5 bytes per 8-byte pointer)
} metadata_t;

MALLOC_STATIC_ASSERT(sizeof(((metadata_t *)NULL)->trace_buffer) <= UINT16_MAX,
		"16 bits for trace buffer size");
MALLOC_STATIC_ASSERT(sizeof(metadata_t) == 256, "metadata_t size");

typedef struct {
	// Malloc zone
	malloc_zone_t malloc_zone;
	malloc_zone_t *wrapped_zone;

	// Configuration
	uint32_t num_slots;
	uint32_t max_allocations;
	uint32_t max_metadata;
	uint32_t sample_counter_range;
	uint32_t left_align_pct;
	bool debug;
	uint64_t debug_log_throttle_ms;

	// Quarantine
	size_t size;
	vm_address_t begin;
	vm_address_t end;
	size_t region_size;
	vm_address_t region_begin;

	// Metadata
	slot_t *slots;
	metadata_t *metadata;
	uint8_t padding[PAGE_MAX_SIZE];

	// Mutable state
	_malloc_lock_s lock;
	uint32_t num_allocations;
	uint32_t num_metadata;
	uint32_t rr_slot_index;

	// Statistics
	size_t size_in_use;
	size_t max_size_in_use;
	uint64_t last_log_time;
} pgm_zone_t;

ASSERT_WRAPPER_ZONE(pgm_zone_t);

MALLOC_STATIC_ASSERT(__offsetof(pgm_zone_t, padding) < PAGE_MAX_SIZE,
		"First page is mapped read-only");
MALLOC_STATIC_ASSERT(__offsetof(pgm_zone_t, lock) >= PAGE_MAX_SIZE,
		"Mutable state is on separate page");
MALLOC_STATIC_ASSERT(sizeof(pgm_zone_t) < (2 * PAGE_MAX_SIZE),
		"Zone fits on 2 pages");


#pragma mark -
#pragma mark Thread Local Sample Counter

MALLOC_STATIC_ASSERT(sizeof(void *) >= sizeof(uint32_t), "Pointer is used as 32bit counter");

#define TSD_GET_COUNTER() ((uint32_t)_pthread_getspecific_direct(__TSD_MALLOC_PROB_GUARD_SAMPLE_COUNTER))
#define TSD_SET_COUNTER(val) _pthread_setspecific_direct(__TSD_MALLOC_PROB_GUARD_SAMPLE_COUNTER, (void *)(uintptr_t)val)

static const uint32_t k_no_sample = UINT32_MAX;

void
pgm_thread_set_disabled(bool disabled)
{
	if (disabled) {
		TSD_SET_COUNTER(k_no_sample);
	} else {
		TSD_SET_COUNTER(0);
	}
}


#pragma mark -
#pragma mark Decider Functions

// The "decider" functions are performance critical.  They should be inlinable and must not lock.

MALLOC_ALWAYS_INLINE
static inline boolean_t
is_full(const pgm_zone_t *zone)
{
	return zone->num_allocations == zone->max_allocations;
}

static uint32_t rand_uniform(uint32_t upper_bound);

#ifndef PGM_MOCK_SHOULD_SAMPLE_COUNTER
MALLOC_ALWAYS_INLINE
static inline boolean_t
should_sample_counter(uint32_t counter_range)
{
	uint32_t counter = TSD_GET_COUNTER();
	if (counter == k_no_sample) {
		return false;
	}
	// 0 -> regenerate counter; 1 -> sample allocation
	if (counter == 0) {
		counter = rand_uniform(counter_range);
	} else {
		counter--;
	}
	TSD_SET_COUNTER(counter);
	return counter == 0;
}
#endif

MALLOC_ALWAYS_INLINE
static inline boolean_t
should_sample(pgm_zone_t *zone, size_t size)
{
	boolean_t good_size = (size <= PAGE_SIZE);
	boolean_t not_full = !is_full(zone); // Optimization: racy check; we check again in allocate() for correctness.
	return good_size && not_full && should_sample_counter(zone->sample_counter_range);
}

MALLOC_ALWAYS_INLINE
static inline boolean_t
is_guarded(const pgm_zone_t *zone, vm_address_t addr)
{
	return zone->begin <= addr && addr < zone->end;
}


#pragma mark -
#pragma mark Slot <-> Address Mapping

// Prevent overflows on 32 bit platforms (sizeof(size_t) == sizeof(uint32_t)).
static const uint32_t k_max_slots = (MALLOC_TARGET_64BIT) ?
		UINT32_MAX :
		(UINT32_MAX / PAGE_MAX_SIZE - 1) / 2;  // reverse of quarantine_size()

static size_t
quarantine_size(uint32_t num_slots)
{
	MALLOC_ASSERT(num_slots <= k_max_slots);
	return (2 * num_slots + 1) * PAGE_SIZE;
}

static vm_address_t
page_addr(const pgm_zone_t *zone, uint32_t slot)
{
	MALLOC_ASSERT(slot < zone->num_slots);
	uint32_t page = 1 + 2 * slot;
	vm_offset_t offset = page * PAGE_SIZE;
	return zone->begin + offset;
}

static vm_address_t
block_addr(const pgm_zone_t *zone, uint32_t slot) {
	vm_address_t page = page_addr(zone, slot);
	uint16_t offset = zone->slots[slot].offset;
	return page + offset;
}

static uint32_t
page_idx(const pgm_zone_t *zone, vm_address_t addr)
{
	MALLOC_ASSERT(is_guarded(zone, addr));
	vm_offset_t offset = addr - zone->begin;
	return (uint32_t)(offset / PAGE_SIZE);
}

static boolean_t
is_guard_page(const pgm_zone_t *zone, vm_address_t addr)
{
	return page_idx(zone, addr) % 2 == 0;
}


#pragma mark -
#pragma mark Slot Lookup

static uint32_t
nearest_slot(const pgm_zone_t *zone, vm_address_t addr)
{
	if (addr < (zone->begin + PAGE_SIZE)) {
		return 0;
	}
	if (addr >= (zone->end - PAGE_SIZE)) {
		return zone->num_slots - 1;
	}

	uint32_t page = page_idx(zone, addr);
	uint32_t slot = (page - 1) / 2;
	boolean_t guard_page = is_guard_page(zone, addr);
	boolean_t right_half = ((addr % PAGE_SIZE) >= (PAGE_SIZE / 2));

	if (guard_page && right_half) {
		slot++; // Round up.
	}
	return slot;
}

typedef enum {
	b_block_addr,			// Canonical block address.
	b_valid,					// Address within block.
	b_oob_slot,				// Outside block, but within slot.
	b_oob_guard_page	// Guard page.
} bounds_status_t;

typedef struct {
	uint32_t slot;
	bounds_status_t bounds : 31;
	boolean_t live_block_addr : 1; // Canonical block address for live allocation.
} slot_lookup_t;

MALLOC_STATIC_ASSERT(sizeof(slot_lookup_t) == 8, "slot_lookup_t size");

static slot_lookup_t
lookup_slot(const pgm_zone_t *zone, vm_address_t addr)
{
	MALLOC_ASSERT(is_guarded(zone, addr));
	MALLOC_ASSERT(zone->begin % PAGE_SIZE == 0);

	uint32_t slot = nearest_slot(zone, addr);
	uint16_t offset = (addr % PAGE_SIZE);
	uint16_t begin = zone->slots[slot].offset;
	uint16_t end = begin + zone->slots[slot].size;

	bounds_status_t bounds;
	if (is_guard_page(zone, addr)) {
		bounds = b_oob_guard_page;
	} else if (offset == begin) {
		bounds = b_block_addr;
	} else if (begin < offset && offset < end) {
		bounds = b_valid;
	} else {
		bounds = b_oob_slot;
	}

	boolean_t live_slot = (zone->slots[slot].state == ss_allocated);
	return (slot_lookup_t){
		.slot = slot,
		.bounds = bounds,
		.live_block_addr = (live_slot && bounds == b_block_addr)
	};
}


#pragma mark -
#pragma mark Allocator Helpers

// Darwin ABI requires 16 byte alignment.
static const size_t k_min_alignment = 16;

static bool
is_power_of_2(size_t n) {
	return __builtin_popcountl(n) == 1;
}

static size_t
block_size(size_t size)
{
	if (size == 0) {
		return k_min_alignment;
	}
	const size_t mask = (k_min_alignment - 1);
	return (size + mask) & ~mask;
}

// Current implementation: round-robin; delays reuse until at least (num_slots - max_allocations).
// Possible alternatives: LRU, random.
static uint32_t
choose_available_slot(pgm_zone_t *zone)
{
	uint32_t slot = zone->rr_slot_index;
	while (zone->slots[slot].state == ss_allocated) {
		slot = (slot + 1) % zone->num_slots;
	}
	// Delay reuse if immediately freed.
	zone->rr_slot_index = (slot + 1) % zone->num_slots;
	return slot;
}

static uint32_t
choose_metadata(pgm_zone_t *zone, uint32_t slot)
{
	if (zone->num_metadata < zone->max_metadata) {
		return zone->num_metadata++;
	}

	uint32_t old_index = zone->slots[slot].metadata;
	if (zone->metadata[old_index].slot == slot) {
		return old_index;
	}

	while (true) {
		uint32_t index = rand_uniform(zone->max_metadata);
		uint32_t s = zone->metadata[index].slot;
		if (zone->slots[s].state == ss_freed) {
			return index;
		}
	}
}

static uint16_t
choose_offset_on_page(size_t size,
		size_t alignment,
		uint32_t left_align_pct,
		uint16_t page_size)
{
	MALLOC_ASSERT(size <= page_size);
	MALLOC_ASSERT(alignment <= page_size && is_power_of_2(alignment));
	MALLOC_ASSERT(is_power_of_2(page_size));
	MALLOC_ASSERT(left_align_pct <= 100);

	if (rand_uniform(100) < left_align_pct) {
		return 0;
	}
	size_t mask = ~(alignment - 1);
	return (page_size - size) & mask;
}

MALLOC_ALWAYS_INLINE
static inline size_t my_trace_collect(uint8_t *buffer, size_t size);

MALLOC_ALWAYS_INLINE
static inline void capture_trace(metadata_t *metadata, bool alloc)
{
	alloc_info_t *info = alloc ? &metadata->alloc : &metadata->dealloc;
	info->thread_id = _pthread_threadid_self_np_direct();
	info->time = mach_absolute_time();

	size_t offset;
	if (alloc) {
		offset = 0;
		metadata->dealloc = (alloc_info_t){};
	} else {
		offset = MIN(metadata->alloc.trace_size, sizeof(metadata->trace_buffer) / 2);
		metadata->alloc.trace_size = offset;
	}
	uint8_t *buffer = &metadata->trace_buffer[offset];
	size_t size = sizeof(metadata->trace_buffer) - offset;
	info->trace_size = my_trace_collect(buffer, size);
}


#pragma mark -
#pragma mark Allocator Functions

static void mark_inaccessible(vm_address_t page);
static void mark_read_write(vm_address_t page);
static void debug_zone(pgm_zone_t *zone, const char *label, vm_address_t addr);

// Note: the functions below require locking.

static size_t
lookup_size(pgm_zone_t *zone, vm_address_t addr)
{
	slot_lookup_t res = lookup_slot(zone, addr);
	if (!res.live_block_addr) {
		return 0;
	}
	return zone->slots[res.slot].size;
}

static vm_address_t
allocate(pgm_zone_t *zone, size_t size, size_t alignment)
{
	MALLOC_ASSERT(size <= PAGE_SIZE);
	MALLOC_ASSERT(k_min_alignment <= alignment && alignment <= PAGE_SIZE);
	MALLOC_ASSERT(is_power_of_2(alignment));

	if (is_full(zone)) {
		return (vm_address_t)NULL;
	}

	size = block_size(size);
	uint32_t slot = choose_available_slot(zone);
	uint32_t metadata = choose_metadata(zone, slot);
	uint16_t offset = choose_offset_on_page(
			size, alignment, zone->left_align_pct, PAGE_SIZE);

	// Mark page writable before updating metadata.  Ensures metadata is correct
	// whenever a fault could be triggered.
	vm_address_t page = page_addr(zone, slot);
	mark_read_write(page);

	zone->slots[slot] = (slot_t){
		.state = ss_allocated,
		.metadata = metadata,
		.size = size,
		.offset = offset
	};
	zone->metadata[metadata].slot = slot;
	capture_trace(&zone->metadata[metadata], /*alloc=*/true);

	zone->num_allocations++;
	zone->size_in_use += size;
	zone->max_size_in_use = MAX(zone->size_in_use, zone->max_size_in_use);

	vm_address_t addr = page + offset;
	debug_zone(zone, "allocated", addr);

	return addr;
}

static void
deallocate(pgm_zone_t *zone, vm_address_t addr)
{
	slot_lookup_t res = lookup_slot(zone, addr);
	if (!res.live_block_addr) {
		// TODO(yln): error report; TODO(yln): distinguish between most likely cause
		// corrupted pointer (unused, *) or (*, !block_ptr) and double free (freed, block_ptr)
		MALLOC_REPORT_FATAL_ERROR(addr, "ProbGuard: invalid pointer passed to free");
	}

	uint32_t slot = res.slot;
	uint32_t metadata = zone->slots[slot].metadata;

	zone->slots[slot].state = ss_freed;
	capture_trace(&zone->metadata[metadata], /*alloc=*/false);

	zone->num_allocations--;
	zone->size_in_use -= zone->slots[slot].size;

	// Mark page inaccessible after updating metadata.  Ensures metadata is
	// correct whenever a fault could be triggered.
	vm_address_t page = page_addr(zone, slot);
	mark_inaccessible(page);

	debug_zone(zone, "freed", addr);
}

#define DELEGATE(function, args...) \
	zone->wrapped_zone->function(zone->wrapped_zone, args)

static vm_address_t
reallocate(pgm_zone_t *zone, vm_address_t addr, size_t new_size, boolean_t sample)
{
	boolean_t guarded = is_guarded(zone, addr);
	// Note: should_sample() is stateful.
	MALLOC_ASSERT(guarded || sample);

	size_t size;
	if (guarded) {
		size = lookup_size(zone, addr);
	} else {
		size = DELEGATE(size, (void *)addr);
	}
	if (!size) {
		// TODO(yln): error report
		MALLOC_REPORT_FATAL_ERROR(addr, "ProbGuard: invalid pointer passed to realloc");
	}

	vm_address_t new_addr;
	if (sample && !is_full(zone)) {
		new_addr = allocate(zone, new_size, k_min_alignment);
		MALLOC_ASSERT(new_addr);
	} else {
		new_addr = (vm_address_t)DELEGATE(malloc, new_size);
		if (!new_addr) {
			return (vm_address_t)NULL;
		}
	}
	memcpy((void *)new_addr, (void *)addr, MIN(size, new_size));

	if (guarded) {
		deallocate(zone, addr);
	} else {
		DELEGATE(free, (void *)addr);
	}
	return new_addr;
}


#pragma mark -
#pragma mark Lock Helpers

static void init_lock(pgm_zone_t *zone) { _malloc_lock_init(&zone->lock); }
static void lock(pgm_zone_t *zone) { _malloc_lock_lock(&zone->lock); }
static void unlock(pgm_zone_t *zone) { _malloc_lock_unlock(&zone->lock); }
static boolean_t trylock(pgm_zone_t *zone) { return _malloc_lock_trylock(&zone->lock); }


#pragma mark -
#pragma mark Zone Functions

#define DELEGATE_UNSAMPLED(size, function, args...) \
	if (os_likely(!should_sample(zone, size))) \
		return DELEGATE(function, args)

#define DELEGATE_UNGUARDED(ptr, function, args...) \
	if (os_likely(!is_guarded(zone, (vm_address_t)ptr))) \
		return DELEGATE(function, args)

#define SAMPLED_ALLOCATE(size, alignment, function, args...) \
	DELEGATE_UNSAMPLED(size, function, args); \
	lock(zone); \
	void *ptr = (void *)allocate(zone, size, alignment); \
	unlock(zone); \
	if (!ptr) return DELEGATE(function, args)

#define GUARDED_DEALLOCATE(ptr, function, args...) \
	DELEGATE_UNGUARDED(ptr, function, args); \
	lock(zone); \
	deallocate(zone, (vm_address_t)ptr); \
	unlock(zone)


static size_t
pgm_size(pgm_zone_t *zone, const void *ptr)
{
	DELEGATE_UNGUARDED(ptr, size, ptr);
	lock(zone);
	size_t size = lookup_size(zone, (vm_address_t)ptr);
	unlock(zone);
	return size;
}

static void *
pgm_malloc(pgm_zone_t *zone, size_t size)
{
	SAMPLED_ALLOCATE(size, k_min_alignment, malloc, size);
	return ptr;
}

static void *
pgm_malloc_type_malloc(pgm_zone_t *zone, size_t size, malloc_type_id_t type_id)
{
	SAMPLED_ALLOCATE(size, k_min_alignment, malloc_type_malloc, size, type_id);
	return ptr;
}

static void *
pgm_calloc(pgm_zone_t *zone, size_t num_items, size_t size)
{
	size_t total_size;
	if (os_unlikely(os_mul_overflow(num_items, size, &total_size))) {
		return DELEGATE(calloc, num_items, size);
	}
	SAMPLED_ALLOCATE(total_size, k_min_alignment, calloc, num_items, size);
	bzero(ptr, total_size);
	return ptr;
}

static void *
pgm_malloc_type_calloc(pgm_zone_t *zone, size_t num_items, size_t size,
		malloc_type_id_t type_id)
{
	size_t total_size;
	if (os_unlikely(os_mul_overflow(num_items, size, &total_size))) {
		return DELEGATE(malloc_type_calloc, num_items, size, type_id);
	}
	SAMPLED_ALLOCATE(total_size, k_min_alignment, malloc_type_calloc, num_items,
			size, type_id);
	bzero(ptr, total_size);
	return ptr;
}

static void *
pgm_valloc(pgm_zone_t *zone, size_t size)
{
	SAMPLED_ALLOCATE(size, /*alignment=*/PAGE_SIZE, valloc, size);
	return ptr;
}

static void
pgm_free(pgm_zone_t *zone, void *ptr)
{
	GUARDED_DEALLOCATE(ptr, free, ptr);
}

static void *
pgm_realloc(pgm_zone_t *zone, void *ptr, size_t new_size)
{
	if (os_unlikely(!ptr)) {
		return pgm_malloc(zone, new_size);
	}
	boolean_t sample = should_sample(zone, new_size);
	if (os_likely(!sample)) {
		DELEGATE_UNGUARDED(ptr, realloc, ptr, new_size);
	}
	lock(zone);
	void *new_ptr = (void *)reallocate(zone, (vm_address_t)ptr, new_size, sample);
	unlock(zone);
	return new_ptr;
}

static void *
pgm_malloc_type_realloc(pgm_zone_t *zone, void *ptr, size_t new_size,
		malloc_type_id_t type_id)
{
	if (os_unlikely(!ptr)) {
		return pgm_malloc_type_malloc(zone, new_size, type_id);
	}
	boolean_t sample = should_sample(zone, new_size);
	if (os_likely(!sample)) {
		DELEGATE_UNGUARDED(ptr, malloc_type_realloc, ptr, new_size, type_id);
	}
	lock(zone);
	void *new_ptr = (void *)reallocate(zone, (vm_address_t)ptr, new_size, sample);
	unlock(zone);
	return new_ptr;
}

static void my_vm_deallocate(vm_address_t addr, size_t size);
static void
pgm_destroy(pgm_zone_t *zone)
{
	malloc_destroy_zone(zone->wrapped_zone);  // frees slots and metadata
	my_vm_deallocate(zone->region_begin, zone->region_size);
	my_vm_deallocate((vm_address_t)zone, sizeof(pgm_zone_t));
}

static void *
pgm_memalign(pgm_zone_t *zone, size_t alignment, size_t size)
{
	if (os_unlikely(alignment > PAGE_SIZE)) {
		return DELEGATE(memalign, alignment, size);
	}
	size_t adj_alignment = MAX(alignment, k_min_alignment);
	SAMPLED_ALLOCATE(size, adj_alignment, memalign, alignment, size);
	return ptr;
}

static void *
pgm_malloc_type_memalign(pgm_zone_t *zone, size_t alignment, size_t size,
		malloc_type_id_t type_id)
{
	if (os_unlikely(alignment > PAGE_SIZE)) {
		return DELEGATE(malloc_type_memalign, alignment, size, type_id);
	}
	size_t adj_alignment = MAX(alignment, k_min_alignment);
	SAMPLED_ALLOCATE(size, adj_alignment, malloc_type_memalign, alignment, size,
			type_id);
	return ptr;
}

static void
pgm_free_definite_size(pgm_zone_t *zone, void *ptr, size_t size)
{
	GUARDED_DEALLOCATE(ptr, free_definite_size, ptr, size);
}

static boolean_t
pgm_claimed_address(pgm_zone_t *zone, void *ptr)
{
	DELEGATE_UNGUARDED(ptr, claimed_address, ptr);
	return true;
}

static void *
pgm_malloc_with_options(pgm_zone_t *zone, size_t align, size_t size,
		uint64_t options)
{
	if (os_unlikely(align > PAGE_SIZE)) {
		return DELEGATE(malloc_with_options, align, size, options);
	}
	size_t adj_alignment = MAX(align, k_min_alignment);
	SAMPLED_ALLOCATE(size, adj_alignment, malloc_with_options, align, size, options);
	if (options & MALLOC_NP_OPTION_CLEAR) {
		bzero(ptr, size);
	}
	return ptr;
}

static void *
pgm_malloc_type_malloc_with_options(pgm_zone_t *zone, size_t align, size_t size,
		uint64_t options, malloc_type_id_t type_id)
{
	if (os_unlikely(align > PAGE_SIZE)) {
		return DELEGATE(malloc_type_malloc_with_options, align, size, options,
				type_id);
	}
	size_t adj_alignment = MAX(align, k_min_alignment);
	SAMPLED_ALLOCATE(size, adj_alignment, malloc_type_malloc_with_options,
			align, size, options, type_id);
	if (options & MALLOC_NP_OPTION_CLEAR) {
		bzero(ptr, size);
	}
	return ptr;
}

#pragma mark -
#pragma mark Integrity Checks

static const size_t k_zone_spacer = 32 * 1024 * 1024;  // 32 MB

static bool
check_configuration(const pgm_zone_t *zone)
{
	// 0 < max_allocations << max_metadata <= num_slots <= k_max_slots
	return (0 < zone->max_allocations) &&
			(zone->max_allocations <= zone->max_metadata / 2) &&  // choose_metadata() relies on max_allocations << max_metadata
			(zone->max_metadata <= zone->num_slots) &&
			(zone->num_slots <= k_max_slots) &&
			(zone->sample_counter_range > 0) &&
			(0 <= zone->left_align_pct && zone->left_align_pct <= 100);
}

static bool
check_zone(const pgm_zone_t *zone)
{
	return check_configuration(zone) &&
			// Quarantine
			(zone->size == quarantine_size(zone->num_slots)) &&
			(zone->begin % PAGE_SIZE == 0) &&
			(zone->size % PAGE_SIZE == 0) &&
			(zone->begin + zone->size == zone->end) &&
			(zone->begin < zone->end) &&
			(zone->region_size == 2 * k_zone_spacer + zone->size) &&
			(zone->region_begin == zone->begin - k_zone_spacer) &&
			(zone->region_begin < zone->begin) &&
			// Mutable state
			(zone->num_allocations <= zone->max_allocations) &&
			(zone->num_metadata <= zone->max_metadata) &&
			(zone->num_allocations <= zone->num_metadata) &&
			(zone->rr_slot_index < zone->num_slots) &&
			// Metadata
			(zone->slots && zone->metadata) &&
			// Statistics
			(zone->size_in_use <= zone->max_size_in_use);
}

static bool
check_slot(const pgm_zone_t *zone, const slot_t *slot)
{
	if (slot->state == ss_unused) {
		return true;
	}
	return (slot->state <= ss_freed) &&
			(slot->metadata < zone->num_metadata) &&
			(slot->size <= PAGE_SIZE) &&
			(slot->size == block_size(slot->size)) &&
			(slot->offset <= PAGE_SIZE) &&
			(slot->offset % k_min_alignment == 0) &&
			((size_t)slot->offset + slot->size <= PAGE_SIZE);
}

static bool
check_slots(const pgm_zone_t *zone)
{
	uint32_t num_allocations = 0;
	size_t size_in_use = 0;

	for (uint32_t i = 0; i < zone->num_slots; i++) {
		slot_t *slot = &zone->slots[i];
		if (!check_slot(zone, slot)) {
			return false;
		}
		if (slot->state == ss_allocated) {
			num_allocations++;
			size_in_use += zone->slots[i].size;
		}
	}

	return (num_allocations == zone->num_allocations) &&
			(size_in_use == zone->size_in_use);
}

static bool
check_md(const pgm_zone_t *zone, const metadata_t *md)
{
	ptrdiff_t index = md - zone->metadata;
	return (md->slot < zone->num_slots) &&
			(zone->slots[md->slot].metadata == index) &&
			((size_t)md->alloc.trace_size + md->dealloc.trace_size <= sizeof(md->trace_buffer));
}

static bool
check_metadata(const pgm_zone_t *zone)
{
	for (uint32_t i = 0; i < zone->num_metadata; i++) {
		metadata_t *metadata = &zone->metadata[i];
		if (!check_md(zone, metadata)) {
			return false;
		}
	}
	return true;
}


#pragma mark -
#pragma mark Introspection Functions

typedef enum {
	rt_zone_only = 1 << 0,
	rt_slots     = 1 << 1,
	rt_metadata  = 1 << 2,
} read_type_t;

#define READ(remote_address, size, local_memory, checker, check_data) \
{ \
	kern_return_t kr = reader(task, (vm_address_t)remote_address, size, (void **)local_memory); \
	if (kr != KERN_SUCCESS) return kr; \
	if (!checker(check_data)) return KERN_FAILURE; \
}

static kern_return_t
read_zone(task_t task, vm_address_t zone_address, memory_reader_t reader, pgm_zone_t *zone, read_type_t read_type)
{
	reader = reader_or_in_memory_fallback(reader, task);

	pgm_zone_t *zone_ptr;
	READ(zone_address, sizeof(pgm_zone_t), &zone_ptr, check_zone, zone_ptr);
	*zone = *zone_ptr;  // Copy to writable memory

	if (read_type & rt_slots) {
		READ(zone->slots, zone->num_slots * sizeof(slot_t), &zone->slots, check_slots, zone);
	}
	if (read_type & rt_metadata) {
		READ(zone->metadata, zone->max_metadata * sizeof(metadata_t), &zone->metadata, check_metadata, zone);
	}
	return KERN_SUCCESS;
}

#define READ_ZONE(zone, read_type) \
	pgm_zone_t zone_copy; \
	{ \
		kern_return_t kr = read_zone(task, zone_address, reader, &zone_copy, read_type); \
		if (kr != KERN_SUCCESS) return kr; \
	} \
	const pgm_zone_t *zone = &zone_copy;

#define RECORD(remote_address, size_, type) \
{ \
	vm_range_t range = { .address = remote_address, .size = size_ }; \
	recorder(task, context, type, &range, /*count=*/1); \
}

static kern_return_t
pgm_enumerator(task_t task, void *context, unsigned type_mask,
		vm_address_t zone_address, memory_reader_t reader,
		vm_range_recorder_t recorder)
{
	boolean_t record_allocs = (type_mask & MALLOC_PTR_IN_USE_RANGE_TYPE);
	boolean_t record_regions = (type_mask & MALLOC_PTR_REGION_RANGE_TYPE);
	if (!record_allocs && !record_regions) {
		return KERN_SUCCESS;
	}

	READ_ZONE(zone, rt_slots);

	for (uint32_t i = 0; i < zone->num_slots; i++) {
		if (zone->slots[i].state != ss_allocated) {
			continue;
		}
		// TODO(yln): we could do these in bulk.  Currently, it shouldn't matter
		// since the number of active slots (bounded by max_allocations) is small.
		// If we optimize our allocator (to prevent wasting a page per allocation)
		// and this allows us to significantly grow the number of allocations, then
		// we should change the code here to record in chunks.
		if (record_regions) {
			vm_address_t page = page_addr(zone, i);
			RECORD(page, PAGE_SIZE, MALLOC_PTR_REGION_RANGE_TYPE);
		}
		if (record_allocs) {
			vm_address_t alloc = block_addr(zone, i);
			RECORD(alloc, zone->slots[i].size, MALLOC_PTR_IN_USE_RANGE_TYPE);
		}
	}
	return KERN_SUCCESS;
}

static void
pgm_statistics(const pgm_zone_t *zone, malloc_statistics_t *stats)
{
	*stats = (malloc_statistics_t){
		.blocks_in_use = zone->num_allocations,
		.size_in_use = zone->size_in_use,
		.max_size_in_use = zone->max_size_in_use,
		.size_allocated = zone->num_allocations * PAGE_SIZE
	};
}

static kern_return_t
pgm_statistics_task(task_t task, vm_address_t zone_address, memory_reader_t reader, malloc_statistics_t *stats)
{
	READ_ZONE(zone, rt_zone_only);
	pgm_statistics(zone, stats);
	return KERN_SUCCESS;
}

static void
print_zone(pgm_zone_t *zone, boolean_t verbose, print_task_printer_t printer) {
	malloc_statistics_t stats;
	pgm_statistics(zone, &stats);
	printer("ProbGuard zone: slots: %u, slots in use: %u, size in use: %llu, max size in use: %llu, allocated size: %llu\n",
					zone->num_slots, stats.blocks_in_use, stats.size_in_use, stats.max_size_in_use, stats.size_allocated);
	printer("Quarantine: size: %llu, address range: [%p - %p)\n", zone->size, zone->begin, zone->end);

	printer("Slots (#, state, offset, size, block address):\n");
	for (uint32_t i = 0; i < zone->num_slots; i++) {
		slot_state_t state = zone->slots[i].state;
		if (state != ss_allocated && !verbose) {
			continue;
		}
		const char *label = slot_state_labels[state];
		uint16_t offset = zone->slots[i].offset;
		uint16_t size = zone->slots[i].size;
		vm_address_t block = block_addr(zone, i);
		printer("%4u, %9s, %4u, %4u, %p\n", i, label, offset, size, block);
	}
}


static void
pgm_print(pgm_zone_t *zone, boolean_t verbose)
{
	print_zone(zone, verbose, malloc_report_simple);
}

static void
pgm_print_task(task_t task, unsigned level, vm_address_t zone_address, memory_reader_t reader, print_task_printer_t printer)
{
	pgm_zone_t zone;
	kern_return_t kr = read_zone(task, zone_address, reader, &zone, rt_slots);
	if (kr != KERN_SUCCESS) {
		printer("Failed to read ProbGuard zone at %p\n", zone_address);
		return;
	}

	boolean_t verbose = (level >= MALLOC_VERBOSE_PRINT_LEVEL);
	print_zone(&zone, verbose, printer);
}

static void
pgm_log(pgm_zone_t *zone, void *address)
{
	// Unsupported.
}

static size_t
pgm_good_size(pgm_zone_t *zone, size_t size)
{
	return DELEGATE(introspect->good_size, size);
}

static boolean_t
pgm_check(pgm_zone_t *zone)
{
	return check_zone(zone) && check_slots(zone) && check_metadata(zone);
}

static void
pgm_force_lock(pgm_zone_t *zone)
{
	lock(zone);
}

static void
pgm_force_unlock(pgm_zone_t *zone)
{
	unlock(zone);
}

static void
pgm_reinit_lock(pgm_zone_t *zone)
{
	init_lock(zone);
}

static boolean_t
pgm_zone_locked(pgm_zone_t *zone)
{
	boolean_t lock_taken = trylock(zone);
	if (lock_taken) {
		unlock(zone);
	}
	return !lock_taken;
}


#pragma mark -
#pragma mark Zone Templates

#define PGM_ZONE_VERSION 16
#define MIN_WRAPPED_ZONE_VERSION 16

// Suppress warning: incompatible function pointer types
#define FN_PTR(fn) (void *)(&fn)

static const malloc_introspection_t introspection_template = {
	// Block and region enumeration
	.enumerator = FN_PTR(pgm_enumerator),

	// Statistics
	.statistics = FN_PTR(pgm_statistics),
	.task_statistics = FN_PTR(pgm_statistics_task),

	// Logging
	.print = FN_PTR(pgm_print),
	.print_task = FN_PTR(pgm_print_task),
	.log = FN_PTR(pgm_log),

	// Queries
	.good_size = FN_PTR(pgm_good_size),
	.check = FN_PTR(pgm_check),

	// Locking
	.force_lock = FN_PTR(pgm_force_lock),
	.force_unlock = FN_PTR(pgm_force_unlock),
	.reinit_lock = FN_PTR(pgm_reinit_lock),
	.zone_locked = FN_PTR(pgm_zone_locked),

	// Discharge checking
	.enable_discharge_checking = NULL,
	.disable_discharge_checking = NULL,
	.discharge = NULL,
#ifdef __BLOCKS__
	.enumerate_discharged_pointers = NULL,
#else
	.enumerate_unavailable_without_blocks = NULL,
#endif

	// Zone type
	.zone_type = MALLOC_ZONE_TYPE_PGM,
};

static const malloc_zone_t malloc_zone_template = {
	// Reserved for CFAllocator
	.reserved1 = NULL,
	.reserved2 = NULL,

	// Standard operations
	.size = FN_PTR(pgm_size),
	.malloc = FN_PTR(pgm_malloc),
	.calloc = FN_PTR(pgm_calloc),
	.valloc = FN_PTR(pgm_valloc),
	.free = FN_PTR(pgm_free),
	.realloc = FN_PTR(pgm_realloc),
	.destroy = FN_PTR(pgm_destroy),

	// Batch operations
	.batch_malloc = malloc_zone_batch_malloc_fallback,
	.batch_free = malloc_zone_batch_free_fallback,

	// Introspection
	.zone_name = "ProbGuardMallocZone",
	.version = PGM_ZONE_VERSION,
	.introspect = (malloc_introspection_t *)&introspection_template, // Effectively const.

	// Specialized operations
	.memalign = FN_PTR(pgm_memalign),
	.free_definite_size = FN_PTR(pgm_free_definite_size),
	.pressure_relief = malloc_zone_pressure_relief_fallback,
	.claimed_address = FN_PTR(pgm_claimed_address),
	.try_free_default = NULL,
	.malloc_with_options = FN_PTR(pgm_malloc_with_options),

	// Typed entrypoints
	.malloc_type_malloc = FN_PTR(pgm_malloc_type_malloc),
	.malloc_type_calloc = FN_PTR(pgm_malloc_type_calloc),
	.malloc_type_realloc = FN_PTR(pgm_malloc_type_realloc),
	.malloc_type_memalign = FN_PTR(pgm_malloc_type_memalign),
	.malloc_type_malloc_with_options =
			FN_PTR(pgm_malloc_type_malloc_with_options),
};


#pragma mark -
#pragma mark Configuration Options

static const char *
env_var(const char *name)
{
	return getenv(name);
}

static uint32_t
env_uint(const char *name, uint32_t default_value) {
	const char *value = env_var(name);
	if (!value) return default_value;
	return (uint32_t)strtoul(value, NULL, 0);
}

static boolean_t
env_bool(const char *name) {
	const char *value = env_var(name);
	if (!value) return FALSE;
	return value[0] == '1';
}

#if CONFIG_FEATUREFLAGS_SIMPLE
# define FEATURE_FLAG(feature, default) os_feature_enabled_simple(libmalloc, feature, default)
#else
# define FEATURE_FLAG(feature, default) (default)
#endif


#pragma mark -
#pragma mark Zone Configuration

static struct {
	bool internal_build;
	bool MallocProbGuard_is_set;
	bool MallocProbGuard;
	bool targeted_coverage;
} g_env;

void
pgm_init_config(bool internal_build)
{
	// Avoid dirty memory; do not write in the common case
	if (internal_build) {
		g_env.internal_build = internal_build;
	}
	if (env_var("MallocProbGuard")) {
		g_env.MallocProbGuard_is_set = true;
		g_env.MallocProbGuard = env_bool("MallocProbGuard");
	}

	const char *all_factors = env_var("__TRIFactors");
	const char *pgm_factor =
			"PROBABILISTIC_GUARD_MALLOC_TARGETED_COVERAGE:enable=1";
	if (all_factors && strstr(all_factors, pgm_factor)) {
		g_env.targeted_coverage = true;
	}
}

static bool
is_platform_binary(void)
{
#if CONFIG_CHECK_PLATFORM_BINARY
	return malloc_is_platform_binary;
#else
	return _malloc_is_platform_binary();
#endif
}

extern bool main_image_has_section(const char* segname, const char *sectname);
static bool
should_activate(bool internal_build)
{
	if (!internal_build && !is_platform_binary()) {
		return false;
	}
	uint32_t activation_rate = (internal_build ? 250 : 1000);
	if (rand_uniform(activation_rate) != 0) {
		return false;
	}
	if (main_image_has_section("__DATA", "__pgm_opt_out")) {
		return false;
	}
	return true;
}

#if TARGET_OS_WATCH || TARGET_OS_TV
static bool
is_low_memory_device(void)
{
	uint64_t low_memory = 1.2 * 1024 * 1024 * 1024;  // 1.2 GB
	return platform_hw_memsize() <= low_memory;
}
#endif

bool
pgm_should_enable(void)
{
	// Env var override
	if (g_env.MallocProbGuard_is_set) {
		return g_env.MallocProbGuard;
	}

	// Feature flags
	if (!FEATURE_FLAG(ProbGuard, true)) {
		return false;
	}
	if (FEATURE_FLAG(ProbGuardAllProcesses, false)) {
		return true;
	}

	// Excluded configurations
#if TARGET_OS_WATCH || TARGET_OS_TV
	if (is_low_memory_device()) {
		return false;
	}
#elif TARGET_OS_BRIDGE || TARGET_OS_DRIVERKIT
	if (!g_env.internal_build) {
		return false;
	}
#endif

	// Targeted coverage via Trial
	if (g_env.targeted_coverage) {
		return true;
	}

	// Random activation
	return should_activate(g_env.internal_build);
}

static uint32_t
choose_memory_budget_in_kb(void)
{
	return (TARGET_OS_OSX ? 8 : 2) * 1024;
}

static uint32_t
choose_sample_rate(void)
{
	uint32_t min = 500, max = 5000;
	return rand_uniform(max - min) + min;
}

static const double k_slot_multiplier = 10.0;
static const double k_metadata_multiplier = 3.0;
static uint32_t
compute_max_allocations(size_t memory_budget_in_kb)
{
	size_t memory_budget = memory_budget_in_kb * 1024;
	size_t fixed_overhead = round_page(sizeof(pgm_zone_t));
	size_t vm_map_entry_size = 80; // struct vm_map_entry in <vm/vm_map.h>
	size_t per_allocation_overhead =
			PAGE_SIZE +
			k_slot_multiplier * 2 * vm_map_entry_size + // TODO(yln): Implement mark_inaccessible to fill holes so we can drop the k_slot_multiplier here. +27% more protected allocations!
			// 2 * vm_map_entry_size + // Allocations split the VM region
			k_slot_multiplier * sizeof(slot_t) +
			k_metadata_multiplier * sizeof(metadata_t);

	uint32_t max_allocations = (uint32_t)((memory_budget - fixed_overhead) / per_allocation_overhead);
	if (memory_budget < fixed_overhead || max_allocations == 0) {
		MALLOC_REPORT_FATAL_ERROR(0, "ProbGuard: memory budget too small");
	}
	return max_allocations;
}

static void
configure_zone(pgm_zone_t *zone)
{
	uint32_t memory_budget_in_kb = env_uint("MallocProbGuardMemoryBudgetInKB", choose_memory_budget_in_kb());
	zone->max_allocations = env_uint("MallocProbGuardAllocations", compute_max_allocations(memory_budget_in_kb));
	zone->num_slots = env_uint("MallocProbGuardSlots", k_slot_multiplier * zone->max_allocations);
	zone->max_metadata = env_uint("MallocProbGuardMetadata", k_metadata_multiplier * zone->max_allocations);
	uint32_t sample_rate = env_uint("MallocProbGuardSampleRate", choose_sample_rate());
	// Approximate a (1 / sample_rate) chance for sampling; 1 means "always sample".
	zone->sample_counter_range = (sample_rate != 1) ? (2 * sample_rate) : 1;
	zone->left_align_pct = env_uint("MallocProbGuardLeftAlignPercentage", 10);
	zone->debug = env_bool("MallocProbGuardDebug");
	zone->debug_log_throttle_ms = env_uint("MallocProbGuardDebugLogThrottleInMillis", 1000);

	if (zone->debug) {
		malloc_report(ASL_LEVEL_INFO,
				"ProbGuard configuration: %u kB budget, 1/%u sample rate, %u/%u/%u allocations/metadata/slots\n",
				memory_budget_in_kb, sample_rate, zone->max_allocations, zone->max_metadata, zone->num_slots);
	}
	if (!check_configuration(zone)) {
		MALLOC_REPORT_FATAL_ERROR(0, "ProbGuard: bad configuration");
	}
}


#pragma mark -
#pragma mark Zone Creation

#define VM_PROT_READ_WRITE (VM_PROT_READ | VM_PROT_WRITE)

static vm_address_t my_vm_map(size_t size, vm_prot_t protection, int tag);
static void my_vm_map_fixed(vm_address_t addr, size_t size, vm_prot_t protection, int tag);
static void my_vm_deallocate(vm_address_t addr, size_t size);
static void my_vm_protect(vm_address_t addr, size_t size, vm_prot_t protection);

static void
disable_unsupported_apis(malloc_zone_t *pgm_zone, const malloc_zone_t *wrapped_zone)
{
	#define DISABLE_UNSUPPORTED(api) if (!wrapped_zone->api) pgm_zone->api = NULL;

	// In practice, there are no zones we support wrapping right now that don't
	// have these entrypoints
	DISABLE_UNSUPPORTED(memalign)
	DISABLE_UNSUPPORTED(malloc_type_memalign)
	DISABLE_UNSUPPORTED(free_definite_size)
	DISABLE_UNSUPPORTED(claimed_address)

	// These ones are actually load-bearing: the nano and scalable zones do not
	// implement these entrypoints
	DISABLE_UNSUPPORTED(malloc_with_options)
	DISABLE_UNSUPPORTED(malloc_type_malloc_with_options)
}

static void
setup_zone(pgm_zone_t *zone, malloc_zone_t *wrapped_zone)
{
	// Malloc zone
	zone->malloc_zone = malloc_zone_template;
	zone->wrapped_zone = wrapped_zone;
	disable_unsupported_apis(&zone->malloc_zone, wrapped_zone);

	// Configuration
	configure_zone(zone);

	// Quarantine
	zone->size = quarantine_size(zone->num_slots);
	zone->region_size = 2 * k_zone_spacer + zone->size;
	zone->region_begin = my_vm_map(zone->region_size, VM_PROT_NONE, VM_MEMORY_MALLOC);
	zone->begin = zone->region_begin + k_zone_spacer;
	zone->end = zone->begin + zone->size;
	my_vm_map_fixed(zone->begin, zone->size, VM_PROT_NONE, VM_MEMORY_MALLOC_PROB_GUARD);

	// Metadata
	zone->slots = DELEGATE(calloc, zone->num_slots, sizeof(slot_t));
	zone->metadata = DELEGATE(calloc, zone->max_metadata, sizeof(metadata_t));
	MALLOC_ASSERT(zone->slots && zone->metadata);

	// Mutable state
	init_lock(zone);
}

malloc_zone_t *
pgm_create_zone(malloc_zone_t *wrapped_zone)
{
	MALLOC_ASSERT(wrapped_zone->version >= MIN_WRAPPED_ZONE_VERSION);

	pgm_zone_t *zone = (pgm_zone_t *)my_vm_map(sizeof(pgm_zone_t), VM_PROT_READ_WRITE, VM_MEMORY_MALLOC);
	setup_zone(zone, wrapped_zone);
	my_vm_protect((vm_address_t)zone, PAGE_MAX_SIZE, VM_PROT_READ);

	return (malloc_zone_t *)zone;
}


#pragma mark -
#pragma mark Logging

static uint64_t
to_millis(uint64_t mach_ticks)
{
	mach_timebase_info_data_t timebase;
	mach_timebase_info(&timebase);
	const uint64_t nanos_per_ms = 1e6;
	return (mach_ticks * timebase.numer / timebase.denom) / nanos_per_ms;
}

static bool
should_log(pgm_zone_t *zone)
{
	uint64_t now = mach_absolute_time();
	uint64_t delta_ms = to_millis(now - zone->last_log_time);
	boolean_t log = (delta_ms >= zone->debug_log_throttle_ms);
	if (log) {
		zone->last_log_time = now;
	}
	return log;
}

static void
debug_zone(pgm_zone_t *zone, const char *label, vm_address_t addr)
{
	if (!zone->debug) {
		return;
	}
	if (should_log(zone)) {
		malloc_report(ASL_LEVEL_INFO,
				"ProbGuard: %9s 0x%llx, fill state: %3u/%u\n", label,
				(unsigned long long)addr, zone->num_allocations,
				zone->max_allocations);
	}
	if (!pgm_check(zone)) {
		MALLOC_REPORT_FATAL_ERROR(addr, "ProbGuard: zone integrity check failed");
	}
}


#pragma mark -
#pragma mark Fault Diagnosis

static void
fill_in_trace(const alloc_info_t *info, const uint8_t *buffer, stack_trace_t *trace)
{
	trace->thread_id = info->thread_id;
	trace->time = info->time;
	uint32_t max_frames = sizeof(trace->frames) / sizeof(trace->frames[0]);
	trace->num_frames = trace_decode(buffer, info->trace_size, trace->frames, max_frames);
}

static void
fill_in_report(const pgm_zone_t *zone, uint32_t slot, pgm_report_t *report)
{
	slot_t *s = &zone->slots[slot];
	metadata_t *m = &zone->metadata[s->metadata];

	report->nearest_allocation = block_addr(zone, slot);
	report->allocation_size = s->size;
	report->allocation_state = slot_state_labels[s->state];
	report->num_traces = 0;

	if (m->slot == slot) {
		report->num_traces++;
		fill_in_trace(&m->alloc, m->trace_buffer, &report->alloc_trace);
		if (s->state == ss_freed) {
			report->num_traces++;
			uint8_t *buffer = &m->trace_buffer[m->alloc.trace_size];
			fill_in_trace(&m->dealloc, buffer, &report->dealloc_trace);
		}
	}
}

#define KR_NO_PGM (KERN_RETURN_MAX + 1)
static kern_return_t
diagnose_page_fault(const pgm_zone_t *zone, vm_address_t fault_address, pgm_report_t *report)
{
	if (!is_guarded(zone, fault_address)) {
		return KR_NO_PGM;
	}

	slot_lookup_t res = lookup_slot(zone, fault_address);
	// Guaranteed by lookup_slot()
	MALLOC_ASSERT(res.slot < zone->num_slots);
	// Checked by read_zone()
	MALLOC_ASSERT(zone->slots[res.slot].metadata < zone->max_metadata);
	slot_state_t ss = zone->slots[res.slot].state;

	// We should have gotten here because of a page fault.
	if (ss == ss_allocated && res.bounds != b_oob_guard_page) {
		malloc_report(ASL_LEVEL_ERR | MALLOC_REPORT_LOG_ONLY,
				"Failed to generate PGM report for fault address %p: "
				"slot is unexpectedly allocated with bounds %d\n",
				(void *)fault_address, (int)res.bounds);
		return KERN_FAILURE;
	}

	// Note that all of the following error conditions may also be caused by:
	//  *) Randomly corrupted pointer
	//  *) Long-range OOB (access stride > (page size / 2))
	// We will always misdiagnose some of these errors no matter how we slice it.

	// TODO(yln): extract "nearest allocation helper"
	switch (ss) {
		case ss_unused:
			// Nearest slot was never used.
			// TODO(yln): if bounds == oob_guard_page; we could try to look at the slot on the other side of the guard page.
			report->error_type = "long-range OOB";
			report->confidence = "low";
			break;
		case ss_allocated:
			// Most likely an OOB access from an active allocation onto a guard page.
			MALLOC_ASSERT(res.bounds == b_oob_guard_page);
			report->error_type = "out-of-bounds";
			report->confidence = "high";
			break;
		case ss_freed:
			if (res.bounds == b_block_addr || res.bounds == b_valid) {
				report->error_type = "use-after-free";
				report->confidence = "high";
			} else {
				MALLOC_ASSERT(res.bounds == b_oob_slot || res.bounds == b_oob_guard_page);
				// This could be a combination of OOB and UAF, or one of the generic errors
				// outlined above.
				// TODO(yln): still try to diagnose something here
				report->error_type = "OOB + UAF";
				report->confidence = "low";
			}
			break;
		default:
			// Checked by read_zone()
			__builtin_unreachable();
	}

	report->fault_address = fault_address;
	fill_in_report(zone, res.slot, report);
	return KERN_SUCCESS;
}


#pragma mark -
#pragma mark Crash Reporter SPI

// KERN_FAILURE - memory read error
// KERN_SUCCESS - memory read ok, PGM report filled
// KR_NO_PGM    - memory read ok, but no PGM result, try next zone
MALLOC_STATIC_ASSERT(KR_NO_PGM > KERN_RETURN_MAX, "KR_NO_PGM");

static crash_reporter_memory_reader_t g_crm_reader;
static const uint32_t k_max_read_memory = 3;  // See read_zone() and read_type_t
static void *read_memory[k_max_read_memory];
static uint32_t num_read_memory;
static kern_return_t
memory_reader_adapter(task_t task, vm_address_t address, vm_size_t size, void **local_memory)
{
	MALLOC_ASSERT(num_read_memory < k_max_read_memory);
	void *ptr = g_crm_reader(task, address, size);
	*local_memory = ptr;
	if (!ptr) return KERN_FAILURE;
	read_memory[num_read_memory++] = ptr;
	return KERN_SUCCESS;
}

static memory_reader_t *
setup_memory_reader(crash_reporter_memory_reader_t crm_reader)
{
	MALLOC_ASSERT(crm_reader);
	g_crm_reader = crm_reader;
	num_read_memory = 0;
	return memory_reader_adapter;
}

static void
free_read_memory()
{
	for (uint32_t i = 0; i < num_read_memory; i++) {
		_free(read_memory[i]);
	}
	num_read_memory = 0;
}

static kern_return_t
is_pgm_zone(vm_address_t zone_address, task_t task, memory_reader_t reader)
{
	unsigned zone_type;
	kern_return_t kr = get_zone_type(task, reader, zone_address, &zone_type);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	return (zone_type == MALLOC_ZONE_TYPE_PGM) ? KERN_SUCCESS : KR_NO_PGM;
}

static kern_return_t
diagnose_fault_from_external_process(vm_address_t fault_address, pgm_report_t *report,
		task_t task, vm_address_t zone_address, memory_reader_t reader)
{
	READ_ZONE(zone, rt_slots | rt_metadata);
	return diagnose_page_fault(zone, fault_address, report);
}

static kern_return_t
extract_report_select_zone(vm_address_t fault_address, pgm_report_t *report,
		task_t task, vm_address_t *zone_addresses, uint32_t zone_count, memory_reader_t reader)
{
	for (uint32_t i = 0; i < zone_count; i++) {
		kern_return_t kr = is_pgm_zone(zone_addresses[i], task, reader);
		free_read_memory();
		if (kr == KR_NO_PGM) continue;
		if (kr != KERN_SUCCESS) return kr;

		kr = diagnose_fault_from_external_process(fault_address, report, task, zone_addresses[i], reader);
		free_read_memory();
		if (kr == KR_NO_PGM) continue;
		return kr;
	}
	return KERN_FAILURE;
}

static _malloc_lock_s crash_reporter_lock = _MALLOC_LOCK_INIT;
kern_return_t
pgm_extract_report_from_corpse(vm_address_t fault_address, pgm_report_t *report, task_t task,
		vm_address_t *zone_addresses, uint32_t zone_count, crash_reporter_memory_reader_t crm_reader)
{
	_malloc_lock_lock(&crash_reporter_lock);

	memory_reader_t *reader = setup_memory_reader(crm_reader);
	kern_return_t kr = extract_report_select_zone(fault_address, report, task, zone_addresses, zone_count, reader);

	_malloc_lock_unlock(&crash_reporter_lock);
	return kr;
}


#pragma mark -
#pragma mark Mockable Helpers

#ifndef PGM_MOCK_RANDOM
static uint32_t
rand_uniform(uint32_t upper_bound)
{
	MALLOC_ASSERT(upper_bound > 0);
	return arc4random_uniform(upper_bound);
}
#endif

#ifndef PGM_MOCK_TRACE_COLLECT
MALLOC_ALWAYS_INLINE
static inline size_t
my_trace_collect(uint8_t *buffer, size_t size)
{
	return trace_collect(buffer, size);
}
#endif

#ifndef PGM_MOCK_PAGE_ACCESS
static void
mark_inaccessible(vm_address_t page)
{
	int res = madvise((void *)page, PAGE_SIZE, CONFIG_MADVISE_STYLE);
	MALLOC_ASSERT(res == 0);
	my_vm_protect(page, PAGE_SIZE, VM_PROT_NONE);
}

static void
mark_read_write(vm_address_t page)
{
	// It is faster to just unprotect the page without calling madvise() first.
	my_vm_protect(page, PAGE_SIZE, VM_PROT_READ_WRITE);
}
#endif


#pragma mark -
#pragma mark Mach VM Helpers

static vm_address_t
my_vm_map_common(vm_address_t addr, size_t size, vm_prot_t protection, int vm_flags, int tag)
{
	vm_map_t target = mach_task_self();
	mach_vm_address_t address = addr;
	mach_vm_size_t size_rounded = round_page(size);
	mach_vm_offset_t mask = 0x0;
	int flags = vm_flags | VM_MAKE_TAG(tag);
	mem_entry_name_port_t object = MEMORY_OBJECT_NULL;
	memory_object_offset_t offset = 0;
	boolean_t copy = FALSE;
	vm_prot_t cur_protection = protection;
	vm_prot_t max_protection = VM_PROT_READ | VM_PROT_WRITE;
	vm_inherit_t inheritance = VM_INHERIT_DEFAULT;

	kern_return_t kr = mach_vm_map(target, &address, size_rounded, mask, flags,
		object, offset, copy, cur_protection, max_protection, inheritance);
	MALLOC_ASSERT(kr == KERN_SUCCESS);
	return (vm_address_t)address;
}

static vm_address_t
my_vm_map(size_t size, vm_prot_t protection, int tag)
{
	return my_vm_map_common(0, size, protection, VM_FLAGS_ANYWHERE, tag);
}

static void
my_vm_map_fixed(vm_address_t addr, size_t size, vm_prot_t protection, int tag)
{
	int flags = VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE;
	vm_address_t addr2 = my_vm_map_common(addr, size, protection, flags, tag);
	MALLOC_ASSERT(addr2 == addr);
}

static void
my_vm_deallocate(vm_address_t addr, size_t size)
{
	vm_map_t target = mach_task_self();
	mach_vm_address_t address = (mach_vm_address_t)addr;
	mach_vm_size_t size_rounded = round_page(size);
	kern_return_t kr = mach_vm_deallocate(target, address, size_rounded);
	MALLOC_ASSERT(kr == KERN_SUCCESS);
}

static void
my_vm_protect(vm_address_t addr, size_t size, vm_prot_t protection) {
	vm_map_t target = mach_task_self();
	mach_vm_address_t address = (mach_vm_address_t)addr;
	mach_vm_size_t size_rounded = round_page(size);
	boolean_t set_maximum = FALSE;
	kern_return_t kr = mach_vm_protect(target, address, size_rounded, set_maximum, protection);
	MALLOC_ASSERT(kr == KERN_SUCCESS);
}
