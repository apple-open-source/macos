#import "internal.h"

#import <XCTest/XCTest.h>

#define XCTAssertNotNull(ptr) XCTAssertNotEqual(ptr, NULL)

#if CONFIG_LARGE_CACHE

@interface magazine_large_tests : XCTestCase {
@private
	malloc_zone_t *mzone;
}
@end

@implementation magazine_large_tests

- (void)setUp {
	large_cache_enabled = true;
	mzone = malloc_create_zone(0, 0);
}

- (void)tearDown {
	if (mzone) {
		malloc_destroy_zone(mzone);
	}
}

- (size_t)minLargeAllocationSize {
	size_t threshold;
#if CONFIG_MEDIUM_ALLOCATOR
	threshold = MEDIUM_LIMIT_THRESHOLD + 1;
#else
	threshold = SMALL_LIMIT_THRESHOLD + 1;
#endif // CONFIG_MEDIUM_ALLOCATOR
	return threshold;
}

- (void *)large_malloc:(size_t)size {
	XCTAssertGreaterThanOrEqual(size, [self minLargeAllocationSize]);
	return malloc_zone_malloc(mzone, size);
}

- (void)large_free:(void *)ptr {
	malloc_zone_free(mzone, ptr);
}

- (void)testLargeMallocSucceeds {
	XCTAssertNotNull([self large_malloc: [self minLargeAllocationSize]]);
}

- (void)testZoneDestruction {
	// Allocate and free an entry onto death row
	void *ptr = [self large_malloc: [self minLargeAllocationSize]];
	XCTAssertNotNull(ptr);
	[self large_free: ptr];

	malloc_destroy_zone(mzone);
	mzone = NULL;
}

@end

#endif // CONFIG_LARGE_CACHE
