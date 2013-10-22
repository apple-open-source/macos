/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2012 Apple Computer, Inc.  All Rights Reserved.
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
#include "IOHIDTransactionClass.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDParserPriv.h"

__BEGIN_DECLS
#include <asl.h>
#include <mach/mach.h>
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IODataQueueClient.h>
#include <System/libkern/OSCrossEndian.h>
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
    if (!isValid()) \
        return kIOReturnExclusiveAccess; \
} while (0)

#define allChecks() do {	    \
    connectCheck();		    \
    openCheck();		    \
    seizeCheck();                   \
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

typedef struct _IOHIDObsoleteCallbackArgs {
    IOHIDObsoleteDeviceClass * self;
    void * callback;
    void * target;
    void * refcon;
    uint32_t * pLength;
}IOHIDObsoleteCallbackArgs;

enum {
    kCreateMatchingHIDElementsWithDictionaries = 0x1000
};

static void ElementCacheApplierFunction(const void *key __unused, const void *value, void *context)
{
    _IOHIDElementSetDeviceInterface((IOHIDElementRef)value, (IOHIDDeviceDeviceInterface**)context);
}

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
    fHIDDevice.pseudoVTable = (IUnknownVTbl *)  &sHIDDeviceInterfaceV2;
    fHIDDevice.obj = this;

    fService 			= MACH_PORT_NULL;
    fConnection 		= MACH_PORT_NULL;
    fAsyncPort 			= NULL;
    fNotifyPort 		= NULL;
    fDeviceValidPort    = MACH_PORT_NULL;
    fRunLoop 			= NULL;
    fAsyncCFMachPort    = NULL;
    fAsyncCFSource      = NULL;
	fNotifyCFSource		= NULL;
    fIsOpen 			= false;
    fIsLUNZero			= false;
    fIsTerminated		= false;
    fAsyncPortSetupDone = false;
	fAsyncPrivateDataRef	= NULL;
	fNotifyPrivateDataRef   = NULL;
    fRemovalCallback    = NULL; 
    fRemovalTarget		= NULL;
    fRemovalRefcon		= NULL;
    fQueues             = NULL;
    fElementCache       = NULL;
    fProperties         = NULL;
	fCurrentValuesMappedMemory  = 0;
	fCurrentValuesMappedMemorySize = 0;
    fElementCount 		= 0;
    fElementData        = NULL;
    fElements 			= NULL;
    fReportHandlerElementCount	= 0;
    fReportHandlerElementData	= NULL;
    fReportHandlerElements      = NULL;
    fReportHandlerQueue = NULL; 
    fInputReportCallback= NULL;
    fInputReportRefcon  = NULL;
    fInputReportBuffer  = NULL;
	fInputReportBufferSize = 0;
	fInputReportOptions = 0;
    fGeneration = -1;
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
    
    if ( fProperties ) {
        CFRelease(fProperties);
        fProperties = NULL;
    }
    
    if (fReportHandlerQueue){
        delete fReportHandlerQueue;
        fReportHandlerQueue = 0;
    }
    
    if (fElementCache) {
        CFDictionaryApplyFunction(fElementCache, ElementCacheApplierFunction, NULL);
        CFRelease(fElementCache);
    }
    
    if (fReportHandlerElementData)
        CFRelease(fReportHandlerElementData);
    
    if (fElementData)
        CFRelease(fElementData);
        
    if (fQueues)
        CFRelease(fQueues);
        
    if ( fDeviceValidPort )
        mach_port_mod_refs(mach_task_self(), fDeviceValidPort, MACH_PORT_RIGHT_RECEIVE, -1);
        
	if (fNotifyCFSource) {
        if ( fRunLoop )
            CFRunLoopRemoveSource(fRunLoop, fNotifyCFSource, kCFRunLoopDefaultMode);
        // Per IOKit documentation, this run loop source should not be retained/released
        fNotifyCFSource = NULL;
    }
    
    if (fNotifyPort)
        IONotificationPortDestroy(fNotifyPort);
    
    if (fRunLoop)
        CFRelease(fRunLoop);
    fRunLoop = NULL;
    
    // Even though we are leveraging IONotificationPort, we don't actually uses it's event source or CFMachPort
    // because we wanted to filter the message.  As such, we need to manually clean up our CFMachPort and source.
    if ( fAsyncCFMachPort ) {
        CFMachPortInvalidate(fAsyncCFMachPort);
        CFRelease(fAsyncCFMachPort);
    }
        
    if (fAsyncCFSource)
        CFRelease(fAsyncCFSource);
        
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

    // ¥¥¥¥ todo add to list
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
    
    // ¥¥¥¥ todo remove from list
    if ( fQueues )
    {
        CFSetRemoveValue(fQueues, (void *)iohidQueue);
    }
    
    return res;
}

HRESULT IOHIDDeviceClass::attachTransaction (IOHIDTransactionClass * transaction)
{
    HRESULT res = S_OK;
    
    transaction->setOwningDevice(this);

    // ¥¥¥¥ todo add to list
    
    return res;

}

HRESULT IOHIDDeviceClass::detachTransaction (IOHIDTransactionClass * transaction)
{
    HRESULT res = S_OK;

    transaction->setOwningDevice(NULL);
    
    // ¥¥¥¥ todo remove from list
    
    return res;
}

IOHIDQueueClass * IOHIDDeviceClass::createQueue(bool reportHandler)
{
    IOHIDQueueClass * newQueue = new IOHIDQueueClass;
    
    // attach the queue to us
    attachQueue (newQueue, reportHandler);

	return newQueue;
}

HRESULT IOHIDDeviceClass::queryInterfaceQueue (CFUUIDRef uuid __unused, void **ppv)
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

HRESULT IOHIDDeviceClass::queryInterfaceTransaction (CFUUIDRef uuid __unused, void **ppv)
{
    HRESULT res = S_OK;
    
    IOHIDTransactionClass * transaction = new IOHIDTransactionClass;
    
    attachTransaction(transaction);
    
    transaction->create();
    
    *ppv = transaction->getInterfaceMap();
        
    return res;
}


HRESULT IOHIDDeviceClass::queryInterface(REFIID iid, void **ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, kIOHIDDeviceQueueInterfaceID))
        res = queryInterfaceQueue(uuid, ppv);
    else if (CFEqual(uuid, kIOHIDDeviceTransactionInterfaceID))
        res = queryInterfaceTransaction(uuid, ppv);
    else if (CFEqual(uuid, IUnknownUUID) || CFEqual(uuid, kIOCFPlugInInterfaceID))
    {
        *ppv = &iunknown;
        addRef();
    }
    else if (CFEqual(uuid, kIOHIDDeviceDeviceInterfaceID))
    {
        *ppv = &fHIDDevice;
        addRef();
    }
    else {
        *ppv = 0;
        HIDLog ("not found\n");
    }

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
    return res;
}

IOReturn IOHIDDeviceClass::
probe(CFDictionaryRef propertyTable __unused, io_service_t inService, SInt32 *order __unused)
{
    if (!inService || !IOObjectConformsTo(inService, "IOHIDDevice"))
        return kIOReturnBadArgument;

    return kIOReturnSuccess;
}

IOReturn IOHIDDeviceClass::start(CFDictionaryRef propertyTable __unused, io_service_t inService)
{
    IOReturn 			res;
    kern_return_t 		kr;
    CFMutableDictionaryRef	properties;

    fService = inService;
    IOObjectRetain(fService);
    
    res = IOServiceOpen(fService, mach_task_self(), kIOHIDLibUserClientConnectManager, &fConnection);
    if (res != kIOReturnSuccess)
        return res;

    connectCheck();

    fNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);

    // Per IOKit documentation, this run loop source should not be retained/released
    fNotifyCFSource = IONotificationPortGetRunLoopSource(fNotifyPort);
    
    fRunLoop = CFRunLoopGetMain();
    CFRetain(fRunLoop);
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


    // Create port to determine if mem maps are valid.  Use 
    // IODataQueueAllocateNotificationPort cause that limits the msg queue to 
    // one entry.
    fDeviceValidPort = IODataQueueAllocateNotificationPort();
    if (fDeviceValidPort == MACH_PORT_NULL)
        return kIOReturnNoMemory;
        
    kr = IOConnectSetNotificationPort(fConnection, kIOHIDLibUserClientDeviceValidPortType, fDeviceValidPort, NULL);
    if (kr != kIOReturnSuccess)
        return kr;
    
    uint64_t output[2];
    uint32_t len = 2;

    kr = IOConnectCallScalarMethod(fConnection, kIOHIDLibUserClientGetElementCount, 0, 0, output, &len); 
    if (kr != kIOReturnSuccess)
        return kr;

    HIDLog("IOHIDDeviceClass::start: elementCount=%lld reportHandlerCount=%lld\n", output[0], output[1]);

    fElementCount               = output[0];
    fReportHandlerElementCount  = output[1];
    
    
    buildElements(kHIDElementType, &fElementData, &fElements, &fElementCount);
    buildElements(kHIDReportHandlerType, &fReportHandlerElementData, &fReportHandlerElements, &fReportHandlerElementCount);

	fElementCache = CFDictionaryCreateMutable(
                                            kCFAllocatorDefault, 
                                            0, 
                                            &kCFTypeDictionaryKeyCallBacks, 
                                            &kCFTypeDictionaryValueCallBacks);
                                            
    if ( !fElementCache )
        return kIOReturnNoMemory;
        
    fProperties = CFDictionaryCreateMutable(
                                            kCFAllocatorDefault, 
                                            0, 
                                            &kCFTypeDictionaryKeyCallBacks, 
                                            &kCFTypeDictionaryValueCallBacks);

    if ( !fProperties )
        return kIOReturnNoMemory;

    return kIOReturnSuccess;
}

