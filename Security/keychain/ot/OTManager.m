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


#include <Security/SecEntitlements.h>
#import <Foundation/NSXPCConnection.h>
#import <Foundation/NSXPCConnection_Private.h>

#if OCTAGON
#import <TargetConditionals.h>
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTControlProtocol.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTClique.h"
#import "keychain/ot/OTManager.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTRamping.h"
#import "keychain/ot/OT.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ot/OTJoiningConfiguration.h"
#import "keychain/ot/OTSOSAdapter.h"

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSCloudKitClassDependencies.h"

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>
#import "SecPasswordGenerate.h"

#import "keychain/categories/NSError+UsefulConstructors.h"
#include <CloudKit/CloudKit_Private.h>
#import <KeychainCircle/PairingChannel.h>

#import "keychain/escrowrequest/Framework/SecEscrowRequest.h"
#import "keychain/escrowrequest/EscrowRequestServer.h"

// If your callbacks might pass back a CK error, you should use XPCSanitizeError()
// Otherwise, XPC might crash on the other side if they haven't linked CloudKit.framework.
#define XPCSanitizeError CKXPCSuitableError

#import <Accounts/Accounts.h>
#import <Accounts/ACAccountStore_Private.h>
#import <Accounts/ACAccountType_Private.h>
#import <Accounts/ACAccountStore.h>
#import <AppleAccount/ACAccountStore+AppleAccount.h>
#import <AppleAccount/ACAccount+AppleAccount.h>

#import <CoreCDP/CDPFollowUpController.h>

#import "keychain/TrustedPeersHelper/TPHObjcTranslation.h"
#import "keychain/SecureObjectSync/SOSAccountTransaction.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#import "keychain/SecureObjectSync/SOSAccount.h"
#pragma clang diagnostic pop

#import "utilities/SecTapToRadar.h"

static NSString* const kOTRampForEnrollmentRecordName = @"metadata_rampstate_enroll";
static NSString* const kOTRampForRestoreRecordName = @"metadata_rampstate_restore";
static NSString* const kOTRampForCFURecordName = @"metadata_rampstate_cfu";
static NSString* const kOTRampForGhostBustMIDName = @"metadata_rampstate_ghostBustMID";
static NSString* const kOTRampForghostBustSerialName = @"metadata_rampstate_ghostBustSerial";
static NSString* const kOTRampForghostBustAgeName = @"metadata_rampstate_ghostBustAge";
static NSString* const kOTRampZoneName = @"metadata_zone";
#define NUM_NSECS_IN_24_HRS (86400 * NSEC_PER_SEC)

#if OCTAGON
@interface OTManager (lockstateTracker) <CKKSLockStateNotification>
@end
#endif

@interface OTManager () <NSXPCListenerDelegate>
@property NSXPCListener *listener;

@property (nonatomic, strong) OTRamp *gbmidRamp;
@property (nonatomic, strong) OTRamp *gbserialRamp;
@property (nonatomic, strong) OTRamp *gbAgeRamp;
@property (nonatomic, strong) CKKSLockStateTracker *lockStateTracker;
@property (nonatomic, strong) id<OctagonFollowUpControllerProtocol> cdpd;

// Current contexts
@property NSMutableDictionary<NSString*, OTCuttlefishContext*>* contexts;
@property NSMutableDictionary<NSString*, OTClientStateMachine*>* clients;
@property dispatch_queue_t queue;

@property id<NSXPCProxyCreating> cuttlefishXPCConnection;

// Dependencies for injection
@property (readonly) id<OTSOSAdapter> sosAdapter;
@property (readonly) id<OTAuthKitAdapter> authKitAdapter;
@property (readonly) id<OTDeviceInformationAdapter> deviceInformationAdapter;
@property (readonly) Class<OctagonAPSConnection> apsConnectionClass;
@property (readonly) Class<SecEscrowRequestable> escrowRequestClass;
// If this is nil, all logging is disabled
@property (readonly, nullable) Class<SFAnalyticsProtocol> loggerClass;
@property (nonatomic) BOOL sosEnabledForPlatform;
@end

@implementation OTManager
@synthesize cuttlefishXPCConnection = _cuttlefishXPCConnection;
@synthesize sosAdapter = _sosAdapter;
@synthesize authKitAdapter = _authKitAdapter;
@synthesize deviceInformationAdapter = _deviceInformationAdapter;

- (instancetype)init
{
    // Under Octagon, the sos adapter is not considered essential.
    id<OTSOSAdapter> sosAdapter = (OctagonPlatformSupportsSOS() ?
                                   [[OTSOSActualAdapter alloc] initAsEssential:NO] :
                                   [[OTSOSMissingAdapter alloc] init]);

    return [self initWithSOSAdapter:sosAdapter
                     authKitAdapter:[[OTAuthKitActualAdapter alloc] init]
           deviceInformationAdapter:[[OTDeviceInformationActualAdapter alloc] init]
                 apsConnectionClass:[APSConnection class]
                 escrowRequestClass:[EscrowRequestServer class] // Use the server class here to skip the XPC layer
                        loggerClass:[CKKSAnalytics class]
                   lockStateTracker:[CKKSLockStateTracker globalTracker]
          cloudKitClassDependencies:[CKKSCloudKitClassDependencies forLiveCloudKit]
            cuttlefishXPCConnection:nil
                               cdpd:[[CDPFollowUpController alloc] init]];
}

- (instancetype)initWithSOSAdapter:(id<OTSOSAdapter>)sosAdapter
                    authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
          deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter
                apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
                escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
                       loggerClass:(Class<SFAnalyticsProtocol>)loggerClass
                  lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
         cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
           cuttlefishXPCConnection:(id<NSXPCProxyCreating>)cuttlefishXPCConnection
                              cdpd:(id<OctagonFollowUpControllerProtocol>)cdpd
{
    if((self = [super init])) {
        _sosAdapter = sosAdapter;
        _authKitAdapter = authKitAdapter;
        _deviceInformationAdapter = deviceInformationAdapter;
        _loggerClass = loggerClass;
        _lockStateTracker = lockStateTracker;
        _sosEnabledForPlatform = OctagonPlatformSupportsSOS();
        _cuttlefishXPCConnection = cuttlefishXPCConnection;

        _cloudKitContainer = [CKKSViewManager makeCKContainer:SecCKKSContainerName usePCS:SecCKKSContainerUsePCS];
        _accountStateTracker = [[CKKSAccountStateTracker alloc] init:_cloudKitContainer
                                           nsnotificationCenterClass:cloudKitClassDependencies.nsnotificationCenterClass];

        self.contexts = [NSMutableDictionary dictionary];
        self.clients = [NSMutableDictionary dictionary];

        self.queue = dispatch_queue_create("otmanager", DISPATCH_QUEUE_SERIAL);

        _apsConnectionClass = apsConnectionClass;
        _escrowRequestClass = escrowRequestClass;

        _cdpd = cdpd;

        _viewManager = [[CKKSViewManager alloc] initWithContainer:_cloudKitContainer
                                                       sosAdapter:sosAdapter
                                              accountStateTracker:_accountStateTracker
                                                 lockStateTracker:lockStateTracker
                                        cloudKitClassDependencies:cloudKitClassDependencies];

        // The default CuttlefishContext always exists:
        (void) [self contextForContainerName:OTCKContainerName contextID:OTDefaultContext];
        
        secnotice("octagon", "otmanager init");
    }
    return self;
}

