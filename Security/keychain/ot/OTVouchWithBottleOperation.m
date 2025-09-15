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
#import "keychain/categories/NSError+UsefulConstructors.h"
#import <os/feature_private.h>

#import "keychain/ot/OTVouchWithBottleOperation.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/OTStates.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

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
    if ((self = [super init])) {
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

    AAFAnalyticsEventSecurity* vouchWithBottleEvent = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                               altDSID:self.deps.activeAccount.altDSID
                                                                                                                flowID:self.deps.flowID
                                                                                                       deviceSessionID:self.deps.deviceSessionID
                                                                                                             eventName:kSecurityRTCEventNameVouchWithBottle
                                                                                                       testsAreEnabled:SecCKKSTestsEnabled()
                                                                                                        canSendMetrics:self.deps.permittedToSendMetrics
                                                                                                              category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    if (self.bottleSalt != nil) {
        secnotice("octagon", "using passed in altdsid, altdsid is: %@", self.bottleSalt);
    } else {
        NSString* altDSID = self.deps.activeAccount.altDSID;
        if (altDSID == nil) {
            secnotice("authkit", "No configured altDSID: %@", self.deps.activeAccount);
            self.error = [NSError errorWithDomain:OctagonErrorDomain
                                             code:OctagonErrorNoAppleAccount
                                      description:@"No altDSID configured"];
            [self runBeforeGroupFinished:self.finishedOp];
            [vouchWithBottleEvent sendMetricWithResult:NO error:self.error];
            return;
        }

        self.bottleSalt = altDSID;
    }

    // Preflight the vouch: this will tell us the peerID of the recovering peer.
    // Then, filter the tlkShares array to include only tlks sent to that peer.
    WEAKIFY(self);
    [self.deps.cuttlefishXPCWrapper preflightVouchWithBottleWithSpecificUser:self.deps.activeAccount
                                                                    bottleID:self.bottleID
                                                                     altDSID:self.deps.activeAccount.altDSID
                                                                      flowID:self.deps.flowID
                                                             deviceSessionID:self.deps.deviceSessionID
                                                              canSendMetrics:self.deps.permittedToSendMetrics
                                                                       reply:^(NSString * _Nullable peerID,
                                                                               TPSyncingPolicy* peerSyncingPolicy,
                                                                               BOOL refetchWasNeeded,
                                                                               NSError * _Nullable preflightError) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventPreflightVouchWithBottle hardFailure:true result:preflightError];

        if (preflightError || !peerID) {
            secerror("octagon: Error preflighting voucher using bottle: %@", preflightError);
            self.error = preflightError;
            [self runBeforeGroupFinished:self.finishedOp];
            [vouchWithBottleEvent sendMetricWithResult:NO error:self.error];
            return;
        }

        secnotice("octagon", "Bottle %@ is for peerID %@", self.bottleID, peerID);

        // Tell CKKS to spin up the new views and policy
        // But, do not persist this view set! We'll do that when we actually manager to join
        [self.deps.ckks setCurrentSyncingPolicy:peerSyncingPolicy];

        [self proceedWithPeerID:peerID refetchWasNeeded:refetchWasNeeded vouchWithBottleEvent:vouchWithBottleEvent];
    }];
}

- (void)proceedWithPeerID:(NSString*)peerID
         refetchWasNeeded:(BOOL)refetchWasNeeded
     vouchWithBottleEvent:(AAFAnalyticsEventSecurity*)vouchWithBottleEvent
{
    WEAKIFY(self);

    [self.deps.cuttlefishXPCWrapper fetchRecoverableTLKSharesWithSpecificUser:self.deps.activeAccount
                                                                       peerID:peerID
                                                                      altDSID:self.deps.activeAccount.altDSID
                                                                       flowID:self.deps.flowID
                                                              deviceSessionID:self.deps.deviceSessionID
                                                               canSendMetrics:self.deps.permittedToSendMetrics
                                                                        reply:^(NSArray<CKRecord *> * _Nullable keyHierarchyRecords, NSError * _Nullable fetchError) {
        STRONGIFY(self);

        if (fetchError) {
            secerror("octagon: Error fetching TLKShares to recover: %@", fetchError);
            self.error = fetchError;
            [self runBeforeGroupFinished:self.finishedOp];
            [vouchWithBottleEvent sendMetricWithResult:NO error:self.error];
            return;
        }

        NSMutableArray<CKKSTLKShare*>* filteredTLKShares = [NSMutableArray array];
        for(CKRecord* record in keyHierarchyRecords) {
            if ([record.recordType isEqual:SecCKRecordTLKShareType]) {
                CKKSTLKShareRecord* tlkShare = [[CKKSTLKShareRecord alloc] initWithCKRecord:record contextID:self.deps.contextID];
                [filteredTLKShares addObject:tlkShare.share];
            }
        }

        [self proceedWithFilteredTLKShares:filteredTLKShares vouchWithBottleEvent:vouchWithBottleEvent];
    }];
}

- (void)proceedWithFilteredTLKShares:(NSArray<CKKSTLKShare*>*)tlkShares
                vouchWithBottleEvent:(AAFAnalyticsEventSecurity*)vouchWithBottleEvent
{
    WEAKIFY(self);

    [self.deps.cuttlefishXPCWrapper vouchWithBottleWithSpecificUser:self.deps.activeAccount
                                                           bottleID:self.bottleID
                                                            entropy:self.entropy
                                                         bottleSalt:self.bottleSalt
                                                          tlkShares:tlkShares
                                                            altDSID:self.deps.activeAccount.altDSID
                                                             flowID:self.deps.flowID
                                                    deviceSessionID:self.deps.deviceSessionID
                                                     canSendMetrics:self.deps.permittedToSendMetrics
                                                              reply:^(NSData * _Nullable voucher,
                                                                      NSData * _Nullable voucherSig,
                                                                      NSArray<CKKSTLKShare*>* _Nullable newTLKShares,
                                                                      TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                                      NSError * _Nullable vouchError) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventVoucherWithBottle hardFailure:true result:vouchError];

        if (vouchError) {
            secerror("octagon: Error preparing voucher using bottle: %@", vouchError);
            self.error = vouchError;
            [self runBeforeGroupFinished:self.finishedOp];
            [vouchWithBottleEvent sendMetricWithResult:NO error:self.error];
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

        if (self.saveVoucher) {
            secnotice("octagon", "Saving voucher for later use...");
            NSError* saveError = nil;
            [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nullable(OTAccountMetadataClassC * _Nonnull metadata) {
                metadata.voucher = voucher;
                metadata.voucherSignature = voucherSig;
                [metadata setTLKSharesPairedWithVoucher:newTLKShares];
                return metadata;
            } error:&saveError];
            if (saveError) {
                secnotice("octagon", "unable to save voucher: %@", saveError);
                self.error = saveError;
                [self runBeforeGroupFinished:self.finishedOp];
                [vouchWithBottleEvent sendMetricWithResult:NO error:self.error];
                return;
            }
        }

        secnotice("octagon", "Successfully vouched with a bottle: %@, %@", voucher, voucherSig);
        self.nextState = self.intendedState;
        [self runBeforeGroupFinished:self.finishedOp];
        [vouchWithBottleEvent sendMetricWithResult:YES error:nil];
    }];
}

@end

#endif // OCTAGON
