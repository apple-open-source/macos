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

#import "TPModel.h"
#import "TPPeer.h"
#import "TPHash.h"
#import "TPCircle.h"
#import "TPVoucher.h"
#import "TPPeerPermanentInfo.h"
#import "TPPeerStableInfo.h"
#import "TPPeerDynamicInfo.h"
#import "TPPolicy.h"
#import "TPPolicyDocument.h"


@interface TPModel ()
@property (nonatomic, strong) NSMutableDictionary<NSString*, TPPeer*>* peersByID;
@property (nonatomic, strong) NSMutableDictionary<NSString*, TPCircle*>* circlesByID;
@property (nonatomic, strong) NSMutableDictionary<NSNumber*, TPPolicyDocument*>* policiesByVersion;
@property (nonatomic, strong) NSMutableSet<TPVoucher*>* vouchers;
@property (nonatomic, strong) id<TPDecrypter> decrypter;
@end

@implementation TPModel

- (instancetype)initWithDecrypter:(id<TPDecrypter>)decrypter
{
    self = [super init];
    if (self) {
        _peersByID = [[NSMutableDictionary alloc] init];
        _circlesByID = [[NSMutableDictionary alloc] init];
        _policiesByVersion = [[NSMutableDictionary alloc] init];
        _vouchers = [[NSMutableSet alloc] init];
        _decrypter = decrypter;
    }
    return self;
}

- (TPCounter)latestEpochAmongPeerIDs:(NSSet<NSString*> *)peerIDs
{
    TPCounter latestEpoch = 0;
    for (NSString *peerID in peerIDs) {
        TPPeer *peer = self.peersByID[peerID];
        if (nil == peer) {
            continue;
        }
        latestEpoch = MAX(latestEpoch, peer.permanentInfo.epoch);
    }
    return latestEpoch;
}

- (void)registerPolicyDocument:(TPPolicyDocument *)policyDoc
{
    NSAssert(policyDoc, @"policyDoc must not be nil");
    self.policiesByVersion[@(policyDoc.policyVersion)] = policyDoc;
}

- (void)registerPeerWithPermanentInfo:(TPPeerPermanentInfo *)permanentInfo
{
    NSAssert(permanentInfo, @"permanentInfo must not be nil");
    if (nil == [self.peersByID objectForKey:permanentInfo.peerID]) {
        TPPeer *peer = [[TPPeer alloc] initWithPermanentInfo:permanentInfo];
        [self.peersByID setObject:peer forKey:peer.peerID];
    } else {
        // Do nothing, to avoid overwriting the existing peer object which might have accumulated state.
    }
}

- (void)deletePeerWithID:(NSString *)peerID
{
    [self.peersByID removeObjectForKey:peerID];
}

- (BOOL)hasPeerWithID:(NSString *)peerID
{
    return nil != self.peersByID[peerID];
}

- (nonnull TPPeer *)peerWithID:(NSString *)peerID
{
    TPPeer *peer = [self.peersByID objectForKey:peerID];
    NSAssert(nil != peer, @"peerID is not registered: %@", peerID);
    return peer;
}

- (TPPeerStatus)statusOfPeerWithID:(NSString *)peerID
{
    TPPeer *peer = [self peerWithID:peerID];
    TPPeerStatus status = 0;
    if (0 < [peer.circle.includedPeerIDs count]) {
        status |= TPPeerStatusFullyReciprocated;
        // This flag might get cleared again, below.
    }
    for (NSString *otherID in peer.circle.includedPeerIDs) {
        if ([peerID isEqualToString:otherID]) {
            continue;
        }
        TPPeer *otherPeer = self.peersByID[otherID];
        if (nil == otherPeer) {
            continue;
        }
        if ([otherPeer.circle.includedPeerIDs containsObject:peerID]) {
            status |= TPPeerStatusPartiallyReciprocated;
        } else {
            status &= ~TPPeerStatusFullyReciprocated;
        }
        if ([otherPeer.circle.excludedPeerIDs containsObject:peerID]) {
            status |= TPPeerStatusExcluded;
        }
        if (otherPeer.permanentInfo.epoch > peer.permanentInfo.epoch) {
            status |= TPPeerStatusOutdatedEpoch;
        }
        if (otherPeer.permanentInfo.epoch > peer.permanentInfo.epoch + 1) {
            status |= TPPeerStatusAncientEpoch;
        }
    }
    if ([peer.circle.excludedPeerIDs containsObject:peerID]) {
        status |= TPPeerStatusExcluded;
    }
    return status;
}


