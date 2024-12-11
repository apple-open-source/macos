/*
 * Copyright (c) 2007-2014,2023-2024 Apple Inc. All Rights Reserved.
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

#include <libDER/libDER.h>
#include <libDER/asn1Types.h>
#include <libDER/DER_Decode.h>
#include <libDER/DER_Encode.h>
#include <libDER/oids.h>

#include <Security/SecBase.h>
#include <Security/SecBasePriv.h>
#include <Security/SecItem.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecInternal.h>
#include "debugging.h"
#include "utilities/SecCFWrappers.h"

//#include <AssertMacros.h>
#include <CommonCrypto/CommonDigest.h>

//#include "p12import.h"
#include <Security/SecImportExportPriv.h>

#include <CoreFoundation/CFPriv.h>

const CFStringRef __nonnull kSecImportExportPassphrase = CFSTR("passphrase");
const CFStringRef __nonnull kSecImportExportKeychain = CFSTR("keychain");
const CFStringRef __nonnull kSecImportExportAccess = CFSTR("access");
const CFStringRef __nonnull kSecImportToMemoryOnly = CFSTR("memory");

const CFStringRef __nonnull kSecImportItemLabel = CFSTR("label");
const CFStringRef __nonnull kSecImportItemKeyID = CFSTR("keyid");
const CFStringRef __nonnull kSecImportItemTrust = CFSTR("trust");
const CFStringRef __nonnull kSecImportItemCertChain = CFSTR("chain");
const CFStringRef __nonnull kSecImportItemIdentity = CFSTR("identity");

static OSStatus importPkcs12CertChainToLegacyKeychain(CFDictionaryRef item, SecKeychainRef importKeychain)
{
    OSStatus status = errSecSuccess;
    // go through certificate chain and all certificates
    CFArrayRef certChain = (CFArrayRef)CFDictionaryGetValue(item, kSecImportItemCertChain);
    if (!certChain || CFGetTypeID(certChain) != CFArrayGetTypeID()) {
        return errSecInternal; // Should never happen since SecPKCS12Import_ios make the item dictionary
    }
    for (unsigned index=0; index<CFArrayGetCount(certChain); index++) {
        SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(certChain, index);
        CFMutableDictionaryRef query = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                                 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(query, kSecClass, kSecClassCertificate);
        CFDictionaryAddValue(query, kSecValueRef, cert);
        if (importKeychain) { CFDictionaryAddValue(query, kSecUseKeychain, importKeychain); }
        OSStatus status = SecItemAdd(query, NULL);
        switch(status) {
            case errSecSuccess:
                secnotice("p12Decode", "cert added to keychain");
                break;
            case errSecDuplicateItem:    // dup cert, OK to skip
                secnotice("p12Decode", "skipping dup cert");
                break;
            default: //all other errors
                secerror("p12Decode: Error %d adding identity to keychain", status);
        }
        CFReleaseNull(query);
    }
    return status;
}

/*
* ECPrivateKey ::= SEQUENCE {
*  version INTEGER { ecPrivkeyVer1(1) } (ecPrivkeyVer1),
*  privateKey OCTET STRING,
*  parameters [0] ECDomainParameters {{ SECGCurveNames }} OPTIONAL,
*  publicKey [1] BIT STRING OPTIONAL
* } */
typedef struct {
    DERItem        version;
    DERItem        privateKey;
    DERItem        parameters;
    DERItem        publicKey;
} DER_ECPrivateKey;

const DERItemSpec DER_ECPrivateKeyItemSpecs[] =
{
    { DER_OFFSET(DER_ECPrivateKey, version),
        ASN1_INTEGER,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DER_ECPrivateKey, privateKey),
        ASN1_OCTET_STRING,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DER_ECPrivateKey, parameters),
        ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0,
        DER_DEC_OPTIONAL  },
    { DER_OFFSET(DER_ECPrivateKey, publicKey),
        ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 1,
        DER_DEC_OPTIONAL }
};
const DERSize DERNumECPrivateKeyItemSpecs = sizeof(DER_ECPrivateKeyItemSpecs) / sizeof(DERItemSpec);

