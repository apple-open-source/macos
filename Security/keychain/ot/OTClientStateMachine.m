/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import <CoreCDP/CDPAccount.h>

#import <Security/Security.h>

#include <utilities/SecFileLocations.h>
#include <Security/SecRandomP.h>

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSResultOperation.h"

#import "keychain/ckks/OctagonAPSReceiver.h"

#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"
#import "keychain/ot/ObjCImprovements.h"

#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTSOSAdapter.h"
#import "keychain/ot/OTEpochOperation.h"
#import "keychain/ot/OTClientVoucherOperation.h"
#import "keychain/ot/OTStates.h"

#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"

#define otclientnotice(scope, format, ...) __extension__({ \
        os_log(secLogObjForCFScope((__bridge CFStringRef)(scope)), format, ##__VA_ARGS__); \
    })

/*Piggybacking and ProximitySetup as Acceptor Octagon only*/
OctagonState* const OctagonStateAcceptorBeginClientJoin = (OctagonState*)@"client_join";
OctagonState* const OctagonStateAcceptorBeginAwaitEpochRequest = (OctagonState*)@"await_epoch_request";
OctagonState* const OctagonStateAcceptorEpochPrepared = (OctagonState*)@"epoch_prepared";
OctagonState* const OctagonStateAcceptorAwaitingIdentity = (OctagonState*)@"await_identity";
OctagonState* const OctagonStateAcceptorVoucherPrepared = (OctagonState*)@"voucher_prepared";
OctagonState* const OctagonStateAcceptorDone = (OctagonState*)@"done";

NSDictionary<OctagonState*, NSNumber*>* OctagonClientStateMap(void) {
    static NSDictionary<OctagonState*, NSNumber*>* map = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        map = @{
                OctagonStateAcceptorBeginClientJoin:         @0U,
                OctagonStateAcceptorBeginAwaitEpochRequest:  @1U,
                OctagonStateAcceptorEpochPrepared:           @2U,
                OctagonStateAcceptorAwaitingIdentity:        @3U,
                OctagonStateAcceptorVoucherPrepared:         @4U,
                OctagonStateAcceptorDone:                    @5U,
                };
    });
    return map;
}

@interface OTClientStateMachine ()
{
    OctagonState* _currentState;
}

@property dispatch_queue_t queue;
@property NSOperationQueue* operationQueue;

// Make writable
@property OctagonState* currentState;
@property NSString* clientScope;

// Set this to an operation to pause the state machine in-flight
@property NSOperation* holdStateMachineOperation;

@property CKKSResultOperation* nextClientStateMachineCycleOperation;

@property NSMutableArray<OctagonStateTransitionRequest<CKKSResultOperation<OctagonStateTransitionOperationProtocol>*>*>* stateMachineClientRequests;

@end

@implementation OTClientStateMachine

- (instancetype)initWithContainerName:(NSString*)containerName
                            contextID:(NSString*)contextID
                           clientName:(NSString*)clientName
                           cuttlefish:(id<NSXPCProxyCreating>)cuttlefish
{
    if ((self = [super init])) {
        _containerName = containerName;
        _clientName = clientName;
        _contextID = contextID;

        _queue = dispatch_queue_create("com.apple.security.otclientstatemachine", DISPATCH_QUEUE_SERIAL);
        _operationQueue = [[NSOperationQueue alloc] init];

        _stateConditions = [[NSMutableDictionary alloc] init];
        [OctagonClientStateMap() enumerateKeysAndObjectsUsingBlock:^(OctagonState * _Nonnull key, NSNumber * _Nonnull obj, BOOL * _Nonnull stop) {
            self.stateConditions[key] = [[CKKSCondition alloc] init];
        }];

        // Use the setter method to set the condition variables
        self.currentState = OctagonStateMachineNotStarted;

        _stateMachineClientRequests = [NSMutableArray array];
        _holdStateMachineOperation = [NSBlockOperation blockOperationWithBlock:^{}];

        OctagonStateTransitionOperation* beginClientJoin = [OctagonStateTransitionOperation named:@"initialize-client"
                                                                                      entering:OctagonStateAcceptorBeginClientJoin];
        [beginClientJoin addDependency:_holdStateMachineOperation];
        [_operationQueue addOperation:beginClientJoin];

        CKKSResultOperation* startStateMachineOp = [self createOperationToFinishAttemptForClient:beginClientJoin  clientName:clientName];
        [_operationQueue addOperation:startStateMachineOp];

        _cuttlefishXPCConnection = cuttlefish;

        _clientScope = [NSString stringWithFormat:@"octagon-client-%@-state", clientName];
    }
    return self;
}

