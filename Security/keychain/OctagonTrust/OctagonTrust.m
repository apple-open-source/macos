/*
* Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#if __OBJC2__

#import <objc/runtime.h>

#import "OctagonTrust.h"
#import <Security/OTClique+Private.h>
#import <utilities/debugging.h>
#import "OTEscrowTranslation.h"

#import <SoftLinking/SoftLinking.h>
#import <CloudServices/SecureBackup.h>
#import <CloudServices/SecureBackupConstants.h>
#import "keychain/OctagonTrust/categories/OctagonTrustEscrowRecoverer.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTClique+Private.h"
#import <Security/OctagonSignPosts.h>
#include <utilities/SecCFRelease.h>

SOFT_LINK_FRAMEWORK(PrivateFrameworks, KeychainCircle);
SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, CloudServices);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
SOFT_LINK_CLASS(CloudServices, SecureBackup);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupErrorDomain, NSErrorDomain);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupiCDPRecordsKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupMetadataKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupRecordIDKey, NSString*);

SOFT_LINK_CONSTANT(CloudServices, kEscrowServiceRecordDataKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupKeybagDigestKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupBagPasswordKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupRecordLabelKey, NSString*);

#pragma clang diagnostic pop

static NSString * const kOTEscrowAuthKey = @"kOTEscrowAuthKey";
NSString* OTCKContainerName = @"com.apple.security.keychain";

@implementation OTConfigurationContext(Framework)

@dynamic escrowAuth;

-(void) setEscrowAuth:(id)e
{
    objc_setAssociatedObject(self, (__bridge const void * _Nonnull)(kOTEscrowAuthKey), e, OBJC_ASSOCIATION_ASSIGN);

}
- (id) escrowAuth
{
    return objc_getAssociatedObject(self, (__bridge const void * _Nonnull)(kOTEscrowAuthKey));
}

@end

@implementation OTClique(Framework)

+ (NSArray<OTEscrowRecord*>*)filterViableSOSRecords:(NSArray<OTEscrowRecord*>*)nonViable
{
    NSMutableArray<OTEscrowRecord*>* viableSOS = [NSMutableArray array];
    for(OTEscrowRecord* record in nonViable) {
        if (record.viabilityStatus == OTEscrowRecord_SOSViability_SOS_VIABLE) {
            [viableSOS addObject:record];
        }
    }

    return viableSOS;
}

/*
 * desired outcome from filtering on an iOS/macOS platform:
 *              Escrow Record data validation outcome               Outcome of fetch records
                Octagon Viable, SOS Viable                          Record gets recommended, normal Octagon and SOS join
                Octagon Viable, SOS Doesn't work                    Record gets recommended, normal Octagon join, SOS reset
                Octagon Doesn't work, SOS Viable                    Record gets recommended, Octagon reset, normal SOS join
                Octagon Doesn't work, SOS Doesn't work              no records recommended.  Force user to reset all protected data

 * desired outcome from filtering on an watchOS/tvOS/etc:
 Escrow Record data validation outcome                           Outcome of fetch records
             Octagon Viable, SOS Viable                          Record gets recommended, normal Octagon and SOS noop
             Octagon Viable, SOS Doesn't work                    Record gets recommended, normal Octagon join, SOS noop
             Octagon Doesn't work, SOS Viable                    Record gets recommended, Octagon reset, SOS noop
             Octagon Doesn't work, SOS Doesn't work              no records recommended.  Force user to reset all protected data
*/