static const DERByte encodedAlgIdECsecp256[] = {
    0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
};
static const DERByte encodedAlgIdECsecp384[] = {
    0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22,
};
static const DERByte encodedAlgIdECsecp521[] = {
    0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x23,
};

static CF_RETURNS_RETAINED CFDataRef encodeECPrivateKey(SecKeyRef key, CFErrorRef *error) {
    CFMutableDataRef result = NULL;
    CFMutableDataRef K = NULL;

    /* SecKeyCopyExternalRepresentation returns 04 | X | Y | K for an EC private key
     * but we need just K as the private key for the ASN1 specs.
     * So we strip 04 | X | Y by requesting the public key external representation
     * ( 04 | X | Y ) and deleting that length off the private key external representation. */
    SecKeyRef pubKey = SecKeyCopyPublicKey(key);
    CFDataRef pubKeyData = SecKeyCopyExternalRepresentation(pubKey, error);
    CFDataRef privKeyData = SecKeyCopyExternalRepresentation(key, error);
    require(pubKeyData && privKeyData, errOut);
    K = CFDataCreateMutableCopy(NULL, 0, privKeyData);
    CFDataDeleteBytes(K, CFRangeMake(0, CFDataGetLength(pubKeyData)));

    DER_ECPrivateKey ecPrivKey;
    memset(&ecPrivKey, 0, sizeof(ecPrivKey));
    uint8_t version = 1;
    ecPrivKey.version.data = &version;
    ecPrivKey.version.length = 1;
    ecPrivKey.privateKey.data = (DERByte *)CFDataGetBytePtr(K);
    ecPrivKey.privateKey.length = (size_t)CFDataGetLength(K);
    SecECNamedCurve curve = SecECKeyGetNamedCurve(key);
    switch(curve) {
        case kSecECCurveSecp256r1:
            ecPrivKey.parameters.data = (DERByte *)encodedAlgIdECsecp256;
            ecPrivKey.parameters.length = sizeof(encodedAlgIdECsecp256);
            break;
        case kSecECCurveSecp384r1:
            ecPrivKey.parameters.data = (DERByte *)encodedAlgIdECsecp384;
            ecPrivKey.parameters.length = sizeof(encodedAlgIdECsecp384);
            break;
        case kSecECCurveSecp521r1:
            ecPrivKey.parameters.data = (DERByte *)encodedAlgIdECsecp521;
            ecPrivKey.parameters.length = sizeof(encodedAlgIdECsecp521);
            break;
        default:
            goto errOut;
    }

    size_t keyLen = 0;
    require_noerr(DERLengthOfEncodedSequenceFromObject(ASN1_CONSTR_SEQUENCE, &ecPrivKey, sizeof(ecPrivKey), (DERShort)DERNumECPrivateKeyItemSpecs, DER_ECPrivateKeyItemSpecs, &keyLen), errOut);
    require(keyLen < LONG_MAX, errOut);
    result = CFDataCreateMutable(NULL, (CFIndex)keyLen);
    require(result, errOut);
    CFDataSetLength(result, (CFIndex)keyLen);
    require_noerr_action(DEREncodeSequenceFromObject(ASN1_CONSTR_SEQUENCE, &ecPrivKey, sizeof(ecPrivKey),
                                                        (DERShort)DERNumECPrivateKeyItemSpecs,  DER_ECPrivateKeyItemSpecs,
                                                     CFDataGetMutableBytePtr(result), (size_t)CFDataGetLength(result), &keyLen),
                         errOut, CFReleaseNull(result));

errOut:
    CFReleaseNull(K);
    CFReleaseNull(privKeyData);
    CFReleaseNull(pubKeyData);
    CFReleaseNull(pubKey);
    return result;
}

