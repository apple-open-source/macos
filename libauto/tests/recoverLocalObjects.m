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
#import <pthread.h>
#import <objc/objc-auto.h>
#import "auto_zone.h"

// CONFIG GC -C99 -lauto

int Counter = 0;

@interface TestObject : NSObject {
    TestObject *link;
}
@property(retain) TestObject *link;
@end
@implementation TestObject
@synthesize link;
- (void)finalize {
    ++Counter;
    //printf("finalizing %p\n", self);
    [super finalize];
}
@end

int howmany = 200000;

void *doOnPthread(void *unused) {
    objc_registerThreadWithCollector();
    for (int i = 0; i < howmany; ++i) {
        [[TestObject alloc] init];
    }
    return NULL;
}


int main(int argc, char *argv[]) {
    objc_startCollectorThread();
    pthread_t thread;
    pthread_create(&thread, NULL, doOnPthread, NULL);
    pthread_join(thread, NULL);
    auto_collect(auto_zone(), AUTO_COLLECT_FULL_COLLECTION|AUTO_COLLECT_SYNCHRONOUS, NULL);
    if (Counter*2 < howmany) {
        printf("recovered %d of %d local thread only objects\n", Counter, howmany);
        return 1;
    }
    printf("%s: Success\n", argv[0]);
    return 0;
}
