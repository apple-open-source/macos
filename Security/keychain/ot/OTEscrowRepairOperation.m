/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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

#import <AuthKit/AKAppleIDAuthenticationController.h>
#import <AuthKit/AKAppleIDAuthenticationContext.h>
#import <AuthKit/AKAppleIDAuthenticationContext_Private.h>

#import <CloudServices/CloudServices.h>

#import <LocalAuthentication/LocalAuthentication_Private.h>

#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

#if TARGET_OS_OSX
#import <SystemConfiguration/SystemConfiguration.h>
#import <AppleSystemInfo/ASI_CPU.h>
#else
#import <MobileGestalt.h>
#endif

#import <os/feature_private.h>

#import "keychain/ot/OTEscrowRepairOperation.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTFollowup.h"
#import "keychain/ot/ObjCImprovements.h"

#import "keychain/ot/OTLAContextAdapter.h"

#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wdeprecated-declarations"

@interface OTEscrowRepairOperation ()
@property OTOperationDependencies* deps;
@property OTFollowup* followupHandler;
@property NSOperation* finishedOp;
@property AppleKeyStorePasscodeCacheReason contextType;
@end

@implementation OTEscrowRepairOperation

@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                     followupHandler:(OTFollowup*)followupHandler
                         contextType:(AppleKeyStorePasscodeCacheReason)contextType
{
    if ((self = [super init])) {
        _deps = dependencies;
        _followupHandler = followupHandler;
        _intendedState = intendedState;
        _nextState = errorState;
        _contextType = contextType;
    }
    return self;
}

- (BOOL)shouldIgnoreError:(NSError*)error
{
    if ([error.domain isEqualToString:CKErrorDomain] && error.code == CKErrorNetworkUnavailable) {
        NSError* underlyingError = error.userInfo[NSUnderlyingErrorKey];

        if (underlyingError && [underlyingError.domain isEqualToString:NSURLErrorDomain] &&
            (underlyingError.code == NSURLErrorNotConnectedToInternet || underlyingError.code == NSURLErrorUnknown ||
             underlyingError.code == NSURLErrorCannotFindHost || underlyingError.code == NSURLErrorCannotConnectToHost ||
             underlyingError.code == NSURLErrorDNSLookupFailed || underlyingError.code == NSURLErrorInternationalRoamingOff ||
             underlyingError.code == NSURLErrorDataNotAllowed || underlyingError.code == NSURLErrorCannotLoadFromNetwork)) {
            return YES;
        }
    }

    if ([error.domain isEqualToString:kCloudServicesErrorDomain] && error.code == kCloudServicesBadParametersError) {
        return YES;
    }

    if ([error.domain isEqualToString:TrustedPeersHelperErrorDomain] && error.code == TrustedPeersHelperErrorCodeFailedToLoadSecret) {
        NSError* underlyingError = error.userInfo[NSUnderlyingErrorKey];

        if([underlyingError.domain isEqualToString:@"securityd"] && underlyingError.code == errSecInteractionNotAllowed) {
            return YES;
        }
    }

    return NO;
}

