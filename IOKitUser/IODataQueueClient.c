/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

		// Check if there's enough room before the end of the queue for a header.
        // If there is room, check if there's enough room to hold the header and
        // the data.

        if ((dataQueue->head + DATA_QUEUE_ENTRY_HEADER_SIZE > dataQueue->queueSize) ||
            ((dataQueue->head + head->size + DATA_QUEUE_ENTRY_HEADER_SIZE) > dataQueue->queueSize))
        {
            // No room for the header or the data, wrap to the beginning of the queue.
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
            // we wraped around to beginning, so read from there
			// either there was not even room for the header
			if ((dataQueue->head + DATA_QUEUE_ENTRY_HEADER_SIZE > dataQueue->queueSize) ||
				// or there was room for the header, but not for the data
				((dataQueue->head + head->size + DATA_QUEUE_ENTRY_HEADER_SIZE) > dataQueue->queueSize)) {
                entry = dataQueue->queue;
                newHead = dataQueue->queue->size + DATA_QUEUE_ENTRY_HEADER_SIZE;
            // else it is at the end
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
                        dataQueue->head = newHead;
                    } else {
                        retVal = kIOReturnNoSpace;
                    }
                } else {
                    retVal = kIOReturnBadArgument;
                }
            } else {
                dataQueue->head = newHead;
            }

            // RY: Update the data size here.  This will
            // insure that dataSize is always updated.
            if (dataSize) {
                *dataSize = entry->size;
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

