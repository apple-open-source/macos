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

#import <Foundation/Foundation.h>
#import <TrustedPeers/TrustedPeers.h>
#import <objc/runtime.h>

#import <AppleFeatures/AppleFeatures.h>

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocolTransitObjects.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperSpecificUser.h"
#import "keychain/ckks/CKKSKeychainBackedKey.h"
#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"

#import "keychain/ot/OTConstants.h"

@class OTAccountSettings;
@class OTEscrowMoveRequestContext;
NS_ASSUME_NONNULL_BEGIN

// Any client hoping to use the TrustedPeersHelperProtocol should have an entitlement
// 'com.apple.private.trustedpeershelper.client' set to boolean YES.

@interface TrustedPeersHelperPeerState : NSObject <NSSecureCoding>
@property (nullable) NSString* peerID;
@property BOOL identityIsPreapproved;
@property TPPeerStatus peerStatus;
@property BOOL memberChanges;
@property BOOL unknownMachineIDsPresent;
@property (nullable) NSString* osVersion;
@property (nullable) TPPBPeerStableInfoSetting* walrus;
@property (nullable) TPPBPeerStableInfoSetting* webAccess;

- (instancetype)initWithPeerID:(NSString* _Nullable)peerID
                 isPreapproved:(BOOL)isPreapproved
                        status:(TPPeerStatus)peerStatus
                 memberChanges:(BOOL)memberChanges
             unknownMachineIDs:(BOOL)unknownMachineIDs
                     osVersion:(NSString * _Nullable)osVersion
                        walrus:(TPPBPeerStableInfoSetting* _Nullable)walrus
                     webAccess:(TPPBPeerStableInfoSetting* _Nullable) webAccess
;
@end

@interface TrustedPeersHelperPeer : NSObject <NSSecureCoding>
@property (nullable) NSString* peerID;
@property (nullable) NSData* signingSPKI;
@property (nullable) NSData* encryptionSPKI;
@property (nullable) NSSet<NSString*>* viewList;

@property (nullable) TPPBSecureElementIdentity* secureElementIdentity;

- (instancetype)initWithPeerID:(NSString*)peerID
                   signingSPKI:(NSData*)signingSPKI
                encryptionSPKI:(NSData*)encryptionSPKI
         secureElementIdentity:(TPPBSecureElementIdentity* _Nullable)secureElementIdentity
                      viewList:(NSSet<NSString*>*)viewList;
@end

@interface TrustedPeersHelperEgoPeerStatus : NSObject <NSSecureCoding>
@property TPPeerStatus egoStatus;
@property NSString* _Nullable egoPeerID;
@property NSString* _Nullable egoPeerMachineID;
@property (assign) uint64_t numberOfPeersInOctagon;

// Note: this field does not include untrusted peers
@property NSDictionary<NSString*, NSNumber*>* viablePeerCountsByModelID;

// Note: this field does include untrusted peers
@property NSDictionary<NSString*, NSNumber*>* peerCountsByMachineID;

@property BOOL isExcluded;
@property BOOL isLocked;

- (instancetype)initWithEgoPeerID:(NSString* _Nullable)egoPeerID
                 egoPeerMachineID:(NSString* _Nullable)egoPeerMachineID
                           status:(TPPeerStatus)egoStatus
        viablePeerCountsByModelID:(NSDictionary<NSString*, NSNumber*>*)viablePeerCountsByModelID
            peerCountsByMachineID:(NSDictionary<NSString*, NSNumber*>*)peerCountsByMachineID
                       isExcluded:(BOOL)isExcluded
                         isLocked:(BOOL)isLocked;

@end

@interface TrustedPeersHelperCustodianRecoveryKey : NSObject <NSSecureCoding>
@property NSString* uuid;
@property (nullable) NSData* encryptionKey;
@property (nullable) NSData* signingKey;
@property (nullable) NSString* recoveryString;
@property (nullable) NSString* salt;
@property TPPBCustodianRecoveryKey_Kind kind;

