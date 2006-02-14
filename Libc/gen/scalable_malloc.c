/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

/* Author: Bertrand Serlet, August 1999 */

#include "scalable_malloc.h"

#include <pthread_internals.h>

#include <unistd.h>
#include <libc.h>
#include <mach/vm_statistics.h>
#include <mach/mach_init.h>

/*********************	DEFINITIONS	************************/

#define DEBUG_MALLOC	0	// set to one to debug malloc itself

#define DEBUG_CLIENT	0	// set to one to help debug a nasty memory smasher

#if DEBUG_MALLOC
#warning DEBUG_MALLOC ENABLED
# define INLINE
# define ALWAYSINLINE
# define CHECK_LOCKED(szone, fun)						\
do {										\
    if (__is_threaded && TRY_LOCK(szone->lock)) {				\
	malloc_printf("*** lock was not set %p in %s\n", szone->lock, fun);	\
    }										\
} while (0)
#else
# define INLINE	__inline__
# define ALWAYSINLINE __attribute__((always_inline))
# define CHECK_LOCKED(szone, fun)	{}
#endif

/*
 * Access to global variables is slow, so optimise our handling of vm_page_size
 * and vm_page_shift.
 */
#define _vm_page_size 	vm_page_size	/* to get to the originals */
#define _vm_page_shift	vm_page_shift
#define vm_page_size	4096		/* our normal working sizes */
#define vm_page_shift	12

typedef unsigned short msize_t; // a size in multiples of SHIFT_SMALL_QUANTUM or SHIFT_TINY_QUANTUM

/*
 * Note that in the LP64 case, this is 24 bytes, necessitating the 32-byte tiny grain size.
 */
typedef struct {
    uintptr_t	checksum;
    void	*previous;
    void	*next;
} free_list_t;

typedef struct {
    uintptr_t 	address_and_num_pages;
    // this type represents both an address and a number of pages
    // the low bits are the number of pages; the high bits are the address
    // note that the exact number of bits used for depends on the page size
    // also, this cannot represent pointers larger than 1 << (vm_page_shift * 2)
} compact_range_t;

typedef unsigned char	grain_t;

#define CHECK_REGIONS			(1 << 31)

#ifdef __LP64__
# define CHECKSUM_MAGIC			0xdeadbeef0badc0de
#else
# define CHECKSUM_MAGIC			0x357B
#endif

#define MAX_RECORDER_BUFFER	256

/*********************	DEFINITIONS for tiny	************************/

/*
 * Memory in the Tiny range is allocated from regions (heaps) pointed to by the szone's tiny_regions
 * pointer.
 *
 * Each region is laid out as a heap (1MB in 32-bit mode, 2MB in 64-bit mode), followed by a header
 * block.  The header block is arranged:
 *
 * 0x0
 * 	header bits
 * 0x2000
 *	0xffffffff pad word
 * 0x2004
 *	in-use bits
 * 0x4004
 *	pad word (not written)
 *
 * Each bitfield comprises NUM_TINY_BLOCKS bits, and refers to the corresponding TINY_QUANTUM block
 * within the heap.
 *
 * The bitfields are used to encode the state of memory within the heap.  The header bit indicates
 * that the corresponding quantum is the first quantum in a block (either in use or free).  The
 * in-use bit is set for the header if the block has been handed out (allocated).  If the header
 * bit is not set, the in-use bit is invalid.
 *
 * The szone maintains an array of 32 freelists, each of which is used to hold free objects
 * of the corresponding quantum size.
 *
 * A free block is laid out as:
 *
 * Offset (32-bit mode)	(64-bit mode)
 * 0x0			0x0
 *					checksum
 * 0x4			0x08
 *					previous
 * 0x8			0x10
 *					next
 * 0xc			0x18
 *					size (in quantum counts)
 * end - 2		end - 2
 *					size (in quantum counts)
 * end			end
 *
 * All fields are pointer-sized, except for the size which is an unsigned short.
 *
 */

#ifdef __LP64__
# define SHIFT_TINY_QUANTUM			5	// Required to fit free_list_t
#else
# define SHIFT_TINY_QUANTUM			4	// Required for AltiVec
#endif
#define	TINY_QUANTUM				(1 << SHIFT_TINY_QUANTUM)

#define FOLLOWING_TINY_PTR(ptr,msize)		(((unsigned char *)(ptr)) + ((msize) << SHIFT_TINY_QUANTUM))

#define NUM_TINY_SLOTS				32	// number of slots for free-lists

#define SHIFT_NUM_TINY_BLOCKS			16
#define NUM_TINY_BLOCKS				(1 << SHIFT_NUM_TINY_BLOCKS)
#define TINY_BLOCKS_ALIGN			(SHIFT_NUM_TINY_BLOCKS + SHIFT_TINY_QUANTUM)

/*
 * Enough room for the data, followed by the bit arrays (2-bits per block) plus 2 words of padding
 * as our bitmap operators overflow, plus rounding to the nearest page.
 */
#define TINY_REGION_SIZE	((NUM_TINY_BLOCKS * TINY_QUANTUM + (NUM_TINY_BLOCKS >> 2) + 8 + vm_page_size - 1) & ~ (vm_page_size - 1))

/*
 * Obtain the size of a free tiny block (in msize_t units).
 */
#ifdef __LP64__
# define TINY_FREE_SIZE(ptr)			(((msize_t *)(ptr))[14])
#else
# define TINY_FREE_SIZE(ptr)			(((msize_t *)(ptr))[6])
#endif
/*
 * The size is also kept at the very end of a free block.
 */
#define TINY_PREVIOUS_MSIZE(ptr)		((msize_t *)(ptr))[-1]

/*
 * Beginning and end pointers for a region's heap.
 */
#define TINY_REGION_ADDRESS(region)		((void *)(region))
#define TINY_REGION_END(region)			(TINY_REGION_ADDRESS(region) + (1 << TINY_BLOCKS_ALIGN))

/*
 * Locate the heap base for a pointer known to be within a tiny region.
 */
#define TINY_REGION_FOR_PTR(_p)			((void *)((uintptr_t)(_p) & ~((1 << TINY_BLOCKS_ALIGN) - 1)))

/*
 * Convert between byte and msize units.
 */
#define TINY_BYTES_FOR_MSIZE(_m)		((_m) << SHIFT_TINY_QUANTUM)
#define TINY_MSIZE_FOR_BYTES(_b)		((_b) >> SHIFT_TINY_QUANTUM)

/*
 * Locate the block header for a pointer known to be within a tiny region.
 */
#define TINY_BLOCK_HEADER_FOR_PTR(_p)		((void *)(((((uintptr_t)(_p)) >> TINY_BLOCKS_ALIGN) + 1) << TINY_BLOCKS_ALIGN))

/*
 * Locate the inuse map for a given block header pointer.
 */  
#define TINY_INUSE_FOR_HEADER(_h)		((void *)((uintptr_t)(_h) + (NUM_TINY_BLOCKS >> 3) + 4))

/*
 * Compute the bitmap index for a pointer known to be within a tiny region.
 */
#define TINY_INDEX_FOR_PTR(_p) 			(((uintptr_t)(_p) >> SHIFT_TINY_QUANTUM) & (NUM_TINY_BLOCKS - 1))

typedef void *tiny_region_t;

#define INITIAL_NUM_TINY_REGIONS		24	// must be even for szone to be aligned

#define TINY_CACHE	1	// This governs a last-free cache of 1 that bypasses the free-list

#if ! TINY_CACHE
#warning TINY_CACHE turned off
#endif

/*********************	DEFINITIONS for small	************************/

/*
 * Memory in the Small range is allocated from regions (heaps) pointed to by the szone's small_regions
 * pointer.
 *
 * Each region is laid out as a heap (8MB in 32-bit and 64-bit mode), followed by the metadata array.
 * The array is arranged as an array of shorts, one for each SMALL_QUANTUM in the heap.
 *
 * The MSB of each short is set for the first quantum in a free block.  The low 15 bits encode the
 * block size (in SMALL_QUANTUM units), or are zero if the quantum is not the first in a block.
 *
 * The szone maintains an array of 32 freelists, each of which is used to hold free objects
 * of the corresponding quantum size.
 *
 * A free block is laid out as:
 *
 * Offset (32-bit mode)	(64-bit mode)
 * 0x0			0x0
 *					checksum
 * 0x4			0x08
 *					previous
 * 0x8			0x10
 *					next
 * 0xc			0x18
 *					size (in quantum counts)
 * end - 2		end - 2
 *					size (in quantum counts)
 * end			end
 *
 * All fields are pointer-sized, except for the size which is an unsigned short.
 *
 */

#define SMALL_IS_FREE				(1 << 15)

#define	SHIFT_SMALL_QUANTUM			(SHIFT_TINY_QUANTUM + 5)	// 9
#define	SMALL_QUANTUM				(1 << SHIFT_SMALL_QUANTUM) // 512 bytes

#define FOLLOWING_SMALL_PTR(ptr,msize)		(((unsigned char *)(ptr)) + ((msize) << SHIFT_SMALL_QUANTUM))

#define NUM_SMALL_SLOTS				32	// number of slots for free-lists

/*
 * We can only represent up to 1<<15 for msize; but we choose to stay even below that to avoid the
 * convention msize=0 => msize = (1<<15)
 */
#define SHIFT_NUM_SMALL_BLOCKS			14
#define NUM_SMALL_BLOCKS			(1 << SHIFT_NUM_SMALL_BLOCKS)
#define SMALL_BLOCKS_ALIGN			(SHIFT_NUM_SMALL_BLOCKS + SHIFT_SMALL_QUANTUM) // 23
#define SMALL_REGION_SIZE			(NUM_SMALL_BLOCKS * SMALL_QUANTUM + NUM_SMALL_BLOCKS * 2)	// data + meta data

#define SMALL_PREVIOUS_MSIZE(ptr)		((msize_t *)(ptr))[-1]

/*
 * Convert between byte and msize units.
 */
#define SMALL_BYTES_FOR_MSIZE(_m)		((_m) << SHIFT_SMALL_QUANTUM)
#define SMALL_MSIZE_FOR_BYTES(_b)		((_b) >> SHIFT_SMALL_QUANTUM)


#define SMALL_REGION_ADDRESS(region)		((unsigned char *)region)
#define SMALL_REGION_END(region)		(SMALL_REGION_ADDRESS(region) + (1 << SMALL_BLOCKS_ALIGN))

/*
 * Locate the heap base for a pointer known to be within a small region.
 */
#define SMALL_REGION_FOR_PTR(_p)		((void *)((uintptr_t)(_p) & ~((1 << SMALL_BLOCKS_ALIGN) - 1)))

/*
 * Locate the metadata base for a pointer known to be within a small region.
 */
#define SMALL_META_HEADER_FOR_PTR(_p)		((msize_t *)(((((uintptr_t)(_p)) >> SMALL_BLOCKS_ALIGN) + 1) << SMALL_BLOCKS_ALIGN))

/*
 * Compute the metadata index for a pointer known to be within a small region.
 */
#define SMALL_META_INDEX_FOR_PTR(_p)	       (((uintptr_t)(_p) >> SHIFT_SMALL_QUANTUM) & (NUM_SMALL_BLOCKS - 1))

/*
 * Find the metadata word for a pointer known to be within a small region.
 */
#define SMALL_METADATA_FOR_PTR(_p)		(SMALL_META_HEADER_FOR_PTR(_p) + SMALL_META_INDEX_FOR_PTR(_p))

/*
 * Determine whether a pointer known to be within a small region points to memory which is free.
 */
#define SMALL_PTR_IS_FREE(_p)			(*SMALL_METADATA_FOR_PTR(_p) & SMALL_IS_FREE)

/*
 * Extract the msize value for a pointer known to be within a small region.
 */
#define SMALL_PTR_SIZE(_p)			(*SMALL_METADATA_FOR_PTR(_p) & ~SMALL_IS_FREE)

typedef void * small_region_t;

#define INITIAL_NUM_SMALL_REGIONS		6	// must be even for szone to be aligned

#define PROTECT_SMALL				0	// Should be 0: 1 is too slow for normal use

#define SMALL_CACHE	1
#if !SMALL_CACHE
#warning SMALL_CACHE turned off
#endif

/*********************	DEFINITIONS for large	************************/

#define LARGE_THRESHOLD			(15 * 1024) // at or above this use "large"

#if (LARGE_THRESHOLD > NUM_SMALL_SLOTS * SMALL_QUANTUM)
#error LARGE_THRESHOLD should always be less than NUM_SMALL_SLOTS * SMALL_QUANTUM
#endif

#define VM_COPY_THRESHOLD		(40 * 1024)
    // When all memory is touched after a copy, vm_copy() is always a lose
    // But if the memory is only read, vm_copy() wins over memmove() at 3 or 4 pages (on a G3/300MHz)
    // This must be larger than LARGE_THRESHOLD

/*
 * Given a large_entry, return the address of the allocated block.
 */
#define LARGE_ENTRY_ADDRESS(entry)					\
    (void *)(((entry).address_and_num_pages >> vm_page_shift) << vm_page_shift)

/*
 * Given a large entry, return the number of pages or bytes in the allocated block.
 */
#define LARGE_ENTRY_NUM_PAGES(entry)					\
    ((entry).address_and_num_pages & (vm_page_size - 1))
#define LARGE_ENTRY_SIZE(entry)						\
    (LARGE_ENTRY_NUM_PAGES(entry) << vm_page_shift)

/*
 * Compare a pointer with a large entry.
 */
#define LARGE_ENTRY_MATCHES(entry,ptr)					\
    ((((entry).address_and_num_pages - (uintptr_t)(ptr)) >> vm_page_shift) == 0)

#define LARGE_ENTRY_IS_EMPTY(entry)	(((entry).address_and_num_pages) == 0)

typedef compact_range_t large_entry_t;

/*********************	DEFINITIONS for huge	************************/

typedef vm_range_t huge_entry_t;

/*********************	zone itself	************************/

typedef struct {
    malloc_zone_t	basic_zone;
    pthread_lock_t	lock;
    unsigned		debug_flags;
    void		*log_address;

    /* Regions for tiny objects */
    unsigned		num_tiny_regions;
    tiny_region_t	*tiny_regions;
    void		*last_tiny_free; // low SHIFT_TINY_QUANTUM indicate the msize
    unsigned		tiny_bitmap; // cache of the 32 free lists
    free_list_t		*tiny_free_list[NUM_TINY_SLOTS]; // 31 free lists for 1*TINY_QUANTUM to 31*TINY_QUANTUM plus 1 for larger than 32*SMALL_QUANTUM
    size_t		tiny_bytes_free_at_end; // the last free region in the last block is treated as a big block in use that is not accounted for
    unsigned		num_tiny_objects;
    size_t		num_bytes_in_tiny_objects;

    /* Regions for small objects */
    unsigned		num_small_regions;
    small_region_t	*small_regions;
    void		*last_small_free; // low SHIFT_SMALL_QUANTUM indicate the msize
    unsigned		small_bitmap; // cache of the free list
    free_list_t		*small_free_list[NUM_SMALL_SLOTS];
    size_t		small_bytes_free_at_end; // the last free region in the last block is treated as a big block in use that is not accounted for
    unsigned		num_small_objects;
    size_t		num_bytes_in_small_objects;

    /* large objects: vm_page_shift <= log2(size) < 2 *vm_page_shift */
    unsigned		num_large_objects_in_use;
    unsigned		num_large_entries;
    large_entry_t	*large_entries; // hashed by location; null entries don't count
    size_t		num_bytes_in_large_objects;
    
    /* huge objects: log2(size) >= 2 *vm_page_shift */
    unsigned		num_huge_entries;
    huge_entry_t	*huge_entries;
    size_t		num_bytes_in_huge_objects;

    /* Initial region list */
    tiny_region_t	initial_tiny_regions[INITIAL_NUM_TINY_REGIONS];
    small_region_t	initial_small_regions[INITIAL_NUM_SMALL_REGIONS];
} szone_t;

#if DEBUG_MALLOC || DEBUG_CLIENT
static void		szone_sleep(void);
#endif
static void		szone_error(szone_t *szone, const char *msg, const void *ptr);
static void		protect(szone_t *szone, void *address, size_t size, unsigned protection, unsigned debug_flags);
static void		*allocate_pages(szone_t *szone, size_t size, unsigned char align, unsigned debug_flags, int vm_page_label);
static void		deallocate_pages(szone_t *szone, void *addr, size_t size, unsigned debug_flags);
static kern_return_t	_szone_default_reader(task_t task, vm_address_t address, vm_size_t size, void **ptr);

static INLINE void	free_list_checksum(szone_t *szone, free_list_t *ptr, const char *msg) ALWAYSINLINE;
static INLINE void	free_list_set_checksum(szone_t *szone, free_list_t *ptr) ALWAYSINLINE;
static unsigned		free_list_count(const free_list_t *ptr);

static INLINE msize_t	get_tiny_meta_header(const void *ptr, boolean_t *is_free) ALWAYSINLINE;
static INLINE void	set_tiny_meta_header_in_use(const void *ptr, msize_t msize) ALWAYSINLINE;
static INLINE void	set_tiny_meta_header_middle(const void *ptr) ALWAYSINLINE;
static INLINE void	set_tiny_meta_header_free(const void *ptr, msize_t msize) ALWAYSINLINE;
static INLINE boolean_t	tiny_meta_header_is_free(const void *ptr) ALWAYSINLINE;
static INLINE void	*tiny_previous_preceding_free(void *ptr, msize_t *prev_msize) ALWAYSINLINE;
static INLINE void	tiny_free_list_add_ptr(szone_t *szone, void *ptr, msize_t msize) ALWAYSINLINE;
static INLINE void	tiny_free_list_remove_ptr(szone_t *szone, void *ptr, msize_t msize) ALWAYSINLINE;
static INLINE tiny_region_t *tiny_region_for_ptr_no_lock(szone_t *szone, const void *ptr) ALWAYSINLINE;
static INLINE void	tiny_free_no_lock(szone_t *szone, tiny_region_t *region, void *ptr, msize_t msize) ALWAYSINLINE;
static void		*tiny_malloc_from_region_no_lock(szone_t *szone, msize_t msize);
static INLINE boolean_t	try_realloc_tiny_in_place(szone_t *szone, void *ptr, size_t old_size, size_t new_size) ALWAYSINLINE;
static boolean_t	tiny_check_region(szone_t *szone, tiny_region_t *region);
static kern_return_t	tiny_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t region_address, unsigned short num_regions, size_t tiny_bytes_free_at_end, memory_reader_t reader, vm_range_recorder_t recorder);
static INLINE void	*tiny_malloc_from_free_list(szone_t *szone, msize_t msize) ALWAYSINLINE;
static INLINE void	*tiny_malloc_should_clear(szone_t *szone, msize_t msize, boolean_t cleared_requested) ALWAYSINLINE;
static INLINE void	free_tiny(szone_t *szone, void *ptr, tiny_region_t *tiny_region) ALWAYSINLINE;
static void		print_tiny_free_list(szone_t *szone);
static void		print_tiny_region(boolean_t verbose, tiny_region_t region, size_t bytes_at_end);
static boolean_t	tiny_free_list_check(szone_t *szone, grain_t slot);