- (instancetype)initWithSOSAdapter:(id<OTSOSAdapter>)sosAdapter
                  lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
         cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
{
    if((self = [super init])) {
        _sosAdapter = sosAdapter;
        _lockStateTracker = lockStateTracker;

        _cloudKitContainer = [CKKSViewManager makeCKContainer:SecCKKSContainerName usePCS:SecCKKSContainerUsePCS];
        _accountStateTracker = [[CKKSAccountStateTracker alloc] init:_cloudKitContainer
                                           nsnotificationCenterClass:cloudKitClassDependencies.nsnotificationCenterClass];

        _viewManager = [[CKKSViewManager alloc] initWithContainer:_cloudKitContainer
                                                       sosAdapter:sosAdapter
                                              accountStateTracker:_accountStateTracker
                                                 lockStateTracker:lockStateTracker
                                        cloudKitClassDependencies:cloudKitClassDependencies];
    }
    return self;
}

- (void)initializeOctagon
{
    secnotice("octagon", "Initializing Octagon...");

    if(OctagonIsEnabled()) {
        secnotice("octagon", "starting default state machine...");
        OTCuttlefishContext* c = [self contextForContainerName:OTCKContainerName
                                                     contextID:OTDefaultContext];

        [c startOctagonStateMachine];
        [self registerForCircleChangedNotifications];
    }
}

- (BOOL)waitForReady:(NSString* _Nullable)containerName context:(NSString*)context wait:(int64_t)wait
{
    OTCuttlefishContext* c = [self contextForContainerName:containerName contextID:context];
    return [c waitForReady:wait];
}

- (void) moveToCheckTrustedStateForContainer:(NSString* _Nullable)containerName context:(NSString*)context
{
    OTCuttlefishContext* c = [self contextForContainerName:containerName
                                                     contextID:context];
    [c startOctagonStateMachine];
    [c moveToCheckTrustedState];
}

- (void)registerForCircleChangedNotifications
{
    __weak __typeof(self) weakSelf = self;

    // If we're not in the tests, go ahead and register for a notification
    if(!SecCKKSTestsEnabled()) {
        int token = 0;
        notify_register_dispatch(kSOSCCCircleChangedNotification, &token, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^(int t) {
            secnotice("octagon", "circle changed notification called, checking trust state");
            [weakSelf moveToCheckTrustedStateForContainer:OTCKContainerName context:OTDefaultContext];
        });
    }
}

+ (instancetype _Nullable)manager {
    if(!OctagonIsEnabled()) {
        secerror("octagon: Attempt to fetch a manager while Octagon is disabled");
        return nil;
    }

    if([CKDatabase class] == nil) {
        // CloudKit is not linked. We cannot bring Octagon up.
        secerror("Octagon: CloudKit.framework appears to not be linked. Cannot create an Octagon manager (on pain of crash).");
        return nil;
    }

    return [self resetManager:false to:nil];
}

+ (instancetype _Nullable)resetManager:(bool)reset to:(OTManager* _Nullable)obj
{
    static OTManager* manager = nil;

    if(!manager || reset || obj) {
        @synchronized([self class]) {
            if(obj != nil) {
                manager = obj;
            } else {
                if(reset) {
                    manager = nil;
                } else if (manager == nil && OctagonIsEnabled()) {
                    manager = [[OTManager alloc] init];
                }
            }
        }
    }

    return manager;
}

- (void)ensureRampsInitialized
{
    CKContainer* container = [CKKSViewManager manager].container;
    CKDatabase* database = [container privateCloudDatabase];
    CKRecordZoneID* zoneID = [[CKRecordZoneID alloc] initWithZoneName:kOTRampZoneName ownerName:CKCurrentUserDefaultName];

    CKKSAccountStateTracker *accountTracker = [CKKSViewManager manager].accountTracker;
    CKKSReachabilityTracker *reachabilityTracker = [CKKSViewManager manager].reachabilityTracker;
    CKKSLockStateTracker *lockStateTracker = [CKKSViewManager manager].lockStateTracker;

    if(!self.gbmidRamp) {
        self.gbmidRamp = [[OTRamp alloc]initWithRecordName:kOTRampForGhostBustMIDName
                                          localSettingName:@"ghostBustMID"
                                                 container:container
                                                  database:database
                                                    zoneID:zoneID
                                            accountTracker:accountTracker
                                          lockStateTracker:lockStateTracker
                                       reachabilityTracker:reachabilityTracker
                          fetchRecordRecordsOperationClass:[CKFetchRecordsOperation class]];
    }

    if(!self.gbserialRamp) {
        self.gbserialRamp = [[OTRamp alloc]initWithRecordName:kOTRampForghostBustSerialName
                                             localSettingName:@"ghostBustSerial"
                                                    container:container
                                                     database:database
                                                       zoneID:zoneID
                                               accountTracker:accountTracker
                                             lockStateTracker:lockStateTracker
                                          reachabilityTracker:reachabilityTracker
                             fetchRecordRecordsOperationClass:[CKFetchRecordsOperation class]];
    }

    if(!self.gbAgeRamp) {
        self.gbAgeRamp = [[OTRamp alloc]initWithRecordName:kOTRampForghostBustAgeName
                                          localSettingName:@"ghostBustAge"
                                                 container:container
                                                  database:database
                                                    zoneID:zoneID
                                            accountTracker:accountTracker
                                          lockStateTracker:lockStateTracker
                                       reachabilityTracker:reachabilityTracker
                          fetchRecordRecordsOperationClass:[CKFetchRecordsOperation class]];
    }
}

////
// MARK: SPI routines
////


- (void)signIn:(NSString*)altDSID
     container:(NSString* _Nullable)containerName
       context:(NSString*)contextID
         reply:(void (^)(NSError * _Nullable signedInError))reply
{
    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityAccountAvailable];

    if(containerName == nil) {
        containerName = OTCKContainerName;
    }

    NSError *error = nil;
    OTCuttlefishContext* context = [self contextForContainerName:containerName contextID:contextID];

    secnotice("octagon","signing in %@ for altDSID: %@", context, altDSID);
    [context accountAvailable:altDSID error:&error];

    [tracker stopWithEvent:OctagonEventSignIn result:error];

    reply(error);
}

- (void)signOut:(NSString* _Nullable)containerName
        context:(NSString*)contextID
          reply:(void (^)(NSError * _Nullable signedOutError))reply
{
    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityAccountNotAvailable];

    if(containerName == nil) {
        containerName = OTCKContainerName;
    }

    NSError* error = nil;

    // TODO: should we compare DSIDs here?
    OTCuttlefishContext* context = [self contextForContainerName:containerName contextID:contextID];

    secnotice("octagon", "signing out of octagon trust: %@", context);

    [context accountNoLongerAvailable:&error];
    if(error) {
        secnotice("octagon", "signing out failed: %@", error);
    }

    [tracker stopWithEvent:OctagonEventSignOut result:error];

    reply(error);
}

- (void)notifyIDMSTrustLevelChangeForContainer:(NSString* _Nullable)containerName
                                       context:(NSString*)contextID
                                         reply:(void (^)(NSError * _Nullable error))reply
{
    if(containerName == nil) {
        containerName = OTCKContainerName;
    }

    NSError *error = nil;
    OTCuttlefishContext* context = [self contextForContainerName:containerName contextID:contextID];
    secnotice("octagon","received a notification of IDMS trust level change in %@", context);
    [context idmsTrustLevelChanged:&error];

    reply(error);
}

