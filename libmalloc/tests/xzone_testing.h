#ifndef __XZONE_TESTING_H__
#define __XZONE_TESTING_H__

#include <darwintest.h>

#include <../src/internal.h>

#include "tmo_test_defs.h"

#if CONFIG_XZONE_MALLOC

#pragma mark shims

#define TESTING_XZONE_MALLOC 1

// On exclavekit, we only want one copy of the code enclosed here, which we'll
// arbitrarily build into the metapool tests - they just have to be built into
// one of them

#if !TARGET_OS_EXCLAVEKIT || defined(TESTING_METAPOOL)

#include "../src/vm.c"

void
malloc_report(uint32_t flags, const char *fmt, ...)
{
	T_LOG("malloc_report(): %s", fmt);
}

void
malloc_zone_error(uint32_t flags, bool is_corruption, const char *fmt, ...)
{
	__builtin_trap();
}

#endif // !TARGET_OS_EXCLAVEKIT || defined(TESTING_METAPOOL)

static void test_malloc_lock_lock(_malloc_lock_s *lock) {
#if MALLOC_HAS_OS_LOCK
	os_unfair_lock_lock(lock);
#else
	T_QUIET; T_ASSERT_EQ(pthread_mutex_lock(lock), 0, "Lock lock");
#endif // MALLOC_HAS_OS_LOCK
}
#define _malloc_lock_lock(lock) test_malloc_lock_lock(lock);

static void test_malloc_lock_unlock(_malloc_lock_s *lock) {
#if MALLOC_HAS_OS_LOCK
	os_unfair_lock_unlock(lock);
#else
	T_QUIET; T_ASSERT_EQ(pthread_mutex_unlock(lock), 0, "Unlock lock");
#endif // MALLOC_HAS_OS_LOCK
}
#define _malloc_lock_unlock(lock) test_malloc_lock_unlock(lock);

#if !MALLOC_TARGET_EXCLAVES && !defined(TESTING_METAPOOL)

// When not specifically testing the metapool, stub out these functions so that
// we can build code that uses them.

void *
xzm_metapool_alloc(xzm_metapool_t mp)
{
	__builtin_trap();
	return NULL;
}

void
xzm_metapool_free(xzm_metapool_t mp, void *blockp)
{
	__builtin_trap();
}

#endif // !MALLOC_TARGET_EXCLAVES && !defined(TESTING_METAPOOL)

#pragma mark TMO test helpers

#if defined(_MALLOC_TYPE_ENABLED) && _MALLOC_TYPE_ENABLED
#define HAVE_MALLOC_TYPE 1
#else
#define HAVE_MALLOC_TYPE 0
#endif

extern malloc_zone_t **malloc_zones;
extern int32_t malloc_num_zones;

static inline xzm_malloc_zone_t
get_default_xzone_zone(void)
{
	bool found_pgm = false;
	bool found_nano = false;

	unsigned i = 0;

	malloc_zone_t *zone = malloc_zones[i];

	const char *name = malloc_get_zone_name(zone);
	if ((zone->version >= 14 &&
			zone->introspect->zone_type == MALLOC_ZONE_TYPE_PGM) ||
			(name && !strcmp(name, "ProbGuardMallocZone"))) {
		found_pgm = true;

		i++;
		if (i == malloc_num_zones) {
			T_ASSERT_FAIL("didn't find xzone zone");
		}
		zone = malloc_zones[i];
	}

	T_ASSERT_GE(zone->version, 14, "zone version");

#if CONFIG_SANITIZER
	if (zone->introspect->zone_type == MALLOC_ZONE_TYPE_SANITIZER &&
			malloc_sanitizer_is_enabled()) {
		i++;
		if (i == malloc_num_zones) {
			T_ASSERT_FAIL("didn't find xzone zone");
		}
		zone = malloc_zones[i];
	}
#endif

	if (zone->introspect->zone_type != MALLOC_ZONE_TYPE_XZONE) {
		// Maybe it's nano?
		i++;
		if (i == malloc_num_zones) {
			T_ASSERT_FAIL("didn't find xzone xzone");
		}
		malloc_zone_t *helper_zone = malloc_zones[i];
		const char *helper_name = malloc_get_zone_name(helper_zone);
		if (helper_name && strcmp(helper_name, "MallocHelperZone") != 0) {
			T_ASSERT_FAIL("unexpected zone %s", helper_name);
		}

		found_nano = true;

		zone = helper_zone;
		T_ASSERT_GE(zone->version, 14, "helper zone version");
		T_ASSERT_EQ(zone->introspect->zone_type, MALLOC_ZONE_TYPE_XZONE,
				"helper zone is xzone malloc");
	}

	bool nano_on_xzone = false;
	enum {
		PGM_NO_EXPECTATION,
		PGM_EXPECTED_ENABLED,
		PGM_EXPECTED_DISABLED,
	} pgm_expectation = PGM_NO_EXPECTATION;

#if !MALLOC_TARGET_EXCLAVES
	nano_on_xzone = getenv("MallocNanoOnXzone");

	const char *pgm_env = getenv("MallocProbGuard");
	if (pgm_env) {
		pgm_expectation = (*pgm_env == '1' ? PGM_EXPECTED_ENABLED :
				PGM_EXPECTED_DISABLED);
	}
#endif

	T_ASSERT_EQ(nano_on_xzone, found_nano,
			"Nano state matched expectation (%d)", (int)nano_on_xzone);

	switch (pgm_expectation) {
	case PGM_NO_EXPECTATION:
		T_LOG("PGM enablement: %d (no expectation)", (int)found_pgm);
		break;
	case PGM_EXPECTED_ENABLED:
		T_ASSERT_TRUE(found_pgm, "PGM enabled");
		break;
	case PGM_EXPECTED_DISABLED:
		T_ASSERT_FALSE(found_pgm, "PGM disabled");
		break;
	default:
		T_ASSERT_FAIL("pgm_expectation");
		break;
	}

	return (xzm_malloc_zone_t)zone;
}

