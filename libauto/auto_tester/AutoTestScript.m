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
//  AutoTestScript.m
//  auto
//
//  Created by Josh Behnke on 5/19/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import <objc/objc-auto.h>
#import <objc/objc-class.h>
#import <auto_zone.h>
#import "AutoTestScript.h"

enum {
    INCOMPLETE,     // the test is not complete
    COMPLETE        // the test is complete
};


// holds all test subclasses
static NSArray *_testClasses = nil;

// the currently running script
static AutoTestScript *_script;

@implementation AutoTestScript

+ (void)initialize
{
    if (self == [AutoTestScript class]) {
        // need to warm up AutoTestSynchronizer too
        [AutoTestSynchronizer class];
        
        // Start the collector thread, but disabled. This way we know collections will only run when the test wants.
        auto_collector_disable(auto_zone());
        objc_startCollectorThread();
    }
}

+ (BOOL)isBeingDebugged
{
    return NO;
}

+ (NSArray *)testClasses:(BOOL)debugOnly
{
    // Dynamically find all the subclasses of AutoTestScript
    if (_testClasses == nil) {
        NSMutableArray *testClasses = [[NSMutableArray alloc] init];
        int numClasses;
        Class * classes = NULL;
        Class ats = [AutoTestScript class];
        
        classes = NULL;
        numClasses = objc_getClassList(NULL, 0);
        
        if (numClasses > 0 )
        {
            classes = malloc(sizeof(Class) * numClasses);
            numClasses = objc_getClassList(classes, numClasses);
            int i;
            for (i=0; i<numClasses; i++) {
                // Some fumkiness to weed out weird clases like NSZombie and Object without actually messaging them.
                Class sc = class_getSuperclass(classes[i]);
                if (sc) {
                    Class ssc = class_getSuperclass(sc);
                    if (ssc) {
                        if ([classes[i] isSubclassOfClass:ats]) {
                            // Might need to weed out abstract subclasses?
                            if (!debugOnly || [classes[i] isBeingDebugged])
                                [testClasses addObject:classes[i]];
                        }
                    }
                }
            }
            free(classes);
        }
        _testClasses = testClasses;
    }
    return _testClasses;
}


+ (AutoTestScript *)runningScript
{
    return _script;
}


- (id)init
{
    self = [super init];
    if (self) {
        _completeCondition = [[NSConditionLock alloc] initWithCondition:INCOMPLETE];
    }
    return self;
}


- (void)runTest
{
    _script = self;
    [[self testThreadWithStartingSelector:@selector(startTest)] start];
    [self waitForCompletion];
    _script = nil;
}


- (void)waitForCompletion
{
    if ([_completeCondition lockWhenCondition:COMPLETE beforeDate:[NSDate dateWithTimeIntervalSinceNow:[self timeoutInterval]]]) {
        [_completeCondition unlock];
    } else {
        [self fail:"test timed out"];
    }
}


/* 
 Create a NSThread object and set it up to be a test thread. 
 Because we cannot do allocations while the test is running we pre-allocate a bunch of stuff and stick it in the thread's dictionary. 
 */
- (NSThread *)testThreadWithStartingSelector:(SEL)selector
{
    NSThread *thread = [[NSThread alloc] initWithTarget:self selector:@selector(testThread:) object:[NSValue valueWithBytes:&selector objCType:@encode(SEL)]];
    NSMutableDictionary *threadDict = [thread threadDictionary];
    return thread;
}


- (void)testSelectorLoop
{
    AutoTestSynchronizer *synchronizer = [AutoTestSynchronizer mySynchronizer];
    if (!synchronizer) {
        [self fail:"testSelectorLoop invoked with no assigned synchronizer"];
        return;
    }
    
    SEL testSelector = [synchronizer startingSelector];
    if (!testSelector) {
        [self fail:"testSelectorLoop invoked with no selector set"];
        return;
    }
    
    // The main test loop.
    const char *sel_name = sel_getName(testSelector); // a debugging convenience
    do {
        @synchronized(self) {
            if ([self respondsToSelector:testSelector]) {
                //printf("invoking: %s\n", sel_getName(testSelector));
                objc_msgSend(self, testSelector);
            } else {
                [self fail:"test does not respond to selector"];
                break;
            }
        }
        testSelector = [synchronizer nextSelector];
        sel_name = sel_getName(testSelector);
        if (testSelector)
            [synchronizer waitUntilSignaled];
    } while (testSelector);
}


- (AutoTestSynchronizer *)setNextTestSelector:(SEL)nextSelector
{
    AutoTestSynchronizer *synchronizer = [AutoTestSynchronizer checkoutSynchronizer];
    [synchronizer setNextSelector:nextSelector];
    return synchronizer;
}


