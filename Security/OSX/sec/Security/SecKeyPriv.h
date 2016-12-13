/*
 * Copyright (c) 2006-2010,2012-2015 Apple Inc. All Rights Reserved.
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

/*!
	@header SecKeyPriv
	The functions provided in SecKeyPriv.h implement and manage a particular
    type of keychain item that represents a key.  A key can be stored in a
    keychain, but a key can also be a transient object.

	You can use a key as a keychain item in most functions.
*/

#ifndef _SECURITY_SECKEYPRIV_H_
#define _SECURITY_SECKEYPRIV_H_

#include <Security/SecKey.h>
#include <Security/SecAsn1Types.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFDictionary.h>

__BEGIN_DECLS

typedef struct __SecDERKey {
	uint8_t             *oid;
	CFIndex             oidLength;

	uint8_t             *parameters;
	CFIndex             parametersLength;

    /* Contents of BIT STRING in DER Encoding */
	uint8_t             *key;
	CFIndex             keyLength;
} SecDERKey;


typedef uint32_t SecKeyEncoding;
enum {
    /* Typically only used for symmetric keys. */
    kSecKeyEncodingRaw = 0,

    /* RSA keys are DER-encoded according to PKCS1. */
    kSecKeyEncodingPkcs1 = 1,

    /* RSA keys are DER-encoded according to PKCS1 with Apple Extensions. */
    kSecKeyEncodingApplePkcs1 = 2,

    /* RSA public key in SecRSAPublicKeyParams format.  keyData is a pointer
       to a SecRSAPublicKeyParams and keyDataLength is
       sizeof(SecRSAPublicKeyParams). */
    kSecKeyEncodingRSAPublicParams = 3,

    /* RSA public key in SecRSAPublicKeyParams format.  keyData is a pointer
       to a SecRSAPublicKeyParams and keyDataLength is
       sizeof(SecRSAPublicKeyParams). */
    kSecDERKeyEncoding = 4,

    /* Internal "encodings to send other data" */
    kSecGenerateKey = 5,
    kSecExtractPublicFromPrivate = 6,

    /* Encoding came from SecKeyCopyPublicBytes for a public key,
       or internally from a private key */
    kSecKeyEncodingBytes = 7,

    /* Handing in a private key from corecrypto directly. */
    kSecKeyCoreCrypto = 8,

};

typedef uint32_t SecKeyWrapType;
enum {
    /* wrap key in RFC3394 (AESWrap) */
    kSecKeyWrapRFC3394 = 0,

    /* wrap key in PGP style (support EC keys only right now) */
    kSecKeyWrapPublicKeyPGP = 1,

};

typedef CF_ENUM(CFIndex, SecKeyOperationMode) {
    kSecKeyOperationModePerform = 0,
    kSecKeyOperationModeCheckIfSupported = 1,
};

typedef OSStatus (*SecKeyInitMethod)(SecKeyRef, const uint8_t *, CFIndex,
    SecKeyEncoding);
typedef void  (*SecKeyDestroyMethod)(SecKeyRef);
typedef OSStatus (*SecKeyRawSignMethod)(SecKeyRef key, SecPadding padding,
	const uint8_t *dataToSign, size_t dataToSignLen,
	uint8_t *sig, size_t *sigLen);
typedef OSStatus (*SecKeyRawVerifyMethod)(
    SecKeyRef key, SecPadding padding, const uint8_t *signedData,
    size_t signedDataLen, const uint8_t *sig, size_t sigLen);
typedef OSStatus (*SecKeyEncryptMethod)(SecKeyRef key, SecPadding padding,
    const uint8_t *plainText, size_t plainTextLen,
	uint8_t *cipherText, size_t *cipherTextLen);
typedef OSStatus (*SecKeyDecryptMethod)(SecKeyRef key, SecPadding padding,
    const uint8_t *cipherText, size_t cipherTextLen,
    uint8_t *plainText, size_t *plainTextLen);
typedef OSStatus (*SecKeyComputeMethod)(SecKeyRef key,
    const uint8_t *pub_key, size_t pub_key_len,
    uint8_t *computed_key, size_t *computed_key_len);
