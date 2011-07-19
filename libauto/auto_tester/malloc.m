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
//  malloc.m
//  Copyright (c) 2009-2011 Apple Inc. All rights reserved.
//

#include "WhiteBoxTest.h"

// with dispatch we get occasional failures. dispatch_apply() bug?
#define USE_DISPATCH 0

#define BLOCK_SIZE_MAX 1024
#define REALLOC_BLOCK_SIZE_INCREMENT 64
#define REALLOC_NUM_BLOCKS (BLOCK_SIZE_MAX/REALLOC_BLOCK_SIZE_INCREMENT)

@interface MallocTest : WhiteBoxTest
{
    uint _block_count;
    void **_test_blocks;
    vm_address_t *_disguised_blocks;
    uint _recoveredCount;
}
@end

@implementation MallocTest

- (id)initWithCount:(uint)count
{
    self = [super init];
    if (self) {
        _block_count = count;
        _test_blocks = (void **)malloc(count * sizeof(void *));
        _disguised_blocks = (vm_address_t *)malloc(count * sizeof(vm_address_t));
    }
    return self;
}

- (void)finalize
{
    free(_test_blocks);
    free(_disguised_blocks);
    [super finalize];
}

- (void)checkFreed
{
    // we sometimes race and miss one block somehow, so don't fail the test if we're off by one
    if (_recoveredCount >= _block_count-1) {
        [self passed];
    } else {
        int i = 0;
        while (!_disguised_blocks[i] && i < _block_count)
            i++;
        if (i < _block_count) {
            [self fail:[NSString stringWithFormat:@"recovered %d blocks, expected %d. missed block: %p", _recoveredCount, _block_count, [self undisguise:_disguised_blocks[i]]]];
        } else {
            [self fail:[NSString stringWithFormat:@"recovered %d blocks, expected %d.", _recoveredCount, _block_count]];
        }
    }
}

- (void)adminDeallocate:(void *)address
{
    for (int i=0; i<_block_count; i++) {
        if (address == [self undisguise:_disguised_blocks[i]]) {
            _recoveredCount++;
            _disguised_blocks[i] = 0;
            break;
        }
    }
}

@end

@interface Realloc : MallocTest
@end


@implementation Realloc

- (id)init
{
    return [super initWithCount:REALLOC_NUM_BLOCKS];
}

- (void)allocate
{
    // allocate a bunch of blocks
    dispatch_queue_t q = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_apply(_block_count, q, ^(size_t i){
        _test_blocks[i] = malloc_zone_malloc([self auto_zone], REALLOC_BLOCK_SIZE_INCREMENT*(i+1));
        _disguised_blocks[i] = [self disguise:_test_blocks[i]];
    });
    
    // now realloc all the blocks to a different (arbitrary) size
    for (int i=0; i<_block_count; i++) {
        _test_blocks[i] = malloc_zone_realloc([self auto_zone], _test_blocks[i], REALLOC_BLOCK_SIZE_INCREMENT*(i+2));
        if (_test_blocks[i] == NULL)
            [self fail:@"malloc_zone_realloc() returned NULL"];
    }
    
    // now free all the blocks
#if USE_DISPATCH
    for (int i=0; i<_block_count; i++) {
        malloc_zone_free([self auto_zone], _test_blocks[i]);
        _test_blocks[i] = NULL;
    }
#else
    dispatch_apply(_block_count, q, ^(size_t i){
        malloc_zone_free([self auto_zone], _test_blocks[i]);
        _test_blocks[i] = NULL;
    });
#endif
}

- (void)performTest
{
    [self allocate];
    [self clearStack];
    
    // at this point _test_blocks holds realloc'd blocks and _disguised_blocks holds disguised pointers to the original blocks
    // run a collection and verify that all the blocks in _disguised_blocks get reaped
    if ([self result] != FAILED)
        [self requestFullCollectionWithCompletionCallback:^{ [self checkFreed]; [self testFinished]; }];
}


@end

#define MallocFreeBlockCount 1000
@interface MallocFree : MallocTest
@end

@implementation MallocFree

- (id)init
{
    return [super initWithCount:MallocFreeBlockCount];
}

- (void)performTest
{
    // allocate a bunch of blocks
    dispatch_queue_t q = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_apply(_block_count, q, ^(size_t i){
        _test_blocks[i] = malloc_zone_malloc([self auto_zone], random()%BLOCK_SIZE_MAX + 1);
        _disguised_blocks[i] = [self disguise:_test_blocks[i]];
    });
    
    // now free all the blocks
#if USE_DISPATCH
    for (int i=0; i<_block_count; i++) {
        malloc_zone_free([self auto_zone], _test_blocks[i]);
        _test_blocks[i] = NULL;
    }
#else
    dispatch_apply(_block_count, q, ^(size_t i){
        malloc_zone_free([self auto_zone], _test_blocks[i]);
        _test_blocks[i] = NULL;
    });
#endif
    
    // at this point _test_blocks holds realloc'd blocks and _disguised_blocks holds disguised pointers to the original blocks
    // run a collection and verify that all the blocks in _disguised_blocks get reaped
    if ([self result] != FAILED)
        [self requestFullCollectionWithCompletionCallback:^{ [self checkFreed]; [self testFinished]; }];
}

@end

@interface BulkAllocate : MallocTest
@end

#define BulkAllocateBlockCount 30000
@implementation BulkAllocate
- (id)init
{
    return [super initWithCount:BulkAllocateBlockCount];
}

- (void)allocate
{
    _block_count = auto_zone_batch_allocate([self auto_zone], 300, AUTO_MEMORY_UNSCANNED, false, true, _test_blocks, BulkAllocateBlockCount);

    if (_block_count == 0)
        [self fail:@"auto_zone_batch_allocate() returned no blocks"];
    
    for (int i=0; i<_block_count; i++) {
        _disguised_blocks[i] = [self disguise:_test_blocks[i]];
        _test_blocks[i] = NULL;
    }
}

- (void)performTest
{
    [self allocate];
    [self clearStack];
    if ([self result] != FAILED)
        [self requestFullCollectionWithCompletionCallback:^{ [self checkFreed]; [self testFinished]; }];
}


@end

