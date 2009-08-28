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
// single-threaded-finalize-on-main-thread.m
//  gctests
//
//  Created by Blaine Garst on 12/4/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//
// CONFIG GC



#include <objc/objc-auto.h>
#include <stdio.h>
#include <Foundation/Foundation.h>

int Allocated = 0;
int Recovered = 0;

@interface TestObject : NSObject
@end

@implementation TestObject
- init {
   ++Allocated;
   return self;
}
- (void)finalize {
   ++Recovered;
  // printf("%p: -finalize called\n", self);
   [super finalize];
}
@end

id aGlobal;

int main(int argc, char *argv[]) {
   int i = 0;
   int howmany = 10000;
   objc_finalizeOnMainThread([TestObject self]);
   for (; i < howmany; ++i) aGlobal = [[TestObject alloc] init];
   objc_collect(OBJC_COLLECT_IF_NEEDED|OBJC_WAIT_UNTIL_DONE);
   
   if ((Recovered + 100) < Allocated) {
	printf("only recovered %d of %d items\n", Recovered, Allocated);
        return 1;
   }
   printf("%s: success\n", argv[0]);
   return 0;
}
