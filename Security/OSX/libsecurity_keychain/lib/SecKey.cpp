/*
 * Copyright (c) 2002-2015 Apple Inc. All Rights Reserved.
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

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecKeyInternal.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <libDER/asn1Types.h>
#include <libDER/DER_Encode.h>
#include <libDER/DER_Decode.h>
#include <libDER/DER_Keys.h>
#include <Security/SecAsn1Types.h>
#include <Security/SecAsn1Coder.h>
#include <Security/SecCFAllocator.h>
#include <security_keychain/KeyItem.h>
#include <security_utilities/casts.h>
#include <CommonCrypto/CommonKeyDerivation.h>

#include <CoreFoundation/CFPriv.h>
// 'verify' macro is somehow dragged in from CFPriv.h and breaks compilation of signclient.h, so undef it, we don't need it.
#undef verify

#include "SecBridge.h"

#include <security_keychain/Access.h>
#include <security_keychain/Keychains.h>
#include <security_keychain/KeyItem.h>
#include <string.h>
#include <syslog.h>

#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_client/wrapkey.h>
#include <security_cdsa_client/genkey.h>
#include <security_cdsa_client/signclient.h>
#include <security_cdsa_client/cryptoclient.h>

#include "SecImportExportCrypto.h"

static OSStatus
SecCDSAKeyInit(SecKeyRef key, const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    CDSASecKey *cdsaKey = (CDSASecKey *)key;
    cdsaKey->key = const_cast<KeyItem *>(reinterpret_cast<const KeyItem *>(keyData));
    CDSASecKey::keyItem(key)->initializeWithSecKeyRef(key);
    cdsaKey->credentialType = kSecCredentialTypeDefault;
    cdsaKey->cdsaKeyMutex = new Mutex();
    return errSecSuccess;
}

static void
SecCDSAKeyDestroy(SecKeyRef keyRef) {
    // Note: If this key is holding the last strong reference to its keychain, the keychain will be released during this operation.
    // If we hold the keychain's mutex (the key's 'mutexForObject') during this destruction, pthread gets upset.
    // Hold a reference to the keychain (if it exists) until after we release the keychain's mutex.

    CDSASecKey *cdsaKey = static_cast<CDSASecKey *>(keyRef);
    StMaybeLock<Mutex> cdsaMutex(cdsaKey->cdsaKeyMutex);

    KeyItem *keyItem = static_cast<KeyItem *>(keyRef->key);

    if (keyItem == NULL) {
        // KeyImpl::attachSecKeyRef disconnected us from KeyItem instance, there is nothing to do for us.
        cdsaMutex.unlock();
        delete cdsaKey->cdsaKeyMutex;
        return;
    }

    Keychain kc = keyItem->keychain();

    // We have a +1 reference to the KeyItem now; no need to protect our storage any more
    cdsaMutex.unlock();

    {
        StMaybeLock<Mutex> _(keyItem->getMutexForObject());
        keyItem = static_cast<KeyItem *>(keyRef->key);
        if (keyItem == NULL) {
            // Second version of the check above, the definitive one because this one is performed with locked object's mutex, therefore we can be sure that KeyImpl is still connected to this keyRef instance.
            return;
        }

        keyItem->aboutToDestruct();
        delete keyItem;
    }

    delete cdsaKey->cdsaKeyMutex;

    (void) kc; // Tell the compiler we're actually using this variable. At destruction time, it'll release the keychain.
}

static CFStringRef
SecCDSAKeyCopyDescription(SecKeyRef key) {
    const CssmKey::Header header = CDSASecKey::keyItem(key)->unverifiedKeyHeader();
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR( "<SecCDSAKeyRef %p: algorithm id: %lu, class=%x, algorithm=%x, usage=%x attrs=%x>"), key, SecKeyGetAlgorithmId(key), (unsigned)header.keyClass(), (unsigned)header.algorithm(), (unsigned)header.usage(), (unsigned)header.attributes());
}

static size_t
SecCDSAKeyGetBlockSize(SecKeyRef key) {

    CFErrorRef *error = NULL;
    BEGIN_SECKEYAPI(size_t,0)

    const CssmKey::Header keyHeader = CDSASecKey::keyItem(key)->unverifiedKeyHeader();
    switch(keyHeader.algorithm())
    {
        case CSSM_ALGID_RSA:
        case CSSM_ALGID_DSA:
            result = keyHeader.LogicalKeySizeInBits / 8;
            break;
        case CSSM_ALGID_ECDSA:
        {
            /* Block size is up to 9 bytes of DER encoding for sequence of 2 integers,
             * plus both coordinates for the point used */
#define ECDSA_KEY_SIZE_IN_BYTES(bits) (((bits) + 7) / 8)
#define ECDSA_MAX_COORD_SIZE_IN_BYTES(n) (ECDSA_KEY_SIZE_IN_BYTES(n) + 1)
            size_t coordSize = ECDSA_MAX_COORD_SIZE_IN_BYTES(keyHeader.LogicalKeySizeInBits);
            assert(coordSize < 256); /* size must fit in a byte for DER */
            size_t coordDERLen = (coordSize > 127) ? 2 : 1;
            size_t coordLen = 1 + coordDERLen + coordSize;

            size_t pointSize = 2 * coordLen;
            assert(pointSize < 256); /* size must fit in a byte for DER */
            size_t pointDERLen = (pointSize > 127) ? 2 : 1;
            size_t pointLen = 1 + pointDERLen + pointSize;

            result = pointLen;
        }
            break;
        case CSSM_ALGID_AES:
            result = 16; /* all AES keys use 128-bit blocks */
            break;
        case CSSM_ALGID_DES:
        case CSSM_ALGID_3DES_3KEY:
            result = 8; /* all DES keys use 64-bit blocks */
            break;
        default:
            assert(0); /* some other key algorithm */
            result = 16; /* FIXME: revisit this */
            break;
    }

    END_SECKEYAPI
}

static CFIndex
SecCDSAKeyGetAlgorithmId(SecKeyRef key) {

    CFErrorRef *error = NULL;
    BEGIN_SECKEYAPI(CFIndex, 0)

    result = kSecNullAlgorithmID;
    switch (CDSASecKey::keyItem(key)->unverifiedKeyHeader().AlgorithmId) {
        case CSSM_ALGID_RSA:
            result = kSecRSAAlgorithmID;
            break;
        case CSSM_ALGID_DSA:
            result = kSecDSAAlgorithmID;
            break;
        case CSSM_ALGID_ECDSA:
            result = kSecECDSAAlgorithmID;
            break;
        default:
            result = kSecNullAlgorithmID;
            break;
    }

    END_SECKEYAPI
}

static CFDataRef SecCDSAKeyCopyPublicKeyDataWithSubjectInfo(CSSM_ALGORITHMS algorithm, uint32 keySizeInBits, CFDataRef pubKeyInfo) {
    // First check, whether X509 pubkeyinfo is already present.  If not, add it according to the key type.
    DERItem keyItem = { (DERByte *)CFDataGetBytePtr(pubKeyInfo), int_cast<CFIndex, DERSize>(CFDataGetLength(pubKeyInfo)) };
    DERSubjPubKeyInfo subjPubKey;
    if (DERParseSequence(&keyItem, DERNumSubjPubKeyInfoItemSpecs,
                         DERSubjPubKeyInfoItemSpecs,
                         &subjPubKey, sizeof(subjPubKey)) == DR_Success) {
        return CFDataRef(CFRetain(pubKeyInfo));
    }

    // We have always size rounded to full bytes so bitstring encodes leading 00.
    CFRef<CFMutableDataRef> bitStringPubKey = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFDataSetLength(bitStringPubKey, 1);
    CFDataAppendBytes(bitStringPubKey, CFDataGetBytePtr(pubKeyInfo), CFDataGetLength(pubKeyInfo));
    subjPubKey.pubKey.data = static_cast<DERByte *>(const_cast<UInt8 *>(CFDataGetBytePtr(bitStringPubKey)));
    subjPubKey.pubKey.length = CFDataGetLength(bitStringPubKey);

    // Encode algId according to algorithm used.
    static const DERByte oidRSA[] = {
        0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00,
    };
    static const DERByte oidECsecp256[] = {
        0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
    };
    static const DERByte oidECsecp384[] = {
        0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22,
    };
    static const DERByte oidECsecp521[] = {
        0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x23,
    };
    subjPubKey.algId.length = 0;
    if (algorithm == CSSM_ALGID_RSA) {
        subjPubKey.algId.data = const_cast<DERByte *>(oidRSA);
        subjPubKey.algId.length = sizeof(oidRSA);
    } else if (algorithm == CSSM_ALGID_ECDSA) {
        if (keySizeInBits == 256) {
            subjPubKey.algId.data = const_cast<DERByte *>(oidECsecp256);
            subjPubKey.algId.length = sizeof(oidECsecp256);
        } else if (keySizeInBits == 384) {
            subjPubKey.algId.data = const_cast<DERByte *>(oidECsecp384);
            subjPubKey.algId.length = sizeof(oidECsecp384);
        } if (keySizeInBits == 521) {
            subjPubKey.algId.data = const_cast<DERByte *>(oidECsecp521);
            subjPubKey.algId.length = sizeof(oidECsecp521);
        }
    }
    DERSize size = DERLengthOfEncodedSequence(ASN1_CONSTR_SEQUENCE, &subjPubKey,
                                              DERNumSubjPubKeyInfoItemSpecs, DERSubjPubKeyInfoItemSpecs);
    CFRef<CFMutableDataRef> keyData = CFDataCreateMutable(kCFAllocatorDefault, size);
    CFDataSetLength(keyData, size);
    if (DEREncodeSequence(ASN1_CONSTR_SEQUENCE, &subjPubKey,
                          DERNumSubjPubKeyInfoItemSpecs, DERSubjPubKeyInfoItemSpecs,
                          static_cast<DERByte *>(CFDataGetMutableBytePtr(keyData)), &size) == DR_Success) {
        CFDataSetLength(keyData, size);
    } else {
        keyData.release();
    }

    return keyData.yield();
}

typedef struct {
    DERItem    privateKey;
    DERItem    publicKey;
} DERECPrivateKey;

