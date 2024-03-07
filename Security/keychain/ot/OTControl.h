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

#if __OBJC2__

#ifndef OTCONTROL_H
#define OTCONTROL_H

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>

#import <AppleFeatures/AppleFeatures.h>

#import <Security/OTConstants.h>
#import <Security/OTClique.h>

NS_ASSUME_NONNULL_BEGIN

@class OTAccountSettings;
@class OTWebAccess;
@class OTWalrus;

@class OTCurrentSecureElementIdentities;
@class OTCustodianRecoveryKey;
@class OTInheritanceKey;
@class OTJoiningConfiguration;
@class OTSecureElementPeerIdentity;
@class OTAccountMetadataClassC;

@class TrustedPeersHelperHealthCheckResult;

@interface OTControlArguments : NSObject <NSSecureCoding>
@property (strong) NSString* contextID;
@property (strong) NSString* containerName;
@property (strong, nullable) NSString* altDSID;
@property (strong, nullable) NSString* flowID;
@property (strong, nullable) NSString* deviceSessionID;

- (instancetype)init;
- (instancetype)initWithConfiguration:(OTConfigurationContext*)configuration;
- (instancetype)initWithAltDSID:(NSString* _Nullable)altDSID;
- (instancetype)initWithAltDSID:(NSString* _Nullable)altDSID
                         flowID:(NSString* _Nullable)flowID
                deviceSessionID:(NSString* _Nullable)deviceSessionID;
- (instancetype)initWithContainerName:(NSString* _Nullable)containerName
                            contextID:(NSString*)contextID
                              altDSID:(NSString* _Nullable)altDSID;

- (instancetype)initWithContainerName:(NSString* _Nullable)containerName
                            contextID:(NSString*)contextID
                              altDSID:(NSString* _Nullable)altDSID
                               flowID:(NSString* _Nullable)flowID
                      deviceSessionID:(NSString* _Nullable)deviceSessionID;


- (OTConfigurationContext*)makeConfigurationContext;
@end

@interface OTControl : NSObject

@property (assign) BOOL synchronous;

+ (OTControl* _Nullable)controlObject:(NSError* _Nullable __autoreleasing* _Nullable)error;
+ (OTControl* _Nullable)controlObject:(bool)sync error:(NSError* _Nullable *)error;

- (instancetype)initWithConnection:(NSXPCConnection*)connection sync:(bool)sync;

- (void)restore:(NSString *)contextID dsid:(NSString *)dsid secret:(NSData*)secret escrowRecordID:(NSString*)escrowRecordID
          reply:(void (^)(NSData* signingKeyData, NSData* encryptionKeyData, NSError* _Nullable error))reply
API_DEPRECATED("Use OTClique API", macos(10.14, 10.15.1), ios(4, 17.2));
- (void)encryptionKey:(void (^)(NSData* result, NSError* _Nullable error))reply
API_DEPRECATED("No longer needed", macos(10.14, 10.15.1), ios(4, 17.2));
- (void)signingKey:(void (^)(NSData* result, NSError* _Nullable error))reply
API_DEPRECATED("No longer needed", macos(10.14, 10.15.1), ios(4, 17.2));
- (void)listOfRecords:(void (^)(NSArray* list, NSError* _Nullable error))reply
API_DEPRECATED("No longer needed", macos(10.14, 10.15.1), ios(4, 17.2));
- (void)reset:(void (^)(BOOL result, NSError* _Nullable error))reply
API_DEPRECATED("No longer needed", macos(10.14, 10.15.1), ios(4, 17.2));

- (void)appleAccountSignedIn:(OTControlArguments*)arguments reply:(void (^)(NSError * _Nullable error))reply;
- (void)appleAccountSignedOut:(OTControlArguments*)arguments reply:(void (^)(NSError * _Nullable error))reply;
- (void)notifyIDMSTrustLevelChangeForAltDSID:(OTControlArguments*)arguments reply:(void (^)(NSError * _Nullable error))reply;

- (void)rpcEpochWithArguments:(OTControlArguments*)arguments
                configuration:(OTJoiningConfiguration*)config
                        reply:(void (^)(uint64_t epoch,
                                        NSError * _Nullable error))reply;

- (void)rpcPrepareIdentityAsApplicantWithArguments:(OTControlArguments*)arguments
                                     configuration:(OTJoiningConfiguration*)config
                                             reply:(void (^)(NSString * _Nullable peerID,
                                                             NSData * _Nullable permanentInfo,
                                                             NSData * _Nullable permanentInfoSig,
                                                             NSData * _Nullable stableInfo,
                                                             NSData * _Nullable stableInfoSig,
                                                             NSError * _Nullable error))reply;
