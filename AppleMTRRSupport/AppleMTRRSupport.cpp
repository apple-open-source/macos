/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include "AppleMTRRSupport.h"
#include <i386/mtrr.h>

#define super IOService
OSDefineMetaClassAndStructors( AppleMTRRSupport, IOService )

//-------------------------------------------------------------------------

IOService * AppleMTRRSupport::probe( IOService * provider,
                                     SInt32    * score )
{
    OSNumber * cpuNumber;

    if (!super::probe(provider, score)) return 0;

    // Only match against CPU 0.

    cpuNumber = OSDynamicCast( OSNumber,
                               provider->getProperty( "IOCPUNumber" ) );
    if ( cpuNumber && cpuNumber->unsigned32BitValue() != 0 )
    {
        return 0; // not CPU 0, fail probe
    }

    return this;
}

//-------------------------------------------------------------------------

bool AppleMTRRSupport::start( IOService * provider )
{
    PE_Video 		video;
    kern_return_t  	status = KERN_NOT_SUPPORTED;

    if ( super::start(provider) == false )
        return false;  // superclass not happy

    if ( IOService::getPlatform()->getConsoleInfo( &video ) != kIOReturnSuccess )
        return false;  // no platform console info

    if ( video.v_baseAddr > 0x100000 /* In graphics mode */ )
    {
        UInt32 realSize = video.v_rowBytes * video.v_height;

        // Round size to next power of 2 boundary.

        vramAddr = video.v_baseAddr;
        vramSize = 1 * 1024 * 1024;

        while ((vramSize < realSize) && (vramSize < 64 * 1024 * 1024))
        {
            vramSize <<= 1;
        }

        status = mtrr_range_add( vramAddr, vramSize, MTRR_TYPE_WRITECOMBINE );
        if ( status != KERN_SUCCESS )
        {
            IOLog("%s: set WC memory type error %d\n", getName(), status);
            vramSize = 0;
        }
        else
            IOLog("%s: Enabled Write-Combining for memory range %lx:%lx\n",
                  getName(), vramAddr, vramSize);
    }

    return (status == KERN_SUCCESS);
}

//-------------------------------------------------------------------------

void AppleMTRRSupport::free()
{
    if ( vramAddr && vramSize )
        mtrr_range_remove( vramAddr, vramSize, MTRR_TYPE_WRITECOMBINE );
    super::free();
}
