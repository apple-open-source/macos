
#include <Security/Security.h>
#include <Security/SecBreadcrumb.h>
#include <Security/SecRandom.h>

#include <corecrypto/ccaes.h>
#include <corecrypto/ccpbkdf2.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/ccmode_factory.h>
#include <corecrypto/ccsha2.h>

#include <CommonCrypto/CommonRandomSPI.h>

#define CFReleaseNull(CF) ({ __typeof__(CF) *const _pcf = &(CF), _cf = *_pcf; (_cf ? (*_pcf) = ((__typeof__(CF))0), (CFRelease(_cf), ((__typeof__(CF))0)) : _cf); })

static const int kKeySize = CCAES_KEY_SIZE_128;
static const int kSaltSize = 20;
static const int kIterations = 5000;
static const CFIndex tagLen = 16;
static const uint8_t BCversion = 1;
static const size_t paddingSize = 256;
static const size_t maxSize = 1024;

Boolean
SecBreadcrumbCreateFromPassword(CFStringRef inPassword,
                                CFDataRef *outBreadcrumb,
                                CFDataRef *outEncryptedKey,
                                CFErrorRef *outError)
{
    const struct ccmode_ecb *ecb = ccaes_ecb_encrypt_mode();
    const struct ccmode_gcm gcm = CCMODE_FACTORY_GCM_ENCRYPT(ecb);
    const struct ccdigest_info *di = ccsha256_di();
    CFMutableDataRef key, npw;
    CFDataRef pw;
    
    *outBreadcrumb = NULL;
    *outEncryptedKey = NULL;
    if (outError)
        *outError = NULL;
    
    key = CFDataCreateMutable(NULL, 0);
    if (key == NULL)
        return false;
    
    CFDataSetLength(key, kKeySize + kSaltSize + 4);
    CCRandomCopyBytes(kCCRandomDefault, CFDataGetMutableBytePtr(key), CFDataGetLength(key) - 4);
    uint32_t size = htonl(kIterations);
    memcpy(CFDataGetMutableBytePtr(key) + kKeySize + kSaltSize, &size, sizeof(size));
    
    /*
     * Create data for password
     */
    
    pw = CFStringCreateExternalRepresentation(NULL, inPassword, kCFStringEncodingUTF8, 0);
    if (pw == NULL) {
        CFReleaseNull(key);
        return false;
    }

    const CFIndex passwordLength = CFDataGetLength(pw);
    
    if (passwordLength > maxSize) {
        CFReleaseNull(pw);
        CFReleaseNull(key);
        return false;
    }

    CFIndex paddedSize = passwordLength + paddingSize - (passwordLength % paddingSize);
    const CFIndex outLength = 1 + 4 + paddedSize + tagLen;
    
    npw = CFDataCreateMutable(NULL, outLength);
    if (npw == NULL) {
        CFReleaseNull(pw);
        CFReleaseNull(key);
        return false;
    }
    CFDataSetLength(npw, outLength);

    memset(CFDataGetMutableBytePtr(npw), 0, outLength);
    CFDataGetMutableBytePtr(npw)[0] = BCversion;
    size = htonl(passwordLength);
    memcpy(CFDataGetMutableBytePtr(npw) + 1, &size, sizeof(size));
    memcpy(CFDataGetMutableBytePtr(npw) + 5, CFDataGetBytePtr(pw), passwordLength);
    
    /*
     * Now create a GCM encrypted password using the random key
     */
    
    ccgcm_ctx_decl(gcm.size, ctx);
    gcm.init(&gcm, ctx, kKeySize, CFDataGetMutableBytePtr(key));
    gcm.gmac(ctx, 1, CFDataGetMutableBytePtr(npw));
    gcm.gcm(ctx, outLength - tagLen - 1, CFDataGetMutableBytePtr(npw) + 1, CFDataGetMutableBytePtr(npw) + 1);
    gcm.finalize(ctx, tagLen, CFDataGetMutableBytePtr(npw) + outLength - tagLen);
    ccgcm_ctx_clear(gcm.size, ctx);
    
    /*
     * Wrapping key is PBKDF2(sha256) over password
     */
    
    if (di->output_size < kKeySize) abort();
    
    uint8_t rawkey[di->output_size];
    
    if (ccpbkdf2_hmac(di, CFDataGetLength(pw), CFDataGetBytePtr(pw),
                      kSaltSize, CFDataGetMutableBytePtr(key) + kKeySize,
                      kIterations,
                      sizeof(rawkey), rawkey) != 0)
        abort();
    
    /*
     * Wrap the random key with one round of ECB cryto
     */

    ccecb_ctx_decl(ecb->size, ecbkey);
    ecb->init(ecb, ecbkey, kKeySize, rawkey);
    ecb->ecb(ecbkey, 1, CFDataGetMutableBytePtr(key), CFDataGetMutableBytePtr(key));
    
    /*
     *
     */
    
    memset(rawkey, 0, sizeof(rawkey));
    CFReleaseNull(pw);
    
    *outBreadcrumb = npw;
    *outEncryptedKey = key;
    
    return true;
}


