/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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


#import <Foundation/Foundation.h>

#include "shared_regressions.h"

static void create_random_key_worker(id keyType, int keySize, bool permPub, bool permPriv) {
    NSDictionary *params = nil;
    NSError *error = nil;

    params = @{
        (id)kSecAttrKeyType: keyType,
        (id)kSecAttrKeySizeInBits: @(keySize),
        (id)kSecAttrLabel: @"si-44-seckey-gen:0",
        (id)kSecPublicKeyAttrs: @{
            (id)kSecAttrIsPermanent: @(permPub),
        },
        (id)kSecPrivateKeyAttrs: @{
            (id)kSecAttrIsPermanent: @(permPriv),
        },
    };

    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    ok(privateKey != nil, "generating key (type:%@, size:%d, permPub:%d, permPriv:%d) : %@", keyType, keySize, (int)permPub, (int)permPriv, error);

    id publicKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privateKey));
    ok(publicKey != nil, "got public key from generated private key");

    error = nil;
    NSData *pubKeyData = CFBridgingRelease(SecKeyCopyExternalRepresentation((SecKeyRef)publicKey, (void *)&error));
    ok(pubKeyData != nil, "get public key data, error %@", error);

    params = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyType: keyType,
        (id)kSecAttrKeySizeInBits: @(keySize),
        (id)kSecAttrLabel: @"si-44-seckey-gen:0",
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
        (id)kSecReturnAttributes: @YES,
    };
    NSArray *items = nil;
    OSStatus expected = (permPub || permPriv) ? errSecSuccess : errSecItemNotFound;
    is_status(SecItemCopyMatching((CFDictionaryRef)params, (void *)&items), expected, "keychain query for generated keys");
    is((int)items.count, (permPub ? 1 : 0) + (permPriv ? 1 : 0), "found keys in the keychain");

    if (!permPub) {
        // Store public key into keychain explicitly.
        NSDictionary *add = @{
            (id)kSecValueRef: (id)publicKey,
            (id)kSecAttrLabel: @"si-44-seckey-gen:0",
            (id)kSecReturnPersistentRef: @YES,
        };
        NSData *pRef;
        ok_status(SecItemAdd((CFDictionaryRef)add, (void *)&pRef), "Add public key");
        ok([pRef isKindOfClass:NSData.class] || [pRef isKindOfClass:NSArray.class] , "got persistent ref");
    } else {
        ok(true);
        ok(true);
    }

    if (!permPriv) {
        // Store public key into keychain explicitly.
        NSDictionary *add = @{
            (id)kSecValueRef: (id)privateKey,
            (id)kSecAttrLabel: @"si-44-seckey-gen:0",
            (id)kSecReturnPersistentRef: @YES,
        };
        NSData *pRef;
        ok_status(SecItemAdd((CFDictionaryRef)add, (void *)&pRef), "Add private key");
        ok([pRef isKindOfClass:NSData.class] || [pRef isKindOfClass:NSArray.class] , "got persistent ref");
    } else {
        ok(true);
        ok(true);
    }

    items = nil;
    ok_status(SecItemCopyMatching((CFDictionaryRef)params, (void *)&items), "keychain query for generated keys");
    is((int)items.count, 2, "keypair is in the keychain");

    params = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyType: keyType,
        (id)kSecAttrKeySizeInBits: @(keySize),
#if TARGET_OS_OSX
        // Despite headerdoc and other docs, SecItemDelete on macOS deletes only first found item, we need to persuade
        // it to delete everything passing the query.  On the other hand, iOS implementation errs out when
        // kSecMatchLimit is given, so we need to add it only for macOS.
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
#endif
        (id)kSecAttrLabel: @"si-44-seckey-gen:0",
    };
    ok_status(SecItemDelete((CFDictionaryRef)params), "clear generated pair from keychain");
}