- (instancetype)initWithUUID:(NSString*)uuid
               encryptionKey:(NSData* _Nullable)encryptionKey
                  signingKey:(NSData* _Nullable)signingKey
              recoveryString:(NSString* _Nullable)recoveryString
                        salt:(NSString* _Nullable)salt
                        kind:(TPPBCustodianRecoveryKey_Kind)kind;
@end

@interface TrustedPeersHelperTLKRecoveryResult : NSObject <NSSecureCoding>
@property NSSet<NSString*>* successfulKeysRecovered;
@property int64_t totalTLKSharesRecovered;
@property NSDictionary<NSString*, NSArray<NSError*>*>* tlkRecoveryErrors;

- (instancetype)initWithSuccessfulKeyUUIDs:(NSSet<NSString*>*)successfulKeysRecovered
                   totalTLKSharesRecovered:(int64_t)totalTLKSharesRecovered
                         tlkRecoveryErrors:(NSDictionary<NSString*, NSArray<NSError*>*>*)tlkRecoveryErrors;
@end

@interface TrustedPeersHelperHealthCheckResult: NSObject<NSSecureCoding>
@property bool postRepairCFU;
@property bool postEscrowCFU;
@property bool resetOctagon;
@property bool leaveTrust;
@property bool reroll;
@property (nullable) OTEscrowMoveRequestContext* moveRequest;
@property uint64_t totalEscrowRecords;
@property uint64_t collectableEscrowRecords;
@property uint64_t collectedEscrowRecords;
@property bool escrowRecordGarbageCollectionEnabled;
@property uint64_t totalTlkShares;
@property uint64_t collectableTlkShares;
@property uint64_t collectedTlkShares;
@property bool tlkShareGarbageCollectionEnabled;
@property uint64_t totalPeers;
@property uint64_t trustedPeers;
@property uint64_t superfluousPeers;
@property uint64_t peersCleanedup;
@property bool superfluousPeersCleanupEnabled;

- (instancetype)initWithPostRepairCFU:(bool)postRepairCFU
                        postEscrowCFU:(bool)postEscrowCFU
                         resetOctagon:(bool)resetOctagon
                           leaveTrust:(bool)leaveTrust
                               reroll:(bool)reroll
                          moveRequest:(OTEscrowMoveRequestContext* _Nullable)moveRequest
                   totalEscrowRecords:(uint64_t)totalEscrowRecords
             collectableEscrowRecords:(uint64_t)collectableEscrowRecords
               collectedEscrowRecords:(uint64_t)collectedEscrowRecords
 escrowRecordGarbageCollectionEnabled:(bool)escrowRecordGarbageCollectionEnabled
                       totalTlkShares:(uint64_t)totalTlkShares
                 collectableTlkShares:(uint64_t)collectableTlkShares
                   collectedTlkShares:(uint64_t)collectedTlkShares
     tlkShareGarbageCollectionEnabled:(bool)tlkShareGarbageCollectionEnabled
                           totalPeers:(uint64_t)totalPeers
                         trustedPeers:(uint64_t)trustedPeers
                     superfluousPeers:(uint64_t)superfluousPeers
                       peersCleanedup:(uint64_t)peersCleanedup
       superfluousPeersCleanupEnabled:(bool)superfluousPeersCleanupEnabled;

- (NSDictionary*)dictionaryRepresentation;
@end

@interface CuttlefishPCSServiceIdentifier : NSObject <NSSecureCoding>
@property (nullable) NSNumber* PCSServiceID;
@property (nullable) NSData* PCSPublicKey;
@property (nullable) NSString* zoneID;

- (instancetype)init:(NSNumber*)PCSServiceID
        PCSPublicKey:(NSData*)PCSPublicKey
              zoneID:(NSString*)zoneID;

@end

@interface CuttlefishPCSIdentity : NSObject <NSSecureCoding>
@property CuttlefishPCSServiceIdentifier* service;
@property CKRecord* item;

- (instancetype)init:(CuttlefishPCSServiceIdentifier*)service
                item:(CKRecord*)item;

@end

@interface CuttlefishCurrentItemSpecifier : NSObject <NSSecureCoding>
@property NSString* zoneID;
@property NSString* itemPtrName;

