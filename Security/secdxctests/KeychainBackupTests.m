#import "KeychainXCTest.h"
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#include <Security/SecEntitlements.h>
#include <ipc/server_security_helpers.h>
#import "OTConstants.h"
#include "keychain/securityd/SecItemSchema.h"
#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemDb.h"

@interface KeychainBackupTests : KeychainXCTest
@end


@implementation KeychainBackupTests {
    NSString* _applicationIdentifier;
}

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
    [super setUp];
    _applicationIdentifier = @"com.apple.security.backuptests";
    SecSecurityClientSetApplicationIdentifier((__bridge CFStringRef)_applicationIdentifier);
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

# pragma mark - Test OTA Backups

// Code lovingly adapted from si-33-keychain-backup
#if USE_KEYSTORE
- (NSData*)createKeybagWithType:(keybag_handle_t)bag_type password:(NSData*)password
{
    keybag_handle_t handle = bad_keybag_handle;
    kern_return_t bag_created = aks_create_bag(password ? password.bytes : NULL, password ? (int)password.length : 0, bag_type, &handle);
    XCTAssertEqual(bag_created, kAKSReturnSuccess, @"Unable to create keybag");

    void *bag = NULL;
    int bagLen = 0;
    kern_return_t bag_saved = aks_save_bag(handle, &bag, &bagLen);
    XCTAssertEqual(bag_saved, kAKSReturnSuccess, @"Unable to save keybag");

    NSData* bagData = [NSData dataWithBytes:bag length:bagLen];
    XCTAssertNotNil(bagData, @"Unable to create NSData from bag bytes");

    return bagData;
}
#endif

// All backup paths ultimately lead to SecServerCopyKeychainPlist which does the actual exporting,
// so this test ought to suffice for all backup configurations
- (void)testAppClipDoesNotBackup {

    // First add a "regular" item for each class, which we expect to be in the backup later
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    query[(id)kSecClass] = (id)kSecClassInternetPassword;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    query[(id)kSecClass] = (id)kSecClassCertificate;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    query[(id)kSecClass] = (id)kSecClassKey;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    // Switch to being an app clip, add another item for each class, which we expect not to find in the backup
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];

    query[(id)kSecClass] = (id)kSecClassGenericPassword;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    query[(id)kSecClass] = (id)kSecClassInternetPassword;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    query[(id)kSecClass] = (id)kSecClassCertificate;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    query[(id)kSecClass] = (id)kSecClassKey;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    SecSecurityClientAppClipToRegular();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementRestoreKeychain, kCFBooleanTrue);

    // Code lovingly adapted from si-33-keychain-backup
    NSData* keybag;
#if USE_KEYSTORE
    keybag = [self createKeybagWithType:kAppleKeyStoreBackupBag password:nil];
#else
    keybag = [NSData new];
#endif

    NSData* data = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, nil));

    XCTAssert(data);
    XCTAssertGreaterThan([data length], 42, @"Got empty dictionary");
    NSDictionary* keychain = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nil error:nil];

    // Only one item should be here for each class, which is the regular one.
    XCTAssertEqual([keychain[@"genp"] count], 1);
    XCTAssertEqual([keychain[@"inet"] count], 1);
    XCTAssertEqual([keychain[@"cert"] count], 1);
    XCTAssertEqual([keychain[@"keys"] count], 1);
}


