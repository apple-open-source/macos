/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#if CONFIG_EARLY_MALLOC

#if MALLOC_TARGET_EXCLAVES_INTROSPECTOR
#pragma GCC diagnostic ignored "-Wunused-function"
#endif // MALLOC_TARGET_EXCLAVES_INTROSPECTOR

/*
 * An arena looks like this:
 *
 * +------+------+------------------------------------------------+
 * | hdr  | bits | blocks...                                      |
 * +------+------+------------------------------------------------+
 *
 * There are 2 bits per block, where a block is 16 bytes.
 *
 * For sanity we want a number of blocks that is a multiple of 64,
 * which gives us a granularity of (2 * sizeof(uint64_t) + MFM_QUANTUM * 64)
 * increments.
 */

#define MFM_TRACE           0
#if MALLOC_TARGET_EXCLAVES || MALLOC_TARGET_EXCLAVES_INTROSPECTOR
#define MFM_ARENA_SIZE      (1ul << 20)
#else
#define MFM_ARENA_SIZE      (8ul << 20)
#endif /* MALLOC_TARGET_EXCLAVES || MALLOC_TARGET_EXCLAVES_INTROSPECTOR */
#define MFM_QUANTUM         16ul
#define MFM_SIZE_CLASSES    (__builtin_ctz(MFM_ALLOC_SIZE_MAX / MFM_QUANTUM) + 1)
#define MFM_USABLE_SIZE     (MFM_ARENA_SIZE - sizeof(struct mfm_header) - 4 * sizeof(uint64_t))

/* compute the maximum number of blocks we can have, aligned to a page */
#define MFM_GRANULE         (PAGE_MAX_SIZE / MFM_QUANTUM)
#define MFM_RATIO           (MFM_GRANULE / 4 + MFM_QUANTUM * MFM_GRANULE)
#define MFM_BLOCKS_COUNT    (MFM_GRANULE * (MFM_USABLE_SIZE / MFM_RATIO))
#define MFM_BLOCKS_SIZE     (MFM_BLOCKS_COUNT * MFM_QUANTUM)

#define MFM_BITS_COUNT      (2 * howmany(MFM_BLOCKS_COUNT, 64))
#define MFM_BITS_SIZE       (MFM_BITS_COUNT * sizeof(uint64_t))

#define MFM_PADDING_SIZE    (MFM_USABLE_SIZE - MFM_BLOCKS_SIZE - MFM_BITS_SIZE)

#define MFM_BLOCK_SIZE_MAX  (MFM_ARENA_SIZE / MFM_QUANTUM)
#define MFM_BLOCK_SIZE_BITS (63 - __builtin_clzl(MFM_BLOCK_SIZE_MAX))

static_assert(CHAR_BIT == 8, "CHAR_BIT is 8");
static_assert(powerof2(MFM_ALLOC_SIZE_MAX), "MFM_ALLOC_SIZE_MAX is a power of 2");
static_assert(powerof2(MFM_BLOCK_SIZE_MAX), "MFM_BLOCK_SIZE_MAX is a power of 2");

#if !MALLOC_TARGET_EXCLAVES
#define MFM_INTERNAL_CRASH(code, msg)  ({ \
	_os_set_crash_log_cause_and_message(code, "BUG IN LIBMALLOC: " msg); \
	__builtin_trap(); \
})

#define MFM_CLIENT_CRASH(code, msg)  ({ \
	_os_set_crash_log_cause_and_message(code, \
			"BUG IN CLIENT OF LIBMALLOC: " msg); \
	__builtin_trap(); \
})
#else
#define MFM_INTERNAL_CRASH(code, msg) \
	__liblibc_fatal_error("BUG IN LIBMALLOC (%llu): " msg, (uint64_t)code)

#define MFM_CLIENT_CRASH(code, msg) \
	__liblibc_fatal_error("BUG IN CLIENT OF LIBMALLOC (%llu): " msg, (uint64_t)code)
#endif // !MALLOC_TARGET_EXCLAVES

struct mfm_block {
#if __has_feature(ptrauth_calls)
	void *__ptrauth(ptrauth_key_process_dependent_data, true,
			ptrauth_string_discriminator("mfmb_next"),
			"authenticates-null-values")
	                        mfmb_next;
#else
	uint64_t                mfmb_next;
#endif
	uint64_t                mfmb_prev;
} __attribute__((aligned(MFM_QUANTUM)));

struct mfm_header {
	_malloc_lock_s          mfm_lock;
	uint32_t                mfm_size;
	size_t                  mfm_bump;
	size_t                  mfm_bump_hwm;
	size_t                  mfm_alloc_count;
	struct mfm_block        mfm_freelist[MFM_SIZE_CLASSES];
#if MALLOC_TARGET_EXCLAVES || MALLOC_TARGET_EXCLAVES_INTROSPECTOR
	plat_map_t              mfm_map;
#endif // MALLOC_TARGET_EXCLAVES || MALLOC_TARGET_EXCLAVES_INTROSPECTOR
};

