/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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


#include "SecOTR.h"
#include "SecOTRIdentityPriv.h"
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFData.h>

#include <Security/SecItem.h>
#include <Security/SecKeyPriv.h>

#include <Security/oidsalg.h>
#include <Security/SecCertificatePriv.h>

#include "SecOTRErrors.h"

#include <TargetConditionals.h>

//
// Algorthim ID initialization
//

#define kMessageIdentityRSAKeyBits 1280
#define kMessageIdentityECKeyBits 256

void EnsureOTRAlgIDInited(void)
{
    static dispatch_once_t kSignatureAlgID_ONCE;
    static SecAsn1AlgId kOTRECSignatureAlgID;

    dispatch_once(&kSignatureAlgID_ONCE, ^{
        kOTRECSignatureAlgID.algorithm = CSSMOID_ECDSA_WithSHA1;
        kOTRSignatureAlgIDPtr = &kOTRECSignatureAlgID;
    });
}


static CFStringRef sSigningKeyName = CFSTR("OTR Signing Key");

//
// SecOTRFullIdentity implementation
//

CFGiblisFor(SecOTRFullIdentity);

static CF_RETURNS_RETAINED CFStringRef SecOTRFullIdentityCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecOTRFullIdentityRef requestor = (SecOTRFullIdentityRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault,NULL,CFSTR("<SecOTRPublicIdentity: %p %02x%02x%02x%02x%02x%02x%02x%02x>"),
                                    requestor,
                                    requestor->publicIDHash[0], requestor->publicIDHash[1],
                                    requestor->publicIDHash[2], requestor->publicIDHash[3],
                                    requestor->publicIDHash[4], requestor->publicIDHash[5],
                                    requestor->publicIDHash[6], requestor->publicIDHash[7]);
}

static void SecOTRFullIdentityDestroy(CFTypeRef cf) {
    SecOTRFullIdentityRef requestor = (SecOTRFullIdentityRef)cf;

    CFReleaseNull(requestor->privateSigningKey);
    CFReleaseNull(requestor->publicSigningKey);
}


//
// Shared statics
//

static OSStatus SecOTRFIPurgeFromKeychainByValue(SecKeyRef key, CFStringRef label)
{
    OSStatus status;
    const void *keys[] =   { kSecClass,
        kSecAttrLabel,
        kSecValueRef,
    };
    const void *values[] = { kSecClassKey,
        label,
        key,
    };
    CFDictionaryRef dict = CFDictionaryCreate(NULL, keys, values, array_size(values), NULL, NULL);
    status = SecItemDelete(dict);
    CFReleaseSafe(dict);

    return status;
}

static bool SecKeyDigestAndSignWithError(
                                  SecKeyRef           key,            /* Private key */
                                  const SecAsn1AlgId  *algId,         /* algorithm oid/params */
                                  const uint8_t       *dataToDigest,	/* signature over this data */
                                  size_t              dataToDigestLen,/* length of dataToDigest */
                                  uint8_t             *sig,			/* signature, RETURNED */
                                  size_t              *sigLen,		/* IN/OUT */
                                  CFErrorRef          *error) {

    OSStatus status = SecKeyDigestAndSign(key, algId, dataToDigest, dataToDigestLen, sig, sigLen);
    require_noerr(status, fail);
    return true;
fail:
    SecOTRCreateError(secOTRErrorOSError, status, CFSTR("Error signing message. OSStatus in error code."), NULL, error);
    return false;
}

//
// SecOTRFullIdentity Functions
//

static bool SecOTRFICachePublicHash(SecOTRFullIdentityRef fullID, CFErrorRef *error)
{
    SecOTRPublicIdentityRef pubID = SecOTRPublicIdentityCopyFromPrivate(NULL, fullID, error);

    require(pubID, fail);

    SecOTRPICopyHash(pubID, fullID->publicIDHash);

fail:
    CFReleaseSafe(pubID);
    return (pubID != NULL); // This is safe because we're not accessing the value after release, just checking if it ever had a value of some nature.
}

#if !TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
#define SEC_CONST_DECL(k,v) CFTypeRef k = (CFTypeRef)(CFSTR(v));
SEC_CONST_DECL (kSecAttrAccessible, "pdmn");
SEC_CONST_DECL (kSecAttrAccessibleAlwaysThisDeviceOnly, "dku");
#endif

