/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/IOLib.h>

#include "IOHIDPointingDevice.h" 

typedef struct ScrollDescriptor {    
    //09 38:    Usage (Wheel)
    UInt8 wheelUsageOp;
    UInt8 wheelUsageNum;
    //15 81:    Logical Minimum.... (-127) 
    UInt8 wheelLogMinOp;
    UInt8 wheelLogMinNum;
    //25 7F:    Logical Maximum.... (127) 
    UInt8 wheelLogMaxOp;
    UInt8 wheelLogMaxNum;
    //35 00:    Physical Minimum.... (0) 
    UInt8 wheelPhyMinOp;
    UInt8 wheelPhyMinNum;
    //45 00:    Physical Maximum.... (0) 
    UInt8 wheelPhyMaxOp;
    UInt8 wheelPhyMaxNum;
    //55 00:    Unit Exponent.... (0) 
    UInt8 wheelUnitExpOp;
    UInt8 wheelUnitExpNum;
    //65 00:    Unit.... (0) 
    UInt8 wheelUnitOp;
    UInt8 wheelUnitNum;
    //75 08:    Report Size........ (8) 
    UInt8 wheelRptSizeOp;
    UInt8 wheelRptSizeNum;
    //95 01:    Report Count....... (1) 
    UInt8 wheelCountOp;
    UInt8 wheelCountNum;
    //81 06:    Input (Data)
    UInt8 wheelInputOp;
    UInt8 wheelInputNum;
}ScrollDescriptor;

typedef struct GenericMouseDescriptor {
    //05 01: Usage Page (Generic Desktop)
    UInt8 devUsagePageOp;
    UInt8 devUsagePageNum;
    //09 02: Usage (Mouse)
    UInt8 devUsageOp;
    UInt8 devUsageNum;
    //A1 01: Collection (Application)
    UInt8 appCollectionOp;
    UInt8 appCollectionNum;
    //05 09:    Usage Page (Button)
    UInt8 buttonUsagePageOp;
    UInt8 buttonUsagePageNum;
    //19 01:    Usage Minimum...... (1) 
    UInt8 buttonUsageMinOp;
    UInt8 buttonUsageMinNum;
    //29 08:    Usage Maximum...... (8) 
    UInt8 buttonUsageMaxOp;
    UInt8 buttonUsageMaxNum;
    //15 00:    Logical Minimum.... (0) 
    UInt8 buttonLogMinOp;
    UInt8 buttonLogMinNum;
    //25 01:    Logical Maximum.... (1) 
    UInt8 buttonLogMaxOp;
    UInt8 buttonLogMaxNum;
    //95 08:    Report Count....... (8) 
    UInt8 buttonRptCountOp;
    UInt8 buttonRptCountNum;
    //75 01:    Report Size........ (1) 
    UInt8 buttonRptSizeOp;
    UInt8 buttonRptSizeNum;
    //81 02:    Input (Data)
    UInt8 buttonInputOp;
    UInt8 buttonInputNum;
    //95 00:    Report Count....... (0) 
    UInt8 fillCountOp;
    UInt8 fillCountNum;
    //75 01:    Report Size........ (1) 
    UInt8 fillSizeOp;
    UInt8 fillSizeNum;
    //81 00:    Input (Constant)
    UInt8 fillInputOp;
    UInt8 fillInputNum;
    //05 01:    Usage Page (Generic Desktop)
    UInt8 pointerUsagePageOp;
    UInt8 pointerUsagePageNum;
    //09 01:    Usage (Pointer)
    UInt8 pointerUsageOp;
    UInt8 pointerUsageNum;
    //A1 00:    Collection (Physical)
    UInt8 pysCollectionOp;
    UInt8 pysCollectionNum;
    //09 30:    Usage (X)
    UInt8 xUsageOp;
    UInt8 xUsageNum;
    //09 31:    Usage (Y)
    UInt8 yUsageOp;
    UInt8 yUsageNum;
    //16 0:    Logical Minimum.... (0) 
    UInt8 xyLogMinOp;
    UInt8 xyLogMinNum[2];//
    //26 0:    Logical Maximum.... (0) 
    UInt8 xyLogMaxOp;
    UInt8 xyLogMaxNum[2];//
    //36 00:    Physical Minimum.... (0) 
    UInt8 xyPhyMinOp;
    UInt8 xyPhyMinNum[2];//
    //46 00:    Physical Maximum.... (0) 
    UInt8 xyPhyMaxOp;
    UInt8 xyPhyMaxNum[2];//
    //55 00:    Unit Exponent.... (0) 
    UInt8 xyUnitExpOp;
    UInt8 xyUnitExpNum;
    //65 00:    Unit.... (0) 
    UInt8 xyUnitOp;
    UInt8 xyUnitNum;
    //75 10:    Report Size........ (16) 
    UInt8 xyRptSizeOp;
    UInt8 xyRptSizeNum;
    //95 02:    Report Count....... (2) 
    UInt8 xyRptCountOp;
    UInt8 xyRptCountNum;
    //81 06:    Input (Data)
    UInt8 xyInputOp;
    UInt8 xyInputNum;
    //C0:       End Collection 
    UInt8 pysCollectionEnd;

    ScrollDescriptor scrollDescriptor;
    
    UInt8 appCollectionEnd;
} GenericMouseDescriptor;


