/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include "IODataQueueClientPrivate.h"
#include <IOKit/IODataQueueShared.h>

#include <mach/message.h>
#include <mach/mach_port.h>
#include <mach/port.h>
#include <mach/mach_init.h>
#include <IOKit/OSMessageNotification.h>
#include <libkern/OSAtomic.h>


static IOReturn _IODataQueueSendDataAvailableNotification(IODataQueueMemory *dataQueue);

Boolean IODataQueueDataAvailable(IODataQueueMemory *dataQueue)
{
    return (dataQueue && (dataQueue->head != dataQueue->tail));
}

IODataQueueEntry *IODataQueuePeek(IODataQueueMemory *dataQueue)
{
    IODataQueueEntry *entry = 0;

    if (dataQueue && (dataQueue->head != dataQueue->tail)) {
            IODataQueueEntry *  head        = 0;
            UInt32              headSize    = 0;
        UInt32              headOffset  = dataQueue->head;
            UInt32              queueSize   = dataQueue->queueSize;
            
            head         = (IODataQueueEntry *)((char *)dataQueue->queue + headOffset);
            headSize     = head->size;
                    
            // Check if there's enough room before the end of the queue for a header.
            // If there is room, check if there's enough room to hold the header and
            // the data.

            if ((headOffset + DATA_QUEUE_ENTRY_HEADER_SIZE > queueSize) ||
                ((headOffset + headSize + DATA_QUEUE_ENTRY_HEADER_SIZE) > queueSize))
            {
                // No room for the header or the data, wrap to the beginning of the queue.
                entry = dataQueue->queue;
            } else {
                entry = head;
            }
        }

    return entry;
}

IOReturn
IODataQueueDequeue(IODataQueueMemory *dataQueue, void *data, uint32_t *dataSize)
{
    IOReturn            retVal          = kIOReturnSuccess;
    IODataQueueEntry *  entry           = 0;
    UInt32              entrySize       = 0;
    UInt32              newHeadOffset   = 0;

    if (dataQueue) {
        if (dataQueue->head != dataQueue->tail) {
            IODataQueueEntry *  head        = 0;
            UInt32              headSize    = 0;
            UInt32              headOffset  = dataQueue->head;
            UInt32              queueSize   = dataQueue->queueSize;
            
            head         = (IODataQueueEntry *)((char *)dataQueue->queue + headOffset);
            headSize     = head->size;
                        
            // we wraped around to beginning, so read from there
            // either there was not even room for the header
            if ((headOffset + DATA_QUEUE_ENTRY_HEADER_SIZE > queueSize) ||
                // or there was room for the header, but not for the data
                ((headOffset + headSize + DATA_QUEUE_ENTRY_HEADER_SIZE) > queueSize)) {
                entry       = dataQueue->queue;
                entrySize   = entry->size;
                newHeadOffset = entrySize + DATA_QUEUE_ENTRY_HEADER_SIZE;
            // else it is at the end
            } else {
                entry = head;
                entrySize = entry->size;
                newHeadOffset = headOffset + entrySize + DATA_QUEUE_ENTRY_HEADER_SIZE;
            }
        }

        if (entry) {
            if (data) {
                if (dataSize) {
                    if (entrySize <= *dataSize) {
                        memcpy(data, &(entry->data), entrySize);
                        OSAtomicCompareAndSwap32Barrier(dataQueue->head, newHeadOffset, (int32_t *)&dataQueue->head);
                    } else {
                        retVal = kIOReturnNoSpace;
                    }
                } else {
                    retVal = kIOReturnBadArgument;
                }
            } else {
                OSAtomicCompareAndSwap32Barrier(dataQueue->head, newHeadOffset, (int32_t *)&dataQueue->head);
            }

            // RY: Update the data size here.  This will
            // ensure that dataSize is always updated.
            if (dataSize) {
                *dataSize = entrySize;
            }
        } else {
            retVal = kIOReturnUnderrun;
        }
    } else {
        retVal = kIOReturnBadArgument;
    }
    
    return retVal;
}

