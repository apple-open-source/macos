#include "xzone_testing.h"

#if CONFIG_XZONE_MALLOC

#if __has_feature(typed_memory_operations)
#error "test must build without TMO"
#endif

void **
cpp_new_fallback(void);

void
cpp_delete_fallback(void **ptrs);

static void
test_bucketing(void)
{
	bool nano_on_xzone = getenv("MallocNanoOnXzone");
	xzm_malloc_zone_t zone = nano_on_xzone ?
			get_default_xzone_helper_zone() : get_default_xzone_zone();

	void *ptrs[N_UNIQUE_CALLSITES] = { NULL };

	int i = 0;
	CALL_N_CALLSITES(({ ptrs[i] = malloc(512); i++; }));
	validate_bucket_distribution(zone, "malloc(512)", ptrs, N_UNIQUE_CALLSITES,
			true);

	i = 0;
	CALL_N_CALLSITES(({ ptrs[i] = calloc(1, 512); i++; }));
	validate_bucket_distribution(zone, "calloc(1, 512)", ptrs,
			N_UNIQUE_CALLSITES, true);

	i = 0;
	CALL_N_CALLSITES(({ ptrs[i] = realloc(NULL, 512); i++; }));
	validate_bucket_distribution(zone, "realloc(NULL, 512)", ptrs,
			N_UNIQUE_CALLSITES, true);

	i = 0;
	CALL_N_CALLSITES(({
		void *p = malloc(128);
		ptrs[i] = realloc(p, 512);
		i++;
	}));
	validate_bucket_distribution(zone, "realloc(p, 512)", ptrs,
			N_UNIQUE_CALLSITES, true);

	i = 0;
	CALL_N_CALLSITES(({
		void *p = NULL;
		int rc = posix_memalign(&p, 64, 512);
		ptrs[i] = rc ? NULL : p;
		i++;
	}));
	validate_bucket_distribution(zone, "posix_memalign(&p, 64, 512)", ptrs,
			N_UNIQUE_CALLSITES, true);

	i = 0;
	CALL_N_CALLSITES(({ ptrs[i] = aligned_alloc(64, 512); i++; }));
	validate_bucket_distribution(zone, "aligned_alloc(64, 512)", ptrs,
			N_UNIQUE_CALLSITES, true);

	i = 0;
	CALL_N_CALLSITES(({
		ptrs[i] = malloc_zone_malloc_with_options_np(NULL, sizeof(void *), 512,
				0);
		i++;
	}));
	validate_bucket_distribution(zone, "malloc_zone_malloc_with_options_np",
			ptrs, N_UNIQUE_CALLSITES, true);

#if HAVE_MALLOC_TYPE
	// libc++ fallback bucketing is gated by TMO enablement, so we can only test
	// that on platforms with TMO enabled
	void **cpp_ptrs = cpp_new_fallback();
	T_ASSERT_NOTNULL(cpp_ptrs, "cpp_ptrs");
	validate_bucket_distribution(zone, "operator new", cpp_ptrs,
			N_UNIQUE_CALLSITES, false);
	cpp_delete_fallback(cpp_ptrs);
#endif // HAVE_MALLOC_TYPE
}

// The "no_debug" tag is needed to opt out of using the libmalloc debug dylib,
// which is compiled without optimization.  We depend on tail-call optimization
// to allow us to get the true return address.

T_DECL(malloc_type_callsite_fastpath,
		"Validate bucketing for callsite type descriptors from fast path",
		T_META_TAG_XZONE_ONLY,
		T_META_TAG_NANO_ON_XZONE,
		T_META_TAG("no_debug"),
		T_META_ENVVAR("MallocProbGuard=0"),
		T_META_ENVVAR("MallocNanoZone=1"),
		T_META_ENVVAR(PTR_BUCKET_ENVVAR))
{
	test_bucketing();
}

T_DECL(malloc_type_callsite_slowpath,
		"Validate bucketing for callsite type descriptors from slow path",
		T_META_TAG_XZONE_ONLY,
		T_META_TAG_NANO_ON_XZONE,
		T_META_TAG("no_debug"),
		T_META_ENVVAR("MallocTracing=1"), // enable tracing to activate slowpath
		T_META_ENVVAR("MallocProbGuard=0"),
		T_META_ENVVAR("MallocNanoZone=1"),
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