SecOTRFullIdentityRef SecOTRFullIdentityCreate(CFAllocatorRef allocator, CFErrorRef *error)
{
    CFDictionaryRef keygen_parameters = NULL;
    SecOTRFullIdentityRef newID = CFTypeAllocate(SecOTRFullIdentity, struct _SecOTRFullIdentity, allocator);
    SecKeyRef tempSigningKey = NULL;

    newID->publicSigningKey = NULL;
    newID->privateSigningKey = NULL;

    require(newID, out);

    EnsureOTRAlgIDInited();

    const int signing_keySizeLocal = kMessageIdentityECKeyBits;
    CFNumberRef signing_bitsize = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &signing_keySizeLocal);

    const void *signing_keygen_keys[] = { kSecAttrKeyType,
                                          kSecAttrKeySizeInBits,
                                          kSecAttrIsPermanent,
                                          kSecAttrAccessible,
                                          kSecAttrLabel,
                                        };

    const void *signing_keygen_vals[] = { kSecAttrKeyTypeEC,
                                          signing_bitsize,
                                          kCFBooleanTrue,
                                          kSecAttrAccessibleAlwaysThisDeviceOnly,
                                          sSigningKeyName
                                        };
    keygen_parameters = CFDictionaryCreate(kCFAllocatorDefault,
                                           signing_keygen_keys, signing_keygen_vals, array_size(signing_keygen_vals),
                                           &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
    CFReleaseNull(signing_bitsize);
    require_noerr(SecKeyGeneratePair(keygen_parameters, &tempSigningKey, &newID->privateSigningKey), out);
    CFReleaseNull(keygen_parameters);

    newID->publicSigningKey = SecKeyCreatePublicFromPrivate(tempSigningKey);

    (void) SecOTRFIPurgeFromKeychainByValue(tempSigningKey, sSigningKeyName);
    CFReleaseNull(tempSigningKey);

    require(SecOTRFICachePublicHash(newID, error), out);

    return newID;

out:
    if (NULL != newID) {
        SecOTRFIPurgeFromKeychain(newID, NULL);
    }
    CFReleaseSafe(keygen_parameters);
    CFReleaseSafe(newID);
    CFReleaseSafe(tempSigningKey);
    return NULL;
}


static
OSStatus SecOTRFICreatePrivateKeyReadPersistentRef(const uint8_t **bytes, size_t *size, SecKeyRef* privateKey)
{
    OSStatus status = errSecParam;
    uint16_t dataSize;
    CFDataRef persistentRef = NULL;

    require_noerr_quiet(readSize(bytes, size, &dataSize), fail);
    require_quiet(dataSize <= *size, fail);

    SecKeyRef lookedUpKey = NULL;
    persistentRef = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, *bytes, dataSize, kCFAllocatorNull);
    require_quiet(persistentRef, fail);

    require_noerr_quiet(SecKeyFindWithPersistentRef(persistentRef, &lookedUpKey), fail);

    *privateKey = lookedUpKey;

    *bytes += dataSize;
    *size -= dataSize;

    status = errSecSuccess;

fail:
    CFReleaseSafe(persistentRef);

    return status;
}

static
OSStatus SecOTRFICreateKeysFromReadPersistentRef(const uint8_t **bytes, size_t *size, SecKeyRef *publicKey, SecKeyRef* privateKey)
{
    SecKeyRef foundKey = NULL;

    OSStatus status = SecOTRFICreatePrivateKeyReadPersistentRef(bytes, size, &foundKey);
    require_noerr_quiet(status, fail);
    require_quiet(foundKey, fail);

    *publicKey = SecKeyCreatePublicFromPrivate(*privateKey);
    require_action_quiet(*publicKey, fail, status = errSecParam);

    *privateKey = foundKey;
    foundKey = NULL;

    status = errSecSuccess;

fail:
    CFReleaseSafe(foundKey);
    return status;
}

typedef SecKeyRef (*SecOTRPublicKeyCreateFunction)(CFAllocatorRef allocator, const uint8_t** data, size_t* limit);

static
OSStatus SecOTRFICreateKeysFromReadPersistentRefAndPublicKey(const uint8_t **bytes, size_t *size, SecKeyRef *publicKey, SecKeyRef* privateKey, SecOTRPublicKeyCreateFunction createPublic)
{
    SecKeyRef foundKey = NULL;

    OSStatus status = SecOTRFICreatePrivateKeyReadPersistentRef(bytes, size, &foundKey);
    require_noerr_quiet(status, fail);
    require_quiet(foundKey, fail);

    *publicKey = (*createPublic)(NULL, bytes, size);
    require_action_quiet(*publicKey, fail, status = errSecParam);

    *privateKey = foundKey;

fail:
    return status;
}

