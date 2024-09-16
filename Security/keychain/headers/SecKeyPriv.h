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

#include <Security/SecBase.h>
#include <Security/SecKey.h>
#include <Security/SecAsn1Types.h>
#include <CoreFoundation/CFRuntime.h>

#if SEC_OS_OSX_INCLUDES
#include <Security/x509defs.h>
#endif

#if SEC_OS_OSX
#include <Security/SecKey.h>
#include <AvailabilityMacros.h>
#endif

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

typedef struct SecRSAPublicKeyParams {
    uint8_t             *modulus;            /* modulus */
    CFIndex             modulusLength;
    uint8_t             *exponent;            /* public exponent */
    CFIndex             exponentLength;
} SecRSAPublicKeyParams;

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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

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

#pragma clang diagnostic pop

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
    CFRuntimeBase          _base;

    const SecKeyDescriptor *key_class;

    /* The actual key handled by class. */
    void *key;
};

/* Create a public key from a CFData containing a SubjectPublicKeyInfo in DER format. */
SecKeyRef SecKeyCreateFromSubjectPublicKeyInfoData(CFAllocatorRef allocator,
                                                   CFDataRef subjectPublicKeyInfoData);

/* Create a SubjectPublicKeyInfo in DER format from a SecKey */
CFDataRef SecKeyCopySubjectPublicKeyInfo(SecKeyRef key);


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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/* Create a public key from an oid, params and keyData all in DER format. */
SecKeyRef SecKeyCreatePublicFromDER(CFAllocatorRef allocator,
    const SecAsn1Oid *oid1, const SecAsn1Item *params,
    const SecAsn1Item *keyData);

#pragma clang diagnostic pop

/* Create public key from private key */
SecKeyRef SecKeyCreatePublicFromPrivate(SecKeyRef privateKey);

/* Get Private Key (if present) by publicKey. */
SecKeyRef SecKeyCopyMatchingPrivateKey(SecKeyRef publicKey, CFErrorRef *error);

OSStatus SecKeyGetMatchingPrivateKeyStatus(SecKeyRef publicKey, CFErrorRef *error);
CFDataRef SecKeyCreatePersistentRefToMatchingPrivateKey(SecKeyRef publicKey, CFErrorRef *error);

/* Return an attribute dictionary used to find a private key by public key hash */
CFDictionaryRef CreatePrivateKeyMatchingQuery(SecKeyRef publicKey, bool returnPersistentRef);

/* Return a key from an attribute dictionary that was used to store this item
 in a keychain. */
SecKeyRef SecKeyCreateFromAttributeDictionary(CFDictionaryRef refAttributes);

// SecAsn1AlgId is deprecated, but we still need to use it.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

OSStatus SecKeyDigestAndVerify(
                               SecKeyRef           key,            /* Public key */
                               const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                               const uint8_t       *dataToDigest,     /* signature over this data */
                               size_t              dataToDigestLen,/* length of dataToDigest */
                               const uint8_t       *sig,               /* signature to verify */
                               size_t              sigLen);           /* length of sig */

/* Return an attribute dictionary used to store this item in a keychain. */
CFDictionaryRef SecKeyCopyAttributeDictionary(SecKeyRef key);

OSStatus SecKeyDigestAndSign(
    SecKeyRef           key,            /* Private key */
     const SecAsn1AlgId  *algId,         /* algorithm oid/params */
     const uint8_t       *dataToDigest,     /* signature over this data */
     size_t              dataToDigestLen,/* length of dataToDigest */
     uint8_t             *sig,               /* signature, RETURNED */
     size_t              *sigLen);          /* IN/OUT */

OSStatus SecKeyVerifyDigest(
    SecKeyRef           key,            /* Private key */
    const SecAsn1AlgId  *algId,         /* algorithm oid/params */
    const uint8_t       *digestData,     /* signature over this digest */
    size_t              digestDataLen,/* length of dataToDigest */
    const uint8_t       *sig,               /* signature to verify */
    size_t              sigLen);        /* length of sig */

OSStatus SecKeySignDigest(
    SecKeyRef           key,            /* Private key */
    const SecAsn1AlgId  *algId,         /* algorithm oid/params */
    const uint8_t       *digestData,     /* signature over this digest */
    size_t              digestDataLen,/* length of digestData */
    uint8_t             *sig,               /* signature, RETURNED */
    size_t              *sigLen);           /* IN/OUT */

#pragma clang diagnostic pop

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
    kSecEd25519AlgorithmID = 4,
    kSecX25519AlgorithmID = 5,
    kSecEd448AlgorithmID = 6,
    kSecX448AlgorithmID = 7,
    kSecKyberAlgorithmID = 8,
};

/*!
 @function SecKeyGetAlgorithmId
 @abstract Returns an enumerated constant value which identifies the algorithm for the given key.
 @param key A key reference.
 @result An algorithm identifier.
 */
