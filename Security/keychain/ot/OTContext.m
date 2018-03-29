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

#import "OTContext.h"
#import "SFPublicKey+SPKI.h"

#include <utilities/SecFileLocations.h>
#include <Security/SecRandomP.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSAnalytics.h"

#import "CoreCDP/CDPFollowUpController.h"
#import "CoreCDP/CDPFollowUpContext.h"
#import <CoreCDP/CDPAccount.h>

NSString* OTCKContainerName = @"com.apple.security.keychain";
NSString* OTCKZoneName = @"OctagonTrust";
static NSString* const kOTRampZoneName = @"metadata_zone";

@interface OTContext (lockstateTracker) <CKKSLockStateNotification>
@end

@interface OTContext ()

@property (nonatomic, strong) NSString* contextID;
@property (nonatomic, strong) NSString* contextName;
@property (nonatomic, strong) NSString* dsid;

@property (nonatomic, strong) OTLocalStore* localStore;
@property (nonatomic, strong) OTCloudStore* cloudStore;
@property (nonatomic, strong) NSData* changeToken;
@property (nonatomic, strong) NSString* egoPeerID;
@property (nonatomic, strong) NSDate*   egoPeerCreationDate;
@property (nonatomic, strong) dispatch_queue_t queue;
@property (nonatomic, weak) id <OTContextIdentityProvider> identityProvider;

@property (nonatomic, strong) CKKSCKAccountStateTracker* accountTracker;
@property (nonatomic, strong) CKKSLockStateTracker* lockStateTracker;
@property (nonatomic, strong) CKKSReachabilityTracker *reachabilityTracker;

@end

@implementation OTContext

-(CKContainer*)makeCKContainer:(NSString*)containerName {
    CKContainer* container = [CKContainer containerWithIdentifier:containerName];
    container = [[CKContainer alloc] initWithContainerID: container.containerID];
    return container;
}

-(BOOL) isPrequeliteEnabled
{
    BOOL result = YES;
    if([PQLConnection class] == nil) {
        secerror("OT: prequelite appears to not be linked. Can't create OT objects.");
        result = NO;
    }
    return result;
}

- (nullable instancetype) initWithContextID:(NSString*)contextID
                                       dsid:(NSString*)dsid
                                 localStore:(OTLocalStore*)localStore
                                 cloudStore:(nullable OTCloudStore*)cloudStore
                           identityProvider:(id <OTContextIdentityProvider>)identityProvider
                                      error:(NSError**)error
{
    if(![self isPrequeliteEnabled]){
        // We're running in the base build environment, which lacks a bunch of libraries.
        // We don't support doing anything in this environment. Bye.
        return nil;
    }

    self = [super init];
    if (self) {
        NSError* localError = nil;
        _contextID = contextID;
        _dsid = dsid;
        _identityProvider = identityProvider;
        _localStore = localStore;
        
        NSString* contextAndDSID = [NSString stringWithFormat:@"%@-%@", contextID, dsid];

        CKContainer* container = [self makeCKContainer:OTCKContainerName];

        _accountTracker = [CKKSViewManager manager].accountTracker;
        _lockStateTracker = [CKKSViewManager manager].lockStateTracker;
        _reachabilityTracker = [CKKSViewManager manager].reachabilityTracker;

        if(!cloudStore) {
            _cloudStore = [[OTCloudStore alloc]initWithContainer:container
                                                        zoneName:OTCKZoneName
                                                  accountTracker:_accountTracker
                                             reachabilityTracker:_reachabilityTracker
                                                      localStore:_localStore
                                                       contextID:contextID
                                                            dsid:dsid
                            fetchRecordZoneChangesOperationClass:[CKFetchRecordZoneChangesOperation class]
                                      fetchRecordsOperationClass:[CKFetchRecordsOperation class]
                                             queryOperationClass:[CKQueryOperation class]
                               modifySubscriptionsOperationClass:[CKModifySubscriptionsOperation class]
                                 modifyRecordZonesOperationClass:[CKModifyRecordZonesOperation class]
                                              apsConnectionClass:[APSConnection class]
                                                  operationQueue:nil];
        } else{
            _cloudStore = cloudStore;
        }

        OTContextRecord* localContextRecord = [_localStore readLocalContextRecordForContextIDAndDSID:contextAndDSID error:&localError];

        if(localContextRecord == nil || localContextRecord.contextID == nil){
            localError = nil;
            BOOL result = [_localStore initializeContextTable:contextID dsid:dsid error:&localError];
            if(!result || localError != nil){
                secerror("octagon: reading from database failed with error: %@", localError);
                if (error) {
                    *error = localError;
                }
                return nil;
            }
            localContextRecord = [_localStore readLocalContextRecordForContextIDAndDSID:contextAndDSID error:&localError];
            if(localContextRecord == nil || localError !=nil){
                secerror("octagon: reading from database failed with error: %@", localError);
                if (error) {
                    *error = localError;
                }
                return nil;
            }
        }
        
        _contextID = localContextRecord.contextID;
        _contextName = localContextRecord.contextName;
        _changeToken = localContextRecord.changeToken;
        _egoPeerID = localContextRecord.egoPeerID;
        _egoPeerCreationDate = localContextRecord.egoPeerCreationDate;
        
        _queue = dispatch_queue_create("com.apple.security.otcontext", DISPATCH_QUEUE_SERIAL);
    }
    return self;
}

