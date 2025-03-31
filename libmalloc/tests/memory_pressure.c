#include <darwintest.h>
#include <dispatch/dispatch.h>
#include <malloc/malloc.h>
#include <os/lock.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "../src/platform.h"
#include "xzone_testing.h"

#if TARGET_OS_WATCH
#define TEST_TIMEOUT 1200
#endif // TARGET_OS_WATCH

TAILQ_HEAD(thead, entry);
struct entry {
	TAILQ_ENTRY(entry) next;
};

static void
stress(size_t sz, size_t cnt)
{
	struct thead head = TAILQ_HEAD_INITIALIZER(head);
	TAILQ_INIT(&head);

	for (int t=0; t<100; t++) {
		for (int i=0; i<cnt; i++) {
			struct entry *p = calloc(1, sz);
			T_QUIET; T_ASSERT_NOTNULL(p, "Failed to make allocation with size %zu", sz);
			TAILQ_INSERT_TAIL(&head, p, next);
		}
		int i=0;
		struct entry *p;
		while ((p = TAILQ_FIRST(&head)) != NULL) {
			TAILQ_REMOVE(&head, p, next);
			free((void *)p);
			i++;
		}
	}
}

T_DECL(tiny_mem_pressure, "tiny memory pressure",
#if TARGET_OS_WATCH
		T_META_TIMEOUT(TEST_TIMEOUT),
#endif // TARGET_OS_WATCH
		T_META_ENVVAR("MallocDebugReport=stderr"),
		T_META_ENVVAR("MallocScribble=1"),
		T_META_ENVVAR("MallocSpaceEfficient=1"),
		T_META_ENVVAR("MallocMaxMagazines=1"),
		T_META_TAG_VM_NOT_PREFERRED,
		T_META_CHECK_LEAKS(false))
{
	dispatch_queue_t q = dispatch_queue_create("pressure queue", 0); // serial
	dispatch_async(q, ^{
		while (1) {
			malloc_zone_pressure_relief(0, 0);
			usleep(100000);
		}
	});
	stress(128, 50000);
	T_PASS("didn't crash");
}

T_DECL(small_mem_pressure, "small memory pressure thread",
		T_META_TAG_VM_NOT_PREFERRED,
		T_META_RUN_CONCURRENTLY(true),
#if TARGET_OS_WATCH
		T_META_TIMEOUT(TEST_TIMEOUT),
#endif // TARGET_OS_WATCH
#if TARGET_OS_OSX
		T_META_ALL_VALID_ARCHS(true), // test Rosetta
		// darwintest multi-arch support relies on the first line of stderr
		// being reserved for arch(1) complaining about a given slice being
		// unsupported, so we can only put the malloc debug reporting on stderr
		// when we don't need that
		T_META_ENVVAR("MallocDebugReport=none"),
#else // TARGET_OS_OSX
		T_META_ENVVAR("MallocDebugReport=stderr"),
#endif // TARGET_OS_OSX
		T_META_ENVVAR("MallocScribble=1"),
		T_META_ENVVAR("MallocSpaceEfficient=1"),
		T_META_ENVVAR("MallocMaxMagazines=1"),
		T_META_CHECK_LEAKS(false))
{
	dispatch_queue_t q = dispatch_queue_create("pressure queue", 0); // serial
	dispatch_async(q, ^{
		while (1) {
			malloc_zone_pressure_relief(0, 0);
			usleep(10000);
		}
	});
	stress(512, 20000);
	T_PASS("didn't crash");
}

// Disabled until rdar://83904507 is fixed
//
// Need to compile the test out entirely because T_META_MAYFAIL doesn't handle
// test crashes - rdar://86164532
#if 0