#define _TEST_STRINGIFY(x) #x
#define TEST_STRINGIFY(x) _TEST_STRINGIFY(x)

#define PTR_BUCKET_ENVVAR "MallocXzonePtrBucketCount=" \
		TEST_STRINGIFY(XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT)

static inline void
validate_bucket_distribution(xzm_malloc_zone_t zone, const char *expr,
		void **ptrs, size_t n, bool do_free, bool require_smooth_distribution)
{
	size_t counts[XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT] = { 0 };

	size_t early_count = 0;
	for (int i = 0; i < n; i++) {
		void *p = ptrs[i];

		T_QUIET; T_ASSERT_NOTNULL(p, "(%s) allocation not NULL", expr);

		xzm_slice_kind_t kind;
		xzm_segment_group_id_t sgid;
		xzm_xzone_bucket_t bucket;
		bool lookup = xzm_ptr_lookup_4test(zone, p, &kind, &sgid, &bucket);

		if (i == 0) {
			size_t size = malloc_size(p);
			T_LOG("(%s) malloc_size %zu", expr, size);
		}

		if (do_free) {
			free(p);
		}

		if (!lookup) {
			early_count++;
			continue;
		}

		T_QUIET; T_ASSERT_EQ((int)kind, XZM_SLICE_KIND_TINY_CHUNK,
				"tiny chunk");
		T_QUIET; T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_POINTER_XZONES,
				"xzone pointer segment group");
		T_QUIET; T_ASSERT_GE(bucket, XZM_XZONE_BUCKET_POINTER_BASE,
				"pointer bucket lower bound");
		T_QUIET; T_ASSERT_LT(bucket,
				XZM_XZONE_BUCKET_POINTER_BASE +
						XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT,
				"pointer bucket upper bound");

		counts[bucket - XZM_XZONE_BUCKET_POINTER_BASE]++;

	}

	T_LOG("(%s) %zu early allocations", expr, early_count);

	if (require_smooth_distribution) {
		for (int i = 0; i < XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT; i++) {
			T_EXPECT_GT(counts[i], (size_t)0,
					"(%s) expected nonzero allocations for bucket %d, found %zu",
					expr, i, counts[i]);
		}
	} else {
		// In practice, we can at least assert that the allocations didn't all
		// fall into a single bucket.  Although that's theoretically possible,
		// it's unlikely enough that it's worth flagging as a test failure that
		// we'll occasionally have to waive so that we can catch cases where the
		// type descriptor is getting lost entirely
		int nonzero_counts = 0;
		for (int i = 0; i < XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT; i++) {
			T_LOG("(%s) found %zu allocations for bucket %d", expr, counts[i],
					i);
			if (counts[i]) {
				nonzero_counts++;
			}
		}
		T_EXPECT_GT(nonzero_counts, 1,
				"(%s) expected at least two buckets with nonzero counts, found %d",
				expr, nonzero_counts);
	}
}

#endif // CONFIG_XZONE_MALLOC

#endif // __XZONE_TESTING_H__
