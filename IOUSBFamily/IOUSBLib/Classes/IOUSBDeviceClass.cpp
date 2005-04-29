/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CoreFoundation.h>

#include "IOUSBDeviceClass.h"
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

IOCFPlugInInterface ** 
IOUSBDeviceClass::alloc()
{
    IOUSBDeviceClass *me;

    me = new IOUSBDeviceClass;
    if (me)
        return (IOCFPlugInInterface **) &me->iunknown.pseudoVTable;
    else
        return 0;
}

IOUSBDeviceClass::IOUSBDeviceClass()
: IOUSBIUnknown(&sIOCFPlugInInterfaceV1),
  fService(MACH_PORT_NULL),
  fConnection(MACH_PORT_NULL),
  fAsyncPort(MACH_PORT_NULL),
  fCFSource(0),
  fIsOpen(false),
  fConfigurations(NULL)
{
    fUSBDevice.pseudoVTable = (IUnknownVTbl *)  &sUSBDeviceInterfaceV197;
    fUSBDevice.obj = this;
}

IOUSBDeviceClass::~IOUSBDeviceClass()
{
    if (fConfigurations)
    {
        // release the config descriptor data
        int i;
        for (i=0; i< fNumConfigurations; i++)
            if (fConfigurations[i])
                free(fConfigurations[i]);
        free(fConfigurations);
        fConfigurations = NULL;
    }
    
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
IOUSBDeviceClass::queryInterface(REFIID iid, void **ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOCFPlugInInterfaceID)) 
    {
        *ppv = &iunknown;
        addRef();
    }
    else if (CFEqual(uuid, kIOUSBDeviceInterfaceID) || CFEqual(uuid, kIOUSBDeviceInterfaceID182) ||
             CFEqual(uuid, kIOUSBDeviceInterfaceID187) || CFEqual(uuid, kIOUSBDeviceInterfaceID197) ) 
    {
        *ppv = &fUSBDevice;
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
IOUSBDeviceClass::probe(CFDictionaryRef propertyTable, io_service_t inService, SInt32 *order)
{
    if (!inService || !IOObjectConformsTo(inService, "IOUSBDevice"))
        return kIOReturnBadArgument;

    return kIOReturnSuccess;
}



IOReturn 
IOUSBDeviceClass::start(CFDictionaryRef propertyTable, io_service_t inService)
{
    IOReturn 			res;
    CFMutableDictionaryRef 	entryProperties = 0;
    kern_return_t 		kr;
    
    DEBUGPRINT("IOUSBDeviceClass::start\n");
    fService = inService;
    res = IOServiceOpen(fService, mach_task_self(), 0, &fConnection);
    if (res != kIOReturnSuccess)
        return res;

    connectCheck();

    kr = IORegistryEntryCreateCFProperties(fService, &entryProperties, NULL, 0);
    if (entryProperties) 
    {
        CFTypeRef val;
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDeviceClass));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fClass);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDeviceSubClass));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fSubClass);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDeviceProtocol));
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
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBManufacturerStringIndex));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fManufacturerStringIndex);
        else
            fManufacturerStringIndex = 0;
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBProductStringIndex));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fProductStringIndex);
        else
            fProductStringIndex = 0;
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBSerialNumberStringIndex));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fSerialNumberStringIndex);
        else
            fSerialNumberStringIndex = 0;
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDeviceNumConfigs));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fNumConfigurations);
        else
            fNumConfigurations = 0;
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDevicePropertySpeed));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberCharType, (void*)&fSpeed);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDevicePropertyBusPowerAvailable));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongType, (void*)&fPowerAvail);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDevicePropertyAddress));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberShortType, (void*)&fAddress);
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDevicePropertyLocationID));
        if (val)
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongType, (void*)&fLocationID);

        fConfigDescCacheValid = false;
        if (fNumConfigurations)
        {
            fConfigurations = (IOUSBConfigurationDescriptorPtr*) malloc(fNumConfigurations * sizeof(IOUSBConfigurationDescriptorPtr));
            bzero(fConfigurations, fNumConfigurations * sizeof(IOUSBConfigurationDescriptorPtr));
            kr = CacheConfigDescriptor();
        }
        CFRelease(entryProperties);
    }


    return kIOReturnSuccess;
}



