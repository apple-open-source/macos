#ifndef _APPLEAHOI_H
#define _APPLEAHOI_H

#include <IOKit/IOService.h>

#include "AppleDBDMAAudio.h"
#include "PlatformInterface.h"
#include "AppleOnboardAudioUserClient.h"

typedef enum {
	kControlBusFatalErrorRecovery	= 0,
	kClockSourceInterruptedRecovery
} FatalRecoverySelector;

const static UInt32 kFamilyOption_OpenMultiple = 0x00010000;

class AudioHardwareObjectInterface : public IOService {

    OSDeclareDefaultStructors(AudioHardwareObjectInterface);

protected:
	PlatformInterface *		mPlatformInterface;
	IOWorkLoop *			mWorkLoop;

    SInt32					mVolLeft;
    SInt32					mVolRight;
    SInt32					mGainLeft;
    SInt32					mGainRight;
	Boolean					mAnalogMuteState;
	Boolean					mDigitalMuteState;
	Boolean					mDisableLoadingEQFromFile;

public:

	virtual void			initPlugin(PlatformInterface* inPlatformObject) {return;}
	virtual bool			preDMAEngineInit () {return false;}
	virtual bool			postDMAEngineInit () {return true;}

	virtual void			setWorkLoop (IOWorkLoop * inWorkLoop) {mWorkLoop = inWorkLoop;}
	virtual IOWorkLoop *	getWorkLoop () { return mWorkLoop; } 
	
	virtual bool 			willTerminate ( IOService * provider, IOOptionBits options ) {return false;}
	virtual bool 			requestTerminate ( IOService * provider, IOOptionBits options ) {return false;}
	
	virtual UInt32			getActiveOutput (void) {return 0;}
	virtual IOReturn		setActiveOutput (UInt32 outputPort) {return kIOReturnError;}
	virtual UInt32			getActiveInput (void) {return 0;}
	virtual IOReturn		setActiveInput (UInt32 input) {return kIOReturnError;}

	IOReturn				setMute (bool muteState);														//	[3435307]	rbm
	IOReturn				setMute (bool muteState, UInt32 streamType);									//	[3435307]	rbm
	virtual IOReturn		setCodecMute (bool muteState) {return 0;}										//	[3435307]	rbm
	virtual IOReturn		setCodecMute (bool muteState, UInt32 streamType) {return kIOReturnError;}		//	[3435307]	rbm
	virtual bool			hasAnalogMute () { return false; }												//	[3435307]	rbm
	virtual bool			hasDigitalMute () { return false; }												//	[3435307]	rbm
	virtual bool			hasHardwareVolume () { return true; }											//	[3527440]	aml

	virtual bool			getInputMute () {return false;}
	virtual IOReturn		setInputMute (bool muteState) {return 0;}

	virtual	UInt32			getMaximumdBVolume (void) {return 0;}
	virtual	UInt32			getMinimumdBVolume (void) {return 0;}
	virtual	UInt32			getMaximumVolume (void) {return 0;}
	virtual	UInt32			getMinimumVolume (void) {return 0;}
	virtual	UInt32			getMaximumdBGain (void) {return 0;}
	virtual	UInt32			getMinimumdBGain (void) {return 0;}
	virtual	UInt32			getMaximumGain (void) {return 0;}
	virtual	UInt32			getMinimumGain (void) {return 0;}
	virtual UInt32			getDefaultInputGain ( void ) { return 0; }										//	[3514617]	rbm		3 Feb 2004

	bool					setVolume (UInt32 leftVolume, UInt32 rightVolume);
	virtual bool			setCodecVolume (UInt32 leftVolume, UInt32 rightVolume) {return false;}
	virtual IOReturn		setPlayThrough (bool playthroughState) {return kIOReturnError;}
	virtual IOReturn		setInputGain (UInt32 leftGain, UInt32 rightGain) {return kIOReturnError;}
	
	virtual	void			setProcessing (UInt32 inEQIndex) {return;}
	virtual	void			setEQProcessing (void * inEQStructure, Boolean inRealtime) {return;}
	virtual	void			setDRCProcessing (void * inDRCStructure, Boolean inRealtime) {return;}
	virtual	void			disableProcessing (Boolean inRealtime) {return;}
	virtual	void			enableProcessing (void) {return;}
	
	virtual IOReturn		performDeviceSleep () {return kIOReturnError;}
	virtual IOReturn		performDeviceWake () {return kIOReturnError;}

	virtual IOReturn		setSampleRate ( UInt32 sampleRate ) { return kIOReturnError; }
	virtual IOReturn		setSampleDepth ( UInt32 sampleDepth ) { return kIOReturnError; }
	virtual IOReturn		setSampleType ( UInt32 sampleType ) { return kIOReturnSuccess; }
	
	virtual UInt32			getClockLock ( void ) { return 0; }
	virtual IOReturn		breakClockSelect ( UInt32 clockSource ) { return kIOReturnError; }
	virtual IOReturn		makeClockSelect ( UInt32 clockSource ) { return kIOReturnError; }
	
	virtual	void			notifyHardwareEvent ( UInt32 statusSelector, UInt32 newValue ) { return; } 

	virtual IOReturn		recoverFromFatalError ( FatalRecoverySelector selector ) {return kIOReturnError;}

	virtual	UInt32			getCurrentSampleFrame (void) {return 0;}
	virtual void			setCurrentSampleFrame (UInt32 value) {return;}
	
	virtual void			poll ( void ) { return; }
	virtual bool			getClockLockStatus ( void ) { return FALSE; }

	//	
	//	User Client Support
	//
	virtual IOReturn			getPluginState ( HardwarePluginDescriptorPtr outState ) = 0;
	virtual IOReturn			setPluginState ( HardwarePluginDescriptorPtr inState ) = 0;
	virtual	HardwarePluginType	getPluginType ( void ) = 0;
};

#if 0
//===========================================================================================================================
//	AppleLegacyOnboardAudioUserClient
//===========================================================================================================================

class	AudioHardwareObjectUserClient : public IOUserClient
{
    OSDeclareDefaultStructors( AudioHardwareObjectUserClient )
	
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
#endif _APPLEAHOI_H
