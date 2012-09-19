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

// 
#include <TargetConditionals.h>

#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CoreFoundation.h>

#include "IOUSBDeviceClass.h"
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBUserClient.h>
#include <IOKit/usb/IOUSBLib.h>

#if !TARGET_OS_EMBEDDED
#endif

#include <stdio.h>

__BEGIN_DECLS
#include <mach/mach.h>
#include <IOKit/iokitmig.h>
__END_DECLS

#pragma mark #defines

#ifndef IOUSBLIBDEBUG   
	#define IOUSBLIBDEBUG		0
#endif

#if IOUSBLIBDEBUG
	#define __STDC_FORMAT_MACROS 1
	#include <inttypes.h>
	#define DEBUGPRINT(x,...)	asl_log(fASLClient, NULL, ASL_LEVEL_NOTICE,  x, ##__VA_ARGS__);
#else
	#define DEBUGPRINT(x,...)       
#endif

#define ATTACHEDCHECK() do {	    \
    if (!fDeviceIsAttached)		    \
	{						\
		DEBUGPRINT("+IOUSBDeviceClass[%p]::attached check failed\n", this); \
		return kIOReturnNoDevice;   \
	}						\
} while (0)

#define OPENCHECK() do {	    \
    if (!fIsOpen)		    \
	{						\
		DEBUGPRINT("+IOUSBDeviceClass[%p]::open check failed\n", this);\
        return kIOReturnNotOpen;    \
	}						\
} while (0)

#define ALLCHECKS() do {	    \
    ATTACHEDCHECK();		    \
    OPENCHECK();		    \
} while (0)

#pragma mark -

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
	fClass(0),
	fSubClass(0),
	fProtocol(0),
	fVendor(0),
	fProduct(0),
	fDeviceReleaseNumber(0),
	fManufacturerStringIndex(0),
	fProductStringIndex(0),
	fSerialNumberStringIndex(0),
	fNumConfigurations(0),
	fAddress(0),
	fPowerAvail(0),
	fSpeed(0),
	fLocationID(0),
	fConfigurations(NULL),
	fConfigDescCacheValid(false),
	fASLClient(NULL),
	fDeviceIsAttached(false)
{
#if IOUSBLIBDEBUG
	fASLClient = asl_open(NULL, "com.apple.iousblib", 0);
#endif
	
	DEBUGPRINT("IOUSBDeviceClass[%p]::IOUSBDeviceClass\n", this);
#ifdef SUPPORTS_SS_USB
    fUSBDevice.pseudoVTable = (IUnknownVTbl *)  &sUSBDeviceInterfaceV500;
#else
    fUSBDevice.pseudoVTable = (IUnknownVTbl *)  &sUSBDeviceInterfaceV320;
#endif
    fUSBDevice.obj = this;
}

IOUSBDeviceClass::~IOUSBDeviceClass()
{
	DEBUGPRINT("IOUSBDeviceClass[%p]::~IOUSBDeviceClass\n", this);
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
    
    if (fConnection) 
	{
        IOServiceClose(fConnection);
        fConnection = MACH_PORT_NULL;
    }
        
    if (fService) 
	{
        IOObjectRelease(fService);
        fService = MACH_PORT_NULL;
    }
	
	if (fAsyncPort)
    {
		IONotificationPortDestroy(fAsyncPort);
        fAsyncPort = NULL;
    }
	
	if ( fASLClient )
		asl_close(fASLClient);
}

#pragma mark -

HRESULT 
IOUSBDeviceClass::queryInterface(REFIID iid, void **ppv)
{
	DEBUGPRINT("IOUSBDeviceClass[%p]::queryInterface, fService= 0x%x\n", this, fService);
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOCFPlugInInterfaceID)) 
    {
		DEBUGPRINT("IOUSBDeviceClass[%p]::queryInterface for IUnknownUUID or kIOCFPlugInInterfaceID\n", this);
       *ppv = &iunknown;
        addRef();
    }
    else if (CFEqual(uuid, kIOUSBDeviceInterfaceID) || 
			 CFEqual(uuid, kIOUSBDeviceInterfaceID182) ||
             CFEqual(uuid, kIOUSBDeviceInterfaceID187) || 
			 CFEqual(uuid, kIOUSBDeviceInterfaceID197) ||
			 CFEqual(uuid, kIOUSBDeviceInterfaceID245) ||
			 CFEqual(uuid, kIOUSBDeviceInterfaceID300) ||
			 CFEqual(uuid, kIOUSBDeviceInterfaceID320) 
#ifdef SUPPORTS_SS_USB
			 || CFEqual(uuid, kIOUSBDeviceInterfaceID500)
#endif
			 ) 
    {
        *ppv = &fUSBDevice;
        addRef();

		// Version 245 fixes rdar://3030440.  In order to not change the behavior of previous versions, we will release the
		// fService that we now retain in start().  
        if ( (CFEqual(uuid, kIOUSBDeviceInterfaceID)
			|| CFEqual(uuid, kIOUSBDeviceInterfaceID182) 
			|| CFEqual(uuid, kIOUSBDeviceInterfaceID187) 
			|| CFEqual(uuid, kIOUSBDeviceInterfaceID197)) ) 
		{
			if ( fService != IO_OBJECT_NULL )
			{
				DEBUGPRINT("IOUSBDeviceClass[%p]::queryInterface releasing retained fService.  See rdar://3030440\n", this);
				IOObjectRelease(fService);
			}
		}
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
#pragma unused (propertyTable, order)
	DEBUGPRINT("IOUSBDeviceClass[%p]::probe\n", this);
    if (!inService || !IOObjectConformsTo(inService, "IOUSBDevice"))
	{
		DEBUGPRINT("IOUSBDeviceClass[%p]::probe returning kIOReturnBadArgument\n", this);
        return kIOReturnBadArgument;
	}

    return kIOReturnSuccess;
}



