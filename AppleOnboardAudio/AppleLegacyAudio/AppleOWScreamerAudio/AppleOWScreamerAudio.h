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
/*
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * Interface definition for the Awacs audio Controller
 *
 * HISTORY
 *
 */

#ifndef _APPLEOWSCREAMERAUDIO_H
#define _APPLEOWSCREAMERAUDIO_H

#include <IOKit/audio/IOAudioDevice.h>

class IOAudioControl;

struct awacsOW_regmap_t;
struct IODBDMAChannelRegisters;
struct IODBDMADescriptor;

class IOAudioLevelControl;

// temp values for volume until rolled into uber class
enum
{
    kMaximumVolume 	= 32,
    kMinimumVolume 	= 0,
    kInitialVolume	= 32
};

enum {
     kOutMute = 0,
    kOutVolLeft = 1,
    kOutVolRight = 2,
    kPassThruToggle = 3,
    kInGainLeft = 4,
    kInGainRight = 5,
	kHeadphoneInsert = 6,
	kInputInsert = 7,
    kNumControls
};

class AppleOWScreamerAudio : public IOAudioDevice
{
    OSDeclareDefaultStructors(AppleOWScreamerAudio);

protected:
    volatile awacsOW_regmap_t *	ioBase;
    UInt32			soundControlRegister;
    UInt32			codecControlRegister[8];
    UInt32			codecStatus;

    bool			iicAudioDevicePresent;

    bool			updateStatus;

	// this is the basic set of ports
	IOAudioToggleControl *		outMute;
	IOAudioToggleControl *		playthruToggle;
	IOAudioToggleControl *		headphoneConnection;
	IOAudioToggleControl *		inputConnection;
	IOAudioLevelControl *		outVolLeft;
	IOAudioLevelControl *		outVolRight;
	IOAudioLevelControl *		inGainLeft;
	IOAudioLevelControl *		inGainRight;
	IOAudioSelectorControl *	outputSelector;			// This is a read only selector

    UInt32 			gVolLeft, gVolRight;
    UInt32			numDetects;
    UInt32			numOutputs;
    UInt32			awacsVersion;
    
    short 	curActiveSpkr;
    bool			duringInitialization;
    
public:
    virtual bool init(OSDictionary *properties);
    virtual void free();

    virtual void retain() const;
    virtual void release() const;
    
    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);
    virtual IOService* probe(IOService *provider, SInt32* score);

    virtual void initHardware();
    virtual void recalibrate();

    virtual IOReturn performPowerStateChange(IOAudioDevicePowerState oldPowerState,
                                                IOAudioDevicePowerState newPowerState,
                                                UInt32 *microsecondsUntilComplete);

    virtual IOReturn performDeviceWake();
    virtual IOReturn performDeviceSleep();
    
    static IOReturn volumeLeftChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
    virtual IOReturn volumeLeftChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
    
    static IOReturn volumeRightChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
    virtual IOReturn volumeRightChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
    
    static IOReturn outputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue);
    virtual IOReturn outputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue);

    static IOReturn gainLeftChangeHandler(IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue);
    virtual IOReturn gainLeftChanged(IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue);

    static IOReturn gainRightChangeHandler(IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue);
    virtual IOReturn gainRightChanged(IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue);
    
    static IOReturn passThruChangeHandler(IOService *target, IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue);
    virtual IOReturn passThruChanged(IOAudioControl *passThruControl, SInt32 oldValue, SInt32 newValue);


    IOReturn setModemSound(bool state);
    virtual IOReturn callPlatformFunction( const OSSymbol * functionName,bool waitForFunction,
            void *param1, void *param2, void *param3, void *param4 );

    IOReturn	setToneHardwareMuteRear(bool mute);		// How to mute/unmute the rear ext spkr jack
    IOReturn	setToneHardwareMuteFront(bool mute);		// How to mute/unmute the rest of the accoustic outputs
    IOReturn	setToneHardwareMuteBoomer(bool mute);		
    
    IOReturn	setToneHardwareVolume(UInt32 volLeft, UInt32 volRight);
    IOReturn	setToneHardwareBalance(UInt32 volLeft, UInt32 volRight);

    UInt8 VolumeToPRAMValue( UInt32 volLeft ,  UInt32 volRight);
    void WritePRAMVol(  UInt32 volLeft ,  UInt32 volRight);

protected:
    virtual bool createPorts(IOAudioEngine *driverDMAEngine);

    virtual void checkStatus(bool force);
    static void timerCallback(OSObject *target, IOAudioDevice *device);
    
    IOReturn writesgs7433(UInt8 RegIndex, unsigned char RegValue );
    IOReturn readsgs7433( UInt8 RegIndex, unsigned char *RegValue);
    
    UInt32  	sndHWGetProgOutput(void );
    IOReturn   	sndHWSetProgOutput(UInt32 outputBits);

    UInt32	sndHWGetActiveInputExclusive(void);
    IOReturn 	sndHWSetActiveInputExclusive(UInt32 input );
    
    UInt32 	sndHWGetRegister(UInt32 regNum);
    IOReturn 	sndHWSetRegister(UInt32 regNum, UInt32 value);
    
};

#endif /* _APPLEOWSCREAMERAUDIO_H */