static CF_RETURNS_RETAINED CFDataRef SecKeyCopyLegacyKeychainCompatibleExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    CFDictionaryRef keyAttrs = SecKeyCopyAttributes(key);
    CFDataRef result = NULL;
    CFStringRef type = CFDictionaryGetValue(keyAttrs, kSecAttrKeyType);
    if (CFEqualSafe(type, kSecAttrKeyTypeRSA)) {
        result = SecKeyCopyExternalRepresentation(key, error);
    } else if (CFEqualSafe(type, kSecAttrKeyTypeECSECPrimeRandom)) {
        result = encodeECPrivateKey(key, error);
    }

    CFReleaseNull(keyAttrs);
    return result;
}

static _Nullable SecKeyRef copyPrivateKeyForPublicKeyDigest(CFDataRef keyID, SecKeychainRef importKeychain)
{
    SecKeyRef privateKey = NULL;
    CFTypeRef result = NULL;
    if (keyID == NULL) { return NULL; }

    CFMutableDictionaryRef pkquery = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(pkquery, kSecClass, kSecClassKey);
    CFDictionaryAddValue(pkquery, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
    CFDictionaryAddValue(pkquery, kSecAttrApplicationLabel, keyID);
    if (importKeychain) {
        CFMutableArrayRef searchlist = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        CFArrayAppendValue(searchlist, importKeychain);
        CFDictionaryAddValue(pkquery, kSecMatchSearchList, searchlist);
        CFReleaseNull(searchlist);
    }
    CFDictionaryAddValue(pkquery, kSecReturnRef, kCFBooleanTrue);
    OSStatus status = SecItemCopyMatching(pkquery, &result);
    if (status == errSecSuccess && CFGetTypeID(result) == SecKeyGetTypeID()) {
        CFAssignRetained(privateKey, (SecKeyRef)result);
        result = NULL;
    }
    CFReleaseNull(result);
    CFReleaseNull(pkquery);
    return privateKey;
}

static OSStatus importPkcs12KeyToLegacyKeychain(SecIdentityRef identity, SecKeychainRef importKeychain, SecAccessRef importAccess, SecKeyRef * CF_RETURNS_RETAINED outKey)
{
    SecKeyRef privateKey = NULL;
    CFErrorRef error = NULL;
    CFDataRef keyID = NULL;
    CFDataRef keyData = NULL;
    CFStringRef keyType = NULL;

    OSStatus status = SecIdentityCopyPrivateKey(identity, &privateKey);
    require_noerr(status, errOut);

    // export the iOS-style key and re-import with legacy access control
    keyID = SecKeyCopyPublicKeyHash(privateKey);
    keyData = SecKeyCopyLegacyKeychainCompatibleExternalRepresentation(privateKey, &error);
    require_action(error == NULL, errOut, status = (OSStatus)CFErrorGetCode(error););

    // before we attempt to import this key, has it already been imported to this keychain?
    SecKeyRef existingKey = copyPrivateKeyForPublicKeyDigest(keyID, importKeychain);
    if (existingKey) {
        CFReleaseNull(privateKey);
        CFAssignRetained(privateKey, existingKey);
        goto errOut;
    }

    SecExternalFormat inputFormat = kSecFormatOpenSSL;
    SecExternalItemType itemType = kSecItemTypePrivateKey;
    SecItemImportExportFlags flags = 0;
    SecKeyImportExportParameters keyParams; /* filled in below... */
    memset(&keyParams, 0, sizeof(SecKeyImportExportParameters));
    keyParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
    keyParams.accessRef = importAccess;
    CFArrayRef impItems = NULL;
    status = SecKeychainItemImport(keyData, NULL, &inputFormat, &itemType, flags,
                                   &keyParams, importKeychain, &impItems);
    // try to replace iOS-style memory-based private key with CDSA keychain-based key
    if (status == errSecSuccess && impItems != NULL) {
        // we can get the key directly from the output items array
        SecKeyRef impKeyRef = (SecKeyRef)CFArrayGetValueAtIndex(impItems, 0);
        if (CFGetTypeID(impKeyRef) == SecKeyGetTypeID()) {
            CFRetainAssign(privateKey, impKeyRef);
        }
    } else if (status == errSecDuplicateItem && keyID != NULL) {
        // we can look up the private key given the digest of its public key
        SecKeyRef foundKey = copyPrivateKeyForPublicKeyDigest(keyID, importKeychain);
        if (foundKey) {
            CFAssignRetained(privateKey, foundKey);
        }
    }
    CFReleaseNull(impItems);

errOut:
    if (outKey) {
        *outKey = CFRetainSafe(privateKey);
    }
    CFReleaseNull(privateKey);
    CFReleaseNull(keyID);
    CFReleaseNull(keyData);
    CFReleaseNull(error);
    CFReleaseNull(keyType);
    return status;
}

static OSStatus importPkcs12CertToLegacyKeychain(SecIdentityRef identity, SecKeychainRef importKeychain, SecCertificateRef * CF_RETURNS_RETAINED outCert)
{
    SecCertificateRef certificate = NULL;
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                                             0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    OSStatus status = SecIdentityCopyCertificate(identity, &certificate);
    if (status == errSecSuccess && certificate != NULL) {
        CFDictionaryAddValue(query, kSecClass, kSecClassCertificate);
        CFDictionaryAddValue(query, kSecValueRef, certificate);
        if (importKeychain) { CFDictionaryAddValue(query, kSecUseKeychain, importKeychain); }
        status = SecItemAdd(query, NULL);
    }
    switch(status) {
        case errSecSuccess:
            secnotice("p12Decode", "cert added to keychain");
            break;
        case errSecDuplicateItem:    // dup cert, OK to skip
            secnotice("p12Decode", "skipping dup cert");
            status = errSecSuccess;
            break;
        default: //all other errors
            secerror("p12Decode: Error %d adding identity to keychain", status);
    }
    if (outCert) {
        *outCert = CFRetainSafe(certificate);
    }
    CFReleaseNull(certificate);
    CFReleaseNull(query);
    return status;
}

static OSStatus importPkcs12IdentityToLegacyKeychain(CFDictionaryRef item, SecKeychainRef importKeychain, SecAccessRef importAccess)
{
    SecIdentityRef identity = (SecIdentityRef)CFDictionaryGetValue(item, kSecImportItemIdentity);
    SecKeyRef privateKey = NULL;
    SecCertificateRef certificate = NULL;
    if (!identity || CFGetTypeID(identity) != SecIdentityGetTypeID()) {
        return errSecInternal; // Should never happen since SecPKCS12Import_ios make the item dictionary
    }

    // retrieve and add the constituent parts of the identity
    OSStatus status = importPkcs12KeyToLegacyKeychain(identity, importKeychain, importAccess, &privateKey);
    require_noerr(status, errOut);
    status = importPkcs12CertToLegacyKeychain(identity, importKeychain, &certificate);
    require_noerr(status, errOut);

    // update the returned item dictionary
    if (certificate && privateKey) {
        SecIdentityRef localIdentity = SecIdentityCreate(NULL, certificate, privateKey);
        if (localIdentity) {
            // replace identity with one using the keychain-based private key
            CFDictionarySetValue((CFMutableDictionaryRef)item, kSecImportItemIdentity, localIdentity);
            CFReleaseNull(localIdentity);
        }
        // set label item in output array to match legacy behavior
        CFStringRef label = SecCertificateCopySubjectSummary(certificate);
        if (label) {
            CFDictionarySetValue((CFMutableDictionaryRef)item, kSecImportItemLabel, label);
            CFReleaseNull(label);
        }
        CFDataRef keyID = SecKeyCopyPublicKeyHash(privateKey);
        if (keyID) {
            CFDictionarySetValue((CFMutableDictionaryRef)item, kSecImportItemKeyID, keyID);
        }
        CFReleaseNull(keyID);
    }

errOut:
    CFReleaseNull(privateKey);
    CFReleaseNull(certificate);

    return status;
}

static OSStatus parsePkcs12ItemsAndAddtoLegacyKeychain(const void *value, CFDictionaryRef options)
{
    OSStatus status = errSecSuccess;
    SecKeychainRef importKeychain = NULL;
    SecAccessRef importAccess = NULL;
    if (options) {
        importKeychain = (SecKeychainRef) CFDictionaryGetValue(options, kSecImportExportKeychain);
        CFRetainSafe(importKeychain);
        importAccess = (SecAccessRef) CFDictionaryGetValue(options, kSecImportExportAccess);
        CFRetainSafe(importAccess);
    }
    if (!importKeychain) {
        // legacy import behavior requires a keychain, so use default
        status = SecKeychainCopyDefault(&importKeychain);
        if (!importKeychain && !status) { status = errSecNoDefaultKeychain; }
        require_noerr(status, errOut);
    }
    if (CFGetTypeID(value) == CFDictionaryGetTypeID()) {
        CFDictionaryRef item = (CFDictionaryRef)value;
        if (CFDictionaryContainsKey(item, kSecImportItemIdentity)) {
            status = importPkcs12IdentityToLegacyKeychain(item, importKeychain, importAccess);
            require_noerr(status, errOut);
        }
        if (CFDictionaryContainsKey(item, kSecImportItemCertChain)) {
            status = importPkcs12CertChainToLegacyKeychain(item, importKeychain);
        }
    }
errOut:
    CFReleaseNull(importKeychain);
    CFReleaseNull(importAccess);
    return status;
}

// This wrapper calls the iOS p12 code to extract items from PKCS12 data into process memory.
// Once extracted into process memory, the wrapper maintains support for importing keys into
// legacy macOS keychains with SecAccessRef access control. If kSecUseDataProtectionKeychain
// is specified in options, items are imported to the "modern" data protection keychain.
//
OSStatus SecPKCS12Import(CFDataRef pkcs12_data, CFDictionaryRef options, CFArrayRef *items)
{
    if (!items) {
        return errSecParam;
    }
    __block OSStatus status = SecPKCS12Import_ios(pkcs12_data, options, items);
    if (_CFMZEnabled() || status != errSecSuccess) {
        // Catalyst callers get iOS behavior (no macOS keychain or legacy access control)
        return status;
    }
    Boolean useLegacyKeychain = true; // may be overridden by kSecUseDataProtectionKeychain
    Boolean useKeychain = true; // may be overridden by kSecImportToMemoryOnly
    if (options) {
        // macOS callers can explicitly specify the data protection keychain (no legacy access)
        CFBooleanRef dataProtectionEnabled = CFDictionaryGetValue(options, kSecUseDataProtectionKeychain);
        if (dataProtectionEnabled && (dataProtectionEnabled == kCFBooleanTrue)) {
            useLegacyKeychain = false;
        }
        // macOS callers can also specify not to use the keychain
        CFBooleanRef keychainDisabled = CFDictionaryGetValue(options, kSecImportToMemoryOnly);
        if (keychainDisabled && (keychainDisabled == kCFBooleanTrue)) {
            useKeychain = false;
        }
    }
    if (useKeychain) {
        // items is an array of dictionary containing kSecImportItemIdentity,kSecImportItemCertChain
        // kSecImportItemTrust keys/value pairs.
        if (useLegacyKeychain) {
            CFArrayForEach(*items, ^(const void *value) {
                OSStatus itemStatus = parsePkcs12ItemsAndAddtoLegacyKeychain(value, options);
                if (itemStatus != errSecSuccess) {
                    status = itemStatus;
                }
            });
        }
        // SecPKCS12Import_ios adds items to ModernKeychain if kSecUseDataProtectionKeychain is true
    }
    return status;
}

