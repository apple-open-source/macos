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
#define CFRUNLOOP_NEW_API 1

#include <CoreFoundation/CFMachPort.h>
//#include <IOKit/hid/IOHIDLib.h>
//#include <unistd.h>
#include <IOKit/hid/IOHIDValue.h>
#include "IOHIDQueueClass.h"
#include "IOHIDLibUserClient.h"

__BEGIN_DECLS
#include <IOKit/IODataQueueClient.h>
#include <mach/mach.h>
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
#include <System/libkern/OSCrossEndian.h>
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

#define createdCheck() do {     \
    if (!fIsCreated)            \
    return kIOReturnError;      \
} while (0)

#define openCheck() do {            \
    if (!fOwningDevice ||           \
        !fOwningDevice->fIsOpen)    \
        return kIOReturnNotOpen;    \
} while (0)

#define terminatedCheck() do {		\
    if ((!fOwningDevice) ||		\
         (fOwningDevice->fIsTerminated))\
        return kIOReturnNotAttached;	\
} while (0)    

#define mostChecks() do {   \
    ownerCheck();           \
    connectCheck();			\
    createdCheck();         \
    terminatedCheck();      \
} while (0)

#define allChecks() do {    \
    mostChecks();           \
    openCheck();			\
} while (0)


IOHIDQueueClass::IOHIDQueueClass() : IOHIDIUnknown(NULL)
{
    fHIDQueue.pseudoVTable = (IUnknownVTbl *)  &sHIDQueueInterfaceV2;
    fHIDQueue.obj = this;
    
    fAsyncPort              = MACH_PORT_NULL;
    fCFSource               = NULL;
    fIsCreated              = false;
    fCreatedFlags           = 0;
    fQueueRef               = 0;
    fCreatedDepth           = 0;
    fQueueEntrySizeChanged  = false;
    fQueueMappedMemory      = NULL;
    fQueueMappedMemorySize  = 0;
    fOwningDevice           = NULL;
    fEventCallback          = NULL;
    fEventRefcon            = NULL;
    fElements               = NULL;
}

IOHIDQueueClass::~IOHIDQueueClass()
{
    if (fIsCreated)
        dispose();
        
    if (fElements)
        CFRelease(fElements);
        
    // if we are owned, detatch
    if (fOwningDevice)
        fOwningDevice->detachQueue(this);
		
	if (fAsyncPort)
        mach_port_destroy(mach_task_self(), fAsyncPort);
        
    if (fCFSource)
        CFRelease(fCFSource);
}

HRESULT IOHIDQueueClass::queryInterface(REFIID iid, void **	ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, kIOHIDDeviceQueueInterfaceID))
    {
        *ppv = getInterfaceMap();
        addRef();
    }
    else {
        res = fOwningDevice->queryInterface(iid, ppv);
    }

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
    return res;
}

