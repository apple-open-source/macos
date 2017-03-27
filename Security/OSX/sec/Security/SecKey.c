/*
 * Copyright (c) 2006-2015 Apple Inc. All Rights Reserved.
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
#include <Security/SecBase.h>

#include <Security/SecKeyInternal.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemShim.h>
#include <Security/SecFramework.h>

#include <utilities/SecIOFormat.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/array_size.h>

#include "SecRSAKeyPriv.h"
#include "SecECKeyPriv.h"
#include "SecCTKKeyPriv.h"
#include "SecBasePriv.h"
#include <Security/SecKeyPriv.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
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
#include <Security/SecureTransport.h> /* For error codes. */

#include <corecrypto/ccrng_system.h>

#include <asl.h>
#include <stdlib.h>
#include <syslog.h>

#include <libDER/asn1Types.h>
#include <libDER/DER_Keys.h>
#include <libDER/DER_Encode.h>

CFDataRef SecKeyCopyPublicKeyHash(SecKeyRef key)
{
	CFDataRef pubKeyDigest = NULL, pubKeyBlob = NULL;

	/* encode the public key. */
    require_noerr_quiet(SecKeyCopyPublicBytes(key, &pubKeyBlob), errOut);
    require_quiet(pubKeyBlob, errOut);

	/* Calculate the digest of the public key. */
	require_quiet(pubKeyDigest = SecSHA1DigestCreate(CFGetAllocator(key),
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
    require_noerr_quiet(SecKeyCopyPublicBytes(key, &pubKeyBlob), errOut);
    require_quiet(pubKeyBlob, errOut);

	/* Calculate the digest of the public key. */
	require_quiet(pubKeyDigest = SecSHA1DigestCreate(allocator,
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
	DICT_ADDPAIR(kSecAttrCanEncrypt, privateBlob ? kCFBooleanFalse : kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrCanDecrypt, privateBlob ? kCFBooleanTrue : kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanDerive, kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrCanSign, privateBlob ? kCFBooleanTrue : kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanVerify, privateBlob ? kCFBooleanFalse : kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrCanSignRecover, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanVerifyRecover, kCFBooleanFalse);
	DICT_ADDPAIR(kSecAttrCanWrap, privateBlob ? kCFBooleanFalse : kCFBooleanTrue);
	DICT_ADDPAIR(kSecAttrCanUnwrap, privateBlob ? kCFBooleanTrue : kCFBooleanFalse);
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
#if !TARGET_OS_IPHONE
    CFReleaseSafe(key->cdsaKey);
#endif
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
    if (key1->key_class->version >= 4 && key1->key_class->isEqual)
        return key1->key_class->isEqual(key1, key2);
    if (key1->key_class->extraBytes)
        return !memcmp(key1->key, key2->key, key1->key_class->extraBytes);

    /* TODO: Won't work when we get reference keys. */
    CFDictionaryRef d1, d2;
    d1 = SecKeyCopyAttributeDictionary(key1);
    d2 = SecKeyCopyAttributeDictionary(key2);
    // Returning NULL is an error; bail out of the equality check
    if(!d1 || !d2) {
        CFReleaseSafe(d1);
        CFReleaseSafe(d2);
        return false;
    }
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
static CF_RETURNS_RETAINED CFMutableDictionaryRef merge_params(CFDictionaryRef dict,
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

CFIndex SecKeyGetAlgorithmIdentifier(SecKeyRef key) {
    if (!key || !key->key_class)  {
    // TBD: somehow, a key can be created with a NULL key_class in the
    // SecCertificateCopyPublicKey -> SecKeyCreatePublicFromDER code path
        return kSecNullAlgorithmID;
    }
    /* This method was added to version 1 keys. */
    if (key->key_class->version > 0 && key->key_class->getAlgorithmID) {
        return key->key_class->getAlgorithmID(key);
    }
    /* All version 0 keys were RSA. */
    return kSecRSAAlgorithmID;
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
    CFStringRef tokenID = CFDictionaryGetValue(parameters, kSecAttrTokenID);

    require_quiet(ktype, errOut);

    if (tokenID != NULL) {
        result = SecCTKKeyGeneratePair(parameters, &pubKey, &privKey);
    } else if (CFEqual(ktype, kSecAttrKeyTypeECSECPrimeRandom)) {
        result = SecECKeyGeneratePair(parameters, &pubKey, &privKey);
    } else if (CFEqual(ktype, kSecAttrKeyTypeRSA)) {
        result = SecRSAKeyGeneratePair(parameters, &pubKey, &privKey);
    }

    require_noerr_quiet(result, errOut);

    /* Store the keys in the keychain if they are marked as permanent. */
    if (getBoolForKey(pubParams, kSecAttrIsPermanent, false)) {
        require_noerr_quiet(result = add_ref(pubKey, pubParams), errOut);
    }
    /* Token-based private keys are automatically stored on the token. */
    if (tokenID == NULL && getBoolForKey(privParams, kSecAttrIsPermanent, false)) {
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
    return SecKeyCopyPublicKey(privateKey);
}

CFDictionaryRef CreatePrivateKeyMatchingQuery(SecKeyRef publicKey, bool returnPersistentRef)
{
    const CFTypeRef refType = (returnPersistentRef) ? kSecReturnPersistentRef: kSecReturnRef;

    CFDataRef public_key_hash = SecKeyCopyPublicKeyHash(publicKey);

    CFDictionaryRef query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         kSecClass,                 kSecClassKey,
                                                         kSecAttrKeyClass,          kSecAttrKeyClassPrivate,
                                                         kSecAttrSynchronizable,    kSecAttrSynchronizableAny,
                                                         kSecAttrApplicationLabel,  public_key_hash,
                                                         refType,                   kCFBooleanTrue,
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
    SecKeyRef privateKey = NULL;
    CFTypeRef queryResult = NULL;
    CFDictionaryRef query = NULL;

    require_action_quiet(publicKey != NULL, errOut, SecError(errSecParam, error, CFSTR("Null Public Key")));

    query = CreatePrivateKeyMatchingQuery(publicKey, false);

    require_quiet(SecError(SecItemCopyMatching(query, &queryResult), error,
                           CFSTR("Error finding private key from public: %@"), publicKey), errOut);

    if (CFGetTypeID(queryResult) == SecKeyGetTypeID()) {
        privateKey = (SecKeyRef) queryResult;
        queryResult = NULL;
    }

errOut:
    CFReleaseNull(query);
    CFReleaseNull(queryResult);
    return privateKey;
}

OSStatus SecKeyGetMatchingPrivateKeyStatus(SecKeyRef publicKey, CFErrorRef *error) {
    OSStatus retval = errSecParam;
    CFTypeRef private_key = NULL;
    CFDictionaryRef query = NULL;

    require_action_quiet(publicKey != NULL, errOut, SecError(errSecParam, error, NULL, CFSTR("Null Public Key")));

    query = CreatePrivateKeyMatchingQuery(publicKey, false);

    retval = SecItemCopyMatching(query, &private_key);

    if (!retval && CFGetTypeID(private_key) != SecKeyGetTypeID()) {
        retval = errSecInternalComponent;
    }

errOut:
    CFReleaseNull(query);
    CFReleaseNull(private_key);
    return retval;
}


SecKeyRef SecKeyCreatePublicFromDER(CFAllocatorRef allocator,
                                    const SecAsn1Oid *oid, const SecAsn1Item *params,
                                    const SecAsn1Item *keyData) {
    SecKeyRef publicKey = NULL;
	if (SecAsn1OidCompare(oid, &CSSMOID_RSA)) {
        /* pkcs1 1 */
        /* Note that we call SecKeyCreateRSAPublicKey_ios directly instead of
           SecKeyCreateRSAPublicKey, since on OS X the latter function will return
           a CSSM SecKeyRef, and we always want an iOS format SecKeyRef here.
         */
		publicKey = SecKeyCreateRSAPublicKey_ios(allocator,
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
		publicKey = SecKeyCreateECPublicKey(allocator,
                                            (const uint8_t *)&derKey, sizeof(derKey), kSecDERKeyEncoding);
    } else {
		secwarning("Unsupported algorithm oid");
	}

    return publicKey;
}


SecKeyRef SecKeyCreateFromSubjectPublicKeyInfoData(CFAllocatorRef allocator, CFDataRef subjectPublicKeyInfoData)
{
    DERReturn drtn;

    DERItem subjectPublicKeyInfoDER = {
        .data = (uint8_t *)CFDataGetBytePtr(subjectPublicKeyInfoData),
        .length = (DERSize)CFDataGetLength(subjectPublicKeyInfoData),
    };
    DERSubjPubKeyInfo subjectPublicKeyInfo;
    DERAlgorithmId algorithmId;
    DERItem pubKeyBytes;

    drtn = DERParseSequence(&subjectPublicKeyInfoDER,
                            DERNumSubjPubKeyInfoItemSpecs, DERSubjPubKeyInfoItemSpecs,
                            &subjectPublicKeyInfo, sizeof(subjectPublicKeyInfo));

    require_noerr_quiet(drtn, out);

    drtn = DERParseSequenceContent(&subjectPublicKeyInfo.algId,
                                   DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
                                   &algorithmId, sizeof(algorithmId));
    require_noerr_quiet(drtn, out);

    DERByte unusedBits;
    drtn = DERParseBitString(&subjectPublicKeyInfo.pubKey, &pubKeyBytes, &unusedBits);
    require_noerr_quiet(drtn, out);

    /* Convert DERItem to SecAsn1Item : */
    const SecAsn1Oid oid = { .Data = algorithmId.oid.data, .Length = algorithmId.oid.length };
    const SecAsn1Item params = { .Data = algorithmId.params.data, .Length = algorithmId.params.length };
    const SecAsn1Item pubKey = { .Data = pubKeyBytes.data, .Length = pubKeyBytes.length };

    return SecKeyCreatePublicFromDER(allocator, &oid, &params, &pubKey);

out:

    return NULL;

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

static SecKeyAlgorithm SecKeyGetSignatureAlgorithmForPadding(SecKeyRef key, SecPadding padding) {
    switch (SecKeyGetAlgorithmIdentifier(key)) {
        case kSecRSAAlgorithmID:
            switch (padding) {
                case kSecPaddingNone:
                    return kSecKeyAlgorithmRSASignatureRaw;
                case kSecPaddingPKCS1:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw;
#if TARGET_OS_IPHONE
                case kSecPaddingPKCS1SHA1:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1;
                case kSecPaddingPKCS1SHA224:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224;
                case kSecPaddingPKCS1SHA256:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256;
                case kSecPaddingPKCS1SHA384:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384;
                case kSecPaddingPKCS1SHA512:
                    return kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512;
#else
                    // On CSSM-based implementation, these functions actually did hash its input,
                    // so keep doing that for backward compatibility.
                case kSecPaddingPKCS1SHA1:
                    return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1;
                case kSecPaddingPKCS1SHA224:
                    return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224;
                case kSecPaddingPKCS1SHA256:
                    return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256;
                case kSecPaddingPKCS1SHA384:
                    return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384;
                case kSecPaddingPKCS1SHA512:
                    return kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512;
#endif
                default:
                    return NULL;
            }
        case kSecECDSAAlgorithmID:
            switch (padding) {
                case kSecPaddingSigRaw:
                    return kSecKeyAlgorithmECDSASignatureRFC4754;
                default:
                    // Although it is not very logical, previous SecECKey implementation really considered
                    // anything else than SigRaw (incl. None!) as PKCS1 (i.e. x962), so we keep the behaviour
                    // for backward compatibility.
                    return kSecKeyAlgorithmECDSASignatureDigestX962;
            }
        default:
            return NULL;
    }
}

// Generic wrapper helper for invoking new-style CFDataRef-based operations with ptr/length arguments
// used by legacy RawSign-style functions.
static OSStatus SecKeyPerformLegacyOperation(SecKeyRef key,
                                             const uint8_t *in1Ptr, size_t in1Len,
                                             const uint8_t *in2Ptr, size_t in2Len,
                                             uint8_t *outPtr, size_t *outLen,
                                             CFTypeRef (^operation)(CFDataRef in1, CFDataRef in2, CFRange *resultRange, CFErrorRef *error)) {
    CFErrorRef error = NULL;
    OSStatus status = errSecSuccess;
    CFDataRef in1 = CFDataCreateWithBytesNoCopy(NULL, in1Ptr, in1Len, kCFAllocatorNull);
    CFDataRef in2 = CFDataCreateWithBytesNoCopy(NULL, in2Ptr, in2Len, kCFAllocatorNull);
    CFRange range = { 0, -1 };
    CFTypeRef output = operation(in1, in2, &range, &error);
    require_quiet(output, out);
    if (CFGetTypeID(output) == CFDataGetTypeID() && outLen != NULL) {
        if (range.length == -1) {
            range.length = CFDataGetLength(output);
        }
        require_action_quiet((size_t)range.length <= *outLen, out,
                             SecError(errSecParam, &error, CFSTR("buffer too small")));
        *outLen = range.length;
        CFDataGetBytes(output, range, outPtr);
    }

out:
    CFReleaseSafe(in1);
    CFReleaseSafe(in2);
    CFReleaseSafe(output);
    if (error != NULL) {
        status = SecErrorGetOSStatus(error);
        if (status == errSecVerifyFailed) {
            // Legacy functions used errSSLCrypto, while new implementation uses errSecVerifyFailed.
            status = errSSLCrypto;
        }
        CFRelease(error);
    }
    return status;
}

OSStatus SecKeyRawSign(
                       SecKeyRef           key,            /* Private key */
                       SecPadding          padding,		/* kSecPaddingNone or kSecPaddingPKCS1 */
                       const uint8_t       *dataToSign,	/* signature over this data */
                       size_t              dataToSignLen,	/* length of dataToSign */
                       uint8_t             *sig,			/* signature, RETURNED */
                       size_t              *sigLen) {		/* IN/OUT */
    SecKeyAlgorithm algorithm = SecKeyGetSignatureAlgorithmForPadding(key, padding);
    if (algorithm == NULL) {
        return errSecParam;
    }
    return SecKeyPerformLegacyOperation(key, dataToSign, dataToSignLen, NULL, 0, sig, sigLen,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyCreateSignature(key, algorithm, in1, error);
                                        });
}

OSStatus SecKeyRawVerify(
                         SecKeyRef           key,            /* Public key */
                         SecPadding          padding,		/* kSecPaddingNone or kSecPaddingPKCS1 */
                         const uint8_t       *signedData,	/* signature over this data */
                         size_t              signedDataLen,	/* length of dataToSign */
                         const uint8_t       *sig,			/* signature */
                         size_t              sigLen) {		/* length of signature */
    SecKeyAlgorithm algorithm = SecKeyGetSignatureAlgorithmForPadding(key, padding);
    if (algorithm == NULL) {
        return errSecParam;
    }
    OSStatus status = SecKeyPerformLegacyOperation(key, signedData, signedDataLen, sig, sigLen, NULL, NULL,
                                                   ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                                       return SecKeyVerifySignature(key, algorithm, in1, in2, error)
                                                       ? kCFBooleanTrue : NULL;
                                                   });
    return status;
}

static SecKeyAlgorithm SecKeyGetEncryptionAlgorithmForPadding(SecKeyRef key, SecPadding padding) {
    switch (SecKeyGetAlgorithmIdentifier(key)) {
        case kSecRSAAlgorithmID:
            switch (padding) {
                case kSecPaddingNone:
                    return kSecKeyAlgorithmRSAEncryptionRaw;
                case kSecPaddingPKCS1:
                    return kSecKeyAlgorithmRSAEncryptionPKCS1;
                case kSecPaddingOAEP:
                    return kSecKeyAlgorithmRSAEncryptionOAEPSHA1;
                default:
                    return NULL;
            }
        default:
            return NULL;
    }
}

OSStatus SecKeyEncrypt(
                       SecKeyRef           key,                /* Public key */
                       SecPadding          padding,			/* kSecPaddingNone, kSecPaddingPKCS1, kSecPaddingOAEP */
                       const uint8_t		*plainText,
                       size_t              plainTextLen,		/* length of plainText */
                       uint8_t             *cipherText,
                       size_t              *cipherTextLen) {	/* IN/OUT */
    SecKeyAlgorithm algorithm = SecKeyGetEncryptionAlgorithmForPadding(key, padding);
    if (algorithm == NULL) {
        return errSecParam;
    }

    return SecKeyPerformLegacyOperation(key, plainText, plainTextLen, NULL, 0, cipherText, cipherTextLen,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyCreateEncryptedData(key, algorithm, in1, error);
                                        });
}

OSStatus SecKeyDecrypt(
                       SecKeyRef           key,                /* Private key */
                       SecPadding          padding,			/* kSecPaddingNone, kSecPaddingPKCS1, kSecPaddingOAEP */
                       const uint8_t       *cipherText,
                       size_t              cipherTextLen,		/* length of cipherText */
                       uint8_t             *plainText,
                       size_t              *plainTextLen) {	/* IN/OUT */
    SecKeyAlgorithm algorithm = SecKeyGetEncryptionAlgorithmForPadding(key, padding);
    if (algorithm == NULL) {
        return errSecParam;
    }
    return SecKeyPerformLegacyOperation(key, cipherText, cipherTextLen, NULL, 0, plainText, plainTextLen,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            CFDataRef decrypted = SecKeyCreateDecryptedData(key, algorithm, in1, error);
                                            const UInt8 *data;
                                            if (decrypted != NULL && algorithm == kSecKeyAlgorithmRSAEncryptionRaw &&
                                                *(data = CFDataGetBytePtr(decrypted)) == 0x00) {
                                                // Strip zero-padding from the beginning of the block, as the contract of this
                                                // function says.
                                                range->length = CFDataGetLength(decrypted);
                                                while (*data == 0x00 && range->length > 0) {
                                                    range->location++;
                                                    range->length--;
                                                    data++;
                                                }
                                            }
                                            return decrypted;
                                        });
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
    CFErrorRef error = NULL;
    SecKeyRef key = SecKeyCreateWithData(CFDictionaryGetValue(refAttributes, kSecValueData), refAttributes, &error);
    if (key == NULL) {
        CFStringRef description = CFErrorCopyDescription(error);
        secwarning("%@", description);
        CFRelease(description);
        CFRelease(error);
    }
    return key;
}

static SecKeyAlgorithm SecKeyGetAlgorithmForSecAsn1AlgId(SecKeyRef key, const SecAsn1AlgId *algId, bool digestData) {
    static const struct TableItem {
        const SecAsn1Oid *oid1, *oid2;
        const SecKeyAlgorithm *algorithms[2];
    } translationTableRSA[] = {
        { &CSSMOID_SHA1WithRSA, &CSSMOID_SHA1, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1,
        } },
        { &CSSMOID_SHA224WithRSA, &CSSMOID_SHA224, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224,
        } },
        { &CSSMOID_SHA256WithRSA, &CSSMOID_SHA256, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256,
        } },
        { &CSSMOID_SHA384WithRSA, &CSSMOID_SHA384, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384,
        } },
        { &CSSMOID_SHA512WithRSA, &CSSMOID_SHA512, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512,
        } },
        { &CSSMOID_MD5, NULL, {
            [false] = &kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5,
            [true] = &kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5,
        } },
        { NULL },
    }, translationTableECDSA[] = {
        { &CSSMOID_ECDSA_WithSHA1, &CSSMOID_SHA1, {
            [false] = &kSecKeyAlgorithmECDSASignatureDigestX962,
            [true] = &kSecKeyAlgorithmECDSASignatureMessageX962SHA1,
        } },
        { &CSSMOID_ECDSA_WithSHA224, &CSSMOID_SHA224, {
            [false] = &kSecKeyAlgorithmECDSASignatureDigestX962,
            [true] = &kSecKeyAlgorithmECDSASignatureMessageX962SHA224,
        } },
        { &CSSMOID_ECDSA_WithSHA256, &CSSMOID_SHA256, {
            [false] = &kSecKeyAlgorithmECDSASignatureDigestX962,
            [true] = &kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
        } },
        { &CSSMOID_ECDSA_WithSHA384, &CSSMOID_SHA384, {
            [false] = &kSecKeyAlgorithmECDSASignatureDigestX962,
            [true] = &kSecKeyAlgorithmECDSASignatureMessageX962SHA384,
        } },
        { &CSSMOID_ECDSA_WithSHA512, &CSSMOID_SHA512, {
            [false] = &kSecKeyAlgorithmECDSASignatureDigestX962,
            [true] = &kSecKeyAlgorithmECDSASignatureMessageX962SHA512,
        } },
        { NULL },
    };

    const struct TableItem *table;
    switch (SecKeyGetAlgorithmIdentifier(key)) {
        case kSecRSAAlgorithmID:
            table = translationTableRSA;
            break;
        case kSecECDSAAlgorithmID:
            table = translationTableECDSA;
            break;
        default:
            return NULL;
    }

    for (; table->oid1 != NULL; table++) {
        if (SecAsn1OidCompare(table->oid1, &algId->algorithm) ||
            (table->oid2 != NULL && SecAsn1OidCompare(table->oid2, &algId->algorithm))) {
            return *table->algorithms[digestData];
        }
    }
    return NULL;
}

