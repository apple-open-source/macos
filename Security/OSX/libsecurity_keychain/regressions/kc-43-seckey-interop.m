/*
 * Copyright (c) 2007-2009,2013-2014 Apple Inc. All Rights Reserved.
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


#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <Security/Security.h>
#include <Security/SecRandom.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecKeychainItem.h>

#include "keychain_regressions.h"
#include "utilities/SecCFRelease.h"
#include "utilities/array_size.h"

#if !TARGET_OS_IPHONE
static inline bool CFEqualSafe(CFTypeRef left, CFTypeRef right)
{
    if (left == NULL || right == NULL)
        return left == right;
    else
        return CFEqual(left, right);
}
#endif

static const int kTestGenerateNoLegacyCount = 11;
static void test_generate_nolegacy() {
    NSDictionary *query, *params = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA,
        (id)kSecAttrKeySizeInBits: @1024,
        (id)kSecAttrNoLegacy: @YES,
        (id)kSecAttrIsPermanent: @YES,
        (id)kSecAttrLabel: @"sectests:generate-no-legacy",
    };

    SecKeyRef pubKey = NULL, privKey = NULL, key = NULL;
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)params, &pubKey, &privKey));

    query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: @"sectests:generate-no-legacy",
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic,
        (id)kSecAttrNoLegacy: @YES,
        (id)kSecReturnRef: @YES,
    };
    ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef *)&key));
    eq_cf(key, pubKey);
    CFReleaseNull(key);

    query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: @"sectests:generate-no-legacy",
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrNoLegacy: @YES,
        (id)kSecReturnRef: @YES,
    };
    ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef *)&key));
    eq_cf(key, privKey);
    CFReleaseNull(key);

    query = @{
              (id)kSecClass: (id)kSecClassKey,
              (id)kSecAttrLabel: @"sectests:generate-no-legacy",
              (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic,
              (id)kSecAttrNoLegacy: @YES,
              (id)kSecReturnRef: @YES,
              };
    ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef *)&key));
    eq_cf(key, pubKey);
    CFReleaseNull(key);

    query = @{
              (id)kSecClass: (id)kSecClassKey,
              (id)kSecAttrLabel: @"sectests:generate-no-legacy",
              (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
              (id)kSecAttrNoLegacy: @YES,
              (id)kSecReturnRef: @YES,
              };
    ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef *)&key));
    eq_cf(key, privKey);
    CFReleaseNull(key);
    
    query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: @"sectests:generate-no-legacy",
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
        (id)kSecAttrNoLegacy: @YES,
    };
    ok_status(SecItemDelete((__bridge CFDictionaryRef)query));
    is_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);

    CFReleaseNull(privKey);
    CFReleaseNull(pubKey);
}

#if !RC_HIDE_J79 && !RC_HIDE_J80
static const int kTestGenerateAccessControlCount = 4;
static void test_generate_access_control() {
    SecAccessControlRef ac = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleAlways,
                                                             0 /* | kSecAccessControlPrivateKeyUsage */, NULL);
    NSDictionary *params = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA,
        (id)kSecAttrKeySizeInBits: @1024,
        (id)kSecAttrAccessControl: (__bridge id)ac,
        (id)kSecAttrIsPermanent: @YES,
        (id)kSecAttrLabel: @"sectests:generate-access-control",
    };

    SecKeyRef pubKey, privKey;
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)params, &pubKey, &privKey));

    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: @"sectests:generate-access-control",
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
        (id)kSecAttrNoLegacy: @YES,
    };
    ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL));

    ok_status(SecItemDelete((__bridge CFDictionaryRef)query));

    is_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);

    CFReleaseSafe(ac);
    CFReleaseSafe(privKey);
    CFReleaseSafe(pubKey);
}
#else
static const int kTestGenerateAccessControlCount = 0;
#endif

