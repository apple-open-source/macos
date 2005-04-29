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
#include <CoreFoundation/CFPriv.h>
//#include <IOKit/hid/IOHIDLib.h>
//#include <unistd.h>

#include "IOHIDDeviceClass.h"
#include "IOHIDQueueClass.h"
#include "IOHIDOutputTransactionClass.h"
#include "IOHIDLibUserClient.h"
#include "IOHIDPrivateKeys.h"

__BEGIN_DECLS
#include <mach/mach.h>
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
#include <IOKit/IOMessage.h>
__END_DECLS

#define connectCheck() do {	    \
    if (!fConnection)		    \
	return kIOReturnNoDevice;   \
} while (0)

#define openCheck() do {	    \
    if (!fIsOpen)		    \
        return kIOReturnNotOpen;    \
} while (0)

#define terminatedCheck() do {      \
    if (fIsTerminated)		    \
        return kIOReturnNotAttached;\
} while (0)    

#define seizeCheck() do {           \
    if (fIsSeized)                  \
        return kIOReturnExclusiveAccess; \
} while (0)

#define allChecks() do {	    \
    connectCheck();		    \
    seizeCheck();                   \
    openCheck();		    \
    terminatedCheck();              \
} while (0)

#ifndef max
#define max(a, b) \
    ((a > b) ? a:b)
#endif

#ifndef min
#define min(a, b) \
    ((a < b) ? a:b)
#endif


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
: IOHIDIUnknown(&sIOCFPlugInInterfaceV1)
{
    fHIDDevice.pseudoVTable = (IUnknownVTbl *)  &sHIDDeviceInterfaceV122;
    fHIDDevice.obj = this;

    fService 			= MACH_PORT_NULL;
    fConnection 		= MACH_PORT_NULL;
    fAsyncPort 			= MACH_PORT_NULL;
    fNotifyPort 		= MACH_PORT_NULL;

    fIsOpen 			= false;
    fIsLUNZero			= false;
    fIsTerminated		= false;
    fIsSeized			= false;
    fAsyncPortSetupDone = false;

    fRunLoop 			= NULL;
    fCFSource			= NULL;
	fNotifyCFSource		= NULL;
    fQueues			= NULL;
    fDeviceElements		= NULL;
    fReportHandlerQueue		= NULL; 
    fRemovalCallback		= NULL; 
    fRemovalTarget		= NULL;
    fRemovalRefcon		= NULL;
    fInputReportCallback	= NULL;
    fInputReportTarget 		= NULL;
    fInputReportRefcon		= NULL;

    fCachedFlags		= 0;
    
    fElementCount 		= 0;
    fElements 			= NULL;
	
	fCurrentValuesMappedMemory  = NULL;
	fCurrentValuesMappedMemorySize = NULL;

	fAsyncPrivateDataRef	= NULL;
	fNotifyPrivateDataRef   = NULL;
    
    fReportHandlerElementCount	= 0;
    fReportHandlerElements	= NULL;
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
    
    if (fReportHandlerQueue){
        delete fReportHandlerQueue;
        fReportHandlerQueue = 0;
    }
    
    if (fReportHandlerElements)
        delete[] fReportHandlerElements;
    
    if (fElements)
        delete[] fElements;
        
    if (fQueues)
        CFRelease(fQueues);
        
    if (fDeviceElements)
        CFRelease(fDeviceElements);
	
	if (fNotifyCFSource && fRunLoop)
		CFRunLoopRemoveSource(fRunLoop, fNotifyCFSource, kCFRunLoopDefaultMode);
        
    if (fNotifyPort)
        IONotificationPortDestroy(fNotifyPort);
        
    if (fAsyncPort)
        IONotificationPortDestroy(fAsyncPort);

	if (fAsyncPrivateDataRef) {
		IOObjectRelease(fAsyncPrivateDataRef->notification);
		free(fAsyncPrivateDataRef);
	}
		
	if (fNotifyPrivateDataRef) {
		IOObjectRelease(fNotifyPrivateDataRef->notification);
		free(fNotifyPrivateDataRef);
	}
}

HRESULT	IOHIDDeviceClass::attachQueue (IOHIDQueueClass * iohidQueue, bool reportHandler)
{
    HRESULT res = S_OK;
    
    iohidQueue->setOwningDevice(this);

    // ееее todo add to list
    if ( !reportHandler && ( fQueues || 
        ( fQueues = CFSetCreateMutable(kCFAllocatorDefault, 0, 0) ) ) )
    {    
        CFSetAddValue(fQueues, (void *)iohidQueue);
    }
    
    return res;
}

HRESULT	IOHIDDeviceClass::detachQueue (IOHIDQueueClass * iohidQueue)
{
    HRESULT res = S_OK;

    iohidQueue->setOwningDevice(NULL);
    
    // ееее todo remove from list
    if ( fQueues )
    {
        CFSetRemoveValue(fQueues, (void *)iohidQueue);
    }
    
    return res;
}

HRESULT IOHIDDeviceClass::attachOutputTransaction (IOHIDOutputTransactionClass * iohidOutputTrans)
{
    HRESULT res = S_OK;
    
    iohidOutputTrans->setOwningDevice(this);

    // ееее todo add to list
    
    return res;

}

HRESULT IOHIDDeviceClass::detachOutputTransaction (IOHIDOutputTransactionClass * iohidOutputTrans)
{
    HRESULT res = S_OK;

    iohidOutputTrans->setOwningDevice(NULL);
    
    // ееее todo remove from list
    
    return res;
}

IOHIDQueueClass * IOHIDDeviceClass::createQueue(bool reportHandler)
{
    IOHIDQueueClass * newQueue = new IOHIDQueueClass;
    
    // attach the queue to us
    attachQueue (newQueue, reportHandler);

	return newQueue;
}

HRESULT IOHIDDeviceClass::queryInterfaceQueue (void **ppv)
{
    HRESULT res = S_OK;
    
    // create the queue class
    IOHIDQueueClass * newQueue = createQueue();

    // add a ref for the one we return
//    newQueue->addRef();
    
    // set the return
    *ppv = newQueue->getInterfaceMap();
    
    return res;
}

