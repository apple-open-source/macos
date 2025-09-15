/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/OTCheckHealthOperation.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import <Security/SecInternalReleasePriv.h>
#import <CloudServices/SecureBackup.h>

#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

#import <os/feature_private.h>

#if !TARGET_OS_SIMULATOR
#include "utilities/SecAKSWrappers.h"
#endif

#if TARGET_OS_MAC && !TARGET_OS_SIMULATOR
#include <unistd.h>
#endif

@interface OTCheckHealthOperation ()
@property OTOperationDependencies* deps;

@property NSOperation* finishOp;
@end

@implementation OTCheckHealthOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                          deviceInfo:(nonnull OTDeviceInformation *)deviceInfo
                skipRateLimitedCheck:(BOOL)skipRateLimitedCheck
             reportRateLimitingError:(BOOL)reportRateLimitingError
                              repair:(BOOL)repair
                 danglingPeerCleanup:(BOOL)danglingPeerCleanup
                          updateIdMS:(BOOL)updateIdMS
{
    if((self = [super init])) {
        _deps = dependencies;
        _intendedState = intendedState;
        _nextState = errorState;
        _results = nil;
        _deviceInfo = deviceInfo;
        _skipRateLimitingCheck = skipRateLimitedCheck;
        _reportRateLimitingError = reportRateLimitingError;
        _repair = repair;
        _danglingPeerCleanup = danglingPeerCleanup;
        _updateIdMS = updateIdMS;
    }
    return self;
}

+ (BOOL) checkIfPasscodeIsSetForDevice
{
    BOOL passcodeIsSet = NO;
#if TARGET_OS_MAC && !TARGET_OS_SIMULATOR
    aks_device_state_s deviceState;
    kern_return_t retCode = aks_get_device_state(session_keybag_handle, &deviceState);
    if (kAKSReturnSuccess == retCode){
        passcodeIsSet = (deviceState.lock_state != keybag_lock_disabled ? YES : NO);
    } else {
        secerror("octagon-health: aks_get_device_state failed with: %d", retCode);
    }
    secnotice("octagon-health", "checkIfPasscodeIsSetForDevice is %{BOOL}d", passcodeIsSet);
#endif
    return passcodeIsSet;
}

- (void)checkMachineID {
    NSError* localError = nil;
    OTAccountMetadataClassC* accountState = [self.deps.stateHolder loadOrCreateAccountMetadata:&localError];
    if (accountState == nil || localError != nil) {
        secnotice("octagon-health", "could not fetch account state -- not checking machine id (%@)", localError);
        return;
    }
    NSString* oldMachineID = accountState.machineID;

    NSError *error = nil;
    NSString* machineID = [self.deps.authKitAdapter machineID:self.deps.activeAccount.altDSID
                                                       flowID:self.deps.flowID
                                              deviceSessionID:self.deps.deviceSessionID
                                               canSendMetrics:self.deps.permittedToSendMetrics
                                                        error:&error];
    if (machineID == nil || error != nil) {
        secnotice("octagon-health", "fetching machine id failed: %@", error);
        return;
    }
    if (![machineID isEqualToString:oldMachineID]) {
        secnotice("octagon-health", "machineID %@ -> %@", oldMachineID, machineID);
        if (IsRollOctagonIdentityEnabled()) {
            secnotice("octagon-health", "reroll feature flag enabled -- rerolling");
            OctagonPendingFlag *pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:OctagonFlagRerollIdentity
                                                                            conditions:OctagonPendingConditionsDeviceUnlocked | OctagonPendingConditionsNetworkReachable];
            [self.deps.flagHandler handlePendingFlag:pendingFlag];
        } else {
            secnotice("octagon-health", "reroll feature flag disabled -- not rerolling");
        }
    }
}

