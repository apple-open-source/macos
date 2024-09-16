/*
 * Copyright (c) 2023-2024 Apple Inc. All Rights Reserved.
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

#import <XCTest/XCTest.h>

#include <Security/SecImportExport.h>
#include <CommonCrypto/CommonCryptor.h>
#include <Security/SecIdentity.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecBasePriv.h>
#include <Security/SecKey.h>
#import <Security/SecKeyPriv.h>
#include <Security/SecECKey.h>
#include <Security/SecCertificate.h>
#include <Security/SecInternal.h>

#include <CoreFoundation/CoreFoundation.h>
#include <stdlib.h>
#include <unistd.h>
#include <utilities/array_size.h>

#import "SecPKCS12Tests.h"

@interface SecPKCS12Tests : XCTestCase
@end

@implementation SecPKCS12Tests

/* Notes: SecItem doesn't seem to work particularly well for certificates.
    - searching by kSecValueRef always returns the input cert
    - delete by kSecValueRef doesn't always delete the certificate (seems to be based on the certificate itself)
    - searching with a specified keychain only finds items in the searchlist
  The below calls take these behaviors into consideration.
 */

- (void)delete_identity:(SecIdentityRef)identity {
    if (!identity) { return; }
    SecCertificateRef cert = NULL;
    SecKeyRef key = NULL;

    XCTAssertEqual(errSecSuccess, SecIdentityCopyCertificate(identity, &cert));
    XCTAssertEqual(errSecSuccess, SecIdentityCopyPrivateKey(identity, &key));

    NSData *importedSN = CFBridgingRelease(SecCertificateCopySerialNumberData(cert,NULL));
    NSData *importedIssuer = CFBridgingRelease(SecCertificateCopyNormalizedIssuerSequence(cert));

    NSMutableDictionary *deleteCert = [@{ (id)kSecClass : (id)kSecClassCertificate,
                                          (id)kSecAttrIssuer : importedIssuer,
                                          (id)kSecAttrSerialNumber : importedSN,
                                          (id)kSecUseDataProtectionKeychain : @NO,
                                       } mutableCopy];
    SecItemDelete((__bridge CFDictionaryRef)deleteCert);
    deleteCert[(id)kSecUseDataProtectionKeychain] = @YES;
    SecItemDelete((__bridge CFDictionaryRef)deleteCert);

    NSMutableDictionary *deleteKey = [@{ (id)kSecClass : (id)kSecClassKey,
                                         (id)kSecValueRef : (__bridge id)key,
                                         (id)kSecUseDataProtectionKeychain : @NO,
                                       } mutableCopy];
    SecItemDelete((__bridge CFDictionaryRef)deleteKey);
    deleteKey[(id)kSecUseDataProtectionKeychain] = @YES;
    SecItemDelete((__bridge CFDictionaryRef)deleteKey);

    CFReleaseNull(cert);
    CFReleaseNull(key);
}

- (void)delete_certificate:(SecCertificateRef)certificate {
    if (!certificate) { return; }
    SecCertificateRef cert = CFRetainSafe(certificate);
    NSData *importedSN = CFBridgingRelease(SecCertificateCopySerialNumberData(cert,NULL));
    NSData *importedIssuer = CFBridgingRelease(SecCertificateCopyNormalizedIssuerSequence(cert));

    NSMutableDictionary *deleteCert = [@{ (id)kSecClass : (id)kSecClassCertificate,
                                          (id)kSecAttrIssuer : importedIssuer,
                                          (id)kSecAttrSerialNumber : importedSN,
                                          (id)kSecUseDataProtectionKeychain : @NO,
                                       } mutableCopy];
    SecItemDelete((__bridge CFDictionaryRef)deleteCert);
    deleteCert[(id)kSecUseDataProtectionKeychain] = @YES;
    SecItemDelete((__bridge CFDictionaryRef)deleteCert);
    CFReleaseNull(cert);
}

- (void)delete_chain:(CFArrayRef)chain {
    if (!chain) { return; }
    for (CFIndex idx = 0; idx < CFArrayGetCount(chain); idx++) {
        [self delete_certificate:(SecCertificateRef)CFArrayGetValueAtIndex(chain, idx)];
    }
}

