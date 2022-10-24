#import "internal.h"

#import <XCTest/XCTest.h>

#define XCTAssertNotNull(ptr) XCTAssertNotEqual(ptr, NULL)

static void assert_zero(void *ptr, size_t len)
{
	char *p = ptr;

	// naive zero check
	for (size_t i = 0; i < len; i++) {
		XCTAssertEqual(p[i], 0, @"at byte %zu", i);
	}
}

static void
assert_freelist_block_zero(void *ptr, size_t len, bool cache)
{
	size_t orig_len = len;
	char *p = ptr;

	if (!cache) {
		// Skip the free list pointers
		p += sizeof(tiny_free_list_t);
		len -= sizeof(tiny_free_list_t);

		if (orig_len > TINY_QUANTUM) {
			// Skip leading inline size
			p += sizeof(msize_t);
			len -= sizeof(msize_t);

			// Skip trailing inline size
			len -= sizeof(msize_t);
		}
	}

	assert_zero(p, len);
}

@interface magazine_tiny_tests : XCTestCase {
@public
	struct rack_s tiny_rack;
}

- (void *)tiny_malloc:(size_t)size;
- (void *)tiny_calloc:(size_t)size;
- (void)tiny_free:(void *)ptr;

@end

@implementation magazine_tiny_tests

- (void)setUp {
	malloc_zero_on_free = true;

	memset(&tiny_rack, 'a', sizeof(tiny_rack));
	rack_init(&tiny_rack, RACK_TYPE_TINY, 1, 0);

	// make an arbitrary initial allocation just to make sure the region isn't
	// fully free at any point during the subsequent test
	(void)[self tiny_malloc:42];
}

- (void)tearDown {
	rack_destroy_regions(&tiny_rack, TINY_REGION_SIZE);
	rack_destroy(&tiny_rack);
}

- (void *)tiny_malloc:(size_t)size {
	return tiny_malloc_should_clear(&tiny_rack, TINY_MSIZE_FOR_BYTES(size), false);
}

- (void *)tiny_calloc:(size_t)size {
	return tiny_malloc_should_clear(&tiny_rack, TINY_MSIZE_FOR_BYTES(size), true);
}

- (void)tiny_free:(void *)ptr {
	region_t region = tiny_region_for_ptr_no_lock(&tiny_rack, ptr);
	XCTAssertNotNull(region);
	free_tiny(&tiny_rack, ptr, region, 0, false);
}

@end

@interface magazine_tiny_regular_tests : magazine_tiny_tests
@end

@implementation magazine_tiny_regular_tests

- (void)tearDown {
	XCTAssertNotEqual(tiny_check(&tiny_rack, 0), 0);
	[super tearDown];
}

- (void)testTinyMallocSucceeds {
	XCTAssertNotNull([self tiny_malloc:256]);
}

- (void)testTinyRegionFoundAfterMalloc {
	void *ptr = [self tiny_malloc:256];
	XCTAssertNotNull(ptr);

	XCTAssertNotNull(tiny_region_for_ptr_no_lock(&tiny_rack, ptr));
}

- (void)testTinySizeMatchesMalloc {
	void *ptr = [self tiny_malloc:256];
	XCTAssertNotNull(ptr);

	XCTAssertEqual(tiny_size(&tiny_rack, ptr), 256);
}

// A block freed to the tiny cache should be cleared
- (void)testTinyZeroOnFreeToCache {
	const size_t size = 64;

	void *ptr = [self tiny_malloc:size];
	memset(ptr, 'a', size);

	[self tiny_free:ptr];

	assert_freelist_block_zero(ptr, size, true);
}

// A block coalesced with a previous block should result in a cleared coalesced
// block
- (void)testTinyZeroOnFreeCoalescePrevious {
	const size_t size = 272; // skip the tiny cache

	void *ptr1 = [self tiny_malloc:size];
	memset(ptr1, 'a', size);

	void *ptr2 = [self tiny_malloc:size];
	memset(ptr2, 'b', size);

	XCTAssertEqual((uintptr_t)ptr1 + size, (uintptr_t)ptr2);

	[self tiny_free:ptr1];
	// Should coalesce backward with block 1
	[self tiny_free:ptr2];

	assert_freelist_block_zero(ptr1, size * 2, false);

	// Make sure calloc clears
	void *ptr3 = [self tiny_calloc:size];
	XCTAssertEqual(ptr3, ptr1);
	assert_zero(ptr3, size);
}

