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
 *  BLGetPreBootEnvironmentType.c
 *  bless
 *
 *  Created by Shantonu Sen on 7/12/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>

#include <mach/mach_error.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

#include <CoreFoundation/CoreFoundation.h>

#include "bless.h"
#include "bless_private.h"

#define kBootRomPath "/openprom"
#define kEFIPath "/efi"
#define kiBootPath "/chosen/iBoot"

int BLGetPreBootEnvironmentType(BLContextPtr context,
				BLPreBootEnvType *pbType) {	
    const char *path = NULL;
    kern_return_t ret;
    io_registry_entry_t entry = 0;
    CFMutableDictionaryRef props = NULL;
    CFDataRef model = NULL;
    mach_port_t	masterPort;
	
#if 0
    *pbType = kBLPreBootEnvType_EFI;
    return 0;
#endif
    
    ret = IOMasterPort( MACH_PORT_NULL, &masterPort );
    if(ret) return 0;
    
    path = kIODeviceTreePlane ":" kBootRomPath;
	
    entry = IORegistryEntryFromPath(masterPort, path);
	
    if(entry == 0) {
		path = kIODeviceTreePlane ":" kEFIPath;

		entry = IORegistryEntryFromPath(masterPort, path);
		
		if(entry == 0) {
			path = kIODeviceTreePlane ":" kiBootPath;
			
			entry = IORegistryEntryFromPath(masterPort, path);
			
			if(entry == 0) {
				*pbType = kBLPreBootEnvType_Unknown;
				contextprintf(context, kBLLogLevelVerbose,  "No OpenFirmware or EFI.\n");			
			} else {
				*pbType = kBLPreBootEnvType_iBoot;
				IOObjectRelease(entry);
				contextprintf(context, kBLLogLevelVerbose,  "iBoot found at %s\n", path);			
			}
		} else {
			*pbType = kBLPreBootEnvType_EFI;
			IOObjectRelease(entry);
			contextprintf(context, kBLLogLevelVerbose,  "EFI found at %s\n", path);			
		}
		
		return 0;
    }
	
	// for OF
    ret = IORegistryEntryCreateCFProperties(entry, &props,
											kCFAllocatorDefault, 0);
	
    if(ret) {
		contextprintf(context, kBLLogLevelError, "Could not get entry properties\n");
		CFRelease(props);
		IOObjectRelease(entry);
		// unknown
		return 0;
    }
	
    model = CFDictionaryGetValue(props, CFSTR("model"));
    if(model == NULL) {
		contextprintf(context, kBLLogLevelVerbose,  "No 'model' property for %s\n", path);
		CFRelease(props);
		IOObjectRelease(entry);
		return 0;
    }
	
	*pbType = kBLPreBootEnvType_OpenFirmware;
    contextprintf(context, kBLLogLevelVerbose, "OpenFirmware found at %s\n", path);
    contextprintf(context, kBLLogLevelVerbose, "OpenFirmware model is \"%*s\"\n", (int)CFDataGetLength(model), CFDataGetBytePtr(model));
    
    CFRelease(props);
    IOObjectRelease(entry);
	
    return 0;
}
