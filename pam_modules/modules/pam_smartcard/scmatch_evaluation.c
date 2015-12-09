#include <OpenDirectory/OpenDirectory.h>
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>

#include "scmatch_evaluation.h"


CFArrayRef copyCardCertificates()
{
    CFArrayRef smartCardKeychains  = NULL;
    CFTypeRef certificates = NULL;
    
    OSStatus status = SecKeychainCopyDomainSearchList(kSecPreferencesDomainDynamic, &smartCardKeychains);
    if (status != errSecSuccess)
        goto cleanup;
    
    const void *keys[] = { kSecClass, kSecReturnRef, kSecMatchLimit, kSecMatchSearchList };
    const void *vals[] = { kSecClassCertificate, kCFBooleanTrue, kSecMatchLimitAll, smartCardKeychains };
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, sizeof(keys) / sizeof(keys[0]), NULL, NULL);
    if (!query)
        goto cleanup;
    
    status = SecItemCopyMatching(query, &certificates);
    CFRelease(query);
    
    if (status != errSecSuccess)
        goto cleanup;
    
cleanup:
    if (smartCardKeychains)
        CFRelease(smartCardKeychains);
    
    if(certificates == NULL)
        return NULL;
    
    if (CFGetTypeID(certificates) == CFArrayGetTypeID())
        return certificates;
    
    CFRelease(certificates);
    return NULL;
}

SecKeychainRef copySmartCardKeychainForUser(ODRecordRef odRecord, const char* username)
{
    if (odRecord == NULL || username == NULL)
        return NULL;
    
    CFArrayRef certificates = copyCardCertificates();
    if (certificates)
    {
        SecKeychainRef result = copyHashMatchedKeychain(odRecord, certificates);
        if (result == NULL)
            result = copyAttributeMatchedKeychain(odRecord, certificates);
        CFRelease(certificates);
        return result;
    }
    
    return NULL;
}