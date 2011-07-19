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
//  AssociativeRefRecovery.m
//  Copyright (c) 2008-2011 Apple Inc. All rights reserved.
//

#import <auto_zone.h>
#import <malloc/malloc.h>
#import <objc/runtime.h>

#import "BlackBoxTest.h"

/*
 This test verifies that associatied blocks are reclaimed along with the main block.
 It tests a Foundation object, bridged CF object, and a non-object block.
 */

static void *_foundation_key;
static void *_cf_bridged_key;
static void *_non_object_key;


@interface AssociativeRefRecovery : BlackBoxTest {
    BOOL _main_block_finalized;
    id _main_block;
    vm_address_t _disguised_main_block;
    __weak id _foundation_ref;
    __weak CFArrayRef _cf_bridged_ref;
    __weak void *_non_object;
}

- (void)verifyNotCollected;
- (void)verifyCollected;

@end

@implementation AssociativeRefRecovery
- (void)performTest
{
    _main_block = [TestFinalizer new];
    _disguised_main_block = [self disguise:_main_block];
    
    id obj = [NSObject new];
    _foundation_ref = obj;
    auto_zone_set_associative_ref([self auto_zone], _main_block, &_foundation_key, obj);
    
    CFArrayRef cf = CFArrayCreate(NULL, NULL, 0, NULL);
    CFMakeCollectable(cf);
    _cf_bridged_ref = cf;
    auto_zone_set_associative_ref([self auto_zone], (void *)_main_block, &_cf_bridged_key, (void *)cf);

    void *block = malloc_zone_malloc([self auto_zone], 1);
    _non_object = block;
    auto_zone_set_associative_ref([self auto_zone], (void *)_main_block, &_non_object_key, block);
    auto_zone_release([self auto_zone], block);
    
    // Run a full collection
    [self requestFullCollectionWithCompletionCallback:^{ [self verifyNotCollected]; }];
}

- (void)verifyNotCollected
{
    if (_main_block_finalized)
        [self fail:@"main block was incorrectly collected"];
    
    if (!_foundation_ref)
        [self fail:@"associated foundation object was incorrectly collected"];
    if (auto_zone_get_associative_ref([self auto_zone], _main_block, &_foundation_key) != _foundation_ref)
        [self fail:@"failed to retrieve associated foundation object"];
    
    if (!_cf_bridged_ref)
        [self fail:@"associated bridged CF object was incorrectly collected"];
    if (auto_zone_get_associative_ref([self auto_zone], _main_block, &_cf_bridged_key) != _cf_bridged_ref)
        [self fail:@"failed to retrieve associated bridged CF object"];

    if (!_non_object)
        [self fail:@"associated non-object block was incorrectly collected"];
    if (auto_zone_get_associative_ref([self auto_zone], _main_block, &_non_object_key) != _non_object)
        [self fail:@"failed to retrieve associated non-object block"];

    if ([self result] != FAILED) {
        _main_block = nil;
        [self requestFullCollectionWithCompletionCallback:^{ [self verifyCollected]; }];
    }
}

- (void)verifyCollected
{
    if (!_main_block_finalized)
        [self fail:@"main block was not collected"];
    if (_foundation_ref)
        [self fail:@"failed to collect block associated with foundation object"];
    if (_cf_bridged_ref)
        [self fail:@"failed to collect block associated with bridged CF object"];
    if (_non_object)
        [self fail:@"failed to collect block associated with non-object block"];
    
    [self passed];
    [self testFinished];
}

- (void)didFinalize:(TestFinalizer *)finalizer
{
    if (finalizer == [self undisguise:_disguised_main_block])
        _main_block_finalized = YES;
}

@end


@interface AssociativeLinkedList : BlackBoxTest {
    NSPointerArray *objects;
    NSUInteger count;
}
- (void)performTest;
- (void)verifyNotCollected;
@end

@implementation AssociativeLinkedList

static char next_key;

static NSData *unscannedData() {
    const NSUInteger length = 16;
    NSMutableData *data = [NSMutableData dataWithLength:16];
    uint8_t *bytes = (uint8_t *)[data mutableBytes];
    for (NSUInteger i = 0; i < length; ++i) {
        *bytes++ = (uint8_t)random();
    }
    return [data copyWithZone:NULL];
}

- (void)performTest {
    objects = [NSPointerArray pointerArrayWithOptions:NSPointerFunctionsZeroingWeakMemory];
    
    // created an associative linked list using unscanned objects. then verify that none of the elements have been prematurely collected.
    id *list = auto_zone_allocate_object([self auto_zone], sizeof(id), AUTO_POINTERS_ONLY, false, true);
    *list = unscannedData();
    id current = *list;
    for (int i = 0; i < 1000; i++) {
        id next = unscannedData();
        objc_setAssociatedObject(current, &next_key, next, OBJC_ASSOCIATION_ASSIGN);
        current = next;
    }
    objc_setAssociatedObject(self, &next_key, (id)list, OBJC_ASSOCIATION_ASSIGN);
    
    current = *list;
    while (current != nil) {
        [objects addPointer:current];
        current = objc_getAssociatedObject(current, &next_key);
    }
    count = [objects count];
    
    // Run a full collection
    [self requestExhaustiveCollectionWithCompletionCallback:^{ [self verifyNotCollected]; }];
}

- (void)verifyNotCollected {
    [objects compact];
    if ([objects count] != count) {
        [self fail:@"unscanned objects failed to stay alive across a collection."];
    } else {
        [self passed];
    }
    [self testFinished];
}

@end
