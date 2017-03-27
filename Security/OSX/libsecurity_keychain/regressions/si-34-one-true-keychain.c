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
#include <utilities/SecCFRelease.h>

#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <utilities/SecCFRelease.h>
#include <libaks.h>
#include <AssertMacros.h>


/* Test whether the one true keychain pertains to the iOS keychain and only to the iOS keychain. */
static void tests(void)
{
    int v_eighty = 80;
    CFNumberRef eighty = CFNumberCreate(NULL, kCFNumberSInt32Type, &v_eighty);
    const char *v_data = "test";
    const char *v_data2 = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    CFDataRef pwdata2 = CFDataCreate(NULL, (UInt8 *)v_data2, strlen(v_data2));
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFTypeRef result = NULL;
    CFDictionaryAddValue(query, kSecClass, kSecClassInternetPassword);
    CFDictionaryAddValue(query, kSecAttrServer, CFSTR("members.spamcop.net"));
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("smith"));
    CFDictionaryAddValue(query, kSecAttrPort, eighty); CFReleaseNull(eighty);
    CFDictionaryAddValue(query, kSecAttrProtocol, kSecAttrProtocolHTTP);
    CFDictionaryAddValue(query, kSecAttrAuthenticationType, kSecAttrAuthenticationTypeDefault);

    CFMutableDictionaryRef noLegacyQuery = CFDictionaryCreateMutableCopy(NULL, 0, query);
    CFMutableDictionaryRef syncAnyQuery = CFDictionaryCreateMutableCopy(NULL, 0, query);
    CFMutableDictionaryRef syncQuery = CFDictionaryCreateMutableCopy(NULL, 0, query);

    CFDictionaryAddValue(noLegacyQuery, kSecAttrNoLegacy, kCFBooleanTrue);
    CFDictionaryAddValue(syncAnyQuery, kSecAttrSynchronizable, kSecAttrSynchronizableAny);
    CFDictionaryAddValue(syncQuery, kSecAttrSynchronizable, kCFBooleanTrue);

    SecItemDelete(query);
    SecItemDelete(noLegacyQuery);
    SecItemDelete(syncQuery);
    SecItemDelete(syncAnyQuery);

    CFDictionaryAddValue(query, kSecValueData, pwdata);
    ok_status(SecItemAdd(query, NULL), "add internet password in OS X keychain");
    CFDictionaryRemoveValue(query, kSecValueData);

    ok_status(SecItemCopyMatching(query, &result), "find the osx item");
    CFReleaseNull(result);
    is_status(SecItemCopyMatching(noLegacyQuery, &result), errSecItemNotFound, "do not find the osx item with noLegacy");
    CFReleaseNull(result);
    ok_status(SecItemCopyMatching(syncAnyQuery, &result), "find the osx item with synchronizableAny");
    CFReleaseNull(result);
    is_status(SecItemCopyMatching(syncQuery, &result), errSecItemNotFound, "do not find the osx item with synchronizable");
    CFReleaseNull(result);

    CFMutableDictionaryRef toUpdate = CFDictionaryCreateMutable(NULL, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFDictionaryAddValue(toUpdate, kSecValueData, pwdata2);
    CFReleaseNull(pwdata2);

    ok_status(SecItemUpdate(query, toUpdate), "update the osx item");
    is_status(SecItemUpdate(noLegacyQuery, toUpdate), errSecItemNotFound, "do not update the osx item with noLegacy");
    ok_status(SecItemUpdate(syncAnyQuery, toUpdate), "update the osx item with synchronizableAny");

    is_status(SecItemDelete(noLegacyQuery), errSecItemNotFound, "do not delete the osx item with noLegacy");
    ok_status(SecItemDelete(syncAnyQuery), "delete the osx item with synchronizableAny");




    CFDictionaryAddValue(noLegacyQuery, kSecValueData, pwdata);
    CFReleaseNull(pwdata);
    ok_status(SecItemAdd(noLegacyQuery, &result), "add internet password in iOS keychain");
    CFDictionaryRemoveValue(noLegacyQuery, kSecValueData);

    ok_status(SecItemCopyMatching(query, &result), "find the ios item with generic query");
    CFReleaseNull(result);
    ok_status(SecItemCopyMatching(noLegacyQuery, &result), "find the ios item with noLegacy");
    CFReleaseNull(result);
    ok_status(SecItemCopyMatching(syncAnyQuery, &result), "find the ios item with synchronizableAny");
    CFReleaseNull(result);
    is_status(SecItemCopyMatching(syncQuery, &result), errSecItemNotFound, "do not find the ios item with synchronizable");
    CFReleaseNull(result);

    ok_status(SecItemUpdate(query, toUpdate), "update the ios item without any flags");
    ok_status(SecItemUpdate(noLegacyQuery, toUpdate), "update the ios item with noLegacy");
    ok_status(SecItemUpdate(syncAnyQuery, toUpdate), "update the ios item with synchronizableAny");

    CFDictionaryRemoveValue(noLegacyQuery, kSecValueData);

    ok_status(SecItemDelete(noLegacyQuery), "delete the item with noLegacy");

    CFReleaseNull(toUpdate);
    CFReleaseNull(query);
    CFReleaseNull(noLegacyQuery);
    CFReleaseNull(syncQuery);
    CFReleaseNull(syncAnyQuery);
}

int si_34_one_true_keychain(int argc, char *const *argv)
{
	plan_tests(19);
    
    
	tests();
    
	return 0;
}