- (void)handleIdentityChangeForSigningKey:(SFECKeyPair*)peerSigningKey
                         ForEncryptionKey:(SFECKeyPair*)encryptionKey
                                ForPeerID:(NSString*)peerID
                                    reply:(void (^)(BOOL result,
                                                    NSError* _Nullable error))reply
{
    secnotice("octagon", "handleIdentityChangeForSigningKey: %@", peerID);
    reply(NO,
          [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)preflightBottledPeer:(NSString*)contextID
                        dsid:(NSString*)dsid
                       reply:(void (^)(NSData* _Nullable entropy,
                                       NSString* _Nullable bottleID,
                                       NSData* _Nullable signingPublicKey,
                                       NSError* _Nullable error))reply
{
    secnotice("octagon", "preflightBottledPeer: %@ %@", contextID, dsid);
    reply(nil,
          nil,
          nil,
          [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)launchBottledPeer:(NSString*)contextID
                 bottleID:(NSString*)bottleID
                    reply:(void (^ _Nullable)(NSError* _Nullable error))reply
{
    secnotice("octagon", "launchBottledPeer");
    reply([NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)restore:(NSString *)contextID dsid:(NSString *)dsid secret:(NSData*)secret escrowRecordID:(NSString*)escrowRecordID reply:(void (^)(NSData* signingKeyData, NSData* encryptionKeyData, NSError *))reply
{
    secnotice("octagon", "restore");
    reply(nil,
          nil,
          [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)scrubBottledPeer:(NSString*)contextID
                bottleID:(NSString*)bottleID
                   reply:(void (^ _Nullable)(NSError* _Nullable error))reply
{
    reply([NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

////
// MARK: OTCTL tool routines
////

-(void)reset:(void (^)(BOOL result, NSError *))reply
{
    reply(NO,
          [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)listOfEligibleBottledPeerRecords:(void (^)(NSArray* listOfRecords, NSError * _Nullable))reply
{
    reply(@[],
          [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)octagonEncryptionPublicKey:(void (^)(NSData* encryptionKey, NSError *))reply
{
    reply(nil,
          [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

-(void)octagonSigningPublicKey:(void (^)(NSData* encryptionKey, NSError *))reply
{
    reply(nil,
          [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain
                              code:errSecUnimplemented
                          userInfo:nil]);
}

- (void)setCuttlefishXPCConnection:(id<NSXPCProxyCreating>)cuttlefishXPCConnection
{
    _cuttlefishXPCConnection = cuttlefishXPCConnection;
}

- (id<NSXPCProxyCreating>)cuttlefishXPCConnection
{
    if(!_cuttlefishXPCConnection) {
        NSXPCConnection* xpcConnection = [[NSXPCConnection alloc] initWithServiceName:@"com.apple.TrustedPeersHelper"];
        xpcConnection.remoteObjectInterface = TrustedPeersHelperSetupProtocol([NSXPCInterface interfaceWithProtocol:@protocol(TrustedPeersHelperProtocol)]);
        [xpcConnection resume];
        _cuttlefishXPCConnection = xpcConnection;
    }

    return _cuttlefishXPCConnection;
}

- (OTClientStateMachine*)clientStateMachineForContainerName:(NSString* _Nullable)containerName
                                                  contextID:(NSString*)contextID
                                                 clientName:(NSString*)clientName
{
    __block OTClientStateMachine* client = nil;

    if(containerName == nil) {
        containerName = SecCKKSContainerName;
    }

    dispatch_sync(self.queue, ^{
        NSString* key = [NSString stringWithFormat:@"%@-%@", containerName, clientName];
        secnotice("octagon-client", "fetching context for key: %@", key);
        client = self.clients[key];
        if(!client) {
            client = [[OTClientStateMachine alloc] initWithContainerName:containerName
                                                               contextID:contextID
                                                              clientName:clientName
                                                              cuttlefish:self.cuttlefishXPCConnection];

            self.clients[key] = client;
        }
    });

    return client;
}

- (void)removeClientContextForContainerName:(NSString* _Nullable)containerName
                                  clientName:(NSString*)clientName
{
    if(containerName == nil) {
        containerName = SecCKKSContainerName;
    }

    dispatch_sync(self.queue, ^{
        NSString* key = [NSString stringWithFormat:@"%@-%@", containerName, clientName];
        [self.clients removeObjectForKey:key];
        secnotice("octagon", "removed client context with key: %@", key);
    });
}

- (void)removeContextForContainerName:(NSString*)containerName
                            contextID:(NSString*)contextID
{
    dispatch_sync(self.queue, ^{
        NSString* key = [NSString stringWithFormat:@"%@-%@", containerName, contextID];
        self.contexts[key] = nil;
    });
}

- (OTCuttlefishContext*)contextForContainerName:(NSString* _Nullable)containerName
                                      contextID:(NSString*)contextID
{
    return [self contextForContainerName:containerName
                               contextID:contextID
                              sosAdapter:self.sosAdapter
                          authKitAdapter:self.authKitAdapter
                        lockStateTracker:self.lockStateTracker
                     accountStateTracker:self.accountStateTracker
                deviceInformationAdapter:self.deviceInformationAdapter];
}

- (OTCuttlefishContext*)contextForContainerName:(NSString* _Nullable)containerName
                                      contextID:(NSString*)contextID
                                     sosAdapter:(id<OTSOSAdapter>)sosAdapter
                                 authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                               lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                            accountStateTracker:(CKKSAccountStateTracker*)accountStateTracker
                       deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter
{
    __block OTCuttlefishContext* context = nil;

    if(containerName == nil) {
        containerName = SecCKKSContainerName;
    }

    dispatch_sync(self.queue, ^{
        NSString* key = [NSString stringWithFormat:@"%@-%@", containerName, contextID];

        context = self.contexts[key];

        // Right now, CKKS can only handle one session per address space (and SQL database).
        // Therefore, only the primary OTCuttlefishContext gets to own the view manager.
        CKKSViewManager* viewManager = nil;
        if([containerName isEqualToString:SecCKKSContainerName] &&
           [contextID isEqualToString:OTDefaultContext]) {
            viewManager = self.viewManager;
        }

        if(!context) {
            context = [[OTCuttlefishContext alloc] initWithContainerName:containerName
                                                               contextID:contextID
                                                              cuttlefish:self.cuttlefishXPCConnection
                                                              sosAdapter:sosAdapter
                                                          authKitAdapter:authKitAdapter
                                                         ckksViewManager:viewManager
                                                        lockStateTracker:lockStateTracker
                                                     accountStateTracker:accountStateTracker
                                                deviceInformationAdapter:deviceInformationAdapter
                                                      apsConnectionClass:self.apsConnectionClass
                                                      escrowRequestClass:self.escrowRequestClass
                                                                    cdpd:self.cdpd];
            self.contexts[key] = context;
        }
    });

    return context;
}

- (void)clearAllContexts
{
    if(self.contexts) {
        dispatch_sync(self.queue, ^{
            [self.contexts removeAllObjects];
        });
    }
}

- (void)fetchEgoPeerID:(NSString* _Nullable)container
               context:(NSString*)context
                 reply:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))reply
{
    if(!container) {
        container = OTCKContainerName;
    }
    secnotice("octagon", "Received a fetch peer ID for container (%@) and context (%@)", container, context);
    OTCuttlefishContext* cfshContext = [self contextForContainerName:container contextID:context];
    [cfshContext rpcFetchEgoPeerID:^(NSString * _Nullable peerID,
                                     NSError * _Nullable error) {
        reply(peerID, XPCSanitizeError(error));
    }];
}

- (void)fetchTrustStatus:(NSString *)container
                 context:(NSString *)context
           configuration:(OTOperationConfiguration *)configuration
                   reply:(void (^)(CliqueStatus status,
                                   NSString* _Nullable peerID,
                                   NSNumber * _Nullable numberOfPeersInOctagon,
                                   BOOL isExcluded,
                                   NSError* _Nullable error))reply
{
    if(!container) {
        container = OTCKContainerName;
    }
    secnotice("octagon", "Received a trust status for container (%@) and context (%@)", container, context);
    OTCuttlefishContext* cfshContext = [self contextForContainerName:container contextID:context];

    [cfshContext rpcTrustStatus:configuration reply:^(CliqueStatus status,
                                                      NSString * _Nullable peerID,
                                                      NSDictionary<NSString *,NSNumber *> * _Nullable peerCountByModelID,
                                                      BOOL isExcluded,
                                                      BOOL isLocked,
                                                      NSError * _Nullable error) {
        // Our clients don't need the whole breakout of peers, so just count for them
        long peerCount = 0;
        for(NSNumber* n in peerCountByModelID.allValues) {
            peerCount += [n longValue];
        }

        reply(status, peerID, @(peerCount), isExcluded, error);
    }];
}

- (void)fetchCliqueStatus:(NSString* _Nullable)container
                  context:(NSString*)contextID
            configuration:(OTOperationConfiguration *)configuration
                    reply:(void (^)(CliqueStatus cliqueStatus, NSError* _Nullable error))reply
{
    if(!container) {
        container = OTCKContainerName;
    }
    if(configuration == nil) {
        configuration = [[OTOperationConfiguration alloc] init];
    }

    __block OTCuttlefishContext* context = nil;
    dispatch_sync(self.queue, ^{
        NSString* key = [NSString stringWithFormat:@"%@-%@", container, contextID];

        context = self.contexts[key];
    });

    if(!context) {
        reply(-1, [NSError errorWithDomain:OctagonErrorDomain
                                       code:OTErrorNoSuchContext
                                description:[NSString stringWithFormat:@"No context for (%@,%@)", container, contextID]]);
        return;
    }


    [context rpcTrustStatus:configuration reply:^(CliqueStatus status,
                                                  NSString* egoPeerID,
                                                  NSDictionary<NSString *,NSNumber *> * _Nullable peerCountByModelID,
                                                  BOOL isExcluded,
                                                  BOOL isLocked,
                                                  NSError * _Nullable error) {
        reply(status, error);
    }];
}

- (void)status:(NSString* _Nullable)containerName
       context:(NSString*)contextID
         reply:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply
{
    if(!containerName) {
        containerName = OTCKContainerName;
    }

    secnotice("octagon", "Received a status RPC for container (%@) and context (%@)", containerName, contextID);

    __block OTCuttlefishContext* context = nil;
    dispatch_sync(self.queue, ^{
        NSString* key = [NSString stringWithFormat:@"%@-%@", containerName, contextID];

        context = self.contexts[key];
    });

    if(!context) {
        reply(nil, [NSError errorWithDomain:OctagonErrorDomain
                                       code:OTErrorNoSuchContext
                                description:[NSString stringWithFormat:@"No context for (%@,%@)", containerName, contextID]]);
        return;
    }

    [context rpcStatus:reply];
}

- (void)startOctagonStateMachine:(NSString* _Nullable)container
                         context:(NSString*)context
                           reply:(void (^)(NSError* _Nullable error))reply
{
    secnotice("octagon", "Received a start-state-machine RPC for container (%@) and context (%@)", container, context);

    OTCuttlefishContext* cfshContext = [self contextForContainerName:container contextID:context];
    [cfshContext startOctagonStateMachine];
    reply(nil);
}

- (void)resetAndEstablish:(NSString *)container
                  context:(NSString *)context
                  altDSID:(NSString*)altDSID
              resetReason:(CuttlefishResetReason)resetReason
                    reply:(void (^)(NSError * _Nullable))reply
{
    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityResetAndEstablish];

    OTCuttlefishContext* cfshContext = [self contextForContainerName:container contextID:context];
    [cfshContext startOctagonStateMachine];
    [cfshContext rpcResetAndEstablish:resetReason reply:^(NSError* resetAndEstablishError) {
        [tracker stopWithEvent:OctagonEventResetAndEstablish result:resetAndEstablishError];
        reply(resetAndEstablishError);
    }];
}

- (void)establish:(NSString *)container
          context:(NSString *)context
          altDSID:(NSString*)altDSID
            reply:(void (^)(NSError * _Nullable))reply
{
    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityEstablish];

    OTCuttlefishContext* cfshContext = [self contextForContainerName:container contextID:context];
    [cfshContext startOctagonStateMachine];
    [cfshContext rpcEstablish:altDSID reply:^(NSError* establishError) {
        [tracker stopWithEvent:OctagonEventEstablish result:establishError];
        reply(establishError);
    }];
}

- (void)leaveClique:(NSString* _Nullable)container
            context:(NSString*)context
              reply:(void (^)(NSError* _Nullable error))reply
{
    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityLeaveClique];

    OTCuttlefishContext* cfshContext = [self contextForContainerName:container contextID:context];
    [cfshContext startOctagonStateMachine];
    [cfshContext rpcLeaveClique:^(NSError* leaveError) {
        [tracker stopWithEvent:OctagonEventLeaveClique result:leaveError];
        reply(leaveError);
    }];
}

- (void)removeFriendsInClique:(NSString* _Nullable)container
                      context:(NSString*)context
                      peerIDs:(NSArray<NSString*>*)peerIDs
                        reply:(void (^)(NSError* _Nullable error))reply
{
    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityRemoveFriendsInClique];

    OTCuttlefishContext* cfshContext = [self contextForContainerName:container contextID:context];
    [cfshContext startOctagonStateMachine];
    [cfshContext rpcRemoveFriendsInClique:peerIDs reply:^(NSError* removeFriendsError) {
        [tracker stopWithEvent:OctagonEventRemoveFriendsInClique result:removeFriendsError];
        reply(removeFriendsError);
    }];
}

- (void)peerDeviceNamesByPeerID:(NSString* _Nullable)container
                        context:(NSString*)context
                          reply:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply
{
    OTCuttlefishContext* cfshContext = [self contextForContainerName:container contextID:context];
    [cfshContext rpcFetchDeviceNamesByPeerID:reply];
}

- (void)fetchAllViableBottles:(NSString* _Nullable)container
                      context:(NSString*)context
                        reply:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs, NSArray<NSString*>* _Nullable sortedPartialBottleIDs, NSError* _Nullable error))reply
{
    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityFetchAllViableBottles];

    OTCuttlefishContext* cfshContext = [self contextForContainerName:container contextID:context];
    [cfshContext rpcFetchAllViableBottles:^(NSArray<NSString *> * _Nullable sortedBottleIDs,
                                            NSArray<NSString *> * _Nullable sortedPartialEscrowRecordIDs,
                                            NSError * _Nullable error) {
        [tracker stopWithEvent:OctagonEventFetchAllBottles result:error];
        reply(sortedBottleIDs, sortedPartialEscrowRecordIDs, error);
    }];
}

- (void)fetchEscrowContents:(NSString* _Nullable)containerName
                  contextID:(NSString *)contextID
                      reply:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply
{
    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityFetchEscrowContents];

    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:contextID];
    [cfshContext fetchEscrowContents:^(NSData *entropy,
                                       NSString *bottleID,
                                       NSData *signingPublicKey,
                                       NSError *error) {
        [tracker stopWithEvent:OctagonEventFetchEscrowContents result:error];
        reply(entropy, bottleID, signingPublicKey, error);
    }];
}

////
// MARK: Pairing Routines as Initiator
////
- (void)rpcPrepareIdentityAsApplicantWithConfiguration:(OTJoiningConfiguration*)config
                                              reply:(void (^)(NSString * _Nullable peerID,
                                                              NSData * _Nullable permanentInfo,
                                                              NSData * _Nullable permanentInfoSig,
                                                              NSData * _Nullable stableInfo,
                                                              NSData * _Nullable stableInfoSig,
                                                              NSError * _Nullable error))reply
{
    OTCuttlefishContext* cfshContext = [self contextForContainerName:config.containerName contextID:config.contextID];
    [cfshContext handlePairingRestart:config];
    [cfshContext startOctagonStateMachine];
    [cfshContext rpcPrepareIdentityAsApplicantWithConfiguration:config
                                                          epoch:config.epoch
                                                          reply:^(NSString * _Nullable peerID,
                                                                  NSData * _Nullable permanentInfo,
                                                                  NSData * _Nullable permanentInfoSig,
                                                                  NSData * _Nullable stableInfo,
                                                                  NSData * _Nullable stableInfoSig,
                                                                  NSError * _Nullable error) {
        reply(peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error);
    }];
}

- (void)rpcJoinWithConfiguration:(OTJoiningConfiguration*)config
                       vouchData:(NSData*)vouchData
                        vouchSig:(NSData*)vouchSig
                           reply:(void (^)(NSError * _Nullable error))reply
{
    OTCuttlefishContext* cfshContext = [self contextForContainerName:config.containerName contextID:config.contextID];
    [cfshContext handlePairingRestart:config];
    [cfshContext startOctagonStateMachine];
    [cfshContext rpcJoin:vouchData vouchSig:vouchSig reply:^(NSError * _Nullable error) {
        reply(error);
    }];
}

////
// MARK: Pairing Routines as Acceptor
////

- (void)rpcEpochWithConfiguration:(OTJoiningConfiguration*)config
           reply:(void (^)(uint64_t epoch,
                           NSError * _Nullable error))reply
{
    OTCuttlefishContext* acceptorCfshContext = [self contextForContainerName:config.containerName contextID:config.contextID];
    [acceptorCfshContext startOctagonStateMachine];

    // Let's assume that the new device's machine ID has made it to the IDMS list by now, and let's refresh our idea of that list
    [acceptorCfshContext requestTrustedDeviceListRefresh];

    OTClientStateMachine *clientStateMachine = [self clientStateMachineForContainerName:config.containerName contextID:config.contextID clientName:config.pairingUUID];

    [clientStateMachine startOctagonStateMachine];

    [clientStateMachine rpcEpoch:acceptorCfshContext reply:^(uint64_t epoch, NSError * _Nullable error) {
        reply(epoch, error);
    }];
}

- (void)rpcVoucherWithConfiguration:(OTJoiningConfiguration*)config
            peerID:(NSString*)peerID
     permanentInfo:(NSData *)permanentInfo
  permanentInfoSig:(NSData *)permanentInfoSig
        stableInfo:(NSData *)stableInfo
     stableInfoSig:(NSData *)stableInfoSig
             reply:(void (^)(NSData* voucher, NSData* voucherSig, NSError * _Nullable error))reply
{
    OTCuttlefishContext* acceptorCfshContext = [self contextForContainerName:config.containerName contextID:config.contextID];
    [acceptorCfshContext startOctagonStateMachine];

    // Let's assume that the new device's machine ID has made it to the IDMS list by now, and let's refresh our idea of that list
    [acceptorCfshContext requestTrustedDeviceListRefresh];
    OTClientStateMachine *clientStateMachine = [self clientStateMachineForContainerName:config.containerName contextID:config.contextID clientName:config.pairingUUID];

    [clientStateMachine rpcVoucher:acceptorCfshContext peerID:peerID permanentInfo:permanentInfo permanentInfoSig:permanentInfoSig stableInfo:stableInfo stableInfoSig:stableInfoSig reply:^(NSData *voucher, NSData *voucherSig, NSError *error) {
        reply(voucher, voucherSig, error);
    }];
}

- (void)restore:(NSString * _Nullable)containerName
      contextID:(nonnull NSString *)contextID
     bottleSalt:(nonnull NSString *)bottleSalt
        entropy:(nonnull NSData *)entropy
       bottleID:(nonnull NSString *)bottleID
          reply:(nonnull void (^)(NSError * _Nullable))reply {
    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityBottledPeerRestore];

    secnotice("octagon", "restore via bottle invoked for container: %@, context: %@", containerName, contextID);

    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:contextID];

    [cfshContext startOctagonStateMachine];

    [cfshContext joinWithBottle:bottleID entropy:entropy bottleSalt:bottleSalt reply:^(NSError *error) {
        [tracker stopWithEvent:OctagonEventBottledPeerRestore result:error];
        reply(error);
    }];
}

////
// MARK: Ghost busting using ramp records
////

-(BOOL) ghostbustByMidEnabled {
    [self ensureRampsInitialized];
    return [self.gbmidRamp checkRampStateWithError:nil];
}

-(BOOL) ghostbustBySerialEnabled {
    [self ensureRampsInitialized];
    return [self.gbserialRamp checkRampStateWithError:nil];
}

-(BOOL) ghostbustByAgeEnabled {
    [self ensureRampsInitialized];
    return [self.gbAgeRamp checkRampStateWithError:nil];
}

////
// MARK: Analytics
////

- (void)setupAnalytics
{
    WEAKIFY(self);

    [[self.loggerClass logger] AddMultiSamplerForName:@"Octagon-healthSummary"
                                     withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport
                                                block:^NSDictionary<NSString *,NSNumber *> *{
        STRONGIFY(self);

        // We actually only care about the default context for the default container
        OTCuttlefishContext* cuttlefishContext = [self contextForContainerName:OTCKContainerName contextID:OTDefaultContext];

        secnotice("octagon-analytics", "Reporting analytics for container: %@, context: %@", OTCKContainerName, OTDefaultContext);

        NSMutableDictionary* values = [NSMutableDictionary dictionary];

        NSError* error = nil;
        SOSCCStatus sosStatus = [self.sosAdapter circleStatus:&error];
        if(error) {
            secnotice("octagon-analytics", "Error fetching SOS status: %@", error);
        }
        values[OctagonAnalyticsSOSStatus] = @((int)sosStatus);
        NSDate* dateOfLastPPJ = [[CKKSAnalytics logger] datePropertyForKey:OctagonEventUpgradePreflightPreapprovedJoin];
        values[OctagonAnalyticsDateOfLastPreflightPreapprovedJoin] = @([CKKSAnalytics fuzzyDaysSinceDate:dateOfLastPPJ]);

        values[OctagonAnalyticsStateMachineState] = OctagonStateMap()[cuttlefishContext.stateMachine.currentState];

        NSError* metadataError = nil;
        OTAccountMetadataClassC* metadata = [cuttlefishContext.accountMetadataStore loadOrCreateAccountMetadata:&metadataError];
        if(!metadata || metadataError) {
            secnotice("octagon-analytics", "Error fetching Octagon metadata: %@", metadataError);
        }
        values[OctagonAnalyticIcloudAccountState] = metadata ? @(metadata.icloudAccountState) : nil;
        values[OctagonAnalyticCDPBitStatus] = metadata? @(metadata.cdpState) : nil;
        values[OctagonAnalyticsTrustState] = metadata ? @(metadata.trustState) : nil;

        TPSyncingPolicy* syncingPolicy = [metadata getTPSyncingPolicy];
        values[OctagonAnalyticsUserControllableViewsSyncing] = syncingPolicy ? @(syncingPolicy.syncUserControllableViewsAsBoolean) : nil;

        NSDate* healthCheck = [cuttlefishContext currentMemoizedLastHealthCheck];
        values[OctagonAnalyticsLastHealthCheck] = @([CKKSAnalytics fuzzyDaysSinceDate:healthCheck]);

        NSDate* dateOfLastKSR = [[CKKSAnalytics logger] datePropertyForKey: OctagonAnalyticsLastKeystateReady];
        values[OctagonAnalyticsLastKeystateReady] = @([CKKSAnalytics fuzzyDaysSinceDate:dateOfLastKSR]);

        if(metadata && metadata.icloudAccountState == OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE) {
            values[OctagonAnalyticsAttemptedJoin] = @(metadata.attemptedJoin);

            NSError* machineIDError = nil;
            NSString* machineID = [cuttlefishContext.authKitAdapter machineID:&machineIDError];
            if(machineIDError) {
                secnotice("octagon-analytics", "Error fetching machine ID: %@", metadataError);
            }

            values[OctagonAnalyticsHaveMachineID] = @(machineID != nil);

            if(machineID) {
                NSError* midOnListError = nil;
                BOOL midOnList = [cuttlefishContext machineIDOnMemoizedList:machineID error:&midOnListError];

                if(midOnListError) {
                    secnotice("octagon-analytics", "Error fetching 'mid on list': %@", midOnListError);
                } else {
                    values[OctagonAnalyticsMIDOnMemoizedList] = @(midOnList);
                }

                NSError* peersWithMIDError = nil;
                NSNumber* peersWithMID = [cuttlefishContext numberOfPeersInModelWithMachineID:machineID error:&peersWithMIDError];
                if(peersWithMID && peersWithMIDError == nil) {
                    values[OctagonAnalyticsPeersWithMID] = peersWithMID;
                } else {
                    secnotice("octagon-analytics", "Error fetching how many peers have our MID: %@", midOnListError);
                }
            }
        }

        // Track CFU usage and success/failure metrics
        // 1. Users in a good state will have no outstanding CFU, and will not have done a CFU
        // 2. Users in a bad state who have not repsonded to the CFU (repaired) will have a pending CFU.
        // 3. Users in a bad state who have acted on the CFU will have no pending CFU, but will have CFU failures.
        //
        // We also record the time window between the last followup completion and invocation.
        NSDate* dateOfLastFollowup = [[CKKSAnalytics logger] datePropertyForKey: OctagonAnalyticsLastCoreFollowup];
        values[OctagonAnalyticsLastCoreFollowup] = @([CKKSAnalytics fuzzyDaysSinceDate:dateOfLastFollowup]);
        // We used to report this, but it was never set
        //values[OctagonAnalyticsCoreFollowupStatus] = @(followupHandler.followupStatus);

        for (NSString *type in [self cdpContextTypes]) {
            NSString *metricName = [NSString stringWithFormat:@"%@%@", OctagonAnalyticsCDPStateRun, type];
            NSString *countName = [NSString stringWithFormat:@"%@%@Tries", OctagonAnalyticsCDPStateRun, type];

            NSDate *lastFailure = [[CKKSAnalytics logger] datePropertyForKey:metricName];
            if (lastFailure) {
                values[metricName] = @([CKKSAnalytics fuzzyDaysSinceDate:lastFailure]);
                values[countName] = [[CKKSAnalytics logger] numberPropertyForKey:countName];
            }
        }

        // SecEscrowRequest
        id<SecEscrowRequestable> request = [self.escrowRequestClass request:&error];
        if (request) {
            values[OctagonAnalyticsPrerecordPending] = @([request pendingEscrowUpload:&error]);
            if (error) {
                secnotice("octagon-analytics", "Error fetching pendingEscrowUpload status: %@", error);
            }
        } else {
            secnotice("octagon-analytics", "Error fetching escrowRequestClass: %@", error);
        }

        {
            ACAccountStore *store = [[ACAccountStore alloc] init];
            ACAccount* primaryAccount = [store aa_primaryAppleAccount];
            if(primaryAccount) {
                values[OctagonAnalyticsKVSProvisioned] = @([primaryAccount isProvisionedForDataclass:ACAccountDataclassKeyValue]);
                values[OctagonAnalyticsKVSEnabled] = @([primaryAccount isEnabledForDataclass:ACAccountDataclassKeyValue]);
                values[OctagonAnalyticsKeychainSyncProvisioned] = @([primaryAccount isProvisionedForDataclass:ACAccountDataclassKeychainSync]);
                values[OctagonAnalyticsKeychainSyncEnabled] = @([primaryAccount isEnabledForDataclass:ACAccountDataclassKeychainSync]);
                values[OctagonAnalyticsCloudKitProvisioned] = @([primaryAccount isProvisionedForDataclass:ACAccountDataclassCKDatabaseService]);
                values[OctagonAnalyticsCloudKitEnabled] = @([primaryAccount isEnabledForDataclass:ACAccountDataclassCKDatabaseService]);
            }
        }

        return values;
    }];

    [[self.loggerClass logger] AddMultiSamplerForName:@"CFU-healthSummary"
                                     withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport
                                                block:^NSDictionary<NSString *,NSNumber *> *{
        OTCuttlefishContext* cuttlefishContext = [self contextForContainerName:OTCKContainerName contextID:OTDefaultContext];
        return [cuttlefishContext.followupHandler sfaStatus];
    }];
}

- (NSArray<OTCliqueCDPContextType> *)cdpContextTypes
{
    static NSArray<OTCliqueCDPContextType> *contextTypes = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        contextTypes = @[OTCliqueCDPContextTypeNone,
                         OTCliqueCDPContextTypeSignIn,
                         OTCliqueCDPContextTypeRepair,
                         OTCliqueCDPContextTypeFinishPasscodeChange,
                         OTCliqueCDPContextTypeRecoveryKeyGenerate,
                         OTCliqueCDPContextTypeRecoveryKeyNew,
                         OTCliqueCDPContextTypeUpdatePasscode,
        ];
    });
    return contextTypes;
}

////
// MARK: Recovery Key
////

- (void) createRecoveryKey:(NSString* _Nullable)containerName
                 contextID:(NSString *)contextID
               recoveryKey:(NSString *)recoveryKey
                     reply:(void (^)( NSError *error))reply
{
    secnotice("octagon", "Setting recovery key for container: %@, context: %@", containerName, contextID);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivitySetRecoveryKey];

    if (!self.sosEnabledForPlatform) {
        secnotice("octagon-recovery", "Device does not participate in SOS; cannot enroll recovery key in Octagon");
        NSError* notFullPeerError = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorLimitedPeer userInfo:@{NSLocalizedDescriptionKey : @"Device is considered a limited peer, cannot enroll recovery key in Octagon"}];
        [tracker stopWithEvent:OctagonEventRecoveryKey result:notFullPeerError];
        reply(notFullPeerError);
        return;
    }

    CFErrorRef validateError = NULL;
    bool res = SecPasswordValidatePasswordFormat(kSecPasswordTypeiCloudRecoveryKey, (__bridge CFStringRef)recoveryKey, &validateError);
    if (!res) {
        NSError *validateErrorWrapper = nil;
        NSMutableDictionary *userInfo = [NSMutableDictionary dictionary];
        userInfo[NSLocalizedDescriptionKey] = @"malformed recovery key";
        if(validateError) {
            userInfo[NSUnderlyingErrorKey] = CFBridgingRelease(validateError);
        }

        validateErrorWrapper = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorRecoveryKeyMalformed userInfo:userInfo];

        secerror("recovery failed validation with error:%@", validateError);

        [tracker stopWithEvent:OctagonEventSetRecoveryKeyValidationFailed result:validateErrorWrapper];
        reply(validateErrorWrapper);
        return;
    }

    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:contextID];

    [cfshContext startOctagonStateMachine];

    [cfshContext rpcSetRecoveryKey:recoveryKey reply:^(NSError * _Nullable error) {
        [tracker stopWithEvent:OctagonEventRecoveryKey result:error];
        reply(error);
    }];
}

