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

#include <sys/systm.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/audio/IOAudioPort.h>
#include "AppleAC97AudioDriver.h"
#include "IOAC97Debug.h"

// Enter idle state after this number of seconds.
#define kIdleTimerSeconds  63

#define CLASS AppleAC97AudioDriver
#define super IOAudioDevice
OSDefineMetaClassAndStructors( AppleAC97AudioDriver, IOAudioDevice )

//---------------------------------------------------------------------------

IOService * CLASS::probe( IOService * provider, SInt32 * score )
{
    IOAC97AudioCodec * codec;

    if (super::probe(provider, score) == 0)
        return 0;

    codec = OSDynamicCast( IOAC97AudioCodec, provider );
    if (codec && (codec->getCodecID() == 0))
    {
        return this;
    }

    return 0;
}

//---------------------------------------------------------------------------
// initHardware (inherited from IOAudioDevice)

bool CLASS::initHardware( IOService * provider )
{
    const OSString * rootName;
    const OSString * codecName;
    
    DebugLog("%s::%s(%p)\n", getName(), __FUNCTION__, provider);

    if (super::initHardware(provider) == false)
    {
        goto fail;
    }

    // Allocate a callout entry to perform async power changes.

    fPowerChangeThreadCall = thread_call_allocate(
                             (thread_call_func_t)  powerChangeThreadHandler,
                             (thread_call_param_t) this );

    if (fPowerChangeThreadCall == 0)
        goto fail;

    // Hardware is fully enabled. Change currentPowerState to override the
    // power state reported by getPowerState() so IOAudioFamily will power
    // down to idle after idle timer expiration.

    currentPowerState = kIOAudioDeviceActive;

    // Keep a reference to our provider. Must be an audio codec.

    fAudioCodec = OSDynamicCast( IOAC97AudioCodec, provider );
    if ( fAudioCodec == 0 )
    {
        goto fail;
    }

    fAudioRoot = fAudioCodec->getAudioController();
    if ( fAudioRoot == 0 )
    {
        goto fail;
    }

    setDeviceTransportType(kIOAudioDeviceTransportTypeBuiltIn);
    setDeviceName("AC97 Built-in Audio");
    setDeviceShortName("AC97Audio");

    rootName  = OSDynamicCast(OSString,
                fAudioRoot->getProperty(kIOAC97HardwareNameKey));
    codecName = OSDynamicCast(OSString,
                fAudioCodec->getProperty(kIOAC97HardwareNameKey));
    if (rootName && codecName)
    {
        char * nameBuf;
        int    nameLength = rootName->getLength() + codecName->getLength() +
                            strlen(" / ") + 1;
        
        if ((nameBuf = (char *)IOMalloc(nameLength)))
        {
            snprintf(nameBuf, nameLength, "%s / %s",
                     rootName->getCStringNoCopy(),
                     codecName->getCStringNoCopy());
            setManufacturerName(nameBuf);
            IOFree(nameBuf, nameLength);
        }
    }

    if ( fAudioCodec->open( this ) == false )
    {
        DebugLog("%s: codec open error\n", getName());
        goto fail;
    }

    if ( createAudioEngines() == false )
    {
        DebugLog("%s: createAudioEngine() error\n", getName());
        goto fail;
    }

    if ( createAudioPorts() == false )
    {
        DebugLog("%s: createAudioPorts() error\n", getName());
        goto fail;
    }

    if ( engageAudioEngines() == false )
    {
        DebugLog("%s: engageAudioEngines() error\n", getName());
        goto fail;
    }

    // After engine goes idle, wait a while before the driver is instructed
    // to enter idle power state. This reduces power transitions when audio
    // engine is stopped then restarted within a small time interval.

    setIdleAudioSleepTime( kIdleTimerSeconds * 1000000000ULL );

    return true;

fail:
    if ( fAudioCodec )
    {
        fAudioCodec->close( this );
        fAudioCodec = 0;
    }

    return false;
}

//---------------------------------------------------------------------------

#ifndef RELEASE
#define RELEASE(x) do { if (x) { (x)->release(); (x) = 0; } } while (0)
#endif

void CLASS::free()
{
    RELEASE( fEnginePCMOut );
    RELEASE( fEnginePCMIn  );
    RELEASE( fEngineSPDIF  );
    RELEASE( fInputAudioPort  );
    RELEASE( fOutputAudioPort );

    if ( fPowerChangeThreadCall )
    {
        thread_call_free( fPowerChangeThreadCall );
        fPowerChangeThreadCall = 0;
    }

    fAudioCodec = 0;
    fAudioRoot  = 0;

    super::free();
}

//---------------------------------------------------------------------------

IOReturn CLASS::message( UInt32      type,
                         IOService * provider,
                         void *      arg )
{
    DebugLog("%s::%s type=%lx provider=%p arg=%p\n", getName(), __FUNCTION__,
             type, provider, arg);

    if ( ( provider )
    &&   ( provider == fAudioCodec )
    &&   ( type == kIOMessageServiceIsTerminated ) )
    {
        provider->close( this );
        return kIOReturnSuccess;
    }

    return super::message( type, provider, arg );
}

