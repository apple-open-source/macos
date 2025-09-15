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
#include <security_keychain/SecIdentityInternal.h>

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

static OSStatus importPkcs12IdentityToLegacyKeychain(CFDictionaryRef item, SecKeychainRef importKeychain, SecAccessRef importAccess)
{
    OSStatus status = errSecInternal;
    SecIdentityRef identity = (SecIdentityRef)CFDictionaryGetValue(item, kSecImportItemIdentity);

    if (!identity || CFGetTypeID(identity) != SecIdentityGetTypeID()) {
        return status; // Should never happen since SecPKCS12Import_ios make the item dictionary
    }

    SecKeyRef privateKey = NULL;
    SecCertificateRef certificate = NULL;
    SecIdentityRef localIdentity = SecIdentityImportToFileBackedKeychain(identity, importKeychain, importAccess);
    if (!localIdentity) {
        return status;
    }
    require_noerr(status = SecIdentityCopyPrivateKey(localIdentity, &privateKey), errOut);
    require_noerr(status = SecIdentityCopyCertificate(identity, &certificate), errOut);

    // update the returned item dictionary
    if (localIdentity && certificate && privateKey) {
        // replace identity with one using the keychain-based private key
        CFDictionarySetValue((CFMutableDictionaryRef)item, kSecImportItemIdentity, localIdentity);
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
    CFReleaseNull(localIdentity);
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

