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
//  resurrect.m
//  Copyright (c) 2009-2011 Apple Inc. All rights reserved.
//

#import "BlackBoxTest.h"
#import <objc/objc-auto.h>
#import <objc/runtime.h>

@interface GlobalResurrectionContainer : NSObject
{
    @public
    id object;
}
@end

@implementation GlobalResurrectionContainer
@end

static id global;
static __weak id global_weak;
static GlobalResurrectionContainer *ivarTester;

/*
 There are several different resurrection tests, which are all TestCase subclasses.
 Each also has a test class with a -finalize that implements the resurrection.
 In general, each test instanciates a test class object, runs a collection, an verifies
 that the correct resurrection behavior happens.
*/

/* These are the helper test classes. */

@interface ResurrectMemmoveOkTester : TestFinalizer {
@public
    id srcObject;
    id dstObject;
}
@end

@interface ResurrectAssociatedTester : TestFinalizer
@end

@interface ResurrectCASTester : TestFinalizer
@end

@interface ResurrectGlobalTester : TestFinalizer
@end

@interface ResurrectGlobalIvarTester : TestFinalizer
@end

@interface ResurrectGlobalMemmoveTester : TestFinalizer
@end

@interface ResurrectGlobalWeakTester : TestFinalizer
@end

@interface ResurrectThreadedTester : TestFinalizer
@end

@interface ResurrectRetainTester : TestFinalizer
@end

/* 
 These are the TestCase classes. We have a base class that is common to all the resurrection tests. 
 The base class verifies that the test object was actually finalized.
 */

@interface Resurrection : BlackBoxTest
{
    uint _finalizedCount;
    vm_address_t _localTester;
    vm_address_t _globalTester;
    NSMutableArray *_expectedMessages;
}
@end

@interface ResurrectMemmoveOk : Resurrection
@end

@interface ResurrectAssociated : Resurrection
@end

@interface ResurrectCAS : Resurrection
@end

@interface ResurrectGlobal : Resurrection
@end

@interface ResurrectGlobalIvar : Resurrection
@end

@interface ResurrectGlobalMemmove : Resurrection
@end

@interface ResurrectGlobalWeak : Resurrection
@end

@interface ResurrectThreaded : Resurrection
@end

@interface ResurrectRetain : Resurrection
@end

/* helper implementations */
 
@implementation ResurrectMemmoveOkTester
- init {
    self = [super init];
    if (self) {
        srcObject = [[NSObject alloc] init];
    }
    return self;
}

- (void)finalize {
    // verify objc_memmove_collectable into dead ivar gives no resurrection warning
    if (!objc_is_finalized(srcObject)) {
        [[TestCase currentTestCase] fail:@"inner object not garbage"];
    }
    objc_memmove_collectable(&dstObject, &self->srcObject, sizeof(void *));
    [super finalize];
}

@end


@implementation ResurrectAssociatedTester
// associate a garbage object (self) with a non-garbage object
- (void)finalize {
    objc_setAssociatedObject(
                             [TestCase currentTestCase],     // target
                             self,       // use self as key
                             self,       // use self (garbage) as value
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);      
    objc_setAssociatedObject(
                             [TestCase currentTestCase],     // target
                             self,       // use self as key
                             nil,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);      
    [super finalize];
}
@end


@implementation ResurrectCASTester
// store thread local and global objects into global variable using atomic primitive, test for resurrection warning
- (void)finalize {
    objc_atomicCompareAndSwapGlobal(global, self, &global);
    objc_atomicCompareAndSwapGlobal(global, nil, &global);
    [super finalize];
}
@end

@implementation ResurrectGlobalTester
// store the garbage object in a global
- (void)finalize
{
    global = self;
    global = nil;
    [super finalize];
}
@end

@implementation ResurrectGlobalIvarTester
// store the garbage object in a global object's ivar
- (void)finalize
{
    ivarTester->object = self;
    ivarTester->object = nil;
    [super finalize];
}
@end

@implementation ResurrectGlobalMemmoveTester
// store the garbage object in a global object's ivar
- (void)finalize
{
    // verify objc_memmove_collectable into live ivar gives resurrection warning
    objc_memmove_collectable(&ivarTester->object, &self, sizeof(void *));
    [super finalize];
}
@end

@implementation ResurrectGlobalWeakTester
- (void)finalize
{
    // verify a store into a global weak produces a warning
    global_weak = self;
    global_weak = nil;
    [super finalize];
}
@end

@implementation ResurrectThreadedTester

static void *otherThreadResurrect(void *arg)
{
    objc_registerThreadWithCollector();
    global = arg;
    global = nil;
    return NULL;
}

- (void)finalize
{
    pthread_t pthread;
    pthread_create(&pthread, NULL, otherThreadResurrect, self);
    pthread_join(pthread, NULL);
    
    [super finalize];
}
@end

@implementation ResurrectRetainTester
- (void)finalize
{
    CFRetain(self);
    [super finalize];
}
@end

/* Resurrection base class implementation */
@implementation Resurrection

+ (void)initialize
{
    if (!ivarTester) ivarTester = [GlobalResurrectionContainer new];
}

+ (BOOL)isConcreteTestCase
{
    if (self == [Resurrection class])
        return NO;
    return [super isConcreteTestCase];
}

- (Class)testClass
{
    // construct the test class name from our class name
    NSString *testName = [[self className] stringByAppendingString:@"Tester"];
    return NSClassFromString(testName);
}

// by default all tests are run with both a local and a global test object
- (BOOL)doGlobal
{
    return YES;
}
- (BOOL)doLocal
{
    return YES;
}