- (void) joinWithRecoveryKey:(NSString* _Nullable)containerName
                   contextID:(NSString *)contextID
                 recoveryKey:(NSString*)recoveryKey
                       reply:(void (^)(NSError * _Nullable))reply
{
    secnotice("octagon", "join with recovery key invoked for container: %@, context: %@", containerName, contextID);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityJoinWithRecoveryKey];

    CFErrorRef validateError = NULL;
    bool res = SecPasswordValidatePasswordFormat(kSecPasswordTypeiCloudRecoveryKey, (__bridge CFStringRef)recoveryKey, &validateError);
    if (!res) {
        NSError *validateErrorWrapper = nil;
        NSMutableDictionary *userInfo = [NSMutableDictionary dictionary];
        userInfo[NSLocalizedDescriptionKey] = @"malformed recovery key";
        if(validateError) {
            userInfo[NSUnderlyingErrorKey] = CFBridgingRelease(validateError);
        }

        validateErrorWrapper = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorRecoveryKeyMalformed userInfo:userInfo];

        secerror("recovery failed validation with error:%@", validateError);

        [tracker stopWithEvent:OctagonEventJoinRecoveryKeyValidationFailed result:validateErrorWrapper];
        reply(validateErrorWrapper);
        return;
    }

    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:contextID];

    [cfshContext startOctagonStateMachine];

    [cfshContext joinWithRecoveryKey:recoveryKey reply:^(NSError *error) {
        if ((error.code == TrustedPeersHelperErrorCodeNotEnrolled || error.code == TrustedPeersHelperErrorCodeUntrustedRecoveryKeys)
            && [error.domain isEqualToString:TrustedPeersHelperErrorDomain]){
            // If we hit either of these errors, and the local device thinks it should be able to set a recovery key,
            // let's reset and establish octagon then enroll this recovery key in the new circle

            if (!self.sosEnabledForPlatform) {
                secerror("octagon: recovery key is not enrolled in octagon, and current device can't set recovery keys");
                [tracker stopWithEvent:OctagonEventJoinRecoveryKeyCircleResetFailed result:error];
                reply(error);
                return;
            }

            secerror("octagon, recovery key is not enrolled in octagon, resetting octagon circle");
            [[self.loggerClass logger] logResultForEvent:OctagonEventJoinRecoveryKeyCircleReset hardFailure:NO result:error];

            [cfshContext rpcResetAndEstablish:CuttlefishResetReasonRecoveryKey reply:^(NSError *resetError) {
                if (resetError) {
                    secerror("octagon, failed to reset octagon");
                    [tracker stopWithEvent:OctagonEventJoinRecoveryKeyCircleResetFailed result:resetError];
                    reply(resetError);
                    return;
                } else {
                    // Now enroll the recovery key
                    secnotice("octagon", "attempting enrolling recovery key");
                    [self createRecoveryKey:containerName contextID:contextID recoveryKey:recoveryKey reply:^(NSError *enrollError) {
                        if(enrollError){
                            secerror("octagon, failed to enroll new recovery key: %@", enrollError);
                            [tracker stopWithEvent:OctagonEventJoinRecoveryKeyEnrollFailed result:enrollError];
                            reply(enrollError);
                            return;
                        }else{
                            secnotice("octagon", "successfully enrolled recovery key");
                            [tracker stopWithEvent:OctagonEventRecoveryKey result:nil];
                            reply (nil);
                            return;
                        }
                    }];
                }
            }];
        } else {
            secerror("octagon, join with recovery key failed: %d", (int)[error code]);
            [tracker stopWithEvent:OctagonEventJoinRecoveryKeyFailed result:error];
            reply(error);
        }
    }];
}

