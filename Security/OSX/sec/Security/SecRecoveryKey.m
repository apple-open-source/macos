//
//  SecRecoveryKey.c
//

#import "SecRecoveryKey.h"

#import <corecrypto/cchkdf.h>
#import <corecrypto/ccsha2.h>
#import <corecrypto/ccec.h>

#import <utilities/SecCFWrappers.h>
#import <CommonCrypto/CommonRandomSPI.h>
#import <AssertMacros.h>

#import <Security/SecureObjectSync/SOSCloudCircle.h>

#import "SecCFAllocator.h"
#import "SecPasswordGenerate.h"
#import "SecBase64.h"

typedef struct _CFSecRecoveryKey *CFSecRecoveryKeyRef;


static uint8_t backupPublicKey[] = { 'B', 'a', 'c', 'k', 'u', ' ', 'P', 'u', 'b', 'l', 'i', 'c', 'k', 'e', 'y' };
static uint8_t passwordInfoKey[] = { 'p', 'a', 's', 's', 'w', 'o', 'r', 'd', ' ', 's', 'e', 'c', 'r', 'e', 't' };

#define RK_BACKUP_HKDF_SIZE    128
#define RK_PASSWORD_HKDF_SIZE  32

CFGiblisFor(CFSecRecoveryKey);

struct _CFSecRecoveryKey {
    CFRuntimeBase _base;
    CFDataRef basecode;
};

static void
CFSecRecoveryKeyDestroy(CFTypeRef cf)
{
    CFSecRecoveryKeyRef rk = (CFSecRecoveryKeyRef)cf;
    CFReleaseNull(rk->basecode);
}


static CFStringRef
CFSecRecoveryKeyCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions)
{
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<SecRecoveryKey: %p>"), cf);
}


static bool
ValidateRecoveryKey(CFStringRef recoveryKey)
{

    return SecPasswordValidatePasswordFormat(kSecPasswordTypeiCloudRecoveryKey, recoveryKey, NULL);
}


NSString *
SecRKCreateRecoveryKeyString(NSError **error)
{
    CFErrorRef cferror = NULL;

    CFStringRef recoveryKey = SecPasswordGenerate(kSecPasswordTypeiCloudRecoveryKey, &cferror, NULL);
    if (recoveryKey == NULL) {
        if (error) {
            *error = CFBridgingRelease(cferror);
        } else {
            CFReleaseNull(cferror);
        }
        return NULL;
    }
    if (!ValidateRecoveryKey(recoveryKey)) {
        CFRelease(recoveryKey);
        return NULL;
    }

    return (__bridge NSString *)recoveryKey;
}


SecRecoveryKey *
SecRKCreateRecoveryKey(NSString *masterKey)
{
    if (!ValidateRecoveryKey((__bridge CFStringRef)masterKey))
        return NULL;

    CFSecRecoveryKeyRef rk = CFTypeAllocate(CFSecRecoveryKey, struct _CFSecRecoveryKey, NULL);
    if (rk == NULL)
        return NULL;

    rk->basecode = CFStringCreateExternalRepresentation(SecCFAllocatorZeroize(),
                                                        (__bridge CFStringRef)masterKey,
                                                        kCFStringEncodingUTF8, 0);
    if (rk->basecode == NULL) {
        CFRelease(rk);
        return NULL;
    }

    return (__bridge SecRecoveryKey *)rk;
}

static CFDataRef
SecRKCreateDerivedSecret(CFSecRecoveryKeyRef rk, CFIndex outputLength,
                         const uint8_t *variant, size_t variantLength)
{
    CFMutableDataRef derived;
    int status;

    derived = CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), outputLength);
    if (derived == NULL)
        return NULL;

    status = cchkdf(ccsha256_di(),
                    CFDataGetLength(rk->basecode), CFDataGetBytePtr(rk->basecode),
                    4, "salt",
                    variantLength, variant,
                    CFDataGetLength(derived), CFDataGetMutableBytePtr(derived));
    if (status) {
        CFReleaseNull(derived);
    }
    return derived;
}


