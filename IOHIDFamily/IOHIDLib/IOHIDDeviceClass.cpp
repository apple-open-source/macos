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

#include "IOHIDDeviceClass.h"
#include "IOHIDQueueClass.h"
#include "IOHIDLibUserClient.h"

#if IOHID_PSEUDODEVICE
// evil hackery to include this file, its just for the fake device
#define _NSBUILDING_APPKIT_DLL 0
#include <CoreFoundation/CFVeryPrivate.h>
#undef _NSBUILDING_APPKIT_DLL
#endif

__BEGIN_DECLS
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
__END_DECLS

#define connectCheck() do {	    \
    if (!fConnection)		    \
	return kIOReturnNoDevice;   \
} while (0)

#define openCheck() do {	    \
    if (!fIsOpen)		    \
        return kIOReturnNotOpen;    \
} while (0)

#define allChecks() do {	    \
    connectCheck();		    \
    openCheck();		    \
} while (0)

IOCFPlugInInterface ** IOHIDDeviceClass::alloc()
{
    IOHIDDeviceClass *me;

    me = new IOHIDDeviceClass;
    if (me)
        return (IOCFPlugInInterface **) &me->iunknown.pseudoVTable;
    else
        return 0;
}

IOHIDDeviceClass::IOHIDDeviceClass()
: IOHIDIUnknown(&sIOCFPlugInInterfaceV1),
  fService(MACH_PORT_NULL),
  fConnection(MACH_PORT_NULL),
  fAsyncPort(MACH_PORT_NULL),
  fIsOpen(false),
  fIsLUNZero(false)
{
    fHIDDevice.pseudoVTable = (IUnknownVTbl *)  &sHIDDeviceInterfaceV1;
    fHIDDevice.obj = this;
    
    fElementCount = 0;
    fElements = nil;
}

IOHIDDeviceClass::~IOHIDDeviceClass()
{
    if (fConnection) {
        IOServiceClose(fConnection);
        fConnection = MACH_PORT_NULL;
    }
        
    if (fService) {
        IOObjectRelease(fService);
        fService = MACH_PORT_NULL;
    }
    
    if (fElements)
        delete[] fElements;
}

HRESULT	IOHIDDeviceClass::attachQueue (IOHIDQueueClass * iohidQueue)
{
    HRESULT res = S_OK;
    
    iohidQueue->setOwningDevice(this);

    // ееее todo add to list
    
    return res;
}

HRESULT	IOHIDDeviceClass::detachQueue (IOHIDQueueClass * iohidQueue)
{
    HRESULT res = S_OK;

    iohidQueue->setOwningDevice(NULL);
    
    // ееее todo remove from list
    
    return res;
}


HRESULT IOHIDDeviceClass::queryInterfaceQueue (void **ppv)
{
    HRESULT res = S_OK;
    
    // create the queue class
    IOHIDQueueClass * newQueue = new IOHIDQueueClass;
    
    // attach the queue to us
    attachQueue (newQueue);
    
    // add a ref for the one we return
//    newQueue->addRef();
    
    // set the return
    *ppv = newQueue->getInterfaceMap();
    
    return res;
}

HRESULT IOHIDDeviceClass::queryInterfaceOutputTransaction (void **ppv)
{
    HRESULT res = E_NOINTERFACE;
    
    *ppv = 0;
    
    return res;
}


HRESULT IOHIDDeviceClass::queryInterface(REFIID iid, void **ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, kIOHIDQueueInterfaceID))
        res = queryInterfaceQueue(ppv);
    else if (CFEqual(uuid, kIOHIDOutputTransactionInterfaceID))
        res = queryInterfaceOutputTransaction(ppv);
    else if (CFEqual(uuid, IUnknownUUID)
         ||  CFEqual(uuid, kIOCFPlugInInterfaceID))
    {
        *ppv = &iunknown;
        addRef();
    }
    else if (CFEqual(uuid, kIOHIDDeviceInterfaceID))
    {
        *ppv = &fHIDDevice;
        addRef();
    }
    else {
        *ppv = 0;
        printf ("not found\n");
    }

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
    return res;
}

IOReturn IOHIDDeviceClass::
probe(CFDictionaryRef propertyTable, io_service_t inService, SInt32 *order)
{
    if (!inService || !IOObjectConformsTo(inService, "IOHIDDevice"))
        return kIOReturnBadArgument;

    return kIOReturnSuccess;
}

