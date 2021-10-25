/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "utilities/debugging.h"
#import <os/feature_private.h>

#import "keychain/ot/OTVouchWithBottleOperation.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/OTStates.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTVouchWithBottleOperation ()
@property OTOperationDependencies* deps;

@property NSOperation* finishedOp;
@end

@implementation OTVouchWithBottleOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                            bottleID:(NSString*)bottleID
                             entropy:(NSData*)entropy
                          bottleSalt:(NSString*)bottleSalt
                         saveVoucher:(BOOL)saveVoucher
{
    if((self = [super init])) {
        _deps = dependencies;
        _intendedState = intendedState;
        _nextState = errorState;

        _bottleID = bottleID;
        _entropy = entropy;
        _bottleSalt = bottleSalt;

        _saveVoucher = saveVoucher;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "creating voucher using a bottle with escrow record id: %@", self.bottleID);

    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    if(self.bottleSalt != nil) {
        secnotice("octagon", "using passed in altdsid, altdsid is: %@", self.bottleSalt);
    } else {
        NSError *error = nil;

        NSString* altDSID = [self.deps.authKitAdapter primaryiCloudAccountAltDSID:&error];
        if(altDSID){
            secnotice("octagon", "fetched altdsid is: %@", altDSID);
            self.bottleSalt = altDSID;
        }
        else {
            secnotice("octagon", "authkit doesn't know about the altdsid, using stored value: %@", error);
            NSError* accountError = nil;
            OTAccountMetadataClassC* account = [self.deps.stateHolder loadOrCreateAccountMetadata:&accountError];

            if(account && !accountError) {
                secnotice("octagon", "retrieved account, altdsid is: %@", account.altDSID);
                self.bottleSalt = account.altDSID;
            } else {
                if (accountError == nil) {
                    accountError = [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecInternalError userInfo:nil];
                }
                secerror("failed to retrieve account object: %@", accountError);
                self.error = accountError;
                [self runBeforeGroupFinished:self.finishedOp];
                return;
            }
        }
    }

    // Preflight the vouch: this will tell us the peerID of the recovering peer.
    // Then, filter the tlkShares array to include only tlks sent to that peer.
    WEAKIFY(self);
    [self.deps.cuttlefishXPCWrapper preflightVouchWithBottleWithContainer:self.deps.containerName
                                                                  context:self.deps.contextID
                                                                 bottleID:self.bottleID
                                                                    reply:^(NSString * _Nullable peerID,
                                                                            TPSyncingPolicy* peerSyncingPolicy,
                                                                            BOOL refetchWasNeeded,
                                                                            NSError * _Nullable error) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventPreflightVouchWithBottle hardFailure:true result:error];

        if(error || !peerID) {
            secerror("octagon: Error preflighting voucher using bottle: %@", error);
            self.error = error;
            [self runBeforeGroupFinished:self.finishedOp];
            return;
        }

        secnotice("octagon", "Bottle %@ is for peerID %@", self.bottleID, peerID);

        // Tell CKKS to spin up the new views and policy
        // But, do not persist this view set! We'll do that when we actually manager to join
        [self.deps.ckks setCurrentSyncingPolicy:peerSyncingPolicy];

        [self proceedWithPeerID:peerID refetchWasNeeded:refetchWasNeeded];
    }];
}

- (void)proceedWithPeerID:(NSString*)peerID refetchWasNeeded:(BOOL)refetchWasNeeded
{
    WEAKIFY(self);

    [self.deps.cuttlefishXPCWrapper fetchRecoverableTLKSharesWithContainer:self.deps.containerName
                                                                   context:self.deps.contextID
                                                                    peerID:peerID
                                                                     reply:^(NSArray<CKRecord *> * _Nullable keyHierarchyRecords, NSError * _Nullable error) {
        STRONGIFY(self);

        if(error) {
            secerror("octagon: Error fetching TLKShares to recover: %@", error);
            // recovering these is best-effort, so fall through.
        }

        NSMutableArray<CKKSTLKShare*>* filteredTLKShares = [NSMutableArray array];
        for(CKRecord* record in keyHierarchyRecords) {
            if([record.recordType isEqual:SecCKRecordTLKShareType]) {
                CKKSTLKShareRecord* tlkShare = [[CKKSTLKShareRecord alloc] initWithCKRecord:record];
                [filteredTLKShares addObject:tlkShare.share];
            }
        }

        [self proceedWithFilteredTLKShares:filteredTLKShares];
    }];
}

- (void)proceedWithFilteredTLKShares:(NSArray<CKKSTLKShare*>*)tlkShares
{
    WEAKIFY(self);

    [self.deps.cuttlefishXPCWrapper vouchWithBottleWithContainer:self.deps.containerName
                                                         context:self.deps.contextID
                                                        bottleID:self.bottleID
                                                         entropy:self.entropy
                                                      bottleSalt:self.bottleSalt
                                                       tlkShares:tlkShares
                                                           reply:^(NSData * _Nullable voucher,
                                                                   NSData * _Nullable voucherSig,
                                                                   NSArray<CKKSTLKShare*>* _Nullable newTLKShares,
                                                                   TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                                   NSError * _Nullable error) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventVoucherWithBottle hardFailure:true result:error];

        if(error){
            secerror("octagon: Error preparing voucher using bottle: %@", error);
            self.error = error;
            [self runBeforeGroupFinished:self.finishedOp];
            return;
        }

        [[CKKSAnalytics logger] recordRecoveredTLKMetrics:tlkShares
                                       tlkRecoveryResults:tlkRecoveryResults
                                 uniqueTLKsRecoveredEvent:OctagonAnalyticsBottledUniqueTLKsRecovered
                                totalSharesRecoveredEvent:OctagonAnalyticsBottledTotalTLKSharesRecovered
                           totalRecoverableTLKSharesEvent:OctagonAnalyticsBottledTotalTLKShares
                                totalRecoverableTLKsEvent:OctagonAnalyticsBottledUniqueTLKsWithSharesCount
                                totalViewsWithSharesEvent:OctagonAnalyticsBottledTLKUniqueViewCount];

        self.voucher = voucher;
        self.voucherSig = voucherSig;

        if(self.saveVoucher) {
            secnotice("octagon", "Saving voucher for later use...");
            NSError* saveError = nil;
            [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
                metadata.voucher = voucher;
                metadata.voucherSignature = voucherSig;
                [metadata setTLKSharesPairedWithVoucher:newTLKShares];
                return metadata;
            } error:&saveError];
            if(saveError) {
                secnotice("octagon", "unable to save voucher: %@", saveError);
                [self runBeforeGroupFinished:self.finishedOp];
                return;
            }
        }

        secnotice("octagon", "Successfully vouched with a bottle: %@, %@", voucher, voucherSig);
        self.nextState = self.intendedState;
        [self runBeforeGroupFinished:self.finishedOp];
    }];
}

@end

#endif // OCTAGON
