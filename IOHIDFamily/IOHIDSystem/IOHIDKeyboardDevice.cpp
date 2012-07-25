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

extern unsigned int hid_adb_2_usb_keymap[];  //In Cosmo_USB2ADB.cpp

#define super IOHIDDeviceShim

OSDefineMetaClassAndStructors( IOHIDKeyboardDevice, IOHIDDeviceShim )


IOHIDKeyboardDevice * 
IOHIDKeyboardDevice::newKeyboardDeviceAndStart(IOService * owner, UInt32 location)
{
    IOService * provider = owner;
    
    while ( NULL != (provider = provider->getProvider()) )
    {
	if(OSDynamicCast(IOHIDDevice, provider) || OSDynamicCast(IOHIDevice, provider))
            return  0;
    }


    IOHIDKeyboardDevice * device = new IOHIDKeyboardDevice;
    
    if (device)
    {
        if ( device->initWithLocation(location) && device->attach(owner) )
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


bool IOHIDKeyboardDevice::initWithLocation( UInt32 location )
{
    if (!super::initWithLocation(location))
        return false;
        
    _report 		= 0;
    _cachedLEDState 	= 0;
    _inputReportOnly 	= true;
    
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
            
    if ( (_keyboard = OSDynamicCast(IOHIKeyboard, provider)) )
    {
        _inputReportOnly = ((transport() == kIOHIDTransportADB) && (_keyboard->deviceType() >= 0xc3));
        _cachedLEDState = _keyboard->getLEDStatus() & 0x3;
    }

    _report = IOBufferMemoryDescriptor::withCapacity(
        sizeof(GenericKeyboardRpt), kIODirectionNone, true);        
                                        
    bzero(_report->getBytesNoCopy(), sizeof(GenericKeyboardRpt));

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

    if (_inputReportOnly)
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

IOReturn IOHIDKeyboardDevice::getReport(IOMemoryDescriptor  *report,
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

IOReturn IOHIDKeyboardDevice::setReport(IOMemoryDescriptor * report,
                                        IOHIDReportType      reportType __unused,
                                        IOOptionBits         options )
{
    UInt8 	ledState;
    UInt8	mask;    

    if ((options & 0xff) || (_inputReportOnly) || !_keyboard)
        return kIOReturnError;

    report->readBytes( 0, (void *)&ledState, sizeof(UInt8) );
    
    mask = (1 << (kHIDUsage_LED_NumLock - 1));
    if ( (ledState & mask) && !(_cachedLEDState & mask) )
    {
        _keyboard->setNumLockFeedback(true);
    }
    else if ( !(ledState & mask) && (_cachedLEDState & mask) )
    {
        _keyboard->setNumLockFeedback(false);
    }
    
    mask = (1 << (kHIDUsage_LED_CapsLock - 1));
    if ( (ledState & mask) && !(_cachedLEDState & mask) )
    {
        _keyboard->setAlphaLockFeedback(true);
    }
    else if ( !(ledState & mask) && (_cachedLEDState & mask) )
    {
        _keyboard->setAlphaLockFeedback(false);
    }
    
    _cachedLEDState = ledState;
    
    return kIOReturnSuccess;
}

void IOHIDKeyboardDevice::setCapsLockLEDElement(bool state)
{
    UInt8	mask = (1 << (kHIDUsage_LED_CapsLock-1));
    
    if (_inputReportOnly)
        return;

    if (state)
        _cachedLEDState |=  mask;
        
    else
        _cachedLEDState &= ~mask;
        
    *(UInt8 *)(_report->getBytesNoCopy()) = _cachedLEDState;
    
    handleReport(_report, kIOHIDReportTypeOutput);
}

void IOHIDKeyboardDevice::setNumLockLEDElement(bool state)
{
    UInt8	mask = (1 << (kHIDUsage_LED_NumLock-1));

    if (_inputReportOnly)
        return;

    if (state)
        _cachedLEDState |= mask;
        
    else
        _cachedLEDState &= ~mask;

    *(UInt8 *)(_report->getBytesNoCopy()) = _cachedLEDState;
    
    handleReport(_report, kIOHIDReportTypeOutput);
}

#define SET_MODIFIER_BIT(bitField, key, down)	\
    if (down) {bitField |= (1 << (key - 0xe0));}	\
    else {bitField &= ~(1 << (key - 0xe0));}

void IOHIDKeyboardDevice::postKeyboardEvent(UInt8 key, bool keyDown)
{
    GenericKeyboardRpt *report = (GenericKeyboardRpt *)_report->getBytesNoCopy();
    UInt8		usbKey;
        
    if (!report)
        return;
        
    // Convert ADB scan code to USB
    if (! (usbKey = hid_adb_2_usb_keymap[key]))
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

enum {
    kUSB_LEFT_CONTROL_BIT = 0x01,
    kUSB_LEFT_SHIFT_BIT = 0x02,
    kUSB_LEFT_ALT_BIT = 0x04,
    kUSB_LEFT_FLOWER_BIT = 0x08,

    kUSB_RIGHT_CONTROL_BIT = 0x10,
    kUSB_RIGHT_SHIFT_BIT = 0x20,
    kUSB_RIGHT_ALT_BIT = 0x040,
    kUSB_RIGHT_FLOWER_BIT = 0x80
};

void IOHIDKeyboardDevice::postFlagKeyboardEvent(UInt32 flags)
{        
    GenericKeyboardRpt *report      = (GenericKeyboardRpt *)_report->getBytesNoCopy();
    UInt32              flagDelta   = (flags ^ _lastFlags);

    if (!flagDelta)
        return;

    report->modifiers = 0;
    _lastFlags = flags;
    
    if ( flagDelta & 0x0000ffff )
    {        
        if( flags & NX_DEVICELSHIFTKEYMASK )
            report->modifiers |= kUSB_LEFT_SHIFT_BIT;
        if( flags & NX_DEVICELCTLKEYMASK )
            report->modifiers |= kUSB_LEFT_CONTROL_BIT;
        if( flags & NX_DEVICELALTKEYMASK )
            report->modifiers |= kUSB_LEFT_ALT_BIT;
        if( flags & NX_DEVICELCMDKEYMASK )
            report->modifiers |= kUSB_LEFT_FLOWER_BIT;

        if( flags & NX_DEVICERSHIFTKEYMASK )
            report->modifiers |= kUSB_RIGHT_SHIFT_BIT;
        if( flags & NX_DEVICERCTLKEYMASK )
            report->modifiers |= kUSB_RIGHT_CONTROL_BIT;
        if( flags & NX_DEVICERALTKEYMASK )
            report->modifiers |= kUSB_RIGHT_ALT_BIT;
        if( flags & NX_DEVICERCMDKEYMASK )
            report->modifiers |= kUSB_RIGHT_FLOWER_BIT;    
    }
    else if ( flagDelta & 0xffff0000 )
    {
        if( flags & NX_SHIFTMASK )
            report->modifiers |= kUSB_LEFT_SHIFT_BIT;
        if( flags & NX_CONTROLMASK )
            report->modifiers |= kUSB_LEFT_CONTROL_BIT;
        if( flags & NX_ALTERNATEMASK )
            report->modifiers |= kUSB_LEFT_ALT_BIT;
        if( flags & NX_COMMANDMASK )
            report->modifiers |= kUSB_LEFT_FLOWER_BIT;
    }
    
    if ( flagDelta & NX_ALPHASHIFTMASK )
    {
        postKeyboardEvent(0x39, flags & NX_ALPHASHIFTMASK);
        return;
    }
        
    handleReport(_report);
}

OSString * IOHIDKeyboardDevice::newProductString() const
{
    OSString * string = 0;

    if ( !(string = super::newProductString()) )
        string = OSString::withCString("Virtual Keyboard");
        
    return string;
}
