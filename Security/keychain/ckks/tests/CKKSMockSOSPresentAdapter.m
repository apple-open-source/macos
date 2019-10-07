#import "keychain/ckks/CKKSListenerCollection.h"
#import "keychain/ckks/tests/CKKSMockSOSPresentAdapter.h"

@interface CKKSMockSOSPresentAdapter()
@property CKKSListenerCollection* peerChangeListeners;
@end

@implementation CKKSMockSOSPresentAdapter
@synthesize essential = _essential;
@synthesize sosEnabled = _sosEnabled;

- (instancetype)initWithSelfPeer:(CKKSSOSSelfPeer*)selfPeer
                    trustedPeers:(NSSet<CKKSSOSPeer*>*)trustedPeers
                       essential:(BOOL)essential
{
    if((self = [super init])) {
        _sosEnabled = true;
        _essential = essential;

        _circleStatus = kSOSCCInCircle;

        _excludeSelfPeerFromTrustSet = false;

        _peerChangeListeners = [[CKKSListenerCollection alloc] initWithName:@"ckks-mock-sos"];

        _selfPeer = selfPeer;
        _trustedPeers = [trustedPeers mutableCopy];
    }
    return self;
}

- (NSString*)providerID
{
    return [NSString stringWithFormat:@"[CKKSMockSOSPresentAdapter: %@]", self.selfPeer.peerID];
}

- (SOSCCStatus)circleStatus:(NSError * _Nullable __autoreleasing * _Nullable)error
{
    if(!self.sosEnabled || self.circleStatus == kSOSCCError) {
        if(error && self.circleStatus == kSOSCCError) {
            *error = [NSError errorWithDomain:(__bridge NSString*)kSOSErrorDomain code:self.circleStatus userInfo:nil];
        }
        return kSOSCCError;
    }

    return self.circleStatus;
}

// I currently don't understand when SOS returns a self or not. I've seen it return a self while not in kSOSCCInCircle,
// which seems wrong. So, always return a self, unless we're in an obvious error state.
- (id<CKKSSelfPeer> _Nullable)currentSOSSelf:(NSError * _Nullable __autoreleasing * _Nullable)error
{
    if(self.selfPeerError) {
        if(error) {
            *error = self.selfPeerError;
        }
        return nil;
    }

    if(self.sosEnabled && self.circleStatus == kSOSCCInCircle) {
        return self.selfPeer;
    } else {
        if(error) {
            *error = [NSError errorWithDomain:(__bridge NSString*)kSOSErrorDomain code:self.circleStatus userInfo:nil];
        }
        return nil;
    }
}

- (CKKSSelves * _Nullable)fetchSelfPeers:(NSError *__autoreleasing  _Nullable * _Nullable)error
{
    id<CKKSSelfPeer> peer = [self currentSOSSelf:error];
    if(!peer) {
        return nil;
    }

    return [[CKKSSelves alloc] initWithCurrent:peer allSelves:nil];
}

- (NSSet<id<CKKSRemotePeerProtocol>> * _Nullable)fetchTrustedPeers:(NSError * _Nullable __autoreleasing * _Nullable)error
{
    if(self.trustedPeersError) {
        if(*error) {
            *error = self.trustedPeersError;
        }
        return nil;
    }

    // TODO: I'm actually not entirely sure what SOS does if it's not in circle?
    if(self.sosEnabled && self.circleStatus == kSOSCCInCircle) {
        if(self.excludeSelfPeerFromTrustSet) {
            return self.trustedPeers;
        } else {
            return [self allPeers];
        }
    } else {
        if(error) {
            *error = [NSError errorWithDomain:(__bridge NSString*)kSOSErrorDomain code:kSOSCCNotInCircle userInfo:nil];
        }
        return nil;
    }
}

- (void)updateOctagonKeySetWithAccount:(nonnull id<CKKSSelfPeer>)currentSelfPeer error:(NSError *__autoreleasing  _Nullable * _Nullable)error {
    return;
}

- (void)registerForPeerChangeUpdates:(nonnull id<CKKSPeerUpdateListener>)listener {
    [self.peerChangeListeners registerListener:listener];
}

- (void)sendSelfPeerChangedUpdate {
    [self.peerChangeListeners iterateListeners: ^(id<CKKSPeerUpdateListener> listener) {
        [listener selfPeerChanged:self];
    }];
}

- (void)sendTrustedPeerSetChangedUpdate {
    [self.peerChangeListeners iterateListeners: ^(id<CKKSPeerUpdateListener> listener) {
        [listener trustedPeerSetChanged:self];
    }];
}

- (NSSet<id<CKKSRemotePeerProtocol>>*)allPeers
{
    // include the self peer, but as a CKKSSOSPeer object instead of a self peer
    CKKSSOSPeer* s = [[CKKSSOSPeer alloc] initWithSOSPeerID:self.selfPeer.peerID
                                        encryptionPublicKey:self.selfPeer.publicEncryptionKey
                                           signingPublicKey:self.selfPeer.publicSigningKey
                                                   viewList:self.selfPeer.viewList];

    return [self.trustedPeers setByAddingObject: s];
}

@end
