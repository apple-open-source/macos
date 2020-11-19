
#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSPeerProvider.h"
#import "keychain/ot/CuttlefishXPCWrapper.h"

NS_ASSUME_NONNULL_BEGIN

@interface OctagonSelfPeer : NSObject <CKKSSelfPeer>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPeerID:(NSString*)peerID
               signingIdentity:(SFIdentity*)signingIdentity
            encryptionIdentity:(SFIdentity*)encryptionIdentity;

@end

@interface OctagonCKKSPeerAdapter : NSObject  <CKKSPeerProvider>

@property (nullable) NSString* peerID;
@property (readonly) CuttlefishXPCWrapper* cuttlefishXPCWrapper;
@property (readonly) NSString* containerName;
@property (readonly) NSString* contextID;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPeerID:(NSString*)peerID
                 containerName:(NSString*)containerName
                     contextID:(NSString*)contextID
                 cuttlefishXPC:(CuttlefishXPCWrapper*)cuttlefishXPCWrapper;
@end

NS_ASSUME_NONNULL_END

#endif
