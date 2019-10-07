#import "TestsObjcTranslation.h"
#import <Security/OTClique.h>
#import <OCMock/OCMock.h>

#import <SoftLinking/SoftLinking.h>
#import "keychain/ot/OTCuttlefishContext.h"
#import <Security/SecItemPriv.h>
#import "keychain/categories/NSError+UsefulConstructors.h"

static const uint8_t signingKey_384[] = {
    0x04, 0xe4, 0x1b, 0x3e, 0x88, 0x81, 0x9f, 0x3b, 0x80, 0xd0, 0x28, 0x1c,
    0xd9, 0x07, 0xa0, 0x8c, 0xa1, 0x89, 0xa8, 0x3b, 0x69, 0x91, 0x17, 0xa7,
    0x1f, 0x00, 0x31, 0x91, 0x82, 0x89, 0x1f, 0x5c, 0x44, 0x2d, 0xd6, 0xa8,
    0x22, 0x1f, 0x22, 0x7d, 0x27, 0x21, 0xf2, 0xc9, 0x75, 0xf2, 0xda, 0x41,
    0x61, 0x55, 0x29, 0x11, 0xf7, 0x71, 0xcf, 0x66, 0x52, 0x2a, 0x27, 0xfe,
    0x77, 0x1e, 0xd4, 0x3d, 0xfb, 0xbc, 0x59, 0xe4, 0xed, 0xa4, 0x79, 0x2a,
    0x9b, 0x73, 0x3e, 0xf4, 0xf4, 0xe3, 0xaf, 0xf2, 0x8d, 0x34, 0x90, 0x92,
    0x47, 0x53, 0xd0, 0x34, 0x1e, 0x49, 0x87, 0xeb, 0x11, 0x89, 0x0f, 0x9c,
    0xa4, 0x99, 0xe8, 0x4f, 0x39, 0xbe, 0x21, 0x94, 0x88, 0xba, 0x4c, 0xa5,
    0x6a, 0x60, 0x1c, 0x2f, 0x77, 0x80, 0xd2, 0x73, 0x14, 0x33, 0x46, 0x5c,
    0xda, 0xee, 0x13, 0x8a, 0x3a, 0xdb, 0x4e, 0x05, 0x4d, 0x0f, 0x6d, 0x96,
    0xcd, 0x28, 0xab, 0x52, 0x4c, 0x12, 0x2b, 0x79, 0x80, 0xfe, 0x9a, 0xe4,
    0xf4
};

@implementation TestsObjectiveC : NSObject
+ (void)setNewRecoveryKeyWithData:(OTConfigurationContext *)ctx
                      recoveryKey:(NSString*)recoveryKey
                            reply:(void(^)(void* rk,
                                           NSError* _Nullable error))reply
{
    [OTClique setNewRecoveryKeyWithData:ctx recoveryKey:recoveryKey reply:^(SecRecoveryKey * _Nullable rk, NSError * _Nullable error) {
        reply((__bridge void*)rk, error);
    }];
}

+ (void)recoverOctagonUsingData:(OTConfigurationContext *)ctx
                    recoveryKey:(NSString*)recoveryKey
                          reply:(void(^)(NSError* _Nullable error))reply
{
    [OTClique recoverOctagonUsingData: ctx recoveryKey:recoveryKey reply:reply];
}

+ (BOOL)saveCoruptDataToKeychainForContainer:(NSString*)containerName
                                   contextID:(NSString*)contextID
                                       error:(NSError**)error
{
    NSData* signingFromBytes = [[NSData alloc] initWithBytes:signingKey_384 length:sizeof(signingKey_384)];

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
                                    (id)kSecValueData : signingFromBytes,
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

@end
