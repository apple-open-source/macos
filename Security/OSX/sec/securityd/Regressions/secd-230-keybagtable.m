/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include "secd_regressions.h"

#include <securityd/SecDbItem.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecFileLocations.h>
#include <utilities/fileIo.h>

#include <securityd/SecItemServer.h>

#include <Security/SecBasePriv.h>
#include <Security/SecItemBackup.h>

#include <TargetConditionals.h>
#include <AssertMacros.h>

#import "SecBackupKeybagEntry.h"

#if 0

<rdar://problem/30685971> Add test for keybag table add SPI
<rdar://problem/30412884> Create an SPI to add a keybag to the keychain database (Keybag Table)

// original secd_35_keychain_migrate_inet

sudo defaults write /Library/Preferences/com.apple.security V10SchemaUpgradeTest -bool true
sudo defaults read /Library/Preferences/com.apple.security V10SchemaUpgradeTest

#endif

#if USE_KEYSTORE
#include <libaks.h>

#include "SecdTestKeychainUtilities.h"

static const bool kTestCustomKeybag = false;
static const bool kTestLocalKeybag = false;

void SecAccessGroupsSetCurrent(CFArrayRef accessGroups);
CFArrayRef SecAccessGroupsGetCurrent();

#define kSecdTestCreateCustomKeybagTestCount 6
#define kSecdTestLocalKeybagTestCount 1
#define kSecdTestKeybagtableTestCount 5
#define kSecdTestAddItemTestCount 2

#define DATA_ARG(x) (x) ? CFDataGetBytePtr((x)) : NULL, (x) ? (int)CFDataGetLength((x)) : 0

// copied from si-33-keychain-backup.c
static CFDataRef create_keybag(keybag_handle_t bag_type, CFDataRef password)
{
    keybag_handle_t handle = bad_keybag_handle;

    if (aks_create_bag(DATA_ARG(password), bag_type, &handle) == 0) {
        void * keybag = NULL;
        int keybag_size = 0;
        if (aks_save_bag(handle, &keybag, &keybag_size) == 0) {
            return CFDataCreate(kCFAllocatorDefault, keybag, keybag_size);
        }
    }

    return CFDataCreate(kCFAllocatorDefault, NULL, 0);
}

static bool createCustomKeybag() {
    /* custom keybag */
    keybag_handle_t keybag;
    keybag_state_t state;
    char *passcode="password";
    int passcode_len=(int)strlen(passcode);
    const bool kTestLockedKeybag = false;

    ok(kIOReturnSuccess==aks_create_bag(passcode, passcode_len, kAppleKeyStoreDeviceBag, &keybag), "create keybag");
    ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
    ok(!(state&keybag_state_locked), "keybag unlocked");
    SecItemServerSetKeychainKeybag(keybag);

    if (kTestLockedKeybag) {
        /* lock */
        ok(kIOReturnSuccess==aks_lock_bag(keybag), "lock keybag");
        ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
        ok(state&keybag_state_locked, "keybag locked");
    }

    return true;
}

static int keychainTestEnvironment(const char *environmentName, dispatch_block_t do_in_reset, dispatch_block_t do_in_environment) {
    //
    // Setup phase
    //
    CFArrayRef old_ag = SecAccessGroupsGetCurrent();
    CFMutableArrayRef test_ag = CFArrayCreateMutableCopy(NULL, 0, old_ag);
    CFArrayAppendValue(test_ag, CFSTR("test"));
    SecAccessGroupsSetCurrent(test_ag);

    secd_test_setup_temp_keychain(environmentName, do_in_reset);
    bool haveCustomKeybag = kTestCustomKeybag && createCustomKeybag();

    // Perform tasks in the test keychain environment
    if (do_in_environment)
        do_in_environment();

    //
    // Cleanup phase
    //

    // Reset keybag
    if (haveCustomKeybag)
        SecItemServerResetKeychainKeybag();

    // Reset server accessgroups
    SecAccessGroupsSetCurrent(old_ag);
    CFReleaseSafe(test_ag);
    // Reset custom $HOME
    SetCustomHomePath(NULL);
    SecKeychainDbReset(NULL);
    return 0;
}