static const DERItemSpec DERECPrivateKeyItemSpecs[] =
{
    { 0,
        ASN1_INTEGER,
        DER_DEC_SKIP },
    { DER_OFFSET(DERECPrivateKey, privateKey),
        ASN1_OCTET_STRING,
        DER_DEC_NO_OPTS | DER_ENC_NO_OPTS },
    { 0,
        ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0,
        DER_DEC_SKIP | DER_ENC_NO_OPTS },
    { DER_OFFSET(DERECPrivateKey, publicKey),
        ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 1,
        DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
};
static const DERSize DERNumECPrivateKeyItemSpecs =
sizeof(DERECPrivateKeyItemSpecs) / sizeof(DERItemSpec);

typedef struct {
    DERItem bitString;
} DERECPrivateKeyPublicKey;

static const DERItemSpec DERECPrivateKeyPublicKeyItemSpecs[] =
{
    { DER_OFFSET(DERECPrivateKeyPublicKey, bitString),
        ASN1_BIT_STRING,
        DER_DEC_NO_OPTS | DER_ENC_NO_OPTS },
};
static const DERSize DERNumECPrivateKeyPublicKeyItemSpecs =
sizeof(DERECPrivateKeyPublicKeyItemSpecs) / sizeof(DERItemSpec);

static void
SecCDSAKeyGetPublicKeyDataFromPrivateKey(KeyItem *keyItem, CssmOwnedData &keyData) {
    Key &key = keyItem->key();
    try {
        PassThrough passThrough(key->csp());
        passThrough.key(key);
        switch (key->algorithm()) {
            case CSSM_ALGID_RSA:
                passThrough.add(CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT, uint32(CSSM_KEYBLOB_RAW_FORMAT_PKCS1));
                break;
            case CSSM_ALGID_ECDSA:
                passThrough.add(CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT, uint32(CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING));
                break;
            default:
                CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
        }
        CssmData *pubKeyData;
        passThrough(CSSM_APPLECSP_PUBKEY, (const void *)NULL, &pubKeyData);
        keyData.set(*pubKeyData);
        passThrough.allocator().free(pubKeyData);
    } catch (const CssmError &error) {
        // We need to handle the case when running against old securityd which does not implement new getPublicKey() MIG IPC.
        if (error.error != CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED) {
            // Some other failure, propagate further.
            throw;
        }

        WrapKey wrapKey(key->csp(), CSSM_ALGID_NONE);
        wrapKey.cred(keyItem->getCredentials(CSSM_ACL_AUTHORIZATION_DECRYPT, kSecCredentialTypeDefault));
        switch (key->algorithm()) {
            case CSSM_ALGID_RSA: {
                wrapKey.add(CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT, uint32(CSSM_KEYBLOB_RAW_FORMAT_PKCS1));
                Key wrappedKey = wrapKey(key);
                DERItem keyItem = { (DERByte *)wrappedKey->data(), int_cast<size_t, DERSize>(wrappedKey->length()) };
                DERRSAKeyPair keyPair;
                if (DERParseSequence(&keyItem, DERNumRSAKeyPairItemSpecs, DERRSAKeyPairItemSpecs,
                                     &keyPair, sizeof(keyPair)) != DR_Success) {
                    CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
                }
                DERRSAPubKeyPKCS1 pubKey = { keyPair.n, keyPair.e };
                DERSize size = DERLengthOfEncodedSequence(ASN1_SEQUENCE, &pubKey,
                                                          DERNumRSAPubKeyPKCS1ItemSpecs, DERRSAPubKeyPKCS1ItemSpecs);
                keyData.length(size);
                if (DEREncodeSequence(ASN1_SEQUENCE, &pubKey,
                                      DERNumRSAPubKeyPKCS1ItemSpecs, DERRSAPubKeyPKCS1ItemSpecs,
                                      static_cast<DERByte *>(keyData.data()), &size) != DR_Success) {
                    CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
                }
                keyData.length(size);
                *static_cast<DERByte *>(keyData.data()) = ONE_BYTE_ASN1_CONSTR_SEQUENCE;
                break;
            }
            case CSSM_ALGID_ECDSA: {
                // Convert DER format into x9.63 format, which is expected for exported key.
                Key wrappedKey = wrapKey(key);
                DERItem keyItem = { (DERByte *)wrappedKey->data(), int_cast<size_t, DERSize>(wrappedKey->length()) };
                DERECPrivateKey privateKey;
                DERECPrivateKeyPublicKey privateKeyPublicKey;
                DERByte numUnused;
                DERItem pubKeyItem;
                if (DERParseSequence(&keyItem, DERNumECPrivateKeyItemSpecs, DERECPrivateKeyItemSpecs,
                                     &privateKey, sizeof(privateKey)) != DR_Success ||
                    DERParseSequenceContent(&privateKey.publicKey, DERNumECPrivateKeyPublicKeyItemSpecs,
                                            DERECPrivateKeyPublicKeyItemSpecs,
                                            &privateKeyPublicKey, sizeof(privateKeyPublicKey)) != DR_Success ||
                    DERParseBitString(&privateKeyPublicKey.bitString, &pubKeyItem, &numUnused) != DR_Success) {
                    CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
                }
                keyData.copy(pubKeyItem.data, pubKeyItem.length);
                break;
            }
            default:
                CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
        }
    }
}

static CFDataRef
SecCDSAKeyCopyExternalRepresentation(SecKeyRef keyRef, CFErrorRef *error) {
    BEGIN_SECKEYAPI(CFDataRef, NULL)

    KeychainCore::KeyItem *keyItem = CDSASecKey::keyItem(keyRef);
    Key &key = keyItem->key();
    WrapKey wrapKey(key->csp(), CSSM_ALGID_NONE);
    if (key->keyClass() == CSSM_KEYCLASS_PRIVATE_KEY) {
        // Creds are needed for wrapping private keys.
        wrapKey.cred(keyItem->getCredentials(CSSM_ACL_AUTHORIZATION_DECRYPT, kSecCredentialTypeDefault));
    }
    switch (key->algorithm()) {
        case CSSM_ALGID_RSA: {
            wrapKey.add((key->keyClass() == CSSM_KEYCLASS_PUBLIC_KEY) ? CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT : CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT, uint32(CSSM_KEYBLOB_RAW_FORMAT_PKCS1));
            Key wrappedKey = wrapKey(key);
            result = CFDataCreate(SecCFAllocatorZeroize(), static_cast<const UInt8 *>(wrappedKey->data()), int_cast<size_t, CFIndex>(wrappedKey->length()));
            break;
        }
        case CSSM_ALGID_ECDSA: {
            switch (key->keyClass()) {
                case CSSM_KEYCLASS_PUBLIC_KEY: {
                    wrapKey.add(CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT, uint32(CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING));
                    Key wrappedKey = wrapKey(key);
                    result = CFDataCreate(SecCFAllocatorZeroize(), static_cast<const UInt8 *>(wrappedKey->data()), int_cast<size_t, CFIndex>(wrappedKey->length()));
                    break;
                }
                case CSSM_KEYCLASS_PRIVATE_KEY: {
                    // First, get public key.
                    CssmAutoData pubKeyData(key.allocator());
                    SecCDSAKeyGetPublicKeyDataFromPrivateKey(keyItem, pubKeyData);
                    CFRef<CFMutableDataRef> keyData = CFDataCreateMutable(SecCFAllocatorZeroize(), 0);
                    CFDataAppendBytes(keyData, static_cast<const UInt8 *>(pubKeyData.data()), int_cast<CFIndex, size_t>(pubKeyData.length()));

                    // Use wrap to get private key part and append it after public key.
                    wrapKey.add(CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT, uint32(CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING));
                    Key wrappedKey = wrapKey(key);
                    CFDataAppendBytes(keyData, static_cast<const UInt8 *>(wrappedKey->data()), int_cast<CFIndex, size_t>(wrappedKey->length()));
                    result = keyData.yield();
                    break;
                }
                default:
                    CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
            }
            break;
        }
        default:
            CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
    }

    END_SECKEYAPI
}

static CFDataRef SecCDSAKeyCreatePublicBytes(SecKeyRef keyRef, CFErrorRef *error) {
    BEGIN_SECKEYAPI(CFDataRef, NULL)

    KeychainCore::KeyItem *keyItem = CDSASecKey::keyItem(keyRef);
    CssmClient::Key &key = keyItem->key();
    switch (key->keyClass()) {
        case CSSM_KEYCLASS_PRIVATE_KEY: {
            CssmAutoData pubKeyData(key.allocator());
            SecCDSAKeyGetPublicKeyDataFromPrivateKey(keyItem, pubKeyData);
            result = CFDataCreate(SecCFAllocatorZeroize(), static_cast<const UInt8 *>(pubKeyData.data()), int_cast<CFIndex, size_t>(pubKeyData.length()));
            break;
        }
        case CSSM_KEYCLASS_PUBLIC_KEY: {
            // Just forward to getting external representation of the public key.
            result = SecCDSAKeyCopyExternalRepresentation(keyRef, error);
            break;
        }
        default:
            CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
    }

    END_SECKEYAPI
}

static OSStatus SecCDSAKeyCopyPublicBytes(SecKeyRef keyRef, CFDataRef *serialization) {
    CFRef<CFErrorRef> error;
    *serialization = SecCDSAKeyCreatePublicBytes(keyRef, error.take());
    return *serialization != NULL ? errSecSuccess : (OSStatus)CFErrorGetCode(error);
}

static CFDataRef SecCDSAKeyCopyLabel(SecKeyRef key) {
    CFDataRef label = NULL;
    if (CDSASecKey::keyItem(key)->isPersistent()) {
        UInt32 tags[] = { kSecKeyLabel }, formats[] = { CSSM_DB_ATTRIBUTE_FORMAT_BLOB };
        SecKeychainAttributeInfo info = { 1, tags, formats };
        SecKeychainAttributeList *list = NULL;
        CDSASecKey::keyItem(key)->getAttributesAndData(&info, NULL, &list, NULL, NULL);
        if (list->count == 1) {
            SecKeychainAttribute *attr = list->attr;
            label = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)attr->data, (CFIndex)attr->length);
        }
        CDSASecKey::keyItem(key)->freeAttributesAndData(list, NULL);
    }
    return label;
}