struct mfm_arena {
	struct mfm_block        mfm_base[0];
	struct mfm_header       mfm_header;
	/*
	 * The bit map goes in pairs of 64 bits.
	 *
	 * The first word (even indices) represent block starts
	 * at the quantum level, whether the blocks are allocated,
	 * or free.
	 *
	 * The second bitmap has a representation that is dependent
	 * on the size of the block.
	 *
	 * Assuming a block of N quanta, and a bitmap of N bits
	 * from b(0) through b(n-1):
	 *
	 * - b(0) and b(n-1) [possibly collapsed] represent whether
	 *   the block is allocated (1) or free (0)
	 *
	 * - if the block is longer than 64 bits, then b(1..MFM_BLOCK_SIZE_BITS)
	 *   and b(n-MFM_BLOCK_SIZE_BITS-2..b-2) both contain the length - 1
	 *   of the block.
	 */
	uint64_t                mfm_before[2];
	uint64_t                mfm_bits[MFM_BITS_COUNT];
	uint64_t                mfm_after[2];
	uint8_t                 padding[MFM_PADDING_SIZE];
	struct mfm_block        mfm_blocks[MFM_BLOCKS_COUNT];
	struct mfm_block        mfm_end[0];
};
static_assert(sizeof(struct mfm_arena) == MFM_ARENA_SIZE, "I can do math");

/*
 * Inelegant ways to not have to spell out mfm_header everywhere.
 */
#define mfmh_lock           mfm_header.mfm_lock
#define mfmh_size           mfm_header.mfm_size
#define mfmh_bump           mfm_header.mfm_bump
#define mfmh_bump_hwm       mfm_header.mfm_bump_hwm
#define mfmh_alloc_count    mfm_header.mfm_alloc_count
#define mfmh_freelist       mfm_header.mfm_freelist

static struct mfm_arena    *mfm_arena;

#pragma mark validation and helper functions

/*!
 * @brief
 * Returns whether the pointer belongs to the MFM allocator
 *
 * @discussion
 * No precondition.
 */
static inline bool
__mfm_address_owned(struct mfm_arena *arena, const void *ptr)
{
	const struct mfm_block *blk = ptr;

	return arena && blk >= arena->mfm_blocks && blk < arena->mfm_end;
}

/*!
 * @brief
 * Returns the block index for a pointer (index within @c mfm_blocks).
 *
 * @discussion
 * The pointer must belong to MFM (@c __mfm_address_owned() returns true).
 */
static inline size_t
__mfm_block_index(struct mfm_arena *arena, const void *ptr)
{
	if ((uintptr_t)ptr % sizeof(struct mfm_block)) {
		MFM_CLIENT_CRASH(ptr, "invalid address");
	}
	return (size_t)((struct mfm_block *)ptr - arena->mfm_blocks);
}

/*!
 * @function __mfm_block_is_allocated()
 *
 * @brief
 * Returns whether a block index is an allocated block.
 *
 * @discussion
 * The index must have been returned by @c __mfm_block_index()
 */
static inline bool
__mfm_block_is_allocated(struct mfm_arena *arena, size_t index)
{
	uint64_t *bits = arena->mfm_bits;
	size_t word = index >> 6;
	size_t bit  = index & 63;

	return bits[2 * word] & bits[2 * word + 1] & (1ull << bit);
}

/*!
 * @function __mfm_prev_block_is_allocated()
 *
 * @brief
 * Returns whether a block index follows an allocated block.
 *
 * @discussion
 * The index must have been returned by @c __mfm_block_index()
 * The index must be a block start.
 */
static inline bool
__mfm_prev_block_is_allocated(struct mfm_arena *arena, size_t index)
{
	uint64_t *bits = arena->mfm_bits;
	ssize_t word = ((ssize_t)index - 1) >> 6;
	size_t  bit  = (index - 1) & 63;

	/* this might read in mfm_before[1] */
	return bits[2 * word + 1] & (1ull << bit);
}

/*!
 * @function __mfm_block_size()
 *
 * @brief
 * Returns a block size.
 *
 * @discussion
 * The index must have been returned by @c __mfm_block_index()
 * The index must be a block start.
 */
