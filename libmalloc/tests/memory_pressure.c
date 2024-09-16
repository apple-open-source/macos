#include <darwintest.h>
#include <dispatch/dispatch.h>
#include <malloc/malloc.h>
#include <os/lock.h>
#include <stdlib.h>
#include <sys/queue.h>

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
