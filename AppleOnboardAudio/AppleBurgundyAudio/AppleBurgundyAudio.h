/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
    UInt32						currentOutputMuteReg;
    UInt32               		lastStatusRegister;

    UInt8						mirrorVGAReg[4];
    int    						localSettlingTime[5];

    bool 						mIsMute;
	Boolean						mModemActive;
	Boolean						mInternalSpeakerActive;
	Boolean						mExternalSpeakerActive;
	Boolean						mHeadphonesActive;
	Boolean						mMonoSpeakerActive;
    bool 						mVolumeMuteIsActive;
    UInt32 						curInsense;
    UInt32 						mVolRight;
	UInt32						mVolLeft;
    UInt32 						mMuxMix;
    UInt32 						mLogicalInput; // keep track of the latest input
    bool  						duringInitialization;
    bool						gCanPollSatus;

public:
	// Classical Unix function
    virtual bool init(OSDictionary *properties);
    virtual void free();
    virtual IOService* probe(IOService *provider, SInt32*);

	// IOAudioDevice subclass
    virtual bool	initHardware(IOService *provider);
        
protected:    
    virtual void	checkStatus(bool force);
    static void		timerCallback(OSObject *target, IOAudioDevice *device);
    void			setDeviceDetectionActive();
    void			setDeviceDetectionInActive();      

    void 			sndHWInitialize(IOService *provider);
	virtual void	sndHWPostDMAEngineInit (IOService *provider);

    UInt32 			sndHWGetInSenseBits(void);
    UInt32 			sndHWGetRegister(UInt32 regNum);
    IOReturn   		sndHWSetRegister(UInt32 regNum, UInt32 value);

public:
    UInt32				sndHWGetConnectedDevices(void);
	virtual IOReturn	setModemSound(bool state);
protected:    

	// activation functions
    UInt32		sndHWGetActiveOutputExclusive(void);
    IOReturn   	sndHWSetActiveOutputExclusive(UInt32 outputPort );
    UInt32 		sndHWGetActiveInputExclusive(void);
    IOReturn   	sndHWSetActiveInputExclusive(UInt32 input );
    UInt32 		sndHWGetProgOutput();    
    IOReturn   	sndHWSetProgOutput(UInt32 outputBits);
    
    IOReturn sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain);
	// control function
    bool   		sndHWGetSystemMute(void);
    IOReturn  	sndHWSetSystemMute(bool mutestate);
    bool   		sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume);
    IOReturn   	sndHWSetSystemVolume(UInt32 value);
    IOReturn	sndHWSetPlayThrough(bool playthroughstate);
    
	// Power Management
    IOReturn   			sndHWSetPowerState(IOAudioDevicePowerState theState);
	virtual IOReturn	performDeviceWake ();
	virtual IOReturn	performDeviceSleep ();
	virtual IOReturn	performDeviceIdleSleep ();

	// Identification
    UInt32 	sndHWGetType( void );
    UInt32	sndHWGetManufacturer( void );
    
	// Burgundy soecific routine
    void 	DisconnectMixer(UInt32 mixer );
    UInt32	GetPhysicalOutputPort(UInt32 logicalPort );
    UInt32 	GetPhysicalInputPort(UInt32 logicalPort );
    UInt32 	GetInputPortType(UInt32 inputPhysicalPort);
    UInt8	GetInputMux(UInt32 physicalInput);
    void	ReleaseMux(UInt8 mux);
    void	ReserveMux(UInt8 mux, UInt32 physicalInput);

	// User Client calls
	virtual UInt8		readGPIO (UInt32 selector) {return 0;}
	virtual void		writeGPIO (UInt32 selector, UInt8 data) {return;}
	virtual Boolean		getGPIOActiveState (UInt32 gpioSelector) {return 0;}
	virtual void		setGPIOActiveState ( UInt32 selector, UInt8 gpioActiveState ) {return;}
	virtual Boolean		checkGpioAvailable ( UInt32 selector ) {return 0;}
	virtual IOReturn	readHWReg32 ( UInt32 selector, UInt32 * registerData ) {return kIOReturnUnsupported;}
	virtual IOReturn	writeHWReg32 ( UInt32 selector, UInt32 registerData ) {return kIOReturnUnsupported;}
	virtual IOReturn	readCodecReg ( UInt32 selector, void * registerData,  UInt32 * registerDataSize ) {return kIOReturnUnsupported;}
	virtual IOReturn	writeCodecReg ( UInt32 selector, void * registerData ) {return kIOReturnUnsupported;}
	virtual IOReturn	readSpkrID ( UInt32 selector, UInt32 * speakerIDPtr );
	virtual IOReturn	getCodecRegSize ( UInt32 selector, UInt32 * codecRegSizePtr ) {return kIOReturnUnsupported;}
	virtual	IOReturn	getVolumePRAM ( UInt32 * pramDataPtr ) {return kIOReturnUnsupported;}
	virtual IOReturn	getDmaState ( UInt32 * dmaStatePtr ) {return kIOReturnUnsupported;}
	virtual IOReturn	getStreamFormat ( IOAudioStreamFormat * streamFormatPtr ) {return kIOReturnUnsupported;}
	virtual IOReturn	readPowerState ( UInt32 selector, IOAudioDevicePowerState * powerState ) {return kIOReturnUnsupported;}
	virtual IOReturn	setPowerState ( UInt32 selector, IOAudioDevicePowerState powerState ) {return kIOReturnUnsupported;}
	virtual IOReturn	setBiquadCoefficients ( UInt32 selector, void * biquadCoefficients, UInt32 coefficientSize ) {return kIOReturnUnsupported;}
	virtual IOReturn	getBiquadInformation ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr ) {return kIOReturnUnsupported;}
	virtual IOReturn	getProcessingParameters ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr )  {return kIOReturnUnsupported;}
	virtual IOReturn	setProcessingParameters ( UInt32 scalarArg1, void * inStructPtr, UInt32 inStructSize ) {return kIOReturnUnsupported;}
	virtual	IOReturn	invokeInternalFunction ( UInt32 functionSelector, void * inData ) { return kIOReturnUnsupported; }

};

#endif /* !_APPLEBURGUNDYAUDIO_H */