T_DECL(medium_mem_pressure, "medium memory pressure thread",
#if TARGET_OS_WATCH
		T_META_TIMEOUT(TEST_TIMEOUT),
#endif // TARGET_OS_WATCH
		T_META_ENVVAR("MallocDebugReport=stderr"),
		T_META_ENVVAR("MallocScribble=1"),
		T_META_ENVVAR("MallocSpaceEfficient=1"),
		T_META_ENVVAR("MallocMaxMagazines=1"),
		T_META_MAYFAIL("Disabled until rdar://83904507 is fixed"),
		T_META_CHECK_LEAKS(false))
{
	dispatch_queue_t q = dispatch_queue_create("pressure queue", 0); // serial
	dispatch_async(q, ^{
		while (1) {
			malloc_zone_pressure_relief(0, 0);
			usleep(100000);
		}
	});
	stress(64*1024, 1000);
	T_PASS("didn't crash");
}

#endif

#if CONFIG_MALLOC_PROCESS_IDENTITY && CONFIG_XZONE_MALLOC
#if !HAVE_MALLOC_TYPE
#error "must have _MALLOC_TYPE_ENABLED"
#endif // !HAVE_MALLOC_TYPE

T_DECL(xzone_mem_pressure, "xzone memory pressure",
		T_META_ENVVAR("MallocNanoZone=1"),
		T_META_ENVVAR("MallocProbGuard=0"),
		T_META_TAG("no_debug"),
		T_META_TAG_NANO_ON_XZONE,
		T_META_TAG_XZONE_ONLY)
{
#define XZM_DATA_RANGE_SIZE    GiB(10)
	xzm_malloc_zone_t zone = get_default_xzone_zone();
	xzm_slice_kind_t kind;
	xzm_segment_group_id_t sgid;
	xzm_xzone_bucket_t bucket;

	struct data {
		size_t dummy[XZM_DATA_RANGE_SIZE / sizeof(size_t)];
	};
	void *data_alloc = malloc(sizeof(struct data));
#if TARGET_OS_OSX
	bool data_large = DEFAULT_LARGE_CACHE_ENABLED;
#if CONFIG_XZM_CLUSTER_AWARE
	data_large = true;
#elif CONFIG_LARGE_CACHE && !MALLOC_TARGET_EXCLAVES
	const char *env = getenv("MallocLargeCache")
	if (env) {
		data_large = env && !strcmp(env, "1");
	}
#endif

	T_ASSERT_NOTNULL(data_alloc, "data allocation succeeded");
	T_ASSERT_TRUE(xzm_ptr_lookup_4test(zone, data_alloc, &kind, &sgid, &bucket),
			"data allocation lookup");
	T_ASSERT_EQ(kind, XZM_SLICE_KIND_HUGE_CHUNK, "huge chunk");
	T_ASSERT_EQ(sgid, data_large ?
			XZM_SEGMENT_GROUP_DATA_LARGE : XZM_SEGMENT_GROUP_DATA,
			"data segment group");
	T_ASSERT_EQ(bucket, XZM_XZONE_BUCKET_DATA, "data bucket");
	free(data_alloc);
#else
	T_ASSERT_NULL(data_alloc, "data allocation failed");
#endif

#define XZM_POINTER_RANGE_SIZE GiB(8)
#define NUM_ALLOCS ((XZM_POINTER_RANGE_SIZE / XZM_LARGE_BLOCK_SIZE_MAX) + 1)
	struct pointer {
		void *dummy[XZM_LARGE_BLOCK_SIZE_MAX / sizeof(void *)];
	};
	// Exhaust this type in the early allocator
    for (unsigned i = 0; i < 1000; i++) {
        void *ptr = malloc(sizeof(struct pointer));
        T_QUIET; T_ASSERT_NOTNULL(ptr, "early malloc");
        free(ptr);
    }

	void *ptr_allocs[NUM_ALLOCS];
#if !TARGET_OS_OSX
	bool did_succeed = false, did_fail = false;
#endif
	for (size_t i = 0; i < NUM_ALLOCS; ++i) {
		ptr_allocs[i] = malloc(sizeof(struct pointer));
#if TARGET_OS_OSX
		T_ASSERT_NOTNULL(ptr_allocs[i], "pointer allocation succeeded");
		bucket = (xzm_xzone_bucket_t)~0u;
		T_ASSERT_TRUE(xzm_ptr_lookup_4test(zone, ptr_allocs[i], &kind, &sgid,
				&bucket), "pointer allocation lookup");
		T_ASSERT_EQ(kind, XZM_SLICE_KIND_LARGE_CHUNK, "large chunk");
		T_ASSERT_EQ(sgid, XZM_SEGMENT_GROUP_POINTER_LARGE,
				"pointer segment group");
		T_ASSERT_EQ(bucket, (xzm_xzone_bucket_t)~0u, "no bucket");
#else
		if (ptr_allocs[i]) {
			did_succeed = true;
		} else {
			did_fail = true;
		}
#endif
	}
#if !TARGET_OS_OSX
	T_ASSERT_TRUE(did_succeed, "a pointer allocation succeeded");
	T_ASSERT_TRUE(did_fail, "a pointer allocation failed, presumably after exhausting memory");
#endif

	for (size_t i = 0; i < NUM_ALLOCS; ++i) {
		free(ptr_allocs[i]);
	}
}
#endif