CFIndex SecKeyGetAlgorithmId(SecKeyRef key)
SPI_AVAILABLE(macos(10.8), ios(9.0));

#if TARGET_OS_IPHONE
/*!
     @function SecKeyGetAlgorithmID
     @abstract Returns an enumerated constant value which identifies the algorithm for the given key.
     @param key A key reference.
     @result An algorithm identifier.
     @discussion Deprecated in iOS 9.0. Note that SecKeyGetAlgorithmID also exists on macOS
     with different arguments for CDSA-based SecKeyRefs, and returns different values.
     For compatibility, your code should migrate to use SecKeyGetAlgorithmId instead.
*/
CFIndex SecKeyGetAlgorithmID(SecKeyRef key)
SPI_DEPRECATED_WITH_REPLACEMENT("SecKeyGetAlgorithmId", ios(5.0, 9.0)) API_UNAVAILABLE(macCatalyst);
#endif // TARGET_OS_IPHONE

#if TARGET_OS_OSX
/*!
 @function SecKeyGetAlgorithmID
 @abstract Returns a pointer to a CSSM_X509_ALGORITHM_IDENTIFIER structure for the given key.
 @param key A key reference.
 @param algid On return, a pointer to a CSSM_X509_ALGORITHM_IDENTIFIER structure.
 @result A result code.  See "Security Error Codes" (SecBase.h).
 @discussion Deprecated in OS X 10.8 and later. Continued use is strongly discouraged,
 since there is a naming conflict with a similar function (also deprecated) on iOS that
 had different arguments and a different return value. Use SecKeyGetAlgorithmId instead.
 */
OSStatus SecKeyGetAlgorithmID(SecKeyRef key, const CSSM_X509_ALGORITHM_IDENTIFIER **algid)
SPI_DEPRECATED_WITH_REPLACEMENT("SecKeyGetAlgorithmId", macos(10.2, 10.8)) API_UNAVAILABLE(ios, tvos, watchos, bridgeos, macCatalyst);
#endif

#if !SEC_OS_OSX
typedef CF_ENUM(int, SecKeySize) {
    kSecKeyKeySizeInBits        = 0,
    kSecKeySignatureSize        = 1,
    kSecKeyEncryptedDataSize    = 2,
    // More might belong here, but we aren't settled on how
    // to take into account padding and/or digest types.
};

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
SPI_DEPRECATED("No longer supported", macos(10.8, 12.0), ios(5.0, 15.0), tvos(5.0, 15.0), watchos(1.0, 8.0));
#endif

/*!
 @function SecKeyLookupPersistentRef
 @abstract Looks up a SecKeyRef via persistent ref.
 @param persistentRef The persistent ref data for looking up.
 @param lookedUpData retained SecKeyRef for the found object.
 @result Errors when using SecItemFind for the persistent ref.
 */
OSStatus SecKeyFindWithPersistentRef(CFDataRef persistentRef, SecKeyRef* lookedUpData)
SPI_DEPRECATED("No longer supported", macos(10.9, 12.0), ios(7.0, 15.0), tvos(7.0, 15.0), watchos(1.0, 8.0));

/*!
 @function SecKeyCopyPersistentRef
 @abstract Gets a persistent reference for a key.
 @param key Key to make a persistent ref for.
 @param persistentRef Allocated data representing the persistent ref.
 @result Errors when using SecItemFind for the persistent ref.
 */
OSStatus SecKeyCopyPersistentRef(SecKeyRef key, CFDataRef* persistentRef)
SPI_DEPRECATED("No longer supported", macos(10.9, 12.0), ios(7.0, 15.0), tvos(7.0, 15.0), watchos(1.0, 8.0));

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
SPI_DEPRECATED("No longer supported", macos(10.10, 12.0), ios(8.0, 15.0), tvos(8.0, 15.0), watchos(1.0, 8.0));

/*!
 @function _SecKeyWrapKey
 @abstract Unwrap a key
 */

CFDataRef
_SecKeyCopyUnwrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef wrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
SPI_DEPRECATED("No longer supported", macos(10.10, 12.0), ios(8.0, 15.0), tvos(8.0, 15.0), watchos(1.0, 8.0));

