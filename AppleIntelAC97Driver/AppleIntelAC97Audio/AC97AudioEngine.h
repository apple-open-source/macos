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

#ifndef __APPLE_INTEL_AC97_AUDIO_ENGINE_H
#define __APPLE_INTEL_AC97_AUDIO_ENGINE_H

#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioTypes.h>
#include "AC97Defines.h"
#include "AC97Codec.h"

class AppleIntelAC97AudioEngine : public IOAudioEngine
{
    OSDeclareDefaultStructors( AppleIntelAC97AudioEngine )

protected:
    IOService *        _provider;        // Our provider
    IOPhysicalAddress  _outBDListPhys;   // output BD physical address
    IOPhysicalAddress  _inBDListPhys;    // input BD physical address
    UInt32             _out48KRate;      // sample rate at 48KHz
    UInt32             _bufferCount;     // number of data buffers
    UInt32             _bufferSize;      // size of each buffer in bytes
    UInt32             _bufferSamples;   // size of each buffer in samples
    bool               _isRunning;       // are we running?
    IOBufferMemoryDescriptor * _outSampleMemory;  // output sample buffer
    IOBufferMemoryDescriptor * _outBDListMemory;  // output descriptors
    IOBufferMemoryDescriptor * _inSampleMemory;   // input sample buffer
    IOBufferMemoryDescriptor * _inBDListMemory;   // input descriptors

    AppleIntelAC97Codec *          _codec;
    IOFilterInterruptEventSource * _interruptEvSrc;
    IOTimerEventSource *           _timerEvSrc;

    static bool interruptFilterHandler( OSObject *                     owner,
                                        IOFilterInterruptEventSource * source );

    static void interruptHandler( OSObject *               owner,
                                  IOInterruptEventSource * source,
                                  int                      count );

    void             interruptFilter( void );

    void             updateDescriptorTail( DMAChannel channel );

    UInt32           setOutputSampleRate( UInt32 rate,
                                          bool   calibrate = false );

    UInt32           setInputSampleRate( UInt32 rate );

    UInt32           getOutputPosition( void );

public:
    virtual bool     init( OSDictionary *        properties,
                           IOService *           provider,
                           AppleIntelAC97Codec * codec );

    virtual void     free( void );

    virtual bool     initHardware( IOService * provider );

    virtual IOReturn performAudioEngineStart( void );

    virtual IOReturn performAudioEngineStop( void );

    virtual UInt32   getCurrentSampleFrame( void );

    virtual IOReturn performFormatChange(
                           IOAudioStream *             audioStream,
                           const IOAudioStreamFormat * newFormat,
                           const IOAudioSampleRate *   newSampleRate );

    virtual IOReturn clipOutputSamples(
                           const void *                mixBuf,
                           void *                      sampleBuf,
                           UInt32                      firstSampleFrame,
                           UInt32                      numSampleFrames,
                           const IOAudioStreamFormat * streamFormat,
                           IOAudioStream *             audioStream );

    virtual IOReturn convertInputSamples(
                           const void *                sampleBuf,
                           void *                      destBuf,
                           UInt32                      firstSampleFrame,
                           UInt32                      numSampleFrames,
                           const IOAudioStreamFormat * streamFormat,
                           IOAudioStream *             audioStream );

    virtual void timerFired( void );
};

#endif /* !__APPLE_INTEL_AC97_AUDIO_ENGINE_H */
