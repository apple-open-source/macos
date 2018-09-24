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

#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/NSOperationCategories.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#include <utilities/debugging.h>

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

- (NSString*)operationStateString {
    return ([self isFinished] ? [NSString stringWithFormat:@"finished %@", self.finishDate] :
            [self isCancelled] ? @"cancelled" :
            [self isExecuting] ? @"executing" :
            [self isReady] ? @"ready" :
            @"pending");
}

- (NSString*)description {
    NSString* state = [self operationStateString];

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

        for (NSOperation *op in strongSelf.dependencies) {
            [strongSelf removeDependency:op];
        }
    }];
}

- (void)start {
    if(![self allDependentsSuccessful]) {
        secdebug("ckksresultoperation", "Not running due to some failed dependent: %@", self.error);
        [self cancel];
    } else {
        [self invalidateTimeout];

    }

    [super start];
}

- (void)invalidateTimeout {
    dispatch_sync(self.timeoutQueue, ^{
        if(![self isCancelled]) {
            self.timeoutCanOccur = false;
        };
    });
}

- (NSError* _Nullable)dependenciesDescriptionError {
    NSError* underlyingReason = nil;
    NSArray* dependencies = [self.dependencies copy];
    dependencies = [dependencies objectsAtIndexes: [dependencies indexesOfObjectsPassingTest: ^BOOL (id obj,
                                                                                                     NSUInteger idx,
                                                                                                     BOOL* stop) {
        return [obj isFinished] ? NO : YES;
    }]];

    for(NSOperation* dependency in dependencies) {
        if([dependency isKindOfClass:[CKKSResultOperation class]]) {
            CKKSResultOperation* ro = (CKKSResultOperation*)dependency;
            underlyingReason = [ro descriptionError] ?: underlyingReason;
        }
    }

    return underlyingReason;
}

// Returns, for this CKKSResultOperation, an error describing this operation or its dependents.
// Used mainly by other CKKSResultOperations who time out waiting for this operation to start/complete.
- (NSError* _Nullable)descriptionError {
    if(self.descriptionErrorCode != 0) {
        return [NSError errorWithDomain:CKKSResultDescriptionErrorDomain
                                   code:self.descriptionErrorCode
                               userInfo:nil];
    } else {
        return [self dependenciesDescriptionError];
    }
}

- (NSError*)_onqueueTimeoutError {
    // Find if any of our dependencies are CKKSResultOperations with a custom reason for existing

    NSError* underlyingReason = [self descriptionError];

    NSError* error = [NSError errorWithDomain:CKKSResultErrorDomain
                                         code:CKKSResultTimedOut
                                  description:[NSString stringWithFormat:@"Operation(%@) timed out waiting to start for [%@]",
                                               [self selfname],
                                               [self pendingDependenciesString:@""]]
                                   underlying:underlyingReason];
    return error;
}

- (instancetype)timeout:(dispatch_time_t)timeout {
    __weak __typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, timeout), self.timeoutQueue, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if(strongSelf.timeoutCanOccur) {
            strongSelf.error = [self _onqueueTimeoutError];
            strongSelf.timeoutCanOccur = false;
            [strongSelf cancel];
        }
    });

    return self;
}

- (void)addSuccessDependency:(CKKSResultOperation *)operation {
    [self addNullableSuccessDependency:operation];
}

- (void)addNullableSuccessDependency:(CKKSResultOperation *)operation {
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
        NSMutableArray<NSOperation*>* cancelledSuboperations = [NSMutableArray array];

        for(CKKSResultOperation* op in operations) {
            finished  &= !!([op isFinished]);
            cancelled |= !!([op isCancelled]);
            failed    |= (op.error != nil);

            if([op isCancelled]) {
                [cancelledSuboperations addObject:op];
            }

            // TODO: combine suberrors
            if(op.error != nil) {
                if([op.error.domain isEqual: CKKSResultErrorDomain] && op.error.code == CKKSResultSubresultError) {
                    // Already a subresult, just copy it on in
                    self.error = op.error;
                } else {
                    self.error = [NSError errorWithDomain:CKKSResultErrorDomain
                                                     code:CKKSResultSubresultError
                                              description:@"Success-dependent operation failed"
                                               underlying:op.error];
                }
            }
        }

        result = finished && !( cancelled || failed );

        if(!result && self.error == nil) {
            self.error = [NSError errorWithDomain:CKKSResultErrorDomain code: CKKSResultSubresultCancelled userInfo:@{NSLocalizedDescriptionKey:[NSString stringWithFormat:@"Operation (%@) cancelled", cancelledSuboperations]}];
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

+ (instancetype)named:(NSString*)name withBlockTakingSelf:(void(^)(CKKSResultOperation* op))block
{
    CKKSResultOperation* op = [[CKKSResultOperation alloc] init];
    __weak __typeof(op) weakOp = op;
    [op addExecutionBlock:^{
        __strong __typeof(op) strongOp = weakOp;
        block(strongOp);
    }];
    op.name = name;
    return op;
}
@end

#endif // OCTAGON
