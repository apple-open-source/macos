/*
 * Copyright (c) 2026 Apple Inc. All Rights Reserved.
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

#import <CloudServices/SecureBackup.h>

#import <KeychainCircle/AAFAnalyticsEvent+Security.h>
#import <KeychainCircle/SecurityAnalyticsConstants.h>

#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTEscrowRecordManager.h"

#import "NSError+UsefulConstructors.h"

@interface SecureBackup(OTEscrowEnrollCachedEscrowRecordOperation) <OTSecureBackupEnablerProtocol>
@end

@implementation OTEscrowRecordManager

+ (OTEscrowCheckRateLimitState)effectiveRateLimitState:(OTCuttlefishAccountStateHolder*)stateHolder
                                    passcodeGeneration:(NSNumber*)passcodeGeneration
                                      intendedBottleID:(NSString*)intendedBottleID
                           computedDaysLeftOnRateLimit:(NSInteger*)daysLeftOnRateLimit
                                                 error:(NSError**)error
{
    NSError* cacheError = nil;
    OTAccountMetadataClassCEscrowRecordCache* escrowRecordCache = [stateHolder getEscrowRecordCache:&cacheError];
    if (cacheError) {
        secnotice("octagon-escrow-repair", "failed to get escrow record cache: %@", cacheError);
        if (error) {
            *error = cacheError;
        }
        return OTEscrowCheckRateLimitStateUnknown;
    }

    // If the cache is missing or invalid, then we are not rate-limited.
    if (escrowRecordCache == nil ||
        escrowRecordCache.cacheVersion != ESCROW_REPAIR_CURRENT_VERSION ||
        ![intendedBottleID isEqualToString:escrowRecordCache.bottleID] ||
        escrowRecordCache.passcodeGeneration != passcodeGeneration.unsignedLongLongValue) {
        // TODO: If cache exists but is invalid, delete it (best-effort)
        if (daysLeftOnRateLimit) {
            *daysLeftOnRateLimit = 0;
        }
        return OTEscrowCheckRateLimitStateNotRateLimited;
    }

    NSInteger timeLeft = escrowRecordCache.rateLimitTimeLeft;

    if (daysLeftOnRateLimit) {
        *daysLeftOnRateLimit = timeLeft;
    }

    if (escrowRecordCache.serializedRecord != nil) {
        return (timeLeft > 0) ? OTEscrowCheckRateLimitStateValidCacheRateLimited : OTEscrowCheckRateLimitStateValidCacheNotRateLimited;
    } else {
        return (timeLeft > 0) ? OTEscrowCheckRateLimitStateRateLimited : OTEscrowCheckRateLimitStateNotRateLimited;
    }
}

+ (BOOL)generateCachedEscrowRecordForReason:(AppleKeyStorePasscodeCacheReason)reason
                         passcodeGeneration:(NSNumber*)passcodeGeneration
                               dependencies:(OTOperationDependencies*)deps
                       cuttlefishXPCWrapper:(CuttlefishXPCWrapper*)cuttlefishXPCWrapper
                                      error:(NSError**)error
{
    ACAccount* account = nil;

    if (!SecCKKSTestsEnabled()) {
        NSError* accountError = nil;
        account = [[ACAccountStore defaultStore] accountWithIdentifier:deps.activeAccount.appleAccountID error:&accountError];
        if (!account) {
            secnotice("octagon-escrow-repair", "failed to get account: %@", accountError);
            if (error) {
                *error = accountError;
            }
            return NO;
        }
    }

    NSError* fetchError = nil;
    NSData* entropy = nil;
    NSString* bottleID = nil;
    NSData* escrowSigningSPKI = nil;

    BOOL fetchSuccess = [self fetchEscrowContentWithActiveAccount:deps.activeAccount
                                             cuttlefishXPCWrapper:cuttlefishXPCWrapper
                                                          entropy:&entropy
                                                         bottleID:&bottleID
                                                escrowSigningSPKI:&escrowSigningSPKI
                                                            error:&fetchError];
    if (!fetchSuccess || fetchError) {
        secnotice("octagon-escrow-repair", "failed to fetch escrow content: %@", fetchError);
        if (error) {
            *error = fetchError;
        }
        return NO;
    }

    BOOL success = NO;
    NSError* localError = nil;

    NSString* eventName = reason == AppleKeyStorePasscodeCacheReasonPasscodeChanged ?
        kSecurityRTCEventNameEscrowRepairGenerateRecordPasscodeChanged :
        kSecurityRTCEventNameEscrowRepairGenerateRecordPasscodeUnlocked;
    AAFAnalyticsEventSecurity* event = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                altDSID:deps.activeAccount.altDSID
                                                                                                 flowID:deps.flowID
                                                                                        deviceSessionID:deps.deviceSessionID
                                                                                              eventName:eventName
                                                                                        testsAreEnabled:SecCKKSTestsEnabled()
                                                                                         canSendMetrics:YES
                                                                                               category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    /*
     * Note about LAContext / LACredentialTypePasscodeStashSecret lifetime. The externalized context is only valid as long as
     * the associated LAContext exists. Until AKS has generated the SRP blob, the LAContext object must persist.
     */
    NSError* laError = nil;
    LAContext* laContext = nil;
    BOOL laSuccess = [deps.laContextAdapter setCredential:[NSData data] type:LACredentialTypePasscodeStashSecret laContext:&laContext error:&laError];
    if (laSuccess) {
        NSError* generateError = nil;
        BOOL generateSuccess = [self generateRecordWithPasscodeStashSecret:laContext.externalizedContext
                                                              dependencies:deps
                                                                   account:account
                                                        passcodeGeneration:passcodeGeneration
                                                                   entropy:entropy
                                                                  bottleID:bottleID
                                                         escrowSigningSPKI:escrowSigningSPKI
                                                                     error:&generateError];
        if (generateSuccess) {
            success = YES;
        } else {
            secnotice("octagon-escrow-repair", "failed to generate and store record: %@", generateError);
            localError = generateError;
        }
    } else {
        secnotice("octagon-escrow-repair", "failed to retrieve passcode stash: %@", laError);
        localError = laError;
    }

    if (success) {
        [event sendMetricWithResult:YES error:nil];
    } else {
        [event sendMetricWithResult:NO error:localError];
        if (error) {
            *error = localError;
        }
    }

    return success;
}

