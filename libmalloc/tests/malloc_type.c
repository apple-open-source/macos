#include <stdlib.h>
#include <malloc/malloc.h>
#include <malloc_private.h>

#if !TARGET_OS_EXCLAVECORE
#include <objc/message.h>

typedef id (*alloc)(Class, SEL);
typedef id (*release)(void *, SEL);
#endif // !TARGET_OS_EXCLAVECORE

#include "../src/platform.h"

#include "xzone_testing.h"

// Not covered:
// - DriverKit (this test doesn't build for it)


// Build-time test: we should have the _MALLOC_TYPE_ENABLED macro exactly where
// we intend to
#if defined(__LP64__) && (TARGET_OS_IOS || TARGET_OS_VISION || __is_target_os(watchos) || TARGET_OS_TV || TARGET_OS_OSX || \
		TARGET_OS_EXCLAVEKIT || TARGET_OS_EXCLAVECORE)
#if !HAVE_MALLOC_TYPE
#error "must have _MALLOC_TYPE_ENABLED"
#endif // !HAVE_MALLOC_TYPE
#else
#if HAVE_MALLOC_TYPE
#error "must not have _MALLOC_TYPE_ENABLED"
#endif // HAVE_MALLOC_TYPE
#endif

void *
cpp_new_data(void);

void
cpp_delete_data(void *p);

void *
cpp_new_ptr(void);

void
cpp_delete_ptr(void *p);

#if CONFIG_XZONE_MALLOC
static inline bool
have_data_large(xzm_malloc_zone_t zone) {
	return _xzm_malloc_zone_main(zone)->xzmz_defer_large;
}
#endif // CONFIG_XZONE_MALLOC

T_DECL(malloc_type_placement, "End-to-end type isolation test",
		T_META_ENVVAR("MallocNanoZone=1"),
#if TARGET_OS_WATCH && HAVE_MALLOC_TYPE
		T_META_ENVVAR(PTR_BUCKET_ENVVAR), // disables narrow bucketing
#endif
		T_META_TAG_XZONE_ONLY,
		T_META_TAG_XZONE_AND_PGM,
		T_META_TAG_VM_NOT_ELIGIBLE)
{
#if HAVE_MALLOC_TYPE
#if !MALLOC_TARGET_EXCLAVES
	malloc_set_thread_options((malloc_thread_options_t){
		.DisableProbabilisticGuardMalloc = true,
	});
#endif

	xzm_malloc_zone_t zone = get_default_xzone_zone();

	xzm_slice_kind_t kind;
	xzm_segment_group_id_t sgid;
	xzm_xzone_bucket_t bucket;

	struct test_xzone_data {
		int a;
		uint8_t pad[512]; // pad so we don't fall into nano-on-xzone
	};

	// Exhaust this type in the early allocator
	for (int i = 0; i < 1000; i++) {
		void *a = malloc(sizeof(struct test_xzone_data));
		T_QUIET; T_ASSERT_NOTNULL(a, "early malloc");
		free(a);
	}

	T_LOG("tiny C data");

	void *ptr = malloc(sizeof(struct test_xzone_data));
	T_QUIET; T_ASSERT_NOTNULL(ptr, "malloc");

	bool lookup = xzm_ptr_lookup_4test(zone, ptr, &kind, &sgid, &bucket);
	T_QUIET; T_ASSERT_TRUE(lookup, "lookup");

	T_ASSERT_EQ((int)kind, XZM_SLICE_KIND_TINY_CHUNK, "tiny chunk");
	T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_DATA, "data segment group");
	T_ASSERT_EQ((int)bucket, XZM_XZONE_BUCKET_DATA, "data bucket");

	free(ptr);

	T_LOG("tiny C++ data");

	// C++ pure data
	ptr = cpp_new_data();

	lookup = xzm_ptr_lookup_4test(zone, ptr, &kind, &sgid, &bucket);
	T_QUIET; T_ASSERT_TRUE(lookup, "lookup");

	T_ASSERT_EQ((int)kind, XZM_SLICE_KIND_TINY_CHUNK, "tiny chunk");
	T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_DATA, "data segment group");
	T_ASSERT_EQ((int)bucket, XZM_XZONE_BUCKET_DATA, "data bucket");

	cpp_delete_data(ptr);

