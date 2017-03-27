/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#ifndef kc_30_xara_helpers_h
#define kc_30_xara_helpers_h

#include <Security/Security.h>
#include <Security/cssmapi.h>
#include <security_utilities/debugging.h>
#include "utilities/SecCFRelease.h"
#include "kc-helpers.h"

#if TARGET_OS_MAC

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"

/* name is the name of the test, not the name of the keychain */
static SecKeychainRef newKeychain(const char * name) {
    SecKeychainRef kc = NULL;
    char* password = "password";

    // Kill the test keychain if it exists.
    unlink(keychainFile);
    unlink(keychainDbFile);

    // Delete from CDSA-land? No tests, here, it'll work or it won't.
    SecKeychainOpen(keychainName, &kc);
    if(kc) {
        SecKeychainDelete(kc);
        CFReleaseNull(kc);
    }

    ok_status(SecKeychainCreate(keychainName, (UInt32) strlen(password), password, false, NULL, &kc), "%s: SecKeychainCreate", name);

    char path[400];
    UInt32 len = sizeof(path);
    ok_status(SecKeychainGetPath(kc, &len, path), "%s: SecKeychainGetPath", name);
    eq_stringn(path, len, keychainDbFile, strlen(keychainDbFile), "%s: paths do not match", name);
    return kc;
}
#define newKeychainTests 3

/* name is the name of the test, not the name of the keychain */
static SecKeychainRef newCustomKeychain(const char * name, const char * path, const char * password) {
    SecKeychainRef kc = NULL;

    // Kill the keychain if it exists.
    unlink(path);

    ok_status(SecKeychainCreate(path, (UInt32) strlen(password), password, false, NULL, &kc), "%s: SecKeychainCreate", name);
    return kc;
}
#define newCustomKeychainTests 1

static SecKeychainRef openCustomKeychain(const char * name, const char * path, const char * password) {
    SecKeychainRef kc = NULL;
    ok_status(SecKeychainOpen(path, &kc), "%s: SecKeychainOpen", name);

    if(password) {
        ok_status(SecKeychainUnlock(kc, (UInt32) strlen(password), password, true), "%s: SecKeychainUnlock", name);
    } else {
        pass("make test count right");
    }

    return kc;
}
#define openCustomKeychainTests 2

static SecKeychainRef openKeychain(const char * name) {
    return openCustomKeychain(name, keychainName, NULL);
}
#define openKeychainTests (openCustomKeychainTests)

#define checkPartitionIDsTests 3
static void checkPartitionIDs(const char* name, SecKeychainItemRef item, uint32_t n) {
    if(!item) {
        for(int i = 0; i < checkPartitionIDsTests; i++) {
            fail("%s: checkNoPartitionIDs not passed an item", name);
        }
        return;
    }
    SecAccessRef access = NULL;
    ok_status(SecKeychainItemCopyAccess(item, &access), "%s: SecKeychainItemCopyAccess", name);

    CFArrayRef acllist = NULL;
    ok_status(SecAccessCopyACLList(access, &acllist), "%s: SecAccessCopyACLList", name);

    int partitionIDsFound = 0;
    CFStringRef output = NULL;

    if(acllist) {
        for(int i = 0; i < CFArrayGetCount(acllist); i++) {
            SecACLRef acl = (SecACLRef) CFArrayGetValueAtIndex(acllist, i);

            CFArrayRef auths = SecACLCopyAuthorizations(acl);
            CFRange searchrange = {0, CFArrayGetCount(auths)};
            if(CFArrayContainsValue(auths, searchrange, kSecACLAuthorizationPartitionID)) {

                // found a hash. match it.
                partitionIDsFound++;
            }

            CFReleaseNull(auths);
        }

        CFReleaseNull(acllist);
    }

    is(partitionIDsFound, n, "%s: Wrong number of partition IDs found", name);
}

#define getIntegrityHashTests 3
static CFStringRef getIntegrityHash(const char* name, SecKeychainItemRef item) {
    if(!item) {
        for(int i = 0; i < getIntegrityHashTests; i++) {
            fail("%s: getIntegrityHash not passed an item", name);
        }
        return NULL;
    }
    SecAccessRef access = NULL;
    ok_status(SecKeychainItemCopyAccess(item, &access), "%s: SecKeychainItemCopyAccess", name);

    CFArrayRef acllist = NULL;
    ok_status(SecAccessCopyACLList(access, &acllist), "%s: SecAccessCopyACLList", name);

    int hashesFound = 0;
    CFStringRef output = NULL;

    if(acllist) {
        for(int i = 0; i < CFArrayGetCount(acllist); i++) {
            SecACLRef acl = (SecACLRef) CFArrayGetValueAtIndex(acllist, i);

            CFArrayRef auths = SecACLCopyAuthorizations(acl);
            CFRange searchrange = {0, CFArrayGetCount(auths)};
            if(CFArrayContainsValue(auths, searchrange, kSecACLAuthorizationIntegrity)) {

                CFArrayRef applications = NULL;
                CFStringRef description = NULL;
                SecKeychainPromptSelector selector;
                SecACLCopyContents(acl, &applications, &description, &selector);

                // found a hash. match it.
                hashesFound++;

                output = description;
            }

            CFReleaseNull(auths);
        }

        CFReleaseNull(acllist);
    }

    is(hashesFound, 1, "%s: Wrong number of hashes found", name);
    return output;
}

// Pulls the Integrity hash out of an item and compares it against the given one.
static void checkIntegrityHash(const char* name, SecKeychainItemRef item, CFStringRef expectedHash) {
    CFStringRef hash = getIntegrityHash(name, item);

    if(!hash) {
        fail("No hash to match");
        return;
    }

    // We can't use use the ok macro here, because we
    // might run it too many times and mess up the test count.
    if(CFStringCompare(expectedHash, hash, 0) == kCFCompareEqualTo) {
        pass("Hashes match.");
    } else {
        printf("%s: Hashes didn't match. Was: ", name);
        fflush(stdout);
        CFShow(hash);
        printf(" expected: ");
        fflush(stdout);
        CFShow(expectedHash);
        fflush(stdout);
        fail("Hashes don't match");
    }
}
#define checkIntegrityHashTests (getIntegrityHashTests + 1)

static void checkHashesMatch(const char* name, SecKeychainItemRef item, SecKeychainItemRef comp) {
    CFStringRef itemhash    = getIntegrityHash(name, item);
    CFStringRef comparehash = getIntegrityHash(name, comp);

    if(!itemhash) {
        fail("%s: original item not passed in", name);
        return;
    }
    if(!comparehash) {
        fail("%s: compare item not passed in", name);
        return;
    }

    is(CFStringCompare(itemhash, comparehash, 0), kCFCompareEqualTo, "%s: hashes do not match", name);
    if(CFStringCompare(itemhash, comparehash, 0) != kCFCompareEqualTo) {
        fflush(stdout);
        CFShow(itemhash);
        CFShow(comparehash);
    }
}
#define checkHashesMatchTests (getIntegrityHashTests + getIntegrityHashTests + 1)

#pragma clang diagnostic pop
#else

#endif /* TARGET_OS_MAC */

#endif /* kc_30_xara_helpers_h */