-(void)testPKCS12ImportDuplicate {
    // test that SecPKCS12Import succeeds with a duplicate item
    // in the legacy keychain
    CFDataRef message = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                    _user_one_p12, sizeof(_user_one_p12), kCFAllocatorNull);
    CFArrayRef items = NULL;
    SecCertificateRef cert = NULL;
    SecKeyRef pkey = NULL;

    CFStringRef password = CFSTR("user-one");
    CFDictionaryRef options = CFDictionaryCreate(NULL,
                                                 (const void **)&kSecImportExportPassphrase,
                                                 (const void **)&password, 1,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    XCTAssertEqual(SecPKCS12Import(message, options, &items), errSecSuccess, @"import user one");

    XCTAssertEqual(CFArrayGetCount(items), 1, @"one identity");
    CFDictionaryRef item = CFArrayGetValueAtIndex(items, 0);
    SecIdentityRef identity = (SecIdentityRef)CFDictionaryGetValue(item, kSecImportItemIdentity);
    XCTAssertNotNil((__bridge id)identity, @"pull identity from imported data");

    XCTAssertEqual(CFGetTypeID(identity), SecIdentityGetTypeID(), @"this is a SecIdentityRef");
    XCTAssertEqual(SecIdentityCopyPrivateKey(identity, &pkey), errSecSuccess, @"get private key");
    XCTAssertEqual(SecIdentityCopyCertificate(identity, &cert), errSecSuccess, @"get certificate");

    CFReleaseNull(items);
    CFReleaseNull(cert);
    CFReleaseNull(pkey);

    /* Identity has been imported to memory on iOS, and to the keychain on macOS.
     We want to make sure re-importing will still return the identity on any
     platform, even if it would be a duplicate item in the legacy keychain. */

    XCTAssertEqual(SecPKCS12Import(message, options, &items), errSecSuccess, @"import user one");

    XCTAssertEqual(CFArrayGetCount(items), 1, @"one identity");
    item = CFArrayGetValueAtIndex(items, 0);
    identity = (SecIdentityRef)CFDictionaryGetValue(item, kSecImportItemIdentity);
    XCTAssertNotNil((__bridge id)identity, @"pull identity from imported data");
    CFArrayRef chain = (CFArrayRef)CFDictionaryGetValue(item, kSecImportItemCertChain);
    XCTAssertNotNil((__bridge id)chain, @"pull chain from imported data");

    XCTAssertEqual(CFGetTypeID(identity), SecIdentityGetTypeID(), @"this is a SecIdentityRef");
    XCTAssertEqual(SecIdentityCopyPrivateKey(identity, &pkey), errSecSuccess, @"get private key");
    XCTAssertEqual(SecIdentityCopyCertificate(identity, &cert), errSecSuccess, @"get certificate");

    /* all done with the imported identity, so remove it from the keychain (if added) */
    [self delete_identity:identity];
    [self delete_chain:chain];

    CFReleaseNull(items);
    CFReleaseNull(cert);
    CFReleaseNull(pkey);
    CFReleaseNull(message);
    CFReleaseNull(options);
    CFReleaseNull(password);
}

-(void)testPKCS12CertDecodeFailure {
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
    XCTAssertEqual(SecPKCS12Import(message, options, &items), errSecUnknownFormat, @"import cert decode failure p12");
#else
    XCTAssertEqual(SecPKCS12Import(message, options, &items), errSecDecode, @"import cert decode failure p12");
#endif
    CFReleaseNull(message);
    CFReleaseNull(items);
    CFReleaseNull(options);

}

