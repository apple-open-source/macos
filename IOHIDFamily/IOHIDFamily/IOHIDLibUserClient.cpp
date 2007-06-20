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
#include <sys/systm.h>
#include <sys/proc.h>
#include <kern/task.h>
#include <mach/port.h>
#include <mach/message.h>

#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOService.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOKitKeysPrivate.h>
#include "IOHIDLibUserClient.h"
#include "IOHIDDevice.h"
#include "IOHIDEventQueue.h"


#define super IOUserClient

struct AsyncParam {
    OSAsyncReference 		fAsyncRef;
    UInt32                  fMax;
    IOMemoryDescriptor 		*fMem;
    IOHIDReportType         reportType;
};

struct AsyncGateParam {
    OSAsyncReference        asyncRef; 
    IOHIDReportType         reportType; 
    UInt32                  reportID; 
    void *                  reportBuffer;
    UInt32                  reportBufferSize;
    UInt32                  completionTimeOutMS;
};


OSDefineMetaClassAndStructors(IOHIDLibUserClient, IOUserClient);

const IOExternalMethod IOHIDLibUserClient::
sMethods[kIOHIDLibUserClientNumCommands] = {
    { //    kIOHIDLibUserClientOpen
	0,
	(IOMethod) &IOHIDLibUserClient::open,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kIOHIDLibUserClientClose
	0,
	(IOMethod) &IOHIDLibUserClient::close,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { //    kIOHIDLibUserClientCreateQueue
	0,
	(IOMethod) &IOHIDLibUserClient::createQueue,
	kIOUCScalarIScalarO,
	2,
	1
    },
    { //    kIOHIDLibUserClientDisposeQueue
	0,
	(IOMethod) &IOHIDLibUserClient::disposeQueue,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kIOHIDLibUserClientAddElementToQueue
	0,
	(IOMethod) &IOHIDLibUserClient::addElementToQueue,
	kIOUCScalarIScalarO,
	3,
	1
    },
    { //    kIOHIDLibUserClientRemoveElementFromQueue
	0,
	(IOMethod) &IOHIDLibUserClient::removeElementFromQueue,
	kIOUCScalarIScalarO,
	2,
	1
    },
    { //    kIOHIDLibUserClientQueueHasElement
	0,
	(IOMethod) &IOHIDLibUserClient::queueHasElement,
	kIOUCScalarIScalarO,
	2,
	1
    },
    { //    kIOHIDLibUserClientStartQueue
	0,
	(IOMethod) &IOHIDLibUserClient::startQueue,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kIOHIDLibUserClientStopQueue
	0,
	(IOMethod) &IOHIDLibUserClient::stopQueue,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kIOHIDLibUserClientUpdateElementValue
	0,
	(IOMethod) &IOHIDLibUserClient::updateElementValue,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kIOHIDLibUserClientPostElementValue
	0,
	(IOMethod) &IOHIDLibUserClient::postElementValue,
	kIOUCStructIStructO,
	0xffffffff,
	0
    },
    { //    kIOHIDLibUserClientGetReport
	0,
	(IOMethod) &IOHIDLibUserClient::getReport,
	kIOUCScalarIStructO,
	2,
	0xffffffff
    },
    { //    kIOHIDLibUserClientGetReportOOL
	0,
	(IOMethod) &IOHIDLibUserClient::getReportOOL,
	kIOUCStructIStructO,
	sizeof(IOHIDReportReq),
	sizeof(UInt32)
    },
    { //    kIOHIDLibUserClientSetReport
	0,
	(IOMethod) &IOHIDLibUserClient::setReport,
	kIOUCScalarIStructI,
	2,
	0xffffffff
    },
    { //    kIOHIDLibUserClientSetReportOOL
	0,
	(IOMethod) &IOHIDLibUserClient::setReportOOL,
	kIOUCStructIStructO,
	sizeof(IOHIDReportReq),
	0
    },
    { //    kIOHIDLibUserClientDeviceIsValid
    0,
	(IOMethod) &IOHIDLibUserClient::deviceIsValid,
	kIOUCScalarIScalarO,
	0,
	2
    }    
};

const IOExternalAsyncMethod IOHIDLibUserClient::
sAsyncMethods[kIOHIDLibUserClientNumAsyncCommands] = {
    { //	kIOHIDLibUserClientSetAsyncPort
	0,
	(IOAsyncMethod) &IOHIDLibUserClient::setAsyncPort,
	kIOUCScalarIScalarO,
	0,
	0
    },
    { // 	kIOHIDLibUserClientSetQueueAsyncPort
	0,
	(IOAsyncMethod) &IOHIDLibUserClient::setQueueAsyncPort,
	kIOUCScalarIScalarO,
	1,
	0    
    },
    { //    kIOHIDLibUserClientAsyncGetReport
	0,
	(IOAsyncMethod) &IOHIDLibUserClient::asyncGetReport,
	kIOUCScalarIScalarO,
	5,
	0
    },
    { //    kIOHIDLibUserClientAsyncSetReport
	0,
	(IOAsyncMethod) &IOHIDLibUserClient::asyncSetReport,
	kIOUCScalarIScalarO,
	5,
	0
    }
};


bool IOHIDLibUserClient::
initWithTask(task_t owningTask, void * /* security_id */, UInt32 /* type */)
{
    if (!super::init())
	return false;

    fClient = owningTask;    
    task_reference (fClient);

    proc_t p = (proc_t)get_bsdtask_info(fClient);
    fPid = proc_pid(p);

    fQueueSet = OSSet::withCapacity(4);
    if (!fQueueSet)
        return false;
    
    return true;
}

IOReturn IOHIDLibUserClient::clientClose(void)
{
    if ( !isInactive() ) {
        fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::cleanupGated));
        terminate();
    }
    
    return kIOReturnSuccess;
}

