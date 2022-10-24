//
//  magazine_malloc.c
//  libmalloc
//
//  Created by Kim Topley on 11/8/17.
//
#include <darwintest.h>
#include <stdlib.h>
#include <malloc/malloc.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

T_DECL(malloc_zone_batch, "malloc_zone_batch_malloc and malloc_zone_batch_free")
{
	const unsigned count = 10;
	void **results;
	unsigned number;

	// Use malloc_zone_batch_malloc() with a size that maps to the tiny
	// allocator. This should succeed.
	results = calloc(count, sizeof(void *));
	number = malloc_zone_batch_malloc(malloc_default_zone(), 32, results, count);
	T_ASSERT_EQ(number, count, "allocated from tiny zone");
	for (int i = 0; i < count; i++) {
		T_QUIET; T_ASSERT_NOTNULL(results[i], "pointer %d is NULL", i);
	}
	malloc_zone_batch_free(malloc_default_zone(), results, count);
	free(results);

	// Use malloc_zone_batch_malloc() with a size that maps to the small
	// allocator. This should fail.
	results = calloc(count, sizeof(void *));
	number = malloc_zone_batch_malloc(malloc_default_zone(), 2048, results, count);
	T_ASSERT_EQ(0, number, "could not allocat from small zone");
	for (int i = 0; i < count; i++) {
		T_QUIET; T_ASSERT_NULL(results[i], "pointer %d is not NULL", i);
	}
	malloc_zone_batch_free(malloc_default_zone(), results, count);
	free(results);
}

T_DECL(malloc_zone_batch_zero_on_free, "malloc_zone_batch_free must zero-on-free",
		T_META_ENVVAR("MallocZeroOnFree=1"),
		T_META_ENVVAR("MallocCheckZeroOnFreeCorruption=1"),
		T_META_ENVVAR("MallocNanoZone=0"))
{
	const int n = 3;
	const size_t size = 272;
	void *allocations[n];
	for (int i = 0; i < n; i++) {
		allocations[i] = malloc(size);
		T_QUIET; T_ASSERT_NOTNULL(allocations[i], "malloc()");
		memset(allocations[i], 'a', size);
	}

	malloc_zone_batch_free(malloc_default_zone(), allocations, n);

	char *allocation = calloc(1, size);
	T_QUIET; T_ASSERT_NOTNULL(allocation, "calloc()");

	for (size_t i = 0; i < size; i++) {
		T_QUIET; T_ASSERT_EQ(allocation[i], '\0', "byte %zu should be 0", i);
	}

	free(allocation);

	T_PASS("Successful calloc after batch free");
}