- (instancetype)init:(NSString*)itemPtrName
              zoneID:(NSString*)zoneID;

@end

@interface CuttlefishCurrentItem : NSObject <NSSecureCoding>
@property CuttlefishCurrentItemSpecifier* itemPtr;
@property CKRecord* item;

- (instancetype)init:(CuttlefishCurrentItemSpecifier*)itemPtr
                item:(CKRecord*)item;

@end

// This protocol describes the interface of the TrustedPeersHelper XPC service.
@protocol TrustedPeersHelperProtocol

// This is used by a unit test which exercises the XPC-service plumbing.
- (void)pingWithReply:(void (^)(void))reply;

- (void)dumpWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                       reply:(void (^)(NSDictionary * _Nullable, NSError * _Nullable))reply;

- (void)honorIDMSListChangesForSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                      reply:(void (^)(NSString * _Nullable, NSError * _Nullable))reply;

- (void)octagonPeerIDGivenBottleIDWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                          bottleID:(NSString*)bottleID
                                             reply:(void (^)(NSString * _Nullable, NSError * _Nullable))reply;

- (void)trustedDeviceNamesByPeerIDWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                      reply:(void (^)(NSDictionary<NSString*, NSString*> * _Nullable, NSError * _Nullable))reply;

- (void)departByDistrustingSelfWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                          reply:(void (^)(NSError * _Nullable))reply;

- (void)distrustPeerIDsWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                peerIDs:(NSSet<NSString*>*)peerIDs
                                  reply:(void (^)(NSError * _Nullable))reply;

- (void)dropPeerIDsWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                            peerIDs:(NSSet<NSString*>*)peerIDs
                              reply:(void (^)(NSError * _Nullable))reply;

- (void)trustStatusWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                              reply:(void (^)(TrustedPeersHelperEgoPeerStatus *status,
                                              NSError* _Nullable error))reply;

- (void)resetWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                  resetReason:(CuttlefishResetReason)reason
            idmsTargetContext:(NSString*_Nullable)idmsTargetContext
       idmsCuttlefishPassword:(NSString*_Nullable)idmsCuttlefishPassword
                   notifyIdMS:(bool)notifyIdMS
              internalAccount:(bool)internalAccount
                  demoAccount:(bool)demoAccount
                        reply:(void (^)(NSError * _Nullable error))reply;

- (void)performCKServerUnreadableDataRemovalWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                             internalAccount:(BOOL)internalAccount
                                                 demoAccount:(BOOL)demoAccount
                                                       reply:(void (^)(NSError * _Nullable error))reply;

- (void)localResetWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                             reply:(void (^)(NSError * _Nullable error))reply;

// The following three machine ID list manipulation functions do not attempt to apply the results to the model
// If you'd like that to occur, please call update()

// TODO: how should we communicate TLK rolling when the update() call will remove a peer?
// <rdar://problem/46633449> Octagon: must be able to roll TLKs when a peer departs due to machine ID list

// listDifferences: False if the allowedMachineIDs list passed in exactly matches the previous state,
//                  True if there were any differences

- (void)setAllowedMachineIDsWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                           allowedMachineIDs:(NSSet<NSString *> *)allowedMachineIDs
                       userInitiatedRemovals:(NSSet<NSString *> * _Nullable)userInitiatedRemovals
                             evictedRemovals:(NSSet<NSString *> * _Nullable)evictedRemovals
                       unknownReasonRemovals:(NSSet<NSString *> * _Nullable)unknownReasonRemovals
                        honorIDMSListChanges:(BOOL)honorIDMSListChanges
                                     version:(NSString* _Nullable)version
                                      flowID:(NSString * _Nullable)flowID
                             deviceSessionID:(NSString * _Nullable)deviceSessionID
                              canSendMetrics:(BOOL)canSendMetrics
                                     altDSID:(NSString * _Nullable)altDSID
                           trustedDeviceHash:(NSString * _Nullable)trustedDeviceHash
                           deletedDeviceHash:(NSString * _Nullable)deletedDeviceHash
               trustedDevicesUpdateTimestamp:(NSNumber * _Nullable)trustedDevicesUpdateTimestamp
                                       reply:(void (^)(BOOL listDifferences, NSError * _Nullable error))reply;