static CFDictionaryRef
SecCDSAKeyCopyAttributeDictionary(SecKeyRef key) {

    CFErrorRef *error = NULL;
    BEGIN_SECKEYAPI(CFDictionaryRef, NULL)

    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);

    CFDictionarySetValue(dict, kSecClass, kSecClassKey);

    const CssmKey::Header header = CDSASecKey::keyItem(key)->unverifiedKeyHeader();
    CFIndex sizeValue = header.LogicalKeySizeInBits;
    CFRef<CFNumberRef> sizeInBits = CFNumberCreate(NULL, kCFNumberCFIndexType, &sizeValue);
    CFDictionarySetValue(dict, kSecAttrKeySizeInBits, sizeInBits);
    CFDictionarySetValue(dict, kSecAttrEffectiveKeySize, sizeInBits);

    CFRef<CFDataRef> label = SecCDSAKeyCopyLabel(key);
    if (!label) {
        // For floating keys, calculate label as SHA1 of pubkey bytes.
        CFRef<CFDataRef> pubKeyBlob;
        if (SecCDSAKeyCopyPublicBytes(key, pubKeyBlob.take()) == errSecSuccess) {
            uint8_t pubKeyHash[CC_SHA1_DIGEST_LENGTH];
            CC_SHA1(CFDataGetBytePtr(pubKeyBlob), CC_LONG(CFDataGetLength(pubKeyBlob)), pubKeyHash);
            label.take(CFDataCreate(kCFAllocatorDefault, pubKeyHash, sizeof(pubKeyHash)));
        }
    }

    if (label) {
        CFDictionarySetValue(dict, kSecAttrApplicationLabel, label);
    }

    CSSM_KEYATTR_FLAGS attrs = header.attributes();
    CFDictionarySetValue(dict, kSecAttrIsPermanent, (attrs & CSSM_KEYATTR_PERMANENT) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrIsPrivate, (attrs & CSSM_KEYATTR_PRIVATE) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrIsModifiable, (attrs & CSSM_KEYATTR_MODIFIABLE) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrIsSensitive, (attrs & CSSM_KEYATTR_SENSITIVE) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrIsExtractable, (attrs & CSSM_KEYATTR_EXTRACTABLE) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrWasAlwaysSensitive, (attrs & CSSM_KEYATTR_ALWAYS_SENSITIVE) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrWasNeverExtractable, (attrs & CSSM_KEYATTR_NEVER_EXTRACTABLE) ? kCFBooleanTrue : kCFBooleanFalse);

    CFDictionarySetValue(dict, kSecAttrCanEncrypt, (header.useFor(CSSM_KEYUSE_ENCRYPT)) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrCanDecrypt, (header.useFor(CSSM_KEYUSE_DECRYPT)) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrCanSign, (header.useFor(CSSM_KEYUSE_SIGN)) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrCanVerify, (header.useFor(CSSM_KEYUSE_VERIFY)) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrCanSignRecover, (header.useFor(CSSM_KEYUSE_SIGN_RECOVER)) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrCanVerifyRecover, (header.useFor(CSSM_KEYUSE_VERIFY_RECOVER)) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrCanWrap, (header.useFor(CSSM_KEYUSE_WRAP)) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrCanUnwrap, (header.useFor(CSSM_KEYUSE_UNWRAP)) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(dict, kSecAttrCanDerive, (header.useFor(CSSM_KEYUSE_DERIVE)) ? kCFBooleanTrue : kCFBooleanFalse);

    switch (header.keyClass()) {
        case CSSM_KEYCLASS_PUBLIC_KEY:
            CFDictionarySetValue(dict, kSecAttrKeyClass, kSecAttrKeyClassPublic);
            break;
        case CSSM_KEYCLASS_PRIVATE_KEY:
            CFDictionarySetValue(dict, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
            break;
    }

    switch (header.algorithm()) {
        case CSSM_ALGID_RSA:
            CFDictionarySetValue(dict, kSecAttrKeyType, kSecAttrKeyTypeRSA);
            break;
        case CSSM_ALGID_ECDSA:
            CFDictionarySetValue(dict, kSecAttrKeyType, kSecAttrKeyTypeECSECPrimeRandom);
            break;
    }

    CFRef<CFDataRef> keyData = SecCDSAKeyCopyExternalRepresentation(key, NULL);
    if (keyData) {
        CFDictionarySetValue(dict, kSecValueData, keyData);
        if (header.algorithm() == CSSM_ALGID_RSA && header.keyClass() == CSSM_KEYCLASS_PUBLIC_KEY) {
            DERItem keyItem = { const_cast<DERByte *>(static_cast<const DERByte *>(CFDataGetBytePtr(keyData))), int_cast<CFIndex, DERSize>(CFDataGetLength(keyData)) };
            DERRSAPubKeyPKCS1 decodedKey;
            if (DERParseSequence(&keyItem, DERNumRSAPubKeyPKCS1ItemSpecs,
                                 DERRSAPubKeyPKCS1ItemSpecs,
                                 &decodedKey, sizeof(decodedKey)) == DR_Success) {
                CFRef<CFDataRef> modulus = CFDataCreate(kCFAllocatorDefault, decodedKey.modulus.data,
                                                        decodedKey.modulus.length);
                CFDictionarySetValue(dict, CFSTR("_rsam"), modulus);
                CFRef<CFDataRef> exponent = CFDataCreate(kCFAllocatorDefault, decodedKey.pubExponent.data,
                                                         decodedKey.pubExponent.length);
                CFDictionarySetValue(dict, CFSTR("_rsae"), exponent);
            }
        }
    }

    result = dict;

    END_SECKEYAPI
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-const-variable"
static CSSM_DB_NAME_ATTR(kInfoKeyLabel, kSecKeyLabel, (char*) "Label", 0, NULL, BLOB);
#pragma clang diagnostic pop

static SecKeyRef SecCDSAKeyCopyPublicKey(SecKeyRef privateKey) {
    CFErrorRef *error = NULL;
    BEGIN_SECKEYAPI(SecKeyRef, NULL)

    result = NULL;
    KeyItem *key = CDSASecKey::keyItem(privateKey);
    CFRef<CFDataRef> label = SecCDSAKeyCopyLabel(privateKey);
    if (label) {
        // Lookup public key in the database.
        DbUniqueRecord uniqueId;
        SSDb ssDb(dynamic_cast<SSDbImpl *>(&(*key->keychain()->database())));
        SSDbCursor dbCursor(ssDb, 1);
        dbCursor->recordType(CSSM_DL_DB_RECORD_PUBLIC_KEY);
        dbCursor->add(CSSM_DB_EQUAL, kInfoKeyLabel, CssmData(CFDataRef(label)));
        if (dbCursor->next(NULL, NULL, uniqueId)) {
            Item publicKey = key->keychain()->item(CSSM_DL_DB_RECORD_PUBLIC_KEY, uniqueId);
            result = reinterpret_cast<SecKeyRef>(publicKey->handle());
        }
    }

    if (result == NULL && key->publicKey()) {
        SecPointer<KeyItem> publicKey(new KeyItem(key->publicKey()));
        result = reinterpret_cast<SecKeyRef>(publicKey->handle());
    }

    END_SECKEYAPI
}

static KeyItem *SecCDSAKeyPrepareParameters(SecKeyRef key, SecKeyOperationType &operation, SecKeyAlgorithm algorithm,
                                            CSSM_ALGORITHMS &baseAlgorithm, CSSM_ALGORITHMS &secondaryAlgorithm,
                                            CSSM_ALGORITHMS &paddingAlgorithm, CFIndex &inputSizeLimit) {
    KeyItem *keyItem = CDSASecKey::keyItem(key);
    CSSM_KEYCLASS keyClass = keyItem->key()->header().keyClass();
    baseAlgorithm = keyItem->key()->header().algorithm();
    switch (baseAlgorithm) {
        case CSSM_ALGID_RSA:
            if ((keyClass == CSSM_KEYCLASS_PRIVATE_KEY && operation == kSecKeyOperationTypeSign) ||
                (keyClass == CSSM_KEYCLASS_PUBLIC_KEY && operation == kSecKeyOperationTypeVerify)) {
                if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureRaw)) {
                    secondaryAlgorithm = CSSM_ALGID_NONE;
                    paddingAlgorithm = CSSM_PADDING_NONE;
                    inputSizeLimit = 0;
                } else if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw)) {
                    secondaryAlgorithm = CSSM_ALGID_NONE;
                    paddingAlgorithm = CSSM_PADDING_PKCS1;
                    inputSizeLimit = -11;
                } else if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1)) {
                    secondaryAlgorithm = CSSM_ALGID_SHA1;
                    paddingAlgorithm = CSSM_PADDING_PKCS1;
                    inputSizeLimit = 20;
                } else if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224)) {
                    secondaryAlgorithm = CSSM_ALGID_SHA224;
                    paddingAlgorithm = CSSM_PADDING_PKCS1;
                    inputSizeLimit = 224 / 8;
                } else if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256)) {
                    secondaryAlgorithm = CSSM_ALGID_SHA256;
                    paddingAlgorithm = CSSM_PADDING_PKCS1;
                    inputSizeLimit = 256 / 8;
                } else if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384)) {
                    secondaryAlgorithm = CSSM_ALGID_SHA384;
                    paddingAlgorithm = CSSM_PADDING_PKCS1;
                    inputSizeLimit = 384 / 8;
                } else if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512)) {
                    secondaryAlgorithm = CSSM_ALGID_SHA512;
                    paddingAlgorithm = CSSM_PADDING_PKCS1;
                    inputSizeLimit = 512 / 8;
                } else if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15MD5)) {
                    secondaryAlgorithm = CSSM_ALGID_MD5;
                    paddingAlgorithm = CSSM_PADDING_PKCS1;
                    inputSizeLimit = 16;
                } else {
                    return NULL;
                }
            } else if ((keyClass == CSSM_KEYCLASS_PRIVATE_KEY && operation == kSecKeyOperationTypeDecrypt) ||
                       (keyClass == CSSM_KEYCLASS_PUBLIC_KEY && operation == kSecKeyOperationTypeEncrypt)) {
                if (CFEqual(algorithm, kSecKeyAlgorithmRSAEncryptionRaw)) {
                    secondaryAlgorithm = CSSM_ALGID_NONE;
                    paddingAlgorithm = CSSM_PADDING_NONE;
                    inputSizeLimit = 0;
                } else if (CFEqual(algorithm, kSecKeyAlgorithmRSAEncryptionPKCS1)) {
                    secondaryAlgorithm = CSSM_ALGID_NONE;
                    paddingAlgorithm = CSSM_PADDING_PKCS1;
                    inputSizeLimit = operation == kSecKeyOperationTypeEncrypt ? -11 : 0;
                } else {
                    return NULL;
                }
            } else if (keyClass == CSSM_KEYCLASS_PUBLIC_KEY && operation == kSecKeyOperationTypeDecrypt &&
                       CFEqual(algorithm, kSecKeyAlgorithmRSAEncryptionRaw)) {
                // Raw RSA decryption is identical to raw RSA encryption, so lets use encryption instead of decryption,
                // because CDSA keys refuses to perform decrypt using public key.
                operation = kSecKeyOperationTypeEncrypt;
                secondaryAlgorithm = CSSM_ALGID_NONE;
                paddingAlgorithm = CSSM_PADDING_NONE;
                inputSizeLimit = 0;
            } else {
                return NULL;
            }
            break;
        case CSSM_ALGID_ECDSA:
            if ((keyClass == CSSM_KEYCLASS_PRIVATE_KEY && operation == kSecKeyOperationTypeSign) ||
                (keyClass == CSSM_KEYCLASS_PUBLIC_KEY && operation == kSecKeyOperationTypeVerify)) {
                if (CFEqual(algorithm, kSecKeyAlgorithmECDSASignatureDigestRFC4754)) {
                    secondaryAlgorithm = CSSM_ALGID_NONE;
                    paddingAlgorithm = CSSM_PADDING_SIGRAW;
                } else if (CFEqual(algorithm, kSecKeyAlgorithmECDSASignatureDigestX962)) {
                    secondaryAlgorithm = CSSM_ALGID_NONE;
                    paddingAlgorithm = CSSM_PADDING_PKCS1;
                } else {
                    return NULL;
                }
            } else if (keyClass == CSSM_KEYCLASS_PRIVATE_KEY && operation == kSecKeyOperationTypeKeyExchange) {
                if (CFEqual(algorithm,kSecKeyAlgorithmECDHKeyExchangeStandard) ||
                    CFEqual(algorithm, kSecKeyAlgorithmECDHKeyExchangeCofactor)) {
                    baseAlgorithm = CSSM_ALGID_ECDH;
                } else if (CFEqual(algorithm, kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA1) ||
                           CFEqual(algorithm, kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA1)) {
                    baseAlgorithm = CSSM_ALGID_ECDH_X963_KDF;
                } else {
                    return NULL;
                }
            } else {
                return NULL;
            }
            break;
        default:
            MacOSError::throwMe(errSecParam);
    }
    return keyItem;
}

static CFDataRef
SecCDSAKeyCopyPaddedPlaintext(SecKeyRef key, CFDataRef plaintext, SecKeyAlgorithm algorithm) {
    CFIndex blockSize = CDSASecKey::keyItem(key)->key().header().LogicalKeySizeInBits / 8;
    CFIndex plaintextLength = CFDataGetLength(plaintext);
    if ((algorithm == kSecKeyAlgorithmRSAEncryptionRaw || algorithm == kSecKeyAlgorithmRSASignatureRaw)
        && plaintextLength < blockSize) {
        // Pre-pad with zeroes.
        CFMutableDataRef result(CFDataCreateMutable(kCFAllocatorDefault, blockSize));
        CFDataSetLength(result, blockSize);
        CFDataReplaceBytes(result, CFRangeMake(blockSize - plaintextLength, plaintextLength),
                           CFDataGetBytePtr(plaintext), plaintextLength);
        return result;
    } else {
        return CFDataRef(CFRetain(plaintext));
    }
}

