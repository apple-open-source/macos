//
//  mockaksSystemKeychain.m
//  Security
//

#import <XCTest/XCTest.h>
#import "mockaksxcbase.h"

#include <Security/SecItemPriv.h>
#import "ipc/securityd_client.h"
#import <utilities/SecFileLocations.h>
#import "featureflags/featureflags.h" // To override SystemKeychainAlways FF

@interface mockaksSystemKeychain : mockaksxcbase
@end

@implementation mockaksSystemKeychain

- (void)tearDown {
    _SecSystemKeychainAlwaysClearOverride();
}

#if TARGET_OS_IPHONE || TARGET_OS_OSX
/*
 * Test add api in all its variants, using kSecUseSystemKeychain.
 * Expected behavior is different in edu mode because we pay attention to kSecUseSystemKeychain.
 */
- (void)helperOldSystemKeychainFlagWithEduMode:(bool)eduMode
{
    NSDictionary *item, *query, *attrsToUpdate;
    CFTypeRef result = NULL;

#if TARGET_OS_IOS
    if (eduMode)
        SecSecuritySetMusrMode(true, 502, 502);
#endif

    /*
     * first clean some
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
    };
    SecItemDelete((CFDictionaryRef)query);

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
    };
    SecItemDelete((CFDictionaryRef)query);

    /*
     * Add entry
     */

    item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
    };

    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecSuccess, "SecItemAdd kSecUseSystemKeychain edumode:%d", eduMode);

    /*
     * Check we can't find it in our keychain
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecReturnAttributes: @YES,
    };

    XCTAssertEqual(SecItemCopyMatching((CFTypeRef)query, &result), eduMode ? errSecItemNotFound : noErr, "SecItemCopyMatching kSecUseSystemKeychain edumode:%d", eduMode);

    if (eduMode) {
        XCTAssertNil((__bridge id)result, "SecItemCopyMatching kSecUseSystemKeychain edumode:%d unexpectedly returned a result", eduMode);
        XCTAssertEqual(SecItemDelete((CFTypeRef)query), errSecItemNotFound, "SecItemDelete kSecUseSystemKeychain edumode:%d", eduMode);
    } else {
        XCTAssertEqual(CFGetTypeID(result), CFDictionaryGetTypeID(), "SecItemCopyMatching kSecUseSystemKeychain edumode:%d unexpectedly returned something that's not a dict", eduMode);
        XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrLabel], query[(id)kSecAttrLabel], "SecItemCopyMatching kSecUseSystemKeychain edumode:%d label doesn't match", eduMode);
    }

    CFReleaseNull(result);

    /*
     * Check we can find it in system keychain
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
    };

    attrsToUpdate = @{
        (id)kSecAttrComment : @"This is a test. This is only a test.",
    };

    XCTAssertEqual(SecItemUpdate((CFTypeRef)query, (CFTypeRef)attrsToUpdate), errSecSuccess, "SecItemUpdate(system) kSecUseSystemKeychain edumode:%d", eduMode);

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
        (id)kSecReturnAttributes: @YES,
    };

    XCTAssertEqual(SecItemCopyMatching((CFTypeRef)query, &result), errSecSuccess, "SecItemCopyMatching(system) kSecUseSystemKeychain edumode:%d", eduMode);
    XCTAssertNotEqual(result, NULL, "SecItemCopyMatching(system) kSecUseSystemKeychain edumode:%d returned NULL result", eduMode);
    if (result != NULL) { // prevent crashes, so other test will run
        XCTAssertEqual(CFGetTypeID(result), CFDictionaryGetTypeID(), "SecItemCopyMatching(system) kSecUseSystemKeychain edumode:%d unexpectedly returned something that's not a dict", eduMode);
        XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrLabel], query[(id)kSecAttrLabel], "SecItemCopyMatching(system) kSecUseSystemKeychain edumode:%d label doesn't match", eduMode);
        XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrComment], attrsToUpdate[(id)kSecAttrComment], "SecItemCopyMatching(system) kSecUseSystemKeychain edumode:%d comment doesn't match", eduMode);
        CFReleaseNull(result);
    }

    /* try to update the sync flag to true:
     * - in edu mode, the sync flag is set, but ignored (down in the sync infrastructure)
     * - not edu mode, the system flag is ignored (item added to user keychain)
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
    };

    attrsToUpdate = @{
        (id)kSecAttrSynchronizable : @YES,
    };

    XCTAssertEqual(SecItemUpdate((CFTypeRef)query, (CFTypeRef)attrsToUpdate), errSecSuccess, "SecItemUpdate(system) kSecUseSystemKeychain edumode:%d", eduMode);

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
        (id)kSecAttrSynchronizable : @YES,
    };

    XCTAssertEqual(SecItemDelete((CFTypeRef)query), errSecSuccess, "SecItemDelete(system) kSecUseSystemKeychain+sync edumode:%d", eduMode);

    /*
     * Add entry with both system and sync flags:
     * - in edu mode, the sync flag is set, but ignored (down in the sync infrastructure)
     * - not edu mode, the system flag is ignored (item added to user keychain)
     */

    item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
        (id)kSecAttrSynchronizable : @YES,
    };

    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecSuccess, "SecItemAdd sync+kSecUseSystemKeychain edumode:%d", eduMode);
    XCTAssertEqual(SecItemDelete((CFTypeRef)item), errSecSuccess, "SecItemDelete after SecItemAdd sync+kSecUseSystemKeychain edumode:%d", eduMode);

