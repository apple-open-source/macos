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
#include "AC97Controller.h"
#include "AC97Debug.h"

#define super IOService
OSDefineMetaClassAndAbstractStructors( AppleIntelAC97Controller, IOService )

//---------------------------------------------------------------------------

const OSSymbol * gAC97AudioFunction = 0;
const OSSymbol * gAC97ModemFunction = 0;

class AppleIntelAC97ControllerGlobals
{
public:
    AppleIntelAC97ControllerGlobals();
    ~AppleIntelAC97ControllerGlobals();
    inline bool isValid() const;
};

static AppleIntelAC97ControllerGlobals gAppleIntelAC97ControllerGlobals;

AppleIntelAC97ControllerGlobals::AppleIntelAC97ControllerGlobals()
{
    gAC97AudioFunction = OSSymbol::withCStringNoCopy(kAudioFunctionKey);
    gAC97ModemFunction = OSSymbol::withCStringNoCopy(kModemFunctionKey);
}

AppleIntelAC97ControllerGlobals::~AppleIntelAC97ControllerGlobals()
{
    RELEASE( gAC97AudioFunction );
    RELEASE( gAC97ModemFunction );
}

bool AppleIntelAC97ControllerGlobals::isValid() const
{
    return ( gAC97AudioFunction && gAC97ModemFunction );
}

//---------------------------------------------------------------------------

bool AppleIntelAC97Controller::start( IOService * provider )
{
    OSString *       funcStr;
    const OSSymbol * funcSym;
    UInt32           codecCount;

    DebugLog("%s::%s(%p)\n", getName(), __FUNCTION__, provider);

    if ( provider == 0 || super::start(provider) == false )
        goto fail;

    if ( gAppleIntelAC97ControllerGlobals.isValid() == false )
        goto fail;

    funcStr = OSDynamicCast(OSString, getProperty(kControllerFunctionKey));
    if ( funcStr == 0 )
        goto fail;

    funcSym = OSSymbol::withString(funcStr);
    if (!funcSym || !setProperty(kControllerFunctionKey, (OSObject *)funcSym))
        goto fail;

    _provider = provider;
    _provider->retain();

    // Open provider (exclusively) before using it.

    if ( _provider->open(this) == false )
    {
        goto fail;
    }

    // Let the subclass process and configure the provider.

    if ( configureProvider(_provider) == false )
    {
        _provider->close(this);
        goto fail;
    }
    
    //
    // It is now safe to access the bus master and mixer registers.
    //

    // Deassert AC_RST# line.

    bmWrite32( kGlobalControl, kGlobalColdResetDisable | k2ChannelMode );

    // Create and attach codec nubs.

    codecCount = attachCodecs();

    // Close provider. Re-open on client demand.

    _provider->close(this);

    // Publish codecs, and trigger client matching.

    publishCodecs();

    return (codecCount > 0);

fail:
    return false;
}

//---------------------------------------------------------------------------

void AppleIntelAC97Controller::free()
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    RELEASE( _provider );

    for ( int codecID = 0; codecID < kMaxCodecCount; codecID++ )
    {
        RELEASE( _codecs[codecID] );
    }

    super::free();
}

//---------------------------------------------------------------------------

UInt32 AppleIntelAC97Controller::attachCodecs()
{
    int count = 0;

    for ( int codecID = 0; codecID < kMaxCodecCount; codecID++ )
    {
        AppleIntelAC97Codec * codec = createCodec(codecID);
        if ( codec )
        {
            if ( codec->attach(this) )
            {
                _codecs[codecID] = codec;
                count++;
            }
            codec->release();
        }
    }
    
    return count;
}

//---------------------------------------------------------------------------

void AppleIntelAC97Controller::publishCodecs()
{
    for ( int codecID = 0; codecID < kMaxCodecCount; codecID++ )
        if ( _codecs[codecID] ) _codecs[codecID]->registerService();
}

//---------------------------------------------------------------------------

const OSSymbol * AppleIntelAC97Controller::getControllerFunction() const
{
    return (const OSSymbol *) getProperty( kControllerFunctionKey );
}
    
//---------------------------------------------------------------------------
// handleOpen - handle an open from a codec object.

bool AppleIntelAC97Controller::handleOpen( IOService *  client,
                                           IOOptionBits options,
                                           void *       arg )
{
    AppleIntelAC97Codec * codec = OSDynamicCast(AppleIntelAC97Codec, client);
    bool success = true;  

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, client);

    if ( codec == 0 ||
        ( _openMask == 0 && _provider->open(this) == false ) )
    {
        success = false;
    }

    if ( success )
    {
        _openMask |= (1 << codec->getCodecID());
    }

    return success;
}

//---------------------------------------------------------------------------
// handleClose - handle a close from a codec object.

void AppleIntelAC97Controller::handleClose( IOService *  client,
                                            IOOptionBits options )
{
    AppleIntelAC97Codec * codec = OSDynamicCast(AppleIntelAC97Codec, client);
    UInt32 savedOpenMask = _openMask;

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, client);

    if ( codec )
        _openMask &= ~(1 << codec->getCodecID());

    if ( savedOpenMask && _openMask == 0 )
        _provider->close(this);
}

//---------------------------------------------------------------------------
// handleIsOpen

