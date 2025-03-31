/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
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

#if !MALLOC_TARGET_EXCLAVES
static volatile uintptr_t entropic_address = 0;
static volatile uintptr_t entropic_base = 0;
static volatile uintptr_t entropic_limit = 0;
#endif // !MALLOC_TARGET_EXCLAVES

MALLOC_NOEXPORT
uint64_t malloc_entropy[2] = {0, 0};

#define ENTROPIC_KABILLION 0x10000000 /* 256Mb */
#define ENTROPIC_USER_RANGE_SIZE 0x200000000ULL /* 8Gb */

// <rdar://problem/22277891> align 64bit ARM shift to 32MB PTE entries
#if MALLOC_TARGET_IOS && MALLOC_TARGET_64BIT
#define ENTROPIC_SHIFT 25
#else // MALLOC_TARGET_IOS && MALLOC_TARGET_64BIT
#define ENTROPIC_SHIFT SMALL_BLOCKS_ALIGN
#endif

void
mvm_aslr_init(void)
{
	// Prepare ASLR
#if MALLOC_TARGET_EXCLAVES
	arc4random_buf(malloc_entropy, sizeof(malloc_entropy));
#elif defined(__i386__) || defined(__x86_64__) || defined(__arm64__) || TARGET_OS_DRIVERKIT || (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
#if defined(__i386__)
	uintptr_t stackbase = 0x8fe00000;
	int entropic_bits = 3;
#elif defined(__x86_64__)
	uintptr_t stackbase = USRSTACK64;
	int entropic_bits = 16;
#elif defined(__arm64__)
#if defined(__LP64__)
	uintptr_t stackbase = USRSTACK64;
	int entropic_bits = 7;
#else // __LP64__
	uintptr_t stackbase = USRSTACK;
	int entropic_bits = 3;
#endif
#else
	uintptr_t stackbase = USRSTACK;
	int entropic_bits = 3;
#endif
	// assert(((1 << entropic_bits) - 1) << SMALL_BLOCKS_ALIGN < (stackbase - MAXSSIZ - ENTROPIC_KABILLION));

	if (mvm_aslr_enabled()) {
		if (0 == entropic_address) {
			uintptr_t t = stackbase - MAXSSIZ - ((uintptr_t)(malloc_entropy[1] &
				((1 << entropic_bits) - 1)) << ENTROPIC_SHIFT);
#if MALLOC_TARGET_IOS && MALLOC_TARGET_64BIT
			uintptr_t addr = 0;

			/* If kernel VM user ranges are enabled mach_vm_allocate/map will provide memory
			 * in the upper VM address range. This range is randomized per process. For now
			 * we do not have this metadata plumbed through so we make a single allocation
			 * with the appropriate tag to determine where our heap is. If we are given an
			 * allocation above where we expect then we can safely assume VM ranges are enabled.
			 *
			 * If so we do not need to apply further entropy but do need to ensure
			 * we mask off the address to a PTE boundary.
			 */ 
			if (KERN_SUCCESS == mach_vm_allocate(mach_task_self(), (mach_vm_address_t *)&addr,
					vm_page_quanta_size, VM_FLAGS_ANYWHERE | VM_MAKE_TAG(VM_MEMORY_MALLOC_TINY))) {
				// Fall through and use existing base if addr < stackbase
				if (addr > stackbase) {
					t = (addr + ENTROPIC_USER_RANGE_SIZE) & ~((1 << ENTROPIC_SHIFT) - 1);
					OSAtomicCompareAndSwapLong(0, addr, (volatile long *)&entropic_base);
				}

				mach_vm_deallocate(mach_task_self(), addr, vm_page_quanta_size);
			}
#endif // MALLOC_TARGET_IOS && MALLOC_TARGET_64BIT

			OSAtomicCompareAndSwapLong(0, t, (volatile long *)&entropic_limit);
			OSAtomicCompareAndSwapLong(0, t - ENTROPIC_KABILLION, (volatile long *)&entropic_address);
		}
	} else {
		// zero slide when ASLR has been disabled by boot-arg. Eliminate cloaking.
		malloc_entropy[0] = 0;
		malloc_entropy[1] = 0;
	}
#else // TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
#error ASLR unhandled on this platform
#endif // TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
}