// A block coalesced with a next block should result in a cleared coalesced
// block
- (void)testTinyZeroOnFreeCoalesceNext {
	const size_t size = 272; // skip the tiny cache

	void *ptr1 = [self tiny_malloc:size];
	memset(ptr1, 'a', size);

	void *ptr2 = [self tiny_malloc:size];
	memset(ptr2, 'b', size);

	XCTAssertEqual((uintptr_t)ptr1 + size, (uintptr_t)ptr2);

	[self tiny_free:ptr2];
	// Should coalesce forward with block 2
	[self tiny_free:ptr1];

	assert_freelist_block_zero(ptr1, size * 2, false);

	// Make sure calloc clears
	void *ptr3 = [self tiny_calloc:size];
	XCTAssertEqual(ptr3, ptr1);
	assert_zero(ptr3, size);
}

// A block coalesced with a small next block should result in a cleared
// coalesced block
- (void)testTinyZeroOnFreeCoalesceNextSmall {
	const size_t size = 272; // skip the cache
	const size_t small_size = 64; // go through the cache

	void *ptr1 = [self tiny_malloc:size];
	memset(ptr1, 'a', size);

	void *ptr2 = [self tiny_malloc:small_size];
	memset(ptr2, 'b', small_size);

	void *ptr3 = [self tiny_malloc:small_size];
	memset(ptr3, 'c', small_size);

	XCTAssertEqual((uintptr_t)ptr1 + size, (uintptr_t)ptr2);
	XCTAssertEqual((uintptr_t)ptr2 + small_size, (uintptr_t)ptr3);

	[self tiny_free:ptr2];
	// Push block 2 out of the cache
	[self tiny_free:ptr3];
	// Should coalesce forward with block 2
	[self tiny_free:ptr1];

	assert_freelist_block_zero(ptr1, size + small_size, false);
}

// A leftover block should be cleared correctly
- (void)testTinyZeroOnFreeLeftover {
	const size_t size = 272; // skip the tiny cache

	void *ptr1 = [self tiny_malloc:size];
	memset(ptr1, 'a', size);

	void *ptr2 = [self tiny_malloc:size];
	memset(ptr2, 'b', size);

	void *ptr3 = [self tiny_malloc:size];
	memset(ptr3, 'c', size);

	XCTAssertEqual((uintptr_t)ptr1 + size, (uintptr_t)ptr2);
	XCTAssertEqual((uintptr_t)ptr2 + size, (uintptr_t)ptr3);

	// Should all coalesce together
	[self tiny_free:ptr1];
	[self tiny_free:ptr2];
	[self tiny_free:ptr3];

	// Now pull the first one off again
	void *ptr4 = [self tiny_malloc:size];
	XCTAssertEqual(ptr4, ptr1); // Should get ptr1 back

	// The leftover starting at ptr2 should be cleared correctly
	assert_freelist_block_zero(ptr2, size * 2, false);
}

// A leftover block from realloc should be cleared correctly
- (void)testTinyZeroOnFreeReallocLeftover {
	const size_t size = 272; // skip the tiny cache

	void *ptr1 = [self tiny_malloc:size];
	memset(ptr1, 'a', size);

	void *ptr2 = [self tiny_malloc:size];
	memset(ptr2, 'b', size);

	void *ptr3 = [self tiny_malloc:size];
	memset(ptr3, 'c', size);

	void *ptr4 = [self tiny_malloc:size];
	memset(ptr4, 'd', size);

	XCTAssertEqual((uintptr_t)ptr1 + size, (uintptr_t)ptr2);
	XCTAssertEqual((uintptr_t)ptr2 + size, (uintptr_t)ptr3);
	XCTAssertEqual((uintptr_t)ptr3 + size, (uintptr_t)ptr4);

	// Should coalesce together
	[self tiny_free:ptr2];
	[self tiny_free:ptr3];
	[self tiny_free:ptr4];

	// Now grow the first allocation into the free block after, which should
	// consume up until ptr3
	boolean_t result = tiny_try_realloc_in_place(&tiny_rack, ptr1, size,
			size * 2);
	XCTAssertEqual(result, 1);

	// The leftover starting at ptr3 should be cleared correctly
	assert_freelist_block_zero(ptr3, size * 2, false);
}

