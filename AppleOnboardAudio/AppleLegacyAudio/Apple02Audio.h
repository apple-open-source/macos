/*
 *  Apple02Audio.h
 *  Apple02Audio
 *
 *  Created by cerveau on Mon Jun 04 2001.
 *  Copyright (c) 2001 Apple Computer Inc. All rights reserved.
 *
 */

#ifndef __APPLEONBOARDAUDIO__
#define __APPLEONBOARDAUDIO__

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOUserClient.h>

#include "AudioHardwareCommon.h"
#include "AudioHardwareConstants.h"
#include "AudioDeviceTreeParser.h"
#include "AudioHardwareOutput.h"
#include "AudioHardwareInput.h"
#include "AudioHardwarePower.h"
#include "Apple02DBDMAAudioDMAEngine.h"

enum {
    kOutMute			= 0,
    kOutVolLeft			= 1,
    kOutVolRight		= 2,
    kPassThruToggle		= 3,
    kInGainLeft			= 4,
    kInGainRight		= 5,
    kInputSelector		= 6,
	kOutVolMaster		= 7,
	kPRAMVol			= 8,
	kHeadphoneInsert	= 9,
	kInputInsert		= 10,
    kNumControls
};

enum invokeInternalFunctionSelectors {
	kInvokeHeadphoneInterruptHandler,
	kInvokeSpeakerInterruptHandler,
	kInvokeResetiSubCrossover,
	kInvokePerformDeviceSleep,
	kInvokePerformDeviceIdleSleep,
	kInvokePerformDeviceWake
};
		
#define kBatteryPowerDownDelayTime		30000000000ULL				/* 30 seconds					*/
#define kACPowerDownDelayTime			300000000000ULL				/* 300 seconds == 5 minutes		*/
#define kiSubMaxVolume					60
#define kiSubVolumePercent				92

typedef struct {
	UInt32			layoutID;			//	identify the target CPU
	UInt32			portID;				//	identify port (see: AudioPortTypes in AudioHardwareConstants.h)
	UInt32			speakerID;			//	dallas ROM ID (concatenates all fields)
} SpeakerIDStruct;
typedef SpeakerIDStruct * SpeakerIDStructPtr;

class IOAudioControl;

class Apple02Audio : public IOAudioDevice
{
    friend class AudioHardwareOutput;
    friend class AudioHardwareInput;
    friend class AudioHardwareDetect;
    friend class AudioHardwareMux;
    friend class AudioPowerObject;
	friend class AOAUserClient;
    
    OSDeclareDefaultStructors(Apple02Audio);

protected:
	// general controls : these are the default controls attached to a DMA audio engine
    IOAudioToggleControl *		outMute;
    IOAudioToggleControl *		playthruToggle;
	IOAudioToggleControl *		headphoneConnection;
	IOAudioToggleControl *		inputConnection;
	IOAudioLevelControl *		pramVol;
	IOAudioLevelControl *		outVolMaster;
    IOAudioLevelControl *		outVolLeft;
    IOAudioLevelControl *		outVolRight;
    IOAudioLevelControl *		inGainLeft;
    IOAudioLevelControl *		inGainRight;
    IOAudioSelectorControl *	inputSelector;
	IOAudioSelectorControl *	outputSelector;			// This is a read only selector
	thread_call_t				mPowerThread;
	thread_call_t				mInitHardwareThread;

	// globals for the driver
	unsigned long long			idleSleepDelayTime;
	IOTimerEventSource *		idleTimer;
    bool						gIsMute;					// global mute (that is on all the ports)
    bool						gIsPlayThroughActive;		// playthrough mode is on
    SInt32      				gVolLeft;
    SInt32      				gVolRight;
    SInt32      				gGainLeft;
    SInt32      				gGainRight;
    bool 						gHasModemSound;
    bool 						gIsModemSoundActive;
    UInt32						gLastInputSourceBeforeModem;
    bool						gExpertMode;				// when off we are in a OS 9 like config. On we 
    UInt32						fMaxVolume;
    UInt32						fMinVolume;
	IOAudioDevicePowerState		ourPowerState;
	Boolean						shuttingDown;

	// we keep the engines around to have a cleaner initHardware
    Apple02DBDMAAudioDMAEngine *	driverDMAEngine;

	// Port Handler like info
    OSArray	*					AudioDetects;
    OSArray	*					AudioOutputs;
    OSArray	*					AudioInputs;
    OSArray	*					AudioSoftDSPFeatures;
    
	// Other objects
    AudioDeviceTreeParser *		theAudioDeviceTreeParser;
    AudioPowerObject *			theAudioPowerObject;
    
