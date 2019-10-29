#if OCTAGON
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSPeerProvider.h"
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
@end

#endif