void * __sized_by_or_null(size)
mvm_allocate_plat(uintptr_t addr, size_t size, uint8_t align, int flags, int debug_flags, int vm_page_label, plat_map_t *map_out)
{
	void * __unsafe_indexable mapped;
	kern_return_t kr;

	if (addr && (flags & VM_FLAGS_ANYWHERE)) {
		// Pass MALLOC_ABORT_ON_ERROR to make this call abort
		malloc_zone_error(MALLOC_ABORT_ON_ERROR | debug_flags, false,
			"Unsupported anywhere allocation at address 0x%lx of size 0x%lx with flags %d\n",
			(unsigned long) addr, (unsigned long) size, flags);
	}
#if MALLOC_TARGET_EXCLAVES
	// Memory will be reserved and/or populated, and the handle initialized
	const _liblibc_map_type_t type = LIBLIBC_MAP_TYPE_PRIVATE |
			((flags & VM_FLAGS_ANYWHERE) ? LIBLIBC_MAP_TYPE_NONE : LIBLIBC_MAP_TYPE_FIXED) |
			((debug_flags & MALLOC_CAN_FAULT) ? LIBLIBC_MAP_TYPE_FAULTABLE : LIBLIBC_MAP_TYPE_NONE) |
			((debug_flags & MALLOC_NO_POPULATE) ? LIBLIBC_MAP_TYPE_NOCOMMIT : LIBLIBC_MAP_TYPE_NONE) |
			((debug_flags & DISABLE_ASLR) ? LIBLIBC_MAP_TYPE_NORAND : LIBLIBC_MAP_TYPE_NONE);
	const _liblibc_map_perm_t perm = LIBLIBC_MAP_PERM_READ |
			LIBLIBC_MAP_PERM_WRITE;
	mapped = mmap_plat(map_out, addr, size, perm, type, align,
			(unsigned)vm_page_label);
	kr = errno;
	// This message is not printed on non-exclaves targets. Certain code paths,
	// like xzm_segment_group_try_realloc_huge_chunk, may fail under normal
	// conditions, and would print a spurious message, but are disabled on
	// exclaves.
	if (!mapped) {
		malloc_zone_error(debug_flags, false,
			"Failed to allocate memory at address 0x%lx of size 0x%lx with flags %d: %d\n", addr, size, flags, kr);
	}
#else
	(void)map_out;
	if (debug_flags & (MALLOC_CAN_FAULT | MALLOC_NO_POPULATE)) {
		// Pass MALLOC_ABORT_ON_ERROR to make this call abort
		malloc_zone_error(MALLOC_ABORT_ON_ERROR | debug_flags, false,
				"Unsupported unpopulated allocation at address 0x%lx of size 0x%lx with flags %d\n",
				(unsigned long) addr, (unsigned long) size, flags);
	}


	mach_vm_address_t vm_addr = addr;
	mach_vm_offset_t allocation_mask = ((mach_vm_offset_t)1 << align) - 1;
	kr = mach_vm_map(mach_task_self(), &vm_addr, (mach_vm_size_t)size,
			allocation_mask, flags | VM_MAKE_TAG(vm_page_label),
			MEMORY_OBJECT_NULL, 0, FALSE, VM_PROT_DEFAULT, VM_PROT_ALL,
			VM_INHERIT_DEFAULT);
	mapped = (kr == KERN_SUCCESS) ? (void *)vm_addr : NULL;
#endif // MALLOC_TARGET_EXCLAVES

	return __unsafe_forge_bidi_indexable(void *, mapped, size);
}

void * __sized_by_or_null(size)
mvm_allocate_pages(size_t size, uint8_t align, uint32_t debug_flags,
		int vm_page_label)
{
	return mvm_allocate_pages_plat(size, align, debug_flags, vm_page_label, NULL);
}

