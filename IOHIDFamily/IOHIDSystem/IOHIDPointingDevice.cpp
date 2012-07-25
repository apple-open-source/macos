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

#include <IOKit/IOLib.h>

#include "IOHIDFamilyPrivate.h"
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

static Boolean CheckDeviceUsage(IOHIDDevice * device, UInt32 usagePage, UInt32 usage)
{
    OSDictionary * matchingDictionary = OSDictionary::withCapacity(2);
    Boolean ret = FALSE;
    
    if ( matchingDictionary ) 
    {
        OSNumber * number;
        
        number = OSNumber::withNumber(usagePage, 32);
        if ( number )
        {
            matchingDictionary->setObject(kIOHIDDeviceUsagePageKey, number);
            number->release();
        }
        
        number = OSNumber::withNumber(usage, 32);
        if ( number )
        {
            matchingDictionary->setObject(kIOHIDDeviceUsageKey, number);
            number->release();
        }
        
        ret = CompareDeviceUsage(device, matchingDictionary, NULL, 0);
        
        matchingDictionary->release();
    }
    
    return ret;
}

#define super IOHIDDeviceShim

OSDefineMetaClassAndStructors( IOHIDPointingDevice, IOHIDDeviceShim )


IOHIDPointingDevice * 
IOHIDPointingDevice::newPointingDeviceAndStart(IOService *owner, UInt8 numButtons, UInt32 resolution, bool scroll, UInt32 location)
{
    IOService * provider = owner;
    
    while ( NULL != (provider = provider->getProvider()) )
    {
        if(OSDynamicCast(IOHIDevice, provider) || 
            (OSDynamicCast(IOHIDDevice, provider) && CheckDeviceUsage((IOHIDDevice*)provider, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse)) )
        {
                return  0;
        }
    }

    IOHIDPointingDevice * device = new IOHIDPointingDevice;
    
    if (device)
    {
        if (!device->initWithLocation(location)){
            device->release();
            return 0;
        }
        device->_numButtons = numButtons;
        device->_resolution = resolution;
        device->_isScrollPresent = scroll;
        
        if ( device->attach(owner) )
        {
            if (!device->start(owner))
            {
                device->detach(owner);
                device->release();
                device = 0;
            }
        }
        else 
        {
            device->release();
            device = 0;
        }
    }
    
    return device;
}


bool IOHIDPointingDevice::initWithLocation( UInt32 location )
{
    if (!super::initWithLocation(location))
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
        
    _pointing = OSDynamicCast(IOHIPointing, provider);
    
    _report = IOBufferMemoryDescriptor::withCapacity(
        sizeof(GenericMouseReport), kIODirectionNone, true);
                                        
    bzero(_report->getBytesNoCopy(), sizeof(GenericMouseReport));
    
    return (_report) ? true : false;
}

IOReturn IOHIDPointingDevice::newReportDescriptor(
                        IOMemoryDescriptor ** descriptor ) const 
{
    void 	*desc;
    
    if (!descriptor)
        return kIOReturnBadArgument;

    *descriptor = IOBufferMemoryDescriptor::withCapacity( 
        sizeof(GenericMouseDescriptor),
        kIODirectionNone,
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
    
    GenericMouseDescriptor *mouse = (GenericMouseDescriptor *)desc;
   
    if ((_numButtons <= 8) &&
        (_numButtons != mouse->buttonRptCountNum))
    {
        mouse->buttonRptCountNum = _numButtons;
        mouse->buttonUsageMaxNum = _numButtons;
        mouse->fillCountNum = 8 - _numButtons;
    }

        
    if (_resolution && _resolution != 400)
    {
        convert16to8((unsigned short)-32767, mouse->xyLogMinNum);
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

IOReturn IOHIDPointingDevice::getReport(IOMemoryDescriptor  *report,
                                        IOHIDReportType     reportType,
                                        IOOptionBits        options __unused )
{
    if (!report)
        return kIOReturnError;

    if ( reportType != kIOHIDReportTypeInput)
        return kIOReturnUnsupported;
        
    report->writeBytes(0, _report->getBytesNoCopy(), min(report->getLength(), _report->getLength()));
    return kIOReturnSuccess;
}

void IOHIDPointingDevice::postMouseEvent(UInt8 buttons, UInt16 x, UInt16 y, UInt8 wheel)
{
    GenericMouseReport *report = (GenericMouseReport*)_report->getBytesNoCopy();
    
    if (!report)
        return;
        
    report->buttons = buttons;
    convert16to8(x, report->x);
    convert16to8(y, report->y);
    report->wheel = wheel;
    
    handleReport(_report);
}

OSString * IOHIDPointingDevice::newProductString() const
{
    OSString * string = 0;

    if ( !(string = super::newProductString()) )
        string = OSString::withCString("Virtual Mouse");
        
    return string;
}

