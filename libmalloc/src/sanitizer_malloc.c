/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#if CONFIG_SANITIZER

#pragma mark -
#pragma mark Types and Structures

typedef struct {
	// Malloc zone
	malloc_zone_t malloc_zone;
	malloc_zone_t *wrapped_zone;

	// Configuration
	bool debug;
	bool do_poisoning;
	size_t redzone_size; // minimum amount of right padding
	size_t max_items_in_quarantine; // 0 means unlimited
	size_t max_bytes_in_quarantine; // 0 means unlimited

#if !MALLOC_TARGET_EXCLAVES
	// Stacktrace tracking data structures
	struct stacktrace_depo_t *depo;
	struct pointer_map_t *map;

	uint8_t padding[PAGE_MAX_SIZE];
#endif /* !MALLOC_TARGET_EXCLAVES */

	// Mutable state
	_malloc_lock_s lock;
	struct quarantined_chunk *quarantine_head;
	struct quarantined_chunk *quarantine_tail;
	size_t items_in_quarantine;
	size_t bytes_in_quarantine;
} sanitizer_zone_t;

ASSERT_WRAPPER_ZONE(sanitizer_zone_t);

#if !MALLOC_TARGET_EXCLAVES
MALLOC_STATIC_ASSERT(offsetof(sanitizer_zone_t, padding) < PAGE_MAX_SIZE,
		"First page is mapped read-only");
MALLOC_STATIC_ASSERT(offsetof(sanitizer_zone_t, lock) >= PAGE_MAX_SIZE,
		"Mutable state is on separate page");
MALLOC_STATIC_ASSERT(sizeof(sanitizer_zone_t) < (2 * PAGE_MAX_SIZE),
		"Zone fits on 2 pages");
#endif /* !MALLOC_TARGET_EXCLAVES */

#ifndef ASAN_SHADOW_ALIGNMENT
#define ASAN_SHADOW_ALIGNMENT 8
#endif /* ASAN_SHADOW_ALIGNMENT */

#define DELEGATE(function, args...) \
	zone->wrapped_zone->function(zone->wrapped_zone, args)

#if MALLOC_TARGET_EXCLAVES
static sanitizer_zone_t sanitizer_zone;
#endif /* MALLOC_TARGET_EXCLAVES */

// Lock helpers
static void
init_lock(sanitizer_zone_t *zone)
{
	_malloc_lock_init(&zone->lock);
}

static void
lock(sanitizer_zone_t *zone)
{
	_malloc_lock_lock(&zone->lock);
}

static void
unlock(sanitizer_zone_t *zone)
{
	_malloc_lock_unlock(&zone->lock);
}

static bool
trylock(sanitizer_zone_t *zone)
{
	return _malloc_lock_trylock(&zone->lock);
}

// VM allocation/deallocate helpers
#if !MALLOC_TARGET_EXCLAVES
static vm_address_t
sanitizer_vm_map(size_t size, vm_prot_t protection, int tag)
{
	vm_map_t target = mach_task_self();
	mach_vm_address_t address = 0;
	mach_vm_size_t size_rounded = round_page(size);
	mach_vm_offset_t mask = 0x0;
	int flags = VM_FLAGS_ANYWHERE | VM_MAKE_TAG(tag);
	mem_entry_name_port_t object = MEMORY_OBJECT_NULL;
	memory_object_offset_t offset = 0;
	bool copy = false;
	vm_prot_t cur_protection = protection;
	vm_prot_t max_protection = VM_PROT_READ | VM_PROT_WRITE;
	vm_inherit_t inheritance = VM_INHERIT_DEFAULT;

	kern_return_t kr = mach_vm_map(target, &address, size_rounded, mask, flags,
		object, offset, copy, cur_protection, max_protection, inheritance);
	MALLOC_ASSERT(kr == KERN_SUCCESS);
	return (vm_address_t)address;
}

static void
sanitizer_vm_deallocate(vm_address_t addr, size_t size)
{
	vm_map_t target = mach_task_self();
	mach_vm_address_t address = (mach_vm_address_t)addr;
	mach_vm_size_t size_rounded = round_page(size);
	kern_return_t kr = mach_vm_deallocate(target, address, size_rounded);
	MALLOC_ASSERT(kr == KERN_SUCCESS);
}

static void
sanitizer_vm_protect(vm_address_t addr, size_t size, vm_prot_t protection)
{
	vm_map_t target = mach_task_self();
	mach_vm_address_t address = (mach_vm_address_t)addr;
	mach_vm_size_t size_rounded = round_page(size);
	bool set_maximum = false;
	kern_return_t kr = mach_vm_protect(target, address, size_rounded, set_maximum, protection);
	MALLOC_ASSERT(kr == KERN_SUCCESS);
}

// Env helpers
static const char *
env_var(const char *name)
{
	const char **env = (const char **)*_NSGetEnviron();
	return _simple_getenv(env, name);
}

static bool
env_bool(const char *name)
{
	const char *value = env_var(name);
	if (!value) return false;
	return value[0] == '1';
}

static uint32_t
env_uint(const char *name, uint32_t default_value)
{
	const char *value = env_var(name);
	if (!value) return default_value;
	return (uint32_t)strtoul(value, NULL, 0);
}

static uint32_t
stacktrace_depo_insert(struct stacktrace_depo_t *depo, uintptr_t * __counted_by(count) pcs, size_t count);

static bool
pointer_map_find(struct pointer_map_t *map, uintptr_t ptr, uint64_t *word_out);

static void
pointer_map_insert(struct pointer_map_t *map, uintptr_t ptr, uint64_t word);

#define wrap(index, container) ((index) & (countof(container) - 1))

static uint32_t OS_ALWAYS_INLINE
insert_current_stacktrace_into_depo(struct stacktrace_depo_t *depo, uint32_t top_frames_to_ignore)
{
	void * __unsafe_indexable pcs[16 + top_frames_to_ignore];
#if MALLOC_TARGET_EXCLAVES
	ssize_t num_pcs = backtrace(pcs, countof(pcs));
#else
	uint32_t num_pcs;
	thread_stack_pcs((vm_address_t *)pcs, (unsigned)countof(pcs), &num_pcs);
#endif // MALLOC_TARGET_EXCLAVES
	if (num_pcs <= top_frames_to_ignore) {
		return 0;
	}
	const size_t num_frames = (size_t)num_pcs - top_frames_to_ignore;
	return stacktrace_depo_insert(depo, (uintptr_t * __counted_by(num_frames))&pcs[top_frames_to_ignore], num_frames);
}

static void OS_ALWAYS_INLINE
record_alloc_stacktrace(struct stacktrace_depo_t *depo, struct pointer_map_t *map, void *ptr, size_t size)
{
	if (ptr == NULL || size >= PAGE_SIZE) {
		return;
	}
	uint32_t alloc_hash = insert_current_stacktrace_into_depo(depo, 1);
	pointer_map_insert(map, (uintptr_t)ptr, alloc_hash);
}
#endif /* !MALLOC_TARGET_EXCLAVES */

static void
unpoison(sanitizer_zone_t *zone, void * __sized_by(size) ptr, size_t size);

#pragma mark -
#pragma mark Quarantine Logic

typedef struct quarantined_chunk {
	uint64_t next_and_size;
#if !MALLOC_TARGET_EXCLAVES
	uint64_t stacktrace_hashes;
#endif /* !MALLOC_TARGET_EXCLAVES */
} quarantined_chunk_t;

