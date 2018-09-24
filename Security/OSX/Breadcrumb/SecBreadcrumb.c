/*
 * Copyright (c) 2014 - 2016 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <Security/Security.h>
#include <Security/SecBreadcrumb.h>
#include <Security/SecRandom.h>

#include <corecrypto/ccaes.h>
#include <corecrypto/ccpbkdf2.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/ccmode_factory.h>
#include <corecrypto/ccsha2.h>

#include <CommonCrypto/CommonRandomSPI.h>

#import "SecCFAllocator.h"

#define CFReleaseNull(CF) ({ __typeof__(CF) *const _pcf = &(CF), _cf = *_pcf; (_cf ? (*_pcf) = ((__typeof__(CF))0), (CFRelease(_cf), ((__typeof__(CF))0)) : _cf); })

#define kBCKeySize CCAES_KEY_SIZE_128
#define kBCSaltSize 20
#define kBCIterations 5000
#define BCTagLen 16
#define BCIVLen 16
#define BCversion1 1
#define BCversion2 2
#define BCPaddingSize 256
#define BCMaxSize 1024

Boolean
SecBreadcrumbCreateFromPassword(CFStringRef inPassword,
                                CFDataRef *outBreadcrumb,
                                CFDataRef *outEncryptedKey,
                                CFErrorRef *outError)
{
    const struct ccmode_ecb *ecb = ccaes_ecb_encrypt_mode();
    const struct ccmode_gcm *gcm = ccaes_gcm_encrypt_mode();
    uint8_t iv[BCIVLen];
    CFMutableDataRef key, npw;
    CFDataRef pw;
    
    *outBreadcrumb = NULL;
    *outEncryptedKey = NULL;
    if (outError)
        *outError = NULL;
    
    key = CFDataCreateMutable(SecCFAllocatorZeroize(), 0);
    if (key == NULL)
        return false;
    
    CFDataSetLength(key, kBCKeySize + kBCSaltSize + 4);
    if (SecRandomCopyBytes(kSecRandomDefault, CFDataGetLength(key) - 4, CFDataGetMutableBytePtr(key)) != 0) {
        CFReleaseNull(key);
        return false;
    }
    if (SecRandomCopyBytes(kSecRandomDefault, BCIVLen, iv) != 0) {
        CFReleaseNull(key);
        return false;
    }

    uint32_t size = htonl(kBCIterations);
    memcpy(CFDataGetMutableBytePtr(key) + kBCKeySize + kBCSaltSize, &size, sizeof(size));
    
    /*
     * Create data for password
     */
    
    pw = CFStringCreateExternalRepresentation(SecCFAllocatorZeroize(), inPassword, kCFStringEncodingUTF8, 0);
    if (pw == NULL) {
        CFReleaseNull(key);
        return false;
    }

    const CFIndex passwordLength = CFDataGetLength(pw);
    
    if (passwordLength > BCMaxSize) {
        CFReleaseNull(pw);
        CFReleaseNull(key);
        return false;
    }

    CFIndex paddedSize = passwordLength + BCPaddingSize - (passwordLength % BCPaddingSize);
    const CFIndex outLength = 1 + BCIVLen + 4 + paddedSize + BCTagLen;
    
    npw = CFDataCreateMutable(NULL, outLength);
    if (npw == NULL) {
        CFReleaseNull(pw);
        CFReleaseNull(key);
        return false;
    }
    CFDataSetLength(npw, outLength);

    cc_clear(outLength, CFDataGetMutableBytePtr(npw));
    CFDataGetMutableBytePtr(npw)[0] = BCversion2;
    memcpy(CFDataGetMutableBytePtr(npw) + 1, iv, BCIVLen);
    size = htonl(passwordLength);
    memcpy(CFDataGetMutableBytePtr(npw) + 1 + BCIVLen, &size, sizeof(size));
    memcpy(CFDataGetMutableBytePtr(npw) + 1 + BCIVLen + 4, CFDataGetBytePtr(pw), passwordLength);
    
    /*
     * Now create a GCM encrypted password using the random key
     */
    
    ccgcm_ctx_decl(gcm->size, ctx);
    ccgcm_init(gcm, ctx, kBCKeySize, CFDataGetMutableBytePtr(key));
    ccgcm_set_iv(gcm, ctx, BCIVLen, iv);
    ccgcm_gmac(gcm, ctx, 1, CFDataGetMutableBytePtr(npw));
    ccgcm_update(gcm, ctx, outLength - BCTagLen - BCIVLen - 1, CFDataGetMutableBytePtr(npw) + 1 + BCIVLen, CFDataGetMutableBytePtr(npw) + 1 + BCIVLen);
    ccgcm_finalize(gcm, ctx, BCTagLen, CFDataGetMutableBytePtr(npw) + outLength - BCTagLen);
    ccgcm_ctx_clear(gcm->size, ctx);
    
    /*
     * Wrapping key is PBKDF2(sha256) over password
     */
    
    const struct ccdigest_info *di = ccsha256_di();
    uint8_t rawkey[CCSHA256_OUTPUT_SIZE];
    _Static_assert(sizeof(rawkey) >= kBCKeySize, "keysize changed w/o updating digest");
    if (sizeof(rawkey) != di->output_size) abort();

    if (ccpbkdf2_hmac(di, CFDataGetLength(pw), CFDataGetBytePtr(pw),
                      kBCSaltSize, CFDataGetMutableBytePtr(key) + kBCKeySize,
                      kBCIterations,
                      sizeof(rawkey), rawkey) != 0)
        abort();
    
    /*
     * Wrap the random key with one round of ECB cryto
     */

    ccecb_ctx_decl(ccecb_context_size(ecb), ecbkey);
    ccecb_init(ecb, ecbkey, kBCKeySize, rawkey);
    ccecb_update(ecb, ecbkey, 1, CFDataGetMutableBytePtr(key), CFDataGetMutableBytePtr(key));
    ccecb_ctx_clear(ccecb_context_size(ecb), ecbkey);

    /*
     *
     */
    
    cc_clear(sizeof(rawkey), rawkey);
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
    CFMutableDataRef gcmkey, oldpw;
    CFIndex outLength;
    CFDataRef pw;
    uint32_t size;
    
    *outPassword = NULL;
    if (outError)
        *outError = NULL;
    
    if (CFDataGetLength(inEncryptedKey) < kBCKeySize + kBCSaltSize + 4) {
        return false;
    }
    
    if (CFDataGetBytePtr(inBreadcrumb)[0] == BCversion1) {
        if (CFDataGetLength(inBreadcrumb) < 1 + 4 + BCPaddingSize + BCTagLen)
            return false;

        outLength = CFDataGetLength(inBreadcrumb) - 1 - BCTagLen;
    } else if (CFDataGetBytePtr(inBreadcrumb)[0] == BCversion2) {
        if (CFDataGetLength(inBreadcrumb) < 1 + BCIVLen + 4 + BCPaddingSize + BCTagLen)
            return false;
        outLength = CFDataGetLength(inBreadcrumb) - 1 - BCIVLen - BCTagLen;
    } else {
        return false;
    }
    
    gcmkey = CFDataCreateMutableCopy(SecCFAllocatorZeroize(), 0, inEncryptedKey);
    if (gcmkey == NULL) {
        return false;
    }
    
    if ((outLength % 16) != 0 && outLength < 4) {
        CFReleaseNull(gcmkey);
        return false;
    }

    oldpw = CFDataCreateMutable(SecCFAllocatorZeroize(), outLength);
    if (oldpw == NULL) {
        CFReleaseNull(gcmkey);
        return false;
    }
    CFDataSetLength(oldpw, outLength);

    /*
     * Create data for password
     */
    
    pw = CFStringCreateExternalRepresentation(SecCFAllocatorZeroize(), inPassword, kCFStringEncodingUTF8, 0);
    if (pw == NULL) {
        CFReleaseNull(oldpw);
        CFReleaseNull(gcmkey);
        return false;
    }
    
    /*
     * Wrapping key is HMAC(sha256) over password
     */

    const struct ccdigest_info *di = ccsha256_di();
    uint8_t rawkey[CCSHA256_OUTPUT_SIZE];
    _Static_assert(sizeof(rawkey) >= kBCKeySize, "keysize changed w/o updating digest");
    if (sizeof(rawkey) != di->output_size) abort();

    memcpy(&size, CFDataGetMutableBytePtr(gcmkey) + kBCKeySize + kBCSaltSize, sizeof(size));
    size = ntohl(size);
    
    if (ccpbkdf2_hmac(di, CFDataGetLength(pw), CFDataGetBytePtr(pw),
                      kBCSaltSize, CFDataGetMutableBytePtr(gcmkey) + kBCKeySize,
                      size,
                      sizeof(rawkey), rawkey) != 0)
        abort();

    CFReleaseNull(pw);
    
    /*
     * Unwrap the random key with one round of ECB cryto
     */

    ccecb_ctx_decl(ccecb_context_size(ecb), ecbkey);
    ccecb_init(ecb, ecbkey, kBCKeySize, rawkey);
    ccecb_update(ecb, ecbkey, 1, CFDataGetMutableBytePtr(gcmkey), CFDataGetMutableBytePtr(gcmkey));
    ccecb_ctx_clear(ccecb_context_size(ecb), ecbkey);
    /*
     * GCM unwrap
     */

    uint8_t tag[BCTagLen];

    if (CFDataGetBytePtr(inBreadcrumb)[0] == BCversion1) {
        memcpy(tag, CFDataGetBytePtr(inBreadcrumb) + 1 + outLength, BCTagLen);

        ccgcm_one_shot_legacy(ccaes_gcm_decrypt_mode(), kBCKeySize,  CFDataGetMutableBytePtr(gcmkey), 0, NULL, 1, CFDataGetBytePtr(inBreadcrumb),
                              outLength, CFDataGetBytePtr(inBreadcrumb) + 1, CFDataGetMutableBytePtr(oldpw), BCTagLen, tag);
        if (memcmp(tag, CFDataGetBytePtr(inBreadcrumb) + 1 + outLength, BCTagLen) != 0) {
            CFReleaseNull(oldpw);
            CFReleaseNull(gcmkey);
            return false;
        }

    } else {
        const uint8_t *iv = CFDataGetBytePtr(inBreadcrumb) + 1;
        int res;
        memcpy(tag, CFDataGetBytePtr(inBreadcrumb) + 1 + BCIVLen + outLength, BCTagLen);

        res = ccgcm_one_shot(ccaes_gcm_decrypt_mode(), kBCKeySize, CFDataGetMutableBytePtr(gcmkey),
                             BCIVLen, iv,
                             1, CFDataGetBytePtr(inBreadcrumb),
                             outLength, CFDataGetBytePtr(inBreadcrumb) + 1 + BCIVLen, CFDataGetMutableBytePtr(oldpw),
                             BCTagLen, tag);
        if (res) {
            CFReleaseNull(gcmkey);
            CFReleaseNull(oldpw);
            CFReleaseNull(gcmkey);
            return false;
        }
    }

    CFReleaseNull(gcmkey);
    

    memcpy(&size, CFDataGetMutableBytePtr(oldpw), sizeof(size));
    size = ntohl(size);
    if ((ssize_t) size > outLength - 4) {
        CFReleaseNull(oldpw);
        return false;
    }
    memmove(CFDataGetMutableBytePtr(oldpw), CFDataGetMutableBytePtr(oldpw) + 4, size);
    CFDataSetLength(oldpw, size);
    
    *outPassword = CFStringCreateFromExternalRepresentation(SecCFAllocatorZeroize(), oldpw, kCFStringEncodingUTF8);
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
    uint8_t rawkey[CCSHA256_OUTPUT_SIZE];
    CFDataRef newpw = NULL, oldpw = NULL;
    CFMutableDataRef newEncryptedKey;

    _Static_assert(sizeof(rawkey) >= kBCKeySize, "keysize changed w/o updating digest");
    if (sizeof(rawkey) != di->output_size) abort();

    if (CFDataGetLength(encryptedKey) < kBCKeySize + kBCSaltSize + 4) {
        return NULL;
    }

    newEncryptedKey = CFDataCreateMutableCopy(SecCFAllocatorZeroize(), 0, encryptedKey);
    if (newEncryptedKey == NULL) {
        return NULL;
    }
    
    oldpw = CFStringCreateExternalRepresentation(SecCFAllocatorZeroize(), oldPassword, kCFStringEncodingUTF8, 0);
    if (oldpw == NULL) {
        CFReleaseNull(newEncryptedKey);
        return false;
    }

    newpw = CFStringCreateExternalRepresentation(SecCFAllocatorZeroize(), newPassword, kCFStringEncodingUTF8, 0);
    if (newpw == NULL) {
        CFReleaseNull(newEncryptedKey);
        CFReleaseNull(oldpw);
        return false;
    }

    /*
     * Unwrap with new key
     */

    uint32_t iter;
    
    memcpy(&iter, CFDataGetMutableBytePtr(newEncryptedKey) + kBCKeySize + kBCSaltSize, sizeof(iter));
    iter = ntohl(iter);
    
    if (ccpbkdf2_hmac(di, CFDataGetLength(oldpw), CFDataGetBytePtr(oldpw),
                      kBCSaltSize, CFDataGetMutableBytePtr(newEncryptedKey) + kBCKeySize,
                      iter,
                      sizeof(rawkey), rawkey) != 0)
        abort();
    
    CFReleaseNull(oldpw);

    
    ccecb_ctx_decl(dec->size, deckey);
    ccecb_init(dec, deckey, kBCKeySize, rawkey);
    ccecb_update(dec, deckey, 1, CFDataGetMutableBytePtr(newEncryptedKey), CFDataGetMutableBytePtr(newEncryptedKey));
    ccecb_ctx_clear(ccecb_context_size(dec), deckey);

    cc_clear(sizeof(rawkey), rawkey);

    /*
     * Re-wrap with new key
     */
   
    if (ccpbkdf2_hmac(di, CFDataGetLength(newpw), CFDataGetBytePtr(newpw),
                      kBCSaltSize, CFDataGetMutableBytePtr(newEncryptedKey) + kBCKeySize,
                      iter,
                      sizeof(rawkey), rawkey) != 0)
        abort();
    
    CFReleaseNull(newpw);

    
    ccecb_ctx_decl(enc->size, enckey);
    ccecb_init(enc, enckey, kBCKeySize, rawkey);
    ccecb_update(enc, enckey, 1, CFDataGetMutableBytePtr(newEncryptedKey), CFDataGetMutableBytePtr(newEncryptedKey));
    ccecb_ctx_clear(ccecb_context_size(enc), enckey);

    cc_clear(sizeof(rawkey), rawkey);

    return newEncryptedKey;
}
