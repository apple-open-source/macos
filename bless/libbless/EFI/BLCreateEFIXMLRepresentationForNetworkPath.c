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
 *  BLCreateEFIXMLRepresentationForNetworkPath.c
 *  bless
 *
 *  Created by Shantonu Sen on 11/15/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 */

#import <IOKit/IOKitLib.h>
#import <IOKit/IOCFSerialize.h>
#import <IOKit/IOBSD.h>
#import <IOKit/IOKitKeys.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>

#import <CoreFoundation/CoreFoundation.h>

#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "bless.h"
#include "bless_private.h"

int BLCreateEFIXMLRepresentationForNetworkPath(BLContextPtr context,
                                               BLNetBootProtocolType protocol,
                                               const char *interface,
                                               const char *host,
                                               const char *path,
                                               const char *optionalData,
                                               CFStringRef *xmlString)
{
    mach_port_t masterPort;
    kern_return_t kret;
    io_service_t iface;
    
    CFDataRef xmlData;
    CFMutableDictionaryRef dict, matchDict;
    CFMutableArrayRef array;
    CFDataRef macAddress;
    
    const UInt8 *xmlBuffer;
    UInt8 *outBuffer;
    CFIndex count;
    
    kret = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if(kret) return 1;
        
    
    
    array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    
    dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
    
    matchDict = IOBSDNameMatching(masterPort, 0, interface);
    CFDictionarySetValue(matchDict, CFSTR(kIOProviderClassKey), CFSTR(kIONetworkInterfaceClass));

    CFRetain(matchDict);
    iface = IOServiceGetMatchingService(masterPort,
                                        matchDict);
    
    if(iface == IO_OBJECT_NULL) {
        contextprintf(context, kBLLogLevelError, "Could not find object for %s\n", interface);
        CFRelease(matchDict);
        CFRelease(dict);
        CFRelease(array);
        return 1;
    }
    
    CFDictionaryAddValue(dict, CFSTR("IOMatch"), matchDict);
    CFRelease(matchDict);
    
    macAddress = IORegistryEntrySearchCFProperty(iface, kIOServicePlane,
                                                 CFSTR(kIOMACAddress),
                                                 kCFAllocatorDefault,
                                                 kIORegistryIterateRecursively|kIORegistryIterateParents);
    if(macAddress) {
        contextprintf(context, kBLLogLevelVerbose, "MAC address %s found for %s\n",
					  BLGetCStringDescription(macAddress), interface);
        
        CFDictionaryAddValue(dict, CFSTR("BLMACAddress"), macAddress);
        CFRelease(macAddress);
    } else {
        contextprintf(context, kBLLogLevelVerbose, "No MAC address found for %s\n", interface);        
    }
    
    IOObjectRelease(iface);
    
    CFArrayAppendValue(array, dict);
    CFRelease(dict);
    
    if(host) {
        CFStringRef hostString;
        
        hostString = CFStringCreateWithCString(kCFAllocatorDefault, host, kCFStringEncodingUTF8);

        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(dict, CFSTR("IOEFIDevicePathType"),
                             CFSTR("MessagingIPv4"));
        CFDictionaryAddValue(dict, CFSTR("RemoteIpAddress"),
                             hostString);
        CFArrayAppendValue(array, dict);
        CFRelease(dict);
        CFRelease(hostString);
        
        if(path) {
            CFStringRef pathString;
            
            pathString = CFStringCreateWithCString(kCFAllocatorDefault, path, kCFStringEncodingUTF8);
            
            dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
            CFDictionaryAddValue(dict, CFSTR("IOEFIDevicePathType"),
                                 CFSTR("MediaFilePath"));
            CFDictionaryAddValue(dict, CFSTR("Path"),
                                 pathString);
            CFArrayAppendValue(array, dict);
            CFRelease(dict);            
            CFRelease(pathString);
        }
        
    }

    contextprintf(context, kBLLogLevelVerbose, "Netboot protocol %d\n", protocol);        
    if (protocol == kBLNetBootProtocol_PXE) {
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(dict, CFSTR("IOEFIDevicePathType"),
                             CFSTR("MessagingNetbootProtocol"));
        CFDictionaryAddValue(dict, CFSTR("Protocol"),
                             CFSTR("FE3913DB-9AEE-4E40-A294-ABBE93A1A4B7"));
        CFArrayAppendValue(array, dict);
        CFRelease(dict);
    }
    
    if(optionalData) {
        CFStringRef optString = CFStringCreateWithCString(kCFAllocatorDefault, optionalData, kCFStringEncodingUTF8);
        
        dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
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

