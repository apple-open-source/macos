/*
 * Copyright (c) 2010,2011 Apple Inc. All Rights Reserved.
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

/*
 * SecECKey.c - CoreFoundation based rsa key object
 */

#include "SecECKey.h"

#include <Security/SecKeyInternal.h>
#include <Security/SecItem.h>
#include <Security/SecBasePriv.h>
#include <AssertMacros.h>
#include <Security/SecureTransport.h> /* For error codes. */
#include <CoreFoundation/CFData.h> /* For error codes. */
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <CoreFoundation/CFNumber.h>
#include <Security/SecFramework.h>
#include <Security/SecRandom.h>
#include <security_utilities/debugging.h>
#include "SecItemPriv.h"
#include <Security/SecInternal.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccrng.h>

#define kMaximumECKeySize 521

static CFIndex SecECKeyGetAlgorithmID(SecKeyRef key) {
    return kSecECDSAAlgorithmID;
}



/*
 *
 * Public Key
 *
 */

/* Public key static functions. */
static void SecECPublicKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    ccec_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    if (ccec_ctx_cp(pubkey).zp)
        cc_zero(ccec_pub_ctx_size(ccn_sizeof_n(ccec_ctx_n(pubkey))), pubkey.pub);
}

static ccec_const_cp_t getCPForBits(size_t bits)
{
    switch (bits) {
        case 192:
            return ccec_cp_192();
        case 224:
            return ccec_cp_224();
        case 256:
            return ccec_cp_256();
        case 384:
            return ccec_cp_384();
        case 521:
            return ccec_cp_521();
        default:
        {
            ccec_const_cp_t nullCP = { .zp = NULL };
            return nullCP;
        }
    }
}
static ccec_const_cp_t getCPForPublicSize(CFIndex publicLength)
{
    ccec_const_cp_t cp;
    switch (publicLength) {
        case 49:
            cp = ccec_cp_192();
            break;
        case 57:
            cp = ccec_cp_224();
            break;
        case 65:
            cp = ccec_cp_256();
            break;
        case 97:
            cp = ccec_cp_384();
            break;
        case 133:
            cp = ccec_cp_521();
            break;
        default:
        {
            ccec_const_cp_t nullCP = { .zp = NULL };
            return nullCP;
        }
    }
    return cp;
}

static ccec_const_cp_t getCPForPrivateSize(CFIndex publicLength)
{
    ccec_const_cp_t cp;
    switch (publicLength) {
        case 49 + 24:
            cp = ccec_cp_192();
            break;
        case 57 + 28:
            cp = ccec_cp_224();
            break;
        case 65 + 32:
            cp = ccec_cp_256();
            break;
        case 97 + 48:
            cp = ccec_cp_384();
            break;
        case 133 + 66:
            cp = ccec_cp_521();
            break;
        default:
        {
            ccec_const_cp_t nullCP = { .zp = NULL };
            return nullCP;
        }
    }
    return cp;
}

static OSStatus SecECPublicKeyInit(SecKeyRef key,
    const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    ccec_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    OSStatus err = errSecParam;

    switch (encoding) {
    case kSecDERKeyEncoding:
    {
        const SecDERKey *derKey = (const SecDERKey *)keyData;
        if (keyDataLength != sizeof(SecDERKey)) {
            err = errSecDecode;
            break;
        }

        ccec_const_cp_t cp = getCPForPublicSize(derKey->keyLength);

        /* TODO: Parse and use real params from passed in derKey->algId.params */
        err = (ccec_import_pub(cp, derKey->keyLength, derKey->key, pubkey)
               ? errSecDecode : errSecSuccess);
        break;
    }
    case kSecKeyEncodingBytes:
    {
        ccec_const_cp_t cp = getCPForPublicSize(keyDataLength);
        err = (ccec_import_pub(cp, keyDataLength, keyData, pubkey)
               ? errSecDecode : errSecSuccess);
        break;
    }
    case kSecExtractPublicFromPrivate:
    {
        ccec_full_ctx_t fullKey;
        fullKey._full = (ccec_full_ctx *) keyData;

        cc_size fullKeyN = ccec_ctx_n(fullKey);
        require(fullKeyN <= ccn_nof(kMaximumECKeySize), errOut);
        memcpy(pubkey._pub, fullKey.pub, ccec_pub_ctx_size(ccn_sizeof_n(fullKeyN)));
        err = errSecSuccess;
        break;
    }
    case kSecKeyEncodingApplePkcs1:
    default:
        err = errSecParam;
        break;
    }

errOut:
    return err;
}

