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
#include "AC97Codec.h"
#include "AC97Controller.h"
#include "AC97Debug.h"

//---------------------------------------------------------------------------
// OSMetaClass stuff.

#define super IOService
OSDefineMetaClassAndStructors( AppleIntelAC97Codec, IOService )

//---------------------------------------------------------------------------

AppleIntelAC97Codec *
AppleIntelAC97Codec::codec( AppleIntelAC97Controller * controller,
                            CodecID                    codecID,
                            void *                     codecParam )
{
    AppleIntelAC97Codec * codec = new AppleIntelAC97Codec;

    if (codec && !codec->init(controller, codecID, codecParam))
    {
        codec->release();
        codec = 0;
    }

    return codec;
}

//---------------------------------------------------------------------------

bool AppleIntelAC97Codec::init( AppleIntelAC97Controller * controller,
                                CodecID                    codecID,
                                void *                     codecParam )
{
    const OSSymbol * funcSym;
    bool             ret = false;

    if ( super::init( 0 ) == false ) return false;

    _controller     = controller;
    _codecID        = codecID;
    _codecParam     = codecParam;

    funcSym = controller->getControllerFunction();

    // Add properties to aid client matching.

    setProperty( kCodecIDKey, _codecID, 32 );
    setProperty( kCodecFunctionKey, (OSObject *)funcSym );

    if ( funcSym == gAC97AudioFunction )
        ret = probeAudioCodec();
    else if ( funcSym == gAC97ModemFunction )
        ret = probeModemCodec();

    return ret;
}

//---------------------------------------------------------------------------

void AppleIntelAC97Codec::free()
{
    return super::free();
}

//---------------------------------------------------------------------------

bool AppleIntelAC97Codec::probeAudioCodec()
{
    char vendorID[8];

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, this);

    // Reset audio subsystem.

    if ( _writeRegister( kCodecAudioReset, 0 ) != kIOReturnSuccess )
    {
        DebugLog("%s: kCodecAudioReset write failed\n", getName());
        goto fail;
    }
    IOSleep(20);

    // The master volume register (0x02) is non-zero for audio codecs.

	if ( _readRegister( kCodecMasterVolume ) == 0 ) goto fail;

    // Poll powerdown register and wait for subsection ready flags.

    for ( int i = 0; i < 10; i++ )
    {
        if ( ( _readRegister( kCodecPowerdown ) &
             ( kADCReady | kDACReady | kAnalogReady | kVrefReady ) ) ==
             ( kADCReady | kDACReady | kAnalogReady | kVrefReady ) )
        {
            DebugLog("%s: audio subsection ready\n", getName());
            break;
        }
        IOSleep(10);
    }

    // Shadow all codec register to memory.

    for ( int offset = 0; offset < kCodecRegisterCount; offset++ )
    {
        _readRegister( offset );
    }

    // Figure out the usage for the second audio output pair.

    _auxOutputMode = kAuxOutModeLineOut;  // default is LINE_OUT
    if ( ( readRegister( kCodecAudioReset ) & kHeadphoneOutSupport ) &&
         ( readRegister( kCodecAuxOutVolume ) == 0x8000 ) )
    {
        DebugLog("%s: AUX_OUT is HP_OUT\n", getName());
        _auxOutputMode = kAuxOutModeHeadphoneOut;
    }

    // Check if the master volume control has 5 or 6 bits of resolution.

    _masterVolumeBitCount = 5;
    _writeRegister( kCodecMasterVolume, 0x8020 );
    if ( _readRegister( kCodecMasterVolume ) == 0x8020 )
    {
        // Has 6th bit support on master volume.
        DebugLog("%s: 6th bit master volume support\n", getName());
        _masterVolumeBitCount = 6;
    }

    // Default analog mixer configuration.

	writeRegister( kCodecPCMOutVolume, 0x0808 ); // 0 db
	writeRegister( kCodecPCBeepVolume, 0x0000 ); // 0 db
	writeRegister( kCodecCDVolume,     0x0808 ); // 0 db

    // Publish audio codec properties.

    sprintf(vendorID, "%c%c%c%x",
            (UInt8)(readRegister( kCodecVendorID1 ) >> 8),
            (UInt8) readRegister( kCodecVendorID1 ),
            (UInt8)(readRegister( kCodecVendorID2 ) >> 8),
            (UInt8) readRegister( kCodecVendorID2 ));

    setProperty(kResetRegisterKey, readRegister( kCodecAudioReset ), 16);
    setProperty(kExtAudioIDRegisterKey, readRegister( kCodecExtAudioID ), 16);
    setProperty(kPNPVendorIDKey, vendorID);

    return true;  // success

