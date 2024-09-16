#import "keychain/TrustedPeersHelper/TPHObjcTranslation.h"
#include "utilities/SecCFRelease.h"

#import <Security/SecKey.h>
#import <Security/SecKeyPriv.h>

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>
#import <corecrypto/ccsha2.h>
#import <corecrypto/ccrng.h>

@implementation TPHObjectiveC : NSObject

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

+ (SFECKeyPair* _Nullable)fetchKeyPairWithPrivateKeyPersistentRef:(NSData *)persistentRef error:(NSError**)error
{
    NSDictionary* query = @{
        (id)kSecReturnRef : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecValuePersistentRef : persistentRef,
    };

    CFTypeRef foundRef = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &foundRef);

    if (status == errSecSuccess && CFGetTypeID(foundRef) == SecKeyGetTypeID()) {
        SFECKeyPair* keyPair = [[SFECKeyPair alloc] initWithSecKey:(SecKeyRef)foundRef];
        CFReleaseNull(foundRef);
        return keyPair;
    } else {
        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:status
                                     userInfo:nil];
        }
        return nil;
    }
}

#pragma clang diagnostic pop

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
