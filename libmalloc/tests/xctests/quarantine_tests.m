#import "internal.h"

#if CONFIG_QUARANTINE

#import <dlfcn.h>
#import <XCTest/XCTest.h>

#define XCTAssertNotNull(ptr) XCTAssertNotEqual(ptr, NULL)

@interface quarantine_tests : XCTestCase {
@private
	malloc_zone_t *qzone;
}
@end

@implementation quarantine_tests

- (void)setUp {
	setenv("MallocQuarantineNoPoisoning", "1", 1);
	
	malloc_zone_t *szone = create_scalable_zone(0, 0);
	self->qzone = quarantine_create_zone(szone);
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
_quarantine_test_allocationFunction(malloc_zone_t *qzone, size_t size) {
	void *p = malloc_zone_malloc(qzone, size);
	asm volatile (""); // disable tail-calling
	return p;
}

// Exported so that the symbol name isn't stripped and can be looked up via dladdr.
MALLOC_EXPORT MALLOC_NOINLINE
void
_quarantine_test_deallocationFunction(malloc_zone_t *qzone, void *p) {
	malloc_zone_free(qzone, p);
	asm volatile (""); // disable tail-calling
	return;
}

- (void)testStacktraces {
	void *p = _quarantine_test_allocationFunction(qzone, 32);
	XCTAssertNotNull(p);
	_quarantine_test_deallocationFunction(qzone, p);
	
	quarantine_report_t report = {};
	kern_return_t ret = quarantine_diagnose_fault_from_crash_reporter(p, &report, mach_task_self(), qzone, memory_reader);
	XCTAssertEqual(ret, KERN_SUCCESS);
	
	XCTAssertEqual(report.fault_address, (uintptr_t)p);
	XCTAssertEqual(report.nearest_allocation, (uintptr_t)p);
	XCTAssertEqual(report.allocation_size, 32);
	
	bool allocSiteFrameFound = false;
	for (int i = 0; i < report.alloc_trace.num_frames; i++) {
		Dl_info info = {};
		dladdr(report.alloc_trace.frames[i], &info);
		NSLog(@"frame[%d] = %s", i, info.dli_sname);
		if ([@(info.dli_sname) isEqualTo:@"_quarantine_test_allocationFunction"]) {
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
		if ([@(info.dli_sname) isEqualTo:@"_quarantine_test_deallocationFunction"]) {
			deallocSiteFrameFound = true;
			break;
		}
	}
	XCTAssertTrue(deallocSiteFrameFound);
}

@end

#endif // CONFIG_QUARANTINE
