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

#ifndef _IOKIT_HID_IOHIDDEVICESHIM_H
#define _IOKIT_HID_IOHIDDEVICESHIM_H

#include "IOHIDDevice.h"
#include "IOHIDevice.h"

#define kIOHIDAppleVendorID 1452
typedef enum IOHIDTransport {
    kIOHIDTransportNone = 0,
    kIOHIDTransportUSB,
    kIOHIDTransportADB,
    kIOHIDTransportPS2
} IOHIDTransport;

class IOHIDDeviceShim : public IOHIDDevice
{
    OSDeclareDefaultStructors( IOHIDDeviceShim )

private:
    IOService *       _device;
    IOHIDevice *      _hiDevice;
    IOHIDTransport		_transport;
    UInt32            _location;
    boolean_t         _allowVirtualProvider;

protected:

    virtual bool handleStart( IOService * provider );
    
public:
    virtual IOReturn newReportDescriptor(
                        IOMemoryDescriptor ** descriptor ) const = 0;
    virtual bool initWithLocation(UInt32 location = 0);
    
    virtual IOHIDTransport transport() {return _transport;};
    
    virtual OSString * newTransportString() const;
    virtual OSString * newProductString() const;
    virtual OSString * newManufacturerString() const;
    virtual OSNumber * newVendorIDNumber() const;
    virtual OSNumber * newProductIDNumber() const;
    virtual OSNumber * newLocationIDNumber() const;
    virtual OSString * newSerialNumberString() const;
    
    virtual bool       isSeized();
    virtual bool       initWithParameters(UInt32 location, boolean_t allowVirtualProvider);

};

#endif /* !_IOKIT_HID_IOHIDDEVICESHIM_H */
