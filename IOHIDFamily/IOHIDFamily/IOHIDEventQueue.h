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

#ifndef _IOKIT_HID_IOHIDEVENTQUEUE_H
#define _IOKIT_HID_IOHIDEVENTQUEUE_H

#include <IOKit/IOSharedDataQueue.h>
#include <IOKit/IOLocks.h>
#include "IOHIDKeys.h"
#include "IOHIDElementPrivate.h"

#define DEFAULT_HID_ENTRY_SIZE  sizeof(IOHIDElementValue)+ sizeof(void *)

//---------------------------------------------------------------------------
// IOHIDEventQueue class.
//
// IOHIDEventQueue is a subclass of IOSharedDataQueue. But this may change
// if the HID Manager requires HID specific functionality for the
// event queueing.

class IOHIDEventQueue: public IOSharedDataQueue
{
    OSDeclareDefaultStructors( IOHIDEventQueue )
    
protected:
    IOOptionBits            _state;
    
    IOLock *                _lock;
        
    UInt32                  _currentEntrySize;
    UInt32                  _maxEntrySize;
    UInt32                  _numEntries;
    
    OSSet *                 _elementSet;

    IOMemoryDescriptor *    _descriptor;
    
    IOHIDQueueOptionsType   _options;

    struct ExpansionData { };
    /*! @var reserved
        Reserved for future use.  (Internal use only)  */
    ExpansionData * _reserved;
    

public:
    static IOHIDEventQueue * withCapacity( UInt32 size );
    
    static IOHIDEventQueue * withEntries( UInt32 numEntries,
                                          UInt32 entrySize );
                                          
    virtual void free();

    virtual Boolean enqueue( void * data, UInt32 dataSize );

    virtual void start();
    virtual void stop();
    virtual Boolean isStarted();
    
    virtual void setOptions(IOHIDQueueOptionsType flags);
    virtual IOHIDQueueOptionsType getOptions();

    virtual void enable();
    virtual void disable();
    
    virtual void addElement( IOHIDElementPrivate * element );
    virtual void removeElement( IOHIDElementPrivate * element );
    
    virtual UInt32 getEntrySize ();

    virtual IOMemoryDescriptor *getMemoryDescriptor();

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