bool IOHIDLibUserClient::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::cleanupGated));
    return super::didTerminate(provider, options, defer);
}

void IOHIDLibUserClient::cleanupGated()
{
   if (fClient) {
        task_deallocate(fClient);
        fClient = 0;
    }
   
   if (fNub) {	
   
        // First clear any remaining queues
        setStateForQueues(kHIDQueueStateClear);
        
        // Have been started so we better detach
		
        // make sure device is closed (especially on crash)
        // note radar #2729708 for a more comprehensive fix
        // probably should also subclass clientDied for crash specific code
        fNub->close(this, fCachedOptionBits);        
    }
    
    if ( fResourceNotification ) {
        fResourceNotification->remove();
        fResourceNotification = 0;
    }
    
    if (fResourceES) {
        if ( fWL )
            fWL->removeEventSource(fResourceES);
        fResourceES->release();
        fResourceES = 0;
    }
}

bool IOHIDLibUserClient::start(IOService *provider)
{
    IOCommandGate * cmdGate = NULL;
    
    if (!super::start(provider))
		return false;

    fNub = OSDynamicCast(IOHIDDevice, provider);
    if (!fNub)
		return false;
            
	OSNumber *primaryUsage = OSDynamicCast(OSNumber, fNub->getProperty(kIOHIDPrimaryUsageKey));
	OSNumber *primaryUsagePage = OSDynamicCast(OSNumber, fNub->getProperty(kIOHIDPrimaryUsagePageKey));

	if ((primaryUsagePage && (primaryUsagePage->unsigned32BitValue() == kHIDPage_GenericDesktop)) &&
		(primaryUsage && ((primaryUsage->unsigned32BitValue() == kHIDUsage_GD_Keyboard) || (primaryUsage->unsigned32BitValue() == kHIDUsage_GD_Keypad)))) 
	{
		fNubIsKeyboard = true;
	}

    fWL = getWorkLoop();
    if (!fWL)
        goto ABORT_START;

    fWL->retain();

    cmdGate = IOCommandGate::commandGate(this);
    if (!cmdGate)
        goto ABORT_START;
    
    fWL->addEventSource(cmdGate);
    
    fGate = cmdGate;

    fResourceES = IOInterruptEventSource::interruptEventSource
        (this, OSMemberFunctionCast(IOInterruptEventSource::Action, this, &IOHIDLibUserClient::resourceNotificationGated));
        
    if ( !fResourceES )
        goto ABORT_START;

    fWL->addEventSource(fResourceES);

    // Get notified everytime Root properties change
    fResourceNotification = addNotification(
        gIOPublishNotification, 
        serviceMatching("IOResources"),
        OSMemberFunctionCast(IOServiceNotificationHandler, this, &IOHIDLibUserClient::resourceNotification),
        this);
                    
    if ( !fResourceNotification )
        goto ABORT_START;
    
    return true;

ABORT_START:
    if (fGate) {
        fWL->removeEventSource(fGate);
        fWL->release();
        fWL = 0;
        fGate->release();
        fGate = 0;
    }

    return false;
}

bool IOHIDLibUserClient::resourceNotification(void * refcon, IOService *service)
{
    if (!isInactive() && fResourceES) 
        fResourceES->interruptOccurred(0, 0, 0);
    return true;
}

