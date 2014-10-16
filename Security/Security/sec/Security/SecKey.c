/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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
 * SecKey.c - CoreFoundation based key object
 */


#include <Security/SecKeyInternal.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecFramework.h>

#include <utilities/SecIOFormat.h>

#include <utilities/SecCFWrappers.h>

#include "SecRSAKeyPriv.h"
#include "SecECKeyPriv.h"
#include "SecBasePriv.h"

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <Security/SecBase.h>
#include <pthread.h>
#include <string.h>
#include <AssertMacros.h>
#include <utilities/debugging.h>
#include <utilities/SecCFError.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecAsn1Coder.h>
#include <Security/oidsalg.h>
#include <Security/SecInternal.h>
#include <Security/SecRandom.h>
#include <corecrypto/ccrng_system.h>
#include <asl.h>
#include <stdlib.h>

/* Static functions. */
#define MAX_DIGEST_LEN (CC_SHA512_DIGEST_LENGTH)

/* Currently length of SHA512 oid + 1 */
#define MAX_OID_LEN (10)

#define DER_MAX_DIGEST_INFO_LEN  (10 + MAX_DIGEST_LEN + MAX_OID_LEN)

/* Encode the digestInfo header into digestInfo and return the offset from
 digestInfo at which to put the actual digest.  Returns 0 if digestInfo
 won't fit within digestInfoLength bytes.
 
 0x30, topLen,
 0x30, algIdLen,
 0x06, oid.Len, oid.Data,
 0x05, 0x00
 0x04, digestLen
 digestData
 */

static size_t DEREncodeDigestInfoPrefix(const SecAsn1Oid *oid,
                                        size_t digestLength, uint8_t *digestInfo, size_t digestInfoLength) {
    size_t algIdLen = oid->Length + 4;
    size_t topLen = algIdLen + digestLength + 4;
	size_t totalLen = topLen + 2;
    
    if (totalLen > digestInfoLength) {
        return 0;
    }
    
    size_t ix = 0;
    digestInfo[ix++] = (SEC_ASN1_SEQUENCE | SEC_ASN1_CONSTRUCTED);
    digestInfo[ix++] = topLen;
    digestInfo[ix++] = (SEC_ASN1_SEQUENCE | SEC_ASN1_CONSTRUCTED);
    digestInfo[ix++] = algIdLen;
    digestInfo[ix++] = SEC_ASN1_OBJECT_ID;
    digestInfo[ix++] = oid->Length;
    memcpy(&digestInfo[ix], oid->Data, oid->Length);
    ix += oid->Length;
    digestInfo[ix++] = SEC_ASN1_NULL;
    digestInfo[ix++] = 0;
    digestInfo[ix++] = SEC_ASN1_OCTET_STRING;
    digestInfo[ix++] = digestLength;
    
    return ix;
}

static CFDataRef SecKeyCopyPublicKeyHash(SecKeyRef key)
{
	CFDataRef pubKeyDigest = NULL, pubKeyBlob = NULL;

	/* encode the public key. */
    require_noerr(SecKeyCopyPublicBytes(key, &pubKeyBlob), errOut);
    require(pubKeyBlob, errOut);
    
	/* Calculate the digest of the public key. */
	require(pubKeyDigest = SecSHA1DigestCreate(CFGetAllocator(key),
                                               CFDataGetBytePtr(pubKeyBlob), CFDataGetLength(pubKeyBlob)),
			errOut);
errOut:
    CFReleaseNull(pubKeyBlob);
    return pubKeyDigest;
}


/*
 */
static CFDictionaryRef SecKeyCopyAttributeDictionaryWithLocalKey(SecKeyRef key,
                                                                 CFTypeRef keyType,
                                                                 CFDataRef privateBlob)
{
	CFAllocatorRef allocator = CFGetAllocator(key);
	DICT_DECLARE(25);
	CFDataRef pubKeyDigest = NULL, pubKeyBlob = NULL;
	CFDictionaryRef dict = NULL;
    
    size_t sizeValue = SecKeyGetSize(key, kSecKeyKeySizeInBits);
    CFNumberRef sizeInBits = CFNumberCreate(allocator, kCFNumberLongType, &sizeValue);
    
	/* encode the public key. */
    require_noerr(SecKeyCopyPublicBytes(key, &pubKeyBlob), errOut);
    require(pubKeyBlob, errOut);
    
	/* Calculate the digest of the public key. */
	require(pubKeyDigest = SecSHA1DigestCreate(allocator,
                                               CFDataGetBytePtr(pubKeyBlob), CFDataGetLength(pubKeyBlob)),
			errOut);
    
	DICT_ADDPAIR(kSecClass, kSecClassKey);
	DICT_ADDPAIR(kSecAttrKeyClass, privateBlob ? kSecAttrKeyClassPrivate : kSecAttrKeyClassPublic);
	DICT_ADDPAIR(kSecAttrApplicationLabel, pubKeyDigest);
	DICT_ADDPAIR(kSecAttrIsPermanent, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrIsPrivate, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrIsModifiable, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrKeyType, keyType);
	DICT_ADDPAIR(kSecAttrKeySizeInBits, sizeInBits);
	DICT_ADDPAIR(kSecAttrEffectiveKeySize, sizeInBits);
	DICT_ADDPAIR(kSecAttrIsSensitive, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrWasAlwaysSensitive, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrIsExtractable, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrWasNeverExtractable, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanEncrypt, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanDecrypt, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrCanDerive, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrCanSign, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrCanVerify, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanSignRecover, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanVerifyRecover, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanWrap, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanUnwrap, kCFBooleanTrue);
	DICT_ADDPAIR(kSecValueData, privateBlob ? privateBlob : pubKeyBlob);
    dict = DICT_CREATE(allocator);
    
errOut:
	// @@@ Zero out key material.
	CFReleaseSafe(pubKeyDigest);
	CFReleaseSafe(pubKeyBlob);
	CFReleaseSafe(sizeInBits);
    
	return dict;
}