IOReturn IOHIDQueueClass::getAsyncEventSource(CFTypeRef *source)
{
    IOReturn ret;
    CFMachPortRef cfPort;
    CFMachPortContext context;
    Boolean shouldFreeInfo;

    if (!fAsyncPort) {     
        ret = getAsyncPort(0);
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

/* CFMachPortCallBack */
void IOHIDQueueClass::queueEventSourceCallback(CFMachPortRef cfPort, mach_msg_header_t *msg, CFIndex size, void *info){
    
    IOHIDQueueClass *queue = (IOHIDQueueClass *)info;
    
    if ( queue ) {
        if ( queue->fEventCallback ) {
                
            (queue->fEventCallback)(queue->fEventRefcon, 
                            kIOReturnSuccess, 
                            (void *)&queue->fHIDQueue);
        }
    }
}


IOReturn IOHIDQueueClass::getAsyncPort(mach_port_t *port)
{
    IOReturn	ret;
    mach_port_t asyncPort;
    connectCheck();
    
    ret = IOCreateReceivePort(kOSAsyncCompleteMessageID, &asyncPort);
    if (kIOReturnSuccess == ret) {		
        if (port)
            *port = asyncPort;

		return setAsyncPort(asyncPort);
    }

    return ret;
}

IOReturn IOHIDQueueClass::setAsyncPort(mach_port_t port)
{
    if ( !port )
		return kIOReturnError;

	fAsyncPort = port;

	if (!fIsCreated)
		return kIOReturnSuccess;
		
	io_async_ref64_t		asyncRef;
	uint64_t				input	= fQueueRef;
	uint32_t				len		= 0;

    return IOConnectCallAsyncScalarMethod(fOwningDevice->fConnection, kIOHIDLibUserClientSetQueueAsyncPort, fAsyncPort, asyncRef, 1, &input, 1, 0, &len);
}


IOReturn IOHIDQueueClass::create (IOOptionBits flags, uint32_t depth)
{
    IOReturn ret = kIOReturnSuccess;

    connectCheck();
    
    // ¥¥Êtodo, check flags/depth to see if different (might need to recreate?)
    if (fIsCreated)
        dispose();

    // sent message to create queue
    uint64_t    input[2];
    uint64_t    output;
    uint32_t    outputCount = 1;
    
    input[0] = flags;
    input[1] = depth;
    
    ret = IOConnectCallScalarMethod(fOwningDevice->fConnection, kIOHIDLibUserClientCreateQueue, input, 2, &output, &outputCount); 

    if (ret != kIOReturnSuccess)
        return ret;
    
    // we have created it
    fIsCreated      = true;
    fQueueRef       = output;
    fCreatedFlags   = flags;
    fCreatedDepth   = depth;
    
    // if we have async port, set it on other side
    if (fAsyncPort)
    {
        ret = setAsyncPort(fAsyncPort);
        if (ret != kIOReturnSuccess) {
            (void) this->dispose();
            return ret;
        }
    }
    
    if (!fElements)
        fElements = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
    else {
        IOHIDElementRef *   elements;
        CFIndex             i, count = CFSetGetCount(fElements);
        
        elements = (IOHIDElementRef *)malloc(sizeof(IOHIDElementRef) *  count);
        
        if (elements)
        {
            for (i=0; i<count; i++)
                addElement(elements[i]);
         
            free(elements);
        }
    }
        
    return ret;
}

IOReturn IOHIDQueueClass::dispose()
{
    IOReturn ret = kIOReturnSuccess;

    mostChecks();

    // ¥¥¥¥ TODO unmap memory when that call works
    if ( fQueueMappedMemory )
    {
#if !__LP64__
    vm_address_t        mappedMem = (vm_address_t)fQueueMappedMemory;
#else
    mach_vm_address_t   mappedMem = (mach_vm_address_t)fQueueMappedMemory;
#endif

        ret = IOConnectUnmapMemory (fOwningDevice->fConnection, 
                                    fQueueRef, 
                                    mach_task_self(), 
                                    mappedMem);
        fQueueMappedMemory = NULL;
        fQueueMappedMemorySize = 0;
    }    


    uint64_t    input = fQueueRef;
    uint32_t    outputCount = 0;
    
    ret = IOConnectCallScalarMethod(fOwningDevice->fConnection, kIOHIDLibUserClientDisposeQueue, &input, 1, 0, &outputCount); 

    if (ret != kIOReturnSuccess)
        return ret;

    // mark it dead
    fIsCreated = false;
        
    fQueueRef = 0;
    
    return kIOReturnSuccess;
}

IOReturn IOHIDQueueClass::getDepth(uint32_t * pDepth)
{
    *pDepth = fCreatedDepth;
    return kIOReturnSuccess;
}

/* Any number of hid elements can feed the same queue */
IOReturn IOHIDQueueClass::addElement (IOHIDElementRef element, IOOptionBits options)
{
    IOReturn    ret = kIOReturnSuccess;
    
    if (!element)
        return kIOReturnBadArgument;

    mostChecks();

    uint64_t    input[3];
    uint64_t    sizeChange;
    uint32_t    outputCount = 1;
    
    input[0] = fQueueRef;
    input[1] = (uint64_t)IOHIDElementGetCookie(element);
    input[2] = options;
    
    ret = IOConnectCallScalarMethod(fOwningDevice->fConnection, kIOHIDLibUserClientAddElementToQueue, input, 3, &sizeChange, &outputCount); 

    if (ret != kIOReturnSuccess)
        return ret;

    fQueueEntrySizeChanged = sizeChange;
    
    if (fElements)
        CFSetSetValue(fElements, element);

    return kIOReturnSuccess;
}

IOReturn IOHIDQueueClass::removeElement (IOHIDElementRef element, IOOptionBits options)
{
    IOReturn ret = kIOReturnSuccess;

    if (!element)
        return kIOReturnBadArgument;

    mostChecks();

    uint64_t    input[2];
    uint64_t    sizeChange;
    uint32_t    outputCount = 1;
    
    input[0] = fQueueRef;
    input[1] = (uint64_t)IOHIDElementGetCookie(element);
    
    ret = IOConnectCallScalarMethod(fOwningDevice->fConnection, kIOHIDLibUserClientRemoveElementFromQueue, input, 2, &sizeChange, &outputCount); 

    if (ret != kIOReturnSuccess)
        return ret;

    fQueueEntrySizeChanged = sizeChange;
    
    if (fElements)
        CFSetRemoveValue(fElements, element);

    return kIOReturnSuccess;
}

IOReturn IOHIDQueueClass::hasElement (IOHIDElementRef element, Boolean * pValue, IOOptionBits options)
{
    if (!element || !pValue)
        return kIOReturnBadArgument;
        
    mostChecks();
        
    uint64_t    input[2];
    uint64_t    returnHasElement;
    uint32_t    outputCount = 1;
    
    input[0] = fQueueRef;
    input[1] = (uint64_t)IOHIDElementGetCookie(element);
    
    IOReturn ret = IOConnectCallScalarMethod(fOwningDevice->fConnection, kIOHIDLibUserClientQueueHasElement, input, 2, &returnHasElement, &outputCount); 

    *pValue = returnHasElement;

    return ret;
}


/* start/stop data delivery to a queue */
IOReturn IOHIDQueueClass::start (IOOptionBits options)
{
    IOReturn ret = kIOReturnSuccess;
    
    mostChecks();
    
    // if the queue size changes, we will need to dispose of the 
    // queue mapped memory
    if ( fQueueEntrySizeChanged && fQueueMappedMemory )
    {
        ret = IOConnectUnmapMemory (fOwningDevice->fConnection, 
                                    fQueueRef, 
                                    mach_task_self(), 
                                    (uintptr_t)fQueueMappedMemory);
        fQueueMappedMemory      = NULL;
        fQueueMappedMemorySize  = 0;
    }    

    uint64_t    input       = fQueueRef;
    uint32_t    outputCount = 0;
        
    ret = IOConnectCallScalarMethod(fOwningDevice->fConnection, kIOHIDLibUserClientStartQueue, &input, 1, 0, &outputCount); 

    if (ret != kIOReturnSuccess)
        return ret;
    
    // get the queue shared memory
    if ( !fQueueMappedMemory )
    {
#if !__LP64__
        vm_address_t        address = nil;
        vm_size_t           size    = 0;
#else
        mach_vm_address_t   address = nil;
        mach_vm_size_t      size    = 0;
#endif

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
            fQueueEntrySizeChanged  = false;
        }
    }

    return kIOReturnSuccess;
}

IOReturn IOHIDQueueClass::stop (IOOptionBits options)
{
    IOReturn ret = kIOReturnSuccess;

    allChecks();

    uint64_t    input       = fQueueRef;
    uint32_t    outputCount = 0;
        
    ret = IOConnectCallScalarMethod(fOwningDevice->fConnection, kIOHIDLibUserClientStopQueue, &input, 1, 0, &outputCount); 

    if (ret != kIOReturnSuccess)
        return ret;
                
    // ¥¥¥ TODO after we stop the queue, we should empty the queue here, in user space
    // (to be consistant with setting the head from user space)
    
    return kIOReturnSuccess;
}

IOReturn IOHIDQueueClass::copyNextEventValue (IOHIDValueRef * pEvent, uint32_t timeout, IOOptionBits options)
{
    IOReturn ret = kIOReturnSuccess;
    
    allChecks();
    
    if ( !fQueueMappedMemory )
        return kIOReturnNoMemory;

    // check entry size
    IODataQueueEntry *  nextEntry = IODataQueuePeek(fQueueMappedMemory);
    uint32_t            entrySize;

	// if queue empty, then stop
	if (nextEntry == NULL)
		return kIOReturnUnderrun;

    entrySize = nextEntry->size;
    ROSETTA_ONLY(
        entrySize = OSSwapInt32(entrySize);
    );

    uint32_t dataSize = sizeof(IOHIDElementValue);
    
    // check size of next entry
    // Make sure that it is not smaller than IOHIDElementValue
    if (entrySize < sizeof(IOHIDElementValue))
        HIDLog ("IOHIDQueueClass: Queue size mismatch (%ld, %ld)\n", entrySize, sizeof(IOHIDElementValue));
    
    // dequeue the item
//    HIDLog ("IOHIDQueueClass::getNextEvent about to dequeue\n");
    ret = IODataQueueDequeue(fQueueMappedMemory, NULL, &dataSize);
//    HIDLog ("IODataQueueDequeue result %lx\n", (uint32_t) ret);
    

    // if we got an entry
    if (ret == kIOReturnSuccess && nextEntry)
    {
        IOHIDElementValue * nextElementValue = (IOHIDElementValue *) &(nextEntry->data);
        IOHIDElementCookie  cookie = nextElementValue->cookie;
        
        ROSETTA_ONLY(
            cookie = (IOHIDElementCookie)OSSwapInt32((uint32_t)cookie);
        );
        
        if ( pEvent )
            *pEvent = _IOHIDValueCreateWithElementValuePtr(kCFAllocatorDefault, fOwningDevice->getElement(cookie), nextElementValue);
    }
    
    return ret;
}

IOReturn IOHIDQueueClass::setEventCallback (IOHIDCallback callback, void * refcon)
{
    fEventCallback = callback;
    fEventRefcon = refcon;
    
    return kIOReturnSuccess;
}

IOHIDDeviceQueueInterface IOHIDQueueClass::sHIDQueueInterfaceV2 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDQueueClass::_getAsyncEventSource,
    &IOHIDQueueClass::_setDepth,
    &IOHIDQueueClass::_getDepth,
    &IOHIDQueueClass::_addElement,
    &IOHIDQueueClass::_removeElement,
    &IOHIDQueueClass::_hasElement,
    &IOHIDQueueClass::_start,
    &IOHIDQueueClass::_stop,
    &IOHIDQueueClass::_setEventCallback,
    &IOHIDQueueClass::_copyNextEventValue
};