IOReturn 
IOUSBDeviceClass::start(CFDictionaryRef propertyTable, io_service_t inService)
{
#pragma unused (propertyTable)
    IOReturn 			res;
    CFMutableDictionaryRef 	entryProperties = 0;
    kern_return_t 		kr;
    
#if !TARGET_OS_EMBEDDED
#endif
	
    res = IOServiceOpen(inService, mach_task_self(), 0, &fConnection);
    if (res != kIOReturnSuccess)
	{
 		DEBUGPRINT("IOUSBDeviceClass[%p]::start  IOServiceOpen returned 0x%x\n", this, res);
		return res;
	}
	
    if ( fConnection == MACH_PORT_NULL )
		return kIOReturnNoDevice;
	
	fDeviceIsAttached = true;
    
	// Make sure that we retain our service so that we can use it later on.  Previous to UUID245, we didn't used to do this, we just
	// released it in the dtor!  We now retain it, but in order to not break any current apps that relied on the broken behavior, we
	// will only retain it for UUIDs 245 or later.  However, we can only know the UUID in QueryInterface AND if we retain it there, a window
	// exists where the caller can release the io_service between the call to IOCreatePlugInInterfaceForService() -- which calls start() and the
	// QueryInterface().  So, we now retain it here and then release it in QueryInterface if the UUID is < 245.
    //
    res = IOObjectRetain(inService);
    if (res)
	{
		DEBUGPRINT("IOUSBDeviceClass[%p]::start  IOObjectRetain returned 0x%x\n", this, res);
        return res;
	}
    
	fService = inService;
	
	DEBUGPRINT("IOUSBDeviceClass[%p]::start (fService: 0x%x)", this, fService);
	
	kr = IORegistryEntryCreateCFProperties(fService, &entryProperties, NULL, 0);
    if (kr)
	{
		DEBUGPRINT("IOUSBDeviceClass[%p]::start IORegistryEntryCreateCFProperties returned 0x%x", this, kr);
		return kr;
	}
	
    if (entryProperties && (CFGetTypeID(entryProperties) == CFDictionaryGetTypeID())) 
    {
		// The dictionary contains values that are signed 32 or 64 bit.  To avoid CF sign extensions issues read the values into a temp
		// SInt64 and then assign to the local ivar
		// (rdar://4951538 & rdar://5081728)
		
        CFTypeRef	val;
		SInt64		tempValue;
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDeviceClass));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fClass = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDeviceSubClass));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fSubClass = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDeviceProtocol));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fProtocol = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBVendorID));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fVendor = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBProductID));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
 		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fProduct = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDeviceReleaseNumber));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fDeviceReleaseNumber = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBManufacturerStringIndex));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fManufacturerStringIndex = tempValue;
		}		
        else
            fManufacturerStringIndex = 0;
		
		val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBProductStringIndex));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fProductStringIndex = tempValue;
		}
		else
            fProductStringIndex = 0;
		
		val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBSerialNumberStringIndex));
		if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fSerialNumberStringIndex = tempValue;
		}
		else
            fSerialNumberStringIndex = 0;
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDeviceNumConfigs));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fNumConfigurations = tempValue;
		}
		else
            fNumConfigurations = 0;
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDevicePropertySpeed));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fSpeed = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDevicePropertyBusPowerAvailable));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fPowerAvail = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDevicePropertyAddress));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fAddress = tempValue;
		}
		
		val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDevicePropertyLocationID));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fLocationID = tempValue;
		}
		
        fConfigDescCacheValid = false;
        if (fNumConfigurations)
        {
            fConfigurations = (IOUSBConfigurationDescriptorPtr*) malloc(fNumConfigurations * sizeof(IOUSBConfigurationDescriptorPtr));
            bzero(fConfigurations, fNumConfigurations * sizeof(IOUSBConfigurationDescriptorPtr));
			kr = CacheConfigDescriptor();
			if (kr)
			{
				DEBUGPRINT("IOUSBDeviceClass[%p]::start CacheConfigDescriptor returned 0x%x", this, kr);
				return kr;
			}
		}
        CFRelease(entryProperties);
    }
	
	
    return kIOReturnSuccess;
}



IOReturn 
IOUSBDeviceClass::stop()
{
	IOReturn ret = kIOReturnSuccess;
	
	DEBUGPRINT("IOUSBDeviceClass[%p]::stop\n", this);
	ATTACHEDCHECK();
	if (fIsOpen)
		ret = USBDeviceClose();
	
	return ret;
}
	
#pragma mark -