CFDictionaryRef SecKeyGeneratePrivateAttributeDictionary(SecKeyRef key,
                                                         CFTypeRef keyType,
                                                         CFDataRef privateBlob)
{
    return SecKeyCopyAttributeDictionaryWithLocalKey(key, keyType, privateBlob);
}

CFDictionaryRef SecKeyGeneratePublicAttributeDictionary(SecKeyRef key, CFTypeRef keyType)
{
    return SecKeyCopyAttributeDictionaryWithLocalKey(key, keyType, NULL);
}

static CFStringRef SecKeyCopyDescription(CFTypeRef cf) {
    SecKeyRef key = (SecKeyRef)cf;
    
    if(key->key_class->describe)
        return key->key_class->describe(key);
    else
        return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecKeyRef: %p>"), key); 
}

static void SecKeyDestroy(CFTypeRef cf) {
    SecKeyRef key = (SecKeyRef)cf;
    if (key->key_class->destroy)
        key->key_class->destroy(key);
}

static Boolean SecKeyEqual(CFTypeRef cf1, CFTypeRef cf2)
{
    SecKeyRef key1 = (SecKeyRef)cf1;
    SecKeyRef key2 = (SecKeyRef)cf2;
    if (key1 == key2)
        return true;
    if (!key2 || key1->key_class != key2->key_class)
        return false;
    if (key1->key_class->extraBytes)
        return !memcmp(key1->key, key2->key, key1->key_class->extraBytes);
    
    /* TODO: Won't work when we get reference keys. */
    CFDictionaryRef d1, d2;
    d1 = SecKeyCopyAttributeDictionary(key1);
    d2 = SecKeyCopyAttributeDictionary(key2);
    Boolean result = CFEqual(d1, d2);
    CFReleaseSafe(d1);
    CFReleaseSafe(d2);
    return result;
}

struct ccrng_state *ccrng_seckey;

CFGiblisWithFunctions(SecKey, NULL, NULL, SecKeyDestroy, SecKeyEqual, NULL, NULL, SecKeyCopyDescription, NULL, NULL, ^{
    static struct ccrng_system_state ccrng_system_state_seckey;
    ccrng_seckey = (struct ccrng_state *)&ccrng_system_state_seckey;
    ccrng_system_init(&ccrng_system_state_seckey);
})

static bool getBoolForKey(CFDictionaryRef dict, CFStringRef key, bool default_value) {
	CFTypeRef value = CFDictionaryGetValue(dict, key);
	if (value) {
		if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
			return CFBooleanGetValue(value);
		} else {
			secwarning("Value %@ for key %@ is not bool", value, key);
		}
	}
    
	return default_value;
}

static OSStatus add_ref(CFTypeRef item, CFMutableDictionaryRef dict) {
	CFDictionarySetValue(dict, kSecValueRef, item);
	return SecItemAdd(dict, NULL);
}

static void merge_params_applier(const void *key, const void *value,
                                 void *context) {
	CFMutableDictionaryRef result = (CFMutableDictionaryRef)context;
	CFDictionaryAddValue(result, key, value);
}

/* Create a mutable dictionary that is based on the subdictionary for key
 with any attributes from the top level dict merged in. */
static CFMutableDictionaryRef merge_params(CFDictionaryRef dict,
                                           CFStringRef key) {
	CFDictionaryRef subdict = CFDictionaryGetValue(dict, key);
	CFMutableDictionaryRef result;
    
	if (subdict) {
		result = CFDictionaryCreateMutableCopy(NULL, 0, subdict);
		/* Add everything in dict not already in result to result. */
		CFDictionaryApplyFunction(dict, merge_params_applier, result);
	} else {
		result = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	}
    
	/* Remove values that only belong in the top level dict. */
	CFDictionaryRemoveValue(result, kSecPublicKeyAttrs);
	CFDictionaryRemoveValue(result, kSecPrivateKeyAttrs);
	CFDictionaryRemoveValue(result, kSecAttrKeyType);
	CFDictionaryRemoveValue(result, kSecAttrKeySizeInBits);
    
	return result;
}

/* Generate a private/public keypair. */
OSStatus SecKeyGeneratePair(CFDictionaryRef parameters,
                            SecKeyRef *publicKey, SecKeyRef *privateKey) {
    OSStatus result = errSecUnsupportedAlgorithm;
    SecKeyRef privKey = NULL;
	SecKeyRef pubKey = NULL;
    CFMutableDictionaryRef pubParams = merge_params(parameters, kSecPublicKeyAttrs),
    privParams = merge_params(parameters, kSecPrivateKeyAttrs);
	CFStringRef ktype = CFDictionaryGetValue(parameters, kSecAttrKeyType);
    
    require(ktype, errOut);
    
    if (CFEqual(ktype, kSecAttrKeyTypeEC)) {
        result = SecECKeyGeneratePair(parameters, &pubKey, &privKey);
    } else if (CFEqual(ktype, kSecAttrKeyTypeRSA)) {
        result = SecRSAKeyGeneratePair(parameters, &pubKey, &privKey);
    }
    
    require_noerr(result, errOut);
    
    /* Store the keys in the keychain if they are marked as permanent. */
    if (getBoolForKey(pubParams, kSecAttrIsPermanent, false)) {
        require_noerr_quiet(result = add_ref(pubKey, pubParams), errOut);
    }
    if (getBoolForKey(privParams, kSecAttrIsPermanent, false)) {
        require_noerr_quiet(result = add_ref(privKey, privParams), errOut);
    }
    
    if (publicKey) {
        *publicKey = pubKey;
        pubKey = NULL;
    }
    if (privateKey) {
        *privateKey = privKey;
        privKey = NULL;
    }
    
errOut:
	CFReleaseSafe(pubParams);
	CFReleaseSafe(privParams);
    CFReleaseSafe(pubKey);
    CFReleaseSafe(privKey);
    
    return result;
}

