/*
 * Copyright (c) 2008-2010,2012-2014,2023 Apple Inc. All Rights Reserved.
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


#include <Security/SecImportExport.h>

#include <CommonCrypto/CommonCryptor.h>
#include <Security/SecIdentity.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecBasePriv.h>
#include <Security/SecKey.h>
#include <Security/SecECKey.h>
#include <Security/SecCertificate.h>

#include <Security/SecInternal.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>
#include <unistd.h>
#include <utilities/array_size.h>
#include "shared_regressions.h"
#include "si-61-pkcs12.h"

#if TARGET_OS_OSX
static void delete_identity(SecCertificateRef cert, SecKeyRef pkey, Boolean useDPKC) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(query, kSecClass, kSecClassCertificate);
    CFDictionaryAddValue(query, kSecValueRef, cert);
    if (useDPKC) {
        CFDictionaryAddValue(query, kSecUseDataProtectionKeychain, kCFBooleanTrue);
        CFDictionaryAddValue(query, kSecAttrSynchronizable, kCFBooleanTrue);
    }
    SecItemDelete(query);

    CFDictionaryRemoveAllValues(query);
    CFDictionaryAddValue(query, kSecClass, kSecClassKey);
    CFDictionaryAddValue(query, kSecValueRef, pkey);
    if (useDPKC) {
        CFDictionaryAddValue(query, kSecUseDataProtectionKeychain, kCFBooleanTrue);
        CFDictionaryAddValue(query, kSecAttrSynchronizable, kCFBooleanTrue);
    }
    SecItemDelete(query);
    CFReleaseNull(query);
}
#endif


static void tests(void)
{
    CFDataRef message = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
        _user_one_p12, sizeof(_user_one_p12), kCFAllocatorNull);
    CFArrayRef items = NULL;
    SecCertificateRef cert = NULL;
    SecKeyRef pkey = NULL;

#ifndef __clang_analyzer__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // Disable compile-time nullability checks, otherwise the code below won't compile.
#if LEGACY_OSX_PKCS12
    // legacy code returned errSecPassphraseRequired instead of errSecAuthFailed; rdar://87484096
    is_status(SecPKCS12Import(message, NULL, &items), errSecPassphraseRequired,
              "try null password on a known good p12");
#else
    is_status(SecPKCS12Import(message, NULL, &items), errSecAuthFailed,
              "try null password on a known good p12");
#endif
#pragma clang diagnostic pop
#endif // __clang_analyzer__
    CFReleaseNull(items);

    CFStringRef password = CFSTR("user-one");
    CFDictionaryRef options = CFDictionaryCreate(NULL,
        (const void **)&kSecImportExportPassphrase,
        (const void **)&password, 1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    ok_status(SecPKCS12Import(message, options, &items), "import user one");

    is(CFArrayGetCount(items), 1, "one identity");
    CFDictionaryRef item = CFArrayGetValueAtIndex(items, 0);
    SecIdentityRef identity = NULL;
    ok(identity = (SecIdentityRef)CFDictionaryGetValue(item, kSecImportItemIdentity), "pull identity from imported data");

    ok(CFGetTypeID(identity)==SecIdentityGetTypeID(),"this is a SecIdentityRef");
    ok_status(SecIdentityCopyPrivateKey(identity, &pkey),"get private key");
    ok_status(SecIdentityCopyCertificate(identity, &cert), "get certificate");

#if TARGET_OS_OSX
    /* We need to delete the identity from the keychain because SecPKCS12Import imports to the
     * keychain on macOS. */
    delete_identity(cert, pkey, FALSE);