IOReturn IOHIDDeviceClass::createSharedMemory(uint64_t generation)
{
    // get the shared memory
    if ( generation == fGeneration ) 
        return kIOReturnSuccess;
        
#if !__LP64__
    vm_address_t        address = nil;
    vm_size_t           size    = 0;
#else
    mach_vm_address_t   address = nil;
    mach_vm_size_t      size    = 0;
#endif
    IOReturn ret = IOConnectMapMemory (	
                                fConnection, 
                                kIOHIDLibUserClientElementValuesType, 
                                mach_task_self(), 
                                &address, 
                                &size, 
                                kIOMapAnywhere	);

    if (ret != kIOReturnSuccess)
        return kIOReturnError;
        
    fCurrentValuesMappedMemory = address;
    fCurrentValuesMappedMemorySize = size;
    
    if ( !fCurrentValuesMappedMemory )
        return kIOReturnNoMemory;

    fGeneration = generation;
    
    return kIOReturnSuccess;
}

IOReturn IOHIDDeviceClass::releaseSharedMemory()
{    
    // finished with the shared memory
    if (!fCurrentValuesMappedMemory) 
        return kIOReturnSuccess;
        
    IOReturn ret = IOConnectUnmapMemory (	
                                fConnection, 
                                kIOHIDLibUserClientElementValuesType, 
                                mach_task_self(), 
                                fCurrentValuesMappedMemory);
                                
    fCurrentValuesMappedMemory      = 0;
    fCurrentValuesMappedMemorySize  = 0;
    
    return ret;
}

Boolean IOHIDDeviceClass::isValid()
{
    IOReturn kr;
    
    struct {
            mach_msg_header_t	msgHdr;
            OSNotificationHeader	notifyHeader;
            mach_msg_trailer_t	trailer;
    } msg;
    
    kr = mach_msg(&msg.msgHdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg), fDeviceValidPort, 0, MACH_PORT_NULL);
    
    switch ( kr ) {
        case MACH_MSG_SUCCESS:
            uint64_t args[2];
            uint32_t len = 2;
            
            args[0] = 1;
            args[1] = fGeneration;

            kr = IOConnectCallScalarMethod(fConnection, kIOHIDLibUserClientDeviceIsValid, 0, 0, args, &len); 
            
            if ( args[0] /*valid*/ )
                kr = createSharedMemory(args[1] /*generation */);
            else {
                fCurrentValuesMappedMemory      = 0;
                fCurrentValuesMappedMemorySize  = 0;
            }
            break;
    };

    return fCurrentValuesMappedMemory != 0;
}


IOReturn IOHIDDeviceClass::getProperty(CFStringRef key, CFTypeRef * pProperty)
{
    CFTypeRef property = CFDictionaryGetValue(fProperties, key);
    
    if ( !property ) {
        property = IORegistryEntrySearchCFProperty(fService, kIOServicePlane, key, kCFAllocatorDefault, kIORegistryIterateRecursively| kIORegistryIterateParents);
        if ( property ) {
            CFDictionarySetValue(fProperties, key, property);
            CFRelease(property);
        }
    }
    
    if ( pProperty )
        *pProperty = property;
        
    return kIOReturnSuccess;
}

IOReturn IOHIDDeviceClass::setProperty(CFStringRef key, CFTypeRef property)
{
    CFDictionarySetValue(fProperties, key, property);
    
    return kIOReturnSuccess;
}

// RY: There are 2 General Interest notification event sources.
// One is operating on the main run loop via fNotifyPort and is internal 
// to the IOHIDDeviceClass.  The other is used by the client to get removal 
// notification via fAsyncPort.  This method is used by both and disguished 
// by the port in the refcon.
void IOHIDDeviceClass::_deviceNotification(void *refCon, io_service_t service __unused, natural_t messageType, void * messageArgument )
{
    IOHIDDeviceClass *  self;
    MyPrivateData *     privateDataRef  = (MyPrivateData *) refCon;
    IOOptionBits        options         = (IOOptionBits)((addr64_t)messageArgument);
    
    if (!privateDataRef)
        return;
    
    self = privateDataRef->self;
    
    if (!self || (messageType != kIOMessageServiceIsTerminated))
        return;
				
    self->fIsTerminated = true;
    
    if ( privateDataRef != self->fAsyncPrivateDataRef)
        return;
    
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
}

IOReturn IOHIDDeviceClass::getAsyncEventSource(CFTypeRef *source)
{
    connectCheck();

    if (!fAsyncPort) {     
        IOReturn ret;
        ret = getAsyncPort(0);
        if (kIOReturnSuccess != ret)
            return ret;
    }

    if (!fAsyncCFMachPort) {
        CFMachPortContext   context;
        Boolean             shouldFreeInfo = FALSE;

        context.version         = 1;
        context.info            = this;
        context.retain          = NULL;
        context.release         = NULL;
        context.copyDescription = NULL;

        fAsyncCFMachPort = CFMachPortCreateWithPort(NULL, IONotificationPortGetMachPort(fAsyncPort),
                    (CFMachPortCallBack) _cfmachPortCallback,
                    &context, &shouldFreeInfo);
                    
        if ( shouldFreeInfo ) {
            // The CFMachPort we got might not work, but we'll proceed with it anyway.
            asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s received an unexpected reused CFMachPort", __func__);                    
        }
            
        if (!fAsyncCFMachPort)
            return kIOReturnNoMemory;
    }
    
    if ( !fAsyncCFSource ) {
        fAsyncCFSource = CFMachPortCreateRunLoopSource(NULL, fAsyncCFMachPort, 0);
        if (!fAsyncCFSource)
            return kIOReturnNoMemory;
    }

    if (source)
        *source = fAsyncCFSource;

    return kIOReturnSuccess;
}

IOReturn IOHIDDeviceClass::getAsyncPort(mach_port_t *port)
{
    IOReturn ret = kIOReturnSuccess;

    connectCheck();
    
    if ( !fAsyncPort )  {

        fAsyncPort = IONotificationPortCreate(kIOMasterPortDefault);
        if (!fAsyncPort)
            return kIOReturnNoMemory;
        
        if (fIsOpen)
            ret = finishAsyncPortSetup();
    }

    if (port)
        *port = IONotificationPortGetMachPort(fAsyncPort);
    
    return ret;
}

IOReturn IOHIDDeviceClass::finishAsyncPortSetup()
{
	finishReportHandlerQueueSetup();

    fAsyncPortSetupDone = true;

    return IOConnectSetNotificationPort(fConnection, kIOHIDLibUserClientAsyncPortType, IONotificationPortGetMachPort(fAsyncPort), NULL);
}

IOReturn IOHIDDeviceClass::open(IOOptionBits options)
{
    IOReturn ret = kIOReturnSuccess;

    connectCheck();

    do {
        // ¥¥Êtodo, check flags to see if different (if so, we might need to reopen)
        if (fIsOpen)
            break;
          
        uint32_t len = 0;
        uint64_t input = options;
        
        ret = IOConnectCallScalarMethod(fConnection, kIOHIDLibUserClientOpen, &input, 1, 0, &len); 

        if (ret != kIOReturnSuccess)
            break;

        fIsOpen = true;

        if (!fAsyncPortSetupDone && fAsyncPort) {
            ret = finishAsyncPortSetup();

            if (ret != kIOReturnSuccess) {
                close();
                break;
            }
        }
        
        isValid();
        
    } while (false);
    
    return ret;
}

IOReturn IOHIDDeviceClass::close(IOOptionBits options __unused)
{
    IOReturn ret;
    
    openCheck();
    connectCheck();

    ret = releaseSharedMemory();

    uint32_t len = 0;

    ret = IOConnectCallScalarMethod(fConnection, kIOHIDLibUserClientClose, 0, 0, 0, &len); 

    fIsOpen = false;
    fIsLUNZero = false;

    return ret;
}

IOReturn IOHIDDeviceClass::getElementValue(IOHIDElementRef element, IOHIDValueRef * pEvent, uint32_t timeout __unused, IOHIDValueCallback callback __unused, void * refcon __unused, IOOptionBits options)
{
    uint32_t generation = 0;
    IOReturn kr = getCurrentElementValueAndGeneration(element, pEvent, &generation);
    
    // If the generation is 0, this element has never
    // been processed.  We should query the element
    //  to get the current value.
    if ((kr == kIOReturnSuccess) && ((options & kHIDGetElementValuePreventPoll) == 0) && ((options & kHIDGetElementValueForcePoll) || ((IOHIDElementGetType(element) == kIOHIDElementTypeFeature) && (generation == 0))))
    {        
        uint64_t    input = (uint64_t) IOHIDElementGetCookie(element);
        size_t    outputCount = 0;
            
        allChecks();

		kr = IOConnectCallScalarMethod(fConnection, kIOHIDLibUserClientUpdateElementValues, &input, 1, 0, 0); 
				
		if (kr == kIOReturnSuccess)
			kr = getCurrentElementValueAndGeneration(element, pEvent);        
	}    
 
    return kr;
}

IOReturn IOHIDDeviceClass::setElementValue(IOHIDElementRef element, IOHIDValueRef event, uint32_t timeout __unused, IOHIDValueCallback callback __unused, void * refcon __unused, IOOptionBits options)
{
    kern_return_t           kr = kIOReturnBadArgument;
    IOHIDElementStruct		elementStruct;
    
    allChecks();

    if (!getElementStruct(IOHIDElementGetCookie(element), &elementStruct))
        return kr;

    // we are only interested feature and output elements
    if ((elementStruct.type != kIOHIDElementTypeFeature) && (elementStruct.type != kIOHIDElementTypeOutput))
        return kr;
                
    // get ptr to shared memory for this element
    if (elementStruct.valueLocation < fCurrentValuesMappedMemorySize)
    {
        IOHIDElementValue * elementValue = (IOHIDElementValue *)(fCurrentValuesMappedMemory + elementStruct.valueLocation);
        
        _IOHIDValueCopyToElementValuePtr(event, elementValue);
        
        // See if the value is pended
        if ( options & kHIDSetElementValuePendEvent )
            return kIOReturnSuccess;
                        
        uint64_t    input = (uint64_t)IOHIDElementGetCookie(element);
        uint32_t    outputCount = 0;
        
        kr = IOConnectCallScalarMethod(fConnection, kIOHIDLibUserClientPostElementValues, &input, 1, 0, &outputCount); 
    }

    return kr;
}