typedef size_t (*SecKeyBlockSizeMethod)(SecKeyRef key);
typedef CFDictionaryRef (*SecKeyCopyDictionaryMethod)(SecKeyRef key);
typedef CFIndex (*SecKeyGetAlgorithmIDMethod)(SecKeyRef key);
typedef OSStatus (*SecKeyCopyPublicBytesMethod)(SecKeyRef key, CFDataRef *serialization);
typedef CFDataRef (*SecKeyCopyWrapKeyMethod)(SecKeyRef key, SecKeyWrapType type, CFDataRef unwrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error);
typedef CFDataRef (*SecKeyCopyUnwrapKeyMethod)(SecKeyRef key, SecKeyWrapType type, CFDataRef wrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error);
typedef CFStringRef (*SecKeyDescribeMethod)(SecKeyRef key);

typedef CFDataRef (*SecKeyCopyExternalRepresentationMethod)(SecKeyRef key, CFErrorRef *error);
typedef SecKeyRef (*SecKeyCopyPublicKeyMethod)(SecKeyRef key);
typedef Boolean (*SecKeyIsEqualMethod)(SecKeyRef key1, SecKeyRef key2);
typedef SecKeyRef (*SecKeyCreateDuplicateMethod)(SecKeyRef key);
typedef Boolean (*SecKeySetParameterMethod)(SecKeyRef key, CFStringRef name, CFPropertyListRef value, CFErrorRef *error);

/*!
 @abstract Performs cryptographic operation with the key.
 @param key Key to perform the operation on.
 @param operation Type of operation to be performed.
 @param algorithm Algorithm identifier for the operation.  Determines format of input and output data.
 @param allAlgorithms Array of algorithms which were traversed until we got to this operation.  The last member of this array is always the same as @c algorithm parameter.
 @param mode Mode in which the operation is performed.  Two available modes are checking only if the operation can be performed or actually performing the operation.
 @param in1 First input parameter for the operation, meaningful only in ModePerform.
 @param in2 Second input parameter for the operation, meaningful only in ModePerform.
 @param error Error details when NULL is returned.
 @return NULL if some failure occured. kCFNull if operation/algorithm/key combination is not supported, otherwise the result of the operation or kCFBooleanTrue in ModeCheckIfSupported.
 */
typedef CFTypeRef(*SecKeyCopyOperationResultMethod)(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm, CFArrayRef allAlgorithms, SecKeyOperationMode mode, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error);

#define kSecKeyDescriptorVersion  (4)

typedef struct __SecKeyDescriptor {
    /* Version of this SecKeyDescriptor.  Must be kSecKeyDescriptorVersion. */
    uint32_t version;

    /* Name of this key class for use by SecKeyShow(). */
    const char *name;

    /* If nonzero, SecKeyCreate will allocate this many bytes for the key
       field in the SecKeyRef it creates.  If zero key is NULL and the
       implementor can choose to dynamically allocate it in the init
       function and free it in the destroy function.  */
    uint32_t extraBytes;

    /* Called by SecKeyCreate(). */
    SecKeyInitMethod init;
    /* Called by destructor (final CFRelease() or gc if using). */
    SecKeyDestroyMethod destroy;
    /* Called by SecKeyRawSign(). */
    SecKeyRawSignMethod rawSign;
    /* Called by SecKeyRawVerify(). */
    SecKeyRawVerifyMethod rawVerify;
    /* Called by SecKeyEncrypt(). */
    SecKeyEncryptMethod encrypt;
    /* Called by SecKeyDecrypt(). */
    SecKeyDecryptMethod decrypt;
    /* Reserved for future use. */
    SecKeyComputeMethod compute;
    /* Called by SecKeyGetBlockSize(). */
    SecKeyBlockSizeMethod blockSize;
    /* Called by SecKeyCopyAttributeDictionary(), which is private. */
    SecKeyCopyDictionaryMethod copyDictionary;
    /* Called by SecKeyDescribeMethod(). */
    SecKeyDescribeMethod describe;
#if kSecKeyDescriptorVersion > 0
    /* Called by SecKeyCopyAttributeDictionary(), which is private. */
    SecKeyGetAlgorithmIDMethod getAlgorithmID;
#endif
#if kSecKeyDescriptorVersion > 1
    SecKeyCopyPublicBytesMethod copyPublic;
#endif
#if kSecKeyDescriptorVersion > 2
    SecKeyCopyWrapKeyMethod copyWrapKey;
    SecKeyCopyUnwrapKeyMethod copyUnwrapKey;
#endif
#if kSecKeyDescriptorVersion > 3
    SecKeyCopyExternalRepresentationMethod copyExternalRepresentation;
    SecKeyCopyPublicKeyMethod copyPublicKey;
    SecKeyCopyOperationResultMethod copyOperationResult;
    SecKeyIsEqualMethod isEqual;
    SecKeyCreateDuplicateMethod createDuplicate;
    SecKeySetParameterMethod setParameter;
#endif
} SecKeyDescriptor;

