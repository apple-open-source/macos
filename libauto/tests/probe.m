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
//  probe.m
//  gctests
//
//  Created by Blaine Garst on 3/6/09.
//  Copyright 2009 __MyCompanyName__. All rights reserved.
//

// CONFIG -C99 GC open -lauto

/*
We add two new spis to the collector for Instruments near the end of SnowLeopard.
One is to probe to see if a pointer is an auto allocated object.
The other is to exactly walk a pointer that is a scanned object.

The is-an-auto-object is only approximate since, of course, the pointer might already be garbage at one instant and become a free node the next, and so is only appropriate for use for when GC is frozen.  Such times include during auto_zone_dump callbacks, perhaps others.  It is useful for separating wheat from chaff though.

The exact scanning interface must work for all objects regardless of whether they have detailed exact scanning knowledge or not.  Objects with all ids use an empty "conservative" layout, for example, and objects with extended memory use conservative for the extra parts.  Extended memory can be explicitly requested or can simply be due to quantum size roundup.
*/

#import <Foundation/Foundation.h>
#import <auto_zone.h>

@interface TestObject : NSObject {
@public
    long junk;
    id interesting;
    long junk2;
    id more[1]; // really at least 3
}
@end
@implementation TestObject
@end

void errorAndDie(const char *message) {
    printf("**** %s\n", message);
    exit(1);
}
 
void test() {
    TestObject *to = [[TestObject alloc] init];
    TestObject *to2 = [[TestObject alloc] init];
    to->junk = (long)to2;    // should not be seen
    to->interesting = to2;   // should be
    to->junk2 = (long)to2;   // should not be
    to->more[0] = to2;       // should be
    to->more[1] = to2;       // should also since its in the quantum slop
    to->more[2] = NULL;      // ditto
    
    // does probe interface tell us good things?
    auto_zone_t *zone = auto_zone();
    
    if (auto_zone_probe_unlocked(zone, NULL)) {
        errorAndDie("NULL seems to be an object");
    }
    if (auto_zone_probe_unlocked(zone, (void *)test)) {
        errorAndDie("function seems to be an object");
    }
    if (auto_zone_probe_unlocked(zone, *(void **)to)) {
        errorAndDie("class seems to be an object");
    }
    if ( ! auto_zone_probe_unlocked(zone, to)) {
        errorAndDie("doesn't see 'to' as an object");
    }
    
    long word = 0;
    long *pword = &word;
    
    if (0) {
        long *alias = (long *)to;
        int i;
        for (i = 0; i < 8; ++i) {
            printf("to + %d = %lx\n", i, alias[i]);
        }
        printf("[TestObject self] = %p\n", [TestObject self]);
    }
    auto_zone_scan_exact(zone, to, ^(void *base, unsigned long byte_offset, void *candidate) {
        if ((candidate != to2) && (candidate != NULL) 
            && (candidate != [TestObject self])
            ) {
            printf("scan exact base %p, offset %ld, candidate %p =?= to2 %p\n", base, byte_offset, candidate, to2);
            errorAndDie("bad candidate!");
        }
        else
            *pword = (*pword)|(1 << (byte_offset/sizeof(void *))); // remember the to2's
    });
    
    // should see
    // isa         1
    // junk        0
    // interesting 1
    // junk2       0
    // more[0]     1
    // more[1]     1    // slop scanned
    // more[2]     1    // slop scanned
    // more[3]     1    // slop scanned
    // ..............
    // 00011110101
    // 0xf5
    
    if (word != 0xf5) {
        printf("wanted 0xf5, got %lx\n", word);
        errorAndDie("didn't scan what we wanted");
    }
}

void testArray() {
    NSMutableArray *array = [NSMutableArray new];
    [array addObject:[NSObject new]];
    [array addObject:[NSObject new]];
    [array addObject:[NSObject new]];
    [array addObject:[NSObject new]];
    [array addObject:[NSObject new]];
    [array addObject:[NSObject new]];
    [array addObject:[NSObject new]];
    
    auto_zone_t *zone = auto_zone();
    // make sure its what we think it should be, a scanned object
    
    //int layout = auto_zone_get_layout_type(zone, array);
    //printf("got %d for layout, AUTO_IS_OBJECT %d, AUTO_IS_UNSCANNED %d\n", layout, AUTO_OBJECT, AUTO_UNSCANNED);
    
    // now make sure that there is a backing pointer in the GC heap
    __block int counter = 0;
    auto_zone_scan_exact(zone, array, ^(void *base, unsigned long byte_offset, void *candidate) {
        ++counter;
    });
    //printf("number of scanned elements in NSMutableArray: %d\n", counter);
    if (counter < 2) {
        errorAndDie("Hmm, not even 2 fields in an NSMutableArray");
}
    
int main(int argc, char *argv[]) {
    test();
    testArray();
    printf("%s: success\n", argv[0]);
    return 0;
}