void * __sized_by_or_null(size)
mvm_allocate_pages_plat(size_t size, uint8_t align, uint32_t debug_flags,
		int vm_page_label, plat_map_t *map_out)
{
#if MALLOC_TARGET_EXCLAVES
	return mvm_allocate_plat(0, size, align, VM_FLAGS_ANYWHERE, debug_flags, vm_page_label, map_out);
#else
	(void)map_out;
	boolean_t add_prelude_guard_page = debug_flags & MALLOC_ADD_PRELUDE_GUARD_PAGE;
	boolean_t add_postlude_guard_page = debug_flags & MALLOC_ADD_POSTLUDE_GUARD_PAGE;
	boolean_t purgeable = debug_flags & MALLOC_PURGEABLE;
	boolean_t use_entropic_range = !(debug_flags & DISABLE_ASLR);
	mach_vm_address_t vm_addr;
	uintptr_t addr;
	mach_vm_size_t allocation_size = round_page_quanta(size);
	mach_vm_offset_t allocation_mask = ((mach_vm_offset_t)1 << align) - 1;
	int alloc_flags = VM_FLAGS_ANYWHERE | VM_MAKE_TAG(vm_page_label);
	kern_return_t kr;

	if (!allocation_size) {
		allocation_size = vm_page_quanta_size;
	}
	if (add_postlude_guard_page || add_prelude_guard_page) {
		if (add_prelude_guard_page && align > vm_page_quanta_shift) {
			/* <rdar://problem/16601499> alignment greater than pagesize needs more work */
			allocation_size += (1 << align) + large_vm_page_quanta_size;
		} else {
			allocation_size += add_prelude_guard_page && add_postlude_guard_page ?
					2 * large_vm_page_quanta_size : large_vm_page_quanta_size;
		}
	}

	if (purgeable) {
		alloc_flags |= VM_FLAGS_PURGABLE;
	}


	if (allocation_size < size) { // size_t arithmetic wrapped!
		return NULL;
	}

retry:
	vm_addr = use_entropic_range ? entropic_address : vm_page_quanta_size;
	kr = mach_vm_map(mach_task_self(), &vm_addr, allocation_size,
			allocation_mask, alloc_flags, MEMORY_OBJECT_NULL, 0, FALSE,
			VM_PROT_DEFAULT, VM_PROT_ALL, VM_INHERIT_DEFAULT);
	if (kr == KERN_NO_SPACE && use_entropic_range) {
		vm_addr = vm_page_quanta_size;
		kr = mach_vm_map(mach_task_self(), &vm_addr, allocation_size,
				allocation_mask, alloc_flags, MEMORY_OBJECT_NULL, 0, FALSE,
				VM_PROT_DEFAULT, VM_PROT_ALL, VM_INHERIT_DEFAULT);
	}
	if (kr) {
		if (kr != KERN_NO_SPACE) {
			malloc_zone_error(debug_flags, false, "can't allocate region\n:"
					"*** mach_vm_map(size=%lu, flags: %x) failed (error code=%d)\n",
					size, debug_flags, kr);
		}
		return NULL;
	}
	addr = (uintptr_t)vm_addr;

	if (use_entropic_range) {
		// Don't allow allocation to rise above entropic_limit (for tidiness).
		if (addr + allocation_size > entropic_limit) { // Exhausted current range?
			uintptr_t t = entropic_address;
			uintptr_t u = t - ENTROPIC_KABILLION;

			// provided we don't wrap, deallocate and retry, in theexpanded
			// entropic range
			if (u < t && u >= entropic_base) {
				mach_vm_deallocate(mach_task_self(), vm_addr, allocation_size);
				OSAtomicCompareAndSwapLong(t, u,
						(volatile long *)&entropic_address);  // Just one reduction please
				goto retry;
			}
			// fall through to use what we got
		}
		
		if (addr < entropic_address) { // we wrapped to find this allocation, expand the entropic range
			uintptr_t t = entropic_address;
			uintptr_t u = t - ENTROPIC_KABILLION;
			if (u < t && u >= entropic_base) {
				OSAtomicCompareAndSwapLong(t, u, (volatile long *)&entropic_address);  // Just one reduction please
			}
			// fall through to use what we got
		}
	}

	if (add_postlude_guard_page || add_prelude_guard_page) {
		if (add_prelude_guard_page && align > vm_page_quanta_shift) {
			/* <rdar://problem/16601499> calculate the first address inside the alignment padding
			 * where we can place the guard page and still be aligned.
			 *
			 * |-----------------------------------------------------------|
			 * |leading|gp|                  alloc                  |gp| t |
			 * |-----------------------------------------------------------|
			 */
			uintptr_t alignaddr = ((addr + large_vm_page_quanta_size) + (1 << align) - 1) & ~((1 << align) - 1);
			size_t leading = alignaddr - addr - large_vm_page_quanta_size;
			size_t trailing = (1 << align) - large_vm_page_quanta_size - leading;

			/* Unmap the excess area. */
			kr = mach_vm_deallocate(mach_task_self(), addr, leading);
			if (kr) {
				malloc_zone_error(debug_flags, false, "can't unmap excess guard region\n"
						"*** mach_vm_deallocate(addr=%p, size=%lu) failed (code=%d)\n",
						(void *)addr, leading, kr);
				return NULL;
			}

			if (trailing) {
				kr = mach_vm_deallocate(mach_task_self(), addr + allocation_size - trailing, trailing);
				if (kr) {
					malloc_zone_error(debug_flags, false, "can't unmap excess trailing guard region\n"
							"*** mach_vm_deallocate(addr=%p, size=%lu) failed (code=%d)\n",
							(void *)(addr + allocation_size - trailing), trailing, kr);
					return NULL;
				}
			}

			addr = alignaddr;
		} else if (add_prelude_guard_page) {
			addr += large_vm_page_quanta_size;
		}
		mvm_protect_plat((void *)addr, size, PROT_NONE, debug_flags, map_out);
	}
	return (void *)addr;
#endif // MALLOC_TARGET_EXCLAVES
}