IOReturn IOHIDDeviceClass::getCurrentElementValueAndGeneration(IOHIDElementRef element, IOHIDValueRef *pEvent, uint32_t * pGeneration)
{
    IOHIDElementStruct  elementStruct;
    IOHIDEventStruct    valueEvent;
    IOHIDValueRef       valueRef;
    
    allChecks();

    if (!element || !getElementStruct(IOHIDElementGetCookie(element), &elementStruct) || (elementStruct.type == kIOHIDElementTypeCollection))
        return kIOReturnBadArgument;
        
    // get the value
    // get ptr to shared memory for this elementStruct
    if (elementStruct.valueLocation < fCurrentValuesMappedMemorySize)
    {
        IOHIDElementValue * elementValue = (IOHIDElementValue *)(fCurrentValuesMappedMemory + elementStruct.valueLocation);
        uint64_t            timeStamp    = *((uint64_t *)&(elementValue->timestamp));
        uint32_t            generation   = elementValue->generation;

        ROSETTA_ONLY(
            timeStamp   = OSSwapInt64(timeStamp);
            generation  = OSSwapInt32(generation);
        );

        valueRef = _IOHIDElementGetValue(element);
        
        if ( !valueRef || (IOHIDValueGetTimeStamp(valueRef) < timeStamp) )
        {
            valueRef = _IOHIDValueCreateWithElementValuePtr(kCFAllocatorDefault, element, elementValue);

            if (valueRef) {
                _IOHIDElementSetValue(element, valueRef);
                CFRelease(valueRef); // Should be retained by the element
            }
        }
        
        if (pEvent)
            *pEvent = valueRef;
            
        if ( pGeneration )
            *pGeneration = generation;
    }
    
    return kIOReturnSuccess;
    
}

struct IOHIDReportRefCon {
    IOHIDReportType type;
    uint8_t *       buffer;
    uint32_t        reportID;
    IOHIDReportCallback	callback;
    void *			callbackRefcon;
    void *			sender;
};

IOReturn IOHIDDeviceClass::setReport(IOHIDReportType        reportType, 
                                     uint32_t               reportID, 
                                     const uint8_t          *report, 
                                     CFIndex                reportLength, 
                                     uint32_t               timeout, 
                                     IOHIDReportCallback    callback, 
                                     void                   *refcon, 
                                     IOOptionBits           options __unused)
{
    uint64_t    in[3];
    IOReturn    ret;

    allChecks();

    // Async setReport
    if (callback) 
    {
        if (!fAsyncPort)
            return kIOReturnError; //kIOUSBNoAsyncPortErr;
            
        io_async_ref64_t    asyncRef;
        IOHIDReportRefCon * hidRefcon = 0;

    
        in[0] = reportType;
        in[1] = reportID;
        in[2] = timeout; 
        
        hidRefcon = (IOHIDReportRefCon *)malloc(sizeof(IOHIDReportRefCon));
        
        if (!hidRefcon)
            return kIOReturnError;
            
        hidRefcon->type         = reportType;
        hidRefcon->reportID     = reportID;
        hidRefcon->buffer       = (uint8_t *)report;
        hidRefcon->callback		= callback;
        hidRefcon->callbackRefcon 	= refcon;
        hidRefcon->sender		= &fHIDDevice;
    
        asyncRef[kIOAsyncCalloutFuncIndex]      = (uint64_t)_hidReportCallback;
        asyncRef[kIOAsyncCalloutRefconIndex]    = (uint64_t)hidRefcon;
    
        ret = IOConnectCallAsyncMethod(fConnection, kIOHIDLibUserClientSetReport, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, in, 3, report, reportLength, 0, 0, 0, 0);
    
    }
    else
    {
        in[0] = reportType;
        in[1] = reportID;        
        in[2] = 0; 
        ret = IOConnectCallMethod(fConnection, kIOHIDLibUserClientSetReport, in, 3, report, (size_t)reportLength, 0, 0, 0, 0);
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
IOHIDDeviceClass::getReport(IOHIDReportType     reportType, 
                            uint32_t            reportID, 
                            uint8_t             *report, 
                            CFIndex             *pReportLength, 
                            uint32_t            timeout, 
                            IOHIDReportCallback callback, 
                            void                *refcon, 
                            IOOptionBits        options __unused)
{
    uint64_t    in[3];
    IOReturn    ret;
    size_t      reportLength = *pReportLength;

    allChecks();
    
    if (!pReportLength || (*pReportLength < 0))
    	return kIOReturnNoMemory;
    	
    // Async getReport
    if (callback) 
    {
        if (!fAsyncPort)
            return kIOReturnError; //kIOUSBNoAsyncPortErr;
            
        io_async_ref64_t    asyncRef;
        IOHIDReportRefCon * hidRefcon = 0;

    
        in[0] = reportType;
        in[1] = reportID;
        in[2] = timeout; 
        
        hidRefcon = (IOHIDReportRefCon *)malloc(sizeof(IOHIDReportRefCon));
        
        if (!hidRefcon)
            return kIOReturnError;
            
        hidRefcon->type         = reportType;
        hidRefcon->reportID     = reportID;
        hidRefcon->buffer       = report;
        hidRefcon->callback		= callback;
        hidRefcon->callbackRefcon 	= refcon;
        hidRefcon->sender		= &fHIDDevice;
    
        asyncRef[kIOAsyncCalloutFuncIndex]      = (uint64_t)_hidReportCallback;
        asyncRef[kIOAsyncCalloutRefconIndex]    = (uint64_t)hidRefcon;
        
    
        ret = IOConnectCallAsyncMethod(fConnection, kIOHIDLibUserClientGetReport, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, in, 3, 0, 0, 0, 0, report, &reportLength);    
    }
    else
    {
        in[0] = reportType;
        in[1] = reportID;
        in[2] = 0; 
        ret = IOConnectCallMethod(fConnection, kIOHIDLibUserClientGetReport, in, 3, 0, 0, 0, 0, report, &reportLength);
    }

    *pReportLength = reportLength;
    
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fConnection = MACH_PORT_NULL;
		ret = kIOReturnNoDevice;
    }
    return ret;
}


bool IOHIDDeviceClass::getElementDictIntValue(CFDictionaryRef element, CFStringRef key, uint32_t * value)
{
    CFTypeRef   object = CFDictionaryGetValue (element, key);
    uint32_t    number = 0;
    CFTypeID    typeID = object ? CFGetTypeID(object) : 0; // _kCFRuntimeNotATypeID
    
    if (!value) {
        char buff[64] = "unknown";
        if (key)
            CFStringGetCString(key, buff, 64, kCFStringEncodingUTF8);
        asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s called with no value for %s\n", __PRETTY_FUNCTION__, buff);
    }
    else {
        if (typeID == CFNumberGetTypeID())
        {
            if (CFNumberGetValue((CFNumberRef) object, kCFNumberLongType, &number))
            {
                *value = number;
                return true;
            }
        }
        else if (typeID == CFBooleanGetTypeID())
        {
            *value = (object == kCFBooleanTrue);
            return true;
        }
    }
    return false;
}

void IOHIDDeviceClass::setElementDictIntValue(CFMutableDictionaryRef element, CFStringRef key, uint32_t value)
{
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    
    if ( !number )
        return;

    CFDictionarySetValue(element, key, number);
    CFRelease(number);
}

void IOHIDDeviceClass::setElementDictBoolValue( CFMutableDictionaryRef element, CFStringRef key, bool value)
{
    CFBooleanRef boolVal = (value) ? kCFBooleanTrue : kCFBooleanFalse;
    CFDictionarySetValue(element, key, boolVal);
}


CFTypeRef IOHIDDeviceClass::createElement(CFDataRef data, IOHIDElementStruct * element, uint32_t index, CFTypeRef parentElement, CFMutableDictionaryRef elementCache, bool * isElementCached, IOOptionBits options)
{
    CFTypeRef   type = 0;
    CFNumberRef key = 0;
    uint32_t    cookie = element->cookieMin + index;
    
    if ( elementCache )
    {
        key = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &cookie);
        
        if ( key )
        {
            type = CFDictionaryGetValue(elementCache, key);
            
            if ( type )
            {
                if (isElementCached)
                    *isElementCached = true;
                    
                CFRetain(type);
                CFRelease(key);
                    
                return type;
            }
        }
    }
    
    if ( options & kCreateMatchingHIDElementsWithDictionaries )
    {
        CFMutableDictionaryRef dictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
        if (dictionary)
        {
            uint32_t reportCount = element->reportCount;
            uint32_t size = element->size;
            
            do {
                if ( parentElement )
                    CFDictionarySetValue(dictionary, CFSTR(kIOHIDElementParentCollectionKey), parentElement);
                    
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementCookieKey), cookie);
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementCollectionCookieKey), element->parentCookie);
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementTypeKey), element->type);
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementUsageKey), element->usageMin + ((element->usageMin!=element->usageMax) ? index : 0));
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementUsagePageKey), element->usagePage);
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementReportIDKey), element->reportID);

                if ( element->type == kIOHIDElementTypeCollection )
                {
                    setElementDictIntValue(dictionary, CFSTR(kIOHIDElementCollectionTypeKey), element->collectionType);
                    break;
                }

                if ( element->duplicateValueSize && (element->duplicateIndex != 0xffffffff))
                {
                    reportCount = 1;
                    size        = element->reportSize;                
                    setElementDictIntValue(dictionary, CFSTR(kIOHIDElementDuplicateIndexKey), element->duplicateIndex + ((index>0) ? index-1: 0));
                }

                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementSizeKey), size);
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementReportSizeKey), element->reportSize);
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementReportCountKey), reportCount);

                setElementDictBoolValue(dictionary, CFSTR(kIOHIDElementHasNullStateKey), (element->flags & kHIDDataNullStateBit) == kHIDDataNullState);
                setElementDictBoolValue(dictionary, CFSTR(kIOHIDElementHasPreferredStateKey), (element->flags & kHIDDataNoPreferredBit) != kHIDDataNoPreferred);
                setElementDictBoolValue(dictionary, CFSTR(kIOHIDElementIsNonLinearKey), (element->flags & kHIDDataNonlinearBit) == kHIDDataNonlinear);
                setElementDictBoolValue(dictionary, CFSTR(kIOHIDElementIsRelativeKey), (element->flags & kHIDDataRelativeBit) == kHIDDataRelative);
                setElementDictBoolValue(dictionary, CFSTR(kIOHIDElementIsWrappingKey), (element->flags & kHIDDataWrapBit) == kHIDDataWrap);
                setElementDictBoolValue(dictionary, CFSTR(kIOHIDElementIsArrayKey), (element->flags & kHIDDataArrayBit) == kHIDDataArray);

                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementMaxKey), element->max);
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementMinKey), element->min);
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementScaledMaxKey), element->scaledMax);
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementScaledMinKey), element->scaledMin);
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementUnitKey), element->unit);
                setElementDictIntValue(dictionary, CFSTR(kIOHIDElementUnitExponentKey), element->unitExponent);
                
            } while (false);
            
            type = dictionary;
        }
    }
    else 
    {
        type = _IOHIDElementCreateWithParentAndData(kCFAllocatorDefault, (IOHIDElementRef)parentElement, data, element, index);
        
        if (type)
            _IOHIDElementSetDeviceInterface((IOHIDElementRef)type, (IOHIDDeviceDeviceInterface **)&fHIDDevice);
    }

    if ( key && type && elementCache )
        CFDictionarySetValue(elementCache, key, type);
    
    if (key)
        CFRelease(key);
    
    if (isElementCached)
        *isElementCached = false;
    
	return type;
}