MALLOC_STATIC_ASSERT(sizeof(quarantined_chunk_t) <= 16,
		"quarantined_chunk_t must be 16 bytes to fit in all allocations");

typedef union {
	uint64_t i;
	struct {
		uint64_t next_ptr : 48;
		uint64_t size : 16;
	} parts;
} next_and_size;

MALLOC_STATIC_ASSERT(sizeof(next_and_size) == 8,
		"next_and_size must be 8 bytes");

static void OS_NOINLINE
place_into_quarantine(sanitizer_zone_t *zone, void * __unsafe_indexable _ptr, size_t size)
{
	if (_ptr == NULL) {
		return;
	}

	// We need to know the size of the chunk, for quarantine bookkeeping
	if (size == 0) {
		size = DELEGATE(size, _ptr);
	}

	void *ptr = __unsafe_forge_bidi_indexable(void *, _ptr, size);
	// Don't quarantine large allocations to avoid one single huge allocation
	// evicting the whole quarantine.
	if (size > PAGE_SIZE) {
		// Actually unpoison before handing back to the allocator. This is not
		// always strictly necessary, but only when executing under memory
		// instrumentation that may check the shadow map for us
		if (zone->do_poisoning) {
			unpoison(zone, ptr, size);
		}

		return DELEGATE(free, ptr);
	}

#if !MALLOC_TARGET_EXCLAVES
	uint32_t dealloc_stack_hash = insert_current_stacktrace_into_depo(zone->depo, 2);
	uint64_t stored_word = 0;
	pointer_map_find(zone->map, (uintptr_t)ptr, &stored_word);
	uint32_t alloc_stack_hash = (uint32_t)stored_word;
	uint64_t hashes = alloc_stack_hash | (((uint64_t)dealloc_stack_hash) << 32);
#endif

	lock(zone);

	// Append ptr to the tail of the quarantine list
	if (zone->items_in_quarantine == 0) {
		zone->quarantine_tail = zone->quarantine_head = ptr;
	} else {
		next_and_size n;
		n.i = _malloc_read_uint64_via_rsp(&zone->quarantine_tail->next_and_size);
		n.parts.next_ptr = (uintptr_t)ptr;
		_malloc_write_uint64_via_rsp(&zone->quarantine_tail->next_and_size, n.i);
		zone->quarantine_tail = ptr;
	}
	next_and_size n = { .parts = { .next_ptr = 0, .size = size } };
	_malloc_write_uint64_via_rsp(&zone->quarantine_tail->next_and_size, n.i);
#if !MALLOC_TARGET_EXCLAVES
	_malloc_write_uint64_via_rsp(&zone->quarantine_tail->stacktrace_hashes, hashes);
#endif

	zone->items_in_quarantine += 1;
	zone->bytes_in_quarantine += size;

	// Now let's remove and free chunks from the quarantine list that are over
	// limits. To minimize the work that we do under the zone lock, we only
	// remove chunks from the quarantine list (i.e. we adjust quarantine_head
	// and statistics), and then only actually free the chunks outside of the
	// lock.
	long items_over_limit = (zone->max_items_in_quarantine > 0 &&
		zone->items_in_quarantine > zone->max_items_in_quarantine) ?
		zone->items_in_quarantine - zone->max_items_in_quarantine : 0;
	long bytes_over_limit = (zone->max_bytes_in_quarantine > 0 &&
		zone->bytes_in_quarantine > zone->max_bytes_in_quarantine) ?
		zone->bytes_in_quarantine - zone->max_bytes_in_quarantine : 0;

	quarantined_chunk_t *items_to_free_head = zone->quarantine_head;
	size_t items_to_free_count = 0;
	size_t items_to_free_size = 0;

	quarantined_chunk_t *iterator = zone->quarantine_head;
	while (items_over_limit > 0 || bytes_over_limit > 0) {
		next_and_size n;
		n.i = _malloc_read_uint64_via_rsp(&iterator->next_and_size);
		quarantined_chunk_t *next = __unsafe_forge_single(quarantined_chunk_t *, n.parts.next_ptr);
		size_t iterator_size = n.parts.size;

		items_to_free_count += 1;
		items_to_free_size += iterator_size;
		items_over_limit -= 1;
		bytes_over_limit -= iterator_size;

		iterator = next;
	}

	zone->quarantine_head = iterator;
	zone->items_in_quarantine -= items_to_free_count;
	zone->bytes_in_quarantine -= items_to_free_size;

	unlock(zone);

	// Actually free chunks. At this point, they are already removed from the
	// quarantine list so we are the exclusive owner of them.
	iterator = items_to_free_head;
	for (size_t i = 0; i < items_to_free_count; i++) {
		next_and_size n;
		n.i = _malloc_read_uint64_via_rsp(&iterator->next_and_size);
		quarantined_chunk_t *next = __unsafe_forge_single(quarantined_chunk_t *, n.parts.next_ptr);
		size_t iterator_size = n.parts.size;

		if (zone->debug) malloc_report(ASL_LEVEL_INFO, "evicting %p from quarantine, size = 0x%lx\n", iterator, iterator_size);

		// Forge the pointer because it is only sized for quarantined_chunk_t
		void *iterator_ptr = __unsafe_forge_bidi_indexable(void *, iterator,
				iterator_size);

		// Same as above, perform actual unpoisoning
		if (zone->do_poisoning) {
			unpoison(zone, iterator_ptr, iterator_size);
		}

		DELEGATE(free_definite_size, iterator_ptr, iterator_size);

		iterator = next;
	}
}

#if !MALLOC_TARGET_EXCLAVES
#pragma mark -
#pragma mark MurmurHash2

// 32-bit MurmurHash2, public domain by Austin Appleby,
// <https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp>.

#define MURMUR2_SEED 0xe3be96d1  // fair dice roll
#define MURMUR2_MULTIPLIER 0x5bd1e995

static uint32_t
murmur2_init()
{
	return MURMUR2_SEED;
}

static void
murmur2_add_uint32(uint32_t *hstate, uint32_t val)
{
	val *= MURMUR2_MULTIPLIER;
	val ^= val >> 24;
	val *= MURMUR2_MULTIPLIER;
	*hstate *= MURMUR2_MULTIPLIER;
	*hstate ^= val;
}

static void
murmur2_add_uintptr(uint32_t *hstate, uintptr_t ptr)
{
	murmur2_add_uint32(hstate, (uint32_t)ptr);
#if MALLOC_TARGET_64BIT
	murmur2_add_uint32(hstate, (uint32_t)(ptr >> 32));
#endif
}

static uint32_t
murmur2_finalize(uint32_t *hstate)
{
	uint32_t X = *hstate;
	X ^= X >> 13;
	X *= MURMUR2_MULTIPLIER;
	X ^= X >> 15;
	return X;
}

static uint32_t
murmur2_hash_pointer(uintptr_t ptr)
{
	uint32_t hstate = murmur2_init();
	murmur2_add_uintptr(&hstate, ptr);
	return murmur2_finalize(&hstate);
}

static uint32_t
murmur2_hash_backtrace(uintptr_t * __counted_by(count) pcs, size_t count)
{
	uint32_t hstate = murmur2_init();
	for (int i = 0; i < count; i++) {
		murmur2_add_uintptr(&hstate, pcs[i]);
	}
	return murmur2_finalize(&hstate);
}


#pragma mark -
#pragma mark Stack Trace Depo