- (void)xpc24HrNotification
{
    secnotice("octagon-health", "Received 24hr xpc notification");

    [self healthCheck:OTCKContainerName context:OTDefaultContext skipRateLimitingCheck:NO reply:^(NSError * _Nullable error) {
        if(error) {
            secerror("octagon: error attempting to check octagon health: %@", error);
        } else {
            secnotice("octagon", "health check success");
        }
    }];
}

- (void)healthCheck:(NSString *)container context:(NSString *)context skipRateLimitingCheck:(BOOL)skipRateLimitingCheck reply:(void (^)(NSError *_Nullable error))reply
{
    OTCuttlefishContext* cfshContext = [self contextForContainerName:container contextID:context];

    secnotice("octagon", "notifying container of change");

    [cfshContext notifyContainerChange:nil];

    [cfshContext checkOctagonHealth:skipRateLimitingCheck reply:^(NSError *error) {
        if(error) {
            reply(error);
        } else {
            reply(nil);
        }
    }];
}

- (void)setSOSEnabledForPlatformFlag:(bool) value
{
    self.sosEnabledForPlatform = value;
}

- (void)allContextsHalt
{
    for(OTCuttlefishContext* context in self.contexts.allValues) {
        [context.stateMachine haltOperation];

        // Also, clear the viewManager strong pointer
        [context clearCKKSViewManager];
    }
}

