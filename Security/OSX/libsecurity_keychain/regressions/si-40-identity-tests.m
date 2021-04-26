/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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
 *
 */

#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <TargetConditionals.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "keychain_regressions.h"
#include <utilities/SecCFRelease.h>

#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecKeychain.h>
#include "OSX/utilities/SecCFWrappers.h"
#include "OSX/sec/Security/SecFramework.h"

#include "si-40-identity-tests_data.h"

/* entry point prototype */
int si_40_identity_tests(int argc, char *const *argv);

static void tests() {
    OSStatus status = 0;
    CFDataRef p12Blob = NULL;
    isnt(p12Blob = (__bridge CFDataRef)[NSData dataWithBytes:test_p12 length:sizeof(test_p12)], NULL, "copy test_p12");

    CFArrayRef items = NULL; /* return value is retained, must release it */
    SecKeychainRef keychain = NULL; /* return value is retained, must release it */
    status = SecKeychainCopyDefault(&keychain);
    is(status, errSecSuccess, "keychain status");
    isnt(keychain, NULL, "no default keychain");

    CFMutableDictionaryRef options = NULL; /* return value is retained, must release it */
    options = CFDictionaryCreateMutableForCFTypes(NULL);
    isnt(options, NULL, "no options dictionary");
    CFDictionaryAddValue(options, kSecImportExportPassphrase, CFSTR("test"));
    CFDictionaryAddValue(options, kSecImportExportKeychain, keychain);

    status = SecPKCS12Import(p12Blob, options, &items);
    if (status == errSecDuplicateItem) {
        status = errSecSuccess; // ok if it already exists
    }
    is(status, errSecSuccess, "import p12 status");
    isnt(items, NULL, "import p12 items");

    NSDictionary *itemDict = (__bridge NSDictionary*)CFArrayGetValueAtIndex(items, 0);
    SecIdentityRef identity = (__bridge SecIdentityRef)itemDict[(__bridge NSString*)kSecImportItemIdentity];
    isnt(identity, NULL, "import identity");

    SecIdentityRef foundIdentity = NULL; /* return value is retained, must release it */

    // PLAIN NAMES: make sure these test cases produce identity preference items
    // which are not per-application.

    NSString *plainNameOne = @"Test Identity Preference Item";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameOne, NULL);
    is(status, errSecSuccess, "set preferred identity with plain name containing spaces");

    NSString *plainNameTwo = @"Test.Identity.Preference.Item";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameTwo, NULL);
    is(status, errSecSuccess, "set preferred identity with plain name containing dots");

    NSString *plainNameThree = @"@Test.Identity.Preference.Item";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameThree, NULL);
    is(status, errSecSuccess, "set preferred identity with plain name containing at-sign");

    NSString *plainNameFour = @"TestIdentityPreferenceItem";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameFour, NULL);
    is(status, errSecSuccess, "set preferred identity with plain name containing no spaces");

    NSString *plainNameFive = @"*.Identity.Preference.Item";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameFive, NULL);
    is(status, errSecSuccess, "set preferred identity with plain name wildcard entry");

    NSString *plainNameSix = @"si-40-identity-tests@apple.com";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)plainNameSix, NULL);
    is(status, errSecSuccess, "set preferred identity with RFC822 email address");

    status = SecIdentityDeleteApplicationPreferenceItems();
    if (status == errSecItemNotFound) {
        // it's ok if there were no per-app items found to delete at this point.
        // We are only testing that calling this function does NOT delete any of the
        // plain name preferences we created above.
        status = errSecSuccess;
    }
    is(status, errSecSuccess, "per-app preference item deletion failed with unexpected error");

    // check that the plain name prefs exist and survived the per-app item deletion.
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameOne, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 1 should not be deleted");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameTwo, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 2 should not be deleted");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameThree, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 3 should not be deleted");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameFour, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 4 should not be deleted");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameFive, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 5 should not be deleted");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameSix, NULL, NULL);
    isnt(foundIdentity, NULL, "plain name identity preference 6 should not be deleted");
    CFReleaseNull(foundIdentity);

    // clear the plain name prefs
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameOne, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 1");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameOne, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 1 found after being cleared");
    CFReleaseNull(foundIdentity);
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameTwo, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 2");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameTwo, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 2 found after being cleared");
    CFReleaseNull(foundIdentity);
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameThree, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 3");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameThree, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 3 found after being cleared");
    CFReleaseNull(foundIdentity);
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameFour, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 4");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameFour, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 4 found after being cleared");
    CFReleaseNull(foundIdentity);
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameFive, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 5");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameFive, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 5 found after being cleared");
    CFReleaseNull(foundIdentity);
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)plainNameSix, NULL);
    is(status, errSecSuccess, "clear preferred identity with plain name 6");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)plainNameSix, NULL, NULL);
    is(foundIdentity, NULL, "plain name identity preference 6 found after being cleared");
    CFReleaseNull(foundIdentity);

    //
    // URL NAMES: make sure that these produce per-app preference items.
    //

    NSString *uriNameOne = @"https://test-pref.apple.com/";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)uriNameOne, NULL);
    is(status, errSecSuccess, "set preferred identity with uri name 1");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriNameOne, NULL, NULL);
    isnt(foundIdentity, NULL, "preferred identity 1 not found after being set");
    CFReleaseNull(foundIdentity);

    NSString *uriNameTwo = @"ldaps://test-pref.apple.com/cn=si-40-identity-tests";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)uriNameTwo, NULL);
    is(status, errSecSuccess, "set preferred identity with uri name 2");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriNameTwo, NULL, NULL);
    isnt(foundIdentity, NULL, "preferred identity 2 not found after being set");
    CFReleaseNull(foundIdentity);

    // Check that the new API deletes all of our per-app URL preference items.
    // We always expect errSecSuccess here, since we know we have items to delete.
    status = SecIdentityDeleteApplicationPreferenceItems();
    is(status, errSecSuccess, "should find and delete our app uri preference items");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriNameOne, NULL, NULL);
    is(foundIdentity, NULL, "preferred identity 1 should not be found after deleting prefs");
    CFReleaseNull(foundIdentity);
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriNameTwo, NULL, NULL);
    is(foundIdentity, NULL, "preferred identity 2 should not be found after deleting prefs");
    CFReleaseNull(foundIdentity);

    //
    // WILDCARD NAMES: make sure these are still supported for URL name lookups
    //

    // add wildcard entry
    NSString *wildcardNameOne = @"*.test-pref-subdomain.apple.com";
    status = SecIdentitySetPreferred(identity, (__bridge CFStringRef)wildcardNameOne, NULL);
    is(status, errSecSuccess, "set preferred identity for wildcard name 1");
    // check that preferred identity is found for URI matching this wildcard
    NSString *uriWildcardMatchOne = @"https://match.test-pref-subdomain.apple.com";
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriWildcardMatchOne, NULL, NULL);
    isnt(foundIdentity, NULL, "preferred identity not found for wildcard match 1");
    CFReleaseNull(foundIdentity);
    // clear wildcard entry, then check that match is not found
    status = SecIdentitySetPreferred(NULL, (__bridge CFStringRef)wildcardNameOne, NULL);
    is(status, errSecSuccess, "clear preferred identity for wildcard name 1");
    foundIdentity = SecIdentityCopyPreferred((__bridge CFStringRef)uriWildcardMatchOne, NULL, NULL);
    is(foundIdentity, NULL, "preferred identity found after wildcard name 1 was cleared");
    CFReleaseNull(foundIdentity);


    CFReleaseNull(options);
    CFReleaseNull(items);
    CFReleaseNull(keychain);
}

int si_40_identity_tests(int argc, char *const *argv)
{
    plan_tests(43);

    tests();

    return 0;
}
