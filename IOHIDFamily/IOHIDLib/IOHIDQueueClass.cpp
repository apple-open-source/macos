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
#define CFRUNLOOP_NEW_API 1

#include <CoreFoundation/CFMachPort.h>
//#include <IOKit/hid/IOHIDLib.h>
//#include <unistd.h>

#include "IOHIDQueueClass.h"
#include "IOHIDLibUserClient.h"

__BEGIN_DECLS
#include <IOKit/IODataQueueClient.h>
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
__END_DECLS

#define ownerCheck() do {		\
    if (!fOwningDevice)			\
	return kIOReturnNoDevice;	\
} while (0)

#define connectCheck() do {		\
    if ((!fOwningDevice) ||		\
    	(!fOwningDevice->fConnection))	\
	return kIOReturnNoDevice;	\
} while (0)

#define openCheck() do {	    \
    if (!fIsCreated)		    \
        return kIOReturnNotOpen;    \
} while (0)

#define allChecks() do {	    \
    connectCheck();		    \
    openCheck();		    \
} while (0)

IOHIDQueueClass::IOHIDQueueClass()
: IOHIDIUnknown(NULL),
  fAsyncPort(MACH_PORT_NULL),
  fIsCreated(false),
  fEventCallback(NULL),
  fEventTarget(NULL),
  fEventRefcon(NULL)
{
    fHIDQueue.pseudoVTable = (IUnknownVTbl *)  &sHIDQueueInterfaceV1;
    fHIDQueue.obj = this;
}

IOHIDQueueClass::~IOHIDQueueClass()
{
    // if we are owned, detatch
    if (fOwningDevice)
        fOwningDevice->detachQueue(this);
}

HRESULT IOHIDQueueClass::queryInterface(REFIID /*iid*/, void **	/*ppv*/)
{
    // еее should we return our parent if that type is asked for???
    
    return E_NOINTERFACE;
}

IOReturn IOHIDQueueClass::
createAsyncEventSource(CFRunLoopSourceRef *source)
{
    IOReturn ret;
    CFMachPortRef cfPort;
    CFMachPortContext context;
    Boolean shouldFreeInfo;

    if (!fAsyncPort) {     
        ret = createAsyncPort(0);
        if (kIOReturnSuccess != ret)
            return ret;
    }

    context.version = 1;
    context.info = this;
    context.retain = NULL;
    context.release = NULL;
    context.copyDescription = NULL;

    cfPort = CFMachPortCreateWithPort(NULL, fAsyncPort,
                (CFMachPortCallBack) IOHIDQueueClass::queueEventSourceCallback,
                &context, &shouldFreeInfo);
    if (!cfPort)
        return kIOReturnNoMemory;
    
    fCFSource = CFMachPortCreateRunLoopSource(NULL, cfPort, 0);
    CFRelease(cfPort);
    if (!fCFSource)
        return kIOReturnNoMemory;

    if (source)
        *source = fCFSource;

    return kIOReturnSuccess;
}

CFRunLoopSourceRef IOHIDQueueClass::getAsyncEventSource()
{
    return fCFSource;
}

/* CFMachPortCallBack */
void IOHIDQueueClass::queueEventSourceCallback(CFMachPortRef *cfPort, mach_msg_header_t *msg, CFIndex size, void *info){
    
    IOHIDQueueClass *queue = (IOHIDQueueClass *)info;
    
    if ( queue ) {
        if ( queue->fEventCallback ) {
                
            (queue->fEventCallback)(queue->fEventTarget, 
                            kIOReturnSuccess, 
                            queue->fEventRefcon, 
                            (void *)&queue->fHIDQueue);
        }
    }
}


