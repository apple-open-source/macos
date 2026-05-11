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

#import <Security/SecInternalReleasePriv.h>

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

#import "keychain/ot/OTEscrowEnrollCachedEscrowRecordOperation.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTFollowup.h"
#import "keychain/ot/ObjCImprovements.h"

#import "keychain/ot/OTLAContextAdapter.h"

#import "NSError+UsefulConstructors.h"

@interface SecureBackup(OTEscrowEnrollCachedEscrowRecordOperation) <OTSecureBackupEnablerProtocol>
@end

NSString * const kSecureBackupNetworkReachedHintKey = @"SecureBackupNetworkReachedHint";

typedef NS_ENUM(NSInteger, SecureBackupOperationReachedNetwork) {
    SecureBackupOperationReachedNetworkUnknown = 0,
    SecureBackupOperationReachedNetworkFailed = 1,
    SecureBackupOperationReachedNetworkSuccess = 2,
};

#pragma clang diagnostic push
#pragma clang diagnostic warning "-Wdeprecated-declarations"

@interface OTEscrowEnrollCachedEscrowRecordOperation ()
@property OTOperationDependencies* deps;
@property OTFollowup* followupHandler;
@property NSOperation* finishedOp;
@end

@implementation OTEscrowEnrollCachedEscrowRecordOperation

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

    NSError* cacheError = nil;
    OTAccountMetadataClassCEscrowRecordCache* escrowRecordCache = [self.deps.stateHolder getEscrowRecordCache:&cacheError];
    if (!escrowRecordCache || cacheError) {
        secnotice("octagon-escrow-repair", "failed to get escrow record cache: %@", cacheError);
        self.error = cacheError;
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }

    NSData* encodedEscrowRecord = escrowRecordCache.serializedRecord;
    if (!encodedEscrowRecord) {
        NSError* missingRecordError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorEscrowCacheMissingRecord userInfo:nil];
        secnotice("octagon-escrow-repair", "escrow record cache missing serialized record: %@", missingRecordError);
        self.error = missingRecordError;
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }

    NSKeyedUnarchiver* unarchiver = [[NSKeyedUnarchiver alloc] initForReadingFromData:encodedEscrowRecord error:nil];
    OTSerializedPlistEscrowRecord* serializedEscrowRecord = [unarchiver decodeObjectOfClass:[OTSerializedPlistEscrowRecord class] forKey:NSKeyedArchiveRootObjectKey];
    if (!serializedEscrowRecord || ![serializedEscrowRecord isKindOfClass:[OTSerializedPlistEscrowRecord class]]) {
        NSError* badRecordError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorEscrowCacheBadRecord userInfo:nil];
        secnotice("octagon-escrow-repair", "escrow record cache unable to decode serialized record: %@", badRecordError);
        self.error = badRecordError;
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }

    NSDictionary* existingRecord = nil;
    NSError* getRecordError = nil;
    if (![self getExistingRecord:&existingRecord peerID:octagonPeerID error:&getRecordError]) {
        secnotice("octagon-escrow-repair", "failed to get existing record: %@", getRecordError);
        self.error = getRecordError;
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

    NSError* persistError = nil;
    BOOL persisted = [self.deps.stateHolder persistLastEscrowRepairAttempted:[NSDate now] error:&persistError];
    if (!persisted || persistError) {
        secnotice("octagon-escrow-repair", "failed to persist escrow repair attempt date: %@", persistError);
    }

    NSError* enableError = nil;
    BOOL enableResult = [self enableWithSerializedEscrowRecord:serializedEscrowRecord
                                                existingRecord:existingRecord
                                                       account:account
                                                 octagonPeerID:octagonPeerID
                                                         error:&enableError];

    if (!enableResult || enableError) {
        secerror("octagon-escrow-repair: failed to enable using serialized escrow record: %@", enableError);
        if (enableError == nil) {
            enableError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorFailedToStoreSerializedEscrowRecord userInfo:nil];
        }
        self.error = enableError;
    } else {
        secnotice("octagon-escrow-repair", "Successfully enabled backup");
    }

    [self runBeforeGroupFinished:self.finishedOp];
}

