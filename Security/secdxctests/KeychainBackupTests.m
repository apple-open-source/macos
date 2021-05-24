#import "KeychainXCTest.h"
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#include <Security/SecEntitlements.h>
#include <ipc/server_security_helpers.h>

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

@end
