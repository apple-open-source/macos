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

static UInt32	volumeTable[] = {					// db = 20 LOG(x) but we just use table. from 0.0 to -70 db
	0x00000000,														// -infinity
	0x00000015,		0x00000016,		0x00000017,		0x00000019,		// -70.0,	-69.5,	-69.0,	-68.5,
	0x0000001A,		0x0000001C,		0x0000001D,		0x0000001F,		// -68.0,	-67.5,	-67.0,	-66.5,
	0x00000021,		0x00000023,		0x00000025,		0x00000027,		// -66.0,	-65.5,	-65.0,	-64.5,
	0x00000029,		0x0000002C,		0x0000002E,		0x00000031,		// -64.0,	-63.5,	-63.0,	-62.5,
	0x00000034,		0x00000037,		0x0000003A,		0x0000003E,		// -62.0,	-61.5,	-61.0,	-60.5,
	0x00000042,		0x00000045,		0x0000004A,		0x0000004E,		// -60.0,	-59.5,	-59.0,	-58.5,
	0x00000053,		0x00000057,		0x0000005D,		0x00000062,		// -58.0,	-57.5,	-57.0,	-56.5,
	0x00000068,		0x0000006E,		0x00000075,		0x0000007B,		// -56.0,	-55.5,	-55.0,	-54.5,
	0x00000083,		0x0000008B,		0x00000093,		0x0000009B,		// -54.0,	-53.5,	-53.0,	-52.5,
	0x000000A5,		0x000000AE,		0x000000B9,		0x000000C4,		// -52.0,	-51.5,	-51.0,	-50.5,
	0x000000CF,		0x000000DC,		0x000000E9,		0x000000F6,		// -50.0,	-49.5,	-49.0,	-48.5,
	0x00000105,		0x00000114,		0x00000125,		0x00000136,		// -48.0,	-47.5,	-47.0,	-46.5,
	0x00000148,		0x0000015C,		0x00000171,		0x00000186,		// -46.0,	-45.5,	-45.0,	-44.5,
	0x0000019E,		0x000001B6,		0x000001D0,		0x000001EB,		// -44.0,	-43.5,	-43.0,	-42.5,
	0x00000209,		0x00000227,		0x00000248,		0x0000026B,		// -42.0,	-41.5,	-41.0,	-40.5,
	0x0000028F,		0x000002B6,		0x000002DF,		0x0000030B,		// -40.0,	-39.5,	-39.0,	-38.5,
	0x00000339,		0x0000036A,		0x0000039E,		0x000003D5,		// -38.0,	-37.5,	-37.0,	-36.5,
	0x0000040F,		0x0000044C,		0x0000048D,		0x000004D2,		// -36.0,	-35.5,	-35.0,	-34.5,
	0x0000051C,		0x00000569,		0x000005BB,		0x00000612,		// -34.0,	-33.5,	-33.0,	-32.5,
	0x0000066E,		0x000006D0,		0x00000737,		0x000007A5,		// -32.0,	-31.5,	-31.0,	-30.5,
	0x00000818,		0x00000893,		0x00000915,		0x0000099F,		// -30.0,	-29.5,	-29.0,	-28.5,
	0x00000A31,		0x00000ACC,		0x00000B6F,		0x00000C1D,		// -28.0,	-27.5,	-27.0,	-26.5,
	0x00000CD5,		0x00000D97,		0x00000E65,		0x00000F40,		// -26.0,	-25.5,	-25.0,	-24.5,
	0x00001027,		0x0000111C,		0x00001220,		0x00001333,		// -24.0,	-23.5,	-23.0,	-22.5,
	0x00001456,		0x0000158A,		0x000016D1,		0x0000182B,		// -22.0,	-21.5,	-21.0,	-20.5,
	0x0000199A,		0x00001B1E,		0x00001CB9,		0x00001E6D,		// -20.0,	-19.5,	-19.0,	-18.5,
	0x0000203A,		0x00002223,		0x00002429,		0x0000264E,		// -18.0,	-17.5,	-17.0,	-16.5,
	0x00002893,		0x00002AFA,		0x00002D86,		0x00003039,		// -16.0,	-15.5,	-15.0,	-14.5,
	0x00003314,		0x0000361B,		0x00003950,		0x00003CB5,		// -14.0,	-13.5,	-13.0,	-12.5,
	0x0000404E,		0x0000441D,		0x00004827,		0x00004C6D,		// -12.0,	-11.5,	-11.0,	-10.5,
	0x000050F4,		0x000055C0,		0x00005AD5,		0x00006037,		// -10.0,	-9.5,	-9.0,	-8.5,
	0x000065EA,		0x00006BF4,		0x0000725A,		0x00007920,		// -8.0,	-7.5,	-7.0,	-6.5,
	0x0000804E,		0x000087EF,		0x00008FF6,		0x0000987D,		// -6.0,	-5.5,	-5.0,	-4.5,
	0x0000A186,		0x0000AB19,		0x0000B53C,		0x0000BFF9,		// -4.0,	-3.5,	-3.0,	-2.5,
	0x0000CB59,		0x0000D766,		0x0000E429,		0x0000F1AE,		// -2.0,	-1.5,	-1.0,	-0.5,
	0x00010000,		0x00010F2B,		0x00011F3D,		0x00013042,		// 0.0,		+0.5,	+1.0,	+1.5,
	0x00014249,		0x00015562,		0x0001699C,		0x00017F09,		// 2.0,		+2.5,	+3.0,	+3.5,
	0x000195BC,		0x0001ADC6,		0x0001C73D,		0x0001E237,		// 4.0,		+4.5,	+5.0,	+5.5,
	0x0001FECA,		0x00021D0E,		0x00023D1D,		0x00025F12,		// 6.0,		+6.5,	+7.0,	+7.5,
	0x0002830B,		0x0002A925,		0x0002D182,		0x0002FC42,		// 8.0,		+8.5,	+9.0,	+9.5,
	0x0003298B,		0x00035983,		0x00038C53,		0x0003C225,		// 10.0,	+10.5,	+11.0,	+11.5,
	0x0003FB28,		0x0004378B,		0x00047783,		0x0004BB44,		// 12.0,	+12.5,	+13.0,	+13.5,
	0x0005030A,		0x00054F10,		0x00059F98,		0x0005F4E5,		// 14.0,	+14.5,	+15.0,	+15.5,
	0x00064F40,		0x0006AEF6,		0x00071457,		0x00077FBB,		// 16.0,	+16.5,	+17.0,	+17.5,
	0x0007F17B														// +18.0 dB
};

