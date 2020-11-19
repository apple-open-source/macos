#if OCTAGON
#import <dispatch/dispatch.h>
#import <notify.h>

#import "keychain/ot/OTSOSAdapter.h"

#import "keychain/SecureObjectSync/SOSCloudCircleInternal.h"
#include "keychain/SecureObjectSync/SOSViews.h"

#import "keychain/securityd/SOSCloudCircleServer.h"
#import "OSX/utilities/SecCFWrappers.h"

#import "keychain/categories/NSError+UsefulConstructors.h"

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSListenerCollection.h"
#import "keychain/SecureObjectSync/SOSAccount.h"
#import "keychain/SecureObjectSync/SOSAccountPriv.h"

#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ckks/CKKSAnalytics.h"

@interface OTSOSActualAdapter ()
@property CKKSListenerCollection* peerChangeListeners;
@end

@implementation OTSOSActualAdapter
@synthesize sosEnabled;
@synthesize essential = _essential;
@synthesize providerID = _providerID;

+ (NSSet<NSString*>*)sosCKKSViewList
{
    static NSSet<NSString*>* list = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        list = CFBridgingRelease(SOSViewCopyViewSet(kViewSetCKKS));
    });
    return list;
}

- (instancetype)initAsEssential:(BOOL)essential {
    if((self = [super init])) {
        self.sosEnabled = true;

        _essential = essential;

        _providerID = @"[OTSOSActualAdapter]";
        _peerChangeListeners = [[CKKSListenerCollection alloc] initWithName:@"ckks-sos"];

        __typeof(self) weakSelf = self;
        // If this is a live server, register with notify
        if(!SecCKKSTestsEnabled()) {
            int token = 0;
            notify_register_dispatch(kSOSCCCircleOctagonKeysChangedNotification, &token, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^(int t) {
                // Since SOS doesn't change the self peer, we can reliably just send "trusted peers changed"; it'll be mostly right
                secnotice("octagon-sos", "Received a notification that the SOS Octagon peer set changed");
                [weakSelf sendTrustedPeerSetChangedUpdate];
            });
        }
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<OTSOSActualAdapter e:%d>", self.essential];
}

- (SOSCCStatus)circleStatus:(NSError**)error
{
    CFErrorRef cferror = nil;
    SOSCCStatus status = SOSCCThisDeviceIsInCircle(&cferror);
    if(error && cferror) {
        *error = CFBridgingRelease(cferror);
    } else {
        CFReleaseNull(cferror);
    }
    return status;
}

- (id<CKKSSelfPeer> _Nullable)currentSOSSelf:(NSError**)error
{
    __block SFECKeyPair* signingPrivateKey = nil;
    __block SFECKeyPair* encryptionPrivateKey = nil;

    __block NSError* localerror = nil;

    CFErrorRef cferror = nil;

    SOSCCStatus circleStatus = [self circleStatus:&localerror];
    if(circleStatus != kSOSCCInCircle) {
        if(!localerror) {
            localerror = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNoPeersAvailable
                                      description:@"Not in SOS circle, but no error returned"];
        }
        secerror("octagon-sos: Not in circle : %@ %@", SOSAccountGetSOSCCStatusString(circleStatus), localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    SOSPeerInfoRef egoPeerInfo = SOSCCCopyMyPeerInfo(&cferror);
    NSString* egoPeerID = egoPeerInfo ? (NSString*)CFBridgingRelease(CFRetainSafe(SOSPeerInfoGetPeerID(egoPeerInfo))) : nil;
    CFReleaseNull(egoPeerInfo);
    
    if(!egoPeerID || cferror) {
        localerror = CFBridgingRelease(cferror);
        if(!localerror) {
            localerror = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNoPeersAvailable
                                      description:@"No SOS peer info available, but no error returned"];
        }

        secerror("octagon-sos: Error fetching self peer : %@", cferror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    SOSCCPerformWithAllOctagonKeys(^(SecKeyRef octagonEncryptionKey, SecKeyRef octagonSigningKey, CFErrorRef cferror) {
        if(cferror) {
            localerror = (__bridge NSError*)cferror;
            return;
        }
        if (!cferror && octagonEncryptionKey && octagonSigningKey) {
            signingPrivateKey = [[SFECKeyPair alloc] initWithSecKey:octagonSigningKey];
            encryptionPrivateKey = [[SFECKeyPair alloc] initWithSecKey:octagonEncryptionKey];
        } else {
            localerror = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNoPeersAvailable
                                      description:@"Not all SOS peer keys available, but no error returned"];
        }
    });

    if(localerror) {
        if(![[CKKSLockStateTracker globalTracker] isLockedError:localerror]) {
            secerror("octagon-sos: Error fetching self encryption keys: %@", localerror);
        }
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    CKKSSOSSelfPeer* selfPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:egoPeerID
                                                             encryptionKey:encryptionPrivateKey
                                                                signingKey:signingPrivateKey
                                                                  viewList:[OTSOSActualAdapter sosCKKSViewList]];
    return selfPeer;
}

- (CKKSSelves * _Nullable)fetchSelfPeers:(NSError *__autoreleasing  _Nullable * _Nullable)error {
    id<CKKSSelfPeer> peer = [self currentSOSSelf:error];
    if(!peer) {
        return nil;
    }

    return [[CKKSSelves alloc] initWithCurrent:peer allSelves:nil];
}

- (NSSet<id<CKKSRemotePeerProtocol>>* _Nullable)fetchTrustedPeers:(NSError**)error
{
    __block NSMutableSet<id<CKKSRemotePeerProtocol>>* peerSet = [NSMutableSet set];

    __block NSError* localError = nil;

    SOSCCPerformWithTrustedPeers(^(CFSetRef sosPeerInfoRefs, CFErrorRef cfTrustedPeersError) {
        if(cfTrustedPeersError) {
            secerror("octagon-sos: Error fetching trusted peers: %@", cfTrustedPeersError);
            if(localError) {
                localError = (__bridge NSError*)cfTrustedPeersError;
            }
        }

        CFSetForEach(sosPeerInfoRefs, ^(const void* voidPeer) {
            CFErrorRef cfPeerError = NULL;
            SOSPeerInfoRef sosPeerInfoRef = (SOSPeerInfoRef)voidPeer;

            if(!sosPeerInfoRef) {
                return;
            }

            CFStringRef cfpeerID = SOSPeerInfoGetPeerID(sosPeerInfoRef);
            SecKeyRef cfOctagonSigningKey = NULL, cfOctagonEncryptionKey = NULL;

            cfOctagonSigningKey = SOSPeerInfoCopyOctagonSigningPublicKey(sosPeerInfoRef, &cfPeerError);
            if (cfOctagonSigningKey) {
                cfOctagonEncryptionKey = SOSPeerInfoCopyOctagonEncryptionPublicKey(sosPeerInfoRef, &cfPeerError);
            }

            if(cfOctagonSigningKey == NULL || cfOctagonEncryptionKey == NULL) {
                // Don't log non-debug for -50; it almost always just means this peer didn't have octagon keys
                if(cfPeerError == NULL
                   || !(CFEqualSafe(CFErrorGetDomain(cfPeerError), kCFErrorDomainOSStatus) && (CFErrorGetCode(cfPeerError) == errSecParam)))
                {
                    secerror("octagon-sos: error fetching octagon keys for peer: %@ %@", sosPeerInfoRef, cfPeerError);
                } else {
                    secinfo("octagon-sos", "Peer(%@) doesn't have Octagon keys, but this is expected: %@", cfpeerID, cfPeerError);
                }
            }

            // Add all peers to the trust set: old-style SOS peers will just have null keys
            SFECPublicKey* signingPublicKey = cfOctagonSigningKey ? [[SFECPublicKey alloc] initWithSecKey:cfOctagonSigningKey] : nil;
            SFECPublicKey* encryptionPublicKey = cfOctagonEncryptionKey ? [[SFECPublicKey alloc] initWithSecKey:cfOctagonEncryptionKey] : nil;

            CKKSSOSPeer* peer = [[CKKSSOSPeer alloc] initWithSOSPeerID:(__bridge NSString*)cfpeerID
                                                   encryptionPublicKey:encryptionPublicKey
                                                      signingPublicKey:signingPublicKey
                                                              viewList:[OTSOSActualAdapter sosCKKSViewList]];
            [peerSet addObject:peer];

            CFReleaseNull(cfOctagonSigningKey);
            CFReleaseNull(cfOctagonEncryptionKey);
            CFReleaseNull(cfPeerError);
        });
    });

    if(error && localError) {
        *error = localError;
    }

    return peerSet;
}


- (BOOL)preloadOctagonKeySetOnAccount:(id<CKKSSelfPeer>)currentSelfPeer error:(NSError**)error {
    // in case we don't have the keys, don't try to update them
    if (currentSelfPeer.publicSigningKey.secKey == NULL || currentSelfPeer.publicEncryptionKey.secKey == NULL) {
        secnotice("octagon-preload-keys", "no octagon keys available skipping updating SOS record");
        return YES;
    }

    __block CFDataRef signingFullKey = CFBridgingRetain(currentSelfPeer.signingKey.keyData);
    __block CFDataRef encryptionFullKey = CFBridgingRetain(currentSelfPeer.encryptionKey.keyData);
    __block SecKeyRef octagonSigningPubSecKey = CFRetainSafe(currentSelfPeer.publicSigningKey.secKey);
    __block SecKeyRef octagonEncryptionPubSecKey = CFRetainSafe(currentSelfPeer.publicEncryptionKey.secKey);
    __block SecKeyRef signingFullKeyRef = CFRetainSafe(currentSelfPeer.signingKey.secKey);
    __block SecKeyRef encryptionFullKeyRef = CFRetainSafe(currentSelfPeer.encryptionKey.secKey);

    SFAnalyticsActivityTracker *tracker = [[[CKKSAnalytics class] logger] startLogSystemMetricsForActivityNamed:OctagonSOSAdapterUpdateKeys];

    __block BOOL ret;
    __block NSError *localError = nil;

    /* WARNING! Please be very very careful passing keys to this routine.  Note the slightly different variations of keys*/
    SOSCCPerformPreloadOfAllOctagonKeys(signingFullKey, encryptionFullKey,
                                        signingFullKeyRef, encryptionFullKeyRef,
                                        octagonSigningPubSecKey, octagonEncryptionPubSecKey,
                                       ^(CFErrorRef cferror) {
                                           [tracker stopWithEvent: OctagonSOSAdapterUpdateKeys result:(__bridge NSError * _Nullable)(cferror)];
                                           if(cferror) {
                                               secerror("octagon-preload-keys: failed to preload Octagon keys in SOS:%@", cferror);
                                               ret = NO;
                                               localError = (__bridge NSError *)cferror;
                                           } else {
                                               ret = YES;
                                               secnotice("octagon-preload-keys", "successfully preloaded Octagon keys in SOS!");
                                           }
                                           CFRelease(signingFullKey);
                                           CFRelease(encryptionFullKey);
                                           CFRelease(octagonSigningPubSecKey);
                                           CFRelease(octagonEncryptionPubSecKey);
                                           CFRelease(signingFullKeyRef);
                                           CFRelease(encryptionFullKeyRef);
                                       });
    if (error) {
        *error = localError;
    }
    return ret;

}

- (BOOL)updateOctagonKeySetWithAccount:(id<CKKSSelfPeer>)currentSelfPeer error:(NSError**)error {

    // in case we don't have the keys, don't try to update them
    if (currentSelfPeer.publicSigningKey.secKey == NULL || currentSelfPeer.publicEncryptionKey.secKey == NULL) {
        secnotice("octagon-sos", "no octagon keys available skipping updating SOS record");
        return YES;
    }

    __block CFDataRef signingFullKey = CFBridgingRetain(currentSelfPeer.signingKey.keyData);
    __block CFDataRef encryptionFullKey = CFBridgingRetain(currentSelfPeer.encryptionKey.keyData);
    __block CFDataRef signingPublicKey = CFBridgingRetain(currentSelfPeer.publicSigningKey.keyData);
    __block CFDataRef encryptionPublicKey = CFBridgingRetain(currentSelfPeer.publicEncryptionKey.keyData);
    __block SecKeyRef octagonSigningPubSecKey = CFRetainSafe(currentSelfPeer.publicSigningKey.secKey);
    __block SecKeyRef octagonEncryptionPubSecKey = CFRetainSafe(currentSelfPeer.publicEncryptionKey.secKey);

    SFAnalyticsActivityTracker *tracker = [[[CKKSAnalytics class] logger] startLogSystemMetricsForActivityNamed:OctagonSOSAdapterUpdateKeys];

    __block BOOL ret;
    __block NSError *localError = nil;

    /* WARNING! Please be very very careful passing keys to this routine.  Note the slightly different variations of keys*/
    SOSCCPerformUpdateOfAllOctagonKeys(signingFullKey, encryptionFullKey,
                                       signingPublicKey, encryptionPublicKey,
                                       octagonSigningPubSecKey, octagonEncryptionPubSecKey,
                                       ^(CFErrorRef cferror) {
                                           [tracker stopWithEvent: OctagonSOSAdapterUpdateKeys result:(__bridge NSError * _Nullable)(cferror)];
                                           if(cferror) {
                                               secerror("octagon-sos: failed to update Octagon keys in SOS:%@", cferror);
                                               ret = NO;
                                               localError = (__bridge NSError *)cferror;
                                           } else {
                                               ret = YES;
                                               secnotice("octagon-sos", "successfully updated Octagon keys in SOS!");
                                           }
                                           CFRelease(signingFullKey);
                                           CFRelease(encryptionFullKey);
                                           CFRelease(signingPublicKey);
                                           CFRelease(encryptionPublicKey);
                                           CFRelease(octagonSigningPubSecKey);
                                           CFRelease(octagonEncryptionPubSecKey);
                                       });
    if (error) {
        *error = localError;
    }
    return ret;
}

- (BOOL)updateCKKS4AllStatus:(BOOL)status error:(NSError**)error
{
    CFErrorRef cferror = nil;
    bool result = SOSCCSetCKKS4AllStatus(status, &cferror);

    NSError* localerror = CFBridgingRelease(cferror);

    if(!result || localerror) {
        secerror("octagon-sos: failed to update CKKS4All status in SOS: %@", localerror);
    } else {
        secnotice("octagon-sos", "successfully updated CKKS4All status in SOS to '%@'", status ? @"supported" : @"not supported");
    }

    if(error && localerror) {
        *error = localerror;
    }
    return result;
}

- (void)registerForPeerChangeUpdates:(nonnull id<CKKSPeerUpdateListener>)listener {
    [self.peerChangeListeners registerListener:listener];
}

- (void)sendSelfPeerChangedUpdate {
    [self.peerChangeListeners iterateListeners: ^(id<CKKSPeerUpdateListener> listener) {
        [listener selfPeerChanged: self];
    }];
}

- (void)sendTrustedPeerSetChangedUpdate {
    [self.peerChangeListeners iterateListeners: ^(id<CKKSPeerUpdateListener> listener) {
        [listener trustedPeerSetChanged: self];
    }];
}

- (nonnull CKKSPeerProviderState *)currentState {
    __block CKKSPeerProviderState* result = nil;

    [SOSAccount performOnQuietAccountQueue: ^{
        NSError* selfPeersError = nil;
        CKKSSelves* currentSelfPeers = [self fetchSelfPeers:&selfPeersError];

        NSError* trustedPeersError = nil;
        NSSet<id<CKKSRemotePeerProtocol>>* currentTrustedPeers = [self fetchTrustedPeers:&trustedPeersError];

        result = [[CKKSPeerProviderState alloc] initWithPeerProviderID:self.providerID
                                                             essential:self.essential
                                                             selfPeers:currentSelfPeers
                                                        selfPeersError:selfPeersError
                                                          trustedPeers:currentTrustedPeers
                                                     trustedPeersError:trustedPeersError];
    }];

    return result;
}

- (BOOL)safariViewSyncingEnabled:(NSError**)error
{
    CFErrorRef viewCFError = NULL;
    SOSViewResultCode result = SOSCCView(kSOSViewAutofillPasswords, kSOSCCViewQuery, &viewCFError);

    if(viewCFError) {
        if(error) {
            *error = CFBridgingRelease(viewCFError);
        } else {
            CFReleaseNull(viewCFError);
        }
        return NO;
    }

    return result == kSOSCCViewMember;
}
@end

@implementation OTSOSMissingAdapter
@synthesize sosEnabled;
@synthesize providerID = _providerID;
@synthesize essential = _essential;

- (instancetype)init {
    if((self = [super init])) {
        self.sosEnabled = false;
        _providerID = @"[OTSOSMissingAdapter]";

        // This adapter is never going to return anything, so you probably shouldn't ever consider it Must Succeed
        _essential = NO;
    }
    return self;
}

- (SOSCCStatus)circleStatus:(NSError**)error
{
    return kSOSCCNotInCircle;
}

- (id<CKKSSelfPeer> _Nullable)currentSOSSelf:(NSError**)error
{
    if(error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                     code:errSecUnimplemented
                              description:@"SOS unsupported on this platform"];
    }
    return nil;
}

- (NSSet<id<CKKSRemotePeerProtocol>>* _Nullable)fetchTrustedPeers:(NSError**)error
{
    if(error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                     code:errSecUnimplemented
                              description:@"SOS unsupported on this platform"];
    }
    return nil;
}

