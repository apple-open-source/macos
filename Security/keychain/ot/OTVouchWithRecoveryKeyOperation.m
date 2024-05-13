/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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
#import "keychain/categories/NSError+UsefulConstructors.h"

#import "keychain/ot/OTVouchWithRecoveryKeyOperation.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/OTStates.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTVouchWithRecoveryKeyOperation ()
@property OTOperationDependencies* deps;

@property NSString* salt;
@property NSString* recoveryKey;

@property NSOperation* finishOp;
@end

@implementation OTVouchWithRecoveryKeyOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                         recoveryKey:(NSString*)recoveryKey
                         saveVoucher:(BOOL)saveVoucher
{
    if((self = [super init])) {
        _deps = dependencies;
        _intendedState = intendedState;
        _nextState = errorState;

        _recoveryKey = recoveryKey;

        _saveVoucher = saveVoucher;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "creating voucher using a recovery key");

    self.finishOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishOp];

    NSString* altDSID = self.deps.activeAccount.altDSID;
    if(altDSID == nil) {
        secnotice("authkit", "No configured altDSID: %@", self.deps.activeAccount);
        self.error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorNoAppleAccount
                                  description:@"No altDSID configured"];
        [self runBeforeGroupFinished:self.finishOp];
        return;
    }

    self.salt = altDSID;

    // First, let's preflight the vouch (to receive a policy and view set to use for TLK fetching
    WEAKIFY(self);
    [self.deps.cuttlefishXPCWrapper preflightVouchWithRecoveryKeyWithSpecificUser:self.deps.activeAccount
                                                                      recoveryKey:self.recoveryKey
                                                                             salt:self.salt
                                                                            reply:^(NSString * _Nullable recoveryKeyID,
                                                                                    TPSyncingPolicy* _Nullable peerSyncingPolicy,
                                                                                    NSError * _Nullable error) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventPreflightVouchWithRecoveryKey hardFailure:true result:error];

        if(error || !recoveryKeyID) {
            secerror("octagon: Error preflighting voucher using recovery key: %@", error);
            self.error = error;
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }

        secnotice("octagon", "Recovery key ID %@ looks good to go", recoveryKeyID);

        // Tell CKKS to spin up the new views and policy
        // But, do not persist this view set! We'll do that when we actually manage to join
        [self.deps.ckks setCurrentSyncingPolicy:peerSyncingPolicy];

        [self proceedWithRecoveryKeyID:recoveryKeyID];
    }];
}

- (void)proceedWithRecoveryKeyID:(NSString*)recoveryKeyID
{
    WEAKIFY(self);

    [self.deps.cuttlefishXPCWrapper fetchRecoverableTLKSharesWithSpecificUser:self.deps.activeAccount
                                                                       peerID:recoveryKeyID
                                                                        reply:^(NSArray<CKRecord *> * _Nullable keyHierarchyRecords,
                                                                                NSError * _Nullable error) {
        STRONGIFY(self);

        if(error) {
            secerror("octagon: Error fetching TLKShares to recover: %@", error);
            self.error = error;
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }

        NSMutableArray<CKKSTLKShare*>* filteredTLKShares = [NSMutableArray array];
        for(CKRecord* record in keyHierarchyRecords) {
            if([record.recordType isEqual:SecCKRecordTLKShareType]) {
                CKKSTLKShareRecord* tlkShare = [[CKKSTLKShareRecord alloc] initWithCKRecord:record contextID:self.deps.contextID];
                [filteredTLKShares addObject:tlkShare.share];
            }
        }

        [self proceedWithFilteredTLKShares:filteredTLKShares];
    }];
}

- (void)proceedWithFilteredTLKShares:(NSArray<CKKSTLKShare*>*)tlkShares
{
    WEAKIFY(self);

    [self.deps.cuttlefishXPCWrapper vouchWithRecoveryKeyWithSpecificUser:self.deps.activeAccount
                                                             recoveryKey:self.recoveryKey
                                                                    salt:self.salt
                                                               tlkShares:tlkShares
                                                                   reply:^(NSData * _Nullable voucher,
                                                                           NSData * _Nullable voucherSig,
                                                                           NSArray<CKKSTLKShare*>* _Nullable newTLKShares,
                                                                           TrustedPeersHelperTLKRecoveryResult* _Nullable tlkRecoveryResults,
                                                                           NSError * _Nullable error) {
        STRONGIFY(self);
        [[CKKSAnalytics logger] logResultForEvent:OctagonEventVoucherWithRecoveryKey hardFailure:true result:error];
        if(error){
            secerror("octagon: Error preparing voucher using recovery key: %@", error);
            self.error = error;
            [self runBeforeGroupFinished:self.finishOp];
            return;
        }


        [[CKKSAnalytics logger] recordRecoveredTLKMetrics:tlkShares
                                       tlkRecoveryResults:tlkRecoveryResults
                                 uniqueTLKsRecoveredEvent:OctagonAnalyticsRKUniqueTLKsRecovered
                                totalSharesRecoveredEvent:OctagonAnalyticsRKTotalTLKSharesRecovered
                           totalRecoverableTLKSharesEvent:OctagonAnalyticsRKTotalTLKShares
                                totalRecoverableTLKsEvent:OctagonAnalyticsRKUniqueTLKsWithSharesCount
                                totalViewsWithSharesEvent:OctagonAnalyticsRKTLKUniqueViewCount];

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
                [self runBeforeGroupFinished:self.finishOp];
                return;
            }
        }

        secnotice("octagon", "Successfully vouched with a recovery key: %@, %@", voucher, voucherSig);
        self.nextState = self.intendedState;
        [self runBeforeGroupFinished:self.finishOp];
    }];
}

@end

#endif // OCTAGON
