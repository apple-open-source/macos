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
#if OCTAGON

#import <CoreCDP/CDPAccount.h>
#import <notify.h>
#import <os/feature_private.h>
#import <Security/Security.h>
#import <Security/SecInternalReleasePriv.h>
#import <Security/SecLaunchSequence.h>
#include <Security/SecRandomP.h>
#import <Security/SecXPCHelper.h>
#import <SecurityFoundation/SFKey_Private.h>
#include <sys/sysctl.h>
#import <TrustedPeers/TrustedPeers.h>

#import <TapToRadarKit/TapToRadarKit.h>

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#include "keychain/SecureObjectSync/SOSInternal.h"

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/escrowrequest/EscrowRequestServer.h"
#import "keychain/ot/OTAuthKitAdapter.h"
#import "keychain/ot/OTCheckHealthOperation.h"
#import "keychain/ot/OTPairingVoucherOperation.h"
#import "keychain/ot/OTClique.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTCreateCustodianRecoveryKeyOperation.h"
#import "keychain/ot/OTCreateInheritanceKeyOperation.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTDetermineCDPBitStatusOperation.h"
#import "keychain/ot/OTDetermineCDPCapableAccountStatusOperation.h"
#import "keychain/ot/OTDeviceInformationAdapter.h"
#import "keychain/ot/OTEnsureOctagonKeyConsistency.h"
#import "keychain/ot/OTEstablishOperation.h"
#import "keychain/ot/OTFetchViewsOperation.h"
#import "keychain/ot/OTFindCustodianRecoveryKeyOperation.h"
#import "keychain/ot/OTFollowup.h"
#import "keychain/ot/OTJoinSOSAfterCKKSFetchOperation.h"
#import "keychain/ot/OTJoinWithVoucherOperation.h"
#import "keychain/ot/OTLeaveCliqueOperation.h"
#import "keychain/ot/OTLocalCKKSResetOperation.h"
#import "keychain/ot/OTLocalCuttlefishReset.h"
#import "keychain/ot/OTModifyUserControllableViewStatusOperation.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTPreflightVouchWithCustodianRecoveryKeyOperation.h"
#import "keychain/ot/OTPreloadOctagonKeysOperation.h"
#import "keychain/ot/OTRecreateInheritanceKeyOperation.h"
#import "keychain/ot/OTCreateInheritanceKeyWithClaimTokenAndWrappingKeyOperation.h"
#import "keychain/ot/OTPrepareAndRecoverTLKSharesForInheritancePeerOperation.h"
#import "keychain/ot/OTPrepareOperation.h"
#import "keychain/ot/OTRemoveCustodianRecoveryKeyOperation.h"
#import "keychain/ot/OTRemovePeersOperation.h"
#import "keychain/ot/OTResetCKKSZonesLackingTLKsOperation.h"
#import "keychain/ot/OTResetOperation.h"
#import "keychain/ot/OTSOSAdapter.h"
#import "keychain/ot/OTSOSUpdatePreapprovalsOperation.h"
#import "keychain/ot/OTSOSUpgradeOperation.h"
#import "keychain/ot/OTSetAccountSettingsOperation.h"
#import "keychain/ot/OTSetCDPBitOperation.h"
#import "keychain/ot/OTSetRecoveryKeyOperation.h"
#import "keychain/ot/OTStashAccountSettingsOperation.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ot/OTStoreInheritanceKeyOperation.h"
#import "keychain/ot/OTTooManyPeersAdapter.h"
#import "keychain/ot/OTTriggerEscrowUpdateOperation.h"
#import "keychain/ot/OTUpdateTPHOperation.h"
#import "keychain/ot/OTUpdateTrustedDeviceListOperation.h"
#import "keychain/ot/OTUploadNewCKKSTLKsOperation.h"
#import "keychain/ot/OTVouchWithBottleOperation.h"
#import "keychain/ot/OTVouchWithCustodianRecoveryKeyOperation.h"
#import "keychain/ot/OTVouchWithRecoveryKeyOperation.h"
#import "keychain/ot/OTVouchWithRerollOperation.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OctagonCKKSPeerAdapter.h"
#import "keychain/ot/OctagonCheckTrustStateOperation.h"
#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"
#import "keychain/OctagonTrust/OTCustodianRecoveryKey.h"
#import "keychain/OctagonTrust/OTInheritanceKey.h"
#import "keychain/securityd/SOSCloudCircleServer.h"

#import "utilities/SecFileLocations.h"
#import "utilities/SecTapToRadar.h"

#import "keychain/analytics/SecurityAnalyticsConstants.h"
#import "keychain/analytics/SecurityAnalyticsReporterRTC.h"
#import "keychain/analytics/AAFAnalyticsEvent+Security.h"

#if TARGET_OS_WATCH
#import "keychain/otpaird/OTPairingClient.h"
#endif /* TARGET_OS_WATCH */

NSString* OTCuttlefishContextErrorDomain = @"otcuttlefish";
static dispatch_time_t OctagonStateTransitionDefaultTimeout = 10*NSEC_PER_SEC;
static dispatch_time_t OctagonStateTransitionTimeoutForTests = 20*NSEC_PER_SEC;
static dispatch_time_t OctagonStateTransitionTimeoutForLongOps = 120*NSEC_PER_SEC;
static dispatch_time_t OctagonNFSOneHour = 3600*NSEC_PER_SEC;
static dispatch_time_t OctagonNFSTwoSeconds = 2*NSEC_PER_SEC;

@class CKKSLockStateTracker;

@implementation OTMetricsSessionData

-(instancetype)initWithFlowID:(NSString*)flowID deviceSessionID:(NSString*)deviceSessionID
{
    if (self = [super init]) {
        _flowID = flowID;
        _deviceSessionID = deviceSessionID;
    }
    return self;
}

@end

@interface OTCuttlefishContext () <OTCuttlefishAccountStateHolderNotifier>
{
    NSString* _bottleID;
    NSString* _bottleSalt;
    NSData* _entropy;
    CuttlefishResetReason _resetReason;
    NSString* _Nullable _idmsTargetContext;
    NSString* _Nullable _idmsCuttlefishPassword;
    BOOL _notifyIdMS;
    BOOL _skipRateLimitingCheck;
    BOOL _repair;
    BOOL _reportRateLimitingError;
    TrustedPeersHelperHealthCheckResult* _healthCheckResults;
}

@property SecLaunchSequence* launchSequence;
@property NSOperationQueue* operationQueue;
@property (nonatomic, strong) OTCuttlefishAccountStateHolder *accountMetadataStore;
@property OTFollowup *followupHandler;

@property CKAccountInfo* cloudKitAccountInfo;
@property CKKSCondition *cloudKitAccountStateKnown;

@property CKKSNearFutureScheduler* suggestTLKUploadNotifier;
@property CKKSNearFutureScheduler* requestPolicyCheckNotifier;
@property CKKSNearFutureScheduler* upgradeUserControllableViewsRateLimiter;
@property CKKSNearFutureScheduler* fixupRetryScheduler;

@property CKKSReachabilityTracker* reachabilityTracker;

@property (nullable, nonatomic, strong) NSString* recoveryKey;
@property (nullable, nonatomic, strong) OTCustodianRecoveryKey* custodianRecoveryKey;
@property (nullable, nonatomic, strong) OTInheritanceKey* inheritanceKey;

@property OctagonAPSReceiver* apsReceiver;

// Make writable
@property (nullable) CKKSKeychainView* ckks;
@property (nullable) TPSpecificUser* activeAccount;

// Dependencies (for injection)
@property id<CKKSPeerProvider> octagonAdapter;
@property (readonly) Class<SecEscrowRequestable> escrowRequestClass;
@property (readonly) Class<CKKSNotifier> notifierClass;

@property (nonatomic) BOOL initialBecomeUntrustedPosted;
@property (nullable, nonatomic, strong) NSString* machineID;

@property (nullable) OTAccountSettings* accountSettings;

@end

@implementation OTCuttlefishContext

- (instancetype)initWithContainerName:(NSString*)containerName
                            contextID:(NSString*)contextID
                        activeAccount:(TPSpecificUser* _Nullable)activeAccount
                           cuttlefish:(id<NSXPCProxyCreating>)cuttlefish
                      ckksAccountSync:(CKKSKeychainView* _Nullable)ckks
                           sosAdapter:(id<OTSOSAdapter>)sosAdapter
                      accountsAdapter:(id<OTAccountsAdapter>)accountsAdapter
                       authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                       personaAdapter:(id<OTPersonaAdapter>)personaAdapter
                  tooManyPeersAdapter:(id<OTTooManyPeersAdapter>)tooManyPeersAdapter
                    tapToRadarAdapter:(id<OTTapToRadarAdapter>)tapToRadarAdapter
                     lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                  reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                  accountStateTracker:(id<CKKSCloudKitAccountStateTrackingProvider, CKKSOctagonStatusMemoizer>)accountStateTracker
             deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter
                   apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
                   escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
                        notifierClass:(Class<CKKSNotifier>)notifierClass
                                 cdpd:(id<OctagonFollowUpControllerProtocol>)cdpd
{
    if ((self = [super init])) {
        WEAKIFY(self);

        _containerName = containerName;
        _contextID = contextID;

        // Ideally, we'd replace containerName and contextID with this object. But, complications
        // arise for the default context, which needs to exist without an attached account.
        // For now, this is nullable.
        _activeAccount = activeAccount;
        _reachabilityTracker = reachabilityTracker;

        _apsReceiver = [OctagonAPSReceiver receiverForNamedDelegatePort:SecCKKSAPSNamedPort
                                                     apsConnectionClass:apsConnectionClass];
        [_apsReceiver registerCuttlefishReceiver:self
                                forContainerName:self.containerName
                                       contextID:contextID];

        _ckks = ckks;

        _initialBecomeUntrustedPosted = NO;

        _tooManyPeersAdapter = tooManyPeersAdapter;
        _tapToRadarAdapter = tapToRadarAdapter;

        _launchSequence = [[SecLaunchSequence alloc] initWithRocketName:@"com.apple.octagon.launch"];

        _queue = dispatch_queue_create("com.apple.security.otcuttlefishcontext", DISPATCH_QUEUE_SERIAL);
        _operationQueue = [[NSOperationQueue alloc] init];
        _cloudKitAccountStateKnown = [[CKKSCondition alloc] init];

        _accountMetadataStore = [[OTCuttlefishAccountStateHolder alloc] initWithQueue:_queue
                                                                            container:_containerName
                                                                              context:_contextID
                                                                       personaAdapter:personaAdapter
                                                                        activeAccount:activeAccount];

        [_accountMetadataStore registerNotification:self];

        _stateMachine = [[OctagonStateMachine alloc] initWithName:[contextID isEqualToString:OTDefaultContext] ? @"octagon" : [NSString stringWithFormat:@"octagon-%@", contextID]
                                                           states:[OTStates OctagonStateMap]
                                                            flags:[OTStates AllOctagonFlags]
                                                     initialState:OctagonStateInitializing
                                                            queue:_queue
                                                      stateEngine:self
                                       unexpectedStateErrorDomain:OctagonStateTransitionErrorDomain
                                                 lockStateTracker:lockStateTracker
                                              reachabilityTracker:reachabilityTracker];

        _sosAdapter = sosAdapter;
        [_sosAdapter registerForPeerChangeUpdates:self];
        _accountsAdapter = accountsAdapter;
        _authKitAdapter = authKitAdapter;
        _personaAdapter = personaAdapter;
        _deviceAdapter = deviceInformationAdapter;
        [_deviceAdapter registerForDeviceNameUpdates:self];

        _cuttlefishXPCWrapper = [[CuttlefishXPCWrapper alloc] initWithCuttlefishXPCConnection:cuttlefish];
        _lockStateTracker = lockStateTracker;
        _accountStateTracker = accountStateTracker;

        _followupHandler = [[OTFollowup alloc] initWithFollowupController:cdpd];

        [accountStateTracker registerForNotificationsOfCloudKitAccountStatusChange:self];
        [_authKitAdapter registerNotification:self];

        _notifierClass = notifierClass;
        _escrowRequestClass = escrowRequestClass;

        _suggestTLKUploadNotifier = [[CKKSNearFutureScheduler alloc] initWithName:@"octagon-tlk-request"
                                                                            delay:500*NSEC_PER_MSEC
                                                                 keepProcessAlive:false
                                                        dependencyDescriptionCode:0
                                                                            block:^{
                                                                                STRONGIFY(self);
                                                                                secnotice("octagon-ckks", "Adding flag for CKKS TLK upload");
                                                                                [self.stateMachine handleFlag:OctagonFlagCKKSRequestsTLKUpload];
                                                                            }];

        _requestPolicyCheckNotifier = [[CKKSNearFutureScheduler alloc] initWithName:@"octagon-policy-check"
                                                                              delay:500*NSEC_PER_MSEC
                                                                   keepProcessAlive:false
                                                          dependencyDescriptionCode:0 block:^{
            STRONGIFY(self);
            secnotice("octagon-ckks", "Adding flag for CKKS policy check");
            [self.stateMachine handleFlag:OctagonFlagCKKSRequestsPolicyCheck];
        }];

        _upgradeUserControllableViewsRateLimiter = [[CKKSNearFutureScheduler alloc] initWithName:@"octagon-upgrade-ucv"
                                                                                    initialDelay:0*NSEC_PER_SEC
                                                                                 continuingDelay:10*NSEC_PER_SEC
                                                                                keepProcessAlive:false
                                                                       dependencyDescriptionCode:0
                                                                                           block:^{
            STRONGIFY(self);
            OctagonPendingFlag* pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:OctagonFlagAttemptUserControllableViewStatusUpgrade
                                                                            conditions:OctagonPendingConditionsDeviceUnlocked | OctagonPendingConditionsNetworkReachable];
            [self.stateMachine handlePendingFlag:pendingFlag];
        }];

        _fixupRetryScheduler = [[CKKSNearFutureScheduler alloc] initWithName:@"octagon-retry-fixup"
                                                                initialDelay:(SecCKKSTestsEnabled() ? 2 : 10) * NSEC_PER_SEC
                                                          exponentialBackoff:2
                                                                maximumDelay:600 * NSEC_PER_SEC
                                                            keepProcessAlive:false
                                                   dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                       block:^{}];

        _shouldSendMetricsForOctagon = OTAccountMetadataClassC_MetricsState_UNKNOWN;

        _checkMetricsTrigger = [[CKKSNearFutureScheduler alloc] initWithName:@"ensure-metrics-off"
                                                                initialDelay:(SecCKKSTestsEnabled() ? OctagonNFSTwoSeconds : OctagonNFSOneHour)
                                                             continuingDelay:0
                                                            keepProcessAlive:YES
                                                   dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                       block:^{
            STRONGIFY(self);
            secnotice("octagon-metrics", "Added check-on-metrics flag to the state machine");
            [self.stateMachine handleFlag:OctagonFlagCheckOnRTCMetrics];
        }];
    }
    return self;
}

- (void)clearCKKS
{
    self.ckks = nil;
}

- (void)resetCKKS:(CKKSKeychainView*)view
{
    self.ckks = view;
}

- (void)dealloc
{
    // TODO: how to invalidate this?
    //[self.cuttlefishXPCWrapper invalidate];
}

- (void)notifyTrustChanged:(OTAccountMetadataClassC_TrustState)trustState {

    secnotice("octagon", "Changing trust status to: %@",
              (trustState == OTAccountMetadataClassC_TrustState_TRUSTED) ? @"Trusted" : @"Untrusted");

    /*
     * We are posting the legacy SOS notification if we don't use SOS
     * need to rework clients to use a new signal instead of SOS.
     */
    if (!SOSCompatibilityModeGetCachedStatus()) {
        [self.notifierClass post:[NSString stringWithUTF8String:kSOSCCCircleChangedNotification]];
    }

    [self.notifierClass post:[NSString stringWithUTF8String:OTTrustStatusChangeNotification]];
}

- (void)accountStateUpdated:(OTAccountMetadataClassC*)newState from:(OTAccountMetadataClassC *)oldState
{
    if (newState.icloudAccountState == OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE && oldState.icloudAccountState != OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE) {
        [self.launchSequence addEvent:@"iCloudAccount"];
    }

    if (newState.trustState == OTAccountMetadataClassC_TrustState_TRUSTED && oldState.trustState != OTAccountMetadataClassC_TrustState_TRUSTED) {
        [self.launchSequence addEvent:@"Trusted"];
    }
    if (newState.trustState != OTAccountMetadataClassC_TrustState_TRUSTED && oldState.trustState == OTAccountMetadataClassC_TrustState_TRUSTED) {
        [self.launchSequence addEvent:@"Untrusted"];
        [self notifyTrustChanged:newState.trustState];

        // At trust loss time, issue a TTR on homepod
        if(self.operationDependencies.deviceInformationAdapter.isHomePod) {
            secnotice("octagon", "Trust transition from TRUSTED to some other state, posting TTR");
            NSError *error;
            NSMutableDictionary *dict = [NSMutableDictionary dictionaryWithCapacity:5];
            dict[@"serial"] = self.operationDependencies.deviceInformationAdapter.serialNumber;
            dict[@"name"] = self.operationDependencies.deviceInformationAdapter.deviceName;
            dict[@"os_version"] = self.operationDependencies.deviceInformationAdapter.osVersion;
            dict[@"model_id"] = self.operationDependencies.deviceInformationAdapter.modelID;
            dict[@"peer_id"] = newState.peerID;
            NSData *jsonData = [NSJSONSerialization dataWithJSONObject:dict
                                                    options:NSJSONWritingSortedKeys
                                                    error:&error];
            NSString * _Nullable jsonString;
            if (!jsonData) {
                jsonString = [NSString stringWithFormat:@"Error while serializing identifiers: %@", error];
            } else {
                jsonString = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
            }

            [self.tapToRadarAdapter postHomePodLostTrustTTR:jsonString];
        } else {
            secnotice("octagon", "Trust transition from TRUSTED to UNTRUSTED on a non-homepod");
        }
    }

    if(![newState.syncingPolicy isEqualToData:oldState.syncingPolicy]) {
        // Let's parse these policies, and see if the UCV bit changed
        TPSyncingPolicy* newSyncingPolicy = newState.getTPSyncingPolicy;
        TPSyncingPolicy* oldSyncingPolicy = oldState.getTPSyncingPolicy;

        if(newSyncingPolicy.syncUserControllableViews != oldSyncingPolicy.syncUserControllableViews) {
            secnotice("octagon-ucv", "User controllable view state changed; posting notification");
            [self.notifierClass post:OTUserControllableViewStatusChanged];
        }
    }
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<OTCuttlefishContext: %@, %@>", self.containerName, self.contextID];
}

- (void)notificationOfMachineIDListChange
{
    secnotice("octagon", "machine ID list notification -- refreshing device list");
    [self requestTrustedDeviceListRefresh];
}


- (BOOL)canSendMetricsUsingAccountState:(OTAccountMetadataClassC_MetricsState)currentState {
    // If currentState is ENABLED or UNKNOWN, allow sending metrics.
    // A value of DISABLED means the device is Trusted so we should stop sending metrics in for the Octagon layer.
    return (currentState == OTAccountMetadataClassC_MetricsState_NOTPERMITTED) ? NO : YES;
}

- (void)cloudkitAccountStateChange:(CKAccountInfo* _Nullable)oldAccountInfo
                                to:(CKAccountInfo*)currentAccountInfo
{
    dispatch_sync(self.queue, ^{
        // We don't persist the CK account state; rather, we fetch it anew on every daemon launch.
        // But, we also have to integrate it into our asynchronous state machine.
        // So, record the current CK account value, and trigger state machine reprocessing.

        secnotice("octagon", "Told of a new CK account status: %@", currentAccountInfo);
        self.cloudKitAccountInfo = currentAccountInfo;

        if(currentAccountInfo.accountStatus == CKAccountStatusAvailable && self.activeAccount == nil) {
            // In order to maintain consistent values for flowID and deviceSessionID, copy the sessionMetrics to a local variable
            // that way when another thread mutates the sessionMetrics property, this thread can safely use the local copy
            __strong OTMetricsSessionData* localSessionMetrics = self.sessionMetrics;
            AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                         altDSID:self.activeAccount.altDSID
                                                                                                          flowID:localSessionMetrics.flowID
                                                                                                 deviceSessionID:localSessionMetrics.deviceSessionID
                                                                                                       eventName:kSecurityRTCEventNameCloudKitAccountAvailability
                                                                                                 testsAreEnabled:SecCKKSTestsEnabled()
                                                                                                  canSendMetrics:[self canSendMetricsUsingAccountState: self.shouldSendMetricsForOctagon]
                                                                                                        category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

            // There's probably an account. Let's optimistically fill in our account specifier if it's missing.
            // Do this off the queue just in case of deadlocks.
            NSError* accountError = nil;
            self.activeAccount = [self.accountsAdapter findAccountForCurrentThread:self.personaAdapter
                                                                   optionalAltDSID:nil
                                                             cloudkitContainerName:self.containerName
                                                                  octagonContextID:self.contextID
                                                                             error:&accountError];
            if(self.activeAccount) {
                secnotice("octagon-account", "Found a new account (%@): %@", self.contextID, self.activeAccount);
                [self.accountMetadataStore changeActiveAccount:self.activeAccount];
            } else {
                secnotice("octagon-account", "Unable to find a current account (context %@): %@", self.contextID, accountError);
            }
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:self.activeAccount ? YES : NO error:accountError];

        } else {
            secnotice("octagon-account", "skipping account fetch %@", self.contextID);
        }

        [self.stateMachine _onqueuePokeStateMachine];

        // But, having the state machine perform the signout is confusing: it would need to make decisions based
        // on things other than the current state. So, use the RPC mechanism to give it input.
        // If we receive a sign-in before the sign-out rpc runs, the state machine will be sufficient to get back into
        // the in-account state.

        // Also let other clients now that we have CK account status
        [self.cloudKitAccountStateKnown fulfill];

        // Note: do _not_ yet reset the TPSpecificUser. As part of teardown, we might still need it to pass to TPH.
    });

    if(!(currentAccountInfo.accountStatus == CKAccountStatusAvailable)) {
        secnotice("octagon", "Informed that the CK account is now unavailable: %@", currentAccountInfo);
        // In order to maintain consistent values for flowID and deviceSessionID, copy the sessionMetrics to a local variable
        // that way when another thread mutates the sessionMetrics property, this thread can safely use the local copy
        __strong OTMetricsSessionData* localSessionMetrics = self.sessionMetrics;
        AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                     altDSID:self.activeAccount.altDSID
                                                                                                      flowID:localSessionMetrics.flowID
                                                                                             deviceSessionID:localSessionMetrics.deviceSessionID
                                                                                                   eventName:kSecurityRTCEventNameCloudKitAccountAvailability
                                                                                             testsAreEnabled:SecCKKSTestsEnabled()
                                                                                              canSendMetrics:[self canSendMetricsUsingAccountState:self.shouldSendMetricsForOctagon]
                                                                                                    category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

        // Add a state machine request to return to OctagonStateWaitingForCloudKitAccount
        [self.stateMachine doSimpleStateMachineRPC:@"cloudkit-account-gone"
                                                op:[OctagonStateTransitionOperation named:@"cloudkit-account-gone"
                                                                                 entering:OctagonStateWaitingForCloudKitAccount]
                                      sourceStates:[OTStates OctagonInAccountStates]
                                             reply:^(NSError* error) {
            BOOL success = (error == nil) ? YES : NO;
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:success error:error];
        }];
    }
}

- (BOOL)accountAvailable:(NSString*)altDSID error:(NSError**)error
{
    secnotice("octagon", "Account available with altDSID: %@ %@", altDSID, self);

    self.launchSequence.firstLaunch = true;

    NSError* accountError = nil;
    self.activeAccount = [self.accountsAdapter findAccountForCurrentThread:self.personaAdapter
                                                           optionalAltDSID:altDSID
                                                     cloudkitContainerName:self.containerName
                                                          octagonContextID:self.contextID
                                                                     error:&accountError];
    if(self.activeAccount == nil || accountError != nil) {
        secerror("octagon-account: unable to determine active account for context(%@). Issues ahead: %@", self.contextID, accountError);
    } else {
        secnotice("octagon-account", "Found a new account (%@): %@", self.contextID, self.activeAccount);
        [self.accountMetadataStore changeActiveAccount:self.activeAccount];

        if(self.ckks.operationDependencies.activeAccount != nil && ![self.ckks.operationDependencies.activeAccount isEqual:self.activeAccount]) {
            // After a signout and then a sign-in of the same account, we might have a different accountID and personaID matching our altDSID.
            // Tell CKKS about the new world.
            secnotice("ckks-account", "Updating CKKS's idea of account to %@; old: %@", self.activeAccount, self.ckks.operationDependencies.activeAccount);
            self.ckks.operationDependencies.activeAccount = self.activeAccount;
        }
    }

    NSError* localError = nil;
    [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
        // Do not set the account available bit here, since we need to check if it's HSA2/Managed. The initializing state should do that for us...
        metadata.altDSID = altDSID;

        return metadata;
    } error:&localError];

    if(localError) {
        secerror("octagon: unable to persist new account availability: %@", localError);
    }

    [self.stateMachine handleFlag:OctagonFlagAccountIsAvailable];

    if(localError) {
        if(error) {
            *error = localError;
        }
        return NO;
    }
    return YES;
}
- (void)moveToCheckTrustedState
{
    if (self.lockStateTracker) {
        [self.lockStateTracker recheck];
    }

    [self.stateMachine handleFlag:OctagonFlagCheckTrustState];
}


- (BOOL)idmsTrustLevelChanged:(NSError**)error
{
    [self.stateMachine handleFlag:OctagonFlagIDMSLevelChanged];
    return YES;
}

- (BOOL)accountNoLongerAvailable
{
    if (self.lockStateTracker) {
        [self.lockStateTracker recheck];
    }

    [self.stateMachine handleFlag:OctagonFlagAppleAccountSignedOut];

    return YES;
}

- (OTCDPStatus)getCDPStatus:(NSError*__autoreleasing*)error
{
    NSError* localError = nil;
    OTAccountMetadataClassC* accountState = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];

    if(localError) {
        secnotice("octagon-cdp-status", "error fetching account metadata: %@", localError);
        if(error) {
            *error = localError;
        }

        return OTCDPStatusUnknown;
    }

    OTCDPStatus status = OTCDPStatusUnknown;
    switch(accountState.cdpState) {
        case OTAccountMetadataClassC_CDPState_UNKNOWN:
            status = OTCDPStatusUnknown;
            break;
        case OTAccountMetadataClassC_CDPState_DISABLED:
            status = OTCDPStatusDisabled;
            break;
        case OTAccountMetadataClassC_CDPState_ENABLED:
            status = OTCDPStatusEnabled;
            break;
    }

    secnotice("octagon-cdp-status", "current cdp status is: %@", OTCDPStatusToString(status));
    return status;
}

