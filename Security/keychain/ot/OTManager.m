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


#import <TargetConditionals.h>
#import <AppleFeatures/AppleFeatures.h>
#include <Security/SecEntitlements.h>
#import <Foundation/NSXPCConnection.h>
#import <Foundation/NSXPCConnection_Private.h>

#import <os/feature_private.h>

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
#import "keychain/ot/OTFollowup.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ot/OTJoiningConfiguration.h"
#import "keychain/ot/OTSOSAdapter.h"

#import "keychain/OctagonTrust/OTCustodianRecoveryKey.h"
#import "keychain/OctagonTrust/OTInheritanceKey.h"

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKeychainView.h"
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

#import <SoftLinking/SoftLinking.h>
#import <CloudServices/SecureBackup.h>

#import "keychain/TrustedPeersHelper/TPHObjcTranslation.h"
#import "keychain/SecureObjectSync/SOSAccountTransaction.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#import "keychain/SecureObjectSync/SOSAccount.h"
#pragma clang diagnostic pop

#import "utilities/SecTapToRadar.h"
#import "keychain/SigninMetrics/OctagonSignPosts.h"

SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, CloudServices);
SOFT_LINK_CLASS(CloudServices, SecureBackup);

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
// Map of "context name" to CKKS sync object
@property dispatch_queue_t queue;

@property id<NSXPCProxyCreating> cuttlefishXPCConnection;

// Dependencies for injection
@property (readonly) id<OTSOSAdapter> sosAdapter;
@property (readonly) id<OTAccountsAdapter> accountsAdapter;
@property (readonly) id<OTAuthKitAdapter> authKitAdapter;
@property (readonly) id<OTTooManyPeersAdapter> tooManyPeersAdapter;
@property (readonly) id<OTDeviceInformationAdapter> deviceInformationAdapter;
@property (readonly) id<OTPersonaAdapter> personaAdapter;
@property (readonly) Class<OctagonAPSConnection> apsConnectionClass;
@property (readonly) Class<SecEscrowRequestable> escrowRequestClass;
@property (readonly) Class<CKKSNotifier> notifierClass;
// If this is nil, all logging is disabled
@property (readonly, nullable) Class<SFAnalyticsProtocol> loggerClass;
@property (nonatomic) BOOL sosEnabledForPlatform;
@property CKKSNearFutureScheduler* savedTLKNotifier;

@property (readonly) CKKSCloudKitClassDependencies* cloudKitClassDependencies;

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
                    accountsAdapter:[[OTAccountsActualAdapter alloc] init]
                     authKitAdapter:[[OTAuthKitActualAdapter alloc] init]
                tooManyPeersAdapter:[[OTTooManyPeersActualAdapter alloc] init]
           deviceInformationAdapter:[[OTDeviceInformationActualAdapter alloc] init]
                     personaAdapter:[[OTPersonaActualAdapter alloc] init]
                 apsConnectionClass:[APSConnection class]
                 escrowRequestClass:[EscrowRequestServer class] // Use the server class here to skip the XPC layer
                      notifierClass:[CKKSNotifyPostNotifier class]
                        loggerClass:[CKKSAnalytics class]
                   lockStateTracker:[CKKSLockStateTracker globalTracker]
                reachabilityTracker:[[CKKSReachabilityTracker alloc] init]
          cloudKitClassDependencies:[CKKSCloudKitClassDependencies forLiveCloudKit]
            cuttlefishXPCConnection:nil
                               cdpd:[[CDPFollowUpController alloc] init]];
}

- (instancetype)initWithSOSAdapter:(id<OTSOSAdapter>)sosAdapter
                   accountsAdapter:(id<OTAccountsAdapter>)accountsAdapter
                    authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
               tooManyPeersAdapter:(id<OTTooManyPeersAdapter>)tooManyPeersAdapter
          deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter
                    personaAdapter:(id<OTPersonaAdapter>)personaAdapter
                apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
                escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
                     notifierClass:(Class<CKKSNotifier>)notifierClass
                       loggerClass:(Class<SFAnalyticsProtocol>)loggerClass
                  lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
               reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
         cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
           cuttlefishXPCConnection:(id<NSXPCProxyCreating>)cuttlefishXPCConnection
                              cdpd:(id<OctagonFollowUpControllerProtocol>)cdpd
{
    if((self = [super init])) {
        _sosAdapter = sosAdapter;
        _accountsAdapter = accountsAdapter;
        _authKitAdapter = authKitAdapter;
        _tooManyPeersAdapter = tooManyPeersAdapter;
        _deviceInformationAdapter = deviceInformationAdapter;
        _personaAdapter = personaAdapter;
        _loggerClass = loggerClass;
        _lockStateTracker = lockStateTracker;
        _reachabilityTracker = reachabilityTracker;

        _sosEnabledForPlatform = OctagonPlatformSupportsSOS();
        _cuttlefishXPCConnection = cuttlefishXPCConnection;

        _cloudKitContainer = [OTManager makeCKContainer:SecCKKSContainerName];
        _accountStateTracker = [[CKKSAccountStateTracker alloc] init:_cloudKitContainer
                                           nsnotificationCenterClass:cloudKitClassDependencies.nsnotificationCenterClass];
        _cloudKitClassDependencies = cloudKitClassDependencies;
        self.contexts = [NSMutableDictionary dictionary];
        self.clients = [NSMutableDictionary dictionary];

        self.queue = dispatch_queue_create("otmanager", DISPATCH_QUEUE_SERIAL);

        _apsConnectionClass = apsConnectionClass;
        _escrowRequestClass = escrowRequestClass;
        _notifierClass = notifierClass;

        _cdpd = cdpd;

        _viewManager = [[CKKSViewManager alloc] initWithContainer:_cloudKitContainer
                                                       sosAdapter:sosAdapter
                                              accountStateTracker:_accountStateTracker
                                                 lockStateTracker:lockStateTracker
                                              reachabilityTracker:_reachabilityTracker
                                                   personaAdapter:_personaAdapter
                                        cloudKitClassDependencies:cloudKitClassDependencies
                                                  accountsAdapter:accountsAdapter];

        WEAKIFY(self);
        _savedTLKNotifier = [[CKKSNearFutureScheduler alloc] initWithName:@"newtlks"
                                                                    delay:5*NSEC_PER_SEC
                                                         keepProcessAlive:true
                                                dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                    block:^{
            STRONGIFY(self);
            [self.viewManager notifyNewTLKsInKeychain];
        }];

        // The default CuttlefishContext always exists:
        (void) [self contextForContainerName:OTCKContainerName contextID:OTDefaultContext];
        
        secnotice("octagon", "otmanager init");
    }
    return self;
}

- (instancetype)initWithSOSAdapter:(id<OTSOSAdapter>)sosAdapter
                  lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                    personaAdapter:(id<OTPersonaAdapter>)personaAdapter
         cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
{
    if((self = [super init])) {
        _sosAdapter = sosAdapter;
        _lockStateTracker = lockStateTracker;
        _personaAdapter = personaAdapter;
        _cloudKitClassDependencies = cloudKitClassDependencies;

        _contexts = [NSMutableDictionary dictionary];

        _cloudKitContainer = [OTManager makeCKContainer:SecCKKSContainerName];
        _accountStateTracker = [[CKKSAccountStateTracker alloc] init:_cloudKitContainer
                                           nsnotificationCenterClass:cloudKitClassDependencies.nsnotificationCenterClass];
        _reachabilityTracker = [[CKKSReachabilityTracker alloc] init];

        _notifierClass = cloudKitClassDependencies.notifierClass;

        _viewManager = [[CKKSViewManager alloc] initWithContainer:_cloudKitContainer
                                                       sosAdapter:sosAdapter
                                              accountStateTracker:_accountStateTracker
                                                 lockStateTracker:lockStateTracker
                                              reachabilityTracker:_reachabilityTracker
                                                   personaAdapter:_personaAdapter
                                        cloudKitClassDependencies:cloudKitClassDependencies
                                                  accountsAdapter:self.accountsAdapter];
        
        self.queue = dispatch_queue_create("otmanager", DISPATCH_QUEUE_SERIAL);

        WEAKIFY(self);
        _savedTLKNotifier = [[CKKSNearFutureScheduler alloc] initWithName:@"newtlks"
                                                                    delay:5*NSEC_PER_SEC
                                                         keepProcessAlive:true
                                                dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                    block:^{
            STRONGIFY(self);
            [self.viewManager notifyNewTLKsInKeychain];
        }];

    }
    return self;
}

