
#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSPeerProvider.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperSpecificUser.h"
#import "keychain/ot/CuttlefishXPCWrapper.h"
#import "keychain/ot/OTPersonaAdapter.h"

NS_ASSUME_NONNULL_BEGIN

@interface OctagonSelfPeer : NSObject <CKKSSelfPeer>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPeerID:(NSString*)peerID
               signingIdentity:(SFIdentity*)signingIdentity
            encryptionIdentity:(SFIdentity*)encryptionIdentity;

@end

@interface OctagonCKKSPeerAdapter : NSObject  <CKKSPeerProvider>

@property (nullable) NSString* peerID;
@property (readonly) TPSpecificUser* specificUser;
@property (readonly) CuttlefishXPCWrapper* cuttlefishXPCWrapper;
@property id<OTPersonaAdapter> personaAdapter;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPeerID:(NSString*)peerID
                  specificUser:(TPSpecificUser*)specificUser
                personaAdapter:(id<OTPersonaAdapter>)personaAdapter
                 cuttlefishXPC:(CuttlefishXPCWrapper*)cuttlefishXPCWrapper;
@end

NS_ASSUME_NONNULL_END

#endif