static const int kTestAddIOSKeyCount = 6;
static void test_add_ios_key() {
    NSDictionary *params = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA,
        (id)kSecAttrKeySizeInBits: @1024,
        (id)kSecAttrNoLegacy: @YES,
        (id)kSecAttrIsPermanent: @NO,
    };

    SecKeyRef pubKey, privKey;
    ok_status(SecKeyGeneratePair((__bridge CFDictionaryRef)params, &pubKey, &privKey));

    NSDictionary *attrs = @{
        (id)kSecValueRef: (__bridge id)privKey,
        (id)kSecAttrLabel: @"sectests:add-ios-key",
    };
    ok_status(SecItemAdd((__bridge CFDictionaryRef)attrs, NULL));

    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: @"sectests:add-ios-key",
        (id)kSecAttrNoLegacy: @YES,
        (id)kSecReturnRef: @YES,
    };
    SecKeyRef key = NULL;
    ok_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef *)&key));
    eq_cf(key, privKey);
    CFReleaseNull(key);

    query = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: @"sectests:add-ios-key",
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
        (id)kSecAttrNoLegacy: @YES,
    };
    ok_status(SecItemDelete((__bridge CFDictionaryRef)query));
    is_status(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);

    CFReleaseNull(pubKey);
    CFReleaseNull(privKey);
}

static const char *certDataBase64 = "\
MIIEQjCCAyqgAwIBAgIJAJdFadWqNIfiMA0GCSqGSIb3DQEBBQUAMHMxCzAJBgNVBAYTAkNaMQ8wDQYD\
VQQHEwZQcmFndWUxFTATBgNVBAoTDENvc21vcywgSW5jLjEXMBUGA1UEAxMOc3VuLmNvc21vcy5nb2Qx\
IzAhBgkqhkiG9w0BCQEWFHRoaW5nQHN1bi5jb3Ntb3MuZ29kMB4XDTE2MDIyNjE0NTQ0OVoXDTE4MTEy\
MjE0NTQ0OVowczELMAkGA1UEBhMCQ1oxDzANBgNVBAcTBlByYWd1ZTEVMBMGA1UEChMMQ29zbW9zLCBJ\
bmMuMRcwFQYDVQQDEw5zdW4uY29zbW9zLmdvZDEjMCEGCSqGSIb3DQEJARYUdGhpbmdAc3VuLmNvc21v\
cy5nb2QwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC5u9gnYEDzQIVu7yC40VcXTZ01D9CJ\
oD/mH62tebEHEdfVPLWKeq+uAHnJ6fTIJQvksaISOxwiOosFjtI30mbe6LZ/oK22wYX+OUwKhAYjZQPy\
RYfuaJe/52F0zmfUSJ+KTbUZrXbVVFma4xPfpg4bptvtGkFJWnufvEEHimOGmO5O69lXA0Hit1yLU0/A\
MQrIMmZT8gb8LMZGPZearT90KhCbTHAxjcBfswZYeL8q3xuEVHXC7EMs6mq8IgZL7mzSBmrCfmBAIO0V\
jW2kvmy0NFxkjIeHUShtYb11oYYyfHuz+1vr1y6FIoLmDejKVnwfcuNb545m26o+z/m9Lv9bAgMBAAGj\
gdgwgdUwHQYDVR0OBBYEFGDdpPELS92xT+Hkh/7lcc+4G56VMIGlBgNVHSMEgZ0wgZqAFGDdpPELS92x\
T+Hkh/7lcc+4G56VoXekdTBzMQswCQYDVQQGEwJDWjEPMA0GA1UEBxMGUHJhZ3VlMRUwEwYDVQQKEwxD\
b3Ntb3MsIEluYy4xFzAVBgNVBAMTDnN1bi5jb3Ntb3MuZ29kMSMwIQYJKoZIhvcNAQkBFhR0aGluZ0Bz\
dW4uY29zbW9zLmdvZIIJAJdFadWqNIfiMAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcNAQEFBQADggEBAFYi\
Zu/dfAMOrD51bYxP88Wu6iDGBe9nMG/0lkKgnX5JQKCxfxFMk875rfa+pljdUMOaPxegOXq1DrYmQB9O\
/pHI+t7ozuWHRj2zKkVgMWAygNWDPcoqBEus53BdAgA644aPN2JvnE4NEPCllOMKftPoIWbd/5ZjCx3a\
bCuxBdXq5YSmiEnOdGfKeXjeeEiIDgARb4tLgH5rkOpB1uH/ZCWn1hkiajBhrGhhPhpA0zbkZg2Ug+8g\
XPlx1yQB1VOJkj2Z8dUEXCaRRijInCJ2eU+pgJvwLV7mxmSED7DEJ+b+opxJKYrsdKBU6RmYpPrDa+KC\
/Yfu88P9hKKj0LmBiREA\
";

