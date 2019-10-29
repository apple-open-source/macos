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
#import "keychain/ot/OTControlProtocol.h"
#import "keychain/ot/OTSOSAdapter.h"
#import "keychain/ot/OTAuthKitAdapter.h"
#import "keychain/ot/OTDeviceInformationAdapter.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/escrowrequest/Framework/SecEscrowRequest.h"
#import "keychain/ckks/CKKSAccountStateTracker.h"
#include "keychain/securityd/SecDbItem.h"
#import <CoreCDP/CDPAccount.h>
NS_ASSUME_NONNULL_BEGIN

@class OTContext;
@class OTCuttlefishContext;
@class OTClientStateMachine;
@class CKKSLockStateTracker;
@class CKKSAccountStateTracker;

@interface OTManager : NSObject <OTControlProtocol>

@property (nonatomic, readonly) CKKSLockStateTracker* lockStateTracker;
@property id<CKKSCloudKitAccountStateTrackingProvider> accountStateTracker;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithSOSAdapter:(id<OTSOSAdapter>)sosAdapter
                   authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
          deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter
                apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
                escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
                       loggerClass:(Class<SFAnalyticsProtocol> _Nullable)loggerClass
                  lockStateTracker:(CKKSLockStateTracker* _Nullable)lockStateTracker
               accountStateTracker:(id<CKKSCloudKitAccountStateTrackingProvider>)accountStateTracker
           cuttlefishXPCConnection:(id<NSXPCProxyCreating> _Nullable)cuttlefishXPCConnection
                              cdpd:(id<OctagonFollowUpControllerProtocol>)cdpd;

// Call this to start up the state machinery
- (void)initializeOctagon;
- (BOOL)waitForReady:(NSString* _Nullable)containerName context:(NSString*)context wait:(int64_t)wait;
- (void)moveToCheckTrustedStateForContainer:(NSString* _Nullable)containerName context:(NSString*)context;

+ (instancetype _Nullable)manager;
+ (instancetype _Nullable)resetManager:(bool)reset to:(OTManager* _Nullable)obj;
- (void)xpc24HrNotification:(NSString* _Nullable)containerName context:(NSString*)context skipRateLimitingCheck:(BOOL)skipRateLimitingCheck reply:(void (^)(NSError *error))reply;

- (OTCuttlefishContext*)contextForContainerName:(NSString* _Nullable)containerName
                                      contextID:(NSString*)contextID
                                     sosAdapter:(id<OTSOSAdapter>)sosAdapter
                                 authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                               lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                            accountStateTracker:(id<CKKSCloudKitAccountStateTrackingProvider>)accountStateTracker
                       deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter;

- (OTCuttlefishContext*)contextForContainerName:(NSString* _Nullable)containerName
                                      contextID:(NSString*)contextID;

- (void)removeContextForContainerName:(NSString*)containerName
                            contextID:(NSString*)contextID;

- (OTClientStateMachine*)clientStateMachineForContainerName:(NSString* _Nullable)containerName
                                                  contextID:(NSString*)contextID
                                                 clientName:(NSString*)clientName;

-(BOOL)ghostbustByMidEnabled;
-(BOOL)ghostbustBySerialEnabled;
-(BOOL)ghostbustByAgeEnabled;

-(void)restore:(NSString* _Nullable)containerName
     contextID:(NSString *)contextID
    bottleSalt:(NSString *)bottleSalt
       entropy:(NSData *)entropy
      bottleID:(NSString *)bottleID
         reply:(void (^)(NSError * _Nullable))reply;

- (void)createRecoveryKey:(NSString* _Nullable)containerName
                contextID:(NSString *)contextID
              recoveryKey:(NSString *)recoveryKey
                    reply:(void (^)( NSError * _Nullable))reply;

- (void)joinWithRecoveryKey:(NSString* _Nullable)containerName
                  contextID:(NSString *)contextID
                recoveryKey:(NSString*)recoveryKey
                      reply:(void (^)(NSError * _Nullable))reply;

- (void)allContextsHalt;
- (void)allContextsDisablePendingFlags;
- (bool)allContextsPause:(uint64_t)within;

- (void)waitForOctagonUpgrade:(NSString* _Nullable)container
                      context:(NSString*)context
                        reply:(void (^)(NSError* _Nullable error))reply;

// Metrics and analytics
- (void)postCDPFollowupResult:(BOOL)success
                         type:(OTCliqueCDPContextType)type
                        error:(NSError * _Nullable)error
                containerName:(NSString* _Nullable)containerName
                  contextName:(NSString *)contextName
                        reply:(void (^)(NSError *error))reply;

//test only
- (void)setSOSEnabledForPlatformFlag:(bool) value;
@end
NS_ASSUME_NONNULL_END

#endif  // OCTAGON

