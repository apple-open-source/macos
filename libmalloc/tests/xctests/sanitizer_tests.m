#import "internal.h"

#if CONFIG_SANITIZER

#import <dlfcn.h>
#import <XCTest/XCTest.h>

#define XCTAssertNotNull(ptr) XCTAssertNotEqual(ptr, NULL)

@interface sanitizer_tests : XCTestCase {
@private
	malloc_zone_t *szone;
}
@end

@implementation sanitizer_tests

static bool did_allocate = false, did_deallocate = false, did_internal = false;
static void heap_allocate(void *ptr, size_t leftrz, size_t alloc, size_t rightrz) {
	did_allocate = true;
}
static void heap_deallocate(void *ptr, size_t sz) {
	did_deallocate = true;
}
static void heap_internal(void *ptr, size_t sz) {
	did_internal = true;
}

#define REDZONE_SIZE 16
#define STR(x) XSTR(x)
#define XSTR(x) #x

- (void)setUp {
	static struct malloc_sanitizer_poison sanitizer_funcs = {
		.heap_allocate_poison = heap_allocate,
		.heap_deallocate_poison = heap_deallocate,
		.heap_internal_poison = heap_internal,
	};

	setenv("MallocSanitizerRedzoneSize", STR(REDZONE_SIZE), 1);
	malloc_sanitizer_set_functions(&sanitizer_funcs);
	malloc_zone_t *szone = create_scalable_zone(0, 0);
	self->szone = sanitizer_create_zone(szone);
}

static void *
memory_reader(task_t task, vm_address_t address, size_t size)
{
	void *p = malloc(size);
	memcpy(p, address, size);
	return p;
}

// Exported so that the symbol name isn't stripped and can be looked up via dladdr.
MALLOC_EXPORT MALLOC_NOINLINE
void *
_sanitizer_test_allocationFunction(malloc_zone_t *szone, size_t size) __attribute__((disable_tail_calls)) {
	void *p = malloc_zone_malloc(szone, size);
	return p;
}

// Exported so that the symbol name isn't stripped and can be looked up via dladdr.
MALLOC_EXPORT MALLOC_NOINLINE
void
_sanitizer_test_deallocationFunction(malloc_zone_t *szone, void *p) __attribute__((disable_tail_calls)) {
	malloc_zone_free(szone, p);
	return;
}

- (void)testStacktraces {
	void *p = _sanitizer_test_allocationFunction(szone, 32);
	XCTAssertNotNull(p);
	_sanitizer_test_deallocationFunction(szone, p);

	sanitizer_report_t report = {};
	kern_return_t ret = sanitizer_diagnose_fault_from_crash_reporter(p, &report, mach_task_self(), szone, memory_reader);
	XCTAssertEqual(ret, KERN_SUCCESS);

	XCTAssertEqual(report.fault_address, (uintptr_t)p);
	XCTAssertEqual(report.nearest_allocation, (uintptr_t)p);
	XCTAssertGreaterThanOrEqual(report.allocation_size, 32);

	bool allocSiteFrameFound = false;
	for (int i = 0; i < report.alloc_trace.num_frames; i++) {
		Dl_info info = {};
		dladdr(report.alloc_trace.frames[i], &info);
		NSLog(@"frame[%d] = %s", i, info.dli_sname);
		if ([@(info.dli_sname) isEqualTo:@"_sanitizer_test_allocationFunction"]) {
			allocSiteFrameFound = true;
			break;
		}
	}
	XCTAssertTrue(allocSiteFrameFound);

	bool deallocSiteFrameFound = false;
	for (int i = 0; i < report.dealloc_trace.num_frames; i++) {
		Dl_info info = {};
		dladdr(report.dealloc_trace.frames[i], &info);
		NSLog(@"frame[%d] = %s", i, info.dli_sname);
		if ([@(info.dli_sname) isEqualTo:@"_sanitizer_test_deallocationFunction"]) {
			deallocSiteFrameFound = true;
			break;
		}
	}
	XCTAssertTrue(deallocSiteFrameFound);
}

- (void)testPoisonAllocation {
	XCTAssertFalse(did_allocate);
	XCTAssertFalse(did_deallocate);

	const size_t alloc_sz = 1;
	void *p = _sanitizer_test_allocationFunction(szone, alloc_sz);
	XCTAssertNotNull(p);

	XCTAssertTrue(did_allocate);

	size_t sz = szone->size(szone, p);
	XCTAssertEqual(sz, alloc_sz);

	_sanitizer_test_deallocationFunction(szone, p);
	XCTAssertTrue(did_deallocate);
}

- (void)testBadPoisonAllocation {
	void *p = _sanitizer_test_allocationFunction(szone, SIZE_MAX - (REDZONE_SIZE - 1));
	XCTAssertEqual(p, NULL);
}

- (void)testInternalPoison {
	char dummy[16];

	XCTAssertFalse(did_internal);
	const struct malloc_sanitizer_poison *funcs = malloc_sanitizer_get_functions();
	XCTAssertNotNull(funcs);
	XCTAssertNotNull(funcs->heap_internal_poison);
	funcs->heap_internal_poison(dummy, sizeof(dummy));
	XCTAssertTrue(did_internal);
}

@end

#endif // CONFIG_SANITIZER
