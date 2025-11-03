//
//  magazine_tiny_test.c
//  libmalloc
//
//  Created by Matt Wright on 8/22/16.
//
//

#include <darwintest.h>

#include "../src/magazine_tiny.c"
#include "magazine_testing.h"

// Stubs
bool aggressive_madvise_enabled = DEFAULT_AGGRESSIVE_MADVISE_ENABLED;

unsigned malloc_zero_on_free_sample_period = 0;
malloc_zero_policy_t malloc_zero_policy = MALLOC_ZERO_POLICY_DEFAULT;

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_VM_PREFERRED,
		T_META_TAG_NO_ALLOCATOR_OVERRIDE);

static inline void
tiny_test_rack_setup(rack_t *rack)
{
	test_rack_setup(rack, RACK_TYPE_TINY);
}

T_DECL(basic_tiny_alloc, "tiny rack init and alloc")
{
	struct rack_s rack;
	tiny_test_rack_setup(&rack);

	void *ptr = tiny_malloc_should_clear(&rack, TINY_MSIZE_FOR_BYTES(32), false);
	T_ASSERT_NOTNULL(ptr, "allocation");

	region_t *rgn = tiny_region_for_ptr_no_lock(&rack, ptr);
	T_ASSERT_NOTNULL(rgn, "allocation region found in rack");

	size_t sz = tiny_size(&rack, ptr);
	T_ASSERT_EQ((int)sz, 32, "size == 32");
}

T_DECL(basic_tiny_teardown, "tiny rack init, alloc, teardown")
{
	struct rack_s rack;
	tiny_test_rack_setup(&rack);

	void *ptr = tiny_malloc_should_clear(&rack, TINY_MSIZE_FOR_BYTES(32), false);
	T_ASSERT_NOTNULL(ptr, "allocation");

	rack_destroy_regions(&rack, TINY_REGION_SIZE);
	for (int i=0; i < rack.region_generation->num_regions_allocated; i++) {
		T_QUIET;
		T_ASSERT_TRUE(rack.region_generation->hashed_regions[i] == HASHRING_OPEN_ENTRY ||
					  rack.region_generation->hashed_regions[i] == HASHRING_REGION_DEALLOCATED,
					  "all regions destroyed");
	}

	rack_destroy(&rack);
	T_ASSERT_NULL(rack.magazines, "magazines destroyed");
}

T_DECL(basic_tiny_free, "tiny free")
{
	struct rack_s rack;
	tiny_test_rack_setup(&rack);

	void *ptr = tiny_malloc_should_clear(&rack, TINY_MSIZE_FOR_BYTES(32), false);
	T_ASSERT_NOTNULL(ptr, "allocation");

	// free doesn't return an error (unless we assert here)
	free_tiny(&rack, ptr, TINY_REGION_FOR_PTR(ptr), 0, false);

	size_t sz = tiny_size(&rack, ptr);
	T_ASSERT_EQ((int)sz, 0, "allocation freed (sz == 0)");
}

T_DECL(basic_tiny_shrink, "tiny rack shrink")
{
	struct rack_s rack;
	tiny_test_rack_setup(&rack);

	void *ptr = tiny_malloc_should_clear(&rack, TINY_MSIZE_FOR_BYTES(64), false);
	T_ASSERT_NOTNULL(ptr, "allocation");

	size_t sz = tiny_size(&rack, ptr);
	T_ASSERT_EQ((int)sz, 64, "size == 64");

	void *nptr = tiny_try_shrink_in_place(&rack, ptr, sz, 16);
	size_t nsz = tiny_size(&rack, nptr);
	T_ASSERT_EQ_PTR(ptr, nptr, "ptr == nptr");
	T_ASSERT_EQ((int)nsz, 16, "nsz == 16");
}

T_DECL(basic_tiny_realloc_in_place, "tiny rack realloc in place")
{
	struct rack_s rack;
	tiny_test_rack_setup(&rack);

	// Allocate two blocks and free the second, then try to realloc() the first.
	// This should extend in-place using the one-level death row cache that's
	// occupied by the second block.
	void *ptr = tiny_malloc_should_clear(&rack, TINY_MSIZE_FOR_BYTES(16), false);
	T_ASSERT_NOTNULL(ptr, "allocation");

	size_t sz = tiny_size(&rack, ptr);
	T_ASSERT_EQ((int)sz, 16, "size == 16");

	void *ptr2 = tiny_malloc_should_clear(&rack, TINY_MSIZE_FOR_BYTES(16), false);
	T_ASSERT_NOTNULL(ptr2, "allocation 2");
	T_ASSERT_EQ_PTR(ptr2, (void *)((uintptr_t)ptr + 16), "sequential allocations");
	free_tiny(&rack, ptr2, TINY_REGION_FOR_PTR(ptr2), 0, false);

	// Attempt to realloc up to 32 bytes, this should happen in place
	// because of the death-row cache.
	boolean_t reallocd = tiny_try_realloc_in_place(&rack, ptr, sz, 32);
	T_ASSERT_TRUE(reallocd, "realloced #1");

	size_t nsz = tiny_size(&rack, ptr);
	T_ASSERT_EQ((int)nsz, 32, "realloc size == 32");

	// Try another realloc(). This should extend in place because the rest of
	// the rack is empty.
	reallocd = tiny_try_realloc_in_place(&rack, ptr, nsz, 64);
	T_ASSERT_TRUE(reallocd, "realloced #2");
	nsz = tiny_size(&rack, ptr);
	T_ASSERT_EQ((int)nsz, 64, "realloc size == 64");

	free_tiny(&rack, ptr, TINY_REGION_FOR_PTR(ptr), 0, false);
}

T_DECL(tiny_free_deallocate, "check tiny regions deallocate when empty")
{
	struct rack_s rack;
	memset(&rack, 'a', sizeof(rack));
	rack_init(&rack, RACK_TYPE_TINY, 1, 0);
	// force recirc to be released when empty
	recirc_retained_regions = 0;

	magazine_t *mag = &rack.magazines[0];
	T_ASSERT_EQ(mag->mag_last_region, NULL, "no regions before allocation");

	void *ptr = tiny_malloc_should_clear(&rack, 1, false);
	T_ASSERT_NE(ptr, NULL, "allocate");

	region_t region = TINY_REGION_FOR_PTR(ptr);
	mag_index_t mag_index = MAGAZINE_INDEX_FOR_TINY_REGION(region);
	magazine_t *dest_mag = &(rack.magazines[mag_index]);
	T_ASSERT_GE(mag_index, -1, "assert ptr not in recirc");

	// Is to pass the recirc discriminant in tiny_free_try_recirc_to_depot
	// which normally prevents recirc unless 1.5x of a region has been
	// allocated in the magazine as a whole.
	dest_mag->num_bytes_in_magazine = (5 * TINY_REGION_SIZE);

	SZONE_MAGAZINE_PTR_LOCK(dest_mag);
	tiny_free_no_lock(&rack, dest_mag, mag_index, region, ptr, 1, false);

	T_ASSERT_EQ(mag->mag_last_region, NULL, "no regions after last free");

	magazine_t *depot = &rack.magazines[DEPOT_MAGAZINE_INDEX];
	T_ASSERT_EQ(depot->mag_num_objects, 0, "no objects in depot after last free");
	T_ASSERT_EQ(depot->num_bytes_in_magazine, 0ul, "no region in depot after last free");
}
