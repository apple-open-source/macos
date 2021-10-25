/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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

#import <utilities/debugging.h>

#import <Security/SecKey.h>
#import <Security/SecKeyPriv.h>

#import "keychain/ot/OTPrepareAndRecoverTLKSharesForInheritancePeerOperation.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/OTStates.h"

#import "keychain/OctagonTrust/OTInheritanceKey.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTPrepareAndRecoverTLKSharesForInheritancePeerOperation ()
@property OTOperationDependencies* deps;
@property OTInheritanceKey* ik;
@property NSString* salt;
@property NSOperation* finishOp;
@property TrustedPeersHelperCustodianRecoveryKey *tphcrk;
@end

@implementation OTPrepareAndRecoverTLKSharesForInheritancePeerOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                                  ik:(OTInheritanceKey*)ik
                          deviceInfo:(OTDeviceInformation*)deviceInfo
                      policyOverride:(TPPolicyVersion* _Nullable)policyOverride
                  isInheritedAccount:(BOOL)isInheritedAccount
                               epoch:(uint64_t)epoch
{
    if((self = [super init])) {
        _deps = dependencies;
        _intendedState = intendedState;
        _nextState = errorState;

        _ik = ik;

        _deviceInfo = deviceInfo;
        _epoch = epoch;

        _intendedState = intendedState;
        _nextState = errorState;

        _policyOverride = policyOverride;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "creating inheritance peer and recovering shares using an inheritance key");

    self.finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishOp];

    NSString* bottleSalt = @"";

    NSString *str = [self.ik.recoveryKeyData base64EncodedStringWithOptions:0];
    self.salt = bottleSalt;
    self.tphcrk = [[TrustedPeersHelperCustodianRecoveryKey alloc] initWithUUID:self.ik.uuid.UUIDString
                                                                 encryptionKey:nil
                                                                    signingKey:nil
                                                                recoveryString:str
                                                                          salt:self.salt
                                                                          kind:TPPBCustodianRecoveryKey_Kind_INHERITANCE_KEY];
    
    __block TPPBSecureElementIdentity* existingSecureElementIdentity = nil;

    NSError* persistError = nil;

    BOOL persisted = [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
        existingSecureElementIdentity = [metadata parsedSecureElementIdentity];
        return metadata;
    } error:&persistError];

    if(!persisted || persistError) {
        secerror("octagon: failed to save 'se' state: %@", persistError);
    }
    
    WEAKIFY(self);

    [self.deps.cuttlefishXPCWrapper prepareInheritancePeerWithContainer:self.deps.containerName
                                                                context:self.deps.contextID
                                                                  epoch:self.epoch
                                                              machineID:self.deviceInfo.machineID
                                                             bottleSalt:bottleSalt
                                                               bottleID:[NSUUID UUID].UUIDString
                                                                modelID:self.deviceInfo.modelID
                                                             deviceName:self.deviceInfo.deviceName
                                                           serialNumber:self.deviceInfo.serialNumber
                                                              osVersion:self.deviceInfo.osVersion
                                                          policyVersion:self.policyOverride
                                                          policySecrets:nil
                                              syncUserControllableViews:TPPBPeerStableInfoUserControllableViewStatus_UNKNOWN
                                                  secureElementIdentity:existingSecureElementIdentity
                                            signingPrivKeyPersistentRef:nil
                                                encPrivKeyPersistentRef:nil
                                                                    crk:self.tphcrk
                                                                  reply:^(NSString * _Nullable peerID,
                                                                          NSData * _Nullable permanentInfo,
                                                                          NSData * _Nullable permanentInfoSig,
                                                                          NSData * _Nullable stableInfo,
                                                                          NSData * _Nullable stableInfoSig,
                                                                          TPSyncingPolicy * _Nullable syncingPolicy,
                                                                          NSString * _Nullable recoveryKeyID,
                                                                          NSArray<CKRecord*>* _Nullable keyHierarchyRecords,
                                                                          NSError * _Nullable error) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventPrepareIdentity hardFailure:true result:error];
        if(error) {
            secerror("octagon-inheritor: Error preparing inheritor identity: %@", error);
            self.error = error;
            [self runBeforeGroupFinished:self.finishOp];
        } else {
            secnotice("octagon-inheritor", "Prepared: %@ %@ %@", peerID, permanentInfo, permanentInfoSig);
            self.peerID = peerID;
            self.permanentInfo = permanentInfo;
            self.permanentInfoSig = permanentInfoSig;
            self.stableInfo = stableInfo;
            self.stableInfoSig = stableInfoSig;

            NSError* localError = nil;

            secnotice("octagon-inheritor", "New syncing policy: %@ views: %@", syncingPolicy, syncingPolicy.viewList);

            NSMutableArray<CKKSTLKShare*>* filteredTLKShares = [NSMutableArray array];
            for(CKRecord* record in keyHierarchyRecords) {
                if([record.recordType isEqual:SecCKRecordTLKShareType]) {
                    CKKSTLKShareRecord* tlkShare = [[CKKSTLKShareRecord alloc] initWithCKRecord:record];
                    [filteredTLKShares addObject:tlkShare.share];
                }
            }

            BOOL persisted = [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
                metadata.peerID = peerID;
                metadata.trustState = OTAccountMetadataClassC_TrustState_TRUSTED;

                metadata.isInheritedAccount = YES;
                [metadata.tlkSharesForVouchedIdentitys removeAllObjects];
                [metadata setTPSyncingPolicy:syncingPolicy];
                return metadata;
            } error:&localError];

            if(!persisted || localError) {
                secnotice("octagon-inheritor", "Couldn't persist metadata: %@", localError);
                self.error = localError;
                [self runBeforeGroupFinished:self.finishOp];
                return;
            }

            // Let CKKS know of the new policy, so it can spin up
            [self.deps.ckks setCurrentSyncingPolicy:syncingPolicy];
            [self proceedWithFilteredTLKShares:filteredTLKShares];
        }
    }];
}


