//
//  SOSAccountGetSet.c
//  Security
//
//

#include "SOSAccountPriv.h"

#include <utilities/SecCFWrappers.h>

//
// MARK: Generic Value manipulation
//

static inline bool SOSAccountEnsureExpansion(SOSAccountRef account, CFErrorRef *error) {
    if (!account->expansion) {
        account->expansion = CFDictionaryCreateMutableForCFTypes(NULL);
    }

    return SecAllocationError(account->expansion, error, CFSTR("Can't Alloc Account Expansion dictionary"));
}

bool SOSAccountClearValue(SOSAccountRef account, CFStringRef key, CFErrorRef *error) {
    bool success = SOSAccountEnsureExpansion(account, error);
    require_quiet(success, errOut);

    CFDictionaryRemoveValue(account->expansion, key);
errOut:
    return success;
}

bool SOSAccountSetValue(SOSAccountRef account, CFStringRef key, CFTypeRef value, CFErrorRef *error) {
    if (value == NULL) return SOSAccountClearValue(account, key, error);

    bool success = SOSAccountEnsureExpansion(account, error);
    require_quiet(success, errOut);

    CFDictionarySetValue(account->expansion, key, value);
errOut:
    return success;
}

CFTypeRef SOSAccountGetValue(SOSAccountRef account, CFStringRef key, CFErrorRef *error) {
    if (!account->expansion) {
        return NULL;
    }
    return CFDictionaryGetValue(account->expansion, key);
}


//
// MARK: Value as Set manipulation
//
bool SOSAccountValueSetContainsValue(SOSAccountRef account, CFStringRef key, CFTypeRef value);
void SOSAccountValueUnionWith(SOSAccountRef account, CFStringRef key, CFSetRef valuesToUnion);
void SOSAccountValueSubtractFrom(SOSAccountRef account, CFStringRef key, CFSetRef valuesToSubtract);

bool SOSAccountValueSetContainsValue(SOSAccountRef account, CFStringRef key, CFTypeRef value) {
    CFSetRef foundSet = asSet(SOSAccountGetValue(account, key, NULL), NULL);
    return foundSet && CFSetContainsValue(foundSet, value);
}

void SOSAccountValueUnionWith(SOSAccountRef account, CFStringRef key, CFSetRef valuesToUnion) {
    CFMutableSetRef unionedSet = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, valuesToUnion);
    CFSetRef foundSet = asSet(SOSAccountGetValue(account, key, NULL), NULL);
    if (foundSet) {
        CFSetUnion(unionedSet, foundSet);
    }
    SOSAccountSetValue(account, key, unionedSet, NULL);
    CFReleaseNull(unionedSet);
}

void SOSAccountValueSubtractFrom(SOSAccountRef account, CFStringRef key, CFSetRef valuesToSubtract) {
    CFSetRef foundSet = asSet(SOSAccountGetValue(account, key, NULL), NULL);
    if (foundSet) {
        CFMutableSetRef subtractedSet = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, foundSet);
        CFSetSubtract(subtractedSet, valuesToSubtract);
        SOSAccountSetValue(account, key, subtractedSet, NULL);
        CFReleaseNull(subtractedSet);
    }
}

//
// MARK: UUID
CFStringRef SOSAccountCopyUUID(SOSAccountRef account) {
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

void SOSAccountEnsureUUID(SOSAccountRef account) {
    CFStringRef uuid = SOSAccountCopyUUID(account);
    CFReleaseNull(uuid);
}

