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

#ifndef _IOKIT_HID_IOHIDKEYBOARDDEVICE_H
#define _IOKIT_HID_IOHIDKEYBOARDDEVICE_H

#include "IOHIDDevice.h"
#include "IOHIKeyboard.h"

class IOHIDKeyboardDevice : public IOHIDDevice
{
    OSDeclareDefaultStructors( IOHIDKeyboardDevice )

private:
    IOBufferMemoryDescriptor *	_report;
    IOHIKeyboard *		_provider;
    
    UInt8 _adb2usb[256];

protected:

    virtual void free();
    virtual bool handleStart( IOService * provider );
    
public:
    static IOHIDKeyboardDevice	* newKeyboardDevice();
    
    virtual bool init( OSDictionary * dictionary = 0 );
    virtual OSString * newTransportString() const;
    virtual OSString * newProductString() const;
    virtual OSString * newManufacturerString() const;
    virtual OSNumber * newVendorIDNumber() const;
    virtual OSNumber * newProductIDNumber() const;
    virtual OSNumber * newLocationIDNumber() const;
    virtual OSString * newSerialNumberString() const;

    virtual OSNumber * newPrimaryUsageNumber() const;
    virtual OSNumber * newPrimaryUsagePageNumber() const;
    virtual IOReturn newReportDescriptor(
                        IOMemoryDescriptor ** descriptor ) const;
                                                                
    virtual void postKeyboardEvent(UInt8 key, bool keyDown);
};

#endif /* !_IOKIT_HID_IOHIDKEYBOARDDEVICE_H */
