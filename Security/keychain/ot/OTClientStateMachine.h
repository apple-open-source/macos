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
#import <ApplePushService/ApplePushService.h>
#import <Foundation/Foundation.h>

#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"

#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTSOSAdapter.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"
#import <KeychainCircle/PairingChannel.h>

NS_ASSUME_NONNULL_BEGIN

/* Piggybacking and ProximitySetup as Acceptor, Octagon only */
extern OctagonState* const OctagonStateAcceptorBeginClientJoin;
extern OctagonState* const OctagonStateAcceptorEpochPrepared;
extern OctagonState* const OctagonStateAcceptorAwaitingIdentity;
extern OctagonState* const OctagonStateAcceptorVoucherPrepared;
extern OctagonState* const OctagonStateAcceptorDone;

NSDictionary<OctagonState*, NSNumber*>* OctagonClientStateMap(void);

@interface OTClientStateMachine : NSObject

@property (readonly) id<NSXPCProxyCreating> cuttlefishXPCConnection;

@property (readonly) NSString* containerName;
@property (readonly) NSString* contextID;
@property (readonly) NSString* clientName;

@property (readonly) OctagonState* currentState;
@property NSMutableDictionary<OctagonState*, CKKSCondition*>* stateConditions;

- (instancetype)initWithContainerName:(NSString*)containerName
                            contextID:(NSString*)contextID
                            clientName:(NSString*)clientName
                           cuttlefish:(id<NSXPCProxyCreating>)cuttlefish;

- (void)startOctagonStateMachine;
- (void)notifyContainerChange;

- (void)rpcEpoch:(OTCuttlefishContext*)cuttlefishContext
           reply:(void (^)(uint64_t epoch,
                           NSError * _Nullable error))reply;

- (void)rpcVoucher:(OTCuttlefishContext*)cuttlefishContext
            peerID:(NSString*)peerID
     permanentInfo:(NSData *)permanentInfo
  permanentInfoSig:(NSData *)permanentInfoSig
        stableInfo:(NSData *)stableInfo
     stableInfoSig:(NSData *)stableInfoSig
             reply:(void (^)(NSData* voucher, NSData* voucherSig, NSError * _Nullable error))reply;
@end

NS_ASSUME_NONNULL_END
#endif

