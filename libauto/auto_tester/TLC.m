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
//  TLC.m
//  Copyright (c) 2009-2011 Apple Inc. All rights reserved.
//

#import "BlackBoxTest.h"

/*
 TLC_Stack_Root verifies that a stack reference keeps a thread local object rooted
 */
@interface TLC_Stack_Root : BlackBoxTest
{
    vm_address_t _testBlock;
}

@end

/*
 TLC_Local_Finalize verifies that a thread local collection finalizes a test object on the calling thread.
 */
@interface TLC_Local_Finalize : BlackBoxTest 
{
    pthread_t _thread;
}
@end

/*
 TLC_Demand_Collect allocates many local objects without ever requesting a collection and verifies that
 local objects are collected and finalized on a different thread.
 */
@interface TLC_Demand_Collect : BlackBoxTest
{
    pthread_t _thread;
    volatile BOOL _somethingFinalized;
}
@end



@implementation TLC_Stack_Root

- (void)performTest
{
    id testObject = [[TestFinalizer alloc] init];
    _testBlock = [self disguise:testObject];
    [self runThreadLocalCollection];
    [testObject self];
    
    if ([self result]==IN_PROGRESS)
        [self passed];
    [self testFinished];
}

- (void)didFinalize:(TestFinalizer *)finalizer
{
    if (finalizer == [self undisguise:_testBlock])
        [self fail:@"stack reference did not root thread local object"];
}

@end


@implementation TLC_Local_Finalize

- (void)allocateOneLocal
{
    [self disguise:[[TestFinalizer alloc] init]];
}

- (void)performTest
{
    _thread = pthread_self();
    [self allocateOneLocal];
    [self clearStack];
    [self runThreadLocalCollection];
    if ([self result]!=PASSED)
        [self fail:@"Failed to finalize local object on calling thread."];
    [self testFinished];
}

- (void)didFinalize:(TestFinalizer *)finalizer
{
    if (_thread == pthread_self())
        [self passed];
}

@end


@implementation TLC_Demand_Collect

- (void)performTest
{
    _thread = pthread_self();
    while (!_somethingFinalized) {
        [[TestFinalizer alloc] init];
    }
    [self passed];
    [self testFinished];
}

- (void)didFinalize:(TestFinalizer *)finalizer
{
    if (_thread == pthread_self() && !_somethingFinalized)
        [self fail:@"Demand based TLC ran finalizer on allocating thread"];
    _somethingFinalized = YES;
}

@end
