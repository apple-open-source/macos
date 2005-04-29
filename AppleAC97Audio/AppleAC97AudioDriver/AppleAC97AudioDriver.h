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

#ifndef __APPLEAC97AUDIODRIVER_H
#define __APPLEAC97AUDIODRIVER_H

#include <IOKit/audio/IOAudioDevice.h>
#include "IOAC97AudioCodec.h"
#include "IOAC97Controller.h"
#include "AppleAC97AudioEngine.h"

class AppleAC97AudioDriver : public IOAudioDevice
{
    OSDeclareDefaultStructors( AppleAC97AudioDriver )

protected:
    IOAC97AudioCodec *      fAudioCodec;
    IOAC97Controller *      fAudioRoot;
    AppleAC97AudioEngine *  fEnginePCMOut;
    AppleAC97AudioEngine *  fEnginePCMIn;
    AppleAC97AudioEngine *  fEngineSPDIF;
    IOAudioPort *           fInputAudioPort;
    IOAudioPort *           fOutputAudioPort;
    thread_call_t           fPowerChangeThreadCall;

    static void         powerChangeThreadHandler(
                                 thread_call_param_t param0,
                                 thread_call_param_t param1 );

    static IOReturn     powerChangeThreadAction(
                                 OSObject * target,
                                 void * arg0, void * arg1,
                                 void * arg2, void * arg3 );

    virtual bool        createAudioEngines( void );
    virtual bool        engageAudioEngines( void );
    virtual bool        createAudioPorts( void );

    virtual void        lowerPowerState( int newPowerState );
    virtual void        raisePowerState( int newPowerState );

    virtual void        free();

public:
    virtual IOService * probe( IOService * provider, SInt32 * score );

    virtual bool        initHardware(
                                 IOService * provider );

    virtual IOReturn    message( UInt32      type,
                                 IOService * provider,
                                 void *      arg );

    virtual IOReturn    performPowerStateChange(
                                 IOAudioDevicePowerState oldPowerState,
                                 IOAudioDevicePowerState newPowerState,
                                 UInt32 *    microsecondsUntilComplete );

    virtual IOReturn    acknowledgePowerChange(
                                 IOService * whichDriver );

    virtual IOReturn    protectedSetPowerState(
                                 UInt32 powerStateOrdinal,
                                 IOService * device );
};

#endif /* !__APPLEAC97AUDIODRIVER_H */
