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

OSDefineMetaClassAndAbstractStructors( IOHIDDeviceShim, IOHIDDevice )

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
			if ((_device = (IOService *)device->metaCast("IOHIDDevice")))
			{
				break;
			}

            if ((_device = (IOService *)device->metaCast("IOUSBDevice")) || (_device = (IOService *)device->metaCast("IOUSBInterface")))
            {
                _transport = kIOHIDTransportUSB;
                break;
            }

			if ( NULL != (_device = (IOService *)device->metaCast("IOADBDevice")) )
            {
                _transport = kIOHIDTransportADB;
                break;
            }

			if ( NULL != (_device = (IOService *)device->metaCast("ApplePS2Controller")) )
            {
                _transport = kIOHIDTransportPS2;
                break;
            }
        }
        while ( NULL != (device = device->getProvider()) );
    }

    return true;
}

OSString * IOHIDDeviceShim::newProductString() const
{
    OSString * string       = NULL;
    OSString * returnString = NULL;

    if (_device) {
        string = (OSString*)_device->copyProperty("USB Product Name");
        if (!OSDynamicCast(OSString,string))
            OSSafeReleaseNULL(string);
        if (!string) {
            string = (OSString*)_device->copyProperty(kIOHIDProductKey);
            if (!OSDynamicCast(OSString,string))
                OSSafeReleaseNULL(string);
        }
    }

    if (string) {
        returnString = OSString::withString(string);
    }
    else if (_hiDevice) {
        if (_hiDevice->hidKind() == kHIRelativePointingDevice) {
            string = (OSString*)_hiDevice->copyProperty(kIOHIDPointerAccelerationTypeKey);
            if (!OSDynamicCast(OSString,string))
                OSSafeReleaseNULL(string);
            if (string && string->isEqualTo(kIOHIDTrackpadAccelerationType)) {
                returnString = OSString::withCString("Trackpad");
            }
            else  {
                returnString = OSString::withCString("Mouse");
            }
        }
        else if (_hiDevice->hidKind() == kHIKeyboardDevice) {
            returnString = (_transport == kIOHIDTransportADB) ? OSString::withCString("Built-in keyboard") : OSString::withCString("Keyboard");
        }
    }

    if ( _hiDevice && returnString )
        _hiDevice->setProperty(kIOHIDProductKey, returnString);

    OSSafeReleaseNULL(string);
    return returnString;
}

OSString * IOHIDDeviceShim::newManufacturerString() const
{
    OSString * string       = NULL;
    OSString * returnString = NULL;

    if (_device) {
        string = (OSString*)_device->copyProperty("USB Vendor Name");
        if (!OSDynamicCast(OSString,string))
            OSSafeReleaseNULL(string);
        if (!string) {
            string = (OSString*)_device->copyProperty(kIOHIDManufacturerKey);
            if (!OSDynamicCast(OSString,string))
                OSSafeReleaseNULL(string);
        }
    }

    if (string) {
        returnString = OSString::withString(string);
    }
    else if (_hiDevice && (_hiDevice->deviceType() > 2)) {
        returnString = OSString::withCString("Apple");
    }
    OSSafeReleaseNULL(string);

    if ( _hiDevice && returnString )
        _hiDevice->setProperty(kIOHIDManufacturerKey, returnString);

    return returnString;
}

OSString * IOHIDDeviceShim::newTransportString() const
{
    OSString * returnString = NULL;

    switch (_transport)
    {
        case kIOHIDTransportUSB:
            returnString = OSString::withCString("USB");
			break;

        case kIOHIDTransportADB:
            returnString = OSString::withCString("ADB");
			break;

        case kIOHIDTransportPS2:
            returnString = OSString::withCString("PS2");
			break;

		default:
			returnString = _device ?
								(OSString*)_device->copyProperty(kIOHIDTransportKey) :
								NULL;
			break;
    }

    if ( _hiDevice && returnString )
        _hiDevice->setProperty(kIOHIDTransportKey, returnString);

    return returnString;
}

