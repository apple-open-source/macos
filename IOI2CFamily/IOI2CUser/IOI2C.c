/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2C.c,v 1.6 2005/07/01 16:09:52 bwpang Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2C.c,v $
 *		Revision 1.6  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.5  2005/02/08 20:49:47  jlehrer
 *		Clean up compile warnings.
 *		
 *		Revision 1.4  2004/12/15 00:29:45  jlehrer
 *		Added options to extended read/write.
 *		Removed type from openI2CDevice.
 *		
 *		Revision 1.3  2004/09/17 20:29:14  jlehrer
 *		Removed ASPL headers.
 *		Added u3,mac-io,pmu-i2c search.
 *		Clean up DLOGs.
 *		
 *		Revision 1.2  2004/06/08 23:40:25  jlehrer
 *		Added getSMUI2CInterface
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#include <string.h>
#include <stdio.h>
#include "IOI2C.h"

#define DEBUG 1

#ifdef DEBUG
//#define DLOG(fmt, args...)  printf(fmt, ## args)
#define DLOG printf
#else
#define DLOG(fmt, args...)
#endif

#pragma mark ***
#pragma mark *** IOI2CFamily API
#pragma mark ***

CFArrayRef findI2CDevices(void)
{
	kern_return_t			status;
	io_iterator_t			iter;
	io_registry_entry_t		next, nub, bus;
	CFMutableArrayRef		array;
	CFMutableDictionaryRef	dict;
	char					path[8*1024];
	CFStringRef				pathKey;
	CFStringRef				compatibleKey;
	CFStringRef				cfStr;
	CFTypeRef				cfRef;

	DLOG("findI2CDevices\n");
	// Get an iterator of all IOI2CDevice matches.
	status = IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("IOI2CDevice"), &iter);
	if ((status != kIOReturnSuccess) || (iter == 0))
	{
		DLOG("findI2CDevices found none\n");
		return 0;
	}

	pathKey = CFStringCreateWithCString(NULL, "path", kCFStringEncodingMacRoman);
	compatibleKey = CFStringCreateWithCString(NULL, "compatible", kCFStringEncodingMacRoman);
	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (pathKey && compatibleKey && array)
	{
		DLOG("findI2CDevices searching...\n");
		while (next = IOIteratorNext(iter))
		{
			DLOG("findI2CDevices next\n");

			if (0 == (dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)))
			{
				DLOG("findI2CDevices CFDict, ain't!\n");
				break;
			}

			if (kIOReturnSuccess == (status = IORegistryEntryGetPath(next, kIOServicePlane, path)))
			{
				DLOG("findI2CDevices got path:\n\"%s\"\n",path);

				if (cfStr = CFStringCreateWithCString(NULL, path, kCFStringEncodingMacRoman))
				{
					DLOG("findI2CDevices adding path\n");
					CFDictionaryAddValue(dict, pathKey, cfStr), 
					CFRelease(cfStr);
				}

				DLOG("findI2CDevices adding class key\n");
				CFDictionaryAddValue(dict, CFSTR("IOClass"), CFSTR("IOI2CDevice"));

				if (kIOReturnSuccess == (status = IORegistryEntryGetParentEntry(next, kIOServicePlane, &nub)))
				{
					if (cfRef = IORegistryEntryCreateCFProperty(nub, compatibleKey, NULL, 0))
					{
						DLOG("findI2CDevices got compatible\n");
						CFDictionaryAddValue(dict, compatibleKey, cfRef);
						CFRelease(cfRef);
					}

					if (cfRef = IORegistryEntryCreateCFProperty(nub, CFSTR("reg"), NULL, 0))
					{
						DLOG("findI2CDevices got address\n");
						CFDictionaryAddValue(dict, CFSTR("address"), cfRef);
						CFRelease(cfRef);
					}

					if (kIOReturnSuccess == (status = IORegistryEntryGetParentEntry(nub, kIODeviceTreePlane, &bus)))
					{
						// The device parent can be a single-bus controller; 
						// in which case the controller may use the "reg" property for iomapping
						// in which case the bus id will be in the "AAPL,bus-id" property...
						if (cfRef = IORegistryEntryCreateCFProperty(bus, CFSTR("AAPL,i2c-bus"), NULL, 0))
						{
							DLOG("findI2CDevices got bus by \"AAPL,i2c-bus\"\n");
							CFDictionaryAddValue(dict, CFSTR("bus"), cfRef);
							CFRelease(cfRef);
						}
						else
						if (cfRef = IORegistryEntryCreateCFProperty(bus, CFSTR("reg"), NULL, 0))
						{
							DLOG("findI2CDevices got bus by \"reg\"\n");
							CFDictionaryAddValue(dict, CFSTR("bus"), cfRef);
							CFRelease(cfRef);
						}
						IOObjectRelease(bus);
					}

					IOObjectRelease(nub);
				}

				DLOG("findI2CDevices got past compatible\n");

				CFArrayAppendValue(array, dict);
				DLOG("findI2CDevices got past array dict\n");
			}
		}
		DLOG("findI2CDevices iter done\n");
	}

	DLOG("findI2CDevices release iter\n");
	IOObjectRelease(iter);
	if (pathKey)
		CFRelease(pathKey);
	if (compatibleKey)
		CFRelease(compatibleKey);
	if (array)
	{
		if (CFArrayGetCount(array) > 0)
		{
			DLOG("findI2CDevices found devices\n");
			return array;
		}

		CFRelease(array);
	}
	return 0;
}