IOReturn IOHIDDeviceClass::
start(CFDictionaryRef propertyTable, io_service_t inService)
{
    IOReturn res;
    CFMutableDictionaryRef entryProperties = 0;
    kern_return_t kr;
    
    fService = inService;
	IOObjectRetain(fService);
    res = IOServiceOpen(fService, mach_task_self(), 0, &fConnection);
    if (res != kIOReturnSuccess)
        return res;

    connectCheck();
    
    kr = IORegistryEntryCreateCFProperties (fService,
                                            &entryProperties,
                                            kCFAllocatorDefault,
                                            kNilOptions );
    if (entryProperties)
    {
        BuildElements((CFDictionaryRef) entryProperties);
        
        CFRelease(entryProperties);
    }


    return kIOReturnSuccess;
}

IOReturn IOHIDDeviceClass::
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
                (CFMachPortCallBack) IODispatchCalloutFromMessage,
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

CFRunLoopSourceRef IOHIDDeviceClass::getAsyncEventSource()
{
    return fCFSource;
}

IOReturn IOHIDDeviceClass::createAsyncPort(mach_port_t *port)
{
    IOReturn ret;

    connectCheck();

    ret = IOCreateReceivePort(kOSAsyncCompleteMessageID, &fAsyncPort);
    if (kIOReturnSuccess == ret) {
        if (port)
            *port = fAsyncPort;

        if (fIsOpen) {
            natural_t asyncRef[1];
            mach_msg_type_number_t len = 0;
        
            // async kIOCDBUserClientSetAsyncPort,  kIOUCScalarIScalarO,    0,	0
            return io_async_method_structureI_structureO(
                    fConnection, fAsyncPort, asyncRef, 1,
                    kIOHIDLibUserClientSetAsyncPort, NULL, 0, NULL, &len);
        }
    }

    return ret;
}

mach_port_t IOHIDDeviceClass::getAsyncPort()
{
    return fAsyncPort;
}

IOReturn IOHIDDeviceClass::open(UInt32 flags)
{
    IOReturn ret = kIOReturnSuccess;

    connectCheck();

    // ее╩todo, check flags to see if different (if so, we might need to reopen)
    if (fIsOpen)
        return kIOReturnSuccess;

    mach_msg_type_number_t len = 0;

    //  kIOHIDLibUserClientOpen,  kIOUCScalarIScalarO,    0,	0
    ret = io_connect_method_scalarI_scalarO(
            fConnection, kIOHIDLibUserClientOpen, NULL, 0, NULL, &len);
    if (ret != kIOReturnSuccess)
        return ret;

    fIsOpen = true;

    if (fAsyncPort) {
        natural_t asyncRef[1];
        mach_msg_type_number_t len = 0;
    
        // async 
        // kIOHIDLibUserClientSetAsyncPort,  kIOUCScalarIScalarO,    0,	0
        ret = io_async_method_scalarI_scalarO(
                fConnection, fAsyncPort, asyncRef, 1,
                kIOHIDLibUserClientSetAsyncPort, NULL, 0, NULL, &len);
        if (ret != kIOReturnSuccess) {
            close();
            return ret;
        }
    }
    
    // get the shared memory
    vm_address_t address = nil;
    vm_size_t size = 0;
    
    ret = IOConnectMapMemory (	fConnection, 
                                IOHIDLibUserClientElementValuesType, 
                                mach_task_self(), 
                                &address, 
                                &size, 
                                kIOMapAnywhere	);
    if (ret == kIOReturnSuccess)
    {
        fCurrentValuesMappedMemory = address;
        fCurrentValuesMappedMemorySize = size;
    }
    
    return ret;
}

IOReturn IOHIDDeviceClass::close()
{
    allChecks();

#if 0
    IOCDBCommandClass::
	commandDeviceClosing((IOCDBDeviceInterface **) &fCDBDevice); 
#endif

// еее IOConnectUnmapMemory does not work, so we cannot call it
// when the user client finally goes away (when our client closes the service)
// everything will get cleaned up, but this is still ugly
#if 0
    // finished with the shared memory
    if (fCurrentValuesMappedMemory != 0)
    {
        (void) IOConnectUnmapMemory (	fConnection, 
                                        IOHIDLibUserClientElementValuesType, 
                                        mach_task_self(), 
                                        fCurrentValuesMappedMemory);
        fCurrentValuesMappedMemory = nil;
        fCurrentValuesMappedMemorySize = 0;
    }
#endif

    mach_msg_type_number_t len = 0;
    // kIOCDBUserClientClose,	kIOUCScalarIScalarO,	 0,  0
    (void) io_connect_method_scalarI_scalarO(fConnection,
		kIOHIDLibUserClientClose, NULL, 0, NULL, &len);

    fIsOpen = false;
    fIsLUNZero = false;

    return kIOReturnSuccess;
}