- (void)proceedWithFilteredTLKShares:(NSArray<CKKSTLKShare*>*)tlkShares
{
    WEAKIFY(self);

    [self.deps.cuttlefishXPCWrapper recoverTLKSharesForInheritorWithContainer:self.deps.containerName
                                                                      context:self.deps.contextID
                                                                          crk:self.tphcrk
                                                                    tlkShares:tlkShares
                                                                        reply:^(NSArray<CKKSTLKShare *> * _Nullable newSelfTLKShares,
                                                                                TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                                                NSError * _Nullable error) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventVoucherWithInheritanceKey hardFailure:true result:error];
        if(error){
            secerror("octagon-inheritor: Error recovering tlkshares: %@", error);
            self.error = error;
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }

        [[CKKSAnalytics logger] recordRecoveredTLKMetrics:tlkShares
                                       tlkRecoveryResults:tlkRecoveryResults
                                 uniqueTLKsRecoveredEvent:OctagonAnalyticsInheritanceUniqueTLKsRecovered
                                totalSharesRecoveredEvent:OctagonAnalyticsInheritanceTotalTLKSharesRecovered
                           totalRecoverableTLKSharesEvent:OctagonAnalyticsInheritanceTotalTLKShares
                                totalRecoverableTLKsEvent:OctagonAnalyticsInheritanceUniqueTLKsWithSharesCount
                                totalViewsWithSharesEvent:OctagonAnalyticsInheritanceTLKUniqueViewCount];

        secnotice("octagon-inheritor", "Saving tlkshares for later use...");
        NSError* saveError = nil;
        [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
            [metadata setTLKSharesPairedWithVoucher:newSelfTLKShares];
            return metadata;
        } error:&saveError];
        if(saveError) {
            secnotice("octagon-inheritor", "unable to save shares: %@", saveError);
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }

        secnotice("octagon-inheritor", "Successfully recovered shares");
        self.nextState = self.intendedState;
        [self runBeforeGroupFinished:self.finishOp];
    }];
}

@end

#endif // OCTAGON
