/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOPlatformExpert.h>
#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include "AppleAC97AudioEngine.h"
#include "IOAC97Debug.h"

#define kBytesPerSample   2    // number of bytes per 16-bit sample
#define kNumChannels      2
#define kConfigArraySize  2

#define CLASS AppleAC97AudioEngineSPDIF
#define super AppleAC97AudioEngine

OSDefineMetaClassAndStructors( AppleAC97AudioEngineSPDIF,
                               AppleAC97AudioEngine )

//---------------------------------------------------------------------------

bool CLASS::init( IOAudioDevice * provider, IOAC97AudioCodec * codec )
{
    IOPlatformExpert * platform;

    if (!provider || !codec)
        return false;

    if (super::init(provider, codec) == false)
        return false;

    platform = IOService::getPlatform();
    if (platform && platform->numBatteriesSupported())
        return false;

    fRoot = codec->getAudioController();
    if (!fRoot)
        return false;

    fConfigArray = OSArray::withCapacity(kConfigArraySize);
    if (!fConfigArray)
        return false;

    return true;
}

//---------------------------------------------------------------------------

void CLASS::free( void )
{
    DebugLog("%s::%s %p\n", getName(), __FUNCTION__, this);

    fRoot = 0;
    fConfigActive = 0;

    if (fConfigArray)
    {
        fConfigArray->release();
        fConfigArray = 0;
    }

    if (fOutputStream)
    {
        fOutputStream->release();
        fOutputStream = 0;
    }

    super::free();
}

//---------------------------------------------------------------------------

bool CLASS::initAudioConfigurations( void )
{
    IOAudioSampleRate   sampleRate;
    IOAC97AudioConfig * config;
    IOReturn            ret;

    // Generate configurations for AC3 and PCM SPDIF streams.

    if (!fConfigArray) return false;
    fConfigArray->flushCollection();

    // PCM @ 48K configuration

    config = IOAC97AudioConfig::audioConfig(
                 kIOAC97DMAEngineTypeAudioSPDIF,
                 kIOAC97DMADataDirectionOutput,
                 kNumChannels,
                 kIOAC97SampleFormatPCM16,
                 kIOAC97SampleRate48K );

    if (config &&
        fRoot->prepareAudioConfiguration(config) == kIOReturnSuccess &&
        config->isValid())
    {
        config->setClientData((void *)kIOAudioStreamSampleFormatLinearPCM);
        fConfigArray->setObject(config);
        DebugLog("%s: PCM 48K config success\n", getName());
    }
    if (config)
    {
        config->release();
        config = 0;
    }

    // AC3 @ 48K configuration

    config = IOAC97AudioConfig::audioConfig(
                 kIOAC97DMAEngineTypeAudioSPDIF,
                 kIOAC97DMADataDirectionOutput,
                 kNumChannels,
                 kIOAC97SampleFormatAC3,
                 kIOAC97SampleRate48K );

    if (config &&
        fRoot->prepareAudioConfiguration(config) == kIOReturnSuccess &&
        config->isValid())
    {
        config->setClientData((void *)kIOAudioStreamSampleFormat1937AC3);
        fConfigArray->setObject(config);
        DebugLog("%s: AC3 48K config success\n", getName());
    }
    if (config)
    {
        config->release();
        config = 0;
    }

    // Activate the first supported configuration.

    config = (IOAC97AudioConfig *) fConfigArray->getObject(0);
    if (config == 0)
    {
        DebugLog("%s: no SPDIF configuration\n", getName());
        return false;
    }

    ret = fRoot->activateAudioConfiguration(
                 config, this, interruptHandler, NULL );

    if (ret != kIOReturnSuccess)
    {
        DebugLog("%s: activateAudioConfiguration failed\n", getName());
        return false;
    }

    // Set the engine's initial sample rate. Is this necessary?

    sampleRate.whole = config->getSampleRate();
    sampleRate.fraction = 0;
    setSampleRate( &sampleRate );

    fConfigActive = config;

    return true;
}

//---------------------------------------------------------------------------

