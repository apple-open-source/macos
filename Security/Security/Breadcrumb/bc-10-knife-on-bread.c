/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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


#include <Security/Security.h>
#include <Security/SecBreadcrumb.h>

#include "breadcrumb_regressions.h"

static void
print_hex(const char *label, CFDataRef data)
{
    CFIndex count, n;
    printf("%s = ", label);
    const uint8_t *ptr = CFDataGetBytePtr(data);
    count = CFDataGetLength(data);
    for (n = 0; n < count; n++) {
        printf("%02x", ptr[n]);
    }
    printf("\n");
}



#define kTestCount 6
int bc_10_password(int argc, char *const *argv)
{
    CFDataRef breadcrumb = NULL, encryptedKey = NULL;
    CFStringRef oldPassword = NULL;
    CFStringRef password = CFSTR("password");
    CFStringRef newpassword = CFSTR("newpassword");
    CFErrorRef error = NULL;

    plan_tests(kTestCount);

    ok(SecBreadcrumbCreateFromPassword(password, &breadcrumb, &encryptedKey, &error), "wrap failed");

    ok(SecBreadcrumbCopyPassword(password, breadcrumb, encryptedKey, &oldPassword, NULL), "unwrap failed");

    ok(oldPassword && CFStringCompare(password, oldPassword, 0) == kCFCompareEqualTo, "not same password");
    CFRelease(oldPassword);
    
    print_hex("encrypted key before", encryptedKey);
    
    CFDataRef newEncryptedKey;

    
    printf("changing password from \"password\" to \"newpassword\"\n");

    newEncryptedKey = SecBreadcrumbCreateNewEncryptedKey(password,
                                                         newpassword,
                                                         encryptedKey,
                                                         &error);
    ok(newEncryptedKey, "no new encrypted key");
    
    print_hex("encrypted key after", newEncryptedKey);

    
    ok(SecBreadcrumbCopyPassword(newpassword, breadcrumb, newEncryptedKey, &oldPassword, NULL), "unwrap failed");
    
    ok(oldPassword && CFStringCompare(password, oldPassword, 0) == kCFCompareEqualTo, "not same password");

    CFRelease(breadcrumb);
    CFRelease(oldPassword);
    CFRelease(newEncryptedKey);

    return 0;
}