-(void)testPKCS12ImportCertsOnly {
    SecCertificateRef leafCert = NULL;
    SecCertificateRef caCert = NULL;
    CFArrayRef p12Items = NULL;
    CFDataRef message = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, _certs_only_p12,
                                                    sizeof(_certs_only_p12), kCFAllocatorNull);
    CFStringRef password = CFSTR("3ca22a64e3fc65c4695e13ea879a6589c28ebbf6");
    CFDictionaryRef options = CFDictionaryCreate(NULL,
                                                 (const void **)&kSecImportExportPassphrase,
                                                 (const void **)&password, 1,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    XCTAssertEqual(SecPKCS12Import(message, options, &p12Items), errSecSuccess, @"import certs-only p12");
    CFReleaseNull(message);
    CFReleaseNull(options);

    // verify that we successfully imported certificates without private key
    NSArray *items = CFBridgingRelease(p12Items);
    NSUInteger expectedCount = 1;
#if !TARGET_OS_OSX
    // iOS platforms will not import certificates without an associated private key.
    // Remove this when rdar://134315793 is resolved.
    expectedCount = 0;
#endif // TARGET_OS_OSX
    XCTAssertEqual(items.count, expectedCount, @"imported cert array");
    if (!expectedCount) { return; }

    NSArray *chain = items[0][(__bridge NSString*)kSecImportItemCertChain];
    leafCert = (__bridge SecCertificateRef)chain[0];
    XCTAssertEqual(CFGetTypeID(leafCert), SecCertificateGetTypeID(), @"leaf is a certificate");
    caCert = (__bridge SecCertificateRef)chain[1];
    XCTAssertEqual(CFGetTypeID(caCert), SecCertificateGetTypeID(), @"ca is a certificate");

    // remove imported certificates from keychain
    [self delete_certificate:leafCert];
    [self delete_certificate:caCert];
}

-(void)testMissingPasswordPBE1 {
    // test that we can handle a sha256 MAC while using old-style PBES1
    // MAC: sha256, PBES1, PBKDF, pbeWithSHA1And3-KeyTripleDES-CBC
    CFDataRef message = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                    _test_p12_pbes1, sizeof(_test_p12_pbes1), kCFAllocatorNull);
    CFArrayRef items = NULL;
#ifndef __clang_analyzer__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // Disable compile-time nullability checks, otherwise the code below won't compile.
#if LEGACY_OSX_PKCS12
    // legacy code returned errSecPassphraseRequired instead of errSecAuthFailed; rdar://87484096
    XCTAssertEqual(SecPKCS12Import(message, NULL, NULL), errSecPassphraseRequired,
                   @"try null password on a known good p12");
#else
    XCTAssertEqual(SecPKCS12Import(message, NULL, &items), errSecAuthFailed,
                   @"try null password on a known good p12");
#endif
#pragma clang diagnostic pop
#endif // __clang_analyzer__

    CFReleaseNull(message);
    CFReleaseNull(items);
}

-(void)testMissingPasswordPBE2 {
    // test new implementation (PBES2) with new KDF, PRF, and AES-256-CBC
    // MAC: sha256, PBES2, PBKDF2, AES-256-CBC, Iteration 2048, PRF hmacWithSHA256
    CFDataRef message = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                                    _test_HMAC_SHA256_p12, sizeof(_test_HMAC_SHA256_p12), kCFAllocatorNull);
    CFArrayRef items = NULL;
#ifndef __clang_analyzer__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // Disable compile-time nullability checks, otherwise the code below won't compile.
#if LEGACY_OSX_PKCS12
    // legacy code returned errSecPassphraseRequired instead of errSecAuthFailed; rdar://87484096
    XCTAssertEqual(SecPKCS12Import(message, NULL, NULL), errSecPassphraseRequired,
                   @"try null password on a known good p12");
#else
    XCTAssertEqual(SecPKCS12Import(message, NULL, &items), errSecAuthFailed,
                   @"try null password on a known good p12");
#endif
#pragma clang diagnostic pop
#endif // __clang_analyzer__

    CFReleaseNull(message);
    CFReleaseNull(items);
}

typedef struct {
    uint8_t *p12;
    size_t p12Len;
    NSString *pwd;
} PKCS12TestVector;