- (TPPeerPermanentInfo *)getPermanentInfoForPeerWithID:(NSString *)peerID
{
    return [self peerWithID:peerID].permanentInfo;
}

- (TPPeerStableInfo *)getStableInfoForPeerWithID:(NSString *)peerID
{
    return [self peerWithID:peerID].stableInfo;
}

- (NSData *)getWrappedPrivateKeysForPeerWithID:(NSString *)peerID
{
    return [self peerWithID:peerID].wrappedPrivateKeys;
}

- (void)setWrappedPrivateKeys:(nullable NSData *)wrappedPrivateKeys
                forPeerWithID:(NSString *)peerID
{
    [self peerWithID:peerID].wrappedPrivateKeys = wrappedPrivateKeys;
}

- (TPPeerDynamicInfo *)getDynamicInfoForPeerWithID:(NSString *)peerID
{
    return [self peerWithID:peerID].dynamicInfo;
}

- (TPCircle *)getCircleForPeerWithID:(NSString *)peerID
{
    return [self peerWithID:peerID].circle;
}

- (void)registerCircle:(TPCircle *)circle
{
    NSAssert(circle, @"circle must not be nil");
    [self.circlesByID setObject:circle forKey:circle.circleID];
    
    // A dynamicInfo might have been set on a peer before we had the circle identified by dynamicInfo.circleID.
    // Check if this circle is referenced by any dynamicInfo.circleID.
    [self.peersByID enumerateKeysAndObjectsUsingBlock:^(NSString *peerID, TPPeer *peer, BOOL *stop) {
        if (nil == peer.circle && [peer.dynamicInfo.circleID isEqualToString:circle.circleID]) {
            peer.circle = circle;
        }
    }];
}

- (void)deleteCircleWithID:(NSString *)circleID
{
    [self.peersByID enumerateKeysAndObjectsUsingBlock:^(NSString *peerID, TPPeer *peer, BOOL *stop) {
        NSAssert(![circleID isEqualToString:peer.dynamicInfo.circleID],
                 @"circle being deleted is in use by peer %@, circle %@", peerID, circleID);
    }];
    [self.circlesByID removeObjectForKey:circleID];
}

- (TPCircle *)circleWithID:(NSString *)circleID
{
    return [self.circlesByID objectForKey:circleID];
}

- (TPResult)updateStableInfo:(TPPeerStableInfo *)stableInfo
               forPeerWithID:(NSString *)peerID
{
    TPPeer *peer = [self peerWithID:peerID];
    return [peer updateStableInfo:stableInfo];
}

- (TPPeerStableInfo *)createStableInfoWithDictionary:(NSDictionary *)dict
                                       policyVersion:(TPCounter)policyVersion
                                          policyHash:(NSString *)policyHash
                                       policySecrets:(nullable NSDictionary *)policySecrets
                                       forPeerWithID:(NSString *)peerID
                                               error:(NSError **)error
{
    TPPeer *peer = [self peerWithID:peerID];
    TPCounter clock = [self maxClock] + 1;
    return [TPPeerStableInfo stableInfoWithDict:dict
                                          clock:clock
                                  policyVersion:policyVersion
                                     policyHash:policyHash
                                  policySecrets:policySecrets
                                trustSigningKey:peer.permanentInfo.trustSigningKey
                                          error:error];
}