HRESULT IOHIDDeviceClass::queryInterfaceOutputTransaction (void **ppv)
{
    HRESULT res = S_OK;
    
    IOHIDOutputTransactionClass * newOutputTrans = new IOHIDOutputTransactionClass;
    
    attachOutputTransaction(newOutputTrans);
    
    *ppv = newOutputTrans->getInterfaceMap();
        
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
    else if (	CFEqual(uuid, kIOHIDDeviceInterfaceID) ||  
                CFEqual(uuid, kIOHIDDeviceInterfaceID121) ||
                CFEqual(uuid, kIOHIDDeviceInterfaceID122) )
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
    IOReturn 			res;
    kern_return_t 		kr;
    mach_port_t 		masterPort;
    CFMutableDictionaryRef	properties;

    fService = inService;
    IOObjectRetain(fService);
    
    res = IOServiceOpen(fService, mach_task_self(), 0, &fConnection);
    if (res != kIOReturnSuccess)
        return res;

    connectCheck();

    // First create a master_port for my task
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr || !masterPort)
        return kIOReturnError;
    
    fNotifyPort = IONotificationPortCreate(masterPort);
    fNotifyCFSource = IONotificationPortGetRunLoopSource(fNotifyPort);
    
    fRunLoop = CFRunLoopGetMain();//CFRunLoopGetCurrent();
    CFRunLoopAddSource(fRunLoop, fNotifyCFSource, kCFRunLoopDefaultMode);

    fNotifyPrivateDataRef = (MyPrivateData *)malloc(sizeof(MyPrivateData));
    bzero(fNotifyPrivateDataRef, sizeof(MyPrivateData));
    
    fNotifyPrivateDataRef->self		= this;

    // Register for an interest notification of this device being removed. Use a reference to our
    // private data as the refCon which will be passed to the notification callback.
    kr = IOServiceAddInterestNotification( fNotifyPort,
                                            fService,
                                            kIOGeneralInterest,
                                            IOHIDDeviceClass::_deviceNotification,
                                            fNotifyPrivateDataRef,
                                            &(fNotifyPrivateDataRef->notification));

    // Now done with the master_port
    mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = 0;
    
    kr = IORegistryEntryCreateCFProperties (fService,
                                            &properties,
                                            kCFAllocatorDefault,
                                            kNilOptions );

    if ( !properties || (kr != kIOReturnSuccess))
        return kIOReturnError;
        
    fDeviceElements = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);    
    if ( !fDeviceElements )
        return kIOReturnError;
        
    BuildElements((CFDictionaryRef) properties, fDeviceElements);
    FindReportHandlers((CFDictionaryRef) properties); 
    CFRelease(properties);       

    return kIOReturnSuccess;
}

