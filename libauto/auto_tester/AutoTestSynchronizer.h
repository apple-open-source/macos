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
//  AutoTestSynchronizer.h
//  auto
//
//  Created by Josh Behnke on 6/2/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <pthread.h>

/*
 AutoTestSynchronizer is used to synchronize threads running a test case, and also to store miscellaneous supporting data. Refer to AutoTestScript.h for more.
 
 AutoTestSynchronizer provides support for marshalling a thread from running one test selector to another. It stores the next selector to execute and implements a wait point between selectors. In typical use a thread running a test selector will invoke (via AutoTestScript) -setNextSelector: then -waitUntilSignaled. The latter puts the thread to sleep until some other thread invokes -signal, whereupon -waitUntilSignaled returns and AutoTestScript invokes the next test selector.
 */

@interface AutoTestSynchronizer : NSObject {
    SEL _startingSelector;
    void *_stackPointers;
    pthread_t _thread;
    SEL _selector;
    pthread_mutex_t _lock;
    pthread_cond_t _cond;
    BOOL _waiting;
    BOOL _signaled;
}

@property(readwrite, nonatomic) SEL startingSelector;
@property(readwrite, nonatomic) void *stackPointers;

// Returns a synchronizer object for the calling thread. The object remains associated until -checkin is called.
+ (AutoTestSynchronizer *)checkoutSynchronizer;

// Returns a synchronizer object if one is associated with the calling thread. Returns nil if none is associated.
+ (AutoTestSynchronizer *)mySynchronizer;

// Disassociate the receiver from its thread (which must be the calling thread)
- (void)checkin;

/*
 Get/set the next selector to run.
 */
- (SEL)nextSelector;
- (void)setNextSelector:(SEL)nextSel;

// Blocks the calling thread until some other thread invokes -signal. nextSelector is cleared as a side effect.
- (void)waitUntilSignaled;

// Wakes up a thread that is blocked in -waitUntilSignaled
- (void)signal;

// Returns yes if the thread associated with the receiving synchronizer instance is a tester thread (as opposed to a collector spawned thread running a probe callout)
- (BOOL)isTesterThread;

@end
