/*
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

#include <IOKit/system.h>
#include <IOKit/IODataQueueShared.h>
#include "IOHIDEventQueue.h"

#define super IODataQueue
OSDefineMetaClassAndStructors( IOHIDEventQueue, IODataQueue )

//---------------------------------------------------------------------------
// Factory methods.

IOHIDEventQueue * IOHIDEventQueue::withCapacity( UInt32 size )
{
    IOHIDEventQueue * queue = new IOHIDEventQueue;
    
    if ( queue && !queue->initWithCapacity(size) )
    {
        queue->release();
        queue = 0;
    }
    
    queue->_started = false;
    
    return queue;
}

IOHIDEventQueue * IOHIDEventQueue::withEntries( UInt32 numEntries,
                                                UInt32 entrySize )
{
    IOHIDEventQueue * queue = new IOHIDEventQueue;

    if ( queue && !queue->initWithEntries(numEntries, entrySize) )
    {
        queue->release();
        queue = 0;
    }

    queue->_started = false;

    return queue;
}

//---------------------------------------------------------------------------
// Add data to the queue.

Boolean IOHIDEventQueue::enqueue( void * data, UInt32 dataSize )
{
    // if we are not started, then dont enqueue
    // for now, return true, since we dont wish to push an error back
    if (!_started)
        return true;

    return super::enqueue(data, dataSize);
}

//---------------------------------------------------------------------------
// 

OSMetaClassDefineReservedUnused(IOHIDEventQueue,  0);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  1);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  2);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  3);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  4);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  5);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  6);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  7);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  8);
OSMetaClassDefineReservedUnused(IOHIDEventQueue,  9);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 10);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 11);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 12);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 13);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 14);
OSMetaClassDefineReservedUnused(IOHIDEventQueue, 15);