- (TPResult)updateDynamicInfo:(TPPeerDynamicInfo *)dynamicInfo
                forPeerWithID:(NSString *)peerID
{
    TPPeer *peer = [self peerWithID:peerID];
    TPResult result = [peer updateDynamicInfo:dynamicInfo];
    if (result != TPResultOk) {
        return result;
    }
    TPCircle *circle = [self.circlesByID objectForKey:dynamicInfo.circleID];
    if (nil != circle) {
        peer.circle = circle;
    } else {
        // When the corresponding circleID is eventually registered,
        // a call to registerCircle: will set peer.circle.
    }
    return result;
}

- (TPCounter)maxClock
{
    __block TPCounter maxClock = 0;
    [self.peersByID enumerateKeysAndObjectsUsingBlock:^(NSString *peerID, TPPeer *peer, BOOL *stop) {
        if (nil != peer.stableInfo) {
            maxClock = MAX(maxClock, peer.stableInfo.clock);
        }
        if (nil != peer.dynamicInfo) {
            maxClock = MAX(maxClock, peer.dynamicInfo.clock);
        }
    }];
    return maxClock;
}

- (TPCounter)maxRemovals
{
    __block TPCounter maxRemovals = 0;
    [self.peersByID enumerateKeysAndObjectsUsingBlock:^(NSString *peerID, TPPeer *peer, BOOL *stop) {
        if (nil != peer.dynamicInfo) {
            maxRemovals = MAX(maxRemovals, peer.dynamicInfo.removals);
        }
    }];
    return maxRemovals;
}

- (TPPeerDynamicInfo *)createDynamicInfoForPeerWithID:(NSString *)peerID
                                               circle:(TPCircle *)circle
                                               clique:(NSString *)clique
                                          newRemovals:(TPCounter)newRemovals
                                                error:(NSError **)error
{
    TPPeer *peer = self.peersByID[peerID];

    TPCounter clock = [self maxClock] + 1;
    TPCounter removals = [self maxRemovals] + newRemovals;
    
    return [TPPeerDynamicInfo dynamicInfoWithCircleID:circle.circleID
                                               clique:clique
                                             removals:removals
                                                clock:clock
                                      trustSigningKey:peer.permanentInfo.trustSigningKey
                                                error:error];
}

- (BOOL)canTrustCandidate:(TPPeerPermanentInfo *)candidate inEpoch:(TPCounter)epoch
{
    return candidate.epoch + 1 >= epoch;
}

- (BOOL)canIntroduceCandidate:(TPPeerPermanentInfo *)candidate
                  withSponsor:(TPPeerPermanentInfo *)sponsor
                      toEpoch:(TPCounter)epoch
                  underPolicy:(id<TPPolicy>)policy
{
    if (![self canTrustCandidate:candidate inEpoch:sponsor.epoch]) {
        return NO;
    }
    if (![self canTrustCandidate:candidate inEpoch:epoch]) {
        return NO;
    }
    
    NSString *sponsorCategory = [policy categoryForModel:sponsor.modelID];
    NSString *candidateCategory = [policy categoryForModel:candidate.modelID];
    
    return [policy trustedPeerInCategory:sponsorCategory canIntroduceCategory:candidateCategory];
}

- (nullable TPVoucher *)createVoucherForCandidate:(TPPeerPermanentInfo *)candidate
                                    withSponsorID:(NSString *)sponsorID
                                            error:(NSError **)error
{
    TPPeer *sponsor = [self peerWithID:sponsorID];
    
    NSSet<NSString*> *peerIDs = [sponsor.trustedPeerIDs setByAddingObject:candidate.peerID];
    id<TPPolicy> policy = [self policyForPeerIDs:peerIDs error:error];
    if (nil == policy) {
        return nil;
    }

    if (![self canIntroduceCandidate:candidate
                         withSponsor:sponsor.permanentInfo
                             toEpoch:sponsor.permanentInfo.epoch
                         underPolicy:policy])
    {
        if (error) {
            *error = nil;
        }
        return nil;
    }
    
    // clock is correctly zero if sponsor does not yet have dynamicInfo
    TPCounter clock = sponsor.dynamicInfo.clock;
    return [TPVoucher voucherWithBeneficiaryID:candidate.peerID
                                     sponsorID:sponsorID
                                         clock:clock
                               trustSigningKey:sponsor.permanentInfo.trustSigningKey
                                         error:error];
}