+ (NSArray<OTEscrowRecord*>* _Nullable)filterRecords:(NSArray<OTEscrowRecord*>*)allRecords
{
    NSMutableArray<OTEscrowRecord*>* viable = [NSMutableArray array];
    NSMutableArray<OTEscrowRecord*>* partial = [NSMutableArray array];
    NSMutableArray<OTEscrowRecord*>* nonViable = [NSMutableArray array];

    for (OTEscrowRecord* record in allRecords) {
        if(record.recordViability == OTEscrowRecord_RecordViability_RECORD_VIABILITY_FULLY_VIABLE &&
           record.escrowInformationMetadata.bottleId != nil &&
           [record.escrowInformationMetadata.bottleId length] != 0) {
            [viable addObject:record];
        } else if(record.recordViability == OTEscrowRecord_RecordViability_RECORD_VIABILITY_PARTIALLY_VIABLE &&
                  record.escrowInformationMetadata.bottleId != nil &&
                  [record.escrowInformationMetadata.bottleId length] != 0) {
            [partial addObject:record];
        } else {
            [nonViable addObject:record];
        }
    }

    for(OTEscrowRecord* record in viable) {
        secnotice("octagontrust-fetchescrowrecords", "viable record: %@ serial:%@ bottleID:%@ silent allowed:%d",
                  record.label,
                  record.escrowInformationMetadata.serial,
                  record.escrowInformationMetadata.bottleId,
                  (int)record.silentAttemptAllowed);
    }
    for(OTEscrowRecord* record in partial) {
        secnotice("octagontrust-fetchescrowrecords", "partially viable record: %@ serial:%@ bottleID:%@ silent allowed:%d",
                  record.label,
                  record.escrowInformationMetadata.serial,
                  record.escrowInformationMetadata.bottleId,
                  (int)record.silentAttemptAllowed);
    }
    for(OTEscrowRecord* record in nonViable) {
        secnotice("octagontrust-fetchescrowrecords", "nonviable record: %@ serial:%@ bottleID:%@ silent allowed:%d",
                  record.label,
                  record.escrowInformationMetadata.serial,
                  record.escrowInformationMetadata.bottleId,
                  (int)record.silentAttemptAllowed);
    }

    if ([viable count] > 0) {
        secnotice("octagontrust-fetchescrowrecords", "Returning %d viable records", (int)[viable count]);
        return viable;
    }

    if ([partial count] > 0) {
        secnotice("octagontrust-fetchescrowrecords", "Returning %d partially viable records", (int)[partial count]);
        return partial;
    }

    if (OctagonPlatformSupportsSOS()) {
        NSArray<OTEscrowRecord*>* viableSOSRecords = [self filterViableSOSRecords:nonViable];
        secnotice("octagontrust-fetchescrowrecords", "Returning %d sos viable records", (int)[viableSOSRecords count]);
        return viableSOSRecords;
    }

    secnotice("octagontrust-fetchescrowrecords", "no viable records!");

    return nil;
}

+ (NSArray<OTEscrowRecord*>* _Nullable)fetchAndHandleEscrowRecords:(OTConfigurationContext*)data shouldFilter:(BOOL)shouldFiler error:(NSError**)error
{
    OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameFetchEscrowRecords);
    bool subTaskSuccess = false;

    NSError* localError = nil;
    NSArray* escrowRecordDatas = [OTClique fetchEscrowRecordsInternal:data error:&localError];
    if(localError) {
        secerror("octagontrust-fetchAndHandleEscrowRecords: failed to fetch escrow records: %@", localError);
        if(error){
            *error = localError;
        }
        OctagonSignpostEnd(signPost, OctagonSignpostNameFetchEscrowRecords, OctagonSignpostNumber1(OctagonSignpostNameFetchEscrowRecords), (int)subTaskSuccess);
        return nil;
    }

    NSMutableArray<OTEscrowRecord*>* escrowRecords = [NSMutableArray array];

    for(NSData* recordData in escrowRecordDatas){
        OTEscrowRecord* translated = [[OTEscrowRecord alloc] initWithData:recordData];
        if(translated.escrowInformationMetadata.bottleValidity == nil || [translated.escrowInformationMetadata.bottleValidity isEqualToString:@""]){
            switch(translated.recordViability) {
                case OTEscrowRecord_RecordViability_RECORD_VIABILITY_FULLY_VIABLE:
                    translated.escrowInformationMetadata.bottleValidity = @"valid";
                    break;
                case OTEscrowRecord_RecordViability_RECORD_VIABILITY_PARTIALLY_VIABLE:
                    translated.escrowInformationMetadata.bottleValidity = @"valid";
                    break;
                case OTEscrowRecord_RecordViability_RECORD_VIABILITY_LEGACY:
                    translated.escrowInformationMetadata.bottleValidity = @"invalid";
                    break;
            }
        }
        if(translated.recordId == nil || [translated.recordId isEqualToString:@""]){
            translated.recordId = [[translated.label stringByReplacingOccurrencesOfString:OTEscrowRecordPrefix withString:@""] mutableCopy];
        }
        if((translated.serialNumber == nil || [translated.serialNumber isEqualToString:@""]) && translated.escrowInformationMetadata.peerInfo != nil && [translated.escrowInformationMetadata.peerInfo length] > 0) {
            CFErrorRef cfError = NULL;
            SOSPeerInfoRef peer = SOSPeerInfoCreateFromData(kCFAllocatorDefault, &cfError, (__bridge CFDataRef)translated.escrowInformationMetadata.peerInfo);
            if(cfError != NULL){
                secnotice("octagontrust-handleEscrowRecords", "failed to create sos peer info: %@", cfError);
            } else {
                translated.serialNumber = (NSString*)CFBridgingRelease(SOSPeerInfoCopySerialNumber(peer));
            }
        }

        [escrowRecords addObject:translated];
    }
    subTaskSuccess = true;
    OctagonSignpostEnd(signPost, OctagonSignpostNameFetchEscrowRecords, OctagonSignpostNumber1(OctagonSignpostNameFetchEscrowRecords), (int)subTaskSuccess);

    if (shouldFiler == YES) {
        return [OTClique filterRecords:escrowRecords];
    }

    return escrowRecords;
}

