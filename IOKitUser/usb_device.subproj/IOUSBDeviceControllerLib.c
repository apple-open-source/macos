/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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
/*
 *  IOUSBDeviceLib.c
 *  IOUSBDeviceFamily
 *
 *  Created by Paul Chinn on 11/6/07.
 *  Copyright 2008 Apple Inc. All rights reserved.
 *
 */

#include <pthread.h>
#include <CoreFoundation/CFRuntime.h>
#include <IOKit/IOKitLib.h>
#include <sys/sysctl.h>

#include "IOUSBDeviceControllerLib.h"

#define CONFIG_FILE_PATH		CFSTR("/System/Library/AppleUSBDevice/USBDeviceConfiguration.plist")

static void __IOUSBDeviceControllerRelease( CFTypeRef object );
static IOUSBDeviceControllerRef __deviceRefFromService(CFAllocatorRef allocator, io_service_t service);

static pthread_once_t __sessionTypeInit = PTHREAD_ONCE_INIT;
static CFTypeID __kIOUSBDeviceControllerTypeID = _kCFRuntimeNotATypeID;

static CFRunLoopRef _runLoop;
static CFStringRef _runLoopMode;
static IOUSBDeviceArrivalCallback _arrivalCallback;
static void* _arrivalContext;
static IONotificationPortRef _notifyPort;
static 	io_iterator_t           _notifyIterator;

static CFTypeID __kIOUSBDeviceDescriptionTypeID = _kCFRuntimeNotATypeID;
static void __IOUSBDeviceDescriptionRelease( CFTypeRef object );
static CFStringRef __IOUSBDeviceDescriptionSerializeDebug(CFTypeRef cf);
static pthread_once_t __deviceDescriptionTypeInit = PTHREAD_ONCE_INIT;
#define DESTROY(thing) if(thing) CFRelease(thing)

static IOUSBDeviceDescriptionRef __IOUSBDeviceDescriptionCreateFromDictionary(CFAllocatorRef allocator, CFMutableDictionaryRef descDict);
static IOUSBDeviceDescriptionRef __IOUSBDeviceDescriptionCreateFromFile( CFAllocatorRef allocator, CFStringRef filePath );

typedef struct __IOUSBDeviceDescription
	{
		CFRuntimeBase					cfBase;
		CFMutableDictionaryRef			info;
		CFAllocatorRef					allocator;
	}__IOUSBDeviceDescription;

static const CFRuntimeClass __IOUSBDeviceDescriptionClass = {
0,                      // version
"IOUSBDeviceDescription",         // className
NULL,                   // init
NULL,                   // copy
__IOUSBDeviceDescriptionRelease,					// finalize
NULL,                   // equal
NULL,                   // hash
NULL,                   // copyFormattingDesc
__IOUSBDeviceDescriptionSerializeDebug,
NULL,
NULL
};

typedef struct __IOUSBDeviceController
{
    CFRuntimeBase                   cfBase;   // base CFType information
	io_service_t					deviceIOService; //io service represeniting the underlying device controller
	
} __IOUSBDeviceController, *__IOUSBDeviceControllerRef;

static const CFRuntimeClass __IOUSBDeviceControllerClass = {
    0,                      // version
    "IOUSBDeviceController",         // className
    NULL,                   // init
    NULL,                   // copy
    __IOUSBDeviceControllerRelease,					// finalize
    NULL,                   // equal
    NULL,                   // hash
    NULL,                   // copyFormattingDesc
    NULL,
    NULL,
	NULL
};

static void __controllerPublished(void *refcon __attribute__((unused)), io_iterator_t iterator)
{
	io_service_t service;
	IOUSBDeviceControllerRef device;
	
	
	while((service = IOIteratorNext(iterator)))
	{
		device = __deviceRefFromService(kCFAllocatorDefault, service);
		if(device)
		{
			_arrivalCallback(_arrivalContext, device);
			CFRelease(device);
		}
	}
}

		
		
void __IOUSBDeviceControllerRegister(void)
{
    __kIOUSBDeviceControllerTypeID = _CFRuntimeRegisterClass(&__IOUSBDeviceControllerClass);
}