struct __SecKey {
    CFRuntimeBase		_base;

    const SecKeyDescriptor *key_class;

#if !TARGET_OS_IPHONE
    // On OSX, keep optional SecKeyRef which holds dynamically, on-demand created CSSM-based key with the same
    // key material.  It is used to implement SecKeyGetCSSMKey().
    SecKeyRef cdsaKey;
#endif

    /* The actual key handled by class. */
    void *key;
};

/*!
    @function SecKeyCreate
    @abstract Given a private key and data to sign, generate a digital signature.
    @param allocator allocator to use when allocating this key instance.
    @param key_class pointer to a SecKeyDescriptor.
    @param keyData The second argument to the init() function in the key_class.
    @param keyDataLength The third argument to the init() function in the key_class.
    @param encoding The fourth argument to the init() function in the key_class.
    @result A newly allocated SecKeyRef.
 */
SecKeyRef SecKeyCreate(CFAllocatorRef allocator,
    const SecKeyDescriptor *key_class, const uint8_t *keyData,
	CFIndex keyDataLength, SecKeyEncoding encoding);

/* Create a public key from an oid, params and keyData all in DER format. */
SecKeyRef SecKeyCreatePublicFromDER(CFAllocatorRef allocator,
    const SecAsn1Oid *oid1, const SecAsn1Item *params,
    const SecAsn1Item *keyData);

/* Create a public key from a CFData containing a SubjectPublicKeyInfo in DER format. */
SecKeyRef SecKeyCreateFromSubjectPublicKeyInfoData(CFAllocatorRef allocator,
    CFDataRef subjectPublicKeyInfoData);

/* Create public key from private key */
SecKeyRef SecKeyCreatePublicFromPrivate(SecKeyRef privateKey);

/* Get Private Key (if present) by publicKey. */
SecKeyRef SecKeyCopyMatchingPrivateKey(SecKeyRef publicKey, CFErrorRef *error);
OSStatus SecKeyGetMatchingPrivateKeyStatus(SecKeyRef publicKey, CFErrorRef *error);

CFDataRef SecKeyCreatePersistentRefToMatchingPrivateKey(SecKeyRef publicKey, CFErrorRef *error);

/* Return an attribute dictionary used to find a private key by public key hash */
CFDictionaryRef CreatePrivateKeyMatchingQuery(SecKeyRef publicKey, bool returnPersistentRef);

/* Return an attribute dictionary used to store this item in a keychain. */
CFDictionaryRef SecKeyCopyAttributeDictionary(SecKeyRef key);

/* Return a key from an attribute dictionary that was used to store this item
   in a keychain. */
SecKeyRef SecKeyCreateFromAttributeDictionary(CFDictionaryRef refAttributes);

OSStatus SecKeyDigestAndVerify(
    SecKeyRef           key,            /* Public key */
	const SecAsn1AlgId  *algId,         /* algorithm oid/params */
	const uint8_t       *dataToDigest,	/* signature over this data */
	size_t              dataToDigestLen,/* length of dataToDigest */
	const uint8_t       *sig,			/* signature to verify */
	size_t              sigLen); 		/* length of sig */

OSStatus SecKeyDigestAndSign(
    SecKeyRef           key,            /* Private key */
	const SecAsn1AlgId  *algId,         /* algorithm oid/params */
	const uint8_t       *dataToDigest,	/* signature over this data */
	size_t              dataToDigestLen,/* length of dataToDigest */
	uint8_t             *sig,			/* signature, RETURNED */
	size_t              *sigLen);		/* IN/OUT */

OSStatus SecKeyVerifyDigest(
    SecKeyRef           key,            /* Private key */
    const SecAsn1AlgId  *algId,         /* algorithm oid/params */
    const uint8_t       *digestData,	/* signature over this digest */
    size_t              digestDataLen,/* length of dataToDigest */
    const uint8_t       *sig,			/* signature to verify */
    size_t              sigLen);        /* length of sig */

