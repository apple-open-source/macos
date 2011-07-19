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
//  badpointers.m
//  gctests
//
//  Created by Blaine Garst on 11/11/08.
//  Copyright 2008 Apple. All rights reserved.
//
// CONFIG GC -C99 -lauto

#include <auto_zone.h>
#include <stdio.h>



void testComplicated(auto_zone_t *the_zone, void *badPointer) {
/*
boolean_t auto_zone_atomicCompareAndSwap(auto_zone_t *zone, void *existingValue, void *newValue, void *volatile *location, boolean_t isGlobal, boolean_t issueBarrier);
    // Atomically update a location with a new GC value.  These use OSAtomicCompareAndSwapPtr{Barrier} with appropriate write-barrier interlocking logic.

boolean_t auto_zone_atomicCompareAndSwapPtr(auto_zone_t *zone, void *existingValue, void *newValue, void *volatile *location, boolean_t issueBarrier);
    // Atomically update a location with a new GC value.  These use OSAtomicCompareAndSwapPtr{Barrier} with appropriate write-barrier interlocking logic.
    // This version checks location, and if it points into global storage, registers a root.

extern void *auto_zone_write_barrier_memmove(auto_zone_t *zone, void *dst, const void *src, size_t size);
*/
}

void testBadPointer(auto_zone_t *the_zone, void *badPointer) {
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

void construct_and_test() {
    auto_zone_t *the_zone = auto_zone();
    // small values
    long ptr = (long)auto_zone_allocate_object(the_zone, 24, AUTO_MEMORY_UNSCANNED, 0, 0);
    long badPointer = (ptr & ~(ptr & SUBZONE)) + sizeof(void *);
    int i;
    //printf("ptr %lx bad ptr %lx\n", ptr, badPointer);
    for (i = 0; i < 32; ++i) {
        testBadPointer(the_zone, (void *)(i*sizeof(void *)+badPointer));
    }
    
    // large values
    long interiors[64];
    long size = 64*64*1024;  // 64 (32) quantums big
    ptr = (long)auto_zone_allocate_object(the_zone, 24, AUTO_MEMORY_UNSCANNED, 0, 0);
    long base = ptr & ~(64*1024-1);
    long offset = ptr - base;
    for (int i = 1; i < 64; ++i) {
        // at every 64K address boundary
        long bad_address = ptr + i*64*1024;
        long *fake_Large = (long *)(bad_address - offset);
        for (int j = 0; j < 10; ++j)
            fake_Large[j] = -1;
        
        interiors[i-1] = bad_address;
    }
    for (int i = 0; i < 63; ++i)
        testBadPointer(the_zone, (void *)interiors[i]);
        
}

int main(int argc, char *argv[]) {
    construct_and_test();
    printf("%s: Success\n", argv[0]);
    return 0;
}
    
    