static OSStatus SecECPublicKeyRawVerify(SecKeyRef key, SecPadding padding,
    const uint8_t *signedData, size_t signedDataLen,
    const uint8_t *sig, size_t sigLen) {
    int err = errSSLCrypto; // TODO: Should be errSecNotSigner;
    ccec_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    bool valid = 0;

    if (ccec_verify(pubkey, signedDataLen, signedData, sigLen, sig, &valid))
        err = errSSLCrypto; // TODO: This seems weird. Shouldn't be SSL error
    if (valid)
        err = errSecSuccess;

    return err;
}

static OSStatus SecECPublicKeyRawEncrypt(SecKeyRef key, SecPadding padding,
    const uint8_t *plainText, size_t plainTextLen,
	uint8_t *cipherText, size_t *cipherTextLen) {
    ccec_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    int err = errSecUnimplemented;

#if 0
    require_noerr(err = ccec_wrap_key(pubkey, &ccsha256_di,
                                      plainTextLen, plainText, cipherText), errOut);

errOut:
#endif
    return err;
}

static size_t SecECPublicKeyBlockSize(SecKeyRef key) {
    /* Get key size in octets */
    ccec_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    return ccec_ccn_size(ccec_ctx_cp(pubkey));
}

/* Encode the public key and return it in a newly allocated CFDataRef. */
static CFDataRef SecECPublicKeyExport(CFAllocatorRef allocator,
	ccec_pub_ctx_t pubkey) {
    size_t pub_size = ccec_export_pub_size(pubkey);
	CFMutableDataRef blob = CFDataCreateMutable(allocator, pub_size);
    if (blob) {
        CFDataSetLength(blob, pub_size);
        ccec_export_pub(pubkey, CFDataGetMutableBytePtr(blob));
    }

	return blob;
}

static OSStatus SecECPublicKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serailziation)
{
    ccec_pub_ctx_t pubkey;
    pubkey.pub = key->key;

	CFAllocatorRef allocator = CFGetAllocator(key);
    *serailziation = SecECPublicKeyExport(allocator, pubkey);

    if (NULL == *serailziation)
        return errSecDecode;
    else
        return errSecSuccess;
}

static CFDictionaryRef SecECPublicKeyCopyAttributeDictionary(SecKeyRef key) {
    return SecKeyGeneratePublicAttributeDictionary(key, kSecAttrKeyTypeEC);
}

SecKeyDescriptor kSecECPublicKeyDescriptor = {
    kSecKeyDescriptorVersion,
    "ECPublicKey",
    ccec_pub_ctx_size(ccn_sizeof(kMaximumECKeySize)), /* extraBytes */
    SecECPublicKeyInit,
    SecECPublicKeyDestroy,
    NULL, /* SecKeyRawSignMethod */
    SecECPublicKeyRawVerify,
    SecECPublicKeyRawEncrypt,
    NULL, /* SecKeyDecryptMethod */
    NULL, /* SecKeyComputeMethod */
    SecECPublicKeyBlockSize,
	SecECPublicKeyCopyAttributeDictionary,
    SecECKeyGetAlgorithmID,
    SecECPublicKeyCopyPublicOctets,
};

/* Public Key API functions. */
SecKeyRef SecKeyCreateECPublicKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecECPublicKeyDescriptor, keyData,
        keyDataLength, encoding);
}



/*
 *
 * Private Key
 *
 */

/* Private key static functions. */
static void SecECPrivateKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    ccec_full_ctx_t fullkey;
    fullkey.hdr = key->key;
    if (ccec_ctx_cp(fullkey).zp)
        cc_zero(ccec_full_ctx_size(ccn_sizeof_n(ccec_ctx_n(fullkey))), fullkey.hdr);
}