static const char *keyDataBase64 = "\
MIIEogIBAAKCAQEAubvYJ2BA80CFbu8guNFXF02dNQ/QiaA/5h+trXmxBxHX1Ty1inqvrgB5yen0yCUL\
5LGiEjscIjqLBY7SN9Jm3ui2f6CttsGF/jlMCoQGI2UD8kWH7miXv+dhdM5n1Eifik21Ga121VRZmuMT\
36YOG6bb7RpBSVp7n7xBB4pjhpjuTuvZVwNB4rdci1NPwDEKyDJmU/IG/CzGRj2Xmq0/dCoQm0xwMY3A\
X7MGWHi/Kt8bhFR1wuxDLOpqvCIGS+5s0gZqwn5gQCDtFY1tpL5stDRcZIyHh1EobWG9daGGMnx7s/tb\
69cuhSKC5g3oylZ8H3LjW+eOZtuqPs/5vS7/WwIDAQABAoIBAGcwmQAPdyZus3OVwa1NCUD2KyB+39KG\
yNmWwgx+br9Jx4s+RnJghVh8BS4MIKZOBtSRaEUOuCvAMNrupZbD+8leq34vDDRcQpCizr+M6Egj6FRj\
Ewl+7Mh+yeN2hbMoghL552MTv9D4Iyxteu4nuPDd/JQ3oQwbDFIL6mlBFtiBDUr9ndemmcJ0WKuzor6a\
3rgsygLs8SPyMefwIKjh5rJZls+iv3AyVEoBdCbHBz0HKgLVE9ZNmY/gWqda2dzAcJxxMdafeNVwHovv\
BtyyRGnA7Yikx2XT4WLgKfuUsYLnDWs4GdAa738uxPBfiddQNeRjN7jRT1GZIWCk0P29rMECgYEA8jWi\
g1Dph+4VlESPOffTEt1aCYQQWtHs13Qex95HrXX/L49fs6cOE7pvBh7nVzaKwBnPRh5+3bCPsPmRVb7h\
k/GreOriCjTZtyt2XGp8eIfstfirofB7c1lNBjT61BhgjJ8Moii5c2ksNIOOZnKtD53n47mf7hiarYkw\
xFEgU6ECgYEAxE8Js3gIPOBjsSw47XHuvsjP880nZZx/oiQ4IeJb/0rkoDMVJjU69WQu1HTNNAnMg4/u\
RXo31h+gDZOlE9t9vSXHdrn3at67KAVmoTbRknGxZ+8tYpRJpPj1hyufynBGcKwevv3eHJHnE5eDqbHx\
ynZFkXemzT9aMy3R4CCFMXsCgYAYyZpnG/m6WohE0zthMFaeoJ6dSLGvyboWVqDrzXjCbMf/4wllRlxv\
cm34T2NXjpJmlH2c7HQJVg9uiivwfYdyb5If3tHhP4VkdIM5dABnCWoVOWy/NvA7XtE+KF/fItuGqKRP\
WCGaiRHoEeqZ23SQm5VmvdF7OXNi/R5LiQ3o4QKBgAGX8qg2TTrRR33ksgGbbyi1UJrWC3/TqWWTjbEY\
uU51OS3jvEQ3ImdjjM3EtPW7LqHSxUhjGZjvYMk7bZefrIGgkOHx2IRRkotcn9ynKURbD+mcE249beuc\
6cFTJVTrXGcFvqomPWtV895A2JzECQZvt1ja88uuu/i2YoHDQdGJAoGAL2TEgiMXiunb6PzYMMKKa+mx\
mFnagF0Ek3UJ9ByXKoLz3HFEl7cADIkqyenXFsAER/ifMyCoZp/PDBd6ZkpqLTdH0jQ2Yo4SllLykoiZ\
fBWMfjRu4iw9E0MbPB3blmtzfv53BtWKy0LUOlN4juvpqryA7TgaUlZkfMT+T1TC7xU=\
";

