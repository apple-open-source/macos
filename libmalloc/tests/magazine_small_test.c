//
//  magazine_tiny_test.c
//  libmalloc
//
//  Created by Matt Wright on 8/22/16.
//
//

#include <darwintest.h>

#include "../src/magazine_small.c"
#include "magazine_testing.h"

static inline void
test_rack_setup(rack_t *rack)
{
	memset(rack, 'a', sizeof(rack));
	rack_init(rack, RACK_TYPE_SMALL, 1, 0);
	T_QUIET; T_ASSERT_NOTNULL(rack->magazines, "magazine initialisation");
}

T_DECL(basic_small_alloc, "small rack init and alloc")
{
	struct rack_s rack;
	test_rack_setup(&rack);

	void *ptr = small_malloc_should_clear(&rack, SMALL_MSIZE_FOR_BYTES(512), false);
	T_ASSERT_NOTNULL(ptr, "allocation");

	region_t *rgn = small_region_for_ptr_no_lock(&rack, ptr);
	T_ASSERT_NOTNULL(rgn, "allocation region found in rack");

	size_t sz = small_size(&rack, ptr);
	T_ASSERT_EQ((int)sz, 512, "size == 32");
}

T_DECL(basic_tiny_teardown, "small rack init, alloc, teardown")
{
	struct rack_s rack;
	test_rack_setup(&rack);

	void *ptr = small_malloc_should_clear(&rack, TINY_MSIZE_FOR_BYTES(512), false);
	T_ASSERT_NOTNULL(ptr, "allocation");

	rack_destroy_regions(&rack, SMALL_REGION_SIZE);
	for (int i=0; i < rack.region_generation->num_regions_allocated; i++) {
		T_QUIET;
		T_ASSERT_TRUE(rack.region_generation->hashed_regions[i] == HASHRING_OPEN_ENTRY ||
					  rack.region_generation->hashed_regions[i] == HASHRING_REGION_DEALLOCATED,
					  "all regions destroyed");
	}

	rack_destroy(&rack);
	T_ASSERT_NULL(rack.magazines, "magazines destroyed");
}

T_DECL(basic_small_free, "small free")
{
	struct rack_s rack;
	test_rack_setup(&rack);

	void *ptr = small_malloc_should_clear(&rack, SMALL_MSIZE_FOR_BYTES(512), false);
	T_ASSERT_NOTNULL(ptr, "allocation");

	// free doesn't return an error (unless we assert here)
	free_small(&rack, ptr, SMALL_REGION_FOR_PTR(ptr), 0);

	size_t sz = small_size(&rack, ptr);
	T_ASSERT_EQ((int)sz, 0, "allocation freed (sz == 0)");
}

T_DECL(basic_small_shrink, "small rack shrink")
{
	struct rack_s rack;
	test_rack_setup(&rack);

	void *ptr = small_malloc_should_clear(&rack, SMALL_MSIZE_FOR_BYTES(1024), false);
	T_ASSERT_NOTNULL(ptr, "allocation");

	size_t sz = small_size(&rack, ptr);
	T_ASSERT_EQ((int)sz, 1024, "size == 1024");

	void *nptr = small_try_shrink_in_place(&rack, ptr, sz, 512);
	size_t nsz = small_size(&rack, nptr);
	T_ASSERT_EQ_PTR(ptr, nptr, "ptr == nptr");
	T_ASSERT_EQ((int)nsz, 512, "nsz == 512");
}

T_DECL(basic_small_realloc_in_place, "small rack realloc in place")
{
	struct rack_s rack;
	test_rack_setup(&rack);

	void *ptr = small_malloc_should_clear(&rack, SMALL_MSIZE_FOR_BYTES(512), false);
	T_ASSERT_NOTNULL(ptr, "allocation");

	size_t sz = small_size(&rack, ptr);
	T_ASSERT_EQ((int)sz, 512, "size == 512");

	// Due to <rdar://problem/28032870>, we can't just do alloc+realloc here because
	// the allocator won't see the remainder of the region as free (the
	// mag_bytes_free_at_end section is marked as busy and realloc doesn't check
	// for it).

	// On top of that, <rdar://problem/28033016> means we can't do
	// alloc+alloc+free+realloc because the free'd block gets put in the small cache
	// and the realloc path *also* doesn't check that. That means to actually do a
	// fast-path realloc you need to:

	//   1. alloc three times (to get the blocks out of the free_at_end region)
	//   2. free the logically consecutive block
	//   3. free any other block, to get the consecutive block out of the small cache

	void *ptr2 = small_malloc_should_clear(&rack, SMALL_MSIZE_FOR_BYTES(512), false);
	T_ASSERT_NOTNULL(ptr2, "allocation 2");
	T_ASSERT_EQ_PTR(ptr2, (void *)((uintptr_t)ptr + 512), "sequential allocations");

	void *ptr3 = small_malloc_should_clear(&rack, SMALL_MSIZE_FOR_BYTES(512), false);
	T_ASSERT_NOTNULL(ptr3, "allocation 3");

	free_small(&rack, ptr2, SMALL_REGION_FOR_PTR(ptr2), 0);
	free_small(&rack, ptr3, SMALL_REGION_FOR_PTR(ptr3), 0);

	// attempt to realloc up to 1024 bytes, this should happen in place
	// as the rack is otherwise completely empty
	boolean_t reallocd = small_try_realloc_in_place(&rack, ptr, sz, 1024);
	T_ASSERT_TRUE(reallocd, "realloced");

	size_t nsz = small_size(&rack, ptr);
	T_ASSERT_EQ((int)nsz, 1024, "realloc size == 1024");
}
