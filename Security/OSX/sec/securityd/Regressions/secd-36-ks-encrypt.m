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


#include "secd_regressions.h"

#include <Security/Security.h>

#include <utilities/SecCFWrappers.h>
#include "SecDbKeychainItem.h"

#include <TargetConditionals.h>

#if USE_KEYSTORE
#include <libaks.h>

#include "SecdTestKeychainUtilities.h"

int secd_36_ks_encrypt(int argc, char *const *argv)
{
    plan_tests(8);

    keybag_handle_t keybag;
    keybag_state_t state;
    CFDictionaryRef data = NULL;
    CFDataRef enc = NULL;
    CFErrorRef error = NULL;
    SecAccessControlRef ac = NULL;
    bool ret;

    char passcode[] = "password";
    int passcode_len = sizeof(passcode) - 1;


    /* Create and lock custom keybag */
    is(kIOReturnSuccess, aks_create_bag(passcode, passcode_len, kAppleKeyStoreDeviceBag, &keybag), "create keybag");
    is(kIOReturnSuccess, aks_get_lock_state(keybag, &state), "get keybag state");
    is(0, (int)(state&keybag_state_locked), "keybag unlocked");

    data = (__bridge CFDictionaryRef)@{
        (id)kSecValueData : @"secret here",
    };

    ok(ac = SecAccessControlCreate(NULL, &error), "SecAccessControlCreate: %@", error);
    ok(SecAccessControlSetProtection(ac, kSecAttrAccessibleWhenUnlocked, &error), "SecAccessControlSetProtection: %@", error);

    ret = ks_encrypt_data(keybag, ac, NULL, data, NULL, &enc, true, &error);
    is(true, ret);

    CFReleaseNull(ac);

    {
        CFMutableDictionaryRef attributes = NULL;
        uint32_t version = 0;

        ret = ks_decrypt_data(keybag, kAKSKeyOpDecrypt, &ac, NULL, enc, NULL, NULL, &attributes, &version, &error);
        is(true, ret, "ks_decrypt_data: %@", error);

        ok(CFEqual(SecAccessControlGetProtection(ac), kSecAttrAccessibleWhenUnlocked), "AccessControl protection is: %@", SecAccessControlGetProtection(ac));

        CFReleaseNull(ac);
    }

    CFReleaseNull(error);
    CFReleaseNull(enc);

    return 0;
}

#else /* !USE_KEYSTORE */

int secd_36_ks_encrypt(int argc, char *const *argv)
{
    plan_tests(1);
    ok(true);
    return 0;
}
#endif /* USE_KEYSTORE */