- (BOOL)setCDPEnabled:(NSError* __autoreleasing *)error
{
    NSError* localError = nil;
    [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
        metadata.cdpState = OTAccountMetadataClassC_CDPState_ENABLED;
        return metadata;
    } error:&localError];

    [self.stateMachine handleFlag:OctagonFlagCDPEnabled];

    if(localError) {
        secerror("octagon-cdp-status: unable to persist CDP enablement: %@", localError);
        if(error) {
            *error = localError;
        }
        return NO;
    }

    secnotice("octagon-cdp-status", "Successfully set CDP status bit to 'enabled''");
    return YES;
}

- (void)localReset:(nonnull void (^)(NSError * _Nullable))reply
{

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:@{
        OctagonStateLocalReset:  @{
            OctagonStateLocalResetClearLocalContextState: @{
                OctagonStateInitializing: [OctagonStateTransitionPathStep success],
            },
        },
    }];

    NSSet<OctagonState*>* sourceStates = [OTStates OctagonAllStates];

    [self.stateMachine doWatchedStateMachineRPC:@"local-reset-watcher"
                                   sourceStates:sourceStates
                                           path:path
                                          reply:reply];
}

- (NSDictionary*)establishStatePathDictionary
{
    return @{
        OctagonStateEstablishEnableCDPBit: @{
            OctagonStateReEnactDeviceList: @{
                OctagonStateReEnactPrepare: @{
                    OctagonStateResetAndEstablishClearLocalContextState: @{
                        OctagonStateReEnactReadyToEstablish: @{
                            OctagonStateEscrowTriggerUpdate: @{
                                OctagonStateBecomeReady: @{
                                    OctagonStateReady: [OctagonStateTransitionPathStep success],
                                },
                            },

                            // Error handling extra states:
                            OctagonStateEstablishCKKSReset: @{
                                OctagonStateEstablishAfterCKKSReset: @{
                                    OctagonStateEscrowTriggerUpdate: @{
                                        OctagonStateBecomeReady: @{
                                            OctagonStateReady: [OctagonStateTransitionPathStep success],
                                        },
                                    },
                                },
                            },
                        },
                    },
                },
            },
        },
    };
}

- (void)rpcEstablish:(nonnull NSString *)altDSID
               reply:(nonnull void (^)(NSError * _Nullable))reply
{
    // The establish flow can split into an error-handling path halfway through; this is okay
    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:[self establishStatePathDictionary]];

    [self.stateMachine doWatchedStateMachineRPC:@"establish"
                                   sourceStates:[OTStates OctagonInAccountStates]
                                           path:path
                                          reply:reply];
}


- (void)rpcReset:(CuttlefishResetReason)resetReason
           reply:(nonnull void (^)(NSError * _Nullable))reply
{
    _resetReason = resetReason;
    
    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary: @{
        OctagonStateCuttlefishReset: @{
            OctagonStateCKKSResetAfterOctagonReset: @{
                OctagonStateLocalReset:  @{
                    OctagonStateLocalResetClearLocalContextState: @{
                        OctagonStateInitializing: [OctagonStateTransitionPathStep success],
                    },
                },
            },
        },
    }];

    [self.stateMachine doWatchedStateMachineRPC:@"rpc-reset"
                                   sourceStates:[OTStates OctagonInAccountStates]
                                           path:path
                                          reply:reply];
}
- (void)performCKServerUnreadableDataRemoval:(void (^)(NSError* _Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon-perform-ckserver-unreadable-data-removal", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    [self.cuttlefishXPCWrapper performCKServerUnreadableDataRemovalWithSpecificUser:self.activeAccount
                                                                              reply:^(NSError * _Nullable removeError) {
        if (removeError) {
            secerror("octagon-perform-ckserver-unreadable-data-removal: failed with error: %@", removeError);
        } else {
            secnotice("octagon-perform-ckserver-unreadable-data-removal", "succeeded!");
        }
        reply(removeError);
    }];
}

- (void)rpcResetAndEstablish:(CuttlefishResetReason)resetReason
                       reply:(nonnull void (^)(NSError * _Nullable))reply
{
    [self rpcResetAndEstablish:resetReason
             idmsTargetContext:nil
        idmsCuttlefishPassword:nil
                    notifyIdMS:false
               accountSettings:nil
                         reply:reply];
}

- (OTAccountSettings*)mergedAccountSettings:(OTAccountSettings* _Nullable)incoming
{
    OTAccountSettings* existing = self.accountSettings;

    OTAccountSettings* merged = [[OTAccountSettings alloc] init];
    merged.walrus = incoming.hasWalrus ? incoming.walrus : existing.walrus;
    merged.webAccess = incoming.hasWebAccess ? incoming.webAccess : existing.webAccess;
    return merged;
}

- (void)rpcResetAndEstablish:(CuttlefishResetReason)resetReason
           idmsTargetContext:(NSString *_Nullable)idmsTargetContext
      idmsCuttlefishPassword:(NSString *_Nullable)idmsCuttlefishPassword
                  notifyIdMS:(bool)notifyIdMS
             accountSettings:(OTAccountSettings *_Nullable)accountSettings
                       reply:(nonnull void (^)(NSError * _Nullable))reply
{
    _resetReason = resetReason;
    _idmsTargetContext = idmsTargetContext;
    _idmsCuttlefishPassword = idmsCuttlefishPassword;
    _notifyIdMS = notifyIdMS;
    self.accountSettings = [self mergedAccountSettings:accountSettings];

    // The reset flow can split into an error-handling path halfway through; this is okay
    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary: @{
        OctagonStateResetBecomeUntrusted: @{
            OctagonStateResetAnyMissingTLKCKKSViews: @{
                OctagonStateResetAndEstablish: [self establishStatePathDictionary]
            },
        },
    }];

    // Now, take the state machine from any in-account state to the beginning of the reset flow.
    [self.stateMachine doWatchedStateMachineRPC:@"rpc-reset-and-establish"
                                   sourceStates:[OTStates OctagonInAccountStates]
                                           path:path
                                          reply:reply];
}

- (void)rpcLeaveClique:(nonnull void (^)(NSError * _Nullable))reply
{
    if ([self.stateMachine isPaused]) {
        if ([[OTStates OctagonNotInCliqueStates] intersectsSet: [NSSet setWithObject: [self.stateMachine currentState]]]) {
            secnotice("octagon-leave-clique", "device is not in clique to begin with - returning");
            reply(nil);
            return;
        }
    }

    OTLeaveCliqueOperation* op = [[OTLeaveCliqueOperation alloc] initWithDependencies:self.operationDependencies
                                                                        intendedState:OctagonStateBecomeUntrusted
                                                                           errorState:OctagonStateCheckTrustState];

    [self.stateMachine doSimpleStateMachineRPC:@"leave-clique"
                                            op:op
                                  sourceStates:[OTStates OctagonInAccountStates]
                                         reply:reply];
}

- (void)rpcRemoveFriendsInClique:(NSArray<NSString*>*)peerIDs
                           reply:(void (^)(NSError * _Nullable))reply
{
    OTRemovePeersOperation* op = [[OTRemovePeersOperation alloc] initWithDependencies:self.operationDependencies
                                                                            intendedState:OctagonStateBecomeReady
                                                                               errorState:OctagonStateBecomeReady
                                                                              peerIDs:peerIDs];

    NSSet<OctagonState*>* sourceStates = [OTStates OctagonReadyStates];
    [self.stateMachine doSimpleStateMachineRPC:@"remove-friends"
                                            op:op
                                  sourceStates:sourceStates
                                         reply:reply];
}

- (OTDeviceInformation*)prepareInformation
{
    NSError* error = nil;
    NSString* machineID = nil;

    if ([self.deviceAdapter isMachineIDOverridden]) {
        machineID = [self.deviceAdapter getOverriddenMachineID];
    } else {
        // In order to maintain consistent values for flowID and deviceSessionID, copy the sessionMetrics to a local variable
        // that way when another thread mutates the sessionMetrics property, this thread can safely use the local copy
        __strong OTMetricsSessionData* localSessionMetrics = self.sessionMetrics;
        machineID = [self.authKitAdapter machineID:self.activeAccount.altDSID
                                            flowID:localSessionMetrics.flowID
                                   deviceSessionID:localSessionMetrics.deviceSessionID
                                    canSendMetrics:[self canSendMetricsUsingAccountState:self.shouldSendMetricsForOctagon]
                                             error:&error];
    }

    if(!machineID || error) {
        secerror("octagon: Unable to fetch machine ID; expect signin to fail: %@", error);
    }

    return [[OTDeviceInformation alloc] initForContainerName:self.containerName
                                                   contextID:self.contextID
                                                       epoch:0
                                                   machineID:machineID
                                                     modelID:self.deviceAdapter.modelID
                                                  deviceName:self.deviceAdapter.deviceName
                                                serialNumber:self.deviceAdapter.serialNumber
                                                   osVersion:self.deviceAdapter.osVersion];
}

- (OTOperationDependencies*)operationDependencies
{
    // In order to maintain consistent values for flowID and deviceSessionID, copy the sessionMetrics to a local variable
    // that way when another thread mutates the sessionMetrics property, this thread can safely use the local copy
    __strong OTMetricsSessionData* localSessionMetrics = self.sessionMetrics;

    return [[OTOperationDependencies alloc] initForContainer:self.containerName
                                                   contextID:self.contextID
                                               activeAccount:self.activeAccount
                                                 stateHolder:self.accountMetadataStore
                                                 flagHandler:self.stateMachine
                                                  sosAdapter:self.sosAdapter
                                              octagonAdapter:self.octagonAdapter
                                             accountsAdapter:self.accountsAdapter
                                              authKitAdapter:self.authKitAdapter
                                              personaAdapter:self.personaAdapter
                                           deviceInfoAdapter:self.deviceAdapter
                                             ckksAccountSync:self.ckks
                                            lockStateTracker:self.lockStateTracker
                                        cuttlefishXPCWrapper:self.cuttlefishXPCWrapper
                                          escrowRequestClass:self.escrowRequestClass
                                               notifierClass:self.notifierClass
                                                      flowID:localSessionMetrics.flowID
                                             deviceSessionID:localSessionMetrics.deviceSessionID
                                      permittedToSendMetrics:[self canSendMetricsUsingAccountState:self.shouldSendMetricsForOctagon]
                                          reachabilityTracker:self.reachabilityTracker];
}

- (void)startOctagonStateMachine
{
    [self.stateMachine startOperation];
}

- (void)handlePairingRestart:(OTJoiningConfiguration*)config
{
    if(self.pairingUUID == nil){
        secnotice("octagon-pairing", "received new pairing UUID (%@)", config.pairingUUID);
        self.pairingUUID = config.pairingUUID;
    }

    if(![self.pairingUUID isEqualToString:config.pairingUUID]){
        secnotice("octagon-pairing", "current pairing UUID (%@) does not match config UUID (%@)", self.pairingUUID, config.pairingUUID);

        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
        [self localReset:^(NSError * _Nullable localResetError) {
            if(localResetError) {
                secerror("localReset returned an error: %@", localResetError);
            }else{
                secnotice("octagon", "localReset succeeded");
                self.pairingUUID = config.pairingUUID;
            }
            dispatch_semaphore_signal(sema);
        }];
        if (0 != dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 10))) {
            secerror("octagon: Timed out waiting for local reset to complete");
        }
    }
}

- (void)clearPairingUUID
{
    self.pairingUUID = nil;
}