static
OSStatus SecOTRFIInitFromV1Bytes(SecOTRFullIdentityRef newID, CFAllocatorRef allocator,
                                const uint8_t **bytes,size_t *size) {
    require(**bytes == 1, fail);
    ++*bytes;
    --*size;

    require_noerr_quiet(SecOTRFICreateKeysFromReadPersistentRef(bytes, size, &newID->publicSigningKey, &newID->privateSigningKey), fail);

    return errSecSuccess;

fail:
    CFReleaseNull(newID->publicSigningKey);
    CFReleaseNull(newID->privateSigningKey);

    return errSecParam;
}

static
OSStatus SecOTRFIInitFromV2Bytes(SecOTRFullIdentityRef newID, CFAllocatorRef allocator,
                                const uint8_t **bytes,size_t *size) {
    require(**bytes == 2, fail);
    ++*bytes;
    --*size;

    require_noerr_quiet(SecOTRFICreateKeysFromReadPersistentRefAndPublicKey(bytes, size, &newID->publicSigningKey, &newID->privateSigningKey, &CreateECPublicKeyFrom), fail);

    return errSecSuccess;

fail:
    CFReleaseNull(newID->publicSigningKey);
    CFReleaseNull(newID->privateSigningKey);

    return errSecParam;
}

SecOTRFullIdentityRef SecOTRFullIdentityCreateFromSecKeyRef(CFAllocatorRef allocator, SecKeyRef privateKey,
                                                            CFErrorRef *error) {
    // TODO - make sure this is an appropriate key type
    SecOTRFullIdentityRef newID = CFTypeAllocate(SecOTRFullIdentity, struct _SecOTRFullIdentity, allocator);
    newID->privateSigningKey = privateKey;
    CFRetain(newID->privateSigningKey);
    newID->publicSigningKey = SecKeyCreatePublicFromPrivate(privateKey);
    require(SecOTRFICachePublicHash(newID, error), fail);
    return newID;
fail:
    CFRelease(newID->privateSigningKey);
    CFRelease(newID->publicSigningKey);
    CFReleaseSafe(newID);
    return NULL;
}

SecOTRFullIdentityRef SecOTRFullIdentityCreateFromBytes(CFAllocatorRef allocator, const uint8_t**bytes, size_t *size, CFErrorRef *error)
{
    SecOTRFullIdentityRef newID = CFTypeAllocate(SecOTRFullIdentity, struct _SecOTRFullIdentity, allocator);
    EnsureOTRAlgIDInited();

    require(*size > 0, fail);

    switch (**bytes) {
        case 1:
            require_noerr_quiet(SecOTRFIInitFromV1Bytes(newID, allocator, bytes, size), fail);
            break;
        case 2:
            require_noerr_quiet(SecOTRFIInitFromV2Bytes(newID, allocator, bytes, size), fail);
            break;
        case 0: // Version 0 was used in seeds of 5.0, transition from those seeds unsupported - keys were in exported data.
        default:
            require(false, fail);
            break;
    }

    require(SecOTRFICachePublicHash(newID, error), fail);

    return newID;

fail:
    if (NULL != newID) {
        SecOTRFIPurgeFromKeychain(newID, NULL);
    }
    CFReleaseSafe(newID);
    return NULL;
}

SecOTRFullIdentityRef SecOTRFullIdentityCreateFromData(CFAllocatorRef allocator, CFDataRef data, CFErrorRef *error)
{
    if (data == NULL)
        return NULL;

    size_t size = (size_t)CFDataGetLength(data);
    const uint8_t* bytes = CFDataGetBytePtr(data);

    return SecOTRFullIdentityCreateFromBytes(allocator, &bytes, &size, error);
}

bool SecOTRFIPurgeFromKeychain(SecOTRFullIdentityRef thisID, CFErrorRef *error)
{
    OSStatus result = SecOTRFIPurgeFromKeychainByValue(thisID->privateSigningKey, sSigningKeyName);
    if (errSecSuccess == result) {
        return true;
    } else {
        SecOTRCreateError(secOTRErrorOSError, result, CFSTR("OSStatus returned in error code"), NULL, error);
        return false;
    }
}


