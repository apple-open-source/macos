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

// CONFIG open GC -C99 runtime: objc_assign_global_error

// store thread local and global objects into global variable

@interface TestObject : NSObject {
@public
    id string;
    bool isGlobal;
}
- (void)makeGlobal;
@end

TestObject *target;

TestObject *global;
int localDidFinalize = 0;
int globalDidFinalize = 0;
int Errors;

@implementation TestObject 

- (void)finalize {
    if (isGlobal) {
        static int counter = 0;
        if (counter++ == 0) {
            target = self;
            globalDidFinalize = 1;
        }
    }
    else  {
        static int counter = 0;
        if (counter++ == 0) {
            target = self;
            localDidFinalize = 1;
        }
    }
    [super finalize];
}
- (void)makeGlobal {
    [[NSGarbageCollector defaultCollector] disableCollectorForPointer:self];
    [[NSGarbageCollector defaultCollector] enableCollectorForPointer:self];
    isGlobal = true;
}
@end

#define LOCALOBJECTS 0

int main(int argc, char *argv[]) {
    global = [[TestObject alloc] init];

#if LOCALOBJECTS    
    for (int i = 0; i < 101; ++i) [[TestObject alloc] init];
    [[NSGarbageCollector defaultCollector] collectIfNeeded];
    if (localDidFinalize == 0) {
        printf("%s: Whoops, no locals were finalized!!\n", argv[0]);
        ++Errors;
    }

#else    
    for (int i = 0; i < 101; ++i) [[[TestObject alloc] init] makeGlobal];
    [[NSGarbageCollector defaultCollector] collectExhaustively];
    if (globalDidFinalize == 0) {
        printf("%s: Whoops, no globals were finalized!!\n", argv[0]);
        ++Errors;
    }
#endif

    if (Errors == 0) {
        printf("%s: Success!\n", argv[0]);
    }
    return Errors;
    
}