#pragma mark --- State Machine Transitions

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)_onqueueNextStateMachineTransition:(OctagonState*)currentState
                                                                                                        flags:(nonnull OctagonFlags *)flags
                                                                                                 pendingFlags:(nonnull id<OctagonStateOnqueuePendingFlagHandler>)pendingFlagHandler
{
    dispatch_assert_queue(self.queue);

    [self.launchSequence addEvent:currentState];

    // if the 'apple account signed out' flag is set, handle it above everything else!
    if([flags _onqueueContains:OctagonFlagAppleAccountSignedOut]) {
        [flags _onqueueRemoveFlag:OctagonFlagAppleAccountSignedOut];
        secnotice("octagon", "handling apple account signed out flag");
        return [self appleAccountSignOutOperation];
    }

    // If We're initializing, or there was some recent update to the account state,
    // attempt to see what state we should enter.
    if([currentState isEqualToString: OctagonStateInitializing]) {
        return [self initializingOperation];
    }

    if([currentState isEqualToString:OctagonStateWaitForCDPCapableSecurityLevel]) {
        if([flags _onqueueContains:OctagonFlagIDMSLevelChanged]) {
            [flags _onqueueRemoveFlag:OctagonFlagIDMSLevelChanged];
            return [OctagonStateTransitionOperation named:@"cdp-capable-check"
                                                 entering:OctagonStateDetermineiCloudAccountState];
        }

        secnotice("octagon", "Waiting for an CDP Capable account");
        return nil;
    }

    if([currentState isEqualToString:OctagonStateWaitingForCloudKitAccount]) {
        // Here, integrate the memoized CK account state into our state machine
        if(self.cloudKitAccountInfo && self.cloudKitAccountInfo.accountStatus == CKAccountStatusAvailable) {
            secnotice("octagon", "CloudKit reports an account is available!");
            return [OctagonStateTransitionOperation named:@"ck-available"
                                                 entering:OctagonStateCloudKitNewlyAvailable];
        } else {
            secnotice("octagon", "Waiting for a CloudKit account; current state is %@", self.cloudKitAccountInfo ?: @"uninitialized");

            // Nudge the accountStateTracker if it hasn't successfully delivered a status
            if(self.cloudKitAccountInfo == nil) {
                secnotice("octagon", "Asking for a real CK account state");
                [self.accountStateTracker recheckCKAccountStatus];
            }

            return nil;
        }
    }

    if([currentState isEqualToString:OctagonStateCloudKitNewlyAvailable]) {
        return [self cloudKitAccountNewlyAvailableOperation:OctagonStateDetermineCDPState];
    }

    if([currentState isEqualToString:OctagonStateDetermineCDPState]) {
        return [[OTDetermineCDPBitStatusOperation alloc] initWithDependencies:self.operationDependencies
                                                                intendedState:OctagonStateCheckForAccountFixups
                                                                   errorState:OctagonStateWaitForCDP];
    }

    if([currentState isEqualToString:OctagonStateWaitForCDP]) {
        if ([flags _onqueueContains:OctagonFlagCDPEnabled]) {
            [flags _onqueueRemoveFlag:OctagonFlagCDPEnabled];
            secnotice("octagon", "CDP is newly available!");

            return [OctagonStateTransitionOperation named:@"cdp_enabled"
                                                 entering:OctagonStateDetermineiCloudAccountState];

        } else if([flags _onqueueContains:OctagonFlagCuttlefishNotification]) {
            [flags _onqueueRemoveFlag:OctagonFlagCuttlefishNotification];
            return [OctagonStateTransitionOperation named:@"cdp_enabled_push_received"
                                                 entering:OctagonStateWaitForCDPUpdated];

        } else if([flags _onqueueContains:OctagonFlagPendingNetworkAvailablity]) {
            [flags _onqueueRemoveFlag:OctagonFlagPendingNetworkAvailablity];
            return [OctagonStateTransitionOperation named:@"check_cdp_status_upon_network_availability"
                                                 entering:OctagonStateWaitForCDPUpdated];
        } else {
            return nil;
        }
    }

    if([currentState isEqualToString:OctagonStateWaitForCDPUpdated]) {
        return [[OTUpdateTPHOperation alloc] initWithDependencies:self.operationDependencies
                                                    intendedState:OctagonStateDetermineCDPState
                                                 peerUnknownState:nil
                                                determineCDPState:OctagonStateDetermineCDPState
                                                       errorState:OctagonStateDetermineCDPState
                                                     forceRefetch:NO
                                                        retryFlag:OctagonFlagCuttlefishNotification];
    }

    if([currentState isEqualToString:OctagonStateCheckForAccountFixups]) {
        return [self checkForAccountFixupsOperation:OctagonStatePerformAccountFixups];
    }

    if([currentState isEqualToString:OctagonStatePerformAccountFixups]) {
        return [OctagonStateTransitionOperation named:@"fixups-complete"
                                             entering:OctagonStateCheckTrustState];
    }

    if([currentState isEqualToString:OctagonStateCheckTrustState]) {
        return [[OctagonCheckTrustStateOperation alloc] initWithDependencies:self.operationDependencies
                                                               intendedState:OctagonStateBecomeUntrusted
                                                                  errorState:OctagonStateBecomeUntrusted];
    }
#pragma mark --- Octagon Health Check States
    if([currentState isEqualToString:OctagonStateCDPHealthCheck]) {
        return [[OTDetermineCDPBitStatusOperation alloc] initWithDependencies:self.operationDependencies
                                                       intendedState:OctagonStateSecurityTrustCheck
                                                          errorState:OctagonStateWaitForCDP];
    }

    if([currentState isEqualToString:OctagonStateSecurityTrustCheck]) {
        return [self evaluateSecdOctagonTrust];
    }

    if([currentState isEqualToString:OctagonStateTPHTrustCheck]) {
        return [self evaluateTPHOctagonTrust];
    }

    if([currentState isEqualToString:OctagonStateCuttlefishTrustCheck]) {
        return [self cuttlefishTrustEvaluation];
    }

    if ([currentState isEqualToString:OctagonStatePostRepairCFU]) {
        return [self postRepairCFUAndBecomeUntrusted];
    }

    if ([currentState isEqualToString:OctagonStateHealthCheckReset]) {
        // A small violation of state machines...
        _resetReason = CuttlefishResetReasonHealthCheck;
        return [OctagonStateTransitionOperation named:@"begin-reset"
                                             entering:OctagonStateResetBecomeUntrusted];
    }

    if([currentState isEqualToString:OctagonStateBecomeUntrusted]) {
        return [self becomeUntrustedOperation:OctagonStateUntrusted];
    }

    if([currentState isEqualToString:OctagonStateBecomeReady]) {
        return [self becomeReadyOperation];
    }

    if([currentState isEqualToString:OctagonStateRefetchCKKSPolicy]) {
        return [[OTFetchViewsOperation alloc] initWithDependencies:self.operationDependencies
                                                     intendedState:OctagonStateBecomeReady
                                                        errorState:OctagonStateError];
    }

    if([currentState isEqualToString:OctagonStateEnableUserControllableViews]) {
        return [[OTModifyUserControllableViewStatusOperation alloc] initWithDependencies:self.operationDependencies
                                                                      intendedViewStatus:TPPBPeerStableInfoUserControllableViewStatus_ENABLED
                                                                           intendedState:OctagonStateBecomeReady
                                                                        peerMissingState:OctagonStateReadyUpdated
                                                                                errorState:OctagonStateBecomeReady];
    }

    if([currentState isEqualToString:OctagonStateDisableUserControllableViews]) {
        return [[OTModifyUserControllableViewStatusOperation alloc] initWithDependencies:self.operationDependencies
                                                                      intendedViewStatus:TPPBPeerStableInfoUserControllableViewStatus_DISABLED
                                                                           intendedState:OctagonStateBecomeReady
                                                                        peerMissingState:OctagonStateReadyUpdated
                                                                              errorState:OctagonStateBecomeReady];
    }

    if([currentState isEqualToString:OctagonStateSetUserControllableViewsToPeerConsensus]) {
        // Setting the status to FOLLOWING will either enable or disable the value, depending on our peers.
        return [[OTModifyUserControllableViewStatusOperation alloc] initWithDependencies:self.operationDependencies
                                                                      intendedViewStatus:TPPBPeerStableInfoUserControllableViewStatus_FOLLOWING
                                                                           intendedState:OctagonStateBecomeReady
                                                                        peerMissingState:OctagonStateReadyUpdated
                                                                              errorState:OctagonStateBecomeReady];
    }

    if([currentState isEqualToString:OctagonStateSetAccountSettings]) {
        return [[OTSetAccountSettingsOperation alloc] initWithDependencies:self.operationDependencies
                                                             intendedState:OctagonStateBecomeReady
                                                                errorState:OctagonStateCheckTrustState
                                                                  settings:self.accountSettings];
    }

    if([currentState isEqualToString:OctagonStateNoAccount]) {
        // We only want to move out of untrusted if something useful has happened!
        if([flags _onqueueContains:OctagonFlagAccountIsAvailable]) {
            [flags _onqueueRemoveFlag:OctagonFlagAccountIsAvailable];
            secnotice("octagon", "Account is available!  Attempting initializing op!");
            return [OctagonStateTransitionOperation named:@"account-probably-present"
                                                 entering:OctagonStateInitializing];
        }
    }

    if([currentState isEqualToString:OctagonStateUntrusted]) {
        // We only want to move out of untrusted if something useful has happened!
        if([flags _onqueueContains:OctagonFlagEgoPeerPreapproved]) {
            [flags _onqueueRemoveFlag:OctagonFlagEgoPeerPreapproved];
            if(self.sosAdapter.sosEnabled) {
                secnotice("octagon", "Preapproved flag is high. Attempt SOS upgrade again!");
                return [OctagonStateTransitionOperation named:@"ck-available"
                                                     entering:OctagonStateAttemptSOSUpgrade];

            } else {
                secnotice("octagon", "We are untrusted, but it seems someone preapproves us now. Unfortunately, this platform doesn't support SOS.");
            }
        }

        if([flags _onqueueContains:OctagonFlagAttemptSOSUpgrade]) {
            [flags _onqueueRemoveFlag:OctagonFlagAttemptSOSUpgrade];
            if(self.sosAdapter.sosEnabled) {
                secnotice("octagon", "Attempt SOS upgrade again!");
                return [OctagonStateTransitionOperation named:@"attempt-sos-upgrade"
                                                     entering:OctagonStateAttemptSOSUpgrade];

            } else {
                secnotice("octagon", "We are untrusted, but this platform doesn't support SOS.");
            }
        }

        if([flags _onqueueContains:OctagonFlagAttemptSOSConsistency]) {
            [flags _onqueueRemoveFlag:OctagonFlagAttemptSOSConsistency];
            if(self.sosAdapter.sosEnabled) {
                secnotice("octagon", "Attempting SOS upgrade again (due to a consistency notification)");
                return [OctagonStateTransitionOperation named:@"attempt-sos-upgrade"
                                                     entering:OctagonStateAttemptSOSUpgrade];
            } else {
                secnotice("octagon", "Someone would like us to check SOS consistency, but this platform doesn't support SOS.");
            }
        }

        if([flags _onqueueContains:OctagonFlagCuttlefishNotification]) {
            [flags _onqueueRemoveFlag:OctagonFlagCuttlefishNotification];
            secnotice("octagon", "Updating TPH (while untrusted) due to push");
            return [OctagonStateTransitionOperation named:@"untrusted-update"
                                                 entering:OctagonStateUntrustedUpdated];
        }

        // We're untrusted; no need for the IDMS level flag anymore
        if([flags _onqueueContains:OctagonFlagIDMSLevelChanged]) {
            [flags _onqueueRemoveFlag:OctagonFlagIDMSLevelChanged];
        }

        // We're untrusted; no need for the CDP level flag anymore
        if([flags _onqueueContains:OctagonFlagCDPEnabled]) {
            secnotice("octagon", "Removing 'CDP enabled' flag");
            [flags _onqueueRemoveFlag:OctagonFlagCDPEnabled];
        }

        if ([flags _onqueueContains:OctagonFlagCheckTrustState]) {
            secnotice("octagon", "Checking trust state");
            [flags _onqueueRemoveFlag:OctagonFlagCheckTrustState];
            return [OctagonStateTransitionOperation named:@"check-trust-state"
                                                 entering:OctagonStateCheckTrustState];
        }
    }

    if([currentState isEqualToString:OctagonStateUntrustedUpdated]) {
            return [[OTUpdateTPHOperation alloc] initWithDependencies:self.operationDependencies
                                                        intendedState:OctagonStateUntrusted
                                                     peerUnknownState:OctagonStatePeerMissingFromServer
                                                    determineCDPState:nil
                                                           errorState:OctagonStateUntrusted
                                                         forceRefetch:NO
                                                            retryFlag:OctagonFlagCuttlefishNotification];
    }

    if([currentState isEqualToString:OctagonStateDetermineiCloudAccountState]) {
        secnotice("octagon", "Determine iCloud account status");

        // If there's an HSA2/Managed account, return to 'initializing' here, as we want to centralize decisions on what to do next
        return [[OTDetermineCDPCapableAccountStatusOperation alloc] initWithDependencies:self.operationDependencies
                                                                       stateIfCDPCapable:OctagonStateInitializing
                                                                    stateIfNotCDPCapable:OctagonStateWaitForCDPCapableSecurityLevel
                                                                        stateIfNoAccount:OctagonStateNoAccount
                                                                              errorState:OctagonStateError];
    }
    
    if([currentState isEqualToString:OctagonStateCuttlefishReset]) {
        secnotice("octagon", "Resetting cuttlefish");
        return [[OTResetOperation alloc] init:self.containerName
                                    contextID:self.contextID
                                       reason:_resetReason
                            idmsTargetContext:nil
                       idmsCuttlefishPassword:nil
                                   notifyIdMS:nil
                                intendedState:OctagonStateCKKSResetAfterOctagonReset
                                 dependencies:self.operationDependencies
                                   errorState:OctagonStateError
                         cuttlefishXPCWrapper:self.cuttlefishXPCWrapper];
        
    }
    
    if([currentState isEqualToString:OctagonStateCKKSResetAfterOctagonReset]) {
        return [[OTLocalCKKSResetOperation alloc] initWithDependencies:self.operationDependencies
                                                         intendedState:OctagonStateLocalReset
                                                            errorState:OctagonStateBecomeUntrusted];

    }
    if([currentState isEqualToString:OctagonStateLocalReset]) {
        secnotice("octagon", "Attempting local-reset");
        return [[OTLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                     intendedState:OctagonStateLocalResetClearLocalContextState
                                                    errorState:OctagonStateInitializing];
    }

    if([currentState isEqualToString:OctagonStateLocalResetClearLocalContextState]) {
        [self clearContextState];
        return [OctagonStateTransitionOperation named:@"move-to-initializing"
                                             entering:OctagonStateInitializing];
    }

    if([currentState isEqualToString:OctagonStateNoAccountDoReset]) {
        secnotice("octagon", "Attempting local-reset as part of signout");
        [self clearContextState];
        return [[OTLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                     intendedState:OctagonStateNoAccount
                                                        errorState:OctagonStateNoAccount];
    }

    if([currentState isEqualToString:OctagonStatePeerMissingFromServer]) {
        [self clearContextState];
        return [[OTLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                     intendedState:OctagonStateBecomeUntrusted
                                                        errorState:OctagonStateBecomeUntrusted];
    }

    if([currentState isEqualToString:OctagonStateEnsureConsistency]) {
        secnotice("octagon", "Ensuring consistency of things that might've changed");
        if(self.sosAdapter.sosEnabled) {
            return [[OTEnsureOctagonKeyConsistency alloc] initWithDependencies:self.operationDependencies
                                                                 intendedState:OctagonStateEnsureUpdatePreapprovals
                                                                    errorState:OctagonStateBecomeReady];
        }

        // Add further consistency checks here.
        return [OctagonStateTransitionOperation named:@"no-consistency-checks"
                                             entering:OctagonStateBecomeReady];
    }

    if([currentState isEqualToString:OctagonStateBottlePreloadOctagonKeysInSOS]) {
        secnotice("octagon", "Preloading Octagon Keys on the SOS Account");

        if(self.sosAdapter.sosEnabled) {
            OctagonState* nextState = (self.custodianRecoveryKey || self.recoveryKey) ? OctagonStateJoinSOSAfterCKKSFetch : OctagonStateSetAccountSettings;
            return [[OTPreloadOctagonKeysOperation alloc] initWithDependencies:self.operationDependencies
                                                                 intendedState:nextState
                                                                    errorState:nextState];
        }
        // Add further consistency checks here.
        return [OctagonStateTransitionOperation named:@"no-preload-octagon-key"
                                             entering:OctagonStateSetAccountSettings];
    }

    if([currentState isEqualToString:OctagonStateEnsureUpdatePreapprovals]) {
        secnotice("octagon", "SOS is enabled; ensuring preapprovals are correct");
        return [[OTSOSUpdatePreapprovalsOperation alloc] initWithDependencies:self.operationDependencies
                                                                intendedState:OctagonStateBecomeReady
                                                           sosNotPresentState:OctagonStateBecomeReady
                                                                   errorState:OctagonStateBecomeReady];
    }

    if([currentState isEqualToString:OctagonStateAttemptSOSUpgradeDetermineCDPState]) {
        return [[OTDetermineCDPBitStatusOperation alloc] initWithDependencies:self.operationDependencies
                                                                intendedState:OctagonStateAttemptSOSUpgrade
                                                                   errorState:OctagonStateWaitForCDP];
    }

    if([currentState isEqualToString:OctagonStateAttemptSOSUpgrade]) {
        secnotice("octagon", "Investigating SOS status");
        return [[OTSOSUpgradeOperation alloc] initWithDependencies:self.operationDependencies
                                                     intendedState:OctagonStateBecomeReady
                                                 ckksConflictState:OctagonStateSOSUpgradeCKKSReset
                                                        errorState:OctagonStateBecomeUntrusted
                                                        deviceInfo:self.prepareInformation
                                                    policyOverride:self.policyOverride];

    } else if([currentState isEqualToString:OctagonStateSOSUpgradeCKKSReset]) {
        return [[OTLocalCKKSResetOperation alloc] initWithDependencies:self.operationDependencies
                                                         intendedState:OctagonStateSOSUpgradeAfterCKKSReset
                                                            errorState:OctagonStateBecomeUntrusted];

    } else if([currentState isEqualToString:OctagonStateSOSUpgradeAfterCKKSReset]) {
        return [[OTSOSUpgradeOperation alloc] initWithDependencies:self.operationDependencies
                                                     intendedState:OctagonStateBecomeReady
                                                 ckksConflictState:OctagonStateBecomeUntrusted
                                                        errorState:OctagonStateBecomeUntrusted
                                                        deviceInfo:self.prepareInformation
                                                    policyOverride:self.policyOverride];


    } else if([currentState isEqualToString:OctagonStateStashAccountSettingsForRecoveryKey]) {
        return [[OTStashAccountSettingsOperation alloc] initWithDependencies:self.operationDependencies
                                                               intendedState:OctagonStateCreateIdentityForRecoveryKey
                                                                  errorState:OctagonStateCreateIdentityForRecoveryKey
                                                             accountSettings:self
                                                                 accountWide:true
                                                                  forceFetch:true];
    } else if([currentState isEqualToString:OctagonStateCreateIdentityForRecoveryKey]) {
        return [[OTPrepareOperation alloc] initWithDependencies:self.operationDependencies
                                                  intendedState:OctagonStateVouchWithRecoveryKey
                                                     errorState:OctagonStateBecomeUntrusted
                                                     deviceInfo:[self prepareInformation]
                                                 policyOverride:self.policyOverride
                                                accountSettings:self.accountSettings
                                                          epoch:1];

    } else if([currentState isEqualToString:OctagonStateCreateIdentityForCustodianRecoveryKey]) {
        return [[OTPrepareOperation alloc] initWithDependencies:self.operationDependencies
                                                  intendedState:OctagonStateVouchWithCustodianRecoveryKey
                                                     errorState:OctagonStateBecomeUntrusted
                                                     deviceInfo:[self prepareInformation]
                                                 policyOverride:self.policyOverride
                                                accountSettings:self.accountSettings
                                                          epoch:1];

    } else if([currentState isEqualToString:OctagonStateBottleJoinCreateIdentity]) {
        return [[OTPrepareOperation alloc] initWithDependencies:self.operationDependencies
                                                  intendedState:OctagonStateBottleJoinVouchWithBottle
                                                     errorState:OctagonStateBecomeUntrusted
                                                     deviceInfo:[self prepareInformation]
                                                 policyOverride:self.policyOverride
                                                accountSettings:self.accountSettings
                                                          epoch:1];

    } else if([currentState isEqualToString:OctagonStateBottleJoinVouchWithBottle]) {
        return [[OTVouchWithBottleOperation alloc] initWithDependencies:self.operationDependencies
                                                          intendedState:OctagonStateInitiatorSetCDPBit
                                                             errorState:OctagonStateBecomeUntrusted
                                                               bottleID:_bottleID
                                                                entropy:_entropy
                                                             bottleSalt:_bottleSalt
                                                            saveVoucher:YES];

    } else if([currentState isEqualToString:OctagonStateVouchWithRecoveryKey]) {
        return [[OTVouchWithRecoveryKeyOperation alloc] initWithDependencies:self.operationDependencies
                                                               intendedState:OctagonStateInitiatorSetCDPBit
                                                                  errorState:OctagonStateBecomeUntrusted
                                                                 recoveryKey:self.recoveryKey
                                                                 saveVoucher:YES];

    } else if([currentState isEqualToString:OctagonStateVouchWithCustodianRecoveryKey]) {
        return [[OTVouchWithCustodianRecoveryKeyOperation alloc] initWithDependencies:self.operationDependencies
                                                                        intendedState:OctagonStateInitiatorSetCDPBit
                                                                           errorState:OctagonStateBecomeUntrusted
                                                                 custodianRecoveryKey:self.custodianRecoveryKey
                                                                          saveVoucher:YES];

    } else if([currentState isEqualToString:OctagonStatePrepareAndRecoverTLKSharesForInheritancePeer]) {
        return [[OTPrepareAndRecoverTLKSharesForInheritancePeerOperation alloc] initWithDependencies:self.operationDependencies
                                                                                       intendedState:OctagonStateBecomeInherited
                                                                                          errorState:OctagonStateError
                                                                                                  ik:self.inheritanceKey
                                                                                          deviceInfo:[self prepareInformation]
                                                                                      policyOverride:self.policyOverride
                                                                                  isInheritedAccount:YES
                                                                                               epoch:1];

    } else if([currentState isEqualToString:OctagonStateJoinSOSAfterCKKSFetch]) {
        return  [[OTJoinSOSAfterCKKSFetchOperation alloc] initWithDependencies:self.operationDependencies
                                                                 intendedState:OctagonStateSetAccountSettings
                                                                    errorState:OctagonStateSetAccountSettings];

    } else if([currentState isEqualToString:OctagonStateInitiatorSetCDPBit]) {
        return [[OTSetCDPBitOperation alloc] initWithDependencies:self.operationDependencies
                                                    intendedState:OctagonStateInitiatorUpdateDeviceList
                                                       errorState:OctagonStateDetermineCDPState];

    } else if([currentState isEqualToString:OctagonStateInitiatorUpdateDeviceList]) {
        // As part of the 'initiate' flow, we need to update the trusted device list-you're probably on it already
        OTUpdateTrustedDeviceListOperation* op = [[OTUpdateTrustedDeviceListOperation alloc] initWithDependencies:self.operationDependencies
                                                                                                    intendedState:OctagonStateInitiatorJoin
                                                                                                 listUpdatesState:OctagonStateInitiatorJoin
                                                                                                       errorState:OctagonStateBecomeUntrusted
                                                                                                        retryFlag:nil];
        return op;

    } else if ([currentState isEqualToString:OctagonStateInitiatorJoin]){
        return [[OTJoinWithVoucherOperation alloc] initWithDependencies:self.operationDependencies
                                                          intendedState:OctagonStateBottlePreloadOctagonKeysInSOS
                                                      ckksConflictState:OctagonStateInitiatorJoinCKKSReset
                                                             errorState:OctagonStateBecomeUntrusted];

    } else if([currentState isEqualToString:OctagonStateInitiatorJoinCKKSReset]) {
        return [[OTLocalCKKSResetOperation alloc] initWithDependencies:self.operationDependencies
                                                         intendedState:OctagonStateInitiatorJoinAfterCKKSReset
                                                            errorState:OctagonStateBecomeUntrusted];

    } else if ([currentState isEqualToString:OctagonStateInitiatorJoinAfterCKKSReset]){
        return [[OTJoinWithVoucherOperation alloc] initWithDependencies:self.operationDependencies
                                                          intendedState:OctagonStateBottlePreloadOctagonKeysInSOS
                                                      ckksConflictState:OctagonStateBecomeUntrusted
                                                             errorState:OctagonStateBecomeUntrusted];

    } else if([currentState isEqualToString:OctagonStateResetBecomeUntrusted]) {
        return [self becomeUntrustedOperation:OctagonStateResetAnyMissingTLKCKKSViews];
    } else if([currentState isEqualToString:OctagonStateResetAndEstablish]) {
        return [[OTResetOperation alloc] init:self.containerName
                                    contextID:self.contextID
                                       reason:_resetReason
                            idmsTargetContext:_idmsTargetContext
                       idmsCuttlefishPassword:_idmsCuttlefishPassword
                                   notifyIdMS:_notifyIdMS
                                intendedState:OctagonStateEstablishEnableCDPBit
                                 dependencies:self.operationDependencies
                                   errorState:OctagonStateError
                         cuttlefishXPCWrapper:self.cuttlefishXPCWrapper];

    } else if([currentState isEqualToString:OctagonStateResetAnyMissingTLKCKKSViews]) {
        return [[OTResetCKKSZonesLackingTLKsOperation alloc] initWithDependencies:self.operationDependencies
                                                                    intendedState:OctagonStateResetAndEstablish
                                                                       errorState:OctagonStateError];

    } else if([currentState isEqualToString:OctagonStateEstablishEnableCDPBit]) {
        return [[OTSetCDPBitOperation alloc] initWithDependencies:self.operationDependencies
                                                    intendedState:OctagonStateReEnactDeviceList
                                                       errorState:OctagonStateError];

    } else if([currentState isEqualToString:OctagonStateReEnactDeviceList]) {
        return [[OTUpdateTrustedDeviceListOperation alloc] initWithDependencies:self.operationDependencies
                                                                  intendedState:OctagonStateReEnactPrepare
                                                               listUpdatesState:OctagonStateReEnactPrepare
                                                                     errorState:OctagonStateBecomeUntrusted
                                                                      retryFlag:nil];

    } else if([currentState isEqualToString:OctagonStateReEnactPrepare]) {
        // <rdar://problem/56270219> Octagon: use epoch transmitted across pairing channel
        // Note: Resetting the account returns epoch to 0.
        return [[OTPrepareOperation alloc] initWithDependencies:self.operationDependencies
                                                  intendedState:OctagonStateResetAndEstablishClearLocalContextState
                                                     errorState:OctagonStateError
                                                     deviceInfo:[self prepareInformation]
                                                 policyOverride:self.policyOverride
                                                accountSettings:self.accountSettings
                                                          epoch:0];

    } else if([currentState isEqualToString:OctagonStateResetAndEstablishClearLocalContextState]) {
        secnotice("octagon","clear cuttlefish context state");
        [self clearContextState];
        return [OctagonStateTransitionOperation named:@"moving-to-re-enact-ready-to-establish"
                                             entering:OctagonStateReEnactReadyToEstablish];

    } else if([currentState isEqualToString:OctagonStateReEnactReadyToEstablish]) {
        return [[OTEstablishOperation alloc] initWithDependencies:self.operationDependencies
                                                    intendedState:OctagonStateEscrowTriggerUpdate
                                                ckksConflictState:OctagonStateEstablishCKKSReset
                                                       errorState:OctagonStateBecomeUntrusted];

    } else if([currentState isEqualToString:OctagonStateEstablishCKKSReset]) {
        return [[OTLocalCKKSResetOperation alloc] initWithDependencies:self.operationDependencies
                                                         intendedState:OctagonStateEstablishAfterCKKSReset
                                                            errorState:OctagonStateBecomeUntrusted];

    } else if([currentState isEqualToString:OctagonStateEstablishAfterCKKSReset]) {
        // If CKKS fails again, just go to "become untrusted"
        return [[OTEstablishOperation alloc] initWithDependencies:self.operationDependencies
                                                    intendedState:OctagonStateEscrowTriggerUpdate
                                                   ckksConflictState:OctagonStateBecomeUntrusted
                                                       errorState:OctagonStateBecomeUntrusted];

    } else if ([currentState isEqualToString:OctagonStateEscrowTriggerUpdate]){
        return [[OTTriggerEscrowUpdateOperation alloc] initWithDependencies:self.operationDependencies
                                                              intendedState:OctagonStateBecomeReady
                                                                 errorState:OctagonStateError];

    } else if ([currentState isEqualToString:OctagonStateHealthCheckLeaveClique]) {
        return [[OTLeaveCliqueOperation alloc] initWithDependencies: self.operationDependencies
                                                      intendedState: OctagonStateBecomeUntrusted
                                                         errorState: OctagonStateBecomeUntrusted];

    } else if([currentState isEqualToString:OctagonStateWaitForClassCUnlock]) {
        if([flags _onqueueContains:OctagonFlagUnlocked]) {
            [flags _onqueueRemoveFlag:OctagonFlagUnlocked];
            return [OctagonStateTransitionOperation named:[NSString stringWithFormat:@"%@", @"initializing-after-initial-unlock"]
                                                 entering:OctagonStateInitializing];
        }

        [pendingFlagHandler _onqueueHandlePendingFlagLater:[[OctagonPendingFlag alloc] initWithFlag:OctagonFlagUnlocked
                                                                                         conditions:OctagonPendingConditionsDeviceUnlocked]];
        return nil;

    } else if([currentState isEqualToString: OctagonStateWaitForUnlock]) {
        if([flags _onqueueContains:OctagonFlagUnlocked]) {
            [flags _onqueueRemoveFlag:OctagonFlagUnlocked];
            return [OctagonStateTransitionOperation named:[NSString stringWithFormat:@"%@", @"initializing-after-unlock"]
                                                 entering:OctagonStateInitializing];
        }

        [pendingFlagHandler _onqueueHandlePendingFlagLater:[[OctagonPendingFlag alloc] initWithFlag:OctagonFlagUnlocked
                                                                                         conditions:OctagonPendingConditionsDeviceUnlocked]];
        return nil;

    } else if([currentState isEqualToString: OctagonStateUpdateSOSPreapprovals]) {
        secnotice("octagon", "Updating SOS preapprovals");

        // TODO: if this update fails, we need to redo it later.
        return [[OTSOSUpdatePreapprovalsOperation alloc] initWithDependencies:self.operationDependencies
                                                                intendedState:OctagonStateReady
                                                           sosNotPresentState:OctagonStateReady
                                                                   errorState:OctagonStateReady];

    } else if([currentState isEqualToString:OctagonStateAssistCKKSTLKUpload]) {
        return [[OTUploadNewCKKSTLKsOperation alloc] initWithDependencies:self.operationDependencies
                                                            intendedState:OctagonStateReady
                                                        ckksConflictState:OctagonStateAssistCKKSTLKUploadCKKSReset
                                                         peerMissingState:OctagonStateReadyUpdated
                                                               errorState:OctagonStateReady];

    } else if([currentState isEqualToString:OctagonStateAssistCKKSTLKUploadCKKSReset]) {
        return [[OTLocalCKKSResetOperation alloc] initWithDependencies:self.operationDependencies
                                                         intendedState:OctagonStateAssistCKKSTLKUploadAfterCKKSReset
                                                            errorState:OctagonStateBecomeReady];

    } else if([currentState isEqualToString:OctagonStateAssistCKKSTLKUploadAfterCKKSReset]) {
        // If CKKS fails again, just go to 'ready'
        return [[OTUploadNewCKKSTLKsOperation alloc] initWithDependencies:self.operationDependencies
                                                            intendedState:OctagonStateReady
                                                        ckksConflictState:OctagonStateReady
                                                         peerMissingState:OctagonStateReadyUpdated
                                                               errorState:OctagonStateReady];

    } else if([currentState isEqualToString:OctagonStateStashAccountSettingsForReroll]) {
        return [[OTStashAccountSettingsOperation alloc] initWithDependencies:self.operationDependencies
                                                               intendedState:OctagonStateCreateIdentityForReroll
                                                                  errorState:OctagonStateCreateIdentityForReroll
                                                             accountSettings:self
                                                                 accountWide:true
                                                                  forceFetch:true];

    } else if([currentState isEqualToString:OctagonStateCreateIdentityForReroll]) {
        return [[OTPrepareOperation alloc] initWithDependencies:self.operationDependencies
                                                  intendedState:OctagonStateVouchWithReroll
                                                     errorState:OctagonStateBecomeUntrusted
                                                     deviceInfo:[self prepareInformation]
                                                 policyOverride:self.policyOverride
                                                accountSettings:self.accountSettings
                                                          epoch:1];

    } else if([currentState isEqualToString:OctagonStateVouchWithReroll]) {
        return [[OTVouchWithRerollOperation alloc] initWithDependencies:self.operationDependencies
                                                          intendedState:OctagonStateInitiatorSetCDPBit
                                                             errorState:OctagonStateBecomeUntrusted
                                                            saveVoucher:YES];
    } else if([currentState isEqualToString:OctagonStateBecomeInherited]) {
        return [self becomeInheritedOperation];
    } else if([currentState isEqualToString:OctagonStateInherited]) {
        [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:OctagonAnalyticsLastKeystateReady];
        [self.launchSequence launch];
        [[CKKSAnalytics logger] noteLaunchSequence:self.launchSequence];

        return nil;
    } else if([currentState isEqualToString:OctagonStateReady]) {
        if([flags _onqueueContains:OctagonFlagCuttlefishNotification]) {
            [flags _onqueueRemoveFlag:OctagonFlagCuttlefishNotification];
            secnotice("octagon", "Updating TPH (while ready) due to push");
            return [OctagonStateTransitionOperation named:@"octagon-update"
                                                 entering:OctagonStateReadyUpdated];
        }


        if([flags _onqueueContains:OctagonFlagCKKSRequestsTLKUpload]) {
            [flags _onqueueRemoveFlag:OctagonFlagCKKSRequestsTLKUpload];
            return [OctagonStateTransitionOperation named:@"ckks-assist"
                                                 entering:OctagonStateAssistCKKSTLKUpload];
        }

        if([flags _onqueueContains:OctagonFlagFetchAuthKitMachineIDList]) {
            [flags _onqueueRemoveFlag:OctagonFlagFetchAuthKitMachineIDList];

            secnotice("octagon", "Received an suggestion to update the machine ID list (while ready); updating trusted device list");

            // If the cached list changes due to this fetch, go into 'updated'. Otherwise, back into ready with you!
            return [[OTUpdateTrustedDeviceListOperation alloc] initWithDependencies:self.operationDependencies
                                                                      intendedState:OctagonStateReady
                                                                   listUpdatesState:OctagonStateReadyUpdated
                                                                         errorState:OctagonStateReady
                                                                          retryFlag:OctagonFlagFetchAuthKitMachineIDList];
        }

        if([flags _onqueueContains:OctagonFlagAttemptSOSUpdatePreapprovals]) {
            [flags _onqueueRemoveFlag:OctagonFlagAttemptSOSUpdatePreapprovals];
            if(self.sosAdapter.sosEnabled) {
                secnotice("octagon", "Attempt SOS Update preapprovals again!");
                return [OctagonStateTransitionOperation named:@"attempt-sos-update-preapproval"
                                                     entering:OctagonStateUpdateSOSPreapprovals];
            } else {
                secnotice("octagon", "We are untrusted, but this platform doesn't support SOS.");
            }
        }

        if([flags _onqueueContains:OctagonFlagAttemptSOSConsistency]) {
            [flags _onqueueRemoveFlag:OctagonFlagAttemptSOSConsistency];
            if(self.sosAdapter.sosEnabled) {
                secnotice("octagon", "Attempting SOS consistency checks");
                return [OctagonStateTransitionOperation named:@"attempt-sos-update-preapproval"
                                                     entering:OctagonStateEnsureConsistency];
            } else {
                secnotice("octagon", "Someone would like us to check SOS consistency, but this platform doesn't support SOS.");
            }
        }

        if([flags _onqueueContains:OctagonFlagAttemptUserControllableViewStatusUpgrade]) {
            [flags _onqueueRemoveFlag:OctagonFlagAttemptUserControllableViewStatusUpgrade];
            secnotice("octagon", "Attempting user-view control upgrade");
            return [OctagonStateTransitionOperation named:@"attempt-user-view-upgrade"
                                                 entering:OctagonStateSetUserControllableViewsToPeerConsensus];
        }

        if([flags _onqueueContains:OctagonFlagCKKSRequestsPolicyCheck]) {
            [flags _onqueueRemoveFlag:OctagonFlagCKKSRequestsPolicyCheck];
            secnotice("octagon", "Updating CKKS policy");
            return [OctagonStateTransitionOperation named:@"ckks-policy-update"
                                                 entering:OctagonStateReadyUpdated];
        }

        if([flags _onqueueContains:OctagonFlagCKKSViewSetChanged]) {
            // We want to tell CKKS that we're trusted again.
            [flags _onqueueRemoveFlag:OctagonFlagCKKSViewSetChanged];
            return [OctagonStateTransitionOperation named:@"ckks-update-trust"
                                                 entering:OctagonStateBecomeReady];
        }

        if([flags _onqueueContains:OctagonFlagSecureElementIdentityChanged]) {
            [flags _onqueueRemoveFlag:OctagonFlagSecureElementIdentityChanged];
            return [OctagonStateTransitionOperation named:@"octagon-set-secureelement"
                                                 entering:OctagonStateReadyUpdated];
        }

        if([flags _onqueueContains:OctagonFlagAccountIsAvailable]) {
            // We're in ready--we already know the account is available
            secnotice("octagon", "Removing 'account is available' flag");
            [flags _onqueueRemoveFlag:OctagonFlagAccountIsAvailable];
        }

        // We're ready; no need for the IDMS level flag anymore
        if([flags _onqueueContains:OctagonFlagIDMSLevelChanged]) {
            secnotice("octagon", "Removing 'IDMS level changed' flag");
            [flags _onqueueRemoveFlag:OctagonFlagIDMSLevelChanged];
        }

        // We're ready; no need for the CDP level flag anymore
        if([flags _onqueueContains:OctagonFlagCDPEnabled]) {
            secnotice("octagon", "Removing 'CDP enabled' flag");
            [flags _onqueueRemoveFlag:OctagonFlagCDPEnabled];
        }

        if([flags _onqueueContains:OctagonFlagCheckOnRTCMetrics]) {
            secnotice("octagon-metrics", "Checking metrics");
            [flags _onqueueRemoveFlag:OctagonFlagCheckOnRTCMetrics];
            WEAKIFY(self);
            return [OctagonStateTransitionOperation named:@"check-on-metrics" 
                                                intending:OctagonStateReady
                                               errorState:OctagonStateReady
                                      withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                STRONGIFY(self);

                NSError* fetchError = nil;
                BOOL sendingMetricsPermitted = [self fetchSendingMetricsPermitted:&fetchError];

                if (fetchError) {
                    secerror("octagon-metrics: failed to fetch account metadata: %@", fetchError);
                    [self.checkMetricsTrigger trigger];
                } else {
                    secnotice("octagon-metrics", "current metrics setting set to: %@", sendingMetricsPermitted ? @"Permitted" : @"Not Permitted");

                    if (sendingMetricsPermitted) {
                        NSError* persistError = nil;
                        BOOL persisted = [self persistSendingMetricsPermitted:NO error:&persistError];

                        if (persisted == NO || persistError) {
                            secerror("octagon-metrics: failed to persist metrics setting: %@", persistError);
                        } else {
                            secnotice("octagon-metrics", "persisted metrics setting set to not permitted");
                        }
                    }
                    [self.checkMetricsTrigger cancel];
                    self.checkMetricsTrigger = nil;
                }
            }];
        }

        [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:OctagonAnalyticsLastKeystateReady];
        [self.launchSequence launch];
        [[CKKSAnalytics logger] noteLaunchSequence:self.launchSequence];
        return nil;
    }

    if([currentState isEqualToString:OctagonStateReadyUpdated]) {
        return [[OTUpdateTPHOperation alloc] initWithDependencies:self.operationDependencies
                                                    intendedState:OctagonStateReady
                                                 peerUnknownState:OctagonStatePeerMissingFromServer
                                                determineCDPState:nil
                                                       errorState:OctagonStateReady
                                                     forceRefetch:NO
                                                        retryFlag:OctagonFlagCuttlefishNotification];

    }

    if ([currentState isEqualToString:OctagonStateError]) {
        return nil;
    }

    return nil;
}

- (void)setMetricsStateToActive
{
    secnotice("octagon-metrics", "Metrics now switching to ON");

    self.ckks.operationDependencies.sendMetric = true;
    self.ckks.zoneChangeFetcher.sendMetric = true;
    self.shouldSendMetricsForOctagon = OTAccountMetadataClassC_MetricsState_PERMITTED;
}

- (void)setMetricsStateToInactive
{
    secnotice("octagon-metrics", "Metrics now switching to OFF");

    self.ckks.operationDependencies.sendMetric = false;
    self.ckks.zoneChangeFetcher.sendMetric = false;
    self.shouldSendMetricsForOctagon = OTAccountMetadataClassC_MetricsState_NOTPERMITTED;
}

- (void)setMetricsToState:(OTAccountMetadataClassC_MetricsState)state
{
    if (state == OTAccountMetadataClassC_MetricsState_NOTPERMITTED) {
        [self setMetricsStateToInactive];
    } else {
        [self setMetricsStateToActive];
    }
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)initializingOperation
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"octagon-initializing"
                                        intending:OctagonStateNoAccount
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                                  STRONGIFY(self);
                                  NSError* localError = nil;
                                  OTAccountMetadataClassC* account = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];
                                  if(localError && [self.lockStateTracker isLockedError:localError]){
                                      secnotice("octagon", "Device is locked! pending initialization on unlock");
                                      // Since we can't load a class C item, we go to a different waitforunlock state.
                                      // That way, we'll be less likely for an RPC to break us.
                                      op.nextState = OctagonStateWaitForClassCUnlock;
                                      return;
                                  }

                                  if(localError || !account) {
                                      secnotice("octagon", "Error loading account data: %@", localError);
                                      [self setMetricsStateToActive];
                                      op.nextState = OctagonStateNoAccount;

                                  } else if(account.icloudAccountState == OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE) {
                                      secnotice("octagon", "An CDP Capable iCloud account exists; waiting for CloudKit to confirm");

                                      [self setMetricsToState:account.sendingMetricsPermitted];

                                      if(self.activeAccount == nil) {
                                          NSError* accountError = nil;
                                          self.activeAccount = [self.accountsAdapter findAccountForCurrentThread:self.personaAdapter
                                                                                                 optionalAltDSID:nil
                                                                                           cloudkitContainerName:self.containerName
                                                                                                octagonContextID:self.contextID
                                                                                                           error:&accountError];
                                          if(self.activeAccount == nil || accountError != nil) {
                                              secerror("octagon-account: unable to determine active account for context(%@). Issues ahead: %@", self.contextID, accountError);
                                          } else {
                                              secnotice("octagon-account", "Found a new account (%@): %@", self.contextID, self.activeAccount);
                                              [self.accountMetadataStore changeActiveAccount:self.activeAccount];
                                          }
                                          if(self.activeAccount.altDSID == nil || ![self.activeAccount.altDSID isEqualToString:account.altDSID]) {
                                              secerror("octagon-account: discovered altDSID (%@) does not match persisted altDSID (%@)", self.activeAccount.altDSID, account.altDSID);
                                          }
                                      }

                                      // Inform the account state tracker of our HSA2/Managed account
                                      [self.accountStateTracker setCDPCapableiCloudAccountStatus:CKKSAccountStatusAvailable];

                                      // This seems an odd place to do this, but CKKS currently also tracks the CloudKit account state.
                                      // Since we think we have an HSA2/Managed account, let CKKS figure out its own CloudKit state
                                      secnotice("octagon-ckks", "Initializing CKKS views");
                                      [self.cuttlefishXPCWrapper fetchCurrentPolicyWithSpecificUser:self.activeAccount
                                                                                    modelIDOverride:self.deviceAdapter.modelID
                                                                                 isInheritedAccount:account.isInheritedAccount
                                                                                              reply:^(TPSyncingPolicy * _Nullable syncingPolicy,
                                                                                                      TPPBPeerStableInfoUserControllableViewStatus userControllableViewStatusOfPeers,
                                                                                                      NSError * _Nullable policyError) {
                                          if(!syncingPolicy || policyError) {
                                              secerror("octagon-ckks: Unable to fetch initial syncing policy. THIS MIGHT CAUSE SYNCING FAILURES LATER: %@", policyError);
                                          } else {
                                              secnotice("octagon-ckks", "Fetched initial syncing policy: %@", syncingPolicy);
                                              [self.ckks setCurrentSyncingPolicy:syncingPolicy];
                                          }
                                      }];

                                      [self.ckks beginCloudKitOperation];

                                      op.nextState = OctagonStateWaitingForCloudKitAccount;

                                  } else if(account.icloudAccountState == OTAccountMetadataClassC_AccountState_NO_ACCOUNT && account.altDSID != nil) {
                                      secnotice("octagon", "An iCloud account exists, but doesn't appear to be CDP Capable. Let's check!");
                                      [self setMetricsToState:account.sendingMetricsPermitted];
                                      op.nextState = OctagonStateDetermineiCloudAccountState;

                                  } else if(account.icloudAccountState == OTAccountMetadataClassC_AccountState_NO_ACCOUNT) {
                                      [self.accountStateTracker setCDPCapableiCloudAccountStatus:CKKSAccountStatusNoAccount];

                                      secnotice("octagon", "No iCloud account available.");
                                      [self setMetricsToState:account.sendingMetricsPermitted];
                                      op.nextState = OctagonStateNoAccount;

                                  } else {
                                      secnotice("octagon", "Unknown account state (%@). Determining...", [account icloudAccountStateAsString:account.icloudAccountState]);
                                      [self setMetricsToState:account.sendingMetricsPermitted];
                                      op.nextState = OctagonStateDetermineiCloudAccountState;
                                  }
                              }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)appleAccountSignOutOperation
{
    return [OctagonStateTransitionOperation named:@"octagon-account-gone"
                                        intending:OctagonStateNoAccountDoReset
                                       errorState:OctagonStateNoAccountDoReset
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        __block NSError* localError = nil;

        secnotice("octagon", "Account now unavailable: %@", self);
        [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC *(OTAccountMetadataClassC * metadata) {
            metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_NO_ACCOUNT;
            metadata.altDSID = nil;
            metadata.trustState = OTAccountMetadataClassC_TrustState_UNKNOWN;
            metadata.cdpState = OTAccountMetadataClassC_CDPState_UNKNOWN;

            // Clear the SE identity, as it only belongs to a single account
            metadata.secureElementIdentity = nil;

            return metadata;
        } error:&localError];

        if(localError) {
            secerror("octagon: unable to persist new account availability: %@", localError);
        }

        [self.accountStateTracker setCDPCapableiCloudAccountStatus:CKKSAccountStatusNoAccount];

        // Note: do not reset the TPSpecificUser here. It might be relevant for diagnosing things later.
        // If an account signs back in for the active persona, it'll be reset.

        // Bring CKKS down, too
        secnotice("octagon-ckks", "Informing %@ of new untrusted status (due to account disappearance)", self.ckks);
        [self.ckks endTrustedOperation];

        op.error = localError;
    }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)checkForAccountFixupsOperation:(OctagonState*)intendedState
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"octagon-fixup_check"
                                        intending:intendedState
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        STRONGIFY(self);
        NSError* localError = nil;
        OTAccountMetadataClassC* metadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];

        if(localError && [self.lockStateTracker isLockedError:localError]){
            secnotice("octagon", "Device is locked! pending initialization on unlock");
            // Since we can't load a class C item, we go to a different waitforunlock state.
            // That way, we'll be less likely for an RPC to break us.
            op.nextState = OctagonStateWaitForClassCUnlock;
            return;
        }

        if(localError || !metadata) {
            secnotice("octagon", "Error loading account data: %@", localError);
            op.nextState = OctagonStateNoAccount;
            return;
        }

        op.nextState = intendedState;
    }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)evaluateSecdOctagonTrust
{
    return [OctagonStateTransitionOperation named:@"octagon-health-securityd-trust-check"
                                 intending:OctagonStateTPHTrustCheck
                                errorState:OctagonStatePostRepairCFU
                       withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                           NSError* localError = nil;
                           OTAccountMetadataClassC* account = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];
                           if(account.peerID && account.trustState == OTAccountMetadataClassC_TrustState_TRUSTED) {
                               secnotice("octagon-health", "peer is trusted: %@", account.peerID);
                               op.nextState = OctagonStateTPHTrustCheck;

                           } else {
                               secnotice("octagon-health", "trust state (%@). checking in with TPH", [account trustStateAsString:account.trustState]);
                               op.nextState = [self repairAccountIfTrustedByTPHWithIntendedState:OctagonStateTPHTrustCheck];
                           }
                       }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)evaluateTPHOctagonTrust
{
    return [OctagonStateTransitionOperation named:@"octagon-health-tph-trust-check"
                                        intending:OctagonStateCuttlefishTrustCheck
                                       errorState:OctagonStatePostRepairCFU
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                                  [self checkTrustStatusAndPostRepairCFUIfNecessary:^(CliqueStatus status, BOOL posted, BOOL hasIdentity, BOOL isLocked, NSError *trustFromTPHError) {

                                      [[CKKSAnalytics logger] logResultForEvent:OctagonEventTPHHealthCheckStatus hardFailure:false result:trustFromTPHError];
                                      if(trustFromTPHError) {
                                          secerror("octagon-health: hit an error asking TPH for trust status: %@", trustFromTPHError);
                                          op.error = trustFromTPHError;
                                          op.nextState = OctagonStateError;
                                      } else {
                                          if (isLocked == YES) {
                                              op.nextState = OctagonStateWaitForUnlock;
                                              secnotice("octagon-health", "TPH says device is locked!");
                                          } else if (hasIdentity == NO) {
                                              op.nextState = OctagonStateUntrusted;
                                          } else if(hasIdentity == YES && status == CliqueStatusIn){
                                              secnotice("octagon-health", "TPH says we're trusted and in");
                                              op.nextState = OctagonStateCuttlefishTrustCheck;
                                          } else if (hasIdentity == YES && status != CliqueStatusIn){
                                              secnotice("octagon-health", "TPH says we have an identity but we are not in Octagon, posted CFU: %d", !!posted);
                                              op.nextState = OctagonStatePostRepairCFU;
                                          } else {
                                              secnotice("octagon-health", "weird shouldn't hit this catch all.. assuming untrusted");
                                              op.nextState = OctagonStateUntrusted;
                                          }
                                      }
                                  }];
                              }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)cuttlefishTrustEvaluation
{

    OTCheckHealthOperation* op = [[OTCheckHealthOperation alloc] initWithDependencies:self.operationDependencies
                                                                        intendedState:OctagonStateBecomeReady
                                                                           errorState:OctagonStateBecomeReady
                                                                           deviceInfo:self.prepareInformation
                                                                 skipRateLimitedCheck:_skipRateLimitingCheck
                                                              reportRateLimitingError:_reportRateLimitingError
                                                                               repair:_repair];

    WEAKIFY(self);
    CKKSResultOperation* callback = [CKKSResultOperation named:@"rpcHealthCheck"
                                                     withBlock:^{
                                                         STRONGIFY(self);
                                                         secnotice("octagon-health",
                                                                   "Returning from cuttlefish trust check call: postRepairCFU(%d), postEscrowCFU(%d), resetOctagon(%d), leaveTrust(%d), reroll(%d), moveRequest(%d), results=%@",
                                                                   op.results.postRepairCFU, op.results.postEscrowCFU, op.results.resetOctagon, op.results.leaveTrust, op.results.reroll, op.results.moveRequest != nil, op.results);
                                                         self->_healthCheckResults = op.results;
                                                         if(op.results.postRepairCFU) {
                                                             secnotice("octagon-health", "Posting Repair CFU");
                                                             NSError* postRepairCFUError = nil;
                                                             [self postRepairCFU:&postRepairCFUError];
                                                             if(postRepairCFUError) {
                                                                 op.error = postRepairCFUError;
                                                             }
                                                         }
                                                         if(op.results.postEscrowCFU) {
                                                             //hold up, perhaps we already are pending an upload.
                                                             NSError* shouldPostError = nil;
                                                             BOOL shouldPost = [self shouldPostConfirmPasscodeCFU:&shouldPostError];
                                                             if(shouldPostError) {
                                                                 secerror("octagon-health, hit an error evaluating prerecord status: %@", shouldPostError);
                                                                 op.error = shouldPostError;
                                                             }
                                                             if(shouldPost) {
                                                                 secnotice("octagon-health", "Posting Escrow CFU");
                                                                 NSError* postEscrowCFUError = nil;
                                                                 BOOL ret = [self postConfirmPasscodeCFU:&postEscrowCFUError];
                                                                 if(!ret) {
                                                                     op.error = postEscrowCFUError;
                                                                 }
                                                             } else {
                                                                 secnotice("octagon-health", "Not posting confirm passcode CFU, already pending a prerecord upload");
                                                             }

                                                         }
                                                        if(op.results.leaveTrust){
                                                            secnotice("octagon-health", "Leaving Octagon and SOS trust");
                                                            NSError* leaveError = nil;
                                                            if(![self leaveTrust:&leaveError]) {
                                                                op.error = leaveError;
                                                            }
                                                        }
                                                        if(op.results.reroll) {
                                                            CKKSResultOperation* rerollOp = [CKKSResultOperation named:@"reroll"
                                                                                                   withBlockTakingSelf:^(CKKSResultOperation* _Nonnull thisOp) {
                                                                    STRONGIFY(self);
                                                                    secnotice("octagon-health", "Rerolling Octagon PeerID");
                                                                    [self rerollWithReply:^(NSError* _Nullable error) {
                                                                            if (error) {
                                                                                secerror("octagon-health: reroll failed: %@", error);
                                                                                thisOp.error = error;
                                                                            }
                                                                        }];
                                                                }];
                                                            [op addDependency:rerollOp];
                                                            [self.operationQueue addOperation:rerollOp];
                                                        }
                                                        if(op.results.moveRequest) {
                                                            secnotice("octagon-health", "Received escrow move request: %@", op.results.moveRequest);
                                                            NSError* moveError = nil;
                                                            if(![self processMoveRequest:op.results.moveRequest error:&moveError]) {
                                                                op.error = moveError;
                                                            }
                                                        }
                                                     }];
    [callback addDependency:op];
    [self.operationQueue addOperation: callback];
    return op;
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)postRepairCFUAndBecomeUntrusted
{
    return [OctagonStateTransitionOperation named:@"octagon-health-post-repair-cfu"
                                        intending:OctagonStateUntrusted
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        __block BOOL deviceIsLocked = NO;
        [self checkTrustStatusAndPostRepairCFUIfNecessary:^(CliqueStatus status,
                                                            BOOL posted,
                                                            BOOL hasIdentity,
                                                            BOOL isLocked,
                                                            NSError * _Nullable postError) {
            if(postError) {
                secerror("ocagon-health: failed to post repair cfu via state machine: %@", postError);
            } else if (isLocked) {
                deviceIsLocked = isLocked;
                secnotice("octagon-health", "device is locked, not posting cfu");
            } else {
                secnotice("octagon-health", "posted repair cfu via state machine");
            }
        }];
        if (deviceIsLocked == YES) {
            op.nextState = OctagonStateWaitForUnlock;
        } else {
            op.nextState = OctagonStateUntrusted;
        }
      }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)cloudKitAccountNewlyAvailableOperation:(OctagonState*)intendedState
{
    WEAKIFY(self);

    return [OctagonStateTransitionOperation named:@"octagon-icloud-account-available"
                                        intending:intendedState
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        STRONGIFY(self);

        NSError* localError = nil;
        OTAccountMetadataClassC* account = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];
        if (localError && [self.lockStateTracker isLockedError:localError]){
            secnotice("octagon", "Device is locked! pending initialization on unlock");
            // Since we can't load a class C item, we go to a different waitforunlock state.
            // That way, we'll be less likely for an RPC to break us.
            op.nextState = OctagonStateWaitForClassCUnlock;
            return;
        }

        if (localError || !account) {
            secnotice("octagon", "Error loading account data: %@", localError);
            op.nextState = OctagonStateNoAccount;
        }
        if (account.isInheritedAccount) {
            op.nextState = OctagonStateBecomeInherited;
            return;
        }

        // TVs can't use escrow records. So, don't preload the cache.
#if !TARGET_OS_TV
        if(account.warmedEscrowCache == NO) {
            if(account.peerID == nil) {
                secnotice("octagon-warm-escrowcache", "Beginning fetching escrow records to warm up the escrow cache in TPH");

                self.pendingEscrowCacheWarmup = [[CKKSCondition alloc] init];

                dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                    STRONGIFY(self);
                    [self rpcFetchAllViableEscrowRecordsFromSource:OTEscrowRecordFetchSourceDefault
                                                             reply:^(NSArray<NSData*>* _Nullable records,
                                                                     NSError* _Nullable error) {
                        STRONGIFY(self);
                        if (error) {
                            secerror("octagon-warm-escrowcache: failed to fetch escrow records, %@", error);
                        } else {
                            secnotice("octagon-warm-escrowcache", "Successfully fetched escrow records");
                        }

                        [self.pendingEscrowCacheWarmup fulfill];
                    }];
                });
            } else {
                secnotice("octagon-warm-escrowcache", "Already have a peerID; no need to warm escrow cache");
            }

            // Because cache warming is purely optional for performance, immediately write down that we started the attempt.
            // If we crash or otherwise fail before the cache attempt occurs, correctness should not be impacted.
            NSError* stateError = nil;
            [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
                metadata.warmedEscrowCache = YES;
                return metadata;
            } error:&stateError];

            if(stateError == nil) {
                secnotice("octagon-warm-escrowcache", "Successfully persisted warmed-escrow-cache attempt state");
            } else {
                secerror("octagon-warm-escrowcache: Failed to write down escrow cache attempt: %@", stateError);
            }
        }
