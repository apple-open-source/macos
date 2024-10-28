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


#ifndef OTClique_h
#define OTClique_h

#if __OBJC2__

#import <Foundation/Foundation.h>
#import <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#import <Security/SecureObjectSync/SOSPeerInfo.h>
#import <Security/SecureObjectSync/SOSTypes.h>
#import <Security/OTConstants.h>
#import <Security/SecRecoveryKey.h>

typedef NS_ENUM(NSInteger, CliqueStatus) {
    CliqueStatusIn         = 0, /*There is a clique and I am in it*/
    CliqueStatusNotIn      = 1, /*There is a clique and I am not in it - you should get a voucher to join or tell another peer to trust us*/
    CliqueStatusPending    = 2, /*For compatibility, keeping the pending state */
    CliqueStatusAbsent     = 3, /*There is no clique - you can establish one */
    CliqueStatusNoCloudKitAccount = 4, /* no cloudkit account present */
    CliqueStatusError      = -1 /*unable to determine circle status, inspect CFError to find out why */
};

typedef NS_ENUM(NSInteger, OTCDPStatus) {
    OTCDPStatusUnknown = 0,
    OTCDPStatusDisabled = 1,
    OTCDPStatusEnabled = 2,
};

NS_ASSUME_NONNULL_BEGIN

NSString* OTCliqueStatusToString(CliqueStatus status);
CliqueStatus OTCliqueStatusFromString(NSString* str);
NSString* OTCDPStatusToString(OTCDPStatus status);

@class CKKSControl;
@class KCPairingChannel;
@class KCPairingChannelContext;
@class OTControl;
@class OTCustodianRecoveryKey;
@class OTInheritanceKey;
@class OTPairingChannel;
@class OTPairingChannelContext;

extern NSString* kSecEntitlementPrivateOctagonEscrow;
extern NSString* kSecEntitlementPrivateOctagonSecureElement;
extern NSString* kSecEntitlementPrivateOctagonWalrus;

@interface OTConfigurationContext : NSObject
@property (nonatomic, copy) NSString* context;
@property (nonatomic, copy) NSString* containerName;
@property (nonatomic, copy, nullable) NSString* dsid API_DEPRECATED_WITH_REPLACEMENT("altDSID", macos(10.15, 14.0), ios(13.0, 17.0), tvos(13.0, 17.0), watchos(6.0, 10.0));
@property (nonatomic, copy, nullable) NSString* altDSID;
@property (nonatomic, copy, nullable) NSString* authenticationAppleID;
@property (nonatomic, copy, nullable) NSString* passwordEquivalentToken;
@property (nonatomic) BOOL overrideEscrowCache; // SPI_DEPRECATED("use 'escrowFetchSource' instead", ios(14.0, 16.2));
@property (nonatomic) OTEscrowRecordFetchSource escrowFetchSource;
@property (nonatomic) BOOL octagonCapableRecordsExist;
@property (nonatomic) BOOL overrideForSetupAccountScript; // this should only be used for the account setup script
@property (nonatomic) BOOL overrideForJoinAfterRestore; // this should only be used in tests
// Use these when tracking metrics for RTC
@property (nonatomic, copy, nullable) NSString* flowID;
@property (nonatomic, copy, nullable) NSString* deviceSessionID;

// Use this to inject your own OTControl object. It must be configured as synchronous.
@property (nullable, strong) OTControl* otControl;

// Use this to inject your own CKKSControl object. It must be configured as synchronous.
@property (nullable, strong) CKKSControl* ckksControl;

// Use this to inject your own SecureBackup object. It must conform to the OctagonEscrowRecoverer protocol.
@property (nullable, strong) id sbd;

@property (nonatomic) BOOL testsEnabled;

// Create a new synchronous OTControl if one doesn't already exist in context.
- (OTControl* _Nullable)makeOTControl:(NSError**)error;
@end

// OTBottleIDs: an Obj-C Tuple

@interface OTBottleIDs : NSObject
@property (strong) NSArray<NSString*>* preferredBottleIDs;
@property (strong) NSArray<NSString*>* partialRecoveryBottleIDs;
@end

@interface OTOperationConfiguration : NSObject <NSSecureCoding>
@property (nonatomic, assign) uint64_t timeoutWaitForCKAccount;
@property (nonatomic, assign) NSQualityOfService qualityOfService;
@property (nonatomic, assign) BOOL discretionaryNetwork;
@property (nonatomic, assign) BOOL useCachedAccountStatus;
@end