#endif

    CFReleaseNull(items);
    CFReleaseNull(message);
    CFReleaseNull(options);
    CFReleaseNull(password);
    CFReleaseNull(cert);
    CFReleaseNull(pkey);

    message = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
        _user_two_p12, sizeof(_user_two_p12), kCFAllocatorNull);
    items = NULL;
    password = CFSTR("user-two");
    options = CFDictionaryCreate(NULL,
        (const void **)&kSecImportExportPassphrase,
        (const void **)&password, 1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    ok_status(SecPKCS12Import(message, options, &items), "import user two");
    is(CFArrayGetCount(items), 1, "one identity");
    item = CFArrayGetValueAtIndex(items, 0);
    ok(identity = (SecIdentityRef)CFDictionaryGetValue(item, kSecImportItemIdentity), "pull identity from imported data");

    ok(CFGetTypeID(identity)==SecIdentityGetTypeID(),"this is a SecIdentityRef");
    ok_status(SecIdentityCopyPrivateKey(identity, &pkey),"get private key");
    ok_status(SecIdentityCopyCertificate(identity, &cert), "get certificate");

#if TARGET_OS_OSX
    delete_identity(cert, pkey, FALSE);
#endif


    CFReleaseNull(items);
    CFReleaseNull(message);
    CFReleaseNull(options);
    CFReleaseNull(password);
    CFReleaseNull(cert);
    CFReleaseNull(pkey);



    message = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                          ECDSA_fails_import_p12, ECDSA_fails_import_p12_len, kCFAllocatorNull);
    items = NULL;
    password = CFSTR("test");
    options = CFDictionaryCreate(NULL,
                                 (const void **)&kSecImportExportPassphrase,
                                 (const void **)&password, 1,
                                 &kCFTypeDictionaryKeyCallBacks,
                                 &kCFTypeDictionaryValueCallBacks);

    ok_status(SecPKCS12Import(message, options, &items), "import ECDSA_fails_import_p12");
    is(CFArrayGetCount(items), 1, "one identity");
    item = CFArrayGetValueAtIndex(items, 0);
    ok(identity = (SecIdentityRef)CFDictionaryGetValue(item, kSecImportItemIdentity), "pull identity from imported data");

    ok(CFGetTypeID(identity)==SecIdentityGetTypeID(),"this is a SecIdentityRef");
    ok_status(SecIdentityCopyPrivateKey(identity, &pkey),"get private key");
    ok_status(SecIdentityCopyCertificate(identity, &cert), "get certificate");

    SecKeyRef pubkey = NULL;
#if LEGACY_OSX_PKCS12
    ok(pubkey = SecCertificateCopyKey(cert), "get public key from cert");
#else
    ok(pubkey = SecKeyCopyPublicKey(pkey), "get public key from private key");
#endif
    CFReleaseNull(message);

    /* Sign something. */
    uint8_t something[20] = {0x80, 0xbe, 0xef, 0xba, 0xd0, };
    message = CFDataCreateWithBytesNoCopy(NULL, something, sizeof(something), kCFAllocatorNull);
    CFDataRef signature = NULL;
    CFErrorRef error = NULL;
    ok(signature = SecKeyCreateSignature(pkey, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, message, NULL), "sign something");
    ok(SecKeyVerifySignature(pubkey, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, message, signature, NULL), "verify sig on something");

#if TARGET_OS_OSX
    delete_identity(cert, pkey, FALSE);
#endif

    CFReleaseNull(pubkey);
    CFReleaseNull(pkey);

    CFDataRef pubdata = NULL;
    ok(pkey = SecKeyCreateECPrivateKey(kCFAllocatorDefault,
        ECDSA_fails_import_priv_only, ECDSA_fails_import_priv_only_len,
        kSecKeyEncodingPkcs1), "import privkey without pub");
    ok_status(SecKeyCopyPublicBytes(pkey, &pubdata), "pub key from priv key");
    ok(pubkey = SecKeyCreateECPublicKey(kCFAllocatorDefault,
        CFDataGetBytePtr(pubdata), CFDataGetLength(pubdata), kSecKeyEncodingBytes),
       "recreate seckey");
    ok(SecKeyVerifySignature(pubkey, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, message, signature, &error), "verify sig on something");

    CFReleaseNull(pubdata);
    CFReleaseNull(pubkey);
    CFReleaseNull(pkey);
    CFReleaseNull(signature);
    CFReleaseNull(items);
    CFReleaseNull(message);
    CFReleaseNull(options);
    CFReleaseNull(password);
    CFReleaseNull(cert);

    /* P521 test */
    CFDataRef cert_p521_p12 = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                          ec521_host_pfx, sizeof(ec521_host_pfx), kCFAllocatorNull);
    CFStringRef password_p521 = CFSTR("test!123");
    CFDictionaryRef options_p521 = CFDictionaryCreate(NULL,
                                                      (const void **)&kSecImportExportPassphrase,
                                                      (const void **)&password_p521, 1,
                                                      &kCFTypeDictionaryKeyCallBacks,
                                                      &kCFTypeDictionaryValueCallBacks);
    ok_status(SecPKCS12Import(cert_p521_p12, options_p521, &items), "Import p512 PKCS12 cert");
    is(CFArrayGetCount(items), 1, "one identity");
    item = CFArrayGetValueAtIndex(items, 0);
    ok(identity = (SecIdentityRef)CFDictionaryGetValue(item, kSecImportItemIdentity), "pull identity from imported data");

    ok(CFGetTypeID(identity)==SecIdentityGetTypeID(),"this is a SecIdentityRef");
    ok_status(SecIdentityCopyPrivateKey(identity, &pkey),"get private key");
    ok_status(SecIdentityCopyCertificate(identity, &cert), "get certificate");