OSNumber * IOHIDDeviceShim::newVendorIDNumber() const
{
    OSNumber * number       = NULL;
    OSNumber * returnNumber = NULL;

    if (_device) {
        number = (OSNumber*)_device->copyProperty("idVendor");
        if (!OSDynamicCast(OSNumber, number))
            OSSafeReleaseNULL(number);
        if (!number) {
            number = (OSNumber*)_device->copyProperty(kIOHIDVendorIDKey);
            if (!OSDynamicCast(OSNumber, number))
                OSSafeReleaseNULL(number);
        }
    }

    if (number) {
        returnNumber = OSNumber::withNumber(number->unsigned32BitValue(), 32);
    }
    else if (_hiDevice && (_hiDevice->deviceType() > 2)) {
        UInt32	vendorID = kIOHIDAppleVendorID;
        returnNumber = OSNumber::withNumber(vendorID, 32);
    }
    OSSafeReleaseNULL(number);

    if ( _hiDevice && returnNumber )
        _hiDevice->setProperty(kIOHIDVendorIDKey, returnNumber);

    return returnNumber;
}

OSNumber * IOHIDDeviceShim::newProductIDNumber() const
{
    OSNumber * number       = NULL;
    OSNumber * returnNumber = NULL;

    if (_device) {
        number = (OSNumber*)_device->copyProperty("idProduct");
        if (!OSDynamicCast(OSNumber, number))
            OSSafeReleaseNULL(number);
        if (!number) {
            number = (OSNumber*)_device->copyProperty(kIOHIDProductIDKey);
            if (!OSDynamicCast(OSNumber, number))
                OSSafeReleaseNULL(number);
        }
    }

    if (number)
        returnNumber = OSNumber::withNumber(number->unsigned32BitValue(), 32);
    OSSafeReleaseNULL(number);

    if ( _hiDevice && returnNumber )
        _hiDevice->setProperty(kIOHIDProductIDKey, returnNumber);

    return returnNumber;
}

OSNumber * IOHIDDeviceShim::newLocationIDNumber() const
{
    UInt32      location = _location;

    if (_device && !location)
    {
        OSNumber *number = (OSNumber*)_device->copyProperty("locationID");
        if (!OSDynamicCast(OSNumber, number))
            OSSafeReleaseNULL(number);
        if (!number) {
            number = (OSNumber*)_device->copyProperty(kIOHIDLocationIDKey);
            if (!OSDynamicCast(OSNumber, number))
                OSSafeReleaseNULL(number);
        }

        if (number)
        {
            location = number->unsigned32BitValue();
        }
        else
        {
            // Make up a location based on the ADB address and handler id
            number = (OSNumber*)_device->copyProperty("address");
            if ( OSDynamicCast(OSNumber, number) )
                location |= number->unsigned8BitValue() << 24;
            OSSafeReleaseNULL(number);

            number = (OSNumber*)_device->copyProperty("handler id");
            if ( OSDynamicCast(OSNumber, number) )
                location |= number->unsigned8BitValue() << 16;
        }
        OSSafeReleaseNULL(number);
    }

    return (location) ? OSNumber::withNumber(location, 32) : 0;
}

OSString * IOHIDDeviceShim::newSerialNumberString() const
{
    OSNumber *number = NULL;
    OSString *string = NULL;
    char str[33];

    if (_device) {
        number = (OSNumber*)_device->copyProperty("iSerialNumber");
        if ( OSDynamicCast(OSNumber, number) ) {
            snprintf(str, sizeof (str), "%d", number->unsigned32BitValue());
            str[32] = 0;
            string = OSString::withCString(str);
        }
        else {
            string = (OSString*)_device->copyProperty(kIOHIDSerialNumberKey);
            if (!OSDynamicCast(OSString, string)) {
                OSSafeReleaseNULL(string);
            }
        }
        OSSafeReleaseNULL(number);
    }

    return string;
}

bool IOHIDDeviceShim::isSeized()
{
    return _reserved->seizedClient != NULL;
}