IOReturn IOHIDQueueClass::createAsyncPort(mach_port_t *port)
{
    IOReturn ret;
    
    connectCheck();
    
    ret = IOCreateReceivePort(kOSAsyncCompleteMessageID, &fAsyncPort);
    if (kIOReturnSuccess == ret) {
        if (port)
            *port = fAsyncPort;

        if (fIsCreated) {
            natural_t 			asyncRef[1];
            int				input[1];
            mach_msg_type_number_t 	len = 0;
        
            input[0] = (int) fQueueRef;
            // async kIOHIDLibUserClientSetQueueAsyncPort, kIOUCScalarIScalarO, 1, 0
            return io_async_method_scalarI_scalarO(
                    fOwningDevice->fConnection, fAsyncPort, asyncRef, 1,
                    kIOHIDLibUserClientSetQueueAsyncPort, input, 1, NULL, &len);
        }
    }

    return ret;
}

mach_port_t IOHIDQueueClass::getAsyncPort()
{
    return fAsyncPort;
}

IOReturn IOHIDQueueClass::create (UInt32 flags, UInt32 depth)
{
    IOReturn ret = kIOReturnSuccess;

    connectCheck();
    
    // ее╩todo, check flags/depth to see if different (might need to recreate?)
    if (fIsCreated)
        return kIOReturnSuccess;

    // sent message to create queue
    //  kIOHIDLibUserClientCreateQueue, kIOUCScalarIScalarO, 2, 1
    int args[6], i = 0;
    args[i++] = flags;
    args[i++] = depth;
    mach_msg_type_number_t len = 1;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientCreateQueue, args, i, (int *) &fQueueRef, &len);
    if (ret != kIOReturnSuccess)
        return ret;
    
    // we have created it
    fIsCreated = true;
    fCreatedFlags = flags;
    fCreatedDepth = depth;
    
    // if we have async port, set it on other side
    if (fAsyncPort)
    {
        natural_t 			asyncRef[1];
        int				input[1];
        mach_msg_type_number_t 	len = 0;
    
        input[0] = (int) fQueueRef;
        // async kIOHIDLibUserClientSetQueueAsyncPort, kIOUCScalarIScalarO, 1, 0
        return io_async_method_scalarI_scalarO(
                fOwningDevice->fConnection, fAsyncPort, asyncRef, 1,
                kIOHIDLibUserClientSetQueueAsyncPort, input, 1, NULL, &len);
        if (ret != kIOReturnSuccess) {
            (void) this->dispose();
            return ret;
        }
    }
    
    // get the queue shared memory
    vm_address_t address = nil;
    vm_size_t size = 0;
    
    ret = IOConnectMapMemory (	fOwningDevice->fConnection, 
                                fQueueRef, 
                                mach_task_self(), 
                                &address, 
                                &size, 
                                kIOMapAnywhere	);
    if (ret == kIOReturnSuccess)
    {
        fQueueMappedMemory = (IODataQueueMemory *) address;
        fQueueMappedMemorySize = size;
    }
    
    return ret;
}

IOReturn IOHIDQueueClass::dispose()
{
    IOReturn ret = kIOReturnSuccess;

    allChecks();

    // sent message to dispose queue
    mach_msg_type_number_t len = 0;

    //  kIOHIDLibUserClientDisposeQueue, kIOUCScalarIScalarO, 1, 0
    int args[6], i = 0;
    args[i++] = fQueueRef;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientDisposeQueue, args, i, NULL, &len);
    if (ret != kIOReturnSuccess)
        return ret;

    // mark it dead
    fIsCreated = false;
    
    // ееее TODO unmap memory when that call works
    
    return kIOReturnSuccess;
}

/* Any number of hid elements can feed the same queue */
IOReturn IOHIDQueueClass::addElement (
                            IOHIDElementCookie elementCookie,
                            UInt32 flags)
{
    IOReturn ret = kIOReturnSuccess;

    allChecks();

    //  kIOHIDLibUserClientAddElementToQueue, kIOUCScalarIScalarO, 3, 0
    int args[6], i = 0;
    args[i++] = fQueueRef;
    args[i++] = (int) elementCookie;
    args[i++] = flags;
    mach_msg_type_number_t len = 0;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientAddElementToQueue, args, i, NULL, &len);
    if (ret != kIOReturnSuccess)
        return ret;

    return kIOReturnSuccess;
}