// RY: There are 2 General Interest notification event sources.
// One is operating on the main run loop via fNotifyPort and is internal 
// to the IOHIDDeviceClass.  The other is used by the client to get removal 
// notification via fAsyncPort.  This method is used by both and disguished 
// by the port in the refcon.
void IOHIDDeviceClass::_deviceNotification( void *refCon,
                                            io_service_t service,
                                            natural_t messageType,
                                            void *messageArgument )
{
    IOHIDDeviceClass	*self;
    MyPrivateData	*privateDataRef = (MyPrivateData *) refCon;
    IOOptionBits	options = (IOOptionBits)messageArgument;
    
    if (!privateDataRef)
        return;
    
    self = privateDataRef->self;
    
    if (!self)
        return;

    switch(messageType)
    {
        case kIOMessageServiceIsTerminated:
				
            self->fIsTerminated = true;
            
			if ( privateDataRef != self->fAsyncPrivateDataRef)
				break;
            
            if (self->fRemovalCallback)
            {            
                ((IOHIDCallbackFunction)self->fRemovalCallback)(
                                                self->fRemovalTarget,
                                                kIOReturnSuccess,
                                                self->fRemovalRefcon,
                                                (void *)&(self->fHIDDevice));
            }
            // Free up the notificaiton
            IOObjectRelease(privateDataRef->notification);
            free(self->fAsyncPrivateDataRef);
			self->fAsyncPrivateDataRef = 0;
            break;
        
        case kIOMessageServiceIsRequestingClose:
			if ( privateDataRef != self->fNotifyPrivateDataRef)
				break;

            if ((options & kIOHIDOptionsTypeSeizeDevice) &&
                (options != self->fCachedFlags))
            {
                self->stopAllQueues(true);
                if (self->fReportHandlerQueue)
                    self->fReportHandlerQueue->stop();
				self->close();
                self->fIsSeized = true;
            }
            break;
            
        case kIOMessageServiceWasClosed:
			if ( privateDataRef != self->fNotifyPrivateDataRef)
				break;

            if (self->fIsSeized &&
                (options & kIOHIDOptionsTypeSeizeDevice) &&
                (options != self->fCachedFlags))
            {
                self->fIsSeized = false;

				self->open(self->fCachedFlags);

                self->startAllQueues(true);
                if (self->fReportHandlerQueue)
                    self->fReportHandlerQueue->start();
            }
            break;
    }    
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

    cfPort = CFMachPortCreateWithPort(NULL, IONotificationPortGetMachPort(fAsyncPort),
                (CFMachPortCallBack) _cfmachPortCallback,
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
    IOReturn		ret;
    mach_port_t		masterPort;

    connectCheck();
    
    // If we already have a port, don't create a new one.
    if (fAsyncPort) {
        if (port)
            *port = IONotificationPortGetMachPort(fAsyncPort);
        return kIOReturnSuccess;
    }

    // First create a master_port for my task
    ret = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (ret || !masterPort)
        return kIOReturnError;

	fAsyncPort = IONotificationPortCreate(masterPort);
	
    if (fAsyncPort) {
        if (port)
            *port = IONotificationPortGetMachPort(fAsyncPort);

        if (fIsOpen) {
			ret = finishAsyncPortSetup();
        }
    }
    
	mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = 0;

    return ret;
}

IOReturn IOHIDDeviceClass::finishAsyncPortSetup()
{
	natural_t				asyncRef[1];
	mach_msg_type_number_t  len = 0;
			
	finishReportHandlerQueueSetup();

    fAsyncPortSetupDone = true;

	// async kIOCDBUserClientSetAsyncPort,  kIOUCScalarIScalarO,    0,	0
	return io_async_method_scalarI_scalarO(
			fConnection, IONotificationPortGetMachPort(fAsyncPort), asyncRef, 1,
			kIOHIDLibUserClientSetAsyncPort, NULL, 0, NULL, &len);
}

mach_port_t IOHIDDeviceClass::getAsyncPort()
{
    return IONotificationPortGetMachPort(fAsyncPort);
}

IOReturn IOHIDDeviceClass::open(UInt32 flags)
{
    IOReturn ret = kIOReturnSuccess;
    int input[1];

    connectCheck();

    // ее╩todo, check flags to see if different (if so, we might need to reopen)
    if (fIsOpen)
        return kIOReturnSuccess;
      
    input[0] = flags;
    fCachedFlags = flags;

    mach_msg_type_number_t len = 0;

    //  kIOHIDLibUserClientOpen,  kIOUCScalarIScalarO,    0,	0
    ret = io_connect_method_scalarI_scalarO(
            fConnection, kIOHIDLibUserClientOpen, input, 1, NULL, &len);
    if (ret != kIOReturnSuccess) {
        fCachedFlags = 0;
        return ret;
    }

    fIsOpen = true;
    fIsSeized = false;

    if (!fAsyncPortSetupDone && fAsyncPort) {
		ret = finishAsyncPortSetup();

		if (ret != kIOReturnSuccess) {
            close();
            return ret;
        }
    }
    
    // get the shared memory
	if ( !fCurrentValuesMappedMemory )
	{
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
	}
    
    return ret;
}

IOReturn IOHIDDeviceClass::close()
{
    openCheck();
    connectCheck();

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
                                   IOHIDCallbackFunction 	removalCallback,
                                   void *			removalTarget,
                                   void *			removalRefcon)
{	
	IOReturn ret = kIOReturnSuccess;

	if (!fAsyncPrivateDataRef)
	{
		
		fAsyncPrivateDataRef = (MyPrivateData *)malloc(sizeof(MyPrivateData));
		bzero(fAsyncPrivateDataRef, sizeof(MyPrivateData));
		
		fAsyncPrivateDataRef->self		= this;

        if (!fAsyncPort) {     
            ret = createAsyncPort(0);
            if (kIOReturnSuccess != ret)
                return ret;
        }

		// Register for an interest notification of this device being removed. Use a reference to our
		// private data as the refCon which will be passed to the notification callback.
		ret = IOServiceAddInterestNotification( fAsyncPort,
												fService,
												kIOGeneralInterest,
												IOHIDDeviceClass::_deviceNotification,
												fAsyncPrivateDataRef,
												&(fAsyncPrivateDataRef->notification));
	}
	
    fRemovalCallback    = removalCallback;
    fRemovalTarget      = removalTarget;
    fRemovalRefcon      = removalRefcon;

    return ret;
}

IOReturn IOHIDDeviceClass::getElementValue(IOHIDElementCookie	elementCookie,
                                           IOHIDEventStruct *	valueEvent)
{
    IOReturn 	kr;
    
    kr = fillElementValue(elementCookie, valueEvent);
    
    // If the timestamp is 0, this element has never
    // been processed.  We should query the element
    //  to get the current value.
    if ( (*(UInt64 *)&valueEvent->timestamp == 0) && 
        (kr == kIOReturnSuccess) && 
        (valueEvent->type == kIOHIDElementTypeFeature))
    {
        kr = queryElementValue (elementCookie,
                            valueEvent,
                            0,
                            NULL,
                            NULL,
                            NULL);
    }
    
    return kr;
}

IOReturn IOHIDDeviceClass::setElementValue(
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon,
                                bool				pushToDevice)
{
    kern_return_t           	kr = kIOReturnBadArgument;
    IOHIDElementStruct		element;
    
    allChecks();

    if (!getElement(elementCookie, &element))
        return kr;

    // we are only interested feature and output elements
    if ((element.type != kIOHIDElementTypeFeature) && 
            (element.type != kIOHIDElementTypeOutput))
        return kr;
                
    // get ptr to shared memory for this element
    if (element.valueLocation < fCurrentValuesMappedMemorySize)
    {
        IOHIDElementValue * elementValue = (IOHIDElementValue *)
                (fCurrentValuesMappedMemory + element.valueLocation);
                
        // if size is just one 32bit word
        if (elementValue->totalSize == sizeof (IOHIDElementValue))
        {
            //elementValue->cookie = valueEvent->elementCookie;
            elementValue->value[0] = valueEvent->value;
            //elementValue->timestamp = valueEvent->timestamp;
        }
        // handle the long value size case.
        // we are assuming here that the end user has an allocated
        // longValue buffer.
        else if (elementValue->totalSize > sizeof (IOHIDElementValue))
        {
            UInt32 longValueSize = valueEvent->longValueSize;
            
            if ((longValueSize > (UInt32)element.bytes) ||
                ( valueEvent->longValue == NULL))
                return kr;
                
            bzero(&(elementValue->value), 
                (elementValue->totalSize - sizeof(IOHIDElementValue)) + sizeof(UInt32));
            
            // *** FIX ME ***
            // Since we are setting mapped memory, we should probably
            // hold a shared lock
            convertByteToWord ((const UInt8 *)valueEvent->longValue, elementValue->value, longValueSize<<3);
            //elementValue->timestamp = valueEvent->timestamp;
        }
        
        // Don't push the value out to the device if not told to.  
        // This is needed for transactions.
        if (!pushToDevice)
            return kIOReturnSuccess;
                        
        UInt32			input[1];
        IOByteCount		outputCount = 0;
                
        input[0] = (UInt32) elementCookie;
        
        //  kIOHIDLibUserClientPostElementValue,  kIOUCStructIStructO,    1,	0
        kr = io_connect_method_structureI_structureO(
                fConnection, kIOHIDLibUserClientPostElementValue, 
                (char *)input, sizeof(UInt32), NULL, (mach_msg_type_number_t *)&outputCount);
        
    }
        
            
    
    return kr;
}

IOReturn IOHIDDeviceClass::queryElementValue(
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon)
{
    IOReturn			ret = kIOReturnBadArgument;
    IOHIDElementStruct		element;
    int				input[1];
    mach_msg_type_number_t	len = 0;
        
    input[0] = (int) elementCookie;

    allChecks();

    if (!getElement(elementCookie, &element))
        return ret;

    //  kIOHIDLibUserClientUpdateElementValue,  kIOUCScalarIScalarO,    1,	0
    ret = io_connect_method_scalarI_scalarO(
            fConnection, 
            kIOHIDLibUserClientUpdateElementValue, 
            input, 1, NULL, &len);
            
    if (ret == kIOReturnSuccess)
        ret = fillElementValue(elementCookie, valueEvent);
    
    return ret;

}

IOReturn IOHIDDeviceClass::fillElementValue(IOHIDElementCookie		elementCookie,
                                            IOHIDEventStruct *		valueEvent)
{
    IOHIDElementStruct	element;

    allChecks();
    
    if (!getElement(elementCookie, &element) || (element.type == kIOHIDElementTypeCollection))
        return kIOReturnBadArgument;
        
    // get the value
    SInt32		value = 0;
    void *		longValue = 0;
    UInt32		longValueSize = 0;
    UInt64		timestamp = 0;
        
    // get ptr to shared memory for this element
    if (element.valueLocation < fCurrentValuesMappedMemorySize)
    {
        IOHIDElementValue * elementValue = (IOHIDElementValue *)
                (fCurrentValuesMappedMemory + element.valueLocation);
        
        // if size is just one 32bit word
        if (elementValue->totalSize == sizeof (IOHIDElementValue))
        {
            value = elementValue->value[0];
            timestamp = *(UInt64 *)& elementValue->timestamp;
        }
        // handle the long value size case.
        // we are assuming here that the end user will deallocate
        // the longValue buffer.
        else if (elementValue->totalSize > sizeof (IOHIDElementValue))
        {
            longValueSize = element.bytes;
            longValue = malloc ( longValueSize );
            bzero(longValue, longValueSize);

            // *** FIX ME ***
            // Since we are getting mapped memory, we should probably
            // hold a shared lock
            convertWordToByte((const UInt32 *)elementValue->value, (UInt8 *)longValue, longValueSize<<3);
            
            timestamp = *(UInt64 *)& elementValue->timestamp;
        }
    }
    
    // fill in the event
    valueEvent->type = (IOHIDElementType) element.type;
    valueEvent->elementCookie = elementCookie;
    valueEvent->value = value;
    *(UInt64 *)& valueEvent->timestamp = timestamp;
    valueEvent->longValueSize = longValueSize;
    valueEvent->longValue = longValue;

    return kIOReturnSuccess;
    
}

struct IOHIDReportRefCon {
    IOHIDReportCallbackFunction	callback;
    void *			callbackTarget;
    void *			callbackRefcon;
    void *			sender;
};

// Added functions Post Jaguar
IOReturn 
IOHIDDeviceClass::setReport (	IOHIDReportType			reportType,
                                UInt32				reportID,
                                void *				reportBuffer,
                                UInt32				reportBufferSize,
                                UInt32 				timeoutMS,
                                IOHIDReportCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon)
{
    int				in[5];
    mach_msg_type_number_t	len = 0;
    IOReturn			ret;

    allChecks();

    // Async setReport
    if (callback) 
    {
        if (!fAsyncPort)
            return kIOReturnError; //kIOUSBNoAsyncPortErr;
            
        natural_t		asyncRef[kIOAsyncCalloutCount];
        IOHIDReportRefCon	* hidRefcon = 0;

    
        in[0] = reportType;
        in[1] = reportID;
        in[2] = (natural_t)reportBuffer;
        in[3] = reportBufferSize;
        in[4] = timeoutMS; 
        
        hidRefcon = (IOHIDReportRefCon *)malloc(sizeof(IOHIDReportRefCon));
        
        if (!hidRefcon)
            return kIOReturnError;
            
        hidRefcon->callback		= callback;
        hidRefcon->callbackTarget 	= callbackTarget;
        hidRefcon->callbackRefcon 	= callbackRefcon;
        hidRefcon->sender		= &fHIDDevice;
    
        asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) _hidReportCallback;
        asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) hidRefcon;
    
        ret = io_async_method_scalarI_scalarO( fConnection, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, kIOHIDLibUserClientAsyncSetReport, in, 5, NULL, &len);
    
    }
    else
    {
        if(reportBufferSize < sizeof(io_struct_inband_t)) 
        {
            in[0] = reportType;
            in[1] = reportID;
            ret = io_connect_method_scalarI_structureI( fConnection, kIOHIDLibUserClientSetReport, in, 2, (char *)reportBuffer, reportBufferSize);
        }
        else 
        {
            IOHIDReportReq		req;
                        
            req.reportType = reportType;
            req.reportID = reportID;
            req.reportBuffer = reportBuffer;
            req.reportBufferSize = reportBufferSize;
            
            ret = io_connect_method_structureI_structureO( fConnection, kIOHIDLibUserClientSetReportOOL, (char*)&req, sizeof(req), NULL, &len);
        }    
    }
    
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;

}