SecKeyRef SecKeyCreatePublicFromPrivate(SecKeyRef privateKey) {
    CFDataRef serializedPublic = NULL;
    SecKeyRef result = NULL;
    
    require_noerr_quiet(SecKeyCopyPublicBytes(privateKey, &serializedPublic), fail);
    require_quiet(serializedPublic, fail);
    
    result = SecKeyCreateFromPublicData(kCFAllocatorDefault, SecKeyGetAlgorithmID(privateKey), serializedPublic);
    
fail:
    CFReleaseSafe(serializedPublic);
    
    return result;
}

static CFDictionaryRef CreatePrivateKeyMatchingQuery(SecKeyRef publicKey, bool returnPersistentRef)
{
    CFDataRef public_key_hash = SecKeyCopyPublicKeyHash(publicKey);
    
    CFDictionaryRef query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         kSecClass,                 kSecClassKey,
                                                         kSecAttrKeyClass,          kSecAttrKeyClassPrivate,
                                                         kSecAttrSynchronizable,    kSecAttrSynchronizableAny,
                                                         kSecAttrApplicationLabel,  public_key_hash,
                                                         kSecReturnPersistentRef,   kCFBooleanTrue,
                                                         NULL);
    CFReleaseNull(public_key_hash);
    
    return query;
}

CFDataRef SecKeyCreatePersistentRefToMatchingPrivateKey(SecKeyRef publicKey, CFErrorRef *error) {
    CFTypeRef persistentRef = NULL;
    CFDictionaryRef query = CreatePrivateKeyMatchingQuery(publicKey, true);

    require_quiet(SecError(SecItemCopyMatching(query, &persistentRef),error ,
                           CFSTR("Error finding persistent ref to key from public: %@"), publicKey), fail);
fail:
    CFReleaseNull(query);
    return (CFDataRef)persistentRef;
}

SecKeyRef SecKeyCopyMatchingPrivateKey(SecKeyRef publicKey, CFErrorRef *error) {
    CFTypeRef private_key = NULL;
    
    CFDictionaryRef query = CreatePrivateKeyMatchingQuery(publicKey, false);    
    
    require_quiet(SecError(SecItemCopyMatching(query, &private_key), error,
                           CFSTR("Error finding private key from public: %@"), publicKey), fail);
fail:
    CFReleaseNull(query);
    return (SecKeyRef)private_key;
}

SecKeyRef SecKeyCreatePublicFromDER(CFAllocatorRef allocator,
                                    const SecAsn1Oid *oid, const SecAsn1Item *params,
                                    const SecAsn1Item *keyData) {
    SecKeyRef publicKey = NULL;
	if (SecAsn1OidCompare(oid, &CSSMOID_RSA)) {
        /* pkcs1 1 */
		publicKey = SecKeyCreateRSAPublicKey(kCFAllocatorDefault,
                                             keyData->Data, keyData->Length, kSecKeyEncodingPkcs1);
	} else if (SecAsn1OidCompare(oid, &CSSMOID_ecPublicKey)) {
        SecDERKey derKey = {
            .oid = oid->Data,
            .oidLength = oid->Length,
            .key = keyData->Data,
            .keyLength = keyData->Length,
        };
        if (params) {
            derKey.parameters = params->Data;
            derKey.parametersLength = params->Length;
        }
		publicKey = SecKeyCreateECPublicKey(kCFAllocatorDefault,
                                            (const uint8_t *)&derKey, sizeof(derKey), kSecDERKeyEncoding);
    } else {
		secwarning("Unsupported algorithm oid");
	}
    
    return publicKey;
}

SecKeyRef SecKeyCreate(CFAllocatorRef allocator,
                       const SecKeyDescriptor *key_class, const uint8_t *keyData,
                       CFIndex keyDataLength, SecKeyEncoding encoding) {
	if (!key_class) return NULL;
    size_t size = sizeof(struct __SecKey) + key_class->extraBytes;
    SecKeyRef result = (SecKeyRef)_CFRuntimeCreateInstance(allocator,
                                                           SecKeyGetTypeID(), size - sizeof(CFRuntimeBase), NULL);
	if (result) {
		memset((char*)result + sizeof(result->_base), 0, size - sizeof(result->_base));
        result->key_class = key_class;
        if (key_class->extraBytes) {
            /* Make result->key point to the extraBytes we allocated. */
            result->key = ((char*)result) + sizeof(*result);
        }
        if (key_class->init) {
			OSStatus status;
			status = key_class->init(result, keyData, keyDataLength, encoding);
			if (status) {
				secwarning("init %s key: %" PRIdOSStatus, key_class->name, status);
				CFRelease(result);
				result = NULL;
			}
		}
    }
    return result;
}

enum {
    kSecKeyDigestInfoSign,
    kSecKeyDigestInfoVerify
};