CFArrayRef findI2CControllers(void)
{
	kern_return_t			status;
	io_iterator_t			iter;
	io_registry_entry_t		next, nub;
	CFMutableArrayRef		array;
	CFMutableDictionaryRef	dict;
	char					path[8*1024];
	CFStringRef				pathKey;
	CFStringRef				compatibleKey;
	CFStringRef				cfStr;
	CFTypeRef				cfRef;

	DLOG("findI2CControllers\n");
	// Get an iterator of all IOI2CDevice matches.
	status = IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("IOI2CController"), &iter);
	if ((status != kIOReturnSuccess) || (iter == 0))
	{
		DLOG("findI2CControllers found none\n");
		return 0;
	}

	pathKey = CFStringCreateWithCString(NULL, "path", kCFStringEncodingMacRoman);
	compatibleKey = CFStringCreateWithCString(NULL, "compatible", kCFStringEncodingMacRoman);
	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (pathKey && compatibleKey && array)
	{
		DLOG("findI2CControllers searching...\n");
		while (next = IOIteratorNext(iter))
		{
			DLOG("findI2CControllers next\n");

			if (0 == (dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)))
			{
				DLOG("findI2CControllers CFDict, ain't none!\n");
				break;
			}

			if (kIOReturnSuccess == (status = IORegistryEntryGetPath(next, kIOServicePlane, path)))
			{
				DLOG("findI2CControllers got path:\n\"%s\"\n",path);

				if (cfStr = CFStringCreateWithCString(NULL, path, kCFStringEncodingMacRoman))
				{
					DLOG("findI2CControllers adding path\n");
					CFDictionaryAddValue(dict, pathKey, cfStr), 
					CFRelease(cfStr);
				}

				DLOG("findI2CControllers adding class key\n");
				CFDictionaryAddValue(dict, CFSTR("IOClass"), CFSTR("IOI2CController"));

				if (kIOReturnSuccess == (status = IORegistryEntryGetParentEntry(next, kIOServicePlane, &nub)))
				{
					if (cfRef = IORegistryEntryCreateCFProperty(nub, compatibleKey, NULL, 0))
					{
						DLOG("findI2CControllers got compatible\n");
						CFDictionaryAddValue(dict, compatibleKey, cfRef);
						CFRelease(cfRef);
					}

					if (cfRef = IORegistryEntryCreateCFProperty(nub, CFSTR("AAPL,bus-id"), NULL, 0))
					{
						DLOG("findI2CControllers got bus\n");
						CFDictionaryAddValue(dict, CFSTR("bus"), cfRef);
						CFRelease(cfRef);
					}
					else
					if (cfRef = IORegistryEntryCreateCFProperty(nub, CFSTR("reg"), NULL, 0))
					{
						DLOG("findI2CControllers got bus\n");
						CFDictionaryAddValue(dict, CFSTR("bus"), cfRef);
						CFRelease(cfRef);
					}

					IOObjectRelease(nub);
				}

				DLOG("findI2CControllers got past compatible\n");

				CFArrayAppendValue(array, dict);
				DLOG("findI2CControllers got past array dict\n");
			}
		}
		DLOG("findI2CControllers iter done\n");
	}

	DLOG("findI2CControllers release iter\n");
	IOObjectRelease(iter);
	if (pathKey)
		CFRelease(pathKey);
	if (compatibleKey)
		CFRelease(compatibleKey);
	if (array)
	{
		if (CFArrayGetCount(array) > 0)
		{
			DLOG("findI2CControllers found devices\n");
			return array;
		}

		CFRelease(array);
	}
	return 0;
}


