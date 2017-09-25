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

#include "utilities/SecCFRelease.h"
#include "utilities/array_size.h"

#include "transform_regressions.h"

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

static const int kTestTransformSignVerify = 85;

static void sign_verify_transform_from_key(SecKeyRef privateKey, SecKeyRef publicKey, bool expect_success) {
    // Create signature transform with private key
    CFErrorRef error=NULL;
    NSData *testData = [NSData dataWithBytes:"test" length:4];
    SecTransformRef signer = SecSignTransformCreate(privateKey, &error);

    ok(signer != nil, "transform execution succeeded");
    ok(expect_success==(error==NULL), "no error %@",error);
    CFReleaseNull(error);

    ok(expect_success==SecTransformSetAttribute(signer, kSecTransformInputAttributeName, (CFDataRef)testData, &error),
       "set input data to verify transform");
    ok(expect_success==(error==NULL), "error reported %@",error);
    CFReleaseNull(error);

    NSData *signature = (__bridge_transfer NSData *)SecTransformExecute(signer, &error);
    ok(expect_success==(signature!=nil), "create signature with transform");
    ok(expect_success==(error==NULL), "error reported %@",error);
    CFReleaseNull(error);

    // Create verify transform with public key.
    ok(publicKey != NULL, "get public key from private key");
    SecTransformRef verifier = SecVerifyTransformCreate(publicKey, (CFDataRef)signature, &error);
    ok(verifier, "create verification transform");
    ok(expect_success==(error==NULL), "no error %@",error);
    CFReleaseNull(error);

    ok(expect_success==SecTransformSetAttribute(verifier, kSecTransformInputAttributeName, (CFDataRef)testData, &error),
       "set input data to verify transform");
    ok(expect_success==(error==NULL), "no error %@",error);
    CFReleaseNull(error);

    NSNumber *result = (__bridge_transfer NSNumber *)SecTransformExecute(verifier, &error);
    ok(expect_success==(result!=nil), "transform execution succeeded");
    ok(expect_success==(error==NULL), "no error %@",error);
    ok(expect_success==result.boolValue, "transform verified signature");

    CFReleaseNull(signer);
    CFReleaseNull(verifier);
    CFReleaseNull(error);
}

static void createKeysWithUsage(SecKeyRef *privateKey, SecKeyRef *publicKey, CFTypeRef privKeyUsage, CFTypeRef pubKeyUsage, CFDataRef keyData) {


    CFArrayRef keyUsagesArray = NULL;
    // First the private key
    {
        CFArrayRef itemsRef = 0;
        CFTypeRef  keyAtt[1] = { kSecAttrIsExtractable };
        CFArrayRef keyAttArray = CFArrayCreate(NULL, keyAtt, 1, &kCFTypeArrayCallBacks);
        CFTypeRef  keyUsagesPriv[1] = { privKeyUsage };
        keyUsagesArray = CFArrayCreate(NULL, keyUsagesPriv, 1, &kCFTypeArrayCallBacks);
        SecItemImportExportKeyParameters priv_params={.keyUsage=keyUsagesArray,.keyAttributes=keyAttArray};

        ok_status(SecItemImport(keyData, 0, 0, 0, 0, &priv_params, 0, &itemsRef));
        NSArray *items_priv = CFBridgingRelease(itemsRef);
        is(items_priv.count,1);
        *privateKey = (SecKeyRef)CFBridgingRetain([items_priv objectAtIndex:0]);
        CFReleaseNull(keyUsagesArray);
        CFReleaseNull(keyAttArray);
    }

    // Public key
    {
        CFErrorRef error=NULL;
        CFArrayRef itemsRef = 0;
        CFTypeRef  keyUsagesPub[1] = { pubKeyUsage };
        SecKeyRef  publicKey_tmp = SecKeyCopyPublicKey(*privateKey); // It does not have the attribute we want
        CFDataRef  publicKeyData = SecKeyCopyExternalRepresentation(publicKey_tmp,&error); // Extract the external representation and import with attributes
        keyUsagesArray = CFArrayCreate(NULL, keyUsagesPub, 1, &kCFTypeArrayCallBacks);
        SecItemImportExportKeyParameters pub_params={.keyUsage=keyUsagesArray};

        ok_status(SecItemImport(publicKeyData, 0, 0, 0, 0, &pub_params, 0, &itemsRef));
        NSArray *items_pub = CFBridgingRelease(itemsRef);
        is(items_pub.count,1);
        *publicKey = (SecKeyRef)CFBridgingRetain([items_pub objectAtIndex:0]);
        CFReleaseNull(keyUsagesArray);
        CFReleaseNull(publicKeyData);
        CFReleaseNull(publicKey_tmp);
    }
}



