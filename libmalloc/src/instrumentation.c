// We need to include "internal.h" (which includes platform.h)
// before we can use CONFIG_MTE.
#include "internal.h"

#if CONFIG_MTE

MALLOC_NOEXPORT
uint8_t *
memtag_assign_tag(uint8_t *address, size_t size)
{
	// Exclude the canonical tag by default.
	uint64_t mask = 0x0001;

	// Exclude the tag currently associated with the given block.
	mask = _memtag_exclude_tag(address, mask);

	// Exclude the tag associated with the previous block.
	// Keep the check within the page boundary, to avoid hitting potentially
	// unmapped memory.
	if (memtag_p2align(address, PAGE_SIZE) == memtag_p2align(address - 16, PAGE_SIZE)) {
		mask = _memtag_exclude_tag(address - 16, mask);
	}

	// Exclude the tag associated with the next block.
	if (memtag_p2align(address + size - 1, PAGE_SIZE) == memtag_p2align(address + size, PAGE_SIZE)) {
		mask = _memtag_exclude_tag(address + size, mask);
	}

	return _memtag_create_random_tag(address, mask);
}

MALLOC_NOEXPORT
uint8_t *
memtag_init_chunk(uint8_t *chunk_start, size_t chunk_size, uint64_t block_size)
{
	size_t num_blocks = chunk_size / block_size;
	uint8_t *tagged_addr = NULL;
	uint8_t *first_block = NULL;
	for (size_t idx = 0; idx < num_blocks; idx++) {
		uint8_t *block_addr = &chunk_start[idx * block_size];
		// Exclude the canonical tag by default.
		uint64_t exclude_mask = 0x0001;
		// Exclude the tag of the previously tagged block
		if (tagged_addr != NULL) {
			exclude_mask = _memtag_update_mask(tagged_addr, exclude_mask);
		}
		tagged_addr = _memtag_create_random_tag(block_addr, exclude_mask);
		if (idx == 0) {
			first_block = tagged_addr;
		}
		memtag_set_tag(tagged_addr, block_size);
	}
	return first_block;
}

#ifndef DARWINTEST

bool
memtag_handle_mismatch(void *ptr)
{
#if !MALLOC_TARGET_EXCLAVES
	// Speculatively set the crash log message: this is required to inform the
	// client in case we crash while trying to load the tag (ldg) for the
	// pointer we were given, which might be a totally invalid value.
	// Note that the exception generated in this case is not an MTE violation.
	_os_set_crash_log_cause_and_message((uintptr_t)ptr,
			"BUG IN CLIENT OF LIBMALLOC: pointer being freed was not valid");
#endif

	// Load the physical tag for the pointer.
	const uint8_t *__unsafe_indexable ldg = memtag_fixup_ptr(ptr);
	if (!memtag_tags_match(ptr, ldg)) {
		// Extract the 4-bit tags, both for the logical and the physical tag.
		// Then, compute an 8-bit value encoding the expected logical tag in the
		// higher bits, and the physical tag in the lower bits.
		// This value is then used as the crash report reason.
		const uintptr_t ltag = ((uintptr_t)ptr) >> 56 & 0xf;
		const uintptr_t ptag = ((uintptr_t)ldg) >> 56 & 0xf;
		const uint8_t encoded = (ltag << 4) | ptag;

#if !MALLOC_TARGET_EXCLAVES
		_os_set_crash_log_cause_and_message(encoded,
			 "BUG IN CLIENT OF LIBMALLOC: MTE tag mismatch"
			 " (probable double-free)");
		// Dereference the pointer carrying the invalid tag.
		*(volatile char *)ptr;

		// If we survived that, we must be in soft mode.

		_os_set_crash_log_cause_and_message(encoded,
			 "BUG IN CLIENT OF LIBMALLOC: ignored previous invalid free"
			 " due to MTE tag mismatch in soft mode (probable double-free)");

		// We'll now retry the free() with the correct tag: if there's anything
		// else wrong other than the tag that will result in a normal abort, and
		// if there isn't then the block will have been free()d and we'll signal
		// to the caller to continue.
		find_zone_and_free((void *)ldg, false);
		return true;
#else // !MALLOC_TARGET_EXCLAVES
		// For Exclaves, we currently do not have an equivalent way of setting
		// a crash log message as _os_set_crash_log_cause_and_message. Since
		// the different context is less susceptible to inadvertently turning
		// fatal exceptions into catchable ones, for the moment we simply abort.
		__liblibc_fatal_error(
			"BUG IN CLIENT OF LIBMALLOC (%llu): MTE tag mismatch",
			(uint64_t)encoded);
#endif // !MALLOC_TARGET_EXCLAVES
	}

	return false;
}

#endif // DARWINTEST

#endif // CONFIG_MTE
