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
//  BlockLifetime.m
//  auto
//
//  Created by Josh Behnke on 5/19/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import "BlockLifetime.h"
#import "AutoConfiguration.h"

@interface BlockLifetimeTestObject : NSObject
{
    @public
    id anObject;
}
@end
@implementation BlockLifetimeTestObject
@end


@implementation BlockLifetime

static id _globalTestBlock = nil;

// First step: allocate a new object and request a full collection.
// The new object should be thread local at this point.
- (void)startTest
{
    
    // We need our test object to live in a space in memory that has its own write barrier card.
    // We try to accomplish this by allocating a bunch of objects that we then throw away.
//    BlockLifetimeTestObject *first = [BlockLifetimeTestObject new];
//    BlockLifetimeTestObject *current = first;
//    for (int i=0; i<Auto::write_barrier_quantum / sizeof(BlockLifetimeTestObject) * 10; i++) {
//        current->anObject = [BlockLifetimeTestObject new];
//        current = (BlockLifetimeTestObject *)current->anObject;
//    }

    // Create the test block and assign it directly to _disguisedBlock. 
    // This enables all the probes that look for the test block.
    _disguisedTestBlock = [self disguise:[BlockLifetimeTestObject new]];
    
    // We store the test block on the stack.
    STACK_POINTER_TYPE *stackPointers = [self stackPointers];
    stackPointers[0] = (id)[self undisguise:_disguisedTestBlock];
    
    // Run a full collection
    [self requestFullCollection];
    _testThreadSynchronizer = [self setNextTestSelector:@selector(globalTransition)];
}

// defeat the write barrier card optimization by touching the object
- (void)touchObject
{
    BlockLifetimeTestObject *object = (BlockLifetimeTestObject *)[self undisguise:_disguisedTestBlock];
    object->anObject = self;
    object->anObject = nil;
}

// verify the block got scanned, flag an error if it did not
- (void)verifyScanned
{
    if (!_scanned)
        [self fail:"block was not scanned as expected"];
    _scanned = NO;
}


// Second step: transition the block to global
- (void)globalTransition
{
    [self verifyScanned];
    
    // verify the block isn't global already
    if (_becameGlobal)
        [self fail:"block became global prematurely"];
    
    // make the block go global and verify that we saw the transition
    // it's possible we never saw the transition because it went global before we started watching
    // that situation probably merits investigation
    _globalTestBlock = (id)[self undisguise:_disguisedTestBlock];
    if (!_becameGlobal)
        [self fail:"block failed to become global"];
    
    // now switch the block's reference off the stack and into an ivar
    STACK_POINTER_TYPE *stackPointers = [self stackPointers];
    stackPointers[0] = nil;
    // now the only reference is in _globalTestBlock

    // do generational collections until the block becomes old
    [self touchObject];
    [self requestGenerationalCollection];
    _testThreadSynchronizer = [self setNextTestSelector:@selector(ageLoop)];
}


// This method runs generational collections until the block reaches age 0
- (void)ageLoop
{
    SEL next;
    [self verifyScanned];

    if (_age != 0) {
        // set up for the next iteration
        next = _cmd;
        [self touchObject];
        [self requestGenerationalCollection];
    } else {
        // We are done iterating, the block is now age 0.
        // Now we want to verify that the block does *NOT* get scanned during a generational collection.
        // First must run a full collection to clear write barrier cards.
        next = @selector(clearBarriers);
        [self requestFullCollection];
    }
    _testThreadSynchronizer = [self setNextTestSelector:next];
}


// The full collection does not clear write barrier cards. Run a generational to clear them.
- (void)clearBarriers
{
    [self verifyScanned];
    [self requestGenerationalCollection];
    _testThreadSynchronizer = [self setNextTestSelector:@selector(generationalScan)];
}


// Now the object is old and should have the write barrier cards cleared.
// Verify that a generational scan does *not* scan the block.
- (void)generationalScan
{
    [self verifyScanned];
    [self requestGenerationalCollection];
    _testThreadSynchronizer = [self setNextTestSelector:@selector(verifyGenerationalScan)];
}


// The last scan was generational, the block is old, and write barriers were cleared. Verify the block was *not* scanned.
- (void)verifyGenerationalScan
{
    if (_scanned)
        [self fail:"age 0 block scanned during generational collection"];
    
    // Now just verify that the block gets collected
    _globalTestBlock = nil;
    _shouldCollect = YES;
    
    // Run a generational and verify the old block was not collected.
    [self requestGenerationalCollection];
    _testThreadSynchronizer = [self setNextTestSelector:@selector(uncollectedCheck)];
}


// We expect that the old garbage block would not be collected by a generational collection
- (void)uncollectedCheck
{
    if (_collected)
        [self fail:"old block collected by generational collection"];
    
    // now run a full collection
    [self requestFullCollection];
    _testThreadSynchronizer = [self setNextTestSelector:@selector(collectedCheck)];
}


// A full collection should have collected the garbage block.
- (void)collectedCheck
{
    if (!_collected)
        [self fail:"block not collected after full collection"];
    // done with test
}


// In this test we always want to wake up the test thread when a collection completes.
- (void)collectionComplete
{
    [_testThreadSynchronizer signal];
    [super heapCollectionComplete];
}


// Watch for our test block becoming global.
- (void)blockBecameGlobal:(void *)block withAge:(uint32_t)age
{
    if (block == [self undisguise:_disguisedTestBlock]) {
        _becameGlobal = YES;
        _age = age;
    }
    [super blockBecameGlobal:block withAge:age];
}


// Monitor the aging of our test block
- (void)blockMatured:(void *)block newAge:(uint32_t)age
{
    if (block == [self undisguise:_disguisedTestBlock]) {
        if (age != _age - 1)
            [self fail:"Age decrease != 1 after mature"];
        _age = age;
    }
    [super blockMatured:block newAge:age];
}


// Check if the test block is in the garbage list
- (void)endHeapScanWithGarbage:(void **)garbage_list count:(size_t)count
{
    if ([self block:[self undisguise:_disguisedTestBlock] isInList:garbage_list count:count]) {
        if (!_shouldCollect)
            [self fail:"block was collected prematurely"];
        _collected = YES;
    }
    [super endHeapScanWithGarbage:garbage_list count:count];
}


// Monitor when our test block is scanned, and verify it never gets scanned twice in the same collection.
- (void)scanBlock:(void *)block endAddress:(void *)end
{
    if (block == [self undisguise:_disguisedTestBlock]) {
        if (_scanned) {
            [self fail:"block scanned twice"];
        }
        _scanned = YES;
    }
    [super scanBlock:block endAddress:end];
}

- (void)scanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map
{
    [self scanBlock:block endAddress:end];
    [super scanBlock:block endAddress:end withLayout:map];
}

- (void)scanBlock:(void *)block endAddress:(void *)end withWeakLayout:(const unsigned char *)map
{
    [self scanBlock:block endAddress:end];
    [super scanBlock:block endAddress:end withWeakLayout:map];
}

@end