IOReturn 
IOUSBDeviceClass::stop()
{
	IOReturn ret = kIOReturnSuccess;
	
	connectCheck();
	if (fIsOpen)
		ret = USBDeviceClose();
	
	return ret;
}
	


IOReturn
IOUSBDeviceClass::CacheConfigDescriptor()
{
    int 	i;
    IOReturn	kr = kIOReturnSuccess;
    
    for (i = 0; i < fNumConfigurations; i++)
    {
        IOUSBConfigurationDescriptorPtr	config;
        IOUSBConfigurationDescHeader	configHdr;
        mach_msg_type_number_t		size;

        size = sizeof(configHdr);
        kr = io_connect_method_scalarI_structureO(fConnection, kUSBDeviceUserClientGetConfigDescriptor, &i, 1, (char *)&configHdr, &size);
        if (kr)
            break;
        size = USBToHostWord(configHdr.wTotalLength);
        config = (IOUSBConfigurationDescriptorPtr)malloc(size+2);
        kr = io_connect_method_scalarI_structureO(fConnection, kUSBDeviceUserClientGetConfigDescriptor, &i, 1, (char *)config, &size);
        if (kr)
            break;
        
        // Add a dummy empty descriptor on the end
        *((char*)config + size) = 0;
        *((char*)config + size + 1) = 0;
        fConfigurations[i] = config;
    }

    if ( kr == kIOReturnSuccess )
       fConfigDescCacheValid = TRUE;
        
    return kr;
}