- (BOOL)updateOctagonKeySetWithAccount:(nonnull id<CKKSSelfPeer>)currentSelfPeer error:(NSError *__autoreleasing  _Nullable * _Nullable)error {
    if(error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                     code:errSecUnimplemented
                              description:@"SOS unsupported on this platform"];
    }
    return NO;
}

- (BOOL)updateCKKS4AllStatus:(BOOL)status error:(NSError**)error
{
    if(error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                     code:errSecUnimplemented
                              description:@"SOS unsupported on this platform"];
    }
    return NO;
}

- (CKKSSelves * _Nullable)fetchSelfPeers:(NSError * _Nullable __autoreleasing * _Nullable)error
{
    if(error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                     code:errSecUnimplemented
                              description:@"SOS unsupported on this platform"];
    }
    return nil;
}

- (void)registerForPeerChangeUpdates:(nonnull id<CKKSPeerUpdateListener>)listener
{
    // no op
}

- (void)sendSelfPeerChangedUpdate
{
    // no op
}

- (void)sendTrustedPeerSetChangedUpdate
{
    // no op
}

- (nonnull CKKSPeerProviderState *)currentState {
    NSError* unimplementedError = [NSError errorWithDomain:NSOSStatusErrorDomain
                                                      code:errSecUnimplemented
                                               description:@"SOS unsupported on this platform"];
    return [[CKKSPeerProviderState alloc] initWithPeerProviderID:self.providerID
                                                                 essential:self.essential
                                                                 selfPeers:nil
                                                            selfPeersError:unimplementedError
                                                              trustedPeers:nil
                                                         trustedPeersError:unimplementedError];
}