IOReturn
IOUSBDeviceClass::CacheConfigDescriptor()
{
    int 	i;
    IOReturn	kr = kIOReturnSuccess;
    
	DEBUGPRINT("IOUSBDeviceClass[%p]::CacheConfigDescriptor\n", this);

    for (i = 0; i < fNumConfigurations; i++)
    {
        IOUSBConfigurationDescriptorPtr	configPtr = NULL;
        IOUSBConfigurationDescHeader	configHdr;
		uint64_t						in[5];
		size_t							configSize;
		
		in[0] = (uint64_t) i;
		configSize = sizeof(configHdr);
		
		DEBUGPRINT("+IOUSBDeviceClass[%p]::CacheConfigDescriptor asking for header for config = %d\n", this, i);
		
        kr = IOConnectCallMethod(fConnection, kUSBDeviceUserClientGetConfigDescriptor, in, 1, 0, 0, 0, 0, (void *) &configHdr, &configSize); 
		if (kr)
		{
			DEBUGPRINT("+IOUSBDeviceClass[%p]::CacheConfigDescriptor kUSBDeviceUserClientGetConfigDescriptor asking for header of config %d returned 0x%x\n", this, i, kr);
			break;
		}
		
		// Not that we have the header, we can get the real length of the descriptor
 		in[0] = (uint64_t) i;
        configSize = USBToHostWord(configHdr.wTotalLength);
        configPtr = (IOUSBConfigurationDescriptorPtr) malloc(configSize+2);
		
		DEBUGPRINT("+IOUSBDeviceClass[%p]::CacheConfigDescriptor asking for config %d, size:  %ld, pointer: %p\n", this, i, configSize, configPtr);
		
		kr = IOConnectCallMethod(fConnection, kUSBDeviceUserClientGetConfigDescriptor, in, 1, 0, 0, 0, 0, configPtr, &configSize); 
		if (kr)
		{
			DEBUGPRINT("+IOUSBDeviceClass[%p]::CacheConfigDescriptor kUSBDeviceUserClientGetConfigDescriptor asking for full config %d returned 0x%x\n", this, i, kr);
			break;
		}
        
        // Add a dummy empty descriptor on the end
        *((char*)configPtr + configSize) = 0;
        *((char*)configPtr + configSize + 1) = 0;
        fConfigurations[i] = configPtr;
    }
		
    if ( kr == kIOReturnSuccess )
       fConfigDescCacheValid = TRUE;
        
	DEBUGPRINT("-IOUSBDeviceClass[%p]::CacheConfigDescriptor returning 0x%x\n", this, kr);
    return kr;
}

#pragma mark IORegisty Getters

