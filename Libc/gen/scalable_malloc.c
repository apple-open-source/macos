/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* Author: Bertrand Serlet, August 1999 */

#import "scalable_malloc.h"

#define __POSIX_LIB__
#import <unistd.h>
#import <pthread_internals.h>	// for spin lock
#import <libc.h>
#include <mach/vm_statistics.h>

/*********************	DEFINITIONS	************************/

static unsigned vm_page_shift = 0; // guaranteed to be intialized by zone creation

#define DEBUG_MALLOC	0	// set to one to debug malloc itself
#define DEBUG_CLIENT	0	// set to one to help debug a nasty memory smasher

#if DEBUG_MALLOC
#warning DEBUG ENABLED
#define INLINE
#else
#define INLINE	inline
#endif

#define CHECK_REGIONS			(1 << 31)

#define VM_COPY_THRESHOLD		(40 * 1024)
    // When all memory is touched after a copy, vm_copy() is always a lose
    // But if the memory is only read, vm_copy() wins over memmove() at 3 or 4 pages (on a G3/300MHz)
    // This must be larger than LARGE_THRESHOLD

#define KILL_THRESHOLD			(32 * 1024)

#define LARGE_THRESHOLD			(3 * vm_page_size) // at or above this use "large"

#define	SHIFT_QUANTUM			4	// Required for AltiVec
#define	QUANTUM				(1 << SHIFT_QUANTUM) // allocation quantum
#define MIN_BLOCK			1	// minimum size, in QUANTUM multiples

/* The header of a small block in use contains its size (expressed as multiples of QUANTUM, and header included), or 0;
If 0 then the block is either free (in which case the size is directly at the block itself), or the last block (indicated by either being beyond range, or having 0 in the block itself) */

#define PTR_HEADER_SIZE			(sizeof(msize_t))
#define FOLLOWING_PTR(ptr,msize)	(((char *)(ptr)) + ((msize) << SHIFT_QUANTUM))
#define PREVIOUS_MSIZE(ptr)		((msize_t *)(ptr))[-2]

#define THIS_FREE			0x8000	// indicates this block is free
#define PREV_FREE			0x4000	// indicates previous block is free
#define MSIZE_FLAGS_FOR_PTR(ptr)	(((msize_t *)(ptr))[-1])

#define REGION_SIZE			(1 << (16 - 2 + SHIFT_QUANTUM)) // since we only have 16 bits for msize_t, and 1 bit is taken by THIS_FREE, and 1 by PREV_FREE

#define INITIAL_NUM_REGIONS		8 // must always be at least 2 to always have 1 slot empty

#define CHECKSUM_MAGIC			0x357B

#define PROTECT_SMALL			0	// Should be 0: 1 is too slow for normal use

#define MAX_RECORDER_BUFFER	256

#define MAX_GRAIN	128

typedef unsigned short msize_t; // a size in multiples of SHIFT_QUANTUM

typedef struct {
    unsigned	checksum;
    void	*previous;
    void	*next;
} free_list_t;

typedef struct {
    unsigned 	address_and_num_pages;
    // this type represents both an address and a number of pages
    // the low bits are the number of pages
    // the high bits are the address
    // note that the exact number of bits used for depends on the page size
    // also, this cannot represent pointers larger than 1 << (vm_page_shift * 2)
} compact_range_t;

typedef vm_address_t region_t;

typedef compact_range_t large_entry_t;

typedef vm_range_t huge_entry_t;

typedef unsigned short	grain_t;

typedef struct {
    malloc_zone_t	basic_zone;
    pthread_lock_t	lock;
    unsigned		debug_flags;
    void		*log_address;
    
    /* Regions for small objects */
    unsigned		num_regions;
    region_t		*regions;
        // this array is always created with 1 extra slot to be able to add a region without taking memory right away
    unsigned		last_region_hit;
    free_list_t		*free_list[MAX_GRAIN];
    unsigned		num_bytes_free_in_last_region; // these bytes are cleared
    unsigned		num_small_objects;
    unsigned		num_bytes_in_small_objects;
    
    /* large objects: vm_page_shift <= log2(size) < 2 *vm_page_shift */
    unsigned		num_large_objects_in_use;
    unsigned		num_large_entries;
    unsigned		num_bytes_in_large_objects;
    large_entry_t	*large_entries;
        // large_entries are hashed by location
        // large_entries that are 0 should be discarded
    
    /* huge objects: log2(size) >= 2 *vm_page_shift */
    unsigned		num_bytes_in_huge_objects;
    unsigned		num_huge_entries;
    huge_entry_t	*huge_entries;
} szone_t;

static void *szone_malloc(szone_t *szone, size_t size);
static void *szone_valloc(szone_t *szone, size_t size);
static INLINE void *szone_malloc_should_clear(szone_t *szone, size_t size, boolean_t cleared_requested);
static void szone_free(szone_t *szone, void *ptr);
static size_t szone_good_size(szone_t *szone, size_t size);
static boolean_t szone_check_all(szone_t *szone, const char *function);
static void szone_print(szone_t *szone, boolean_t verbose);
static INLINE region_t *region_for_ptr_no_lock(szone_t *szone, const void *ptr);
static vm_range_t large_free_no_lock(szone_t *szone, large_entry_t *entry);

#define LOG(szone,ptr)	(szone->log_address && (szone->num_small_objects > 8) && (((unsigned)szone->log_address == -1) || (szone->log_address == (void *)(ptr))))

/*********************	ACCESSOR MACROS	************************/

#define SZONE_LOCK(szone) 		LOCK(szone->lock)
#define SZONE_UNLOCK(szone)		UNLOCK(szone->lock)

#define CHECK(szone,fun)		if ((szone)->debug_flags & CHECK_REGIONS) szone_check_all(szone, fun)

#define REGION_ADDRESS(region)		(region)
#define REGION_END(region)		(region+REGION_SIZE)

#define LARGE_ENTRY_ADDRESS(entry)	(((entry).address_and_num_pages >> vm_page_shift) << vm_page_shift)
#define LARGE_ENTRY_NUM_PAGES(entry)	((entry).address_and_num_pages & ((1 << vm_page_shift) - 1))
#define LARGE_ENTRY_SIZE(entry)		(LARGE_ENTRY_NUM_PAGES(entry) << vm_page_shift)
#define LARGE_ENTRY_MATCHES(entry,ptr)	(!(((entry).address_and_num_pages - (unsigned)(ptr)) >> vm_page_shift))
#define LARGE_ENTRY_IS_EMPTY(entry)	(!((entry).address_and_num_pages))

/*********************	VERY LOW LEVEL UTILITIES	************************/

static void szone_error(szone_t *szone, const char *msg, const void *ptr) {
    if (szone) SZONE_UNLOCK(szone);
    if (ptr) {
        malloc_printf("*** malloc[%d]: error for object %p: %s\n", getpid(), ptr, msg);
#if DEBUG_MALLOC
        szone_print(szone, 1);
#endif
    } else {
        malloc_printf("*** malloc[%d]: error: %s\n", getpid(), msg);
    }
#if DEBUG_CLIENT
    malloc_printf("*** Sleeping to help debug\n");
    sleep(3600); // to help debug
#endif
}

static void protect(szone_t *szone, vm_address_t address, vm_size_t size, unsigned protection, unsigned debug_flags) {
    kern_return_t	err;
    if (!(debug_flags & SCALABLE_MALLOC_DONT_PROTECT_PRELUDE)) {
        err = vm_protect(mach_task_self(), address - vm_page_size, vm_page_size, 0, protection);
        if (err) malloc_printf("*** malloc[%d]: Can't protect(%x) region for prelude guard page at 0x%x\n", getpid(), protection, address - vm_page_size);
    }
    if (!(debug_flags & SCALABLE_MALLOC_DONT_PROTECT_POSTLUDE)) {
        err = vm_protect(mach_task_self(), (vm_address_t)(address + size), vm_page_size, 0, protection);
        if (err) malloc_printf("*** malloc[%d]: Can't protect(%x) region for postlude guard page at 0x%x\n", getpid(), protection, address + size);
    }
}

static vm_address_t allocate_pages(szone_t *szone, size_t size, unsigned debug_flags, int vm_page_label) {
    kern_return_t	err;
    vm_address_t	addr;
    boolean_t		add_guard_pages = debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES;
    size_t		allocation_size = round_page(size);
    if (!allocation_size) allocation_size = vm_page_size;
    if (add_guard_pages) allocation_size += 2 * vm_page_size;
    err = vm_allocate(mach_task_self(), &addr, allocation_size, vm_page_label | 1);
    if (err) {
	malloc_printf("*** malloc: vm_allocate(size=%d) failed with %d\n", size, err);
        szone_error(szone, "Can't allocate region", NULL);
        return NULL;
    }
    if (add_guard_pages) {
        addr += vm_page_size;
        protect(szone, addr, size, 0, debug_flags);
    }
    return addr;
}

static void deallocate_pages(szone_t *szone, vm_address_t addr, size_t size, unsigned debug_flags) {
    kern_return_t	err;
    boolean_t		add_guard_pages = debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES;
    if (add_guard_pages) {
        addr -= vm_page_size;
        size += 2 * vm_page_size;
    }
    err = vm_deallocate(mach_task_self(), addr, size);
    if (err) {
        szone_error(szone, "Can't deallocate_pages region", (void *)addr);
    }
}

static kern_return_t _szone_default_reader(task_t task, vm_address_t address, vm_size_t size, void **ptr) {
    *ptr = (void *)address;
    return 0;
}

/*********************	FREE LIST UTILITIES	************************/

static INLINE grain_t grain_for_msize(szone_t *szone, msize_t msize) {
    // assumes msize >= MIN_BLOCK
#if DEBUG_MALLOC
    if (msize < MIN_BLOCK) {
        szone_error(szone, "grain_for_msize: msize too small", NULL);
    }
#endif
    return (msize < MAX_GRAIN + MIN_BLOCK) ? msize - MIN_BLOCK : MAX_GRAIN - 1;
}

static INLINE msize_t msize_for_grain(szone_t *szone, grain_t grain) {
    // 0 if multiple sizes
    return (grain < MAX_GRAIN - 1) ? grain + MIN_BLOCK : 0;
}

