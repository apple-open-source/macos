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

#include <CoreFoundation/CoreFoundation.h>
#include <TargetConditionals.h>
#include <stdio.h>

#include "keychain_regressions.h"
#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <libaks.h>
#include <AssertMacros.h>


static CFDataRef create_keybag(keybag_handle_t bag_type, CFDataRef password)
{
    keybag_handle_t handle = bad_keybag_handle;
    
    if (aks_create_bag(NULL, 0, bag_type, &handle) == 0) {
        void * keybag = NULL;
        int keybag_size = 0;
        if (aks_save_bag(handle, &keybag, &keybag_size) == 0) {
            return CFDataCreate(kCFAllocatorDefault, keybag, keybag_size);
        }
    }
    
    return CFDataCreate(kCFAllocatorDefault, NULL, 0);
}

/* Test low level keychain migration from device to device interface. */
static void tests(void)
{
    int v_eighty = 80;
    CFNumberRef eighty = CFNumberCreate(NULL, kCFNumberSInt32Type, &v_eighty);
    const char *v_data = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFTypeRef result = NULL;
    CFDictionaryAddValue(query, kSecClass, kSecClassInternetPassword);
    CFDictionaryAddValue(query, kSecAttrServer, CFSTR("members.spamcop.net"));
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("smith"));
    CFDictionaryAddValue(query, kSecAttrPort, eighty);
    CFDictionaryAddValue(query, kSecAttrProtocol, kSecAttrProtocolHTTP);
    CFDictionaryAddValue(query, kSecAttrAuthenticationType, kSecAttrAuthenticationTypeDefault);
    CFDictionaryAddValue(query, kSecValueData, pwdata);
    CFDictionaryAddValue(query, kSecAttrSynchronizable, kCFBooleanTrue);
    
    CFDataRef keybag = NULL, password = NULL;
    
    keybag = create_keybag(kAppleKeyStoreAsymmetricBackupBag, password);
    
    SecItemDelete(query);
    
    // add syncable item
    ok_status(SecItemAdd(query, NULL), "add internet password");
    
    ok_status(SecItemCopyMatching(query, &result), "find item we are about to destroy");
    if (result) { CFRelease(result); result = NULL; }

    CFDictionaryRef backup = NULL;
    
    ok_status(_SecKeychainBackupSyncable(keybag, password, NULL, &backup), "export items");
    
    ok_status(SecItemDelete(query), "delete item we backed up");
    is_status(SecItemCopyMatching(query, &result), errSecItemNotFound, "find item we are about to destroy");
    if (result) { CFRelease(result); result = NULL; }
    
    ok_status(_SecKeychainRestoreSyncable(keybag, password, backup), "import items");
    
    ok_status(SecItemCopyMatching(query, &result), "find restored item");
    if (result) { CFRelease(result); result = NULL; }
    
    ok_status(SecItemDelete(query), "delete restored item");
    
    if (backup) { CFRelease(backup); }
}

int si_33_keychain_backup(int argc, char *const *argv)
{
	plan_tests(8);
    
    
	tests();
    
	return 0;
}
