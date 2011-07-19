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
//  CollectionChecking.m
//  Copyright (c) 2010-2011 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "BlackBoxTest.h"

@interface CollectionCheckingBlock : BlackBoxTest
{
    __strong void *_testBlock;
    BOOL _gotCallback;
}
@end

@interface SubzoneBlockCollectionCheck : CollectionCheckingBlock
@end

@interface LargeBlockCollectionCheck : CollectionCheckingBlock
@end

@implementation CollectionCheckingBlock

+ (BOOL)isConcreteTestCase
{
    return self != [CollectionCheckingBlock class];
}

- (__strong void *)allocateBlock
{
}

- (void)testFinished
{
    _testBlock = nil;
    auto_zone_disable_collection_checking([self auto_zone]);
    [super testFinished];
}

- (void)processOutputLine:(NSString *)line
{
    if ([line rangeOfString:@"was not collected"].location != NSNotFound) {
    } else {
        [super processOutputLine:line];
    }
}

- (void)outputComplete
{
    if (_gotCallback) {
        [self passed];
    } else {
        [self fail:@"collection checking did not call callback with block"];
    }
    [super outputComplete];
}

- (id)callback
{
    return nil;
}

- (void)performTest
{
    _testBlock = [self allocateBlock];
    malloc_zone_enable_discharge_checking([self auto_zone]);
    malloc_zone_discharge([self auto_zone], _testBlock);
    //auto_zone_enumerate_uncollected([self auto_zone], NULL);
    malloc_zone_enumerate_discharged_pointers([self auto_zone], ^(void *block, void *info){
        if (block == _testBlock) {
            _gotCallback = YES;
        }
    });
    [self testFinished];
}
@end

@implementation SubzoneBlockCollectionCheck
- (__strong void *)allocateBlock
{
    return NSAllocateObject([NSObject class], 0, NULL);
}
@end

@implementation LargeBlockCollectionCheck
- (__strong void *)allocateBlock
{
    return NSAllocateObject([NSObject class], 1024*1024, NULL);
}
@end