typedef struct GenericMouseReport{
    UInt8 buttons;
    UInt8 x[2];
    UInt8 y[2];
    UInt8 wheel;
} GenericMouseReport;

static inline void convert16to8( const UInt16 src,
                           UInt8 * dst)
{
    dst[0] = 0x00ff & src;
    dst[1] = (0xff00 & src) >> 8;
}


#define super IOHIDDevice

OSDefineMetaClassAndStructors( IOHIDPointingDevice, IOHIDDevice )


IOHIDPointingDevice * 
IOHIDPointingDevice::newPointingDevice(UInt8 numButtons, UInt32 resolution, bool scroll)
{
    IOHIDPointingDevice * device = new IOHIDPointingDevice;
    
    if (device)
    {
        if (!device->init()){
            device->release();
            return 0;
        }
        device->_numButtons = numButtons;
        device->_resolution = resolution;
        device->_isScrollPresent = scroll;
    }
    
    return device;
}


bool IOHIDPointingDevice::init( OSDictionary * dictionary = 0 )
{
    if (!super::init(dictionary))
        return false;
        
    _report = 0;
    
    return true;
}

void IOHIDPointingDevice::free()
{
    if (_report) _report->release();
    
    super::free();
}

bool IOHIDPointingDevice::handleStart( IOService * provider )
{
    if (!super::handleStart(provider))
        return false;
        
    _provider = OSDynamicCast(IOHIPointing, provider);
    
    if (!_provider)
        return false;
            
    _report = IOBufferMemoryDescriptor::withCapacity(
        sizeof(GenericMouseReport), kIODirectionOutIn, true);
                                        
    bzero(_report->getBytesNoCopy(), sizeof(GenericMouseReport));
    
    return (_report) ? true : false;
}

OSNumber * IOHIDPointingDevice::newPrimaryUsageNumber() const
{
    return OSNumber::withNumber(kHIDUsage_GD_Mouse, 32);
}

OSNumber * IOHIDPointingDevice::newPrimaryUsagePageNumber() const
{
    return OSNumber::withNumber(kHIDPage_GenericDesktop, 32);
}

OSString * IOHIDPointingDevice::newProductString() const
{
    if (!_provider->getProvider() ||
        !_provider->getProvider()->getProvider() ||
        !_provider->getProvider()->getProvider()->getProperty("USB Product Name"))
        return OSString::withCString("Generic Mouse");

    return OSString::withString( 
        _provider->getProvider()->getProvider()->getProperty("USB Product Name"));
}

OSString * IOHIDPointingDevice::newManufacturerString() const
{
    if (!_provider->getProvider() ||
        !_provider->getProvider()->getProvider() ||
        !_provider->getProvider()->getProvider()->getProperty("USB Vendor Name"))
        return 0;

    return OSString::withString(
        _provider->getProvider()->getProvider()->getProperty("USB Vendor Name"));
}

OSString * IOHIDPointingDevice::newTransportString() const
{
    OSString * provStr = _provider->getProperty("IOProviderClass");
    
    if ( !provStr )
        return 0;
        
    if ( provStr->isEqualTo("IOADBDevice"))
        return OSString::withCString("ADB");
        
    else if ( provStr->isEqualTo("IOUSBInterface"))
        return OSString::withCString("USB");
        
    return 0;
}

OSNumber * IOHIDPointingDevice::newVendorIDNumber() const
{    
    if (!_provider->getProvider() ||
        !_provider->getProvider()->getProvider())
        return 0;
    
    OSNumber *num = 
        _provider->getProvider()->getProvider()->getProperty("idVendor");
    
    if (num)
        return OSNumber::withNumber(num->unsigned32BitValue(), 32);
        
    return 0;
}

OSNumber * IOHIDPointingDevice::newProductIDNumber() const
{
    if (!_provider->getProvider() ||
        !_provider->getProvider()->getProvider())
        return 0;

    OSNumber *num = 
        _provider->getProvider()->getProvider()->getProperty("idProduct");
    
    if (num)
        return OSNumber::withNumber(num->unsigned32BitValue(), 32);
        
    return 0;
}

