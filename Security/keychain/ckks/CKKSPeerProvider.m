#if OCTAGON
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSPeerProvider.h"
#import "keychain/ckks/CKKSTLKShareRecord.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

@implementation CKKSPeerProviderState
- (instancetype)initWithPeerProviderID:(NSString*)providerID
                             essential:(BOOL)essential
                             selfPeers:(CKKSSelves* _Nullable)selfPeers
                        selfPeersError:(NSError* _Nullable)selfPeersError
                          trustedPeers:(NSSet<id<CKKSRemotePeerProtocol>>* _Nullable)currentTrustedPeers
                     trustedPeersError:(NSError* _Nullable)trustedPeersError
{
    if((self = [super init])) {
        _peerProviderID = providerID;
        _essential = essential;
        _currentSelfPeers = selfPeers;
        _currentSelfPeersError = selfPeersError;
        _currentTrustedPeers = currentTrustedPeers;
        _currentTrustedPeersError = trustedPeersError;

        if(_currentTrustedPeers) {
            NSMutableSet<NSString*>* trustedPeerIDs = [NSMutableSet set];
            for(id<CKKSPeer> peer in _currentTrustedPeers) {
                [trustedPeerIDs addObject:peer.peerID];
            }
            _currentTrustedPeerIDs = trustedPeerIDs;
        }
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<CKKSPeerProviderState(%@): %@%@ %@%@>",
            self.peerProviderID,
            self.currentSelfPeers,
            self.currentSelfPeersError ?: @"",
            self.currentTrustedPeers,
            self.currentTrustedPeersError ?: @""];
}

+ (CKKSPeerProviderState*)noPeersState:(id<CKKSPeerProvider>)provider
{
    return [[CKKSPeerProviderState alloc] initWithPeerProviderID:provider.providerID
                                                       essential:provider.essential
                                                       selfPeers:nil
                                             selfPeersError:[NSError errorWithDomain:CKKSErrorDomain
                                                                                code:CKKSNoPeersAvailable
                                                                         description:@"No current self peer available"]
                                               trustedPeers:nil
                                          trustedPeersError:[NSError errorWithDomain:CKKSErrorDomain
                                                                                code:CKKSNoPeersAvailable
                                                                         description:@"No current trusted peers available"]];
}


+ (CKKSPeerProviderState*)createFromProvider:(id<CKKSPeerProvider>)provider
{
    NSError* selfPeersError = nil;
    CKKSSelves* currentSelfPeers = [provider fetchSelfPeers:&selfPeersError];

    NSError* trustedPeersError = nil;
    NSSet<id<CKKSRemotePeerProtocol>>* currentTrustedPeers = [provider fetchTrustedPeers:&trustedPeersError];

    return [[CKKSPeerProviderState alloc] initWithPeerProviderID:provider.providerID
                                                       essential:provider.essential
                                                       selfPeers:currentSelfPeers
                                                  selfPeersError:selfPeersError
                                                    trustedPeers:currentTrustedPeers
                                               trustedPeersError:trustedPeersError];
}

