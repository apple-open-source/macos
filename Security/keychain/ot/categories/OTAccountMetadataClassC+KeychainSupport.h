
#if OCTAGON

#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"
#import "keychain/ot/proto/generated_source/OTAccountMetadataClassCEscrowRecordCache.h"
#import "keychain/ot/OTPersonaAdapter.h"

// For the escrowRepairAttemptVersion field.
#define ESCROW_REPAIR_CURRENT_VERSION 4

NS_ASSUME_NONNULL_BEGIN

@interface OTAccountMetadataClassC (KeychainSupport)

- (BOOL)saveToKeychainForContainer:(NSString*)containerName
                         contextID:(NSString*)contextID
                   personaAdapter:(id<OTPersonaAdapter>)personaAdapter
               personaUniqueString:(NSString* _Nullable)personaUniqueString
                             error:(NSError**)error;

+ (BOOL)deleteFromKeychainForContainer:(NSString*)containerName
                             contextID:(NSString*)contextID
                       personaAdapter:(id<OTPersonaAdapter>)personaAdapter
                   personaUniqueString:(NSString* _Nullable)personaUniqueString
                                 error:(NSError**)error  __attribute__((swift_error(nonnull_error)));

+ (OTAccountMetadataClassC* _Nullable)loadFromKeychainForContainer:(NSString*)containerName
                                                         contextID:(NSString*)contextID
                                                   personaAdapter:(id<OTPersonaAdapter>)personaAdapter
                                               personaUniqueString:(NSString* _Nullable)personaUniqueString
                                                             error:(NSError**)error;
@end

@class TPSyncingPolicy;
@class CKKSTLKShare;
@class OTSecureElementPeerIdentity;
@class TPPBSecureElementIdentity;

@interface OTAccountMetadataClassC (NSSecureCodingSupport)

@property (nullable, readonly) NSDate* memoizedLastHealthCheck;
@property (nullable, readonly) NSDate* memoizedLastEscrowRepairTriggered;
@property (nullable, readonly) NSDate* memoizedLastEscrowRepairAttempted;
@property (nullable, readonly) NSDate* memoizedEscrowRecordCacheTimestamp;

- (void)setTPSyncingPolicy:(TPSyncingPolicy* _Nullable)policy;
- (TPSyncingPolicy* _Nullable)getTPSyncingPolicy;

- (void)setTLKSharesPairedWithVoucher:(NSArray<CKKSTLKShare*>*)newTLKShares;
- (NSArray<CKKSTLKShare*>*)getTLKSharesPairedWithVoucher;

- (void)setOctagonSecureElementIdentity:(OTSecureElementPeerIdentity *)secureElementIdentity;
- (TPPBSecureElementIdentity* _Nullable)parsedSecureElementIdentity;

@end

@interface OTAccountMetadataClassCEscrowRecordCache (KeychainSupport)

@property (readonly) NSInteger rateLimitTimeLeft;

@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