// This is the coresponding dB values of the entries in the volumeTable arrary above.
static IOFixed	volumedBTable[] = {
	-70 << 16,														// Should really be -infinity
	-70 << 16,	-69 << 16 | 0x8000,	-69 << 16,	-68 << 16 | 0x8000,
	-68 << 16,	-67 << 16 | 0x8000,	-67 << 16,	-66 << 16 | 0x8000,
	-66 << 16,	-65 << 16 | 0x8000,	-65 << 16,	-64 << 16 | 0x8000,
	-64 << 16,	-63 << 16 | 0x8000,	-63 << 16,	-62 << 16 | 0x8000,
	-62 << 16,	-61 << 16 | 0x8000,	-61 << 16,	-60 << 16 | 0x8000,
	-60 << 16,	-59 << 16 | 0x8000,	-59 << 16,	-58 << 16 | 0x8000,
	-58 << 16,	-57 << 16 | 0x8000,	-57 << 16,	-56 << 16 | 0x8000,
	-56 << 16,	-55 << 16 | 0x8000,	-55 << 16,	-54 << 16 | 0x8000,
	-54 << 16,	-53 << 16 | 0x8000,	-53 << 16,	-52 << 16 | 0x8000,
	-52 << 16,	-51 << 16 | 0x8000,	-51 << 16,	-50 << 16 | 0x8000,
	-50 << 16,	-49 << 16 | 0x8000,	-49 << 16,	-48 << 16 | 0x8000,
	-48 << 16,	-47 << 16 | 0x8000,	-47 << 16,	-46 << 16 | 0x8000,
	-46 << 16,	-45 << 16 | 0x8000,	-45 << 16,	-44 << 16 | 0x8000,
	-44 << 16,	-43 << 16 | 0x8000,	-43 << 16,	-42 << 16 | 0x8000,
	-42 << 16,	-41 << 16 | 0x8000,	-41 << 16,	-40 << 16 | 0x8000,
	-40 << 16,	-39 << 16 | 0x8000,	-39 << 16,	-38 << 16 | 0x8000,
	-38 << 16,	-37 << 16 | 0x8000,	-37 << 16,	-36 << 16 | 0x8000,
	-36 << 16,	-35 << 16 | 0x8000,	-35 << 16,	-34 << 16 | 0x8000,
	-34 << 16,	-33 << 16 | 0x8000,	-33 << 16,	-32 << 16 | 0x8000,
	-32 << 16,	-31 << 16 | 0x8000,	-31 << 16,	-30 << 16 | 0x8000,
	-30 << 16,	-29 << 16 | 0x8000,	-29 << 16,	-28 << 16 | 0x8000,
	-28 << 16,	-27 << 16 | 0x8000,	-27 << 16,	-26 << 16 | 0x8000,
	-26 << 16,	-25 << 16 | 0x8000,	-25 << 16,	-24 << 16 | 0x8000,
	-24 << 16,	-23 << 16 | 0x8000,	-23 << 16,	-22 << 16 | 0x8000,
	-22 << 16,	-21 << 16 | 0x8000,	-21 << 16,	-20 << 16 | 0x8000,
	-20 << 16,	-19 << 16 | 0x8000,	-19 << 16,	-18 << 16 | 0x8000,
	-18 << 16,	-17 << 16 | 0x8000,	-17 << 16,	-16 << 16 | 0x8000,
	-16 << 16,	-15 << 16 | 0x8000,	-15 << 16,	-14 << 16 | 0x8000,
	-14 << 16,	-13 << 16 | 0x8000,	-13 << 16,	-12 << 16 | 0x8000,
	-12 << 16,	-11 << 16 | 0x8000,	-11 << 16,	-10 << 16 | 0x8000,
	-10 << 16,	 -9 << 16 | 0x8000,	 -9 << 16,	 -8 << 16 | 0x8000,
	 -8 << 16,	 -7 << 16 | 0x8000,	 -7 << 16,	 -6 << 16 | 0x8000,
	 -6 << 16,	 -5 << 16 | 0x8000,	 -5 << 16,	 -4 << 16 | 0x8000,
	 -4 << 16,	 -3 << 16 | 0x8000,	 -3 << 16,	 -2 << 16 | 0x8000,
	 -2 << 16,	 -1 << 16 | 0x8000,	 -1 << 16,	  0 << 16 | 0x8000,
	  0 << 16,	  0 << 16 | 0x8000,	 +1 << 16,	 +1 << 16 | 0x8000,
	 +2 << 16,	 +2 << 16 | 0x8000,	 +3 << 16,	 +3 << 16 | 0x8000,
	 +4 << 16,	 +4 << 16 | 0x8000,	 +5 << 16,	 +5 << 16 | 0x8000,
	 +6 << 16,	 +6 << 16 | 0x8000,	 +7 << 16,	 +7 << 16 | 0x8000,
	 +8 << 16,	 +8 << 16 | 0x8000,	 +9 << 16,	 +9 << 16 | 0x8000,
	+10 << 16,	+10 << 16 | 0x8000,	+11 << 16,	+11 << 16 | 0x8000,
	+12 << 16,	+12 << 16 | 0x8000,	+13 << 16,	+13 << 16 | 0x8000,
	+14 << 16,	+14 << 16 | 0x8000,	+15 << 16,	+15 << 16 | 0x8000,
	+16 << 16,	+16 << 16 | 0x8000,	+17 << 16,	+17 << 16 | 0x8000,
	+18 << 16
};

