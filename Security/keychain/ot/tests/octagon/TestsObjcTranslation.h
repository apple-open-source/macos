
#ifndef TestsObjcTranslation_h
#define TestsObjcTranslation_h

#import <Foundation/Foundation.h>
#import <Security/OTClique.h>
#import <TrustedPeers/TrustedPeers.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/ckks/CKKSPeer.h"

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
@end

NS_ASSUME_NONNULL_END

#endif /* TestsObjcTranslation_h */
