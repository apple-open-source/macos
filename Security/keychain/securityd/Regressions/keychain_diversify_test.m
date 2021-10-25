//
//  keychain_diversify_test.m
//  Security
//
//  Created by Aarthi Sampath on 3/5/21.
//

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>

#include "SecurityTool/sharedTool/builtin_commands.h"
#include "keychain/securityd/SecDbItem.h"
#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemDataSource.h"

#include <utilities/array_size.h>
#include <utilities/SecFileLocations.h>

#include <unistd.h>
#include <mach/mach_time.h>

#include "keychain/securityd/Regressions/SecdTestKeychainUtilities.h"

#if USE_KEYSTORE
#include "OSX/utilities/SecAKSWrappers.h"
#include "keychain/securityd/SecKeybagSupport.h"

static void test_setup_temp_keychain(const char* test_prefix, dispatch_block_t do_in_reset)
{
    CFStringRef tmp_dir = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("/tmp/%s.%X/"), test_prefix, arc4random());
    CFStringRef keychain_dir = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@Library/Keychains"), tmp_dir);
    secnotice("secdtest", "Keychain path: %@", keychain_dir);
    
    CFStringPerformWithCString(keychain_dir, ^(const char *keychain_dir_string) {
        errno_t err = mkpath_np(keychain_dir_string, 0755);
        if(err != 0) {
            secnotice("secdtest", "Unable to make directory; issues ahead: %d", (int)err);
        }
    });
    
    
    /* set custom keychain dir, reset db */
    SecSetCustomHomeURLString(tmp_dir);

    SecKeychainDbReset(do_in_reset);

    CFReleaseNull(tmp_dir);
    CFReleaseNull(keychain_dir);
}

static bool test_teardown_delete_temp_keychain(const char* test_prefix)
{
    NSURL* keychainDir = (NSURL*)CFBridgingRelease(SecCopyHomeURL());

    SecItemDataSourceFactoryReleaseAll();
    SecKeychainDbForceClose();
    SecKeychainDbReset(NULL);

    // Only perform the desctructive step if the url matches what we expect!
    NSString* testName = [NSString stringWithUTF8String:test_prefix];

    if([keychainDir.path hasPrefix:[NSString stringWithFormat:@"/tmp/%@.", testName]]) {
        secnotice("secd_tests", "Removing test-specific keychain directory at %@", keychainDir);

        NSError* removeError = nil;
        [[NSFileManager defaultManager] removeItemAtURL:keychainDir error:&removeError];
        if(removeError) {
            secnotice("secd_tests", "Failed to remove directory: %@", removeError);
            return false;
        }
        return true;
     } else {
         secnotice("secd_tests", "Not removing keychain directory (%@), as it doesn't appear to be test-specific (for test %@)", keychainDir.path, testName);
         return false;
    }
}

#if 0
/*
* These functions are currently unused, but might be useful as we're finishing up UUID diversification.
* If it hasn't been used in a while, let's delete it.
*/

static void _time_start(uint64_t *time)
{
    if (time) {
        *time = mach_continuous_time();
    }
}

static void _time_stop(uint64_t start, uint64_t *result)
{
    uint64_t stop = mach_continuous_time();
    uint64_t us;
    static uint64_t time_overhead_measured = 0;
    static double timebase_factor = 0;

    if (time_overhead_measured == 0) {
        uint64_t t0 = mach_continuous_time();
        time_overhead_measured = mach_continuous_time() - t0;

        struct mach_timebase_info timebase_info = {};
        mach_timebase_info(&timebase_info);
        timebase_factor = ((double)timebase_info.numer)/((double)timebase_info.denom);
    }

    us = ((stop - start - time_overhead_measured) * timebase_factor) / NSEC_PER_USEC;
    if (result) {
        *result = us;
    }
}

