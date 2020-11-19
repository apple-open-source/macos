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
#if !TARGET_OS_BRIDGE // SecurityFoundation not mastered on BridgeOS
#import <SecurityFoundation/SFKey.h>
#else
@class SFECKeyPair;
#endif

#import <Security/OTConstants.h>
#import <Security/OTClique.h>

#if !TARGET_OS_BRIDGE // SecurityFoundation not mastered on BridgeOS
#import <SecurityFoundation/SFKey.h>
#else
@class SFECKeyPair;
#endif

NS_ASSUME_NONNULL_BEGIN

@class OTJoiningConfiguration;


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

- (void)signIn:(NSString*)dsid container:(NSString* _Nullable)container context:(NSString*)contextID reply:(void (^)(NSError * _Nullable error))reply;
- (void)signOut:(NSString* _Nullable)container context:(NSString*)contextID reply:(void (^)(NSError * _Nullable error))reply;
- (void)notifyIDMSTrustLevelChangeForContainer:(NSString* _Nullable)container context:(NSString*)contextID reply:(void (^)(NSError * _Nullable error))reply;

- (void)handleIdentityChangeForSigningKey:(SFECKeyPair* _Nonnull)peerSigningKey
                         ForEncryptionKey:(SFECKeyPair* _Nonnull)encryptionKey
                                ForPeerID:(NSString*)peerID
                                    reply:(void (^)(BOOL result,
                                                    NSError* _Nullable error))reply
    API_DEPRECATED("No longer needed", macos(10.14, 10.15.1), ios(4, 17.2));

- (void)rpcEpochWithConfiguration:(OTJoiningConfiguration*)config
                            reply:(void (^)(uint64_t epoch,
                                            NSError * _Nullable error))reply;

- (void)rpcPrepareIdentityAsApplicantWithConfiguration:(OTJoiningConfiguration*)config
                                                 reply:(void (^)(NSString * _Nullable peerID,
                                                                 NSData * _Nullable permanentInfo,
                                                                 NSData * _Nullable permanentInfoSig,
                                                                 NSData * _Nullable stableInfo,
                                                                 NSData * _Nullable stableInfoSig,
                                                                 NSError * _Nullable error))reply;
- (void)rpcVoucherWithConfiguration:(OTJoiningConfiguration*)config
                             peerID:(NSString*)peerID
                      permanentInfo:(NSData *)permanentInfo
                   permanentInfoSig:(NSData *)permanentInfoSig
                         stableInfo:(NSData *)stableInfo
                      stableInfoSig:(NSData *)stableInfoSig
                              reply:(void (^)(NSData* voucher, NSData* voucherSig, NSError * _Nullable error))reply;

- (void)rpcJoinWithConfiguration:(OTJoiningConfiguration*)config
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

- (void)fetchEgoPeerID:(NSString* _Nullable)container
               context:(NSString*)context
                 reply:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))reply;

- (void)fetchCliqueStatus:(NSString* _Nullable)container
                  context:(NSString*)context
            configuration:(OTOperationConfiguration *)configuration
                    reply:(void (^)(CliqueStatus cliqueStatus, NSError* _Nullable error))reply;

- (void)fetchTrustStatus:(NSString* _Nullable)container
                 context:(NSString*)context
           configuration:(OTOperationConfiguration *)configuration
                   reply:(void (^)(CliqueStatus status,
                                   NSString* _Nullable peerID,
                                   NSNumber * _Nullable numberOfOctagonPeers,
                                   BOOL isExcluded,
                                   NSError * _Nullable error))reply;

// Likely won't be used once Octagon is turned on for good
- (void)startOctagonStateMachine:(NSString* _Nullable)container
                         context:(NSString*)context
                           reply:(void (^)(NSError* _Nullable error))reply;

- (void)resetAndEstablish:(NSString* _Nullable)container
                  context:(NSString*)context
                  altDSID:(NSString*)altDSID
              resetReason:(CuttlefishResetReason)resetReason
                    reply:(void (^)(NSError* _Nullable error))reply;

- (void)establish:(NSString* _Nullable)container
          context:(NSString*)context
          altDSID:(NSString*)altDSID
            reply:(void (^)(NSError* _Nullable error))reply;

