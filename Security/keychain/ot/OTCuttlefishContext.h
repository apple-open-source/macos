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
#ifndef OTCUTTLEFISH_CONTEXT
#define OTCUTTLEFISH_CONTEXT

#import <ApplePushService/ApplePushService.h>
#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "OTDeviceInformation.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTClique.h"
#import "keychain/ot/OTFollowup.h"
#import "keychain/ot/OTSOSAdapter.h"
#import "keychain/ot/OTAuthKitAdapter.h"
#import "keychain/ot/OTDeviceInformationAdapter.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"
#import <KeychainCircle/PairingChannel.h>
#import "keychain/ot/OTJoiningConfiguration.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/escrowrequest/Framework/SecEscrowRequest.h"

#import <CoreCDP/CDPAccount.h>

#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKeychainView.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTCuttlefishContext : NSObject <OctagonCuttlefishUpdateReceiver,
                                           OTAuthKitAdapterNotifier,
                                           OctagonStateMachineEngine,
                                           CKKSCloudKitAccountStateListener,
                                           CKKSPeerUpdateListener>

@property (readonly) id<NSXPCProxyCreating> cuttlefishXPCConnection;
@property (readonly) OTFollowup *followupHandler;

@property (readonly) NSString                               *containerName;
@property (readonly) NSString                               *contextID;
@property (readonly) NSString                               *altDSID;
@property (nonatomic,strong) NSString                       *_Nullable pairingUUID;
@property (nonatomic, readonly) CKKSLockStateTracker        *lockStateTracker;
@property (nonatomic, readonly) OTCuttlefishAccountStateHolder* accountMetadataStore;
@property (readonly) OctagonStateMachine* stateMachine;
@property (readonly) BOOL postedRepairCFU;
@property (readonly) BOOL postedEscrowRepairCFU;
@property (readonly) BOOL postedRecoveryKeyCFU;
@property (nullable, nonatomic) CKKSNearFutureScheduler* apsRateLimiter;

@property (readonly, nullable) CKKSViewManager*             viewManager;

// Dependencies (for injection)
@property id<OTAuthKitAdapter> authKitAdapter;

@property dispatch_queue_t queue;

- (instancetype)initWithContainerName:(NSString*)containerName
                            contextID:(NSString*)contextID
                           cuttlefish:(id<NSXPCProxyCreating>)cuttlefish
                           sosAdapter:(id<OTSOSAdapter>)sosAdapter
                       authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                      ckksViewManager:(CKKSViewManager* _Nullable)viewManager
                     lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                  accountStateTracker:(id<CKKSCloudKitAccountStateTrackingProvider, CKKSOctagonStatusMemoizer>)accountStateTracker
             deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter
                   apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
                   escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
                                 cdpd:(id<OctagonFollowUpControllerProtocol>)cdpd;

// Call one of these when the account state changes. OTCuttlefishContext is responsible for maintaining this state across daemon restarts.
- (BOOL)accountAvailable:(NSString*)altDSID error:(NSError**)error;
- (BOOL)accountNoLongerAvailable:(NSError**)error;
- (BOOL)idmsTrustLevelChanged:(NSError**)error;

- (void)startOctagonStateMachine;
- (void)handlePairingRestart:(OTJoiningConfiguration*)config;

- (void)rpcPrepareIdentityAsApplicantWithConfiguration:(OTJoiningConfiguration*)config
                                                 epoch:(uint64_t)epoch
                                              reply:(void (^)(NSString * _Nullable peerID,
                                                              NSData * _Nullable permanentInfo,
                                                              NSData * _Nullable permanentInfoSig,
                                                              NSData * _Nullable stableInfo,
                                                              NSData * _Nullable stableInfoSig,
                                                              NSError * _Nullable error))reply;
- (void)rpcJoin:(NSData*)vouchData
       vouchSig:(NSData*)vouchSig
preapprovedKeys:(NSArray<NSData*>* _Nullable)preapprovedKeys
          reply:(void (^)(NSError * _Nullable error))reply;

- (void)rpcResetAndEstablish:(nonnull void (^)(NSError * _Nullable))reply;

- (void)localReset:(nonnull void (^)(NSError * _Nullable))reply;

- (void)rpcEstablish:(nonnull NSString *)altDSID
               reply:(nonnull void (^)(NSError * _Nullable))reply;

- (void)rpcLeaveClique:(nonnull void (^)(NSError * _Nullable))reply;


-(void)joinWithBottle:(NSString*)bottleID
              entropy:(NSData *)entropy
           bottleSalt:(NSString *)bottleSalt
                reply:(void (^)(NSError * _Nullable error))reply;

-(void)joinWithRecoveryKey:(NSString*)recoveryKey
                     reply:(void (^)(NSError * _Nullable error))reply;

- (void)rpcRemoveFriendsInClique:(NSArray<NSString*>*)peerIDs
                           reply:(void (^)(NSError*))reply;

- (void)notifyContainerChange:(APSIncomingMessage* _Nullable)notification;
- (void)notifyContainerChangeWithUserInfo:(NSDictionary*)userInfo;

- (void)rpcStatus:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply;
- (void)rpcFetchEgoPeerID:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))reply;
- (void)rpcTrustStatus:(OTOperationConfiguration *)configuration
                 reply:(void (^)(CliqueStatus status,
                                 NSString* _Nullable peerID,
                                 NSDictionary<NSString*, NSNumber*>* _Nullable peerCountByModelID,
                                 BOOL isExcluded,
                                 NSError * _Nullable))reply;
- (void)rpcFetchDeviceNamesByPeerID:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply;
- (void)rpcFetchAllViableBottles:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs, NSArray<NSString*>* _Nullable sortedPartialEscrowRecordIDs, NSError* _Nullable error))reply;
- (void)fetchEscrowContents:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply;
- (void)rpcSetRecoveryKey:(NSString*)recoveryKey reply:(void (^)(NSError * _Nullable error))reply;

- (void)requestTrustedDeviceListRefresh;

- (OTDeviceInformation*)prepareInformation;

// called when circle changed notification fires
- (void) moveToCheckTrustedState;

- (OTOperationDependencies*)operationDependencies;

- (void)attemptSOSUpgrade:(void (^)(NSError* _Nullable error))reply;

- (void)waitForOctagonUpgrade:(void (^)(NSError* error))reply;

- (void)clearPendingCFUFlags;

// For testing.
- (void)setPostedBool:(BOOL)posted;
- (OTAccountMetadataClassC_AccountState)currentMemoizedAccountState;
- (OTAccountMetadataClassC_TrustState)currentMemoizedTrustState;
- (NSDate* _Nullable) currentMemoizedLastHealthCheck;
- (void) checkTrustStatusAndPostRepairCFUIfNecessary:(void (^ _Nullable)(CliqueStatus status, BOOL posted, BOOL hasIdentity, NSError * _Nullable error))reply;
- (void) setAccountStateHolder:(OTCuttlefishAccountStateHolder*)accountMetadataStore;

// Octagon Health Check Helpers
- (void)checkOctagonHealth:(BOOL)skipRateLimitingCheck reply:(void (^)(NSError * _Nullable error))reply;
- (BOOL)postRepairCFU:(NSError**)error;
- (void)postConfirmPasscodeCFU:(NSError**)error;
- (void)postRecoveryKeyCFU:(NSError**)error;

@end

NS_ASSUME_NONNULL_END
#endif // OTCUTTLEFISH_CONTEXT
#endif