OSStatus SecKeyDigestAndVerify(
                               SecKeyRef           key,            /* Private key */
                               const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                               const uint8_t       *dataToDigest,	/* signature over this data */
                               size_t              dataToDigestLen,/* length of dataToDigest */
                               const uint8_t       *sig,			/* signature to verify */
                               size_t              sigLen) {		/* length of sig */

    SecKeyAlgorithm algorithm = SecKeyGetAlgorithmForSecAsn1AlgId(key, algId, true);
    if (algorithm == NULL) {
        return errSecUnimplemented;
    }

    return SecKeyPerformLegacyOperation(key, dataToDigest, dataToDigestLen, sig, sigLen, NULL, NULL,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyVerifySignature(key, algorithm, in1, in2, error) ?
                                            kCFBooleanTrue : NULL;
                                        });
}

OSStatus SecKeyDigestAndSign(
                             SecKeyRef           key,            /* Private key */
                             const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                             const uint8_t       *dataToDigest,	/* signature over this data */
                             size_t              dataToDigestLen,/* length of dataToDigest */
                             uint8_t             *sig,			/* signature, RETURNED */
                             size_t              *sigLen) {		/* IN/OUT */
    SecKeyAlgorithm algorithm = SecKeyGetAlgorithmForSecAsn1AlgId(key, algId, true);
    if (algorithm == NULL) {
        return errSecUnimplemented;
    }

    return SecKeyPerformLegacyOperation(key, dataToDigest, dataToDigestLen, NULL, 0, sig, sigLen,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyCreateSignature(key, algorithm, in1, error);
                                        });
}

