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
//  slop.m
//  gctests
//
//  Created by Blaine Garst on 10/21/08.
//  Copyright 2008-2009 Apple, Inc. All rights reserved.
//
// CONFIG GC -C99

#import <Foundation/Foundation.h>

@interface NotSoSmall : NSObject {
    // 4 items per quantum, 3 quantum in TLC
    long junk[12]; // overlap into next quantum by 1
}
@end

@interface Bigger : NotSoSmall {
    long junk2[3];  // just enough to flesh out allocation
}
@end

@implementation NotSoSmall
- init {
    if (junk[13]) {
        printf("found non-zero junk!\n");
        exit(1);
    }
    return self;
}

@end
@implementation Bigger

- init {
    [super init];
    junk2[0] = 0x2342341;
    junk2[1] = 0x2322232;
    junk2[2] = 0x2342343;  // if these were objects they would leak when they came back as NotSoSmall
    return self;
}

@end


void test() {
    NSGarbageCollector *collector = [NSGarbageCollector defaultCollector];
    for (int i = 0; i < 30; ++i) {
        // allocate a bunch of Bigger
        for (int j = 0; j < 300; ++j) {
            [[Bigger alloc] init];
        }
        [collector collectExhaustively];
        for (int j = 0; j < 300; ++j) {
            [[NotSoSmall alloc] init];
        }
        [collector collectExhaustively];
    }
}

int main(int argc, char *argv[]) {
    test();
    printf("%s: success\n", argv[0]);
    return 0;
}