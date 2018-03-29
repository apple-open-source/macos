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

#if OCTAGON

#import "CKKSGroupOperation.h"
#include <utilities/debugging.h>

@interface CKKSGroupOperation()
@property bool fillInError;
@property NSBlockOperation* startOperation;
@property NSBlockOperation* finishOperation;
@property dispatch_queue_t queue;

@property NSMutableArray<CKKSResultOperation*>* internalSuccesses;
@end

@implementation CKKSGroupOperation

- (instancetype)init {
    if(self = [super init]) {
        __weak __typeof(self) weakSelf = self;

        _fillInError = true;

        _operationQueue = [[NSOperationQueue alloc] init];
        _internalSuccesses = [[NSMutableArray alloc] init];

        _queue = dispatch_queue_create("CKKSGroupOperationDispatchQueue", DISPATCH_QUEUE_SERIAL);

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
        [self.startOperation removeDependenciesUponCompletion];

        // The finish operation will 'finish' us
        _finishOperation = [NSBlockOperation blockOperationWithBlock:^{
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                secerror("ckks: received callback for released object");
                return;
            }

            [strongSelf completeOperation];
        }];
        [self.finishOperation removeDependenciesUponCompletion];

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

// We are pending if our start operation is pending but we are also not cancelled yet
- (BOOL)isPending {
    return [self.startOperation isPending] && ![self isCancelled];
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
            return [NSString stringWithFormat: @"<%@: %@ %@ - %@>", [self selfname],
                    [self operationStateString],
                    self.finishDate,
                    self.error];
        } else {
            return [NSString stringWithFormat: @"<%@: %@ %@>", [self selfname],
                    [self operationStateString],
                    self.finishDate];
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
        return [NSString stringWithFormat: @"<%@: %@ [%@] error:%@>", [self selfname], [self operationStateString], opsString, self.error];
    } else {
        return [NSString stringWithFormat: @"<%@: %@ [%@]%@>", [self selfname], [self operationStateString], opsString, [self pendingDependenciesString:@" dep:"]];
    }
}

- (NSString*)debugDescription {
    return [self description];
}

- (BOOL)isConcurrent {
    return YES;
}

- (BOOL)isExecuting {
    __block BOOL ret = FALSE;
    dispatch_sync(self.queue, ^{
        ret = self->executing;
    });
    return ret;
}

- (BOOL)isFinished {
    __block BOOL ret = FALSE;
    dispatch_sync(self.queue, ^{
        ret = self->finished;
    });
    return ret;
}

- (void)start {
    [self invalidateTimeout];

    if([self isCancelled]) {
        [self willChangeValueForKey:@"isFinished"];
        dispatch_sync(self.queue, ^{
            self->finished = YES;
        });
        [self didChangeValueForKey:@"isFinished"];
        return;
    }

    [self.operationQueue addOperation: self.startOperation];

    [self willChangeValueForKey:@"isExecuting"];
    dispatch_sync(self.queue, ^{
        self->executing = YES;
    });
    [self didChangeValueForKey:@"isExecuting"];
}

- (void)cancel {

    // Block off the start operation
    NSBlockOperation* block = [NSBlockOperation blockOperationWithBlock:^{}];
    [self.startOperation addDependency: block];

    [super cancel];

    // Cancel all operations currently on the queue, except for the finish operation
    NSArray<NSOperation*>* ops = [self.operationQueue.operations copy];
    for(NSOperation* op in ops) {
        if(![op isEqual: self.finishOperation]) {
            [op cancel];
        }
    }

    NSArray<NSOperation*>* finishDependencies = [self.finishOperation.dependencies copy];
    for(NSOperation* finishDep in finishDependencies) {
        if(!([ops containsObject: finishDep] || [finishDep isEqual:self.startOperation])) {
            // This is finish dependency that we don't control (and isn't our start operation)
            // Since we're cancelled, don't wait for it.
            [self.finishOperation removeDependency: finishDep];
        }
    }

    if([self.startOperation isPending]) {
        // If we were cancelled before starting, don't fill in our error later; we'll probably just get subresult cancelled
        self.fillInError = false;
    }

    // Now, we're in a position where either:
    //  1. This operation hasn't been started, and is now 'cancelled'
    //  2. This operation has beens started, and is now cancelled, and has delivered a 'cancel' message to all its suboperations,
    //       which may or may not comply
    //
    // In either case, this operation will complete its finish operation whenever it is 'started' and all of its cancelled suboperations finish.

    [self.operationQueue addOperation: block];
}

- (void)completeOperation {
    [self willChangeValueForKey:@"isFinished"];
    [self willChangeValueForKey:@"isExecuting"];

    dispatch_sync(self.queue, ^{
        if(self.fillInError) {
            // Run through all the failable operations in this group, and determine if we should be considered successful ourselves
            [self allSuccessful: self.internalSuccesses];
        }

        self->executing = NO;
        self->finished = YES;
    });

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

    // Make sure we wait for it.
    [self.finishOperation addDependency: suboperation];
    if([self.finishOperation isFinished]) {
        @throw [NSException exceptionWithName:NSInternalInconsistencyException reason:[NSString stringWithFormat:@"Attempt to add operation(%@) to completed group(%@)", suboperation, self] userInfo:nil];
    }

    // And it waits for us.
    [suboperation addDependency: self.startOperation];

    // If this is a CKKSResultOperation, then its result impacts our result.
    if([suboperation isKindOfClass: [CKKSResultOperation class]]) {
        // don't use addSuccessDependency, because it's not a dependency for The Group Operation, but rather a suboperation
        @synchronized(self) {
            [self.internalSuccesses addObject: (CKKSResultOperation*) suboperation];
        }
    }
}

+ (instancetype)operationWithBlock:(void (^)(void))block {
    CKKSGroupOperation* op = [[CKKSGroupOperation alloc] init];
    NSBlockOperation* blockOp = [NSBlockOperation blockOperationWithBlock:block];
    [op runBeforeGroupFinished:blockOp];
    return op;
}

+(instancetype)named:(NSString*)name withBlock:(void(^)(void)) block {
    CKKSGroupOperation* blockOp = [CKKSGroupOperation operationWithBlock: block];
    blockOp.name = name;
    return blockOp;
}

+ (instancetype)named:(NSString*)name withBlockTakingSelf:(void(^)(CKKSGroupOperation* strongOp))block
{
    CKKSGroupOperation* op = [[CKKSGroupOperation alloc] init];
    __weak __typeof(op) weakOp = op;
    [op runBeforeGroupFinished:[NSBlockOperation blockOperationWithBlock:^{
        __strong __typeof(op) strongOp = weakOp;
        block(strongOp);
    }]];
    op.name = name;
    return op;
}

@end

#endif // OCTAGON