- (void)dealloc
{
    // TODO: how to invalidate this?
    //[self.cuttlefishXPCConnection invalidate];
}

- (OctagonState* _Nonnull)currentState {
    return _currentState;
}

- (void)setCurrentState:(OctagonState* _Nonnull)state {
    if((state == nil && _currentState == nil) || ([state isEqualToString:_currentState])) {
        // No change, do nothing.
    } else {
        // Fixup the condition variables as part of setting this state
        if(_currentState) {
            self.stateConditions[_currentState] = [[CKKSCondition alloc] init];
        }

        _currentState = state;

        if(state) {
            [self.stateConditions[state] fulfill];
        }
    }
}

- (void)startOctagonStateMachine {
    dispatch_sync(self.queue, ^{
        if(self.holdStateMachineOperation) {
            [self.operationQueue addOperation: self.holdStateMachineOperation];
            self.holdStateMachineOperation = nil;
        }
    });
}

#pragma mark --- Client State Machine Machinery

- (CKKSResultOperation*)createOperationToFinishAttemptForClient:(CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)attempt clientName:(NSString*)clientName
{
    WEAKIFY(self);

    CKKSResultOperation* followUp = [CKKSResultOperation named:@"octagon-state-follow-up" withBlock:^{
        STRONGIFY(self);

        dispatch_sync(self.queue, ^{
            otclientnotice(self.clientScope, "Finishing state transition attempt %@", attempt);

            self.currentState = attempt.nextState;
            self.nextClientStateMachineCycleOperation = nil;

            [self _onqueueStartNextClientStateMachineOperation:clientName];
        });
    }];
    [followUp addNullableDependency:self.holdStateMachineOperation];
    [followUp addNullableDependency:attempt];
    return followUp;
}

- (void)_onqueuePokeClientStateMachine:(NSString*)clientName
{
    dispatch_assert_queue(self.queue);
    if(!self.nextClientStateMachineCycleOperation) {
        [self _onqueueStartNextClientStateMachineOperation:clientName];
    }
}

- (void)_onqueueStartNextClientStateMachineOperation:(NSString*)clientName {
    dispatch_assert_queue(self.queue);

    CKKSResultOperation<OctagonStateTransitionOperationProtocol>* nextAttempt = [self _onqueueNextClientStateMachineTransition:clientName];
    if(nextAttempt) {
        otclientnotice(self.clientScope, "Beginning client state transition attempt %@", nextAttempt);

        self.nextClientStateMachineCycleOperation = [self createOperationToFinishAttemptForClient:nextAttempt clientName:clientName];
        [self.operationQueue addOperation:self.nextClientStateMachineCycleOperation];

        [nextAttempt addNullableDependency:self.holdStateMachineOperation];
        [self.operationQueue addOperation:nextAttempt];
    }
}

- (void)handleExternalClientStateMachineRequest:(OctagonStateTransitionRequest<CKKSResultOperation<OctagonStateTransitionOperationProtocol>*>*)request client:(NSString*)clientName
{
    dispatch_sync(self.queue, ^{
        [self.stateMachineClientRequests addObject:request];

        [self _onqueuePokeClientStateMachine:clientName];
    });
}
-(BOOL) isAcceptorWaitingForFirstMessage
{
    BOOL isWaitingForFirstMessage = NO;

    if([self.currentState isEqualToString:OctagonStateAcceptorBeginClientJoin] || [self.currentState isEqualToString:OctagonStateMachineNotStarted]){
        isWaitingForFirstMessage = YES;
    }
    return isWaitingForFirstMessage;
}
#pragma mark --- Client State Machine Transitions
- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)_onqueueNextClientStateMachineTransition:(NSString*)clientName
{
    dispatch_assert_queue(self.queue);

    // Check requests: do any of them want to come from this state?
    for(OctagonStateTransitionRequest<OctagonStateTransitionOperation*>* request in self.stateMachineClientRequests) {
        if([request.sourceStates containsObject:self.currentState]) {
            CKKSResultOperation<OctagonStateTransitionOperationProtocol>* attempt = [request _onqueueStart];

            if(attempt) {
                otclientnotice(self.clientScope, "Running client %@ state machine request %@ (from %@)", clientName, request, self.currentState);
                return attempt;
            }
        }
    }
    if([self.currentState isEqualToString: OctagonStateAcceptorVoucherPrepared]){
        return [OctagonStateTransitionOperation named:@"octagon-voucher-prepared"
                                            intending:OctagonStateAcceptorDone
                                           errorState:OctagonStateError
                                  withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
            otclientnotice(self.clientScope, "moving to state done for %@", clientName);
            op.nextState = OctagonStateAcceptorDone;
        }];
    }else if([self.currentState isEqualToString: OctagonStateAcceptorDone]){
        otclientnotice(self.clientScope, "removing client connection for %@", clientName);
    }

    return nil;
}

