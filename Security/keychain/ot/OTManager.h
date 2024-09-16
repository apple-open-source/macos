/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#if OCTAGON
#import "Analytics/SFAnalytics.h"
#import "keychain/ot/OTManager.h"
#import "keychain/ot/OTRamping.h"
#import "keychain/ot/OTFollowup.h"
#import <Security/OTControlProtocol.h>
#import "keychain/ot/OTSOSAdapter.h"
#import "keychain/ot/OTAccountsAdapter.h"
#import "keychain/ot/OTAuthKitAdapter.h"
#import "keychain/ot/OTPersonaAdapter.h"
#import "keychain/ot/OTTapToRadarAdapter.h"
#import "keychain/ot/OTTooManyPeersAdapter.h"
#import "keychain/ot/OTDeviceInformationAdapter.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import <Security/SecEscrowRequest.h>
#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CKKSViewManager.h"
#include "keychain/securityd/SecDbItem.h"
#import <CoreCDP/CDPAccount.h>
NS_ASSUME_NONNULL_BEGIN

@class OTContext;
@class OTCuttlefishContext;
@class CKKSLockStateTracker;
@class CKKSAccountStateTracker;
@class CloudKitClassDependencies;

@interface OTManager : NSObject <OTControlProtocol>

@property (nonatomic, readonly) CKKSLockStateTracker* lockStateTracker;
@property CKKSAccountStateTracker* accountStateTracker;
@property CKKSReachabilityTracker* reachabilityTracker;

@property (readonly) CKContainer* cloudKitContainer;
@property (nullable) CKKSViewManager* viewManager;

// Creates an OTManager ready for use with live external systems.
- (instancetype)init;

- (instancetype)initWithSOSAdapter:(id<OTSOSAdapter>)sosAdapter
                   accountsAdapter:(id<OTAccountsAdapter>)accountsAdapter
                    authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
               tooManyPeersAdapter:(id<OTTooManyPeersAdapter>)tooManyPeersAdapter
                 tapToRadarAdapter:(id<OTTapToRadarAdapter>)tapToRadarAdapter
          deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter
                    personaAdapter:(id<OTPersonaAdapter>)personaAdapter
                apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
                escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
                     notifierClass:(Class<CKKSNotifier>)notifierClass
                       loggerClass:(Class<SFAnalyticsProtocol>)loggerClass
                  lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
               reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
         cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
           cuttlefishXPCConnection:(id<NSXPCProxyCreating> _Nullable)cuttlefishXPCConnection
                              cdpd:(id<OctagonFollowUpControllerProtocol>)cdpd;

// Call this to start up the state machinery
- (void)initializeOctagon;
- (BOOL)waitForReady:(OTControlArguments*)arguments
                wait:(int64_t)wait;

// Call this to ensure SFA is ready
- (void)setupAnalytics;

+ (instancetype _Nullable)manager;
+ (instancetype _Nullable)resetManager:(bool)reset to:(OTManager* _Nullable)obj;
- (void)xpc24HrNotification;

- (OTCuttlefishContext*)contextForContainerName:(NSString* _Nullable)containerName
                                      contextID:(NSString*)contextID
                                     sosAdapter:(id<OTSOSAdapter>)sosAdapter
                                accountsAdapter:(id<OTAccountsAdapter>)accountsAdapter
                                 authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                            tooManyPeersAdapter:(id<OTTooManyPeersAdapter>)tooManyPeersAdapter
                              tapToRadarAdapter:(id<OTTapToRadarAdapter>)tapToRadarAdapter
                               lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                       deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter;

- (OTCuttlefishContext*)contextForContainerName:(NSString* _Nullable)containerName
                                      contextID:(NSString*)contextID;

- (OTCuttlefishContext* _Nullable)contextForClientRPC:(OTControlArguments*)arguments
                                      createIfMissing:(BOOL)createIfMissing
                              allowNonPrimaryAccounts:(BOOL)allowNonPrimaryAccounts
                                                error:(NSError**)error;

- (CKKSKeychainView* _Nullable)ckksForClientRPC:(OTControlArguments*)arguments
                                createIfMissing:(BOOL)createIfMissing
                        allowNonPrimaryAccounts:(BOOL)allowNonPrimaryAccounts
                                          error:(NSError**)error;

- (void)removeContextForContainerName:(NSString*)containerName
                            contextID:(NSString*)contextID;

-(BOOL)ghostbustByMidEnabled;
-(BOOL)ghostbustBySerialEnabled;
-(BOOL)ghostbustByAgeEnabled;

- (void)restoreFromBottle:(OTControlArguments*)arguments
                  entropy:(NSData *)entropy
                 bottleID:(NSString *)bottleID
                    reply:(void (^)(NSError * _Nullable))reply;

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
                               reply:(void (^)(NSError *_Nullable))reply;

- (void)preflightJoinWithCustodianRecoveryKey:(OTControlArguments*)arguments
                         custodianRecoveryKey:(OTCustodianRecoveryKey *)crk
                                        reply:(void (^)(NSError *_Nullable))reply;