- (void)setupKeychainItemsWithQuery:(NSMutableDictionary*)query andReturnPrefGENP:(NSData**)prefGENP prefINET:(NSData**)prefINET prefCERT:(NSData**)prefCERT prefKEYS:(NSData**)prefKEYS oldStyle:(BOOL)oldStyle
{
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    query[(id)kSecReturnPersistentRef] = @YES;
    CFTypeRef prefGENPBeforeRestore = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &prefGENPBeforeRestore), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefGENPBeforeRestore, "persistent ref should not be nil");
    if (!oldStyle) {
        XCTAssertTrue(CFDataGetLength(prefGENPBeforeRestore) == 20, "persistent ref length should be 20");
    }

    query[(id)kSecClass] = (id)kSecClassInternetPassword;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    CFTypeRef prefINETBeforeRestore = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &prefINETBeforeRestore), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefINETBeforeRestore, "persistent ref should not be nil");
    if (!oldStyle) {
        XCTAssertTrue(CFDataGetLength(prefINETBeforeRestore) == 20, "persistent ref length should be 20");
    }
    query[(id)kSecClass] = (id)kSecClassCertificate;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    CFTypeRef prefCERTBeforeRestore = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &prefCERTBeforeRestore), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefCERTBeforeRestore, "persistent ref should not be nil");
    if (!oldStyle) {
        XCTAssertTrue(CFDataGetLength(prefCERTBeforeRestore) == 20, "persistent ref length should be 20");
    }
    query[(id)kSecClass] = (id)kSecClassKey;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    CFTypeRef prefKEYBeforeRestore = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &prefKEYBeforeRestore), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefKEYBeforeRestore, "persistent ref should not be nil");
    if (!oldStyle) {
        XCTAssertTrue(CFDataGetLength(prefKEYBeforeRestore) == 20, "persistent ref length should be 20");
    }
    *prefGENP = (__bridge NSData *)prefGENPBeforeRestore;
    *prefINET = (__bridge NSData *)prefINETBeforeRestore;
    *prefCERT = (__bridge NSData *)prefCERTBeforeRestore;
    *prefKEYS = (__bridge NSData *)prefKEYBeforeRestore;
}

- (void)findKeychainItemsWithQuery:(NSMutableDictionary*)query andReturnPrefGENP:(NSData**)prefGENP prefINET:(NSData**)prefINET prefCERT:(NSData**)prefCERT prefKEYS:(NSData**)prefKEYS
{
    CFTypeRef prefGENPAfterRestore = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &prefGENPAfterRestore), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefGENPAfterRestore, "persistent ref should not be nil");
    XCTAssertTrue(CFDataGetLength(prefGENPAfterRestore) == 20, "persistent ref length should be 20");

    query[(id)kSecClass] = (id)kSecClassInternetPassword;
    CFTypeRef prefINETAfterRestore = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &prefINETAfterRestore), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefINETAfterRestore, "persistent ref should not be nil");
    XCTAssertTrue(CFDataGetLength(prefINETAfterRestore) == 20, "persistent ref length should be 20");

    query[(id)kSecClass] = (id)kSecClassCertificate;
    CFTypeRef prefCERTAfterRestore = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &prefCERTAfterRestore), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefCERTAfterRestore, "persistent ref should not be nil");
    XCTAssertTrue(CFDataGetLength(prefCERTAfterRestore) == 20, "persistent ref length should be 20");

    query[(id)kSecClass] = (id)kSecClassKey;
    CFTypeRef prefKEYAfterRestore = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &prefKEYAfterRestore), errSecSuccess);
    XCTAssertNotNil((__bridge id)prefKEYAfterRestore, "persistent ref should not be nil");
    XCTAssertTrue(CFDataGetLength(prefKEYAfterRestore) == 20, "persistent ref length should be 20");

    *prefGENP = (__bridge NSData *)prefGENPAfterRestore;
    *prefINET = (__bridge NSData *)prefINETAfterRestore;
    *prefCERT = (__bridge NSData *)prefCERTAfterRestore;
    *prefKEYS = (__bridge NSData *)prefKEYAfterRestore;
}

- (void)findKeychainItemsWithQuery:(NSMutableDictionary*)query oldData:(NSData*)oldData updatedData:(NSData*)updatedData
{
    CFTypeRef restoredItem = NULL;

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, &restoredItem), errSecSuccess);
    XCTAssertNotNil((__bridge id)restoredItem, "restoredItem should not be nil");
    NSData* vData = ((__bridge NSDictionary*)restoredItem)[(id)kSecValueData];
    XCTAssertNotNil(vData, "vData should not be nil");
    XCTAssertEqualObjects(vData, updatedData, "item should contain the updated item's data");
}

