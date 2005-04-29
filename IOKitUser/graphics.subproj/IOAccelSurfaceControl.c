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

static io_connect_t idConnect;
enum { kAlloc, kFree };

IOReturn IOAccelCreateAccelID(IOOptionBits options, IOAccelID * identifier)
{
    IOReturn err;

    if (!idConnect)
    {
	io_service_t
	service = IORegistryEntryFromPath(kIOMasterPortDefault, 
					kIOServicePlane ":/IOResources/IODisplayWrangler");
	if (service) 
	{
	    err = IOServiceOpen(service, mach_task_self(), 0, &idConnect);
	    IOObjectRelease(service);
	}
    }

    if (!idConnect)
	return (kIOReturnNotReady);

    err = IOConnectMethodScalarIScalarO(idConnect, kAlloc, 2, 1, options, *identifier, identifier);

    return (err);
}

IOReturn IOAccelDestroyAccelID(IOOptionBits options, IOAccelID identifier)
{
    IOReturn err;

    if (!idConnect)
	return (kIOReturnNotReady);

    err = IOConnectMethodScalarIScalarO(idConnect, kFree, 2, 0, options, identifier);

    return (err);
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
        IOReturn err;

        err = IOAccelSetSurfaceFramebufferShapeWithBackingAndLength(connect, rgn, options, 
			framebufferIndex, backing, rowbytes, rgn->bounds.h * rowbytes);

	return (err);
}

IOReturn IOAccelSetSurfaceFramebufferShapeWithBackingAndLength( IOAccelConnect connect, IOAccelDeviceRegion *rgn,
                                            eIOAccelSurfaceShapeBits options, UInt32 framebufferIndex,
					    IOVirtualAddress backing, UInt32 rowbytes, UInt32 backingLength )
{
        IOReturn err;
	int data[5];

	data[0] = options;
        data[1] = framebufferIndex;
        data[2] = backing;
        data[3] = rowbytes;
        data[4] = backingLength;

	err = io_connect_method_scalarI_structureI((io_connect_t) connect, kIOAccelSurfaceSetShapeBackingAndLength,
		data, 5, (char *) rgn, IOACCEL_SIZEOF_DEVICE_REGION(rgn));

        if ((kIOReturnUnsupported == err) || (kIOReturnBadArgument == err))
	{
                err = io_connect_method_scalarI_structureI((io_connect_t) connect, kIOAccelSurfaceSetShapeBacking,
                        data, 4, (char *) rgn, IOACCEL_SIZEOF_DEVICE_REGION(rgn));
	}
	return (err);
}

IOReturn IOAccelSurfaceControl( IOAccelConnect connect,
                                    UInt32 selector, UInt32 arg, UInt32 * result) 
{
        IOReturn err;

	int countio = 1;
	int data[2];

        data[0] = selector;
	data[1] = arg;

	err = io_connect_method_scalarI_scalarO((io_connect_t) connect, kIOAccelSurfaceControl,
		data, 2, result, &countio);
    
        return( err );
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