#if TARGET_OS_OSX
    delete_identity(cert, pkey, FALSE);
#endif

    CFReleaseNull(items);
    CFReleaseNull(cert_p521_p12);
    CFReleaseNull(password_p521);
    CFReleaseNull(options_p521);
    CFReleaseNull(pkey);
    CFReleaseNull(cert);
}

static void test_cert_decode_error(void) {
    CFDataRef message = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, _cert_decode_error_p12,
                                                    sizeof(_cert_decode_error_p12), kCFAllocatorNull);
    CFArrayRef items = NULL;
    CFStringRef password = CFSTR("1234");
    CFDictionaryRef options = CFDictionaryCreate(NULL,
                                                 (const void **)&kSecImportExportPassphrase,
                                                 (const void **)&password, 1,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
#if LEGACY_OSX_PKCS12
    // legacy code returned errSecUnknownFormat instead of errSecDecode; rdar://87484096
    is(SecPKCS12Import(message, options, &items), errSecUnknownFormat, "import cert decode failure p12");
#else
    is(SecPKCS12Import(message, options, &items), errSecDecode, "import cert decode failure p12");
#endif
    CFReleaseNull(message);
    CFReleaseNull(items);
    CFReleaseNull(options);

}

#if TARGET_OS_OSX
static void test_data_protection_kc(void)
{
    CFDataRef message = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
        _user_one_p12, sizeof(_user_one_p12), kCFAllocatorNull);
    CFArrayRef items = NULL;
    CFStringRef password = CFSTR("user-one");
    SecCertificateRef cert = NULL;
    SecKeyRef pkey = NULL;

    const void *keys[] = {
        kSecImportExportPassphrase,
        kSecUseDataProtectionKeychain,
        kSecAttrSynchronizable
    };
    const void *values[] = {
        password,
        kCFBooleanTrue,
        kCFBooleanTrue
    };
    CFDictionaryRef options = CFDictionaryCreate(NULL, keys, values,
        array_size(keys), &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    ok_status(SecPKCS12Import(message, options, &items), "import to data protection kc");
    is(CFArrayGetCount(items), 1, "one identity");
    CFDictionaryRef item = CFArrayGetValueAtIndex(items, 0);
    SecIdentityRef identity = NULL;
    ok(identity = (SecIdentityRef)CFDictionaryGetValue(item, kSecImportItemIdentity), "pull identity from imported data");

    ok(CFGetTypeID(identity)==SecIdentityGetTypeID(),"this is a SecIdentityRef");
    ok_status(SecIdentityCopyPrivateKey(identity, &pkey),"get private key");
    ok_status(SecIdentityCopyCertificate(identity, &cert), "get certificate");

    CFStringRef group = CFSTR("com.apple.security.regressions");
    const void *identkeys[] = {
        kSecClass,
        kSecAttrAccessGroup,
        kSecUseDataProtectionKeychain,
        kSecAttrSynchronizable,
        kSecReturnRef
    };
    const void *identvalues[] = {
        kSecClassIdentity,
        group,
        kCFBooleanTrue,
        kCFBooleanTrue,
        kCFBooleanTrue
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, identkeys, identvalues,
        array_size(identkeys), &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    CFTypeRef result = NULL;
    ok_status(SecItemCopyMatching(query, &result), "found identity");
    ok(CFEqual(identity, result), "identity matches");

    delete_identity(cert, pkey, TRUE);

    CFReleaseNull(message);
    CFReleaseNull(items);
    CFReleaseNull(options);
    CFReleaseNull(pkey);
    CFReleaseNull(cert);

}
#endif /* TARGET_OS_OSX */

int si_61_pkcs12(int argc, char *const *argv)
{
	plan_tests(33);

	tests();
    test_cert_decode_error();
#if TARGET_OS_OSX
    test_data_protection_kc();
#endif
	return 0;
}
