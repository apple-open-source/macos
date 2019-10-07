
#if OCTAGON

#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import "OSX/sec/Security/SecItemShim.h"

#import "OSX/utilities/SecCFRelease.h"

#import "OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTConstants.h"

@implementation OTAccountMetadataClassC (KeychainSupport)


- (BOOL)saveToKeychainForContainer:(NSString*)containerName
                         contextID:(NSString*)contextID
                             error:(NSError**)error
{
    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassInternetPassword,
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecAttrAccessGroup: @"com.apple.security.octagon",
                                    (id)kSecAttrDescription: [NSString stringWithFormat:@"Octagon Account State (%@,%@)", containerName, contextID],
                                    (id)kSecAttrServer: [NSString stringWithFormat:@"octagon-%@", containerName],
                                    (id)kSecAttrAccount: [NSString stringWithFormat:@"octagon-%@", containerName], // Really should be alt-DSID, no?
                                    (id)kSecAttrPath: [NSString stringWithFormat:@"octagon-%@", contextID],
                                    (id)kSecAttrIsInvisible: @YES,
                                    (id)kSecValueData : self.data,
                                    (id)kSecAttrSynchronizable : @NO,
                                    (id)kSecAttrSysBound : @(kSecSecAttrSysBoundPreserveDuringRestore),
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
                                  description: [NSString stringWithFormat:@"SecItemAdd: %d", (int)status]];
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

+ (BOOL) deleteFromKeychainForContainer:(NSString*)containerName
                              contextID:(NSString*)contextID error:(NSError**)error
{
    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassInternetPassword,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecAttrAccessGroup: @"com.apple.security.octagon",
                                    (id)kSecAttrServer: [NSString stringWithFormat:@"octagon-%@", containerName],
                                    (id)kSecAttrAccount: [NSString stringWithFormat:@"octagon-%@", containerName],
                                    (id)kSecAttrPath: [NSString stringWithFormat:@"octagon-%@", contextID],
                                    (id)kSecAttrSynchronizable : @NO,
                                    } mutableCopy];

    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
    if(status) {
        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:status
                                     userInfo:@{NSLocalizedDescriptionKey:
                                                    [NSString stringWithFormat:@"SecItemDelete: %d", (int)status]}];
        }
        return NO;
    }
    return YES;
}

+ (OTAccountMetadataClassC* _Nullable)loadFromKeychainForContainer:(NSString*)containerName
                                                         contextID:(NSString*)contextID error:(NSError**)error
{
    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassInternetPassword,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecAttrAccessGroup: @"com.apple.security.octagon",
                                    (id)kSecAttrServer: [NSString stringWithFormat:@"octagon-%@", containerName],
                                    (id)kSecAttrAccount: [NSString stringWithFormat:@"octagon-%@", containerName],
                                    (id)kSecAttrPath: [NSString stringWithFormat:@"octagon-%@", contextID],
                                    (id)kSecAttrSynchronizable : @NO,
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

    NSDictionary* resultDict = CFBridgingRelease(result);

    OTAccountMetadataClassC* state = [[OTAccountMetadataClassC alloc] initWithData:resultDict[(id)kSecValueData]];
    if(!state) {
        if(error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorDeserializationFailure description:@"couldn't deserialize account state"];
        }
        NSError* deleteError = nil;
        BOOL deleted = [OTAccountMetadataClassC deleteFromKeychainForContainer:containerName contextID:contextID error:&deleteError];
        if(deleted == NO || deleteError) {
            secnotice("octagon", "failed to reset account metadata in keychain, %@", deleteError);
        }
        return nil;
    }

    //check if an account state has the appropriate attributes
    if(resultDict[(id)kSecAttrSysBound] == nil || ![resultDict[(id)kSecAttrAccessible] isEqualToString:(id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly]){
        [state saveToKeychainForContainer:containerName contextID:contextID error:error];
    }

    return state;
}

@end

#endif // OCTAGON