- (void)rpcVoucherWithArguments:(OTControlArguments*)arguments
                  configuration:(OTJoiningConfiguration*)config
                         peerID:(NSString*)peerID
                  permanentInfo:(NSData *)permanentInfo
               permanentInfoSig:(NSData *)permanentInfoSig
                     stableInfo:(NSData *)stableInfo
                  stableInfoSig:(NSData *)stableInfoSig
                          reply:(void (^)(NSData* voucher, NSData* voucherSig, NSError * _Nullable error))reply;

- (void)rpcJoinWithArguments:(OTControlArguments*)arguments
               configuration:(OTJoiningConfiguration*)config
                   vouchData:(NSData*)vouchData
                    vouchSig:(NSData*)vouchSig
                       reply:(void (^)(NSError * _Nullable error))reply;



// Call this to 'preflight' a bottled peer entry. This will create sufficient entropy, derive and save all relevant keys,
// then return the entropy to the caller. If something goes wrong during this process, do not store the returned entropy.
- (void)preflightBottledPeer:(NSString*)contextID
                        dsid:(NSString*)dsid
                       reply:(void (^)(NSData* _Nullable entropy,
                                       NSString* _Nullable bottleID,
                                       NSData* _Nullable signingPublicKey,
                                       NSError* _Nullable error))reply
API_DEPRECATED("Use OTClique API", macos(10.14, 10.15), ios(4, 17));

// Call this to 'launch' a preflighted bottled peer entry. This indicates that you've successfully stored the entropy,
// and we should save the bottled peer entry off-device for later retrieval.
- (void)launchBottledPeer:(NSString*)contextID
                 bottleID:(NSString*)bottleID
                    reply:(void (^ _Nullable)(NSError* _Nullable error))reply
API_DEPRECATED("No longer needed", macos(10.14, 10.15), ios(4, 17));

// Call this to scrub the launch of a preflighted bottled peer entry. This indicates you've terminally failed to store the
// preflighted entropy, and this bottled peer will never be used again and can be deleted.
- (void)scrubBottledPeer:(NSString*)contextID
                bottleID:(NSString*)bottleID
                   reply:(void (^ _Nullable)(NSError* _Nullable error))reply
API_DEPRECATED("No longer needed", macos(10.14, 10.15), ios(4, 17));

- (void)status:(NSString* _Nullable)container
       context:(NSString*)context
         reply:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply;

- (void)status:(OTControlArguments*)arguments
         reply:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply;

- (void)fetchEgoPeerID:(OTControlArguments*)arguments
                 reply:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))reply;

- (void)fetchCliqueStatus:(OTControlArguments*)arguments
            configuration:(OTOperationConfiguration *)configuration
                    reply:(void (^)(CliqueStatus cliqueStatus, NSError* _Nullable error))reply;

- (void)fetchTrustStatus:(OTControlArguments*)arguments
           configuration:(OTOperationConfiguration *)configuration
                   reply:(void (^)(CliqueStatus status,
                                   NSString* _Nullable peerID,
                                   NSNumber * _Nullable numberOfOctagonPeers,
                                   BOOL isExcluded,
                                   NSError * _Nullable error))reply;

// Likely won't be used once Octagon is turned on for good
- (void)startOctagonStateMachine:(OTControlArguments*)arguments
                           reply:(void (^)(NSError* _Nullable error))reply;

- (void)resetAndEstablish:(OTControlArguments*)arguments
              resetReason:(CuttlefishResetReason)resetReason
        idmsTargetContext:(NSString *_Nullable)idmsTargetContext
   idmsCuttlefishPassword:(NSString *_Nullable)idmsCuttlefishPassword
	       notifyIdMS:(bool)notifyIdMS
          accountSettings:(OTAccountSettings *_Nullable)accountSettings
                    reply:(void (^)(NSError* _Nullable error))reply;

- (void)establish:(OTControlArguments*)arguments
            reply:(void (^)(NSError * _Nullable))reply;

- (void)leaveClique:(OTControlArguments*)arguments
              reply:(void (^)(NSError* _Nullable error))reply;

- (void)removeFriendsInClique:(OTControlArguments*)arguments
                      peerIDs:(NSArray<NSString*>*)peerIDs
                        reply:(void (^)(NSError* _Nullable error))reply;

- (void)peerDeviceNamesByPeerID:(OTControlArguments*)arguments
                          reply:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply;

- (void)fetchAllViableBottles:(OTControlArguments*)arguments
                       source:(OTEscrowRecordFetchSource)source
                        reply:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs, NSArray<NSString*> * _Nullable sortedPartialBottleIDs, NSError* _Nullable error))reply;

- (void)restoreFromBottle:(OTControlArguments*)arguments
                  entropy:(NSData *)entropy
                 bottleID:(NSString *)bottleID
                    reply:(void (^)(NSError * _Nullable))reply;