static OSStatus SecECPrivateKeyInit(SecKeyRef key,
    const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    ccec_full_ctx_t fullkey;
    fullkey.hdr = key->key;
    OSStatus err = errSecParam;

    switch (encoding) {
    case kSecKeyEncodingPkcs1:
        /* TODO: DER import size (and thus cp), pub.x, pub.y and k. */
        //err = ecc_import(keyData, keyDataLength, fullkey);
        break;
    case kSecKeyEncodingBytes:
    {
        ccec_const_cp_t cp = getCPForPrivateSize(keyDataLength);
        require(cp.zp != NULL, abort);

        ccec_ctx_init(cp, fullkey);
        size_t pubSize = ccec_export_pub_size(fullkey);

        require(pubSize < (size_t) keyDataLength, abort);
        require_noerr_action(ccec_import_pub(cp, pubSize, keyData, fullkey),
                             abort,
                             err = errSecDecode);


        keyData += pubSize;
        keyDataLength -= pubSize;

        cc_unit *k = ccec_ctx_k(fullkey);
        require_noerr_action(ccn_read_uint(ccec_ctx_n(fullkey), k, keyDataLength, keyData),
                             abort,
                             err = errSecDecode);

        err = errSecSuccess;
        break;

    }
    case kSecGenerateKey:
    {
        CFDictionaryRef parameters = (CFDictionaryRef) keyData;

        CFTypeRef ksize = CFDictionaryGetValue(parameters, kSecAttrKeySizeInBits);
        CFIndex keyLengthInBits = getIntValue(ksize);

        ccec_const_cp_t cp = getCPForBits(keyLengthInBits);

        if (!cp.zp) {
            secwarning("Invalid or missing key size in: %@", parameters);
            return errSecKeySizeNotAllowed;
        }

        if (!ccec_generate_key(cp, ccrng_seckey, fullkey))
            err = errSecSuccess;
        break;
    }

    default:
        break;
    }
abort:
    return err;
}

static OSStatus SecECPrivateKeyRawSign(SecKeyRef key, SecPadding padding,
    const uint8_t *dataToSign, size_t dataToSignLen,
    uint8_t *sig, size_t *sigLen) {
    ccec_full_ctx_t fullkey;
    fullkey.hdr = key->key;
    int err;;
    check(sigLen);

    require_noerr(err = ccec_sign(fullkey, dataToSignLen, dataToSign,
                                  sigLen, sig, ccrng_seckey), errOut);
errOut:

    return err;
}

#if 0
static const struct ccdigest_info *
ccdigest_lookup_by_oid(unsigned long oid_size, const void *oid) {
    static const struct ccdigest_info *dis[] = {
        &ccsha1_di,
        &ccsha224_di,
        &ccsha256_di,
        &ccsha384_di,
        &ccsha512_di
    };
    size_t i;
    for (i = 0; i < sizeof(dis) / sizeof(*dis); ++i) {
        if (oid_size == dis[i]->oid_size && !memcmp(dis[i]->oid, oid, oid_size))
            return dis[i];
    }
    return NULL;
}
#endif

static OSStatus SecECPrivateKeyRawDecrypt(SecKeyRef key, SecPadding padding,
	const uint8_t *cipherText, size_t cipherTextLen,
	uint8_t *plainText, size_t *plainTextLen) {
    ccec_full_ctx_t fullkey;
    fullkey.hdr = key->key;
    int err = errSecUnimplemented;

#if 0
    err = ccec_unwrap_key(fullkey, ccrng_seckey, ccdigest_lookup_by_oid,
                          cipherTextLen, cipherText, plainTextLen, plainText);
#endif

    return err;
}

static size_t SecECPrivateKeyBlockSize(SecKeyRef key) {
    ccec_full_ctx_t fullkey;
    fullkey.hdr = key->key;
    /* Get key size in octets */
    return ccec_ccn_size(ccec_ctx_cp(fullkey));
}

static OSStatus SecECPrivateKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serailziation)
{
    ccec_full_ctx_t fullkey;
    fullkey.hdr = key->key;

	CFAllocatorRef allocator = CFGetAllocator(key);
    *serailziation = SecECPublicKeyExport(allocator, fullkey);

    if (NULL == *serailziation)
        return errSecDecode;
    else
        return errSecSuccess;
}

static CFDataRef SecECPPrivateKeyExport(CFAllocatorRef allocator,
                                        ccec_full_ctx_t fullkey) {
    size_t prime_size = ccec_cp_prime_size(ccec_ctx_cp(fullkey));
    size_t key_size = ccec_export_pub_size(fullkey) + prime_size;
	CFMutableDataRef blob = CFDataCreateMutable(allocator, key_size);
    if (blob) {
        CFDataSetLength(blob, key_size);
        ccec_export_pub(fullkey, CFDataGetMutableBytePtr(blob));
        UInt8 *dest = CFDataGetMutableBytePtr(blob) + ccec_export_pub_size(fullkey);
        const cc_unit *k = ccec_ctx_k(fullkey);
        ccn_write_uint_padded(ccec_ctx_n(fullkey), k, prime_size, dest);
    }

	return blob;
}