IOReturn IOHIDDeviceClass::setRemovalCallback(
                                   IOHIDCallbackFunction *	removalCallback,
                                   void *			removalTarget,
                                   void *			removalRefcon)
{
    return kIOReturnUnsupported;
}

IOReturn IOHIDDeviceClass::getElementValue(IOHIDElementCookie	elementCookie,
                                           IOHIDEventStruct *	valueEvent)
{
    kern_return_t           	kr = kIOReturnBadArgument;
    
    for (long index = 0; index < fElementCount; index++)
    {
        if (fElements[index].cookie == (unsigned long) elementCookie)
            {
                // get the value
                SInt32		value = 0;
                UInt64		timestamp = 0;
                
#if IOHID_PSEUDODEVICE
                value = fElements[index].currentValue;
                timestamp = __CFReadTSR();
                
                // in pseudo-device increment value
                
                // if the pause count is non-zero, then decrment the pause count, but do not change the value
                if (fElements[index].pauseCount > 0)
                    fElements[index].pauseCount--;
                // otherwise increment in direction
                else 
                {
                    fElements[index].currentValue += fElements[index].increment;
                    
                    // switch direction at ends
                    if (fElements[index].currentValue <= fElements[index].min)
                        fElements[index].increment = 1;
                    else if (fElements[index].currentValue >= fElements[index].max)
                        fElements[index].increment = -1;

                    // additional bounds check (in case we make increment greater than 1)
                    if (fElements[index].currentValue < fElements[index].min)
                        fElements[index].currentValue = fElements[index].min;
                    else if (fElements[index].currentValue > fElements[index].max)
                        fElements[index].currentValue = fElements[index].max;
                    
                    // if its a button, lets add a pause
                    if (fElements[index].type == kIOHIDElementTypeInput_Button)
                    {
                        // if we are button up, pause longer (50-1050 polls)
                        if (fElements[index].currentValue == fElements[index].min)
                            fElements[index].pauseCount = 50 + (random() % 1000);
                        // otherwise pause for a short time (0-5 polls)
                        else
                            fElements[index].pauseCount = (random() % 5);
                    }
                }

#else
                // get ptr to shared memory for this element
                if (fElements[index].valueLocation < fCurrentValuesMappedMemorySize)
                {
                   IOHIDElementValue * elementValue = (IOHIDElementValue *)
                            (fCurrentValuesMappedMemory + fElements[index].valueLocation);
                    
                    // if size is just one 32bit word
                    if (elementValue->totalSize == sizeof (IOHIDElementValue))
                    {
                        value = elementValue->value[0];
                        timestamp = *(UInt64 *)& elementValue->timestamp;
                    }
                    // еее todo long sizes
                }
#endif
                
                // fill in the event
                valueEvent->type = (IOHIDElementType) fElements[index].type;
                valueEvent->elementCookie = elementCookie;
                valueEvent->value = value;
                *(UInt64 *)& valueEvent->timestamp = timestamp;
                valueEvent->longValueSize = 0;
                valueEvent->longValue = NULL;
            
                kr = kIOReturnSuccess;
            }
    }


    return kr;
}

IOReturn IOHIDDeviceClass::setElementValue(
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction *	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon)
{
    return kIOReturnUnsupported;
}

IOReturn IOHIDDeviceClass::queryElementValue(
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction *	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon)
{
    return kIOReturnUnsupported;
}

IOReturn IOHIDDeviceClass::startAllQueues()
{
    return kIOReturnUnsupported;
}

IOReturn IOHIDDeviceClass::stopAllQueues()
{
    return kIOReturnUnsupported;
}

IOHIDQueueInterface ** IOHIDDeviceClass::allocQueue()
{
    IOHIDQueueInterface **	iohidqueue;
    HRESULT 			res;
    
    res = this->queryInterface(CFUUIDGetUUIDBytes(kIOHIDQueueInterfaceID), 
            (void **) &iohidqueue);

    return iohidqueue;
}
    