static INLINE void free_list_checksum(szone_t *szone, free_list_t *ptr) {
    // We always checksum, as testing whether to do it (based on szone->debug_flags) is as fast as doing it
    if (ptr->checksum != (((unsigned)ptr->previous) ^ ((unsigned)ptr->next) ^ CHECKSUM_MAGIC)) {
	szone_error(szone, "Incorrect checksum for freed object - object was probably modified after being freed; break at szone_error", ptr);
    }
}

static INLINE void free_list_set_checksum(szone_t *szone, free_list_t *ptr) {
    // We always set checksum, as testing whether to do it (based on szone->debug_flags) is slower than just doing it
    ptr->checksum = ((unsigned)ptr->previous) ^ ((unsigned)ptr->next) ^ CHECKSUM_MAGIC;
}

static void free_list_add_ptr(szone_t *szone, void *ptr, msize_t msize) {
    // Adds an item to the proper free list
    // Also marks the header of the block properly
    grain_t	grain = grain_for_msize(szone, msize);
    free_list_t	*free_ptr = ptr;
    free_list_t	*free_head = szone->free_list[grain];
    msize_t	*follower = (msize_t *)FOLLOWING_PTR(ptr, msize);
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) malloc_printf("In free_list_add_ptr(), ptr=%p, msize=%d\n", ptr, msize);
    if (((unsigned)ptr) & (QUANTUM - 1)) {
        szone_error(szone, "free_list_add_ptr: Unaligned ptr", ptr);
    }
#endif
    MSIZE_FLAGS_FOR_PTR(ptr) = msize | THIS_FREE;
    if (free_head) {
        free_list_checksum(szone, free_head);
#if DEBUG_MALLOC
        if (free_head->previous) {
            malloc_printf("ptr=%p grain=%d free_head=%p previous=%p\n", ptr, grain, free_head, free_head->previous);
            szone_error(szone, "free_list_add_ptr: Internal invariant broken (free_head->previous)", ptr);
        }
        if (!(MSIZE_FLAGS_FOR_PTR(free_head) & THIS_FREE)) {
            malloc_printf("ptr=%p grain=%d free_head=%p\n", ptr, grain, free_head);
            szone_error(szone, "free_list_add_ptr: Internal invariant broken (free_head is not a free pointer)", ptr);
        }
        if ((grain != MAX_GRAIN-1) && (MSIZE_FLAGS_FOR_PTR(free_head) != (THIS_FREE | msize))) {
            malloc_printf("ptr=%p grain=%d free_head=%p previous_msize=%d\n", ptr, grain, free_head, MSIZE_FLAGS_FOR_PTR(free_head));
            szone_error(szone, "free_list_add_ptr: Internal invariant broken (incorrect msize)", ptr);
        }
#endif
        free_head->previous = free_ptr;
        free_list_set_checksum(szone, free_head);
    }
    free_ptr->previous = NULL;
    free_ptr->next = free_head;
    free_list_set_checksum(szone, free_ptr);
    szone->free_list[grain] = free_ptr;
    // mark the end of this block
    PREVIOUS_MSIZE(follower) = msize;
    MSIZE_FLAGS_FOR_PTR(follower) |= PREV_FREE;
}

static void free_list_remove_ptr(szone_t *szone, void *ptr, msize_t msize) {
    // Removes item in the proper free list
    // msize could be read, but all callers have it so we pass it in
    grain_t	grain = grain_for_msize(szone, msize);
    free_list_t	*free_ptr = ptr;
    free_list_t	*next = free_ptr->next;
    free_list_t	*previous = free_ptr->previous;
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) malloc_printf("In free_list_remove_ptr(), ptr=%p, msize=%d\n", ptr, msize);
#endif
    free_list_checksum(szone, free_ptr);
    if (!previous) {
#if DEBUG_MALLOC
        if (szone->free_list[grain] != ptr) {
            malloc_printf("ptr=%p grain=%d msize=%d szone->free_list[grain]=%p\n", ptr, grain, msize, szone->free_list[grain]);
            szone_error(szone, "free_list_remove_ptr: Internal invariant broken (szone->free_list[grain])", ptr);
            return;
        }
#endif
        szone->free_list[grain] = next;
    } else {
        previous->next = next;
        free_list_set_checksum(szone, previous);
    }
    if (next) {
        next->previous = previous;
        free_list_set_checksum(szone, next);
    }
    MSIZE_FLAGS_FOR_PTR(FOLLOWING_PTR(ptr, msize)) &= ~ PREV_FREE;
}

static boolean_t free_list_check(szone_t *szone, grain_t grain) {
    unsigned	count = 0;
    free_list_t	*ptr = szone->free_list[grain];
    free_list_t	*previous = NULL;
    while (ptr) {
        msize_t	msize_and_free = MSIZE_FLAGS_FOR_PTR(ptr);
        count++;
        if (!(msize_and_free & THIS_FREE)) {
            malloc_printf("*** malloc[%d]: In-use ptr in free list grain=%d count=%d ptr=%p\n", getpid(), grain, count, ptr);
            return 0;
        }
        if (((unsigned)ptr) & (QUANTUM - 1)) {
            malloc_printf("*** malloc[%d]: Unaligned ptr in free list grain=%d  count=%d ptr=%p\n", getpid(), grain, count, ptr);
            return 0;
        }
        if (!region_for_ptr_no_lock(szone, ptr)) {
            malloc_printf("*** malloc[%d]: Ptr not in szone grain=%d  count=%d ptr=%p\n", getpid(), grain, count, ptr);
            return 0;
        }
        free_list_checksum(szone, ptr);
        if (ptr->previous != previous) {
            malloc_printf("*** malloc[%d]: Previous incorrectly set grain=%d  count=%d ptr=%p\n", getpid(), grain, count, ptr);
            return 0;
        }
        if ((grain != MAX_GRAIN-1) && (msize_and_free != (msize_for_grain(szone, grain) | THIS_FREE))) {
            malloc_printf("*** malloc[%d]: Incorrect msize for grain=%d  count=%d ptr=%p msize=%d\n", getpid(), grain, count, ptr, msize_and_free);
            return 0;
        }
        previous = ptr;
        ptr = ptr->next;
    }
    return 1;
}

/*********************	SMALL BLOCKS MANAGEMENT	************************/

static INLINE region_t *region_for_ptr_no_lock(szone_t *szone, const void *ptr) {
    region_t		*first_region = szone->regions;
    region_t		*region = first_region + szone->last_region_hit;
    region_t		this = *region;
    if ((unsigned)ptr - (unsigned)REGION_ADDRESS(this) < (unsigned)REGION_SIZE) {
        return region;
    } else {
        // We iterate in reverse order becase last regions are more likely
        region = first_region + szone->num_regions;
        while (region != first_region) {
            this = *(--region);
            if ((unsigned)ptr - (unsigned)REGION_ADDRESS(this) < (unsigned)REGION_SIZE) {
                szone->last_region_hit = region - first_region;
                return region;
            }
        }
        return NULL;
    }
}

static INLINE void small_free_no_lock(szone_t *szone, region_t *region, void *ptr, msize_t msize_and_free) {
    msize_t	msize = msize_and_free & ~ PREV_FREE;
    size_t	original_size = msize << SHIFT_QUANTUM;
    void	*next_block = ((char *)ptr + original_size);
    msize_t	next_msize_and_free;
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) malloc_printf("In small_free_no_lock(), ptr=%p, msize=%d\n", ptr, msize);
    if (msize < MIN_BLOCK) {
        malloc_printf("In small_free_no_lock(), ptr=%p, msize=%d\n", ptr, msize);
        szone_error(szone, "Trying to free small block that is too small", ptr);
    }
#endif
    if (((vm_address_t)next_block < REGION_END(*region)) && ((next_msize_and_free = MSIZE_FLAGS_FOR_PTR(next_block)) & THIS_FREE)) {
        // If the next block is free, we coalesce
        msize_t	next_msize = next_msize_and_free & ~THIS_FREE;
        if (LOG(szone,ptr)) malloc_printf("In small_free_no_lock(), for ptr=%p, msize=%d coalesced next block=%p next_msize=%d\n", ptr, msize, next_block, next_msize);
        free_list_remove_ptr(szone, next_block, next_msize);
        msize += next_msize;
    }
    // Let's try to coalesce backwards now
    if (msize_and_free & PREV_FREE) {
        msize_t	previous_msize = PREVIOUS_MSIZE(ptr);
        void	*previous = ptr - (previous_msize << SHIFT_QUANTUM);
#if DEBUG_MALLOC
        if (LOG(szone,previous)) malloc_printf("In small_free_no_lock(), coalesced backwards for %p previous=%p, msize=%d\n", ptr, previous, previous_msize);
        if (!previous_msize || (previous_msize >= (((vm_address_t)ptr - REGION_ADDRESS(*region)) >> SHIFT_QUANTUM))) {
            szone_error(szone, "Invariant 1 broken when coalescing backwards", ptr);
        }
        if (MSIZE_FLAGS_FOR_PTR(previous) != (previous_msize | THIS_FREE)) {
            malloc_printf("previous=%p its_msize_and_free=0x%x previous_msize=%d\n", previous, MSIZE_FLAGS_FOR_PTR(previous), previous_msize);
            szone_error(szone, "Invariant 3 broken when coalescing backwards", ptr);
        }
#endif
        free_list_remove_ptr(szone, previous, previous_msize);
        ptr = previous;
        msize += previous_msize;
#if DEBUG_MALLOC
	if (msize & PREV_FREE) {
	    malloc_printf("In small_free_no_lock(), after coalescing with previous ptr=%p, msize=%d previous_msize=%d\n", ptr, msize, previous_msize);
	    szone_error(szone, "Incorrect coalescing", ptr);
	}
#endif 
    }
    if (szone->debug_flags & SCALABLE_MALLOC_DO_SCRIBBLE) {
	if (!msize) {
	    szone_error(szone, "Incorrect size information - block header was damaged", ptr);
	} else {
	    memset(ptr, 0x55, (msize << SHIFT_QUANTUM) - PTR_HEADER_SIZE);
	}
    }
    free_list_add_ptr(szone, ptr, msize);
    CHECK(szone, "small_free_no_lock: added to free list");
    szone->num_small_objects--;
    szone->num_bytes_in_small_objects -= original_size; // we use original_size and not msize to avoid double counting the coalesced blocks
}

