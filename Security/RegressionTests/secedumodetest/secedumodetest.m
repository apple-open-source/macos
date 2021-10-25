//
//  Copyright 2016 Apple. All rights reserved.
//

#include <Foundation/Foundation.h>
#include <Security/Security.h>
#include <Security/SecItemPriv.h>
#include <TargetConditionals.h>
#include <err.h>


int main (int argc, const char * argv[])
{
    @autoreleasepool {

        NSDictionary *addItem = @{
              (id)kSecClass : (id)kSecClassGenericPassword,
              (id)kSecAttrLabel : @"vpn-test-label",
              (id)kSecAttrAccount : @"vpn-test-account",
              (id)kSecValueData : @"password",
              (id)kSecUseSystemKeychain : @YES,
        };

        NSDictionary *querySystemItem = @{
            (id)kSecClass : (id)kSecClassGenericPassword,
            (id)kSecAttrLabel : @"vpn-test-label",
            (id)kSecAttrAccount : @"vpn-test-account",
            (id)kSecUseSystemKeychain : @YES,
        };

        NSDictionary *queryItem = @{
            (id)kSecClass : (id)kSecClassGenericPassword,
            (id)kSecAttrLabel : @"vpn-test-label",
            (id)kSecAttrAccount : @"vpn-test-account",
        };

        (void)SecItemDelete((__bridge CFDictionaryRef)querySystemItem);


        if (!SecItemAdd((__bridge CFDictionaryRef)addItem, NULL))
            errx(1, "failed to add");


        if (!SecItemCopyMatching((__bridge CFDictionaryRef)queryItem, NULL))
            errx(1, "failed to find in user + system");

        if (!SecItemCopyMatching((__bridge CFDictionaryRef)querySystemItem, NULL))
            errx(1, "failed to find in system");


        if (!SecItemDelete((__bridge CFDictionaryRef)querySystemItem))
            errx(1, "failed to clean up");

        return 0;
    }
}