- (void)initializeOctagon
{
    secnotice("octagon", "Initializing Octagon...");
    
    secnotice("octagon", "starting default state machine...");
    if (OctagonSupportsPersonaMultiuser()) {
        NSArray<TPSpecificUser*>* activeAccounts = [self.accountsAdapter inflateAllTPSpecificUsers:OTCKContainerName
                                                                                  octagonContextID:OTDefaultContext];
        for (TPSpecificUser* activeAccount in activeAccounts) {
            NSError* contextError = nil;
            
            OTCuttlefishContext* c = [self contextForClientRPCWithActiveAccount:activeAccount
                                                                createIfMissing:YES
                                                        allowNonPrimaryAccounts:YES
                                                                          error:&contextError];
            if (!c || contextError) {
                secnotice("octagon", "failed to get context for active account: %@, error:%@", activeAccount, contextError);
            } else {
                secnotice("octagon", "kicking off state machine for active account: %@", activeAccount);
                [c startOctagonStateMachine];
            }
        }
    }
    
    OTCuttlefishContext* c = [self contextForContainerName:OTCKContainerName
                                                 contextID:OTDefaultContext];
    [c startOctagonStateMachine];
    [self registerForCircleChangedNotifications];
#if TARGET_OS_WATCH
    [self registerForNRDeviceNotifications];
#endif /* TARGET_OS_WATCH */
}

- (BOOL)waitForReady:(OTControlArguments*)arguments
                wait:(int64_t)wait
{
    NSError* error = nil;
    OTCuttlefishContext* c = [self contextForClientRPC:arguments
                                       createIfMissing:NO
                                                 error:&error];
    if(c == nil) {
        secnotice("octagon", "Cannot wait for ready: %@", error);
        return NO;
    }
    return [c waitForReady:wait];
}

- (void)moveToCheckTrustedStateForArguments:(OTControlArguments*)arguments
{
    NSError* error = nil;
    OTCuttlefishContext* c = [self contextForClientRPC:arguments
                                       createIfMissing:NO
                                                 error:&error];

    if(error) {
        secnotice("octagon", "Cannot move to check trusted state: %@", error);
    }
    [c startOctagonStateMachine];
    [c moveToCheckTrustedState];
}

- (void)registerForCircleChangedNotifications
{
    if(![OTClique platformSupportsSOS]) {
        return;
    }
    
    __weak __typeof(self) weakSelf = self;

    // If we're not in the tests, go ahead and register for a notification
    if(!SecCKKSTestsEnabled()) {
        int token = NOTIFY_TOKEN_INVALID;
        notify_register_dispatch(kSOSCCCircleChangedNotification, &token, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^(int t) {
            secnotice("octagon", "circle changed notification called, checking trust state");
            [weakSelf moveToCheckTrustedStateForArguments:[[OTControlArguments alloc] init]];
        });
    }
}

#if TARGET_OS_WATCH
- (void)registerForNRDeviceNotifications
{
    __weak __typeof(self) weakSelf = self;

    // If we're not in the tests, go ahead and register for a notification
    if(!SecCKKSTestsEnabled()) {
        int token = NOTIFY_TOKEN_INVALID;
        // NRPairedDeviceRegistryDeviceDidPairDarwinNotification
        notify_register_dispatch("com.apple.nanoregistry.devicedidpair", &token, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^(int t) {
            secnotice("octagon", "device paired, checking trust state");
            [weakSelf moveToCheckTrustedStateForArguments:[[OTControlArguments alloc] init]];
        });
    }
}
#endif /* TARGET_OS_WATCH */

+ (instancetype _Nullable)manager {
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
                } else if (manager == nil) {
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

- (void)appleAccountSignedIn:(OTControlArguments*)arguments
                       reply:(void (^)(NSError * _Nullable signedInError))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* context = [self contextForClientRPC:arguments
                                                       error:&clientError];
    if(context == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a signin RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityAccountAvailable];

    NSError *error = nil;

    secnotice("octagon","signing in %@ for altDSID: %@", context, arguments.altDSID);
    [context accountAvailable:arguments.altDSID error:&error];

    [tracker stopWithEvent:OctagonEventSignIn result:error];

    reply(error);
}

- (void)appleAccountSignedOut:(OTControlArguments*)arguments
                        reply:(void (^)(NSError * _Nullable error))reply
{
    // At this point in time, the account no longer exists. So, our normal approach of looking it up will fail the RPC.
    // Hack around this by looking up any context with the matching altDSID.

    if(arguments.altDSID == nil) {
        secnotice("octagon-account", "rejecting a signout RPC due to missing altDSID: %@", arguments);
        reply([NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNotSupported description:@"Must provide an altDSID to sign out"]);
        return;
    }

    secnotice("octagon", "signing out of octagon trust: %@", arguments);

    __block BOOL foundContext = NO;

    dispatch_sync(self.queue, ^{
        // We want to sign out of every context with this altDSID, while ignoring the persona.
        for(OTCuttlefishContext* context in self.contexts.allValues) {
            if([arguments.altDSID isEqualToString:context.activeAccount.altDSID]) {
                secnotice("octagon", "signing out of octagon trust for context: %@", context);
                foundContext = YES;

                SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityAccountNotAvailable];
                NSError* error = nil;
                [context accountNoLongerAvailable:&error];
                if(error) {
                    secnotice("octagon", "signing out failed: %@", error);
                }
                [tracker stopWithEvent:OctagonEventSignOut result:error];
            }
        }
    });

    if(!foundContext) {
        secnotice("octagon", "Failed to find a context to sign out.");
        reply([NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNoSuchContext description:@"Couldn't find a context with this altDSID"]);
        return;
    }

    reply(nil);
}

- (void)notifyIDMSTrustLevelChangeForAltDSID:(OTControlArguments*)arguments
                                       reply:(void (^)(NSError * _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* context = [self contextForClientRPC:arguments
                                                       error:&clientError];
    if(context == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a IDMS trust level change RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    NSError *error = nil;
    secnotice("octagon","received a notification of IDMS trust level change in %@", context);
    [context idmsTrustLevelChanged:&error];

    reply(error);
}

////
// MARK: OTCTL tool routines
////

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
        OTCuttlefishContext* ctx = self.contexts[key];
        if(ctx) {
            [ctx.stateMachine haltOperation];
            [ctx.ckks halt];
        }
        self.contexts[key] = nil;
    });
}


// Moved this from CKKSViewManager

- (CKKSKeychainView* _Nullable)ckksAccountSyncForContainer:(NSString*_Nullable)containerName
                                                 contextID:(NSString*)contextID
                                           possibleAccount:(TPSpecificUser* _Nullable)possibleAccount
{
    OTCuttlefishContext* otc = [self contextForContainerName:containerName
                                                   contextID:contextID
                                             possibleAccount:possibleAccount];
    if(otc == nil) {
        secnotice("octagon", "No context for container/contextID");
    }
    return otc.ckks;
}

- (OTCuttlefishContext* _Nullable)restartCKKSAccountSyncWithoutSettingPolicyForContext:(OTCuttlefishContext*)cuttlefishContext
{        
    CKKSKeychainView* view = cuttlefishContext.ckks;
    NSSet<NSString*>* viewAllowList = view.viewAllowList;
    NSString *containerName = cuttlefishContext.containerName;
    NSString *contextID = cuttlefishContext.contextID;
    TPSpecificUser* account = view.operationDependencies.activeAccount;
    
    [self removeContextForContainerName:containerName
                              contextID:contextID];
    
     
    OTCuttlefishContext* newctx = [self contextForContainerName:containerName
                                                      contextID:contextID
                                                possibleAccount:account
                                                createIfMissing:YES
                                                     sosAdapter:self.sosAdapter
                                                accountsAdapter:self.accountsAdapter
                                                 authKitAdapter:self.authKitAdapter
                                            tooManyPeersAdapter:self.tooManyPeersAdapter
                                               lockStateTracker:self.lockStateTracker
                                       deviceInformationAdapter:self.deviceInformationAdapter];
    
    if(viewAllowList) {
        [newctx.ckks setSyncingViewsAllowList:viewAllowList];
    }
    return newctx;
}