static OSStatus SecOTRFIPurgeAllFromKeychainByLabel(CFStringRef label)
{
    OSStatus status;
    const void *keys[] =   { kSecClass,
        kSecAttrKeyClass,
        kSecAttrLabel,
    };
    const void *values[] = { kSecClassKey,
        kSecAttrKeyClassPrivate,
        label,
    };
    CFDictionaryRef dict = CFDictionaryCreate(NULL, keys, values, array_size(values), NULL, NULL);
    bool deleteAtLeastOne = false;
    int loopLimiter = 500;
    do {
        status = SecItemDelete(dict);
        if (status == errSecSuccess) {
            deleteAtLeastOne = true;
        }
        loopLimiter--;
    } while ((status == errSecSuccess) && (loopLimiter > 0));
    if ((status == errSecItemNotFound) && (deleteAtLeastOne)) {
        // We've looped until we can't delete any more keys.
        // Since this will produce an expected 'itemNotFound', but we don't want to break the contract above
        // (and also we want to make sense)
        // we muffle the non-error to a success case, which it is.
        status = errSecSuccess;
    }
    CFReleaseSafe(dict);

    return status;
}

bool SecOTRFIPurgeAllFromKeychain(CFErrorRef *error)
{
    OSStatus result = SecOTRFIPurgeAllFromKeychainByLabel(sSigningKeyName);
    if (errSecSuccess == result) {
        return true;
    } else {
        SecOTRCreateError(secOTRErrorOSError, result, CFSTR("OSStatus returned in error code"), NULL, error);
        return false;
    }
}

static OSStatus appendPersistentRefData(SecKeyRef theKey, CFMutableDataRef serializeInto, CFStringRef name)
{
    OSStatus status;
    CFDataRef persistent_ref = NULL;
    require_noerr(status = SecKeyCopyPersistentRef(theKey, &persistent_ref), fail);

    status = appendSizeAndData(persistent_ref, serializeInto);

fail:
    CFReleaseSafe(persistent_ref);

    return status;
}

static OSStatus SecOTRFIAppendV2Serialization(SecOTRFullIdentityRef fullID, CFMutableDataRef serializeInto)
{
    const uint8_t version = 2;
    CFIndex start = CFDataGetLength(serializeInto);

    CFDataAppendBytes(serializeInto, &version, sizeof(version));

    require(errSecSuccess == appendPersistentRefData(fullID->privateSigningKey, serializeInto, sSigningKeyName), fail);
    require(errSecSuccess == appendPublicOctetsAndSize(fullID->publicSigningKey, serializeInto), fail);
    return errSecSuccess;

fail:
    CFDataSetLength(serializeInto, start);

    return errSecParam;
}


bool SecOTRFIAppendSerialization(SecOTRFullIdentityRef fullID, CFMutableDataRef serializeInto, CFErrorRef *error)
{
    OSStatus status = SecOTRFIAppendV2Serialization(fullID, serializeInto);
    if (errSecSuccess == status) {
        return true;
    } else {
        SecOTRCreateError(secOTRErrorOSError, status, CFSTR("OSStatus returned in error code"), NULL, error);
        return false;
    }
}

size_t SecOTRFISignatureSize(SecOTRFullIdentityRef fullID)
{
    return SecKeyGetSize(fullID->publicSigningKey, kSecKeySignatureSize);
}

bool SecOTRFIAppendSignature(SecOTRFullIdentityRef fullID,
                                CFDataRef dataToHash,
                                CFMutableDataRef appendTo,
                                CFErrorRef *error)
{
    const size_t signatureSize = SecOTRFISignatureSize(fullID);
    const CFIndex sourceLength = CFDataGetLength(dataToHash);
    const uint8_t* sourceData = CFDataGetBytePtr(dataToHash);

    CFIndex start = CFDataGetLength(appendTo);

    require(((CFIndex)signatureSize) >= 0, fail);

    CFDataIncreaseLength(appendTo, (CFIndex)signatureSize + 1);

    uint8_t *size = CFDataGetMutableBytePtr(appendTo) + start;
    uint8_t* signatureStart = size + 1;
    size_t signatureUsed = signatureSize;

   require(SecKeyDigestAndSignWithError(fullID->privateSigningKey, kOTRSignatureAlgIDPtr,
                                    sourceData, (size_t)sourceLength,
                                    signatureStart, &signatureUsed, error), fail);

    require(signatureUsed < 256, fail);
    require(((CFIndex)signatureUsed) >= 0, fail);
    *size = signatureUsed;

    CFDataSetLength(appendTo, start + (CFIndex)signatureUsed + 1);

    return true;

fail:
    CFDataSetLength(appendTo, start);

    return false;
}

void SecOTRFIAppendPublicHash(SecOTRFullIdentityRef fullID, CFMutableDataRef appendTo)
{
    CFDataAppendBytes(appendTo, fullID->publicIDHash, sizeof(fullID->publicIDHash));
}

bool SecOTRFIComparePublicHash(SecOTRFullIdentityRef fullID, const uint8_t hash[kMPIDHashSize])
{
    return 0 == memcmp(hash, fullID->publicIDHash, kMPIDHashSize);
}
