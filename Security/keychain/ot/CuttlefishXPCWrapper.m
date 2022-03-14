/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/CuttlefishXPCWrapper.h"
#import <AppleFeatures/AppleFeatures.h>

@implementation CuttlefishXPCWrapper
- (instancetype) initWithCuttlefishXPCConnection: (id<NSXPCProxyCreating>)cuttlefishXPCConnection
{
    if ((self = [super init])) {
        _cuttlefishXPCConnection = cuttlefishXPCConnection;
    }
    return self;
}

+ (bool)retryable:(NSError *_Nonnull)error
{
    return error.domain == NSCocoaErrorDomain && error.code == NSXPCConnectionInterrupted;
}

enum {NUM_RETRIES = 5};

- (void)pingWithReply:(void (^)(void))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                    }
                    ++i;
                }] pingWithReply:reply];
    } while (retry);
}

- (void)dumpWithContainer:(NSString *)container
                  context:(NSString *)context
                    reply:(void (^)(NSDictionary * _Nullable, NSError * _Nullable))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, error);
                    }
                    ++i;
                }] dumpWithContainer:container context:context reply:reply];
    } while (retry);
}

- (void)departByDistrustingSelfWithContainer:(NSString *)container
                                     context:(NSString *)context
                                       reply:(void (^)(NSError * _Nullable))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(error);
                    }
                    ++i;
                }] departByDistrustingSelfWithContainer:container context:context reply:reply];
    } while (retry);
}

- (void)distrustPeerIDsWithContainer:(NSString *)container
                             context:(NSString *)context
                             peerIDs:(NSSet<NSString*>*)peerIDs
                               reply:(void (^)(NSError * _Nullable))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(error);
                    }
                    ++i;
                }] distrustPeerIDsWithContainer:container context:context peerIDs:peerIDs reply:reply];
    } while (retry);
}

- (void)trustStatusWithContainer:(NSString *)container
                         context:(NSString *)context
                           reply:(void (^)(TrustedPeersHelperEgoPeerStatus *status,
                                           NSError* _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, error);
                    }
                    ++i;
                }] trustStatusWithContainer:container context:context reply:reply];
    } while (retry);
}

- (void)resetWithContainer:(NSString *)container
                   context:(NSString *)context
               resetReason:(CuttlefishResetReason)reason
                     reply:(void (^)(NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(error);
                    }
                    ++i;
                }] resetWithContainer:container context:context resetReason:reason reply:reply];
    } while (retry);
}

- (void)localResetWithContainer:(NSString *)container
                        context:(NSString *)context
                          reply:(void (^)(NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(error);
                    }
                    ++i;
                }] localResetWithContainer:container context:context reply:reply];
    } while (retry);
}

- (void)setAllowedMachineIDsWithContainer:(NSString *)container
                                  context:(NSString *)context
                        allowedMachineIDs:(NSSet<NSString*> *)allowedMachineIDs
                            honorIDMSListChanges:(BOOL)accountIsDemo
                                    reply:(void (^)(BOOL listDifferences, NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(NO, error);
                    }
                    ++i;
        }] setAllowedMachineIDsWithContainer:container context:context allowedMachineIDs:allowedMachineIDs honorIDMSListChanges:accountIsDemo reply:reply];
    } while (retry);
}

- (void)addAllowedMachineIDsWithContainer:(NSString *)container
                                  context:(NSString *)context
                               machineIDs:(NSArray<NSString*> *)machineIDs
                                    reply:(void (^)(NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(error);
                    }
                    ++i;
                }] addAllowedMachineIDsWithContainer:container
                                             context:context
                                          machineIDs:machineIDs
                                               reply:reply];
    } while (retry);
}

- (void)removeAllowedMachineIDsWithContainer:(NSString *)container
                                     context:(NSString *)context
                                  machineIDs:(NSArray<NSString*> *)machineIDs
                                       reply:(void (^)(NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(error);
                    }
                    ++i;
                }] removeAllowedMachineIDsWithContainer:container context:context machineIDs:machineIDs reply:reply];
    } while (retry);
}


