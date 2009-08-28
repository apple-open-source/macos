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
//  Radar5983285.m
//  auto
//
//  Created by Josh Behnke on 6/5/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import "Radar5983285.h"
#import <objc/objc-auto.h>

// simple test class with nontrivial layout
@interface Radar5983285_TestClass : NSObject
{
    id foo;
    int bar;
    id cat;
}
@end
@implementation Radar5983285_TestClass
@end

@implementation Radar5983285

@synthesize disguisedPointer;
@synthesize testThreadSynchronizer;
@synthesize collectorThreadSynchronizer;
@synthesize testBlockCollected;
@synthesize correctLayout;
@synthesize scannedWithBogusLayout;

- (void)startTest
{
    // allocate a test block and fetch its layout
    id testObject = [Radar5983285_TestClass new];
    auto_collection_control_t *control = auto_collection_parameters([self auto_zone]);
    self.correctLayout = control->layout_for_address([self auto_zone], testObject);
    [self setDisguisedPointer:[self disguise:testObject]];
    
    // keep a reference to the test block on the stack
    [self stackPointers][0] = testObject;
    
    // request a generational collection and go to sleep until our block gets pended
    self.testThreadSynchronizer = [self setNextTestSelector:@selector(blockWasPended)];
    [self requestGenerationalCollection];
}

- (void)blockWasPended
{
    if (_blockWasPended) {
        // The heap collector is about to pend our test block.
        // Clear our local reference and run a local collection.
        [self stackPointers][0] = nil;
        
        // allocate a bunch of objects to encourage the local collector to run
        int i;
        for (i=0; i<5000; i++)
            [NSObject new];
        
        // this runs a local collection
        [self requestCollection:AUTO_COLLECT_GENERATIONAL_COLLECTION|AUTO_COLLECT_IF_NEEDED];
        
        if (!self.testBlockCollected)
            [self fail:"test block was not collected as expected during local collection"];
        
        // now wake up the heap collector
        [self.collectorThreadSynchronizer signal];
        self.testThreadSynchronizer = [self setNextTestSelector:@selector(testResult)];
    } else {
        [self fail:"test block was not pended as expected"];
    }
}

- (void)testResult
{
    // check that our block was scanned with the correct layout
    if (self.scannedWithBogusLayout)
        [self fail:"block was scanned with invalid layout"];
}

- (void)heapCollectionComplete
{
    // just wake up test thread
    [self.testThreadSynchronizer signal];
    [super heapCollectionComplete];
}

- (void)setPending:(void *)block
{
    // wake up test thread once our block gets pended
    if (block == [self undisguise:self.disguisedPointer]) {
        _blockWasPended = YES;
        [self.testThreadSynchronizer signal];
        self.testThreadSynchronizer = nil;
        self.collectorThreadSynchronizer = [self setNextTestSelector:@selector(nop)];
    }
    [super setPending:block];
}

- (void)scanBlock:(void *)block endAddress:(void *)end
{
    // our block has layout info so should not be scanned conservatively
    if (block == [self undisguise:self.disguisedPointer]) {
        [self fail:"test block scanned without layout"];
    }
    [super scanBlock:block endAddress:end];
}

- (void)scanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map
{
    // verify the block's layout
    if (block == [self undisguise:self.disguisedPointer]) {
        if (map != self.correctLayout)
            self.scannedWithBogusLayout = YES;
    }
    [super scanBlock:block endAddress:end withLayout:map];
}

- (void)scanBlock:(void *)block endAddress:(void *)end withWeakLayout:(const unsigned char *)map
{
    // our block does not have weak layout
    if (block == [self undisguise:self.disguisedPointer]) {
        [self fail:"test block scanned with weak layout"];
    }
    [super scanBlock:block endAddress:end withWeakLayout:map];
}

- (void)endLocalScanWithGarbage:(void **)garbage_list count:(size_t)count
{
    // monitor for our test block becoming garbage
    if ([self block:[self undisguise:self.disguisedPointer] isInList:garbage_list count:count]) {
        self.testBlockCollected = YES;
    }
    [super endLocalScanWithGarbage:garbage_list count:count];
}

- (void)blockBecameGlobal:(void *)block withAge:(uint32_t)age
{
    // monitor if our test block becomes global (it should not)
    if (block == [self undisguise:self.disguisedPointer]) {
        [self fail:"test block became global unexpectedly"];
    }
    [super blockBecameGlobal:block withAge:age];
}

@end