- (void)removeCustodianRecoveryKey:(OTControlArguments*)arguments
                              uuid:(NSUUID *)uuid
                             reply:(void (^)(NSError *_Nullable error))reply;

- (void)checkCustodianRecoveryKey:(OTControlArguments*)arguments
                             uuid:(NSUUID *)uuid
                            reply:(void (^)(bool exists, NSError *_Nullable error))reply;

- (void)createInheritanceKey:(OTControlArguments*)arguments
                        uuid:(NSUUID *_Nullable)uuid
                       reply:(void (^)(OTInheritanceKey *_Nullable crk, NSError *_Nullable error))reply;

- (void)generateInheritanceKey:(OTControlArguments*)arguments
                        uuid:(NSUUID *_Nullable)uuid
                       reply:(void (^)(OTInheritanceKey *_Nullable crk, NSError *_Nullable error))reply;

- (void)storeInheritanceKey:(OTControlArguments*)arguments
                         ik:(OTInheritanceKey *)ik
                      reply:(void (^)(NSError *_Nullable error)) reply;

- (void)joinWithInheritanceKey:(OTControlArguments*)arguments
                inheritanceKey:(OTInheritanceKey *)ik
                         reply:(void (^)(NSError *_Nullable))reply;

- (void)preflightJoinWithInheritanceKey:(OTControlArguments*)arguments
                         inheritanceKey:(OTInheritanceKey *)ik
                                  reply:(void (^)(NSError *_Nullable))reply;

- (void)removeInheritanceKey:(OTControlArguments*)arguments
                        uuid:(NSUUID *)uuid
                       reply:(void (^)(NSError *_Nullable error))reply;

- (void)checkInheritanceKey:(OTControlArguments*)arguments
                       uuid:(NSUUID *)uuid
                      reply:(void (^)(bool exists, NSError *_Nullable error))reply;

- (void)recreateInheritanceKey:(OTControlArguments*)arguments
                          uuid:(NSUUID *_Nullable)uuid
                         oldIK:(OTInheritanceKey *)oldIK
                        reply:(void (^)(OTInheritanceKey *_Nullable crk, NSError *_Nullable error))reply;

- (void)tlkRecoverabilityForEscrowRecordData:(OTControlArguments*)arguments
                                  recordData:(NSData*)recordData
                                      source:(OTEscrowRecordFetchSource)source
                                       reply:(void (^)(NSArray<NSString*>* _Nullable views, NSError* _Nullable error))reply;

- (void)setMachineIDOverride:(OTControlArguments*)arguments
                   machineID:(NSString* _Nullable)machineID
                       reply:(void (^)(NSError* _Nullable replyError))reply;

- (void)getAccountMetadata:(OTControlArguments*)arguments
                     reply:(void (^)(OTAccountMetadataClassC* metadata, NSError* _Nullable replyError))reply;

- (CKKSKeychainView* _Nullable)ckksAccountSyncForContainer:(NSString*_Nullable)containerName
                                                 contextID:(NSString*)contextID
                                           possibleAccount:(TPSpecificUser* _Nullable)possibleAccount;

- (OTCuttlefishContext* _Nullable)restartOctagonContext:(OTCuttlefishContext*)cuttlefishContext;
- (CKKSKeychainView* _Nullable)restartCKKSAccountSyncWithoutSettingPolicy:(CKKSKeychainView*)view;

- (void)haltAll;
- (void)dropAllActors;

- (void)allContextsHalt;
- (void)allContextsDisablePendingFlags;
- (bool)allContextsPause:(uint64_t)within;

- (void)waitForOctagonUpgrade:(OTControlArguments*)arguments
                        reply:(void (^)(NSError* _Nullable error))reply;

- (BOOL)fetchSendingMetricsPermitted:(OTControlArguments*)arguments error:(NSError**)error;
- (BOOL)persistSendingMetricsPermitted:(OTControlArguments*)arguments sendingMetricsPermitted:(BOOL)sendingMetricsPermitted error:(NSError**)error;

// Metrics and analytics
- (void)postCDPFollowupResult:(OTControlArguments*)arguments
                      success:(BOOL)success
                         type:(OTCliqueCDPContextType)type
                        error:(NSError * _Nullable)error
                        reply:(void (^)(NSError *error))reply;

- (void)fetchAccountSettings:(OTControlArguments*)arguments
                       reply:(void (^)(OTAccountSettings* _Nullable setting, NSError * _Nullable error))reply;

// Helper function to make CK containers
+ (CKContainer*)makeCKContainer:(NSString*)containerName;

@end

@interface OTManager (Testing)
- (void)setSOSEnabledForPlatformFlag:(bool) value;

- (void)clearAllContexts;

// Note that the OTManager returned by this will not work particularly well, if you want to do Octagon things
// This should only be used for the CKKS tests
- (instancetype)initWithSOSAdapter:(id<OTSOSAdapter>)sosAdapter
                  lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                    personaAdapter:(id<OTPersonaAdapter>)personaAdapter
         cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies;

- (void)invalidateEscrowCache:(OTControlArguments*)arguments
                        reply:(nonnull void (^)(NSError * _Nullable error))reply;
@end

NS_ASSUME_NONNULL_END

#endif  // OCTAGON

