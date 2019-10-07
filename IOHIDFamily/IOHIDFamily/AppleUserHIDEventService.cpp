/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2018 Apple Computer, Inc.  All Rights Reserved.
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

#include "AppleUserHIDEventService.h"
#include "IOHIDPrivateKeys.h"
#include <AssertMacros.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "IOHIDDebug.h"
#include <IOKit/hid/AppleHIDUsageTables.h>
#include "IOHIDFamilyPrivate.h"
#include <IOKit/IOKitKeys.h>

#define kIOHIDEventServiceDextEntitlement "com.apple.developer.driverkit.family.hid.eventservice"

//===========================================================================
// AppleUserHIDEventService class
//===========================================================================

#define super IOHIDEventDriver

OSDefineMetaClassAndStructors( AppleUserHIDEventService, IOHIDEventDriver )

#define _elements   ivar->elements
#define _provider   ivar->provider

IOService *AppleUserHIDEventService::probe(IOService *provider, SInt32 *score)
{
    if (isSingleUser()) {
        return NULL;
    }
    
    return super::probe(provider, score);
}

bool AppleUserHIDEventService::init(OSDictionary *dict)
{
    if (!super::init(dict)) {
        return false;
    }
    
    ivar = IONew(AppleUserHIDEventService::AppleUserHIDEventService_IVars, 1);
    if (!ivar) {
        return false;
    }
    
    bzero(ivar, sizeof(AppleUserHIDEventService::AppleUserHIDEventService_IVars));
    
    setProperty(kIOServiceDEXTEntitlementsKey, kIOHIDEventServiceDextEntitlement);
    
    return true;
}

void AppleUserHIDEventService::free()
{
    if (ivar) {
        OSSafeReleaseNULL(_elements);
        IODelete(ivar, AppleUserHIDEventService::AppleUserHIDEventService_IVars, 1);
    }
    
    super::free();
}

//----------------------------------------------------------------------------------------------------
// IOUserHIDEventService::start
//----------------------------------------------------------------------------------------------------
bool AppleUserHIDEventService::start(IOService * provider)
{
    bool     ok = false;
    IOReturn ret;
    IOHIDInterface *interface = NULL;
    
    setProperty(kIOHIDRegisterServiceKey, kOSBooleanFalse);
    
    interface = OSDynamicCast(IOHIDInterface, provider);
    require(interface, exit);
    
    _elements = interface->createMatchingElements();
    require(_elements, exit);
    
    ret = Start(provider);
    if (kIOReturnSuccess != ret) {
        IOLog("IOUserHIDEventService start:0x%x\n", ret);
        return false;
    }
    
    _provider = interface;
    
    ok = true;
    
exit:
    return ok;
}


//----------------------------------------------------------------------------------------------------
// AppleUserHIDEventService::setProperties
//----------------------------------------------------------------------------------------------------
IOReturn AppleUserHIDEventService::setProperties(OSObject * properties)
{
    IOReturn result = kIOReturnBadArgument;
    
    // IOHIDEventService will push these properties to the
    // kIOHIDEventServicePropertiesKey dictionary.
    result = super::setProperties(properties);
    
    return result;
}

OSArray *AppleUserHIDEventService::getReportElements()
{
    return _elements;
}

IOReturn AppleUserHIDEventService::setElementValue(UInt32 usagePage,
                                                   UInt32 usage,
                                                   UInt32 value)
{
    IOReturn ret = kIOReturnUnsupported;
    
    if (usagePage == kHIDPage_LEDs) {
        SetLED(usage, value);
        ret = kIOReturnSuccess;
    }
    
    return ret;
}

bool AppleUserHIDEventService::terminate(IOOptionBits options)
{
    
    // This should be tracked by driverkit and we need not to explicitly call
    // close for provider as we earlier did as fix for 48119007
    
    return super::terminate(options);
}

bool AppleUserHIDEventService::handleStart(IOService *provider __unused)
{
    return true;
}

OSString *AppleUserHIDEventService::getTransport()
{
    return IOHIDEventService::getTransport();
}

OSString *AppleUserHIDEventService::getManufacturer()
{
    return IOHIDEventService::getManufacturer();
}

OSString *AppleUserHIDEventService::getProduct()
{
    return IOHIDEventService::getProduct();
}

OSString *AppleUserHIDEventService::getSerialNumber()
{
    return IOHIDEventService::getSerialNumber();
}

UInt32 AppleUserHIDEventService::getLocationID()
{
    return IOHIDEventService::getLocationID();
}

UInt32 AppleUserHIDEventService::getVendorID()
{
    return IOHIDEventService::getVendorID();
}

UInt32 AppleUserHIDEventService::getVendorIDSource()
{
    return IOHIDEventService::getVendorIDSource();
}

UInt32 AppleUserHIDEventService::getProductID()
{
    return IOHIDEventService::getProductID();
}

UInt32 AppleUserHIDEventService::getVersion()
{
    return IOHIDEventService::getVersion();
}

UInt32 AppleUserHIDEventService::getCountryCode()
{
    return IOHIDEventService::getCountryCode();
}