static int addOneItemTest(NSString *account) {
    /* Creating a password */
    const char *v_data = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));

    NSDictionary *item = @{
        (__bridge NSString *)kSecClass : (__bridge NSString *)kSecClassInternetPassword,
        (__bridge NSString *)kSecAttrServer : @"members.spamcop.net",
        (__bridge NSString *)kSecAttrAccount : account, // e.g. @"smith",
        (__bridge NSString *)kSecAttrPort : @80,
        (__bridge NSString *)kSecAttrProtocol : @"http",
        (__bridge NSString *)kSecAttrAuthenticationType : @"dflt",
        (__bridge NSString *)kSecValueData : (__bridge NSData *)pwdata
    };

    ok_status(SecItemAdd((CFDictionaryRef)item, NULL), "add internet password, while unlocked");
    CFReleaseSafe(pwdata);
    return 0;
}

static int localKeybagTest() {
    const char *pass = "sup3rsekretpassc0de";
    CFDataRef password = CFDataCreate(NULL, (UInt8 *)pass, strlen(pass));
    CFDataRef keybag = create_keybag(kAppleKeyStoreAsymmetricBackupBag, password);
    ok(keybag != NULL);
    CFReleaseNull(keybag);
    CFReleaseNull(password);
    return 0;
}

static int test_keybagtable() {
    CFErrorRef error = NULL;
    const char *pass = "sup3rsekretpassc0de";
    CFDataRef password = CFDataCreate(NULL, (UInt8 *)pass, strlen(pass));
    CFDataRef identifier = NULL;
    CFURLRef pathinfo = NULL;

    ok(SecBackupKeybagAdd(password, &identifier, &pathinfo, &error));
    CFReleaseNull(error);

    NSDictionary *deleteQuery = @{(__bridge NSString *)kSecAttrPublicKeyHash:(__bridge NSData *)identifier};
    ok(SecBackupKeybagDelete((__bridge CFDictionaryRef)deleteQuery, &error));

    ok(SecBackupKeybagAdd(password, &identifier, &pathinfo, &error));
    CFReleaseNull(error);

    ok(SecBackupKeybagAdd(password, &identifier, &pathinfo, &error));
    CFReleaseNull(error);

    NSDictionary *deleteAllQuery = @{(id)kSecMatchLimit: (id)kSecMatchLimitAll};
    ok(SecBackupKeybagDelete((__bridge CFDictionaryRef)deleteAllQuery, &error));

    CFReleaseNull(identifier);
    CFReleaseNull(pathinfo);
    CFReleaseNull(password);
    CFReleaseNull(error);
    return 0;
}

static void showHomeURL() {
#if DEBUG
    CFURLRef homeURL = SecCopyHomeURL();
    NSLog(@"Home URL for test : %@", homeURL);
    CFReleaseSafe(homeURL);
#endif
}

int secd_230_keybagtable(int argc, char *const *argv)
{
    int testcount = kSecdTestSetupTestCount + kSecdTestKeybagtableTestCount + kSecdTestAddItemTestCount;
    if (kTestLocalKeybag)
        testcount += kSecdTestLocalKeybagTestCount;
    if (kTestCustomKeybag)
        testcount += kSecdTestCreateCustomKeybagTestCount;
    plan_tests(testcount);

    dispatch_block_t run_tests = ^{
        showHomeURL();
        if (kTestLocalKeybag)
            localKeybagTest();
        addOneItemTest(@"smith");
        test_keybagtable();
        addOneItemTest(@"jones");
    };

    dispatch_block_t do_in_reset = NULL;
    dispatch_block_t do_in_environment = run_tests;

    keychainTestEnvironment("secd_230_keybagtable", do_in_reset, do_in_environment);

    return 0;
}

#else

int secd_230_keybagtable(int argc, char *const *argv)
{
    plan_tests(1);
    secLogDisable();

    todo("Not yet working in simulator");

    TODO: {
        ok(false);
    }
    /* not implemented in simulator (no keybag) */
    return 0;
}
#endif
