
#if OCTAGON

#import <SecurityFoundation/SecurityFoundation.h>
#import <SecurityFoundation/SFIdentity.h>

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/OctagonCKKSPeerAdapter.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSListenerCollection.h"

#import "keychain/ot/ObjCImprovements.h"
#import "utilities/debugging.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

@interface OctagonSelfPeer ()
@property SFIdentity* encryptionIdentity;
@property SFIdentity* signingIdentity;
@end

@implementation OctagonSelfPeer
@synthesize peerID = _peerID;

- (instancetype)initWithPeerID:(NSString*)peerID
               signingIdentity:(SFIdentity*)signingIdentity
            encryptionIdentity:(SFIdentity*)encryptionIdentity
{
    if((self = [super init])) {
        _peerID = peerID;
        _signingIdentity = signingIdentity;
        _encryptionIdentity = encryptionIdentity;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<OctagonSelfPeer: %@>", self.peerID];
}

- (SFECPublicKey*)publicEncryptionKey
{
    return self.encryptionIdentity.publicKey;
}

- (SFECPublicKey*)publicSigningKey
{
    return self.signingIdentity.publicKey;
}

- (SFECKeyPair*)encryptionKey
{
    return self.encryptionIdentity.keyPair;
}

- (SFECKeyPair*)signingKey
{
    return self.signingIdentity.keyPair;
}

- (bool)matchesPeer:(nonnull id<CKKSPeer>)peer {
    NSString* otherPeerID = peer.peerID;

    if(self.peerID == nil && otherPeerID == nil) {
        return true;
    }

    return [self.peerID isEqualToString:otherPeerID];
}

@end

@interface OctagonCKKSPeerAdapter ()
@property CKKSListenerCollection* peerChangeListeners;
@end

@implementation OctagonCKKSPeerAdapter
@synthesize essential = _essential;
@synthesize providerID = _providerID;


- (instancetype)initWithPeerID:(NSString*)peerID
                  specificUser:(TPSpecificUser*)specificUser
                personaAdapter:(id<OTPersonaAdapter>)personaAdapter
                 cuttlefishXPC:(CuttlefishXPCWrapper*)cuttlefishXPCWrapper
{
    if((self = [super init])) {
        _providerID = [NSString stringWithFormat:@"[OctagonCKKSPeerAdapter:%@]", peerID];
        _peerID = peerID;

        _specificUser = specificUser;
        _personaAdapter = personaAdapter;
        _cuttlefishXPCWrapper = cuttlefishXPCWrapper;

        _peerChangeListeners = [[CKKSListenerCollection alloc] initWithName:@"ckks-sos"];

        // Octagon is king.
        _essential = YES;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<OctagonCKKSPeerAdapter: %@ e:%d>", self.peerID, self.essential];
}

- (SFIdentity*)fetchIdentity:(NSString*)identifier error:(NSError *__autoreleasing  _Nullable * _Nullable)error
{
    __block SFIdentity* identity = nil;
    [self.personaAdapter performBlockWithPersonaIdentifier:self.specificUser.personaUniqueString block:^{
        SFKeychainManager* keychainManager = [SFKeychainManager defaultOverCommitManager];
        
        NSError* localError = nil;
        
        SFKeychainIdentityFetchResult* result = [keychainManager identityForIdentifier:identifier];
        
        switch(result.resultType) {
            case SFKeychainFetchResultTypeError:
            case SFKeychainFetchResultTypeNeedsAuthentication:
                secnotice("octagon-ckks", "Unable to fetch identity '%@' from keychain: %@", identifier, result.error);
                localError = result.error;
                break;
                
            case SFKeychainFetchResultTypeValueAvailable:
                identity = result.value;
                break;
        }
        
        if(error && localError) {
            *error = localError;
        }
    }];
    return identity;
}

- (CKKSSelves * _Nullable)fetchSelfPeers:(NSError *__autoreleasing  _Nullable * _Nullable)error
{
    __block CKKSSelves* selves = nil;
    [self.personaAdapter performBlockWithPersonaIdentifier:self.specificUser.personaUniqueString block:^{
        if(self.peerID) {
            // Shameless duplication of swift code in TPH. Thanks, perf team!
            NSError* keychainError = nil;

            SFIdentity* signingIdentity = [self fetchIdentity:[NSString stringWithFormat:@"signing-key %@", self.peerID] error:&keychainError];
            if(!signingIdentity || keychainError) {
                if(error) {
                    // TODO: ensure error exists
                    *error = keychainError;
                }
                return;
            }

            SFIdentity* encryptionIdentity = [self fetchIdentity:[NSString stringWithFormat:@"encryption-key %@", self.peerID] error:&keychainError];
            if(!encryptionIdentity || keychainError) {
                if(error) {
                    // TODO: ensure error exists
                    *error = keychainError;
                }
                return;
            }

            OctagonSelfPeer* selfPeer = [[OctagonSelfPeer alloc] initWithPeerID:self.peerID
                                                                signingIdentity:signingIdentity
                                                             encryptionIdentity:encryptionIdentity];

            selves = [[CKKSSelves alloc] initWithCurrent:selfPeer allSelves:[NSSet set]];
            return;
        }

        secnotice("octagon-ckks", "No peer ID; therefore no identity");
        if(error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorNoIdentity
                                  description:@"no peer ID present"];
        }
    }];
    return selves;
}

- (NSSet<id<CKKSRemotePeerProtocol>> * _Nullable)fetchTrustedPeers:(NSError *__autoreleasing  _Nullable * _Nullable)error
{
    // TODO: make this memoized somehow, somewhere
    __block NSMutableSet<id<CKKSRemotePeerProtocol>> * peers = nil;
    [self.personaAdapter performBlockWithPersonaIdentifier:self.specificUser.personaUniqueString block:^{
        __block NSError* localerror;

        WEAKIFY(self);
        [self.cuttlefishXPCWrapper fetchTrustStateWithSpecificUser:self.specificUser
                                                             reply:^(TrustedPeersHelperPeerState * _Nullable selfPeerState,
                                                                     NSArray<TrustedPeersHelperPeer *> * _Nullable trustedPeers,
                                                                     NSError * _Nullable operror) {
            STRONGIFY(self);
            if(operror) {
                secnotice("octagon", "Unable to fetch trusted peers for (%@): %@", self.specificUser, operror);
                localerror = operror;

            } else {
                peers = [NSMutableSet set];

                // Turn these peers into CKKSPeers
                for(TrustedPeersHelperPeer* peer in trustedPeers) {
                    SFECPublicKey* signingKey = [SFECPublicKey keyWithSubjectPublicKeyInfo:peer.signingSPKI];
                    SFECPublicKey* encryptionKey = [SFECPublicKey keyWithSubjectPublicKeyInfo:peer.encryptionSPKI];

                    CKKSActualPeer* ckkspeer = [[CKKSActualPeer alloc] initWithPeerID:peer.peerID
                                                                  encryptionPublicKey:encryptionKey
                                                                     signingPublicKey:signingKey
                                                                             viewList:peer.viewList];
                    [peers addObject:ckkspeer];
                }
            }
        }];

        if(error && localerror) {
            *error = localerror;
        }
    }];
    return peers;
}

- (void)registerForPeerChangeUpdates:(nonnull id<CKKSPeerUpdateListener>)listener {
    [self.personaAdapter performBlockWithPersonaIdentifier:self.specificUser.personaUniqueString block:^{
        [self.peerChangeListeners registerListener:listener];
    }];
}

- (void)sendSelfPeerChangedUpdate {
    [self.personaAdapter performBlockWithPersonaIdentifier:self.specificUser.personaUniqueString block:^{
        [self.peerChangeListeners iterateListeners: ^(id<CKKSPeerUpdateListener> listener) {
            [listener selfPeerChanged: self];
        }];
    }];
}

- (void)sendTrustedPeerSetChangedUpdate {
    [self.personaAdapter performBlockWithPersonaIdentifier:self.specificUser.personaUniqueString block:^{
        [self.peerChangeListeners iterateListeners: ^(id<CKKSPeerUpdateListener> listener) {
            [listener trustedPeerSetChanged: self];
        }];
    }];
}

- (nonnull CKKSPeerProviderState *)currentState {
    __block CKKSPeerProviderState *state =  nil;
    [self.personaAdapter performBlockWithPersonaIdentifier:self.specificUser.personaUniqueString block:^{
        state =  [CKKSPeerProviderState createFromProvider:self];
    }];
    return state;
}


@end

#endif // OCTAGON