static OSStatus SecKeyDigestInfoSignVerify(
                                           SecKeyRef           key,            /* Private key */
                                           SecPadding          padding,		/* kSecPaddingPKCS1@@@ */
                                           const uint8_t       *dataToSign,	/* signature over this data */
                                           size_t              dataToSignLen,	/* length of dataToSign */
                                           uint8_t             *sig,			/* signature, RETURNED */
                                           size_t              *sigLen,        /* IN/OUT */
                                           int mode) {
    size_t digestInfoLength = DER_MAX_DIGEST_INFO_LEN;
    uint8_t digestInfo[digestInfoLength];
    const SecAsn1Oid *digestOid;
    size_t digestLen;
    
    switch (padding) {
#if 0
        case kSecPaddingPKCS1MD2:
            digestLen = CC_MD2_DIGEST_LENGTH;
            digestOid = &CSSMOID_MD2;
            break;
        case kSecPaddingPKCS1MD4:
            digestLen = CC_MD4_DIGEST_LENGTH;
            digestOid = &CSSMOID_MD4;
            break;
        case kSecPaddingPKCS1MD5:
            digestLen = CC_MD5_DIGEST_LENGTH;
            digestOid = &CSSMOID_MD5;
            break;
#endif
        case kSecPaddingPKCS1SHA1:
            digestLen = CC_SHA1_DIGEST_LENGTH;
            digestOid = &CSSMOID_SHA1;
            break;
        case kSecPaddingPKCS1SHA224:
            digestLen = CC_SHA224_DIGEST_LENGTH;
            digestOid = &CSSMOID_SHA224;
            break;
        case kSecPaddingPKCS1SHA256:
            digestLen = CC_SHA256_DIGEST_LENGTH;
            digestOid = &CSSMOID_SHA256;
            break;
        case kSecPaddingPKCS1SHA384:
            digestLen = CC_SHA384_DIGEST_LENGTH;
            digestOid = &CSSMOID_SHA384;
            break;
        case kSecPaddingPKCS1SHA512:
            digestLen = CC_SHA512_DIGEST_LENGTH;
            digestOid = &CSSMOID_SHA512;
            break;
        default:
            return errSecUnsupportedPadding;
    }
    
    if (dataToSignLen != digestLen)
        return errSecParam;
    
    size_t offset = DEREncodeDigestInfoPrefix(digestOid, digestLen,
                                              digestInfo, digestInfoLength);
    if (!offset)
        return errSecBufferTooSmall;
    
    /* Append the digest to the digestInfo prefix and adjust the length. */
    memcpy(&digestInfo[offset], dataToSign, digestLen);
    digestInfoLength = offset + digestLen;
    
    if (mode == kSecKeyDigestInfoSign) {
        return key->key_class->rawSign(key, kSecPaddingPKCS1,
                                       digestInfo, digestInfoLength, sig, sigLen);
    } else {
        return key->key_class->rawVerify(key, kSecPaddingPKCS1,
                                         digestInfo, digestInfoLength, sig, *sigLen);
    }
    
    return errSecSuccess;
}

OSStatus SecKeyRawSign(
                       SecKeyRef           key,            /* Private key */
                       SecPadding          padding,		/* kSecPaddingNone or kSecPaddingPKCS1 */
                       const uint8_t       *dataToSign,	/* signature over this data */
                       size_t              dataToSignLen,	/* length of dataToSign */
                       uint8_t             *sig,			/* signature, RETURNED */
                       size_t              *sigLen) {		/* IN/OUT */
    if (!key->key_class->rawSign)
        return errSecUnsupportedOperation;
    
    if (padding < kSecPaddingPKCS1MD2) {
        return key->key_class->rawSign(key, padding, dataToSign, dataToSignLen,
                                       sig, sigLen);
    } else {
        return SecKeyDigestInfoSignVerify(key, padding, dataToSign, dataToSignLen,
                                          sig, sigLen, kSecKeyDigestInfoSign);
    }
}

OSStatus SecKeyRawVerify(
                         SecKeyRef           key,            /* Public key */
                         SecPadding          padding,		/* kSecPaddingNone or kSecPaddingPKCS1 */
                         const uint8_t       *signedData,	/* signature over this data */
                         size_t              signedDataLen,	/* length of dataToSign */
                         const uint8_t       *sig,			/* signature */
                         size_t              sigLen) {		/* length of signature */
    if (!key->key_class->rawVerify)
        return errSecUnsupportedOperation;
    
    if (padding < kSecPaddingPKCS1MD2) {
        return key->key_class->rawVerify(key, padding, signedData, signedDataLen,
                                         sig, sigLen);
    } else {
        /* Casting away the constness of sig is safe since
         SecKeyDigestInfoSignVerify only modifies sig if
         mode == kSecKeyDigestInfoSign. */
        return SecKeyDigestInfoSignVerify(key, padding,
                                          signedData, signedDataLen, (uint8_t *)sig, &sigLen,
                                          kSecKeyDigestInfoVerify);
    }
}

OSStatus SecKeyEncrypt(
                       SecKeyRef           key,                /* Public key */
                       SecPadding          padding,			/* kSecPaddingNone, kSecPaddingPKCS1, kSecPaddingOAEP */
                       const uint8_t		*plainText,
                       size_t              plainTextLen,		/* length of plainText */
                       uint8_t             *cipherText,
                       size_t              *cipherTextLen) {	/* IN/OUT */
    if (key->key_class->encrypt)
        return key->key_class->encrypt(key, padding, plainText, plainTextLen,
                                       cipherText, cipherTextLen);
    return errSecUnsupportedOperation;
}

