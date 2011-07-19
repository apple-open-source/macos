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
//  threadedresurrection.m
//  gctests
//
//  Created by Blaine Garst on 10/20/08.
//  Copyright 2008 Apple. All rights reserved.
//
// CONFIG open GC -C99 runtime: auto_zone_resurrection_error



#import <Foundation/Foundation.h>
#import <pthread.h>
#import <objc/objc-auto.h>


NSMutableSet *GlobalSet = nil;
bool useThread = false;
int Allocated = 0;
int Reclaimed = 0;

void *helper(void *arg) {
    objc_registerThreadWithCollector();
    [GlobalSet addObject:(id)arg];
    // do some mythical processing
    [GlobalSet removeObject:(id)arg];
    objc_unregisterThreadWithCollector();
    return arg;
}

@interface TestObject : NSObject
@end

@implementation TestObject : NSObject

- init {
    self = [super init];
    ++Allocated;
    return self;
}

- (void)finalize {
    ++Reclaimed;
    if (!useThread) {
        helper(self);
        [super finalize];
        return;
    }
    pthread_t pthread;
    pthread_create(&pthread, NULL, helper, self);
    pthread_join(pthread, NULL);
    [super finalize];
}

@end

void doTest(bool doCheat) {
    useThread = doCheat;
    TestObject *to = [[TestObject alloc] init];
    to = nil;
    for (int i = 0; i < 300; ++i)
        [NSObject new]; // give TLC something to do;
    
    // make sure
    [[NSGarbageCollector defaultCollector] collectIfNeeded];
    [[NSGarbageCollector defaultCollector] collectExhaustively];
}

int main(int argc, char *argv[]) {
    GlobalSet = [NSMutableSet new];
    //printf("will use thread...\n");
    doTest(true);
    //printf("will do on collector thread...\n");
    doTest(false);
    printf("%d allocated, %d collected\n", Allocated, Reclaimed);
    printf("%s: Success\n", argv[0]);
    return 0;
}
    