- (void)fetchEscrowContents:(OTControlArguments*)arguments
                      reply:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply;

- (void)createRecoveryKey:(OTControlArguments*)arguments
              recoveryKey:(NSString *)recoveryKey
                    reply:(void (^)( NSError * _Nullable))reply;

- (void)joinWithRecoveryKey:(OTControlArguments*)arguments
                recoveryKey:(NSString*)recoveryKey
                      reply:(void (^)(NSError * _Nullable))reply;

- (void)createCustodianRecoveryKey:(OTControlArguments*)arguments
                              uuid:(NSUUID *_Nullable)uuid
                             reply:(void (^)(OTCustodianRecoveryKey *_Nullable crk, NSError *_Nullable error))reply;

- (void)joinWithCustodianRecoveryKey:(OTControlArguments*)arguments
                custodianRecoveryKey:(OTCustodianRecoveryKey *)crk
                               reply:(void(^)(NSError* _Nullable error)) reply;

- (void)preflightJoinWithCustodianRecoveryKey:(OTControlArguments*)arguments
                         custodianRecoveryKey:(OTCustodianRecoveryKey *)crk
                                        reply:(void(^)(NSError* _Nullable error)) reply;

- (void)removeCustodianRecoveryKey:(OTControlArguments*)arguments
                              uuid:(NSUUID *)uuid
                             reply:(void (^)(NSError *_Nullable error))reply;

- (void)checkCustodianRecoveryKey:(OTControlArguments*)arguments
                             uuid:(NSUUID *)uuid
                            reply:(void (^)(bool exists, NSError *_Nullable error))reply;

- (void)createInheritanceKey:(OTControlArguments*)arguments
                        uuid:(NSUUID *_Nullable)uuid
                       reply:(void (^)(OTInheritanceKey *_Nullable ik, NSError *_Nullable error))reply;

- (void)generateInheritanceKey:(OTControlArguments*)arguments
                          uuid:(NSUUID *_Nullable)uuid
                         reply:(void (^)(OTInheritanceKey *_Nullable ik, NSError *_Nullable error))reply;

- (void)storeInheritanceKey:(OTControlArguments*)arguments
                         ik:(OTInheritanceKey *)ik
                      reply:(void (^)(NSError *_Nullable error)) reply;

- (void)joinWithInheritanceKey:(OTControlArguments*)arguments
                inheritanceKey:(OTInheritanceKey *)ik
                         reply:(void(^)(NSError* _Nullable error)) reply;

- (void)preflightJoinWithInheritanceKey:(OTControlArguments*)arguments
                         inheritanceKey:(OTInheritanceKey *)ik
                                  reply:(void(^)(NSError* _Nullable error)) reply;

- (void)removeInheritanceKey:(OTControlArguments*)arguments
                        uuid:(NSUUID *)uuid
                       reply:(void (^)(NSError *_Nullable error))reply;

- (void)checkInheritanceKey:(OTControlArguments*)arguments
                       uuid:(NSUUID *)uuid
                      reply:(void (^)(bool exists, NSError *_Nullable error))reply;

- (void)healthCheck:(OTControlArguments*)arguments
skipRateLimitingCheck:(BOOL)skipRateLimitingCheck
             repair:(BOOL)repair
reply:(void (^)(TrustedPeersHelperHealthCheckResult *_Nullable results, NSError *_Nullable error))reply;

- (void)simulateReceivePush:(OTControlArguments*)arguments
                      reply:(void (^)(NSError *_Nullable error))reply;

- (void)waitForOctagonUpgrade:(OTControlArguments*)arguments
                        reply:(void (^)(NSError* _Nullable error))reply;

- (void)postCDPFollowupResult:(OTControlArguments*)arguments
                      success:(BOOL)success
                         type:(OTCliqueCDPContextType)type
                        error:(NSError * _Nullable)error
                        reply:(void (^)(NSError* _Nullable error))reply;

- (void)tapToRadar:(NSString *)action
       description:(NSString *)description
             radar:(NSString *)radar
             reply:(void (^)(NSError* _Nullable error))reply;

- (void)setCDPEnabled:(OTControlArguments*)arguments
                reply:(void (^)(NSError* _Nullable error))reply;

- (void)getCDPStatus:(OTControlArguments*)arguments
               reply:(void (^)(OTCDPStatus status, NSError* _Nullable error))reply;

- (void)refetchCKKSPolicy:(OTControlArguments*)arguments
                    reply:(void (^)(NSError* _Nullable error))reply;

- (void)fetchEscrowRecords:(OTControlArguments*)arguments
                    source:(OTEscrowRecordFetchSource)source
                     reply:(void (^)(NSArray<NSData*>* _Nullable records,
                                     NSError* _Nullable error))reply;