- (void)allContextsDisablePendingFlags
{
    for(OTCuttlefishContext* context in self.contexts.allValues) {
        [context.stateMachine disablePendingFlags];
    }
}

- (bool)allContextsPause:(uint64_t)within
{
    for(OTCuttlefishContext* context in self.contexts.allValues) {
        if(context.stateMachine.currentState != OctagonStateMachineNotStarted) {
            if([context.stateMachine.paused wait:within] != 0) {
                return false;
            }
        }
    }
    return true;
}

- (void)waitForOctagonUpgrade:(NSString* _Nullable)containerName
                      context:(NSString*)context
                        reply:(void (^)(NSError* error))reply

{
    secnotice("octagon-sos", "Attempting wait for octagon upgrade");
    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:context];

    [cfshContext startOctagonStateMachine];

    [cfshContext waitForOctagonUpgrade:^(NSError * _Nonnull error) {
        reply(error);
    }];
}

- (OTFollowupContextType)cliqueCDPTypeToFollowupContextType:(OTCliqueCDPContextType)type
{
    if ([type isEqualToString:OTCliqueCDPContextTypeNone]) {
        return OTFollowupContextTypeNone;
    } else if ([type isEqualToString:OTCliqueCDPContextTypeSignIn]) {
        return OTFollowupContextTypeNone;
    } else if ([type isEqualToString:OTCliqueCDPContextTypeRepair]) {
        return OTFollowupContextTypeStateRepair;
    } else if ([type isEqualToString:OTCliqueCDPContextTypeFinishPasscodeChange]) {
        return OTFollowupContextTypeOfflinePasscodeChange;
    } else if ([type isEqualToString:OTCliqueCDPContextTypeRecoveryKeyGenerate]) {
        return OTFollowupContextTypeRecoveryKeyRepair;
    } else if ([type isEqualToString:OTCliqueCDPContextTypeRecoveryKeyNew]) {
        return OTFollowupContextTypeRecoveryKeyRepair;
    } else if ([type isEqualToString:OTCliqueCDPContextTypeUpdatePasscode]) {
        return OTFollowupContextTypeNone;
    } else {
        return OTFollowupContextTypeNone;
    }
}