#if !TARGET_OS_EXCLAVECORE
	T_LOG("tiny ObjC");

	// To test ObjC allocations, use NSObject
	// Skip for nano-on-xzone because it's not big enough to fall through to
	// xzone malloc

	Class c_nsobject = objc_getClass("NSObject");
	SEL s_alloc = sel_registerName("alloc");
	SEL s_release = sel_registerName("release");

	// Exhaust this type in the early allocator
	for (int i = 0; i < 1000; i++) {
		id obj = ((alloc)objc_msgSend)(c_nsobject, s_alloc);
		((release)objc_msgSend)(obj, s_release);
	}

	id obj = ((alloc)objc_msgSend)(c_nsobject, s_alloc);
	lookup = xzm_ptr_lookup_4test(zone, (void *)obj, &kind, &sgid, &bucket);
	T_QUIET; T_ASSERT_TRUE(lookup, "lookup");

	T_ASSERT_EQ((int)kind, XZM_SLICE_KIND_TINY_CHUNK, "tiny chunk");
	T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_POINTER_XZONES,
			"xzone pointer segment group");
	T_ASSERT_EQ((int)bucket, XZM_XZONE_BUCKET_OBJC, "ObjC bucket");

	((release)objc_msgSend)(obj, s_release);
#endif // !TARGET_OS_EXCLAVECORE

	T_LOG("tiny C pointer");

	struct test_xzone_pointer {
		void *p;
		uint8_t pad[512];
	};

	// Exhaust this type in the early allocator
	for (int i = 0; i < 1000; i++) {
		void *a = malloc(sizeof(struct test_xzone_pointer));
		T_QUIET; T_ASSERT_NOTNULL(a, "early malloc");
		free(a);
	}

	ptr = malloc(sizeof(struct test_xzone_pointer));
	T_QUIET; T_ASSERT_NOTNULL(ptr, "malloc");

	lookup = xzm_ptr_lookup_4test(zone, ptr, &kind, &sgid, &bucket);
	T_QUIET; T_ASSERT_TRUE(lookup, "lookup");

	T_ASSERT_EQ((int)kind, XZM_SLICE_KIND_TINY_CHUNK, "tiny chunk");
	T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_POINTER_XZONES,
			"xzone pointer segment group");
	T_ASSERT_GE((int)bucket, XZM_XZONE_BUCKET_POINTER_BASE,
			"pointer bucket");
	T_ASSERT_LT((int)bucket, XZM_XZONE_DEFAULT_BUCKET_COUNT,
			"pointer bucket range");

	free(ptr);

	T_LOG("tiny C++ pointer");

	ptr = cpp_new_ptr();

	lookup = xzm_ptr_lookup_4test(zone, ptr, &kind, &sgid, &bucket);
	T_QUIET; T_ASSERT_TRUE(lookup, "lookup");

	T_ASSERT_EQ((int)kind, XZM_SLICE_KIND_TINY_CHUNK, "tiny chunk");
	T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_POINTER_XZONES,
			"xzone pointer segment group");
	T_ASSERT_GE((int)bucket, XZM_XZONE_BUCKET_POINTER_BASE,
			"pointer bucket");
	T_ASSERT_LT((int)bucket, XZM_XZONE_DEFAULT_BUCKET_COUNT,
			"pointer bucket range");

	cpp_delete_ptr(ptr);

	T_LOG("large C pointer");

	struct test_large_pointer {
		void *p;
		char data[1 << 16];
	};

	ptr = malloc(sizeof(struct test_large_pointer));
	T_QUIET; T_ASSERT_NOTNULL(ptr, "malloc");

	lookup = xzm_ptr_lookup_4test(zone, ptr, &kind, &sgid, &bucket);
	T_QUIET; T_ASSERT_TRUE(lookup, "lookup");

	T_ASSERT_EQ((int)kind, XZM_SLICE_KIND_LARGE_CHUNK, "large chunk");
	T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_POINTER_LARGE,
			"large pointer segment group");

	free(ptr);

	T_LOG("large C data");

	const bool data_large = have_data_large(zone);
	struct test_large_data {
		char data[1 << 16];
	};

	ptr = malloc(sizeof(struct test_large_data));
	T_QUIET; T_ASSERT_NOTNULL(ptr, "malloc");

	lookup = xzm_ptr_lookup_4test(zone, ptr, &kind, &sgid, &bucket);
	T_QUIET; T_ASSERT_TRUE(lookup, "lookup");

	T_ASSERT_EQ((int)kind, XZM_SLICE_KIND_LARGE_CHUNK, "large chunk");
	if (data_large) {
		T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_DATA_LARGE, "data segment group");
	} else {
		T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_DATA, "data segment group");
	}

	free(ptr);

	T_LOG("huge C pointer");

	struct test_huge_pointer {
		void *p;
		char data[1 << 24];
	};

	ptr = malloc(sizeof(struct test_huge_pointer));
	T_QUIET; T_ASSERT_NOTNULL(ptr, "malloc");

	lookup = xzm_ptr_lookup_4test(zone, ptr, &kind, &sgid, &bucket);
	T_QUIET; T_ASSERT_TRUE(lookup, "lookup");

	T_ASSERT_EQ((int)kind, XZM_SLICE_KIND_HUGE_CHUNK, "huge chunk");
	if (data_large) {
		T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_DATA_LARGE, "data segment group");
	} else {
		T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_DATA, "data segment group");
	}

	free(ptr);