void IOHIDLibUserClient::resourceNotificationGated()
{
    IOReturn ret = kIOReturnSuccess;
    OSData * data;
    IOService * service = getResourceService();
    
    do {
        // Always force success on seize
        if ( kIOHIDOptionsTypeSeizeDevice & fCachedOptionBits )
            break;
        
        if ( !service ) {
			ret = kIOReturnError;
            break;
		}
            
        data = OSDynamicCast(OSData, service->getProperty(kIOConsoleUsersSeedKey));

        if ( !data || !data->getLength() || !data->getBytesNoCopy()) {
			ret = kIOReturnError;
            break;
		}
            
        UInt64 currentSeed = 0;
        
        switch ( data->getLength() ) {
            case sizeof(UInt8):
                currentSeed = *(UInt8*)(data->getBytesNoCopy());
                break;
            case sizeof(UInt16):
                currentSeed = *(UInt16*)(data->getBytesNoCopy());
                break;
            case sizeof(UInt32):
                currentSeed = *(UInt32*)(data->getBytesNoCopy());
                break;
            case sizeof(UInt64):
            default:
                currentSeed = *(UInt64*)(data->getBytesNoCopy());
                break;
        }
            
        // We should return rather than break so that previous setting is retained
        if ( currentSeed == fCachedConsoleUsersSeed )
            return;
            
        fCachedConsoleUsersSeed = currentSeed;

        ret = clientHasPrivilege(fClient, kIOClientPrivilegeAdministrator);
        if (ret == kIOReturnSuccess) 
            break;

		if ( fNubIsKeyboard ) {
			IOUCProcessToken token;
			token.token = fClient;
			token.pid = fPid;
			ret = clientHasPrivilege(&token, kIOClientPrivilegeSecureConsoleProcess);
		} else {
			ret = clientHasPrivilege(fClient, kIOClientPrivilegeConsoleUser);
		}
    } while (false);
    
    setValid(kIOReturnSuccess == ret);
}

IOExternalMethod *IOHIDLibUserClient::
getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if (index < (UInt32) kIOHIDLibUserClientNumCommands)
    {
	*target = this;
	return (IOExternalMethod *) &sMethods[index];
    }
    else
	return 0;
}

IOExternalAsyncMethod * IOHIDLibUserClient::
getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if (index < (UInt32) kIOHIDLibUserClientNumAsyncCommands)
    {
	*target = this;
	return (IOExternalAsyncMethod *) &sAsyncMethods[index];
    }
    else
	return 0;
}


IOReturn IOHIDLibUserClient::
setAsyncPort(OSAsyncReference asyncRef, void *, void *, void *,
                                        void *, void *, void *)
{
    fWakePort = (mach_port_t) asyncRef[0];
    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::
setQueueAsyncPort(OSAsyncReference asyncRef, void * vInQueue, void *, void *,
                                        void *, void *, void *)
{
    IOHIDEventQueue * eventQueue = (IOHIDEventQueue *) vInQueue;
    
    fQueuePort = (mach_port_t) asyncRef[0];
    
    if ( !eventQueue ) 
        return kIOReturnBadArgument;

    eventQueue->setNotificationPort(fQueuePort);

    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::open(void * flags)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::openGated), flags);
}

IOReturn IOHIDLibUserClient::openGated(IOOptionBits options)
{
    IOReturn ret = kIOReturnNotPrivileged;
    
    ret = clientHasPrivilege(fClient, kIOClientPrivilegeLocalUser);
    if (ret != kIOReturnSuccess)
        ret = clientHasPrivilege(fClient, kIOClientPrivilegeAdministrator);

    if (ret != kIOReturnSuccess)
        return ret;
    
    if (!fNub->IOService::open(this, options))
        return kIOReturnExclusiveAccess;   
		
    fCachedOptionBits = options;

    fCachedConsoleUsersSeed = 0;
	resourceNotificationGated();
	
    return kIOReturnSuccess;
}


IOReturn IOHIDLibUserClient::close()
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::closeGated));
}

IOReturn IOHIDLibUserClient::closeGated()
{
    fNub->close(this, fCachedOptionBits);

    setValid(false);

	fCachedOptionBits = 0;

    // @@@ gvdl: release fWakePort leak them for the time being

    return kIOReturnSuccess;
}


void IOHIDLibUserClient::free()
{
    
    if (fQueueSet) {
        fQueueSet->release();
        fQueueSet = 0;
    }
    
    if (fNub) {
        fNub = 0;
    }

    if (fResourceES) {
        if ( fWL )
            fWL->removeEventSource(fResourceES);
        fResourceES->release();
        fResourceES = 0;
    }

    if (fGate) {
        if (fWL) {
            fWL->removeEventSource(fGate);
        }
        
        fGate->release();
        fGate = 0;
    }

    if ( fWL ) {
        fWL->release();
        fWL = 0;
    }
    super::free();
}

IOReturn IOHIDLibUserClient::message(UInt32 type, IOService * provider, void * argument )
{
    if ( !isInactive() && fGate ) 
        fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::messageGated), (void *)type, provider, argument);
    return super::message(type, provider, argument);
}

