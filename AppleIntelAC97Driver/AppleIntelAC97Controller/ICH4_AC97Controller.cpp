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
#include <libkern/OSByteOrder.h>
#include "ICH4_AC97Controller.h"
#include "AC97Debug.h"

#define super AppleIntelAC97Controller
OSDefineMetaClassAndStructors( AppleIntelICH4AC97Controller,
                               AppleIntelAC97Controller )

//---------------------------------------------------------------------------

AppleIntelAC97Codec *
AppleIntelICH4AC97Controller::createCodec( CodecID codecID )
{
    AppleIntelAC97Codec * codec = 0;
    UInt32                readyMask;

    DebugLog("%s::%s(%d) Global Status = %lx\n", getName(), __FUNCTION__,
             codecID, getGlobalStatus());

    // FIXME - this needs work. Add secondary audio codec support.

    if ( codecID > 0 ) return 0;

    // On ICH4, a codec can be connected to any of the three AC_SDINx
    // input lines. Wait for any line to assert ready state.

    readyMask = kPriCodecReady | kSecCodecReady | k3rdCodecReady;

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

bool AppleIntelICH4AC97Controller::configureProvider( IOService * provider )
{
    IOPCIDevice * pci;
    UInt8         irq;

    pci = OSDynamicCast( IOPCIDevice, provider );
    if ( pci == 0 )
        goto fail;
    
    // Initialize PCI config space.

    pci->setMemoryEnable( true );
    pci->setIOEnable( false );
    pci->setBusMasterEnable( true );
    
    // Fetch the Mixer and Bus-Master base address registers.

    _mixerMap = pci->mapDeviceMemoryWithRegister( kIOPCIConfigBaseAddress2 );
    _bmMap    = pci->mapDeviceMemoryWithRegister( kIOPCIConfigBaseAddress3 );

    if ( !_mixerMap || !_bmMap ) goto fail;
    
    _mixerBase = (void *) _mixerMap->getVirtualAddress();
    _bmBase    = (void *) _bmMap->getVirtualAddress();
    irq        = pci->configRead8( kIOPCIConfigInterruptLine );

    DebugLog("%s mixerBase = %p bmBase = %p irq = %d\n", getName(),
             _mixerBase, _bmBase, irq);

    return true;

fail:
    return false;
}

//---------------------------------------------------------------------------

void AppleIntelICH4AC97Controller::free()
{
    if ( _mixerMap )
    {
        _mixerMap->release();
        _mixerMap = 0;
    }
    
    if ( _bmMap )
    {
        _bmMap->release();
        _bmMap = 0;
    }
    
    super::free();
}

//---------------------------------------------------------------------------

#define DefineBMRegAccessors(c, p, w, b)                               \
void c::p##Write##w(UInt16 offset, UInt##w value, DMAChannel channel)  \
{                                                                      \
    OSWriteLittleInt##w(b, offset + (channel * 0x10), value);          \
}                                                                      \
UInt##w c::p##Read##w(UInt16 offset, DMAChannel channel) const         \
{                                                                      \
    return OSReadLittleInt##w(b, offset + (channel * 0x10));           \
}

DefineBMRegAccessors( AppleIntelICH4AC97Controller, bm, 32, _bmBase )
DefineBMRegAccessors( AppleIntelICH4AC97Controller, bm, 16, _bmBase )

void AppleIntelICH4AC97Controller::bmWrite8(  UInt16     offset,
                                              UInt8      value,
                                              DMAChannel channel )
{
    *((UInt8 *)_bmBase + offset + (channel * 0x10)) = value;
}

UInt8 AppleIntelICH4AC97Controller::bmRead8(  UInt16      offset,
                                              DMAChannel  channel ) const
{
    return *((UInt8 *)_bmBase + offset + (channel * 0x10));
}

//---------------------------------------------------------------------------

IOReturn AppleIntelICH4AC97Controller::mixerRead16( CodecID  codecID,
                                                    UInt8    offset,
                                                    UInt16 * value )
{
    IOReturn ret;

    if ( codecID >= 4 ) return kIOReturnBadArgument;

    ret = reserveACLink();

    if ( ret == kIOReturnSuccess )
    {
        if ( value )
            *value = OSReadLittleInt16(_mixerBase, offset + (codecID * 0x80));

        releaseACLink();
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn AppleIntelICH4AC97Controller::mixerWrite16( CodecID codecID,
                                                     UInt8   offset,
                                                     UInt16  value )
{
    IOReturn ret;

    if ( codecID >= 4 ) return kIOReturnBadArgument;

    ret = reserveACLink();

    if ( ret == kIOReturnSuccess )
    {
        OSWriteLittleInt16(_mixerBase, offset + (codecID * 0x80), value);

        // Hardware will clear the semaphore when the write is complete.
    }

    return kIOReturnSuccess;
}