- (void)fetchAllowedMachineIDsWithContainer:(nonnull NSString *)container
                                    context:(nonnull NSString *)context
                                      reply:(nonnull void (^)(NSSet<NSString *> * _Nullable, NSError * _Nullable))reply {
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, error);
                    }
                    ++i;
                }] fetchAllowedMachineIDsWithContainer:container context:context reply:reply];
    } while (retry);
}


- (void)fetchEgoEpochWithContainer:(NSString *)container
                           context:(NSString *)context
                             reply:(void (^)(unsigned long long epoch,
                                             NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(0, error);
                    }
                    ++i;
                }] fetchEgoEpochWithContainer:container context:context reply:reply];
    } while (retry);
}

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
               policyVersion:(nullable TPPolicyVersion *)policyVersion
               policySecrets:(nullable NSDictionary<NSString*,NSData*> *)policySecrets
   syncUserControllableViews:(TPPBPeerStableInfoUserControllableViewStatus)syncUserControllableViews
       secureElementIdentity:(nullable TPPBSecureElementIdentity*)secureElementIdentity
 signingPrivKeyPersistentRef:(nullable NSData *)spkPr
     encPrivKeyPersistentRef:(nullable NSData*)epkPr
                       reply:(void (^)(NSString * _Nullable peerID,
                                       NSData * _Nullable permanentInfo,
                                       NSData * _Nullable permanentInfoSig,
                                       NSData * _Nullable stableInfo,
                                       NSData * _Nullable stableInfoSig,
                                       TPSyncingPolicy* _Nullable syncingPolicy,
                                       NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, nil, nil, nil, nil, error);
                    }
                    ++i;
                }] prepareWithContainer:container
         context:context
         epoch:epoch
         machineID:machineID
         bottleSalt:bottleSalt
         bottleID:bottleID
         modelID:modelID
         deviceName:deviceName
         serialNumber:serialNumber
         osVersion:osVersion
         policyVersion:policyVersion
         policySecrets:policySecrets
         syncUserControllableViews:syncUserControllableViews
         secureElementIdentity:secureElementIdentity
         setting:setting
         signingPrivKeyPersistentRef:spkPr
         encPrivKeyPersistentRef:epkPr
         reply:reply];
    } while (retry);
}

- (void)establishWithContainer:(NSString *)container
                       context:(NSString *)context
                      ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)viewKeySets
                     tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
               preapprovedKeys:(nullable NSArray<NSData*> *)preapprovedKeys
                         reply:(void (^)(NSString * _Nullable peerID,
                                         NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                         TPSyncingPolicy* _Nullable syncingPolicy,
                                         NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, nil, error);
                    }
                    ++i;
                }] establishWithContainer:container context:context ckksKeys:viewKeySets tlkShares:tlkShares preapprovedKeys:preapprovedKeys reply:reply];
    } while (retry);
}

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
                                     NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, error);
                    }
                    ++i;
                }] vouchWithContainer:container context:context peerID:peerID permanentInfo:permanentInfo permanentInfoSig:permanentInfoSig stableInfo:stableInfo stableInfoSig:stableInfoSig ckksKeys:viewKeySets reply:reply];
    } while (retry);
}


- (void)preflightVouchWithBottleWithContainer:(nonnull NSString *)container
                                      context:(nonnull NSString *)context
                                     bottleID:(nonnull NSString *)bottleID
                                        reply:(nonnull void (^)(NSString * _Nullable,
                                                                TPSyncingPolicy* _Nullable peerSyncingPolicy,
                                                                BOOL refetchWasNeeded,
                                                                NSError * _Nullable))reply {
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, false, error);
                    }
                    ++i;
                }] preflightVouchWithBottleWithContainer:container
                                                 context:context
                                                bottleID:bottleID
                                                   reply:reply];
    } while (retry);
}

