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
#import <SecurityFoundation/SFKey_Private.h>

#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTDefines.h"

#include <utilities/SecFileLocations.h>
#include <Security/SecRandomP.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSAnalytics.h"

#import <CoreCDP/CDPAccount.h>

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

@property (nonatomic, strong) CKKSAccountStateTracker* accountTracker;
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
                                                    zoneModifier:[CKKSViewManager manager].zoneModifier
                                       cloudKitClassDependencies:[CKKSCloudKitClassDependencies forLiveCloudKit]
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
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorNoIdentity userInfo:@{NSLocalizedDescriptionKey: @"OTIdentity does not have an SOS peer id"}];
        }
        return nil;
    }

    OTPreflightInfo* info = [[OTPreflightInfo alloc]init];
    info.escrowedSigningSPKI = bprec.escrowedSigningSPKI;

    if(!info.escrowedSigningSPKI){
        if(error){
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorEscrowSigningSPKI userInfo:@{NSLocalizedDescriptionKey: @"Escrowed spinging SPKI is nil"}];
        }
        secerror("octagon: Escrowed spinging SPKI is nil");
        return nil;
    }

    info.bottleID = bprec.recordName;
    if(!info.bottleID){
        if(error){
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorBottleID userInfo:@{NSLocalizedDescriptionKey: @"BottleID is nil"}];
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
                                                escrowSigningSPKI:[escrowKeys.signingKey.publicKey encodeSubjectPublicKeyInfo]];
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