- (TPResult)registerVoucher:(TPVoucher *)voucher
{
    NSAssert(voucher, @"voucher must not be nil");
    TPPeer *sponsor = [self peerWithID:voucher.sponsorID];
    if (![sponsor.permanentInfo.trustSigningKey checkSignature:voucher.voucherInfoSig matchesData:voucher.voucherInfoPList]) {
        return TPResultSignatureMismatch;
    }
    [self.vouchers addObject:voucher];
    return TPResultOk;
}

- (NSSet<NSString*> *)calculateUnusedCircleIDs
{
    NSMutableSet<NSString *>* circleIDs = [NSMutableSet setWithArray:[self.circlesByID allKeys]];
    
    [self.peersByID enumerateKeysAndObjectsUsingBlock:^(NSString *peerID, TPPeer *peer, BOOL *stop) {
        if (nil != peer.dynamicInfo) {
            [circleIDs removeObject:peer.dynamicInfo.circleID];
        }
    }];
    return circleIDs;
}

- (nullable NSError *)considerCandidateID:(NSString *)candidateID
                              withSponsor:(TPPeer *)sponsor
                  toExpandIncludedPeerIDs:(NSMutableSet<NSString *>*)includedPeerIDs
                       andExcludedPeerIDs:(NSMutableSet<NSString *>*)excludedPeerIDs
                                 forEpoch:(TPCounter)epoch
{
    if ([includedPeerIDs containsObject:candidateID]) {
        // Already included, nothing to do.
        return nil;
    }
    if ([excludedPeerIDs containsObject:candidateID]) {
        // Denied.
        return nil;
    }
    
    TPPeer *candidate = self.peersByID[candidateID];
    if (nil == candidate) {
        return nil;
    }
    NSMutableSet<NSString*> *peerIDs = [NSMutableSet setWithSet:includedPeerIDs];
    [peerIDs minusSet:excludedPeerIDs];
    [peerIDs addObject:candidateID];
    NSError *error = nil;
    id<TPPolicy> policy = [self policyForPeerIDs:peerIDs error:&error];
    if (nil == policy) {
        return error;
    }
                                 
    if ([self canIntroduceCandidate:candidate.permanentInfo
                        withSponsor:sponsor.permanentInfo
                            toEpoch:epoch
                        underPolicy:policy])
    {
        [includedPeerIDs addObject:candidateID];
        [excludedPeerIDs unionSet:candidate.circle.excludedPeerIDs];
        
        // The accepted candidate can now be a sponsor.
        error = [self recursivelyExpandIncludedPeerIDs:includedPeerIDs
                                    andExcludedPeerIDs:excludedPeerIDs
                           withPeersTrustedBySponsorID:candidateID
                                              forEpoch:epoch];
        if (nil != error) {
            return error;
        }
    }
    return nil;
}

- (nullable NSError *)considerVouchersSponsoredByPeer:(TPPeer *)sponsor
                  toReecursivelyExpandIncludedPeerIDs:(NSMutableSet<NSString *>*)includedPeerIDs
                                   andExcludedPeerIDs:(NSMutableSet<NSString *>*)excludedPeerIDs
                                             forEpoch:(TPCounter)epoch
{
    for (TPVoucher *voucher in self.vouchers) {
        if ([voucher.sponsorID isEqualToString:sponsor.peerID]
            && voucher.clock == sponsor.dynamicInfo.clock)
        {
            NSError *error = [self considerCandidateID:voucher.beneficiaryID
                                           withSponsor:sponsor
                               toExpandIncludedPeerIDs:includedPeerIDs
                                    andExcludedPeerIDs:excludedPeerIDs
                                              forEpoch:epoch];
            if (nil != error) {
                return error;
            }
        }
    }
    return nil;
}