typedef NSString* OTCliqueCDPContextType NS_STRING_ENUM;
extern OTCliqueCDPContextType OTCliqueCDPContextTypeNone;
extern OTCliqueCDPContextType OTCliqueCDPContextTypeSignIn;
extern OTCliqueCDPContextType OTCliqueCDPContextTypeRepair;
extern OTCliqueCDPContextType OTCliqueCDPContextTypeFinishPasscodeChange;

// These next two should be marked unavailable once CDP no longer uses them
extern OTCliqueCDPContextType OTCliqueCDPContextTypeRecoveryKeyGenerate; // API_UNAVAILABLE(watchos, tvos);
extern OTCliqueCDPContextType OTCliqueCDPContextTypeRecoveryKeyNew; // API_UNAVAILABLE(watchos, tvos);
extern OTCliqueCDPContextType OTCliqueCDPContextTypeUpdatePasscode;
extern OTCliqueCDPContextType OTCliqueCDPContextTypeConfirmPasscodeCyrus;


// OTClique

@interface OTClique : NSObject

+ (BOOL)platformSupportsSOS;

@property (nonatomic, readonly, nullable) NSString* cliqueMemberIdentifier;
@property (nonatomic, readonly, strong) OTConfigurationContext *ctx;

- (instancetype) init NS_UNAVAILABLE;

// MARK: Clique SPI

/* *
 * @abstract, initializes a clique object given a context.  A clique object enables octagon trust operations for a given context and dsid.
 * @param ctx, a collection of arguments describing the world
 * @return an instance of octagon trust
 */
- (instancetype)initWithContextData:(OTConfigurationContext*)ctx;

/* *
 * @abstract   Establish a new clique, reset protected data
 *   Reset the clique
 *   Delete backups
 *   Delete all CKKS data
 *
 * @param   ctx, context containing parameters to setup OTClique
 * @return  clique, returns a new clique instance
 * @param  error, error gets filled if something goes horribly wrong
 */
+ (instancetype _Nullable)newFriendsWithContextData:(OTConfigurationContext*)data error:(NSError * __autoreleasing *)error __deprecated_msg("use newFriendsWithContextData:resetReason:error: instead");

/* *
* @abstract   Establish a new clique, reset protected data
*   Reset the clique
*   Delete backups
*   Delete all CKKS data
*
* @param   ctx, context containing parameters to setup OTClique
* @param    resetReason, a reason that drives cdp to perform a reset
* @return  clique, returns a new clique instance
* @param  error, error gets filled if something goes horribly wrong
*/
+ (instancetype _Nullable)newFriendsWithContextData:(OTConfigurationContext*)data resetReason:(CuttlefishResetReason)resetReason error:(NSError * __autoreleasing *)error;

/*
 * @abstract Perform a SecureBackup escrow/keychain recovery and attempt to use the information therein to join this account.
 *       You do not need to call joinAfterRestore after calling this method.
 * @param data The OTClique configuration data
 * @param sbdRecoveryArguments the grab bag of things you'd normally pass to SecureBackup's recoverWithInfo.
 * @param error Reports any error along the process, including 'incorrect secret' and 'couldn't rejoin account'.
 * @return a fresh new OTClique, if the account rejoin was successful. Otherwise, nil.
 */
+ (OTClique* _Nullable)performEscrowRecoveryWithContextData:(OTConfigurationContext*)data
                                            escrowArguments:(NSDictionary*)sbdRecoveryArguments
                                                      error:(NSError**)error;

/* *
 * @abstract   Create pairing channel with
 *
 * @param ctx, context containing parameters to setup the pairing channel as the initiator
 * @return  KCPairingChannel, An instance of a KCPairingCHannel
 */
- (KCPairingChannel *)setupPairingChannelAsInitiator:(KCPairingChannelContext *)ctx;

- (KCPairingChannel * _Nullable)setupPairingChannelAsInitator:(KCPairingChannelContext *)ctx error:(NSError * __autoreleasing *)error __deprecated_msg("setupPairingChannelAsInitiator:error: deprecated, use setupPairingChannelAsInitiator:");