static void *small_malloc_from_region_no_lock(szone_t *szone, msize_t msize) {
    // Allocates from the last region or a freshly allocated region
    region_t		*last_region = szone->regions + szone->num_regions - 1;
    vm_address_t	new_address;
    void		*ptr;
    msize_t		msize_and_free;
    unsigned		region_capacity;
    ptr = (void *)(REGION_END(*last_region) - szone->num_bytes_free_in_last_region + PTR_HEADER_SIZE);
#if DEBUG_MALLOC
    if (((vm_address_t)ptr) & (QUANTUM - 1)) {
        szone_error(szone, "Invariant broken while using end of region", ptr);
    }
#endif
    msize_and_free = MSIZE_FLAGS_FOR_PTR(ptr);
#if DEBUG_MALLOC
    if (msize_and_free != PREV_FREE && msize_and_free != 0) {
        malloc_printf("*** malloc[%d]: msize_and_free = %d\n", getpid(), msize_and_free);
        szone_error(szone, "Invariant broken when allocating at end of zone", ptr);
    }
#endif
    // In order to make sure we don't have 2 free pointers following themselves, if the last item is a free item, we combine it and clear it
    if (msize_and_free == PREV_FREE) {
        msize_t		previous_msize = PREVIOUS_MSIZE(ptr);
        void		*previous = ptr - (previous_msize << SHIFT_QUANTUM);
#if DEBUG_MALLOC
        if (LOG(szone, ptr)) malloc_printf("Combining last with free space at %p\n", ptr);
        if (!previous_msize || (previous_msize >= (((vm_address_t)ptr - REGION_ADDRESS(*last_region)) >> SHIFT_QUANTUM)) || (MSIZE_FLAGS_FOR_PTR(previous) != (previous_msize | THIS_FREE))) {
            szone_error(szone, "Invariant broken when coalescing backwards at end of zone", ptr);
        }
#endif
        free_list_remove_ptr(szone, previous, previous_msize);
        szone->num_bytes_free_in_last_region += previous_msize << SHIFT_QUANTUM;
        memset(previous, 0, previous_msize << SHIFT_QUANTUM);
        MSIZE_FLAGS_FOR_PTR(previous) = 0;
        ptr = previous;
    }
    // first try at the end of the last region
    CHECK(szone, __PRETTY_FUNCTION__);
    if (szone->num_bytes_free_in_last_region >= (msize << SHIFT_QUANTUM)) {
        szone->num_bytes_free_in_last_region -= (msize << SHIFT_QUANTUM);
        szone->num_small_objects++;
        szone->num_bytes_in_small_objects += msize << SHIFT_QUANTUM;
        MSIZE_FLAGS_FOR_PTR(ptr) = msize;
        return ptr;
    }
    // time to create a new region
    new_address = allocate_pages(szone, REGION_SIZE, 0, VM_MAKE_TAG(VM_MEMORY_MALLOC_SMALL));
    if (!new_address) {
        // out of memory!
        return NULL;
    }
    // let's prepare to free the remnants of last_region
    if (szone->num_bytes_free_in_last_region >= QUANTUM) {
        msize_t	this_msize = szone->num_bytes_free_in_last_region >> SHIFT_QUANTUM;
        // malloc_printf("Entering last block %p size=%d\n", ptr, this_msize << SHIFT_QUANTUM);
        if (this_msize >= MIN_BLOCK) {
            free_list_add_ptr(szone, ptr, this_msize);
        } else {
            // malloc_printf("Leaking last block at %p\n", ptr);
        }
        szone->num_bytes_free_in_last_region -= this_msize << SHIFT_QUANTUM; // to avoid coming back here
    }
    last_region[1] = new_address;
    szone->num_regions++;
    szone->num_bytes_free_in_last_region = REGION_SIZE - QUANTUM + PTR_HEADER_SIZE - (msize << SHIFT_QUANTUM);
    ptr = (void *)(new_address + QUANTUM); // waste the first bytes
    region_capacity = (MSIZE_FLAGS_FOR_PTR(szone->regions) * QUANTUM - PTR_HEADER_SIZE) / sizeof(region_t);
    if (szone->num_regions >= region_capacity) {
        unsigned	new_capacity = region_capacity * 2 + 1;
        msize_t		new_msize = (new_capacity * sizeof(region_t) + PTR_HEADER_SIZE + QUANTUM - 1) / QUANTUM;
        region_t	*new_regions = ptr;
        // malloc_printf("Now %d regions growing regions %p to %d\n", szone->num_regions, szone->regions, new_capacity);
        MSIZE_FLAGS_FOR_PTR(new_regions) = new_msize;
        szone->num_small_objects++;
        szone->num_bytes_in_small_objects += new_msize << SHIFT_QUANTUM;
        memcpy(new_regions, szone->regions, szone->num_regions * sizeof(region_t));
        // We intentionally leak the previous regions pointer to avoid multi-threading crashes if another thread was reading it (unlocked) while we are changing it
        // Given that in practise the number of regions is typically a handful, this should not be a big deal
        szone->regions = new_regions;
        ptr += (new_msize << SHIFT_QUANTUM);
        szone->num_bytes_free_in_last_region -= (new_msize << SHIFT_QUANTUM);
        // malloc_printf("Regions is now %p next ptr is %p\n", szone->regions, ptr);
    }
    szone->num_small_objects++;
    szone->num_bytes_in_small_objects += msize << SHIFT_QUANTUM;
    MSIZE_FLAGS_FOR_PTR(ptr) = msize;
    return ptr;
}

static boolean_t szone_check_region(szone_t *szone, region_t *region) {
    void		*ptr = (void *)REGION_ADDRESS(*region) + QUANTUM;
    vm_address_t	region_end = REGION_END(*region);
    int			is_last_region = region == szone->regions + szone->num_regions - 1;
    msize_t		prev_free = 0;
    while ((vm_address_t)ptr < region_end) {
        msize_t		msize_and_free = MSIZE_FLAGS_FOR_PTR(ptr);
        if (!(msize_and_free & THIS_FREE)) {
            msize_t	msize = msize_and_free & ~PREV_FREE;
            if ((msize_and_free & PREV_FREE) != prev_free) {
                malloc_printf("*** malloc[%d]: invariant broken for %p (prev_free=%d) this msize=%d\n", getpid(), ptr, prev_free, msize_and_free);
                return 0;
            }
            if (!msize) {
                int	extra = (is_last_region) ? szone->num_bytes_free_in_last_region : QUANTUM;
                if (((unsigned)(ptr + extra)) < region_end) {
                    malloc_printf("*** malloc[%d]: invariant broken at region end: ptr=%p extra=%d index=%d num_regions=%d end=%p\n", getpid(), ptr, extra, region - szone->regions, szone->num_regions, (void *)region_end);
                    return 0;
                }
                break; // last encountered
            }
	    if (msize > (LARGE_THRESHOLD / QUANTUM)) {
                malloc_printf("*** malloc[%d]: invariant broken for %p this msize=%d - size is too large\n", getpid(), ptr, msize_and_free);
                return 0;
            }
            if ((msize < MIN_BLOCK) && ((unsigned)ptr != region_end - QUANTUM)) {
                malloc_printf("*** malloc[%d]: invariant broken for %p this msize=%d - size is too small\n", getpid(), ptr, msize_and_free);
                return 0;
            }
            ptr += msize << SHIFT_QUANTUM;
            prev_free = 0;
	    if (is_last_region && ((vm_address_t)ptr - PTR_HEADER_SIZE > region_end - szone->num_bytes_free_in_last_region)) {
                malloc_printf("*** malloc[%d]: invariant broken for %p this msize=%d - block extends beyond allocated region\n", getpid(), ptr, msize_and_free);
	    }
        } else {
            // free pointer
            msize_t	msize = msize_and_free & ~THIS_FREE;
            free_list_t	*free_head = ptr;
            msize_t	*follower = (void *)FOLLOWING_PTR(ptr, msize);
	    if ((msize_and_free & PREV_FREE) && !prev_free) {
                malloc_printf("*** malloc[%d]: invariant broken for free block %p this msize=%d: PREV_FREE set while previous block is in use\n", getpid(), ptr, msize);
                return 0;
	    }
            if (msize < MIN_BLOCK) {
                malloc_printf("*** malloc[%d]: invariant broken for free block %p this msize=%d\n", getpid(), ptr, msize);
                return 0;
            }
            if (prev_free) {
                malloc_printf("*** malloc[%d]: invariant broken for %p (2 free in a row)\n", getpid(), ptr);
                return 0;
            }
            free_list_checksum(szone, free_head);
            if (free_head->previous && !(MSIZE_FLAGS_FOR_PTR(free_head->previous) & THIS_FREE)) {
                malloc_printf("*** malloc[%d]: invariant broken for %p (previous %p is not a free pointer)\n", getpid(), ptr, free_head->previous);
                return 0;
            }
            if (free_head->next && !(MSIZE_FLAGS_FOR_PTR(free_head->next) & THIS_FREE)) {
                malloc_printf("*** malloc[%d]: invariant broken for %p (next is not a free pointer)\n", getpid(), ptr);
                return 0;
            }
            if (PREVIOUS_MSIZE(follower) != msize) {
                malloc_printf("*** malloc[%d]: invariant broken for free %p followed by %p in region [%x-%x] (end marker incorrect) should be %d; in fact %d\n", getpid(), ptr, follower, REGION_ADDRESS(*region), region_end, msize, PREVIOUS_MSIZE(follower));
                return 0;
            }
            ptr = follower;
            prev_free = PREV_FREE;
        }
    }
    return 1;
}

