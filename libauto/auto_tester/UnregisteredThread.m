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
//  UnregisteredThread.m
//  auto
//
//  Created by Josh Behnke on 7/31/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import "UnregisteredThread.h"


@implementation UnregisteredThread

static void *registerTest(UnregisteredThread *tester)
{
    @synchronized (tester) {
        auto_zone_register_thread([tester auto_zone]);
        auto_zone_assert_thread_registered([tester auto_zone]);
        auto_zone_unregister_thread([tester auto_zone]);
        [tester->_synchronizer signal];
    }
    return NULL;
}

static void *unregisterTest(UnregisteredThread *tester)
{
    @synchronized (tester) {
        printf("Should see an error message about an unregistered thread here:\n");
        auto_zone_assert_thread_registered([tester auto_zone]);
        [tester->_synchronizer signal];
    }
    return NULL;
}

- (void)startTest
{
    _synchronizer = [self setNextTestSelector:@selector(verifyUnregistered)];
    _probeTriggered = NO;
    if (pthread_create(&_testThread, NULL, (void *(*)(void *))unregisterTest, self) != 0) {
        [self fail:"failed to create thread\n"];
        return;
    }
}

- (void)verifyUnregistered
{
    pthread_join(_testThread, NULL);
    if (!_probeTriggered) {
        [self fail:"unregistered thread error failed to trigger"];
    }
    
    _synchronizer = [self setNextTestSelector:@selector(verifyRegistered)];    
    _probeTriggered = NO;
    if (pthread_create(&_testThread, NULL, (void *(*)(void *))registerTest, self) != 0) {
        [self fail:"failed to create thread\n"];
        return;
    }
}

- (void)verifyRegistered
{
    pthread_join(_testThread, NULL);
    if (_probeTriggered) {
        [self fail:"unregistered thread error triggered on registered thread"];
    }
}

- (void)threadRegistrationError
{
    _probeTriggered = YES;
}

@end
