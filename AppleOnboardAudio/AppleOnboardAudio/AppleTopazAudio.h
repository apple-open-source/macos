/*
 *  AppleTopazAudio.h
 *  AppleOnboardAudio
 *
 *  Created by Matthew Xavier Mora on Thu Mar 13 2003.
 *  Copyright © 2003 Apple Computer, Inc. All rights reserved.
 *
 *	29 October 2003		rbm		[3446131]	Converted to plugin architecture.
 *
 */

#include "PlatformInterface.h"
#include "AudioHardwareUtilities.h"
#include "AppleTopazPluginFactory.h"
#include "AudioHardwareObjectInterface.h"
#include "CS8420_hw.h"
#include "AppleDBDMAAudio.h"
#include "AppleOnboardAudio.h"
#include "AppleOnboardAudioUserClient.h"
#include "AppleTopazPlugin.h"

typedef enum {
	kTopazState_Idle =	0,
	kTopazState_PrepareToArmLossOfAES3,
	kTopazState_ArmLossOfAES3,
	kTopazState_TriggerLossOfAES3,
	kTopazState_PrepareToArmTryAES3,
	kTopazState_ArmTryAES3,
	kTopazState_TriggerTryAES3
} TOPAZ_STATE_MACHINE_STATES;

typedef enum {
	kMachine2_idleState = 0,
	kMachine2_startState,
	kMachine2_delay1State,
	kMachine2_setRxd_ILRCK,
	kMachine2_setRxd_AES3
} STATE_MACHINE_2;

#define	kCLOCK_UNLOCK_ERROR_TERMINAL_COUNT	3

class AppleTopazAudio : public AudioHardwareObjectInterface
{
    OSDeclareDefaultStructors(AppleTopazAudio);

public:

	virtual	bool 			init(OSDictionary *properties);
	virtual	bool			start(IOService * provider);
    virtual void			free();
	virtual void			initPlugin(PlatformInterface* inPlatformObject);
	virtual bool			preDMAEngineInit ();
	virtual UInt32			getActiveOutput (void) { return kSndHWOutput1; }
	virtual IOReturn		setActiveOutput (UInt32 outputPort) { return kIOReturnSuccess; }
	virtual UInt32			getActiveInput (void) { return kSndHWInput1; }
	virtual IOReturn		setActiveInput (UInt32 input) { return kIOReturnSuccess; }
	virtual IOReturn		setCodecMute (bool muteState);												//	[3435307]	rbm
	virtual IOReturn		setCodecMute (bool muteState, UInt32 streamType);							//	[3435307]	rbm
	virtual bool			hasDigitalMute ();															//	[3435307]	rbm
	virtual bool			setCodecVolume (UInt32 leftVolume, UInt32 rightVolume) { return false; }	//	[3435307]	rbm
	virtual IOReturn		setPlayThrough (bool playthroughState) { return kIOReturnError; }
	virtual IOReturn		setInputGain (UInt32 leftGain, UInt32 rightGain) { return kIOReturnError; }
	virtual IOReturn		performDeviceSleep ();
	virtual IOReturn		performDeviceWake ();

	virtual IOReturn		setSampleRate ( UInt32 sampleRate );
	virtual IOReturn		setSampleDepth ( UInt32 sampleDepth );
	virtual IOReturn		setSampleType ( UInt32 sampleType );

	virtual UInt32			getClockLock ( void );
	virtual IOReturn		breakClockSelect ( UInt32 clockSource );
	virtual IOReturn		makeClockSelect ( UInt32 clockSource );

	virtual	void			notifyHardwareEvent ( UInt32 statusSelector, UInt32 newValue ); 

	virtual IOReturn		recoverFromFatalError ( FatalRecoverySelector selector );

	virtual bool 			willTerminate ( IOService * provider, IOOptionBits options );
	virtual bool 			requestTerminate ( IOService * provider, IOOptionBits options );
	
	virtual void			poll ( void );
	void					stateMachine1 ( void );
	void					stateMachine2 ( void );
	
	//	
	//	User Client Support
	//
	
	virtual IOReturn		getPluginState ( HardwarePluginDescriptorPtr outState );
	virtual IOReturn		setPluginState ( HardwarePluginDescriptorPtr inState );
	virtual	HardwarePluginType	getPluginType ( void );
	
	ChanStatusStruct		mChannelStatus;
	
	UInt8	 				CODEC_ReadID ( void );
	void					CODEC_Reset ( void );

private:
	
	void					generalRecovery ( void );
	
	PlatformInterface* 		mPlatformObject;	// pointer to platform object class
	TOPAZ_CODEC_TYPES		mCodecID;
	
	UInt32					mUnlockErrorCount;
	UInt32					mI2CinProcess;
	
	UInt32					mClockSwitchRecoveryPending;
	UInt32					mCurrentMachine1State;
	UInt32					mCurrentMachine2State;
	
	bool					mRecoveryInProcess;
	bool					mUnlockStatus;
	bool					mAES3detected;
	bool					mAttemptingExternalLock;
	
	UInt32 					mClockSource;
	
	AppleTopazPlugin *		mTopazPlugin;

protected:
	AppleOnboardAudio *		mAudioDeviceProvider;
	
	UInt32					mSampleRate;
	UInt32					mSampleDepth;
};