OSStatus SecKeyVerifyDigest(
                            SecKeyRef           key,            /* Private key */
                            const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                            const uint8_t       *digestData,	/* signature over this digest */
                            size_t              digestDataLen,/* length of dataToDigest */
                            const uint8_t       *sig,			/* signature to verify */
                            size_t              sigLen) {		/* length of sig */
    SecKeyAlgorithm algorithm = SecKeyGetAlgorithmForSecAsn1AlgId(key, algId, false);
    if (algorithm == NULL) {
        return errSecUnimplemented;
    }

    return SecKeyPerformLegacyOperation(key, digestData, digestDataLen, sig, sigLen, NULL, NULL,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyVerifySignature(key, algorithm, in1, in2, error) ?
                                            kCFBooleanTrue : NULL;
                                        });
}

OSStatus SecKeySignDigest(
                          SecKeyRef           key,            /* Private key */
                          const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                          const uint8_t       *digestData,	/* signature over this digest */
                          size_t              digestDataLen,/* length of digestData */
                          uint8_t             *sig,			/* signature, RETURNED */
                          size_t              *sigLen) {		/* IN/OUT */
    SecKeyAlgorithm algorithm = SecKeyGetAlgorithmForSecAsn1AlgId(key, algId, false);
    if (algorithm == NULL) {
        return errSecUnimplemented;
    }

    return SecKeyPerformLegacyOperation(key, digestData, digestDataLen, NULL, 0, sig, sigLen,
                                        ^CFTypeRef(CFDataRef in1, CFDataRef in2, CFRange *range, CFErrorRef *error) {
                                            return SecKeyCreateSignature(key, algorithm, in1, error);
                                        });
}