OSStatus SecKeyDecrypt(
                       SecKeyRef           key,                /* Private key */
                       SecPadding          padding,			/* kSecPaddingNone, kSecPaddingPKCS1, kSecPaddingOAEP */
                       const uint8_t       *cipherText,
                       size_t              cipherTextLen,		/* length of cipherText */
                       uint8_t             *plainText,
                       size_t              *plainTextLen) {	/* IN/OUT */
    if (key->key_class->decrypt)
        return key->key_class->decrypt(key, padding, cipherText, cipherTextLen,
                                       plainText, plainTextLen);
    return errSecUnsupportedOperation;
}

size_t SecKeyGetBlockSize(SecKeyRef key) {
    if (key->key_class->blockSize)
        return key->key_class->blockSize(key);
    return 0;
}

/* Private API functions. */

CFDictionaryRef SecKeyCopyAttributeDictionary(SecKeyRef key) {
    if (key->key_class->copyDictionary)
        return key->key_class->copyDictionary(key);
    return NULL;
}

SecKeyRef SecKeyCreateFromAttributeDictionary(CFDictionaryRef refAttributes) {
	/* TODO: Support having an allocator in refAttributes. */
 	CFAllocatorRef allocator = NULL;
	CFDataRef data = CFDictionaryGetValue(refAttributes, kSecValueData);
	CFTypeRef ktype = CFDictionaryGetValue(refAttributes, kSecAttrKeyType);
	SInt32 algorithm;
	SecKeyRef ref;
    
	/* First figure out the key type (algorithm). */
	if (CFGetTypeID(ktype) == CFNumberGetTypeID()) {
		CFNumberGetValue(ktype, kCFNumberSInt32Type, &algorithm);
	} else if (isString(ktype)) {
        algorithm = CFStringGetIntValue(ktype);
        CFStringRef t = CFStringCreateWithFormat(0, 0, CFSTR("%ld"), (long) algorithm);
        if (!CFEqual(t, ktype)) {
            secwarning("Unsupported key class: %@", ktype);
            CFReleaseSafe(t);
            return NULL;
        }
        CFReleaseSafe(t);
    } else {
		secwarning("Unsupported key type: %@", ktype);
		return NULL;
	}
    
	/* TODO: The code below won't scale well, consider moving to something
     table driven. */
	SInt32 class;
	CFTypeRef kclass = CFDictionaryGetValue(refAttributes, kSecAttrKeyClass);
	if (CFGetTypeID(kclass) == CFNumberGetTypeID()) {
		CFNumberGetValue(kclass, kCFNumberSInt32Type, &class);
	} else if (isString(kclass)) {
        class = CFStringGetIntValue(kclass);
        CFStringRef t = CFStringCreateWithFormat(0, 0, CFSTR("%ld"), (long) class);
        if (!CFEqual(t, kclass)) {
            CFReleaseSafe(t);
            secwarning("Unsupported key class: %@", kclass);
            return NULL;
        }
        CFReleaseSafe(t);
    } else {
		secwarning("Unsupported key class: %@", kclass);
		return NULL;
	}
    
    switch (class) {
        case 0: // kSecAttrKeyClassPublic
            switch (algorithm) {
                case 42: // kSecAlgorithmRSA
                    ref = SecKeyCreateRSAPublicKey(allocator,
                                                   CFDataGetBytePtr(data), CFDataGetLength(data),
                                                   kSecKeyEncodingBytes);
                    break;
                case 43: // kSecAlgorithmECDSA
                case 73: // kSecAlgorithmEC
                    ref = SecKeyCreateECPublicKey(allocator,
                                                  CFDataGetBytePtr(data), CFDataGetLength(data),
                                                  kSecKeyEncodingBytes);
                    break;
                default:
                    secwarning("Unsupported public key type: %@", ktype);
                    ref = NULL;
                    break;
            };
            break;
        case 1: // kSecAttrKeyClassPrivate
            switch (algorithm) {
                case 42: // kSecAlgorithmRSA
                    ref = SecKeyCreateRSAPrivateKey(allocator,
                                                    CFDataGetBytePtr(data), CFDataGetLength(data),
                                                    kSecKeyEncodingBytes);
                    break;
                case 43: // kSecAlgorithmECDSA
                case 73: // kSecAlgorithmEC
                    ref = SecKeyCreateECPrivateKey(allocator,
                                                   CFDataGetBytePtr(data), CFDataGetLength(data),
                                                   kSecKeyEncodingBytes);
                    break;
                default:
                    secwarning("Unsupported private key type: %@", ktype);
                    ref = NULL;
                    break;
            };
            break;
        case 2: // kSecAttrKeyClassSymmetric
            secwarning("Unsupported symmetric key type: %@", ktype);
            ref = NULL;
            break;
        default:
            secwarning("Unsupported key class: %@", kclass);
            ref = NULL;
    }
    
	return ref;
}

/* TODO: This function should ensure that this keys algorithm matches the
 signature algorithm. */