CFArrayRef findPPCI2CInterfaces(void)
{
	CFMutableArrayRef		array;
	CFArrayRef				pathsarray;
	CFMutableDictionaryRef	dict;

	DLOG("findPPCI2CInterfaces\n");

	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (pathsarray = findI2CInterfaces())
	{
		int i, count = CFArrayGetCount(pathsarray);
		for (i = 0; i < count; i++)
		{
			if (dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks))
			{
				CFDictionaryAddValue(dict, CFSTR("path"), CFArrayGetValueAtIndex(pathsarray, i)), 
				CFDictionaryAddValue(dict, CFSTR("IOClass"), CFSTR("PPCI2CInterface"));
				CFArrayAppendValue(array, dict);
			}
		}
	}

	if (array)
	{
		if (CFArrayGetCount(array) > 0)
		{
			DLOG("findPPCI2CInterfaces found devices\n");
			return array;
		}

		CFRelease(array);
	}
	return 0;
}




IOReturn openI2CDevice(
	I2CDeviceRef	*device,
	CFStringRef		path)
{
	kern_return_t	status;
	io_string_t		pathBuf;
	io_service_t	svc;
	io_connect_t	cnct;
	const char		*strPtr;

	if (path == NULL || device == NULL)
		return kIOReturnBadArgument;

	device->_i2c_key = kIOI2C_CLIENT_KEY_DEFAULT;

	// Put the ioreg path into an io_string_t
	strPtr = CFStringGetCStringPtr(path, kCFStringEncodingMacRoman);
	if (strPtr == NULL)
		return kIOReturnBadArgument;

	strcpy(pathBuf, strPtr);

	// get a service handle from the path
	svc = IORegistryEntryFromPath(kIOMasterPortDefault, pathBuf);

	if (svc == MACH_PORT_NULL)
	{
		DLOG("IOI2C failed to get service handle from path\n");
		return kIOReturnNotFound;
	}

	// Attempt to instantiate the user client
	status = IOServiceOpen(svc, mach_task_self(), kIOI2CUserClientType, &cnct);

	// release the service handle
	IOObjectRelease(svc);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to instantiate user client\n");
		return status;
	}

	// store the connection handle and return success
	device->_i2c_connect = cnct;
	return kIOReturnSuccess;
}

IOReturn closeI2CDevice(
	I2CDeviceRef	*device)
{
	kern_return_t status = kIOReturnSuccess;

	if (device == NULL)
		return kIOReturnBadArgument;

	if (device->_i2c_key)
	{
		unlockI2CDevice(device);
		device->_i2c_key = kIOI2C_CLIENT_KEY_DEFAULT;
	}

	// get rid of the user client
	if (device->_i2c_connect)
	{
		IOServiceClose(device->_i2c_connect);
		device->_i2c_connect = 0;
	}

	return status;
}

IOReturn lockI2CDevice(
	I2CDeviceRef	*device)
{
	if (device == NULL)
		return kIOReturnBadArgument;
	return IOConnectMethodScalarIScalarO(device->_i2c_connect, kI2CUCLock, 1, 1, 0, &device->_i2c_key);
}

IOReturn unlockI2CDevice(
	I2CDeviceRef	*device)
{
	if (device == NULL)
		return kIOReturnBadArgument;
	return IOConnectMethodScalarIScalarO(device->_i2c_connect, kI2CUCUnlock, 1, 0, device->_i2c_key);
}

IOReturn readI2CDevice(
	I2CDeviceRef	*device,
	UInt32			subAddress,
	UInt8			*readBuf,
	UInt32			count)
{
	I2CUserReadInput	inputs;
	I2CUserReadOutput	outputs;
	IOByteCount		inSize, outSize;
	kern_return_t	status;
	int				i;

	if (count > kI2CUCBufSize)
	{
		DLOG("IOI2C readI2CBus count is too large\n");
		return kIOReturnBadArgument;
	}

	inputs.count	= count;
	inputs.mode		= kI2CMode_Combined;
	inputs.subAddr	= subAddress;
	inputs.key		= device->_i2c_key;

	inSize = sizeof(I2CUserReadInput);
	outSize = sizeof(I2CUserReadOutput);

	status = IOConnectMethodStructureIStructureO(device->_i2c_connect,
			kI2CUCRead, inSize, &outSize, &inputs, &outputs);

	if (status == 0)
	{
		if (outputs.realCount != count)
			return kIOReturnError;

		for (i = 0; i < count; i++)
			readBuf[i] = outputs.buf[i];
	}

	return status;
}