- (void)notifyContainerChange
{
    secerror("OTCuttlefishContext: received a cuttlefish push notification (%@)", self.containerName);
    [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        secerror("octagon: Can't talk with TrustedPeersHelper, update is lost: %@", error);

    }] updateWithContainer:self.containerName
     context:self.contextID
     deviceName:nil
     serialNumber:nil
     osVersion:nil
     policyVersion:nil
     policySecrets:nil
     syncUserControllableViews:nil
     reply:^(TrustedPeersHelperPeerState* peerState, TPSyncingPolicy* policy, NSError* error) {
         if(error) {
             secerror("OTCuttlefishContext: updating errored: %@", error);
         } else {
             secerror("OTCuttlefishContext: update complete");
         }
     }];
}

#pragma mark --- External Interfaces

- (void)rpcEpoch:(OTCuttlefishContext*)cuttlefishContext
           reply:(void (^)(uint64_t epoch,
                           NSError * _Nullable error))reply
{
    OTEpochOperation* pendingOp = [[OTEpochOperation alloc] init:self.containerName
                                                       contextID:self.contextID
                                                   intendedState:OctagonStateAcceptorAwaitingIdentity
                                                      errorState:OctagonStateAcceptorDone
                                            cuttlefishXPCWrapper:cuttlefishContext.cuttlefishXPCWrapper];

    OctagonStateTransitionRequest<OTEpochOperation*>* request = [[OctagonStateTransitionRequest alloc] init:@"rpcEpoch"
                                                                                               sourceStates:[NSSet setWithArray:@[OctagonStateAcceptorBeginClientJoin]]
                                                                                                serialQueue:self.queue
                                                                                                    timeout:2*NSEC_PER_SEC
                                                                                               transitionOp:pendingOp];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"rpcEpoch-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning an epoch call: %llu  %@", pendingOp.epoch, pendingOp.error);
                                                         reply(pendingOp.epoch,
                                                               pendingOp.error);
                                                     }];
    [callback addDependency:pendingOp];
    [self.operationQueue addOperation: callback];

    [self handleExternalClientStateMachineRequest:request client:self.clientName];

    return;
}

- (void)rpcVoucher:(OTCuttlefishContext*)cuttlefishContext
            peerID:(NSString*)peerID
     permanentInfo:(NSData *)permanentInfo
  permanentInfoSig:(NSData *)permanentInfoSig
        stableInfo:(NSData *)stableInfo
     stableInfoSig:(NSData *)stableInfoSig
             reply:(void (^)(NSData* voucher, NSData* voucherSig, NSError * _Nullable error))reply
{
    OTClientVoucherOperation* pendingOp = [[OTClientVoucherOperation alloc] initWithDependencies:cuttlefishContext.operationDependencies
                                                                                   intendedState:OctagonStateAcceptorVoucherPrepared
                                                                                      errorState:OctagonStateAcceptorDone
                                                                                      deviceInfo:[cuttlefishContext prepareInformation]
                                                                                          peerID:peerID
                                                                                   permanentInfo:permanentInfo
                                                                                permanentInfoSig:permanentInfoSig
                                                                                      stableInfo:stableInfo
                                                                                   stableInfoSig:stableInfoSig];

    OctagonStateTransitionRequest<OTClientVoucherOperation*>* request = [[OctagonStateTransitionRequest alloc] init:@"rpcVoucher"
                                                                                                       sourceStates:[NSSet setWithArray:@[OctagonStateAcceptorAwaitingIdentity]]
                                                                                                        serialQueue:self.queue
                                                                                                            timeout:2*NSEC_PER_SEC

                                                                                                       transitionOp:pendingOp];
    CKKSResultOperation* callback = [CKKSResultOperation named:@"rpcVoucher-callback"
                                                     withBlock:^{
        secnotice("otrpc", "Returning a voucher call: %@, %@, %@", pendingOp.voucher, pendingOp.voucherSig, pendingOp.error);
        reply(pendingOp.voucher, pendingOp.voucherSig, pendingOp.error);
    }];
    [callback addDependency:pendingOp];
    [self.operationQueue addOperation: callback];

    [self handleExternalClientStateMachineRequest:request client:self.clientName];

    return;
}

@end
#endif
