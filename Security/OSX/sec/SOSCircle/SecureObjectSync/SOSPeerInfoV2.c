//
//  SOSPeerInfoV2.c
//  sec
//
//  Created by Richard Murphy on 1/26/15.
//
//

#include <AssertMacros.h>
#include "SOSPeerInfoV2.h"
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>
#include <utilities/der_date.h>


// Description Dictionary Entries Added for V2
CFStringRef sV2DictionaryKey            = CFSTR("V2DictionaryData");        // CFData wrapper for V2 extensions
CFStringRef sViewsKey                   = CFSTR("Views");                   // Array of View labels
CFStringRef sSerialNumberKey            = CFSTR("SerialNumber");
CFStringRef sViewsPending               = CFSTR("ViewsPending");            // Array of View labels (pending)

CFStringRef sSecurityPropertiesKey      = CFSTR("SecurityProperties");
CFStringRef kSOSHsaCrKeyDictionary      = CFSTR("HSADictionary");
CFStringRef sRingState                  = CFSTR("RingState");
CFStringRef sBackupKeyKey               = CFSTR("BackupKey");
CFStringRef sEscrowRecord               = CFSTR("EscrowRecord");

#if TARGET_OS_IPHONE

// need entitlement for this:

#include <MobileGestalt.h>

static CFStringRef SOSCopySerialNumberAsString(CFErrorRef *error) {
    CFTypeRef iosAnswer = (CFStringRef) MGCopyAnswer(kMGQSerialNumber, NULL);
    if(!iosAnswer) {
        SOSCreateError(kSOSErrorAllocationFailure,  CFSTR("No Memory"), NULL, error);
    }
    return (CFStringRef) iosAnswer;
}

#else

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

static CFStringRef SOSCopySerialNumberAsString(CFErrorRef *error) {
    CFStringRef serialNumber = NULL;
    CFStringRef retval = NULL;

    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    require_action_quiet(platformExpert, errOut, SOSCreateError(kSOSErrorAllocationFailure,  CFSTR("No Memory"), NULL, error));
    serialNumber = IORegistryEntryCreateCFProperty(platformExpert, CFSTR(kIOPlatformSerialNumberKey), kCFAllocatorDefault, 0);
    if(serialNumber) retval = CFStringCreateCopy(kCFAllocatorDefault, serialNumber);
    IOObjectRelease(platformExpert);
    CFReleaseNull(serialNumber);
errOut:
    return retval;
}

#endif

bool SOSPeerInfoSerialNumberIsSet(SOSPeerInfoRef pi) {
    CFStringRef serial = SOSPeerInfoV2DictionaryCopyString(pi, sSerialNumberKey);
    bool retval = (serial != NULL);
    CFReleaseNull(serial);
    return retval;
}

void SOSPeerInfoSetSerialNumber(SOSPeerInfoRef pi) {
    CFStringRef serialNumber = SOSCopySerialNumberAsString(NULL);
    if(serialNumber) SOSPeerInfoV2DictionarySetValue(pi, sSerialNumberKey, serialNumber);
    CFReleaseNull(serialNumber);
}

const CFStringRef SOSSerialUnknown = CFSTR("Unknown");

void SOSPeerInfoSetTestSerialNumber(SOSPeerInfoRef pi, CFStringRef serialNumber) {
    if(serialNumber) SOSPeerInfoV2DictionarySetValue(pi, sSerialNumberKey, serialNumber);
}

CFStringRef SOSPeerInfoCopySerialNumber(SOSPeerInfoRef pi) {
    CFStringRef retval = SOSPeerInfoV2DictionaryCopyString(pi, sSerialNumberKey);
    return (retval ? retval : CFRetain(SOSSerialUnknown));
}

static bool SOSPeerInfoV2SanityCheck(SOSPeerInfoRef pi) {
    if(!pi) {
        return false;
    }
    if(!SOSPeerInfoVersionHasV2Data(pi)) {
        return false;
    }
    return true;
}

