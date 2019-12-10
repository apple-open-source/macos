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

#import "keychain/ckks/CKKSKeychainBackedKey.h"
#import "keychain/ckks/CKKSTLKShare.h"

#import "keychain/ot/OTConstants.h"

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

- (instancetype)initWithPeerID:(NSString* _Nullable)peerID
                 isPreapproved:(BOOL)isPreapproved
                        status:(TPPeerStatus)peerStatus
                 memberChanges:(BOOL)memberChanges
             unknownMachineIDs:(BOOL)unknownMachineIDs
                     osVersion:(NSString * _Nullable)osVersion;
@end

@interface TrustedPeersHelperPeer : NSObject <NSSecureCoding>
@property (nullable) NSString* peerID;
@property (nullable) NSData* signingSPKI;
@property (nullable) NSData* encryptionSPKI;
@property (nullable) NSSet<NSString*>* viewList;

- (instancetype)initWithPeerID:(NSString*)peerID
                   signingSPKI:(NSData*)signingSPKI
                encryptionSPKI:(NSData*)encryptionSPKI
                      viewList:(NSSet<NSString*>*)viewList;
@end

@interface TrustedPeersHelperEgoPeerStatus : NSObject <NSSecureCoding>
@property TPPeerStatus egoStatus;
@property NSString* _Nullable egoPeerID;
@property (assign) uint64_t numberOfPeersInOctagon;

// Note: this field does not include untrusted peers
@property NSDictionary<NSString*, NSNumber*>* viablePeerCountsByModelID;

// Note: this field does include untrusted peers
@property NSDictionary<NSString*, NSNumber*>* peerCountsByMachineID;

@property BOOL isExcluded;
@property BOOL isLocked;

- (instancetype)initWithEgoPeerID:(NSString* _Nullable)egoPeerID
                           status:(TPPeerStatus)egoStatus
        viablePeerCountsByModelID:(NSDictionary<NSString*, NSNumber*>*)viablePeerCountsByModelID
            peerCountsByMachineID:(NSDictionary<NSString*, NSNumber*>*)peerCountsByMachineID
                       isExcluded:(BOOL)isExcluded
                         isLocked:(BOOL)isLocked;

@end

// This protocol describes the interface of the TrustedPeersHelper XPC service.
@protocol TrustedPeersHelperProtocol

// This is used by a unit test which exercises the XPC-service plumbing.
- (void)pingWithReply:(void (^)(void))reply;

- (void)dumpWithContainer:(NSString *)container
                  context:(NSString *)context
                    reply:(void (^)(NSDictionary * _Nullable, NSError * _Nullable))reply;

- (void)departByDistrustingSelfWithContainer:(NSString *)container
                                     context:(NSString *)context
                                       reply:(void (^)(NSError * _Nullable))reply;

- (void)distrustPeerIDsWithContainer:(NSString *)container
                             context:(NSString *)context
                             peerIDs:(NSSet<NSString*>*)peerIDs
                               reply:(void (^)(NSError * _Nullable))reply;

- (void)trustStatusWithContainer:(NSString *)container
                         context:(NSString *)context
                           reply:(void (^)(TrustedPeersHelperEgoPeerStatus *status,
                                           NSError* _Nullable error))reply;

- (void)resetWithContainer:(NSString *)container
                   context:(NSString *)context
                    resetReason:(CuttlefishResetReason)reason
                     reply:(void (^)(NSError * _Nullable error))reply;

- (void)localResetWithContainer:(NSString *)container
                        context:(NSString *)context
                          reply:(void (^)(NSError * _Nullable error))reply;

// The following three machine ID list manipulation functions do not attempt to apply the results to the model
// If you'd like that to occur, please call update()

// TODO: how should we communicate TLK rolling when the update() call will remove a peer?
// <rdar://problem/46633449> Octagon: must be able to roll TLKs when a peer departs due to machine ID list

// listDifferences: False if the allowedMachineIDs list passed in exactly matches the previous state,
//                  True if there were any differences
- (void)setAllowedMachineIDsWithContainer:(NSString *)container
                                  context:(NSString *)context
                        allowedMachineIDs:(NSSet<NSString*> *)allowedMachineIDs
                                    reply:(void (^)(BOOL listDifferences, NSError * _Nullable error))reply;

- (void)addAllowedMachineIDsWithContainer:(NSString *)container
                                  context:(NSString *)context
                               machineIDs:(NSArray<NSString*> *)machineIDs
                                    reply:(void (^)(NSError * _Nullable error))reply;

- (void)removeAllowedMachineIDsWithContainer:(NSString *)container
                                     context:(NSString *)context
                                  machineIDs:(NSArray<NSString*> *)machineIDs
                                       reply:(void (^)(NSError * _Nullable error))reply;

- (void)fetchAllowedMachineIDsWithContainer:(NSString *)container
                                    context:(NSString *)context
                                      reply:(void (^)(NSSet<NSString*>* _Nullable machineIDs, NSError* _Nullable error))reply;

