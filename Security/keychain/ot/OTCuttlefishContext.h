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
#import <AppleFeatures/AppleFeatures.h>
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/TrustedPeersHelper/TrustedPeersHelperSpecificUser.h"

#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "OTDeviceInformation.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTClique.h"
#import "keychain/ot/OTFollowup.h"
#import "keychain/ot/OTSOSAdapter.h"
#import "keychain/ot/OTAccountsAdapter.h"
#import "keychain/ot/OTAuthKitAdapter.h"
#import "keychain/ot/OTTooManyPeersAdapter.h"
#import "keychain/ot/OTTapToRadarAdapter.h"
#import "keychain/ot/OTDeviceInformationAdapter.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"
#import <KeychainCircle/PairingChannel.h>
#import <Security/OTJoiningConfiguration.h>
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/CuttlefishXPCWrapper.h"
#import "keychain/ot/OTStashAccountSettingsOperation.h"
#import <Security/SecEscrowRequest.h>

#import <CoreCDP/CDPAccount.h>

#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ot/proto/generated_source/OTSecureElementPeerIdentity.h"
#import "keychain/ot/proto/generated_source/OTCurrentSecureElementIdentities.h"
#import "keychain/ot/proto/generated_source/OTAccountSettings.h"
#import "keychain/ot/proto/generated_source/OTWalrus.h"
#import "keychain/ot/proto/generated_source/OTWebAccess.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTMetricsSessionData : NSObject

@property (readonly, atomic, strong, nullable) NSString* flowID;
@property (readonly, atomic, strong, nullable) NSString* deviceSessionID;

-(instancetype)init NS_UNAVAILABLE;
-(instancetype)initWithFlowID:(NSString*)flowID deviceSessionID:(NSString*)deviceSessionID;
@end

@interface OTCuttlefishContext : NSObject <OctagonCuttlefishUpdateReceiver,
                                           OTAuthKitAdapterNotifier,
                                           OctagonStateMachineEngine,
                                           CKKSCloudKitAccountStateListener,
                                           CKKSPeerUpdateListener,
                                           OTDeviceInformationNameUpdateListener,
                                           OTAccountSettingsContainer>

@property (readonly) CuttlefishXPCWrapper* cuttlefishXPCWrapper;
@property (readonly) OTFollowup *followupHandler;

@property (readonly) NSString                               *containerName;
@property (readonly) NSString                               *contextID;

@property (readonly, nullable) TPSpecificUser               *activeAccount;

@property (nonatomic, strong, nullable) NSString            *pairingUUID;
@property (nonatomic, readonly) CKKSLockStateTracker        *lockStateTracker;
@property (nonatomic, readonly) OTCuttlefishAccountStateHolder* accountMetadataStore;
@property (readonly) OctagonStateMachine* stateMachine;
@property (nullable, nonatomic) CKKSNearFutureScheduler* apsRateLimiter;
@property (nullable, nonatomic) CKKSNearFutureScheduler* sosConsistencyRateLimiter;
@property (nullable, nonatomic) CKKSNearFutureScheduler* checkMetricsTrigger;

@property (atomic, strong, nullable) OTMetricsSessionData* sessionMetrics;
@property (nonatomic) OTAccountMetadataClassC_MetricsState shouldSendMetricsForOctagon;

@property (readonly, nullable) CKKSKeychainView*            ckks;

// Dependencies (for injection)
@property (readonly) id<CKKSCloudKitAccountStateTrackingProvider, CKKSOctagonStatusMemoizer> accountStateTracker;
@property (readonly) id<OTDeviceInformationAdapter> deviceAdapter;
@property (readonly) id<OTAccountsAdapter> accountsAdapter;
@property (readonly) id<OTAuthKitAdapter> authKitAdapter;
@property (readonly) id<OTPersonaAdapter> personaAdapter;
@property (readonly) id<OTSOSAdapter> sosAdapter;
@property (readonly) id<OTTooManyPeersAdapter> tooManyPeersAdapter;
@property (readonly) id<OTTapToRadarAdapter> tapToRadarAdapter;

// CKKSConditions (for testing teardowns)
@property (nullable) CKKSCondition* pendingEscrowCacheWarmup;

@property dispatch_queue_t queue;