/* *
 * @abstract   Configure this peer as the acceptor during piggybacking
 *
 * @param ctx, context containing parameters to setup the pairing channel as the acceptor
 * @return  KCPairingChannel, An instance of a KCPairingChannel
 */
- (KCPairingChannel *)setupPairingChannelAsAcceptor:(KCPairingChannelContext *)ctx;

- (KCPairingChannel * _Nullable)setupPairingChannelAsAcceptor:(KCPairingChannelContext *)ctx error:(NSError * __autoreleasing *)error __deprecated_msg("setupPairingChannelAsAcceptor:error: deprecated, use setupPairingChannelAsAcceptor:");

/* *
 * @abstract Get the cached status of clique - returns one of:
 *       There is no clique - you can establish one
 *       There is a clique and I am not in it - you should get a voucher to join or tell another peer to trust us
 *       There is a clique and I am in it
 * @param error, error gets filled if something goes horribly wrong
 * @return  cached cliqueStatus, value will represent one of the above
 */
- (CliqueStatus)cachedCliqueStatus:(BOOL)useCached error:(NSError * __autoreleasing *)error
    __deprecated_msg("use fetchCliqueStatus:");

/* *
 * @abstract Get status of clique - returns one of:
 *       There is no clique - you can establish one
 *       There is a clique and I am not in it - you should get a voucher to join or tell another peer to trust us
 *       There is a clique and I am in it
 * @param error, error gets filled if something goes horribly wrong
 * @return  cliqueStatus, value will represent one of the above
 */
- (CliqueStatus)fetchCliqueStatus:(NSError * __autoreleasing * _Nonnull)error;

/* *
 * @abstract Get status of clique - returns one of:
 *       There is no clique - you can establish one
 *       There is a clique and I am not in it - you should get a voucher to join or tell another peer to trust us
 *       There is a clique and I am in it
 * @param configuration, behavior of operations performed follow up this operation
 * @param error, error gets filled if something goes horribly wrong
 * @return  cliqueStatus, value will represent one of the above
 */
- (CliqueStatus)fetchCliqueStatus:(OTOperationConfiguration *)configuration error:(NSError * __autoreleasing * _Nonnull)error;

/* *
 * @abstract Exclude given a member identifier
 * @param   friendIdentifiers, friends to remove
 * @param   error, error gets filled if something goes horribly wrong
 * @return  BOOL, YES if successful. No if call failed.
 */
- (BOOL)removeFriendsInClique:(NSArray<NSString*>*)friendIdentifiers error:(NSError * __autoreleasing *)error;

 /* *
  * @abstract Depart (exclude self)
  *            Un-enroll from escrow
  * @param   error, error gets filled if something goes horribly wrong
  * @return BOOL, YES if successful. No if call failed.
  */
- (BOOL)leaveClique:(NSError * __autoreleasing *)error;

/* *
 * @abstract Get list of peerIDs and device names
 * @param   error, error gets filled if something goes horribly wrong
 * @return friends, list of peer ids and their mapping to device names of all devices currently in the clique,
 *                  ex: NSDictionary[peerID, device Name];
 */
- (NSDictionary<NSString*,NSString*>* _Nullable)peerDeviceNamesByPeerID:(NSError * __autoreleasing *)error;

/*
 * CDP bit handling
 */

+ (BOOL)setCDPEnabled:(OTConfigurationContext*)arguments
                error:(NSError* __autoreleasing*)error;

+ (OTCDPStatus)getCDPStatus:(OTConfigurationContext*)arguments
                      error:(NSError* __autoreleasing *)error;

/*
 * User view handling
 */

/* *
 * @abstract Set the current status of user-controllable views. This is unavailable on TV and Watch, and will error.
 * @param error, This will return an error if anything goes wrong
 * @return success
 */
- (BOOL)setUserControllableViewsSyncStatus:(BOOL)enabled
                                     error:(NSError* __autoreleasing *)error;

/* *
 * @abstract Fetch the current status of user-controllable views
 * @param   error, This will return an error if anything goes wrong
 * @return status, The status of syncing. Note that in the success case, this can be NO while error remains empty.
 */
- (BOOL)fetchUserControllableViewsSyncingEnabled:(NSError* __autoreleasing *)error __attribute__((swift_error(nonnull_error)));

/* *
 * @abstract Fetch the current status of user-controllable views. This is _always_ asynchronous.
 * @param reply, A callback reporting the status of view syncing, plus any error that was encountered.
 */