static kern_return_t small_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t region_address, unsigned num_regions, memory_reader_t reader, vm_range_recorder_t recorder) {
    region_t		*regions;
    unsigned		index = 0;
    vm_range_t		buffer[MAX_RECORDER_BUFFER];
    unsigned		count = 0;
    kern_return_t	err;
    err = reader(task, region_address, sizeof(region_t) * num_regions, (void **)&regions);
    if (err) return err;
    while (index < num_regions) {
        region_t	region = regions[index++];
        vm_range_t	range = {REGION_ADDRESS(region), REGION_SIZE};
        vm_address_t	start = range.address + QUANTUM;
        // malloc_printf("Enumerating small ptrs for Region starting at 0x%x\n", start);
        if (type_mask & MALLOC_PTR_REGION_RANGE_TYPE) recorder(task, context, MALLOC_PTR_REGION_RANGE_TYPE, &range, 1);
        if (type_mask & MALLOC_PTR_IN_USE_RANGE_TYPE) while (start < range.address + range.size) {
            void	*previous;
            msize_t	msize_and_free;
            err = reader(task, start - PTR_HEADER_SIZE, QUANTUM, (void **)&previous);
            if (err) return err;
            previous += PTR_HEADER_SIZE;
            msize_and_free = MSIZE_FLAGS_FOR_PTR(previous);
            if (!(msize_and_free & THIS_FREE)) {
                // Block in use
                msize_t		msize = msize_and_free & ~PREV_FREE;
                if (!msize) break; // last encountered
                buffer[count].address = start;
                buffer[count].size = (msize << SHIFT_QUANTUM) - PTR_HEADER_SIZE;
                count++;
                if (count >= MAX_RECORDER_BUFFER) {
                    recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE, buffer, count);
                    count = 0;
                }
                start += msize << SHIFT_QUANTUM;
            } else {
                // free pointer
                msize_t	msize = msize_and_free & ~THIS_FREE;
                start += msize << SHIFT_QUANTUM;
            }
        }
        // malloc_printf("End region - count=%d\n", count);
    }
    if (count) recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE, buffer, count);
    return 0;
}

static INLINE void *small_malloc_from_free_list(szone_t *szone, msize_t msize, boolean_t *locked) {
    void	*ptr;
    msize_t	this_msize;
    free_list_t	**free_list;
    free_list_t	**limit = szone->free_list + MAX_GRAIN - 1;
    // first try the small grains
    free_list = szone->free_list + grain_for_msize(szone, msize);
    while (free_list < limit) {
        // try bigger grains
        ptr = *free_list;
        if (ptr) {
            if (!*locked) { *locked = 1; SZONE_LOCK(szone); CHECK(szone, __PRETTY_FUNCTION__); }
            ptr = *free_list;
            if (ptr) {
                // optimistic test worked
                free_list_t	*next;
                next = ((free_list_t *)ptr)->next;
                if (next) {
                    next->previous = NULL;
                    free_list_set_checksum(szone, next);
                }
                *free_list = next;
                this_msize = MSIZE_FLAGS_FOR_PTR(ptr) & ~THIS_FREE;
                MSIZE_FLAGS_FOR_PTR(FOLLOWING_PTR(ptr, this_msize)) &= ~ PREV_FREE;
                goto add_leftover_and_proceed;
            }
        }
        free_list++;
    }
    // We now check the large grains for one that is big enough
    if (!*locked) { *locked = 1; SZONE_LOCK(szone); CHECK(szone, __PRETTY_FUNCTION__); }
    ptr = *free_list;
    while (ptr) {
        this_msize = MSIZE_FLAGS_FOR_PTR(ptr) & ~THIS_FREE;
        if (this_msize >= msize) {
            free_list_remove_ptr(szone, ptr, this_msize);
            goto add_leftover_and_proceed;
        }
        ptr = ((free_list_t *)ptr)->next;
    }
    return NULL;
add_leftover_and_proceed:
    if (this_msize >= msize + MIN_BLOCK) {
        if (LOG(szone,ptr)) malloc_printf("In small_malloc_should_clear(), adding leftover ptr=%p, this_msize=%d\n", ptr, this_msize);
        free_list_add_ptr(szone, ptr + (msize << SHIFT_QUANTUM), this_msize - msize);
        this_msize = msize;
    }
    szone->num_small_objects++;
    szone->num_bytes_in_small_objects += this_msize << SHIFT_QUANTUM;
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) malloc_printf("In small_malloc_should_clear(), ptr=%p, this_msize=%d, msize=%d\n", ptr, this_msize, msize);
#endif
    MSIZE_FLAGS_FOR_PTR(ptr) = this_msize;
    return ptr;
}

static INLINE void *small_malloc_should_clear(szone_t *szone, msize_t msize, boolean_t cleared_requested) {
    boolean_t	locked = 0;
    void	*ptr;
#if DEBUG_MALLOC
    if (! (msize & 0xffff)) {
        szone_error(szone, "Invariant broken (!msize) in allocation (region)", NULL);
    }
    if (msize < MIN_BLOCK) {
        szone_error(szone, "Invariant broken (msize too small) in allocation (region)", NULL);
    }
#endif
    ptr = small_malloc_from_free_list(szone, msize, &locked);
    if (ptr) {
        CHECK(szone, __PRETTY_FUNCTION__);
        SZONE_UNLOCK(szone);
        if (cleared_requested) memset(ptr, 0, (msize << SHIFT_QUANTUM) - PTR_HEADER_SIZE);
        return ptr;
    } else {
        if (!locked) SZONE_LOCK(szone);
        CHECK(szone, __PRETTY_FUNCTION__);
        ptr = small_malloc_from_region_no_lock(szone, msize);
        // we don't clear because this freshly allocated space is pristine
        CHECK(szone, __PRETTY_FUNCTION__);
        SZONE_UNLOCK(szone);
    }
    return ptr;
}

static INLINE void *small_malloc_cleared_no_lock(szone_t *szone, msize_t msize) {
    // tries to allocate a small, cleared block
    boolean_t	locked = 1;
    void	*ptr;
    ptr = small_malloc_from_free_list(szone, msize, &locked);
    if (ptr) {
        memset(ptr, 0, (msize << SHIFT_QUANTUM) - PTR_HEADER_SIZE);
        return ptr;
    } else {
        ptr = small_malloc_from_region_no_lock(szone, msize);
        // we don't clear because this freshly allocated space is pristine
    }
    return ptr;
}

/*********************	LARGE ENTRY UTILITIES	************************/

#if DEBUG_MALLOC

static void large_debug_print(szone_t *szone) {
    unsigned	num_large_entries = szone->num_large_entries;
    unsigned	index = num_large_entries;
    while (index--) {
        large_entry_t	*range = szone->large_entries + index;
        large_entry_t	entry = *range;
        if (!LARGE_ENTRY_IS_EMPTY(entry)) malloc_printf("%d: 0x%x(%dKB);  ", index, LARGE_ENTRY_ADDRESS(entry), LARGE_ENTRY_SIZE(entry)/1024);
    }
    malloc_printf("\n");
}
#endif

static large_entry_t *large_entry_for_pointer_no_lock(szone_t *szone, const void *ptr) {
    // result only valid during a lock
    unsigned	num_large_entries = szone->num_large_entries;
    unsigned	hash_index;
    unsigned	index;
    if (!num_large_entries) return NULL;
    hash_index = ((unsigned)ptr >> vm_page_shift) % num_large_entries;
    index = hash_index;
    do {
        large_entry_t	*range = szone->large_entries + index;
        large_entry_t	entry = *range;
        if (LARGE_ENTRY_MATCHES(entry, ptr)) return range;
        if (LARGE_ENTRY_IS_EMPTY(entry)) return NULL; // end of chain
        index++; if (index == num_large_entries) index = 0;
    } while (index != hash_index);
    return NULL;
}

static void large_entry_insert_no_lock(szone_t *szone, large_entry_t range) {
    unsigned	num_large_entries = szone->num_large_entries;
    unsigned	hash_index = (range.address_and_num_pages >> vm_page_shift) % num_large_entries;
    unsigned	index = hash_index;
    // malloc_printf("Before insertion of 0x%x\n", LARGE_ENTRY_ADDRESS(range));
    do {
        large_entry_t	*entry = szone->large_entries + index;
        if (LARGE_ENTRY_IS_EMPTY(*entry)) {
            *entry = range;
            return; // end of chain
        }
        index++; if (index == num_large_entries) index = 0;
    } while (index != hash_index);
}

static INLINE void large_entries_rehash_after_entry_no_lock(szone_t *szone, large_entry_t *entry) {
    unsigned	num_large_entries = szone->num_large_entries;
    unsigned	hash_index = entry - szone->large_entries;
    unsigned	index = hash_index;
    do {
        large_entry_t	range;
        index++; if (index == num_large_entries) index = 0;
        range = szone->large_entries[index];
        if (LARGE_ENTRY_IS_EMPTY(range)) return;
        szone->large_entries[index].address_and_num_pages = 0;
        large_entry_insert_no_lock(szone, range); // this will reinsert in the proper place
    } while (index != hash_index);
}

static INLINE large_entry_t *large_entries_alloc_no_lock(szone_t *szone, unsigned num) {
    size_t	size = num * sizeof(large_entry_t);
    boolean_t	is_vm_allocation = size >= LARGE_THRESHOLD;
    if (is_vm_allocation) {
        return (void *)allocate_pages(szone, round_page(size), 0, VM_MAKE_TAG(VM_MEMORY_MALLOC_LARGE));
    } else {
        return small_malloc_cleared_no_lock(szone, (size + PTR_HEADER_SIZE + QUANTUM - 1) >> SHIFT_QUANTUM);
    }
}

static void large_entries_free_no_lock(szone_t *szone, large_entry_t *entries, unsigned num) {
    size_t	size = num * sizeof(large_entry_t);
    boolean_t	is_vm_allocation = size >= LARGE_THRESHOLD;
    if (is_vm_allocation) {
        deallocate_pages(szone, (vm_address_t)entries, round_page(size), 0);
    } else {
        region_t	*region = region_for_ptr_no_lock(szone, entries);
        msize_t		msize_and_free = MSIZE_FLAGS_FOR_PTR(entries);
        if (msize_and_free & THIS_FREE) {
            szone_error(szone, "Object already freed being freed", entries);
            return;
        }
        small_free_no_lock(szone, region, entries, msize_and_free);
    }
}

static void large_entries_grow_no_lock(szone_t *szone) {
    unsigned		old_num_entries = szone->num_large_entries;
    large_entry_t	*old_entries = szone->large_entries;
    unsigned		new_num_entries = (old_num_entries) ? old_num_entries * 2 + 1 : 15; // always an odd number for good hashing
    large_entry_t	*new_entries = large_entries_alloc_no_lock(szone, new_num_entries);
    unsigned		index = old_num_entries;
    szone->num_large_entries = new_num_entries;
    szone->large_entries = new_entries;
    // malloc_printf("_grow_large_entries old_num_entries=%d new_num_entries=%d\n", old_num_entries, new_num_entries);
    while (index--) {
        large_entry_t	oldRange = old_entries[index];
        if (!LARGE_ENTRY_IS_EMPTY(oldRange)) large_entry_insert_no_lock(szone, oldRange);
    }
    if (old_entries) large_entries_free_no_lock(szone, old_entries, old_num_entries);
}