// Data structure to store up to 512k unique stacktraces, if they're on average
// 8 frames large, barring hash collisions, loosely modelled after Scudo:
// <https://github.com/llvm/llvm-project/blob/main/compiler-rt/lib/scudo/>.
//
// - The "handle" to a stored stacktrace is its own hash (Murmur2).
// - Frames are stored in a ring buffer, oldest get replaced on wrap.
// - Look-up will not return evicted data because we check the hash.
// - Friendly for remote inspection by ReportCrash (no pointers).
// - Lock-free, non-synchronizing insertion (no in-process look-ups).
// - Racy same-hash insertion might store the frames twice, but that's fine.
// - Hash collisions (murmur2 produces uint32_t hashes) will cause a stacktrace
//   to be unique'd against a different one, and look-up can retrieve a wrong
//   stacktrace. Should be rare enough with a good hashing algorithm, and it's
//   fine given we store stacktraces only for diagnostic purposes.
//
// The sanitizer zone captures alloc and dealloc stack traces and saves them
// into the depo. The handles/hashes are then stored elsewhere (in pointer_map
// for live allocations, and in quarantine_chunk_t for quarantined ones).

typedef struct stacktrace_depo_t {
	uint64_t index[1 << 19];  // 512k entries, 4 MiB in size
	uint64_t storage[1 << 22];  // 4m entries, 32 MiB in size
	uint64_t storage_pos; // can be over countof(storage), always use wrap()
} stacktrace_depo_t;

typedef union {
	uint64_t i;
	struct {
		uint32_t hash;
		uint32_t pos : 24;
		uint32_t count : 8;
	} parts;
} index_entry;
MALLOC_STATIC_ASSERT(sizeof(index_entry) == 8, "index_entry should be 64 bits");

static stacktrace_depo_t *
stacktrace_depo_create()
{
	return mvm_allocate_pages(sizeof(stacktrace_depo_t), PAGE_SHIFT, 0, VM_MEMORY_ANALYSIS_TOOL);
}

static void
stacktrace_depo_destroy(stacktrace_depo_t *depo)
{
	mvm_deallocate_pages(depo, sizeof(stacktrace_depo_t), 0);
}

static uint32_t
stacktrace_depo_insert(stacktrace_depo_t *depo, uintptr_t * __counted_by(count) pcs, size_t count)
{
	MALLOC_ASSERT(count < 256);
	uint32_t hash = murmur2_hash_backtrace(pcs, count);
	uint32_t index_pos = wrap(hash, depo->index);

	index_entry entry;
	entry.i = os_atomic_load_wide(&depo->index[index_pos], relaxed);
	if (entry.parts.count == count && entry.parts.hash == hash) {
		return hash;
	}

	uint64_t old_storage_pos = wrap(os_atomic_add_orig(&depo->storage_pos,
			count, relaxed), depo->storage);
	entry.parts.hash = hash;
	entry.parts.pos = (uint32_t)old_storage_pos;
	entry.parts.count = (uint32_t)count;
	os_atomic_store_wide(&depo->index[index_pos], entry.i, relaxed);
	for (int i = 0; i < count; i++) {
		uint32_t pos = wrap(old_storage_pos + i, depo->storage);
		os_atomic_store_wide(&depo->storage[pos], pcs[i], relaxed);
	}
	return hash;
}

// Doesn't need to use atomics or be thread-safe against insertion because
// look-up is only used from ReportCrash against a corpse.
static size_t
stacktrace_depo_find(stacktrace_depo_t *depo, uint32_t hash, uintptr_t * __counted_by(max_size) pcs, size_t max_size)
{
	uint32_t index_pos = wrap(hash, depo->index);

	index_entry entry;
	entry.i = depo->index[index_pos];
	if (entry.parts.hash != hash || entry.parts.pos > countof(depo->storage)) {
		return 0;
	}

	uint32_t hstate = murmur2_init();
	for (int i = 0; i < entry.parts.count; i++) {
		uint32_t pos = wrap(entry.parts.pos + i, depo->storage);
		if (i < max_size) {
		    // Explicit cast as it doesn't otherwise compile on watchOS (error: implicit conversion loses integer precision)
			pcs[i] = (uintptr_t)depo->storage[pos];
		}
		murmur2_add_uintptr(&hstate, pcs[i]);
	}

	if (hash != murmur2_finalize(&hstate)) {
		return 0;
	}

	return MIN(max_size, entry.parts.count);
}


#pragma mark -
#pragma mark Pointer Map

// Data structure to associate and store a 64-bit value for arbitrary pointers.
//
// We use the pointer map to store handles/hashes of stacktraces for live heap
// allocations. When the same pointer is inserted again, it must have already
// been quarantined and free'd and recycled, so it's okay to drop the previous
// data associated with it. On slot collision (20 bits), we evict the older
// entry, in which case we just lose track of the associated allocation
// stacktrace for the older allocation. When a chunk is quarantined, we transfer
// the stacktrace handle into quarantine_chunk_t, so we no longer care about the
// pointer map holding the right value for it. Look-up will never return a wrong
// value, because it checks the pointer address in the storage.

typedef struct pointer_map_t {
	__uint128_t storage[1 << 20];  // 1m entries, 16 MiB in size
} pointer_map_t;

typedef union {
	__uint128_t i;
	struct {
		uint64_t ptr;
		uint64_t word;
	} parts;
} pointer_map_entry;

MALLOC_STATIC_ASSERT(sizeof(pointer_map_entry) == 16, "pointer_map_entry should be 16 bytes");

static pointer_map_t *
pointer_map_create()
{
	return mvm_allocate_pages(sizeof(pointer_map_t), PAGE_SHIFT, 0, VM_MEMORY_ANALYSIS_TOOL);
}

static void
pointer_map_destroy(pointer_map_t *map)
{
	mvm_deallocate_pages(map, sizeof(pointer_map_t), 0);
}

static void
pointer_map_insert(pointer_map_t *map, uintptr_t ptr, uint64_t word)
{
	uint32_t hash = murmur2_hash_pointer(ptr);
	uint32_t pos = wrap(hash, map->storage);
	pointer_map_entry entry;
	entry.parts.ptr = ptr;
	entry.parts.word = word;
	os_atomic_store_wide(&map->storage[pos], entry.i, relaxed);
}

static bool
pointer_map_find(pointer_map_t *map, uintptr_t ptr, uint64_t *word_out)
{
	uint32_t hash = murmur2_hash_pointer(ptr);
	uint32_t pos = wrap(hash, map->storage);
	pointer_map_entry entry;
	entry.i = os_atomic_load_wide(&map->storage[pos], relaxed);
	if (entry.parts.ptr != ptr) {
		return false;
	}
	*word_out = entry.parts.word;
	return true;
}
#endif /* !MALLOC_TARGET_EXCLAVES */

#pragma mark -
#pragma mark Poisoning Functions

// Size of redzone is stored in the last aligned size_t at the end of the allocation
static size_t get_redzone_size(sanitizer_zone_t *zone, const void * __sized_by(size) ptr, size_t size)
{
	MALLOC_ASSERT(zone->do_poisoning);
	// Size of redzone is stored in the last aligned size_t at the end of the allocation
	const size_t offset = sizeof(size_t) + size % sizeof(size_t);
	const size_t *redzone_size_ptr =
			__unsafe_forge_single(size_t *, ((uintptr_t)ptr + size - offset));
	// When executing under instrumentation, the translation layer automatically
	// checks against the shadow map, instead of letting the compiled program's
	// instrumentation handle it. This means we need to bypass it since the size
	// is stored within the allocation redzone
    // Explicit cast as it doesn't otherwise compile on watchOS (error: implicit conversion loses integer precision)
#if MALLOC_TARGET_64BIT
	const size_t redzone_size = (size_t)_malloc_read_uint64_via_rsp(redzone_size_ptr);
#else
	const size_t redzone_size = (size_t)*(uint32_t *)redzone_size_ptr;
#endif
	MALLOC_ASSERT(redzone_size >= zone->redzone_size && redzone_size < size);
	return redzone_size;
}

