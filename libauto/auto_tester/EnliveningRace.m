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
//  EnliveningRace.m
//  Copyright (c) 2008-2011 Apple Inc.. All rights reserved.
//

#import <alloca.h>

#import "WhiteBoxTest.h"

@interface EnliveningRace : WhiteBoxTest {
}
@end


@interface ERList : NSObject {
    ERList *next;
    void *reserved;
}
@property ERList *next;
@end
@implementation ERList
@synthesize next;
@end

@interface ERCondition : NSObject <NSLocking> {
    pthread_mutex_t _mutex;
    pthread_cond_t _condition;
}
- (void)wait;
- (BOOL)waitWithTimeout:(long)seconds;
- (void)signal;
@end

@implementation ERCondition

- (id)init {
    self = [super init];
    pthread_mutex_init(&_mutex, NULL);
    pthread_cond_init(&_condition, NULL);
    return self;
}

- (void)finalize {
    pthread_cond_destroy(&_condition);
    pthread_mutex_destroy(&_mutex);
    [super finalize];
}

- (void)lock {
    pthread_mutex_lock(&_mutex);
}

- (void)unlock {
    pthread_mutex_unlock(&_mutex);
}

- (void)wait {
    pthread_cond_wait(&_condition, &_mutex);
}

- (BOOL)waitWithTimeout:(long)seconds {
    struct timespec timeout = { seconds, 0 };
    pthread_cond_timedwait_relative_np(&_condition, &_mutex, &timeout);
}

- (void)signal {
    pthread_cond_signal(&_condition);
}

@end


@interface ERQueue : NSObject {
    id owner;
    ERList *list;
    void *reserved;
}
@property id owner;
@property ERList *list;
@end

@implementation ERQueue
@synthesize owner, list;
@end

static ERQueue *queue;

static void *worker_address;
static void *list_address;
static void *rest_address;

@interface ERWorker : NSThread {
    ERCondition *condition;
    ERList *work;
}
@property(readonly) ERCondition *condition;
@end

@implementation ERWorker

@synthesize condition;

- (id)init {
    self = [super init];
    if (self) {
        condition = [ERCondition new];
    }
    return self;
}

- (void)createList {
    // create a 3 element work list.
    list_address = queue.list = [ERList new];
    ERList *list = queue.list;
    rest_address = list.next = [ERList new];
    list = list.next;
    list.next = [ERList new];
}

- (void)swapWork {
    // swap ownership of the list between the worker and the queue.
    ERList *temp = queue.list;
    queue.list = work;
    work = temp;
}

- (ERList *)hideWork {
    ERList *list = queue.list;
    ERList *rest = list.next;
    list.next = nil;
    return rest;
}

- (void)advance {
    [condition lock];
    [condition signal];
    [condition waitWithTimeout:10]; // wait up to 10 seconds to avoid deadlocks.
    [condition unlock];
}

#define CLEAR_STACK() bzero(alloca(512), 512)

- (void)main {
    [condition lock];

    [self createList];
    CLEAR_STACK();
    [condition signal];
    [condition wait];
    
    // list is about to be scanned. hide the list from the collector.
    [self swapWork];
    CLEAR_STACK();
    [condition signal];
    [condition wait];

    // now that queue has been scanned, put it back.
    [self swapWork];
    CLEAR_STACK();
    [condition signal];
    [condition wait];
    
    // hide the rest of the list on the stack. only the head of the list will get to be enlivened.
    ERList *rest = [self hideWork];
    [condition signal];
    [condition wait];
    
    // we have this extra state to ensure that rest will remain live to this thread.
    size_t size = malloc_size(rest);
    [condition signal];
    [condition unlock];
}

@end

@implementation EnliveningRace

#warning this test isn't working right
- (NSString *)shouldSkip
{
    return @"This test isn't working right";
}

- (void)testDone {
    if ([self result] != FAILED)
        [self passed];
}

- (void)performTest {
    queue = [ERQueue new];
    worker_address = queue.owner = [ERWorker new];
    
    // wait for the worker to create the list.
    ERWorker *worker = queue.owner;
    ERCondition *condition = worker.condition;
    [condition lock];
    [worker start];
    [condition wait];
    [condition unlock];

    // start a collection. when "list" is scanned, transfer control from collector to us.
    [self requestFullCollectionWithCompletionCallback:^{ [self testDone]; }];
}

- (void)scanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map {
    if (block == queue) {
        // about to scan the queue. tell the worker to take the items.
        ERWorker *worker = queue.owner;
        [worker advance];
    } else if (block == worker_address) {
        worker_address = worker_address;
    } else if (block == list_address) {
        list_address = list_address;
    } else if (block == rest_address) {
        rest_address = rest_address;
    }
}

- (void)didScanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map {
    if (block == queue) {
        // now, tell the worker to put the items back.
        ERWorker *worker = queue.owner;
        [worker advance];
    }
}

- (void)scanBarrier {
    ERWorker *worker = queue.owner;
    [worker advance];
}

- (void)endHeapScanWithGarbage:(void **)garbage_list count:(size_t)count {
    ERWorker *worker = queue.owner;
    [worker advance];
    for (size_t i = 0; i < count; i++) {
        if (garbage_list[i] == rest_address) {
            [self fail:@"Enlivening Race Condition Detected."];
            break;
        }
    }
}

@end
