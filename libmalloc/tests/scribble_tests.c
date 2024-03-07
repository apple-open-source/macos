//
//  scribble_tests.c
//  libsystem_malloc
//
//  Created by Aaron Morrison on 7/14/23.
//
#include <stdlib.h>
#include <darwintest.h>

#include <malloc_private.h>

#include "base.h"

#define SCRIBBLE_ALLOC_BYTE 0xaa

static bool memchk(void *ptr, uint8_t byte, size_t size)
{
	for (int i = 0; i < size; i++) {
		if (((uint8_t*)ptr)[i] != byte) {
			T_LOG("memchk: offset 0x%x in %p - expected 0x%02x, actual 0x%02x",
					i, ptr, byte, ((uint8_t*)ptr)[i]);
			return false;
		}
	}
	return true;
}

// note that scribble seems to break leaks for szone malloc
T_DECL(malloc_scribble_check, "check MallocScribble works",
	T_META_TAG_XZONE,
	T_META_ENVVAR("MallocScribble=1"),
	T_META_CHECK_LEAKS(false))
{
	// Ensure that TINY allocations are scribbled on allocation
	// It would be nice to check that they're set to the scrabble value (0x55)
	// on free, but we can't rely on that always working in an integration test
	void *ptr = malloc(KiB(1));
	T_EXPECT_TRUE(memchk(ptr, SCRIBBLE_ALLOC_BYTE, KiB(1)), "Scribble on malloc");
	free(ptr);

	// Realloc a TINY allocation into a SMALL allocation. The newly allocated
	// bytes should be set to the scribble value, while the old bytes should
	// stay at whatever we set them to (0x33 in this case)
	ptr = malloc(KiB(4));
	memset(ptr, 0x33, KiB(4));
	ptr = realloc(ptr, KiB(8));
	T_EXPECT_TRUE(memchk(ptr, 0x33, KiB(4)), "Memory rewritten on realloc");
	T_EXPECT_TRUE(memchk((uint8_t*)ptr + KiB(4), SCRIBBLE_ALLOC_BYTE, KiB(4)),
			"Scribble on realloc");
	ptr = realloc(ptr, KiB(2));
	T_EXPECT_TRUE(memchk(ptr, 0x33, KiB(2)), "realloc down");

	free(ptr);

	// Now test out LARGE (>32K) pointers
	ptr = malloc(KiB(64));
	T_EXPECT_TRUE(memchk(ptr, SCRIBBLE_ALLOC_BYTE, KiB(64)),
			"Scribble on LARGE allocation");
	free(ptr);

	// Realloc a LARGE allocation, which may be done in-place. The new bytes
	// should be the scribble value
	ptr = malloc(KiB(64));
	memset(ptr, 0x33, KiB(64));
	ptr = realloc(ptr, KiB(128));
	T_EXPECT_TRUE(memchk(ptr, 0x33, KiB(64)), "Memory retained on realloc");
	T_EXPECT_TRUE(memchk((uint8_t*)ptr + KiB(64), SCRIBBLE_ALLOC_BYTE, KiB(64)),
			 "Scribble on realloc");
	ptr = realloc(ptr, KiB(48));
	T_EXPECT_TRUE(memchk(ptr, 0x33, KiB(48)), "realloc down");
	free(ptr);

	// Now test out HUGE (>2M) pointers
	ptr = malloc(MiB(4)); // 4M
	T_EXPECT_TRUE(memchk(ptr, SCRIBBLE_ALLOC_BYTE, MiB(4)),
			"Scribble on malloc HUGE");
	free(ptr);

	// Realloc HUGE allocation, same as above
	ptr = malloc(MiB(4));
	memset(ptr, 0x33, MiB(4));
	ptr = realloc(ptr, MiB(8));
	T_EXPECT_TRUE(memchk(ptr, 0x33, MiB(4)), "Memory retained on realloc");
	T_EXPECT_TRUE(memchk((uint8_t*)ptr + MiB(4), SCRIBBLE_ALLOC_BYTE, MiB(4)),
			"Scribble on realloc");
	ptr = realloc(ptr, MiB(3));
	T_EXPECT_TRUE(memchk(ptr, 0x33, MiB(3)), "realloc down");
	free(ptr);

	// Exhaust the early allocator and make sure that new allocations are still
	// scribbled
	for (int i = 0; i < 64; i++) {
		ptr = malloc(64);
		free(ptr);
	}
	ptr = malloc(64);
	T_EXPECT_TRUE(memchk(ptr, SCRIBBLE_ALLOC_BYTE, 64), "Scribble on malloc");
	free(ptr);

	// Make sure that memory returned by calloc is zeroed, for all size classes
	for (uint64_t s = 1; s < 0x4000000; s <<= 1) {
		ptr = calloc(s, 1);
		T_EXPECT_TRUE(memchk(ptr, 0, s), "Calloc should return zeroed memory");
		free(ptr);
	}

	// Make sure memory returned by malloc_zone_malloc_with_options_np() is
	// correct
	ptr = malloc_zone_malloc_with_options_np(NULL, sizeof(void *), KiB(1), 0);
	T_EXPECT_TRUE(memchk(ptr, SCRIBBLE_ALLOC_BYTE, KiB(1)),
			"malloc_zone_malloc_with_options_np()");
	free(ptr);

	ptr = malloc_zone_malloc_with_options_np(NULL, sizeof(void *), KiB(1),
			MALLOC_NP_OPTION_CLEAR);
	T_EXPECT_TRUE(memchk(ptr, 0, KiB(1)),
			"malloc_zone_malloc_with_options_np(MALLOC_NP_OPTION_CLEAR)");
	free(ptr);

	// Allocate and free many allocations smaller than the zero on free
	// threshold (1024), in order to push several pages into the isolation zone.
	// Then calloc those same allocations, to make sure that the memory is zeroed
	const size_t num_ptrs = 256;
	void **ptr_array = calloc(sizeof(void*), num_ptrs);
	for (int i = 0; i < num_ptrs; i++) {
		ptr_array[i] = malloc(512);
		T_QUIET;
		T_EXPECT_TRUE(memchk(ptr_array[i], SCRIBBLE_ALLOC_BYTE, 512),
				"Scribble on malloc");
	}
	for (int i = 0; i < num_ptrs; i++) {
		free(ptr_array[i]);
		ptr_array[i] = NULL;
	}
	for (int i = 0; i < num_ptrs; i++) {
		ptr_array[i] = calloc(512, 1);
		T_QUIET;
		T_EXPECT_TRUE(memchk(ptr_array[i], 0x00, 512),
				"Calloc should return zeroed memory");
	}
	for (int i = 0; i < num_ptrs; i++) {
		free(ptr_array[i]);
		ptr_array[i] = NULL;
	}
}