#if SEC_OS_OSX_INCLUDES
/*!
    @function SecKeyGetStrengthInBits
    @abstract Returns key strength in bits for the given key.
    @param key A key reference.
    @param algid A pointer to a CSSM_X509_ALGORITHM_IDENTIFIER structure, as returned from a call to SecKeyGetAlgorithmID.
    @param strength On return, the key strength in bits.
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecKeyGetStrengthInBits(SecKeyRef key, const CSSM_X509_ALGORITHM_IDENTIFIER *algid, unsigned int *strength)
SPI_DEPRECATED("CSSM_X509_ALGORITHM_IDENTIFIER is deprecated", macos(10.4, 10.14));

/*!
    @function SecKeyImportPair
    @abstract Takes an asymmetric key pair and stores it in the keychain specified by the keychain parameter.
    @param keychainRef A reference to the keychain in which to store the private and public key items. Specify NULL for the default keychain.
    @param publicCssmKey A CSSM_KEY which is valid for the CSP returned by SecKeychainGetCSPHandle(). This may be a normal key or reference key.
    @param privateCssmKey A CSSM_KEY which is valid for the CSP returned by SecKeychainGetCSPHandle(). This may be a normal key or reference key.
    @param initialAccess A SecAccess object that determines the initial access rights to the private key. The public key is given an any/any acl by default.
    @param publicKey Optional output pointer to the keychain item reference of the imported public key. The caller must call CFRelease on this value if it is returned.
    @param privateKey Optional output pointer to the keychain item reference of the imported private key. The caller must call CFRelease on this value if it is returned.
    @result A result code.  See "Security Error Codes" (SecBase.h).
    @deprecated in 10.5 and later. Use the SecItemImport function instead; see <Security/SecImportExport.h>
*/
OSStatus SecKeyImportPair(
        SecKeychainRef keychainRef,
        const CSSM_KEY *publicCssmKey,
        const CSSM_KEY *privateCssmKey,
        SecAccessRef initialAccess,
        SecKeyRef* publicKey,
        SecKeyRef* privateKey)