	// Dynamic variable that handle the connected devices
    sndHWDeviceSpec				currentDevices;
    bool 						fCPUNeedsPhaseInversion;	// true if this CPU's channels are out-of-phase
    bool 						mHasHardwareInputGain;		// aml 5.3.02
    IOFixed 					mDefaultInMinDB;			// aml 5.24.02, added for saving input control range
	IOFixed 					mDefaultInMaxDB;
	bool 						mRangeInChanged;	
	
	bool						mTerminating;
	DualMonoModeType			mInternalMicDualMonoMode;	// aml 6.17.02
	
	UInt32						mProcessingParams[kMaxProcessingParamSize/sizeof(UInt32)];
	bool						disableLoadingEQFromFile;

public:
	// Classical Unix funxtions
    virtual bool init(OSDictionary *properties);
    virtual void free();
    virtual IOService* probe(IOService *provider, SInt32*);
	virtual void stop (IOService *provider);

    bool     getMuteState();
    void     setMuteState(bool newMuteState);
	// Useful getter
    virtual OSArray *getDetectArray();
	// IOAudioDevice subclass
    virtual bool initHardware(IOService *provider);
	static void				initHardwareThread (Apple02Audio * aoa, void * provider);
	static IOReturn			initHardwareThreadAction (OSObject * owner, void * provider, void * arg2, void * arg3, void * arg4);
	virtual IOReturn		protectedInitHardware (IOService * provider);

    virtual IOReturn createDefaultsPorts();
    
    virtual IORegistryEntry * FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value);

    static IOReturn outputControlChangeHandler (IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
	static IOReturn inputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue);

	virtual IOReturn volumeMasterChange(SInt32 newValue);
    virtual IOReturn volumeLeftChange(SInt32 newValue);
    virtual IOReturn volumeRightChange(SInt32 newValue);
	virtual IOReturn outputMuteChange(SInt32 newValue);

    virtual IOReturn gainLeftChanged(SInt32 newValue);

    virtual IOReturn gainRightChanged(SInt32 newValue);
    
    virtual IOReturn passThruChanged(SInt32 newValue);

    virtual IOReturn inputSelectorChanged(SInt32 newValue);

	static void performPowerStateChangeThread (Apple02Audio * aoa, void * newPowerState);
	static IOReturn performPowerStateChangeThreadAction (OSObject * owner, void * arg1, void * arg2, void * arg3, void * arg4);
	virtual IOReturn performPowerStateChange (IOAudioDevicePowerState oldPowerState, IOAudioDevicePowerState newPowerState,
                                                                                            UInt32 *microsecondsUntilComplete);
	virtual void setTimerForSleep ();
	static void sleepHandlerTimer (OSObject *owner, IOTimerEventSource *sender);

    virtual IOReturn setModemSound(bool state);
    virtual IOReturn callPlatformFunction( const OSSymbol * functionName,bool waitForFunction,
            void *param1, void *param2, void *param3, void *param4 );
    
	virtual IOReturn	newUserClient( task_t 			inOwningTask,
							void *			inSecurityID,
							UInt32 			inType,
							IOUserClient **	outHandler );

protected:
	// Do the link to the IOAudioFamily 
	// These will help to create the port config through the OF Device Tree
            
    IOReturn configureDMAEngines(IOService *provider);
    IOReturn parseAndActivateInit(IOService *provider);
    IOReturn configureAudioDetects(IOService *provider);
    IOReturn configureAudioOutputs(IOService *provider);
    IOReturn configureAudioInputs(IOService *provider);
    IOReturn configurePowerObject(IOService *provider);

	static IOReturn sysPowerDownHandler (void * target, void * refCon, UInt32 messageType, IOService * provider, void * messageArgument, vm_size_t argSize);

    sndHWDeviceSpec getCurrentDevices();
    void setCurrentDevices(sndHWDeviceSpec devices);
    void changedDeviceHandler(UInt32 odevices);

	IOReturn setAggressiveness(unsigned long type, unsigned long newLevel);

public:
    virtual void setDeviceDetectionActive() = 0;
    virtual void setDeviceDetectionInActive() = 0;
protected:
    // The PRAM utility
	UInt32 PRAMToVolumeValue (void);
    UInt8 VolumeToPRAMValue (UInt32 leftVol, UInt32 rightVol);
    void WritePRAMVol (UInt32 volLeft, UInt32 volRight);
	UInt8 ReadPRAMVol (void);
    
	// Hardware specific functions : These are all virtual functions and we have to 
	// to the work here
    virtual void 		sndHWInitialize(IOService *provider) = 0;
	virtual void		sndHWPostDMAEngineInit (IOService *provider) = 0;
    virtual UInt32 		sndHWGetInSenseBits(void) = 0;
    virtual UInt32 		sndHWGetRegister(UInt32 regNum) = 0;
    virtual IOReturn   	sndHWSetRegister(UInt32 regNum, UInt32 value) = 0;

	virtual void		sndHWPostThreadedInit (IOService *provider) { return; } // [3284411]