IOReturn 
IOHIDDeviceClass::getReport (	IOHIDReportType			reportType,
                                UInt32				reportID,
                                void *				reportBuffer,
                                UInt32 *			reportBufferSize,
                                UInt32 				timeoutMS,
                                IOHIDReportCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon)
{
    int				in[5];
    mach_msg_type_number_t	len = 0;
    IOReturn			ret;

    allChecks();

    // Async getReport
    if (callback) 
    {
        if (!fAsyncPort)
            return kIOReturnError; //kIOUSBNoAsyncPortErr;
            
        natural_t		asyncRef[kIOAsyncCalloutCount];
        IOHIDReportRefCon	* hidRefcon = 0;

    
        in[0] = reportType;
        in[1] = reportID;
        in[2] = (natural_t)reportBuffer;
        in[3] = *reportBufferSize;
        in[4] = timeoutMS; 
        
        hidRefcon = (IOHIDReportRefCon *)malloc(sizeof(IOHIDReportRefCon));
        
        if (!hidRefcon)
            return kIOReturnError;
            
        hidRefcon->callback		= callback;
        hidRefcon->callbackTarget 	= callbackTarget;
        hidRefcon->callbackRefcon 	= callbackRefcon;
        hidRefcon->sender		= &fHIDDevice;
    
        asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) _hidReportCallback;
        asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) hidRefcon;
    
        ret = io_async_method_scalarI_scalarO( fConnection, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, kIOHIDLibUserClientAsyncGetReport, in, 5, NULL, &len);
    
    }
    else
    {
        if(*reportBufferSize < sizeof(io_struct_inband_t)) 
        {
            in[0] = reportType;
            in[1] = reportID;
            ret = io_connect_method_scalarI_structureO( fConnection, kIOHIDLibUserClientGetReport, in, 2, (char *)reportBuffer, (unsigned int *)reportBufferSize);
        }
        else 
        {
            IOHIDReportReq		req;
            
            len = sizeof(*reportBufferSize);
            
            req.reportType = reportType;
            req.reportID = reportID;
            req.reportBuffer = reportBuffer;
            req.reportBufferSize = *reportBufferSize;
            
            ret = io_connect_method_structureI_structureO( fConnection, kIOHIDLibUserClientGetReportOOL, (char*)&req, sizeof(req), (char*)reportBufferSize, &len);
        }    
    }
    
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}

//---------------------------------------------------------------------------
// Compare the properties in the supplied table to this object's properties.