CFIndex SecKeyGetAlgorithmId(SecKeyRef key) {
	return SecKeyGetAlgorithmIdentifier(key);
}

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR))
/* On OS X, SecKeyGetAlgorithmID has a different function signature (two arguments,
   with output in the second argument). Therefore, avoid implementing this function here
   if compiling for OS X.
 */
#else
CFIndex SecKeyGetAlgorithmID(SecKeyRef key) {
	return SecKeyGetAlgorithmIdentifier(key);
}
#endif

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

    if (kSecECDSAAlgorithmID == SecKeyGetAlgorithmIdentifier(key)) {
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

#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

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

static SInt32 SecKeyParamsGetSInt32(CFTypeRef value, CFStringRef errName, CFErrorRef *error) {
    SInt32 result = -1;
    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        if (!CFNumberGetValue(value, kCFNumberSInt32Type, &result) || result < 0) {
            SecError(errSecParam, error, CFSTR("Unsupported %@: %@"), errName, value);
        }
    } else if (isString(value)) {
        result = CFStringGetIntValue(value);
        CFStringRef t = CFStringCreateWithFormat(0, 0, CFSTR("%ld"), (long) result);
        if (!CFEqual(t, value) || result < 0) {
            SecError(errSecParam, error, CFSTR("Unsupported %@: %@"), errName, value);
            result = -1;
        }
        CFReleaseSafe(t);
    } else {
        SecError(errSecParam, error, CFSTR("Unsupported %@: %@"), errName, value);
    }
    return result;
}

