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

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <libkern/OSByteOrder.h>

#include "AC97AudioEngine.h"
#include "AC97Debug.h"

#define kNumChannels       2    // number of audio channels (stereo = 2)
#define kBytesPerSample    2    // number of bytes per 16-bit sample
#define kNumDescriptors    32   // number of buffer descriptors (fixed)
#define k48KSampleRate     48000

#define super IOAudioEngine
OSDefineMetaClassAndStructors( AppleIntelAC97AudioEngine, IOAudioEngine )

//---------------------------------------------------------------------------

bool AppleIntelAC97AudioEngine::init( OSDictionary *        properties,
                                      IOService *           provider,
                                      AppleIntelAC97Codec * codec )
{
    // Let IOAudioEngine initialize first.

    if ( super::init( properties ) == false ) return false;

    // The (primary) codec object that the driver is attached to.

    _codec = codec;

    // Keep a reference to our provider, an IOAudioDevice.

    _provider = provider;

    // Set audio engine properties.

    _bufferCount   = kNumDescriptors;  // number of buffers/descriptors
    _bufferSize    = PAGE_SIZE / 2;    // bytes per buffer
    _bufferSamples = _bufferSize / kBytesPerSample;

    // Report the size of the sample buffer in sample frame units.

    setNumSampleFramesPerBuffer( _bufferCount *
                                 _bufferSamples / kNumChannels);

    // Report the number of samples ahead of the playback head that
    // is safe to write into the sample buffer.

    setSampleOffset( 64 /* very conservative value */ );

    return true;
}

//---------------------------------------------------------------------------

void AppleIntelAC97AudioEngine::free()
{
    if ( _outSampleBuf )
    {
        IOFreeAligned( _outSampleBuf, _outSampleBufSize );
        _outSampleBuf = 0;
    }

    if ( _outBDList )
    {
        IOFreeAligned( _outBDList, _outBDListSize );
        _outBDList = 0;
    }

    RELEASE( _interruptEvSrc );
    RELEASE( _timerEvSrc );

    super::free();
}

//---------------------------------------------------------------------------