IOReturn writeI2CDevice(I2CDeviceRef *device, UInt32 subAddress, UInt8 *writeBuf, UInt32 count)
{
	I2CUserWriteInput	inputs;
	I2CUserWriteOutput	outputs;
	IOByteCount		inSize, outSize;
	kern_return_t	status;
	int				i;

	if (count > kI2CUCBufSize)
	{
		DLOG("IOI2C writeI2CBus count is too large\n");
		return kIOReturnBadArgument;
	}

	inputs.count		= count;
	inputs.mode			= kI2CMode_StandardSub;
	inputs.subAddr		= subAddress;
	inputs.key			= device->_i2c_key;
	outputs.realCount	= 0;

	inSize = sizeof(I2CUserWriteInput);
	outSize = sizeof(I2CUserWriteOutput);

	for (i = 0; i < count; i++)
		inputs.buf[i] = writeBuf[i];

	status = IOConnectMethodStructureIStructureO(device->_i2c_connect,
			kI2CUCWrite, inSize, &outSize, &inputs, &outputs);

	if (status == 0)
	{
		if (outputs.realCount != count)
			return kIOReturnError;
	}

	return status;
}






IOReturn lockI2CExtended(
	I2CDeviceRef	*device,
	UInt32			bus)
{
	if (device == NULL)
		return kIOReturnBadArgument;
	return IOConnectMethodScalarIScalarO(device->_i2c_connect, kI2CUCLock, 1, 1, bus, &device->_i2c_key);
}

IOReturn readI2CExtended(
	I2CDeviceRef	*device,
	UInt32			bus,
	UInt32			address,
	UInt32			subAddress,
	UInt8			*readBuf,
	UInt32			count,
	UInt32			mode,
	UInt32			options)
{
	I2CUserReadInput	inputs;
	I2CUserReadOutput	outputs;
	IOByteCount		inSize, outSize;
	kern_return_t	status;
	int				i;

	if (count > kI2CUCBufSize)
	{
		DLOG("IOI2C readI2CExtended count is too large\n");
		return kIOReturnBadArgument;
	}

	inputs.options	= options;
	inputs.mode		= mode;
	inputs.busNo	= bus;
	inputs.addr		= address;
	inputs.subAddr	= subAddress;
	inputs.count	= count;
	inputs.key		= device->_i2c_key;

	inSize = sizeof(I2CUserReadInput);
	outSize = sizeof(I2CUserReadOutput);

	status = IOConnectMethodStructureIStructureO(device->_i2c_connect,
			kI2CUCRead, inSize, &outSize, &inputs, &outputs);

	if (status == 0)
	{
		if (outputs.realCount != count)
			return kIOReturnError;

		for (i = 0; i < count; i++)
			readBuf[i] = outputs.buf[i];
	}

	return status;
}

IOReturn writeI2CExtended(
	I2CDeviceRef	*device,
	UInt32			bus,
	UInt32			address,
	UInt32			subAddress,
	UInt8			*writeBuf,
	UInt32			count,
	UInt32			mode,
	UInt32			options)
{
	I2CUserWriteInput	inputs;
	I2CUserWriteOutput	outputs;
	IOByteCount		inSize, outSize;
	kern_return_t	status;
	int				i;

	if (count > kI2CUCBufSize)
	{
		DLOG("IOI2C writeI2CExtended count is too large\n");
		return kIOReturnBadArgument;
	}

	inputs.options		= options;
	inputs.mode			= mode;
	inputs.busNo		= bus;
	inputs.addr			= address;
	inputs.subAddr		= subAddress;
	inputs.count		= count;
	inputs.key			= device->_i2c_key;
	outputs.realCount	= 0;

	inSize = sizeof(I2CUserWriteInput);
	outSize = sizeof(I2CUserWriteOutput);

	for (i = 0; i < count; i++)
		inputs.buf[i] = writeBuf[i];

	status = IOConnectMethodStructureIStructureO(device->_i2c_connect,
			kI2CUCWrite, inSize, &outSize, &inputs, &outputs);

	if (status == 0)
	{
		if (outputs.realCount != count)
			return kIOReturnError;
	}

	return status;
}








#pragma mark ***
#pragma mark *** PPCI2CInterface API
#pragma mark ***