- (void)leaveClique:(NSString* _Nullable)container
            context:(NSString*)context
              reply:(void (^)(NSError* _Nullable error))reply;

- (void)removeFriendsInClique:(NSString* _Nullable)container
                      context:(NSString*)context
                      peerIDs:(NSArray<NSString*>*)peerIDs
                        reply:(void (^)(NSError* _Nullable error))reply;

- (void)peerDeviceNamesByPeerID:(NSString* _Nullable)container
                        context:(NSString*)context
                          reply:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply;

- (void)fetchAllViableBottles:(NSString* _Nullable)container
                      context:(NSString*)context
                        reply:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs, NSArray<NSString*> * _Nullable sortedPartialBottleIDs, NSError* _Nullable error))reply;

-(void)restore:(NSString* _Nullable)containerName
     contextID:(NSString *)contextID
    bottleSalt:(NSString *)bottleSalt
       entropy:(NSData *)entropy
      bottleID:(NSString *)bottleID
         reply:(void (^)(NSError * _Nullable))reply;

- (void)fetchEscrowContents:(NSString* _Nullable)containerName
                  contextID:(NSString *)contextID
                      reply:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply;

- (void) createRecoveryKey:(NSString* _Nullable)containerName
                 contextID:(NSString *)contextID
               recoveryKey:(NSString *)recoveryKey
                     reply:(void (^)( NSError * _Nullable))reply;

- (void) joinWithRecoveryKey:(NSString* _Nullable)containerName
                   contextID:(NSString *)contextID
                 recoveryKey:(NSString*)recoveryKey
                       reply:(void (^)(NSError * _Nullable))reply;

- (void)healthCheck:(NSString* _Nullable)container
            context:(NSString *)context
skipRateLimitingCheck:(BOOL)skipRateLimitingCheck
              reply:(void (^)(NSError *_Nullable error))reply;

- (void)waitForOctagonUpgrade:(NSString* _Nullable)container
                      context:(NSString*)context
                        reply:(void (^)(NSError* _Nullable error))reply;

- (void)postCDPFollowupResult:(BOOL)success
                         type:(OTCliqueCDPContextType)type
                        error:(NSError * _Nullable)error
                containerName:(NSString* _Nullable)containerName
                  contextName:(NSString *)contextName
                        reply:(void (^)(NSError* _Nullable error))reply;

- (void)tapToRadar:(NSString *)action
       description:(NSString *)description
             radar:(NSString *)radar
             reply:(void (^)(NSError* _Nullable error))reply;

- (void)setCDPEnabled:(NSString* _Nullable)containerName
            contextID:(NSString*)contextID
                reply:(void (^)(NSError* _Nullable error))reply;

- (void)getCDPStatus:(NSString* _Nullable)containerName
           contextID:(NSString*)contextID
               reply:(void (^)(OTCDPStatus status, NSError* _Nullable error))reply;

- (void)refetchCKKSPolicy:(NSString* _Nullable)containerName
                contextID:(NSString*)contextID
                    reply:(void (^)(NSError* _Nullable error))reply;


- (void)fetchEscrowRecords:(NSString * _Nullable)container
                 contextID:(NSString*)contextID
                forceFetch:(BOOL)forceFetch
                     reply:(void (^)(NSArray<NSData*>* _Nullable records,
                                     NSError* _Nullable error))reply;

- (void)setUserControllableViewsSyncStatus:(NSString* _Nullable)containerName
                                 contextID:(NSString*)contextID
                                   enabled:(BOOL)enabled
                                     reply:(void (^)(BOOL nowSyncing, NSError* _Nullable error))reply;

- (void)fetchUserControllableViewsSyncStatus:(NSString* _Nullable)containerName
                                   contextID:(NSString*)contextID
                                       reply:(void (^)(BOOL nowSyncing, NSError* _Nullable error))reply;

- (void)invalidateEscrowCache:(NSString * _Nullable)containerName
                    contextID:(NSString*)contextID
                        reply:(nonnull void (^)(NSError * _Nullable error))reply;

@end

NS_ASSUME_NONNULL_END

#endif // OTCONTROL_H
#endif  // __OBJC__