static INLINE void	small_meta_header_set_is_free(msize_t *meta_headers, unsigned index, msize_t msize) ALWAYSINLINE;
static INLINE void	small_meta_header_set_in_use(msize_t *meta_headers, msize_t index, msize_t msize) ALWAYSINLINE;
static INLINE void	small_meta_header_set_middle(msize_t *meta_headers, msize_t index) ALWAYSINLINE;
static void		small_free_list_add_ptr(szone_t *szone, void *ptr, msize_t msize);
static void		small_free_list_remove_ptr(szone_t *szone, void *ptr, msize_t msize);
static INLINE small_region_t *small_region_for_ptr_no_lock(szone_t *szone, const void *ptr) ALWAYSINLINE;
static INLINE void	small_free_no_lock(szone_t *szone, small_region_t *region, void *ptr, msize_t msize) ALWAYSINLINE;
static void		*small_malloc_from_region_no_lock(szone_t *szone, msize_t msize);
static INLINE boolean_t	try_realloc_small_in_place(szone_t *szone, void *ptr, size_t old_size, size_t new_size) ALWAYSINLINE;
static boolean_t	szone_check_small_region(szone_t *szone, small_region_t *region);
static kern_return_t	small_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t region_address, unsigned short num_regions, size_t small_bytes_free_at_end, memory_reader_t reader, vm_range_recorder_t recorder);
static INLINE void	*small_malloc_from_free_list(szone_t *szone, msize_t msize) ALWAYSINLINE;
static INLINE void	*small_malloc_should_clear(szone_t *szone, msize_t msize, boolean_t cleared_requested) ALWAYSINLINE;
static INLINE void	*small_malloc_cleared_no_lock(szone_t *szone, msize_t msize) ALWAYSINLINE;
static INLINE void	free_small(szone_t *szone, void *ptr, small_region_t *small_region) ALWAYSINLINE;
static void		print_small_free_list(szone_t *szone);
static void		print_small_region(szone_t *szone, boolean_t verbose, small_region_t *region, size_t bytes_at_end);
static boolean_t	small_free_list_check(szone_t *szone, grain_t grain);

#if DEBUG_MALLOC
static void		large_debug_print(szone_t *szone);
#endif
static large_entry_t	*large_entry_for_pointer_no_lock(szone_t *szone, const void *ptr);
static void		large_entry_insert_no_lock(szone_t *szone, large_entry_t range);
static INLINE void	large_entries_rehash_after_entry_no_lock(szone_t *szone, large_entry_t *entry) ALWAYSINLINE;
static INLINE large_entry_t *large_entries_alloc_no_lock(szone_t *szone, unsigned num) ALWAYSINLINE;
static void		large_entries_free_no_lock(szone_t *szone, large_entry_t *entries, unsigned num, vm_range_t *range_to_deallocate);
static void		large_entries_grow_no_lock(szone_t *szone, vm_range_t *range_to_deallocate);
static vm_range_t	large_free_no_lock(szone_t *szone, large_entry_t *entry);
static kern_return_t	large_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t large_entries_address, unsigned num_entries, memory_reader_t reader, vm_range_recorder_t recorder);
static huge_entry_t	*huge_entry_for_pointer_no_lock(szone_t *szone, const void *ptr);
static boolean_t	huge_entry_append(szone_t *szone, huge_entry_t huge);
static kern_return_t	huge_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t huge_entries_address, unsigned num_entries, memory_reader_t reader, vm_range_recorder_t recorder);
static void		*large_and_huge_malloc(szone_t *szone, unsigned num_pages);
static INLINE void	free_large_or_huge(szone_t *szone, void *ptr) ALWAYSINLINE;
static INLINE int	try_realloc_large_or_huge_in_place(szone_t *szone, void *ptr, size_t old_size, size_t new_size) ALWAYSINLINE;

static void		szone_free(szone_t *szone, void *ptr);
static INLINE void	*szone_malloc_should_clear(szone_t *szone, size_t size, boolean_t cleared_requested) ALWAYSINLINE;
static void		*szone_malloc(szone_t *szone, size_t size);
static void		*szone_calloc(szone_t *szone, size_t num_items, size_t size);
static void		*szone_valloc(szone_t *szone, size_t size);
static size_t		szone_size(szone_t *szone, const void *ptr);
static void		*szone_realloc(szone_t *szone, void *ptr, size_t new_size);
static unsigned		szone_batch_malloc(szone_t *szone, size_t size, void **results, unsigned count);
static void		szone_batch_free(szone_t *szone, void **to_be_freed, unsigned count);
static void		szone_destroy(szone_t *szone);
static size_t		szone_good_size(szone_t *szone, size_t size);

static boolean_t	szone_check_all(szone_t *szone, const char *function);
static boolean_t	szone_check(szone_t *szone);
static kern_return_t	szone_ptr_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t zone_address, memory_reader_t reader, vm_range_recorder_t recorder);
static void		szone_print(szone_t *szone, boolean_t verbose);
static void		szone_log(malloc_zone_t *zone, void *log_address);
static void		szone_force_lock(szone_t *szone);
static void		szone_force_unlock(szone_t *szone);

static void		szone_statistics(szone_t *szone, malloc_statistics_t *stats);

static void		*frozen_malloc(szone_t *zone, size_t new_size);
static void		*frozen_calloc(szone_t *zone, size_t num_items, size_t size);
static void		*frozen_valloc(szone_t *zone, size_t new_size);
static void		*frozen_realloc(szone_t *zone, void *ptr, size_t new_size);
static void		frozen_free(szone_t *zone, void *ptr);
static void		frozen_destroy(szone_t *zone);

#if DEBUG_MALLOC
# define LOG(szone,ptr)							\
    (szone->log_address && (((uintptr_t)szone->log_address == -1) || (szone->log_address == (void *)(ptr))))
#else
# define LOG(szone,ptr)		0
#endif

#define SZONE_LOCK(szone)						\
	do {								\
	    LOCK(szone->lock);						\
	} while (0)

#define SZONE_UNLOCK(szone)						\
	do {								\
	    UNLOCK(szone->lock);					\
	} while (0)

#define LOCK_AND_NOTE_LOCKED(szone,locked)				\
do {									\
    CHECK(szone, __PRETTY_FUNCTION__);					\
    locked = 1; SZONE_LOCK(szone);					\
} while (0)

#if DEBUG_MALLOC || DEBUG_CLIENT
# define CHECK(szone,fun)						\
    if ((szone)->debug_flags & CHECK_REGIONS) szone_check_all(szone, fun)
#else
# define CHECK(szone,fun)	do {} while (0)
#endif

/*********************	VERY LOW LEVEL UTILITIES  ************************/

#if DEBUG_MALLOC || DEBUG_CLIENT
static void
szone_sleep(void)
{

    if (getenv("MallocErrorSleep")) {
	malloc_printf("*** sleeping to help debug\n");
	sleep(3600); // to help debug
    }
}
#endif

static void
szone_error(szone_t *szone, const char *msg, const void *ptr)
{

    if (szone) SZONE_UNLOCK(szone);
    if (ptr) {
	malloc_printf("*** error for object %p: %s\n", ptr, msg);
    } else {
	malloc_printf("*** error: %s\n", msg);
    }
    malloc_printf("*** set a breakpoint in szone_error to debug\n");
#if DEBUG_MALLOC
    szone_print(szone, 1);
    szone_sleep();
#endif
#if DEBUG_CLIENT
    szone_sleep();
#endif
}

static void
protect(szone_t *szone, void *address, size_t size, unsigned protection, unsigned debug_flags)
{
    kern_return_t	err;

    if (!(debug_flags & SCALABLE_MALLOC_DONT_PROTECT_PRELUDE)) {
	err = vm_protect(mach_task_self(), (vm_address_t)(uintptr_t)address - vm_page_size, vm_page_size, 0, protection);
	if (err) {
	    malloc_printf("*** can't protect(%p) region for prelude guard page at %p\n",
	      protection,address - (1 << vm_page_shift));
	}
    }
    if (!(debug_flags & SCALABLE_MALLOC_DONT_PROTECT_POSTLUDE)) {
	err = vm_protect(mach_task_self(), (vm_address_t)(uintptr_t)address + size, vm_page_size, 0, protection);
	if (err) {
	    malloc_printf("*** can't protect(%p) region for postlude guard page at %p\n",
	      protection, address + size);
	}
    }
}

static void *
allocate_pages(szone_t *szone, size_t size, unsigned char align, unsigned debug_flags, int vm_page_label)
{
    // align specifies a desired alignment (as a log) or 0 if no alignment requested
    kern_return_t	err;
    vm_address_t	vm_addr;
    uintptr_t		addr, aligned_address;
    boolean_t		add_guard_pages = debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES;
    size_t		allocation_size = round_page(size);
    size_t		delta;

    if (align) add_guard_pages = 0; // too cumbersome to deal with that
    if (!allocation_size) allocation_size = 1 << vm_page_shift;
    if (add_guard_pages) allocation_size += 2 * (1 << vm_page_shift);
    if (align) allocation_size += (size_t)1 << align;
    err = vm_allocate(mach_task_self(), &vm_addr, allocation_size, vm_page_label | 1);
    if (err) {
	malloc_printf("*** vm_allocate(size=%lld) failed (error code=%d)\n", (long long)size, err);
	szone_error(szone, "can't allocate region", NULL);
	return NULL;
    }
    addr = (uintptr_t)vm_addr;
    if (align) {
	aligned_address = (addr + ((uintptr_t)1 << align) - 1) & ~ (((uintptr_t)1 << align) - 1);
	if (aligned_address != addr) {
	    delta = aligned_address - addr;
	    err = vm_deallocate(mach_task_self(), (vm_address_t)addr, delta);
	    if (err)
		malloc_printf("*** freeing unaligned header failed with %d\n", err);
	    addr = aligned_address;
	    allocation_size -= delta;
	}
	if (allocation_size > size) {
	    err = vm_deallocate(mach_task_self(), (vm_address_t)addr + size, allocation_size - size);
	    if (err)
		malloc_printf("*** freeing unaligned footer failed with %d\n", err);
	}
    }
    if (add_guard_pages) {
	addr += (uintptr_t)1 << vm_page_shift;
	protect(szone, (void *)addr, size, 0, debug_flags);
    }
    return (void *)addr;
}

static void
deallocate_pages(szone_t *szone, void *addr, size_t size, unsigned debug_flags)
{
    kern_return_t	err;
    boolean_t		add_guard_pages = debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES;

    if (add_guard_pages) {
	addr -= 1 << vm_page_shift;
	size += 2 * (1 << vm_page_shift);
    }
    err = vm_deallocate(mach_task_self(), (vm_address_t)addr, size);
    if (err && szone)
	szone_error(szone, "Can't deallocate_pages region", addr);
}

static kern_return_t
_szone_default_reader(task_t task, vm_address_t address, vm_size_t size, void **ptr)
{
    *ptr = (void *)address;
    return 0;
}

static INLINE void
free_list_checksum(szone_t *szone, free_list_t *ptr, const char *msg)
{
    // We always checksum, as testing whether to do it (based on szone->debug_flags) is as fast as
    // doing it
    // XXX not necessarily true for LP64 case
    if (ptr->checksum != (((uintptr_t)ptr->previous) ^ ((uintptr_t)ptr->next) ^ CHECKSUM_MAGIC)) {
#if DEBUG_MALLOC
	malloc_printf("*** incorrect checksum: %s\n", msg);
#endif
	szone_error(szone, "incorrect checksum for freed object "
	  "- object was probably modified after being freed, break at szone_error to debug", ptr);
    }
}

static INLINE void
free_list_set_checksum(szone_t *szone, free_list_t *ptr)
{
    // We always set checksum, as testing whether to do it (based on
    // szone->debug_flags) is slower than just doing it
    // XXX not necessarily true for LP64 case
    ptr->checksum = ((uintptr_t)ptr->previous) ^ ((uintptr_t)ptr->next) ^ CHECKSUM_MAGIC;
}

static unsigned
free_list_count(const free_list_t *ptr)
{
    unsigned	count = 0;

    while (ptr) {
	count++;
	ptr = ptr->next;
    }
    return count;
}

/* XXX inconsistent use of BITMAP32 and BITARRAY operations could be cleaned up */

#define BITMAP32_SET(bitmap,bit) 	(bitmap |= 1 << (bit))
#define BITMAP32_CLR(bitmap,bit)	(bitmap &= ~ (1 << (bit)))
#define BITMAP32_BIT(bitmap,bit) 	((bitmap >> (bit)) & 1)

#define BITMAP32_FFS(bitmap) (ffs(bitmap))
    // returns bit # of first bit that's one, starting at 1 (returns 0 if !bitmap)

/*********************	TINY FREE LIST UTILITIES	************************/

// We encode the meta-headers as follows:
// Each quantum has an associated set of 2 bits:
// block_header when 1 says this block is the beginning of a block
// in_use when 1 says this block is in use
// so a block in use of size 3 is 1-1 0-X 0-X
// for a free block TINY_FREE_SIZE(ptr) carries the size and the bits are 1-0 X-X X-X
// for a block middle the bits are 0-0

// Attention double evaluation for these
#define BITARRAY_SET(bits,index)	(bits[index>>3] |= (1 << (index & 7)))
#define BITARRAY_CLR(bits,index)	(bits[index>>3] &= ~(1 << (index & 7)))
#define BITARRAY_BIT(bits,index)	(((bits[index>>3]) >> (index & 7)) & 1)

// Following is for start<8 and end<=start+32
#define BITARRAY_MCLR_LESS_32(bits,start,end)				\
do {									\
    unsigned char	*_bits = (bits);				\
    unsigned	_end = (end);						\
    switch (_end >> 3) {						\
	case 4: _bits[4] &= ~ ((1 << (_end - 32)) - 1); _end = 32;	\
	case 3: _bits[3] &= ~ ((1 << (_end - 24)) - 1); _end = 24;	\
	case 2: _bits[2] &= ~ ((1 << (_end - 16)) - 1); _end = 16;	\
	case 1: _bits[1] &= ~ ((1 << (_end - 8)) - 1); _end = 8;	\
	case 0: _bits[0] &= ~ ((1 << _end) - (1 << (start)));		\
    }									\
} while (0)

#if 0	// Simple but slow version
#warning Slow version in effect
#define BITARRAY_MCLR(bits,index,num)					\
do {									\
    unsigned	_ctr = (num);						\
    unsigned	_cur = (index);						\
									\
    while (_ctr--) {BITARRAY_CLR(bits,_cur); _cur++; }			\
} while (0)
#else

// Following is for num <= 32
#define BITARRAY_MCLR(bits,index,num)					\
do {									\
    unsigned		_index = (index);				\
    unsigned char	*_rebased = (bits) + (_index >> 3);		\
									\
    _index &= 7;							\
    BITARRAY_MCLR_LESS_32(_rebased, _index, _index + (num));		\
} while (0)
#endif

static INLINE msize_t
get_tiny_meta_header(const void *ptr, boolean_t *is_free)
{
    // returns msize and is_free
    // may return 0 for the msize component (meaning 65536)
    unsigned char	*block_header;
    unsigned char	*in_use;
    msize_t		index;
    unsigned		byte_index;

    block_header = TINY_BLOCK_HEADER_FOR_PTR(ptr);
    index = TINY_INDEX_FOR_PTR(ptr);
    byte_index = index >> 3;
      
    block_header += byte_index;
    index &= 7;
    *is_free = 0;
    if (!BITMAP32_BIT(*block_header, index))
	return 0;
    in_use = TINY_INUSE_FOR_HEADER(block_header);
    if (!BITMAP32_BIT(*in_use, index)) {
	*is_free = 1;
	return TINY_FREE_SIZE(ptr);
    }
    uint32_t	*addr = (uint32_t *)((uintptr_t)block_header & ~3);
    uint32_t	word0 = OSReadLittleInt32(addr, 0) >> index;
    uint32_t	word1 = OSReadLittleInt32(addr, 4) << (8 - index);
    uint32_t	bits = (((uintptr_t)block_header & 3) * 8);	// precision loss on LP64 OK here
    uint32_t	word = (word0 >> bits) | (word1 << (24 - bits));
    uint32_t	result = ffs(word >> 1);
    return result;
}

static INLINE void
set_tiny_meta_header_in_use(const void *ptr, msize_t msize)
{
    unsigned char	*block_header;
    unsigned char	*in_use;
    msize_t		index;
    unsigned		byte_index;
    msize_t		clr_msize;
    unsigned		end_bit;
	
    block_header = TINY_BLOCK_HEADER_FOR_PTR(ptr);
    index = TINY_INDEX_FOR_PTR(ptr);
    byte_index = index >> 3;
    
#if DEBUG_MALLOC
    if (msize >= 32)
	malloc_printf("set_tiny_meta_header_in_use() invariant broken %p %d\n", ptr, msize);
    if ((unsigned)index + (unsigned)msize > 0x10000)
	malloc_printf("set_tiny_meta_header_in_use() invariant broken (2) %p %d\n", ptr, msize);
#endif
    block_header += byte_index;
    index &= 7;
    BITMAP32_SET(*block_header, index); 
    in_use = TINY_INUSE_FOR_HEADER(block_header);
    BITMAP32_SET(*in_use, index); 
    index++; 
    clr_msize = msize-1;
    if (clr_msize) {
	byte_index = index >> 3;
	block_header += byte_index; in_use += byte_index;
	index &= 7;
	end_bit = index + clr_msize;
	BITARRAY_MCLR_LESS_32(block_header, index, end_bit);
	BITARRAY_MCLR_LESS_32(in_use, index, end_bit);
    }
    BITARRAY_SET(block_header, index+clr_msize); // we set the block_header bit for the following block to reaffirm next block is a block
#if DEBUG_MALLOC
    {
	boolean_t ff;
	msize_t	mf;
	
	mf = get_tiny_meta_header(ptr, &ff);
	if (msize != mf) {
	    malloc_printf("setting header for tiny in_use %p : %d\n", ptr, msize);
	    malloc_printf("reading header for tiny %p : %d %d\n", ptr, mf, ff);
	}
    }
#endif
}

static INLINE void
set_tiny_meta_header_middle(const void *ptr)
{
    // indicates this block is in the middle of an in use block
    unsigned char	*block_header;
    unsigned char	*in_use;
    msize_t		index;

    block_header = TINY_BLOCK_HEADER_FOR_PTR(ptr);
    in_use = TINY_INUSE_FOR_HEADER(block_header);
    index = TINY_INDEX_FOR_PTR(ptr);

    BITARRAY_CLR(block_header, index);
    BITARRAY_CLR(in_use, index); 
    TINY_FREE_SIZE(ptr) = 0;
}

static INLINE void
set_tiny_meta_header_free(const void *ptr, msize_t msize)
{
    // !msize is acceptable and means 65536
    unsigned char	*block_header;
    unsigned char	*in_use;
    msize_t		index;

    block_header = TINY_BLOCK_HEADER_FOR_PTR(ptr);
    in_use = TINY_INUSE_FOR_HEADER(block_header);
    index = TINY_INDEX_FOR_PTR(ptr);

#if DEBUG_MALLOC
    if ((unsigned)index + (unsigned)msize > 0x10000) {
	malloc_printf("setting header for tiny free %p msize too large: %d\n", ptr, msize);
    }
#endif
    BITARRAY_SET(block_header, index); BITARRAY_CLR(in_use, index); 
    TINY_FREE_SIZE(ptr) = msize;
    // mark the end of this block
    if (msize) {	// msize==0 => the whole region is free
	void	*follower = FOLLOWING_TINY_PTR(ptr, msize);
	TINY_PREVIOUS_MSIZE(follower) = msize;
    }
#if DEBUG_MALLOC
    boolean_t	ff;
    msize_t	mf = get_tiny_meta_header(ptr, &ff);
    if ((msize != mf) || !ff) {
	malloc_printf("setting header for tiny free %p : %d\n", ptr, msize);
	malloc_printf("reading header for tiny %p : %d %d\n", ptr, mf, ff);
    }
#endif
}

