#if CONFIG_MTE

#ifndef __MEMTAG_H__
#define __MEMTAG_H__

// Make sure we have +memtag when including this file.
#if !defined(__ARM_FEATURE_MEMORY_TAGGING)
#error "This file requires __ARM_FEATURE_MEMORY_TAGGING"
#endif

#include <arm_acle.h>

__ptrcheck_abi_assume_unsafe_indexable();

#define MEMTAG_TAG_MAX 0xf

#define memtag_p2roundup(x, align) (-(-(x) & -(align)))
#define memtag_p2align(x, align) ((uintptr_t)(x) & -(align))

union memtag_ptr {
	uint64_t value;

	struct {
		uint64_t ptr_bits : 56; // address part
		uint64_t ptr_tag : 4;   // logical tag
		uint64_t ptr_upper : 4;
	};
};

// Add the logical tag contained in `tagged_addr` to the mask
// specified by `excluded`, and return the updated mask.
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint64_t
_memtag_update_mask(uint8_t *tagged_addr, uint64_t excluded)
{
	return __arm_mte_exclude_tag(tagged_addr, excluded);
}

// Return a pointer whose address part comes from `addr`, and whose logical tag
// is the allocation tag loaded from the memory pointed to by `addr`.
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_memtag_load_tag(uint8_t *addr)
{
	return __arm_mte_get_tag(addr);
}

