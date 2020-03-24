
#if OCTAGON

#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTAccountMetadataClassC (KeychainSupport)

- (BOOL)saveToKeychainForContainer:(NSString*)containerName contextID:(NSString*)contextID error:(NSError**)error;

+ (BOOL) deleteFromKeychainForContainer:(NSString*)containerName
                              contextID:(NSString*)contextID error:(NSError**)error  __attribute__((swift_error(nonnull_error)));

+ (OTAccountMetadataClassC* _Nullable)loadFromKeychainForContainer:(NSString*)containerName contextID:(NSString*)contextID error:(NSError**)error;
@end

@class TPPolicy;
@interface OTAccountMetadataClassC (NSSecureCodingSupport)
- (void)setTPPolicy:(TPPolicy* _Nullable)policy;
- (TPPolicy* _Nullable)getTPPolicy;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
