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

#include <TargetConditionals.h>
#include <AssertMacros.h>

#if TARGET_OS_IPHONE && USE_KEYSTORE
#include <libaks.h>

#include "SecdTestKeychainUtilities.h"

#include "ios8-inet-keychain-2.h"

void SecAccessGroupsSetCurrent(CFArrayRef accessGroups);
CFArrayRef SecAccessGroupsGetCurrent();

int secd_35_keychain_migrate_inet(int argc, char *const *argv)
{
    plan_tests(11 + kSecdTestSetupTestCount);

    __block keybag_handle_t keybag;
    __block keybag_state_t state;
    char *passcode="password";
    int passcode_len=(int)strlen(passcode);

    /* custom keychain dir */
    secd_test_setup_temp_keychain("secd_35_keychain_migrate_inet", ^{
        CFStringRef keychain_path_cf = __SecKeychainCopyPath();

        CFStringPerformWithCString(keychain_path_cf, ^(const char *keychain_path) {
            writeFile(keychain_path, ios8_inet_keychain_2_db, ios8_inet_keychain_2_db_len);

            /* custom notification */
            SecItemServerSetKeychainChangedNotification("com.apple.secdtests.keychainchanged");

            /* Create and lock custom keybag */
            ok(kIOReturnSuccess==aks_create_bag(passcode, passcode_len, kAppleKeyStoreDeviceBag, &keybag), "create keybag");
            ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
            ok(!(state&keybag_state_locked), "keybag unlocked");
            SecItemServerSetKeychainKeybag(keybag);

            /* lock */
            ok(kIOReturnSuccess==aks_lock_bag(keybag), "lock keybag");
            ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
            ok(state&keybag_state_locked, "keybag locked");
        });

        CFReleaseSafe(keychain_path_cf);
    });

    CFArrayRef old_ag = SecAccessGroupsGetCurrent();
    CFMutableArrayRef test_ag = CFArrayCreateMutableCopy(NULL, 0, old_ag);
    CFArrayAppendValue(test_ag, CFSTR("com.apple.cfnetwork"));
    SecAccessGroupsSetCurrent(test_ag);

    /* querying a password */
    const void *keys[] = {
        kSecClass,
        kSecAttrAccessGroup,
        kSecAttrSynchronizable,
        kSecMatchLimit,
        kSecReturnAttributes,
    };
    const void *values[] = {
        kSecClassInternetPassword,
        CFSTR("com.apple.cfnetwork"),
        kSecAttrSynchronizableAny,
        kSecMatchLimitAll,
        kCFBooleanTrue,
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
                                               array_size(keys), NULL, NULL);
    CFTypeRef results = NULL;
    is_status(SecItemCopyMatching(query, &results), errSecInteractionNotAllowed);

    ok(kIOReturnSuccess==aks_unlock_bag(keybag, passcode, passcode_len), "lock keybag");
    ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
    ok(!(state&keybag_state_locked), "keybag unlocked");

    // We should be able to query 2 inet items from the DB here.  But the database is encrypted
    // by keybag which we do not know, so no item can be actually retrieved.  The test could be
    // improved by crafting DB to update using keybag hardcoded in the test, that way it would be possible
    // to check that 2 inet items are really retrieved here.
    is_status(SecItemCopyMatching(query, &results), errSecItemNotFound);

    /* Reset keybag */
    SecItemServerResetKeychainKeybag();

    // Reset server accessgroups.
    SecAccessGroupsSetCurrent(old_ag);
    CFReleaseSafe(test_ag);

    CFReleaseSafe(results);
    CFReleaseSafe(query);
    return 0;
}

#else

int secd_35_keychain_migrate_inet(int argc, char *const *argv)
{
    plan_tests(1);

    todo("Not yet working in simulator");

TODO: {
    ok(false);
}
    /* not implemented in simulator (no keybag) */
    /* Not implemented in OSX (no upgrade scenario) */
    return 0;
}
#endif
