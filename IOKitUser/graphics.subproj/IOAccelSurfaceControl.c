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

#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOAccelSurfaceControl.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/iokitmig.h>
#include <CoreFoundation/CFNumber.h>
#include <stdlib.h>

IOReturn IOAccelFindAccelerator( io_service_t framebuffer,
                                io_service_t * pAccelerator, UInt32 * pFramebufferIndex )
{
    IOReturn      kr;
    io_service_t  accelerator = MACH_PORT_NULL;

    mach_port_t     masterPort;
    CFDictionaryRef props = 0;
    CFStringRef     cfStr;
    CFNumberRef     cfNum;
    const char *    cStr;
    char *	    buffer = NULL;

    *pAccelerator = MACH_PORT_NULL;
    *pFramebufferIndex = 0;

    do {

        IOMasterPort(MACH_PORT_NULL, &masterPort);

        kr = IORegistryEntryCreateCFProperties( framebuffer, &props,
                                                kCFAllocatorDefault, kNilOptions);
        if( kr != kIOReturnSuccess)
            continue;
        cfStr = CFDictionaryGetValue( props, CFSTR(kIOAccelTypesKey) );
        if( !cfStr)
            continue;

        cStr = CFStringGetCStringPtr( cfStr, kCFStringEncodingMacRoman);
        if( !cStr) {
            CFIndex bufferSize = CFStringGetLength(cfStr) + 1;
            buffer = malloc( bufferSize);
            if( buffer && CFStringGetCString( cfStr, buffer, bufferSize, kCFStringEncodingMacRoman))
                cStr = buffer;
        }
        if( !cStr)
            continue;

        accelerator = IORegistryEntryFromPath( masterPort, cStr );
        if( !accelerator)
            continue;
        if( !IOObjectConformsTo( accelerator, kIOAcceleratorClassName )) {
                IOObjectRelease( accelerator );
                accelerator = MACH_PORT_NULL;
                continue;
        }

        cfNum = CFDictionaryGetValue( props, CFSTR(kIOAccelIndexKey) );
        if( cfNum) 
            CFNumberGetValue( cfNum, kCFNumberSInt32Type, pFramebufferIndex );

    } while( false );

    if( buffer)
        free( buffer);
    if( props)
        CFRelease( props);

    *pAccelerator = accelerator;

    return( kr );
}

IOReturn IOAccelCreateSurface( io_service_t framebuffer, UInt32 wid, eIOAccelSurfaceModeBits modebits,
                                IOAccelConnect *connect )
{
	IOReturn      kr;
	io_connect_t  window = MACH_PORT_NULL;
	int        data[3];
	io_service_t  accelerator;
        UInt32	framebufferIndex;

	*connect = NULL;

        kr = IOAccelFindAccelerator( framebuffer, &accelerator, &framebufferIndex );
        if( kr != kIOReturnSuccess)
	{
		return kr;
	}

        /* Create a context */
        kr = IOServiceOpen( accelerator,
                    mach_task_self(),
                    kIOAccelSurfaceClientType,
                    &window );

        IOObjectRelease(accelerator);

        if( kr != kIOReturnSuccess)
        {
                window = MACH_PORT_NULL;
        }


	if( !window )
	{
		return kr;
	}

	/* Set the window id */
	data[0] = wid;
	data[1] = modebits;
	data[2] = framebufferIndex;
	kr = io_connect_method_scalarI_structureI(window, kIOAccelSurfaceSetIDMode,
		data, 3, NULL, 0);
	if(kr != kIOReturnSuccess)
	{
		IOServiceClose(window);
		return kr;
	}

	*connect = (IOAccelConnect) window;

	return kIOReturnSuccess;
}

IOReturn IOAccelDestroySurface( IOAccelConnect connect )
{
	IOReturn kr;

	if(!connect)
		return kIOReturnError;

	kr = IOServiceClose((io_connect_t) connect);

	return kr;
}

IOReturn IOAccelSetSurfaceShape( IOAccelConnect connect, IOAccelDeviceRegion *rgn, eIOAccelSurfaceShapeBits options )
{
	return io_connect_method_scalarI_structureI((io_connect_t) connect, kIOAccelSurfaceSetShape,
		(int *) &options, 1, (char *) rgn, IOACCEL_SIZEOF_DEVICE_REGION(rgn));
}

IOReturn IOAccelLockSurface( IOAccelConnect connect, IOAccelSurfaceInformation * info, UInt32 infoSize )
{
	IOReturn ret;

	ret = io_connect_method_scalarI_structureO((io_connect_t) connect, kIOAccelSurfaceLock,
		NULL, 0, (char *) info, (int *) &infoSize);

	return ret;
}

IOReturn IOAccelLockSurfaceInfo( IOAccelConnect connect, IOAccelSurfaceInformation * info, UInt32 infoSize )
{
    return( IOAccelLockSurface( connect, info, infoSize ));
}

IOReturn IOAccelUnlockSurface( IOAccelConnect connect )
{
	int countio = 0;

	return io_connect_method_scalarI_scalarO((io_connect_t) connect, kIOAccelSurfaceUnlock,
		NULL, 0, NULL, &countio);
}

IOReturn IOAccelWaitForSurface( IOAccelConnect connect )
{
	return kIOReturnSuccess;
}

/* Flush surface to visible region */
IOReturn IOAccelFlushSurface( IOAccelConnect connect, IOOptionBits options )
{
	int countio = 0;

	return io_connect_method_scalarI_scalarO((io_connect_t) connect, kIOAccelSurfaceFlush,
		(int*) &options, 1, NULL, &countio);
}

IOReturn IOAccelReadSurface( IOAccelConnect connect, IOAccelSurfaceReadData * parameters )
{
	int countio = 0;

	return io_connect_method_structureI_structureO((io_connect_t) connect, kIOAccelSurfaceRead,
		(char *) parameters, sizeof( IOAccelSurfaceReadData), NULL, &countio);

}