IOReturn IOHIDQueueClass::removeElement (IOHIDElementCookie elementCookie)
{
    IOReturn ret = kIOReturnSuccess;

    allChecks();

    //  kIOHIDLibUserClientRemoveElementFromQueue, kIOUCScalarIScalarO, 2, 0
    int args[6], i = 0;
    args[i++] = fQueueRef;
    args[i++] = (int) elementCookie;
    mach_msg_type_number_t len = 0;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientRemoveElementFromQueue, args, i, NULL, &len);
    if (ret != kIOReturnSuccess)
        return ret;

    return kIOReturnSuccess;
}

Boolean IOHIDQueueClass::hasElement (IOHIDElementCookie elementCookie)
{
    Boolean returnHasElement = false;

    // cannot do allChecks(), since return is a Boolean
    if (((!fOwningDevice) ||
    	(!fOwningDevice->fConnection)) ||
        (!fIsCreated))
        return false;

    //  kIOHIDLibUserClientQueueHasElement, kIOUCScalarIScalarO, 2, 1
    int args[6], i = 0;
    args[i++] = fQueueRef;
    args[i++] = (int) elementCookie;
    mach_msg_type_number_t len = 1;
    IOReturn ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientQueueHasElement, args, 
            i, (int *) &returnHasElement, &len);
    if (ret != kIOReturnSuccess)
        return false;

    return returnHasElement;
}


/* start/stop data delivery to a queue */
IOReturn IOHIDQueueClass::start ()
{
    IOReturn ret = kIOReturnSuccess;

    allChecks();

    //  kIOHIDLibUserClientStartQueue, kIOUCScalarIScalarO, 1, 0
    int args[6], i = 0;
    args[i++] = fQueueRef;
    mach_msg_type_number_t len = 0;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientStartQueue, args, i, NULL, &len);
    if (ret != kIOReturnSuccess)
        return ret;

    return kIOReturnSuccess;
}

IOReturn IOHIDQueueClass::stop ()
{
    IOReturn ret = kIOReturnSuccess;

    allChecks();

    //  kIOHIDLibUserClientStopQueue, kIOUCScalarIScalarO, 1, 0
    int args[6], i = 0;
    args[i++] = fQueueRef;
    mach_msg_type_number_t len = 0;
    ret = io_connect_method_scalarI_scalarO(
            fOwningDevice->fConnection, kIOHIDLibUserClientStopQueue, args, i, NULL, &len);
    if (ret != kIOReturnSuccess)
        return ret;
    
    // еее TODO after we stop the queue, we should empty the queue here, in user space
    // (to be consistant with setting the head from user space)
    
    return kIOReturnSuccess;
}

