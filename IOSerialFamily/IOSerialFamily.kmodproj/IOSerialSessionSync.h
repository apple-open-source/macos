/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*
 * IOSerialSessionSync.h
 *
 * 2000-10-21	gvdl	Initial real change to IOKit serial family.
 *
 */


#ifndef _IOSERIALSESSIONSYNC_H
#define _IOSERIALSESSIONSYNC_H

#include <IOKit/serial/IOSerialStreamSync.h>

typedef enum { None, Preempt, NonPreempt } PTTypeT;

/*
 * IOSS_KERN_BUFMAX 1/2 of available kernel stack.
 */
#define IOSS_KERN_BUFMAX (4096 / 2)

struct IOSerialSessionSyncEntry;
class IOSerialSessionSync : public IOSerialStreamSync
{
    OSDeclareDefaultStructors(IOSerialSessionSync)

protected:
    static void initialize();
    IOSerialSessionSyncEntry *fStreamSyncEntry;	// Pointer to shared port entry
    IOReturn fLockError;		// If set then return this error

    // Internal port handoff code
    virtual IOReturn acquireSession(PTTypeT req_type, bool sleep);
    virtual void releaseSessionSync();
    virtual IOReturn getType(PTTypeT want_type, bool sleep);
    virtual IOReturn requestType(PTTypeT new_type, bool sleep);

public:
    static IOSerialSessionSync * withStreamSync(IOSerialStreamSync *nub);

    virtual bool initWithStreamSync(IOSerialStreamSync *stream);

    virtual IOReturn acquireAudit(bool sleep);
    virtual IOReturn acquirePort(bool sleep);
    virtual IOReturn releasePort();
    virtual IOReturn stopPort();

    virtual IOReturn setState(UInt32 state, UInt32 mask);
    virtual UInt32   getState();
    virtual IOReturn watchState(UInt32 *state, UInt32 mask);
    virtual UInt32   nextEvent();
    virtual IOReturn executeEvent(UInt32 event, UInt32 data);
    virtual IOReturn requestEvent(UInt32 event, UInt32 *data);
    virtual IOReturn enqueueEvent(UInt32 event, UInt32 data, bool sleep);
    virtual IOReturn dequeueEvent(UInt32 *event, UInt32 *data, bool sleep);
    virtual IOReturn
        enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep);
    virtual IOReturn
        dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min);
};

#endif _IOSERIALSESSIONSYNC_H