- (CKKSKeychainView* _Nullable)restartCKKSAccountSyncWithoutSettingPolicy:(CKKSKeychainView*)view
{
    __block OTCuttlefishContext* cuttlefishContext = nil;

    dispatch_sync(self.queue, ^{
        for(OTCuttlefishContext* otc in self.contexts.allValues) {
            if([otc.ckks.container isEqual:view.container] && [otc.ckks.operationDependencies.contextID isEqualToString:view.operationDependencies.contextID]) {
                cuttlefishContext = otc;
                break;
            }
        }
    });

    if(cuttlefishContext == nil) {
        ckksnotice_global("ckkstests", "Could not find a parent OTCuttlefishContext for view: %@", view);
        return nil;
    }
    
    OTCuttlefishContext* newctx = [self restartCKKSAccountSyncWithoutSettingPolicyForContext: cuttlefishContext];
    return newctx.ckks;
}

- (void)haltAll
{
    [self allContextsHalt];
}

- (void)dropAllActors
{
    [self clearAllContexts];
}

- (void)cancelPendingOperations {
    [self.savedTLKNotifier cancel];
}

- (OTCuttlefishContext*)contextForContainerName:(NSString* _Nullable)containerName
                                      contextID:(NSString*)contextID
                                     sosAdapter:(id<OTSOSAdapter>)sosAdapter
                                accountsAdapter:(id<OTAccountsAdapter>)accountsAdapter
                                 authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                            tooManyPeersAdapter:(id<OTTooManyPeersAdapter>)tooManyPeersAdapter
                               lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                       deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter
{
    return [self contextForContainerName:containerName
                               contextID:contextID
                         possibleAccount:nil
                         createIfMissing:YES
                              sosAdapter:sosAdapter
                         accountsAdapter:accountsAdapter
                          authKitAdapter:authKitAdapter
                     tooManyPeersAdapter:tooManyPeersAdapter
                        lockStateTracker:lockStateTracker
                deviceInformationAdapter:deviceInformationAdapter];
}

- (OTCuttlefishContext*)contextForContainerName:(NSString* _Nullable)containerName
                                      contextID:(NSString*)contextID
                                possibleAccount:(TPSpecificUser* _Nullable)possibleAccount
                                createIfMissing:(BOOL)createIfMissing
                                     sosAdapter:(id<OTSOSAdapter>)sosAdapter
                                accountsAdapter:(id<OTAccountsAdapter>)accountsAdapter
                                 authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                            tooManyPeersAdapter:(id<OTTooManyPeersAdapter>)tooManyPeersAdapter
                               lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                       deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter
{
    __block OTCuttlefishContext* context = nil;

    if(containerName == nil) {
        containerName = SecCKKSContainerName;
    }

    dispatch_sync(self.queue, ^{
        NSString* key = [NSString stringWithFormat:@"%@-%@", containerName, contextID];
        context = self.contexts[key];

        if(context == nil && createIfMissing) {
            CKKSAccountStateTracker* accountStateTracker = nil;
            CKContainer* cloudKitContainer = nil;
            if (possibleAccount) {
                cloudKitContainer = [possibleAccount makeCKContainer];
                accountStateTracker = [[CKKSAccountStateTracker alloc] init:cloudKitContainer nsnotificationCenterClass:self.cloudKitClassDependencies.nsnotificationCenterClass];
            } else {
                accountStateTracker = self.accountStateTracker;
                cloudKitContainer = self.cloudKitContainer;
            }
            
            CKKSKeychainView* ckks = nil;
            if((OctagonSupportsPersonaMultiuser() && !SecCKKSTestsEnabled()) || ([containerName isEqualToString:SecCKKSContainerName] && [contextID isEqualToString:OTDefaultContext])) {
                NSString* ckksContextID = [contextID isEqualToString:OTDefaultContext] ? CKKSDefaultContextID : contextID;
                ckks = [[CKKSKeychainView alloc] initWithContainer:cloudKitContainer
                                                         contextID:ckksContextID
                                                     activeAccount:possibleAccount
                                                    accountTracker:accountStateTracker
                                                  lockStateTracker:self.lockStateTracker
                                               reachabilityTracker:self.reachabilityTracker
                                                  savedTLKNotifier:self.savedTLKNotifier
                                         cloudKitClassDependencies:self.cloudKitClassDependencies
                                                    personaAdapter:self.personaAdapter];
            }
            
            context = [[OTCuttlefishContext alloc] initWithContainerName:containerName
                                                               contextID:contextID
                                                           activeAccount:possibleAccount
                                                              cuttlefish:self.cuttlefishXPCConnection
                                                         ckksAccountSync:ckks
                                                              sosAdapter:sosAdapter
                                                         accountsAdapter:accountsAdapter
                                                          authKitAdapter:authKitAdapter
                                                          personaAdapter:self.personaAdapter
                                                     tooManyPeersAdapter:tooManyPeersAdapter
                                                        lockStateTracker:lockStateTracker
                                                     reachabilityTracker:self.reachabilityTracker
                                                     accountStateTracker:accountStateTracker
                                                deviceInformationAdapter:deviceInformationAdapter
                                                      apsConnectionClass:self.apsConnectionClass
                                                      escrowRequestClass:self.escrowRequestClass
                                                           notifierClass:self.notifierClass
                                                                    cdpd:self.cdpd];
            self.contexts[key] = context;
        }
        
    });
    
    return context;
}

- (OTCuttlefishContext*)contextForContainerName:(NSString* _Nullable)containerName
                                      contextID:(NSString*)contextID
                                possibleAccount:(TPSpecificUser* _Nullable)possibleAccount
{
    return [self contextForContainerName:containerName
                               contextID:contextID
                         possibleAccount:possibleAccount
                         createIfMissing:YES
                              sosAdapter:self.sosAdapter
                         accountsAdapter:self.accountsAdapter
                          authKitAdapter:self.authKitAdapter
                     tooManyPeersAdapter:self.tooManyPeersAdapter
                        lockStateTracker:self.lockStateTracker
                deviceInformationAdapter:self.deviceInformationAdapter];
}

- (OTCuttlefishContext*)contextForContainerName:(NSString* _Nullable)containerName
                                      contextID:(NSString*)contextID
{
    return [self contextForContainerName:containerName
                               contextID:contextID
                         possibleAccount:nil];
}


- (OTCuttlefishContext* _Nullable)contextForClientRPC:(OTControlArguments*)arguments
                                                error:(NSError**)error
{
    return [self contextForClientRPC:arguments
                     createIfMissing:YES
                               error:error];
}

- (OTCuttlefishContext* _Nullable)contextForClientRPC:(OTControlArguments*)arguments
                                      createIfMissing:(BOOL)createIfMissing
                                                error:(NSError**)error
{
    BOOL threadIsPrimary = [self.personaAdapter currentThreadIsForPrimaryiCloudAccount];

    BOOL allowNonPrimaryAccounts = OctagonSupportsPersonaMultiuser();
#if !TARGET_OS_TV
    // Trust the flag, but log loudly if the value seems wrong on non-TVs.
    if(allowNonPrimaryAccounts) {
        secwarning("octagon-account: Supporting non-primary accounts on possibly-unsupported platform");
    }
#endif

    if(!allowNonPrimaryAccounts && !threadIsPrimary) {
        secnotice("octagon", "Rejecting client RPC for non-primary persona");
        if(error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorNotSupported
                                  description:@"Octagon APIs do not support non-primary users"];
        }
        return nil;
    }

    return [self contextForClientRPC:arguments
                     createIfMissing:createIfMissing
             allowNonPrimaryAccounts:allowNonPrimaryAccounts
                               error:error];
}

- (OTCuttlefishContext* _Nullable)contextForClientRPCWithActiveAccount:(TPSpecificUser*)activeAccount
                                                       createIfMissing:(BOOL)createIfMissing
                                               allowNonPrimaryAccounts:(BOOL)allowNonPrimaryAccounts
                                                                 error:(NSError**)error
{
    secnotice("octagon-account", "Finding a context for user: %@", activeAccount);
    OTCuttlefishContext* context = [self contextForContainerName:activeAccount.cloudkitContainerName
                                                       contextID:activeAccount.octagonContextID
                                                 possibleAccount:activeAccount
                                                 createIfMissing:createIfMissing
                                                      sosAdapter:self.sosAdapter
                                                 accountsAdapter:self.accountsAdapter
                                                  authKitAdapter:self.authKitAdapter
                                             tooManyPeersAdapter:self.tooManyPeersAdapter
                                                lockStateTracker:self.lockStateTracker
                                        deviceInformationAdapter:self.deviceInformationAdapter];
    if(!context && error) {
        // The caller didn't want us to create this context. Return them an error.
        *error = [NSError errorWithDomain:OctagonErrorDomain
                                     code:OctagonErrorNoSuchContext
                              description:@"Context does not exist"];
    }

    // If the TPSpecificUser configured on the context does not match the activeAccount, complain loudly!
    if(context.activeAccount != nil && ![context.activeAccount isEqual:activeAccount]) {
        secnotice("octagon-account", "Context for user(%@) is for user(%@) instead", activeAccount, context.activeAccount);
    }

    return context;
}

