
#ifndef TPHObjcTranslation_h
#define TPHObjcTranslation_h

#import <SecurityFoundation/SFKey.h>
#import <corecrypto/ccec.h>

NS_ASSUME_NONNULL_BEGIN

// This file is for translation of C/Obj-C APIs into swift-friendly things

@interface TPHObjectiveC : NSObject

+ (SFECKeyPair* _Nullable)fetchKeyPairWithPrivateKeyPersistentRef:(NSData*)persistentRef error:(NSError**)error;
+ (ccec_full_ctx_t)ccec384Context;
+ (size_t) ccsha384_diSize;
+ (void)contextFree:(void*) context;
+ (SFAESKeyBitSize)aes256BitSize;
+ (NSString*)digestUsingSha384:(NSData*) data;
@end

NS_ASSUME_NONNULL_END

#endif /* TPHObjcTranslation_h */
