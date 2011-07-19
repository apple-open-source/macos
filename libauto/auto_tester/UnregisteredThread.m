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
//  UnregisteredThread.m
//  Copyright (c) 2008-2001 Apple Inc. All rights reserved.
//

#import <objc/objc-auto.h>
#import "BlackBoxTest.h"
#import "WhiteBoxTest.h"

@interface UnregisteredThread : BlackBoxTest
{
    pthread_t _thread;
    BOOL _sawWarning;
}
@end

@interface RegisteredThread : BlackBoxTest
{
    pthread_t _thread;
}
@end

static void *registerTest(UnregisteredThread *tester)
{
    @synchronized (tester) {
        auto_zone_register_thread(objc_collectableZone());
        auto_zone_assert_thread_registered(objc_collectableZone());
    }
    return NULL;
}

static void *unregisterTest(UnregisteredThread *tester)
{
    @synchronized (tester) {
        auto_zone_assert_thread_registered(objc_collectableZone());
    }
    return NULL;
}


@implementation UnregisteredThread

- (void)processOutputLine:(NSString *)line
{
    NSString *expectedString = @"error: GC operation on unregistered thread. Thread registered implicitly. Break on auto_zone_thread_registration_error() to debug.";
    NSRange r = [line rangeOfString:expectedString];
    if (r.location == NSNotFound) {
        [super processOutputLine:line];
    } else {
        _sawWarning = YES;
    }
}

- (void)performTest
{
    if (pthread_create(&_thread, NULL, (void *(*)(void *))unregisterTest, self) != 0) {
        [self fail:@"failed to create thread\n"];
        return;
    }
    pthread_join(_thread, NULL);
    [self testFinished];
}

- (void)outputComplete
{
    if (_sawWarning)
        [self passed];
    else
        [self fail:@"did not see thread registration warning"];
    [super outputComplete];
}

@end

@implementation RegisteredThread

- (void)performTest
{
    if (pthread_create(&_thread, NULL, (void *(*)(void *))registerTest, self) != 0) {
        [self fail:@"failed to create thread\n"];
        return;
    }
    pthread_join(_thread, NULL);
    [self passed];
    [self testFinished];
}

@end

