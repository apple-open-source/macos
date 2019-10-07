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

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/CKKSAnalytics.h"

NS_ASSUME_NONNULL_BEGIN

@protocol OctagonStateString <NSObject>
@end
typedef NSString<OctagonStateString> OctagonState;

@protocol OctagonFlagString <NSObject>
@end
typedef NSString<OctagonFlagString> OctagonFlag;

// NotStarted indicates that this state machine is not yet started
extern OctagonState* const OctagonStateMachineNotStarted;

// Halted indicates that the state machine is halted, and won't move again
extern OctagonState* const OctagonStateMachineHalted;

@protocol OctagonStateTransitionOperationProtocol
// Holds this operation's opinion of the next state, given that this operation just ran
@property OctagonState* nextState;

// Hold the state this operation was originally hoping to enter
@property (readonly) OctagonState* intendedState;
@end


@interface OctagonStateTransitionOperation : CKKSResultOperation <OctagonStateTransitionOperationProtocol>
@property OctagonState* nextState;
@property (readonly) OctagonState* intendedState;

+ (instancetype)named:(NSString*)name
            intending:(OctagonState*)intendedState
           errorState:(OctagonState*)errorState
              timeout:(dispatch_time_t)timeout
  withBlockTakingSelf:(void(^)(OctagonStateTransitionOperation* op))block;

// convenience constructor. Will always succeed at entering the state.
+ (instancetype)named:(NSString*)name
             entering:(OctagonState*)intendedState;
@end


@interface OctagonStateTransitionRequest<__covariant OperationType : CKKSResultOperation<OctagonStateTransitionOperationProtocol>*> : NSObject
@property (readonly) NSString* name;
@property (readonly) NSSet<OctagonState*>* sourceStates;
@property (readonly) OperationType transitionOperation;

- (instancetype)timeout:(dispatch_time_t)timeout;
- (OperationType _Nullable)_onqueueStart;

- (instancetype)init:(NSString*)name
        sourceStates:(NSSet<OctagonState*>*)sourceStates
         serialQueue:(dispatch_queue_t)queue
             timeout:(dispatch_time_t)timeout
        transitionOp:(OperationType)transitionOp;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
