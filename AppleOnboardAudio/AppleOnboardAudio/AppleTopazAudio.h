/*
 *  AppleTopazAudio.h
 *  AppleOnboardAudio
 *
 *  Created by Matthew Xavier Mora on Thu Mar 13 2003.
 *  Copyright © 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "AudioHardwareObjectInterface.h"
#include "CS8420_hw.h"
#include "PlatformInterface.h"
#include "AppleDBDMAAudio.h"
#include "AppleOnboardAudio.h"
#include "AppleOnboardAudioUserClient.h"

typedef enum {
	kCS8406_CODEC = 0,
	kCS8420_CODEC
} TOPAZ_CODEC_TYPES;

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
	virtual void			postDMAEngineInit ();
	virtual UInt32			getActiveOutput (void) { return kSndHWOutput1; }
	virtual IOReturn		setActiveOutput (UInt32 outputPort) { return kIOReturnSuccess; }
	virtual UInt32			getActiveInput (void) { return kSndHWInput1; }
	virtual IOReturn		setActiveInput (UInt32 input) { return kIOReturnSuccess; }
	virtual bool			getMute ();
	virtual IOReturn		setMute (bool muteState);
	virtual bool			setVolume (UInt32 leftVolume, UInt32 rightVolume) { return false; }
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
	virtual	UInt32			getCurrentSampleFrame (void);
	virtual void			setCurrentSampleFrame (UInt32 value);

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
	
private:
	UInt8					CODEC_GetDataMask ( UInt8 regAddr );
	IOReturn				CODEC_GetRegSize ( UInt32 selector, UInt32 * codecRegSizePtr );
	IOReturn 				CODEC_IsControlRegister ( UInt8 regAddr );
	IOReturn 				CODEC_IsStatusRegister ( UInt8 regAddr );
	IOReturn 				CODEC_ReadRegister ( UInt8 regAddr, UInt8 * registerData, UInt32 size );
	void					CODEC_Reset ( void );
	IOReturn				CODEC_SetChannelStatus ( void );
	IOReturn 				CODEC_WriteRegister ( UInt8 regAddr, UInt8 registerData );
	
	void					generalRecovery ( void );
	
	UInt8					mShadowRegs[128];	//	write through shadow registers for CS84xx
	PlatformInterface* 		mPlatformObject;	// pointer to platform object class
	TOPAZ_CODEC_TYPES		mCodecID;
	
	bool					mCurMuteState;
	bool					mNonAudio;
	UInt8					mCurrentMAP;
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
	
protected:
	AppleOnboardAudio *		mAudioDeviceProvider;
	
	UInt32					mSampleRate;
	UInt32					mSampleDepth;
};
