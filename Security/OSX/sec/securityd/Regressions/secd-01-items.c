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
#include <securityd/SecItemServer.h>

#include <utilities/array_size.h>
#include <utilities/SecFileLocations.h>

#include <unistd.h>

#include "SecdTestKeychainUtilities.h"

#if USE_KEYSTORE
#include <libaks.h>

int secd_01_items(int argc, char *const *argv)
{
    plan_tests(24 + kSecdTestSetupTestCount);

    secd_test_setup_testviews(); // if running all tests get the test views setup first
    /* custom keychain dir */
    secd_test_setup_temp_keychain("secd_01_items", NULL);

    /* custom keybag */
    keybag_handle_t keybag;
    keybag_state_t state;
    char *passcode="password";
    int passcode_len=(int)strlen(passcode);

    ok(kIOReturnSuccess==aks_create_bag(passcode, passcode_len, kAppleKeyStoreDeviceBag, &keybag), "create keybag");
    ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
    ok(!(state&keybag_state_locked), "keybag unlocked");
    SecItemServerSetKeychainKeybag(keybag);

    /* lock */
    ok(kIOReturnSuccess==aks_lock_bag(keybag), "lock keybag");
    ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
    ok(state&keybag_state_locked, "keybag locked");

    
    SecKeychainDbReset(NULL);

    /* Creating a password */
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
        kSecValueData
    };
    const void *values[] = {
        kSecClassInternetPassword,
        CFSTR("members.spamcop.net"),
        CFSTR("smith"),
        eighty,
        CFSTR("http"),
        CFSTR("dflt"),
        pwdata
    };
    
    CFDictionaryRef item = CFDictionaryCreate(NULL, keys, values,
                                              array_size(keys), NULL, NULL);

    
    is_status(SecItemAdd(item, NULL), errSecInteractionNotAllowed, "add internet password while locked");

    /* unlock */
    ok(kIOReturnSuccess==aks_unlock_bag(keybag, passcode, passcode_len), "unlock keybag");
    ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
    ok(!(state&keybag_state_locked), "keybag unlocked");

    ok_status(SecItemAdd(item, NULL), "add internet password, while unlocked");

    
    /* lock */
    ok(kIOReturnSuccess==aks_lock_bag(keybag), "lock keybag");
    ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
    ok(state&keybag_state_locked, "keybag locked");

    is_status(SecItemAdd(item, NULL), errSecInteractionNotAllowed,
              "add internet password again, while locked");

    /* unlock */
    ok(kIOReturnSuccess==aks_unlock_bag(keybag, passcode, passcode_len), "unlock keybag");
    ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
    ok(!(state&keybag_state_locked), "keybag unlocked");

    is_status(SecItemAdd(item, NULL), errSecDuplicateItem,
              "add internet password again, while unlocked");

    CFTypeRef results = NULL;
    /* Create a dict with all attrs except the data. */
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values,
                                               (array_size(keys)) - 1, NULL, NULL);
    ok_status(SecItemCopyMatching(query, &results), "find internet password, while unlocked ");
    if (results) {
        CFRelease(results);
        results = NULL;
    }

    /* lock */
    ok(kIOReturnSuccess==aks_lock_bag(keybag), "lock keybag");
    ok(kIOReturnSuccess==aks_get_lock_state(keybag, &state), "get keybag state");
    ok(state&keybag_state_locked, "keybag locked");

    is_status(SecItemCopyMatching(query, &results), errSecInteractionNotAllowed, "find internet password, while locked ");

    /* Reset keybag and custom $HOME */
    SecItemServerResetKeychainKeybag();
    SetCustomHomePath(NULL);
    SecKeychainDbReset(NULL);

    
    CFReleaseNull(pwdata);
    CFReleaseNull(eighty);
    CFReleaseSafe(item);
    CFReleaseSafe(query);

	return 0;
}

#else

int secd_01_items(int argc, char *const *argv)
{
    plan_tests(1);

    todo("Not yet working in simulator");

TODO: {
    ok(false);
}

    /* not implemented in simulator (no keybag) */
	return 0;
}
#endif

