/*
 * Copyright (c) 2007-2008,2012-2013 Apple Inc. All Rights Reserved.
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
#include <Security/SecRSAKey.h>
#include <Security/SecECKey.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include <Security/SecInternal.h>
#include <libDER/oids.h>
#include "debugging.h"
#include "utilities/SecCFWrappers.h"

#include <AssertMacros.h>

#include "p12import.h"
#include <Security/SecImportExportPriv.h>

#if !TARGET_OS_OSX
const CFStringRef kSecImportExportPassphrase = CFSTR("passphrase");
const CFStringRef kSecImportToMemoryOnly = CFSTR("memory");
const CFStringRef kSecImportItemLabel = CFSTR("label");
const CFStringRef kSecImportItemKeyID = CFSTR("keyid");
const CFStringRef kSecImportItemTrust = CFSTR("trust");
const CFStringRef kSecImportItemCertChain = CFSTR("chain");
const CFStringRef kSecImportItemIdentity = CFSTR("identity");
#endif

typedef struct {
    CFMutableArrayRef certs;
    p12_error *status;
} collect_certs_context;


static void collect_certs(const void *key, const void *value, void *context)
{
    if (!CFDictionaryContainsKey(value, CFSTR("key"))) {
        CFDataRef cert_bytes = CFDictionaryGetValue(value, CFSTR("cert"));
        if (!cert_bytes)
            return;
        collect_certs_context *a_collect_certs_context = (collect_certs_context *)context;
        SecCertificateRef cert = 
            SecCertificateCreateWithData(kCFAllocatorDefault, cert_bytes);
        if (!cert)  {
            *a_collect_certs_context->status = p12_decodeErr;
            return;
        }
        CFMutableArrayRef cert_array = a_collect_certs_context->certs;
        CFArrayAppendValue(cert_array, cert);
        CFRelease(cert);
    }
}

typedef struct {
    CFMutableArrayRef identities;
    CFMutableArrayRef unreferenced_certs;
    CFArrayRef certs;
    p12_error *status;
} build_trust_chains_context;

static void build_trust_chains(const void *key, const void *value, 
    void *context)
{
    CFMutableDictionaryRef identity_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    SecKeyRef private_key = NULL;
    SecCertificateRef cert = NULL;
    SecIdentityRef identity = NULL;
    SecPolicyRef policy = NULL;
    CFArrayRef cert_chain = NULL;
    CFMutableArrayRef eval_chain = NULL;
    SecTrustRef trust = NULL;
    build_trust_chains_context * a_build_trust_chains_context = (build_trust_chains_context*)context;

    CFDataRef key_bytes = CFDictionaryGetValue(value, CFSTR("key"));
    require(key_bytes, out);
    CFDataRef cert_bytes = CFDictionaryGetValue(value, CFSTR("cert"));
    require(cert_bytes, out);
    CFDataRef algoid_bytes = CFDictionaryGetValue(value, CFSTR("algid"));


    DERItem algorithm = { (DERByte *)CFDataGetBytePtr(algoid_bytes), CFDataGetLength(algoid_bytes) };
    if (DEROidCompare(&oidEcPubKey, &algorithm)) {
        require (private_key = SecKeyCreateECPrivateKey(kCFAllocatorDefault,
                                                         CFDataGetBytePtr(key_bytes), CFDataGetLength(key_bytes),
                                                         kSecKeyEncodingPkcs1), out);
    } else if (DEROidCompare(&oidRsa, &algorithm)) {
        require (private_key = SecKeyCreateRSAPrivateKey(kCFAllocatorDefault,
                                                         CFDataGetBytePtr(key_bytes), CFDataGetLength(key_bytes),
                                                         kSecKeyEncodingPkcs1), out);
    } else {
        *a_build_trust_chains_context->status = p12_decodeErr;
        goto out;
    }

    require_action(cert = SecCertificateCreateWithData(kCFAllocatorDefault, cert_bytes), out,
                   *a_build_trust_chains_context->status = p12_decodeErr);
    require_action(identity = SecIdentityCreate(kCFAllocatorDefault, cert, private_key), out,
                   *a_build_trust_chains_context->status = p12_decodeErr);
    CFDictionarySetValue(identity_dict, kSecImportItemIdentity, identity);
    
    eval_chain = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require(eval_chain, out);
    CFArrayAppendValue(eval_chain, cert);
    CFRange all_certs = { 0, CFArrayGetCount(a_build_trust_chains_context->certs) };
    CFArrayAppendArray(eval_chain, a_build_trust_chains_context->certs, all_certs);
    require(policy = SecPolicyCreateBasicX509(), out);
    SecTrustResultType result;
    SecTrustCreateWithCertificates(eval_chain, policy, &trust);
    require(trust, out);
    SecTrustEvaluate(trust, &result);
    CFDictionarySetValue(identity_dict, kSecImportItemTrust, trust);

    require(cert_chain = SecTrustCopyCertificateChain(trust), out);
    CFDictionarySetValue(identity_dict, kSecImportItemCertChain, cert_chain);
    
    CFArrayAppendValue(a_build_trust_chains_context->identities, identity_dict);
out:
    CFReleaseSafe(identity_dict);
    CFReleaseSafe(identity);
    CFReleaseSafe(private_key);
    CFReleaseSafe(cert);
    CFReleaseSafe(policy);
    CFReleaseSafe(cert_chain);
    CFReleaseSafe(eval_chain);
    CFReleaseSafe(trust);
}

#if TARGET_OS_OSX
static void build_unreferenced_certs(const void *value, void *context) {
    bool found = false;
    SecCertificateRef cert = (SecCertificateRef)value;
    build_trust_chains_context *a_build_trust_chains_context = (build_trust_chains_context*)context;
    CFMutableArrayRef identities = a_build_trust_chains_context->identities;
    CFIndex count = CFArrayGetCount(identities);
    for (CFIndex idx = 0; idx < count; idx++) {
        CFDictionaryRef dict = (CFDictionaryRef)CFArrayGetValueAtIndex(identities, idx);
        CFArrayRef chain = (CFArrayRef)CFDictionaryGetValue(dict, kSecImportItemCertChain);
        if (chain) {
            CFRange range = { 0, CFArrayGetCount(chain) };
            if (CFArrayContainsValue(chain, range, cert)) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        CFArrayAppendValue(a_build_trust_chains_context->unreferenced_certs, cert);
    }
}
#endif

static CFMutableDictionaryRef secItemOptionsFromPKCS12Options(CFDictionaryRef options)
{
    CFMutableDictionaryRef secItemOptions = CFDictionaryCreateMutableCopy(NULL, 0, options);
    CFDictionaryRemoveValue(secItemOptions, kSecImportExportPassphrase);
#if TARGET_OS_OSX
    CFDictionaryRemoveValue(secItemOptions, kSecImportExportKeychain);
    CFDictionaryRemoveValue(secItemOptions, kSecImportExportAccess);
#endif
    CFDictionaryRemoveValue(secItemOptions, kSecImportToMemoryOnly); // This shouldn't be here if we're making SecItem options

    return secItemOptions;
}

static OSStatus parsePkcs12ItemsAndAddtoModernKeychain(const void *value, CFDictionaryRef options)
{
    OSStatus status = errSecSuccess;
    if (CFGetTypeID(value) == CFDictionaryGetTypeID())
    {
        CFDictionaryRef item = (CFDictionaryRef)value;
        if (CFDictionaryContainsKey(item, kSecImportItemIdentity)) {
            SecIdentityRef identity = (SecIdentityRef)CFDictionaryGetValue(item, kSecImportItemIdentity);
            if (!identity || CFGetTypeID(identity) != SecIdentityGetTypeID()) {
                return errSecInternal; // Should never happen since SecPKCS12Import_ios make the item dictionary
            }
            CFMutableDictionaryRef query = secItemOptionsFromPKCS12Options(options);
            CFDictionaryAddValue(query, kSecUseDataProtectionKeychain, kCFBooleanTrue);
            CFDictionaryAddValue(query, kSecClass, kSecClassIdentity);
            CFDictionaryAddValue(query, kSecValueRef, identity);
            status = SecItemAdd(query, NULL);
            switch(status) {
                case errSecSuccess:
                    secnotice("p12Decode", "cert added to keychain");
                    break;
                case errSecDuplicateItem:    // dup cert, OK to skip
                    secnotice("p12Decode", "skipping dup cert");
                    status = errSecSuccess;
                    break;
                default: //all other errors
                    secerror("p12Decode: Error %d adding identity to keychain", (int)status);
            }
            CFReleaseNull(query);
        }
        if (CFDictionaryContainsKey(item, kSecImportItemCertChain)) {
            //go through certificate chain and all certificates
            CFArrayRef certChain = (CFArrayRef)CFDictionaryGetValue(item, kSecImportItemCertChain);
            if (!certChain || CFGetTypeID(certChain) != CFArrayGetTypeID()) {
                return errSecInternal; // Should never happen since SecPKCS12Import_ios make the item dictionary
            }
            for (unsigned index=0; index<CFArrayGetCount(certChain); index++) {
                SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(certChain, index);
                CFMutableDictionaryRef query = secItemOptionsFromPKCS12Options(options);
                CFDictionaryAddValue(query, kSecUseDataProtectionKeychain, kCFBooleanTrue);
                CFDictionaryAddValue(query, kSecClass, kSecClassCertificate);
                CFDictionaryAddValue(query, kSecValueRef, cert);
                status = SecItemAdd(query, NULL);
                switch(status) {
                    case errSecSuccess:
                        secnotice("p12Decode", "cert added to keychain");
                        break;
                    case errSecDuplicateItem:    // dup cert, OK to skip
                        secnotice("p12Decode", "skipping dup cert");
                        status = errSecSuccess;
                        break;
                    default: //all other errors
                        secerror("p12Decode: Error %d adding identity to keychain",  (int)status);
                }
                CFReleaseNull(query);
            }
        }
    }
    return status;
}

static OSStatus SecPKCS12ImportToModernKeychain(CFArrayRef *items, CFDictionaryRef options) {
    if (!options) {
        return errSecSuccess;
    }

    // Callers can specify `kSecUseDataProtectionKeychain` to add items to the keychain
    CFBooleanRef dataProtectionEnabled = CFDictionaryGetValue(options, kSecUseDataProtectionKeychain);
    if (!dataProtectionEnabled || (dataProtectionEnabled == kCFBooleanFalse)) {
        return errSecSuccess;
    }

    __block OSStatus status = errSecSuccess;
    CFArrayForEach(*items, ^(const void *value) {
        OSStatus itemStatus = parsePkcs12ItemsAndAddtoModernKeychain(value, options);
        if (itemStatus != errSecSuccess) {
            status = itemStatus;
        }
    });

    return status;
}

#if TARGET_OS_OSX
OSStatus SecPKCS12Import_ios(CFDataRef pkcs12_data, CFDictionaryRef options, CFArrayRef *items)
#else
OSStatus SecPKCS12Import(CFDataRef pkcs12_data, CFDictionaryRef options, CFArrayRef *items)
#endif
{
    pkcs12_context context = {};
    SecAsn1CoderCreate(&context.coder);
    if (options) {
        context.passphrase = CFDictionaryGetValue(options, kSecImportExportPassphrase);
        CFRetainSafe(context.passphrase);
    }
    if (!context.passphrase) {
        // Note that this changes a null passphrase into an empty passphrase, which are not
        // the same thing for key derivation purposes (the former is zero bytes of data,
        // while the latter is 2 null bytes.) Since key derivation must have some passphrase,
        // we will choose whether to handle 0-length as null data in p12_pbe_gen().
        context.passphrase = CFStringCreateWithCString(NULL, "", kCFStringEncodingUTF8);
    }
    context.items = CFDictionaryCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    p12_error status = p12decode(&context, pkcs12_data);
    if (!status) {
        CFMutableArrayRef certs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        CFMutableArrayRef unreferenced_certs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        collect_certs_context a_collect_certs_context = { certs, &status };
        CFDictionaryApplyFunction(context.items, collect_certs, &a_collect_certs_context);

        if (!status) {
            CFMutableArrayRef identities = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
            build_trust_chains_context a_build_trust_chains_context = { identities, unreferenced_certs, certs, &status };
            CFDictionaryApplyFunction(context.items, build_trust_chains, &a_build_trust_chains_context);
#if TARGET_OS_OSX
            /* unreferenced certs without an associated private key need to be returned separately */
            CFRange range = { 0, CFArrayGetCount(a_build_trust_chains_context.certs) };
            CFArrayApplyFunction((CFArrayRef)certs, range, build_unreferenced_certs, &a_build_trust_chains_context);
            if (CFArrayGetCount(a_build_trust_chains_context.unreferenced_certs) > 0) {
                CFMutableDictionaryRef certs_dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                    0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                CFDictionarySetValue(certs_dict, kSecImportItemCertChain, a_build_trust_chains_context.unreferenced_certs);
                CFArrayAppendValue(identities, certs_dict);
                CFReleaseSafe(certs_dict);
            }
#else
            /* ignoring certs that weren't picked up as part of the certchain for found keys */
#endif
            *items = identities;
        }
        CFReleaseSafe(unreferenced_certs);
        CFReleaseSafe(certs);
    }

    CFReleaseSafe(context.items);
    CFReleaseSafe(context.passphrase);
    SecAsn1CoderRelease(context.coder);
    
    switch (status) {
        case p12_noErr: break; // Continue to add to keychain
        case p12_passwordErr: return errSecAuthFailed;
        case p12_decodeErr: return errSecDecode;
        default: return errSecInternal;
    };

    // If we successfully decoded the P12, add items to keychain, if requested
    return SecPKCS12ImportToModernKeychain(items, options);
}

