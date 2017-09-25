/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import "CKKSGroupOperation.h"
#import "CKKSCondition.h"
#include <utilities/debugging.h>

@implementation NSOperation (CKKSUsefulPrintingOperation)
- (NSString*)selfname {
    if(self.name) {
        return [NSString stringWithFormat: @"%@(%@)", NSStringFromClass([self class]), self.name];
    } else {
        return NSStringFromClass([self class]);
    }
}

-(void)linearDependencies: (NSHashTable*) collection {
    @synchronized(collection) {
        for(NSOperation* existingop in collection) {
            if(existingop == self) {
                // don't depend on yourself
                continue;
            }
            [self addDependency: existingop];
        }
        [collection addObject:self];
    }
}

-(NSString*)pendingDependenciesString:(NSString*)prefix {
    NSArray* dependencies = [self.dependencies copy];
    dependencies = [dependencies objectsAtIndexes: [dependencies indexesOfObjectsPassingTest: ^BOOL (id obj,
                                                                                                     NSUInteger idx,
                                                                                                     BOOL* stop) {
        return [obj isPending] ? YES : NO;
    }]];

    if(dependencies.count == 0u) {
        return @"";
    }

    return [NSString stringWithFormat: @"%@%@", prefix, [dependencies componentsJoinedByString: @", "]];
}

- (NSString*)description {
    NSString* state = ([self isFinished] ? @"finished" :
                       [self isCancelled] ? @"cancelled" :
                       [self isExecuting] ? @"executing" :
                       [self isReady] ? @"ready" :
                       @"pending");

    return [NSString stringWithFormat: @"<%@: %@%@>", [self selfname], state, [self pendingDependenciesString: @" dep:"]];
}
- (NSString*)debugDescription {
    NSString* state = ([self isFinished] ? @"finished" :
                       [self isCancelled] ? @"cancelled" :
                       [self isExecuting] ? @"executing" :
                       [self isReady] ? @"ready" :
                       @"pending");

    return [NSString stringWithFormat: @"<%@ (%p): %@%@>", [self selfname], self, state, [self pendingDependenciesString: @" dep:"]];
}

- (BOOL)isPending {
    return (!([self isExecuting] || [self isFinished])) ? YES : NO;
}

- (void)addNullableDependency: (NSOperation*) op {
    if(op) {
        [self addDependency:op];
    }
}
@end

@implementation NSBlockOperation (CKKSUsefulConstructorOperation)
+(instancetype)named: (NSString*)name withBlock: (void(^)(void)) block {
    // How many blocks could a block block if a block could block blocks?
    NSBlockOperation* blockOp = [NSBlockOperation blockOperationWithBlock: block];
    blockOp.name = name;
    return blockOp;
}
@end


@interface CKKSResultOperation()
@property NSMutableArray<CKKSResultOperation*>* successDependencies;
@property bool timeoutCanOccur;
@property dispatch_queue_t timeoutQueue;
@property void (^finishingBlock)(void);
@end

@implementation CKKSResultOperation
- (instancetype)init {
    if(self = [super init]) {
        _error = nil;
        _successDependencies = [[NSMutableArray alloc] init];
        _timeoutCanOccur = true;
        _timeoutQueue = dispatch_queue_create("result-operation-timeout", DISPATCH_QUEUE_SERIAL);
        _completionHandlerDidRunCondition = [[CKKSCondition alloc] init];

        __weak __typeof(self) weakSelf = self;
        _finishingBlock = ^(void) {
            weakSelf.finishDate = [NSDate dateWithTimeIntervalSinceNow:0];
        };
        self.completionBlock = ^{}; // our _finishing block gets added in the method override
    }
    return self;
}

- (NSString*)description {
    NSString* state = ([self isFinished] ? [NSString stringWithFormat:@"finished %@", self.finishDate] :
                       [self isCancelled] ? @"cancelled" :
                       [self isExecuting] ? @"executing" :
                       [self isReady] ? @"ready" :
                       @"pending");

    if(self.error) {
        return [NSString stringWithFormat: @"<%@: %@ error:%@>", [self selfname], state, self.error];
    } else {
        return [NSString stringWithFormat: @"<%@: %@%@>", [self selfname], state, [self pendingDependenciesString:@" dep:"]];
    }
}

- (NSString*)debugDescription {
    return [self description];
}

- (void)setCompletionBlock:(void (^)(void))completionBlock
{
    __weak __typeof(self) weakSelf = self;
    [super setCompletionBlock:^(void) {
        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            secerror("ckksresultoperation: completion handler called on deallocated operation instance");
            completionBlock(); // go ahead and still behave as things would if this method override were not here
            return;
        }

        strongSelf.finishingBlock();
        completionBlock();
        [strongSelf.completionHandlerDidRunCondition fulfill];
    }];
}