bool AppleIntelAC97AudioEngine::initHardware( IOService * provider )
{
    bool                      success  = false;
	IOWorkLoop *              workLoop = getWorkLoop();
    IOAudioSampleRate         sampleRate;
    IOAudioStream *           stream;
    IOAudioStreamFormat       streamFormat =
    {
        /* fNumChannels */    2,     
                              kIOAudioStreamSampleFormatLinearPCM,
                              kIOAudioStreamNumericRepresentationSignedInt,
        /* fBitDepth  */      16,    
        /* fBitWidth  */      16,    
                              kIOAudioStreamAlignmentLowByte,
                              kIOAudioStreamByteOrderLittleEndian,
        /* fIsMixable */      true,  
    };

    DebugLog("%s::%s(%p)\n", getName(), __FUNCTION__, provider);

    if ( super::initHardware(provider) == false )
    {
        IOLog("%s: super::initHardware failed\n", getName());
        goto exit;
    }

	// Allocate memory for the output sample buffer.

    _outSampleBufSize = round_page( _bufferCount * _bufferSize );
    _outSampleBuf     = IOMallocAligned( _outSampleBufSize, PAGE_SIZE );

    if ( _outSampleBuf == 0 )
    {
        IOLog("%s: failed to allocate %ld bytes for output sample buffer\n",
              getName(), _outSampleBufSize);
        goto exit;
    }

    bzero( _outSampleBuf, _outSampleBufSize );

    // Allocate memory for the buffer descriptors.

    _outBDListSize = sizeof(AC97BD) * _bufferCount;
    _outBDList     = (AC97BD *) IOMallocAligned( _outBDListSize, PAGE_SIZE );

    if ( _outBDList == 0 )
    {
        IOLog("%s: failed to allocate %ld bytes for output descriptor list\n",
              getName(), _outBDListSize);
        goto exit;
    }

    // Get the physical address of the buffer descriptor list.

    _outBDListPhys = pmap_extract( kernel_pmap, (vm_address_t)_outBDList );

    DebugLog("%s: BDList virt = %p phys = %lx\n", getName(),
             _outBDList, _outBDListPhys);

    // Initialize buffer descriptor list.

    for ( UInt32 i = 0; i < _bufferCount; i++ )
    {
        OSWriteLittleInt32( &_outBDList[i].pointer, 0,
                            pmap_extract( kernel_pmap,
                                          (IOVirtualAddress) _outSampleBuf
                                          + (i * _bufferSize) ) );
        
        OSWriteLittleInt16( &_outBDList[i].length, 0, _bufferSamples );
        OSWriteLittleInt16( &_outBDList[i].command, 0, 0 );
    }

    // A timestamp is required by the audio family every time the engine
    // loops around the sample buffer. To do this, arm an interrupt on the
    // last descriptor, and have the primary interrupt handler record the
    // time stamp.

    OSWriteLittleInt16( &_outBDList[kNumDescriptors - 1].command, 0,
                        kInterruptOnCompletion );

    // Install an interrupt handler to take time stamps.

    if ( workLoop == 0 )
    {
        IOLog("%s: No work loop\n", getName());
        goto exit;
    }

    // Locate the IOPCIDevice that has the interrupt properties.

    IOService * pciDevice;
	for ( pciDevice = provider; pciDevice; pciDevice = pciDevice->getProvider() )
    {
        if ( pciDevice->metaCast( "IOPCIDevice" ) )
        {
            break;
        }
    }

    _interruptEvSrc =
    IOFilterInterruptEventSource::filterInterruptEventSource(
                     this,
                     AppleIntelAC97AudioEngine::interruptHandler,
                     AppleIntelAC97AudioEngine::interruptFilterHandler,
                     pciDevice,
                     0 );

    if ( _interruptEvSrc == 0 )
    {
        IOLog("%s: unable to create interrupt event source\n", getName());
        goto exit;
    }
    workLoop->addEventSource( _interruptEvSrc );

    // Since multiple PCI devices may share the same interrupt line, and
    // the interrupt line will be masked when any of the attached interrupt
    // sources is disabled. We enable the interrupt source right away.

    _interruptEvSrc->enable();

	// Create the output audio stream.

	stream = new IOAudioStream;
    if ( stream == 0 ||
         stream->initWithAudioEngine( this,
                 /* direction */      kIOAudioStreamDirectionOutput,
                 /* startChannelID */ 1 ) != true )
    {
        IOLog("%s: output stream new/init error\n", getName());
        goto exit;
    }

    // Calibrate output sample rate.

    if ( setOutputSampleRate( k48KSampleRate, true ) != k48KSampleRate )
    {
        IOLog("%s: output sample rate calibration error\n", getName());
        goto exit;
    }

    // Add support for 48000 Hz sample rate.

    sampleRate.whole = 48000; 
    sampleRate.fraction = 0;
	if ( setOutputSampleRate( sampleRate.whole ) == sampleRate.whole )
    {
        stream->addAvailableFormat( &streamFormat, &sampleRate, &sampleRate );
        setSampleRate( &sampleRate );
    }

    // Add support for 44100 Hz sample rate.

    sampleRate.whole = 44100; 
    sampleRate.fraction = 0;
	if ( setOutputSampleRate( sampleRate.whole ) == sampleRate.whole )
    {
        stream->addAvailableFormat( &streamFormat, &sampleRate, &sampleRate );
        setSampleRate( &sampleRate );
    }

    stream->setSampleBuffer( _outSampleBuf, _bufferCount * _bufferSize );
    stream->setFormat( &streamFormat );
    addAudioStream( stream );
    stream->release();

    DebugLog("%s::%s(%p) success!\n", getName(), __FUNCTION__, provider);

    success = true;

exit:
    return success;
}

//---------------------------------------------------------------------------

UInt32
AppleIntelAC97AudioEngine::setOutputSampleRate( UInt32 rate,
                                                bool   calibrate = false )
{

    UInt32 dacRate;
    UInt32 actualDACRate;

    DebugLog("%s::%s(%ld)\n", getName(), __FUNCTION__, rate);

    if ( calibrate )
    {
        UInt32 before, after;
    	AbsoluteTime now, deadline;
        
        // Set the DAC to 48KHz, this should always succeed.
    
        _codec->setDACSampleRate( k48KSampleRate );
    
        // Measure the real sampling rate since some machines are misclocked.
    
        _codec->setDescriptorBaseAddress( kChannelPCMOut, _outBDListPhys );
    
        updateDescriptorTail( kChannelPCMOut );
    
        _codec->startDMAChannel( kChannelPCMOut );

        before = getOutputPosition();

        // Busy poll for 50 milliseconds. Make sure the descriptor list is
        // large enough without stalling the engine.
    
        clock_interval_to_deadline( 50, kMillisecondScale, &deadline );
        do {
            clock_get_uptime( &now );
        } while ( CMP_ABSOLUTETIME(&deadline, &now) > 0 );

        after = getOutputPosition();

        _codec->stopDMAChannel( kChannelPCMOut );
    
        // Computed the measured sample rate when 48KHz rate was programmed.
    
        _out48KRate = ( after - before ) / ( kNumChannels * kBytesPerSample ) *
                      20;  /* 1 / 50ms */

        DebugLog("%s: measured DAC 48K sample rate: %ld (%ld-%ld)\n", getName(),
                 _out48KRate, before, after);
    
        // Ignore small variances.

        if ( _out48KRate < 48500 && _out48KRate > 47500 )
            _out48KRate = k48KSampleRate;

        setProperty( "DAC 48KHz Sample Rate", _out48KRate, 32 );
    }

    // Compute the value that should be programmed on the DAC to achieve
    // the desired sampling rate.

    if ( _out48KRate == 0 ) return 0;

	dacRate = rate * k48KSampleRate / _out48KRate;

    _codec->setDACSampleRate( dacRate, &actualDACRate );

    if ( actualDACRate != dacRate )
        rate = actualDACRate * _out48KRate / k48KSampleRate;

	return rate;
}