IOReturn readI2CInterface(
	I2CInterfaceRef iface,
	UInt32			bus,
	UInt32			address,
	UInt32			subAddress,
	UInt8			*buffer,
	UInt32			count,
	UInt32			mode)
{
	I2CReadInput	inputs;
	I2CReadOutput	outputs;
	IOByteCount		inSize, outSize;
	kern_return_t	status;
	int				i;

	if (count > kI2CUCBufSize)
	{
		DLOG("IOI2C readI2CBus count is too large\n");
		return kIOReturnBadArgument;
	}

	inputs.busNo	= bus;
	inputs.addr		= address;
	inputs.subAddr	= subAddress;
	inputs.count	= count;
	inputs.mode		= mode;

	inSize = sizeof(I2CReadInput);
	outSize = sizeof(I2CReadOutput);

	status = IOConnectMethodStructureIStructureO(iface,
			kI2CUCRead, inSize, &outSize, &inputs, &outputs);

	if (status == 0)
	{
		if (outputs.realCount != count)
			return kIOReturnError;

		for (i = 0; i < count; i++)
			buffer[i] = outputs.buf[i];
	}

	return status;
}

IOReturn writeI2CInterface(
	I2CInterfaceRef iface,
	UInt32			bus,
	UInt32			address,
	UInt32			subAddress,
	UInt8			*buffer,
	UInt32			count,
	UInt32			mode)
{
	I2CWriteInput	inputs;
	I2CWriteOutput	outputs;
	IOByteCount		inSize, outSize;
	kern_return_t	status;
	int				i;

	if (count > kI2CUCBufSize)
	{
		DLOG("IOI2C writeI2CExtended count is too large\n");
		return kIOReturnBadArgument;
	}

	inputs.mode			= mode;
	inputs.busNo		= bus;
	inputs.addr			= address;
	inputs.subAddr		= subAddress;
	inputs.count		= count;
	outputs.realCount	= 0;

	inSize = sizeof(I2CWriteInput);
	outSize = sizeof(I2CWriteOutput);

	for (i = 0; i < count; i++)
		inputs.buf[i] = buffer[i];

	status = IOConnectMethodStructureIStructureO(iface,
			kI2CUCWrite, inSize, &outSize, &inputs, &outputs);

	if (status == 0)
	{
		if (outputs.realCount != count)
			return kIOReturnError;
	}

	return status;
}



#if 1

#define	IFACE_CLASSNAME	"PPCI2CInterface"

Boolean isResouce(const char *key)
{
	Boolean				exists;
	io_registry_entry_t resources;
	CFStringRef			propKey;
	CFTypeRef			cfRef;

	propKey = CFStringCreateWithCString(NULL, key,
			kCFStringEncodingMacRoman);

	resources = IOServiceGetMatchingService(kIOMasterPortDefault,
			IOServiceNameMatching("IOResources"));

	if (resources == 0)
	{
		DLOG("IOI2C failed to get IOResources\n");
		return false;
	}

	cfRef = IORegistryEntryCreateCFProperty(resources,
			propKey, NULL, 0);

	IOObjectRelease(resources);
	CFRelease(propKey);

	if (cfRef == NULL)
	{
		exists = false;
	}
	else
	{
		exists = true;
		CFRelease(cfRef);
	}

	return exists;
}

CFArrayRef findI2CInterfaces(void)
{
	CFStringRef			anIface;
	CFMutableArrayRef	ifaces;

	ifaces = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	// mac io i2c
	if (anIface = getMacIOI2CInterface())
	{
		CFArrayAppendValue(ifaces, anIface);
		CFRelease(anIface);
	}

	// unin i2c
	if (anIface = getUniNI2CInterface())
	{
		CFArrayAppendValue(ifaces, anIface);
		CFRelease(anIface);
	}

	// pmu i2c
	if (anIface = getPMUI2CInterface())
	{
		CFArrayAppendValue(ifaces, anIface);
		CFRelease(anIface);
	}

	// smu i2c
	if (anIface = getSMUI2CInterface())
	{
		CFArrayAppendValue(ifaces, anIface);
		CFRelease(anIface);
	}

	if (CFArrayGetCount(ifaces) > 0)
	{
		return ifaces;
	}
	else
	{
		CFRelease(ifaces);
		return NULL;
	}
}

/*
 * Attempt to find the key largo I2C interface.  Look for a device alias
 * called "ki2c" or for the i2c node that's a child of mac-io.
 */