static CFDictionaryRef SecECPrivateKeyCopyAttributeDictionary(SecKeyRef key) {
	CFDictionaryRef dict = NULL;
	CFAllocatorRef allocator = CFGetAllocator(key);

    ccec_full_ctx_t fullkey;
    fullkey.hdr = key->key;

	CFDataRef fullKeyBlob = NULL;

	/* Export the full ec key pair. */
	require(fullKeyBlob = SecECPPrivateKeyExport(allocator, fullkey), errOut);

	dict = SecKeyGeneratePrivateAttributeDictionary(key, kSecAttrKeyTypeEC, fullKeyBlob);

errOut:
	CFReleaseSafe(fullKeyBlob);

	return dict;
}

SecKeyDescriptor kSecECPrivateKeyDescriptor = {
    kSecKeyDescriptorVersion,
    "ECPrivateKey",
    ccec_full_ctx_size(ccn_sizeof(kMaximumECKeySize)), /* extraBytes */
    SecECPrivateKeyInit,
    SecECPrivateKeyDestroy,
    SecECPrivateKeyRawSign,
    NULL, /* SecKeyRawVerifyMethod */
    NULL, /* SecKeyEncryptMethod */
    SecECPrivateKeyRawDecrypt,
    NULL, /* SecKeyComputeMethod */
    SecECPrivateKeyBlockSize,
	SecECPrivateKeyCopyAttributeDictionary,
    SecECKeyGetAlgorithmID,
    SecECPrivateKeyCopyPublicOctets,
};

/* Private Key API functions. */
SecKeyRef SecKeyCreateECPrivateKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecECPrivateKeyDescriptor, keyData,
        keyDataLength, encoding);
}


OSStatus SecECKeyGeneratePair(CFDictionaryRef parameters,
                              SecKeyRef *publicKey, SecKeyRef *privateKey) {
    OSStatus status = errSecParam;

    CFAllocatorRef allocator = NULL; /* @@@ get from parameters. */
    SecKeyRef pubKey = NULL;

    SecKeyRef privKey = SecKeyCreate(allocator, &kSecECPrivateKeyDescriptor,
                                     (const void*) parameters, 0, kSecGenerateKey);

    require(privKey, errOut);

    /* Create SecKeyRef's from the pkcs1 encoded keys. */
    pubKey = SecKeyCreate(allocator, &kSecECPublicKeyDescriptor,
                                    privKey->key, 0, kSecExtractPublicFromPrivate);

    require(pubKey, errOut);

    if (publicKey) {
        *publicKey = pubKey;
        pubKey = NULL;
    }
    if (privateKey) {
        *privateKey = privKey;
        privKey = NULL;
    }

    status = errSecSuccess;

errOut:
    CFReleaseSafe(pubKey);
    CFReleaseSafe(privKey);

    return status;
}


/* It's debatable whether this belongs here or in the ssl code since the
   curve values come from a tls related rfc4492. */
SecECNamedCurve SecECKeyGetNamedCurve(SecKeyRef key) {
    if (key->key_class != &kSecECPublicKeyDescriptor &&
        key->key_class != &kSecECPrivateKeyDescriptor)
        return kSecECCurveNone;

    ccec_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    switch (ccec_cp_prime_size(ccec_ctx_cp(pubkey))) {
#if 0
    case 24:
        return kSecECCurveSecp192r1;
    case 28:
        return kSecECCurveSecp224r1;
#endif
    case 32:
        return kSecECCurveSecp256r1;
    case 48:
        return kSecECCurveSecp384r1;
    case 66:
        return kSecECCurveSecp521r1;
    }
    return kSecECCurveNone;
}

CFDataRef SecECKeyCopyPublicBits(SecKeyRef key) {
    if (key->key_class != &kSecECPublicKeyDescriptor &&
        key->key_class != &kSecECPrivateKeyDescriptor)
        return NULL;

    ccec_pub_ctx_t pubkey;
    pubkey.pub = key->key;
    return SecECPublicKeyExport(CFGetAllocator(key), pubkey);
}