- (void)fetchEgoEpochWithContainer:(NSString *)container
                           context:(NSString *)context
                             reply:(void (^)(unsigned long long epoch,
                                             NSError * _Nullable error))reply;

- (void)prepareWithContainer:(NSString *)container
                     context:(NSString *)context
                       epoch:(unsigned long long)epoch
                   machineID:(NSString *)machineID
                  bottleSalt:(NSString *)bottleSalt
                    bottleID:(NSString *)bottleID
                     modelID:(NSString *)modelID
                  deviceName:(nullable NSString*)deviceName
                serialNumber:(NSString *)serialNumber
                   osVersion:(NSString *)osVersion
               policyVersion:(nullable NSNumber *)policyVersion
               policySecrets:(nullable NSDictionary<NSString*,NSData*> *)policySecrets
 signingPrivKeyPersistentRef:(nullable NSData *)spkPr
     encPrivKeyPersistentRef:(nullable NSData*)epkPr
                       reply:(void (^)(NSString * _Nullable peerID,
                                       NSData * _Nullable permanentInfo,
                                       NSData * _Nullable permanentInfoSig,
                                       NSData * _Nullable stableInfo,
                                       NSData * _Nullable stableInfoSig,
                                       NSError * _Nullable error))reply;

// If there already are existing CKKSViews, please pass in their key sets anyway.
// This function will create a self TLK Share for those TLKs.
- (void)establishWithContainer:(NSString *)container
                       context:(NSString *)context
                      ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)viewKeySets
                     tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
               preapprovedKeys:(nullable NSArray<NSData*> *)preapprovedKeys
                         reply:(void (^)(NSString * _Nullable peerID,
                                         NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                         NSError * _Nullable error))reply;

// Returns a voucher for the given peer ID using our own identity
// If TLK CKKSViewKeys are given, TLKShares will be created and uploaded for this new peer before this call returns.
- (void)vouchWithContainer:(NSString *)container
                   context:(NSString *)context
                    peerID:(NSString *)peerID
             permanentInfo:(NSData *)permanentInfo
          permanentInfoSig:(NSData *)permanentInfoSig
                stableInfo:(NSData *)stableInfo
             stableInfoSig:(NSData *)stableInfoSig
                  ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)viewKeySets
                     reply:(void (^)(NSData * _Nullable voucher,
                                     NSData * _Nullable voucherSig,
                                     NSError * _Nullable error))reply;

// Preflighting a vouch will return the peer ID associated with the bottle you will be recovering.
// You can then use that peer ID to filter the tlkshares provided to vouchWithBottle.
- (void)preflightVouchWithBottleWithContainer:(NSString *)container
                                      context:(NSString *)context
                                     bottleID:(NSString*)bottleID
                                        reply:(void (^)(NSString* _Nullable peerID,
                                                        NSError * _Nullable error))reply;

// Returns a voucher for our own identity, created by the identity inside this bottle
- (void)vouchWithBottleWithContainer:(NSString *)container
                             context:(NSString *)context
                            bottleID:(NSString*)bottleID
                             entropy:(NSData*)entropy
                          bottleSalt:(NSString*)bottleSalt
                           tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                               reply:(void (^)(NSData * _Nullable voucher,
                                               NSData * _Nullable voucherSig,
                                               NSError * _Nullable error))reply;

// Returns a voucher for our own identity, using recovery key
- (void)vouchWithRecoveryKeyWithContainer:(NSString *)container
                                  context:(NSString *)context
                              recoveryKey:(NSString*)recoveryKey
                                     salt:(NSString*)salt
                                tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                                    reply:(void (^)(NSData * _Nullable voucher,
                                                    NSData * _Nullable voucherSig,
                                                    NSError * _Nullable error))reply;

// As of right now, join and attemptPreapprovedJoin will upload TLKShares for any TLKs that this peer already has.
// Note that in The Future, a device might decide to join an existing Octagon set while introducing a new view.
// These interfaces will have to change...
- (void)joinWithContainer:(NSString *)container
                  context:(NSString *)context
              voucherData:(NSData *)voucherData
               voucherSig:(NSData *)voucherSig
                 ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)viewKeySets
                tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
          preapprovedKeys:(NSArray<NSData*> *)preapprovedKeys
                    reply:(void (^)(NSString * _Nullable peerID,
                                    NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                    NSError * _Nullable error))reply;

// Preflighting a preapproved join suggests whether or not you expect to succeed in an immediate preapprovedJoin() call
// This only inspects the Octagon model, and ignores the trusted device list, so that you can preflight the preapprovedJoin()
// before fetching that list.
// This will return YES if there are no existing peers, or if the existing peers preapprove your prepared identity.
// This will return NO otherwise.
- (void)preflightPreapprovedJoinWithContainer:(NSString *)container
                                      context:(NSString *)context
                                        reply:(void (^)(BOOL launchOkay,
                                                        NSError * _Nullable error))reply;