// For this key, who doesn't yet have a valid CKKSTLKShare for it?
// Note that we really want a record sharing the TLK to ourselves, so this function might return
// a non-empty set even if all peers have the TLK: it wants us to make a record for ourself.
- (NSSet<id<CKKSPeer>>* _Nullable)findPeersMissingTLKSharesFor:(CKKSCurrentKeySet*)keyset
                                                         error:(NSError**)error
{
    if(self.currentTrustedPeersError) {
        ckkserror("ckksshare", keyset.tlk, "Couldn't find missing shares because trusted peers aren't available: %@", self.currentTrustedPeersError);
        if(error) {
            *error = self.currentTrustedPeersError;
        }
        return [NSSet set];
    }
    if(self.currentSelfPeersError) {
        ckkserror("ckksshare", keyset.tlk, "Couldn't find missing shares because self peers aren't available: %@", self.currentSelfPeersError);
        if(error) {
            *error = self.currentSelfPeersError;
        }
        return [NSSet set];
    }

    NSArray<CKKSTLKShareRecord*>* tlkShares = [keyset.tlkShares arrayByAddingObjectsFromArray:keyset.pendingTLKShares ?: @[]];

    NSMutableSet<id<CKKSPeer>>* peersMissingShares = [NSMutableSet set];

    // Ensure that the 'self peer' is one of the current trusted peers. Otherwise, any TLKShare we create
    // won't be considered trusted the next time through...
    if(![self.currentTrustedPeerIDs containsObject:self.currentSelfPeers.currentSelf.peerID]) {
        ckkserror("ckksshare", keyset.tlk, "current self peer (%@) is not in the set of trusted peers: %@",
                  self.currentSelfPeers.currentSelf.peerID,
                  self.currentTrustedPeerIDs);

        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSLackingTrust
                                  description:[NSString stringWithFormat:@"current self peer (%@) is not in the set of trusted peers",
                                               self.currentSelfPeers.currentSelf.peerID]];
        }

        return nil;
    }

    for(id<CKKSRemotePeerProtocol> peer in self.currentTrustedPeers) {
        if(![peer shouldHaveView:keyset.tlk.zoneName]) {
            ckkserror("ckksshare", keyset.tlk.keycore.zoneID, "Peer (%@) is not supposed to have view, skipping", peer);
            continue;
        }

        // Determine if we think this peer has enough things shared to them
        bool alreadyShared = false;
        for(CKKSTLKShareRecord* existingShare in tlkShares) {
            // Ensure this share is to this peer...
            if(![existingShare.share.receiverPeerID isEqualToString:peer.peerID]) {
                continue;
            }

            // If an SOS Peer sent this share, is its signature still valid? Or did the signing key change?
            if([existingShare.senderPeerID hasPrefix:CKKSSOSPeerPrefix]) {
                NSError* signatureError = nil;
                if(![existingShare signatureVerifiesWithPeerSet:self.currentTrustedPeers error:&signatureError]) {
                    ckksnotice("ckksshare", keyset.tlk, "Existing TLKShare's signature doesn't verify with current peer set: %@ %@", signatureError, existingShare);
                    continue;
                }
            }

            if([existingShare.tlkUUID isEqualToString:keyset.tlk.uuid] && [self.currentTrustedPeerIDs containsObject:existingShare.senderPeerID]) {
                // Was this shared to us?
                if([peer.peerID isEqualToString:self.currentSelfPeers.currentSelf.peerID]) {
                    // We only count this as 'found' if we did the sharing and it's to our current keys
                    NSData* currentKey = self.currentSelfPeers.currentSelf.publicEncryptionKey.keyData;

                    if([existingShare.senderPeerID isEqualToString:self.currentSelfPeers.currentSelf.peerID] &&
                       [existingShare.share.receiverPublicEncryptionKeySPKI isEqual:currentKey]) {
                        ckksnotice("ckksshare", keyset.tlk, "Local peer %@ is shared %@ via self: %@", peer, keyset.tlk, existingShare);
                        alreadyShared = true;
                        break;
                    } else {
                        ckksnotice("ckksshare", keyset.tlk, "Local peer %@ is shared %@ via trusted %@, but that's not good enough", peer, keyset.tlk, existingShare);
                    }

                } else {
                    // Was this shared to the remote peer's current keys?
                    NSData* currentKeySPKI = peer.publicEncryptionKey.keyData;

                    if([existingShare.share.receiverPublicEncryptionKeySPKI isEqual:currentKeySPKI]) {
                        // Some other peer has a trusted share. Cool!
                        ckksnotice("ckksshare", keyset.tlk, "Peer %@ is shared %@ via trusted %@", peer, keyset.tlk, existingShare);
                        alreadyShared = true;
                        break;
                    } else {
                        ckksnotice("ckksshare", keyset.tlk, "Peer %@ has a share for %@, but to old keys: %@", peer, keyset.tlk, existingShare);
                    }
                }
            }
        }

        if(!alreadyShared) {
            // Add this peer to our set, if it has an encryption key to receive the share
            if(peer.publicEncryptionKey) {
                [peersMissingShares addObject:peer];
            }
        }
    }

    if(peersMissingShares.count > 0u) {
        // Log each and every one of the things
        ckksnotice("ckksshare", keyset.tlk, "Missing TLK shares for %lu peers: %@", (unsigned long)peersMissingShares.count, peersMissingShares);
        ckksnotice("ckksshare", keyset.tlk, "Self peers are (%@) %@", self.currentSelfPeersError ?: @"no error", self.currentSelfPeers);
        ckksnotice("ckksshare", keyset.tlk, "Trusted peers are (%@) %@", self.currentTrustedPeersError ?: @"no error", self.currentTrustedPeers);
    }

    return peersMissingShares;
}