@end

@interface magazine_tiny_scribble_tests : magazine_tiny_tests
@end

@implementation magazine_tiny_scribble_tests

- (void)setUp {
	malloc_zero_on_free = true;
	aggressive_madvise_enabled = true;

	memset(&tiny_rack, 'a', sizeof(tiny_rack));
	rack_init(&tiny_rack, RACK_TYPE_TINY, 1, MALLOC_DO_SCRIBBLE);

	// make an arbitrary initial allocation just to make sure the region isn't
	// fully free at any point during the subsequent test
	(void)[self tiny_malloc:42];
}

// 128 * 256 == 32k
#define SCRIBBLE_TEST_ALLOCATIONS 128

- (void)testTinyZeroOnFreeScribbleCalloc {
	const size_t size = 256;

	// Allocate and free two full max-size pages, twice, to exercise madvise
	// scribbling logic
	for (int outer = 0; outer < 2; outer++) {
		void *allocations[SCRIBBLE_TEST_ALLOCATIONS];
		for (int i = 0; i < SCRIBBLE_TEST_ALLOCATIONS; i++) {
			allocations[i] = [self tiny_malloc:size];
		}
		for (int i = 0; i < SCRIBBLE_TEST_ALLOCATIONS; i++) {
			[self tiny_free:allocations[i]];
		}
	}

	// Now make sure we get back cleared allocations
	for (int i = 0; i < SCRIBBLE_TEST_ALLOCATIONS; i++) {
		void *allocation = [self tiny_calloc:size];
		assert_zero(allocation, size);
	}
}

@end

@interface magazine_tiny_corruption_tests : magazine_tiny_tests
@end

extern jmp_buf *zone_error_expected_jmp;

// The structure of these tests is:
// - setjmp() at the start
// - Set up an interesting state in tiny
// - Corrupt a free block at some offset to make sure we detect it
// - Perform an allocation that should get that free block
//
// That should result in an internal call to malloc_zone_error(), which is
// replaced by a mock that longjmp()s to the zone_error_expected_jmp global that
// we set to the jmp_buf from the beginning of the test.
//
// This allows us to test that a call to malloc_zone_error() occurs where we
// expect.
//
// To avoid potential issues mixing setjmp()/longjmp() and ObjC, the relevant
// test logic is in C functions.

typedef void (*corruption_test_fn_t)(magazine_tiny_corruption_tests *self,
		int offset, jmp_buf *env);

static void
run_corruption_test_at_offset(magazine_tiny_corruption_tests *self,
		corruption_test_fn_t testfn, int offset)
{
	jmp_buf env;

	int val = setjmp(env);
	if (val != 0) {
		// success!
		return;
	}

	testfn(self, offset, &env);

	XCTFail("Expected testfn to induce a call to malloc_zone_error");
}

static void
corrupt_at_offset(void *ptr, size_t size, int offset, jmp_buf *env)
{
	XCTAssertEqual(offset % 4, 0);

	if (offset < 0) {
		offset = (int)size + offset;
	}

	*(uint32_t *)((char *)ptr + offset) = 0xabcddcbau;

	// expect the next interaction to detect corruption
	zone_error_expected_jmp = env;
}

static void
test_corruption_in_cache(magazine_tiny_corruption_tests *self, int offset,
		jmp_buf *env)
{
	const size_t size = 64;

	void *ptr = [self tiny_malloc:size];
	memset(ptr, 'a', size);

	[self tiny_free:ptr];

	corrupt_at_offset(ptr, size, offset, env);
	(void)tiny_malloc_should_clear(&self->tiny_rack, TINY_MSIZE_FOR_BYTES(size),
			false);
}

static void
test_corruption_in_freelist(magazine_tiny_corruption_tests *self, int offset,
		jmp_buf *env)
{
	const size_t size = 272; // skip the tiny cache

	void *ptr = [self tiny_malloc:size];
	memset(ptr, 'a', size);

	[self tiny_free:ptr];

	corrupt_at_offset(ptr, size, offset, env);
	(void)tiny_malloc_should_clear(&self->tiny_rack, TINY_MSIZE_FOR_BYTES(size),
			false);
}