+ (BOOL)fetchEscrowContentWithActiveAccount:(TPSpecificUser*)activeAccount
                       cuttlefishXPCWrapper:(CuttlefishXPCWrapper*)cuttlefishXPCWrapper
                                    entropy:(NSData**)entropy
                                   bottleID:(NSString**)bottleID
                          escrowSigningSPKI:(NSData**)escrowSigningSPKI
                                      error:(NSError**)error
{
    __block NSString *localBottleID = nil;
    __block NSData *localEntropy = nil;
    __block NSData* localEscrowedSPKIKey = nil;
    __block NSError* fetchError = nil;

    [cuttlefishXPCWrapper fetchEscrowContentsWithSpecificUser:activeAccount
                                                        reply:^(NSData * _Nullable entropy,
                                                                NSString * _Nullable bottleID,
                                                                NSData * _Nullable signingPublicKey,
                                                                NSError * _Nullable returnError) {
        if (returnError) {
            secerror("Failed to get escrow contents: %@", returnError.localizedDescription);
            fetchError = returnError;
        } else {
            secnotice("octagon-escrow-repair", "Received bottle entropy for ID %@", bottleID);
            localEscrowedSPKIKey = signingPublicKey;
            localBottleID = bottleID;
            localEntropy = entropy;
        }
    }];

    if (fetchError || !localBottleID || !localEntropy || !localEscrowedSPKIKey) {
        if(error) {
            if (fetchError) {
                *error = fetchError;
            } else {
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorFailedToFetchEscrowContent description:@"Failed to fetch escrow content"];
            }
        }
        return NO;
    }

    if (entropy) {
        *entropy = localEntropy;
    }
    if (bottleID) {
        *bottleID = localBottleID;
    }
    if (escrowSigningSPKI) {
        *escrowSigningSPKI = localEscrowedSPKIKey;
    }
    return YES;
}