//---------------------------------------------------------------------------

bool CLASS::createAudioEngines( void )
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    fEnginePCMOut = new AppleAC97AudioEnginePCMOut;
    if (fEnginePCMOut && !fEnginePCMOut->init(this, fAudioCodec))
    {
        fEnginePCMOut->release();
        fEnginePCMOut = 0;
    }

    fEnginePCMIn = new AppleAC97AudioEnginePCMIn;
    if (fEnginePCMIn && !fEnginePCMIn->init(this, fAudioCodec))
    {
        fEnginePCMIn->release();
        fEnginePCMIn = 0;
    }

    fEngineSPDIF = new AppleAC97AudioEngineSPDIF;
    if (fEngineSPDIF && !fEngineSPDIF->init(this, fAudioCodec))
    {
        fEngineSPDIF->release();
        fEngineSPDIF = 0;
    }

    return (fEnginePCMOut && fEnginePCMIn);
}

//---------------------------------------------------------------------------

bool CLASS::createAudioPorts( void )
{
    bool success = false;

    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    do {
        // Create and attach master output port.

        fOutputAudioPort = IOAudioPort::withAttributes(
                                        kIOAudioPortTypeOutput,
                                        "Master Output Port" );
        if ( fOutputAudioPort == 0 )
            break;

        // Create and attach master input port.

        fInputAudioPort = IOAudioPort::withAttributes(
                                       kIOAudioPortTypeInput,
                                       "Master Input Port" );
        if ( fInputAudioPort == 0 )
            break;

        success = true;
    }
    while ( false );

    return success;
}

//---------------------------------------------------------------------------

bool CLASS::engageAudioEngines( void )
{
    DebugLog("%s::%s\n", getName(), __FUNCTION__);

    IOAC97Assert(fEnginePCMOut != 0);
    IOAC97Assert(fEnginePCMIn  != 0);

    if (activateAudioEngine( fEnginePCMOut ) != kIOReturnSuccess)
    {
        DebugLog("%s: activateAudioEngine PCMOut error\n", getName());
        return false;
    }
    else
    {
        attachAudioPort( fOutputAudioPort, fEnginePCMOut, 0 );
    }

    if (activateAudioEngine( fEnginePCMIn ) != kIOReturnSuccess)
    {
        DebugLog("%s: activateAudioEngine PCMIn error\n", getName());
        fEnginePCMIn->release();
        fEnginePCMIn = 0;
    }
    else
    {
        attachAudioPort( fInputAudioPort, 0, fEnginePCMIn );
    }

    if (fEngineSPDIF)
    {
        if (activateAudioEngine( fEngineSPDIF ) != kIOReturnSuccess)
        {
            DebugLog("%s: activateAudioEngine SPDIF error\n", getName());
            fEngineSPDIF->release();
            fEngineSPDIF = 0;
        }
        else
        {
            attachAudioPort( fOutputAudioPort, fEngineSPDIF, 0 );
        }
    }

    return true;
}

#pragma mark -
#pragma mark еее Power Management еее
#pragma mark -

//---------------------------------------------------------------------------

