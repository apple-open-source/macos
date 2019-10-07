#if OCTAGON

#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/SecureObjectSync/SOSCloudCircle.h"

NS_ASSUME_NONNULL_BEGIN

@protocol OTSOSAdapter <CKKSPeerProvider>
@property bool sosEnabled;
- (SOSCCStatus)circleStatus:(NSError**)error;
- (id<CKKSSelfPeer> _Nullable)currentSOSSelf:(NSError**)error;
- (NSSet<id<CKKSRemotePeerProtocol>>* _Nullable)fetchTrustedPeers:(NSError**)error;
- (void)updateOctagonKeySetWithAccount:(id<CKKSSelfPeer>)currentSelfPeer error:(NSError**)error;
@end

@interface OTSOSActualAdapter : NSObject <OTSOSAdapter>
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initAsEssential:(BOOL)essential;

// Helper methods
+ (NSArray<NSData*>*)peerPublicSigningKeySPKIs:(NSSet<id<CKKSPeer>>* _Nullable)peers;

+ (NSSet<NSString*>*)sosCKKSViewList;
@end

// This adapter is for a platform which does not have SOS (e.g., aTV, Watch, HomePod)
@interface OTSOSMissingAdapter : NSObject <OTSOSAdapter>
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