static bool CompareProperty( CFDictionaryRef element, CFDictionaryRef matching, CFStringRef key )
{
    // We return success if we match the key in the dictionary with the key in
    // the property table, or if the prop isn't present
    //
    CFTypeRef 	elementValue;
    CFTypeRef	matchValue;
    bool	matches = true;
    
    elementValue = CFDictionaryGetValue(element, key);
    matchValue = CFDictionaryGetValue(matching, key);

    if( elementValue && matchValue )
        matches = CFEqual(elementValue, matchValue);
    else if (!elementValue && matchValue)
        matches = false;

    return matches;
}

IOReturn 
IOHIDDeviceClass::copyMatchingElements(CFDictionaryRef matchingDict, CFArrayRef *elements)
{    
    if (!elements)
        return kIOReturnBadArgument;
     
    if ( matchingDict )
    {
        CFMutableArrayRef	tempElements = 0;
        CFDictionaryRef     element;

        if (!(tempElements = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks)))
        {
            *elements = 0;
            return kIOReturnNoMemory;
        }
            
        for (int i=0; i<fElementCount; i++)
        {
            if ( !(element = fElements[i].elementDictionaryRef) )
                continue;
                
            // Compare properties.        
            if (CompareProperty(element, matchingDict, CFSTR(kIOHIDElementCookieKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementTypeKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementCollectionTypeKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementUsageKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementUsagePageKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementMinKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementMaxKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementScaledMinKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementScaledMaxKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementSizeKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementReportSizeKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementReportCountKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementIsArrayKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementIsRelativeKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementIsWrappingKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementIsNonLinearKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementHasPreferredStateKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementHasNullStateKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementVendorSpecificKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementUnitKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementUnitExponentKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementNameKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementValueLocationKey))
                && CompareProperty(element, matchingDict, CFSTR(kIOHIDElementDuplicateIndexKey)))
            {            
                CFArrayAppendValue(tempElements, element);
            }
        }

        *elements = tempElements;
    }
    else if (!(*elements = CFArrayCreateCopy(kCFAllocatorDefault, fDeviceElements)))
        return kIOReturnNoMemory;    

    if (CFArrayGetCount(*elements) == 0)
    {
        CFRelease(*elements);
        *elements = 0;
    }
    
    return kIOReturnSuccess;
}

IOReturn IOHIDDeviceClass::setInterruptReportHandlerCallback(
                                void *				reportBuffer,
                                UInt32				reportBufferSize,
                                IOHIDReportCallbackFunction 	callback, 
                                void * 				callbackTarget, 
                                void * 				callbackRefcon)
                                
{
    IOReturn		ret = kIOReturnSuccess;

    fInputReportCallback 	= callback;
    fInputReportTarget		= callbackTarget;
    fInputReportRefcon		= callbackRefcon;
    fInputReportBuffer		= reportBuffer;
    fInputReportBufferSize	= reportBufferSize;   
     
    // Lazy set up of the queue.
    if ( !fReportHandlerQueue )
    {
		fReportHandlerQueue = createQueue(true);

        ret = fReportHandlerQueue->create(0, 8);
        
        if (ret != kIOReturnSuccess)
            goto SET_REPORT_HANDLER_CLEANUP;
        
        for (int i=0; i<fReportHandlerElementCount; i++)
        {
            ret = fReportHandlerQueue->addElement((IOHIDElementCookie)fReportHandlerElements[i].cookie, 0);
    
            if (ret != kIOReturnSuccess)
                goto SET_REPORT_HANDLER_CLEANUP;
        }

		
		if ( fAsyncPort && fIsOpen )
		{
			ret = finishReportHandlerQueueSetup();
			if (ret != kIOReturnSuccess)
				goto SET_REPORT_HANDLER_CLEANUP;
		}
    }    
    
    return kIOReturnSuccess;
    
SET_REPORT_HANDLER_CLEANUP:
    delete fReportHandlerQueue;
    fReportHandlerQueue = 0;
    
    return ret;
}

IOReturn IOHIDDeviceClass::finishReportHandlerQueueSetup()
{
	IOReturn ret = kIOReturnError;
	
	if ( fReportHandlerQueue )
	{
		do {
			ret = fReportHandlerQueue->setEventCallout(_hidReportHandlerCallback, this, 0);
			
			if (ret != kIOReturnSuccess) break;
				
			ret = fReportHandlerQueue->setAsyncPort(IONotificationPortGetMachPort(fAsyncPort));
			
			if (ret != kIOReturnSuccess) break;
					
			ret = fReportHandlerQueue->start();

			if (ret != kIOReturnSuccess) break;
            			
		} while ( false );
	}
	return ret;
}

void IOHIDDeviceClass::_cfmachPortCallback(CFMachPortRef cfPort, mach_msg_header_t *msg, CFIndex size, void *info) {
    mach_msg_header_t *			msgh = (mach_msg_header_t *)msg;
	IOHIDDeviceClass *			self = (IOHIDDeviceClass *) info;
	
	if ( !self )
		return;
		
    if( msgh->msgh_id == kOSNotificationMessageID)
		IODispatchCalloutFromMessage(cfPort, msg, info);
	else if ( self->fReportHandlerQueue )
		IOHIDQueueClass::queueEventSourceCallback(cfPort, msg, size, self->fReportHandlerQueue);
}

void IOHIDDeviceClass::_hidReportHandlerCallback(void * target, IOReturn result, void * refcon, void * sender)
{
    IOHIDEventStruct 		event;
    IOHIDElementStruct		element;
    IOHIDDeviceClass *		self = (IOHIDDeviceClass *)target;
    IOHIDQueueClass *		queue = self->fReportHandlerQueue;
    AbsoluteTime 		zeroTime = {0,0};
    UInt32			size = 0;

    if (!self || !self->fIsOpen)
        return;
            
    while ((result = queue->getNextEvent( &event, zeroTime, 0)) == kIOReturnSuccess) {
        
        if ( event.longValueSize == 0)
        {                        
            self->getElement(event.elementCookie, &element);                
            size = element.bytes;
            size = min(size, self->fInputReportBufferSize);
            bzero(self->fInputReportBuffer, size);

            self->convertWordToByte((const UInt32 *)(&(event.value)), (UInt8 *)self->fInputReportBuffer, size << 3);
            
        }
        else if (event.longValueSize != 0 && (event.longValue != NULL))
        {
            size = min(self->fInputReportBufferSize, event.longValueSize);
            bcopy(event.longValue, self->fInputReportBuffer, size);
            free(event.longValue);
        }

        if (!self->fInputReportCallback)
            return;
            
        (self->fInputReportCallback)(   self->fInputReportTarget, 
                                        result, 
                                        self->fInputReportRefcon, 
                                        &(self->fHIDDevice),
                                        size);
    }
}


