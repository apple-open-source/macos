/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
//  AutoTestScript.h
//  auto
//
//  Created by Josh Behnke on 5/19/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import <pthread.h>
#import <Foundation/Foundation.h>
#import <auto_zone.h>
#import "AutoTestSynchronizer.h"

/*
 AutoTestScript is an abstract base class that provides infrastructure for running test cases. A concrete test subclass, or test "script" can be thought of as a controller for running a particular test scenario. The infrastructure provides support for synchronizing with and observing data from events in the collector.
 
 A test script contains several test methods and probe callouts (defined below), and the test infrastructure provides support for executing them in a deterministic, reproducible manner. 
 
 A test method is a selector that returns void and takes no arguments. A test method implements an atomic portion of a test scenario, and typically represents "user code" . All test methods are invoked inside an @synchronized block using the test object's lock so no two ever execute concurrently. A test method can invoke setNextTestSelector: to indicate that another test method should be executed after the current one. setNextTestSelector: returns a synchronizer object that should be stored. When the executing test method returns the calling thread will block until that synchronizer object is signaled, whereupon the selector that was passed to setNextTestSelector: will be invoked (once the object's lock can be aquired). (Note that the selector passed to setNextTestSelector: can be determined at runtime. Conditional branching and looping of test method is supported.) If a test method exits without calling setNextTestSelector: then that thread is considered to be complete. When all test threads are complete the test is considered to be finished.
 
 A probe callout is a call that originates from a probe point in the collector. (See AUTO_PROBE.) The test infrastructure maps the call from the collector into a method invocation on the test object. See below for the list of probe point callouts. A probe callout method may have arguments to provide information about the collector's internal state. Probe callouts are executed inside and @synchronized block using the test object's lock. The probe callouts allow the test object to track the collector's status and observe its internal state. A test could determine when globals have been scanned or if a particular block was found as garbage for example. One typical action for a probe implementation is to signal a test thread. A probe callout can also invoke setNextTestSelector:. If it does then the probe callout does not return to the collector immediately. Instead it enters a loop similar to the test method loop, blocking until the synchronizer is signaled. Only when a method completes without invoking setNextTestSelector: will the probe callout return to the collector. This permits very fine grained synchronization with test threads, though it does require many small methods.
 
 Consider an example test scenario as follows:
 Thread A does X.
 The collector starts running and gets to point C1.
 Thread B does Y.
 The collector continues and gets to point C2.
 Thread A does Z.
 The collector finishes a colletion.
 Test if test block is/is not collected.
 
 There are a few ways the test could be implemented. One example would be the following methods:
 -startTest - create a new thread to act as thread B using testThreadWithStartingSelector:startB. Then invoke method X.
 -X - perform X and invoke setNextTestSelector:Z. Record the returned synchronizer as thread A's synchronizer. Request a collection.
 -startB - invoke setNextTestSelector:Y. Record the returned synchronizer as thread B's synchronizer.
 -C1 probe callout - signal thread B's synchronizer, invoke setNextTestSelector:nop and record the returned synchronizer as collector's synchronizer
 -Y - perform Y and signal collector's synchronizer. Thread B then exits (because setNextTestSelector was not invoked.)
 -C2 probe callout - signal thread A's synchronizer, invoke setNextTestSelector:nop and record the returned synchronizer as collector's synchronizer
 -Z perform Z, set the next selector to -check, signal the collector's synchronizer
 -endHeapScanWithGarbage:count: - determine if the test block is in the garbage list and set a flag accordingly
 -heapCollectionComplete - signal thread A's synchronizer
 -check - check the flag to determine if the test block was reclaimed appropriately, then exit to end the test
 
 Misc Notes
 
 A test thread's synchronizer will not change for as long as the thread continues executing a chain of test methods. But the synchronizer returned to a thread performing a collector probe callout might change from one callout to the next. (It will remain the same until the thread returns to the collector though.)
 
 It is very easy to write a test that simply deadlocks. A probe callout may happen when the collector is holding locks. Probe callouts might happen during allocation. Probe callouts might even happen during a write barrier in a test method. A test implementor must be very careful to understand how the test interacts with the collector to write tests that will be robust and legitimate in the face of collector changes.
 
 Some probe callouts can happen on the same thread that is executing a test method. For example an assignment that results in a write barrier might trigger a callout to blockBecameGlobal:withAge:.
 
 It is possible to write a test case to simulate a race condition by implementing the synchronization required to deterministically produce that "race". However, because the test infrastructure itself performs explicit synchronization it is not possible to simulate/test a problem due to a missing (hardware) memory barrier.
 
 When a test is started a dedicated thread is created which invokes -startTest. At this point the collector is blocked from running (disabled). Collections can be started when desired using -requestGenerationalCollection and -requestFullCollection. Additional test threads can be created using -testThreadWithStartingSelector: (and subsequently started by invoking -start on the returned NSThread instance). The test should be prepared for the collector to find arbitrary garbage blocks in addition to any that it generates.
 
 A test should leave the world in the same state where it was started. This means all test threads should exit and the collector should not be running. This may require the test to implement an additional test step that simply waits for a collection to complete (-heapCollectionComplete).
 
 AutoTestScript provides methods to mathematically disguise/undisguise a pointer to make it invisible to the collector.
 
 A probe callout can sometimes be useful to debug a test failure. For example if a test block is being scanned unexpectedly then it might be useful to implement -setPending: to test if the block being pended is the test block, and put a breakpoint on the case where it matches.
 
 The test infrastructure maintains very tight control over when collections are run, using the collection enable/disable api. Any collections initiated by calls other than these AutoTestScript api (such as framework calls) will be skipped. This is implemented by disabling the collector early on, and generally keeping it disabled. When a test thread wants a collection to happen it can request one using -requestCollection: or related methods. These methods unblock the collector, initiate the collection request, then block the collector again, all within a single method that is executed within an @synchronized block in the test script. Because there is also a probe point at the beginning of auto_collect() which requires the same lock on the test script no other thread can initiate a collection request while collection is enabled.
 */

