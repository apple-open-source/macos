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
 *  BLInterpretEFIXMLRepresentationAsDevice.c
 *  bless
 *
 *  Created by Shantonu Sen on 12/2/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 */

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOBSD.h>

#include "bless.h"
#include "bless_private.h"

#if USE_DISKARBITRATION
#include <DiskArbitration/DiskArbitration.h>
#endif

static int checkForMatch(BLContextPtr context, CFDictionaryRef dict,
						 char *bsdName, int bsdNameLen);

static CFUUIDRef    copyVolUUIDFromDiskArb(BLContextPtr context,
                                          CFStringRef bsdName);

int BLInterpretEFIXMLRepresentationAsDevice(BLContextPtr context,
                                            CFStringRef xmlString,
                                            char *bsdName,
                                            int bsdNameLen)
{
	CFArrayRef  efiArray = NULL;
    CFIndex     count, i;
    char        buffer[1024];
	int			foundDevice = 0;
	
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
    
    // for each entry, see if there's a volume UUID, or if IOMatch works
    count = CFArrayGetCount(efiArray);
    for(i=0; i < count; i++) {
        CFDictionaryRef dict = CFArrayGetValueAtIndex(efiArray, i);
        
        if(CFGetTypeID(dict) != CFDictionaryGetTypeID()) {
            CFRelease(efiArray);
            contextprintf(context, kBLLogLevelError, "Bad type in XML string\n");
            return 2;                    
        }

        if(checkForMatch(context, dict, bsdName, bsdNameLen)) {
			foundDevice = 1;
			break;
		}        
    }
	
    CFRelease(efiArray);
    
	if(!foundDevice) {
		contextprintf(context, kBLLogLevelVerbose, "Could not find disk device for string\n");
		return 4;
	}
    
    return 0;
}


static int checkForMatch(BLContextPtr context, CFDictionaryRef dict,
						 char *bsdName, int bsdNameLen)
{
	CFStringRef		fsuuid;
	CFUUIDRef		uuid = NULL;
	io_service_t	service;
	int				foundDevice = 0;
	
	// first check for volume UUID. if it's present at all, it's preferred
	
	fsuuid = CFDictionaryGetValue(dict, CFSTR("BLVolumeUUID"));
	if(fsuuid && CFGetTypeID(fsuuid) == CFStringGetTypeID()) {
		uuid = CFUUIDCreateFromString(kCFAllocatorDefault, fsuuid);
	
		if(uuid == NULL) {
			contextprintf(context, kBLLogLevelVerbose, "Bad Volume UUID\n");
		}
	}
	
	if(uuid) {
		CFStringRef	lastBSDName = CFDictionaryGetValue(dict, CFSTR("BLLastBSDName"));
		
		// use this as a cache. eventually we may want to scan DiskArb's list of volumes
		if(lastBSDName && CFGetTypeID(lastBSDName) == CFStringGetTypeID()) {
			CFUUIDRef       dauuid = NULL;

            dauuid = copyVolUUIDFromDiskArb(context, lastBSDName);
            
			if(dauuid && CFEqual(uuid, dauuid)) {
				// found it!
                CFStringGetCString(lastBSDName, bsdName, 
                                   bsdNameLen, kCFStringEncodingUTF8);
                
				contextprintf(context, kBLLogLevelVerbose, "Found device: %s\n", bsdName);
				foundDevice = 1;		
				
				CFRelease(dauuid);
			}
		}
    }
    
    if (!foundDevice) {
		// not present, let's hope the matching dictionary was unique enough
		CFDictionaryRef iomatch = CFDictionaryGetValue(dict, CFSTR("IOMatch"));
		if(iomatch && CFGetTypeID(iomatch) == CFDictionaryGetTypeID()) {
			
			CFRetain(iomatch); // IOServiceGetMatchingService releases 1 ref
			service = IOServiceGetMatchingService(kIOMasterPortDefault,iomatch);
			if(service != IO_OBJECT_NULL) {
				
				CFStringRef name = NULL;
				
				if(!IOObjectConformsTo(service,kIOMediaClass)) {
					contextprintf(context, kBLLogLevelVerbose, "found service but it is not a media object\n");                            
				} else {
                    
					name = IORegistryEntryCreateCFProperty(service,
														   CFSTR(kIOBSDNameKey),
														   kCFAllocatorDefault,
														   0);
					if(name && CFGetTypeID(name) == CFStringGetTypeID()) {
                        // if we had a volume uuid, validate it!
                        if(uuid) {
                            CFUUIDRef       dauuid = NULL;
                            
                            dauuid = copyVolUUIDFromDiskArb(context, name);
                            
                            if(dauuid && CFEqual(uuid, dauuid)) {
                                foundDevice = 1;		                                
                                CFRelease(dauuid);
                            } else {
                                // we had a UUID, but this disk didn't, or was incorrect
                                foundDevice = 0;		                                                                
                            }
                        } else {
                            // we don't have a volume UUID, so assume this is enough
                            foundDevice = 1;
                        }
						
                        if(foundDevice) {
                            CFStringGetCString(name, bsdName, bsdNameLen, kCFStringEncodingUTF8);
                            contextprintf(context, kBLLogLevelVerbose, "Found device: %s\n", bsdName);
                        }
                    }
					
					if(name) CFRelease(name);
				}
				
				IOObjectRelease(service);
			}
		}
	}

    if(uuid) {
        CFRelease(uuid);
    }
    
	if(foundDevice) {
		return 1;
	} else {
		return 0;
	}
}

static CFUUIDRef    copyVolUUIDFromDiskArb(BLContextPtr context,
                                           CFStringRef bsdName)
{
    CFUUIDRef       dauuid = NULL;
#if USE_DISKARBITRATION
    DASessionRef    session = NULL;
    DADiskRef       dadisk = NULL;
    char			lastBSDNameCString[MNAMELEN];
    
    CFStringGetCString(bsdName, lastBSDNameCString, 
                       sizeof(lastBSDNameCString),kCFStringEncodingUTF8);
    
    session = DASessionCreate(kCFAllocatorDefault);
    if(session) {
        dadisk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, 
                                         lastBSDNameCString);
        if(dadisk) {
            CFDictionaryRef descrip = DADiskCopyDescription(dadisk);
            if(descrip) {
                dauuid = CFDictionaryGetValue(descrip, kDADiskDescriptionVolumeUUIDKey);
                
                if(dauuid)
                    CFRetain(dauuid);
                CFRelease(descrip);
            }
            
            CFRelease(dadisk);
        }
        
        CFRelease(session);
    }
#endif // USE_DISKARBITRATION
    return dauuid;
}