void 
IOHIDDeviceClass::_hidReportCallback(void *refcon, IOReturn result, UInt32 bufferSize)
{
    IOHIDReportRefCon *hidRefcon = (IOHIDReportRefCon *)refcon;
    
    if (!hidRefcon || !hidRefcon->callback)
        return;
    
    ((IOHIDReportCallbackFunction)hidRefcon->callback)( hidRefcon->callbackTarget,
                                                        result,
                                                        hidRefcon->callbackRefcon,
                                                        hidRefcon->sender,
                                                        bufferSize);
                                                    
    free(hidRefcon);
}

//---------------------------------------------------------------------------
// Not very efficient, will do for now.

#define BIT_MASK(bits)  ((1 << (bits)) - 1)

#define UpdateByteOffsetAndShift(bits, offset, shift)  \
    do { offset = bits >> 3; shift = bits & 0x07; } while (0)

#define UpdateWordOffsetAndShift(bits, offset, shift)  \
    do { offset = bits >> 5; shift = bits & 0x1f; } while (0)
    
void IOHIDDeviceClass::convertByteToWord( const UInt8 * src,
                           UInt32 *      dst,
                           UInt32        bitsToCopy)
{
    UInt32 srcOffset;
    UInt32 srcShift;
    UInt32 srcStartBit   = 0;
    UInt32 dstShift      = 0;
    UInt32 dstStartBit   = 0;
    UInt32 dstOffset     = 0;
    UInt32 lastDstOffset = 0;
    UInt32 word          = 0;
    UInt8  bitsProcessed;
    UInt32 totalBitsProcessed = 0;

    while ( bitsToCopy )
    {
        UInt32 tmp;

        UpdateByteOffsetAndShift( srcStartBit, srcOffset, srcShift );

        bitsProcessed = min( bitsToCopy,
                             min( 8 - srcShift, 32 - dstShift ) );

        tmp = (src[srcOffset] >> srcShift) & BIT_MASK(bitsProcessed);

        word |= ( tmp << dstShift );

        dstStartBit += bitsProcessed;
        srcStartBit += bitsProcessed;
        bitsToCopy  -= bitsProcessed;
		totalBitsProcessed += bitsProcessed;

        UpdateWordOffsetAndShift( dstStartBit, dstOffset, dstShift );

        if ( ( dstOffset != lastDstOffset ) || ( bitsToCopy == 0 ) )
        {
            dst[lastDstOffset] = word;
            word = 0;
            lastDstOffset = dstOffset;
        }
    }
}

void IOHIDDeviceClass::convertWordToByte( const UInt32 * src,
                           UInt8 *        dst,
                           UInt32         bitsToCopy)
{
    UInt32 dstOffset;
    UInt32 dstShift;
    UInt32 dstStartBit = 0;
    UInt32 srcShift    = 0;
    UInt32 srcStartBit = 0;
    UInt32 srcOffset   = 0;
    UInt8  bitsProcessed;
    UInt32 tmp;

    while ( bitsToCopy )
    {
        UpdateByteOffsetAndShift( dstStartBit, dstOffset, dstShift );

        bitsProcessed = min( bitsToCopy,
                             min( 8 - dstShift, 32 - srcShift ) );

        tmp = (src[srcOffset] >> srcShift) & BIT_MASK(bitsProcessed);

        dst[dstOffset] |= ( tmp << dstShift );

        dstStartBit += bitsProcessed;
        srcStartBit += bitsProcessed;
        bitsToCopy  -= bitsProcessed;

        UpdateWordOffsetAndShift( srcStartBit, srcOffset, srcShift );
    }
}

IOReturn IOHIDDeviceClass::startAllQueues(bool deviceInitiated)
{
    IOReturn ret = kIOReturnSuccess;
    
    if ( fQueues )
    {
        int			queueCount = CFSetGetCount(fQueues);
        IOHIDQueueClass **	queues = NULL;
        
        queues = (IOHIDQueueClass **)malloc(sizeof(IOHIDQueueClass *) * queueCount);
    
        CFSetGetValues(fQueues, (const void **)queues);
        
        for (int i=0; queues && i<queueCount; i++)
        {
            ret = queues[i]->start(deviceInitiated);
        }
        
        if (queues)
            free(queues);

    }

    return ret;
}

IOReturn IOHIDDeviceClass::stopAllQueues(bool deviceInitiated)
{
    IOReturn ret = kIOReturnSuccess;
    
    if ( fQueues )
    {
        int			queueCount = CFSetGetCount(fQueues);
        IOHIDQueueClass **	queues	= NULL;
    
        queues = (IOHIDQueueClass **)malloc(sizeof(IOHIDQueueClass *) * queueCount);

        CFSetGetValues(fQueues, (const void **)queues);
        
        for (int i=0; queues && i<queueCount && ret==kIOReturnSuccess; i++)
        {
            ret = queues[i]->stop(deviceInitiated);
        }
        
        if (queues)
            free(queues);

    }

    return ret;
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

IOHIDDeviceInterface122 IOHIDDeviceClass::sHIDDeviceInterfaceV122 =
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
    &IOHIDDeviceClass::deviceAllocOutputTransaction,
    // New with version 1.2.1
    &IOHIDDeviceClass::deviceSetReport,
    &IOHIDDeviceClass::deviceGetReport,
    // New with version 1.2.2
    &IOHIDDeviceClass::deviceCopyMatchingElements,
    &IOHIDDeviceClass::deviceSetInterruptReportHandlerCallback
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
                                   IOHIDCallbackFunction	removalCallback,
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
                                IOHIDElementCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon)
    { return getThis(self)->setElementValue (	elementCookie,
                                                valueEvent,
                                                timeoutMS,
                                                callback,
                                                callbackTarget,
                                                callbackRefcon, 
                                                true); }

