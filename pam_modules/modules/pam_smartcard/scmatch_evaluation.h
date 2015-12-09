#ifndef scmatch_evaluation_h
#define scmatch_evaluation_h
#include <Security/Security.h>
#include <CoreFoundation/CFArray.h>
#include <OpenDirectory/OpenDirectory.h>

SecKeychainRef copyAttributeMatchedKeychain(ODRecordRef odRecord, CFArrayRef certificates);
SecKeychainRef copyHashMatchedKeychain(ODRecordRef odRecord, CFArrayRef certificates);
SecKeychainRef copySmartCardKeychainForUser(ODRecordRef odRecord, const char* username);

#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); }
#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); \
    if (_cf) { (CF) = NULL; CFRelease(_cf); } }

#endif /* scmatch_evaluation_h */
