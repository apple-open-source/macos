/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */
//
//  TestCase.h
//  Copyright (c) 2009 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>
#import "auto_zone.h"

@class TestCase;

/* TestFinalizer sends didFinalize:self to the currently running TestCase from its -finalize */
@interface TestFinalizer : NSObject
{
    TestCase *_allocatingTest;
}
@end

typedef enum {
    PENDING,
    SKIPPED,
    IN_PROGRESS,
    PASSED,
    FAILED
} TestResult;

@interface TestCase : NSObject {
    // test thread support
    pthread_t _testThread;
    pthread_mutex_t _mutex;
    pthread_cond_t _cond;
    SEL _selector;
    vm_address_t _disguisedStackBuffer;
    BOOL _outputCompleteCalled;
    NSMutableString *_testOutput;
    
    dispatch_queue_t _testQueue;
    dispatch_queue_t _completionCallbackQueue;
    void (^_completionCallback)(void);
    TestResult _result;
    NSString *_resultMessage;
    
    // for monitoring stderr during the test
    int _orig_fd;
    int _fds[2];
}

/* Subclass overrides */

// TestCase returns true for subclasses which implement a -performTest method.
// Abstract subclasses can override to be skipped from the test class scan.
+ (BOOL)isConcreteTestCase;

// override to implement the test
- (void)performTest;

// subclasses can determine at runtime that the test should not be attempted
// to skip the test the subclass should return a string explaining why the test could not be run
// to run the test subclasses should return [super shouldSkip]
- (NSString *)shouldSkip;

// tests can monitor test object finalization by overriding
- (void)didFinalize:(TestFinalizer *)finalizer;

// Tests which monitor console output can override to scan the output. Output is delivered one line at a time.
// The current implementation uses a small-ish buffer for output.
- (void)processOutputLine:(NSString *)line;

// Invoked after all output has been sent to processOutputLine:. Tests that expect to see output can override to fail as needed.
// Subclasses must call super.
- (void)outputComplete;


/* for use by test implementors */

// Access auto environment variable controlled settings
- (BOOL)resurrectionIsFatal;

// Returns a string like "PASSED" or "FAILED: message"
- (NSString *)resultString;

// subclasses invoke to indicate test result
- (void)setTestResult:(TestResult)result message:(NSString *)msg;

// invokes setTestResult:PASSED, but only if the result is not already failed
// thus it is safe to call -passed at the end of the test without overriding an earlier failure
- (void)passed;

// subclasses invoke to indicate a test failure
// invokes setTestResult:FAILED
// (for whitebox tests pass a constant object if calling from a collector callout where allocation is unsafe)
- (void)fail:(NSString *)msg;

// subclasses invoke on the test queue to signal the test is complete
- (void)testFinished;

// subclasses which need to scan output can invoke -flushStderr to flush any pending output.
// can only be invoked once, as the flush destroys the output stream
- (void)flushStderr;

// invokes auto_zone_collect with the requested options
- (void)collectWithOptions:(auto_zone_options_t)options;
- (void)collectWithOptions:(auto_zone_options_t)options completionCallback:(void (^)(void))callback;

- (void)runThreadLocalCollection;
- (void)requestAdvisoryCollectionWithCompletionCallback:(void (^)(void))callback;      // AUTO_ZONE_COLLECT_NO_OPTIONS
- (void)requestGenerationalCollectionWithCompletionCallback:(void (^)(void))callback;  // AUTO_ZONE_COLLECT_GENERATIONAL_COLLECTION
- (void)requestFullCollectionWithCompletionCallback:(void (^)(void))callback;          // AUTO_ZONE_COLLECT_FULL_COLLECTION
- (void)requestExhaustiveCollectionWithCompletionCallback:(void (^)(void))callback;    // AUTO_ZONE_COLLECT_EXHAUSTIVE_COLLECTION

- (void)clearStack;

- (BOOL)block:(void *)block isInList:(void **)blockList count:(uint32_t)count;

- (auto_zone_t *)auto_zone;

/* Utility methods to disguise/undisguise a pointer. A disguised pointer will be invisible to the collector. */
- (vm_address_t)disguise:(void *)pointer;
- (void *)undisguise:(vm_address_t)disguisedPointer;

- (void)startTestThread;
- (void)stopTestThread;
- (void)performSelectorOnTestThread:(SEL)cmd;
- (id *)testThreadStackBuffer;
- (uintptr_t)testThreadStackBufferSize;

/* For use by the test machienery */
// returns an array containing all the concrete test classes
+ (NSArray *)testClasses;

// returns the currently running test case
+ (id)currentTestCase;

- (void)setCompletionCallback:(void (^)(void))completionCallback;

- (void)startTest;
- (void)testFinished;

- (TestResult)result;

- (NSString *)testOutput;

// subclasses invoke to indicate test result and complete the test
- (void)setTestResult:(TestResult)result message:(NSString *)msg;

// invokes setTestResult:PASSED
- (void)passed;

// subclasses invoke to indicate a test failure
// invokes setTestResult:FAILED
// (for whitebox tests pass a constant object if calling from a collector callout where allocation is unsafe)
- (void)fail:(NSString *)msg;

- (BOOL)failed; // returns YES if the test has previously set a failure condition

// invokes auto_zone_collect with the requested options
- (void)collectWithOptions:(auto_zone_options_t)options;
- (void)collectWithOptions:(auto_zone_options_t)options completionCallback:(void (^)(void))callback;


- (void)runThreadLocalCollection;
- (void)requestAdvisoryCollectionWithCompletionCallback:(void (^)(void))callback;      // AUTO_ZONE_COLLECT_NO_OPTIONS
- (void)requestGenerationalCollectionWithCompletionCallback:(void (^)(void))callback;  // AUTO_ZONE_COLLECT_GENERATIONAL_COLLECTION
- (void)requestFullCollectionWithCompletionCallback:(void (^)(void))callback;          // AUTO_ZONE_COLLECT_FULL_COLLECTION
- (void)requestExhaustiveCollectionWithCompletionCallback:(void (^)(void))callback;    // AUTO_ZONE_COLLECT_EXHAUSTIVE_COLLECTION

- (void)requestCompactionWithCompletionCallback:(void (^)(void))callback;
- (void)requestCompactionAnalysisWithCompletionCallback:(void (^)(void))callback;

@end