OSStatus SecKeySignDigest(
    SecKeyRef           key,            /* Private key */
    const SecAsn1AlgId  *algId,         /* algorithm oid/params */
    const uint8_t       *digestData,	/* signature over this digest */
    size_t              digestDataLen,/* length of digestData */
    uint8_t             *sig,			/* signature, RETURNED */
    size_t              *sigLen); 		/* IN/OUT */

OSStatus SecKeyCopyPublicBytes(SecKeyRef key, CFDataRef* serializedPublic);
SecKeyRef SecKeyCreateFromPublicBytes(CFAllocatorRef allocator, CFIndex algorithmID, const uint8_t *keyData, CFIndex keyDataLength);
SecKeyRef SecKeyCreateFromPublicData(CFAllocatorRef allocator, CFIndex algorithmID, CFDataRef serialized);
CFDataRef SecKeyCopyPublicKeyHash(SecKeyRef key);

/* This function directly creates an iOS-format SecKeyRef from public key bytes. */
SecKeyRef SecKeyCreateRSAPublicKey_ios(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding);


CF_RETURNS_RETAINED
CFDictionaryRef SecKeyGeneratePrivateAttributeDictionary(SecKeyRef key,
                                                         CFTypeRef keyType,
                                                         CFDataRef privateBlob);
CF_RETURNS_RETAINED
CFDictionaryRef SecKeyGeneratePublicAttributeDictionary(SecKeyRef key, CFTypeRef keyType);

enum {
    kSecNullAlgorithmID = 0,
    kSecRSAAlgorithmID = 1,
    kSecDSAAlgorithmID = 2,   /* unsupported, just here for reference. */
    kSecECDSAAlgorithmID = 3,
};

/*!
	@function SecKeyGetAlgorithmID
	@abstract Returns an enumerated constant value which identifies the algorithm for the given key.
	@param key A key reference.
	@result An algorithm identifier.
	@discussion Deprecated in iOS 9.0. Note that SecKeyGetAlgorithmID also exists on OS X
	with different arguments for CDSA-based SecKeyRefs, and returns different values.
	For compatibility, your code should migrate to use SecKeyGetAlgorithmId instead.
*/
CFIndex SecKeyGetAlgorithmID(SecKeyRef key)
	__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_8, __IPHONE_5_0, __IPHONE_9_0);

/*!
	@function SecKeyGetAlgorithmId
	@abstract Returns an enumerated constant value which identifies the algorithm for the given key.
	@param key A key reference.
	@result An algorithm identifier.
*/
CFIndex SecKeyGetAlgorithmId(SecKeyRef key)
	__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_9_0);


typedef enum {
    kSecKeyKeySizeInBits        = 0,
    kSecKeySignatureSize        = 1,
    kSecKeyEncryptedDataSize    = 2,
    // More might belong here, but we aren't settled on how
    // to take into account padding and/or digest types.
} SecKeySize;

/*!
 @function SecKeyGetSize
 @abstract Returns a size in bytes.
 @param key The key for which the block length is requested.
 @param whichSize The size that you want evaluated.
 @result The block length of the key in bytes.
 @discussion If for example key is an RSA key the value returned by
 this function is the size of the modulus.
 */
size_t SecKeyGetSize(SecKeyRef key, SecKeySize whichSize)
__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_5_0);


/*!
 @function SecKeyLookupPersistentRef
 @abstract Looks up a SecKeyRef via persistent ref.
 @param persistentRef The persistent ref data for looking up.
 @param lookedUpData retained SecKeyRef for the found object.
 @result Errors when using SecItemFind for the persistent ref.
 */
OSStatus SecKeyFindWithPersistentRef(CFDataRef persistentRef, SecKeyRef* lookedUpData)
__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

/*!
 @function SecKeyCopyPersistentRef
 @abstract Gets a persistent reference for a key.
 @param key Key to make a persistent ref for.
 @param persistentRef Allocated data representing the persistent ref.
 @result Errors when using SecItemFind for the persistent ref.
 */
OSStatus SecKeyCopyPersistentRef(SecKeyRef key, CFDataRef* persistentRef)
__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);


/*
 *
 */

