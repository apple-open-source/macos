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
#include <IOKit/IOLib.h>
#include <IOKit/IODataQueueShared.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <libkern/OSAtomic.h>
#undef enqueue
#include "IOHIDEventServiceQueue.h"
#include "IOHIDEvent.h"

#define super IOSharedDataQueue
OSDefineMetaClassAndStructors( IOHIDEventServiceQueue, super )

IOHIDEventServiceQueue *IOHIDEventServiceQueue::withCapacity(UInt32 size)
{
    IOHIDEventServiceQueue *dataQueue = new IOHIDEventServiceQueue;

    if (dataQueue) {
        if  (!dataQueue->initWithCapacity(size)) {
            dataQueue->release();
            dataQueue = 0;
        }
    }

    return dataQueue;
}

void IOHIDEventServiceQueue::free()
{
    if ( _descriptor )
    {
        _descriptor->release();
        _descriptor = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------
// Add event to the queue.

Boolean IOHIDEventServiceQueue::enqueueEvent( IOHIDEvent * event )
{
    IOByteCount         dataSize  = event->getLength();
    const UInt32        head      = dataQueue->head;  // volatile
    const UInt32        tail      = dataQueue->tail;
    const UInt32        entrySize = dataSize + DATA_QUEUE_ENTRY_HEADER_SIZE;
    IODataQueueEntry *  entry;
    bool                queueFull = false;
    bool                result    = true;

    if ( tail >= head )
    {
        // Is there enough room at the end for the entry?
        if ( (tail + entrySize) <= getQueueSize() )
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

            entry->size = dataSize;
            event->readBytes(&entry->data, dataSize);

            // The tail can be out of bound when the size of the new entry
            // exactly matches the available space at the end of the queue.
            // The tail can range from 0 to getQueueSize() inclusive.

            // RY: effectively performs a memory barrier
            OSAddAtomic(entrySize, (SInt32 *)&dataQueue->tail);
        }
        else if ( head > entrySize ) 	// Is there enough room at the beginning?
        {
            // Wrap around to the beginning, but do not allow the tail to catch
            // up to the head.

            dataQueue->queue->size = dataSize;

            // We need to make sure that there is enough room to set the size before
            // doing this. The user client checks for this and will look for the size
            // at the beginning if there isn't room for it at the end.

            if ( ( getQueueSize() - tail ) >= DATA_QUEUE_ENTRY_HEADER_SIZE )
            {
                ((IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail))->size = dataSize;
            }

            event->readBytes(&dataQueue->queue->data, dataSize);
            
            // RY: effectively performs a memory barrier
            OSCompareAndSwap(dataQueue->tail, entrySize, &dataQueue->tail);
        }
        else
        {
            queueFull = true;
            result = false;	// queue is full
        }
    }
    else
    {
        // Do not allow the tail to catch up to the head when the queue is full.
        // That's why the comparison uses a '>' rather than '>='.

        if ( (head - tail) > entrySize )
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

            entry->size = dataSize;
            event->readBytes(&entry->data, dataSize);

            // RY: effectively performs a memory barrier
            OSAddAtomic(entrySize, (SInt32 *)&dataQueue->tail);
        }
        else
        {
            queueFull = true;
            result = false;	// queue is full
        }
    }

    // Send notification (via mach message) that data is available if either the
    // queue was empty prior to enqueue() or queue was emptied during enqueue()
    if ( ( head == tail ) || ( dataQueue->head == tail ) || queueFull) {
//        if (queueFull) {
//            IOLog("IOHIDEventServiceQueue::enqueueEvent - Queue is full, notifying again\n");
//        }
        sendDataAvailableNotification();
    }

    return result;
}


//---------------------------------------------------------------------------
// set the notification port

void IOHIDEventServiceQueue::setNotificationPort(mach_port_t port) {
    super::setNotificationPort(port);

    if (dataQueue->head != dataQueue->tail)
        sendDataAvailableNotification();
}

//---------------------------------------------------------------------------
// get a mem descriptor.  replacing default behavior

IOMemoryDescriptor * IOHIDEventServiceQueue::getMemoryDescriptor()
{
    if (!_descriptor)
        _descriptor = super::getMemoryDescriptor();

    return _descriptor;
}

//---------------------------------------------------------------------------
