/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
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


#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecFramework.h>
#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecECKey.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#include <Security/SecInternal.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>

#include <libaks_acl_cf_keys.h>

#include <ctkclient_test.h>
#include <coreauthd_spi.h>

#include "secd_regressions.h"

#include "SecdTestKeychainUtilities.h"
#include "SecKeybagSupport.h"

extern void LASetErrorCodeBlock(CFErrorRef (^newCreateErrorBlock)(void));

static void test_item_add(void) {

    static const UInt8 data[] = { 0x01, 0x02, 0x03, 0x04 };
    CFDataRef valueData = CFDataCreate(NULL, data, sizeof(data));
    static const UInt8 oid[] = { 0x05, 0x06, 0x07, 0x08 };
    CFDataRef oidData = CFDataCreate(NULL, oid, sizeof(oid));

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                                           kSecClass, kSecClassGenericPassword,
                                                                           kSecAttrTokenID, CFSTR("tokenid"),
                                                                           kSecAttrService, CFSTR("ctktest-service"),
                                                                           kSecValueData, valueData,
                                                                           kSecReturnAttributes, kCFBooleanTrue,
                                                                           NULL);
    // Setup token hook.
    __block int phase = 0;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;
        eq_cf(CFDictionaryGetValue(attributes, kSecAttrTokenID), CFSTR("tokenid"));

        blocks->createOrUpdateObject = Block_copy(^CFDataRef(CFDataRef objectID, CFMutableDictionaryRef at, CFErrorRef *error) {
            phase++;
            is(objectID, NULL);
            eq_cf(CFDictionaryGetValue(at, kSecClass), kSecClassGenericPassword);
            eq_cf(CFDictionaryGetValue(at, kSecAttrService), CFDictionaryGetValue(attrs, kSecAttrService));
            eq_cf(CFDictionaryGetValue(at, kSecAttrTokenID), CFSTR("tokenid"));
            eq_cf(CFDictionaryGetValue(at, kSecValueData), valueData);
            CFDictionaryRemoveValue(at, kSecValueData);
            return CFRetainSafe(oidData);
        });

        blocks->copyObjectAccessControl = Block_copy(^CFDataRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            SecAccessControlRef ac = SecAccessControlCreate(NULL, NULL);
            SecAccessControlSetProtection(ac, kSecAttrAccessibleAlways, NULL);
            SecAccessControlAddConstraintForOperation(ac, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL);
            CFDataRef acData = SecAccessControlCopyData(ac);
            CFRelease(ac);
            return acData;
        });

        blocks->copyObjectData = Block_copy(^CFTypeRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            return CFRetain(valueData);
        });
    });

    CFTypeRef result = NULL;
    ok_status(SecItemAdd(attrs, &result));
    eq_cf(CFDictionaryGetValue(result, kSecAttrService), CFSTR("ctktest-service"));
    eq_cf(CFDictionaryGetValue(result, kSecAttrTokenID), CFSTR("tokenid"));
    is(CFDictionaryGetValue(result, kSecValueData), NULL);
    CFReleaseNull(result);

    is(phase, 3);

    phase = 0;
    CFDictionarySetValue(attrs, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(attrs, kSecAttrService, CFSTR("ctktest-service1"));
    ok_status(SecItemAdd(attrs, &result));
    eq_cf(CFDictionaryGetValue(result, kSecAttrService), CFSTR("ctktest-service1"));
    eq_cf(CFDictionaryGetValue(result, kSecAttrTokenID), CFSTR("tokenid"));
    eq_cf(CFDictionaryGetValue(result, kSecValueData), valueData);
    CFReleaseNull(result);

    is(phase, 4);

    phase = 0;
    CFDictionaryRemoveValue(attrs, kSecReturnAttributes);
    CFDictionarySetValue(attrs, kSecAttrAccount, CFSTR("2nd"));
    ok_status(SecItemAdd(attrs, &result));
    eq_cf(result, valueData);
    CFReleaseNull(result);
    is(phase, 4);

    CFRelease(attrs);
    CFRelease(valueData);
    CFRelease(oidData);
}
static const int kItemAddTestCount = 31;

