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
 * IOSerialSessionSync.cpp - This file contains the definition of the
 * IOSerialSessionSync Class, which is a generic class designed for
 * devices that do word oriented transfers (async) rather than block or packet
 * transfers.  Most notably, RS-232, Printer, Mouse, and Keyboard type devices.
 * It is designed to talk to a kernal device that talks the streamices
 * protocol.
 *
 * 2000-10-21	gvdl	Initial real change to IOKit serial family.
 */

#include <string.h>

#include <IOKit/assert.h>
#include <IOKit/system.h>

#include <IOKit/IOLib.h>
#include <IOKit/IOConditionLock.h>

#include "IOSerialSessionSync.h"

// 
// port is used for all communications with the device.  This guarantees that
// we can not talk to the device without having a valid lock on it.
//
#define STREAM ((IOSerialStreamSync *) fProvider)

#define super IOSerialStreamSync
OSDefineMetaClassAndStructors(IOSerialSessionSync, IOSerialStreamSync)

struct IOSerialSessionSyncEntry {
    queue_chain_t fLink;
    IOConditionLock  *fSleepLock;	// StreamSync sleep condition
    IOConditionLock *fHandOverLock;	// Port token - Non interruptible
    IOSerialStreamSync *stream;
    IOSerialSessionSync *session;		// SessionSync who holds lock on stream
    int fRefCount;			// Count of references;
    PTTypeT type;			// Current type of port
    UInt8 waitP, waitN;			// counts of waiting threads
};

class IOSerialSessionSyncGlobals {
private:
    IOLock *fLock;		// Guard against IOMalloc sleep
    queue_head_t fStreamSyncList;	// queue of fStreamSyncListEntry's

public:
    IOSerialSessionSyncGlobals();
    ~IOSerialSessionSyncGlobals();

    inline bool isValid();

    IOSerialSessionSyncEntry *createEntry(IOSerialStreamSync *stream, bool *cyclePortP);
    void releaseEntry(IOSerialSessionSyncEntry *entry);
};

// Create an instance of the IOSerialSessionSyncGlobals
// This runs the static constructor and destructor so that
// I can grab and release a lock as appropriate.
static IOSerialSessionSyncGlobals sSessionSyncGlobals;

IOSerialSessionSyncGlobals::IOSerialSessionSyncGlobals()
{
    /* Init port lock list. */
    queue_init(& fStreamSyncList);
    
    fLock = IOLockAlloc();
    if (fLock)
        IOLockInit(fLock);
}

IOSerialSessionSyncGlobals::~IOSerialSessionSyncGlobals()
{
    /* Init port lock list. */
    queue_init(&fStreamSyncList);
    
    if (fLock)
        IOLockFree(fLock);
}

bool IOSerialSessionSyncGlobals::isValid()
{
    return (fLock != 0);
}

IOSerialSessionSyncEntry *IOSerialSessionSyncGlobals::
createEntry(IOSerialStreamSync *stream, bool *cyclePortP)
{
    IOSerialSessionSyncEntry *entry;
    IOConditionLock *sleepLock = 0, *handOverLock = 0;
    bool undoAcquire = false;

    IOTakeLock(fLock);

    do {
        queue_iterate(&fStreamSyncList, entry, IOSerialSessionSyncEntry *, fLink) {
            if (stream == entry->stream) {
                entry->fRefCount++;
                break;
            }
        }

        if ( !queue_end(&fStreamSyncList, (queue_entry_t) entry)) {
            *cyclePortP = true;
            break;
        }

        *cyclePortP = false;
        entry = 0;

        // Didn't find the port so try to acquire it
        if (stream->acquirePort(false) != kIOReturnSuccess)
            break;
        undoAcquire = true;

        sleepLock = new IOConditionLock;
        handOverLock = new IOConditionLock;
        entry = (IOSerialSessionSyncEntry *) IOMalloc(sizeof(*entry));
        if (!sleepLock || !handOverLock || !entry)
            break;

        undoAcquire = false;

        sleepLock->initWithCondition(false);
        handOverLock->initWithCondition(true);

        bzero(entry, sizeof(*entry));
        entry->fRefCount	= 1;
        entry->stream		= stream;
        entry->type		= None;
        entry->fSleepLock	= sleepLock; sleepLock = 0;
        entry->fHandOverLock	= handOverLock; handOverLock = 0;
        queue_enter(&fStreamSyncList, entry, IOSerialSessionSyncEntry *, fLink);

    } while(false);

    if (handOverLock)
        handOverLock->release();
    if (sleepLock)
        sleepLock->release();
    if (undoAcquire)
        stream->releasePort();

    IOUnlock(fLock);
    return entry;
}