//---------------------------------------------------------------------------

IOReturn AppleIntelAC97AudioEngine::performAudioEngineStart()
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    // Record the timestamp.

    takeTimeStamp( false );

    // Set the buffer descriptor list base address register.

	_codec->setDescriptorBaseAddress( kChannelPCMOut, _outBDListPhys );

    // Set the last valid index.
    
	updateDescriptorTail( kChannelPCMOut );

    // Start the output DMA engine.

    _codec->startDMAChannel( kChannelPCMOut );
    _isRunning = true;
        
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn AppleIntelAC97AudioEngine::performAudioEngineStop()
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    // Stop the output DMA engine.

    _isRunning = false;
	_codec->stopDMAChannel( kChannelPCMOut );

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void AppleIntelAC97AudioEngine::interruptFilter()
{
    if ( _codec->serviceChannelInterrupt( kChannelPCMOut ) )
    {
        takeTimeStamp();
    }
}

//---------------------------------------------------------------------------

bool AppleIntelAC97AudioEngine::interruptFilterHandler(
                                    OSObject *                     owner,
                                    IOFilterInterruptEventSource * source )
{
	AppleIntelAC97AudioEngine * self = (AppleIntelAC97AudioEngine *) owner;
    if ( self )
    {
        self->interruptFilter();
    }
    return false;
}

//---------------------------------------------------------------------------

void
AppleIntelAC97AudioEngine::interruptHandler( OSObject *               owner,
                                             IOInterruptEventSource * source,
                                             int                      count )
{
    /* Not called */
}

//---------------------------------------------------------------------------

UInt32 AppleIntelAC97AudioEngine::getCurrentSampleFrame()
{
    // Report the start of the current buffer in sample frame units.
    // This tells the audio family that it is safe to erase samples
    // up to but not including the current position.

    return ( getOutputPosition() & ~(1024 - 1) ) /
           ( kNumChannels * kBytesPerSample );
}

//---------------------------------------------------------------------------

IOReturn AppleIntelAC97AudioEngine::performFormatChange(
                           IOAudioStream *             audioStream,
                           const IOAudioStreamFormat * newFormat,
                           const IOAudioSampleRate *   newSampleRate )
{
    if ( newFormat )
    {
        DebugLog("%s::%s %d bits per sample\n", getName(), __FUNCTION__,
                 newFormat->fBitDepth);
    }

    if ( newSampleRate )
    {
        DebugLog("%s::%s %ld sample rate\n", getName(), __FUNCTION__,
                 newSampleRate->whole);
        
        setOutputSampleRate( newSampleRate->whole );
    }

	return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void AppleIntelAC97AudioEngine::updateDescriptorTail( DMAChannel channel )
{
    UInt8 index = _codec->getCurrentIndexValue( channel );

    // Update the last valid index to the descriptor before the current
    // descriptor.

    _codec->setLastValidIndex( channel, (index - 1) & (kNumDescriptors - 1) );   
}

//---------------------------------------------------------------------------

void AppleIntelAC97AudioEngine::timerFired()
{
    // Override the call from IOAudioEngine to periodically update the
    // tail of the descriptor list.

    updateDescriptorTail( kChannelPCMOut );
    super::timerFired();
}

//---------------------------------------------------------------------------

UInt32 AppleIntelAC97AudioEngine::getOutputPosition()
{
    UInt8  index;
    UInt32 samplesRemain;

    samplesRemain = _codec->getCurrentBufferPosition( kChannelPCMOut, &index );

    return ( ((index + 1) * _bufferSize) - (samplesRemain * kBytesPerSample) );
}