static vm_range_t large_free_no_lock(szone_t *szone, large_entry_t *entry) {
    // frees the specific entry in the size table
    // returns a range to truly deallocate
    vm_range_t		range;
    range.address = LARGE_ENTRY_ADDRESS(*entry);
    range.size = LARGE_ENTRY_SIZE(*entry);
    szone->num_large_objects_in_use --;
    szone->num_bytes_in_large_objects -= range.size;
    if (szone->debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES) {
        protect(szone, range.address, range.size, VM_PROT_READ | VM_PROT_WRITE, szone->debug_flags);
        range.address -= vm_page_size;
        range.size += 2 * vm_page_size;
    }
    // printf("Entry is 0x%x=%d; cache is 0x%x ; found=0x%x\n", entry, entry-szone->large_entries, szone->large_entries, large_entry_for_pointer_no_lock(szone, (void *)range.address));
    entry->address_and_num_pages = 0;
    large_entries_rehash_after_entry_no_lock(szone, entry);
#if DEBUG_MALLOC
    if (large_entry_for_pointer_no_lock(szone, (void *)range.address)) {
        malloc_printf("*** malloc[%d]: Freed entry 0x%x still in use; num_large_entries=%d\n", getpid(), range.address, szone->num_large_entries);
        large_debug_print(szone);
        sleep(3600);
    }
#endif
    return range;
}

static kern_return_t large_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t large_entries_address, unsigned num_entries, memory_reader_t reader, vm_range_recorder_t recorder) {
    unsigned		index = 0;
    vm_range_t		buffer[MAX_RECORDER_BUFFER];
    unsigned		count = 0;
    large_entry_t	*entries;
    kern_return_t	err;
    err = reader(task, large_entries_address, sizeof(large_entry_t) * num_entries, (void **)&entries);
    if (err) return err;
    index = num_entries;
    if ((type_mask & MALLOC_ADMIN_REGION_RANGE_TYPE) && (num_entries * sizeof(large_entry_t) >= LARGE_THRESHOLD)) {
        vm_range_t	range;
        range.address = large_entries_address;
        range.size = round_page(num_entries * sizeof(large_entry_t));
        recorder(task, context, MALLOC_ADMIN_REGION_RANGE_TYPE, &range, 1);
    }
    if (type_mask & (MALLOC_PTR_IN_USE_RANGE_TYPE | MALLOC_PTR_REGION_RANGE_TYPE)) while (index--) {
        large_entry_t	entry = entries[index];
        if (!LARGE_ENTRY_IS_EMPTY(entry)) {
            vm_range_t	range;
            range.address = LARGE_ENTRY_ADDRESS(entry);
            range.size = LARGE_ENTRY_SIZE(entry);
            buffer[count++] = range;
            if (count >= MAX_RECORDER_BUFFER) {
                recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE | MALLOC_PTR_REGION_RANGE_TYPE, buffer, count);
                count = 0;
            }
        }
    }
    if (count) recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE | MALLOC_PTR_REGION_RANGE_TYPE, buffer, count);
    return 0;
}

/*********************	HUGE ENTRY UTILITIES	************************/

static huge_entry_t *huge_entry_for_pointer_no_lock(szone_t *szone, const void *ptr) {
    unsigned	index = szone->num_huge_entries;
    while (index--) {
        huge_entry_t	*huge = szone->huge_entries + index;
        if (huge->address == (vm_address_t)ptr) return huge;
    }
    return NULL;
}

static boolean_t huge_entry_append(szone_t *szone, huge_entry_t huge) {
    // We do a little dance with locking because doing allocation (even in the default szone) may cause something to get freed in this szone, with a deadlock
    huge_entry_t	*new_huge_entries = NULL;
    SZONE_LOCK(szone);
    while (1) {
        unsigned	num_huge_entries;
        num_huge_entries = szone->num_huge_entries;
        SZONE_UNLOCK(szone);
        // malloc_printf("In huge_entry_append currentEntries=%d\n", num_huge_entries);
        if (new_huge_entries) szone_free(szone, new_huge_entries);
        new_huge_entries = szone_malloc(szone, (num_huge_entries + 1) * sizeof(huge_entry_t));
	if (new_huge_entries == NULL)
	    return 1;
        SZONE_LOCK(szone);
        if (num_huge_entries == szone->num_huge_entries) {
            // No change - our malloc still applies
            huge_entry_t	*old_huge_entries = szone->huge_entries;
            if (num_huge_entries) memcpy(new_huge_entries, old_huge_entries, num_huge_entries * sizeof(huge_entry_t));
            new_huge_entries[szone->num_huge_entries++] = huge;
            szone->huge_entries = new_huge_entries;
            SZONE_UNLOCK(szone);
            szone_free(szone, old_huge_entries);
            // malloc_printf("Done huge_entry_append now=%d\n", szone->num_huge_entries);
            return 0;
        }
        // try again!
    }
}

static kern_return_t huge_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t huge_entries_address, unsigned num_entries, memory_reader_t reader, vm_range_recorder_t recorder) {
    huge_entry_t	*entries;
    kern_return_t	err;
    err = reader(task, huge_entries_address, sizeof(huge_entry_t) * num_entries, (void **)&entries);
    if (err) return err;
    if (num_entries) recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE | MALLOC_PTR_REGION_RANGE_TYPE, entries, num_entries);
    return 0;
}

static void *large_and_huge_malloc(szone_t *szone, unsigned num_pages) {
    vm_address_t	addr = 0;
    if (!num_pages) num_pages = 1; // minimal allocation size for this szone
    // malloc_printf("In large_and_huge_malloc for %dKB\n", num_pages * vm_page_size / 1024);
    if (num_pages >= (1 << vm_page_shift)) {
        huge_entry_t	huge;
        huge.size = num_pages << vm_page_shift;
        addr = allocate_pages(szone, huge.size, szone->debug_flags, VM_MAKE_TAG(VM_MEMORY_MALLOC_HUGE));
        if (!addr) return NULL;
        huge.address = addr;
        if (huge_entry_append(szone, huge))
	    return NULL;
        SZONE_LOCK(szone);
        szone->num_bytes_in_huge_objects += huge.size;
    } else {
        vm_size_t		size = num_pages << vm_page_shift;
        large_entry_t		entry;
	addr = allocate_pages(szone, size, szone->debug_flags, VM_MAKE_TAG(VM_MEMORY_MALLOC_LARGE));
	if (LOG(szone, addr)) malloc_printf("In szone_malloc true large allocation at %p for %dKB\n", (void *)addr, size / 1024);
	SZONE_LOCK(szone);
	if (!addr) {
	    SZONE_UNLOCK(szone);
	    return NULL;
	}
#if DEBUG_MALLOC
	if (large_entry_for_pointer_no_lock(szone, (void *)addr)) {
	    malloc_printf("Freshly allocated is already in use: 0x%x\n", addr);
	    large_debug_print(szone);
	    sleep(3600);
	}
#endif
        if ((szone->num_large_objects_in_use + 1) * 4 > szone->num_large_entries) {
            // density of hash table too high; grow table
            // we do that under lock to avoid a race
            // malloc_printf("In szone_malloc growing hash table current=%d\n", szone->num_large_entries);
            large_entries_grow_no_lock(szone);
        }
        // malloc_printf("Inserting large entry (0x%x, %dKB)\n", addr, num_pages * vm_page_size / 1024);
        entry.address_and_num_pages = addr | num_pages;
#if DEBUG_MALLOC
        if (large_entry_for_pointer_no_lock(szone, (void *)addr)) {
            malloc_printf("Entry about to be added already in use: 0x%x\n", addr);
            large_debug_print(szone);
            sleep(3600);
        }
#endif
        large_entry_insert_no_lock(szone, entry);
#if DEBUG_MALLOC
        if (!large_entry_for_pointer_no_lock(szone, (void *)addr)) {
            malloc_printf("Can't find entry just added\n");
            large_debug_print(szone);
            sleep(3600);
        }
#endif
        // malloc_printf("Inserted large entry (0x%x, %d pages)\n", addr, num_pages);
        szone->num_large_objects_in_use ++;
        szone->num_bytes_in_large_objects += size;
    }
    SZONE_UNLOCK(szone);
    return (void *)addr;
}

/*********************	Zone call backs	************************/