- (OTCuttlefishContext* _Nullable)contextForClientRPC:(OTControlArguments*)arguments
                                      createIfMissing:(BOOL)createIfMissing
                              allowNonPrimaryAccounts:(BOOL)allowNonPrimaryAccounts
                                                error:(NSError**)error
{
    BOOL threadPersonaIsPrimary = [self.personaAdapter currentThreadIsForPrimaryiCloudAccount];
    
#if TARGET_OS_TV
    bool isHomePod = (MGGetSInt32Answer(kMGQDeviceClassNumber, MGDeviceClassInvalid) == MGDeviceClassAudioAccessory);
    
    if(isHomePod) {
        if(!threadPersonaIsPrimary) {
            secnotice("octagon-account", "Rejecting non-primary request on HomePod");
            if(error) {
                *error = [NSError errorWithDomain:OctagonErrorDomain
                                             code:OctagonErrorInvalidPersona
                                      description:@"Non-primary personas not supported on this platform"];
            }
            return nil;
        }
        
        return [self contextForContainerName:arguments.containerName
                                   contextID:arguments.contextID];
    }
#endif /* TARGET_OS_TV */
    
    NSError* accountLookupError = nil;
    TPSpecificUser* specificAccount = [self.accountsAdapter findAccountForCurrentThread:self.personaAdapter
                                                                        optionalAltDSID:arguments.altDSID
                                                                  cloudkitContainerName:arguments.containerName
                                                                       octagonContextID:arguments.contextID
                                                                                  error:&accountLookupError];

    if(specificAccount == nil || accountLookupError != nil) {
        // _if_ the caller didn't ask for a specific altDSID, and _if_ they're on the primary persona asking for the default context,
        // then return what they're asking for, even if there isn't an active account
        if(threadPersonaIsPrimary
           && arguments.altDSID == nil
           && [arguments.containerName isEqualToString:OTCKContainerName]
           && [arguments.contextID isEqualToString:OTDefaultContext]) {
            secnotice("octagon-account", "Cannot find an account matching persona/altDSID, allowing default context return: %@ %@", arguments, accountLookupError);
            return [self contextForContainerName:arguments.containerName
                                       contextID:arguments.contextID];
            
        } else {
            if(error) {
                if (!accountLookupError) {
                    *error = [NSError errorWithDomain:OctagonErrorDomain
                                                 code:OctagonErrorNoAppleAccount
                                          description:@"No altDSID configured"];
                } else {
                    *error = accountLookupError;
                }
                if (*error) {
                    secnotice("octagon-account", "Cannot find an account matching: %@ %@", arguments, *error);
                }
            }
            return nil;
        }
    }

    if(!allowNonPrimaryAccounts && !specificAccount.isPrimaryAccount) {
        secnotice("octagon-account", "Rejecting finding a OTCuttlefishContext for non-primary account (on primary persona)");
        if(error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNotSupported description:@"Octagon APIs do not support non-primary accounts"];
        }
        return nil;
    }

    return [self contextForClientRPCWithActiveAccount:specificAccount
                                      createIfMissing:createIfMissing
                              allowNonPrimaryAccounts:allowNonPrimaryAccounts
                                                error:error];
}

- (CKKSKeychainView* _Nullable)ckksForClientRPC:(OTControlArguments*)arguments
                                createIfMissing:(BOOL)createIfMissing
                        allowNonPrimaryAccounts:(BOOL)allowNonPrimaryAccounts
                                          error:(NSError**)error
{
    NSString* persona = [ self.personaAdapter currentThreadPersonaUniqueString];
    
    secnotice("ckkspersona", "ckksForClientRPC: thread persona is %@", persona);
    
    OTCuttlefishContext* context = [self contextForClientRPC:arguments
                                             createIfMissing:createIfMissing
                                     allowNonPrimaryAccounts:allowNonPrimaryAccounts
                                                       error:error];
    
    if (!context) {
        if (error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorNoSuchContext
                                  description:@"Context does not exist"];
        } else {
            secnotice("ckkspersona", "ckksForClientRPC: no OTCuttlefishContext found for persona %@", persona);
        }
        return nil;
    }
    
    if (!context.ckks) {
        if (error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorNoSuchCKKS
                                  description:@"ckks does not exist"];
        } else {
            secnotice("ckkspersona", "ckksForClientRPC: no CKKSKeychainView found for persona %@", persona);
        }
        return nil;
    }
    
    return context.ckks;
}

- (void)clearAllContexts
{
    if(self.contexts) {
        dispatch_sync(self.queue, ^{
            [self.contexts removeAllObjects];
        });
    }
}

- (void)fetchEgoPeerID:(OTControlArguments*)arguments
                 reply:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a fetchEgoPeerID RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, clientError);
        return;
    }

    secnotice("octagon", "Received a fetch peer ID for arguments (%@)", arguments);
    [cfshContext rpcFetchEgoPeerID:^(NSString * _Nullable peerID,
                                     NSError * _Nullable error) {
        reply(peerID, XPCSanitizeError(error));
    }];
}

- (void)fetchTrustStatus:(OTControlArguments*)arguments
           configuration:(OTOperationConfiguration *)configuration
                   reply:(void (^)(CliqueStatus status,
                                   NSString* _Nullable peerID,
                                   NSNumber * _Nullable numberOfPeersInOctagon,
                                   BOOL isExcluded,
                                   NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a fetchTrustStatus RPC for arguments (%@): %@", arguments, clientError);
        reply(CliqueStatusError, nil, nil, NO, clientError);
        return;
    }

    secnotice("octagon", "Received a trust status for arguments (%@)", arguments);
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

- (void)fetchCliqueStatus:(OTControlArguments*)arguments
            configuration:(OTOperationConfiguration *)configuration
                    reply:(void (^)(CliqueStatus cliqueStatus, NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* context = [self contextForClientRPC:arguments
                                             createIfMissing:NO
                                                       error:&clientError];
    if(context == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a fetchCliqueStatus RPC for arguments (%@): %@", arguments, clientError);
        reply(CliqueStatusError, clientError);
        return;
    }

    if(configuration == nil) {
        configuration = [[OTOperationConfiguration alloc] init];
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

- (void)status:(OTControlArguments*)arguments
         reply:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply
{

    NSError* clientError = nil;
    OTCuttlefishContext* context = [self contextForClientRPC:arguments
                                             createIfMissing:NO
                                                       error:&clientError];
    if(context == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a status RPC for arguments(%@): %@", arguments, clientError);
        reply(nil, clientError);
        return;
    }

    secnotice("octagon", "Received a status RPC for arguments (%@): %@", arguments, context);
    [context rpcStatus:reply];
}

- (void)startOctagonStateMachine:(OTControlArguments*)arguments
                           reply:(void (^)(NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                       error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a startOctagonStateMachine RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon", "Received a start-state-machine RPC for arguments (%@)", arguments);

    [cfshContext startOctagonStateMachine];
    reply(nil);
}


- (void)resetAndEstablish:(OTControlArguments*)arguments
              resetReason:(CuttlefishResetReason)resetReason
                    reply:(void (^)(NSError * _Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                       error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a resetAndEstablish RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityResetAndEstablish];

    [cfshContext startOctagonStateMachine];
    [cfshContext rpcResetAndEstablish:resetReason reply:^(NSError* resetAndEstablishError) {
        [tracker stopWithEvent:OctagonEventResetAndEstablish result:resetAndEstablishError];
        reply(resetAndEstablishError);
    }];
}

- (void)establish:(OTControlArguments*)arguments
            reply:(void (^)(NSError * _Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a establish RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityEstablish];

    [cfshContext startOctagonStateMachine];
    [cfshContext rpcEstablish:arguments.altDSID reply:^(NSError* establishError) {
        [tracker stopWithEvent:OctagonEventEstablish result:establishError];
        reply(establishError);
    }];
}

- (void)leaveClique:(OTControlArguments*)arguments
              reply:(void (^)(NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a leaveClique RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityLeaveClique];

    [cfshContext startOctagonStateMachine];
    [cfshContext rpcLeaveClique:^(NSError* leaveError) {
        [tracker stopWithEvent:OctagonEventLeaveClique result:leaveError];
        reply(leaveError);
    }];
}

- (void)removeFriendsInClique:(OTControlArguments*)arguments
                      peerIDs:(NSArray<NSString*>*)peerIDs
                        reply:(void (^)(NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a removeFriendsInClique RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityRemoveFriendsInClique];

    [cfshContext startOctagonStateMachine];
    [cfshContext rpcRemoveFriendsInClique:peerIDs reply:^(NSError* removeFriendsError) {
        [tracker stopWithEvent:OctagonEventRemoveFriendsInClique result:removeFriendsError];
        reply(removeFriendsError);
    }];
}

- (void)peerDeviceNamesByPeerID:(OTControlArguments*)arguments
                          reply:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a peerDeviceNamesByPeerID RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, clientError);
        return;
    }

    [cfshContext rpcFetchDeviceNamesByPeerID:reply];
}

- (void)fetchAllViableBottles:(OTControlArguments*)arguments
                        reply:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs, NSArray<NSString*>* _Nullable sortedPartialBottleIDs, NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a fetchAllViableBottles RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, nil, clientError);
        return;
    }

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityFetchAllViableBottles];

    [cfshContext rpcFetchAllViableBottles:^(NSArray<NSString *> * _Nullable sortedBottleIDs,
                                            NSArray<NSString *> * _Nullable sortedPartialEscrowRecordIDs,
                                            NSError * _Nullable error) {
        [tracker stopWithEvent:OctagonEventFetchAllBottles result:error];
        reply(sortedBottleIDs, sortedPartialEscrowRecordIDs, error);
    }];
}

