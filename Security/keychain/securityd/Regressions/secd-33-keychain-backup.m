/*
 * Copyright (c) 2010,2012-2014 Apple Inc. All Rights Reserved.
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

#include "keychain/securityd/SecKeybagSupport.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecInternal.h>
#include <Security/SecItemPriv.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>

#include <AssertMacros.h>
#include "OSX/utilities/SecAKSWrappers.h"

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sqlite3.h>

#include "secd_regressions.h"
#include "SecdTestKeychainUtilities.h"
#include "server_security_helpers.h"

struct test_persistent_s {
    CFTypeRef persist[2];
    CFDictionaryRef query;
    CFDictionaryRef query1;
    CFDictionaryRef query2;
    CFMutableDictionaryRef query3;
    CFMutableDictionaryRef query4;
};

static void test_persistent(struct test_persistent_s *p)
{
    int v_eighty = 80;
    CFNumberRef eighty = CFNumberCreate(NULL, kCFNumberSInt32Type, &v_eighty);
    const char *v_data = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    const void *keys[] = {
        kSecClass,
        kSecAttrServer,
        kSecAttrAccount,
        kSecAttrPort,
        kSecAttrProtocol,
        kSecAttrAuthenticationType,
        kSecReturnPersistentRef,
        kSecValueData
    };
    const void *values[] = {
        kSecClassInternetPassword,
        CFSTR("zuigt.nl"),
        CFSTR("frtnbf"),
        eighty,
        CFSTR("http"),
        CFSTR("dflt"),
        kCFBooleanTrue,
        pwdata
    };
    CFDictionaryRef item = CFDictionaryCreate(NULL, keys, values,
                                              array_size(keys), &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);

    p->persist[0] = NULL;
    // NUKE anything we might have left around from a previous test run so we don't crash.
    SecItemDelete(item);
    ok_status(SecItemAdd(item, &p->persist[0]), "add internet password");
    CFTypeRef results = NULL;
    CFTypeRef results2 = NULL;
SKIP: {
    skip("no persistent ref", 6, ok(p->persist[0], "got back persistent ref"));

    /* Create a dict with all attrs except the data. */
    keys[(array_size(keys)) - 2] = kSecReturnAttributes;
    p->query = CFDictionaryCreate(NULL, keys, values,
                                  (array_size(keys)) - 1, &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks);
    ok_status(SecItemCopyMatching(p->query, &results), "find internet password by attr");

    const void *keys_persist[] = {
        kSecReturnAttributes,
        kSecValuePersistentRef
    };
    const void *values_persist[] = {
        kCFBooleanTrue,
        p->persist[0]
    };
    p->query2 = CFDictionaryCreate(NULL, keys_persist, values_persist,
                                   (array_size(keys_persist)), &kCFTypeDictionaryKeyCallBacks,
                                   &kCFTypeDictionaryValueCallBacks);
    ok_status(SecItemCopyMatching(p->query2, &results2), "find internet password by persistent ref");
    ok(CFEqual(results, results2 ? results2 : CFSTR("")), "same item (attributes)");

    CFReleaseNull(results);
    CFReleaseNull(results2);

    ok_status(SecItemDelete(p->query), "delete internet password");

    ok_status(!SecItemCopyMatching(p->query, &results),
              "don't find internet password by attributes");
    ok(!results, "no results");
}

    /* clean up left over from aborted run */
    if (results) {
        CFDictionaryRef cleanup = CFDictionaryCreate(NULL, (const void **)&kSecValuePersistentRef,
                                                     &results, 1, &kCFTypeDictionaryKeyCallBacks,
                                                     &kCFTypeDictionaryValueCallBacks);
        SecItemDelete(cleanup);
        CFRelease(results);
        CFRelease(cleanup);
    }

    ok_status(!SecItemCopyMatching(p->query2, &results2),
              "don't find internet password by persistent ref anymore");
    ok(!results2, "no results");

    CFReleaseNull(p->persist[0]);

    /* Add a new item and get it's persitant ref. */
    ok_status(SecItemAdd(item, &p->persist[0]), "add internet password");
    p->persist[1] = NULL;
    CFMutableDictionaryRef item2 = CFDictionaryCreateMutableCopy(NULL, 0, item);
    CFDictionarySetValue(item2, kSecAttrAccount, CFSTR("johndoe-bu"));
    // NUKE anything we might have left around from a previous test run so we don't crash.
    SecItemDelete(item2);
    ok_status(SecItemAdd(item2, &p->persist[1]), "add second internet password");
    CFMutableDictionaryRef update = NULL;
    CFStringRef server = NULL;