- (void)fetchUserControllableViewsSyncingEnabledAsync:(void (^)(BOOL nowSyncing, NSError* _Nullable error))reply;

/* *
 * @abstract Establish a new OT circle
 * @param   error, This will return an error if anything goes wrong
 * @return YES if establish successfully created a new Octagon circle, NO if something went wrong.
 */
- (BOOL)establish:(NSError**)error;


/* SOS glue */

- (BOOL)joinAfterRestore:(NSError * __autoreleasing *)error
API_DEPRECATED("No longer needed", macos(10.15, 14.0), ios(13.0, 17.0), watchos(6.0, 10.0), tvos(13.0,17.0));

- (BOOL)waitForInitialSync:(NSError *__autoreleasing*)error
API_DEPRECATED("No longer needed", macos(10.15, 14.0), ios(13.0, 17.0), watchos(6.0, 10.0), tvos(13.0,17.0));

- (NSArray* _Nullable)copyViewUnawarePeerInfo:(NSError *__autoreleasing*)error
API_DEPRECATED("No longer needed", macos(10.15, 14.0), ios(13.0, 17.0), watchos(6.0, 10.0), tvos(13.0,17.0));

- (BOOL)setUserCredentialsWithLabel:(NSString*)userLabel
                           password:(NSData*)userPassword
                               dsid:(NSString*)dsid
                              error:(NSError *__autoreleasing*)error
    API_DEPRECATED("No longer needed", macos(14.0, 14.0), ios(17.0, 17.0), tvos(17.0, 17.0), watchos(10.0, 10.0));

- (BOOL)setUserCredentialsAndDSID:(NSString*)userLabel
                         password:(NSData*)userPassword
                            error:(NSError *__autoreleasing*)error
    API_DEPRECATED_WITH_REPLACEMENT("setUserCredentialsWithLabel:password:dsid:error:", macos(10.15, 14.0), ios(13.0, 17.0), tvos(13.0, 17.0), watchos(6.0, 10.0));

- (BOOL)tryUserCredentialsWithLabel:(NSString*)userLabel
                           password:(NSData*)userPassword
                               dsid:(NSString*)dsid
                              error:(NSError *__autoreleasing*)error
    API_DEPRECATED("No longer needed", macos(14.0, 14.0), ios(17.0, 17.0), tvos(17.0, 17.0), watchos(10.0, 10.0));

- (BOOL)tryUserCredentialsAndDSID:(NSString*)userLabel
                         password:(NSData*)userPassword
                            error:(NSError *__autoreleasing*)error
    API_DEPRECATED_WITH_REPLACEMENT("tryUserCredentialsWithLabel:password:dsid:error:", macos(10.15, 14.0), ios(13.0, 17.0), tvos(13.0, 17.0), watchos(6.0, 10.0));

- (NSArray* _Nullable)copyPeerPeerInfo:(NSError *__autoreleasing*)error
API_DEPRECATED("No longer needed", macos(10.15, 14.0), ios(13.0, 17.0), watchos(6.0, 10.0), tvos(13.0,17.0));

- (BOOL)peersHaveViewsEnabled:(NSArray<NSString*>*)viewNames error:(NSError *__autoreleasing*)error
API_DEPRECATED("No longer needed", macos(10.15, 14.0), ios(13.0, 17.0), watchos(6.0, 10.0), tvos(13.0,17.0));

- (BOOL)requestToJoinCircle:(NSError *__autoreleasing*)error
API_DEPRECATED("No longer needed", macos(10.15, 14.0), ios(13.0, 17.0), watchos(6.0, 10.0), tvos(13.0,17.0));

- (BOOL)accountUserKeyAvailable
API_DEPRECATED("No longer needed", macos(10.15, 14.0), ios(13.0, 17.0), watchos(6.0, 10.0), tvos(13.0,17.0));


/*
 * @abstract Ask for the list of best bottle IDs to restore for this account
 *    Ideally, we will replace this with a findOptimalEscrowRecordIDsWithContextData, but we're gated on
 *      Cuttlefish being able to read EscrowProxy (to get real escrow record IDs):
 *      <rdar://problem/44618259> [CUTTLEFISH] Cuttlefish needs to call Escrow Proxy to validate unmigrated accounts
 * @param data The OTClique configuration data
 * @param error Reports any error along the process
 * @return A pair of lists of escrow record IDs
 */
