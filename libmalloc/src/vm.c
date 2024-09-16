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
	if (addr && (flags & VM_FLAGS_ANYWHERE)) {
		// Pass MALLOC_ABORT_ON_ERROR to make this call abort
		malloc_zone_error(MALLOC_ABORT_ON_ERROR | debug_flags, false,
			"Unsupported anywhere allocation at address 0x%lx of size 0x%lx with flags %d\n",
			(unsigned long) addr, (unsigned long) size, flags);
	}
#if MALLOC_TARGET_EXCLAVES
	// This call can have different behavior depending on `flags` and `map_out`:
	// 1. If the input handle is invalid and MALLOC_NO_POPULATE is not present,
	//	  the handle is initialized and memory is both reserved and populated
	// 2. If the input handle is invalid and MALLOC_NO_POPULATE is present,
	//	  the handle is initialized and memory is only reserved
	// 3. If the input handle is valid and MALLOC_NO_POPULATE is not present,
	//    memory is populated
	const _liblibc_map_type_t type = LIBLIBC_MAP_TYPE_PRIVATE |
		((flags & VM_FLAGS_ANYWHERE) ? LIBLIBC_MAP_TYPE_NONE : LIBLIBC_MAP_TYPE_FIXED) |
		((debug_flags & MALLOC_NO_POPULATE) ? LIBLIBC_MAP_TYPE_NOCOMMIT : LIBLIBC_MAP_TYPE_NONE) |
		((debug_flags & DISABLE_ASLR) ? LIBLIBC_MAP_TYPE_NORAND : LIBLIBC_MAP_TYPE_NONE);
	const _liblibc_map_perm_t perm = LIBLIBC_MAP_PERM_READ | LIBLIBC_MAP_PERM_WRITE;
	void * __unsafe_indexable map = mmap_plat(map_out, addr, size, perm,
			type, align, (unsigned)vm_page_label);
	if (!map) {
		malloc_zone_error(debug_flags, false,
			"Failed to allocate memory at address 0x%lx of size 0x%lx with flags %d\n", addr, size, flags);
	}
	return __unsafe_forge_bidi_indexable(void *, map, size);
#else
	(void)map_out;
	if (debug_flags & MALLOC_NO_POPULATE) {
		// Pass MALLOC_ABORT_ON_ERROR to make this call abort
		malloc_zone_error(MALLOC_ABORT_ON_ERROR | debug_flags, false,
				"Unsupported unpopulated allocation at address 0x%lx of size 0x%lx with flags %d\n",
				(unsigned long) addr, (unsigned long) size, flags);
	}


	mach_vm_address_t vm_addr = addr;
	mach_vm_offset_t allocation_mask = ((mach_vm_offset_t)1 << align) - 1;
	kern_return_t kr = mach_vm_map(mach_task_self(), &vm_addr,
			(mach_vm_size_t)size, allocation_mask,
			flags | VM_MAKE_TAG(vm_page_label), MEMORY_OBJECT_NULL, 0, FALSE,
			VM_PROT_DEFAULT, VM_PROT_ALL, VM_INHERIT_DEFAULT);
	if (kr) {
		return NULL;
	}
	return __unsafe_forge_bidi_indexable(void *, vm_addr, size);
#endif // MALLOC_TARGET_EXCLAVES
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
#if MALLOC_TARGET_EXCLAVES
	if (!munmap_plat(map, addr, size)) {
		malloc_zone_error(debug_flags, false,
			"Failed to deallocate at address %p of size 0x%lx\n", addr, size);
	}
#else
	(void)map;
	kern_return_t kr = mach_vm_deallocate(mach_task_self(),
		(mach_vm_address_t)addr, (mach_vm_size_t)size);
	if (kr) {
		malloc_zone_error(debug_flags, false,
			"Failed to deallocate at address %p of size 0x%lx\n", addr, size);
	}
#endif // MALLOC_TARGET_EXCLAVES
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
			"Unsupported deallocation address %p or size %lu\n", address, size);
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
#if MALLOC_TARGET_EXCLAVES
	if (!(advice == MADV_FREE || advice == MADV_FREE_REUSABLE) ||
		(debug_flags & (MALLOC_ADD_GUARD_PAGE_FLAGS | MALLOC_PURGEABLE))) {
		malloc_zone_error(MALLOC_ABORT_ON_ERROR | debug_flags, true,
			"Unsupported allocation advice %d or debug flags %u\n", advice, debug_flags);
	}

	if (!madvise_plat(map, addr, sz, LIBLIBC_MAP_HINT_UNUSED)) {
		return 1;
	}
#else
	(void)map;
	if (madvise(addr, sz, advice) == -1) {
		return 1;
	}
#endif // MALLOC_TARGET_EXCLAVES
	return 0;
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

#if CONFIG_DEFERRED_RECLAIM
static struct mach_vm_reclaim_ringbuffer_v1_s reclaim_buffer;
static _malloc_lock_s reclaim_buffer_lock = _MALLOC_LOCK_INIT;

kern_return_t
mvm_deferred_reclaim_init(void)
{
	return mach_vm_reclaim_ringbuffer_init(&reclaim_buffer);
}


bool
mvm_reclaim_mark_used(uint64_t id, mach_vm_address_t ptr, uint32_t size, unsigned int debug_flags)
{
	bool used;
	if (debug_flags & MALLOC_ADD_GUARD_PAGE_FLAGS) {
		if (os_add_overflow(size, 2 * large_vm_page_quanta_size, &size)) {
			return false;
		}
		ptr -= large_vm_page_quanta_size;
	}
	_malloc_lock_lock(&reclaim_buffer_lock);
	used = mach_vm_reclaim_mark_used(&reclaim_buffer, id, ptr, size);
	_malloc_lock_unlock(&reclaim_buffer_lock);
	return used;
}

uint64_t
mvm_reclaim_mark_free(vm_address_t ptr, uint32_t size, unsigned int debug_flags)
{
	uint64_t id;
	bool should_update_kernel_accounting = false;
	if (debug_flags & MALLOC_ADD_GUARD_PAGE_FLAGS) {
		if (os_add_overflow(size, 2 * large_vm_page_quanta_size, &size)) {
			return VM_RECLAIM_INDEX_NULL;
		}
		ptr -= large_vm_page_quanta_size;
	}
	_malloc_lock_lock(&reclaim_buffer_lock);
	id = mach_vm_reclaim_mark_free(&reclaim_buffer, ptr, size,
			MACH_VM_RECLAIM_DEALLOCATE, &should_update_kernel_accounting);
	_malloc_lock_unlock(&reclaim_buffer_lock);
	if (should_update_kernel_accounting) {
		mach_vm_reclaim_update_kernel_accounting(&reclaim_buffer);
	}
	return id;
}

bool
mvm_reclaim_is_available(uint64_t id)
{
	return mach_vm_reclaim_is_available(&reclaim_buffer, id);
}
#endif // CONFIG_DEFERRED_RECLAIM