SKIP: {
    skip("no persistent ref", 3, ok(p->persist[0], "got back persistent ref from first internet password"));

    is(CFGetTypeID(p->persist[0]), CFDataGetTypeID(), "result is a CFData");
    p->query3 = CFDictionaryCreateMutableForCFTypes(NULL);
    CFDictionaryAddValue(p->query3, kSecValuePersistentRef, p->persist[0]);
    update = CFDictionaryCreateMutableForCFTypes(NULL);
    CFDictionaryAddValue(update, kSecAttrServer, CFSTR("zuigt.com"));
    ok_status(SecItemUpdate(p->query3, update), "update via persitant ref");

    /* Verify that the update really worked. */
    CFDictionaryAddValue(p->query3, kSecReturnAttributes, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(p->query3, &results2), "find updated internet password by persistent ref");
    server = CFDictionaryGetValue(results2, kSecAttrServer);
    ok(CFEqual(server, CFSTR("zuigt.com")), "verify attribute was modified by update");
    CFReleaseNull(results2);
    CFDictionaryRemoveValue(p->query3, kSecReturnAttributes);
}

SKIP: {
    skip("no persistent ref", 2, ok(p->persist[1], "got back persistent ref"));

    /* Verify that item2 wasn't affected by the update. */
    p->query4 = CFDictionaryCreateMutableForCFTypes(NULL);
    CFDictionaryAddValue(p->query4, kSecValuePersistentRef, p->persist[1]);
    CFDictionaryAddValue(p->query4, kSecReturnAttributes, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(p->query4, &results2), "find non updated internet password by persistent ref");
    server = CFDictionaryGetValue(results2, kSecAttrServer);
    ok(CFEqual(server, CFSTR("zuigt.nl")), "verify second items attribute was not modified by update");
    CFReleaseNull(results2);
}

    /* Delete the item via persitant ref. */
    ok_status(SecItemDelete(p->query3), "delete via persitant ref");
    is_status(SecItemCopyMatching(p->query3, &results2), errSecItemNotFound,
              "don't find deleted internet password by persistent ref");
    CFReleaseNull(results2);
    ok_status(SecItemCopyMatching(p->query4, &results2),
              "find non deleted internet password by persistent ref");
    CFReleaseNull(results2);

    CFReleaseNull(update);
    CFReleaseNull(item);
    CFReleaseNull(item2);
    CFReleaseNull(eighty);
    CFReleaseNull(pwdata);
}

static void test_persistent2(struct test_persistent_s *p)
{
    CFTypeRef results = NULL;
    CFTypeRef results2 = NULL;

    ok_status(!SecItemCopyMatching(p->query, &results),
              "don't find internet password by attributes");
    ok(!results, "no results");

    ok_status(!SecItemCopyMatching(p->query2, &results2),
              "don't find internet password by persistent ref anymore");
    ok(!results2, "no results");

SKIP:{
    ok_status(SecItemCopyMatching(p->query4, &results2), "find non updated internet password by persistent ref");
    skip("non updated internet password by persistent ref NOT FOUND!", 2, results2);
    ok(results2, "non updated internet password not found");
    CFStringRef server = CFDictionaryGetValue(results2, kSecAttrServer);
    ok(CFEqual(server, CFSTR("zuigt.nl")), "verify second items attribute was not modified by update");
    CFReleaseNull(results2);
}

    is_status(SecItemCopyMatching(p->query3, &results2), errSecItemNotFound,
              "don't find deleted internet password by persistent ref");
    CFReleaseNull(results2);
    ok_status(SecItemCopyMatching(p->query4, &results2),
              "find non deleted internet password by persistent ref");
    CFReleaseNull(results2);

    ok_status(SecItemDelete(p->query4),"Deleted internet password by persistent ref");

    CFRelease(p->query);
    CFRelease(p->query2);
    CFRelease(p->query3);
    CFRelease(p->query4);
    CFReleaseNull(p->persist[0]);
    CFReleaseNull(p->persist[1]);
}