- (void)fetchEscrowContents:(OTControlArguments*)arguments
                      reply:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a fetchEscrowContents RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, nil, nil, clientError);
        return;
    }

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityFetchEscrowContents];

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
///
- (void)rpcPrepareIdentityAsApplicantWithArguments:(OTControlArguments*)arguments
                                     configuration:(nonnull OTJoiningConfiguration *)config
                                             reply:(void (^)(NSString * _Nullable peerID,
                                                             NSData * _Nullable permanentInfo,
                                                             NSData * _Nullable permanentInfoSig,
                                                             NSData * _Nullable stableInfo,
                                                             NSData * _Nullable stableInfoSig,
                                                             NSError * _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a rpcPrepareIdentityAsApplicant RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, nil, nil, nil, nil, clientError);
        return;
    }

    [cfshContext handlePairingRestart:config];
    [cfshContext startOctagonStateMachine];
    
    OctagonSignpost prepareSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelInitiatorPrepare);
    __block bool subTaskSuccess = false;

    [cfshContext rpcPrepareIdentityAsApplicantWithConfiguration:config
                                                          epoch:config.epoch
                                                          reply:^(NSString * _Nullable peerID,
                                                                  NSData * _Nullable permanentInfo,
                                                                  NSData * _Nullable permanentInfoSig,
                                                                  NSData * _Nullable stableInfo,
                                                                  NSData * _Nullable stableInfoSig,
                                                                  NSError * _Nullable error) {
        if (error == nil) {
            subTaskSuccess = true;
        }
        OctagonSignpostEnd(prepareSignPost, OctagonSignpostNamePairingChannelInitiatorPrepare, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorPrepare), (int)subTaskSuccess);
        
        reply(peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error);
    }];
}


- (void)rpcJoinWithArguments:(OTControlArguments*)arguments
               configuration:(nonnull OTJoiningConfiguration *)config
                   vouchData:(nonnull NSData *)vouchData
                    vouchSig:(nonnull NSData *)vouchSig
                       reply:(nonnull void (^)(NSError * _Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a rpcJoinWithArguments RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }
    [cfshContext handlePairingRestart:config];
    [cfshContext startOctagonStateMachine];
   
    OctagonSignpost joinSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelInitiatorJoinOctagon);
    __block bool subTaskSuccess = false;
    [cfshContext rpcJoin:vouchData vouchSig:vouchSig reply:^(NSError * _Nullable error) {
        if (error == nil) {
            [cfshContext clearPairingUUID];
            subTaskSuccess = true;
        }
        OctagonSignpostEnd(joinSignPost, OctagonSignpostNamePairingChannelInitiatorJoinOctagon, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelInitiatorJoinOctagon), (int)subTaskSuccess);
        reply(error);
    }];
}


////
// MARK: Pairing Routines as Acceptor
////

- (void)rpcEpochWithArguments:(OTControlArguments*)arguments
                configuration:(nonnull OTJoiningConfiguration *)config
                        reply:(nonnull void (^)(uint64_t, NSError * _Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* acceptorCfshContext = [self contextForClientRPC:arguments
                                                                   error:&clientError];
    if(acceptorCfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a rpcEpoch RPC for arguments (%@): %@", arguments, clientError);
        reply(0, clientError);
        return;
    }

    [acceptorCfshContext startOctagonStateMachine];

    // Let's assume that the new device's machine ID has made it to the IDMS list by now, and let's refresh our idea of that list
    [acceptorCfshContext requestTrustedDeviceListRefresh];

    OTClientStateMachine *clientStateMachine = [self clientStateMachineForContainerName:acceptorCfshContext.containerName contextID:acceptorCfshContext.contextID clientName:config.pairingUUID];

    [clientStateMachine startOctagonStateMachine];

    
    OctagonSignpost epochSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelAcceptorEpoch);
    __block bool subTaskSuccess = false;
    [clientStateMachine rpcEpoch:acceptorCfshContext reply:^(uint64_t epoch, NSError * _Nullable error) {
        if (error == nil) {
            subTaskSuccess = true;
        }
        OctagonSignpostEnd(epochSignPost, OctagonSignpostNamePairingChannelAcceptorEpoch, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorEpoch), (int)subTaskSuccess);
        reply(epoch, error);
    }];
}

- (void)rpcVoucherWithArguments:(OTControlArguments*)arguments
                  configuration:(nonnull OTJoiningConfiguration *)config
                         peerID:(NSString*)peerID
                  permanentInfo:(NSData *)permanentInfo
               permanentInfoSig:(NSData *)permanentInfoSig
                     stableInfo:(NSData *)stableInfo
                  stableInfoSig:(NSData *)stableInfoSig
                          reply:(void (^)(NSData* _Nullable voucher, NSData* _Nullable voucherSig, NSError * _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* acceptorCfshContext = [self contextForClientRPC:arguments
                                                                   error:&clientError];
    if(acceptorCfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a rpcVoucher RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, nil, clientError);
        return;
    }

    [acceptorCfshContext startOctagonStateMachine];

    // Let's assume that the new device's machine ID has made it to the IDMS list by now, and let's refresh our idea of that list
    [acceptorCfshContext requestTrustedDeviceListRefresh];
    OTClientStateMachine *clientStateMachine = [self clientStateMachineForContainerName:acceptorCfshContext.containerName contextID:acceptorCfshContext.contextID clientName:config.pairingUUID];

    OctagonSignpost voucherSignPost = OctagonSignpostBegin(OctagonSignpostNamePairingChannelAcceptorVoucher);
    __block bool subTaskSuccess = false;
    [clientStateMachine rpcVoucher:acceptorCfshContext peerID:peerID permanentInfo:permanentInfo permanentInfoSig:permanentInfoSig stableInfo:stableInfo stableInfoSig:stableInfoSig reply:^(NSData *voucher, NSData *voucherSig, NSError *error) {
        if (error == nil) {
            subTaskSuccess = true;
        }
        OctagonSignpostEnd(voucherSignPost, OctagonSignpostNamePairingChannelAcceptorVoucher, OctagonSignpostNumber1(OctagonSignpostNamePairingChannelAcceptorVoucher), (int)subTaskSuccess);
        reply(voucher, voucherSig, error);
    }];
}