static CFTypeRef SecCDSAKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                               CFArrayRef allAlgorithms, SecKeyOperationMode mode,
                                               CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    BEGIN_SECKEYAPI(CFTypeRef, kCFNull)
    CFIndex inputSizeLimit = 0;
    CSSM_ALGORITHMS baseAlgorithm, secondaryAlgorithm, paddingAlgorithm;
    KeyItem *keyItem = SecCDSAKeyPrepareParameters(key, operation, algorithm, baseAlgorithm, secondaryAlgorithm, paddingAlgorithm, inputSizeLimit);
    if (keyItem == NULL) {
        // Operation/algorithm/key combination is not supported.
        return kCFNull;
    } else if (mode == kSecKeyOperationModeCheckIfSupported) {
        // Operation is supported and caller wants to just know that.
        return kCFBooleanTrue;
    } else if (baseAlgorithm == CSSM_ALGID_RSA) {
        if (inputSizeLimit <= 0) {
            inputSizeLimit += SecCDSAKeyGetBlockSize(key);
        }
        if (CFDataGetLength((CFDataRef)in1) > inputSizeLimit) {
            MacOSError::throwMe(errSecParam);
        }
    }

    CDSASecKey *cdsaKey = static_cast<CDSASecKey *>(key);
    switch (operation) {
        case kSecKeyOperationTypeSign: {
            CssmClient::Sign signContext(keyItem->csp(), baseAlgorithm, secondaryAlgorithm);
            signContext.key(keyItem->key());
            signContext.cred(keyItem->getCredentials(CSSM_ACL_AUTHORIZATION_SIGN, cdsaKey->credentialType));
            signContext.add(CSSM_ATTRIBUTE_PADDING, paddingAlgorithm);
            CFRef<CFDataRef> input = SecCDSAKeyCopyPaddedPlaintext(key, CFRef<CFDataRef>::check(in1, errSecParam), algorithm);
            CssmAutoData signature(signContext.allocator());
            signContext.sign(CssmData(CFDataRef(input)), signature.get());
            result = CFDataCreate(NULL, static_cast<const UInt8 *>(signature.data()), CFIndex(signature.length()));
            break;
        }
        case kSecKeyOperationTypeVerify: {
            CssmClient::Verify verifyContext(keyItem->csp(), baseAlgorithm, secondaryAlgorithm);
            verifyContext.key(keyItem->key());
            verifyContext.cred(keyItem->getCredentials(CSSM_ACL_AUTHORIZATION_ANY, cdsaKey->credentialType));
            verifyContext.add(CSSM_ATTRIBUTE_PADDING, paddingAlgorithm);
            CFRef<CFDataRef> input = SecCDSAKeyCopyPaddedPlaintext(key, CFRef<CFDataRef>::check(in1, errSecParam), algorithm);
            verifyContext.verify(CssmData(CFDataRef(input)), CssmData(CFRef<CFDataRef>::check(in2, errSecParam)));
            result = kCFBooleanTrue;
            break;
        }
        case kSecKeyOperationTypeEncrypt: {
            CssmClient::Encrypt encryptContext(keyItem->csp(), baseAlgorithm);
            encryptContext.key(keyItem->key());
            encryptContext.padding(paddingAlgorithm);
            encryptContext.cred(keyItem->getCredentials(CSSM_ACL_AUTHORIZATION_ENCRYPT, cdsaKey->credentialType));
            CFRef<CFDataRef> input = SecCDSAKeyCopyPaddedPlaintext(key, CFRef<CFDataRef>::check(in1, errSecParam), algorithm);
            CssmAutoData output(encryptContext.allocator()), remainingData(encryptContext.allocator());
            size_t length = encryptContext.encrypt(CssmData(CFDataRef(input)), output.get(), remainingData.get());
            result = CFDataCreateMutable(kCFAllocatorDefault, output.length() + remainingData.length());
            CFDataAppendBytes(CFMutableDataRef(result), static_cast<const UInt8 *>(output.data()), output.length());
            CFDataAppendBytes(CFMutableDataRef(result), static_cast<const UInt8 *>(remainingData.data()), remainingData.length());
            CFDataSetLength(CFMutableDataRef(result), length);
            break;
        }
        case kSecKeyOperationTypeDecrypt: {
            CssmClient::Decrypt decryptContext(keyItem->csp(), baseAlgorithm);
            decryptContext.key(keyItem->key());
            decryptContext.padding(paddingAlgorithm);
            decryptContext.cred(keyItem->getCredentials(CSSM_ACL_AUTHORIZATION_DECRYPT, cdsaKey->credentialType));
            CssmAutoData output(decryptContext.allocator()), remainingData(decryptContext.allocator());
            size_t length = decryptContext.decrypt(CssmData(CFRef<CFDataRef>::check(in1, errSecParam)),
                                                   output.get(), remainingData.get());
            result = CFDataCreateMutable(kCFAllocatorDefault, output.length() + remainingData.length());
            CFDataAppendBytes(CFMutableDataRef(result), static_cast<const UInt8 *>(output.data()), output.length());
            CFDataAppendBytes(CFMutableDataRef(result), static_cast<const UInt8 *>(remainingData.data()), remainingData.length());
            CFDataSetLength(CFMutableDataRef(result), length);
            break;
        }
        case kSecKeyOperationTypeKeyExchange: {
            CFIndex requestedLength = 0;
            CssmData sharedInfo;
            switch (baseAlgorithm) {
                case CSSM_ALGID_ECDH:
                    requestedLength = (keyItem->key().header().LogicalKeySizeInBits + 7) / 8;
                    break;
                case CSSM_ALGID_ECDH_X963_KDF:
                    CFDictionaryRef params = CFRef<CFDictionaryRef>::check(in2, errSecParam);
                    CFTypeRef value = params ? CFDictionaryGetValue(params, kSecKeyKeyExchangeParameterRequestedSize) : NULL;
                    if (value == NULL || CFGetTypeID(value) != CFNumberGetTypeID() ||
                        !CFNumberGetValue(CFNumberRef(value), kCFNumberCFIndexType, &requestedLength)) {
                        MacOSError::throwMe(errSecParam);
                    }
                    value = CFDictionaryGetValue(params, kSecKeyKeyExchangeParameterSharedInfo);
                    if (value != NULL && CFGetTypeID(value) == CFDataGetTypeID()) {
                        sharedInfo = CssmData(CFDataRef(value));
                    }
                    break;
            }

            CssmClient::DeriveKey derive(keyItem->csp(), baseAlgorithm, CSSM_ALGID_AES, uint32(requestedLength * 8));
            derive.key(keyItem->key());
            derive.cred(keyItem->getCredentials(CSSM_ACL_AUTHORIZATION_DERIVE, kSecCredentialTypeDefault));
            derive.salt(sharedInfo);
            CssmData param(CFRef<CFDataRef>::check(in1, errSecParam));
            Key derivedKey = derive(&param, KeySpec(CSSM_KEYUSE_ANY, CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE));

            // Export raw data of newly derived key (by wrapping with an empty key).
            CssmClient::WrapKey wrapper(keyItem->csp(), CSSM_ALGID_NONE);
            Key wrappedKey = wrapper(derivedKey);
            result = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)wrappedKey->data(), CFIndex(wrappedKey->length()));
            break;
        }
        default:
            break;
    }

    END_SECKEYAPI
}

static Boolean SecCDSAKeyIsEqual(SecKeyRef key1, SecKeyRef key2) {
    CFErrorRef *error = NULL;
    BEGIN_SECKEYAPI(Boolean, false)

    result = CDSASecKey::keyItem(key1)->equal(*CDSASecKey::keyItem(key2));

    END_SECKEYAPI
}

static Boolean SecCDSAKeySetParameter(SecKeyRef key, CFStringRef name, CFPropertyListRef value, CFErrorRef *error) {
    BEGIN_SECKEYAPI(Boolean, false)

    if (CFEqual(name, kSecUseAuthenticationUI)) {
        static_cast<CDSASecKey *>(key)->credentialType = CFEqual(value, kSecUseAuthenticationUIAllow) ? kSecCredentialTypeDefault : kSecCredentialTypeNoUI;
        result = true;
    } else {
        result = SecError(errSecUnimplemented, error, CFSTR("Unsupported parameter '%@' for SecKeyCDSASetParameter"), name);
    }

    END_SECKEYAPI
}

const SecKeyDescriptor kSecCDSAKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "CDSAKey",
    .extraBytes = (sizeof(class CDSASecKey) > sizeof(struct __SecKey) ? (sizeof(class CDSASecKey) - sizeof(struct __SecKey)) : 0),

    .init = SecCDSAKeyInit,
    .destroy = SecCDSAKeyDestroy,
    .describe = SecCDSAKeyCopyDescription,
    .blockSize = SecCDSAKeyGetBlockSize,
    .copyDictionary = SecCDSAKeyCopyAttributeDictionary,
    .getAlgorithmID = SecCDSAKeyGetAlgorithmId,
    .copyPublic = SecCDSAKeyCopyPublicBytes,
    .copyPublicKey = SecCDSAKeyCopyPublicKey,
    .copyExternalRepresentation = SecCDSAKeyCopyExternalRepresentation,
    .copyOperationResult = SecCDSAKeyCopyOperationResult,
    .isEqual = SecCDSAKeyIsEqual,
    .setParameter = SecCDSAKeySetParameter,
};

namespace Security {
    namespace KeychainCore {
        SecCFObject *KeyItem::fromSecKeyRef(CFTypeRef ptr) {
            if (ptr == NULL || CFGetTypeID(ptr) != SecKeyGetTypeID()) {
                return NULL;
            }

            SecKeyRef key = static_cast<SecKeyRef>(const_cast<void *>(ptr));
            if (key->key_class == &kSecCDSAKeyDescriptor) {
                return static_cast<SecCFObject *>(key->key);
            }

            CFRef<SecKeyRef> cdsaKey = SecKeyCopyAuxilliaryCDSAKeyForKey(key);
            if (!cdsaKey) {
                // Create CDSA key from exported data of existing key.
                CFRef<CFDictionaryRef> keyAttributes = SecKeyCopyAttributes(key);
                if (keyAttributes) {
                    CFRef<CFDataRef> keyData = SecKeyCopyExternalRepresentation(key, NULL);
                    if (!keyData) {
                        CFTypeRef pubKeyHash = CFDictionaryGetValue(keyAttributes, kSecAttrApplicationLabel);
                        const void *keys[] = { kSecClass, kSecUseDataProtectionKeychain, kSecReturnRef, kSecMatchLimit };
                        const void *values[] = { kSecClassIdentity, kCFBooleanFalse, kCFBooleanTrue, kSecMatchLimitAll };
                        CFRef<CFDictionaryRef> query = CFDictionaryCreate(kCFAllocatorDefault, keys, values,
                                                                          sizeof(keys) / sizeof(*keys),
                                                                          &kCFTypeDictionaryKeyCallBacks,
                                                                          &kCFTypeDictionaryValueCallBacks);
                        CFRef<CFArrayRef> identities;
                        OSStatus status = SecItemCopyMatching(query, (CFTypeRef *)identities.take());
                        if (status == errSecSuccess) {
                            for (int i = 0; i < CFArrayGetCount(identities); ++i) {
                                CFRef<SecKeyRef> privateKey;
                                if (SecIdentityCopyPrivateKey((SecIdentityRef)CFArrayGetValueAtIndex(identities, i), privateKey.take()) != errSecSuccess) {
                                    continue;
                                }
                                CFRef<CFDictionaryRef> attrs = SecKeyCopyAttributes(privateKey);
                                if (CFEqual(CFDictionaryGetValue(attrs, kSecAttrApplicationLabel), pubKeyHash)) {
                                    cdsaKey = privateKey;
                                    SecKeySetAuxilliaryCDSAKeyForKey(key, cdsaKey.get());
                                    break;
                                }
                            }
                        }
                    } else {
                        cdsaKey.take(SecKeyCreateFromData(keyAttributes, keyData, NULL));
                        if (cdsaKey) {
                            SecKeySetAuxilliaryCDSAKeyForKey(key, cdsaKey.get());
                        }
                    }
                }
            }

            return cdsaKey ? CDSASecKey::keyItem(cdsaKey.get()) : NULL;
        }

        // You need to hold this key's MutexForObject when you run this
        void KeyItem::attachSecKeyRef() const {
            SecKeyRef key = SecKeyCreate(NULL, &kSecCDSAKeyDescriptor, reinterpret_cast<const uint8_t *>(this), 0, 0);
            CDSASecKey::keyItem(key)->mWeakSecKeyRef = key;
        }

    }
}

extern "C" Boolean SecKeyIsCDSAKey(SecKeyRef ref);
Boolean SecKeyIsCDSAKey(SecKeyRef ref) {
    return ref->key_class == &kSecCDSAKeyDescriptor;
}

