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

#include "IOHIDKeyboardDevice.h" 

struct GenericKeyboardDescriptor {
    //05 01: Usage Page (Generic Desktop)
    UInt8 devUsagePageOp;
    UInt8 devUsagePageNum;
    //09 06: Usage (Keyboard)
    UInt8 devUsageOp;
    UInt8 devUsageNum;
    //A1 01: Collection (Application)
    UInt8 appCollectionOp;
    UInt8 appCollectionNum;
    //05 07:    Usage Page (Key Codes)
    UInt8 modUsagePageOp;
    UInt8 modUsagePageNum;
    //19 e0:    Usage Minimum...... (224) 
    UInt8 modUsageMinOp;
    UInt8 modUsageMinNum;
    //29 e7:    Usage Maximum...... (231) 
    UInt8 modUsageMaxOp;
    UInt8 modUsageMaxNum;
    //15 00:    Logical Minimum.... (0) 
    UInt8 modLogMinOp;
    UInt8 modLogMinNum;
    //25 01:    Logical Maximum.... (1) 
    UInt8 modLogMaxOp;
    UInt8 modLogMaxNum;
    //95 01:    Report Count....... (1) 
    UInt8 modRptCountOp;
    UInt8 modRptCountNum;
    //75 08:    Report Size........ (8) 
    UInt8 modRptSizeOp;
    UInt8 modRptSizeNum;
    //81 02:    Input (Data)
    UInt8 modInputOp;
    UInt8 modInputNum;

    //95 01:    Report Count....... (1) 
    UInt8 rsrvCountOp;
    UInt8 rsrvCountNum;
    //75 08:    Report Size........ (8) 
    UInt8 rsrvSizeOp;
    UInt8 rsrvSizeNum;
    //81 01:    Input (Constant)
    UInt8 rsrvInputOp;
    UInt8 rsrvInputNum;

/*
    //95 03:    Report Count....... (3) 
    UInt8 ledRptCountOp;
    UInt8 ledRptCountNum;
    //75 01:    Report Size........ (1) 
    UInt8 ledRptSizeOp;
    UInt8 ledRptSizeNum; 
    //05 08:    Usage Page (LEDs)
    UInt8 ledUsagePageOp;
    UInt8 ledUsagePageNum;
    //19 01:    Usage Minimum...... (1) 
    UInt8 ledUsageMinOp;
    UInt8 ledUsageMinNum;
    //29 03:    Usage Maximum...... (3) 
    UInt8 ledUsageMaxOp;
    UInt8 ledUsageMaxNum;
    //91 02:    Output (Data)
    UInt8 ledInputOp;
    UInt8 ledInputNum;
    
    //95 01:    Report Count....... (1) 
    UInt8 fillRptCountOp;
    UInt8 fillRptCountNum;
    //75 05:    Report Size........ (5) 
    UInt8 fillRptSizeOp;
    UInt8 fillRptSizeNum;
    //91 01:    Output (Constant)
    UInt8 fillInputOp;
    UInt8 fillInputNum;
*/

    //95 06:    Report Count....... (6) 
    UInt8 keyRptCountOp;
    UInt8 keyRptCountNum;
    //75 08:    Report Size........ (8) 
    UInt8 keyRptSizeOp;
    UInt8 keyRptSizeNum;
    //15 00:    Logical Minimum.... (0) 
    UInt8 keyLogMinOp;
    UInt8 keyLogMinNum;
    //26 ff 00:    Logical Maximum.... (255) 
    UInt8 keyLogMaxOp;
    UInt16 keyLogMaxNum;
    //05 07:    Usage Page (Key Codes)
    UInt8 keyUsagePageOp;
    UInt8 keyUsagePageNum;
    //19 00:    Usage Minimum...... (0) 
    UInt8 keyUsageMinOp;
    UInt8 keyUsageMinNum;
    //29 ff:    Usage Maximum...... (255) 
    UInt8 keyUsageMaxOp;
    UInt8 keyUsageMaxNum;
    //81 00:    Input (Array)
    UInt8 keyInputOp;
    UInt8 keyInputNum;