IOReturn 
IOUSBDeviceClass::GetDeviceClass(UInt8 *devClass)
{
    connectCheck();
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceClass\n");
    *devClass = fClass;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceSubClass(UInt8 *devSubClass)
{
    connectCheck();
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceSubClass\n");
    *devSubClass = fSubClass;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceProtocol(UInt8 *devProtocol)
{
    connectCheck();
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceProtocol\n");
    *devProtocol = fProtocol;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceVendor(UInt16 *devVendor)
{
    connectCheck();
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceVendor\n");
    *devVendor = fVendor;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceProduct(UInt16 *devProduct)
{
    connectCheck();
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceProduct\n");
    *devProduct = fProduct;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceReleaseNumber(UInt16 *devRelNum)
{
    connectCheck();
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceReleaseNumber\n");
    *devRelNum = fDeviceReleaseNumber;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceAddress(USBDeviceAddress *addr)
{
    connectCheck();
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceClass\n");
    *addr = fAddress;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceBusPowerAvailable(UInt32 *powerAvail)
{
    connectCheck();
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceClass\n");
    *powerAvail = fPowerAvail;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceSpeed(UInt8 *devSpeed)
{
    connectCheck();
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceSpeed\n");
    *devSpeed = fSpeed;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetNumberOfConfigurations(UInt8 *numConfig)
{
    connectCheck();
    DEBUGPRINT("IOUSBDeviceClass::GetNumberOfConfigurations\n");
    *numConfig = fNumConfigurations;
    return kIOReturnSuccess;
}


IOReturn
IOUSBDeviceClass::GetLocationID(UInt32 *locationID)
{ 
    connectCheck();
    DEBUGPRINT("IOUSBDeviceClass::GetLocationID\n");
    *locationID = fLocationID;
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetConfigurationDescriptorPtr(UInt8 index, IOUSBConfigurationDescriptorPtr *desc)
{
    IOReturn	kr = kIOReturnSuccess;
    
    connectCheck();
    
    DEBUGPRINT("IOUSBDeviceClass::GetConfigurationDescriptorPtr\n");
    if (index >= fNumConfigurations)
        return kIOUSBConfigNotFound;
	
    if ( !fConfigDescCacheValid )
    {
        printf("IOUSBDeviceClass::GetConfigurationDescriptorPtr cache was INVALID\n");
        kr = CacheConfigDescriptor();
    }
        
    *desc = fConfigurations[index];
    
    return kr;
}


IOReturn
IOUSBDeviceClass::SetConfiguration(UInt8 configNum)
{
    mach_msg_type_number_t	len = 0;
    int				t = configNum;
    IOReturn			ret;

    allChecks();
    ret = io_connect_method_scalarI_scalarO(fConnection, kUSBDeviceUserClientSetConfig, &t, 1, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fConnection = MACH_PORT_NULL;
		ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBDeviceClass::GetConfiguration(UInt8 *config)
{
    mach_msg_type_number_t	len = 1;
    IOReturn			ret;
    int				result;

    connectCheck();
    ret = io_connect_method_scalarI_scalarO(fConnection, kUSBDeviceUserClientGetConfig, NULL, 0, &result, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fConnection = MACH_PORT_NULL;
		ret = kIOReturnNoDevice;
    }
    if (ret == kIOReturnSuccess)
        *config = *(UInt8*)&result;
    return ret;
}



IOReturn IOUSBDeviceClass::
CreateDeviceAsyncEventSource(CFRunLoopSourceRef *source)
{
    IOReturn 		ret;
    CFMachPortRef 	cfPort;
    CFMachPortContext 	context;
    Boolean 		shouldFreeInfo;

    if (!fAsyncPort) 
    {     
        ret = CreateDeviceAsyncPort(0);
        if (kIOReturnSuccess != ret)
            return ret;
    }

    context.version = 1;
    context.info = this;
    context.retain = NULL;
    context.release = NULL;
    context.copyDescription = NULL;

    cfPort = CFMachPortCreateWithPort(NULL, fAsyncPort, (CFMachPortCallBack) IODispatchCalloutFromMessage, &context, &shouldFreeInfo);
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
IOUSBDeviceClass::GetDeviceAsyncEventSource()
{
    return fCFSource;
}



IOReturn 
IOUSBDeviceClass::CreateDeviceAsyncPort(mach_port_t *port)
{
    IOReturn ret;

    ret = IOCreateReceivePort(kOSAsyncCompleteMessageID, &fAsyncPort);
    if (kIOReturnSuccess == ret) 
    {
        if (port)
            *port = fAsyncPort;

        if (fIsOpen) 
        {
            natural_t 			asyncRef[1];
            mach_msg_type_number_t 	len = 0;
        
            // async kIOCDBUserClientSetAsyncPort,  kIOUCScalarIScalarO,    0,	0
            ret = io_async_method_scalarI_scalarO( fConnection, fAsyncPort, asyncRef, 1,  kUSBDeviceUserClientSetAsyncPort, NULL, 0, NULL, &len);
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
IOUSBDeviceClass::GetDeviceAsyncPort()
{
    return fAsyncPort;
}



IOReturn 
IOUSBDeviceClass::USBDeviceOpen(bool seize)
{
    IOReturn 		ret;
    int			t = seize;

    connectCheck();

    if (fIsOpen)
        return kIOReturnSuccess;

    mach_msg_type_number_t len = 0;

    //  kIOCDBUserClientOpen,  kIOUCScalarIScalarO,    1,	0
    ret = io_connect_method_scalarI_scalarO(fConnection, kUSBDeviceUserClientOpen, &t, 1, NULL, &len);

    if (ret == kIOReturnSuccess)
    {
		fIsOpen = true;
		
		if (fAsyncPort) 
		{
			natural_t asyncRef[1];
			mach_msg_type_number_t len = 0;
		
			// async 
			// kIOCDBUserClientSetAsyncPort,  kIOUCScalarIScalarO,    0,	0
			ret = io_async_method_scalarI_scalarO(fConnection, fAsyncPort, asyncRef, 1, kUSBDeviceUserClientSetAsyncPort, NULL, 0, NULL, &len);
			if ((ret != kIOReturnSuccess) && (ret != MACH_SEND_INVALID_DEST))
				USBDeviceClose();
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
IOUSBDeviceClass::USBDeviceClose()
{   
    IOReturn	ret;
    allChecks();

    mach_msg_type_number_t len = 0;
    fIsOpen = false;

    // kIOCDBUserClientClose,	kIOUCScalarIScalarO,	 0,  0
    ret =  io_connect_method_scalarI_scalarO(fConnection, kUSBDeviceUserClientClose, NULL, 0, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fConnection = MACH_PORT_NULL;
		ret = kIOReturnNoDevice;
    }
    return ret;
}


IOReturn 
IOUSBDeviceClass::GetBusFrameNumber(UInt64 *frame, AbsoluteTime *atTime)
{
    IOUSBGetFrameStruct 	stuff;
    mach_msg_type_number_t 	outSize = sizeof(stuff);
    IOReturn 			ret;

    connectCheck();

    ret = io_connect_method_scalarI_structureO(fConnection, kUSBDeviceUserClientGetFrameNumber, NULL, 0, (char *)&stuff, &outSize);
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
IOUSBDeviceClass::ResetDevice()
{
    mach_msg_type_number_t	len = 0;
    IOReturn			ret;

    allChecks();
    ret = io_connect_method_scalarI_scalarO(fConnection, kUSBDeviceUserClientResetDevice, NULL, 0, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fConnection = MACH_PORT_NULL;
		ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBDeviceClass::USBDeviceReEnumerate(UInt32 options)
{
    mach_msg_type_number_t	len = 0;
    int				t = options;
    IOReturn			ret;

    allChecks();
    ret = io_connect_method_scalarI_scalarO(fConnection, kUSBDeviceUserClientReEnumerateDevice, &t, 1, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fConnection = MACH_PORT_NULL;
		ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn 
IOUSBDeviceClass::DeviceRequest(IOUSBDevRequestTO *req)
{
    IOReturn 		ret = kIOReturnSuccess;
    
    connectCheck();

    if ( req->wLength <= sizeof(io_struct_inband_t))
    {
        // the buffer can be copied directly
        int	in[4];

        in[0] = (req->bmRequestType << 8) | req->bRequest;
        in[1] = (req->wValue << 16) | req->wIndex;
        in[2] = req->noDataTimeout;
		in[3] = req->completionTimeout;
	
        switch ((req->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask)
        {
            case kUSBOut:
                ret =  io_connect_method_scalarI_structureI(fConnection, kUSBDeviceUserClientDeviceRequestOut, in, 4, (char *)req->pData, req->wLength);
                if(kIOReturnSuccess == ret)
                    req->wLenDone = req->wLength;
                else
                    req->wLenDone = 0;
                break;
                
            case kUSBIn:
                mach_msg_type_number_t 	reqSize = req->wLength;
                ret = io_connect_method_scalarI_structureO(fConnection, kUSBDeviceUserClientDeviceRequestIn, in, 4, (char *)req->pData, &reqSize);
                if(kIOReturnSuccess == ret)
                    req->wLenDone = reqSize;
                break;
        }
    }
    else
    {
    // too much data to push through the entire buffer directly. memory must be mapped, so just send the regular structure
        mach_msg_type_number_t 	outSize = 0;
        switch ((req->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask)
        {
            case kUSBOut:
                ret = io_connect_method_structureI_structureO(fConnection, kUSBDeviceUserClientDeviceRequestOutOOL, 
                                (char *)req, sizeof(IOUSBDevRequestTO), NULL, &outSize);
                if(kIOReturnSuccess == ret)
                    req->wLenDone = req->wLength;
                else
                    req->wLenDone = 0;
                break;
                
            case kUSBIn:
                mach_msg_type_number_t 	reqSize = req->wLength;
                outSize = sizeof(reqSize);
                ret = io_connect_method_structureI_structureO(fConnection, kUSBDeviceUserClientDeviceRequestInOOL, 
                                (char *)&req, sizeof(IOUSBDevRequestTO), (char *)&reqSize, &outSize);
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
IOUSBDeviceClass::DeviceRequestAsync(IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon)
{
    connectCheck();
    mach_msg_type_number_t 	outSize = 0;
    natural_t			asyncRef[kIOAsyncCalloutCount];
    int				selector = 0;
    IOReturn			ret;
    
    connectCheck();

    if (!fAsyncPort)
        return kIOUSBNoAsyncPortErr;

    asyncRef[kIOAsyncCalloutFuncIndex] = (natural_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (natural_t) refCon;

    switch ((req->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask)
    {
        case kUSBOut:
            selector = kUSBDeviceUserClientDeviceAsyncRequestOut;
            break;
            
        case kUSBIn:
            selector = kUSBDeviceUserClientDeviceAsyncRequestIn;
            break;
    }
    ret = io_async_method_structureI_structureO(fConnection, fAsyncPort, asyncRef, kIOAsyncCalloutCount, 
                                                selector, (char*)req, sizeof(IOUSBDevRequestTO), NULL, &outSize);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fConnection = MACH_PORT_NULL;
		ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn 
IOUSBDeviceClass::CreateInterfaceIterator(IOUSBFindInterfaceRequest *intfReq, io_iterator_t *iter)
{
    mach_msg_type_number_t 	outSize = sizeof(io_iterator_t);
    IOReturn			ret;

    connectCheck();
    ret = io_connect_method_structureI_structureO(fConnection, kUSBDeviceUserClientCreateInterfaceIterator, 
                                (char *)intfReq, sizeof(IOUSBFindInterfaceRequest), (char*)iter, &outSize);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fConnection = MACH_PORT_NULL;
		ret = kIOReturnNoDevice;
    }
    return ret;
}


IOReturn
IOUSBDeviceClass::USBDeviceSuspend(bool suspend)
{
    mach_msg_type_number_t	len = 0;
    int				t = suspend;
    IOReturn			ret;

    allChecks();
    ret = io_connect_method_scalarI_scalarO(fConnection, kUSBDeviceUserClientSuspend, &t, 1, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fConnection = MACH_PORT_NULL;
		ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBDeviceClass::USBDeviceAbortPipeZero(void)
{
    mach_msg_type_number_t	len = 0;
    IOReturn			ret;

    allChecks();
    
    return io_connect_method_scalarI_scalarO(fConnection, kUSBDeviceUserClientAbortPipeZero, NULL, 0, NULL, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fConnection = MACH_PORT_NULL;
		ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBDeviceClass::USBDeviceGetManufacturerStringIndex(UInt8 *msi)
{
    connectCheck();
    *msi = fManufacturerStringIndex;
    return kIOReturnSuccess;
}


IOReturn
IOUSBDeviceClass::USBDeviceGetProductStringIndex(UInt8 *psi)
{
    connectCheck();
    *psi = fProductStringIndex;
    return kIOReturnSuccess;
}


IOReturn
IOUSBDeviceClass::USBDeviceGetSerialNumberStringIndex(UInt8 *snsi)
{
    connectCheck();
    *snsi = fSerialNumberStringIndex;
    return kIOReturnSuccess;
}


IOReturn
IOUSBDeviceClass::GetBusMicroFrameNumber(UInt64 *microFrame, AbsoluteTime *atTime)
{
    IOUSBGetFrameStruct 	stuff;
    mach_msg_type_number_t 	outSize = sizeof(stuff);
    IOReturn 			ret;

    connectCheck();

    ret = io_connect_method_scalarI_structureO(fConnection, kUSBDeviceUserClientGetMicroFrameNumber, NULL, 0, (char *)&stuff, &outSize);
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
IOUSBDeviceClass::GetIOUSBLibVersion(NumVersion *ioUSBLibVersion, NumVersion *usbFamilyVersion)
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
IOUSBDeviceClass::sIOCFPlugInInterfaceV1 = {
    0,
    &IOUSBIUnknown::genericQueryInterface,
    &IOUSBIUnknown::genericAddRef,
    &IOUSBIUnknown::genericRelease,
    1, 0,	// version/revision
    &IOUSBDeviceClass::deviceProbe,
    &IOUSBDeviceClass::deviceStart,
    &IOUSBDeviceClass::deviceStop
};


IOUSBDeviceStruct197 
IOUSBDeviceClass::sUSBDeviceInterfaceV197 = {
    0,
    &IOUSBIUnknown::genericQueryInterface,
    &IOUSBIUnknown::genericAddRef,
    &IOUSBIUnknown::genericRelease,
    &IOUSBDeviceClass::deviceCreateDeviceAsyncEventSource,
    &IOUSBDeviceClass::deviceGetDeviceAsyncEventSource,
    &IOUSBDeviceClass::deviceCreateDeviceAsyncPort,
    &IOUSBDeviceClass::deviceGetDeviceAsyncPort,
    &IOUSBDeviceClass::deviceUSBDeviceOpen,
    &IOUSBDeviceClass::deviceUSBDeviceClose,
    &IOUSBDeviceClass::deviceGetDeviceClass,
    &IOUSBDeviceClass::deviceGetDeviceSubClass,
    &IOUSBDeviceClass::deviceGetDeviceProtocol,
    &IOUSBDeviceClass::deviceGetDeviceVendor,
    &IOUSBDeviceClass::deviceGetDeviceProduct,
    &IOUSBDeviceClass::deviceGetDeviceReleaseNumber,
    &IOUSBDeviceClass::deviceGetDeviceAddress,
    &IOUSBDeviceClass::deviceGetDeviceBusPowerAvailable,
    &IOUSBDeviceClass::deviceGetDeviceSpeed,
    &IOUSBDeviceClass::deviceGetNumberOfConfigurations,
    &IOUSBDeviceClass::deviceGetLocationID,
    &IOUSBDeviceClass::deviceGetConfigurationDescriptorPtr,
    &IOUSBDeviceClass::deviceGetConfiguration,
    &IOUSBDeviceClass::deviceSetConfiguration,
    &IOUSBDeviceClass::deviceGetBusFrameNumber,
    &IOUSBDeviceClass::deviceResetDevice,
    &IOUSBDeviceClass::deviceDeviceRequest,
    &IOUSBDeviceClass::deviceDeviceRequestAsync,
    &IOUSBDeviceClass::deviceCreateInterfaceIterator,
    // ----------new with 1.8.2
    &IOUSBDeviceClass::deviceUSBDeviceOpenSeize,
    &IOUSBDeviceClass::deviceDeviceRequestTO,
    &IOUSBDeviceClass::deviceDeviceRequestAsyncTO,
    &IOUSBDeviceClass::deviceUSBDeviceSuspend,
    &IOUSBDeviceClass::deviceUSBDeviceAbortPipeZero,
    &IOUSBDeviceClass::deviceGetManufacturerStringIndex,
    &IOUSBDeviceClass::deviceGetProductStringIndex,
    &IOUSBDeviceClass::deviceGetSerialNumberStringIndex,
    // ----------new with 1.8.7
    &IOUSBDeviceClass::deviceReEnumerateDevice,
    // ----------new with 1.9.7
    &IOUSBDeviceClass::deviceGetBusMicroFrameNumber,
    &IOUSBDeviceClass::deviceGetIOUSBLibVersion
};


// Methods for routing iocfplugin interface
IOReturn 
IOUSBDeviceClass:: deviceProbe(void *self, CFDictionaryRef propertyTable, io_service_t inService, SInt32 *order)
    { return getThis(self)->probe(propertyTable, inService, order); }


IOReturn 
IOUSBDeviceClass::deviceStart(void *self, CFDictionaryRef propertyTable, io_service_t inService)
    { return getThis(self)->start(propertyTable, inService); }
    

IOReturn 
IOUSBDeviceClass::deviceStop(void *self)
	{ return getThis(self)->stop(); }


// Methods for routing asynchronous completion plumbing.
IOReturn 
IOUSBDeviceClass::deviceCreateDeviceAsyncEventSource(void *self, CFRunLoopSourceRef *source)
    { return getThis(self)->CreateDeviceAsyncEventSource(source); }

CFRunLoopSourceRef 
IOUSBDeviceClass::deviceGetDeviceAsyncEventSource(void *self)
    { return getThis(self)->GetDeviceAsyncEventSource(); }

IOReturn 
IOUSBDeviceClass::deviceCreateDeviceAsyncPort(void *self, mach_port_t *port)
    { return getThis(self)->CreateDeviceAsyncPort(port); }

mach_port_t 
IOUSBDeviceClass::deviceGetDeviceAsyncPort(void *self)
    { return getThis(self)->GetDeviceAsyncPort(); }

IOReturn
IOUSBDeviceClass::deviceGetDeviceClass(void *self, UInt8 *devClass)
    { return getThis(self)->GetDeviceClass(devClass); }

IOReturn
IOUSBDeviceClass::deviceGetDeviceSubClass(void *self, UInt8 *devSubClass)
    { return getThis(self)->GetDeviceSubClass(devSubClass); }

IOReturn
IOUSBDeviceClass::deviceGetDeviceProtocol(void *self, UInt8 *devProtocol)
    { return getThis(self)->GetDeviceProtocol(devProtocol); }

IOReturn
IOUSBDeviceClass::deviceGetDeviceVendor(void *self, UInt16 *devVendor)
    { return getThis(self)->GetDeviceVendor(devVendor); }

IOReturn
IOUSBDeviceClass::deviceGetDeviceProduct(void *self, UInt16 *devProduct)
    { return getThis(self)->GetDeviceProduct(devProduct); }

IOReturn
IOUSBDeviceClass::deviceGetDeviceReleaseNumber(void *self, UInt16 *devRelNum)
    { return getThis(self)->GetDeviceReleaseNumber(devRelNum); }

IOReturn
IOUSBDeviceClass::deviceGetDeviceAddress(void *self, USBDeviceAddress *addr)
    { return getThis(self)->GetDeviceAddress(addr); }

IOReturn
IOUSBDeviceClass::deviceGetDeviceBusPowerAvailable(void *self, UInt32 *powerAvail)
    { return getThis(self)->GetDeviceBusPowerAvailable(powerAvail); }

IOReturn
IOUSBDeviceClass::deviceGetDeviceSpeed(void *self, UInt8 *devSpeed)
    { return getThis(self)->GetDeviceSpeed(devSpeed); }

IOReturn
IOUSBDeviceClass::deviceGetNumberOfConfigurations(void *self, UInt8 *numConfig)
    { return getThis(self)->GetNumberOfConfigurations(numConfig); }

IOReturn
IOUSBDeviceClass::deviceGetLocationID(void *self, UInt32 *locationID)
    { return getThis(self)->GetLocationID(locationID); }

IOReturn
IOUSBDeviceClass::deviceGetConfigurationDescriptorPtr(void *self, UInt8 index, IOUSBConfigurationDescriptorPtr *desc)
    { return getThis(self)->GetConfigurationDescriptorPtr(index, desc); }

IOReturn
IOUSBDeviceClass::deviceGetConfiguration(void *self, UInt8 *configNum)
    { return getThis(self)->GetConfiguration(configNum); }

IOReturn
IOUSBDeviceClass::deviceSetConfiguration(void *self, UInt8 configNum)
    { return getThis(self)->SetConfiguration(configNum); }

IOReturn 
IOUSBDeviceClass::deviceUSBDeviceOpen(void *self)
    { return getThis(self)->USBDeviceOpen(false); }

IOReturn 
IOUSBDeviceClass::deviceUSBDeviceClose(void *self)
    { return getThis(self)->USBDeviceClose(); }

IOReturn 
IOUSBDeviceClass::deviceGetBusFrameNumber(void *self, UInt64 *frame, AbsoluteTime *atTime)
    { return getThis(self)->GetBusFrameNumber(frame, atTime); }

IOReturn 
IOUSBDeviceClass::deviceResetDevice(void *self)
    { return getThis(self)->ResetDevice(); }

IOReturn 
IOUSBDeviceClass::deviceDeviceRequest(void *self, IOUSBDevRequest *reqIn)
{ 
    IOUSBDevRequestTO	req;
    IOReturn		err;
    
    req.bmRequestType = reqIn->bmRequestType;
    req.bRequest = reqIn->bRequest;
    req.wValue = reqIn->wValue;
    req.wIndex = reqIn->wIndex;
    req.wLength = reqIn->wLength;
    req.pData = reqIn->pData;
    req.wLenDone = reqIn->wLenDone;
    req.completionTimeout = kUSBDefaultControlCompletionTimeoutMS;
    req.noDataTimeout = kUSBDefaultControlNoDataTimeoutMS;
    
    err =  getThis(self)->DeviceRequest(&req);
    reqIn->wLenDone = req.wLenDone;
    return err;
}

IOReturn 
IOUSBDeviceClass::deviceDeviceRequestAsync(void *self, IOUSBDevRequest *reqIn, IOAsyncCallback1 callback, void *refCon)
{ 
    IOUSBDevRequestTO	req;
    
    req.bmRequestType = reqIn->bmRequestType;
    req.bRequest = reqIn->bRequest;
    req.wValue = reqIn->wValue;
    req.wIndex = reqIn->wIndex;
    req.wLength = reqIn->wLength;
    req.pData = reqIn->pData;
    req.wLenDone = reqIn->wLenDone;
    req.completionTimeout =  kUSBDefaultControlCompletionTimeoutMS;
    req.noDataTimeout = kUSBDefaultControlNoDataTimeoutMS;
    
    return getThis(self)->DeviceRequestAsync(&req, callback, refCon); 
}

IOReturn 
IOUSBDeviceClass::deviceCreateInterfaceIterator(void *self, IOUSBFindInterfaceRequest *intfReq, io_iterator_t *iter)
    { return getThis(self)->CreateInterfaceIterator(intfReq, iter); }

IOReturn 
IOUSBDeviceClass::deviceUSBDeviceOpenSeize(void *self)
    { return getThis(self)->USBDeviceOpen(true); }

IOReturn 
IOUSBDeviceClass::deviceDeviceRequestTO(void *self, IOUSBDevRequestTO *req)
    { return getThis(self)->DeviceRequest(req); }

IOReturn 
IOUSBDeviceClass::deviceDeviceRequestAsyncTO(void *self, IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon)
    { return getThis(self)->DeviceRequestAsync(req, callback, refCon); }

IOReturn
IOUSBDeviceClass::deviceUSBDeviceSuspend(void *self, Boolean suspend)
    { return getThis(self)->USBDeviceSuspend(suspend); }

IOReturn
IOUSBDeviceClass::deviceUSBDeviceAbortPipeZero(void *self)
    { return getThis(self)->USBDeviceAbortPipeZero(); }

IOReturn
IOUSBDeviceClass::deviceGetManufacturerStringIndex(void *self, UInt8 *msi)
    { return getThis(self)->USBDeviceGetManufacturerStringIndex(msi); }


IOReturn
IOUSBDeviceClass::deviceGetProductStringIndex(void *self, UInt8 *psi)
    { return getThis(self)->USBDeviceGetProductStringIndex(psi); }


IOReturn
IOUSBDeviceClass::deviceGetSerialNumberStringIndex(void *self, UInt8 *snsi)
    { return getThis(self)->USBDeviceGetSerialNumberStringIndex(snsi); }

IOReturn
IOUSBDeviceClass::deviceReEnumerateDevice(void *self, UInt32 options)
{ return getThis(self)->USBDeviceReEnumerate(options); }

IOReturn
IOUSBDeviceClass::deviceGetBusMicroFrameNumber(void *self, UInt64 *microFrame, AbsoluteTime *atTime)
{ return getThis(self)->GetBusMicroFrameNumber(microFrame, atTime); }

IOReturn
IOUSBDeviceClass::deviceGetIOUSBLibVersion( void *self, NumVersion *ioUSBLibVersion, NumVersion *usbFamilyVersion)
{ return getThis(self)->GetIOUSBLibVersion(ioUSBLibVersion, usbFamilyVersion); }