static void test_item_query() {
    static const UInt8 oid[] = { 0x05, 0x06, 0x07, 0x08 };
    CFDataRef oidData = CFDataCreate(NULL, oid, sizeof(oid));
    static const UInt8 data[] = { 0x01, 0x02, 0x03, 0x04 };
    CFDataRef valueData = CFDataCreate(NULL, data, sizeof(data));
    CFDataRef valueData2 = CFDataCreate(NULL, data, sizeof(data) - 1);

    __block int phase = 0;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;
        eq_cf(CFDictionaryGetValue(attributes, kSecAttrTokenID), CFSTR("tokenid"));

        blocks->copyObjectData = _Block_copy(^CFTypeRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            return CFRetain(valueData);
        });
    });

    // Add non-token item with the same service, to test queries returning mixed results.
    CFMutableDictionaryRef attrs = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                                           kSecClass, kSecClassGenericPassword,
                                                                           kSecAttrService, CFSTR("ctktest-service"),
                                                                           kSecValueData, valueData2,
                                                                           NULL);
    ok_status(SecItemAdd(attrs, NULL));
    CFRelease(attrs);

    // Query with service.
    CFMutableDictionaryRef query;
    query = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                    kSecClass, kSecClassGenericPassword,
                                                    kSecAttrService, CFSTR("ctktest-service"),
                                                    kSecReturnAttributes, kCFBooleanTrue,
                                                    kSecReturnData, kCFBooleanTrue,
                                                    NULL);

    phase = 0;
    CFTypeRef result = NULL;
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 2);
    is(CFGetTypeID(result), CFDictionaryGetTypeID());
    eq_cf(CFDictionaryGetValue(result, kSecValueData), valueData);
    is(CFGetTypeID(CFDictionaryGetValue(result, kSecAttrAccessControl)), SecAccessControlGetTypeID());
    eq_cf(CFDictionaryGetValue(result, kSecAttrService), CFSTR("ctktest-service"));
    CFReleaseSafe(result);

    phase = 0;
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 2);
    is(CFGetTypeID(result), CFArrayGetTypeID());
    is(CFArrayGetCount(result), 2);
    CFReleaseSafe(result);

    phase = 0;
    CFDictionaryRemoveValue(query, kSecMatchLimit);
    CFDictionaryRemoveValue(query, kSecReturnData);
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 0);
    is(CFGetTypeID(result), CFDictionaryGetTypeID());
    is(CFDictionaryGetValue(result, kSecValueData), NULL);
    CFReleaseSafe(result);

    phase = 0;
    CFDictionaryRemoveValue(query, kSecReturnAttributes);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecAttrTokenID, CFSTR("tokenid"));
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 2);
    eq_cf(result, valueData);
    CFReleaseSafe(result);

    CFRelease(query);
    CFRelease(valueData);
    CFRelease(valueData2);
    CFRelease(oidData);
}
static const int kItemQueryTestCount = 21;

