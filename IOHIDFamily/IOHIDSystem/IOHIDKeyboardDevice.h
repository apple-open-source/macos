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

#ifndef _IOKIT_HID_IOHIDKEYBOARDDEVICE_H
#define _IOKIT_HID_IOHIDKEYBOARDDEVICE_H

#include "IOHIDDeviceShim.h"
#include "IOHIKeyboard.h"

class IOHIDKeyboardDevice : public IOHIDDeviceShim
{
    OSDeclareDefaultStructors( IOHIDKeyboardDevice )

private:
    IOBufferMemoryDescriptor *	_report;
    IOHIKeyboard *		_keyboard;
    
    UInt8			_cachedLEDState;
    UInt32          _lastFlags;
    
    bool			_inputReportOnly;

protected:

    virtual void free();
    virtual bool handleStart( IOService * provider );
    
public:
    static IOHIDKeyboardDevice	* newKeyboardDeviceAndStart(IOService * owner, UInt32 location = 0);
    
    virtual bool initWithLocation( UInt32 location = 0 );

    virtual IOReturn newReportDescriptor(
                        IOMemoryDescriptor ** descriptor ) const;
                        
    virtual OSString * newProductString() const;

    virtual IOReturn getReport( IOMemoryDescriptor * report,
                                 IOHIDReportType      reportType,
                                 IOOptionBits         options );
                                 
    virtual IOReturn setReport( IOMemoryDescriptor * report,
                                IOHIDReportType      reportType,
                                IOOptionBits         options );
                                                                
    virtual void postKeyboardEvent(UInt8 key, bool keyDown);
    virtual void postFlagKeyboardEvent(UInt32 flags);
    
    virtual void setCapsLockLEDElement(bool state);
    virtual void setNumLockLEDElement(bool state);
  
    virtual bool matchPropertyTable(
                                    OSDictionary *              table,
                                    SInt32 *                    score);
  
};

#endif /* !_IOKIT_HID_IOHIDKEYBOARDDEVICE_H */
