/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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
//  MainThreadFinalization.m
//  Copyright (c) 2009-2011 Apple Inc. All rights reserved.
//

#include "BlackBoxTest.h"
#include <objc/objc-auto.h>
#include <objc/runtime.h>
#include <dispatch/dispatch.h>

@interface MainThreadFinalizer : TestFinalizer
@end

@implementation MainThreadFinalizer

+ (void)initialize
{
    objc_finalizeOnMainThread(self);
}

@end


@interface MainThreadFinalization : BlackBoxTest
{
    BOOL _finalized;
}

@end

@implementation MainThreadFinalization

- (void)allocate
{
    // force test object out of thread local
    size_t size = class_getInstanceSize([MainThreadFinalizer class]);
    for (int i=0; i<1024*1024/size*2; i++)
        CFRelease(CFRetain([MainThreadFinalizer new]));
}

- (void)performTest
{
    _finalized = NO;
    [self allocate];
    [self clearStack];
    dispatch_sync(dispatch_get_main_queue(), ^{ 
        objc_collect(OBJC_WAIT_UNTIL_DONE|OBJC_FULL_COLLECTION); 
    } );
    if (_finalized)
        [self passed];
    else
        [self fail:@"Did not finalize main thread object"];
    [self testFinished];
}

- (void)didFinalize:(TestFinalizer *)finalizer
{
    if (!pthread_main_np())
        [self fail:@"Finalized called on non main thread"];
    _finalized = YES;
}

@end