- (void)probeDidComplete
{
    AutoTestSynchronizer *synchronizer = [AutoTestSynchronizer mySynchronizer];
    if (synchronizer && ![synchronizer isTesterThread]) {
        SEL s = [synchronizer nextSelector];
        [synchronizer waitUntilSignaled];
        [synchronizer setStartingSelector:s];
        [self testSelectorLoop];
        [synchronizer checkin];
    }
}


/*
 This method is the initial method invoked on each test thread. 
 It implements the loop that walks through the test selectors.
 It is the stack frame where the stack pointer array lives.
 */
- (void)testThread:(NSValue *)selectorValue
{
    @synchronized (self) {
        _threadCount++;
    }
    
    AutoTestSynchronizer *synchronizer = [AutoTestSynchronizer checkoutSynchronizer];
    id stackPointers[[self stackPointerCount]];
    [synchronizer setStackPointers:&stackPointers[0]];
        
    // Look up other objects from the thread dict
    SEL firstTestSelector;
    [selectorValue getValue:&firstTestSelector];
    [synchronizer setStartingSelector:firstTestSelector];
    [self testSelectorLoop];
    
    // We are done. If this was the last thread then notify that the test is complete.
    @synchronized(self) {
        _threadCount--;
        if (_threadCount == 0) {
            [_completeCondition lock];
            [_completeCondition unlockWithCondition:COMPLETE];
        }
    }
    [synchronizer checkin];
}


- (void)startTest
{
    // subclasses should override, or avoid calling by overriding -testSelectors
    [self fail:"startTest called on AutoTestScript. Test start not implemented."];
}


- (NSTimeInterval)timeoutInterval
{
    return 600.0;
}


- (NSInteger)stackPointerCount
{
    return 16;
}

- (void)requestCollection:(auto_collection_mode_t)mode
{
    // Allow the collector to run and request a collection. Collection will be disabled again as soon as the collector starts.
    @synchronized (self) {
        auto_zone_t *zone = auto_zone();
        auto_collector_reenable(zone);
        auto_collect(zone, mode, NULL);
        auto_collector_disable(zone);
    }
}


- (void)requestGenerationalCollection
{
    [self requestCollection:AUTO_COLLECT_GENERATIONAL_COLLECTION];
}


- (void)requestFullCollection
{
    [self requestCollection:AUTO_COLLECT_FULL_COLLECTION];
}


- (BOOL)testRunning
{
    return _threadCount != 0;
}


- (const char *)failureMessage
{
    return _failureMessage;
}


- (STACK_POINTER_TYPE *)stackPointers
{
    AutoTestSynchronizer *synchronizer = [AutoTestSynchronizer mySynchronizer];
    id *bufferPtr = [synchronizer stackPointers];
    return (STACK_POINTER_TYPE *)bufferPtr;
}


- (void)fail:(const char *)failureMessage
{
    @synchronized(self) {
        if (!_failureMessage) {
            _failureMessage = failureMessage;
        }
        //printf("FAIL: %s\n", failureMessage);
    }
}


- (vm_address_t)disguise:(void *)pointer;
{
    return (vm_address_t)pointer + 1;
}


- (void *)undisguise:(vm_address_t)disguisedPointer;
{
    return (void *)(disguisedPointer - 1);
}

- (auto_zone_t *)auto_zone
{
    return auto_zone();
}

- (BOOL)block:(void *)block isInList:(void **)blockList count:(uint32_t)count
{
    BOOL found = NO;
    uint32_t i;
    for (i = 0; i < count && !found; i++)
        found = (blockList[i] == block);
    return found;
}


- (void)nop
{
}


/*
 Methods invoked via probe points in the collector, in alphabetical order.
 */
- (void)autoCollect:(auto_collection_mode_t)mode
{
}

- (void)beginHeapScan:(BOOL)generational
{
}

- (void)beginLocalScan
{
}

- (void)blockBecameGlobal:(void *)block withAge:(uint32_t)age
{
}

- (void)blockMatured:(void *)block newAge:(uint32_t)age
{
}

- (void)collectionComplete
{
}

- (void)endHeapScanWithGarbage:(void **)garbage_list count:(size_t)count
{
}

- (void)endLocalScanWithGarbage:(void **)garbage_list count:(size_t)count
{
}

- (void)scanBarrier
{
}

- (void)endThreadScan
{
}

- (void)heapCollectionComplete
{
}

- (void)localCollectionComplete
{
}

- (void)scanBlock:(void *)block endAddress:(void *)end
{
}

- (void)scanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map
{
}

- (void)didScanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map
{
}

- (void)scanBlock:(void *)block endAddress:(void *)end withWeakLayout:(const unsigned char *)map
{
}

- (void)setPending:(void *)block
{
}

- (void)threadRegistrationError
{
}

@end
