
#if OCTAGON

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKSCheckKeyHierarchyOperation.h"
#import "keychain/ckks/CKKSMirrorEntry.h"
#import "keychain/ot/OTDefines.h"
#import "utilities/SecTrace.h"

@implementation CKKSCheckKeyHierarchyOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if(self = [super init]) {
        _deps = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)main {
    NSArray<CKKSPeerProviderState*>* currentTrustStates = [self.deps currentTrustStates];

    __block CKKSCurrentKeySet* set = nil;

    [self.deps.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
        set = [CKKSCurrentKeySet loadForZone:self.deps.zoneID];
    }];

    // Drop off the sql queue: we can do the rest of this function with what we've already loaded

    if(set.error && !([set.error.domain isEqual: @"securityd"] && set.error.code == errSecItemNotFound)) {
        ckkserror("ckkskey", self.deps.zoneID, "Error examining existing key hierarchy: %@", set.error);
    }

    if(!set.currentTLKPointer && !set.currentClassAPointer && !set.currentClassCPointer) {
        ckkserror("ckkskey", self.deps.zoneID, "Error examining existing key hierarchy (missing all CKPs, likely no hierarchy exists): %@", set);
        self.nextState = SecCKKSZoneKeyStateWaitForTLKCreation;
        return;
    }

    // Check keyset
    if(!set.tlk || !set.classA || !set.classC) {
        ckkserror("ckkskey", self.deps.zoneID, "Error examining existing key hierarchy (missing at least one key): %@", set);
        self.error = set.error;
        self.nextState = SecCKKSZoneKeyStateUnhealthy;
        return;
    }

    NSError* localerror = nil;
    bool probablyOkIfUnlocked = false;

    // keychain being locked is not a fatal error here
    [set.tlk loadKeyMaterialFromKeychain:&localerror];
    if(localerror && ![self.deps.lockStateTracker isLockedError:localerror]) {
        ckkserror("ckkskey", self.deps.zoneID, "Error loading TLK(%@): %@", set.tlk, localerror);
        self.error = localerror;
        self.nextState = SecCKKSZoneKeyStateUnhealthy;
        return;
    } else if(localerror) {
        ckkserror("ckkskey", self.deps.zoneID, "Soft error loading TLK(%@), maybe locked: %@", set.tlk, localerror);
        probablyOkIfUnlocked = true;
    }
    localerror = nil;

    // keychain being locked is not a fatal error here
    [set.classA loadKeyMaterialFromKeychain:&localerror];
    if(localerror && ![self.deps.lockStateTracker isLockedError:localerror]) {
        ckkserror("ckkskey", self.deps.zoneID, "Error loading classA key(%@): %@", set.classA, localerror);
        self.error = localerror;
        self.nextState = SecCKKSZoneKeyStateUnhealthy;
        return;
    } else if(localerror) {
        ckkserror("ckkskey", self.deps.zoneID, "Soft error loading classA key(%@), maybe locked: %@", set.classA, localerror);
        probablyOkIfUnlocked = true;
    }
    localerror = nil;

    // keychain being locked is a fatal error here, since this is class C
    [set.classC loadKeyMaterialFromKeychain:&localerror];
    if(localerror) {
        ckkserror("ckkskey", self.deps.zoneID, "Error loading classC(%@): %@", set.classC, localerror);
        self.error = localerror;
        self.nextState = SecCKKSZoneKeyStateUnhealthy;
        return;
    }

    // Check that the classA and classC keys point to the current TLK
    if(![set.classA.parentKeyUUID isEqualToString: set.tlk.uuid]) {
        localerror = [NSError errorWithDomain:CKKSServerExtensionErrorDomain
                                         code:CKKSServerUnexpectedSyncKeyInChain
                                     userInfo:@{
                                                NSLocalizedDescriptionKey: @"Current class A key does not wrap to current TLK",
                                               }];
        ckkserror("ckkskey", self.deps.zoneID, "Key hierarchy unhealthy: %@", localerror);
        self.error = localerror;
        self.nextState = SecCKKSZoneKeyStateUnhealthy;
        return;
    }
    if(![set.classC.parentKeyUUID isEqualToString: set.tlk.uuid]) {
        localerror = [NSError errorWithDomain:CKKSServerExtensionErrorDomain
                                         code:CKKSServerUnexpectedSyncKeyInChain
                                     userInfo:@{
                                                NSLocalizedDescriptionKey: @"Current class C key does not wrap to current TLK",
                                               }];
        ckkserror("ckkskey", self.deps.zoneID, "Key hierarchy unhealthy: %@", localerror);
        self.error = localerror;
        self.nextState = SecCKKSZoneKeyStateUnhealthy;
        return;
    }

    // Now that we're pretty sure we have the keys, are they shared appropriately?
    // We need trust in order to proceed here
    if(currentTrustStates.count == 0u) {
        ckkserror("ckkskey", self.deps.zoneID, "Can't check TLKShares due to missing trust states");
        [self.deps provideKeySet:set];
        self.nextState = SecCKKSZoneKeyStateLoseTrust;
        return;
    }

    // If we've reached this point, we have a workable keyset. Let's provide it to all waiters.
    [self.deps provideKeySet:set];

    if(probablyOkIfUnlocked) {
        ckkserror("ckkskey", self.deps.zoneID, "Can't check TLKShares due to lock state");
        [self.deps provideKeySet:set];
        self.nextState = SecCKKSZoneKeyStateReadyPendingUnlock;
        return;
    }

    // Check that every trusted peer has at least one TLK share
    // If any trust state check works, don't error out
    bool anyTrustStateSucceeded = false;
    for(CKKSPeerProviderState* trustState in currentTrustStates) {
        NSSet<id<CKKSPeer>>* missingShares = [trustState findPeersMissingTLKSharesFor:set
                                                                                error:&localerror];

        if(localerror && [self.deps.lockStateTracker isLockedError:localerror]) {
            ckkserror("ckkskey", self.deps.zoneID, "Couldn't find missing TLK shares due to lock state: %@", localerror);
            continue;

        } else if(([localerror.domain isEqualToString:TrustedPeersHelperErrorDomain] && localerror.code == TrustedPeersHelperErrorNoPreparedIdentity) ||
                  ([localerror.domain isEqualToString:CKKSErrorDomain] && localerror.code == CKKSLackingTrust) ||
                  ([localerror.domain isEqualToString:CKKSErrorDomain] && localerror.code == CKKSNoPeersAvailable)) {
            ckkserror("ckkskey", self.deps.zoneID, "Couldn't find missing TLK shares due some trust issue: %@", localerror);

            if(trustState.essential) {
                ckkserror("ckkskey", self.deps.zoneID, "Trust state is considered essential; entering waitfortrust: %@", trustState);

                // Octagon can reinform us when it thinks we should start again
                self.nextState = SecCKKSZoneKeyStateLoseTrust;

                return;
            } else {
                ckkserror("ckkskey", self.deps.zoneID, "Peer provider is considered nonessential; ignoring error: %@", trustState);
                continue;
            }

        } else if(localerror) {
            ckkserror("ckkskey", self.deps.zoneID, "Error finding missing TLK shares: %@", localerror);
            continue;
        }

        if(!missingShares || missingShares.count != 0u) {
            ckksnotice("ckksshare", self.deps.zoneID, "TLK (%@) is not shared correctly for trust state %@, but we believe AKS is locked", set.tlk, trustState.peerProviderID);

            self.error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSMissingTLKShare
                                      description:[NSString stringWithFormat:@"Missing shares for %lu peers", (unsigned long)missingShares.count]];
            self.nextState = SecCKKSZoneKeyStateHealTLKShares;
            return;
        } else {
            ckksnotice("ckksshare", self.deps.zoneID, "TLK (%@) is shared correctly for trust state %@", set.tlk, trustState.peerProviderID);
        }

        anyTrustStateSucceeded |= true;
    }

    if(!anyTrustStateSucceeded) {
        self.error = localerror;
        self.nextState = SecCKKSZoneKeyStateError;

        return;
    }

    // Got to the bottom? Cool! All keys are present and accounted for.
    self.nextState = SecCKKSZoneKeyStateReady;
}

@end

#endif // OCTAGON