#if TARGET_OS_IOS
    if (eduMode)
        SecSecuritySetMusrMode(false, 501, -1);
#endif
}

#if TARGET_OS_IOS
- (void)testOldFlagInEduMode {
    [self helperOldSystemKeychainFlagWithEduMode:true];
}
#endif

- (void)testOldFlagNotInEduMode {
#if TARGET_OS_OSX
    // When both kSecUseSystemKeychain & kSecUseDataProtectionKeychain are true, we get a param error
    // because the old flag is not appropriate for macOS DP keychain
    NSDictionary *item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
        (id)kSecUseDataProtectionKeychain : @YES,
    };

    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecParam, "SecItemAdd kSecUseSystemKeychain & kSecUseDataProtectionKeychain");

#ifndef NDEBUG // Don't run the rest of this test in BATS yet. See rdar://101983503
    // Clean up in case previous test failed
    NSDictionary *query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
    };
    SecItemDelete((CFDictionaryRef)query);

    // Add with kSecUseSystemKeychain, which is ignored when kSecUseDataProtectionKeychain is not set
    item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychain : @YES,
    };

    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecSuccess, "SecItemAdd kSecUseSystemKeychain");

    // So it should be in the legacy keychain
    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecReturnAttributes: @YES,
    };

    CFTypeRef result = NULL;
    XCTAssertEqual(SecItemCopyMatching((CFTypeRef)query, &result), noErr, "SecItemCopyMatching after kSecUseSystemKeychain");
    if (result) { // so we don't crash calling CFGetTypeID
        XCTAssertEqual(CFGetTypeID(result), CFDictionaryGetTypeID(), "SecItemCopyMatching after kSecUseSystemKeychain unexpectedly returned something that's not a dict");
        XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrLabel], query[(id)kSecAttrLabel], "SecItemCopyMatching after kSecUseSystemKeychain label doesn't match");
        CFReleaseNull(result);
    }
    SecItemDelete((CFDictionaryRef)query);
#endif // NDEBUG
#else // TARGET_OS_OSX
    [self helperOldSystemKeychainFlagWithEduMode:false];
#endif // TARGET_OS_OSX
}

#if TARGET_OS_IOS || TARGET_OS_OSX

/*
 * Test add api in all its variants, using kSecUseSystemKeychainAlways.
 * Pay attention to kSecUseSystemKeychainAlways, whether in edu mode or not.
 */
