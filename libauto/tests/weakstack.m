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
//  weakstack.m
//  gctests
//
//  Created by Blaine Garst on 11/3/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//


// CONFIG GC -C99

#import <Foundation/Foundation.h>
#import <objc/objc-auto.h>

enum {
    magic = 10
};

void doCollect() {
    int array[magic];
    for (int i = 0; i < magic; ++i)
        array[i] = -1;
    NSGarbageCollector *collector = [NSGarbageCollector defaultCollector];
    [collector collectIfNeeded];
    [collector collectExhaustively];
    for (int i = 0; i < magic; ++i) {
        if (array[i] != -1) {
            printf("stack was cleared!!!\n");
            exit(1);
        }
    }
}

void registerStackLocation(int i) {
    int array[i];
    id x = nil;
    //printf("address of x is %p\n", x);
    objc_assign_weak([NSObject new], &x);   // allowed, but broken
}

void testFunction() {
    for (int i = 0; i < magic; ++i) {
        registerStackLocation(i);
    }
    doCollect();
}

int main(int argc, char *argv[]) {
    testFunction();
    printf("%s: success\n", argv[0]);
    return 0;
}



