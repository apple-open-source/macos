/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
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
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CoreFoundation.h>

#include "IOUSBInterfaceClass.h"
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBUserClient.h>

#include <stdio.h>

__BEGIN_DECLS
#include <mach/mach.h>
#include <IOKit/iokitmig.h>
__END_DECLS

#ifndef IOUSBLIBDEBUG
    #define IOUSBLIBDEBUG		0
#endif

#if IOUSBLIBDEBUG
    #define DEBUGPRINT(x,...)	printf(x, ##__VA_ARGS__)
#else
    #define DEBUGPRINT(x,...)
#endif

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

IOCFPlugInInterface ** IOUSBInterfaceClass::alloc()
{
    IOUSBInterfaceClass *me;

    me = new IOUSBInterfaceClass;
    if (me)
        return (IOCFPlugInInterface **) &me->iunknown.pseudoVTable;
    else
        return 0;
}


IOUSBInterfaceClass::IOUSBInterfaceClass()
: IOUSBIUnknown(&sIOCFPlugInInterfaceV1),
  fService(MACH_PORT_NULL),
  fConnection(MACH_PORT_NULL),
  fAsyncPort(MACH_PORT_NULL),
  fCFSource(0),
  fIsOpen(false)
{
    fUSBInterface.pseudoVTable = (IUnknownVTbl *)  &sUSBInterfaceInterfaceV197;
    fUSBInterface.obj = this;
}


IOUSBInterfaceClass::~IOUSBInterfaceClass()
{
    if (fConnection) {
        IOServiceClose(fConnection);
        fConnection = MACH_PORT_NULL;
    }
        
    if (fService) {
        IOObjectRelease(fService);
        fService = MACH_PORT_NULL;
    }
}


HRESULT 
IOUSBInterfaceClass::queryInterface(REFIID iid, void **ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOCFPlugInInterfaceID)) 
    {
        *ppv = &iunknown;
        addRef();
    }
    else if (CFEqual(uuid, kIOUSBInterfaceInterfaceID)
             || CFEqual(uuid, kIOUSBInterfaceInterfaceID182)
             || CFEqual(uuid, kIOUSBInterfaceInterfaceID183)
             || CFEqual(uuid, kIOUSBInterfaceInterfaceID190)
             || CFEqual(uuid, kIOUSBInterfaceInterfaceID192)
             || CFEqual(uuid, kIOUSBInterfaceInterfaceID197)) 
    {
        *ppv = &fUSBInterface;
        addRef();
    }
    else
        *ppv = 0;

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
    return res;
}



IOReturn 
IOUSBInterfaceClass::probe(CFDictionaryRef propertyTable, io_service_t inService, SInt32 *order)
{
    if (!inService || !IOObjectConformsTo(inService, "IOUSBInterface"))
        return kIOReturnBadArgument;

    return kIOReturnSuccess;
}



IOReturn 
IOUSBInterfaceClass::start(CFDictionaryRef propertyTable, io_service_t inService)
{
    IOReturn 			res;
    mach_msg_type_number_t 	len = 1;

    fService = inService;
    fNextCookie = 0;
    fUserBufferInfoListHead = NULL;
    res = IOServiceOpen(fService, mach_task_self(), 0, &fConnection);
    if (res != kIOReturnSuccess)
        return res;

    connectCheck();

    
    res = io_connect_method_scalarI_scalarO(fConnection, kUSBInterfaceUserClientGetDevice, NULL, 0, (int *)&fDevice, &len);

    if (res)
        fDevice = NULL;
    
    // Make sure that we retain our service so that we can use it later on
    //
    res = mach_port_mod_refs(mach_task_self(), fService, MACH_PORT_RIGHT_SEND, 1);
    
    if (res)
        return res;
        
    return GetPropertyInfo();
}


IOReturn
IOUSBInterfaceClass::GetPropertyInfo(void)
{
    IOReturn			kr;
    CFMutableDictionaryRef 	entryProperties = 0;
    
    kr = IORegistryEntryCreateCFProperties(fService, &entryProperties, NULL, 0);
    
    if (kr)
        return kr;
        
    if (entryProperties) 
    {
        CFTypeRef val;
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBInterfaceClass));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fClass);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBInterfaceSubClass));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fSubClass);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBInterfaceProtocol));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fProtocol);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBVendorID));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberShortType, (void*)&fVendor);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBProductID));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberShortType, (void*)&fProduct);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDeviceReleaseNumber));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberShortType, (void*)&fDeviceReleaseNumber);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBConfigurationValue));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fConfigValue);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBInterfaceNumber));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fInterfaceNumber);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBAlternateSetting));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fAlternateSetting);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBNumEndpoints));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fNumEndpoints);
       val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBInterfaceStringIndex));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fStringIndex);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDevicePropertyLocationID));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongType, (void*)&fLocationID);
        CFRelease(entryProperties);
    }

    return kIOReturnSuccess;
}