- (nullable NSError *)recursivelyExpandIncludedPeerIDs:(NSMutableSet<NSString *>*)includedPeerIDs
                                    andExcludedPeerIDs:(NSMutableSet<NSString *>*)excludedPeerIDs
                           withPeersTrustedBySponsorID:(NSString *)sponsorID
                                              forEpoch:(TPCounter)epoch
{
    TPPeer *sponsor = self.peersByID[sponsorID];
    if (nil == sponsor) {
        // It is possible that we might receive a voucher sponsored
        // by a peer that has not yet been registered or has been deleted,
        // or that a peer will have a circle that includes a peer that
        // has not yet been registered or has been deleted.
        return nil;
    }
    [excludedPeerIDs unionSet:sponsor.circle.excludedPeerIDs];
    for (NSString *candidateID in sponsor.circle.includedPeerIDs) {
        NSError *error = [self considerCandidateID:candidateID
                                       withSponsor:sponsor
                           toExpandIncludedPeerIDs:includedPeerIDs
                                andExcludedPeerIDs:excludedPeerIDs
                                          forEpoch:epoch];
        if (nil != error) {
            return error;
        }
    }
    return [self considerVouchersSponsoredByPeer:sponsor
             toReecursivelyExpandIncludedPeerIDs:includedPeerIDs
                              andExcludedPeerIDs:excludedPeerIDs
                                        forEpoch:epoch];
}

- (TPPeerDynamicInfo *)calculateDynamicInfoForPeerWithID:(NSString *)peerID
                                           addingPeerIDs:(NSArray<NSString*> *)addingPeerIDs
                                         removingPeerIDs:(NSArray<NSString*> *)removingPeerIDs
                                            createClique:(NSString* (^)())createClique
                                           updatedCircle:(TPCircle **)updatedCircle
                                                   error:(NSError **)error
{
    TPPeer *peer = [self peerWithID:peerID];
    TPCounter epoch = peer.permanentInfo.epoch;

    // If we have dynamicInfo then we must know the corresponding circle.
    NSAssert(nil != peer.circle || nil == peer.dynamicInfo, @"dynamicInfo without corresponding circle");
    
    // If I am excluded by myself then make no changes. I am no longer playing the game.
    // This is useful in the case where I have replaced myself with a new peer.
    if ([peer.circle.excludedPeerIDs containsObject:peerID]) {
        if (updatedCircle) {
            *updatedCircle = peer.circle;
        }
        return peer.dynamicInfo;
    }
    
    NSMutableSet<NSString *> *includedPeerIDs = [NSMutableSet setWithSet:peer.circle.includedPeerIDs];
    NSMutableSet<NSString *> *excludedPeerIDs = [NSMutableSet setWithSet:peer.circle.excludedPeerIDs];

    // I trust myself by default, though this might be overridden by excludedPeerIDs
    [includedPeerIDs addObject:peerID];
    
    // The user has explictly told us to trust addingPeerIDs.
    // This implies that the peers included in the circles of addingPeerIDs should also be trusted,
    // as long epoch tests pass. This is regardless of whether trust policy says a member of addingPeerIDs
    // can *introduce* a peer in its circle, because it isn't introducing it, the user already trusts it.
    [includedPeerIDs addObjectsFromArray:addingPeerIDs];
    for (NSString *addingPeerID in addingPeerIDs) {
        TPPeer *addingPeer = self.peersByID[addingPeerID];
        for (NSString *candidateID in addingPeer.circle.includedPeerIDs) {
            TPPeer *candidate = self.peersByID[candidateID];
            if (candidate && [self canTrustCandidate:candidate.permanentInfo inEpoch:epoch]) {
                [includedPeerIDs addObject:candidateID];
            }
        }
    }
    
    [excludedPeerIDs addObjectsFromArray:removingPeerIDs];
    [includedPeerIDs minusSet:excludedPeerIDs];

    // We iterate over a copy because the loop will mutate includedPeerIDs
    NSSet<NSString *>* sponsorIDs = [includedPeerIDs copy];

    for (NSString *sponsorID in sponsorIDs) {
        NSError *err = [self recursivelyExpandIncludedPeerIDs:includedPeerIDs
                                           andExcludedPeerIDs:excludedPeerIDs
                                  withPeersTrustedBySponsorID:sponsorID
                                                     forEpoch:epoch];
        if (nil != err) {
            if (error) {
                *error = err;
            }
            return nil;
        }
    }
    NSError *err = [self considerVouchersSponsoredByPeer:peer
                     toReecursivelyExpandIncludedPeerIDs:includedPeerIDs
                                      andExcludedPeerIDs:excludedPeerIDs
                                                forEpoch:epoch];
    if (nil != err) {
        if (error) {
            *error = err;
        }
        return nil;
    }

    [includedPeerIDs minusSet:excludedPeerIDs];
    
    NSString *clique = [self bestCliqueAmongPeerIDs:includedPeerIDs];
    if (nil == clique) {
        clique = peer.dynamicInfo.clique;
    }
    if (nil == clique && nil != createClique) {
        clique = createClique();
    }
    if (nil == clique) {
        // Either nil == createClique or createClique returned nil.
        // We would create a clique but caller has said not to.
        // Not an error, it's just what they asked for.
        if (error) {
            *error = nil;
        }
        return nil;
    }

    TPCircle *newCircle;
    if ([excludedPeerIDs containsObject:peerID]) {
        // I have been kicked out, and anybody who trusts me should now exclude me.
        newCircle = [TPCircle circleWithIncludedPeerIDs:addingPeerIDs excludedPeerIDs:@[peerID]];
    } else {
        // Drop items from excludedPeerIDs that predate epoch - 1
        NSSet<NSString*> *filteredExcluded = [excludedPeerIDs objectsPassingTest:^BOOL(NSString *exPeerID, BOOL *stop) {
            TPPeer *exPeer = self.peersByID[exPeerID];
            if (nil == exPeer) {
                return YES;
            }
            // If we could trust it then we have to keep it in the exclude list.
            return [self canTrustCandidate:exPeer.permanentInfo inEpoch:epoch];
        }];
        newCircle = [TPCircle circleWithIncludedPeerIDs:[includedPeerIDs allObjects]
                                           excludedPeerIDs:[filteredExcluded allObjects]];
    }
    if (updatedCircle) {
        *updatedCircle = newCircle;
    }
    return [self createDynamicInfoForPeerWithID:peerID
                                         circle:newCircle
                                         clique:clique
                                    newRemovals:[removingPeerIDs count]
                                          error:error];
}

