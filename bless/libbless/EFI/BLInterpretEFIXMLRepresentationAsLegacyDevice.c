/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
 *  BLInterpretEFIXMLRepresentationAsLegacyDevice.c
 *  bless
 *
 *  Created by Shantonu Sen on 2/8/06.
 *  Copyright 2006-2007 Apple Inc. All Rights Reserved.
 *
 */

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/IOBSD.h>

#include <sys/mount.h>
#include <stdbool.h>

#include "bless.h"
#include "bless_private.h"

static int findMatch(BLContextPtr context, CFStringRef legacyType,
						 CFStringRef xmlString, char *bsdName, int bsdNameLen);

int BLInterpretEFIXMLRepresentationAsLegacyDevice(BLContextPtr context,
                                            CFStringRef xmlString,
                                            char *bsdName,
                                            int bsdNameLen)
{
	CFArrayRef  efiArray = NULL;
    CFIndex     count, i;
	int			ret;
    char        buffer[1024];
	int			foundLegacyPath = 0;
	CFStringRef	legacyType = NULL;
	
	if(!BLSupportsLegacyMode(context)) {
        contextprintf(context, kBLLogLevelVerbose, "Legacy mode not supported on this system\n");		
		return 1;
	}
	
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
		CFStringRef		compType;
        
        if(CFGetTypeID(dict) != CFDictionaryGetTypeID()) {
            CFRelease(efiArray);
            contextprintf(context, kBLLogLevelError, "Bad type in XML string\n");
            return 2;                    
        }
		
		compType = CFDictionaryGetValue(dict, CFSTR("IOEFIDevicePathType"));
		if(compType && CFEqual(compType, CFSTR("MediaFirmwareVolumeFilePath"))) {
			CFStringRef	guid;
			
			guid = CFDictionaryGetValue(dict, CFSTR("Guid"));
			if(guid && CFEqual(guid, CFSTR("2B0585EB-D8B8-49A9-8B8C-E21B01AEF2B7"))) {
				foundLegacyPath = 1;
				continue;
			}
		}
		
		legacyType = CFDictionaryGetValue(dict, CFSTR("IOEFIBootOption"));
		if(legacyType && CFGetTypeID(legacyType) == CFStringGetTypeID()) {
			CFRetain(legacyType);
		} else {
			legacyType = NULL;
		}
		
    }
	
	if(!foundLegacyPath || !legacyType) {
		contextprintf(context, kBLLogLevelVerbose, "Boot option is not a legacy device\n");
		return 4;
	} else {
		contextprintf(context, kBLLogLevelVerbose, "Boot option is a legacy device\n");		
	}
    
	ret = findMatch(context, legacyType, xmlString, bsdName, bsdNameLen);
	if(ret) {
		contextprintf(context, kBLLogLevelVerbose, "Could not find device for legacy type\n");		
		return 5;
	}
	
	CFRelease(legacyType);
	
    return 0;
}


static int findMatch(BLContextPtr context, CFStringRef legacyType,
						 CFStringRef xmlString, char *bsdName, int bsdNameLen)
{
	char			legacyCStr[256];
	int				numfs, i;
	int				ret;
	size_t			bufsize;
	struct statfs	*buf;
	bool			foundMatch = false;
	
	if(CFStringGetCString(legacyType, legacyCStr, sizeof(legacyCStr), kCFStringEncodingUTF8)) {
		contextprintf(context, kBLLogLevelVerbose, "Searching for legacy type '%s'\n", legacyCStr);		
	}

	numfs = getfsstat(NULL, 0, MNT_NOWAIT);
	if(numfs < 0) {
		contextprintf(context, kBLLogLevelError, "Could not get list of filesystems\n");		
		return 1;
	}
	
	bufsize = numfs*sizeof(buf[0]);
	buf = (struct statfs *)calloc(bufsize, sizeof(char));
	if(buf == NULL) {
		return 2;
	}
		
	numfs = getfsstat(buf, bufsize, MNT_NOWAIT);
	if(numfs < 0) {
		contextprintf(context, kBLLogLevelError, "Could not get list of filesystems\n");		
		return 1;
	}
		
	for(i=0; i < numfs; i++) {
		struct statfs *sb = &buf[i];
		CFStringRef		newXML = NULL;
		
		if(!(sb->f_flags & MNT_LOCAL))
			continue;
		
		if(0 != strncmp(sb->f_mntfromname, "/dev/", 5))
			continue;
		
		contextprintf(context, kBLLogLevelVerbose, "filesystem[%d] '%s' => '%s'\n", 
					  i, sb->f_mntfromname, sb->f_mntonname);		

		ret = BLCreateEFIXMLRepresentationForLegacyDevice(context,
													sb->f_mntfromname + 5,
														  &newXML);
		if(ret) {
			contextprintf(context, kBLLogLevelVerbose, "Ignoring '%s'\n", 
						  sb->f_mntfromname);
			continue;
		}
		
		if(CFEqual(newXML, xmlString)) {
			// this is a match
			// see if it's a filesystem that gets priority
			
			
			if(0 == strcmp("ntfs", sb->f_fstypename)
			   || 0 == strcmp("msdos", sb->f_fstypename)) {
				
				strlcpy(bsdName, sb->f_mntfromname + 5, bsdNameLen);
				CFRelease(newXML);
				foundMatch = true;
				break;
			}
			
			if(!foundMatch) {
				// we don't have anything else, so go for it
				strlcpy(bsdName, sb->f_mntfromname + 5, bsdNameLen);
				foundMatch = true;
			} else {
				// no better than existing match
			}
			
		}

		CFRelease(newXML);
		
	}

	free(buf);
	
	if(!foundMatch)
		return 3;
	
	contextprintf(context, kBLLogLevelVerbose, "Matching legacy device '%s'\n", 
				  bsdName);

	
	return 0;
	
}
