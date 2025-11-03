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
	malloc_zero_policy = MALLOC_ZERO_ON_FREE;

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
	malloc_zero_policy = MALLOC_ZERO_ON_FREE;
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