- (void)groupStart
{
    secnotice("octagon-health", "Beginning cuttlefish health checkup");

    self.finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishOp];

    [self checkMachineID];

    if(self.skipRateLimitingCheck == NO) {
        secnotice("octagon-health", "running rate limiting checks!");
        NSDate* lastUpdate = nil;
        NSError* accountLoadError = nil;
        self.error = nil;

        lastUpdate = [self.deps.stateHolder lastHealthCheckupDate:&accountLoadError];

        if([self.deps.lockStateTracker isLockedError:accountLoadError]) {
            secnotice("octagon-health", "device is locked, not performing cuttlefish check");
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }
        secnotice("octagon-health", "last health check timestamp: %@", lastUpdate);

        // Only query cuttlefish for trust status every 3 days (1 day for internal installs)
        NSDateComponents* offset = [[NSDateComponents alloc] init];
        if(SecIsInternalRelease()) {
            [offset setHour:-23];
        } else {
            [offset setHour:-3*24];
        }
        NSDate *now = [NSDate date];
        NSDate* deadline = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:now options:0];

        if(lastUpdate == nil || [lastUpdate compare: deadline] == NSOrderedAscending) {
            secnotice("octagon-health", "Not rate-limiting: last updated %@ vs %@", lastUpdate, deadline);
        } else {
            secnotice("octagon-health", "Last update is within 3 days (%@); rate-limiting this operation", lastUpdate);
            NSString *description = [NSString stringWithFormat:@"Rate-limited the OTCheckHealthOperation:%@", lastUpdate];
            NSError *rateLimitedError =  [NSError errorWithDomain:@"securityd"
                                                             code:errSecInternalError
                                                         userInfo:@{NSLocalizedDescriptionKey: description}];
            secnotice("octagon-health", "rate limited! %@", rateLimitedError);
            if (self.reportRateLimitingError) {
                self.error = rateLimitedError;
            } else {
                self.nextState = self.intendedState; //not setting the error on the results op as I don't want a CFU posted.
            }
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }
        NSError* persistedError = nil;
        BOOL persisted = [self.deps.stateHolder persistLastHealthCheck:now error:&persistedError];

        if([self.deps.lockStateTracker isLockedError:persistedError]) {
            secnotice("octagon-health", "device is locked, not performing cuttlefish check");
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }
        if(persisted == NO || persistedError) {
            secerror("octagon-health: failed to persist last health check value:%@", persistedError);
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }
    } else {
        secnotice("octagon-health", "NOT running rate limiting checks!");
    }
    WEAKIFY(self);

    [self.deps.cuttlefishXPCWrapper requestHealthCheckWithSpecificUser:self.deps.activeAccount
                                                   requiresEscrowCheck:[OTCheckHealthOperation checkIfPasscodeIsSetForDevice]
                                                                repair:self.repair
                                                   danglingPeerCleanup:self.danglingPeerCleanup
                                                            updateIdMS:self.updateIdMS

#if TARGET_OS_TV
                                                      knownFederations:[NSArray array]
#else
                                                      knownFederations:[SecureBackup knownICDPFederations:NULL]
#endif
                                                                flowID:self.deps.flowID
                                                       deviceSessionID:self.deps.deviceSessionID 
                                                                 reply:^(TrustedPeersHelperHealthCheckResult* result, NSError *error) {
            STRONGIFY(self);
            if(error) {
                secerror("octagon-health: error: %@", error);
                self.error = error;

                [self runBeforeGroupFinished:self.finishOp];
                return;
            } else {
                secnotice("octagon-health", "cuttlefish came back with these suggestions: %@", result);
                [self handleRepairSuggestions:result];
            }
        }];
}

- (void)handleRepairSuggestions:(TrustedPeersHelperHealthCheckResult*)results
{
    self.results = results;

    if (self.results.resetOctagon) {
        secnotice("octagon-health", "Resetting Octagon as per Cuttlefish request");
        self.nextState = OctagonStateHealthCheckReset;
    } else if(self.results.leaveTrust) {
        secnotice("octagon-health", "Leaving clique as per Cuttlefish request");
        self.nextState = OctagonStateHealthCheckLeaveClique;
    } else {
        self.nextState = self.intendedState;
    }

    [self runBeforeGroupFinished:self.finishOp];
}

@end

@interface OTCheckEscrowOperation ()
@property OTOperationDependencies* deps;
@property OTFollowup* followupHandler;

@property NSOperation* finishOp;
@property BOOL isBackgroundCheck;

@property (nullable) OTEscrowCheckCallResult* results;
@end