static void
test_realloc_cache_corruption(magazine_tiny_corruption_tests *self, int offset,
		jmp_buf *env)
{
	const size_t size = 64;

	void *ptr1 = [self tiny_malloc:size];
	memset(ptr1, 'a', size);

	void *ptr2 = [self tiny_malloc:size];
	memset(ptr2, 'a', size);

	XCTAssertEqual((uintptr_t)ptr1 + size, (uintptr_t)ptr2);

	[self tiny_free:ptr2]; // put in the cache

	corrupt_at_offset(ptr2, size, offset, env);

	(void)tiny_try_realloc_in_place(&self->tiny_rack, ptr1, size,
			size * 2);
}

static void
test_realloc_freelist_corruption(magazine_tiny_corruption_tests *self,
		int offset, jmp_buf *env)
{
	const size_t size = 272; // skip the cache
	const size_t small_size = 64; // go through the cache

	void *ptr1 = [self tiny_malloc:size];
	memset(ptr1, 'a', size);

	void *ptr2 = [self tiny_malloc:small_size];
	memset(ptr2, 'b', small_size);

	void *ptr3 = [self tiny_malloc:small_size];
	memset(ptr3, 'c', small_size);

	XCTAssertEqual((uintptr_t)ptr1 + size, (uintptr_t)ptr2);
	XCTAssertEqual((uintptr_t)ptr2 + small_size, (uintptr_t)ptr3);

	[self tiny_free:ptr2];
	// Push block 2 out of the cache
	[self tiny_free:ptr3];

	corrupt_at_offset(ptr2, small_size, offset, env);

	(void)tiny_try_realloc_in_place(&self->tiny_rack, ptr1, size,
			size + small_size);
}

static void
test_realloc_end_corruption(magazine_tiny_corruption_tests *self,
		int offset, jmp_buf *env)
{
	const size_t size = 64;
	const size_t end_size = 16;

	void *ptr1 = [self tiny_malloc:size];
	memset(ptr1, 'a', size);

	void *end = (char *)ptr1 + size;
	corrupt_at_offset(end, end_size, offset, env);

	(void)tiny_try_realloc_in_place(&self->tiny_rack, ptr1, size,
			size + end_size);
}

@implementation magazine_tiny_corruption_tests

- (void)setUp {
	zone_error_expected_jmp = NULL;
	malloc_zero_on_free_sample_period = 1; // zero-assert on every allocation
	[super setUp];
}

// Need to generate a separate test case method for each offset we want to test
// corruption at to reset tiny_rack to something sane

#define CORRUPTION_TEST_CASE(name, fn, off, offname) \
	- (void)name##Offset##offname { \
		run_corruption_test_at_offset(self, fn, off); \
	}

#define SHORT_CORRUPTION_TEST_CASES(name, fn) \
	CORRUPTION_TEST_CASE(name, fn, 0, 0) \
	CORRUPTION_TEST_CASE(name, fn, 4, 4) \
	CORRUPTION_TEST_CASE(name, fn, 8, 8) \
	CORRUPTION_TEST_CASE(name, fn, 12, 12)

#define CORRUPTION_TEST_CASES(name, fn) \
	SHORT_CORRUPTION_TEST_CASES(name, fn) \
	CORRUPTION_TEST_CASE(name, fn, 16, 16) \
	CORRUPTION_TEST_CASE(name, fn, 20, 20) \
	CORRUPTION_TEST_CASE(name, fn, 24, 24) \
	CORRUPTION_TEST_CASE(name, fn, -4, Negative4) \
	CORRUPTION_TEST_CASE(name, fn, -8, Negative8)

#define LONG_CORRUPTION_TEST_CASES(name, fn) \
	CORRUPTION_TEST_CASES(name, fn) \
	CORRUPTION_TEST_CASE(name, fn, 128, 128)

CORRUPTION_TEST_CASES(testCorruptionInCache, test_corruption_in_cache);
LONG_CORRUPTION_TEST_CASES(testCorruptionInFreeList,
		test_corruption_in_freelist);
CORRUPTION_TEST_CASES(testCorruptionOnReallocFromCache,
		test_realloc_cache_corruption);
CORRUPTION_TEST_CASES(testCorruptionOnReallocFromFreeList,
		test_realloc_freelist_corruption);
SHORT_CORRUPTION_TEST_CASES(testCorruptionOnReallocFromEnd,
		test_realloc_end_corruption);

@end
