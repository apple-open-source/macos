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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <malloc/malloc.h>
#include <auto_zone.h>

struct BlockRecorderContext {
    size_t blocks_in_use;
    size_t size_in_use;
    malloc_zone_t *zone;
};
typedef struct BlockRecorderContext BlockRecorderContext;

static void blockRecorder(task_t task, void *context, unsigned type, vm_range_t *ranges, unsigned count) {
    BlockRecorderContext *blockContext = context;
    while (count--) {
        blockContext->blocks_in_use++;
        // <rdar://problem/4574925> scalable malloc shouldn't count the zone as part of the allocated block space.
        if (ranges->address != (vm_address_t)blockContext->zone) blockContext->size_in_use += ranges->size;
        ++ranges;
    }
}

void test_introspection(malloc_zone_t *zone) {
    static void* pointers[1000];
    BlockRecorderContext context = { 0, 0, zone };
    malloc_statistics_t stats;
    size_t i;

    // allocate 1000 randomly sized blocks.
    for (i = 0; i < 1000; ++i) {
        size_t random_size = (random() & 0x1FFFF);
        pointers[i] = malloc_zone_malloc(zone, random_size);
    }
    
    // deallocate a random number of the blocks.
    for (i = 0; i < 1000; ++i) {
        if (random() & 0x1) malloc_zone_free(zone, pointers[i]);
    }
    
    // validate that these agree with the values returned by the enumeration APIs.
    zone->introspect->enumerator(mach_task_self(), &context, MALLOC_PTR_IN_USE_RANGE_TYPE, (vm_address_t)zone, NULL, blockRecorder);
    malloc_zone_statistics(zone, &stats);
    assert(stats.blocks_in_use == context.blocks_in_use);
    assert(stats.size_in_use == context.size_in_use);
}

int main() {
    test_introspection(auto_zone());
    test_introspection(malloc_default_zone());
    test_introspection(malloc_create_zone(8192, 0));
}