extern const CFStringRef _kSecKeyWrapPGPSymAlg; /* CFNumber */
extern const CFStringRef _kSecKeyWrapPGPFingerprint; /* CFDataRef, at least 20 bytes */
extern const CFStringRef _kSecKeyWrapPGPWrapAlg; /* kSecKeyWrapRFC6637WrapNNN, or any of the other PGP wrap algs */
extern const CFStringRef _kSecKeyWrapRFC6637Flags;
extern const CFStringRef _kSecKeyWrapRFC6637WrapDigestSHA256KekAES128;
extern const CFStringRef _kSecKeyWrapRFC6637WrapDigestSHA512KekAES256;

enum { kSecKeyWrapPGPFingerprintMinSize = 20 };
/*!
 @function _SecKeyCopyWrapKey
 @abstract Wrap a key
 */

CFDataRef
_SecKeyCopyWrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef unwrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
__OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_8_0);

/*!
 @function _SecKeyWrapKey
 @abstract Unwrap a key
 */

CFDataRef
_SecKeyCopyUnwrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef wrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
__OSX_AVAILABLE_STARTING(__MAC_10_10, __IPHONE_8_0);

/*!
 @enum SecKeyAttestationKeyType
 @abstract Defines types of builtin attestation keys.
*/
typedef CF_ENUM(uint32_t, SecKeyAttestationKeyType)
{
	kSecKeyAttestationKeyTypeSIK = 0,
	kSecKeyAttestationKeyTypeGID
} __OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecKeyCopyAttestationKey
 @abstract Returns a copy of a builtin attestation key.

 @param keyType Type of the requested builtin key.
 @param error An optional pointer to a CFErrorRef. This value is set if an error occurred.

 @result On success a SecKeyRef containing the requested key is returned, on failure it returns NULL.
*/
SecKeyRef SecKeyCopyAttestationKey(SecKeyAttestationKeyType keyType, CFErrorRef *error)
__OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecKeyCreateAttestation
 @abstract Attests a key with another key.

 @param key The attesting key.
 @param keyToAttest The key which is to be attested.
 @param error An optional pointer to a CFErrorRef. This value is set if an error occurred.

 @result On success a CFDataRef containing the attestation data is returned, on failure it returns NULL.
 
 @discussion Key attestation only works for CTK SEP keys, i.e. keys created with kSecAttrTokenID=kSecAttrTokenIDSecureEnclave.
*/
CFDataRef SecKeyCreateAttestation(SecKeyRef key, SecKeyRef keyToAttest, CFErrorRef *error)
__OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecKeySetParameter
 @abstract Sets unspecified key parameter for the backend.

 @param key Key to set the parameter to.
 @param name Identifies parameter to be set.
 @param value New value for the parameter.
 @param error Error which gathers more information when something went wrong.

 @discussion Serves as channel between SecKey client and backend for passing additional sideband data send from SecKey caller
 to SecKey implementation backend.  Parameter names and types are either generic kSecUse*** attributes or are a contract between
 SecKey user (application) and backend and in this case are not interpreted by SecKey layer in any way.
 */
Boolean SecKeySetParameter(SecKeyRef key, CFStringRef name, CFPropertyListRef value, CFErrorRef *error)
__OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 @function SecKeyCreateDuplicate
 @abstract Creates duplicate fo the key.

 @param key Source key to be duplicated

 @discussion Only memory representation of the key is duplicated, so if the key is backed by keychain, only one instance
 stays in the keychain.  Duplicating key is useful for setting 'temporary' key parameters using SecKeySetParameter.
 If the key is immutable (i.e. does not support SecKeySetParameter), calling this method is identical to calling CFRetain().
 */
SecKeyRef SecKeyCreateDuplicate(SecKeyRef key)
__OSX_AVAILABLE(10.12) __IOS_AVAILABLE(10.0) __TVOS_AVAILABLE(10.0) __WATCHOS_AVAILABLE(3.0);

/*!
 Algorithms for converting between bigendian and core-crypto ccunit data representation.
 */
extern const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureRawCCUnit;
extern const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionRawCCUnit;

/*!
 Internal algorithm for RSA-MD5.  We do not want to export MD5 in new API, but we need it
 for implementing legacy interfaces.
 */
extern const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5;
extern const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePKCS1v15MD5;

/*!
 Algorithms for interoperability with libaks smartcard support.
 */
extern const SecKeyAlgorithm kSecKeyAlgorithmECIESEncryptionAKSSmartCard;

__END_DECLS

#endif /* !_SECURITY_SECKEYPRIV_H_ */