static INLINE boolean_t
tiny_meta_header_is_free(const void *ptr)
{
    // returns msize and is_free shifted by 16
    // may return 0 for the msize component (meaning 65536)
    unsigned char	*block_header;
    unsigned char	*in_use;
    msize_t		index;

    block_header = TINY_BLOCK_HEADER_FOR_PTR(ptr);
    in_use = TINY_INUSE_FOR_HEADER(block_header);
    index = TINY_INDEX_FOR_PTR(ptr);
    if (!BITARRAY_BIT(block_header, index))
	return 0;
    return !BITARRAY_BIT(in_use, index);
}

static INLINE void *
tiny_previous_preceding_free(void *ptr, msize_t *prev_msize)
{
    // returns the previous block, assuming and verifying it's free
    unsigned char	*block_header;
    unsigned char	*in_use;
    msize_t		index;
    msize_t		previous_msize;
    msize_t		previous_index;
    void		*previous_ptr;

    block_header = TINY_BLOCK_HEADER_FOR_PTR(ptr);
    in_use = TINY_INUSE_FOR_HEADER(block_header);
    index = TINY_INDEX_FOR_PTR(ptr);
    
    if (!index)
	return NULL;
    if ((previous_msize = TINY_PREVIOUS_MSIZE(ptr)) > index)
	return NULL;
    
    previous_index = index - previous_msize;
    previous_ptr = (void *)(TINY_REGION_FOR_PTR(ptr) + TINY_BYTES_FOR_MSIZE(previous_index));
    if (TINY_FREE_SIZE(previous_ptr) != previous_msize)
	return NULL;

    if (!BITARRAY_BIT(block_header, previous_index))
	return NULL;
    if (BITARRAY_BIT(in_use, previous_index))
	return NULL;
    
    // conservative check did match true check
    *prev_msize = previous_msize;
    return previous_ptr;
}

static INLINE void
tiny_free_list_add_ptr(szone_t *szone, void *ptr, msize_t msize)
{
    // Adds an item to the proper free list
    // Also marks the meta-header of the block properly
    // Assumes szone has been locked
    grain_t	slot = (!msize || (msize >= NUM_TINY_SLOTS)) ? NUM_TINY_SLOTS - 1 : msize - 1;
    free_list_t	*free_ptr = ptr;
    free_list_t	*free_head = szone->tiny_free_list[slot];

#if DEBUG_MALLOC
    if (LOG(szone,ptr)) {
	malloc_printf("in tiny_free_list_add_ptr(), ptr=%p, msize=%d\n", ptr, msize);
    }
    if (((unsigned)ptr) & (TINY_QUANTUM - 1)) {
	szone_error(szone, "tiny_free_list_add_ptr: Unaligned ptr", ptr);
    }
#endif
    set_tiny_meta_header_free(ptr, msize);
    if (free_head) {
	free_list_checksum(szone, free_head, __PRETTY_FUNCTION__);
#if DEBUG_MALLOC
	if (free_head->previous) {
	    malloc_printf("ptr=%p slot=%d free_head=%p previous=%p\n", ptr, slot, free_head, free_head->previous);
	    szone_error(szone, "tiny_free_list_add_ptr: Internal invariant broken (free_head->previous)", ptr);
	}
	if (! tiny_meta_header_is_free(free_head)) {
	    malloc_printf("ptr=%p slot=%d free_head=%p\n", ptr, slot, free_head);
	    szone_error(szone, "tiny_free_list_add_ptr: Internal invariant broken (free_head is not a free pointer)", ptr);
	}
#endif
	free_head->previous = free_ptr;
	free_list_set_checksum(szone, free_head);
    } else {
	BITMAP32_SET(szone->tiny_bitmap, slot);
    }
    free_ptr->previous = NULL;
    free_ptr->next = free_head;
    free_list_set_checksum(szone, free_ptr);
    szone->tiny_free_list[slot] = free_ptr;
}

static INLINE void
tiny_free_list_remove_ptr(szone_t *szone, void *ptr, msize_t msize)
{
    // Removes item in the proper free list
    // msize could be read, but all callers have it so we pass it in
    // Assumes szone has been locked
    grain_t	slot = (!msize || (msize >= NUM_TINY_SLOTS)) ? NUM_TINY_SLOTS - 1 : msize - 1;
    free_list_t	*free_ptr = ptr;
    free_list_t	*next = free_ptr->next;
    free_list_t	*previous = free_ptr->previous;

#if DEBUG_MALLOC
    if (LOG(szone,ptr)) {
	malloc_printf("In tiny_free_list_remove_ptr(), ptr=%p, msize=%d\n", ptr, msize);
    }
#endif
    free_list_checksum(szone, free_ptr, __PRETTY_FUNCTION__);
    if (!previous) {
	// The block to remove is the head of the free list
#if DEBUG_MALLOC
	if (szone->tiny_free_list[slot] != ptr) {
	    malloc_printf("ptr=%p slot=%d msize=%d szone->tiny_free_list[slot]=%p\n",
	      ptr, slot, msize, szone->tiny_free_list[slot]);
	    szone_error(szone, "tiny_free_list_remove_ptr: Internal invariant broken (szone->tiny_free_list[slot])", ptr);
	    return;
	}
#endif
	szone->tiny_free_list[slot] = next;
	if (!next) BITMAP32_CLR(szone->tiny_bitmap, slot);
    } else {
	previous->next = next;
	free_list_set_checksum(szone, previous);
    }
    if (next) {
	next->previous = previous;
	free_list_set_checksum(szone, next);
    }
}

/*
 * Find the tiny region containing (ptr) (if any).
 *
 * We take advantage of the knowledge that tiny regions are always (1 << TINY_BLOCKS_ALIGN) aligned.
 */
static INLINE tiny_region_t *
tiny_region_for_ptr_no_lock(szone_t *szone, const void *ptr)
{
    tiny_region_t *region;
    tiny_region_t rbase;
    int	i;
    
    /* mask off irrelevant lower bits */
    rbase = TINY_REGION_FOR_PTR(ptr);
    /* iterate over allocated regions - XXX not terribly efficient for large number of regions */
    for (i = szone->num_tiny_regions, region = szone->tiny_regions; i > 0; i--, region++)
	if (rbase == *region)
	    return(region);
    return(NULL);
}

static INLINE void
tiny_free_no_lock(szone_t *szone, tiny_region_t *region, void *ptr, msize_t msize)
{
    size_t	original_size = TINY_BYTES_FOR_MSIZE(msize);
    void	*next_block = ((char *)ptr + original_size);
    msize_t	previous_msize;
    void	*previous;
    msize_t	next_msize;
    free_list_t	*big_free_block;
    free_list_t	*after_next_block;
    free_list_t	*before_next_block;

#if DEBUG_MALLOC
    if (LOG(szone,ptr)) {
	malloc_printf("in tiny_free_no_lock(), ptr=%p, msize=%d\n", ptr, msize);
    }
    if (! msize) {
	malloc_printf("in tiny_free_no_lock(), ptr=%p, msize=%d\n", ptr, msize);
	szone_error(szone, "trying to free tiny block that is too small", ptr);
    }
#endif
    // We try to coalesce this block with the preceeding one
    previous = tiny_previous_preceding_free(ptr, &previous_msize);
    if (previous) {
#if DEBUG_MALLOC
	if (LOG(szone, ptr) || LOG(szone,previous)) { 
	    malloc_printf("in tiny_free_no_lock(), coalesced backwards for %p previous=%p\n", ptr, previous);
	}
#endif
	tiny_free_list_remove_ptr(szone, previous, previous_msize);
	ptr = previous;
	msize += previous_msize;
    }
    // We try to coalesce with the next block
    if ((next_block < TINY_REGION_END(*region)) && tiny_meta_header_is_free(next_block)) {
	// The next block is free, we coalesce
	next_msize = TINY_FREE_SIZE(next_block);
#if DEBUG_MALLOC
	if (LOG(szone, ptr) || LOG(szone, next_block)) {
	    malloc_printf("in tiny_free_no_lock(), for ptr=%p, msize=%d coalesced forward=%p next_msize=%d\n",
	      ptr, msize, next_block, next_msize);
	}
#endif
	if (next_msize >= NUM_TINY_SLOTS) {
	    // we take a short cut here to avoid removing next_block from the slot 31 freelist and then adding ptr back to slot 31
	    msize += next_msize;
	    big_free_block = (free_list_t *)next_block;
	    after_next_block = big_free_block->next;
	    before_next_block = big_free_block->previous;
	    free_list_checksum(szone, big_free_block, __PRETTY_FUNCTION__);
	    if (!before_next_block) {
		szone->tiny_free_list[NUM_TINY_SLOTS-1] = ptr;
	    } else {
		before_next_block->next = ptr;
		free_list_set_checksum(szone, before_next_block);
	    }
	    if (after_next_block) {
		after_next_block->previous = ptr;
		free_list_set_checksum(szone, after_next_block);
	    }
	    ((free_list_t *)ptr)->previous = before_next_block;
	    ((free_list_t *)ptr)->next = after_next_block;
	    free_list_set_checksum(szone, ptr);
	    set_tiny_meta_header_free(ptr, msize);
	    set_tiny_meta_header_middle(big_free_block); // clear the meta_header to enable coalescing backwards
	    goto tiny_free_ending;
	}
	tiny_free_list_remove_ptr(szone, next_block, next_msize);
	set_tiny_meta_header_middle(next_block); // clear the meta_header to enable coalescing backwards
	msize += next_msize;
    }
    if ((szone->debug_flags & SCALABLE_MALLOC_DO_SCRIBBLE) && msize) {
	memset(ptr, 0x55, TINY_BYTES_FOR_MSIZE(msize));
    }
    tiny_free_list_add_ptr(szone, ptr, msize);
  tiny_free_ending:
    // When in proper debug mode we write on the memory to help debug memory smashers
    szone->num_tiny_objects--;
    szone->num_bytes_in_tiny_objects -= original_size; // we use original_size and not msize to avoid double counting the coalesced blocks
}

static void *
tiny_malloc_from_region_no_lock(szone_t *szone, msize_t msize)
{
    tiny_region_t	last_region, *new_regions;
    void		*last_block, *ptr, *aligned_address;

    // Allocates from the last region or a freshly allocated region
    // Before anything we transform the tiny_bytes_free_at_end - if any - to a regular free block
    if (szone->tiny_bytes_free_at_end) {
	last_region = szone->tiny_regions[szone->num_tiny_regions-1];
	last_block = TINY_REGION_END(last_region) - szone->tiny_bytes_free_at_end;
	tiny_free_list_add_ptr(szone, last_block, TINY_MSIZE_FOR_BYTES(szone->tiny_bytes_free_at_end));
	szone->tiny_bytes_free_at_end = 0;
    }
    // time to create a new region
    aligned_address = allocate_pages(szone, TINY_REGION_SIZE, TINY_BLOCKS_ALIGN, 0, VM_MAKE_TAG(VM_MEMORY_MALLOC_TINY));
    if (!aligned_address) {
	// out of memory!
	return NULL;
    }
    // We set the padding after block_header to be all 1
    ((uint32_t *)(aligned_address + (1 << TINY_BLOCKS_ALIGN) + (NUM_TINY_BLOCKS >> 3)))[0] = ~0;
    if (szone->num_tiny_regions == INITIAL_NUM_TINY_REGIONS) {
	// XXX logic here fails after initial reallocation of tiny regions is exhausted (approx 4GB of
	// tiny allocations)
	new_regions = small_malloc_from_region_no_lock(szone, 16); // 16 * 512 bytes is plenty of tiny regions (more than 4,000)
	if (!new_regions) return NULL;
	memcpy(new_regions, szone->tiny_regions, INITIAL_NUM_TINY_REGIONS * sizeof(tiny_region_t));
	szone->tiny_regions = new_regions; // we set the pointer after it's all ready to enable enumeration from another thread without locking
    }
    szone->tiny_regions[szone->num_tiny_regions] = aligned_address;
    szone->num_tiny_regions ++; // we set the number after the pointer is all ready to enable enumeration from another thread without taking the lock
    ptr = aligned_address; 
    set_tiny_meta_header_in_use(ptr, msize);
    szone->num_tiny_objects++;
    szone->num_bytes_in_tiny_objects += TINY_BYTES_FOR_MSIZE(msize);
    // We put a header on the last block so that it appears in use (for coalescing, etc...)
    set_tiny_meta_header_in_use(ptr + TINY_BYTES_FOR_MSIZE(msize), 1);
    szone->tiny_bytes_free_at_end = TINY_BYTES_FOR_MSIZE(NUM_TINY_BLOCKS - msize);
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) {
	malloc_printf("in tiny_malloc_from_region_no_lock(), ptr=%p, msize=%d\n", ptr, msize);
    }
#endif
    return ptr;
}

static INLINE boolean_t
try_realloc_tiny_in_place(szone_t *szone, void *ptr, size_t old_size, size_t new_size)
{
    // returns 1 on success
    msize_t	index;
    msize_t	old_msize;
    unsigned	next_index;
    void	*next_block;
    boolean_t	is_free;
    msize_t	next_msize, coalesced_msize, leftover_msize;
    void	*leftover;

    index = TINY_INDEX_FOR_PTR(ptr);
    old_msize = TINY_MSIZE_FOR_BYTES(old_size);
    next_index = index + old_msize;
    
    if (next_index >= NUM_TINY_BLOCKS) {
	return 0;
    }
    next_block = (char *)ptr + old_size;
    SZONE_LOCK(szone);
    is_free = tiny_meta_header_is_free(next_block);
    if (!is_free) {
	SZONE_UNLOCK(szone);
	return 0; // next_block is in use;
    }
    next_msize = TINY_FREE_SIZE(next_block);
    if (old_size + TINY_MSIZE_FOR_BYTES(next_msize) < new_size) {
	SZONE_UNLOCK(szone);
	return 0; // even with next block, not enough
    }
    tiny_free_list_remove_ptr(szone, next_block, next_msize);
    set_tiny_meta_header_middle(next_block); // clear the meta_header to enable coalescing backwards
    coalesced_msize = TINY_MSIZE_FOR_BYTES(new_size - old_size + TINY_QUANTUM - 1);
    leftover_msize = next_msize - coalesced_msize;
    if (leftover_msize) {
	leftover = next_block + TINY_BYTES_FOR_MSIZE(coalesced_msize);
	tiny_free_list_add_ptr(szone, leftover, leftover_msize);
    }
    set_tiny_meta_header_in_use(ptr, old_msize + coalesced_msize);
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) {
	malloc_printf("in try_realloc_tiny_in_place(), ptr=%p, msize=%d\n", ptr, old_msize + coalesced_msize);
    }
#endif
    szone->num_bytes_in_tiny_objects += TINY_BYTES_FOR_MSIZE(coalesced_msize);
    SZONE_UNLOCK(szone);
    CHECK(szone, __PRETTY_FUNCTION__);
    return 1;
}

static boolean_t
tiny_check_region(szone_t *szone, tiny_region_t *region)
{
    uintptr_t		start, ptr, region_end, follower;
    boolean_t		prev_free = 0;
    boolean_t		is_free;
    msize_t		msize;
    free_list_t		*free_head;

    /* establish region limits */
    start = (uintptr_t)TINY_REGION_ADDRESS(*region);
    ptr = start;
    region_end = (uintptr_t)TINY_REGION_END(*region);

    /*
     * The last region may have a trailing chunk which has not been converted into inuse/freelist
     * blocks yet.
     */
    if (region == szone->tiny_regions + szone->num_tiny_regions - 1)
	    region_end -= szone->tiny_bytes_free_at_end;


    /*
     * Scan blocks within the region.
     */
    while (ptr < region_end) {
	/*
	 * If the first block is free, and its size is 65536 (msize = 0) then the entire region is
	 * free.
	 */
	msize = get_tiny_meta_header((void *)ptr, &is_free);
	if (is_free && !msize && (ptr == start)) {
	    return 1;
	}

	/*
	 * If the block's size is 65536 (msize = 0) then since we're not the first entry the size is
	 * corrupt.
	 */
	if (!msize) {
	    malloc_printf("*** invariant broken for tiny block %p this msize=%d - size is too small\n",
	      ptr, msize);
	    return 0;
	}

	if (!is_free) {
	    /*
	     * In use blocks cannot be more than 31 quanta large.
	     */
	    prev_free = 0;
	    if (msize > 31 * TINY_QUANTUM) {
		malloc_printf("*** invariant broken for %p this tiny msize=%d[%p] - size is too large\n",
		  ptr, msize, msize);
		return 0;
	    }
	    /* move to next block */
	    ptr += TINY_BYTES_FOR_MSIZE(msize);
	} else {
	    /*
	     * Free blocks must have been coalesced, we cannot have a free block following another
	     * free block.
	     */
	    if (prev_free) {
		malloc_printf("*** invariant broken for free block %p this tiny msize=%d: two free blocks in a row\n",
		  ptr, msize);
		return 0;
	    }
	    prev_free = 1;
	    /*
	     * Check the integrity of this block's entry in its freelist.
	     */
	    free_head = (free_list_t *)ptr;
	    free_list_checksum(szone, free_head, __PRETTY_FUNCTION__);
	    if (free_head->previous && !tiny_meta_header_is_free(free_head->previous)) {
		malloc_printf("*** invariant broken for %p (previous %p is not a free pointer)\n",
		  ptr, free_head->previous);
		return 0;
	    }
	    if (free_head->next && !tiny_meta_header_is_free(free_head->next)) {
		malloc_printf("*** invariant broken for %p (next in free list %p is not a free pointer)\n",
		  ptr, free_head->next);
		return 0;
	    }
	    /*
	     * Check the free block's trailing size value.
	     */
	    follower = (uintptr_t)FOLLOWING_TINY_PTR(ptr, msize);
	    if ((follower != region_end) && (TINY_PREVIOUS_MSIZE(follower) != msize)) {
		malloc_printf("*** invariant broken for tiny free %p followed by %p in region [%p-%p] "
		  "(end marker incorrect) should be %d; in fact %d\n",
		  ptr, follower, TINY_REGION_ADDRESS(*region), region_end, msize, TINY_PREVIOUS_MSIZE(follower));
		return 0;
	    }
	    /* move to next block */
	    ptr = follower;
	}
    }
    /*
     * Ensure that we scanned the entire region
     */
    if (ptr != region_end) {
	malloc_printf("*** invariant broken for region end %p - %p\n", ptr, region_end);
	return 0;
    }
    /*
     * Check the trailing block's integrity.
     */
    if (region == szone->tiny_regions + szone->num_tiny_regions - 1) {
	if (szone->tiny_bytes_free_at_end) {
	    msize = get_tiny_meta_header((void *)ptr, &is_free);
	    if (is_free || (msize != 1)) {
		malloc_printf("*** invariant broken for blocker block %p - %d %d\n", ptr, msize, is_free);
	    }
	}
    }
    return 1;
}