static CFMutableDictionaryRef test_create_lockdown_identity_query(void) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(NULL);
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("test-delete-me"));
    CFDictionaryAddValue(query, kSecAttrAccessGroup, CFSTR("lockdown-identities"));
    return query;
}

static CFMutableDictionaryRef test_create_managedconfiguration_query(void) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(NULL);
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrService, CFSTR("com.apple.managedconfiguration"));
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("Public"));
    CFDictionaryAddValue(query, kSecAttrAccessGroup, CFSTR("apple"));
    return query;
}

static CFMutableDictionaryRef test_create_accounts_query(void) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(NULL);
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrService, CFSTR("com.apple.account.CloudKit.token"));
    CFDictionaryAddValue(query, kSecAttrAccessGroup, CFSTR("apple"));
    return query;
}

static void test_add_lockdown_identity_items(void) {
    CFMutableDictionaryRef query = test_create_lockdown_identity_query();
    const char *v_data = "lockdown identity data (which should be a cert + key)";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    CFDictionaryAddValue(query, kSecValueData, pwdata);
    ok_status(SecItemAdd(query, NULL), "test_add_lockdown_identity_items");
    CFReleaseSafe(pwdata);
    CFReleaseSafe(query);
}

static void test_remove_lockdown_identity_items(void) {
    CFMutableDictionaryRef query = test_create_lockdown_identity_query();
    ok_status(SecItemDelete(query), "test_remove_lockdown_identity_items");
    CFReleaseSafe(query);
}

static void test_no_find_lockdown_identity_item(void) {
    CFMutableDictionaryRef query = test_create_lockdown_identity_query();
    is_status(SecItemCopyMatching(query, NULL), errSecItemNotFound,
              "test_no_find_lockdown_identity_item");
    CFReleaseSafe(query);
}

static CFMutableDictionaryRef test_create_sysbound_query(void) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(NULL);
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("sysbound"));
    CFDictionaryAddValue(query, kSecAttrAccessGroup, CFSTR("apple"));
    return query;
}

static void test_add_sysbound_item(void) {
    CFMutableDictionaryRef query = test_create_sysbound_query();
    int32_t val = kSecSecAttrSysBoundPreserveDuringRestore;
    CFNumberRef num = CFNumberCreate(NULL, kCFNumberSInt32Type, &val);
    CFDictionaryAddValue(query, kSecAttrSysBound, num);
    CFReleaseNull(num);

    const char *v_data = "sysbound identity data";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    CFDictionaryAddValue(query, kSecValueData, pwdata);
    ok_status(SecItemAdd(query, NULL), "test_add_sysbound_item");
    CFReleaseSafe(pwdata);
    CFReleaseSafe(query);
}

static void test_remove_sysbound_item(void) {
    CFMutableDictionaryRef query = test_create_sysbound_query();
    ok_status(SecItemDelete(query), "test_remove_sysbound_item");
    CFReleaseSafe(query);
}

static void test_find_sysbound_item(OSStatus expectedCode) {
    CFMutableDictionaryRef query = test_create_sysbound_query();
    is_status(SecItemCopyMatching(query, NULL), expectedCode,
              "test_find_sysbound_item");
    CFReleaseSafe(query);
}

/*
 * BT
 */