static CFDataRef SOSPeerInfoGetV2Data(SOSPeerInfoRef pi) {
    if(SOSPeerInfoV2SanityCheck(pi) == false) return NULL;
    return asData(CFDictionaryGetValue(pi->description, sV2DictionaryKey), NULL);
}

static CFMutableDictionaryRef SOSCreateDictionaryFromDER(CFDataRef v2Data, CFErrorRef *error) {
    CFMutableDictionaryRef retval = NULL;
    CFPropertyListRef pl = NULL;
    
    if(!v2Data) {
        secerror("Creating raw dictionary instead of creating from DER");
        return CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    if(CFGetTypeID(v2Data) != CFDataGetTypeID()) {
        SOSCreateError(kSOSErrorBadFormat, CFSTR("Corrupted v2Data Item"), NULL, error);
        goto fail;
    }

    const uint8_t *der_p = CFDataGetBytePtr(v2Data);
    const uint8_t *der_end = CFDataGetLength(v2Data) + der_p;
    
    der_p = der_decode_plist(NULL, kCFPropertyListImmutable, &pl, error, der_p, der_end);

    if (der_p == NULL || der_p != der_end) {
        SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Format of Dictionary DER"), NULL, error);
        goto fail;
    }

    if (CFGetTypeID(pl) != CFDictionaryGetTypeID()) {
        CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(pl));
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                 CFSTR("Expected dictionary got %@"), description);
        CFReleaseSafe(description);
        CFReleaseSafe(pl);
        goto fail;
    }

    retval = (CFMutableDictionaryRef) pl;
    return retval;
    
fail:
    CFReleaseNull(retval);
    return NULL;
}


static CFDataRef SOSCreateDERFromDictionary(CFDictionaryRef di, CFErrorRef *error) {
    size_t size = der_sizeof_plist(di, error);
    if (size == 0) return NULL;
    uint8_t der[size];
    der_encode_plist(di, error, der, der+size);
    return CFDataCreate(kCFAllocatorDefault, der, size);
}


bool SOSPeerInfoUpdateToV2(SOSPeerInfoRef pi, CFErrorRef *error) {
    bool retval = false;
    CFDataRef v2data = NULL;
    if(!pi) return false;
    
    SOSPeerInfoSetVersionNumber(pi, PEERINFO_CURRENT_VERSION);
    CFMutableDictionaryRef v2Dictionary = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableSetRef views = SOSViewCopyViewSet(kViewSetDefault);
    CFMutableSetRef secproperties = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    CFDictionaryAddValue(v2Dictionary, sViewsKey, views);
    CFDictionaryAddValue(v2Dictionary, sSecurityPropertiesKey, secproperties);
    
    CFDictionaryAddValue(v2Dictionary, sDeviceID, CFSTR(""));
    CFDictionaryAddValue(v2Dictionary, sTransportType, SOSTransportMessageTypeIDSV2);
    CFDictionaryAddValue(v2Dictionary, sPreferIDS, kCFBooleanFalse);
    CFDictionaryAddValue(v2Dictionary, sPreferIDSFragmentation, kCFBooleanTrue);
    CFDictionaryAddValue(v2Dictionary, sPreferIDSACKModel, kCFBooleanTrue);
    
    require_action_quiet((v2data = SOSCreateDERFromDictionary(v2Dictionary, error)), out, SOSCreateError(kSOSErrorAllocationFailure, CFSTR("No Memory"), NULL, error));
    CFDictionaryAddValue(pi->description, sV2DictionaryKey, v2data);
    // To use the central serial number setting routine this must be done after the V2 dictionary is installed.
    SOSPeerInfoSetSerialNumber(pi);
    SOSPeerInfoExpandV2Data(pi, error);
    retval = true;
out:
    CFReleaseNull(views);
    CFReleaseNull(v2data);
    CFReleaseNull(v2Dictionary);
    return retval;
}