static kern_return_t
tiny_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t region_address, unsigned short num_regions, size_t tiny_bytes_free_at_end, memory_reader_t reader, vm_range_recorder_t recorder)
{
    tiny_region_t	*regions;
    unsigned		index = 0;
    vm_range_t		buffer[MAX_RECORDER_BUFFER];
    unsigned		count = 0;
    kern_return_t	err;
    tiny_region_t	region;
    vm_range_t		range;
    vm_range_t		admin_range;
    vm_range_t		ptr_range;
    unsigned char	*mapped_region;
    unsigned char	*block_header;
    unsigned char	*in_use;
    unsigned		block_index;
    unsigned		block_limit;
    boolean_t		is_free;
    msize_t		msize;
    void		*mapped_ptr;
    unsigned 		bit;
    
    err = reader(task, region_address, sizeof(tiny_region_t) * num_regions, (void **)&regions);
    if (err) return err;
    while (index < num_regions) {
	// unsigned		num_in_use = 0;
	// unsigned		num_free = 0;
	region = regions[index];
	range.address = (vm_address_t)TINY_REGION_ADDRESS(region);
	range.size = (vm_size_t)TINY_REGION_SIZE;
	if (type_mask & MALLOC_ADMIN_REGION_RANGE_TYPE) {
	    admin_range.address = range.address + (1 << TINY_BLOCKS_ALIGN);
	    admin_range.size = range.size - (1 << TINY_BLOCKS_ALIGN);
	    recorder(task, context, MALLOC_ADMIN_REGION_RANGE_TYPE, &admin_range, 1);
	}
	if (type_mask & (MALLOC_PTR_REGION_RANGE_TYPE | MALLOC_ADMIN_REGION_RANGE_TYPE)) {
	    ptr_range.address = range.address;
	    ptr_range.size = 1 << TINY_BLOCKS_ALIGN;
	    recorder(task, context, MALLOC_PTR_REGION_RANGE_TYPE, &ptr_range, 1);
	}
	if (type_mask & MALLOC_PTR_IN_USE_RANGE_TYPE) {
	    err = reader(task, range.address, range.size, (void **)&mapped_region);
	    if (err)
		return err;
	    block_header = (unsigned char *)(mapped_region + (1 << TINY_BLOCKS_ALIGN));
	    in_use = block_header + (NUM_TINY_BLOCKS >> 3) + 4;
	    block_index = 0;
	    block_limit = NUM_TINY_BLOCKS;
	    if (index == num_regions - 1)
		block_limit -= TINY_MSIZE_FOR_BYTES(tiny_bytes_free_at_end);
	    while (block_index < block_limit) {
		is_free = !BITARRAY_BIT(in_use, block_index);
		if (is_free) {
		    mapped_ptr = mapped_region + TINY_BYTES_FOR_MSIZE(block_index);
		    msize = TINY_FREE_SIZE(mapped_ptr);
		    if (!msize)
			break;
		} else {
		    msize = 1; 
		    bit = block_index + 1;
		    while (! BITARRAY_BIT(block_header, bit)) {
			bit++;
			msize ++;
		    }
		    buffer[count].address = range.address + TINY_BYTES_FOR_MSIZE(block_index);
		    buffer[count].size = TINY_BYTES_FOR_MSIZE(msize);
		    count++;
		    if (count >= MAX_RECORDER_BUFFER) {
			recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE, buffer, count);
			count = 0;
		    }
		}
		block_index += msize;
	    }
	}
	index++;
    }
    if (count) {
	recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE, buffer, count);
    }
    return 0;
}

static INLINE void *
tiny_malloc_from_free_list(szone_t *szone, msize_t msize)
{
    // Assumes we've locked the region
    free_list_t		*ptr;
    msize_t		this_msize;
    grain_t		slot = msize - 1;
    free_list_t		**free_list = szone->tiny_free_list;
    free_list_t		**the_slot = free_list + slot;
    free_list_t		*next;
    free_list_t		**limit;
    unsigned		bitmap;
    msize_t		leftover_msize;
    free_list_t		*leftover_ptr;

    /*
     * Look for an exact match by checking the freelist for this msize.
     */
    ptr = *the_slot;
    if (ptr) {
	next = ptr->next;
	if (next) {
	    next->previous = NULL;
	    free_list_set_checksum(szone, next);
	}
	*the_slot = next;
	this_msize = msize;
#if DEBUG_MALLOC
	if (LOG(szone, ptr)) {
	    malloc_printf("in tiny_malloc_from_free_list(), exact match ptr=%p, this_msize=%d\n", ptr, this_msize);
	}
#endif
	goto return_tiny_alloc;
    }

    /*
     * Iterate over freelists for larger blocks looking for the next-largest block.
     */
    bitmap = szone->tiny_bitmap & ~ ((1 << slot) - 1);
    if (!bitmap)
	goto try_tiny_malloc_from_end;
    slot = BITMAP32_FFS(bitmap) - 1;
    limit = free_list + NUM_TINY_SLOTS - 1;
    free_list += slot;
    while (free_list < limit) {
	// try bigger grains
	ptr = *free_list;
	if (ptr) {
	    next = ptr->next;
	    if (next) {
		next->previous = NULL;
		free_list_set_checksum(szone, next);
	    }
	    *free_list = next;
	    this_msize = TINY_FREE_SIZE(ptr);
#if DEBUG_MALLOC
	    if (LOG(szone, ptr)) {
		malloc_printf("in tiny_malloc_from_free_list(), bigger grain ptr=%p, msize=%d this_msize=%d\n", ptr, msize, this_msize);
	    }
#endif
	    goto add_leftover_and_proceed;
	}
	free_list++;
    }
    // we are now looking at the last slot (31)
    ptr = *limit;
    if (ptr) {
	this_msize = TINY_FREE_SIZE(ptr);
	next = ptr->next;
	if (this_msize - msize >= NUM_TINY_SLOTS) {
	    // the leftover will go back to the free list, so we optimize by modifying the free list rather than removing the head and then adding back
	    leftover_msize = this_msize - msize;
	    leftover_ptr = (free_list_t *)((unsigned char *)ptr + TINY_BYTES_FOR_MSIZE(msize));
	    *limit = leftover_ptr;
	    if (next) {
		next->previous = leftover_ptr;
		free_list_set_checksum(szone, next);
	    }
	    leftover_ptr->next = next;
	    leftover_ptr->previous = NULL;
	    free_list_set_checksum(szone, leftover_ptr);
	    set_tiny_meta_header_free(leftover_ptr, leftover_msize);
#if DEBUG_MALLOC
	    if (LOG(szone,ptr)) {
		malloc_printf("in tiny_malloc_from_free_list(), last slot ptr=%p, msize=%d this_msize=%d\n", ptr, msize, this_msize);
	    }
#endif
	    this_msize = msize;
	    goto return_tiny_alloc;
	}
	*limit = next;
	if (next) {
	    next->previous = NULL;
	    free_list_set_checksum(szone, next);
	}
	goto add_leftover_and_proceed;
    }
try_tiny_malloc_from_end:
    // Let's see if we can use szone->tiny_bytes_free_at_end
    if (szone->tiny_bytes_free_at_end >= TINY_BYTES_FOR_MSIZE(msize)) {
	ptr = (free_list_t *)(TINY_REGION_END(szone->tiny_regions[szone->num_tiny_regions-1]) - szone->tiny_bytes_free_at_end);
	szone->tiny_bytes_free_at_end -= TINY_BYTES_FOR_MSIZE(msize);
	if (szone->tiny_bytes_free_at_end) {
	    // let's add an in use block after ptr to serve as boundary
	    set_tiny_meta_header_in_use((unsigned char *)ptr + TINY_BYTES_FOR_MSIZE(msize), 1);
	}
	this_msize = msize;
#if DEBUG_MALLOC
	if (LOG(szone, ptr)) {
	    malloc_printf("in tiny_malloc_from_free_list(), from end ptr=%p, msize=%d\n", ptr, msize);
	}
#endif
	goto return_tiny_alloc;
    }
    return NULL;
add_leftover_and_proceed:
    if (!this_msize || (this_msize > msize)) {
	leftover_msize = this_msize - msize;
	leftover_ptr = (free_list_t *)((unsigned char *)ptr + TINY_BYTES_FOR_MSIZE(msize));
#if DEBUG_MALLOC
	if (LOG(szone,ptr)) {
	    malloc_printf("in tiny_malloc_from_free_list(), adding leftover ptr=%p, this_msize=%d\n", ptr, this_msize);
	}
#endif
	tiny_free_list_add_ptr(szone, leftover_ptr, leftover_msize);
	this_msize = msize;
    }
return_tiny_alloc:
    szone->num_tiny_objects++;
    szone->num_bytes_in_tiny_objects += TINY_BYTES_FOR_MSIZE(this_msize);
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) {
	malloc_printf("in tiny_malloc_from_free_list(), ptr=%p, this_msize=%d, msize=%d\n", ptr, this_msize, msize);
    }
#endif
    set_tiny_meta_header_in_use(ptr, this_msize);
    return ptr;
}

static INLINE void *
tiny_malloc_should_clear(szone_t *szone, msize_t msize, boolean_t cleared_requested)
{
    boolean_t	locked = 0;
    void	*ptr;

#if DEBUG_MALLOC
    if (!msize) {
	szone_error(szone, "invariant broken (!msize) in allocation (region)", NULL);
	return(NULL);
    }
#endif
#if TINY_CACHE
    ptr = szone->last_tiny_free;
    if ((((uintptr_t)ptr) & (TINY_QUANTUM - 1)) == msize) {
	// we have a candidate - let's lock to make sure
	LOCK_AND_NOTE_LOCKED(szone, locked);
	if (ptr == szone->last_tiny_free) {
	    szone->last_tiny_free = NULL;
	    SZONE_UNLOCK(szone);
	    CHECK(szone, __PRETTY_FUNCTION__);
	    ptr = (void *)((uintptr_t)ptr & ~ (TINY_QUANTUM - 1));
	    if (cleared_requested) {
		memset(ptr, 0, TINY_BYTES_FOR_MSIZE(msize));
	    }
#if DEBUG_MALLOC
	    if (LOG(szone,ptr)) {
		malloc_printf("in tiny_malloc_should_clear(), tiny cache ptr=%p, msize=%d\n", ptr, msize);
	    }
#endif
	    return ptr;
	}
    }
#endif
    // Except in rare occasions where we need to add a new region, we are going to end up locking, so we might as well lock right away to avoid doing unnecessary optimistic probes
    if (!locked) LOCK_AND_NOTE_LOCKED(szone, locked);
    ptr = tiny_malloc_from_free_list(szone, msize);
    if (ptr) {
	SZONE_UNLOCK(szone);
	CHECK(szone, __PRETTY_FUNCTION__);
	if (cleared_requested) {
	    memset(ptr, 0, TINY_BYTES_FOR_MSIZE(msize));
	}
	return ptr;
    }
    ptr = tiny_malloc_from_region_no_lock(szone, msize);
    // we don't clear because this freshly allocated space is pristine
    SZONE_UNLOCK(szone);
    CHECK(szone, __PRETTY_FUNCTION__);
    return ptr;
}

static INLINE void
free_tiny(szone_t *szone, void *ptr, tiny_region_t *tiny_region)
{
    msize_t	msize;
    boolean_t	is_free;
#if TINY_CACHE
    void *ptr2;
#endif

    // ptr is known to be in tiny_region
    SZONE_LOCK(szone);
#if TINY_CACHE
    ptr2 = szone->last_tiny_free;
    /* check that we don't already have this pointer in the cache */
    if (ptr == (void *)((uintptr_t)ptr2 & ~ (TINY_QUANTUM - 1))) {
	szone_error(szone, "double free", ptr);
	return;
    }
#endif /* TINY_CACHE */
    msize = get_tiny_meta_header(ptr, &is_free);
    if (is_free) {
	szone_error(szone, "double free", ptr);
	return;
    }
#if DEBUG_MALLOC
    if (!msize) {
	malloc_printf("*** szone_free() block in use is too large: %p\n", ptr);
	return;
    }
#endif
#if TINY_CACHE
    if (msize < TINY_QUANTUM) {	// to see if the bits fit in the last 4 bits
	szone->last_tiny_free = (void *)(((uintptr_t)ptr) | msize);
	if (!ptr2) {
	    SZONE_UNLOCK(szone);
	    CHECK(szone, __PRETTY_FUNCTION__);
	    return;
	}
	msize = (uintptr_t)ptr2 & (TINY_QUANTUM - 1);
	ptr = (void *)(((uintptr_t)ptr2) & ~(TINY_QUANTUM - 1));
	tiny_region = tiny_region_for_ptr_no_lock(szone, ptr);
	if (!tiny_region) {
	    szone_error(szone, "double free (tiny cache)", ptr);
	}
    }
#endif
    tiny_free_no_lock(szone, tiny_region, ptr, msize);
    SZONE_UNLOCK(szone);
    CHECK(szone, __PRETTY_FUNCTION__);
}

static void
print_tiny_free_list(szone_t *szone)
{
    grain_t	slot = 0;
    free_list_t	*ptr;

    malloc_printf("tiny free sizes: ");
    while (slot < NUM_TINY_SLOTS) {
	ptr = szone->tiny_free_list[slot];
	if (ptr) {
	    malloc_printf("%s%y[%d]; ", (slot == NUM_TINY_SLOTS-1) ? ">=" : "", (slot+1)*TINY_QUANTUM, free_list_count(ptr));
	}
	slot++;
    }
}

static void
print_tiny_region(boolean_t verbose, tiny_region_t region, size_t bytes_at_end)
{
    unsigned	counts[1024];
    unsigned	in_use = 0;
    uintptr_t	start = (uintptr_t)TINY_REGION_ADDRESS(region);
    uintptr_t	current = start;
    uintptr_t	limit =  (uintptr_t)TINY_REGION_END(region) - bytes_at_end;
    boolean_t	is_free;
    msize_t	msize;
    unsigned	ci;
    
    memset(counts, 0, 1024 * sizeof(unsigned));
    while (current < limit) {
	msize = get_tiny_meta_header((void *)current, &is_free);
	if (is_free & !msize && (current == start)) {
	    // first block is all free
	    break;
	}
	if (!msize) {
	    malloc_printf("*** error with %p: msize=%d\n", (void *)current, (unsigned)msize);
	    break;
	}
	if (!is_free) {
	    // block in use
	    if (msize > 32)
		malloc_printf("*** error at %p msize for in_use is %d\n", (void *)current, msize);
	    if (msize < 1024)
		counts[msize]++;
	    in_use++;
	}
	current += TINY_BYTES_FOR_MSIZE(msize);
    }
    malloc_printf("Tiny region [%p-%p, %y]\t", (void *)start, TINY_REGION_END(region), (int)TINY_REGION_SIZE);
    malloc_printf("In_use=%d ", in_use);
    if (bytes_at_end) malloc_printf("untouched=%y ", bytes_at_end);
    if (verbose && in_use) {
	malloc_printf("\tSizes in use: ");
	for (ci = 0; ci < 1024; ci++)
	    if (counts[ci])
		malloc_printf("%d[%d]", TINY_BYTES_FOR_MSIZE(ci), counts[ci]);
    }
    malloc_printf("\n");
}

static boolean_t
tiny_free_list_check(szone_t *szone, grain_t slot)
{
    unsigned	count = 0;
    free_list_t	*ptr = szone->tiny_free_list[slot];
    free_list_t	*previous = NULL;
    boolean_t	is_free;

    CHECK_LOCKED(szone, __PRETTY_FUNCTION__);
    while (ptr) {
	free_list_checksum(szone, ptr, __PRETTY_FUNCTION__);
	is_free = tiny_meta_header_is_free(ptr);
	if (! is_free) {
	    malloc_printf("*** in-use ptr in free list slot=%d count=%d ptr=%p\n", slot, count, ptr);
	    return 0;
	}
	if (((uintptr_t)ptr) & (TINY_QUANTUM - 1)) {
	    malloc_printf("*** inaligned ptr in free list slot=%d  count=%d ptr=%p\n", slot, count, ptr);
	    return 0;
	}
	if (!tiny_region_for_ptr_no_lock(szone, ptr)) {
	    malloc_printf("*** itr not in szone slot=%d  count=%d ptr=%p\n", slot, count, ptr);
	    return 0;
	}
	if (ptr->previous != previous) {
	    malloc_printf("*** irevious incorrectly set slot=%d  count=%d ptr=%p\n", slot, count, ptr);
	    return 0;
	}
	previous = ptr;
	ptr = ptr->next;
	count++;
    }
    return 1;
}

/*********************	SMALL FREE LIST UTILITIES	************************/

/*
 * Mark a block as free.  Only the first quantum of a block is marked thusly,
 * the remainder are marked "middle".
 */
static INLINE void
small_meta_header_set_is_free(msize_t *meta_headers, unsigned index, msize_t msize)
{

    meta_headers[index] = msize | SMALL_IS_FREE;
}

/*
 * Mark a block as in use.  Only the first quantum of a block is marked thusly,
 * the remainder are marked "middle".
 */
static INLINE void
small_meta_header_set_in_use(msize_t *meta_headers, msize_t index, msize_t msize)
{

    meta_headers[index] = msize;
}

/*
 * Mark a quantum as being the second or later in a block.
 */
static INLINE void
small_meta_header_set_middle(msize_t *meta_headers, msize_t index)
{

    meta_headers[index] = 0;
}

// Adds an item to the proper free list
// Also marks the header of the block properly
// Assumes szone has been locked    
static void
small_free_list_add_ptr(szone_t *szone, void *ptr, msize_t msize)
{
    grain_t	grain = (msize <= NUM_SMALL_SLOTS) ? msize - 1 : NUM_SMALL_SLOTS - 1;
    free_list_t	*free_ptr = ptr;
    free_list_t	*free_head = szone->small_free_list[grain];
    void	*follower;

    CHECK_LOCKED(szone, __PRETTY_FUNCTION__);
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) {
	malloc_printf("in small_free_list_add_ptr(), ptr=%p, msize=%d\n", ptr, msize);
    }
    if (((uintptr_t)ptr) & (SMALL_QUANTUM - 1)) {
	szone_error(szone, "small_free_list_add_ptr: Unaligned ptr", ptr);
    }
#endif
    if (free_head) {
	free_list_checksum(szone, free_head, __PRETTY_FUNCTION__);
#if DEBUG_MALLOC
	if (free_head->previous) {
	    malloc_printf("ptr=%p grain=%d free_head=%p previous=%p\n",
	      ptr, grain, free_head, free_head->previous);
	    szone_error(szone, "small_free_list_add_ptr: Internal invariant broken (free_head->previous)", ptr);
	}
	if (!SMALL_PTR_IS_FREE(free_head)) {
	    malloc_printf("ptr=%p grain=%d free_head=%p\n", ptr, grain, free_head);
	    szone_error(szone, "small_free_list_add_ptr: Internal invariant broken (free_head is not a free pointer)", ptr);
	}
#endif
	free_head->previous = free_ptr;
	free_list_set_checksum(szone, free_head);
    } else {
	BITMAP32_SET(szone->small_bitmap, grain);
    }
    free_ptr->previous = NULL;
    free_ptr->next = free_head;
    free_list_set_checksum(szone, free_ptr);
    szone->small_free_list[grain] = free_ptr;
    follower = ptr + SMALL_BYTES_FOR_MSIZE(msize);
    SMALL_PREVIOUS_MSIZE(follower) = msize;
}