- (instancetype)initWithContainerName:(NSString*)containerName
                            contextID:(NSString*)contextID
                        activeAccount:(TPSpecificUser* _Nullable)activeAccount
                           cuttlefish:(id<NSXPCProxyCreating>)cuttlefish
                      ckksAccountSync:(CKKSKeychainView* _Nullable)ckks
                           sosAdapter:(id<OTSOSAdapter>)sosAdapter
                      accountsAdapter:(id<OTAccountsAdapter>)accountsAdapter
                       authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                       personaAdapter:(id<OTPersonaAdapter>)personaAdapter
                  tooManyPeersAdapter:(id<OTTooManyPeersAdapter>)tooManyPeersAdapter
                    tapToRadarAdapter:(id<OTTapToRadarAdapter>)tapToRadarAdapter
                     lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                  reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                  accountStateTracker:(id<CKKSCloudKitAccountStateTrackingProvider, CKKSOctagonStatusMemoizer>)accountStateTracker
             deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter
                   apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
                   escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
                        notifierClass:(Class<CKKSNotifier>)notifierClass
                                 cdpd:(id<OctagonFollowUpControllerProtocol>)cdpd;

// Call one of these when the account state changes. OTCuttlefishContext is responsible for maintaining this state across daemon restarts.
- (BOOL)accountAvailable:(NSString*)altDSID error:(NSError**)error;
- (BOOL)accountNoLongerAvailable:(NSError**)error;
- (BOOL)idmsTrustLevelChanged:(NSError**)error;

// Call these to manipulate the "CDP-ness" of the account
// Note that there is no way to turn CDP back off again
- (OTCDPStatus)getCDPStatus:(NSError* __autoreleasing *)error;
- (BOOL)setCDPEnabled:(NSError* __autoreleasing *)error;

- (void)deviceNameUpdated;

- (void)startOctagonStateMachine;
- (void)handlePairingRestart:(OTJoiningConfiguration*)config;
- (void)clearPairingUUID;

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
          reply:(void (^)(NSError * _Nullable error))reply;

- (void)rpcReset:(CuttlefishResetReason)resetReason
           reply:(nonnull void (^)(NSError * _Nullable))reply;

- (void)rpcResetAndEstablish:(CuttlefishResetReason)resetReason
           idmsTargetContext:(NSString *_Nullable)idmsTargetContext
      idmsCuttlefishPassword:(NSString *_Nullable)idmsCuttlefishPassword
                  notifyIdMS:(bool)notifyIdMS
             accountSettings:(OTAccountSettings *_Nullable)accountSettings
                       reply:(nonnull void (^)(NSError * _Nullable))reply;

- (void)rpcResetAndEstablish:(CuttlefishResetReason)resetReason
                       reply:(nonnull void (^)(NSError * _Nullable))reply;

- (void)localReset:(nonnull void (^)(NSError * _Nullable))reply;

- (void)rpcEstablish:(nonnull NSString *)altDSID
               reply:(nonnull void (^)(NSError * _Nullable))reply;

- (void)rpcLeaveClique:(nonnull void (^)(NSError * _Nullable))reply;


- (void)joinWithBottle:(NSString*)bottleID
               entropy:(NSData *)entropy
            bottleSalt:(NSString *)bottleSalt
                 reply:(void (^)(NSError * _Nullable error))reply;

- (void)joinWithRecoveryKey:(NSString*)recoveryKey
                     reply:(void (^)(NSError * _Nullable error))reply;

- (void)joinWithCustodianRecoveryKey:(OTCustodianRecoveryKey*)crk
                              reply:(void (^)(NSError * _Nullable error))reply;

- (void)preflightJoinWithCustodianRecoveryKey:(OTCustodianRecoveryKey*)crk
                                        reply:(void (^)(NSError * _Nullable error))reply;

- (void)joinWithInheritanceKey:(OTInheritanceKey*)ik
                         reply:(void (^)(NSError * _Nullable error))reply;

- (void)preflightJoinWithInheritanceKey:(OTInheritanceKey*)ik
                                  reply:(void (^)(NSError * _Nullable error))reply;

- (void)preflightRecoverOctagonUsingRecoveryKey:(NSString *)recoveryKey
                                          reply:(void (^)(BOOL, NSError * _Nullable))reply;

