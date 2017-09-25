//
//  SOSAccountGetSet.c
//  Security
//
//

#include "SOSAccountPriv.h"

#include <utilities/SecCFWrappers.h>

#import <Security/SecureObjectSync/SOSAccountTrust.h>
#import <Security/SecureObjectSync/SOSAccountTrustClassic.h>

//
// MARK: Generic Value manipulation
//

static inline bool SOSAccountEnsureExpansion(SOSAccount* account, CFErrorRef *error) {
    
    if (!account.trust.expansion) {
        account.trust.expansion = [NSMutableDictionary dictionary];
    }

    return SecAllocationError(((__bridge CFDictionaryRef)account.trust.expansion), error, CFSTR("Can't Alloc Account Expansion dictionary"));
}

bool SOSAccountClearValue(SOSAccount* account, CFStringRef key, CFErrorRef *error) {
    bool success = SOSAccountEnsureExpansion(account, error);
    if(!success){
        return success;
    }

    [account.trust.expansion removeObjectForKey: (__bridge NSString* _Nonnull)(key)];

    return success;
}

bool SOSAccountSetValue(SOSAccount* account, CFStringRef key, CFTypeRef value, CFErrorRef *error) {
    if (value == NULL) return SOSAccountClearValue(account, key, error);

    bool success = SOSAccountEnsureExpansion(account, error);
    if(!success)
        return success;

    [account.trust.expansion setObject:(__bridge id _Nonnull)(value) forKey:(__bridge NSString* _Nonnull)(key)];

    return success;
}

//
// MARK: UUID
CFStringRef SOSAccountCopyUUID(SOSAccount* account) {
    CFStringRef uuid = CFRetainSafe(asString(SOSAccountGetValue(account, kSOSAccountUUID, NULL), NULL));
    if (uuid == NULL) {
        CFUUIDRef newID = CFUUIDCreate(kCFAllocatorDefault);
        uuid = CFUUIDCreateString(kCFAllocatorDefault, newID);

        CFErrorRef setError = NULL;
        if (!SOSAccountSetValue(account, kSOSAccountUUID, uuid, &setError)) {
            secerror("Failed to set UUID: %@ (%@)", uuid, setError);
        }
        CFReleaseNull(setError);
        CFReleaseNull(newID);
    }
    return uuid;
}

void SOSAccountEnsureUUID(SOSAccount* account) {
    CFStringRef uuid = SOSAccountCopyUUID(account);
    CFReleaseNull(uuid);
}

