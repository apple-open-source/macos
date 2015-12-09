/******************************************************************
 * The purpose of this module is to implement
 * hash matching for smartcard and user account
 ******************************************************************/

#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <Security/SecCertificatePriv.h>
#include "Common.h"
#include "scmatch_evaluation.h"

#if !defined(kODAuthAuthorityPubkey)
#define kODAuthAuthorityPubkey                      CFSTR("pubkeyhash")
#endif

CFDataRef createDataFromHexString(CFStringRef str)
{
    CFIndex length = CFStringGetLength(str);
    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, (length + 1)/ 2); // for odd length
    if (!data)
        return NULL;
    char byteChars[3] = {0, 0, 0};
    uint8_t wholeByte;
    for (CFIndex i = 0; i < length; i += 2) {
        byteChars[0] = (char)CFStringGetCharacterAtIndex(str, i);
        byteChars[1] = (char)CFStringGetCharacterAtIndex(str, i + 1);
        wholeByte = (uint8_t)strtoul(byteChars, NULL, 16);
        CFDataAppendBytes(data, (UInt8*)&wholeByte, 1);
    }
    return data;
}

SecKeychainRef copyHashMatchedKeychain(ODRecordRef odRecord, CFArrayRef certificates)
{
    CFArrayRef authStrings;
    CFDataRef hash = NULL;
    
    int odRes = od_record_attribute_create_cfarray(odRecord, kODAttributeTypeAuthenticationAuthority,  &authStrings);
    if (odRes != PAM_SUCCESS)
        return NULL;
    
    
    for (CFIndex i = 0; i < CFArrayGetCount(authStrings); ++i)
    {
        CFArrayRef parts = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, CFArrayGetValueAtIndex(authStrings, i), CFSTR(";"));
        if (parts)
        {
            if (CFArrayGetCount(parts) == 3 && CFStringCompare(CFArrayGetValueAtIndex(parts, 1), kODAuthAuthorityPubkey, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
            {
                hash = createDataFromHexString(CFArrayGetValueAtIndex(parts, 2));
            }
            CFRelease(parts);
        }
        if (hash)
            break;
    }
    CFRelease(authStrings);
    
    if (!hash)
        return NULL;
    
    SecKeychainRef retval = NULL;
    
    CFIndex count = CFArrayGetCount(certificates);
    uint32_t index;
    bool matches;
    
    for (index = 0; index < count; ++index)
    {
        SecCertificateRef candidate = (SecCertificateRef)CFArrayGetValueAtIndex(certificates, index);
        CFDataRef certificateHash = SecCertificateCopyPublicKeySHA1Digest(candidate);
        if (certificateHash)
        {
            matches = certificateHash && CFEqual(certificateHash, hash);
            CFRelease(certificateHash);
            if (matches)
            {
                SecKeychainItemCopyKeychain((SecKeychainItemRef)candidate, &retval); // we do not care about the result a NULL retval means failure
                break;
            }
        }
    }
    
    CFRelease(hash);
    return retval;
}