- (BOOL)safariViewSyncingEnabled:(NSError**)error
{
    return NO;
}

- (BOOL)preloadOctagonKeySetOnAccount:(nonnull id<CKKSSelfPeer>)currentSelfPeer error:(NSError *__autoreleasing  _Nullable * _Nullable)error {
    if(error) {
        *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                     code:errSecUnimplemented
                              description:@"SOS unsupported on this platform"];
    }
    return NO;
}

@end

@implementation OTSOSAdapterHelpers

+ (NSArray<NSData*>*)peerPublicSigningKeySPKIs:(NSSet<id<CKKSPeer>>* _Nullable)peerSet
{
    NSMutableArray<NSData*>* publicSigningSPKIs = [NSMutableArray array];

    for(id<CKKSPeer> peer in peerSet) {
        NSData* spki = [peer.publicSigningKey encodeSubjectPublicKeyInfo];
        if(!spki) {
            secerror("octagon-sos: Can't create SPKI for peer: %@", peer);
        } else {
            secerror("octagon-sos: Created SPKI for peer: %@", peer);
            [publicSigningSPKIs addObject:spki];
        }
    }
    return publicSigningSPKIs;
}

+ (NSArray<NSData*>* _Nullable)peerPublicSigningKeySPKIsForCircle:(id<OTSOSAdapter>)sosAdapter error:(NSError**)error
{
    NSError* peerError = nil;
    SOSCCStatus sosStatus = [sosAdapter circleStatus:&peerError];

    if(sosStatus != kSOSCCInCircle || peerError) {
        secerror("octagon-sos: Not in circle; not preapproving keys: %@ (%@)", SOSAccountGetSOSCCStatusString(sosStatus), peerError);
        if(error) {
            *error = peerError;
        }
        return nil;
    } else {
        // We're in-circle: preapprove these peers
        NSError* peerError = nil;

        NSSet<id<CKKSRemotePeerProtocol>>* peerSet = [sosAdapter fetchTrustedPeers:&peerError];

        if(!peerSet || peerError) {
            secerror("octagon-sos: Can't fetch trusted peer SPKIs: %@", peerError);
            if(error) {
                *error = peerError;
            }
            return nil;
        }

        return [self peerPublicSigningKeySPKIs:peerSet];
    }
}
@end
#endif // OCTAGON