Boolean SecKeyIsLegacyInstance(SecKeyRef keyRef) {
    if (keyRef == NULL) {
        return false;
    }
    CFTypeID typeID = CFGetTypeID(keyRef);
    if (typeID == _kCFRuntimeNotATypeID) {
        return false;
    }
    return ((typeID == SecKeyGetTypeID() && SecKeyIsCDSAKey(keyRef)) ||
            (typeID == SecKeychainItemGetTypeID())) ? true : false;
}

static OSStatus SecKeyCreatePairInternal(
	SecKeychainRef keychainRef,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE publicKeyUsage,
	uint32 publicKeyAttr,
	CSSM_KEYUSE privateKeyUsage,
	uint32 privateKeyAttr,
	SecAccessRef initialAccess,
	SecKeyRef* publicKeyRef,
	SecKeyRef* privateKeyRef)
{
	BEGIN_SECAPI

    Keychain keychain;
    SecPointer<Access> theAccess(initialAccess ? Access::required(initialAccess) : new Access("<key>"));
    SecPointer<KeyItem> pubItem, privItem;
    if (((publicKeyAttr | privateKeyAttr) & CSSM_KEYATTR_PERMANENT) != 0) {
        keychain = Keychain::optional(keychainRef);
    }
    StMaybeLock<Mutex> _(keychain ? keychain->getKeychainMutex() : NULL);
    KeyItem::createPair(keychain, algorithm, keySizeInBits, contextHandle, publicKeyUsage, publicKeyAttr,
                        privateKeyUsage, privateKeyAttr, theAccess, pubItem, privItem);

	// Return the generated keys.
	if (publicKeyRef)
		*publicKeyRef = pubItem->handle();
	if (privateKeyRef)
		*privateKeyRef = privItem->handle();

	END_SECAPI
}

OSStatus
SecKeyCreatePair(
	SecKeychainRef keychainRef,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE publicKeyUsage,
	uint32 publicKeyAttr,
	CSSM_KEYUSE privateKeyUsage,
	uint32 privateKeyAttr,
	SecAccessRef initialAccess,
	SecKeyRef* publicKeyRef,
	SecKeyRef* privateKeyRef)
{
    OSStatus result = SecKeyCreatePairInternal(keychainRef, algorithm, keySizeInBits, contextHandle, publicKeyUsage,
                                               publicKeyAttr, privateKeyUsage, privateKeyAttr, initialAccess, publicKeyRef, privateKeyRef);

    return result;
}



OSStatus
SecKeyGetCSSMKey(SecKeyRef key, const CSSM_KEY **cssmKey)
{
	BEGIN_SECAPI

	Required(cssmKey) = KeyItem::required(key)->key();

	END_SECAPI
}


//
// Private APIs
//

static ModuleNexus<Mutex> gSecReturnedKeyCSPsMutex;
static ModuleNexus<std::set<CssmClient::CSP>> gSecReturnedKeyCSPs;

OSStatus
SecKeyGetCSPHandle(SecKeyRef keyRef, CSSM_CSP_HANDLE *cspHandle)
{
    BEGIN_SECAPI

	SecPointer<KeyItem> keyItem(KeyItem::required(keyRef));

    // Once we vend this handle, we can no longer delete this CSP object via RAII (and thus call CSSM_ModuleDetach on the CSP).
    // Keep a global pointer to it to force the CSP to stay live forever.
    CssmClient::CSP returnedKeyCSP = keyItem->csp();
    {
        StLock<Mutex> _(gSecReturnedKeyCSPsMutex());
        gSecReturnedKeyCSPs().insert(returnedKeyCSP);
    }
	Required(cspHandle) = returnedKeyCSP->handle();

	END_SECAPI
}

/* deprecated as of 10.8 */
OSStatus
SecKeyGetAlgorithmID(SecKeyRef keyRef, const CSSM_X509_ALGORITHM_IDENTIFIER **algid)
{
	BEGIN_SECAPI

	SecPointer<KeyItem> keyItem(KeyItem::required(keyRef));
	Required(algid) = &keyItem->algorithmIdentifier();

	END_SECAPI
}

OSStatus
SecKeyGetStrengthInBits(SecKeyRef keyRef, const CSSM_X509_ALGORITHM_IDENTIFIER *algid, unsigned int *strength)
{
    BEGIN_SECAPI

	SecPointer<KeyItem> keyItem(KeyItem::required(keyRef));
	Required(strength) = keyItem->strengthInBits(algid);

	END_SECAPI
}

OSStatus
SecKeyGetCredentials(
	SecKeyRef keyRef,
	CSSM_ACL_AUTHORIZATION_TAG operation,
	SecCredentialType credentialType,
	const CSSM_ACCESS_CREDENTIALS **outCredentials)
{
	BEGIN_SECAPI

	SecPointer<KeyItem> keyItem(KeyItem::required(keyRef));
	Required(outCredentials) = keyItem->getCredentials(operation, credentialType);

	END_SECAPI
}

OSStatus
SecKeyImportPair(
	SecKeychainRef keychainRef,
	const CSSM_KEY *publicCssmKey,
	const CSSM_KEY *privateCssmKey,
	SecAccessRef initialAccess,
	SecKeyRef* publicKey,
	SecKeyRef* privateKey)
{
	BEGIN_SECAPI

	Keychain keychain = Keychain::optional(keychainRef);
	SecPointer<Access> theAccess(initialAccess ? Access::required(initialAccess) : new Access("<key>"));
	SecPointer<KeyItem> pubItem, privItem;

	KeyItem::importPair(keychain,
		Required(publicCssmKey),
		Required(privateCssmKey),
        theAccess,
        pubItem,
        privItem);

	// Return the generated keys.
	if (publicKey)
		*publicKey = pubItem->handle();
	if (privateKey)
		*privateKey = privItem->handle();

	END_SECAPI
}

static OSStatus
SecKeyGenerateWithAttributes(
	SecKeychainAttributeList* attrList,
	SecKeychainRef keychainRef,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE keyUsage,
	uint32 keyAttr,
	SecAccessRef initialAccess,
	SecKeyRef* keyRef)
{
	BEGIN_SECAPI

	Keychain keychain;
	SecPointer<Access> theAccess;

	if (keychainRef)
		keychain = KeychainImpl::required(keychainRef);
	if (initialAccess)
		theAccess = Access::required(initialAccess);

	SecPointer<KeyItem> item = KeyItem::generateWithAttributes(attrList,
        keychain,
        algorithm,
        keySizeInBits,
        contextHandle,
        keyUsage,
        keyAttr,
        theAccess);

	// Return the generated key.
	if (keyRef)
		*keyRef = item->handle();

	END_SECAPI
}

OSStatus
SecKeyGenerate(
	SecKeychainRef keychainRef,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE keyUsage,
	uint32 keyAttr,
	SecAccessRef initialAccess,
	SecKeyRef* keyRef)
{
	return SecKeyGenerateWithAttributes(NULL,
		keychainRef, algorithm, keySizeInBits,
		contextHandle, keyUsage, keyAttr,
		initialAccess, keyRef);
}

/* new in 10.6 */
/* Generate a floating key reference from a CSSM_KEY */
OSStatus
SecKeyCreateWithCSSMKey(const CSSM_KEY *cssmKey,
    SecKeyRef *keyRef)
{
	BEGIN_SECAPI

	Required(cssmKey);
    if(cssmKey->KeyData.Length == 0){
        MacOSError::throwMe(errSecInvalidAttributeKeyLength);
    }
    if(cssmKey->KeyData.Data == NULL){
        MacOSError::throwMe(errSecInvalidPointer);
    }
	CssmClient::CSP csp(cssmKey->KeyHeader.CspId);
	CssmClient::Key key(csp, *cssmKey);
	KeyItem *item = new KeyItem(key);

	// Return the generated key.
	if (keyRef)
        *keyRef = SecKeyCreate(NULL, &kSecCDSAKeyDescriptor, (const uint8_t *)item, 0, 0);

	END_SECAPI
}



static u_int32_t ConvertCFStringToInteger(CFStringRef ref)
{
	if (ref == NULL)
	{
		return 0;
	}

	// figure out the size of the string
	CFIndex numChars = CFStringGetMaximumSizeForEncoding(CFStringGetLength(ref), kCFStringEncodingUTF8);
	char *buffer = (char *)malloc(numChars);
    if (NULL == buffer) {
        UnixError::throwMe(ENOMEM);
    }
	if (!CFStringGetCString(ref, buffer, numChars, kCFStringEncodingUTF8))
	{
        free(buffer);
		MacOSError::throwMe(errSecParam);
	}

    u_int32_t result = atoi(buffer);
    free(buffer);
    return result;
}



static OSStatus CheckAlgorithmType(CFDictionaryRef parameters, CSSM_ALGORITHMS &algorithms)
{
	// figure out the algorithm to use
	CFStringRef ktype = (CFStringRef) CFDictionaryGetValue(parameters, kSecAttrKeyType);
	if (ktype == NULL)
	{
		return errSecParam;
	}

	if (CFEqual(ktype, kSecAttrKeyTypeRSA)) {
		algorithms = CSSM_ALGID_RSA;
		return errSecSuccess;
       } else if(CFEqual(ktype, kSecAttrKeyTypeECDSA) ||
                       CFEqual(ktype, kSecAttrKeyTypeEC)) {
		algorithms = CSSM_ALGID_ECDSA;
		return errSecSuccess;
	} else if(CFEqual(ktype, kSecAttrKeyTypeAES)) {
		algorithms = CSSM_ALGID_AES;
		return errSecSuccess;
	} else if(CFEqual(ktype, kSecAttrKeyType3DES)) {
		algorithms = CSSM_ALGID_3DES;
		return errSecSuccess;
	} else {
		return errSecUnsupportedAlgorithm;
	}
}



static OSStatus GetKeySize(CFDictionaryRef parameters, CSSM_ALGORITHMS algorithms, uint32 &keySizeInBits)
{

    // get the key size and check it for validity
    CFTypeRef ref = CFDictionaryGetValue(parameters, kSecAttrKeySizeInBits);

    keySizeInBits = kSecDefaultKeySize;

    CFTypeID bitSizeType = CFGetTypeID(ref);
    if (bitSizeType == CFStringGetTypeID())
        keySizeInBits = ConvertCFStringToInteger((CFStringRef) ref);
    else if (bitSizeType == CFNumberGetTypeID())
        CFNumberGetValue((CFNumberRef) ref, kCFNumberSInt32Type, &keySizeInBits);
    else return errSecParam;


    switch (algorithms) {
    case CSSM_ALGID_ECDSA:
        if(keySizeInBits == kSecDefaultKeySize) keySizeInBits = kSecp256r1;
        if(keySizeInBits == kSecp192r1 || keySizeInBits == kSecp256r1 || keySizeInBits == kSecp384r1 || keySizeInBits == kSecp521r1 ) return errSecSuccess;
        break;
    case CSSM_ALGID_RSA:
			  if(keySizeInBits % 8) return errSecParam;
        if(keySizeInBits == kSecDefaultKeySize) keySizeInBits = 2048;
        if(keySizeInBits >= kSecRSAMin && keySizeInBits <= kSecRSAMax) return errSecSuccess;
        break;
    case CSSM_ALGID_AES:
        if(keySizeInBits == kSecDefaultKeySize) keySizeInBits = kSecAES128;
        if(keySizeInBits == kSecAES128 || keySizeInBits == kSecAES192 || keySizeInBits == kSecAES256) return errSecSuccess;
        break;
    case CSSM_ALGID_3DES:
        if(keySizeInBits == kSecDefaultKeySize) keySizeInBits = kSec3DES192;
        if(keySizeInBits == kSec3DES192) return errSecSuccess;
        break;
    default:
        break;
    }
    return errSecParam;
}