static CFMutableDictionaryRef test_create_bt_query(bool sync) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(NULL);
    CFDictionaryAddValue(query, kSecClass, kSecClassGenericPassword);
    CFDictionaryAddValue(query, kSecAttrAccessGroup, CFSTR("com.apple.bluetooth"));
    CFDictionaryAddValue(query, kSecAttrSynchronizable, sync ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(query, kSecAttrAccount, sync ? CFSTR("sync") : CFSTR("non-sync"));
    return query;
}

static void test_add_bt_items(const char *data) {
    CFMutableDictionaryRef query = NULL;

    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)data, strlen(data));

    query = test_create_bt_query(false);
    (void)SecItemDelete(query);
    CFDictionarySetValue(query, kSecValueData, pwdata);
    ok_status(SecItemAdd(query, NULL), "test_add_bt_item(nonsync)");
    CFReleaseSafe(query);

    query = test_create_bt_query(true);
    (void)SecItemDelete(query);
    CFDictionarySetValue(query, kSecValueData, pwdata);
    ok_status(SecItemAdd(query, NULL), "test_add_bt_item(sync)");
    CFReleaseSafe(query);

    CFReleaseSafe(pwdata);
}

static void test_find_bt_item(OSStatus expectedCode, bool sync, const char *data) {
    CFMutableDictionaryRef query = test_create_bt_query(sync);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDataRef pwdata = NULL;
    is_status(SecItemCopyMatching(query, (CFTypeRef *)&pwdata), expectedCode,
              "test_find_bt_item: %s", data);
    CFIndex len = strlen(data);
    is(len, CFDataGetLength(pwdata), "length wrong(%s)", data);
    ok(memcmp(data, CFDataGetBytePtr(pwdata), len) == 0, "length wrong(%s)", data);
    CFReleaseSafe(query);
}

/*
 * MC
 */

static void test_add_managedconfiguration_item(void) {
    CFMutableDictionaryRef query = test_create_managedconfiguration_query();
    const char *v_data = "public managedconfiguration password history data";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    CFDictionaryAddValue(query, kSecValueData, pwdata);
    ok_status(SecItemAdd(query, NULL), "test_add_managedconfiguration_item");
    CFReleaseSafe(pwdata);
    CFReleaseSafe(query);
}

static void test_find_managedconfiguration_item(void) {
    CFMutableDictionaryRef query = test_create_managedconfiguration_query();
    ok_status(SecItemCopyMatching(query, NULL), "test_find_managedconfiguration_item");
    ok_status(SecItemDelete(query), "test_find_managedconfiguration_item (deleted)");
    CFReleaseSafe(query);
}

/*
 * Accounts
 */

static void test_add_accounts_item(const char *string) {
    CFMutableDictionaryRef query = test_create_accounts_query();
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)string, strlen(string));
    CFDictionaryAddValue(query, kSecValueData, pwdata);
    ok_status(SecItemAdd(query, NULL), "test_add_accounts_item");
    CFReleaseSafe(pwdata);
    CFReleaseSafe(query);
}

static char *test_find_accounts_item(void) {
    CFMutableDictionaryRef query = test_create_accounts_query();
    CFDataRef data = NULL;
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(query, (CFTypeRef *)&data), "test_find_accounts_item");
    if (data == NULL)
        return NULL;
    ok(isData(data), "data");
    size_t len = CFDataGetLength(data);
    char *str = malloc(len + 1);
    memcpy(str, CFDataGetBytePtr(data), len);
    str[len] = '\0';
    CFReleaseSafe(query);
    CFReleaseSafe(data);
    return str;
}

static void test_delete_accounts_item(void) {
    CFMutableDictionaryRef query = test_create_accounts_query();
    ok_status(SecItemDelete(query), "test_delete_accounts_item");
    CFReleaseSafe(query);
}