- (void)updateKeychainItemsWithQuery:(NSMutableDictionary*)findQuery
{
    //incoming query will be for GENP

    // Add every primary key attribute to this find dictionary
    NSMutableDictionary* f = [[NSMutableDictionary alloc] init];
    f[(id)kSecClass] = findQuery[(id)kSecClass];
    f[(id)kSecUseDataProtectionKeychain] = findQuery[(id)kSecUseDataProtectionKeychain];
    f[(id)kSecAttrSynchronizable] = findQuery[(id)kSecAttrSynchronizable];

    NSMutableDictionary* updateQuery = [findQuery mutableCopy];
    updateQuery[(id)kSecClass] = nil;

    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)f, (__bridge CFDictionaryRef)updateQuery), errSecSuccess);

    f[(id)kSecClass] = (id)kSecClassInternetPassword;
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)f, (__bridge CFDictionaryRef)updateQuery), errSecSuccess);

    f[(id)kSecClass] = (id)kSecClassCertificate;
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)f, (__bridge CFDictionaryRef)updateQuery), errSecSuccess);

    f[(id)kSecClass] = (id)kSecClassKey;
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)f, (__bridge CFDictionaryRef)updateQuery), errSecSuccess);
}

- (void)testSecItemBackupAndRestoreWithPersistentRefs
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    NSData* prefGENPBeforeRestore = nil;
    NSData* prefINETBeforeRestore = nil;
    NSData* prefCERTBeforeRestore = nil;
    NSData* prefKEYSBeforeRestore = nil;

    [self setupKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPBeforeRestore prefINET:&prefINETBeforeRestore prefCERT:&prefCERTBeforeRestore prefKEYS:&prefKEYSBeforeRestore oldStyle:NO];

    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementRestoreKeychain, @YES);

    NSData* keybag;
#if USE_KEYSTORE
    keybag = [self createKeybagWithType:kAppleKeyStoreBackupBag password:nil];
#else
    keybag = [NSData new];
#endif

    NSData* data = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, nil));

    XCTAssert(data);
    NSDictionary* keychain = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nil error:nil];

    XCTAssertEqual([keychain[@"genp"] count], 1);
    XCTAssertEqual([keychain[@"inet"] count], 1);
    XCTAssertEqual([keychain[@"cert"] count], 1);
    XCTAssertEqual([keychain[@"keys"] count], 1);

    query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess);
    query[(id)kSecClass] = (id)kSecClassKey;
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess);
    query[(id)kSecClass] = (id)kSecClassCertificate;
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess);
    query[(id)kSecClass] = (id)kSecClassInternetPassword;
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess);


    XCTAssertEqual(_SecKeychainRestoreBackup((__bridge CFDataRef)data, (__bridge CFDataRef)keybag, NULL), errSecSuccess, "keychain restore should succeed");

    query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecReturnPersistentRef : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    NSData* prefGENPAfterRestore = nil;
    NSData* prefINETAfterRestore = nil;
    NSData* prefCERTAfterRestore = nil;
    NSData* prefKEYSAfterRestore = nil;

    [self findKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPAfterRestore prefINET:&prefINETAfterRestore prefCERT:&prefCERTAfterRestore prefKEYS:&prefKEYSAfterRestore];

    XCTAssertEqualObjects(prefKEYSBeforeRestore, prefKEYSAfterRestore, "persistent refs from the KEYS table should be equal");
    XCTAssertEqualObjects(prefGENPBeforeRestore, prefGENPAfterRestore, "persistent refs from the GENP should be equal");
    XCTAssertEqualObjects(prefINETBeforeRestore, prefINETAfterRestore, "persistent refs from the INET should be equal");
    XCTAssertEqualObjects(prefCERTBeforeRestore, prefCERTAfterRestore, "persistent refs from the CERT should be equal");

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

// tests restoring a backup of items that don't already exist in the keychain and sets a new UUID for each
- (void)testSecItemRestoreBackupOldStylePrefsAndAddUUIDPersistentRefs
{
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    NSData* prefGENPBeforeRestore = nil;
    NSData* prefINETBeforeRestore = nil;
    NSData* prefCERTBeforeRestore = nil;
    NSData* prefKEYSBeforeRestore = nil;

    [self setupKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPBeforeRestore prefINET:&prefINETBeforeRestore prefCERT:&prefCERTBeforeRestore prefKEYS:&prefKEYSBeforeRestore oldStyle:YES];

    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementRestoreKeychain, @YES);

    NSData* keybag;
#if USE_KEYSTORE
    keybag = [self createKeybagWithType:kAppleKeyStoreBackupBag password:nil];
#else
    keybag = [NSData new];