IOHIDOutputTransactionInterface ** IOHIDDeviceClass::allocOutputTransaction()
{
    IOHIDOutputTransactionInterface **	iohidoutputtransaction;
    HRESULT 				res;
    
    res = this->queryInterface(CFUUIDGetUUIDBytes(kIOHIDOutputTransactionInterfaceID), 
                (void **) &iohidoutputtransaction);

    return iohidoutputtransaction;
}

IOCFPlugInInterface IOHIDDeviceClass::sIOCFPlugInInterfaceV1 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    1, 0,	// version/revision
    &IOHIDDeviceClass::deviceProbe,
    &IOHIDDeviceClass::deviceStart,
    &IOHIDDeviceClass::deviceClose
};

IOHIDDeviceInterface IOHIDDeviceClass::sHIDDeviceInterfaceV1 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDDeviceClass::deviceCreateAsyncEventSource,
    &IOHIDDeviceClass::deviceGetAsyncEventSource,
    &IOHIDDeviceClass::deviceCreateAsyncPort,
    &IOHIDDeviceClass::deviceGetAsyncPort,
    &IOHIDDeviceClass::deviceOpen,
    &IOHIDDeviceClass::deviceClose,
    &IOHIDDeviceClass::deviceSetRemovalCallback,
    &IOHIDDeviceClass::deviceGetElementValue,
    &IOHIDDeviceClass::deviceSetElementValue,
    &IOHIDDeviceClass::deviceQueryElementValue,
    &IOHIDDeviceClass::deviceStartAllQueues,
    &IOHIDDeviceClass::deviceStopAllQueues,
    &IOHIDDeviceClass::deviceAllocQueue,
    &IOHIDDeviceClass::deviceAllocOutputTransaction
};

// Methods for routing iocfplugin interface
IOReturn IOHIDDeviceClass::
deviceProbe(void *self,
            CFDictionaryRef propertyTable,
            io_service_t inService, SInt32 *order)
    { return getThis(self)->probe(propertyTable, inService, order); }

IOReturn IOHIDDeviceClass::deviceStart(void *self,
                                            CFDictionaryRef propertyTable,
                                            io_service_t inService)
    { return getThis(self)->start(propertyTable, inService); }

IOReturn IOHIDDeviceClass::deviceStop(void *self)
    { return getThis(self)->close(); }

// Methods for routing asynchronous completion plumbing.
IOReturn IOHIDDeviceClass::
deviceCreateAsyncEventSource(void *self, CFRunLoopSourceRef *source)
    { return getThis(self)->createAsyncEventSource(source); }

CFRunLoopSourceRef IOHIDDeviceClass::
deviceGetAsyncEventSource(void *self)
    { return getThis(self)->getAsyncEventSource(); }

IOReturn IOHIDDeviceClass::
deviceCreateAsyncPort(void *self, mach_port_t *port)
    { return getThis(self)->createAsyncPort(port); }

mach_port_t IOHIDDeviceClass::
deviceGetAsyncPort(void *self)
    { return getThis(self)->getAsyncPort(); }

IOReturn IOHIDDeviceClass::deviceOpen(void *self, UInt32 flags)
    { return getThis(self)->open(flags); }

IOReturn IOHIDDeviceClass::deviceClose(void *self)
    { return getThis(self)->close(); }

IOReturn IOHIDDeviceClass::deviceSetRemovalCallback(void * 	self,
                                   IOHIDCallbackFunction *	removalCallback,
                                   void *			removalTarget,
                                   void *			removalRefcon)
    { return getThis(self)->setRemovalCallback (removalCallback,
                                                removalTarget,
                                                removalRefcon); }

IOReturn IOHIDDeviceClass::deviceGetElementValue(void * self,
                                IOHIDElementCookie	elementCookie,
                                IOHIDEventStruct *	valueEvent)
    { return getThis(self)->getElementValue (elementCookie, valueEvent); }

IOReturn IOHIDDeviceClass::deviceSetElementValue(void *	 	self,
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction *	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon)
    { return getThis(self)->setElementValue (	elementCookie,
                                                valueEvent,
                                                timeoutMS,
                                                callback,
                                                callbackTarget,
                                                callbackRefcon); }

IOReturn IOHIDDeviceClass::deviceQueryElementValue(void * 	self,
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction *	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon)
    { return getThis(self)-> queryElementValue (elementCookie,
                                                valueEvent,
                                                timeoutMS,
                                                callback,
                                                callbackTarget,
                                                callbackRefcon); }