- (void)restoreFromBottle:(OTControlArguments*)arguments
                  entropy:(NSData *)entropy
                 bottleID:(NSString *)bottleID
                    reply:(void (^)(NSError * _Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                                   error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a restoreFromBottle RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityBottledPeerRestore];

    secnotice("octagon", "restore via bottle invoked for arguments (%@)", arguments);

    [cfshContext startOctagonStateMachine];

    [cfshContext joinWithBottle:bottleID entropy:entropy bottleSalt:arguments.altDSID reply:^(NSError *error) {
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

        values[OctagonAnalyticsStateMachineState] = [OTStates OctagonStateMap][cuttlefishContext.stateMachine.currentState];

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

                NSError* egoPeerStatusError = nil;
                TrustedPeersHelperEgoPeerStatus* egoPeerStatus = [cuttlefishContext egoPeerStatus:&egoPeerStatusError];

                if(egoPeerStatus && egoPeerStatusError == nil) {
                    NSNumber* numberOfPeersWithMachineID = egoPeerStatus.peerCountsByMachineID[machineID] ?: @(0);
                    secnotice("octagon-metrics", "Number of peers with machineID (%@): %@", machineID, numberOfPeersWithMachineID);
                    values[OctagonAnalyticsPeersWithMID] = numberOfPeersWithMachineID;

                    values[OctagonAnalyticsEgoMIDMatchesCurrentMID] = @([machineID isEqualToString:egoPeerStatus.egoPeerMachineID]);
                    secnotice("octagon-metrics", "MID match (current vs Octagon peer): %@, %@, %@", values[OctagonAnalyticsEgoMIDMatchesCurrentMID], machineID, egoPeerStatus.egoPeerMachineID);

                    size_t totalPeers = 0;
                    for(NSNumber* peers in egoPeerStatus.peerCountsByMachineID.allValues) {
                        totalPeers += [peers longValue];
                    }

                    size_t totalViablePeers = 0;
                    for(NSNumber* peers in egoPeerStatus.viablePeerCountsByModelID.allValues) {
                        totalViablePeers += [peers longValue];
                    }
                    secnotice("octagon-metrics", "Peers: %zu, viable peers %zu", totalPeers, totalViablePeers);
                    values[OctagonAnalyticsTotalPeers] = @(totalPeers);
                    values[OctagonAnalyticsTotalViablePeers] = @(totalViablePeers);

                } else {
                    secnotice("octagon-analytics", "Error fetching how many peers have our MID: %@", egoPeerStatusError);
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
            ACAccountStore *store = [ACAccountStore defaultStore];
            ACAccount* primaryAccount = [store aa_primaryAppleAccount];
            if(primaryAccount) {
                values[OctagonAnalyticsKVSProvisioned] = @([primaryAccount isProvisionedForDataclass:ACAccountDataclassKeyValue]);
                values[OctagonAnalyticsKVSEnabled] = @([primaryAccount isEnabledForDataclass:ACAccountDataclassKeyValue]);
                values[OctagonAnalyticsKeychainSyncProvisioned] = @([primaryAccount isProvisionedForDataclass:ACAccountDataclassKeychainSync]);
                values[OctagonAnalyticsKeychainSyncEnabled] = @([primaryAccount isEnabledForDataclass:ACAccountDataclassKeychainSync]);
                values[OctagonAnalyticsCloudKitProvisioned] = @([primaryAccount isProvisionedForDataclass:ACAccountDataclassCKDatabaseService]);
                values[OctagonAnalyticsCloudKitEnabled] = @([primaryAccount isEnabledForDataclass:ACAccountDataclassCKDatabaseService]);

                NSString *altDSID = primaryAccount.aa_altDSID;
                if(altDSID) {
                    values[OctagonAnalyticsSecureBackupTermsAccepted] = @([getSecureBackupClass() getAcceptedTermsForAltDSID:altDSID withError:nil] != nil);
                }
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
#if OCTAGON_PLATFORM_SUPPORTS_RK_CFU
                         OTCliqueCDPContextTypeRecoveryKeyGenerate,
                         OTCliqueCDPContextTypeRecoveryKeyNew,
#endif
                         OTCliqueCDPContextTypeUpdatePasscode,
                         OTCliqueCDPContextTypeConfirmPasscodeCyrus,
        ];
    });
    return contextTypes;
}

////
// MARK: Recovery Key
////

- (void)createRecoveryKey:(OTControlArguments*)arguments
              recoveryKey:(NSString *)recoveryKey
                    reply:(void (^)( NSError *error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a createRecoveryKey RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon", "Setting recovery key for arguments (%@)", arguments);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivitySetRecoveryKey];

    if (!self.sosEnabledForPlatform) {
        secnotice("octagon-recovery", "Device does not participate in SOS; cannot enroll recovery key in Octagon");
        NSError* notFullPeerError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorOperationUnavailableOnLimitedPeer userInfo:@{NSLocalizedDescriptionKey : @"Device is considered a limited peer, cannot enroll recovery key in Octagon"}];
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

        validateErrorWrapper = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecoveryKeyMalformed userInfo:userInfo];

        secerror("recovery failed validation with error:%@", validateError);

        [tracker stopWithEvent:OctagonEventSetRecoveryKeyValidationFailed result:validateErrorWrapper];
        reply(validateErrorWrapper);
        return;
    }

    [cfshContext startOctagonStateMachine];

    [cfshContext rpcSetRecoveryKey:recoveryKey reply:^(NSError * _Nullable error) {
        [tracker stopWithEvent:OctagonEventRecoveryKey result:error];
        reply(error);
    }];
}

- (void)joinWithRecoveryKey:(OTControlArguments*)arguments
                recoveryKey:(NSString*)recoveryKey
                      reply:(void (^)(NSError * _Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a joinWithRecoveryKey RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon", "join with recovery key invoked for arguments (%@)", arguments);

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

        validateErrorWrapper = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecoveryKeyMalformed userInfo:userInfo];

        secerror("recovery failed validation with error:%@", validateError);

        [tracker stopWithEvent:OctagonEventJoinRecoveryKeyValidationFailed result:validateErrorWrapper];
        reply(validateErrorWrapper);
        return;
    }

    [cfshContext startOctagonStateMachine];

    [cfshContext joinWithRecoveryKey:recoveryKey reply:^(NSError *_Nullable error) {
        if (error) {
            if ((error.code == TrustedPeersHelperErrorCodeNotEnrolled || error.code == TrustedPeersHelperErrorCodeUntrustedRecoveryKeys)
                && [error.domain isEqualToString:TrustedPeersHelperErrorDomain]) {
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
                        [self createRecoveryKey:arguments recoveryKey:recoveryKey reply:^(NSError *enrollError) {
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

                return;
            } else {
                secerror("octagon, join with recovery key failed: %@", error);
                [tracker stopWithEvent:OctagonEventJoinRecoveryKeyFailed result:error];
                reply(error);
                return;
            }
        } else {
            secnotice("octagon", "join with recovery key succeeded");
            [tracker stopWithEvent:OctagonEventJoinRecoveryKey result:error];
            reply(nil);
            return;
        }
    }];
}

////
// MARK: Custodian Recovery Key
////

- (void)createCustodianRecoveryKey:(OTControlArguments*)arguments
                              uuid:(NSUUID *_Nullable)uuid
                             reply:(void (^)(OTCustodianRecoveryKey *_Nullable crk, NSError *_Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a createCustodianRecoveryKey RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, clientError);
        return;
    }

    secnotice("octagon-custodian-recovery", "Creating Custodian Recovery Key for arguments (%@)", arguments);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityCreateCustodianRecoveryKey];

    if (!self.sosEnabledForPlatform) {
        secnotice("octagon-custodian-recovery", "Device does not participate in SOS; cannot enroll recovery key in Octagon");
        NSError* notFullPeerError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorOperationUnavailableOnLimitedPeer userInfo:@{NSLocalizedDescriptionKey : @"Device is considered a limited peer, cannot enroll recovery key in Octagon"}];
        [tracker stopWithEvent:OctagonEventCustodianRecoveryKey result:notFullPeerError];
        reply(nil, notFullPeerError);
        return;
    }

    [cfshContext startOctagonStateMachine];

    [cfshContext rpcCreateCustodianRecoveryKeyWithUUID:uuid reply:^(OTCustodianRecoveryKey *_Nullable crk, NSError *_Nullable error) {
        [tracker stopWithEvent:OctagonEventCustodianRecoveryKey result:error];
        reply(crk, error);
    }];
}