    //C0:    End Collection 
    UInt8 appCollectionEnd;
} GenericKeyboardDescriptor;

typedef struct GenericKeyboardRpt {
    UInt8 modifiers;
    UInt8 reserved;
    UInt8 keys[6];
} GenericKeyboardRpt;


#define super IOHIDDevice

OSDefineMetaClassAndStructors( IOHIDKeyboardDevice, IOHIDDevice )


IOHIDKeyboardDevice * 
IOHIDKeyboardDevice::newKeyboardDevice()
{
    IOHIDKeyboardDevice * device = new IOHIDKeyboardDevice;
    
    if (device && !device->init())
    {
        device->release();
        return 0;
    }
    
    return device;
}


bool IOHIDKeyboardDevice::init( OSDictionary * dictionary = 0 )
{
    if (!super::init(dictionary))
        return false;
        
    _report = 0;
    bzero(_adb2usb, sizeof(UInt8) * 256);
    
    _adb2usb[0x35] = 0x29;
    _adb2usb[0x7a] = 0x3a;
    _adb2usb[0x78] = 0x3b;
    _adb2usb[0x63] = 0x3c;
    _adb2usb[0x76] = 0x3d;
    _adb2usb[0x60] = 0x3e;
    _adb2usb[0x61] = 0x3f;
    _adb2usb[0x62] = 0x40;
    _adb2usb[0x64] = 0x41;
    _adb2usb[0x65] = 0x42;
    _adb2usb[0x6d] = 0x43;
    _adb2usb[0x67] = 0x44;
    _adb2usb[0x6f] = 0x45;
    _adb2usb[0x72] = 0x49;
    _adb2usb[0x73] = 0x4a;
    _adb2usb[0x74] = 0x4b;
    _adb2usb[0x79] = 0x4e;
    _adb2usb[0x32] = 0x35;
    _adb2usb[0x12] = 0x1e;
    _adb2usb[0x13] = 0x1f;
    _adb2usb[0x14] = 0x20;
    _adb2usb[0x15] = 0x21;
    _adb2usb[0x17] = 0x22;
    _adb2usb[0x16] = 0x23;
    _adb2usb[0x1a] = 0x24;
    _adb2usb[0x1c] = 0x25;
    _adb2usb[0x19] = 0x26;
    _adb2usb[0x1d] = 0x27;
    _adb2usb[0x1b] = 0x2d;
    _adb2usb[0x18] = 0x2e;
    _adb2usb[0x5d] = 0x89;
    _adb2usb[0x33] = 0x2a;
    _adb2usb[0x47] = 0x53;
    _adb2usb[0x51] = 0x67;
    _adb2usb[0x4b] = 0x54;
    
    _adb2usb[0x43] = 0x55;
    _adb2usb[0x30] = 0x2b;
    _adb2usb[0x0c] = 0x14;
    _adb2usb[0x0d] = 0x1a;
    _adb2usb[0x0e] = 0x08;
    _adb2usb[0x0f] = 0x15;
    _adb2usb[0x11] = 0x17;
    _adb2usb[0x10] = 0x1c;
    _adb2usb[0x20] = 0x18;
    _adb2usb[0x22] = 0x0c;
    _adb2usb[0x1f] = 0x12;
    _adb2usb[0x23] = 0x13;
    _adb2usb[0x21] = 0x2f;
    _adb2usb[0x1e] = 0x30;
    _adb2usb[0x2a] = 0x31;
    _adb2usb[0x59] = 0x5f;
    _adb2usb[0x5b] = 0x60;
    _adb2usb[0x5c] = 0x61;
    _adb2usb[0x4e] = 0x56;
    _adb2usb[0x39] = 0x39;
    _adb2usb[0x00] = 0x04;
    _adb2usb[0x01] = 0x16;
    _adb2usb[0x02] = 0x07;
    _adb2usb[0x03] = 0x09;
    _adb2usb[0x05] = 0x0a;
    _adb2usb[0x04] = 0x0b;
    _adb2usb[0x26] = 0x0d;
    _adb2usb[0x28] = 0x0e;
    _adb2usb[0x25] = 0x0f;
    _adb2usb[0x29] = 0x33;
    _adb2usb[0x27] = 0x34;
    _adb2usb[0x2a] = 0x32;
    _adb2usb[0x24] = 0x28;
    _adb2usb[0x56] = 0x5c;
    _adb2usb[0x57] = 0x5d;
    _adb2usb[0x58] = 0x5e;
    _adb2usb[0x45] = 0x57;
    _adb2usb[0x38] = 0xe1;
    _adb2usb[0x0a] = 0x64;
    _adb2usb[0x06] = 0x1d;
    _adb2usb[0x07] = 0x1b;
    _adb2usb[0x08] = 0x06;
    _adb2usb[0x09] = 0x19;
    _adb2usb[0x0b] = 0x05;
    _adb2usb[0x2d] = 0x11;
    _adb2usb[0x2e] = 0x10;
    
    _adb2usb[0x2b] = 0x36;
    _adb2usb[0x2f] = 0x37;
    _adb2usb[0x2c] = 0x38;
    _adb2usb[0x5e] = 0x87;
    _adb2usb[0x7b] = 0xe5;
    _adb2usb[0x53] = 0x59;
    _adb2usb[0x54] = 0x5a;
    _adb2usb[0x55] = 0x5b;
    _adb2usb[0x36] = 0xe0;
    _adb2usb[0x3a] = 0xe2;
    _adb2usb[0x37] = 0xe3;
    _adb2usb[0x66] = 0x91;
    _adb2usb[0x31] = 0x2c;
    _adb2usb[0x68] = 0x90;
    _adb2usb[0x7e] = 0xe7;
    _adb2usb[0x7c] = 0xe6;
    
    _adb2usb[0x3b] = 0xe0;  // spec say l. arrow, but Tibook has cntrl
    _adb2usb[0x3e] = 0x52;
    _adb2usb[0x3d] = 0x51;
    _adb2usb[0x3c] = 0x4f;
    _adb2usb[0x7b] = 0x50;
    _adb2usb[0x7e] = 0x52;
    _adb2usb[0x7d] = 0x51;
    _adb2usb[0x7c] = 0x4f;
    
    _adb2usb[0x52] = 0x62;
    _adb2usb[0x5f] = 0x85;
    _adb2usb[0x41] = 0x63;
    _adb2usb[0x4c] = 0x58;
    _adb2usb[0x69] = 0x46;
    _adb2usb[0x6b] = 0x47;
    _adb2usb[0x71] = 0x48;
    _adb2usb[0x75] = 0x4c;
    _adb2usb[0x77] = 0x4d;
    _adb2usb[0x7d] = 0xe4;
    _adb2usb[0x6a] = 0x58;

    return true;
}