- (void)start {
    if(![self allDependentsSuccessful]) {
        secdebug("ckksresultoperation", "Not running due to some failed dependent: %@", self.error);
        [self cancel];
    } else {
        dispatch_sync(self.timeoutQueue, ^{
            if(![self isCancelled]) {
                self.timeoutCanOccur = false;
            };
        });
    }

    [super start];
}

- (instancetype)timeout:(dispatch_time_t)timeout {
    __weak __typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, timeout), self.timeoutQueue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if(strongSelf.timeoutCanOccur) {
            strongSelf.error = [NSError errorWithDomain:CKKSResultErrorDomain code: CKKSResultTimedOut userInfo:@{NSLocalizedDescriptionKey:[NSString stringWithFormat:@"Operation timed out waiting to start for [%@]", [self pendingDependenciesString:@""]]}];
            strongSelf.timeoutCanOccur = false;
            [strongSelf cancel];
        }
    });

    return self;
}

- (void)addSuccessDependency: (CKKSResultOperation*) operation {
    if(!operation) {
        return;
    }
    @synchronized(self) {
        [self.successDependencies addObject: operation];
        [self addDependency: operation];
    }
}

- (bool)allDependentsSuccessful {
    return [self allSuccessful: self.successDependencies];
}

- (bool)allSuccessful: (NSArray<CKKSResultOperation*>*) operations {
    @synchronized(self) {
        bool result = false;

        bool finished = true;   // all dependents must be finished
        bool cancelled = false; // no dependents can be cancelled
        bool failed = false;    // no dependents can have failed

        for(CKKSResultOperation* op in operations) {
            finished  &= !!([op isFinished]);
            cancelled |= !!([op isCancelled]);
            failed    |= (op.error != nil);

            // TODO: combine suberrors
            if(op.error != nil) {
                if([op.error.domain isEqual: CKKSResultErrorDomain] && op.error.code == CKKSResultSubresultError) {
                    // Already a subresult, just copy it on in
                    self.error = op.error;
                } else {
                    self.error = [NSError errorWithDomain:CKKSResultErrorDomain code: CKKSResultSubresultError userInfo:@{ NSUnderlyingErrorKey: op.error}];
                }
            }
        }

        result = finished && !( cancelled || failed );

        if(!result && self.error == nil) {
            self.error = [NSError errorWithDomain:CKKSResultErrorDomain code: CKKSResultSubresultCancelled userInfo:nil];
        }
        return result;
    }
}

+ (CKKSResultOperation*)operationWithBlock:(void (^)(void))block {
    CKKSResultOperation* op = [[CKKSResultOperation alloc] init];
    [op addExecutionBlock: block];
    return op;
}

+(instancetype)named:(NSString*)name withBlock:(void(^)(void)) block {
    CKKSResultOperation* blockOp = [CKKSResultOperation operationWithBlock: block];
    blockOp.name = name;
    return blockOp;
}
@end


@interface CKKSGroupOperation()
@property NSBlockOperation* startOperation;
@property NSBlockOperation* finishOperation;

@property NSMutableArray<CKKSResultOperation*>* internalSuccesses;
@end


@implementation CKKSGroupOperation

- (instancetype)init {
    if(self = [super init]) {
        __weak __typeof(self) weakSelf = self;

        _operationQueue = [[NSOperationQueue alloc] init];
        _internalSuccesses = [[NSMutableArray alloc] init];

        // At start, we'll call this method (for subclasses)
        _startOperation = [NSBlockOperation blockOperationWithBlock:^{
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                secerror("ckks: received callback for released object");
                return;
            }

            if(![strongSelf allDependentsSuccessful]) {
                secdebug("ckksgroup", "Not running due to some failed dependent: %@", strongSelf.error);
                [strongSelf cancel];
                return;
            }

            [strongSelf groupStart];
        }];

        // The finish operation will 'finish' us
        _finishOperation = [NSBlockOperation blockOperationWithBlock:^{
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                secerror("ckks: received callback for released object");
                return;
            }

            [strongSelf completeOperation];
        }];

        [self.finishOperation addDependency: self.startOperation];
        [self.operationQueue addOperation: self.finishOperation];

        self.startOperation.name = @"group-start";
        self.finishOperation.name = @"group-finish";

        executing = NO;
        finished = NO;
    }
    return self;
}