static void szone_free(szone_t *szone, void *ptr) {
    region_t		*region;
    large_entry_t	*entry;
    vm_range_t		vm_range_to_deallocate;
    huge_entry_t	*huge;
    if (LOG(szone, ptr)) malloc_printf("In szone_free with %p\n", ptr);
    if (!ptr) return;
    if ((vm_address_t)ptr & (QUANTUM - 1)) {
        szone_error(szone, "Non-aligned pointer being freed", ptr);
        return;
    }
    // try a small pointer
    region = region_for_ptr_no_lock(szone, ptr);
    if (region) {
        // this is indeed a valid pointer
        msize_t		msize_and_free;
        SZONE_LOCK(szone);
        msize_and_free = MSIZE_FLAGS_FOR_PTR(ptr);
        if (msize_and_free & THIS_FREE) {
            szone_error(szone, "Object already freed being freed", ptr);
            return;
        }
        CHECK(szone, __PRETTY_FUNCTION__);
        small_free_no_lock(szone, region, ptr, msize_and_free);
        CHECK(szone, __PRETTY_FUNCTION__);
        SZONE_UNLOCK(szone);
        return;
    }
    if (((unsigned)ptr) & (vm_page_size - 1)) {
        szone_error(szone, "Non-page-aligned, non-allocated pointer being freed", ptr);
        return;
    }
    SZONE_LOCK(szone);
    entry = large_entry_for_pointer_no_lock(szone, ptr);
    if (entry) {
        // malloc_printf("Ready for deallocation [0x%x-%dKB]\n", LARGE_ENTRY_ADDRESS(*entry), LARGE_ENTRY_SIZE(*entry)/1024);
	if (KILL_THRESHOLD && (LARGE_ENTRY_SIZE(*entry) > KILL_THRESHOLD)) {
	    // We indicate to the VM system that these pages contain garbage and therefore don't need to be swapped out
	    vm_msync(mach_task_self(), LARGE_ENTRY_ADDRESS(*entry), LARGE_ENTRY_SIZE(*entry), VM_SYNC_KILLPAGES);
	}
        vm_range_to_deallocate = large_free_no_lock(szone, entry);
#if DEBUG_MALLOC
        if (large_entry_for_pointer_no_lock(szone, ptr)) {
            malloc_printf("*** malloc[%d]: Just after freeing 0x%x still in use num_large_entries=%d\n", getpid(), ptr, szone->num_large_entries);
            large_debug_print(szone);
            sleep(3600);
        }
#endif
    } else if ((huge = huge_entry_for_pointer_no_lock(szone, ptr))) {
        vm_range_to_deallocate = *huge;
        *huge = szone->huge_entries[--szone->num_huge_entries]; // last entry fills that spot
        szone->num_bytes_in_huge_objects -= vm_range_to_deallocate.size;
    } else {
#if DEBUG_MALLOC
        large_debug_print(szone);
#endif
        szone_error(szone, "Pointer being freed was not allocated", ptr);
        return;
    }
    CHECK(szone, __PRETTY_FUNCTION__);
    SZONE_UNLOCK(szone); // we release the lock asap
    // we deallocate_pages, including guard pages
    if (vm_range_to_deallocate.address) {
        // malloc_printf("About to deallocate 0x%x size %dKB\n", vm_range_to_deallocate.address, vm_range_to_deallocate.size / 1024);
#if DEBUG_MALLOC
        if (large_entry_for_pointer_no_lock(szone, (void *)vm_range_to_deallocate.address)) {
            malloc_printf("*** malloc[%d]: Invariant broken: 0x%x still in use num_large_entries=%d\n", getpid(), vm_range_to_deallocate.address, szone->num_large_entries);
            large_debug_print(szone);
            sleep(3600);
        }
#endif
        deallocate_pages(szone, vm_range_to_deallocate.address, vm_range_to_deallocate.size, 0);
    }
}

static INLINE void *szone_malloc_should_clear(szone_t *szone, size_t size, boolean_t cleared_requested) {
    void	*ptr;
    if (!((szone->debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES) && PROTECT_SMALL) && (size < LARGE_THRESHOLD)) {
        // think small
        size_t		msize = (size + PTR_HEADER_SIZE + QUANTUM - 1) >> SHIFT_QUANTUM;
        if (msize < MIN_BLOCK) msize = MIN_BLOCK;
        ptr = small_malloc_should_clear(szone, msize, cleared_requested);
#if DEBUG_MALLOC
        if ((MSIZE_FLAGS_FOR_PTR(ptr) & ~ PREV_FREE) < msize) {
            malloc_printf("ptr=%p this=%d msize=%d\n", ptr, MSIZE_FLAGS_FOR_PTR(ptr), (int)msize);
            szone_error(szone, "Pointer allocated has improper size (1)", ptr);
            return NULL;
        }
        if ((MSIZE_FLAGS_FOR_PTR(ptr) & ~ PREV_FREE) < MIN_BLOCK) {
            malloc_printf("ptr=%p this=%d msize=%d\n", ptr, MSIZE_FLAGS_FOR_PTR(ptr), (int)msize);
            szone_error(szone, "Pointer allocated has improper size (2)", ptr);
            return NULL;
        }
#endif
    } else {
        unsigned		num_pages;
        num_pages = round_page(size) >> vm_page_shift;
        ptr = large_and_huge_malloc(szone, num_pages);
    }
    if (LOG(szone, ptr)) malloc_printf("szone_malloc returned %p\n", ptr);
    return ptr;
}

static void *szone_malloc(szone_t *szone, size_t size) {
    return szone_malloc_should_clear(szone, size, 0);
}

static void *szone_calloc(szone_t *szone, size_t num_items, size_t size) {
    return szone_malloc_should_clear(szone, num_items * size, 1);
}

static void *szone_valloc(szone_t *szone, size_t size) {
    void	*ptr;
    unsigned	num_pages;
    num_pages = round_page(size) >> vm_page_shift;
    ptr = large_and_huge_malloc(szone, num_pages);
    if (LOG(szone, ptr)) malloc_printf("szone_valloc returned %p\n", ptr);
    return ptr;
}

static size_t szone_size(szone_t *szone, const void *ptr) {
    size_t		size = 0;
    region_t		*region;
    large_entry_t	*entry;
    huge_entry_t	*huge;
    if (!ptr) return 0;
    if (LOG(szone, ptr)) malloc_printf("In szone_size for %p (szone=%p)\n", ptr, szone);
    if ((vm_address_t)ptr & (QUANTUM - 1)) return 0;
    if ((((unsigned)ptr) & (vm_page_size - 1)) && (MSIZE_FLAGS_FOR_PTR(ptr) & THIS_FREE)) {
        // not page aligned, but definitely not in use
        return 0;
    }
    // Try a small pointer
    region = region_for_ptr_no_lock(szone, ptr);
    // malloc_printf("FOUND REGION %p\n", region);
    if (region) {
        // this is indeed a valid pointer
        msize_t		msize_and_free = MSIZE_FLAGS_FOR_PTR(ptr);
        return (msize_and_free & THIS_FREE) ? 0 : ((msize_and_free & ~PREV_FREE) << SHIFT_QUANTUM) - PTR_HEADER_SIZE;
    }
    if (((unsigned)ptr) & (vm_page_size - 1)) {
        return 0;
    }
    SZONE_LOCK(szone);
    entry = large_entry_for_pointer_no_lock(szone, ptr);
    if (entry) {
        size = LARGE_ENTRY_SIZE(*entry);
    } else if ((huge = huge_entry_for_pointer_no_lock(szone, ptr))) {
        size = huge->size;
    }
    SZONE_UNLOCK(szone); 
    // malloc_printf("szone_size for large/huge %p returned %d\n", ptr, (unsigned)size);
    if (LOG(szone, ptr)) malloc_printf("szone_size for %p returned %d\n", ptr, (unsigned)size);
    return size;
}

static INLINE int try_realloc_small_in_place(szone_t *szone, void *ptr, size_t old_size, size_t new_size) {
    // returns 1 on success
    void	*next_block = (char *)ptr + old_size + PTR_HEADER_SIZE;
    msize_t	next_msize_and_free;
    msize_t	next_msize;
    region_t	region;
    msize_t	coalesced_msize;
    msize_t	leftover_msize;
    msize_t	new_msize_and_free;
    void	*following_ptr;
    SZONE_LOCK(szone);
    region = szone->regions[szone->num_regions - 1];
    if (((vm_address_t)ptr >= region) && ((vm_address_t)ptr < region + REGION_SIZE) && ((vm_address_t)next_block == REGION_END(region) - szone->num_bytes_free_in_last_region + PTR_HEADER_SIZE)) {
	// This could be optimized but it is so rare it's not worth it
	SZONE_UNLOCK(szone);
	return 0;
    }
    // If the next block is free, we coalesce
    next_msize_and_free = MSIZE_FLAGS_FOR_PTR(next_block);
#if DEBUG_MALLOC
    if ((vm_address_t)next_block & (QUANTUM - 1)) {
	szone_error(szone, "Internal invariant broken in realloc(next_block)", next_block);
    }
    if (next_msize_and_free & PREV_FREE) {
	malloc_printf("try_realloc_small_in_place: 0x%x=PREV_FREE|%d\n", next_msize_and_free, next_msize_and_free & ~PREV_FREE);
	SZONE_UNLOCK(szone);
	return 0;
    }
#endif
    next_msize = next_msize_and_free & ~THIS_FREE;
    if (!(next_msize_and_free & THIS_FREE) || !next_msize || (old_size + (next_msize << SHIFT_QUANTUM) < new_size)) {
	SZONE_UNLOCK(szone);
	return 0;
    }
    coalesced_msize = (new_size - old_size + QUANTUM - 1) >> SHIFT_QUANTUM;
    leftover_msize = next_msize - coalesced_msize;
    new_msize_and_free = MSIZE_FLAGS_FOR_PTR(ptr);
    // malloc_printf("Realloc in place for %p;  current msize=%d next_msize=%d wanted=%d\n", ptr, MSIZE_FLAGS_FOR_PTR(ptr), next_msize, new_size);
    free_list_remove_ptr(szone, next_block, next_msize);
    if ((leftover_msize < MIN_BLOCK) || (leftover_msize < coalesced_msize / 4)) {
	// don't bother splitting it off
	// malloc_printf("No leftover ");
	coalesced_msize = next_msize;
	leftover_msize = 0;
    } else {
	void	*leftover = next_block + (coalesced_msize << SHIFT_QUANTUM);
	// malloc_printf("Leftover ");
	free_list_add_ptr(szone, leftover, leftover_msize);
    }
    new_msize_and_free += coalesced_msize;
    MSIZE_FLAGS_FOR_PTR(ptr) = new_msize_and_free;
    following_ptr = FOLLOWING_PTR(ptr, new_msize_and_free & ~PREV_FREE);
    MSIZE_FLAGS_FOR_PTR(following_ptr) &= ~ PREV_FREE;
#if DEBUG_MALLOC
    {
	msize_t	ms = MSIZE_FLAGS_FOR_PTR(following_ptr);
	msize_t	pms = PREVIOUS_MSIZE(FOLLOWING_PTR(following_ptr, ms & ~THIS_FREE));
	malloc_printf("Following ptr of coalesced (%p) has msize_and_free=0x%x=%s%d end_of_block_marker=%d\n", following_ptr, ms, (ms & THIS_FREE) ? "THIS_FREE|" : "", ms & ~THIS_FREE, pms);
    }
    if (LOG(szone,ptr)) malloc_printf("In szone_realloc(), ptr=%p, msize=%d\n", ptr, MSIZE_FLAGS_FOR_PTR(ptr));
#endif
    CHECK(szone, __PRETTY_FUNCTION__);
    szone->num_bytes_in_small_objects += coalesced_msize << SHIFT_QUANTUM;
    SZONE_UNLOCK(szone);
    // malloc_printf("Extended ptr %p for realloc old=%d desired=%d new=%d leftover=%d\n", ptr, (unsigned)old_size, (unsigned)new_size, (unsigned)szone_size(szone, ptr), leftover_msize << SHIFT_QUANTUM);
    return 1;
}

