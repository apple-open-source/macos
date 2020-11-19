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
#include "IOHIDEventService.h"
#include "IOHIDEvent.h"
#include "IOHIDDebug.h"
#include <os/overflow.h>
#include <pexpert/pexpert.h>

#define super IOSharedDataQueue
OSDefineMetaClassAndStructors( IOHIDEventServiceQueue, super )

IOHIDEventServiceQueue *IOHIDEventServiceQueue::withCapacity(UInt32 size, UInt32 options)
{
    IOHIDEventServiceQueue *dataQueue = new IOHIDEventServiceQueue;
    bool noFullMsg = false;

    if (dataQueue) {
        if  (!dataQueue->initWithCapacity(size)) {
            dataQueue->release();
            dataQueue = 0;
        }
    }

    dataQueue->_options = options;
    dataQueue->_notificationCount = 0;

    PE_parse_boot_argn("hidq_no_full_msg", &noFullMsg, sizeof(noFullMsg));
    dataQueue->_options |= (noFullMsg ? kIOHIDEventServiceQueueOptionNoFullNotification : 0);

    return dataQueue;
}

IOHIDEventServiceQueue *IOHIDEventServiceQueue::withCapacity(OSObject *owner, UInt32 size, UInt32 options)
{
    IOHIDEventServiceQueue *dataQueue = IOHIDEventServiceQueue::withCapacity(size, options);
    
    if (dataQueue) {
        dataQueue->_owner = owner;
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
    IOByteCount         eventSize = event->getLength();
    IOByteCount         dataSize  = ALIGNED_DATA_SIZE (eventSize, sizeof(uint32_t));
    UInt32              head;
    UInt32              tail;
    UInt32              newTail = 0;
    UInt32              entrySize;
    IODataQueueEntry *  entry;
    bool                queueFull = false;
    bool                result    = true;
    
    // check overflow of alignment
    if (dataSize < eventSize) {
        return false;
    }
    
    // check overflow of entrySize
    if (os_add_overflow(dataSize, DATA_QUEUE_ENTRY_HEADER_SIZE, &entrySize)) {
        return false;
    }
    
    // Force a single read of head and tail
    tail = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->tail, __ATOMIC_RELAXED);
    head = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->head, __ATOMIC_ACQUIRE);

    if ( tail > getQueueSize() || head > getQueueSize() || entrySize < dataSize)
    {
        return false;
    }

    if ( tail >= head )
    {
        // Is there enough room at the end for the entry?
        if ( (getQueueSize() - tail) >= entrySize )
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

            entry->size = (UInt32)dataSize;
            event->readBytes(&entry->data, eventSize);

            // The tail can be out of bound when the size of the new entry
            // exactly matches the available space at the end of the queue.
            // The tail can range from 0 to getQueueSize() inclusive.

            newTail = tail + entrySize;
        }
        else if ( head > entrySize ) 	// Is there enough room at the beginning?
        {
            // Wrap around to the beginning, but do not allow the tail to catch
            // up to the head.

            dataQueue->queue->size = (UInt32)dataSize;

            // We need to make sure that there is enough room to set the size before
            // doing this. The user client checks for this and will look for the size
            // at the beginning if there isn't room for it at the end.

            if ( ( getQueueSize() - tail ) >= DATA_QUEUE_ENTRY_HEADER_SIZE )
            {
                ((IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail))->size = (UInt32)dataSize;
            }

            event->readBytes(&dataQueue->queue->data, eventSize);
            
            newTail = entrySize;
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

        if ( (head - tail) > entrySize  )
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

            entry->size = (UInt32)dataSize;
            event->readBytes(&entry->data, eventSize);

            newTail = tail + entrySize;
        }
        else
        {
            queueFull = true;
            result = false;	// queue is full
        }
    }

    if (result) {
        // Publish the data we just enqueued
        __c11_atomic_store((_Atomic UInt32 *)&dataQueue->tail, newTail, __ATOMIC_RELEASE);
    }

    if (tail != head) {
        // From <rdar://problem/43093190> IOSharedDataQueue stalls
        //
        // The memory barrier below pairs with the one in ::dequeue
        // so that either our store to the tail cannot be missed by
        // the next dequeue attempt, or we will observe the dequeuer
        // making the queue empty.
        //
        // Of course, if we already think the queue is empty,
        // there's no point paying this extra cost.
        //
        __c11_atomic_thread_fence(__ATOMIC_SEQ_CST);
        head = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->head, __ATOMIC_RELAXED);
    }

    // Send notification (via mach message) that data is available if either the
    // queue was empty prior to enqueue() or queue was emptied during enqueue()
    if ( (event->getOptions() & kHIDDispatchOptionDeliveryNotificationSuppress) == 0) {
        if ( (_options & kIOHIDEventServiceQueueOptionNotificationForce)
            || (event->getOptions() & kHIDDispatchOptionDeliveryNotificationForce)
            || ( head == tail )) {

            sendDataAvailableNotification();

        } else if (queueFull) {

            if ( !(_options & kIOHIDEventServiceQueueOptionNoFullNotification) ) {
                sendDataAvailableNotification();
            }
            else {
                HIDLogError("IOHIDEventServiceQueue::enqueueEvent - Queue is full! %p", _owner);
            }
        }
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

void IOHIDEventServiceQueue::sendDataAvailableNotification()
{
    _notificationCount++;
    super::sendDataAvailableNotification();
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

bool IOHIDEventServiceQueue::serialize(OSSerialize * serializer) const {
    
    bool ret;
    
    if (serializer->previouslySerialized(this)) {
        return true;
    }
    
    OSDictionary *dict = OSDictionary::withCapacity(2);
    if ( dict ) {
        OSNumber * num = OSNumber::withNumber(dataQueue->head, 32);
        if (num) {
            dict->setObject("head", num);
            num->release();
        }
        num = OSNumber::withNumber(dataQueue->tail, 32);
        if (num) {
            dict->setObject("tail", num);
            num->release();
        }
        num = OSNumber::withNumber(_notificationCount, 64);
        if (num) {
            dict->setObject("NotificationCount", num);
            num->release();
        }
        num = OSNumber::withNumber((_options & kIOHIDEventServiceQueueOptionNotificationForce), 32);
        if (num) {
            dict->setObject("NotificationForce", num);
            num->release();
        }
        num = OSNumber::withNumber((_options & kIOHIDEventServiceQueueOptionNoFullNotification), 32);
        if (num) {
            dict->setObject("NoFullMsg", num);
            num->release();
        }
        ret = dict->serialize(serializer);
        dict->release();
    } else {
        ret = false;
    }
    return ret;
}
