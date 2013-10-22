/*
 *  secd-31-keychain-unreadable.c
 *  Security
 *
 *  Created by Michael Brouwer on 5/23/08.
 *  Copyright (c) 2008-2010,2013 Apple Inc.  All Rights Reserved.
 *
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


/* Create an empty keychain file that can't be read or written and make sure
   securityd can deal with it. */
static void tests(void)
{
    /* custom keychain dir */
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
    ok_status(SecItemAdd(query, NULL), "add internet password");
    is_status(SecItemAdd(query, NULL), errSecDuplicateItem,
	"add internet password again");

    ok_status(SecItemCopyMatching(query, NULL), "Found the item we added");

    ok_status(SecItemDelete(query),"Deleted the item we added");

    CFReleaseSafe(eighty);
    CFReleaseSafe(pwdata);
    CFReleaseSafe(query);
}

int secd_31_keychain_unreadable(int argc, char *const *argv)
{
	plan_tests(7 + kSecdTestSetupTestCount);
	tests();

	return 0;
}