bool CLASS::initAudioStreams( void )
{
    IOAC97AudioConfig *  config;
    IOAudioSampleRate    sampleRate;
    IOAudioStreamFormat  streamFormat =
    {
    /* fNumChannels */   kNumChannels,
    /* fSampleFormat */  0,
    /* fNumericRep */    kIOAudioStreamNumericRepresentationSignedInt,
    /* fBitDepth  */     16,
    /* fBitWidth  */     16,
    /* fAlignment */     kIOAudioStreamAlignmentLowByte,
    /* fByteOrder */     kIOAudioStreamByteOrderLittleEndian,
    /* fIsMixable */     true,
    /* fDriverTag */     0
    };

    // Create an audio stream for SPDIF.

    fOutputStream = new IOAudioStream;
    if ( fOutputStream == 0 ||
         fOutputStream->initWithAudioEngine( this,
                        /* direction */      kIOAudioStreamDirectionOutput,
                        /* startChannelID */ 1 ) != true )
    {
        DebugLog("%s: output stream error\n", getName());
        return false;
    }

    // Go through our configuration array and add a stream format
    // for each supported configuration.

    for (int i = 0;
         config = (IOAC97AudioConfig *) fConfigArray->getObject(i);
         i++)
    {
        streamFormat.fSampleFormat = (UInt32) config->getClientData();
        sampleRate.whole           = config->getSampleRate();
        sampleRate.fraction        = 0;
        streamFormat.fDriverTag    = i;

        fOutputStream->addAvailableFormat(
                       /* streamFormat */  &streamFormat,
                            /* minRate */  &sampleRate,
                            /* maxRate */  &sampleRate );

        DebugLog("%s: added SPDIF format %lx @ %u format to output stream\n",
                 getName(), streamFormat.fSampleFormat, sampleRate.whole);
    }

    // Add output stream to the engine.    

    addAudioStream( fOutputStream );

    // Apply the initial audio format. This will cause performFormatChange
    // to be called.

    if (fConfigActive)
    {
        streamFormat.fNumChannels  = fConfigActive->getAudioChannelCount();
        streamFormat.fSampleFormat = (UInt32)fConfigActive->getClientData();
        streamFormat.fDriverTag    = 0;
        fOutputStream->setFormat( &streamFormat );
        DebugLog("%s: set SPDIF format to %lx, tag %lu\n",
                 getName(), streamFormat.fSampleFormat,
                 streamFormat.fDriverTag);
    }

    return true;
}

//---------------------------------------------------------------------------

bool CLASS::initHardware( IOService * provider )
{
    DebugLog("%s::%s(%p)\n", getName(), __FUNCTION__, provider);

    if (super::initHardware(provider) == false)
    {
        DebugLog("%s: super::initHardware failed\n", getName());
        goto exit;
    }

    if (initAudioConfigurations() == false)
    {
        DebugLog("%s: initAudioConfigurations failed\n", getName());
        goto exit;
    }

    if (initAudioStreams() == false)
    {
        DebugLog("%s: initAudioStreams failed\n", getName());
        goto exit;
    }

    // Report the number of samples ahead of the playback head that
    // is safe to write into the sample buffer.

    setSampleOffset(  kSampleOffset  );
    setSampleLatency( kSampleLatency );
    setDescription( "AC97 Audio (SPDIF)" );
    return true;

exit:
    stopHardware(provider);
    return false;
}

//---------------------------------------------------------------------------

void CLASS::stopHardware( IOService * provider )
{
    if (fRoot && fConfigActive)
    {
        fRoot->deactivateAudioConfiguration( fConfigActive );
        fConfigActive = 0;
    }
}

//---------------------------------------------------------------------------

IOReturn CLASS::performAudioEngineStart( void )
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    takeTimeStamp( false );

    fIsRunning = true;
    fRoot->startDMAEngine( fDMAEngineID );

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::performAudioEngineStop( void )
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    fRoot->stopDMAEngine( fDMAEngineID );
    fIsRunning = false;    

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void CLASS::interruptHandler( void * target, void * param )
{
    CLASS * me = (CLASS *) target;

    if (me && me->fIsRunning)
    {
        me->takeTimeStamp();
    }
}

//---------------------------------------------------------------------------

UInt32 CLASS::getCurrentSampleFrame( void )
{
    UInt32 sampleFrame;

    // Report the start of the current buffer in sample frame units.
    // This tells the audio family that it is safe to erase samples
    // up to but not including the current position.

    sampleFrame = fRoot->getDMAEngineHardwarePointer(fDMAEngineID) /
                  (fNumChannels * kBytesPerSample);

    DebugLog("[SF: %lx]\n", sampleFrame);

    return sampleFrame;
}

//---------------------------------------------------------------------------

void CLASS::timerFired( void )
{
    // Override the call from IOAudioEngine to periodically update the
    // tail of the descriptor list(s).

    if ( fIsRunning )
    {
        fRoot->startDMAEngine(fDMAEngineID);
    }

    super::timerFired();
}

//---------------------------------------------------------------------------