OSNumber * IOHIDPointingDevice::newLocationIDNumber() const
{
    if (!_provider->getProvider() ||
        !_provider->getProvider()->getProvider())
        return 0;

    OSNumber *num = 
        _provider->getProvider()->getProvider()->getProperty("locationID");
    
    if (num)
        return OSNumber::withNumber(num->unsigned32BitValue(), 32);
        
    return 0;
}

OSString * IOHIDPointingDevice::newSerialNumberString() const
{
    if (!_provider->getProvider() ||
        !_provider->getProvider()->getProvider())
        return 0;

    OSNumber *num = 
        _provider->getProvider()->getProvider()->getProperty("iSerialNumber");
    
    char str[33];
    
    if (num)
    {
        sprintf(str, "%d", num->unsigned32BitValue());
        str[32] = 0;
        return OSString::withCString(str);
    }
        
    return 0;
}

IOReturn IOHIDPointingDevice::newReportDescriptor(
                        IOMemoryDescriptor ** descriptor ) const 
{
    void 	*desc;
    
    if (!descriptor)
        return kIOReturnBadArgument;

    *descriptor = IOBufferMemoryDescriptor::withCapacity( 
        sizeof(GenericMouseDescriptor),
        kIODirectionOutIn,
        true);
                                        
    if (! *descriptor)
        return kIOReturnNoMemory;
        
    desc = ((IOBufferMemoryDescriptor *)(*descriptor))->getBytesNoCopy();
    
    UInt8 genMouseDesc[] = {
        0x05, 0x01, 
        0x09, 0x02, 
        0xA1, 0x01, 
            // Button
            0x05, 0x09, 0x19, 0x01, 
            0x29, 0x08, 0x15, 0x00, 
            0x25, 0x01, 0x95, 0x08, 
            0x75, 0x01, 0x81, 0x02,

            // Constant
            0x95, 0x00, 0x75, 0x01, 
            0x81, 0x00,
            
            // Pointer
            0x05, 0x01, 0x09, 0x01, 
            0xA1, 0x00,
                0x09, 0x30, 
                0x09, 0x31, 
                
                // log min/max
                0x16, 0x01, 0x80,
                0x26, 0xff, 0x7f,
                
                // Phy min/max
                0x36, 0x00, 0x00,
                0x46, 0x00, 0x00,
                
                // Unit, Unit Exponent
                0x55, 0x00, 
                0x65, 0x00,
                
                0x75, 0x10, 0x95, 0x02,
                0x81, 0x06,
            0xC0,
            // Wheel Padding
            0x00, 0x00,
            0x00, 0x00,
            0x00, 0x00,
            0x00, 0x00,
            0x00, 0x00,
            0x00, 0x00,
            0x00, 0x00,
            0x00, 0x00,
            0x00, 0x00,
            0x00, 0x00,
        
        0xC0
    };

    bcopy(genMouseDesc, desc, sizeof(GenericMouseDescriptor));
    
    GenericMouseDescriptor *mouse = desc;
   
    if ((_numButtons <= 8) &&
        (_numButtons != mouse->buttonRptCountNum))
    {
        mouse->buttonRptCountNum = _numButtons;
        mouse->buttonUsageMaxNum = _numButtons;
        mouse->fillCountNum = 8 - _numButtons;
    }

        
    if (_resolution != 400)
    {
        convert16to8(-32767, mouse->xyLogMinNum);
        convert16to8(32767, mouse->xyLogMaxNum);
        
        UInt16 phys = 3276700 / _resolution;
        convert16to8(-phys, mouse->xyPhyMinNum);
        convert16to8(phys, mouse->xyPhyMaxNum);
        
        mouse->xyUnitNum = 0x13;
        mouse->xyUnitExpNum = 0x0e;
    }

    if (_isScrollPresent)
    {
        UInt8 scrollDes[] = {
            0x09, 0x38,
            0x15, 0x81, 
            0x25, 0x7f, 
            0x35, 0x00, 
            0x45, 0x00, 
            0x55, 0x00, 
            0x65, 0x00,
            0x75, 0x08, 
            0x95, 0x01,
            0x81, 0x06
        };
        
        bcopy(scrollDes, &mouse->scrollDescriptor, sizeof(ScrollDescriptor));
    }

    
    return kIOReturnSuccess;
}

void IOHIDPointingDevice::postMouseEvent(UInt8 buttons, UInt16 x, UInt16 y, UInt8 wheel)
{
    GenericMouseReport *report = _report->getBytesNoCopy();
    
    if (!report)
        return;
        
    report->buttons = buttons;
    convert16to8(x, report->x);
    convert16to8(y, report->y);
    report->wheel = wheel;
    
    handleReport(_report);
}