IOReturn 
IOUSBDeviceClass::GetDeviceClass(UInt8 *devClass)
{
    ATTACHEDCHECK();
    *devClass = fClass;
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceClass = %d\n", *devClass);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceSubClass(UInt8 *devSubClass)
{
    ATTACHEDCHECK();
    *devSubClass = fSubClass;
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceSubClass = %d\n", *devSubClass);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceProtocol(UInt8 *devProtocol)
{
    ATTACHEDCHECK();
    *devProtocol = fProtocol;
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceProtocol = %d\n", *devProtocol);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceVendor(UInt16 *devVendor)
{
    ATTACHEDCHECK();
    *devVendor = fVendor;
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceVendor = %d\n", *devVendor);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceProduct(UInt16 *devProduct)
{
    ATTACHEDCHECK();
    *devProduct = fProduct;
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceProduct = %d\n", *devProduct);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceReleaseNumber(UInt16 *devRelNum)
{
    ATTACHEDCHECK();
    *devRelNum = fDeviceReleaseNumber;
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceReleaseNumber = %d\n", *devRelNum);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceAddress(USBDeviceAddress *addr)
{
    ATTACHEDCHECK();
    *addr = fAddress;
	DEBUGPRINT("IOUSBDeviceClass::GetDeviceAddress  = %d\n", *addr);
   return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceBusPowerAvailable(UInt32 *powerAvail)
{
    ATTACHEDCHECK();
    *powerAvail = fPowerAvail;
	DEBUGPRINT("IOUSBDeviceClass::GetDeviceBusPowerAvailable = %" PRIu32 "\n", (uint32_t) *powerAvail);
   return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetDeviceSpeed(UInt8 *devSpeed)
{
    ATTACHEDCHECK();
    *devSpeed = fSpeed;
    DEBUGPRINT("IOUSBDeviceClass::GetDeviceSpeed = %d\n", *devSpeed);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBDeviceClass::GetNumberOfConfigurations(UInt8 *numConfig)
{
    ATTACHEDCHECK();
    *numConfig = fNumConfigurations;
    DEBUGPRINT("IOUSBDeviceClass::GetNumberOfConfigurations = %d\n", *numConfig);
    return kIOReturnSuccess;
}


IOReturn
IOUSBDeviceClass::GetLocationID(UInt32 *locationID)
{ 
    ATTACHEDCHECK();
    *locationID = fLocationID;
    DEBUGPRINT("IOUSBDeviceClass::GetLocationID = 0x%" PRIx32 "\n", (uint32_t)*locationID);
    return kIOReturnSuccess;
}

IOReturn
IOUSBDeviceClass::USBDeviceGetManufacturerStringIndex(UInt8 *msi)
{
    ATTACHEDCHECK();
    *msi = fManufacturerStringIndex;
    DEBUGPRINT("IOUSBDeviceClass::USBDeviceGetManufacturerStringIndex = %d\n", *msi);
    return kIOReturnSuccess;
}


IOReturn
IOUSBDeviceClass::USBDeviceGetProductStringIndex(UInt8 *psi)
{
    ATTACHEDCHECK();
    *psi = fProductStringIndex;
    DEBUGPRINT("IOUSBDeviceClass::USBDeviceGetProductStringIndex = %d\n", *psi);
    return kIOReturnSuccess;
}


IOReturn
IOUSBDeviceClass::USBDeviceGetSerialNumberStringIndex(UInt8 *snsi)
{
    ATTACHEDCHECK();
    *snsi = fSerialNumberStringIndex;
	DEBUGPRINT("IOUSBDeviceClass::USBDeviceGetSerialNumberStringIndex = %d\n", *snsi);
	return kIOReturnSuccess;
}

#pragma mark -

IOReturn 
IOUSBDeviceClass::GetConfigurationDescriptorPtr(UInt8 index, IOUSBConfigurationDescriptorPtr *desc)
{
    IOReturn	kr = kIOReturnSuccess;
    
    ATTACHEDCHECK();
    
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
    uint64_t			in[1];
    size_t				len = 1;
    IOReturn			kr = kIOReturnSuccess;

    DEBUGPRINT("IOUSBDeviceClass::SetConfiguration to %d\n", configNum);
    ALLCHECKS();
	
	in[0] = (uint64_t) configNum;
	
    kr = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientSetConfig, in, 1, NULL, NULL); 
    if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
	
	if ( kr != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBDeviceClass::SetConfiguration returning 0x%x\n", kr);
	}
	
    return kr;
}



IOReturn
IOUSBDeviceClass::GetConfiguration(UInt8 *config)
{
    uint64_t			output[1];
    uint32_t			len = 1;
    IOReturn			kr = kIOReturnSuccess;

	DEBUGPRINT("+IOUSBDeviceClass[%p]::GetConfiguration\n", this);
    ATTACHEDCHECK();

	output[0] = 0;
	kr = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientGetConfig, 0, 0, output, &len);

	DEBUGPRINT("IOUSBDeviceClass::GetConfiguration ret: 0x%x, config = 0x%qx\n", kr, output[0]);
	if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
    if (kr == kIOReturnSuccess)
	{
		*config = (UInt8) output[0];
	}
	
	if ( kr != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBDeviceClass::GetConfiguration returning 0x%x\n", kr);
	}
	
    return kr;
}



IOReturn IOUSBDeviceClass::
CreateDeviceAsyncEventSource(CFRunLoopSourceRef *source)
{
    IOReturn 		ret;
    CFMachPortRef 	cfPort;
    CFMachPortContext 	context;
    Boolean 		shouldFreeInfo;
	
	DEBUGPRINT("IOUSBDeviceClass[%p]::CreateDeviceAsyncEventSource\n", this);
	if ( fCFSource != NULL )
	{
		DEBUGPRINT("IOUSBDeviceClass[%p]::CreateInterfaceAsyncEventSource already had a CFSource!", this);

		// Since we are returning our fCFSource and this is a Create call, be sure to retain it
		CFRetain(fCFSource);
		*source = fCFSource;
		return kIOReturnSuccess;
	}
	
    if (!fAsyncPort) 
    {     
    	// This CreateDeviceAsyncPort() will create the fCFSource
        ret = CreateDeviceAsyncPort(0);
        if (kIOReturnSuccess != ret)
            return ret;
    }
	
    if (!fCFSource)
        return kIOReturnNoMemory;
	
    if (source)
    {
        // We retain the fCFSource because our API is a "Create" API, which the callers will assume that they
        // have to release it.  Since we now get it via IONotificationPortGetRunLoopSource(), the IONotificationPortDestroy()
        // will release it -- if we don't retain it here, we would do a double relese
        CFRetain(fCFSource);
		*source = fCFSource;
    }
	
	DEBUGPRINT("-IOUSBDeviceClass::CreateDeviceAsyncEventSource\n");
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
	IOReturn		kr = kIOReturnSuccess;
    mach_port_t		masterPort;
	
    ATTACHEDCHECK();
    
    // If we already have a port, don't create a new one.
    if (fAsyncPort) 
	{
		if (port)
			*port = IONotificationPortGetMachPort(fAsyncPort);
		return kIOReturnSuccess;
    }
	
	fAsyncPort = IONotificationPortCreate(kIOMasterPortDefault);
	
    if (fAsyncPort) 
	{
        // Get the runloop source from our notification port
        fCFSource = IONotificationPortGetRunLoopSource(fAsyncPort);
      
		if (port)
            *port = IONotificationPortGetMachPort(fAsyncPort);
		
		if (fIsOpen) 
		{
			io_async_ref64_t    asyncRef;
			uint32_t				len = 0;
			kr = IOConnectCallAsyncScalarMethod(fConnection, kUSBDeviceUserClientSetAsyncPort, IONotificationPortGetMachPort(fAsyncPort), asyncRef, 1, 0, 0, 0, &len);
			if (kr == MACH_SEND_INVALID_DEST)
			{
				fIsOpen = false;
				fDeviceIsAttached = false;
				kr = kIOReturnNoDevice;
			}
		}
    }
    
    return kr;
}



mach_port_t 
IOUSBDeviceClass::GetDeviceAsyncPort()
{
	if ( fAsyncPort == MACH_PORT_NULL )
		return MACH_SEND_INVALID_DEST;
	else
		return IONotificationPortGetMachPort(fAsyncPort);
}



IOReturn 
IOUSBDeviceClass::USBDeviceOpen(bool seize)
{
    IOReturn ret = kIOReturnSuccess;
	
	DEBUGPRINT("+IOUSBDeviceClass[%p]::USBDeviceOpen\n", this);
    ATTACHEDCHECK();
	
    if (fIsOpen)
        return kIOReturnSuccess;
	
    uint32_t	len = 0;
    uint64_t	input = (uint64_t) seize;
	
    ret = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientOpen, &input, 1, 0, &len); 
	
    if (ret == kIOReturnSuccess)
	{
		fIsOpen = true;
		
		if (fAsyncPort) 
		{
			io_async_ref64_t    asyncRef;
			
			ret = IOConnectCallAsyncScalarMethod(fConnection, kUSBDeviceUserClientSetAsyncPort, IONotificationPortGetMachPort(fAsyncPort), asyncRef, 1, 0, 0, 0, &len);
			if ( (ret != kIOReturnSuccess) and (ret != MACH_SEND_INVALID_DEST ))
			{
				USBDeviceClose();
			}
		}
    }
	
    if (ret == MACH_SEND_INVALID_DEST)
	{
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	
	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBDeviceClass[%p]::USBDeviceOpen returns 0x%x\n", this, ret);
	}
    return ret;
}



IOReturn 
IOUSBDeviceClass::USBDeviceClose()
{   
    IOReturn	ret;
	DEBUGPRINT("+IOUSBDeviceClass[%p]::USBDeviceClose\n", this);

    ALLCHECKS();

    fIsOpen = false;

    ret = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientClose,0, 0, 0, 0); 
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBDeviceClass[%p]::USBDeviceClose returns 0x%x\n", this, ret);
	}
    return ret;
}



IOReturn 
IOUSBDeviceClass::ResetDevice()
{
    IOReturn			ret = kIOReturnSuccess;
	
 	DEBUGPRINT("+IOUSBDeviceClass[%p]::ResetDevice\n", this);
    ALLCHECKS();
	
    ret = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientResetDevice, 0, 0, 0, 0); 
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
 	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBDeviceClass[%p]::ResetDevice returns 0x%x\n", this, ret);
	}
    return ret;
}



IOReturn
IOUSBDeviceClass::USBDeviceReEnumerate(UInt32 options)
{
    uint64_t			in[1];
    uint32_t			len = 1;
    IOReturn			ret = kIOReturnSuccess;
	
    DEBUGPRINT("IOUSBDeviceClass::USBDeviceReEnumerate options: 0x%" PRIx32 "\n", (uint32_t) options);
    ALLCHECKS();
	
	in[0] = (uint64_t) options;
	
    ret = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientReEnumerateDevice, in, 1, NULL, NULL); 
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
 	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBDeviceClass[%p]::USBDeviceReEnumerate returns 0x%x\n", this, ret);
	}
    return ret;
}