static void test_transform_sign_verify() {
    SecKeyRef privateKey = NULL;
    SecKeyRef publicKey = NULL;
    // Create private key instance.
    NSData *keyData = [[NSData alloc] initWithBase64EncodedString:[NSString stringWithUTF8String:keyDataBase64]
                                                          options:NSDataBase64DecodingIgnoreUnknownCharacters];

    // Import key with SecKeyCreateWithData
    {
        NSDictionary *keyAttrs = @{ (id)kSecAttrKeyType: (id)kSecAttrKeyTypeRSA, (id)kSecAttrKeySizeInBits: @2048,
                                    (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate };
        privateKey = SecKeyCreateWithData((CFDataRef)keyData, (CFDictionaryRef)keyAttrs, NULL);
        ok(privateKey != NULL, "create private key from data");
        publicKey = SecKeyCopyPublicKey(privateKey);
        sign_verify_transform_from_key(privateKey,publicKey,true);
        CFReleaseNull(privateKey);
        CFReleaseNull(publicKey);
    }

    // Import the key with default attribute (no key Usage specified)
    {
        CFArrayRef itemsRef = 0;
        CFTypeRef  keyAtt[1] = { kSecAttrIsExtractable };
        CFArrayRef keyAttArray = CFArrayCreate(NULL, keyAtt, 1, &kCFTypeArrayCallBacks);
        SecItemImportExportKeyParameters params={.keyAttributes=keyAttArray};
        ok_status(SecItemImport((__bridge CFDataRef)keyData, 0, 0, 0, 0, &params, 0, &itemsRef));
        NSArray *items = CFBridgingRelease(itemsRef);
        is(items.count,1);
        privateKey = ((SecKeyRef)CFBridgingRetain([items objectAtIndex:0]));
        SecKeyRef  publicKey = SecKeyCopyPublicKey(privateKey);
        sign_verify_transform_from_key(privateKey,publicKey,true);
        CFReleaseNull(privateKey);
        CFReleaseNull(publicKey);
    }

    // Import the key with sign attribute
    {
        createKeysWithUsage(&privateKey,&publicKey,kSecAttrCanSign,kSecAttrCanVerify,(__bridge CFDataRef)keyData);
        sign_verify_transform_from_key(privateKey,publicKey,true);
        CFReleaseNull(privateKey);
        CFReleaseNull(publicKey);
    }

    // Import the key with sign attribute
    {
        createKeysWithUsage(&privateKey,&publicKey,kSecAttrCanSign,kSecAttrCanVerify,(__bridge CFDataRef)keyData);
        sign_verify_transform_from_key(privateKey,publicKey,true);
        CFReleaseNull(privateKey);
        CFReleaseNull(publicKey);
    }

    // Import the key with enc attribute
    {
        createKeysWithUsage(&privateKey,&publicKey,kSecAttrCanDecrypt,kSecAttrCanEncrypt,(__bridge CFDataRef)keyData);
        sign_verify_transform_from_key(privateKey,publicKey,false);
        CFReleaseNull(privateKey);
        CFReleaseNull(publicKey);
    }
}

static const int kTestCount = kTestTransformSignVerify;

int transform_01_sigverify(int argc, char *const *argv) {
    plan_tests(kTestCount);

    test_transform_sign_verify();
    
    return 0;
}
