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
 *  BLInterpretEFIXMLRepresentationAsNetworkPath.c
 *  bless
 *
 *  Created by Shantonu Sen on 12/2/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 */

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>

#include <sys/socket.h>
#include <net/if.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "bless.h"
#include "bless_private.h"

int BLInterpretEFIXMLRepresentationAsNetworkPath(BLContextPtr context,
                                                 CFStringRef xmlString,
                                                 BLNetBootProtocolType *protocol,
                                                 char *interface,
                                                 char *host,
                                                 char *path)
{
    CFArrayRef  efiArray = NULL;
    CFIndex     count, i, foundinterfaceindex, foundserverindex;
    int         foundmac = 0, foundinterface = 0, foundserver = 0, foundPXE = 0;
    CFDataRef   macAddress = 0;
    char        buffer[1024];
        
    io_iterator_t   iter;
    io_service_t    service;
    kern_return_t   kret;
    
    
    if(!CFStringGetCString(xmlString, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
        return 1;
    }
    
    efiArray = IOCFUnserialize(buffer,
                               kCFAllocatorDefault,
                               0,
                               NULL);
    if(efiArray == NULL) {
        contextprintf(context, kBLLogLevelError, "Could not unserialize string\n");
        return 2;
    }
    
    if(CFGetTypeID(efiArray) != CFArrayGetTypeID()) {
        CFRelease(efiArray);
        contextprintf(context, kBLLogLevelError, "Bad type in XML string\n");
        return 2;        
    }
    
    // we do a first pass to validate types, and check for the BLMacAddress hint
    count = CFArrayGetCount(efiArray);
    foundinterfaceindex = -1;
    for(i=0; i < count; i++) {
        CFDictionaryRef dict = CFArrayGetValueAtIndex(efiArray, i);
        
        if(CFGetTypeID(dict) != CFDictionaryGetTypeID()) {
            CFRelease(efiArray);
            contextprintf(context, kBLLogLevelError, "Bad type in XML string\n");
            return 2;                    
        }
        
        if(!foundmac) {
            macAddress = CFDictionaryGetValue(dict, CFSTR("BLMACAddress"));
            if(macAddress) {
                if(CFGetTypeID(macAddress) == CFDataGetTypeID()) {
                    // we found it at this index
                    foundmac = 1;
                    foundinterfaceindex = i;
                }
            }                
        }
        
        if(!foundPXE) {
            CFStringRef compType;
            compType = CFDictionaryGetValue(dict, CFSTR("IOEFIDevicePathType"));
            if(compType && CFEqual(compType, CFSTR("MessagingNetbootProtocol"))) {
                CFStringRef	guid;
                
                guid = CFDictionaryGetValue(dict, CFSTR("Protocol"));
                if(guid && CFEqual(guid, CFSTR("FE3913DB-9AEE-4E40-A294-ABBE93A1A4B7"))) {
                    foundPXE = 1;
                }
            }
            
        }
        
    }
    
    if(foundmac) {
        const unsigned char *macbytes = CFDataGetBytePtr(macAddress);
        CFIndex        maclength = CFDataGetLength(macAddress);
        
        contextprintf(context, kBLLogLevelVerbose, "Found MAC address: ");
        for(i=0; i < maclength; i++) {
               contextprintf(context, kBLLogLevelVerbose, "%02x%s",
                             macbytes[i],
                             i < maclength-1 ? ":" : "");
        }
        contextprintf(context, kBLLogLevelVerbose, "\n");
        
        // search for all network interfaces that have this mac address

        kret = IOServiceGetMatchingServices(kIOMasterPortDefault,
                                            IOServiceMatching(kIONetworkInterfaceClass),
                                            &iter);
        if(kret == 0) {
            while ((service = IOIteratorNext(iter))) {
                CFDataRef checkMac;
                
                checkMac = IORegistryEntrySearchCFProperty(service, kIOServicePlane,
                                                           CFSTR(kIOMACAddress),
                                                           kCFAllocatorDefault,
                                                           kIORegistryIterateRecursively|kIORegistryIterateParents);
                if(checkMac) {
                 
                    // see if it matches ours
                    if(CFGetTypeID(checkMac) == CFDataGetTypeID()
                       && CFEqual(checkMac, macAddress)) {
                        CFStringRef name = IORegistryEntryCreateCFProperty(service,
                                                                           CFSTR(kIOBSDNameKey),
                                                                           kCFAllocatorDefault,
                                                                           0);
                        if(name && CFGetTypeID(name) == CFStringGetTypeID()) {
                            CFStringGetCString(name, interface, IF_NAMESIZE,kCFStringEncodingUTF8);
                            contextprintf(context, kBLLogLevelVerbose, "Found interface: %s\n", interface);
                            foundinterface = 1;
                        }
                        
                        if(name) CFRelease(name);
                    }
                    CFRelease(checkMac);
                }
                
            
                IOObjectRelease(service);
            }
            IOObjectRelease(iter);
            
        }    
    }
    
    if(!foundinterface) {
        foundinterfaceindex = -1; // reset, in case BLMacAddress was a false positive
        
        // try to match against any IOMatch
        for(i=0; i < count; i++) {
            CFDictionaryRef dict = CFArrayGetValueAtIndex(efiArray, i);
            CFDictionaryRef iomatch = CFDictionaryGetValue(dict, CFSTR("IOMatch"));
        
            if(iomatch && CFGetTypeID(iomatch) == CFDictionaryGetTypeID()) {
             
				CFRetain(iomatch);
                service = IOServiceGetMatchingService(kIOMasterPortDefault,iomatch);
                if(service != IO_OBJECT_NULL) {
                    
                    CFStringRef name = NULL;
                    
                    if(!IOObjectConformsTo(service,kIONetworkInterfaceClass)) {
                        contextprintf(context, kBLLogLevelVerbose, "found service but it is not a network interface\n");                            
                    } else {
                    
                        name = IORegistryEntryCreateCFProperty(service,
                                                                           CFSTR(kIOBSDNameKey),
                                                                           kCFAllocatorDefault,
                                                                           0);
                        if(name && CFGetTypeID(name) == CFStringGetTypeID()) {
                            CFStringGetCString(name, interface, IF_NAMESIZE,kCFStringEncodingUTF8);
                            contextprintf(context, kBLLogLevelVerbose, "Found interface: %s\n", interface);
                            foundinterface = 1;
                            foundinterfaceindex = i;
                        }
                        
                        if(name) CFRelease(name);
                    }

                    IOObjectRelease(service);
                }
            }
            
        }
        
    }
    
    if(!foundinterface) {
        contextprintf(context, kBLLogLevelVerbose, "Could not find network interface.\n");
        return 3;
    }
    
    // now that we know which entry had the interface, start searching from then on for
    // a potential remove server. If not, this is broadcast mode
    foundserverindex = -1;
    for(i=foundinterfaceindex; i < count; i++) {
        CFDictionaryRef dict = CFArrayGetValueAtIndex(efiArray, i);
        CFStringRef devpathtype = CFDictionaryGetValue(dict, CFSTR("IOEFIDevicePathType"));
        
        if(devpathtype && CFEqual(devpathtype, CFSTR("MessagingIPv4"))) {
            CFStringRef remoteIP = CFDictionaryGetValue(dict, CFSTR("RemoteIpAddress"));
            
            if(remoteIP && CFGetTypeID(remoteIP) == CFStringGetTypeID()) {
                CFStringGetCString(remoteIP, host, NS_MAXDNAME,kCFStringEncodingUTF8);
                contextprintf(context, kBLLogLevelVerbose, "Found server: %s\n", host);
                foundserver = 1;
                foundserverindex = i;
                break;
            } else {
                contextprintf(context, kBLLogLevelVerbose, "Malformed MessagingIPv4 entry. Ignoring...\n");
            }
            
        }
    }

    if(foundserver) {

        path[0] = '\0';
    } else {
        // if no server, there can't be a TFTP path
        
        strlcpy(host, "255.255.255.255", NS_MAXDNAME);
        path[0] = '\0';
    }
    
    if(foundPXE) {
        *protocol = kBLNetBootProtocol_PXE;
    } else {
        *protocol = kBLNetBootProtocol_BSDP;    
    }
        
    return 0;
}