+ (OTBottleIDs* _Nullable)findOptimalBottleIDsWithContextData:(OTConfigurationContext*)data
                                                        error:(NSError**)error;

// This call is a noop.
+ (instancetype _Nullable)recoverWithContextData:(OTConfigurationContext*)data
                                        bottleID:(NSString*)bottleID
                                 escrowedEntropy:(NSData*)entropy
                                           error:(NSError**)error __deprecated_msg("recoverWithContextData:bottleID:escrowedEntropy:error: deprecated, use performEscrowRecoveryWithContextData:escrowArguments:error");

// used by sbd to fill in the escrow record
// You must have the entitlement "com.apple.private.octagon.escrow-content" to use this
// Also known as kSecEntitlementPrivateOctagonEscrow
- (void)fetchEscrowContents:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply;

// used by sbd to enroll a recovery key in octagon
+ (void)setNewRecoveryKeyWithData:(OTConfigurationContext*)ctx
                      recoveryKey:(NSString*)recoveryKey
                            reply:(void(^)(SecRecoveryKey * _Nullable rk,
                                           NSError* _Nullable error))reply;


// Create a new custodian recovery key.
+ (void)createCustodianRecoveryKey:(OTConfigurationContext*)ctx
                              uuid:(NSUUID *_Nullable)uuid
                             reply:(void (^)(OTCustodianRecoveryKey *_Nullable crk, NSError *_Nullable error))reply;

// Recover using a custodian recovery key.
+ (void)recoverOctagonUsingCustodianRecoveryKey:(OTConfigurationContext*)ctx
                           custodianRecoveryKey:(OTCustodianRecoveryKey *)crk
                                          reply:(void(^)(NSError* _Nullable error)) reply;

// Preflight (dry-run) recover using a custodian recovery key.
+ (void)preflightRecoverOctagonUsingCustodianRecoveryKey:(OTConfigurationContext*)ctx
                                    custodianRecoveryKey:(OTCustodianRecoveryKey *)crk
                                                   reply:(void(^)(NSError* _Nullable error)) reply;

// Remove a custodian recovery key.
+ (void)removeCustodianRecoveryKey:(OTConfigurationContext*)ctx
          custodianRecoveryKeyUUID:(NSUUID *)uuid
                             reply:(void (^)(NSError *_Nullable error))reply;

// Check for the existence of a custodian recovery key.
+ (void)checkCustodianRecoveryKey:(OTConfigurationContext*)ctx
         custodianRecoveryKeyUUID:(NSUUID *)uuid
                            reply:(void (^)(bool exists, NSError *_Nullable error))reply;

// Create a new inheritance key.
+ (void)createInheritanceKey:(OTConfigurationContext*)ctx
                        uuid:(NSUUID *_Nullable)uuid
                       reply:(void (^)(OTInheritanceKey *_Nullable ik, NSError *_Nullable error)) reply;

// Generate a new inheritance key (populate structure only, but nothing stored in Octagon).
+ (void)generateInheritanceKey:(OTConfigurationContext*)ctx
                          uuid:(NSUUID *_Nullable)uuid
                         reply:(void (^)(OTInheritanceKey *_Nullable ik, NSError *_Nullable error)) reply;

// Store inheritance key in Octagon.
+ (void)storeInheritanceKey:(OTConfigurationContext*)ctx
                         ik:(OTInheritanceKey *)ik
                      reply:(void (^)(NSError *_Nullable error)) reply;

// Join using an inheritance key.
+ (void)recoverOctagonUsingInheritanceKey:(OTConfigurationContext*)ctx
                           inheritanceKey:(OTInheritanceKey *)ik
                                    reply:(void(^)(NSError* _Nullable error)) reply;

// Preflight (dry-run) join using an inheritance key.
+ (void)preflightRecoverOctagonUsingInheritanceKey:(OTConfigurationContext*)ctx
                                    inheritanceKey:(OTInheritanceKey *)ik
                                             reply:(void(^)(NSError* _Nullable error)) reply;

// Remove an inheritance key.
+ (void)removeInheritanceKey:(OTConfigurationContext*)ctx
          inheritanceKeyUUID:(NSUUID *)uuid
                       reply:(void (^)(NSError *_Nullable error))reply;

