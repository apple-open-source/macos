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
#import <objc/runtime.h>
#import <objc/objc-auto.h>

// CONFIG GC -C99 -lauto

static BOOL finalized = NO;

@interface FinalizingObject : NSObject
- (void)finalize;
@end

@implementation FinalizingObject
- (void)finalize {
    finalized = YES;
    [super finalize];
}
@end

static int uniqueKey;

static id createReferences() {
    // <rdar://problem/6463922>
    id object = @"Constant Object";
    id value = [FinalizingObject new];
    objc_setAssociatedObject(object, &uniqueKey, value, OBJC_ASSOCIATION_ASSIGN);
    return object;
}

static void breakReferences(id object) {
    objc_setAssociatedObject(object, &uniqueKey, nil, OBJC_ASSOCIATION_ASSIGN);
}

int main(int argc, char *argv[]) {
    id object = createReferences();
    [[NSGarbageCollector defaultCollector] collectExhaustively];
    if (finalized) {
        printf("Failure\n");
        return 1;
    }
    breakReferences(object);
    [[NSGarbageCollector defaultCollector] collectExhaustively];
    if (finalized) { 
        printf("%s: Success\n", argv[0]);
        return 0;
    } else {
        printf("Failure\n");
        return 2;
    }
}
