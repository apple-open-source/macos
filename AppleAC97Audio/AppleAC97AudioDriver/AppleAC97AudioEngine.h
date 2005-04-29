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

#ifndef __APPLEAC97AUDIOENGINE_H
#define __APPLEAC97AUDIOENGINE_H

#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioTypes.h>
#include "IOAC97AudioCodec.h"
#include "IOAC97Controller.h"
#include "IOAC97AudioConfig.h"

#define ACTIVATE_LOWEST_CONFIG_FIRST  1

/*
 * AppleAC97AudioEngine
 */
class AppleAC97AudioEngine : public IOAudioEngine
{
    OSDeclareAbstractStructors( AppleAC97AudioEngine )

public:
    enum {
        kPowerStateSleep,
        kPowerStateActive
    };

    enum {
        kSampleOffset  = 64,
        kSampleLatency = 32
    };

    virtual bool     init( IOAudioDevice *    provider,
                           IOAC97AudioCodec * codec );

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

    virtual IOReturn handlePowerStateChange( UInt32 whatState ) = 0;
};

/*
 * AppleAC97AudioEnginePCMOut
 */
class AppleAC97AudioEnginePCMOut : public AppleAC97AudioEngine
{
    OSDeclareDefaultStructors( AppleAC97AudioEnginePCMOut )

protected:
    enum {
        kAudioConfigPCMOut2 = 0,
        kAudioConfigPCMOut4,
        kAudioConfigPCMOut6,
        kAudioConfigCount
    };

    IOAC97Controller *   fRoot;
    bool                 fIsRunning;
    bool                 fIsSleeping;
    IOAudioStream *      fOutputStream;
    IOAC97AudioConfig *  fConfigActive;
    OSArray *            fConfigArray;
    IOItemCount          fNumChannels;
    IOAC97DMAEngineID    fDMAEngineID;
    

    static void      interruptHandler( void * target, void * param );

    virtual bool     initAudioConfigurations( void );

    virtual bool     initAudioStreams( void );

    virtual void     setActiveConfiguration(
                           IOAC97AudioConfig * config );

public:
    virtual bool     init( IOAudioDevice *    provider,
                           IOAC97AudioCodec * codec );

    virtual void     free( void );

    virtual bool     initHardware( IOService * provider );

    virtual void     stopHardware( IOService * provider );

    virtual IOReturn performAudioEngineStart( void );

    virtual IOReturn performAudioEngineStop( void );

    virtual UInt32   getCurrentSampleFrame( void );

    virtual void     timerFired( void );

    virtual IOReturn performFormatChange(
                           IOAudioStream *             audioStream,
                           const IOAudioStreamFormat * newFormat,
                           const IOAudioSampleRate *   newSampleRate );

    virtual IOReturn handlePowerStateChange( UInt32 whatState );
};

/*
 * AppleAC97AudioEnginePCMIn
 */
class AppleAC97AudioEnginePCMIn : public AppleAC97AudioEngine
{
    OSDeclareDefaultStructors( AppleAC97AudioEnginePCMIn )

protected:
    IOAC97Controller *   fRoot;
    bool                 fIsRunning;
    bool                 fIsSleeping;
    bool                 fControlsAdded;
    IOAudioStream *      fInputStream;
    IOAC97AudioConfig *  fConfigActive;
    OSArray *            fConfigArray;
    IOItemCount          fNumChannels;
    IOAC97DMAEngineID    fDMAEngineID;

    static void      interruptHandler( void * target, void * param );

    virtual bool     initAudioConfigurations( void );

    virtual bool     initAudioStreams( void );

    virtual void     setActiveConfiguration(
                           IOAC97AudioConfig * config );

public:
    virtual bool     init( IOAudioDevice *    provider,
                           IOAC97AudioCodec * codec );

    virtual void     free( void );

    virtual bool     initHardware( IOService * provider );

    virtual void     stopHardware( IOService * provider );

    virtual IOReturn performAudioEngineStart( void );

    virtual IOReturn performAudioEngineStop( void );

    virtual UInt32   getCurrentSampleFrame( void );

    virtual void     timerFired( void );

    virtual IOReturn performFormatChange(
                           IOAudioStream *             audioStream,
                           const IOAudioStreamFormat * newFormat,
                           const IOAudioSampleRate *   newSampleRate );

    virtual IOReturn handlePowerStateChange( UInt32 whatState );
};

/*
 * AppleAC97AudioEngineSPDIF
 */
class AppleAC97AudioEngineSPDIF : public AppleAC97AudioEngine
{
    OSDeclareDefaultStructors( AppleAC97AudioEngineSPDIF )

protected:
    IOAC97Controller *   fRoot;
    bool                 fIsRunning;
    bool                 fIsSleeping;
    bool                 fControlsAdded;
    IOAudioStream *      fOutputStream;
    IOAC97AudioConfig *  fConfigActive;
    OSArray *            fConfigArray;
    IOItemCount          fNumChannels;
    IOAC97DMAEngineID    fDMAEngineID;

    static void      interruptHandler( void * target, void * param );

    virtual bool     initAudioConfigurations( void );

    virtual bool     initAudioStreams( void );

    virtual void     setActiveConfiguration(
                           IOAC97AudioConfig * config );

public:
    virtual bool     init( IOAudioDevice *    provider,
                           IOAC97AudioCodec * codec );

    virtual void     free( void );

    virtual bool     initHardware( IOService * provider );

    virtual void     stopHardware( IOService * provider );

    virtual IOReturn performAudioEngineStart( void );

    virtual IOReturn performAudioEngineStop( void );

    virtual UInt32   getCurrentSampleFrame( void );

    virtual void     timerFired( void );

    virtual IOReturn performFormatChange(
                           IOAudioStream *             audioStream,
                           const IOAudioStreamFormat * newFormat,
                           const IOAudioSampleRate *   newSampleRate );

    virtual IOReturn handlePowerStateChange( UInt32 whatState );
};

#endif /* !__APPLEAC97AUDIOENGINE_H */