void IOHIDKeyboardDevice::free()
{
    if (_report) _report->release();
    
    super::free();
}

bool IOHIDKeyboardDevice::handleStart( IOService * provider )
{
    if (!super::handleStart(provider))
        return false;
        
    _provider = OSDynamicCast(IOHIKeyboard, provider);
    
    if (!_provider)
        return false;
            
    _report = IOBufferMemoryDescriptor::withCapacity(
        sizeof(GenericKeyboardRpt), kIODirectionOutIn, true);
                                        
    bzero(_report->getBytesNoCopy(), sizeof(GenericKeyboardRpt));
    
    return (_report) ? true : false;
}

OSNumber * IOHIDKeyboardDevice::newPrimaryUsageNumber() const
{
    return OSNumber::withNumber(kHIDUsage_GD_Keyboard, 32);
}

OSNumber * IOHIDKeyboardDevice::newPrimaryUsagePageNumber() const
{
    return OSNumber::withNumber(kHIDPage_GenericDesktop, 32);
}

OSString * IOHIDKeyboardDevice::newProductString() const
{
    if (!_provider->getProvider() ||
        !_provider->getProvider()->getProvider() ||
        !_provider->getProvider()->getProvider()->getProperty("USB Product Name"))
        return OSString::withCString("Generic Keyboard");

    return OSString::withString( 
        _provider->getProvider()->getProvider()->getProperty("USB Product Name"));
}

