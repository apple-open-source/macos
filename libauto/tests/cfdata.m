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
//  cfdata.m
//  gctests
//
//  Created by Blaine Garst on 11/13/08.
//  Copyright 2008 Apple. All rights reserved.
//

// CONFIG GC -C99


#import <Foundation/Foundation.h>
#import <CoreFoundation/CFData.h>
#include <malloc/malloc.h>


@interface TestObject : NSObject {
    __strong CFDataRef  subject;
    __strong CFMutableDataRef mutableSubject;
    
    __strong CFDataRef subjectCopy;
    __strong CFMutableDataRef mutableSubjectCopy;
    
    __strong CFMutableDataRef subjectMutableCopy;
    __strong CFMutableDataRef mutableSubjectMutableCopy;
    
}

- init;

@end

@interface TestTestObject : TestObject {
    bool shouldDealloc;
    __strong void *backingData;
}
@end


@implementation TestObject
- (void) makeCopies {
    CFIndex size = 32*sizeof(void *);
    subjectCopy         = CFDataCreateCopy(NULL, subject);
    mutableSubjectCopy  = CFDataCreateMutableCopy(NULL, size, mutableSubject);
    
    subjectMutableCopy          = CFDataCreateMutableCopy(NULL, size, subject);
    mutableSubjectMutableCopy   = CFDataCreateMutableCopy(NULL, size, mutableSubject);
}

- (void)makeCollectable {
    CFMakeCollectable(subjectCopy);
    CFMakeCollectable(mutableSubjectCopy);
    CFMakeCollectable(subjectMutableCopy);
    CFMakeCollectable(mutableSubjectMutableCopy);
    CFMakeCollectable(subject);
    CFMakeCollectable(mutableSubject);
}

- (void)grow {
    CFIndex size = 32*sizeof(void *);
    CFDataSetLength(mutableSubjectCopy, 2*size);
    CFDataSetLength(subjectMutableCopy, 2*size);
    CFDataSetLength(mutableSubjectMutableCopy, 2*size);
    CFDataSetLength(mutableSubject, 2*size);
}

- (void)dealloc {
    CFRelease(subjectCopy);
    CFRelease(mutableSubjectCopy);
    CFRelease(subjectMutableCopy);
    CFRelease(mutableSubjectMutableCopy);
    CFRelease(subject);
    CFRelease(mutableSubject);
    [super dealloc];
}

- init {
    id buffer[32];
    for (int i = 0; i < 32; ++i)
        buffer[i] = self;
    CFIndex size = sizeof(void *)*32;
    if (!subject) subject         = CFDataCreate(NULL, (uint8_t *)buffer, size);
    mutableSubject  = CFDataCreateMutable(NULL, size);
    const UInt8 *bytes  = CFDataGetBytePtr((CFMutableDataRef)mutableSubject);
    memcpy((void *)bytes, buffer, size);
    
    [self makeCopies];
    [self makeCollectable];
    [self grow];
    return self;
}


bool allSame(void *value, void **items, int nitems) {
    for (int i = 0; i < nitems; ++i)
        if (items[i] != value) return false;
    return true;
}

- (void)test {
    bool result = true;
    CFIndex count = CFDataGetLength(subject)/sizeof(void *);
    result = result && allSame(self, (void **)CFDataGetBytePtr(subject), count);
    result = result && allSame(self, (void **)CFDataGetBytePtr(mutableSubject), count);
    result = result && allSame(self, (void **)CFDataGetBytePtr(subjectCopy), count);
    result = result && allSame(self, (void **)CFDataGetBytePtr(mutableSubjectCopy), count);
    result = result && allSame(self, (void **)CFDataGetBytePtr(subjectMutableCopy), count);
    result = result && allSame(self, (void **)CFDataGetBytePtr(mutableSubjectMutableCopy), count);
    if (!result) {
        printf("Data changed!\n");
        exit(1);
    }
}

@end

@implementation TestTestObject

- (void)allocateBacking:(CFIndex)size {
    backingData = malloc_zone_malloc((malloc_zone_t*)NSDefaultMallocZone(), size);
}

- init {
    id buffer[32];
    for (int i = 0; i < 32; ++i)
        buffer[i] = self;
    CFIndex size = sizeof(void *)*32;
    [self allocateBacking:size];
    
    memcpy(backingData, buffer, size);
    subject         = CFDataCreateWithBytesNoCopy(NULL, (const UInt8*)backingData, size, NULL);
    //if ([NSGarbageCollector defaultCollector]) printf("backing data %p held by %p\n", backingData, subject);
    return [super init];
}
- initExplicit {
    id buffer[32];
    for (int i = 0; i < 32; ++i)
        buffer[i] = self;
    CFIndex size = sizeof(void *)*32;
    [self allocateBacking:size];
    memcpy(backingData, buffer, size);
    // explicitly use kCFAllocatorDefault
    subject         = CFDataCreateWithBytesNoCopy(NULL, (const UInt8*)backingData, size, kCFAllocatorDefault);
    //if ([NSGarbageCollector defaultCollector]) printf("backing data %p held by %p\n", backingData, subject);
    return [super init];
}
- initOwned {
    id buffer[32];
    for (int i = 0; i < 32; ++i)
        buffer[i] = self;
    CFIndex size = sizeof(void *)*32;
    [self allocateBacking:size];
    memcpy(backingData, buffer, size);
    // explicitly use kCFAllocatorNull to say that the memory is owned elsewhere
    subject         = CFDataCreateWithBytesNoCopy(NULL, (const UInt8*)backingData, size, kCFAllocatorNull);
    shouldDealloc = true;
    //if ([NSGarbageCollector defaultCollector]) printf("backing data %p held by %p, will dealloc\n", backingData, subject);
    return [super init];
}
- (void) dealloc {
    if (shouldDealloc) free(backingData);
    [super dealloc];
}
- (void) finalize {
    if (shouldDealloc) free(backingData);
    [super finalize];
}
@end

#define LOOPS 100

void testLoop() {
    NSMutableArray *array = [[NSMutableArray alloc] init];

    for (int i = 0; i < LOOPS; ++i) {
        TestObject *to = [[TestObject alloc] init];
        [array addObject:to];
        [to release];
    }
    for (int i = 0; i < LOOPS; ++i) {
        TestTestObject *to = [[TestTestObject alloc] init];
        [array addObject:to];
        [to release];
    }

    for (int i = 0; i < LOOPS; ++i) {
        TestTestObject *to = [[TestTestObject alloc] initExplicit];
        [array addObject:to];
        [to release];
    }

    for (int i = 0; i < LOOPS; ++i) {
        TestTestObject *to = [[TestTestObject alloc] initOwned];
        [array addObject:to];
        [to release];
    }

    [array release];
    NSGarbageCollector *collector = [NSGarbageCollector defaultCollector];
    [collector collectIfNeeded];
    [collector collectExhaustively];
}


int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [NSAutoreleasePool new];
    for (int i = 0; i < (1+LOOPS); ++i) {   // just enough for GC to not find things on the stack
        testLoop();
        //printf("iteration %d done\n", i);
    }
    [pool drain];
    printf("%s: Success\n", argv[0]);
    return 0;
}
