/*
 * Copyright (c) 2005-2007 Apple Inc. All Rights Reserved.
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
/*
 *  BLCreateEFIXMLRepresentationForDevice.c
 *  bless
 *
 *  Created by Shantonu Sen on 12/2/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 */
#include <TargetConditionals.h>

#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFSerialize.h>
#import <IOKit/IOBSD.h>
#import <IOKit/IOKitKeys.h>
#import <IOKit/storage/IOMedia.h>

#import <CoreFoundation/CoreFoundation.h>

#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "bless.h"
#include "bless_private.h"

extern int addMatchingInfoForBSDName(BLContextPtr context,
                              mach_port_t masterPort,
                              CFMutableDictionaryRef dict,
                              const char *bsdName,
                              bool shortForm);

int BLCreateEFIXMLRepresentationForDevice(BLContextPtr context,
                                        const char *bsdName,
                                        const char *optionalData,
                                        CFStringRef *xmlString,
                                        bool shortForm)
{
    int ret;
    mach_port_t masterPort;
    kern_return_t kret;
    
    CFDataRef xmlData;
    CFMutableDictionaryRef dict;
    CFMutableArrayRef array;
    
    const UInt8 *xmlBuffer;
    UInt8 *outBuffer;
    CFIndex count;
    
    kret = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if(kret) return 1;
    
    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    
    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
    
    ret = addMatchingInfoForBSDName(context, masterPort, dict, bsdName, shortForm);
    if(ret) {
        CFRelease(dict);
        CFRelease(array);
        return 2;
    }    
    CFArrayAppendValue(array, dict);
    CFRelease(dict);
    
    if(optionalData) {
        CFStringRef optString = CFStringCreateWithCString(kCFAllocatorDefault, optionalData, kCFStringEncodingUTF8);
        
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(dict, CFSTR("IOEFIBootOption"),
                             optString);
        CFArrayAppendValue(array, dict);
        CFRelease(dict);        
        
        CFRelease(optString);
    }
    
    xmlData = IOCFSerialize(array, 0);
    CFRelease(array);
    
    if(xmlData == NULL) {
        contextprintf(context, kBLLogLevelError, "Can't create XML representation\n");
        return 2;
    }
    
    count = CFDataGetLength(xmlData);
    xmlBuffer = CFDataGetBytePtr(xmlData);
    outBuffer = calloc(count+1, sizeof(char)); // terminate
    
    memcpy(outBuffer, xmlBuffer, count);
    CFRelease(xmlData);
    
    *xmlString = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)outBuffer, kCFStringEncodingUTF8);
    
    free(outBuffer);
    
    return 0;
}

