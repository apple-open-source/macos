//
//  si-15-delete-access-group.m
//  sec
//
//  Created by Love Hörnquist Åstrand on 2016-06-28.
//
//

#import <Foundation/Foundation.h>
#include <Security/Security.h>
#include <Security/SecItemPriv.h>
#include <unistd.h>

#include "Security_regressions.h"


int si_15_delete_access_group(int argc, char *const *argv)
{
    plan_tests(4);

    @autoreleasepool {
        NSDictionary *query = NULL, *item = NULL;
        NSDictionary *query2 = NULL, *item2 = NULL;
        NSString *agrp = @"123456.test.group";
        NSString *agrp2 = @"123456.test.group2";
        CFErrorRef error = NULL;

        /*
         * Clean first
         */
        query = @{
             (id)kSecClass : (id)kSecClassGenericPassword,
             (id)kSecAttrLabel : @"keychain label",
        };
        query2 = @{
            (id)kSecClass : (id)kSecClassGenericPassword,
            (id)kSecAttrLabel : @"keychain label2",
        };
        SecItemDelete((CFDictionaryRef)query);
        SecItemDelete((CFDictionaryRef)query2);

        /*
         * Add entry
         */

        item = @{
            (id)kSecClass : (id)kSecClassGenericPassword,
            (id)kSecAttrLabel : @"keychain label",
            (id)kSecAttrAccessGroup : agrp
        };

        ok_status(SecItemAdd((CFDictionaryRef)item, NULL), "SecItemAdd2");

        ok_status(SecItemCopyMatching((CFDictionaryRef)query, NULL));

        item2 = @{
            (id)kSecClass : (id)kSecClassGenericPassword,
            (id)kSecAttrLabel : @"keychain label2",
            (id)kSecAttrAccessGroup : agrp2
        };

        ok_status(SecItemAdd((CFDictionaryRef)item2, NULL), "SecItemAdd2");

        is_status(SecItemCopyMatching((CFDictionaryRef)query, NULL), errSecSuccess);
        is_status(SecItemCopyMatching((CFDictionaryRef)query2, NULL), errSecSuccess);


        ok(SecItemDeleteAllWithAccessGroups((__bridge CFArrayRef)@[ agrp ], &error),
           "SecItemDeleteAllWithAccessGroups: %@", error);

        if (error)
            CFRelease(error);

        is_status(SecItemCopyMatching((CFDictionaryRef)query, NULL), errSecItemNotFound);
        is_status(SecItemCopyMatching((CFDictionaryRef)query2, NULL), errSecSuccess);

        ok(SecItemDeleteAllWithAccessGroups((__bridge CFArrayRef)@[ agrp2 ], &error),
           "SecItemDeleteAllWithAccessGroups: %@", error);

        if (error)
            CFRelease(error);

        is_status(SecItemCopyMatching((CFDictionaryRef)query, NULL), errSecItemNotFound);
        is_status(SecItemCopyMatching((CFDictionaryRef)query2, NULL), errSecItemNotFound);
    }

    return 0;
}