IOReturn 
IOHIDDeviceClass::copyMatchingElements(CFDictionaryRef matchingDict, CFArrayRef * elements, CFTypeRef parentElement, CFMutableDictionaryRef elementCache, IOOptionBits options)
{    
   if (!elements)
        return kIOReturnBadArgument;
     
    IOHIDElementStruct      element;
    CFMutableArrayRef       tempElements        = 0;
    CFMutableArrayRef       subElements         = 0;
    CFTypeRef               elementType         = 0;
    CFTypeRef               object              = 0;
    uint32_t                number              = 0;
    uint32_t                index               = 0;
    uint32_t                matchingCookieMin   = 0;
    uint32_t                matchingCookieMax   = 0;
    uint32_t                matchingUsageMin    = 0;
    uint32_t                matchingUsageMax    = 0;
    uint32_t                matchingDupIndex    = 0;
    bool                    isMatchingCookieMin = false;
    bool                    isMatchingCookieMax = false;
    bool                    isMatchingUsageMin  = false;
    bool                    isMatchingUsageMax  = false;
    bool                    isMatchingDupIndex  = false;
    bool                    isElementCached     = false;
    bool                    isDuplicateRoot     = false;
    

    if (!(tempElements = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks)))
    {
        *elements = 0;
        return kIOReturnNoMemory;
    }
        
    for (index=0; index<fElementCount; index++)
    {        
        isMatchingCookieMin = isMatchingCookieMax = false;
        isMatchingUsageMax = isMatchingUsageMin = false;
        matchingCookieMin = matchingCookieMax = 0;
        matchingUsageMin = matchingUsageMax = 0;
        isDuplicateRoot = (fElements[index].duplicateValueSize != 0);
        
        if ( matchingDict )
        {
            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementCookieKey), &number) ) 
            {
                if ((number < fElements[index].cookieMin) || (number > fElements[index].cookieMax))
                    continue;

                matchingCookieMin = number;
                matchingCookieMax = number;
                isMatchingCookieMin = true;
                isMatchingCookieMax = true;
            }
            else
            {
                if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementCookieMinKey), &number) ) 
                {
                    if ( number < fElements[index].cookieMin )
                        continue;
                    
                    matchingCookieMin = number;
                    isMatchingCookieMin = true;
                }
                
                if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementCookieMaxKey), &number) ) 
                {
                    if ( number > fElements[index].cookieMax )
                        continue;
                    
                    matchingCookieMax = number;
                    isMatchingCookieMax = true;
                }
            }

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementCollectionCookieKey), &number) &&
                (number != fElements[index].parentCookie))
                continue;
                
            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementTypeKey), &number) &&
                (number != fElements[index].type))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementCollectionTypeKey), &number) &&
                (number != fElements[index].collectionType))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementReportIDKey), &number) &&
                (number != fElements[index].reportID))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementUsageKey), &number) ) 
            {
                if ((number < fElements[index].usageMin) || (number > fElements[index].usageMax))
                    continue;
                
                matchingUsageMin    = number;
                matchingUsageMax    = number;
                isMatchingUsageMin  = true;
                isMatchingUsageMax  = true;
            }
            else
            {
                if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementUsageMinKey), &number) ) 
                {
                    if ( number < fElements[index].usageMin )
                        continue;
                    
                    matchingUsageMin = number;
                    isMatchingUsageMin = true;
                }
                
                if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementUsageMaxKey), &number) ) 
                {
                    if ( number > fElements[index].usageMax )
                        continue;
                    
                    matchingUsageMax = number;
                    isMatchingUsageMax = true;
                }
            }

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementUsagePageKey), &number) &&
                (number != fElements[index].usagePage))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementMinKey), &number) &&
                ((int32_t)number != fElements[index].min))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementMaxKey), &number) &&
                ((int32_t)number != fElements[index].max))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementScaledMinKey), &number) &&
                ((int32_t)number != fElements[index].scaledMin))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementScaledMaxKey), &number) &&
                ((int32_t)number != fElements[index].scaledMax))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementSizeKey), &number) &&
                (number != fElements[index].size))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementReportSizeKey), &number) &&
                (number != fElements[index].reportSize))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementReportCountKey), &number) &&
                (number != fElements[index].reportCount))
                continue;
                
            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementIsRelativeKey), &number) &&
                (number != ((fElements[index].flags & kHIDDataRelativeBit) == kHIDDataRelative)))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementIsWrappingKey), &number) &&
                (number != ((fElements[index].flags & kHIDDataWrapBit) == kHIDDataWrap)))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementIsNonLinearKey), &number) &&
                (number != ((fElements[index].flags & kHIDDataNonlinearBit) == kHIDDataNonlinear)))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementHasPreferredStateKey), &number) &&
                (number != ((fElements[index].flags & kHIDDataNoPreferredBit) != kHIDDataNoPreferred)))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementHasNullStateKey), &number) &&
                (number != ((fElements[index].flags & kHIDDataNullStateBit) == kHIDDataNullState)))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementIsArrayKey), &number) &&
                (number != ((fElements[index].flags & kHIDDataArrayBit) == kHIDDataArray)))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementUnitKey), &number) &&
                (number != fElements[index].unit))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementUnitExponentKey), &number) &&
                (number != fElements[index].unitExponent))
                continue;

            if ( getElementDictIntValue(matchingDict, CFSTR(kIOHIDElementDuplicateIndexKey), &number) )
            {
                if ( !isDuplicateRoot )
                    continue;
                    
                matchingDupIndex   = number;
                isMatchingDupIndex = true;
            }
        }
            
        uint32_t rangeIndex = 0;
        uint32_t usageIndex = 0;
        uint32_t duplicateIndex = 0;
        
        while ((fElements[index].cookieMin+rangeIndex)<= fElements[index].cookieMax) 
        {                      
            // Break out once we are out of the usage range
            if (isMatchingUsageMax && (matchingUsageMax < (fElements[index].usageMin+usageIndex)))
                break;

            // Break out once we are out of the cookie range
            if (isMatchingCookieMax && (matchingCookieMax < (fElements[index].cookieMin+rangeIndex)))
                break;
                
            // create guy
            if ((!isMatchingCookieMin || (isMatchingCookieMin && (matchingCookieMin <= (fElements[index].cookieMin+rangeIndex)))) && 
                (!isMatchingUsageMin || (isMatchingUsageMin && (matchingUsageMin <= (fElements[index].usageMin+usageIndex)))) &&
                // The container element is always going to be the first 
                // element in a duplicate range and should be ignored when 
                // matching on a specific index.
                (!isMatchingDupIndex || 
                    (((fElements[index].cookieMin+rangeIndex) != fElements[index].cookieMin) && 
                    isMatchingDupIndex && (matchingDupIndex == (fElements[index].duplicateIndex+duplicateIndex)))))
            {                
                isElementCached = false;
                elementType = createElement(fElementData, &fElements[index], (fElements[index].duplicateValueSize&&!isDuplicateRoot) ? duplicateIndex : rangeIndex, parentElement, elementCache, &isElementCached, options);
                
                if ( elementType )
                {
                    CFArrayAppendValue(tempElements, elementType);
                    CFRelease(elementType);
   
					// RY: Adding a non-cached collection requires digging for the sub elements
                    // Ideally, I would like to be able to generate these objects only when
                    // requested via CFDictionaryGetValue on kIOHIDElementKey.  Unfortunately,
                    // CF does not provide a callback method for CFDictionaryGetValue.  There
                    // are ways to work with the CFEqual callback, but you need to handle 
                    // recursively calling back into yourself on a SetValue.  Spoke to CTP on
                    // CF team and requested the ability to have a faulting CFDictionary.  This
                    // appears to be desired by other teams as well and is planned for Leopard.
                    // Utill then, just populate like normal.
                    if ((CFDictionaryGetTypeID()==CFGetTypeID(elementType)) && !isElementCached && (fElements[index].type==kIOHIDElementTypeCollection))
                    {
                        CFMutableDictionaryRef tempMatchingDict;
                        tempMatchingDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

                        if ( tempMatchingDict )
                        {                            
                            CFArrayRef tempSubElements = 0;
                            
                            setElementDictIntValue(tempMatchingDict, CFSTR(kIOHIDElementCollectionCookieKey), fElements[index].cookieMin+rangeIndex);
                            copyMatchingElements(tempMatchingDict, &tempSubElements, elementType, elementCache, options);
                            
                            if (tempSubElements)
                            {
                                CFDictionarySetValue((CFMutableDictionaryRef)elementType, CFSTR(kIOHIDElementKey), tempSubElements);
                                CFRelease(tempSubElements);
                            }
                            
                            CFRelease(tempMatchingDict);
                        }
                    }
                }
                				
				// Matched on duplicate index so we can break out of the loop
				if ( isMatchingDupIndex )
                    break;
                    
                // Break out of both loops if we are looking for a single cookie
                if (isMatchingCookieMin && (matchingCookieMin == fElements[index].cookieMin) &&
                    isMatchingCookieMax && (matchingCookieMax == matchingCookieMin))
                    goto FINISH_ELEMENT_SEARCH;
            }
            
            rangeIndex++;
            
            if ((fElements[index].usageMin+usageIndex) != fElements[index].usageMax )
                 usageIndex++;

            // if a duplicate adjust the other stuff as well
            if ( fElements[index].duplicateValueSize && !isDuplicateRoot)
                duplicateIndex++;
                
            isDuplicateRoot = false;
        }      
    }

