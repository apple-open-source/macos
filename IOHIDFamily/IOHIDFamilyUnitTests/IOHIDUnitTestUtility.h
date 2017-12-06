//
//  IOHIDUnitTestUtility.h
//  IOHIDFamily
//
//  Created by YG on 10/24/16.
//
//

#ifndef IOHIDUnitTestUtility_h
#define IOHIDUnitTestUtility_h
#include <TargetConditionals.h>
#include <dispatch/dispatch.h>

#include <mach/mach_time.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/AppleHIDUsageTables.h>


#define _EVAL(x) #x
#define EVAL(x) _EVAL (x)

#define OS_TYPE_MAC(x)       D ## x
#define OS_TYPE_EMBEDDED(x)  D ## x

#if TARGET_OS_EMBEDDED
#undef OS_TYPE_EMBEDDED
#define OS_TYPE_EMBEDDED(x) x
#else
#undef OS_TYPE_MAC
#define OS_TYPE_MAC(x) x
#endif

//#define CONDTIONAL_TEST_CASE(x) test ## x
#define CONDTIONAL_TEST_CASE(x) x
#define MAC_OS_ONLY_TEST_CASE(x) OS_TYPE_MAC(x)
#define EMBEDDED_OS_ONLY_TEST_CASE(x) OS_TYPE_EMBEDDED(x)

#define _CAT(a, ...) a ## __VA_ARGS__

#define TEST_CASE_1(x, ...) __VA_ARGS__
#define TEST_CASE_0(x, ...) D ## x

#define TEST_CASE(c,f) _CAT(TEST_CASE_,c)


#define TestLog(fmt,...) NSLog(@#fmt, ##__VA_ARGS__)

// kDefaultMaxEventLatencyThresholdtime in ns
#define kDefaultMaxEventLatencyThreshold      200000000

//kDefaultReportDispatchCompletionTime time in usec
#define kDefaultReportDispatchCompletionTime  5000000

//kServiceMatchingTimeout time in sec
#define  kServiceMatchingTimeout  5

//kDeviceMatchingTimeout time in sec
#define  kDeviceMatchingTimeout   5

extern dispatch_queue_t IOHIDUnitTestCreateRootQueue (int priority, int poolSize);
extern uint64_t  IOHIDInitTestAbsoluteTimeToNanosecond (uint64_t abs);
CFRunLoopRef IOHIDUnitTestCreateRunLoop (int priority);
void IOHIDUnitTestDestroyRunLoop (CFRunLoopRef runloop);

#define MS_TO_US(x) (x*1000ull)
#define MS_TO_NS(x) (MS_TO_US(x)*1000ull)
#define NS_TO_MS(x) ((x)/1000ull)

#define VALUE_IN_RANGE(x,l,r) ((l)<=(x) && (x) <= (r))
#define VALUE_PST(x,p) ((p < 0) ? (x) - ((abs(p))*(x))/100 : (x) + ((abs(p))*(x))/100)

#define HIDTestEventLatency(s) XCTAssertTrue(s.maxLatency < kDefaultMaxEventLatencyThreshold, "Max Event Latency %llu, Avarage latency %llu", s.maxLatency, s.averageLatency);


#define HIDXCTAssertAndThrowTrue(expression, ...)                                                       \
{                                                                                                       \
    _XCTPrimitiveAssertTrue(self, expression, @#expression, __VA_ARGS__);                               \
    BOOL expressionValue = !!(expression);                                                              \
    if (!expressionValue) {                                                                             \
        [NSException raise:@"HIDXCTAssertAndThrowTrue" format:@"%s:%d", __PRETTY_FUNCTION__, __LINE__]; \
    }                                                                                                   \
}

#undef check_compile_time
#if( !defined( check_compile_time ) )
#if( defined( __cplusplus ) )
    #define check_compile_time( X ) extern "C" int compile_time_assert_failed[ (X) ? 1 : -1 ]
#else
    #define check_compile_time( X ) extern int compile_time_assert_failed[ (X) ? 1 : -1 ]
#endif
#endif

#endif /* IOHIDUnitTestUtility_h */
