/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
    const UInt32       head      = dataQueue->head;
    const UInt32       tail      = dataQueue->tail;
    const UInt32       entrySize = dataSize + DATA_QUEUE_ENTRY_HEADER_SIZE;
    IODataQueueEntry * entry;
    
    // if we are not started, then dont enqueue
    // for now, return true, since we dont wish to push an error back
    if (!_started)
        return true;
    
    if ( tail >= head )
    {
        if ( (tail + entrySize) < dataQueue->queueSize )
        {
            entry = (IODataQueueEntry *)((char *)dataQueue->queue + tail);

            entry->size = dataSize;
            memcpy(&entry->data, data, dataSize);
            dataQueue->tail += entrySize;
        }
        else if ( head > entrySize )
        {
            // Wrap around to the beginning, but do not allow the tail to catch
            // up to the head.

            dataQueue->queue->size = dataSize;
            ((IODataQueueEntry *)((char *)dataQueue->queue + tail))->size = dataSize;
            memcpy(&dataQueue->queue->data, data, dataSize);
            dataQueue->tail = entrySize;
        }
        else
        {
            return false;	// queue is full
        }
    }
    else
    {
        // Do not allow the tail to catch up to the head when the queue is full.
        // That's why we use a '>' rather than '>='.

        if ( (head - tail) > entrySize )
        {
            entry = (IODataQueueEntry *)((char *)dataQueue->queue + tail);
        
            entry->size = dataSize;
            memcpy(&entry->data, data, dataSize);
            dataQueue->tail += entrySize;
        }
        else
        {
            return false;	// queue is full
        }
    }

    // Send notification (via mach message) that data is available.

    if ( ( head == tail )                /* queue was empty prior to enqueue() */
    ||   ( dataQueue->head == tail ) )   /* queue was emptied during enqueue() */
    {
        sendDataAvailableNotification();
    }

    return true;
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