- (void)helperNewSystemKeychainFlagWithEduMode:(bool)eduMode
{
    NSDictionary *item, *query, *queryUpdate, *attrsToUpdate0, *attrsToUpdate1, *attrsToUpdate2;
    CFTypeRef result = NULL;

    if (eduMode)
        SecSecuritySetMusrMode(true, 502, 502);

    /*
     * first clean some
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
    };
    SecItemDelete((CFDictionaryRef)query);

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychainAlways : @YES,
    };
    SecItemDelete((CFDictionaryRef)query);

    /*
     * Add entry
     */

    item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychainAlways : @YES,
    };

    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecSuccess, "SecItemAdd kSecUseSystemKeychainAlways edumode:%d", eduMode);

    /*
     * Check we can't find it in our keychain
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecReturnAttributes: @YES,
    };

    XCTAssertEqual(SecItemCopyMatching((CFTypeRef)query, &result), errSecItemNotFound, "SecItemCopyMatching kSecUseSystemKeychainAlways edumode:%d", eduMode);
    XCTAssertEqual(result, NULL, "SecItemCopyMatching kSecUseSystemKeychainAlways edumode:%d unexpectedly returned a result", eduMode);
    XCTAssertEqual(SecItemDelete((CFTypeRef)query), errSecItemNotFound, "SecItemDelete kSecUseSystemKeychainAlways edumode:%d", eduMode);

    /*
     * Check we can find it in system keychain
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychainAlways : @YES,
        (id)kSecReturnAttributes : @YES,
    };

    XCTAssertEqual(SecItemCopyMatching((CFTypeRef)query, &result), errSecSuccess, "SecItemCopyMatching(system) kSecUseSystemKeychainAlways edumode:%d", eduMode);
    XCTAssertEqual(CFGetTypeID(result), CFDictionaryGetTypeID(), "SecItemCopyMatching(system) kSecUseSystemKeychainAlways edumode:%d unexpectedly returned something that's not a dict", eduMode);
    XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrLabel], query[(id)kSecAttrLabel], "SecItemCopyMatching(system) kSecUseSystemKeychainAlways edumode:%d label doesn't match", eduMode);
    CFReleaseNull(result);

    queryUpdate = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychainAlways : @YES,
    };

    attrsToUpdate0 = @{
        (id)kSecAttrComment : @"This is a test. This is only a test.",
    };

    XCTAssertEqual(SecItemUpdate((CFTypeRef)queryUpdate, (CFTypeRef)attrsToUpdate0), errSecSuccess, "SecItemUpdate(system) kSecUseSystemKeychainAlways edumode:%d", eduMode);
    XCTAssertEqual(SecItemCopyMatching((CFTypeRef)query, &result), errSecSuccess, "SecItemCopyMatching(system) kSecUseSystemKeychainAlways edumode:%d", eduMode);
    XCTAssertEqual(CFGetTypeID(result), CFDictionaryGetTypeID(), "SecItemCopyMatching(system) kSecUseSystemKeychainAlways edumode:%d unexpectedly returned something that's not a dict", eduMode);
    XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrComment], attrsToUpdate0[(id)kSecAttrComment], "SecItemCopyMatching(system) kSecUseSystemKeychainAlways edumode:%d comment doesn't match", eduMode);
    CFReleaseNull(result);

    /* try to update the sync flag to true, which should fail */

    attrsToUpdate1 = @{
        (id)kSecAttrSynchronizable : @YES,
    };

    XCTAssertEqual(SecItemUpdate((CFTypeRef)queryUpdate, (CFTypeRef)attrsToUpdate1), errSecParam, "SecItemUpdate(system) kSecUseSystemKeychainAlways+kSecAttrSynchronizable edumode:%d", eduMode);

    /* try to update the sync flag to "1", which should fail */

    attrsToUpdate2 = @{
        (id)kSecAttrSynchronizable : @"1",
    };

    XCTAssertEqual(SecItemUpdate((CFTypeRef)queryUpdate, (CFTypeRef)attrsToUpdate2), errSecParam, "SecItemUpdate(system) kSecUseSystemKeychainAlways+kSecAttrSynchronizable edumode:%d", eduMode);

    XCTAssertEqual(SecItemCopyMatching((CFTypeRef)query, &result), errSecSuccess, "SecItemCopyMatching(system) kSecUseSystemKeychainAlways edumode:%d", eduMode);
    XCTAssertEqual(CFGetTypeID(result), CFDictionaryGetTypeID(), "SecItemCopyMatching(system) kSecUseSystemKeychainAlways edumode:%d unexpectedly returned something that's not a dict", eduMode);
    XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrLabel], query[(id)kSecAttrLabel], "SecItemCopyMatching(system) kSecUseSystemKeychainAlways edumode:%d label doesn't match", eduMode);
    XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrComment], attrsToUpdate0[(id)kSecAttrComment], "SecItemCopyMatching(system) kSecUseSystemKeychainAlways edumode:%d comment doesn't match", eduMode);
    XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrSynchronizable], @NO, "SecItemCopyMatching(system) kSecUseSystemKeychainAlways edumode:%d should not be synchronizable", eduMode);
    CFReleaseNull(result);

    XCTAssertEqual(SecItemDelete((CFTypeRef)query), errSecSuccess, "SecItemDelete(system) kSecUseSystemKeychainAlways edumode:%d", eduMode);

    /*
     * Add entry with both system and sync, which is not allowed
     */

    item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychainAlways : @YES,
        (id)kSecAttrSynchronizable : @YES,
    };

    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecParam, "SecItemAdd sync+kSecUseSystemKeychainAlways edumode:%d", eduMode);

    /*
     * Add entry with both system and sync="1", which is not allowed
     */

    item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychainAlways : @YES,
        (id)kSecAttrSynchronizable : @"1",
    };

    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecParam, "SecItemAdd sync+kSecUseSystemKeychainAlways edumode:%d", eduMode);

    // Add with both system keychain flags, which is not allowed
    item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychainAlways : @YES,
        (id)kSecUseSystemKeychain : @YES,
    };
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecParam, "SecItemAdd both flags edumode:%d", eduMode);

    // Update with both system keychain flags, which is not allowed
    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychainAlways : @YES,
        (id)kSecUseSystemKeychain : @YES,
    };
    XCTAssertEqual(SecItemUpdate((CFDictionaryRef)query, (CFDictionaryRef)attrsToUpdate0), errSecParam, "SecItemUpdate both flags edumode:%d", eduMode);

    // CopyMatching with both system keychain flags, which is not allowed
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)query, NULL), errSecParam, "SecItemCopyMatching both flags edumode:%d", eduMode);

    // Delete with both system keychain flags, which is not allowed
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)query), errSecParam, "SecItemDelete both flags edumode:%d", eduMode);

    if (eduMode)
        SecSecuritySetMusrMode(false, 501, -1);

}

