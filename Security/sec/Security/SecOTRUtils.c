//
//  SecOTRUtils.c
//  libSecOTR
//
//  Created by Keith Henrickson on 6/11/12.
//
//

#include "SecOTR.h"
#include "SecOTRIdentityPriv.h"
#include "SecOTRSessionPriv.h"
#include <utilities/SecCFWrappers.h>
#include <stdlib.h>

#include <AssertMacros.h>

#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecBase64.h>

#include <TargetConditionals.h>

CFStringRef sLocalErrorDomain = CFSTR("com.apple.security.otr.error");

void SecOTRCreateError(enum SecOTRError family, CFIndex errorCode, CFStringRef descriptionString, CFErrorRef previousError, CFErrorRef *newError) {
    if (newError && !(*newError)) {
        const void* keys[2] = {kCFErrorDescriptionKey, kCFErrorUnderlyingErrorKey};
        const void* values[2] = {descriptionString, previousError};
        *newError = CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault, (family == secOTRErrorLocal) ? sLocalErrorDomain : kCFErrorDomainOSStatus, errorCode, keys, values, (previousError != NULL) ? 2 : 1);
    } else {
        CFReleaseSafe(previousError);
    }
}

OSStatus insertSize(CFIndex size, uint8_t* here)
{
    require(size < 0xFFFF, fail);
    
    uint8_t bytes[] = { (size >> 8) & 0xFF, size & 0xFF };
    memcpy(here, bytes, sizeof(bytes));
    
    return errSecSuccess;
    
fail:
    return errSecParam;
}

OSStatus appendSize(CFIndex size, CFMutableDataRef into)
{
    require(size < 0xFFFF, fail);
    
    uint8_t bytes[] = { (size >> 8) & 0xFF, size & 0xFF };
    CFDataAppendBytes(into, bytes, sizeof(bytes));
    
    return errSecSuccess;
    
fail:
    return errSecParam;
}

OSStatus readSize(const uint8_t** data, size_t* limit, uint16_t* size)
{
    require(limit != NULL, fail);
    require(data != NULL, fail);
    require(size != NULL, fail);
    require(*limit > 1, fail);
    
    *size = ((uint16_t)(*data)[0]) << 8 | ((uint16_t) (*data)[1]) << 0;
    
    *limit -= 2;
    *data += 2;
    
    return errSecSuccess;
fail:
    return errSecParam;
}

OSStatus appendSizeAndData(CFDataRef data, CFMutableDataRef appendTo)
{
    OSStatus status = errSecNotAvailable;
    
    require_noerr(appendSize(CFDataGetLength(data), appendTo), exit);
    CFDataAppend(appendTo, data);
    
    status = errSecSuccess;
    
exit:
    return status;
}

OSStatus appendPublicOctetsAndSize(SecKeyRef fromKey, CFMutableDataRef appendTo)
{
    OSStatus status = errSecDecode;
    CFDataRef serializedKey = NULL;
    
    require_noerr(SecKeyCopyPublicBytes(fromKey, &serializedKey), exit);
    require(serializedKey, exit);
    
    status = appendSizeAndData(serializedKey, appendTo);
    
exit:
    CFReleaseNull(serializedKey);
    return status;
}

OSStatus appendPublicOctets(SecKeyRef fromKey, CFMutableDataRef appendTo)
{
    OSStatus status = errSecDecode;
    CFDataRef serializedKey = NULL;
    
    require_noerr(SecKeyCopyPublicBytes(fromKey, &serializedKey), exit);
    require(serializedKey, exit);
    
    CFDataAppend(appendTo, serializedKey);
    
    status = errSecSuccess;
    
exit:
    CFReleaseNull(serializedKey);
    return status;
}


/* Given an EC public key in encoded form return a SecKeyRef representing
 that key. Supported encodings are kSecKeyEncodingPkcs1. */
