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
//  auto_realloc_test.m
//  gctests
//
//  Created by Blaine Garst on 2/23/09.
//  Copyright 2009 Apple. All rights reserved.
//


// CONFIG GC   -C99 rdar://6593098

#import <Foundation/Foundation.h>
#import <objc/objc-auto.h>
#import <malloc/malloc.h>
//#import </usr/local/include/auto_zone.h>

#if 0
typedef struct malloc_statistics_t {
    unsigned	blocks_in_use;
    size_t	size_in_use;
    size_t	max_size_in_use;	/* high water mark of touched memory */
    size_t	size_allocated;		/* reserved in memory */
} malloc_statistics_t;
#endif

void doTest() {
    malloc_zone_t *zone = (malloc_zone_t *)NSDefaultMallocZone();
    for (int i = 0; i < 1000; ++i) {
        char *buffer = (char *)malloc_zone_malloc(zone, 1024);
        char *old_buffer = buffer;
        buffer = (char *)malloc_zone_realloc(zone, buffer, 512);
        free(buffer);
    }
}

int iterationsTillStable() {
    int iterationsSinceMaxChanged = 0;
    int iterationCount = 0;
    int max_blocks_in_use = 0;
    while (iterationsSinceMaxChanged < 5) {
        doTest();
        objc_collect(OBJC_EXHAUSTIVE_COLLECTION | OBJC_WAIT_UNTIL_DONE);
        malloc_statistics_t stats;
        malloc_zone_statistics((malloc_zone_t *)NSDefaultMallocZone(), &stats);
        if (stats.blocks_in_use > max_blocks_in_use) {
            max_blocks_in_use = stats.blocks_in_use;
            iterationsSinceMaxChanged = 0;
        }
        else {
            ++iterationsSinceMaxChanged;
        }
        if (++iterationCount > 200) {
            printf("blocks in use (%d) not stabilizing! quitting after %d iterations\n", max_blocks_in_use, iterationCount);
            return -1;
        }
    }
    return iterationCount;
}

int main(int argc, char *argv[]) {
    int iterations;
    objc_startCollectorThread();
    iterations = iterationsTillStable();
    if (iterations > 0) {
        printf("%s: success at %d\n", argv[0], iterations);
        return 0;
    }
    return 1;
}