enum AttributeType
{
	kStringType,
	kBooleanType,
	kIntegerType
};



struct ParameterAttribute
{
	const CFStringRef *name;
	AttributeType type;
};



static ParameterAttribute gAttributes[] =
{
	{
		&kSecAttrLabel,
		kStringType
	},
	{
		&kSecAttrIsPermanent,
		kBooleanType
	},
	{
		&kSecAttrApplicationTag,
		kStringType
	},
	{
		&kSecAttrEffectiveKeySize,
		kBooleanType
	},
	{
		&kSecAttrCanEncrypt,
		kBooleanType
	},
	{
		&kSecAttrCanDecrypt,
		kBooleanType
	},
	{
		&kSecAttrCanDerive,
		kBooleanType
	},
	{
		&kSecAttrCanSign,
		kBooleanType
	},
	{
		&kSecAttrCanVerify,
		kBooleanType
	},
	{
		&kSecAttrCanUnwrap,
		kBooleanType
	}
};

const int kNumberOfAttributes = sizeof(gAttributes) / sizeof(ParameterAttribute);

static OSStatus ScanDictionaryForParameters(CFDictionaryRef parameters, void* attributePointers[])
{
	int i;
	for (i = 0; i < kNumberOfAttributes; ++i)
	{
		// see if the corresponding tag exists in the dictionary
		CFTypeRef value = CFDictionaryGetValue(parameters, *(gAttributes[i].name));
		if (value != NULL)
		{
			switch (gAttributes[i].type)
			{
				case kStringType:
					// just return the value
					*(CFTypeRef*) attributePointers[i] = value;
				break;

				case kBooleanType:
				{
					CFBooleanRef bRef = (CFBooleanRef) value;
					*(bool*) attributePointers[i] = CFBooleanGetValue(bRef);
				}
				break;

				case kIntegerType:
				{
					CFNumberRef nRef = (CFNumberRef) value;
					CFNumberGetValue(nRef, kCFNumberSInt32Type, attributePointers[i]);
				}
				break;
			}
		}
	}

	return errSecSuccess;
}



static OSStatus GetKeyParameters(CFDictionaryRef parameters, int keySize, bool isPublic, CSSM_KEYUSE &keyUse, uint32 &attrs, CFTypeRef &labelRef, CFDataRef &applicationTagRef)
{
	// establish default values
	labelRef = NULL;
	bool isPermanent = false;
	applicationTagRef = NULL;
	CFTypeRef effectiveKeySize = NULL;
	bool canDecrypt = isPublic ? false : true;
	bool canEncrypt = !canDecrypt;
	bool canDerive = true;
	bool canSign = isPublic ? false : true;
	bool canVerify = !canSign;
	bool canUnwrap = isPublic ? false : true;
	attrs = CSSM_KEYATTR_EXTRACTABLE;
	keyUse = 0;

	void* attributePointers[] = {&labelRef, &isPermanent, &applicationTagRef, &effectiveKeySize, &canEncrypt, &canDecrypt,
								 &canDerive, &canSign, &canVerify, &canUnwrap};

	// look for modifiers in the general dictionary
	OSStatus result = ScanDictionaryForParameters(parameters, attributePointers);
	if (result != errSecSuccess)
	{
		return result;
	}

	// see if we have anything which modifies the defaults
	CFTypeRef key;
	if (isPublic)
	{
		key = kSecPublicKeyAttrs;
	}
	else
	{
		key = kSecPrivateKeyAttrs;
	}

	CFTypeRef dType = CFDictionaryGetValue(parameters, key);
	if (dType != NULL)
	{
		// this had better be a dictionary
		if (CFGetTypeID(dType) != CFDictionaryGetTypeID())
		{
			return errSecParam;
		}

		// pull any additional parameters out of this dictionary
		result = ScanDictionaryForParameters((CFDictionaryRef)dType, attributePointers);
		if (result != errSecSuccess)
		{
			return result;
		}
	}

	// figure out the key usage
	keyUse = 0;
	if (canDecrypt)
	{
		keyUse |= CSSM_KEYUSE_DECRYPT;
	}

	if (canEncrypt)
	{
		keyUse |= CSSM_KEYUSE_ENCRYPT;
	}

	if (canDerive)
	{
		keyUse |= CSSM_KEYUSE_DERIVE;
	}

	if (canSign)
	{
		keyUse |= CSSM_KEYUSE_SIGN;
	}

	if (canVerify)
	{
		keyUse |= CSSM_KEYUSE_VERIFY;
	}

	if (canUnwrap)
	{
		keyUse |= CSSM_KEYUSE_UNWRAP;
	}

	// public key is always extractable;
	// private key is extractable by default unless explicitly set to false
	CFTypeRef value = NULL;
	if (!isPublic && CFDictionaryGetValueIfPresent(parameters, kSecAttrIsExtractable, (const void **)&value) && value)
	{
		Boolean keyIsExtractable = CFEqual(kCFBooleanTrue, value);
		if (!keyIsExtractable)
			attrs = 0;
	}

    if (isPermanent) {
        attrs |= CSSM_KEYATTR_PERMANENT;
    }

	return errSecSuccess;
}



static OSStatus MakeKeyGenParametersFromDictionary(CFDictionaryRef parameters,
												   CSSM_ALGORITHMS &algorithms,
												   uint32 &keySizeInBits,
												   CSSM_KEYUSE &publicKeyUse,
												   uint32 &publicKeyAttr,
												   CFTypeRef &publicKeyLabelRef,
												   CFDataRef &publicKeyAttributeTagRef,
												   CSSM_KEYUSE &privateKeyUse,
												   uint32 &privateKeyAttr,
												   CFTypeRef &privateKeyLabelRef,
												   CFDataRef &privateKeyAttributeTagRef,
												   SecAccessRef &initialAccess)
{
	OSStatus result;

	result = CheckAlgorithmType(parameters, algorithms);
	if (result != errSecSuccess)
	{
		return result;
	}

	result = GetKeySize(parameters, algorithms, keySizeInBits);
	if (result != errSecSuccess)
	{
		return result;
	}

	result = GetKeyParameters(parameters, keySizeInBits, false, privateKeyUse, privateKeyAttr, privateKeyLabelRef, privateKeyAttributeTagRef);
	if (result != errSecSuccess)
	{
		return result;
	}

	result = GetKeyParameters(parameters, keySizeInBits, true, publicKeyUse, publicKeyAttr, publicKeyLabelRef, publicKeyAttributeTagRef);
	if (result != errSecSuccess)
	{
		return result;
	}

	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrAccess, (const void **)&initialAccess))
	{
		initialAccess = NULL;
	}
	else if (SecAccessGetTypeID() != CFGetTypeID(initialAccess))
	{
		return errSecParam;
	}

	return errSecSuccess;
}



static OSStatus SetKeyLabelAndTag(SecKeyRef keyRef, CFTypeRef label, CFDataRef tag)
{
	int numToModify = 0;
	if (label != NULL)
	{
		numToModify += 1;
	}

	if (tag != NULL)
	{
		numToModify += 1;
	}

	if (numToModify == 0)
	{
		return errSecSuccess;
	}

	SecKeychainAttributeList attrList;
	SecKeychainAttribute attributes[numToModify];

	int i = 0;
    void *data = NULL;

	if (label != NULL)
	{
		if (CFStringGetTypeID() == CFGetTypeID(label)) {
			CFStringRef label_string = static_cast<CFStringRef>(label);
			attributes[i].tag = kSecKeyPrintName;
			attributes[i].data = (void*) CFStringGetCStringPtr(label_string, kCFStringEncodingUTF8);
			if (NULL == attributes[i].data) {
				CFIndex buffer_length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(label_string), kCFStringEncodingUTF8);
				data = attributes[i].data = malloc((size_t)buffer_length);
				if (NULL == attributes[i].data) {
					UnixError::throwMe(ENOMEM);
				}
				if (!CFStringGetCString(label_string, static_cast<char *>(attributes[i].data), buffer_length, kCFStringEncodingUTF8)) {
                    free(data);
					MacOSError::throwMe(errSecParam);
				}
			}
			attributes[i].length = (UInt32)strlen(static_cast<char *>(attributes[i].data));
		} else if (CFDataGetTypeID() == CFGetTypeID(label)) {
			// 10.6 bug compatibility
			CFDataRef label_data = static_cast<CFDataRef>(label);
			attributes[i].tag = kSecKeyLabel;
			attributes[i].data = (void*) CFDataGetBytePtr(label_data);
			attributes[i].length = (UInt32)CFDataGetLength(label_data);
		} else {
			MacOSError::throwMe(errSecParam);
		}
		i++;
	}

	if (tag != NULL)
	{
		attributes[i].tag = kSecKeyApplicationTag;
		attributes[i].data = (void*) CFDataGetBytePtr(tag);
		attributes[i].length = (UInt32)CFDataGetLength(tag);
		i++;
	}

	attrList.count = numToModify;
	attrList.attr = attributes;
    
	OSStatus result = SecKeychainItemModifyAttributesAndData((SecKeychainItemRef) keyRef, &attrList, 0, NULL);
    if (data)
    {
        free(data);
    }
    
    return result;
}


static CFDictionaryRef CopyFullKeyParameters(CFDictionaryRef baseDictionary, CFStringRef subtypeName) {
    CFMutableDictionaryRef merged = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, baseDictionary);
    CFDictionarySetValue(merged, kSecClass, kSecClassKey);
    CFDictionaryRef overrideDictionary = (CFDictionaryRef)CFDictionaryGetValue(baseDictionary, subtypeName);
    if (overrideDictionary != NULL) {
        cfDictionaryApplyBlock(overrideDictionary, ^(const void *key, const void *value) {
            CFDictionarySetValue(merged, key, value);
        });
    }
    return merged;
}

static SecKeyRef _SecCDSAKeyCreateRandomKey(bool alwaysPermanent, CFDictionaryRef parameters, SecKeyRef *publicKey, CFErrorRef *error) {
    CSSM_ALGORITHMS algorithms;
    uint32 keySizeInBits;
    CSSM_KEYUSE publicKeyUse;
    uint32 publicKeyAttr;
    CFTypeRef publicKeyLabelRef;
    CFDataRef publicKeyAttributeTagRef;
    CSSM_KEYUSE privateKeyUse;
    uint32 privateKeyAttr;
    CFTypeRef privateKeyLabelRef;
    CFDataRef privateKeyAttributeTagRef;
    SecAccessRef initialAccess;
    SecKeychainRef keychain;

    OSStatus status = MakeKeyGenParametersFromDictionary(parameters, algorithms, keySizeInBits, publicKeyUse, publicKeyAttr, publicKeyLabelRef, publicKeyAttributeTagRef, privateKeyUse, privateKeyAttr, privateKeyLabelRef, privateKeyAttributeTagRef, initialAccess);

    if (status != errSecSuccess) {
        SecError(status, error, CFSTR("inconsistent parameters for CDSA key generation"));
        return nil;
    }

    // verify keychain parameter
    keychain = (SecKeychainRef)CFDictionaryGetValue(parameters, kSecUseKeychain);
    if (keychain != NULL && SecKeychainGetTypeID() != CFGetTypeID(keychain)) {
        keychain = NULL;
    }

    if (alwaysPermanent) {
        publicKeyAttr |= CSSM_KEYATTR_PERMANENT;
        privateKeyAttr |= CSSM_KEYATTR_PERMANENT;
    }

    // do the key generation
    SecKeyRef privateKey = NULL;
    status = SecKeyCreatePair(keychain, algorithms, keySizeInBits, 0, publicKeyUse, publicKeyAttr, privateKeyUse, privateKeyAttr, initialAccess, publicKey, &privateKey);
    if (status != errSecSuccess) {
        SecError(status, error, CFSTR("failed to generate CDSA key"));
        return nil;
    }

    // set the label and print attributes on the keys
    SetKeyLabelAndTag(*publicKey, publicKeyLabelRef, publicKeyAttributeTagRef);
    SetKeyLabelAndTag(privateKey, privateKeyLabelRef, privateKeyAttributeTagRef);
    return privateKey;
}