void
mvm_deallocate_plat(void * __sized_by(size) addr, size_t size, int debug_flags, plat_map_t *map)
{
	kern_return_t kr;

#if MALLOC_TARGET_EXCLAVES
	kr = munmap_plat(map, addr, size) ? KERN_SUCCESS : errno;
#else
	(void)map;
	kr = mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)addr,
			(mach_vm_size_t)size);
#endif // MALLOC_TARGET_EXCLAVES

	if (kr != KERN_SUCCESS) {
		malloc_zone_error(debug_flags, false,
			"Failed to deallocate at address %p of size 0x%lx: %d\n", addr, size, kr);
	}
}

void
mvm_deallocate_pages(void * __sized_by(size) addr, size_t size,
		unsigned debug_flags)
{
	mvm_deallocate_pages_plat(addr, size, debug_flags, NULL);
}

void
mvm_deallocate_pages_plat(void * __sized_by(size) addr, size_t size,
		unsigned debug_flags, plat_map_t *map)
{
#if MALLOC_TARGET_EXCLAVES
	if (debug_flags & (MALLOC_ADD_GUARD_PAGE_FLAGS | MALLOC_PURGEABLE)) {
		malloc_zone_error(MALLOC_ABORT_ON_ERROR | debug_flags, true,
			"Unsupported deallocation debug flags %u\n", debug_flags);
	}
	mvm_deallocate_plat(addr, size, debug_flags, map);
#else
	(void)map;
	boolean_t added_prelude_guard_page = debug_flags & MALLOC_ADD_PRELUDE_GUARD_PAGE;
	boolean_t added_postlude_guard_page = debug_flags & MALLOC_ADD_POSTLUDE_GUARD_PAGE;
	mach_vm_address_t vm_addr = (mach_vm_address_t)addr;
	mach_vm_size_t allocation_size = size;

	if (added_prelude_guard_page) {
		vm_addr -= large_vm_page_quanta_size;
		allocation_size += large_vm_page_quanta_size;
	}
	if (added_postlude_guard_page) {
		allocation_size += large_vm_page_quanta_size;
	}
	mvm_deallocate_plat(__unsafe_forge_bidi_indexable(void *, vm_addr,
			allocation_size), (size_t)allocation_size, debug_flags, NULL);
#endif // MALLOC_TARGET_EXCLAVES
}

void
mvm_protect(void * __sized_by(size) address, size_t size, unsigned protection,
		unsigned debug_flags)
{
	mvm_protect_plat(address, size, protection, debug_flags, NULL);
}

void
mvm_protect_plat(void * __sized_by(size) address, size_t size, unsigned protection,
		unsigned debug_flags, plat_map_t *map)
{
#if MALLOC_TARGET_EXCLAVES
	const _liblibc_map_perm_t perm =
		((protection & PROT_READ) ? LIBLIBC_MAP_PERM_READ : LIBLIBC_MAP_PERM_NONE) |
		((protection & PROT_WRITE) ? LIBLIBC_MAP_PERM_WRITE : LIBLIBC_MAP_PERM_NONE) |
		((protection & PROT_EXEC) ? LIBLIBC_MAP_PERM_EXECUTE : LIBLIBC_MAP_PERM_NONE);
	if (debug_flags & (MALLOC_ADD_GUARD_PAGE_FLAGS | MALLOC_PURGEABLE)) {
		malloc_zone_error(MALLOC_ABORT_ON_ERROR | debug_flags, true,
			"Unsupported deallocation debug flags %u\n", debug_flags);
	}
	if (!mprotect_plat(map, address, size, perm)) {
		malloc_zone_error(MALLOC_ABORT_ON_ERROR | debug_flags, true,
			"Unsupported deallocation address %p or size %lu: %d\n", address, size, errno);
	}
#else
	(void)map;
	kern_return_t err;

	if ((debug_flags & MALLOC_ADD_PRELUDE_GUARD_PAGE) && !(debug_flags & MALLOC_DONT_PROTECT_PRELUDE)) {
		err = mprotect((void *)((uintptr_t)address - large_vm_page_quanta_size), large_vm_page_quanta_size, protection);
		if (err) {
			malloc_report(ASL_LEVEL_ERR, "*** can't mvm_protect(%u) region for prelude guard page at %p\n", protection,
					(void *)((uintptr_t)address - large_vm_page_quanta_size));
		}
	}
	if ((debug_flags & MALLOC_ADD_POSTLUDE_GUARD_PAGE) && !(debug_flags & MALLOC_DONT_PROTECT_POSTLUDE)) {
		err = mprotect((void *)(round_page_quanta(((uintptr_t)address + size))), large_vm_page_quanta_size, protection);
		if (err) {
			malloc_report(ASL_LEVEL_ERR, "*** can't mvm_protect(%u) region for postlude guard page at %p\n", protection,
					(void *)((uintptr_t)address + size));
		}
	}
#endif // MALLOC_TARGET_EXCLAVES
}