static void set_redzone_size(sanitizer_zone_t *zone, void * __sized_by(usr_size + redzone_size) ptr, size_t usr_size, size_t redzone_size)
{
	MALLOC_ASSERT(zone->do_poisoning);
	const size_t offset =
			sizeof(size_t) + (usr_size + redzone_size) % sizeof(size_t);
	size_t *redzone_size_ptr = ptr + (usr_size + redzone_size - offset);
	// Same as above, this may be a reallocation that has not yet been unpoisoned
#if MALLOC_TARGET_64BIT
	_malloc_write_uint64_via_rsp(redzone_size_ptr, redzone_size);
#else
	*(uint32_t *)redzone_size_ptr = redzone_size;
#endif
}

static void poison_alloc(sanitizer_zone_t *zone, void * __sized_by(usr_size + redzone_size) ptr, size_t usr_size, size_t redzone_size)
{
	if (zone->debug) malloc_report(ASL_LEVEL_INFO, "poison_alloc(%p, 0x%lx, 0x%lx)\n", ptr, usr_size, redzone_size);

	MALLOC_ASSERT(ptr);

	// Always set the redzone size, even if we can't actually poison allocations
	set_redzone_size(zone, ptr, usr_size, redzone_size);

	const struct malloc_sanitizer_poison *sanitizer = malloc_sanitizer_get_functions();
	if (sanitizer && sanitizer->heap_allocate_poison) {
		(*sanitizer->heap_allocate_poison)((uintptr_t)ptr, 0, usr_size, redzone_size);
	} else if (zone->debug) {
		malloc_report(ASL_LEVEL_WARNING, "MallocSanitizerZone: Not poisoning allocation %p of size %lu with redzone size %lu due to missing pointers!\n", ptr, usr_size, redzone_size);
	}
}

static void poison_free(sanitizer_zone_t *zone, void * __sized_by(size) ptr, size_t size)
{
	if (zone->debug) malloc_report(ASL_LEVEL_INFO, "poison_free(%p, 0x%lx)\n", ptr, size);

	MALLOC_ASSERT(ptr);

	const struct malloc_sanitizer_poison *sanitizer = malloc_sanitizer_get_functions();
	if (sanitizer && sanitizer->heap_deallocate_poison) {
		(*sanitizer->heap_deallocate_poison)((uintptr_t)ptr, size);
	} else if (zone->debug) {
		malloc_report(ASL_LEVEL_WARNING, "MallocSanitizerZone: Not poisoning deallocation %p of size %lu due to missing pointers!\n", ptr, size);
	}
}

static void unpoison(sanitizer_zone_t *zone, void * __sized_by(size) ptr, size_t size)
{
	if (zone->debug) malloc_report(ASL_LEVEL_INFO, "unpoison(%p, 0x%lx)\n", ptr, size);

	MALLOC_ASSERT(ptr);

	const struct malloc_sanitizer_poison *sanitizer = malloc_sanitizer_get_functions();
	if (sanitizer && sanitizer->heap_allocate_poison) {
		(*sanitizer->heap_allocate_poison)((uintptr_t)ptr, 0, size, 0);
	} else if (zone->debug) {
		malloc_report(ASL_LEVEL_WARNING, "MallocSanitizerZone: Not unpoisoning %p of size %lu due to missing pointers!\n", ptr, size);
	}
}

#pragma mark -
#pragma mark Zone Functions

static size_t
sanitizer_size(sanitizer_zone_t *zone, const void * __unsafe_indexable ptr)
{
	size_t size = DELEGATE(size, ptr);
	if (!size) {
		return 0;
	}

	if (zone->do_poisoning) {
		const size_t redzone_size = get_redzone_size(zone, __unsafe_forge_bidi_indexable(void *, ptr, size), size);
		if (zone->debug) malloc_report(ASL_LEVEL_INFO, "size(%p) = 0x%lx - redzone 0x%lx\n", ptr, size, redzone_size);

		MALLOC_ASSERT(size > redzone_size);
		size -= redzone_size;
	} else {
		if (zone->debug) malloc_report(ASL_LEVEL_INFO, "size(%p) = 0x%lx\n", ptr, size);
	}
	return size;
}

static void * __alloc_size(2) __sized_by_or_null(size)
sanitizer_malloc_type_malloc_noalign_with_options(sanitizer_zone_t *zone,
		size_t size, malloc_zone_malloc_options_t options,
		malloc_type_id_t type_id)
{
	if (!size) {
		size = 1;
	}
	size_t redzone_size = zone->redzone_size;
	const size_t usr_size = size;
	if (zone->do_poisoning) {
		const size_t mask = ASAN_SHADOW_ALIGNMENT - 1;
		// Round up redzone so that allocation is padded to shadow alignment
		redzone_size += (ASAN_SHADOW_ALIGNMENT - (usr_size & mask)) & mask;
		// Recalculate the total allocation size
		size = usr_size + redzone_size;
		// Check for overflow once at the end
		if (size < usr_size) {
			malloc_set_errno_fast(MZ_POSIX, ENOMEM);
			return NULL;
		}
	}

	void *ptr;
#if MALLOC_TARGET_64BIT
	malloc_type_descriptor_t type_desc = { .type_id = type_id };
#endif // MALLOC_TARGET_64BIT
	if (zone->wrapped_zone->version >= 16) {
		if (zone->wrapped_zone->malloc_type_malloc_with_options) {
			// Dispatch directly with pass-thru options
			ptr = DELEGATE(malloc_type_malloc_with_options, 0, size, options,
					type_id);
		} else if (options & MALLOC_ZONE_MALLOC_OPTION_CLEAR) {
			// Need fallback for this option
			ptr = DELEGATE(malloc_type_calloc, 1, size, type_id);
		} else {
			// Remaining options already handled in parent, ignore them
			ptr = DELEGATE(malloc_type_malloc, size, type_id);
		}
	} else if (zone->wrapped_zone->version >= 15 &&
			zone->wrapped_zone->malloc_with_options) {
		// Dispatch directly with type TSD and pass-thru options
#if MALLOC_TARGET_64BIT
		malloc_set_tsd_type_descriptor(type_desc);
#endif // MALLOC_TARGET_64BIT
		ptr = DELEGATE(malloc_with_options, 0, size, options);
#if MALLOC_TARGET_64BIT
		malloc_set_tsd_type_descriptor(MALLOC_TYPE_DESCRIPTOR_NONE);
#endif // MALLOC_TARGET_64BIT
	} else {
		const malloc_zone_malloc_options_t known_options =
				MALLOC_ZONE_MALLOC_OPTION_CLEAR
				;
		if (options & ~known_options) {
			malloc_zone_error(MALLOC_ABORT_ON_ERROR, true,
					"sanitizer_malloc_with_options: unsupported options 0x%llx\n",
					options);
			__builtin_trap();
		}

		// Set the type TSD and check the options
#if MALLOC_TARGET_64BIT
		malloc_set_tsd_type_descriptor(type_desc);
#endif // MALLOC_TARGET_64BIT
		if (options & MALLOC_ZONE_MALLOC_OPTION_CLEAR) {
			// Need fallback for this option
			ptr = DELEGATE(calloc, 1, size);
		} else {
			// Remaining options already handled in parent, ignore them
			ptr = DELEGATE(malloc, size);
		}
#if MALLOC_TARGET_64BIT
		malloc_set_tsd_type_descriptor(MALLOC_TYPE_DESCRIPTOR_NONE);
#endif // MALLOC_TARGET_64BIT
	}

#if !MALLOC_TARGET_EXCLAVES
	record_alloc_stacktrace(zone->depo, zone->map, ptr, usr_size);
#endif /* !MALLOC_TARGET_EXCLAVES */
	if (zone->debug) malloc_report(ASL_LEVEL_INFO, "malloc(0x%lx) = %p\n", size, ptr);
	if (ptr && zone->do_poisoning) {
		// Recalculate the redzone size to include allocator padding
		size_t actual_size = DELEGATE(size, ptr);
		MALLOC_ASSERT(actual_size >= size);
		redzone_size += actual_size - size;
		ptr = __unsafe_forge_bidi_indexable(void *, ptr,
				usr_size + redzone_size);
		poison_alloc(zone, ptr, usr_size, redzone_size);
	}
	return ptr;
}

