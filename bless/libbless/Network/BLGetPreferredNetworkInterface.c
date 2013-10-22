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
 *  BLGetIOServiceForPreferredNetworkInterface.c
 *  bless
 *
 *  Created by Shantonu Sen on 11/14/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 */

#import <mach/mach_error.h>

#import <IOKit/IOKitLib.h>
#import <IOKit/IOKitKeys.h>
#import <IOKit/network/IONetworkInterface.h>
#import <IOKit/network/IONetworkController.h>
#import <IOKit/network/IONetworkMedium.h>
#import <IOKit/IOBSD.h>

#include <CoreFoundation/CoreFoundation.h>

#include <sys/socket.h>
#include <net/if.h>

#include "bless.h"
#include "bless_private.h"

extern bool isInterfaceLinkUp(BLContextPtr context,
                              io_service_t serv);

static io_service_t getLinkUpInterface(BLContextPtr context,
                                       io_iterator_t iterator);

/* Algorithm:
    1) Search for IONetworkInterface that are built-in and have
        an active link
    2) Rank those according to order seen, or IOPrimaryInterface
*/
int BLGetPreferredNetworkInterface(BLContextPtr context,
                                char *ifname)
{
  
    io_service_t    interface = IO_OBJECT_NULL;
    kern_return_t   kret;
    CFMutableDictionaryRef  matchingDict = NULL, propDict = NULL;
    io_iterator_t   iterator = IO_OBJECT_NULL;
    
    ifname[0] = '\0';
    
    matchingDict = IOServiceMatching(kIONetworkInterfaceClass);    
    propDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
    
    CFDictionaryAddValue(propDict, CFSTR(kIOBuiltin), kCFBooleanTrue);
    CFDictionaryAddValue(propDict, CFSTR(kIOPrimaryInterface), kCFBooleanTrue);
    CFDictionaryAddValue(matchingDict, CFSTR(kIOPropertyMatchKey), propDict);
    CFRelease(propDict);
    
    kret = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict,
                                            &iterator);
    if(kret) {
        contextprintf(context, kBLLogLevelError, "Could not get interface iterator\n");
        return 1;
    }
    
    interface = getLinkUpInterface(context, iterator);
    IOObjectRelease(iterator);
    
    if(interface == IO_OBJECT_NULL) {
        
        matchingDict = IOServiceMatching(kIONetworkInterfaceClass);    
        propDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
        
        CFDictionaryAddValue(propDict, CFSTR(kIOBuiltin), kCFBooleanTrue);
        CFDictionaryAddValue(matchingDict, CFSTR(kIOPropertyMatchKey), propDict);
        CFRelease(propDict);
        
        kret = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict,
                                            &iterator);
        if(kret) {
            contextprintf(context, kBLLogLevelError, "Could not get interface iterator\n");
            return 1;
        }
        
        interface = getLinkUpInterface(context, iterator);
        IOObjectRelease(iterator);        
    }

	if(interface == IO_OBJECT_NULL) {
        
        matchingDict = IOServiceMatching(kIONetworkInterfaceClass);    
        propDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
        
        CFDictionaryAddValue(matchingDict, CFSTR(kIOPropertyMatchKey), propDict);
        CFRelease(propDict);
        
        kret = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict,
                                            &iterator);
        if(kret) {
            contextprintf(context, kBLLogLevelError, "Could not get interface iterator\n");
            return 1;
        }
        
        interface = getLinkUpInterface(context, iterator);
        IOObjectRelease(iterator);        
    }
	
    if(interface != IO_OBJECT_NULL) {
        CFStringRef name;

        name = IORegistryEntryCreateCFProperty(interface, CFSTR(kIOBSDNameKey),
                                               kCFAllocatorDefault,
                                               0);

        if(name == NULL || CFGetTypeID(name) != CFStringGetTypeID()) {
            if(name) CFRelease(name);
            IOObjectRelease(interface);

            contextprintf(context, kBLLogLevelError, "Preferred interface does not have a BSD name\n");
            return 2;
        }
        
        if(!CFStringGetCString(name, ifname, IF_NAMESIZE, kCFStringEncodingUTF8)) {
            CFRelease(name);
            IOObjectRelease(interface);

            contextprintf(context, kBLLogLevelError, "Could not get BSD name\n");
            return 3;
        }

        CFRelease(name);
        IOObjectRelease(interface);

        contextprintf(context, kBLLogLevelVerbose, "Found primary interface: %s\n", ifname);

        return 0;
    }
    
    return 2;
}

static io_service_t getLinkUpInterface(BLContextPtr context,
                                       io_iterator_t iterator)
{
    io_service_t    serv;
    kern_return_t   kret;

    if(!IOIteratorIsValid(iterator))
        IOIteratorReset(iterator);
    
    while((serv = IOIteratorNext(iterator))) {
        io_string_t   path;
        bool        hasLink;
        
        hasLink = isInterfaceLinkUp(context, serv);
        
        kret = IORegistryEntryGetPath(serv, kIOServicePlane, path);
        if(kret) {
            strlcpy(path, "<unknown>", sizeof path);
        }
        
        contextprintf(context, kBLLogLevelVerbose, "Interface at %s %s an active link\n",
                      path,
                      hasLink ? "has" : "does not have");

        if(hasLink) {
            return serv;
        } else {
            IOObjectRelease(serv);
        }
    }
    
    return IO_OBJECT_NULL;
}