bool AppleIntelAC97Controller::handleIsOpen( const IOService * client ) const
{
    bool isOpen;

    if ( client )
    {
        AppleIntelAC97Codec * codec
               = OSDynamicCast( AppleIntelAC97Codec, client );
        isOpen = ( codec && (_openMask & (1 << codec->getCodecID())) );
    }
    else
        isOpen = ( _openMask != 0 );

    return isOpen;
}

//---------------------------------------------------------------------------

IOReturn AppleIntelAC97Controller::reserveACLink()
{
    for ( int loops = 0; loops < 500; loops++ )
    {
        if ((bmRead8(kCodecAccessSemaphore) & kCodecAccessInProgress) == 0)
            return kIOReturnSuccess;
        IOSleep(1);
    }
    DebugLog("%s: Codec access semaphore timeout\n", getName());
    return kIOReturnTimeout;
}

//---------------------------------------------------------------------------

void AppleIntelAC97Controller::releaseACLink()
{
    bmWrite8( kCodecAccessSemaphore, 0 );
}

//---------------------------------------------------------------------------

void AppleIntelAC97Controller::coldReset()
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    // Issue a cold reset throughout the AC97 circuitry.

    bmWrite32( kGlobalControl, 0 );
    IOSleep( 20 );

    // Remove reset, and configure Output PCM for 2 channel mode.

    bmWrite32( kGlobalControl, kGlobalColdResetDisable | k2ChannelMode );
}

//---------------------------------------------------------------------------

void AppleIntelAC97Controller::warmReset()
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);
}

//---------------------------------------------------------------------------

IOReturn
AppleIntelAC97Controller::setDescriptorBaseAddress( DMAChannel        channel,
                                                    IOPhysicalAddress base )
{
    DebugLog("%s::%s (%ld, %lx)\n", getName(), __FUNCTION__, channel, base);
    bmWrite32( kBMBufferDescBaseAddress, base, channel );
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void
AppleIntelAC97Controller::setLastValidIndex( DMAChannel channel, UInt8 index )
{
    bmWrite8( kBMLastValidIndex, index, channel );
}

//---------------------------------------------------------------------------

UInt8
AppleIntelAC97Controller::getCurrentIndexValue( DMAChannel channel ) const
{
    return bmRead8( kBMCurrentIndex, channel );
}

//---------------------------------------------------------------------------

UInt32
AppleIntelAC97Controller::getCurrentBufferPosition( DMAChannel  channel,
                                                    UInt8 *     index ) const
{
    UInt16 picb; // number of samples left in current buffer
    UInt8  civ;  // current index value
    UInt8  civ_last;

    // Determine which descriptor within the list of 32 descriptors is
    // currently being processed.

    civ = bmRead8( kBMCurrentIndex, channel );

    do {
        // Get the number of samples left to be processed in the
        // current descriptor.

        civ_last = civ;
        picb = bmRead16( kBMPositionInBuffer, channel );
    }
    while ( ( civ = bmRead8( kBMCurrentIndex, channel ) ) != civ_last );

    if ( index ) *index = civ;

    return picb;
}

//---------------------------------------------------------------------------

IOReturn AppleIntelAC97Controller::startDMAChannel( DMAChannel channel )
{
    DebugLog("%s::%s (%ld) [%04x %08lx %08lx]\n", getName(), __FUNCTION__,
             channel, bmRead16(kBMStatus, channel),
             bmRead32(kGlobalStatus), bmRead32(kGlobalControl));

    bmWrite8( kBMControl,
              kInterruptOnCompletionEnable | kRunBusMaster, channel );

#if 0 // adverse effect on clock calibration with slow console
    DebugLog("%s::%s (%ld) [%04x %08lx %08lx]\n", getName(), __FUNCTION__,
             channel, bmRead16(kBMStatus, channel),
             bmRead32(kGlobalStatus), bmRead32(kGlobalControl));
#endif

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void AppleIntelAC97Controller::stopDMAChannel( DMAChannel channel )
{
    int limit;

    DebugLog("%s::%s (%ld)\n", getName(), __FUNCTION__, channel);

    // Stop the DMA engine, and wait for halt completion.

    bmWrite8(kBMControl, 0, channel);
    for ( limit = 1000; limit; limit-- )
    {
        if (bmRead16(kBMStatus, channel) & kDMAControllerHalted) break;
        IOSleep(1);
    }
    if (limit == 0) IOLog("%s: controller halt timeout\n", getName());

    // Reset the DMA engine, then wait for it to auto-clear.
    // According to Intel, setting the reset registers bit while the engine
    // is running may cause undefined consequences.

    bmWrite8(kBMControl, kResetRegisters, channel);
    for ( limit = 1000; limit; limit-- )
    {
        if ((bmRead8(kBMControl, channel) & kResetRegisters) == 0) break;
        IOSleep(1);
    }
    if (limit == 0) IOLog("%s: reset registers timeout\n", getName());
}

//---------------------------------------------------------------------------

bool AppleIntelAC97Controller::serviceChannelInterrupt( DMAChannel channel )
{
    bool interrupted = false;

    UInt16 status = bmRead16( kBMStatus, channel );

    if ( status & kBufferCompletionInterrupt )
    {
        interrupted = true;
    }

    // Acknowledge and clear all interrupt sources.

    status &= ( kBufferCompletionInterrupt
              | kLastValidBufferInterrupt
              | kFIFOError );

    bmWrite16( kBMStatus, status, channel );

    return interrupted;
}
