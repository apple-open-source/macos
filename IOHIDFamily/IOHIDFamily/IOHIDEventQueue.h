/*
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

#ifndef _IOKIT_HID_IOHIDEVENTQUEUE_H
#define _IOKIT_HID_IOHIDEVENTQUEUE_H

#include <IOKit/IOSharedDataQueue.h>
#include <IOKit/IOLocks.h>
#include "IOHIDKeys.h"
#include "IOHIDElementPrivate.h"
#include "IOHIDLibUserClient.h"

enum {
    kHIDQueueStarted    = 0x01,
    kHIDQueueDisabled   = 0x02
};

#define HID_QUEUE_HEADER_SIZE               (sizeof(IOHIDElementValue))  // 24b
#define HID_QUEUE_CAPACITY_MIN              16384       // 16k
#define HID_QUEUE_CAPACITY_MAX              131072      // 128k
#define HID_QUEUE_CAPACITY_MAX_ENTITLED     3145728     // 3mb

#define HID_QUEUE_USAGE_BUCKETS 11
#define HID_QUEUE_BUCKET_DENOM (100/(HID_QUEUE_USAGE_BUCKETS-1))

//---------------------------------------------------------------------------
// IOHIDEventQueue class.
//
// IOHIDEventQueue is a subclass of IOSharedDataQueue. But this may change
// if the HID Manager requires HID specific functionality for the
// event queueing.

class IOHIDEventQueue: public IOSharedDataQueue
{
    OSDeclareDefaultStructors( IOHIDEventQueue )
    
protected:
    UInt32                  _numEntries;
    UInt32                  _entrySize;
    IOOptionBits            _state;
    IOHIDQueueOptionsType   _options;
    UInt64                  _enqueueErrorCount;
    UInt64                  _usageCounts[HID_QUEUE_USAGE_BUCKETS];

    void            updateUsageCounts();
    OSDictionary    *copyUsageCountDict() const;

public:
    static IOHIDEventQueue *withCapacity(UInt32 size);
    static IOHIDEventQueue *withEntries(UInt32 numEntries, UInt32 entrySize);
    
    virtual Boolean enqueue(void *data, UInt32 dataSize) APPLE_KEXT_OVERRIDE;
    
    inline virtual void setOptions(IOHIDQueueOptionsType flags) { _options = flags; }
    inline virtual IOHIDQueueOptionsType getOptions() { return _options; }
    
    // start/stop are accessible from user space.
    inline virtual void start() { _state |= kHIDQueueStarted; }
    inline virtual void stop() { _state &= ~kHIDQueueStarted; }
    
    // enable disable are only accessible from kernel.
    inline virtual void enable() { _state &= ~kHIDQueueDisabled; }
    inline virtual void disable() { _state |= kHIDQueueDisabled; }
    
    virtual bool serialize(OSSerialize * serializer) const APPLE_KEXT_OVERRIDE;

};

#endif /* !_IOKIT_HID_IOHIDEVENTQUEUE_H */