#endif

        secnotice("octagon", "iCloud sign in occurred. Attempting to register with APS...");

        // Register with APS, but don't bother to wait until it's complete.
        CKContainer* ckContainer = [CKContainer containerWithIdentifier:self.containerName];
        [ckContainer serverPreferredPushEnvironmentWithCompletionHandler: ^(NSString *apsPushEnvString, NSError *error) {
            STRONGIFY(self);

            if(!self) {
                secerror("octagonpush: received callback for released object");
                return;
            }

            if(error || (apsPushEnvString == nil)) {
                secerror("octagonpush: Received error fetching preferred push environment (%@): %@", apsPushEnvString, error);
            } else {
                secnotice("octagonpush", "Registering for environment '%@'", apsPushEnvString);

                [self.apsReceiver registerForEnvironment:apsPushEnvString];
            }
        }];

        op.nextState = op.intendedState;
    }];
}

- (OctagonState*) repairAccountIfTrustedByTPHWithIntendedState:(OctagonState*)intendedState
{
    __block OctagonState* nextState = intendedState;

    //let's check in with TPH real quick to make sure it agrees with our local assessment
    secnotice("octagon-health", "repairAccountIfTrustedByTPHWithIntendedState: calling into TPH for trust status");

    OTOperationConfiguration *config = [[OTOperationConfiguration alloc]init];

    [self rpcTrustStatus:config reply:^(CliqueStatus status,
                                        NSString* egoPeerID,
                                        NSDictionary<NSString*, NSNumber*>* _Nullable peerCountByModelID,
                                        BOOL isExcluded,
                                        BOOL isLocked,
                                        NSError * _Nullable error) {
        BOOL hasIdentity = egoPeerID != nil;
        secnotice("octagon-health", "repairAccountIfTrustedByTPHWithIntendedState status: %ld, peerID: %@, isExcluded: %d error: %@", (long)status, egoPeerID, isExcluded, error);

        if (error) {
            secnotice("octagon-health", "got an error from tph, returning to become_ready state: %@", error);
            nextState = OctagonStateBecomeReady;
            return;
        }

        if (isLocked) {
            secnotice("octagon-health", "device is locked");
            nextState = OctagonStateWaitForUnlock;
            return;
        }

        if(hasIdentity && status == CliqueStatusIn) {
            secnotice("octagon-health", "TPH believes we're trusted, accepting ego peerID as %@", egoPeerID);

            NSError* persistError = nil;
            BOOL persisted = [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
                metadata.trustState = OTAccountMetadataClassC_TrustState_TRUSTED;
                metadata.peerID = egoPeerID;
                return metadata;
            } error:&persistError];
            if(!persisted || persistError) {
                secerror("octagon-health: couldn't persist results: %@", persistError);
                nextState = OctagonStateError;
            } else {
                secnotice("octagon-health", "added trusted identity to account metadata");
                nextState = intendedState;
            }

        } else if (hasIdentity && status != CliqueStatusIn) {
            secnotice("octagon-health", "TPH believes we're not trusted, requesting CFU post");
            nextState = OctagonStatePostRepairCFU;
        }
    }];

    return nextState;
}

- (BOOL)checkForPhonePeerPresence:(NSDictionary<NSString*, NSNumber*>* _Nullable)peerCountByModelID
{
    // Are there any iphones or iPads? about? Only iOS devices can repair apple TVs.
    bool phonePeerPresent = NO;
    for(NSString* modelID in peerCountByModelID.allKeys) {
        bool iPhone = [modelID hasPrefix:@"iPhone"];
        bool iPad = [modelID hasPrefix:@"iPad"];
        if(!iPhone && !iPad) {
            continue;
        }

        int count = [peerCountByModelID[modelID] intValue];
        if(count > 0) {
            secnotice("octagon", "Have %d peers with model %@", count, modelID);
            phonePeerPresent = YES;
            break;
        }
    }
    return phonePeerPresent;
}

- (void)checkTrustStatusAndPostRepairCFUIfNecessary:(void (^ _Nullable)(CliqueStatus status, BOOL posted, BOOL hasIdentity, BOOL isLocked, NSError * _Nullable error))reply
{
    WEAKIFY(self);
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];
    [self rpcTrustStatus:configuration reply:^(CliqueStatus status,
                                               NSString* _Nullable egoPeerID,
                                               NSDictionary<NSString*, NSNumber*>* _Nullable peerCountByModelID,
                                               BOOL isExcluded,
                                               BOOL isLocked,
                                               NSError * _Nullable error) {
        STRONGIFY(self);

        secnotice("octagon", "clique status: %@, egoPeerID: %@, peerCountByModelID: %@, isExcluded: %d error: %@", OTCliqueStatusToString(status), egoPeerID, peerCountByModelID, isExcluded, error);

        BOOL hasIdentity = egoPeerID != nil;

#if TARGET_OS_TV
        BOOL phonePeerPresent = [self checkForPhonePeerPresence:peerCountByModelID];
#endif

        if (status == CliqueStatusError && error.code == errSecItemNotFound) {
            secerror("octagon: Lost our identity keys!");
#if TARGET_OS_TV
            if(!phonePeerPresent) {
                secnotice("octagon", "No iOS peers in account; not posting CFU");
                reply(status, NO, hasIdentity, isLocked, nil);
                return;
            }
#endif
            secerror("octagon: Posting CFU");
            NSError* localError = nil;
            BOOL posted = [self postRepairCFU:&localError];
            reply(status, posted, hasIdentity, isLocked, localError);
            return;
        }

        if (error && error.code != errSecInteractionNotAllowed) {
            reply(status, NO, hasIdentity, isLocked, error);
            return;
        }

        if (isLocked) {
            secnotice("octagon", "device is locked; not posting CFU");
            reply(status, NO, hasIdentity, isLocked, error);
            return;
        }

#if TARGET_OS_TV
        if(!phonePeerPresent) {
            secnotice("octagon", "No iOS peers in account; not posting CFU");
            reply(status, NO, hasIdentity, isLocked, nil);
            return;
        }
#endif

        // On platforms with SOS, we only want to post a CFU if we've attempted to join at least once.
        // This prevents us from posting a CFU, then performing an SOS upgrade and succeeding.
        if(self.sosAdapter.sosEnabled) {
            NSError* fetchError = nil;
            OTAccountMetadataClassC* accountState = [self.accountMetadataStore loadOrCreateAccountMetadata:&fetchError];

            if(!accountState || fetchError){
                secerror("octagon: failed to retrieve joining attempt information: %@", fetchError);
                // fall through to below: posting the CFU is better than a false negative

            } else if(accountState.attemptedJoin == OTAccountMetadataClassC_AttemptedAJoinState_ATTEMPTED) {
                // Normal flow, fall through to below
            } else {
                // Triple-check with SOS: if it's in a bad state, post the CFU anyway
                secnotice("octagon", "SOS is enabled and we haven't attempted to join; checking with SOS");

                NSError* circleError = nil;
                SOSCCStatus sosStatus = [self.sosAdapter circleStatus:&circleError];

                if(circleError && [circleError.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain] && circleError.code == kSOSErrorNotReady) {
                    secnotice("octagon", "SOS is not ready, not posting CFU until it becomes so");
                    reply(status, NO, hasIdentity, isLocked, nil);
                    return;

                } else if(circleError) {
                    // Any other error probably indicates that there is some circle, but we're not in it
                    secnotice("octagon", "SOS is in an unknown error state, posting CFU: %@", circleError);

                } else if(sosStatus == kSOSCCInCircle) {
                    secnotice("octagon", "SOS is InCircle, not posting CFU");
                    reply(status, NO, hasIdentity, isLocked, nil);
                    return;
                } else {
                    secnotice("octagon", "SOS is %@, posting CFU", (__bridge NSString*)SOSCCGetStatusDescription(sosStatus));
                }
            }
        }

        if(status == CliqueStatusNotIn || status == CliqueStatusAbsent || isExcluded) {
            NSError* localError = nil;
            BOOL posted = [self postRepairCFU:&localError];
            reply(status, posted, hasIdentity, isLocked, localError);
            return;
        }
        reply(status, NO, hasIdentity, isLocked, nil);
        return;
    }];
}

#if TARGET_OS_WATCH
- (void)startCompanionPairing
{
    OTPairingInitiateWithCompletion(self.queue, false, ^(bool success, NSError *error) {
        if (success) {
            secnotice("octagon", "companion pairing succeeded");
        } else {
            if (error == nil) {
                error = [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecInternalError userInfo:nil];
            }
            secnotice("octagon", "companion pairing failed: %@", error);
        }
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventCompanionPairing hardFailure:false result:error];
    });
}
#endif /* TARGET_OS_WATCH */

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)becomeUntrustedOperation:(OctagonState*)intendedState
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"octagon-become-untrusted"
                                        intending:intendedState
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                                  STRONGIFY(self);
                                  NSError* localError = nil;

                                  // During testing, don't kick this off until it's needed
                                  if([self.contextID isEqualToString:OTDefaultContext]) {
                                      [self.accountStateTracker triggerOctagonStatusFetch];
                                  }

                                  __block BOOL deviceIsLocked = NO;
                                  [self checkTrustStatusAndPostRepairCFUIfNecessary:^(CliqueStatus status, BOOL posted, BOOL hasIdentity, BOOL isLocked, NSError * _Nullable postError) {

                                      [[CKKSAnalytics logger] logResultForEvent:OctagonEventCheckTrustForCFU hardFailure:false result:postError];
                                      if (postError && postError.code != errSecInteractionNotAllowed) {
                                          secerror("octagon: hit an error checking trust state or posting a cfu: %@", postError);
                                      } else if (isLocked == YES || (postError && postError.code == errSecInteractionNotAllowed)) {
                                          deviceIsLocked = isLocked;
                                          secerror("octagon: device is locked, not posting cfu");
                                      } else {
                                          secnotice("octagon", "clique status: %@, posted cfu: %d", OTCliqueStatusToString(status), !!posted);
                                      }
                                  }];

                                  if (deviceIsLocked == YES) {
                                      secnotice("octagon", "device is locked, state moving to wait for unlock");
                                      op.nextState = OctagonStateWaitForUnlock;
                                      return;
                                  }

#if TARGET_OS_WATCH
                                  [self startCompanionPairing];
