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

#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOService.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOWorkLoop.h>

#include "IOHIDLibUserClient.h"
#include "IOHIDDevice.h"
#include "IOHIDEventQueue.h"


#define super IOUserClient

struct AsyncParam {
    OSAsyncReference 		fAsyncRef;
    UInt32 			fMax;
    IOMemoryDescriptor 		*fMem;
    IOHIDReportType		reportType;
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
	0
    },
    { //    kIOHIDLibUserClientRemoveElementFromQueue
	0,
	(IOMethod) &IOHIDLibUserClient::removeElementFromQueue,
	kIOUCScalarIScalarO,
	2,
	0
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
    fGate = 0;
    fNubIsTerminated = false;
    fCachedOptionBits = 0;
    
    task_reference (fClient);
    
    return true;
}

IOReturn IOHIDLibUserClient::clientClose(void)
{
   if (fClient) {
        task_deallocate(fClient);
        fClient = 0;
    }
   
   if (fNub) {	
        // Have been started so we better detach
		
        // make sure device is closed (especially on crash)
        // note radar #2729708 for a more comprehensive fix
        // probably should also subclass clientDied for crash specific code
        fNub->close(this, fCachedOptionBits);
        
        detach(fNub);
    }

    return kIOReturnSuccess;
}

bool IOHIDLibUserClient::start(IOService *provider)
{
    IOWorkLoop *wl = 0;

    if (!super::start(provider))
	return false;

    fNub = OSDynamicCast(IOHIDDevice, provider);
    if (!fNub)
	return false;
        
    fNub->retain();
    
    fGate = 0;

    wl = getWorkLoop();
    if (!wl)
        goto ABORT_START;

    fGate = IOCommandGate::commandGate(this);
    if (!fGate)
        goto ABORT_START;
    
    wl->retain();
    wl->addEventSource(fGate);
    
    return true;

ABORT_START:
    if (fGate) {
        wl->removeEventSource(fGate);
        wl->release();
        fGate->release();
        fGate = 0;
    }

    return false;
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

IOReturn IOHIDLibUserClient::
open(void * flags, void *, void *, void *, void *, void *)
{
    IOReturn		ret = kIOReturnSuccess;
    IOOptionBits 	options = (IOOptionBits)flags;
    
    ret = clientHasPrivilege(fClient, kIOClientPrivilegeLocalUser);
    if (ret != kIOReturnSuccess)
    {
        ret = clientHasPrivilege(fClient, kIOClientPrivilegeAdministrator);
        if (ret != kIOReturnSuccess)
            return ret;
    }

    if (!fNub->open(this, options))
	return kIOReturnExclusiveAccess;
        
    fCachedOptionBits = options;

    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::
close(void *, void *, void *, void *, void *, void *gated)
{
    if ( ! (bool) gated ) {
    
        return fGate->runAction(closeAction);
    }
    else /* gated */ {
    
        fNub->close(this, fCachedOptionBits);
    
        // @@@ gvdl: release fWakePort leak them for the time being
    
        return kIOReturnSuccess;
    }
    
    return kIOReturnSuccess;
}

bool
IOHIDLibUserClient::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{    
    fNub->close(this, fCachedOptionBits);
    
    fNubIsTerminated = true;
    
    return super::didTerminate(provider, options, defer);
}

bool
IOHIDLibUserClient::requestTerminate( IOService * provider, IOOptionBits options )
{
    return false;
}


void IOHIDLibUserClient::free()
{
    IOWorkLoop *wl;

    if (fGate) 
    {
        wl = fGate->getWorkLoop();
        if (wl) 
            wl->release();
        
        fGate->release();
        fGate = 0;
    }
    
    if (fNub)
    {
        fNub->release();
        fNub = 0;
    }
    
    super::free();
}


IOReturn IOHIDLibUserClient::closeAction
    (OSObject *self, void *, void *, void *, void *)
{
    IOHIDLibUserClient *me = (IOHIDLibUserClient *) self;
    return me->close(0, 0, 0, 0, 0, /* gated = */ (void *) true);
}

IOReturn IOHIDLibUserClient::
clientMemoryForType (	UInt32			type,
                        IOOptionBits *		options,
                        IOMemoryDescriptor ** 	memory )
{
    IOReturn             ret = kIOReturnNoMemory;
    IOMemoryDescriptor * memoryToShare = NULL;
    
    // if the type is element values, then get that
    if (type == IOHIDLibUserClientElementValuesType)
    {
        // if we can get an element values ptr
        if (fNub && !isInactive() && !fNubIsTerminated)
            memoryToShare = fNub->getMemoryWithCurrentElementValues();
    }
    // otherwise, the type is an object pointer (evil hack alert - see header)
    else
    {
        // evil hack, the type is an IOHIDEventQueue ptr (as returned by createQueue)
        IOHIDEventQueue * queue = (IOHIDEventQueue *) type;
        
        // get queue memory
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

IOReturn IOHIDLibUserClient::
createQueue(void * vInFlags, void * vInDepth, void * vOutQueue, void *, void *, void * gated)
{
    // UInt32	flags = (UInt32) vInFlags;
    UInt32	depth = (UInt32) vInDepth;
    void **	outQueue = (void **) vOutQueue;

    // create the queue (fudge it a bit bigger than requested)
    IOHIDEventQueue * eventQueue = IOHIDEventQueue::withEntries (depth+1, 
                            sizeof(IOHIDElementValue) + sizeof(void *));
    
    // set out queue
    *outQueue = eventQueue;
    
    return kIOReturnSuccess;
}

IOReturn IOHIDLibUserClient::
disposeQueue(void * vInQueue, void *, void *, void *, void *, void * gated)
{
    IOReturn ret = kIOReturnSuccess;

    // parameter typing
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;

    // remove this queue from all elements that use it
    if (fNub && !isInactive() && !fNubIsTerminated)
        ret = fNub->stopEventDelivery (queue);
    
    // release this queue
    queue->release();

    return kIOReturnSuccess;
}

    // Add an element to a queue
IOReturn IOHIDLibUserClient::
addElementToQueue(void * vInQueue, void * vInElementCookie, 
                            void * vInFlags, void *, void *, void * gated)
{
    IOReturn ret = kIOReturnSuccess;

    // parameter typing
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;
    IOHIDElementCookie elementCookie = (IOHIDElementCookie) vInElementCookie;
    // UInt32 flags = (UInt32) vInFlags;
    
    // add the queue to the element's queues
    if (fNub && !isInactive() && !fNubIsTerminated)
        ret = fNub->startEventDelivery (queue, elementCookie);
    
    return ret;
}   
    // remove an element from a queue
IOReturn IOHIDLibUserClient::
removeElementFromQueue (void * vInQueue, void * vInElementCookie, 
                            void *, void *, void *, void * gated)
{
    IOReturn ret = kIOReturnSuccess;

    // parameter typing
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;
    IOHIDElementCookie elementCookie = (IOHIDElementCookie) vInElementCookie;

    // remove the queue from the element's queues
    if (fNub && !isInactive() && !fNubIsTerminated)
        ret = fNub->stopEventDelivery (queue, elementCookie);
    
    return ret;
}    
    // Check to see if a queue has an element
IOReturn IOHIDLibUserClient::
queueHasElement (void * vInQueue, void * vInElementCookie, 
                            void * vOutHasElement, void *, void *, void * gated)
{
    IOReturn ret = kIOReturnSuccess;

    // parameter typing
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;
    IOHIDElementCookie elementCookie = (IOHIDElementCookie) vInElementCookie;
    Boolean * outHasElement = (Boolean *) vOutHasElement;

    // check to see if that element is feeding that queue
    bool hasElement = false;
    
    if (fNub && !isInactive() && !fNubIsTerminated)
        ret = fNub->checkEventDelivery (queue, elementCookie, &hasElement);
    
    // set return
    *outHasElement = hasElement;
    
    return ret;
}    
    // start a queue
IOReturn IOHIDLibUserClient::
startQueue (void * vInQueue, void *, void *, 
                            void *, void *, void * gated)
{
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;

    // start the queue
    queue->start();

    return kIOReturnSuccess;
}    
    // stop a queue
IOReturn IOHIDLibUserClient::
stopQueue (void * vInQueue, void *, void *, 
                            void *, void *, void * gated)
{
    IOHIDEventQueue * queue = (IOHIDEventQueue *) vInQueue;

    // stop the queue
    queue->stop();

    return kIOReturnSuccess;
}

    // update the feature element value
IOReturn IOHIDLibUserClient::
updateElementValue (void * cookie, void *, void *, 
                            void *, void *, void * )
{
    IOReturn			ret = kIOReturnError;
    
    if (fNub && !isInactive() && !fNubIsTerminated)
        ret = fNub->updateElementValues(&cookie, 1);
    
    return ret;
}

    // Set the element values
IOReturn IOHIDLibUserClient::
postElementValue (void * cookies, void * cookiesBytes, void *, 
                            void *, void *, void * )
{
    IOReturn	ret = kIOReturnError;
    UInt32	numCookies = ((UInt32)cookiesBytes) / sizeof(UInt32);
        
    if (fNub && !isInactive() && !fNubIsTerminated)
        ret = fNub->postElementValues((IOHIDElementCookie *)cookies, numCookies);
            
    return ret;
}

IOReturn IOHIDLibUserClient::
getReport (IOHIDReportType reportType, UInt32 reportID, 
            void *reportBuffer, UInt32 *reportBufferSize)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;
        
    if (fNub && !isInactive() && !fNubIsTerminated)
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

IOReturn IOHIDLibUserClient::
getReportOOL(  IOHIDReportReq *reqIn, 
                        UInt32 *sizeOut, 
                        IOByteCount inCount, 
                        IOByteCount *outCount)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;
        
    if (fNub && !isInactive() && !fNubIsTerminated)
    {
        *sizeOut = 0;
        mem = IOMemoryDescriptor::withAddress(reqIn->reportBuffer, reqIn->reportBufferSize, kIODirectionIn, fClient);
        if(mem)
        { 
            ret = mem->prepare();
            if(ret == kIOReturnSuccess)
                ret = fNub->getReport(mem, reqIn->reportType, reqIn->reportID);
                
            // make sure the element values are updated.
            if (ret == kIOReturnSuccess)
                fNub->handleReport(mem, reqIn->reportType, kIOHIDReportOptionNotInterrupt);
                
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

IOReturn IOHIDLibUserClient::
setReport (IOHIDReportType reportType, UInt32 reportID, void *reportBuffer,
                                UInt32 reportBufferSize)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;

    if (fNub && !isInactive() && !fNubIsTerminated)
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

IOReturn IOHIDLibUserClient::
setReportOOL (IOHIDReportReq *req, IOByteCount inCount)
{
    IOReturn 			ret;
    IOMemoryDescriptor *	mem;

    if (fNub && !isInactive() && !fNubIsTerminated)
    {
        mem = IOMemoryDescriptor::withAddress(req->reportBuffer, req->reportBufferSize, kIODirectionOut, fClient);
        if(mem) 
        {
            ret = mem->prepare();
            if(ret == kIOReturnSuccess)
                ret = fNub->setReport(mem, req->reportType, req->reportID);
            
            // make sure the element values are updated.
            if (ret == kIOReturnSuccess)
                fNub->handleReport(mem, req->reportType, kIOHIDReportOptionNotInterrupt);
            
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


IOReturn IOHIDLibUserClient::
asyncGetReport (OSAsyncReference asyncRef, IOHIDReportType reportType, 
                            UInt32 reportID, void *reportBuffer,
                            UInt32 reportBufferSize, UInt32 completionTimeOutMS)
{
    IOReturn 			ret;
    IOHIDCompletion		tap;
    IOMemoryDescriptor *	mem = NULL;
    AsyncParam * 		pb = NULL;

    retain();
    
    if (fNub && !isInactive() && !fNubIsTerminated)
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
    
            bcopy(asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
            pb->fMax = reportBufferSize;
            pb->fMem = mem;
            pb->reportType = reportType;
            tap.target = this;
            tap.action = &ReqComplete;
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
                            
IOReturn IOHIDLibUserClient::
asyncSetReport (OSAsyncReference asyncRef, IOHIDReportType reportType, 
                            UInt32 reportID, void *reportBuffer,
                            UInt32 reportBufferSize, UInt32 completionTimeOutMS)
{
    IOReturn 			ret;
    IOHIDCompletion		tap;
    IOMemoryDescriptor *	mem = NULL;
    AsyncParam * 		pb = NULL;

    retain();

    if (fNub && !isInactive() && !fNubIsTerminated)
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
    
            bcopy(asyncRef, pb->fAsyncRef, sizeof(OSAsyncReference));
            pb->fMax = reportBufferSize;
            pb->fMem = mem;
            pb->reportType = reportType;
            tap.target = this;
            tap.action = &ReqComplete;
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
                                
void IOHIDLibUserClient::
ReqComplete(void *obj, void *param, IOReturn res, UInt32 remaining)
{
    void *	args[1];
    AsyncParam * pb = (AsyncParam *)param;
    IOHIDLibUserClient *me = OSDynamicCast(IOHIDLibUserClient, (OSObject*)obj);

    if (!me)
	return;

    if(res == kIOReturnSuccess) 
    {
        args[0] = (void *)(pb->fMax - remaining);
        
        // make sure the element values are updated.
        if (me->fNub && !me->isInactive() && !me->fNubIsTerminated)
            me->fNub->handleReport(pb->fMem, pb->reportType, kIOHIDReportOptionNotInterrupt);
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

    me->release();
}