#endif

    NSData* data = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, nil));

    XCTAssert(data);
    NSDictionary* keychain = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nil error:nil];

    XCTAssertEqual([keychain[@"genp"] count], 1);
    XCTAssertEqual([keychain[@"inet"] count], 1);
    XCTAssertEqual([keychain[@"cert"] count], 1);
    XCTAssertEqual([keychain[@"keys"] count], 1);

    query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess);

    query[(id)kSecClass] = (id)kSecClassKey;
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess);

    query[(id)kSecClass] = (id)kSecClassCertificate;
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess);

    query[(id)kSecClass] = (id)kSecClassInternetPassword;
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess);

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    XCTAssertEqual(_SecKeychainRestoreBackup((__bridge CFDataRef)data, (__bridge CFDataRef)keybag, NULL), errSecSuccess, "keychain restore should succeed");

    query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecReturnPersistentRef : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    NSData* prefGENPAfterRestore = nil;
    NSData* prefINETAfterRestore = nil;
    NSData* prefCERTAfterRestore = nil;
    NSData* prefKEYSAfterRestore = nil;

    [self findKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPAfterRestore prefINET:&prefINETAfterRestore prefCERT:&prefCERTAfterRestore prefKEYS:&prefKEYSAfterRestore];

    XCTAssertNotEqualObjects(prefKEYSBeforeRestore, prefKEYSAfterRestore, "persistent refs from the KEYS table should NOT be equal");
    XCTAssertNotEqualObjects(prefGENPBeforeRestore, prefGENPAfterRestore, "persistent refs from the GENP should NOT be equal");
    XCTAssertNotEqualObjects(prefINETBeforeRestore, prefINETAfterRestore, "persistent refs from the INET should NOT be equal");
    XCTAssertNotEqualObjects(prefCERTBeforeRestore, prefCERTAfterRestore, "persistent refs from the CERT should NOT be equal");

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

// tests restoring a backup of items that already exist in the keychain but without UUID persistent references set
- (void)testSecItemRestoreItemBackupAndAddUUIDPersistentRefs
{
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    NSData* prefGENPBeforeRestore = nil;
    NSData* prefINETBeforeRestore = nil;
    NSData* prefCERTBeforeRestore = nil;
    NSData* prefKEYSBeforeRestore = nil;

    [self setupKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPBeforeRestore prefINET:&prefINETBeforeRestore prefCERT:&prefCERTBeforeRestore prefKEYS:&prefKEYSBeforeRestore oldStyle:YES];

    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementRestoreKeychain, @YES);

    NSData* keybag;
#if USE_KEYSTORE
    keybag = [self createKeybagWithType:kAppleKeyStoreBackupBag password:nil];
#else
    keybag = [NSData new];
#endif

    NSData* data = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, nil));

    XCTAssert(data);
    NSDictionary* keychain = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nil error:nil];

    XCTAssertEqual([keychain[@"genp"] count], 1);
    XCTAssertEqual([keychain[@"inet"] count], 1);
    XCTAssertEqual([keychain[@"cert"] count], 1);
    XCTAssertEqual([keychain[@"keys"] count], 1);

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    XCTAssertEqual(_SecKeychainRestoreBackup((__bridge CFDataRef)data, (__bridge CFDataRef)keybag, NULL), errSecSuccess, "keychain restore should succeed");

    query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecReturnPersistentRef : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    NSData* prefGENPAfterRestore = nil;
    NSData* prefINETAfterRestore = nil;
    NSData* prefCERTAfterRestore = nil;
    NSData* prefKEYSAfterRestore = nil;

    [self findKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPAfterRestore prefINET:&prefINETAfterRestore prefCERT:&prefCERTAfterRestore prefKEYS:&prefKEYSAfterRestore];

    XCTAssertNotEqualObjects(prefKEYSBeforeRestore, prefKEYSAfterRestore, "persistent refs from the KEYS table should NOT be equal");
    XCTAssertNotEqualObjects(prefGENPBeforeRestore, prefGENPAfterRestore, "persistent refs from the GENP should NOT be equal");
    XCTAssertNotEqualObjects(prefINETBeforeRestore, prefINETAfterRestore, "persistent refs from the INET should NOT be equal");
    XCTAssertNotEqualObjects(prefCERTBeforeRestore, prefCERTAfterRestore, "persistent refs from the CERT should NOT be equal");

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