IOReturn IOHIDDeviceClass::deviceQueryElementValue(void * 	self,
                                IOHIDElementCookie		elementCookie,
                                IOHIDEventStruct *		valueEvent,
                                UInt32 				timeoutMS,
                                IOHIDElementCallbackFunction	callback,
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
    

// Added methods
IOReturn 
IOHIDDeviceClass::deviceSetReport (void * 			self,
                                IOHIDReportType			reportType,
                                UInt32				reportID,
                                void *				reportBuffer,
                                UInt32				reportBufferSize,
                                UInt32 				timeoutMS,
                                IOHIDReportCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon)
{
    return getThis(self)->setReport(reportType, reportID, reportBuffer, reportBufferSize, timeoutMS, callback, callbackTarget, callbackRefcon);
}


IOReturn 
IOHIDDeviceClass::deviceGetReport (void * 			self,
                                IOHIDReportType			reportType,
                                UInt32				reportID,
                                void *				reportBuffer,
                                UInt32 *			reportBufferSize,
                                UInt32 				timeoutMS,
                                IOHIDReportCallbackFunction	callback,
                                void * 				callbackTarget,
                                void *				callbackRefcon)
{
    return getThis(self)->getReport(reportType, reportID, reportBuffer, reportBufferSize, timeoutMS, callback, callbackTarget, callbackRefcon);
}

IOReturn 
IOHIDDeviceClass::deviceCopyMatchingElements(void * self, CFDictionaryRef matchingDict, CFArrayRef *elements)
{
    return getThis(self)->copyMatchingElements(matchingDict, elements);
}

IOReturn 
IOHIDDeviceClass::deviceSetInterruptReportHandlerCallback(void * 	self, 
                                void *				reportBuffer,
                                UInt32				reportBufferSize,
                                IOHIDReportCallbackFunction 	callback, 
                                void * 				callbackTarget, 
                                void * 				callbackRefcon)
{
    return getThis(self)->setInterruptReportHandlerCallback(reportBuffer, reportBufferSize, callback, callbackTarget, callbackRefcon);
}

// End added methods

kern_return_t IOHIDDeviceClass::BuildElements (CFDictionaryRef properties, CFMutableArrayRef array)
{
    kern_return_t           	kr = kIOReturnSuccess;
    long			allocatedElementCount;


    // count the number of leaves and allocate
    fElementCount = this->CountElements(properties, 0, CFSTR(kIOHIDElementKey));
    fElements = new IOHIDElementStruct[fElementCount];
    
    // initialize allocation to zero
    allocatedElementCount = 0;
    
    // recursively add leaf elements
    kr = CreateLeafElements (properties, array, 0, &allocatedElementCount, CFSTR(kIOHIDElementKey), fElements);
    
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
    CFMutableArrayRef		array;
    CFStringRef			key;
    IOHIDElementStruct *	elements;
    long			value;
    void *			data;
};
typedef struct StaticWalkElementsParams StaticWalkElementsParams;

void IOHIDDeviceClass::StaticCountElements (const void * value, void * parameter)
{
    // Only call array entries that are dictionaries.
    if ( CFGetTypeID(value) != CFDictionaryGetTypeID() ) 
    {
        printf ("\nIOHIDDeviceClass:Unexpected device registry structure - non-dict array\n"); // ееее make this debug only print
        return;
    }
    
    StaticWalkElementsParams * params = (StaticWalkElementsParams *) parameter;
    
    // increment count by this sub element
    params->value += params->iohiddevice->CountElements(params->properties, (CFTypeRef) value, params->key);
}

// this function recersively counts the leaf elements, if zero is passed as element, it starts at top
long IOHIDDeviceClass::CountElements (CFDictionaryRef properties, CFTypeRef element, CFStringRef key)
{

    // count starts zero
    long count = 0;
    
    // if element is zero, we are starting at the top
    if (element == 0)
    {
        // get the elements object
        element = CFDictionaryGetValue (properties, key);
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
        params.key = key;
        params.value = 0;
        params.data = NULL;
        
        // count the size of the array
        CFRange range = { 0, CFArrayGetCount((CFArrayRef) element) };
        
        // call count leaf elements for each array entry
        CFArrayApplyFunction((CFArrayRef) element, range, StaticCountElements, &params);
        
        // now update count
        count = params.value;
    }
    // else if it is a dictionary, then it is an element
    // either a collection element or a leaf element
    else if (type == CFDictionaryGetTypeID())
    {
        count = 1;
        // if there are sub-elements, then this is not a leaf
        CFTypeRef subElements = CFDictionaryGetValue ((CFDictionaryRef) element, CFSTR(kIOHIDElementKey));
        if (subElements)
        {
            // recursively count leaf elements
            count += this->CountElements ((CFDictionaryRef) element, subElements, key);
        }
        
        if (CFDictionaryGetValue ((CFDictionaryRef) element, CFSTR(kIOHIDElementDuplicateValueSizeKey)))
        {
            CFNumberRef numberRef;
            UInt32      duplicateCount;
            
            if ( (numberRef = (CFNumberRef) CFDictionaryGetValue ((CFDictionaryRef) element, CFSTR(kIOHIDElementReportCountKey)))
                 && CFNumberGetValue(numberRef, kCFNumberLongType, &duplicateCount))
            {
                count += duplicateCount;
            }
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
    kr = params->iohiddevice->CreateLeafElements(params->properties, params->array, (CFTypeRef) value, (long *) params->data, params->key, params->elements);
    
    if (params->value == kIOReturnSuccess)
        params->value = kr;
}

// this function recersively creates the leaf elements, if zero is passed as element, it starts at top
kern_return_t IOHIDDeviceClass::CreateLeafElements (CFDictionaryRef properties, CFMutableArrayRef array,
                    CFTypeRef element, long * allocatedElementCount, CFStringRef key, IOHIDElementStruct * elements)
{
    kern_return_t 	kr = kIOReturnSuccess;
    bool		isRootItem = false;

    // if element is zero, we are starting at the top
    if (element == 0)
    {
        // get the elements object
        element = CFDictionaryGetValue (properties, key);
        properties = 0;
        isRootItem = true;
    }
    
    // get the type of the object
    CFTypeID type = CFGetTypeID(element);
    
    // if this is an array, then it is not a leaf
    if (type == CFArrayGetTypeID())
    {
        // setup param block for array callback
        StaticWalkElementsParams params;
        params.iohiddevice 	= this;
        params.properties 	= properties;
        params.array 		= array;
        params.key          = key;
        params.elements 	= elements;
        params.value 		= kIOReturnSuccess;
        params.data 		= allocatedElementCount;
        
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
        CFMutableDictionaryRef  dictionary  = (CFMutableDictionaryRef) element;
        CFDictionaryRef         tempElement = 0;
        IOHIDElementStruct      hidelement;
        CFTypeRef               object;
        long                    number;
        
        // Check to see if this is a duplicate item.  if so, skip processing.
        object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementDuplicateIndexKey));
        if (object != 0) return kr;

        // get the actual dictionary ref
        if ( array )
        {            
            if (!isRootItem && properties)
                CFDictionarySetValue(dictionary, CFSTR(kIOHIDElementParentCollectionKey), properties);

            tempElement = CFDictionaryCreateCopy(kCFAllocatorDefault, dictionary);

            CFArrayAppendValue(array, tempElement);
            CFRelease(tempElement);

            hidelement.elementDictionaryRef = tempElement;
        }
        
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

        if ( CFStringCompare( key, CFSTR(kIOHIDElementKey), 0 ) == kCFCompareEqualTo)
        {
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
        }
        
        // if there are sub-elements, then this is not a leaf
        CFTypeRef subElements = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementKey));
        if (subElements)
        {
            // allocate and copy the data
            elements[(*allocatedElementCount)++] = hidelement;

            // recursively create leaf elements
            tempElement = CFDictionaryCreateCopy(kCFAllocatorDefault, dictionary);
            
            kr = this->CreateLeafElements (tempElement, array, subElements, allocatedElementCount, key, elements);
            
            CFRelease(tempElement);
        }
        // otherwise, this is a leaf, allocate and fill in our data
        else
        {
            // get the element size
            object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementSizeKey));
            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                return kIOReturnInternalError;
            if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
                return kIOReturnInternalError;
            hidelement.bytes = number >> 3;
            hidelement.bytes += (number % 8) ? 1 : 0;

            if ( ! CFStringCompare( key, CFSTR(kIOHIDElementKey), 0 ) )
            {
    
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
            }
                                    
            // otherwise, get the element value offset in shared memory for real device
            object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementValueLocationKey));
            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                return kIOReturnInternalError;
            if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
                return kIOReturnInternalError;
            hidelement.valueLocation = number;
            
            // allocate and copy the data
            elements[(*allocatedElementCount)++] = hidelement;

            // Check for duplicates
            do
            {
                CFTypeRef   duplicateReportBitsNumber   = 0;
                UInt32      duplicateSizeOffset         = 0;
                UInt32      duplicateCount              = 0;
                
                object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementDuplicateValueSizeKey));
                if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                    break;
                if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &duplicateSizeOffset))
                    break;

                object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementReportCountKey));
                if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                    break;
                if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &duplicateCount))
                    break;

                object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementReportSizeKey));
                if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                    break;
                if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
                    break;
                hidelement.bytes = number >> 3;
                hidelement.bytes += (number % 8) ? 1 : 0;
                duplicateReportBitsNumber = object;

                for ( unsigned i=0; i<duplicateCount; i++)
                {
                    hidelement.cookie ++;
                    hidelement.valueLocation += duplicateSizeOffset;

                    hidelement.elementDictionaryRef = 0;

                    if ( array )
                    {
                        CFMutableDictionaryRef tempMutableDict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, dictionary);

                        if ( tempMutableDict )
                        {

                            CFDictionaryRemoveValue(tempMutableDict, CFSTR(kIOHIDElementReportSizeKey));
                            CFDictionaryRemoveValue(tempMutableDict, CFSTR(kIOHIDElementReportCountKey));

                            CFDictionarySetValue(tempMutableDict, CFSTR(kIOHIDElementSizeKey), duplicateReportBitsNumber);

                            object = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &hidelement.cookie);
                            CFDictionarySetValue(tempMutableDict, CFSTR(kIOHIDElementCookieKey), object);
                            CFRelease( object );
                            
                            object = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &hidelement.valueLocation);
                            CFDictionarySetValue(tempMutableDict, CFSTR(kIOHIDElementValueLocationKey), object);
                            CFRelease( object );

                            object = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &i);
                            CFDictionarySetValue(tempMutableDict, CFSTR(kIOHIDElementDuplicateIndexKey), object);
                            CFRelease( object );
                        
                            tempElement = CFDictionaryCreateCopy(kCFAllocatorDefault, tempMutableDict);

                            CFArrayAppendValue(array, tempElement);
                            CFRelease(tempMutableDict);
                            CFRelease(tempElement);

                            hidelement.elementDictionaryRef = tempElement;
                        }
                        
                    }
                
                    elements[(*allocatedElementCount)++] = hidelement;

                }
                
            } while ( 0 );

        }
    }
    // this case should not happen, something else was found
    else
        printf ("\nIOHIDDeviceClass:Unexpected device registry structure - strange type\n"); // ееее make this debug only print
    
    return kr;
}