@implementation OTCheckEscrowOperation

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                     followupHandler:(OTFollowup*)followupHandler
                   isBackgroundCheck:(BOOL)isBackgroundCheck
{
    if((self = [super init])) {
        _deps = dependencies;
        _followupHandler = followupHandler;
        _results = nil;
        _isBackgroundCheck = isBackgroundCheck;
    }
    return self;
}

- (NSNumber* __nullable) getPasscodeGeneration {
    NSNumber* retPasscodeGeneration;
#if !TARGET_OS_SIMULATOR
    NSDictionary* deviceConfigurations = (__bridge_transfer NSDictionary*)MKBGetDeviceConfigurations(NULL); // CF_RETURNS_RETAINED
    if (deviceConfigurations) {
        NSNumber* passcodeGeneration = deviceConfigurations[(__bridge NSString*)kAKSConfigPasscodeGeneration];
        if ([passcodeGeneration isKindOfClass:[NSNumber class]]) {
            retPasscodeGeneration = passcodeGeneration;
        } else {
            secnotice("octagon-escrowcheck", "Unable to get passcodeGeneration");
        }
    }
#else
    retPasscodeGeneration = @(0);
#endif /* !TARGET_OS_SIMULATOR */
    return retPasscodeGeneration;
}

- (void)performEscrowCheck:(void (^)(OTEscrowCheckCallResult *_Nullable results, NSError * _Nullable error))reply
{
    secnotice("octagon-escrowcheck", "Beginning cuttlefish escrow check");

#if TARGET_OS_TV
    secnotice("octagon-escrowcheck", "not running check on TV");
    NSError* error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNoEscrowCheckOnTV userInfo: @{ NSLocalizedDescriptionKey : @"not running escrow check on AppleTV"}];
    reply(nil, error);
    return;
#endif

    // get current passcode generation
    NSNumber* passcodeGen = [self getPasscodeGeneration];
    if (!passcodeGen){
        secnotice("octagon-escrowcheck", "unable to obtain passcode generation for device, returning");
        NSError* error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorUnableToGetPasscodeGeneration userInfo: @{ NSLocalizedDescriptionKey : @"unable to obtain passcode generation for device"}];
        reply(nil, error);
        return;
    }

    WEAKIFY(self);
    [self.deps.cuttlefishXPCWrapper requestEscrowCheckWithSpecificUser:self.deps.activeAccount
                                                   requiresEscrowCheck:[OTCheckHealthOperation checkIfPasscodeIsSetForDevice]
                                                    passcodeGeneration:[passcodeGen unsignedLongLongValue]
                                                      knownFederations:[SecureBackup knownICDPFederations:NULL]
                                                     isBackgroundCheck:self.isBackgroundCheck
                                                                flowID:self.deps.flowID
                                                       deviceSessionID:self.deps.deviceSessionID
                                                                 reply:^(OTEscrowCheckCallResult* result, NSError *error) {
        STRONGIFY(self);
        if (error) {
            secerror("octagon-escrowcheck: error: %@", error);
        } else {
            secnotice("octagon-escrowcheck", "cuttlefish came back with these suggestions: %@", result);
            [self handleRepairSuggestions:result];
        }

        reply(result, error);
    }];
}

- (void)handleRepairSuggestions:(OTEscrowCheckCallResult*)results
{
    self.results = results;

    if (!self.results.octagonTrusted) {
        return;
    }

    if (!self.results.needsReenroll) {
        secnotice("octagon-health", "iCSC Doesn't need Reenroll");

        NSError* clearError = nil;
        if (![self.followupHandler clearAllRepairFollowUps:self.deps.activeAccount error:&clearError]) {
            secnotice("octagon-escrow-repair", "failed to clear follow ups: %@", clearError);
        }

        return;
    }

    secnotice("octagon-health", "iCSC Needs Reenroll");

    if (self.results.repairReason == OTEscrowCheckRepairReasonRecordNeedsMigration && !os_feature_enabled(Security, EscrowCheckMigration)) {
        secnotice("octagon-escrow-repair", "escrow check migration is disabled");
        return;
    }

    AAFAnalyticsEventSecurity* event = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                altDSID:self.deps.activeAccount.altDSID
                                                                                                 flowID:self.deps.flowID
                                                                                        deviceSessionID:self.deps.deviceSessionID
                                                                                              eventName:kSecurityRTCEventNameEscrowPasscodeEnableCacheFlow
                                                                                        testsAreEnabled:SecCKKSTestsEnabled()
                                                                                         canSendMetrics:YES
                                                                                               category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    NSError* flowError = nil;
    BOOL flowSuccess = [self enablePasscodeCacheFlow:&flowError];
    [event sendMetricWithResult:flowSuccess error:flowError];
}

