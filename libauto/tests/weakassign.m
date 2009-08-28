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

// CONFIG GC -C99

bool doGarbage = false;
bool doKeeper = false;
int finalized = 0;

@interface TestObject : NSObject {
    __weak id weakObject;
    id strongObject;
    id keeperObject;
}
- (void)setWeak:(id)another;
- (void)setStrong:(id)another;
- (void)setKeeper:(id)another;
@end

@implementation TestObject

- (void)setWeak:(id)another {
    weakObject = another;
}
- (void)setStrong:(id)another {
    strongObject = another;
}
- (void)setKeeper:(id)another {
    keeperObject = another;
}

- (void)finalize {
    if (doGarbage) {
        weakObject = strongObject; // garbage object
        if (!weakObject) {
            printf("weak object has no strong value!\n");
            exit(1);
        }
    }
    if (doKeeper) {
        weakObject = keeperObject; // real object
        if (!weakObject) {
            printf("weak object has no keepr value!\n");
            exit(1);
        }
    }
    [super finalize];
    ++finalized;
}
@end


void test1() {
    NSMutableSet *set = [NSMutableSet new];
    for (int i = 0; i < 2000; ++i) {
        id object = [NSObject new];
        [set addObject:object];
        TestObject *to1 = [TestObject new];
        TestObject *to2 = [TestObject new];
        [to1 setStrong:to2];
        [to2 setStrong:to1];
        [to1 setKeeper:object];
        [to2 setKeeper:object];
    }
    [[NSGarbageCollector defaultCollector] collectIfNeeded];
    if (finalized == 0) {
        printf("Whoops, nothing local collected\n");
        exit(1);
    }
}

void testall() {
    doGarbage = true;
    test1();
    doGarbage = false;
    doKeeper = true;
    test1();
}

int main(int argc, char *argv[]) {
    testall();
    //printf("%d objects collected\n", finalized);

    printf("%s: Success!\n", argv[0]);
    return 0;
}