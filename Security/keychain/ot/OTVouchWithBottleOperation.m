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

#import <utilities/debugging.h>

#import "keychain/ot/OTVouchWithBottleOperation.h"
#import "keychain/ot/OTClientStateMachine.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"

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
{
    if((self = [super init])) {
        _deps = dependencies;
        _intendedState = intendedState;
        _nextState = errorState;

        _bottleID = bottleID;
        _entropy = entropy;
        _bottleSalt = bottleSalt;
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
    } else{
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
            }
            if(accountError || !account){
                secerror("failed to rerieve account object: %@", accountError);
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
                                                                            NSSet<NSString*>* peerSyncingViews,
                                                                            TPPolicy* peerSyncingPolicy,
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
        [self.deps.viewManager setSyncingViews:peerSyncingViews sortingPolicy:peerSyncingPolicy];

        [self proceedWithPeerID:peerID];
    }];
}

- (void)proceedWithPeerID:(NSString*)peerID
{
    WEAKIFY(self);

    // After a vouch, we also want to acquire all TLKs that the bottled peer might have had
    OTFetchCKKSKeysOperation* fetchKeysOp = [[OTFetchCKKSKeysOperation alloc] initWithDependencies:self.deps];
    [self runBeforeGroupFinished:fetchKeysOp];

    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"bottle-tlks"
                                                            withBlock:^{
        STRONGIFY(self);

        NSMutableArray<CKKSTLKShare*>* filteredTLKShares = [NSMutableArray array];
        for(CKKSTLKShare* share in fetchKeysOp.tlkShares) {
            // If we didn't get a peerID, just pass every tlkshare and hope for the best
            if(peerID == nil || [share.receiverPeerID isEqualToString:peerID]) {
                [filteredTLKShares addObject:share];
            }
        }

        [self proceedWithKeys:fetchKeysOp.viewKeySets filteredTLKShares:filteredTLKShares];
    }];

    [proceedWithKeys addDependency:fetchKeysOp];
    [self runBeforeGroupFinished:proceedWithKeys];
}


- (void)noteMetric:(NSString*)metric count:(int64_t)count
{
    NSString* metricName = [NSString stringWithFormat:@"%@%lld", metric, count];

    [[CKKSAnalytics logger] logResultForEvent:metricName
                                  hardFailure:NO
                                       result:nil];

    [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:metricName];
    [[CKKSAnalytics logger] setNumberProperty:[[NSNumber alloc]initWithLong:count] forKey:metric];
}

- (void)proceedWithKeys:(NSArray<CKKSKeychainBackedKeySet*>*)viewKeySets filteredTLKShares:(NSArray<CKKSTLKShare*>*)tlkShares
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
                                                                   int64_t uniqueTLKsRecovered,
                                                                   int64_t totalTLKSharesRecovered,
                                                                   NSError * _Nullable error) {
            STRONGIFY(self);
            [[CKKSAnalytics logger] logResultForEvent:OctagonEventVoucherWithBottle hardFailure:true result:error];

            if(error){
                secerror("octagon: Error preparing voucher using bottle: %@", error);
                self.error = error;
                [self runBeforeGroupFinished:self.finishedOp];
                return;
            }

            //collect TLK count metrics
            [self noteMetric:OctagonAnalyticsBottledUniqueTLKsRecovered count:uniqueTLKsRecovered];
            [self noteMetric:OctagonAnalyticsBottledTotalTLKSharesRecovered count:totalTLKSharesRecovered];
            [self noteMetric:OctagonAnalyticsBottledTotalTLKShares count:tlkShares.count];

            NSMutableSet<NSString*>* uniqueTLKsWithShares = [NSMutableSet set];
            for (CKKSTLKShare* share in tlkShares) {
                [uniqueTLKsWithShares addObject:share.tlkUUID];
            }

            [self noteMetric:OctagonAnalyticsBottledUniqueTLKsWithSharesCount count:uniqueTLKsWithShares.count];

            NSMutableDictionary *views = [NSMutableDictionary dictionary];
            for (CKKSTLKShare *share in tlkShares) {
                views[share.zoneID] = share.zoneID;
            }
            [self noteMetric:OctagonAnalyticsBottledTLKUniqueViewCount count:views.count];

            secnotice("octagon", "Received bottle voucher");

            self.voucher = voucher;
            self.voucherSig = voucherSig;
            self.nextState = self.intendedState;
            [self runBeforeGroupFinished:self.finishedOp];
        }];
}

@end

#endif // OCTAGON
