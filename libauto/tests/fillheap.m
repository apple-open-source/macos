/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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

// CONFIG GC -C99 -lauto

#include <auto_zone.h>
#include <stdio.h>

#define BufferLength (1 << 20)
static auto_zone_t* gc_zone;

static void init_gc_zone(void) {
    gc_zone = auto_zone_create("test collected zone");
    auto_zone_register_thread(gc_zone);
}


int main(int argc, char *argv[]) {
    init_gc_zone();
    
    char* buffer;    
    //printf("Starting\n");
    while (buffer = (char*)auto_zone_allocate_object(gc_zone, (size_t)BufferLength, AUTO_OBJECT_UNSCANNED, 1, 0)) {
        //printf("Allocatted buffer %p\n", buffer);
        buffer[BufferLength - 1] = 1;  // This crashes on the last allocation without the fix for radar://7594903 
    }

    printf("%s: success\n", argv[0]);
    return 0;
}