- (void)vouchWithBottleWithContainer:(NSString *)container
                             context:(NSString *)context
                            bottleID:(NSString*)bottleID
                             entropy:(NSData*)entropy
                          bottleSalt:(NSString*)bottleSalt
                           tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                               reply:(void (^)(NSData * _Nullable voucher,
                                               NSData * _Nullable voucherSig,
                                               NSArray<CKKSTLKShare*>* _Nullable newTLKShares,
                                               TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                               NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, nil, nil, error);
                    }
                    ++i;
                }] vouchWithBottleWithContainer:container context:context bottleID:bottleID entropy:entropy bottleSalt:bottleSalt tlkShares:tlkShares reply:reply];
    } while (retry);
}

- (void)preflightVouchWithRecoveryKeyWithContainer:(nonnull NSString *)container
                                           context:(nonnull NSString *)context
                                       recoveryKey:(NSString*)recoveryKey
                                              salt:(NSString*)salt
                                             reply:(nonnull void (^)(NSString * _Nullable,
                                                                     TPSyncingPolicy* _Nullable peerSyncingPolicy,
                                                                     NSError * _Nullable))reply {
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, error);
                    }
                    ++i;
                }] preflightVouchWithRecoveryKeyWithContainer:container
                                                 context:context
                                             recoveryKey:recoveryKey
                                                    salt:salt
                                                   reply:reply];
    } while (retry);
}

- (void)preflightVouchWithCustodianRecoveryKeyWithContainer:(NSString*)container
                                                    context:(NSString*)context
                                                        crk:(TrustedPeersHelperCustodianRecoveryKey*)crk
                                                      reply:(void (^)(NSString* _Nullable recoveryKeyID,
                                                                      TPSyncingPolicy* _Nullable syncingPolicy,
                                                                      NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, error);
                    }
                    ++i;
                }] preflightVouchWithCustodianRecoveryKeyWithContainer:container
                                                               context:context
                                                                   crk:crk
                                                                 reply:reply];
    } while (retry);
}

- (void)vouchWithRecoveryKeyWithContainer:(NSString *)container
                                  context:(NSString *)context
                              recoveryKey:(NSString*)recoveryKey
                                     salt:(NSString*)salt
                                tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                                    reply:(void (^)(NSData * _Nullable voucher,
                                                    NSData * _Nullable voucherSig,
                                                    NSArray<CKKSTLKShare*>* _Nullable newTLKShares,
                                                    TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                    NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, nil, nil, error);
                    }
                    ++i;
                }] vouchWithRecoveryKeyWithContainer:container context:context recoveryKey:recoveryKey salt:salt tlkShares:tlkShares reply:reply];
    } while (retry);
}

- (void)vouchWithCustodianRecoveryKeyWithContainer:(NSString *)container
                                           context:(NSString *)context
                                               crk:(TrustedPeersHelperCustodianRecoveryKey*)crk
                                         tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                                             reply:(void (^)(NSData * _Nullable voucher,
                                                             NSData * _Nullable voucherSig,
                                                             NSArray<CKKSTLKShare*>* _Nullable newTLKShares,
                                                             TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                             NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, nil, nil, error);
                    }
                    ++i;
                }] vouchWithCustodianRecoveryKeyWithContainer:container context:context crk:crk tlkShares:tlkShares reply:reply];
    } while (retry);
}

- (void)joinWithContainer:(NSString *)container
                  context:(NSString *)context
              voucherData:(NSData *)voucherData
               voucherSig:(NSData *)voucherSig
                 ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)viewKeySets
                tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
          preapprovedKeys:(nullable NSArray<NSData*> *)preapprovedKeys
                    reply:(void (^)(NSString * _Nullable peerID,
                                    NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                    TPSyncingPolicy* _Nullable syncingPolicy,
                                    NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, nil, error);
                    }
                    ++i;
                }] joinWithContainer:container context:context voucherData:voucherData voucherSig:voucherSig ckksKeys:viewKeySets tlkShares:tlkShares preapprovedKeys:preapprovedKeys reply:reply];
    } while (retry);
}

