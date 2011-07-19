/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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


#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include "bless.h"
#include "bless_private.h"


// 9300208: have bless/setboot.c (e.g. setit()) use BLSetEFIBootDevice()
// or, have this code use (the more generic) setit()
kern_return_t
BLSetEFIBootDevice(BLContextPtr context, char *bsdName)
{
    kern_return_t rval = KERN_FAILURE;
    char *errmsg;
    CFStringRef xmlString = NULL;
    io_registry_entry_t optionsNode = IO_OBJECT_NULL;

    errmsg = "unable to create objects";
    if (BLCreateEFIXMLRepresentationForDevice(NULL, bsdName, NULL,&xmlString,0))
        goto finish;
    contextprintf(context, kBLLogLevelVerbose,
                  "setting NVRAM to boot from '%s'\n", bsdName);
#if 0
contextprintf(context, kBLLogLevelNormal, "XML for device:\n"); 
CFShow(xmlString);
#endif
    
    errmsg = "unable to find /options node";
    optionsNode = IORegistryEntryFromPath(kIOMasterPortDefault,
                                          kIODeviceTreePlane ":/options");
    if (IO_OBJECT_NULL == optionsNode)   goto finish;
    
    errmsg = "error setting efi-boot-device";
    rval = IORegistryEntrySetCFProperty(optionsNode,
                                        CFSTR("efi-boot-device"), xmlString);
    
finish:
    if (rval != KERN_SUCCESS) {
        contextprintf(context, kBLLogLevelError,
                  "setEFIBootDevice(): %s\n", errmsg);
    }

    if (optionsNode != IO_OBJECT_NULL)
        IOObjectRelease(optionsNode);
    if (xmlString)      CFRelease(xmlString);
        
    return rval;
}