SPI_DEPRECATED_WITH_REPLACEMENT("SecItemImport", macos(10.0, 10.5)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/*!
    @function SecKeyCreate
    @abstract Create a key reference from the supplied key data.
    @param allocator CFAllocator to allocate the key data. Pass NULL to use the default allocator.
    @param keyClass A descriptor for the particular class of key that is being created.
    @param keyData Data from which to create the key. Specify the format of this data in the encoding parameter.
    @param keyDataLength Length of the data pointed to by keyData.
    @param encoding A value of type SecKeyEncoding which describes the format of keyData.
    @result A key reference.
    @discussion Warning: this function is NOT intended for use outside the Security stack in its current state. <rdar://3201885>
    IMPORTANT: on Mac OS X 10.5 and earlier, the SecKeyCreate function had a different parameter list.
    The current parameter list matches the iPhone OS implementation. Existing clients of this function
    on macOS (and there should not be any outside the Security stack, per the warning above) must
    migrate to the replacement function, SecKeyCreateWithCSSMKey.
*/
SecKeyRef SecKeyCreate(CFAllocatorRef allocator,
    const SecKeyDescriptor *keyClass, const uint8_t *keyData,
    CFIndex keyDataLength, SecKeyEncoding encoding);

/*!
    @function SecKeyCreateWithCSSMKey
    @abstract Generate a temporary floating key reference for a CSSM_KEY.
    @param key A pointer to a CSSM_KEY structure.
    @param keyRef On return, a key reference.
    @result A result code. See "Security Error Codes" (SecBase.h).
    @discussion Warning: this function is NOT intended for use outside the Security stack in its current state. <rdar://3201885>
*/
OSStatus SecKeyCreateWithCSSMKey(const CSSM_KEY *key, SecKeyRef* keyRef)
SPI_DEPRECATED("CSSM_KEY is deprecated", macos(10.11, 10.14)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

/* Convenience function to determine type of key instance. */
Boolean SecKeyIsLegacyInstance(SecKeyRef keyRef)
SPI_AVAILABLE(macos(15.0)) API_UNAVAILABLE(ios, watchos, tvos, bridgeos, macCatalyst);

#if TARGET_OS_OSX
// Alias macOS versions of this deprecated SPI to unique macOS names. Undecorated names are used for macCatalyst.
#define SecKeyRawSign SecKeyRawSign_macOS
#define SecKeyRawVerify SecKeyRawVerify_macOS
#endif

CF_IMPLICIT_BRIDGING_ENABLED

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/*!
    @function SecKeyRawSign
    @abstract Given a private key and data to sign, generate a digital signature.
    @param key Private key with which to sign.
    @param padding See Padding Types above, typically kSecPaddingPKCS1SHA1.
    @param dataToSign The data to be signed, typically the digest of the actual data.
    @param dataToSignLen Length of dataToSign in bytes.
    @param sig Pointer to buffer in which the signature will be returned.
    @param sigLen IN/OUT maximum length of sig buffer on input, actualy length of sig on output.
    @result A result code. See "Security Error Codes" (SecBase.h).
    @discussion If the padding argument is kSecPaddingPKCS1, PKCS1 padding
     will be performed prior to signing. If this argument is kSecPaddingNone,
     the incoming data will be signed "as is".

     When PKCS1 padding is performed, the maximum length of data that can
     be signed is the value returned by SecKeyGetBlockSize() - 11.

     NOTE: The behavior this function with kSecPaddingNone is undefined if the
     first byte of dataToSign is zero; there is no way to verify leading zeroes
     as they are discarded during the calculation.

     If you want to generate a proper PKCS1 style signature with DER encoding of
     the digest type - and the dataToSign is a SHA1 digest - use kSecPaddingPKCS1SHA1.
*/
OSStatus SecKeyRawSign(
        SecKeyRef           key,
        SecPadding          padding,
        const uint8_t       *dataToSign,
        size_t              dataToSignLen,
        uint8_t             *sig,
        size_t              *sigLen)
SPI_DEPRECATED("Use SecKeyCreateSignature", macos(10.7, 10.15));


/*!
    @function SecKeyRawVerify
    @abstract Given a public key, data which has been signed, and a signature, verify the signature.
    @param key Public key with which to verify the signature.
    @param padding See Padding Types above, typically kSecPaddingPKCS1SHA1.
    @param signedData The data over which sig is being verified, typically the digest of the actual data.
    @param signedDataLen Length of signedData in bytes.
    @param sig Pointer to the signature to verify.
    @param sigLen Length of sig in  bytes.
    @result A result code. See "Security Error Codes" (SecBase.h).
    @discussion If the padding argument is kSecPaddingPKCS1, PKCS1 padding
     will be checked during verification. If this argument is kSecPaddingNone,
     the incoming data will be compared directly to sig.

     If you are verifying a proper PKCS1-style signature, with DER encoding of the digest
     type - and the signedData is a SHA1 digest - use kSecPaddingPKCS1SHA1.
*/
OSStatus SecKeyRawVerify(
         SecKeyRef           key,
         SecPadding          padding,
         const uint8_t       *signedData,
         size_t              signedDataLen,
         const uint8_t       *sig,
         size_t              sigLen)
SPI_DEPRECATED("Use SecKeyVerifySignature", macos(10.7, 10.15));

CF_IMPLICIT_BRIDGING_DISABLED

/*!
    @function SecKeyEncrypt
    @abstract Encrypt a block of plaintext.
    @param key Public key with which to encrypt the data.
    @param padding See Padding Types above, typically kSecPaddingPKCS1.
    @param plainText The data to encrypt.
    @param plainTextLen Length of plainText in bytes, this must be less
     or equal to the value returned by SecKeyGetBlockSize().
    @param cipherText Pointer to the output buffer.
    @param cipherTextLen On input, specifies how much space is available at
     cipherText; on return, it is the actual number of cipherText bytes written.
    @result A result code. See "Security Error Codes" (SecBase.h).
    @discussion If the padding argument is kSecPaddingPKCS1, PKCS1 padding
     will be performed prior to encryption. If this argument is kSecPaddingNone,
     the incoming data will be encrypted "as is".

     When PKCS1 padding is performed, the maximum length of data that can
     be encrypted is the value returned by SecKeyGetBlockSize() - 11.

     When memory usage is a critical issue, note that the input buffer
     (plainText) can be the same as the output buffer (cipherText).
*/
OSStatus SecKeyEncrypt(
       SecKeyRef           key,
       SecPadding          padding,
       const uint8_t       *plainText,
       size_t              plainTextLen,
       uint8_t             *cipherText,
       size_t              *cipherTextLen)
SPI_DEPRECATED("Use SecKeyCreateEncryptedData", macos(10.7, 10.15));


/*!
    @function SecKeyDecrypt
    @abstract Decrypt a block of ciphertext.
    @param key Private key with which to decrypt the data.
    @param padding See SecPadding types above; typically kSecPaddingPKCS1.
    @param cipherText The data to decrypt.
    @param cipherTextLen Length of cipherText in bytes; this must be less
     or equal to the value returned by SecKeyGetBlockSize().
    @param plainText Pointer to the output buffer.
    @param plainTextLen On input, specifies how much space is available at
     plainText; on return, it is the actual number of plainText bytes written.
    @result A result code. See "Security Error Codes" (SecBase.h).
    @discussion If the padding argument is kSecPaddingPKCS1, PKCS1 padding
     will be removed after decryption. If this argument is kSecPaddingNone,
     the decrypted data will be returned "as is".

     When memory usage is a critical issue, note that the input buffer
     (plainText) can be the same as the output buffer (cipherText).
*/
OSStatus SecKeyDecrypt(
       SecKeyRef           key,             /* Private key */
       SecPadding          padding,            /* kSecPaddingNone, kSecPaddingPKCS1, kSecPaddingOAEP */
       const uint8_t       *cipherText,
       size_t              cipherTextLen,    /* length of cipherText */
       uint8_t             *plainText,
       size_t              *plainTextLen)    /* IN/OUT */
SPI_DEPRECATED("Use SecKeyCreateDecryptedData", macos(10.7, 10.15));

OSStatus SecKeyVerifyDigest(
       SecKeyRef           key,            /* Private key */
       const SecAsn1AlgId  *algId,         /* algorithm oid/params */
       const uint8_t       *digestData,       /* signature over this digest */
       size_t              digestDataLen,  /* length of dataToDigest */
       const uint8_t       *sig,           /* signature to verify */
       size_t              sigLen)         /* length of sig */
SPI_DEPRECATED("Use SecKeyVerifySignature", macos(10.7, 10.15));

OSStatus SecKeySignDigest(
       SecKeyRef           key,            /* Private key */
       const SecAsn1AlgId  *algId,         /* algorithm oid/params */
       const uint8_t       *digestData,    /* signature over this digest */
       size_t              digestDataLen,  /* length of digestData */
       uint8_t             *sig,           /* signature, RETURNED */
       size_t              *sigLen)        /* IN/OUT */
SPI_DEPRECATED("Use SecKeyCreateSignature", macos(10.7, 10.15));
#pragma clang diagnostic pop

#endif  // SEC_OS_OSX_INCLUDES


/* These are the named curves we support. These values come from RFC 4492
 section 5.1.1, with the exception of SSL_Curve_None which means
 "ECDSA not negotiated". */
typedef enum
{
    kSecECCurveNone = -1,
    kSecECCurveSecp256r1 = 23,
    kSecECCurveSecp384r1 = 24,
    kSecECCurveSecp521r1 = 25
} SecECNamedCurve;

/* Return a named curve enum for ecPrivateKey. */
SecECNamedCurve SecECKeyGetNamedCurve(SecKeyRef ecPrivateKey);
CFDataRef SecECKeyCopyPublicBits(SecKeyRef key);

/* Given an RSA public key in encoded form return a SecKeyRef representing
   that key. Supported encodings are kSecKeyEncodingPkcs1. */
SecKeyRef SecKeyCreateRSAPublicKey(CFAllocatorRef allocator,
                                   const uint8_t *keyData, CFIndex keyDataLength,
                                   SecKeyEncoding encoding);

CFDataRef SecKeyCopyModulus(SecKeyRef rsaPublicKey);
CFDataRef SecKeyCopyExponent(SecKeyRef rsaPublicKey);

/*!
 @function SecKeyCopyPublicBytes
 @abstract Gets the bits of a public key
 @param key Key to retrieve the bits.
 @param publicBytes An out parameter to receive the public key bits
 @result Errors if any when retrieving the public key bits..
 */
OSStatus SecKeyCopyPublicBytes(SecKeyRef key, CFDataRef* publicBytes);

/*!
    @function SecKeyCreatePublicFromPrivate
    @abstract Create a public SecKeyRef from a private SecKeyRef
    @param privateKey The private SecKeyRef for which you want the public key
    @result A public SecKeyRef, or NULL if the conversion failed
    @discussion This is a "best attempt" function, hence the SPI nature. If the public
    key bits are not in memory, it attempts to load from the keychain. If the public
    key was not tracked on the keychain, it will fail.
*/
SecKeyRef SecKeyCreatePublicFromPrivate(SecKeyRef privateKey);

/*!
    @function SecKeyCreateFromPublicData
*/
SecKeyRef SecKeyCreateFromPublicData(CFAllocatorRef allocator, CFIndex algorithmID, CFDataRef publicBytes);

#if SEC_OS_OSX_INCLUDES
OSStatus SecKeyRawVerifyOSX(
         SecKeyRef           key,
         SecPadding          padding,
         const uint8_t       *signedData,
         size_t              signedDataLen,
         const uint8_t       *sig,
         size_t              sigLen)
SPI_DEPRECATED("Use SecKeyVerifySignature", macos(10.7, 10.15));

#endif // SEC_OS_OSX_INCLUDES

/*!
 @constant kSecKeyApplePayEnabled If set to kCFBooleanTrue during SecKeyCreateRandomKey, then the SEP-based key is ApplePay-enabled,
 which means that it can be used for ECIES to re-crypt.
 */
extern const CFStringRef kSecKeyApplePayEnabled
SPI_AVAILABLE(macos(10.14.4), ios(12.2), tvos(12.2), watchos(5.2));

/*!
 @constant kSecKeyOSBound If set to kCFBooleanTrue during SecKeyCreateRandomKey, then the SEP-based key is OS-bound (AKS option).
 This option is not supported and silently ignored on Intel-based platforms.
 */
extern const CFStringRef kSecKeyOSBound
SPI_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
 @constant kSecKeySealedHashesBound If set to kCFBooleanTrue during SecKeyCreateRandomKey, then the SEP-based key is sealed-hashes-bound (AKS option)
 */
extern const CFStringRef kSecKeySealedHashesBound
SPI_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
 @constant kSecKeyEncryptionParameterSymmetricKeySizeInBits CFNumberRef with size in bits for ephemeral
 symmetric key used for encryption/decryption.
 */
extern const CFStringRef kSecKeyEncryptionParameterSymmetricKeySizeInBits
SPI_AVAILABLE(macos(10.14.4), ios(12.2), tvos(12.2), watchos(5.2));

/*!
 @constant kSecKeyEncryptionParameterSymmetricAAD CFDataRef with additional authentiction data for AES-GCM encryption.
 */
extern const CFStringRef kSecKeyEncryptionParameterSymmetricAAD
SPI_AVAILABLE(macos(10.14.4), ios(12.2), tvos(12.2), watchos(5.2));

/*!
 @constant kSecKeyEncryptionParameterRecryptParameters Usable only for SecKeyCreateDecryptedDataWithParameters.
 Contains dictionary with parameters for re-encryption using the same algorithm and encryption parameters
 specified inside this dictionary.
 */
extern const CFStringRef kSecKeyEncryptionParameterRecryptParameters
SPI_AVAILABLE(macos(10.14.4), ios(12.2), tvos(12.2), watchos(5.2));

/*!
 @constant kSecKeyEncryptionParameterRecryptCertificate Usable only inside kSecKeyEncryptionParameterRecryptParameters.
 Specifies certificate whose public key is used to re-crypt previously decrypted data. This parameter should contain
 CFDataRef with X509 DER-encoded data of the certificate chain {leaf, intermediate, root}, leaf's public key is
 to be used for re-encryption.
 */
extern const CFStringRef kSecKeyEncryptionParameterRecryptCertificate
SPI_AVAILABLE(macos(10.14.4), ios(12.2), tvos(12.2), watchos(5.2));

/*!
 @function SecKeyCreateEncryptedDataWithParameters
 @abstract Encrypt a block of plaintext.
 @param key Public key with which to encrypt the data.
 @param algorithm One of SecKeyAlgorithm constants suitable to perform encryption with this key.
 @param plaintext The data to encrypt. The length and format of the data must conform to chosen algorithm,
 typically be less or equal to the value returned by SecKeyGetBlockSize().
 @param parameters Dictionary with additional parameters for encryption. Supported parameters are:
  - kSecKeyKeyExchangeParameterSharedInfo: additional SharedInfo value for ECIES encryptions
  - kSecKeyEncryptionParameterSymmetricKeySizeInBits: 128 or 256, size of ephemeral AES key used for symmetric encryption
  - kSecKeyEncryptionParameterSymmetricAAD: optional CFDataRef with additiona authentication data for AES-GCM encryption
 @param error On error, will be populated with an error object describing the failure.
 See "Security Error Codes" (SecBase.h).
 @result The ciphertext represented as a CFData, or NULL on failure.
 @discussion Encrypts plaintext data using specified key.  The exact type of the operation including the format
 of input and output data is specified by encryption algorithm.
 */
CFDataRef SecKeyCreateEncryptedDataWithParameters(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef plaintext,
                                                  CFDictionaryRef parameters, CFErrorRef *error)
SPI_AVAILABLE(macos(10.14.4), ios(12.2), tvos(12.2), watchos(5.2));

/*!
 @function SecKeyCreateDecryptedDataWithParameters
 @abstract Decrypt a block of ciphertext.
 @param key Private key with which to decrypt the data.
 @param algorithm One of SecKeyAlgorithm constants suitable to perform decryption with this key.
 @param ciphertext The data to decrypt. The length and format of the data must conform to chosen algorithm,
 typically be less or equal to the value returned by SecKeyGetBlockSize().
 @param parameters Dictionary with additional parameters for decryption.Supported parameters are:
  - kSecKeyKeyExchangeParameterSharedInfo: additional SharedInfo value for ECIES encryptions
  - kSecKeyEncryptionParameterSymmetricKeySizeInBits: 128 or 256, size of ephemeral AES key used for symmetric encryption
  - kSecKeyEncryptionParameterSymmetricAAD: optional CFDataRef with additiona authentication data for AES-GCM encryption
  - kSecKeyEncryptionParameterRecryptParameters: optional CFDictionaryRef with parameters for immediate re-encryption
    of decrypted data. If present, the dictionary *must* contain at least kSecKeyEncryptionParameterRecryptCertificate parameter.

 @param error On error, will be populated with an error object describing the failure.
 See "Security Error Codes" (SecBase.h).
 @result The plaintext represented as a CFData, or NULL on failure.
 @discussion Decrypts ciphertext data using specified key.  The exact type of the operation including the format
 of input and output data is specified by decryption algorithm.
 */
CFDataRef SecKeyCreateDecryptedDataWithParameters(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef ciphertext,
                                                  CFDictionaryRef parameters, CFErrorRef *error)
SPI_AVAILABLE(macos(10.14.4), ios(12.2), tvos(12.2), watchos(5.2));

/*!
 @function SecKeyCreateEncapsulatedKey
 @abstract Creates a new shared key and returns its encapsulated form.
 @param key Public key of receiver who will be able to decapsulate encapsulated key.
 @param algorithm One of SecKeyAlgorithm constants suitable to perform encapsulation of this key.
 @param sharedKey Output parameter, will be filled with resulting plaintext shared key.
 @param error On error, will be populated with an error object describing the failure.
 See "Security Error Codes" (SecBase.h).
 @result Encapsulated key.
 @discussion Generates new shared key and returns its encapsulated form.
 */

CFDataRef SecKeyCreateEncapsulatedKey(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef *sharedKey, CFErrorRef *error)
SPI_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
 @function SecKeyCreateDecapsulatedKey
 @abstract Decapsulates shared key encapsulated by equal algorithm and public key belonging to private key used in this call.
 @param key Private key belonging to public key which was used to encapsulate the shared key.
 @param algorithm One of SecKeyAlgorithm constants suitable to perform encapsulation of this key.
 @param encapsulatedKey Encapsulated form of shared key, typically created by SecKeyCreateEncapsulatedKey.
 @param error On error, will be populated with an error object describing the failure.
 See "Security Error Codes" (SecBase.h).
 @result Plaintext shared key.
 @discussion Decapsulates shared key encapsulated by equal algorithm and public key belonging to private key used in this call.
 */
CFDataRef SecKeyCreateDecapsulatedKey(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef encapsulatedKey, CFErrorRef *error)
SPI_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

/*!
 @enum SecKeySystemKeyType
 @abstract Defines types of builtin system keys.
*/
typedef CF_ENUM(uint32_t, SecKeySystemKeyType)
{
    kSecKeySystemKeyTypeSIK = 0,
    kSecKeySystemKeyTypeGID = 1,
    kSecKeySystemKeyTypeUIKCommitted = 2,
    kSecKeySystemKeyTypeUIKProposed = 3,
    kSecKeySystemKeyTypeSecureElement SPI_AVAILABLE(ios(15.0)) = 4,
    kSecKeySystemKeyTypeOIKCommitted = 5,
    kSecKeySystemKeyTypeOIKProposed = 6,
    kSecKeySystemKeyTypeDAKCommitted = 7,
    kSecKeySystemKeyTypeDAKProposed = 8,
    kSecKeySystemKeyTypeHavenCommitted = 9,
    kSecKeySystemKeyTypeHavenProposed = 10,
    kSecKeySystemKeyTypeSDAKCommitted = 11,
    kSecKeySystemKeyTypeSDAKProposed = 12,
    kSecKeySystemKeyTypeDCIK SPI_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0)) = 13,
} SPI_AVAILABLE(macos(12.0), ios(15.0), tvos(15.0), watchos(8.0));