#if TARGET_OS_IOS
- (void)testNewFlagInEduMode {
    _SecSystemKeychainAlwaysOverride(true);
    [self helperNewSystemKeychainFlagWithEduMode:true];
}
#endif // TARGET_OS_IOS

- (void)testNewFlagNotInEduMode {
    _SecSystemKeychainAlwaysOverride(true);
    [self helperNewSystemKeychainFlagWithEduMode:false];
}

/*
 * Test using kSecUseSystemKeychainAlways when it's disabled (via FF)
 */
- (void)testNewFlagDisabled {
    _SecSystemKeychainAlwaysOverride(false);

    /*
     * Add entry with new flag, which should not be allowed
     */

    NSDictionary *item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseSystemKeychainAlways : @YES,
    };

    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecParam, "test_new_system_keychain_flag_disabled SecItemAdd");
}
#endif // TARGET_OS_IOS || TARGET_OS_OSX

#if TARGET_OS_TV
/*
 * Test kSecUseUserIndependentKeychain
 */
- (void)testUserIndependentKeychain {
    NSDictionary *item, *query, *queryUpdate, *attrsToUpdate0, *attrsToUpdate1, *attrsToUpdate2;
    CFTypeRef result = NULL;

    /*
     * first clean some
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
    };
    SecItemDelete((CFDictionaryRef)query);

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseUserIndependentKeychain : @YES,
    };
    SecItemDelete((CFDictionaryRef)query);

    /*
     * Add entry
     */

    item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseUserIndependentKeychain : @YES,
    };

    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecSuccess, "SecItemAdd kSecUseUserIndependentKeychain");

    /*
     * Check we can't find it in our keychain
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecReturnAttributes: @YES,
    };

    XCTAssertEqual(SecItemCopyMatching((CFTypeRef)query, &result), errSecItemNotFound, "SecItemCopyMatching kSecUseUserIndependentKeychain");
    XCTAssertEqual(result, NULL, "SecItemCopyMatching kSecUseUserIndependentKeychain unexpectedly returned a result");
    XCTAssertEqual(SecItemDelete((CFTypeRef)query), errSecItemNotFound, "SecItemDelete kSecUseUserIndependentKeychain");

    /*
     * Check we can find it in user-independent keychain
     */

    query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseUserIndependentKeychain : @YES,
        (id)kSecReturnAttributes : @YES,
    };

    XCTAssertEqual(SecItemCopyMatching((CFTypeRef)query, &result), errSecSuccess, "SecItemCopyMatching kSecUseUserIndependentKeychain");
    XCTAssertEqual(CFGetTypeID(result), CFDictionaryGetTypeID(), "SecItemCopyMatching kSecUseUserIndependentKeychain unexpectedly returned something that's not a dict");
    XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrLabel], query[(id)kSecAttrLabel], "SecItemCopyMatching kSecUseUserIndependentKeychain label doesn't match");
    CFReleaseNull(result);

    queryUpdate = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseUserIndependentKeychain : @YES,
    };

    attrsToUpdate0 = @{
        (id)kSecAttrComment : @"This is a test. This is only a test.",
    };

    XCTAssertEqual(SecItemUpdate((CFTypeRef)queryUpdate, (CFTypeRef)attrsToUpdate0), errSecSuccess, "SecItemUpdate kSecUseUserIndependentKeychain");
    XCTAssertEqual(SecItemCopyMatching((CFTypeRef)query, &result), errSecSuccess, "SecItemCopyMatching kSecUseUserIndependentKeychain");
    XCTAssertEqual(CFGetTypeID(result), CFDictionaryGetTypeID(), "SecItemCopyMatching kSecUseUserIndependentKeychain unexpectedly returned something that's not a dict");
    XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrComment], attrsToUpdate0[(id)kSecAttrComment], "SecItemCopyMatching kSecUseUserIndependentKeychain comment doesn't match");
    CFReleaseNull(result);

    /* try to update the sync flag to true, which should fail */

    attrsToUpdate1 = @{
        (id)kSecAttrSynchronizable : @YES,
    };

    XCTAssertEqual(SecItemUpdate((CFTypeRef)queryUpdate, (CFTypeRef)attrsToUpdate1), errSecParam, "SecItemUpdate kSecUseUserIndependentKeychain+kSecAttrSynchronizable");

    /* try to update the sync flag to "1", which should fail */

    attrsToUpdate2 = @{
        (id)kSecAttrSynchronizable : @"1",
    };

    XCTAssertEqual(SecItemUpdate((CFTypeRef)queryUpdate, (CFTypeRef)attrsToUpdate2), errSecParam, "SecItemUpdate kSecUseUserIndependentKeychain+kSecAttrSynchronizable");

    XCTAssertEqual(SecItemCopyMatching((CFTypeRef)query, &result), errSecSuccess, "SecItemCopyMatching kSecUseUserIndependentKeychain");
    XCTAssertEqual(CFGetTypeID(result), CFDictionaryGetTypeID(), "SecItemCopyMatching kSecUseUserIndependentKeychain unexpectedly returned something that's not a dict");
    XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrLabel], query[(id)kSecAttrLabel], "SecItemCopyMatching kSecUseUserIndependentKeychain label doesn't match");
    XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrComment], attrsToUpdate0[(id)kSecAttrComment], "SecItemCopyMatching kSecUseUserIndependentKeychain comment doesn't match");
    XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrSynchronizable], @NO, "SecItemCopyMatching kSecUseUserIndependentKeychainshould not be synchronizable");
    CFReleaseNull(result);

    XCTAssertEqual(SecItemDelete((CFTypeRef)query), errSecSuccess, "SecItemDelete kSecUseUserIndependentKeychain");

    /*
     * Add entry with both uUICK and sync, which is not allowed
     */

    item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseUserIndependentKeychain : @YES,
        (id)kSecAttrSynchronizable : @YES,
    };

    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecParam, "SecItemAdd sync+kSecUseUserIndependentKeychain");

    /*
     * Add entry with both uUIKC and sync="1", which is not allowed
     */

    item = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrLabel : @"keychain label",
        (id)kSecUseUserIndependentKeychain : @YES,
        (id)kSecAttrSynchronizable : @"1",
    };

    XCTAssertEqual(SecItemAdd((CFDictionaryRef)item, NULL), errSecParam, "SecItemAdd sync+kSecUseUserIndependentKeychain");
}