static inline size_t
__mfm_block_size(struct mfm_arena *arena, size_t index)
{
	size_t word = (index + 1) >> 6;
	size_t bit  = (index + 1) & 63;
	__uint128_t bits128;
	uint64_t bits64;

	/*
	 * we want the distance from b to 1 (and b is 1).
	 * [.....b00000] [000001.....]
	 *        ^
	 *        bit
	 */
	bits64 = arena->mfm_bits[2 * word] >> bit;
	if (__probable(bits64)) {
		/* bits64 looks like [000001....] */
		return __builtin_ctzll(bits64) + 1;
	}

	bits64 = arena->mfm_bits[2 * word + 2];
	if (__probable(bits64)) {
		/* word0 is [000001.......] */
		return 65 + __builtin_ctzll(bits64) - bit;
	}

	/*
	 * we want to extract the length placed "after" b:
	 *
	 * word0         word1
	 * [.....bxxxxx] [xxxxx......]
	 *        ^
	 *        bit
	 */
	bits128 = arena->mfm_bits[2 * word + 1];
	if (bit + MFM_BLOCK_SIZE_BITS > 64) {
		bits128 |= (__uint128_t)arena->mfm_bits[2 * word + 3] << 64;
	}
	bits128 >>= bit;

	return (bits128 & (MFM_BLOCK_SIZE_MAX - 1)) + 1;
}

/*!
 * @function __mfm_prev_block_size()
 *
 * @brief
 * Returns the previous block size.
 *
 * @discussion
 * The index must have been returned by @c __mfm_block_index()
 * The index must be a block start.
 * The previous block must be free (@c __mfm_prev_block_is_allocated() returns false)
 */
static inline size_t
__mfm_prev_block_size(struct mfm_arena *arena, size_t index)
{
	ssize_t word = ((ssize_t)index - 1) >> 6;
	size_t  bit  = (index - 1) & 63;
	__uint128_t bits128;
	uint64_t bits64;

	/*
	 * we want the distance from 1 to b (and b is 0 or collapsed with the 1).
	 *
	 * word0         word1
	 * [.....100000] [00000b.....]
	 *                     ^
	 *                   bit
	 */
	bits64 = arena->mfm_bits[2 * word] << (63 - bit);
	if (__probable(bits64)) {
		/* bits64 looks like [....10000000b] */
		return __builtin_clzll(bits64) + 1;
	}

	bits64 = arena->mfm_bits[2 * word - 2];
	if (__probable(bits64)) {
		/* bits64 looks like [.......1000000] */
		return __builtin_clzll(bits64) + bit + 2;
	}

	/*
	 * we want to decode the size laid down before b (and b is 0).
	 *
	 * word0         word1
	 * [.....xxxxxx] [xxxxxb.....]
	 *                     ^
	 *                   bit
	 */
	bits128 = (__uint128_t)arena->mfm_bits[2 * word + 1] << 64;
	if (bit < MFM_BLOCK_SIZE_BITS) {
		bits128 |= arena->mfm_bits[2 * word - 1];
	}
	bits128 >>= (64 + bit - MFM_BLOCK_SIZE_BITS);

	return (bits128 & (MFM_BLOCK_SIZE_MAX - 1)) + 1;
}

/*!
 * @function __mfm_block_clear_start()
 *
 * @brief
 * Clears the "block start" bit for this block.
 *
 * @discussion
 * This function leaves the "allocated" and possible "size" bits untouched.
 */
static inline void
__mfm_block_clear_start(struct mfm_arena *arena, size_t index)
{
	size_t word = index >> 6;
	size_t bit  = index & 63;

	arena->mfm_bits[2 * word] &= ~(1ull << bit);
}

/*!
 * @function __mfm_block_mark_start()
 *
 * @brief
 * Sets the "block start" bit for this block.
 *
 * @discussion
 * This function leaves the "allocated" and possible "size" bits untouched.
 */
static inline void
__mfm_block_mark_start(struct mfm_arena *arena, size_t index)
{
	size_t word = index >> 6;
	size_t bit  = index & 63;

	arena->mfm_bits[2 * word] |= 1ull << bit;
}

static inline void
__mfm_block_set_sizes(struct mfm_arena *arena, bool allocated, size_t index, size_t size)
{
	__uint128_t mask, bits;
	size_t word, bit;

	word = index >> 6;
	bit  = index & 63;

	mask = 2 * MFM_BLOCK_SIZE_MAX - 1;
	bits = 2 * (size - 1) + allocated;
	mask = mask << bit;
	bits = bits << bit;

	arena->mfm_bits[2 * word + 1] &= ~(uint64_t)(mask >> 0);
	arena->mfm_bits[2 * word + 1] |= (uint64_t)(bits >> 0);
	if (bit + MFM_BLOCK_SIZE_BITS >= 64) {
		arena->mfm_bits[2 * word + 3] &= ~(uint64_t)(mask >> 64);
		arena->mfm_bits[2 * word + 3] |= (uint64_t)(bits >> 64);
	}

	word = (index + size - 1) >> 6;
	bit  = (index + size - 1) & 63;

	mask = 2 * MFM_BLOCK_SIZE_MAX - 1;
	bits = allocated * MFM_BLOCK_SIZE_MAX + (size - 1);
	mask = mask << (64 + bit - MFM_BLOCK_SIZE_BITS);
	bits = bits << (64 + bit - MFM_BLOCK_SIZE_BITS);

	if (64 + bit + 1 - MFM_BLOCK_SIZE_BITS <= 64) {
		arena->mfm_bits[2 * word - 1] &= ~(uint64_t)(mask >> 0);
		arena->mfm_bits[2 * word - 1] |= (uint64_t)(bits >> 0);
	}
	arena->mfm_bits[2 * word + 1] &= ~(uint64_t)(mask >> 64);
	arena->mfm_bits[2 * word + 1] |= (uint64_t)(bits >> 64);
}