/*!
 @function SecKeyCopySystemKey
 @abstract Returns a copy of a builtin system key.

 @param keyType Type of the requested builtin key.
 @param error An optional pointer to a CFErrorRef. This value is set if an error occurred.

 @result On success a SecKeyRef containing the requested key is returned, on failure it returns NULL.
*/
SecKeyRef SecKeyCopySystemKey(SecKeySystemKeyType keyType, CFErrorRef *error)
SPI_AVAILABLE(macos(12.0), ios(15.0), tvos(15.0), watchos(8.0));

typedef CF_ENUM(uint32_t, SecKeyAttestationKeyType)
{
    kSecKeyAttestationKeyTypeSIK = 0,
    kSecKeyAttestationKeyTypeGID = 1,
    kSecKeyAttestationKeyTypeUIKCommitted SPI_DEPRECATED_WITH_REPLACEMENT("kSecKeySystemKeyTypeUIKCommitted", macos(10.14, 12.0), ios(12.0, 15.0), tvos(12.0, 15.0), watchos(5.0, 8.0)) = 2,
    kSecKeyAttestationKeyTypeUIKProposed SPI_DEPRECATED_WITH_REPLACEMENT("kSecKeySystemKeyTypeUIKProposed", macos(10.14, 12.0), ios(12.0, 15.0), tvos(12.0, 15.0), watchos(5.0, 8.0)) = 3,
    kSecKeyAttestationKeyTypeSecureElement SPI_DEPRECATED_WITH_REPLACEMENT("kSecKeySystemKeyTypeSecureElement", ios(13.0, 15.0)) = 4,
    kSecKeyAttestationKeyTypeOIKCommitted SPI_DEPRECATED_WITH_REPLACEMENT("kSecKeySystemKeyTypeOIKCommitted", macos(10.14, 12.0), ios(12.0, 15.0), tvos(12.0, 15.0), watchos(5.0, 8.0)) = 5,
    kSecKeyAttestationKeyTypeOIKProposed SPI_DEPRECATED_WITH_REPLACEMENT("kSecKeySystemKeyTypeOIKProposed", macos(10.14, 12.0), ios(12.0, 15.0), tvos(12.0, 15.0), watchos(5.0, 8.0)) = 6,
    kSecKeyAttestationKeyTypeDAKCommitted SPI_DEPRECATED_WITH_REPLACEMENT("kSecKeySystemKeyTypeDAKCommitted", macos(10.14, 12.0), ios(12.0, 15.0), tvos(12.0, 15.0), watchos(5.0, 8.0)) = 7,
    kSecKeyAttestationKeyTypeDAKProposed SPI_DEPRECATED_WITH_REPLACEMENT("kSecKeySystemKeyTypeDAKProposed", macos(10.14, 12.0), ios(12.0, 15.0), tvos(12.0, 15.0), watchos(5.0, 8.0)) = 8,
} SPI_DEPRECATED_WITH_REPLACEMENT("SecKeySystemKeyType", macos(10.12, 12.0), ios(10.0, 15.0), tvos(10.0, 15.0), watchos(3.0, 8.0));