static void * __alloc_size(2) __sized_by_or_null(size)
sanitizer_malloc(sanitizer_zone_t *zone, size_t size)
{
	return sanitizer_malloc_type_malloc_noalign_with_options(zone, size, 0,
			malloc_get_tsd_type_id());
}

static void * __alloc_size(2) __sized_by_or_null(size)
sanitizer_malloc_type_malloc(sanitizer_zone_t *zone, size_t size,
		malloc_type_id_t type_id)
{
	return sanitizer_malloc_type_malloc_noalign_with_options(zone, size, 0,
			type_id);
}

static void * __alloc_size(2,3) __sized_by_or_null(num_items * size)
sanitizer_malloc_type_calloc(sanitizer_zone_t *zone, size_t num_items,
		size_t size, malloc_type_id_t type_id)
{
	size_t usr_size;
	if (!size || !num_items) {
		usr_size = 1;
	} else if (calloc_get_size(num_items, size, 0, &usr_size)) {
		malloc_set_errno_fast(MZ_POSIX, ENOMEM);
		return NULL;
	}

	size_t redzone_size = zone->redzone_size;
	if (zone->do_poisoning) {
		// Round up redzone so that allocation is padded to shadow alignment
		redzone_size += ASAN_SHADOW_ALIGNMENT - (usr_size & (ASAN_SHADOW_ALIGNMENT - 1));
		// Recalculate the total allocation size
		num_items = 1;
		size = usr_size + redzone_size;
		// Check for overflow once at the end
		if (size < usr_size) {
			malloc_set_errno_fast(MZ_POSIX, ENOMEM);
			return NULL;
		}
	}

	void *ptr;
	if (zone->wrapped_zone->version >= 16) {
		ptr = __unsafe_forge_bidi_indexable(void *,
				DELEGATE(malloc_type_calloc, num_items, size, type_id), usr_size);
	} else {
#if MALLOC_TARGET_64BIT
		malloc_set_tsd_type_descriptor(
				(malloc_type_descriptor_t){ .type_id = type_id });
#endif // MALLOC_TARGET_64BIT
		ptr = __unsafe_forge_bidi_indexable(void *,
				DELEGATE(calloc, num_items, size), usr_size);
#if MALLOC_TARGET_64BIT
		malloc_set_tsd_type_descriptor(MALLOC_TYPE_DESCRIPTOR_NONE);
#endif // MALLOC_TARGET_64BIT
	}

	if (zone->debug) malloc_report(ASL_LEVEL_INFO, "calloc(0x%lx, 0x%lx) = %p\n", num_items, size, ptr);
#if !MALLOC_TARGET_EXCLAVES
	record_alloc_stacktrace(zone->depo, zone->map, ptr, usr_size);
#endif /* !MALLOC_TARGET_EXCLAVES */
	if (ptr && zone->do_poisoning) {
		// Recalculate the redzone size to include allocator padding
		size_t actual_size = DELEGATE(size, ptr);
		MALLOC_ASSERT(actual_size >= size);
		redzone_size += actual_size - size;
		ptr = __unsafe_forge_bidi_indexable(void *, ptr,
				usr_size + redzone_size);
		poison_alloc(zone, ptr, usr_size, redzone_size);
	}
	return ptr;
}


static void * __alloc_size(2,3) __sized_by_or_null(num_items * size)
sanitizer_calloc(sanitizer_zone_t *zone, size_t num_items, size_t size)
{
	return sanitizer_malloc_type_calloc(zone, num_items, size,
			malloc_get_tsd_type_id());
}

static void * __alloc_size(2) __sized_by_or_null(size)
sanitizer_valloc(sanitizer_zone_t *zone, size_t size)
{
	if (!size) {
		size = 1;
	}
	size_t redzone_size = zone->redzone_size;
	const size_t usr_size = size;
	if (zone->do_poisoning) {
		// Round up redzone so that allocation is padded to shadow alignment
		redzone_size += ASAN_SHADOW_ALIGNMENT - (usr_size & (ASAN_SHADOW_ALIGNMENT - 1));
		// Recalculate the total allocation size
		size = usr_size + redzone_size;
		// Check for overflow once at the end
		if (size < usr_size) {
			return NULL;
		}
	}
	void *ptr = DELEGATE(valloc, size);
#if !MALLOC_TARGET_EXCLAVES
	record_alloc_stacktrace(zone->depo, zone->map, ptr, usr_size);
#endif /* !MALLOC_TARGET_EXCLAVES */
	if (zone->debug) malloc_report(ASL_LEVEL_INFO, "valloc(0x%lx) = %p\n", size, ptr);
	if (ptr && zone->do_poisoning) {
		// Recalculate the redzone size to include allocator padding
		size_t actual_size = DELEGATE(size, ptr);
		MALLOC_ASSERT(actual_size >= size);
		redzone_size += actual_size - size;
		ptr = __unsafe_forge_bidi_indexable(void *, ptr,
				usr_size + redzone_size);
		poison_alloc(zone, ptr, usr_size, redzone_size);
	}
	return ptr;
}

static void
sanitizer_free(sanitizer_zone_t *zone, void * __unsafe_indexable ptr)
{
	if (os_unlikely(!ptr)) {
		return;
	}

	size_t size = 0;
	if (zone->do_poisoning) {
		size = DELEGATE(size, ptr);
		poison_free(zone, __unsafe_forge_bidi_indexable(void *, ptr, size), size);
	}
	if (zone->debug) malloc_report(ASL_LEVEL_INFO, "free(%p)\n", ptr);
	place_into_quarantine(zone, ptr, size);
}

