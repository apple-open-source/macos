/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/OTStates.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

OctagonState* const OctagonStateMachineNotStarted = (OctagonState*) @"not_started";
OctagonState* const OctagonStateMachineHalted = (OctagonState*) @"halted";

#pragma mark -- OctagonStateTransitionOperation

@implementation OctagonStateTransitionOperation
- (instancetype)initIntending:(OctagonState*)intendedState
                   errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _nextState = errorState;
        _intendedState = intendedState;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<OctagonStateTransitionOperation(%@): intended:%@ actual:%@>", self.name, self.intendedState, self.nextState];
}

+ (instancetype)named:(NSString*)name
            intending:(OctagonState*)intendedState
           errorState:(OctagonState*)errorState
  withBlockTakingSelf:(void(^)(OctagonStateTransitionOperation* op))block
{
    OctagonStateTransitionOperation* op = [[self alloc] initIntending:intendedState
                                                           errorState:errorState];
    WEAKIFY(op);
    [op addExecutionBlock:^{
        STRONGIFY(op);
        block(op);
    }];
    op.name = name;
    return op;
}

+ (instancetype)named:(NSString*)name
             entering:(OctagonState*)intendedState
{
    OctagonStateTransitionOperation* op = [[self alloc] initIntending:intendedState
                                                           errorState:intendedState];
    op.name = name;
    return op;
}

@end

#pragma mark -- OctagonStateTransitionGroupOperation

@implementation OctagonStateTransitionGroupOperation
- (instancetype)initIntending:(OctagonState*)intendedState
                   errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _nextState = errorState;
        _intendedState = intendedState;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<OctagonStateTransitionGroupOperation(%@): intended:%@ actual:%@>", self.name, self.intendedState, self.nextState];
}

+ (instancetype)named:(NSString*)name
            intending:(OctagonState*)intendedState
           errorState:(OctagonState*)errorState
  withBlockTakingSelf:(void(^)(OctagonStateTransitionGroupOperation* op))block
{
    OctagonStateTransitionGroupOperation* op = [[self alloc] initIntending:intendedState
                                                                errorState:errorState];
    WEAKIFY(op);
    [op runBeforeGroupFinished:[NSBlockOperation blockOperationWithBlock:^{
        STRONGIFY(op);
        block(op);
    }]];
    op.name = name;
    return op;
}
@end

#pragma mark -- OctagonStateTransitionRequest

@interface OctagonStateTransitionRequest ()
@property dispatch_queue_t queue;
@property bool timeoutCanOccur;
@end

@implementation OctagonStateTransitionRequest

- (instancetype)init:(NSString*)name
        sourceStates:(NSSet<OctagonState*>*)sourceStates
         serialQueue:(dispatch_queue_t)queue
             timeout:(dispatch_time_t)timeout
        transitionOp:(CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)transitionOp
{
    if((self = [super init])) {
        _name = name;
        _sourceStates = sourceStates;
        _queue = queue;

        _timeoutCanOccur = true;
        _transitionOperation = transitionOp;
    }
    
    [self timeout:timeout];

    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<OctagonStateTransitionRequest: %@ %@ sources:%d>", self.name, self.transitionOperation, (unsigned int)[self.sourceStates count]];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)_onqueueStart
{
    dispatch_assert_queue(self.queue);

    if(self.timeoutCanOccur) {
        self.timeoutCanOccur = false;
        return self.transitionOperation;
    } else {
        return nil;
    }
}

- (instancetype)timeout:(dispatch_time_t)timeout
{
    WEAKIFY(self);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, timeout), self.queue, ^{
        STRONGIFY(self);
        if(self.timeoutCanOccur) {
            self.timeoutCanOccur = false;

            // The operation will only realize it's finished once added to any operation queue. Fake one up.
            [self.transitionOperation timeout:0*NSEC_PER_SEC];
            [[[NSOperationQueue alloc] init] addOperation:self.transitionOperation];
        }
    });

    return self;
}

@end

#endif // OCTAGON