// tests restoring a backup of items that already exist in the keychain with UUID persistent references set
- (void)testSecItemRestoreConflictedUUIDPersistentReferenceItems
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    NSData* oldPassword = [@"password" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* updatedPassword = [@"updatedPassword" dataUsingEncoding:NSUTF8StringEncoding];

    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrSynchronizable : @YES,
        (id)kSecValueData : oldPassword,
    } mutableCopy];

    NSData* prefGENPBeforeRestore = nil;
    NSData* prefINETBeforeRestore = nil;
    NSData* prefCERTBeforeRestore = nil;
    NSData* prefKEYSBeforeRestore = nil;

    [self setupKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPBeforeRestore prefINET:&prefINETBeforeRestore prefCERT:&prefCERTBeforeRestore prefKEYS:&prefKEYSBeforeRestore oldStyle:NO];

    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementRestoreKeychain, @YES);

    NSData* keybag;
#if USE_KEYSTORE
    keybag = [self createKeybagWithType:kAppleKeyStoreBackupBag password:nil];
#else
    keybag = [NSData new];
#endif

    NSData* data = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, nil));

    XCTAssert(data);
    NSDictionary* keychain = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nil error:nil];

    XCTAssertEqual([keychain[@"genp"] count], 1);
    XCTAssertEqual([keychain[@"inet"] count], 1);
    XCTAssertEqual([keychain[@"cert"] count], 1);
    XCTAssertEqual([keychain[@"keys"] count], 1);

    query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrSynchronizable : @YES,
        (id)kSecValueData : updatedPassword,
    } mutableCopy];

    //now let's change the local items to force the item conflict
    [self updateKeychainItemsWithQuery:query];

    XCTAssertEqual(_SecKeychainRestoreBackup((__bridge CFDataRef)data, (__bridge CFDataRef)keybag, NULL), errSecSuccess, "keychain restore should succeed");

    query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrSynchronizable : @YES,
        (id)kSecReturnAttributes : @YES,
        (id)kSecReturnData : @YES,
    } mutableCopy];

    [self findKeychainItemsWithQuery:query oldData:oldPassword updatedData:updatedPassword];

    NSData* prefGENPAfterRestore = nil;
    NSData* prefINETAfterRestore = nil;
    NSData* prefCERTAfterRestore = nil;
    NSData* prefKEYSAfterRestore = nil;

    query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecReturnPersistentRef : @YES,
        (id)kSecAttrSynchronizable : @YES,
        (id)kSecValueData : oldPassword,
    } mutableCopy];

    [self findKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPAfterRestore prefINET:&prefINETAfterRestore prefCERT:&prefCERTAfterRestore prefKEYS:&prefKEYSAfterRestore];

    XCTAssertEqualObjects(prefKEYSBeforeRestore, prefKEYSAfterRestore, "persistent refs from the KEYS table should be equal");
    XCTAssertEqualObjects(prefGENPBeforeRestore, prefGENPAfterRestore, "persistent refs from the GENP should be equal");
    XCTAssertEqualObjects(prefINETBeforeRestore, prefINETAfterRestore, "persistent refs from the INET should be equal");
    XCTAssertEqualObjects(prefCERTBeforeRestore, prefCERTAfterRestore, "persistent refs from the CERT should be equal");


    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

// tests restoring a backup of sysbound items that don't already exist in the keychain and sets a new UUID for each
- (void)testSecItemRestoreSysboundBackupAndAddUUIDPersistentRefs
{
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrSysBound : @(kSecSecAttrSysBoundPreserveDuringRestore),
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    NSData* prefGENPBeforeRestore = nil;
    NSData* prefINETBeforeRestore = nil;
    NSData* prefCERTBeforeRestore = nil;
    NSData* prefKEYSBeforeRestore = nil;

    [self setupKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPBeforeRestore prefINET:&prefINETBeforeRestore prefCERT:&prefCERTBeforeRestore prefKEYS:&prefKEYSBeforeRestore oldStyle:YES];

    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementRestoreKeychain, @YES);

    NSData* keybag;
#if USE_KEYSTORE
    keybag = [self createKeybagWithType:kAppleKeyStoreBackupBag password:nil];
#else
    keybag = [NSData new];