static INLINE int try_realloc_large_in_place(szone_t *szone, void *ptr, size_t old_size, size_t new_size) {
    vm_address_t	addr = (vm_address_t)ptr + old_size;
    large_entry_t	*entry;
    kern_return_t	err;
    if (((szone->debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES) && PROTECT_SMALL)) return 0; // don't want to bother with the protected case
#if DEBUG_MALLOC
    if (old_size != ((old_size >> vm_page_shift) << vm_page_shift)) malloc_printf("*** old_size is %d\n", old_size);
#endif
    // malloc_printf("=== Trying (1) to extend %p from %d to %d\n", ptr, old_size, new_size);
    SZONE_LOCK(szone);
    entry = large_entry_for_pointer_no_lock(szone, (void *)addr);
    SZONE_UNLOCK(szone);
    if (entry) return 0; // large pointer already exist in table - extension is not going to work
    new_size = round_page(new_size);
    // malloc_printf("=== Trying (2) to extend %p from %d to %d\n", ptr, old_size, new_size);
    err = vm_allocate(mach_task_self(), &addr, new_size - old_size, VM_MAKE_TAG(VM_MEMORY_MALLOC_LARGE)); // we ask for allocation specifically at addr
    if (err) return 0;
    // we can just extend the block
    SZONE_LOCK(szone);
    entry = large_entry_for_pointer_no_lock(szone, ptr);
    if (!entry) szone_error(szone, "large entry reallocated is not properly in table", ptr);
    // malloc_printf("=== Successfully reallocated at end of %p from %d to %d\n", ptr, old_size, new_size);
    entry->address_and_num_pages = (vm_address_t)ptr | (new_size >> vm_page_shift);
    szone->num_bytes_in_large_objects += new_size - old_size;
    SZONE_UNLOCK(szone); // we release the lock asap
    return 1;
}

static void *szone_realloc(szone_t *szone, void *ptr, size_t new_size) {
    size_t		old_size = 0;
    void		*new_ptr;
    if (LOG(szone, ptr)) malloc_printf("In szone_realloc for %p, %d\n", ptr, (unsigned)new_size);
    if (!ptr) return szone_malloc(szone, new_size);
    old_size = szone_size(szone, ptr);
    if (!old_size) {
        szone_error(szone, "Pointer being reallocated was not allocated", ptr);
        return NULL;
    }
    if (old_size >= new_size) return ptr;
    if (!((szone->debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES) && PROTECT_SMALL) && ((new_size + PTR_HEADER_SIZE + QUANTUM - 1) < LARGE_THRESHOLD)) {
        // We now try to realloc in place
	if (try_realloc_small_in_place(szone, ptr, old_size, new_size)) return ptr;
    }
    if ((old_size > VM_COPY_THRESHOLD) && ((new_size + vm_page_size - 1) < (1 << (vm_page_shift + vm_page_shift)))) {
	// we know it's a large block, and not a huge block (both for old and new)
        kern_return_t	err = 0;
        unsigned	num_pages;
	large_entry_t	*entry;
	vm_range_t	range;
        num_pages = round_page(new_size) >> vm_page_shift;
	if (try_realloc_large_in_place(szone, ptr, old_size, new_size)) return ptr;
        new_ptr = large_and_huge_malloc(szone, num_pages);
        err = vm_copy(mach_task_self(), (vm_address_t)ptr, old_size, (vm_address_t)new_ptr);
        if (err) {
            szone_error(szone, "Can't vm_copy region", ptr);
        }
	// We do not want the kernel to alias the old and the new, so we deallocate the old pointer right away and tear down the ptr-to-size data structure
	SZONE_LOCK(szone);
	entry = large_entry_for_pointer_no_lock(szone, ptr);
	if (!entry) {
	    szone_error(szone, "Can't find entry for large copied block", ptr);
	}
	range = large_free_no_lock(szone, entry);
	SZONE_UNLOCK(szone); // we release the lock asap
	// we truly deallocate_pages, including guard pages
	deallocate_pages(szone, range.address, range.size, 0);
	if (LOG(szone, ptr)) malloc_printf("szone_realloc returned %p for %d\n", new_ptr, (unsigned)new_size);
	return new_ptr;
    } else {
	new_ptr = szone_malloc(szone, new_size);
	if (new_ptr == NULL)
	    return NULL;
        memcpy(new_ptr, ptr, old_size);
    }
    szone_free(szone, ptr);
    if (LOG(szone, ptr)) malloc_printf("szone_realloc returned %p for %d\n", new_ptr, (unsigned)new_size);
    return new_ptr;
}

static void szone_destroy(szone_t *szone) {
    unsigned	index;
    index = szone->num_large_entries;
    while (index--) {
        large_entry_t	*entry = szone->large_entries + index;
        if (!LARGE_ENTRY_IS_EMPTY(*entry)) {
            large_entry_t	range;
            range = *entry;
            // we deallocate_pages, including guard pages
            deallocate_pages(szone, LARGE_ENTRY_ADDRESS(range), LARGE_ENTRY_SIZE(range), szone->debug_flags);
        }
    }
    if (szone->num_large_entries * sizeof(large_entry_t) >= LARGE_THRESHOLD) large_entries_free_no_lock(szone, szone->large_entries, szone->num_large_entries); // we do not free in the small chunk case
    index = szone->num_huge_entries;
    while (index--) {
        huge_entry_t	*huge = szone->huge_entries + index;
        deallocate_pages(szone, huge->address, huge->size, szone->debug_flags);
    }
    // and now we free regions, with regions[0] as the last one (the final harakiri)
    index = szone->num_regions;
    while (index--) { // we skip the first region, that is the zone itself
        region_t	region = szone->regions[index];
        deallocate_pages(szone, REGION_ADDRESS(region), REGION_SIZE, 0);
    }
}

static size_t szone_good_size(szone_t *szone, size_t size) {
    if (!((szone->debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES) && PROTECT_SMALL) && (size < LARGE_THRESHOLD)) {
        // think small
        msize_t	msize = (size + PTR_HEADER_SIZE + QUANTUM - 1) >> SHIFT_QUANTUM;
        if (msize < MIN_BLOCK) msize = MIN_BLOCK;
        return (msize << SHIFT_QUANTUM) - PTR_HEADER_SIZE;
    } else {
        unsigned		num_pages;
        num_pages = round_page(size) >> vm_page_shift;
        if (!num_pages) num_pages = 1; // minimal allocation size for this
        return num_pages << vm_page_shift;
    }
}

unsigned szone_check_counter = 0;
unsigned szone_check_start = 0;
unsigned szone_check_modulo = 1;

static boolean_t szone_check_all(szone_t *szone, const char *function) {
    unsigned	index = 0;
    SZONE_LOCK(szone);
    while (index < szone->num_regions) {
        region_t	*region = szone->regions + index++;
        if (!szone_check_region(szone, region)) {
            SZONE_UNLOCK(szone);
            szone->debug_flags &= ~ CHECK_REGIONS;
            malloc_printf("*** malloc[%d]: Region %d incorrect szone_check_all(%s) counter=%d\n", getpid(), index-1, function, szone_check_counter);
            szone_error(szone, "Check: region incorrect", NULL);
            return 0;
        }
    }
    index = 0;
    while (index < MAX_GRAIN) {
        if (! free_list_check(szone, index)) {
            SZONE_UNLOCK(szone);
            szone->debug_flags &= ~ CHECK_REGIONS;
            malloc_printf("*** malloc[%d]: Free list incorrect (grain=%d) szone_check_all(%s) counter=%d\n", getpid(), index, function, szone_check_counter);
            szone_error(szone, "Check: free list incorrect", NULL);
            return 0;
        }
        index++;
    }
    SZONE_UNLOCK(szone);
    return 1;
}

static boolean_t szone_check(szone_t *szone) {
    if (! (++szone_check_counter % 10000)) {
        malloc_printf("At szone_check counter=%d\n", szone_check_counter);
    }
    if (szone_check_counter < szone_check_start) return 1;
    if (szone_check_counter % szone_check_modulo) return 1;
    return szone_check_all(szone, "");
}

static kern_return_t szone_ptr_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t zone_address, memory_reader_t reader, vm_range_recorder_t recorder) {
    szone_t		*szone;
    kern_return_t	err;
    if (!reader) reader = _szone_default_reader;
    // malloc_printf("Enumerator for zone 0x%x\n", zone_address);
    err = reader(task, zone_address, sizeof(szone_t), (void **)&szone);
    if (err) return err;
    // malloc_printf("Small ptrs enumeration for zone 0x%x\n", zone_address);
    err = small_in_use_enumerator(task, context, type_mask, (vm_address_t)szone->regions, szone->num_regions, reader, recorder);
    if (err) return err;
    // malloc_printf("Large ptrs enumeration for zone 0x%x\n", zone_address);
    err = large_in_use_enumerator(task, context, type_mask, (vm_address_t)szone->large_entries, szone->num_large_entries, reader, recorder);
    if (err) return err;
    // malloc_printf("Huge ptrs enumeration for zone 0x%x\n", zone_address);
    err = huge_in_use_enumerator(task, context, type_mask, (vm_address_t)szone->huge_entries, szone->num_huge_entries, reader, recorder);
    return err;
}

static void szone_print_free_list(szone_t *szone) {
    grain_t		grain = MAX_GRAIN;
    malloc_printf("Free Sizes: ");
    while (grain--) {
        free_list_t	*ptr = szone->free_list[grain];
        if (ptr) {
            unsigned	count = 0;
            while (ptr) {
                count++;
                // malloc_printf("%p ", ptr);
                ptr = ptr->next;
            }
            malloc_printf("%s%d[%d] ", (grain == MAX_GRAIN-1) ? ">=" : "", (grain+1)*QUANTUM, count);
        }
    }
    malloc_printf("\n");
}