static IOReturn
__IODataQueueEnqueue(IODataQueueMemory *dataQueue, uint32_t dataSize, void *data, IODataQueueClientEnqueueReadBytesCallback callback, void * refcon)
{
    UInt32              head        = dataQueue->head;  // volatile
    UInt32              tail        = dataQueue->tail;
    UInt32              queueSize   = dataQueue->queueSize;
    UInt32              entrySize   = dataSize + DATA_QUEUE_ENTRY_HEADER_SIZE;
    IOReturn            retVal      = kIOReturnSuccess;
    IODataQueueEntry *  entry;

    if ( tail >= head )
    {
        // Is there enough room at the end for the entry?
        if ( (tail + entrySize) <= queueSize )
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

            if ( data )
                memcpy(&(entry->data), data, dataSize);
            else if ( callback )
                (*callback)(refcon, &(entry->data), dataSize);

            entry->size = dataSize;            

            // The tail can be out of bound when the size of the new entry
            // exactly matches the available space at the end of the queue.
            // The tail can range from 0 to queueSize inclusive.

            OSAtomicAdd32Barrier(entrySize, (int32_t *)&dataQueue->tail);
        }
        else if ( head > entrySize )     // Is there enough room at the beginning?
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue);
            
            if ( data ) 
                memcpy(&(entry->data), data, dataSize);
            else if ( callback )
                (*callback)(refcon, &(entry->data), dataSize);

            // Wrap around to the beginning, but do not allow the tail to catch
            // up to the head.

            entry->size = dataSize;

            // We need to make sure that there is enough room to set the size before
            // doing this. The user client checks for this and will look for the size
            // at the beginning if there isn't room for it at the end.

            if ( ( queueSize - tail ) >= DATA_QUEUE_ENTRY_HEADER_SIZE )
            {
                ((IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail))->size = dataSize;
            }

            OSAtomicCompareAndSwap32Barrier(dataQueue->tail, entrySize, (int32_t *)&dataQueue->tail);
        }
        else
        {
            retVal = kIOReturnOverrun;  // queue is full
        }
    }
    else
    {
        // Do not allow the tail to catch up to the head when the queue is full.
        // That's why the comparison uses a '>' rather than '>='.

        if ( (head - tail) > entrySize )
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

            if ( data )
                memcpy(&(entry->data), data, dataSize);
            else if ( callback )
                (*callback)(refcon, &(entry->data), dataSize);

            entry->size = dataSize;
            
            OSAtomicAdd32Barrier(entrySize, (int32_t *)&dataQueue->tail);
        }
        else
        {
            retVal = kIOReturnOverrun;  // queue is full
        }
    }
    
    // Send notification (via mach message) that data is available.    
    
    if ( retVal == kIOReturnSuccess ) {
        if ( ( head == tail )                                                           /* queue was empty prior to enqueue() */
        ||   ( dataQueue->head == tail ) )  /* queue was emptied during enqueue() */
        {
            retVal = _IODataQueueSendDataAvailableNotification(dataQueue);
        }
#if TARGET_IPHONE_SIMULATOR
        else
        {
            retVal = _IODataQueueSendDataAvailableNotification(dataQueue);
        }
#endif
    }

    else if ( retVal == kIOReturnOverrun ) {
        // Send extra data available notification, this will fail and we will
        // get a send possible notification when the client starts responding
        (void) _IODataQueueSendDataAvailableNotification(dataQueue);
    }

    return retVal;
}

IOReturn
IODataQueueEnqueue(IODataQueueMemory *dataQueue, void *data, uint32_t dataSize)
{
    return __IODataQueueEnqueue(dataQueue, dataSize, data, NULL, NULL);
}


IOReturn
_IODataQueueEnqueueWithReadCallback(IODataQueueMemory *dataQueue, uint32_t dataSize, IODataQueueClientEnqueueReadBytesCallback callback, void * refcon)
{
    return __IODataQueueEnqueue(dataQueue, dataSize, NULL, callback, refcon);
}


IOReturn IODataQueueWaitForAvailableData(IODataQueueMemory *dataQueue, mach_port_t notifyPort)
{
    IOReturn kr;
    struct {
            mach_msg_header_t msgHdr;
//            OSNotificationHeader notifyHeader;
            mach_msg_trailer_t trailer;
    } msg;
    
    if (dataQueue && (notifyPort != MACH_PORT_NULL)) {
        kr = mach_msg(&msg.msgHdr, MACH_RCV_MSG, 0, sizeof(msg), notifyPort, 0, MACH_PORT_NULL);
    } else {
        kr = kIOReturnBadArgument;
    }

    return kr;
}

mach_port_t IODataQueueAllocateNotificationPort()
{
    mach_port_t        port = MACH_PORT_NULL;
    mach_port_limits_t    limits;
    mach_msg_type_number_t    info_cnt;

    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);

    info_cnt = MACH_PORT_LIMITS_INFO_COUNT;

    mach_port_get_attributes(mach_task_self(),
                                port,
                                MACH_PORT_LIMITS_INFO,
                                (mach_port_info_t)&limits,
                                &info_cnt);

    limits.mpl_qlimit = 1;    // Set queue to only 1 message

    mach_port_set_attributes(mach_task_self(),
                                port,
                                MACH_PORT_LIMITS_INFO,
                                (mach_port_info_t)&limits,
                                MACH_PORT_LIMITS_INFO_COUNT);

    return port;
}

IOReturn IODataQueueSetNotificationPort(IODataQueueMemory *dataQueue, mach_port_t notifyPort)
{
    IODataQueueAppendix *   appendix    = NULL;
    UInt32                  queueSize   = 0;
            
    if ( !dataQueue )
        return kIOReturnBadArgument;
        
    queueSize = dataQueue->queueSize;
    
    appendix = (IODataQueueAppendix *)((UInt8 *)dataQueue + queueSize + DATA_QUEUE_MEMORY_HEADER_SIZE);

    appendix->msgh.msgh_bits        = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    appendix->msgh.msgh_size        = sizeof(appendix->msgh);
    appendix->msgh.msgh_remote_port = notifyPort;
    appendix->msgh.msgh_local_port  = MACH_PORT_NULL;
    appendix->msgh.msgh_id          = 0;

    return kIOReturnSuccess;
}

IOReturn _IODataQueueSendDataAvailableNotification(IODataQueueMemory *dataQueue)
{
    IODataQueueAppendix *   appendix    = NULL;
    UInt32                  queueSize   = 0;
            
    queueSize = dataQueue->queueSize;
    
    appendix = (IODataQueueAppendix *)((UInt8 *)dataQueue + queueSize + DATA_QUEUE_MEMORY_HEADER_SIZE);

    if ( appendix->msgh.msgh_remote_port == MACH_PORT_NULL )
        return kIOReturnSuccess;  // return success if no port is declared
    
    kern_return_t        kr;
    mach_msg_header_t   msgh = appendix->msgh;
        
    kr = mach_msg(&msgh, MACH_SEND_MSG | MACH_SEND_TIMEOUT, msgh.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    switch(kr) {
        case MACH_SEND_TIMED_OUT:    // Notification already sent
        case MACH_MSG_SUCCESS:
            break;
        default:
            // perhaps add log here
            break;
    }
    
    return kr;
}