- (void)joinWithCustodianRecoveryKey:(OTControlArguments*)arguments
                custodianRecoveryKey:(OTCustodianRecoveryKey *)crk
                               reply:(void (^)(NSError *_Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a joinWithCustodianRecoveryKey RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon", "join with custodian recovery key %@ invoked for container: %@, context: %@", crk.uuid, arguments.containerName, arguments.contextID);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityJoinWithCustodianRecoveryKey];

    [cfshContext startOctagonStateMachine];

    [cfshContext joinWithCustodianRecoveryKey:crk reply:^(NSError *error) {
            if (error) {
                secerror("octagon, join with custodian recovery key failed: %@", error);
            } else {
                secnotice("octagon", "join with custodian recovery key succeeded");
            }
            [tracker stopWithEvent:OctagonEventCustodianRecoveryKey result:error];
            reply(error);
        }];
}

- (void)preflightJoinWithCustodianRecoveryKey:(OTControlArguments*)arguments
                         custodianRecoveryKey:(OTCustodianRecoveryKey *)crk
                                        reply:(void (^)(NSError *_Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                       error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a preflightJoinWithCustodianRecoveryKey RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon", "preflight join with custodian recovery key %@ invoked for container: %@, context: %@", crk.uuid, arguments.containerName, arguments.contextID);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityPreflightJoinWithCustodianRecoveryKey];

    [cfshContext startOctagonStateMachine];

    [cfshContext preflightJoinWithCustodianRecoveryKey:crk reply:^(NSError *error) {
            if (error) {
                secerror("octagon, preflight join with custodian recovery key failed: %@", error);
            } else {
                secnotice("octagon", "preflight join with custodian recovery key succeeded");
            }
            [tracker stopWithEvent:OctagonEventPreflightCustodianRecoveryKey result:error];
            reply(error);
        }];
}

- (void)removeCustodianRecoveryKey:(OTControlArguments*)arguments
                              uuid:(NSUUID *)uuid
                             reply:(void (^)(NSError *_Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a removeCustodianRecoveryKey RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon-custodian-recovery", "Remove Custodian Recovery Key %@ for container: %@, context: %@", uuid, arguments.containerName, arguments.contextID);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityRemoveCustodianRecoveryKey];

    if (!self.sosEnabledForPlatform) {
        secnotice("octagon-custodian-recovery", "Device does not participate in SOS; cannot remove recovery key in Octagon");
        NSError* notFullPeerError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorOperationUnavailableOnLimitedPeer userInfo:@{NSLocalizedDescriptionKey : @"Device is considered a limited peer, cannot remove recovery key in Octagon"}];
        [tracker stopWithEvent:OctagonEventCustodianRecoveryKey result:notFullPeerError];
        reply(notFullPeerError);
        return;
    }

    [cfshContext startOctagonStateMachine];

    [cfshContext rpcRemoveCustodianRecoveryKeyWithUUID:uuid reply:^(NSError *_Nullable error) {
        [tracker stopWithEvent:OctagonEventCustodianRecoveryKey result:error];
        reply(error);
    }];
}

////
// MARK: Inheritance Key
////


- (void)createInheritanceKey:(OTControlArguments*)arguments
                        uuid:(NSUUID *_Nullable)uuid
                       reply:(void (^)(OTInheritanceKey *_Nullable crk, NSError *_Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a createInheritanceKey RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, clientError);
        return;
    }

    secnotice("octagon-inheritance", "Creating Inheritance Key for arguments (%@)", arguments);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityCreateInheritanceKey];

    if (!self.sosEnabledForPlatform) {
        secnotice("octagon-inheritance", "Device does not participate in SOS; cannot enroll recovery key in Octagon");
        NSError* notFullPeerError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorOperationUnavailableOnLimitedPeer userInfo:@{NSLocalizedDescriptionKey : @"Device is considered a limited peer, cannot enroll recovery key in Octagon"}];
        [tracker stopWithEvent:OctagonEventInheritanceKey result:notFullPeerError];
        reply(nil, notFullPeerError);
        return;
    }

    [cfshContext startOctagonStateMachine];

    [cfshContext rpcCreateInheritanceKeyWithUUID:uuid reply:^(OTInheritanceKey *_Nullable crk, NSError *_Nullable error) {
        [tracker stopWithEvent:OctagonEventInheritanceKey result:error];
        reply(crk, error);
    }];
}

- (void)generateInheritanceKey:(OTControlArguments*)arguments
                        uuid:(NSUUID *_Nullable)uuid
                       reply:(void (^)(OTInheritanceKey *_Nullable crk, NSError *_Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a generateInheritanceKey RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, clientError);
        return;
    }

    secnotice("octagon-inheritance", "Generating Inheritance Key for arguments (%@)", arguments);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityGenerateInheritanceKey];

    if (!self.sosEnabledForPlatform) {
        secnotice("octagon-inheritance", "Device does not participate in SOS; cannot enroll recovery key in Octagon");
        NSError* notFullPeerError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorOperationUnavailableOnLimitedPeer userInfo:@{NSLocalizedDescriptionKey : @"Device is considered a limited peer, cannot enroll recovery key in Octagon"}];
        [tracker stopWithEvent:OctagonEventInheritanceKey result:notFullPeerError];
        reply(nil, notFullPeerError);
        return;
    }

    [cfshContext startOctagonStateMachine];

    [cfshContext rpcGenerateInheritanceKeyWithUUID:uuid reply:^(OTInheritanceKey *_Nullable crk, NSError *_Nullable error) {
        [tracker stopWithEvent:OctagonEventInheritanceKey result:error];
        reply(crk, error);
    }];
}

- (void)storeInheritanceKey:(OTControlArguments*)arguments
                         ik:(OTInheritanceKey *)ik
                      reply:(void (^)(NSError *_Nullable error)) reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a storeInheritanceKey RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon-inheritance", "Storing Inheritance Key for arguments (%@)", arguments);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityStoreInheritanceKey];

    if (!self.sosEnabledForPlatform) {
        secnotice("octagon-inheritance", "Device does not participate in SOS; cannot enroll recovery key in Octagon");
        NSError* notFullPeerError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorOperationUnavailableOnLimitedPeer userInfo:@{NSLocalizedDescriptionKey : @"Device is considered a limited peer, cannot enroll recovery key in Octagon"}];
        [tracker stopWithEvent:OctagonEventInheritanceKey result:notFullPeerError];
        reply(notFullPeerError);
        return;
    }

    [cfshContext startOctagonStateMachine];

    [cfshContext rpcStoreInheritanceKeyWithIK:ik reply:^(NSError *_Nullable error) {
        [tracker stopWithEvent:OctagonEventInheritanceKey result:error];
        reply(error);
    }];
}