CFStringRef getMacIOI2CInterface(void)
{
	io_service_t	i2c_nub, i2c_driver;
	io_string_t		path;
	io_name_t		className;
	kern_return_t	status;
	CFStringRef		pathStr;

	// Look for PPCI2CInterface.i2c-mac-io resouces
	if (!isResouce("PPCI2CInterface.i2c-mac-io"))
	{
		DLOG("IOI2C no PPCI2CInterface bound to mac-io i2c\n");
		return NULL;
	}

	// Look up the i2c interface using the ki2c device alias
	i2c_nub = (io_service_t)IORegistryEntryFromPath(kIOMasterPortDefault,
			"IODeviceTree:ki2c");

	if (i2c_nub == MACH_PORT_NULL)
	{
		DLOG("IOI2C failed to resolve ki2c device alias\n");

		i2c_nub = (io_service_t)IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:mac-io/i2c");
	
		if (i2c_nub == MACH_PORT_NULL)
		{
			DLOG("IOI2C failed to find mac-io/i2c device nub\n");
			return NULL;
		}
	}

	if (i2c_nub == MACH_PORT_NULL)
	{
		DLOG("IOI2C failed to resolve ki2c device alias\n");
		return NULL;
	}

	// Get a reference to the i2c driver by looking up the nub's child in
	// the service plane
	status = IORegistryEntryGetChildEntry(i2c_nub, kIOServicePlane, &i2c_driver);

	// Release the reference to the nub
	IOObjectRelease(i2c_nub);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to fetch child of mac-io i2c nub\n");
		return NULL;
	}

	// Verify that i2c_driver is the right class
	status = IOObjectGetClass(i2c_driver, className);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to get class of mac-io i2c driver\n");
		return NULL;
	}

	if (strcmp(className, IFACE_CLASSNAME) != 0)
	{
		DLOG("IOI2C mac-io i2c driver is wrong class: %s\n", className);
		return NULL;
	}

	// We've found the right node, now get the path, encapsulate it and
	// return it to the caller
	status = IORegistryEntryGetPath(i2c_driver, kIOServicePlane, path);

	// Release the reference to the driver
	IOObjectRelease(i2c_driver);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to get path to mac-io i2c driver\n");
		return NULL;
	}

	pathStr = CFStringCreateWithCString(NULL, path, kCFStringEncodingMacRoman);

	if (pathStr == NULL)
	{
		DLOG("IOI2C failed to create CFString for mac-io i2c driver path\n");
		return NULL;
	}

	return pathStr;
}

CFStringRef getUniNI2CInterface(void)
{
	io_service_t	i2c_nub, i2c_driver;
	io_string_t		path;
	io_name_t		className;
	kern_return_t	status;
	CFStringRef		pathStr;

	// Look for PPCI2CInterface.i2c-uni-n resouces
	if (!isResouce("PPCI2CInterface.i2c-uni-n"))
	{
		DLOG("IOI2C no PPCI2CInterface bound to UniN i2c\n");
		return NULL;
	}

	// Look up the i2c interface using the ui2c device alias
	i2c_nub = (io_service_t)IORegistryEntryFromPath(kIOMasterPortDefault,
			"IODeviceTree:ui2c");

	if (i2c_nub == MACH_PORT_NULL)
	{
		DLOG("IOI2C failed to resolve ui2c device alias\n");

		i2c_nub = (io_service_t)IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:u3/i2c");
	
		if (i2c_nub == MACH_PORT_NULL)
		{
			DLOG("IOI2C failed to find u3/i2c device nub\n");
			return NULL;
		}
	}

	// Get a reference to the i2c driver by looking up the nub's child in
	// the service plane
	status = IORegistryEntryGetChildEntry(i2c_nub, kIOServicePlane, &i2c_driver);

	// Release the reference to the nub
	IOObjectRelease(i2c_nub);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to fetch child of unin i2c nub\n");
		return NULL;
	}

	// Verify that i2c_driver is the right class
	status = IOObjectGetClass(i2c_driver, className);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to get class of unin i2c driver\n");
		return NULL;
	}

	if (strcmp(className, IFACE_CLASSNAME) != 0)
	{
		DLOG("IOI2C unin i2c driver is wrong class: %s\n", className);
		return NULL;
	}

	// We've found the right node, now get the path, encapsulate it and
	// return it to the caller
	status = IORegistryEntryGetPath(i2c_driver, kIOServicePlane, path);

	// Release the reference to the driver
	IOObjectRelease(i2c_driver);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to get path to unin i2c driver\n");
		return NULL;
	}

	pathStr = CFStringCreateWithCString(NULL, path, kCFStringEncodingMacRoman);

	if (pathStr == NULL)
	{
		DLOG("IOI2C failed to create CFString for unin i2c driver path\n");
		return NULL;
	}

	return pathStr;
}

