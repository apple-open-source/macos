/*
 *  mp-00-identity.c
 *  regressions
 *
 *  Created by Mitch Adler on 2/3/11.
 *  Copyright 2011 Apple Inc. All rights reserved.
 *
 */

#include <stdio.h>

#include "Security_regressions.h"

#include <CoreFoundation/CFData.h>
#include <Security/SecOTRSession.h>
#include <Security/SecInternal.h>
#include <Security/SecBasePriv.h>

static void RegressionsLogError(CFErrorRef error) {
    if (error == NULL) {
        return;
    }
    CFDictionaryRef tempDictionary = CFErrorCopyUserInfo(error);
    CFIndex errorCode = CFErrorGetCode(error);
    CFStringRef errorDomain = CFErrorGetDomain(error);
    CFStringRef errorString = CFDictionaryGetValue(tempDictionary, kCFErrorDescriptionKey);
    CFErrorRef previousError = (CFErrorRef)CFDictionaryGetValue(tempDictionary, kCFErrorUnderlyingErrorKey);
    if (previousError != NULL) {
        RegressionsLogError(previousError);
    }
    char errorDomainStr[1024];
    char errorStringStr[1024];
    
    CFStringGetCString(errorDomain, errorDomainStr, 1024, kCFStringEncodingUTF8);
    CFStringGetCString(errorString, errorStringStr, 1024, kCFStringEncodingUTF8);
    printf("OTR: %s (%ld) -- %s\n", errorDomainStr, errorCode, errorStringStr);
    CFReleaseSafe(tempDictionary);
}