// Tell TPH that we were unable to fetch the TDL, and so it shouldn't enforce the list until setAllowedMachineIDsWithSpecificUser is called
- (void)markTrustedDeviceListFetchFailed:(TPSpecificUser* _Nullable)specificUser
                                   reply:(void (^)(NSError * _Nullable error))reply;

- (void)fetchAllowedMachineIDsWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                         reply:(void (^)(NSSet<NSString*>* _Nullable machineIDs, NSError* _Nullable error))reply;

- (void)fetchEgoEpochWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                reply:(void (^)(unsigned long long epoch,
                                                NSError * _Nullable error))reply;

- (void)prepareWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                          epoch:(unsigned long long)epoch
                      machineID:(NSString *)machineID
                     bottleSalt:(NSString *)bottleSalt
                       bottleID:(NSString *)bottleID
                        modelID:(NSString *)modelID
                     deviceName:(nullable NSString *)deviceName
                   serialNumber:(nullable NSString *)serialNumber
                      osVersion:(NSString *)osVersion
                  policyVersion:(nullable TPPolicyVersion *)policyVersion
                  policySecrets:(nullable NSDictionary<NSString*,NSData*> *)policySecrets
      syncUserControllableViews:(TPPBPeerStableInfoUserControllableViewStatus)syncUserControllableViews
          secureElementIdentity:(nullable TPPBSecureElementIdentity*)secureElementIdentity
                        setting:(nullable OTAccountSettings*)setting
    signingPrivKeyPersistentRef:(nullable NSData *)spkPr
        encPrivKeyPersistentRef:(nullable NSData*)epkPr
                          reply:(void (^)(NSString * _Nullable peerID,
                                          NSData * _Nullable permanentInfo,
                                          NSData * _Nullable permanentInfoSig,
                                          NSData * _Nullable stableInfo,
                                          NSData * _Nullable stableInfoSig,
                                          TPSyncingPolicy* _Nullable syncingPolicy,
                                          NSError * _Nullable error))reply;

- (void)prepareInheritancePeerWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                         epoch:(unsigned long long)epoch
                                     machineID:(NSString *)machineID
                                    bottleSalt:(NSString *)bottleSalt
                                      bottleID:(NSString *)bottleID
                                       modelID:(NSString *)modelID
                                    deviceName:(nullable NSString *)deviceName
                                  serialNumber:(nullable NSString *)serialNumber
                                     osVersion:(NSString *)osVersion
                                 policyVersion:(nullable TPPolicyVersion *)policyVersion
                                 policySecrets:(nullable NSDictionary<NSString*,NSData*> *)policySecrets
                     syncUserControllableViews:(TPPBPeerStableInfoUserControllableViewStatus)syncUserControllableViews
                         secureElementIdentity:(nullable TPPBSecureElementIdentity*)secureElementIdentity
                   signingPrivKeyPersistentRef:(nullable NSData *)spkPr
                       encPrivKeyPersistentRef:(nullable NSData*)epkPr
                                           crk:(TrustedPeersHelperCustodianRecoveryKey*)crk
                                         reply:(void (^)(NSString * _Nullable peerID,
                                                         NSData * _Nullable permanentInfo,
                                                         NSData * _Nullable permanentInfoSig,
                                                         NSData * _Nullable stableInfo,
                                                         NSData * _Nullable stableInfoSig,
                                                         TPSyncingPolicy* _Nullable syncingPolicy,
                                                         NSString * _Nullable recoveryKeyID,
                                                         NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                         NSError * _Nullable error))reply;

// If there already are existing CKKSViews, please pass in their key sets anyway.
// This function will create a self TLK Share for those TLKs.
- (void)establishWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                         ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)viewKeySets
                        tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                  preapprovedKeys:(nullable NSArray<NSData*> *)preapprovedKeys
                            reply:(void (^)(NSString * _Nullable peerID,
                                            NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                            TPSyncingPolicy* _Nullable syncingPolicy,
                                            NSError * _Nullable error))reply;