static void test_item_update() {
    static const UInt8 data[] = { 0x01, 0x02, 0x03, 0x04 };
    CFDataRef valueData2 = CFDataCreate(NULL, data, sizeof(data) - 1);
    CFTypeRef result = NULL;

    CFMutableDictionaryRef query, attrs;

    // Setup token hook.
    __block int phase = 0;
    __block bool store_value = false;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;
        eq_cf(CFDictionaryGetValue(attributes, kSecAttrTokenID), CFSTR("tokenid"));

        blocks->createOrUpdateObject = Block_copy(^CFDataRef(CFDataRef objectID, CFMutableDictionaryRef at, CFErrorRef *error) {
            phase++;
            eq_cf(CFDictionaryGetValue(at, kSecValueData), valueData2);
            if (!store_value) {
                CFDictionaryRemoveValue(at, kSecValueData);
            }
            return CFRetainSafe(objectID);
        });

        blocks->copyObjectAccessControl = Block_copy(^CFDataRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            SecAccessControlRef ac = SecAccessControlCreate(NULL, NULL);
            SecAccessControlSetProtection(ac, kSecAttrAccessibleAlways, NULL);
            SecAccessControlAddConstraintForOperation(ac, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL);
            CFDataRef acData = SecAccessControlCopyData(ac);
            CFRelease(ac);
            return acData;
        });

        blocks->copyObjectData = Block_copy(^CFTypeRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            return CFRetain(valueData2);
        });
    });

    query = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                    kSecClass, kSecClassGenericPassword,
                                                    kSecAttrTokenID, CFSTR("tokenid"),
                                                    kSecAttrService, CFSTR("ctktest-service"),
                                                    NULL);

    attrs = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                    kSecValueData, valueData2,
                                                    NULL);

    ok_status(SecItemUpdate(query, attrs));
    is(phase, 3);

    phase = 0;
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query, &result));
    eq_cf(valueData2, result);
    CFRelease(result);
    is(phase, 2);

    phase = 0;
    store_value = true;
    CFDictionaryRemoveValue(query, kSecReturnData);
    ok_status(SecItemUpdate(query, attrs));
    is(phase, 3);

    phase = 0;
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query, &result));
    eq_cf(valueData2, result);
    CFRelease(result);
    is(phase, 0);

    phase = 0;
    CFDictionarySetValue(query, kSecAttrService, CFSTR("ctktest-service1"));
    CFDictionaryRemoveValue(query, kSecReturnData);
    ok_status(SecItemUpdate(query, attrs));
    is(phase, 5);

    phase = 0;
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 0);
    is(CFGetTypeID(result), CFArrayGetTypeID());
    is(CFArrayGetCount(result), 2);
    eq_cf(CFArrayGetValueAtIndex(result, 0), valueData2);
    eq_cf(CFArrayGetValueAtIndex(result, 1), valueData2);

    CFRelease(query);
    CFRelease(attrs);
    CFRelease(valueData2);
}
static const int kItemUpdateTestCount = 26;

static void test_item_delete(void) {

    CFMutableDictionaryRef query;
    CFTypeRef result;

    __block int phase = 0;
    __block CFErrorRef deleteError = NULL;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;
        eq_cf(CFDictionaryGetValue(attributes, kSecAttrTokenID), CFSTR("tokenid"));

        blocks->copyObjectAccessControl = _Block_copy(^CFDataRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            SecAccessControlRef ac = SecAccessControlCreate(NULL, NULL);
            SecAccessControlSetProtection(ac, kSecAttrAccessibleAlways, NULL);
            SecAccessControlAddConstraintForOperation(ac, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL);
            CFDataRef acData = SecAccessControlCopyData(ac);
            CFRelease(ac);
            return acData;
        });

        blocks->deleteObject = _Block_copy(^bool(CFDataRef objectID, CFErrorRef *error) {
            phase++;
            if (deleteError != NULL) {
                CFAssignRetained(*error, deleteError);
                deleteError = NULL;
                return false;
            }
            return true;
        });
    });

    query = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                    kSecClass, kSecClassGenericPassword,
                                                    kSecAttrTokenID, CFSTR("tokenid"),
                                                    kSecAttrService, CFSTR("ctktest-service"),
                                                    NULL);

    phase = 0;
    ok_status(SecItemDelete(query));
    is(phase, 2);

    phase = 0;
    is_status(SecItemCopyMatching(query, &result), errSecItemNotFound);
    is(phase, 0);

    phase = 0;
    CFDictionarySetValue(query, kSecAttrService, CFSTR("ctktest-service1"));
    ok_status(SecItemCopyMatching(query, &result));
    is(phase, 0);

    phase = 0;
#if LA_CONTEXT_IMPLEMENTED
    LASetErrorCodeBlock(^{ return (CFErrorRef)NULL; });
    deleteError = CFErrorCreate(NULL, CFSTR(kTKErrorDomain), kTKErrorCodeAuthenticationFailed, NULL);
    ok_status(SecItemDelete(query), "delete multiple token items");
    is(phase, 6, "connect + delete-auth-fail + copyAccess + connect + delete + delete-2nd");
#else
    ok_status(SecItemDelete(query), "delete multiple token items");
    is(phase, 3, "connect + delete + delete");