IOReturn
IOUSBDeviceClass::USBDeviceSuspend(bool suspend)
{
    uint64_t			in[1];
    size_t				len = 1;
    IOReturn			ret = kIOReturnSuccess;
	
    DEBUGPRINT("IOUSBDeviceClass::USBDeviceSuspend to %s\n", suspend ? "Suspend" : "Resume");
    ALLCHECKS();
	
	in[0] = (uint64_t) suspend;
	
    ret = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientSuspend, in, 1, NULL, NULL); 
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
 	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBDeviceClass[%p]::USBDeviceSuspend returns 0x%x\n", this, ret);
	}
    return ret;
}



IOReturn
IOUSBDeviceClass::USBDeviceAbortPipeZero(void)
{
    IOReturn			ret;
	
    ALLCHECKS();
    
    ret = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientAbortPipeZero, 0, 0, 0, 0); 
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
    return ret;
}

#pragma mark -
IOReturn
IOUSBDeviceClass::GetUSBDeviceInformation(UInt32 *info)
{
    uint64_t			output[1];
    uint32_t			len = 1;
    IOReturn			kr = kIOReturnSuccess;
	
	DEBUGPRINT("+IOUSBDeviceClass[%p]::GetUSBDeviceInformation\n", this);
    ATTACHEDCHECK();
	
	output[0] = 0;
	kr = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientGetDeviceInformation, 0, 0, output, &len);
	
	DEBUGPRINT("IOUSBDeviceClass::GetUSBDeviceInformation  ret: 0x%x, info = 0x%qx\n", kr, output[0]);
	if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
    if (kr == kIOReturnSuccess)
	{
		*info = (UInt32) output[0];
	}
	
	if ( kr != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBDeviceClass::GetUSBDeviceInformation returning 0x%x\n", kr);
	}
	
    return kr;
}


IOReturn
IOUSBDeviceClass::RequestExtraPower(UInt32 type, UInt32 requestedPower, UInt32 *powerAvailable)
{
    uint64_t			input[2];
    uint64_t			output[1];
    uint32_t			len = 1;
    IOReturn			kr = kIOReturnSuccess;
	
	DEBUGPRINT("+IOUSBDeviceClass[%p]::RequestExtraPower type: %d, requested: %d\n", this, (uint32_t) type, (uint32_t) requestedPower);
    ATTACHEDCHECK();
	
	input[0] = (uint64_t) type;
	input[1] = (uint64_t) requestedPower;
	output[0] = 0;
	kr = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientRequestExtraPower, input, 2, output, &len);
	
	DEBUGPRINT("IOUSBDeviceClass::RequestExtraPower ret: 0x%x, powerAvailable = %qd\n", kr, output[0]);
	if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
    if (kr == kIOReturnSuccess)
	{
		*powerAvailable = (UInt32) output[0];
	}
	
	if ( kr != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBDeviceClass::RequestExtraPower returning 0x%x\n", kr);
	}
	
    return kr;
}


