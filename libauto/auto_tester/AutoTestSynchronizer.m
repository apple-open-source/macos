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
//  AutoTestSynchronizer.m
//  auto
//
//  Created by Josh Behnke on 6/2/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import "AutoTestSynchronizer.h"
#import <objc/objc-auto.h>


@implementation AutoTestSynchronizer

#define SYNCHRONIZER_COUNT 32
static NSArray *_synchronizers = nil;
static pthread_key_t _synchronizerKey;

// It is an error if this ever gets called. Threads should not exit with an associated synchronizer.
static void synchronizerDestructor(void *unused)
{
    printf("test bug: some thread didn't give up its synchronizer\n");
    exit(1);
}


@synthesize startingSelector=_startingSelector;
@synthesize stackPointers=_stackPointers;


+ (void)initialize
{
    // We just make a bunch of instances so we won't need to allocate during test execution.
    if (self == [AutoTestSynchronizer class]) {
        AutoTestSynchronizer *synchronizers[SYNCHRONIZER_COUNT];
        int i;
        for (i=0; i<SYNCHRONIZER_COUNT; i++) {
            synchronizers[i] = [[AutoTestSynchronizer alloc] init];
        }
        _synchronizers = [[NSArray alloc] initWithObjects:synchronizers count:SYNCHRONIZER_COUNT];
        if (pthread_key_create(&_synchronizerKey, synchronizerDestructor)) {
            printf("FAIL: could not create synchronizer key\n");
            exit(1);
        }
    }
}


+ (AutoTestSynchronizer *)checkoutSynchronizer
{
    // First see if there is an instance already associated with this thread
    AutoTestSynchronizer *synchronizer = [self mySynchronizer];
    
    // No? Then find an unassociated instance.
    if (synchronizer == nil) {
        @synchronized ([AutoTestSynchronizer class]) {
            for (AutoTestSynchronizer *s in _synchronizers) {
                if (!s->_thread) {
                    synchronizer = s;
                    break;
                }
            }
            
            // Associate the instance with the current thread.
            if (synchronizer) {
                synchronizer->_thread = pthread_self();
                pthread_setspecific(_synchronizerKey, synchronizer);
            }
        }
    }
    
    if (!synchronizer) {
        printf("Failed to check out synchronizer. Increase SYNCHRONIZER_COUNT?\n");
        exit(1);
    }
    return synchronizer;
}


+ (AutoTestSynchronizer *)mySynchronizer
{
    // Just check the thread key and see if there is an associated instance.
    AutoTestSynchronizer *synchronizer = (AutoTestSynchronizer *)pthread_getspecific(_synchronizerKey);
    return synchronizer;
}


// Disassociate the receiver from its thread (which must be the calling thread)
- (void)checkin
{
    @synchronized ([AutoTestSynchronizer class]) {
        _thread = nil;
        _selector = nil;
        [self setStackPointers:NULL];
        [self setStartingSelector:NULL];
        pthread_setspecific(_synchronizerKey, NULL);
    }
}


- (id)init
{
    self = [super init];
    if (self) {
        pthread_mutex_init(&_lock, NULL);
        pthread_cond_init(&_cond, NULL);
    }
    return self;
}


- (BOOL)isTesterThread
{
    // A tester thread will have set a stack pointer buffer
    return [self stackPointers] != NULL;
}


- (SEL)nextSelector
{
    return _selector;
}


- (void)setNextSelector:(SEL)nextSel
{
    pthread_mutex_lock(&_lock);
    if (_selector != NULL) {
        printf("setNextSelector: called, but next selector already set\n");
        exit(1);
    }
    if (_waiting) {
        printf("setNextSelector: called while a thread is waiting\n");
        exit(1);
    }
    _selector = nextSel;
    _waiting = NO;
    _signaled = NO;
    pthread_mutex_unlock(&_lock);
}


- (void)waitUntilSignaled
{
    // Note that _selector is cleared as a side effect.
    pthread_mutex_lock(&_lock);
    if (_selector) {
        _waiting = YES;
        objc_clear_stack(0);
        while (!_signaled)
            pthread_cond_wait(&_cond, &_lock);
        _waiting = NO;
        _selector = NULL;
    } else {
        printf("AutoTestSynchronizer waitUntilSignaled called but no selector set.\n");
        exit(1);
    }
    pthread_mutex_unlock(&_lock);
}


- (void)signal
{
    pthread_mutex_lock(&_lock);
    if (_signaled) {
        printf("signal called, but synchronizer already signaled!\n");
    }
    while (!_waiting && !_signaled)
        pthread_cond_wait(&_cond, &_lock);
    _signaled = YES;
    pthread_cond_signal(&_cond);
    pthread_mutex_unlock(&_lock);
}


@end
