/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 1998, 1999, 2000AppleBurgundyAudio Apple Computer, Inc.  All rights reserved.
 *
 * Interface definition for the Burgundy audio Controller
 *
 * HISTORY
 *
 */

#ifndef _APPLEBURGUNDYAUDIO_H
#define _APPLEBURGUNDYAUDIO_H

#include "AudioHardwareCommon.h"
#include "AppleOnboardAudio.h"
#include "AudioHardwareConstants.h"
#include "AudioDeviceTreeParser.h"
#include "AudioHardwarePower.h"

class IOAudioControl;

struct IODBDMAChannelRegisters;
struct IODBDMADescriptor;

class IOAudioLevelControl;
class AudioDeviceTreeParser;

class AppleBurgundyAudio : public AppleOnboardAudio
{	
    friend class AudioHardwareOutput;
    friend class AudioHardwareInput;
    friend class AudioHardwareMux;
    OSDeclareDefaultStructors(AppleBurgundyAudio);


protected:
    // This is the base of the burgundy registers:
    UInt8                              *ioBaseBurgundy;

    // Register Mirrors:
    UInt32               		soundControlRegister;
    UInt32                		CodecControlRegister[8];
    UInt32				currentOutputMuteReg;
    UInt32               		lastStatusRegister;

    UInt8				mirrorVGAReg[4];
    int    localSettlingTime[5];

    bool mIsMute;
    bool mVolumeMuteIsActive;
    UInt32 curInsense;
    UInt32 mVolRight, mVolLeft;
    UInt32 mMuxMix;
    UInt32 mLogicalInput; //keep track of the latest input
    bool  duringInitialization;
    bool	gCanPollSatus;

public:
        //Classical Unix function
    virtual bool init(OSDictionary *properties);
    virtual void free();
    virtual IOService* probe(IOService *provider, SInt32*);

        //IOAudioDevice subclass
    virtual bool initHardware(IOService *provider);
        
protected:    
    virtual void checkStatus(bool force);
    static void timerCallback(OSObject *target, IOAudioDevice *device);
    void setDeviceDetectionActive();
    void setDeviceDetectionInActive();      

    void 	sndHWInitialize(IOService *provider);
    UInt32 	sndHWGetInSenseBits(void);
    UInt32 	sndHWGetRegister(UInt32 regNum);
    IOReturn   	sndHWSetRegister(UInt32 regNum, UInt32 value);

public:
    UInt32	sndHWGetConnectedDevices(void);
	virtual IOReturn setModemSound(bool state);
protected:    

            //activation functions
    UInt32	sndHWGetActiveOutputExclusive(void);
    IOReturn   	sndHWSetActiveOutputExclusive(UInt32 outputPort );
    UInt32 	sndHWGetActiveInputExclusive(void);
    IOReturn   	sndHWSetActiveInputExclusive(UInt32 input );
    UInt32 	sndHWGetProgOutput();    
    IOReturn   	sndHWSetProgOutput(UInt32 outputBits);
    
    IOReturn sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain);
            // control function
    bool   	sndHWGetSystemMute(void);
    IOReturn  	sndHWSetSystemMute(bool mutestate);
    bool   	sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume);
    IOReturn   	sndHWSetSystemVolume(UInt32 value);
    IOReturn	sndHWSetPlayThrough(bool playthroughstate);
    
            //Power Management
    IOReturn   	sndHWSetPowerState(IOAudioDevicePowerState theState);

            //Identification
    UInt32 	sndHWGetType( void );
    UInt32	sndHWGetManufacturer( void );
    
        //Burgundy soecific routine
    void 	DisconnectMixer(UInt32 mixer );
    UInt32	GetPhysicalOutputPort(UInt32 logicalPort );
    UInt32 	GetPhysicalInputPort(UInt32 logicalPort );
    UInt32 	GetInputPortType(UInt32 inputPhysicalPort);
    UInt8	GetInputMux(UInt32 physicalInput);
    void	ReleaseMux(UInt8 mux);
    void	ReserveMux(UInt8 mux, UInt32 physicalInput);
};

#endif /* !_APPLEBURGUNDYAUDIO_H */