//	[3787193]
//	TAS3004 SLAS325 Specification, 6.3.3, p. 6-3:	Wait states lasts from 42 mS to 231 mS
//													Wait states ... occasionally up to 15 mS
//	Note that the 231 mS delay above only applies to volume, bass and treble (i.e 3 registers maximum)
//	while the 15 mS delay above only applies to the biquad filters.  A 300 µS delay applies to
//	the DRC (rounded up to 1 mS) while all other registers do not implement wait states.  All non
//	delayed register transactions are rounded up to an assumption of 1 mS per transaction.
//	Timing info:		RESET Setup									  5 mS 
//						RESET Hold									 20 mS 
//						RESET Release								 10 mS 
//						Register Flush:	Volume, Bass & Treble		693 mS		(i.e. 3 registers * 231 mS)
//										DRC							  1 mS
//										EQ							240 mS		(i.e. 16 registers * 15 mS)
//										All others					  7 mS		(i.3. 7 registers * 1 mS)
//	
//						TOTAL										976 mS
//						
#define	kTAS3004_SLEEP_TIME_MICROSECONDS	976000		/*	reset of 35 mS + register flush of 30 registers at 15.0 mS per register	*/

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

	virtual IOReturn	performSetPowerState ( UInt32 currentPowerState, UInt32 pendingPowerState );	//	[3933529]
	virtual IOReturn	performDeviceWake ();
	virtual IOReturn	performDeviceSleep ();

	virtual IOReturn	setSampleRate ( UInt32 sampleRate );
	virtual IOReturn	setSampleDepth ( UInt32 sampleDepth );

	virtual UInt32		getClockLock ( void ) { return 0; }
	virtual IOReturn	breakClockSelect ( UInt32 clockSource );
	virtual IOReturn	makeClockSelect ( UInt32 clockSource );

	virtual void			poll ( void ) { return; }

	virtual IOReturn		requestSleepTime ( UInt32 * microsecondsUntilComplete );	//	[3787193]

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