void IOSerialSessionSyncGlobals::
releaseEntry(IOSerialSessionSyncEntry *entry)
{
    int refCount;

    IOTakeLock(fLock);
    refCount = --(entry->fRefCount);
    if (refCount)
        IOUnlock(fLock);
    else {
        queue_remove(&fStreamSyncList, entry, IOSerialSessionSyncEntry *, fLink);
        IOUnlock(fLock);
 
        entry->fSleepLock->release();
        entry->fHandOverLock->release();
        entry->stream->releasePort();
        IOFree(entry, sizeof(*entry));
    }
}

bool IOSerialSessionSync::initWithStreamSync(IOSerialStreamSync *nub)
{
    if (!super::init())
        return false;

    if (!sSessionSyncGlobals.isValid())
        return false;

    if (!OSDynamicCast(IOSerialStreamSync, nub))
        return false;

    STREAM = nub;
    
    fLockError = kIOReturnNotOpen;	// SessionSync Not Locked
    fStreamSyncEntry = 0;			// Only setup once lock is achieved

    return true;
}

IOSerialSessionSync * IOSerialSessionSync::withStreamSync(IOSerialStreamSync *nub)
{
    IOSerialSessionSync *me = new IOSerialSessionSync;
    if (me && !me->initWithStreamSync(nub)) {
        me->release();
        me = 0;
    }

    return me;
}

/*
 * Acquires an preemptable lock on the current session.
 */
IOReturn IOSerialSessionSync::acquireAudit(bool sleep)
{
    return acquireSession(Preempt, sleep);
}

/*
 * Acquires an exclusive lock on the current session.
 */
IOReturn IOSerialSessionSync::acquirePort(bool sleep)
{
    return acquireSession(NonPreempt, sleep);
}


/*
 * Release all locks for this session, called from release as well
 */
IOReturn IOSerialSessionSync::releasePort()
{
    if (fStreamSyncEntry)
    {
        requestType(None, false);

        // Release the Port
        releaseSessionSync();
    }

    return kIOReturnSuccess;
}

/*
 * Release all locks for this session, called from release as well
 */
IOReturn IOSerialSessionSync::stopPort()
{
    releasePort();
    fLockError = kIOReturnOffline;

    return kIOReturnSuccess;
}

IOReturn IOSerialSessionSync::
setState(UInt32 state, UInt32 mask)
{
    if (fLockError)
        return fLockError;

    return STREAM->setState(state, mask);
}

UInt32 IOSerialSessionSync::getState()
{
    if (fLockError)
        return 0;

    return STREAM->getState();
}

IOReturn IOSerialSessionSync::watchState(UInt32 *state, UInt32 mask)
{
    IOReturn result;

    if (fLockError)
        return fLockError;
    
    result = STREAM->watchState(state, mask);

    if (fLockError)
        return fLockError;

    return result;
}

UInt32 IOSerialSessionSync::nextEvent()
{
    if (fLockError != kIOReturnSuccess)
        return PD_E_EOQ;

    return STREAM->nextEvent();
}

IOReturn IOSerialSessionSync::executeEvent(UInt32 event, UInt32 data)
{
    if (fLockError)
        return fLockError;

    return STREAM->executeEvent(event, data);
}

IOReturn IOSerialSessionSync::requestEvent(UInt32 event, UInt32 *data)
{
    if (fLockError)
        return fLockError;

    return STREAM->requestEvent(event, data);
}

IOReturn IOSerialSessionSync::enqueueEvent(UInt32 event, UInt32 data, bool sleep)
{
    IOReturn result;

    if (fLockError)
        return fLockError;

    result = STREAM->enqueueEvent(event, data, sleep);

    if (fLockError)
        return fLockError;

    return result;
}

IOReturn IOSerialSessionSync::dequeueEvent(UInt32 *event, UInt32 *data, bool sleep)
{
    IOReturn result;

    if (fLockError)
        return fLockError;

    result = STREAM->dequeueEvent(event, data, sleep);
    
    if (fLockError)
        return fLockError;
 
    return result;
}

IOReturn IOSerialSessionSync::
enqueueData(UInt8 *buffer, UInt32 size, UInt32 *count, bool sleep)
{
    IOReturn result;

    if (fLockError)
        return fLockError;

    result = STREAM->enqueueData(buffer, size, count, sleep);

    if (fLockError)
        return fLockError;

    return result;
}

IOReturn IOSerialSessionSync::
dequeueData(UInt8 *buffer, UInt32 size, UInt32 *count, UInt32 min)
{
    IOReturn result;

    if (fLockError)
        return fLockError;

    result = STREAM->dequeueData(buffer, size, count, min);

    if (fLockError)
        return fLockError;

    return result;
}

IOReturn IOSerialSessionSync::acquireSession(PTTypeT req_type, bool sleep)
{
    bool cyclePort;

    if (fStreamSyncEntry)
        cyclePort = (req_type == Preempt && fStreamSyncEntry->waitN);
    else {
        fStreamSyncEntry = sSessionSyncGlobals.createEntry(STREAM, &cyclePort);
        if (!fStreamSyncEntry)
            return kIOReturnExclusiveAccess;
    }

    fLockError = requestType(req_type, sleep);
    if (fLockError != kIOReturnSuccess)
        releaseSessionSync();
    else if (cyclePort) {
        fStreamSyncEntry->stream->releasePort();
        fLockError = fStreamSyncEntry->stream->acquirePort(false);
        assert(fLockError == kIOReturnSuccess);
    }

    return fLockError;
}

