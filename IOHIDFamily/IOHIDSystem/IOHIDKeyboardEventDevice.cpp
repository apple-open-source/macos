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

#include "IOHIDKeyboardEventDevice.h"
#include "IOHIKeyboard.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDDebug.h"
#include "ev_keymap.h"
#include "IOHIDUsageTables.h"

typedef struct __attribute__((packed)) GenericKeyboardRpt {
    UInt8 modifiers;
    UInt8 reserved;
    UInt8 keys[6];
    UInt8 consumerKeys[6];
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
    0x05, 0x0c,                               //   Usage Page (Consumer)
    0x19, 0x00,                               //   Usage Minimum........... (0)
    0x2A, 0xFF, 0x00,                         //   Usage Maximum........... (255)
    0x95, 0x06,                               //   Report Count............ (6)
    0x75, 0x08,                               //   Report Size............. (8)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                         //   Logical Maximum......... (255)
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
    0x05, 0x0c,                               //   Usage Page (Consumer)
    0x19, 0x00,                               //   Usage Minimum........... (0)
    0x2A, 0xFF, 0x00,                         //   Usage Maximum........... (255)
    0x95, 0x05,                               //   Report Count............ (5)
    0x75, 0x08,                               //   Report Size............. (8)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                         //   Logical Maximum......... (255)
    0x81, 0x00,                               //   Input...................(Data, Array, Absolute)
    0xC0,                                     // End Collection
};

extern unsigned int hid_adb_2_usb_keymap[];  //In Cosmo_USB2ADB.cpp
extern unsigned int hid_adb_2_usb_keymap_length;

#define super IOHIDDeviceShim

OSDefineMetaClassAndStructors( IOHIDKeyboardEventDevice, IOHIDDeviceShim )