//------------------------------------------------------------------------------
// IOHIDManagerGetTypeID
//------------------------------------------------------------------------------
CFTypeID IOUSBDeviceControllerGetTypeID(void)
{
    if ( _kCFRuntimeNotATypeID == __kIOUSBDeviceControllerTypeID )
        pthread_once(&__sessionTypeInit, __IOUSBDeviceControllerRegister);
	
    return __kIOUSBDeviceControllerTypeID;
}

static IOUSBDeviceControllerRef __deviceRefFromService(CFAllocatorRef allocator, io_service_t service)
{
    IOUSBDeviceControllerRef device = NULL;
	void *          offset  = NULL;
    uint32_t        size;
	size    = sizeof(__IOUSBDeviceController) - sizeof(CFRuntimeBase);
    device = (IOUSBDeviceControllerRef)_CFRuntimeCreateInstance(
															allocator, 
															IOUSBDeviceControllerGetTypeID(), 
															size, 
															NULL);
    
    if (!device)
		return NULL;
	
    offset = device;
    bzero(offset + sizeof(CFRuntimeBase), size);
    
	device->deviceIOService = service;
	IOObjectRetain(service);
    return device;
}

IOReturn IOUSBDeviceControllerCreate(     
								   CFAllocatorRef          allocator,
									IOUSBDeviceControllerRef* deviceRefPtr
								   )
{    
    CFMutableDictionaryRef 	matchingDict;
	io_service_t		deviceIOService;
	IOUSBDeviceControllerRef		deviceRef;
	matchingDict = IOServiceMatching("IOUSBDeviceController");
	if (!matchingDict)
		return kIOReturnNoMemory;
	
	deviceIOService = IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
	if(!deviceIOService)
		return kIOReturnNotFound;
	deviceRef = __deviceRefFromService(allocator, deviceIOService);
	IOObjectRelease(deviceIOService);
	if(deviceRef == NULL)
		return kIOReturnNoMemory;
	*deviceRefPtr = deviceRef;
	return kIOReturnSuccess;
}

IOReturn IOUSBDeviceControllerGoOffAndOnBus(IOUSBDeviceControllerRef device, uint32_t msecdelay)
{
	IOReturn rval;
	CFNumberRef delay = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &msecdelay);
	if(!delay)
		return kIOReturnNoMemory;
	rval = IOUSBDeviceControllerSendCommand(device, CFSTR("GoOffAndOnBus"), delay);
	CFRelease(delay);
	return rval;
}

IOReturn IOUSBDeviceControllerForceOffBus(IOUSBDeviceControllerRef device, int enable)
{
	if(enable)
		return IOUSBDeviceControllerSendCommand(device, CFSTR("ForceOffBusEnable"), NULL);
	else
		return IOUSBDeviceControllerSendCommand(device, CFSTR("ForceOffBusDisable"), NULL);
}

void IOUSBDeviceControllerRemoveArrivalCallback()
{
	if(!_notifyPort)
		return;
	IOObjectRelease(_notifyIterator);
	CFRunLoopRemoveSource(_runLoop, IONotificationPortGetRunLoopSource(_notifyPort), _runLoopMode);
	IONotificationPortDestroy(_notifyPort);
	_notifyIterator = 0; _runLoop = NULL; _runLoopMode = NULL; _notifyPort = NULL;
}

IOReturn IOUSBDeviceControllerRegisterArrivalCallback(IOUSBDeviceArrivalCallback callback, void *context,CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
	IOReturn kr;
	CFMutableDictionaryRef  matchingDict;
	
	_runLoop = runLoop;
	_runLoopMode = runLoopMode;
	
	if(_runLoop)
	{
		if(!_notifyPort)
			_notifyPort = IONotificationPortCreate(kIOMasterPortDefault);
		CFRunLoopAddSource(_runLoop, IONotificationPortGetRunLoopSource(_notifyPort), _runLoopMode);
	}
	
	if(!_notifyPort)
		return kIOReturnError;
	
	_arrivalCallback = callback;
	_arrivalContext = context;
	matchingDict = IOServiceMatching("IOUSBDeviceController");
	if((kr = IOServiceAddMatchingNotification(_notifyPort, kIOPublishNotification, matchingDict, __controllerPublished, 0, &_notifyIterator)))
		return kr;
	__controllerPublished(NULL, _notifyIterator);
	return kIOReturnSuccess;
}