IOReturn IOHIDDeviceClass::deviceStartAllQueues(void * self)
    { return getThis(self)->startAllQueues (); }

IOReturn IOHIDDeviceClass::deviceStopAllQueues(void * self)
    { return getThis(self)->stopAllQueues (); }

IOHIDQueueInterface ** IOHIDDeviceClass::deviceAllocQueue(void *self)
    { return getThis(self)->allocQueue (); }
    
IOHIDOutputTransactionInterface **
          IOHIDDeviceClass::deviceAllocOutputTransaction (void *self)
    { return getThis(self)->allocOutputTransaction (); }
    
kern_return_t IOHIDDeviceClass::BuildElements (CFDictionaryRef properties)
{
    kern_return_t           	kr = kIOReturnSuccess;
    long			allocatedElementCount;


    // count the number of leaves and allocate
    fElementCount = this->CountLeafElements(properties, 0);
    fElements = new IOHIDElementStruct[fElementCount];
    
    // initialize allocation to zero
    allocatedElementCount = 0;
    
    // recursively add leaf elements
    kr = this->CreateLeafElements (properties, 0, &allocatedElementCount);
    
//    printf ("%ld elements allocated of %ld expected\n", allocatedElementCount, fElementCount);
    
    // if we had errors, set the count to the number actually created
    fElementCount = allocatedElementCount;
    
#if 0
    for (long index = 0; index < fElementCount; index++)
    {
        printf ("%ld-> (%ld, %ld) %lx:%lx, type %lx\n", fElements[index].cookie, fElements[index].min, fElements[index].max, fElements[index].usage, fElements[index].usagePage, fElements[index].type);
    }
#endif
    
    return kr;
}

struct StaticWalkElementsParams
{
    IOHIDDeviceClass *		iohiddevice;
    CFDictionaryRef 		properties;
    long			value;
    void *			data;
};
typedef struct StaticWalkElementsParams StaticWalkElementsParams;

void IOHIDDeviceClass::StaticCountLeafElements (const void * value, void * parameter)
{
    // Only call array entries that are dictionaries.
    if ( CFGetTypeID(value) != CFDictionaryGetTypeID() ) 
    {
        printf ("\nIOHIDDeviceClass:Unexpected device registry structure - non-dict array\n"); // ееее make this debug only print
        return;
    }
    
    StaticWalkElementsParams * params = (StaticWalkElementsParams *) parameter;
    
    // increment count by this sub element
    params->value += params->iohiddevice->CountLeafElements(params->properties, (CFTypeRef) value);
}

// this function recersively counts the leaf elements, if zero is passed as element, it starts at top
long IOHIDDeviceClass::CountLeafElements (CFDictionaryRef properties, CFTypeRef element)
{

    // count starts zero
    long count = 0;
    
    // if element is zero, we are starting at the top
    if (element == 0)
    {
        // get the elements object
        element = CFDictionaryGetValue (properties, CFSTR(kIOHIDElementKey));
    }
    
    // get the type of the object
    CFTypeID type = CFGetTypeID(element);
    
    // if this is an array, then it is not a leaf
    if (type == CFArrayGetTypeID())
    {
        // setup param block for array callback
        StaticWalkElementsParams params;
        params.iohiddevice = this;
        params.properties = properties;
        params.value = 0;
        params.data = NULL;
        
        // count the size of the array
        CFRange range = { 0, CFArrayGetCount((CFArrayRef) element) };
        
        // call count leaf elements for each array entry
        CFArrayApplyFunction((CFArrayRef) element, range, StaticCountLeafElements, &params);
        
        // now update count
        count = params.value;
    }
    // else if it is a dictionary, then it is an element
    // either a collection element or a leaf element
    else if (type == CFDictionaryGetTypeID())
    {
        // if there are sub-elements, then this is not a leaf
        CFTypeRef subElements = CFDictionaryGetValue ((CFDictionaryRef) element, CFSTR(kIOHIDElementKey));
        if (subElements)
        {
            // recursively count leaf elements
            count = this->CountLeafElements ((CFDictionaryRef) element, subElements);
        }
        // otherwise, this is a leaf
        else
        {
            count = 1;
        }
    }
    // this case should not happen, something else was found
    else
        printf ("\nIOHIDDeviceClass:Unexpected device registry structure - strange type\n"); // ееее make this debug only print
    
    return count;
}

