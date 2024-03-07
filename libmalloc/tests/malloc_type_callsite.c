#include "xzone_testing.h"

#if CONFIG_XZONE_MALLOC

#if __has_feature(typed_memory_operations)
#error "test must build without TMO"
#endif

#define _TEST_STRINGIFY(x) #x
#define TEST_STRINGIFY(x) _TEST_STRINGIFY(x)

#define PTR_BUCKET_ENVVAR "MallocXzonePtrBucketCount=" \
		TEST_STRINGIFY(XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT)

volatile int dummy;

// duplicated to malloc_type_callsite_cpp.cpp
//
// The dummy stores are to prevent all of the callsites from being the same
// number of instructions apart, as that can make it through our scrambling of
// the type descriptor and cause us to fail to distribute across the buckets as
// intended
#define _CALL_10(expr) \
		(expr); \
		(expr); \
		dummy = 42; \
		(expr); \
		dummy = 42; \
		dummy = 42; \
		(expr); \
		(expr); \
		(expr); \
		dummy = 42; \
		(expr); \
		(expr); \
		(expr); \
		(expr)

#define CALL_100(expr) \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr)

#define N_UNIQUE_CALLSITES 200

// must match constant above
#define CALL_N_CALLSITES(expr) \
		CALL_100(expr); \
		CALL_100(expr)

void
call_n_cpp_new(void **ptrs);

void
delete_n_cpp(void **ptrs);

static void
validate_bucket_distribution(xzm_malloc_zone_t zone, const char *expr,
		void **ptrs, bool do_free)
{
	size_t counts[XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT] = { 0 };

	size_t early_count = 0;
	for (int i = 0; i < N_UNIQUE_CALLSITES; i++) {
		void *p = ptrs[i];

		T_QUIET; T_ASSERT_NOTNULL(p, "(%s) allocation not NULL", expr);

		xzm_slice_kind_t kind;
		xzm_segment_group_id_t sgid;
		xzm_xzone_bucket_t bucket;
		bool lookup = xzm_ptr_lookup_4test(zone, p, &kind, &sgid, &bucket);

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

	for (int i = 0; i < XZM_XZONE_DEFAULT_POINTER_BUCKET_COUNT; i++) {
		T_EXPECT_GT(counts[i], (size_t)0,
				"(%s) expected nonzero allocations for bucket %d, found %zu",
				expr, i, counts[i]);
	}
}

static void
test_bucketing(void)
{
	xzm_malloc_zone_t zone = get_default_xzone_zone();

	void *ptrs[N_UNIQUE_CALLSITES] = { NULL };

	int i = 0;
	CALL_N_CALLSITES(({ ptrs[i] = malloc(256); i++; }));
	validate_bucket_distribution(zone, "malloc(256)", ptrs, true);

	i = 0;
	CALL_N_CALLSITES(({ ptrs[i] = calloc(1, 256); i++; }));
	validate_bucket_distribution(zone, "calloc(1, 256)", ptrs, true);

	i = 0;
	CALL_N_CALLSITES(({ ptrs[i] = realloc(NULL, 256); i++; }));
	validate_bucket_distribution(zone, "realloc(NULL, 256)", ptrs, true);

	i = 0;
	CALL_N_CALLSITES(({
		void *p = malloc(128);
		ptrs[i] = realloc(p, 256);
		i++;
	}));
	validate_bucket_distribution(zone, "realloc(p, 256)", ptrs, true);

	i = 0;
	CALL_N_CALLSITES(({
		void *p = NULL;
		int rc = posix_memalign(&p, 64, 256);
		ptrs[i] = rc ? NULL : p;
		i++;
	}));
	validate_bucket_distribution(zone, "posix_memalign(&p, 64, 256)", ptrs,
			true);

	i = 0;
	CALL_N_CALLSITES(({
		ptrs[i] = malloc_zone_malloc_with_options_np(NULL, 1, 256, 0);
		i++;
	}));
	validate_bucket_distribution(zone, "malloc_zone_malloc_with_options_np",
			ptrs, true);

#if 0
	call_n_cpp_new(ptrs);
	validate_bucket_distribution(zone, "operator new", ptrs, false);
	delete_n_cpp(ptrs);
#endif
}

// The "no_debug" tag is needed to opt out of using the libmalloc debug dylib,
// which is compiled without optimization.  We depend on tail-call optimization
// to allow us to get the true return address.

T_DECL(malloc_type_callsite_fastpath,
		"Validate bucketing for callsite type descriptors from fast path",
		T_META_TAG_XZONE_ONLY,
		T_META_TAG("no_debug"),
		T_META_ENVVAR(PTR_BUCKET_ENVVAR))
{
	test_bucketing();
}

T_DECL(malloc_type_callsite_slowpath,
		"Validate bucketing for callsite type descriptors from fast path",
		T_META_TAG_XZONE_ONLY,
		T_META_TAG("no_debug"),
		T_META_ENVVAR("MallocTracing=1"), // enable tracing to activate slowpath
		T_META_ENVVAR(PTR_BUCKET_ENVVAR))
{
	test_bucketing();
}

#else // CONFIG_XZONE_MALLOC

T_DECL(malloc_type_callsite_fastpath,
		"Validate bucketing for callsite type descriptors from fast path",
		T_META_ENABLED(false))
{
	T_SKIP("Nothing to test under !CONFIG_XZONE_MALLOC");
}

#endif // CONFIG_XZONE_MALLOC
