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
#include <IOKit/IOMessage.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/audio/IOAudioPort.h>

#include "AC97AudioDriver.h"
#include "AC97AudioEngine.h"
#include "AC97Debug.h"

#define super IOAudioDevice
OSDefineMetaClassAndStructors( AppleIntelAC97AudioDriver, IOAudioDevice )

//---------------------------------------------------------------------------
// initHardware (inherited from IOAudioDevice)

bool AppleIntelAC97AudioDriver::initHardware( IOService * provider )
{
    DebugLog("%s::%s(%p)\n", getName(), __FUNCTION__, provider);

    if ( super::initHardware(provider) == false )
    {
        IOLog("super::initHardware\n");
        goto fail;
    }

    setManufacturerName( "Intel" );
    setDeviceName( "AC97 Audio Controller" );

    // Keep a reference to our provider. Must be an audio codec.

    _audioCodec = OSDynamicCast( AppleIntelAC97Codec, provider );
    if ( _audioCodec == 0 )
    {
        goto fail;
    }

    // Open codec for exclusive access.

    if ( _audioCodec->open( this ) == false )
    {
        DebugLog("%s: codec open error\n", getName());
        goto fail;
    }

    // Create audio engine.

    if ( createAudioEngine() == false )
    {
        DebugLog("%s: createAudioEngine() error\n", getName());
        goto fail;
    }

    // Create audio ports, and attach them to audio engine.

	if ( createAudioPorts() == false )
    {
        DebugLog("%s: createAudioPorts() error\n", getName());
        goto fail;
    }

    // Activate audio engine.

    if ( activateAudioEngine( _audioEngine ) != kIOReturnSuccess )
    {
        DebugLog("%s: activateAudioEngine() error\n", getName());
        goto fail;
    }

    // Flush default audio control settings.

    flushAudioControls();

    return true;

fail:
    if ( _audioCodec )
    {
        _audioCodec->close( this );
        _audioCodec = 0;
    }
        
    return false;
}

//---------------------------------------------------------------------------

void AppleIntelAC97AudioDriver::free()
{
    RELEASE( _audioEngine );
    super::free();
}

//---------------------------------------------------------------------------

IOReturn AppleIntelAC97AudioDriver::message( UInt32      type,
                                             IOService * provider,
                                             void *      arg )
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    if ( ( provider )
    &&   ( provider == _audioCodec )
    &&   ( type == kIOMessageServiceIsTerminated ) )
    {
        provider->close( this );
        return kIOReturnSuccess;
    }
    
    return super::message( type, provider, arg );
}

//---------------------------------------------------------------------------

bool AppleIntelAC97AudioDriver::createAudioEngine()
{
    AppleIntelAC97AudioEngine * audioEngine = new AppleIntelAC97AudioEngine;
    bool                        success     = false;

    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    do {
        if ( audioEngine == 0 )
        {
            IOLog("%s: audio engine new failed\n", getName());
            break;
        }

        if ( audioEngine->init( 0, this, _audioCodec ) == false )
        {
            IOLog("%s: audio engine init failed\n", getName());
            break;
        }

        success = true;
    }
    while ( false );

    _audioEngine = audioEngine;

    return success;
}

/*
 * IOAudioControl channel ID values.
 */
enum {
    kOutMute = 0,
    kOutVolLeft,
    kOutVolRight,
    kNumControls
};

//---------------------------------------------------------------------------

