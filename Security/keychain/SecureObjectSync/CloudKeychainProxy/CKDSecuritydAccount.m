//
//  CKDSecuritydAccount+CKDSecuritydAccount_m.m
//  Security
//
//

#import "Foundation/Foundation.h"
#import "CKDSecuritydAccount.h"

#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecItemPriv.h>

@implementation CKDSecuritydAccount

+ (instancetype) securitydAccount
{
    return [[CKDSecuritydAccount alloc] init];
}

- (NSSet*) keysChanged: (NSDictionary<NSString*, NSObject*>*)keyValues error: (NSError**) error
{
    CFErrorRef cf_error = NULL;
    NSArray* handled = (__bridge_transfer NSArray*) _SecKeychainSyncUpdateMessage((__bridge CFDictionaryRef)keyValues, &cf_error);
    NSError *updateError = (__bridge_transfer NSError*)cf_error;
    if (error)
        *error = updateError;

    return [NSSet setWithArray:handled];
}

- (bool) ensurePeerRegistration: (NSError**) error
{
    CFErrorRef localError = NULL;
    bool result = SOSCCProcessEnsurePeerRegistration(error ? &localError : NULL);

    if (error && localError) {
        *error = (__bridge_transfer NSError*) localError;
    }

    return result;
}

- (NSSet<NSString*>*) syncWithPeers: (NSSet<NSString*>*) peerIDs backups: (NSSet<NSString*>*) backupPeerIDs error: (NSError**) error
{
    CFErrorRef localError = NULL;
    CFSetRef handledPeers = SOSCCProcessSyncWithPeers((__bridge CFSetRef) peerIDs, (__bridge CFSetRef) backupPeerIDs, &localError);

    if (error && localError) {
        *error = (__bridge_transfer NSError*) localError;
    }

    return (__bridge_transfer NSSet<NSString*>*) handledPeers;
}

- (SyncWithAllPeersReason) syncWithAllPeers: (NSError**) error
{
    CFErrorRef localError = NULL;
    SyncWithAllPeersReason result = SOSCCProcessSyncWithAllPeers(error ? &localError : NULL);

    if (error && localError) {
        *error = (__bridge_transfer NSError*) localError;
    }

    return result;
}

@end
