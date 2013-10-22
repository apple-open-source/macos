//
//  SOSInternal.c
//  sec
//
//  Created by Mitch Adler on 7/18/12.
//
//

#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSAccount.h>

#include "utilities/SecCFError.h"
#include "utilities/SecCFRelease.h"
#include "utilities/SecCFWrappers.h"
#include "utilities/iOSforOSX.h"

#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <securityd/SecDbItem.h> // For SecError
#include "utilities/iOSforOSX.h"

#include <Security/SecBase64.h>

#include <AssertMacros.h>

CFStringRef kSOSErrorDomain = CFSTR("com.apple.security.sos.error");

bool SOSCreateError(CFIndex errorCode, CFStringRef descriptionString, CFErrorRef previousError, CFErrorRef *newError) {
    SOSCreateErrorWithFormat(errorCode, previousError, newError, NULL, CFSTR("%@"), descriptionString);
    return true;
}

bool SOSCreateErrorWithFormat(CFIndex errorCode, CFErrorRef previousError, CFErrorRef *newError,
                              CFDictionaryRef formatOptions, CFStringRef format, ...) {
    va_list va;
    va_start(va, format);
    bool res = SOSCreateErrorWithFormatAndArguments(errorCode, previousError, newError, formatOptions, format, va);
    va_end(va);
    return res;
}

bool SOSCreateErrorWithFormatAndArguments(CFIndex errorCode, CFErrorRef previousError, CFErrorRef *newError,
                                          CFDictionaryRef formatOptions, CFStringRef format, va_list args)
{
    SecCFCreateErrorWithFormatAndArguments(errorCode, kSOSErrorDomain, previousError, newError, formatOptions, format, args);
    return true;
}


//
// Utility Functions
//

static OSStatus GenerateECPairImp(int keySize, CFBooleanRef permanent, SecKeyRef* public, SecKeyRef *full)
{
    static const CFStringRef sTempNameToUse = CFSTR("GenerateECPair Temporary Key - Shouldn't be live");

    CFNumberRef signing_bitsize = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keySize);

    CFDictionaryRef keygen_parameters = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                     kSecAttrKeyType,       kSecAttrKeyTypeEC,
                                                                     kSecAttrKeySizeInBits, signing_bitsize,
                                                                     kSecAttrIsPermanent,   permanent,
                                                                     kSecAttrLabel,         sTempNameToUse,
                                                                     NULL);
    CFReleaseNull(signing_bitsize);
    OSStatus result = SecKeyGeneratePair(keygen_parameters, public, full);
    CFReleaseNull(keygen_parameters);

    return result;
}

OSStatus GenerateECPair(int keySize, SecKeyRef* public, SecKeyRef *full)
{
    return GenerateECPairImp(keySize, kCFBooleanFalse, public, full);
}

OSStatus GeneratePermanentECPair(int keySize, SecKeyRef* public, SecKeyRef *full)
{
    return GenerateECPairImp(keySize, kCFBooleanTrue, public, full);
}

static CFStringRef SOSCircleCopyDescriptionFromData(CFDataRef data)
{
    CFErrorRef error;
    CFStringRef result = NULL;

    SOSCircleRef circle = SOSCircleCreateFromData(kCFAllocatorDefault, data, &error);

    if (circle)
        result = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), circle);

    CFReleaseSafe(circle);

    return result;
}

CFStringRef SOSChangesCopyDescription(CFDictionaryRef changes, bool is_sender)
{
    CFMutableStringRef string = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("<Changes: {\n"));

    CFDictionaryForEach(changes, ^(const void *key, const void *value) {
        CFStringRef value_description = NULL;
        if (isString(key) && isData(value)) {
            CFDataRef value_data = (CFDataRef) value;
            switch (SOSKVSKeyGetKeyType(key)) {
                case kCircleKey:
                    value_description = SOSCircleCopyDescriptionFromData(value_data);
                    break;
                case kMessageKey:
                    value_description = SOSMessageCopyDescription(value_data);
                    break;
                default:
                    break;
            }

        }
        CFStringAppendFormat(string, NULL, CFSTR("    '%@' %s %@\n"),
                             key,
                             is_sender ? "<=" : "=>",
                             value_description ? value_description : value);
        
        CFReleaseNull(value_description);
    });

    CFStringAppendFormat(string, NULL, CFSTR("}"));

    return string;
}

CFStringRef SOSCopyIDOfKey(SecKeyRef key, CFErrorRef *error)
{
    const struct ccdigest_info * di = ccsha1_di();
    CFDataRef publicBytes = NULL;
    CFStringRef result = NULL;

    uint8_t digest[di->output_size];
    char encoded[2 * di->output_size]; // Big enough for base64 encoding.

    require_quiet(SecError(SecKeyCopyPublicBytes(key, &publicBytes), error, CFSTR("Failed to export public bytes %@"), key), fail);
    
    ccdigest(di, CFDataGetLength(publicBytes), CFDataGetBytePtr(publicBytes), digest);

    size_t length = SecBase64Encode(digest, sizeof(digest), encoded, sizeof(encoded));
    assert(length && length < sizeof(encoded));
    if (length > 26)
      length = 26;
    encoded[length] = 0;
    CFReleaseNull(publicBytes);
    return CFStringCreateWithCString(kCFAllocatorDefault, encoded, kCFStringEncodingASCII);

fail:
    CFReleaseNull(publicBytes);
    return result;
}