static PKCS12TestVector p12TestVectors[] = {
    {
        // MAC: sha1, PBES1, PBKDF, pbeWithSHA1And3-KeyTripleDES-CBC
        .p12 = _user_one_p12,
        .p12Len = sizeof(_user_one_p12),
        .pwd = @"user-one",
    },
    {
        // MAC: sha1, PBES1, PBKDF, pbeWithSHA1And3-KeyTripleDES-CBC
        .p12 = _user_two_p12,
        .p12Len = sizeof(_user_two_p12),
        .pwd = @"user-two",
    },
    {
        // MAC: sha1, PBES1, PBKDF, pbeWithSHA1And3-KeyTripleDES-CBC
        .p12 = ECDSA_fails_import_p12,
        .p12Len = sizeof(ECDSA_fails_import_p12),
        .pwd = @"test",
    },
    {
        // MAC: sha1, PBES1, PBKDF, pbeWithSHA1And3-KeyTripleDES-CBC
        .p12 = ec521_host_pfx,
        .p12Len = sizeof(ec521_host_pfx),
        .pwd = @"test!123",
    },
    {
        // test new implementation (PBES2) with new KDF, PRF, and AES-256-CBC
        // MAC: sha256, PBES2, PBKDF2, AES-256-CBC, Iteration 2048, PRF hmacWithSHA256
        .p12 = _test_HMAC_SHA256_p12,
        .p12Len = sizeof(_test_HMAC_SHA256_p12),
        .pwd = @"test",
    },
    {
        // test that we can handle a sha256 MAC while using old-style PBES1
        // MAC: sha256, PBES1, PBKDF, pbeWithSHA1And3-KeyTripleDES-CBC
        .p12 = _test_p12_pbes1,
        .p12Len = sizeof(_test_p12_pbes1),
        .pwd = @"secret",
    },
    {
        // three certs (leaf, intermediate, root) + private key
        // MAC: sha256, PBES2, PBKDF2, AES-256-CBC, Iteration 2048, PRF hmacWithSHA256
        .p12 = _threeCertsAndKey_p12,
        .p12Len = sizeof(_threeCertsAndKey_p12),
        .pwd = @"test",
    },
};

static size_t p12TestVectorsCount = sizeof(p12TestVectors) / sizeof(PKCS12TestVector);

- (void)runImportToMemoryTest:(NSData *)p12data password:(NSString *)password {
    CFArrayRef cfItems;
    SecCertificateRef cert = NULL;
    SecKeyRef pkey = NULL;
    NSDictionary *options = @{
        (__bridge NSString*)kSecImportExportPassphrase : password,
        (__bridge NSString*)kSecImportToMemoryOnly : @YES,
    };

    XCTAssertEqual(SecPKCS12Import((__bridge CFDataRef)p12data, (__bridge CFDictionaryRef)options, &cfItems),
                   errSecSuccess, @"import p12");
    NSArray *items = CFBridgingRelease(cfItems);

    // verify that we successfully imported items to memory
    XCTAssertEqual(items.count, 1, @"one identity");
    NSDictionary* item = items[0];
    SecIdentityRef identity = (__bridge SecIdentityRef)item[(__bridge NSString*)kSecImportItemIdentity];
    XCTAssertNotNil((__bridge id)identity, @"pull identity from imported data");
    CFArrayRef chain = (__bridge CFArrayRef)item[(__bridge NSString*)kSecImportItemCertChain];
    XCTAssertNotNil((__bridge id)chain, @"pull chain from imported data");

    XCTAssertEqual(CFGetTypeID(identity), SecIdentityGetTypeID(), @"this is a SecIdentityRef");
    XCTAssertEqual(SecIdentityCopyPrivateKey(identity, &pkey), errSecSuccess, @"get private key");
    XCTAssertEqual(SecIdentityCopyCertificate(identity, &cert), errSecSuccess, @"get certificate");
    NSData *certSN = CFBridgingRelease(SecCertificateCopySerialNumberData(cert,NULL));
    NSData *certIssuer = CFBridgingRelease(SecCertificateCopyNormalizedIssuerSequence(cert));

    // verify that no part of the identity is in the keychain
    NSMutableDictionary *query = [NSMutableDictionary dictionaryWithCapacity:0];
    query[(id)kSecClass] = (id)kSecClassCertificate;
    query[(id)kSecAttrIssuer] = certIssuer;
    query[(id)kSecAttrSerialNumber] = certSN;
    XCTAssertNotEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    [query removeAllObjects];
    query[(id)kSecClass] = (id)kSecClassKey;
    query[(id)kSecValueRef] = (__bridge id)pkey;
    XCTAssertNotEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

#if TARGET_OS_OSX
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // On macOS, verify that our SecKeyRef and SecCertificateRef will
    // not crash SecKeychainItemCopyKeychain. Since they are not legacy
    // keychain items, we expect an error to be returned. The errors
    // differ since in-memory certificates are bridged to legacy
    // C++ ItemImpl instances and can be treated as a SecKeychainItemRef
    // with no keychain; other types are just invalid keychain items.
    SecKeychainRef itemKeychain = NULL;
    XCTAssertEqual(SecKeychainItemCopyKeychain((SecKeychainItemRef)pkey, &itemKeychain), errSecInvalidItemRef,  @"identity key is not a keychain item, and we cannot translate it to a legacy instance");
    XCTAssertNil((__bridge id)itemKeychain, @"key has no keychain");
    CFReleaseNull(itemKeychain);
    XCTAssertEqual(SecKeychainItemCopyKeychain((SecKeychainItemRef)cert, &itemKeychain), errSecNoSuchKeychain,  @"identity cert is a legacy keychain item, but has no associated keychain");
    XCTAssertNil((__bridge id)itemKeychain, @"cert has no keychain");
    CFReleaseNull(itemKeychain);
#pragma clang diagnostic pop
#endif // TARGET_OS_OSX

    [self delete_identity:identity];
    [self delete_chain:chain];
    CFReleaseNull(cert);
    CFReleaseNull(pkey);
}

