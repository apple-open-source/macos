/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

    mach_port_t     	   masterPort;
    CFMutableDictionaryRef props = 0;
    CFStringRef            cfStr;
    CFNumberRef            cfNum;
    const char *           cStr;
    char *	           buffer = NULL;

    *pAccelerator = MACH_PORT_NULL;
    *pFramebufferIndex = 0;

    do {

        IOMasterPort(MACH_PORT_NULL, &masterPort);

        kr = IORegistryEntryCreateCFProperties( framebuffer, &props,
                                                kCFAllocatorDefault, kNilOptions);
        if( kr != kIOReturnSuccess)
            continue;
        kr = kIOReturnError;
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

        kr = kIOReturnSuccess;

    } while( false );

    if( buffer)
        free( buffer);
    if( props)
        CFRelease( props);

    *pAccelerator = accelerator;

    return( kr );
}

IOReturn IOAccelCreateSurface( io_service_t accelerator, UInt32 wid, eIOAccelSurfaceModeBits modebits,
                                IOAccelConnect *connect )
{
	IOReturn	kr;
	io_connect_t	window = MACH_PORT_NULL;
	int		data[2];
        int		countio = 0;

	*connect = NULL;

        /* Create a context */
        kr = IOServiceOpen( accelerator,
                    mach_task_self(),
                    kIOAccelSurfaceClientType,
                    &window );

        if( kr != kIOReturnSuccess)
        {
		return kr;
        }

	/* Set the window id */
	data[0] = wid;
	data[1] = modebits;

	kr = io_connect_method_scalarI_scalarO(window, kIOAccelSurfaceSetIDMode,
		data, 2, NULL, &countio);
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

IOReturn IOAccelSetSurfaceScale( IOAccelConnect connect, IOOptionBits options,
                                    IOAccelSurfaceScaling * scaling, UInt32 scalingSize )
{
	return io_connect_method_scalarI_structureI((io_connect_t) connect,
                kIOAccelSurfaceSetScale,
                (int *) &options, 1,
		(char *) scaling, scalingSize);
}


IOReturn IOAccelSetSurfaceFramebufferShape( IOAccelConnect connect, IOAccelDeviceRegion *rgn,
                                            eIOAccelSurfaceShapeBits options, UInt32 framebufferIndex )
{
	int data[2];

	data[0] = options;
        data[1] = framebufferIndex;

	return io_connect_method_scalarI_structureI((io_connect_t) connect, kIOAccelSurfaceSetShape,
		data, 2, (char *) rgn, IOACCEL_SIZEOF_DEVICE_REGION(rgn));
}

IOReturn IOAccelSetSurfaceFramebufferShapeWithBacking( IOAccelConnect connect, IOAccelDeviceRegion *rgn,
                                            eIOAccelSurfaceShapeBits options, UInt32 framebufferIndex,
					    IOVirtualAddress backing, UInt32 rowbytes )
{
	int data[4];

	data[0] = options;
        data[1] = framebufferIndex;
        data[2] = backing;
        data[3] = rowbytes;

	return io_connect_method_scalarI_structureI((io_connect_t) connect, kIOAccelSurfaceSetShapeBacking,
		data, 4, (char *) rgn, IOACCEL_SIZEOF_DEVICE_REGION(rgn));
}

IOReturn IOAccelReadLockSurface( IOAccelConnect connect, IOAccelSurfaceInformation * info, UInt32 infoSize )
{
	IOReturn ret;

	ret = io_connect_method_scalarI_structureO((io_connect_t) connect, kIOAccelSurfaceReadLock,
		NULL, 0, (char *) info, (int *) &infoSize);

	return ret;
}

IOReturn IOAccelReadLockSurfaceWithOptions( IOAccelConnect connect, IOOptionBits options,
					    IOAccelSurfaceInformation * info, UInt32 infoSize )
{
	IOReturn ret;

	ret = io_connect_method_scalarI_structureO((io_connect_t) connect, kIOAccelSurfaceReadLockOptions,
		 (int *) &options, 1, (char *) info, (int *) &infoSize);

	return ret;
}

IOReturn IOAccelWriteLockSurface( IOAccelConnect connect, IOAccelSurfaceInformation * info, UInt32 infoSize )
{
	IOReturn ret;

	ret = io_connect_method_scalarI_structureO((io_connect_t) connect, kIOAccelSurfaceWriteLock,
		NULL, 0, (char *) info, (int *) &infoSize);

	return ret;
}

IOReturn IOAccelWriteLockSurfaceWithOptions( IOAccelConnect connect, IOOptionBits options,
					     IOAccelSurfaceInformation * info, UInt32 infoSize )
{
	IOReturn ret;

	ret = io_connect_method_scalarI_structureO((io_connect_t) connect, kIOAccelSurfaceWriteLockOptions,
		(int *) &options, 1, (char *) info, (int *) &infoSize);

	return ret;
}

IOReturn IOAccelReadUnlockSurface( IOAccelConnect connect )
{
	int countio = 0;

	return io_connect_method_scalarI_scalarO((io_connect_t) connect, kIOAccelSurfaceReadUnlock,
		NULL, 0, NULL, &countio);
}

IOReturn IOAccelReadUnlockSurfaceWithOptions( IOAccelConnect connect, IOOptionBits options )
{
	int countio = 0;

	return io_connect_method_scalarI_scalarO((io_connect_t) connect, kIOAccelSurfaceReadUnlockOptions,
		(int *) &options, 1, NULL, &countio);
}

IOReturn IOAccelWriteUnlockSurface( IOAccelConnect connect )
{
	int countio = 0;

	return io_connect_method_scalarI_scalarO((io_connect_t) connect, kIOAccelSurfaceWriteUnlock,
		NULL, 0, NULL, &countio);
}

IOReturn IOAccelWriteUnlockSurfaceWithOptions( IOAccelConnect connect, IOOptionBits options )
{
	int countio = 0;

	return io_connect_method_scalarI_scalarO((io_connect_t) connect, kIOAccelSurfaceWriteUnlockOptions,
		(int *) &options, 1, NULL, &countio);
}

IOReturn IOAccelWaitForSurface( IOAccelConnect connect )
{
	return kIOReturnSuccess;
}

IOReturn IOAccelQueryLockSurface( IOAccelConnect connect )
{
	int countio = 0;

	return io_connect_method_scalarI_scalarO((io_connect_t) connect, kIOAccelSurfaceQueryLock,
		NULL, 0, NULL, &countio);
}

/* Flush surface to visible region */
IOReturn IOAccelFlushSurfaceOnFramebuffers( IOAccelConnect connect, IOOptionBits options, UInt32 framebufferMask )
{
	int countio = 0;
	int data[2];

        data[0] = framebufferMask;
	data[1] = options;

	return io_connect_method_scalarI_scalarO((io_connect_t) connect, kIOAccelSurfaceFlush,
		data, 2, NULL, &countio);
}

IOReturn IOAccelReadSurface( IOAccelConnect connect, IOAccelSurfaceReadData * parameters )
{
	int countio = 0;

	return io_connect_method_structureI_structureO((io_connect_t) connect, kIOAccelSurfaceRead,
		(char *) parameters, sizeof( IOAccelSurfaceReadData), NULL, &countio);

}

