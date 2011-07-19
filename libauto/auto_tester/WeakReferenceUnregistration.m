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
//  WeakReferenceUnregistration.m
//  Copyright (c) 2010-2011 Apple Inc. All rights reserved.
//

#import "BlackBoxTest.h"

@interface WeakReferenceUnregistration : BlackBoxTest {
@public
    uintptr_t slot;
    BOOL finalized;
}
@end

@interface WeakSlotObject : TestFinalizer {
@public
    __weak WeakReferenceUnregistration *test;
}
@end

@implementation WeakSlotObject
@end

@implementation WeakReferenceUnregistration

- (void)didFinalize:(WeakSlotObject *)object {
    finalized = YES;
    if (object->test != self) [self fail:@"object->slot not pointing to self."];
}

- (BOOL)findSlot {
    __block BOOL foundSlot = NO;
    auto_zone_visitor_t weak_visitor = { sizeof(auto_zone_visitor_t) };
    weak_visitor.visit_weak = ^(const void *value, void *const*location, auto_weak_callback_block_t *callback) {
        if ((uintptr_t)location == slot) foundSlot = YES;
    };
    auto_zone_visit([self auto_zone], &weak_visitor);
    return foundSlot;
}

- (void)performTest {
    static volatile WeakSlotObject* object;
    object = [WeakSlotObject new];
    slot = (uintptr_t)&object->test;
    object->test = self;
    if (![self findSlot]) [self fail:@"weak slot not found"];
    object = nil;
    [self requestExhaustiveCollectionWithCompletionCallback:^{
        if (!self->finalized)
            [self fail:@"object not finalized."];
        else if ([self findSlot])
            [self fail:@"weak slot found!"];
        else
            [self passed];
        [self testFinished];
    }];
}

@end