void SOSPeerInfoPackV2Data(SOSPeerInfoRef pi) {
    require(SOSPeerInfoV2SanityCheck(pi), errOut);
    require_quiet(pi->v2Dictionary, errOut);
    CFDataRef v2der = SOSCreateDERFromDictionary(pi->v2Dictionary, NULL);
    CFDictionarySetValue(pi->description, sV2DictionaryKey, v2der);
    CFReleaseNull(v2der);
errOut:
    return;
}

bool SOSPeerInfoExpandV2Data(SOSPeerInfoRef pi, CFErrorRef *error) {
    CFDataRef v2data = NULL;
    CFMutableDictionaryRef v2Dictionary = NULL;

    require_action_quiet((v2data = SOSPeerInfoGetV2Data(pi)), out, SOSCreateError(kSOSErrorDecodeFailure, CFSTR("No V2 Data in description"), NULL, error));
    require_action_quiet((v2Dictionary = SOSCreateDictionaryFromDER(v2data, error)), out, SOSCreateError(kSOSErrorDecodeFailure, CFSTR("Can't expand V2 Dictionary"), NULL, error));
    CFReleaseNull(pi->v2Dictionary);
    pi->v2Dictionary = v2Dictionary;
    return true;

out:
    CFReleaseNull(v2Dictionary);
    return false;

}

void SOSPeerInfoV2DictionarySetValue(SOSPeerInfoRef pi, const void *key, const void *value) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    if (value == NULL)
        CFDictionaryRemoveValue(pi->v2Dictionary, key);
    else
        CFDictionarySetValue(pi->v2Dictionary, key, value);
    SOSPeerInfoPackV2Data(pi);
errOut:
    return;
}

void SOSPeerInfoV2DictionaryRemoveValue(SOSPeerInfoRef pi, const void *key) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    CFDictionaryRemoveValue(pi->v2Dictionary, key);
    SOSPeerInfoPackV2Data(pi);
errOut:
    return;
}

bool SOSPeerInfoV2DictionaryHasBoolean(SOSPeerInfoRef pi, const void *key) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    CFTypeRef value = CFDictionaryGetValue(pi->v2Dictionary, key);
    if(asBoolean(value,NULL) != NULL)
        return true;
errOut:
    return false;
}

// Not API to prevent owenership confusion
static CFStringRef SOSPeerInfoV2DictionaryGetAsString(SOSPeerInfoRef pi, const void *key) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    return asString(CFDictionaryGetValue(pi->v2Dictionary, key), NULL);
errOut:
    return NULL;

}

bool SOSPeerInfoV2DictionaryHasStringValue(SOSPeerInfoRef pi, const void *key, CFStringRef value) {
    return CFEqualSafe(SOSPeerInfoV2DictionaryGetAsString(pi, key), value);
}

bool SOSPeerInfoV2DictionaryHasString(SOSPeerInfoRef pi, const void *key) {
    CFStringRef possibleID = SOSPeerInfoV2DictionaryGetAsString(pi, key);
    return possibleID != NULL && CFStringGetLength(possibleID) > 0;
}

bool SOSPeerInfoV2DictionaryHasSet(SOSPeerInfoRef pi, const void *key) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    CFTypeRef value = CFDictionaryGetValue(pi->v2Dictionary, key);
    if(asSet(value,NULL) != NULL)
        return true;
errOut:
    return false;
}

bool SOSPeerInfoV2DictionaryHasData(SOSPeerInfoRef pi, const void *key) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    CFTypeRef value = CFDictionaryGetValue(pi->v2Dictionary, key);
    if(asData(value,NULL) != NULL)
        return true;
errOut:
    return false;
}

CFMutableStringRef SOSPeerInfoV2DictionaryCopyString(SOSPeerInfoRef pi, const void *key) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    CFStringRef value = asString(CFDictionaryGetValue(pi->v2Dictionary, key), NULL);
    if(value != NULL)
        return CFStringCreateMutableCopy(kCFAllocatorDefault, CFStringGetLength(value), value);