/*!
 * @function __mfm_block_mark_free()
 *
 * @brief
 * Marks the block as "free" and sets its size if need be.
 *
 * @discussion
 * The "block start" bits aren't updated.
 */
static inline void
__mfm_block_mark_free(struct mfm_arena *arena, size_t index, size_t size)
{
	size_t word, bit;

	if (__probable(size < 64)) {
		word = index >> 6;
		bit  = index & 63;
		arena->mfm_bits[2 * word + 1] &= ~(1ull << bit);

		word = (index + size - 1) >> 6;
		bit  = (index + size - 1) & 63;
		arena->mfm_bits[2 * word + 1] &= ~(1ull << bit);
	} else {
		__mfm_block_set_sizes(arena, false, index, size);
	}
}

/*!
 * @function MFM_BLOCKS_COUNT()
 *
 * @brief
 * Marks the block as "allocated" and sets its size if need be.
 *
 * @discussion
 * The "block start" bits aren't updated.
 */
static inline void
__mfm_block_mark_allocated(struct mfm_arena *arena, size_t index, size_t size)
{
	size_t word, bit;

	if (__probable(size < 64)) {
		word = index >> 6;
		bit  = index & 63;
		arena->mfm_bits[2 * word + 1] |= 1ull << bit;

		word = (index + size - 1) >> 6;
		bit  = (index + size - 1) & 63;
		arena->mfm_bits[2 * word + 1] |= 1ull << bit;
	} else {
		__mfm_block_set_sizes(arena, true, index, size);
	}
}

/*!
 * @function __mfm_block_offset(0
 *
 * @brief
 * Computes the offset of a block to be used inside the allocator queues.
 */
static inline uint64_t
__mfm_block_offset(struct mfm_arena *arena, struct mfm_block *blk)
{
	return blk - arena->mfm_base;
}

/*!
 * @function __mfm_size_class_down()
 *
 * @brief
 * Computes the size class for a given number of blocks (rounded down).
 */
static uint32_t
__mfm_size_class_down(size_t block_count)
{
	uint32_t sc = 63 - __builtin_clzl(block_count);

	return MIN(sc, MFM_SIZE_CLASSES - 1);
}

/*!
 * @function __mfm_size_class_up()
 *
 * @brief
 * Computes the size class for a given number of blocks (rounded up).
 */
static uint32_t
__mfm_size_class_up(size_t block_count)
{
	if (block_count > 1) {
		uint32_t sc = 64 - __builtin_clzl(block_count - 1);

		return MIN(sc, MFM_SIZE_CLASSES - 1);
	}
	return 0;
}

static uint64_t
__mfm_block_next(struct mfm_block *blk)
{
#if __has_feature(ptrauth_calls)
	return (uint64_t)(void *)blk->mfmb_next;
#else
	return blk->mfmb_next;
#endif
}

static void
__mfm_block_set_next(struct mfm_block *blk, uint64_t next)
{
#if __has_feature(ptrauth_calls)
	blk->mfmb_next = (void *)next;
#else
	blk->mfmb_next = next;
#endif
}


/*!
 * @function __mfm_block_insert_head()
 *
 * @brief
 * Inserts a block onto a queue head.
 */
static inline void
__mfm_block_insert_head(
	struct mfm_arena       *arena,
	struct mfm_block       *hblk,
	struct mfm_block       *blk)
{
	uint64_t head, offs, next;
	struct mfm_block *next_blk;

	head = __mfm_block_offset(arena, hblk);
	next = __mfm_block_next(hblk);
	offs = __mfm_block_offset(arena, blk);
	next_blk = &arena->mfm_base[next];


	blk->mfmb_prev = head;
	__mfm_block_set_next(blk, next);
	__mfm_block_set_next(hblk, offs);
	next_blk->mfmb_prev = offs;
}

/*!
 * @function __mfm_block_remove()
 *
 * @brief
 * Removes a block from a queue (and sets linkages to 0)
 */
static inline void
__mfm_block_remove(struct mfm_arena *arena, struct mfm_block *blk)
{
	uint64_t next, prev;
	struct mfm_block *next_blk, *prev_blk;


	next = __mfm_block_next(blk);
	prev = blk->mfmb_prev;
	next_blk = &arena->mfm_base[next];
	prev_blk = &arena->mfm_base[prev];
	next_blk->mfmb_prev = prev;
	__mfm_block_set_next(prev_blk, next);
	__builtin_bzero(blk, sizeof(struct mfm_block));
}