bool AppleIntelAC97AudioDriver::createAudioPorts()
{
    IOAudioPort *          outputPort  = 0;
    IOAudioLevelControl *  outVolLeft  = 0;
    IOAudioLevelControl *  outVolRight = 0;
    IOAudioToggleControl * outMute     = 0;
    bool                   success     = false;
    UInt32                 initialVolume;

    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    do {
        // Master output port.

        outputPort = IOAudioPort::withAttributes( kIOAudioPortTypeOutput,
                                                  "Master Output port" );
        if ( outputPort == 0 ) break;

        initialVolume = _audioCodec->getOutputVolumeMax() * 3 / 4;

        // Left master output volume.

        outVolLeft = IOAudioLevelControl::createVolumeControl(
                     /* initialValue */   initialVolume,
                     /* minValue     */   _audioCodec->getOutputVolumeMin(),
                     /* maxValue     */   _audioCodec->getOutputVolumeMax(),
                     /* minDB        */   _audioCodec->getOutputVolumeMinDB(),
                     /* maxDB        */   _audioCodec->getOutputVolumeMaxDB(),
                     /* channelID    */   kIOAudioControlChannelIDDefaultLeft,
                     /* channelName  */   kIOAudioControlChannelNameLeft,
                     /* cntrlID      */   kOutVolLeft, 
                     /* usage        */   kIOAudioControlUsageOutput );

        if ( outVolLeft == 0 ) break;

        outVolLeft->setValueChangeHandler(
                    (IOAudioControl::IntValueChangeHandler)
                        audioControlChangeHandler,
                    this );

        _audioEngine->addDefaultAudioControl( outVolLeft );

        // Right master output volume.

        outVolRight = IOAudioLevelControl::createVolumeControl(
                      /* initialValue */   initialVolume,
                      /* minValue     */   _audioCodec->getOutputVolumeMin(),
                      /* maxValue     */   _audioCodec->getOutputVolumeMax(),
                      /* minDB        */   _audioCodec->getOutputVolumeMinDB(),
                      /* maxDB        */   _audioCodec->getOutputVolumeMaxDB(),
                      /* channelID    */   kIOAudioControlChannelIDDefaultRight,
                      /* channelName  */   kIOAudioControlChannelNameRight,
                      /* cntrlID      */   kOutVolRight, 
                      /* usage        */   kIOAudioControlUsageOutput );

        if ( outVolRight == 0 ) break;

        outVolRight->setValueChangeHandler(
                     (IOAudioControl::IntValueChangeHandler)
                         audioControlChangeHandler,
                     this );

        _audioEngine->addDefaultAudioControl( outVolRight );

        // Mute master output.

        outMute = IOAudioToggleControl::createMuteControl(
                  /* initialValue */    false,
                  /* channelID    */    kIOAudioControlChannelIDAll,
                  /* channelName  */    kIOAudioControlChannelNameAll,
                  /* cntrlID      */    kOutMute, 
                  /* usage        */    kIOAudioControlUsageOutput);
        
        if ( outMute == 0 ) break;

        outMute->setValueChangeHandler(
                 (IOAudioControl::IntValueChangeHandler)
                     audioControlChangeHandler,
                 this );

        _audioEngine->addDefaultAudioControl( outMute );

        // Attach audio port to audio topology.

         attachAudioPort( outputPort, _audioEngine, 0 );

         success = true;
    }
    while ( false );

    if ( outputPort  ) outputPort->release();
    if ( outVolLeft  ) outVolLeft->release();
    if ( outVolRight ) outVolRight->release();
	if ( outMute     ) outMute->release();

    return success;
}

//---------------------------------------------------------------------------

IOReturn AppleIntelAC97AudioDriver::audioControlChangeHandler(
                               IOService *      target,
                               IOAudioControl * control,
                               SInt32           oldValue,
                               SInt32           newValue )
{
    AppleIntelAC97AudioDriver * self;
    AppleIntelAC97Codec *       audioCodec = 0;
    
    self = (AppleIntelAC97AudioDriver *) target;

    if ( self ) audioCodec = self->_audioCodec;
    if ( audioCodec == 0 ) return kIOReturnBadArgument;

    switch ( control->getChannelID() )
    {
        case kOutVolLeft:
            audioCodec->setOutputVolumeLeft( newValue );
            break;
        
        case kOutVolRight:
            audioCodec->setOutputVolumeRight( newValue );
            break;
        
        case kOutMute:
            audioCodec->setOutputVolumeMute( newValue );
    }
    
    return kIOReturnSuccess;
}
