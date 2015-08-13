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

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/assert.h>
#include <IOKit/IOKitKeys.h>

#include <IOKit/graphics/IOAccelerator.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>
#include <IOKit/IOUserClient.h>


OSDefineMetaClassAndStructors(IOAccelerator, IOService)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static IOLock *         gLock;
static queue_head_t     gGlobalList;
static UInt32           gTotalCount;
static SInt32           gTweak;

struct IOAccelIDRecord
{
    IOAccelID           id;
    SInt32              retain;
    queue_chain_t       task_link;
    queue_chain_t       glob_link;
};

enum { kTweakBits = 0x1f };     // sizeof(IOAccelIDRecord) == 24

class IOAccelerationUserClient : public IOUserClient
{
    /*
     * Declare the metaclass information that is used for runtime
     * typechecking of IOKit objects.
     */

    OSDeclareDefaultStructors( IOAccelerationUserClient );

private:
    task_t              fTask;
    queue_head_t        fTaskList;

    static void initialize();

public:
    /* IOService overrides */
    virtual bool start( IOService * provider );
    virtual void stop( IOService * provider );

    /* IOUserClient overrides */
    virtual bool initWithTask( task_t owningTask, void * securityID,
                                                UInt32 type,  OSDictionary * properties );
    virtual IOReturn clientClose( void );

    virtual IOExternalMethod * getTargetAndMethodForIndex(
                                            IOService ** targetP, UInt32 index );


    IOReturn extCreate(IOOptionBits options,
                        IOAccelID requestedID, IOAccelID * idOut);
    IOReturn extDestroy(IOOptionBits options, IOAccelID id);

};

#define super IOUserClient
OSDefineMetaClassAndStructorsWithInit(IOAccelerationUserClient, IOUserClient, 
                                        IOAccelerationUserClient::initialize());

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IOAccelerationUserClient::initialize()
{
    if (!gLock)
    {
        gLock = IOLockAlloc();
        queue_init(&gGlobalList);
    }
}

bool IOAccelerationUserClient::initWithTask( task_t owningTask, void * securityID,
                                             UInt32 type,  OSDictionary * properties )
{
   
    if ( properties != NULL )
            properties->setObject ( kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue );
   
    fTask = owningTask;
    queue_init(&fTaskList);

    return( super::initWithTask( owningTask, securityID, type, properties ));
}

bool IOAccelerationUserClient::start( IOService * provider )
{
    if( !super::start( provider ))
        return( false );

    return (true);
}

IOReturn IOAccelerationUserClient::clientClose( void )
{
    if( !isInactive())
        terminate();

    return( kIOReturnSuccess );
}

void IOAccelerationUserClient::stop( IOService * provider )
{
    IOAccelIDRecord * record;

    IOLockLock(gLock);

    while (!queue_empty( &fTaskList ))
    {
        queue_remove_first( &fTaskList,
                            record,
                            IOAccelIDRecord *,
                            task_link );

        if (--record->retain)
            record->task_link.next = 0;
        else
        {
            queue_remove(&gGlobalList,
                            record,
                            IOAccelIDRecord *,
                            glob_link);
            gTotalCount--;
            IODelete(record, IOAccelIDRecord, 1);
        }
    }
    IOLockUnlock(gLock);

    super::stop( provider );
}