FINISH_ELEMENT_SEARCH:
    *elements = tempElements;

    if (CFArrayGetCount(*elements) == 0)
    {
        CFRelease(*elements);
        *elements = 0;
    }
    
    return kIOReturnSuccess;
}

IOReturn IOHIDDeviceClass::setInterruptReportCallback(uint8_t * report, CFIndex reportLength, IOHIDReportCallback callback, void * refcon, IOOptionBits options)
                                
{
    IOReturn ret = kIOReturnSuccess;

    fInputReportCallback 	= callback;
    fInputReportRefcon		= refcon;
    fInputReportBuffer		= report;
    fInputReportBufferSize	= reportLength;
    fInputReportOptions     = options;
     
    // Lazy set up of the queue.
    if ( !fReportHandlerQueue )
    {
		fReportHandlerQueue = createQueue(true);

        ret = fReportHandlerQueue->create(0, 8);
        
        if (ret != kIOReturnSuccess)
            goto SET_REPORT_HANDLER_CLEANUP;
        
        for (uint32_t i=0; i<fReportHandlerElementCount; i++)
        {
            ret = fReportHandlerQueue->addElement(getElement((IOHIDElementCookie)fReportHandlerElements[i].cookieMin), 0);
    
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
			ret = fReportHandlerQueue->setEventCallback(_hidReportHandlerCallback, this);
			
			if (ret != kIOReturnSuccess) break;
				
			ret = fReportHandlerQueue->setAsyncPort(IONotificationPortGetMachPort(fAsyncPort));
			
			if (ret != kIOReturnSuccess) break;
					
			ret = fReportHandlerQueue->start();

			if (ret != kIOReturnSuccess) break;
            			
		} while ( false );
	}
	return ret;
}

void IOHIDDeviceClass::_cfmachPortCallback(CFMachPortRef cfPort, mach_msg_header_t *msg, CFIndex size, void *info) 
{
    mach_msg_header_t *			msgh = (mach_msg_header_t *)msg;
	IOHIDDeviceClass *			self = (IOHIDDeviceClass *) info;
	
	if ( !self )
		return;
		
    if( msgh->msgh_id == kOSNotificationMessageID)
		IODispatchCalloutFromMessage(cfPort, msg, info);
	else if ( self->fReportHandlerQueue )
		IOHIDQueueClass::queueEventSourceCallback(cfPort, msg, size, self->fReportHandlerQueue);
}

void IOHIDDeviceClass::_hidReportHandlerCallback(void * refcon, IOReturn result, void * sender __unused)
{
    IOHIDValueRef           event;
    IOHIDDeviceClass *		self = (IOHIDDeviceClass *)refcon;
    IOHIDQueueClass *		queue = self->fReportHandlerQueue;
    uint32_t                value, size = 0;

    if (!self || !self->fIsOpen)
        return;
            
    while ((result = queue->copyNextEventValue( &event, 0, 0)) == kIOReturnSuccess) 
    {
        if (IOHIDValueGetBytePtr(event) && IOHIDValueGetLength(event))
        {
            size = min(self->fInputReportBufferSize, IOHIDValueGetLength(event));
            bcopy(IOHIDValueGetBytePtr(event), self->fInputReportBuffer, size);
        }
        
        if (self->fInputReportCallback)            
            (self->fInputReportCallback)(
                                        self->fInputReportRefcon, 
                                        result, 
                                        &(self->fHIDDevice),
                                        kIOHIDReportTypeInput,
                                        IOHIDElementGetReportID(IOHIDValueGetElement(event)),
                                        self->fInputReportBuffer,
                                        size);

        CFRelease(event);
    }
}


void 
IOHIDDeviceClass::_hidReportCallback(void *refcon, IOReturn result, uint32_t bufferSize)
{
    IOHIDReportRefCon *hidRefcon = (IOHIDReportRefCon *)refcon;
    
    if (!hidRefcon || !hidRefcon->callback)
        return;
    
    ((IOHIDReportCallback)hidRefcon->callback)( hidRefcon->callbackRefcon,
                                                result,
                                                hidRefcon->sender,
                                                hidRefcon->type,
                                                hidRefcon->reportID,
                                                hidRefcon->buffer,
                                                bufferSize);
                                                    
    free(hidRefcon);
}

IOReturn IOHIDDeviceClass::startAllQueues()
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
            ret = queues[i]->start();
        }
        
        if (queues)
            free(queues);

    }

    return ret;
}

IOReturn IOHIDDeviceClass::stopAllQueues()
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
            ret = queues[i]->stop();
        }
        
        if (queues)
            free(queues);
    }

    return ret;
}

IOCFPlugInInterface IOHIDDeviceClass::sIOCFPlugInInterfaceV1 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    1, 0,	// version/revision
    &IOHIDDeviceClass::_probe,
    &IOHIDDeviceClass::_start,
    &IOHIDDeviceClass::_stop
};

IOHIDDeviceDeviceInterface IOHIDDeviceClass::sHIDDeviceInterfaceV2 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDDeviceClass::_open,
    &IOHIDDeviceClass::_close,
    &IOHIDDeviceClass::_getProperty,
    &IOHIDDeviceClass::_setProperty,
    &IOHIDDeviceClass::_getAsyncEventSource,
    &IOHIDDeviceClass::_copyMatchingElements,
    &IOHIDDeviceClass::_setElementValue,
    &IOHIDDeviceClass::_getElementValue,
    &IOHIDDeviceClass::_setInterruptReportCallback,
    &IOHIDDeviceClass::_setReport,
    &IOHIDDeviceClass::_getReport
};

// Methods for routing iocfplugin interface
IOReturn IOHIDDeviceClass:: _probe(void *self, CFDictionaryRef propertyTable, io_service_t inService, SInt32 *order)
    { return getThis(self)->probe(propertyTable, inService, order); }

IOReturn IOHIDDeviceClass::_start(void *self, CFDictionaryRef propertyTable, io_service_t inService)
    { return getThis(self)->start(propertyTable, inService); }

IOReturn IOHIDDeviceClass::_stop(void *self)
    { return getThis(self)->close(); }

IOReturn IOHIDDeviceClass::_open(void * self, IOOptionBits options)
{ return getThis(self)->open(options); }

IOReturn IOHIDDeviceClass::_close(void * self, IOOptionBits options)
{ return getThis(self)->close(options); }

IOReturn IOHIDDeviceClass::_getProperty(void * self, CFStringRef key, CFTypeRef * pProperty)
{ return getThis(self)->getProperty(key, pProperty); }

IOReturn IOHIDDeviceClass::_setProperty(void * self, CFStringRef key, CFTypeRef property)
{ return getThis(self)->setProperty(key, property); }

IOReturn IOHIDDeviceClass::_getAsyncPort(void * self, mach_port_t * port)
{ return getThis(self)->getAsyncPort(port); }

IOReturn IOHIDDeviceClass::_getAsyncEventSource(void * self, CFTypeRef * pSource)
{ return getThis(self)->getAsyncEventSource(pSource); }
    
IOReturn IOHIDDeviceClass::_copyMatchingElements(void * self, CFDictionaryRef matchingDict, CFArrayRef * elements, IOOptionBits options)
{
    IOReturn            ret     = kIOReturnNoMemory;
    IOHIDDeviceClass *  selfRef = getThis(self);
    
	return selfRef->copyMatchingElements(matchingDict, elements, 0, selfRef->fElementCache, options);        
}

IOReturn IOHIDDeviceClass::_setInterruptReportCallback(void * self, uint8_t * report, CFIndex reportLength, 
                            IOHIDReportCallback callback, void * refcon, IOOptionBits options)
{ return getThis(self)->setInterruptReportCallback(report, reportLength, callback, refcon, options); }