IOReturn IOHIDQueueClass::_getAsyncEventSource(void *self, CFTypeRef *source)
    { return getThis(self)->getAsyncEventSource(source); }

IOReturn IOHIDQueueClass::_getAsyncPort(void *self, mach_port_t *port)
    { return getThis(self)->getAsyncPort(port); }
    
IOReturn IOHIDQueueClass::_setDepth(void *self, uint32_t depth, IOOptionBits options)
    { return getThis(self)->create(options, depth); }

IOReturn IOHIDQueueClass::_getDepth(void *self, uint32_t *pDepth)
    { return getThis(self)->getDepth(pDepth); }

IOReturn IOHIDQueueClass::_addElement (void * self, IOHIDElementRef element, IOOptionBits options)
    { return getThis(self)->addElement(element, options); }

IOReturn IOHIDQueueClass::_removeElement (void * self, IOHIDElementRef element, IOOptionBits options)
    { return getThis(self)->removeElement(element, options); }

IOReturn IOHIDQueueClass::_hasElement (void * self, IOHIDElementRef element, Boolean * pValue, IOOptionBits options)
    { return getThis(self)->hasElement(element, pValue, options); }

IOReturn IOHIDQueueClass::_start (void * self, IOOptionBits options)
    { return getThis(self)->start(options); }