extern "C" SecKeyRef SecKeyCreateRandomKey_ios(CFDictionaryRef parameters, CFErrorRef *error);
OSStatus SecItemCategorizeQuery(CFDictionaryRef query, bool &can_target_ios, bool &can_target_osx, bool &useDataProtectionKeychainFlag);

/* new in 10.6 */
/* Generate a private/public keypair. */
static SecKeyRef _SecKeyCreateRandomKey(bool alwaysPermanent, CFDictionaryRef parameters, SecKeyRef *publicKey, CFErrorRef *error) {
	BEGIN_SECKEYAPI(SecKeyRef, NULL)

	Required(parameters);
    Required(publicKey);

    bool privateCanTargetIOS = false, privateCanTargetMacOS = false, unused;
    CFDictionaryRef merged = CopyFullKeyParameters(parameters, kSecPrivateKeyAttrs);
    OSStatus status = SecItemCategorizeQuery(merged, privateCanTargetIOS, privateCanTargetMacOS, unused);
    CFRelease(merged);
    if (status != errSecSuccess) {
        SecError(status, error, CFSTR("inconsistent private key parameters for key generation"));
        return nil;
    }

    bool publicCanTargetIOS = false, publicCanTargetMacOS = false;
    merged = CopyFullKeyParameters(parameters, kSecPublicKeyAttrs);
    status = SecItemCategorizeQuery(merged, publicCanTargetIOS, publicCanTargetMacOS, unused);
    CFRelease(merged);
    if (status != errSecSuccess) {
        SecError(status, error, CFSTR("inconsistent public key parameters for key generation"));
        return nil;
    }

    // CDSA keys have precedence unless we are told otherwise.
    if (!publicCanTargetMacOS || !privateCanTargetMacOS) {
        if (publicCanTargetIOS && privateCanTargetIOS) {
            // Generate keys in iOS keychain.
            *publicKey = NULL;
            result = SecKeyCreateRandomKey_ios(parameters, error);
        } else {
            // Inconsistent query, cannot be generated neither on iOS or macOS.
            SecError(errSecParam, error, CFSTR("inconsistent key parameters, cannot target neither modern nor legacy key implementation"));
        }
    } else {
        result = _SecCDSAKeyCreateRandomKey(alwaysPermanent, parameters, publicKey, error);
    }
	END_SECKEYAPI
}

OSStatus
SecKeyGeneratePair(CFDictionaryRef parameters, SecKeyRef *publicKey, SecKeyRef *privateKey) {
    CFErrorRef error = NULL;
    SecKeyRef pubKey;
    SecKeyRef privKey = _SecKeyCreateRandomKey(true, parameters, &pubKey, &error);
    if (privKey == NULL) {
        OSStatus status = errSecInternal;
        if (error != NULL) {
            status = (OSStatus)CFErrorGetCode(error);
            CFRelease(error);
        }
        return status;
    }

    if (publicKey != NULL) {
        if (pubKey == NULL) {
            *publicKey = SecKeyCopyPublicKey(privKey);
        } else {
            *publicKey = pubKey;
        }
    } else if (pubKey != NULL) {
        CFRelease(pubKey);
    }
    if (privateKey != NULL) {
        *privateKey = privKey;
    } else {
        CFRelease(privKey);
    }
    return errSecSuccess;
}

SecKeyRef
SecKeyCreateRandomKey(CFDictionaryRef parameters, CFErrorRef *error) {
    SecKeyRef publicKey = NULL;
    SecKeyRef privKey = _SecKeyCreateRandomKey(false, parameters, &publicKey, error);
    if (privKey != NULL && publicKey != NULL) {
        CFRelease(publicKey);
    }
    return privKey;
}

OSStatus SecKeyRawVerifyOSX(
    SecKeyRef           key,            /* Public key */
	SecPadding          padding,		/* kSecPaddingNone or kSecPaddingPKCS1 */
	const uint8_t       *signedData,	/* signature over this data */
	size_t              signedDataLen,	/* length of dataToSign */
	const uint8_t       *sig,			/* signature */
	size_t              sigLen)
{
    return SecKeyRawVerify(key,padding,signedData,signedDataLen,sig,sigLen);
}

/*
    M4 Additions
*/

static CFTypeRef
utilGetStringFromCFDict(CFDictionaryRef parameters, CFTypeRef key, CFTypeRef defaultValue)
{
		CFTypeRef value = CFDictionaryGetValue(parameters, key);
        if (value != NULL) return value;
        return defaultValue;
}

static uint32_t
utilGetNumberFromCFDict(CFDictionaryRef parameters, CFTypeRef key, uint32_t defaultValue)
{
        uint32_t integerValue;
		CFTypeRef value = CFDictionaryGetValue(parameters, key);
        if (value != NULL) {
            CFNumberRef nRef = (CFNumberRef) value;
            CFNumberGetValue(nRef, kCFNumberSInt32Type, &integerValue);
            return integerValue;
        }
        return defaultValue;
 }

static uint32_t
utilGetMaskValFromCFDict(CFDictionaryRef parameters, CFTypeRef key, uint32_t maskValue)
{
		CFTypeRef value = CFDictionaryGetValue(parameters, key);
        if (value != NULL) {
            CFBooleanRef bRef = (CFBooleanRef) value;
            if(CFBooleanGetValue(bRef)) return maskValue;
        }
        return 0;
}

static void
utilGetKeyParametersFromCFDict(CFDictionaryRef parameters, CSSM_ALGORITHMS *algorithm, uint32 *keySizeInBits, CSSM_KEYUSE *keyUsage, CSSM_KEYCLASS *keyClass)
{
    CFTypeRef algorithmDictValue = utilGetStringFromCFDict(parameters, kSecAttrKeyType, kSecAttrKeyTypeAES);
    CFTypeRef keyClassDictValue = utilGetStringFromCFDict(parameters, kSecAttrKeyClass, kSecAttrKeyClassSymmetric);

    if(CFEqual(algorithmDictValue, kSecAttrKeyTypeAES)) {
        *algorithm = CSSM_ALGID_AES;
        *keySizeInBits = 128;
        *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeDES)) {
        *algorithm = CSSM_ALGID_DES;
        *keySizeInBits = 128;
         *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyType3DES)) {
        *algorithm = CSSM_ALGID_3DES_3KEY_EDE;
        *keySizeInBits = 128;
        *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeRC4)) {
        *algorithm = CSSM_ALGID_RC4;
        *keySizeInBits = 128;
        *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeRC2)) {
        *algorithm = CSSM_ALGID_RC2;
        *keySizeInBits = 128;
         *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeCAST)) {
        *algorithm = CSSM_ALGID_CAST;
        *keySizeInBits = 128;
         *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeRSA)) {
        *algorithm = CSSM_ALGID_RSA;
        *keySizeInBits = 128;
         *keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeDSA)) {
        *algorithm = CSSM_ALGID_DSA;
        *keySizeInBits = 128;
         *keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeECDSA) ||
            CFEqual(algorithmDictValue, kSecAttrKeyTypeEC)) {
        *algorithm = CSSM_ALGID_ECDSA;
        *keySizeInBits = 128;
        *keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
    } else {
        *algorithm = CSSM_ALGID_AES;
        *keySizeInBits = 128;
        *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    }

    if(CFEqual(keyClassDictValue, kSecAttrKeyClassPublic)) {
        *keyClass = CSSM_KEYCLASS_PUBLIC_KEY;
    } else if(CFEqual(keyClassDictValue, kSecAttrKeyClassPrivate)) {
        *keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
    } else if(CFEqual(keyClassDictValue, kSecAttrKeyClassSymmetric)) {
         *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    }

    *keySizeInBits = utilGetNumberFromCFDict(parameters, kSecAttrKeySizeInBits, *keySizeInBits);
    *keyUsage =  utilGetMaskValFromCFDict(parameters, kSecAttrCanEncrypt, CSSM_KEYUSE_ENCRYPT) |
                utilGetMaskValFromCFDict(parameters, kSecAttrCanDecrypt, CSSM_KEYUSE_DECRYPT) |
                utilGetMaskValFromCFDict(parameters, kSecAttrCanWrap, CSSM_KEYUSE_WRAP) |
                utilGetMaskValFromCFDict(parameters, kSecAttrCanUnwrap, CSSM_KEYUSE_UNWRAP);


    if(*keyClass == CSSM_KEYCLASS_PRIVATE_KEY || *keyClass == CSSM_KEYCLASS_PUBLIC_KEY) {
		*keyUsage |=  utilGetMaskValFromCFDict(parameters, kSecAttrCanSign, CSSM_KEYUSE_SIGN) |
					utilGetMaskValFromCFDict(parameters, kSecAttrCanVerify, CSSM_KEYUSE_VERIFY);
    }

    if(*keyUsage == 0) {
		switch (*keyClass) {
			case CSSM_KEYCLASS_PRIVATE_KEY:
				*keyUsage = CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_UNWRAP | CSSM_KEYUSE_SIGN;
				break;
			case CSSM_KEYCLASS_PUBLIC_KEY:
				*keyUsage = CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_WRAP;
				break;
			default:
				*keyUsage = CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP | CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY;
				break;
		}
	}
}

static CFStringRef
utilCopyDefaultKeyLabel(void)
{
	// generate a default label from the current date
	CFDateRef dateNow = CFDateCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent());
	CFStringRef defaultLabel = CFCopyDescription(dateNow);
	CFRelease(dateNow);

	return defaultLabel;
}