// The number of stack pointers available to a test thread
#define SCRIPT_POINTER_COUNT 16

#define STACK_POINTERS_USE_WRITE_BARRIER 1

#if STACK_POINTERS_USE_WRITE_BARRIER
#define STACK_POINTER_TYPE id
#else
#define STACK_POINTER_TYPE void *
#endif

@interface AutoTestScript : NSObject {
    const char *_failureMessage;                  // the first failure message encountered, or NULL if no failures
    uint32_t _threadCount;                  // count of running test threads
    NSConditionLock *_completeCondition;    // Used to synchronize with test completion.
}


/*
 Methods invoked via probe points in the collector, in alphabetical order.
 Concrete subclasses override these to observe whichever probe points are needed.
 Subclasses should invoke the AutoTestScript implementations, as some invariants may be checked for all tests.
 */

/* Invoked from the auto_collect probe at the beginning of auto_collect(). */
- (void)autoCollect:(auto_collection_mode_t)mode;

/* Invoked from the begin_heap_scan probe near the beginning of auto_collect_internal(). */
- (void)beginHeapScan:(BOOL)generational;

/* Invoked from the begin_local_scan probe near the beginning of ThreadLocalCollector::collect(). */
- (void)beginLocalScan;

/* Invoked from the make_global probe in Subzone::make_global(). */
- (void)blockBecameGlobal:(void *)block withAge:(uint32_t)age;

/* Invoked from the mature probe in Subzone::mature(). */
- (void)blockMatured:(void *)block newAge:(uint32_t)age;

/* Invoked at the end of collection, immediately after threads blocked waiting on synchronous collection are woken. */
- (void)collectionComplete;

/* Invoked from the end_heap_scan probe in auto_collect_internal(). Occurs after scanning is complete and the garbage list has been determined. */
- (void)endHeapScanWithGarbage:(void **)garbage_list count:(size_t)count;

/* Invoked from the end_local_scan probe in ThreadLocalCollector::process_local_garbage(). Occurs after scanning is complete and the garbage list has been determined. */
- (void)endLocalScanWithGarbage:(void **)garbage_list count:(size_t)count;

/* Invoked from the scan_barrier probe in MemoryScanner::scan(), right before the scan_barrier() is performed. */
- (void)scanBarrier;

/* Invoked from the end_thread_scan probe in MemoryScanner::scan(). Occurs after locked down thread scanning is complete. */
- (void)endThreadScan;

/* Invoked from the heap_collection_complete probe near the end of auto_collect_internal(). */
/* This is invoked _after_ the collector is reenabled so it can be used to trigger a new collection. */
- (void)heapCollectionComplete;