#endif

    phase = 0;
    is_status(SecItemCopyMatching(query, &result), errSecItemNotFound);
    is(phase, 0);

    is_status(SecItemDelete(query), errSecItemNotFound);

    CFRelease(query);
    CFReleaseSafe(deleteError);
}
#if LA_CONTEXT_IMPLEMENTED
static const int kItemDeleteTestCount = 15;
#else
static const int kItemDeleteTestCount = 14;
#endif

static void test_key_generate(void) {

    __block int phase = 0;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;

        blocks->createOrUpdateObject = _Block_copy(^CFDataRef(CFDataRef objectID, CFMutableDictionaryRef at, CFErrorRef *error) {
            phase++;
            is(objectID, NULL);
            CFDictionarySetValue(at, kSecClass, kSecClassKey);
            SecKeyRef publicKey = NULL, privateKey = NULL;
            CFMutableDictionaryRef params = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                                                    kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                                                    kSecAttrKeySizeInBits, CFSTR("256"),
                                                                                    NULL);
            ok_status(SecKeyGeneratePair(params, &publicKey, &privateKey));
            CFDictionaryRef privKeyAttrs = SecKeyCopyAttributeDictionary(privateKey);
            CFRelease(privateKey);
            CFRelease(publicKey);
            CFRelease(params);
            CFDataRef oid = CFRetainSafe(CFDictionaryGetValue(privKeyAttrs, kSecValueData));
            CFRelease(privKeyAttrs);
            return oid;
        });

        blocks->copyObjectAccessControl = _Block_copy(^CFDataRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            SecAccessControlRef ac = SecAccessControlCreate(NULL, NULL);
            SecAccessControlSetProtection(ac, kSecAttrAccessibleAlways, NULL);
            SecAccessControlAddConstraintForOperation(ac, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL);
            CFDataRef acData = SecAccessControlCopyData(ac);
            CFRelease(ac);
            return acData;
        });

        blocks->copyPublicKeyData = _Block_copy(^CFDataRef(CFDataRef objectID, CFErrorRef *error) {
            phase++;
            SecKeyRef privKey = SecKeyCreateECPrivateKey(NULL, CFDataGetBytePtr(objectID), CFDataGetLength(objectID), kSecKeyEncodingBytes);
            CFDataRef publicData;
            ok_status(SecKeyCopyPublicBytes(privKey, &publicData));
            CFRelease(privKey);
            return publicData;
        });

        blocks->copyObjectData = _Block_copy(^CFTypeRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            return kCFNull;
        });
    });

    CFDictionaryRef prk_params = CFDictionaryCreateForCFTypes(NULL,
                                                              kSecAttrIsPermanent, kCFBooleanTrue,
                                                              NULL);

    CFMutableDictionaryRef params = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                                            kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                                            kSecAttrKeySizeInBits, CFSTR("256"),
                                                                            kSecAttrTokenID, CFSTR("tokenid"),
                                                                            kSecPrivateKeyAttrs, prk_params,
                                                                            NULL);
    CFRelease(prk_params);

    SecKeyRef publicKey = NULL, privateKey = NULL;
    phase = 0;
    ok_status(SecKeyGeneratePair(params, &publicKey, &privateKey));
    is(phase, 5);

    CFDictionaryRef query = CFDictionaryCreateForCFTypes(NULL,
                                                         kSecValueRef, privateKey,
                                                         kSecReturnAttributes, kCFBooleanTrue,
                                                         kSecReturnRef, kCFBooleanTrue,
                                                         kSecReturnData, kCFBooleanTrue,
                                                         NULL);
    phase = 0;
    CFDictionaryRef result = NULL, keyAttrs = NULL;
    ok_status(SecItemCopyMatching(query, (CFTypeRef *)&result));
    is(phase, 3);
    is(CFDictionaryGetValue(result, kSecValueData), NULL);
    eq_cf(CFDictionaryGetValue(result, kSecAttrTokenID), CFSTR("tokenid"));
    keyAttrs = SecKeyCopyAttributeDictionary((SecKeyRef)CFDictionaryGetValue(result, kSecValueRef));
    eq_cf(CFDictionaryGetValue(keyAttrs, kSecAttrApplicationLabel), CFDictionaryGetValue(result, kSecAttrApplicationLabel));
    CFAssignRetained(keyAttrs, SecKeyCopyAttributeDictionary(publicKey));
    eq_cf(CFDictionaryGetValue(keyAttrs, kSecAttrApplicationLabel), CFDictionaryGetValue(result, kSecAttrApplicationLabel));

    CFRelease(result);
    CFRelease(keyAttrs);
    CFRelease(publicKey);
    CFRelease(privateKey);

    CFRelease(query);
    CFRelease(params);
}
static const int kKeyGenerateTestCount = 14;

