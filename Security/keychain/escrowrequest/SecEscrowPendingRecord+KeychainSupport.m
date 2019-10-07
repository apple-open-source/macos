
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import "OSX/sec/Security/SecItemShim.h"

#import "OSX/utilities/SecCFRelease.h"
#import "utilities/debugging.h"

#import "SecEscrowPendingRecord+KeychainSupport.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

@implementation SecEscrowPendingRecord (KeychainSupport)

- (BOOL)saveToKeychain:(NSError**)error
{
    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassInternetPassword,
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
                                    (id)kSecAttrAccessGroup: @"com.apple.sbd",
                                    (id)kSecAttrServer: @"escrow-prerecord",
                                    (id)kSecAttrDescription: [NSString stringWithFormat:@"Escrow Prerecord: %@", self.uuid],
                                    (id)kSecAttrAccount: self.uuid,
                                    (id)kSecValueData : self.data,
                                    (id)kSecAttrIsInvisible: @YES,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecAttrSynchronizable : @NO,
                                    } mutableCopy];

    CFTypeRef result = NULL;
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, &result);

    NSError* localerror = nil;

    // Did SecItemAdd fall over due to an existing item?
    if(status == errSecDuplicateItem) {
        // Add every primary key attribute to this find dictionary
        NSMutableDictionary* findQuery = [[NSMutableDictionary alloc] init];
        findQuery[(id)kSecClass]              = query[(id)kSecClass];
        findQuery[(id)kSecAttrSynchronizable] = query[(id)kSecAttrSynchronizable];
        findQuery[(id)kSecAttrSyncViewHint]   = query[(id)kSecAttrSyncViewHint];
        findQuery[(id)kSecAttrAccessGroup]    = query[(id)kSecAttrAccessGroup];
        findQuery[(id)kSecAttrAccount]        = query[(id)kSecAttrAccount];
        findQuery[(id)kSecAttrServer]         = query[(id)kSecAttrServer];
        findQuery[(id)kSecAttrPath]           = query[(id)kSecAttrPath];
        findQuery[(id)kSecUseDataProtectionKeychain] = query[(id)kSecUseDataProtectionKeychain];

        NSMutableDictionary* updateQuery = [query mutableCopy];
        updateQuery[(id)kSecClass] = nil;

        status = SecItemUpdate((__bridge CFDictionaryRef)findQuery, (__bridge CFDictionaryRef)updateQuery);

        if(status) {
            localerror = [NSError errorWithDomain:NSOSStatusErrorDomain
                                             code:status
                                      description:[NSString stringWithFormat:@"SecItemUpdate: %d", (int)status]];
        }
    } else if(status != 0) {
        localerror = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:status
                                  description:[NSString stringWithFormat:@"SecItemAdd: %d", (int)status]];
    }

    if(localerror) {
        if(error) {
            *error = localerror;
        }
        return false;
    } else {
        return true;
    }
}

- (BOOL)deleteFromKeychain:(NSError**)error
{
    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassInternetPassword,
                                    (id)kSecAttrAccessGroup: @"com.apple.sbd",
                                    (id)kSecAttrServer: @"escrow-prerecord",
                                    (id)kSecAttrAccount: self.uuid,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecAttrSynchronizable : @NO,
                                    } mutableCopy];

    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);

    if(status != errSecSuccess) {
        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:status
                                  description:[NSString stringWithFormat:@"SecItemAdd: %d", (int)status]];;
        }
        return false;
    }

    return true;
}

+ (SecEscrowPendingRecord* _Nullable)loadFromKeychain:(NSString*)uuid error:(NSError**)error
{
    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassInternetPassword,
                                    (id)kSecAttrAccessGroup: @"com.apple.sbd",
                                    (id)kSecAttrServer: @"escrow-prerecord",
                                    (id)kSecAttrAccount: uuid,
                                    (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                                    (id)kSecAttrSynchronizable : @NO,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecReturnAttributes: @YES,
                                    (id)kSecReturnData: @YES,
                                    } mutableCopy];

    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

    if(status) {
        CFReleaseNull(result);

        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:status
                                     userInfo:@{NSLocalizedDescriptionKey:
                                                    [NSString stringWithFormat:@"SecItemCopyMatching: %d", (int)status]}];
        }
        return nil;
    }

    NSDictionary* resultDict = (NSDictionary*) CFBridgingRelease(result);
    SecEscrowPendingRecord* record = [[SecEscrowPendingRecord alloc] initWithData:resultDict[(id)kSecValueData]];

    //TODO: if no record, add an error here

    return record;
}

+ (NSArray<SecEscrowPendingRecord*>* _Nullable)loadAllFromKeychain:(NSError**)error
{
    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassInternetPassword,
                                    (id)kSecAttrAccessGroup: @"com.apple.sbd",
                                    (id)kSecAttrServer: @"escrow-prerecord",
                                    (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                    (id)kSecAttrSynchronizable : @NO,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecReturnAttributes: @YES,
                                    (id)kSecReturnData: @YES,
                                    } mutableCopy];

    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

    if(status) {
        CFReleaseNull(result);

        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:status
                                     userInfo:@{NSLocalizedDescriptionKey:
                                                    [NSString stringWithFormat:@"SecItemCopyMatching: %d", (int)status]}];
        }
        return nil;
    }

    NSMutableArray<SecEscrowPendingRecord*>* records = [NSMutableArray array];
    NSDictionary* resultArray = CFBridgingRelease(result);

    for(NSDictionary* item in resultArray) {
        SecEscrowPendingRecord* record = [[SecEscrowPendingRecord alloc] initWithData:item[(id)kSecValueData]];
        if(record) {
            [records addObject: record];
        } else {
            secerror("escrowrequest: Unable to deserialize keychain item");
        }
    }

    return records;
}

@end

@implementation SecEscrowPendingRecord (EscrowAttemptTimeout)
- (BOOL)escrowAttemptedWithinLastSeconds:(NSTimeInterval)timeInterval
{
    NSDate* limitDate = [NSDate dateWithTimeIntervalSinceNow:-timeInterval];
    uint64_t limitMillis = [limitDate timeIntervalSince1970] * 1000;

    if(self.hasLastEscrowAttemptTime && self.lastEscrowAttemptTime >= limitMillis) {
        return YES;
    }
    return NO;
}
@end