int
mvm_madvise(void * __sized_by(sz) addr, size_t sz, int advice, unsigned debug_flags)
{
	return mvm_madvise_plat(addr, sz, advice, debug_flags, NULL);
}

int
mvm_madvise_plat(void * __sized_by(sz) addr, size_t sz, int advice, unsigned debug_flags, plat_map_t *map)
{
	kern_return_t kr;

#if MALLOC_TARGET_EXCLAVES
	if ((debug_flags & (MALLOC_ADD_GUARD_PAGE_FLAGS | MALLOC_PURGEABLE))) {
		malloc_zone_error(MALLOC_ABORT_ON_ERROR | debug_flags, true,
			"Unsupported debug flags %u\n", debug_flags);
	}

	kr = !madvise_plat(map, addr, sz, advice) ? KERN_SUCCESS : errno;
	if (kr != KERN_SUCCESS) {
		malloc_zone_error(debug_flags, false,
			"Failed to madvise %d at address %p of size 0x%lx: %d\n", advice,
			addr, sz, kr);
	}
#else
	(void)map;
	kr = !madvise(addr, sz, advice) ? KERN_SUCCESS : errno;
#endif // MALLOC_TARGET_EXCLAVES

	return !(kr == KERN_SUCCESS);
}

int
mvm_madvise_free(void *rack, void *r, uintptr_t pgLo, uintptr_t pgHi, uintptr_t *last, boolean_t scribble)
{
	return mvm_madvise_free_plat(rack, r, pgLo, pgHi, last, scribble, NULL);
}

int
mvm_madvise_free_plat(void *rack, void *r, uintptr_t pgLo, uintptr_t pgHi, uintptr_t *last, boolean_t scribble, plat_map_t *map)
{
	if (pgHi > pgLo) {
		size_t len = pgHi - pgLo;
		void *ptr = __unsafe_forge_bidi_indexable(void *, pgLo, len);

		if (scribble && malloc_zero_policy != MALLOC_ZERO_ON_FREE) {
			memset(ptr, SCRUBBLE_BYTE, len); // Scribble on MADV_FREEd memory
		}

#if MALLOC_TARGET_IOS
		if (last) {
			if (*last == pgLo) {
				return 0;
			}

			*last = pgLo;
		}
#endif // MALLOC_TARGET_IOS

#if MALLOC_TARGET_EXCLAVES
		if (mvm_madvise_plat(ptr, len, CONFIG_MADVISE_STYLE, 0, map)) {
			return 1;
		}
#else
		MAGMALLOC_MADVFREEREGION(rack, r, (void *)pgLo, (int)len); // DTrace USDT Probe
		if (mvm_madvise(ptr, len, CONFIG_MADVISE_STYLE, 0)) {
			/* -1 return: VM map entry change makes this unfit for reuse. Something evil lurks. */
#if DEBUG_MADVISE
			malloc_zone_error(NULL, false,
					"madvise_free_range madvise(..., MADV_FREE_REUSABLE) failed for %p, length=%d\n",
					(void *)pgLo, len);
#endif // DEBUG_MADVISE
			return 1;
		} else {
			MALLOC_TRACE(TRACE_madvise, (uintptr_t)r, (uintptr_t)pgLo, len, CONFIG_MADVISE_STYLE);
		}
#endif // MALLOC_TARGET_EXCLAVES
	}
	return 0;
}