static void test_key_sign(void) {

    static const UInt8 data[] = { 0x01, 0x02, 0x03, 0x04 };
    CFDataRef valueData = CFDataCreate(NULL, data, sizeof(data));

    __block int phase = 0;
    __block CFErrorRef signError = NULL;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;

        blocks->copyPublicKeyData = _Block_copy(^CFDataRef(CFDataRef objectID, CFErrorRef *error) {
            phase++;
            SecKeyRef privKey = SecKeyCreateECPrivateKey(NULL, CFDataGetBytePtr(objectID), CFDataGetLength(objectID), kSecKeyEncodingBytes);
            CFDataRef publicData;
            ok_status(SecKeyCopyPublicBytes(privKey, &publicData));
            CFRelease(privKey);
            return publicData;
        });

        blocks->copyObjectAccessControl = _Block_copy(^CFDataRef(CFDataRef oid, CFErrorRef *error) {
            phase++;
            SecAccessControlRef ac = SecAccessControlCreate(NULL, NULL);
            SecAccessControlSetProtection(ac, kSecAttrAccessibleAlways, NULL);
            SecAccessControlAddConstraintForOperation(ac, kAKSKeyOpDefaultAcl, kCFBooleanTrue, NULL);
            CFDataRef acData = SecAccessControlCopyData(ac);
            CFRelease(ac);
            return acData;
        });

        blocks->copySignature = _Block_copy(^CFDataRef(CFDataRef objectID, CFIndex padding, CFDataRef plainText, CFErrorRef *error) {
            phase++;
            if (signError != NULL) {
                CFAssignRetained(*error, signError);
                signError = NULL;
                return NULL;
            }
            return CFRetainSafe(valueData);
        });
    });

    CFDictionaryRef query = CFDictionaryCreateForCFTypes(NULL,
                                                         kSecClass, kSecClassKey,
                                                         kSecReturnRef, kCFBooleanTrue,
                                                         NULL);
    phase = 0;
    SecKeyRef privateKey = NULL;
    ok_status(SecItemCopyMatching(query, (CFTypeRef *)&privateKey));
    is(phase, 1);

    phase = 0;
    CFMutableDataRef sig = CFDataCreateMutable(NULL, 0);
    CFDataSetLength(sig, 256);
    size_t sigLen = CFDataGetLength(sig);
    ok_status(SecKeyRawSign(privateKey, kSecPaddingNone, data, sizeof(data), CFDataGetMutableBytePtr(sig), &sigLen));
    is(phase, 1);
    CFDataSetLength(sig, sigLen);
    is(CFDataGetLength(sig), CFDataGetLength(valueData));
    eq_cf(valueData, sig);

#if LA_CONTEXT_IMPLEMENTED
    phase = 0;
    CFDataSetLength(sig, 256);
    sigLen = CFDataGetLength(sig);
    LASetErrorCodeBlock(^ { return (CFErrorRef)NULL; });
    signError = CFErrorCreate(NULL, CFSTR(kTKErrorDomain), kTKErrorCodeAuthenticationFailed, NULL);
    ok_status(SecKeyRawSign(privateKey, kSecPaddingNone, data, sizeof(data), CFDataGetMutableBytePtr(sig), &sigLen));
    is(phase, 4);
    is(signError, NULL);
    CFDataSetLength(sig, sigLen);
    is(CFDataGetLength(sig), CFDataGetLength(valueData));
    eq_cf(valueData, sig);
#endif

    CFReleaseSafe(signError);
    CFRelease(sig);
    CFRelease(privateKey);
    CFRelease(query);
}
#if LA_CONTEXT_IMPLEMENTED
static const int kKeySignTestCount = 11;
#else
static const int kKeySignTestCount = 6;
#endif