static int ref_key_stress_test(void)
{

    const char *v_data = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    const void *keys[] = {
        kSecClass,
        kSecAttrAccount,
        kSecAttrService,
        kSecAttrLabel,
        kSecValueData
    };
    const void *values[] = {
        kSecClassGenericPassword,
        CFSTR("smith"),
        CFSTR("testservice"),
        CFSTR("test"),
        pwdata
    };
    CFStringRef label = NULL;
    CFStringRef service = NULL;
    const char *v_data2 = "different password data this time";
    CFDataRef pwdata2 = CFDataCreate(NULL, (UInt8 *)v_data2, strlen(v_data2));
    CFMutableDictionaryRef changes = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(changes, kSecValueData, pwdata2);

    CFDictionaryRef item = CFDictionaryCreate(NULL, keys, values, array_size(keys), NULL, NULL);
    CFMutableDictionaryRef labeledItems = CFDictionaryCreateMutableCopy(NULL, 0, item);
    for (int i = 0; i < 1000; i++) {
        label = CFStringCreateWithFormat(NULL, NULL, CFSTR("testItem%05d"), i);
        service = CFStringCreateWithFormat(NULL, NULL, CFSTR("testService%05d"), i);
        CFDictionarySetValue(labeledItems, kSecAttrLabel, label);
        CFDictionarySetValue(labeledItems, kSecAttrService, service);
        SecItemAdd(labeledItems, NULL);
    }
    CFTypeRef results = NULL;
    /* Create a dict with all attrs except the data. */
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, (array_size(keys)) - 1, NULL, NULL);
    CFMutableDictionaryRef queryLabel = CFDictionaryCreateMutableCopy(NULL, 0, query);
    uint64_t start, duration;

    _time_start(&start);
    for (int i = 0; i < 1000; i++) {
        label = CFStringCreateWithFormat(NULL, NULL, CFSTR("testItem%05d"), i);
        service = CFStringCreateWithFormat(NULL, NULL, CFSTR("testService%05d"), i);
        CFDictionarySetValue(queryLabel, kSecAttrLabel, label);
        CFDictionarySetValue(queryLabel, kSecAttrService, service);
        SecItemCopyMatching(queryLabel, &results);
        if (results) {
            CFRelease(results);
            results = NULL;
        }
        /* Modify the data of the item. */
        SecItemUpdate(queryLabel, changes);
    }
    _time_stop(start, &duration);
    if (duration > 10000000000000000) { // weed out all bad times
        duration = 0;
    }
    fprintf(stderr, "time: %llu us\n", duration);
    if (duration > 10000000) {
        fprintf(stderr, "\e[1;33mtest exceeded time duration of 10 seconds\e[0m\n");
    }

    CFReleaseNull(query);
    CFReleaseNull(queryLabel);
    CFReleaseNull(pwdata);
    CFReleaseNull(pwdata2);
    CFReleaseNull(changes);
    CFReleaseNull(item);
    CFReleaseNull(label);
    CFReleaseNull(labeledItems);

    return 0;
}
#endif // 0

static void add_items_test(void)
{
    char passcode[] = "password";
    int passcode_len = sizeof(passcode) - 1;
    keybag_handle_t keybag;
    keybag_state_t state;

    test_setup_temp_keychain("keydiversity_test", NULL);
    aks_create_bag(passcode, passcode_len, kAppleKeyStoreDeviceBag, &keybag);
    aks_get_lock_state(keybag, &state);

    const char *v_data = "test";
    CFDataRef pwdata = CFDataCreate(NULL, (UInt8 *)v_data, strlen(v_data));
    const void *keys[] = {
        kSecClass,
        kSecAttrAccount,
        kSecAttrService,
        kSecAttrLabel,
        kSecValueData,
        kSecUseDataProtectionKeychain
    };
    const void *values[] = {
        kSecClassGenericPassword,
        CFSTR("smith"),
        CFSTR("testservice"),
        CFSTR("test"),
        pwdata,
        kCFBooleanTrue,
    };

    CFDictionaryRef item = CFDictionaryCreate(NULL, keys, values, array_size(keys), NULL, NULL);
    OSStatus status = SecItemAdd(item, NULL);
    printf("SecItemAdd return = %d\n", status);
    status = SecItemDelete(item);
    printf("SecItemDelete return = %d\n", status);
    CFReleaseNull(item);

    test_teardown_delete_temp_keychain("keydiversity_test");
}

/*
 How to test this for persona enabled enterprise mode
 1. create a persona
 umtest createpersona personatype enterprise passcode {device_passcode}
 2. Run this test tool in that persona
 umtest launchinpersona personatype enterprise imagepath /usr/local/bin/security keychain-diversify-test
 */
int keychain_diversify_test(__unused int argc, __unused char * const * argv)
{
    add_items_test();
    
    return 0;
}
#endif /* USE_KEYSTORE */
