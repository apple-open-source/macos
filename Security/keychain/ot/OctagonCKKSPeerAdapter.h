
#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ckks/CKKSPeer.h"

NS_ASSUME_NONNULL_BEGIN

@interface OctagonSelfPeer : NSObject <CKKSSelfPeer>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPeerID:(NSString*)peerID
               signingIdentity:(SFIdentity*)signingIdentity
            encryptionIdentity:(SFIdentity*)encryptionIdentity;

@end

@interface OctagonCKKSPeerAdapter : NSObject  <CKKSPeerProvider>

@property (nullable) NSString* peerID;
@property OTOperationDependencies* deps;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithPeerID:(NSString*)peerID operationDependencies:(OTOperationDependencies*)deps;
@end

NS_ASSUME_NONNULL_END

#endif