IOReturn
IOUSBDeviceClass::ReturnExtraPower(UInt32 type, UInt32 powerReturned)
{
    uint64_t			input[2];
    IOReturn			kr = kIOReturnSuccess;
	
    DEBUGPRINT("IOUSBDeviceClass::ReturnExtraPower type: %d, amount %d\n", (uint32_t)type, (uint32_t)powerReturned);
    ATTACHEDCHECK();
	
	input[0] = (uint64_t) type;
	input[1] = (uint64_t) powerReturned;
	
    kr = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientReturnExtraPower, input, 2, NULL, NULL); 
	if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
	if ( kr != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBDeviceClass::ReturnExtraPower returning 0x%x\n", kr);
	}
	
    return kr;
}


IOReturn
IOUSBDeviceClass::GetExtraPowerAllocated(UInt32 type, UInt32 *powerAllocated)
{
    uint64_t			input[1];
    uint64_t			output[1];
    uint32_t			len = 1;
    IOReturn			kr = kIOReturnSuccess;
	
	DEBUGPRINT("+IOUSBDeviceClass[%p]::GetExtraPowerAllocated type: %d\n", this, (uint32_t) type);
    ATTACHEDCHECK();
	
	input[0] = (uint64_t) type;
	output[0] = 0;
	kr = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientGetExtraPowerAllocated, input, 1, output, &len);
	
	DEBUGPRINT("IOUSBDeviceClass::GetExtraPowerAllocated ret: 0x%x, powerAllocated = %qd\n", kr, output[0]);
	if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
    if (kr == kIOReturnSuccess)
	{
		*powerAllocated = (UInt32) output[0];
	}
	
	if ( kr != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBDeviceClass::GetExtraPowerAllocated returning 0x%x\n", kr);
	}
	
    return kr;
}

#ifdef SUPPORTS_SS_USB
IOReturn
IOUSBDeviceClass::GetBandwidthAvailableForDevice(UInt32 *bandwidth)
{
    uint64_t			output[1];
    uint32_t			len = 1;
    IOReturn			ret = kIOReturnSuccess;
	
	DEBUGPRINT("+IOUSBDeviceClass[%p]::GetBandwidthAvailableForDevice\n", this);
	
	ATTACHEDCHECK();
	
	output[0] = 0;
	ret = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientGetBandwidthAvailableForDevice, 0, 0, output, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	
	if (ret == kIOReturnSuccess)
	{
		*bandwidth = (UInt32) output[0];
	}
	
	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBDeviceClass[%p]::GetBandwidthAvailableForDevice returning 0x%x\n", this, ret);
	}
	
	DEBUGPRINT("-IOUSBDeviceClass[%p]::GetBandwidthAvailableForDevice returning %" PRIu32 "\n", this, (uint32_t)*bandwidth);
	
    return ret;
}
#endif

