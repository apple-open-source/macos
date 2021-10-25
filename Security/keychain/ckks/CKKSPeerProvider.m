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
    @autoreleasepool {
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
                                   underlying:self.currentSelfPeersError];
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
            @autoreleasepool {
                NSError* possibleShareError = nil;
                ckksnotice("ckksshare", proposedTLK, "Checking possible TLK share %@ as %@", possibleShare, selfPeer);

                CKKSKeychainBackedKey* possibleKey = [possibleShare recoverTLK:selfPeer
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