// Returns a voucher for the given peer ID using our own identity
// If TLK CKKSViewKeys are given, TLKShares will be created and uploaded for this new peer before this call returns.
- (void)vouchWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                       peerID:(NSString *)peerID
                permanentInfo:(NSData *)permanentInfo
             permanentInfoSig:(NSData *)permanentInfoSig
                   stableInfo:(NSData *)stableInfo
                stableInfoSig:(NSData *)stableInfoSig
                     ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)viewKeySets
                       flowID:(NSString * _Nullable)flowID
              deviceSessionID:(NSString * _Nullable)deviceSessionID
               canSendMetrics:(BOOL)canSendMetrics
                        reply:(void (^)(NSData * _Nullable voucher,
                                        NSData * _Nullable voucherSig,
                                        NSError * _Nullable error))reply;

// Preflighting a vouch will return the peer ID associated with the bottle you will be recovering, as well as
// the syncing policy used by that peer, and,
// You can then use that peer ID to filter the tlkshares provided to vouchWithBottle.
// If TPH had to refetch anything from the network, it will report that fact as refetchNeeded.
- (void)preflightVouchWithBottleWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                        bottleID:(NSString*)bottleID
                                           reply:(void (^)(NSString* _Nullable peerID,
                                                           TPSyncingPolicy* _Nullable syncingPolicy,
                                                           BOOL refetchWasNeeded,
                                                           NSError * _Nullable error))reply;

// Returns a voucher for our own identity, created by the identity inside this bottle
- (void)vouchWithBottleWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                               bottleID:(NSString*)bottleID
                                entropy:(NSData*)entropy
                             bottleSalt:(NSString*)bottleSalt
                              tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                                  reply:(void (^)(NSData * _Nullable voucher,
                                                  NSData * _Nullable voucherSig,
                                                  NSArray<CKKSTLKShare*>* _Nullable newSelfTLKShares,
                                                  TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                  NSError * _Nullable error))reply;

// Preflighting a vouch will return the RK ID, view list and policy associated with the RK you will be recovering.
// You can then use that peer ID to filter the tlkshares provided to vouchWithRecoveryKey.
- (void)preflightVouchWithRecoveryKeyWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                          recoveryKey:(NSString*)recoveryKey
                                                 salt:(NSString*)salt
                                                reply:(void (^)(NSString* _Nullable recoveryKeyID,
                                                                TPSyncingPolicy* _Nullable syncingPolicy,
                                                                NSError * _Nullable error))reply;

// Preflighting a vouch will return the RK ID, view list and policy associated with the RK you will be recovering.
// You can then use that peer ID to filter the tlkshares provided to vouchWithRecoveryKey.
- (void)preflightVouchWithCustodianRecoveryKeyWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                                           crk:(TrustedPeersHelperCustodianRecoveryKey*)crk
                                                         reply:(void (^)(NSString* _Nullable recoveryKeyID,
                                                                         TPSyncingPolicy* _Nullable syncingPolicy,
                                                                         NSError * _Nullable error))reply;

// Returns a voucher for our own identity, using recovery key
- (void)vouchWithRecoveryKeyWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                 recoveryKey:(NSString*)recoveryKey
                                        salt:(NSString*)salt
                                   tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                                       reply:(void (^)(NSData * _Nullable voucher,
                                                       NSData * _Nullable voucherSig,
                                                       NSArray<CKKSTLKShare*>* _Nullable newSelfTLKShares,
                                                       TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                       NSError * _Nullable error))reply;

//returns set of new tlkShares for inheritor
- (void)recoverTLKSharesForInheritorWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                                 crk:(TrustedPeersHelperCustodianRecoveryKey*)crk
                                           tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                                               reply:(void (^)(NSArray<CKKSTLKShare*>* _Nullable newSelfTLKShares,
                                                               TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                               NSError * _Nullable error))reply;

// Returns a voucher for our own identity, using custodian recovery key
- (void)vouchWithCustodianRecoveryKeyWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                                  crk:(TrustedPeersHelperCustodianRecoveryKey*)crk
                                            tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                                                reply:(void (^)(NSData * _Nullable voucher,
                                                                NSData * _Nullable voucherSig,
                                                                NSArray<CKKSTLKShare*>* _Nullable newSelfTLKShares,
                                                                TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                                NSError * _Nullable error))reply;