SecKeyRef SecKeyCopyAttestationKey(SecKeyAttestationKeyType keyType, CFErrorRef *error)
SPI_DEPRECATED_WITH_REPLACEMENT("SecKeyCopySystemKey", macos(10.12, 12.0), ios(10.0, 15.0), tvos(10.0, 15.0), watchos(3.0, 8.0));

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
SPI_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0));

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
SPI_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0));

extern const CFStringRef kSecKeyParameterSETokenAttestationNonce
SPI_AVAILABLE(macos(10.13), ios(11.0), tvos(11.0), watchos(4.0));

/*!
 Available lifetime operations for SEP system keys.
 */
typedef CF_ENUM(int, SecKeyControlLifetimeType) {
    kSecKeyControlLifetimeTypeBump = 0,
    kSecKeyControlLifetimeTypeCommit = 1,
} SPI_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0));

/*!
 @function SecKeyControlLifetime
 @abstract Performs key lifetime control operations for SEP system keys.

 @param key Key to be controlled, currently must be either proposed or committed system UIK.
 @param type Type of the control operation to be performed.
 @param error Error which gathers more information when something went wrong.
 */
Boolean SecKeyControlLifetime(SecKeyRef key, SecKeyControlLifetimeType type, CFErrorRef *error)
SPI_AVAILABLE(macos(10.14), ios(12.0), tvos(12.0), watchos(5.0));