static OSStatus SecKeyGetDigestInfo(SecKeyRef this, const SecAsn1AlgId *algId,
                                    const uint8_t *data, size_t dataLen, bool digestData,
                                    uint8_t *digestInfo, size_t *digestInfoLen /* IN/OUT */) {
    unsigned char *(*digestFcn)(const void *, CC_LONG, unsigned char *);
    CFIndex keyAlgID = kSecNullAlgorithmID;
    const SecAsn1Oid *digestOid;
    size_t digestLen;
    size_t offset = 0;
    
    /* Since these oids all have the same prefix, use switch. */
    if ((algId->algorithm.Length == CSSMOID_RSA.Length) &&
        !memcmp(algId->algorithm.Data, CSSMOID_RSA.Data,
                algId->algorithm.Length - 1)) {
            keyAlgID = kSecRSAAlgorithmID;
            switch (algId->algorithm.Data[algId->algorithm.Length - 1]) {
#if 0
                case 2: /* oidMD2WithRSA */
                    digestFcn = CC_MD2;
                    digestLen = CC_MD2_DIGEST_LENGTH;
                    digestOid = &CSSMOID_MD2;
                    break;
                case 3: /* oidMD4WithRSA */
                    digestFcn = CC_MD4;
                    digestLen = CC_MD4_DIGEST_LENGTH;
                    digestOid = &CSSMOID_MD4;
                    break;
                case 4: /* oidMD5WithRSA */
                    digestFcn = CC_MD5;
                    digestLen = CC_MD5_DIGEST_LENGTH;
                    digestOid = &CSSMOID_MD5;
                    break;
#endif /* 0 */
                case 5: /* oidSHA1WithRSA */
                    digestFcn = CC_SHA1;
                    digestLen = CC_SHA1_DIGEST_LENGTH;
                    digestOid = &CSSMOID_SHA1;
                    break;
                case 11: /* oidSHA256WithRSA */
                    digestFcn = CC_SHA256;
                    digestLen = CC_SHA256_DIGEST_LENGTH;
                    digestOid = &CSSMOID_SHA256;
                    break;
                case 12: /* oidSHA384WithRSA */
                    /* pkcs1 12 */
                    digestFcn = CC_SHA384;
                    digestLen = CC_SHA384_DIGEST_LENGTH;
                    digestOid = &CSSMOID_SHA384;
                    break;
                case 13: /* oidSHA512WithRSA */
                    digestFcn = CC_SHA512;
                    digestLen = CC_SHA512_DIGEST_LENGTH;
                    digestOid = &CSSMOID_SHA512;
                    break;
                case 14: /* oidSHA224WithRSA */
                    digestFcn = CC_SHA224;
                    digestLen = CC_SHA224_DIGEST_LENGTH;
                    digestOid = &CSSMOID_SHA224;
                    break;
                default:
                    secdebug("key", "unsupported rsa signature algorithm");
                    return errSecUnsupportedAlgorithm;
            }
        } else if ((algId->algorithm.Length == CSSMOID_ECDSA_WithSHA224.Length) &&
                   !memcmp(algId->algorithm.Data, CSSMOID_ECDSA_WithSHA224.Data,
                           algId->algorithm.Length - 1)) {
                       keyAlgID = kSecECDSAAlgorithmID;
                       switch (algId->algorithm.Data[algId->algorithm.Length - 1]) {
                           case 1: /* oidSHA224WithECDSA */
                               digestFcn = CC_SHA224;
                               digestLen = CC_SHA224_DIGEST_LENGTH;
                               break;
                           case 2: /* oidSHA256WithECDSA */
                               digestFcn = CC_SHA256;
                               digestLen = CC_SHA256_DIGEST_LENGTH;
                               break;
                           case 3: /* oidSHA384WithECDSA */
                               /* pkcs1 12 */
                               digestFcn = CC_SHA384;
                               digestLen = CC_SHA384_DIGEST_LENGTH;
                               break;
                           case 4: /* oidSHA512WithECDSA */
                               digestFcn = CC_SHA512;
                               digestLen = CC_SHA512_DIGEST_LENGTH;
                               break;
                           default:
                               secdebug("key", "unsupported ecdsa signature algorithm");
                               return errSecUnsupportedAlgorithm;
                       }
                   } else if (SecAsn1OidCompare(&algId->algorithm, &CSSMOID_ECDSA_WithSHA1)) {
                       keyAlgID = kSecECDSAAlgorithmID;
                       digestFcn = CC_SHA1;
                       digestLen = CC_SHA1_DIGEST_LENGTH;
                   } else if (SecAsn1OidCompare(&algId->algorithm, &CSSMOID_SHA1)) {
                       digestFcn = CC_SHA1;
                       digestLen = CC_SHA1_DIGEST_LENGTH;
                       digestOid = &CSSMOID_SHA1;
                   } else if ((algId->algorithm.Length == CSSMOID_SHA224.Length) &&
                              !memcmp(algId->algorithm.Data, CSSMOID_SHA224.Data, algId->algorithm.Length - 1))
                   {
                       switch (algId->algorithm.Data[algId->algorithm.Length - 1]) {
                           case 4: /* OID_SHA224 */
                               digestFcn = CC_SHA224;
                               digestLen = CC_SHA224_DIGEST_LENGTH;
                               digestOid = &CSSMOID_SHA224;
                               break;
                           case 1: /* OID_SHA256 */
                               digestFcn = CC_SHA256;
                               digestLen = CC_SHA256_DIGEST_LENGTH;
                               digestOid = &CSSMOID_SHA256;
                               break;
                           case 2: /* OID_SHA384 */
                               /* pkcs1 12 */
                               digestFcn = CC_SHA384;
                               digestLen = CC_SHA384_DIGEST_LENGTH;
                               digestOid = &CSSMOID_SHA384;
                               break;
                           case 3: /* OID_SHA512 */
                               digestFcn = CC_SHA512;
                               digestLen = CC_SHA512_DIGEST_LENGTH;
                               digestOid = &CSSMOID_SHA512;
                               break;
                           default:
                               secdebug("key", "unsupported sha-2 signature algorithm");
                               return errSecUnsupportedAlgorithm;
                       }
                   } else if (SecAsn1OidCompare(&algId->algorithm, &CSSMOID_MD5)) {
                       digestFcn = CC_MD5;
                       digestLen = CC_MD5_DIGEST_LENGTH;
                       digestOid = &CSSMOID_MD5;
                   } else {
                       secdebug("key", "unsupported digesting algorithm");
                       return errSecUnsupportedAlgorithm;
                   }
    
    /* check key is appropriate for signature (superfluous for digest only oid) */
    if (keyAlgID == kSecNullAlgorithmID)
        keyAlgID = SecKeyGetAlgorithmID(this);
    else if (keyAlgID != SecKeyGetAlgorithmID(this))
        return errSecUnsupportedAlgorithm;
    
    switch(keyAlgID) {
        case kSecRSAAlgorithmID:
            offset = DEREncodeDigestInfoPrefix(digestOid, digestLen,
                                               digestInfo, *digestInfoLen);
            if (!offset)
                return errSecBufferTooSmall;
            break;
        case kSecDSAAlgorithmID:
            if (digestOid != &CSSMOID_SHA1)
                return errSecUnsupportedAlgorithm;
            break;
        case kSecECDSAAlgorithmID:
            break;
        default:
            secdebug("key", "unsupported signature algorithm");
            return errSecUnsupportedAlgorithm;
    }
    
    if (digestData) {
        if(dataLen>UINT32_MAX) /* Check for overflow with CC_LONG cast */
            return errSecParam;
        digestFcn(data, (CC_LONG)dataLen, &digestInfo[offset]);
        *digestInfoLen = offset + digestLen;
    } else {
        if (dataLen != digestLen)
            return errSecParam;
        memcpy(&digestInfo[offset], data, dataLen);
        *digestInfoLen = offset + dataLen;
    }
    
    return errSecSuccess;
}