static void test_key_generate_with_params(void) {

    const UInt8 data[] = "foo";
    CFDataRef cred_ref = CFDataCreate(NULL, data, 4);
    __block int phase = 0;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        phase++;
        eq_cf(CFDictionaryGetValue(attributes, kSecUseOperationPrompt), CFSTR("prompt"));
        is(CFDictionaryGetValue(attributes, kSecUseAuthenticationUI), NULL);
        eq_cf(CFDictionaryGetValue(attributes, kSecUseCredentialReference), cred_ref);

        blocks->createOrUpdateObject = _Block_copy(^CFDataRef(CFDataRef objectID, CFMutableDictionaryRef at, CFErrorRef *error) {
            phase++;
            SecCFCreateError(-4 /* kTKErrorCodeCanceledByUser */, CFSTR(kTKErrorDomain), CFSTR(""), NULL, error);
            return NULL;
        });
    });

    CFDictionaryRef prk_params = CFDictionaryCreateForCFTypes(NULL,
                                                              kSecAttrIsPermanent, kCFBooleanTrue,
                                                              NULL);

    CFMutableDictionaryRef params = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                                            kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                                            kSecAttrKeySizeInBits, CFSTR("256"),
                                                                            kSecAttrTokenID, CFSTR("tokenid"),
                                                                            kSecPrivateKeyAttrs, prk_params,
                                                                            kSecUseOperationPrompt, CFSTR("prompt"),
                                                                            kSecUseAuthenticationUI, kSecUseAuthenticationUIAllow,
                                                                            kSecUseCredentialReference, cred_ref,
                                                                            NULL);
    CFRelease(prk_params);

    SecKeyRef publicKey = NULL, privateKey = NULL;
    phase = 0;
    diag("This will produce an internal assert - on purpose");
    is_status(SecKeyGeneratePair(params, &publicKey, &privateKey), errSecUserCanceled);
    is(phase, 2);

    CFReleaseSafe(publicKey);
    CFReleaseSafe(privateKey);
    CFRelease(params);
    CFRelease(cred_ref);
}
static const int kKeyGenerateWithParamsTestCount = 5;

static void test_error_codes(void) {

    CFMutableDictionaryRef attrs = CFDictionaryCreateMutableForCFTypesWith(NULL,
                                                                           kSecClass, kSecClassGenericPassword,
                                                                           kSecAttrTokenID, CFSTR("tokenid"),
                                                                           NULL);
    // Setup token hook.
    __block OSStatus ctk_error = 0;
    TKTokenTestSetHook(^(CFDictionaryRef attributes, TKTokenTestBlocks *blocks) {
        blocks->createOrUpdateObject = _Block_copy(^CFDataRef(CFDataRef objectID, CFMutableDictionaryRef at, CFErrorRef *error) {
            SecCFCreateError(ctk_error, CFSTR(kTKErrorDomain), CFSTR(""), NULL, error);
            return NULL;
        });
    });

    ctk_error = kTKErrorCodeBadParameter;
    is_status(SecItemAdd(attrs, NULL), errSecParam);

    ctk_error = -1 /* kTKErrorCodeNotImplemented */;
    is_status(SecItemAdd(attrs, NULL), errSecUnimplemented);

    ctk_error = -4 /* kTKErrorCodeCanceledByUser */;
    is_status(SecItemAdd(attrs, NULL), errSecUserCanceled);

    CFRelease(attrs);
}
static const int kErrorCodesCount = 3;

static void tests(void) {
    /* custom keychain dir */
    secd_test_setup_temp_keychain("secd_33_keychain_ctk", NULL);

    test_item_add();
    test_item_query();
    test_item_update();
    test_item_delete();
    test_key_generate();
    test_key_sign();
    test_key_generate_with_params();
    test_error_codes();
}

int secd_33_keychain_ctk(int argc, char *const *argv) {
    plan_tests(kItemAddTestCount +
               kItemQueryTestCount +
               kItemUpdateTestCount +
               kItemDeleteTestCount +
               kKeyGenerateTestCount +
               kKeySignTestCount +
               kKeyGenerateWithParamsTestCount +
               kErrorCodesCount +
               kSecdTestSetupTestCount);
    tests();

    return 0;
}