// Removes item in the proper free list
// msize could be read, but all callers have it so we pass it in
// Assumes szone has been locked
static void
small_free_list_remove_ptr(szone_t *szone, void *ptr, msize_t msize)
{
    grain_t	grain = (msize <= NUM_SMALL_SLOTS) ? msize - 1 : NUM_SMALL_SLOTS - 1;
    free_list_t	*free_ptr = ptr;
    free_list_t	*next = free_ptr->next;
    free_list_t	*previous = free_ptr->previous;

    CHECK_LOCKED(szone, __PRETTY_FUNCTION__);
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) {
	malloc_printf("in small_free_list_remove_ptr(), ptr=%p, msize=%d\n", ptr, msize);
    }
#endif
    free_list_checksum(szone, free_ptr, __PRETTY_FUNCTION__);
    if (!previous) {
#if DEBUG_MALLOC
	if (szone->small_free_list[grain] != ptr) {
	    malloc_printf("ptr=%p grain=%d msize=%d szone->small_free_list[grain]=%p\n",
	      ptr, grain, msize, szone->small_free_list[grain]);
	    szone_error(szone, "small_free_list_remove_ptr: Internal invariant broken (szone->small_free_list[grain])", ptr);
	    return;
	}
#endif
	szone->small_free_list[grain] = next;
	if (!next) BITMAP32_CLR(szone->small_bitmap, grain);
    } else {
	previous->next = next;
	free_list_set_checksum(szone, previous);
    }
    if (next) {
	next->previous = previous;
	free_list_set_checksum(szone, next);
    }
}

static INLINE small_region_t *
small_region_for_ptr_no_lock(szone_t *szone, const void *ptr)
{
    small_region_t	*region;
    small_region_t	rbase;
    int			i;
    
    /* find assumed heap/region base */
    rbase = SMALL_REGION_FOR_PTR(ptr);

    /* scan existing regions for a match */
    for (i = szone->num_small_regions, region = szone->small_regions; i > 0; i--, region++)
	if (rbase == *region)
	    return(region);
    return(NULL);
}

static INLINE void
small_free_no_lock(szone_t *szone, small_region_t *region, void *ptr, msize_t msize)
{
    msize_t		*meta_headers = SMALL_META_HEADER_FOR_PTR(ptr);
    unsigned		index = SMALL_META_INDEX_FOR_PTR(ptr);
    size_t		original_size = SMALL_BYTES_FOR_MSIZE(msize);
    unsigned char	*next_block = ((unsigned char *)ptr + original_size);
    msize_t		next_index = index + msize;
    msize_t		previous_msize, next_msize;
    void		*previous;

    // Assumes locked
    CHECK_LOCKED(szone, __PRETTY_FUNCTION__);
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) {
	malloc_printf("in small_free_no_lock(), ptr=%p, msize=%d\n", ptr, msize);
    }
    if (! msize) {
	malloc_printf("in small_free_no_lock(), ptr=%p, msize=%d\n", ptr, msize);
	szone_error(szone, "trying to free small block that is too small", ptr);
    }
#endif
    // We try to coalesce this block with the preceeding one
    if (index && (SMALL_PREVIOUS_MSIZE(ptr) <= index)) {
	previous_msize = SMALL_PREVIOUS_MSIZE(ptr);
	if (meta_headers[index - previous_msize] == (previous_msize | SMALL_IS_FREE)) {
	    previous = ptr - SMALL_BYTES_FOR_MSIZE(previous_msize);
	    // previous is really to be coalesced
#if DEBUG_MALLOC
	    if (LOG(szone, ptr) || LOG(szone,previous)) { 
		malloc_printf("in small_free_no_lock(), coalesced backwards for %p previous=%p\n", ptr, previous);
	    }
#endif
	    small_free_list_remove_ptr(szone, previous, previous_msize);
	    small_meta_header_set_middle(meta_headers, index);
	    ptr = previous;
	    msize += previous_msize;
	    index -= previous_msize;
	}
    }
    // We try to coalesce with the next block
    if ((next_block < SMALL_REGION_END(*region)) && (meta_headers[next_index] & SMALL_IS_FREE)) {
	// next block is free, we coalesce
	next_msize = meta_headers[next_index] & ~ SMALL_IS_FREE;
#if DEBUG_MALLOC
	if (LOG(szone,ptr)) malloc_printf("In small_free_no_lock(), for ptr=%p, msize=%d coalesced next block=%p next_msize=%d\n", ptr, msize, next_block, next_msize);
#endif
	small_free_list_remove_ptr(szone, next_block, next_msize);
	small_meta_header_set_middle(meta_headers, next_index);
	msize += next_msize;
    }
    if (szone->debug_flags & SCALABLE_MALLOC_DO_SCRIBBLE) {
	if (!msize) {
	    szone_error(szone, "incorrect size information - block header was damaged", ptr);
	} else {
	    memset(ptr, 0x55, SMALL_BYTES_FOR_MSIZE(msize));
	}
    }
    small_free_list_add_ptr(szone, ptr, msize);
    small_meta_header_set_is_free(meta_headers, index, msize);
    szone->num_small_objects--;
    szone->num_bytes_in_small_objects -= original_size; // we use original_size and not msize to avoid double counting the coalesced blocks
}

static void *
small_malloc_from_region_no_lock(szone_t *szone, msize_t msize)
{
    small_region_t	last_region;
    void		*last_block;
    void		*ptr;
    void 		*new_address;
    msize_t		*meta_headers;
    msize_t		index ;
    size_t		region_capacity;
    msize_t		new_msize;
    small_region_t	*new_regions;
    msize_t		msize_left;

    // Allocates from the last region or a freshly allocated region
    CHECK_LOCKED(szone, __PRETTY_FUNCTION__);
    // Before anything we transform the small_bytes_free_at_end - if any - to a regular free block
    if (szone->small_bytes_free_at_end) {
	last_region = szone->small_regions[szone->num_small_regions - 1];
	last_block = (void *)(SMALL_REGION_END(last_region) - szone->small_bytes_free_at_end);
	small_free_list_add_ptr(szone, last_block, SMALL_MSIZE_FOR_BYTES(szone->small_bytes_free_at_end));
	*SMALL_METADATA_FOR_PTR(last_block) = SMALL_MSIZE_FOR_BYTES(szone->small_bytes_free_at_end) | SMALL_IS_FREE;
	szone->small_bytes_free_at_end = 0;
    }
    // time to create a new region
    new_address = allocate_pages(szone, SMALL_REGION_SIZE, SMALL_BLOCKS_ALIGN, 0, VM_MAKE_TAG(VM_MEMORY_MALLOC_SMALL));
    if (!new_address) {
	// out of memory!
	return NULL;
    }
    ptr = new_address;
    meta_headers = SMALL_META_HEADER_FOR_PTR(ptr);
    index = 0;
    if (szone->num_small_regions == INITIAL_NUM_SMALL_REGIONS) {
	// time to grow the number of regions
	region_capacity = (1 << (32 - SMALL_BLOCKS_ALIGN)) - 20; // that is for sure the maximum number of small regions we can have
	new_msize = (region_capacity * sizeof(small_region_t) + SMALL_QUANTUM - 1) / SMALL_QUANTUM;
	new_regions = ptr;
	small_meta_header_set_in_use(meta_headers, index, new_msize);
	szone->num_small_objects++;
	szone->num_bytes_in_small_objects += SMALL_BYTES_FOR_MSIZE(new_msize);
	memcpy(new_regions, szone->small_regions, INITIAL_NUM_SMALL_REGIONS * sizeof(small_region_t));
	// We intentionally leak the previous regions pointer to avoid multi-threading crashes if
	// another thread was reading it (unlocked) while we are changing it.
	szone->small_regions = new_regions; // note we set this pointer after it's all set
	ptr += SMALL_BYTES_FOR_MSIZE(new_msize);
	index = new_msize;
    }
    szone->small_regions[szone->num_small_regions] = new_address;
    // we bump the number of regions AFTER we have changes the regions pointer to enable finding a
    // small region without taking the lock
    // XXX naive assumption assumes memory ordering coherence between this and other CPUs
    szone->num_small_regions++;
    small_meta_header_set_in_use(meta_headers, index, msize);
    msize_left = NUM_SMALL_BLOCKS - index;
    szone->num_small_objects++;
    szone->num_bytes_in_small_objects += SMALL_BYTES_FOR_MSIZE(msize);
    // add a big free block
    index += msize; msize_left -= msize;
    meta_headers[index] = msize_left;
    szone->small_bytes_free_at_end = SMALL_BYTES_FOR_MSIZE(msize_left);
    return ptr;
}

static INLINE boolean_t
try_realloc_small_in_place(szone_t *szone, void *ptr, size_t old_size, size_t new_size)
{
    // returns 1 on success
    msize_t	*meta_headers = SMALL_META_HEADER_FOR_PTR(ptr);
    unsigned	index = SMALL_META_INDEX_FOR_PTR(ptr);
    msize_t	old_msize = SMALL_MSIZE_FOR_BYTES(old_size);
    msize_t	new_msize = SMALL_MSIZE_FOR_BYTES(new_size + SMALL_QUANTUM - 1);
    void	*next_block = (char *)ptr + old_size;
    unsigned	next_index = index + old_msize;
    msize_t	next_msize_and_free;
    msize_t	next_msize;
    msize_t	leftover_msize;
    void	*leftover;
    unsigned	leftover_index;

    if (next_index >= NUM_SMALL_BLOCKS) {
	return 0;
    }
#if DEBUG_MALLOC
    if ((uintptr_t)next_block & (SMALL_QUANTUM - 1)) {
	szone_error(szone, "internal invariant broken in realloc(next_block)", next_block);
    }
    if (meta_headers[index] != old_msize)
	malloc_printf("*** try_realloc_small_in_place incorrect old %d %d\n",
	  meta_headers[index], old_msize);
#endif
    SZONE_LOCK(szone);
    /*
     * Look for a free block immediately afterwards.  If it's large enough, we can consume (part of)
     * it.
     */
    next_msize_and_free = meta_headers[next_index];
    next_msize = next_msize_and_free & ~ SMALL_IS_FREE;
    if (!(next_msize_and_free & SMALL_IS_FREE) || (old_msize + next_msize < new_msize)) {
	SZONE_UNLOCK(szone);
	return 0;
    }
    /*
     * The following block is big enough; pull it from its freelist and chop off enough to satisfy
     * our needs.
     */
    small_free_list_remove_ptr(szone, next_block, next_msize);
    small_meta_header_set_middle(meta_headers, next_index);
    leftover_msize = old_msize + next_msize - new_msize;
    if (leftover_msize) {
	/* there's some left, so put the remainder back */
	leftover = (unsigned char *)ptr + SMALL_BYTES_FOR_MSIZE(new_msize);
	small_free_list_add_ptr(szone, leftover, leftover_msize);
	leftover_index = index + new_msize;
	small_meta_header_set_is_free(meta_headers, leftover_index, leftover_msize);
    }
#if DEBUG_MALLOC
    if (SMALL_BYTES_FOR_MSIZE(new_msize) >= LARGE_THRESHOLD) {
	malloc_printf("*** realloc in place for %p exceeded msize=%d\n", new_msize);
    }
#endif
    small_meta_header_set_in_use(meta_headers, index, new_msize);
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) {
	malloc_printf("in szone_realloc(), ptr=%p, msize=%d\n", ptr, *SMALL_METADATA_FOR_PTR(ptr));
    }
#endif
    szone->num_bytes_in_small_objects += SMALL_BYTES_FOR_MSIZE(new_msize - old_msize);
    SZONE_UNLOCK(szone);
    CHECK(szone, __PRETTY_FUNCTION__);
    return 1;
}

static boolean_t
szone_check_small_region(szone_t *szone, small_region_t *region)
{
    unsigned char	*ptr = SMALL_REGION_ADDRESS(*region);
    msize_t		*meta_headers = SMALL_META_HEADER_FOR_PTR(ptr);
    unsigned char	*region_end = SMALL_REGION_END(*region);
    msize_t		prev_free = 0;
    unsigned		index;
    msize_t		msize_and_free;
    msize_t		msize;
    free_list_t		*free_head;
    msize_t		*follower;

    CHECK_LOCKED(szone, __PRETTY_FUNCTION__);
    if (region == szone->small_regions + szone->num_small_regions - 1) region_end -= szone->small_bytes_free_at_end;
    while (ptr < region_end) {
	index = SMALL_META_INDEX_FOR_PTR(ptr);
	msize_and_free = meta_headers[index];
	if (!(msize_and_free & SMALL_IS_FREE)) {
	    // block is in use
	    msize = msize_and_free;
	    if (!msize) {
		malloc_printf("*** invariant broken: null msize ptr=%p region#=%d num_small_regions=%d end=%p\n",
		  ptr, region - szone->small_regions, szone->num_small_regions, (void *)region_end);
		return 0;
	    }
	    if (msize > (LARGE_THRESHOLD / SMALL_QUANTUM)) {
		malloc_printf("*** invariant broken for %p this small msize=%d - size is too large\n",
		  ptr, msize_and_free);
		return 0;
	    }
	    ptr += SMALL_BYTES_FOR_MSIZE(msize);
	    prev_free = 0;
	} else {
	    // free pointer
	    msize = msize_and_free & ~ SMALL_IS_FREE;
	    free_head = (free_list_t *)ptr;
	    follower = (msize_t *)FOLLOWING_SMALL_PTR(ptr, msize);
	    if (!msize) {
		malloc_printf("*** invariant broken for free block %p this msize=%d\n", ptr, msize);
		return 0;
	    }
	    if (prev_free) {
		malloc_printf("*** invariant broken for %p (2 free in a row)\n", ptr);
		return 0;
	    }
	    free_list_checksum(szone, free_head, __PRETTY_FUNCTION__);
	    if (free_head->previous && !SMALL_PTR_IS_FREE(free_head->previous)) {
		malloc_printf("*** invariant broken for %p (previous %p is not a free pointer)\n",
		  ptr, free_head->previous);
		return 0;
	    }
	    if (free_head->next && !SMALL_PTR_IS_FREE(free_head->next)) {
		malloc_printf("*** invariant broken for %p (next is not a free pointer)\n", ptr);
		return 0;
	    }
	    if (SMALL_PREVIOUS_MSIZE(follower) != msize) {
		malloc_printf("*** invariant broken for small free %p followed by %p in region [%p-%p] "
		  "(end marker incorrect) should be %d; in fact %d\n",
		  ptr, follower, SMALL_REGION_ADDRESS(*region), region_end, msize, SMALL_PREVIOUS_MSIZE(follower));
		return 0;
	    }
	    ptr = (unsigned char *)follower;
	    prev_free = SMALL_IS_FREE;
	}
    }
    return 1;
}

static kern_return_t
small_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t region_address, unsigned short num_regions, size_t small_bytes_free_at_end, memory_reader_t reader, vm_range_recorder_t recorder)
{
    small_region_t	*regions;
    unsigned		index = 0;
    vm_range_t		buffer[MAX_RECORDER_BUFFER];
    unsigned		count = 0;
    kern_return_t	err;
    small_region_t	region;
    vm_range_t		range;
    vm_range_t		admin_range;
    vm_range_t		ptr_range;
    unsigned char	*mapped_region;
    msize_t		*block_header;
    unsigned		block_index;
    unsigned		block_limit;
    msize_t		msize_and_free;
    msize_t		msize;

    err = reader(task, region_address, sizeof(small_region_t) * num_regions, (void **)&regions);
    if (err) return err;
    while (index < num_regions) {
	region = regions[index];
	range.address = (vm_address_t)SMALL_REGION_ADDRESS(region);
	range.size = SMALL_REGION_SIZE;
	if (type_mask & MALLOC_ADMIN_REGION_RANGE_TYPE) {
	    admin_range.address = range.address + (1 << SMALL_BLOCKS_ALIGN);
	    admin_range.size = range.size - (1 << SMALL_BLOCKS_ALIGN);
	    recorder(task, context, MALLOC_ADMIN_REGION_RANGE_TYPE, &admin_range, 1);
	}
	if (type_mask & (MALLOC_PTR_REGION_RANGE_TYPE | MALLOC_ADMIN_REGION_RANGE_TYPE)) {
	    ptr_range.address = range.address;
	    ptr_range.size = 1 << SMALL_BLOCKS_ALIGN;
	    recorder(task, context, MALLOC_PTR_REGION_RANGE_TYPE, &ptr_range, 1);
	}
	if (type_mask & MALLOC_PTR_IN_USE_RANGE_TYPE) {
	    err = reader(task, range.address, range.size, (void **)&mapped_region);
	    if (err) return err;
	    block_header = (msize_t *)(mapped_region + (1 << SMALL_BLOCKS_ALIGN));
	    block_index = 0;
	    block_limit = NUM_SMALL_BLOCKS;
	    if (index == num_regions - 1)
		block_limit -= SMALL_MSIZE_FOR_BYTES(small_bytes_free_at_end);
	    while (block_index < block_limit) {
		msize_and_free = block_header[block_index];
		msize = msize_and_free & ~ SMALL_IS_FREE;
		if (! (msize_and_free & SMALL_IS_FREE)) {
		    // Block in use
		    buffer[count].address = range.address + SMALL_BYTES_FOR_MSIZE(block_index);
		    buffer[count].size = SMALL_BYTES_FOR_MSIZE(msize);
		    count++;
		    if (count >= MAX_RECORDER_BUFFER) {
			recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE, buffer, count);
			count = 0;
		    }
		}
		block_index += msize;
	    }
	}
	index++;
    }
    if (count) {
	recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE, buffer, count);
    }
    return 0;
}