- (NSString *)bestCliqueAmongPeerIDs:(NSSet<NSString*>*)peerIDs
{
    // The "best" clique is considered the one that is last in lexical ordering.
    NSString *bestClique = nil;
    for (NSString *peerID in peerIDs) {
        NSString *clique = self.peersByID[peerID].dynamicInfo.clique;
        if (clique) {
            if (bestClique && NSOrderedAscending != [bestClique compare:clique]) {
                continue;
            }
            bestClique = clique;
        }
    }
    return bestClique;
}

- (TPCircle *)advancePeerWithID:(NSString *)peerID
                  addingPeerIDs:(NSArray<NSString*> *)addingPeerIDs
                removingPeerIDs:(NSArray<NSString*> *)removingPeerIDs
                   createClique:(NSString* (^)())createClique
{
    TPCircle *circle = nil;
    TPPeerDynamicInfo *dyn;
    dyn = [self calculateDynamicInfoForPeerWithID:peerID
                                    addingPeerIDs:addingPeerIDs
                                  removingPeerIDs:removingPeerIDs
                                     createClique:createClique
                                    updatedCircle:&circle
                                            error:NULL];
    if (dyn) {
        [self registerCircle:circle];
        [self updateDynamicInfo:dyn forPeerWithID:peerID];
        return circle;
    } else {
        return nil;
    }
}

NSString *TPErrorDomain = @"com.apple.security.trustedpeers";

enum {
    TPErrorUnknownPolicyVersion = 1,
    TPErrorPolicyHashMismatch = 2,
    TPErrorMissingStableInfo = 3,
};