#if USE_KEYSTORE
#define DATA_ARG(x) (x) ? CFDataGetBytePtr((x)) : NULL, (x) ? (int)CFDataGetLength((x)) : 0
static CFDataRef create_keybag(keybag_handle_t bag_type, CFDataRef password)
{
    CFDataRef result = NULL;
    void *bag = NULL;
    int bagLen = 0;

    keybag_handle_t handle = bad_keybag_handle;
    require_noerr(aks_create_bag(DATA_ARG(password), bag_type, &handle), out);
    require_noerr(aks_save_bag(handle, &bag, &bagLen), out);

    result = CFDataCreate(kCFAllocatorDefault, bag, bagLen);
    out:
    return result;
}
#endif

/* Test low level keychain migration from device to device interface. */
static void tests(void)
{
    {
        CFMutableDictionaryRef lock_down_query = test_create_lockdown_identity_query();
        (void)SecItemDelete(lock_down_query);
        CFReleaseNull(lock_down_query);
    }

    int v_eighty = 80;
    CFNumberRef eighty = CFNumberCreate(NULL, kCFNumberSInt32Type, &v_eighty);
    const char *v_data = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    CFMutableDictionaryRef query = CFDictionaryCreateMutableForCFTypes(NULL);
    CFDictionaryAddValue(query, kSecClass, kSecClassInternetPassword);
    CFDictionaryAddValue(query, kSecAttrServer, CFSTR("members.spamcop.net"));
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("smith"));
    CFDictionaryAddValue(query, kSecAttrPort, eighty);
    CFDictionaryAddValue(query, kSecAttrProtocol, kSecAttrProtocolHTTP);
    CFDictionaryAddValue(query, kSecAttrAuthenticationType, kSecAttrAuthenticationTypeDefault);
    CFDictionaryAddValue(query, kSecValueData, pwdata);
    // NUKE anything we might have left around from a previous test run so we don't crash.
    (void)SecItemDelete(query);

    ok_status(SecItemAdd(query, NULL), "add internet password");
    is_status(SecItemAdd(query, NULL), errSecDuplicateItem,
              "add internet password again");

    ok_status(SecItemCopyMatching(query, NULL), "Found the item we added");

    struct test_persistent_s p = {};
    test_persistent(&p);

    CFDataRef backup = NULL, keybag = NULL, password = NULL;

    test_add_lockdown_identity_items();
    test_add_sysbound_item();
    test_add_accounts_item("1");
    test_add_bt_items("kaka1");

#if USE_KEYSTORE
    keybag = create_keybag(kAppleKeyStoreBackupBag, password);
#else
    keybag = CFDataCreate(kCFAllocatorDefault, NULL, 0);
#endif

    ok(backup = _SecKeychainCopyBackup(keybag, password),
       "_SecKeychainCopyBackup");

    test_add_managedconfiguration_item();
    test_delete_accounts_item();
    test_add_accounts_item("2");
    test_remove_lockdown_identity_items();
    test_remove_sysbound_item();

    test_add_bt_items("kaka2");

    ok_status(_SecKeychainRestoreBackup(backup, keybag, password),
              "_SecKeychainRestoreBackup");
    CFReleaseSafe(backup);

    test_no_find_lockdown_identity_item();
    test_find_sysbound_item(errSecItemNotFound);
    test_find_managedconfiguration_item();
    char *val = test_find_accounts_item();
    eq_string(val, "2", "string not 2 as expected: %s", val);
    test_delete_accounts_item();

    /*
     * Check that the kaka1 entry was "overwritten"
     */
    test_find_bt_item(errSecSuccess, true, "kaka2");
    test_find_bt_item(errSecSuccess, false, "kaka1");

    ok_status(SecItemCopyMatching(query, NULL),
              "Found the item we added after restore");

    test_persistent2(&p);

#if USE_KEYSTORE
    CFReleaseNull(keybag);
    keybag = create_keybag(kAppleKeyStoreOTABackupBag, password);
