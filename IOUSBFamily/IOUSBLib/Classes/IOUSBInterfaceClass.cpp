/*
 * Copyright © 1998-2012 Apple Inc. All rights reserved.
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

#include <TargetConditionals.h>

#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CoreFoundation.h>

#include "IOUSBInterfaceClass.h"
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBUserClient.h>
#if !TARGET_OS_EMBEDDED
#endif

#include <stdio.h>

__BEGIN_DECLS
#include <mach/mach.h>
#include <IOKit/iokitmig.h>
__END_DECLS

#if IOUSBLIBDEBUG
	#define __STDC_FORMAT_MACROS 1
	#include <inttypes.h>
	#define DEBUGPRINT(x,...)	asl_log(fASLClient, NULL, ASL_LEVEL_NOTICE,  x, ##__VA_ARGS__);
#else
    #define DEBUGPRINT(x,...)   
#endif

#define ATTACHEDCHECK() do {	    \
    if (!fInterfaceIsAttached)		    \
	{						\
		DEBUGPRINT("IOUSBInterfaceClass[%p]::attached check failed\n", this); \
		return kIOReturnNoDevice;   \
	}						\
} while (0)

#define OPENCHECK() do {	    \
    if (!fIsOpen)		    \
	{						\
		DEBUGPRINT("IOUSBInterfaceClass[%p]::open check failed\n", this);\
		return kIOReturnNotOpen;    \
	}						\
} while (0)

#define ALLCHECKS() do {	    \
    ATTACHEDCHECK();		    \
    OPENCHECK();		    \
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
	fService(IO_OBJECT_NULL),
	fDevice(IO_OBJECT_NULL),
	fConnection(IO_OBJECT_NULL),
	fAsyncPort(MACH_PORT_NULL),
	fCFSource(0),
	fIsOpen(false),
	fClass(0),
	fSubClass(0),
	fProtocol(0),
	fConfigValue(0),
	fInterfaceNumber(0),
	fAlternateSetting(0),
	fNumEndpoints(0),
	fStringIndex(0),
	fVendor(0),
	fProduct(0),
	fDeviceReleaseNumber(0),
	fLocationID(0),
	fNumConfigurations(0),
	fNextCookie(0),
	fUserBufferInfoListHead(NULL),
	fConfigLength(0),
	fInterfaceDescriptor(NULL),
	fConfigurations(NULL),
	fConfigDescCacheValid(false),
	fCurrentConfigIndex(0),
	fNeedContiguousMemoryForLowLatencyIsoch(0),
	fNeedsToReleasefDevice(false),
	fASLClient(NULL),
	fInterfaceIsAttached(false)
{
#if IOUSBLIBDEBUG
	fASLClient = asl_open(NULL, "com.apple.iousblib", 0);
#endif
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::IOUSBInterfaceClass\n", this);
    fUSBInterface.pseudoVTable = (IUnknownVTbl *)  &sUSBInterfaceInterfaceV500;
    fUSBInterface.obj = this;
}


IOUSBInterfaceClass::~IOUSBInterfaceClass()
{
    DEBUGPRINT("+IOUSBInterfaceClass[%p]::~IOUSBInterfaceClass\n", this);
    if (fConfigurations)
    {
        // release the config descriptor data
        int i;
        for (i=0; i< fNumConfigurations; i++)
            if (fConfigurations[i])
			free(fConfigurations[i]);
        
		free(fConfigurations);
        fConfigurations = NULL;
		fConfigDescCacheValid = false;
    }

    if (fConnection) 
	{
        IOServiceClose(fConnection);
        fConnection = MACH_PORT_NULL;
		fInterfaceIsAttached = false;
    }
        
    if (fService) 
	{
        IOObjectRelease(fService);
        fService = MACH_PORT_NULL;
    }

    if (fDevice) 
	{
		if ( fNeedsToReleasefDevice )
			IOObjectRelease(fDevice);
        fDevice = MACH_PORT_NULL;
    }

	if (fAsyncPort)
	{
		IONotificationPortDestroy(fAsyncPort);
        fAsyncPort = NULL;
	}

	if (fASLClient)
		asl_close(fASLClient);
}

#pragma mark -

HRESULT 
IOUSBInterfaceClass::queryInterface(REFIID iid, void **ppv)
{
    DEBUGPRINT("IOUSBInterfaceClass[%p]::queryInterface\n", this);
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOCFPlugInInterfaceID)) 
    {
        *ppv = &iunknown;
        addRef();
    }
    else if (   CFEqual(uuid, kIOUSBInterfaceInterfaceID182)
             || CFEqual(uuid, kIOUSBInterfaceInterfaceID183)
             || CFEqual(uuid, kIOUSBInterfaceInterfaceID190)
             || CFEqual(uuid, kIOUSBInterfaceInterfaceID192)
			 || CFEqual(uuid, kIOUSBInterfaceInterfaceID197)
			 || CFEqual(uuid, kIOUSBInterfaceInterfaceID220)
			 || CFEqual(uuid, kIOUSBInterfaceInterfaceID245)
			 || CFEqual(uuid, kIOUSBInterfaceInterfaceID300)
             || CFEqual(uuid, kIOUSBInterfaceInterfaceID)
			 || CFEqual(uuid, kIOUSBInterfaceInterfaceID500)
			 )
    {
        *ppv = &fUSBInterface;
        addRef();
		if (   CFEqual(uuid, kIOUSBInterfaceInterfaceID182)
			   || CFEqual(uuid, kIOUSBInterfaceInterfaceID183)
			   || CFEqual(uuid, kIOUSBInterfaceInterfaceID190)
			   || CFEqual(uuid, kIOUSBInterfaceInterfaceID192)
			   || CFEqual(uuid, kIOUSBInterfaceInterfaceID197)
			   || CFEqual(uuid, kIOUSBInterfaceInterfaceID220)
			   || CFEqual(uuid, kIOUSBInterfaceInterfaceID) )
		{
			fNeedsToReleasefDevice = false;
		}
		else
		{
			// Version 245 fixes rdar://4418782, by releaseing our fDevice.  We need to make sure that
			// we only do that fix for 245 and above
			DEBUGPRINT("IOUSBInterfaceClass[%p]::queryInterface fixing rdar://4418782 by releasing fDevice in destructor\n", this);
			fNeedsToReleasefDevice = true;
		}
		
    }
    else
        *ppv = 0;

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
	if ( res )
	{
		DEBUGPRINT("-IOUSBInterfaceClass[%p]::queryInterface %" PRId32 "\n", this, (int32_t)res);
	}
	
    return res;
}



IOReturn 
IOUSBInterfaceClass::probe(CFDictionaryRef propertyTable, io_service_t inService, SInt32 *order)
{
#pragma unused (propertyTable, order)
    DEBUGPRINT("IOUSBInterfaceClass[%p]::probe\n", this);
    if (!inService || !IOObjectConformsTo(inService, "IOUSBInterface"))
        return kIOReturnBadArgument;

    return kIOReturnSuccess;
}


#pragma mark -

IOReturn 
IOUSBInterfaceClass::start(CFDictionaryRef propertyTable, io_service_t inService)
{
#pragma unused (propertyTable)
    IOReturn			res;
    uint64_t			output[1];
    uint32_t			len = 1;
	UInt32				type = 0;


    fNextCookie = 0;
    fConfigDescCacheValid = false;
    fInterfaceDescriptor = NULL;
    fConfigLength = 0;
    fUserBufferInfoListHead = NULL;
	
    res = IOServiceOpen(inService, mach_task_self(), type, &fConnection);
    if (res != kIOReturnSuccess)
        return res;

    if ( fConnection == MACH_PORT_NULL )
		return kIOReturnNoDevice;    

	fInterfaceIsAttached = true;
	
   // Make sure that we retain our service so that we can use it later on
    //
    res = IOObjectRetain(inService);
    if (res)
	{
		DEBUGPRINT("-IOUSBInterfaceClass::start  IOObjectRetain returned 0x%x\n", res);
        return res;
	}
    fService = inService;

	output[0] = 0;
	res = IOConnectCallScalarMethod(fConnection, kUSBInterfaceUserClientGetDevice, 0, 0, output, &len);

    if (res)
        fDevice = IO_OBJECT_NULL;
	else
		fDevice = (io_service_t) output[0];
    
	DEBUGPRINT("-IOUSBInterfaceClass::start  kUSBInterfaceUserClientGetDevice res = 0x%x, output[0] = 0x%qx, fDevice 0x%x", res, output[0], fDevice);
	res = GetPropertyInfo();
	
	DEBUGPRINT("-IOUSBInterfaceClass::start  0x%x, fDevice = 0x%x\n", res, fDevice);
    return res;
}



IOReturn 
IOUSBInterfaceClass::stop()
{
	IOReturn ret = kIOReturnSuccess;
	
    DEBUGPRINT("IOUSBInterfaceClass[%p]::stop\n", this);
	ATTACHEDCHECK();
	if (fIsOpen)
		ret = USBInterfaceClose();
	
	return ret;
}



IOReturn
IOUSBInterfaceClass::GetPropertyInfo(void)
{
    IOReturn				kr;
    CFMutableDictionaryRef 	entryProperties = NULL;
	CFTypeRef				val;
	SInt64					tempValue;
	
	
    DEBUGPRINT("+IOUSBInterfaceClass[%p]::GetPropertyInfo (fService: 0x%x, fDevice: 0x%x)", this, fService, fDevice);
    kr = IORegistryEntryCreateCFProperties(fService, &entryProperties, NULL, 0);
    
    if (kr)
	{
		DEBUGPRINT("+IOUSBInterfaceClass[%p]::GetPropertyInfo IORegistryEntryCreateCFProperties returned 0x%x", this, kr);
       return kr;
	}
        
    if (entryProperties && (CFGetTypeID(entryProperties) == CFDictionaryGetTypeID())) 
    {
		// The dictionary contains values that are signed 32 or 64 bit.  To avoid CF sign extensions issues read the values into a temp
		// SInt64 and then assign to the local ivar
		// (rdar://4951538 & rdar://5081728)
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBInterfaceClass));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fClass = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBInterfaceSubClass));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fSubClass = tempValue;
		}
		
		val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBInterfaceProtocol));
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
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBConfigurationValue));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fConfigValue = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBInterfaceNumber));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fInterfaceNumber = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBAlternateSetting));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fAlternateSetting = tempValue;
		}
		
		val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBNumEndpoints));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fNumEndpoints = tempValue;
		}
		
		val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBInterfaceStringIndex));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fStringIndex = tempValue;
		}
		
        val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDevicePropertyLocationID));
        if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
		{
            CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
			fLocationID = tempValue;
		}
		
        CFDataRef data = (CFDataRef) CFDictionaryGetValue(entryProperties, CFSTR("InterfaceDescriptor"));
        if (data && (CFGetTypeID(data) == CFDataGetTypeID()))
        {
            fInterfaceDescriptor = (IOUSBInterfaceDescriptor *) CFDataGetBytePtr( data );
        }      
        CFRelease(entryProperties);
		entryProperties = NULL;
    }
    
    // Look at the device's properties
    //
    kr = IORegistryEntryCreateCFProperties(fDevice, &entryProperties, NULL, 0);
	if (kr)
	{
 		DEBUGPRINT("+IOUSBInterfaceClass[%p]::GetPropertyInfo IORegistryEntryCreateCFProperties#2 returned 0x%x", this, kr);
		return kr;
	}
	
    if (entryProperties && (CFGetTypeID(entryProperties) == CFDictionaryGetTypeID())) 
    {
		// We only need to do the following once per instantiation
		if ( fConfigurations == NULL)
		{
			val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBDeviceNumConfigs));
			if (val && (CFGetTypeID(val) == CFNumberGetTypeID()))
			{
				CFNumberGetValue((CFNumberRef)val, kCFNumberLongLongType, &tempValue);
				fNumConfigurations = tempValue;
			}
			else
				fNumConfigurations = 0;
			
			if (fNumConfigurations)
			{
				fConfigurations = (IOUSBConfigurationDescriptorPtr*) malloc(fNumConfigurations * sizeof(IOUSBConfigurationDescriptorPtr));
				bzero(fConfigurations, fNumConfigurations * sizeof(IOUSBConfigurationDescriptorPtr));
			}
			
			val = CFDictionaryGetValue(entryProperties, CFSTR(kUSBControllerNeedsContiguousMemoryForIsoch));
			if (val && (CFGetTypeID(val) == CFBooleanGetTypeID()))
			{
				fNeedContiguousMemoryForLowLatencyIsoch = CFBooleanGetValue((CFBooleanRef) val);
			}
			else
				fNeedContiguousMemoryForLowLatencyIsoch = false;
			
			DEBUGPRINT("IOUSBInterfaceClass[%p]::GetPropertyInfo NeedContiguousMemoryForIsoch = %d\n", this, fNeedContiguousMemoryForLowLatencyIsoch);
		}
		
        CFRelease(entryProperties);
		entryProperties = NULL;
    }

    DEBUGPRINT("-IOUSBInterfaceClass[%p]::GetPropertyInfo", this);
    return kIOReturnSuccess;
}

IOReturn
IOUSBInterfaceClass::GetIOUSBLibVersion(NumVersion *ioUSBLibVersion, NumVersion *usbFamilyVersion)
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
    
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetIOUSBLibVersion = %" PRIx32 ", %" PRIx32 "\n", this, *(uint32_t *) ioUSBLibVersion, *(uint32_t *) usbFamilyVersion);
    return kIOReturnSuccess;
	
}


#pragma mark IORegistry Getters
IOReturn 
IOUSBInterfaceClass::GetInterfaceClass(UInt8 *intfClass)
{
    ATTACHEDCHECK();
    *intfClass = fClass;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetInterfaceClass = %d\n", this, *intfClass);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetInterfaceSubClass(UInt8 *intfSubClass)
{
    ATTACHEDCHECK();
    *intfSubClass = fSubClass;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetInterfaceSubClass = %d\n", this, *intfSubClass);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetInterfaceProtocol(UInt8 *intfProtocol)
{
    ATTACHEDCHECK();
    *intfProtocol = fProtocol;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetInterfaceProtocol = %d\n", this, *intfProtocol);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetInterfaceStringIndex(UInt8 *intfSI)
{
    ATTACHEDCHECK();
    *intfSI = fStringIndex;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetInterfaceStringIndex = %d\n", this, *intfSI);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetDeviceVendor(UInt16 *devVendor)
{
    ATTACHEDCHECK();
    *devVendor = fVendor;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetDeviceVendor = %d\n", this,*devVendor);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetDeviceProduct(UInt16 *devProduct)
{
    ATTACHEDCHECK();
    *devProduct = fProduct;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetDeviceProduct = %d\n", this, *devProduct);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetDeviceReleaseNumber(UInt16 *devRelNum)
{
    ATTACHEDCHECK();
    *devRelNum = fDeviceReleaseNumber;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetDeviceReleaseNumber = %d\n", this, *devRelNum);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetConfigurationValue(UInt8 *configVal)
{
    ATTACHEDCHECK();
    *configVal = fConfigValue;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetConfigurationValue = %d\n", this, *configVal);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetInterfaceNumber(UInt8 *intfNumber)
{
    ATTACHEDCHECK();
    *intfNumber = fInterfaceNumber;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetInterfaceNumber = %d\n", this, *intfNumber);
    return kIOReturnSuccess;
}


IOReturn
IOUSBInterfaceClass::GetAlternateSetting(UInt8 *intfAlternateSetting)
{ 
    ATTACHEDCHECK();
    *intfAlternateSetting = fAlternateSetting;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetAlternateSetting = %d\n", this, *intfAlternateSetting);
    return kIOReturnSuccess;
}


IOReturn
IOUSBInterfaceClass::GetNumEndpoints(UInt8 *intfNumEndpoints)
{ 
    ATTACHEDCHECK();
    *intfNumEndpoints = fNumEndpoints;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetNumEndpoints = %d\n", this, *intfNumEndpoints);
    return kIOReturnSuccess;
}


IOReturn
IOUSBInterfaceClass::GetLocationID(UInt32 *locationID)
{ 
    ATTACHEDCHECK();
    *locationID = fLocationID;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetLocationID\n = 0x%" PRIx32 "", this, (uint32_t) *locationID);
    return kIOReturnSuccess;
}


IOReturn 
IOUSBInterfaceClass::GetDevice(io_service_t *device)
{    
    ATTACHEDCHECK();
    *device = fDevice;
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetDevice = 0x%x\n", this, *device);
    return kIOReturnSuccess;
}


#pragma mark IOUSBInterface 

IOReturn 
IOUSBInterfaceClass::CreateInterfaceAsyncEventSource(CFRunLoopSourceRef *source)
{
    IOReturn			ret;
    CFMachPortContext	context;
    CFMachPortRef		cfPort;
    Boolean				shouldFreeInfo;

    DEBUGPRINT("IOUSBInterfaceClass[%p]::CreateInterfaceAsyncEventSource\n", this);
	if ( fCFSource )
	{
		DEBUGPRINT("IOUSBInterfaceClass[%p]::CreateInterfaceAsyncEventSource already had a CFSource!", this);
		
		// Since we are returning our fCFSource and this is a Create call, be sure to retain it
		CFRetain(fCFSource);
        *source = fCFSource;
		return kIOReturnSuccess;
	}
	
    if (!fAsyncPort) 
    {     
    	// This CreateDeviceAsyncPort() will create the fCFSource
        ret = CreateInterfaceAsyncPort(0);
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
			uint32_t			len = 0;
			
			kr = IOConnectCallAsyncScalarMethod(fConnection, kUSBInterfaceUserClientSetAsyncPort, IONotificationPortGetMachPort(fAsyncPort), asyncRef, 1, 0, 0, 0, &len);
			if (kr == MACH_SEND_INVALID_DEST)
			{
				fIsOpen = false;
				fInterfaceIsAttached = false;
				kr = kIOReturnNoDevice;
			}
		}
    }
    
    return kr;
}



mach_port_t 
IOUSBInterfaceClass::GetInterfaceAsyncPort()
{
	if ( fAsyncPort == MACH_PORT_NULL )
		return MACH_SEND_INVALID_DEST;
	else
		return IONotificationPortGetMachPort(fAsyncPort);
}



IOReturn 
IOUSBInterfaceClass::USBInterfaceOpen(bool seize)
{

    IOReturn ret = kIOReturnSuccess;
	
	DEBUGPRINT("+IOUSBInterfaceClass[%p]::USBInterfaceOpen\n", this);
    ATTACHEDCHECK();
	
    if (fIsOpen)
        return kIOReturnSuccess;
	
    uint32_t	len = 0;
    uint64_t	input = (uint64_t) seize;
	
    ret = IOConnectCallScalarMethod(fConnection, kUSBInterfaceUserClientOpen, &input, 1, 0, &len); 
	
    if (ret == kIOReturnSuccess)
	{
		fIsOpen = true;
		
		if (fAsyncPort) 
		{
			io_async_ref64_t    asyncRef;
			
			ret = IOConnectCallAsyncScalarMethod(fConnection, kUSBInterfaceUserClientSetAsyncPort, IONotificationPortGetMachPort(fAsyncPort), asyncRef, 1, 0, 0, 0, &len);
			if ( (ret != kIOReturnSuccess) and (ret != MACH_SEND_INVALID_DEST ))
			{
				USBInterfaceClose();
			}
		}
    }
	
    if (ret == MACH_SEND_INVALID_DEST)
	{
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	
	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBInterfaceClass[%p]::USBInterfaceOpen returns 0x%x\n", this, ret);
	}
    return ret;
}



IOReturn 
IOUSBInterfaceClass::USBInterfaceClose()
{
    IOReturn		ret = kIOReturnSuccess;
    LowLatencyUserBufferInfoV3 *	buffer;
    LowLatencyUserBufferInfoV3 *	nextBuffer;
    
    DEBUGPRINT("+IOUSBInterfaceClass::USBInterfaceClose\n");

    mach_msg_type_number_t len = 0;
    fIsOpen = false;

    if ( fInterfaceIsAttached )
    {
		ret = IOConnectCallScalarMethod(fConnection, kUSBInterfaceUserClientClose,0, 0, 0, 0); 
        if (ret == MACH_SEND_INVALID_DEST)
        {
            fIsOpen = false;
            fInterfaceIsAttached = false;
            ret = kIOReturnNoDevice;
        }
    }

    // Need to free any buffers that has been allocated by the low latency stuff that has not been
    // released!
    //
    if (fUserBufferInfoListHead != NULL)
    {
        nextBuffer = fUserBufferInfoListHead;
        buffer = fUserBufferInfoListHead;
        DEBUGPRINT("fUserBufferInfoListHead != NULL: %p, next: %p\n", fUserBufferInfoListHead, buffer->nextBuffer);
        
        // Traverse the list and release memory
        //
        while ( nextBuffer != NULL )
        {
            nextBuffer = (LowLatencyUserBufferInfoV3*) buffer->nextBuffer;
            
            DEBUGPRINT("Releasing 0x%qx, %p\n", buffer->bufferAddress, buffer );
            free ( (void *) buffer->bufferAddress );
            free ( buffer );
            
            buffer = nextBuffer;
        }
        
        fUserBufferInfoListHead = NULL;
    }
 
    return ret;
}


#pragma mark -


IOReturn 
IOUSBInterfaceClass::SetAlternateInterface(UInt8 alternateSetting)
{    
    uint64_t			in[1];
    size_t				len = 1;
    IOReturn			kr = kIOReturnSuccess;
	
    DEBUGPRINT("IOUSBInterfaceClass::SetAlternateInterface to %d\n", alternateSetting);

    ALLCHECKS();

 	in[0] = (uint64_t) alternateSetting;
	kr = IOConnectCallScalarMethod(fConnection, kUSBInterfaceUserClientSetAlternateInterface, in, 1, NULL, NULL); 

	if (kr == kIOReturnSuccess)
		kr = GetPropertyInfo();
    
	if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
	
	if ( kr != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBInterfaceClass::SetConfiguration returning 0x%x\n", kr);
	}
	
    return kr;
}


IOReturn
IOUSBInterfaceClass::GetBusMicroFrameNumber(UInt64 *microFrame, AbsoluteTime *atTime)
{
    IOUSBGetFrameStruct 	frameInfo;
    IOReturn				ret;
    size_t					len;
	
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetBusMicroFrameNumber\n", this);

    ATTACHEDCHECK();
	
	len = sizeof(IOUSBGetFrameStruct);
    ret = IOConnectCallStructMethod(fConnection, kUSBInterfaceUserClientGetMicroFrameNumber, 0, 0, &frameInfo, &len);
    if (kIOReturnSuccess == ret)
    {
#if !TARGET_OS_EMBEDDED
#endif
		{
			*microFrame = frameInfo.frame;
			*atTime = frameInfo.timeStamp;
		}
    }
    if (ret == MACH_SEND_INVALID_DEST)
    {
        fIsOpen = false;
        fInterfaceIsAttached = false;
        ret = kIOReturnNoDevice;
    }
    return ret;
}



IOReturn
IOUSBInterfaceClass::GetBusFrameNumber(UInt64 *frame, AbsoluteTime *atTime)
{
    IOUSBGetFrameStruct 	frameInfo;
    IOReturn				ret;
    size_t					len;
	
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetBusFrameNumber\n", this);

    ATTACHEDCHECK();
	
	len = sizeof(IOUSBGetFrameStruct);
    ret = IOConnectCallStructMethod(fConnection, kUSBInterfaceUserClientGetFrameNumber, 0, 0, &frameInfo, &len);

    if (kIOReturnSuccess == ret) 
    {
#if !TARGET_OS_EMBEDDED
#endif
		{
			*frame = frameInfo.frame;
			*atTime = frameInfo.timeStamp;
		}
    }
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
 	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBInterfaceClass[%p]::GetBusFrameNumber returns 0x%x\n", this, ret);
	}
    return ret;
}


IOReturn
IOUSBInterfaceClass::GetBusFrameNumberWithTime(UInt64 *frame, AbsoluteTime *atTime)
{
    IOUSBGetFrameStruct 	frameInfo;
    IOReturn				ret;
    size_t					len;
	
    DEBUGPRINT("IOUSBInterfaceClass[%p]::GetBusFrameNumberWithTime\n", this);

    ATTACHEDCHECK();
	
	len = sizeof(IOUSBGetFrameStruct);
    ret = IOConnectCallStructMethod(fConnection, kUSBInterfaceUserClientGetFrameNumberWithTime, 0, 0, (void *) &frameInfo, &len);
    if (kIOReturnSuccess == ret) 
    {
#if !TARGET_OS_EMBEDDED
#endif
		{
			*frame = frameInfo.frame;
			*atTime = frameInfo.timeStamp;
		}
    }
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
 	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBInterfaceClass[%p]::GetBusFrameNumberWithTime returns 0x%x\n", this, ret);
	}
    return ret;
}

IOReturn
IOUSBInterfaceClass::GetFrameListTime(UInt32 *microsecondsInFrame)
{
    uint64_t			output[1];
    uint32_t			len = 1;
    IOReturn			ret = kIOReturnSuccess;
	
	DEBUGPRINT("+IOUSBInterfaceClass[%p]::GetFrameListTime\n", this);
	
	ATTACHEDCHECK();
	
 	output[0] = 0;
    ret = IOConnectCallScalarMethod(fConnection, kUSBInterfaceUserClientGetFrameListTime, 0, 0, output, &len);
 	if (ret == kIOReturnSuccess)
	{
		*microsecondsInFrame = (UInt32) output[0];
	}
	
	if (ret == MACH_SEND_INVALID_DEST)
    {
        fIsOpen = false;
        fInterfaceIsAttached = false;
        ret = kIOReturnNoDevice;
    }
	
 	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("+IOUSBInterfaceClass[%p]::GetFrameListTime returns 0x%x\n", this, ret);
	}
    return ret;
}



IOReturn
IOUSBInterfaceClass::GetBandwidthAvailable(UInt32 *bandwidth)
{
    uint64_t			output[1];
    uint32_t			len = 1;
    IOReturn			ret = kIOReturnSuccess;
	
	DEBUGPRINT("+IOUSBInterfaceClass[%p]::GetBandwidthAvailable\n", this);

	ATTACHEDCHECK();
	
	output[0] = 0;
	ret = IOConnectCallScalarMethod(fConnection, kUSBInterfaceUserClientGetBandwidthAvailable, 0, 0, output, &len);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }

	if (ret == kIOReturnSuccess)
	{
		*bandwidth = (UInt32) output[0];
	}
	
	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBInterfaceClass[%p]::GetBandwidthAvailable returning 0x%x\n", this, ret);
	}

	DEBUGPRINT("-IOUSBInterfaceClass[%p]::GetBandwidthAvailable returning %" PRIu32 "\n", this, (uint32_t)*bandwidth);

    return ret;
}



IOReturn
IOUSBInterfaceClass::GetEndpointProperties(UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval)
{
    uint64_t			output[3];
    uint64_t			input[3];
    uint32_t			outLen = 3;
    IOReturn			ret = kIOReturnSuccess;
	
	DEBUGPRINT("+IOUSBInterfaceClass[%p]::GetEndpointProperties\n", this);

	ATTACHEDCHECK();
	
    input[0] = (uint64_t) alternateSetting;
    input[1] = (uint64_t) endpointNumber;
    input[2] = (uint64_t) direction;
	
	output[0] = 0;
	output[1] = 0;
	output[2] = 0;
	
	ret = IOConnectCallScalarMethod( fConnection, kUSBInterfaceUserClientGetEndpointProperties,  input, 3, output, &outLen);
	
    if (ret == kIOReturnSuccess)
    {
		*transferType = (UInt8) output[0];
		*maxPacketSize = (UInt16) output[1];
		*interval = (UInt8) output[2];
    }
	
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }

	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBInterfaceClass[%p]::GetEndpointProperties returning 0x%x\n", this, ret);
	}
	
    DEBUGPRINT("-IOUSBInterfaceClass[%p]::GetEndpointProperties returned 0x%x for altSetting: %d, endpointNumber: %d, direction %d, transferType: %d, mps: 0x%x, interval %d\n",
			   this, ret, alternateSetting, endpointNumber, direction, *transferType, *maxPacketSize, *interval);
    return ret;
}


#pragma mark Control Request
IOReturn
IOUSBInterfaceClass::ControlRequest(UInt8 pipeRef, IOUSBDevRequestTO *req)
{
    IOReturn 		ret = kIOReturnSuccess;
    uint64_t		input[9];
    uint64_t		output[1];
	uint32_t		outputCnt = 1;
    size_t			len;
    
	DEBUGPRINT("IOUSBInterfaceClass::ControlRequest\n");

	ATTACHEDCHECK();
	
 	DEBUGPRINT("IOUSBInterfaceClass::ControlRequest(pipeRef: %d): \n\tbmRequestType = 0x%2.2x\n\tbRequest = 0x%2.2x\n\twValue = 0x%4.4x\n\twIndex = 0x%4.4x\n\twLength = 0x%4.4x\n\tpData = %p\n\tnoDataTimeout = %" PRIu32 "\n\tcompletionTimeout = %" PRIu32 "\n",
	pipeRef,
	req->bmRequestType,
	req->bRequest,
	req->wValue,
	req->wIndex,
	req->wLength,
	req->pData,
	(uint32_t) req->noDataTimeout,
	(uint32_t) req->completionTimeout);
	
	input[0] = (uint64_t) pipeRef;
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
		
	switch ((req->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask)
	{
		case kUSBOut:
			ret = IOConnectCallMethod( fConnection, kUSBInterfaceUserClientControlRequestOut, input, 9, req->pData, len, 0, 0, 0, 0);
			if (kIOReturnSuccess == ret)
				req->wLenDone = req->wLength;
			else
				req->wLenDone = 0;
			break;
			
		case kUSBIn:
			output[0] = 0;
			ret = IOConnectCallMethod( fConnection, kUSBInterfaceUserClientControlRequestIn, input, 9, 0, 0, output, &outputCnt, req->pData, &len);
			if (kIOReturnSuccess == ret)
				req->wLenDone = len;
			else
				req->wLenDone = 0;
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
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
 	
	DEBUGPRINT("IOUSBInterfaceClass::ControlRequest returning 0x%x, req->wLenDone = %" PRIu32 "\n", ret, (uint32_t) req->wLenDone);
	
	return ret;
}


IOReturn
IOUSBInterfaceClass::ControlRequestAsync(UInt8 pipeRef, IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon)
{
	io_async_ref64_t    asyncRef;
	size_t				len;
    IOReturn			ret = kIOReturnSuccess;
    uint64_t			input[9];
    uint64_t		output[1];	// Not used for async, but sync expects it, so async must too.
	uint32_t		outputCnt = 1;
	
	DEBUGPRINT("IOUSBInterfaceClass::DeviceRequestAsync(pipe: %d): \n\tbmRequestType = 0x%2.2x\n\tbRequest = 0x%2.2x\n\twValue = 0x%4.4x\n\twIndex = 0x%4.4x\n\twLength = 0x%4.4x\n\tpData = %p\n\tnoDataTimeout = %" PRIu32 "\n\tcompletionTimeout = %" PRIu32 "\n",
	pipeRef,
	req->bmRequestType,
	req->bRequest,
	req->wValue,
	req->wIndex,
	req->wLength,
	req->pData,
	(uint32_t) req->noDataTimeout,
	(uint32_t) req->completionTimeout);
	
    if (!fAsyncPort)
	{
		DEBUGPRINT("IOUSBInterfaceClass::ControlRequestAsync  NO async port\n");
        return kIOUSBNoAsyncPortErr;
	}
	
	ATTACHEDCHECK();
	
	input[0] = (uint64_t) pipeRef;
	input[1] = (uint64_t) req->bmRequestType;
	input[2] = (uint64_t) req->bRequest;
	input[3] = (uint64_t) req->wValue;
	input[4] = (uint64_t) req->wIndex;
	input[5] = (uint64_t) req->wLength;
	input[6] = (uint64_t) req->pData;
	input[7] = (uint64_t) req->noDataTimeout;
	input[8] = (uint64_t) req->completionTimeout;
	
	asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t) refCon;
	
    
    switch ((req->bmRequestType >> kUSBRqDirnShift) & kUSBRqDirnMask)
    {
	case kUSBOut:
		ret = IOConnectCallAsyncScalarMethod( fConnection, kUSBInterfaceUserClientControlRequestOut, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, input, 9, 0, 0);
		break;
		
	case kUSBIn:
		ret = IOConnectCallAsyncScalarMethod( fConnection, kUSBInterfaceUserClientControlRequestIn, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, input, 9, output, &outputCnt);
		break;
    }
	
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
 	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBInterfaceClass::ControlRequestAsync returning 0x%x\n", ret);
	}
    return ret;
}


#pragma mark Pipe methods


IOReturn
IOUSBInterfaceClass::ReadPipe(UInt8 pipeRef, void *buf, UInt32 *size, UInt32 noDataTimeout, UInt32 completionTimeout)
{
    IOReturn 		ret = kIOReturnSuccess;
    uint64_t		input[5];
    size_t			len;
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::ReadPipe to pipe %d, length %" PRIu32 ", buf: %p, noDataTimeout: %" PRIu32 ", completionTimeout %" PRIu32 "\n",  this, pipeRef, (uint32_t) *size, buf, (uint32_t) noDataTimeout, (uint32_t) completionTimeout);
	
    ALLCHECKS();
	
	input[0] = (uint64_t) pipeRef;
	input[1] = (uint64_t) noDataTimeout;
	input[2] = (uint64_t) completionTimeout;
	input[3] = 0;
	input[4] = 0;
	
	len = *size;
	
	ret = IOConnectCallMethod( fConnection, kUSBInterfaceUserClientReadPipe, input, 5, 0, 0, 0, 0, buf, &len);
	
    if (ret == kIOReturnSuccess)
	{
		*size = (UInt32) len;
	}
	
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::ReadPipe  returning error 0x%x,  size: 0x%" PRIu32 "\n", this, ret, (uint32_t) *size);
	
	return ret;
}


IOReturn
IOUSBInterfaceClass::ReadPipeAsync(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refCon)
{
	io_async_ref64_t    asyncRef;
	size_t				len;
    IOReturn			ret;
    uint64_t			input[5];
	
    if (!fAsyncPort)
	{
		DEBUGPRINT("IOUSBInterfaceClass[%p]::ReadPipeAsync  NO async port\n", this);
        return kIOUSBNoAsyncPortErr;
	}
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::ReadPipeAsync to pipe %d, length %" PRIu32 ", buf: %p, noDataTimeout: %" PRIu32 ", completionTimeout %" PRIu32 ", refCon: %p\n", this, pipeRef, (uint32_t) size, buf, (uint32_t) noDataTimeout, (uint32_t) completionTimeout, refCon);
	
    ALLCHECKS();
	
	input[0] = (uint64_t) pipeRef;
	input[1] = (uint64_t) noDataTimeout;
	input[2] = (uint64_t) completionTimeout;
	input[3] = (uint64_t) buf;
	input[4] = (uint64_t) size;
	
    asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t) refCon;
	
	ret = IOConnectCallAsyncScalarMethod( fConnection, kUSBInterfaceUserClientReadPipe, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, input, 5, 0, 0);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::ReadPipeAsync returning 0x%x\n", this, ret);
	
	return ret;
}




IOReturn
IOUSBInterfaceClass::WritePipe(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout)
{
    IOReturn 		ret = kIOReturnSuccess;
    uint64_t		input[5];
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::WritePipe to pipe %d, length %" PRIu32 ", buf: %p, noDataTimeout: %" PRIu32 ", completionTimeout %" PRIu32 "\n",  this, pipeRef, (uint32_t) size, buf, (uint32_t) noDataTimeout, (uint32_t) completionTimeout);
	
    ALLCHECKS();
	
	input[0] = (uint64_t) pipeRef;
	input[1] = (uint64_t) noDataTimeout;
	input[2] = (uint64_t) completionTimeout;
	input[3] = 0;
	input[4] = 0;
	
	ret = IOConnectCallMethod( fConnection, kUSBInterfaceUserClientWritePipe, input, 5, buf, size, 0, 0, 0, 0);
	
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::WritePipe  returning error 0x%x\n", this, ret);
	
	return ret;
}



IOReturn 
IOUSBInterfaceClass::WritePipeAsync(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refCon)
{
	io_async_ref64_t    asyncRef;
    IOReturn			ret;
    uint64_t			input[5];
	
    if (!fAsyncPort)
	{
		DEBUGPRINT("IOUSBInterfaceClass[%p]::WritePipeAsync  NO async port\n", this);
        return kIOUSBNoAsyncPortErr;
	}
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::WritePipeAsync to pipe %d, length %" PRIu32 ", buf: %p, noDataTimeout: %" PRIu32 ", completionTimeout %" PRIu32 ", refCon: %p\n", this, pipeRef, (uint32_t) size, buf, (uint32_t) noDataTimeout, (uint32_t) completionTimeout, refCon);

    ALLCHECKS();
	
	input[0] = (uint64_t) pipeRef;
	input[1] = (uint64_t) noDataTimeout;
	input[2] = (uint64_t) completionTimeout;
	input[3] = (uint64_t) buf;
	input[4] = (uint64_t) size;
	
    asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t) refCon;
	
	ret = IOConnectCallAsyncScalarMethod( fConnection, kUSBInterfaceUserClientWritePipe, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, input, 5, 0, 0);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::WritePipeAsync returning 0x%x\n", this, ret);
	
	return ret;
}


IOReturn 
IOUSBInterfaceClass::GetPipeProperties(UInt8 pipeRef, UInt8 *direction, UInt8 *number, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval)
{
    uint64_t			output[5];
    uint64_t			input[1];
    uint32_t			outLen = 5;
    IOReturn			ret = kIOReturnSuccess;
    
    DEBUGPRINT("+IOUSBInterfaceClass[%p]::GetPipeProperties\n", this);

    ALLCHECKS();

	input[0] = (uint64_t) pipeRef;

	output[0] = 0;
	output[1] = 0;
	output[2] = 0;
	output[3] = 0;
	output[4] = 0;
	
	ret = IOConnectCallScalarMethod( fConnection, kUSBInterfaceUserClientGetPipeProperties,  input, 1, output, &outLen);
    if (ret == kIOReturnSuccess)
    {
		*direction		= (UInt8) output[0];
		*number			= (UInt8) output[1];
		*transferType	= (UInt8) output[2];
		*maxPacketSize	= (UInt16) output[3];
		*interval		= (UInt8) output[4];
    }
	
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBInterfaceClass[%p]::GetPipeProperties returning 0x%x\n", this, ret);
	}
	
    DEBUGPRINT("-IOUSBInterfaceClass[%p]::GetPipeProperties err = 0x%x, pipeRef: %d, direction %d, number: %d, transferType: %d, mps: 0x%x, interval %d\n",
			   this, ret, pipeRef, *direction, *number, *transferType, *maxPacketSize, *interval);
    return ret;
}


IOReturn 
IOUSBInterfaceClass::GetPipePropertiesV2(UInt8 pipeRef, UInt8 *direction, UInt8 *number, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval, UInt8 *maxBurst, UInt8 *mult, UInt16 *bytesPerInterval)
{
    uint64_t			output[8];
    uint64_t			input[1];
    uint32_t			outLen = 8;
    IOReturn			ret = kIOReturnSuccess;
    
    DEBUGPRINT("+IOUSBInterfaceClass[%p]::GetPipePropertiesV2\n", this);
	
    ALLCHECKS();
	
	input[0] = (uint64_t) pipeRef;
	
	output[0] = 0;
	output[1] = 0;
	output[2] = 0;
	output[3] = 0;
	output[4] = 0;
	output[5] = 0;
	output[6] = 0;
	output[7] = 0;
	
	ret = IOConnectCallScalarMethod( fConnection, kUSBInterfaceUserClientGetPipePropertiesV2,  input, 1, output, &outLen);
    if (ret == kIOReturnSuccess)
    {
		*direction			= (UInt8) output[0];
		*number				= (UInt8) output[1];
		*transferType		= (UInt8) output[2];
		*maxPacketSize		= (UInt16) output[3];
		*interval			= (UInt8) output[4];
		*maxBurst			= (UInt8) output[5];
		*mult				= (UInt8) output[6];
		*bytesPerInterval	= (UInt16) output[7];
    }
	else
	{
		*direction			= 0;
		*number				= 0;
		*transferType		= 0;
		*maxPacketSize		= 0;
		*interval			= 0;
		*maxBurst			= 0;
		*mult				= 0;
		*bytesPerInterval	= 0;
	}
	
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	if ( ret != kIOReturnSuccess )
	{
		DEBUGPRINT("IOUSBInterfaceClass[%p]::GetPipePropertiesV2 returning 0x%x\n", this, ret);
	}
	
    DEBUGPRINT("-IOUSBInterfaceClass[%p]::GetPipePropertiesV2 err = 0x%x, pipeRef: %d, direction %d, number: %d, transferType: %d, mps: 0x%x, interval: %d, maxBurst: %d, mult: %d, bytesPerInterval: %d\n",
			   this, ret, pipeRef, *direction, *number, *transferType, *maxPacketSize, *interval, *maxBurst, *mult, *bytesPerInterval);
    return ret;
}

IOReturn
IOUSBInterfaceClass::GetPipeStatus(UInt8 pipeRef)
{
    uint64_t			input[1];
    IOReturn			kr = kIOReturnSuccess;
    
	DEBUGPRINT("IOUSBInterfaceClass[%p]::GetPipeStatus\n", this);
	
	ALLCHECKS();
	
	input[0] = (uint64_t) pipeRef;
	
	kr = IOConnectCallScalarMethod(fConnection, kUSBInterfaceUserClientGetPipeStatus, input, 1, 0, 0);
    if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::GetPipeStatus returning 0x%x\n",this,  kr);
    
	return kr;
}



IOReturn
IOUSBInterfaceClass::AbortPipe(UInt8 pipeRef)
{
    uint64_t			input[1];
    IOReturn			kr = kIOReturnSuccess;
    
	DEBUGPRINT("IOUSBInterfaceClass::AbortPipe\n");
	
	ALLCHECKS();
	
	input[0] = (uint64_t) pipeRef;
	
	kr = IOConnectCallScalarMethod(fConnection, kUSBInterfaceUserClientAbortPipe, input, 1, 0, 0);
    if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
	
	DEBUGPRINT("IOUSBInterfaceClass::AbortPipe returning 0x%x\n", kr);
    
	return kr;
}



IOReturn
IOUSBInterfaceClass::ResetPipe(UInt8 pipeRef)
{
    uint64_t			input[1];
    IOReturn			kr = kIOReturnSuccess;
    
	DEBUGPRINT("IOUSBInterfaceClass::ResetPipe\n");
	
	ALLCHECKS();
	
	input[0] = (uint64_t) pipeRef;
	
	kr = IOConnectCallScalarMethod(fConnection, kUSBInterfaceUserClientResetPipe, input, 1, 0, 0);
    if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
	
	DEBUGPRINT("IOUSBInterfaceClass::ResetPipe returning 0x%x\n", kr);
    
	return kr;

}



IOReturn
IOUSBInterfaceClass::ClearPipeStall(UInt8 pipeRef, bool bothEnds)
{
    uint64_t			input[2];
    IOReturn			kr = kIOReturnSuccess;
    
	DEBUGPRINT("IOUSBInterfaceClass::ClearPipeStall\n");
	
	ALLCHECKS();
	
	input[0] = (uint64_t) pipeRef;
	input[1] = (uint64_t) bothEnds;
	
	kr = IOConnectCallScalarMethod(fConnection, kUSBInterfaceUserClientClearPipeStall, input, 2, 0, 0);
    if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
	
	DEBUGPRINT("IOUSBInterfaceClass::ClearPipeStall returning 0x%x\n", kr);
    
	return kr;
}



IOReturn
IOUSBInterfaceClass::SetPipePolicy(UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval)
{
	uint64_t			input[3];
    IOReturn			kr = kIOReturnSuccess;
    
	DEBUGPRINT("IOUSBInterfaceClass::SetPipePolicy (%d, %d, %d)\n", pipeRef, maxPacketSize, maxInterval);
	
	ALLCHECKS();
	
	input[0] = (uint64_t) pipeRef;
	input[1] = (uint64_t) maxPacketSize;
	input[2] = (uint64_t) maxInterval;
	
	kr = IOConnectCallScalarMethod(fConnection, kUSBInterfaceUserClientSetPipePolicy, input, 3, 0, 0);
    if (kr == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		kr = kIOReturnNoDevice;
    }
	
	DEBUGPRINT("IOUSBInterfaceClass::SetPipePolicy returning 0x%x\n", kr);
    
	return kr;
}


#pragma mark Isoch
IOReturn 
IOUSBInterfaceClass::ReadIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refCon)
{
	io_async_ref64_t    asyncRef;
    IOReturn			ret;
    UInt32				i, total;
	uint64_t			input[6];

	if (!fAsyncPort)
	{
		DEBUGPRINT("IOUSBInterfaceClass[%p]::ReadIsochPipeAsync  NO async port\n", this);
        return kIOUSBNoAsyncPortErr;
	}
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::ReadIsochPipeAsync\n", this);
	
	ALLCHECKS();
	
    total = 0;
    for(i=0; i < numFrames; i++)
	{
        total += frameList[i].frReqCount;
	}
	
	input[0] = (uint64_t) pipeRef;
	input[1] = (uint64_t) buf;
	input[2] = (uint64_t) total;
	input[3] = (uint64_t) frameStart;
	input[4] = (uint64_t) numFrames;
	input[5] = (uint64_t) frameList;
	
#if !TARGET_OS_EMBEDDED
#endif
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::ReadIsochPipeAsync  pipe: %d, buf: %p, total: %" PRIu32 ", frameStart: %qd, numFrames: %" PRIu32 ", frameListPtr: %p\n",
			   this, pipeRef, buf, (uint32_t) total, frameStart, (uint32_t) numFrames, frameList);
	
    asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t) refCon;
	
	ret = IOConnectCallAsyncScalarMethod( fConnection, kUSBInterfaceUserClientReadIsochPipe, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, 
									input, 6, 0, 0);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	if ( ret )
	{
		DEBUGPRINT("IOUSBInterfaceClass[%p]::ReadIsochPipeAsync returning 0x%x\n", this, ret);
	}
	
    return ret;
	
}

IOReturn 
IOUSBInterfaceClass::WriteIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refCon)
{
	io_async_ref64_t    asyncRef;
    IOReturn			ret;
    UInt32				i, total;
	uint64_t			input[6];
	
	if (!fAsyncPort)
	{
		DEBUGPRINT("IOUSBInterfaceClass[%p]::WriteIsochPipeAsync  NO async port\n", this);
        return kIOUSBNoAsyncPortErr;
	}
	
	DEBUGPRINT("IOUSBInterfaceClass[%p]::WriteIsochPipeAsync\n", this);
	
    ALLCHECKS();

    total = 0;
    for(i=0; i < numFrames; i++)
        total += frameList[i].frReqCount;
	
	input[0] = (uint64_t) pipeRef;
	input[1] = (uint64_t) buf;
	input[2] = (uint64_t) total;
	input[3] = (uint64_t) frameStart;
	input[4] = (uint64_t) numFrames;
	input[5] = (uint64_t) frameList;

#if !TARGET_OS_EMBEDDED
#endif

	DEBUGPRINT("IOUSBInterfaceClass[%p]::WriteIsochPipeAsync  pipe: %d, buf: %p, total: %" PRIu32 ", frameStart: 0x%qx, numFrames: %" PRIu32 ", frameListPtr: %p\n",
			   this, pipeRef, buf, (uint32_t) total, frameStart, (uint32_t) numFrames, frameList);

    asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t) refCon;
		
	ret = IOConnectCallAsyncScalarMethod( fConnection, kUSBInterfaceUserClientWriteIsochPipe, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, 
								   input, 6, 0, 0);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }
	if ( ret )
	{
		DEBUGPRINT("IOUSBInterfaceClass[%p]::WriteIsochPipeAsync returning 0x%x\n", this, ret);
	}
    return ret;
}


#pragma mark Low Latench Isoch


IOReturn 
IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refCon)
{
	io_async_ref64_t				asyncRef;
    uint32_t						i, total;
    IOReturn						ret;
    LowLatencyUserBufferInfoV3 *	dataBufferInfo;
    LowLatencyUserBufferInfoV3 *	frameListData;
	uint64_t						input[9];
	uint64_t						frameList64 = (uint64_t) frameList;
	
	if (!fAsyncPort)
	{
		DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync  NO async port\n");
        return kIOUSBNoAsyncPortErr;
	}
	
	DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync (buffer: %p)\n", buf);
	
    ALLCHECKS();
		
    total = 0;
    for ( i=0; i < numFrames; i++)
	{
		// DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync  frReqCount[%ld]: %d\n", i, frameList[i].frReqCount);
       	total += frameList[i].frReqCount;
	}
	
	// IOUSBLowLatencyIsocStruct
	//    UInt32 			fPipe;						// Input 0
	//    UInt32 			fBufSize;					// input 1
	//    UInt64 			fStartFrame;				// input 2
	//    UInt32 			fNumFrames;					// input 3
	//    UInt32			fUpdateFrequency;			// input 4
	//    UInt32			fDataBufferCookie;			// input 5
	//    UInt32			fDataBufferOffset;			// input 6
	//    UInt32			fFrameListBufferCookie;		// input 7
	//    UInt32			fFrameListBufferOffset;		// Input 8
	//
	input[0] = (uint64_t) pipeRef;
    input[1] = (uint64_t) total;
    input[2] = (uint64_t) frameStart;
    input[3] = (uint64_t) numFrames;
    input[4] = (uint64_t) updateFrequency;
	
	// Find the data buffer in our list of buffers
    //
    dataBufferInfo = FindBufferAddressRangeInList( buf, total);
    if ( dataBufferInfo != NULL )
    {
        input[5] = (uint64_t) dataBufferInfo->cookie;
        input[6] = (uint64_t) ((uint64_t)buf - dataBufferInfo->bufferAddress);
		
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync(buffer: %p)  Found DataBufferInfo for buffer %p: info: %p, offset = %qd, cookie: %qd\n", buf, buf, dataBufferInfo, input[6], input[5]);
    }
    else
    {
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync(buffer: %p) Ooops, couldn't find buffer %p in our list\n", buf, buf);
        return kIOUSBLowLatencyBufferNotPreviouslyAllocated;
    }
    
    // Find the frame list buffer in our list of buffers
    //
    frameListData = FindBufferAddressRangeInList( (void *)frameList64, sizeof (IOUSBLowLatencyIsocFrame) * numFrames);
    if ( frameListData != NULL )
    {
        input[7] = (uint64_t) frameListData->cookie;
        input[8] = (uint64_t) (frameList64 - frameListData->bufferAddress);
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync(buffer: %p)  Found frameListData for buffer 0x%qx: data: %p, offset = %qd, cookie : %qd\n", buf, frameList64, frameListData, input[8], input[7]);
    }
    else
    {
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync(buffer: %p)  Ooops, couldn't find buffer %p in our list\n", buf, frameList);
        return kIOUSBLowLatencyFrameListNotPreviouslyAllocated;
    }
	
	DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync(buffer: %p)  pipe: %d, total: 0x%x, frameStart: 0x%qx, numFrames: %d, updateFrequency: %d, dataCookie: %qd, dataOffset: %qd, frameListCookie: %qd, frameListOffset: %qd\n", 
			   buf, pipeRef, total, frameStart, (uint32_t)numFrames, (uint32_t)updateFrequency, input[5], input[6], input[7], input[8]);
	
#if !TARGET_OS_EMBEDDED
#endif
    asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t) refCon;
	
	ret = IOConnectCallAsyncScalarMethod( fConnection, kUSBInterfaceUserClientLowLatencyReadIsochPipe, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, 
								   input, 9, 0, 0);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }

	DEBUGPRINT("IOUSBInterfaceClass::LowLatencyReadIsochPipeAsync(buffer: %p)  returning 0x%x\n", buf, ret);
	
    return ret;
}



IOReturn 
IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refCon)
{
	io_async_ref64_t				asyncRef;
    IOReturn						ret;
    uint32_t						i, total;
    LowLatencyUserBufferInfoV3 *	dataBufferInfo;
    LowLatencyUserBufferInfoV3 *	frameListData;
	uint64_t						input[9];
	uint64_t						frameList64 = (uint64_t) frameList;

	if (!fAsyncPort)
	{
		DEBUGPRINT("IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync  NO async port\n");
        return kIOUSBNoAsyncPortErr;
	}
	
	DEBUGPRINT("IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync\n");
	
    ALLCHECKS();
	
    total = 0;
    for(i=0; i < numFrames; i++)
	{
        total += frameList[i].frReqCount;
	}
	
	input[0] = (uint64_t) pipeRef;
    input[1] = (uint64_t) total;
    input[2] = (uint64_t) frameStart;
    input[3] = (uint64_t) numFrames;
    input[4] = (uint64_t) updateFrequency;
	
    // Find the data buffer in our list of buffers
    //
    dataBufferInfo = FindBufferAddressRangeInList( buf, total);
    if ( dataBufferInfo != NULL )
    {
        input[5] = (uint64_t) dataBufferInfo->cookie;
        input[6] = (uint64_t) ((uint64_t)buf - dataBufferInfo->bufferAddress);
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync Found Data buffer: offset = %qd, cookie: %qd\n", input[5], input[6]);
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
        input[7] = (uint64_t) frameListData->cookie;
        input[8] = (uint64_t) (frameList64 - frameListData->bufferAddress);
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync Found FrameList buffer: offset = %qd, cookie = %qd\n", input[7], input[8]);
    }
    else
    {
        DEBUGPRINT("IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync Ooops, couldn't find buffer %p in our list\n",frameList);
        return kIOUSBLowLatencyFrameListNotPreviouslyAllocated;
    }
	
	DEBUGPRINT("IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync  pipe: %d, total: 0x%x, frameStart: 0x%qx, numFrames: 0x%x, updateFrequency: %d, dataCookie: %qd, dataOffset: %qd, frameListCookie: %qd, frameListOffset: %qd\n",
			   pipeRef, total, frameStart, (uint32_t)numFrames, (uint32_t)updateFrequency, input[5], input[6], input[7], input[8]);
	
#if !TARGET_OS_EMBEDDED
#endif
	
    asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t) callback;
    asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t) refCon;
	
	ret = IOConnectCallAsyncScalarMethod( fConnection, kUSBInterfaceUserClientLowLatencyWriteIsochPipe, IONotificationPortGetMachPort(fAsyncPort), asyncRef, kIOAsyncCalloutCount, 
								   input, 9, 0, 0);
    if (ret == MACH_SEND_INVALID_DEST)
    {
		fIsOpen = false;
		fInterfaceIsAttached = false;
		ret = kIOReturnNoDevice;
    }

	DEBUGPRINT("IOUSBInterfaceClass::LowLatencyWriteIsochPipeAsync  returning 0x%x\n", ret);
 
	return ret;
	
}


IOReturn
IOUSBInterfaceClass::LowLatencyCreateBuffer( void ** buffer, IOByteCount bufferSize, UInt32 bufferType )
{
    LowLatencyUserBufferInfoV3 *	bufferInfo;
    IOReturn						result = kIOReturnSuccess;
    uint64_t						output[1];						// Used for the UHCI mapped address
    uint32_t						len = 1;
    vm_address_t					data;
    kern_return_t					ret = kIOReturnSuccess;
	bool							useKernelBuffer = false;
	uint64_t						input[7];

	ALLCHECKS();

    DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer size: %d, type, %d\n", (int)bufferSize, (int)bufferType);
    
	if ( bufferSize == 0 )
	{
        DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  Requested size was 0!  Returning kIOReturnBadArgument\n",sizeof(LowLatencyUserBufferInfoV3));
        result = kIOReturnBadArgument;
        goto ErrorExit;
	}
    // Allocate our buffer Data and zero it
    //
    bufferInfo = ( LowLatencyUserBufferInfoV3 *) malloc( sizeof(LowLatencyUserBufferInfoV3) );
    if ( bufferInfo == NULL )
    {
        DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  Could not allocate a LowLatencyUserBufferInfoV3 of %ld bytes\n",sizeof(LowLatencyUserBufferInfoV3));
        *buffer = NULL;
        result = kIOReturnNoMemory;
        goto ErrorExit;
    }
    
    bzero(bufferInfo, sizeof(LowLatencyUserBufferInfoV3));
    
	// If the request if for a Read or Write buffer we will use a buffer allocated by the kernel.  Previously this was done only for UHCI controllers, but with the support 
	// of more than 2GB of memory, our DMA buffers might need to be in low address space, so we need to allocate those DMA buffers in the kernel.  Those buffers will be allocated
	// with the restrictions specified by the different controllers (using the GetLowLatencyOptionsAndPhysicalMask API).
	if ( (bufferType == kUSBLowLatencyWriteBuffer) or (bufferType == kUSBLowLatencyReadBuffer) )
	{
        DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  Using an allocation from the kernel for buffer %d\n", (uint32_t)fNextCookie);
		useKernelBuffer = true;
		*buffer = NULL;
	}
	else 
	{
		// Now, attempt to allocate the users data
		//
		ret =  vm_allocate( mach_task_self(),
							&data,
							bufferSize,
							VM_FLAGS_ANYWHERE);
		
		if ( ret != kIOReturnSuccess )
		{
			DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  Could not vm_allocate a buffer of size %d, type %d.  Result = 0x%x\n", (uint32_t)bufferSize, (uint32_t)bufferType, ret);
			result = kIOReturnNoMemory;
			goto ErrorExit;
		}
		else
		{
			DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  vm_allocate'd:   %p of size %d, type %d\n", (void *)data, (uint32_t)bufferSize, (uint32_t)bufferType);
		}
		
		*buffer = (void *) data;
		
		if ( *buffer == NULL )
		{
			DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  Could not allocate a buffer of size %d, type %d\n", (uint32_t)bufferSize, (uint32_t)bufferType);
			result = kIOReturnNoMemory;
			goto ErrorExit;
		}
    }

    // Update our buffer Data
    //
    bufferInfo->cookie	=  (uint64_t) fNextCookie++;
    bufferInfo->bufferAddress =  (uint64_t) *buffer;
    bufferInfo->bufferSize =  (uint64_t) bufferSize;
    bufferInfo->bufferType =  (uint64_t) bufferType;
    bufferInfo->isPrepared =  (uint64_t) 0;
    bufferInfo->nextBuffer =  (uint64_t) NULL;

    // Update our buffer Data
    //
    input[0] = (uint64_t) bufferInfo->cookie;
	input[1] = (uint64_t) bufferInfo->bufferAddress;
    input[2] = (uint64_t) bufferInfo->bufferSize;
	input[3] = (uint64_t) bufferInfo->bufferType;
	input[4] = (uint64_t) bufferInfo->isPrepared;
	input[5] = 0;
    input[6] = (uint64_t) bufferInfo->nextBuffer;
    
    // OK, ready to call the kernel so that it does its thing with this buffer
    //
	len = 1;
	output[0] = 0;
	
	result = IOConnectCallScalarMethod( fConnection, kUSBInterfaceUserClientLowLatencyPrepareBuffer, input, 7, output, &len);
    
    if ( result == kIOReturnSuccess )
    {
		if ( useKernelBuffer )
		{
			DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer Buffer: %p, mappedUHCIAddress now = 0x%qx\n", *buffer, output[0]);
			bufferInfo->mappedUHCIAddress = output[0];
			*buffer = (void *) bufferInfo->mappedUHCIAddress;
			bufferInfo->bufferAddress = (uint64_t) *buffer;
			// DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer Reading value of first word: 0x%lx\n", *( UInt32 *) output[0]);
		}		

		// Need to set out bufferinfo
        // Cool, we have a good buffer, add it to our list
        //
        AddDataBufferToList( bufferInfo );
    }
    else
    {
        // OK, something went wrong, so we need to release anything we allocated
        //
		if ( bufferInfo->bufferAddress )
			ret = vm_deallocate( mach_task_self(), (vm_address_t) bufferInfo->bufferAddress, bufferInfo->bufferSize );
        
		*buffer = NULL;
        free ( bufferInfo );
        
        // Fall through to return result
        //
        DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  Kernel call to kUSBInterfaceUserClientLowLatencyPrepareBuffer returned 0x%x\n", result);
    }
    
    
ErrorExit:    
		if ( result != kIOReturnSuccess )
	{
			DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  returning error 0x%x\n", result);
	}
	else
	{
		DEBUGPRINT("IOUSBLib::LowLatencyCreateBuffer:  Created buffer %p, cookie: %d\n", *buffer,(uint32_t) fNextCookie-1);
	}


    return result;
}


IOReturn
IOUSBInterfaceClass::LowLatencyDestroyBuffer( void * buffer )
{
    LowLatencyUserBufferInfoV3 *	bufferData;
    IOReturn						result = kIOReturnSuccess;
    size_t							len = 0;
    bool							found;
    kern_return_t					ret = kIOReturnSuccess;
	uint64_t						input[7];
    
    DEBUGPRINT("IOUSBLib::LowLatencyDestroyBuffer, buffer %p\n", buffer);
    
    // We need to find the LowLatencyUserBufferInfoV3 structure that contains
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
    
    input[0] = (uint64_t) bufferData->cookie;
	input[1] = (uint64_t) bufferData->bufferAddress;
    input[2] = (uint64_t) bufferData->bufferSize;
	input[3] = (uint64_t) bufferData->bufferType;
	input[4] = 0;
	input[5] = 0;
    input[6] = (uint64_t) bufferData->nextBuffer;

    if ( fConnection )
    {
        // Call into the kernel to release the kernel objects for this buffer data
        //
		result = IOConnectCallScalarMethod( fConnection, kUSBInterfaceUserClientLowLatencyReleaseBuffer, input, 7, 0, 0);
    }
    
    // If there is an error, we still need to free our data
    // Now, free the memory
    //
	if ( bufferData->mappedUHCIAddress == 0LL )
	{
		ret = vm_deallocate( mach_task_self(), (vm_address_t) bufferData->bufferAddress, bufferData->bufferSize );
		if ( ret != kIOReturnSuccess )
		{
			DEBUGPRINT("IOUSBLib::LowLatencyDestroyBuffer:  Could not vm_deallocate buffer (%p) from our list (0x%x)\n", buffer, ret);
			result = kIOReturnBadArgument;
		}
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
IOUSBInterfaceClass::AddDataBufferToList( LowLatencyUserBufferInfoV3 * insertBuffer )
{
   LowLatencyUserBufferInfoV3 *	buffer;
    
    // Traverse the list looking for last buffer and insert ours into it
    //
    if ( fUserBufferInfoListHead == NULL )
    {
        fUserBufferInfoListHead = insertBuffer;
        return;
    }
    
    buffer = fUserBufferInfoListHead;
    
    while ( buffer->nextBuffer != 0LL )
    {
        buffer = (LowLatencyUserBufferInfoV3 *) buffer->nextBuffer;
    }
    
    // When we get here, nextBuffer is pointing to NULL.  Our insert buffer
    // already has nextBuffer = NULL, so we just insert it
    //
    buffer->nextBuffer = insertBuffer;
}


LowLatencyUserBufferInfoV3 *
IOUSBInterfaceClass::FindBufferAddressInList( void *address )
{
    LowLatencyUserBufferInfoV3 *	buffer;
    bool			foundIt = true;
    
    // Traverse the list looking for this buffer
    //
    if ( fUserBufferInfoListHead == NULL )
    {
        DEBUGPRINT("IOUSBLib::FindBufferAddressInList:  fUserBufferInfoListHead is NULL!\n");
        return NULL;
    }
    
    buffer = fUserBufferInfoListHead;
    
    // Now, we need to see if our address is the same as one in the buffer list
    //
    while ( buffer->bufferAddress != (uint64_t) address )
    {
        buffer = (LowLatencyUserBufferInfoV3 *) buffer->nextBuffer;
        if ( buffer == NULL )
        {
            foundIt = false;
            break;
        }
    }
    
    if ( foundIt )
        return buffer;
    else
	{
        DEBUGPRINT("IOUSBLib::FindBufferAddressInList:  could not find buffer %p, returning NULL\n", address);
        return NULL;
	}
}

LowLatencyUserBufferInfoV3 *
IOUSBInterfaceClass::FindBufferAddressRangeInList( void * address, UInt32 size )
{
    // Need to find and see if this address range is within any of the buffers
    // in our buffer data list
    //
    LowLatencyUserBufferInfoV3 *	buffer;
    uint64_t			addressStart;
    uint64_t			addressEnd;
    uint64_t			bufferStart;
    uint64_t			bufferEnd;
    bool				foundIt = false;
    
	// DEBUGPRINT("IOUSBLib::FindBufferAddressRangeInList:  Looking for buffer: %p, size 0x%lx\n", address, size);
    // If no list, return NULL
    //
    if (fUserBufferInfoListHead == NULL)
	{
		DEBUGPRINT("IOUSBLib::FindBufferAddressRangeInList:  fUserBufferInfoListHead is NULL!\n");
        return NULL;
	}
        
    // Convert pointers to integers
    //
    addressStart = (uint64_t) address;
    addressEnd = (uint64_t) (addressStart + size);
    
    // Start at the beginning of the list
    //
    buffer = fUserBufferInfoListHead;
    
    do {
        // Calculate the bufferStart and bufferEnd for this buffer
        //
        bufferStart = (uint64_t) buffer->bufferAddress;
        bufferEnd = (uint64_t) (bufferStart + buffer->bufferSize);
        
		// DEBUGPRINT("IOUSBLib::FindBufferAddressRangeInList:  Looking at buffer: %p, size 0x%lx\n", buffer->bufferAddress, buffer->bufferSize);
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
        buffer = (LowLatencyUserBufferInfoV3 *) buffer->nextBuffer;
    
    } while ( buffer != NULL );
    
    if ( foundIt )
	{
		// DEBUGPRINT("IOUSBLib::FindBufferAddressRangeInList:  Found buffer: %p, size 0x%lx\n", buffer->bufferAddress, buffer->bufferSize);
        return buffer;
	}
    else
	{
		DEBUGPRINT("IOUSBLib::FindBufferAddressRangeInList:  Could not find address %p, size 0x%x is NULL!\n", address, (uint32_t)size);
        return NULL;
	}
}


bool
IOUSBInterfaceClass::RemoveDataBufferFromList( LowLatencyUserBufferInfoV3 * removeBuffer )
{
    LowLatencyUserBufferInfoV3 *	buffer;
    LowLatencyUserBufferInfoV3 *	previousBuffer;
    
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
        fUserBufferInfoListHead = (LowLatencyUserBufferInfoV3*)buffer->nextBuffer;
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
            buffer = (LowLatencyUserBufferInfoV3*)previousBuffer->nextBuffer;
        }
        
        // When we get here, buffer is pointing to the same buffer as removeBuffer
        // and previous buffer is pointing to the previous element in the link list,
        // so, update the link in previous to point to removeBuffer->nextBuffer;
        //
        buffer->nextBuffer = removeBuffer->nextBuffer;
    }
    
    return true;
}

#pragma mark Config Descriptor Caching

IOReturn
IOUSBInterfaceClass::CacheConfigDescriptor()
{
    int 	i;
    IOReturn	kr = kIOReturnSuccess;
    
	DEBUGPRINT("+IOUSBInterfaceClass[%p]::CacheConfigDescriptor\n", this);
	
    for (i = 0; i < fNumConfigurations; i++)
    {
        IOUSBConfigurationDescriptorPtr	configPtr = NULL;
        IOUSBConfigurationDescHeader	configHdr;
		uint64_t						in[5];
		size_t							configSize;
		
		in[0] = (uint64_t) i;
		configSize = sizeof(configHdr);
		
		DEBUGPRINT("+IOUSBInterfaceClass[%p]::CacheConfigDescriptor asking for header for config = %d\n", this, i);
		
        kr = IOConnectCallMethod(fConnection, kUSBInterfaceUserClientGetConfigDescriptor, in, 1, 0, 0, 0, 0, (void *) &configHdr, &configSize); 
		if (kr)
		{
			DEBUGPRINT("+IOUSBInterfaceClass[%p]::CacheConfigDescriptor kUSBDeviceUserClientGetConfigDescriptor asking for header of config %d returned 0x%x\n", this, i, kr);
			break;
		}
		
		// Not that we have the header, we can get the real length of the descriptor
 		in[0] = (uint64_t) i;
        configSize = USBToHostWord(configHdr.wTotalLength);
        configPtr = (IOUSBConfigurationDescriptorPtr) malloc(configSize+2);
		
		DEBUGPRINT("+IOUSBInterfaceClass[%p]::CacheConfigDescriptor asking for config %d, size:  %ld, pointer: %p\n", this, i, configSize, configPtr);
		
		kr = IOConnectCallMethod(fConnection, kUSBInterfaceUserClientGetConfigDescriptor, in, 1, 0, 0, 0, 0, configPtr, &configSize); 
		if (kr)
		{
			DEBUGPRINT("+IOUSBInterfaceClass[%p]::CacheConfigDescriptor kUSBDeviceUserClientGetConfigDescriptor asking for full config %d returned 0x%x\n", this, i, kr);
			break;
		}
        
        // Add a dummy empty descriptor on the end
        *((char*)configPtr + configSize) = 0;
        *((char*)configPtr + configSize + 1) = 0;
        fConfigurations[i] = configPtr;
    }
	
    if ( kr == kIOReturnSuccess )
	{
		fConfigDescCacheValid = TRUE;
		DEBUGPRINT("IOUSBInterfaceClass[%p]::CacheConfigDescriptor setting fConfigDescCacheValid to true\n", this);
		
		// Now, find the configuration that corresponds to the fConfigValue for this interface and set that to fCurrentConfigIndex
		for (i = 0; i < fNumConfigurations; i++)
		{
			IOUSBConfigurationDescriptorPtr	configPtr = fConfigurations[i];
			
			if ( configPtr->bConfigurationValue == fConfigValue )
			{
				fCurrentConfigIndex = i;
				break;
			}
		}
	}
	
	DEBUGPRINT("-IOUSBInterfaceClass[%p]::CacheConfigDescriptor returning 0x%x\n", this, kr);
    return kr;
}




IOUSBDescriptorHeader *
IOUSBInterfaceClass::NextDescriptor(const void *desc)
{
    const UInt8 *next = (const UInt8 *)desc;
    UInt8 length = next[0];
	
	if ( length == 0 )
	{
		return NULL;
	}
	
    next = &next[length];
	
    return((IOUSBDescriptorHeader *)next);
}


const IOUSBDescriptorHeader*
IOUSBInterfaceClass::FindNextDescriptor(const void * startDescriptor, UInt8 descType)
{
    IOUSBDescriptorHeader *			descriptorHeader;
    UInt8							configIndex;
    IOUSBConfigurationDescriptor *	curConfDesc;
    UInt16							curConfLength;
    UInt8							curConfig;

	// Get the current configuration
	curConfDesc = fConfigurations[fCurrentConfigIndex];

	// If we haven't cached the config descritpor, do so
    if (!fConfigDescCacheValid || (curConfDesc == NULL) )
	{
        IOReturn ret = CacheConfigDescriptor();
		
		if ( ret != kIOReturnSuccess )
		{
			DEBUGPRINT("+IOUSBInterfaceClass::FindNextDescriptor CacheConfigDescriptor returned 0x%x, returning NULL\n", ret);
			return NULL;
		}
			
	}
	
	// Get the current configuration
	curConfDesc = fConfigurations[fCurrentConfigIndex];
	curConfLength = USBToHostWord(curConfDesc->wTotalLength);
	
	// If our startDescriptor is NULL, then we want to search starting at the beginning of the configuration descriptor
    if ( startDescriptor == NULL )
	{
        descriptorHeader = (IOUSBDescriptorHeader*)curConfDesc;
	}
    else
    {
		// Verify that the descriptor address we are starting at is in the range of our configuration descriptor
        if ((startDescriptor < curConfDesc) || ( ((UInt64)startDescriptor - (UInt64)curConfDesc) >= curConfLength))
        {
			DEBUGPRINT("+IOUSBInterfaceClass::FindNextDescriptor Starting descriptor (%p) is out of range (%p, %d)\n", startDescriptor, curConfDesc, curConfLength);
            return NULL;
        }
		
		// OK, we have a descritpor within our configuration descriptor
        descriptorHeader = (IOUSBDescriptorHeader *)startDescriptor;
    }

	// Now, look through all the descriptors in this configuration, looking for the next one after the starting one
    do
    {
		// Save the current and look at the next
        IOUSBDescriptorHeader *	lasthdr = descriptorHeader;
        descriptorHeader = NextDescriptor(descriptorHeader);
		
        if (NULL == descriptorHeader)
        {
			// If we reach the end, then there is no "Next" descriptor
            return NULL;
        }
		
        if (lasthdr == descriptorHeader)
        {
			// If we reach the end, then there is no "Next" descriptor
            return NULL;
        }
		
        if ( ( (UInt64)descriptorHeader - (UInt64)curConfDesc) >= curConfLength)
        {
			// If the next descriptor is outside the current configuration, then there is no "Next" descriptor
            return NULL;
        }
		
        if (descType == 0)
        {
			// if the descType says 0, then it's a wild card and we just return the next descriptor
            return descriptorHeader;			
        }

        if (descriptorHeader->bDescriptorType == descType)
        {
			// We have a match!  return it
            return descriptorHeader;
        }
    } while(true);
}



IOUSBDescriptorHeader *
IOUSBInterfaceClass::FindNextAssociatedDescriptor(const void * currentDescriptor, UInt8 descriptorType)
{
    IOReturn			kr = kIOReturnSuccess;
    const IOUSBDescriptorHeader *next;
    IOUSBInterfaceDescriptor	*interfaceDesc = NULL;

    if (!fConnection)
        return NULL;

    DEBUGPRINT("+IOUSBInterfaceClass[%p]::FindNextAssociatedDescriptor %p, type: %d, fConfigDescCacheValid = %d\n", this, currentDescriptor, descriptorType, fConfigDescCacheValid);

    // Need to first get the current Config Descriptor
    //
    if ( ! fConfigDescCacheValid)
    {
        kr = CacheConfigDescriptor();
        if ( kr != kIOReturnSuccess )
            return NULL;
    }

    DEBUGPRINT("-IOUSBInterfaceClass::FindNextAssociatedDescriptor %p, type: %d\n", currentDescriptor, descriptorType);

    // Need to find the interface descriptor for THIS interface, not just the first one..
    // 
    if ( currentDescriptor == NULL )
    {
        while ( true )
        {
            currentDescriptor = FindNextDescriptor(currentDescriptor, kUSBInterfaceDesc);
            if (currentDescriptor == NULL )
                break;
            
            interfaceDesc = (IOUSBInterfaceDescriptor *) currentDescriptor;

            if ( interfaceDesc->bInterfaceNumber == fInterfaceNumber )
                break;
            
        }
    }
        

    next = ( const IOUSBDescriptorHeader *) currentDescriptor;

    while (true)
    {
        next = FindNextDescriptor(next, kUSBAnyDesc);

        // The Interface Descriptor ends when we find the end of the Config Desc, an Interface Association Descriptor
        // or an InterfaceDescriptor WITH a different interface number (this will allow us to look for the alternate settings
        // of Interface descriptors
        //
        if (!next || (next->bDescriptorType == kUSBInterfaceAssociationDesc) || ( (next->bDescriptorType == kUSBInterfaceDesc) && ( descriptorType != kUSBInterfaceDesc) ) )
        {
            DEBUGPRINT("IOUSBInterfaceClass::FindNextAssociatedDescriptor returning NULL becuase we reached the end or found an IAD\n");
            return NULL;
        }

        if ( (next->bDescriptorType == kUSBInterfaceDesc) && ( ( (IOUSBInterfaceDescriptor *)next)->bInterfaceNumber != fInterfaceNumber) )
        {
            DEBUGPRINT("IOUSBInterfaceClass::FindNextAssociatedDescriptor returning NULL cause we found a new interface descriptor with a different interface #\n");
            return NULL;
        }

        if (next->bDescriptorType == descriptorType || descriptorType == kUSBAnyDesc)
            break;
    }
    DEBUGPRINT("IOUSBInterfaceClass::FindNextAssociatedDescriptor returning %p\n", next);

    return (IOUSBDescriptorHeader *)next;
}


IOUSBDescriptorHeader *
IOUSBInterfaceClass::FindNextAltInterface(const void * currentDescriptor, IOUSBFindInterfaceRequest *request)
{
#pragma unused (currentDescriptor, request)
   return NULL;
}


#pragma mark Routing Methods

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


IOUSBInterfaceStruct500 
IOUSBInterfaceClass::sUSBInterfaceInterfaceV500 = {
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
    &IOUSBInterfaceClass::interfaceGetIOUSBLibVersion,
    // ---------- new with 2.2.0
    &IOUSBInterfaceClass::interfaceFindNextAssociatedDescriptor,
    &IOUSBInterfaceClass::interfaceFindNextAltInterface,
    // ---------- new with 3.0.0
    &IOUSBInterfaceClass::interfaceGetBusFrameNumberWithTime,
    // ---------- new with 5.0.0
    &IOUSBInterfaceClass::interfaceGetPipePropertiesV2
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
	{ return getThis(self)->stop(); }

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
//--------------- added in 2.2.0
IOUSBDescriptorHeader *
IOUSBInterfaceClass::interfaceFindNextAssociatedDescriptor( void *self, const void *currentDescriptor, UInt8 descriptorType)
{ return getThis(self)->FindNextAssociatedDescriptor(currentDescriptor, descriptorType); }

IOUSBDescriptorHeader *
IOUSBInterfaceClass::interfaceFindNextAltInterface( void *self, const void *currentDescriptor, IOUSBFindInterfaceRequest *request)
{ return getThis(self)->FindNextAltInterface(currentDescriptor, request); }

IOReturn
IOUSBInterfaceClass::interfaceGetBusFrameNumberWithTime(void *self, UInt64 *frame, AbsoluteTime *atTime)
    { return getThis(self)->GetBusFrameNumberWithTime(frame, atTime); }
//--------------- added in 5.0.0
IOReturn
IOUSBInterfaceClass::interfaceGetPipePropertiesV2(void *self, UInt8 pipeRef, UInt8 *direction, UInt8 *address, UInt8 *attributes, 
												UInt16 *maxpacketSize, UInt8 *interval, UInt8 *maxBurst, UInt8 *mult, UInt16 *bytesPerInterval)
{ return getThis(self)->GetPipePropertiesV2(pipeRef, direction, address, attributes, maxpacketSize, interval, maxBurst, mult, bytesPerInterval); }