void IOSerialSessionSync::releaseSessionSync()
{
    assert(fStreamSyncEntry);

    if (fStreamSyncEntry->session == this)
    {
        // If we are the current session holder then deactivate the port on
        // on releasing it.
        (void) fStreamSyncEntry->stream->executeEvent(PD_E_ACTIVE, false);
    }

    sSessionSyncGlobals.releaseEntry(fStreamSyncEntry);
    fStreamSyncEntry = (IOSerialSessionSyncEntry *) NULL;
    fLockError   = kIOReturnNotOpen;
}

IOReturn IOSerialSessionSync::getType(PTTypeT want_type, bool sleep)
{
    UInt8 *counter;
    IOReturn result = kIOReturnSuccess;

    assert(want_type == Preempt || want_type == NonPreempt);
    assert(fStreamSyncEntry);

    if (!sleep)
        result = kIOReturnCannotLock;
    else	// sleep until available
    {
        // Point to the appropriate waiting process counter
        counter = (want_type == Preempt)
            ? & fStreamSyncEntry->waitP : & fStreamSyncEntry->waitN;

        // Log waiting and release table lock
        (*counter)++;

        // Wait for Current lock holder to release
        if (fStreamSyncEntry->fSleepLock->lockWhen(want_type))
            result = kIOReturnIPCError;

        // Accept the batton if it has been passed and release it
        fStreamSyncEntry->fHandOverLock->lock();
        if (fStreamSyncEntry->fHandOverLock->getCondition())
            fStreamSyncEntry->fHandOverLock->unlock();
        else
            fStreamSyncEntry->fHandOverLock->unlockWith(true);

        (*counter)--;

        // Change type if condition lock returned cleanly
        if (!result)
        {
            fStreamSyncEntry->type   = want_type;
            fStreamSyncEntry->session = this;
        }
    }

    return result;
}

IOReturn IOSerialSessionSync::requestType(PTTypeT new_type, bool sleep)
{
    IOReturn result = kIOReturnBadArgument;

    assert(fStreamSyncEntry);
    if (new_type == None)
    {
        assert(fStreamSyncEntry->type != None);

        fStreamSyncEntry->type   = None;
        fStreamSyncEntry->session = NULL;

        if (fStreamSyncEntry->waitN)
            new_type = NonPreempt;
        else if (fStreamSyncEntry->waitP)
            new_type = Preempt;
        else
            new_type = None;

        if (new_type != None)
        {
            // Setup the batton for handover completion if anybody to hand to
            fStreamSyncEntry->fHandOverLock->lock();
            fStreamSyncEntry->fHandOverLock->unlockWith(false);
        }

        // Wake up anybody waiting on new_type
        fStreamSyncEntry->fSleepLock->unlockWith(new_type);

        if (new_type != None)
        {
            // Complete the handover batton and reset type
            fStreamSyncEntry->fHandOverLock->lockWhen(true);
            fStreamSyncEntry->fHandOverLock->unlock();
        }
        result = kIOReturnSuccess;
    }
    else if (fStreamSyncEntry->session == this)
    {
        // Allow the current session holder to change lock type
        switch (new_type)
        {
        case Preempt:
            if (fStreamSyncEntry->waitN)
            {
                // Waiting Non Preemptable so give up lock
                requestType(None, false);
                result = getType(Preempt, sleep);
                break;
            }
            // Fall through if no waiting Non Preempt threads

        case NonPreempt:
            fStreamSyncEntry->type = new_type;
            result = kIOReturnSuccess;
            break;

            default: break;
        }
    }
    else switch (fStreamSyncEntry->type)
    {
    case None:
        if (fStreamSyncEntry->fSleepLock->lock())
            result = kIOReturnIPCError;
        else
        {
            fStreamSyncEntry->type    = new_type;
            fStreamSyncEntry->session = this;
            result = kIOReturnSuccess;
        }
        break;

    case NonPreempt:
        switch (new_type)
        {
        case Preempt:
        case NonPreempt:
            result = getType(new_type, sleep);
            break;

        default: break;
        }
        break;	// End of type == NonPreempt

    case Preempt:
        switch (new_type)
        {
        case NonPreempt:
            // Preempt the current lock holder
            fStreamSyncEntry->session->releaseSessionSync();

            // Take over the lock from the preempted session
            fStreamSyncEntry->type   = new_type;
            fStreamSyncEntry->session = this;
            result = kIOReturnSuccess;
            break;

        case Preempt:
            result = getType(new_type, sleep);
            break;

        default: break;
        }
        break;	// End of type == Preempt
    }	// End of switch type

    return result;
}
