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

#include "IOHIDKeyboardDevice.h" 

typedef struct GenericLEDKeyboardDescriptor {
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


    //95 02:    Report Count....... (2) 
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
    //29 02:    Usage Maximum...... (2) 
    UInt8 ledUsageMaxOp;
    UInt8 ledUsageMaxNum;
    //91 02:    Output (Data)
    UInt8 ledInputOp;
    UInt8 ledInputNum;
    
    //95 01:    Report Count....... (1) 
    UInt8 fillRptCountOp;
    UInt8 fillRptCountNum;
    //75 03:    Report Size........ (3) 
    UInt8 fillRptSizeOp;
    UInt8 fillRptSizeNum;
    //91 01:    Output (Constant)
    UInt8 fillInputOp;
    UInt8 fillInputNum;


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
} GenericLEDKeyboardDescriptor;

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


    //95 02:    Report Count....... (2) 
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
    //29 02:    Usage Maximum...... (2) 
    UInt8 ledUsageMaxOp;
    UInt8 ledUsageMaxNum;
    //91 02:    Output (Data)
    UInt8 ledInputOp;
    UInt8 ledInputNum;
    
    //95 01:    Report Count....... (1) 
    UInt8 fillRptCountOp;
    UInt8 fillRptCountNum;
    //75 03:    Report Size........ (3) 
    UInt8 fillRptSizeOp;
    UInt8 fillRptSizeNum;
    //91 01:    Output (Constant)
    UInt8 fillInputOp;
    UInt8 fillInputNum;


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