#endif

    ok(backup = _SecKeychainCopyBackup(keybag, password),
       "_SecKeychainCopyBackup");
    ok_status(_SecKeychainRestoreBackup(backup, keybag, password),
              "_SecKeychainRestoreBackup");
    ok_status(SecItemCopyMatching(query, NULL),
              "Found the item we added after restore");
    CFReleaseNull(backup);

    // force tombstone to be added, since it's not the default behavior per rdar://14680869
    CFDictionaryAddValue(query, kSecUseTombstones, kCFBooleanTrue);

    ok_status(SecItemDelete(query), "Deleted item we added");

#if USE_KEYSTORE
    CFReleaseNull(keybag);
    keybag = create_keybag(kAppleKeyStoreOTABackupBag /* use truthiness bag once it's there */, password);
#endif

    // add syncable item
    CFDictionaryAddValue(query, kSecAttrSynchronizable, kCFBooleanTrue);
    ok_status(SecItemAdd(query, NULL), "add internet password");

    // and non-syncable item
    test_add_managedconfiguration_item();

    CFDictionaryRef syncableBackup = NULL;

    CFErrorRef error = NULL;
    CFDictionaryRef scratch = NULL;
SKIP: {
    skip("skipping syncable backup tests", 7,
         ok_status(_SecKeychainBackupSyncable(keybag, password, NULL, &syncableBackup), "export items"));

    // TODO: add item, call SecServerCopyTruthInTheCloud again

    // CFShow(syncableBackup);

    // find and delete
    skip("skipping syncable backup tests", 6,
         ok_status(SecItemCopyMatching(query, NULL), "find item we are about to destroy"));

    skip("skipping syncable backup tests", 5,
         ok_status(SecItemDelete(query), "delete item we backed up"));

    // ensure we added a tombstone
    CFDictionaryAddValue(query, kSecAttrTombstone, kCFBooleanTrue);
    skip("skipping syncable backup tests", 4,
         ok_status(SecItemCopyMatching(query, NULL), "find tombstone for item we deleted"));
    CFDictionaryRemoveValue(query, kSecAttrTombstone);

    test_find_managedconfiguration_item(); // <- 2 tests here

    // TODO: add a different new item - delete what's not in the syncableBackup?

    // Do another backup after some changes
    skip("skipping syncable backup tests", 1,
         ok_status(_SecKeychainBackupSyncable(keybag, password, syncableBackup, &scratch), "export items after changes"));

    skip("skipping syncable backup tests", 0,
         ok_status(_SecKeychainRestoreSyncable(keybag, password, syncableBackup), "import items"));
}
    CFReleaseNull(scratch);
    CFReleaseNull(error);

    // non-syncable item should (still) be gone -> add should work
    test_add_managedconfiguration_item();
    test_find_managedconfiguration_item();

    // syncable item should have not been restored, because the tombstone was newer than the item in the backup -> copy matching should fail
    is_status(errSecItemNotFound, SecItemCopyMatching(query, NULL),
              "find restored item");
    is_status(errSecItemNotFound, SecItemDelete(query), "delete restored item");

    CFReleaseSafe(syncableBackup);
    CFReleaseSafe(keybag);
    CFReleaseSafe(eighty);
    CFReleaseSafe(pwdata);
    CFReleaseSafe(query);
}

int secd_33_keychain_backup(int argc, char *const *argv)
{
    plan_tests(85);

    CFArrayRef currentACL = CFRetainSafe(SecAccessGroupsGetCurrent());

    NSMutableArray *newACL = [NSMutableArray arrayWithArray:(__bridge NSArray *)currentACL];
    [newACL addObjectsFromArray:@[
                                  @"com.apple.bluetooth",
                                  @"lockdown-identities",
                                  @"apple",
                                  ]];

    SecAccessGroupsSetCurrent((__bridge CFArrayRef)newACL);


    secd_test_setup_temp_keychain("secd_33_keychain_backup", NULL);

    tests();

    SecAccessGroupsSetCurrent(currentACL);
    CFReleaseNull(currentACL);

    return 0;
}