- (void)postCDPFollowupResult:(BOOL)success
                         type:(OTCliqueCDPContextType)type
                        error:(NSError * _Nullable)error
                containerName:(NSString* _Nullable)containerName
                  contextName:(NSString *)contextName
                        reply:(void (^)(NSError *error))reply
{
    NSString* metricName = [NSString stringWithFormat:@"%@%@", OctagonAnalyticsCDPStateRun, type];
    NSString* countName = [NSString stringWithFormat:@"%@%@Tries", OctagonAnalyticsCDPStateRun, type];

    [[CKKSAnalytics logger] logResultForEvent:metricName
                                  hardFailure:NO
                                       result:error];
    if (error) {
        [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:metricName];
        [[CKKSAnalytics logger] incrementIntegerPropertyForKey:countName];
    } else {
        [[CKKSAnalytics logger] setDateProperty:NULL forKey:metricName];
        [[CKKSAnalytics logger] setNumberProperty:NULL forKey:countName];
    }

    // Always return without error
    reply(nil);
}

- (void)tapToRadar:(NSString *)action
       description:(NSString *)description
             radar:(NSString *)radar
             reply:(void (^)(NSError * _Nullable))reply
{
    SecTapToRadar *ttr = [[SecTapToRadar alloc] initTapToRadar:action description:description radar:radar];
    [ttr trigger];
    reply(NULL);
}