static SecKeyRef SecKeyCreateECPublicKey(CFAllocatorRef allocator,
                                  const uint8_t *keyData, CFIndex keyDataLength) {
    CFDataRef tempData = CFDataCreate(kCFAllocatorDefault, keyData, keyDataLength);
    SecKeyRef newPublicKey = SecKeyCreateFromPublicData(kCFAllocatorDefault, kSecECDSAAlgorithmID, tempData);

    CFRelease(tempData);
    return newPublicKey;
}

typedef SecKeyRef (*createFunction_t)(CFAllocatorRef allocator,
                                      const uint8_t *keyData, CFIndex keyDataLength);

static SecKeyRef CallCreateFunctionFrom(CFAllocatorRef allocator, const uint8_t** data, size_t* limit, createFunction_t create)
{
    uint16_t foundLength = 0;
    const uint8_t* foundData = NULL;
    
    require(limit != NULL, fail);
    require(data != NULL, fail);
    
    require_noerr(readSize(data, limit, &foundLength), fail);
    require(foundLength <= *limit, fail);
    
    foundData = *data;
    
    *limit -= foundLength;
    *data += foundLength;
    
fail:
    
    return create(allocator, foundData, foundLength);
}

SecKeyRef CreateECPublicKeyFrom(CFAllocatorRef allocator, const uint8_t** data, size_t* limit)
{
    return CallCreateFunctionFrom(allocator, data, limit, &SecKeyCreateECPublicKey);
}

void SecOTRGetIncomingBytes(CFDataRef incomingMessage, CFMutableDataRef decodedBytes)
{
    CFDataRef header = CFStringCreateExternalRepresentation(kCFAllocatorDefault, CFSTR("?OTR:"), kCFStringEncodingUTF8, '?');
    CFDataRef footer = CFStringCreateExternalRepresentation(kCFAllocatorDefault, CFSTR("."), kCFStringEncodingUTF8, '?');
    CFRange headerLoc = CFDataFind(incomingMessage, header, CFRangeMake(0, CFDataGetLength(header)), 0);
    CFRange footerLoc = CFDataFind(incomingMessage, footer, CFRangeMake(0, CFDataGetLength(incomingMessage)), 0);
    if (kCFNotFound == headerLoc.location) {
        CFDataAppend(decodedBytes, incomingMessage);
    } else {
        CFDataRef bodyData = CFDataCreateReferenceFromRange(kCFAllocatorDefault, incomingMessage, CFRangeMake(headerLoc.length, footerLoc.location - headerLoc.length));
        size_t size = SecBase64Decode((char*)CFDataGetBytePtr(bodyData), CFDataGetLength(bodyData), NULL, 0);
        uint8_t decodedByteArray[size];
        SecBase64Decode((char*)CFDataGetBytePtr(bodyData), CFDataGetLength(bodyData), decodedByteArray, size);
        CFDataAppendBytes(decodedBytes, decodedByteArray, size);
        CFRelease(bodyData);
    }
    CFRelease(header);
    CFRelease(footer);
}

void SecOTRPrepareOutgoingBytes(CFMutableDataRef destinationMessage, CFMutableDataRef protectedMessage)
{
    CFDataRef header = CFStringCreateExternalRepresentation(kCFAllocatorDefault, CFSTR("?OTR:"), kCFStringEncodingUTF8, '?');
    CFDataRef footer = CFStringCreateExternalRepresentation(kCFAllocatorDefault, CFSTR("."), kCFStringEncodingUTF8, '?');
    size_t base64Len = SecBase64Encode(CFDataGetBytePtr(destinationMessage), CFDataGetLength(destinationMessage), NULL, 0);
    char base64Message [base64Len];
    SecBase64Encode(CFDataGetBytePtr(destinationMessage), CFDataGetLength(destinationMessage), base64Message, base64Len);
    CFDataRef base64Data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (uint8_t*)base64Message, base64Len, kCFAllocatorNull);
    
    CFDataAppend(protectedMessage, header);
    CFDataAppend(protectedMessage, base64Data);
    CFDataAppend(protectedMessage, footer);
    
    CFRelease(header);
    CFRelease(footer);
    CFRelease(base64Data);
}