static UInt8 gGenLEDKeyboardDesc[] = {
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

    0x95, 0x02,
    0x75, 0x01,
    0x05, 0x08,
    0x19, 0x01,
    0x29, 0x02,
    0x91, 0x02,
    0x95, 0x01,
    0x75, 0x06,
    0x91, 0x01,

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

static UInt8 gGenKeyboardDesc[] = {
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

static unsigned char hid_adb_2_usb_keymap[] = 
{
	0x04,	// 0
	0x16,	// 1
	0x07,	// 2
	0x09,	// 3
	0x0b,	// 4
	0x0a,	// 5
	0x1d,	// 6
	0x1b,	// 7
	0x06,	// 8
	0x19,	// 9
	0x64,	// a
	0x05,	// b
	0x14,	// c
	0x1a,	// d
	0x08,	// e
	0x15,	// f
	0x1c,	// 10
	0x17,	// 11
	0x1e,	// 12
	0x1f,	// 13
	0x20,	// 14
	0x21,	// 15
	0x23,	// 16
	0x22,	// 17
	0x2e,	// 18
	0x26,	// 19
	0x24,	// 1a
	0x2d,	// 1b
	0x25,	// 1c
	0x27,	// 1d
	0x30,	// 1e
	0x12,	// 1f
	0x18,	// 20
	0x2f,	// 21
	0x0c,	// 22
	0x13,	// 23
	0x28,	// 24
	0x0f,	// 25
	0x0d,	// 26
	0x34,	// 27
	0x0e,	// 28
	0x33,	// 29
	0x31,	// 2a
	0x36,	// 2b
	0x38,	// 2c
	0x11,	// 2d
	0x10,	// 2e
	0x37,	// 2f
	0x2b,	// 30
	0x2c,	// 31
	0x35,	// 32
	0x2a,	// 33
	0x58,	// 34
	0x29,	// 35
	0xe7,	// 36
	0xe3,	// 37
	0xe1,	// 38
	0x39,	// 39
	0xe2,	// 3a
	0xe0,	// 3b
	0xe5,	// 3c
	0xe6,	// 3d
	0xe4,	// 3e
	0x00,	// 3f
	0x00,	// 40
	0x63,	// 41
	0x00,	// 42
	0x55,	// 43
	0x00,	// 44
	0x57,	// 45
	0x00,	// 46
	0x53,	// 47
	0x00,	// 48
	0x00,	// 49
	0x00,	// 4a
	0x54,	// 4b
	0x58,	// 4c
	0x00,	// 4d
	0x56,	// 4e
	0x00,	// 4f
	0x00,	// 50
	0x67,	// 51
	0x62,	// 52
	0x59,	// 53
	0x5a,	// 54
	0x5b,	// 55
	0x5c,	// 56
	0x5d,	// 57
	0x5e,	// 58
	0x5f,	// 59
	0x00,	// 5a
	0x60,	// 5b
	0x61,	// 5c
	0x89,	// 5d
	0x87,	// 5e
	0x85,	// 5f
	0x3e,	// 60
	0x3f,	// 61
	0x40,	// 62
	0x3c,	// 63
	0x41,	// 64
	0x42,	// 65
	0x91,	// 66
	0x44,	// 67
	0x90,	// 68
	0x68,	// 69
	0x6b,	// 6a
	0x69,	// 6b
	0x0,	// 6c
	0x43,	// 6d
	0x65,	// 6e  Microsoft Winodows95 key
	0x45,	// 6f
	0x0,	// 70
	0x6a,	// 71
	0x75,	// 72
	0x4a,	// 73
	0x4b,	// 74
	0x4c,	// 75
	0x3d,	// 76
	0x4d,	// 77
	0x3b,	// 78
	0x4e,	// 79
	0x3a,	// 7a
	0x50,	// 7b
	0x4f,	// 7c
	0x51,	// 7d
	0x52,	// 7e
	0x66,	// 7f
};

#define super IOHIDDeviceShim

OSDefineMetaClassAndStructors( IOHIDKeyboardDevice, super )


IOHIDKeyboardDevice * 
IOHIDKeyboardDevice::newKeyboardDevice(IOService * owner)
{
    IOService * provider = owner;
    
    while (provider = provider->getProvider())
    {
	if(OSDynamicCast(IOHIDDevice, provider) || OSDynamicCast(IOHIDevice, provider))
            return  0;
    }


    IOHIDKeyboardDevice * device = new IOHIDKeyboardDevice;
    
    if (device && !device->init())
    {
        device->release();
        return 0;
    }
    
    return device;
}


bool IOHIDKeyboardDevice::init( OSDictionary * dictionary )
{
    if (!super::init(dictionary))
        return false;
        
    _report 		= 0;
    _cachedLEDState 	= 0;
    _pmuControlledLED 	= false;
    
    bcopy(hid_adb_2_usb_keymap, _adb2usb, sizeof(hid_adb_2_usb_keymap));

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

    _pmuControlledLED = ((transport() == kIOHIDTransportADB) && (_provider->deviceType() >= 0xc3));

    _report = IOBufferMemoryDescriptor::withCapacity(
        sizeof(GenericKeyboardRpt), kIODirectionNone, true);        
                                        
    bzero(_report->getBytesNoCopy(), sizeof(GenericKeyboardRpt));

    _cachedLEDState = _provider->getLEDStatus() & 0x3;
    *(UInt8 *)(_report->getBytesNoCopy()) = _cachedLEDState;

    return (_report) ? true : false;
}

IOReturn IOHIDKeyboardDevice::newReportDescriptor(
                        IOMemoryDescriptor ** descriptor ) const 
{
    void  * desc;
    UInt8 * descBytes;
    UInt8 descSize;
    
    if (!descriptor)
        return kIOReturnBadArgument;

    if (_pmuControlledLED)
    {
        descSize = sizeof(GenericKeyboardDescriptor);
        descBytes = gGenKeyboardDesc;
    }
    else 
    {
        descSize = sizeof(GenericLEDKeyboardDescriptor);
        descBytes = gGenLEDKeyboardDesc;
    }

    *descriptor = IOBufferMemoryDescriptor::withCapacity( 
        descSize,
        kIODirectionNone,
        true);
                                        
    if (! *descriptor)
        return kIOReturnNoMemory;
        
    desc = ((IOBufferMemoryDescriptor *)(*descriptor))->getBytesNoCopy();
    bcopy(descBytes, desc, descSize);
    
    return kIOReturnSuccess;
}

IOReturn IOHIDKeyboardDevice::getReport( IOMemoryDescriptor * report,
                                 IOHIDReportType      reportType,
                                 IOOptionBits         options )
{
    if (!report)
        return kIOReturnError;

    if ( reportType != kIOHIDReportTypeInput)
        return kIOReturnUnsupported;
        
    report->writeBytes(0, _report->getBytesNoCopy(), min(report->getLength(), _report->getLength()));
    return kIOReturnSuccess;
}

IOReturn IOHIDKeyboardDevice::setReport(IOMemoryDescriptor * report,
                                        IOHIDReportType      reportType,
                                        IOOptionBits         options )
{
    UInt8 	ledState;
    UInt8	mask;    

    if ((options & 0xff) || (_pmuControlledLED))
        return kIOReturnError;

    report->readBytes( 0, (void *)&ledState, sizeof(UInt8) );
    
    mask = (1 << (kHIDUsage_LED_NumLock - 1));
    if ( (ledState & mask) && !(_cachedLEDState & mask) )
    {
        _provider->setNumLockFeedback(true);
    }
    else if ( !(ledState & mask) && (_cachedLEDState & mask) )
    {
        _provider->setNumLockFeedback(false);
    }
    
    mask = (1 << (kHIDUsage_LED_CapsLock - 1));
    if ( (ledState & mask) && !(_cachedLEDState & mask) )
    {
        _provider->setAlphaLockFeedback(true);
    }
    else if ( !(ledState & mask) && (_cachedLEDState & mask) )
    {
        _provider->setAlphaLockFeedback(false);
    }
    
    _cachedLEDState = ledState;
    
    return kIOReturnSuccess;
}

void IOHIDKeyboardDevice::setCapsLockLEDElement(bool state)
{
    UInt8	mask = (1 << (kHIDUsage_LED_CapsLock-1));
    
    if (_pmuControlledLED)
        return;

    if (state)
        _cachedLEDState |=  mask;
        
    else
        _cachedLEDState &= ~mask;
        
    *(UInt8 *)(_report->getBytesNoCopy()) = _cachedLEDState;
    
    handleReport(_report);
}

void IOHIDKeyboardDevice::setNumLockLEDElement(bool state)
{
    UInt8	mask = (1 << (kHIDUsage_LED_NumLock-1));

    if (_pmuControlledLED)
        return;

    if (state)
        _cachedLEDState |= mask;
        
    else
        _cachedLEDState &= ~mask;

    *(UInt8 *)(_report->getBytesNoCopy()) = _cachedLEDState;
    
    handleReport(_report);
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
