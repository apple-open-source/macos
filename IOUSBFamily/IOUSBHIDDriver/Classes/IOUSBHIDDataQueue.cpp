/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
#include <IOKit/IODataQueueShared.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>

#include <IOKit/usb/IOUSBHIDDataQueue.h>

#define super OSObject

OSDefineMetaClassAndStructors(IOUSBHIDDataQueue, super)

IOUSBHIDDataQueue *IOUSBHIDDataQueue::withCapacity(UInt32 size)
{
    IOUSBHIDDataQueue *dataQueue = new IOUSBHIDDataQueue;

    if (dataQueue) {
        if  (!dataQueue->initWithCapacity(size)) {
            dataQueue->release();
            dataQueue = 0;
        }
    }

    return dataQueue;
}

IOUSBHIDDataQueue *IOUSBHIDDataQueue::withEntries(UInt32 numEntries, UInt32 entrySize)
{
    IOUSBHIDDataQueue *dataQueue = new IOUSBHIDDataQueue;

    if (dataQueue) {
        if (!dataQueue->initWithEntries(numEntries, entrySize)) {
            dataQueue->release();
            dataQueue = 0;
        }
    }

    return dataQueue;
}

Boolean IOUSBHIDDataQueue::initWithCapacity(UInt32 size)
{
    if (!super::init()) {
        return false;
    }

    if (dataQueue == 0) {
        return false;
    }

    dataQueue->queueSize = size;
    dataQueue->head = 0;
    dataQueue->tail = 0;

    return true;
}

Boolean IOUSBHIDDataQueue::initWithEntries(UInt32 numEntries, UInt32 entrySize)
{
    return (initWithCapacity(numEntries * (DATA_QUEUE_ENTRY_HEADER_SIZE + entrySize)));
}

void IOUSBHIDDataQueue::free()
{
    }

    super::free();

    return;
}



Boolean 
IOUSBHIDDataQueue::enqueue(void *data, UInt32 dataSize)
{
    IODataQueueEntry *head, *tail;
    UInt32 headVal;

    if ((dataQueue->tail + DATA_QUEUE_ENTRY_HEADER_SIZE + dataSize) <= dataQueue->queueSize) {
        tail = (IODataQueueEntry *)((char *)dataQueue->queue + dataQueue->tail);
        
        tail->size = dataSize;
        memcpy(&tail->data, data, dataSize);
        dataQueue->tail += DATA_QUEUE_ENTRY_HEADER_SIZE + dataSize;
    } else if (dataQueue->head >= (DATA_QUEUE_ENTRY_HEADER_SIZE + dataSize)) {	// Wrap around to the beginning
        dataQueue->queue->size = dataSize;
        ((IODataQueueEntry *)((char *)dataQueue->queue + dataQueue->tail))->size = dataSize;
        memcpy(&dataQueue->queue->data, data, dataSize);
        dataQueue->tail = DATA_QUEUE_ENTRY_HEADER_SIZE + dataSize;
    } else {
        return false;
    }

    headVal = dataQueue->head;
    tail = (IODataQueueEntry *)((char *)dataQueue->queue + dataQueue->tail);
    head = (IODataQueueEntry *)((char *)dataQueue->queue + headVal);

    if (((headVal + head->size + DATA_QUEUE_ENTRY_HEADER_SIZE) == dataQueue->tail)
     || (((headVal + head->size + DATA_QUEUE_ENTRY_HEADER_SIZE) > dataQueue->queueSize)
      && (dataQueue->queue->size == dataQueue->tail))) {
        sendDataAvailableNotification();
    }

    return true;
}

void IOUSBHIDDataQueue::setNotificationPort(mach_port_t port)
{
    static struct _hidnotifyMsg init_msg = { {
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0),
        sizeof (struct _hidnotifyMsg),
        MACH_PORT_NULL,
        MACH_PORT_NULL,
        0,
        0
    } };

    if (notifyMsg == 0) {
        notifyMsg = IOMalloc(sizeof(struct _hidnotifyMsg));
    }

    *((struct _hidnotifyMsg *)notifyMsg) = init_msg;

    ((struct _hidnotifyMsg *)notifyMsg)->h.msgh_remote_port = port;
}

void IOUSBHIDDataQueue::sendDataAvailableNotification()
{
    kern_return_t		kr;
    mach_msg_header_t *	msgh;

    msgh = (mach_msg_header_t *)notifyMsg;
    if (msgh) {
        kr = mach_msg_send_from_kernel(msgh, msgh->msgh_size);
        switch(kr) {
            case MACH_SEND_TIMED_OUT:	// Notification already sent
            case MACH_MSG_SUCCESS:
                break;
            default:
                IOLog("%s: dataAvailableNotification failed - msg_send returned: %d\n", /*getName()*/"IOUSBHIDDataQueue", kr);
                break;
        }
    }
}

IOMemoryDescriptor *IOUSBHIDDataQueue::getMemoryDescriptor()
{
    IOMemoryDescriptor *descriptor = 0;

    if (dataQueue != 0) {
        descriptor = IOMemoryDescriptor::withAddress(dataQueue, dataQueue->queueSize + DATA_QUEUE_MEMORY_HEADER_SIZE, kIODirectionOutIn);
    }

    return descriptor;
}

