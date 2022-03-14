
#if OCTAGON

#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTAccountMetadataClassC (KeychainSupport)

- (BOOL)saveToKeychainForContainer:(NSString*)containerName contextID:(NSString*)contextID error:(NSError**)error;

+ (BOOL)deleteFromKeychainForContainer:(NSString*)containerName
                             contextID:(NSString*)contextID error:(NSError**)error  __attribute__((swift_error(nonnull_error)));

+ (OTAccountMetadataClassC* _Nullable)loadFromKeychainForContainer:(NSString*)containerName contextID:(NSString*)contextID error:(NSError**)error;
@end

@class TPSyncingPolicy;
@class CKKSTLKShare;
@class OTSecureElementPeerIdentity;
@class TPPBSecureElementIdentity;
@interface OTAccountMetadataClassC (NSSecureCodingSupport)
- (void)setTPSyncingPolicy:(TPSyncingPolicy* _Nullable)policy;
- (TPSyncingPolicy* _Nullable)getTPSyncingPolicy;

- (void)setTLKSharesPairedWithVoucher:(NSArray<CKKSTLKShare*>*)newTLKShares;
- (NSArray<CKKSTLKShare*>*)getTLKSharesPairedWithVoucher;

- (void)setOctagonSecureElementIdentity:(OTSecureElementPeerIdentity *)secureElementIdentity;
- (TPPBSecureElementIdentity* _Nullable)parsedSecureElementIdentity;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
