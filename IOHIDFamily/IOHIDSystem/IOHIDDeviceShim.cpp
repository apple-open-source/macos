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

#include <IOKit/IOLib.h>    // IOMalloc/IOFree
#include "IOHIDDeviceShim.h"

#define super IOHIDDevice

OSDefineMetaClassAndAbstractStructors( IOHIDDeviceShim, super )

bool IOHIDDeviceShim::initWithLocation(UInt32 location)
{
    if (!super::init())
        return false;
    
    _device 	= 0;
    _hiDevice 	= 0;
    _transport  = kIOHIDTransportNone;
    _location   = location;
    
    return true;
}

bool IOHIDDeviceShim::handleStart( IOService * provider )
{
    IOService *device = 0;
    
    if (!super::handleStart(provider))
        return false;

    if ((_hiDevice = OSDynamicCast(IOHIDevice, provider)))
    {
        if (_hiDevice->getProperty(kIOHIDVirtualHIDevice) == kOSBooleanTrue)
            return false;
            
        device = _hiDevice;
        do {
            if (_device = (IOService *)device->metaCast("IOUSBDevice"))
            {
                _transport = kIOHIDTransportUSB;
                break;
            }
                
            else if (_device = (IOService *)device->metaCast("IOADBDevice"))
            {
                _transport = kIOHIDTransportADB;
                break;
            }
            
            else if (_device = (IOService *)device->metaCast("ApplePS2Controller"))
            {
                _transport = kIOHIDTransportPS2;
                break;
            }
        } while (device = device->getProvider());
    }
                
    return true;
}

OSString * IOHIDDeviceShim::newProductString() const
{
    OSString * string;
    
    if (_device && (string = OSDynamicCast(OSString, _device->getProperty("USB Product Name"))))
        return OSString::withString(string);

    if (_hiDevice)
    {
        if (_hiDevice->hidKind() == kHIRelativePointingDevice)
        {
            if ((string = (OSString *)_hiDevice->getProperty(kIOHIDPointerAccelerationTypeKey)) &&
                (string->isEqualTo(kIOHIDTrackpadAccelerationType)))
            {
                return OSString::withCString("Trackpad");
            }
            return OSString::withCString("Mouse");
        }
        else if (_hiDevice->hidKind() == kHIKeyboardDevice)
        {
            return OSString::withCString("Keyboard");
        }
    }

    return 0;
}

OSString * IOHIDDeviceShim::newManufacturerString() const
{
    OSString * string;
    
    if (_device && (string = OSDynamicCast(OSString, _device->getProperty("USB Vendor Name"))))
        return OSString::withString(string);
        
    if (_hiDevice && (_hiDevice->deviceType() > 2))
        return OSString::withCString("Apple");

    return 0;
}

OSString * IOHIDDeviceShim::newTransportString() const
{
    switch (_transport)
    {
        case kIOHIDTransportUSB:
            return OSString::withCString("USB");

        case kIOHIDTransportADB:
            return OSString::withCString("ADB");

        case kIOHIDTransportPS2:
            return OSString::withCString("PS2");

        default:
            return 0;
    }        
}

OSNumber * IOHIDDeviceShim::newVendorIDNumber() const
{    
    OSNumber *  number;
    
    if (_device && (number = OSDynamicCast(OSNumber, _device->getProperty("idVendor"))))
        return OSNumber::withNumber(number->unsigned32BitValue(), 32);

    if (_hiDevice && (_hiDevice->deviceType() > 2))
    {
        UInt32	vendorID = kIOHIDAppleVendorID;
        return OSNumber::withNumber(vendorID, 32);
    }
    
    return 0;
}

OSNumber * IOHIDDeviceShim::newProductIDNumber() const
{
    OSNumber * number;
    
    if (_device && (number = OSDynamicCast(OSNumber, _device->getProperty("idProduct"))))
        return OSNumber::withNumber(number->unsigned32BitValue(), 32);

    return 0;
}

OSNumber * IOHIDDeviceShim::newLocationIDNumber() const
{
    OSNumber *  number;
    UInt32      location = _location;
    
    if (_device && !location)
    {
        if (number = OSDynamicCast(OSNumber, _device->getProperty("locationID")))
        {
            location = number->unsigned32BitValue();
        }
        else 
        {
            // Bullshit a location based on the ADB address and handler id        
            if (number = OSDynamicCast(OSNumber, _device->getProperty("address")))
                location |= number->unsigned8BitValue() << 24;
                
            if (number = OSDynamicCast(OSNumber, _device->getProperty("handler id")))
                location |= number->unsigned8BitValue() << 16;
        }
    }
    
    return (location) ? OSNumber::withNumber(location, 32) : 0;
}

OSString * IOHIDDeviceShim::newSerialNumberString() const
{
    OSNumber * number;
    char str[33];
    
    if (_device && (number = OSDynamicCast(OSNumber, _device->getProperty("iSerialNumber"))))
    {
        sprintf(str, "%d", number->unsigned32BitValue());
        str[32] = 0;
        return OSString::withCString(str);
    }

    return 0;
}
