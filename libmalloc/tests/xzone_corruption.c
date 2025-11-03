#include <sys/queue.h>

#include <darwintest.h>
#include <../src/internal.h>

#if CONFIG_XZONE_MALLOC

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_VM_NOT_PREFERRED);

// Ensure that all allocations get the same bucketing
MALLOC_NOINLINE
static void *
malloc_wrapper(size_t n)
{
	return malloc_type_malloc(n, (malloc_type_id_t)42);
}

static bool
same_chunk(void *a, void *b)
{
	uintptr_t chunk_a =
			(((uintptr_t)a) & (XZM_LIMIT_ADDRESS - 1)) >> XZM_TINY_CHUNK_SHIFT;
	uintptr_t chunk_b =
			(((uintptr_t)b) & (XZM_LIMIT_ADDRESS - 1)) >> XZM_TINY_CHUNK_SHIFT;
	return chunk_a == chunk_b;
}

static void
test_freelist_corruption(bool linkage)
{
	pid_t child_pid = fork();
	T_ASSERT_NE(child_pid, -1, "fork()");

	if (!child_pid) {
		// Exhaust the early allocator
		for (int i = 0; i < 1000; i++) {
			void *p = malloc_wrapper(1024);
			free(p);
		}

		uint32_t seqno_cycles = arc4random_uniform(100000);
		uint32_t bit_to_flip = arc4random_uniform(64);

		// Get two allocations from the same chunk, so that we can free and
		// modify one without having to worry about the underlying page being
		// madvised
		void *p1, *p2;
		p1 = malloc_wrapper(1024);
		while (true) {
			p2 = malloc_wrapper(1024);

			if (same_chunk(p1, p2)) {
				// Cycle the sequence number for the chunk up
				for (uint32_t i = 0; i < seqno_cycles; i++) {
					free(p2);
					p2 = malloc_wrapper(1024);
				}

				// Retry a limited number of times, on the outside chance that
				// the signature happens to also be valid for the corrupted bits
				for (int i = 0; i < 5; i++) {
					free(p2);

					xzm_block_t block = p2;
					if (linkage) {
						block->xzb_linkage.xzbl_next_value ^= (1ull << bit_to_flip);
					} else {
						block->xzb_cookie ^= (1ull << bit_to_flip);
					}

					p2 = malloc_wrapper(1024);
				}
				T_ASSERT_FAIL("Corruption not detected");
			}

			p1 = p2;
			// this process is intended to crash, so we're not worried about
			// leaks
		}

		// Should not get here
		T_FAIL("Tiny linkage corruption test failed");
	} else {
		int status;
		pid_t wait_pid = waitpid(child_pid, &status, 0);
		T_ASSERT_EQ(wait_pid, child_pid, "Got child status");
		T_ASSERT_TRUE(WIFSIGNALED(status), "Child terminated by signal");
	}
}

T_DECL(tiny_freelist_cookie_corruption,
		"Crash on corruption of tiny freelist cookie",
		T_META_ENVVAR("MallocXzoneSlotConfig=0"),
		T_META_IGNORECRASHES("xzone_corruption"),
		T_META_TAG_XZONE_ONLY)
{
	test_freelist_corruption(false);
}

T_DECL(tiny_freelist_linkage_corruption,
		"Crash on corruption of tiny freelist linkage",
		T_META_ENVVAR("MallocXzoneSlotConfig=0"),
		T_META_IGNORECRASHES("xzone_corruption"),
		T_META_TAG_XZONE_ONLY,
		T_META_ENABLED(__has_feature(ptrauth_calls)))
{
	test_freelist_corruption(true);
}

#else // CONFIG_XZONE_MALLOC

T_DECL(tiny_freelist_corruption, "Crash on corruption of tiny freelist",
		T_META_ENABLED(false), T_META_TAG_VM_PREFERRED,
		T_META_TAG_NO_ALLOCATOR_OVERRIDE)
{
	T_SKIP("Nothing to test for !CONFIG_XZONE_MALLOC");
}

#endif // CONFIG_XZONE_MALLOC
