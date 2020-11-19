#import <Foundation/Foundation.h>
#import <Security/SecKeychainPriv.h>
#include <MobileKeyBag/MobileKeyBag.h>

static void print(NSString* str) {
    if (![str hasSuffix:@"\n"]) {
        str = [str stringByAppendingString:@"\n"];
    }
    [str writeToFile:@"/dev/stdout" atomically:NO encoding:NSUTF8StringEncoding error:nil];
}

static void usage() {
    print(@"Usage: stashtester [commands]");
    print(@"");
    print(@"Commands:");
    print(@"  -c          Combine stash and load requests (equivalent to -s and -l)");
    print(@"  -l          Send stash login request to securityd (SecKeychainLogin)");
    print(@"  -s          Send stash request to securityd (SecKeychainStash)");
    print(@"  -t          Test the complete operation");
}

static bool performStash() {
    NSLog(@"attempting stash");
    OSStatus result = SecKeychainStash();
    NSLog(@"result from stash: %ld", (long)result);
    return result == errSecSuccess;
}

static bool performLoad() {
    NSLog(@"attempting load");
    OSStatus result = SecKeychainLogin(0, NULL, 0, NULL);
    NSLog(@"result from load: %ld", (long)result);
    return result == errSecSuccess;
}

static NSMutableDictionary* makeQuery(bool includeData) {
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccount : @"stashtester",
        (id)kSecUseDataProtectionKeychain : @NO,
    } mutableCopy];
    if (includeData) {
        query[(id)kSecValueData] = [@"sekrit" dataUsingEncoding:NSUTF8StringEncoding];
    }
    return query;
}

static bool performTest() {
    NSLog(@"Begin test");
    NSLog(@"Adding item to keychain");
    NSMutableDictionary* addQ = makeQuery(true);
    OSStatus result = SecItemAdd((__bridge CFDictionaryRef)addQ, NULL);
    if (result != errSecSuccess) {
        NSLog(@"Failed to add item pre-stash: %d; aborting test", (int)result);
        return false;
    }

    if (!performStash()) {
        NSLog(@"Stash failed; aborting test");
        return false;
    }

    NSLog(@"Locking legacy keychain");
    SecKeychainRef loginkc = NULL;
    SecKeychainCopyLogin(&loginkc);
    result = SecKeychainCopyLogin(&loginkc);
    if (result != errSecSuccess) {
        NSLog(@"Unable to obtain reference to login keychain; aborting test");
        return false;
    }
    result = SecKeychainLock(loginkc);
    if (result != errSecSuccess) {
        NSLog(@"Unable to lock login keychain; aborting test");
        return false;
    }

    SecKeychainStatus status = 0;
    result = SecKeychainGetStatus(loginkc, &status);
    CFRelease(loginkc);
    if (result != errSecSuccess) {
        NSLog(@"Unable to get login keychain status; aborting test");
        return false;
    }

    if (status & kSecUnlockStateStatus) {
        NSLog(@"Login keychain not locked after locking; aborting test");
        return false;
    }

    NSLog(@"Locking keybag");
    int rc = MKBLockDevice((__bridge CFDictionaryRef)@{(id)kKeyBagLockDeviceNow : @YES});
    if (rc != kIOReturnSuccess) {
        NSLog(@"Failed to lock keybag (%d); aborting test", rc);
        return false;
    }

    // MKB asynchronously locks bag, make sure we don't race it
    NSLog(@"Twiddling thumbs for 11 seconds");
    sleep(11);

    NSLog(@"Verifying keybag is locked");
    NSMutableDictionary* checkQ = makeQuery(false);
    checkQ[(id)kSecUseDataProtectionKeychain] = @YES;
    result = SecItemAdd((__bridge CFDictionaryRef)checkQ, NULL);
    if (result != errSecInteractionNotAllowed) {
        NSLog(@"Data protection keychain unexpectedly not locked; aborting test");
        return false;
    }

    if (!performLoad()) {
        NSLog(@"Failed to load stash (%d); aborting test", result);
        return false;
    }

    NSMutableDictionary* findQ = makeQuery(false);
    findQ[(id)kSecReturnData] = @YES;
    CFTypeRef object = NULL;
    result = SecItemCopyMatching((__bridge CFDictionaryRef)findQ, &object);
    NSData* password;
    if (object) {
        password = CFBridgingRelease(object);
    }
    if (result != errSecSuccess || !password || ![[@"sekrit" dataUsingEncoding:NSUTF8StringEncoding] isEqual:password]) {
        NSLog(@"Unable to find item post-stashload (%d, %@); aborting test", result, password);
        return false;
    }

    NSLog(@"Test succeeded");
    return true;
}

static bool cleanup() {
    NSLog(@"Cleaning up");
    NSMutableDictionary* query = makeQuery(false);
    OSStatus result = SecItemDelete((__bridge CFDictionaryRef)query);
    if (result != errSecSuccess) {
        NSLog(@"Cleanup: failed to delete item");
        return false;
    }
    return true;
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        bool stash = false;
        bool load = false;
        bool test = false;
        int arg = 0;
        char * const *gargv = (char * const *)argv;
        while ((arg = getopt(argc, gargv, "clst")) != -1) {
            switch (arg) {
                case 'c':
                    stash = true;
                    load = true;
                    break;
                case 'l':
                    load = true;
                    break;
                case 's':
                    stash = true;
                    break;
                case 't':
                    test = true;
                    break;
                default:
                    usage();
                    return 1;
            }
        }

        if ((!stash && !load && !test) ||
            (test && (stash || load)))
        {
            usage();
            return 1;
        }

        if (test) {
            bool testresult = performTest();
            bool cleanresult = cleanup();
            return (testresult && cleanresult) ? 0 : -1;
        }

        if (stash && !performStash()) {
            return -1;
        }

        if (load && !performLoad()) {
            return -1;
        }
    }
    return 0;
}