errOut:
    return NULL;
}

static void SOSPeerInfoV2DictionaryWithValue(SOSPeerInfoRef pi, const void *key, void(^operation)(CFTypeRef value)) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    CFTypeRef value = CFRetainSafe(CFDictionaryGetValue(pi->v2Dictionary, key));
    operation(value);
    CFReleaseNull(value);
errOut:
    return;
}

void SOSPeerInfoV2DictionaryWithSet(SOSPeerInfoRef pi, const void *key, void(^operation)(CFSetRef set)) {
    SOSPeerInfoV2DictionaryWithValue(pi, key, ^(CFTypeRef value) {
        CFSetRef set = asSet(value, NULL);
        if (set) {
            operation(set);
        }
    });
}

CFMutableSetRef SOSPeerInfoV2DictionaryCopySet(SOSPeerInfoRef pi, const void *key) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    CFSetRef value = asSet(CFDictionaryGetValue(pi->v2Dictionary, key), NULL);
    if(value != NULL)
        return CFSetCreateMutableCopy(kCFAllocatorDefault, CFSetGetCount(value), value);
errOut:
    return NULL;
}

void SOSPeerInfoV2DictionaryForEachSetValue(SOSPeerInfoRef pi, const void *key, void (^action)(const void* value)) {
    CFSetRef value = NULL;
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    value = asSet(CFDictionaryGetValue(pi->v2Dictionary, key), NULL);

errOut:
    if (value) {
        CFSetForEach(value, action);
    }
}

bool SOSPeerInfoV2DictionaryHasSetContaining(SOSPeerInfoRef pi, const void *key, const void* member) {
    CFSetRef value = NULL;
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    value = asSet(CFDictionaryGetValue(pi->v2Dictionary, key), NULL);
errOut:
    return value && CFSetContainsValue(value, member);
}

CFMutableDataRef SOSPeerInfoV2DictionaryCopyData(SOSPeerInfoRef pi, const void *key) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    CFDataRef value = asData(CFDictionaryGetValue(pi->v2Dictionary, key), NULL);
    if(value != NULL)
        return CFDataCreateMutableCopy(kCFAllocatorDefault, CFDataGetLength(value), value);
errOut:
    return NULL;
}

CFBooleanRef SOSPeerInfoV2DictionaryCopyBoolean(SOSPeerInfoRef pi, const void *key) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    CFBooleanRef value = asBoolean(CFDictionaryGetValue(pi->v2Dictionary, key), NULL);
    if(value != NULL)
        return CFRetainSafe(value);
errOut:
    return NULL;
}

CFMutableDictionaryRef SOSPeerInfoV2DictionaryCopyDictionary(SOSPeerInfoRef pi, const void *key) {
    require_quiet(SOSPeerInfoExpandV2Data(pi, NULL), errOut);
    CFDictionaryRef value = asDictionary(CFDictionaryGetValue(pi->v2Dictionary, key), NULL);
    if(value != NULL)
        return CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(value), value);
errOut:
    return NULL;
}


SOSPeerInfoRef SOSPeerInfoCopyWithV2DictionaryUpdate(CFAllocatorRef allocator, SOSPeerInfoRef toCopy, CFDictionaryRef newv2dict, SecKeyRef signingKey, CFErrorRef* error) {
    SOSPeerInfoRef retval = SOSPeerInfoCreateCopy(kCFAllocatorDefault, toCopy, error);
    require_quiet(retval, errOut);
    require_action_quiet(SOSPeerInfoExpandV2Data(retval, error), errOut, CFReleaseNull(retval));
    CFDictionaryForEach(newv2dict, ^(const void *key, const void *value) {
        CFDictionarySetValue(retval->v2Dictionary, key, value);
    });
    SOSPeerInfoPackV2Data(retval);
    require_action_quiet(SOSPeerInfoSign(signingKey, retval, error), errOut, CFReleaseNull(retval));
errOut:
    return retval;
}