public:
    virtual  IOReturn   sndHWSetPowerState(IOAudioDevicePowerState theState) = 0;
    virtual  UInt32		sndHWGetConnectedDevices(void) = 0;
    virtual  UInt32 	sndHWGetProgOutput() = 0;
    virtual  IOReturn   sndHWSetProgOutput(UInt32 outputBits) = 0;
	virtual	UInt32		sndHWGetCurrentSampleFrame (void) = 0;
	virtual void		sndHWSetCurrentSampleFrame (UInt32 value) = 0;

	// User Client calls residing in AOA derived object (accessed indirectly through public APIs)
	virtual UInt8		readGPIO (UInt32 selector) = 0;
	virtual void		writeGPIO (UInt32 selector, UInt8 data) = 0;
	virtual Boolean		getGPIOActiveState (UInt32 gpioSelector) = 0;
	virtual void		setGPIOActiveState ( UInt32 selector, UInt8 gpioActiveState ) = 0;
	virtual Boolean		checkGpioAvailable ( UInt32 selector ) {return 0;}
	virtual IOReturn	readHWReg32 ( UInt32 selector, UInt32 * registerData ) = 0;
	virtual IOReturn	writeHWReg32 ( UInt32 selector, UInt32 registerData ) = 0;
	virtual IOReturn	readCodecReg ( UInt32 selector, void * registerData,  UInt32 * registerDataSize ) = 0;
	virtual IOReturn	writeCodecReg ( UInt32 selector, void * registerData ) = 0;
	virtual IOReturn	readSpkrID ( UInt32 selector, UInt32 * speakerIDPtr ) = 0;
	virtual IOReturn	getCodecRegSize ( UInt32 selector, UInt32 * codecRegSizePtr ) = 0;
	virtual IOReturn	getVolumePRAM ( UInt32 * pramDataPtr ) = 0;
	virtual IOReturn	getDmaState ( UInt32 * dmaStatePtr ) = 0;
	virtual IOReturn	getStreamFormat ( IOAudioStreamFormat * streamFormatPtr ) = 0;
	virtual IOReturn	readPowerState ( UInt32 selector, IOAudioDevicePowerState * powerState ) = 0;
	virtual IOReturn	setPowerState ( UInt32 selector, IOAudioDevicePowerState powerState ) = 0;
	virtual IOReturn	setBiquadCoefficients ( UInt32 selector, void * biquadCoefficients, UInt32 coefficientSize ) = 0;
	virtual IOReturn	getBiquadInformation ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr ) = 0;
	virtual IOReturn	getProcessingParameters ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr ) = 0;
	virtual IOReturn	setProcessingParameters ( UInt32 scalarArg1, void * inStructPtr, UInt32 inStructSize ) = 0;
	virtual	IOReturn	invokeInternalFunction ( UInt32 functionSelector, void * inData ) = 0;
protected:

	// activation functions
    virtual  UInt32	sndHWGetActiveOutputExclusive(void) = 0;
    virtual  IOReturn   sndHWSetActiveOutputExclusive(UInt32 outputPort ) = 0;
    virtual  UInt32 	sndHWGetActiveInputExclusive(void) = 0;
    virtual  IOReturn   sndHWSetActiveInputExclusive(UInt32 input )= 0;
    
	// control function
    virtual  bool   	sndHWGetSystemMute(void) = 0;
    virtual  IOReturn  	sndHWSetSystemMute(bool mutestate) = 0;
    virtual  bool   	sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume) = 0;
    virtual  IOReturn   sndHWSetSystemVolume(UInt32 value) = 0;
    virtual  IOReturn	sndHWSetPlayThrough(bool playthroughstate) = 0;
    virtual	 IOReturn	sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain) = 0;
    
	// Power Management

	// Identification
    virtual UInt32 	sndHWGetType( void ) = 0;
    virtual UInt32	sndHWGetManufacturer( void ) = 0;
};

//===========================================================================================================================
//	AppleLegacyOnboardAudioUserClient
//===========================================================================================================================

class	AppleLegacyOnboardAudioUserClient : public IOUserClient
{
    OSDeclareDefaultStructors( AppleLegacyOnboardAudioUserClient )
	
	public:
	
		static const IOExternalMethod		sMethods[];
		static const IOItemCount			sMethodCount;
	
	protected:
		
		Apple02Audio *					mDriver;
		task_t								mClientTask;
		
	public:
		