static void __IOUSBDeviceControllerRelease( CFTypeRef object )
{
    IOUSBDeviceControllerRef device = (IOUSBDeviceControllerRef)object;
    
    if ( device->deviceIOService )
        IOObjectRelease(device->deviceIOService);
	device->deviceIOService = 0;
}

IOReturn IOUSBDeviceControllerSendCommand(IOUSBDeviceControllerRef device, CFStringRef command, CFTypeRef param)
{
	CFMutableDictionaryRef dict;
	IOReturn kr;
	
	dict = CFDictionaryCreateMutable(NULL, 0,
								 &kCFTypeDictionaryKeyCallBacks,
								 &kCFTypeDictionaryValueCallBacks);
	if(!dict)
		return kIOReturnNoMemory;
	
	CFDictionarySetValue(dict, CFSTR("USBDeviceCommand"), command);
	if(param)
		CFDictionarySetValue(dict, CFSTR("USBDeviceCommandParameter"), param);

	kr = IORegistryEntrySetCFProperties(device->deviceIOService, dict);
	CFRelease(dict);
	return kr;
}

IOReturn IOUSBDeviceControllerSetPreferredConfiguration(IOUSBDeviceControllerRef device, int config)
{
	IOReturn kr = kIOReturnBadArgument;
    
    CFNumberRef config_number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &config);
    
    if ( config_number )
    {
        kr = IOUSBDeviceControllerSendCommand(device, CFSTR("SetDevicePreferredConfiguration"), config_number);
        CFRelease(config_number);
    }
        
	return kr;
}


io_service_t IOUSBDeviceControllerGetService(IOUSBDeviceControllerRef controller)
{	
	return controller->deviceIOService;
}

IOReturn IOUSBDeviceControllerSetDescription(IOUSBDeviceControllerRef device, IOUSBDeviceDescriptionRef	description)
{
	return IOUSBDeviceControllerSendCommand(device, CFSTR("SetDeviceDescription"), description->info);
}

static void __IOUSBDeviceDescriptionRelease( CFTypeRef object )
{
    IOUSBDeviceDescriptionRef desc = (IOUSBDeviceDescriptionRef)object;
	DESTROY(desc->info);
}

static CFStringRef __IOUSBDeviceDescriptionSerializeDebug(CFTypeRef cf)	// str with retain
{
	IOUSBDeviceDescriptionRef desc = (IOUSBDeviceDescriptionRef)cf;
	
	return CFStringCreateWithFormat(NULL, NULL, CFSTR("IOUSBDeviceDescription: pid/vid/ver=%04x/%04x/%d class/sub/prot=%d/%d/%d Mfg:%@ Prod:%@ Ser:%@\n%@"), 
									IOUSBDeviceDescriptionGetProductID(desc), IOUSBDeviceDescriptionGetVendorID(desc), IOUSBDeviceDescriptionGetVersion(desc), 
									IOUSBDeviceDescriptionGetClass(desc), IOUSBDeviceDescriptionGetSubClass(desc), IOUSBDeviceDescriptionGetProtocol(desc), 
									IOUSBDeviceDescriptionGetManufacturerString(desc), IOUSBDeviceDescriptionGetProductString(desc), IOUSBDeviceDescriptionGetSerialString(desc),
	desc->info);
}

void __IOUSBDeviceDescriptionRegister(void)
{
    __kIOUSBDeviceDescriptionTypeID = _CFRuntimeRegisterClass(&__IOUSBDeviceDescriptionClass);
}

CFTypeID IOUSBDevicDeviceDescriptionGetTypeID(void)
{
    if ( _kCFRuntimeNotATypeID == __kIOUSBDeviceDescriptionTypeID )
        pthread_once(&__deviceDescriptionTypeInit, __IOUSBDeviceDescriptionRegister);
	
    return __kIOUSBDeviceDescriptionTypeID;
}