static void test_create_random_key() {
    create_random_key_worker((id)kSecAttrKeyTypeRSA, 1024, false, false);
    create_random_key_worker((id)kSecAttrKeyTypeRSA, 1024, false, true);
    create_random_key_worker((id)kSecAttrKeyTypeRSA, 1024, true, true);
    create_random_key_worker((id)kSecAttrKeyTypeECSECPrimeRandom, 256, false, false);
    create_random_key_worker((id)kSecAttrKeyTypeECSECPrimeRandom, 256, false, true);
    create_random_key_worker((id)kSecAttrKeyTypeECSECPrimeRandom, 256, true, true);

    // Create pair with pub-permanent&priv-nonpermanent currently does not work well, created
    // pub key has some strange ACL which prevents it from sensible use.  However, this combination
    // also does not make sense at all from usage point of view.
//    create_random_key_worker((id)kSecAttrKeyTypeRSA, 1024, true, false);
//    create_random_key_worker((id)kSecAttrKeyTypeECSECPrimeRandom, 256, true, false);
}
static const int TestCountCreateRandomKey = (3 * 12) * 2;

static void test_create_key_with_attrs_run(NSDictionary *attrs) {
    NSMutableDictionary *params = attrs.mutableCopy;
    params[(id)kSecAttrKeyType] = (id)kSecAttrKeyTypeRSA;
    params[(id)kSecAttrKeySizeInBits] = @1024;
    params[(id)kSecAttrIsPermanent] = @YES;

    NSError *error;
    id privKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)params, (void *)&error));
    ok(privKey != nil, "generatekey: %@", privKey);

    id pubKey = CFBridgingRelease(SecKeyCopyPublicKey((SecKeyRef)privKey));
    ok(pubKey != nil, "get pub key");

    NSDictionary *result;
    ok_status(SecItemCopyMatching((CFDictionaryRef)@{(id)kSecValueRef: privKey, (id)kSecReturnAttributes: @YES}, (void *)&result), "copymatching create privkey");
    __block BOOL failed = NO;
    [attrs enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        failed = failed || ![result[key] isEqual:obj];
    }];
    ok(!failed, "retrieved attributes differ (privkey): expected: %@ - got: %@", attrs, result);

    result = nil;
    ok_status(SecItemCopyMatching((CFDictionaryRef)@{(id)kSecValueRef: pubKey, (id)kSecReturnAttributes: @YES}, (void *)&result), "copymatching create privkey");
    failed = NO;
    [attrs enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        failed = failed || ![result[key] isEqual:obj];
    }];
    ok(!failed, "retrieved attributes differ (pubkey): expected: %@ - got: %@", attrs, result);

    // Cleanup of the keys.
    ok_status(SecItemDelete((CFDictionaryRef)@{(id)kSecValueRef: privKey}));
    ok_status(SecItemDelete((CFDictionaryRef)@{(id)kSecValueRef: pubKey}));
}
static const int TestCountCreateKeyWithAttrsRun = 8;

static void test_create_key_with_attrs() {
    test_create_key_with_attrs_run(@{(id)kSecAttrLabel:@"Label"});
    test_create_key_with_attrs_run(@{(id)kSecAttrApplicationTag:[@"AppTag" dataUsingEncoding:NSUTF8StringEncoding]});
    test_create_key_with_attrs_run(@{(id)kSecAttrLabel:@"Label",
                                     (id)kSecAttrApplicationTag:[@"AppTag" dataUsingEncoding:NSUTF8StringEncoding]});
}
static const int TestCountCreateKeyWithAttrs = 3 * TestCountCreateKeyWithAttrsRun;

static const int TestCount =
TestCountCreateRandomKey +
TestCountCreateKeyWithAttrs
;

int si_44_seckey_gen(int argc, char *const *argv) {
    plan_tests(TestCount);

    @autoreleasepool {
        test_create_random_key();
        test_create_key_with_attrs();
    }

    return 0;
}