#endif /* TARGET_OS_WATCH */

                                  [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
                                      metadata.trustState = OTAccountMetadataClassC_TrustState_UNTRUSTED;
                                      metadata.sendingMetricsPermitted = OTAccountMetadataClassC_MetricsState_PERMITTED;
                                      return metadata;
                                  } error:&localError];
        
                                  [self setMetricsStateToActive];
        
                                  if(localError) {
                                      secnotice("octagon", "Unable to set trust state: %@", localError);
                                      op.nextState = OctagonStateError;
                                  } else {
                                      op.nextState = op.intendedState;
                                  }

                                  secnotice("octagon-ckks", "Informing %@ of new untrusted status", self.ckks);
                                  [self.ckks endTrustedOperation];

                                  // We are no longer in a CKKS4All world. Tell SOS!
                                  if(self.sosAdapter.sosEnabled) {
                                      NSError* soserror = nil;
                                      [self.sosAdapter updateCKKS4AllStatus:NO error:&soserror];
                                      if(soserror) {
                                          secnotice("octagon-ckks", "Unable to disable the CKKS4All status in SOS: %@", soserror);
                                      }
                                  }

                                  /*
                                   * Initial notification that we let the world know that trust is up and doing something
                                   */
                                  if (!self.initialBecomeUntrustedPosted) {
                                      [self notifyTrustChanged:OTAccountMetadataClassC_TrustState_UNTRUSTED];
                                      self.initialBecomeUntrustedPosted = YES;
                                  }

                                  self.octagonAdapter = nil;
                              }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)becomeInheritedOperation
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"octagon-inherited"
                                        intending:OctagonStateInherited
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                                  STRONGIFY(self);


        __block NSString* peerID = nil;
        NSError* localError = nil;

        __block TPSyncingPolicy* policy = nil;

        [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
            peerID = metadata.peerID;

            policy = metadata.hasSyncingPolicy ? [metadata getTPSyncingPolicy] : nil;

            return metadata;
        } error:&localError];

        if(!peerID || localError) {
            secerror("octagon-ckks: No peer ID to pass to CKKS. Syncing will be disabled.");
        } else if(!policy) {
            secerror("octagon-ckks: No memoized CKKS policy, re-fetching");
            op.nextState = OctagonStateRefetchCKKSPolicy;
            return;

        } else {
            secnotice("octagon-ckks", "Initializing CKKS views with policy %@: %@", policy, policy.viewList);

            [self.ckks setCurrentSyncingPolicy:policy];

            OctagonCKKSPeerAdapter* inheritorAdapter = [[OctagonCKKSPeerAdapter alloc] initWithPeerID:peerID
                                                                                         specificUser:self.activeAccount
                                                                                       personaAdapter:self.personaAdapter
                                                                                        cuttlefishXPC:self.cuttlefishXPCWrapper];

            NSError* egoPeerKeysError = nil;
            CKKSSelves* selves = [inheritorAdapter fetchSelfPeers:&egoPeerKeysError];
            if(!selves || egoPeerKeysError) {
                secerror("octagon-ckks: Unable to fetch self peers for %@: %@", inheritorAdapter, egoPeerKeysError);

                if([self.lockStateTracker isLockedError:egoPeerKeysError]) {
                    secnotice("octagon-ckks", "Waiting for device unlock to proceed");
                    op.nextState = OctagonStateWaitForUnlock;
                } else {
                    secnotice("octagon-ckks", "Error is scary; becoming untrusted");
                    op.nextState = OctagonStateBecomeUntrusted;
                }
                return;
            }

            // stash a reference to the adapter so we can provide updates later
            self.octagonAdapter = inheritorAdapter;

            // Start all our CKKS views!
            secnotice("octagon-ckks", "Informing CKKS %@ of trusted operation with self peer %@", self.ckks, peerID);

            NSArray<id<CKKSPeerProvider>>* peerProviders = nil;

            if(self.sosAdapter.sosEnabled) {
                peerProviders = @[self.octagonAdapter, self.sosAdapter];

            } else {
                peerProviders = @[self.octagonAdapter];
            }

            self.suggestTLKUploadNotifier = nil;
            self.requestPolicyCheckNotifier = nil;

            [self.ckks beginTrustedOperation:peerProviders
                            suggestTLKUpload:self.suggestTLKUploadNotifier
                          requestPolicyCheck:self.requestPolicyCheckNotifier];
        }
        [self notifyTrustChanged:OTAccountMetadataClassC_TrustState_TRUSTED];

        op.nextState = op.intendedState;
        
        // updating the property to halt sending metrics for Octagon
        // metadata should only be updated to DISABLED by CKKS when initial sync is finished
        self.shouldSendMetricsForOctagon = OTAccountMetadataClassC_MetricsState_NOTPERMITTED;
    }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)becomeReadyOperation
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"octagon-ready"
                                        intending:OctagonStateReady
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                                  STRONGIFY(self);

        if([self.contextID isEqualToString:OTDefaultContext]) {
            [self.accountStateTracker triggerOctagonStatusFetch];
        }

        // Note: we don't modify the account metadata trust state; that will have been done
        // by a join or upgrade operation, possibly long ago

        // but, we do set the 'attempted join' bit, just in case the device joined before we started setting this bit

        // Also, ensure that the CKKS policy is correctly present and set in the view manager
        __block NSString* peerID = nil;
        NSError* localError = nil;

        __block TPSyncingPolicy* policy = nil;

        [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
            peerID = metadata.peerID;

            policy = metadata.hasSyncingPolicy ? [metadata getTPSyncingPolicy] : nil;

            if(metadata.attemptedJoin == OTAccountMetadataClassC_AttemptedAJoinState_ATTEMPTED) {
                return nil;
            }
            metadata.attemptedJoin = OTAccountMetadataClassC_AttemptedAJoinState_ATTEMPTED;

            return metadata;
        } error:&localError];

        if(!peerID || localError) {
            secerror("octagon-ckks: No peer ID to pass to CKKS. Syncing will be disabled.");
        } else if(!policy) {
            secerror("octagon-ckks: No memoized CKKS policy, re-fetching");
            op.nextState = OctagonStateRefetchCKKSPolicy;
            return;

        } else {
            if(policy.syncUserControllableViews == TPPBPeerStableInfoUserControllableViewStatus_UNKNOWN) {
                secnotice("octagon-ckks", "Memoized CKKS policy has no opinion of user-controllable view status");
                // Suggest the update, whenever possible
                [self.upgradeUserControllableViewsRateLimiter trigger];

            } else {
                // We are now in a CKKS4All world. Tell SOS!
                if(self.sosAdapter.sosEnabled) {
                    NSError* soserror = nil;
                    [self.sosAdapter updateCKKS4AllStatus:YES error:&soserror];
                    if(soserror) {
                        secnotice("octagon-ckks", "Unable to enable the CKKS4All status in SOS: %@", soserror);
                    }
                }
            }

            secnotice("octagon-ckks", "Initializing CKKS views with policy %@: %@", policy, policy.viewList);

            [self.ckks setCurrentSyncingPolicy:policy];

            OctagonCKKSPeerAdapter* octagonAdapter = [[OctagonCKKSPeerAdapter alloc] initWithPeerID:peerID
                                                                                       specificUser:self.activeAccount
                                                                                     personaAdapter:self.personaAdapter
                                                                                      cuttlefishXPC:self.cuttlefishXPCWrapper];

            // This octagon adapter must be able to load the self peer keys, or we're in trouble.
            NSError* egoPeerKeysError = nil;
            CKKSSelves* selves = [octagonAdapter fetchSelfPeers:&egoPeerKeysError];
            if(!selves || egoPeerKeysError) {
                secerror("octagon-ckks: Unable to fetch self peers for %@: %@", octagonAdapter, egoPeerKeysError);

                if([self.lockStateTracker isLockedError:egoPeerKeysError]) {
                    secnotice("octagon-ckks", "Waiting for device unlock to proceed");
                    op.nextState = OctagonStateWaitForUnlock;
                } else {
                    secnotice("octagon-ckks", "Error is scary; becoming untrusted");
                    op.nextState = OctagonStateBecomeUntrusted;
                }
                return;
            }

            // stash a reference to the adapter so we can provide updates later
            self.octagonAdapter = octagonAdapter;

            // Start all our CKKS views!
            secnotice("octagon-ckks", "Informing CKKS %@ of trusted operation with self peer %@", self.ckks, peerID);

            NSArray<id<CKKSPeerProvider>>* peerProviders = nil;

            if(self.sosAdapter.sosEnabled) {
                peerProviders = @[self.octagonAdapter, self.sosAdapter];

            } else {
                peerProviders = @[self.octagonAdapter];
            }

            [self.ckks beginTrustedOperation:peerProviders
                            suggestTLKUpload:self.suggestTLKUploadNotifier
                          requestPolicyCheck:self.requestPolicyCheckNotifier];
        }
        [self notifyTrustChanged:OTAccountMetadataClassC_TrustState_TRUSTED];

        op.nextState = op.intendedState;

        // updating the property to halt sending metrics for Octagon
        // metadata should only be updated to DISABLED by CKKS when initial sync is finished
        self.shouldSendMetricsForOctagon = OTAccountMetadataClassC_MetricsState_NOTPERMITTED;

        NSError* fetchError = nil;
        BOOL canSendMetrics = [self fetchSendingMetricsPermitted:&fetchError];
        if (fetchError == nil && canSendMetrics) {
            secnotice("octagon-metrics", "triggered metrics check");
            [self.checkMetricsTrigger trigger];
        } else if (fetchError) {
            secerror("octagon-metrics, failed to fetch metrics setting: %@", fetchError);
        }
    }];
}

#pragma mark --- Utilities to run at times

- (NSString * _Nullable)extractStringKey:(NSString * _Nonnull)key fromDictionary:(NSDictionary * _Nonnull)d
{
    NSString *value = d[key];
    if ([value isKindOfClass:[NSString class]]) {
        return value;
    }
    return NULL;
}

- (void)handleTTRRequest:(NSDictionary *)cfDictionary
{
    NSString *serialNumber = [self extractStringKey:@"s" fromDictionary:cfDictionary];
    NSString *ckDeviceId = [self extractStringKey:@"D" fromDictionary:cfDictionary];
    NSString *alert = [self extractStringKey:@"a" fromDictionary:cfDictionary];
    NSString *description = [self extractStringKey:@"d" fromDictionary:cfDictionary];
    NSString *radar = [self extractStringKey:@"R" fromDictionary:cfDictionary];
    NSString *componentName = [self extractStringKey:@"n" fromDictionary:cfDictionary];
    NSString *componentVersion = [self extractStringKey:@"v" fromDictionary:cfDictionary];
    NSString *componentID = [self extractStringKey:@"I" fromDictionary:cfDictionary];

    if (serialNumber) {
        if (![self.deviceAdapter.serialNumber isEqualToString:serialNumber]) {
            secnotice("octagon", "TTR request not for me (sn)");
            return;
        }
    }
    if (ckDeviceId) {
        NSString *selfDeviceID = self.accountStateTracker.ckdeviceID;
        if (serialNumber == nil || ![selfDeviceID isEqualToString:serialNumber]) {
            secnotice("octagon", "TTR request not for me (deviceId)");
            return;
        }
    }

    if (alert == NULL || description == NULL || radar == NULL) {
        secerror("octagon: invalid type of TTR requeat: %@", cfDictionary);
        return;
    }

    SecTapToRadar *ttr = [[SecTapToRadar alloc] initTapToRadar:alert
                                                   description:description
                                                         radar:radar];
    if (componentName && componentVersion && componentID) {
        ttr.componentName = componentName;
        ttr.componentVersion = componentVersion;
        ttr.componentID = componentID;
    }
    [ttr trigger];
}

// We can't make a APSIncomingMessage in the tests (no public constructor),
// but we don't really care about anything in it but the userInfo dictionary anyway
- (void)notifyContainerChange:(APSIncomingMessage* _Nullable)notification
{
    [self notifyContainerChangeWithUserInfo:notification.userInfo];
}

- (void)notifyContainerChangeWithUserInfo:(NSDictionary* _Nullable)userInfo
{
    secnotice("octagonpush", "received a cuttlefish push notification (%@): %@",
             self.containerName, userInfo);

    NSDictionary *cfDictionary = userInfo[@"cf"];
    if ([cfDictionary isKindOfClass:[NSDictionary class]]) {
        NSString *command = [self extractStringKey:@"k" fromDictionary:cfDictionary];
        if(command) {
            if ([command isEqualToString:@"r"]) {
                [self handleTTRRequest:cfDictionary];
            } else {
                secerror("octagon: unknown command: %@", command);
            }
            return;
        }
    }

    if (self.apsRateLimiter == nil) {
        dispatch_time_t minimumInitialDelay = 2 * NSEC_PER_SEC;
        if (![OTDeviceInformation isFullPeer:self.deviceAdapter.modelID]) {
            __block NSError* localError = nil;
            __block NSNumber* totalTrustedPeers = nil;

            [self rpcFetchTotalCountOfTrustedPeers:^(NSNumber * _Nullable count, NSError * _Nullable countError) {
                if(countError) {
                    secnotice("octagon-count-trusted-peers", "totalTrustedPeers errored: %@", countError);
                    localError = countError;
                } else {
                    secnotice("octagon-count-trusted-peers", "totalTrustedPeers succeeded, total count: %@", count);
                    totalTrustedPeers = count;
                }
            }];
            uint32_t maxSplayWindowSeconds = 60 * 5;
            if (localError == nil && totalTrustedPeers != nil){
                maxSplayWindowSeconds = (3 * [totalTrustedPeers unsignedIntValue]);
            }
            secnotice("octagon", "max splay window seconds for limiter %d", maxSplayWindowSeconds);

            minimumInitialDelay = (NSEC_PER_SEC/MSEC_PER_SEC) * (arc4random_uniform(MSEC_PER_SEC * maxSplayWindowSeconds) + (MSEC_PER_SEC * 2));
        }
        secnotice("octagon", "creating aps rate limiter with min initial delay of %llu", minimumInitialDelay);
        // If we're testing, for the initial delay, use 0.2 second. Otherwise, 2s.
        dispatch_time_t initialDelay = (SecCKKSReduceRateLimiting() ? 200 * NSEC_PER_MSEC : minimumInitialDelay);
        
        // If we're testing, for the initial delay, use 2 second. Otherwise, 30s.
        dispatch_time_t continuingDelay = (SecCKKSReduceRateLimiting() ? 2 * NSEC_PER_SEC : 30 * NSEC_PER_SEC);

        WEAKIFY(self);
        self.apsRateLimiter = [[CKKSNearFutureScheduler alloc] initWithName:@"aps-push-ratelimiter"
                                                               initialDelay:initialDelay
                                                            continuingDelay:continuingDelay
                                                           keepProcessAlive:YES
                                                  dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                      block:^{
                                                                          STRONGIFY(self);
                                                                          if (self == nil) {
                                                                              return;
                                                                          }
                                                                          secnotice("octagon-push-ratelimited", "notifying container of change for context: %@", self.contextID);
                                                                        OctagonPendingFlag *pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:OctagonFlagCuttlefishNotification
                                                                       conditions:OctagonPendingConditionsDeviceUnlocked];

                                                                          [self.stateMachine handlePendingFlag:pendingFlag];
                                                                      }];
    }

    [self.apsRateLimiter trigger];
}

- (BOOL)waitForReady:(int64_t)timeOffset
{
    OctagonState* currentState = [self.stateMachine waitForState:OctagonStateReady wait:timeOffset];
    return [currentState isEqualToString:OctagonStateReady];

}

- (void) popTooManyPeersDialogWithEgoPeerStatus:(TrustedPeersHelperEgoPeerStatus*)egoPeerStatus accountMeta:(OTAccountMetadataClassC*)accountMeta
{
    if (![self.tooManyPeersAdapter shouldPopDialog]) {
        return;
    }

    if (accountMeta.warnedTooManyPeers) {
        secnotice("octagon", "popdialog: Already checked this altDSID: %@", accountMeta.altDSID);
        return;
    }

    NSError* stateError = nil;
    [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
        metadata.warnedTooManyPeers = YES;
        return metadata;
    } error:&stateError];

    if(stateError == nil) {
        secnotice("octagon", "popdialog: Successfully persisted warned-too-many-peers state for %@", accountMeta.altDSID);
    } else {
        secerror("octagon: popdialog: Failed to persist warned-too-many-peers state for %@: %@", accountMeta.altDSID, stateError);
    }

    unsigned long peerCount = 0;
    for(NSNumber* peers in egoPeerStatus.peerCountsByMachineID.allValues) {
        peerCount += [peers longValue];
    }

    secnotice("octagon", "popdialog: ego peer status says peer count is: %lu", peerCount);

    unsigned long peerLimit = [self.tooManyPeersAdapter getLimit];
    if (peerCount < peerLimit) {
        secnotice("octagon", "popdialog: not popping dialog, number of peers ok: %lu < %lu", peerCount, peerLimit);
        return;
    }

    [self.tooManyPeersAdapter popDialogWithCount:peerCount limit:peerLimit];
}

- (void)setMachineIDOverride:(NSString*)machineID
{
    [self.deviceAdapter setOverriddenMachineID:machineID];
}

- (OTAccountMetadataClassC_TrustState)currentMemoizedTrustState
{
    NSError* localError = nil;
    OTAccountMetadataClassC* accountMetadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];

    if(!accountMetadata) {
        secnotice("octagon", "Unable to fetch account metadata: %@", localError);
        return OTAccountMetadataClassC_TrustState_UNKNOWN;
    }

    return accountMetadata.trustState;
}

- (OTAccountMetadataClassC_AccountState)currentMemoizedAccountState
{
    NSError* localError = nil;
    OTAccountMetadataClassC* accountMetadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];

    if(!accountMetadata) {
        secnotice("octagon", "Unable to fetch account metadata: %@", localError);
        return OTAccountMetadataClassC_AccountState_UNKNOWN;
    }

    return accountMetadata.icloudAccountState;
}

- (NSDate* _Nullable) currentMemoizedLastHealthCheck
{
    NSError* localError = nil;
    OTAccountMetadataClassC* accountMetadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];

    if(!accountMetadata) {
        secnotice("octagon", "Unable to fetch account metadata: %@", localError);
        return nil;
    }
    if(accountMetadata.lastHealthCheckup == 0) {
        return nil;
    }
    return [[NSDate alloc] initWithTimeIntervalSince1970: ((NSTimeInterval)accountMetadata.lastHealthCheckup) / 1000.0];
}

- (void)requestTrustedDeviceListRefresh
{
    [self.stateMachine handleFlag:OctagonFlagFetchAuthKitMachineIDList];
}

#pragma mark --- Device Info update handling

- (void)deviceNameUpdated {
    secnotice("octagon-devicename", "device name updated: %@", self.contextID);
    OctagonPendingFlag *pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:OctagonFlagCuttlefishNotification
                                                                    conditions:OctagonPendingConditionsDeviceUnlocked];
    [self.stateMachine handlePendingFlag:pendingFlag];
}

#pragma mark --- SOS update handling


- (void)selfPeerChanged:(id<CKKSPeerProvider>)provider
{
    // Currently, we register for peer changes with just our SOS peer adapter, so the only reason this is called is to receive SOS updates
    // Ignore SOS self peer updates for now.
}

- (void)trustedPeerSetChanged:(id<CKKSPeerProvider>)provider
{
    // Currently, we register for peer changes with just our SOS peer adapter, so the only reason this is called is to receive SOS updates
    secnotice("octagon-sos", "Received an update of an SOS trust set change");

    if(!self.sosAdapter.sosEnabled) {
        secnotice("octagon-sos", "This platform doesn't support SOS. This is probably a bug?");
    }

    if (self.sosConsistencyRateLimiter == nil) {
        secnotice("octagon", "creating SOS consistency rate limiter");
        dispatch_time_t initialDelay = (SecCKKSReduceRateLimiting() ? 200 * NSEC_PER_MSEC : 2 * NSEC_PER_SEC);
        dispatch_time_t maximumDelay = (SecCKKSReduceRateLimiting() ? 10 * NSEC_PER_SEC : 30 * NSEC_PER_SEC);

        WEAKIFY(self);

        void (^block)(void) = ^{
            STRONGIFY(self);
            [self.stateMachine handleFlag:OctagonFlagAttemptSOSConsistency];
        };

        self.sosConsistencyRateLimiter = [[CKKSNearFutureScheduler alloc] initWithName:@"sos-consistency-ratelimiter"
                                                                          initialDelay:initialDelay
                                                                    exponentialBackoff:2
                                                                          maximumDelay:maximumDelay
                                                                      keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionPendingZoneChangeFetchScheduling
                                                                                 block:block];
    }

    [self.sosConsistencyRateLimiter trigger];
}

#pragma mark --- External Interfaces

//Check for account
- (CKKSAccountStatus)checkForCKAccount:(OTOperationConfiguration * _Nullable)configuration {

#if TARGET_OS_WATCH || TARGET_OS_TV
    // Watches and other devices can be very, very slow getting the CK account state
    uint64_t timeout = (45 * NSEC_PER_SEC);
#else
    uint64_t timeout = (5 * NSEC_PER_SEC);
#endif
    if (configuration.timeoutWaitForCKAccount != 0) {
        // We will wait on this timeout up to twice. Halve it!
        timeout = configuration.timeoutWaitForCKAccount / 2;
    }
    if (timeout) {
        /* wait if account is not present yet */
        if([self.cloudKitAccountStateKnown wait:timeout] != 0) {
            secnotice("octagon-ck", "Unable to determine CloudKit account state?");
            // Fall through so that the retry below will ask for a new CK account state, if needed
        }
    }

    __block bool haveAccount = false;
    __block bool accountStatusKnown = false;
    dispatch_sync(self.queue, ^{
        accountStatusKnown = (self.cloudKitAccountInfo != nil);
        haveAccount = (self.cloudKitAccountInfo != nil) && self.cloudKitAccountInfo.accountStatus == CKKSAccountStatusAvailable;
    });

    if(!accountStatusKnown || !haveAccount) {
        // Right after account sign-in, it's possible that the CK account exists, but that we just haven't learned about
        // it yet, and still have the 'no account' state cached. So, let's check in...
        secnotice("octagon-ck", "No CK account present(%@). Attempting to refetch CK account status...", self.contextID);
        if(![self.accountStateTracker notifyCKAccountStatusChangeAndWait:timeout]) {
            secnotice("octagon-ck", "Fetching new CK account status did not complete in time");
        }

        // After the above call finishes, we should have a fresh value in self.cloudKitAccountInfo
        dispatch_sync(self.queue, ^{
            accountStatusKnown = (self.cloudKitAccountInfo != nil);
            haveAccount = (self.cloudKitAccountInfo != nil) && self.cloudKitAccountInfo.accountStatus == CKKSAccountStatusAvailable;
        });
        secnotice("octagon-ck", "After refetch, CK account status(%@) is %@", self.contextID, haveAccount ? @"present" : @"missing");
    }

    if(!accountStatusKnown) {
        return CKKSAccountStatusUnknown;
    }
    return haveAccount ? CKKSAccountStatusAvailable : CKKSAccountStatusNoAccount;
}

// This will return some error if there is no valid CK account.
- (NSError* _Nullable)errorIfNoCKAccount:(OTOperationConfiguration * _Nullable)configuration
{
    CKKSAccountStatus accountStatus = [self checkForCKAccount:configuration];

    switch(accountStatus) {
        case CKKSAccountStatusAvailable:
            return nil;
        case CKKSAccountStatusUnknown:
            return [NSError errorWithDomain:OctagonErrorDomain
                                       code:OctagonErrorICloudAccountStateUnknown
                                description:@"Cannot determine iCloud Account state; try again later"];
        case CKKSAccountStatusNoAccount:
            return [NSError errorWithDomain:OctagonErrorDomain
                                       code:OctagonErrorNotSignedIn
                                description:@"User is not signed into iCloud."];
    }

    return nil;
}
// Acceptor interfaces
- (void)rpcEpoch:(void (^)(uint64_t epoch,
                           NSError * _Nullable error))reply
{

    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("rpc-epoch", "No cloudkit account present: %@", accountError);
        reply(0, accountError);
        return;
    }

    secnotice("rpc-epoch", "Fetching epoch");

    if (self.lockStateTracker) {
        [self.lockStateTracker recheck];
    }

    [self.cuttlefishXPCWrapper fetchEgoEpochWithSpecificUser:self.activeAccount
                                                       reply:^(uint64_t epoch, NSError* _Nullable fetchError) {
        if (fetchError) {
            secerror("rpc-epoch: failed to fetch epoch! error: %@", fetchError);
        } else {
            secnotice("rpc-epoch","fetched epoch");
        }
        reply(epoch, fetchError);
    }];
}

- (void)rpcVoucherWithConfiguration:(NSString*)peerID
                      permanentInfo:(NSData *)permanentInfo
                   permanentInfoSig:(NSData *)permanentInfoSig
                         stableInfo:(NSData *)stableInfo
                      stableInfoSig:(NSData *)stableInfoSig
                              reply:(void (^)(NSData* _Nullable voucher, NSData* _Nullable voucherSig, NSError * _Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("rpc-vouch", "No cloudkit account present: %@", accountError);
        reply(nil, nil, accountError);
        return;
    }

    secnotice("rpc-vouch", "Creating voucher");

    if (self.lockStateTracker) {
        [self.lockStateTracker recheck];
    }

    OTUpdateTrustedDeviceListOperation *updateTDLOperation = [[OTUpdateTrustedDeviceListOperation alloc] initWithDependencies:self.operationDependencies
                                                                                                                intendedState:OctagonStateBecomeReady
                                                                                                             listUpdatesState:OctagonStateBecomeReady
                                                                                                                   errorState:OctagonStateBecomeReady
                                                                                                                    retryFlag:nil];

    OctagonStateTransitionRequest<OTUpdateTrustedDeviceListOperation*>* updateTDLRequest = [[OctagonStateTransitionRequest alloc] init:@"updateTDL"
                                                                                                                          sourceStates:[OTStates OctagonReadyStates]
                                                                                                                           serialQueue:self.queue
                                                                                                                          transitionOp:updateTDLOperation];


    OTPairingVoucherOperation* vouchOperation = [[OTPairingVoucherOperation alloc] initWithDependencies:self.operationDependencies
                                                                                          intendedState:OctagonStateBecomeReady
                                                                                             errorState:OctagonStateBecomeReady
                                                                                             deviceInfo:[self prepareInformation]
                                                                                                 peerID:peerID
                                                                                          permanentInfo:permanentInfo
                                                                                       permanentInfoSig:permanentInfoSig
                                                                                             stableInfo:stableInfo
                                                                                          stableInfoSig:stableInfoSig];


    OctagonStateTransitionRequest<OTPairingVoucherOperation*>* vouchRequest = [[OctagonStateTransitionRequest alloc] init:@"rpcVoucher"
                                                                                                        sourceStates:[OTStates OctagonReadyStates]
                                                                                                         serialQueue:self.queue
                                                                                                        transitionOp:vouchOperation];
    CKKSResultOperation* callbackForVoucher = [CKKSResultOperation named:@"rpcVoucher-callback"
                                                               withBlock:^{
        secnotice("otrpc", "Returning a voucher call: %@, %@, %@", vouchOperation.voucher, vouchOperation.voucherSig, vouchOperation.error);
        reply(vouchOperation.voucher, vouchOperation.voucherSig, vouchOperation.error);
    }];


    WEAKIFY(self);
    CKKSResultOperation* callbackForUpdateTDL = [CKKSResultOperation named:@"updateTDL-callback"
                                                                 withBlock:^{
        STRONGIFY(self);
        secnotice("otrpc", "Returning a updateTDL: %@", updateTDLOperation.error);
        if (updateTDLOperation.error) {
            reply(nil, nil, updateTDLOperation.error);
        } else {
            [self.operationQueue addOperation:callbackForVoucher];
            [self.stateMachine handleExternalRequest:vouchRequest
                                        startTimeout:(SecCKKSTestsEnabled() ? OctagonStateTransitionTimeoutForTests : OctagonStateTransitionTimeoutForLongOps)];
        }
    }];

    [callbackForUpdateTDL addDependency:updateTDLOperation];
    [self.operationQueue addOperation:callbackForUpdateTDL];

    [vouchOperation addDependency:callbackForUpdateTDL];
    [callbackForVoucher addDependency:vouchOperation];

    [self.stateMachine handleExternalRequest:updateTDLRequest
                                startTimeout:(SecCKKSTestsEnabled() ? OctagonStateTransitionTimeoutForTests : OctagonStateTransitionTimeoutForLongOps)];
}

