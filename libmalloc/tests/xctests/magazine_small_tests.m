#import "internal.h"

#import <XCTest/XCTest.h>

#define XCTAssertNotNull(ptr) XCTAssertNotEqual(ptr, NULL)

@interface magazine_small_tests : XCTestCase {
@private
	struct rack_s small_rack;
}
@end

@implementation magazine_small_tests

- (void)setUp {
	memset(&small_rack, 'a', sizeof(small_rack));
	rack_init(&small_rack, RACK_TYPE_SMALL, 1, 0);
}

- (void *)small_malloc:(size_t)size {
	return small_malloc_should_clear(&small_rack, SMALL_MSIZE_FOR_BYTES(size), false);
}

- (void)testSmallMallocSucceeds {
	XCTAssertNotNull([self small_malloc:512]);
}

- (void)testSmallRegionFoundAfterMalloc {
	void *ptr = [self small_malloc:512];
	XCTAssertNotNull(ptr);

	XCTAssertNotNull(small_region_for_ptr_no_lock(&small_rack, ptr));
}

- (void)testSmallSizeMatchesMalloc {
	void *ptr = [self small_malloc:512];
	XCTAssertNotNull(ptr);

	XCTAssertEqual(small_size(&small_rack, ptr), 512);
}

@end