- (void)groupStart
{
    WEAKIFY(self);

    if (!os_feature_enabled(Security, SEPBasedICSCHealingEnabled)) {
        secnotice("octagon-escrow-repair", "skipping escrow repair, feature flag is disabled");
        return;
    }

    AAFAnalyticsEventSecurity *event = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                altDSID:self.deps.activeAccount.altDSID
                                                                                                 flowID:self.deps.flowID
                                                                                        deviceSessionID:self.deps.deviceSessionID
                                                                                              eventName:kSecurityRTCEventNameEscrowRepairOperation
                                                                                        testsAreEnabled:SecCKKSTestsEnabled()
                                                                                         canSendMetrics:YES
                                                                                               category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
    NSString* eventName = nil;
    AAFAnalyticsEventSecurity *contextSpecificEvent = nil;

    if (self.contextType == AppleKeyStorePasscodeCacheReasonPasscodeChanged) {
        eventName = kSecurityRTCEventNameEscrowRepairOperationPasscodeChanged;
    } else if (self.contextType == AppleKeyStorePasscodeCacheReasonPasscodeUnlocked) {
        eventName = kSecurityRTCEventNameEscrowRepairOperationPasscodeUnlocked;
    } else {
        secerror("octagon-escrow-repair: unsupported context type: %ld", (long)self.contextType);
        return;
    }

    if (eventName) {
        contextSpecificEvent = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                        altDSID:self.deps.activeAccount.altDSID
                                                                                         flowID:self.deps.flowID
                                                                                deviceSessionID:self.deps.deviceSessionID
                                                                                      eventName:eventName
                                                                                testsAreEnabled:SecCKKSTestsEnabled()
                                                                                 canSendMetrics:YES
                                                                                       category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
    }

    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        STRONGIFY(self);
        if (self.error) {
            [event sendMetricWithResult:NO error:self.error];
            if (contextSpecificEvent) {
                [contextSpecificEvent sendMetricWithResult:NO error:self.error];
            }
        } else {
            [event sendMetricWithResult:YES error:nil];
            if (contextSpecificEvent) {
                [contextSpecificEvent sendMetricWithResult:YES error:nil];
            }
        }
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    NSError* getEgoPeerError = nil;
    NSString* octagonPeerID = [self.deps.stateHolder getEgoPeerID:&getEgoPeerError];
    if (!octagonPeerID || getEgoPeerError) {
        secnotice("octagon-escrow-repair", "failed to get ego peer id: %@", getEgoPeerError);
        self.error = getEgoPeerError;
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }

    ACAccount* account = nil;
    if (!SecCKKSTestsEnabled()) {
        NSError* accountError = nil;
        account = [[ACAccountStore defaultStore] accountWithIdentifier:self.deps.activeAccount.appleAccountID error:&accountError];
        if (!account) {
            secnotice("octagon-escrow-repair", "failed to get account");
            self.error = accountError;
            [self runBeforeGroupFinished:self.finishedOp];
            return;
        }
    }

    /*
     * Note about LAContext / LACredentialTypePasscodeStashSecret lifetime. The externalized context is only valid as long as
     * the associated LAContext exists. Until AKS has generated the SRP blob, the LAContext object must persist.
     */

    NSError* laError = nil;
    LAContext* laContext = nil;
    BOOL laSuccess = [self.deps.laContextAdapter setCredential:[NSData data] type:LACredentialTypePasscodeStashSecret laContext:&laContext error:&laError];
    if (laSuccess) {
        bool lockSuccess = false;
        CFErrorRef lockError = NULL;

        lockSuccess = SecAKSDoWithKeybagLockAssertion(session_keybag_handle, &lockError, ^{
            NSError* persistError = nil;
            BOOL persisted = [self.deps.stateHolder persistLastEscrowRepairAttempted:[NSDate now] error:&persistError];
            if (!persisted || persistError) {
                secnotice("octagon-escrow-repair", "failed to persist escrow repair attempt date: %@", persistError);
            }

            [self deleteRecord:octagonPeerID];
            NSError* enableError = nil;
            BOOL enableResult = [self enableWithPasscodeStashSecret:laContext.externalizedContext account:account error:&enableError];
            if (enableError && [self shouldIgnoreError:enableError] == YES) {
                secnotice("octagon-escrow-repair", "resetting last escrow repair attempt, ignored error: %@", enableError);
                NSError* clearError = nil;
                BOOL result = [self.deps.stateHolder clearLastEscrowRepairAttempt:&clearError];
                if (result == NO || clearError) {
                    secerror("octagon-escrow-repair: failed to clear last escrow repair attempt: %@", clearError);
                }
                NSError* genericError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorEscrowRepairIgnoredError userInfo:@{NSUnderlyingErrorKey : enableError}];
                self.error = genericError;
            } else if (enableResult == NO || enableError) {
                secerror("octagon-escrow-repair: failed to enable with passcode stash secret: %@", enableError);
                self.error = enableError;
            }
        });

        if (!lockSuccess) {
            self.error = (__bridge_transfer NSError*)lockError;
        }
    } else {
        secnotice("octagon-escrow-repair", "failed to retrieve passcode stash: %@", laError);
        self.error = laError;
    }

    [self runBeforeGroupFinished:self.finishedOp];
}

- (void)deleteRecord:(NSString*)peerID
{
    SecureBackup* sb = [[SecureBackup alloc] initWithUserActivityLabel:@"escrow-repair-disable"];
    sb.icdp = YES; // kSecureBackupContainsiCDPDataKey
    sb.recordID = peerID; // kSecureBackupRecordIDKey

    sb.deviceSessionID = self.deps.deviceSessionID; // kSecureBackupDeviceSessionIDKey
    sb.flowID = self.deps.flowID; // kSecureBackupFlowIDKey

    NSError* disableError = nil;
    bool success = [sb disableWithError:&disableError];
    if (success) {
        secnotice("octagon-escrow-repair", "successfully deleted escrow record");
    } else {
        secnotice("octagon-escrow-repair", "failed to delete escrow record: %@", disableError);
        // error ignored
    }
}