IOReturn IOHIDDeviceClass::_getReport(void * self, IOHIDReportType reportType, uint32_t reportID, uint8_t * report, CFIndex * pReportLength,
                                uint32_t timeout, IOHIDReportCallback callback, void * refcon, IOOptionBits options)
{ return getThis(self)->getReport(reportType, reportID, report, pReportLength, timeout, callback, refcon, options);}

IOReturn IOHIDDeviceClass::_setReport(void * self, IOHIDReportType reportType, uint32_t reportID, const uint8_t * report, CFIndex reportLength,
                                uint32_t timeout, IOHIDReportCallback callback, void * refcon, IOOptionBits options)
{ return getThis(self)->setReport(reportType, reportID, report, reportLength, timeout, callback, refcon, options);}
    
IOReturn IOHIDDeviceClass::_getElementValue(void * self, IOHIDElementRef element, IOHIDValueRef * pEvent,
                                uint32_t timeout, IOHIDValueCallback callback, void * refcon, IOOptionBits options)
{ return getThis(self)->getElementValue(element, pEvent, timeout, callback, refcon, options); };

IOReturn IOHIDDeviceClass::_setElementValue(void * self, IOHIDElementRef element, IOHIDValueRef event,
                                uint32_t timeout, IOHIDValueCallback callback, void * refcon, IOOptionBits options)
{ return getThis(self)->setElementValue(element, event, timeout, callback, refcon, options);};


#define SWAP_KERNEL_ELEMENT(element)                                        \
{                                                                           \
    element.cookieMin           = OSSwapInt32(element.cookieMin);           \
    element.cookieMax           = OSSwapInt32(element.cookieMax);           \
    element.parentCookie        = OSSwapInt32(element.parentCookie);        \
    element.type                = OSSwapInt32(element.type);                \
    element.collectionType      = OSSwapInt32(element.collectionType);      \
    element.flags               = OSSwapInt32(element.flags);               \
    element.usagePage           = OSSwapInt32(element.usagePage);           \
    element.usageMin            = OSSwapInt32(element.usageMin);            \
    element.usageMax            = OSSwapInt32(element.usageMax);            \
    element.min                 = OSSwapInt32(element.min);                 \
    element.max                 = OSSwapInt32(element.max);                 \
    element.scaledMin           = OSSwapInt32(element.scaledMin);           \
    element.scaledMax           = OSSwapInt32(element.scaledMax);           \
    element.size                = OSSwapInt32(element.size);                \
    element.reportSize          = OSSwapInt32(element.reportSize);          \
    element.reportCount         = OSSwapInt32(element.reportCount);         \
    element.reportID            = OSSwapInt32(element.reportID);            \
    element.unit                = OSSwapInt32(element.unit);                \
    element.unitExponent        = OSSwapInt32(element.unitExponent);        \
    element.duplicateValueSize  = OSSwapInt32(element.duplicateValueSize);  \
    element.bytes               = OSSwapInt32(element.bytes);               \
    element.valueLocation       = OSSwapInt32(element.valueLocation);       \
    element.valueSize           = OSSwapInt32(element.valueSize);           \
}

IOReturn IOHIDDeviceClass::buildElements( uint32_t type, CFMutableDataRef * pDataRef, IOHIDElementStruct ** buffer, uint32_t * count )
{
    size_t    size;
    IOReturn    kr;

    
    size = sizeof(IOHIDElementStruct) * *count;

    *pDataRef = CFDataCreateMutable(kCFAllocatorDefault, size);

    if (!*pDataRef)
        return kIOReturnNoMemory;

    // count the number of leaves and allocate
    *buffer = (IOHIDElementStruct*)CFDataGetMutableBytePtr(*pDataRef);

    HIDLog ("IOHIDDeviceClass::buildElements: type=%d *buffer=%4.4x *count=%d size=%d\n", type, *buffer, *count, size);

    bzero(*buffer, size);
    
    uint64_t input = type;
            
    kr = IOConnectCallMethod(fConnection, kIOHIDLibUserClientGetElements, &input, 1, 0, 0, 0, 0, *buffer, &size); 

    *count = size / sizeof(IOHIDElementStruct);

    ROSETTA_ONLY(
        for ( uint32_t i=0; i<*count; i++)
        {
            SWAP_KERNEL_ELEMENT((*buffer)[i]);
        }
    );

#if 0
    for (uint32_t index = 0; index < *count; index++)
    {
        HIDLog ("IOHIDDeviceClass::buildElements: CookieMin=%d CookieMax=%d Min=%d Max=%d UsagePage=0x%x UsageMin=0x%x UsageMax=0x%x Type=%d\n", (*buffer)[index].cookieMin, (*buffer)[index].cookieMax, (*buffer)[index].min, (*buffer)[index].max, (*buffer)[index].usagePage, (*buffer)[index].usageMin, (*buffer)[index].usageMax, (*buffer)[index].type);
    }
#endif

    return kr;
}

IOHIDElementRef IOHIDDeviceClass::getElement(IOHIDElementCookie cookie)
{
    IOHIDElementStruct *elementStruct = 0;
    IOHIDElementRef     element=NULL;
    CFDataRef           dataRef=NULL;
    uint32_t            index=0;
    
    if ( getElementStructPtr(cookie, &elementStruct, &index, &dataRef) )
    {
        element = (IOHIDElementRef)createElement(dataRef, elementStruct, index, 0, fElementCache);
        
        if (element) CFRelease(element); // element should be cached so we can release it
    }
    
    return  element;
}

uint32_t IOHIDDeviceClass::getElementByteSize(IOHIDElementCookie elementCookie)
{
    uint32_t size = 0;
    IOHIDElementStruct element;
    
    if (getElementStruct(elementCookie, &element))
            size = element.bytes;
    
    return size;
}

bool IOHIDDeviceClass::getElementStructPtr(IOHIDElementCookie elementCookie, IOHIDElementStruct ** ppElementStruct, uint32_t * pIndex, CFDataRef * pData)
{
    uint32_t cookieIndex = 0;
    uint32_t index = 0;
    for (index = 0; index < fElementCount; index++)
    {
        if ( ((uint32_t) elementCookie >= fElements[index].cookieMin) && ((uint32_t) elementCookie <= fElements[index].cookieMax) )
        {
            cookieIndex = 0;
            while ( (fElements[index].cookieMin != fElements[index].cookieMax) && ((fElements[index].cookieMin + cookieIndex) != (uint32_t)elementCookie) )
                cookieIndex++;
            
            if ( ppElementStruct )
                *ppElementStruct = &fElements[index];
                
            if ( pIndex )
                *pIndex = cookieIndex;
                
            if ( pData )
                *pData = fElementData;
            return true;
        }
    }    
    cookieIndex = 0;
    for (index = 0; index < fReportHandlerElementCount; index++)
    {
        if ( ((uint32_t) elementCookie >= fReportHandlerElements[index].cookieMin) && ((uint32_t) elementCookie <= fReportHandlerElements[index].cookieMax) )
        {
            if ( ppElementStruct )
                *ppElementStruct = &fReportHandlerElements[index];
                
            if ( pIndex )
                *pIndex = cookieIndex;

            if ( pData )
                *pData = fReportHandlerElementData;
            return true;
        }
    }        
    return false;
}

bool IOHIDDeviceClass::getElementStruct(IOHIDElementCookie elementCookie, IOHIDElementStruct * elementStruct)
{    
    IOHIDElementStruct * pElementStruct = 0;
    
    if (getElementStructPtr(elementCookie, &pElementStruct))
    {
        if (pElementStruct)
        {            
            IOHIDElementStruct tempElement = *pElementStruct;

            while ( tempElement.cookieMin != (uint32_t)elementCookie )
            {
                tempElement.cookieMin++;
                
                if (tempElement.duplicateValueSize)
                    tempElement.valueLocation -= tempElement.duplicateValueSize;
                else
                    tempElement.valueLocation -= tempElement.valueSize;
            }

            // if a duplicate adjust the other stuff as well
            if ( tempElement.duplicateValueSize && (tempElement.cookieMin != pElementStruct->cookieMin))
            {
                tempElement.size = tempElement.reportSize;
                tempElement.bytes = (tempElement.size + 7) / 8;
            }
            
            if (elementStruct)
                *elementStruct = tempElement;
        }
        
        return true;
    }
    
    return false;    
}

//****************************************************************************************************
// Class:       IOHIDObsoleteDeviceClass
// Subclasses:  IOHIDDeviceClass
//****************************************************************************************************
IOCFPlugInInterface ** IOHIDObsoleteDeviceClass::alloc()
{
    IOHIDObsoleteDeviceClass *me;

    me = new IOHIDObsoleteDeviceClass;
    if (me)
        return (IOCFPlugInInterface **) &me->iunknown.pseudoVTable;
    else
        return 0;
}

IOHIDObsoleteDeviceClass::IOHIDObsoleteDeviceClass() : IOHIDDeviceClass()
{
    fHIDDevice.pseudoVTable = (IUnknownVTbl *)  &sHIDDeviceInterfaceV122;
    fHIDDevice.obj = this;
	fInputReportContext = NULL;
}

//====================================================================================================
// IOHIDObsoleteDeviceClass::IOHIDDeviceInterfaceV1 Static Methods
//====================================================================================================