- (nullable id<TPPolicy>)policyForPeerIDs:(NSSet<NSString*> *)peerIDs
                                    error:(NSError **)error
{
    NSAssert(peerIDs.count > 0, @"policyForPeerIDs does not accept empty set");

    TPPolicyDocument *newestPolicyDoc = nil;
    
    // This will become the union of policySecrets across the members of peerIDs
    NSMutableDictionary<NSString*,NSData*> *secrets = [NSMutableDictionary dictionary];
    
    for (NSString *peerID in peerIDs) {
        TPPeerStableInfo *stableInfo = [self peerWithID:peerID].stableInfo;
        if (nil == stableInfo) {
            // Allowing missing stableInfo here might be useful if we are writing a voucher
            // for a peer for which we got permanentInfo over some channel that does not
            // also convey stableInfo.
            continue;
        }
        for (NSString *name in stableInfo.policySecrets) {
            secrets[name] = stableInfo.policySecrets[name];
        }
        if (newestPolicyDoc && newestPolicyDoc.policyVersion > stableInfo.policyVersion) {
            continue;
        }
        TPPolicyDocument *policyDoc = self.policiesByVersion[@(stableInfo.policyVersion)];
        if (nil == policyDoc) {
            if (error) {
                *error = [NSError errorWithDomain:TPErrorDomain
                                             code:TPErrorUnknownPolicyVersion
                                         userInfo:@{
                                                    @"peerID": peerID,
                                                    @"policyVersion": @(stableInfo.policyVersion)
                                                    }];
            }
            return nil;
        }
        if (![policyDoc.policyHash isEqualToString:stableInfo.policyHash]) {
            if (error) {
                *error = [NSError errorWithDomain:TPErrorDomain
                                             code:TPErrorPolicyHashMismatch
                                         userInfo:@{
                                                    @"peerID": peerID,
                                                    @"policyVersion": @(stableInfo.policyVersion),
                                                    @"policyDocHash": policyDoc.policyHash,
                                                    @"peerExpectsHash": stableInfo.policyHash
                                                    }];
            }
            return nil;
        }
        newestPolicyDoc = policyDoc;
    }
    if (nil == newestPolicyDoc) {
        // Can happen if no members of peerIDs have stableInfo
        if (error) {
            *error = [NSError errorWithDomain:TPErrorDomain
                                         code:TPErrorMissingStableInfo
                                     userInfo:nil];
        }
        return nil;
    }
    return [newestPolicyDoc policyWithSecrets:secrets decrypter:self.decrypter error:error];
}

- (NSSet<NSString*> *)getPeerIDsTrustedByPeerWithID:(NSString *)peerID
                                       toAccessView:(NSString *)view
                                              error:(NSError **)error
{
    TPCircle *circle = [self peerWithID:peerID].circle;
    NSMutableSet<NSString*> *peerIDs = [NSMutableSet set];

    id<TPPolicy> policy = [self policyForPeerIDs:circle.includedPeerIDs error:error];

    for (NSString *candidateID in circle.includedPeerIDs) {
        TPPeer *candidate = self.peersByID[candidateID];
        if (candidate != nil) {
            NSString *category = [policy categoryForModel:candidate.permanentInfo.modelID];
            if ([policy peerInCategory:category canAccessView:view]) {
                [peerIDs addObject:candidateID];
            }
        }
    }
    return peerIDs;
}

- (NSDictionary<NSString*,NSNumber*> *)vectorClock
{
    NSMutableDictionary<NSString*,NSNumber*> *dict = [NSMutableDictionary dictionary];
    
    [self.peersByID enumerateKeysAndObjectsUsingBlock:^(NSString *peerID, TPPeer *peer, BOOL *stop) {
        if (peer.stableInfo || peer.dynamicInfo) {
            TPCounter clock = MAX(peer.stableInfo.clock, peer.dynamicInfo.clock);
            dict[peerID] = @(clock);
        }
    }];
    return dict;
}

@end
