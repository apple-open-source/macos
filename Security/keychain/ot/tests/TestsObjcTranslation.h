
#ifndef TestsObjcTranslation_h
#define TestsObjcTranslation_h

#import <Foundation/Foundation.h>
#import <AppleFeatures/AppleFeatures.h>
#import "keychain/ot/OTClique.h"
#import <TrustedPeers/TrustedPeers.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/ckks/CKKSPeer.h"

@class OTSecureElementPeerIdentity;
@class OTCurrentSecureElementIdentities;

NS_ASSUME_NONNULL_BEGIN

// This file is for translation of C/Obj-C APIs into swift-friendly things

@interface TestsObjectiveC : NSObject
+ (void)setNewRecoveryKeyWithData:(OTConfigurationContext *)ctx
                      recoveryKey:(NSString*)recoveryKey
                            reply:(void(^)(void* rk,
                                           NSError* _Nullable error))reply;

+ (void)recoverOctagonUsingData:(OTConfigurationContext *)ctx
                    recoveryKey:(NSString*)recoveryKey
                          reply:(void(^)(NSError* _Nullable error))reply;

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
@end

// The swift-based OctagonTests can't call OctagonTrust methods. Bridge them!
@interface OctagonTrustCliqueBridge : NSObject

- (instancetype)initWithClique:(OTClique*)clique;

- (BOOL)setLocalSecureElementIdentity:(OTSecureElementPeerIdentity*)secureElementIdentity
                                error:(NSError**)error;

- (BOOL)removeLocalSecureElementIdentityPeerID:(NSData*)sePeerID
                                         error:(NSError**)error;

- (OTCurrentSecureElementIdentities* _Nullable)fetchTrustedSecureElementIdentities:(NSError**)error;


- (BOOL)waitForPriorityViewKeychainDataRecovery:(NSError**)error;
@end

NS_ASSUME_NONNULL_END

#endif /* TestsObjcTranslation_h */