+ (NSArray<OTEscrowRecord*>* _Nullable)fetchAllEscrowRecords:(OTConfigurationContext*)data error:(NSError**)error
{
#if OCTAGON
    secnotice("octagontrust-fetchallescrowrecords", "fetching all escrow records for context :%@, altdsid:%@", data.context, data.altDSID);
    return [OTClique fetchAndHandleEscrowRecords:data shouldFilter:NO error:error];
#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return nil;
#endif
}

+ (NSArray<OTEscrowRecord*>* _Nullable)fetchEscrowRecords:(OTConfigurationContext*)data error:(NSError**)error
{
#if OCTAGON
    if (OctagonIsOptimizationEnabled() && OctagonIsEscrowRecordFetchEnabled()) {
        secnotice("octagontrust-fetchescrowrecords", "fetching filtered escrow records for context with feature flag enabled:%@, altdsid:%@", data.context, data.altDSID);
        return [OTClique fetchAndHandleEscrowRecords:data shouldFilter:YES error:error];
    } else {
        if ([OTClique isCloudServicesAvailable] == NO) {
            if (error) {
                *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
            }
            return NULL;
        }

        OctagonSignpost signPost = OctagonSignpostBegin(OctagonSignpostNameGetAccountInfo);
        secnotice("octagontrust-fetchescrowrecords", "fetching escrow records for context with feature flag disabled:%@, altdsid:%@", data.context, data.altDSID);
        id<OctagonEscrowRecovererPrococol> sb = data.sbd ?: [[getSecureBackupClass() alloc] init];

        OTEscrowAuthenticationInformation *escrowAuth = nil;
        NSDictionary* escrowAuthDictionary = nil;
        if(data.escrowAuth != nil) {
            escrowAuth = data.escrowAuth;
            escrowAuthDictionary = [OTEscrowTranslation escrowAuthenticationInfoToDictionary:escrowAuth];
        }
        NSDictionary* results = nil;
        NSError* recoverError = [sb getAccountInfoWithInfo:escrowAuthDictionary results:&results];
        bool subTaskSuccess = false;
        if(recoverError || results == nil) {
            secerror("octagontrust-fetchescrowrecords: error fetching escrow records: %@", recoverError);
            if(error){
                *error = recoverError;
            }
            OctagonSignpostEnd(signPost, OctagonSignpostNameGetAccountInfo, OctagonSignpostNumber1(OctagonSignpostNameGetAccountInfo), (int)subTaskSuccess);
            return nil;
        } else {
            secnotice("octagontrust-fetchescrowrecords", "recovered accountWithInfo results: %@", results);
            NSArray *icdpRecords = results[getkSecureBackupiCDPRecordsKey()];
            secnotice("octagontrust-fetchescrowrecords", "recovered iCDP records: %@", icdpRecords);

            NSMutableArray<OTEscrowRecord*>* records = [NSMutableArray array];
            for (NSDictionary *dictionaryRecord in icdpRecords) {
                OTEscrowRecord* otEscrowRecord = [OTEscrowTranslation dictionaryToEscrowRecord:dictionaryRecord];
                [records addObject:otEscrowRecord];
            }
            secnotice("octagontrust-fetchescrowrecords", "translated dictionary records to escrow record protos: %@", records);

            subTaskSuccess = true;
            OctagonSignpostEnd(signPost, OctagonSignpostNameGetAccountInfo, OctagonSignpostNumber1(OctagonSignpostNameGetAccountInfo), (int)subTaskSuccess);

            return records;
        }
    }
#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return nil;
#endif
}