		//	WARNING:	The following enumerations must maintain the current order.  New
		//				enumerations may be added to the end of the list but must not
		//				be inserted into the center of the list.  Insertion of enumerations
		//				in the center of the list will cause the 'Audio Hardware Utility'
		//				to panic.  
		enum
		{
			kgpioReadIndex	 		=	0,		/*	returns data from gpio																		*/
			kgpioWriteIndex 		=	1,		/*	writes data to gpio																			*/
			kgetGpioActiveState		=	2,		/*	returns TRUE if gpio is active high															*/
			ksetGpioActiveState		=	3,		/*	sets the gpio active state (polarity)														*/
			kcheckIfGpioExists		=	4,		/*	returns TRUE if gpio exists on host CPU														*/
			kreadHWRegister32		=	5,		/*	returns data memory mapped I/O by hardware register reference								*/
			kwriteHWRegister32		=	6,		/*	writed stat to memory mapped I/O by hardware register reference								*/
			kreadCodecRegister		=	7,		/*	returns data CODEC (i.e. Screamer, DACA, Burgundy, Tumbler, Snapper by register reference	*/
			kwriteCodecRegister		=	8,		/*	writes data to CODEC (i.e. Screamer, DACA, Burgundy, Tumbler, Snapper by register reference	*/
			kreadSpeakerID			=	9,		/*	returns data from Dallas ROM																*/
			kgetCodecRegisterSize	=	10,		/*	return the size of a codec register in expressed in bytes									*/
			kreadPRAM				=	11,		/*	return PRAM contents																		*/
			kreadDMAState			=	12,		/*	return DMA state																			*/
			kreadStreamFormat		=	13,		/*	return IOAudioStreamFormat																	*/
			kreadPowerState			=	14,
			ksetPowerState			=	15,
			ksetBiquadCoefficients	=	16,
			kgetBiquadInfo			=	17,
			kgetProcessingParams	=	18,		/*	tbd: (see Aram)	*/
			ksetProcessingParams	=	19,		/*	tbd: (see Aram)	*/
			kinvokeInternalFunction	=	20
		};

		
		//	END WARNING:
		
		// Static member functions
		
		static AppleLegacyOnboardAudioUserClient * Create( Apple02Audio *inDriver, task_t task );
		
		// Creation/Deletion
		
		virtual bool		initWithDriver( Apple02Audio *inDriver, task_t task );
		virtual void		free();
		
		// Public API's
		
		virtual IOReturn	gpioRead ( UInt32 selector, UInt8 * gpioState );
		virtual IOReturn	gpioWrite ( UInt32 selector, UInt8 gpioState );
		virtual IOReturn	gpioGetActiveState (UInt32 selector, UInt8 * gpioActiveState);
		virtual IOReturn	gpioSetActiveState ( UInt32 selector, UInt8 gpioActiveState );
		virtual IOReturn	gpioCheckAvailable ( UInt32 selector, UInt8 * gpioExists );
		virtual IOReturn	hwRegisterRead32 ( UInt32 selector, UInt32 * registerData );
		virtual IOReturn	hwRegisterWrite32 ( UInt32 selector, UInt32 registerData );
		virtual IOReturn	codecReadRegister ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr );
		virtual IOReturn	codecWriteRegister ( UInt32 selector, void * data, UInt32 inStructSize );
		virtual IOReturn	readSpeakerID ( UInt32 selector, UInt32 * speakerIDPtr );
		virtual IOReturn	codecRegisterSize ( UInt32 selector, UInt32 * codecRegSizePtr );
		virtual IOReturn	readPRAMVolume ( UInt32 selector, UInt32 * pramDataPtr );
		virtual IOReturn	readDMAState ( UInt32 selector, UInt32 * dmaStatePtr );
		virtual IOReturn	readStreamFormat ( UInt32 selector, IOAudioStreamFormat * outStructPtr, IOByteCount * outStructSizePtr );
		virtual IOReturn	readPowerState ( UInt32 selector, IOAudioDevicePowerState * powerState );
		virtual IOReturn	setPowerState ( UInt32 selector, IOAudioDevicePowerState powerState );
		virtual IOReturn	setBiquadCoefficients ( UInt32 selector, void * biquadCoefficients, UInt32 coefficientSize );
		virtual IOReturn	getBiquadInfo ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr );
		virtual IOReturn	getProcessingParams ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr );
		virtual IOReturn	setProcessingParams ( UInt32 scalarArg1, void * inStructPtr, UInt32 inStructSize );
		virtual IOReturn	invokeInternalFunction ( UInt32 functionSelector, void * inData, UInt32 inDataSize );

	protected:
		
		// IOUserClient overrides
		
		virtual IOReturn			clientClose();
		virtual IOReturn			clientDied();
		virtual	IOExternalMethod *	getTargetAndMethodForIndex( IOService **outTarget, UInt32 inIndex );
};

#endif