#else // HAVE_MALLOC_TYPE
	T_SKIP("Test not applicable without malloc_type support");
#endif // HAVE_MALLOC_TYPE
}

#if HAVE_MALLOC_TYPE

T_DECL(reuse_large_data_as_tiny,
		"Verify that calloc returns zero'd memory when reusing VA",
#if TARGET_OS_WATCH
		T_META_ENVVAR(PTR_BUCKET_ENVVAR), // disables narrow bucketing
#endif
		T_META_ENVVAR("MallocXzoneGuarded=1"),
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_NOT_ELIGIBLE)
{
	// This test is here specifically to test the regression that caused
	// rdar://123605357, in which preallocated tiny chunks below the zero on
	// free threshold were not cleared, leading calloc to return nonzero memory

	malloc_type_id_t data = (malloc_type_descriptor_t){
		.summary.layout_semantics.generic_data = true,
	}.type_id;

	// Allocate a LARGE block and scribble it
	xzm_malloc_zone_t zone = get_default_xzone_zone();

	const bool data_large = have_data_large(zone);
	const size_t large_size = KiB(16) * 12; // 12 pages
	void *large_ptr = malloc_type_zone_malloc(&zone->xzz_basic_zone, large_size,
			data);
	T_ASSERT_NOTNULL(large_ptr, "Large allocation");

	xzm_slice_kind_t kind;
	xzm_segment_group_id_t sgid;
	xzm_xzone_bucket_t bucket;
	bool lookup = xzm_ptr_lookup_4test(zone, large_ptr, &kind, &sgid, &bucket);
	T_QUIET; T_ASSERT_TRUE(lookup, "Lookup large data pointer");
	T_ASSERT_EQ((int)kind, XZM_SLICE_KIND_LARGE_CHUNK, "Large chunk");
	if (data_large) {
		T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_DATA_LARGE, "data segment group");
	} else {
		T_ASSERT_EQ((int)sgid, XZM_SEGMENT_GROUP_DATA, "data segment group");
	}

	memset(large_ptr, 0xbe, large_size);

	malloc_zone_free(&zone->xzz_basic_zone, large_ptr);

	// Now calloc 12 pages worth of tiny pointers, and make sure that they're
	// all zero. Skip the test if none of these pointers overlap with the large
	// allocation
	const size_t tiny_size = 512;
	const size_t num_tiny_pointers = large_size / tiny_size;
	void *tiny_pointers[num_tiny_pointers] = { NULL };

	bool found_overlap = false;
	for (int i = 0; i < num_tiny_pointers; i++) {
		void *ptr = malloc_type_zone_calloc(&zone->xzz_basic_zone, 1, tiny_size,
				data);
		T_QUIET; T_ASSERT_NOTNULL(ptr, "Tiny allocation");

#if MALLOC_TARGET_EXCLAVES
		T_QUIET; T_ASSERT_TRUE(memisset(ptr, 0, tiny_size),
				"Calloc returns zero'd memory");
#else
		T_QUIET; T_ASSERT_TRUE(!_platform_memcmp_zero_aligned8(ptr,
				tiny_size), "Calloc returns zero'd memory");
#endif // MALLOC_TARGET_EXCLAVES

		if (!found_overlap && (uintptr_t)ptr >= (uintptr_t)large_ptr &&
				(uintptr_t)ptr < ((uintptr_t)large_ptr + large_size)) {
			found_overlap = true;
		}

		tiny_pointers[i] = ptr;
	}

	for (int i = 0; i < num_tiny_pointers; i++) {
		malloc_zone_free(&zone->xzz_basic_zone, tiny_pointers[i]);
	}

	if (!found_overlap) {
		T_SKIP("Tiny pointers never overlapped with large region");
	}
}

