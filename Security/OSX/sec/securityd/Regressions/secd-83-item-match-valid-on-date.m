//
//  secd-83-item-match-valid-on-date.m
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
    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    [dateFormatter setDateFormat:@"yyyy-MM-dd HH:mm:ss zzz"];
    [dateFormatter setLocale:[[NSLocale alloc] initWithLocaleIdentifier:@"us_EN"]];
    NSDate *validDate = [dateFormatter dateFromString: @"2016-04-07 16:00:00 GMT"];
    NSDate *dateBefore = [dateFormatter dateFromString: @"2016-04-06 16:00:00 GMT"];
    NSDate *dateAfter = [dateFormatter dateFromString: @"2017-04-08 16:00:00 GMT"];

    CFTypeRef result = NULL;
    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 6);
    CFReleaseNull(result);

    ok_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchValidOnDate : validDate,
                                                                returnKeyName : @YES }, &result));
    ok(result && CFArrayGetCount(result) == 6);

    is_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchValidOnDate : dateBefore,
                                                                returnKeyName : @YES }, &result), errSecItemNotFound);
    ok(result && CFArrayGetCount(result) == 6);
    is_status(SecItemCopyMatching( (__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassCertificate,
                                                                (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                                                (id)kSecMatchValidOnDate : dateAfter,
                                                                returnKeyName : @YES }, &result), errSecItemNotFound);
    ok(result && CFArrayGetCount(result) == 6);
}

int secd_83_item_match_valid_on_date(int argc, char *const *argv)
{
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    plan_tests(39);

    @autoreleasepool {
    addTestCertificates();
        NSArray *returnKeyNames = @[(id)kSecReturnAttributes, (id)kSecReturnData, (id)kSecReturnRef, (id)kSecReturnPersistentRef];
        for (id returnKeyName in returnKeyNames)
            test(returnKeyName);
    }

    return 0;
}