- (nullable OTBottledPeerSigned *) createBottledPeerRecordForIdentity:(OTIdentity *)identity
                                                               secret:(NSData*)secret
                                                                error:(NSError**)error
{
    NSError* localError = nil;
    if(self.lockStateTracker.isLocked){
        secnotice("octagon", "device is locked");
        if(error){
            *error = [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain code:errSecInteractionNotAllowed userInfo:nil];
        }
        return nil;
    }

    OTEscrowKeys *escrowKeys = [[OTEscrowKeys alloc] initWithSecret:secret dsid:self.dsid error:&localError];
    if (!escrowKeys || localError != nil) {
        secerror("octagon: unable to derive escrow keys: %@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }
    
    OTBottledPeer *bp = [[OTBottledPeer alloc] initWithPeerID:identity.peerID
                                                         spID:identity.spID
                                               peerSigningKey:identity.peerSigningKey
                                            peerEncryptionKey:identity.peerEncryptionKey
                                                   escrowKeys:escrowKeys
                                                        error:&localError];
    if (!bp || localError !=nil) {
        secerror("octagon: unable to create a bottled peer: %@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }
    return [[OTBottledPeerSigned alloc] initWithBottledPeer:bp
                                         escrowedSigningKey:escrowKeys.signingKey
                                             peerSigningKey:identity.peerSigningKey
                                                      error:error];
}

- (NSData* _Nullable) makeMeSomeEntropy:(int)requiredLength
{
    NSMutableData* salt = [NSMutableData dataWithLength:requiredLength];
    if (salt == nil){
        return nil;
    }
    if (SecRandomCopyBytes(kSecRandomDefault, [salt length], [salt mutableBytes]) != 0){
        return nil;
    }
    return salt;
}

- (nullable OTPreflightInfo*) preflightBottledPeer:(NSString*)contextID
                                           entropy:(NSData*)entropy
                                             error:(NSError**)error
{
    NSError* localError = nil;
    if(self.lockStateTracker.isLocked){
        secnotice("octagon", "device is locked");
        if(error){
            *error = [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain code:errSecInteractionNotAllowed userInfo:nil];
        }
        return nil;
    }

    OTIdentity *identity = [self.identityProvider currentIdentity:&localError];
    if (!identity || localError != nil) {
        secerror("octagon: unable to get current identity:%@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }

    OTBottledPeerSigned* bps = [self createBottledPeerRecordForIdentity:identity
                                                                 secret:entropy
                                                                  error:&localError];
    if (!bps || localError != nil) {
        secerror("octagon: failed to create bottled peer record: %@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }
    secnotice("octagon", "created bottled peer:%@", bps);
    
    OTBottledPeerRecord *bprec = [bps asRecord:identity.spID];
    
    if (!identity.spID) {
        secerror("octagon: cannot enroll without a spID");
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorNoIdentity userInfo:@{NSLocalizedDescriptionKey: @"OTIdentity does not have an SOS peer id"}];
        }
        return nil;
    }

    OTPreflightInfo* info = [[OTPreflightInfo alloc]init];
    info.escrowedSigningSPKI = bprec.escrowedSigningSPKI;

    if(!info.escrowedSigningSPKI){
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorEscrowSigningSPKI userInfo:@{NSLocalizedDescriptionKey: @"Escrowed spinging SPKI is nil"}];
        }
        secerror("octagon: Escrowed spinging SPKI is nil");
        return nil;
    }

    info.bottleID = bprec.recordName;
    if(!info.bottleID){
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorBottleID userInfo:@{NSLocalizedDescriptionKey: @"BottleID is nil"}];
        }
        secerror("octagon: BottleID is nil");
        return nil;
    }

    //store record in localStore
    BOOL result = [self.localStore insertBottledPeerRecord:bprec escrowRecordID:identity.spID error:&localError];
    if(!result || localError){
        secerror("octagon: could not persist the bottle record: %@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }

    return info;
}

- (BOOL)scrubBottledPeer:(NSString*)contextID
                bottleID:(NSString*)bottleID
                   error:(NSError**)error
{
    secnotice("octagon", "scrubBottledPeer");
    NSError* localError = nil;
    if(self.lockStateTracker.isLocked){
        secnotice("octagon", "device is locked");
        if(error){
            *error = [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain code:errSecInteractionNotAllowed userInfo:nil];
        }
        return YES;
    }

    BOOL result = [self.localStore deleteBottledPeer:bottleID error:&localError];
    if(!result || localError != nil){
        secerror("octagon: could not remove record for bottleID %@, error:%@", bottleID, localError);
        if (error) {
            *error = localError;
        }
    }
    return result;
}

- (OTBottledPeerSigned *) restoreFromEscrowRecordID:(NSString*)escrowRecordID
                                             secret:(NSData*)secret
                                              error:(NSError**)error
{
    NSError *localError = nil;

    if(self.lockStateTracker.isLocked){
        if(error){
            *error = [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain code:errSecInteractionNotAllowed userInfo:nil];
        }
        return nil;
    }

    OTEscrowKeys *escrowKeys = [[OTEscrowKeys alloc] initWithSecret:secret dsid:self.dsid error:&localError];
    if (!escrowKeys || localError != nil) {
        secerror("unable to derive escrow keys: %@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }

    BOOL result = [self.cloudStore downloadBottledPeerRecord:&localError];
    if(!result || localError){
        secerror("octagon: could not download bottled peer record:%@", localError);
        if(error){
            *error = localError;
        }
    }
    NSString* recordName = [OTBottledPeerRecord constructRecordID:escrowRecordID
                                                escrowSigningSPKI:[escrowKeys.signingKey.publicKey asSPKI]];
    OTBottledPeerRecord* rec = [self.localStore readLocalBottledPeerRecordWithRecordID:recordName error:&localError];

    if (!rec) {
        secerror("octagon: could not read bottled peer record:%@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }

    OTBottledPeerSigned *bps = [[OTBottledPeerSigned alloc] initWithBottledPeerRecord:rec
                                                                           escrowKeys:escrowKeys
                                                                                error:&localError];
    if (!bps) {
        secerror("octagon: could not unpack bottled peer:%@", localError);
        if (error) {
            *error = localError;
        }
        return nil;
    }

    return bps;
}

-(BOOL)bottleExistsLocallyForIdentity:(OTIdentity*)identity logger:(CKKSAnalytics*)logger error:(NSError**)error
{
    NSError* localError = nil;
    //read all the local bp records
    NSArray<OTBottledPeerRecord*>* bottles = [self.localStore readLocalBottledPeerRecordsWithMatchingPeerID:identity.spID error:&localError];
    if(!bottles || [bottles count] == 0 || localError != nil){
        secerror("octagon: there are no eligible bottle peer records: %@", localError);
        [logger logRecoverableError:localError
                           forEvent:OctagonEventBottleCheck
                           zoneName:kOTRampZoneName
                     withAttributes:NULL];
        if(error){
            *error = localError;
        }
        return NO;
    }

    BOOL hasBottle = NO;
    //if check all the records if the peer signing public key matches the bottled one!
    for(OTBottledPeerRecord* bottle in bottles){
        NSData* bottledSigningSPKIData = [[SFECPublicKey fromSPKI:bottle.peerSigningSPKI] keyData];
        NSData* currentIdentitySPKIData = [identity.peerSigningKey.publicKey keyData];

        //spIDs are the same AND check bottle signature
        if([currentIdentitySPKIData isEqualToData:bottledSigningSPKIData] &&
           [OTBottledPeerSigned verifyBottleSignature:bottle.bottle
                                            signature:bottle.signatureUsingPeerKey
                                                  key:identity.peerSigningKey.publicKey
                                                error:error]){
            hasBottle = YES;
        }
    }



    return hasBottle;
}

-(BOOL)queryCloudKitForBottle:(OTIdentity*)identity logger:(CKKSAnalytics*)logger error:(NSError**)error
{
    NSError* localError = nil;
    BOOL hasBottle = NO;
    //attempt to pull down all the records, but continue checking local store even if this fails.
    BOOL fetched = [self.cloudStore downloadBottledPeerRecord:&localError];
    if(fetched == NO || localError != nil){ //couldn't download bottles
        secerror("octagon: 0 bottled peers downloaded: %@", localError);
        [logger logRecoverableError:localError
                           forEvent:OctagonEventBottleCheck
                           zoneName:kOTRampZoneName
                     withAttributes:NULL];
        if(error){
            *error = localError;
        }
        return NO;
    }else{ //downloaded bottles, let's check local store
        hasBottle = [self bottleExistsLocallyForIdentity:identity logger:logger error:&localError];
    }

    if(error){
        *error = localError;
    }
    return hasBottle;
}

-(OctagonBottleCheckState) doesThisDeviceHaveABottle:(NSError**)error
{
    secnotice("octagon", "checking if device has enrolled a bottle");

    if(self.lockStateTracker.isLocked){
        secnotice("octagon", "device locked, not checking for bottle");
        if(error){
            *error = [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain code:errSecInteractionNotAllowed userInfo:nil];
        }
        return UNCLEAR;
    }

    if(self.accountTracker.currentCKAccountInfo.accountStatus != CKAccountStatusAvailable){
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain
                                         code:OTErrorNotSignedIn
                                     userInfo:@{NSLocalizedDescriptionKey: @"iCloud account is logged out"}];
        }
        secnotice("octagon", "not logged into an account");
        return UNCLEAR;
    }

    NSError* localError = nil;
    OctagonBottleCheckState bottleStatus = NOBOTTLE;
    CKKSAnalytics* logger = [CKKSAnalytics logger];
    SFAnalyticsActivityTracker *tracker = [logger logSystemMetricsForActivityNamed:CKKSActivityBottleCheck withAction:nil];
    [tracker start];

    //get our current identity
    OTIdentity* identity = [self.identityProvider currentIdentity:&localError];

    //if we get the locked error, return true so we don't prompt the user
    if(localError && [_lockStateTracker isLockedError:localError]){
        secnotice("octagon", "attempting to perform bottle check while locked: %@", localError);
        return UNCLEAR;
    }

    if(!identity && localError != nil){
        secerror("octagon: do not have an identity: %@", localError);
        [logger logRecoverableError:localError
                           forEvent:OctagonEventBottleCheck
                           zoneName:kOTRampZoneName
                     withAttributes:NULL];
        [tracker stop];
        if(error){
            *error = localError;
        }
        return NOBOTTLE;
    }

    //check locally first
    BOOL bottleExistsLocally = [self bottleExistsLocallyForIdentity:identity logger:logger error:&localError];

    //no bottle and we have no network
    if(!bottleExistsLocally && !self.reachabilityTracker.currentReachability){
        secnotice("octagon", "no network, can't query");
        localError = [NSError errorWithDomain:octagonErrorDomain
                                         code:OTErrorNoNetwork
                                     userInfo:@{NSLocalizedDescriptionKey: @"no network"}];
        [tracker stop];
        if(error){
            *error = localError;
        }
        return UNCLEAR;
    }
    else if(!bottleExistsLocally){
        if([self queryCloudKitForBottle:identity logger:logger error:&localError]){
            bottleStatus = BOTTLE;
        }
    }else if(bottleExistsLocally){
        bottleStatus = BOTTLE;
    }

    if(bottleStatus == NOBOTTLE){
        localError = [NSError errorWithDomain:octagonErrorDomain code:OTErrorNoBottlePeerRecords userInfo:@{NSLocalizedDescriptionKey: @"Peer %@ does not have any bottled records"}];
        secerror("octagon: this device does not have any bottled peers: %@", localError);
        [logger logRecoverableError:localError
                           forEvent:OctagonEventBottleCheck
                           zoneName:kOTRampZoneName
                     withAttributes:@{ OctagonEventAttributeFailureReason : @"does not have bottle"}];
        if(error){
            *error = localError;
        }
    }
    else{
        [logger logSuccessForEventNamed:OctagonEventBottleCheck];
    }

    [tracker stop];

    return bottleStatus;
}

-(void) postFollowUp
{
    NSError* error = nil;

    CKKSAnalytics* logger = [CKKSAnalytics logger];
    SFAnalyticsActivityTracker *tracker = [logger logSystemMetricsForActivityNamed:CKKSActivityBottleCheck withAction:nil];

    [tracker start];
    CDPFollowUpController *cdpd = [[CDPFollowUpController alloc] init];
    CDPFollowUpContext *context = [CDPFollowUpContext contextForOfflinePasscodeChange];

    [cdpd postFollowUpWithContext:context error:&error];
    if(error){
        [logger logUnrecoverableError:error forEvent:OctagonEventCoreFollowUp withAttributes:@{
                                                                                              OctagonEventAttributeFailureReason : @"core follow up failed"}];

        secerror("request to CoreCDP to follow up failed: %@", error);
    }
    else{
        [logger logSuccessForEventNamed:OctagonEventCoreFollowUp];
    }
    [tracker stop];
}


@end
#endif
