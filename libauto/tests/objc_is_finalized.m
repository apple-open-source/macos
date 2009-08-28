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

id global;
int Errors = 0;
int Locals = 0;
int Globals = 0;

@interface Collectable : NSObject {
    bool isGlobal;
}
- (void)makeGlobal;
@end

@implementation Collectable
- (void)makeGlobal {
    global = self;
    global = nil;
    isGlobal = true;
}

- (void)finalize {
    if (!objc_is_finalized(self)) {
        NSLog(@"not known to be finalized!! %p", self);
        ++Errors;
    }
    else if (isGlobal)
        ++Globals;
    else
        ++Locals;
    [super finalize];
}
@end
        

int main(int argc, char *argv[]) {
    objc_startCollectorThread();
    const int howmany = 20000;
    
    for (int i = 0; i < howmany; ++i)
        [[Collectable alloc] init];
    for (int i = 0; i < howmany; ++i)
        [[[Collectable alloc] init] makeGlobal];
    objc_collect(OBJC_EXHAUSTIVE_COLLECTION | OBJC_WAIT_UNTIL_DONE);
    //sleep(3);   // hack to try to avoid thread reaping bug
    if (Globals == 0) {
        printf("%s: *** didn't collect any Globals!\n", argv[0]);
        return 1;
    }
    if (Locals == 0) {
        printf("%s: *** didn't collect any Locals!\n", argv[0]);
        return 1;
    }
    if (Errors) {
        printf("%s: **** finalizer didn't check out\n", argv[0]);
        return 1;
    }
    if (argc > 2) printf("Locals %d/%d, Globals %d/%d, failures %d\n", Locals, howmany, Globals, howmany, Errors);
    printf("%s: Success!\n", argv[0]);
    return 0;
}