#if CONFIG_MAGAZINE_DEFERRED_RECLAIM
static mach_vm_reclaim_ring_t reclaim_buffer;
static _malloc_lock_s reclaim_buffer_lock = _MALLOC_LOCK_INIT;

mach_vm_reclaim_error_t
mvm_deferred_reclaim_init(void)
{
	// Pick a sane minimum number of entries and let vm_reclaim round up
	// to a page boundary. The intention is for the initial size to be
	// one page. We don't support ringbuffer growth on the legacy DRC, so
	// the maximum size will be unmodified.
	mach_vm_reclaim_count_t capacity = mach_vm_reclaim_round_capacity(512);
	return mach_vm_reclaim_ring_allocate(&reclaim_buffer, capacity, capacity);
}


bool
mvm_reclaim_mark_used(mach_vm_reclaim_id_t id, mach_vm_address_t ptr, mach_vm_size_t size, unsigned int debug_flags)
{
	mach_vm_reclaim_error_t kr;
	mach_vm_reclaim_state_t state;
	bool update_accounting;

	if (id == VM_RECLAIM_ID_NULL) {
		// Region was never entered into ring
		// FIXME: Understand why the all cache entries aren't being
		// assigned reclaim IDs (rdar://137709029)
		return true;
	}

	if (debug_flags & MALLOC_ADD_GUARD_PAGE_FLAGS) {
		if (os_add_overflow(size, 2 * large_vm_page_quanta_size, &size)) {
			return false;
		}
		ptr -= large_vm_page_quanta_size;
	}
	_malloc_lock_lock(&reclaim_buffer_lock);
	kr = mach_vm_reclaim_try_cancel(reclaim_buffer, id, ptr, size,
			VM_RECLAIM_DEALLOCATE, &state, &update_accounting);
	MALLOC_ASSERT(kr == VM_RECLAIM_SUCCESS);
	_malloc_lock_unlock(&reclaim_buffer_lock);
	if (update_accounting) {
		mach_vm_reclaim_update_kernel_accounting(reclaim_buffer);
	}
	return mach_vm_reclaim_is_reusable(state);
}

mach_vm_reclaim_id_t
mvm_reclaim_mark_free(mach_vm_address_t ptr, mach_vm_size_t size, unsigned int debug_flags)
{
	mach_vm_reclaim_error_t kr;
	mach_vm_reclaim_id_t id;
	bool should_update_kernel_accounting = false;
	if (debug_flags & MALLOC_ADD_GUARD_PAGE_FLAGS) {
		if (os_add_overflow(size, 2 * large_vm_page_quanta_size, &size)) {
			return VM_RECLAIM_ID_NULL;
		}
		ptr -= large_vm_page_quanta_size;
	}

	_malloc_lock_lock(&reclaim_buffer_lock);

	do {
		id = VM_RECLAIM_ID_NULL;
		kr = mach_vm_reclaim_try_enter(reclaim_buffer, ptr, size,
				VM_RECLAIM_DEALLOCATE, &id, &should_update_kernel_accounting);
		MALLOC_ASSERT(kr == VM_RECLAIM_SUCCESS);
		if (id == VM_RECLAIM_ID_NULL) {
			mach_vm_reclaim_count_t capacity;
			kr = mach_vm_reclaim_ring_capacity(reclaim_buffer, &capacity);
			MALLOC_ASSERT(kr == VM_RECLAIM_SUCCESS);
			kr = mach_vm_reclaim_ring_flush(reclaim_buffer, capacity);
			MALLOC_ASSERT(kr == VM_RECLAIM_SUCCESS);
		}
	} while (id == VM_RECLAIM_ID_NULL);

	_malloc_lock_unlock(&reclaim_buffer_lock);

	if (should_update_kernel_accounting) {
		mach_vm_reclaim_update_kernel_accounting(reclaim_buffer);
	}
	return id;
}

bool
mvm_reclaim_is_available(mach_vm_reclaim_id_t id)
{
	mach_vm_reclaim_error_t err;
	mach_vm_reclaim_state_t state;

	if (id == VM_RECLAIM_ID_NULL) {
		// Region was never entered into ring
		// FIXME: Understand why the all cache entries aren't being
		// assigned reclaim IDs (rdar://137709029)
		return true;
	}

	err = mach_vm_reclaim_query_state(reclaim_buffer, id, VM_RECLAIM_DEALLOCATE, &state);
	MALLOC_ASSERT(err == VM_RECLAIM_SUCCESS);
	return mach_vm_reclaim_is_reusable(state);
}
#endif // CONFIG_MAGAZINE_DEFERRED_RECLAIM
