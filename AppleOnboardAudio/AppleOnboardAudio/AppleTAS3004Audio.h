/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
 * Interface definition for the TAS3004 audio Controller
 *
 * HISTORY
 *
 */

#ifndef _APPLETAS3004AUDIO_H
#define _APPLETAS3004AUDIO_H

#include "TAS_hw.h"
#include "AppleDBDMAAudio.h"
#include "AppleOnboardAudio.h"
#include "AudioHardwareObjectInterface.h"
#include "AppleOnboardAudioUserClient.h"

class IORegistryEntry;

class AppleTAS3004Audio : public AudioHardwareObjectInterface
{
    OSDeclareDefaultStructors(AppleTAS3004Audio);

private:
	SInt32					minVolume;
	SInt32					maxVolume;
	Boolean					gVolMuteActive;
	Boolean					headphonesActive;
	Boolean					lineOutActive;
	Boolean					headphonesConnected;
	Boolean					lineOutConnected;								
	Boolean					dallasSpeakersConnected;
	UInt32					layoutID;									// The ID of the machine we're running on
	UInt32					familyID;									// The ID of the speakers that are plugged in (required for rom verification)
	UInt32					speakerID;									// The ID of the speakers that are plugged in
	UInt32					detectCollection;
	TAS3004_ShadowReg		shadowTAS3004Regs;							// write through shadow registers for TAS3004
	TAS3004_ShadowReg		standbyTAS3004Regs;							// [3280002] used for filter coefficient management
	Boolean					mSemaphores;
	UInt32					deviceID;
	AppleOnboardAudio *		mAudioDeviceProvider;
	Boolean					mTAS_WasDead;
	Boolean					mEQDisabled;

	static const UInt8		kDEQAddress;								// Address for i2c TAS3004

public:
    // Classical Unix driver functions
    virtual bool		init (OSDictionary *properties);
    virtual void		free ();
	virtual bool		start (IOService * provider);
	virtual bool 		willTerminate (IOService * provider, IOOptionBits options);
	virtual bool 		requestTerminate (IOService * provider, IOOptionBits options);

    // Initializatioin
    virtual bool		preDMAEngineInit () ;
	virtual void		initPlugin (PlatformInterface* inPlatformObject);

    // IO activation functions
    virtual IOReturn	setActiveInput (UInt32 input);
   
    // control function
    virtual IOReturn	setCodecMute (bool mutestate);								//	[3435307]	rbm
	virtual IOReturn	setCodecMute (bool muteState, UInt32 streamType);			//	[3435307]	rbm
	virtual bool		hasAnalogMute ();											//	[3435307]	rbm
	virtual	UInt32		getMaximumdBVolume (void);
	virtual	UInt32		getMinimumdBVolume (void);
	virtual	UInt32		getMaximumVolume (void);
	virtual	UInt32		getMinimumVolume (void);
	virtual	UInt32		getMaximumdBGain (void);
	virtual	UInt32		getMinimumdBGain (void);
	virtual	UInt32		getMaximumGain (void);
	virtual	UInt32		getMinimumGain (void);
	virtual	UInt32		getDefaultInputGain (void);
	virtual	UInt32		getDefaultOutputVolume (void);

    virtual bool		setCodecVolume (UInt32 leftVolume, UInt32 rightVolume);		//	[3435307]	rbm

    virtual IOReturn	setPlayThrough (bool playthroughstate);

	virtual	void		setEQProcessing (void * inEQStructure, Boolean inRealtime);
	virtual	void		setDRCProcessing (void * inDRCStructure, Boolean inRealtime);
	virtual	void		disableProcessing (Boolean inRealtime);
	virtual	void		enableProcessing (void);

	virtual	void		notifyHardwareEvent ( UInt32 statusSelector, UInt32 newValue ) { return; }
	virtual	IOReturn	recoverFromFatalError ( FatalRecoverySelector selector );

	virtual IOReturn	performDeviceWake ();
	virtual IOReturn	performDeviceSleep ();

	virtual IOReturn	setSampleRate ( UInt32 sampleRate );
	virtual IOReturn	setSampleDepth ( UInt32 sampleDepth );

	virtual UInt32		getClockLock ( void ) { return 0; }
	virtual IOReturn	breakClockSelect ( UInt32 clockSource );
	virtual IOReturn	makeClockSelect ( UInt32 clockSource );

	virtual void			poll ( void ) { return; }

	//	
	//	User Client Support
	//
	virtual IOReturn	getPluginState ( HardwarePluginDescriptorPtr outState );
	virtual IOReturn	setPluginState ( HardwarePluginDescriptorPtr inState );
	virtual	HardwarePluginType	getPluginType ( void );
	
private:
	// activation functions
	IOReturn			SetVolumeCoefficients (UInt32 left, UInt32 right);
	IOReturn			SetAmplifierMuteState (UInt32 ampID, Boolean muteState);
	IOReturn			InitEQSerialMode (UInt32 mode);
	IOReturn 			GetShadowRegisterInfo( TAS3004_ShadowReg * shadowRegsPtr, UInt8 regAddr, UInt8 ** shadowPtr, UInt8* registerSize );

	IOReturn			CODEC_Initialize ();
    void				CODEC_Reset ( void );
	IOReturn			CODEC_ReadRegister (UInt8 regAddr, UInt8* registerData);
	IOReturn			CODEC_WriteRegister (UInt8 regAddr, UInt8* registerData, UInt8 mode);

	void				SetBiquadInfoToUnityAllPass (void);
	void				SetUnityGainAllPass (void);
	IOReturn			BuildCustomEQCoefficients ( void * eqPrefs );
	IOReturn			SndHWSetOutputBiquad( UInt32 streamID, UInt32 biquadRefNum, FourDotTwenty *biquadCoefficients );
	IOReturn			SndHWSetOutputBiquadGroup( UInt32 biquadFilterCount, FourDotTwenty *biquadCoefficients );
	IOReturn			SetOutputBiquadCoefficients (UInt32 streamID, UInt32 biquadRefNum, UInt8 *biquadCoefficients);
	IOReturn			setBiquadCoefficients ( void * biquadCoefficients );

	IOReturn			SetAnalogPowerDownMode( UInt8 mode );
	IOReturn			ToggleAnalogPowerDownWake( void );
	IORegistryEntry *	FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value);
	IORegistryEntry *	FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value);
	Boolean				HasInput (void);

	//	The normal volume range is from 0.0 dB to -70 dB.  A setting of -70.5 dB results in a muted state.
	//	A value of 0 represents -70.5 dB.  Volume increases 0.5 dB per step.  A value of 141 represents
	//	-70.5 dB + ( 0.5 dB X 141 ) = - 70.0 dB + 70.5 = 0.0 dB.  The absolute maximum available volume
	//	is +18.0 dB.  A value of 177 represents -70.5 dB + ( 0.5 X 177 ) = -70.5 dB + 88.5 dB.
	enum  {
		kMaximumVolume = 141,
		kMinimumVolume = 0,
		kInitialVolume = 101
	};
	
	enum {
		kInternalSpeakerActive	= 1,
		kHeadphonesActive		= 2,
		kExternalSpeakersActive	= 4
	};

	EQPrefsElement		mEQPref;

};

#endif /* _APPLETAS3004AUDIO_H */