+ (BOOL)generateRecordWithPasscodeStashSecret:(NSData*)passcodeStashSecret
                                 dependencies:(OTOperationDependencies*)deps
                                      account:(ACAccount*)account
                           passcodeGeneration:(NSNumber*)passcodeGeneration
                                      entropy:(NSData*)entropy
                                     bottleID:(NSString*)bottleID
                            escrowSigningSPKI:(NSData*)escrowSigningSPKI
                                        error:(NSError**)error
{
    __block BOOL success = NO;
    __block NSError* localError = nil;

    SecureBackup* sb = [[SecureBackup alloc] initWithUserActivityLabel:@"escrow-repair-generate"];

    sb.icdp = YES; // kSecureBackupContainsiCDPDataKey
    sb.usesMultipleiCSC = YES; // kSecureBackupUsesMultipleiCSCKey

    sb.iCloudEnv = [account propertiesForDataclass:@"com.apple.Dataclass.Account"][@"iCloudEnv"]; // kSecureBackupAuthenticationiCloudEnvironment
    sb.authToken = account.aa_authToken; // kSecureBackupAuthenticationAuthToken
    sb.escrowProxyURL = [account propertiesForDataclass:kAccountDataclassKeychainSync][@"escrowProxyUrl"]; // kSecureBackupAuthenticationEscrowProxyURL

    sb.dsid = account.aa_personID; // kSecureBackupAuthenticationDSID

    sb.deviceSessionID = deps.deviceSessionID; // kSecureBackupDeviceSessionIDKey
    sb.flowID = deps.flowID; // kSecureBackupFlowIDKey

    sb.idmsData = [self serializedIDMSData:deps]; // kSecureBackupIDMSDataKey

    sb.passcodeStashSecret = passcodeStashSecret;

    sb.generateClientMetadata = YES;

    sb.entropy = entropy; // kSecureBackupEscrowContentEntropyKey
    sb.bottleID = bottleID; // kSecureBackupEscrowContentBottleIDKey
    sb.escrowSigningPublicKey = escrowSigningSPKI; //kSecureBackupEscrowContentSPKIKey

    sb.returnSerializedEscrowRecord = YES;

    [deps.secureBackupAdapter enableWithResults:sb reply:^(NSDictionary* enableResults, NSError* enableError) {
        if (!enableResults || enableError) {
            localError = enableError;
            return;
        }

        OTSerializedPlistEscrowRecord *serializedEscrowRecord = enableResults[kSecureBackupSerializedEscrowRecordKey];
        if (!serializedEscrowRecord) {
            localError = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorSerializedRecordNotFound description:@"serialized escrow record not returned"];
            return;
        }

        NSError* archiveError = nil;
        NSData* recordData = [NSKeyedArchiver archivedDataWithRootObject:serializedEscrowRecord requiringSecureCoding:YES error:&archiveError];
        if (!recordData || archiveError) {
            localError = archiveError;
            return;
        }

        // Object not fully populated; persistEscrowRecordCache sets cacheTimestamp and cacheVersion.
        OTAccountMetadataClassCEscrowRecordCache* cache = [[OTAccountMetadataClassCEscrowRecordCache alloc] init];
        cache.serializedRecord = recordData;
        cache.bottleID = bottleID;
        cache.passcodeGeneration = passcodeGeneration.unsignedLongLongValue;

        NSError* persistError = nil;
        if (![deps.stateHolder persistEscrowRecordCache:cache error:&persistError]) {
            secnotice("octagon-escrow-repair", "failed to persist escrow record cache: %@", persistError);
            localError = persistError;
            return;
        }

        success = YES;
    }];

    if (!success) {
        if (error) {
            *error = localError;
        }
    }

    return success;
}


+ (NSData*)serializedIDMSData:(OTOperationDependencies*)deps
{
    NSData* result = nil;

    NSError* error = nil;
    NSString* prkCandidate = [deps.authKitAdapter passwordResetTokenByAltDSID:deps.activeAccount.altDSID error:&error];
    if (prkCandidate) {
        NSDictionary* idmsDict = @{ @"prk" : prkCandidate };
        result = [NSKeyedArchiver archivedDataWithRootObject:idmsDict requiringSecureCoding:YES error:nil];
    } else {
        secnotice("octagon-escrow-repair", "failed to escrow account recovery data due to missing password reset token: %@", error);
    }

    return result;
}

+ (NSNumber* __nullable)passcodeGenerationWithError:(NSError**)error
{
    NSNumber* retPasscodeGeneration = nil;

    NSDictionary* deviceConfigurations = (__bridge_transfer NSDictionary*)MKBGetDeviceConfigurations(NULL); // CF_RETURNS_RETAINED
    if (deviceConfigurations) {
        NSNumber* passcodeGeneration = deviceConfigurations[(__bridge NSString*)kAKSConfigPasscodeGeneration];
        if ([passcodeGeneration isKindOfClass:[NSNumber class]]) {
            retPasscodeGeneration = passcodeGeneration;
        }
    }

    if (!retPasscodeGeneration && error) {
        *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorUnableToGetPasscodeGeneration userInfo:nil];
    }

    return retPasscodeGeneration;
}

@end