- (void)runImportToDPKeychainTest:(NSData *)p12data password:(NSString *)password {
    CFArrayRef cfItems;
    SecCertificateRef cert = NULL;
    SecKeyRef pkey = NULL;
    NSDictionary *options = @{
        (__bridge NSString*)kSecImportExportPassphrase : password,
        (__bridge NSString*)kSecUseDataProtectionKeychain : @YES,
    };

    XCTAssertEqual(SecPKCS12Import((__bridge CFDataRef)p12data, (__bridge CFDictionaryRef)options, &cfItems),
                   errSecSuccess, @"import p12");
    NSArray *items = CFBridgingRelease(cfItems);

    // verify that we successfully imported items to memory
    XCTAssertEqual(items.count, 1, @"one identity");
    NSDictionary* item = items[0];
    SecIdentityRef identity = (__bridge SecIdentityRef)item[(__bridge NSString*)kSecImportItemIdentity];
    XCTAssertNotNil((__bridge id)identity, @"pull identity from imported data");
    CFArrayRef chain = (__bridge CFArrayRef)item[(__bridge NSString*)kSecImportItemCertChain];
    XCTAssertNotNil((__bridge id)chain, @"pull chain from imported data");

    XCTAssertEqual(CFGetTypeID(identity), SecIdentityGetTypeID(), @"this is a SecIdentityRef");
    XCTAssertEqual(SecIdentityCopyPrivateKey(identity, &pkey), errSecSuccess, @"get private key");
    XCTAssertEqual(SecIdentityCopyCertificate(identity, &cert), errSecSuccess, @"get certificate");

    NSData *certSN = CFBridgingRelease(SecCertificateCopySerialNumberData(cert,NULL));
    NSData *certIssuer = CFBridgingRelease(SecCertificateCopyNormalizedIssuerSequence(cert));

    // verify that both parts of the identity are in the DP keychain
    NSMutableDictionary *query = [NSMutableDictionary dictionaryWithCapacity:0];
    query[(id)kSecUseDataProtectionKeychain] = @YES;
    query[(id)kSecClass] = (id)kSecClassCertificate;
    query[(id)kSecAttrIssuer] = certIssuer;
    query[(id)kSecAttrSerialNumber] = certSN;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    [query removeAllObjects];
    query[(id)kSecClass] = (id)kSecClassKey;
    query[(id)kSecValueRef] = (__bridge id)pkey;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    [self delete_identity:identity];
    [self delete_chain:chain];
    CFReleaseNull(cert);
    CFReleaseNull(pkey);
}