static INLINE void *
small_malloc_from_free_list(szone_t *szone, msize_t msize)
{
    grain_t	grain = (msize <= NUM_SMALL_SLOTS) ? msize - 1 : NUM_SMALL_SLOTS - 1;
    unsigned	bitmap = szone->small_bitmap & ~ ((1 << grain) - 1);
    void	*ptr;
    msize_t	this_msize;
    free_list_t	**free_list;
    free_list_t	**limit;
    free_list_t	*next;
    msize_t	leftover_msize;
    void	*leftover_ptr;
    msize_t	*meta_headers;
    unsigned	leftover_index;

    // Assumes locked
    CHECK_LOCKED(szone, __PRETTY_FUNCTION__);
    
    if (!bitmap) goto try_small_from_end;
    grain = BITMAP32_FFS(bitmap) - 1;
    // first try the small grains
    limit = szone->small_free_list + NUM_SMALL_SLOTS - 1;
    free_list = szone->small_free_list + grain;
    while (free_list < limit) {
	// try bigger grains
	ptr = *free_list;
	if (ptr) {
	    next = ((free_list_t *)ptr)->next;
	    if (next) {
		next->previous = NULL;
		free_list_set_checksum(szone, next);
	    }
	    *free_list = next;
	    this_msize = SMALL_PTR_SIZE(ptr);
	    goto add_leftover_and_proceed;
	}
	free_list++;
    }
    // We now check the large grains for one that is big enough
    ptr = *free_list;
    while (ptr) {
	this_msize = SMALL_PTR_SIZE(ptr);
	if (this_msize >= msize) {
	    small_free_list_remove_ptr(szone, ptr, this_msize);
	    goto add_leftover_and_proceed;
	}
	ptr = ((free_list_t *)ptr)->next;
    }
try_small_from_end:
    // Let's see if we can use szone->small_bytes_free_at_end
    if (szone->small_bytes_free_at_end >= SMALL_BYTES_FOR_MSIZE(msize)) {
	ptr = (void *)(SMALL_REGION_END(szone->small_regions[szone->num_small_regions-1]) - szone->small_bytes_free_at_end);
	szone->small_bytes_free_at_end -= SMALL_BYTES_FOR_MSIZE(msize);
	if (szone->small_bytes_free_at_end) {
	    // let's mark this block as in use to serve as boundary
	    *SMALL_METADATA_FOR_PTR(ptr + SMALL_BYTES_FOR_MSIZE(msize)) = SMALL_MSIZE_FOR_BYTES(szone->small_bytes_free_at_end);
	}
	this_msize = msize;
	goto return_small_alloc;
    }
    return NULL;
add_leftover_and_proceed:
    if (this_msize > msize) {
	leftover_msize = this_msize - msize;
	leftover_ptr = ptr + SMALL_BYTES_FOR_MSIZE(msize);
#if DEBUG_MALLOC
	if (LOG(szone,ptr)) {
	    malloc_printf("in small_malloc_from_free_list(), adding leftover ptr=%p, this_msize=%d\n", ptr, this_msize);
	}
#endif
	small_free_list_add_ptr(szone, leftover_ptr, leftover_msize);
	meta_headers = SMALL_META_HEADER_FOR_PTR(leftover_ptr);
	leftover_index = SMALL_META_INDEX_FOR_PTR(leftover_ptr);
	small_meta_header_set_is_free(meta_headers, leftover_index, leftover_msize);
	this_msize = msize;
    }
return_small_alloc:
    szone->num_small_objects++;
    szone->num_bytes_in_small_objects += SMALL_BYTES_FOR_MSIZE(this_msize);
#if DEBUG_MALLOC
    if (LOG(szone,ptr)) {
	malloc_printf("in small_malloc_from_free_list(), ptr=%p, this_msize=%d, msize=%d\n", ptr, this_msize, msize);
    }
#endif
    *SMALL_METADATA_FOR_PTR(ptr) = this_msize;
    return ptr;
}

static INLINE void *
small_malloc_should_clear(szone_t *szone, msize_t msize, boolean_t cleared_requested)
{
    boolean_t	locked = 0;
#if SMALL_CACHE
    void	*ptr;
#endif
    
#if SMALL_CACHE
    ptr = (void *)szone->last_small_free;
    if ((((uintptr_t)ptr) & (SMALL_QUANTUM - 1)) == msize) {
	// we have a candidate - let's lock to make sure
	LOCK_AND_NOTE_LOCKED(szone, locked);
	if (ptr == (void *)szone->last_small_free) {
	    szone->last_small_free = NULL;
	    SZONE_UNLOCK(szone);
	    CHECK(szone, __PRETTY_FUNCTION__);
	    ptr = (void *)((uintptr_t)ptr & ~ (SMALL_QUANTUM - 1));
	    if (cleared_requested) {
		memset(ptr, 0, SMALL_BYTES_FOR_MSIZE(msize));
	    }
	    return ptr;
	}
    }
#endif
    // Except in rare occasions where we need to add a new region, we are going to end up locking,
    // so we might as well lock right away to avoid doing unnecessary optimistic probes
    if (!locked) LOCK_AND_NOTE_LOCKED(szone, locked);
    ptr = small_malloc_from_free_list(szone, msize);
    if (ptr) {
	SZONE_UNLOCK(szone);
	CHECK(szone, __PRETTY_FUNCTION__);
	if (cleared_requested) {
	    memset(ptr, 0, SMALL_BYTES_FOR_MSIZE(msize));
	}
	return ptr;
    }
    ptr = small_malloc_from_region_no_lock(szone, msize);
    // we don't clear because this freshly allocated space is pristine
    SZONE_UNLOCK(szone);
    CHECK(szone, __PRETTY_FUNCTION__);
    return ptr;
}

// tries to allocate a small, cleared block
static INLINE void *
small_malloc_cleared_no_lock(szone_t *szone, msize_t msize)
{
    void	*ptr;

    // Assumes already locked
    CHECK_LOCKED(szone, __PRETTY_FUNCTION__);
    ptr = small_malloc_from_free_list(szone, msize);
    if (ptr) {
	memset(ptr, 0, SMALL_BYTES_FOR_MSIZE(msize));
	return ptr;
    } else {
	ptr = small_malloc_from_region_no_lock(szone, msize);
	// we don't clear because this freshly allocated space is pristine
    }
    return ptr;
}

static INLINE void
free_small(szone_t *szone, void *ptr, small_region_t *small_region)
{
    msize_t		msize_and_free;
#if SMALL_CACHE
    void		*ptr2;
#endif
    
    // ptr is known to be in small_region
    msize_and_free = *SMALL_METADATA_FOR_PTR(ptr);
    if (msize_and_free & SMALL_IS_FREE) {
	szone_error(szone, "Object already freed being freed", ptr);
	return;
    }
    CHECK(szone, __PRETTY_FUNCTION__);
    SZONE_LOCK(szone);
#if SMALL_CACHE
    ptr2 = szone->last_small_free;
    szone->last_small_free = (void *)(((uintptr_t)ptr) | msize_and_free);
    if (!ptr2) {
	SZONE_UNLOCK(szone);
	CHECK(szone, __PRETTY_FUNCTION__);
	return;
    }
    msize_and_free = (uintptr_t)ptr2 & (SMALL_QUANTUM - 1);
    ptr = (void *)(((uintptr_t)ptr2) & ~ (SMALL_QUANTUM - 1));
    small_region = small_region_for_ptr_no_lock(szone, ptr);
    if (!small_region) {
	szone_error(szone, "double free (small cache)", ptr);
	return;
    }
#endif
    small_free_no_lock(szone, small_region, ptr, msize_and_free);
    SZONE_UNLOCK(szone);
    CHECK(szone, __PRETTY_FUNCTION__);
}

static void
print_small_free_list(szone_t *szone)
{
    grain_t		grain = 0;
    free_list_t		*ptr;
    
    malloc_printf("small free sizes: ");
    while (grain < NUM_SMALL_SLOTS) {
	ptr = szone->small_free_list[grain];
	if (ptr) {
	    malloc_printf("%s%y[%d]; ", (grain == NUM_SMALL_SLOTS-1) ? ">=" : "", (grain + 1) * SMALL_QUANTUM, free_list_count(ptr));
	}
	grain++;
    }
    malloc_printf("\n");
}

static void
print_small_region(szone_t *szone, boolean_t verbose, small_region_t *region, size_t bytes_at_end)
{
    unsigned		counts[1024];
    unsigned		in_use = 0;
    void		*start = SMALL_REGION_ADDRESS(*region);
    void		*limit = SMALL_REGION_END(*region) - bytes_at_end;
    msize_t		msize_and_free;
    msize_t		msize;
    unsigned		ci;

    memset(counts, 0, 1024 * sizeof(unsigned));
    while (start < limit) {
	msize_and_free = *SMALL_METADATA_FOR_PTR(start);
	msize = msize_and_free & ~ SMALL_IS_FREE;
	if (!(msize_and_free & SMALL_IS_FREE)) {
	    // block in use
	    if (msize < 1024)
		counts[msize]++;
	    in_use++;
	}
	start += SMALL_BYTES_FOR_MSIZE(msize);
    }
    malloc_printf("Small region [%p-%p, %y]\tIn_use=%d ",
      SMALL_REGION_ADDRESS(*region), SMALL_REGION_END(*region), (int)SMALL_REGION_SIZE, in_use);
    if (bytes_at_end)
	malloc_printf("Untouched=%y ", bytes_at_end);
    if (verbose && in_use) {
	malloc_printf("\n\tSizes in use: "); 
	for (ci = 0; ci < 1024; ci++)
	    if (counts[ci])
		malloc_printf("%d[%d] ", SMALL_BYTES_FOR_MSIZE(ci), counts[ci]);
    }
    malloc_printf("\n");
}

static boolean_t
small_free_list_check(szone_t *szone, grain_t grain)
{
    unsigned	count = 0;
    free_list_t	*ptr = szone->small_free_list[grain];
    free_list_t	*previous = NULL;
    msize_t	msize_and_free;

    CHECK_LOCKED(szone, __PRETTY_FUNCTION__);
    while (ptr) {
	msize_and_free = *SMALL_METADATA_FOR_PTR(ptr);
	count++;
	if (!(msize_and_free & SMALL_IS_FREE)) {
	    malloc_printf("*** in-use ptr in free list grain=%d count=%d ptr=%p\n", grain, count, ptr);
	    return 0;
	}
	if (((uintptr_t)ptr) & (SMALL_QUANTUM - 1)) {
	    malloc_printf("*** unaligned ptr in free list grain=%d  count=%d ptr=%p\n", grain, count, ptr);
	    return 0;
	}
	if (!small_region_for_ptr_no_lock(szone, ptr)) {
	    malloc_printf("*** ptr not in szone grain=%d  count=%d ptr=%p\n", grain, count, ptr);
	    return 0;
	}
	free_list_checksum(szone, ptr, __PRETTY_FUNCTION__);
	if (ptr->previous != previous) {
	    malloc_printf("*** previous incorrectly set grain=%d  count=%d ptr=%p\n", grain, count, ptr);
	    return 0;
	}
	previous = ptr;
	ptr = ptr->next;
    }
    return 1;
}

/*********************	LARGE ENTRY UTILITIES	************************/

#if DEBUG_MALLOC

static void
large_debug_print(szone_t *szone)
{
    unsigned		num_large_entries = szone->num_large_entries;
    unsigned		index = num_large_entries;
    large_entry_t	*range;

    for (index = 0, range = szone->large_entries; index < szone->num_large_entries; index++, range++)
	if (!LARGE_ENTRY_IS_EMPTY(*range))
	    malloc_printf("%d: %p(%y);  ", index, LARGE_ENTRY_ADDRESS(*range), LARGE_ENTRY_SIZE(*range));

    malloc_printf("\n");
}
#endif

/*
 * Scan the hash ring looking for an entry for the given pointer.
 */
static large_entry_t *
large_entry_for_pointer_no_lock(szone_t *szone, const void *ptr)
{
    // result only valid with lock held
    unsigned		num_large_entries = szone->num_large_entries;
    unsigned		hash_index;
    unsigned		index;
    large_entry_t	*range;

    if (!num_large_entries)
	return NULL;
    hash_index = ((uintptr_t)ptr >> vm_page_shift) % num_large_entries;
    index = hash_index;
    do {
	range = szone->large_entries + index;
	if (LARGE_ENTRY_MATCHES(*range, ptr))
	    return range;
	if (LARGE_ENTRY_IS_EMPTY(*range))
	    return NULL; // end of chain
	index++;
	if (index == num_large_entries)
	    index = 0;
    } while (index != hash_index);
    return NULL;
}

static void
large_entry_insert_no_lock(szone_t *szone, large_entry_t range)
{
    unsigned		num_large_entries = szone->num_large_entries;
    unsigned		hash_index = (range.address_and_num_pages >> vm_page_shift) % num_large_entries;
    unsigned		index = hash_index;
    large_entry_t	*entry;

    do {
	entry = szone->large_entries + index;
	if (LARGE_ENTRY_IS_EMPTY(*entry)) {
	    *entry = range;
	    return; // end of chain
	}
	index++;
	if (index == num_large_entries)
	    index = 0;
    } while (index != hash_index);
}

static INLINE void
large_entries_rehash_after_entry_no_lock(szone_t *szone, large_entry_t *entry)
{
    unsigned		num_large_entries = szone->num_large_entries;
    unsigned		hash_index = entry - szone->large_entries;
    unsigned		index = hash_index;
    large_entry_t	range;

    do {
	index++;
	if (index == num_large_entries)
	    index = 0;
	range = szone->large_entries[index];
	if (LARGE_ENTRY_IS_EMPTY(range))
	    return;
	szone->large_entries[index].address_and_num_pages = 0;
	large_entry_insert_no_lock(szone, range); // this will reinsert in the
						  // proper place
    } while (index != hash_index);
}

static INLINE large_entry_t *
large_entries_alloc_no_lock(szone_t *szone, unsigned num)
{
    size_t	size = num * sizeof(large_entry_t);
    boolean_t	is_vm_allocation = size >= LARGE_THRESHOLD;

    if (is_vm_allocation) {
	// Note that we allocate memory (via a system call) under a spin lock
	// That is certainly evil, however it's very rare in the lifetime of a process
	// The alternative would slow down the normal case
	return (void *)allocate_pages(szone, round_page(size), 0, 0, VM_MAKE_TAG(VM_MEMORY_MALLOC_LARGE));
    } else {
	return small_malloc_cleared_no_lock(szone, SMALL_MSIZE_FOR_BYTES(size + SMALL_QUANTUM - 1));
    }
}

static void
large_entries_free_no_lock(szone_t *szone, large_entry_t *entries, unsigned num, vm_range_t *range_to_deallocate)
{
    // returns range to deallocate
    size_t		size = num * sizeof(large_entry_t);
    boolean_t		is_vm_allocation = size >= LARGE_THRESHOLD;
    small_region_t	*region;
    msize_t		msize_and_free;
    
    if (is_vm_allocation) {
	range_to_deallocate->address = (vm_address_t)entries;
	range_to_deallocate->size = round_page(size);
    } else {
	range_to_deallocate->size = 0;
	region = small_region_for_ptr_no_lock(szone, entries);
	msize_and_free = *SMALL_METADATA_FOR_PTR(entries);
	if (msize_and_free & SMALL_IS_FREE) {
	    szone_error(szone, "object already freed being freed", entries);
	    return;
	}
	small_free_no_lock(szone, region, entries, msize_and_free);
    }
}

static void
large_entries_grow_no_lock(szone_t *szone, vm_range_t *range_to_deallocate)
{
    // sets range_to_deallocate
    unsigned		old_num_entries = szone->num_large_entries;
    large_entry_t	*old_entries = szone->large_entries;
    unsigned		new_num_entries = (old_num_entries) ? old_num_entries * 2 + 1 : 63; // always an odd number for good hashing
    large_entry_t	*new_entries = large_entries_alloc_no_lock(szone, new_num_entries);
    unsigned		index = old_num_entries;
    large_entry_t	oldRange;

    szone->num_large_entries = new_num_entries;
    szone->large_entries = new_entries;

    /* rehash entries into the new list */
    while (index--) {
	oldRange = old_entries[index];
	if (!LARGE_ENTRY_IS_EMPTY(oldRange)) {
	    large_entry_insert_no_lock(szone, oldRange);
	}
    }
    if (old_entries) {
	large_entries_free_no_lock(szone, old_entries, old_num_entries, range_to_deallocate);
    } else {
	range_to_deallocate->size = 0;
    }
}

// frees the specific entry in the size table
// returns a range to truly deallocate
static vm_range_t
large_free_no_lock(szone_t *szone, large_entry_t *entry)
{
    vm_range_t		range;
    
    range.address = (vm_address_t)LARGE_ENTRY_ADDRESS(*entry);
    range.size = (vm_size_t)LARGE_ENTRY_SIZE(*entry);
    szone->num_large_objects_in_use--;
    szone->num_bytes_in_large_objects -= range.size;
    if (szone->debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES) {
	protect(szone, (void *)range.address, range.size, VM_PROT_READ | VM_PROT_WRITE, szone->debug_flags);
	range.address -= vm_page_size;
	range.size += 2 * vm_page_size;
    }
    entry->address_and_num_pages = 0;
    large_entries_rehash_after_entry_no_lock(szone, entry);
#if DEBUG_MALLOC
    if (large_entry_for_pointer_no_lock(szone, (void *)range.address)) {
	malloc_printf("*** freed entry %p still in use; num_large_entries=%d\n",
	  range.address, szone->num_large_entries);
	large_debug_print(szone);
	szone_sleep();
    }
#endif
    return range;
}

static kern_return_t
large_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t large_entries_address, unsigned num_entries, memory_reader_t reader, vm_range_recorder_t recorder)
{
    unsigned		index = 0;
    vm_range_t		buffer[MAX_RECORDER_BUFFER];
    unsigned		count = 0;
    large_entry_t	*entries;
    kern_return_t	err;
    vm_range_t		range;
    large_entry_t	entry;

    err = reader(task, large_entries_address, sizeof(large_entry_t) * num_entries, (void **)&entries);
    if (err)
	return err;
    index = num_entries;
    if ((type_mask & MALLOC_ADMIN_REGION_RANGE_TYPE) &&
      (num_entries * sizeof(large_entry_t) >= LARGE_THRESHOLD)) {
	range.address = large_entries_address;
	range.size = round_page(num_entries * sizeof(large_entry_t));
	recorder(task, context, MALLOC_ADMIN_REGION_RANGE_TYPE, &range, 1);
    }
    if (type_mask & (MALLOC_PTR_IN_USE_RANGE_TYPE | MALLOC_PTR_REGION_RANGE_TYPE))
	while (index--) {
	    entry = entries[index];
	    if (!LARGE_ENTRY_IS_EMPTY(entry)) {
		range.address = (vm_address_t)LARGE_ENTRY_ADDRESS(entry);
		range.size = (vm_size_t)LARGE_ENTRY_SIZE(entry);
		buffer[count++] = range;
		if (count >= MAX_RECORDER_BUFFER) {
		    recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE | MALLOC_PTR_REGION_RANGE_TYPE, buffer, count);
		    count = 0;
		}
	    }
	}
    if (count) {
	recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE
	  | MALLOC_PTR_REGION_RANGE_TYPE, buffer, count);
    }
    return 0;
}

/*********************	HUGE ENTRY UTILITIES	************************/

static huge_entry_t *
huge_entry_for_pointer_no_lock(szone_t *szone, const void *ptr)
{
    unsigned		index;
    huge_entry_t	*huge;

    for (index = szone->num_huge_entries, huge = szone->huge_entries;
	 index > 0;
	 index--, huge++) {
    
	if ((void *)huge->address == ptr)
	    return huge;
    }
    return NULL;
}

static boolean_t
huge_entry_append(szone_t *szone, huge_entry_t huge)
{
    huge_entry_t	*new_huge_entries = NULL, *old_huge_entries;
    unsigned		num_huge_entries;
    
    // We do a little dance with locking because doing allocation (even in the
    // default szone) may cause something to get freed in this szone, with a
    // deadlock
    // Returns 1 on success
    SZONE_LOCK(szone);
    for (;;) {
	num_huge_entries = szone->num_huge_entries;
	SZONE_UNLOCK(szone);
	/* check for counter wrap */
	if ((num_huge_entries + 1) < num_huge_entries)
		return 0;
	/* stale allocation from last time around the loop? */
	if (new_huge_entries)
	    szone_free(szone, new_huge_entries);
	new_huge_entries = szone_malloc(szone, (num_huge_entries + 1) * sizeof(huge_entry_t));
	if (new_huge_entries == NULL)
	    return 0;
	SZONE_LOCK(szone);
	if (num_huge_entries == szone->num_huge_entries) {
	    // No change - our malloc still applies
	    old_huge_entries = szone->huge_entries;
	    if (num_huge_entries) {
		memcpy(new_huge_entries, old_huge_entries, num_huge_entries * sizeof(huge_entry_t));
	    }
	    new_huge_entries[szone->num_huge_entries++] = huge;
	    szone->huge_entries = new_huge_entries;
	    SZONE_UNLOCK(szone);
	    szone_free(szone, old_huge_entries);
	    return 1;
	}
	// try again!
    }
}