- (void)preflightPreapprovedJoinWithContainer:(NSString *)container
                                      context:(NSString *)context
                              preapprovedKeys:(nullable NSArray<NSData*> *)preapprovedKeys
                                        reply:(void (^)(BOOL launchOkay,
                                                        NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(NO, error);
                    }
                    ++i;
        }] preflightPreapprovedJoinWithContainer:container context:context preapprovedKeys:preapprovedKeys reply:reply];
    } while (retry);
}

- (void)attemptPreapprovedJoinWithContainer:(NSString *)container
                                    context:(NSString *)context
                                   ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)ckksKeys
                                  tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                            preapprovedKeys:(nullable NSArray<NSData*> *)preapprovedKeys
                                      reply:(void (^)(NSString * _Nullable peerID,
                                                      NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                      TPSyncingPolicy* _Nullable syncingPolicy,
                                                      NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, nil, error);
                    }
                    ++i;
        }] attemptPreapprovedJoinWithContainer:container
                                       context:context
                                      ckksKeys:ckksKeys
                                     tlkShares:tlkShares
                               preapprovedKeys:preapprovedKeys
                                         reply:reply];
    } while (retry);
}

- (void)updateWithContainer:(NSString *)container
                    context:(NSString *)context
               forceRefetch:(BOOL)forceRefetch
                 deviceName:(nullable NSString *)deviceName
               serialNumber:(nullable NSString *)serialNumber
                  osVersion:(nullable NSString *)osVersion
              policyVersion:(nullable NSNumber *)policyVersion
              policySecrets:(nullable NSDictionary<NSString*,NSData*> *)policySecrets
  syncUserControllableViews:(nullable NSNumber *)syncUserControllableViews
      secureElementIdentity:(nullable TrustedPeersHelperIntendedTPPBSecureElementIdentity*)secureElementIdentity
                      reply:(void (^)(TrustedPeersHelperPeerState* _Nullable peerState, TPSyncingPolicy* _Nullable policy, NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, error);
                    }
                    ++i;
                }] updateWithContainer:container
                               context:context
                          forceRefetch:forceRefetch
                            deviceName:deviceName
                          serialNumber:serialNumber
                             osVersion:osVersion
                         policyVersion:policyVersion
                         policySecrets:policySecrets
             syncUserControllableViews:syncUserControllableViews
                 secureElementIdentity:secureElementIdentity
                                 reply:reply];
    } while (retry);
}

- (void)setPreapprovedKeysWithContainer:(NSString *)container
                                context:(NSString *)context
                        preapprovedKeys:(NSArray<NSData*> *)preapprovedKeys
                                  reply:(void (^)(TrustedPeersHelperPeerState* _Nullable peerState, NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, error);
                    }
                    ++i;
                }] setPreapprovedKeysWithContainer:container context:context preapprovedKeys:preapprovedKeys reply:reply];
    } while (retry);
}

- (void)updateTLKsWithContainer:(NSString *)container
                        context:(NSString *)context
                       ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)ckksKeys
                      tlkShares:(NSArray<CKKSTLKShare*> *)tlkShares
                          reply:(void (^)(NSArray<CKRecord*>* _Nullable keyHierarchyRecords, NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, error);
                    }
                    ++i;
                }] updateTLKsWithContainer:container context:context ckksKeys:ckksKeys tlkShares:tlkShares reply:reply];
    } while (retry);
}
    
- (void)fetchViableBottlesWithContainer:(NSString *)container
                                context:(NSString *)context
                                  reply:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs, NSArray<NSString*>* _Nullable sortedPartialBottleIDs, NSError* _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, error);
                    }
                    ++i;
                }] fetchViableBottlesWithContainer:container context:context reply:reply];
    } while (retry);
}
   
- (void)fetchEscrowContentsWithContainer:(NSString *)container
                                 context:(NSString *)context
                                   reply:(void (^)(NSData* _Nullable entropy,
                                                   NSString* _Nullable bottleID,
                                                   NSData* _Nullable signingPublicKey,
                                                   NSError* _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, nil, error);
                    }
                    ++i;
                }] fetchEscrowContentsWithContainer:container context:context reply:reply];
    } while (retry);
}