IOReturn CLASS::performPowerStateChange(
                       IOAudioDevicePowerState oldPowerState,
                       IOAudioDevicePowerState newPowerState,
                       UInt32 *    microsecondsUntilComplete )
{
    DebugLog("%s::%s old=%d new=%d\n", getName(), __FUNCTION__,
             oldPowerState, newPowerState);

    // This function is called by the IOAudioFamily on the work loop context.
    // Since we may need a fair amount of time to switch between power states
    // and also allowing time for analog section to stabilize, issue the work
    // asynchronously. Then other drivers can make progress when we block.
    // The family also blocks overlapping setPowerState() calls.

    // Claim async power change completion in 15 seconds or less.

    *microsecondsUntilComplete = 15 * 1000 * 1000;

    // Retain the target of the thread call while in flight.
    // The thread call handler must:
    //
    // 1. Drop the extra retain on the driver
    // 2. Call completePowerStateChange()

    retain();
    if ( thread_call_enter( fPowerChangeThreadCall ) == TRUE )
    {
        release();  // thread entry replaced
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void CLASS::powerChangeThreadHandler( thread_call_param_t param0,
                                      thread_call_param_t param1 )
{
    IOWorkLoop * workLoop;
    CLASS *      driver = (CLASS *) param0;

    // Called on a thread callout to handle audio driver power changes.
    // Synchronize execution with the driver's work loop.

    if (( workLoop = driver->getWorkLoop() ))
    {
        workLoop->runAction( powerChangeThreadAction, driver );
    }

    // Drop the retain held while the thread call was in flight.

    driver->completePowerStateChange();
    driver->release();
}

//---------------------------------------------------------------------------

IOReturn CLASS::powerChangeThreadAction( OSObject * target,
                                         void * arg0, void * arg1,
                                         void * arg2, void * arg3 )
{
    CLASS * driver = (CLASS *) target;
    int     oldPowerState = (int) driver->getPowerState();
    int     newPowerState = (int) driver->getPendingPowerState();

    DebugLog("%s::%s old=%d new=%d\n", driver->getName(), __FUNCTION__,
             oldPowerState, newPowerState);

    // Change power states, one state at a time, until the new power
    // state is achieved.

    if ( newPowerState > oldPowerState )
    {
        while ( oldPowerState++ != newPowerState )
        {
            driver->raisePowerState( oldPowerState );
        }
    }
    else if ( newPowerState < oldPowerState )
    {
        while ( oldPowerState-- != newPowerState )
        {
            driver->lowerPowerState( oldPowerState );
        }
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

void CLASS::lowerPowerState( int newPowerState )
{
    DebugLog("%s::%s (%d)\n", getName(), __FUNCTION__, newPowerState);

    switch ( newPowerState )
    {
        case kIOAudioDeviceIdle:

            // No audio engines are running. Shut down codec subsystems
            // but must be able to resume full operation with relatively
            // low latency.

            fAudioCodec->lowerPowerState(IOAC97AudioCodec::kPowerStateIdle);
            break;

        case kIOAudioDeviceSleep:

            // All audio engines should be stopped. But we also want to
            // deactivate all active audio configurations before sleep.
            
            if (fEnginePCMOut)
                fEnginePCMOut->handlePowerStateChange(
                               AppleAC97AudioEngine::kPowerStateSleep);

            if (fEnginePCMIn)
                fEnginePCMIn->handlePowerStateChange(
                              AppleAC97AudioEngine::kPowerStateSleep);

            if (fEngineSPDIF)
                fEngineSPDIF->handlePowerStateChange(
                              AppleAC97AudioEngine::kPowerStateSleep);

            // Shutdown all codec subsystems. A warm/cold AC-link reset
            // is required before the codec can be used again.

            fAudioCodec->lowerPowerState(IOAC97AudioCodec::kPowerStateSleep);
            break;

        default:
            IOAC97Assert(false);
    }
}

//---------------------------------------------------------------------------

void CLASS::raisePowerState( int newPowerState )
{
    DebugLog("%s::%s (%d)\n", getName(), __FUNCTION__, newPowerState);

    switch ( newPowerState )
    {
        case kIOAudioDeviceIdle:

            // Bring up the codec to idle state.

            fAudioCodec->raisePowerState(IOAC97AudioCodec::kPowerStateIdle);

            // Resumed from system doze or sleep. All codec context may be
            // lost. To restore context, activate all audio configurations.

            if (fEnginePCMOut)
                fEnginePCMOut->handlePowerStateChange(
                               AppleAC97AudioEngine::kPowerStateActive);

            if (fEnginePCMIn)
                fEnginePCMIn->handlePowerStateChange(
                              AppleAC97AudioEngine::kPowerStateActive);

            if (fEngineSPDIF)
                fEngineSPDIF->handlePowerStateChange(
                              AppleAC97AudioEngine::kPowerStateActive);

            flushAudioControls();

            break;

        case kIOAudioDeviceActive:
            fAudioCodec->raisePowerState(IOAC97AudioCodec::kPowerStateActive);
            break;

        default:
            IOAC97Assert(false);
    }
}

//---------------------------------------------------------------------------

IOReturn CLASS::acknowledgePowerChange(
                                               IOService * whichDriver )
{
    DebugLog("%s::%s [ %d->%d ]\n", getName(), __FUNCTION__,
             getPowerState(), getPendingPowerState());

    // This works around unnecessary PM acking by IOAudioFamily.
    // Basically kIOAudioDeviceIdle power state is not known to
    // power management, and IOAudioFamily should not ack. Drop
    // transitions between idle and active states.  Only called
    // from protectedCompletePowerStateChange(), no lock needed.

    if ( getPendingPowerState() != kIOAudioDeviceSleep &&
         getPowerState()        != kIOAudioDeviceSleep )
    {
        DebugLog("%s: ** Dropped ack for non-sleep state transition **\n",
                 getName());
        return kIOReturnSuccess;
    }

    // Workaround for 3421143.
    // Why is IOAudioFamily calling acknowledgePowerChange() to ack
    // a setPowerState() call? Change to acknowledgeSetPowerState().
    // This workaround breaks if we have any power child.

    return acknowledgeSetPowerState();
}

IOReturn CLASS::protectedSetPowerState(
                       unsigned long powerStateOrdinal, IOService * device )
{
    IOReturn ret = super::protectedSetPowerState(powerStateOrdinal, device);

    DebugLog("%s::%s state = %ld, ret = %lx\n",
             getName(), __FUNCTION__, powerStateOrdinal, ret);

    return ret;
}