//Initiator interfaces

- (void)rpcPrepareIdentityAsApplicantWithConfiguration:(OTJoiningConfiguration*)config
                                              epoch:(uint64_t)epoch
                                              reply:(void (^)(NSString * _Nullable peerID,
                                                              NSData * _Nullable permanentInfo,
                                                              NSData * _Nullable permanentInfoSig,
                                                              NSData * _Nullable stableInfo,
                                                              NSData * _Nullable stableInfoSig,
                                                              NSError * _Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(NULL, NULL, NULL, NULL, NULL, accountError);
        return;
    }

    secnotice("otrpc", "Preparing identity as applicant");

    if (self.lockStateTracker) {
        [self.lockStateTracker recheck];
    }

    OTPrepareOperation* pendingOp = [[OTPrepareOperation alloc] initWithDependencies:self.operationDependencies
                                                                       intendedState:OctagonStateInitiatorAwaitingVoucher
                                                                          errorState:OctagonStateBecomeUntrusted
                                                                          deviceInfo:[self prepareInformation]
                                                                      policyOverride:self.policyOverride
                                                                     accountSettings:self.accountSettings
                                                                               epoch:epoch];
   
    BOOL isSlowerDevice = [self.deviceAdapter isWatch] || [self.deviceAdapter isAppleTV] || [self.deviceAdapter isHomePod];

    dispatch_time_t timeOut = 0;
    if (config.timeout != 0) {
        timeOut = config.timeout;
    } else if(isSlowerDevice){
        // Non-iphone non-mac platforms can be slow; heuristically slow them down
        timeOut = 60 * NSEC_PER_SEC;
    } else {
        timeOut = 10 * NSEC_PER_SEC;
    }

    OctagonStateTransitionRequest<OTPrepareOperation*>* request = [[OctagonStateTransitionRequest alloc] init:@"prepareForApplicant"
                                                                                                 sourceStates:[NSSet setWithArray:@[OctagonStateUntrusted,
                                                                                                                                    OctagonStateWaitForCDP,
                                                                                                                                    OctagonStateWaitingForCloudKitAccount,
                                                                                                                                    OctagonStateDetermineiCloudAccountState,
                                                                                                                                    OctagonStateNoAccount,
                                                                                                                                    OctagonStateMachineNotStarted]]
                                                                                                  serialQueue:self.queue
                                                                                                 transitionOp:pendingOp];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"rpcPrepare-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning a prepare call: %@  %@", pendingOp.peerID, pendingOp.error);
                                                         reply(pendingOp.peerID,
                                                               pendingOp.permanentInfo,
                                                               pendingOp.permanentInfoSig,
                                                               pendingOp.stableInfo,
                                                               pendingOp.stableInfoSig,
                                                               pendingOp.error);
                                                     }];
    [callback addDependency:pendingOp];
    [self.operationQueue addOperation: callback];

    [self.stateMachine handleExternalRequest:request
                                startTimeout:timeOut];

    return;
}

- (void)joinWithBottle:(NSString*)bottleID
               entropy:(NSData *)entropy
            bottleSalt:(NSString *)bottleSalt
                 reply:(void (^)(NSError * _Nullable error))reply
{
    _bottleID = bottleID;
    _entropy = entropy;
    _bottleSalt = bottleSalt;
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];

    NSError* accountError = [self errorIfNoCKAccount:configuration];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:@{
        OctagonStateBottleJoinCreateIdentity: @{
            OctagonStateBottleJoinVouchWithBottle: [self joinStatePathDictionary],
        },
    }];

    [self.stateMachine doWatchedStateMachineRPC:@"rpc-join-with-bottle"
                                   sourceStates:[OTStates OctagonInAccountStates]
                                           path:path
                                          reply:^(NSError* _Nullable returningError) {
        if(returningError == nil) {
            [self.notifierClass post:OTJoinedViaBottle];
        }
        reply(returningError);
    }];
}

-(void)joinWithRecoveryKey:(NSString*)recoveryKey
                     reply:(void (^)(NSError * _Nullable error))reply
{
    self.recoveryKey = recoveryKey;
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];

    NSError* accountError = [self errorIfNoCKAccount:configuration];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:@{
          OctagonStateStashAccountSettingsForRecoveryKey: @{
              OctagonStateCreateIdentityForRecoveryKey: @{
                  OctagonStateVouchWithRecoveryKey: [self joinStatePathDictionary],
                        },
                    },
                }];

    [self.stateMachine doWatchedStateMachineRPC:@"rpc-join-with-recovery-key"
                                   sourceStates:[OTStates OctagonInAccountStates]
                                           path:path
                                          reply:reply];
}

- (void)joinWithCustodianRecoveryKey:(OTCustodianRecoveryKey*)crk
                              reply:(void (^)(NSError * _Nullable error))reply
{
    self.custodianRecoveryKey = crk;
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];

    NSError* accountError = [self errorIfNoCKAccount:configuration];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:@{
        OctagonStateCreateIdentityForCustodianRecoveryKey: @{
            OctagonStateVouchWithCustodianRecoveryKey: [self joinStatePathDictionary],
        },
    }];

    [self.stateMachine doWatchedStateMachineRPC:@"rpc-join-with-custodian-recovery-key"
                                   sourceStates:[OTStates OctagonInAccountStates]
                                           path:path
                                          reply:reply];
}

- (void)preflightJoinWithCustodianRecoveryKey:(OTCustodianRecoveryKey*)crk
                                        reply:(void (^)(NSError * _Nullable error))reply
{
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];

    NSError* accountError = [self errorIfNoCKAccount:configuration];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    NSString* altDSID = self.activeAccount.altDSID;
    if(altDSID == nil) {
        secnotice("authkit", "No configured altDSID: %@", self.activeAccount);
        reply([NSError errorWithDomain:OctagonErrorDomain
                                  code:OctagonErrorNoAppleAccount
                           description:@"No altDSID configured"]);
        return;
    }

    NSString* salt = altDSID;
    TrustedPeersHelperCustodianRecoveryKey* tphcrk = [[TrustedPeersHelperCustodianRecoveryKey alloc] initWithUUID:crk.uuid.UUIDString
                                                                                                    encryptionKey:nil
                                                                                                       signingKey:nil
                                                                                                   recoveryString:crk.recoveryString
                                                                                                             salt:salt
                                                                                                             kind:TPPBCustodianRecoveryKey_Kind_RECOVERY_KEY];

    OTPreflightVouchWithCustodianRecoveryKeyOperation* op = [[OTPreflightVouchWithCustodianRecoveryKeyOperation alloc] initWithDependencies:self.operationDependencies
                                                                                                                              intendedState:OctagonStateBecomeReady
                                                                                                                                 errorState:OctagonStateBecomeReady
                                                                                                                                     tphcrk:tphcrk];

    NSSet<OctagonState*>* sourceStates = [OTStates OctagonReadyStates];
    [self.stateMachine doSimpleStateMachineRPC:@"preflight-custodian-recovery-key" op:op sourceStates:sourceStates reply:reply];
}

-(void)joinWithInheritanceKey:(OTInheritanceKey*)ik
                        reply:(void (^)(NSError * _Nullable error))reply
{
    self.inheritanceKey = ik;
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];

    NSError* accountError = [self errorIfNoCKAccount:configuration];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:@{
        OctagonStatePrepareAndRecoverTLKSharesForInheritancePeer: @{
            OctagonStateBecomeInherited: @{
                OctagonStateInherited: [OctagonStateTransitionPathStep success],
            },
        },
    }];

    [self.stateMachine doWatchedStateMachineRPC:@"rpc-join-with-inheritance-key"
                                   sourceStates:[OTStates OctagonInAccountStates]
                                           path:path
                                          reply:reply];
}

-(void)preflightJoinWithInheritanceKey:(OTInheritanceKey*)ik
                                 reply:(void (^)(NSError * _Nullable error))reply
{
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];

    NSError* accountError = [self errorIfNoCKAccount:configuration];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    NSError* error = nil;
    OTCustodianRecoveryKey* crk = [[OTCustodianRecoveryKey alloc] initWithUUID:ik.uuid recoveryString:[ik.recoveryKeyData base64EncodedStringWithOptions:0] error:&error];
    if (crk == nil) {
        secerror("octagon-inheritance: failed to create CRK: %@", error);
        reply(error);
        return;
    }
    TrustedPeersHelperCustodianRecoveryKey* tphcrk = [[TrustedPeersHelperCustodianRecoveryKey alloc] initWithUUID:crk.uuid.UUIDString
                                                                                                    encryptionKey:nil
                                                                                                       signingKey:nil
                                                                                                   recoveryString:crk.recoveryString
                                                                                                             salt:@""
                                                                                                             kind:TPPBCustodianRecoveryKey_Kind_INHERITANCE_KEY];
    OTPreflightVouchWithCustodianRecoveryKeyOperation* op = [[OTPreflightVouchWithCustodianRecoveryKeyOperation alloc] initWithDependencies:self.operationDependencies
                                                                                                                              intendedState:OctagonStateBecomeReady
                                                                                                                                 errorState:OctagonStateBecomeReady
                                                                                                                                     tphcrk:tphcrk];
    NSSet<OctagonState*>* sourceStates = [OTStates OctagonReadyStates];
    [self.stateMachine doSimpleStateMachineRPC:@"preflight-inheritance-recovery-key" op:op sourceStates:sourceStates reply:reply];
}

- (void)preflightRecoverOctagonUsingRecoveryKey:(NSString *)recoveryKey
                                          reply:(void (^)(BOOL, NSError * _Nullable))reply

{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon-preflight-rk", "No cloudkit account present: %@", accountError);
        reply(NO, accountError);
        return;
    }
    
    
    NSString* altDSID = self.activeAccount.altDSID;
    if(altDSID == nil) {
        secnotice("octagon-preflight-rk", "No configured altDSID: %@", self.activeAccount);
        reply(NO, [NSError errorWithDomain:OctagonErrorDomain
                                      code:OctagonErrorNoAppleAccount
                               description:@"No altDSID configured"]);
        return;
    }
    
    NSString* salt = altDSID;
    
    [self.cuttlefishXPCWrapper preflightRecoverOctagonUsingRecoveryKey:self.activeAccount
                                                           recoveryKey:recoveryKey
                                                                  salt:salt
                                                                 reply:^(BOOL correct,
                                                                         NSError * _Nullable preflightError) {
        if(preflightError) {
            secerror("octagon-preflight-rk: error checking recovery key correctness: %@", preflightError);
            reply(NO, preflightError);
        } else {
            secnotice("octagon-preflight-rk", "recovery key is %@", correct ? @"correct" : @"incorrect");
            reply(correct, nil);
        }
    }];
}

- (NSDictionary*)joinStatePathDictionary
{
    return @{
        OctagonStateInitiatorSetCDPBit: @{
                OctagonStateInitiatorUpdateDeviceList: @{
                        OctagonStateInitiatorJoin: @{
                                OctagonStateBottlePreloadOctagonKeysInSOS: @{
                                        OctagonStateJoinSOSAfterCKKSFetch: @{
                                                OctagonStateSetAccountSettings: @{
                                                        OctagonStateBecomeReady: @{
                                                                OctagonStateReady: [OctagonStateTransitionPathStep success],
                                                        },
                                                },
                                        },
                                        OctagonStateSetAccountSettings: @{
                                                OctagonStateBecomeReady: @{
                                                        OctagonStateReady: [OctagonStateTransitionPathStep success],
                                                },
                                        },
                                },
                                OctagonStateInitiatorJoinCKKSReset: @{
                                        OctagonStateInitiatorJoinAfterCKKSReset: @{
                                                OctagonStateBottlePreloadOctagonKeysInSOS: @{
                                                        OctagonStateJoinSOSAfterCKKSFetch: @{
                                                                OctagonStateSetAccountSettings: @{
                                                                        OctagonStateBecomeReady: @{
                                                                                OctagonStateReady: [OctagonStateTransitionPathStep success]
                                                                        },
                                                                },
                                                        },
                                                        OctagonStateSetAccountSettings: @{
                                                                OctagonStateBecomeReady: @{
                                                                        OctagonStateReady: [OctagonStateTransitionPathStep success]
                                                                },
                                                        },
                                                },
                                        },
                                },
                        },
                },
        },
    };
}

- (void)rpcJoin:(NSData*)vouchData
       vouchSig:(NSData*)vouchSig
          reply:(void (^)(NSError * _Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    NSError* metadataError = nil;
    [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
        metadata.voucher = vouchData;
        metadata.voucherSignature = vouchSig;
        return metadata;
    } error:&metadataError];
    if(metadataError) {
        secnotice("octagon", "Unable to save voucher for joining: %@", metadataError);
        reply(metadataError);
        return;
    }

    NSSet<OctagonState*>* sourceStates = [NSSet setWithObjects:OctagonStateInitiatorAwaitingVoucher, OctagonStateUntrusted, nil];

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:[self joinStatePathDictionary]];

    [self.stateMachine doWatchedStateMachineRPC:@"rpc-join"
                                   sourceStates:sourceStates
                                           path:path
                                          reply:^(NSError *error) {
        if (error) {
            secerror("octagon failed to join: %@", error);
        } else {
            [self.ckks rpcFetchBecause:CKKSFetchBecauseOctagonPairingComplete];
        }
        reply(error);
    }];
}

- (NSDictionary *)ckksPeerStatus:(id<CKKSPeer>)peer
{
    NSMutableDictionary *peerStatus = [NSMutableDictionary dictionary];

    if (peer.peerID) {
        peerStatus[@"peerID"] = peer.peerID;
    }
    NSData *spki = peer.publicSigningKey.encodeSubjectPublicKeyInfo;
    if (spki) {
        peerStatus[@"signingSPKI"] = [spki base64EncodedStringWithOptions:0];
        peerStatus[@"signingSPKIHash"] = [TPHashBuilder hashWithAlgo:kTPHashAlgoSHA256 ofData:spki];
    }
    return peerStatus;
}

- (NSArray *)sosTrustedPeersStatus
{
    NSError *localError = nil;
    NSSet<id<CKKSRemotePeerProtocol>>* _Nullable peers = [self.sosAdapter fetchTrustedPeers:&localError];
    if (peers == nil || localError) {
        secnotice("octagon", "No SOS peers present: %@, skipping in status", localError);
        return nil;
    }
    NSMutableArray<NSDictionary *>* trustedSOSPeers = [NSMutableArray array];

    for (id<CKKSPeer> peer in peers) {
        NSDictionary *peerStatus = [self ckksPeerStatus:peer];
        if (peerStatus) {
            [trustedSOSPeers addObject:peerStatus];
        }
    }
    return trustedSOSPeers;
}

- (NSDictionary *)sosSelvesStatus
{
    NSError *localError = nil;

    CKKSSelves* selves = [self.sosAdapter fetchSelfPeers:&localError];
    if (selves == nil || localError) {
        secnotice("octagon", "No SOS selves present: %@, skipping in status", localError);
        return nil;
    }
    NSMutableDictionary* selvesSOSPeers = [NSMutableDictionary dictionary];

    selvesSOSPeers[@"currentSelf"] = [self ckksPeerStatus:selves.currentSelf];

    /*
     * If we have past selves, include them too
     */
    NSMutableSet* pastSelves = [selves.allSelves mutableCopy];
    [pastSelves removeObject:selves.currentSelf];
    if (pastSelves.count) {
        NSMutableArray<NSDictionary *>* pastSelvesStatus = [NSMutableArray array];

        for (id<CKKSPeer> peer in pastSelves) {
            NSDictionary *peerStatus = [self ckksPeerStatus:peer];
            if (peerStatus) {
                [pastSelvesStatus addObject:peerStatus];
            }
        }
        selvesSOSPeers[@"pastSelves"] = pastSelvesStatus;
    }
    return selvesSOSPeers;
}

- (void)rpcStatus:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply
{
    __block NSMutableDictionary* result = [NSMutableDictionary dictionary];

    result[@"containerName"] = self.containerName;
    result[@"contextID"] = self.contextID;
    result[@"activeAccount"] = [self.activeAccount description];

    OTOperationConfiguration* timeoutContext = [[OTOperationConfiguration alloc] init];
    timeoutContext.timeoutWaitForCKAccount = 2 * NSEC_PER_SEC;

    NSError* accountError = [self errorIfNoCKAccount:timeoutContext];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(nil, accountError);
        return;
    }

    if([self.stateMachine.paused wait:3*NSEC_PER_SEC] != 0) {
        secnotice("octagon", "Returning status of unpaused state machine for container (%@) and context (%@)", self.containerName, self.contextID);
        result[@"stateUnpaused"] = @1;
    }

    // This will try to allow the state machine to pause
    result[@"state"] = self.stateMachine.currentState;
    result[@"statePendingFlags"] = [self.stateMachine dumpPendingFlags];
    result[@"stateFlags"] = [self.stateMachine.flags dumpFlags];

    NSError* metadataError = nil;
    OTAccountMetadataClassC* currentAccountMetadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&metadataError];
    if(metadataError) {
        secnotice("octagon", "Failed to load account metaada for container (%@) and context (%@): %@", self.containerName, self.contextID, metadataError);
    }

    result[@"memoizedTrustState"] = @(currentAccountMetadata.trustState);
    result[@"memoizedAccountState"] = @(currentAccountMetadata.icloudAccountState);
    result[@"memoizedCDPStatus"] = @(currentAccountMetadata.cdpState);
    result[@"octagonLaunchSeqence"] = [self.launchSequence eventsByTime];

    NSDate* lastHealthCheck = self.currentMemoizedLastHealthCheck;
    result[@"memoizedlastHealthCheck"] = lastHealthCheck ?: @"Never checked";
    if (self.sosAdapter.sosEnabled) {
        result[@"sosTrustedPeersStatus"] = [self sosTrustedPeersStatus];
        result[@"sosSelvesStatus"] = [self sosSelvesStatus];
    }

    {
        NSError *error;
        id<SecEscrowRequestable> request = [self.escrowRequestClass request:&error];
        result[@"escrowRequest"] = [request fetchStatuses:&error];
    }

    result[@"CoreFollowUp"] = [self.followupHandler sysdiagnoseStatus];

    result[@"lastOctagonPush"] = [[CKKSAnalytics logger] datePropertyForKey:CKKSAnalyticsLastOctagonPush];
    result[@"pushEnvironments"] = [self.apsReceiver registeredPushEnvironments];

    [self.cuttlefishXPCWrapper dumpWithSpecificUser:self.activeAccount
                                              reply:^(NSDictionary * _Nullable dump, NSError * _Nullable dumpError) {
            secnotice("octagon", "Finished dump for status RPC");
            if(dumpError) {
                result[@"contextDumpError"] = [SecXPCHelper cleanseErrorForXPC:dumpError];
            } else {
                result[@"contextDump"] = dump;
            }
            reply(result, nil);
        }];
}

- (void)rpcFetchEgoPeerID:(void (^)(NSString* peerID, NSError* error))reply
{
    // We've memoized this peer ID. Use the memorized version...
    NSError* localError = nil;
    NSString* peerID = [self.accountMetadataStore getEgoPeerID:&localError];

    if(peerID) {
        secnotice("octagon", "Returning peer ID: %@", peerID);
    } else {
        secnotice("octagon", "Unable to fetch peer ID: %@", localError);
    }
    reply(peerID, localError);
}

- (void)rpcFetchPeerAttributes:(NSString*)attribute includeSelf:(BOOL)includeSelf reply:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(nil, accountError);
        return;
    }

    // As this isn't a state-modifying operation, we don't need to go through the state machine.
    [self.cuttlefishXPCWrapper dumpWithSpecificUser:self.activeAccount
                                              reply:^(NSDictionary * _Nullable dump, NSError * _Nullable dumpError) {
            // Pull out our peers
            if(dumpError) {
                secnotice("octagon", "Unable to dump info: %@", dumpError);
                reply(nil, dumpError);
                return;
            }

            NSMutableDictionary<NSString*, NSString*>* peerMap = [NSMutableDictionary dictionary];

            NSDictionary* selfInfo = dump[@"self"];

            if ([attribute isEqualToString:@"bottleID"]) {
                NSArray<NSDictionary*>*bottles = dump[@"bottles"];
                for(NSDictionary *bottle in bottles) {
                    peerMap[bottle[@"peerID"]] = bottle[@"bottleID"];
                }
                reply(peerMap, nil);
                return;
            }

            NSArray* peers = dump[@"peers"];
            NSArray* trustedPeerIDs = selfInfo[@"dynamicInfo"][@"included"];


            for(NSString* peerID in trustedPeerIDs) {
                NSDictionary* peerMatchingID = nil;

                for(NSDictionary* peer in peers) {
                    if([peer[@"peerID"] isEqualToString:peerID]) {
                        peerMatchingID = peer;
                        break;
                    }
                }

                if(!peerMatchingID) {
                    secerror("octagon: have a trusted peer ID without peer information: %@", peerID);
                    continue;
                }

                peerMap[peerID] = peerMatchingID[@"stableInfo"][attribute];
            }

            reply(peerMap, nil);
        }];
}

- (void)rpcFetchDeviceNamesByPeerID:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply
{
    [self rpcFetchPeerAttributes:@"device_name" includeSelf:NO reply:reply];
}


- (void)rpcFetchPeerIDByBottleID:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply
{
    [self rpcFetchPeerAttributes:@"bottleID" includeSelf:YES reply:reply];
}

- (void)rpcSetRecoveryKey:(NSString*)recoveryKey reply:(void (^)(NSError * _Nullable error))reply
{
    if (self.lockStateTracker) {
        [self.lockStateTracker recheck];
    }

    OTSetRecoveryKeyOperation *pendingOp = [[OTSetRecoveryKeyOperation alloc] initWithDependencies:self.operationDependencies
                                                                                       recoveryKey:recoveryKey];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"setRecoveryKey-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning a set recovery key call: %@", pendingOp.error);
                                                         reply(pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation:callback];
    [self.operationQueue addOperation:pendingOp];
}

- (void)rpcIsRecoveryKeySet:(void (^)(BOOL isSet, NSError * _Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(NO, accountError);
        return;
    }

    [self.cuttlefishXPCWrapper isRecoveryKeySet:self.activeAccount reply:^(BOOL isSet, NSError * _Nullable error) {
        reply(isSet, error);
    }];
}

- (void)rpcRemoveRecoveryKey:(void (^)(BOOL removed, NSError * _Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(NO, accountError);
        return;
    }

    [self.cuttlefishXPCWrapper removeRecoveryKey:self.activeAccount reply:^(BOOL removed, NSError * _Nullable removedError) {
        reply(removed, removedError);
    }];
}

- (void)areRecoveryKeysDistrusted:(void (^)(BOOL, NSError *_Nullable))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(NO, accountError);
        return;
    }
    
    [self.cuttlefishXPCWrapper octagonContainsDistrustedRecoveryKeysWithSpecificUser:self.activeAccount reply:^(BOOL containsDistrusted, NSError * _Nullable rkError) {
        reply(containsDistrusted, rkError);
    }];
}

- (void)rpcCreateCustodianRecoveryKeyWithUUID:(NSUUID *_Nullable)uuid
                                        reply:(void (^)(OTCustodianRecoveryKey *_Nullable crk, NSError *_Nullable error))reply
{
    OTCreateCustodianRecoveryKeyOperation *pendingOp = [[OTCreateCustodianRecoveryKeyOperation alloc] initWithUUID:uuid dependencies:self.operationDependencies];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"createCustodianRecoveryKey-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning a create custodian recovery key call: %@", pendingOp.error);
                                                         reply(pendingOp.crk, pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation:callback];
    [self.operationQueue addOperation:pendingOp];
}

- (void)rpcRemoveCustodianRecoveryKeyWithUUID:(NSUUID *)uuid
                                        reply:(void (^)(NSError *_Nullable error))reply
{
    OTRemoveCustodianRecoveryKeyOperation *pendingOp = [[OTRemoveCustodianRecoveryKeyOperation alloc] initWithUUID:uuid dependencies:self.operationDependencies];
    CKKSResultOperation* callback = [CKKSResultOperation named:@"removeCustodianRecoveryKey-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning a remove custodian recovery key call: %@", pendingOp.error);
                                                         reply(pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation:callback];
    [self.operationQueue addOperation:pendingOp];
}

- (void)rpcCheckCustodianRecoveryKeyWithUUID:(NSUUID *)uuid
                                       reply:(void (^)(bool exists, NSError *_Nullable error))reply
{
    OTFindCustodianRecoveryKeyOperation *pendingOp = [[OTFindCustodianRecoveryKeyOperation alloc] initWithUUID:uuid dependencies:self.operationDependencies];
    CKKSResultOperation* callback = [CKKSResultOperation named:@"checkCustodianRecoveryKey-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning a check custodian recovery key call: %@, %@", pendingOp.crk, pendingOp.error);
                                                         reply(pendingOp.crk != nil && pendingOp.crk.kind == TPPBCustodianRecoveryKey_Kind_RECOVERY_KEY,
                                                               pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation:callback];
    [self.operationQueue addOperation:pendingOp];
}

- (void)rpcCreateInheritanceKeyWithUUID:(NSUUID *_Nullable)uuid
                                  reply:(void (^)(OTInheritanceKey *_Nullable ik, NSError *_Nullable error))reply
{
    OTCreateInheritanceKeyOperation *pendingOp = [[OTCreateInheritanceKeyOperation alloc] initWithUUID:uuid dependencies:self.operationDependencies];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"createInheritanceKey-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning an inheritance key call: %@", pendingOp.error);
                                                         reply(pendingOp.ik, pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation:callback];
    [self.operationQueue addOperation:pendingOp];

}

- (void)rpcGenerateInheritanceKeyWithUUID:(NSUUID *_Nullable)uuid
                                  reply:(void (^)(OTInheritanceKey *_Nullable ik, NSError *_Nullable error))reply
{
    if (uuid == nil) {
        uuid = [[NSUUID alloc] init];
    }
    NSError *error = nil;
    OTInheritanceKey *ik = [[OTInheritanceKey alloc] initWithUUID:uuid error:&error];
    if (ik == nil) {
        secerror("octagon: failed to generate inheritance key: %@", error);
        reply(nil, error);
        return;
    }
    reply(ik, nil);
}

- (void)rpcStoreInheritanceKeyWithIK:(OTInheritanceKey*)ik
                               reply:(void (^)(NSError *_Nullable error))reply
{
    OTStoreInheritanceKeyOperation *pendingOp = [[OTStoreInheritanceKeyOperation alloc] initWithIK:ik dependencies:self.operationDependencies];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"storeInheritanceKey-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning an inheritance key call: %@", pendingOp.error);
                                                         reply(pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation:callback];
    [self.operationQueue addOperation:pendingOp];
}