static void * __alloc_size(3) __sized_by_or_null(new_size)
sanitizer_malloc_type_realloc(sanitizer_zone_t *zone,
		void * __unsafe_indexable ptr, size_t new_size,
		malloc_type_id_t type_id)
{
	if (new_size == 0) {
		new_size = 1;
	}

	size_t redzone_size = zone->redzone_size;
	const size_t usr_new_size = new_size;
	if (zone->do_poisoning) {
		// Round up redzone so that allocation is padded to shadow alignment
		redzone_size += ASAN_SHADOW_ALIGNMENT - (new_size & (ASAN_SHADOW_ALIGNMENT - 1));
		// Recalculate the total allocation size
		new_size = usr_new_size + redzone_size;
		// Check for overflow once at the end
		if (new_size < usr_new_size) {
			return NULL;
		}
	}

	void *new_ptr;
	if (zone->wrapped_zone->version >= 16) {
		new_ptr = DELEGATE(malloc_type_malloc, new_size, type_id);
	} else {
#if MALLOC_TARGET_64BIT
		malloc_set_tsd_type_descriptor(
				(malloc_type_descriptor_t){ .type_id = type_id });
#endif // MALLOC_TARGET_64BIT
		new_ptr = DELEGATE(malloc, new_size);
#if MALLOC_TARGET_64BIT
		malloc_set_tsd_type_descriptor(MALLOC_TYPE_DESCRIPTOR_NONE);
#endif // MALLOC_TARGET_64BIT
	}

#if !MALLOC_TARGET_EXCLAVES
	record_alloc_stacktrace(zone->depo, zone->map, new_ptr, usr_new_size);
#endif /* !MALLOC_TARGET_EXCLAVES */
	if (zone->debug) malloc_report(ASL_LEVEL_INFO, "realloc(%p, 0x%lx) = %p\n", ptr, new_size, new_ptr);

	if (ptr != NULL) {
		size_t old_redzone_size = 0;
		const size_t old_size = DELEGATE(size, ptr);
		void *old_ptr = __unsafe_forge_bidi_indexable(void *, ptr, old_size);
		if (zone->do_poisoning) {
			old_redzone_size = get_redzone_size(zone, old_ptr, old_size);
			MALLOC_ASSERT(old_size > old_redzone_size);
		}

		if (zone->debug) malloc_report(ASL_LEVEL_INFO, "realloc(%p, 0x%lx): size(%p) = 0x%lx - redzone 0x%lx)\n", ptr, new_size, old_ptr, old_size, old_redzone_size);

		// Don't free/quarantine the old pointer if allocation failed. Per man page:
		// > For realloc(), the input pointer is still valid if reallocation failed.
		if (new_ptr == NULL) {
			return NULL;
		}

		const size_t usr_old_size = old_size - old_redzone_size;
		memcpy(new_ptr, old_ptr, MIN(usr_old_size, usr_new_size));
		if (zone->do_poisoning) {
			poison_free(zone, old_ptr, old_size);
		}
		place_into_quarantine(zone, ptr, old_size);
	}

	if (new_ptr && zone->do_poisoning) {
		// Recalculate the redzone size to include allocator padding
		size_t actual_size = DELEGATE(size, new_ptr);
		MALLOC_ASSERT(actual_size >= new_size);
		redzone_size += actual_size - new_size;
		new_ptr = __unsafe_forge_bidi_indexable(void *, new_ptr,
				usr_new_size + redzone_size);
		poison_alloc(zone, new_ptr, usr_new_size, redzone_size);
	}
	return new_ptr;
}

static void * __alloc_size(3) __sized_by_or_null(new_size)
sanitizer_realloc(sanitizer_zone_t *zone, void * __unsafe_indexable ptr, size_t new_size)
{
	return sanitizer_malloc_type_realloc(zone, ptr, new_size,
			malloc_get_tsd_type_id());
}

static void
sanitizer_destroy(sanitizer_zone_t *zone)
{
#if !MALLOC_TARGET_EXCLAVES
	stacktrace_depo_destroy(zone->depo);
	pointer_map_destroy(zone->map);
	malloc_destroy_zone(zone->wrapped_zone);
	sanitizer_vm_deallocate((vm_address_t)zone, sizeof(sanitizer_zone_t));
#else
	(void)zone;
#endif /* !MALLOC_TARGET_EXCLAVES */
}

static void * __alloc_align(2) __alloc_size(3) __sized_by_or_null(size)
sanitizer_malloc_type_memalign(sanitizer_zone_t *zone, size_t align,
		size_t size, malloc_type_id_t type_id)
{
	if (!size) {
		size = 1;
	}
	size_t redzone_size = zone->redzone_size;
	const size_t usr_size = size;
	if (zone->do_poisoning) {
		// Recalculate the total allocation size
		size = usr_size + redzone_size;
		// Check for overflow once at the end
		if (size < usr_size) {
			return NULL;
		}
	}

	void *ptr;
	if (zone->wrapped_zone->version >= 16) {
		ptr = DELEGATE(malloc_type_memalign, align, size, type_id);
	} else {
#if MALLOC_TARGET_64BIT
		malloc_set_tsd_type_descriptor(
				(malloc_type_descriptor_t){ .type_id = type_id });
#endif // MALLOC_TARGET_64BIT
		ptr = DELEGATE(memalign, align, size);
#if MALLOC_TARGET_64BIT
		malloc_set_tsd_type_descriptor(MALLOC_TYPE_DESCRIPTOR_NONE);
#endif // MALLOC_TARGET_64BIT
	}

#if !MALLOC_TARGET_EXCLAVES
	record_alloc_stacktrace(zone->depo, zone->map, ptr, usr_size);
#endif /* !MALLOC_TARGET_EXCLAVES */
	if (zone->debug) malloc_report(ASL_LEVEL_INFO, "memalign(0x%lx, 0x%lx)\n", align, size);
	if (ptr && zone->do_poisoning) {
		// Recalculate the redzone size to include allocator padding
		size_t actual_size = DELEGATE(size, ptr);
		MALLOC_ASSERT(actual_size >= size);
		redzone_size += actual_size - size;
		ptr = __unsafe_forge_bidi_indexable(void *, ptr,
				usr_size + redzone_size);
		poison_alloc(zone, ptr, usr_size, redzone_size);
	}
	return ptr;
}

static void * __alloc_align(2) __alloc_size(3) __sized_by_or_null(size)
sanitizer_memalign(sanitizer_zone_t *zone, size_t align, size_t size)
{
	return sanitizer_malloc_type_memalign(zone, align, size,
			malloc_get_tsd_type_id());
}

static void * __alloc_align(2) __alloc_size(3) __sized_by_or_null(size)
sanitizer_malloc_type_malloc_with_options(sanitizer_zone_t *zone, size_t align,
		size_t size, malloc_zone_malloc_options_t options,
		malloc_type_id_t type_id)
{
#if CONFIG_MTE
	// rdar://140822174
	// When dyld interposition or a wrapper zone that does not support
	// forwarding malloc options is enabled, we need to set a flag in
	// the TSD to preserve the semantics of canonical tagging.
	bool use_tsd_fallback =
			(options & MALLOC_ZONE_MALLOC_OPTION_CANONICAL_TAG) &&
			(zone->wrapped_zone->version < 15 ||
			!zone->wrapped_zone->malloc_with_options);
#if !MALLOC_TARGET_EXCLAVES
	malloc_thread_options_t opts;
	if (use_tsd_fallback) {
		opts = malloc_get_thread_options();
		malloc_thread_options_t newopts = opts;
		newopts.ReservedFlag = true;
		_malloc_set_thread_options(newopts);
	}
#else
	MALLOC_ASSERT(!use_tsd_fallback);
#endif // MALLOC_TARGET_EXCLAVES
#endif // CONFIG_MTE

	void *ptr;
	if (!align) {
		ptr = sanitizer_malloc_type_malloc_noalign_with_options(zone, size,
			options, type_id);
	} else {
		ptr = sanitizer_malloc_type_memalign(zone, align, size, type_id);
		if (ptr && (options & MALLOC_ZONE_MALLOC_OPTION_CLEAR)) {
			bzero(ptr, size);
		}
	}

#if CONFIG_MTE
#if !MALLOC_TARGET_EXCLAVES
	// Restore the saved TSD flags
	if (use_tsd_fallback) {
		_malloc_set_thread_options(opts);
	}
#endif // MALLOC_TARGET_EXCLAVES
#endif // CONFIG_MTE

	return ptr;
}

