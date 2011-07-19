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
//  badpointers.m
//  Copyright (c) 2008-2011 Apple Inc. All rights reserved.
//
// CONFIG GC -C99 -lauto

#import "BlackBoxTest.h"

@interface BadPointers : BlackBoxTest
@end

@implementation BadPointers

- (NSString *)shouldSkip
{
#warning this test isn't working right
    return @"this test isn't working right";
}

- (void)testBadPointer:(void *)badPointer 
{
    auto_zone_t *the_zone = [self auto_zone];
    //printf("auto_zone_retain\n");
    auto_zone_retain(the_zone, badPointer);
    //printf("auto_zone_release\n");
    auto_zone_release(the_zone, badPointer);
    //printf("auto_zone_retain_count\n");
    auto_zone_retain_count(the_zone, badPointer);
    //printf("auto_zone_is_finalized\n");
    auto_zone_is_finalized(the_zone, badPointer);
    //printf("auto_zone_set_unscanned\n");
    auto_zone_set_unscanned(the_zone, badPointer);
    //printf("auto_zone_create_copy\n");
    auto_zone_create_copy(the_zone, badPointer);
    //printf("auto_zone_is_valid_pointer\n");
    auto_zone_is_valid_pointer(the_zone, badPointer);
    //printf("auto_zone_size\n");
    auto_zone_size(the_zone, badPointer);
    //printf("auto_zone_base_pointer\n");
    auto_zone_base_pointer(the_zone, badPointer);
    //printf("auto_zone_set_write_barrier\n");
    auto_zone_set_write_barrier(the_zone, badPointer, badPointer);
}

#if defined(__x86_64__)
#define SUBZONE ((1 << 21)-1)
#else
#define SUBZONE ((1 << 20)-1)
#endif

- (void)performTest {
    auto_zone_t *the_zone = [self auto_zone];
    // small values
    uintptr_t ptr = (long)auto_zone_allocate_object(the_zone, 24, AUTO_MEMORY_UNSCANNED, 0, 0);
    uintptr_t badPointer = (ptr & ~(ptr & SUBZONE)) + sizeof(void *);
    int i;
    //printf("ptr %lx bad ptr %lx\n", ptr, badPointer);
    for (i = 0; i < 32; ++i) {
        [self testBadPointer:(void *)(i*sizeof(void *)+badPointer)];
    }
    
    // large values
    uintptr_t interiors[64];
    uintptr_t size = 64*64*1024;  // 64 (32) quantums big
    ptr = (long)auto_zone_allocate_object(the_zone, 24, AUTO_MEMORY_UNSCANNED, 0, 0);
    uintptr_t base = ptr & ~(64*1024-1);
    uintptr_t offset = ptr - base;
    for (int i = 1; i < 64; ++i) {
        // at every 64K address boundary
        uintptr_t bad_address = ptr + i*64*1024;
        uintptr_t *fake_Large = (uintptr_t *)(bad_address - offset);
        for (int j = 0; j < 10; ++j)
            fake_Large[j] = -1;
        
        interiors[i-1] = bad_address;
    }
    for (int i = 0; i < 63; ++i)
        [self testBadPointer:(void *)interiors[i]];
    
    // if we didn't crash then we passed!
    [self passed];        
    [self testFinished];
}

@end    