IOUSBDeviceDescriptionRef IOUSBDeviceDescriptionCreate(CFAllocatorRef allocator)
{
    IOUSBDeviceDescriptionRef devdesc = NULL;
	void *          offset  = NULL;
    uint32_t        size;
	size    = sizeof(__IOUSBDeviceDescription) - sizeof(CFRuntimeBase);
    devdesc = (IOUSBDeviceDescriptionRef)_CFRuntimeCreateInstance(
																		allocator, 
																		IOUSBDevicDeviceDescriptionGetTypeID(), 
																		size, 
																		NULL);
    
    if (!devdesc)
		return NULL;
	
    offset = devdesc;
    bzero(offset + sizeof(CFRuntimeBase), size);
	devdesc->info = CFDictionaryCreateMutable(allocator, 8, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	devdesc->allocator = allocator;
	
	//ensure we have a config descriptor array
	CFMutableArrayRef configs = CFArrayCreateMutable(allocator, 4, &kCFTypeArrayCallBacks);
	CFDictionarySetValue(devdesc->info, CFSTR("ConfigurationDescriptors"), configs);
	CFRelease(configs);
	return devdesc;
}	

IOUSBDeviceDescriptionRef IOUSBDeviceDescriptionCreateFromController(CFAllocatorRef allocator, IOUSBDeviceControllerRef controllerRef)
{
	CFMutableDictionaryRef descDict;
	IOUSBDeviceDescriptionRef descRef = NULL;
	
	if((descDict = (CFMutableDictionaryRef)IORegistryEntryCreateCFProperty(controllerRef->deviceIOService, CFSTR("DeviceDescription"), kCFAllocatorDefault, 0)))
	{
		descRef = __IOUSBDeviceDescriptionCreateFromDictionary(allocator, descDict);
		CFRelease(descDict);
	}
	return descRef;
}
		
static IOUSBDeviceDescriptionRef __IOUSBDeviceDescriptionCreateFromDictionary(CFAllocatorRef allocator, CFMutableDictionaryRef descDict)
{
	IOUSBDeviceDescriptionRef descRef = NULL;
	
	if((descRef = IOUSBDeviceDescriptionCreate(allocator)))
	{
		if(descDict != descRef->info)
		{
			CFRelease(descRef->info);
			descRef->info = (CFMutableDictionaryRef)CFRetain(descDict);
		}
	}	
	return descRef;
}

uint32_t __getDictNumber(IOUSBDeviceDescriptionRef ref, CFStringRef key)
{
	CFNumberRef aNumber;
	uint32_t val=0;
	if((aNumber = CFDictionaryGetValue(ref->info, key )))
		CFNumberGetValue(aNumber, kCFNumberSInt32Type, &val);
	return val;
}

uint8_t IOUSBDeviceDescriptionGetClass(IOUSBDeviceDescriptionRef ref)
{
	return __getDictNumber(ref, CFSTR("deviceClass"));	
}

void IOUSBDeviceDescriptionSetClass(IOUSBDeviceDescriptionRef devDesc, UInt8 class)
{
	CFNumberRef aNumber = CFNumberCreate(devDesc->allocator, kCFNumberCharType, &class);
	CFDictionarySetValue(devDesc->info, CFSTR("deviceClass"), aNumber);
	CFRelease(aNumber);
}

void IOUSBDeviceDescriptionSetSubClass(IOUSBDeviceDescriptionRef devDesc, UInt8 subclass)
{
	CFNumberRef aNumber = CFNumberCreate(devDesc->allocator, kCFNumberCharType, &subclass);
	CFDictionarySetValue(devDesc->info, CFSTR("deviceSubClass"), aNumber);
	CFRelease(aNumber);
}

uint8_t IOUSBDeviceDescriptionGetSubClass(IOUSBDeviceDescriptionRef ref)
{
	return __getDictNumber(ref, CFSTR("deviceSubClass"));	
}

void IOUSBDeviceDescriptionSetProtocol(IOUSBDeviceDescriptionRef devDesc, UInt8 protocol)
{
	CFNumberRef aNumber = CFNumberCreate(devDesc->allocator, kCFNumberCharType, &protocol);
	CFDictionarySetValue(devDesc->info, CFSTR("deviceProtocol"), aNumber);
	CFRelease(aNumber);
}

uint8_t IOUSBDeviceDescriptionGetProtocol(IOUSBDeviceDescriptionRef ref)
{
	return __getDictNumber(ref, CFSTR("deviceProtocol"));	
}

uint16_t IOUSBDeviceDescriptionGetVendorID(IOUSBDeviceDescriptionRef ref)
{
	return __getDictNumber(ref, CFSTR("vendorID"));	
}

void IOUSBDeviceDescriptionSetVendorID(IOUSBDeviceDescriptionRef devDesc, UInt16 vendorID)
{
	CFNumberRef aNumber = CFNumberCreate(devDesc->allocator, kCFNumberShortType, &vendorID);
	CFDictionarySetValue(devDesc->info, CFSTR("vendorID"), aNumber);
	CFRelease(aNumber);
}

uint16_t IOUSBDeviceDescriptionGetProductID(IOUSBDeviceDescriptionRef ref)
{
	return __getDictNumber(ref, CFSTR("productID"));	
}

void IOUSBDeviceDescriptionSetProductID(IOUSBDeviceDescriptionRef devDesc, UInt16 productID)
{
	CFNumberRef aNumber = CFNumberCreate(devDesc->allocator, kCFNumberShortType, &productID);
	CFDictionarySetValue(devDesc->info, CFSTR("productID"), aNumber);
	CFRelease(aNumber);
}

uint16_t IOUSBDeviceDescriptionGetVersion(IOUSBDeviceDescriptionRef ref)
{
	return __getDictNumber(ref, CFSTR("deviceID"));	
}

CFStringRef IOUSBDeviceDescriptionGetManufacturerString(IOUSBDeviceDescriptionRef ref)
{
	return CFDictionaryGetValue(ref->info, CFSTR("manufacturerString"));
}
CFStringRef IOUSBDeviceDescriptionGetProductString(IOUSBDeviceDescriptionRef ref)
{
	return CFDictionaryGetValue(ref->info, CFSTR("productString"));
}
CFStringRef IOUSBDeviceDescriptionGetSerialString(IOUSBDeviceDescriptionRef ref)
{
	return CFDictionaryGetValue(ref->info, CFSTR("serialNumber"));
}
void IOUSBDeviceDescriptionSetSerialString(IOUSBDeviceDescriptionRef ref, CFStringRef serial)
{
	CFDictionarySetValue(ref->info, CFSTR("serialNumber"), serial);
}

void IOUSBDeviceDescriptionRemoveAllConfigurations(IOUSBDeviceDescriptionRef devDesc)
{
	CFArrayRemoveAllValues((CFMutableArrayRef)CFDictionaryGetValue(devDesc->info, CFSTR("ConfigurationDescriptors")));
}

int IOUSBDeviceDescriptionGetMatchingConfiguration(IOUSBDeviceDescriptionRef devDesc, CFArrayRef interfaceNames)
{
    CFArrayRef  configDescs = (CFMutableArrayRef)CFDictionaryGetValue(devDesc->info, CFSTR("ConfigurationDescriptors"));
    CFIndex     count, index;
    Boolean     pass = FALSE;
    int         config = 0;
    int         defaultConfig = 0;
    
    if ( !configDescs ) 
        return 0;
    
    count = CFArrayGetCount(configDescs);
    if ( !count )
        return 0;
    
    for ( index=0; index<count && !pass; index++)
    {
        CFDictionaryRef configuration   = CFArrayGetValueAtIndex(configDescs, index);
        CFArrayRef      configInterfaces;
        CFIndex         configInterfaceCount;
        
        if ( !configuration )
            continue;
        
        if ( CFDictionaryGetValue(configuration, CFSTR("DefaultConfiguration")) == kCFBooleanTrue )
            defaultConfig = index + 1;
        
        configInterfaces = CFDictionaryGetValue(configuration, CFSTR("Interfaces"));
        if ( !configInterfaces )
            continue;
                
        configInterfaceCount = CFArrayGetCount(configInterfaces);
        if ( !configInterfaceCount )
            continue;
        
        if ( !interfaceNames )
            continue;
        CFIndex nameCount = CFArrayGetCount(interfaceNames);            
        CFIndex nameIndex; 
        
        if ( !nameCount )
            continue;
        
        pass = TRUE;
        for (nameIndex=0; nameIndex<nameCount; nameIndex++)
        {
            CFStringRef intefaceName = CFArrayGetValueAtIndex(interfaceNames, nameIndex);
            
            if ( kCFNotFound == CFArrayGetFirstIndexOfValue(configInterfaces, CFRangeMake(0, configInterfaceCount), intefaceName) )
            {
                pass = FALSE;
                break;
            }
        }
        if ( pass )
        {
            config = index + 1; 
            break;
        }            
    }
    
    if ( !config )
        config = defaultConfig;
            
    return config;
}


int IOUSBDeviceDescriptionAppendConfiguration(IOUSBDeviceDescriptionRef devDesc, CFStringRef textDescription, UInt8 attributes, UInt8 maxPower)
{
	CFMutableArrayRef configs = (CFMutableArrayRef)CFDictionaryGetValue(devDesc->info, CFSTR("ConfigurationDescriptors"));
	CFMutableDictionaryRef theConfig;
	CFNumberRef aNumber;
	CFMutableArrayRef interfaces;
	
	//hack to allow manually created descriptions to be sent into the kernel even when its already got one. See the comment
	//in the family's createUSBDevice() function.
	CFDictionarySetValue(devDesc->info, CFSTR("AllowMultipleCreates"), kCFBooleanTrue);
	
	theConfig = CFDictionaryCreateMutable(devDesc->allocator, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	interfaces = CFArrayCreateMutable(devDesc->allocator, 4, &kCFTypeArrayCallBacks);
	CFDictionaryAddValue(theConfig, CFSTR("Interfaces"), interfaces);
	CFRelease(interfaces);

	if(textDescription)
		CFDictionaryAddValue(theConfig, CFSTR("Description"), textDescription);
						   
	aNumber = CFNumberCreate(devDesc->allocator, kCFNumberCharType, &attributes);
	CFDictionaryAddValue(theConfig, CFSTR("Attributes"), aNumber);
	CFRelease(aNumber);
	aNumber = CFNumberCreate(devDesc->allocator, kCFNumberCharType, &maxPower);
	CFDictionaryAddValue(theConfig, CFSTR("MaxPower"), aNumber);
	CFRelease(aNumber);
	CFArrayAppendValue(configs, theConfig);
	CFRelease(theConfig);
	return CFArrayGetCount(configs) - 1;
}

int IOUSBDeviceDescriptionAppendInterfaceToConfiguration(IOUSBDeviceDescriptionRef devDesc, int config, CFStringRef name)
{
	CFMutableDictionaryRef theConfig;
	CFMutableArrayRef configs = (CFMutableArrayRef)CFDictionaryGetValue(devDesc->info, CFSTR("ConfigurationDescriptors"));
	CFMutableArrayRef interfaces;
	
	theConfig = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(configs, config);
	if(NULL == theConfig)
		return -1;
	interfaces = (CFMutableArrayRef)CFDictionaryGetValue(theConfig, CFSTR("Interfaces"));
	CFArrayAppendValue(interfaces, name);
	return CFArrayGetCount(interfaces) - 1;
}

CFArrayRef IOUSBDeviceDescriptionCopyInterfaces(IOUSBDeviceDescriptionRef devDesc)
{
	CFArrayRef configs = CFDictionaryGetValue(devDesc->info, CFSTR("ConfigurationDescriptors"));
	CFIndex numConfigs = CFArrayGetCount(configs);

	CFMutableArrayRef allInterfaces = CFArrayCreateMutable(devDesc->allocator, numConfigs, &kCFTypeArrayCallBacks);
	if (allInterfaces != NULL) {
		
		for (CFIndex i = 0; i < numConfigs; i++) {
			CFDictionaryRef config = CFArrayGetValueAtIndex(configs, i);
			CFArrayRef interfaces = CFDictionaryGetValue(config, CFSTR("Interfaces"));

			/* 
			 * Create a copy of the interfaces to return.  If this fails, we're
			 * in trouble, but try to clean up and get out
			 */
			CFArrayRef interfacesCopy = CFArrayCreateCopy(devDesc->allocator, interfaces);
			if (interfacesCopy == NULL) {
				CFRelease(allInterfaces);
				allInterfaces = NULL;
				break;
			}
			
			CFArrayAppendValue(allInterfaces, interfacesCopy);
			CFRelease(interfacesCopy);
		}
	}

	return allInterfaces;
}

static IOUSBDeviceDescriptionRef __IOUSBDeviceDescriptionCreateFromFile( CFAllocatorRef allocator, CFStringRef filePath )
{
	IOUSBDeviceDescriptionRef devDesc = NULL;

	CFURLRef fileURL = CFURLCreateWithFileSystemPath( allocator, filePath, kCFURLPOSIXPathStyle, false );
	if(fileURL)
	{
		CFDataRef resourceData = NULL;
		SInt32 errorCode;
		
		CFURLCreateDataAndPropertiesFromResource(allocator, fileURL, &resourceData, NULL, NULL, &errorCode);
		if(resourceData)
		{
			CFDictionaryRef allDescriptions;
			allDescriptions = CFPropertyListCreateFromXMLData( allocator,resourceData,kCFPropertyListMutableContainersAndLeaves, NULL);
			if(allDescriptions)
			{
				int rval;
				char machineName[64];
				size_t maxSize=sizeof(machineName);
				CFMutableDictionaryRef thisDeviceDescription = NULL;
				CFStringRef machineNameKey;
				CFDictionaryRef devices;
				
				//find out the name of this machine
				rval = sysctlbyname("hw.machine", machineName, &maxSize, NULL, 0);
				if(rval != 0)
					strncpy(machineName, "sysctl hw.machine failed", sizeof(machineName));

				machineNameKey = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)machineName, kCFStringEncodingASCII);
				devices = CFDictionaryGetValue(allDescriptions, CFSTR("devices"));
				if(!devices) //must be the old-school way...
					thisDeviceDescription = (CFMutableDictionaryRef)CFDictionaryGetValue(allDescriptions, machineNameKey);
				else
				{
					thisDeviceDescription = (CFMutableDictionaryRef)CFDictionaryGetValue(devices, machineNameKey);
					if(!thisDeviceDescription) //can't find a config keyed to this machine...try to load the default
						thisDeviceDescription = (CFMutableDictionaryRef)CFDictionaryGetValue(devices, CFSTR("unknownHardware"));
					if(thisDeviceDescription)
					{
						//check if the configuration descriptor is actually a key into the list of stock configs
						CFStringRef configName = CFDictionaryGetValue(thisDeviceDescription, CFSTR("ConfigurationDescriptors"));
						if(CFGetTypeID(configName) == CFStringGetTypeID()) //yes, it's a string that is a key into the config dict
						{
							CFDictionaryRef configs = CFDictionaryGetValue(allDescriptions, CFSTR("configurations"));
							CFDictionaryRef thisConfig = CFDictionaryGetValue(configs, configName);
							CFDictionarySetValue(thisDeviceDescription, CFSTR("ConfigurationDescriptors"), thisConfig);
						}
					}
				}
				if(thisDeviceDescription)
					devDesc = __IOUSBDeviceDescriptionCreateFromDictionary(NULL, thisDeviceDescription);	
				CFRelease(machineNameKey);
				CFRelease(allDescriptions);
			}
			CFRelease(resourceData);
		}
		CFRelease(fileURL);
	}
	return devDesc;
}


IOUSBDeviceDescriptionRef IOUSBDeviceDescriptionCreateFromDefaults(CFAllocatorRef allocator) 
{
	return __IOUSBDeviceDescriptionCreateFromFile(allocator, CONFIG_FILE_PATH);
}