void IOHIDDeviceClass::StaticCreateLeafElements (const void * value, void * parameter)
{
     kern_return_t kr = kIOReturnSuccess;
    
   // Only call array entries that are dictionaries.
    if (CFGetTypeID(value) != CFDictionaryGetTypeID() ) 
        return;
    
    StaticWalkElementsParams * params = (StaticWalkElementsParams *) parameter;
    
    // increment count by this sub element
    kr = params->iohiddevice->CreateLeafElements(params->properties, (CFTypeRef) value, (long *) params->data);
    
    if (params->value == kIOReturnSuccess)
        params->value = kr;
}

// this function recersively creates the leaf elements, if zero is passed as element, it starts at top
kern_return_t IOHIDDeviceClass::CreateLeafElements (CFDictionaryRef properties, 
                                        CFTypeRef element, long * allocatedElementCount)
{
    kern_return_t 	kr = kIOReturnSuccess;

    // if element is zero, we are starting at the top
    if (element == 0)
    {
        // get the elements object
        element = CFDictionaryGetValue (properties, CFSTR(kIOHIDElementKey));
    }
    
    // get the type of the object
    CFTypeID type = CFGetTypeID(element);
    
    // if this is an array, then it is not a leaf
    if (type == CFArrayGetTypeID())
    {
        // setup param block for array callback
        StaticWalkElementsParams params;
        params.iohiddevice = this;
        params.properties = properties;
        params.value = kIOReturnSuccess;
        params.data = allocatedElementCount;
        
        // count the size of the array
        CFRange range = { 0, CFArrayGetCount((CFArrayRef) element) };
        
        // call count leaf elements for each array entry
        CFArrayApplyFunction((CFArrayRef) element, range, StaticCreateLeafElements, &params);
        
        // now update error result
        kr = params.value;
    }
    // else if it is a dictionary, then it is an element
    // either a collection element or a leaf element
    else if (type == CFDictionaryGetTypeID())
    {
        CFDictionaryRef dictionary = (CFDictionaryRef) element;
        
        // if there are sub-elements, then this is not a leaf
        CFTypeRef subElements = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementKey));
        if (subElements)
        {
            // recursively create leaf elements
            kr = this->CreateLeafElements (dictionary, subElements, allocatedElementCount);
        }
        // otherwise, this is a leaf, allocate and fill in our data
        else
        {
            IOHIDElementStruct	hidelement;
            CFTypeRef 		object;
            long 		number;
            
            // get the cookie element
            object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementCookieKey));
            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                return kIOReturnInternalError;
            if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
                return kIOReturnInternalError;
            hidelement.cookie = number;
            
            // get the element type
            object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementTypeKey));
            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                return kIOReturnInternalError;
            if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
                return kIOReturnInternalError;
            hidelement.type = number;

            // get the element min
            object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementMinKey));
            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                return kIOReturnInternalError;
            if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
                return kIOReturnInternalError;
            hidelement.min = number;
            
            // get the element max
            object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementMaxKey));
            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                return kIOReturnInternalError;
            if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
                return kIOReturnInternalError;
            hidelement.max = number;
            
            // get the element usage
            object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementUsageKey));
            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                return kIOReturnInternalError;
            if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
                return kIOReturnInternalError;
            hidelement.usage = number;
            
            // get the element usage page
            object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementUsagePageKey));
            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                return kIOReturnInternalError;
            if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
                return kIOReturnInternalError;
            hidelement.usagePage = number;
            
            // if pseudo-device, do some additional initialization
#if IOHID_PSEUDODEVICE
            hidelement.currentValue = hidelement.min;
            hidelement.pauseCount = 0;
            hidelement.increment = 1;
#else
            // otherwise, get the element value offset in shared memory for real device
            object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementValueLocationKey));
            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                return kIOReturnInternalError;
            if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
                return kIOReturnInternalError;
            hidelement.valueLocation = number;
#endif
            
            // allocate and copy the data
            fElements[(*allocatedElementCount)++] = hidelement;
        }
    }
    // this case should not happen, something else was found
    else
        printf ("\nIOHIDDeviceClass:Unexpected device registry structure - strange type\n"); // ееее make this debug only print
    
    return kr;
}

IOHIDElementType IOHIDDeviceClass::getElementType(IOHIDElementCookie elementCookie)
{
    IOHIDElementType type = (IOHIDElementType) 0;
    
    for (long index = 0; index < fElementCount; index++)
        if (fElements[index].cookie == (unsigned long) elementCookie)
            type = (IOHIDElementType) fElements[index].type;
    
    return type;
}
