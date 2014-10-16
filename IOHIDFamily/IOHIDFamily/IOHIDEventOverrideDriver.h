/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2013 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef _IOKIT_HID_IOHIDEVENTOVERRIDEDRIVER_H
#define _IOKIT_HID_IOHIDEVENTOVERRIDEDRIVER_H

#include <IOKit/hidevent/IOHIDEventDriver.h>


/*! @class IOHIDOverridRepairDriver : public IOHIDEventDriver
    @abstract
    @discussion
*/

class IOHIDEventOverrideDriver: public IOHIDEventDriver
{
    OSDeclareDefaultStructors( IOHIDEventOverrideDriver )
    
private:
    uint32_t    _rawPointerButtonMask;
    uint32_t    _resultantPointerButtonMask;

    struct {
        IOHIDEventType  eventType;
        union {
            struct {
                uint32_t        usagePage;
                uint32_t        usage;
            } keyboard;
            struct {
                uint32_t        mask;
            } pointer;
        } u;
    } _buttonMap[32];
    
protected:
    virtual void dispatchEvent(IOHIDEvent * event, IOOptionBits options=0);
    
public:
    virtual bool handleStart( IOService * provider );
};

#endif /* !_IOKIT_HID_IOHIDEVENTOVERRIDEDRIVER_H */