void CLASS::setActiveConfiguration( IOAC97AudioConfig * config )
{
    OSArray *    array;
    IOItemCount  numAudioSamples;
    IOItemCount  numSampleFrames;
    UInt32       sampleBufferSize;

    IOBufferMemoryDescriptor * sampleBuffer;
    IOAudioSampleRate          sampleRate;

    DebugLog("%s::%s(%p)\n", getName(), __FUNCTION__, config);

    IOAC97Assert(config != 0);
    sampleBuffer = config->getDMABufferMemory();
    IOAC97Assert(sampleBuffer != 0);

    sampleBufferSize = config->getDMABufferCount() *
                       config->getDMABufferSize();
    IOAC97Assert(sampleBufferSize > 0);

    // Install the active configuration

    fConfigActive = config;
    fDMAEngineID  = config->getDMAEngineID();
    fNumChannels  = config->getAudioChannelCount();
    DebugLog("%s: DMA engine ID = %lu\n", getName(), fDMAEngineID);
    DebugLog("%s: channel count = %lu\n", getName(), fNumChannels);

    if (fOutputStream)
    {
        DebugLog("%s: output sample buffer changed\n", getName());
        fOutputStream->setSampleBuffer( sampleBuffer->getBytesNoCopy(),
                                        sampleBufferSize );
    }

    numAudioSamples = sampleBufferSize / kBytesPerSample;
    numSampleFrames = numAudioSamples / fNumChannels;
    DebugLog("%s: buffer samples %lu sample frames %lu\n",
             getName(), numAudioSamples, numSampleFrames);

    // Report the size of the sample buffer in sample frame units.

    setNumSampleFramesPerBuffer( numSampleFrames );
    
    // Report the engine's sample rate.

    sampleRate.whole = config->getSampleRate();
    sampleRate.fraction = 0;
    setSampleRate( &sampleRate );

    // Create audio controls for this configuration, then add the controls
    // to the audio engine.

    if (fControlsAdded) return;
    fControlsAdded = true;

    array = OSArray::withCapacity( 2 );
    if (array)
    {
        IOAudioControl * control;

        fRoot->createAudioControls( config, array );

        for (int i = 0;
             control = OSDynamicCast(IOAudioControl, array->getObject(i));
             i++)
        {
            addDefaultAudioControl( control );
            control->flushValue();
        }

        array->release();
    }
}

//---------------------------------------------------------------------------

IOReturn CLASS::performFormatChange(
                       IOAudioStream *             audioStream,
                       const IOAudioStreamFormat * newFormat,
                       const IOAudioSampleRate *   newSampleRate )
{
    IOReturn ret = kIOReturnUnsupported;
    UInt32   rate;

    DebugLog("%s::%s(%p, %p, %p)\n", getName(), __FUNCTION__,
             audioStream, newFormat, newSampleRate);

    rate = (newSampleRate) ? newSampleRate->whole :
           (fConfigActive) ? fConfigActive->getSampleRate() : 0;

    if (newFormat)
    {
        IOAC97AudioConfig * config;
        IOAC97AudioConfig * newConfig = 0;

        DebugLog("%s::%s new format %lx rate %u\n",
                 getName(), __FUNCTION__, newFormat->fSampleFormat, rate);

        // Stream formats with different sample rates will be called with
        // the same fDriverTag. So search the configuration list manually
        // rather than using newFormat->fDriverTag as an array index.

        for (int i = 0;
             config = (IOAC97AudioConfig *)fConfigArray->getObject(i);
             i++)
        {
            if ((config->getClientData() == (void *)newFormat->fSampleFormat)
            &&  (!rate || config->getSampleRate() == rate))
            {
                newConfig = config;
                break;
            }
        }

        if (newConfig)
        {
            ret = kIOReturnSuccess;

            if (newConfig != fConfigActive)
            {
                // Close the currently active configuration.

                fRoot->deactivateAudioConfiguration( fConfigActive );
    
                // Open the new configuration. Recover from errors by
                // re-opening the old config.

                ret = fRoot->activateAudioConfiguration(
                             newConfig, this, interruptHandler, NULL );
                if (ret != kIOReturnSuccess)
                {
                    DebugLog("%s: configuration change failed\n", getName());
                    fRoot->activateAudioConfiguration(
                           fConfigActive, this, interruptHandler, NULL );
                    newConfig = fConfigActive;
                }

                //updateChannelNumbers();
            }

            setActiveConfiguration( newConfig );
        }
    }

    DebugLog("%s::%s return value %lx\n", getName(), __FUNCTION__, ret);
    return ret;
}

//---------------------------------------------------------------------------

IOReturn CLASS::handlePowerStateChange( UInt32 whatState )
{
    IOReturn ior = kIOReturnSuccess;

    DebugLog("%s::%s state = %lu\n", getName(), __FUNCTION__, whatState);

    IOAC97Assert(fIsRunning == false);

    if ((whatState == kPowerStateActive) && fIsSleeping)
    {
        if (fConfigActive)
            ior = fRoot->activateAudioConfiguration(
                  fConfigActive, this, interruptHandler, NULL );
        fIsSleeping = false;
    }
    else if ((whatState == kPowerStateSleep) && !fIsSleeping)
    {
        if (fConfigActive)
            fRoot->deactivateAudioConfiguration( fConfigActive );
        fIsSleeping = true;
    }

    return ior;
}
