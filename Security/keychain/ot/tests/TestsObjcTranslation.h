
#ifndef TestsObjcTranslation_h
#define TestsObjcTranslation_h

#import <Foundation/Foundation.h>
#import <AppleFeatures/AppleFeatures.h>
#import <TrustedPeers/TrustedPeers.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>

#import <AppleAccount/AppleAccount.h>
#import <AppleAccount/AppleAccount_Private.h>
#import <AppleAccount/ACAccount+AppleAccount.h>

#include "keychain/ckks/CKKSPeer.h"
#import "keychain/ot/OTClique.h"
#import "keychain/ot/OTAccountsAdapter.h"

@class OTSecureElementPeerIdentity;
@class OTCurrentSecureElementIdentities;
@class OTAccountSettings;
@class OTWalrus;
@class OTWebAccess;

NS_ASSUME_NONNULL_BEGIN

// This file is for translation of C/Obj-C APIs into swift-friendly things

@interface TestsObjectiveC : NSObject
+ (void)setNewRecoveryKeyWithData:(OTConfigurationContext*)ctx
                      recoveryKey:(NSString*)recoveryKey
                            reply:(void(^)(void* rk,
                                           NSError* _Nullable error))reply;

+ (BOOL)saveCoruptDataToKeychainForContainer:(NSString*)containerName
                                   contextID:(NSString*)contextID
                                       error:(NSError**)error;

+ (NSData* _Nullable)copyInitialSyncData:(SOSInitialSyncFlags)flags error:(NSError**)error;

+ (NSDictionary* _Nullable)copyPiggybackingInitialSyncData:(NSData*)data;

+ (BOOL)testSecKey:(CKKSSelves*)octagonSelf error:(NSError**)error;

+ (BOOL)addNRandomKeychainItemsWithoutUpgradedPersistentRefs:(int64_t)number;
+ (BOOL)checkAllPersistentRefBeenUpgraded;
+ (BOOL)expectXNumberOfItemsUpgraded:(int)expected;
+ (NSNumber* _Nullable)lastRowID;
+ (void)setError:(int)errorCode;
+ (void)clearError;
+ (void)clearLastRowID;
+ (void)clearErrorInsertionDictionary;
+ (void)setErrorAtRowID:(int)errorCode;
+ (void)setACAccountStoreWithInvalidationError:(id<OTAccountsAdapter>)adapter;
+ (void)setACAccountStoreWithRandomError:(id<OTAccountsAdapter>)adapter;
+ (int)getInvocationCount;
+ (void)clearInvocationCount;
+ (BOOL)isPlatformHomepod;
@end

// The swift-based OctagonTests can't call OctagonTrust methods. Bridge them!
@interface OctagonTrustCliqueBridge : NSObject

- (instancetype)initWithClique:(OTClique*)clique;

- (BOOL)setLocalSecureElementIdentity:(OTSecureElementPeerIdentity*)secureElementIdentity
                                error:(NSError**)error;

- (BOOL)removeLocalSecureElementIdentityPeerID:(NSData*)sePeerID
                                         error:(NSError**)error;

- (OTCurrentSecureElementIdentities* _Nullable)fetchTrustedSecureElementIdentities:(NSError**)error;

- (BOOL)setAccountSetting:(OTAccountSettings*)setting error:(NSError**)error;

- (OTAccountSettings* _Nullable)fetchAccountSettings:(NSError**)error;

+ (OTAccountSettings* _Nullable)fetchAccountWideSettings:(OTConfigurationContext*)context error:(NSError**)error;

+ (OTAccountSettings* _Nullable)fetchAccountWideSettingsWithForceFetch:(bool)forceFetch
                                                         configuration:(OTConfigurationContext*)configurationContext
                                                                 error:(NSError**)error;

- (BOOL)waitForPriorityViewKeychainDataRecovery:(NSError**)error;

- (NSString*)createAndSetRecoveryKeyWithContext:(OTConfigurationContext*)context error:(NSError**)error;

+ (BOOL)isRecoveryKeySet:(OTConfigurationContext*)ctx error:(NSError**)error;

+ (BOOL)recoverWithRecoveryKey:(OTConfigurationContext*)ctx
                   recoveryKey:(NSString*)recoveryKey
                         error:(NSError**)error;

- (BOOL)removeRecoveryKeyWithContext:(OTConfigurationContext*)context
                               error:(NSError**)error;

- (bool)preflightRecoveryKey:(OTConfigurationContext*)context recoveryKey:(NSString*)recoveryKey error:(NSError**)error;
@end

NS_ASSUME_NONNULL_END

#endif /* TestsObjcTranslation_h */
