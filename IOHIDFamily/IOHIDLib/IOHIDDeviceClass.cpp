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
#include "IOHIDOutputTransactionClass.h"
#include "IOHIDLibUserClient.h"

#if IOHID_PSEUDODEVICE
// evil hackery to include this file, its just for the fake device
#define _NSBUILDING_APPKIT_DLL 0
#include <CoreFoundation/CFVeryPrivate.h>
#undef _NSBUILDING_APPKIT_DLL
#endif

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

#define allChecks() do {	    \
    connectCheck();		    \
    openCheck();		    \
    terminatedCheck();              \
} while (0)

typedef struct MyPrivateData {
    io_object_t			notification;
    IOHIDDeviceClass *		self;
} MyPrivateData;

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
  fNotifyPort(MACH_PORT_NULL),
  fRunLoop(NULL),
  fIsOpen(false),
  fIsLUNZero(false),
  fIsTerminated(false),
  fQueues(NULL),
  fRemovalCallback(NULL), 
  fRemovalTarget(NULL),
  fRemovalRefcon(NULL)
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
        
    if (fQueues)
        CFRelease(fQueues);
        
    if (fNotifyPort)
        IONotificationPortDestroy(fNotifyPort);
        
    if (fAsyncPort)
        mach_port_deallocate(mach_task_self(), fAsyncPort);
}

HRESULT	IOHIDDeviceClass::attachQueue (IOHIDQueueClass * iohidQueue)
{
    HRESULT res = S_OK;
    
    iohidQueue->setOwningDevice(this);

    // ееее todo add to list
    if ( fQueues || 
        ( fQueues = CFSetCreateMutable(kCFAllocatorDefault, 0, 0) ) )
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
    IOReturn 			res;
    kern_return_t 		kr;
    mach_port_t 		masterPort;
    CFRunLoopSourceRef		runLoopSource;
    CFMutableDictionaryRef 	entryProperties = 0;
    MyPrivateData		*privateDataRef = NULL;

    
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
    runLoopSource = IONotificationPortGetRunLoopSource(fNotifyPort);
    
    fRunLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(fRunLoop, runLoopSource, kCFRunLoopDefaultMode);

    privateDataRef = malloc(sizeof(MyPrivateData));
    bzero(privateDataRef, sizeof(MyPrivateData));
    
    privateDataRef->self = this;

    // Register for an interest notification of this device being removed. Use a reference to our
    // private data as the refCon which will be passed to the notification callback.
    kr = IOServiceAddInterestNotification( fNotifyPort,
                                            fService,
                                            kIOGeneralInterest,
                                            IOHIDDeviceClass::_deviceNotification,
                                            privateDataRef,
                                            &(privateDataRef->notification));

    // Now done with the master_port
    mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = 0;

    
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

void IOHIDDeviceClass::_deviceNotification( void *refCon,
                                            io_service_t service,
                                            natural_t messageType,
                                            void *messageArgument )
{
    kern_return_t	kr;
    IOHIDDeviceClass	*self;
    MyPrivateData	*privateDataRef = (MyPrivateData *) refCon;
    
    if (messageType == kIOMessageServiceIsTerminated)
    {    
        if (!privateDataRef)
            return;
           
        self = privateDataRef->self;
        
        if (!self)
            return;
            
        self->fIsTerminated = true;
        
        if (!self->fRemovalCallback)
            return;
        
        ((IOHIDCallbackFunction)self->fRemovalCallback)(self->fRemovalTarget,
                                                        kIOReturnSuccess,
                                                        self->fRemovalRefcon,
                                                        (void *)&(self->fHIDDevice));
        
        
        kr = IOObjectRelease(privateDataRef->notification);
        
        free(privateDataRef);
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
    
    // If we already have a port, don't create a new one.
    if (fAsyncPort) {
        if (port)
            *port = fAsyncPort;
        return kIOReturnSuccess;
    }

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
    fRemovalCallback	= removalCallback;
    fRemovalTarget	= removalTarget;
    fRemovalRefcon	= removalRefcon;

    return kIOReturnSuccess;
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
        (kr == kIOReturnSuccess))
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
    
    if (!getElement(elementCookie, &element))
        return kr;
        

#if ! IOHID_PSEUDODEVICE

    // we are only interested feature and output elements
    if ((element.type != kIOHIDElementTypeFeature) && 
            (element.type != kIOHIDElementTypeOutput))
        return kr;
                
    allChecks();

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
            
            if ((longValueSize > element.bytes) ||
                ( valueEvent->longValue == NULL))
                return kr;
                
            bzero(elementValue->value, 
                (elementValue->totalSize - sizeof(IOHIDElementValue)) + sizeof(UInt32));
            
            // *** FIX ME ***
            // Since we are setting mapped memory, we should probably
            // hold a shared lock
            convertByteToWord (valueEvent->longValue, elementValue->value, longValueSize<<3);
            //elementValue->timestamp = valueEvent->timestamp;
        }
        
        // Don't push the value out to the device if not told to.  
        // This is needed for transactions.
        if (!pushToDevice)
            return kIOReturnSuccess;
                        
        UInt32			input[1];
        IOByteCount			outputCount = 0;
                
        input[0] = (UInt32) elementCookie;
        
        //  kIOHIDLibUserClientPostElementValue,  kIOUCStructIStructO,    1,	0
        kr = io_connect_method_structureI_structureO(
                fConnection, kIOHIDLibUserClientPostElementValue, 
                (UInt8 *)input, sizeof(UInt32), NULL, &outputCount);
        
    }
        