// convenience method for returning an empty output list
- (NSString **)emptyErrorOutput
{
    static NSString *errMsgs[] = {
        nil
    };
    return errMsgs;
}

// convenience method for returning common output list
- (NSString **)standardErrorOutput
{
    static NSString *errMsgs[] = {
        @"resurrection error for object",
        @"garbage pointer stored into reachable memory, break on auto_zone_resurrection_error to debug",
        @"**resurrected**",
        nil
    };
    return errMsgs;
}

// convenience method for returning common output list
- (NSString **)storeErrorOutput
{
    static NSString *errMsgs[] = {
        @"storing an already collected object",
        @"resurrection error for object",
        @"garbage pointer stored into reachable memory, break on auto_zone_resurrection_error to debug",
        @"**resurrected**",
        nil
    };
    return errMsgs;
}

// subclasses override to return customized error messages
- (NSString **)expectedErrorOutput
{
    return [self standardErrorOutput];
}

// subclasses override to return customized error messages for the thread local test block
- (NSString **)expectedLocalErrorOutput
{
    return [self expectedErrorOutput];
}

// subclasses override to return customized error messages for the global test block
- (NSString **)expectedGlobalErrorOutput
{
    return [self expectedErrorOutput];
}

- (NSArray *)localErrorOutput
{
    int c = 0;
    NSString **messages = [self expectedLocalErrorOutput];
    while (messages[c]) c++;
    return [NSArray arrayWithObjects:messages count:c];
}

- (NSArray *)globalErrorOutput
{
    int c = 0;
    NSString **messages = [self expectedGlobalErrorOutput];
    while (messages[c]) c++;
    return [NSArray arrayWithObjects:messages count:c];
}

- (BOOL)isFatalResurrection
{
    return NO;
}

- (NSString *)shouldSkip
{
    if ([self isFatalResurrection] && [self resurrectionIsFatal])
        return @"Resurrection is fatal";
    return [super shouldSkip];
}

- (void)processOutputLine:(NSString *)line
{
    NSString *expected = nil;
    if ([_expectedMessages count])
        expected = [_expectedMessages objectAtIndex:0];
    NSRange errRange;
    if (expected) 
        errRange = [line rangeOfString:expected];
    else
        errRange.location = NSNotFound;
    
    if (errRange.location == NSNotFound) {
        [super processOutputLine:line];
    } else {
        [_expectedMessages removeObjectAtIndex:0];
    }
}

- (void)checkResult
{
    if (_localTester)
        [self fail:@"did not finalize local test object"];
    if (_globalTester)
        [self fail:@"did not finalize global test object"];
    
    // run another collection to clean up the resurrected objects
    [self requestFullCollectionWithCompletionCallback:^{ [self testFinished]; }];
}

// done as a separate method to avoid keeping a reference on the stack
- (void)allocateLocal
{
    _localTester = [self disguise:[[self testClass] new]];
}

- (void)performGlobalTest
{
    if ([self doGlobal]) {
        [_expectedMessages addObjectsFromArray:[self localErrorOutput]];
        global = [[self testClass] new];
        _globalTester = [self disguise:global];
        global = nil;
    }
    [self requestFullCollectionWithCompletionCallback:^{ [self checkResult]; }];
}

- (void)performTest
{
    _expectedMessages = [NSMutableArray array];
    if ([self doLocal]) {
        [self allocateLocal];
        [self clearStack];
        [_expectedMessages addObjectsFromArray:[self localErrorOutput]];
        [self runThreadLocalCollection];
        // Must run 2 collections now. The first will clear the resurrected local block out of
        // the zombie list. The 2nd will cause it to finalize and generate a log.
        [self requestFullCollectionWithCompletionCallback:^{ 
            [self requestFullCollectionWithCompletionCallback: ^{ [self performGlobalTest]; }];
        }];
    } else {
        [self performGlobalTest];
    }
}

- (void)didFinalize:(TestFinalizer *)finalizer
{
    if (finalizer == [self undisguise:_localTester])
        _localTester = 0;
    if (finalizer == [self undisguise:_globalTester])
        _globalTester = 0;
}

- (void)outputComplete
{
    if (![self failed]) {
        if (_expectedMessages == nil || [_expectedMessages count] == 0)
            [self passed];
        else
            [self fail:@"did not emit expected error messages"];
    }
    [super outputComplete];
}
@end


/* Concrete TestCase subclasses */

@implementation ResurrectMemmoveOk
- (NSString **)expectedErrorOutput
{
    return [self emptyErrorOutput];
}
@end

@implementation ResurrectAssociated
@end

@implementation ResurrectCAS
@end

@implementation ResurrectGlobal
- (NSString **)expectedErrorOutput
{
    return [self storeErrorOutput];
}
@end

@implementation ResurrectGlobalIvar
@end

@implementation ResurrectGlobalMemmove
- (NSString *)shouldSkip
{
    return @"no resurrection check in auto_zone_write_barrier_memmove()";
}
@end

@implementation ResurrectGlobalWeak
@end

@implementation ResurrectThreaded
- (BOOL)doLocal
{
    return NO;
}
- (NSString **)expectedErrorOutput
{
    return [self storeErrorOutput];
}
@end

@implementation ResurrectRetain
- (BOOL)isFatalResurrection
{
    return YES;
}

- (NSString **)expectedErrorOutput
{
    static NSString *errMsgs[] = {
        @"was over-retained during finalization, refcount = 1",
        @"This could be an unbalanced CFRetain(), or CFRetain() balanced with -release.",
        @"Break on auto_zone_resurrection_error() to debug.",
        nil
    };
    return errMsgs;
}

@end