// Returns a voucher for our own identity, reroll
- (void)vouchWithRerollWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                              oldPeerID:(NSString*)oldPeerID
                              tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                                  reply:(void (^)(NSData * _Nullable voucher,
                                                  NSData * _Nullable voucherSig,
                                                  NSArray<CKKSTLKShare*>* _Nullable newSelfTLKShares,
                                                  TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                  NSError * _Nullable error))reply;

// As of right now, join and attemptPreapprovedJoin will upload TLKShares for any TLKs that this peer already has.
// Note that in The Future, a device might decide to join an existing Octagon set while introducing a new view.
// These interfaces will have to change...
- (void)joinWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                 voucherData:(NSData *)voucherData
                  voucherSig:(NSData *)voucherSig
                    ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)viewKeySets
                   tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
             preapprovedKeys:(nullable NSArray<NSData*> *)preapprovedKeys
                      flowID:(NSString * _Nullable)flowID
             deviceSessionID:(NSString * _Nullable)deviceSessionID
              canSendMetrics:(BOOL)canSendMetrics
                       reply:(void (^)(NSString * _Nullable peerID,
                                       NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                       TPSyncingPolicy* _Nullable syncingPolicy,
                                       NSError * _Nullable error))reply;

// Preflighting a preapproved join suggests whether or not you expect to succeed in an immediate preapprovedJoin() call
// This only inspects the Octagon model, and ignores the trusted device list, so that you can preflight the preapprovedJoin()
// before fetching that list.
// This will return YES if there are no existing peers, or if the existing peers preapprove your prepared identity, and
//   you are intending to trust at least one preapproving peer (so that you don't stomp all over everyone else at join time).
// This will return NO otherwise.
- (void)preflightPreapprovedJoinWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                 preapprovedKeys:(nullable NSArray<NSData*> *)preapprovedKeys
                                           reply:(void (^)(BOOL launchOkay,
                                                           NSError * _Nullable error))reply;

// A preapproved join might do a join, but it also might do an establish.
// Therefore, it needs all the TLKs and TLKShares as establish does
- (void)attemptPreapprovedJoinWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                      ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)ckksKeys
                                     tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                               preapprovedKeys:(nullable NSArray<NSData*> *)preapprovedKeys
                                         reply:(void (^)(NSString * _Nullable peerID,
                                                         NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                         TPSyncingPolicy* _Nullable syncingPolicy,
                                                         NSError * _Nullable error))reply;

// TODO: if the new policy causes someone to lose access to a view, how should this API work?
// syncUserControllableViews should contain the raw value of the TPPBPeerStableInfoUserControllableViewStatus enum, or be nil
// secureElementIdentity has a wrapper type so you can specify "this identity", "no identity", or nil (no opinion)
- (void)updateWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                  forceRefetch:(BOOL)forceRefetch
                    deviceName:(nullable NSString *)deviceName
                  serialNumber:(nullable NSString *)serialNumber
                     osVersion:(nullable NSString *)osVersion
                 policyVersion:(nullable NSNumber *)policyVersion
                 policySecrets:(nullable NSDictionary<NSString*,NSData*> *)policySecrets
     syncUserControllableViews:(nullable NSNumber *)syncUserControllableViews
         secureElementIdentity:(nullable TrustedPeersHelperIntendedTPPBSecureElementIdentity*)secureElementIdentity
                 walrusSetting:(nullable TPPBPeerStableInfoSetting*)walrusSetting
                     webAccess:(nullable TPPBPeerStableInfoSetting*)webAccess
                         reply:(void (^)(TrustedPeersHelperPeerState* _Nullable peerState,
                                         TPSyncingPolicy* _Nullable syncingPolicy,
                                         NSError * _Nullable error))reply;

- (void)setPreapprovedKeysWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                           preapprovedKeys:(NSArray<NSData*> *)preapprovedKeys
                                     reply:(void (^)(TrustedPeersHelperPeerState* _Nullable peerState, NSError * _Nullable error))reply;

