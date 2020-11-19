/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2020 Apple Computer, Inc.  All Rights Reserved.
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
#include <AssertMacros.h>
#include "IOHIDDebug.h"
#include "IOHIDReportElementQueue.h"

#define super IOHIDEventQueue

OSDefineMetaClassAndStructors(IOHIDReportElementQueue, super)

IOHIDReportElementQueue *IOHIDReportElementQueue::withCapacity(UInt32 size, IOHIDLibUserClient *client)
{
    IOHIDReportElementQueue *queue = new IOHIDReportElementQueue;
    if (queue) {
        queue->fClient = client;
    }

    if (size < HID_QUEUE_CAPACITY_MIN) {
        size = HID_QUEUE_CAPACITY_MIN;
    }

    if (size > HID_QUEUE_CAPACITY_MAX_ENTITLED) {
        size = HID_QUEUE_CAPACITY_MAX_ENTITLED;
    }

    if (queue && !queue->initWithCapacity(size)) {
        queue->release();
        queue = NULL;
    }

    return queue;
}

Boolean IOHIDReportElementQueue::enqueue(IOHIDElementValue *value)
{
    return (fClient->processElement(value, this) == kIOReturnSuccess);
}

Boolean IOHIDReportElementQueue::enqueue(void *data, UInt32 dataSize)
{
    return super::enqueue(data, dataSize);
}