static const int kTestStoreCertToIOS = 5;
static void test_store_cert_to_ios() {
    // Create certificate instance.
    NSData *certData = [[NSData alloc] initWithBase64EncodedString:[NSString stringWithUTF8String:certDataBase64]
                                                           options:NSDataBase64DecodingIgnoreUnknownCharacters];
    SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData);
    ok(cert != NULL, "create certificate from data");

    // Store certificate to modern keychain.
    NSDictionary *attrs = @{
        (id)kSecValueRef: (__bridge id)cert,
        (id)kSecAttrLabel: @"sectests:store_cert_to_ios",
        (id)kSecAttrNoLegacy: @YES,
        (id)kSecReturnPersistentRef: @YES,
    };
    id persistentRef;
    ok_status(SecItemAdd((CFDictionaryRef)attrs, (void *)&persistentRef), "store certificate into iOS keychain");

    // Query certificate, without specification of the keychain.
    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassCertificate,
        (id)kSecAttrLabel: @"sectests:store_cert_to_ios",
        (id)kSecReturnRef: @YES,
    };
    SecCertificateRef queriedCert = NULL;
    ok_status(SecItemCopyMatching((CFDictionaryRef)query, (void *)&queriedCert), "query certificate back");
    eq_cf(cert, queriedCert, "stored and retrieved certificates are the same");

    ok_status(SecItemDelete((CFDictionaryRef)@{ (id)kSecValuePersistentRef: persistentRef }),
                            "delete certificate from keychain");
    CFReleaseNull(cert);
}

static const int kTestStoreIdentityToIOS = 6;
static void test_store_identity_to_ios() {
    // Create certificate instance.
    NSData *certData = [[NSData alloc] initWithBase64EncodedString:[NSString stringWithUTF8String:certDataBase64]
                                                           options:NSDataBase64DecodingIgnoreUnknownCharacters];
    SecCertificateRef certificate = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData);
    ok(certificate != NULL, "create certificate from data");

    // Create private key instance.
    NSData *keyData = [[NSData alloc] initWithBase64EncodedString:[NSString stringWithUTF8String:keyDataBase64]
                                                          options:NSDataBase64DecodingIgnoreUnknownCharacters];
    NSDictionary *keyAttrs = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA, (id)kSecAttrKeySizeInBits: @2048,
                                (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate };
    SecKeyRef privateKey = SecKeyCreateWithData((CFDataRef)keyData, (CFDictionaryRef)keyAttrs, NULL);
    ok(privateKey != NULL, "create private key from data");

    // Create identity from certificate and private key.
    SecIdentityRef identity = SecIdentityCreate(kCFAllocatorDefault, certificate, privateKey);

    // Store identity to the iOS keychain.
    NSDictionary *attrs = @{
        (id)kSecValueRef: (__bridge id)identity,
        (id)kSecAttrLabel: @"sectests:store_identity_to_ios",
        (id)kSecAttrNoLegacy: @YES,
        (id)kSecReturnPersistentRef: @YES,
    };
    id persistentRef;
    ok_status(SecItemAdd((CFDictionaryRef)attrs, (void *)&persistentRef), "store identity into iOS keychain");

    NSDictionary *query = @{
        (id)kSecClass: (id)kSecClassIdentity,
        (id)kSecAttrLabel: @"sectests:store_identity_to_ios",
        (id)kSecReturnRef: @YES,
    };
    SecIdentityRef queriedIdentity = NULL;
    ok_status(SecItemCopyMatching((CFDictionaryRef)query, (void *)&queriedIdentity), "query identity from keychain");
    eq_cf(identity, queriedIdentity, "stored and retrieved identities are identical");

    // Cleanup identity.
    ok_status(SecItemDelete((CFDictionaryRef)@{ (id)kSecValuePersistentRef: persistentRef}),
              "delete identity from iOS keychain");

    CFReleaseNull(identity);
    CFReleaseNull(privateKey);
    CFReleaseNull(certificate);
}

static const int kTestTransformWithIOSKey = 9;
static void test_transform_with_ioskey() {
    // Create private key instance.
    NSData *keyData = [[NSData alloc] initWithBase64EncodedString:[NSString stringWithUTF8String:keyDataBase64]
                                                          options:NSDataBase64DecodingIgnoreUnknownCharacters];
    NSDictionary *keyAttrs = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA, (id)kSecAttrKeySizeInBits: @2048,
                                (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate };
    SecKeyRef privateKey = SecKeyCreateWithData((CFDataRef)keyData, (CFDictionaryRef)keyAttrs, NULL);
    ok(privateKey != NULL, "create private key from data");

    // Create signature transform with private key
    NSData *testData = [NSData dataWithBytes:"test" length:4];
    SecTransformRef signer = SecSignTransformCreate(privateKey, NULL);
    ok(signer != NULL, "create signing transform");
    ok(SecTransformSetAttribute(signer, kSecTransformInputAttributeName, (CFDataRef)testData, NULL),
       "set input data to verify transform");
    NSData *signature = (__bridge_transfer NSData *)SecTransformExecute(signer, NULL);
    ok(signature != nil, "create signature with transform");

    // Create verify transform with public key.
    SecKeyRef publicKey = SecKeyCopyPublicKey(privateKey);
    ok(publicKey != NULL, "get public key from private key");
    SecTransformRef verifier = SecVerifyTransformCreate(publicKey, (CFDataRef)signature, NULL);
    ok(verifier, "create verification transform");
    ok(SecTransformSetAttribute(verifier, kSecTransformInputAttributeName, (CFDataRef)testData, NULL),
       "set input data to verify transform");

    NSNumber *result = (__bridge_transfer NSNumber *)SecTransformExecute(verifier, NULL);
    ok(result != nil, "transform execution succeeded");
    ok(result.boolValue, "transform verified signature");

    CFReleaseNull(signer);
    CFReleaseNull(verifier);
    CFReleaseNull(publicKey);
    CFReleaseNull(privateKey);
}