SecKeyRef
SecKeyGenerateSymmetric(CFDictionaryRef parameters, CFErrorRef *error)
{
	OSStatus result = errSecParam; // default result for an early exit
	SecKeyRef key = NULL;
	SecKeychainRef keychain = NULL;
	SecAccessRef access;
	CFStringRef label;
	CFStringRef appLabel;
	CFStringRef appTag;
	CFStringRef dateLabel = NULL;

	CSSM_ALGORITHMS algorithm;
	uint32 keySizeInBits;
	CSSM_KEYUSE keyUsage;
	uint32 keyAttr = CSSM_KEYATTR_RETURN_DEFAULT;
	CSSM_KEYCLASS keyClass;
	CFTypeRef value;
	Boolean isPermanent;
	Boolean isExtractable;

	// verify keychain parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecUseKeychain, (const void **)&keychain))
		keychain = NULL;
	else if (SecKeychainGetTypeID() != CFGetTypeID(keychain)) {
		keychain = NULL;
		goto errorExit;
	}
	else
		CFRetain(keychain);

	// verify permanent parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrIsPermanent, (const void **)&value))
		isPermanent = false;
	else if (!value || (CFBooleanGetTypeID() != CFGetTypeID(value)))
		goto errorExit;
	else
		isPermanent = CFEqual(kCFBooleanTrue, value);
	if (isPermanent) {
		if (keychain == NULL) {
			// no keychain was specified, so use the default keychain
			result = SecKeychainCopyDefault(&keychain);
		}
		keyAttr |= CSSM_KEYATTR_PERMANENT;
	}

	// verify extractable parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrIsExtractable, (const void **)&value))
		isExtractable = true; // default to extractable if value not specified
	else if (!value || (CFBooleanGetTypeID() != CFGetTypeID(value)))
		goto errorExit;
	else
		isExtractable = CFEqual(kCFBooleanTrue, value);
	if (isExtractable)
		keyAttr |= CSSM_KEYATTR_EXTRACTABLE;

	// verify access parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrAccess, (const void **)&access))
		access = NULL;
	else if (SecAccessGetTypeID() != CFGetTypeID(access))
		goto errorExit;

	// verify label parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrLabel, (const void **)&label))
		label = (dateLabel = utilCopyDefaultKeyLabel()); // no label provided, so use default
	else if (CFStringGetTypeID() != CFGetTypeID(label))
		goto errorExit;

	// verify application label parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrApplicationLabel, (const void **)&appLabel))
		appLabel = (dateLabel) ? dateLabel : (dateLabel = utilCopyDefaultKeyLabel());
	else if (CFStringGetTypeID() != CFGetTypeID(appLabel))
		goto errorExit;

	// verify application tag parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrApplicationTag, (const void **)&appTag))
		appTag = NULL;
	else if (CFStringGetTypeID() != CFGetTypeID(appTag))
		goto errorExit;

    utilGetKeyParametersFromCFDict(parameters, &algorithm, &keySizeInBits, &keyUsage, &keyClass);

	if (!keychain) {
		// the generated key will not be stored in any keychain
		result = SecKeyGenerate(keychain, algorithm, keySizeInBits, 0, keyUsage, keyAttr, access, &key);
	}
	else {
		// we can set the label attributes on the generated key if it's a keychain item
		size_t labelBufLen = (label) ? (size_t)CFStringGetMaximumSizeForEncoding(CFStringGetLength(label), kCFStringEncodingUTF8) + 1 : 1;
		char *labelBuf = (char *)malloc(labelBufLen);
		size_t appLabelBufLen = (appLabel) ? (size_t)CFStringGetMaximumSizeForEncoding(CFStringGetLength(appLabel), kCFStringEncodingUTF8) + 1 : 1;
		char *appLabelBuf = (char *)malloc(appLabelBufLen);
		size_t appTagBufLen = (appTag) ? (size_t)CFStringGetMaximumSizeForEncoding(CFStringGetLength(appTag), kCFStringEncodingUTF8) + 1 : 1;
		char *appTagBuf = (char *)malloc(appTagBufLen);

		if (!label || !CFStringGetCString(label, labelBuf, labelBufLen-1, kCFStringEncodingUTF8))
			labelBuf[0]=0;
		if (!appLabel || !CFStringGetCString(appLabel, appLabelBuf, appLabelBufLen-1, kCFStringEncodingUTF8))
			appLabelBuf[0]=0;
		if (!appTag || !CFStringGetCString(appTag, appTagBuf, appTagBufLen-1, kCFStringEncodingUTF8))
			appTagBuf[0]=0;

		SecKeychainAttribute attrs[] = {
			{ kSecKeyPrintName, (UInt32)strlen(labelBuf), (char *)labelBuf },
			{ kSecKeyLabel, (UInt32)strlen(appLabelBuf), (char *)appLabelBuf },
			{ kSecKeyApplicationTag, (UInt32)strlen(appTagBuf), (char *)appTagBuf }	};
		SecKeychainAttributeList attributes = { sizeof(attrs) / sizeof(attrs[0]), attrs };
		if (!appTag) --attributes.count;

		result = SecKeyGenerateWithAttributes(&attributes,
			keychain, algorithm, keySizeInBits, 0,
			keyUsage, keyAttr, access, &key);

		free(labelBuf);
		free(appLabelBuf);
		free(appTagBuf);
	}

errorExit:
	if (result && error) {
		*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, result, NULL);
	}
	if (dateLabel)
		CFRelease(dateLabel);
	if (keychain)
		CFRelease(keychain);

    return key;
}



SecKeyRef
SecKeyCreateFromData(CFDictionaryRef parameters, CFDataRef keyData, CFErrorRef *error)
{
	CSSM_ALGORITHMS		algorithm;
    uint32				keySizeInBits;
    CSSM_KEYUSE			keyUsage;
    CSSM_KEYCLASS		keyClass;
    CSSM_RETURN			crtn;

    if(keyData == NULL || CFDataGetLength(keyData) == 0){
        MacOSError::throwMe(errSecUnsupportedKeySize);
    }

    utilGetKeyParametersFromCFDict(parameters, &algorithm, &keySizeInBits, &keyUsage, &keyClass);

	CSSM_CSP_HANDLE cspHandle = cuCspStartup(CSSM_FALSE); // TRUE => CSP, FALSE => CSPDL

	SecKeyImportExportParameters iparam;
	memset(&iparam, 0, sizeof(iparam));
	iparam.keyUsage = keyUsage;

    CFRef<CFDataRef> data;
	SecExternalItemType itype;
	switch (keyClass) {
		case CSSM_KEYCLASS_PRIVATE_KEY:
			itype = kSecItemTypePrivateKey;
			break;
        case CSSM_KEYCLASS_PUBLIC_KEY: {
			itype = kSecItemTypePublicKey;
            // Public key import expects public key in SubjPublicKey X509 format.  We want to accept both bare and x509 format,
            // so we have to detect bare format here and extend to full X509 if detected.
            data.take(SecCDSAKeyCopyPublicKeyDataWithSubjectInfo(algorithm, keySizeInBits, keyData));
            break;
        }
		case CSSM_KEYCLASS_SESSION_KEY:
			itype = kSecItemTypeSessionKey;
			break;
		default:
			itype = kSecItemTypeUnknown;
			break;
	}

	CFMutableArrayRef ka = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	// NOTE: if we had a way to specify values other then kSecFormatUnknown we might be more useful.
    crtn = impExpImportRawKey(data ? CFDataRef(data) : keyData, kSecFormatUnknown, itype, algorithm, NULL, cspHandle, 0, NULL, NULL, ka);
	if (crtn == CSSM_OK && CFArrayGetCount((CFArrayRef)ka)) {
		SecKeyRef sk = (SecKeyRef)CFArrayGetValueAtIndex((CFArrayRef)ka, 0);
		CFRetain(sk);
		CFRelease(ka);
		return sk;
	} else {
        CFRelease(ka);
		if (error) {
			*error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, crtn ? crtn : CSSM_ERRCODE_INTERNAL_ERROR, NULL);
		}
		return NULL;
	}
}


void
SecKeyGeneratePairAsync(CFDictionaryRef parametersWhichMightBeMutiable, dispatch_queue_t deliveryQueue,
						SecKeyGeneratePairBlock result)
{
	CFDictionaryRef parameters = CFDictionaryCreateCopy(NULL, parametersWhichMightBeMutiable);
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		SecKeyRef publicKey = NULL;
		SecKeyRef privateKey = NULL;
		OSStatus status = SecKeyGeneratePair(parameters, &publicKey, &privateKey);
		dispatch_async(deliveryQueue, ^{
			CFErrorRef error = NULL;
			if (errSecSuccess != status) {
				error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, status, NULL);
			}
			result(publicKey, privateKey, error);
			if (error) {
				CFRelease(error);
			}
			if (publicKey) {
				CFRelease(publicKey);
			}
			if (privateKey) {
				CFRelease(privateKey);
			}
			CFRelease(parameters);
		});
	});
}

static inline void utilClearAndFree(void *p, size_t len) {
    if(p) {
        if(len) bzero(p, len);
        free(p);
    }
}

SecKeyRef
SecKeyDeriveFromPassword(CFStringRef password, CFDictionaryRef parameters, CFErrorRef *error)
{
    CCPBKDFAlgorithm algorithm;
    CFIndex passwordLen = 0;
    CFDataRef keyData = NULL;
    char *thePassword = NULL;
    uint8_t *salt = NULL;
    uint8_t *derivedKey = NULL;
    size_t  saltLen = 0, derivedKeyLen = 0;
    uint rounds;
    CFDataRef saltDictValue, algorithmDictValue;
    SecKeyRef retval = NULL;

    /* Pick Values from parameters */

    if((saltDictValue = (CFDataRef) CFDictionaryGetValue(parameters, kSecAttrSalt)) == NULL) {
        if(error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecMissingAlgorithmParms, NULL);
        }
        goto errOut;
    }

    derivedKeyLen = utilGetNumberFromCFDict(parameters, kSecAttrKeySizeInBits, 128);
	// This value come in bits but the rest of the code treats it as bytes
	derivedKeyLen /= 8;

    algorithmDictValue = (CFDataRef) utilGetStringFromCFDict(parameters, kSecAttrPRF, kSecAttrPRFHmacAlgSHA256);

    rounds = utilGetNumberFromCFDict(parameters, kSecAttrRounds, 0);

    /* Convert any remaining parameters and get the password bytes */

    saltLen = CFDataGetLength(saltDictValue);
    if((salt = (uint8_t *) malloc(saltLen)) == NULL) {
        if(error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecAllocate, NULL);
        }
        goto errOut;
    }

    CFDataGetBytes(saltDictValue, CFRangeMake(0, saltLen), (UInt8 *) salt);

    passwordLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(password), kCFStringEncodingUTF8) + 1;
    if((thePassword = (char *) malloc(passwordLen)) == NULL) {
        if(error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecAllocate, NULL);
        }
        goto errOut;
    }
    CFStringGetBytes(password, CFRangeMake(0, CFStringGetLength(password)), kCFStringEncodingUTF8, '?', FALSE, (UInt8*)thePassword, passwordLen, &passwordLen);

    if((derivedKey = (uint8_t *) malloc(derivedKeyLen)) == NULL) {
        if(error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecAllocate, NULL);
        }
        goto errOut;
    }

    if(algorithmDictValue == NULL) {
        algorithm = kCCPRFHmacAlgSHA1; /* default */
    } else if(CFEqual(algorithmDictValue, kSecAttrPRFHmacAlgSHA1)) {
        algorithm = kCCPRFHmacAlgSHA1;
    } else if(CFEqual(algorithmDictValue, kSecAttrPRFHmacAlgSHA224)) {
        algorithm = kCCPRFHmacAlgSHA224;
    } else if(CFEqual(algorithmDictValue, kSecAttrPRFHmacAlgSHA256)) {
        algorithm = kCCPRFHmacAlgSHA256;
    } else if(CFEqual(algorithmDictValue, kSecAttrPRFHmacAlgSHA384)) {
        algorithm = kCCPRFHmacAlgSHA384;
    } else if(CFEqual(algorithmDictValue, kSecAttrPRFHmacAlgSHA512)) {
        algorithm = kCCPRFHmacAlgSHA512;
    } else {
        if(error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidAlgorithmParms, NULL);
        }
        goto errOut;
    }

    if(rounds == 0) {
        rounds = 33333; // we need to pass back a consistent value since there's no way to record the round count.
    }

    if(CCKeyDerivationPBKDF(kCCPBKDF2, thePassword, passwordLen, salt, saltLen, algorithm, rounds, derivedKey, derivedKeyLen)) {
        if(error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInternalError, NULL);
        }
        goto errOut;
    }

    if((keyData = CFDataCreate(NULL, derivedKey, derivedKeyLen)) != NULL) {
        retval =  SecKeyCreateFromData(parameters, keyData, error);
        CFRelease(keyData);
    } else {
        if(error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInternalError, NULL);
        }
    }

errOut:
    utilClearAndFree(salt, saltLen);
    utilClearAndFree(thePassword, passwordLen);
    utilClearAndFree(derivedKey, derivedKeyLen);
    return retval;
}

CFDataRef
SecKeyWrapSymmetric(SecKeyRef keyToWrap, SecKeyRef wrappingKey, CFDictionaryRef parameters, CFErrorRef *error)
{
    if(error) {
        *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecUnimplemented, NULL);
    }
    return NULL;
}

SecKeyRef
SecKeyUnwrapSymmetric(CFDataRef *keyToUnwrap, SecKeyRef unwrappingKey, CFDictionaryRef parameters, CFErrorRef *error)
{
    if(error) {
        *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecUnimplemented, NULL);
    }
    return NULL;
}
