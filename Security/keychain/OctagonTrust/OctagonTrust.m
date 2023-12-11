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
#import "utilities/debugging.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "OTEscrowTranslation.h"

#import <SoftLinking/SoftLinking.h>
#import <CloudServices/SecureBackup.h>
#import <CloudServices/SecureBackupConstants.h>
#import "keychain/OctagonTrust/categories/OctagonTrustEscrowRecoverer.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTClique+Private.h"
#import <Security/OctagonSignPosts.h>
#include "utilities/SecCFRelease.h"
#import <Security/SecPasswordGenerate.h>

SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, CloudServices);

SOFT_LINK_CLASS(CloudServices, SecureBackup);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupErrorDomain, NSErrorDomain);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupMetadataKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupRecordIDKey, NSString*);

SOFT_LINK_CONSTANT(CloudServices, kEscrowServiceRecordDataKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupKeybagDigestKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupBagPasswordKey, NSString*);
SOFT_LINK_CONSTANT(CloudServices, kSecureBackupRecordLabelKey, NSString*);

static NSString * const kOTEscrowAuthKey = @"kOTEscrowAuthKey";

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

+ (NSArray<OTEscrowRecord*>*)sortListPrioritizingiOSRecords:(NSArray<OTEscrowRecord*>*)list
{
    NSMutableArray<OTEscrowRecord*>* numericFirst = [NSMutableArray array];
    NSMutableArray<OTEscrowRecord*>* leftover = [NSMutableArray array];

    for (OTEscrowRecord* record in list) {
        if (record.escrowInformationMetadata.clientMetadata.hasSecureBackupUsesNumericPassphrase &&
            record.escrowInformationMetadata.clientMetadata.secureBackupUsesNumericPassphrase) {

            [numericFirst addObject:record];
        } else {
            [leftover addObject:record];
        }
    }

    [numericFirst addObjectsFromArray:leftover];

    return numericFirst;
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
        secnotice("octagontrust-fetchescrowrecords", "viable record: %@ serial:%@ bottleID:%@ silent allowed:%{bool}d",
                  record.label,
                  record.escrowInformationMetadata.serial,
                  record.escrowInformationMetadata.bottleId,
                  (int)record.silentAttemptAllowed);
    }
    for(OTEscrowRecord* record in partial) {
        secnotice("octagontrust-fetchescrowrecords", "partially viable record: %@ serial:%@ bottleID:%@ silent allowed:%{bool}d",
                  record.label,
                  record.escrowInformationMetadata.serial,
                  record.escrowInformationMetadata.bottleId,
                  (int)record.silentAttemptAllowed);
    }
    for(OTEscrowRecord* record in nonViable) {
        secnotice("octagontrust-fetchescrowrecords", "nonviable record: %@ serial:%@ bottleID:%@ silent allowed:%{bool}d",
                  record.label,
                  record.escrowInformationMetadata.serial,
                  record.escrowInformationMetadata.bottleId,
                  (int)record.silentAttemptAllowed);
    }

    if ([viable count] > 0) {
        secnotice("octagontrust-fetchescrowrecords", "Returning %d viable records", (int)[viable count]);
        return [self sortListPrioritizingiOSRecords:viable];
    }

    if ([partial count] > 0) {
        secnotice("octagontrust-fetchescrowrecords", "Returning %d partially viable records", (int)[partial count]);
        return [self sortListPrioritizingiOSRecords:partial];
    }

    if (SOSCCIsSOSTrustAndSyncingEnabled()) {
        NSArray<OTEscrowRecord*>* viableSOSRecords = [self filterViableSOSRecords:nonViable];
        secnotice("octagontrust-fetchescrowrecords", "Returning %d sos viable records", (int)[viableSOSRecords count]);
        return [self sortListPrioritizingiOSRecords:viableSOSRecords];
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
            CFReleaseNull(cfError);
            CFReleaseNull(peer);
        } else {
            if ((translated.serialNumber == nil || [translated.serialNumber isEqualToString:@""]) &&
                (translated.escrowInformationMetadata.serial != nil && [translated.escrowInformationMetadata.serial isEqualToString:@""] == NO)) {
                translated.serialNumber = translated.escrowInformationMetadata.serial;
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
    secnotice("octagontrust-fetchescrowrecords", "fetching filtered escrow records for context:%@, altdsid:%@", data.context, data.altDSID);
    return [OTClique fetchAndHandleEscrowRecords:data shouldFilter:YES error:error];
#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return nil;
#endif
}

+ (OTClique* _Nullable)handleRecoveryResults:(OTConfigurationContext*)data recoveredInformation:(NSDictionary*)recoveredInformation record:(OTEscrowRecord*)record performedSilentBurn:(BOOL)performedSilentBurn recoverError:(NSError*)recoverError error:(NSError**)error
{
    if ([self isCloudServicesAvailable] == NO) {
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
        }
        return nil;
    }

    OTClique* clique = [[OTClique alloc] initWithContextData:data];
    
    if (recoverError) {
        secerror("octagontrust-handleRecoveryResults: sbd escrow recovery failed: %@", recoverError);
        if (error) {
            *error = recoverError;
        }
        return nil;
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
        [control restoreFromBottle:[[OTControlArguments alloc] initWithConfiguration:data]
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
        [clique resetAndEstablish:CuttlefishResetReasonNoBottleDuringEscrowRecovery
                idmsTargetContext:nil
           idmsCuttlefishPassword:nil
                       notifyIdMS:false
                  accountSettings:nil
                            error:&resetError];
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
    secnotice("octagontrust-performEscrowRecovery", "restore path is supported? %{BOOL}d", supportedRestorePath);

    if (supportedRestorePath) {
        OctagonSignpost recoverFromSBDSignPost = OctagonSignpostBegin(OctagonSignpostNameRecoverWithCDPContext);
        recoveredInformation = [sb recoverWithCDPContext:cdpContext escrowRecord:escrowRecord error:&recoverError];
        subTaskSuccess = (recoverError == nil) ? true : false;
        OctagonSignpostEnd(recoverFromSBDSignPost, OctagonSignpostNameRecoverWithCDPContext, OctagonSignpostNumber1(OctagonSignpostNameRecoverWithCDPContext), (int)subTaskSuccess);
    } else {
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

    OTClique* clique = [OTClique handleRecoveryResults:data recoveredInformation:recoveredInformation record:escrowRecord performedSilentBurn:NO recoverError:recoverError error:error];

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
    secnotice("octagontrust-performSilentEscrowRecovery", "restore path is supported? %{BOOL}d", supportedRestorePath);

    if (supportedRestorePath) {
        OctagonSignpost recoverFromSBDSignPost = OctagonSignpostBegin(OctagonSignpostNameRecoverSilentWithCDPContext);
        recoveredInformation = [sb recoverSilentWithCDPContext:cdpContext allRecords:allRecords error:&recoverError];
        subTaskSuccess = (recoverError == nil) ? true : false;
        OctagonSignpostEnd(recoverFromSBDSignPost, OctagonSignpostNameRecoverSilentWithCDPContext, OctagonSignpostNumber1(OctagonSignpostNameRecoverSilentWithCDPContext), (int)subTaskSuccess);
    } else {
        NSDictionary* sbdRecoveryArguments = [OTEscrowTranslation CDPRecordContextToDictionary:cdpContext];

        OctagonSignpost recoverFromSBDSignPost = OctagonSignpostBegin(OctagonSignpostNamePerformRecoveryFromSBD);
        recoverError = [sb recoverWithInfo:sbdRecoveryArguments results:&recoveredInformation];
        subTaskSuccess = (recoverError == nil) ? true : false;
        OctagonSignpostEnd(recoverFromSBDSignPost, OctagonSignpostNamePerformRecoveryFromSBD, OctagonSignpostNumber1(OctagonSignpostNamePerformRecoveryFromSBD), (int)subTaskSuccess);
    }

    NSString* label = recoveredInformation[getkSecureBackupRecordLabelKey()];
    OTEscrowRecord* chosenRecord = [OTClique recordMatchingLabel:label allRecords:allRecords];

    OTClique *clique = [OTClique handleRecoveryResults:data recoveredInformation:recoveredInformation record:chosenRecord performedSilentBurn:YES recoverError:recoverError error:error];

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

+ (BOOL)invalidateEscrowCache:(OTConfigurationContext*)configurationContext error:(NSError**)error
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

    [control invalidateEscrowCache:[[OTControlArguments alloc] initWithConfiguration:configurationContext]
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

- (BOOL)setLocalSecureElementIdentity:(OTSecureElementPeerIdentity*)secureElementIdentity
                                error:(NSError**)error
{
#if OCTAGON
    secnotice("octagontrust-se", "setLocalSecureElementIdentity invoked for context:%@", self.ctx);
    __block NSError* localError = nil;

    OTControl *control = [self.ctx makeOTControl:&localError];
    if (!control) {
        secnotice("octagontrust-se", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }

    [control setLocalSecureElementIdentity:[[OTControlArguments alloc] initWithConfiguration:self.ctx]
                     secureElementIdentity:secureElementIdentity
                                     reply:^(NSError* _Nullable replyError) {
        if(replyError) {
            secnotice("octagontrust-se", "setLocalSecureElementIdentity errored: %@", replyError);
        } else {
            secnotice("octagontrust-se", "setLocalSecureElementIdentity succeeded");
        }

        localError = replyError;
    }];

    if(error && localError) {
        *error = localError;
    }
    return localError == nil;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

- (BOOL)removeLocalSecureElementIdentityPeerID:(NSData*)sePeerID
                                         error:(NSError**)error
{
#if OCTAGON
    secnotice("octagontrust-se", "removeLocalSecureElementIdentityPeerID invoked for context:%@", self.ctx);
    __block NSError* localError = nil;

    OTControl *control = [self.ctx makeOTControl:&localError];
    if (!control) {
        secnotice("octagontrust-se", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }

    [control removeLocalSecureElementIdentityPeerID:[[OTControlArguments alloc] initWithConfiguration:self.ctx]
                        secureElementIdentityPeerID:sePeerID
                                              reply:^(NSError* _Nullable replyError) {
        if(replyError) {
            secnotice("octagontrust-se", "removeLocalSecureElementIdentityPeerID errored: %@", replyError);
        } else {
            secnotice("octagontrust-se", "removeLocalSecureElementIdentityPeerID succeeded");
        }

        localError = replyError;
    }];

    if(error && localError) {
        *error = localError;
    }
    return localError == nil;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

- (OTCurrentSecureElementIdentities* _Nullable)fetchTrustedSecureElementIdentities:(NSError**)error
{
#if OCTAGON
    secnotice("octagontrust-se", "fetchTrustedSecureElementIdentities invoked for context:%@", self.ctx);
    __block NSError* localError = nil;

    OTControl *control = [self.ctx makeOTControl:&localError];
    if (!control) {
        secnotice("octagontrust-se", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }

    __block OTCurrentSecureElementIdentities* ret = nil;

    [control fetchTrustedSecureElementIdentities:[[OTControlArguments alloc] initWithConfiguration:self.ctx]
                                           reply:^(OTCurrentSecureElementIdentities* currentSet,
                                                   NSError* replyError) {
        if(replyError) {
            secnotice("octagontrust-se", "fetchTrustedSecureElementIdentities errored: %@", replyError);
        } else {
            secnotice("octagontrust-se", "fetchTrustedSecureElementIdentities succeeded");
            ret = currentSet;
        }

        localError = replyError;
    }];

    if(error && localError) {
        *error = localError;
    }
    return ret;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

- (BOOL)setAccountSetting:(OTAccountSettings*)settings error:(NSError**)error
{
#if OCTAGON
    secnotice("octagontrust-settings", "setAccountSetting invoked for context:%@", self.ctx);
    __block NSError* localError = nil;

    OTControl *control = [self.ctx makeOTControl:&localError];
    if (!control) {
        secnotice("octagontrust-settings", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }

    [control setAccountSetting:[[OTControlArguments alloc] initWithConfiguration:self.ctx]
                       setting:settings
                         reply:^(NSError * _Nullable replyError) {
        if(replyError) {
            secnotice("octagontrust-settings", "setAccountSetting errored: %@", replyError);
        } else {
            secnotice("octagontrust-settings", "setAccountSetting succeeded");
        }

        localError = replyError;
    }];

    if(error && localError) {
        *error = localError;
    }
    return localError == nil;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif /* OCTAGON */
}


- (OTAccountSettings*)fetchAccountSettings:(NSError**)error
{
#if OCTAGON
    secnotice("octagontrust-fetch-settings", "fetchAccountSettings invoked for context:%@", self.ctx);
    __block NSError* localError = nil;

    OTControl *control = [self.ctx makeOTControl:&localError];
    if (!control) {
        secnotice("octagontrust-fetch-settings", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }

    __block OTAccountSettings* retSetting = nil;

    [control fetchAccountSettings:[[OTControlArguments alloc] initWithConfiguration:self.ctx]
                            reply:^(OTAccountSettings* setting, NSError * _Nullable replyError) {
        if(replyError) {
            secnotice("octagontrust-fetch-settings", "fetchAccountSettings errored: %@", replyError);
        } else {
            secnotice("octagontrust-fetch-settings", "fetchAccountSettings succeeded");
            retSetting = setting;
        }

        localError = replyError;
    }];

    if(error && localError) {
        *error = localError;
    }

    return retSetting;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif /* OCTAGON */
}

+ (OTAccountSettings* _Nullable)_fetchAccountWideSettingsDefaultWithForceFetch:(bool)forceFetch
                                                                    useDefault:(bool)useDefault
                                                                 configuration:(OTConfigurationContext*)configurationContext
                                                                         error:(NSError**)error
{
#if OCTAGON
    secnotice("octagontrust-fetch-account-wide-settings", "fetchAccountWideSettings invoked for context:%@ forceFetch:%{bool}d", configurationContext, forceFetch);

    __block NSError* localError = nil;

    OTControl *control = [configurationContext makeOTControl:&localError];
    if (!control) {
        secnotice("octagontrust-fetch-account-wide-settings", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }

    __block OTAccountSettings* retSetting = nil;

    [control fetchAccountWideSettingsWithForceFetch:forceFetch
                                          arguments:[[OTControlArguments alloc] initWithConfiguration:configurationContext]
                                              reply:^(OTAccountSettings* setting, NSError *replyError) {
        if(replyError) {
            if (useDefault && replyError.code == OctagonErrorNoAccountSettingsSet && [replyError.domain isEqualToString:OctagonErrorDomain]) {
                OTAccountSettings* accountSettings = [[OTAccountSettings alloc] init];
                accountSettings.walrus = [[OTWalrus alloc] init];
                accountSettings.walrus.enabled = false;
                accountSettings.webAccess = [[OTWebAccess alloc] init];
                accountSettings.webAccess.enabled = true;
                retSetting = accountSettings;
                replyError = nil;
                secnotice("octagontrust-fetch-account-wide-settings", "fetchAccountWideSettings succeeded (returning default)");
            } else {
                secnotice("octagontrust-fetch-account-wide-settings", "fetchAccountWideSettings errored: %@", replyError);
            }
        } else {
            secnotice("octagontrust-fetch-account-wide-settings", "fetchAccountWideSettings succeeded");
            retSetting = setting;
        }

        localError = replyError;
    }];

    if(error && localError) {
        *error = localError;
    }

    return retSetting;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif /* OCTAGON */
}

+ (OTAccountSettings* _Nullable)fetchAccountWideSettingsWithForceFetch:(bool)forceFetch
                                                         configuration:(OTConfigurationContext*)configurationContext
                                                                 error:(NSError**)error
{
    return [OTClique _fetchAccountWideSettingsDefaultWithForceFetch:forceFetch useDefault:false configuration:configurationContext error:error];
}

+ (OTAccountSettings* _Nullable)fetchAccountWideSettingsDefaultWithForceFetch:(bool)forceFetch
                                                                configuration:(OTConfigurationContext*)configurationContext
                                                                        error:(NSError**)error
{
    return [OTClique _fetchAccountWideSettingsDefaultWithForceFetch:forceFetch useDefault:true configuration:configurationContext error:error];
}

+ (OTAccountSettings* _Nullable)fetchAccountWideSettings:(OTConfigurationContext*)configurationContext error:(NSError**)error
{
    return [OTClique _fetchAccountWideSettingsDefaultWithForceFetch:false useDefault:false configuration:configurationContext error:error];
}

- (BOOL)waitForPriorityViewKeychainDataRecovery:(NSError**)error
{
#if OCTAGON
    secnotice("octagn-ckks", "waitForPriorityViewKeychainDataRecovery invoked for context:%@", self.ctx);
    __block NSError* localError = nil;

    OTControl *control = [self.ctx makeOTControl:&localError];
    if (!control) {
        secnotice("octagn-ckks", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }

    [control waitForPriorityViewKeychainDataRecovery:[[OTControlArguments alloc] initWithConfiguration:self.ctx]
                                               reply:^(NSError * _Nullable replyError) {
        if(replyError) {
            secnotice("octagn-ckks", "waitForPriorityViewKeychainDataRecovery errored: %@", replyError);
        } else {
            secnotice("octagn-ckks", "waitForPriorityViewKeychainDataRecovery succeeded");
        }

        localError = replyError;
    }];

    if(error && localError) {
        *error = localError;
    }

    return localError == nil;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

- (NSArray<NSString*>* _Nullable)tlkRecoverabilityForEscrowRecord:(OTEscrowRecord*)record error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon-tlk-recoverability", "tlkRecoverabiltyForEscrowRecord invoked for context:%@", self.ctx);
    __block NSError* localError = nil;

    OTControl *control = [self.ctx makeOTControl:&localError];
    if (!control) {
        secnotice("octagon-tlk-recoverability", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }

    __block NSArray<NSString *> * _Nullable views = nil;

    [control tlkRecoverabilityForEscrowRecordData:[[OTControlArguments alloc] initWithConfiguration:self.ctx]
                                       recordData:record.data
                                           source:self.ctx.escrowFetchSource
                                            reply:^(NSArray<NSString *> * _Nullable blockViews, NSError * _Nullable replyError) {
        if(replyError) {
            secnotice("octagon-tlk-recoverability", "tlkRecoverabilityForEscrowRecordData errored: %@", replyError);
        } else {
            secnotice("octagon-tlk-recoverability", "tlkRecoverabilityForEscrowRecordData succeeded");
        }
        views = blockViews;
        localError = replyError;
    }];

    if(error && localError) {
        *error = localError;
    }

    secnotice("octagon-tlk-recoverability", "views %@ supported for record %@", views, record);

    return views;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return nil;
#endif
}


- (BOOL)deliverAKDeviceListDelta:(NSDictionary*)notificationDictionary
                           error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon-authkit", "Delivering authkit payload for context:%@", self.ctx);
    __block NSError* localError = nil;

    OTControl *control = [self.ctx makeOTControl:&localError];
    if (!control) {
        secnotice("octagon-authkit", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }

    [control deliverAKDeviceListDelta:notificationDictionary
                                reply:^(NSError * _Nullable replyError) {
        if(replyError) {
            secnotice("octagon-authkit", "AKDeviceList change delivery errored: %@", replyError);
        } else {
            secnotice("octagon-authkit", "AKDeviceList change delivery succeeded");
        }
        localError = replyError;
    }];

    if(error && localError) {
        *error = localError;
    }

    return (localError == nil) ? YES : NO;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

+ (BOOL)registerRecoveryKeyWithContext:(OTConfigurationContext*)ctx recoveryKey:(NSString*)recoveryKey error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon-register-recovery-key", "registerRecoveryKeyWithContext invoked for context: %@", ctx.context);

    // set the recovery key for Octagon
    NSError* controlError = nil;
    OTControl* control = [ctx makeOTControl:&controlError];
    if(!control) {
        secnotice("octagon-register-recovery-key", "failed to make OTControl object: %@", controlError);
        if (error) {
            *error = controlError;
        }
        return NO;
    }
    
    __block NSError* createError = nil;
    [control createRecoveryKey:[[OTControlArguments alloc] initWithConfiguration:ctx] recoveryKey:recoveryKey reply:^(NSError * replyError) {
        if (replyError){
            secerror("octagon-register-recovery-key, failed to create octagon recovery key error: %@", replyError);
            createError = replyError;
        } else {
            secnotice("octagon-register-recovery-key", "successfully set octagon recovery key");
        }
    }];
    
    if (createError) {
        if (error) {
            *error = createError;
        }
        return NO;
    }
    
    // set the recovery key in SOS if the device has SOS enabled /and/ is in circle.
    CFErrorRef sosError = NULL;
    SOSCCStatus circleStatus = SOSCCThisDeviceIsInCircle(&sosError);
    if (sosError) {
        // we don't care about the sos error here. this function needs to succeed for Octagon.
        secerror("octagon-register-recovery-key, error checking SOS circle status: %@", sosError);
    }
    CFReleaseNull(sosError);
    
    if(SOSCCIsSOSTrustAndSyncingEnabled() && circleStatus == kSOSCCInCircle) {
        NSError* createRecoveryKeyError = nil;
        SecRecoveryKey *rk = SecRKCreateRecoveryKeyWithError(recoveryKey, &createRecoveryKeyError);
        if (!rk || createRecoveryKeyError) {
            secerror("octagon-register-recovery-key, SecRKCreateRecoveryKeyWithError() failed: %@", createRecoveryKeyError);
            __block NSError* localError = nil;
            [control removeRecoveryKey:[[OTControlArguments alloc] initWithConfiguration:ctx] reply:^(NSError * _Nullable removeError) {
                if (removeError) {
                    secerror("octagon-register-recovery-key, failed to remove recovery key from octagon error: %@", removeError);
                } else {
                    secnotice("octagon-register-recovery-key", "successfully removed octagon recovery key");
                }
                localError = removeError;
            }];
            
            NSMutableDictionary *userInfo = [NSMutableDictionary dictionary];
            userInfo[NSLocalizedDescriptionKey] = @"SecRKCreateRecoveryKeyWithError() failed";
            userInfo[NSUnderlyingErrorKey] = createRecoveryKeyError ?: localError;
            
            NSError* retError = nil;
            if ([OTClique isCloudServicesAvailable] == NO) {
                retError = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:userInfo];
            } else {
                retError = [NSError errorWithDomain:getkSecureBackupErrorDomain() code:kSecureBackupInternalError userInfo:userInfo];
            }
            if (error) {
                *error = retError;
            }
            return NO;
        }
        
        CFErrorRef copyError = NULL;
        SOSPeerInfoRef peer = SOSCCCopyMyPeerInfo(&copyError);
        if (peer) {
            CFDataRef backupKey = SOSPeerInfoCopyBackupKey(peer);
            if (backupKey == NULL) {
                CFErrorRef cferr = NULL;
                NSString *str = CFBridgingRelease(SecPasswordGenerate(kSecPasswordTypeiCloudRecovery, &cferr, NULL));
                if (str) {
                    NSData* secret = [str dataUsingEncoding:NSUTF8StringEncoding];
                    
                    CFErrorRef registerError = NULL;
                    SOSPeerInfoRef peerInfo = SOSCCCopyMyPeerWithNewDeviceRecoverySecret((__bridge CFDataRef)secret, &registerError);
                    if (peerInfo) {
                        secnotice("octagon-register-recovery-key", "registered backup key");
                    } else {
                        secerror("octagon-register-recovery-key, SOSCCCopyMyPeerWithNewDeviceRecoverySecret() failed: %@", registerError);
                    }
                    CFReleaseNull(registerError);
                    CFReleaseNull(peerInfo);
                } else {
                    secerror("octagon-register-recovery-key, SecPasswordGenerate() failed: %@", cferr);
                }
                CFReleaseNull(cferr);
            } else {
                secnotice("octagon-register-recovery-key", "backup key already registered");
            }
            CFReleaseNull(backupKey);
            CFReleaseNull(peer);
        } else {
            secerror("octagon-register-recovery-key, SOSCCCopyMyPeerInfo() failed: %@", copyError);
        }
        
        CFReleaseNull(copyError);
        
        CFErrorRef registerError = nil;
        if (!SecRKRegisterBackupPublicKey(rk, &registerError)) {
            secerror("octagon-register-recovery-key, SecRKRegisterBackupPublicKey() failed: %@", registerError);
            __block NSError* localError = nil;
            [control removeRecoveryKey:[[OTControlArguments alloc] initWithConfiguration:ctx] reply:^(NSError * _Nullable replyError) {
                if(replyError) {
                    secerror("octagon-register-recovery-key: removeRecoveryKey failed: %@", replyError);
                } else {
                    secnotice("octagon-register-recovery-key", "removeRecoveryKey succeeded");
                }
                localError = replyError;
            }];
            
            if (error) {
                if (registerError) {
                    *error = CFBridgingRelease(registerError);
                } else if (localError) {
                    *error = localError;
                } else {
                    *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:kSOSErrorFailedToRegisterBackupPublicKey description:@"Failed to register backup public key"];
                }
            } else {
                CFReleaseNull(registerError);
            }
            return NO;
        } else {
            secnotice("octagon-register-recovery-key", "successfully registered recovery key for SOS");
            
            id<OctagonEscrowRecovererPrococol> sb = ctx.sbd ?: [[getSecureBackupClass() alloc] init];
            NSError* enableError = [sb backupForRecoveryKeyWithInfo:nil];
            if (enableError) {
                secerror("octagon-register-recovery-key: failed to perform backup: %@", enableError);
                if (error) {
                    *error = enableError;
                }
                return nil;
            } else {
                secnotice("octagon-register-recovery-key", "created iCloud Identity backup");
            }
        }
    }
    
    return YES;
#else // !OCTAGON
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

+ (NSString * _Nullable)createAndSetRecoveryKeyWithContext:(OTConfigurationContext*)ctx error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon-create-recovery-key", "createAndSetRecoveryKeyWithContext invoked for context: %@", ctx.context);

    NSError* rkError = nil;
    NSString* recoveryKey = SecRKCreateRecoveryKeyString(&rkError);
    if (!recoveryKey || rkError) {
        secerror("octagon-create-recovery-key, failed to create recovery key error: %@", rkError);
        if (error) {
            *error = rkError;
        }
        return nil;
    }

    NSError* localError = nil;
    BOOL registerResult = [self registerRecoveryKeyWithContext:ctx recoveryKey:recoveryKey error:&localError];
    if (registerResult == NO || localError) {
        secerror("octagon-create-recovery-key, failed to register recovery key error: %@", localError);
        if (localError && error) {
            *error = localError;
        }
        return nil;
    }

    return recoveryKey;
#else // !OCTAGON
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return nil;
#endif
}

+ (BOOL)setRecoveryKeyWithContext:(OTConfigurationContext*)ctx
                      recoveryKey:(NSString*)recoveryKey
                            error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon-set-recovery-key", "setRecoveryKeyWithContext invoked for context: %@", ctx.context);

    NSError* controlError = nil;
    OTControl* control = [ctx makeOTControl:&controlError];
    if(!control) {
        secnotice("octagon-set-recovery-key", "failed to make OTControl object: %@", controlError);
        if (error) {
            *error = controlError;
        }
        return NO;
    }
    
    __block NSError* createError = nil;
    [control createRecoveryKey:[[OTControlArguments alloc] initWithConfiguration:ctx] recoveryKey:recoveryKey reply:^(NSError * replyError) {
        if (replyError){
            secerror("octagon-set-recovery-key, failed to set octagon recovery key error: %@", replyError);
            createError = replyError;
        } else {
            secnotice("octagon-set-recovery-key", "successfully set octagon recovery key");
        }
    }];
    
    if (createError) {
        if (error) {
            *error = createError;
        }
        return NO;
    }
    
    return YES;
#else // !OCTAGON
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

+ (BOOL)isRecoveryKeySetInOctagon:(OTConfigurationContext*)ctx error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon-is-recovery-key-set-in-octagon", "Checking Octagon recovery key status for context:%@", ctx);
 
    __block NSError* localError = nil;
    OTControl *control = [ctx makeOTControl:&localError];
    if (!control) {
        secnotice("octagon-is-recovery-key-set-in-octagon", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }

    __block BOOL isSetInOctagon = NO;
    __block NSError* octagonError = nil;
    [control isRecoveryKeySet:[[OTControlArguments alloc] initWithConfiguration:ctx] reply:^(BOOL replyIsSet, NSError * _Nullable replyError) {
        if(replyError) {
            secerror("octagon-is-recovery-key-set-in-octagon: isRecoveryKeySet failed: %@", replyError);
            octagonError = replyError;
        } else {
            secnotice("octagon-is-recovery-key-set-in-octagon", "isRecoveryKeySet: %{BOOL}d", replyIsSet);
            isSetInOctagon = replyIsSet;
        }
    }];

    if(error && octagonError) {
        *error = octagonError;
    }

    return isSetInOctagon;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

+ (BOOL)isRecoveryKeySetInSOS:(OTConfigurationContext*)ctx error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon-is-recovery-key-set-in-sos", "Checking SOS recovery key status for context:%@", ctx);
 
    bool isSetInSOS = false;
    id<OctagonEscrowRecovererPrococol> sb = ctx.sbd ?: [[getSecureBackupClass() alloc] init];
    
    NSError* setInSOSError = nil;
    isSetInSOS = [sb isRecoveryKeySet:&setInSOSError];
    if (setInSOSError) {
        secerror("octagon-is-recovery-key-set-in-sos: failed to check the recovery key in SOS: %@", setInSOSError);
        if (error) {
            *error = setInSOSError;
        }
    } else {
        secnotice("octagon-is-recovery-key-set-in-sos", "recovery key set in SOS: %{BOOL}d", isSetInSOS);
    }

    return isSetInSOS;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}
+ (BOOL)isRecoveryKeySet:(OTConfigurationContext*)ctx error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon-is-recovery-key-set", "Checking recovery key status for context:%@", ctx);
 
    NSError* octagonError = nil;
    BOOL isSetInOctagon = [OTClique isRecoveryKeySetInOctagon:ctx error:&octagonError];
    
    NSError* sosError = nil;
    bool isSetInSOS = [OTClique isRecoveryKeySetInSOS:ctx error:&sosError];
    
    if(error && !isSetInSOS && !isSetInOctagon) {
        if (octagonError) {
            *error = octagonError;
        } else if (sosError) {
            *error = sosError;
        }
    }

    return (isSetInOctagon || isSetInSOS);

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}


typedef NS_ENUM(NSInteger, RecoveryKeyInSOSState) {
    RecoveryKeyInSOSStateUnknown = 0,
    RecoveryKeyInSOSStateDoesNotExist = 1,
    RecoveryKeyInSOSStateExists = 2,
    RecoveryKeyInSOSStateExistsAndIsCorrect = 3
};

+ (RecoveryKeyInSOSState)doesRecoveryKeyExistInSOSAndIsCorrect:(OTConfigurationContext*)ctx
                                  recoveryKey:(NSString*)recoveryKey
                                        error:(NSError**)error
{
    NSError* sosError = nil;
    BOOL isRKSetInSOS = [OTClique isRecoveryKeySetInSOS:ctx error:&sosError];
    
    if (isRKSetInSOS == NO || sosError) {
        secerror("octagon-recover-with-rk: Recovery Key not registered in SOS: %@", sosError);
        if (error) {
            if (sosError) {
                *error = sosError;
            } else {
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNoRecoveryKeyRegistered description:@"Recovery key does not exist in Octagon"];
            }
        }
        return RecoveryKeyInSOSStateDoesNotExist;
    }

    secnotice("octagon-recover-with-rk", "recovery key is registered in SOS");
    id<OctagonEscrowRecovererPrococol> sb = ctx.sbd ?: [[getSecureBackupClass() alloc] init];

    BOOL isRKCorrectInSOS = [sb verifyRecoveryKey:recoveryKey error:&sosError] ? YES : NO;
    if (isRKCorrectInSOS == NO || sosError) {
        secnotice("octagon-recover-with-rk", "recovery key is NOT correct in SOS");
        if (error) {
            if (sosError) {
                *error = sosError;
            } else {
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecoveryKeyIncorrect description:@"Recovery key is incorrect"];
            }
        }
        return RecoveryKeyInSOSStateExists;
    }

    return RecoveryKeyInSOSStateExistsAndIsCorrect;
}

typedef NS_ENUM(NSInteger, RecoveryKeyInOctagonState) {
    RecoveryKeyInOctagonStateUnknown = 0,
    RecoveryKeyInOctagonStateDoesNotExist = 1,
    RecoveryKeyInOctagonStateExists = 2,
    RecoveryKeyInOctagonStateExistsAndIsCorrect = 3
};

+ (RecoveryKeyInOctagonState)doesRecoveryKeyExistInOctagonAndIsCorrect:(OTConfigurationContext*)ctx
                                                           recoveryKey:(NSString*)recoveryKey
                                                                 error:(NSError**)error
{
    __block NSError* localError = nil;
    OTControl *control = [ctx makeOTControl:&localError];
    if (!control) {
        secerror("octagon-recover-with-rk: unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }

    __block NSError* octagonError = nil;
    BOOL isRKSetInOctagon = [OTClique isRecoveryKeySetInOctagon:ctx error:&octagonError];

    if (isRKSetInOctagon == NO || octagonError) {
        secerror("octagon-recover-with-rk: recovery key not registered in Octagon, error: %@", octagonError);
        if (error) {
            if (octagonError) {
                *error = octagonError;
            } else {
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNoRecoveryKeyRegistered description:@"Recovery key does not exist in Octagon"];
            }
        }
        return RecoveryKeyInOctagonStateDoesNotExist;
    }

    secnotice("octagon-recover-with-rk", "recovery key is registered in Octagon, checking if it is correct");

    __block BOOL isRKCorrectInOctagon = NO;
    [control preflightRecoverOctagonUsingRecoveryKey:[[OTControlArguments alloc] initWithConfiguration:ctx] recoveryKey:recoveryKey reply:^(BOOL correct, NSError * _Nullable replyError) {
        if (replyError) {
            secnotice("octagon-recover-with-rk", "Preflight recovery key errored: %@", replyError);
            octagonError = replyError;
        } else {
            secnotice("octagon-recover-with-rk", "Recovery key is %@", correct ? @"correct" : @"incorrect");
            isRKCorrectInOctagon = correct;
        }
    }];

    if (isRKCorrectInOctagon == NO || octagonError) {
        if (error) {
            if (octagonError) {
                *error = octagonError;
            } else {
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecoveryKeyIncorrect description:@"Recovery key is incorrect"];
            }
        }
        return RecoveryKeyInOctagonStateExists;
    }

    return RecoveryKeyInOctagonStateExistsAndIsCorrect;
}

+ (BOOL)recoverWithRecoveryKey:(OTConfigurationContext*)ctx
                   recoveryKey:(NSString*)recoveryKey
                         error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon-recover-with-rk", "Recovering account trust using recovery key for context:%@", ctx);
    
    CFErrorRef validateError = NULL;
    bool res = SecPasswordValidatePasswordFormat(kSecPasswordTypeiCloudRecoveryKey, (__bridge CFStringRef)recoveryKey, &validateError);
    if (!res) {
        NSError *validateErrorWrapper = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecoveryKeyMalformed description:@"Malformed Recovery Key" underlying:CFBridgingRelease(validateError)];

        secerror("octagon-recover-with-rk: recovery failed validation with error:%@", validateErrorWrapper);
        if (error) {
            *error = validateErrorWrapper;
        }
        return NO;
    }

    NSError* sosError = nil;
    RecoveryKeyInSOSState sosState = [OTClique doesRecoveryKeyExistInSOSAndIsCorrect:ctx recoveryKey:recoveryKey error:&sosError];

    NSError* octagonError = nil;
    RecoveryKeyInOctagonState octagonState = [OTClique doesRecoveryKeyExistInOctagonAndIsCorrect:ctx recoveryKey:recoveryKey error:&octagonError];

    if (sosState != RecoveryKeyInSOSStateExistsAndIsCorrect &&
        octagonState != RecoveryKeyInOctagonStateExistsAndIsCorrect) {
        secerror("octagon-recover-with-rk: recovery key will not work for both SOS and Octagon");
        if (error) {
            if (sosState == RecoveryKeyInSOSStateDoesNotExist && octagonState == RecoveryKeyInOctagonStateDoesNotExist) {
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorNoRecoveryKeyRegistered description:@"Recovery key is not registered"];
            } else if ((sosState != RecoveryKeyInSOSStateExistsAndIsCorrect && octagonState == RecoveryKeyInOctagonStateExists) ||
                       (sosState == RecoveryKeyInSOSStateExists && octagonState != RecoveryKeyInOctagonStateExistsAndIsCorrect)) {
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecoveryKeyIncorrect description:@"Recovery key is not correct"];
            } else if (octagonError) {
                *error = octagonError;
            } else if (SOSCCIsSOSTrustAndSyncingEnabled() && sosError) {
                *error = sosError;
            } else {
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecoveryKeyIncorrect description:@"Recovery key is not correct"];
            }
        }
        return NO;
    }

    if (sosState == RecoveryKeyInSOSStateExistsAndIsCorrect && SOSCCIsSOSTrustAndSyncingEnabled()) {
        NSData *keydata = [recoveryKey dataUsingEncoding:NSUTF8StringEncoding];
        if (!keydata) {
            if (error) {
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecoveryKeyMalformed description:@"Malformed recovery key"];
            }
            return NO;
        }

        id<OctagonEscrowRecovererPrococol> sb = ctx.sbd ?: [[getSecureBackupClass() alloc] init];
        NSError* restoreError = nil;
        if ([sb restoreKeychainWithBackupPassword:keydata error:&restoreError]) {
            secnotice("octagon-recover-with-rk","restoreKeychainWithBackupPassword succeeded");
        } else {
            secerror("octagon-recover-with-rk: restoreKeychainWithBackupPassword returned error: %@", restoreError);
            if (octagonState != RecoveryKeyInOctagonStateExistsAndIsCorrect) {
                if (error) {
                    if (restoreError) {
                        *error = restoreError;
                    } else {
                        *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorSecureBackupRestoreUsingRecoveryKeyFailed description:@"Restore Keychain With Backup Password Failed"];
                    }
                }
                return NO;
            }
        }
    }

    // if there is no RK set in Octagon, but there is in SOS and the device can't do SOS things and there exists Octagon viable escrow records,
    // we should return an error and force the escrow restore path
    if (octagonState == RecoveryKeyInOctagonStateDoesNotExist &&
        sosState == RecoveryKeyInSOSStateExistsAndIsCorrect &&
        SOSCCIsSOSTrustAndSyncingEnabled() == NO && ctx.octagonCapableRecordsExist) {
        secnotice("octagon-recover-with-rk", "Recovery key exists in SOS but not in Octagon and this platform does not support SOS.  Octagon records exist, forcing iCSC restore");
        if (error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecoverWithRecoveryKeyNotSupported description:@"recover with recovery key configuration not supported, forcing iCSC restore"];
        }
        return NO;
    }

    __block NSError* localError = nil;
    OTControl *control = [ctx makeOTControl:&localError];
    if (!control) {
        secerror("octagon-recover-with-rk: unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }
    
    // if Recovery Key is not set in Octagon but is set in SOS, reset octagon and set the recovery key
    if (octagonState == RecoveryKeyInOctagonStateDoesNotExist && sosState == RecoveryKeyInSOSStateExistsAndIsCorrect) {

        [control resetAndEstablish:[[OTControlArguments alloc] initWithConfiguration:ctx]
                       resetReason:CuttlefishResetReasonRecoveryKey
                 idmsTargetContext:nil
            idmsCuttlefishPassword:nil
                        notifyIdMS:false
                   accountSettings:nil
                             reply:^(NSError * _Nullable resetError) {
            if(resetError) {
                secnotice("octagon-recover-with-rk", "reset and establish returned an error: %@", resetError);
                localError = resetError;
            } else {
                secnotice("octagon-recover-with-rk", "successfully reset octagon, attempting enrolling recovery key");
                [control createRecoveryKey:[[OTControlArguments alloc] initWithConfiguration:ctx] recoveryKey:recoveryKey reply:^(NSError* enrollError) {
                    if (enrollError){
                        secerror("octagon-recover-with-rk, failed to enroll new recovery key: %@", enrollError);
                        localError = enrollError;
                    } else {
                        secnotice("octagon-recover-with-rk", "successfully enrolled recovery key");
                        
                        if (SOSCCIsSOSTrustAndSyncingEnabled()) {
                            bool joinResult = true;
                            if (!ctx.overrideForJoinAfterRestore) {
                                CFErrorRef restoreError = NULL;
                                joinResult = SOSCCRequestToJoinCircleAfterRestore(&restoreError);
                                secnotice("octagon-recover-with-rk", "Join circle after restore: %d, error: %@", joinResult, restoreError);
                                CFReleaseNull(restoreError);
                            } else {
                                secnotice("octagon-recover-with-rk", "skipping SOSCCRequestToJoinCircleAfterRestore attempt for tests");
                            }
                            
                            secnotice("octagon-recover-with-rk", "joinAfterRestore complete: %@, error: %@", joinResult ? @"success" : @"failure", localError);
                        }
                    }
                }];
            }
        }];
    } else if (octagonState == RecoveryKeyInOctagonStateExistsAndIsCorrect){
        [control recoverWithRecoveryKey:[[OTControlArguments alloc] initWithConfiguration:ctx] recoveryKey:recoveryKey reply:^(NSError * _Nullable replyError) {
            if(replyError) {
                secerror("octagon-recover-with-rk: joining with recovery key failed: %@", replyError);
                localError = replyError;
            } else {
                secnotice("octagon-recover-with-rk", "joining with recovery key succeeded");
            }
        }];
    } else {
        secerror("octagon-recover-with-rk: joining with recovery key failed, recovery key is not correct in Octagon");
        if (error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecoveryKeyIncorrect description:@"Recovery key is not correct"];
        }
        return NO;
    }

    if (localError) {
        if (error) {
            *error = localError;
        }
    }

    return (localError) == nil ? YES : NO;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

- (void)removeRecoveryKeyFromSOSWhenInCircle:(OTConfigurationContext*)ctx
                                       error:(NSError**)error
{
    secnotice("octagon-remove-recovery-key", "Removing recovery key when device is in circle");

    NSError* setInSOSError = nil;
    BOOL rkInSOS = [OTClique isRecoveryKeySetInSOS:ctx error:&setInSOSError];
    if (rkInSOS == NO || setInSOSError) {
        secerror("octagon-register-recovery-key, recovery key not registered in SOS: %@", setInSOSError);
        if (error) {
            *error = setInSOSError;
        }
        return;
    }

    CFErrorRef copyError = NULL;
    SOSPeerInfoRef peer = SOSCCCopyMyPeerInfo(&copyError);

    if (peer == NULL || copyError) {
        secerror("octagon-register-recovery-key, SOSCCCopyMyPeerInfo() failed: %@", copyError);
        if (error) {
            *error = CFBridgingRelease(copyError);
        } else {
            CFReleaseNull(copyError);
        }
        CFReleaseNull(peer);
        return;
    }

    CFDataRef backupKey = SOSPeerInfoCopyBackupKey(peer);
    CFReleaseNull(peer);

    if (backupKey == NULL) {
        CFErrorRef cferr = NULL;
        NSString *str = CFBridgingRelease(SecPasswordGenerate(kSecPasswordTypeiCloudRecovery, &cferr, NULL));
        if (str == nil || cferr) {
            secerror("octagon-register-recovery-key, SecPasswordGenerate() failed: %@", cferr);
            if (error) {
                *error = CFBridgingRelease(cferr);
            } else {
                CFReleaseNull(cferr);
            }
            return;
        }

        NSData* secret = [str dataUsingEncoding:NSUTF8StringEncoding];

        CFErrorRef registerError = NULL;
        SOSPeerInfoRef peerInfo = SOSCCCopyMyPeerWithNewDeviceRecoverySecret((__bridge CFDataRef)secret, &registerError);
        if (peerInfo) {
            secnotice("octagon-register-recovery-key", "registered backup key");
            CFReleaseNull(peerInfo);
        } else {
            secerror("octagon-register-recovery-key, SOSCCCopyMyPeerWithNewDeviceRecoverySecret() failed: %@", registerError);
            if (error) {
                *error = CFBridgingRelease(registerError);
            } else {
                CFReleaseNull(registerError);
            }
            return;
        }
    } else {
        secnotice("octagon-register-recovery-key", "backup key already registered");
        CFReleaseNull(backupKey);
    }

    CFErrorRef sosError = NULL;
    if (!SOSCCRegisterRecoveryPublicKey(NULL, &sosError)) {
        secerror("octagon-remove-recovery-key: failed to remove recovery key from SOS: %@", sosError);
        if (error) {
            *error = CFBridgingRelease(sosError);
        } else {
            CFReleaseNull(sosError);
        }
        return;
    } else {
        id<OctagonEscrowRecovererPrococol> sb = ctx.sbd ?: [[getSecureBackupClass() alloc] init];
        NSError* enableError = [sb backupForRecoveryKeyWithInfo:nil];
        if (enableError) {
            secerror("octagon-remove-recovery-key: failed to perform backup: %@", enableError);
            if (error) {
                *error = enableError;
            }
            return;
        } else {
            secnotice("octagon-remove-recovery-key", "Removed recovery key from SOS");
        }
    }
}

- (void)removeRecoveryKeyFromSOSWhenNOTInCircle:(OTConfigurationContext*)ctx
                                       error:(NSError**)error
{
    secnotice("octagon-remove-recovery-key", "Removing recovery key when not in circle");
    CFErrorRef pushError = NULL;
    bool pushResult = SOSCCPushResetCircle(&pushError);
    if (!pushResult || pushError) {
        secerror("octagon-remove-recovery-key: failed to push: %@", pushError);
        if (error) {
            *error = CFBridgingRelease(pushError);
        } else {
            CFReleaseNull(pushError);
        }
        return;
    } else {
        secnotice("octagon-remove-recovery-key", "successfully pushed a reset circle");
    }

    id<OctagonEscrowRecovererPrococol> sb = ctx.sbd ?: [[getSecureBackupClass() alloc] init];
    NSError* removeError = nil;
    bool removeResult = [sb removeRecoveryKeyFromBackup:&removeError];
    if (removeResult == false || removeError) {
        secerror("octagon-remove-recovery-key: failed to remove recovery key from the backup: %@", removeError);
        if (error) {
            *error = removeError;
        }
        return;
    } else {
        secnotice("octagon-remove-recovery-key", "removed recovery key from the backup");
    }
}

- (BOOL)removeRecoveryKey:(OTConfigurationContext*)ctx
                    error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon-remove-recovery-key", "Removing recovery key for context:%@", ctx);

    CFErrorRef sosError = NULL;
    SOSCCStatus circleStatus = SOSCCThisDeviceIsInCircle(&sosError);
    if (sosError) {
        // we don't care about the sos error here. this function needs to succeed for Octagon.
        secerror("octagon-remove-recovery-key, error checking SOS circle status: %@", sosError);
    }
    CFReleaseNull(sosError);

    NSError* removeError = nil;
    if(SOSCCIsSOSTrustAndSyncingEnabled() && circleStatus == kSOSCCInCircle) {
        [self removeRecoveryKeyFromSOSWhenInCircle:ctx error:&removeError];
    } else {
        [self removeRecoveryKeyFromSOSWhenNOTInCircle:ctx error:&removeError];
    }
    if (removeError) {
        secerror("octagon-remove-recovery-key, error removing recovery key from SOS: %@", removeError);
    } else {
        secnotice("octagon-remove-recovery-key", "Removed recovery key from SOS");
    }

    __block NSError* localError = nil;
    OTControl *control = [ctx makeOTControl:&localError];
    if (!control) {
        secnotice("octagon-remove-recovery-key", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }
    
    [control removeRecoveryKey:[[OTControlArguments alloc] initWithConfiguration:ctx] reply:^(NSError * _Nullable replyError) {
        if(replyError) {
            secerror("octagon-remove-recovery-key: removeRecoveryKey failed: %@", replyError);
        } else {
            secnotice("octagon-remove-recovery-key", "removeRecoveryKey succeeded");
        }
        localError = replyError;
    }];
    
    if(error && localError) {
        *error = localError;
    }
    
    return (localError) == nil ? YES : NO;
    
#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}


+ (BOOL)preflightRecoverOctagonUsingRecoveryKey:(OTConfigurationContext*)ctx
                                    recoveryKey:(NSString*)recoveryKey
                                          error:(NSError**)error
{
#if OCTAGON
    secnotice("octagon-preflight-recovery-key", "Preflight using recovery key for context: %@", ctx);
    __block NSError* localError = nil;
    __block BOOL isRecoveryKeyCorrect = NO;
    
    
    CFErrorRef validateError = NULL;
    bool result = SecPasswordValidatePasswordFormat(kSecPasswordTypeiCloudRecoveryKey, (__bridge CFStringRef)recoveryKey, &validateError);
    if (!result) {
        NSError *validateErrorWrapper = [NSError errorWithDomain:OctagonErrorDomain code:OctagonErrorRecoveryKeyMalformed description:@"malformed recovery key"];
        secerror("octagon-preflight-recovery-key: recovery failed validation with error:%@", validateErrorWrapper);
        if (error) {
            *error = validateErrorWrapper;
        }
        return NO;
    }
    
    
    OTControl *control = [ctx makeOTControl:&localError];
    if (!control) {
        secnotice("octagon-preflight-recovery-key", "unable to create otcontrol: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }

    [control preflightRecoverOctagonUsingRecoveryKey:[[OTControlArguments alloc] initWithConfiguration:ctx] recoveryKey:recoveryKey reply:^(BOOL correct, NSError * _Nullable replyError) {
        if(replyError) {
            secnotice("octagon-preflight-recovery-key", "Preflight recovery key errored: %@", replyError);
        } else {
            secnotice("octagon-preflight-recovery-key", "Recovery key is %@", correct ? @"correct" : @"incorrect");
            isRecoveryKeyCorrect = correct;
        }
        localError = replyError;
    }];

    if(error && localError) {
        *error = localError;
    }

    return isRecoveryKeyCorrect;

#else
    if (error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    }
    return NO;
#endif
}

+ (NSNumber * _Nullable)totalTrustedPeers:(OTConfigurationContext*)ctx error:(NSError * __autoreleasing *)error
{
#if OCTAGON
    secnotice("octagon-count-trusted-peers", "totalTrustedPeers invoked for context: %@", ctx.context);

    NSError* controlError = nil;
    OTControl* control = [ctx makeOTControl:&controlError];
    if(!control) {
        secnotice("octagon-count-trusted-peers", "failed to fetch OTControl object: %@", controlError);
        if (error) {
            *error = controlError;
        }
        return nil;
    }

    __block NSError* localError = nil;
    __block NSNumber* totalTrustedPeers = nil;

    [control totalTrustedPeers:[[OTControlArguments alloc] initWithConfiguration:ctx] reply:^(NSNumber * _Nullable count, NSError * _Nullable countError) {
        if(countError) {
            secnotice("octagon-count-trusted-peers", "totalTrustedPeers errored: %@", countError);
            localError = countError;
        } else {
            secnotice("octagon-count-trusted-peers", "totalTrustedPeers succeeded, total count: %@", count);
            totalTrustedPeers = count;
        }
    }];

    if (localError) {
        if (error) {
            *error = localError;
        }
        return nil;
    }

    secnotice("octagon-count-trusted-peers", "Number of trusted Octagon peers: %@", totalTrustedPeers);

    return totalTrustedPeers;
#else // !OCTAGON
    return NULL;
#endif
}


+ (BOOL)areRecoveryKeysDistrusted:(OTConfigurationContext*)ctx error:(NSError * __autoreleasing *)error
{
#if OCTAGON
    secnotice("octagon-contain-distrusted-recovery-keys", "areRecoveryKeysDistrusted invoked for context: %@", ctx.context);

    NSError* controlError = nil;
    OTControl* control = [ctx makeOTControl:&controlError];
    if(!control) {
        secnotice("octagon-contain-distrusted-recovery-keys", "failed to fetch OTControl object: %@", controlError);
        if (error) {
            *error = controlError;
        }
        return NO;
    }

    __block NSError* localError = nil;
    __block BOOL octagonContainsDistrustedRecoveryKeys = NO;

    [control areRecoveryKeysDistrusted:[[OTControlArguments alloc] initWithConfiguration:ctx] reply:^(BOOL containsDistrusted, NSError * _Nullable rkError) {
        if(rkError) {
            secnotice("octagon-contain-distrusted-recovery-keys", "areRecoveryKeysDistrusted errored: %@", rkError);
            localError = rkError;
        } else {
            secnotice("octagon-contain-distrusted-recovery-keys", "areRecoveryKeysDistrusted succeeded, octagon circle contains distrusted recovery keys: %@", containsDistrusted ? @"YES" : @"NO");
            octagonContainsDistrustedRecoveryKeys = containsDistrusted;
        }
    }];

    if (localError) {
        if (error) {
            *error = localError;
        }
        return NO;
    }

    secnotice("octagon-contain-distrusted-recovery-keys", "Octagon circle %@ distrusted recovery keys", octagonContainsDistrustedRecoveryKeys ? @"contains" : @"does not contain");

    return octagonContainsDistrustedRecoveryKeys;
#else // !OCTAGON
    return NO;
#endif
}

@end

#endif