IOExternalMethod * IOAccelerationUserClient::getTargetAndMethodForIndex(
    IOService ** targetP, UInt32 index )
{
    static const IOExternalMethod methodTemplate[] =
    {
        /* 0 */  { NULL, (IOMethod) &IOAccelerationUserClient::extCreate,
                    kIOUCScalarIScalarO, 2, 1 },
        /* 1 */  { NULL, (IOMethod) &IOAccelerationUserClient::extDestroy,
                    kIOUCScalarIScalarO, 2, 0 },
    };

    if (index >= (sizeof(methodTemplate) / sizeof(methodTemplate[0])))
        return (NULL);

    *targetP = this;

    return ((IOExternalMethod *)(methodTemplate + index));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static 
IOReturn _CreateID(queue_head_t * taskList, IOOptionBits options,
                    IOAccelID requestedID, IOAccelID * idOut)
{
    IOReturn          err;
    Boolean           found;
    IOAccelIDRecord * record;
    IOAccelIDRecord * dup;

    record = IONew(IOAccelIDRecord, 1);
    record->retain = 1;

    IOLockLock(gLock);

    gTotalCount++;

    do
    {
        if (kIOAccelSpecificID & options)
        {
            if ((requestedID > 4095) || (requestedID < -4096))
            {
                err = kIOReturnExclusiveAccess;
                break;
            }
    
            found = false;
            queue_iterate(&gGlobalList,
                            dup,
                            IOAccelIDRecord *,
                            glob_link)
            {
                found = (dup->id == requestedID);
                if (found)
                    break;
            }
    
            if (found)
            {
                err = kIOReturnExclusiveAccess;
                break;
            }
    
            record->id = requestedID;
        }
        else
        {
            record->id = ((IOAccelID) (intptr_t) record) ^ (kTweakBits & gTweak++);
        }

        if (taskList)
        {
            queue_enter(taskList, record,
                            IOAccelIDRecord *, task_link);
        }
        else
            record->task_link.next = 0;

        queue_enter(&gGlobalList, record,
                        IOAccelIDRecord *, glob_link);

        *idOut = record->id;
        err = kIOReturnSuccess;
    }
    while (false);

    if (kIOReturnSuccess != err)
        gTotalCount--;

    IOLockUnlock(gLock);

    if (kIOReturnSuccess != err)
    {
        IODelete(record, IOAccelIDRecord, 1);
    }
    return (err);
}

IOReturn IOAccelerationUserClient::extCreate(IOOptionBits options,
                                                IOAccelID requestedID, IOAccelID * idOut)
{
    return (_CreateID(&fTaskList, options, requestedID, idOut));
}

IOReturn IOAccelerationUserClient::extDestroy(IOOptionBits options, IOAccelID id)
{
    IOAccelIDRecord * record;
    bool found = false;
    IOLockLock(gLock);

    queue_iterate(&fTaskList,
                    record,
                    IOAccelIDRecord *,
                    task_link)
    {
        found = (record->id == id);
        if (found)
        {
            queue_remove(&fTaskList,
                            record,
                            IOAccelIDRecord *,
                            task_link);
            if (--record->retain)
                record->task_link.next = 0;
            else
            {
                queue_remove(&gGlobalList,
                                record,
                                IOAccelIDRecord *,
                                glob_link);
                gTotalCount--;
                IODelete(record, IOAccelIDRecord, 1);
            }
            break;
        }
    }

    IOLockUnlock(gLock);

    return (found ? kIOReturnSuccess : kIOReturnBadMessageID);
}

IOReturn
IOAccelerator::createAccelID(IOOptionBits options, IOAccelID * identifier)
{
    return (_CreateID(0, options, *identifier, identifier));
}

IOReturn
IOAccelerator::retainAccelID(IOOptionBits options, IOAccelID id)
{
    IOAccelIDRecord * record;
    bool found = false;
    IOLockLock(gLock);

    queue_iterate(&gGlobalList,
                    record,
                    IOAccelIDRecord *,
                    glob_link)
    {
        found = (record->id == id);
        if (found)
        {
            record->retain++;
            break;
        }
    }

    IOLockUnlock(gLock);

    return (found ? kIOReturnSuccess : kIOReturnBadMessageID);
}

IOReturn
IOAccelerator::releaseAccelID(IOOptionBits options, IOAccelID id)
{
    IOAccelIDRecord * record;
    bool found = false;
    IOLockLock(gLock);

    queue_iterate(&gGlobalList,
                    record,
                    IOAccelIDRecord *,
                    glob_link)
    {
        found = (record->id == id);
        if (found)
        {
            if (!--record->retain)
            {
                if (record->task_link.next)
                    panic("IOAccelerator::releaseID task_link");

                queue_remove(&gGlobalList,
                                record,
                                IOAccelIDRecord *,
                                glob_link);
                gTotalCount--;
                IODelete(record, IOAccelIDRecord, 1);
            }
            break;
        }
    }

    IOLockUnlock(gLock);

    return (found ? kIOReturnSuccess : kIOReturnBadMessageID);
}