-(BOOL)updateBottleForPeerID:(NSString*)peerID
               newSigningKey:(SFECKeyPair*)signingKey
            newEncryptionKey:(SFECKeyPair*)encryptionKey
                escrowKeySet:(OTEscrowKeys*)keySet
                       error:(NSError**)error
{
    NSError* localError = nil;

    //let's rebottle our bottles!
    OTBottledPeer *bp = [[OTBottledPeer alloc] initWithPeerID:nil
                                                         spID:peerID
                                               peerSigningKey:signingKey
                                            peerEncryptionKey:encryptionKey
                                                   escrowKeys:keySet
                                                        error:&localError];
    if (!bp || localError !=nil) {
        secerror("octagon: unable to create a bottled peer: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }
    OTBottledPeerSigned* bpSigned = [[OTBottledPeerSigned alloc] initWithBottledPeer:bp
                                                                  escrowedSigningKey:keySet.signingKey
                                                                      peerSigningKey:signingKey
                                                                               error:&localError];
    if(!bpSigned || localError){
        secerror("octagon: unable to create a signed bottled peer: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }

    OTBottledPeerRecord *newRecord = [bpSigned asRecord:peerID];

    BOOL uploaded = [self.cloudStore uploadBottledPeerRecord:newRecord escrowRecordID:peerID error:&localError];
    if(!uploaded || localError){
        secerror("octagon: unable to upload bottled peer: %@", localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }

    return YES;
}

-(BOOL)updateAllBottlesForPeerID:(NSString*)peerID
                   newSigningKey:(SFECKeyPair*)newSigningKey
                newEncryptionKey:(SFECKeyPair*)newEncryptionKey
                           error:(NSError**)error
{
    BOOL result = NO;
    BOOL atLeastOneHasBeenUpdated = NO;

    NSError* localError = nil;

    SFECKeyPair *escrowSigningKey = nil;
    SFECKeyPair *escrowEncryptionKey = nil;
    SFAESKey *escrowSymmetricKey = nil;

    result = [self.cloudStore downloadBottledPeerRecord:&localError];
    if(!result || localError){
        secnotice("octagon", "could not download bottles from cloudkit: %@", localError);
    }

    secnotice("octagon", "checking local store for downloaded bottles");

    NSArray<OTBottledPeerRecord*>* bottles = [self.localStore readLocalBottledPeerRecordsWithMatchingPeerID:peerID error:&localError];
    if(!bottles || localError){
        secnotice("octagon", "peer %@ enrolled 0 bottles", peerID);
        if (error) {
            *error = localError;
        }
        return result;
    }

    //iterate through all the bottles and attempt to update all the bottles
    for(OTBottledPeerRecord* bottle in bottles){

        NSString* escrowSigningPubKeyHash = [OTEscrowKeys hashEscrowedSigningPublicKey:bottle.escrowedSigningSPKI];

        BOOL foundKeys = [OTEscrowKeys findEscrowKeysForLabel:escrowSigningPubKeyHash
                                              foundSigningKey:&escrowSigningKey
                                           foundEncryptionKey:&escrowEncryptionKey
                                            foundSymmetricKey:&escrowSymmetricKey
                                                        error:&localError];
        if(!foundKeys){
            secnotice("octagon", "found 0 persisted escrow keys for label: %@", escrowSigningPubKeyHash);
            continue;
        }

        OTEscrowKeys *retrievedKeySet = [[OTEscrowKeys alloc]initWithSigningKey:escrowSigningKey encryptionKey:escrowEncryptionKey symmetricKey:escrowSymmetricKey];

        if(!retrievedKeySet){
            secnotice("octagon", "failed to create escrow keys");
            continue;
        }

        BOOL updated = [self updateBottleForPeerID:peerID newSigningKey:newSigningKey newEncryptionKey:newEncryptionKey escrowKeySet:retrievedKeySet error:&localError];

        if(!updated || localError){
            secnotice("octagon", "could not updated bottle for peerid: %@ for escrowed signing public key hash: %@", peerID, escrowSigningPubKeyHash);
        }else{
            atLeastOneHasBeenUpdated = YES;
        }
    }

    if(!atLeastOneHasBeenUpdated){
        secerror("octagon: no bottles were updated for : %@", peerID);
        result = NO;

        localError = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorBottleUpdate userInfo:@{NSLocalizedDescriptionKey: @"bottle update failed, 0 bottles updated."}];
        if(error){
            *error = localError;
        }
    }else{
        result = YES;
        secnotice("octagon", "bottles were updated for : %@", peerID);
    }
    return result;
}

-(BOOL)bottleExistsLocallyForIdentity:(OTIdentity*)identity
                                error:(NSError**)error
{
    NSError* localError = nil;
    //read all the local bp records
    NSArray<OTBottledPeerRecord*>* bottles = [self.localStore readLocalBottledPeerRecordsWithMatchingPeerID:identity.spID error:&localError];
    if(!bottles || [bottles count] == 0 || localError != nil){
        secerror("octagon: there are no eligible bottle peer records: %@", localError);
        if(error){
            *error = localError;
        }
        return NO;
    }

    BOOL hasBottle = NO;
    //if check all the records if the peer signing public key matches the bottled one!
    for(OTBottledPeerRecord* bottle in bottles){
        NSData* bottledSigningSPKIData = [[SFECPublicKey keyWithSubjectPublicKeyInfo:bottle.peerSigningSPKI] keyData];
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

-(BOOL)queryCloudKitForBottle:(OTIdentity*)identity
                        error:(NSError**)error
{
    NSError* localError = nil;
    BOOL hasBottle = NO;
    //attempt to pull down all the records, but continue checking local store even if this fails.
    BOOL fetched = [self.cloudStore downloadBottledPeerRecord:&localError];
    if(fetched == NO || localError != nil){ //couldn't download bottles
        secerror("octagon: 0 bottled peers downloaded: %@", localError);
        if(error){
            *error = localError;
        }
        return NO;
    }else{ //downloaded bottles, let's check local store
        hasBottle = [self bottleExistsLocallyForIdentity:identity error:&localError];
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

    // Wait until the account tracker has had a chance to figure out the state
    [self.accountTracker.ckAccountInfoInitialized wait:5*NSEC_PER_SEC];

    if(self.accountTracker.currentCKAccountInfo.accountStatus != CKAccountStatusAvailable){
        if(error){
            *error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OTErrorNotSignedIn
                                     userInfo:@{NSLocalizedDescriptionKey: @"iCloud account is logged out"}];
        }
        secnotice("octagon", "not logged into an account");
        return UNCLEAR;
    }

    NSError* localError = nil;
    OctagonBottleCheckState bottleStatus = NOBOTTLE;

    //get our current identity
    OTIdentity* identity = [self.identityProvider currentIdentity:&localError];

    //if we get the locked error, return true so we don't prompt the user
    if(localError && [_lockStateTracker isLockedError:localError]){
        secnotice("octagon", "attempting to perform bottle check while locked: %@", localError);
        return UNCLEAR;
    }

    if(!identity && localError != nil){
        secerror("octagon: do not have an identity: %@", localError);
        if(error){
            *error = localError;
        }
        return NOBOTTLE;
    }

    //check locally first
    BOOL bottleExistsLocally = [self bottleExistsLocallyForIdentity:identity error:&localError];

    //no bottle and we have no network
    if(!bottleExistsLocally && !self.reachabilityTracker.currentReachability){
        secnotice("octagon", "no network, can't query");
        localError = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OTErrorNoNetwork
                                     userInfo:@{NSLocalizedDescriptionKey: @"no network"}];
        if(error){
            *error = localError;
        }
        return UNCLEAR;
    }
    else if(!bottleExistsLocally){
        if([self queryCloudKitForBottle:identity error:&localError]){
            bottleStatus = BOTTLE;
        }
    }else if(bottleExistsLocally){
        bottleStatus = BOTTLE;
    }

    if(bottleStatus == NOBOTTLE){
        localError = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OTErrorNoBottlePeerRecords
                                     userInfo:@{NSLocalizedDescriptionKey: @"Peer does not have any bottled records"}];
        secerror("octagon: this device does not have any bottled peers: %@", localError);
        if(error){
            *error = localError;
        }
    }

    return bottleStatus;
}

@end
#endif