OSString * IOHIDKeyboardDevice::newManufacturerString() const
{
    if (!_provider->getProvider() ||
        !_provider->getProvider()->getProvider() ||
        !_provider->getProvider()->getProvider()->getProperty("USB Vendor Name"))
        return 0;

    return OSString::withString(
        _provider->getProvider()->getProvider()->getProperty("USB Vendor Name"));
}

OSString * IOHIDKeyboardDevice::newTransportString() const
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

OSNumber * IOHIDKeyboardDevice::newVendorIDNumber() const
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

OSNumber * IOHIDKeyboardDevice::newProductIDNumber() const
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

OSNumber * IOHIDKeyboardDevice::newLocationIDNumber() const
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

OSString * IOHIDKeyboardDevice::newSerialNumberString() const
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

IOReturn IOHIDKeyboardDevice::newReportDescriptor(
                        IOMemoryDescriptor ** descriptor ) const 
{
    void *desc;
    
    if (!descriptor)
        return kIOReturnBadArgument;

    *descriptor = IOBufferMemoryDescriptor::withCapacity( 
        sizeof(GenericKeyboardDescriptor),
        kIODirectionOutIn,
        true);
                                        
    if (! *descriptor)
        return kIOReturnNoMemory;
        
    desc = ((IOBufferMemoryDescriptor *)(*descriptor))->getBytesNoCopy();
    
    UInt8 genKeyboardDesc[] = {
        0x05, 0x01,
        0x09, 0x06,
        0xA1, 0x01,
        0x05, 0x07,
        0x19, 0xe0,
        0x29, 0xe7, 
        0x15, 0x00, 
        0x25, 0x01, 
        0x75, 0x01,
        0x95, 0x08,
        0x81, 0x02,
        0x95, 0x01, 
        0x75, 0x08, 
        0x81, 0x01,
    /*    
        0x95, 0x03,
        0x75, 0x01,
        0x05, 0x08,
        0x19, 0x01,
        0x29, 0x03,
        0x91, 0x02,
        0x95, 0x01,
        0x75, 0x05,
        0x91, 0x01,
    */
        0x95, 0x06,
        0x75, 0x08,
        0x15, 0x00,
        0x26, 0xff, 0x00,
        0x05, 0x07,
        0x19, 0x00,
        0x29, 0xff,
        0x81, 0x00,
        0xC0
    }; 


    bcopy(genKeyboardDesc, desc, sizeof(GenericKeyboardDescriptor));

    
    return kIOReturnSuccess;
}

#define SET_MODIFIER_BIT(bitField, key, down)	\
    if (down) {bitField |= (1 << (key - 0xe0));}	\
    else {bitField &= ~(1 << (key - 0xe0));}

void IOHIDKeyboardDevice::postKeyboardEvent(UInt8 key, bool keyDown)
{
    GenericKeyboardRpt *report = _report->getBytesNoCopy();
    UInt8		usbKey;
        
    if (!report)
        return;
        
    // Convert ADB scan code to USB
    if (! (usbKey = _adb2usb[key]))
        return;
    
    // Check if modifier
    if ((usbKey >= 0xe0) && (usbKey <= 0xe7))
    {
	SET_MODIFIER_BIT(report->modifiers, usbKey, keyDown);
    }
    else
    {
        for (int i=0; i<6; i++)
        {                
            if (report->keys[i] == usbKey)
            {
                if (keyDown) return;
                    
                for (int j=i; j<5; j++)
                    report->keys[j] = report->keys[j+1];
                    
                report->keys[5] = 0;
                break;
            }
                
            else if ((report->keys[i] == 0) && keyDown)
            {
                report->keys[i] = usbKey;
                break;
            }
        }
    }
        
    handleReport(_report);
}