#pragma mark locking

static inline void
__mfm_lock(struct mfm_arena *arena)
{
	_malloc_lock_lock(&arena->mfmh_lock);
}

static inline void
__mfm_unlock(struct mfm_arena *arena)
{
	_malloc_lock_unlock(&arena->mfmh_lock);
}

void
mfm_lock(void)
{
	struct mfm_arena *arena = os_atomic_load(&mfm_arena, dependency);

	if (arena) {
		__mfm_lock(arena);
	}
}

void
mfm_unlock(void)
{
	struct mfm_arena *arena = os_atomic_load(&mfm_arena, dependency);

	if (arena) {
		__mfm_unlock(arena);
	}
}

void
mfm_reinit_lock(void)
{
	struct mfm_arena *arena = os_atomic_load(&mfm_arena, dependency);

	if (arena) {
		arena->mfmh_lock = _MALLOC_LOCK_INIT;
	}
}


#pragma mark external interface

#if !MALLOC_TARGET_EXCLAVES_INTROSPECTOR
static void
__mfm_free_block(struct mfm_arena *arena, size_t index, size_t size)
{
	struct mfm_block *head = &arena->mfmh_freelist[__mfm_size_class_down(size)];

	__mfm_block_mark_free(arena, index, size);
	__mfm_block_insert_head(arena, head, &arena->mfm_blocks[index]);
}

void
mfm_initialize(void)
{
	struct mfm_arena *arena;
#if MALLOC_TARGET_EXCLAVES
	plat_map_t map = {0};
#endif // MALLOC_TARGET_EXCLAVES


#if MALLOC_TARGET_EXCLAVES
	arena = mvm_allocate_pages_plat(MFM_ARENA_SIZE, 0, MALLOC_NO_POPULATE,
			VM_MEMORY_MALLOC, mvm_plat_map(map));
#else
	/* this is called early, which means the address space _does_ have 8M */
	arena = mvm_allocate_pages_plat(MFM_ARENA_SIZE, 0,
			DISABLE_ASLR | MALLOC_ADD_GUARD_PAGE_FLAGS, VM_MEMORY_MALLOC,
			NULL);
#endif // MALLOC_TARGET_EXCLAVES

	if (arena == NULL) {
		MFM_INTERNAL_CRASH(arena, "failed to allocate memory");
	}

#if MALLOC_TARGET_EXCLAVES
	/* populate the header up to the block storage */
	const uintptr_t addr = (uintptr_t)mvm_allocate_plat((uintptr_t)arena,
			roundup(offsetof(struct mfm_arena, mfm_blocks), PAGE_SIZE), 0,
			VM_FLAGS_FIXED, 0, 0, mvm_plat_map(map));
	if (addr != (uintptr_t)arena) {
		MFM_INTERNAL_CRASH(addr, "populate of header failed");
	}

	arena->mfm_header.mfm_map = map;
#else
	/* to make clear that this region is not purely metadata, we'll now
	 * overwrite the allocation we received with another at the same location
	 * and size using VM_MEMORY_MALLOC_TINY - we couldn't use that tag
	 * originally because the kernel would have placed it in the heap range */
	mach_vm_address_t vm_addr = (mach_vm_address_t)arena;
	mach_vm_size_t vm_size = (mach_vm_size_t)MFM_ARENA_SIZE;
	int alloc_flags = VM_FLAGS_OVERWRITE | VM_MAKE_TAG(VM_MEMORY_MALLOC_TINY);


	kern_return_t kr = mach_vm_map(mach_task_self(), &vm_addr, vm_size,
			/* mask */ 0, alloc_flags, MEMORY_OBJECT_NULL, /* offset */ 0,
			/* copy */ false, VM_PROT_DEFAULT, VM_PROT_ALL, VM_INHERIT_DEFAULT);
	if (kr != KERN_SUCCESS) {
		MFM_INTERNAL_CRASH(kr, "failed to overwrite mfm arena");
	}
#endif // MALLOC_TARGET_EXCLAVES

	arena->mfmh_lock = _MALLOC_LOCK_INIT;

	/*
	 * mfm_before pretend the bitmap is larger
	 * so that the coalescing logic "works".
	 */
	arena->mfm_before[1] = 1ull << 63;

	/*
	 * And now setup the freelist
	 */
	for (uint32_t i = 0; i < MFM_SIZE_CLASSES; i++) {
		struct mfm_block *blk = &arena->mfmh_freelist[i];
		uint64_t offs = __mfm_block_offset(arena, blk);

		__mfm_block_set_next(blk, offs);
		blk->mfmb_prev = offs;
	}

	__mfm_block_mark_start(arena, 0);

	os_atomic_store(&mfm_arena, arena, release);
}