IOReturn IOHIDLibUserClient::messageGated(UInt32 type, IOService * provider, void * argument )
{
    IOOptionBits options = (IOOptionBits)argument;
    switch ( type ) {
        case kIOMessageServiceIsRequestingClose:
            if ((options & kIOHIDOptionsTypeSeizeDevice) && (options != fCachedOptionBits))
                setValid(false);
            break;
            
        case kIOMessageServiceWasClosed:
            if ((options & kIOHIDOptionsTypeSeizeDevice) && (options != fCachedOptionBits)) {
                // instead of calling set valid, let's make sure we still have
                // permission through the resource notification
                fCachedConsoleUsersSeed = 0;
                resourceNotificationGated();
            }
            break;
    };
    
    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::registerNotificationPort(mach_port_t port, UInt32 type, UInt32	refCon)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::registerNotificationPortGated), (void *)port, (void *)type, (void *)refCon);
}

IOReturn IOHIDLibUserClient::registerNotificationPortGated(mach_port_t port, UInt32 type, UInt32	refCon)
{
    IOReturn kr = kIOReturnSuccess;
    
    switch ( type ) {
        case kIOHIDLibUserClientDeviceValidPortType:
            fValidPort = port;

            static struct _notifyMsg init_msg = { {
                MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0),
                sizeof (struct _notifyMsg),
                MACH_PORT_NULL,
                MACH_PORT_NULL,
                0,
                0
            } };
               
            if ( fValidMessage ) { 
                IOFree(fValidMessage, sizeof (struct _notifyMsg));
                fValidMessage = NULL;
            }

            if ( !fValidPort ) 
                break;
                
            if ( !(fValidMessage = IOMalloc( sizeof(struct _notifyMsg))) ) {
                kr = kIOReturnNoMemory;
                break;
            }
                
            // Initialize the events available message.
            *((struct _notifyMsg *)fValidMessage) = init_msg;

            ((struct _notifyMsg *)fValidMessage)->h.msgh_remote_port = fValidPort;
            
            dispatchMessage(fValidMessage);
            
            break;
        default:
            kr = kIOReturnUnsupported;
            break;
    };

    return kr;
}

void IOHIDLibUserClient::setValid(bool state)
{
    if (fValid == state)
        return;

    if ( !state ) {    
        // unmap this memory
        if (fNub && !isInactive()) {
            IOMemoryDescriptor * mem;
            IOMemoryMap * map;

            mem = fNub->getMemoryWithCurrentElementValues();
            
            if ( mem ) {
                map = removeMappingForDescriptor(mem);
                
                if ( map )
                    map->release();
            }
        }
        fGeneration++;
    }
    
    // set the queue states
    setStateForQueues(state ? kHIDQueueStateEnable : kHIDQueueStateDisable);
    
    // dispatch message
    dispatchMessage(fValidMessage);
    
    fValid = state;
}

IOReturn IOHIDLibUserClient::dispatchMessage(void * message)
{
    IOReturn ret = kIOReturnError;
    mach_msg_header_t * msgh = (mach_msg_header_t *)message;
    if( msgh) {
        ret = mach_msg_send_from_kernel( msgh, msgh->msgh_size);
        switch ( ret ) {
            case MACH_SEND_TIMED_OUT:/* Already has a message posted */
            case MACH_MSG_SUCCESS:	/* Message is posted */
                break;
        };
    }
    return ret;
}

void IOHIDLibUserClient::setStateForQueues(UInt32 state, IOOptionBits options)
{
    OSCollectionIterator * iterator = OSCollectionIterator::withCollection(fQueueSet);
    
    if (iterator)
    {
        IOHIDEventQueue * queue;
        while (queue = (IOHIDEventQueue *)iterator->getNextObject())
        {
            switch (state) {
                case kHIDQueueStateEnable:
                    queue->enable();
                    break;
                case kHIDQueueStateDisable:
                    queue->disable();
                    break;
                case kHIDQueueStateClear:
                    fNub->stopEventDelivery(queue);
                    break;
            };
        }
        iterator->release();
    }
}


IOReturn IOHIDLibUserClient::clientMemoryForType (	
                                    UInt32                  type,
                                    IOOptionBits *          options,
                                    IOMemoryDescriptor ** 	memory )
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::clientMemoryForTypeGated), (void *)type, (void *)options, (void *)memory);
}