static void * __alloc_align(2) __alloc_size(3) __sized_by_or_null(size)
sanitizer_malloc_with_options(sanitizer_zone_t *zone, size_t align, size_t size,
		malloc_zone_malloc_options_t options)
{
	return sanitizer_malloc_type_malloc_with_options(zone, align, size, options,
			malloc_get_tsd_type_id());
}

static void
sanitizer_free_definite_size(sanitizer_zone_t *zone, void * __sized_by(size) ptr, size_t size)
{
	if (zone->debug) malloc_report(ASL_LEVEL_INFO, "free_definite_size(%p, 0x%lx)\n", ptr, size);
	if (zone->do_poisoning) {
		// Provided size is the user accessible size, but we need the total size
		const size_t actual_size = DELEGATE(size, ptr);
		ptr = __unsafe_forge_bidi_indexable(void *, ptr, actual_size);
		size = actual_size;
		poison_free(zone, ptr, size);
	}
	place_into_quarantine(zone, ptr, size);
}

static bool
sanitizer_claimed_address(sanitizer_zone_t *zone, void * __unsafe_indexable ptr)
{
	return DELEGATE(claimed_address, ptr);
}

#pragma mark -
#pragma mark Introspection Functions

static kern_return_t
sanitizer_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t zone_address, memory_reader_t reader, vm_range_recorder_t recorder)
{
	return KERN_NOT_SUPPORTED;
}

static void
sanitizer_statistics(sanitizer_zone_t *zone, malloc_statistics_t *stats)
{
}

static kern_return_t
sanitizer_statistics_task(task_t task, vm_address_t zone_address, memory_reader_t reader, malloc_statistics_t *stats)
{
	return KERN_NOT_SUPPORTED;
}

static void
sanitizer_print(sanitizer_zone_t *zone, bool verbose)
{
}

static void
sanitizer_print_task(task_t task, unsigned level, vm_address_t zone_address, memory_reader_t reader, print_task_printer_t printer)
{
}

static void
sanitizer_log(sanitizer_zone_t *zone, void *address)
{
}

static size_t
sanitizer_good_size(sanitizer_zone_t *zone, size_t size)
{
	return DELEGATE(introspect->good_size, size);
}

static bool
sanitizer_check(sanitizer_zone_t *zone)
{
	return true; // Zone is always in a consistent state.
}

static void
sanitizer_force_lock(sanitizer_zone_t *zone)
{
	lock(zone);
}

static void
sanitizer_force_unlock(sanitizer_zone_t *zone)
{
	unlock(zone);
}

static void
sanitizer_reinit_lock(sanitizer_zone_t *zone)
{
	init_lock(zone);
}

static bool
sanitizer_zone_locked(sanitizer_zone_t *zone)
{
	bool lock_taken = trylock(zone);
	if (lock_taken) {
		unlock(zone);
	}
	return !lock_taken;
}

#if !MALLOC_TARGET_EXCLAVES
#pragma mark -
#pragma mark Crash Reporter API

static _malloc_lock_s crash_reporter_lock = _MALLOC_LOCK_INIT;

static crash_reporter_memory_reader_t g_crm_reader;
static const uint32_t k_max_read_memory = 1024;
static void *read_memory[k_max_read_memory];
static uint32_t num_read_memory;

static kern_return_t
memory_reader_adapter(task_t task, vm_address_t address, vm_size_t size, void **local_memory)
{
	MALLOC_ASSERT(num_read_memory < k_max_read_memory);
	void *ptr = g_crm_reader(task, address, size);
	*local_memory = ptr;
	read_memory[num_read_memory++] = ptr;
	return ptr ? KERN_SUCCESS : KERN_FAILURE;
}

static struct {
	vm_address_t address_to_lookup;
	vm_range_t found_range;
} enumeration_context;

static void
pointer_recorder(task_t task, void *context, unsigned type, vm_range_t * __counted_by(count) ranges, unsigned count)
{
	vm_address_t a = enumeration_context.address_to_lookup;
	for (int i = 0; i < count; i++) {
		if (ranges[i].address <= a && a < ranges[i].address + ranges[i].size) {
			enumeration_context.found_range = ranges[i];
			break;
		}
	}
}

kern_return_t
sanitizer_diagnose_fault_from_crash_reporter(vm_address_t fault_address, sanitizer_report_t *report,
		task_t task, vm_address_t zone_address, crash_reporter_memory_reader_t crm_reader)
{
	_malloc_lock_lock(&crash_reporter_lock);

	#define COPY_FROM_REMOTE(p, type) crm_reader(task, (vm_address_t)p, sizeof(type))
	sanitizer_zone_t *remote_zone = COPY_FROM_REMOTE(zone_address, sanitizer_zone_t);
	pointer_map_t *remote_pointer_map = COPY_FROM_REMOTE(remote_zone->map, pointer_map_t);
	stacktrace_depo_t *remote_depo = COPY_FROM_REMOTE(remote_zone->depo, stacktrace_depo_t);

	enumeration_context.found_range.address = 0;
	enumeration_context.found_range.size = 0;
	enumeration_context.address_to_lookup = fault_address;

	g_crm_reader = crm_reader;
	num_read_memory = 0;

	// We rely on being able to perform zone enumeration across different architecture slices on macOS.
	// On Apple Silicon Macs, ReportCrash is always running as a native (arm64e) process, but we also
	// need to be able to inspect x86_64 targets that are running under Rosetta. So the data layout and
	// zone logic needs to match between x86_64 and arm64(e).
	szone_introspect.enumerator(task, NULL, MALLOC_PTR_IN_USE_RANGE_TYPE, (vm_address_t)remote_zone->wrapped_zone,
								memory_reader_adapter, pointer_recorder);
	for (uint32_t i = 0; i < num_read_memory; i++) {
		_free(read_memory[i]);
	}
	g_crm_reader = NULL;

	bzero(report, sizeof(*report));
	report->fault_address = fault_address;

	if (enumeration_context.found_range.address != 0) {
		report->nearest_allocation = enumeration_context.found_range.address;
		report->allocation_size = enumeration_context.found_range.size;

		quarantined_chunk_t *chunk = COPY_FROM_REMOTE(enumeration_context.found_range.address, quarantined_chunk_t);
		uint32_t alloc_handle = (uint32_t)chunk->stacktrace_hashes;
		uint32_t dealloc_handle = (uint32_t)(chunk->stacktrace_hashes >> 32);

		report->alloc_trace.thread_id = 0;
		// Explicit cast (report->alloc_trace.frames) as it doesn't otherwise compile on watchOS (error: implicit conversion loses integer precision)
		report->alloc_trace.num_frames = (uint32_t)stacktrace_depo_find(remote_depo, alloc_handle,
				(uintptr_t *)report->alloc_trace.frames, countof(report->alloc_trace.frames));

		report->dealloc_trace.thread_id = 0;
		// Explicit cast (report->dealloc_trace.frames) as it doesn't otherwise compile on watchOS (error: implicit conversion loses integer precision)
		report->dealloc_trace.num_frames = (uint32_t)stacktrace_depo_find(remote_depo, dealloc_handle,
				(uintptr_t *)report->dealloc_trace.frames, countof(report->dealloc_trace.frames));

		_free(chunk);
	}

	_free(remote_depo);
	_free(remote_pointer_map);
	_free(remote_zone);

	_malloc_lock_unlock(&crash_reporter_lock);
	return KERN_SUCCESS;
}
#endif /* !MALLOC_TARGET_EXCLAVES */