static const int kTestConvertKeyToPersistentRef = 11;
static void test_convert_key_to_persistent_ref() {
    NSString *label = @"sectests:convert-key-to-persistent-ref";

    // Create
    SecKeyRef privKey = NULL;
    {
        NSDictionary *query = @{
            (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA,
            (id)kSecAttrKeySizeInBits: @1024,
            (id)kSecAttrNoLegacy: @YES,
            (id)kSecAttrIsPermanent: @NO,
        };
        SecKeyRef pubKey = NULL;
        ok_status(SecKeyGeneratePair((CFDictionaryRef)query, &pubKey, &privKey));
        CFReleaseNull(pubKey);
    }

    // Store
    {
        NSDictionary *query = @{
            (id)kSecAttrLabel: label,
            (id)kSecValueRef: (__bridge id)privKey,
            (id)kSecAttrNoLegacy: @YES,
        };
        ok_status(SecItemAdd((CFDictionaryRef)query, NULL));
    }

    // Convert & Compare
    CFDataRef queriedPersistentKeyRef = NULL;
    SecKeyRef queriedKeyRef = NULL;
    {
        NSDictionary *query = @{
            (id)kSecValueRef: (__bridge id)privKey,
            (id)kSecReturnPersistentRef: @YES,
            (id)kSecAttrNoLegacy: @YES,
        };
        ok_status(SecItemCopyMatching((CFDictionaryRef)query, (CFTypeRef *)&queriedPersistentKeyRef));
    }{
        NSDictionary *query = @{
            (id)kSecValuePersistentRef: (__bridge id)queriedPersistentKeyRef,
            (id)kSecReturnRef: @YES,
            (id)kSecAttrNoLegacy: @YES,
        };
        ok_status(SecItemCopyMatching((CFDictionaryRef)query, (CFTypeRef *)&queriedKeyRef));
    }{
        CFDataRef persistentKeyRef = NULL;
        SecKeychainItemRef keyRef = NULL;
        ok_status(SecKeychainItemCreatePersistentReference((SecKeychainItemRef)privKey, &persistentKeyRef));
        ok_status(SecKeychainItemCopyFromPersistentReference(persistentKeyRef, &keyRef));
        eq_cf(privKey, queriedKeyRef);
        eq_cf(keyRef, privKey);
        eq_cf(persistentKeyRef, queriedPersistentKeyRef);
        CFReleaseNull(persistentKeyRef);
        CFReleaseNull(keyRef);
    }
    CFReleaseNull(queriedPersistentKeyRef);
    CFReleaseNull(queriedKeyRef);

    // Cleanup
    CFReleaseNull(privKey);
    {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrLabel: label,
            (id)kSecMatchLimit: (id)kSecMatchLimitAll,
        };
        ok_status(SecItemDelete((CFDictionaryRef)query));
        is_status(SecItemCopyMatching((CFDictionaryRef)query, NULL), errSecItemNotFound);
    }
}