IOReturn IOHIDLibUserClient::clientMemoryForTypeGated(	
                                    UInt32                  type,
                                    IOOptionBits *          options,
                                    IOMemoryDescriptor ** 	memory )
{
    IOReturn                ret             = kIOReturnNoMemory;
    IOMemoryDescriptor *    memoryToShare   = NULL;
    IOHIDEventQueue *       queue           = NULL;
    
    // if the type is element values, then get that
    if (type == kIOHIDLibUserClientElementValuesType)
    {
        // if we can get an element values ptr
        if (fValid && fNub && !isInactive())
            memoryToShare = fNub->getMemoryWithCurrentElementValues();
    }
    // otherwise, the type is an object pointer (evil hack alert - see header)
    // evil hack, the type is an IOHIDEventQueue ptr (as returned by createQueue)
    else if (queue = OSDynamicCast(IOHIDEventQueue, (OSObject *)type))
    {
        memoryToShare = queue->getMemoryDescriptor();
    }
    
    // if we got some memory
    if (memoryToShare)
    {
        // Memory will be released by user client
        // when last map is destroyed.

        memoryToShare->retain();

        ret = kIOReturnSuccess;
    }
    
    // set the result
    *options = 0;
    *memory  = memoryToShare;
        
    return ret;
}

IOReturn IOHIDLibUserClient::deviceIsValid(void * vOutStatus, void * vOutGeneration)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::deviceIsValidGated), vOutStatus, vOutGeneration);
}

IOReturn IOHIDLibUserClient::deviceIsValidGated(void * vOutStatus, void * vOutGeneration)
{
    UInt32 *pStatus = (UInt32 *)vOutStatus;
    UInt32 *pGeneration = (UInt32 *)vOutGeneration;
    
    if ( pStatus )
        *pStatus = fValid;
        
    if ( pGeneration )
        *pGeneration = fGeneration;
    
    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::createQueue(void * vInFlags, void * vInDepth, void * vOutQueue)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::createQueueGated), vInFlags, vInDepth, vOutQueue);
}

IOReturn IOHIDLibUserClient::createQueueGated(void * vInFlags, void * vInDepth, void * vOutQueue)
{
    // UInt32	flags = (UInt32) vInFlags;
    UInt32	depth = (UInt32) vInDepth;
    void **	outQueue = (void **) vOutQueue;

    // create the queue (fudge it a bit bigger than requested)
    IOHIDEventQueue * eventQueue = IOHIDEventQueue::withEntries (depth+1, DEFAULT_HID_ENTRY_SIZE);

    if ( !fValid && eventQueue )
        eventQueue->disable();

    // set out queue
    *outQueue = eventQueue;
            
    // add the queue to the set
    fQueueSet->setObject(eventQueue);
    
    eventQueue->release();
    
    return kIOReturnSuccess;
}


IOReturn IOHIDLibUserClient::disposeQueue(void * vInQueue)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::disposeQueueGated), vInQueue);
}

IOReturn IOHIDLibUserClient::disposeQueueGated(void * vInQueue)
{
    IOReturn ret = kIOReturnSuccess;

    // parameter typing
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;

    // remove this queue from all elements that use it
    if (fNub && !isInactive())
        ret = fNub->stopEventDelivery (queue);
    
    // remove the queue from the set
    fQueueSet->removeObject(queue);

    return kIOReturnSuccess;
}

    // Add an element to a queue
IOReturn IOHIDLibUserClient::addElementToQueue(void * vInQueue, void * vInElementCookie, void * vInFlags, void *vSizeChange)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::addElementToQueueGated), vInQueue, vInElementCookie, vInFlags, vSizeChange);
}

IOReturn IOHIDLibUserClient::addElementToQueueGated(void * vInQueue, void * vInElementCookie, void * vInFlags, void *vSizeChange)
{
    IOReturn    ret     = kIOReturnSuccess;
    UInt32      size    = 0;
    int *       sizeChange  = (int *) vSizeChange;

    // parameter typing
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;
    IOHIDElementCookie elementCookie = (IOHIDElementCookie) vInElementCookie;
    // UInt32 flags = (UInt32) vInFlags;
    
    size = (queue) ? queue->getEntrySize() : 0;
    
    // add the queue to the element's queues
    if (fNub && !isInactive())
        ret = fNub->startEventDelivery (queue, elementCookie);
        
    *sizeChange = (queue && (size != queue->getEntrySize()));
    
    return ret;
}   
    // remove an element from a queue
IOReturn IOHIDLibUserClient::removeElementFromQueue (void * vInQueue, void * vInElementCookie, void * vSizeChange)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::removeElementFromQueueGated), vInQueue, vInElementCookie, vSizeChange);
}