static void szone_print(szone_t *szone, boolean_t verbose) {
    unsigned	info[scalable_zone_info_count];
    unsigned	index = 0;
    scalable_zone_info((void *)szone, info, scalable_zone_info_count);
    malloc_printf("Scalable zone %p: inUse=%d(%dKB) small=%d(%dKB) large=%d(%dKB) huge=%d(%dKB) guard_page=%d\n", szone, info[0], info[1] / 1024, info[2], info[3] / 1024, info[4], info[5] / 1024, info[6], info[7] / 1024, info[8]);
    malloc_printf("%d regions: \n", szone->num_regions);
    while (index < szone->num_regions) {
        region_t	*region = szone->regions + index;
        unsigned	counts[512];
        unsigned	ci = 0;
        unsigned	in_use = 0;
        vm_address_t	start = REGION_ADDRESS(*region) + QUANTUM;
        memset(counts, 0, 512 * sizeof(unsigned));
        while (start < REGION_END(*region)) {
            msize_t	msize_and_free = MSIZE_FLAGS_FOR_PTR(start);
            if (!(msize_and_free & THIS_FREE)) {
                msize_t	msize = msize_and_free & ~PREV_FREE;
                if (!msize) break; // last encountered
                // block in use
                if (msize < 512) counts[msize]++;
                start += msize << SHIFT_QUANTUM;
                in_use++;
            } else {
                msize_t	msize = msize_and_free & ~THIS_FREE;
                // free block
                start += msize << SHIFT_QUANTUM;
            }
        }
        malloc_printf("Region [0x%x-0x%x, %dKB] \tIn_use=%d ", REGION_ADDRESS(*region), REGION_END(*region), (int)REGION_SIZE / 1024, in_use);
        if (verbose) {
            malloc_printf("\n\tSizes in use: ");
            while (ci < 512) {
                if (counts[ci]) malloc_printf("%d[%d] ", ci << SHIFT_QUANTUM, counts[ci]);
                ci++;
            }
        }
        malloc_printf("\n");
        index++;
    }
    if (verbose) szone_print_free_list(szone);
    malloc_printf("Free in last zone %d\n", szone->num_bytes_free_in_last_region);
}

static void szone_log(malloc_zone_t *zone, void *log_address) {
    szone_t	*szone = (void *)zone;
    szone->log_address = log_address;
}

static void szone_force_lock(szone_t *szone) {
    // malloc_printf("szone_force_lock\n");
    SZONE_LOCK(szone);
}

static void szone_force_unlock(szone_t *szone) {
    // malloc_printf("szone_force_unlock\n");
    SZONE_UNLOCK(szone);
}

static struct malloc_introspection_t szone_introspect = {(void *)szone_ptr_in_use_enumerator, (void *)szone_good_size, (void *)szone_check, (void *)szone_print, szone_log, (void *)szone_force_lock, (void *)szone_force_unlock};

malloc_zone_t *create_scalable_zone(size_t initial_size, unsigned debug_flags) {
    szone_t		*szone;
    vm_address_t	addr;
    size_t		msize;
    size_t		msize_used = 0;
    // malloc_printf("=== create_scalable_zone(%d,%d);\n", initial_size, debug_flags);
    if (!vm_page_shift) {
        unsigned	page;
        vm_page_shift = 12; // the minimal for page sizes
        page = 1 << vm_page_shift;
        while (page != vm_page_size) { page += page; vm_page_shift++;};
        if (MIN_BLOCK * QUANTUM < sizeof(free_list_t) + PTR_HEADER_SIZE) {
            malloc_printf("*** malloc[%d]: inconsistant parameters\n", getpid());
        }
    }
    addr = allocate_pages(NULL, REGION_SIZE, 0, VM_MAKE_TAG(VM_MEMORY_MALLOC));
    if (!addr) return NULL;
    szone = (void *)(addr + QUANTUM);
    msize = (sizeof(szone_t) + PTR_HEADER_SIZE + QUANTUM-1) >> SHIFT_QUANTUM;
    MSIZE_FLAGS_FOR_PTR(szone) = msize;
    msize_used += msize; szone->num_small_objects++;
    szone->basic_zone.size = (void *)szone_size;
    szone->basic_zone.malloc = (void *)szone_malloc;
    szone->basic_zone.calloc = (void *)szone_calloc;
    szone->basic_zone.valloc = (void *)szone_valloc;
    szone->basic_zone.free = (void *)szone_free;
    szone->basic_zone.realloc = (void *)szone_realloc;
    szone->basic_zone.destroy = (void *)szone_destroy;
    szone->basic_zone.introspect = &szone_introspect;
    LOCK_INIT(szone->lock);
    szone->debug_flags = debug_flags;
    szone->regions = (void *)((char *)szone + (msize << SHIFT_QUANTUM));
    // we always reserve room for a few regions
    msize = (sizeof(region_t) * INITIAL_NUM_REGIONS + PTR_HEADER_SIZE + QUANTUM-1) >> SHIFT_QUANTUM;
    if (msize < MIN_BLOCK) msize = MIN_BLOCK;
    MSIZE_FLAGS_FOR_PTR(szone->regions) = msize;
    msize_used += msize; szone->num_small_objects++;
    szone->regions[0] = addr;
    szone->num_regions = 1;
    szone->num_bytes_free_in_last_region = REGION_SIZE - ((msize_used+1) << SHIFT_QUANTUM) + PTR_HEADER_SIZE;
    CHECK(szone, __PRETTY_FUNCTION__);
    return (malloc_zone_t *)szone;
}

/*********	The following is private API for debug and perf tools	************/

void scalable_zone_info(malloc_zone_t *zone, unsigned *info_to_fill, unsigned count) {
    szone_t	*szone = (void *)zone;
    unsigned	info[scalable_zone_info_count];
    // We do not lock to facilitate debug
    info[2] = szone->num_small_objects;
    info[3] = szone->num_bytes_in_small_objects;
    info[4] = szone->num_large_objects_in_use;
    info[5] = szone->num_bytes_in_large_objects;
    info[6] = szone->num_huge_entries;
    info[7] = szone->num_bytes_in_huge_objects;
    info[8] = szone->debug_flags;
    info[0] = info[2] + info[4] + info[6];
    info[1] = info[3] + info[5] + info[7];
    memcpy(info_to_fill, info, sizeof(unsigned)*count);
}

/********* Support code for emacs unexec ************/

/* History of freezedry version numbers:
 *
 * 1) Old malloc (before the scalable malloc implementation in this file
 *    existed).
 * 2) Original freezedrying code for scalable malloc.  This code was apparently
 *    based on the old freezedrying code and was fundamentally flawed in its
 *    assumption that tracking allocated memory regions was adequate to fake
 *    operations on freezedried memory.  This doesn't work, since scalable
 *    malloc does not store flags in front of large page-aligned allocations.
 * 3) Original szone-based freezedrying code.
 *
 * No version backward compatibility is provided, but the version number does
 * make it possible for malloc_jumpstart() to return an error if the application
 * was freezedried with an older version of malloc.
 */
#define MALLOC_FREEZEDRY_VERSION 3

typedef struct {
    unsigned version;
    unsigned nszones;
    szone_t *szones;
} malloc_frozen;

static void *frozen_malloc(szone_t *zone, size_t new_size) {
    return malloc(new_size);
}

static void *frozen_calloc(szone_t *zone, size_t num_items, size_t size) {
    return calloc(num_items, size);
}

static void *frozen_valloc(szone_t *zone, size_t new_size) {
    return valloc(new_size);
}

static void *frozen_realloc(szone_t *zone, void *ptr, size_t new_size) {
    size_t	old_size = szone_size(zone, ptr);
    void	*new_ptr;

    if (new_size <= old_size) {
	return ptr;
    }

    new_ptr = malloc(new_size);

    if (old_size > 0) {
	memcpy(new_ptr, ptr, old_size);
    }

    return new_ptr;
}

static void frozen_free(szone_t *zone, void *ptr) {
}

static void frozen_destroy(szone_t *zone) {
}

/********* Pseudo-private API for emacs unexec ************/

/*
 * malloc_freezedry() records all of the szones in use, so that they can be
 * partially reconstituted by malloc_jumpstart().  Due to the differences
 * between reconstituted memory regions and those created by the szone code,
 * care is taken not to reallocate from the freezedried memory, except in the
 * case of a non-growing realloc().
 *
 * Due to the flexibility provided by the zone registration mechanism, it is
 * impossible to implement generic freezedrying for any zone type.  This code
 * only handles applications that use the szone allocator, so malloc_freezedry()
 * returns 0 (error) if any non-szone zones are encountered.
 */

int malloc_freezedry(void) {
    extern unsigned malloc_num_zones;
    extern malloc_zone_t **malloc_zones;
    malloc_frozen *data;
    unsigned i;

    /* Allocate space in which to store the freezedry state. */
    data = (malloc_frozen *) malloc(sizeof(malloc_frozen));

    /* Set freezedry version number so that malloc_jumpstart() can check for
     * compatibility. */
    data->version = MALLOC_FREEZEDRY_VERSION;

    /* Allocate the array of szone pointers. */
    data->nszones = malloc_num_zones;
    data->szones = (szone_t *) calloc(malloc_num_zones, sizeof(szone_t));

    /* Fill in the array of szone structures.  They are copied rather than
     * referenced, since the originals are likely to be clobbered during malloc
     * initialization. */
    for (i = 0; i < malloc_num_zones; i++) {
	if (strcmp(malloc_zones[i]->zone_name, "DefaultMallocZone")) {
	    /* Unknown zone type. */
	    free(data->szones);
	    free(data);
	    return 0;
	}
	memcpy(&data->szones[i], malloc_zones[i], sizeof(szone_t));
    }

    return (int) data;
}

int malloc_jumpstart(int cookie) {
    malloc_frozen *data = (malloc_frozen *) cookie;
    unsigned i;

    if (data->version != MALLOC_FREEZEDRY_VERSION) {
	/* Unsupported freezedry version. */
	return 1;
    }

    for (i = 0; i < data->nszones; i++) {
	/* Set function pointers.  Even the functions that stay the same must be
	 * set, since there are no guarantees that they will be mapped to the
	 * same addresses. */
	data->szones[i].basic_zone.size = (void *) szone_size;
	data->szones[i].basic_zone.malloc = (void *) frozen_malloc;
	data->szones[i].basic_zone.calloc = (void *) frozen_calloc;
	data->szones[i].basic_zone.valloc = (void *) frozen_valloc;
	data->szones[i].basic_zone.free = (void *) frozen_free;
	data->szones[i].basic_zone.realloc = (void *) frozen_realloc;
	data->szones[i].basic_zone.destroy = (void *) frozen_destroy;
	data->szones[i].basic_zone.introspect = &szone_introspect;

	/* Register the freezedried zone. */
	malloc_zone_register(&data->szones[i].basic_zone);
    }

    return 0;
}