- (BOOL)enableWithPasscodeStashSecret:(NSData*)passcodeStashSecret account:(ACAccount*)account error:(NSError**)error
{
    SecureBackup* sb = [[SecureBackup alloc] initWithUserActivityLabel:@"escrow-repair-enable"];

    sb.icdp = YES; // kSecureBackupContainsiCDPDataKey
    sb.usesMultipleiCSC = YES; // kSecureBackupUsesMultipleiCSCKey

    sb.iCloudEnv = [account propertiesForDataclass:@"com.apple.Dataclass.Account"][@"iCloudEnv"]; // kSecureBackupAuthenticationiCloudEnvironment
    sb.authToken = account.aa_authToken; // kSecureBackupAuthenticationAuthToken
    sb.escrowProxyURL = [account propertiesForDataclass:kAccountDataclassKeychainSync][@"escrowProxyUrl"]; // kSecureBackupAuthenticationEscrowProxyURL

    sb.appleID = account.username; // kSecureBackupAuthenticationAppleID
    sb.dsid = account.aa_personID; // kSecureBackupAuthenticationDSID
    sb.iCloudPassword = [self fetchPETForUsername:account.username]; // kSecureBackupAuthenticationPassword

    // TODO: kSecureBackupStingrayMetadataHashKey (update when possible, instead of delete/enroll)

    sb.deviceSessionID = self.deps.deviceSessionID; // kSecureBackupDeviceSessionIDKey
    sb.flowID = self.deps.flowID; // kSecureBackupFlowIDKey

    sb.idmsData = [self serializedIDMSData]; // kSecureBackupIDMSDataKey

    sb.passcodeStashSecret = passcodeStashSecret;

    sb.generateClientMetadata = YES;

    NSError* enableError = nil;
    bool success = [self.deps.secureBackupAdapter enableWithSecureBackup:sb error:&enableError];
    if (success) {
        secnotice("octagon-escrow-repair", "successfully enrolled escrow record");

        NSError* clearError = nil;
        if (![self.followupHandler clearAllRepairFollowUps:self.deps.activeAccount error:&clearError]) {
            secnotice("octagon-escrow-repair", "failed to clear follow ups: %@", clearError);
        }
    } else {
        secnotice("octagon-escrow-repair", "failed to enroll escrow record: %@", enableError);
        if (error) {
            *error = enableError;
        }
        return NO;
    }
    return YES;
}

- (NSString*)fetchPETForUsername:(NSString*)username
{
    __block NSString* result = nil;

    AKAppleIDAuthenticationContext* authContext = [[AKAppleIDAuthenticationContext alloc] init];
    authContext.username = username;
    authContext.authenticationType = AKAppleIDAuthenticationTypeSilent;
    authContext.isUsernameEditable = NO;

    AKAppleIDAuthenticationController *authenticationController = [[AKAppleIDAuthenticationController alloc] init];

    // TODO: 145817503
    dispatch_semaphore_t s = dispatch_semaphore_create(0);
    [authenticationController authenticateWithContext:authContext
                                           completion:^(AKAuthenticationResults authenticationResults, NSError *error) {
        if (error) {
            secnotice("octagon-escrow-repair", "failed to fetch PET: %@", error);
        } else {
            result = authenticationResults[AKAuthenticationPasswordKey];
        }
        dispatch_semaphore_signal(s);
    }];
    dispatch_semaphore_wait(s, DISPATCH_TIME_FOREVER);

    return result;
}

- (NSData*)serializedIDMSData
{
    NSData* result = nil;

    NSError* error = nil;
    NSString* prkCandidate = [self.deps.authKitAdapter passwordResetTokenByAltDSID:self.deps.activeAccount.altDSID error:&error];
    if (prkCandidate) {
        NSDictionary* idmsDict = @{ @"prk" : prkCandidate };
        result = [NSKeyedArchiver archivedDataWithRootObject:idmsDict requiringSecureCoding:YES error:nil];
    } else {
        secnotice("octagon-escrow-repair", "failed to escrow account recovery data due to missing password reset token: %@", error);
    }

    return result;
}

@end

#pragma clang diagnostic pop

#endif // OCTAGON