- (void)fetchPolicyDocumentsWithContainer:(NSString*)container
                                  context:(NSString*)context
                                 versions:(NSSet<TPPolicyVersion*>*)versions
                                    reply:(void (^)(NSDictionary<TPPolicyVersion*, NSData*>* _Nullable entries,
                                                    NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, error);
                    }
                    ++i;
                }] fetchPolicyDocumentsWithContainer:container context:context versions:versions reply:reply];
    } while (retry);
}

- (void)fetchCurrentPolicyWithContainer:(NSString*)container
                                context:(NSString*)context
                        modelIDOverride:(NSString* _Nullable)modelIDOverride
                     isInheritedAccount:(BOOL)isInheritedAccount
                                  reply:(void (^)(TPSyncingPolicy* _Nullable syncingPolicy,
                                                  TPPBPeerStableInfoUserControllableViewStatus userControllableViewStatusOfPeers,
                                                  NSError * _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil,
                              TPPBPeerStableInfoUserControllableViewStatus_UNKNOWN,
                              error);
                    }
                    ++i;
        }] fetchCurrentPolicyWithContainer:container context:context modelIDOverride:modelIDOverride isInheritedAccount:isInheritedAccount reply:reply];
    } while (retry);
}


- (void)validatePeersWithContainer:(NSString *)container
                           context:(NSString *)context
                             reply:(void (^)(NSDictionary * _Nullable, NSError * _Nullable))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, error);
                    }
                    ++i;
                }] validatePeersWithContainer:container context:context reply:reply];
    } while (retry);
}

- (void)fetchTrustStateWithContainer:(NSString *)container
                             context:(NSString *)context
                               reply:(void (^)(TrustedPeersHelperPeerState* _Nullable selfPeerState,
                                               NSArray<TrustedPeersHelperPeer*>* _Nullable trustedPeers,
                                               NSError* _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, error);
                    }
                    ++i;
                }] fetchTrustStateWithContainer:container context:context reply:reply];
    } while (retry);
}

- (void)setRecoveryKeyWithContainer:(NSString *)container
                            context:(NSString *)context
                        recoveryKey:(NSString *)recoveryKey
                               salt:(NSString *)salt
                           ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)ckksKeys
                              reply:(void (^)(NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                              NSError* _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, error);
                    }
                    ++i;
                }] setRecoveryKeyWithContainer:container context:context recoveryKey:recoveryKey salt:salt ckksKeys:ckksKeys reply:reply];
    } while (retry);
}

- (void)createCustodianRecoveryKeyWithContainer:(NSString *)container
                                        context:(NSString *)context
                                    recoveryKey:(NSString *)recoveryString
                                           salt:(NSString *)salt
                                       ckksKeys:(NSArray<CKKSKeychainBackedKeySet*> *)ckksKeys
                                           uuid:(NSUUID *)uuid
                                           kind:(TPPBCustodianRecoveryKey_Kind)kind
                                          reply:(void (^)(NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                          TrustedPeersHelperCustodianRecoveryKey* _Nullable crk,
                                                          NSError* _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(nil, nil, error);
                    }
                    ++i;
                }] createCustodianRecoveryKeyWithContainer:container context:context recoveryKey:recoveryString salt:salt ckksKeys:ckksKeys uuid:uuid kind:kind reply:reply];
    } while (retry);
}

- (void)removeCustodianRecoveryKeyWithContainer:(NSString *)container
                                        context:(NSString *)context
                                           uuid:(NSUUID *)uuid
                                          reply:(void (^)(NSError* _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(error);
                    }
                    ++i;
                }] removeCustodianRecoveryKeyWithContainer:container context:context uuid:uuid reply:reply];
    } while (retry);
}

- (void)reportHealthWithContainer:(NSString *)container
                          context:(NSString *)context
                stateMachineState:(NSString *)state
                       trustState:(NSString *)trustState
                            reply:(void (^)(NSError* _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(error);
                    }
                    ++i;
                }] reportHealthWithContainer:container context:context stateMachineState:state trustState:trustState reply:reply];
    } while (retry);
}