Boolean
SecBreadcrumbCopyPassword(CFStringRef inPassword,
                          CFDataRef inBreadcrumb,
                          CFDataRef inEncryptedKey,
                          CFStringRef *outPassword,
                          CFErrorRef *outError)
{
    const struct ccmode_ecb *ecb = ccaes_ecb_decrypt_mode();
    const struct ccmode_gcm gcm = CCMODE_FACTORY_GCM_DECRYPT(ccaes_ecb_encrypt_mode());
    const struct ccdigest_info *di = ccsha256_di();
    CFMutableDataRef gcmkey, oldpw;
    CFDataRef pw;
    uint32_t size;
    
    *outPassword = NULL;
    if (outError)
        *outError = NULL;
    
    if (CFDataGetLength(inEncryptedKey) < kKeySize + kSaltSize + 4) {
        return false;
    }
    
    if (CFDataGetLength(inBreadcrumb) < 1 + 4 + paddingSize + tagLen) {
        return false;
    }
    
    if (CFDataGetBytePtr(inBreadcrumb)[0] != BCversion) {
        return false;
    }
    
    gcmkey = CFDataCreateMutableCopy(NULL, 0, inEncryptedKey);
    if (gcmkey == NULL) {
        return false;
    }
    
    const CFIndex outLength = CFDataGetLength(inBreadcrumb) - 1 - tagLen;
    if ((outLength % 16) != 0 && outLength < 4) {
        CFReleaseNull(gcmkey);
        return false;
    }

    oldpw = CFDataCreateMutable(NULL, outLength);
    if (oldpw == NULL) {
        CFReleaseNull(gcmkey);
        return false;
    }
    CFDataSetLength(oldpw, outLength);
    

    /*
     * Create data for password
     */
    
    pw = CFStringCreateExternalRepresentation(NULL, inPassword, kCFStringEncodingUTF8, 0);
    if (pw == NULL) {
        CFReleaseNull(oldpw);
        CFReleaseNull(gcmkey);
        return false;
    }
    
    /*
     * Wrapping key is HMAC(sha256) over password
     */

    if (di->output_size < kKeySize) abort();
    
    uint8_t rawkey[di->output_size];
    
    memcpy(&size, CFDataGetMutableBytePtr(gcmkey) + kKeySize + kSaltSize, sizeof(size));
    size = ntohl(size);
    
    if (ccpbkdf2_hmac(di, CFDataGetLength(pw), CFDataGetBytePtr(pw),
                      kSaltSize, CFDataGetMutableBytePtr(gcmkey) + kKeySize,
                      size,
                      sizeof(rawkey), rawkey) != 0)
        abort();

    CFReleaseNull(pw);
    
    /*
     * Unwrap the random key with one round of ECB cryto
     */

    ccecb_ctx_decl(ecb->size, ecbkey);
    ecb->init(ecb, ecbkey, kKeySize, rawkey);
    ecb->ecb(ecbkey, 1, CFDataGetMutableBytePtr(gcmkey), CFDataGetMutableBytePtr(gcmkey));
    
    /*
     * GCM unwrap
     */
    
    uint8_t tag[tagLen];
    ccgcm_ctx_decl(gcm.size, ctx);
    
    gcm.init(&gcm, ctx, kKeySize, CFDataGetMutableBytePtr(gcmkey));
    gcm.gmac(ctx, 1, CFDataGetBytePtr(inBreadcrumb));
    gcm.gcm(ctx, outLength, CFDataGetBytePtr(inBreadcrumb) + 1, CFDataGetMutableBytePtr(oldpw));
    gcm.finalize(ctx, tagLen, tag);
    ccgcm_ctx_clear(gcm.size, ctx);
    
    CFReleaseNull(gcmkey);
    
    if (memcmp(tag, CFDataGetBytePtr(inBreadcrumb) + 1 + outLength, tagLen) != 0) {
        CFReleaseNull(oldpw);
        return false;
    }
    
    memcpy(&size, CFDataGetMutableBytePtr(oldpw), sizeof(size));
    size = ntohl(size);
    if (size > outLength - 4) {
        CFReleaseNull(oldpw);
        return false;
    }
    memmove(CFDataGetMutableBytePtr(oldpw), CFDataGetMutableBytePtr(oldpw) + 4, size);
    CFDataSetLength(oldpw, size);
    
    *outPassword = CFStringCreateFromExternalRepresentation(NULL, oldpw, kCFStringEncodingUTF8);
    CFReleaseNull(oldpw);

    return true;
}

