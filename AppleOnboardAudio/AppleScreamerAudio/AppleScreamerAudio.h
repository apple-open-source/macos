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
/*
 *
 * Interface definition for the Awacs audio Controller
 *
 * HISTORY
 *
 */

#ifndef _APPLESCREAMERAUDIO_H
#define _APPLESCREAMERAUDIO_H

#include "AudioHardwareCommon.h"
#include "AppleOnboardAudio.h"
#include "AudioHardwareConstants.h"
#include "AudioDeviceTreeParser.h"
#include "AudioHardwarePower.h"

class IOAudioControl;

struct awacs_regmap_t;
struct IODBDMAChannelRegisters;
struct IODBDMADescriptor;

/*-----------------------------AWACs Special information --------------------------------*/
typedef struct _AwacsInformation {
    
    UInt32	preRecalDelay;
    UInt32	rampDelay; 
    UInt32	awacsVersion;
    UInt32 	partType;	

    bool outputAActive;
    bool outputCActive;
    bool recalibrateNecessary;
} AwacsInformation;



class IOAudioLevelControl;
class AudioDeviceTreeParser;

class AppleScreamerAudio : public AppleOnboardAudio
{
    friend class AudioHardwareOutput;
    friend class AudioHardwareInput;
    friend class AudioHardwareMux;
    OSDeclareDefaultStructors(AppleScreamerAudio);

protected:
        //Registers
    volatile awacs_regmap_t *	ioBase;
    UInt32			soundControlRegister;
    UInt32			codecControlRegister[8];
    UInt32			codecStatus;

    bool	mVolMuteActive;
    bool	gCanPollSatus;
    
        //information specific to the chip
    AwacsInformation		chipInformation;
    bool mIsMute;
    UInt32 mVolRight, mVolLeft;
        //PM info
 //   bool			wakingFromSleep;
    bool			duringInitialization;
    
      
public:
            //Classical Unix funxtions
    virtual bool init(OSDictionary *properties);
    virtual void free();

    virtual IOService* probe(IOService *provider, SInt32*);

            //IOAudioDevice subclass
    virtual bool initHardware(IOService *provider);
        
                    //PRAM volume - need to move to common class
    UInt8 VolumeToPRAMValue( UInt32 volLeft ,  UInt32 volRight);
    //void WritePRAMVol(  UInt32 volLeft ,  UInt32 volRight);

protected:

            //These will probably change when we have a general method
            //to verify the Detects.
    virtual void checkStatus(bool force);
    static void timerCallback(OSObject *target, IOAudioDevice *device);
    void setDeviceDetectionActive();
    void setDeviceDetectionInActive();       
           // These should be virtual method in a superclass. All "Get" method
           // could be common.
                
            //hardware registers manipulationf
    void 	sndHWInitialize(IOService *provider);
    UInt32 	sndHWGetInSenseBits(void);
    UInt32 	sndHWGetRegister(UInt32 regNum);
    IOReturn   	sndHWSetRegister(UInt32 regNum, UInt32 value);
public:
    UInt32	sndHWGetConnectedDevices(void);
protected:    

            //activation functions
    UInt32	sndHWGetActiveOutputExclusive(void);
    IOReturn   	sndHWSetActiveOutputExclusive(UInt32 outputPort );
    UInt32 	sndHWGetActiveInputExclusive(void);
    IOReturn   	sndHWSetActiveInputExclusive(UInt32 input );
    UInt32 	sndHWGetProgOutput();    
    IOReturn   	sndHWSetProgOutput(UInt32 outputBits);
    
            // control function
    bool   	sndHWGetSystemMute(void);
    IOReturn  	sndHWSetSystemMute(bool mutestate);
    bool   	sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume);
    IOReturn   	sndHWSetSystemVolume(UInt32 value);
    IOReturn	sndHWSetPlayThrough(bool playthroughstate);
    IOReturn sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain);
    
            //Power Management
    IOReturn   	sndHWSetPowerState(IOAudioDevicePowerState theState);

            //Identification
    UInt32 	sndHWGetType( void );
    UInt32	sndHWGetManufacturer( void );
    
            //chip specific
    void GC_Recalibrate(void);
    void restoreSndHWRegisters(void);
    void setAWACsPowerState( IOAudioDevicePowerState state );
    void setScreamerPowerState(IOAudioDevicePowerState state);
    void InitializeShadowRegisters(void);
};

#endif /* _APPLESCREAMERAUDIO_H */