#endif // TARGET_OS_TV

- (NSData *)signWithReimportedKey:(id)key asSystemKey:(BOOL)asSystemKey error:(NSError **)error {
    NSDictionary *attrs = CFBridgingRelease(SecKeyCopyAttributes((SecKeyRef)key));
    XCTAssertNotNil(attrs, @"Failed to copy attrs key %@", key);
    attrs = attrs ?: @{};

    NSMutableDictionary *parameters = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
        (id)kSecAttrTokenOID: attrs[(id)kSecAttrTokenOID],
    }.mutableCopy;
    if (asSystemKey) {
#if TARGET_OS_OSX
        parameters[(id)kSecUseSystemKeychainAlways] = @YES;
#else
        parameters[(id)kSecUseSystemKeychain] = @YES;
#endif
    }

    NSData *signature;
    id newKey = CFBridgingRelease(SecKeyCreateWithData((CFDataRef)[NSData data], (CFDictionaryRef)parameters, (void*)error));
    if (newKey != nil) {
        NSData *message = [@"message" dataUsingEncoding:NSUTF8StringEncoding];
        signature = CFBridgingRelease(SecKeyCreateSignature((SecKeyRef)newKey, kSecKeyAlgorithmECDSASignatureMessageX962SHA256, (CFDataRef)message, (void *)error));
    }
    return signature;
}