size_t
mfm_alloc_size(const void *ptr)
{
	struct mfm_arena *arena = os_atomic_load(&mfm_arena, dependency);
	size_t index;


	if (!__mfm_address_owned(arena, ptr)) {
		return 0ul;
	}

	if ((uintptr_t)ptr % sizeof(struct mfm_block)) {
		return 0ul;
	}

	index = __mfm_block_index(arena, ptr);
	if (!__mfm_block_is_allocated(arena, index)) {
		return 0ul;
	}

	return MFM_QUANTUM * __mfm_block_size(arena, index);
}

void *
mfm_alloc(size_t alloc_size)
{
	struct mfm_arena *arena = os_atomic_load(&mfm_arena, dependency);
	size_t size;
	void *ptr = NULL;

	if (alloc_size > MFM_ALLOC_SIZE_MAX) {
		return NULL;
	}

	size = alloc_size ? howmany(alloc_size, MFM_QUANTUM) : 1;

	__mfm_lock(arena);

	__builtin_assume(size > 0); /* help the compiler a bit */
	for (uint32_t sc = __mfm_size_class_up(size); sc < MFM_SIZE_CLASSES; sc++) {
		struct mfm_block *head;
		struct mfm_block *blk;

		head = &arena->mfmh_freelist[sc];
		blk  = &arena->mfm_base[__mfm_block_next(head)];

		if (head != blk) {
			size_t blk_size, blk_index;

			__mfm_block_remove(arena, blk);
			blk_index = blk - arena->mfm_blocks;
			blk_size  = __mfm_block_size(arena, blk_index);

			if (blk_size > size) {
				__mfm_block_mark_start(arena, blk_index + size);
				__mfm_free_block(arena, blk_index + size,
				    blk_size - size);
			}

			__mfm_block_mark_allocated(arena, blk_index, size);
			ptr = blk;
			arena->mfmh_size += size;
			arena->mfmh_alloc_count += 1;
			goto out;
		}
	}

	if (size < MFM_BLOCKS_COUNT - mfm_arena->mfmh_bump) {
		size_t index = mfm_arena->mfmh_bump;

		__mfm_block_mark_start(arena, index + size);
		__mfm_block_mark_allocated(arena, index, size);
		mfm_arena->mfmh_bump += size;

		arena->mfmh_size += size;
		arena->mfmh_alloc_count += 1;
		ptr = arena->mfm_blocks + index;

		if (mfm_arena->mfmh_bump_hwm < mfm_arena->mfmh_bump) {
#if MALLOC_TARGET_EXCLAVES
			const uintptr_t begin = roundup(
					(uintptr_t)(arena->mfm_blocks + mfm_arena->mfmh_bump_hwm),
					 PAGE_SIZE);
			const uintptr_t end = roundup((uintptr_t)ptr + alloc_size, PAGE_SIZE);
			const size_t bytes = end - begin;
			if (bytes) {
				const uintptr_t addr = (uintptr_t)mvm_allocate_plat(begin,
						bytes, 0, VM_FLAGS_FIXED, 0, 0,
						mvm_plat_map(arena->mfm_header.mfm_map));
				if (addr != begin) {
					MFM_INTERNAL_CRASH(ptr, "populate of pages failed");
				}
			}
#endif

			mfm_arena->mfmh_bump_hwm = mfm_arena->mfmh_bump;
		}
	}

out:
#if MFM_TRACE
	if (ptr) {
		dprintf(STDERR_FILENO, "{ %zd, %p },\n", alloc_size, ptr);
	}
#endif
	__mfm_unlock(arena);


	return ptr;
}

void
mfm_free(void *ptr)
{
	struct mfm_arena *arena = os_atomic_load(&mfm_arena, dependency);
	size_t index, size;
	void *addr = ptr;

#if MFM_TRACE
	dprintf(STDERR_FILENO, "{ -1, %p },\n", ptr);
#endif


	if (!__mfm_address_owned(arena, addr)) {
		MFM_INTERNAL_CRASH(ptr, "not MFM owned");
	}

	index = __mfm_block_index(arena, addr);
	if (!__mfm_block_is_allocated(arena, index)) {
		MFM_CLIENT_CRASH(ptr, "not an allocated block");
	}
	size = __mfm_block_size(arena, index);


	bzero(ptr, MFM_QUANTUM * size);

	__mfm_lock(arena);

	/* check again that while we dropped the lock, metadata still looks ok */
	if (!__mfm_block_is_allocated(arena, index) ||
			size != __mfm_block_size(arena, index)) {
		MFM_CLIENT_CRASH(ptr, "double free detected");
	}

	arena->mfmh_size -= size;
	arena->mfmh_alloc_count -= 1;

	if (!__mfm_prev_block_is_allocated(arena, index)) {
		size_t psize = __mfm_prev_block_size(arena, index);
		size_t prev  = index - psize;

		__mfm_block_clear_start(arena, index);
		__mfm_block_remove(arena, &arena->mfm_blocks[prev]);

		index -= psize;
		size  += psize;
	}

	if (index + size < arena->mfmh_bump &&
	    !__mfm_block_is_allocated(arena, index + size)) {
		size_t next  = index + size;
		size_t nsize = __mfm_block_size(arena, next);

		__mfm_block_clear_start(arena, next);
		__mfm_block_remove(arena, &arena->mfm_blocks[next]);
		size  += nsize;
	}

	if (index + size == arena->mfmh_bump) {
		__mfm_block_clear_start(arena, index + size);
		__mfm_block_mark_free(arena, index, size);
		arena->mfmh_bump = index;
	} else {
		__mfm_free_block(arena, index, size);
	}

	__mfm_unlock(arena);
}