IOReturn IOHIDLibUserClient::removeElementFromQueueGated (void * vInQueue, void * vInElementCookie, void * vSizeChange)
{
    IOReturn    ret     = kIOReturnSuccess;
    UInt32      size    = 0;
    int *       sizeChange = (int *) vSizeChange;

    // parameter typing
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;
    IOHIDElementCookie elementCookie = (IOHIDElementCookie) vInElementCookie;

    size = (queue) ? queue->getEntrySize() : 0;

    // remove the queue from the element's queues
    if (fNub && !isInactive())
        ret = fNub->stopEventDelivery (queue, elementCookie);

    *sizeChange = (queue && (size != queue->getEntrySize()));
    
    return ret;
}    
    // Check to see if a queue has an element
IOReturn IOHIDLibUserClient::queueHasElement (void * vInQueue, void * vInElementCookie, void * vOutHasElement)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::queueHasElementGated), vInQueue, vInElementCookie, vOutHasElement);
}

IOReturn IOHIDLibUserClient::queueHasElementGated (void * vInQueue, void * vInElementCookie, void * vOutHasElement)
{
    IOReturn ret = kIOReturnSuccess;

    // parameter typing
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;
    IOHIDElementCookie elementCookie = (IOHIDElementCookie) vInElementCookie;
    int * outHasElement = (int *) vOutHasElement;

    // check to see if that element is feeding that queue
    bool hasElement = false;
    
    if (fNub && !isInactive())
        ret = fNub->checkEventDelivery (queue, elementCookie, &hasElement);
    
    // set return
    *outHasElement = hasElement;
    
    return ret;
}    
    // start a queue
IOReturn IOHIDLibUserClient::startQueue (void * vInQueue)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::startQueueGated), vInQueue);
}

IOReturn IOHIDLibUserClient::startQueueGated (void * vInQueue)
{
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;

    // start the queue
    queue->start();

    return kIOReturnSuccess;
}    
    // stop a queue
IOReturn IOHIDLibUserClient::stopQueue (void * vInQueue)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::stopQueueGated), vInQueue);
}

IOReturn IOHIDLibUserClient::stopQueueGated (void * vInQueue)
{
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;

    // stop the queue
    queue->stop();

    return kIOReturnSuccess;
}

    // update the feature element value
IOReturn IOHIDLibUserClient::updateElementValue (void * cookie)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::updateElementValueGated), cookie);
}

IOReturn IOHIDLibUserClient::updateElementValueGated (void * cookie)
{
    IOReturn			ret = kIOReturnError;
    
    if (fNub && !isInactive())
        ret = fNub->updateElementValues(&cookie, 1);
    
    return ret;
}

    // Set the element values
IOReturn IOHIDLibUserClient::postElementValue (void * cookies, void * cookiesBytes)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::postElementValueGated), cookies, cookiesBytes);
}

IOReturn IOHIDLibUserClient::postElementValueGated (void * cookies, void * cookiesBytes)
{
    IOReturn	ret = kIOReturnError;
    UInt32	numCookies = ((UInt32)cookiesBytes) / sizeof(UInt32);
        
    if (fNub && !isInactive())
        ret = fNub->postElementValues((IOHIDElementCookie *)cookies, numCookies);
            
    return ret;
}

IOReturn IOHIDLibUserClient::getReport (void *vReportType, void *vReportID, void *vReportBuffer, void *vReportBufferSize)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::getReportGated), vReportType, vReportID, vReportBuffer, vReportBufferSize);
}

IOReturn IOHIDLibUserClient::getReportGated (IOHIDReportType reportType, 
                                            UInt32 reportID, 
                                            void *reportBuffer, 
                                            UInt32 *reportBufferSize)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;
        
    if (fNub && !isInactive())
    {
        mem = IOMemoryDescriptor::withAddress(reportBuffer, *reportBufferSize, kIODirectionIn);
        if(mem)
        { 
            *reportBufferSize = 0;
            ret = fNub->getReport(mem, reportType, reportID);
            
            // make sure the element values are updated.
            if (ret == kIOReturnSuccess)
                fNub->handleReport(mem, reportType, kIOHIDReportOptionNotInterrupt);
                
            *reportBufferSize = mem->getLength();
            mem->release();
        }
        else
            ret =  kIOReturnNoMemory;
    }
    else
        ret = kIOReturnNotAttached;

    return ret;
}

IOReturn IOHIDLibUserClient::getReportOOL( void *vReqIn, void *vSizeOut, void * vInCount, void *vOutCount)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::getReportOOLGated), vReqIn, vSizeOut, vInCount, vOutCount);
}