OSStatus SecKeyDigestAndVerify(
                               SecKeyRef           this,            /* Private key */
                               const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                               const uint8_t       *dataToDigest,	/* signature over this data */
                               size_t              dataToDigestLen,/* length of dataToDigest */
                               const uint8_t       *sig,			/* signature to verify */
                               size_t              sigLen) {		/* length of sig */
    size_t digestInfoLength = DER_MAX_DIGEST_INFO_LEN;
    uint8_t digestInfo[digestInfoLength];
    OSStatus status;
    
    if (this == NULL)
        return errSecParam;
    
    status = SecKeyGetDigestInfo(this, algId, dataToDigest, dataToDigestLen, true,
                                 digestInfo, &digestInfoLength);
    if (status)
        return status;
    return SecKeyRawVerify(this, kSecPaddingPKCS1,
                           digestInfo, digestInfoLength, sig, sigLen);
}

OSStatus SecKeyDigestAndSign(
                             SecKeyRef           this,            /* Private key */
                             const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                             const uint8_t       *dataToDigest,	/* signature over this data */
                             size_t              dataToDigestLen,/* length of dataToDigest */
                             uint8_t             *sig,			/* signature, RETURNED */
                             size_t              *sigLen) {		/* IN/OUT */
    size_t digestInfoLength = DER_MAX_DIGEST_INFO_LEN;
    uint8_t digestInfo[digestInfoLength];
    OSStatus status;
    
    status = SecKeyGetDigestInfo(this, algId, dataToDigest, dataToDigestLen, true /* digest data */,
                                 digestInfo, &digestInfoLength);
    if (status)
        return status;
    return SecKeyRawSign(this, kSecPaddingPKCS1,
                         digestInfo, digestInfoLength, sig, sigLen);
}

OSStatus SecKeyVerifyDigest(
                            SecKeyRef           this,            /* Private key */
                            const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                            const uint8_t       *digestData,	/* signature over this digest */
                            size_t              digestDataLen,/* length of dataToDigest */
                            const uint8_t       *sig,			/* signature to verify */
                            size_t              sigLen) {		/* length of sig */
    size_t digestInfoLength = DER_MAX_DIGEST_INFO_LEN;
    uint8_t digestInfo[digestInfoLength];
    OSStatus status;
    
    status = SecKeyGetDigestInfo(this, algId, digestData, digestDataLen, false /* data is digest */,
                                 digestInfo, &digestInfoLength);
    if (status)
        return status;
    return SecKeyRawVerify(this, kSecPaddingPKCS1,
                           digestInfo, digestInfoLength, sig, sigLen);
}

OSStatus SecKeySignDigest(
                          SecKeyRef           this,            /* Private key */
                          const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                          const uint8_t       *digestData,	/* signature over this digest */
                          size_t              digestDataLen,/* length of digestData */
                          uint8_t             *sig,			/* signature, RETURNED */
                          size_t              *sigLen) {		/* IN/OUT */
    size_t digestInfoLength = DER_MAX_DIGEST_INFO_LEN;
    uint8_t digestInfo[digestInfoLength];
    OSStatus status;
    
    status = SecKeyGetDigestInfo(this, algId, digestData, digestDataLen, false,
                                 digestInfo, &digestInfoLength);
    if (status)
        return status;
    return SecKeyRawSign(this, kSecPaddingPKCS1,
                         digestInfo, digestInfoLength, sig, sigLen);
}

CFIndex SecKeyGetAlgorithmID(SecKeyRef key) {
    /* This method was added to version 1 keys. */
    if (key->key_class->version > 0 && key->key_class->getAlgorithmID)
        return key->key_class->getAlgorithmID(key);
    /* All version 0 key were RSA. */
    return kSecRSAAlgorithmID;
}