- (void)joinWithInheritanceKey:(OTControlArguments*)arguments
                inheritanceKey:(OTInheritanceKey *)ik
                         reply:(void (^)(NSError *_Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a joinWithInheritanceKey RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon-inheritance", "join with inheritance key %@ invoked for container: %@, context: %@", ik.uuid, arguments.containerName, arguments.contextID);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityJoinWithInheritanceKey];

    [cfshContext startOctagonStateMachine];

    [cfshContext joinWithInheritanceKey:ik reply:^(NSError *error) {
            if (error) {
                secerror("octagon, join with inheritance key failed: %@", error);
            } else {
                secnotice("octagon-inheritance", "join with inheritance key succeeded");
            }
            [tracker stopWithEvent:OctagonEventInheritanceKey result:error];
            reply(error);
        }];
}

- (void)preflightJoinWithInheritanceKey:(OTControlArguments*)arguments
                         inheritanceKey:(OTInheritanceKey *)ik
                                  reply:(void (^)(NSError *_Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                       error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a preflightJoinWithInheritanceKey RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon-inheritance", "preflight join with inheritance key %@ invoked for container: %@, context: %@", ik.uuid, arguments.containerName, arguments.contextID);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityPreflightJoinWithInheritanceKey];

    [cfshContext startOctagonStateMachine];

    [cfshContext preflightJoinWithInheritanceKey:ik reply:^(NSError *error) {
            if (error) {
                secerror("octagon, preflight join with inheritance key failed: %@", error);
            } else {
                secnotice("octagon-inheritance", "preflight join with inheritance key succeeded");
            }
            [tracker stopWithEvent:OctagonEventPreflightInheritanceKey result:error];
            reply(error);
        }];
}

- (void)removeInheritanceKey:(OTControlArguments*)arguments
                        uuid:(NSUUID *)uuid
                       reply:(void (^)(NSError *_Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a removeInheritanceKey RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon-inheritance", "Remove Inheritance Key %@ for container: %@, context: %@", uuid, arguments.containerName, arguments.contextID);

    SFAnalyticsActivityTracker *tracker = [[self.loggerClass logger] startLogSystemMetricsForActivityNamed:OctagonActivityRemoveInheritanceKey];

    if (!self.sosEnabledForPlatform) {
        secnotice("octagon-custodian-recovery", "Device does not participate in SOS; cannot remove inheritance key in Octagon");
        NSError* notFullPeerError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorOperationUnavailableOnLimitedPeer userInfo:@{NSLocalizedDescriptionKey : @"Device is considered a limited peer, cannot remove inheritance key in Octagon"}];
        [tracker stopWithEvent:OctagonEventInheritanceKey result:notFullPeerError];
        reply(notFullPeerError);
        return;
    }

    [cfshContext startOctagonStateMachine];

    [cfshContext rpcRemoveInheritanceKeyWithUUID:uuid reply:^(NSError *_Nullable error) {
        [tracker stopWithEvent:OctagonEventInheritanceKey result:error];
        reply(error);
    }];
}

- (void)xpc24HrNotification
{
    secnotice("octagon-health", "Received 24hr xpc notification");

    // rdar://82008582 (aTV miCK: Restart all active Octagon/CKKS state machines on daemon restart)
    [self healthCheck:[[OTControlArguments alloc] init]
skipRateLimitingCheck:NO
                reply:^(NSError * _Nullable error) {
        if(error) {
            secerror("octagon: error attempting to check octagon health: %@", error);
        } else {
            secnotice("octagon", "health check success");
        }
    }];
}

- (void)healthCheck:(OTControlArguments*)arguments
skipRateLimitingCheck:(BOOL)skipRateLimitingCheck
              reply:(void (^)(NSError *_Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a healthCheck RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

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
        [context.ckks halt];
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

        CKKSCondition* escrowCacheWarmup = context.pendingEscrowCacheWarmup;
        if(escrowCacheWarmup != nil) {
            if([escrowCacheWarmup wait:within] != 0) {
                return false;
            }
        }
    }
    return true;
}

- (void)waitForOctagonUpgrade:(OTControlArguments*)arguments
                        reply:(void (^)(NSError* error))reply

{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a waitForOctagonUpgrade RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon-sos", "Attempting wait for octagon upgrade");
    [cfshContext startOctagonStateMachine];

    [cfshContext waitForOctagonUpgrade:^(NSError * _Nonnull error) {
        reply(error);
    }];
}

- (void)waitForPriorityViewKeychainDataRecovery:(OTControlArguments*)arguments
                                          reply:(void (^)(NSError* _Nullable replyError))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a waitForPriorityViewKeychainDataRecovery RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon-ckks", "Attempting wait for priority view processing");
    [cfshContext startOctagonStateMachine];

    [cfshContext rpcWaitForPriorityViewKeychainDataRecovery:^(NSError * _Nonnull error) {
        reply(error);
    }];
}

- (void)postCDPFollowupResult:(OTControlArguments*)arguments
                      success:(BOOL)success
                         type:(OTCliqueCDPContextType)type
                        error:(NSError * _Nullable)error
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

- (void) refetchCKKSPolicy:(OTControlArguments*)arguments
                    reply:(nonnull void (^)(NSError * _Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a refetchCKKSPolicy RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon-ckks", "refetch-ckks-policy");

    [cfshContext startOctagonStateMachine];
    [cfshContext rpcRefetchCKKSPolicy:^(NSError* error) {
        secnotice("octagon-ckks", "refetch-ckks-policy result: %@", error ?: @"no error");
        reply(error);
    }];
}

- (void)getCDPStatus:(OTControlArguments*)arguments
               reply:(nonnull void (^)(OTCDPStatus, NSError * _Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a getCDPStatus RPC for arguments (%@): %@", arguments, clientError);
        reply(OTCDPStatusUnknown, clientError);
        return;
    }

    secnotice("octagon-cdp", "get-cdp-status");

    NSError* error = nil;
    OTCDPStatus status = [cfshContext getCDPStatus:&error];
    reply(status, error);
}


- (void)setCDPEnabled:(OTControlArguments*)arguments
                reply:(nonnull void (^)(NSError * _Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a setCDPEnabled RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon-cdp", "set-cdp-enabled");
    NSError* localError = nil;
    [cfshContext setCDPEnabled:&localError];
    reply(localError);
}

- (void)fetchEscrowRecords:(OTControlArguments*)arguments
                forceFetch:(BOOL)forceFetch
                     reply:(nonnull void (^)(NSArray<NSData *> * _Nullable records,
                                             NSError * _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a fetchEscrowRecords RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, clientError);
        return;
    }

    secnotice("octagon-fetch-escrow-records", "fetching records");

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

- (void)invalidateEscrowCache:(OTControlArguments*)arguments
                        reply:(nonnull void (^)(NSError * _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a invalidateEscrowCache RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    secnotice("octagon-remove-escrow-cache", "beginning removing escrow cache!");
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

- (void)setUserControllableViewsSyncStatus:(OTControlArguments*)arguments
                                   enabled:(BOOL)enabled
                                     reply:(void (^)(BOOL nowSyncing, NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a setUserControllableViewsSyncStatus RPC for arguments (%@): %@", arguments, clientError);
        reply(NO, clientError);
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

- (void)fetchUserControllableViewsSyncStatus:(OTControlArguments*)arguments
                                       reply:(void (^)(BOOL nowSyncing, NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a fetchUserControllableViewsSyncStatus RPC for arguments (%@): %@", arguments, clientError);
        reply(NO, clientError);
        return;
    }

    [cfshContext rpcFetchUserControllableViewsSyncingStatus:^(BOOL areSyncing, NSError * _Nullable error) {
        if(error) {
            secerror("octagon-user-controllable-views: error fetching status: %@", error);
            reply(NO, error);
            return;
        }

        secnotice("octagon-user-controllable-views", "successfully fetched status as: %@", areSyncing ? @"enabled" : @"paused");
        reply(areSyncing, nil);
    }];
}

- (void)resetAccountCDPContents:(OTControlArguments*)arguments
                          reply:(void (^)(NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a resetAccountCDPContents RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    [cfshContext rpcResetAccountCDPContents:^(NSError * _Nullable error) {
        if(error) {
            secerror("octagon-reset-account-cdp-contents: error resetting account cdp contents: %@", error);
            reply(error);
            return;
        }

        secnotice("octagon-reset-account-cdp-contents", "successfully reset account cdp contents");
        reply(nil);
    }];
}

- (void)setLocalSecureElementIdentity:(OTControlArguments*)arguments
                secureElementIdentity:(OTSecureElementPeerIdentity*)secureElementIdentity
                                reply:(void (^)(NSError* _Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a setLocalSecureElementIdentity RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }
    [cfshContext rpcSetLocalSecureElementIdentity:secureElementIdentity reply:reply];
}

- (void)removeLocalSecureElementIdentityPeerID:(OTControlArguments*)arguments
                   secureElementIdentityPeerID:(NSData*)sePeerID
                                         reply:(void (^)(NSError* _Nullable))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a removeLocalSecureElementIdentityPeerID RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    [cfshContext rpcRemoveLocalSecureElementIdentityPeerID:sePeerID reply:reply];
}


- (void)fetchTrustedSecureElementIdentities:(OTControlArguments*)arguments
                                      reply:(void (^)(OTCurrentSecureElementIdentities* _Nullable currentSet,
                                                      NSError* _Nullable replyError))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a fetchTrustedSecureElementIdentities RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, clientError);
        return;
    }

    [cfshContext rpcFetchTrustedSecureElementIdentities:reply];
}


- (void)tlkRecoverabilityForEscrowRecordData:(OTControlArguments*)arguments
                                 recordData:(NSData*)recordData
                                      reply:(void (^)(NSArray<NSString*>* _Nullable views, NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a tlkRecoverabilityForEscrowRecordData RPC for arguments (%@): %@", arguments, clientError);
        reply(nil, clientError);
        return;
    }

    [cfshContext rpcTlkRecoverabilityForEscrowRecordData:recordData reply:reply];
}

- (void)deliverAKDeviceListDelta:(NSDictionary*)notificationDictionary
                           reply:(void (^)(NSError* _Nullable error))reply
{
    [self.authKitAdapter deliverAKDeviceListDeltaMessagePayload:notificationDictionary];
    reply(nil);
}

- (void)setMachineIDOverride:(OTControlArguments*)arguments
                   machineID:(NSString*)machineID
                       reply:(void (^)(NSError* _Nullable replyError))reply
{
    NSError* clientError = nil;
    OTCuttlefishContext* cfshContext = [self contextForClientRPC:arguments
                                                           error:&clientError];
    if(cfshContext == nil || clientError != nil) {
        secnotice("octagon", "Rejecting a setMachineIDOverride RPC for arguments (%@): %@", arguments, clientError);
        reply(clientError);
        return;
    }

    [cfshContext setMachineIDOverride:machineID];
    reply(nil);
}

+ (CKContainer*)makeCKContainer:(NSString*)containerName
{
    CKContainerOptions* containerOptions = [[CKContainerOptions alloc] init];
    containerOptions.bypassPCSEncryption = YES;
    CKContainerID *containerID = [CKContainer containerIDForContainerIdentifier:containerName];
    return [[CKContainer alloc] initWithContainerID:containerID options:containerOptions];
}

@end

#endif