#pragma mark -
IOReturn 
IOUSBDeviceClass::DeviceRequest(IOUSBDevRequestTO *req)
{
    IOReturn 		ret = kIOReturnSuccess;
    uint64_t		input[9];
    uint64_t		output[1];	// For the count on an overrun
	uint32_t		outputCnt = 1;
    size_t			len;
	
    ATTACHEDCHECK();

	DEBUGPRINT("IOUSBDeviceClass::DeviceRequest: \n\tbmRequestType = 0x%2.2x\n\tbRequest = 0x%2.2x\n\twValue = 0x%4.4x\n\twIndex = 0x%4.4x\n\twLength = 0x%4.4x\n\tpData = %p\n\tnoDataTimeout = %" PRIu32 "\n\tcompletionTimeout = %" PRIu32 "\n",
			   req->bmRequestType,
			   req->bRequest,
			   req->wValue,
			   req->wIndex,
			   req->wLength,
			   req->pData,
			   (uint32_t) req->noDataTimeout,
			   (uint32_t) req->completionTimeout);

	input[0] = (uint64_t) 0;					// This would be the pipeZero pipeRef, for symmetry to the interface class usage
	input[1] = (uint64_t) req->bmRequestType;
	input[2] = (uint64_t) req->bRequest;
	input[3] = (uint64_t) req->wValue;
	input[4] = (uint64_t) req->wIndex;
	input[5] = (uint64_t) req->wLength;
	input[6] = (uint64_t) req->pData;
	input[7] = (uint64_t) req->noDataTimeout;
	input[8] = (uint64_t) req->completionTimeout;
	
	req->wLenDone = 0;

	len = req->wLength;
	
	switch ( (req->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask )
	{
	case kUSBOut:
		ret = IOConnectCallMethod( fConnection, kUSBDeviceUserClientDeviceRequestOut, input, 9, req->pData, len, 0, 0, 0, 0);
		if (kIOReturnSuccess == ret)
			req->wLenDone = req->wLength;
		else
			req->wLenDone = 0;
		break;
		
	case kUSBIn:
		output[0] = 0;
		ret = IOConnectCallMethod( fConnection, kUSBDeviceUserClientDeviceRequestIn, input, 9, 0, 0, output, &outputCnt, req->pData, &len);
		if (kIOReturnSuccess == ret)
		{
			req->wLenDone = len;
		}
		if (output[0] != 0)	// We had an overrun
		{
			DEBUGPRINT("IOUSBDeviceClass::DeviceRequest returning kIOReturnOverrun" );
			ret = kIOReturnOverrun;
		}
		break;
	}

    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	
	DEBUGPRINT("IOUSBDeviceClass::DeviceRequest returning 0x%x, wLenDone: %" PRIu32 "",ret, (uint32_t) req->wLenDone);
	
    return ret;
}



IOReturn 
IOUSBDeviceClass::DeviceRequestAsync(IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon)
{
	io_async_ref64_t    asyncRef;
	size_t				len;
    IOReturn			ret = kIOReturnUnsupported;
    uint64_t			input[9];
    uint64_t		output[1];	// Not used for async, but sync expects it, so async must too.
	uint32_t		outputCnt = 1;
	
    if (!fAsyncPort)
	{
		DEBUGPRINT("IOUSBDeviceClass::DeviceRequestAsync  NO async port\n");
        return kIOUSBNoAsyncPortErr;
	}
	
	DEBUGPRINT("IOUSBDeviceClass::DeviceRequestAsync: \n\tbmRequestType = 0x%2.2x\n\tbRequest = 0x%2.2x\n\twValue = 0x%4.4x\n\twIndex = 0x%4.4x\n\twLength = 0x%4.4x\n\tpData = %p\n\tnoDataTimeout = %" PRIu32 "\n\tcompletionTimeout = %" PRIu32 "\n",
			   req->bmRequestType,
			   req->bRequest,
			   req->wValue,
			   req->wIndex,
			   req->wLength,
			   req->pData,
			   (uint32_t) req->noDataTimeout,
			   (uint32_t) req->completionTimeout);

    ATTACHEDCHECK();
		
	input[0] = (uint64_t) 0;
	input[1] = (uint64_t) req->bmRequestType;
	input[2] = (uint64_t) req->bRequest;
	input[3] = (uint64_t) req->wValue;
	input[4] = (uint64_t) req->wIndex;
	input[5] = (uint64_t) req->wLength;
	input[6] = (uint64_t) req->pData;
	input[7] = (uint64_t) req->noDataTimeout;
	input[8] = (uint64_t) req->completionTimeout;

	len = req->wLength;

    asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t) refCon;

    switch ((req->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask)
    {
        case kUSBOut:
			ret = IOConnectCallAsyncScalarMethod( fConnection, kUSBDeviceUserClientDeviceRequestOut, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, input, 9, 0, 0);
          break;
            
        case kUSBIn:
			ret = IOConnectCallAsyncScalarMethod( fConnection, kUSBDeviceUserClientDeviceRequestIn, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, input, 9, output, &outputCnt);
            break;
    }
	
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	DEBUGPRINT("IOUSBDeviceClass::DeviceRequestAsync returning 0x%x\n",ret);
    return ret;
}


#pragma mark -

IOReturn 
IOUSBDeviceClass::CreateInterfaceIterator(IOUSBFindInterfaceRequest *intfReq, io_iterator_t *iter)
{
    uint64_t		input[4];
    uint32_t		len = 1;
    IOReturn		ret = kIOReturnSuccess;
	uint64_t		output[1];
	

    ATTACHEDCHECK(); 
	DEBUGPRINT("+IOUSBDeviceClass[%p]::CreateInterfaceIteratorXX  bInterfaceClass 0x%x, bInterfaceSubClass = 0x%x, bInterfaceProtocol = 0x%x, bAlternateSetting = 0x%x\n", this,
			   intfReq->bInterfaceClass,
			   intfReq->bInterfaceSubClass,
			   intfReq->bInterfaceProtocol,
			   intfReq->bAlternateSetting);
	
	
	input[0] = (uint64_t) intfReq->bInterfaceClass;
	input[1] = (uint64_t) intfReq->bInterfaceSubClass;
	input[2] = (uint64_t) intfReq->bInterfaceProtocol;
	input[3] = (uint64_t) intfReq->bAlternateSetting;

	ret = IOConnectCallScalarMethod(fConnection, kUSBDeviceUserClientCreateInterfaceIterator, input, 4, output, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		DEBUGPRINT("-IOUSBDeviceClass::CreateInterfaceIterator   IOConnectCallStructMethod returned MACH_SEND_INVALID_DEST\n");
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	
	if ( ret == kIOReturnSuccess)
	{
		*iter = (io_iterator_t) output[0];
	}
		
	DEBUGPRINT("-IOUSBDeviceClass::CreateInterfaceIterator returning error 0x%x (%d), iterator: 0x%x\n", ret, ret, *iter);
    return ret;
}


IOReturn 
IOUSBDeviceClass::GetBusFrameNumber(UInt64 *frame, AbsoluteTime *atTime)
{
    IOUSBGetFrameStruct 	frameInfo;
    IOReturn				ret;
    size_t					len;
	
 	DEBUGPRINT("+IOUSBDeviceClass[%p]::GetBusFrameNumber\n", this);
	
	ATTACHEDCHECK();
	
	len = sizeof(IOUSBGetFrameStruct);
    ret = IOConnectCallStructMethod(fConnection, kUSBDeviceUserClientGetFrameNumber, 0, 0, &frameInfo, &len);
    if (kIOReturnSuccess == ret) 
    {
#if !TARGET_OS_EMBEDDED
#endif
		{
			*frame = frameInfo.frame;
			*atTime = frameInfo.timeStamp;
		}
		DEBUGPRINT("IOUSBDeviceClass::GetBusFrameNumber frame: 0x%qx, time.hi: 0x%" PRIx32 ", time.lo: 0x%" PRIx32 "\n", *frame, (uint32_t) (*atTime).hi, (uint32_t) (*atTime).lo);
    }
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
 	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBDeviceClass[%p]::GetBusFrameNumber returns 0x%x\n", this, ret);
	}
	return ret;
}

IOReturn
IOUSBDeviceClass::GetBusMicroFrameNumber(UInt64 *microFrame, AbsoluteTime *atTime)
{
    IOUSBGetFrameStruct 	frameInfo;
    IOReturn				ret;
    size_t					len;
	
  	DEBUGPRINT("+IOUSBDeviceClass[%p]::GetBusMicroFrameNumber\n", this);
	
   ATTACHEDCHECK();

	len = sizeof(IOUSBGetFrameStruct);
	ret = IOConnectCallStructMethod(fConnection, kUSBDeviceUserClientGetMicroFrameNumber, 0, 0, &frameInfo, &len);
    if (kIOReturnSuccess == ret)
    {
#if !TARGET_OS_EMBEDDED
#endif
		{
			*microFrame = frameInfo.frame;
			*atTime = frameInfo.timeStamp;
		}
		DEBUGPRINT("IOUSBDeviceClass::GetBusMicroFrameNumber frame: 0x%qx, time.hi: 0x%" PRIx32 ", time.lo: 0x%" PRIx32 "\n", *microFrame, (uint32_t) (*atTime).hi, (uint32_t) (*atTime).lo);
    }
    if (ret == MACH_SEND_INVALID_DEST)
    {
        fIsOpen = false;
        fDeviceIsAttached = false;
        ret = kIOReturnNoDevice;
    }
 	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBDeviceClass[%p]::GetBusMicroFrameNumber returns 0x%x\n", this, ret);
	}
    return ret;
}