- (BOOL)enableWithSerializedEscrowRecord:(OTSerializedPlistEscrowRecord*)serializedEscrowRecord
                          existingRecord:(NSDictionary*)existingRecord
                                 account:(ACAccount*)account
                           octagonPeerID:(NSString*)octagonPeerID
                                   error:(NSError**)error
{
    NSError* petError = nil;
    NSString* passwordEquivalentToken = [self.deps.authKitAdapter fetchPETForUsername:account.username error:&petError];

    if (!passwordEquivalentToken) {
        if (error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorMissingPasswordEquivalentToken description:@"failed to obtain PET" underlying:petError];
        }
        return NO;
    }

    SecureBackup* sb = [[SecureBackup alloc] initWithUserActivityLabel:@"escrow-repair-enable"];

    sb.icdp = YES; // kSecureBackupContainsiCDPDataKey
    sb.usesMultipleiCSC = YES; // kSecureBackupUsesMultipleiCSCKey

    sb.iCloudEnv = [account propertiesForDataclass:@"com.apple.Dataclass.Account"][@"iCloudEnv"]; // kSecureBackupAuthenticationiCloudEnvironment
    sb.authToken = account.aa_authToken; // kSecureBackupAuthenticationAuthToken
    sb.escrowProxyURL = [account propertiesForDataclass:kAccountDataclassKeychainSync][@"escrowProxyUrl"]; // kSecureBackupAuthenticationEscrowProxyURL

    sb.appleID = account.username; // kSecureBackupAuthenticationAppleID
    sb.dsid = account.aa_personID; // kSecureBackupAuthenticationDSID
    sb.iCloudPassword = passwordEquivalentToken; // kSecureBackupAuthenticationPassword

    sb.metadataHash = existingRecord; // kSecureBackupStingrayMetadataHashKey

    sb.deviceSessionID = self.deps.deviceSessionID; // kSecureBackupDeviceSessionIDKey
    sb.flowID = self.deps.flowID; // kSecureBackupFlowIDKey

    sb.recordID = octagonPeerID; // kSecureBackupRecordIDKey

    sb.serverCheckRecordStatus = YES; // kSecureBackupServerCheckRecordStatusKey

    NSError* storeError = nil;
    BOOL success = [self.deps.secureBackupAdapter storeWithSecureBackup:sb escrowRecord:serializedEscrowRecord error:&storeError];
    if (success) {
        secnotice("octagon-escrow-repair", "successfully enrolled escrow record");

        if ([self preserveCachedRecordUponSuccess]) {
            secnotice("octagon-escrow-repair", "preserving cached escrow record");
        } else {
            [self removeCachedRecord];
        }

        NSError* clearError = nil;
        if (![self.followupHandler clearAllRepairFollowUps:self.deps.activeAccount error:&clearError]) {
            secnotice("octagon-escrow-repair", "failed to clear follow ups: %@", clearError);
        }
    } else {
        if ([self errorIndicatesRecordNonViable:storeError]) {
            secnotice("octagon-escrow-repair", "error indicates that the cached record is not viable, removing");
            [self removeCachedRecord];
        }

        secnotice("octagon-escrow-repair", "failed to store escrow record: %@", storeError);
        if (error) {
            *error = storeError;
        }
        return NO;
    }
    return YES;
}

- (BOOL)getExistingRecord:(NSDictionary**)existingRecord peerID:(NSString*)peerID error:(NSError**)error
{
    SecureBackup* sb = [[SecureBackup alloc] initWithUserActivityLabel:@"escrow-repair-get-account-info"];

    NSError* accountInfoError = nil;
    NSDictionary* accountInfo = [self.deps.secureBackupAdapter getAccountInfoWithSecureBackup:sb error:&accountInfoError];
    if (!accountInfo || accountInfoError) {
        if (error) {
            *error = accountInfoError;
        }
        return NO;
    }

    // Attempt to find existing record. CloudServices will make the appropriate call.
    // not found: enroll
    // found: update_blob
    for (NSDictionary* record in accountInfo[kSecureBackupAlliCDPRecordsKey]) {
        if ([record[kSecureBackupRecordIDKey] isEqualToString:peerID]) {
            *existingRecord = record;
            break;
        }
    }

    return YES;
}

- (BOOL)errorIndicatesRecordNonViable:(NSError*)error
{
    // ESCROW_ERROR_BLOB_NON_VIABLE
    if (error.code == -4023 && [error.domain isEqualToString:kEscrowServiceErrorDomain]) {
        return YES;
    }
    return NO;
}

- (void)removeCachedRecord
{
    NSError* stateError = nil;
    if (![self.deps.stateHolder clearCachedEscrowRecord:&stateError]) {
        secnotice("octagon-escrow-repair", "failed to remove cached escrow record: %@", stateError);
    }
}

- (BOOL)preserveCachedRecordUponSuccess
{
    if (!SecIsInternalRelease()) {
        return NO;
    }

    NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];
    return [defaults boolForKey:@"EscrowRepairPreserveCachedRecord"];
}

@end

#pragma clang diagnostic pop

#endif // OCTAGON