IOReturn IOHIDQueueClass::_stop (void * self, IOOptionBits options)
    { return getThis(self)->stop(options); }

IOReturn IOHIDQueueClass::_copyNextEventValue (void * self, IOHIDValueRef * pEvent, uint32_t timeout, IOOptionBits options)
    { return getThis(self)->copyNextEventValue(pEvent, timeout, options); }

IOReturn IOHIDQueueClass::_setEventCallback (void * self, IOHIDCallback callback, void * refcon)
    { return getThis(self)->setEventCallback(callback, refcon); }

    
    
//****************************************************************************************************
// Class:       IOHIDObsoleteQueueClass
// Subclasses:  IOHIDQueueClass
//****************************************************************************************************
IOHIDQueueInterface IOHIDObsoleteQueueClass::sHIDQueueInterface =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDObsoleteQueueClass::_createAsyncEventSource,
    &IOHIDObsoleteQueueClass::_getAsyncEventSource,
    &IOHIDQueueClass::_getAsyncPort,
    &IOHIDObsoleteQueueClass::_getAsyncPort,
    &IOHIDObsoleteQueueClass::_create,
    &IOHIDObsoleteQueueClass::_dispose,
    &IOHIDObsoleteQueueClass::_addElement,
    &IOHIDObsoleteQueueClass::_removeElement,
    &IOHIDObsoleteQueueClass::_hasElement,
    &IOHIDObsoleteQueueClass::_start,
    &IOHIDObsoleteQueueClass::_stop,
    &IOHIDObsoleteQueueClass::_getNextEvent,
    &IOHIDObsoleteQueueClass::_setEventCallout,
    &IOHIDObsoleteQueueClass::_getEventCallout,
};