SecKeyRef SecKeyCreateWithData(CFDataRef keyData, CFDictionaryRef parameters, CFErrorRef *error) {

    SecKeyRef key = NULL;
    CFAllocatorRef allocator = NULL;

    /* First figure out the key type (algorithm). */
    SInt32 algorithm;
    CFTypeRef ktype = CFDictionaryGetValue(parameters, kSecAttrKeyType);
    require_quiet((algorithm = SecKeyParamsGetSInt32(ktype, CFSTR("key type"), error)) >= 0, out);
    SInt32 class;
    CFTypeRef kclass = CFDictionaryGetValue(parameters, kSecAttrKeyClass);
    require_quiet((class = SecKeyParamsGetSInt32(kclass, CFSTR("key class"), error)) >= 0, out);

    switch (class) {
        case 0: // kSecAttrKeyClassPublic
            switch (algorithm) {
                case 42: // kSecAlgorithmRSA
                    key = SecKeyCreateRSAPublicKey(allocator,
                                                   CFDataGetBytePtr(keyData), CFDataGetLength(keyData),
                                                   kSecKeyEncodingBytes);
                    if (key == NULL) {
                        SecError(errSecParam, error, CFSTR("RSA public key creation from data failed"));
                    }
                    break;
                case 43: // kSecAlgorithmECDSA
                case 73: // kSecAlgorithmEC
                    key = SecKeyCreateECPublicKey(allocator,
                                                  CFDataGetBytePtr(keyData), CFDataGetLength(keyData),
                                                  kSecKeyEncodingBytes);
                    if (key == NULL) {
                        SecError(errSecParam, error, CFSTR("EC public key creation from data failed"));
                    }
                    break;
                default:
                    SecError(errSecParam, error, CFSTR("Unsupported public key type: %@"), ktype);
                    break;
            };
            break;
        case 1: // kSecAttrKeyClassPrivate
            if (CFDictionaryGetValue(parameters, kSecAttrTokenID) != NULL) {
                key = SecKeyCreateCTKKey(allocator, parameters, error);
                break;
            }
            switch (algorithm) {
                case 42: // kSecAlgorithmRSA
                    key = SecKeyCreateRSAPrivateKey(allocator,
                                                    CFDataGetBytePtr(keyData), CFDataGetLength(keyData),
                                                    kSecKeyEncodingBytes);
                    if (key == NULL) {
                        SecError(errSecParam, error, CFSTR("RSA private key creation from data failed"));
                    }
                    break;
                case 43: // kSecAlgorithmECDSA
                case 73: // kSecAlgorithmEC
                    key = SecKeyCreateECPrivateKey(allocator,
                                                   CFDataGetBytePtr(keyData), CFDataGetLength(keyData),
                                                   kSecKeyEncodingBytes);
                    if (key == NULL) {
                        SecError(errSecParam, error, CFSTR("EC public key creation from data failed"));
                    }
                    break;
                default:
                    SecError(errSecParam, error, CFSTR("Unsupported private key type: %@"), ktype);
                    break;
            };
            break;
        case 2: // kSecAttrKeyClassSymmetric
            SecError(errSecUnimplemented, error, CFSTR("Unsupported symmetric key type: %@"), ktype);
            break;
        default:
            SecError(errSecParam, error, CFSTR("Unsupported key class: %@"), kclass);
            break;
    }

out:
    return key;
}

