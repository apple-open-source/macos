#import <xpc/private.h>

#import "utilities/debugging.h"
#import <Security/SecItemPriv.h>
#import "LocalKeychainAnalytics.h"

#import "KeychainStasher.h"

NSString* const kApplicationIdentifier = @"com.apple.security.KeychainStasher";

@implementation KeychainStasher

- (NSError*)errorWithStatus:(OSStatus)status message:(NSString*)format, ... NS_FORMAT_FUNCTION(2, 3)
{
    if (status == errSecSuccess) {
        return nil;
    }

    NSString* desc;
    if (format) {
        va_list ap;
        va_start(ap, format);
        desc = [[NSString alloc] initWithFormat:format arguments:ap];
        va_end(ap);
    }
    return [NSError errorWithDomain:NSOSStatusErrorDomain
                               code:status
                           userInfo:desc ? @{NSLocalizedDescriptionKey : desc} : nil];
}

- (NSMutableDictionary*)baseQuery {
    return [@{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
        // Prevents backups in case this ever comes to the Mac, prevents sync
        (id)kSecAttrSysBound : @(kSecSecAttrSysBoundPreserveDuringRestore),
        // Belt-and-suspenders prohibition on syncing
        (id)kSecAttrSynchronizable : @NO,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrApplicationLabel : @"loginstash",
        // This is the default, but just making sure
        (id)kSecAttrAccessGroup : kApplicationIdentifier,
    } mutableCopy];
}

- (void)stashKey:(NSData*)key withReply:(void (^)(NSError*))reply {
    secnotice("stashkey", "Will attempt to stash key");
    if (!key || key.length == 0) {
        reply([self errorWithStatus:errSecParam message:@"nil or empty key passed"]);
        return;
    }

    NSMutableDictionary* query = [self baseQuery];
    query[(id)kSecValueData] = key;

    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, NULL);
    secnotice("stashkey", "SecItemAdd result: %ld", (long)status);

    if (status == errSecDuplicateItem) {
        [[LocalKeychainAnalytics logger] logResultForEvent:LKAEventStash hardFailure:NO result:[self errorWithStatus:status message:nil]];
        query[(id)kSecValueData] = nil;
        NSDictionary* update = @{(id)kSecValueData : key};
        status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update);
        secnotice("stashkey", "SecItemUpdate result: %ld", (long)status);
    }

    [[LocalKeychainAnalytics logger] logResultForEvent:LKAEventStash hardFailure:YES result:[self errorWithStatus:status message:nil]];
    if (status == errSecSuccess) {
        reply(nil);
    } else {
        reply([self errorWithStatus:status message:@"Stash failed with keychain error"]);
    }
}

- (void)loadKeyWithReply:(void (^)(NSData*, NSError*))reply {
    secnotice("KeychainStasher", "Will attempt to retrieve stashed key");

    NSMutableDictionary* query = [self baseQuery];
    query[(id)kSecReturnData] = @YES;

    CFTypeRef object = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &object);

    if (status != errSecSuccess) {
        if (status == errSecItemNotFound) {
            reply(nil, [self errorWithStatus:status message:@"No stashed key found"]);
        } else {
            reply(nil, [self errorWithStatus:status message:@"Keychain error"]);
        }
        [[LocalKeychainAnalytics logger] logResultForEvent:LKAEventStashLoad hardFailure:YES result:[self errorWithStatus:status message:nil]];
        return;
    }

    NSData* key;
    if (!object || CFGetTypeID(object) != CFDataGetTypeID() || !(key = CFBridgingRelease(object))) {
        reply(nil, [self errorWithStatus:errSecInternalError
                                 message:@"No or bad object: %d / %lu / %d",
                                         object != NULL, object ? CFGetTypeID(object) : 0, key != nil]);
        [[LocalKeychainAnalytics logger] logResultForEvent:LKAEventStashLoad
                                               hardFailure:YES
                                                    result:[self errorWithStatus:errSecInternalError message:nil]];
        return;
    }

    // Caller does not need to wait for our delete
    reply(key, nil);

    query[(id)kSecReturnData] = nil;
    status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status != errSecSuccess) {
        seccritical("Unable to delete masterkey after load: %d", (int)status);
    }
    [[LocalKeychainAnalytics logger] logResultForEvent:LKAEventStashLoad hardFailure:NO result:[self errorWithStatus:status message:nil]];
}

@end
