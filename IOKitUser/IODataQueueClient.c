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

#include <IOKit/IODataQueueClient.h>
#include <IOKit/IODataQueueShared.h>

#include <mach/message.h>
#include <mach/mach_port.h>
#include <mach/port.h>
#include <mach/mach_init.h>
#include <IOKit/OSMessageNotification.h>

Boolean IODataQueueDataAvailable(IODataQueueMemory *dataQueue)
{
    return (dataQueue && (dataQueue->head != dataQueue->tail));
}

IODataQueueEntry *IODataQueuePeek(IODataQueueMemory *dataQueue)
{
    IODataQueueEntry *entry = 0;

    if (dataQueue && (dataQueue->head != dataQueue->tail)) {
        IODataQueueEntry *head = (IODataQueueEntry *)((char *)dataQueue->queue + dataQueue->head);
        if ((dataQueue->head + head->size) > dataQueue->queueSize) { // Wrap around to beginning
            entry = dataQueue->queue;
        } else {
            entry = head;
        }
    }

    return entry;
}
IOReturn IODataQueueDequeue(IODataQueueMemory *dataQueue, void *data, UInt32 *dataSize)
{
    IOReturn retVal = kIOReturnSuccess;
    IODataQueueEntry *entry = 0;
    UInt32 newHead = 0;

    if (dataQueue) {
        if (dataQueue->head != dataQueue->tail) {
            IODataQueueEntry *head = (IODataQueueEntry *)((char *)dataQueue->queue + dataQueue->head);
            if ((dataQueue->head + head->size) > dataQueue->queueSize) { // Wrap around to beginning
                entry = dataQueue->queue;
                newHead = dataQueue->queue->size + DATA_QUEUE_ENTRY_HEADER_SIZE;
            } else {
                entry = head;
                newHead = dataQueue->head + head->size + DATA_QUEUE_ENTRY_HEADER_SIZE;
            }
        }

        if (entry) {
            if (data) {
                if (dataSize) {
                    if (entry->size <= *dataSize) {
                        memcpy(data, &entry->data, entry->size);
                        *dataSize = entry->size;
                        dataQueue->head = newHead;
                    } else {
                        *dataSize = entry->size;
                        retVal = kIOReturnNoSpace;
                    }
                } else {
                    retVal = kIOReturnBadArgument;
                }
            } else {
                dataQueue->head = newHead;
            }
        } else {
            retVal = kIOReturnUnderrun;
        }
    } else {
        retVal = kIOReturnBadArgument;
    }
    
    return retVal;
}

IOReturn IODataQueueWaitForAvailableData(IODataQueueMemory *dataQueue, mach_port_t notifyPort)
{
    IOReturn kr;
    struct {
            mach_msg_header_t	msgHdr;
            OSNotificationHeader	notifyHeader;
            mach_msg_trailer_t	trailer;
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
    mach_port_t		port = MACH_PORT_NULL;
    mach_port_limits_t	limits;
    mach_msg_type_number_t	info_cnt;

    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);

    info_cnt = MACH_PORT_LIMITS_INFO_COUNT;

    mach_port_get_attributes(mach_task_self(),
                                port,
                                MACH_PORT_LIMITS_INFO,
                                (mach_port_info_t)&limits,
                                &info_cnt);

    limits.mpl_qlimit = 1;	// Set queue to only 1 message

    mach_port_set_attributes(mach_task_self(),
                                port,
                                MACH_PORT_LIMITS_INFO,
                                (mach_port_info_t)&limits,
                                MACH_PORT_LIMITS_INFO_COUNT);

    return port;
}