/* Rather thin pass-through for uploading new TLKs (for zones which may have disappeared) */
- (void)updateTLKsWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                          ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)ckksKeys
                         tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                             reply:(void (^)(NSArray<CKRecord*>* _Nullable keyHierarchyRecords, NSError * _Nullable error))reply;

- (void)fetchViableBottlesWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                    source:(OTEscrowRecordFetchSource)source
                                    flowID:(NSString * _Nullable)flowID
                           deviceSessionID:(NSString * _Nullable)deviceSessionID
                                     reply:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs, NSArray<NSString*>* _Nullable sortedPartialBottleIDs, NSError* _Nullable error))reply;

- (void)fetchViableEscrowRecordsWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                          source:(OTEscrowRecordFetchSource)source
                                           reply:(void (^)(NSArray<NSData*>* _Nullable records, NSError* _Nullable error))reply;

- (void)fetchEscrowContentsWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                      reply:(void (^)(NSData* _Nullable entropy,
                                                      NSString* _Nullable bottleID,
                                                      NSData* _Nullable signingPublicKey,
                                                      NSError* _Nullable error))reply;

- (void)fetchPolicyDocumentsWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                    versions:(NSSet<TPPolicyVersion*>*)versions
                                       reply:(void (^)(NSDictionary<TPPolicyVersion*, NSData*>* _Nullable entries,
                                                       NSError * _Nullable error))reply;

- (void)fetchRecoverableTLKSharesWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                           peerID:(NSString* _Nullable)peerID
                                            reply:(void (^)(NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                            NSError * _Nullable error))reply;

// Fetch the policy and view list for current peer.
// Note: userControllableViewStatusOfPeers is not our current peer's view of the world, but rather what
// our peers believe.
// If there is no prepared ego peer, the returned policy will be for a device with modelIDOverride
- (void)fetchCurrentPolicyWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                           modelIDOverride:(NSString* _Nullable)modelID
                        isInheritedAccount:(BOOL)isInheritedAccount
                                     reply:(void (^)(TPSyncingPolicy* _Nullable syncingPolicy,
                                                     TPPBPeerStableInfoUserControllableViewStatus userControllableViewStatusOfPeers,
                                                     NSError * _Nullable error))reply;

// TODO: merge this and trustStatusWithSpecificUser
- (void)fetchTrustStateWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                  reply:(void (^)(TrustedPeersHelperPeerState* _Nullable selfPeerState,
                                                  NSArray<TrustedPeersHelperPeer*>* _Nullable trustedPeers,
                                                  NSError* _Nullable error))reply;

- (void)setRecoveryKeyWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                           recoveryKey:(NSString *)recoveryKey
                                  salt:(NSString *)salt
                              ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)ckksKeys
                                 reply:(void (^)(NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                 NSError* _Nullable error))reply;

- (void)createCustodianRecoveryKeyWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                       recoveryKey:(NSString *)recoveryString
                                              salt:(NSString *)salt
                                          ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)ckksKeys
                                              uuid:(NSUUID *)uuid
                                              kind:(TPPBCustodianRecoveryKey_Kind)kind
                                             reply:(void (^)(NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                             TrustedPeersHelperCustodianRecoveryKey *_Nullable crk,
                                                             NSError* _Nullable error))reply;

- (void)removeCustodianRecoveryKeyWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                              uuid:(NSUUID *)uuid
                                             reply:(void (^)(NSError* _Nullable error))reply;

- (void)findCustodianRecoveryKeyWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                            uuid:(NSUUID *)uuid
                                           reply:(void (^)(TrustedPeersHelperCustodianRecoveryKey* _Nullable crk, NSError* _Nullable error))reply;

- (void)requestHealthCheckWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                       requiresEscrowCheck:(BOOL)requiresEscrowCheck
                                    repair:(BOOL)repair
                          knownFederations:(NSArray<NSString *> *)knownFederations
                                    flowID:(NSString* _Nullable )flowID
                           deviceSessionID:(NSString* _Nullable )deviceSessionID
                                     reply:(void (^)(TrustedPeersHelperHealthCheckResult* _Nullable result, NSError* _Nullable error))reply;