IOHIDDeviceInterface122 IOHIDObsoleteDeviceClass::sHIDDeviceInterfaceV122 =
{
    0,
    &IOHIDIUnknown::genericQueryInterface,
    &IOHIDIUnknown::genericAddRef,
    &IOHIDIUnknown::genericRelease,
    &IOHIDObsoleteDeviceClass::_createAsyncEventSource,
    &IOHIDObsoleteDeviceClass::_getAsyncEventSource,
    &IOHIDDeviceClass::_getAsyncPort,
    &IOHIDObsoleteDeviceClass::_getAsyncPort,
    &IOHIDObsoleteDeviceClass::_open,
    &IOHIDObsoleteDeviceClass::_close,
    &IOHIDObsoleteDeviceClass::_setRemovalCallback,
    &IOHIDObsoleteDeviceClass::_getElementValue,
    &IOHIDObsoleteDeviceClass::_setElementValue,
    &IOHIDObsoleteDeviceClass::_queryElementValue,
    &IOHIDObsoleteDeviceClass::_startAllQueues,
    &IOHIDObsoleteDeviceClass::_stopAllQueues,
    &IOHIDObsoleteDeviceClass::_allocQueue,
    &IOHIDObsoleteDeviceClass::_allocOutputTransaction,
    &IOHIDObsoleteDeviceClass::_setReport,
    &IOHIDObsoleteDeviceClass::_getReport,
    &IOHIDObsoleteDeviceClass::_copyMatchingElements,
    &IOHIDObsoleteDeviceClass::_setInterruptReportHandlerCallback
};

//====================================================================================================
// IOHIDObsoleteDeviceClass::IOHIDDeviceInterfaceV1 Static Methods
//====================================================================================================
IOReturn IOHIDObsoleteDeviceClass::_createAsyncEventSource(void * self, CFRunLoopSourceRef * pSource)
{
    return getThis(self)->createAsyncEventSource(pSource);
}

CFRunLoopSourceRef IOHIDObsoleteDeviceClass::_getAsyncEventSource(void *self)
{ 
    CFTypeRef source = NULL; 
    getThis(self)->getAsyncEventSource(&source); 
    return (CFRunLoopSourceRef)source;
}

mach_port_t IOHIDObsoleteDeviceClass::_getAsyncPort(void *self)
{ 
    mach_port_t port = MACH_PORT_NULL; 
    getThis(self)->getAsyncPort(&port); 
    return port;
}

IOReturn IOHIDObsoleteDeviceClass::_close(void *self)
{ 
    return getThis(self)->close();
}

IOReturn IOHIDObsoleteDeviceClass::_setRemovalCallback(void * self, IOHIDCallbackFunction callback, void * target, void * refcon)
{ 
    return getThis(self)->setRemovalCallback (callback, target, refcon); 
}

IOReturn IOHIDObsoleteDeviceClass::_getElementValue(void * self, IOHIDElementCookie elementCookie, IOHIDEventStruct * valueEvent)
{ 
    return getThis(self)->getElementValue (elementCookie, valueEvent);
}

IOReturn IOHIDObsoleteDeviceClass::_setElementValue(void * self, IOHIDElementCookie cookie, IOHIDEventStruct * pEvent, uint32_t timeout, IOHIDElementCallbackFunction callback, void * target, void * refcon)
{ 
    return getThis(self)->setElementValue(cookie, pEvent, timeout, callback, target, refcon);
}

IOReturn IOHIDObsoleteDeviceClass::_queryElementValue(void * self, IOHIDElementCookie cookie, IOHIDEventStruct * pEvent, uint32_t timeout, IOHIDElementCallbackFunction callback, void * target, void * refcon)
{ 
    return getThis(self)->queryElementValue (cookie, pEvent, timeout, callback, target, refcon);
}

IOReturn IOHIDObsoleteDeviceClass::_startAllQueues(void * self)
{ 
    return getThis(self)->startAllQueues ();
}

IOReturn IOHIDObsoleteDeviceClass::_stopAllQueues(void * self)
{ 
    return getThis(self)->stopAllQueues ();
}

IOHIDQueueInterface ** IOHIDObsoleteDeviceClass::_allocQueue(void *self)
{ 
    return getThis(self)->allocQueue ();
}
    
IOHIDOutputTransactionInterface ** IOHIDObsoleteDeviceClass::_allocOutputTransaction (void *self)
{ 
    return getThis(self)->allocOutputTransaction();
}
    
IOReturn IOHIDObsoleteDeviceClass::_setReport (void * self, IOHIDReportType type, uint32_t id, void * report, uint32_t length, uint32_t timeout, IOHIDReportCallbackFunction callback, void * target, void * refcon)
{ 
    return getThis(self)->setReport(type, id, report, length, timeout, callback, target, refcon);
}

IOReturn IOHIDObsoleteDeviceClass::_getReport (void * self, IOHIDReportType type, uint32_t id, void * report, uint32_t * pLength, uint32_t timeout, IOHIDReportCallbackFunction callback, void * target, void * refcon)
{ 
    return getThis(self)->getReport(type, id, report, pLength, timeout, callback, target, refcon);
}

IOReturn IOHIDObsoleteDeviceClass::_copyMatchingElements(void * self, CFDictionaryRef matchingDict, CFArrayRef *elements)
{
    IOReturn ret = kIOReturnNoMemory;
    CFMutableDictionaryRef cache = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    if ( cache )
    {
        ret = getThis(self)->copyMatchingElements(matchingDict, elements, 0, cache, kCreateMatchingHIDElementsWithDictionaries);
        CFRelease(cache);
    }
        
    return ret;
}

IOReturn IOHIDObsoleteDeviceClass::_setInterruptReportHandlerCallback(void * self, void * report, uint32_t length, IOHIDReportCallbackFunction callback, void * target, void * refcon)
{
    return getThis(self)->setInterruptReportHandlerCallback(report, length, callback, target, refcon);
}

IOHIDQueueClass * IOHIDObsoleteDeviceClass::createQueue(bool reportHandler)
{
    IOHIDObsoleteQueueClass * newQueue = new IOHIDObsoleteQueueClass;
    
    // attach the queue to us
    attachQueue (newQueue, reportHandler);

	return newQueue;
}

HRESULT IOHIDObsoleteDeviceClass::queryInterfaceTransaction (CFUUIDRef uuid __unused, void **ppv)
{
    HRESULT res = S_OK;
    
    IOHIDOutputTransactionClass * transaction = new IOHIDOutputTransactionClass;
    
    transaction->setDirection(kIOHIDTransactionDirectionTypeOutput);
    
    attachTransaction(transaction);
    
    *ppv = transaction->getInterfaceMap();
        
    return res;
}


HRESULT IOHIDObsoleteDeviceClass::queryInterface(REFIID iid, void **ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, kIOHIDQueueInterfaceID))
        res = queryInterfaceQueue(uuid, ppv);
    else if (CFEqual(uuid, kIOHIDOutputTransactionInterfaceID))
        res = queryInterfaceTransaction(uuid, ppv);
    else if (CFEqual(uuid, kIOHIDDeviceInterfaceID) || CFEqual(uuid, kIOHIDDeviceInterfaceID121) || CFEqual(uuid, kIOHIDDeviceInterfaceID122))
    {
        *ppv = &fHIDDevice;
        addRef();
    }
    else {
        res = IOHIDDeviceClass::queryInterface(iid, ppv);
    }

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
    return res;
}

IOReturn IOHIDObsoleteDeviceClass::createAsyncEventSource(CFRunLoopSourceRef * pSource)
{
    IOReturn ret = IOHIDDeviceClass::getAsyncEventSource((CFTypeRef*)pSource);
    
    if ( ret == kIOReturnSuccess && pSource && *pSource )
        CFRetain(*pSource);
        
    return ret;
}


