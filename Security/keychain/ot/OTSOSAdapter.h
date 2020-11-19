#if OCTAGON

#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSPeerProvider.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/SecureObjectSync/SOSCloudCircle.h"

NS_ASSUME_NONNULL_BEGIN

@protocol OTSOSAdapter <CKKSPeerProvider>
@property bool sosEnabled;
- (SOSCCStatus)circleStatus:(NSError**)error;
- (id<CKKSSelfPeer> _Nullable)currentSOSSelf:(NSError**)error;
- (NSSet<id<CKKSRemotePeerProtocol>>* _Nullable)fetchTrustedPeers:(NSError**)error;
- (BOOL)updateOctagonKeySetWithAccount:(id<CKKSSelfPeer>)currentSelfPeer error:(NSError**)error;
- (BOOL)preloadOctagonKeySetOnAccount:(id<CKKSSelfPeer>)currentSelfPeer error:(NSError**)error;
- (BOOL)updateCKKS4AllStatus:(BOOL)status error:(NSError**)error;

- (BOOL)safariViewSyncingEnabled:(NSError**)error __attribute__((swift_error(nonnull_error)));
@end

@interface OTSOSActualAdapter : NSObject <OTSOSAdapter>
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initAsEssential:(BOOL)essential;

// Helper methods.
+ (NSSet<NSString*>*)sosCKKSViewList;
@end


// This adapter is for a platform which does not have SOS (e.g., aTV, Watch, HomePod)
@interface OTSOSMissingAdapter : NSObject <OTSOSAdapter>
@end

// Helper code
@interface OTSOSAdapterHelpers : NSObject
+ (NSArray<NSData*>* _Nullable)peerPublicSigningKeySPKIsForCircle:(id<OTSOSAdapter>)sosAdapter error:(NSError**)error;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