- (void)pushHealthInquiryWithContainer:(NSString *)container
                               context:(NSString *)context
                                 reply:(void (^)(NSError* _Nullable error))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(error);
                    }
                    ++i;
                }] pushHealthInquiryWithContainer:container context:context reply:reply];
    } while (retry);
}

- (void)requestHealthCheckWithContainer:(NSString *)container
                                context:(NSString *)context
                    requiresEscrowCheck:(BOOL)requiresEscrowCheck
                       knownFederations:(NSArray<NSString *> *)knownFederations
                                  reply:(void (^)(BOOL postRepairCFU, BOOL postEscrowCFU, BOOL resetOctagon, BOOL leaveTrust, OTEscrowMoveRequestContext *moveRequest, NSError* _Nullable))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
                    if (i < NUM_RETRIES && [self.class retryable:error]) {
                        secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                        retry = true;
                    } else {
                        secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                        reply(NO, NO, NO, NO, nil, error);
                    }
                    ++i;
        }] requestHealthCheckWithContainer:container context:context requiresEscrowCheck:requiresEscrowCheck knownFederations:knownFederations reply:reply];
    } while (retry);
}

- (void)getSupportAppInfoWithContainer:(NSString *)container
                               context:(NSString *)context
                                 reply:(void (^)(NSData * _Nullable, NSError * _Nullable))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
            if (i < NUM_RETRIES && [self.class retryable:error]) {
                secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                retry = true;
            } else {
                secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                reply(nil, error);
            }
            ++i;
        }] getSupportAppInfoWithContainer:container context:context reply:reply];
    } while (retry);

}

- (void)fetchViableEscrowRecordsWithContainer:(nonnull NSString *)container context:(nonnull NSString *)context forceFetch:(BOOL)forceFetch reply:(nonnull void (^)(NSArray<NSData *> * _Nullable, NSError * _Nullable))reply
{
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
            if (i < NUM_RETRIES && [self.class retryable:error]) {
                secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                retry = true;
            } else {
                secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                reply(nil, error);
            }
            ++i;
        }] fetchViableEscrowRecordsWithContainer:container context:context forceFetch:forceFetch reply:reply];
    } while (retry);
}

- (void)removeEscrowCacheWithContainer:(nonnull NSString *)container context:(nonnull NSString *)context reply:(nonnull void (^)(NSError * _Nullable))reply {
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
            if (i < NUM_RETRIES && [self.class retryable:error]) {
                secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                retry = true;
            } else {
                secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                reply(error);
            }
            ++i;
        }] removeEscrowCacheWithContainer:container context:context reply:reply];
    } while (retry);
}

- (void)resetAccountCDPContentsWithContainer:(nonnull NSString *)container context:(nonnull NSString *)context reply:(nonnull void (^)(NSError * _Nullable))reply {
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
            if (i < NUM_RETRIES && [self.class retryable:error]) {
                secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                retry = true;
            } else {
                secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                reply(error);
            }
            ++i;
        }] resetAccountCDPContentsWithContainer:container context:context reply:reply];
    } while (retry);
}

- (void)fetchRecoverableTLKSharesWithContainer:(nonnull NSString *)container
                                       context:(nonnull NSString *)context
                                        peerID:(NSString * _Nullable)peerID
                                         reply:(nonnull void (^)(NSArray<CKRecord *> * _Nullable, NSError * _Nullable))reply {
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
            if (i < NUM_RETRIES && [self.class retryable:error]) {
                secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                retry = true;
            } else {
                secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                reply(nil, error);
            }
            ++i;
        }] fetchRecoverableTLKSharesWithContainer:container context:context peerID:peerID reply:reply];
    } while (retry);
}