- (void)helperLegacySystemSecKeyInEduMode:(BOOL)eduMode {

    NSError *error;
#if TARGET_OS_IOS
    if (eduMode) {
        // TODO: Force SecIsEduMode() to return true here.
    }
#endif

    // Generate sysKey
    NSDictionary *sysKeyParams = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
        (id)kSecAttrIsPermanent: @NO,
#if TARGET_OS_OSX
        (id)kSecUseSystemKeychainAlways: @YES,
#else
        (id)kSecUseSystemKeychain: @YES,
#endif
    };
    id sysKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)sysKeyParams, (void *)&error));
    XCTAssertNotNil(sysKey, @"Failed to generate system SEP key: %@", error);

    // Generate userKey
    NSDictionary *userKeyParams = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrTokenID: (id)kSecAttrTokenIDAppleKeyStore,
        (id)kSecAttrIsPermanent: @NO,
    };
    id userKey = CFBridgingRelease(SecKeyCreateRandomKey((CFDictionaryRef)userKeyParams, (void *)&error));
    XCTAssertNotNil(sysKey, @"Failed to generate user SEP key: %@", error);

    // Re-import syskey as syskey - should work.
    XCTAssertNotNil([self signWithReimportedKey:sysKey asSystemKey:YES error:&error], @"Failed to sign with reimported syskey as syskey: %@", error);

    // Reimport userkey as userkey - should work.
    XCTAssertNotNil([self signWithReimportedKey:userKey asSystemKey:NO error:&error], @"Failed to sign with reimported userkey as userkey: %@", error);

// Following tests are disabled for now until we teach SecIsEduMode() to be forced to return requested value for testing purposes.
#if 0
    // Re-import syskey as userkey - should not work in edumode
    if (TARGET_OS_OSX || eduMode) {
        XCTAssertNil([self signWithReimportedKey:sysKey asSystemKey:NO error:&error], @"Failed to fail to sign with reimported syskey as userkey");
    } else {
        XCTAssertNotNil([self signWithReimportedKey:sysKey asSystemKey:NO error:&error], @"Failed to sign with reimported syskey as userkey: %@", error);
    }

    // Reimport userkey as syskey - should not work in edumode
    if (TARGET_OS_OSX || eduMode) {
        XCTAssertNil([self signWithReimportedKey:userKey asSystemKey:YES error:&error], @"Failed to fail to sign with reimported userkey as syskey");
    } else {
        XCTAssertNotNil([self signWithReimportedKey:userKey asSystemKey:YES error:&error], @"Failed to sign with reimported userkey as syskey: %@", error);
    }
#endif // 0

#if TARGET_OS_IOS
    if (eduMode) {
        // TODO: Reset forcing SecIsEduMode() to return real value.
    }
#endif
}

- (void)testLegacySystemSecKeyWithoutEduMode {
    [self helperLegacySystemSecKeyInEduMode:NO];
}

#if TARGET_OS_IOS
- (void)testLegacySystemSecKeyInEduMode {
    [self helperLegacySystemSecKeyInEduMode:YES];
}
#endif

#endif // TARGET_OS_IPHONE || TARGET_OS_OSX

@end