- (void)setUserControllableViewsSyncStatus:(OTControlArguments*)arguments
                                   enabled:(BOOL)enabled
                                     reply:(void (^)(BOOL nowSyncing, NSError* _Nullable error))reply;

- (void)fetchUserControllableViewsSyncStatus:(OTControlArguments*)arguments
                                       reply:(void (^)(BOOL nowSyncing, NSError* _Nullable error))reply;

- (void)invalidateEscrowCache:(OTControlArguments*)arguments
                        reply:(nonnull void (^)(NSError * _Nullable error))reply;

- (void)resetAccountCDPContents:(OTControlArguments*)arguments
        idmsTargetContext:(NSString *_Nullable)idmsTargetContext
   idmsCuttlefishPassword:(NSString *_Nullable)idmsCuttlefishPassword
	       notifyIdMS:(bool)notifyIdMS
                          reply:(void (^)(NSError* _Nullable error))reply;


- (void)setLocalSecureElementIdentity:(OTControlArguments*)arguments
                secureElementIdentity:(OTSecureElementPeerIdentity*)secureElementIdentity
                                reply:(void (^)(NSError* _Nullable))reply;

- (void)removeLocalSecureElementIdentityPeerID:(OTControlArguments*)arguments
                   secureElementIdentityPeerID:(NSData*)sePeerID
                                         reply:(void (^)(NSError* _Nullable))reply;

- (void)fetchTrustedSecureElementIdentities:(OTControlArguments*)arguments
                                      reply:(void (^)(OTCurrentSecureElementIdentities* _Nullable currentSet,
                                                      NSError* _Nullable replyError))reply;


- (void)setAccountSetting:(OTControlArguments*)arguments
                  setting:(OTAccountSettings*)setting
                    reply:(void (^)(NSError* _Nullable))reply;

- (void)fetchAccountSettings:(OTControlArguments*)arguments
                       reply:(void (^)(OTAccountSettings* _Nullable setting, NSError* _Nullable replyError))reply;

- (void)fetchAccountWideSettingsWithForceFetch:(bool)forceFetch
                                     arguments:(OTControlArguments*)arguments
                                         reply:(void (^)(OTAccountSettings* _Nullable setting, NSError* _Nullable error))reply;

- (void)waitForPriorityViewKeychainDataRecovery:(OTControlArguments*)arguments
                                          reply:(void (^)(NSError* _Nullable replyError))reply;

- (void)tlkRecoverabilityForEscrowRecordData:(OTControlArguments*)arguments
                                  recordData:(NSData*)recordData
                                      source:(OTEscrowRecordFetchSource)source
                                       reply:(void (^)(NSArray<NSString*>* _Nullable views, NSError* _Nullable error))reply;

- (void)setMachineIDOverride:(OTControlArguments*)arguments
                   machineID:(NSString*)machineID
                       reply:(void (^)(NSError* _Nullable replyError))reply;

- (void)isRecoveryKeySet:(OTControlArguments*)arguments
                   reply:(void (^)(BOOL isSet, NSError* _Nullable error))reply;

- (void)recoverWithRecoveryKey:(OTControlArguments*)arguments
                   recoveryKey:(NSString*)recoveryKey
                         reply:(void (^)(NSError* _Nullable error))reply;

- (void)removeRecoveryKey:(OTControlArguments*)arguments
                    reply:(void (^)(NSError* _Nullable error))reply;

- (void)preflightRecoverOctagonUsingRecoveryKey:(OTControlArguments*)arguments
                                    recoveryKey:(NSString*)recoveryKey
                                          reply:(void (^)(BOOL correct, NSError* _Nullable replyError))reply;

- (void)getAccountMetadata:(OTControlArguments*)arguments
                     reply:(void (^)(OTAccountMetadataClassC* _Nullable metadata, NSError* _Nullable replyError))reply;

- (void)resetAcountData:(OTControlArguments*)arguments
            resetReason:(CuttlefishResetReason)resetReason
                  reply:(void (^)(NSError* _Nullable error))reply;

- (void)totalTrustedPeers:(OTControlArguments*)arguments
                    reply:(void (^)(NSNumber* _Nullable count, NSError* _Nullable error))reply;

- (void)areRecoveryKeysDistrusted:(OTControlArguments*)arguments
                            reply:(void (^)(BOOL distrustedRecoveryKeysExist, NSError* _Nullable error))reply;

- (void)reroll:(OTControlArguments*)arguments
         reply:(void (^)(NSError *_Nullable error))reply;

@end

NS_ASSUME_NONNULL_END

#endif // OTCONTROL_H
#endif  // __OBJC__
