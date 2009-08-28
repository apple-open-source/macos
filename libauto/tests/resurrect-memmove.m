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
//  resurrect-memmove.m
//  gctests
//
//  Created by Blaine Garst on 4/8/09.
//  Copyright 2009 Apple, Inc. All rights reserved.
//

// CONFIG open -C99 GC rdar://6767967

#import <Foundation/Foundation.h>
#import <objc/objc-auto.h>


@interface TestObject : NSObject {
@public
    id deadOnes[12];
}
@end

TestObject *Alive;
int Collected;

@implementation TestObject


- init {
    for (int i = 0; i < 12; ++i)
        deadOnes[i] = [NSObject new];
    return self;
}

- (void)finalize {
    // should not croak cause it could be just a bit pattern that looks like a garbage object
    objc_memmove_collectable(&Alive->deadOnes, deadOnes, 12*sizeof(id));
    ++Collected;
    [super finalize];
}

@end

void doTest() {
    Alive = [TestObject new];
    for (int i = 0; i < 1000; ++i)
        [TestObject new];
    [[NSGarbageCollector defaultCollector] collectIfNeeded];
    [[NSGarbageCollector defaultCollector] collectExhaustively];
}

int main(int argc, char *argv[]) {
    doTest();
    if (Collected == 0) {
        printf("**** did not collect anything!\n");
        return 1;
    }
    printf("%s: Success\n", argv[0]);
    return 0;
}