CFDataRef SecKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    if (!key->key_class->copyExternalRepresentation) {
        SecError(errSecUnimplemented, error, CFSTR("export not implemented for key %@"), key);
        return NULL;
    }

    return key->key_class->copyExternalRepresentation(key, error);
}

CFDictionaryRef SecKeyCopyAttributes(SecKeyRef key) {
    if (key->key_class->copyDictionary)
        return key->key_class->copyDictionary(key);
    return NULL;
}

SecKeyRef SecKeyCopyPublicKey(SecKeyRef key) {
    SecKeyRef result = NULL;
    if (key->key_class->version >= 4 && key->key_class->copyPublicKey) {
        result = key->key_class->copyPublicKey(key);
        if (result != NULL) {
            return result;
        }
    }

    CFDataRef serializedPublic = NULL;

    require_noerr_quiet(SecKeyCopyPublicBytes(key, &serializedPublic), fail);
    require_quiet(serializedPublic, fail);

    result = SecKeyCreateFromPublicData(kCFAllocatorDefault, SecKeyGetAlgorithmIdentifier(key), serializedPublic);

fail:
    CFReleaseSafe(serializedPublic);
    return result;
}

SecKeyRef SecKeyCreateRandomKey(CFDictionaryRef parameters, CFErrorRef *error) {
    SecKeyRef privKey = NULL, pubKey = NULL;
    OSStatus status = SecKeyGeneratePair(parameters, &pubKey, &privKey);
    SecError(status, error, CFSTR("Key generation failed, error %d"), (int)status);
    CFReleaseSafe(pubKey);
    return privKey;
}

SecKeyRef SecKeyCreateDuplicate(SecKeyRef key) {
    if (key->key_class->version >= 4 && key->key_class->createDuplicate) {
        return key->key_class->createDuplicate(key);
    } else {
        return (SecKeyRef)CFRetain(key);
    }
}

