#import "keychain/TrustedPeersHelper/TPHObjcTranslation.h"

#import <Security/SecKey.h>
#import <Security/SecKeyPriv.h>

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>
#import <corecrypto/ccsha2.h>
#import <CommonCrypto/CommonRandomSPI.h>

@implementation TPHObjectiveC : NSObject

+ (SFECKeyPair* _Nullable)fetchKeyPairWithPrivateKeyPersistentRef:(NSData *)persistentRef error:(NSError**)error
{
    SecKeyRef seckey = NULL;
    OSStatus status = SecKeyFindWithPersistentRef((__bridge CFDataRef)persistentRef, &seckey);

    if(status) {
        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:status
                                     userInfo:nil];
        }
        return nil;
    }

    return [[SFECKeyPair alloc] initWithSecKey: seckey];
}

+ (ccec_full_ctx_t)ccec384Context
{
    ccec_const_cp_t cp = ccec_cp_384();
    size_t size = ccec_full_ctx_size(ccec_ccn_size(cp));
    ccec_full_ctx_t heapContext = (ccec_full_ctx_t)malloc(size);
    ccec_ctx_init(cp, heapContext);
    return heapContext;
}

+ (void)contextFree:(void*) context
{
    free(context);
}

+ (size_t) ccsha384_diSize{
    return ccsha384_di()->output_size;
}

+ (SFAESKeyBitSize)aes256BitSize{
    return SFAESKeyBitSize256;
}

+ (NSString*)digestUsingSha384:(NSData*) data {
    const struct ccdigest_info *di = ccsha384_di();
    NSMutableData* result = [[NSMutableData alloc] initWithLength:ccsha384_di()->output_size];

    ccdigest(di, [data length], [data bytes], [result mutableBytes]);

    return [result base64EncodedStringWithOptions:0];
}

@end
