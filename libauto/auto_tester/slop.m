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
//  slop.m
//  Copyright (c) 2009-2011 Apple Inc. All rights reserved.
//

#import "BlackBoxTest.h"
#import <objc/runtime.h>

@interface SlopTester : TestFinalizer
@end

@implementation SlopTester
- (id)init
{
    self = [super init];
    if (self) {
        size_t mySize = malloc_size(self);
        size_t instanceSize = class_getInstanceSize([self class]);
        char *slopPtr = (char *)self;
        for (size_t slopIndex = instanceSize; slopIndex < mySize; slopIndex++) {
            if (slopPtr[slopIndex])
                [[TestCase currentTestCase] fail:@"detected nonzero byte in slop area"];
        }
        
        // store some nonzero bytes in the slop for the next iteration to check
        for (size_t slopIndex = instanceSize; slopIndex < mySize; slopIndex++) {
            slopPtr[slopIndex] = 0xff;
        }
    }
    return self;
}
@end


@interface Slop : BlackBoxTest
{
    int _iterations;
    int _blockCount;
    int _reusedCount;
    NSHashTable *_usedBlocks;
}
- (void)iterate;
@end

@implementation Slop

#define EXTRA_SIZE_MAX 1024
#define MIN_ITERATIONS 5

- (void)checkResult
{
    if (_blockCount > 0) {
        [self fail:[NSString stringWithFormat:@"did not collect %d test blocks", _blockCount]];
    } else {
        if (_reusedCount < EXTRA_SIZE_MAX || _iterations < MIN_ITERATIONS) {
            [self iterate];
            return;
        }
    }
    if (_reusedCount == 0) {
        [self fail:@"did not reuse any blocks - slop may not be checked"];
    }
    [self passed];
    [self testFinished];
    _usedBlocks = nil;
}

- (void)allocate
{
    @synchronized(self) {
        Class st = [SlopTester class];
        for (size_t extra = 0; extra < EXTRA_SIZE_MAX; extra++) {
            id o = [NSAllocateObject(st, extra, NULL) init];
            if ([_usedBlocks member:(id)[self disguise:o]])
                _reusedCount++;
            else
                [_usedBlocks addObject:(id)[self disguise:o]];
            _blockCount++;
        }
    }
}

- (void)iterate
{
    _iterations++;
    [self allocate];
    [self clearStack];
    [self runThreadLocalCollection];
    [self requestFullCollectionWithCompletionCallback:^{ [self checkResult]; }];
}

- (void)performTest
{
    _usedBlocks = [NSHashTable hashTableWithOptions:NSPointerFunctionsOpaquePersonality];
    [self iterate];
}

- (void)didFinalize:(TestFinalizer *)finalizer
{
    @synchronized(self) {
        _blockCount--;
    }
}

@end