bool
mfm_claimed_address(void *ptr)
{
	struct mfm_arena *arena = os_atomic_load(&mfm_arena, dependency);


	return __mfm_address_owned(arena, ptr);
}

void *
mfm_zone_address(void)
{
	return os_atomic_load(&mfm_arena, relaxed);
}
#endif // !MALLOC_TARGET_EXCLAVES_INTROSPECTOR

#pragma mark introspection

static void
print_mfm_arena(struct mfm_arena *arena, bool verbose, print_task_printer_t P)
{
	P("mfm_arena info\n");
	P(" address      : %p\n", arena);
	P(" size         : %zd\n", arena->mfmh_size * MFM_QUANTUM);
	P(" high water   : %zd\n", arena->mfmh_bump * MFM_QUANTUM);
	P(" arena        : [%p, %p)\n", arena->mfm_blocks, arena->mfm_end);
	P("\n");

	P("freelists\n");
	for (uint32_t sc = 0; sc < MFM_SIZE_CLASSES; sc++) {
		struct mfm_block *head = &arena->mfmh_freelist[sc];
		struct mfm_block *blk;

		P(" size %-8zd:\n", MFM_QUANTUM << sc);

		for (blk = &arena->mfm_base[__mfm_block_next(head)];
				blk != head;
				blk = &arena->mfm_base[__mfm_block_next(blk)]) {
			size_t index = blk - arena->mfm_blocks;
			size_t size  = __mfm_block_size(arena, index);

			P("  [%p, %p) size=%zd\n",
			    blk, blk + size, size * MFM_QUANTUM);
		}
	}
	P("\n");

	if (verbose) {
		struct mfm_block *blk;
		size_t size;

		P("blocks\n");
		for (size_t index = 0; index < arena->mfmh_bump; index += size) {
			bool allocated = __mfm_block_is_allocated(arena, index);

			blk  = &arena->mfm_blocks[index];
			size = __mfm_block_size(arena, index);
			P(" %c[%p, %p) size=%zd\n", " *"[allocated],
					blk, blk + size, size * MFM_QUANTUM);
		}

		blk  = &arena->mfm_blocks[arena->mfmh_bump];
		size = MFM_BLOCKS_COUNT - arena->mfmh_bump;
		P("  [%p, %p) size=%zd (bump)\n",
				blk, blk + size, size * MFM_QUANTUM);
		P("\n");
	}
}

#ifndef MFM_TESTING

/* enumerator */

static kern_return_t
mfmi_read_zone(
	task_t                  task,
	vm_address_t            zone_address,
	memory_reader_t         reader,
	struct mfm_arena      **arena_out)
{
	reader = reader_or_in_memory_fallback(reader, task);

	return reader(task, zone_address, MFM_ARENA_SIZE, (void **)arena_out);
}

static kern_return_t
mfmi_enumerator(
	task_t                  task,
	void                   *context,
	unsigned                type_mask,
	vm_address_t            zone_address,
	memory_reader_t         reader,
	vm_range_recorder_t     recorder)
{
	struct mfm_arena *arena;
	kern_return_t kr;

	kr = mfmi_read_zone(task, zone_address, reader, &arena);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	if (type_mask & MALLOC_ADMIN_REGION_RANGE_TYPE) {
		vm_range_t range = {
			.address = zone_address,
			.size    = offsetof(struct mfm_arena, mfm_blocks),
		};

		recorder(task, context, MALLOC_ADMIN_REGION_RANGE_TYPE, &range, 1);
	}

	if (type_mask & MALLOC_PTR_REGION_RANGE_TYPE) {
		vm_range_t range = {
			.address = zone_address + offsetof(struct mfm_arena, mfm_blocks),
			.size    = (uintptr_t)arena->mfm_end - (uintptr_t)arena->mfm_blocks,
		};

		recorder(task, context, MALLOC_PTR_REGION_RANGE_TYPE, &range, 1);
	}

	if (type_mask & MALLOC_PTR_IN_USE_RANGE_TYPE) {
		const size_t array_size = 32;
		struct mfm_block *blk;
		vm_range_t array[array_size];
		uint32_t count = 0;
		size_t size;

		for (size_t index = 0; index < arena->mfmh_bump; index += size) {
			blk  = &arena->mfm_blocks[index];
			size = __mfm_block_size(arena, index);
			if (!__mfm_block_is_allocated(arena, index)) {
				continue;
			}

			if (count == array_size) {
				recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE,
						array, count);
				count = 0;
			}
			size_t blk_offset = (uintptr_t)blk - (uintptr_t)arena;
			array[count].address = zone_address + blk_offset;
			array[count].size    = size * MFM_QUANTUM;
			count++;
		}

		if (count != 0) {
			recorder(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE,
					array, count);
		}
	}

	return KERN_SUCCESS;
}


