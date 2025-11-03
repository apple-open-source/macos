//
//  malloc_with_options_test.c
//  libsystem_malloc
//
//  Created by Aaron Morrison on 7/13/23.
//
#include <stdlib.h>
#include <darwintest.h>
#include <malloc_private.h>
#include <time.h>

#include "../src/platform.h"

#if !MALLOC_TARGET_EXCLAVES
#include <dispatch/dispatch.h>
#include "trace.h"
#include <ktrace.h>
#endif // !MALLOC_TARGET_EXCLAVES

// Returns true if memory is canonically tagged
// equivalent to "are bits 59:56 cleared"
static bool
check_canonical_tag(void *ptr)
{
	return !((uintptr_t)ptr & 0x0f00000000000000);
}

static bool
check_zeroed_memory(void *ptr, size_t size)
{
	uint8_t *ptr_byte = (uint8_t*)ptr;
	for (size_t i = 0; i < size; i++) {
		if (ptr_byte[i] != 0) {
			return false;
		}
	}
	return true;
}

static bool
check_ptr_is_aligned(void *ptr, size_t align)
{
	return ((uintptr_t)ptr % align) == 0;
}

static void
scribble_memory(void *ptr, size_t size)
{
	memset(ptr, 0xbe, size);
}

static void
run_options_test(int iterations)
{
	// Worst case memory consumption of test is 8M * MAX_POINTERS
	const int MAX_POINTERS = 64;
	void *pointers[MAX_POINTERS] = { NULL };
	for (int iteration = 0; iteration < iterations; iteration++) {
		int index = rand() % MAX_POINTERS;
		int opt_rand = rand();

		bool aligned = opt_rand & 0x1;

		bool zeroed = opt_rand & 0x2;
		malloc_zone_malloc_options_t options = MALLOC_ZONE_MALLOC_OPTION_NONE;
		if (zeroed) {
			options |= MALLOC_ZONE_MALLOC_OPTION_CLEAR;
		}

		bool canonical = opt_rand & 0x4;
		if (canonical) {
			options |= MALLOC_NP_OPTION_CANONICAL_TAG;
		}

		opt_rand = rand();
		size_t align = 0;
		size_t size = 0;
		if (aligned) {
			// align must be a power of 2, to use values from 32 to 1MM
			// 4 bits of shift
			align = 8 << (opt_rand & 0xf);

			// We require that size be a multiple of alignment
			// For maximum size = 8M, make size up to align*8
			size = align * (((opt_rand >> 4) & 0x7) + 1);
		} else {
			// size anywhere from 0 to 8M
			size = (opt_rand & 0x7fffff) + 1;
		}


		free(pointers[index]);
		if (opt_rand % 2) {
			pointers[index] = malloc_zone_malloc_with_options(NULL, align, size,
				options);
		} else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
			pointers[index] = malloc_zone_malloc_with_options_np(NULL, align,
				size, options);
#pragma GCC diagnostic pop
		}
		T_QUIET; T_ASSERT_NOTNULL(pointers[index], "Allocation failed\n");

		if (zeroed) {
			T_QUIET; T_ASSERT_TRUE(check_zeroed_memory(pointers[index], size),
					"Memory wasn't cleared");
		}
		if (canonical) {
			T_QUIET; T_ASSERT_TRUE(check_canonical_tag(pointers[index]),
					"Tag isn't canonical");
		}
		if (align) {
			T_QUIET; T_ASSERT_TRUE(check_ptr_is_aligned(pointers[index], align),
				"Pointer isn't aligned");
		}

		// Scribble the memory to make sure that malloc is properly clearing
		// when MALLOC_ZONE_MALLOC_OPTION_CLEAR is set
		scribble_memory(pointers[index], size);
	}

	for (int i = 0; i < MAX_POINTERS; i++) {
		free(pointers[i]);
	}
}

T_DECL(malloc_options, "malloc with options",
	T_META_TAG_ALL_ALLOCATORS, T_META_TAG_VM_NOT_PREFERRED)
{
	unsigned seed = time(NULL);
	T_LOG("seed value = %u", seed);
	srand(seed);

	run_options_test(10000);
}

T_DECL(malloc_pgm_options, "malloc with options, but PGM is enabled",
	T_META_ENVVAR("ProbGuardMalloc=1"),
	T_META_TAG_ALL_ALLOCATORS, T_META_TAG_VM_NOT_PREFERRED)
{
	unsigned seed = time(NULL);
	T_LOG("seed value = %u", seed);
	srand(seed);

	run_options_test(10000);
}

