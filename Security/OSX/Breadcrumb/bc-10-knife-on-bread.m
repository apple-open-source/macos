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


#include <Foundation/Foundation.h>
#include <Security/Security.h>
#include <Security/SecBreadcrumb.h>
#include <utilities/SecCFRelease.h>

#include "breadcrumb_regressions.h"

static NSString *after1 = @"XAKyA0TbLKpDOBl+Ur1CQpjGDtn3wp8bYiM07iJSGVIhaaG4AAATiA==";
static NSString *bc1 = @"AdSXILtQrtsD+eT/UjMxxu4QTjlIJjvFDhpMXfk2eZ1CCJVhCuAhNcoL4DsU85DgSBCAswzVcSEU+bLMt+DT1jJfjJKVBus1Hd5lCA+N4wVtC66w3GK/WDQdGvLZ+BL86GkeRM2/+wH4/t5qOtxIJPS5SYZhnM5EP8xFYg30MLqXZqpwZhqYBJmVPMqEbLuihYAcAJreiZm4NN09CxvD36mvU3NyQOdHzAiQ+ADMiVI84qjU0qFH1KaZEoMHn3AqjAdviHUTOaNQXNepidedBZhSl4QBeuT2CaCYHjCXny9BYT+hCEU1yXn3RYeWyjcmFKmIz8gRvWf3ckF3XaSVL7MwqfsWw1tdI9OPi7zhauqphRGELw==";

static NSString *after2 = @"l/y+EOCUEeQHudNLQd5SoCJ2s/+rfH/kdbxbwZ7YGGb/U2FMAAATiA==";
static NSString *bc2 = @"AuuaJCuKmffY3XAqTYNygSFQ4QnlkSqTHGYUMaxDRA1lQhbxJh58zAOvcsahYH9lSb4+YoMR6G7hDmqlKae8h3jrn0vhT4FlIySFS3MUPvmGOuhUecb+Gi2AYwc9x1uz7f0FSRxxL+v04r2AkmH1Cv6cL7pvued7vxUjzX4VrexFj+uF7i/HSGStg2+D3L+CRs2+dKZZ9BqiKjavsX9XPkvJAD0r8rKHncOBrRxL7A3+ysBTZi2VCi/8QTDSGp6DmpXEJ4NTo/IrZ+trOXe0MuocLMg+Jf6V8jy5ZfaQoGTuM3fJiD6EFGT68QtLrjqU9KdtHhQdCmFVi60zbWqEBRNN7IyRNyPJX48NqFPZuAUW7BL0YbuhdUX2Oj7+hFz99vch1T0=";

#define kTestCount 10
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
    CFReleaseSafe(oldPassword);

    CFDataRef newEncryptedKey;

    printf("changing password from \"password\" to \"newpassword\"\n");

    newEncryptedKey = SecBreadcrumbCreateNewEncryptedKey(password,
                                                         newpassword,
                                                         encryptedKey,
                                                         &error);
    ok(newEncryptedKey, "no new encrypted key");
    
    ok(SecBreadcrumbCopyPassword(newpassword, breadcrumb, newEncryptedKey, &oldPassword, NULL), "unwrap failed");
    
    ok(oldPassword && CFStringCompare(password, oldPassword, 0) == kCFCompareEqualTo, "not same password");

    CFReleaseSafe(breadcrumb);
    CFReleaseSafe(oldPassword);
    CFReleaseSafe(newEncryptedKey);

    /*
     * Check KAT for IV less operation (version1)
     */

    breadcrumb = CFBridgingRetain([[NSData alloc] initWithBase64EncodedString:bc1 options:0]);
    newEncryptedKey = CFBridgingRetain([[NSData alloc] initWithBase64EncodedString:after1 options:0]);

    ok(SecBreadcrumbCopyPassword(newpassword, breadcrumb, newEncryptedKey, &oldPassword, NULL), "unwrap failed");

    ok(oldPassword && CFStringCompare(password, oldPassword, 0) == kCFCompareEqualTo, "not same password");

    CFReleaseSafe(breadcrumb);
    CFReleaseSafe(oldPassword);
    CFReleaseSafe(newEncryptedKey);

    /*
     * Check KAT for IV less operation (version2)
     */

    breadcrumb = CFBridgingRetain([[NSData alloc] initWithBase64EncodedString:bc2 options:0]);
    newEncryptedKey = CFBridgingRetain([[NSData alloc] initWithBase64EncodedString:after2 options:0]);

    ok(SecBreadcrumbCopyPassword(newpassword, breadcrumb, newEncryptedKey, &oldPassword, NULL), "unwrap failed");

    ok(oldPassword && CFStringCompare(password, oldPassword, 0) == kCFCompareEqualTo, "not same password");

    CFReleaseSafe(breadcrumb);
    CFReleaseSafe(oldPassword);
    CFReleaseSafe(newEncryptedKey);

    return 0;
}