- (void)getAccountMetadataWithReply:(void (^)(OTAccountMetadataClassC*_Nullable, NSError *_Nullable))reply;

- (void)rpcRemoveFriendsInClique:(NSArray<NSString*>*)peerIDs
                           reply:(void (^)(NSError * _Nullable))reply;

- (void)notifyContainerChange:(APSIncomingMessage* _Nullable)notification;
- (void)notifyContainerChangeWithUserInfo:(NSDictionary* _Nullable)userInfo;

- (void)rpcStatus:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply;
- (void)rpcFetchEgoPeerID:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))reply;
- (void)rpcTrustStatus:(OTOperationConfiguration *)configuration
                 reply:(void (^)(CliqueStatus status,
                                 NSString* _Nullable peerID,
                                 NSDictionary<NSString*, NSNumber*>* _Nullable peerCountByModelID,
                                 BOOL isExcluded,
                                 BOOL isLocked,
                                 NSError * _Nullable))reply;
- (void)rpcFetchDeviceNamesByPeerID:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply;
- (void)rpcFetchAllViableBottlesFromSource:(OTEscrowRecordFetchSource)source
                                     reply:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs,
                                                     NSArray<NSString*>* _Nullable sortedPartialEscrowRecordIDs,
                                                     NSError* _Nullable error))reply;
- (void)rpcFetchAllViableEscrowRecordsFromSource:(OTEscrowRecordFetchSource)source
                                           reply:(void (^)(NSArray<NSData*>* _Nullable records,
                                                           NSError* _Nullable error))reply;
- (void)rpcInvalidateEscrowCache:(void (^)(NSError* _Nullable error))reply;

- (void)fetchEscrowContents:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply;
- (void)rpcSetRecoveryKey:(NSString*)recoveryKey reply:(void (^)(NSError * _Nullable error))reply;
- (void)rpcCreateCustodianRecoveryKeyWithUUID:(NSUUID *_Nullable)uuid
                                        reply:(void (^)(OTCustodianRecoveryKey *_Nullable crk, NSError *_Nullable error))reply;
- (void)rpcCreateInheritanceKeyWithUUID:(NSUUID *_Nullable)uuid
                                  reply:(void (^)(OTInheritanceKey *_Nullable ik, NSError *_Nullable error))reply;
- (void)rpcGenerateInheritanceKeyWithUUID:(NSUUID *_Nullable)uuid
                                  reply:(void (^)(OTInheritanceKey *_Nullable ik, NSError *_Nullable error))reply;
- (void)rpcStoreInheritanceKeyWithIK:(OTInheritanceKey*)ik
                                  reply:(void (^)(NSError *_Nullable error))reply;
- (void)rpcRemoveCustodianRecoveryKeyWithUUID:(NSUUID *)uuid
                                        reply:(void (^)(NSError *_Nullable error))reply;
- (void)rpcRemoveInheritanceKeyWithUUID:(NSUUID *)uuid
                                  reply:(void (^)(NSError *_Nullable error))reply;
- (void)rpcCheckCustodianRecoveryKeyWithUUID:(NSUUID *)uuid
                                       reply:(void (^)(bool exists, NSError *_Nullable error))reply;
- (void)rpcCheckInheritanceKeyWithUUID:(NSUUID *)uuid
                                 reply:(void (^)(bool exists, NSError *_Nullable error))reply;

- (void)rpcRefetchCKKSPolicy:(void (^)(NSError * _Nullable error))reply;

- (void)rpcFetchUserControllableViewsSyncingStatus:(void (^)(BOOL areSyncing, NSError* _Nullable error))reply;
- (void)rpcSetUserControllableViewsSyncingStatus:(BOOL)status reply:(void (^)(BOOL areSyncing, NSError* _Nullable error))reply;

- (void)rpcSetLocalSecureElementIdentity:(OTSecureElementPeerIdentity*)secureElementIdentity
                                reply:(void (^)(NSError* _Nullable))reply;
- (void)rpcRemoveLocalSecureElementIdentityPeerID:(NSData*)sePeerID
                                            reply:(void (^)(NSError* _Nullable))reply;