- (void)fetchAccountSettingsWithContainer:(nonnull NSString *)container
                                  context:(nonnull NSString *)context
                                    reply:(nonnull void (^)(NSDictionary<NSString*, TPPBPeerStableInfoSetting *> * _Nullable, NSError * _Nullable))reply {
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
            if (i < NUM_RETRIES && [self.class retryable:error]) {
                secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                retry = true;
            } else {
                secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                reply(nil, error);
            }
            ++i;
        }] fetchAccountSettingsWithContainer:container context:context reply:reply];
    } while (retry);
}

- (void)prepareInheritancePeerWithContainer:(nonnull NSString *)container
                                    context:(nonnull NSString *)context
                                      epoch:(unsigned long long)epoch
                                  machineID:(nonnull NSString *)machineID
                                 bottleSalt:(nonnull NSString *)bottleSalt
                                   bottleID:(nonnull NSString *)bottleID
                                    modelID:(nonnull NSString *)modelID
                                 deviceName:(nullable NSString *)deviceName
                               serialNumber:(nullable NSString *)serialNumber
                                  osVersion:(nonnull NSString *)osVersion
                              policyVersion:(nullable TPPolicyVersion *)policyVersion
                              policySecrets:(nullable NSDictionary<NSString *,NSData *> *)policySecrets syncUserControllableViews:(TPPBPeerStableInfoUserControllableViewStatus)syncUserControllableViews
                      secureElementIdentity:(nullable TPPBSecureElementIdentity *)secureElementIdentity
                signingPrivKeyPersistentRef:(nullable NSData *)spkPr
                    encPrivKeyPersistentRef:(nullable NSData *)epkPr
                                        crk:(nonnull TrustedPeersHelperCustodianRecoveryKey *)crk
                                      reply:(nonnull void (^)(NSString * _Nullable, NSData * _Nullable, NSData * _Nullable, NSData * _Nullable, NSData * _Nullable, TPSyncingPolicy * _Nullable, NSString * _Nullable,                                                       NSArray<CKRecord*>* _Nullable keyHierarchyRecords, NSError * _Nullable))reply {
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
            if (i < NUM_RETRIES && [self.class retryable:error]) {
                secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                retry = true;
            } else {
                secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                reply(nil, nil, nil, nil, nil, nil, nil, nil, error);
            }
            ++i;
        }] prepareInheritancePeerWithContainer:container
         context:context
         epoch:epoch
         machineID:machineID
         bottleSalt:bottleSalt
         bottleID:bottleID
         modelID:modelID
         deviceName:deviceName
         serialNumber:serialNumber
         osVersion:osVersion
         policyVersion:policyVersion
         policySecrets:policySecrets
         syncUserControllableViews:syncUserControllableViews
         secureElementIdentity:secureElementIdentity
         signingPrivKeyPersistentRef:spkPr
         encPrivKeyPersistentRef:epkPr
         crk:crk
         reply:reply];
    } while (retry);
}

- (void)recoverTLKSharesForInheritorWithContainer:(nonnull NSString *)container
                                          context:(nonnull NSString *)context
                                              crk:(nonnull TrustedPeersHelperCustodianRecoveryKey *)crk
                                        tlkShares:(nonnull NSArray<CKKSTLKShare *> *)tlkShares
                                            reply:(nonnull void (^)(NSArray<CKKSTLKShare *> * _Nullable,
                                                                    TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                                    NSError * _Nullable))reply {
    __block int i = 0;
    __block bool retry;
    do {
        retry = false;
        [[self.cuttlefishXPCConnection synchronousRemoteObjectProxyWithErrorHandler:^(NSError *_Nonnull error) {
            if (i < NUM_RETRIES && [self.class retryable:error]) {
                secnotice("octagon", "retrying cuttlefish XPC, (%d, %@)", i, error);
                retry = true;
            } else {
                secerror("octagon: Can't talk with TrustedPeersHelper: %@", error);
                reply(nil, nil, error);
            }
            ++i;
        }] recoverTLKSharesForInheritorWithContainer:container context:context crk:crk tlkShares:tlkShares reply:reply];
    } while (retry);
}

@end