T_DECL(tiny_mem_pressure_multi, "test memory pressure in tiny on threads",
#if TARGET_OS_WATCH
		T_META_TIMEOUT(TEST_TIMEOUT),
#endif // TARGET_OS_WATCH
		T_META_TAG_VM_NOT_PREFERRED,
		T_META_CHECK_LEAKS(false)) {
	dispatch_group_t g = dispatch_group_create();
	for (int i=0; i<16; i++) {
		dispatch_group_async(g, dispatch_get_global_queue(0, 0), ^{
			stress(128, 100000);
		});
	}
	dispatch_group_notify(g, dispatch_get_global_queue(0, 0), ^{
		T_PASS("didn't crash!");
		T_END;
	});
	dispatch_release(g);

	while (1) {
		T_LOG("malloc_zone_pressure_relief");
		malloc_zone_pressure_relief(malloc_default_zone(), 0);
		sleep(1);
	}
}

T_DECL(small_mem_pressure_multi, "test memory pressure in small on threads",
		T_META_TAG_VM_NOT_PREFERRED,
		T_META_RUN_CONCURRENTLY(true),
#if TARGET_OS_WATCH
		T_META_TIMEOUT(TEST_TIMEOUT),
#endif // TARGET_OS_WATCH
		T_META_CHECK_LEAKS(false)) {
	dispatch_group_t g = dispatch_group_create();
	for (int i=0; i<3; i++) {
		dispatch_group_async(g, dispatch_get_global_queue(0, 0), ^{
			stress(1024, 100000);
		});
	}
	dispatch_group_notify(g, dispatch_get_global_queue(0, 0), ^{
		T_PASS("didn't crash!");
		T_END;
	});
	dispatch_release(g);

	while (1) {
		T_LOG("malloc_zone_pressure_relief");
		malloc_zone_pressure_relief(malloc_default_zone(), 0);
		sleep(1);
	}
}

T_DECL(medium_mem_pressure_multi, "test memory pressure in medium on threads",
#if TARGET_OS_WATCH
		T_META_TIMEOUT(TEST_TIMEOUT),
#endif // TARGET_OS_WATCH
		T_META_CHECK_LEAKS(false),
		T_META_TAG_VM_NOT_PREFERRED,
		T_META_RUN_CONCURRENTLY(true)) {
	dispatch_group_t g = dispatch_group_create();
	for (int i=0; i<30; i++) {
		dispatch_group_async(g, dispatch_get_global_queue(0, 0), ^{
			stress(64*1024, 1000);
		});
	}
	dispatch_group_notify(g, dispatch_get_global_queue(0, 0), ^{
		T_PASS("didn't crash!");
		T_END;
	});
	dispatch_release(g);

	while (1) {
		T_LOG("malloc_zone_pressure_relief");
		malloc_zone_pressure_relief(malloc_default_zone(), 0);
		sleep(1);
	}
}
