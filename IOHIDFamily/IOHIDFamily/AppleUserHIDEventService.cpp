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
#include <stdatomic.h>

#define kIOHIDEventServiceDextEntitlement "com.apple.developer.driverkit.family.hid.eventservice"


#define HIDEventServiceLogFault(fmt, ...)   HIDLogFault("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDEventServiceLogError(fmt, ...)   HIDLogError("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDEventServiceLog(fmt, ...)        HIDLog("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDEventServiceLogInfo(fmt, ...)    HIDLogInfo("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDEventServiceLogDebug(fmt, ...)   HIDLogDebug("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)


enum {
    kAppleUserHIDEventServiceStateStarted     = 1,
    kAppleUserHIDEventServiceStateStopped     = 2,
    kAppleUserHIDEventServiceStateKRStart     = 4,
    kAppleUserHIDEventServiceStateDKStart     = 8
};

//===========================================================================
// AppleUserHIDEventService class
//===========================================================================

#define super IOHIDEventDriver

OSDefineMetaClassAndStructors( AppleUserHIDEventService, IOHIDEventDriver )

#define _elements   ivar->elements
#define _provider   ivar->provider
#define _state      ivar->state

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
    
    super::setProperty(kIOServiceDEXTEntitlementsKey, kIOHIDEventServiceDextEntitlement);
    
    super::setProperty(kIOHIDRegisterServiceKey, kOSBooleanFalse);

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
    bool            ok        = false;
    IOReturn        ret       = kIOReturnSuccess;
    IOHIDInterface *interface = NULL;
    
    bool dkStart = getProperty(kIOHIDDKStartKey) == kOSBooleanTrue;

    HIDEventServiceLog ("start (state:0x%x)", _state);

    bool krStart = ((atomic_fetch_or((_Atomic UInt32 *)&_state, kAppleUserHIDEventServiceStateKRStart) &  kAppleUserHIDEventServiceStateKRStart) == 0);

    if (dkStart &&
        (atomic_fetch_or((_Atomic UInt32 *)&_state, kAppleUserHIDEventServiceStateDKStart) &  kAppleUserHIDEventServiceStateDKStart)) {
        //attemt to call kernel multiple times
        HIDEventServiceLogError("Attempt to do kernel start for device multiple times");
        return kIOReturnError;
    }
    
    if (krStart) {
        interface = OSDynamicCast(IOHIDInterface, provider);
        require(interface, exit);
        
        _elements = interface->createMatchingElements();
        require(_elements, exit);
        
        _provider = interface;
    }
    
    if (!dkStart) {
        ret = Start(provider);
        require_noerr_action(ret, exit, HIDEventServiceLogError("IOUserHIDEventService::Start:0x%x\n", ret));
    } else if (krStart) {
        ok = super::start(provider);
        require_action(ok, exit, HIDEventServiceLogError("super::start:0x%x\n", ret));
    } else {
        return super::start(provider);
    }

    atomic_fetch_or((_Atomic UInt32 *)&_state, kAppleUserHIDEventServiceStateStarted);

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


void AppleUserHIDEventService::dispatchKeyboardEvent(AbsoluteTime                timeStamp,
                                                     UInt32                      usagePage,
                                                     UInt32                      usage,
                                                     UInt32                      value,
                                                     IOOptionBits                options)
{
    require_action (_state & kAppleUserHIDEventServiceStateStarted, exit, HIDEventServiceLogError("HID EventService not ready (state:0x%x)", _state));
    super::dispatchKeyboardEvent (timeStamp, usagePage, usage, value, options);
exit:
    return;
}



void AppleUserHIDEventService::dispatchScrollWheelEventWithFixed(AbsoluteTime               timeStamp,
                                                                IOFixed                     deltaAxis1,
                                                                IOFixed                     deltaAxis2,
                                                                IOFixed                     deltaAxis3,
                                                                IOOptionBits                options)
{
    require_action (_state & kAppleUserHIDEventServiceStateStarted, exit, HIDEventServiceLogError("HID EventService not ready (state:0x%x)", _state));
    super::dispatchScrollWheelEventWithFixed (timeStamp, deltaAxis1, deltaAxis2, deltaAxis3);
exit:
    return;
}

void AppleUserHIDEventService::dispatchEvent(IOHIDEvent * event, IOOptionBits options)
{
    require_action (_state & kAppleUserHIDEventServiceStateStarted, exit, HIDEventServiceLogError("HID EventService not ready (state:0x%x)", _state));
    super::dispatchEvent (event, options);
exit:
    return;
}
