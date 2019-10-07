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

#ifndef _IOKIT_HID_IOHIDEVENTSERVICEQUEUE_H
#define _IOKIT_HID_IOHIDEVENTSERVICEQUEUE_H

#include <IOKit/IOSharedDataQueue.h>

enum {
    kIOHIDEventServiceQueueOptionNotificationForce = 0x1,
    kIOHIDEventServiceQueueOptionNoFullNotification = 0x2,
};

class IOHIDEvent;
//---------------------------------------------------------------------------
// IOHIDEventSeviceQueue class.
//
// IOHIDEventServiceQueue is a subclass of IOSharedDataQueue.

class IOHIDEventServiceQueue: public IOSharedDataQueue
{
    OSDeclareDefaultStructors( IOHIDEventServiceQueue )
    
protected:
    IOMemoryDescriptor *    _descriptor;
    Boolean                 _state;
    OSObject                *_owner;
    UInt32                  _options;
    UInt64                  _notificationCount;

    virtual void sendDataAvailableNotification() APPLE_KEXT_OVERRIDE;

public:
    static IOHIDEventServiceQueue *withCapacity(UInt32 size, UInt32 options = 0);
    static IOHIDEventServiceQueue *withCapacity(OSObject *owner, UInt32 size, UInt32 options = 0);
    
    virtual void free(void) APPLE_KEXT_OVERRIDE;
    
    inline Boolean getState() { return _state; }
    inline void setState(Boolean state) { _state = state; }
    
    inline OSObject *getOwner() { return _owner; }

    virtual Boolean enqueueEvent(IOHIDEvent * event);

    virtual IOMemoryDescriptor *getMemoryDescriptor(void) APPLE_KEXT_OVERRIDE;
    virtual void setNotificationPort(mach_port_t port) APPLE_KEXT_OVERRIDE;
    virtual bool serialize(OSSerialize * serializer) const APPLE_KEXT_OVERRIDE;

};

//---------------------------------------------------------------------------
#endif /* !_IOKIT_HID_IOHIDEVENTSERVICEQUEUE_H */