- (void)dealloc {
    // If the GroupOperation is dealloced before starting, all of its downstream operations form a retain loop.

    if([self isPending]) {
        [self.operationQueue cancelAllOperations];
        [self.startOperation cancel];
        [super cancel];
    }
}

- (BOOL)isPending {
    return [self.startOperation isPending];
}

- (void)setName:(NSString*) name {
    self.operationQueue.name =  [NSString stringWithFormat: @"group-queue:%@", name];
    self.startOperation.name =  [NSString stringWithFormat: @"group-start:%@", name];
    self.finishOperation.name = [NSString stringWithFormat: @"group-finish:%@", name];
    [super setName: name];
}

- (NSString*)description {
    if(self.isFinished) {
        if(self.error) {
            return [NSString stringWithFormat: @"<%@: finished %@ - %@>", [self selfname], self.finishDate, self.error];
        } else {
            return [NSString stringWithFormat: @"<%@: finished %@>", [self selfname], self.finishDate];
        }
    }

    NSMutableArray* ops = [self.operationQueue.operations mutableCopy];

    [ops removeObject: self.finishOperation];

    // Any extra dependencies from the finish operation should be considered part of this group
    for(NSOperation* finishDep in self.finishOperation.dependencies) {
        if(finishDep != self.startOperation && (NSNotFound == [ops indexOfObject: finishDep])) {
            [ops addObject: finishDep];
        }
    }

    NSString* opsString = [ops componentsJoinedByString:@", "];

    if(self.error) {
        return [NSString stringWithFormat: @"<%@: [%@] error:%@>", [self selfname], opsString, self.error];
    } else {
        return [NSString stringWithFormat: @"<%@: [%@]%@>", [self selfname], opsString, [self pendingDependenciesString:@" dep:"]];
    }
}

- (NSString*)debugDescription {
    return [self description];
}

- (BOOL)isConcurrent {
    return YES;
}

- (BOOL)isExecuting {
    return self->executing;
}

- (BOOL)isFinished {
    return self->finished;
}

- (void)start {
    if([self isCancelled]) {
        [self willChangeValueForKey:@"isFinished"];
        finished = YES;
        [self didChangeValueForKey:@"isFinished"];
        return;
    }

    [self.operationQueue addOperation: self.startOperation];

    [self willChangeValueForKey:@"isExecuting"];
    executing = YES;
    [self didChangeValueForKey:@"isExecuting"];
}

- (void)cancel {
    [self.operationQueue cancelAllOperations];
    [self.startOperation cancel];

    [super cancel];

    // Our finishoperation might not fire (as we cancelled it above), so let's help it out
    [self completeOperation];
}

- (void)completeOperation {
    [self willChangeValueForKey:@"isFinished"];
    [self willChangeValueForKey:@"isExecuting"];

    // Run through all the failable operations in this group, and determine if we should be considered successful ourselves
    [self allSuccessful: self.internalSuccesses];

    executing = NO;
    finished = YES;

    [self didChangeValueForKey:@"isExecuting"];
    [self didChangeValueForKey:@"isFinished"];
}

- (void)addDependency:(NSOperation *)op {
    [super addDependency:op];
    [self.startOperation addDependency: op];
}

- (void)groupStart {
    // Do nothing. Subclasses can do things here.
}

- (void)runBeforeGroupFinished: (NSOperation*) suboperation {
    if([self isCancelled]) {
        // Cancelled operations can't add anything.
        secnotice("ckksgroup", "Not adding operation to cancelled group");
        return;
    }

    // op must wait for this operation to start
    [suboperation addDependency: self.startOperation];

    [self dependOnBeforeGroupFinished: suboperation];
    [self.operationQueue addOperation: suboperation];
}

- (void)dependOnBeforeGroupFinished: (NSOperation*) suboperation {
    if(suboperation == nil) {
        return;
    }

    if([self isCancelled]) {
        // Cancelled operations can't add anything.
        secnotice("ckksgroup", "Can't add operation dependency to cancelled group");
        return;
    }

    if([self.finishOperation isExecuting] || [self.finishOperation isFinished]) {
        @throw @"Attempt to add operation to completed group";
    }

    // If this is a CKKSResultOperation, then its result impacts our result.
    if([suboperation isKindOfClass: [CKKSResultOperation class]]) {
        // don't use addSuccessDependency, because it's not a dependency for The Group Operation, but rather a suboperation
        @synchronized(self) {
            [self.internalSuccesses addObject: (CKKSResultOperation*) suboperation];
        }
    }

    // Make sure it waits for us...
    [suboperation addDependency: self.startOperation];
    // and we wait for it.
    [self.finishOperation addDependency: suboperation];
}

@end