IOReturn IOHIDLibUserClient::getReportOOLGated( IOHIDReportReq *reqIn, 
                                                UInt32 *sizeOut, 
                                                IOByteCount inCount, 
                                                IOByteCount *outCount)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;
        
    if (fNub && !isInactive())
    {
        *sizeOut = 0;
        mem = IOMemoryDescriptor::withAddress((vm_address_t)reqIn->reportBuffer, reqIn->reportBufferSize, kIODirectionIn, fClient);
        if(mem)
        { 
            ret = mem->prepare();
            if(ret == kIOReturnSuccess)
                ret = fNub->getReport(mem, (IOHIDReportType)(reqIn->reportType), reqIn->reportID);
                
            // make sure the element values are updated.
            if (ret == kIOReturnSuccess)
                fNub->handleReport(mem, (IOHIDReportType)(reqIn->reportType), kIOHIDReportOptionNotInterrupt);
                
            *sizeOut = mem->getLength();
            mem->complete();
            mem->release();
        }
        else
            ret =  kIOReturnNoMemory;
    }
    else
        ret = kIOReturnNotAttached;

    return ret;

}

IOReturn IOHIDLibUserClient::setReport (void *vReportType, void *vReportID, void *vReportBuffer, void *vReportBufferSize)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::setReportGated), vReportType, vReportID, vReportBuffer, vReportBufferSize);
}

IOReturn IOHIDLibUserClient::setReportGated( IOHIDReportType reportType, 
                                        UInt32 reportID, 
                                        void *reportBuffer, 
                                        UInt32 reportBufferSize)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;

    if (fNub && !isInactive())
    {
        mem = IOMemoryDescriptor::withAddress(reportBuffer, reportBufferSize, kIODirectionOut);
        if(mem) 
        {
            ret = fNub->setReport(mem, reportType, reportID);
            // make sure the element values are updated.
            if (ret == kIOReturnSuccess)
                fNub->handleReport(mem, reportType, kIOHIDReportOptionNotInterrupt);
                
            mem->release();
        }
        else
            ret = kIOReturnNoMemory;
    }
    else
        ret = kIOReturnNotAttached;

    return ret;
}

IOReturn IOHIDLibUserClient::setReportOOL (void *vReq, void *vInCount)
{
    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::setReportOOLGated), vReq, vInCount);
}

IOReturn IOHIDLibUserClient::setReportOOLGated (IOHIDReportReq *req, IOByteCount inCount)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;

    if (fNub && !isInactive())
    {
        mem = IOMemoryDescriptor::withAddress((vm_address_t)req->reportBuffer, req->reportBufferSize, kIODirectionOut, fClient);
        if(mem) 
        {
            ret = mem->prepare();
            if(ret == kIOReturnSuccess)
                ret = fNub->setReport(mem, (IOHIDReportType)(req->reportType), req->reportID);
            
            // make sure the element values are updated.
            if (ret == kIOReturnSuccess)
                fNub->handleReport(mem, (IOHIDReportType)(req->reportType), kIOHIDReportOptionNotInterrupt);
            
            mem->complete();
            mem->release();
        }
        else
            ret = kIOReturnNoMemory;
    }
    else
        ret = kIOReturnNotAttached;

    return ret;

}


IOReturn IOHIDLibUserClient::asyncGetReport(OSAsyncReference asyncRef, 
                                            IOHIDReportType reportType, 
                                            UInt32 reportID, 
                                            void *reportBuffer,
                                            UInt32 reportBufferSize, 
                                            UInt32 completionTimeOutMS)
{
    AsyncGateParam param;
    
    bcopy(asyncRef, param.asyncRef, sizeof(OSAsyncReference));
    param.reportType            = reportType;
    param.reportID              = reportID;
    param.reportBuffer          = reportBuffer;
    param.reportBufferSize      = reportBufferSize;
    param.completionTimeOutMS   = completionTimeOutMS;

    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::asyncGetReportGated), &param);
}