static const int kTestConvertCertToPersistentRef = 11;
static void test_convert_cert_to_persistent_ref() {
    NSString *label = @"sectests:convert-cert-to-persistent-ref";

    // Create
    SecCertificateRef cert = NULL;
    {
        NSData *certData = [[NSData alloc] initWithBase64EncodedString:[NSString stringWithUTF8String:certDataBase64]
                                                               options:NSDataBase64DecodingIgnoreUnknownCharacters];
        cert = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData);
        ok(cert);
    }

    // Store
    {
        NSDictionary *query = @{
            (id)kSecAttrLabel: label,
            (id)kSecValueRef: (__bridge id)cert,
            (id)kSecAttrNoLegacy: @YES,
        };
        ok_status(SecItemAdd((CFDictionaryRef)query, NULL));
    }

    // Convert & Compare
    CFDataRef queriedPersistentCertRef = NULL;
    SecCertificateRef queriedCertRef = NULL;
    {
        NSDictionary *query = @{
            (id)kSecValueRef: (__bridge id)cert,
            (id)kSecReturnPersistentRef: @YES,
            (id)kSecAttrNoLegacy: @YES,
        };
        ok_status(SecItemCopyMatching((CFDictionaryRef)query, (CFTypeRef *)&queriedPersistentCertRef));
    }{
        NSDictionary *query = @{
            (id)kSecValuePersistentRef: (__bridge id)queriedPersistentCertRef,
            (id)kSecReturnRef: @YES,
            (id)kSecAttrNoLegacy: @YES,
        };
        ok_status(SecItemCopyMatching((CFDictionaryRef)query, (CFTypeRef *)&queriedCertRef));
    }{
        CFDataRef persistentCertRef = NULL;
        SecKeychainItemRef certRef = NULL;
        ok_status(SecKeychainItemCreatePersistentReference((SecKeychainItemRef)cert, &persistentCertRef));
        ok_status(SecKeychainItemCopyFromPersistentReference(persistentCertRef, &certRef));
        eq_cf(cert, queriedCertRef);
        eq_cf(certRef, cert);
        eq_cf(persistentCertRef, queriedPersistentCertRef);
        CFReleaseNull(persistentCertRef);
        CFReleaseNull(certRef);
    }
    CFReleaseNull(queriedPersistentCertRef);
    CFReleaseNull(queriedCertRef);

    // Cleanup
    CFReleaseNull(cert);
    {
        NSDictionary *query = @{
            (id)kSecClass: (id)kSecClassCertificate,
            (id)kSecAttrLabel: label,
            (id)kSecMatchLimit: (id)kSecMatchLimitAll,
        };
        ok_status(SecItemDelete((CFDictionaryRef)query));
        is_status(SecItemCopyMatching((CFDictionaryRef)query, NULL), errSecItemNotFound);
    }
}

static const int kTestConvertIdentityToPersistentRef = 12;
static void test_convert_identity_to_persistent_ref() {
    NSString *label = @"sectests:convert-identity-to-persistent-ref";

    // Create
    SecIdentityRef idnt = NULL;
    {
        NSData *certData = [[NSData alloc] initWithBase64EncodedString:[NSString stringWithUTF8String:certDataBase64]
                                                               options:NSDataBase64DecodingIgnoreUnknownCharacters];
        SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData);
        ok(cert);
        NSData *keyData = [[NSData alloc] initWithBase64EncodedString:[NSString stringWithUTF8String:keyDataBase64]
                                                              options:NSDataBase64DecodingIgnoreUnknownCharacters];
        NSDictionary *keyAttrs = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA,
                                    (id)kSecAttrKeySizeInBits: @2048,
                                    (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate };
        SecKeyRef privKey = SecKeyCreateWithData((CFDataRef)keyData, (CFDictionaryRef)keyAttrs, NULL);
        ok(privKey);
        idnt = SecIdentityCreate(kCFAllocatorDefault, cert, privKey);
        CFReleaseNull(cert);
        CFReleaseNull(privKey);
    }

    // Store
    {
        NSDictionary *query = @{
            (id)kSecAttrLabel: label,
            (id)kSecValueRef: (__bridge id)idnt,
            (id)kSecAttrNoLegacy: @YES,
        };
        ok_status(SecItemAdd((CFDictionaryRef)query, NULL));
    }

    // Convert & Compare
    CFDataRef queriedPersistentIdntRef = NULL;
    SecIdentityRef queriedIdntRef = NULL;
    {
        NSDictionary *query = @{
            (id)kSecValueRef: (__bridge id)idnt,
            (id)kSecReturnPersistentRef: @YES,
            (id)kSecAttrNoLegacy: @YES,
        };
        ok_status(SecItemCopyMatching((CFDictionaryRef)query, (CFTypeRef *)&queriedPersistentIdntRef));
    }{
        NSDictionary *query = @{
            (id)kSecValuePersistentRef: (__bridge id)queriedPersistentIdntRef,
            (id)kSecReturnRef: @YES,
            (id)kSecAttrNoLegacy: @YES,
        };
        ok_status(SecItemCopyMatching((CFDictionaryRef)query, (CFTypeRef *)&queriedIdntRef));
    }{
        CFDataRef persistentIdntRef = NULL;
        SecKeychainItemRef idntRef = NULL;
        ok_status(SecKeychainItemCreatePersistentReference((SecKeychainItemRef)idnt, &persistentIdntRef));
        ok_status(SecKeychainItemCopyFromPersistentReference(persistentIdntRef, &idntRef));
        eq_cf(idnt, queriedIdntRef);
        eq_cf(idntRef, idnt);
        eq_cf(persistentIdntRef, queriedPersistentIdntRef);
        CFReleaseNull(persistentIdntRef);
        CFReleaseNull(idntRef);
    }
    CFReleaseNull(queriedPersistentIdntRef);
    CFReleaseNull(queriedIdntRef);

    // Cleanup
    {
        NSDictionary *query = @{
            // identities can't be filtered out using 'label', so we will use directly the ValueRef here:
            (id)kSecValueRef: (__bridge id)idnt,
            (id)kSecAttrNoLegacy: @YES,
        };
        ok_status(SecItemDelete((CFDictionaryRef)query));
        is_status(SecItemCopyMatching((CFDictionaryRef)query, NULL), errSecItemNotFound);
    }
    CFReleaseNull(idnt);
}

