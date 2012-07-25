/*
 *  SecPBKDF.h
 *
 *  Copyright 2010 Apple Inc. All rights reserved.
 *
 */

#include <CoreFoundation/CFData.h>

#include <CommonCrypto/CommonHMAC.h>

/* CC Based HMAC PRF functions */
void hmac_sha1_PRF(const uint8_t *key,
                   size_t key_len,
                   const uint8_t *text,
                   size_t text_len,
                   uint8_t digest[CC_SHA1_DIGEST_LENGTH]);


/* PBKDF for clients who want to let us allocate the intermediate buffer.
   We over write any intermediate results we use in calculating */
void pbkdf2_hmac_sha1(const uint8_t *passwordPtr, size_t passwordLen,
                      const uint8_t *saltPtr, size_t saltLen,
                      uint32_t iterationCount,
                      void *dkPtr, size_t dkLen);



/* Transformation conveninces from and to CFData where the password bytes used are the UTF-8 representation and 1000 iterations

   This routine promises not to make any copies of the password or salt that aren't
   eradicated before completion.
   
   The size of the result buffer is used to produce the derivedKey.
   
   Be careful when using CFTypes for secrets, they tend to copy data more than you'd like.
   If your password and or salt aren't already in CF types use the buffer versions above.
   
   If you already have the data in this form, the interface will unwrap and not copy the data anywhere extra for you.

   void SecKeyFromPassword_HMAC_sha1(CFDataRef password, CFDataRef salt, uint32_t interationCount, CFMutableDataRef derivedKey)
   {
        pbkdf2_hmac_sha1(CFDataGetBytePtr(password), CFDataGetLength(password),
                         CFDataGetBytePtr(salt), CFDataGetLength(salt),
                         interationCount,
                         CFDataGetMutableBytePtr(derivedKey), CFDataGetLength(derivedKey));

   }
   
   Suggested way to transform strings into data:
   
    CFDataRef   *passwordData    = CFStringCreateExternalRepresentation(NULL, password, kCFStringEncodingUTF8, 0);

    ...

    CFReleaseSafe(passwordData);

*/

void SecKeyFromPassphraseDataHMACSHA1(CFDataRef password, CFDataRef salt, uint32_t interationCount, CFMutableDataRef derivedKey);