IOReturn IOHIDLibUserClient::asyncGetReportGated ( void * param )
{
    IOHIDReportType         reportType          = ((AsyncGateParam *)param)->reportType; 
    UInt32                  reportID            = ((AsyncGateParam *)param)->reportID; 
    void *                  reportBuffer        = ((AsyncGateParam *)param)->reportBuffer;
    UInt32                  reportBufferSize    = ((AsyncGateParam *)param)->reportBufferSize;
    UInt32                  completionTimeOutMS = ((AsyncGateParam *)param)->completionTimeOutMS;
    IOReturn                ret;
    IOHIDCompletion         tap;
    IOMemoryDescriptor *	mem = NULL;
    AsyncParam *            pb = NULL;

    retain();
    
    if (fNub && !isInactive())
    {
        do {
            mem = IOMemoryDescriptor::withAddress((vm_address_t)reportBuffer, reportBufferSize, kIODirectionIn, fClient);
            if(!mem) 
            {
                ret = kIOReturnNoMemory;
                break;
            }
            ret = mem->prepare();
            if(ret != kIOReturnSuccess)
                break;
    
            pb = (AsyncParam *)IOMalloc(sizeof(AsyncParam));
            if(!pb) 
            {
                ret = kIOReturnNoMemory;
                break;
            }
    
            bcopy(((AsyncGateParam *)param)->asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
            pb->fMax = reportBufferSize;
            pb->fMem = mem;
            pb->reportType = reportType;
            tap.target = this;
            tap.action = OSMemberFunctionCast(IOHIDCompletionAction, this, &IOHIDLibUserClient::ReqComplete);
            tap.parameter = pb;
            ret = fNub->getReport(mem, reportType, reportID, completionTimeOutMS, &tap);
        } while (false);
    }
    else
        ret = kIOReturnNotAttached;
    
    if(ret != kIOReturnSuccess) 
    {
	if(mem) 
	{
	    mem->complete();
	    mem->release();
	}
	if(pb)
	    IOFree(pb, sizeof(*pb));
	
        release();
    }
    return ret;

}
                            
IOReturn IOHIDLibUserClient::asyncSetReport(OSAsyncReference asyncRef, 
                                            IOHIDReportType reportType, 
                                            UInt32 reportID, 
                                            void *reportBuffer,
                                            UInt32 reportBufferSize, 
                                            UInt32 completionTimeOutMS)
{
    AsyncGateParam param;
    
    bcopy(asyncRef, param.asyncRef, sizeof(OSAsyncReference));
    param.reportType            = reportType;
    param.reportID              = reportID;
    param.reportBuffer          = reportBuffer;
    param.reportBufferSize      = reportBufferSize;
    param.completionTimeOutMS   = completionTimeOutMS;

    return fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::asyncSetReportGated), &param);
}

IOReturn IOHIDLibUserClient::asyncSetReportGated ( void * param )
{
    IOHIDReportType         reportType          = ((AsyncGateParam *)param)->reportType; 
    UInt32                  reportID            = ((AsyncGateParam *)param)->reportID; 
    void *                  reportBuffer        = ((AsyncGateParam *)param)->reportBuffer;
    UInt32                  reportBufferSize    = ((AsyncGateParam *)param)->reportBufferSize;
    UInt32                  completionTimeOutMS = ((AsyncGateParam *)param)->completionTimeOutMS;
    IOReturn                ret;
    IOHIDCompletion         tap;
    IOMemoryDescriptor *	mem = NULL;
    AsyncParam *            pb = NULL;

    retain();

    if (fNub && !isInactive())
    {
        do {
            mem = IOMemoryDescriptor::withAddress((vm_address_t)reportBuffer, reportBufferSize, kIODirectionOut, fClient);
            if(!mem) 
            {
                ret = kIOReturnNoMemory;
                break;
            }
            ret = mem->prepare();
            if(ret != kIOReturnSuccess)
                break;
    
            pb = (AsyncParam *)IOMalloc(sizeof(AsyncParam));
            if(!pb) 
            {
                ret = kIOReturnNoMemory;
                break;
            }
    
            bcopy(((AsyncGateParam *)param)->asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
            pb->fMax = reportBufferSize;
            pb->fMem = mem;
            pb->reportType = reportType;
            tap.target = this;
            tap.action = OSMemberFunctionCast(IOHIDCompletionAction, this, &IOHIDLibUserClient::ReqComplete);
            tap.parameter = pb;
            ret = fNub->setReport(mem, reportType, reportID, completionTimeOutMS, &tap);
        } while (false);
    }
    else
        ret = kIOReturnNotAttached;
    
    if(ret != kIOReturnSuccess) 
    {
	if(mem) 
	{
	    mem->complete();
	    mem->release();
	}
	if(pb)
	    IOFree(pb, sizeof(*pb));
            
        release();
    }
    return ret;
}
                                
void IOHIDLibUserClient::ReqComplete(void *param, IOReturn res, UInt32 remaining)
{
    fGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDLibUserClient::ReqCompleteGated), param, (void *)res, (void *)remaining);
}

void IOHIDLibUserClient::ReqCompleteGated(void *param, IOReturn res, UInt32 remaining)
{
    void *	args[1];
    AsyncParam * pb = (AsyncParam *)param;

    if(res == kIOReturnSuccess) 
    {
        args[0] = (void *)(pb->fMax - remaining);
        
        // make sure the element values are updated.
        if (fNub && !isInactive())
            fNub->handleReport(pb->fMem, pb->reportType, kIOHIDReportOptionNotInterrupt);
    }
    else 
    {
        args[0] = 0;
    }
    if (pb->fMem)
    {
        pb->fMem->complete();
        pb->fMem->release();
    }

    sendAsyncResult(pb->fAsyncRef, res, args, 1);

    IOFree(pb, sizeof(*pb));

    release();
}