/* statistics */
static void
mfmi_statistics(struct mfm_arena *arena, malloc_statistics_t *stats)
{
	size_t hwm = arena->mfmh_bump_hwm;

	stats->blocks_in_use    = (uint32_t)arena->mfmh_alloc_count;
	stats->size_in_use      = arena->mfmh_size * MFM_QUANTUM;
	stats->size_allocated   = MFM_ARENA_SIZE;
	stats->max_size_in_use += round_page(offsetof(struct mfm_arena, mfm_bits[2 * hwm / 64]));
	stats->max_size_in_use += round_page(hwm * MFM_QUANTUM);
}

static void
mfmi_statistics_self(malloc_zone_t *zone __unused, malloc_statistics_t *stats)
{
	struct mfm_arena *arena = os_atomic_load(&mfm_arena, dependency);

	bzero(stats, sizeof(*stats));
	if (arena) {
		mfmi_statistics(arena, stats);
	}
}

static void
mfmi_statistics_task(
	task_t                  task,
	vm_address_t            zone_address,
	memory_reader_t         reader,
	malloc_statistics_t    *stats)
{
	struct mfm_arena *arena;
	kern_return_t kr;

	kr = mfmi_read_zone(task, zone_address, reader, &arena);

	bzero(stats, sizeof(*stats));
	if (kr == KERN_SUCCESS) {
		mfmi_statistics(arena, stats);
	}
}


/* logging */
static void
mfmi_log(malloc_zone_t *zone __unused, void *address __unused)
{
}

static void
mfmi_print(struct mfm_arena *arena, bool verbose, print_task_printer_t printer)
{
	print_mfm_arena(arena, verbose, printer);
}

static void
mfmi_print_self(malloc_zone_t *zone __unused, boolean_t verbose)
{
	struct mfm_arena *arena = os_atomic_load(&mfm_arena, dependency);

	if (arena) {
		print_mfm_arena(arena, verbose, malloc_report_simple);
	}
}

static void
mfmi_print_task(
	task_t                  task,
	unsigned                level,
	vm_address_t            zone_address,
	memory_reader_t         reader,
	print_task_printer_t    printer)
{
	struct mfm_arena *arena;
	kern_return_t kr;

	kr = mfmi_read_zone(task, zone_address, reader, &arena);
	if (kr == KERN_SUCCESS) {
		mfmi_print(arena, (level >= MALLOC_VERBOSE_PRINT_LEVEL), printer);
	} else {
		printer("Failed to read ProbGuard zone at %p\n", zone_address);
	}
}

/* queries */
static size_t
mfmi_good_size(malloc_zone_t *zone __unused, size_t size)
{
	if (size <= MFM_ALLOC_SIZE_MAX) {
		return roundup(size, 16);
	}
	return 0;
}

static boolean_t
mfmi_check(malloc_zone_t *zone __unused)
{
	return true;
}

/* locking */
static void
mfmi_force_lock(malloc_zone_t *zone __unused)
{
	mfm_lock();
}

static void
mfmi_force_unlock(malloc_zone_t *zone __unused)
{
	mfm_unlock();
}

static void
mfmi_reinit_lock(malloc_zone_t *zone __unused)
{
	mfm_reinit_lock();
}

static boolean_t
mfmi_locked(malloc_zone_t *zone __unused)
{
	struct mfm_arena *arena = os_atomic_load(&mfm_arena, dependency);

	if (arena && _malloc_lock_trylock(&arena->mfmh_lock)) {
		_malloc_lock_unlock(&arena->mfmh_lock);
		return true;
	}
	return false;
}

const struct malloc_introspection_t mfm_introspect = {
	/* enumerator */
	.enumerator      = mfmi_enumerator,

	/* statistics */
	.statistics      = mfmi_statistics_self,
	.task_statistics = mfmi_statistics_task,

	/* logging */
	.print           = mfmi_print_self,
	.log             = mfmi_log,
	.print_task      = mfmi_print_task,

	/* queries */
	.good_size       = mfmi_good_size,
	.check           = mfmi_check,

	/* locking */
	.force_lock      = mfmi_force_lock,
	.force_unlock    = mfmi_force_unlock,
	.zone_locked     = mfmi_locked,
	.reinit_lock     = mfmi_reinit_lock,
};

#endif /* MFM_TESTING */

#endif /* CONFIG_EARLY_MALLOC */