- (void)refetchCKKSPolicy:(NSString * _Nullable)containerName
                contextID:(nonnull NSString *)contextID
                    reply:(nonnull void (^)(NSError * _Nullable))reply {
    secnotice("octagon-ckks", "refetch-ckks-policy");

    if(!containerName) {
        containerName = OTCKContainerName;
    }

    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:contextID];

    if(!cfshContext) {
        reply([NSError errorWithDomain:OctagonErrorDomain
                                  code:OTErrorNoSuchContext
                           description:[NSString stringWithFormat:@"No context for (%@,%@)", containerName, contextID]]);
        return;
    }

    [cfshContext rpcRefetchCKKSPolicy:^(NSError* error) {
        secnotice("octagon-ckks", "refetch-ckks-policy result: %@", error ?: @"no error");
        reply(error);
    }];
}

- (void)getCDPStatus:(NSString * _Nullable)containerName
           contextID:(nonnull NSString *)contextID
               reply:(nonnull void (^)(OTCDPStatus, NSError * _Nullable))reply {
    secnotice("octagon-cdp", "get-cdp-status");

    if(!containerName) {
        containerName = OTCKContainerName;
    }

    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:contextID];

    if(!cfshContext) {
        reply(OTCDPStatusUnknown, [NSError errorWithDomain:OctagonErrorDomain
                                                      code:OTErrorNoSuchContext
                                               description:[NSString stringWithFormat:@"No context for (%@,%@)", containerName, contextID]]);
        return;
    }

    NSError* error = nil;
    OTCDPStatus status = [cfshContext getCDPStatus:&error];

    reply(status, error);
}


- (void)setCDPEnabled:(NSString * _Nullable)containerName
            contextID:(nonnull NSString *)contextID
                reply:(nonnull void (^)(NSError * _Nullable))reply {
    secnotice("octagon-cdp", "set-cdp-enabled");
    if(!containerName) {
        containerName = OTCKContainerName;
    }

    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:contextID];

    if(!cfshContext) {
        reply([NSError errorWithDomain:OctagonErrorDomain
                                  code:OTErrorNoSuchContext
                           description:[NSString stringWithFormat:@"No context for (%@,%@)", containerName, contextID]]);
        return;
    }

    NSError* localError = nil;
    [cfshContext setCDPEnabled:&localError];

    reply(localError);
}

- (void)fetchEscrowRecords:(NSString * _Nullable)containerName
                 contextID:(NSString*)contextID
                forceFetch:(BOOL)forceFetch
                     reply:(nonnull void (^)(NSArray<NSData *> * _Nullable records,
                                             NSError * _Nullable error))reply {

    secnotice("octagon-fetch-escrow-records", "fetching records");
    if(!containerName) {
        containerName = OTCKContainerName;
    }

    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:contextID];

    if(!cfshContext) {
        reply(nil, [NSError errorWithDomain:OctagonErrorDomain
                                       code:OTErrorNoSuchContext
                                description:[NSString stringWithFormat:@"No context for (%@,%@)", containerName, contextID]]);
        return;
    }

    [cfshContext rpcFetchAllViableEscrowRecords:forceFetch reply:^(NSArray<NSData *> * _Nullable records, NSError * _Nullable error) {
        if(error) {
            secerror("octagon-fetch-escrow-records: error fetching records: %@", error);
            reply(nil, error);
            return;
        }
        secnotice("octagon-fetch-escrow-records", "successfully fetched records");
        reply(records, nil);
    }];
}

- (void)invalidateEscrowCache:(NSString * _Nullable)containerName
                    contextID:(NSString*)contextID
                        reply:(nonnull void (^)(NSError * _Nullable error))reply {

    secnotice("octagon-remove-escrow-cache", "beginning removing escrow cache!");
    if(!containerName) {
        containerName = OTCKContainerName;
    }

    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:contextID];

    if(!cfshContext) {
        reply([NSError errorWithDomain:OctagonErrorDomain
                                  code:OTErrorNoSuchContext
                           description:[NSString stringWithFormat:@"No context for (%@,%@)", containerName, contextID]]);
        return;
    }

    [cfshContext rpcInvalidateEscrowCache:^(NSError * _Nullable invalidateError) {
        if(invalidateError) {
            secerror("octagon-remove-escrow-cache: error invalidating escrow cache: %@", invalidateError);
            reply(invalidateError);
            return;
        }
        secnotice("octagon-remove-escrow-caches", "successfully invalidated escrow cache");
        reply(nil);
    }];
}

- (void)setUserControllableViewsSyncStatus:(NSString* _Nullable)containerName
                                 contextID:(NSString*)contextID
                                   enabled:(BOOL)enabled
                                     reply:(void (^)(BOOL nowSyncing, NSError* _Nullable error))reply
{
    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:contextID];

    if(!cfshContext) {
        reply(NO, [NSError errorWithDomain:OctagonErrorDomain
                                      code:OTErrorNoSuchContext
                               description:[NSString stringWithFormat:@"No context for (%@,%@)", containerName, contextID]]);
        return;
    }

    [cfshContext rpcSetUserControllableViewsSyncingStatus:enabled reply:^(BOOL areSyncing, NSError * _Nullable error) {
        if(error) {
            secerror("octagon-user-controllable-views: error setting status: %@", error);
            reply(NO, error);
            return;
        }

        secnotice("octagon-user-controllable-views", "successfully set status to: %@", areSyncing ? @"enabled" : @"paused");
        reply(areSyncing, nil);
    }];
}

- (void)fetchUserControllableViewsSyncStatus:(NSString* _Nullable)containerName
                                   contextID:(NSString*)contextID
                                       reply:(void (^)(BOOL nowSyncing, NSError* _Nullable error))reply
{
    OTCuttlefishContext* cfshContext = [self contextForContainerName:containerName contextID:contextID];

    if(!cfshContext) {
        reply(NO, [NSError errorWithDomain:OctagonErrorDomain
                                      code:OTErrorNoSuchContext
                               description:[NSString stringWithFormat:@"No context for (%@,%@)", containerName, contextID]]);
        return;
    }

    [cfshContext rpcFetchUserControllableViewsSyncingStatus:^(BOOL areSyncing, NSError * _Nullable error) {
        if(error) {
            secerror("octagon-user-controllable-views: error fetching status: %@", error);
            reply(NO, error);
            return;
        }

        secerror("octagon-user-controllable-views: succesfully fetched status as: %@", areSyncing ? @"enabled" : @"paused");
        reply(areSyncing, nil);
    }];
}

@end

#endif