#endif

    NSData* data = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, nil));

    NSDictionary* keychain = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nil error:nil];

    XCTAssertEqual([keychain[@"genp"] count], 0);
    XCTAssertEqual([keychain[@"inet"] count], 0);
    XCTAssertEqual([keychain[@"cert"] count], 0);
    XCTAssertEqual([keychain[@"keys"] count], 0);

    XCTAssert(data);

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    XCTAssertEqual(_SecKeychainRestoreBackup((__bridge CFDataRef)data, (__bridge CFDataRef)keybag, NULL), errSecSuccess, "keychain restore should succeed");

    query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecReturnPersistentRef : @YES,
        (id)kSecAttrSysBound : @(kSecSecAttrSysBoundPreserveDuringRestore),
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    NSData* prefGENPAfterRestore = nil;
    NSData* prefINETAfterRestore = nil;
    NSData* prefCERTAfterRestore = nil;
    NSData* prefKEYSAfterRestore = nil;

    [self findKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPAfterRestore prefINET:&prefINETAfterRestore prefCERT:&prefCERTAfterRestore prefKEYS:&prefKEYSAfterRestore];

    XCTAssertNotEqualObjects(prefKEYSBeforeRestore, prefKEYSAfterRestore, "persistent refs from the KEYS table should NOT be equal");
    XCTAssertNotEqualObjects(prefGENPBeforeRestore, prefGENPAfterRestore, "persistent refs from the GENP should NOT be equal");
    XCTAssertNotEqualObjects(prefINETBeforeRestore, prefINETAfterRestore, "persistent refs from the INET should NOT be equal");
    XCTAssertNotEqualObjects(prefCERTBeforeRestore, prefCERTAfterRestore, "persistent refs from the CERT should NOT be equal");

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

// tests restoring a backup of sysbound UUID persistent ref clad items that already exist in the keychain
- (void)testSecItemRestoreSysboundBackupAndUpdateUUIDPersistentRefs
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrSysBound : @(kSecSecAttrSysBoundPreserveDuringRestore),
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    NSData* prefGENPBeforeRestore = nil;
    NSData* prefINETBeforeRestore = nil;
    NSData* prefCERTBeforeRestore = nil;
    NSData* prefKEYSBeforeRestore = nil;

    [self setupKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPBeforeRestore prefINET:&prefINETBeforeRestore prefCERT:&prefCERTBeforeRestore prefKEYS:&prefKEYSBeforeRestore oldStyle:NO];

    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementRestoreKeychain, @YES);

    NSData* keybag;
#if USE_KEYSTORE
    keybag = [self createKeybagWithType:kAppleKeyStoreBackupBag password:nil];
#else
    keybag = [NSData new];
#endif

    NSData* data = CFBridgingRelease(_SecKeychainCopyBackup((__bridge CFDataRef)keybag, nil));

    NSDictionary* keychain = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nil error:nil];

    XCTAssertEqual([keychain[@"genp"] count], 0);
    XCTAssertEqual([keychain[@"inet"] count], 0);
    XCTAssertEqual([keychain[@"cert"] count], 0);
    XCTAssertEqual([keychain[@"keys"] count], 0);

    XCTAssert(data);

    XCTAssertEqual(_SecKeychainRestoreBackup((__bridge CFDataRef)data, (__bridge CFDataRef)keybag, NULL), errSecSuccess, "keychain restore should succeed");

    query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecReturnPersistentRef : @YES,
        (id)kSecAttrSysBound : @(kSecSecAttrSysBoundPreserveDuringRestore),
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    NSData* prefGENPAfterRestore = nil;
    NSData* prefINETAfterRestore = nil;
    NSData* prefCERTAfterRestore = nil;
    NSData* prefKEYSAfterRestore = nil;

    [self findKeychainItemsWithQuery:query andReturnPrefGENP:&prefGENPAfterRestore prefINET:&prefINETAfterRestore prefCERT:&prefCERTAfterRestore prefKEYS:&prefKEYSAfterRestore];

    XCTAssertEqualObjects(prefKEYSBeforeRestore, prefKEYSAfterRestore, "persistent refs from the KEYS table should be equal");
    XCTAssertEqualObjects(prefGENPBeforeRestore, prefGENPAfterRestore, "persistent refs from the GENP should be equal");
    XCTAssertEqualObjects(prefINETBeforeRestore, prefINETAfterRestore, "persistent refs from the INET should be equal");
    XCTAssertEqualObjects(prefCERTBeforeRestore, prefCERTAfterRestore, "persistent refs from the CERT should be equal");


    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}
@end