- (void)rpcRemoveInheritanceKeyWithUUID:(NSUUID *)uuid
                                  reply:(void (^)(NSError *_Nullable error))reply
{
    OTRemoveCustodianRecoveryKeyOperation *pendingOp = [[OTRemoveCustodianRecoveryKeyOperation alloc] initWithUUID:uuid dependencies:self.operationDependencies];
    CKKSResultOperation* callback = [CKKSResultOperation named:@"removeInheritanceKey-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning remove inheritance key call: %@", pendingOp.error);
                                                         reply(pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation:callback];
    [self.operationQueue addOperation:pendingOp];
}

- (void)rpcCheckInheritanceKeyWithUUID:(NSUUID *)uuid
                                       reply:(void (^)(bool exists, NSError *_Nullable error))reply
{
    OTFindCustodianRecoveryKeyOperation *pendingOp = [[OTFindCustodianRecoveryKeyOperation alloc] initWithUUID:uuid dependencies:self.operationDependencies];
    CKKSResultOperation* callback = [CKKSResultOperation named:@"checkInheritanceKey-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning a check inheritance key call: %@, %@", pendingOp.crk, pendingOp.error);
        reply(pendingOp.crk != nil && pendingOp.crk.kind == TPPBCustodianRecoveryKey_Kind_INHERITANCE_KEY,
                                                               pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation:callback];
    [self.operationQueue addOperation:pendingOp];
}

- (void)rpcRecreateInheritanceKeyWithUUID:(NSUUID *_Nullable)uuid
                                    oldIK:(OTInheritanceKey *)oldIK
                                    reply:(void (^)(OTInheritanceKey *_Nullable ik, NSError *_Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(nil, accountError);
        return;
    }
    
    OTRecreateInheritanceKeyOperation *pendingOp = [[OTRecreateInheritanceKeyOperation alloc] initWithUUID:uuid oldIK:oldIK dependencies:self.operationDependencies];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"recreateInheritanceKey-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning an inheritance key call: %@", pendingOp.error);
                                                         reply(pendingOp.ik, pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation:callback];
    [self.operationQueue addOperation:pendingOp];
}

- (void)rpcCreateInheritanceKeyWithUUID:(NSUUID *_Nullable)uuid
                         claimTokenData:(NSData *)claimTokenData
                        wrappingKeyData:(NSData *)wrappingKeyData
                                  reply:(void (^)(OTInheritanceKey *_Nullable crk, NSError *_Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(nil, accountError);
        return;
    }
    
    OTCreateInheritanceKeyWithClaimTokenAndWrappingKey *pendingOp = [[OTCreateInheritanceKeyWithClaimTokenAndWrappingKey alloc] initWithUUID:uuid
                                                                                                                              claimTokenData:claimTokenData
                                                                                                                             wrappingKeyData:wrappingKeyData
                                                                                                                                dependencies:self.operationDependencies];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"createInheritanceKeyWithClaimTokenAndWrappingKey-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning an inheritance key call: %@", pendingOp.error);
                                                         reply(pendingOp.ik, pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation:callback];
    [self.operationQueue addOperation:pendingOp];
}

- (void)rpcTrustStatusCachedStatus:(OTAccountMetadataClassC*)account
                             reply:(void (^)(CliqueStatus status,
                                             NSString* egoPeerID,
                                             NSDictionary<NSString*, NSNumber*>* _Nullable peerCountByModelID,
                                             BOOL isExcluded,
                                             BOOL isLocked,
                                             NSError *error))reply
{
    CliqueStatus status = CliqueStatusAbsent;

    if (account.isInheritedAccount || account.trustState == OTAccountMetadataClassC_TrustState_TRUSTED) {
        status = CliqueStatusIn;
    } else if (account.trustState == OTAccountMetadataClassC_TrustState_UNTRUSTED) {
        status = CliqueStatusNotIn;
    }

    secinfo("octagon", "returning cached clique status: %@", OTCliqueStatusToString(status));
    reply(status, account.peerID, nil, NO, NO, NULL);
}


- (void)rpcTrustStatus:(OTOperationConfiguration *)configuration
                 reply:(void (^)(CliqueStatus status,
                                 NSString* _Nullable peerID,
                                 NSDictionary<NSString*, NSNumber*>* _Nullable peerCountByModelID,
                                 BOOL isExcluded,
                                 BOOL isLocked,
                                 NSError *error))reply
{
    __block NSError* localError = nil;

    OTAccountMetadataClassC* account = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];
    if(localError && [self.lockStateTracker isLockedError:localError]){
        secnotice("octagon", "Device is locked! pending initialization on unlock");
        reply(CliqueStatusError, nil, nil, NO, NO, localError);
        return;
    }

    if(account.icloudAccountState == OTAccountMetadataClassC_AccountState_NO_ACCOUNT) {
        secnotice("octagon", "no account! returning clique status 'no account'");
        reply(CliqueStatusNoCloudKitAccount, nil, nil, NO, NO, NULL);
        return;
    }

    if (configuration.useCachedAccountStatus) {
        [self rpcTrustStatusCachedStatus:account reply:reply];
        return;
    }

    CKKSAccountStatus ckAccountStatus = [self checkForCKAccount:configuration];
    if(ckAccountStatus == CKKSAccountStatusNoAccount) {
        secnotice("octagon", "No cloudkit account present");
        reply(CliqueStatusNoCloudKitAccount, nil, nil, NO, NO, NULL);
        return;
    } else if(ckAccountStatus == CKKSAccountStatusUnknown) {
        secnotice("octagon", "Unknown cloudkit account status, returning cached trust value");
        [self rpcTrustStatusCachedStatus:account reply:reply];
        return;
    } else if(account.isInheritedAccount) {
        secnotice("octagon", "Inherited account -- should circuit to cached trust value");
        [self rpcTrustStatusCachedStatus:account reply:reply];
        return;
    }

    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
       secnotice("octagon", "No cloudkit account present: %@", accountError);
       reply(CliqueStatusNoCloudKitAccount, nil, nil, NO, NO, accountError);
       return;
    }

    __block NSString* peerID = nil;
    __block NSDictionary<NSString*, NSNumber*>* peerModelCounts = nil;
    __block BOOL excluded = NO;
    __block CliqueStatus trustStatus = CliqueStatusError;
    __block BOOL isLocked = NO;

    [self.cuttlefishXPCWrapper trustStatusWithSpecificUser:self.activeAccount
                                                     reply:^(TrustedPeersHelperEgoPeerStatus *egoStatus,
                                                          NSError *xpcError) {
        TPPeerStatus status = egoStatus.egoStatus;
        peerID = egoStatus.egoPeerID;
        excluded = egoStatus.isExcluded;
        peerModelCounts = egoStatus.viablePeerCountsByModelID;
        isLocked = egoStatus.isLocked;
        localError = xpcError;

        if(xpcError) {
            secnotice("octagon", "error fetching trust status: %@", xpcError);
        } else {
            secnotice("octagon", "trust status: %@", TPPeerStatusToString(status));

            [self popTooManyPeersDialogWithEgoPeerStatus:egoStatus accountMeta:account];

            if((status&TPPeerStatusExcluded) == TPPeerStatusExcluded){
                trustStatus = CliqueStatusNotIn;
            }
            else if((status&TPPeerStatusPartiallyReciprocated) == TPPeerStatusPartiallyReciprocated){
                trustStatus = CliqueStatusIn;
            }
            else if((status&TPPeerStatusAncientEpoch) == TPPeerStatusAncientEpoch){
                //FIX ME HANDLE THIS CASE
                trustStatus=  CliqueStatusIn;
            }
            else if((status&TPPeerStatusOutdatedEpoch) == TPPeerStatusOutdatedEpoch){
                //FIX ME HANDLE THIS CASE
                trustStatus = CliqueStatusIn;
            }
            else if((status&TPPeerStatusFullyReciprocated) == TPPeerStatusFullyReciprocated){
                trustStatus = CliqueStatusIn;
            }
            else if ((status&TPPeerStatusSelfTrust) == TPPeerStatusSelfTrust) {
                trustStatus = CliqueStatusIn;
            }
            else if ((status&TPPeerStatusIgnored) == TPPeerStatusIgnored) {
                trustStatus = CliqueStatusNotIn;
            }
            else if((status&TPPeerStatusUnknown) == TPPeerStatusUnknown){
                trustStatus = CliqueStatusAbsent;
            }
            else {
                secnotice("octagon", "TPPeerStatus is empty");
                trustStatus = CliqueStatusAbsent;
            }

            OTAccountMetadataClassC_TrustState newTrustState;
            switch (trustStatus) {
            case CliqueStatusIn:
                newTrustState = OTAccountMetadataClassC_TrustState_TRUSTED;
                break;
            case CliqueStatusNotIn:
                newTrustState = OTAccountMetadataClassC_TrustState_UNTRUSTED;
                break;
            default:
                newTrustState = OTAccountMetadataClassC_TrustState_UNKNOWN;
                break;
            }
            NSError* localError = nil;
            BOOL persisted = [self.accountMetadataStore persistNewTrustState:newTrustState error:&localError];
            if (!persisted || localError) {
                secerror("octagon: unable to persist clique trust state: %@", localError);
            } else {
                secnotice("octagon", "updated account trust state: %@", OTAccountMetadataClassC_TrustStateAsString(newTrustState));
            }
        }
    }];

    reply(trustStatus, peerID, peerModelCounts, excluded, isLocked, localError);
}

- (void)rpcFetchAllViableBottlesFromSource:(OTEscrowRecordFetchSource)source
                                     reply:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs,
                                                     NSArray<NSString*>* _Nullable sortedPartialEscrowRecordIDs,
                                                     NSError* _Nullable error))reply
{
    NSError* accountError = nil;
    if (source != OTEscrowRecordFetchSourceCache && (accountError = [self errorIfNoCKAccount:nil]) != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(nil, nil, accountError);
        return;
    }
    
    // In order to maintain consistent values for flowID and deviceSessionID, copy the sessionMetrics to a local variable
    // that way when another thread mutates the sessionMetrics property, this thread can safely use the local copy
    __strong OTMetricsSessionData* localSessionMetrics = self.sessionMetrics;

    // As this isn't a state-modifying operation, we don't need to go through the state machine.
    [self.cuttlefishXPCWrapper fetchViableBottlesWithSpecificUser:self.activeAccount
                                                           source:source
                                                           flowID:localSessionMetrics.flowID
                                                  deviceSessionID:localSessionMetrics.deviceSessionID
                                                            reply:^(NSArray<NSString*>* _Nullable sortedEscrowRecordIDs,
                                                                    NSArray<NSString*>* _Nullable sortedPartialEscrowRecordIDs,
                                                                    NSError * _Nullable error) {
            if(error){
                secerror("octagon: error fetching all viable bottles: %@", error);
                reply(nil, nil, error);
            }else{
                secnotice("octagon", "fetched viable bottles: %@", sortedEscrowRecordIDs);
                secnotice("octagon", "fetched partially viable bottles: %@", sortedPartialEscrowRecordIDs);
                reply(sortedEscrowRecordIDs, sortedPartialEscrowRecordIDs, error);
            }
    }];
}

- (void)rpcFetchAllViableEscrowRecordsFromSource:(OTEscrowRecordFetchSource)source
                                           reply:(void (^)(NSArray<NSData*>* _Nullable records,
                                                           NSError* _Nullable error))reply
{
    NSError* accountError = nil; 
    if (source != OTEscrowRecordFetchSourceCache && (accountError = [self errorIfNoCKAccount:nil]) != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(nil, accountError);
        return;
    }

    // As this isn't a state-modifying operation, we don't need to go through the state machine.
    [self.cuttlefishXPCWrapper fetchViableEscrowRecordsWithSpecificUser:self.activeAccount
                                                                 source:source
                                                                  reply:^(NSArray<NSData*>* _Nullable records,
                                                                          NSError* _Nullable error) {
        if (error) {
            secerror("octagon: error fetching all viable escrow records: %@", error);
            reply(nil, error);
        } else {
            secnotice("octagon", "fetched escrow records: %@", records);
            reply(records, error);
        }
    }];
}

- (void)rpcInvalidateEscrowCache:(void (^)(NSError* _Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    // As this isn't a state-modifying operation, we don't need to go through the state machine.
    [self.cuttlefishXPCWrapper removeEscrowCacheWithSpecificUser:self.activeAccount
                                                           reply:^(NSError * _Nullable removeError) {
        if (removeError){
            secerror("octagon: failed to remove escrow cache: %@", removeError);
            reply(removeError);
        } else{
            secnotice("octagon", "successfully removed escrow cache");
            reply(nil);
        }
    }];
}

- (void)fetchEscrowContents:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply
{

    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(nil, nil, nil, accountError);
        return;
    }

    // As this isn't a state-modifying operation, we don't need to go through the state machine.
    [self.cuttlefishXPCWrapper fetchEscrowContentsWithSpecificUser:self.activeAccount
                                                             reply:^(NSData * _Nullable entropy,
                                                                     NSString * _Nullable bottleID,
                                                                     NSData * _Nullable signingPublicKey,
                                                                     NSError * _Nullable error) {
            if(error){
                secerror("octagon: error fetching escrow contents: %@", error);
                reply(nil, nil, nil, error);
            }else{
                secnotice("octagon", "fetched escrow contents for bottle: %@", bottleID);
                reply(entropy, bottleID, signingPublicKey, error);
            }
        }];
}

- (void)rpcRefetchCKKSPolicy:(void (^)(NSError * _Nullable error))reply
{
    [self.stateMachine doWatchedStateMachineRPC:@"octagon-refetch-ckks-policy"
                                   sourceStates:[OTStates OctagonReadyStates]
                                           path:[OctagonStateTransitionPath pathFromDictionary:@{
                                               OctagonStateRefetchCKKSPolicy: @{
                                                       OctagonStateBecomeReady: @{
                                                               OctagonStateReady: [OctagonStateTransitionPathStep success],
                                                       },
                                               },
                                           }]
                                          reply:reply
    ];
}

- (void)rpcFetchUserControllableViewsSyncingStatus:(void (^)(BOOL areSyncing, NSError* _Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(NO, accountError);
        return;
    }

    if ([self.stateMachine isPaused]) {
        if ([[OTStates OctagonNotInCliqueStates] intersectsSet: [NSSet setWithObject: [self.stateMachine currentState]]]) {
            secnotice("octagon-ckks", "device is not in clique, returning not syncing");
            reply(NO, nil);
            return;
        } else if ([[self.stateMachine currentState] isEqualToString:OctagonStateError]) {
            secnotice("octagon-ckks", "state machine in the error state, cannot service request");
            reply(NO, nil);
            return;
        }
    }

    if(self.ckks.syncingPolicy) {
        BOOL syncing = self.ckks.syncingPolicy.syncUserControllableViewsAsBoolean;

        secnotice("octagon-ckks", "Returning user-controllable status as %@ (%@)",
                  syncing ? @"enabled" : @"disabled",
                  TPPBPeerStableInfoUserControllableViewStatusAsString(self.ckks.syncingPolicy.syncUserControllableViews));

        reply(syncing, nil);
        return;
    }

    // No loaded policy? Let's trigger a fetch.
    [self rpcRefetchCKKSPolicy:^(NSError * _Nullable error) {
        if(error) {
            secnotice("octagon-ckks", "Failed to fetch policy: %@", error);
            reply(NO, error);
            return;
        }

        if(self.ckks.syncingPolicy) {
            BOOL syncing = self.ckks.syncingPolicy.syncUserControllableViewsAsBoolean;

            secnotice("octagon-ckks", "Returning user-controllable status as %@ (%@)",
                      syncing ? @"enabled" : @"disabled",
                      TPPBPeerStableInfoUserControllableViewStatusAsString(self.ckks.syncingPolicy.syncUserControllableViews));

            reply(syncing, nil);
            return;

        } else {
            // The tests sometimes don't have a viewManager. If we're in the tests, try this last-ditch effort:
            if(SecCKKSTestsEnabled()) {
                NSError* metadataError = nil;
                OTAccountMetadataClassC* accountState = [self.accountMetadataStore loadOrCreateAccountMetadata:&metadataError];
                if(metadataError) {
                    secnotice("octagon-ckks", "Error fetching acount state: %@", metadataError);
                }
                TPSyncingPolicy* policy = [accountState getTPSyncingPolicy];
                if(policy) {
                    BOOL syncing = policy.syncUserControllableViewsAsBoolean;
                    secnotice("octagon-ckks", "Returning user-controllable status (fetched from account state) as %@ (%@)",
                              syncing ? @"enabled" : @"disabled",
                              TPPBPeerStableInfoUserControllableViewStatusAsString(self.ckks.syncingPolicy.syncUserControllableViews));
                    reply(syncing, nil);
                    return;
                }
            }

            secnotice("octagon-ckks", "Policy missing even after a refetch?");
            reply(NO, [NSError errorWithDomain:OctagonErrorDomain
                                          code:OctagonErrorSyncPolicyMissing
                                   description:@"Sync policy is missing even after refetching"]);
            return;
        }
    }];
}

- (void)rpcSetUserControllableViewsSyncingStatus:(BOOL)status reply:(void (^)(BOOL areSyncing, NSError* _Nullable error))reply
{
#if TARGET_OS_TV
    // TVs can't set this value.
    secnotice("octagon-ckks", "Rejecting set of user-controllable sync status due to platform");
    reply(NO, [NSError errorWithDomain:OctagonErrorDomain
                                  code:OctagonErrorNotSupported
                           description:@"This platform does not support setting the user-controllable view syncing status"]);
    return;
#else
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(NO, accountError);
        return;
    }

    NSError* metadataError = nil;
    OTAccountMetadataClassC* accountState = [self.accountMetadataStore loadOrCreateAccountMetadata:&metadataError];
    if(metadataError) {
        secnotice("octagon-ckks", "Error fetching acount state: %@", metadataError);
    }

    if (accountState.isInheritedAccount) {
        secnotice("octagon-ckks", "Account is inherited, user controllable views cannot be set");
        reply(NO, [NSError errorWithDomain:OctagonErrorDomain
                                      code:OctagonErrorUserControllableViewsUnavailable
                               description:@"Cannot set user controllable views"]);
        return;
    }

    OctagonState* firstState = status ? OctagonStateEnableUserControllableViews : OctagonStateDisableUserControllableViews;

    secnotice("octagon-ckks", "Settting user-controllable sync status as '%@'", status ? @"enabled" : @"disabled");

    [self.stateMachine doWatchedStateMachineRPC:@"octagon-set-policy"
                                   sourceStates:[OTStates OctagonReadyStates]
                                           path:[OctagonStateTransitionPath pathFromDictionary:@{
                                               firstState: @{
                                                       OctagonStateBecomeReady: @{
                                                               OctagonStateReady: [OctagonStateTransitionPathStep success],
                                                       },
                                               },
                                           }]
                                          reply:^(NSError * _Nullable error) {
        if(error) {
            secnotice("octagon-ckks", "Failed to set sync policy to '%@': %@", status ? @"enabled" : @"disabled", error);
            reply(NO, error);
            return;
        }

        if(self.ckks.operationDependencies.syncingPolicy) {
            BOOL finalStatus = self.ckks.operationDependencies.syncingPolicy.syncUserControllableViewsAsBoolean;
            secnotice("octagon-ckks", "User-controllable sync status is set as '%@'", finalStatus ? @"enabled" : @"disabled");
            reply(finalStatus, nil);
            return;
        }

        // The tests sometimes don't have a CKKS object. If we're in the tests, try this last-ditch effort:
        if(SecCKKSTestsEnabled()) {
            NSError* metadataError = nil;
            OTAccountMetadataClassC* accountState = [self.accountMetadataStore loadOrCreateAccountMetadata:&metadataError];
            if(metadataError) {
                secnotice("octagon-ckks", "Error fetching acount state: %@", metadataError);
            }
            TPSyncingPolicy* policy = [accountState getTPSyncingPolicy];
            if(policy) {
                BOOL syncing = policy.syncUserControllableViewsAsBoolean;
                secnotice("octagon-ckks", "Returning user-controllable status (fetched from account state) as %@ (%@)",
                          syncing ? @"enabled" : @"disabled",
                          TPPBPeerStableInfoUserControllableViewStatusAsString(policy.syncUserControllableViews));
                reply(syncing, nil);
                return;
            }
        }

        secnotice("octagon-ckks", "Policy missing even after a refetch?");
        reply(NO, [NSError errorWithDomain:OctagonErrorDomain
                                      code:OctagonErrorSyncPolicyMissing
                               description:@"Sync policy is missing even after refetching"]);
        return;
    }];
#endif
}

- (void)rpcSetAccountSetting:(OTAccountSettings*)settings reply:(void (^)(NSError* _Nullable))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    if ([self.stateMachine isPaused]) {
        if ([[OTStates OctagonReadyStates] intersectsSet: [NSSet setWithObject: [self.stateMachine currentState]]] == NO) {
            secnotice("octagon-settings", "device is not in a ready state to set account settings, returning");
            reply([NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorCannotSetAccountSettings description:@"Device is not in Octagon yet to set account settings"]);
            return;
        }
    } else {
        if ([self waitForReady:OctagonStateTransitionDefaultTimeout] == NO) {
            secnotice("octagon-settings", "rpcSetAccountSetting: failed to reach Ready state");
            reply([NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorCannotSetAccountSettings description:@"Device is not in Octagon yet to set account settings"]);
            return;
        }
    }

    secnotice("octagon-settings", "Setting account settings %@", settings);

    self.accountSettings = [self mergedAccountSettings:settings];

    [self.stateMachine doWatchedStateMachineRPC:@"octagon-set-account-settings"
                                   sourceStates:[OTStates OctagonReadyStates]
                                           path:[OctagonStateTransitionPath pathFromDictionary:@{
                                            OctagonStateSetAccountSettings: @{
                                                OctagonStateBecomeReady: @{
                                                    OctagonStateReady: [OctagonStateTransitionPathStep success],
                                                },
                                            },
                                           }]
                                          reply:reply];
}

- (void)rpcSetLocalSecureElementIdentity:(OTSecureElementPeerIdentity*)secureElementIdentity
                                   reply:(void (^)(NSError* _Nullable))reply
{
    NSError* error = nil;

    [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
        [metadata setOctagonSecureElementIdentity:secureElementIdentity];
        return metadata;
    } error:&error];

    if(error != nil) {
        secnotice("octagon-se", "Unable to persist identity: %@", error);
        reply(error);
        return;
    }

    secnotice("octagon-se", "Successfully persisted new SE identity: %@", secureElementIdentity);

    [self.stateMachine handleFlag:OctagonFlagSecureElementIdentityChanged];
    reply(nil);
}

- (void)rpcRemoveLocalSecureElementIdentityPeerID:(NSData*)sePeerID
                                            reply:(void (^)(NSError* _Nullable))reply
{
    NSError* error = nil;

    [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
        metadata.secureElementIdentity = nil;
        return metadata;
    } error:&error];

    if(error != nil) {
        secnotice("octagon-se", "Unable to persist removal of identity: %@", error);
        reply(error);
        return;
    }

    secnotice("octagon-se", "Successfully persisted removal of SE identity");

    [self.stateMachine handleFlag:OctagonFlagSecureElementIdentityChanged];
    reply(nil);
}

- (void)rpcFetchTrustedSecureElementIdentities:(void (^)(OTCurrentSecureElementIdentities* _Nullable currentSet,
                                                         NSError* _Nullable replyError))reply
{

    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(nil, accountError);
        return;
    }

    // Before heading off to TPH, get the current pending identity (if any).
    // Depending on races, we might return the same identity as both current and pending. This is better than
    // possibly returning neither.
    TPPBSecureElementIdentity* pendingIdentity = nil;

    {
        NSError* metadataLoadError = nil;
        OTAccountMetadataClassC* accountMetadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&metadataLoadError];

        if(accountMetadata == nil || metadataLoadError != nil ) {
            secnotice("octagon", "Unable to load account metadata for (%@,%@): %@", self.containerName, self.contextID, metadataLoadError);
        } else {
            pendingIdentity = accountMetadata.parsedSecureElementIdentity;
        }
    }

    WEAKIFY(self);
    [self.cuttlefishXPCWrapper fetchTrustStateWithSpecificUser:self.activeAccount
                                                         reply:^(TrustedPeersHelperPeerState * _Nullable selfPeerState,
                                                                 NSArray<TrustedPeersHelperPeer *> * _Nullable trustedPeers,
                                                                 NSError * _Nullable operror) {
            STRONGIFY(self);
            if(operror) {
                secnotice("octagon", "Unable to fetch trusted peers for (%@,%@): %@", self.containerName, self.contextID, operror);
                reply(nil, operror);

            } else {
                OTCurrentSecureElementIdentities* currentSet = [[OTCurrentSecureElementIdentities alloc] init];
                currentSet.trustedPeerSecureElementIdentities = [NSMutableArray array];

                for(TrustedPeersHelperPeer* peer in trustedPeers) {
                    if(peer.secureElementIdentity == nil) {
                        continue;
                    }

                    OTSecureElementPeerIdentity* otSEIdentity = [[OTSecureElementPeerIdentity alloc] init];
                    otSEIdentity.peerIdentifier = peer.secureElementIdentity.peerIdentifier;
                    otSEIdentity.peerData = peer.secureElementIdentity.peerData;

                    if([peer.peerID isEqualToString:selfPeerState.peerID]) {
                        currentSet.localPeerIdentity = otSEIdentity;
                    } else {
                        [currentSet.trustedPeerSecureElementIdentities addObject:otSEIdentity];
                    }
                }

                if(pendingIdentity) {
                    OTSecureElementPeerIdentity* otSEIdentity = [[OTSecureElementPeerIdentity alloc] init];
                    otSEIdentity.peerIdentifier = pendingIdentity.peerIdentifier;
                    otSEIdentity.peerData = pendingIdentity.peerData;

                    if(![currentSet.localPeerIdentity isEqual:otSEIdentity]) {
                        secnotice("octagon", "Returning pending identity for (%@,%@): %@", self.containerName, self.contextID, otSEIdentity);
                        currentSet.pendingLocalPeerIdentity = otSEIdentity;
                    }
                }

                reply(currentSet, nil);
            }
        }];
}

- (void)rpcFetchAccountSettings:(void (^)(OTAccountSettings* _Nullable setting, NSError* _Nullable replyError))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(nil, accountError);
        return;
    }

    secinfo("octagon-settings", "Fetching account settings");
    [OTStashAccountSettingsOperation performWithAccountWide:false
                                                 forceFetch:false
                                       cuttlefishXPCWrapper:self.cuttlefishXPCWrapper
                                              activeAccount:self.activeAccount
                                              containerName:self.containerName
                                                  contextID:self.contextID
                                                      reply:^(OTAccountSettings* _Nullable accountSettings, NSError* _Nullable error) {
            if (error != nil) {
                secerror("octagon-settings: Failed fetching account settings: %@", error);
                reply(nil, error);
            } else {
                secerror("octagon-settings: Succeeded fetching account settings: %@", accountSettings);
                reply(accountSettings, nil);
            }
        }];
}

