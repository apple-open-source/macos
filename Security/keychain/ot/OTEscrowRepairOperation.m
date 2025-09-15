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

#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wdeprecated-declarations"

@interface OTEscrowRepairOperation ()
@property OTOperationDependencies* deps;
@property OTFollowup* followupHandler;
@property NSOperation* finishedOp;
@end

@implementation OTEscrowRepairOperation

@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                     followupHandler:(OTFollowup*)followupHandler
{
    if ((self = [super init])) {
        _deps = dependencies;
        _followupHandler = followupHandler;
        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
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

    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        STRONGIFY(self);
        if (self.error) {
            [event sendMetricWithResult:NO error:self.error];
        } else {
            [event sendMetricWithResult:YES error:nil];
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

    NSError* accountError = nil;
    ACAccount* account = [[ACAccountStore defaultStore] accountWithIdentifier:self.deps.activeAccount.appleAccountID error:&accountError];
    if (!account) {
        secnotice("octagon-escrow-repair", "failed to get account");
        self.error = accountError;
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }

    /*
     * Note about LAContext / LACredentialTypePasscodeStashSecret lifetime. The externalized context is only valid as long as
     * the associated LAContext exists. Until AKS has generated the SRP blob, the LAContext object must persist.
     */

    NSError* laError = nil;
    LAContext* laContext = [[LAContext alloc] init];
    BOOL laSuccess = [laContext setCredential:[NSData data] type:LACredentialTypePasscodeStashSecret error:&laError];

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
            [self enableWithPasscodeStashSecret:laContext.externalizedContext account:account];
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

- (void)enableWithPasscodeStashSecret:(NSData*)passcodeStashSecret account:(ACAccount*)account
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
    bool success = [sb enableWithError:&enableError];
    if (success) {
        secnotice("octagon-escrow-repair", "successfully enrolled escrow record");

        NSError* clearError = nil;
        if (![self.followupHandler clearAllRepairFollowUps:self.deps.activeAccount error:&clearError]) {
            secnotice("octagon-escrow-repair", "failed to clear follow ups: %@", clearError);
        }
    } else {
        secnotice("octagon-escrow-repair", "failed to enroll escrow record: %@", enableError);
        self.error = enableError;
    }
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