- (void)rpcFetchTrustedSecureElementIdentities:(void (^)(OTCurrentSecureElementIdentities* _Nullable currentSet,
                                                         NSError* _Nullable replyError))reply;

- (void)rpcTlkRecoverabilityForEscrowRecordData:(NSData*)recordData
                                         source:(OTEscrowRecordFetchSource)source
                                          reply:(void (^)(NSArray<NSString*>* views,
                                                          NSError* replyError))reply;

- (void)rpcSetAccountSetting:(OTAccountSettings*)setting
                       reply:(void (^)(NSError* _Nullable))reply;

- (void)rpcFetchAccountSettings:(void (^)(OTAccountSettings* _Nullable setting, NSError* _Nullable replyError))reply;
- (void)rpcAccountWideSettingsWithForceFetch:(bool)forceFetch reply:(void (^)(OTAccountSettings* _Nullable setting, NSError* _Nullable replyError))reply;

- (void)rpcWaitForPriorityViewKeychainDataRecovery:(void (^)(NSError* _Nullable replyError))reply NS_SWIFT_NAME(rpcWaitForPriorityViewKeychainDataRecovery(reply:));;

- (void)requestTrustedDeviceListRefresh;

- (OTDeviceInformation*)prepareInformation;

// called when circle changed notification fires
- (void)moveToCheckTrustedState;

- (OTOperationDependencies*)operationDependencies;

- (void)waitForOctagonUpgrade:(void (^)(NSError* _Nullable error))reply NS_SWIFT_NAME(waitForOctagonUpgrade(reply:));

- (BOOL)waitForReady:(int64_t)timeOffset;

- (void)rpcIsRecoveryKeySet:(void (^)(BOOL isSet, NSError * _Nullable error))reply;
- (void)rpcRemoveRecoveryKey:(void (^)(BOOL removed, NSError * _Nullable error))reply;
- (void)areRecoveryKeysDistrusted:(void (^)(BOOL, NSError *_Nullable))reply;

- (void)rpcFetchTotalCountOfTrustedPeers:(void (^)(NSNumber* count, NSError* replyError))reply;

- (void)rerollWithReply:(void (^)(NSError *_Nullable error))reply;

// For testing.
- (OTAccountMetadataClassC_AccountState)currentMemoizedAccountState;
- (OTAccountMetadataClassC_TrustState)currentMemoizedTrustState;
- (NSDate* _Nullable) currentMemoizedLastHealthCheck;
- (void)checkTrustStatusAndPostRepairCFUIfNecessary:(void (^ _Nullable)(CliqueStatus status, BOOL posted, BOOL hasIdentity, BOOL isLocked, NSError * _Nullable error))reply;
- (void)rpcResetAccountCDPContentsWithIdmsTargetContext:(NSString *_Nullable)idmsTargetContext
                                 idmsCuttlefishPassword:(NSString*_Nullable)idmsCuttlefishPassword
                                             notifyIdMS:(bool)notifyIdMS
                                                  reply:(void (^)(NSError* _Nullable error))reply;
- (BOOL)checkAllStateCleared;
- (void)clearCKKS;
- (void)setMachineIDOverride:(NSString*)machineID;

@property (nullable) TPPolicyVersion* policyOverride;

// Octagon Health Check Helpers
- (void)checkOctagonHealth:(BOOL)skipRateLimitingCheck repair:(BOOL)repair reply:(void (^)(TrustedPeersHelperHealthCheckResult *_Nullable results, NSError * _Nullable error))reply;

// For reporting
- (BOOL)machineIDOnMemoizedList:(NSString*)machineID error:(NSError**)error NS_SWIFT_NOTHROW;
- (TrustedPeersHelperEgoPeerStatus* _Nullable)egoPeerStatus:(NSError**)error;

- (BOOL)fetchSendingMetricsPermitted:(NSError**)error;
- (BOOL)persistSendingMetricsPermitted:(BOOL)sendingMetricsPermitted error:(NSError**)error;

@end

@interface OTCuttlefishContext (Testing)
- (void)resetCKKS:(CKKSKeychainView*)view NS_SWIFT_NAME(reset(ckks:));
@end

NS_ASSUME_NONNULL_END
#endif // OTCUTTLEFISH_CONTEXT
#endif

