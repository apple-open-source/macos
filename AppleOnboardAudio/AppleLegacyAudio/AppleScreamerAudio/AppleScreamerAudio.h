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
#include "Apple02Audio.h"
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

struct NoisyStateRec {
	UInt32					gainL;
	UInt32					gainR;
	UInt32					attenA;
	UInt32					attenC;
	Boolean					validRecord;
};
typedef struct NoisyStateRec NoisyStateRec;

class IOAudioLevelControl;
class AudioDeviceTreeParser;

class AppleScreamerAudio : public Apple02Audio
{
    friend class AudioHardwareOutput;
    friend class AudioHardwareInput;
    friend class AudioHardwareMux;
    OSDeclareDefaultStructors(AppleScreamerAudio);

protected:
	// Registers
    volatile awacs_regmap_t *		ioBase;
	UInt32							awacsRegs[kMaxSndHWRegisters];	// Shadow awacs registers
    UInt32							soundControlRegister;
    UInt32							codecControlRegister[8];
    UInt32							codecStatus;
	IOAudioDevicePowerState			powerState;
	Boolean							recalibrateNecessary;
	Boolean							deviceIntsEnabled;
	Boolean							leftMute;
	Boolean							rightMute;

    bool							mVolMuteActive;
    bool							gCanPollStatus;
	volatile void					*soundConfigSpace;				// address of sound config space
	
	// information specific to the chip
    AwacsInformation				chipInformation;
	IOService						*ourProvider;					//	[3042658]	rbm	30 Sept 2002
	SInt32							minVolume;						//	[3042658]	rbm	30 Sept 2002
	SInt32							maxVolume;						//	[3042658]	rbm	30 Sept 2002
	Boolean							useMasterVolumeControl;			//	[3042658]	rbm	30 Sept 2002
	UInt32							lastLeftVol;					//	[3042658]	rbm	30 Sept 2002
	UInt32							lastRightVol;					//	[3042658]	rbm	30 Sept 2002
	UInt32							layoutID;						//	[3042658]	rbm	30 Sept 2002
	UInt32							gPowerState;
      
public:
	// Classical Unix funxtions
    virtual bool init(OSDictionary *properties);
    virtual void free();

    virtual IOService* probe(IOService *provider, SInt32*);

	// IOAudioDevice subclass
    virtual bool initHardware(IOService *provider);
        
	// PRAM volume - need to move to common class
    UInt8 		VolumeToPRAMValue( UInt32 volLeft ,  UInt32 volRight);
    // void WritePRAMVol(  UInt32 volLeft ,  UInt32 volRight);

protected:

	// These will probably change when we have a general method
	// to verify the Detects.
    virtual void checkStatus(bool force);
    static void timerCallback(OSObject *target, IOAudioDevice *device);
    void 		setDeviceDetectionActive();
    void 		setDeviceDetectionInActive();     
	  
	// These should be virtual method in a superclass. All "Get" method
	// could be common.
		
	// hardware registers manipulationf
    void 			sndHWInitialize(IOService *provider);
	virtual void	sndHWPostDMAEngineInit (IOService *provider);
	virtual void	sndHWPostThreadedInit (IOService *provider); // [3284411]

    UInt32 		sndHWGetInSenseBits(void);
    UInt32 		sndHWGetRegister(UInt32 regNum);
    IOReturn   	sndHWSetRegister(UInt32 regNum, UInt32 value);
public:
    UInt32		sndHWGetConnectedDevices(void);
protected:    

	// activation functions
    UInt32		sndHWGetActiveOutputExclusive(void);
    IOReturn   	sndHWSetActiveOutputExclusive(UInt32 outputPort );
    UInt32 		sndHWGetActiveInputExclusive(void);
    IOReturn   	sndHWSetActiveInputExclusive(UInt32 input );
	IOReturn	AdjustControls (void);					//	[3042658]	rbm	30 Sept 2002
    UInt32 		sndHWGetProgOutput();    
    IOReturn   	sndHWSetProgOutput(UInt32 outputBits);
	virtual UInt32		sndHWGetCurrentSampleFrame (void);
	virtual void		sndHWSetCurrentSampleFrame (UInt32 value);

	// control function
    bool   		sndHWGetSystemMute(void);
    IOReturn  	sndHWSetSystemMute(bool mutestate);
    bool   		sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume);
    IOReturn   	sndHWSetSystemVolume(UInt32 value);
    IOReturn	sndHWSetPlayThrough(bool playthroughstate);
    IOReturn	sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain);
    
	// Power Management
    IOReturn			sndHWSetPowerState(IOAudioDevicePowerState theState);
	virtual IOReturn	performDeviceWake ();
	virtual IOReturn	performDeviceSleep ();
	virtual IOReturn	performDeviceIdleSleep ();
	IOReturn			setCodecPowerState ( IOAudioDevicePowerState theState );

	// Identification
    UInt32 		sndHWGetType( void );
    UInt32		sndHWGetManufacturer( void );
    
	// chip specific
    void GC_Recalibrate(void);
    void restoreSndHWRegisters(void);
    void setAWACsPowerState( IOAudioDevicePowerState state );
    void setScreamerPowerState(IOAudioDevicePowerState state);
    void InitializeShadowRegisters(void);
	
	void GoRunState( IOAudioDevicePowerState curState );
	void GoDozeState( IOAudioDevicePowerState curState );
	void GoIdleState( IOAudioDevicePowerState curState );
	void GoSleepState( IOAudioDevicePowerState curState );

	IOAudioDevicePowerState SndHWGetPowerState( void );
	void SetStateBits( UInt32 stateBits, UInt32 delay );
	UInt32				GetDeviceID (void);					//	[3042658]	rbm	30 Sept 2002

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

#endif /* _APPLESCREAMERAUDIO_H */
