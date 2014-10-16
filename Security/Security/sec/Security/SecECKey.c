/*
 * Copyright (c) 2010-2014 Apple Inc. All Rights Reserved.
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
#include "SecECKeyPriv.h"

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
#include <utilities/debugging.h>
#include "SecItemPriv.h"
#include <Security/SecInternal.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccder_decode_eckey.h>

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

static ccec_const_cp_t getCPForPublicSize(CFIndex encoded_length)
{
    size_t keysize = ccec_x963_import_pub_size(encoded_length);
    if(ccec_keysize_is_supported(keysize)) {
        return ccec_get_cp(keysize);
    }
    ccec_const_cp_t nullCP = { .zp = NULL };
    return nullCP;
}

static ccec_const_cp_t getCPForPrivateSize(CFIndex encoded_length)
{
    size_t keysize = ccec_x963_import_priv_size(encoded_length);
    if(ccec_keysize_is_supported(keysize)) {
        return ccec_get_cp(keysize);
    }
    ccec_const_cp_t nullCP = { .zp = NULL };
    return nullCP;
}

static ccoid_t ccoid_secp192r1 = CC_EC_OID_SECP192R1;
static ccoid_t ccoid_secp256r1 = CC_EC_OID_SECP256R1;
static ccoid_t ccoid_secp224r1 = CC_EC_OID_SECP224R1;
static ccoid_t ccoid_secp384r1 = CC_EC_OID_SECP384R1;
static ccoid_t ccoid_secp521r1 = CC_EC_OID_SECP521R1;

static ccec_const_cp_t ccec_cp_for_oid(ccoid_t oid)
{
    if (oid.oid) {
        if (ccoid_equal(oid, ccoid_secp192r1)) {
            return ccec_cp_192();
        } else if (ccoid_equal(oid, ccoid_secp256r1)) {
            return ccec_cp_256();
        } else if (ccoid_equal(oid, ccoid_secp224r1)) {
            return ccec_cp_224();
        } else if (ccoid_equal(oid, ccoid_secp384r1)) {
            return ccec_cp_384();
        } else if (ccoid_equal(oid, ccoid_secp521r1)) {
            return ccec_cp_521();
        }
    }
    return (ccec_const_cp_t){NULL};
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
    return ccec_ctx_size(pubkey);
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

static const char *
getCurveName(SecKeyRef key)
{
    SecECNamedCurve curveType = SecECKeyGetNamedCurve(key);

    switch (curveType)
    {
        case kSecECCurveSecp256r1:
            return "kSecECCurveSecp256r1";
            break;
        case kSecECCurveSecp384r1:
            return "kSecECCurveSecp384r1";
            break;
        case kSecECCurveSecp521r1:
            return "kSecECCurveSecp521r1";
        default:
            return "kSecECCurveNone";
    }
}

static CFStringRef SecECPublicKeyCopyKeyDescription(SecKeyRef key)
{
    ccec_pub_ctx_t ecPubkey;
    CFStringRef keyDescription = NULL;
    size_t xlen, ylen, ix;
    CFMutableStringRef xString = NULL;
    CFMutableStringRef yString = NULL;

    ecPubkey.pub = key->key;

    const char* curve = getCurveName(key);

    uint8_t *xunit = (uint8_t*)ccec_ctx_x(ecPubkey);
    require_quiet( NULL != xunit, fail);
    xlen = (size_t)strlen((char*)xunit);


    xString = CFStringCreateMutable(kCFAllocatorDefault, xlen * 2);
    require_quiet( NULL != xString, fail);

    for (ix = 0; ix < xlen; ++ix)
    {
		CFStringAppendFormat(xString, NULL, CFSTR("%02X"), xunit[ix]);
    }

    uint8_t *yunit = (uint8_t*)ccec_ctx_y(ecPubkey);
    require_quiet( NULL != yunit, fail);
    ylen = (size_t)strlen((char*)yunit);

    yString = CFStringCreateMutable(kCFAllocatorDefault, ylen*2);
    require_quiet( NULL != yString, fail);

    for(ix = 0; ix < ylen; ++ix)
    {
        CFStringAppendFormat(yString, NULL, CFSTR("%02X"), yunit[ix]);
    }

    keyDescription = CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR( "<SecKeyRef curve type: %s, algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, y: %@, x: %@, addr: %p>"), curve, (long)SecKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), yString, xString, key);

fail:
	CFReleaseSafe(xString);
	CFReleaseSafe(yString);
	if(!keyDescription)
		keyDescription = CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecKeyRef curve type: %s, algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, addr: %p>"), curve,(long)SecKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), key);

	return keyDescription;
}

static const struct ccec_rfc6637_curve * get_rfc6637_curve(SecKeyRef key)
{
    SecECNamedCurve curveType = SecECKeyGetNamedCurve(key);

    if (curveType == kSecECCurveSecp256r1) {
        return &ccec_rfc6637_dh_curve_p256;
    } else if (curveType == kSecECCurveSecp521r1) {
        return &ccec_rfc6637_dh_curve_p521;
    }
    return NULL;
}

static CFDataRef SecECKeyCopyWrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef unwrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    ccec_pub_ctx_t pubkey;
    int err = errSecUnimplemented;
    const struct ccec_rfc6637_curve *curve;
    const struct ccec_rfc6637_wrap *wrap = NULL;
    uint8_t sym_alg = 0;
    long flags = 0;

    pubkey.pub = key->key;

    if (type != kSecKeyWrapPublicKeyPGP) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unsupported key wrapping algorithm"));
        return NULL;
    }

    curve = get_rfc6637_curve(key);
    if (curve == NULL) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unsupported curve"));
        return NULL;
    }

    CFNumberRef num = CFDictionaryGetValue(parameters, _kSecKeyWrapPGPSymAlg);
    if (!isNumber(num) || !CFNumberGetValue(num, kCFNumberSInt8Type, &sym_alg)) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unknown symalg given"));
        return NULL;
    }

    CFDataRef fingerprint = CFDictionaryGetValue(parameters, _kSecKeyWrapPGPFingerprint);
    if (!isData(fingerprint) || CFDataGetLength(fingerprint) < kSecKeyWrapPGPFingerprintMinSize) {
        SecError(errSecUnsupportedOperation, error, CFSTR("invalid fingerprint"));
        return NULL;
    }

    CFTypeRef wrapAlg = CFDictionaryGetValue(parameters, _kSecKeyWrapPGPWrapAlg);
    if (wrapAlg == NULL) {
        SecError(errSecUnsupportedOperation, error, CFSTR("no wrap alg"));
        return NULL;
    } else if (CFEqual(wrapAlg, _kSecKeyWrapRFC6637WrapDigestSHA256KekAES128)) {
        wrap = &ccec_rfc6637_wrap_sha256_kek_aes128;
    } else if (CFEqual(wrapAlg, _kSecKeyWrapRFC6637WrapDigestSHA512KekAES256)) {
        wrap = &ccec_rfc6637_wrap_sha512_kek_aes256;
    } else {
        SecError(errSecUnsupportedOperation, error, CFSTR("unknown wrap alg"));
        return NULL;
    }

    num = CFDictionaryGetValue(parameters, _kSecKeyWrapRFC6637Flags);
    if (isNull(num)) {
        if (!CFNumberGetValue(num, kCFNumberLongType, &flags)) {
            SecError(errSecUnsupportedOperation, error, CFSTR("invalid flags: %@"), num);
            return NULL;
        }
    } else if (num) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unknown flags"));
        return NULL;
    }

    CFIndex unwrappedKey_size = CFDataGetLength(unwrappedKey);

    CFIndex output_size = ccec_rfc6637_wrap_key_size(pubkey, flags, unwrappedKey_size);
    if (output_size == 0) {
        SecError(errSecUnsupportedOperation, error, CFSTR("can't wrap that key, can't build size"));
        return NULL;
    }

    CFMutableDataRef data = CFDataCreateMutableWithScratch(NULL, output_size);
    require(data, errOut);

    err = ccec_rfc6637_wrap_key(pubkey, CFDataGetMutableBytePtr(data), flags,
                                sym_alg, CFDataGetLength(unwrappedKey), CFDataGetBytePtr(unwrappedKey),
                                curve, wrap, CFDataGetBytePtr(fingerprint),
                                ccrng_seckey);
    if (err) {
        SecError(errSecUnsupportedOperation, error, CFSTR("Failed to wrap key"));
        CFReleaseNull(data);
    }

errOut:
    return data;
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
    SecECPublicKeyCopyKeyDescription,
    SecECKeyGetAlgorithmID,
    SecECPublicKeyCopyPublicOctets,
    SecECKeyCopyWrapKey,
    NULL, /* SecKeyCopyUnwrapKey */
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
    {
        /* TODO: DER import size (and thus cp), pub.x, pub.y and k. */
        //err = ecc_import(keyData, keyDataLength, fullkey);

        /* DER != PKCS#1, but we'll go along with it */
        ccoid_t oid;
        size_t n;
        ccec_const_cp_t cp;

        require_noerr(ccec_der_import_priv_keytype(keyDataLength, keyData, &oid, &n), abort);
        cp = ccec_cp_for_oid(oid);
        if (cp.zp == NULL) {
            cp = ccec_curve_for_length_lookup(n * 8 /* bytes -> bits */,
                ccec_cp_192(), ccec_cp_224(), ccec_cp_256(), ccec_cp_384(), ccec_cp_521(), NULL);
        }
        require_action(cp.zp != NULL, abort, err = errSecDecode);
        ccec_ctx_init(cp, fullkey);

        require_noerr(ccec_der_import_priv(cp, keyDataLength, keyData, fullkey), abort);
        err = errSecSuccess;
        break;
    }
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

        ccec_const_cp_t cp = ccec_get_cp(keyLengthInBits);

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
    ccec_full_ctx_t fullkey = {};
    fullkey.hdr = key->key;
    int err;
    require_action_quiet(sigLen, errOut, err = errSecParam);
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
    for (i = 0; i < array_size(dis); ++i) {
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
    return ccec_ctx_size(fullkey);
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
static CFStringRef SecECPrivateKeyCopyKeyDescription(SecKeyRef key) {

    const char* curve = getCurveName(key);

	return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR( "<SecKeyRef curve type: %s, algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, addr: %p>"), curve, (long)SecKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), key);

}

static CFDataRef SecECKeyCopyUnwrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef wrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    const struct ccec_rfc6637_curve *curve;
    const struct ccec_rfc6637_unwrap *unwrap;
    ccec_full_ctx_t fullkey;
    CFMutableDataRef data;
    int res;
    uint8_t sym_alg = 0;
    unsigned long flags = 0;

    fullkey.hdr = key->key;

    curve = get_rfc6637_curve(key);
    if (curve == NULL) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unsupported curve"));
        return NULL;
    }

    CFTypeRef wrapAlg = CFDictionaryGetValue(parameters, _kSecKeyWrapPGPWrapAlg);
    if (wrapAlg == NULL) {
        SecError(errSecUnsupportedOperation, error, CFSTR("no wrap alg"));
        return NULL;
    } else if (CFEqual(wrapAlg, _kSecKeyWrapRFC6637WrapDigestSHA256KekAES128)) {
        unwrap = &ccec_rfc6637_unwrap_sha256_kek_aes128;
    } else if (CFEqual(wrapAlg, _kSecKeyWrapRFC6637WrapDigestSHA512KekAES256)) {
        unwrap = &ccec_rfc6637_unwrap_sha512_kek_aes256;
    } else {
        SecError(errSecUnsupportedOperation, error, CFSTR("unknown wrap alg"));
        return NULL;
    }

    CFDataRef fingerprint = CFDictionaryGetValue(parameters, _kSecKeyWrapPGPFingerprint);
    if (!isData(fingerprint) || CFDataGetLength(fingerprint) < kSecKeyWrapPGPFingerprintMinSize) {
        SecError(errSecUnsupportedOperation, error, CFSTR("invalid fingerprint"));
        return NULL;
    }

    CFNumberRef num = CFDictionaryGetValue(parameters, _kSecKeyWrapRFC6637Flags);
    if (isNull(num)) {
        if (!CFNumberGetValue(num, kCFNumberSInt32Type, &num)) {
            SecError(errSecUnsupportedOperation, error, CFSTR("invalid flags: %@"), num);
            return NULL;
        }
    } else if (num) {
        SecError(errSecUnsupportedOperation, error, CFSTR("unknown flags"));
        return NULL;
    }

    size_t keysize = CFDataGetLength(wrappedKey);
    data = CFDataCreateMutableWithScratch(NULL, keysize);
    if (data == NULL)
        return NULL;

    res = ccec_rfc6637_unwrap_key(fullkey, &keysize, CFDataGetMutableBytePtr(data),
                                  flags, &sym_alg, curve, unwrap,
                                  CFDataGetBytePtr(fingerprint),
                                  CFDataGetLength(wrappedKey), CFDataGetBytePtr(wrappedKey));
    if (res != 0) {
        CFReleaseNull(data);
        SecError(errSecUnsupportedOperation, error, CFSTR("failed to wrap key"));
        return NULL;
    }
    assert(keysize <= (size_t)CFDataGetLength(data));
    CFDataSetLength(data, keysize);

    if (outParam) {
        CFMutableDictionaryRef out =  CFDictionaryCreateMutableForCFTypes(NULL);
        if (out) {
            CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt8Type, &sym_alg);
            if (num) {
                CFDictionarySetValue(out, _kSecKeyWrapPGPSymAlg, num);
                CFRelease(num);
            }
            *outParam = out;
        }
    }

    return data;
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
    SecECPrivateKeyCopyKeyDescription,
    SecECKeyGetAlgorithmID,
    SecECPrivateKeyCopyPublicOctets,
    SecECKeyCopyWrapKey,
    SecECKeyCopyUnwrapKey,
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
    switch (ccec_ctx_size(pubkey)) {
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

/* Vile accessors that get us the pub or priv key to use temporarily */

bool SecECDoWithFullKey(SecKeyRef key, CFErrorRef* error, void (^action)(ccec_full_ctx_t private)) {
    if (key->key_class == &kSecECPrivateKeyDescriptor) {
        action(key->key);
    } else {
        return SecError(errSecParam, error, CFSTR("Not an EC Full Key object, sorry can't do."));
    }

    return true;
}

bool SecECDoWithPubKey(SecKeyRef key, CFErrorRef* error, void (^action)(ccec_pub_ctx_t public)) {
    if (key->key_class == &kSecECPublicKeyDescriptor) {
        action(key->key);
    } else {
        return SecError(errSecParam, error, CFSTR("Not an EC Public Key object, sorry can't do."));
    }

    return true;
}