CFStringRef getPMUI2CInterface(void)
{
	io_service_t	i2c_nub, i2c_driver;
	io_string_t		path;
	io_name_t		className;
	kern_return_t	status;
	CFStringRef		pathStr;

	// Look for PPCI2CInterface.pmu-i2c resouces
	if (!isResouce("PPCI2CInterface.pmu-i2c"))
	{
		DLOG("IOI2C no PPCI2CInterface bound to PMU i2c\n");
		return NULL;
	}

	// Look up the i2c interface using the pi2c device alias
	i2c_nub = (io_service_t)IORegistryEntryFromPath(kIOMasterPortDefault,
			"IODeviceTree:pi2c");

	if (i2c_nub == MACH_PORT_NULL)
	{
		DLOG("IOI2C failed to resolve pi2c device alias\n");

		// Some machines (Xserve G4) don't have the pi2c alias, so look for
		// the pmu-i2c node directly
		i2c_nub = IOServiceGetMatchingService(kIOMasterPortDefault,
				IOServiceNameMatching("pmu-i2c"));
	
		if (i2c_nub == MACH_PORT_NULL)
		{
			DLOG("IOI2C failed to find pmu-i2c device nub\n");
			return NULL;
		}
	}

	// Get a reference to the i2c driver by looking up the nub's child in
	// the service plane
	status = IORegistryEntryGetChildEntry(i2c_nub, kIOServicePlane, &i2c_driver);

	// Release the reference to the nub
	IOObjectRelease(i2c_nub);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to fetch child of pmu i2c nub\n");
		return NULL;
	}

	// Verify that i2c_driver is the right class
	status = IOObjectGetClass(i2c_driver, className);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to get class of pmu i2c driver\n");
		return NULL;
	}

	if (strcmp(className, IFACE_CLASSNAME) != 0)
	{
		DLOG("IOI2C pmu i2c driver is wrong class: %s\n", className);
		return NULL;
	}

	// We've found the right node, now get the path, encapsulate it and
	// return it to the caller
	status = IORegistryEntryGetPath(i2c_driver, kIOServicePlane, path);

	// Release the reference to the driver
	IOObjectRelease(i2c_driver);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to get path to pmu i2c driver\n");
		return NULL;
	}

	pathStr = CFStringCreateWithCString(NULL, path, kCFStringEncodingMacRoman);

	if (pathStr == NULL)
	{
		DLOG("IOI2C failed to create CFString for pmu i2c driver path\n");
		return NULL;
	}

	return pathStr;
}

CFStringRef getSMUI2CInterface(void)
{
	io_service_t	i2c_nub, i2c_driver;
	io_string_t		path;
	io_name_t		className;
	kern_return_t	status;
	CFStringRef		pathStr;

	// Look for PPCI2CInterface.pmu-i2c resouces
	if (!isResouce("PPCI2CInterface.smu-i2c"))
	{
		DLOG("IOI2C no PPCI2CInterface bound to SMU i2c\n");
		return NULL;
	}

	// Look up the i2c interface using the smu device alias
	i2c_nub = (io_service_t)IORegistryEntryFromPath(kIOMasterPortDefault,
			"IODeviceTree:smu/i2c");

	if (i2c_nub == MACH_PORT_NULL)
	{
		DLOG("IOI2C failed to resolve smu/i2c device alias\n");

		i2c_nub = IOServiceGetMatchingService(kIOMasterPortDefault,
				IOServiceNameMatching("smu-i2c"));

		if (i2c_nub == MACH_PORT_NULL)
		{
			DLOG("IOI2C failed to find smu-i2c device nub\n");
			return NULL;
		}
	}

	// Get a reference to the i2c driver by looking up the nub's child in
	// the service plane
	status = IORegistryEntryGetChildEntry(i2c_nub, kIOServicePlane, &i2c_driver);

	// Release the reference to the nub
	IOObjectRelease(i2c_nub);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to fetch child of smu i2c nub\n");
		return NULL;
	}

	// Verify that i2c_driver is the right class
	status = IOObjectGetClass(i2c_driver, className);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to get class of SMU i2c driver\n");
		return NULL;
	}

	if (strcmp(className, IFACE_CLASSNAME) != 0)
	{
		DLOG("IOI2C SMU i2c driver is wrong class: %s\n", className);
		return NULL;
	}

	// We've found the right node, now get the path, encapsulate it and
	// return it to the caller
	status = IORegistryEntryGetPath(i2c_driver, kIOServicePlane, path);

	// Release the reference to the driver
	IOObjectRelease(i2c_driver);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to get path to smu i2c driver\n");
		return NULL;
	}

	pathStr = CFStringCreateWithCString(NULL, path, kCFStringEncodingMacRoman);

	if (pathStr == NULL)
	{
		DLOG("IOI2C failed to create CFString for smu i2c driver path\n");
		return NULL;
	}

	return pathStr;
}