OSStatus SecKeyCopyPublicBytes(SecKeyRef key, CFDataRef* serializedPublic) {
    if (key->key_class->version > 1 && key->key_class->copyPublic)
        return key->key_class->copyPublic(key, serializedPublic);
    return errSecUnimplemented;
}

SecKeyRef SecKeyCreateFromPublicBytes(CFAllocatorRef allocator, CFIndex algorithmID, const uint8_t *keyData, CFIndex keyDataLength)
{
    switch (algorithmID)
    {
        case kSecRSAAlgorithmID:
            return SecKeyCreateRSAPublicKey(allocator,
                                            keyData, keyDataLength,
                                            kSecKeyEncodingBytes);
        case kSecECDSAAlgorithmID:
            return SecKeyCreateECPublicKey(allocator,
                                           keyData, keyDataLength,
                                           kSecKeyEncodingBytes);
        default:
            return NULL;
    }
}

SecKeyRef SecKeyCreateFromPublicData(CFAllocatorRef allocator, CFIndex algorithmID, CFDataRef serialized)
{
    return SecKeyCreateFromPublicBytes(allocator, algorithmID, CFDataGetBytePtr(serialized), CFDataGetLength(serialized));
}

// This is a bit icky hack to avoid changing the vtable for
// SecKey.
size_t SecKeyGetSize(SecKeyRef key, SecKeySize whichSize)
{
    size_t result = SecKeyGetBlockSize(key);
    
    if (kSecECDSAAlgorithmID == SecKeyGetAlgorithmID(key)) {
        switch (whichSize) {
            case kSecKeyEncryptedDataSize:
                result = 0;
                break;
            case kSecKeySignatureSize:
                result = (result >= 66 ? 9 : 8) + 2 * result;
                break;
            case kSecKeyKeySizeInBits:
                if (result >= 66)
                    return 521;
        }
    }
    
    if (whichSize == kSecKeyKeySizeInBits)
        result *= 8;
    
    return result;
    
}

OSStatus SecKeyFindWithPersistentRef(CFDataRef persistentRef, SecKeyRef* lookedUpData)
{
    CFDictionaryRef query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         kSecReturnRef,             kCFBooleanTrue,
                                                         kSecClass,                 kSecClassKey,
                                                         kSecValuePersistentRef,    persistentRef,
                                                         NULL);
    CFTypeRef foundRef = NULL;
    OSStatus status = SecItemCopyMatching(query, &foundRef);
    
    if (status == errSecSuccess) {
        if (CFGetTypeID(foundRef) == SecKeyGetTypeID()) {
            *lookedUpData = (SecKeyRef) foundRef;
            foundRef = NULL;
            status = errSecSuccess;
        } else {
            status = errSecItemNotFound;
        }
    }
    
    CFReleaseSafe(foundRef);
    CFReleaseSafe(query);
    
    return status;
}

OSStatus SecKeyCopyPersistentRef(SecKeyRef key, CFDataRef* persistentRef)
{
    CFDictionaryRef query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         kSecReturnPersistentRef,   kCFBooleanTrue,
                                                         kSecValueRef,              key,
                                                         kSecAttrSynchronizable,    kSecAttrSynchronizableAny,
                                                         NULL);
    CFTypeRef foundRef = NULL;
    OSStatus status = SecItemCopyMatching(query, &foundRef);
    
    if (status == errSecSuccess) {
        if (CFGetTypeID(foundRef) == CFDataGetTypeID()) {
            *persistentRef = foundRef;
            foundRef = NULL;
        } else {
            status = errSecItemNotFound;
        }
    }
    
    CFReleaseSafe(foundRef);
    CFReleaseSafe(query);
    
    return status;
}

/*
 *
 */

#define SEC_CONST_DECL(k,v) CFTypeRef k = (CFTypeRef)(CFSTR(v));

SEC_CONST_DECL(_kSecKeyWrapPGPSymAlg, "kSecKeyWrapPGPSymAlg");
SEC_CONST_DECL(_kSecKeyWrapPGPFingerprint, "kSecKeyWrapPGPFingerprint");
SEC_CONST_DECL(_kSecKeyWrapPGPWrapAlg, "kSecKeyWrapPGPWrapAlg");
SEC_CONST_DECL(_kSecKeyWrapRFC6637Flags, "kSecKeyWrapPGPECFlags");
SEC_CONST_DECL(_kSecKeyWrapRFC6637WrapDigestSHA256KekAES128, "kSecKeyWrapPGPECWrapDigestSHA256KekAES128");
SEC_CONST_DECL(_kSecKeyWrapRFC6637WrapDigestSHA512KekAES256, "kSecKeyWrapPGPECWrapDigestSHA512KekAES256");

#undef SEC_CONST_DECL

CFDataRef
_SecKeyCopyWrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef unwrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    if (error)
        *error = NULL;
    if (outParam)
        *outParam = NULL;
    if (key->key_class->version > 2 && key->key_class->copyWrapKey)
        return key->key_class->copyWrapKey(key, type, unwrappedKey, parameters, outParam, error);
    SecError(errSecUnsupportedOperation, error, CFSTR("No key wrap supported for key %@"), key);
    return NULL;
}

CFDataRef
_SecKeyCopyUnwrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef wrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    if (error)
        *error = NULL;
    if (outParam)
        *outParam = NULL;
    if (key->key_class->version > 2 && key->key_class->copyUnwrapKey)
        return key->key_class->copyUnwrapKey(key, type, wrappedKey, parameters, outParam, error);

    SecError(errSecUnsupportedOperation, error, CFSTR("No key unwrap for key %@"), key);
    return NULL;
}