- (void)runDefaultImportTest:(NSData *)p12data password:(NSString *)password {
    CFArrayRef cfItems;
    SecCertificateRef cert = NULL;
    SecKeyRef pkey = NULL;
    NSDictionary *options = @{
        (__bridge NSString*)kSecImportExportPassphrase : password,
    };

    XCTAssertEqual(SecPKCS12Import((__bridge CFDataRef)p12data, (__bridge CFDictionaryRef)options, &cfItems),
                   errSecSuccess, @"import p12");
    NSArray *items = CFBridgingRelease(cfItems);

    // verify that we successfully imported items to memory
    XCTAssertEqual(items.count, 1, @"one identity");
    NSDictionary* item = items[0];
    SecIdentityRef identity = (__bridge SecIdentityRef)item[(__bridge NSString*)kSecImportItemIdentity];
    XCTAssertNotNil((__bridge id)identity, @"pull identity from imported data");
    CFArrayRef chain = (__bridge CFArrayRef)item[(__bridge NSString*)kSecImportItemCertChain];
    XCTAssertNotNil((__bridge id)chain, @"pull chain from imported data");

    XCTAssertEqual(CFGetTypeID(identity), SecIdentityGetTypeID(), @"this is a SecIdentityRef");
    XCTAssertEqual(SecIdentityCopyPrivateKey(identity, &pkey), errSecSuccess, @"get private key");
    XCTAssertEqual(SecIdentityCopyCertificate(identity, &cert), errSecSuccess, @"get certificate");
    NSData *certSN = CFBridgingRelease(SecCertificateCopySerialNumberData(cert,NULL));
    NSData *certIssuer = CFBridgingRelease(SecCertificateCopyNormalizedIssuerSequence(cert));

    // check whether both parts of the identity can be found in any keychain, since kc is unspecified
    // (importing to default keychain is expected default behavior on macOS, but not on iOS.)
    NSMutableDictionary *query = [NSMutableDictionary dictionaryWithCapacity:0];
    query[(id)kSecClass] = (id)kSecClassCertificate;
    query[(id)kSecAttrIssuer] = certIssuer;
    query[(id)kSecAttrSerialNumber] = certSN;
    bool foundCert = errSecSuccess == SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL);
#if TARGET_OS_OSX
    XCTAssert(foundCert);
#else
    XCTAssertFalse(foundCert);
#endif // TARGET_OS_OSX
    [query removeAllObjects];
    query[(id)kSecClass] = (id)kSecClassKey;
    query[(id)kSecValueRef] = (__bridge id)pkey;
    bool foundKey = errSecSuccess == SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL);
#if TARGET_OS_OSX
    XCTAssert(foundKey);
#else
    XCTAssertFalse(foundKey);
#endif // TARGET_OS_OSX

#if TARGET_OS_OSX
    /* On macOS, import code also returns 'keyid' and 'label' items. */
    NSData *keyID = (NSData*)item[(__bridge NSString*)kSecImportItemKeyID];
    NSString *label = (NSString*)item[(__bridge NSString*)kSecImportItemLabel];
    XCTAssertNotNil(keyID, @"pull keyid from imported data");
    XCTAssertNotNil(label, @"pull label from imported data");
#endif // TARGET_OS_OSX

    [self delete_identity:identity];
    [self delete_chain:chain];
    CFReleaseNull(cert);
    CFReleaseNull(pkey);

}