// Methods for routing asynchronous completion plumbing.
IOReturn IOHIDObsoleteQueueClass::_createAsyncEventSource(void * self, CFRunLoopSourceRef * pSource)
{
    return getThis(self)->createAsyncEventSource(pSource);
}

CFRunLoopSourceRef IOHIDObsoleteQueueClass::_getAsyncEventSource(void *self)
    { return getThis(self)->fCFSource; }

mach_port_t IOHIDObsoleteQueueClass::_getAsyncPort(void *self)
    { return getThis(self)->fAsyncPort; }

IOReturn IOHIDObsoleteQueueClass::_create (void * self, uint32_t flags, uint32_t depth)
    { return getThis(self)->create(flags, depth); }

IOReturn IOHIDObsoleteQueueClass::_dispose (void * self)
    { return getThis(self)->dispose(); }

IOReturn IOHIDObsoleteQueueClass::_addElement (void * self, IOHIDElementCookie cookie, uint32_t flags)
    { return getThis(self)->addElement(cookie, flags); }

IOReturn IOHIDObsoleteQueueClass::_removeElement (void * self, IOHIDElementCookie elementCookie)
    { return getThis(self)->removeElement(elementCookie); }

Boolean IOHIDObsoleteQueueClass::_hasElement (void * self, IOHIDElementCookie elementCookie)
    { return getThis(self)->hasElement(elementCookie); }

IOReturn IOHIDObsoleteQueueClass::_start (void * self)
    { return getThis(self)->start(); }

IOReturn IOHIDObsoleteQueueClass::_stop (void * self)
    { return getThis(self)->stop(); }

IOReturn IOHIDObsoleteQueueClass::_getNextEvent (void * self, IOHIDEventStruct * event, AbsoluteTime maxTime, uint32_t timeoutMS)
    { return getThis(self)->getNextEvent(event, maxTime, timeoutMS); }

IOReturn IOHIDObsoleteQueueClass::_setEventCallout (void * self, IOHIDCallbackFunction callback, void * target, void * refcon)
    { return getThis(self)->setEventCallout (callback, target, refcon); }

IOReturn IOHIDObsoleteQueueClass::_getEventCallout (void * self, IOHIDCallbackFunction * pCallback, void ** pTarget, void ** pRefcon)
    { return getThis(self)->getEventCallout(pCallback, pTarget, pRefcon); }

IOHIDObsoleteQueueClass::IOHIDObsoleteQueueClass() : IOHIDQueueClass()
{
    fHIDQueue.pseudoVTable = (IUnknownVTbl *)  &sHIDQueueInterface;
    fHIDQueue.obj = this;
    
    fCallback   = NULL;
    fTarget     = NULL;
    fRefcon     = NULL;
}

