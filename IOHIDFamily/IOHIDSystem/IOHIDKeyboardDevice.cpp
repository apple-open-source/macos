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

typedef struct __attribute__((packed)) GenericKeyboardRpt {
    UInt8 modifiers;
    UInt8 reserved;
    UInt8 keys[6];
} GenericKeyboardRpt;

static UInt8 gGenLEDKeyboardDesc[] = {
    0x05, 0x01,                               // Usage Page (Generic Desktop)
    0x09, 0x06,                               // Usage (Keyboard)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x07,                               //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,                               //   Usage Minimum........... (224)
    0x29, 0xE7,                               //   Usage Maximum........... (231)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x75, 0x01,                               //   Report Size............. (1)
    0x95, 0x08,                               //   Report Count............ (8)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x95, 0x01,                               //   Report Count............ (1)
    0x75, 0x08,                               //   Report Size............. (8)
    0x81, 0x01,                               //   Input...................(Constant)
    0x95, 0x02,                               //   Report Count............ (2)
    0x75, 0x01,                               //   Report Size............. (1)
    0x05, 0x08,                               //   Usage Page (LED)
    0x19, 0x01,                               //   Usage Minimum........... (1)
    0x29, 0x02,                               //   Usage Maximum........... (2)
    0x91, 0x02,                               //   Output..................(Data, Variable, Absolute)
    0x95, 0x01,                               //   Report Count............ (1)
    0x75, 0x06,                               //   Report Size............. (6)
    0x91, 0x01,                               //   Output..................(Constant)
    0x95, 0x06,                               //   Report Count............ (6)
    0x75, 0x08,                               //   Report Size............. (8)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                         //   Logical Maximum......... (255)
    0x05, 0x07,                               //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,                               //   Usage Minimum........... (0)
    0x29, 0xFF,                               //   Usage Maximum........... (255)
    0x81, 0x00,                               //   Input...................(Data, Array, Absolute)
    0xC0,                                     // End Collection  
};

static UInt8 gGenKeyboardDesc[] = {
    0x05, 0x01,                               // Usage Page (Generic Desktop)
    0x09, 0x06,                               // Usage (Keyboard)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x07,                               //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,                               //   Usage Minimum........... (224)
    0x29, 0xE7,                               //   Usage Maximum........... (231)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x75, 0x01,                               //   Report Size............. (1)
    0x95, 0x08,                               //   Report Count............ (8)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x95, 0x01,                               //   Report Count............ (1)
    0x75, 0x08,                               //   Report Size............. (8)
    0x81, 0x01,                               //   Input...................(Constant)
    0x95, 0x06,                               //   Report Count............ (6)
    0x75, 0x08,                               //   Report Size............. (8)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                         //   Logical Maximum......... (255)
    0x05, 0x07,                               //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,                               //   Usage Minimum........... (0)
    0x29, 0xFF,                               //   Usage Maximum........... (255)
    0x81, 0x00,                               //   Input...................(Data, Array, Absolute)
    0xC0,                                     // End Collection  
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
        descSize = sizeof(gGenKeyboardDesc);
        descBytes = gGenKeyboardDesc;
    }
    else 
    {
        descSize = sizeof(gGenLEDKeyboardDesc);
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
