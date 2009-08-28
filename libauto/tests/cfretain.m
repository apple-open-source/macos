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
#import <Foundation/Foundation.h>
#import <objc/objc-auto.h>

// CONFIG GC -C99

@interface TestObject : NSObject {
    NSString *string;
}
- (void)makeGlobal;
@end

id global;

@implementation TestObject

- init {
    string = [@"string" mutableCopy];
    return self;
}

- (void)finalize {
    for (int i = 0; i < 10; ++i) {
        CFRetain((CFTypeRef)self);
    }
    for (int i = 0; i < 10; ++i) {
        CFRelease((CFTypeRef)self);
    }
    for (int i = 0; i < 10; ++i) {
        CFRetain((CFTypeRef)string);
    }
    for (int i = 0; i < 10; ++i) {
        CFRelease((CFTypeRef)string);
    }
    [super finalize];
}
- (void)makeGlobal {
    global = self;
    global = nil;
}

@end

int Errors = 0;

#import "lookfor.c"

void generic(const char *name) {
    dup2tmpfile(name);
    NSObject *string = (NSObject *)CFStringCreateCopy(NULL,(CFStringRef)@"hello");
    for (int i = 0; i < 101; ++i) {
        CFRetain((CFTypeRef)string);
    }
    for (int i = 0; i < 101; ++i) {
        CFRelease((CFTypeRef)string);
    }
    CFRelease(string);
    if (!lookfor("")) {
        printf("*** some kind of CFRetain/CFRelease error occurred\n");
        ++Errors;
    }
}
void fromfinalize(const char *name) {
    dup2tmpfile(name);
    for (int i = 0; i < 101; ++i) {
        TestObject *to = [[TestObject alloc] init];
    }
    for (int i = 0; i < 0; ++i) {
        TestObject *to = [[TestObject alloc] init];
        [to makeGlobal];
    }
    [[NSGarbageCollector defaultCollector] collectIfNeeded];
    [[NSGarbageCollector defaultCollector] collectExhaustively];
    if (!lookfor("")) {
        printf("**** some kind of CFRetain/CFRelease error occurred while used in finalize\n");
        ++Errors;
    }
}

int main(int argc, char *argv[]) {
    objc_startCollectorThread();
    // make sure generic CF style objects can CFRetain/CFRelease
    generic(argv[0]);
    fromfinalize(argv[0]);
    if (Errors == 0) {
        printf("%s: Success!\n", argv[0]);
        unlink(tmpfilename);
    }
    else {
        printf("%s: look in %s for details\n", argv[0], tmpfilename);
    }
    return Errors;
}
    
    
    
