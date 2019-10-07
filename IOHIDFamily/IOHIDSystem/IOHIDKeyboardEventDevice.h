/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2016 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _IOKIT_HID_IOHIDKEYBOARDEVENTDEVICE_H
#define _IOKIT_HID_IOHIDKEYBOARDEVENTDEVICE_H

#include "IOHIDDeviceShim.h"
#include "IOHIKeyboard.h"

class IOHIDKeyboardEventDevice : public IOHIDDeviceShim
{
    OSDeclareDefaultStructors( IOHIDKeyboardEventDevice )

private:
  
    IOBufferMemoryDescriptor *	_report;
    IOHIKeyboard *              _keyboard;
    UInt8                       _cachedLEDState;
    UInt32                      _lastFlags;
    bool                        _inputReportOnly;

    static  void _keyboardEvent (
                               IOHIDKeyboardEventDevice * self,
                               unsigned   eventType,
                               unsigned   flags,
                               unsigned   key,
                               unsigned   charCode,
                               unsigned   charSet,
                               unsigned   origCharCode,
                               unsigned   origCharSet,
                               unsigned   keyboardType,
                               bool       repeat,
                               AbsoluteTime ts,
                               OSObject * sender,
                               void *     refcon __unused);

    static  void _keyboardSpecialEvent(
                                IOHIDKeyboardEventDevice * self,
                                unsigned   eventType,
                                unsigned   flags,
                                unsigned   key,
                                unsigned   flavor,
                                UInt64     guid,
                                bool       repeat,
                                AbsoluteTime ts,
                                OSObject * sender,
                                void *     refcon __unused);

    static  void _updateEventFlags(
                                IOHIDKeyboardEventDevice * self,
                                unsigned      flags,
                                OSObject *    sender,
                                void *        refcon __unused);

protected:

    virtual void free(void) APPLE_KEXT_OVERRIDE;
  
    virtual bool handleStart( IOService * provider ) APPLE_KEXT_OVERRIDE;
    
public:
  
    virtual bool start( IOService * provider ) APPLE_KEXT_OVERRIDE;
  
    static IOHIDKeyboardEventDevice	* newKeyboardDeviceAndStart(IOService * owner, UInt32 location = 0);
    
    virtual bool initWithLocation( UInt32 location = 0 ) APPLE_KEXT_OVERRIDE;

    virtual IOReturn newReportDescriptor(IOMemoryDescriptor ** descriptor ) const APPLE_KEXT_OVERRIDE;
                        
    virtual OSString * newProductString() const APPLE_KEXT_OVERRIDE;
    
    virtual OSNumber * newVendorIDNumber() const APPLE_KEXT_OVERRIDE;
    virtual OSNumber * newProductIDNumber() const APPLE_KEXT_OVERRIDE;
    virtual OSString * newManufacturerString() const APPLE_KEXT_OVERRIDE;

    virtual IOReturn getReport( IOMemoryDescriptor * report,
                                 IOHIDReportType      reportType,
                                 IOOptionBits         options ) APPLE_KEXT_OVERRIDE;
                                 
    virtual IOReturn setReport( IOMemoryDescriptor * report,
                                IOHIDReportType      reportType,
                                IOOptionBits         options ) APPLE_KEXT_OVERRIDE;
                                                                
    virtual void postKeyboardEvent(UInt8 key, bool keyDown);
    virtual void postConsumerEvent(UInt8 key, bool keyDown);
    virtual void postFlagKeyboardEvent(UInt32 flags);
    
    virtual void setCapsLockLEDElement(bool state);
    virtual void setNumLockLEDElement(bool state);
  
    virtual IOReturn message(UInt32 type, IOService * provider, void * argument) APPLE_KEXT_OVERRIDE;
};

#endif /* !_IOKIT_HID_IOHIDKEYBOARDDEVICE_H */
