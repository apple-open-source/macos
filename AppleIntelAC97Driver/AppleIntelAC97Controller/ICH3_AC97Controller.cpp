/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOLib.h>
#include <pexpert/i386/protos.h>
#include "ICH3_AC97Controller.h"
#include "AC97Debug.h"

#define super AppleIntelAC97Controller
OSDefineMetaClassAndStructors( AppleIntelICH3AC97Controller,
                               AppleIntelAC97Controller )

//---------------------------------------------------------------------------

AppleIntelAC97Codec *
AppleIntelICH3AC97Controller::createCodec( CodecID codecID )
{
    AppleIntelAC97Codec * codec = 0;
    UInt32                readyMask;

    DebugLog("%s::%s(%d) Global Status = %lx\n", getName(), __FUNCTION__,
             codecID, getGlobalStatus());

    // FIXME - this needs work.

    if ( codecID == 0 )
        readyMask = kPriCodecReady;
    else if ( codecID == 1 )
        readyMask = kSecCodecReady;
    else
        return 0;

    // Wait for codec to become ready.

    for ( int timeout = 0; timeout < 600; timeout += 50 )
    {
        if ( getGlobalStatus() & readyMask )
        {
            codec = AppleIntelAC97Codec::codec( this, codecID );
            break;
        }
        IOSleep(50);
    }

    DebugLog("%s::%s(%d) codec = %p\n", getName(), __FUNCTION__,
             codecID, codec);

    return codec;
}

//---------------------------------------------------------------------------

bool AppleIntelICH3AC97Controller::configureProvider( IOService * provider )
{
    IOPCIDevice * pci;
    UInt8         irq;

    pci = OSDynamicCast( IOPCIDevice, provider );
    if ( pci == 0 )
        goto fail;
    
    // Initialize PCI config space.

    pci->setMemoryEnable( false );
    pci->setIOEnable( true );
    pci->setBusMasterEnable( true );

    // Get a mapping for the Mixer and Bus-Master I/O ranges.

    _mixerBase = pci->configRead32( kIOPCIConfigBaseAddress0 );
    _bmBase    = pci->configRead32( kIOPCIConfigBaseAddress1 );
    irq        = pci->configRead8( kIOPCIConfigInterruptLine );

    // Sanity check on I/O space indicators.

    if ( (_mixerBase & 0x01) == 0 || (_bmBase & 0x01) == 0 )
        goto fail;

    _mixerBase &= ~0x01;
    _bmBase    &= ~0x01;

    DebugLog("%s: mixerBase = %x bmBase = %x irq = %d\n", getName(),
             _mixerBase, _bmBase, irq);

    return true;

fail:
    return false;
}

//---------------------------------------------------------------------------

void AppleIntelICH3AC97Controller::free()
{
    super::free();
}

//---------------------------------------------------------------------------

#define DefineBMRegAccessors(c, p, w, v)                               \
void c::p##Write##w(UInt16 offset, UInt##w value, DMAChannel channel)  \
{                                                                      \
    out##v(_bmBase + offset + (channel * 0x10), value);                \
}                                                                      \
UInt##w c::p##Read##w(UInt16 offset, DMAChannel channel) const         \
{                                                                      \
    return in##v(_bmBase + offset + (channel * 0x10));                 \
}

DefineBMRegAccessors( AppleIntelICH3AC97Controller, bm, 32, l )
DefineBMRegAccessors( AppleIntelICH3AC97Controller, bm, 16, w )
DefineBMRegAccessors( AppleIntelICH3AC97Controller, bm,  8, b )

//---------------------------------------------------------------------------

IOReturn AppleIntelICH3AC97Controller::mixerRead16( CodecID  codecID,
                                                    UInt8    offset,
                                                    UInt16 * value )
{
    IOReturn ret;

    if ( codecID >= 4 ) return kIOReturnBadArgument;

    ret = reserveACLink();

    if ( ret == kIOReturnSuccess )
    {
        if ( value )
            *value = inw(_mixerBase + offset + (codecID * 0x80));

        releaseACLink();
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn AppleIntelICH3AC97Controller::mixerWrite16( CodecID codecID,
                                                     UInt8   offset,
                                                     UInt16  value )
{
    IOReturn ret;

    if ( codecID >= 4 ) return kIOReturnBadArgument;

    ret = reserveACLink();

    if ( ret == kIOReturnSuccess )
    {
        outw(_mixerBase + offset + (codecID * 0x80), value);

        // Hardware will clear the semaphore when the write is complete.
    }

    return kIOReturnSuccess;
}