/*!
 @function SecKeyCreateDuplicate
 @abstract Creates duplicate fo the key.

 @param key Source key to be duplicated

 @discussion Only memory representation of the key is duplicated, so if the key is backed by keychain, only one instance
 stays in the keychain.  Duplicating key is useful for setting 'temporary' key parameters using SecKeySetParameter.
 If the key is immutable (i.e. does not support SecKeySetParameter), calling this method is identical to calling CFRetain().
 */
SecKeyRef SecKeyCreateDuplicate(SecKeyRef key)
SPI_AVAILABLE(macos(10.12), ios(10.0), tvos(10.0), watchos(3.0));

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

/*!
 @constant kSecKeyAlgorithmEdDSASignatureMessageCurve25519SHA512
 EdDSA algorithm, signature is stored in 64 bytes, SHA-512 digest is generated by called function automatically from input data of any size.
 */
extern const SecKeyAlgorithm kSecKeyAlgorithmEdDSASignatureMessageCurve25519SHA512
SPI_AVAILABLE(macos(14.0), ios(17.0), tvos(17.0), watchos(10.0));

/*!
 @constant kSecKeyAlgorithmEdDSASignatureMessageCurve448SHAKE256
 EdDSA algorithm, signature is stored in 114 bytes, SHAKE256 digest is generated by called function automatically from input data of any size.
 */
extern const SecKeyAlgorithm kSecKeyAlgorithmEdDSASignatureMessageCurve448SHAKE256
SPI_AVAILABLE(macos(14.0), ios(17.0), tvos(17.0), watchos(10.0));

/*!
 @constant kSecKeyOperationTypeEncapsulate
 KEM encapsulation operation.
 */
SPI_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0))
static const SecKeyOperationType kSecKeyOperationTypeEncapsulate = (SecKeyOperationType)5;

/*!
 @constant kSecKeyOperationTypeDecapsulate
 KEM decapsulation operation.
 */
SPI_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0))
static const SecKeyOperationType kSecKeyOperationTypeDecapsulate = (SecKeyOperationType)6;

/*!
 @constant kSecKeyAlgorithmKEMKyber
 KEM (Key Encapsulation Mechanism) Kyber algorithm.
 */
extern const SecKeyAlgorithm kSecKeyAlgorithmKEMKyber
SPI_AVAILABLE(macos(15.0), ios(18.0), tvos(18.0), watchos(11.0));

__END_DECLS

#endif /* !_SECURITY_SECKEYPRIV_H_ */