/* Invoked from the local_collection_complete probe at the end of ThreadLocalCollector::collect(). */
- (void)localCollectionComplete;

/* Invoked from the scan_range probe at the beginning of MemoryScanner::scan_range(). */
- (void)scanBlock:(void *)block endAddress:(void *)end;

/* Invoked from the scan_with_layout probe at the beginning of MemoryScanner::scan_with_layout(). */
- (void)scanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map;

/* Invoked from the did_scan_with_layout probe at the end of MemoryScanner::scan_with_layout(). */
- (void)didScanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map;

/* Invoked from the scan_with_weak_layout probe at the beginning of MemoryScanner::scan_with_weak_layout(). */
- (void)scanBlock:(void *)block endAddress:(void *)end withWeakLayout:(const unsigned char *)map;

/* Invoked from the set_pending probe at the beginning of Zone::set_pending(). */
- (void)setPending:(void *)block;

/* Invoked from auto_zone_thread_registration_error */
- (void)threadRegistrationError;


/* Subclasses override. */

/* The first selector of the test. */
- (void)startTest;

/*
 Subclasses override to return a time interval within which the test is expected to complete. If the test does not complete within this interval then it is considered to have failed.
 AutoTestScript returns 600 seconds (10 minutes).
 */
- (NSTimeInterval)timeoutInterval;

/* 
 Subclasses can override to return the number of stack pointers to allocate in each test thread.
 AutoTestScript implements this to return 16.
 */
- (NSInteger)stackPointerCount;

/* Flow control */

/* This method begins execution of the test. It creates a test thread to execute -startTest, and starts that thread. */
- (void)runTest;

/* Blocks the caller until all test threads have completed. */
- (void)waitForCompletion;

/* Create a new test thread object with selector as the first test selector. */
/* The thread object is initialized but no thread is started. */
/* A thread can be started at a later time by invoking -start on the returned NSThread. */
- (NSThread *)testThreadWithStartingSelector:(SEL)selector;

/* 
 Sets nextSelector as the next test selector to be invoked by the calling thread. Upon exiting the current test selector, the calling thread will block until the synchronizer returned by setNextTestSelector: is signaled, whereupon the thread will wake up and invoke nextSelector.
 */
- (AutoTestSynchronizer *)setNextTestSelector:(SEL)nextSelector;


/*
 These invoke auto api to initiate a collection. 
 */
- (void)requestCollection:(auto_collection_mode_t)mode;
- (void)requestGenerationalCollection;
- (void)requestFullCollection;



/* Miscellaneous */

/* Returns the last script that was started */
+ (AutoTestScript *)runningScript;

/* Returns all the implemented test classes */
+ (NSArray *)testClasses:(BOOL)debugOnly;

/* AutoTestScript implements this to return NO. Subclasses under development can override if needed. */
+ (BOOL)isBeingDebugged;

/* Returns true if the test has been started and one or more test threads have not yet exited. */
- (BOOL)testRunning;


/* Retrieve the first failure message encountered while running the test. Returns NULL if there were no failures. */
- (const char *)failureMessage;


// return an array of pointers on the current thread's stack that will persist for the duration of the test
// the array contains SCRIPT_POINTER_COUNT elements
- (STACK_POINTER_TYPE *)stackPointers;


/* Record a failure. failureMessage is an arbitrary description of the failure mode. */
/* Note that no allocations can be performed in the context where this method is called, so failureMessage should be a static or preallocated string. */
- (void)fail:(const char *)failureMessage;


/* Utility methods to disguise/undisguise a pointer. A disguised pointer will be invisible to the collector. */
- (vm_address_t)disguise:(void *)pointer;
- (void *)undisguise:(vm_address_t)disguisedPointer;


/* Returns the auto zone */
- (auto_zone_t *)auto_zone;


/* Convenience method to scan a list of pointers and test whether block is contained therein. */
- (BOOL)block:(void *)block isInList:(void **)blockList count:(uint32_t)count;

/* A test selector that does nothing. Implemented as a convenience to terminate a test method sequence after blocking a thread. */
- (void)nop;


- (void)probeDidComplete; // internal method invoked after a probe callout. This runs a method loop if the callout invoked setNextTestSelector:.

@end
