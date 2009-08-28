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
//  AssociativeRefRecovery.m
//  auto
//
//  Created by Josh Behnke on 8/28/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import "AssociativeRefRecovery.h"
#import <auto_zone.h>
#import <malloc/malloc.h>

@implementation AssociativeRefRecovery
- (void)startTest
{
    id obj = [NSObject new];
    _foundation_ref = [self disguise:[NSObject new]];
    auto_zone_set_associative_ref([self auto_zone], obj, self, [self undisguise:_foundation_ref]);
    
    CFArrayRef cf = CFArrayCreate(NULL, NULL, 0, NULL);
    CFMakeCollectable(cf);
    _cf_bridged_ref = [self disguise:[NSObject new]];
    auto_zone_set_associative_ref([self auto_zone], (void *)cf, self, [self undisguise:_cf_bridged_ref]);

    void *block = malloc_zone_malloc([self auto_zone], 1);
    _non_object = [self disguise:[NSObject new]];
    auto_zone_set_associative_ref([self auto_zone], (void *)block, self, [self undisguise:_non_object]);
    auto_zone_release([self auto_zone], block);
    
    // Run a full collection
    [self requestFullCollection];
    _sync = [self setNextTestSelector:@selector(verifyResults)];
}

- (void)verifyResults
{
    if (!_foundation_ref_collected)
        [self fail:"failed to collect block associated with foundation object"];
    if (!_cf_bridged_ref_collected)
        [self fail:"failed to collect block associated with bridged CF object"];
    if (!_non_object_ref_collected)
        [self fail:"failed to collect block associated with non-object block"];
}

- (void)heapCollectionComplete
{
    [_sync signal];
    [super heapCollectionComplete];
}

- (void)endHeapScanWithGarbage:(void **)garbage_list count:(size_t)count
{
    if ([self block:[self undisguise:_foundation_ref] isInList:garbage_list count:count]) {
        _foundation_ref_collected = YES;
    }
    if ([self block:[self undisguise:_cf_bridged_ref] isInList:garbage_list count:count]) {
        _cf_bridged_ref_collected = YES;
    }
    if ([self block:[self undisguise:_non_object] isInList:garbage_list count:count]) {
        _non_object_ref_collected = YES;
    }
    [super endHeapScanWithGarbage:garbage_list count:count];
}


@end
