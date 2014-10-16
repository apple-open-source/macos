/*
 *  sc-51-persistentEC.c
 *  Security
 *
 *  Copyright (c) 2008-2010,2012-2014 Apple Inc. All Rights Reserved.
 *
 */

/*
    Test code for SOSCircle keys
*/

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>
#if TARGET_OS_EMBEDDED
#include <Security/SecInternal.h>
#endif
#include <Security/SecItemPriv.h>
#include <stdlib.h>
#include <unistd.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/iOSforOSX.h>

#include "SOSCircle_regressions.h"

static SecKeyRef GeneratePermanentFullECKey(int keySize, CFStringRef name)
{    
    SecKeyRef public_key = NULL;
    SecKeyRef full_key = NULL;
    
    CFNumberRef key_size_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keySize);
    
    CFDictionaryRef keygen_parameters = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
        kSecAttrKeyType,        kSecAttrKeyTypeEC,
        kSecAttrKeySizeInBits,  key_size_num,
        kSecAttrIsPermanent,    kCFBooleanTrue,
        kSecAttrAccessible,     kSecAttrAccessibleAlwaysThisDeviceOnly,
        kSecAttrLabel,          name,
        NULL);
    
    CFReleaseNull(key_size_num);
    ok_status(SecKeyGeneratePair(keygen_parameters, &public_key, &full_key), "generate EC Key Pair");
    CFReleaseNull(keygen_parameters);
    CFReleaseNull(public_key);
    
    return full_key;
}

static void tests(void)
{
    CFStringRef ourAccountName = CFSTR("LjzZ2JteIrnHoHWf5hYb1WGqjI");
    CFStringRef circleName = kSecAttrAccessibleWhenUnlocked;
    CFStringRef keyName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("ID for %@-%@"), ourAccountName, circleName);
    const int keySize = 256;
    
    // Create it
    SecKeyRef full_key = GeneratePermanentFullECKey(keySize, keyName);
    ok(full_key, "EC Key generated");
    
    // Now search for it
    CFNumberRef key_size_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keySize);

    CFDictionaryRef keysearch_parameters = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
        kSecClass,              kSecClassKey,
        kSecReturnRef,          kCFBooleanTrue,
        kSecAttrKeySizeInBits,  key_size_num,
        kSecAttrLabel,          keyName,
        NULL);

    CFReleaseNull(key_size_num);
    CFReleaseNull(keyName);

    CFTypeRef results = NULL;
    ok_status(SecItemCopyMatching(keysearch_parameters, &results), "find EC key by attr");
    ok(results && (CFGetTypeID(results) == SecKeyGetTypeID()), "Got a SecKeyRef");
    CFReleaseNull(results);

    ok_status(SecItemDelete(keysearch_parameters), "delete EC Key Pair");

    CFRelease(keysearch_parameters);
}

int sc_51_persistentEC(int argc, char *const *argv)
{
    plan_tests(5);
    tests();

    return 0;
}
