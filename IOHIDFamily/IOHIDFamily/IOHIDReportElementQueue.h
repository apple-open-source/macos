/*
 *
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

#ifndef _IOKIT_HID_IOHIDREPORTELEMENTQUEUE_H
#define _IOKIT_HID_IOHIDREPORTELEMENTQUEUE_H

#include "IOHIDEventQueue.h"
#include "IOHIDLibUserClient.h"

//---------------------------------------------------------------------------
// IODHIDReportElementQueue class.
//
// IODHIDReportElementQueue is a subclass of IOHIDEventQueue. This allows us to enqueue
// large input reports by passing the element into the UC and letting it handle the memory.
// The report is actually enqueued with the call too enqueue(void*, size_t) which puts the
// report into the shared memory.

class IOHIDReportElementQueue: public IOHIDEventQueue
{
    OSDeclareDefaultStructors( IOHIDReportElementQueue )

protected:
    IOHIDLibUserClient *fClient;

public:
    static IOHIDReportElementQueue *withCapacity(UInt32 size, IOHIDLibUserClient *client);

    virtual Boolean enqueue(IOHIDElementValue* element);
    virtual Boolean enqueue(void *data, UInt32 dataSize) APPLE_KEXT_OVERRIDE;
};

#endif /* !_IOKIT_HID_IOHIDREPORTELEMENTQUEUE_H */