#pragma mark -
#pragma mark Zone Templates

// Suppress warning: incompatible function pointer types
#define FN_PTR(fn) (void *)(&fn)

static malloc_introspection_t sanitizer_zone_introspect_template = {
	// Block and region enumeration
	.enumerator = FN_PTR(sanitizer_enumerator),

	// Statistics
	.statistics = FN_PTR(sanitizer_statistics),
	.task_statistics = FN_PTR(sanitizer_statistics_task),

	// Logging
	.print = FN_PTR(sanitizer_print),
	.print_task = FN_PTR(sanitizer_print_task),
	.log = FN_PTR(sanitizer_log),

	// Queries
	.good_size = FN_PTR(sanitizer_good_size),
	.check = FN_PTR(sanitizer_check),

	// Locking
	.force_lock = FN_PTR(sanitizer_force_lock),
	.force_unlock = FN_PTR(sanitizer_force_unlock),
	.reinit_lock = FN_PTR(sanitizer_reinit_lock),
	.zone_locked = FN_PTR(sanitizer_zone_locked),

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
	.zone_type = MALLOC_ZONE_TYPE_SANITIZER,
};

static const malloc_zone_t malloc_zone_template = {
	// Reserved for CFAllocator
	.reserved1 = NULL,
	.reserved2 = NULL,

	// Standard operations
	.size = FN_PTR(sanitizer_size),
	.malloc = FN_PTR(sanitizer_malloc),
	.calloc = FN_PTR(sanitizer_calloc),
	.valloc = FN_PTR(sanitizer_valloc),
	.free = FN_PTR(sanitizer_free),
	.realloc = FN_PTR(sanitizer_realloc),
	.destroy = FN_PTR(sanitizer_destroy),

	// Batch operations
	.batch_malloc = malloc_zone_batch_malloc_fallback,
	.batch_free = malloc_zone_batch_free_fallback,

	// Introspection
	.zone_name = "SanitizerMallocZone",
	.version = 16,
	.introspect = &sanitizer_zone_introspect_template,

	// Specialized operations
	.memalign = FN_PTR(sanitizer_memalign),
	.free_definite_size = FN_PTR(sanitizer_free_definite_size),
	.pressure_relief = malloc_zone_pressure_relief_fallback,
	.claimed_address = FN_PTR(sanitizer_claimed_address),
	.try_free_default = NULL,
	.malloc_with_options = FN_PTR(sanitizer_malloc_with_options),

	// Typed operations
	.malloc_type_malloc = FN_PTR(sanitizer_malloc_type_malloc),
	.malloc_type_calloc = FN_PTR(sanitizer_malloc_type_calloc),
	.malloc_type_realloc = FN_PTR(sanitizer_malloc_type_realloc),
	.malloc_type_memalign = FN_PTR(sanitizer_malloc_type_memalign),
	.malloc_type_malloc_with_options =
			FN_PTR(sanitizer_malloc_type_malloc_with_options),
};


#pragma mark -
#pragma mark Zone Configuration & Creation

bool
sanitizer_should_enable(void)
{
#if !MALLOC_TARGET_EXCLAVES
	return env_bool("MallocSanitizerZone") || env_bool("MallocQuarantineZone");
#elif __LIBLIBC_F_ASAN_INSTRUMENTATION
	return true;
#else
	return false;
#endif /* !MALLOC_TARGET_EXCLAVES */
}

void
sanitizer_reset_environment(void)
{
#if !MALLOC_TARGET_EXCLAVES
	// Unset MallocSanitizerZone from the environment to avoid propagating it
	// to any child processes (posix_spawn, exec, fork).
	unsetenv("MallocSanitizerZone");
	unsetenv("MallocQuarantineZone");
#endif /* !MALLOC_TARGET_EXCLAVES */
}

malloc_zone_t *
sanitizer_create_zone(malloc_zone_t *wrapped_zone)
{
#if !MALLOC_TARGET_EXCLAVES
	sanitizer_zone_t *zone = __unsafe_forge_single(sanitizer_zone_t *,
		sanitizer_vm_map(sizeof(sanitizer_zone_t), VM_PROT_READ | VM_PROT_WRITE,
			VM_MEMORY_MALLOC));
#else
	sanitizer_zone_t *zone = &sanitizer_zone;
#endif /* !MALLOC_TARGET_EXCLAVES */
	zone->malloc_zone = malloc_zone_template;

#if !MALLOC_TARGET_EXCLAVES
	// Since we are calling szone_introspect.enumerator directly, see
	// sanitizer_diagnose_fault_from_crash_reporter.
	MALLOC_ASSERT(wrapped_zone->introspect == &szone_introspect);
#endif /* !MALLOC_TARGET_EXCLAVES */
	zone->wrapped_zone = wrapped_zone;

	if (wrapped_zone->version < 13) {
		malloc_report(MALLOC_REPORT_CRASH,
				"Unsupported wrapped zone version: %u\n",
				wrapped_zone->version);
	}

#if !MALLOC_TARGET_EXCLAVES
	zone->debug = env_bool("MallocSanitizerZoneDebug");
	zone->do_poisoning = !env_bool("MallocSanitizerNoPoisoning");
	zone->redzone_size = env_uint("MallocSanitizerRedzoneSize", 16); // default is 16 bytes
#else
	zone->debug = false;
	zone->do_poisoning = true;
	zone->redzone_size = 16;
#endif /* !MALLOC_TARGET_EXCLAVES */
	MALLOC_ASSERT((zone->redzone_size % ASAN_SHADOW_ALIGNMENT) == 0);
#if !MALLOC_TARGET_EXCLAVES
	zone->max_items_in_quarantine = env_uint("MallocQuarantineMaxItems", 0); // default is 0 = unlimited
	zone->max_bytes_in_quarantine = (size_t)env_uint("MallocQuarantineMaxSizeInMB", 256) << 20; // 256 MB is default
#else
	zone->max_items_in_quarantine = 0;
	zone->max_bytes_in_quarantine = 256 << 20;
#endif /* !MALLOC_TARGET_EXCLAVES */

#if !MALLOC_TARGET_EXCLAVES
	zone->depo = stacktrace_depo_create();
	zone->map = pointer_map_create();
#endif /* !MALLOC_TARGET_EXCLAVES */

	// Init mutable state
	init_lock(zone);
#if !MALLOC_TARGET_EXCLAVES
	sanitizer_vm_protect((vm_address_t)zone, PAGE_MAX_SIZE, VM_PROT_READ);
#endif /* !MALLOC_TARGET_EXCLAVES */
	return __unsafe_forge_single(malloc_zone_t *, zone);
}

#else // CONFIG_SANITIZER

kern_return_t
sanitizer_diagnose_fault_from_crash_reporter(vm_address_t fault_address, sanitizer_report_t *report,
		task_t task, vm_address_t zone_address, crash_reporter_memory_reader_t crm_reader)
{
	return KERN_NOT_SUPPORTED;
}

#endif // CONFIG_SANITIZER
