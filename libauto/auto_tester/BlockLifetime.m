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
//  BlockLifetime.m
//  Copyright (c) 2008-2011 Apple Inc. All rights reserved.
//

#import "Configuration.h"
#import "WhiteBoxTest.h"

/*
 BlockLifetime implements the following test scenario:
 Allocate a test block.
 Perform full collection.
 Verify test block was scanned exactly once.
 Verify test block remained local.
 Assign test block to a global variable.
 Verify test block became global.
 Clear global variable, assign test block to ivar.
 Loop:
 run generational collection
 verify block is scanned exactly once and age decrements by 1
 until test block age becomes 0.
 Run a full collection (to clear write barriers).
 Verify block is scanned exactly once.
 Run a generational collection.
 Verify block *not* scanned.
 Clear test block ivar.
 Run generational collection.
 Verify block *not* scanned and *not* collected.
 Run a full collection.
 Verify block was collected.
 */

@interface BlockLifetime : WhiteBoxTest {
    BOOL _becameGlobal;
    BOOL _scanned;
    BOOL _collected;
    BOOL _shouldCollect;
    uint32_t _age;
    id _testBlock;
    vm_address_t _disguisedTestBlock;
}

@end

@interface BlockLifetimeTestObject : NSObject
{
    @public
    id anObject;
}
@end
@implementation BlockLifetimeTestObject
@end

@interface BlockLifetime(internal)
- (void)globalTransition;
- (void)ageLoop;
- (void)clearBarriers;
- (void)generationalScan;
- (void)verifyGenerationalScan;
- (void)uncollectedCheck;
- (void)collectedCheck;
@end


@implementation BlockLifetime

static id _globalTestBlock = nil;

- (void)allocLocalBlock
{
    _disguisedTestBlock = [self disguise:[BlockLifetimeTestObject new]];
    [self testThreadStackBuffer][0] = (id)[self undisguise:_disguisedTestBlock];
}

- (void)makeTestBlockGlobal
{
    _globalTestBlock = (id)[self undisguise:_disguisedTestBlock];
    [self testThreadStackBuffer][0] = nil;
}

// First step: allocate a new object and request a full collection.
// The new object should be thread local at this point.
- (void)performTest
{
    [self performSelectorOnTestThread:@selector(allocLocalBlock)];
        
    // Run a full collection
    [self requestFullCollectionWithCompletionCallback:^{ [self globalTransition]; }];
}

// defeat the write barrier card optimization by touching the object
- (void)touchObject
{
    BlockLifetimeTestObject *object = (BlockLifetimeTestObject *)[self undisguise:_disguisedTestBlock];
    object->anObject = self;
    object->anObject = nil;
}

// verify the block got scanned, flag an error if it did not
#define verifyScanned() \
do { if (!_scanned) { [self fail:[NSString stringWithFormat:@"block was not scanned as expected in %@", NSStringFromSelector(_cmd)]]; [self testFinished]; return; } _scanned = NO; } while(0)


// Second step: transition the block to global
- (void)globalTransition
{
    verifyScanned();
    
    // verify the block isn't global already
    if (_becameGlobal) {
        [self fail:@"block became global prematurely"];
        [self testFinished];
        return;
    }
    
    // make the block go global and verify that we saw the transition
    // it's possible we never saw the transition because it went global before we started watching
    // that situation probably merits investigation
    [self performSelectorOnTestThread:@selector(makeTestBlockGlobal)];

    if (!_becameGlobal) {
        [self fail:@"block failed to become global"];
        [self testFinished];
        return;
    }
    
    // now the only reference to the test block is in _globalTestBlock

    // do generational collections until the block becomes old
    [self touchObject];
    [self requestGenerationalCollectionWithCompletionCallback:^{ [self ageLoop]; }];
}


// This method runs generational collections until the block reaches age 0
- (void)ageLoop
{
    SEL next;
    verifyScanned();

    if (_age != 0) {
        // set up for the next iteration
        [self touchObject];
        [self requestGenerationalCollectionWithCompletionCallback:^{ [self ageLoop]; }];
    } else {
        // We are done iterating, the block is now age 0.
        // Now we want to verify that the block does *NOT* get scanned during a generational collection.
        // First must run a full collection to clear write barrier cards.
        [self requestFullCollectionWithCompletionCallback:^{ [self clearBarriers]; }];
    }
}


// The full collection does not clear write barrier cards. Run a generational to clear them.
- (void)clearBarriers
{
    verifyScanned();
    [self requestGenerationalCollectionWithCompletionCallback:^{ [self generationalScan]; }];
}


// Now the object is old and should have the write barrier cards cleared.
// Verify that a generational scan does *not* scan the block.
- (void)generationalScan
{
    verifyScanned();
    [self requestGenerationalCollectionWithCompletionCallback:^{ [self verifyGenerationalScan]; }];
}


// The last scan was generational, the block is old, and write barriers were cleared. Verify the block was *not* scanned.
- (void)verifyGenerationalScan
{
    if (_scanned) {
        [self fail:@"age 0 block scanned during generational collection"];
        [self testFinished];
        return;
    }
    
    // Now just verify that the block gets collected
    _globalTestBlock = nil;
    _shouldCollect = YES;
    
    // Run a generational and verify the old block was not collected.
    [self requestGenerationalCollectionWithCompletionCallback:^{ [self uncollectedCheck]; }];
}


// We expect that the old garbage block would not be collected by a generational collection
- (void)uncollectedCheck
{
    if (_collected) {
        [self fail:@"old block collected by generational collection"];
        [self testFinished];
        return;
    }
    
    // now run a full collection
    [self requestFullCollectionWithCompletionCallback:^{ [self collectedCheck]; }];
}


// A full collection should have collected the garbage block.
- (void)collectedCheck
{
    if (!_collected)
        [self fail:@"block not collected after full collection"];
    else
        [self passed];
    [self testFinished];
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
            [self fail:@"Age decrease != 1 after mature"];
        _age = age;
    }
    [super blockMatured:block newAge:age];
}


// Check if the test block is in the garbage list
- (void)endHeapScanWithGarbage:(void **)garbage_list count:(size_t)count
{
    if ([self block:[self undisguise:_disguisedTestBlock] isInList:garbage_list count:count]) {
        if (!_shouldCollect)
            [self fail:@"block was collected prematurely"];
        _collected = YES;
    }
    [super endHeapScanWithGarbage:garbage_list count:count];
}


// Monitor when our test block is scanned, and verify it never gets scanned twice in the same collection.
- (void)scanBlock:(void *)block endAddress:(void *)end
{
    if (block == [self undisguise:_disguisedTestBlock]) {
        if (_scanned) {
            [self fail:@"block scanned twice"];
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

@end