IOReturn 
IOUSBDeviceClass::GetBusFrameNumberWithTime(UInt64 *frame, AbsoluteTime *atTime)
{
    IOUSBGetFrameStruct 	frameInfo;
    IOReturn				ret;
    size_t					len;
	
  	DEBUGPRINT("+IOUSBDeviceClass[%p]::GetBusFrameNumberWithTime\n", this);

    ATTACHEDCHECK();
	
	len = sizeof(IOUSBGetFrameStruct);
	ret = IOConnectCallStructMethod(fConnection, kUSBDeviceUserClientGetFrameNumberWithTime, 0, 0, (void *) &frameInfo, &len);
    if (kIOReturnSuccess == ret) 
    {
#if !TARGET_OS_EMBEDDED
#endif
		{
			*frame = frameInfo.frame;
			*atTime = frameInfo.timeStamp;
		}
		DEBUGPRINT("IOUSBDeviceClass::GetBusFrameNumberWithTime frame: 0x%qx, time.hi: %" PRIx32 ", time.lo: %" PRIx32 "\n", *frame, (uint32_t) (*atTime).hi, (uint32_t) (*atTime).lo);
    }
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fDeviceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
 	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBDeviceClass[%p]::GetBusFrameNumberWithTime returns 0x%x\n", this, ret);
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

    ATTACHEDCHECK();

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


#pragma mark -


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


#ifdef SUPPORTS_SS_USB
IOUSBDeviceStruct500 
IOUSBDeviceClass::sUSBDeviceInterfaceV500 = {
#else
IOUSBDeviceStruct320 
IOUSBDeviceClass::sUSBDeviceInterfaceV320 = {
#endif
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
    &IOUSBDeviceClass::deviceGetIOUSBLibVersion,
    // ----------new with 3.0.0
    &IOUSBDeviceClass::deviceGetBusFrameNumberWithTime,
    // ----------new with 3.2.0
    &IOUSBDeviceClass::deviceGetUSBDeviceInformation,
    &IOUSBDeviceClass::deviceRequestExtraPower,
    &IOUSBDeviceClass::deviceReturnExtraPower,
    &IOUSBDeviceClass::deviceGetExtraPowerAllocated
#ifdef SUPPORTS_SS_USB
    // ---------- new with 5.0.0
    ,&IOUSBDeviceClass::deviceGetBandwidthAvailableForDevice
#endif
};

#pragma mark Routing interfaces

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

IOReturn 
IOUSBDeviceClass::deviceGetBusFrameNumberWithTime(void *self, UInt64 *frame, AbsoluteTime *atTime)
{ return getThis(self)->GetBusFrameNumberWithTime(frame, atTime); }

IOReturn 
IOUSBDeviceClass::deviceGetUSBDeviceInformation(void *self, UInt32	*info)
{ return getThis(self)->GetUSBDeviceInformation(info); }

IOReturn 
IOUSBDeviceClass::deviceRequestExtraPower(void *self, UInt32 type, UInt32 requestedPower, UInt32 *powerAvailable)
{ return getThis(self)->RequestExtraPower(type, requestedPower, powerAvailable); }

IOReturn 
IOUSBDeviceClass::deviceReturnExtraPower(void *self, UInt32 type, UInt32 powerReturned)
{ return getThis(self)->ReturnExtraPower(type, powerReturned); }

IOReturn 
IOUSBDeviceClass::deviceGetExtraPowerAllocated(void *self, UInt32 type, UInt32 *powerAllocated)
{ return getThis(self)->GetExtraPowerAllocated(type, powerAllocated); }

#ifdef SUPPORTS_SS_USB
//--------------- added in 5.0.0
IOReturn
IOUSBDeviceClass::deviceGetBandwidthAvailableForDevice(void *self, UInt32 *bandwidth)
{ return getThis(self)->GetBandwidthAvailableForDevice(bandwidth); }
#endif

