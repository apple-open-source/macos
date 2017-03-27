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
 * limitations under the xLicense.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <Security/Security.h>
#import <Security/SecCertificatePriv.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"
#include "kc-item-helpers.h"

static void tests()
{
    SecKeychainRef kc = getPopulatedTestKeychain();

    CFMutableDictionaryRef query = NULL;
    SecKeychainItemRef item = NULL;

    // Find passwords
    query = createQueryCustomItemDictionaryWithService(kc, kSecClassInternetPassword, CFSTR("test_service"), CFSTR("test_service"));
    item = checkNCopyFirst(testName, query, 1);
    readPasswordContents(item, CFSTR("test_password"));  checkPrompts(0, "after reading a password");
    changePasswordContents(item, CFSTR("new_password")); checkPrompts(0, "changing a internet password");
    readPasswordContents(item, CFSTR("new_password"));   checkPrompts(0, "reading a changed internet password");
    CFReleaseNull(item);

    query = createQueryCustomItemDictionaryWithService(kc, kSecClassInternetPassword, CFSTR("test_service_restrictive_acl"), CFSTR("test_service_restrictive_acl"));
    item = checkNCopyFirst(testName, query, 1);
    readPasswordContentsWithResult(item, errSecAuthFailed, NULL); // we don't expect to be able to read this
    checkPrompts(1, "trying to read internet password without access");

    changePasswordContents(item, CFSTR("new_password"));
    checkPrompts(0, "after changing a internet password without access"); // NOTE: we expect this write to succeed, even though we're not on the ACL. Therefore, we should see 0 prompts for this step.
    readPasswordContentsWithResult(item, errSecAuthFailed, NULL); // we don't expect to be able to read this
    checkPrompts(1, "after changing a internet password without access");
    CFReleaseNull(item);

    query = createQueryCustomItemDictionaryWithService(kc, kSecClassGenericPassword, CFSTR("test_service"), CFSTR("test_service"));
    item = checkNCopyFirst(testName, query, 1);
    readPasswordContents(item, CFSTR("test_password"));   checkPrompts(0, "after reading a generic password");
    changePasswordContents(item, CFSTR("new_password"));  checkPrompts(0, "changing a generic password");
    readPasswordContents(item, CFSTR("new_password"));    checkPrompts(0, "after changing a generic password");
    CFReleaseNull(item);

    query = createQueryCustomItemDictionaryWithService(kc, kSecClassGenericPassword, CFSTR("test_service_restrictive_acl"), CFSTR("test_service_restrictive_acl"));
    item = checkNCopyFirst(testName, query, 1);
    readPasswordContentsWithResult(item, errSecAuthFailed, NULL); // we don't expect to be able to read this
    checkPrompts(1, "trying to read generic password without access");

    changePasswordContents(item, CFSTR("new_password"));
    checkPrompts(0, "changing a generic password without access"); // NOTE: we expect this write to succeed, even though we're not on the ACL. Therefore, we should see 0 prompts for this step.
    readPasswordContentsWithResult(item, errSecAuthFailed, NULL); // we don't expect to be able to read this
    checkPrompts(1, "after changing a generic password without access");
    CFReleaseNull(item);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", testName);
    CFReleaseNull(kc);
}
#define numTests (getPopulatedTestKeychainTests + \
checkNTests + readPasswordContentsTests + checkPromptsTests + changePasswordContentsTests + checkPromptsTests + readPasswordContentsTests + checkPromptsTests + \
checkNTests + readPasswordContentsTests + checkPromptsTests + changePasswordContentsTests + checkPromptsTests + readPasswordContentsTests + checkPromptsTests + \
checkNTests + readPasswordContentsTests + checkPromptsTests + changePasswordContentsTests + checkPromptsTests + readPasswordContentsTests + checkPromptsTests + \
checkNTests + readPasswordContentsTests + checkPromptsTests + changePasswordContentsTests + checkPromptsTests + readPasswordContentsTests + checkPromptsTests + \
+ 1)

int kc_16_item_update_password(int argc, char *const *argv)
{
    plan_tests(numTests);
    initializeKeychainTests(__FUNCTION__);

    tests();

    deleteTestFiles();
    return 0;
}