// A preapproved join might do a join, but it also might do an establish.
// Therefore, it needs all the TLKs and TLKShares as establish does
- (void)attemptPreapprovedJoinWithContainer:(NSString *)container
                                    context:(NSString *)context
                                   ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)ckksKeys
                                  tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                            preapprovedKeys:(NSArray<NSData*> *)preapprovedKeys
                                      reply:(void (^)(NSString * _Nullable peerID,
                                                      NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                      NSError * _Nullable error))reply;

// TODO: if the new policy causes someone to lose access to a view, how should this API work?
- (void)updateWithContainer:(NSString *)container
                    context:(NSString *)context
                 deviceName:(nullable NSString *)deviceName
               serialNumber:(nullable NSString *)serialNumber
                  osVersion:(nullable NSString *)osVersion
              policyVersion:(nullable NSNumber *)policyVersion
              policySecrets:(nullable NSDictionary<NSString*,NSData*> *)policySecrets
                      reply:(void (^)(TrustedPeersHelperPeerState* _Nullable peerState, NSError * _Nullable error))reply;

- (void)setPreapprovedKeysWithContainer:(NSString *)container
                                context:(NSString *)context
                        preapprovedKeys:(NSArray<NSData*> *)preapprovedKeys
                                  reply:(void (^)(NSError * _Nullable error))reply;

/* Rather thin pass-through for uploading new TLKs (for zones which may have disappeared) */
- (void)updateTLKsWithContainer:(NSString *)container
                        context:(NSString *)context
                       ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)ckksKeys
                      tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                          reply:(void (^)(NSArray<CKRecord*>* _Nullable keyHierarchyRecords, NSError * _Nullable error))reply;

- (void)fetchViableBottlesWithContainer:(NSString *)container
                                context:(NSString *)context
                                  reply:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs, NSArray<NSString*>* _Nullable sortedPartialBottleIDs, NSError* _Nullable error))reply;

- (void)fetchEscrowContentsWithContainer:(NSString *)container
                                 context:(NSString *)context
                                   reply:(void (^)(NSData* _Nullable entropy,
                                                   NSString* _Nullable bottleID,
                                                   NSData* _Nullable signingPublicKey,
                                                   NSError* _Nullable error))reply;

// The argument contains N [version:hash] keys,
// the reply block contains 0<=N [version:[hash, data]] entries.
- (void)fetchPolicyDocumentsWithContainer:(NSString*)container
                                  context:(NSString*)context
                                     keys:(NSDictionary<NSNumber*,NSString*>*)keys
                                    reply:(void (^)(NSDictionary<NSNumber*,NSArray<NSString*>*>* _Nullable entries,
                                                    NSError * _Nullable error))reply;

// Fetch the policy for current peer.
- (void)fetchPolicyWithContainer:(NSString*)container
                         context:(NSString*)context
                         reply:(void (^)(TPPolicy * _Nullable policy,
                                         NSError * _Nullable error))reply;

- (void)validatePeersWithContainer:(NSString *)container
                           context:(NSString *)context
                             reply:(void (^)(NSDictionary * _Nullable, NSError * _Nullable))reply;


// TODO: merge this and trustStatusWithContainer
- (void)fetchTrustStateWithContainer:(NSString *)container
                             context:(NSString *)context
                               reply:(void (^)(TrustedPeersHelperPeerState* _Nullable selfPeerState,
                                               NSArray<TrustedPeersHelperPeer*>* _Nullable trustedPeers,
                                               NSError* _Nullable error))reply;

- (void)setRecoveryKeyWithContainer:(NSString *)container
                            context:(NSString *)context
                        recoveryKey:(NSString *)recoveryKey
                               salt:(NSString *)salt
                           ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)ckksKeys
                              reply:(void (^)(NSError* _Nullable error))reply;

- (void)reportHealthWithContainer:(NSString *)container
                          context:(NSString *)context
                stateMachineState:(NSString *)state
                       trustState:(NSString *)trustState
                            reply:(void (^)(NSError* _Nullable error))reply;

- (void)pushHealthInquiryWithContainer:(NSString *)container
                               context:(NSString *)context
                                 reply:(void (^)(NSError* _Nullable error))reply;

- (void)getViewsWithContainer:(NSString *)container
                      context:(NSString *)context
		      inViews:(NSArray<NSString*>*)inViews
		      reply:(void (^)(NSArray<NSString*>* _Nullable, NSError* _Nullable))reply;

- (void)requestHealthCheckWithContainer:(NSString *)container
                                context:(NSString *)context
                    requiresEscrowCheck:(BOOL)requiresEscrowCheck
                                  reply:(void (^)(BOOL postRepairCFU, BOOL postEscrowCFU, BOOL resetOctagon, NSError* _Nullable))reply;

- (void)getSupportAppInfoWithContainer:(NSString *)container
                               context:(NSString *)context
                                 reply:(void (^)(NSData * _Nullable, NSError * _Nullable))reply;

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