T_DECL(malloc_msl_lite_options, "malloc with options, but MSL Lite is enabled",
	T_META_ENVVAR("MallocStackLogging=lite"),
	T_META_TAG_ALL_ALLOCATORS, T_META_TAG_VM_NOT_PREFERRED)
{
	unsigned seed = time(NULL);
	T_LOG("seed value = %u", seed);
	srand(seed);

	run_options_test(10000);
}

T_DECL(malloc_data_only_options, "Malloc with options, all xzones pure data",
		T_META_ENVVAR("MallocXzoneDataOnly=1"),
		T_META_ENVVAR("MallocXzoneGuarded=1"),
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_NOT_PREFERRED)
{
	unsigned seed = time(NULL);
	T_LOG("seed value = %u", seed);
	srand(seed);

	run_options_test(10000);
}

#if !MALLOC_TARGET_EXCLAVES
T_DECL(malloc_options_traced, "malloc with options, but tracing is enabled",
		T_META_ENVVAR("MallocTracing=1"),
		T_META_ASROOT(true),
		T_META_TAG_XZONE_ONLY,
		T_META_TAG_VM_NOT_ELIGIBLE)
{
	/* The runtime calls malloc/calloc repeatedly, but it doesn't seem
	 * to call memalign. Therefore, we can check that the malloc_with_options
	 * SPI calls through the public symbols by counting the memalign tracepoints,
	 * and checking that at least one is generated when tracing is enabled and
	 * an aligned allocation is requested
	 */
	ktrace_session_t s = ktrace_session_create();

	__block int malloc_options_events = 0;
	const size_t expected_alignment = 4096;
	const size_t expected_size = 8192;
	__block size_t actual_alignment = 0;
	__block size_t actual_size = 0;

	T_ASSERT_POSIX_ZERO(ktrace_filter_pid(s, getpid()), NULL);

	ktrace_events_subclass(s, DBG_UMALLOC, DBG_UMALLOC_EXTERNAL,
			^(ktrace_event_t event) {
				if (event->debugid == (TRACE_malloc_options | DBG_FUNC_END)) {
					malloc_options_events++;
					actual_alignment = event->arg2;
					actual_size = event->arg3;
				}
			});

	ktrace_set_completion_handler(s, ^{
		ktrace_session_destroy(s);
		T_ASSERT_EQ(1, malloc_options_events, "Saw malloc_options tracepoint");
		T_ASSERT_EQ(expected_alignment, actual_alignment, NULL);
		T_ASSERT_EQ(expected_size, actual_size, NULL);
		T_END;
	});

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()), NULL);

	void *ptr = malloc_zone_malloc_with_options(NULL, expected_alignment,
			expected_size, MALLOC_ZONE_MALLOC_OPTION_CLEAR);
	T_ASSERT_NOTNULL(ptr, "allocate");
	T_ASSERT_TRUE(check_zeroed_memory(ptr, expected_size), "zeroed");
	T_ASSERT_TRUE(check_ptr_is_aligned(ptr, expected_alignment), "aligned");
	free(ptr);

	ktrace_end(s, false);

	dispatch_main();
}
#endif // !MALLOC_TARGET_EXCLAVES

T_DECL(malloc_options_alignment, "malloc with options, alignment argument",
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_PREFERRED)
{
	void *ptr;
	unsigned align = MALLOC_ZONE_MALLOC_DEFAULT_ALIGN;
	for (unsigned size = 0; size <= 32; ++size) {
		ptr = malloc_zone_malloc_with_options(NULL,
				MALLOC_ZONE_MALLOC_DEFAULT_ALIGN, size,
				MALLOC_ZONE_MALLOC_OPTION_NONE);
		T_ASSERT_NOTNULL(ptr, "allocate default alignment %u with size %u",
				align, size);
		free(ptr);
	}

	align = 16;
	for (unsigned size = 0; size <= 32; ++size) {
		ptr = malloc_zone_malloc_with_options(NULL,
				align, size, MALLOC_ZONE_MALLOC_OPTION_NONE);
		if (size % align) {
			T_ASSERT_NULL(ptr, "allocate non-default alignment %u with non-multiple size %u",
					align, size);
		} else {
			T_ASSERT_NOTNULL(ptr, "allocate non-default alignment %u with multiple size %u",
					align, size);
		}
		free(ptr);
	}
}