IOReturn openI2CInterface(CFStringRef interface, I2CInterfaceRef *iface)
{
	kern_return_t	status;
	io_string_t		path;
	io_service_t	svc;
	io_connect_t	cnct;
	const char		*strPtr;

	if (interface == NULL || iface == NULL) return kIOReturnBadArgument;

	// Put the ioreg path into an io_string_t
	strPtr = CFStringGetCStringPtr(interface, kCFStringEncodingMacRoman);
	if (strPtr == NULL) return kIOReturnBadArgument;

	strcpy(path, strPtr);

	// get a service handle from the path
	svc = IORegistryEntryFromPath(kIOMasterPortDefault, path);

	if (svc == MACH_PORT_NULL)
	{
		DLOG("IOI2C failed to get service handle from path\n");
		return kIOReturnNotFound;
	}

	// Attempt to instantiate the user client
	status = IOServiceOpen(svc, mach_task_self(), 0, &cnct);

	// release the service handle
	IOObjectRelease(svc);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to instantiate user client\n");
		return status;
	}

	// now we have a connection to the user client, call the user
	// client open routine
	status = IOConnectMethodScalarIScalarO(cnct, kI2CUCLock, 0, 0);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to open the user client\n");
		IOServiceClose(cnct);
		return status;
	}

	// store the connection handle and return success
	*iface = cnct;
	return kIOReturnSuccess;
}

IOReturn closeI2CInterface(I2CInterfaceRef iface)
{
	kern_return_t status;

	if (iface == 0) return kIOReturnBadArgument;

	// call the user client's close method
	status = IOConnectMethodScalarIScalarO(iface, kI2CUCUnlock, 0, 0);

	if (status != kIOReturnSuccess)
	{
		DLOG("IOI2C failed to close the user client\n");
	}

	// get rid of the user client
	IOServiceClose(iface);

	return status;
}

IOReturn readI2CBus(I2CInterfaceRef iface, I2CReadInput *inputs,
			I2CReadOutput *outputs)
{
	IOByteCount		inSize, outSize;
	kern_return_t status;

	if (iface == 0 || inputs == NULL || outputs == NULL)
	{
		DLOG("IOI2C readI2CBus received NULL pointer arg\n");
		return kIOReturnBadArgument;
	}

	if (inputs->count > kI2CUCBufSize)
	{
		DLOG("IOI2C readI2CBus count is too large\n");
		return kIOReturnBadArgument;
	}

	inSize = sizeof(I2CReadInput);
	outSize = sizeof(I2CReadOutput);

	status = IOConnectMethodStructureIStructureO(iface,
			kI2CUCRead, inSize, &outSize, inputs, outputs);

	return status;
}

IOReturn writeI2CBus(I2CInterfaceRef iface, I2CWriteInput *inputs,
			I2CWriteOutput *outputs)
{
	IOByteCount		inSize, outSize;
	kern_return_t status;

	if (iface == 0 || inputs == NULL || outputs == NULL)
	{
		DLOG("IOI2C writeI2CBus received NULL pointer arg\n");
		return kIOReturnBadArgument;
	}

	if (inputs->count > kI2CUCBufSize)
	{
		DLOG("IOI2C writeI2CBus count is too large\n");
		return kIOReturnBadArgument;
	}

	inSize = sizeof(I2CWriteInput);
	outSize = sizeof(I2CWriteOutput);

	status = IOConnectMethodStructureIStructureO(iface,
			kI2CUCWrite, inSize, &outSize, inputs, outputs);

	return status;
}

IOReturn rmwI2CBus(I2CInterfaceRef iface, I2CRMWInput *inputs)
{
	IOByteCount		inSize, outSize;
	kern_return_t status;

	if (iface == 0 || inputs == NULL)
	{
		DLOG("IOI2C rmwI2CBus received NULL pointer arg\n");
		return kIOReturnBadArgument;
	}

	inSize = sizeof(I2CRMWInput);
	outSize = 0;

	status = IOConnectMethodStructureIStructureO(iface,
			kI2CUCRMW, inSize, &outSize, inputs, NULL);

	return status;
}

#endif