fail:
    return false; // audio probe failed
}

//---------------------------------------------------------------------------

bool AppleIntelAC97Codec::probeModemCodec()
{
    // Reset modem registers.

    if ( _writeRegister( kCodecExtModemID, 0 ) != kIOReturnSuccess )
    {
        DebugLog("%s: write to kCodecExtModemID failed\n", getName());
        goto fail;
    }
    IOSleep( 20 );

    // Verify that the Ext Modem ID register (0x3C) is non-zero.

	if ( _readRegister( kCodecExtModemID ) == 0 ) goto fail;

    // Cache all registers in the base register set.

    for ( int offset = 0; offset < kCodecRegisterCount; offset++ )
    {
        _readRegister( offset );
    }

    return true;  // success

fail:
    return false; // modem probe failed
}

//---------------------------------------------------------------------------

bool AppleIntelAC97Codec::handleOpen( IOService *  client,
                                      IOOptionBits options,
                                      void *       arg )
{
    bool success = false;

    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, client);

    if ( handleIsOpen( 0 ) == false )
    {
        if ( _controller->open( this ) )
        {
            success = super::handleOpen( client, options, arg );
            if ( success != true )
                _controller->close( this );
        }
    }
    return success;
}

//---------------------------------------------------------------------------

void AppleIntelAC97Codec::handleClose( IOService *  client,
                                       IOOptionBits options )
{
    DebugLog("%s::%s (%p)\n", getName(), __FUNCTION__, client);

    if ( handleIsOpen( client ) )
        _controller->close( this );

    super::handleClose( client, options );
}

//---------------------------------------------------------------------------

UInt16 AppleIntelAC97Codec::_readRegister( UInt8 offset )
{
    UInt16 reg = 0;

    if ( _controller->mixerRead16( _codecID, offset, &reg ) ==
        kIOReturnSuccess )
    {
        _codecRegs[ offset ] = reg;
    }

    return reg;
}

//---------------------------------------------------------------------------

IOReturn AppleIntelAC97Codec::_writeRegister( UInt8 offset, UInt16 value )
{
    IOReturn ret = kIOReturnSuccess;

    ret = _controller->mixerWrite16( _codecID, offset, value );

    if ( ret == kIOReturnSuccess )
        _codecRegs[ offset ] = value;  // Update shadow copy

    return ret;
}

//---------------------------------------------------------------------------

UInt16 AppleIntelAC97Codec::readRegister( UInt8 offset ) const
{
    return _codecRegs[ offset ];
}

//---------------------------------------------------------------------------

IOReturn AppleIntelAC97Codec::writeRegister( UInt8 offset, UInt16 value )
{
    IOReturn ret = kIOReturnSuccess;

    if ( _codecRegs[ offset ] != value )
    {
        ret = _writeRegister( offset, value );
    }

    return ret;
}

//---------------------------------------------------------------------------

void AppleIntelAC97Codec::setOutputVolumeLeft( UInt16 left )
{
    UInt16 volume = readRegister( kCodecMasterVolume );
    UInt8  mask   = (1 << _masterVolumeBitCount) - 1;

    DebugLog("%s::%s %d\n", getName(), __FUNCTION__, left);

    left = mask - ( left & mask );

    volume &= ~(mask << 8);
    volume |=  (left << 8);

    writeRegister( kCodecMasterVolume, volume );

    // For HP_OUT.

    if ( _auxOutputMode == kAuxOutModeHeadphoneOut )
    {
        volume = readRegister( kCodecAuxOutVolume );
        volume &= ~(mask << 8);
        volume |=  (left << 8);
        writeRegister( kCodecAuxOutVolume, volume );
    }
} 

//---------------------------------------------------------------------------

void AppleIntelAC97Codec::setOutputVolumeRight( UInt16 right )
{
    UInt16 volume = readRegister( kCodecMasterVolume );
    UInt8  mask   = (1 << _masterVolumeBitCount) - 1;

    DebugLog("%s::%s %d\n", getName(), __FUNCTION__, right);

    right = mask - ( right & mask );

    volume &= ~mask;
    volume |= right;

    writeRegister( kCodecMasterVolume, volume );

    // For HP_OUT.

    if ( _auxOutputMode == kAuxOutModeHeadphoneOut )
    {
        volume = readRegister( kCodecAuxOutVolume );
        volume &= ~mask;
        volume |= right;
        writeRegister( kCodecAuxOutVolume, volume );
    }
}