#endif
            
    
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
    
    if (!getElement(elementCookie, &element))
        return kIOReturnBadArgument;
        
    // get the value
    SInt32		value = 0;
    void *		longValue = 0;
    UInt32		longValueSize = 0;
    UInt64		timestamp = 0;
    
#if IOHID_PSEUDODEVICE
    value = element.currentValue;
    timestamp = __CFReadTSR();
    
    // in pseudo-device increment value
    
    // if the pause count is non-zero, then decrment the pause count, but do not change the value
    if (element.pauseCount > 0)
        element.pauseCount--;
    // otherwise increment in direction
    else 
    {
        element.currentValue += element.increment;
        
        // switch direction at ends
        if (element.currentValue <= element.min)
            element.increment = 1;
        else if (element.currentValue >= element.max)
            element.increment = -1;

        // additional bounds check (in case we make increment greater than 1)
        if (element.currentValue < element.min)
            element.currentValue = element.min;
        else if (element.currentValue > element.max)
            element.currentValue = element.max;
        
        // if its a button, lets add a pause
        if (element.type == kIOHIDElementTypeInput_Button)
        {
            // if we are button up, pause longer (50-1050 polls)
            if (element.currentValue == element.min)
                element.pauseCount = 50 + (random() % 1000);
            // otherwise pause for a short time (0-5 polls)
            else
                element.pauseCount = (random() % 5);
        }
    }

#else
    allChecks();
    
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
            convertWordToByte(elementValue->value, longValue, longValueSize<<3);
            
            timestamp = *(UInt64 *)& elementValue->timestamp;
        }
    }
#endif
    
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
        
        hidRefcon = malloc(sizeof(IOHIDReportRefCon));
        
        if (!hidRefcon)
            return kIOReturnError;
            
        hidRefcon->callback		= callback;
        hidRefcon->callbackTarget 	= callbackTarget;
        hidRefcon->callbackRefcon 	= callbackRefcon;
        hidRefcon->sender		= &fHIDDevice;
    
        asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) _hidReportCallback;
        asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) hidRefcon;
    
        ret = io_async_method_scalarI_scalarO( fConnection, fAsyncPort, asyncRef, kIOAsyncCalloutCount, kIOHIDLibUserClientAsyncSetReport, in, 5, NULL, &len);
    
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
        
        hidRefcon = malloc(sizeof(IOHIDReportRefCon));
        
        if (!hidRefcon)
            return kIOReturnError;
            
        hidRefcon->callback		= callback;
        hidRefcon->callbackTarget 	= callbackTarget;
        hidRefcon->callbackRefcon 	= callbackRefcon;
        hidRefcon->sender		= &fHIDDevice;
    
        asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) _hidReportCallback;
        asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) hidRefcon;
    
        ret = io_async_method_scalarI_scalarO( fConnection, fAsyncPort, asyncRef, kIOAsyncCalloutCount, kIOHIDLibUserClientAsyncGetReport, in, 5, NULL, &len);
    
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

void 
IOHIDDeviceClass::_hidReportCallback(void *refcon, IOReturn result, UInt32 bufferSize)
{
    IOHIDReportRefCon *hidRefcon = refcon;
    
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
    
#ifndef max
#define max(a, b) \
    ((a > b) ? a:b)
#endif

#ifndef min
#define min(a, b) \
    ((a < b) ? a:b)
#endif

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

IOReturn IOHIDDeviceClass::startAllQueues()
{
    IOReturn ret = kIOReturnSuccess;
    
    if ( fQueues )
    {
        IOHIDQueueClass *queues[CFSetGetCount(fQueues)];
    
        CFSetGetValues(fQueues, (void **)queues);
        
        for (int i=0; queues && i<CFSetGetCount(fQueues) && ret==kIOReturnSuccess; i++)
        {
            ret = queues[i]->start();
        }
    }

    return ret;
}

IOReturn IOHIDDeviceClass::stopAllQueues()
{
    IOReturn ret = kIOReturnSuccess;
    
    if ( fQueues )
    {
        IOHIDQueueClass *queues[CFSetGetCount(fQueues)];
    
        CFSetGetValues(fQueues, (void **)queues);
        
        for (int i=0; queues && i<CFSetGetCount(fQueues) && ret==kIOReturnSuccess; i++)
        {
            ret = queues[i]->stop();
        }
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
    &IOHIDDeviceClass::deviceAllocOutputTransaction,
    // New post Jaguar 10.2
    &IOHIDDeviceClass::deviceSetReport,
    &IOHIDDeviceClass::deviceGetReport    
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
// End added methods

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
            
            // get the element size
            object = CFDictionaryGetValue (dictionary, CFSTR(kIOHIDElementSizeKey));
            if (object == 0 || CFGetTypeID(object) != CFNumberGetTypeID())
                return kIOReturnInternalError;
            if (!CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
                return kIOReturnInternalError;
            hidelement.bytes = number >> 3;
            hidelement.bytes += (number % 8) ? 1 : 0;

            
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
            
    return false;
}
