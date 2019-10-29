//
//  secd-83-item-match-trusted.m
//  sec


/*
 * This is to fool os services to not provide the Keychain manager
 * interface tht doens't work since we don't have unified headers
 * between iOS and OS X. rdar://23405418/
 */
#define __KEYCHAINCORE__ 1

#import <Foundation/Foundation.h>
#import <Security/SecItem.h>
#import <Security/SecBase.h>
#import <utilities/SecCFWrappers.h>


#import "secd_regressions.h"
#import "SecdTestKeychainUtilities.h"
#import "secd-83-item-match.h"

static void test(id returnKeyName) {
    CFTypeRef result = NULL;
    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 6);
    CFReleaseNull(result);

    is_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchTrustedOnly : @YES,
                                                                returnKeyName : @YES }, &result), errSecItemNotFound);
    CFReleaseNull(result);
}

int secd_83_item_match_trusted(int argc, char *const *argv)
{
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    plan_tests(19);

    @autoreleasepool {
        addTestCertificates();
        NSArray *returnKeyNames = @[(id)kSecReturnAttributes, (id)kSecReturnData, (id)kSecReturnRef, (id)kSecReturnPersistentRef];
        for (id returnKeyName in returnKeyNames)
            test(returnKeyName);
    }
    
    return 0;
}
