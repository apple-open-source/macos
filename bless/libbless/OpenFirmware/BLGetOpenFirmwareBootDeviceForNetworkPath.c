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
 *  BLGetOpenFirmwareBootDeviceForNetworkPath.c
 *  bless
 *
 *  Created by Shantonu Sen on 4/11/06.
 *  Copyright 2006-2007 Apple Inc. All Rights Reserved.
 *
 * $Id: BLGetOpenFirmwareBootDeviceForNetworkPath.c,v 1.1 2006/04/12 00:15:05 ssen Exp $
 *
 */

#import <IOKit/IOKitLib.h>
#import <IOKit/IOBSD.h>
#import <IOKit/IOKitKeys.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>

#import <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>

#include "bless.h"
#include "bless_private.h"

int BLGetOpenFirmwareBootDeviceForNetworkPath(BLContextPtr context,
                                               const char *interface,
                                               const char *host,
                                               const char *path,
											   char * ofstring) {

    mach_port_t masterPort;
    kern_return_t kret;
    io_service_t iface, service;
	io_iterator_t iter;
	io_string_t	pathInPlane;
	bool gotPathInPlane = false;

    CFMutableDictionaryRef matchDict;

    kret = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if(kret) return 1;
        
    
    matchDict = IOBSDNameMatching(masterPort, 0, interface);
    CFDictionarySetValue(matchDict, CFSTR(kIOProviderClassKey), CFSTR(kIONetworkInterfaceClass));


    iface = IOServiceGetMatchingService(masterPort,
                                        matchDict);
    
    if(iface == IO_OBJECT_NULL) {
        contextprintf(context, kBLLogLevelError, "Could not find object for %s\n", interface);
        return 1;
    }
	
	// find this the parent that's in the device tree plane
	kret = IORegistryEntryCreateIterator(iface, kIOServicePlane,
		kIORegistryIterateRecursively|kIORegistryIterateParents,
		&iter);
	IOObjectRelease(iface);
	
	if(kret) {
        contextprintf(context, kBLLogLevelError, "Could not find object for %s\n", interface);
        return 2;	
	}

	while ( (service = IOIteratorNext(iter)) != IO_OBJECT_NULL ) {
		
		kret = IORegistryEntryGetPath(service, kIODeviceTreePlane, pathInPlane);
		if(kret == 0) {
			gotPathInPlane = true;
			IOObjectRelease(service);
			break;		
		}
		
		IOObjectRelease(service);    
	}
	IOObjectRelease(iter);

	if(!gotPathInPlane) {
        contextprintf(context, kBLLogLevelError, "Could not find parent for %s in device tree\n", interface);
		return 3;
	}

	contextprintf(context, kBLLogLevelVerbose, "Got path %s for interface %s\n", pathInPlane, interface);

	if(host && path && strlen(path)) {
		sprintf(ofstring, "%s:%s,%s", pathInPlane + strlen(kIODeviceTreePlane) + 1, host, path);		
	} else {
		sprintf(ofstring, "%s:bootp", pathInPlane + strlen(kIODeviceTreePlane) + 1);	
	}

	return 0;
}