kern_return_t IOHIDDeviceClass::FindReportHandlers(CFDictionaryRef properties)
{

    kern_return_t           	kr = kIOReturnSuccess;
    long			allocatedElementCount;


    // count the number of leaves and allocate
    fReportHandlerElementCount = CountElements(properties, 0, CFSTR(kIOHIDInputReportElementsKey));
    fReportHandlerElements = new IOHIDElementStruct[fReportHandlerElementCount];
    
    // initialize allocation to zero
    allocatedElementCount = 0;
    
    // recursively add leaf elements
    kr = CreateLeafElements (properties, 0, 0, &allocatedElementCount, CFSTR(kIOHIDInputReportElementsKey), fReportHandlerElements);
    
//    printf ("%ld elements allocated of %ld expected\n", allocatedElementCount, fElementCount);
    
    // if we had errors, set the count to the number actually created
    fReportHandlerElementCount = allocatedElementCount;
        
    return kr;
}

IOHIDElementType IOHIDDeviceClass::getElementType(IOHIDElementCookie elementCookie)
{
    IOHIDElementStruct      element;
    IOHIDElementType        type = (IOHIDElementType) 0;
    
    if (getElement(elementCookie, &element))
            type = (IOHIDElementType) element.type;
    
    return type;
}

UInt32 IOHIDDeviceClass::getElementByteSize(IOHIDElementCookie elementCookie)
{
    UInt32 size = 0;
    IOHIDElementStruct element;
    
    if (getElement(elementCookie, &element))
            size = element.bytes;
    
    return size;
}

bool IOHIDDeviceClass::getElement(IOHIDElementCookie elementCookie, IOHIDElementStruct *element)
{
    
    for (long index = 0; index < fElementCount; index++)
        if (fElements[index].cookie == (unsigned long) elementCookie)
        {
            *element = fElements[index];
            return true;
        }
        
    for (long index = 0; index < fReportHandlerElementCount; index++)
        if (fReportHandlerElements[index].cookie == (unsigned long) elementCookie)
        {
            *element = fReportHandlerElements[index];
            return true;
        }
            
    return false;
}
