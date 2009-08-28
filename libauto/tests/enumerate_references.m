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
#import <auto_zone.h>

// CONFIG GC -C99 -lauto

@interface TestObject : NSObject {
    TestObject *link;
}
@property(retain) TestObject *link;
@end
@implementation TestObject
@synthesize link;
- (void)dealloc {
    [link release];
    [super dealloc];
}
@end

int Counter = 0;
NSMutableArray *Referers;
int Errors = 0;

void callback(auto_zone_t *zone, void *ctx, auto_reference_t reference) {
   ++Counter;
   if (![Referers containsObject:(id)reference.referrer_base]) {
        ++Errors;
        printf("didn't find referrer %p in array\n", (id)reference.referent);
    }
    //printf("callback zone %p, ctx %p, reference %p, referrer %p, offset %x\n",
    //zone, ctx, reference.referent, reference.referrer_base, reference.referrer_offset);
}

int testResults() {
    TestObject *to = [[TestObject alloc] init];
    //printf("reference: %p\nby ", to);
    // set up an array with 10 objects that refer to "to"
    Referers = [[NSMutableArray alloc] init];
    for (int i = 0; i < 10; ++i) {
        TestObject *two = [[TestObject alloc] init];
        two.link = to;
        [Referers addObject:two];
        [two release];
        //printf(" %p", two);
    }
    //printf("\n");
    // now enumerate the heap looking for referents
    int counter = 0;
    auto_enumerate_references(
        (auto_zone_t *)[[NSGarbageCollector defaultCollector] zone],
        to,
        callback,
        __builtin_frame_address(0), // stackbottom
        NULL);     // ctx
    if (Counter != 10) {
        printf("wanted 10 but only found %d referrers\n", Counter);
        return 1;
    }
    return Errors;
}


int main(int argc, char *argv[]) {
    int result = testResults();
    if (result == 0) printf("%s: Success\n", argv[0]);
    else printf("Failure\n");
    return result;
}