// Load the allocation tag associated to the memory pointed to
// by `addr`, add it to the mask specified by `excluded`, and
// return the updated mask.
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint64_t
_memtag_exclude_tag(uint8_t *addr, uint64_t excluded)
{
	return _memtag_update_mask(_memtag_load_tag(addr), excluded);
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
_memtag_create_random_tag(uint8_t *addr, uint64_t mask)
{
	return __arm_mte_create_random_tag(addr, mask);
}

#pragma mark Exposed interfaces

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
memtag_strip_address(uint8_t *tagged_addr)
{
	union memtag_ptr p = {
		.value = (uint64_t)tagged_addr,
	};
	p.ptr_tag = 0;
	return (uint8_t *)p.value;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t
memtag_extract_tag(uint8_t *tagged_addr)
{
	union memtag_ptr p = {
		.value = (uint64_t)tagged_addr,
	};
	return (uint8_t)p.ptr_tag;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
memtag_mix_tag(uint8_t *canonical_addr, uint8_t tag)
{
	MALLOC_DEBUG_ASSERT(tag <= MEMTAG_TAG_MAX);
	union memtag_ptr p = {
		.value = (uintptr_t)canonical_addr,
	};
	p.ptr_tag = tag;
	return (uint8_t *)p.value;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static bool
memtag_tags_match(const void *p,
	const void *q)
{
	union memtag_ptr pmtp = { .value = (uintptr_t)p };
	union memtag_ptr qmtp = { .value = (uintptr_t)q };
	return pmtp.ptr_tag == qmtp.ptr_tag;
}

MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
memtag_fixup_ptr(uint8_t *ptr)
{
	return _memtag_load_tag(ptr);
}

// Return a pointer whose address part comes from `addr`, and whose logical tag
// is the canonical one (0).
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
memtag_assign_canonical_tag(uint8_t *address)
{
	// Assigning a canonical tag equates to stripping the logical tag included
	// in the address, if any is present.
	return memtag_strip_address(address);
}

// Disable tag checking (i.e. enable Tag Checking Override).
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
memtag_disable_checking(void)
{
	__asm__ __volatile__ ("msr TCO, #1");
}

// Enable tag checking (i.e. disable Tag Checking Override).
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
memtag_enable_checking(void)
{
	__asm__ __volatile__ ("msr TCO, #0");
}

// Set tags for a block with arbitrary 16B alignment
MALLOC_INLINE MALLOC_ALWAYS_INLINE
static void
memtag_set_tag_unaligned(uint8_t *start, size_t size)
{
	MALLOC_DEBUG_ASSERT(!(size % 16));
	MALLOC_DEBUG_ASSERT(!((uintptr_t)start % 16));
	uint8_t *addr = start;
	uint8_t *end  = start + size;

	// NB: For xzone-malloc, the stg and st2g operations can be further
	// conditioned on block sizes that can possibly be 64B-misaligned. This
	// optimization is measurably faster (on the order of 5 cycles per block),
	// but does not work for the early-allocator, for which all block sizes
	// may have arbitrary 16B-alignment.

	// Unconditionally tag the first and last 16B granule
	__asm__ __volatile__ ("stg %0, [%0]" : : "r"(addr) : "memory");
	__asm__ __volatile__ ("stg %0, [%0, #-16]" : : "r"(end) : "memory");

	if (os_likely(size > 32)) {
		// Round up/down to the nearest 32B boundary
		addr = (uint8_t *)((uintptr_t)(start + 31) & -32);
		end  = (uint8_t *)((uintptr_t)(start + size) & -32);

		// Tag the first and last 32B granule.
		__asm__ __volatile__ ("st2g %0, [%0]" : : "r"(addr) : "memory");
		__asm__ __volatile__ ("st2g %0, [%0, #-32]" : : "r"(end) : "memory");
	}

	// Round up/down to the nearest 64B boundary
	addr = (uint8_t *)((uintptr_t)(start + 63) & -64);
	end  = (uint8_t *)((uintptr_t)(start + size) & -64);

	// At this point, any non-64B-aligned portion of the block has been tagged.
	// Tag the remaining 64B granules
	while (addr < end) {
		__asm__ __volatile__ ("dc gva, %0" : : "r"(addr) : "memory");
		addr += 64;
	}
}

// Set tags for blocks with at least 512B alignment
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
memtag_set_tag_aligned_512(uint8_t *addr, vm_size_t size)
{
	MALLOC_DEBUG_ASSERT(!((uintptr_t)addr % 512));
	MALLOC_DEBUG_ASSERT(!(size % 512));
	uint8_t *end = addr + size;
	while (addr < end) {
		__asm__ __volatile__ ("dc gva, %0" : : "r"(addr) : "memory");
		__asm__ __volatile__ ("dc gva, %0" : : "r"(addr + 64) : "memory");
		__asm__ __volatile__ ("dc gva, %0" : : "r"(addr + 128) : "memory");
		__asm__ __volatile__ ("dc gva, %0" : : "r"(addr + 192) : "memory");
		__asm__ __volatile__ ("dc gva, %0" : : "r"(addr + 256) : "memory");
		__asm__ __volatile__ ("dc gva, %0" : : "r"(addr + 320) : "memory");
		__asm__ __volatile__ ("dc gva, %0" : : "r"(addr + 384) : "memory");
		__asm__ __volatile__ ("dc gva, %0" : : "r"(addr + 448) : "memory");
		addr += 512;
	}
}

// Set tags for blocks served from a fixed block-size slab.
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static void
memtag_set_tag(uint8_t *addr, size_t size)
{
	if (!!(size & 511)) {
		memtag_set_tag_unaligned(addr, size);
	} else {
		memtag_set_tag_aligned_512(addr, size);
	}
}

// Return a pointer whose address part comes from `address`, adding a
// random logical tag which is chosen based on the following criteria:
// - It should not be the canonical tag (0)
// - It should not match the allocation tag associated with the granule
//   pointed to by `address`
// - It should not match the allocation tag associated with the granule
//   that that precedes `address`
// - It should not match the allocation tag associated with the granule
//   that that follows the block pointed to by `address`
//   (i.e. the one associated with `address + size`)
MALLOC_NOEXPORT
uint8_t *
memtag_assign_tag(uint8_t *address, size_t size);

// Initialize the allocation tags for the chunk of memory pointed to
// by `chunk_start`, by setting a different tags for all the blocks
// of size `block_size` up to `chunk_size`.
MALLOC_NOEXPORT
uint8_t *
memtag_init_chunk(uint8_t *chunk_start, size_t chunk_size, uint64_t block_size);

// Given a pointer, try to load the tag associated to the physical memory it
// points to, and compare it against its logical tag. If the tags do not match,
// dereference the pointer (carrying the invalid logical tag) to raise a fatal
// exception that cannot be caught by the process.
//
// A result of true indicates that there _was_ a tag mismatch, which was
// handled.  A caller that observes this result should treat it as an indication
// that we're in soft mode and allow control to return to the client.
//
// A result of false indicates that the problem was not just a tag mismatch, so
// we should abort() in the traditional manner for invalid pointers.
MALLOC_NOEXPORT
bool
memtag_handle_mismatch(void *ptr);

// Retag a block.
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
memtag_retag(uint8_t *address, size_t size)
{
	uint8_t *tagged_addr = memtag_assign_tag(address, size);
	memtag_set_tag(tagged_addr, size);
	return tagged_addr;
}

// Canonically tag a block.
MALLOC_ALWAYS_INLINE MALLOC_INLINE
static uint8_t *
memtag_tag_canonical(uint8_t *address, size_t size)
{
	uint8_t *canonical_addr = memtag_assign_canonical_tag(address);
	memtag_set_tag_unaligned(canonical_addr, size);
	return canonical_addr;
}

#endif // __MEMTAG_H__

#endif // CONFIG_MTE