// Check for the existence of an inheritance key.
+ (void)checkInheritanceKey:(OTConfigurationContext*)ctx
         inheritanceKeyUUID:(NSUUID *)uuid
                      reply:(void (^)(bool exists, NSError *_Nullable error))reply;

// Recreate a new inheritance key (with the same keys as an existing IK)
+ (void)recreateInheritanceKey:(OTConfigurationContext*)ctx
                          uuid:(NSUUID *_Nullable)uuid
                         oldIK:(OTInheritanceKey*)oldIK
                         reply:(void (^)(OTInheritanceKey *_Nullable ik, NSError *_Nullable error)) reply;

// Create a new inheritance key with a given claim token/wrappingkey
+ (void)createInheritanceKey:(OTConfigurationContext*)ctx
                        uuid:(NSUUID *_Nullable)uuid
              claimTokenData:(NSData *)claimTokenData
             wrappingKeyData:(NSData *)wrappingKeyData
                       reply:(void (^)(OTInheritanceKey *_Nullable ik, NSError *_Nullable error)) reply;

// CoreCDP will call this function when they failed to complete a successful CDP state machine run.
// Errors provided may be propagated from layers beneath CoreCDP, or contain the CoreCDP cause of failure.
- (void)performedFailureCDPStateMachineRun:(OTCliqueCDPContextType)type
                                     error:(NSError * _Nullable)error
                                     reply:(void(^)(NSError* _Nullable error))reply;

// CoreCDP will call this function when they complete a successful CDP state machine run.
- (void)performedSuccessfulCDPStateMachineRun:(OTCliqueCDPContextType)type
                                        reply:(void(^)(NSError* _Nullable error))reply;

// CoreCDP will call this function when they are upgrading an account from SA to HSA2
- (BOOL)waitForOctagonUpgrade:(NSError** _Nullable)error;


/*
* @abstract CoreCDP to call this function when they need to reset protected data.
*   This routine resets all circles, creates a new octagon and sos circle, then puts this device into each circle.
*   This routine does not create a new escrow record
*   This routine will need ensure OTConfigurationContext contains appleID and passwordEquivalentToken to delete all CDP records
* @param data The OTClique configuration data
* @param error Reports any error along the process
* @return a new clique
 */
+ (OTClique* _Nullable)resetProtectedData:(OTConfigurationContext*)data
                        idmsTargetContext:(NSString *_Nullable)idmsTargetContext
                   idmsCuttlefishPassword:(NSString *_Nullable)idmsCuttlefishPassword
                               notifyIdMS:(bool)notifyIdMS
                                    error:(NSError**)error;

+ (OTClique* _Nullable)resetProtectedData:(OTConfigurationContext*)data
                                    error:(NSError**)error;

/*
* @abstract CoreCDP to call this function when they need to clear the CDP account data without re-establishing a clique.
*   This routine resets both Octagon and SOS.
*   This routine will delete all encrypted content in CloudKit.
*   This routine does NOT create a new octagon and sos circle and does not re-join these systems.
*   This routine does NOT create a new escrow record
*   This routine will need ensure OTConfigurationContext contains appleID and passwordEquivalentToken to delete all CDP records
* @param data The OTClique configuration data
* @param error Reports any error along the process
* @return a BOOL of whehther or not it was successful
 */

+ (BOOL)clearCliqueFromAccount:(OTConfigurationContext*)data
                         error:(NSError**)error;

- (NSString* _Nullable)cliqueMemberIdentifier:(NSError* __autoreleasing * _Nullable)error;

/*
* @abstract CoreCDP to call this function when they only need to perform a ckserver reset.  This means Octagon data will remain
            untouched and only unreadable data will be removed from the server.
*   This routine WILL NOT reset Octagon and SOS.
*   This routine will delete all encrypted content in CloudKit.
*   This routine does NOT create a new octagon and sos circle and does not re-join these systems.
*   This routine does NOT create a new escrow record
* @param data The OTClique configuration data
* @param error Reports any error along the process
* @return a BOOL of whehther or not it was successful
 */
+ (BOOL)performCKServerUnreadableDataRemoval:(OTConfigurationContext*)data
                                       error:(NSError**)error;

@end



NS_ASSUME_NONNULL_END

#endif /* OBJC2 */
#endif /* OctagonTrust_h */
