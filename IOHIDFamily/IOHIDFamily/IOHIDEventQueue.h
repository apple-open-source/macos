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

#ifndef _IOKIT_HID_IOHIDEVENTQUEUE_H
#define _IOKIT_HID_IOHIDEVENTQUEUE_H

#include <IOKit/IODataQueue.h>

//---------------------------------------------------------------------------
// IOHIDEventQueue class.
//
// IOHIDEventQueue is a subclass of IODataQueue. But this may change
// if the HID Manager requires HID specific functionality for the
// event queueing.

class IOHIDEventQueue: public IODataQueue
{
    OSDeclareDefaultStructors( IOHIDEventQueue )
    
protected:
    Boolean	_started;

    struct ExpansionData { };
    /*! @var reserved
        Reserved for future use.  (Internal use only)  */
    ExpansionData * _reserved;

public:
    static IOHIDEventQueue * withCapacity( UInt32 size );
    
    static IOHIDEventQueue * withEntries( UInt32 numEntries,
                                          UInt32 entrySize );

    virtual Boolean enqueue( void * data, UInt32 dataSize );

    virtual void start() { _started = true; };
    virtual void stop()  { _started = false; };
    virtual Boolean isStarted()  { return _started; };

    OSMetaClassDeclareReservedUnused(IOHIDEventQueue,  0);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue,  1);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue,  2);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue,  3);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue,  4);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue,  5);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue,  6);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue,  7);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue,  8);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue,  9);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue, 10);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue, 11);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue, 12);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue, 13);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue, 14);
    OSMetaClassDeclareReservedUnused(IOHIDEventQueue, 15);
};

#endif /* !_IOKIT_HID_IOHIDEVENTQUEUE_H */