- (void)getSupportAppInfoWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                    reply:(void (^)(NSData * _Nullable, NSError * _Nullable))reply;

- (void)resetAccountCDPContentsWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                              idmsTargetContext:(NSString*_Nullable)idmsTargetContext
                         idmsCuttlefishPassword:(NSString*_Nullable)idmsCuttlefishPassword
                                     notifyIdMS:(bool)notifyIdMS
                                internalAccount:(bool)internalAccount
                                    demoAccount:(bool)demoAccount
                                          reply:(void (^)(NSError * _Nullable))reply;

- (void)removeEscrowCacheWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                    reply:(void (^)(NSError * _Nullable))reply;


- (void)fetchAccountSettingsWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                  forceFetch:(bool)forceFetch
                                       reply:(void (^)(NSDictionary<NSString*, TPPBPeerStableInfoSetting *> * _Nullable setting,
                                                       NSError* _Nullable error))reply;

- (void)isRecoveryKeySet:(TPSpecificUser* _Nullable)specificUser
                   reply:(void (^)(BOOL isSet, NSError* _Nullable error))reply;

- (void)removeRecoveryKey:(TPSpecificUser* _Nullable)specificUser
                    reply:(void (^)(BOOL result, NSError* _Nullable error))reply;

- (void)performATOPRVActionsWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                       reply:(void (^)(NSError* _Nullable error))reply;

- (void)testSemaphoreWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                  arg:(NSString*)arg
                                reply:(void (^)(NSError* _Nullable error))reply;

- (void)preflightRecoverOctagonUsingRecoveryKey:(TPSpecificUser* _Nullable)specificUser
                                    recoveryKey:(NSString*)recoveryKey
                                           salt:(NSString*)salt
                                          reply:(void (^)(BOOL correct, NSError * _Nullable error))reply;

- (void)fetchTrustedPeerCountWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                        reply:(void (^)(NSNumber* _Nullable count,
                                                        NSError* _Nullable error))reply;

- (void)octagonContainsDistrustedRecoveryKeysWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                                        reply:(void (^)(BOOL containsDistrusted, NSError* _Nullable error))reply;

- (void)fetchCurrentItemWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                    items:(NSArray<CuttlefishCurrentItemSpecifier*> *)items
                                    reply:(void (^)(NSArray<CuttlefishCurrentItem*>* _Nullable items, NSArray<CKRecord*>* _Nullable syncKeyRecords, NSError* _Nullable error))reply;


- (void)fetchPCSIdentityByPublicKeyWithSpecificUser:(TPSpecificUser* _Nullable)specificUser
                                        pcsservices:(NSArray<CuttlefishPCSServiceIdentifier*> *)pcsservices
                                              reply:(void (^)(NSArray<CuttlefishPCSIdentity*>* _Nullable pcsIdentities, NSArray<CKRecord*>* _Nullable syncKeyRecords, NSError* _Nullable error))reply;

@end

/*
 To use the service from an application or other process, use NSXPCConnection to establish a connection to the service by doing something like this:

     _connectionToService = [[NSXPCConnection alloc] initWithServiceName:@"com.apple.TrustedPeersHelper"];
     _connectionToService.remoteObjectInterface = TrustedPeersHelperSetupProtocol([NSXPCInterface interfaceWithProtocol:@protocol(TrustedPeersHelperProtocol)]);
     [_connectionToService resume];

Once you have a connection to the service, you can use it like this:

     [[_connectionToService remoteObjectProxy] upperCaseString:@"hello" withReply:^(NSString *aString) {
         // We have received a response. Update our text field, but do it on the main thread.
         NSLog(@"Result string was: %@", aString);
     }];

 And, when you are finished with the service, clean up the connection like this:

     [_connectionToService invalidate];
*/


// Use this at protocol creation time to tell NSXPC to do its job
NSXPCInterface* TrustedPeersHelperSetupProtocol(NSXPCInterface* interface);

NS_ASSUME_NONNULL_END
