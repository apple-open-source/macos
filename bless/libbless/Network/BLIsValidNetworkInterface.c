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
 *  BLIsValidNetworkInterface.c
 *  bless
 *
 *  Created by Shantonu Sen on 11/15/05.
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

bool isInterfaceLinkUp(BLContextPtr context,
                        io_service_t service);

bool BLIsValidNetworkInterface(BLContextPtr context,
                              const char *ifname)
{
    
    io_service_t    interface = IO_OBJECT_NULL;
    CFMutableDictionaryRef  matchingDict = NULL, propDict = NULL;
    CFStringRef     bsdName;
    
    bsdName = CFStringCreateWithCString(kCFAllocatorDefault,
                                        ifname,
                                        kCFStringEncodingUTF8);
    if(bsdName == NULL) {
        contextprintf(context, kBLLogLevelError, "Could not get interpret interface as C string\n");        
        return false;
    }
    
    matchingDict = IOServiceMatching(kIONetworkInterfaceClass);    
    propDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                         &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
    
    CFDictionaryAddValue(propDict, CFSTR(kIOBSDNameKey), bsdName);
    CFDictionaryAddValue(matchingDict, CFSTR(kIOPropertyMatchKey), propDict);
    CFRelease(propDict);
    CFRelease(bsdName);
    
    interface = IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
    if(interface == IO_OBJECT_NULL) {
        contextprintf(context, kBLLogLevelError, "Could not get interface for %s\n",
                      ifname);
        return false;
    }

    if(isInterfaceLinkUp(context, interface)) {
        contextprintf(context, kBLLogLevelVerbose, "Interface for %s is valid\n",
                      ifname);
        
    } else {
        IOObjectRelease(interface);
        
        contextprintf(context, kBLLogLevelError, "Interface for %s is not valid\n",
                      ifname);
        
        return false;
    }
    
    IOObjectRelease(interface);
    
    return true;
}

bool isInterfaceLinkUp(BLContextPtr context,
                                io_service_t serv)
{
	CFTypeRef   linkStatus, builtin, netbootable;
	bool        hasLink, isbootable = false;
	
	builtin = IORegistryEntryCreateCFProperty(serv, CFSTR(kIOBuiltin),
											  kCFAllocatorDefault, 0);
	
	if (builtin && CFGetTypeID(builtin) == CFBooleanGetTypeID()
		&& CFEqual(builtin, kCFBooleanTrue)) {
		isbootable = true;
	}
	if (builtin) CFRelease(builtin);
	
	if (!isbootable) {
		netbootable = IORegistryEntrySearchCFProperty(serv, kIOServicePlane, CFSTR("ioNetBootable"),
													  kCFAllocatorDefault, kIORegistryIterateRecursively|kIORegistryIterateParents);
		
		if (netbootable && CFGetTypeID(netbootable) == CFDataGetTypeID()) {
			isbootable = true;
		}
		if (netbootable) CFRelease(netbootable);
	}
	
	if(!isbootable) {
		contextprintf(context, kBLLogLevelError, "Interface is not built-in\n");
		
		return false;
	}
	
	
	linkStatus = IORegistryEntrySearchCFProperty(serv, kIOServicePlane,
												 CFSTR(kIOLinkStatus),
												 kCFAllocatorDefault,
												 kIORegistryIterateRecursively|kIORegistryIterateParents);
	if(linkStatus == NULL) {
		hasLink = false;
	} else {
		if(CFGetTypeID(linkStatus) != CFNumberGetTypeID()) {
			hasLink = false;
		} else {
			uint32_t    linkNum;
			
			if(!CFNumberGetValue(linkStatus, kCFNumberSInt32Type, &linkNum)) {
				hasLink = false;
			} else {
				if((linkNum & (kIONetworkLinkValid|kIONetworkLinkActive))
				   == (kIONetworkLinkValid|kIONetworkLinkActive)) {
					hasLink = true;
				} else {
					hasLink = false;
				}
			}
		}
		CFRelease(linkStatus);
	}
	
	contextprintf(context, kBLLogLevelVerbose, "Interface %s an active link\n",
				  hasLink ? "has" : "does not have");
	
	if(hasLink) {
		return true;
	} else {
		return false;
	}
}