/* read next event from a queue */
/* maxtime, if non-zero, limits read events to those that occured */
/*   on or before maxTime */
/* timoutMS is the timeout in milliseconds, a zero timeout will cause */
/*	this call to be non-blocking (returning queue empty) if there */
/*	is a NULL callback, and blocking forever until the queue is */
/*	non-empty if their is a valid callback */
IOReturn IOHIDQueueClass::getNextEvent (
                        IOHIDEventStruct *	event,
                        AbsoluteTime		maxTime,
                        UInt32 			timeoutMS)
{
    IOReturn ret = kIOReturnSuccess;
    
#if 0
    printf ("IOHIDQueueClass::getNextEvent about to peek\n");

    printf ("fQueueMappedMemory->queueSize = %lx\n", fQueueMappedMemory->queueSize);
    printf ("fQueueMappedMemory->head = %lx\n", fQueueMappedMemory->head);
    printf ("fQueueMappedMemory->tail = %lx\n", fQueueMappedMemory->tail);
#endif
    
    // check entry size
    IODataQueueEntry * nextEntry = IODataQueuePeek(fQueueMappedMemory);

	// if queue empty, then stop
	if (nextEntry == NULL)
		return kIOReturnUnderrun;

#if 0
    printf ("IODataQueuePeek: %lx\n", (UInt32) nextEntry);
    if (nextEntry)
    {
        printf ("nextEntry->size = %lx\n", nextEntry->size);
        printf ("nextEntry->data = %lx\n", (UInt32) nextEntry->data);

        IOHIDElementValue * nextElementValue = (IOHIDElementValue *) &nextEntry->data;
        
        printf ("nextElementValue->cookie = %lx\n", (UInt32) nextElementValue->cookie);
        printf ("nextElementValue->value[0] = %lx\n", nextElementValue->value[0]);
    }
#endif

    UInt32 dataSize = sizeof(IOHIDElementValue);
    
#if 0
    // еее TODO deal with long sizes
    if (event->longValueSize !=	0)
        printf ("long size specified\n");
#endif
    
    // check size of next entry
    // Make sure that it is not smaller than IOHIDElementValue
    if (nextEntry && (nextEntry->size < sizeof(IOHIDElementValue)))
        printf ("IOHIDQueueClass: Queue size mismatch (%ld, %ld)\n", nextEntry->size, sizeof(IOHIDElementValue));
    
    // dequeue the item
//    printf ("IOHIDQueueClass::getNextEvent about to dequeue\n");
    ret = IODataQueueDequeue(fQueueMappedMemory, NULL, &dataSize);
//    printf ("IODataQueueDequeue result %lx\n", (UInt32) ret);
    

    // if we got an entry
    if (ret == kIOReturnSuccess && nextEntry)
    {
    
        IOHIDElementValue * nextElementValue = (IOHIDElementValue *) &nextEntry->data;
        
#if 0
        printf ("nextElementValue->cookie = %lx\n", (UInt32) nextElementValue->cookie);
        printf ("nextElementValue->value[0] = %lx\n", nextElementValue->value[0]);
#endif

        void *		longValue = 0;
        UInt32		longValueSize = 0;
        SInt32		value = 0;
        UInt64		timestamp = 0;

        
        // check size of result
        if (dataSize == sizeof(IOHIDElementValue))
        {
            value = nextElementValue->value[0];
            timestamp = *(UInt64 *)& nextElementValue->timestamp;
        }
        else if (dataSize > sizeof(IOHIDElementValue))
        {
            longValueSize = fOwningDevice->getElementByteSize(nextElementValue->cookie);
            longValue = malloc( longValueSize );
            bzero(longValue, longValueSize);
            
            // *** FIX ME ***
            // Since we are getting mapped memory, we should probably
            // hold a shared lock
            fOwningDevice->convertWordToByte(nextElementValue->value, longValue, longValueSize<<3);
            
            timestamp = *(UInt64 *)& nextElementValue->timestamp;

        }
        else
            printf ("IOHIDQueueClass: Queue size mismatch (%ld, %ld)\n", dataSize, sizeof(IOHIDElementValue));
        
        // copy the data to the event struct
        event->type = fOwningDevice->getElementType(nextElementValue->cookie);
        event->elementCookie = nextElementValue->cookie;
        event->value = value;
        *(UInt64 *)& event->timestamp = timestamp;
        event->longValueSize = longValueSize;
        event->longValue = longValue;
    }
    
    
    return ret;
}


/* set a callback for notification when queue transistions from non-empty */
/* callback, if non-NULL is a callback to be called when data is */
/*  inserted to the queue  */
/* callbackTarget and callbackRefcon are passed to the callback */
IOReturn IOHIDQueueClass::setEventCallout (
                        IOHIDCallbackFunction 	callback,
                        void * 			callbackTarget,
                        void *			callbackRefcon)
{
    fEventCallback = callback;
    fEventTarget = callbackTarget;
    fEventRefcon = callbackRefcon;
    
    return kIOReturnSuccess;
}


/* Get the current notification callout */
IOReturn IOHIDQueueClass::getEventCallout (
                        IOHIDCallbackFunction * 	outCallback,
                        void ** 			outCallbackTarget,
                        void **			outCallbackRefcon)
{

    *outCallback = fEventCallback;
    *outCallbackTarget = fEventTarget;
    *outCallbackRefcon = fEventRefcon;
    
    return kIOReturnSuccess;
}