IOHIDKeyboardEventDevice	* IOHIDKeyboardEventDevice::newKeyboardDeviceAndStart(IOService * owner, UInt32 location)
{
  
    IOHIDKeyboardEventDevice * device = new IOHIDKeyboardEventDevice;
    
    if (device)
    {
        if ( device->initWithParameters(location, true) && device->attach(owner) )
        {
          
            device->setProperty("HIDDefaultBehavior", kOSBooleanTrue);
            device->setProperty(kIOHIDCompatibilityInterface, kOSBooleanTrue);
          
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


bool IOHIDKeyboardEventDevice::initWithLocation( UInt32 location )
{
    if (!super::initWithLocation(location))
        return false;
        
    _report 		= 0;
    _cachedLEDState 	= 0;
    _inputReportOnly 	= true;
    
    return true;
}

void IOHIDKeyboardEventDevice::free()
{
    if (_report) _report->release();
    
    super::free();
}

bool IOHIDKeyboardEventDevice::handleStart( IOService * provider )
{
    if (!super::handleStart(provider)) {
        return false;
    }
  
    if ((_keyboard = OSDynamicCast(IOHIKeyboard, provider))) {
        _inputReportOnly = ((transport() == kIOHIDTransportADB) && (_keyboard->deviceType() >= 0xc3));
        _cachedLEDState = _keyboard->getLEDStatus() & 0x3;
    }

    _report = IOBufferMemoryDescriptor::withCapacity(
        sizeof(GenericKeyboardRpt), kIODirectionNone, true);        
  
    if (_report) {
        bzero(_report->getBytesNoCopy(), sizeof(GenericKeyboardRpt));
    }
    return (_report) ? true : false;
}

bool IOHIDKeyboardEventDevice::start( IOService * provider ) {
  bool success = false;
  
  if ( !super::start(provider) ) {
    HIDLogError ("failed");
    return false;
  }
  success = ((IOHIKeyboard*)provider)->open(
                                            this,
                                            kIOServiceSeize,
                                            0,
                                            (KeyboardEventCallback)        _keyboardEvent,
                                            (KeyboardSpecialEventCallback) _keyboardSpecialEvent,
                                            (UpdateEventFlagsCallback)     _updateEventFlags
                                            );
  provider->setProperty(kIOHIDResetPointerKey, kOSBooleanTrue);
  return success;
}

IOReturn IOHIDKeyboardEventDevice::newReportDescriptor(IOMemoryDescriptor ** descriptor ) const
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

IOReturn IOHIDKeyboardEventDevice::getReport(IOMemoryDescriptor  *report,
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

IOReturn IOHIDKeyboardEventDevice::setReport(IOMemoryDescriptor * report,
                                        IOHIDReportType      reportType __unused,
                                        IOOptionBits         options )
{
    UInt8 	ledState = 0;
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

void IOHIDKeyboardEventDevice::setCapsLockLEDElement(bool state)
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

void IOHIDKeyboardEventDevice::setNumLockLEDElement(bool state)
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

void IOHIDKeyboardEventDevice::postKeyboardEvent(UInt8 key, bool keyDown)
{
    GenericKeyboardRpt *report = (GenericKeyboardRpt *)_report->getBytesNoCopy();
    UInt8		usbKey;
        
    if (!report)
        return;
  
    if (key == 0x90 || key == 0x91) {
        postConsumerEvent(key - 0x90 + 1, keyDown);
        return;
    }
    
    // Convert ADB scan code to USB
    if (key >= hid_adb_2_usb_keymap_length || !(usbKey = hid_adb_2_usb_keymap[key]))
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

void IOHIDKeyboardEventDevice::postConsumerEvent(UInt8 key, bool keyDown)
{
    GenericKeyboardRpt *report = (GenericKeyboardRpt *)_report->getBytesNoCopy();
    UInt8		usbKey;
    switch (key) {
      case NX_KEYTYPE_BRIGHTNESS_UP:
          usbKey = kHIDUsage_Csmr_DisplayBrightnessIncrement;
          break;
      case NX_KEYTYPE_BRIGHTNESS_DOWN:
          usbKey = kHIDUsage_Csmr_DisplayBrightnessDecrement;
          break;
      case NX_KEYTYPE_SOUND_UP:
          usbKey = kHIDUsage_Csmr_VolumeIncrement;
          break;
      case NX_KEYTYPE_SOUND_DOWN:
          usbKey = kHIDUsage_Csmr_VolumeDecrement;
          break;
      case NX_KEYTYPE_EJECT:
          usbKey = kHIDUsage_Csmr_Eject;
          break;
      case NX_POWER_KEY:
          usbKey = kHIDUsage_Csmr_Power;
          break;
      case NX_KEYTYPE_MUTE:
          usbKey = kHIDUsage_Csmr_Mute;
          break;
      case NX_KEYTYPE_PLAY:
          usbKey = kHIDUsage_Csmr_Play;
          break;
      case NX_KEYTYPE_NEXT:
          usbKey = kHIDUsage_Csmr_ScanNextTrack;
          break;
      case NX_KEYTYPE_PREVIOUS:
          usbKey = kHIDUsage_Csmr_ScanPreviousTrack;
          break;
      case NX_KEYTYPE_FAST:
          usbKey = kHIDUsage_Csmr_FastForward;
          break;
      case NX_KEYTYPE_REWIND:
          usbKey = kHIDUsage_Csmr_Rewind;
          break;
      default:
          return;
    }

    for (unsigned int i=0; i< (sizeof(report->consumerKeys)); i++)
    {                
        if (report->consumerKeys[i] == usbKey)
        {
            if (keyDown) return;
                
            for (unsigned int j=i; j<(sizeof(report->consumerKeys) - 1); j++)
                report->consumerKeys[j] = report->consumerKeys[j+1];
                
            report->consumerKeys[sizeof(report->consumerKeys) - 1] = 0;
            break;
        }
            
        else if ((report->consumerKeys[i] == 0) && keyDown)
        {
            report->consumerKeys[i] = usbKey;
            break;
        }
    }
    handleReport(_report);
}

OSNumber * IOHIDKeyboardEventDevice::newVendorIDNumber() const
{
    return OSNumber::withNumber(0x5ac, 32);
}

OSNumber * IOHIDKeyboardEventDevice::newProductIDNumber() const
{
    return OSNumber::withNumber(0xffff, 32);
}

OSString * IOHIDKeyboardEventDevice::newManufacturerString() const
{
    return OSString::withCString("Apple");
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

void IOHIDKeyboardEventDevice::postFlagKeyboardEvent(UInt32 flags)
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

OSString * IOHIDKeyboardEventDevice::newProductString() const
{
    OSString * string = 0;

    if ( !(string = super::newProductString()) )
        string = OSString::withCString("Virtual Keyboard");
        
    return string;
}


IOReturn IOHIDKeyboardEventDevice::message(UInt32 type, IOService * provider, void * argument __unused)
{
  IOReturn     status = kIOReturnSuccess;
  
  switch (type) {
      case kIOMessageServiceIsTerminated:
          if (provider) {
              provider->close( this );
          }
          break;
  }
  return status;
}

void IOHIDKeyboardEventDevice::_keyboardEvent (
                                 IOHIDKeyboardEventDevice * self,
                                 unsigned   eventType,
                                 unsigned   flags,
                                 unsigned   key,
                                 unsigned   charCode __unused,
                                 unsigned   charSet __unused,
                                 unsigned   origCharCode __unused,
                                 unsigned   origCharSet __unused,
                                 unsigned   keyboardType __unused,
                                 bool       repeat,
                                 AbsoluteTime ts __unused,
                                 OSObject * sender __unused,
                                 void *     refcon __unused)
{
  if (repeat) {
      return;
  }
  switch (eventType) {
      case NX_KEYDOWN:
      case NX_KEYUP:
          self->postKeyboardEvent(key, eventType == NX_KEYDOWN);
          break;
      case NX_FLAGSCHANGED:
          self->postFlagKeyboardEvent(flags);
          break;
  }
}

void IOHIDKeyboardEventDevice::_keyboardSpecialEvent(
                                IOHIDKeyboardEventDevice * self,
                                unsigned   eventType,
                                unsigned   flags __unused,
                                unsigned   key __unused,
                                unsigned   flavor,
                                UInt64     guid __unused,
                                bool       repeat __unused,
                                AbsoluteTime ts __unused,
                                OSObject * sender __unused,
                                void *     refcon __unused)
{

  switch (eventType) {
      case NX_KEYDOWN:
      case NX_KEYUP:
          self->postConsumerEvent(flavor, eventType == NX_KEYDOWN);
          break;
  }
}

void IOHIDKeyboardEventDevice::_updateEventFlags(
                                    IOHIDKeyboardEventDevice * self,
                                    unsigned      flags,
                                    OSObject *    sender __unused,
                                    void *        refcon __unused)
{
    self->postFlagKeyboardEvent(flags);
}