static void test_cssm_from_ios_key_single(CFTypeRef keyType, int keySize) {
    NSDictionary *params = @{
                             (id)kSecAttrKeyType: (__bridge id)keyType,
                             (id)kSecAttrKeySizeInBits: @(keySize),
                             (id)kSecAttrNoLegacy: @YES,
                             (id)kSecAttrIsPermanent: @NO,
                             (id)kSecAttrLabel: @"sectests:cssm-from-ios-key",
                             };
    NSError *error;
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    ok(privateKey != nil, "Failed to generate key, err %@", error);
    id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privateKey));
    ok(publicKey != nil, "Failed to get public key from private key");

    const CSSM_KEY *cssmKey = NULL;
    OSStatus status = SecKeyGetCSSMKey((SecKeyRef)publicKey, &cssmKey);
    ok_status(status, "Failed to get CSSM key");
    isnt(cssmKey, NULL, "Got NULL CSSM key");

    CSSM_CSP_HANDLE cspHandle = 0;
    status = SecKeyGetCSPHandle((SecKeyRef)publicKey, &cspHandle);
    ok_status(status, "Failed to get CSP handle");
    isnt(cssmKey, NULL, "Got 0 CSP handle");
}
static const int kTestCSSMFromIOSKeySingleCount = 5;

static void test_cssm_from_ios_key() {
    test_cssm_from_ios_key_single(kSecAttrKeyTypeECSECPrimeRandom, 256);
    test_cssm_from_ios_key_single(kSecAttrKeyTypeECSECPrimeRandom, 384);
    test_cssm_from_ios_key_single(kSecAttrKeyTypeECSECPrimeRandom, 521);
    test_cssm_from_ios_key_single(kSecAttrKeyTypeRSA, 1024);
    test_cssm_from_ios_key_single(kSecAttrKeyTypeRSA, 2048);
}
static const int kTestCSSMFromIOSKeyCount = kTestCSSMFromIOSKeySingleCount * 5;

static const int kTestCount =
    kTestGenerateNoLegacyCount +
    kTestGenerateAccessControlCount +
    kTestAddIOSKeyCount +
    kTestStoreCertToIOS +
    kTestStoreIdentityToIOS +
    kTestTransformWithIOSKey +
    kTestConvertKeyToPersistentRef +
    kTestConvertCertToPersistentRef +
    kTestConvertIdentityToPersistentRef +
    kTestCSSMFromIOSKeyCount;

int kc_43_seckey_interop(int argc, char *const *argv) {
    plan_tests(kTestCount);

    test_generate_nolegacy();
#if !RC_HIDE_J79 && !RC_HIDE_J80
    test_generate_access_control();
#endif
    test_add_ios_key();
    test_store_cert_to_ios();
    test_store_identity_to_ios();
    test_transform_with_ioskey();
    test_convert_key_to_persistent_ref();
    test_convert_cert_to_persistent_ref();
    test_convert_identity_to_persistent_ref();
    test_cssm_from_ios_key();

    return 0;
}