Boolean SecKeySetParameter(SecKeyRef key, CFStringRef name, CFPropertyListRef value, CFErrorRef *error) {
    if (key->key_class->version >= 4 && key->key_class->setParameter) {
        return key->key_class->setParameter(key, name, value, error);
    } else {
        return SecError(errSecUnimplemented, error, CFSTR("setParameter not implemented for %@"), key);
    }
}

#pragma mark Generic algorithm adaptor lookup and invocation

static CFTypeRef SecKeyCopyBackendOperationResult(SecKeyOperationContext *context, SecKeyAlgorithm algorithm,
                                                  CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    CFTypeRef result = kCFNull;
    assert(CFArrayGetCount(context->algorithm) > 0);
    if (context->key->key_class->version >= 4 && context->key->key_class->copyOperationResult != NULL) {
        return context->key->key_class->copyOperationResult(context->key, context->operation, algorithm,
                                                            context->algorithm, context->mode, in1, in2, error);
    }

    // Mapping from algorithms to legacy SecPadding values.
    static const struct {
        const SecKeyAlgorithm *algorithm;
        CFIndex keyAlg;
        SecPadding padding;
    } paddingMap[] = {
        { &kSecKeyAlgorithmRSASignatureRaw, kSecRSAAlgorithmID, kSecPaddingNone },
        { &kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw, kSecRSAAlgorithmID, kSecPaddingPKCS1 },
        { &kSecKeyAlgorithmECDSASignatureRFC4754, kSecECDSAAlgorithmID, kSecPaddingSigRaw },
        { &kSecKeyAlgorithmECDSASignatureDigestX962, kSecECDSAAlgorithmID, kSecPaddingPKCS1 },
        { &kSecKeyAlgorithmRSAEncryptionRaw, kSecRSAAlgorithmID, kSecPaddingNone },
        { &kSecKeyAlgorithmRSAEncryptionPKCS1, kSecRSAAlgorithmID, kSecPaddingPKCS1 },
        { &kSecKeyAlgorithmRSAEncryptionOAEPSHA1, kSecRSAAlgorithmID, kSecPaddingOAEP },
    };
    SecPadding padding = (SecPadding)-1;
    CFIndex keyAlg = SecKeyGetAlgorithmIdentifier(context->key);
    for (size_t i = 0; i < array_size(paddingMap); ++i) {
        if (keyAlg == paddingMap[i].keyAlg && CFEqual(algorithm, *paddingMap[i].algorithm)) {
            padding = paddingMap[i].padding;
            break;
        }
    }
    require_quiet(padding != (SecPadding)-1, out);

    // Check legacy virtual table entries.
    size_t size = 0;
    OSStatus status = errSecSuccess;
    switch (context->operation) {
        case kSecKeyOperationTypeSign:
            if (context->key->key_class->rawSign != NULL) {
                result = kCFBooleanTrue;
                if (context->mode == kSecKeyOperationModePerform) {
                    size = SecKeyGetSize(context->key, kSecKeySignatureSize);
                    result = CFDataCreateMutableWithScratch(NULL, size);
                    status = context->key->key_class->rawSign(context->key, padding,
                                                              CFDataGetBytePtr(in1), CFDataGetLength(in1),
                                                              CFDataGetMutableBytePtr((CFMutableDataRef)result), &size);
                }
            }
            break;
        case kSecKeyOperationTypeVerify:
            if (context->key->key_class->rawVerify != NULL) {
                result = kCFBooleanTrue;
                if (context->mode == kSecKeyOperationModePerform) {
                    status = context->key->key_class->rawVerify(context->key, padding,
                                                                CFDataGetBytePtr(in1), CFDataGetLength(in1),
                                                                CFDataGetBytePtr(in2), CFDataGetLength(in2));
                }
            }
            break;
        case kSecKeyOperationTypeEncrypt:
            if (context->key->key_class->encrypt != NULL) {
                result = kCFBooleanTrue;
                if (context->mode == kSecKeyOperationModePerform) {
                    size = SecKeyGetSize(context->key, kSecKeyEncryptedDataSize);
                    result = CFDataCreateMutableWithScratch(NULL, size);
                    status = context->key->key_class->encrypt(context->key, padding,
                                                              CFDataGetBytePtr(in1), CFDataGetLength(in1),
                                                              CFDataGetMutableBytePtr((CFMutableDataRef)result), &size);
                }
            }
            break;
        case kSecKeyOperationTypeDecrypt:
            if (context->key->key_class->decrypt != NULL) {
                result = kCFBooleanTrue;
                if (context->mode == kSecKeyOperationModePerform) {
                    size = SecKeyGetSize(context->key, kSecKeyEncryptedDataSize);
                    result = CFDataCreateMutableWithScratch(NULL, size);
                    status = context->key->key_class->decrypt(context->key, padding,
                                                              CFDataGetBytePtr(in1), CFDataGetLength(in1),
                                                              CFDataGetMutableBytePtr((CFMutableDataRef)result), &size);
                }
            }
            break;
        default:
            goto out;
    }

    if (status == errSecSuccess) {
        if (CFGetTypeID(result) == CFDataGetTypeID()) {
            CFDataSetLength((CFMutableDataRef)result, size);
        }
    } else {
        SecError(status, error, CFSTR("legacy SecKey backend operation:%d(%d) failed"), (int)context->operation, (int)padding);
        CFReleaseNull(result);
    }

out:
    return result;
}

