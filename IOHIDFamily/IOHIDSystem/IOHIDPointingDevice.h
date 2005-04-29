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

#ifndef _IOKIT_HID_IOHIDPOINTINGDEVICE_H
#define _IOKIT_HID_IOHIDPOINTINGDEVICE_H

#include "IOHIDDeviceShim.h"
#include "IOHIPointing.h"

class IOHIDPointingDevice : public IOHIDDeviceShim
{
    OSDeclareDefaultStructors( IOHIDPointingDevice )

private:
    bool			_isScrollPresent;
    UInt8			_numButtons;
    UInt32			_resolution;
    IOBufferMemoryDescriptor *	_report;
    IOHIPointing *		_pointing;

protected:

    virtual void free();
    virtual bool handleStart( IOService * provider );
    
public:
    static IOHIDPointingDevice	* newPointingDeviceAndStart(IOService * owner, UInt8 numButtons = 8, UInt32 resolution = 100, bool scroll = false, UInt32 location = 0);
    
    virtual bool initWithLocation( UInt32 location = 0 );

    virtual IOReturn newReportDescriptor(
                        IOMemoryDescriptor ** descriptor ) const;
    
    virtual OSString * newProductString() const;
                                                                
    virtual IOReturn getReport( IOMemoryDescriptor * report,
                                 IOHIDReportType      reportType,
                                 IOOptionBits         options );

    virtual void postMouseEvent(UInt8 buttons, UInt16 x, UInt16 y, UInt8 wheel=0);
    
    inline bool isScrollPresent() {return _isScrollPresent;}
};

#endif /* !_IOKIT_HID_IOHIDPOINTINGDEVICE_H */