+ (OTClique* _Nullable)handleRecoveryResults:(OTConfigurationContext*)data recoveredInformation:(NSDictionary*)recoveredInformation sosViability:(OTEscrowRecord_SOSViability)sosViability performedSilentBurn:(BOOL)performedSilentBurn recoverError:(NSError*)recoverError error:(NSError**)error
{
    if ([self isCloudServicesAvailable] == NO) {
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
        }
        return nil;
    }

    OTClique* clique = [[OTClique alloc] initWithContextData:data];
    BOOL resetToOfferingOccured = NO;

    if(recoverError) {
        secnotice("octagontrust-handleRecoveryResults", "sbd escrow recovery failed: %@", recoverError);
        if(recoverError.code == 40 /* kSecureBackupRestoringLegacyBackupKeychainError */ && [recoverError.domain isEqualToString:getkSecureBackupErrorDomain()]) {
            if([OTClique platformSupportsSOS]) {
                secnotice("octagontrust-handleRecoveryResults", "Can't restore legacy backup with no keybag. Resetting SOS to offering");
                CFErrorRef resetToOfferingError = NULL;
                bool successfulReset = SOSCCResetToOffering(&resetToOfferingError);
                resetToOfferingOccured = YES;
                if(!successfulReset || resetToOfferingError) {
                    secerror("octagontrust-handleRecoveryResults: failed to reset to offering:%@", resetToOfferingError);
                } else {
                    secnotice("octagontrust-handleRecoveryResults", "resetting SOS circle successful");
                }
                CFReleaseNull(resetToOfferingError);
            } else {
                secnotice("octagontrust-handleRecoveryResults", "Legacy restore failed on a non-SOS platform");
            }
        } else {
            if(error) {
                *error = recoverError;
            }
            return nil;
        }
    }

    NSError* localError = nil;
    OTControl* control = nil;

    if (data.otControl) {
        control = data.otControl;
    } else {
        control = [data makeOTControl:&localError];
    }
    if (!control) {
        secerror("octagontrust-handleRecoveryResults: unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }

    // look for OT Bottles
    NSString *bottleID = recoveredInformation[@"bottleID"];
    NSString *isValid = recoveredInformation[@"bottleValid"];
    NSData *bottledPeerEntropy = recoveredInformation[@"EscrowServiceEscrowData"][@"BottledPeerEntropy"];
    bool shouldResetOctagon = false;

    if(bottledPeerEntropy && bottleID && [isValid isEqualToString:@"valid"]){
        secnotice("octagontrust-handleRecoveryResults", "recovering from bottle: %@", bottleID);
        __block NSError* restoreBottleError = nil;
        OctagonSignpost bottleRestoreSignPost = performedSilentBurn ? OctagonSignpostBegin(OctagonSignpostNamePerformOctagonJoinForSilent) : OctagonSignpostBegin(OctagonSignpostNamePerformOctagonJoinForNonSilent);

        //restore bottle!
        [control restore:OTCKContainerName
               contextID:data.context
              bottleSalt:data.altDSID
                 entropy:bottledPeerEntropy
                bottleID:bottleID
                   reply:^(NSError * _Nullable restoreError) {
            if(restoreError) {
                secnotice("octagontrust-handleRecoveryResults", "restore bottle errored: %@", restoreError);
            } else {
                secnotice("octagontrust-handleRecoveryResults", "restoring bottle succeeded");
            }
            restoreBottleError = restoreError;
            if (performedSilentBurn) {
                OctagonSignpostEnd(bottleRestoreSignPost, OctagonSignpostNamePerformOctagonJoinForSilent, OctagonSignpostNumber1(OctagonSignpostNamePerformOctagonJoinForSilent), (int)true);
            } else {
                OctagonSignpostEnd(bottleRestoreSignPost, OctagonSignpostNamePerformOctagonJoinForNonSilent, OctagonSignpostNumber1(OctagonSignpostNamePerformOctagonJoinForNonSilent), (int)true);
            }
        }];

        if(restoreBottleError) {
            if(error){
                *error = restoreBottleError;
            }
            return nil;
        }
    } else {
        shouldResetOctagon = true;
    }

    if(shouldResetOctagon) {
        secnotice("octagontrust-handleRecoveryResults", "bottle %@ is not valid, resetting octagon", bottleID);
        NSError* resetError = nil;
        [clique resetAndEstablish:CuttlefishResetReasonNoBottleDuringEscrowRecovery error:&resetError];
        if(resetError) {
            secerror("octagontrust-handleRecoveryResults: failed to reset octagon: %@", resetError);
            if(error){
                *error = resetError;
            }
            return nil;
        } else{
            secnotice("octagontrust-handleRecoveryResults", "reset octagon succeeded");
        }
    }

    // Join SOS circle if platform is supported and we didn't previously reset SOS
    if (OctagonPlatformSupportsSOS() && resetToOfferingOccured == NO) {
        if (sosViability == OTEscrowRecord_SOSViability_SOS_NOT_VIABLE) {
            secnotice("octagontrust-handleRecoveryResults", "Record will not allow device to join SOS.  Invoking reset to offering");
            CFErrorRef resetToOfferingError = NULL;
            bool successfulReset = SOSCCResetToOffering(&resetToOfferingError);
            if(!successfulReset || resetToOfferingError) {
                secerror("octagontrust-handleRecoveryResults: failed to reset to offering:%@", resetToOfferingError);
            } else {
                secnotice("octagontrust-handleRecoveryResults", "resetting SOS circle successful");
            }
            CFReleaseNull(resetToOfferingError);
        } else {
            NSError* joinAfterRestoreError = nil;
            secnotice("octagontrust-handleRecoveryResults", "attempting joinAfterRestore");
            BOOL joinAfterRestoreResult = [clique joinAfterRestore:&joinAfterRestoreError];
            if(joinAfterRestoreError || joinAfterRestoreResult == NO) {
                secnotice("octagontrust-handleRecoveryResults", "failed to join after restore: %@", joinAfterRestoreError);
            } else {
                secnotice("octagontrust-handleRecoveryResults", "joinAfterRestore succeeded");
            }
        }
    }


    // call SBD to kick off keychain data restore
    id<OctagonEscrowRecovererPrococol> sb = data.sbd ?: [[getSecureBackupClass() alloc] init];
    NSError* restoreError = nil;

    NSMutableSet <NSString *> *viewsNotToBeRestored = [NSMutableSet set];
    [viewsNotToBeRestored addObject:@"iCloudIdentity"];
    [viewsNotToBeRestored addObject:@"PCS-MasterKey"];
    [viewsNotToBeRestored addObject:@"KeychainV0"];

    NSDictionary *escrowRecords = recoveredInformation[getkEscrowServiceRecordDataKey()];
    if (escrowRecords == nil) {
        secnotice("octagontrust-handleRecoveryResults", "unable to request keychain restore, record data missing");
        return clique;
    }


    NSData *keybagDigest = escrowRecords[getkSecureBackupKeybagDigestKey()];
    NSData *password = escrowRecords[getkSecureBackupBagPasswordKey()];
    if (keybagDigest == nil || password == nil) {
        secnotice("octagontrust-handleRecoveryResults", "unable to request keychain restore, digest or password missing");
        return clique;
    }

    BOOL haveBottledPeer = (bottledPeerEntropy && bottleID && [isValid isEqualToString:@"valid"]) || shouldResetOctagon;
    [sb restoreKeychainAsyncWithPassword:password
                            keybagDigest:keybagDigest
                         haveBottledPeer:haveBottledPeer
                    viewsNotToBeRestored:viewsNotToBeRestored
                                   error:&restoreError];

    if (restoreError) {
        secerror("octagontrust-handleRecoveryResults: error restoring keychain items: %@", restoreError);
    }

    return clique;
}

+ (instancetype _Nullable)performEscrowRecovery:(OTConfigurationContext*)data
                                     cdpContext:(OTICDPRecordContext*)cdpContext
                                   escrowRecord:(OTEscrowRecord*)escrowRecord
                                          error:(NSError**)error
{
#if OCTAGON
    secnotice("octagontrust-performEscrowRecovery", "performEscrowRecovery invoked for context:%@, altdsid:%@", data.context, data.altDSID);

    if ([self isCloudServicesAvailable] == NO) {
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
        }
        return nil;
    }

    OctagonSignpost performEscrowRecoverySignpost = OctagonSignpostBegin(OctagonSignpostNamePerformEscrowRecovery);
    bool subTaskSuccess = true;

    id<OctagonEscrowRecovererPrococol> sb = data.sbd ?: [[getSecureBackupClass() alloc] init];
    NSDictionary* recoveredInformation = nil;
    NSError* recoverError = nil;

    BOOL supportedRestorePath = [OTEscrowTranslation supportedRestorePath:cdpContext];
    secnotice("octagontrust-performEscrowRecovery", "restore path is supported? %@", supportedRestorePath ? @"YES" : @"NO");

    if (OctagonIsOptimizationEnabled() && supportedRestorePath) {
        secnotice("octagontrust-performEscrowRecovery", "optimization flag turned on");
        OctagonSignpost recoverFromSBDSignPost = OctagonSignpostBegin(OctagonSignpostNameRecoverWithCDPContext);
        recoveredInformation = [sb recoverWithCDPContext:cdpContext escrowRecord:escrowRecord error:&recoverError];
        subTaskSuccess = (recoverError == nil) ? true : false;
        OctagonSignpostEnd(recoverFromSBDSignPost, OctagonSignpostNameRecoverWithCDPContext, OctagonSignpostNumber1(OctagonSignpostNameRecoverWithCDPContext), (int)subTaskSuccess);
    } else {
        secnotice("octagontrust-performEscrowRecovery", "optimization flag turned off");
        NSMutableDictionary* sbdRecoveryArguments = [[OTEscrowTranslation CDPRecordContextToDictionary:cdpContext] mutableCopy];
        NSDictionary* metadata = [OTEscrowTranslation metadataToDictionary:escrowRecord.escrowInformationMetadata];
        sbdRecoveryArguments[getkSecureBackupMetadataKey()] = metadata;
        sbdRecoveryArguments[getkSecureBackupRecordIDKey()] = escrowRecord.recordId;
        secnotice("octagontrust-performEscrowRecovery", "using sbdRecoveryArguments: %@", sbdRecoveryArguments);

        OctagonSignpost recoverFromSBDSignPost = OctagonSignpostBegin(OctagonSignpostNamePerformRecoveryFromSBD);
        recoverError = [sb recoverWithInfo:sbdRecoveryArguments results:&recoveredInformation];
        subTaskSuccess = (recoverError == nil) ? true : false;
        OctagonSignpostEnd(recoverFromSBDSignPost, OctagonSignpostNamePerformRecoveryFromSBD, OctagonSignpostNumber1(OctagonSignpostNamePerformRecoveryFromSBD), (int)subTaskSuccess);
    }

    OTEscrowRecord_SOSViability viableForSOS = escrowRecord.viabilityStatus;

    OTClique* clique = [OTClique handleRecoveryResults:data recoveredInformation:recoveredInformation sosViability:viableForSOS performedSilentBurn:NO recoverError:recoverError error:error];

    if(recoverError) {
        subTaskSuccess = false;
        OctagonSignpostEnd(performEscrowRecoverySignpost, OctagonSignpostNamePerformEscrowRecovery, OctagonSignpostNumber1(OctagonSignpostNamePerformEscrowRecovery), (int)subTaskSuccess);
    } else {
        OctagonSignpostEnd(performEscrowRecoverySignpost, OctagonSignpostNamePerformEscrowRecovery, OctagonSignpostNumber1(OctagonSignpostNamePerformEscrowRecovery), (int)subTaskSuccess);
    }
    return clique;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return nil;
#endif
}