#define ESCROW_TIME_BETWEEN_SILENT_MOVE (180*24*60*60) /* 180 days*/

- (BOOL)enablePasscodeCacheFlow:(NSError**)error
{
    // Retrieve account metadata.
    NSError* accountError = nil;
    OTAccountMetadataClassC* accountState = [self.deps.stateHolder loadOrCreateAccountMetadata:&accountError];
    if (!accountState || accountError) {
        secnotice("octagon-escrow-repair", "failed to get account metadata: %@", accountError);

        if (error) {
            *error = accountError;
        }
        return NO;
    }

    // Get relevant information from account metadata.
    NSString* altDSID = accountState.altDSID;
    NSDate* lastAttemptDate = [NSDate dateWithTimeIntervalSince1970:((NSTimeInterval)accountState.lastEscrowRepairAttempted) / 1000.0];

    // If this is a move request, post a CFU if terms need to be accepted.
    if (self.results.repairReason == OTEscrowCheckRepairReasonRecordNeedsMigration && self.results.moveRequest != nil) {
        NSError* moveError = nil;
        if (![self.deps.secureBackupAdapter moveToFederationAllowed:self.results.moveRequest.intendedFederation altDSID:altDSID error:&moveError]) {
            if (moveError == nil || ([moveError.domain isEqualToString:kCloudServicesErrorDomain] && moveError.code == kCloudServicesMissingSecureTerms)) {
                secnotice("octagon-escrow-repair", "terms acceptance needed, will post follow up");

                NSError* postError = nil;
                if (![self.followupHandler postFollowUp:OTFollowupContextTypeSecureTerms activeAccount:self.deps.activeAccount error:&postError]) {
                    secnotice("octagon-escrow-repair", "failed to post follow up (%@): %@", OTFollowupContextTypeToString(OTFollowupContextTypeSecureTerms), postError);
                }

                if (error) {
                    *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorFollowUpRequired userInfo:nil];
                }
            } else {
                secnotice("octagon-escrow-repair", "failed to determine if federation move is allowed: %@", moveError);

                if (error) {
                    *error = moveError;
                }
            }

            return NO;
        }
    }

    NSDate* now = [NSDate date];

    // Within rate limiting window - must post CFU.
    if ([now timeIntervalSinceDate:lastAttemptDate] < ESCROW_TIME_BETWEEN_SILENT_MOVE) {
        secnotice("octagon-escrow-repair", "rate limited, will not perform silent repair");

        if (error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRateLimited userInfo:nil];
        }
        return NO;
    }

    if (self.results.repairDisabled) {
        secnotice("octagon-escrow-repair", "repair disabled, will not perform silent repair");

        if (error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNotSupported userInfo:nil];
        }
        return NO;
    }

    secnotice("octagon-escrow-repair", "enabling passcode cache flow");

    // Store current date _before_ enabling cache flow, in case the passcode is acquired very quickly.
    NSError* persistError = nil;
    BOOL persisted = [self.deps.stateHolder persistLastEscrowRepairTriggered:now error:&persistError];
    if (!persisted || persistError) {
        secnotice("octagon-escrow-repair", "failed to persist escrow repair trigger date: %@", persistError);
        // If this failed, keep going anyway.
    }

#if !TARGET_OS_SIMULATOR
    // Trigger cache flow. When the passcode is next encountered, the kAppleKeyStoreCacheFlowEnabledNotificationID
    // notification will be posted, which will cause the OctagonFlagPasscodeStashAvailable flag to be set.
    kern_return_t kr = aks_enable_cache_flow(session_keybag_handle);
    if (kr != kAKSReturnSuccess) {
        secnotice("octagon-escrow-repair", "aks_enable_cache_flow failed: %x", kr);

        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:kr userInfo:nil];
        }
        return NO;
    }
#endif // !TARGET_OS_SIMULATOR

    return YES;
}

@end


#endif // OCTAGON