static kern_return_t
huge_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t huge_entries_address, unsigned num_entries, memory_reader_t reader, vm_range_recorder_t recorder)
{
    huge_entry_t	*entries;
    kern_return_t	err;
    
    err = reader(task, huge_entries_address, sizeof(huge_entry_t) * num_entries, (void **)&entries);
    if (err)
	return err;
    if (num_entries)
	recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE | MALLOC_PTR_REGION_RANGE_TYPE, entries, num_entries);

    return 0;
}

static void *
large_and_huge_malloc(szone_t *szone, unsigned num_pages)
{
    void		*addr;
    vm_range_t		range_to_deallocate;
    huge_entry_t	huge_entry;
    size_t		size;
    large_entry_t	large_entry;
    
    if (!num_pages)
	    num_pages = 1; // minimal allocation size for this szone
    size = (size_t)num_pages << vm_page_shift;
    range_to_deallocate.size = 0;
    if (num_pages >= (1 << vm_page_shift)) {
	addr = allocate_pages(szone, size, 0, szone->debug_flags, VM_MAKE_TAG(VM_MEMORY_MALLOC_HUGE));
	if (addr == NULL)
	    return NULL;
	huge_entry.size = size;
	huge_entry.address = (vm_address_t)addr;
	if (!huge_entry_append(szone, huge_entry))
	    return NULL;	// we are leaking the allocation here
	SZONE_LOCK(szone);
	szone->num_bytes_in_huge_objects += size;
    } else {

	addr = allocate_pages(szone, size, 0, szone->debug_flags, VM_MAKE_TAG(VM_MEMORY_MALLOC_LARGE));
#if DEBUG_MALLOC
	if (LOG(szone, addr))
	    malloc_printf("in szone_malloc true large allocation at %p for %y\n", (void *)addr, size);
#endif
	SZONE_LOCK(szone);
	if (addr == NULL) {
	    SZONE_UNLOCK(szone);
	    return NULL;
	}
#if DEBUG_MALLOC
	if (large_entry_for_pointer_no_lock(szone, addr)) {
	    malloc_printf("freshly allocated is already in use: %p\n", addr);
	    large_debug_print(szone);
	    szone_sleep();
	}
#endif
	if ((szone->num_large_objects_in_use + 1) * 4 > szone->num_large_entries) {
	    // density of hash table too high; grow table
	    // we do that under lock to avoid a race
	    large_entries_grow_no_lock(szone, &range_to_deallocate);
	}
	large_entry.address_and_num_pages = (uintptr_t)addr | num_pages;
#if DEBUG_MALLOC
	if (large_entry_for_pointer_no_lock(szone, addr)) {
	    malloc_printf("entry about to be added already in use: %p\n", addr);
	    large_debug_print(szone);
	    szone_sleep();
	}
#endif
	large_entry_insert_no_lock(szone, large_entry);
#if DEBUG_MALLOC
	if (!large_entry_for_pointer_no_lock(szone, (void *)addr)) {
	    malloc_printf("can't find entry just added\n");
	    large_debug_print(szone);
	    szone_sleep();
	}
#endif
	szone->num_large_objects_in_use ++;
	szone->num_bytes_in_large_objects += size;
    }
    SZONE_UNLOCK(szone);
    if (range_to_deallocate.size) {
	deallocate_pages(szone, (void *)range_to_deallocate.address, range_to_deallocate.size, 0); // we deallocate outside the lock
    }
    return (void *)addr;
}

static INLINE void
free_large_or_huge(szone_t *szone, void *ptr)
{
    // We have established ptr is page-aligned and not tiny nor small
    large_entry_t	*entry;
    vm_range_t		vm_range_to_deallocate;
    huge_entry_t	*huge;
    
    SZONE_LOCK(szone);
    entry = large_entry_for_pointer_no_lock(szone, ptr);
    if (entry) {
	vm_range_to_deallocate = large_free_no_lock(szone, entry);
#if DEBUG_MALLOC
	if (large_entry_for_pointer_no_lock(szone, ptr)) {
	    malloc_printf("*** just after freeing %p still in use num_large_entries=%d\n", ptr, szone->num_large_entries);
	    large_debug_print(szone);
	    szone_sleep();
	}
#endif
    } else if ((huge = huge_entry_for_pointer_no_lock(szone, ptr))) {
	vm_range_to_deallocate = *huge;
	*huge = szone->huge_entries[--szone->num_huge_entries]; // last entry fills that spot
	szone->num_bytes_in_huge_objects -= (size_t)vm_range_to_deallocate.size;
    } else {
#if DEBUG_MALLOC
	large_debug_print(szone);
#endif
	szone_error(szone, "pointer being freed was not allocated", ptr);
	return;
    }
    SZONE_UNLOCK(szone); // we release the lock asap
    CHECK(szone, __PRETTY_FUNCTION__);
    // we deallocate_pages, including guard pages
    if (vm_range_to_deallocate.address) {
#if DEBUG_MALLOC
	if (large_entry_for_pointer_no_lock(szone, (void *)vm_range_to_deallocate.address)) {
	    malloc_printf("*** invariant broken: %p still in use num_large_entries=%d\n", vm_range_to_deallocate.address, szone->num_large_entries);
	    large_debug_print(szone);
	    szone_sleep();
	}
#endif
	deallocate_pages(szone, (void *)vm_range_to_deallocate.address, (size_t)vm_range_to_deallocate.size, 0);
    }
}

static INLINE int
try_realloc_large_or_huge_in_place(szone_t *szone, void *ptr, size_t old_size, size_t new_size)
{
    vm_address_t	addr = (vm_address_t)ptr + old_size;
    large_entry_t	*large_entry, saved_entry;
    huge_entry_t	*huge_entry, huge;
    kern_return_t	err;

#if DEBUG_MALLOC
    if (old_size != ((old_size >> vm_page_shift) << vm_page_shift)) {
	malloc_printf("*** old_size is %d\n", old_size);
    }
#endif
    SZONE_LOCK(szone);
    large_entry = large_entry_for_pointer_no_lock(szone, (void *)addr);
    SZONE_UNLOCK(szone);
    if (large_entry) {
	return 0; // large pointer already exists in table - extension is not going to work
    }
    new_size = round_page(new_size);
    /*
     * Ask for allocation at a specific address, and mark as realloc
     * to request coalescing with previous realloc'ed extensions.
     */
    err = vm_allocate(mach_task_self(), &addr, new_size - old_size, VM_MAKE_TAG(VM_MEMORY_REALLOC));
    if (err != KERN_SUCCESS) {
	return 0;
    }
    SZONE_LOCK(szone);
    /*
     * If the new size is still under the large/huge threshold, we can just
     * extend the existing large block.
     *
     * Note: this logic is predicated on the understanding that an allocated
     * block can never really shrink, so that the new size will always be 
     * larger than the old size.
     *
     * Note: the use of 1 << vm_page_shift here has to do with the subdivision
     * of the bits in the large_entry_t, and not the size of a page (directly).
     */
    if ((new_size >> vm_page_shift) < (1 << vm_page_shift)) {
	/* extend existing large entry */
	large_entry = large_entry_for_pointer_no_lock(szone, ptr);
	if (!large_entry) {
	    szone_error(szone, "large entry reallocated is not properly in table", ptr);
	    /* XXX will cause fault on next reference to entry */
	}
	large_entry->address_and_num_pages = (uintptr_t)ptr | (new_size >> vm_page_shift);
	szone->num_bytes_in_large_objects += new_size - old_size;
    } else if ((old_size >> vm_page_shift) >= (1 << vm_page_shift)) {
	/* extend existing huge entry */
	huge_entry = huge_entry_for_pointer_no_lock(szone, ptr);
	if (!huge_entry) {
	    szone_error(szone, "huge entry reallocated is not properly in table", ptr);
	    /* XXX will cause fault on next reference to huge_entry */
	}
	huge_entry->size = new_size;
	szone->num_bytes_in_huge_objects += new_size - old_size;
    } else {
	/* need to convert large entry to huge entry */

	/* release large entry, note we still have the VM allocation */
	large_entry = large_entry_for_pointer_no_lock(szone, ptr);
	saved_entry = *large_entry; // in case we need to put it back
	large_free_no_lock(szone, large_entry);
	szone->num_bytes_in_large_objects -= old_size;

	/* and get a huge entry */
	huge.address = (vm_address_t)ptr;
	huge.size = new_size;	/* fix up size */
	SZONE_UNLOCK(szone);
	if (huge_entry_append(szone, huge)) {
	    szone->num_bytes_in_huge_objects += new_size;
	    return 1; // success!
	}
	SZONE_LOCK(szone);
	// we leak memory (the extra space appended) but data structures are correct
	large_entry_insert_no_lock(szone, saved_entry); // this will reinsert the large entry
    }
    SZONE_UNLOCK(szone); // we release the lock asap
    return 1;
}

/*********************	Zone call backs	************************/

static void
szone_free(szone_t *szone, void *ptr)
{
    tiny_region_t	*tiny_region;
    small_region_t	*small_region;

#if DEBUG_MALLOC
    if (LOG(szone, ptr))
	malloc_printf("in szone_free with %p\n", ptr);
#endif
    if (!ptr)
	return;
    /*
     * Try to free to a tiny region.
     */
    if ((uintptr_t)ptr & (TINY_QUANTUM - 1)) {
	szone_error(szone, "Non-aligned pointer being freed", ptr);
	return;
    }
    if ((tiny_region = tiny_region_for_ptr_no_lock(szone, ptr)) != NULL) {
	free_tiny(szone, ptr, tiny_region);
	return;
    }

    /*
     * Try to free to a small region.
     */
    if ((uintptr_t)ptr & (SMALL_QUANTUM - 1)) {
	szone_error(szone, "Non-aligned pointer being freed (2)", ptr);
	return;
    }
    if ((small_region = small_region_for_ptr_no_lock(szone, ptr)) != NULL) {
	free_small(szone, ptr, small_region);
	return;
    }

    /* check that it's a legal large/huge allocation */
    if ((uintptr_t)ptr & (vm_page_size - 1)) {
	szone_error(szone, "non-page-aligned, non-allocated pointer being freed", ptr);
	return;
    }
    free_large_or_huge(szone, ptr);
}

static INLINE void *
szone_malloc_should_clear(szone_t *szone, size_t size, boolean_t cleared_requested)
{
    void	*ptr;
    msize_t	msize;
    unsigned	num_pages;

    if (size <= 31*TINY_QUANTUM) {
	// think tiny
	msize = TINY_MSIZE_FOR_BYTES(size + TINY_QUANTUM - 1);
	if (!msize)
	    msize = 1;
	ptr = tiny_malloc_should_clear(szone, msize, cleared_requested);
    } else if (!((szone->debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES) && PROTECT_SMALL) && (size < LARGE_THRESHOLD)) {
	// think small
	msize = SMALL_MSIZE_FOR_BYTES(size + SMALL_QUANTUM - 1);
	if (! msize) msize = 1;
	ptr = small_malloc_should_clear(szone, msize, cleared_requested);
    } else {
	// large or huge
	num_pages = round_page(size) >> vm_page_shift;
	if (num_pages == 0)	/* Overflowed */
		ptr = 0;
	else
		ptr = large_and_huge_malloc(szone, num_pages);
    }
#if DEBUG_MALLOC
    if (LOG(szone, ptr))
	malloc_printf("szone_malloc returned %p\n", ptr);
#endif
    /*
     * If requested, scribble on allocated memory.
     */
    if ((szone->debug_flags & SCALABLE_MALLOC_DO_SCRIBBLE) && ptr && !cleared_requested && size)
	memset(ptr, 0xaa, size);

    return ptr;
}

static void *
szone_malloc(szone_t *szone, size_t size) {
    return szone_malloc_should_clear(szone, size, 0);
}

static void *
szone_calloc(szone_t *szone, size_t num_items, size_t size)
{
    return szone_malloc_should_clear(szone, num_items * size, 1);
}

static void *
szone_valloc(szone_t *szone, size_t size)
{
    void	*ptr;
    unsigned	num_pages;
    
    num_pages = round_page(size) >> vm_page_shift;
    ptr = large_and_huge_malloc(szone, num_pages);
#if DEBUG_MALLOC
    if (LOG(szone, ptr))
	malloc_printf("szone_valloc returned %p\n", ptr);
#endif
    return ptr;
}

static size_t
szone_size(szone_t *szone, const void *ptr)
{
    size_t		size = 0;
    boolean_t		is_free;
    msize_t		msize, msize_and_free;
    large_entry_t	*entry;
    huge_entry_t	*huge;
    
    if (!ptr)
	return 0;
#if DEBUG_MALLOC
    if (LOG(szone, ptr)) {
	malloc_printf("in szone_size for %p (szone=%p)\n", ptr, szone);
    }
#endif

    /*
     * Look for it in a tiny region.
     */
    if ((uintptr_t)ptr & (TINY_QUANTUM - 1))
	return 0;
    if (tiny_region_for_ptr_no_lock(szone, ptr)) {
	msize = get_tiny_meta_header(ptr, &is_free);
	return (is_free) ? 0 : TINY_BYTES_FOR_MSIZE(msize);
    }
    
    /*
     * Look for it in a small region.
     */
    if ((uintptr_t)ptr & (SMALL_QUANTUM - 1))
	return 0;
    if (small_region_for_ptr_no_lock(szone, ptr)) {
	msize_and_free = *SMALL_METADATA_FOR_PTR(ptr);
	return (msize_and_free & SMALL_IS_FREE) ? 0 : SMALL_BYTES_FOR_MSIZE(msize_and_free);
    }

    /*
     * If not page-aligned, it cannot have come from a large or huge allocation.
     */
    if ((uintptr_t)ptr & (vm_page_size - 1))
	return(0);

    /*
     * Look for it in a large or huge entry.
     */
    SZONE_LOCK(szone);
    entry = large_entry_for_pointer_no_lock(szone, ptr);
    if (entry) {
	size = LARGE_ENTRY_SIZE(*entry);
    } else if ((huge = huge_entry_for_pointer_no_lock(szone, ptr))) {
	size = huge->size;
    }
    SZONE_UNLOCK(szone); 
#if DEBUG_MALLOC
    if (LOG(szone, ptr)) {
	malloc_printf("szone_size for %p returned %d\n", ptr, (unsigned)size);
    }
#endif
    return size;
}

static void *
szone_realloc(szone_t *szone, void *ptr, size_t new_size)
{
    size_t		old_size;
    void		*new_ptr;
    
#if DEBUG_MALLOC
    if (LOG(szone, ptr)) {
	malloc_printf("in szone_realloc for %p, %d\n", ptr, (unsigned)new_size);
    }
#endif
    if (!ptr) {
	ptr = szone_malloc(szone, new_size);
	return ptr;
    }
    old_size = szone_size(szone, ptr);
    if (!old_size) {
	szone_error(szone, "pointer being reallocated was not allocated", ptr);
	return NULL;
    }
    /* we never shrink an allocation */
    if (old_size >= new_size)
	return ptr;

    /*
     * If the old and new sizes both suit the tiny allocator, try to reallocate in-place.
     */
    if ((new_size + TINY_QUANTUM - 1) <= 31 * TINY_QUANTUM) {
	if (try_realloc_tiny_in_place(szone, ptr, old_size, new_size)) {
	    return ptr;
	}

	/*
	 * If the old and new sizes both suit the small allocator, and we're not protecting the
	 * small allocations, try to reallocate in-place.
	 */
    } else if (!((szone->debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES) && PROTECT_SMALL) &&
      ((new_size + SMALL_QUANTUM - 1) < LARGE_THRESHOLD) &&
      (old_size > 31 * TINY_QUANTUM)) {
	if (try_realloc_small_in_place(szone, ptr, old_size, new_size)) {
	    return ptr;
	}

	/*
	 * If the allocation's a large or huge allocation, try to reallocate in-place there.
	 */
    } else if (!((szone->debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES) && PROTECT_SMALL) && (old_size > LARGE_THRESHOLD)) {
	if (try_realloc_large_or_huge_in_place(szone, ptr, old_size, new_size)) {
	    return ptr;
	}
    }

    /*
     * Can't reallocate in place for whatever reason; allocate a new buffer and copy.
     */
    new_ptr = szone_malloc(szone, new_size);
    if (new_ptr == NULL)
	return NULL;

    /*
     * If the allocation's large enough, try to copy using VM.  If that fails, or
     * if it's too small, just copy by hand.
     */
    if ((old_size < VM_COPY_THRESHOLD) ||
      vm_copy(mach_task_self(), (vm_address_t)ptr, old_size, (vm_address_t)new_ptr))
	memcpy(new_ptr, ptr, old_size);
    szone_free(szone, ptr);
    
#if DEBUG_MALLOC
    if (LOG(szone, ptr)) {
	malloc_printf("szone_realloc returned %p for %d\n", new_ptr, (unsigned)new_size);
    }
#endif
    return new_ptr;
}

// given a size, returns pointers capable of holding that size
// returns the number of pointers allocated
// may return 0 - this function will do best attempts, but just that
static unsigned
szone_batch_malloc(szone_t *szone, size_t size, void **results, unsigned count)
{
    msize_t		msize = TINY_MSIZE_FOR_BYTES(size + TINY_QUANTUM - 1);
    size_t		chunk_size = TINY_BYTES_FOR_MSIZE(msize);
    free_list_t		**free_list = szone->tiny_free_list + msize - 1;
    free_list_t		*ptr = *free_list;
    unsigned		found = 0;

    if (size > 31*TINY_QUANTUM)
	return 0; // only bother implementing this for tiny
    if (!msize)
	msize = 1;
    CHECK(szone, __PRETTY_FUNCTION__);
    SZONE_LOCK(szone); // might as well lock right here to avoid concurrency issues
    while (found < count) {
	if (!ptr)
	    break;
	*results++ = ptr;
	found++;
	set_tiny_meta_header_in_use(ptr, msize);
	ptr = ptr->next;
    }
    if (ptr) {
	ptr->previous = NULL;
	free_list_set_checksum(szone, ptr);
    }
    *free_list = ptr;

    // Note that we could allocate from the free lists for larger msize
    // But that may un-necessarily fragment - so we might as well let the client do that
    // We could also allocate from szone->tiny_bytes_free_at_end
    // But that means we'll "eat-up" the untouched area faster, increasing the working set
    // So we just return what we have and just that
    szone->num_tiny_objects += found;
    szone->num_bytes_in_tiny_objects += chunk_size * found;
    SZONE_UNLOCK(szone);
    return found;
}

