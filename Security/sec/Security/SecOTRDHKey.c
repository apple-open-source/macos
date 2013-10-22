//
//  SecOTRDHKey.c
//  libsecurity_libSecOTR
//
//  Created by Mitch Adler on 3/2/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//
#include "SecOTRDHKey.h"
#include <utilities/SecCFWrappers.h>

#include "SecOTRMath.h"
#include "SecOTRPacketData.h"

#include <corecrypto/ccn.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccec_priv.h>
#include <corecrypto/ccec.h>

#include <CommonCrypto/CommonRandomSPI.h>

#ifdef USECOMMONCRYPTO
#include <CommonCrypto/CommonDigest.h>
#endif

#define kECKeySize 256

struct _SecOTRFullDHKey {
    CFRuntimeBase _base;

    ccec_full_ctx_decl(ccn_sizeof(kECKeySize), _key);
    uint8_t keyHash[CCSHA1_OUTPUT_SIZE];

};

CFGiblisFor(SecOTRFullDHKey);

static size_t AppendECPublicKeyAsDATA(CFMutableDataRef data, ccec_pub_ctx_t public_key)
{
    size_t size = ccec_export_pub_size(public_key);

    AppendLong(data, (uint32_t)size); /* cast: no overflow, pub size always fit in 32 bits */
    ccec_export_pub(public_key, CFDataIncreaseLengthAndGetMutableBytes(data, (CFIndex)size));

    return size;
}


