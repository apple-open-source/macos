/*
 * Copyright (c) 2008-2010,2013-2014,2016 Apple Inc. All Rights Reserved.
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
#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecInternal.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecCFWrappers.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sqlite3.h>

#include "secd_regressions.h"

#include <securityd/SecItemServer.h>

#include "SecdTestKeychainUtilities.h"


#if !(TARGET_OS_IOS && TARGET_OS_SIMULATOR)
static void setKeychainPermissions(int perm) {
    CFStringRef kc_path_cf = __SecKeychainCopyPath();
    CFStringPerformWithCString(kc_path_cf, ^(const char *path) {
        ok_unix(chmod(path, perm), "chmod keychain file %s to be %d", path, perm);
    });
}
#endif

int secd_31_keychain_unreadable(int argc, char *const *argv)
{
#if TARGET_OS_IOS && TARGET_OS_SIMULATOR
    // When running on iOS device in debugger, the target usually runs
    // as root, which means it has access to the file even after setting 000.
    return 0;
#else
    plan_tests(10 + kSecdTestSetupTestCount);
    secd_test_setup_temp_keychain("secd_31_keychain_unreadable", ^{
        CFStringRef keychain_path_cf = __SecKeychainCopyPath();
        
        CFStringPerformWithCString(keychain_path_cf, ^(const char *keychain_path) {
            int fd;
            ok_unix(fd = open(keychain_path, O_RDWR | O_CREAT | O_TRUNC, 0644),
                    "create keychain file '%s'", keychain_path);
            ok_unix(fchmod(fd, 0), " keychain file '%s'", keychain_path);
            ok_unix(close(fd), "close keychain file '%s'", keychain_path);
            
        });
        
        CFReleaseSafe(keychain_path_cf);
    });
    
    int v_eighty = 80;
    CFNumberRef eighty = CFNumberCreate(NULL, kCFNumberSInt32Type, &v_eighty);
    const char *v_data = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(query, kSecClass, kSecClassInternetPassword);
    CFDictionaryAddValue(query, kSecAttrServer, CFSTR("members.spamcop.net"));
    CFDictionaryAddValue(query, kSecAttrAccount, CFSTR("smith"));
    CFDictionaryAddValue(query, kSecAttrPort, eighty);
    CFDictionaryAddValue(query, kSecAttrProtocol, kSecAttrProtocolHTTP);
    CFDictionaryAddValue(query, kSecAttrAuthenticationType, kSecAttrAuthenticationTypeDefault);
    CFDictionaryAddValue(query, kSecValueData, pwdata);
    
    is_status(SecItemAdd(query, NULL), errSecNotAvailable, "Cannot add items to unreadable keychain");
    is_status(SecItemCopyMatching(query, NULL), errSecNotAvailable, "Cannot read items in unreadable keychain");
    
    setKeychainPermissions(0644);
    
    ok_status(SecItemAdd(query, NULL), "Add internet password");
    is_status(SecItemAdd(query, NULL), errSecDuplicateItem,
              "Add internet password again");
    ok_status(SecItemCopyMatching(query, NULL), "Found the item we added");
    
    // For commented tests need to convince secd to let go of connections.
    // Without intervention it keeps them and accesses continue to succeed.
    /*
     setKeychainPermissions(0);
     is_status(SecItemCopyMatching(query, NULL), errSecNotAvailable, "Still cannot read items in unreadable keychain");
     
     setKeychainPermissions(0644);
     ok_status(SecItemCopyMatching(query, NULL), "Found the item again");
     */
    ok_status(SecItemDelete(query),"Deleted the item we added");
    
    CFReleaseNull(eighty);
    CFReleaseNull(pwdata);
    CFReleaseNull(query);
#endif  // !(TARGET_OS_IOS && TARGET_OS_SIMULATOR)
    return 0;
}