CFTypeRef SecKeyRunAlgorithmAndCopyResult(SecKeyOperationContext *context, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {

    // Check algorithm array for cycles; if any value of it is duplicated inside, report 'algorithm not found' error.
    CFIndex algorithmCount = CFArrayGetCount(context->algorithm);
    for (CFIndex index = 0; index < algorithmCount - 1; index++) {
        SecKeyAlgorithm indexAlgorithm = CFArrayGetValueAtIndex(context->algorithm, index);
        for (CFIndex tested = index + 1; tested < algorithmCount; tested++) {
            require_quiet(!CFEqual(indexAlgorithm, CFArrayGetValueAtIndex(context->algorithm, tested)), fail);
        }
    }

    SecKeyAlgorithm algorithm = CFArrayGetValueAtIndex(context->algorithm, algorithmCount - 1);
    CFTypeRef output = SecKeyCopyBackendOperationResult(context, algorithm, in1, in2, error);
    if (output != kCFNull) {
        // Backend handled the operation, return result.
        return output;
    }

    // To silence static analyzer.
    CFReleaseSafe(output);

    // Get adaptor which is able to handle requested algorithm.
    SecKeyAlgorithmAdaptor adaptor = SecKeyGetAlgorithmAdaptor(context->operation, algorithm);
    require_quiet(adaptor != NULL, fail);

    // Invoke the adaptor and return result.
    CFTypeRef result = adaptor(context, in1, in2, error);
    require_quiet(result != kCFNull, fail);
    return result;

fail:
    if (context->mode == kSecKeyOperationModePerform) {
        SecError(errSecParam, error, CFSTR("%@: algorithm not supported by the key %@"),
                 CFArrayGetValueAtIndex(context->algorithm, 0), context->key);
        return NULL;
    } else {
        return kCFNull;
    }
}

#pragma mark Algorithm-related SecKey API entry points

static CFMutableArrayRef SecKeyCreateAlgorithmArray(SecKeyAlgorithm algorithm) {
    CFMutableArrayRef result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(result, algorithm);
    return result;
}

CFDataRef SecKeyCreateSignature(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef dataToSign, CFErrorRef *error) {
    SecKeyOperationContext context = { key, kSecKeyOperationTypeSign, SecKeyCreateAlgorithmArray(algorithm) };
    CFDataRef result = SecKeyRunAlgorithmAndCopyResult(&context, dataToSign, NULL, error);
    SecKeyOperationContextDestroy(&context);
    return result;
}

Boolean SecKeyVerifySignature(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef signedData, CFDataRef signature,
                              CFErrorRef *error) {
    SecKeyOperationContext context = { key, kSecKeyOperationTypeVerify, SecKeyCreateAlgorithmArray(algorithm) };
    CFTypeRef res = SecKeyRunAlgorithmAndCopyResult(&context, signedData, signature, error);
    Boolean result = CFEqualSafe(res, kCFBooleanTrue);
    CFReleaseSafe(res);
    SecKeyOperationContextDestroy(&context);
    return result;
}

CFDataRef SecKeyCreateEncryptedData(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef plainText, CFErrorRef *error) {
    SecKeyOperationContext context = { key, kSecKeyOperationTypeEncrypt, SecKeyCreateAlgorithmArray(algorithm) };
    CFDataRef result = SecKeyRunAlgorithmAndCopyResult(&context, plainText, NULL, error);
    SecKeyOperationContextDestroy(&context);
    return result;
}

CFDataRef SecKeyCreateDecryptedData(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef cipherText, CFErrorRef *error) {
    SecKeyOperationContext context = { key, kSecKeyOperationTypeDecrypt, SecKeyCreateAlgorithmArray(algorithm) };
    CFDataRef result = SecKeyRunAlgorithmAndCopyResult(&context, cipherText, NULL, error);
    SecKeyOperationContextDestroy(&context);
    return result;
}

CFDataRef SecKeyCopyKeyExchangeResult(SecKeyRef key, SecKeyAlgorithm algorithm, SecKeyRef publicKey,
                                      CFDictionaryRef parameters, CFErrorRef *error) {
    CFDataRef publicKeyData = NULL, result = NULL;
    SecKeyOperationContext context = { key, kSecKeyOperationTypeKeyExchange, SecKeyCreateAlgorithmArray(algorithm) };
    require_quiet(publicKeyData = SecKeyCopyExternalRepresentation(publicKey, error), out);
    result = SecKeyRunAlgorithmAndCopyResult(&context, publicKeyData, parameters, error);

out:
    CFReleaseSafe(publicKeyData);
    SecKeyOperationContextDestroy(&context);
    return result;
}

Boolean SecKeyIsAlgorithmSupported(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm) {
    SecKeyOperationContext context = { key, operation, SecKeyCreateAlgorithmArray(algorithm), kSecKeyOperationModeCheckIfSupported };
    CFErrorRef error = NULL;
    CFTypeRef res = SecKeyRunAlgorithmAndCopyResult(&context, NULL, NULL, &error);
    Boolean result = CFEqualSafe(res, kCFBooleanTrue);
    CFReleaseSafe(res);
    CFReleaseSafe(error);
    SecKeyOperationContextDestroy(&context);
    return result;
}