CFDataRef
SecBreadcrumbCreateNewEncryptedKey(CFStringRef oldPassword,
                                   CFStringRef newPassword,
                                   CFDataRef encryptedKey,
                                   CFErrorRef *outError)
{
    const struct ccmode_ecb *enc = ccaes_ecb_encrypt_mode();
    const struct ccmode_ecb *dec = ccaes_ecb_decrypt_mode();
    const struct ccdigest_info *di = ccsha256_di();
    CFMutableDataRef newEncryptedKey;
    CFDataRef newpw = NULL, oldpw = NULL;
    uint8_t rawkey[di->output_size];
    
    if (CFDataGetLength(encryptedKey) < kKeySize + kSaltSize + 4) {
        return NULL;
    }

    newEncryptedKey = CFDataCreateMutableCopy(NULL, 0, encryptedKey);
    if (newEncryptedKey == NULL) {
        return NULL;
    }
    
    oldpw = CFStringCreateExternalRepresentation(NULL, oldPassword, kCFStringEncodingUTF8, 0);
    if (oldpw == NULL) {
        CFReleaseNull(newEncryptedKey);
        return false;
    }

    newpw = CFStringCreateExternalRepresentation(NULL, newPassword, kCFStringEncodingUTF8, 0);
    if (newpw == NULL) {
        CFReleaseNull(newEncryptedKey);
        CFReleaseNull(oldpw);
        return false;
    }

    if (di->output_size < kKeySize) abort();
    
    /*
     * Unwrap with new key
     */

    uint32_t iter;
    
    memcpy(&iter, CFDataGetMutableBytePtr(newEncryptedKey) + kKeySize + kSaltSize, sizeof(iter));
    iter = ntohl(iter);
    
    if (ccpbkdf2_hmac(di, CFDataGetLength(oldpw), CFDataGetBytePtr(oldpw),
                      kSaltSize, CFDataGetMutableBytePtr(newEncryptedKey) + kKeySize,
                      iter,
                      sizeof(rawkey), rawkey) != 0)
        abort();
    
    CFReleaseNull(oldpw);

    
    ccecb_ctx_decl(dec->size, deckey);
    dec->init(dec, deckey, kKeySize, rawkey);
    dec->ecb(deckey, 1, CFDataGetMutableBytePtr(newEncryptedKey), CFDataGetMutableBytePtr(newEncryptedKey));

    memset(rawkey, 0, sizeof(rawkey));
   
    /*
     * Re-wrap with new key
     */
   
    if (ccpbkdf2_hmac(di, CFDataGetLength(newpw), CFDataGetBytePtr(newpw),
                      kSaltSize, CFDataGetMutableBytePtr(newEncryptedKey) + kKeySize,
                      iter,
                      sizeof(rawkey), rawkey) != 0)
        abort();
    
    CFReleaseNull(newpw);

    
    ccecb_ctx_decl(enc->size, enckey);
    enc->init(enc, enckey, kKeySize, rawkey);
    enc->ecb(enckey, 1, CFDataGetMutableBytePtr(newEncryptedKey), CFDataGetMutableBytePtr(newEncryptedKey));

    memset(rawkey, 0, sizeof(rawkey));

    return newEncryptedKey;
}