+ (OTEscrowRecord* _Nullable) recordMatchingLabel:(NSString*)label allRecords:(NSArray<OTEscrowRecord*>*)allRecords
{
    for(OTEscrowRecord* record in allRecords) {
        if ([record.label isEqualToString:label]) {
            return record;
        }
    }
    return nil;
}

+ (instancetype _Nullable)performSilentEscrowRecovery:(OTConfigurationContext*)data
                                           cdpContext:(OTICDPRecordContext*)cdpContext
                                           allRecords:(NSArray<OTEscrowRecord*>*)allRecords
                                                error:(NSError**)error
{
#if OCTAGON
    secnotice("octagontrust-performSilentEscrowRecovery", "performSilentEscrowRecovery invoked for context:%@, altdsid:%@", data.context, data.altDSID);

    if ([self isCloudServicesAvailable] == NO) {
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
        }
        return nil;
    }

    OctagonSignpost performSilentEscrowRecoverySignpost = OctagonSignpostBegin(OctagonSignpostNamePerformSilentEscrowRecovery);
    bool subTaskSuccess = true;

    id<OctagonEscrowRecovererPrococol> sb = data.sbd ?: [[getSecureBackupClass() alloc] init];
    NSDictionary* recoveredInformation = nil;
    NSError* recoverError = nil;

    BOOL supportedRestorePath = [OTEscrowTranslation supportedRestorePath:cdpContext];
    secnotice("octagontrust-performSilentEscrowRecovery", "restore path is supported? %@", supportedRestorePath ? @"YES" : @"NO");

    if (OctagonIsOptimizationEnabled() && supportedRestorePath) {
        secnotice("octagontrust-performSilentEscrowRecovery", "optimization flag turned on");
        OctagonSignpost recoverFromSBDSignPost = OctagonSignpostBegin(OctagonSignpostNameRecoverSilentWithCDPContext);
        recoveredInformation = [sb recoverSilentWithCDPContext:cdpContext allRecords:allRecords error:&recoverError];
        subTaskSuccess = (recoverError == nil) ? true : false;
        OctagonSignpostEnd(recoverFromSBDSignPost, OctagonSignpostNameRecoverSilentWithCDPContext, OctagonSignpostNumber1(OctagonSignpostNameRecoverSilentWithCDPContext), (int)subTaskSuccess);
    } else {
        secnotice("octagontrust-performSilentEscrowRecovery", "optimization flag turned off");
        NSDictionary* sbdRecoveryArguments = [OTEscrowTranslation CDPRecordContextToDictionary:cdpContext];

        OctagonSignpost recoverFromSBDSignPost = OctagonSignpostBegin(OctagonSignpostNamePerformRecoveryFromSBD);
        recoverError = [sb recoverWithInfo:sbdRecoveryArguments results:&recoveredInformation];
        subTaskSuccess = (recoverError == nil) ? true : false;
        OctagonSignpostEnd(recoverFromSBDSignPost, OctagonSignpostNamePerformRecoveryFromSBD, OctagonSignpostNumber1(OctagonSignpostNamePerformRecoveryFromSBD), (int)subTaskSuccess);
    }

    NSString* label = recoveredInformation[getkSecureBackupRecordLabelKey()];
    OTEscrowRecord* chosenRecord = [OTClique recordMatchingLabel:label allRecords:allRecords];
    OTEscrowRecord_SOSViability viableForSOS = chosenRecord ? chosenRecord.viabilityStatus : OTEscrowRecord_SOSViability_SOS_VIABLE_UNKNOWN;

    OTClique *clique = [OTClique handleRecoveryResults:data recoveredInformation:recoveredInformation sosViability:viableForSOS performedSilentBurn:YES recoverError:recoverError error:error];

    if(recoverError) {
        subTaskSuccess = false;
        OctagonSignpostEnd(performSilentEscrowRecoverySignpost, OctagonSignpostNamePerformSilentEscrowRecovery, OctagonSignpostNumber1(OctagonSignpostNamePerformSilentEscrowRecovery), (int)subTaskSuccess);
    } else {
        OctagonSignpostEnd(performSilentEscrowRecoverySignpost, OctagonSignpostNamePerformSilentEscrowRecovery, OctagonSignpostNumber1(OctagonSignpostNamePerformSilentEscrowRecovery), (int)subTaskSuccess);
    }

    return clique;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return nil;
#endif
}

+ (BOOL) invalidateEscrowCache:(OTConfigurationContext*)configurationContext error:(NSError**)error
{
#if OCTAGON
    secnotice("octagontrust-invalidateEscrowCache", "invalidateEscrowCache invoked for context:%@, altdsid:%@", configurationContext.context, configurationContext.altDSID);
    __block NSError* localError = nil;
    __block BOOL invalidatedCache = NO;

    OTControl *control = [configurationContext makeOTControl:&localError];
    if (!control) {
        secnotice("clique-invalidateEscrowCache", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return invalidatedCache;
    }

    [control invalidateEscrowCache:OTCKContainerName
                         contextID:configurationContext.context
                             reply:^(NSError * _Nullable invalidateError) {
        if(invalidateError) {
            secnotice("clique-invalidateEscrowCache", "invalidateEscrowCache errored: %@", invalidateError);
        } else {
            secnotice("clique-invalidateEscrowCache", "invalidateEscrowCache succeeded");
            invalidatedCache = YES;
        }
        localError = invalidateError;
    }];

    if(error && localError) {
        *error = localError;
    }

    secnotice("clique-invalidateEscrowCache", "invalidateEscrowCache complete");

    return invalidatedCache;
#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

@end

#endif