IOReturn 
IOUSBInterfaceClass::CreateInterfaceAsyncEventSource(CFRunLoopSourceRef *source)
{
    IOReturn ret;
    CFMachPortRef cfPort;
    CFMachPortContext context;
    Boolean shouldFreeInfo;

    if (!fAsyncPort) {     
        ret = CreateInterfaceAsyncPort(0);
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



CFRunLoopSourceRef 
IOUSBInterfaceClass::GetInterfaceAsyncEventSource()
{
    return fCFSource;
}



IOReturn 
IOUSBInterfaceClass::CreateInterfaceAsyncPort(mach_port_t *port)
{
    IOReturn 		ret;

    ret = IOCreateReceivePort(kOSAsyncCompleteMessageID, &fAsyncPort);
    if (kIOReturnSuccess == ret) 
    {
        if (port)
            *port = fAsyncPort;

        if (fIsOpen) 
	{
            natural_t asyncRef[1];
            mach_msg_type_number_t len = 0;
        
            // async kIOCDBUserClientSetAsyncPort,  kIOUCScalarIScalarO,    0,	0
            ret = io_async_method_scalarI_scalarO( fConnection, fAsyncPort, asyncRef, 1,  kUSBInterfaceUserClientSetAsyncPort, NULL, 0, NULL, &len);
	    if (ret == MACH_SEND_INVALID_DEST)
	    {
		fIsOpen = false;
		fConnection = MACH_PORT_NULL;
		ret = kIOReturnNoDevice;
	    }
        }
    }

    return ret;
}



mach_port_t 
IOUSBInterfaceClass::GetInterfaceAsyncPort()
{
    return fAsyncPort;
}



IOReturn 
IOUSBInterfaceClass::USBInterfaceOpen(bool seize)
{
    IOReturn 		ret;
    int			t = seize;

    connectCheck();

    if (fIsOpen)
        return kIOReturnSuccess;

    mach_msg_type_number_t len = 0;

    //  kIOCDBUserClientOpen,  kIOUCScalarIScalarO,    0,	0
    ret = io_connect_method_scalarI_scalarO(fConnection, kUSBInterfaceUserClientOpen, &t, 1, NULL, &len);
    if (ret == kIOReturnSuccess)
    {
	fIsOpen = true;
    
	if (fAsyncPort) 
	{
	    natural_t asyncRef[1];
	    mach_msg_type_number_t len = 0;
	
	    // async 
	    // kIOCDBUserClientSetAsyncPort,  kIOUCScalarIScalarO,    0,	0
	    ret = io_async_method_scalarI_scalarO(fConnection, fAsyncPort, asyncRef, 1, kUSBInterfaceUserClientSetAsyncPort, NULL, 0, NULL, &len);
	    if ((ret != kIOReturnSuccess) && (ret != MACH_SEND_INVALID_DEST))
		USBInterfaceClose();
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
IOUSBInterfaceClass::USBInterfaceClose()
{
    IOReturn		ret;
#if 0
    LowLatencyUserBufferInfo *	buffer;
    LowLatencyUserBufferInfo *	nextBuffer;
#endif
    
    allChecks();

    mach_msg_type_number_t len = 0;
    fIsOpen = false;

    // kIOCDBUserClientClose,	kIOUCScalarIScalarO,	 0,  0
    ret = io_connect_method_scalarI_scalarO(fConnection, kUSBInterfaceUserClientClose, NULL, 0, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }

#if 0    
    // Need to free any buffers that has been allocated by the low latency stuff that has not been
    // released!
    //
    if (fUserBufferInfoListHead != NULL)
    {
        nextBuffer = fUserBufferInfoListHead;
        buffer = fUserBufferInfoListHead;
        DEBUGPRINT("fUserBufferInfoListHead != NULL: %p, next: %p\n",fUserBufferInfoListHead, buffer->nextBuffer);
        
        // Traverse the list and release memory
        //
        while ( nextBuffer != NULL )
        {
            nextBuffer = buffer->nextBuffer;
            
            DEBUGPRINT("Releasing %p, %p\n", buffer->bufferAddress, buffer );
            free ( buffer->bufferAddress );
            free ( buffer );
            
            buffer = nextBuffer;
        }
        
        fUserBufferInfoListHead = NULL;
    }

#endif
 
    return ret;
}



IOReturn 
IOUSBInterfaceClass::GetInterfaceClass(UInt8 *intfClass)
{
    connectCheck();
    *intfClass = fClass;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetInterfaceSubClass(UInt8 *intfSubClass)
{
    connectCheck();
    *intfSubClass = fSubClass;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetInterfaceProtocol(UInt8 *intfProtocol)
{
    connectCheck();
    *intfProtocol = fProtocol;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetInterfaceStringIndex(UInt8 *intfSI)
{
    connectCheck();
    *intfSI = fStringIndex;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetDeviceVendor(UInt16 *devVendor)
{
    connectCheck();
    *devVendor = fVendor;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetDeviceProduct(UInt16 *devProduct)
{
    connectCheck();
    *devProduct = fProduct;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetDeviceReleaseNumber(UInt16 *devRelNum)
{
    connectCheck();
    *devRelNum = fDeviceReleaseNumber;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetConfigurationValue(UInt8 *configVal)
{
    connectCheck();
    *configVal = fConfigValue;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetInterfaceNumber(UInt8 *intfNumber)
{
    connectCheck();
    *intfNumber = fInterfaceNumber;
    return kIOReturnSuccess;
}


IOReturn
IOUSBInterfaceClass::GetAlternateSetting(UInt8 *intfAlternateSetting)
{ 
    connectCheck();
    *intfAlternateSetting = fAlternateSetting;
    return kIOReturnSuccess;
}


IOReturn
IOUSBInterfaceClass::GetNumEndpoints(UInt8 *intfNumEndpoints)
{ 
    connectCheck();
    *intfNumEndpoints = fNumEndpoints;
    return kIOReturnSuccess;
}


IOReturn
IOUSBInterfaceClass::GetLocationID(UInt32 *locationID)
{ 
    connectCheck();
    *locationID = fLocationID;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetDevice(io_service_t *device)
{    
    connectCheck();
    *device = fDevice;
    return kIOReturnSuccess;
}



IOReturn 
IOUSBInterfaceClass::SetAlternateInterface(UInt8 alternateSetting)
{    
    int 			t = alternateSetting;
    mach_msg_type_number_t 	len = 0;
    IOReturn			ret;

    allChecks();
    ret = io_connect_method_scalarI_scalarO( fConnection, kUSBInterfaceUserClientSetAlternateInterface, &t, 1, NULL, &len);
    if (ret == kIOReturnSuccess)
	ret = GetPropertyInfo();
    
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBInterfaceClass::GetBusFrameNumber(UInt64 *frame, AbsoluteTime *atTime)
{
    IOUSBGetFrameStruct 	stuff;
    mach_msg_type_number_t 	outSize = sizeof(stuff);
    IOReturn 			ret;

    connectCheck();

    ret = io_connect_method_scalarI_structureO(fConnection, kUSBInterfaceUserClientGetFrameNumber, NULL, 0, (char *)&stuff, &outSize);
    if(kIOReturnSuccess == ret) 
    {
        *frame = stuff.frame;
        *atTime = stuff.timeStamp;
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
IOUSBInterfaceClass::GetBandwidthAvailable(UInt32 *bandwidth)
{
    mach_msg_type_number_t 	outSize = 1;
    IOReturn 			ret;

    connectCheck();

    ret = io_connect_method_scalarI_scalarO(fConnection, kUSBInterfaceUserClientGetBandwidthAvailable, NULL, 0, (int*)bandwidth, &outSize);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBInterfaceClass::GetEndpointProperties(UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval)
{
    int				toUserClient[3];
    int				fromUserClient[3];
    mach_msg_type_number_t 	fromUCsize = 3;
    IOReturn			ret;

    connectCheck();

    toUserClient[0] = alternateSetting;
    toUserClient[1] = endpointNumber;
    toUserClient[2] = direction;
    ret = io_connect_method_scalarI_scalarO(fConnection, kUSBInterfaceUserClientGetEndpointProperties, toUserClient, 3, fromUserClient, &fromUCsize);
    if (ret == kIOReturnSuccess)
    {
	*transferType = fromUserClient[0];
	*maxPacketSize = fromUserClient[1];
	*interval = fromUserClient[2];
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
IOUSBInterfaceClass::ControlRequest(UInt8 pipeRef, IOUSBDevRequestTO *req)
{
    IOReturn 		ret = kIOReturnSuccess;
    
    connectCheck();

    if (req->wLength <= sizeof(io_struct_inband_t))
    {
        // the buffer can be copied directly
        int	in[4];

	in[0] = (pipeRef << 16) | (req->bmRequestType << 8) | req->bRequest;
        in[1] = (req->wValue << 16) | req->wIndex;
        in[2] = req->noDataTimeout;
	in[3] = req->completionTimeout;
	
        switch ((req->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask)
        {
           case kUSBOut:
                ret = io_connect_method_scalarI_structureI(fConnection, kUSBInterfaceUserClientControlRequestOut, in, 4, (char *)req->pData, req->wLength);
                 if(kIOReturnSuccess == ret)
                    req->wLenDone = req->wLength;
                else
                    req->wLenDone = 0;
		break;
                
            case kUSBIn:
                mach_msg_type_number_t 	reqSize = req->wLength;
                ret = io_connect_method_scalarI_structureO(fConnection, kUSBInterfaceUserClientControlRequestIn, in, 4, (char *)req->pData, &reqSize);
                if(kIOReturnSuccess == ret)
                    req->wLenDone = reqSize;
                break;
        }
    }
    else
    {
    // too much data to push through the entire buffer directly. memory must be mapped, so just send the regular structure
        mach_msg_type_number_t 	outSize = 0;
        IOUSBDevReqOOLTO		outReq;
        outReq.bmRequestType = req->bmRequestType;
        outReq.bRequest = req->bRequest;
        outReq.pData = req->pData;
	outReq.wValue = req->wValue;
        outReq.wIndex = req->wIndex;
        outReq.wLength = req->wLength;
        outReq.pipeRef = pipeRef;
	outReq.noDataTimeout = req->noDataTimeout;
	outReq.completionTimeout = req->completionTimeout;

        switch ((req->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask)
        {
            case kUSBOut:
                ret = io_connect_method_structureI_structureO(fConnection, kUSBInterfaceUserClientControlRequestOutOOL, (char *)&outReq, sizeof(outReq), NULL, &outSize);
                 if(kIOReturnSuccess == ret)
                    req->wLenDone = req->wLength;
                else
                    req->wLenDone = 0;
		break;
                
            case kUSBIn:
                mach_msg_type_number_t 	reqSize = req->wLength;
                outSize = sizeof(reqSize);
                ret = io_connect_method_structureI_structureO(fConnection, kUSBInterfaceUserClientControlRequestInOOL, (char *)&outReq, sizeof(outReq), (char *)&reqSize, &outSize);
                if(kIOReturnSuccess == ret)
                    req->wLenDone = reqSize;
                break;
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
IOUSBInterfaceClass::ControlRequestAsync(UInt8 pipeRef, IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon)
{
    mach_msg_type_number_t 	outSize = 0;
    natural_t			asyncRef[kIOAsyncCalloutCount];
    int				selector = 0;
    IOUSBDevReqOOLTO		outReq;
    IOReturn			ret;
        
    connectCheck();

    if (!fAsyncPort)
        return kIOUSBNoAsyncPortErr;

    asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) refCon;

    outReq.bmRequestType = req->bmRequestType;
    outReq.bRequest = req->bRequest;
    outReq.pData = req->pData;
    outReq.wValue = req->wValue;
    outReq.wIndex = req->wIndex;
    outReq.wLength = req->wLength;
    outReq.pipeRef = pipeRef;
    outReq.noDataTimeout = req->noDataTimeout;
    outReq.completionTimeout = req->completionTimeout;
    
    switch ((req->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask)
    {
        case kUSBOut:
            selector = kUSBInterfaceUserClientControlAsyncRequestOut;
            break;
            
        case kUSBIn:
            selector = kUSBInterfaceUserClientControlAsyncRequestIn;
            break;
    }
    ret = io_async_method_structureI_structureO(fConnection, fAsyncPort, asyncRef, kIOAsyncCalloutCount, selector, (char*)&outReq, sizeof(outReq), NULL, &outSize);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn 
IOUSBInterfaceClass::GetPipeProperties(UInt8 pipeRef, UInt8 *direction, UInt8 *number, UInt8 *transferType, 
                                    UInt16 *maxPacketSize, UInt8 *interval)
{
    int				toUserClient = pipeRef;
    int				fromUserClient[5];
    mach_msg_type_number_t 	fromUCsize = 5;
    IOReturn			ret;
    
    allChecks();
    ret =  io_connect_method_scalarI_scalarO(fConnection, kUSBInterfaceUserClientGetPipeProperties,  &toUserClient, 1, fromUserClient, &fromUCsize);
    if (ret == kIOReturnSuccess)
    {
	*direction = fromUserClient[0];
	*number = fromUserClient[1];
	*transferType = fromUserClient[2];
	*maxPacketSize = fromUserClient[3];
	*interval = fromUserClient[4];
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
IOUSBInterfaceClass::GetPipeStatus(UInt8 pipeRef)
{
    mach_msg_type_number_t	len = 0;
    int				toUserClient = pipeRef;
    IOReturn			ret;
    
    allChecks();

    ret = io_connect_method_scalarI_scalarO( fConnection, kUSBInterfaceUserClientGetPipeStatus, &toUserClient, 1, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBInterfaceClass::AbortPipe(UInt8 pipeRef)
{
    mach_msg_type_number_t	len = 0;
    int				toUserClient = pipeRef;
    IOReturn			ret;
    
    allChecks();

    ret = io_connect_method_scalarI_scalarO( fConnection, kUSBInterfaceUserClientAbortPipe, &toUserClient, 1, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBInterfaceClass::ResetPipe(UInt8 pipeRef)
{
    mach_msg_type_number_t	len = 0;
    int				toUserClient = pipeRef;
    IOReturn			ret;

    allChecks();

    ret = io_connect_method_scalarI_scalarO( fConnection, kUSBInterfaceUserClientResetPipe, &toUserClient, 1, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBInterfaceClass::ClearPipeStall(UInt8 pipeRef, bool bothEnds)
{
    mach_msg_type_number_t	len = 0;
    int				toUserClient[2];
    IOReturn			ret;
    
    allChecks();

    toUserClient[0] = pipeRef;
    toUserClient[1] = bothEnds;
    ret = io_connect_method_scalarI_scalarO( fConnection, kUSBInterfaceUserClientClearPipeStall, toUserClient, 2, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBInterfaceClass::SetPipePolicy(UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval)
{
    mach_msg_type_number_t	len = 0;
    int				toUserClient[3];
    IOReturn			ret;
    
    allChecks();

    toUserClient[0] = pipeRef;
    toUserClient[1] = maxPacketSize;
    toUserClient[2] = maxInterval;
    ret = io_connect_method_scalarI_scalarO( fConnection, kUSBInterfaceUserClientSetPipePolicy, toUserClient, 3, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBInterfaceClass::ReadPipe(UInt8 pipeRef, void *buf, UInt32 *size, UInt32 noDataTimeout, UInt32 completionTimeout)
{
    IOReturn		ret;

    allChecks();

    if(*size < sizeof(io_struct_inband_t)) 
    {
	int	in[3];
	in[0] = pipeRef;
	in[1] = noDataTimeout;
	in[2] = completionTimeout;
        ret = io_connect_method_scalarI_structureO( fConnection, kUSBInterfaceUserClientReadPipe, in, 3, (char *)buf, (unsigned int *)size);
    }
    else 
    {
        IOUSBBulkPipeReq		req;
    	mach_msg_type_number_t	len = sizeof(*size);
        req.pipeRef = pipeRef;
        req.buf = buf;
    	req.size = *size;
	req.noDataTimeout = noDataTimeout;
	req.completionTimeout = completionTimeout;
        ret = io_connect_method_structureI_structureO( fConnection, kUSBInterfaceUserClientReadPipeOOL, (char*)&req, sizeof(req), (char*)size, &len);
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
IOUSBInterfaceClass::WritePipe(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout)
{
    IOReturn		ret;

    allChecks();

    if(size < sizeof(io_struct_inband_t)) 
    {
	int	in[3];
	in[0] = pipeRef;
	in[1] = noDataTimeout;
	in[2] = completionTimeout;
        ret = io_connect_method_scalarI_structureI( fConnection, kUSBInterfaceUserClientWritePipe, in, 3, (char *)buf, size);
    }
    else 
    {
        IOUSBBulkPipeReq		req;
    	mach_msg_type_number_t	len = 0;
        req.pipeRef = pipeRef;
        req.buf = buf;
    	req.size = size;
	req.noDataTimeout = noDataTimeout;
	req.completionTimeout = completionTimeout;
        ret = io_connect_method_structureI_structureO( fConnection, kUSBInterfaceUserClientWritePipeOOL, (char*)&req, sizeof(req), NULL, &len);
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
IOUSBInterfaceClass::ReadPipeAsync(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon)
{
    int				in[5];
    natural_t			asyncRef[kIOAsyncCalloutCount];
    mach_msg_type_number_t	len = 0;
    IOReturn			ret;

    allChecks();

    if (!fAsyncPort)
        return kIOUSBNoAsyncPortErr;

    in[0] = (natural_t)pipeRef;
    in[1] = (natural_t)buf;
    in[2] = size;
    in[3] = noDataTimeout;
    in[4] = completionTimeout; 

    asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) refcon;

    ret = io_async_method_scalarI_scalarO( fConnection, fAsyncPort, asyncRef, kIOAsyncCalloutCount, kUSBInterfaceUserClientAsyncReadPipe, in, 5, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn 
IOUSBInterfaceClass::WritePipeAsync(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon)
{
    int				in[5];
    natural_t			asyncRef[kIOAsyncCalloutCount];
    mach_msg_type_number_t	len = 0;
    IOReturn			ret;
    
    allChecks();

    if (!fAsyncPort)
        return kIOUSBNoAsyncPortErr;

    in[0] = (int)pipeRef;
    in[1] = (int)buf;
    in[2] = size;
    in[3] = noDataTimeout;
    in[4] = completionTimeout; 
    
    asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) refcon;

    ret = io_async_method_scalarI_scalarO( fConnection, fAsyncPort, asyncRef, kIOAsyncCalloutCount, kUSBInterfaceUserClientAsyncWritePipe, in, 5, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn 
IOUSBInterfaceClass::ReadIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refcon)
{
    IOUSBIsocStruct		pb;
    natural_t			asyncRef[kIOAsyncCalloutCount];
    mach_msg_type_number_t	len = 0;
    UInt32			i, total;
    IOReturn			ret;

    allChecks();

    if (!fAsyncPort)
        return kIOUSBNoAsyncPortErr;

    total = 0;
    for(i=0; i < numFrames; i++)
        total += frameList[i].frReqCount;

    pb.fPipe = pipeRef;
    pb.fBuffer = buf;
    pb.fBufSize = total;
    pb.fStartFrame = frameStart;
    pb.fNumFrames = numFrames;
    pb.fFrameCounts = frameList;

    asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) refcon;

    ret = io_async_method_structureI_structureO( fConnection, fAsyncPort, asyncRef, kIOAsyncCalloutCount, kUSBInterfaceUserClientReadIsochPipe, (char *)&pb, sizeof(pb), NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;

}



IOReturn 
IOUSBInterfaceClass::WriteIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refcon)
{
    IOUSBIsocStruct		pb;
    natural_t			asyncRef[kIOAsyncCalloutCount];
    mach_msg_type_number_t	len = 0;
    UInt32			i, total;
    IOReturn			ret;

    allChecks();

    if (!fAsyncPort)
        return kIOUSBNoAsyncPortErr;

    total = 0;
    for(i=0; i < numFrames; i++)
        total += frameList[i].frReqCount;

    pb.fPipe = pipeRef;
    pb.fBuffer = buf;
    pb.fBufSize = total;
    pb.fStartFrame = frameStart;
    pb.fNumFrames = numFrames;
    pb.fFrameCounts = frameList;

    asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) refcon;

    ret = io_async_method_structureI_structureO( fConnection, fAsyncPort, asyncRef, kIOAsyncCalloutCount, kUSBInterfaceUserClientWriteIsochPipe, (char *)&pb, sizeof(pb), NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}




IOReturn 
IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon)
{
    IOUSBLowLatencyIsocStruct		pb;
    natural_t				asyncRef[kIOAsyncCalloutCount];
    mach_msg_type_number_t		len = 0;
    UInt32				i, total;
    IOReturn				ret;
    LowLatencyUserBufferInfo *		dataBufferInfo;
    LowLatencyUserBufferInfo *		frameListData;

    allChecks();

    if (!fAsyncPort)
        return kIOUSBNoAsyncPortErr;

    total = 0;
    for(i=0; i < numFrames; i++)
        total += frameList[i].frReqCount;

    // Find the data buffer in our list of buffers
    //
    dataBufferInfo = FindBufferAddressRangeInList( buf, total);
    if ( dataBufferInfo != NULL )
    {
        pb.fDataBufferCookie = dataBufferInfo->cookie;
        pb.fDataBufferOffset = (UInt32) buf - (UInt32)dataBufferInfo->bufferAddress;
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync  Found Data buffer: offset = %ld, cookie: %ld\n", pb.fDataBufferOffset, dataBufferInfo->cookie);
    }
    else
    {
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync Ooops, couldn't find buffer %p in our list\n",buf);
        return kIOUSBLowLatencyBufferNotPreviouslyAllocated;
    }
    
    // Find the frame list buffer in our list of buffers
    //
    frameListData = FindBufferAddressRangeInList( frameList, sizeof (IOUSBLowLatencyIsocFrame) * numFrames);
    if ( frameListData != NULL )
    {
        pb.fFrameListBufferCookie = frameListData->cookie;
        pb.fFrameListBufferOffset = (UInt32) frameList - (UInt32)frameListData->bufferAddress;
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync  Found FrameList buffer: offset = %ld, cookie : %ld\n", pb.fFrameListBufferOffset, frameListData->cookie);
    }
    else
    {
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync  Ooops, couldn't find buffer %p in our list\n",frameList);
        return kIOUSBLowLatencyFrameListNotPreviouslyAllocated;
    }

    pb.fPipe = pipeRef;
    pb.fBufSize = total;
    pb.fStartFrame = frameStart;
    pb.fNumFrames = numFrames;
    pb.fUpdateFrequency = updateFrequency;

    asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) refcon;

    ret = io_async_method_structureI_structureO( fConnection, fAsyncPort, asyncRef, kIOAsyncCalloutCount, kUSBInterfaceUserClientLowLatencyReadIsochPipe, (char *)&pb, sizeof(pb), NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;

}



IOReturn 
IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon)
{
    IOUSBLowLatencyIsocStruct		pb;
    natural_t				asyncRef[kIOAsyncCalloutCount];
    mach_msg_type_number_t		len = 0;
    UInt32				i, total;
    IOReturn				ret;
    LowLatencyUserBufferInfo *		dataBufferInfo;
    LowLatencyUserBufferInfo *		frameListData;

    allChecks();

    if (!fAsyncPort)
        return kIOUSBNoAsyncPortErr;

    total = 0;
    for(i=0; i < numFrames; i++)
        total += frameList[i].frReqCount;

    // Find the data buffer in our list of buffers
    //
    dataBufferInfo = FindBufferAddressRangeInList( buf, total);
    if ( dataBufferInfo != NULL )
    {
        pb.fDataBufferCookie = dataBufferInfo->cookie;
        pb.fDataBufferOffset = (UInt32) buf - (UInt32)dataBufferInfo->bufferAddress;
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync Found Data buffer: offset = %ld, cookie: %ld\n", pb.fDataBufferOffset, dataBufferInfo->cookie);
    }
    else
    {
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync Ooops, couldn't find buffer %p in our list\n",buf);
        return kIOUSBLowLatencyBufferNotPreviouslyAllocated;
    }
    
    // Find the frame list buffer in our list of buffers
    //
    frameListData = FindBufferAddressRangeInList( frameList, sizeof (IOUSBLowLatencyIsocFrame) * numFrames);
    if ( frameListData != NULL )
    {
        pb.fFrameListBufferCookie = frameListData->cookie;
        pb.fFrameListBufferOffset = (UInt32) frameList - (UInt32)frameListData->bufferAddress;
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync Found FrameList buffer: offset = %ld, cookie = %ld\n", pb.fFrameListBufferOffset, frameListData->cookie);
    }
    else
    {
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync Ooops, couldn't find buffer %p in our list\n",frameList);
        return kIOUSBLowLatencyFrameListNotPreviouslyAllocated;
    }

    pb.fPipe = pipeRef;
    pb.fBufSize = total;
    pb.fStartFrame = frameStart;
    pb.fNumFrames = numFrames;
    pb.fUpdateFrequency = updateFrequency;

    asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) refcon;

    ret = io_async_method_structureI_structureO( fConnection, fAsyncPort, asyncRef, kIOAsyncCalloutCount, kUSBInterfaceUserClientLowLatencyWriteIsochPipe, (char *)&pb, sizeof(pb), NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
	fIsOpen = false;
	fConnection = MACH_PORT_NULL;
	ret = kIOReturnNoDevice;
    }
    return ret;
}


IOReturn
IOUSBInterfaceClass::LowLatencyCreateBuffer( void ** buffer, IOByteCount bufferSize, UInt32 bufferType )
{
    LowLatencyUserBufferInfo *	bufferInfo;
    IOReturn			result = kIOReturnSuccess;
    mach_msg_type_number_t 	outSize = 0;
    vm_address_t		data;
    kern_return_t		ret = kIOReturnSuccess;
    
    allChecks();

    DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer size: %d, type, %d\n", (int)bufferSize, (int)bufferType);
    
    // Allocate our buffer Data and zero it
    //
    bufferInfo = ( LowLatencyUserBufferInfo *) malloc( sizeof(LowLatencyUserBufferInfo) );
    if ( bufferInfo == NULL )
    {
        DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  Could not allocate a LowLatencyUserBufferInfo of %ld bytes\n",sizeof(LowLatencyUserBufferInfo));
        *buffer = NULL;
        result = kIOReturnNoMemory;
        goto ErrorExit;
    }
    
    // bzero(bufferInfo, sizeof(LowLatencyUserBufferInfo));
    
    // Now, attempt to allocate the users data
    //
    ret =  vm_allocate( mach_task_self(),
                        &data,
                        bufferSize,
                        VM_FLAGS_ANYWHERE);

    if ( ret != kIOReturnSuccess )
    {
        DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  Could not vm_allocate a buffer of size %ld, type %ld.  Result = 0x%x\n", bufferSize, bufferType, ret);
        result = kIOReturnNoMemory;
        goto ErrorExit;
    }

    *buffer = (void *) data;
    
    if ( *buffer == NULL )
    {
        DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  Could not allocate a buffer of size %ld, type %ld\n", bufferSize, bufferType);
        result = kIOReturnNoMemory;
        goto ErrorExit;
    }
    
    DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer Buffer: %p\n", *buffer);
    
    // Update our buffer Data
    //
    bufferInfo->cookie = fNextCookie++;
    bufferInfo->bufferAddress = *buffer;
    bufferInfo->bufferSize = bufferSize;
    bufferInfo->bufferType = bufferType;
    bufferInfo->isPrepared = false;
    bufferInfo->nextBuffer = NULL;
    
    // OK, ready to call the kernel so that it does its thing with this buffer
    //
    // kIOUCStructIStructO  io_connect_method_structureI_structureO(..., UInt32 * bufferIn,  UInt32 bufferSizeIn,   UInt32 * bufferOut,  UInt32 * bufferSizeInOut )
    //
    result = io_connect_method_structureI_structureO(fConnection, kUSBInterfaceUserClientLowLatencyPrepareBuffer, (char *)bufferInfo, sizeof(LowLatencyUserBufferInfo), NULL, &outSize);
    
    if ( result == kIOReturnSuccess )
    {
        // Cool, we have a good buffer, add it to our list
        //
        AddDataBufferToList( bufferInfo );
    }
    else
    {
        // OK, something went wrong, so we need to release anything we allocated
        //
        fNextCookie--;
        ret = vm_deallocate( mach_task_self(), (vm_address_t) bufferInfo->bufferAddress, bufferInfo->bufferSize );
        *buffer = NULL;
        free ( bufferInfo );
        
        // Fall through to return result
        //
        DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  Kernel call to kUSBInterfaceUserClientLowLatencyPrepareBuffer returned 0x%x\n", result);
    }
    
    
ErrorExit:    
    return result;
}


IOReturn
IOUSBInterfaceClass::LowLatencyDestroyBuffer( void * buffer )
{
    LowLatencyUserBufferInfo *	bufferData;
    IOReturn			result = kIOReturnSuccess;
    mach_msg_type_number_t 	outSize = 0;
    bool			found;
    kern_return_t		ret = kIOReturnSuccess;
    
    allChecks();

    DEBUGPRINT("IOUSBLib::LowLatencyDestroyBuffer, buffer %p\n", buffer);
    
    // We need to find the LowLatencyUserBufferInfo structure that contains
    // this buffer and then remove it from the list and free the structure
    // and the memory that was allocated for it
    //
    bufferData = FindBufferAddressInList( buffer );
    if ( bufferData == NULL )
    {
        DEBUGPRINT("IOUSBLib::LowLatencyDestroyBuffer:  Could not find buffer (%p) in our list\n", buffer);
        result = kIOReturnBadArgument;
        goto ErrorExit;
    }
    
    // Now, remove this bufferData from the list
    //
    found = RemoveDataBufferFromList( bufferData );
    if ( !found )
    {
        DEBUGPRINT("IOUSBLib::LowLatencyDestroyBuffer:  Could not remove buffer (%p) from our list\n", buffer);
        result = kIOReturnBadArgument;
        goto ErrorExit;
    }
    
    // Call into the kernel to release the kernel objects for this buffer data
    //
    // kIOUCStructIStructO  io_connect_method_structureI_structureO(..., UInt32 * bufferIn,  UInt32 bufferSizeIn,   UInt32 * bufferOut,  UInt32 * bufferSizeInOut )
    //
    result = io_connect_method_structureI_structureO(fConnection, kUSBInterfaceUserClientLowLatencyReleaseBuffer, (char *)bufferData, sizeof(LowLatencyUserBufferInfo), NULL, &outSize);

    // If there is an error, we still need to free our data
    // Now, free the memory
    //
    ret = vm_deallocate( mach_task_self(), (vm_address_t) bufferData->bufferAddress, bufferData->bufferSize );
    if ( ret != kIOReturnSuccess )
    {
        DEBUGPRINT("IOUSBLib::LowLatencyDestroyBuffer:  Could not vm_deallocate buffer (%p) from our list (0x%x)\n", buffer, ret);
        result = kIOReturnBadArgument;
    }

    free ( bufferData );
    
    if ( result != kIOReturnSuccess )
    {
        DEBUGPRINT("IOUSBLib::LowLatencyDestroyBuffer:  Kernel call kUSBInterfaceUserClientLowLatencyReleaseBuffer returned 0x%x\n", result);
    }

ErrorExit:
    
    return result;
}

void
IOUSBInterfaceClass::AddDataBufferToList( LowLatencyUserBufferInfo * insertBuffer )
{
    LowLatencyUserBufferInfo *	buffer;
    
    // Traverse the list looking for last buffer and insert ours into it
    //
    if ( fUserBufferInfoListHead == NULL )
    {
        fUserBufferInfoListHead = insertBuffer;
        return;
    }
    
    buffer = fUserBufferInfoListHead;
    
    while ( buffer->nextBuffer != NULL )
    {
        buffer = buffer->nextBuffer;
    }
    
    // When we get here, nextBuffer is pointing to NULL.  Our insert buffer
    // already has nextBuffer = NULL, so we just insert it
    //
    buffer->nextBuffer = insertBuffer;
}


LowLatencyUserBufferInfo *
IOUSBInterfaceClass::FindBufferAddressInList( void *address )
{
    LowLatencyUserBufferInfo *	buffer;
    bool			foundIt = true;
    
    // Traverse the list looking for this buffer
    //
    if ( fUserBufferInfoListHead == NULL )
    {
        return NULL;
    }
    
    buffer = fUserBufferInfoListHead;
    
    // Now, we need to see if our address is the same as one in the buffer list
    //
    while ( buffer->bufferAddress != address )
    {
        buffer = buffer->nextBuffer;
        if ( buffer == NULL )
        {
            foundIt = false;
            break;
        }
    }
    
    if ( foundIt )
        return buffer;
    else
        return false;
}

LowLatencyUserBufferInfo *
IOUSBInterfaceClass::FindBufferAddressRangeInList( void * address, UInt32 size )
{
    // Need to find and see if this address range is within any of the buffers
    // in our buffer data list
    //
    LowLatencyUserBufferInfo *	buffer;
    UInt32			addressStart;
    UInt32			addressEnd;
    UInt32			bufferStart;
    UInt32			bufferEnd;
    bool			foundIt = false;
    
    // If no list, return NULL
    //
    if (fUserBufferInfoListHead == NULL)
        return NULL;
        
    // Convert pointers to integers
    //
    addressStart = (UInt32) address;
    addressEnd = addressStart + size;
    
    // Start at the beginning of the list
    //
    buffer = fUserBufferInfoListHead;
    
    do {
        // Calculate the bufferStart and bufferEnd for this buffer
        //
        bufferStart = (UInt32) buffer->bufferAddress;
        bufferEnd = bufferStart + buffer->bufferSize;
        
        // Now, is our address in that range and
        //
        if ( ( addressStart >= bufferStart) && ( addressStart < bufferEnd) )
        {
            // Yes, it is.  Now, does it fit
            //
            if ( addressEnd <= bufferEnd )
            {
                // And it fits, so we have our buffer
                //
                foundIt = true;
                break;
            }
        }
        
        // Look at the next buffer
        //
        buffer = buffer->nextBuffer;
    
    } while ( buffer != NULL );
    
    if ( foundIt )
        return buffer;
    else
        return NULL;
}


bool
IOUSBInterfaceClass::RemoveDataBufferFromList( LowLatencyUserBufferInfo * removeBuffer )
{
    LowLatencyUserBufferInfo *	buffer;
    LowLatencyUserBufferInfo *	previousBuffer;
    
    // If our head is NULL, then this buffer does not exist in our list
    //
    if ( fUserBufferInfoListHead == NULL )
    {
        return false;
    }
    
    buffer = fUserBufferInfoListHead;
    
    // if our removeBuffer is the first one in the list, then just update the head and
    // exit
    //
    if ( buffer == removeBuffer )
    {
        fUserBufferInfoListHead = buffer->nextBuffer;
    }
    else
    {    
        // Need to start previousBuffer pointing to our initial buffer, in case we match
        // the first time
        //
        previousBuffer = buffer;
        
        while ( buffer->nextBuffer != removeBuffer )
        {
            previousBuffer = buffer;
            buffer = previousBuffer->nextBuffer;
        }
        
        // When we get here, buffer is pointing to the same buffer as removeBuffer
        // and previous buffer is pointing to the previous element in the link list,
        // so, update the link in previous to point to removeBuffer->nextBuffer;
        //
        buffer->nextBuffer = removeBuffer->nextBuffer;
    }
    
    return true;
}


IOReturn
IOUSBInterfaceClass::GetBusMicroFrameNumber(UInt64 *microFrame, AbsoluteTime *atTime)
{
    IOUSBGetFrameStruct 	stuff;
    mach_msg_type_number_t 	outSize = sizeof(stuff);
    IOReturn 			ret;

    connectCheck();

    ret = io_connect_method_scalarI_structureO(fConnection, kUSBInterfaceUserClientGetMicroFrameNumber, NULL, 0, (char *)&stuff, &outSize);
    if(kIOReturnSuccess == ret)
    {
        *microFrame = stuff.frame;
        *atTime = stuff.timeStamp;
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
IOUSBInterfaceClass::GetFrameListTime(UInt32 *microsecondsInFrame)
{
    mach_msg_type_number_t 	outSize = 1;
    IOReturn 			ret;

    connectCheck();

    ret = io_connect_method_scalarI_scalarO(fConnection, kUSBInterfaceUserClientGetFrameListTime, NULL, 0, (int*)microsecondsInFrame, &outSize);
    if (ret == MACH_SEND_INVALID_DEST)
    {
        fIsOpen = false;
        fConnection = MACH_PORT_NULL;
        ret = kIOReturnNoDevice;
    }
    return ret;
}


IOReturn
IOUSBInterfaceClass::GetIOUSBLibVersion(NumVersion *ioUSBLibVersion, NumVersion *usbFamilyVersion)
{
    CFURLRef    bundleURL;
    CFBundleRef myBundle;
    UInt32  	usbFamilyBundleVersion;
    UInt32  	usbLibBundleVersion;
    UInt32 * 	tmp;
    
    connectCheck();

    // Make a CFURLRef from the CFString representation of the
    // bundle's path. See the Core Foundation URL Services chapter
    // for details.
    bundleURL = CFURLCreateWithFileSystemPath(
                                              kCFAllocatorDefault,
                                              CFSTR("/System/Library/Extensions/IOUSBFamily.kext"),
                                              kCFURLPOSIXPathStyle,
                                              true );

    // Make a bundle instance using the URLRef.
    myBundle = CFBundleCreate( kCFAllocatorDefault, bundleURL );

    // Look for the bundle's version number.
    usbFamilyBundleVersion = CFBundleGetVersionNumber( myBundle );

    // Any CF objects returned from functions with "create" or
    // "copy" in their names must be released by us!
    CFRelease( bundleURL );
    CFRelease( myBundle );

    // Make a CFURLRef from the CFString representation of the
    // bundle's path. See the Core Foundation URL Services chapter
    // for details.
    bundleURL = CFURLCreateWithFileSystemPath(
                                              kCFAllocatorDefault,
                                              CFSTR("/System/Library/Extensions/IOUSBFamily.kext/Contents/PlugIns/IOUSBLib.bundle"),
                                              kCFURLPOSIXPathStyle,
                                              true );

    // Make a bundle instance using the URLRef.
    myBundle = CFBundleCreate( kCFAllocatorDefault, bundleURL );

    // Look for the bundle's version number.
    usbLibBundleVersion = CFBundleGetVersionNumber( myBundle );

    // Any CF objects returned from functions with "create" or
    // "copy" in their names must be released by us!
    CFRelease( bundleURL );
    CFRelease( myBundle );

    // Cast the NumVersion to a UInt32 so we can just copy the data directly in.
    //
    if ( ioUSBLibVersion )
    {
        tmp = (UInt32 *) ioUSBLibVersion;
        *tmp = usbLibBundleVersion;
    }

    if ( usbFamilyVersion )
    {
        tmp = (UInt32 *) usbFamilyVersion;
        *tmp = usbFamilyBundleVersion;
    }
    
    return kIOReturnSuccess;

}



IOCFPlugInInterface 
IOUSBInterfaceClass::sIOCFPlugInInterfaceV1 = {
    0,
    &IOUSBIUnknown::genericQueryInterface,
    &IOUSBIUnknown::genericAddRef,
    &IOUSBIUnknown::genericRelease,
    1, 0,	// version/revision
    &IOUSBInterfaceClass::interfaceProbe,
    &IOUSBInterfaceClass::interfaceStart,
    &IOUSBInterfaceClass::interfaceStop
};


IOUSBInterfaceStruct197 
IOUSBInterfaceClass::sUSBInterfaceInterfaceV197 = {
    0,
    &IOUSBIUnknown::genericQueryInterface,
    &IOUSBIUnknown::genericAddRef,
    &IOUSBIUnknown::genericRelease,
    &IOUSBInterfaceClass::interfaceCreateInterfaceAsyncEventSource,
    &IOUSBInterfaceClass::interfaceGetInterfaceAsyncEventSource,
    &IOUSBInterfaceClass::interfaceCreateInterfaceAsyncPort,
    &IOUSBInterfaceClass::interfaceGetInterfaceAsyncPort,
    &IOUSBInterfaceClass::interfaceUSBInterfaceOpen,
    &IOUSBInterfaceClass::interfaceUSBInterfaceClose,
    &IOUSBInterfaceClass::interfaceGetInterfaceClass,
    &IOUSBInterfaceClass::interfaceGetInterfaceSubClass,
    &IOUSBInterfaceClass::interfaceGetInterfaceProtocol,
    &IOUSBInterfaceClass::interfaceGetDeviceVendor,
    &IOUSBInterfaceClass::interfaceGetDeviceProduct,
    &IOUSBInterfaceClass::interfaceGetDeviceReleaseNumber,
    &IOUSBInterfaceClass::interfaceGetConfigurationValue,
    &IOUSBInterfaceClass::interfaceGetInterfaceNumber,
    &IOUSBInterfaceClass::interfaceGetAlternateSetting,
    &IOUSBInterfaceClass::interfaceGetNumEndpoints,
    &IOUSBInterfaceClass::interfaceGetLocationID,
    &IOUSBInterfaceClass::interfaceGetDevice,
    &IOUSBInterfaceClass::interfaceSetAlternateInterface,
    &IOUSBInterfaceClass::interfaceGetBusFrameNumber,
    &IOUSBInterfaceClass::interfaceControlRequest,
    &IOUSBInterfaceClass::interfaceControlRequestAsync,
    &IOUSBInterfaceClass::interfaceGetPipeProperties,
    &IOUSBInterfaceClass::interfaceGetPipeStatus,
    &IOUSBInterfaceClass::interfaceAbortPipe,
    &IOUSBInterfaceClass::interfaceResetPipe,
    &IOUSBInterfaceClass::interfaceClearPipeStall,
    &IOUSBInterfaceClass::interfaceReadPipe,
    &IOUSBInterfaceClass::interfaceWritePipe,
    &IOUSBInterfaceClass::interfaceReadPipeAsync,
    &IOUSBInterfaceClass::interfaceWritePipeAsync,
    &IOUSBInterfaceClass::interfaceReadIsochPipeAsync,
    &IOUSBInterfaceClass::interfaceWriteIsochPipeAsync,
    // ---------- new with 1.8.2
    &IOUSBInterfaceClass::interfaceControlRequestTO,
    &IOUSBInterfaceClass::interfaceControlRequestAsyncTO,
    &IOUSBInterfaceClass::interfaceReadPipeTO,
    &IOUSBInterfaceClass::interfaceWritePipeTO,
    &IOUSBInterfaceClass::interfaceReadPipeAsyncTO,
    &IOUSBInterfaceClass::interfaceWritePipeAsyncTO,
    &IOUSBInterfaceClass::interfaceGetInterfaceStringIndex,
    // ---------- new with 1.8.3
    &IOUSBInterfaceClass::interfaceUSBInterfaceOpenSeize,
    // ---------- new with 1.9.0
    &IOUSBInterfaceClass::interfaceClearPipeStallBothEnds,
    &IOUSBInterfaceClass::interfaceSetPipePolicy,
    &IOUSBInterfaceClass::interfaceGetBandwidthAvailable,
    &IOUSBInterfaceClass::interfaceGetEndpointProperties,
    // ---------- new with 1.9.2
    &IOUSBInterfaceClass::interfaceLowLatencyReadIsochPipeAsync,
    &IOUSBInterfaceClass::interfaceLowLatencyWriteIsochPipeAsync,
    &IOUSBInterfaceClass::interfaceLowLatencyCreateBuffer,
    &IOUSBInterfaceClass::interfaceLowLatencyDestroyBuffer,
    // ---------- new with 1.9.7
    &IOUSBInterfaceClass::interfaceGetBusMicroFrameNumber,
    &IOUSBInterfaceClass::interfaceGetFrameListTime,
    &IOUSBInterfaceClass::interfaceGetIOUSBLibVersion
};


// Methods for routing iocfplugin interface
IOReturn 
IOUSBInterfaceClass:: interfaceProbe(void *self, CFDictionaryRef propertyTable, io_service_t inService, SInt32 *order)
    { return getThis(self)->probe(propertyTable, inService, order); }


IOReturn 
IOUSBInterfaceClass::interfaceStart(void *self, CFDictionaryRef propertyTable, io_service_t inService)
    { return getThis(self)->start(propertyTable, inService); }
    

IOReturn 
IOUSBInterfaceClass::interfaceStop(void *self)
    { return getThis(self)->USBInterfaceClose(); }

IOReturn 
IOUSBInterfaceClass::interfaceCreateInterfaceAsyncEventSource(void *self, CFRunLoopSourceRef *source)
    { return getThis(self)->CreateInterfaceAsyncEventSource(source); }

CFRunLoopSourceRef 
IOUSBInterfaceClass::interfaceGetInterfaceAsyncEventSource(void *self)
    { return getThis(self)->GetInterfaceAsyncEventSource(); }

IOReturn 
IOUSBInterfaceClass::interfaceCreateInterfaceAsyncPort(void *self, mach_port_t *port)
    { return getThis(self)->CreateInterfaceAsyncPort(port); }

mach_port_t 
IOUSBInterfaceClass::interfaceGetInterfaceAsyncPort(void *self)
    { return getThis(self)->GetInterfaceAsyncPort(); }

IOReturn 
IOUSBInterfaceClass::interfaceUSBInterfaceOpen(void *self)
    { return getThis(self)->USBInterfaceOpen(false); }

IOReturn 
IOUSBInterfaceClass::interfaceUSBInterfaceClose(void *self)
    { return getThis(self)->USBInterfaceClose(); }

IOReturn
IOUSBInterfaceClass::interfaceGetInterfaceClass(void *self, UInt8 *devClass)
    { return getThis(self)->GetInterfaceClass(devClass); }

IOReturn
IOUSBInterfaceClass::interfaceGetInterfaceSubClass(void *self, UInt8 *devSubClass)
    { return getThis(self)->GetInterfaceSubClass(devSubClass); }

IOReturn
IOUSBInterfaceClass::interfaceGetInterfaceProtocol(void *self, UInt8 *devProtocol)
    { return getThis(self)->GetInterfaceProtocol(devProtocol); }

IOReturn
IOUSBInterfaceClass::interfaceGetDeviceVendor(void *self, UInt16 *devVendor)
    { return getThis(self)->GetDeviceVendor(devVendor); }

IOReturn
IOUSBInterfaceClass::interfaceGetDeviceProduct(void *self, UInt16 *devProduct)
    { return getThis(self)->GetDeviceProduct(devProduct); }

IOReturn
IOUSBInterfaceClass::interfaceGetDeviceReleaseNumber(void *self, UInt16 *devRelNum)
    { return getThis(self)->GetDeviceReleaseNumber(devRelNum); }
    
IOReturn
IOUSBInterfaceClass::interfaceGetConfigurationValue(void *self, UInt8 *configVal)
    { return getThis(self)->GetConfigurationValue(configVal); }
    
IOReturn
IOUSBInterfaceClass::interfaceGetInterfaceNumber(void *self, UInt8 *intfNumber)
    { return getThis(self)->GetInterfaceNumber(intfNumber); }

IOReturn
IOUSBInterfaceClass::interfaceGetAlternateSetting(void *self, UInt8 *intfAlternateSetting)
    { return getThis(self)->GetAlternateSetting(intfAlternateSetting); }

IOReturn
IOUSBInterfaceClass::interfaceGetNumEndpoints(void *self, UInt8 *intfNumEndpoints)
    { return getThis(self)->GetNumEndpoints(intfNumEndpoints); }

IOReturn
IOUSBInterfaceClass::interfaceGetLocationID(void *self, UInt32 *locationID)
    { return getThis(self)->GetLocationID(locationID); }

IOReturn
IOUSBInterfaceClass::interfaceGetDevice(void *self, io_service_t *device)
    { return getThis(self)->GetDevice(device); }

IOReturn
IOUSBInterfaceClass::interfaceSetAlternateInterface(void *self, UInt8 alternateSetting)
    { return getThis(self)->SetAlternateInterface(alternateSetting); }

IOReturn
IOUSBInterfaceClass::interfaceGetBusFrameNumber(void *self, UInt64 *frame, AbsoluteTime *atTime)
    { return getThis(self)->GetBusFrameNumber(frame, atTime); }

IOReturn
IOUSBInterfaceClass::interfaceControlRequest(void *self, UInt8 pipeRef, IOUSBDevRequest *reqIn)
{ 
    IOUSBDevRequestTO		req;
    IOReturn			err;
    
    req.bmRequestType = reqIn->bmRequestType;
    req.bRequest = reqIn->bRequest;
    req.wValue = reqIn->wValue;
    req.wIndex = reqIn->wIndex;
    req.wLength = reqIn->wLength;
    req.pData = reqIn->pData;
    req.wLenDone = reqIn->wLenDone;
    req.completionTimeout = pipeRef ? 0 : kUSBDefaultControlCompletionTimeoutMS;
    req.noDataTimeout = pipeRef ? 0 : kUSBDefaultControlNoDataTimeoutMS;
    
    err =  getThis(self)->ControlRequest(pipeRef, &req);
    reqIn->wLenDone = req.wLenDone;
    return err;
}

IOReturn
IOUSBInterfaceClass::interfaceControlRequestAsync(void *self, UInt8 pipeRef, IOUSBDevRequest *reqIn, IOAsyncCallback1 callback, void *refCon)
{ 
    IOUSBDevRequestTO	req;
    
    req.bmRequestType = reqIn->bmRequestType;
    req.bRequest = reqIn->bRequest;
    req.wValue = reqIn->wValue;
    req.wIndex = reqIn->wIndex;
    req.wLength = reqIn->wLength;
    req.pData = reqIn->pData;
    req.wLenDone = reqIn->wLenDone;
    req.completionTimeout = pipeRef ? 0 : kUSBDefaultControlCompletionTimeoutMS;
    req.noDataTimeout = pipeRef ? 0 : kUSBDefaultControlNoDataTimeoutMS;
    
    return getThis(self)->ControlRequestAsync(pipeRef, &req, callback, refCon); 
}

IOReturn 
IOUSBInterfaceClass::interfaceGetPipeProperties(void *self, UInt8 pipeRef, UInt8 *direction, UInt8 *address, UInt8 *attributes, 
                                    UInt16 *maxpacketSize, UInt8 *interval)
    { return getThis(self)->GetPipeProperties(pipeRef, direction, address, attributes, maxpacketSize, interval); }

IOReturn
IOUSBInterfaceClass::interfaceGetPipeStatus(void *self, UInt8 pipeRef)
    { return getThis(self)->GetPipeStatus(pipeRef); }

IOReturn
IOUSBInterfaceClass::interfaceAbortPipe(void *self, UInt8 pipeRef)
    { return getThis(self)->AbortPipe(pipeRef); }

IOReturn
IOUSBInterfaceClass::interfaceResetPipe(void *self, UInt8 pipeRef)
    { return getThis(self)->ResetPipe(pipeRef); }

IOReturn
IOUSBInterfaceClass::interfaceClearPipeStall(void *self, UInt8 pipeRef)
    { return getThis(self)->ClearPipeStall(pipeRef, false); }

IOReturn
IOUSBInterfaceClass::interfaceReadPipe(void *self, UInt8 pipeRef, void *buf, UInt32 *size)
    { return getThis(self)->ReadPipe(pipeRef, buf, size, 0, 0); }

IOReturn
IOUSBInterfaceClass::interfaceWritePipe(void *self, UInt8 pipeRef, void *buf, UInt32 size)
    { return getThis(self)->WritePipe(pipeRef, buf, size, 0, 0); }

IOReturn
IOUSBInterfaceClass::interfaceReadPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt32 size, IOAsyncCallback1 callback, void *refcon)
    { return getThis(self)->ReadPipeAsync(pipeRef, buf, size, 0, 0, callback, refcon); }

IOReturn 
IOUSBInterfaceClass::interfaceWritePipeAsync(void *self, UInt8 pipeRef, void *buf, UInt32 size, IOAsyncCallback1 callback, void *refcon)
    { return getThis(self)->WritePipeAsync(pipeRef, buf, size, 0, 0, callback, refcon); }

IOReturn 
IOUSBInterfaceClass::interfaceReadIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refcon)
    { return getThis(self)->ReadIsochPipeAsync(pipeRef, buf, frameStart, numFrames, frameList, callback, refcon); }

IOReturn 
IOUSBInterfaceClass::interfaceWriteIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refcon)
    { return getThis(self)->WriteIsochPipeAsync(pipeRef, buf, frameStart, numFrames, frameList, callback, refcon); }
//--------------- added in 1.8.2
IOReturn
IOUSBInterfaceClass::interfaceControlRequestTO(void *self, UInt8 pipeRef, IOUSBDevRequestTO *req)
    { return getThis(self)->ControlRequest(pipeRef, req); }

IOReturn
IOUSBInterfaceClass::interfaceControlRequestAsyncTO(void *self, UInt8 pipeRef, IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon)
    { return getThis(self)->ControlRequestAsync(pipeRef, req, callback, refCon); }

IOReturn
IOUSBInterfaceClass::interfaceReadPipeTO(void *self, UInt8 pipeRef, void *buf, UInt32 *size, UInt32 noDataTimeout, UInt32 completionTimeout)
    { return getThis(self)->ReadPipe(pipeRef, buf, size, noDataTimeout, completionTimeout); }

IOReturn
IOUSBInterfaceClass::interfaceWritePipeTO(void *self, UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout)
    { return getThis(self)->WritePipe(pipeRef, buf, size, noDataTimeout, completionTimeout); }

IOReturn
IOUSBInterfaceClass::interfaceReadPipeAsyncTO(void *self, UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon)
    { return getThis(self)->ReadPipeAsync(pipeRef, buf, size, noDataTimeout, completionTimeout, callback, refcon); }

IOReturn 
IOUSBInterfaceClass::interfaceWritePipeAsyncTO(void *self, UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon)
    { return getThis(self)->WritePipeAsync(pipeRef, buf, size, noDataTimeout, completionTimeout, callback, refcon); }

IOReturn
IOUSBInterfaceClass::interfaceGetInterfaceStringIndex(void *self, UInt8 *intfSI)
    { return getThis(self)->GetInterfaceStringIndex(intfSI); }

IOReturn 
IOUSBInterfaceClass::interfaceUSBInterfaceOpenSeize(void *self)
    { return getThis(self)->USBInterfaceOpen(true); }

IOReturn 
IOUSBInterfaceClass::interfaceClearPipeStallBothEnds(void *self, UInt8 pipeRef)
    { return getThis(self)->ClearPipeStall(pipeRef, true); }

IOReturn 
IOUSBInterfaceClass::interfaceSetPipePolicy(void *self, UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval)
    { return getThis(self)->SetPipePolicy(pipeRef, maxPacketSize, maxInterval); }

IOReturn 
IOUSBInterfaceClass::interfaceGetBandwidthAvailable(void *self, UInt32 *bandwidth)
    { return getThis(self)->GetBandwidthAvailable(bandwidth); }

IOReturn
IOUSBInterfaceClass::interfaceGetEndpointProperties(void *self, UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval)
    { return getThis(self)->GetEndpointProperties(alternateSetting, endpointNumber, direction, transferType, maxPacketSize, interval); }

//--------------- added in 1.9.2
IOReturn 
IOUSBInterfaceClass::interfaceLowLatencyReadIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon)
    { return getThis(self)->LowLatencyReadIsochPipeAsync(pipeRef, buf, frameStart, numFrames, updateFrequency, frameList, callback, refcon); }

IOReturn 
IOUSBInterfaceClass::interfaceLowLatencyWriteIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon)
    { return getThis(self)->LowLatencyWriteIsochPipeAsync(pipeRef, buf, frameStart, numFrames, updateFrequency, frameList, callback, refcon); }

IOReturn 
IOUSBInterfaceClass::interfaceLowLatencyCreateBuffer(void *self, void * *buffer, IOByteCount size, UInt32 bufferType)
    { return getThis(self)->LowLatencyCreateBuffer( buffer, size, bufferType); }
    
IOReturn 
IOUSBInterfaceClass::interfaceLowLatencyDestroyBuffer(void *self, void * buffer )
    { return getThis(self)->LowLatencyDestroyBuffer( buffer); }

//--------------- added in 1.9.7
IOReturn
IOUSBInterfaceClass::interfaceGetBusMicroFrameNumber(void *self, UInt64 *microFrame, AbsoluteTime *atTime)
{ return getThis(self)->GetBusMicroFrameNumber(microFrame, atTime); }

IOReturn
IOUSBInterfaceClass::interfaceGetFrameListTime(void *self, UInt32 *microsecondsInFrame)
{ return getThis(self)->GetFrameListTime(microsecondsInFrame); }

IOReturn
IOUSBInterfaceClass::interfaceGetIOUSBLibVersion( void *self, NumVersion *ioUSBLibVersion, NumVersion *usbFamilyVersion)
{ return getThis(self)->GetIOUSBLibVersion(ioUSBLibVersion, usbFamilyVersion); }

