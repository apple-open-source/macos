/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include "AppleUSBMergeNub.h"

#include <IOKit/usb/IOUSBController.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOService.h>

#include <libkern/c++/OSNumber.h>

#define super IOService

OSDefineMetaClassAndStructors(AppleUSBMergeNub, IOService)

// this is a special IOUSBDevice driver which will always fail to probe. However, the probe
// will have a side effect, which is that it merge a property dictionary into his provider's
// parent NUB in the gIOUSBPlane
IOService *AppleUSBMergeNub::probe(IOService *provider, SInt32 *score)
{
    const IORegistryPlane * usbPlane = getPlane(kIOUSBPlane);
    
    IOUSBDevice	*device = OSDynamicCast(IOUSBDevice, provider);
    if (device && usbPlane)
    {
        IOUSBNub *parentNub = OSDynamicCast(IOUSBNub, device->getParentEntry(usbPlane));

        if (parentNub)
        {
            OSDictionary *providerDict = (OSDictionary*)getProperty("IOProviderParentUSBNubMergeProperties");
            if (providerDict)
                    parentNub->getPropertyTable()->merge(providerDict);		// merge will verify that this really is a dictionary
        }
    }

    OSDictionary *providerDict = (OSDictionary*)getProperty("IOProviderMergeProperties");
    if (providerDict)
            provider->getPropertyTable()->merge(providerDict);		// merge will verify that this really is a dictionary

    return NULL;								// always fail the probe!
}