#if !MALLOC_TARGET_EXCLAVES

void
test_swift_bucketing(void);

void
validate_swift_obj_array(void **ptrs)
{
	xzm_malloc_zone_t zone = get_default_xzone_zone();

	validate_bucket_distribution(zone, "swift", ptrs, N_TEST_SWIFT_CLASSES,
			false, false);
}

#endif // !MALLOC_TARGET_EXCLAVES

void **
cpp_new_test_structs(void);

void
cpp_delete_test_structs(void **ptrs);

static void
test_bucket_distribution(void)
{
	xzm_malloc_zone_t zone = get_default_xzone_zone();

#if !MALLOC_TARGET_EXCLAVES
	malloc_set_thread_options((malloc_thread_options_t){
		.DisableProbabilisticGuardMalloc = true,
	});
#endif

	void *ptrs[N_TMO_TEST_STRUCTS] = { NULL };

	int i = 0;
#define tmo_malloc_test(type) (({ ptrs[i] = malloc(sizeof(type)); i++; }))
	FOREACH_TMO_TEST_STRUCT(INVOKE_FOR_TMO_TEST_STRUCT_TYPE, tmo_malloc_test);
	validate_bucket_distribution(zone, "malloc()", ptrs, N_TMO_TEST_STRUCTS,
			true, true);

	i = 0;
#define tmo_calloc_test(type) (({ ptrs[i] = calloc(1, sizeof(type)); i++; }))
	FOREACH_TMO_TEST_STRUCT(INVOKE_FOR_TMO_TEST_STRUCT_TYPE, tmo_calloc_test);
	validate_bucket_distribution(zone, "calloc()", ptrs, N_TMO_TEST_STRUCTS,
			true, true);

	i = 0;
#define tmo_realloc_null_test(type) (({ \
	ptrs[i] = realloc(NULL, sizeof(type)); \
	i++; \
}))
	FOREACH_TMO_TEST_STRUCT(INVOKE_FOR_TMO_TEST_STRUCT_TYPE,
			tmo_realloc_null_test);
	validate_bucket_distribution(zone, "realloc(NULL)", ptrs,
			N_TMO_TEST_STRUCTS, true, true);

	i = 0;
#define tmo_realloc_test(type) (({ \
		void *p = malloc(128); \
		ptrs[i] = realloc(p, sizeof(type)); \
		i++; \
}))
	FOREACH_TMO_TEST_STRUCT(INVOKE_FOR_TMO_TEST_STRUCT_TYPE,
			tmo_realloc_test);
	validate_bucket_distribution(zone, "realloc()", ptrs,
			N_TMO_TEST_STRUCTS, true, true);

	i = 0;
#define tmo_posix_memalign_test(type) (({ \
	void *p = NULL; \
	int rc = posix_memalign(&p, 64, sizeof(type)); \
	ptrs[i] = rc ? NULL : p; \
	i++; \
}))
	FOREACH_TMO_TEST_STRUCT(INVOKE_FOR_TMO_TEST_STRUCT_TYPE,
			tmo_posix_memalign_test);
	validate_bucket_distribution(zone, "posix_memalign()", ptrs,
			N_TMO_TEST_STRUCTS, true, true);

	i = 0;
#define tmo_aligned_alloc_test(type) (({ \
		ptrs[i] = aligned_alloc(128, sizeof(type)); \
		i++; \
}))
	FOREACH_TMO_TEST_STRUCT(INVOKE_FOR_TMO_TEST_STRUCT_TYPE,
			tmo_aligned_alloc_test);
	validate_bucket_distribution(zone, "aligned_alloc()", ptrs,
			N_TMO_TEST_STRUCTS, true, true);

	i = 0;
