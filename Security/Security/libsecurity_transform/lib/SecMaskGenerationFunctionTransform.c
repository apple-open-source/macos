/*
 * Copyright (c) 2011-2012,2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include "SecMaskGenerationFunctionTransform.h"
#include "SecCustomTransform.h"
#include "SecDigestTransform.h"
#include "misc.h"
#include "Utilities.h"

static const CFStringRef kMaskGenerationFunctionTransformName = CFSTR("com.apple.security.MGF1");
static const CFStringRef kLengthName = CFSTR("Length");

static SecTransformInstanceBlock MaskGenerationFunctionTransform(CFStringRef name, 
                                                  SecTransformRef newTransform, 
                                                  SecTransformImplementationRef ref)
{
    __block CFMutableDataRef accumulator = CFDataCreateMutable(NULL, 0);
    __block int32_t outputLength = 0;
    
    SecTransformInstanceBlock instanceBlock = ^{
        SecTransformSetTransformAction(ref, kSecTransformActionFinalize, ^{
            CFRelease(accumulator);
            
            return (CFTypeRef)NULL;
        });
        
        // XXX: be a good citizen, put a validator in for the types.
        
        SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kLengthName, ^CFTypeRef(SecTransformAttributeRef attribute, CFTypeRef value) {
            CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, &outputLength);
            if (outputLength <= 0) {
                SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, CreateSecTransformErrorRef(kSecTransformErrorInvalidLength, "MaskGenerationFunction Length must be one or more (not %@)", value));
            }

            return (CFTypeRef)NULL;
        });
        
        SecTransformSetAttributeAction(ref, kSecTransformActionAttributeNotification, kSecTransformInputAttributeName, ^CFTypeRef(SecTransformAttributeRef attribute, CFTypeRef value) {
            if (value) {
                CFDataRef d = value;
                CFDataAppendBytes(accumulator, CFDataGetBytePtr(d), CFDataGetLength(d));
            } else {
                int32_t i = 0, l = 0;
                (void)transforms_assume(outputLength > 0);
                CFStringRef digestType = SecTranformCustomGetAttribute(ref, kSecDigestTypeAttribute, kSecTransformMetaAttributeValue);
                SecTransformRef digest0 = transforms_assume(SecDigestTransformCreate(digestType, 0, NULL));
                int32_t digestLength = 0;
                {
                    CFNumberRef digestLengthAsCFNumber = SecTransformGetAttribute(digest0, kSecDigestLengthAttribute);
                    CFNumberGetValue(transforms_assume(digestLengthAsCFNumber), kCFNumberSInt32Type, &digestLength);
                }
                (void)transforms_assume(digestLength >= 0);

                UInt8 *buffer = malloc(outputLength + digestLength);
                if (!buffer) {
                    SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, GetNoMemoryErrorAndRetain());
                    return (CFErrorRef)NULL;
                }

                dispatch_group_t all_hashed = dispatch_group_create();
                dispatch_group_enter(all_hashed);
                for(; l < outputLength; l += digestLength, i++) {
                    dispatch_group_enter(all_hashed);
                    CFErrorRef err = NULL;
                    SecTransformRef digest = NULL;
                    if (l == 0) {
                        digest = digest0;
                    } else {
                        digest = SecDigestTransformCreate(digestType, 0, &err);
                        if (digest == NULL) {
                            SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, err);
                            return (CFErrorRef)NULL;
                        }
                    }

                    // NOTE: we shuld be able to do this without the copy, make a transform that takes an
                    // array and outputs each item in the array followed by a NULL ought to be quicker.
                    CFMutableDataRef accumulatorPlusCounter = CFDataCreateMutableCopy(NULL, CFDataGetLength(accumulator) + sizeof(uint32_t), accumulator);
                    int32_t bigendian_i = htonl(i);
                    CFDataAppendBytes(accumulatorPlusCounter, (UInt8*)&bigendian_i, sizeof(bigendian_i));
                    SecTransformSetAttribute(digest, kSecTransformInputAttributeName, accumulatorPlusCounter, &err);
                    CFRelease(accumulatorPlusCounter);
                    
                    UInt8 *buf = buffer + l;
                    SecTransformExecuteAsync(digest, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFTypeRef message, CFErrorRef error, Boolean isFinal) {
                        if (message) {
                            CFIndex messageLen = CFDataGetLength(message);
                            CFDataGetBytes(message, CFRangeMake(0, messageLen), buf);
                        }
                        if (error) {
                            SecTransformCustomSetAttribute(ref, kSecTransformAbortAttributeName, kSecTransformMetaAttributeValue, error);
                        }
                        if (isFinal) {
                            dispatch_group_leave(all_hashed);
                        }
                    });
                    CFRelease(digest);
                }
                
                dispatch_group_leave(all_hashed);
                dispatch_group_wait(all_hashed, DISPATCH_TIME_FOREVER);
                CFDataRef out = CFDataCreateWithBytesNoCopy(NULL, buffer, outputLength, kCFAllocatorMalloc);
                SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, out);
                CFRelease(out);
                SecTransformCustomSetAttribute(ref, kSecTransformOutputAttributeName, kSecTransformMetaAttributeValue, NULL);
            }
            return (CFErrorRef)NULL;
        });
        
        return (CFErrorRef)NULL;
    };
    
    return Block_copy(instanceBlock);
}

SecTransformRef SecCreateMaskGenerationFunctionTransform(CFStringRef hashType, int length, CFErrorRef *error)
{
    static dispatch_once_t once;
	__block Boolean ok = TRUE;
    
    if (length <= 0) {
        if (error) {
            *error = CreateSecTransformErrorRef(kSecTransformErrorInvalidLength, "MaskGenerationFunction Length must be one or more (not %d)", length);
        }
        return NULL;
    }
    
    dispatch_once(&once, ^(void) {
        ok = SecTransformRegister(kMaskGenerationFunctionTransformName, MaskGenerationFunctionTransform, error);
    });
        
    if (!ok) {
        return NULL;
    }
    
    SecTransformRef ret = SecTransformCreate(kMaskGenerationFunctionTransformName, error);
    if (!ret) {
        return NULL;
    }
    
    if (!SecTransformSetAttribute(ret, kSecDigestTypeAttribute, hashType ? hashType : kSecDigestSHA1, error)) {
        CFRelease(ret);
        return NULL;
    }

    CFNumberRef len = CFNumberCreate(NULL, kCFNumberIntType, &length);
    ok = SecTransformSetAttribute(ret, kLengthName, len, error);
    CFRelease(len);
    if (!ok) {
        CFRelease(ret);
        return NULL;
    }

    return ret;
}
