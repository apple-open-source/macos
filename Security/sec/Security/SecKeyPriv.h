/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
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
typedef OSStatus (*SecKeyCopyPublicBytesMethod)(SecKeyRef key, CFDataRef *serailziation);
typedef CFStringRef (*SecKeyDescribeMethod)(SecKeyRef key);

#define kSecKeyDescriptorVersion  (2)

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
} SecKeyDescriptor;

struct __SecKey {
    CFRuntimeBase		_base;

    const SecKeyDescriptor *key_class;

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

/* Create public key from private key */
SecKeyRef SecKeyCreatePublicFromPrivate(SecKeyRef privateKey);
SecKeyRef SecKeyCopyMatchingPrivateKey(SecKeyRef publicKey, CFErrorRef *error);
CFDataRef SecKeyCreatePersistentRefToMatchingPrivateKey(SecKeyRef publicKey, CFErrorRef *error);

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

CFIndex SecKeyGetAlgorithmID(SecKeyRef key);

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



__END_DECLS

#endif /* !_SECURITY_SECKEYPRIV_H_ */
