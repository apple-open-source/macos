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
//  Radar5983285.m
//  Copyright (c) 2008-2011 Apple Inc. All rights reserved.
//

#import "WhiteBoxTest.h"

@interface Radar5983285 : WhiteBoxTest {
    BOOL _blockWasPended;
    BOOL _blockWasScanned;
    vm_address_t disguisedPointer;
    BOOL testBlockCollected;
    const unsigned char *correctLayout;
    BOOL scannedWithBogusLayout;
    BOOL scannedWithCorrectLayout;
}

@property(readwrite, nonatomic) vm_address_t disguisedPointer;
@property(readwrite, nonatomic) BOOL testBlockCollected;
@property(readwrite, nonatomic) const unsigned char *correctLayout;
@property(readwrite, nonatomic) BOOL scannedWithBogusLayout;
@property(readwrite, nonatomic) BOOL scannedWithCorrectLayout;

@end

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
@synthesize testBlockCollected;
@synthesize correctLayout;
@synthesize scannedWithBogusLayout;
@synthesize scannedWithCorrectLayout;

- (void)allocateTestBlock
{
    // allocate a test block and fetch its layout
    id testObject = [Radar5983285_TestClass new];
    auto_collection_control_t *control = auto_collection_parameters([self auto_zone]);
    self.correctLayout = control->layout_for_address([self auto_zone], testObject);
    [self setDisguisedPointer:[self disguise:testObject]];
    
    // keep a reference to the test block on the stack
    [self testThreadStackBuffer][0] = testObject;
}


- (void)testResult
{
    // check that our block was scanned with the correct layout
    if (self.scannedWithBogusLayout)
        [self fail:@"block was scanned with invalid layout"];
    else
        [self passed];
    [self testFinished];
}

- (void)performTest
{
    [self performSelectorOnTestThread:@selector(allocateTestBlock)];
    
    // request a generational collection and go to sleep until our block gets pended
    [self requestGenerationalCollectionWithCompletionCallback:^{ [self testResult]; }];
}

- (void)blockWasPended
{
    if (_blockWasPended) {
        // The heap collector is about to pend our test block.
        // Clear our local reference and run a local collection.
        [self testThreadStackBuffer][0] = nil;
        [self runThreadLocalCollection];
        
        if (!self.testBlockCollected)
            [self fail:@"test block was not collected as expected during local collection"];
        
        // now wake up the heap collector
    } else {
        [self fail:@"test block was not pended as expected"];
    }
}

- (void)setPending:(void *)block
{
    // wake up test thread once our block gets pended
    if (block == [self undisguise:self.disguisedPointer]) {
        _blockWasPended = YES;
        [self performSelectorOnTestThread:@selector(blockWasPended)];
    }
    [super setPending:block];
}

- (void)scanBlock:(void *)block endAddress:(void *)end
{
    // our block has layout info so should not be scanned conservatively
    if (block == [self undisguise:self.disguisedPointer] && !self.scannedWithCorrectLayout) {
        [self fail:@"test block scanned without layout"];
    }
    [super scanBlock:block endAddress:end];
}

- (void)scanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map
{
    // verify the block's layout
    if (block == [self undisguise:self.disguisedPointer]) {
        if (map != self.correctLayout)
            self.scannedWithBogusLayout = YES;
        else
            self.scannedWithCorrectLayout = YES;
    }
    [super scanBlock:block endAddress:end withLayout:map];
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
        [self fail:@"test block became global unexpectedly"];
    }
    [super blockBecameGlobal:block withAge:age];
}

@end