static int kTestTestCount = 18;
static void tests(void)
{
    CFErrorRef testError = NULL;
    
    SecOTRFullIdentityRef idToPurge = SecOTRFullIdentityCreate(kCFAllocatorDefault, &testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);
    
    ok(idToPurge != NULL, "Make Identity");
    
    CFMutableDataRef purgeExport = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok(SecOTRFIAppendSerialization(idToPurge, purgeExport, &testError), "First export");
    RegressionsLogError(testError);
    CFReleaseNull(testError);
    
    SecOTRFullIdentityRef purgeIdInflate = SecOTRFullIdentityCreateFromData(kCFAllocatorDefault, purgeExport, &testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);
    
    ok(purgeIdInflate != NULL, "Inflate Identity");

    SecOTRFIPurgeFromKeychain(idToPurge, &testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);

    SecOTRFullIdentityRef failIDInflate = SecOTRFullIdentityCreateFromData(kCFAllocatorDefault, purgeExport, &testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);
  
    ok(failIDInflate == NULL, "Should fail");

    CFReleaseSafe(idToPurge);


    idToPurge = SecOTRFullIdentityCreate(kCFAllocatorDefault, &testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);
    
    ok(idToPurge != NULL, "Make Identity again");

    SecOTRFIPurgeAllFromKeychain(&testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);

    SecOTRFullIdentityRef failIDInflate2 = SecOTRFullIdentityCreateFromData(kCFAllocatorDefault, purgeExport, &testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);
    
    ok(failIDInflate2 == NULL, "Should fail 2");

    SecOTRFullIdentityRef id = SecOTRFullIdentityCreate(kCFAllocatorDefault, &testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);
    
    ok(id != NULL, "Make Identity");

    CFMutableDataRef firstExport = CFDataCreateMutable(kCFAllocatorDefault, 0);

    ok(SecOTRFIAppendSerialization(id, firstExport, &testError), "First export");
    RegressionsLogError(testError);
    CFReleaseNull(testError);

    SecOTRFullIdentityRef idInflate = SecOTRFullIdentityCreateFromData(kCFAllocatorDefault, firstExport, &testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);
    
    ok(idInflate != NULL, "Inflate Identity");


    CFMutableDataRef secondExport = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok(SecOTRFIAppendSerialization(idInflate, secondExport, &testError), "second export");
    RegressionsLogError(testError);
    CFReleaseNull(testError);

    ok(CFDataGetLength(firstExport) == CFDataGetLength(secondExport)
       && 0 == memcmp(CFDataGetBytePtr(firstExport), CFDataGetBytePtr(secondExport), (size_t)CFDataGetLength(firstExport)), "Different exports");
    
    SecOTRPublicIdentityRef pubID = SecOTRPublicIdentityCopyFromPrivate(kCFAllocatorDefault, id, &testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);
    
    ok(id != NULL, "Failed to copy public identity");
    
    CFMutableDataRef firstPublicExport = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok(SecOTRPIAppendSerialization(pubID, firstPublicExport, &testError), "failed first public export");
    RegressionsLogError(testError);
    CFReleaseNull(testError);

    SecOTRPublicIdentityRef pubIDInflate = SecOTRPublicIdentityCreateFromData(kCFAllocatorDefault, firstPublicExport, &testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);
    
    ok(pubIDInflate != NULL, "Pub inflate failed");
    
    CFMutableDataRef secondPublicExport = CFDataCreateMutable(kCFAllocatorDefault, 0);
    
    ok(SecOTRPIAppendSerialization(pubID, secondPublicExport, &testError), "failed second public export");
    RegressionsLogError(testError);
    CFReleaseNull(testError);

    ok(CFDataGetLength(firstPublicExport) == CFDataGetLength(secondPublicExport) 
       && 0 == memcmp(CFDataGetBytePtr(firstPublicExport), CFDataGetBytePtr(secondPublicExport), (size_t)CFDataGetLength(firstPublicExport)), "Different public exports");

    uint8_t sampleByteString[] = {
        0x30, 0x81, 0xf6, 0x81, 0x43, 0x00, 0x41, 0x04, 0xc6, 0x8a, 0x2a, 0x5c, 0x29, 0xa4, 0xb7, 0x58,
        0xe1, 0x3c, 0x07, 0x19, 0x20, 0xf3, 0x0b, 0xb8, 0xb3, 0x40, 0x41, 0x29, 0x4a, 0xa6, 0x7a, 0x56,
        0x28, 0x6d, 0x10, 0x85, 0x2b, 0x14, 0x83, 0xaa, 0x1f, 0x6a, 0x47, 0xbc, 0x19, 0x26, 0x39, 0x1c,
        0xd4, 0xbb, 0x8c, 0xd6, 0x94, 0x24, 0x79, 0x60, 0xfb, 0x8e, 0x4b, 0xf4, 0x0f, 0xbf, 0x38, 0x81,
        0x78, 0xce, 0x1d, 0xd9, 0x03, 0xec, 0x65, 0xcd, 0x82, 0x81, 0xae, 0x00, 0xac, 0x30, 0x81, 0xa9,
        0x02, 0x81, 0xa1, 0x00, 0xd2, 0xf4, 0x40, 0x8b, 0x2f, 0x09, 0x75, 0x2c, 0x68, 0x12, 0x76, 0xb9,
        0xfb, 0x1b, 0x02, 0x91, 0x6d, 0xd7, 0x86, 0x49, 0xdc, 0xef, 0x38, 0xf3, 0x50, 0x58, 0xb5, 0xff,
        0x5c, 0x02, 0x8a, 0xb0, 0xcd, 0xb3, 0x3d, 0x94, 0x71, 0x7d, 0x32, 0x53, 0xed, 0x43, 0xfb, 0xde,
        0xbc, 0x20, 0x21, 0x33, 0xe3, 0xeb, 0x93, 0x48, 0xe8, 0xd1, 0x32, 0x2f, 0x40, 0x40, 0x47, 0x1f,
        0xeb, 0x7e, 0xf6, 0x43, 0x81, 0x51, 0xd6, 0x4f, 0xe0, 0x57, 0xbf, 0x12, 0xeb, 0x18, 0x2e, 0x81,
        0x0b, 0x3a, 0x04, 0xf1, 0xeb, 0x3c, 0xe1, 0xb9, 0xf4, 0x87, 0x37, 0x83, 0x5a, 0x2e, 0x09, 0xf8,
        0xd5, 0xa0, 0x12, 0xfb, 0x35, 0xe4, 0xd4, 0x3f, 0xef, 0x24, 0x3e, 0x6c, 0xff, 0xb1, 0x35, 0x7e,
        0x9f, 0xe7, 0x6d, 0x2f, 0xf8, 0x0d, 0xc6, 0xbc, 0x19, 0xe2, 0x78, 0xb3, 0x71, 0xe1, 0x35, 0xe7,
        0xc7, 0x22, 0x6b, 0x4d, 0x92, 0xc4, 0x10, 0x75, 0x1a, 0x9b, 0x9f, 0x7f, 0xac, 0x2d, 0xfb, 0xc9,
        0x64, 0x1e, 0x80, 0x11, 0x7f, 0x75, 0x8a, 0x86, 0x7e, 0x09, 0x44, 0xc4, 0x71, 0xbf, 0xd4, 0xfa,
        0x8b, 0x6a, 0xb8, 0x9f, 0x02, 0x03, 0x01, 0x00,
        0x01}; 
    
    CFDataRef testInteropImport = CFDataCreate(kCFAllocatorDefault, sampleByteString, sizeof(sampleByteString));
    SecOTRPublicIdentityRef interopIDInflate = SecOTRPublicIdentityCreateFromData(kCFAllocatorDefault, testInteropImport, &testError);
    RegressionsLogError(testError);
    CFReleaseNull(testError);
    ok(interopIDInflate != NULL, "Interop inflate failed");
    
    /* cleanup keychain */
    ok(SecOTRFIPurgeAllFromKeychain(&testError),"cleanup keychain");
    RegressionsLogError(testError);
    CFReleaseNull(testError);

    CFReleaseSafe(pubID);
    CFReleaseSafe(pubIDInflate);
    CFReleaseSafe(firstPublicExport);
    CFReleaseSafe(secondPublicExport);
    CFReleaseSafe(id);
    CFReleaseSafe(idToPurge);
    CFReleaseSafe(idInflate);
    CFReleaseSafe(firstExport);
    CFReleaseSafe(secondExport);
    CFReleaseSafe(purgeExport);
    CFReleaseSafe(purgeIdInflate);
    CFReleaseSafe(failIDInflate);
    CFReleaseSafe(failIDInflate2);
    CFReleaseSafe(testInteropImport);
    CFReleaseSafe(interopIDInflate);
    
}

int otr_00_identity(int argc, char *const *argv)
{
    plan_tests(kTestTestCount);
    
	tests();
    
	return 0;    
}