static CF_RETURNS_RETAINED CFStringRef SecOTRFullDHKeyCopyDescription(CFTypeRef cf)
{
    SecOTRFullDHKeyRef session = (SecOTRFullDHKeyRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecOTRFullDHKeyRef: %p>"), session);
}

static void SecOTRFullDHKeyDestroy(CFTypeRef cf)
{
    SecOTRFullDHKeyRef fullKey = (SecOTRFullDHKeyRef)cf;
    
    bzero(fullKey->_key, sizeof(fullKey->_key));
}

SecOTRFullDHKeyRef SecOTRFullDHKCreate(CFAllocatorRef allocator)
{
    SecOTRFullDHKeyRef newFDHK = CFTypeAllocate(SecOTRFullDHKey, struct _SecOTRFullDHKey, allocator);

    SecFDHKNewKey(newFDHK);
    
    return newFDHK;
}

SecOTRFullDHKeyRef SecOTRFullDHKCreateFromBytes(CFAllocatorRef allocator, const uint8_t**bytes, size_t*size)
{
    SecOTRFullDHKeyRef newFDHK = CFTypeAllocate(SecOTRFullDHKey, struct _SecOTRFullDHKey, allocator);

    ccec_ctx_init(ccec_cp_256(), newFDHK->_key);

    uint32_t publicKeySize;
    require_noerr(ReadLong(bytes, size, &publicKeySize), fail);

    require(publicKeySize <= *size, fail);
    require_noerr(ccec_import_pub(ccec_cp_256(), publicKeySize, *bytes, newFDHK->_key), fail);
    ccdigest(ccsha1_di(), publicKeySize, *bytes, newFDHK->keyHash);
    
    *size -= publicKeySize;
    *bytes += publicKeySize;

    require_noerr(ReadMPI(bytes, size, ccec_ctx_n(newFDHK->_key), ccec_ctx_k(newFDHK->_key)), fail);

    return newFDHK;

fail:
    CFReleaseNull(newFDHK);
    return NULL;
}

void SecFDHKNewKey(SecOTRFullDHKeyRef fullKey)
{
    struct ccrng_state *rng=ccDRBGGetRngState();

#if defined(CCECDH_AVAILABLE)
    ccecdh_generate_key(ccec_cp_256(), rng, fullKey->_key);
#else
    ccec_generate_key(ccec_cp_256(), rng, fullKey->_key);
#endif
    
    size_t size = ccec_export_pub_size(fullKey->_key);
    uint8_t publicKey[size];

    ccec_export_pub(fullKey->_key, publicKey);
    ccdigest(ccsha1_di(), size, publicKey, fullKey->keyHash);
}

void SecFDHKAppendSerialization(SecOTRFullDHKeyRef fullKey, CFMutableDataRef appendTo)
{
    AppendECPublicKeyAsDATA(appendTo, fullKey->_key);
    AppendMPI(appendTo, ccec_ctx_n(fullKey->_key), ccec_ctx_k(fullKey->_key));
}


void SecFDHKAppendPublicSerialization(SecOTRFullDHKeyRef fullKey, CFMutableDataRef appendTo)
{
    if(ccec_ctx_bitlen(fullKey->_key) != kECKeySize) return;
    AppendECPublicKeyAsDATA(appendTo, fullKey->_key);
}


uint8_t* SecFDHKGetHash(SecOTRFullDHKeyRef fullKey)
{
    return fullKey->keyHash;
}




//
//
//
struct _SecOTRPublicDHKey {
    CFRuntimeBase _base;

    ccec_pub_ctx_decl(ccn_sizeof(kECKeySize), _key);
    uint8_t keyHash[CCSHA1_OUTPUT_SIZE];

};

CFGiblisFor(SecOTRPublicDHKey);

static CF_RETURNS_RETAINED CFStringRef SecOTRPublicDHKeyCopyDescription(CFTypeRef cf) {
    SecOTRPublicDHKeyRef session = (SecOTRPublicDHKeyRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecOTRPublicDHKeyRef: %p>"), session);
}

static void SecOTRPublicDHKeyDestroy(CFTypeRef cf) {
    SecOTRPublicDHKeyRef pubKey = (SecOTRPublicDHKeyRef)cf;
    (void) pubKey;
}

static void ccec_copy_public(ccec_pub_ctx_t source, ccec_pub_ctx_t dest)
{
    ccec_ctx_cp(dest) = ccec_ctx_cp(source);
    // TODO: +1?!
    ccn_set(3*ccec_ctx_n(source), (cc_unit*) ccec_ctx_point(dest)._p, (cc_unit*) ccec_ctx_point(source)._p);
}

SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromFullKey(CFAllocatorRef allocator, SecOTRFullDHKeyRef full)
{
    SecOTRPublicDHKeyRef newPDHK = CFTypeAllocate(SecOTRPublicDHKey, struct _SecOTRPublicDHKey, allocator);
    
    ccec_copy_public(full->_key, newPDHK->_key);
    memcpy(newPDHK->keyHash, full->keyHash, CCSHA1_OUTPUT_SIZE);

    return newPDHK;
}

SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromSerialization(CFAllocatorRef allocator, const uint8_t** bytes, size_t *size)
{
    SecOTRPublicDHKeyRef newPDHK = CFTypeAllocate(SecOTRPublicDHKey, struct _SecOTRPublicDHKey, allocator);

    uint32_t publicKeySize;
    require_noerr(ReadLong(bytes, size, &publicKeySize), fail);

    require(publicKeySize <= *size, fail);

    require_noerr(ccec_import_pub(ccec_cp_256(), publicKeySize, *bytes, newPDHK->_key), fail);
    ccdigest(ccsha1_di(), publicKeySize, *bytes, newPDHK->keyHash);

    *size -= publicKeySize;
    *bytes += publicKeySize;

    return newPDHK;
fail:
    CFReleaseNull(newPDHK);
    return NULL;
}

SecOTRPublicDHKeyRef SecOTRPublicDHKCreateFromBytes(CFAllocatorRef allocator, const uint8_t** bytes, size_t *size)
{
    SecOTRPublicDHKeyRef newPDHK = CFTypeAllocate(SecOTRPublicDHKey, struct _SecOTRPublicDHKey, allocator);
    
    require_noerr(ccec_import_pub(ccec_cp_256(), *size, *bytes, newPDHK->_key), fail);
    ccdigest(ccsha1_di(), *size, *bytes, newPDHK->keyHash);

    return newPDHK;
fail:
    CFReleaseNull(newPDHK);
    return NULL;
}

void SecPDHKAppendSerialization(SecOTRPublicDHKeyRef pubKey, CFMutableDataRef appendTo)
{
    AppendECPublicKeyAsDATA(appendTo, pubKey->_key);
}


uint8_t* SecPDHKGetHash(SecOTRPublicDHKeyRef pubKey)
{
    return pubKey->keyHash;
}


void SecPDHKeyGenerateS(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey, cc_unit* s)
{
    ccn_zero(kExponentiationUnits, s);

    size_t keyLen = ccn_sizeof_n(kExponentiationUnits);
    ccec_compute_key(myKey->_key, theirKey->_key, &keyLen, (uint8_t*)s);
}

static int ccec_cmp(ccec_pub_ctx_t l, ccec_pub_ctx_t r)
{
    size_t lsize = ccec_export_pub_size(l);
    size_t rsize = ccec_export_pub_size(r);
    
    int result = 0;
    
    if (lsize == rsize) {
        uint8_t lpub[lsize];
        uint8_t rpub[rsize];
        
        ccec_export_pub(l, lpub);
        ccec_export_pub(r, rpub);
        
        result = memcmp(lpub, rpub, lsize);
    } else {
        result = rsize < lsize ? -1 : 1;
    }
    
    return result;
}

bool SecDHKIsGreater(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey)
{
    return ccec_cmp(myKey->_key, theirKey->_key) > 0;
}

static void DeriveKeys(CFDataRef dataToHash,
                       uint8_t* messageKey,
                       uint8_t* macKey)
{
    if (messageKey == NULL && macKey == NULL)
        return;

    uint8_t hashedSharedKey[CCSHA1_OUTPUT_SIZE];

#ifdef USECOMMONCRYPTO
    (void) CCDigest(kCCDigestSHA1, CFDataGetBytePtr(dataToHash), (uint32_t)CFDataGetLength(dataToHash), hashedSharedKey);
#else
    ccdigest(ccsha1_di(), CFDataGetLength(dataToHash), CFDataGetBytePtr(dataToHash), hashedSharedKey);
#endif

    if (messageKey)
        memcpy(messageKey, hashedSharedKey, kOTRMessageKeyBytes);
    
    if (macKey) {
#ifdef USECOMMONCRYPTO
        (void) CCDigest(kCCDigestSHA1, messageKey, kOTRMessageKeyBytes, macKey);
#else
        ccdigest(ccsha1_di(), kOTRMessageKeyBytes, messageKey, macKey);
#endif
    }
    
    bzero(hashedSharedKey, sizeof(hashedSharedKey));
}

void SecOTRDHKGenerateOTRKeys(SecOTRFullDHKeyRef myKey, SecOTRPublicDHKeyRef theirKey,
                           uint8_t* sendMessageKey, uint8_t* sendMacKey,
                           uint8_t* receiveMessageKey, uint8_t* receiveMacKey)
{
    CFMutableDataRef dataToHash = CFDataCreateMutable(kCFAllocatorDefault, 0);

    {
        cc_unit s[kExponentiationUnits];

        SecPDHKeyGenerateS(myKey, theirKey, s);
        AppendByte(dataToHash, SecDHKIsGreater(myKey, theirKey) ? 0x01 : 0x02);
        AppendMPI(dataToHash, kExponentiationUnits, s);

        ccn_zero(kExponentiationUnits, s);
    }

    DeriveKeys(dataToHash, receiveMessageKey, receiveMacKey);

    uint8_t *messageTypeByte = CFDataGetMutableBytePtr(dataToHash);

    *messageTypeByte ^= 0x03; // Invert the bits since it's either 1 or 2.

    DeriveKeys(dataToHash, sendMessageKey, sendMacKey);

    CFReleaseNull(dataToHash);
}
