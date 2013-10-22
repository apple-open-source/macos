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
 *  BLCopyEFINVRAMVariableAsString.c
 *  bless
 *
 *  Created by Shantonu Sen on 12/2/05.
 *  Copyright 2005-2007 Apple Inc. All Rights Reserved.
 *
 */

#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

#include "bless.h"
#include "bless_private.h"

int BLCopyEFINVRAMVariableAsString(BLContextPtr context,
                                   CFStringRef  name,
                                   CFStringRef *value)
{
    
    io_registry_entry_t optionsNode = 0;
    char            cStr[1024];
    CFTypeRef       valRef;
    CFStringRef     stringRef;
    
    *value = NULL;
    
    optionsNode = IORegistryEntryFromPath(kIOMasterPortDefault, kIODeviceTreePlane ":/options");
    
    if(IO_OBJECT_NULL == optionsNode) {
        contextprintf(context, kBLLogLevelError,  "Could not find " kIODeviceTreePlane ":/options\n");
        return 1;
    }
    
    valRef = IORegistryEntryCreateCFProperty(optionsNode, name, kCFAllocatorDefault, 0);
    IOObjectRelease(optionsNode);
    
    if(valRef == NULL)
        return 0;
    
    if(CFGetTypeID(valRef) == CFStringGetTypeID()) {
        if(!CFStringGetCString(valRef, cStr, sizeof(cStr), kCFStringEncodingUTF8)) {
            contextprintf(context, kBLLogLevelVerbose,
                               "Could not interpret NVRAM variable as UTF-8 string. Ignoring...\n");
            cStr[0] = '\0';
        }
    } else if(CFGetTypeID(valRef) == CFDataGetTypeID()) {
        const UInt8 *ptr = CFDataGetBytePtr(valRef);
        CFIndex len = CFDataGetLength(valRef);
        
        if(len > sizeof(cStr)-1)
            len = sizeof(cStr)-1;
        
        memcpy(cStr, (char *)ptr, len);
        cStr[len] = '\0';
        
    } else {
        contextprintf(context, kBLLogLevelError,  "Could not interpret NVRAM variable. Ignoring...\n");
        cStr[0] = '\0';
    }
    
    CFRelease(valRef);
    
    stringRef = CFStringCreateWithCString(kCFAllocatorDefault, cStr, kCFStringEncodingUTF8);
    if(stringRef == NULL) {
        return 2;
    }
    
    *value = stringRef;
    
    return 0;
}