#if TARGET_OS_OSX
- (void)runExplicitKeychainTest:(NSData *)p12data password:(NSString *)password keychain:(SecKeychainRef)keychain {
    CFArrayRef cfItems;
    SecCertificateRef cert = NULL;
    SecKeyRef pkey = NULL;
    NSDictionary *options = @{
        (__bridge NSString*)kSecImportExportPassphrase : password,
        (__bridge NSString*)kSecImportExportKeychain : (__bridge id)keychain,
    };

    XCTAssertEqual(SecPKCS12Import((__bridge CFDataRef)p12data, (__bridge CFDictionaryRef)options, &cfItems),
                   errSecSuccess, @"import p12");
    NSArray *items = CFBridgingRelease(cfItems);

    // verify that we successfully imported items to memory
    XCTAssertEqual(items.count, 1, @"one identity");
    NSDictionary* item = items[0];
    SecIdentityRef identity = (__bridge SecIdentityRef)item[(__bridge NSString*)kSecImportItemIdentity];
    XCTAssertNotNil((__bridge id)identity, @"pull identity from imported data");
    CFArrayRef chain = (__bridge CFArrayRef)item[(__bridge NSString*)kSecImportItemCertChain];
    XCTAssertNotNil((__bridge id)chain, @"pull chain from imported data");

    XCTAssertEqual(CFGetTypeID(identity), SecIdentityGetTypeID(), @"this is a SecIdentityRef");
    XCTAssertEqual(SecIdentityCopyPrivateKey(identity, &pkey), errSecSuccess, @"get private key");
    XCTAssertEqual(SecIdentityCopyCertificate(identity, &cert), errSecSuccess, @"get certificate");
    NSData *certSN = CFBridgingRelease(SecCertificateCopySerialNumberData(cert,NULL));
    NSData *certIssuer = CFBridgingRelease(SecCertificateCopyNormalizedIssuerSequence(cert));

    // verify that both parts of the identity are in the keychain
    NSMutableDictionary *query = [NSMutableDictionary dictionaryWithCapacity:0];
    query[(id)kSecUseKeychain] = (__bridge id)keychain;
    query[(id)kSecClass] = (id)kSecClassCertificate;
    query[(id)kSecAttrIssuer] = certIssuer;
    query[(id)kSecAttrSerialNumber] = certSN;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    [query removeAllObjects];
    query[(id)kSecUseKeychain] = (__bridge id)keychain;
    query[(id)kSecClass] = (id)kSecClassKey;
    query[(id)kSecValueRef] = (__bridge id)pkey;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    // On macOS, verify that our SecKeyRef and SecCertificateRef can be
    // treated as SecKeychainItemRef since they are in a legacy keychain.
    SecKeychainRef itemKeychain = NULL;
    XCTAssertEqual(SecKeychainItemCopyKeychain((SecKeychainItemRef)pkey, &itemKeychain), errSecSuccess,  @"identity key should be a keychain item after import to explicit keychain");
    XCTAssertNotNil((__bridge id)itemKeychain, @"key has a keychain");
    CFReleaseNull(itemKeychain);
    XCTAssertEqual(SecKeychainItemCopyKeychain((SecKeychainItemRef)cert, &itemKeychain), errSecSuccess,  @"identity cert should be a keychain item after import to explicit keychain");
    XCTAssertNotNil((__bridge id)itemKeychain, @"cert has a keychain");
    CFReleaseNull(itemKeychain);

    /* On macOS, import code also returns 'keyid' and 'label' items. */
    NSData *keyID = (NSData*)item[(__bridge NSString*)kSecImportItemKeyID];
    NSString *label = (NSString*)item[(__bridge NSString*)kSecImportItemLabel];
    XCTAssertNotNil(keyID, @"pull keyid from imported data");
    XCTAssertNotNil(label, @"pull label from imported data");
#pragma clang diagnostic pop

    [self delete_identity:identity];
    [self delete_chain:chain];
    CFReleaseNull(cert);
    CFReleaseNull(pkey);
}
#endif // TARGET_OS_OSX

- (void)testAllP12s {
#if TARGET_OS_OSX
    // Create Keychain
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    SecKeychainRef keychain = NULL;
    NSUUID *testUUID = [NSUUID UUID];
    NSString *fileName = [NSString stringWithFormat:@"SecPKCS12Tests-%@.keychain", [testUUID UUIDString]];
    NSURL *fileURL = [[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:fileName];
    XCTAssertEqual(errSecSuccess, SecKeychainCreate([fileURL fileSystemRepresentation], 8, "password", false, NULL, &keychain), @"make keychain");

    CFArrayRef cfSearchList = NULL;
    NSMutableArray *oldSearchList = NULL, *newSearchList = NULL;
    SecKeychainCopySearchList(&cfSearchList);
    oldSearchList = CFBridgingRelease(cfSearchList);
    newSearchList = [oldSearchList mutableCopy];
    [newSearchList addObject:(__bridge id)keychain];
    SecKeychainSetSearchList((__bridge CFArrayRef)newSearchList);
#pragma clang diagnostic pop
#endif // TARGET_OS_OSX

    // Test
    for (size_t i = 0; i < p12TestVectorsCount; i++) {
        PKCS12TestVector testVector = p12TestVectors[i];
        NSData *p12Data = [NSData dataWithBytes:testVector.p12 length:testVector.p12Len];

        [self runImportToMemoryTest:p12Data password:testVector.pwd];
        [self runImportToDPKeychainTest:p12Data password:testVector.pwd];
        [self runDefaultImportTest:p12Data password:testVector.pwd];
#if TARGET_OS_OSX
        [self runExplicitKeychainTest:p12Data password:testVector.pwd keychain:keychain];
#endif // TARGET_OS_OSX
    }

    // Remove Keychain
#if TARGET_OS_OSX
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    SecKeychainDelete(keychain);
    CFReleaseNull(keychain);
    [[NSFileManager defaultManager] removeItemAtURL:fileURL error:nil];
    SecKeychainSetSearchList((__bridge CFArrayRef)oldSearchList);
#pragma clang diagnostic pop
#endif // TARGET_OS_OSX
}

@end