NSString *
SecRKCopyAccountRecoveryPassword(SecRecoveryKey *rk)
{
    CFStringRef base64Data = NULL;
    CFDataRef derived = NULL;
    void *b64string = NULL;
    size_t base64Len = 0;

    derived = SecRKCreateDerivedSecret((__bridge CFSecRecoveryKeyRef)rk,
                                       RK_PASSWORD_HKDF_SIZE,
                                       passwordInfoKey, sizeof(passwordInfoKey));
    require(derived, fail);

    base64Len = SecBase64Encode(CFDataGetBytePtr(derived), CFDataGetLength(derived), NULL, 0);
    assert(base64Len < 1024);

    b64string = malloc(base64Len);
    require(b64string, fail);

    SecBase64Encode(CFDataGetBytePtr(derived), CFDataGetLength(derived), b64string, base64Len);

    base64Data = CFStringCreateWithBytes(SecCFAllocatorZeroize(),
                                         (const UInt8 *)b64string, base64Len,
                                         kCFStringEncodingUTF8, false);
    require(base64Data, fail);

fail:
    if (b64string) {
        cc_clear(base64Len, b64string);
        free(b64string);
    }
    CFReleaseNull(derived);

    return (__bridge NSString *)base64Data;
}

#if 0
NSString *
SecRKCopyAccountRecoveryVerifier(SecRecoveryKey *rk,
                                 NSString *type,
                                 NSData *salt,
                                 NSNumber *iterations,
                                 NSError **error)
{
    /* use verifier create function from AppleIDAuthSupport with dlopen/dlsym

     CFDataRef
     AppleIDAuthSupportCreateVerifier(CFStringRef proto,
     CFStringRef username,
     CFDataRef salt,
     CFNumberRef iter,
     CFStringRef password,
     CFErrorRef *error);
     */

    return NULL;
}
#endif

static NSData *
RKBackupCreateECKey(SecRecoveryKey *rk, bool fullkey)
{
    CFMutableDataRef publicKeyData = NULL;
    CFDataRef derivedSecret = NULL;
    ccec_const_cp_t cp = ccec_cp_256();
    CFDataRef result = NULL;
    int status;

    ccec_full_ctx_decl_cp(cp, fullKey);

    derivedSecret = SecRKCreateDerivedSecret((__bridge CFSecRecoveryKeyRef)rk, RK_BACKUP_HKDF_SIZE,
                                             backupPublicKey, sizeof(backupPublicKey));
    require(derivedSecret, fail);

    status = ccec_generate_key_deterministic(cp,
                                             CFDataGetLength(derivedSecret), CFDataGetBytePtr(derivedSecret),
                                             ccDRBGGetRngState(),
                                             CCEC_GENKEY_DETERMINISTIC_COMPACT,
                                             fullKey);
    require_noerr(status, fail);

    size_t space = ccec_compact_export_size(fullkey, fullKey);
    publicKeyData = CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), space);
    require_quiet(publicKeyData, fail);

    ccec_compact_export(fullkey, CFDataGetMutableBytePtr(publicKeyData), fullKey);

    CFTransferRetained(result, publicKeyData);
fail:
    CFReleaseNull(derivedSecret);
    CFReleaseNull(publicKeyData);

    return (__bridge NSData *)result;
}

NSData *
SecRKCopyBackupFullKey(SecRecoveryKey *rk)
{
    return RKBackupCreateECKey(rk, true);
}


NSData *
SecRKCopyBackupPublicKey(SecRecoveryKey *rk)
{
    return RKBackupCreateECKey(rk, false);
}

bool
SecRKRegisterBackupPublicKey(SecRecoveryKey *rk, CFErrorRef *error)
{
    CFDataRef backupKey = (__bridge CFDataRef)SecRKCopyBackupPublicKey(rk);
    bool res = false;

    require(backupKey, fail);

    res = SOSCCRegisterRecoveryPublicKey(backupKey, error);
fail:
    CFReleaseNull(backupKey);

    return res;
}