//---------------------------------------------------------------------------

void AppleIntelAC97Codec::setOutputVolumeMute( bool isMute )
{
    UInt16 volume = readRegister( kCodecMasterVolume );

    DebugLog("%s::%s %d\n", getName(), __FUNCTION__, isMute);

    volume &= ~kVolumeMuteBit;
    volume |= (isMute ? kVolumeMuteBit : 0);

    writeRegister( kCodecMasterVolume, volume );

    // For HP_OUT.

    if ( _auxOutputMode == kAuxOutModeHeadphoneOut )
    {
        volume = readRegister( kCodecAuxOutVolume );
        volume &= ~kVolumeMuteBit;
        volume |= (isMute ? kVolumeMuteBit : 0);
        writeRegister( kCodecAuxOutVolume, volume );
    }
}

//---------------------------------------------------------------------------

UInt32 AppleIntelAC97Codec::getOutputVolumeMin() const
{
    return 0;
}

//---------------------------------------------------------------------------

UInt32 AppleIntelAC97Codec::getOutputVolumeMax() const
{
    return ( 1 << _masterVolumeBitCount ) - 1;
}

//---------------------------------------------------------------------------

IOFixed AppleIntelAC97Codec::getOutputVolumeMinDB() const
{
    return ( _masterVolumeBitCount == 6 ) ?
           (-94 << 16) | 0x8000 :
           (-46 << 16) | 0x8000 ;
}

//---------------------------------------------------------------------------

IOFixed AppleIntelAC97Codec::getOutputVolumeMaxDB() const
{
    return 0;
}

//---------------------------------------------------------------------------

IOReturn
AppleIntelAC97Codec::setDACSampleRate( UInt32 rate, UInt32 * actualRate )
{
    IOReturn ret = kIOReturnSuccess;

    DebugLog("%s::%s %ld\n", getName(), __FUNCTION__, rate);

    if ( rate > 65535 )
    {
        ret = kIOReturnBadArgument;  // No double rate audio support.
    }
    else if ( readRegister( kCodecExtAudioID ) & kVariableRatePCMAudio )
    {
        // Enable variable rate support.

        ret = writeRegister( kCodecExtAudioStat,
                             readRegister( kCodecExtAudioStat ) |
                             kVariableRatePCMAudio );

        // Program the sample rate for all DAC's.

        writeRegister( kCodecPCMFrontDACRate,    rate );
        writeRegister( kCodecPCMSurroundDACRate, rate );
        writeRegister( kCodecPCMLFEDACRate,      rate );
    }
    else
        ret = kIOReturnUnsupported;

    if ( actualRate ) *actualRate = _readRegister( kCodecPCMFrontDACRate );

    return ret;
}

//---------------------------------------------------------------------------

UInt32 AppleIntelAC97Codec::getDACSampleRate() const
{
    return readRegister( kCodecPCMFrontDACRate );
}

//---------------------------------------------------------------------------
// Codec nub pass-through functions.

IOReturn
AppleIntelAC97Codec::setDescriptorBaseAddress( DMAChannel        channel,
                                               IOPhysicalAddress baseAddress )
{
    return _controller->setDescriptorBaseAddress( channel, baseAddress );
}

UInt8 AppleIntelAC97Codec::getCurrentIndexValue( DMAChannel channel ) const
{
    return _controller->getCurrentIndexValue( channel );
}

void AppleIntelAC97Codec::setLastValidIndex( DMAChannel channel, UInt8 index )
{
    return _controller->setLastValidIndex( channel, index );
}
                                          
UInt32 AppleIntelAC97Codec::getCurrentBufferPosition( DMAChannel channel,
                                                      UInt8 *    index ) const
{
    return _controller->getCurrentBufferPosition( channel, index );
}
                                         
IOReturn AppleIntelAC97Codec::startDMAChannel( DMAChannel channel )
{
    return _controller->startDMAChannel( channel );
}

void AppleIntelAC97Codec::stopDMAChannel( DMAChannel channel )
{
    return _controller->stopDMAChannel( channel );
}
    
bool AppleIntelAC97Codec::serviceChannelInterrupt( DMAChannel channel )
{
    return _controller->serviceChannelInterrupt( channel );
}
