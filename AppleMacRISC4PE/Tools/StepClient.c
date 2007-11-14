/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: StepClient.c,v 1.2 2005/07/25 21:13:24 raddog Exp $
 *
 */


#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#define kIOPluginStepperData					"stepper-data"
#define kIOPluginEnvStepperDataLoadRequest		"stepper-data-load-request"

#if 1
#define kPluginClientMagicCookie			0x50505543 		// 'PPUC' - For clients requesting privileged (root) access

//============================================================================================================
// Look through the registry and search for an object with the given
// name. If a match is found, the object is returned.
//============================================================================================================

io_object_t GetInterfaceWithName(mach_port_t masterPort, char * className)
{
    kern_return_t	kr;
    io_iterator_t	iterator;
    io_object_t		object;

	object = 0;

    kr = IORegistryCreateIterator( masterPort,
                                   kIOServicePlane,
                                   true,					/* recursive */
                                   &iterator );
    if( kr != kIOReturnSuccess )
        printf( "IORegistryCreateIterator() error %08lx\n", (unsigned long) kr );
	else {
		while( ( object = IOIteratorNext(iterator) ) )
		{
			if( IOObjectConformsTo( object, (char *) className) )
				break;
			
			IOObjectRelease( object );
			object = 0;
		}

		if( iterator ) IORegistryDisposeEnumerator( iterator );
	}

	return( object );
}

static io_object_t GetPluginServicePort( char *serviceName )
{
    static mach_port_t masterPort;
    kern_return_t kr;
    io_object_t peRef;
        
    // Gets a master port to talk with the mach_kernel:
    kr = IOMasterPort( bootstrap_port, &masterPort );
    if (kr != KERN_SUCCESS)
	{
        printf( "IOMasterPort() failed: %08lx\n", (unsigned long)kr);
        return 0;
    }

    // Gets the interface:
    peRef = GetInterfaceWithName( masterPort, serviceName );

    // and return the reference:
    return peRef;
}

//============================================================================================================
// OpenDevice
//============================================================================================================

kern_return_t OpenDevice(io_object_t obj, io_connect_t * con)
{
    kern_return_t ret = IOServiceOpen(obj, mach_task_self(), kPluginClientMagicCookie, con);

    if (ret != kIOReturnSuccess)
	{
        printf( "IOServiceOpen() error %08lx\n", (unsigned long)ret);
    }

    return ret;
}

//============================================================================================================
// CloseDevice
//============================================================================================================

kern_return_t CloseDevice(io_connect_t con)
{
    kern_return_t ret = IOServiceClose(con);

    if (ret != kIOReturnSuccess)
	{
        printf( "CloseDevice() error %08lx\n", (unsigned long)ret);
        return 0;
    }
    
    return ret;
}

kern_return_t stepclientsend (void *pmsTable, UInt32 pmsTableLength, void *pmsAuxTable, UInt32 pmsAuxTableLength)
{
	io_object_t			pluginService;
	io_connect_t		pluginConnection;
	kern_return_t		kr;

	pluginService = GetPluginServicePort ("IOPlatformPlugin");
	
	if (!pluginService)
		return kIOReturnNoDevice;

    if (kIOReturnSuccess != (kr = OpenDevice(pluginService, &pluginConnection))) {
        printf ("Open device error 0x%x\n", kr);
		return kr;
	}

	kr = IOConnectMethodScalarIScalarO( pluginConnection, 2, 4, 0, pmsTable, pmsTableLength, pmsAuxTable, pmsAuxTableLength);
	
	if (kr == kIOReturnSuccess)
		printf ("SUCCESS!!\n");
	else
		printf ("Failed 0x%x\n", kr);

	return  kr;
}

kern_return_t stepclientcontrol (UInt32 newStepLevel)
{
	io_object_t			pluginService;
	io_connect_t		pluginConnection;
	kern_return_t		kr;

	pluginService = GetPluginServicePort ("IOPlatformPlugin");
	
	if (!pluginService)
		return kIOReturnNoDevice;

    if (kIOReturnSuccess != (kr = OpenDevice(pluginService, &pluginConnection))) {
        printf ("Open device error 0x%x\n", kr);
		return kr;
	}

	kr = IOConnectMethodScalarIScalarO( pluginConnection, 3, 1, 0, newStepLevel);
	
	if (kr == kIOReturnSuccess)
		printf ("SUCCESS!!\n");
	else
		printf ("Failed 0x%x\n", kr);

	return  kr;
}

#else
kern_return_t stepclientsend (void *pmsTable, UInt32 pmsTableLength)
{
	CFMutableDictionaryRef		commandDict;
	CFDataRef					stepData;
	io_service_t				platformPlugin;
	kern_return_t				status;

	status = kIOReturnError;
	
	// get platform plugin
	platformPlugin = IOServiceGetMatchingService(kIOMasterPortDefault,
											  IOServiceMatching("IOPlatformPlugin"));

	if (platformPlugin) {
		stepData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)pmsTable, pmsTableLength);
		if (stepData) {
			// Create the dictionary
			commandDict = CFDictionaryCreateMutable(NULL, 1, &kCFTypeDictionaryKeyCallBacks,
													&kCFTypeDictionaryValueCallBacks);
			if (commandDict) {
				// Add the data
				CFDictionaryAddValue( commandDict, CFSTR(kIOPluginEnvStepperDataLoadRequest), stepData);
				
				// And send it down to the kernel
				status = IORegistryEntrySetCFProperties(platformPlugin, commandDict); //...here
				
				if (status != kIOReturnSuccess) 
					printf ("IORegistryEntrySetCFProperties returned 0x%x\n", status);
				CFRelease(commandDict);
				CFRelease(stepData);
			
			}
		}
	}
	
	return status;
}
#endif
