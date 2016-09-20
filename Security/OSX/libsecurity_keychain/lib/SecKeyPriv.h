/*
 * Copyright (c) 2002-2009,2011-2015 Apple Inc. All Rights Reserved.
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
 *
 * SecKeyPriv.h - SPIs to SecKeyRef objects.
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
#include <Security/x509defs.h>
#include <Security/SecAsn1Types.h>
#include <AvailabilityMacros.h>
#include <CoreFoundation/CFRuntime.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct SecRSAPublicKeyParams {
    uint8_t             *modulus;			/* modulus */
    CFIndex             modulusLength;
    uint8_t             *exponent;			/* public exponent */
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
#endif
} SecKeyDescriptor;

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
	DEPRECATED_IN_MAC_OS_X_VERSION_10_8_AND_LATER;

enum {
    kSecNullAlgorithmID = 0,
    kSecRSAAlgorithmID = 1,
    kSecDSAAlgorithmID = 2,   /* unsupported, just here for reference. */
    kSecECDSAAlgorithmID = 3,
};

/*!
	@function SecKeyGetAlgorithmId
	@abstract Returns an enumerated constant value which identifies the algorithm for the given key.
	@param key A key reference.
	@result An algorithm identifier.
*/
CFIndex SecKeyGetAlgorithmId(SecKeyRef key)
	__OSX_AVAILABLE_STARTING(__MAC_10_8, __IPHONE_9_0);

/*!
	@function SecKeyGetStrengthInBits
	@abstract Returns key strength in bits for the given key.
    @param key A key reference.
    @param algid A pointer to a CSSM_X509_ALGORITHM_IDENTIFIER structure, as returned from a call to SecKeyGetAlgorithmID.
    @param strength On return, the key strength in bits.
	@result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecKeyGetStrengthInBits(SecKeyRef key, const CSSM_X509_ALGORITHM_IDENTIFIER *algid, unsigned int *strength);

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
    @deprecated in 10.5 and later. Use the SecKeychainItemImport function instead; see <Security/SecImportExport.h>
*/
OSStatus SecKeyImportPair(
        SecKeychainRef keychainRef,
		const CSSM_KEY *publicCssmKey,
		const CSSM_KEY *privateCssmKey,
        SecAccessRef initialAccess,
        SecKeyRef* publicKey,
        SecKeyRef* privateKey)
		DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER;

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
	on Mac OS X (and there should not be any outside the Security stack, per the warning above) must
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
OSStatus SecKeyCreateWithCSSMKey(const CSSM_KEY *key, SecKeyRef* keyRef);


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
		size_t              *sigLen);


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
		 size_t              sigLen);


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
	   size_t              *cipherTextLen);


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
	   SecPadding          padding,			/* kSecPaddingNone, kSecPaddingPKCS1, kSecPaddingOAEP */
	   const uint8_t       *cipherText,
	   size_t              cipherTextLen,	/* length of cipherText */
	   uint8_t             *plainText,
	   size_t              *plainTextLen);	/* IN/OUT */

OSStatus SecKeyVerifyDigest(
	   SecKeyRef           key,            /* Private key */
	   const SecAsn1AlgId  *algId,         /* algorithm oid/params */
	   const uint8_t       *digestData,	   /* signature over this digest */
	   size_t              digestDataLen,  /* length of dataToDigest */
	   const uint8_t       *sig,           /* signature to verify */
	   size_t              sigLen);        /* length of sig */

OSStatus SecKeySignDigest(
	   SecKeyRef           key,            /* Private key */
	   const SecAsn1AlgId  *algId,         /* algorithm oid/params */
	   const uint8_t       *digestData,    /* signature over this digest */
	   size_t              digestDataLen,  /* length of digestData */
	   uint8_t             *sig,           /* signature, RETURNED */
	   size_t              *sigLen);       /* IN/OUT */


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

OSStatus SecKeyRawVerifyOSX(
		 SecKeyRef           key,
		 SecPadding          padding,
		 const uint8_t       *signedData,
		 size_t              signedDataLen,
		 const uint8_t       *sig,
		 size_t              sigLen);

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
 to SecKey implementation backend (currently only CTK-based token backend is supported).  Parameter names and types are
 a contract between SecKey user (application) and backend and are not interpreted by SecKey layer in any way.
 */
Boolean SecKeySetParameter(SecKeyRef key, CFStringRef name, CFPropertyListRef value, CFErrorRef *error)
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

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECKEYPRIV_H_ */