HRESULT IOHIDObsoleteQueueClass::queryInterface(REFIID iid, void **	ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, kIOHIDQueueInterfaceID))
    {
        *ppv = getInterfaceMap();
        addRef();
    }
    else {
        res = fOwningDevice->queryInterface(iid, ppv);
    }

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
    return res;
}

IOReturn IOHIDObsoleteQueueClass::createAsyncEventSource(CFRunLoopSourceRef * pSource)
{
    IOReturn ret = IOHIDQueueClass::getAsyncEventSource((CFTypeRef *)pSource);
    
    if ( ret == kIOReturnSuccess && pSource && *pSource )
        CFRetain(*pSource);
        
    return ret;
}


IOReturn IOHIDObsoleteQueueClass::addElement (IOHIDElementCookie cookie, uint32_t flags)
{
    return IOHIDQueueClass::addElement(fOwningDevice->getElement(cookie), flags);
}

IOReturn IOHIDObsoleteQueueClass::removeElement (IOHIDElementCookie cookie)
{
    return IOHIDQueueClass::removeElement(fOwningDevice->getElement(cookie));
}

Boolean IOHIDObsoleteQueueClass::hasElement (IOHIDElementCookie cookie)
{
    IOHIDElementRef element = fOwningDevice->getElement(cookie);
    Boolean         value   = FALSE;
    
    return (IOHIDQueueClass::hasElement(element, &value) == kIOReturnSuccess) ? value : FALSE;
}

IOReturn IOHIDObsoleteQueueClass::getNextEvent (IOHIDEventStruct * pEventStruct, AbsoluteTime maxTime, uint32_t timeoutMS)
{
    IOHIDValueRef   event = NULL;
    IOReturn        ret   = kIOReturnBadArgument;
    
    if (pEventStruct)
    {
        ret = copyNextEventValue(&event, timeoutMS);
        
        if ((ret==kIOReturnSuccess) && event)
        {
            uint32_t length = _IOHIDElementGetLength(IOHIDValueGetElement(event));
            
            pEventStruct->type                  = IOHIDElementGetType(IOHIDValueGetElement(event));
            pEventStruct->elementCookie         = IOHIDElementGetCookie(IOHIDValueGetElement(event));
            *(UInt64 *)&pEventStruct->timestamp = IOHIDValueGetTimeStamp(event);
            
            if ( length > sizeof(uint32_t) )
            {
                pEventStruct->longValueSize = length;
                pEventStruct->longValue     = malloc(length);
                bcopy(IOHIDValueGetBytePtr(event), pEventStruct->longValue, length);
            }
            else
            {
                pEventStruct->longValueSize = 0;
                pEventStruct->longValue     = NULL;
                pEventStruct->value         = IOHIDValueGetIntegerValue(event);
            }
            
            CFRelease(event);
        }
    }
    
    return ret;
}

IOReturn IOHIDObsoleteQueueClass::getEventCallout (IOHIDCallbackFunction * pCallback, void ** pTarget, void ** pRefcon)
{
    if (pCallback)  *pCallback  = fCallback;
    if (pTarget)    *pTarget    = fTarget;
    if (pRefcon)    *pRefcon    = fRefcon;
    
    return kIOReturnSuccess;
}

IOReturn IOHIDObsoleteQueueClass::setEventCallout (IOHIDCallbackFunction callback, void * target, void * refcon)
{
    fCallback   = callback;
    fTarget     = target;
    fRefcon     = refcon;
    
    IOHIDQueueClass::setEventCallback(IOHIDObsoleteQueueClass::_eventCallback, this);

    return kIOReturnSuccess;
}

void IOHIDObsoleteQueueClass::_eventCallback(void * refcon, IOReturn result, void * sender)
{
    IOHIDObsoleteQueueClass * self = (IOHIDObsoleteQueueClass *) refcon;
    
    if ( self->fCallback )
        (*self->fCallback)(self->fTarget, result, self->fRefcon, sender);
}