#define tmo_malloc_with_options_test(type) (({ \
	ptrs[i] = malloc_zone_malloc_with_options(NULL, MALLOC_ZONE_MALLOC_DEFAULT_ALIGN, \
			sizeof(type), MALLOC_ZONE_MALLOC_OPTION_NONE); \
	i++; \
}))
	FOREACH_TMO_TEST_STRUCT(INVOKE_FOR_TMO_TEST_STRUCT_TYPE,
			tmo_malloc_with_options_test);
	validate_bucket_distribution(zone, "malloc_zone_malloc_with_options()",
			ptrs, N_TMO_TEST_STRUCTS, true, true);

	void **cpp_ptrs = cpp_new_test_structs();
	T_ASSERT_NOTNULL(cpp_ptrs, "cpp_ptrs");
	validate_bucket_distribution(zone, "C++ new", cpp_ptrs, N_TMO_TEST_STRUCTS,
			false, true);
	cpp_delete_test_structs(cpp_ptrs);

#if !MALLOC_TARGET_EXCLAVES
	test_swift_bucketing();
#endif
}

T_DECL(malloc_type_bucket_distribution_fastpath,
		"Validate distribution over buckets from fast path",
		T_META_ENVVAR(PTR_BUCKET_ENVVAR),
		T_META_ENVVAR("MallocNanoZone=1"),
		T_META_TAG_XZONE_ONLY,
		T_META_TAG_XZONE_AND_PGM,
		T_META_TAG_VM_NOT_ELIGIBLE)
{
	test_bucket_distribution();
}

T_DECL(malloc_type_bucket_distribution_slowpath,
		"Validate distribution over buckets from slow path",
		T_META_ENVVAR(PTR_BUCKET_ENVVAR),
		T_META_ENVVAR("MallocNanoZone=1"),
		T_META_ENVVAR("MallocTracing=1"), // enable tracing to activate slowpath
		T_META_TAG_XZONE_ONLY,
		T_META_TAG_XZONE_AND_PGM,
		T_META_TAG_VM_NOT_ELIGIBLE)
{
	test_bucket_distribution();
}

static inline void
test_ptr_bucketing_function(size_t num_buckets)
{
	size_t distribution[num_buckets] = {};
	const size_t observations = 1 << 16;
	const size_t num_type_hashes = 1 << 12;
	const size_t total_assignments = observations * num_type_hashes;
	uint32_t *hashes = (uint32_t *)calloc(num_type_hashes, sizeof(uint32_t));
	arc4random_buf(hashes, sizeof(uint32_t) * num_type_hashes);

	malloc_type_descriptor_t type_desc = (malloc_type_descriptor_t){
		.summary.layout_semantics.anonymous_pointer = true,
	};

	for (size_t i = 0; i < observations; i++) {
		xzm_bucketing_keys_t keys;
		arc4random_buf(&keys, sizeof(keys));

		for (size_t j = 0; j < num_type_hashes; j++) {
			type_desc.hash = hashes[j];
			uint8_t bucket = xzm_type_choose_ptr_bucket_4test(&keys,
					num_buckets, type_desc);
			distribution[bucket]++;
		}
	}
	free(hashes);

	const double threshold = 0.01; // 1%
	const double lower_bucket_bound = (1.0 / (double)num_buckets) - threshold;
	const double upper_bucket_bound = (1.0 / (double)num_buckets) + threshold;
	for (int i = 0; i < num_buckets; i++) {
		double d = (((double)distribution[i]) / (double)total_assignments);
		T_EXPECT_GT(distribution[i], (size_t)0,
			"bucket %d: nonzero assignments (%zu)", i, distribution[i]);
		T_EXPECT_GT(d, lower_bucket_bound,
			"bucket %d: lower bound (%.2f > %.2f)", i, d, lower_bucket_bound);
		T_EXPECT_LT(d, upper_bucket_bound,
			"bucket %d: lower bound (%.2f < %.2f)", i, d, upper_bucket_bound);
	}
}

T_DECL(malloc_type_ptr_bucketing_function_2,
		"Validate distribution over pointer buckets (N=2)",
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_PREFERRED)
{
	test_ptr_bucketing_function(2);
}

T_DECL(malloc_type_ptr_bucketing_function_3,
		"Validate distribution over pointer buckets (N=3)",
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_PREFERRED)
{
	test_ptr_bucketing_function(3);
}

T_DECL(malloc_type_ptr_bucketing_function_4,
		"Validate distribution over pointer buckets (N=4)",
		T_META_TAG_XZONE_ONLY, T_META_TAG_VM_PREFERRED)
{
	test_ptr_bucketing_function(4);
}

#else // HAVE_MALLOC_TYPE

// still need this to be able to link malloc_type_objc
void
validate_swift_obj_array(void **ptrs)
{
	(void)ptrs;
}

#endif // HAVE_MALLOC_TYPE