- (BOOL)unwrapKey:(CKKSKey*)proposedTLK
       fromShares:(NSArray<CKKSTLKShareRecord*>*)tlkShares
            error:(NSError**)error
{
    if(!self.currentSelfPeers.currentSelf || self.currentSelfPeersError) {
        ckkserror("ckksshare", proposedTLK, "Don't have self peers for %@: %@", self.peerProviderID, self.currentSelfPeersError);
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNoEncryptionKey
                                  description:@"Key unwrap failed"
                                   underlying:self.currentSelfPeersError];;
        }
        return NO;
    }

    if(!self.currentTrustedPeers || self.currentTrustedPeersError) {
        ckkserror("ckksshare", proposedTLK, "Don't have trusted peers: %@", self.currentTrustedPeersError);
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNoPeersAvailable
                                  description:@"No trusted peers"
                               underlying:self.currentTrustedPeersError];
        }
        return NO;
    }

    NSError* lastShareError = nil;

    for(id<CKKSSelfPeer> selfPeer in self.currentSelfPeers.allSelves) {
        NSMutableArray<CKKSTLKShareRecord*>* possibleShares = [NSMutableArray array];

        for(CKKSTLKShareRecord* share in tlkShares) {
            if([share.share.receiverPeerID isEqualToString:selfPeer.peerID]) {
                [possibleShares addObject:share];
            }
        }

        if(possibleShares.count == 0) {
            ckksnotice("ckksshare", proposedTLK, "No CKKSTLKShares to %@ for %@", selfPeer, proposedTLK);
            continue;
        }

        for(CKKSTLKShareRecord* possibleShare in possibleShares) {
            NSError* possibleShareError = nil;
            ckksnotice("ckksshare", proposedTLK, "Checking possible TLK share %@ as %@", possibleShare, selfPeer);

            CKKSKey* possibleKey = [possibleShare recoverTLK:selfPeer
                                                trustedPeers:self.currentTrustedPeers
                                                       error:&possibleShareError];

            if(!possibleKey || possibleShareError) {
                ckkserror("ckksshare", proposedTLK, "Unable to unwrap TLKShare(%@) as %@: %@",
                          possibleShare, selfPeer, possibleShareError);
                ckkserror("ckksshare", proposedTLK, "Current trust set: %@", self.currentTrustedPeers);
                lastShareError = possibleShareError;
                continue;
            }

            bool result = [proposedTLK trySelfWrappedKeyCandidate:possibleKey.aessivkey error:&possibleShareError];
            if(!result || possibleShareError) {
                ckkserror("ckksshare", proposedTLK, "Unwrapped TLKShare(%@) does not unwrap proposed TLK(%@) as %@: %@",
                          possibleShare, proposedTLK, self.currentSelfPeers.currentSelf, possibleShareError);
                lastShareError = possibleShareError;
                continue;
            }

            ckksnotice("ckksshare", proposedTLK, "TLKShare(%@) unlocked TLK(%@) as %@",
                       possibleShare, proposedTLK, selfPeer);

            return YES;
        }
    }

    if(error) {
        *error = [NSError errorWithDomain:CKKSErrorDomain
                                     code:CKKSNoTrustedTLKShares
                              description:[NSString stringWithFormat:@"No trusted TLKShares for %@", proposedTLK]
                               underlying:lastShareError];
    }

    return NO;
}

@end

#endif