IOReturn IOHIDObsoleteDeviceClass::setRemovalCallback(IOHIDCallbackFunction removalCallback, void * removalTarget, void * removalRefcon)
{	
	IOReturn ret = kIOReturnSuccess;

	if (!fAsyncPrivateDataRef)
	{
		
		fAsyncPrivateDataRef = (MyPrivateData *)malloc(sizeof(MyPrivateData));
		bzero(fAsyncPrivateDataRef, sizeof(MyPrivateData));
		
		fAsyncPrivateDataRef->self		= this;

        if (!fAsyncPort) {     
            ret = getAsyncPort(0);
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

IOReturn 
IOHIDObsoleteDeviceClass::setElementValue(IOHIDElementRef element, 
                                          IOHIDValueRef event, 
                                          uint32_t timeout, 
                                          IOHIDValueCallback callback, 
                                          void * refcon, 
                                          IOOptionBits options)
{
    return IOHIDDeviceClass::setElementValue(element, event, timeout, callback, refcon, options);
}

IOReturn IOHIDObsoleteDeviceClass::setElementValue(IOHIDElementCookie cookie, IOHIDEventStruct * pEvent, uint32_t timeout, IOHIDElementCallbackFunction callback, void * target, void * refcon, IOOptionBits options)
{
    IOHIDElementRef element = getElement(cookie);
    IOHIDValueRef   event   = _IOHIDValueCreateWithStruct(kCFAllocatorDefault, element, pEvent);
    IOReturn        kr      = kIOReturnNoMemory;
    
    if ( event )
    {
        IOHIDValueCallback          valueCallback   = NULL;
        IOHIDObsoleteCallbackArgs * valueArgs       = NULL;
        
        if ( callback ) {
            valueCallback   = IOHIDObsoleteDeviceClass::_elementValueCallback;
            valueArgs       = (IOHIDObsoleteCallbackArgs *)malloc(sizeof(IOHIDObsoleteCallbackArgs));
            
            if ( !valueArgs ) {
                CFRelease(event);
                return kIOReturnNoMemory;
            }
 
            bzero(valueArgs, sizeof(IOHIDObsoleteCallbackArgs));
           
            valueArgs->self     = this;
            valueArgs->callback = (void *)callback;
            valueArgs->target   = target;
            valueArgs->refcon   = refcon;
        }
        
        kr = IOHIDDeviceClass::setElementValue(element, event, timeout, valueCallback, valueArgs, options);
        CFRelease(event);
    }
    
    return kr;
}

IOReturn IOHIDObsoleteDeviceClass::queryElementValue(IOHIDElementCookie cookie, IOHIDEventStruct * pEvent, uint32_t timeout, IOHIDElementCallbackFunction callback, void * target, void * refcon)
{
    IOHIDValueRef   event = 0;
    IOReturn        kr = kIOReturnBadArgument;
    
    if ( pEvent )
    {
        IOHIDValueCallback       valueCallback   = NULL;
        IOHIDObsoleteCallbackArgs *     valueArgs       = NULL;
        
        if ( callback ) {
            valueCallback   = IOHIDObsoleteDeviceClass::_elementValueCallback;
            valueArgs       = (IOHIDObsoleteCallbackArgs *)malloc(sizeof(IOHIDObsoleteCallbackArgs));
            
            if ( !valueArgs )
                return kIOReturnNoMemory;

            bzero(valueArgs, sizeof(IOHIDObsoleteCallbackArgs));

            valueArgs->self     = this;
            valueArgs->callback = (void *)callback;
            valueArgs->target   = target;
            valueArgs->refcon   = refcon;
        }
        
        kr = IOHIDDeviceClass::getElementValue(getElement(cookie), &event, timeout, valueCallback, valueArgs, kHIDGetElementValueForcePoll);
        
        if ((kr==kIOReturnSuccess) && event)
        {
            uint32_t length = _IOHIDElementGetLength(IOHIDValueGetElement(event));
            
            pEvent->type                    = IOHIDElementGetType(IOHIDValueGetElement(event));
            pEvent->elementCookie           = IOHIDElementGetCookie(IOHIDValueGetElement(event));
            *(UInt64 *)& pEvent->timestamp  = IOHIDValueGetTimeStamp(event);
            
            if ( length > sizeof(uint32_t) )
            {
                pEvent->longValueSize = length;
                pEvent->longValue     = malloc(length);
                bcopy(IOHIDValueGetBytePtr(event), pEvent->longValue, length);
            }
            else
            {
                pEvent->longValueSize = 0;
                pEvent->longValue     = NULL;
                pEvent->value         = IOHIDValueGetIntegerValue(event);
            }
        }
    }
    
    return kr;
}

IOReturn 
IOHIDObsoleteDeviceClass::getElementValue(IOHIDElementRef element, 
                                          IOHIDValueRef * pEvent, 
                                          uint32_t timeout, 
                                          IOHIDValueCallback callback, 
                                          void * refcon, 
                                          IOOptionBits options)
{
    return IOHIDDeviceClass::getElementValue(element, pEvent, timeout, callback, refcon, options);
}

IOReturn IOHIDObsoleteDeviceClass::getElementValue(IOHIDElementCookie cookie, IOHIDEventStruct * pEvent)

{
    IOHIDValueRef   event = 0;
    IOReturn        kr = kIOReturnBadArgument;

    if ( pEvent )
    {
        kr = IOHIDDeviceClass::getElementValue(getElement(cookie), &event);
        
        if ((kr==kIOReturnSuccess) && event)
        {
            uint32_t length = _IOHIDElementGetLength(IOHIDValueGetElement(event));
            
            pEvent->type                    = IOHIDElementGetType(IOHIDValueGetElement(event));
            pEvent->elementCookie           = IOHIDElementGetCookie(IOHIDValueGetElement(event));
            *(UInt64 *)& pEvent->timestamp  = IOHIDValueGetTimeStamp(event);
            
            if ( length > sizeof(uint32_t) )
            {
                pEvent->longValueSize = length;
                pEvent->longValue     = malloc(length);
                bcopy(IOHIDValueGetBytePtr(event), pEvent->longValue, length);
            }
            else
            {
                pEvent->longValueSize = 0;
                pEvent->longValue     = NULL;
                pEvent->value         = IOHIDValueGetIntegerValue(event);
            }
        }
    }
    
    return kr;
}

IOReturn 
IOHIDObsoleteDeviceClass::setReport(IOHIDReportType reportType, 
                           uint32_t reportID, 
                           const uint8_t * report, 
                           CFIndex reportLength, 
                           uint32_t timeout, 
                           IOHIDReportCallback callback, 
                           void * refcon, 
                           IOOptionBits options)
{
    return IOHIDDeviceClass::setReport(reportType, reportID, report, reportLength, timeout, callback, refcon, options);
}

IOReturn IOHIDObsoleteDeviceClass::setReport(IOHIDReportType type, uint32_t id, void * report, uint32_t length, uint32_t timeout, IOHIDReportCallbackFunction callback, void * target, void * refcon)
{
    IOHIDReportCallback         reportCallback  = NULL;
    IOHIDObsoleteCallbackArgs * reportContext   = NULL;

    if ( callback ) {
        reportCallback  = IOHIDObsoleteDeviceClass::_reportCallback;
        reportContext   = (IOHIDObsoleteCallbackArgs *)malloc(sizeof(IOHIDObsoleteCallbackArgs));
        
        if ( !reportContext )
            return kIOReturnNoMemory;
            
        bzero(reportContext, sizeof(IOHIDObsoleteCallbackArgs));
        
        reportContext->self     = this;
        reportContext->callback = (void *)callback;
        reportContext->target   = target;
        reportContext->refcon   = refcon;
    }
    
    return IOHIDDeviceClass::setReport(type, id, (const uint8_t *)report, length, timeout, reportCallback, reportContext);
}

IOReturn
IOHIDObsoleteDeviceClass::getReport(IOHIDReportType reportType, 
                           uint32_t reportID, 
                           uint8_t * report, 
                           CFIndex * pReportLength, 
                           uint32_t timeout, 
                           IOHIDReportCallback callback, 
                           void * refcon, 
                           IOOptionBits options)
{
    return IOHIDDeviceClass::getReport(reportType, reportID, report, pReportLength, timeout, callback, refcon, options);
}

IOReturn IOHIDObsoleteDeviceClass::getReport(IOHIDReportType type, uint32_t id, void * report, uint32_t * pLength, uint32_t timeout, IOHIDReportCallbackFunction callback, void * target, void * refcon)
{
    IOHIDReportCallback         reportCallback  = NULL;
    IOHIDObsoleteCallbackArgs * reportContext   = NULL;

    if ( callback ) {
        reportCallback  = IOHIDObsoleteDeviceClass::_reportCallback;
        reportContext   = (IOHIDObsoleteCallbackArgs *)malloc(sizeof(IOHIDObsoleteCallbackArgs));
        
        if ( !reportContext )
            return kIOReturnNoMemory;

        bzero(reportContext, sizeof(IOHIDObsoleteCallbackArgs));
        
        reportContext->self     = this;
        reportContext->callback = (void *)callback;
        reportContext->target   = target;
        reportContext->refcon   = refcon;
        reportContext->pLength  = pLength;
    }
    
    CFIndex pReportLength = pLength ? *pLength : 0;
    
    IOReturn ret = IOHIDDeviceClass::getReport(type, id, (uint8_t *)report, &pReportLength, timeout, reportCallback, reportContext);
    
    if ( pLength )
        *pLength = pReportLength;
        
    return ret;
}

IOReturn IOHIDObsoleteDeviceClass::setInterruptReportHandlerCallback(void * report, uint32_t length, IOHIDReportCallbackFunction callback, void * target, void * refcon)
{
    IOHIDObsoleteCallbackArgs * reportContext = (IOHIDObsoleteCallbackArgs *)fInputReportContext;

    if ( !reportContext ) {
        reportContext = (IOHIDObsoleteCallbackArgs *)malloc(sizeof(IOHIDObsoleteCallbackArgs));
        
        if ( !reportContext )
            return kIOReturnNoMemory;
            
        fInputReportContext = reportContext;
    }

    bzero(reportContext, sizeof(IOHIDObsoleteCallbackArgs));
    
    reportContext->self       = this;
    reportContext->callback   = (void *)callback;
    reportContext->target     = target;
    reportContext->refcon     = refcon;
    
    return IOHIDDeviceClass::setInterruptReportCallback((uint8_t*)report, length, IOHIDObsoleteDeviceClass::_reportCallback, reportContext);
}

void IOHIDObsoleteDeviceClass::_elementValueCallback(void * context, IOReturn result, void * sender, IOHIDValueRef value)
{
    IOHIDObsoleteCallbackArgs * valueArgs = (IOHIDObsoleteCallbackArgs *)context;
    
    (*(IOHIDElementCallbackFunction)valueArgs->callback)(
                            valueArgs->target, 
                            result, 
                            valueArgs->refcon, 
                            sender, 
                            IOHIDElementGetCookie(IOHIDValueGetElement(value)));
    
    free(context);
}

void IOHIDObsoleteDeviceClass::_reportCallback(
                                        void *                  context, 
                                        IOReturn                result, 
                                        void *                  sender, 
                                        IOHIDReportType         type __unused, 
                                        uint32_t                reportID __unused,
                                        uint8_t *               report __unused, 
                                        CFIndex                 reportLength)
{
    IOHIDObsoleteCallbackArgs * args = (IOHIDObsoleteCallbackArgs*)context;
    
    if ( args->pLength )
        *(args->pLength) = reportLength;
        
    (*(IOHIDReportCallbackFunction)args->callback)(
                        args->target,
                        result,
                        args->refcon,
                        sender,
                        reportLength);
                        
    if ( args->self->fInputReportContext != context )
        free(context);
}

IOHIDQueueInterface ** IOHIDObsoleteDeviceClass::allocQueue()
{
    IOHIDQueueInterface **	iohidqueue;
    HRESULT 			res;
    
    res = this->queryInterface(CFUUIDGetUUIDBytes(kIOHIDQueueInterfaceID), (void **) &iohidqueue);

    return iohidqueue;
}
    
IOHIDOutputTransactionInterface ** IOHIDObsoleteDeviceClass::allocOutputTransaction()
{
    IOHIDOutputTransactionInterface **	iohidoutputtransaction;
    HRESULT 				res;
    
    res = this->queryInterface(CFUUIDGetUUIDBytes(kIOHIDOutputTransactionInterfaceID), (void **) &iohidoutputtransaction);

    return iohidoutputtransaction;
}
