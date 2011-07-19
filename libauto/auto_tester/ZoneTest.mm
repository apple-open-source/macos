//
//  ZoneTest.mm
//  auto
//
//  Created by Patrick Beard on 11/1/10.
//  Copyright 2010 Apple Inc. All rights reserved.
//

#import "Zone.h"
#import "BlackBoxTest.h"

using namespace Auto;

@interface ZoneTest : BlackBoxTest
@end

@implementation ZoneTest

- (void)performTest {
    Zone *zone = (Zone *)[self auto_zone];
    Thread &thread = zone->register_thread();
    
    void *large_block = zone->block_allocate(thread, 4 * allocate_quantum_large, AUTO_MEMORY_UNSCANNED, false, false);
    Large *large = zone->block_start_large(large_block);
    if (!large) {
        [self fail:@"Zone::block_start_large() failed."];
        [self testFinished];
        return;
    }
    
    bool in_large_bitmap = zone->in_large_bitmap(large_block); 
    bool in_large_memory = zone->in_large_memory(large_block); 
    if (!in_large_bitmap || !in_large_memory) {
        [self fail:@"large_block should be in large memory!"];
        [self testFinished];
        return;
    }
    
    void *non_block = displace(large_block, large->vm_size());
    Large *non_large = zone->block_start_large(non_block);
    if (non_large == large) {
        [self fail:@"non_block should NOT be part of large!"];
        [self testFinished];
        return;
    }
    
    [self setTestResult:PASSED message:@"zone tests passed."];
    [self testFinished];
}

@end