static void
szone_batch_free(szone_t *szone, void **to_be_freed, unsigned count)
{
    unsigned		cc = 0;
    void		*ptr;
    tiny_region_t	*tiny_region;
    boolean_t		is_free;
    msize_t		msize;

    // frees all the pointers in to_be_freed
    // note that to_be_freed may be overwritten during the process
    if (!count)
	return;
    CHECK(szone, __PRETTY_FUNCTION__);
    SZONE_LOCK(szone);
    while (cc < count) {
	ptr = to_be_freed[cc];
	/* XXX this really slows us down */
	tiny_region = tiny_region_for_ptr_no_lock(szone, ptr);
	if (tiny_region) {
	    // this is a tiny pointer
	    msize = get_tiny_meta_header(ptr, &is_free);
	    if (is_free)
		break; // a double free; let the standard free deal with it
	    tiny_free_no_lock(szone, tiny_region, ptr, msize);
	    to_be_freed[cc] = NULL;
	}
	cc++;
    }
    SZONE_UNLOCK(szone);
    CHECK(szone, __PRETTY_FUNCTION__);
    while (count--) {
	ptr = to_be_freed[count];
	if (ptr)
	    szone_free(szone, ptr);
    }
}

static void
szone_destroy(szone_t *szone)
{
    unsigned		index;
    small_region_t	pended_region = 0;
    large_entry_t	*large;
    vm_range_t		range_to_deallocate;
    huge_entry_t	*huge;
    tiny_region_t	tiny_region;
    small_region_t	small_region;

    /* destroy large entries */
    index = szone->num_large_entries;
    while (index--) {
	large = szone->large_entries + index;
	if (!LARGE_ENTRY_IS_EMPTY(*large)) {
	    // we deallocate_pages, including guard pages
	    deallocate_pages(szone, (void *)LARGE_ENTRY_ADDRESS(*large), LARGE_ENTRY_SIZE(*large), szone->debug_flags);
	}
    }
    if (szone->num_large_entries * sizeof(large_entry_t) >= LARGE_THRESHOLD) {
	// we do not free in the small chunk case
	large_entries_free_no_lock(szone, szone->large_entries, szone->num_large_entries, &range_to_deallocate);
	if (range_to_deallocate.size)
	    deallocate_pages(szone, (void *)range_to_deallocate.address, (size_t)range_to_deallocate.size, 0);
    }

    /* destroy huge entries */
    index = szone->num_huge_entries;
    while (index--) {
	huge = szone->huge_entries + index;
	deallocate_pages(szone, (void *)huge->address, huge->size, szone->debug_flags);
    }
    
    /* destroy tiny regions */
    index = szone->num_tiny_regions;
    while (index--) {
        tiny_region = szone->tiny_regions[index];
	deallocate_pages(szone, TINY_REGION_ADDRESS(tiny_region), TINY_REGION_SIZE, 0);
    }
    /* destroy small regions; region 0 must go last as it contains the szone */
    index = szone->num_small_regions;
    while (index--) {
        small_region = szone->small_regions[index];
	/*
	 * If we've allocated more than the basic set of small regions, avoid destroying the
	 * region that contains the array.
	 */
	if ((index > 0) &&
	  (SMALL_REGION_FOR_PTR(szone->small_regions) == SMALL_REGION_ADDRESS(small_region))) {
		pended_region = small_region;
	} else {
		deallocate_pages(szone, (void *)SMALL_REGION_ADDRESS(small_region), SMALL_REGION_SIZE, 0);
	}
    }
    if (pended_region)
        deallocate_pages(NULL, (void *)SMALL_REGION_ADDRESS(pended_region), SMALL_REGION_SIZE, 0);
}

static size_t
szone_good_size(szone_t *szone, size_t size)
{
    msize_t	msize;
    unsigned	num_pages;
    
    if (size <= 31 * TINY_QUANTUM) {
	// think tiny
	msize = TINY_MSIZE_FOR_BYTES(size + TINY_QUANTUM - 1);
	if (! msize) msize = 1;
	return TINY_BYTES_FOR_MSIZE(msize);
    }
    if (!((szone->debug_flags & SCALABLE_MALLOC_ADD_GUARD_PAGES) && PROTECT_SMALL) && (size < LARGE_THRESHOLD)) {
	// think small
	msize = SMALL_MSIZE_FOR_BYTES(size + SMALL_QUANTUM - 1);
	if (! msize) msize = 1;
	return SMALL_BYTES_FOR_MSIZE(msize);
    } else {
	num_pages = round_page(size) >> vm_page_shift;
	if (!num_pages)
	    num_pages = 1; // minimal allocation size for this
	return num_pages << vm_page_shift;
    }
}

unsigned szone_check_counter = 0;
unsigned szone_check_start = 0;
unsigned szone_check_modulo = 1;

static boolean_t
szone_check_all(szone_t *szone, const char *function)
{
    int			index;
    tiny_region_t	*tiny;
    small_region_t	*small;
    
    SZONE_LOCK(szone);
    CHECK_LOCKED(szone, __PRETTY_FUNCTION__);

    /* check tiny regions - chould check region count */
    for (index = szone->num_tiny_regions - 1, tiny = szone->tiny_regions;
	 index >= 0;
	 index--, tiny++) {
	if (!tiny_check_region(szone, tiny)) {
	    SZONE_UNLOCK(szone);
	    szone->debug_flags &= ~ CHECK_REGIONS;
	    malloc_printf("*** tiny region %d incorrect szone_check_all(%s) counter=%d\n",
		szone->num_tiny_regions - index, function, szone_check_counter);
	    szone_error(szone, "check: tiny region incorrect", NULL);
	    return 0;
	}
    }

    for (index = NUM_TINY_SLOTS - 1; index >= 0; index--) {
	if (!tiny_free_list_check(szone, index)) {
	    SZONE_UNLOCK(szone);
	    szone->debug_flags &= ~ CHECK_REGIONS;
	    malloc_printf("*** tiny free list incorrect (slot=%d) szone_check_all(%s) counter=%d\n",
	      index, function, szone_check_counter);
	    szone_error(szone, "check: tiny free list incorrect", NULL);
	    return 0;
	}
    }

    /* check small regions - could check region count */
    for (index = szone->num_small_regions - 1, small = szone->small_regions;
	 index >= 0;
	 index--, small++) {
	if (!szone_check_small_region(szone, small)) {
	    SZONE_UNLOCK(szone);
	    szone->debug_flags &= ~ CHECK_REGIONS;
	    malloc_printf("*** small region %d incorrect szone_check_all(%s) counter=%d\n",
	      szone->num_small_regions, index, function, szone_check_counter);
	    szone_error(szone, "check: small region incorrect", NULL);
	    return 0;
	}
    }
    for (index = NUM_SMALL_SLOTS - 1; index >= 0; index--) {
	if (!small_free_list_check(szone, index)) {
	    SZONE_UNLOCK(szone);
	    szone->debug_flags &= ~ CHECK_REGIONS;
	    malloc_printf("*** small free list incorrect (grain=%d) szone_check_all(%s) counter=%d\n", index, function, szone_check_counter);
	    szone_error(szone, "check: small free list incorrect", NULL);
	    return 0;
	}
    }
    SZONE_UNLOCK(szone);
    // szone_print(szone, 1);
    return 1;
}

static boolean_t
szone_check(szone_t *szone)
{

    if ((++szone_check_counter % 10000) == 0)
	malloc_printf("at szone_check counter=%d\n", szone_check_counter);
    if (szone_check_counter < szone_check_start)
	return 1;
    if (szone_check_counter % szone_check_modulo)
	return 1;
    return szone_check_all(szone, "");
}

static kern_return_t
szone_ptr_in_use_enumerator(task_t task, void *context, unsigned type_mask, vm_address_t zone_address, memory_reader_t reader, vm_range_recorder_t recorder)
{
    szone_t		*szone;
    kern_return_t	err;
    
    if (!reader) reader = _szone_default_reader;
    err = reader(task, zone_address, sizeof(szone_t), (void **)&szone);
    if (err) return err;
    err = tiny_in_use_enumerator(task, context, type_mask,
      (vm_address_t)szone->tiny_regions, szone->num_tiny_regions, szone->tiny_bytes_free_at_end , reader, recorder);
    if (err) return err;
    err = small_in_use_enumerator(task, context, type_mask,
      (vm_address_t)szone->small_regions, szone->num_small_regions, szone->small_bytes_free_at_end , reader, recorder);
    if (err) return err;
    err = large_in_use_enumerator(task, context, type_mask,
      (vm_address_t)szone->large_entries, szone->num_large_entries, reader,
      recorder);
    if (err) return err;
    err = huge_in_use_enumerator(task, context, type_mask,
      (vm_address_t)szone->huge_entries, szone->num_huge_entries, reader,
      recorder);
    return err;
}

// Following method is deprecated:  use scalable_zone_statistics instead
void
scalable_zone_info(malloc_zone_t *zone, unsigned *info_to_fill, unsigned count)
{
    szone_t	*szone = (void *)zone;
    unsigned	info[13];
    
    // We do not lock to facilitate debug
    info[4] = szone->num_tiny_objects;
    info[5] = szone->num_bytes_in_tiny_objects;
    info[6] = szone->num_small_objects;
    info[7] = szone->num_bytes_in_small_objects;
    info[8] = szone->num_large_objects_in_use;
    info[9] = szone->num_bytes_in_large_objects;
    info[10] = szone->num_huge_entries;
    info[11] = szone->num_bytes_in_huge_objects;
    info[12] = szone->debug_flags;
    info[0] = info[4] + info[6] + info[8] + info[10];
    info[1] = info[5] + info[7] + info[9] + info[11];
    info[3] = szone->num_tiny_regions * TINY_REGION_SIZE + szone->num_small_regions * SMALL_REGION_SIZE + info[9] + info[11];
    info[2] = info[3] - szone->tiny_bytes_free_at_end - szone->small_bytes_free_at_end;
    memcpy(info_to_fill, info, sizeof(unsigned)*count);
}

static void
szone_print(szone_t *szone, boolean_t verbose)
{
    unsigned		info[13];
    unsigned		index = 0;
    tiny_region_t	*region;
    
    SZONE_LOCK(szone);
    scalable_zone_info((void *)szone, info, 13);
    malloc_printf("Scalable zone %p: inUse=%d(%y) touched=%y allocated=%y flags=%d\n",
      szone, info[0], info[1], info[2], info[3], info[12]);
    malloc_printf("\ttiny=%d(%y) small=%d(%y) large=%d(%y) huge=%d(%y)\n",
      info[4], info[5], info[6], info[7], info[8], info[9], info[10], info[11]);
    // tiny
    malloc_printf("%d tiny regions: \n", szone->num_tiny_regions);
    while (index < szone->num_tiny_regions) {
	region = szone->tiny_regions + index;
	print_tiny_region(verbose, *region, (index == szone->num_tiny_regions - 1) ? szone->tiny_bytes_free_at_end : 0);
	index++;
    }
    if (verbose) print_tiny_free_list(szone);
    // small
    malloc_printf("%d small regions: \n", szone->num_small_regions);
    index = 0; 
    while (index < szone->num_small_regions) {
	region = szone->small_regions + index;
	print_small_region(szone, verbose, region, (index == szone->num_small_regions - 1) ? szone->small_bytes_free_at_end : 0);
	index++;
    }
    if (verbose)
	print_small_free_list(szone);
    SZONE_UNLOCK(szone);
}

static void
szone_log(malloc_zone_t *zone, void *log_address)
{
    szone_t	*szone = (szone_t *)zone;

    szone->log_address = log_address;
}

static void
szone_force_lock(szone_t *szone)
{
    SZONE_LOCK(szone);
}

static void
szone_force_unlock(szone_t *szone)
{
    SZONE_UNLOCK(szone);
}

boolean_t
scalable_zone_statistics(malloc_zone_t *zone, malloc_statistics_t *stats, unsigned subzone)
{
    szone_t *szone = (szone_t *)zone;
    
    switch (subzone) {
	case 0:	
	    stats->blocks_in_use = szone->num_tiny_objects;
	    stats->size_in_use = szone->num_bytes_in_tiny_objects;
	    stats->size_allocated = szone->num_tiny_regions * TINY_REGION_SIZE;
	    stats->max_size_in_use = stats->size_allocated - szone->tiny_bytes_free_at_end;
	    return 1;
	case 1:	
	    stats->blocks_in_use = szone->num_small_objects;
	    stats->size_in_use = szone->num_bytes_in_small_objects;
	    stats->size_allocated = szone->num_small_regions * SMALL_REGION_SIZE;
	    stats->max_size_in_use = stats->size_allocated - szone->small_bytes_free_at_end;
	    return 1;
	case 2:
	    stats->blocks_in_use = szone->num_large_objects_in_use;
	    stats->size_in_use = szone->num_bytes_in_large_objects;
	    stats->max_size_in_use = stats->size_allocated = stats->size_in_use;
	    return 1;
	case 3:
	    stats->blocks_in_use = szone->num_huge_entries;
	    stats->size_in_use = szone->num_bytes_in_huge_objects;
	    stats->max_size_in_use = stats->size_allocated = stats->size_in_use;
	    return 1;
    }
    return 0;
}

static void
szone_statistics(szone_t *szone, malloc_statistics_t *stats)
{
    size_t	big_and_huge;
    
    stats->blocks_in_use =
      szone->num_tiny_objects +
      szone->num_small_objects +
      szone->num_large_objects_in_use +
      szone->num_huge_entries;
    big_and_huge = szone->num_bytes_in_large_objects + szone->num_bytes_in_huge_objects;
    stats->size_in_use = szone->num_bytes_in_tiny_objects + szone->num_bytes_in_small_objects + big_and_huge;
    stats->max_size_in_use = stats->size_allocated =
      szone->num_tiny_regions * TINY_REGION_SIZE +
      szone->num_small_regions * SMALL_REGION_SIZE +
      big_and_huge ; 

    // Now we account for the untouched areas
    stats->max_size_in_use -= szone->tiny_bytes_free_at_end;
    stats->max_size_in_use -= szone->small_bytes_free_at_end;
}

static const struct malloc_introspection_t szone_introspect = {
    (void *)szone_ptr_in_use_enumerator,
    (void *)szone_good_size,
    (void *)szone_check,
    (void *)szone_print,
    szone_log,
    (void *)szone_force_lock,
    (void *)szone_force_unlock,
    (void *)szone_statistics
}; // marked as const to spare the DATA section

malloc_zone_t *
create_scalable_zone(size_t initial_size, unsigned debug_flags)
{
    szone_t		*szone;
    size_t		msize;
    size_t		msize_used;
    msize_t		free_msize;

    /*
     * Sanity-check our build-time assumptions about the size of a page.
     * Since we have sized various things assuming the default page size,
     * attempting to determine it dynamically is not useful.
     */
    if ((vm_page_size != _vm_page_size) || (vm_page_shift != _vm_page_shift)) {
	malloc_printf("*** FATAL ERROR - machine page size does not match our assumptions.\n");
	exit(-1);
    }

    /* get memory for the zone */
    szone = allocate_pages(NULL, SMALL_REGION_SIZE, SMALL_BLOCKS_ALIGN, 0, VM_MAKE_TAG(VM_MEMORY_MALLOC));
    if (!szone)
	return NULL;
    /* set up the szone structure */
    szone->tiny_regions = szone->initial_tiny_regions;
    szone->small_regions = szone->initial_small_regions;
    msize_used = msize; szone->num_small_objects++;
    szone->basic_zone.version = 3;
    szone->basic_zone.size = (void *)szone_size;
    szone->basic_zone.malloc = (void *)szone_malloc;
    szone->basic_zone.calloc = (void *)szone_calloc;
    szone->basic_zone.valloc = (void *)szone_valloc;
    szone->basic_zone.free = (void *)szone_free;
    szone->basic_zone.realloc = (void *)szone_realloc;
    szone->basic_zone.destroy = (void *)szone_destroy;
    szone->basic_zone.batch_malloc = (void *)szone_batch_malloc;
    szone->basic_zone.batch_free = (void *)szone_batch_free;
    szone->basic_zone.introspect = (struct malloc_introspection_t *)&szone_introspect;
    szone->debug_flags = debug_flags;

    /* as the szone is allocated out of the first tiny, region, reflect that allocation */
    szone->small_regions[0] = szone;
    szone->num_small_regions = 1;
    msize = SMALL_MSIZE_FOR_BYTES(sizeof(szone_t) + SMALL_QUANTUM - 1);
    free_msize = NUM_SMALL_BLOCKS - msize;
    *SMALL_METADATA_FOR_PTR(szone) = msize;
    *(SMALL_METADATA_FOR_PTR(szone) + msize) = free_msize;
    szone->small_bytes_free_at_end = SMALL_BYTES_FOR_MSIZE(free_msize);

    LOCK_INIT(szone->lock);
#if 0
#warning CHECK_REGIONS enabled
    debug_flags |= CHECK_REGIONS;
#endif
#if 0
#warning LOG enabled
    szone->log_address = ~0;
#endif
    CHECK(szone, __PRETTY_FUNCTION__);
    return (malloc_zone_t *)szone;
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
 * 4) Fresher malloc with tiny zone
 * 5) 32/64bit compatible malloc
 *
 * No version backward compatibility is provided, but the version number does
 * make it possible for malloc_jumpstart() to return an error if the application
 * was freezedried with an older version of malloc.
 */
#define MALLOC_FREEZEDRY_VERSION 5

typedef struct {
    unsigned version;
    unsigned nszones;
    szone_t *szones;
} malloc_frozen;

static void *
frozen_malloc(szone_t *zone, size_t new_size)
{
    return malloc(new_size);
}

static void *
frozen_calloc(szone_t *zone, size_t num_items, size_t size)
{
    return calloc(num_items, size);
}

static void *
frozen_valloc(szone_t *zone, size_t new_size)
{
    return valloc(new_size);
}

static void *
frozen_realloc(szone_t *zone, void *ptr, size_t new_size)
{
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

static void
frozen_free(szone_t *zone, void *ptr)
{
}

static void
frozen_destroy(szone_t *zone)
{
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

uintptr_t
malloc_freezedry(void)
{
    extern unsigned malloc_num_zones;
    extern malloc_zone_t **malloc_zones;
    malloc_frozen *data;
    unsigned i;

    /* Allocate space in which to store the freezedry state. */
    data = (malloc_frozen *) malloc(sizeof(malloc_frozen));

    /* Set freezedry version number so that malloc_jumpstart() can check for compatibility. */
    data->version = MALLOC_FREEZEDRY_VERSION;

    /* Allocate the array of szone pointers. */
    data->nszones = malloc_num_zones;
    data->szones = (szone_t *) calloc(malloc_num_zones, sizeof(szone_t));

    /*
     * Fill in the array of szone structures.  They are copied rather than
     * referenced, since the originals are likely to be clobbered during malloc
     * initialization.
     */
    for (i = 0; i < malloc_num_zones; i++) {
	if (strcmp(malloc_zones[i]->zone_name, "DefaultMallocZone")) {
	    /* Unknown zone type. */
	    free(data->szones);
	    free(data);
	    return 0;
	}
	memcpy(&data->szones[i], malloc_zones[i], sizeof(szone_t));
    }

    return((uintptr_t)data);
}

int
malloc_jumpstart(uintptr_t cookie)
{
    malloc_frozen *data = (malloc_frozen *)cookie;
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
	data->szones[i].basic_zone.introspect = (struct malloc_introspection_t *)&szone_introspect;

	/* Register the freezedried zone. */
	malloc_zone_register(&data->szones[i].basic_zone);
    }

    return 0;
}