- (void)rpcAccountWideSettingsWithForceFetch:(bool)forceFetch reply:(void (^)(OTAccountSettings* _Nullable setting, NSError* replyError))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(nil, accountError);
        return;
    }

    secinfo("octagon-settings", "Fetching account-wide settings with force: %{bool}d",forceFetch);
    [OTStashAccountSettingsOperation performWithAccountWide:true
                                                 forceFetch:forceFetch
                                       cuttlefishXPCWrapper:self.cuttlefishXPCWrapper
                                              activeAccount:self.activeAccount
                                              containerName:self.containerName
                                                  contextID:self.contextID
                                                      reply:^(OTAccountSettings* _Nullable accountSettings, NSError* _Nullable error) {
            if (error != nil) {
                secerror("octagon-settings: Failed fetching account settings: %@", error);
                reply(nil, error);
            } else {
                secnotice("octagon-settings", "Succeeded fetching account settings: %@", accountSettings);
                reply(accountSettings, nil);
            }
        }];
}

- (void)rpcWaitForPriorityViewKeychainDataRecovery:(void (^)(NSError* replyError))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    secnotice("octagon-ckks", "Beginning to wait for CKKS Priority view download");

    // Ensure that there is trust before calling into CKKS
    if ([[self.stateMachine waitForState:OctagonStateReady wait:OctagonStateTransitionTimeoutForLongOps] isEqualToString:OctagonStateReady]) {
        CKKSResultOperation* waitOp = [self.ckks rpcWaitForPriorityViewProcessing];

        CKKSResultOperation* replyOp = [CKKSResultOperation named:@"wait-for-sync-reply" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
            if(waitOp.error) {
                secerror("octagon-ckks: Done waiting for CKKS Priority view download with error: %@", waitOp.error);
                if ([waitOp.error.domain isEqualToString:CKKSErrorDomain] && (waitOp.error.code == CKKSLackingTrust)) {
                    // CKKS mistakenly thinks that it's lost trust. Re-check its trust status and try again
                    secerror("octagon-ckks: Retrying wait for CKKS Priority view download");
                    NSError* localError = nil;

                    if (![self recheckCKKSTrustStatus:&localError]) {
                        secerror("octagon-ckks: Unable to retry CKKS Priority view download: %@", localError);
                    }

                    CKKSResultOperation* retryOp = [self.ckks rpcWaitForPriorityViewProcessing];
                    CKKSResultOperation* retryWaitOp = [CKKSResultOperation named:@"wait-for-sync-reply" withBlock:^{
                        if (retryOp.error) {
                            secerror("octagon-ckks: Done waiting for CKKS Priority view download retry with error: %@", waitOp.error);
                        } else {
                            secnotice("octagon-ckks", "Done waiting for CKKS Priority view download retry");
                        }
                        reply(retryOp.error);
                    }];

                    [retryWaitOp addDependency:retryOp];
                    [self.operationQueue addOperation:retryWaitOp];
                    return;
                }
            } else {
                secnotice("octagon-ckks", "Done waiting for CKKS Priority view download");
            }
            reply(waitOp.error);
        }];

        [replyOp addDependency:waitOp];
        [self.operationQueue addOperation:replyOp];
    } else {
        secerror("octagon-ckks: rpcWaitForPriorityViewKeychainDataRecovery: failed to get to ready after timeout");
        reply([NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorCKKSLackingTrust description:@"Octagon has not reached a ready state yet"]);
    }
}

- (void)octagonPeerIDGivenBottleID:(NSString*)bottleID reply:(void (^)(NSString *peerID))reply
{
    [self rpcFetchPeerIDByBottleID:^(NSDictionary<NSString *,NSString *> *peers, NSError * error) {
        __block NSString* peerID = nil;
        [peers enumerateKeysAndObjectsUsingBlock:^(NSString *fetchedPeerID, NSString *bottleID, BOOL *stop) {
            if ([bottleID isEqualToString:bottleID]) {
                peerID = fetchedPeerID;
                *stop = true;
            }
        }];
        reply(peerID);
    }];
}

- (void)tlkRecoverabilityInOctagon:(NSData*)recordData source:(OTEscrowRecordFetchSource)source reply:(void (^)(NSArray<NSString*>* views, NSError* replyError))reply
{
    //fetch all bottleIDs, fully viable and partial
    [self rpcFetchAllViableBottlesFromSource:source reply:^(NSArray<NSString *> * _Nullable sortedBottleIDs, NSArray<NSString *> * _Nullable sortedPartialEscrowRecordIDs, NSError * _Nullable fetchError) {
        if (fetchError) {
            secerror("octagon-tlk-recoverability: fetching bottles failed: %@", fetchError);
            reply(nil, fetchError);
            return;
        } else {
            OTEscrowRecord *otRecord = [[OTEscrowRecord alloc] initWithData:recordData];
            NSString *evaluationBottle = otRecord.escrowInformationMetadata.bottleId;
            if (![sortedBottleIDs containsObject:evaluationBottle] && ![sortedPartialEscrowRecordIDs containsObject:evaluationBottle]) {
                secnotice("octagon-tlk-recoverability", "record's bottleID is not valid in cuttlefish");
                reply(nil, [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecordNotViable userInfo:@{NSLocalizedDescriptionKey : @"Record's bottleID is not valid in cuttlefish"}]);
            } else {
                //fetch the Octagon peer matching the escrow record's serial
                [self octagonPeerIDGivenBottleID:otRecord.escrowInformationMetadata.bottleId reply:^(NSString *peerID) {
                    if (!peerID) {
                        secnotice("octagon-tlk-recoverability", "Octagon peerID not trusted for record %@", otRecord);
                        reply(nil, [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecordNotViable userInfo:@{NSLocalizedDescriptionKey : @"Octagon peerID not trusted for record"}]);
                        return;
                    } else {
                        NSError *localError = nil;
                        NSArray<NSString*>* views = [self.ckks viewsForPeerID:peerID error:&localError];
                        reply(views, localError);
                        return;
                    }
                }];
            }
        }
    }];
}

- (void)rpcTlkRecoverabilityForEscrowRecordData:(NSData*)recordData
                                         source:(OTEscrowRecordFetchSource)source
                                          reply:(void (^)(NSArray<NSString*>* views, NSError* replyError))reply
{
    NSError* accountError = nil;
    if (source != OTEscrowRecordFetchSourceCache && (accountError = [self errorIfNoCKAccount:nil]) != nil) {
        secnotice("octagon-tlk-recoverability", "No cloudkit account present: %@", accountError);
        reply(nil, accountError);
        return;
    }

    [self tlkRecoverabilityInOctagon:recordData source:source reply:^(NSArray<NSString *> *views, NSError *replyError) {
        if (replyError) {
            secerror("octagon-tlk-recoverability: failed assessing tlk recoverability using the octagon identity, error: %@", replyError);
            reply(nil, replyError);
            return;
        }

        if (views && [views count] > 0) {
            secnotice("octagon-tlk-recoverability", "found views using octagon peer matching record! views: %@", views);
            reply(views, nil);
            return;
        } else {
            secerror("octagon-tlk-recoverability: failed to find views");
            reply(nil, [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecordNotViable userInfo:@{ NSLocalizedDescriptionKey : @"Record cannot recover any views" }]);
            return;
        }
    }];
}

- (void)rpcFetchTotalCountOfTrustedPeers:(void (^)(NSNumber* count, NSError* replyError))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(@0, accountError);
        return;
    }

    [self.cuttlefishXPCWrapper fetchTrustedPeerCountWithSpecificUser:self.activeAccount reply:^(NSNumber * _Nullable count, NSError * _Nullable error) {
        reply(count, error);
    }];
}

- (void)rerollWithReply:(void (^)(NSError *_Nullable error))reply
{
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];

    NSError* accountError = [self errorIfNoCKAccount:configuration];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:@{
          OctagonStateStashAccountSettingsForReroll: @{
              OctagonStateCreateIdentityForReroll: @{
                  OctagonStateVouchWithReroll: [self joinStatePathDictionary],
                        },
                    },
                }];

    [self.stateMachine doWatchedStateMachineRPC:@"reroll"
                                   sourceStates:[OTStates OctagonReadyStates]
                                           path:path
                                          reply:reply];
}

#pragma mark --- Health Checker

- (BOOL)postRepairCFU:(NSError**)error
{
    NSError* localError = nil;
    BOOL postSuccess = NO;
    [self.followupHandler postFollowUp:OTFollowupContextTypeStateRepair activeAccount:self.activeAccount error:&localError];
    if(localError){
        secerror("octagon-health: CoreCDP repair failed: %@", localError);
        if(error){
            *error = localError;
        }
    }
    else{
        secnotice("octagon-health", "CoreCDP post repair success");
        postSuccess = YES;
    }
    return postSuccess;
}

- (BOOL)shouldPostConfirmPasscodeCFU:(NSError**)error
{
    NSError* localError = nil;
    id<SecEscrowRequestable> request = [self.escrowRequestClass request:&localError];
    if(!request || localError) {
        secnotice("octagon-health", "Unable to acquire a EscrowRequest object: %@", localError);
        if(error){
            *error = localError;
        }
        return YES;
    }
    BOOL pendingUpload = [request pendingEscrowUpload:&localError];

    if(localError) {
        secnotice("octagon-health", "Failed to check escrow prerecord status: %@", localError);
        if(error) {
            *error = localError;
        }
        return YES;
    }

    if(pendingUpload == YES) {
        secnotice("octagon-health", "prerecord is pending, NOT posting CFU");
        return NO;
    } else {
        secnotice("octagon-health", "no pending prerecords, posting CFU");
        return YES;
    }
}

- (BOOL)recheckCKKSTrustStatus:(NSError**)error {

    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        if (error) {
            *error = accountError;
        }
        return NO;
    }

    secnotice("octagon", "Asked to re-check CKKS's trust status");
    if ([[self.stateMachine waitForState:OctagonStateReady wait:OctagonStateTransitionDefaultTimeout] isEqualToString:OctagonStateReady]) {
        // Poke all our CKKS views!
        secnotice("octagon-ckks", "Resetting CKKS(%@) peer providers", self.ckks);

        NSArray<id<CKKSPeerProvider>>* peerProviders = nil;

        if(self.sosAdapter.sosEnabled) {
            peerProviders = @[self.octagonAdapter, self.sosAdapter];

        } else {
            peerProviders = @[self.octagonAdapter];
        }

        [self.ckks beginTrustedOperation:peerProviders
                       suggestTLKUpload:self.suggestTLKUploadNotifier
                     requestPolicyCheck:self.requestPolicyCheckNotifier];
        return YES;
    } else {
        secerror("octagon-ckks: recheckCKKSTrustStatus: failed to get to ready after timeout");
        if (error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorCKKSLackingTrust description:@"Octagon has not reached a ready state yet"];
        }
        return NO;
    }
}

- (BOOL)leaveTrust:(NSError**)error
{
    if (SOSCompatibilityModeGetCachedStatus()) {
        CFErrorRef cfError = NULL;
        bool left = SOSCCRemoveThisDeviceFromCircle_Server(&cfError);

        if(!left || cfError) {
            secerror("failed to leave SOS circle: %@", cfError);
            if(error) {
                *error = (NSError*)CFBridgingRelease(cfError);
            } else {
                CFReleaseNull(cfError);
            }
            return NO;
        }
    }
    secnotice("octagon-health", "Successfully left SOS");
    return YES;
}

- (BOOL)postConfirmPasscodeCFU:(NSError**)error
{
    BOOL ret = NO;
    NSError* localError = nil;
    ret = [self.followupHandler postFollowUp:OTFollowupContextTypeConfirmExistingSecret activeAccount:self.activeAccount error:&localError];
    if(localError){
        secerror("octagon-health: CoreCDP confirm existing secret failed: %@", localError);
        if(error) {
            *error = localError;
        }
    }
    return ret;
}

#define ESCROW_TIME_BETWEEN_SILENT_MOVE (180*24*60*60) /* 180 days*/

- (BOOL)processMoveRequest:(OTEscrowMoveRequestContext*)moveRequest error:(NSError **)error
{
    bool shouldTriggerEscrowUpdate = false;
    bool shouldPostFollowup = false;
    NSString *altDSID = nil;

    // OctagonEscrowMove enabled by default
    NSError *escrowError = nil;
    id<SecEscrowRequestable> request = [self.escrowRequestClass request:&escrowError];
    if(!request || escrowError) {
        secnotice("octagon-health", "Unable to acquire EscrowRequest object: %@", escrowError);
        if(error) {
            *error = escrowError;
        }
        return NO;
    }

    NSError *metadataError = nil;
    OTAccountMetadataClassC *account = [self.accountMetadataStore loadOrCreateAccountMetadata:&metadataError];
    altDSID = account.altDSID;
    if(!altDSID || metadataError) {
        secnotice("octagon-health", "Failed to get altDSID: %@", metadataError);
        if(error) {
            *error = metadataError;
        }
        return NO;
    }

    if([SecureBackup moveToFederationAllowed:moveRequest.intendedFederation altDSID:altDSID error:NULL]) {
        if(os_feature_enabled(Security, OctagonEscrowMoveUnthrottled) || ![request escrowCompletedWithinLastSeconds:ESCROW_TIME_BETWEEN_SILENT_MOVE]) {
            shouldTriggerEscrowUpdate = true;
            [[CKKSAnalytics logger] logSuccessForEventNamed:OctagonEventEscrowMoveTriggered];
        } else {
            shouldPostFollowup = true;
            secnotice("octagon-health", "Skipping escrow move request (rate limited), posting follow up");
            [[CKKSAnalytics logger] logSuccessForEventNamed:OctagonEventEscrowMoveRateLimited];
        }
    } else {
        shouldPostFollowup = true;
        secnotice("octagon-health", "Secure terms not accepted, posting followup");
        [[CKKSAnalytics logger] logSuccessForEventNamed:OctagonEventEscrowMoveTermsNeeded];
    }

    if(shouldTriggerEscrowUpdate) {
        NSDictionary *options = @{
            SecEscrowRequestOptionFederationMove : moveRequest.intendedFederation,
        };
        NSError *triggerError = nil;
        if ([request triggerEscrowUpdate:@"octagon-health" options:options error:&triggerError]) {
            secnotice("octagon-health", "Triggered escrow move");
        } else {
            secerror("octagon-health: Unable to trigger escrow move: %@", triggerError);
        }
    }

    if(shouldPostFollowup) {
        NSError *followUpError = nil;
        if ([self.followupHandler postFollowUp:OTFollowupContextTypeSecureTerms activeAccount:self.activeAccount error:&followUpError]) {
            secnotice("octagon-health", "Posted secure terms followup");
        } else {
            secerror("octagon-health: Failed to post secure terms followup: %@", followUpError);
        }
    }

    return YES;
}

- (void)checkOctagonHealth:(BOOL)skipRateLimitingCheck repair:(BOOL)repair reply:(void (^)(TrustedPeersHelperHealthCheckResult *_Nullable results, NSError * _Nullable error))reply
{
    secnotice("octagon-health", "Beginning checking overall Octagon Trust");

    //check if we are class C locked
    if ([self.stateMachine isPaused]) {
        if ([[self.stateMachine currentState] isEqualToString:OctagonStateWaitForClassCUnlock]) {
            secnotice("octagon-health", "currently waiting for class C unlock");
            NSError* localError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorClassCLocked description:@"Not performing health check, waiting for Class C Unlock"];
            reply(nil, localError);
            return;
        } else if ([[self.stateMachine currentState] isEqualToString:OctagonStateNoAccount]) {
            secnotice("octagon-health", "Not performing health check, not currently signed in");
            NSError* localError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNoAppleAccount description:@"Not performing health check, not currently signed in"];
            reply(nil, localError);
            return;
        }
    }

    _skipRateLimitingCheck = skipRateLimitingCheck;
    _repair = repair;
    _reportRateLimitingError = YES;

    WEAKIFY(self);

    // If we're not in a CDP-capable account yet, double-check by simulating an IDMS level change.
    // But, likely this is in the right state (unless we missed a notification), so there's no need to continue on and perform the whole health check.
    // Report a failure, as we won't have a healthcheckresult, but afterward, double-check that our understanding of the account type is still correct
    if([[self.stateMachine currentState] isEqualToString:OctagonStateWaitForCDPCapableSecurityLevel]) {
        [self.stateMachine handleFlag:OctagonFlagIDMSLevelChanged];

        NSError* localError = [NSError errorWithDomain:OctagonErrorDomain
                                                  code:OctagonErrorUnsupportedAccount
                                           description:@"Unable to perform health check on this account type"];
        reply(nil, localError);
        return;
    }

    // Ending in "waitforunlock" is okay for a health check
    [self.stateMachine doWatchedStateMachineRPC:@"octagon-trust-health-check"
                                   sourceStates:[OTStates OctagonHealthSourceStates]
                                           path:[OctagonStateTransitionPath pathFromDictionary:@{
                                                OctagonStateCDPHealthCheck: @{
                                                    OctagonStateSecurityTrustCheck: @{
                                                        OctagonStateTPHTrustCheck: @{
                                                            OctagonStateCuttlefishTrustCheck: @{
                                                                OctagonStateBecomeReady: @{
                                                                    OctagonStateReady: [OctagonStateTransitionPathStep success],
                                                                    OctagonStateWaitForUnlock: [OctagonStateTransitionPathStep success],
                                                                },
                                                                // Cuttlefish can suggest we reset the world. Consider reaching here a success,
                                                                // instead of tracking the whole reset.
                                                                OctagonStateHealthCheckReset: [OctagonStateTransitionPathStep success],
                                                                OctagonStateHealthCheckLeaveClique: [OctagonStateTransitionPathStep success],
                                                            },
                                                            OctagonStateWaitForUnlock: [OctagonStateTransitionPathStep success],
                                                            OctagonStateUntrusted: [OctagonStateTransitionPathStep success],
                                                        },
                                                    },
                                                    OctagonStateWaitForCDP: [OctagonStateTransitionPathStep success],
                                                },
                                           }]
                                          reply:^(NSError *_Nullable error) {
            STRONGIFY(self);
            self->_skipRateLimitingCheck = NO;
            self->_repair = NO;
            self->_reportRateLimitingError = NO;
            if (error) {
                reply(nil, error);
            } else {
                secinfo("octagon-health", "results=%@", self->_healthCheckResults);
                reply(self->_healthCheckResults, nil);
                self->_healthCheckResults = nil;
            }
        }];
}

#pragma mark -

- (void)waitForOctagonUpgrade:(void (^)(NSError* _Nullable error))reply
{
    secnotice("octagon-sos", "waitForOctagonUpgrade");

    NSError* localError = nil;

    if (!self.sosAdapter.sosEnabled) {
        secnotice("octagon-sos", "sos not enabled, nothing to do for waitForOctagonUpgrade");
        reply(nil);
        return;
    } else if ([self.sosAdapter circleStatus:&localError] != kSOSCCInCircle) {
        secnotice("octagon-sos", "SOS circle status: %d, cannot perform sos upgrade", [self.sosAdapter circleStatus:&localError]);
        if (localError == nil) {
            localError = [NSError errorWithDomain:(__bridge NSString*)kSOSErrorDomain code:kSOSErrorNoCircle userInfo:@{NSLocalizedDescriptionKey : @"Not in circle"}];
        } else {
            secerror("octagon-sos: error retrieving circle status: %@", localError);
        }
        reply(localError);
        return;
    } else {
        secnotice("octagon-sos", "in sos circle!, attempting upgrade");
    }

    if ([self.stateMachine isPaused]) {
        if ([[self.stateMachine currentState] isEqualToString:OctagonStateReady]) {
            secnotice("octagon-sos", "waitForOctagonUpgrade: already ready, returning");
            reply(nil);
            return;
        }
    } else {
        if ([[self.stateMachine waitForState:OctagonStateReady wait:10*NSEC_PER_SEC] isEqualToString:OctagonStateReady]) {
            secnotice("octagon-sos", "waitForOctagonUpgrade: in ready (after waiting), returning");
            reply(nil);
            return;
        } else {
            secnotice("octagon-sos", "waitForOctagonUpgrade: fail to get to ready after timeout, attempting upgrade");
        }
    }

    NSSet<OctagonState*>* sourceStates = [NSSet setWithArray: @[OctagonStateWaitForCDP,
                                                                OctagonStateUntrusted]];

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:@{
        OctagonStateAttemptSOSUpgradeDetermineCDPState: @{
            OctagonStateAttemptSOSUpgrade: @{
                OctagonStateBecomeReady: @{
                    OctagonStateReady: [OctagonStateTransitionPathStep success],
                },
            },
        },
    }];

    [self.stateMachine doWatchedStateMachineRPC:@"sos-upgrade-to-ready"
                                   sourceStates:sourceStates
                                           path:path
                                          reply:reply];
}

// Metrics passthroughs

- (BOOL)machineIDOnMemoizedList:(NSString*)machineID error:(NSError**)error
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        if (error) {
            *error = accountError;
        }
        return NO;
    }

    __block BOOL onList = NO;
    __block NSError* reterror = nil;
    [self.cuttlefishXPCWrapper fetchAllowedMachineIDsWithSpecificUser:self.activeAccount
                                                             reply:^(NSSet<NSString *> * _Nonnull machineIDs,
                                                                     NSError * _Nullable miderror) {
        if(miderror) {
            secnotice("octagon-metrics", "Failed to fetch allowed machineIDs: %@", miderror);
            reterror = miderror;
        } else {
            if([machineIDs containsObject:machineID]) {
                onList = YES;
            }
            secnotice("octagon-metrics", "MID (%@) on list: %{BOOL}d", machineID, onList);
        }
    }];

    if(reterror && error) {
        *error = reterror;
    }
    return onList;
}


- (NSNumber* _Nullable)currentlyEnforcingIDMSTDL:(NSError**)error
{
    // As this isn't a state-modifying operation, we don't need to go through the state machine.
    __block BOOL enforcing = NO;
    __block NSError* reterror = nil;

    [self.cuttlefishXPCWrapper dumpWithSpecificUser:self.activeAccount
                                              reply:^(NSDictionary * _Nullable dump, NSError * _Nullable dumpError) {
        if (dumpError) {
            secnotice("octagon", "Unable to dump info: %@", dumpError);
            reterror = dumpError;
            return;
        }

        NSString* value = dump[@"honorIDMSListChanges"];
        if ([value isEqualToString:@"YES"]) {
            enforcing = YES;
        }
    }];

    if (reterror) {
        if(error) {
            *error = reterror;
        }

        return nil;
    }

    if (enforcing) {
        return @(YES);
    } else {
        return @(NO);
    }
}

- (TrustedPeersHelperEgoPeerStatus* _Nullable)egoPeerStatus:(NSError**)error
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        if (error) {
            *error = accountError;
        }
        return nil;
    }

    __block TrustedPeersHelperEgoPeerStatus* ret = nil;
    __block NSError* retError = nil;

    [self.cuttlefishXPCWrapper trustStatusWithSpecificUser:self.activeAccount
                                                     reply:^(TrustedPeersHelperEgoPeerStatus *egoStatus,
                                                             NSError *xpcError) {
        if(xpcError) {
            secnotice("octagon-metrics", "Unable to fetch trust status: %@", xpcError);
            retError = xpcError;
        } else {
            ret = egoStatus;
        }
    }];

    if(retError && error) {
        *error = retError;
    }

    return ret;
}

- (void)rpcResetAccountCDPContentsWithIdmsTargetContext:(NSString *_Nullable)idmsTargetContext idmsCuttlefishPassword:(NSString*_Nullable)idmsCuttlefishPassword notifyIdMS:(bool)notifyIdMS reply:(void (^)(NSError* _Nullable error))reply
{
    NSError* accountError = [self errorIfNoCKAccount:nil];
    if (accountError != nil) {
        secnotice("octagon", "No cloudkit account present: %@", accountError);
        reply(accountError);
        return;
    }

    NSString* altDSID = self.activeAccount.altDSID;
    if(altDSID == nil) {
        secnotice("authkit", "No configured altDSID: %@", self.activeAccount);
        NSError *error = [NSError errorWithDomain:OctagonErrorDomain
                                             code:OctagonErrorNoAppleAccount
                                      description:@"No altDSID configured"];
        reply(error);
        return;
    }

    NSError* localError = nil;
    BOOL isAccountDemo = [self.authKitAdapter accountIsDemoAccountByAltDSID:altDSID error:&localError];
    if(localError) {
        secerror("octagon-authkit: failed to fetch demo account flag: %@", localError);
    }

    BOOL internal = SecIsInternalRelease();

    // As this isn't a state-modifying operation, we don't need to go through the state machine.
    [self.cuttlefishXPCWrapper resetAccountCDPContentsWithSpecificUser:self.activeAccount
                                                     idmsTargetContext:idmsTargetContext
                                                idmsCuttlefishPassword:idmsCuttlefishPassword
                                                            notifyIdMS:notifyIdMS
                                                       internalAccount:internal
                                                           demoAccount:isAccountDemo
                                                                 reply:^(NSError * resetError) {
        if (resetError){
            secerror("octagon: failed to reset cdp account contents: %@", resetError);
            reply(resetError);
        } else{
            secnotice("octagon", "successfully reset cdp account contents");
            reply(nil);
        }
    }];
}

- (void)getAccountMetadataWithReply:(void (^)(OTAccountMetadataClassC*_Nullable, NSError *_Nullable))reply
{
    NSError* localError = nil;
    OTAccountMetadataClassC* metadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];
    if (metadata == nil || localError != nil) {
        secnotice("octagon-account-metadata", "error fetching account metadata: %@", localError);
        reply(nil, localError);
        return;
    }
    reply(metadata, nil);
}

- (void)clearContextState
{
    _bottleID = nil;
    _bottleSalt = nil;
    _entropy = nil;
    _resetReason = CuttlefishResetReasonUnknown;
    _idmsTargetContext = nil;
    _idmsCuttlefishPassword = nil;
    _notifyIdMS = false;
    self.accountSettings = nil;
    _skipRateLimitingCheck = NO;
    _repair = NO;
    _reportRateLimitingError = NO;
    self.recoveryKey = nil;
    self.inheritanceKey = nil;
    self.custodianRecoveryKey = nil;
    _healthCheckResults = nil;
}

- (BOOL)checkAllStateCleared
{
    return self.inheritanceKey == nil &&
        self.custodianRecoveryKey == nil &&
        self.recoveryKey == nil &&
        _bottleID == nil &&
        _bottleSalt == nil &&
        _entropy == nil &&
        _resetReason == CuttlefishResetReasonUnknown &&
        _idmsTargetContext == nil &&
        _idmsCuttlefishPassword == nil &&
        _notifyIdMS == false &&
        self.accountSettings == nil &&
        _skipRateLimitingCheck == NO &&
        _repair == NO &&
        _reportRateLimitingError == NO &&
        _healthCheckResults == nil;
}

- (BOOL)fetchSendingMetricsPermitted:(NSError**)error
{
    return [self canSendMetricsUsingAccountState:[self.accountMetadataStore fetchSendingMetricsPermitted:error]];
}

// to be called when CKKS finishes initial sync
- (BOOL)persistSendingMetricsPermitted:(BOOL)sendingMetricsPermitted error:(NSError**)error
{
    if (sendingMetricsPermitted) {
        [self setMetricsStateToActive];
    } else {
        [self setMetricsStateToInactive];
    }
    return [self.accountMetadataStore persistSendingMetricsPermitted:self.shouldSendMetricsForOctagon error:error];
}

@end
#endif