IOHIDQueueInterface IOHIDQueueClass::sHIDQueueInterfaceV1 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDQueueClass::queueCreateAsyncEventSource,
    &IOHIDQueueClass::queueGetAsyncEventSource,
    &IOHIDQueueClass::queueCreateAsyncPort,
    &IOHIDQueueClass::queueGetAsyncPort,
    &IOHIDQueueClass::queueCreate,
    &IOHIDQueueClass::queueDispose,
    &IOHIDQueueClass::queueAddElement,
    &IOHIDQueueClass::queueRemoveElement,
    &IOHIDQueueClass::queueHasElement,
    &IOHIDQueueClass::queueStart,
    &IOHIDQueueClass::queueStop,
    &IOHIDQueueClass::queueGetNextEvent,
    &IOHIDQueueClass::queueSetEventCallout,
    &IOHIDQueueClass::queueGetEventCallout,
};

// Methods for routing asynchronous completion plumbing.
IOReturn IOHIDQueueClass::
queueCreateAsyncEventSource(void *self, CFRunLoopSourceRef *source)
    { return getThis(self)->createAsyncEventSource(source); }

CFRunLoopSourceRef IOHIDQueueClass::
queueGetAsyncEventSource(void *self)
    { return getThis(self)->getAsyncEventSource(); }

IOReturn IOHIDQueueClass::
queueCreateAsyncPort(void *self, mach_port_t *port)
    { return getThis(self)->createAsyncPort(port); }

mach_port_t IOHIDQueueClass::
queueGetAsyncPort(void *self)
    { return getThis(self)->getAsyncPort(); }

/* Basic IOHIDQueue interface */
IOReturn IOHIDQueueClass::
        queueCreate (void * 			self, 
                    UInt32 			flags,
                    UInt32			depth)
    { return getThis(self)->create(flags, depth); }

IOReturn IOHIDQueueClass::queueDispose (void * self)
    { return getThis(self)->dispose(); }

/* Any number of hid elements can feed the same queue */
IOReturn IOHIDQueueClass::queueAddElement (void * self,
                            IOHIDElementCookie elementCookie,
                            UInt32 flags)
    { return getThis(self)->addElement(elementCookie, flags); }

IOReturn IOHIDQueueClass::queueRemoveElement (void * self, IOHIDElementCookie elementCookie)
    { return getThis(self)->removeElement(elementCookie); }

Boolean IOHIDQueueClass::queueHasElement (void * self, IOHIDElementCookie elementCookie)
    { return getThis(self)->hasElement(elementCookie); }

/* start/stop data delivery to a queue */
IOReturn IOHIDQueueClass::queueStart (void * self)
    { return getThis(self)->start(); }

IOReturn IOHIDQueueClass::queueStop (void * self)
    { return getThis(self)->stop(); }

/* read next event from a queue */
IOReturn IOHIDQueueClass::queueGetNextEvent (
                        void * 			self,
                        IOHIDEventStruct *	event,
                        AbsoluteTime		maxTime,
                        UInt32 			timeoutMS)
    { return getThis(self)->getNextEvent(event, maxTime, timeoutMS); }

/* set a callback for notification when queue transistions from non-empty */
IOReturn IOHIDQueueClass::queueSetEventCallout (
                        void * 			self,
                        IOHIDCallbackFunction   callback,
                        void * 			callbackTarget,
                        void *			callbackRefcon)
    { return getThis(self)->setEventCallout(callback, callbackTarget, callbackRefcon); }

/* Get the current notification callout */
IOReturn IOHIDQueueClass::queueGetEventCallout (
                        void * 			self,
                        IOHIDCallbackFunction * outCallback,
                        void ** 		outCallbackTarget,
                        void **			outCallbackRefcon)
    { return getThis(self)->getEventCallout(outCallback, outCallbackTarget, outCallbackRefcon); }

