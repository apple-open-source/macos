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
 * Interface definition for the Texas2 audio Controller
 *
 * HISTORY
 *
 */

#ifndef _APPLETEXAS2AUDIO_H
#define _APPLETEXAS2AUDIO_H

#include <IOKit/i2c/PPCI2CInterface.h>

#include "AppleDallasDriver.h"
#include "AppleOnboardAudio.h"
#include "Texas2_hw.h"

//#define kLOG_EQ_TABLE_TRAVERSE		//	un-comment this to log GetCustomEQCoefficients table traverse
//#define kDEBUG_GPIO					//	un-comment this to log GPIO transactions

class IOInterruptEventSource;
class IORegistryEntry;

struct awacs_regmap_t;
struct IODBDMAChannelRegisters;
struct IODBDMADescriptor;

#if 0
#define kInsertionDelayNanos		4000000000ULL		/*	4 seconds				*/
#else
#define kInsertionDelayNanos		1000000000ULL		/*	[2875924]	1 second	*/
#endif
#define kNotifyTimerDelay			60000	// in milliseconds =  60 seconds
#define kUserLoginDelay				20000
#define kSpeakerConnectError		"SpeakerConnectError"

enum  {
	kMaximumVolume = 141,
	kMinimumVolume = 0,
	kInitialVolume = 101
};

typedef Boolean GpioActiveState;
typedef UInt8* GpioPtr;

enum {
	kInternalSpeakerActive	= 1,
	kHeadphonesActive		= 2,
	kExternalSpeakersActive	= 4
};

// Characteristic constants:
typedef enum TicksPerFrame {
	k64TicksPerFrame		= 64,			// 64 ticks per frame
	k32TicksPerFrame		= 32			// 32 ticks per frame
} TicksPerFrame;

typedef enum ClockSource {
	kClock49MHz				= 49152000,		// 49 MHz clock source
	kClock45MHz				= 45158400,		// 45 MHz clock source
	kClock18MHz				= 18432000		// 18 MHz clock source
} ClockSource;

// Sound Formats:
// FIXME: these values are "interpreted" and mirrored in specific chip values
// so wouldn't be better to have them in some parent class?
typedef enum SoundFormat {
	kSndIOFormatI2SSony,
	kSndIOFormatI2S64x,
	kSndIOFormatI2S32x,

	// This says "we never decided for a sound format before"
	kSndIOFormatUnknown
} SoundFormat;

// declare a class for our driver.  This is based from AppleOnboardAudio
class AppleTexas2Audio : public AppleOnboardAudio
{
    OSDeclareDefaultStructors(AppleTexas2Audio);

    friend class AudioHardwareOutput;
    friend class AudioHardwareInput;
    friend class AudioHardwareMux;

protected:
	SInt32					minVolume;
	SInt32					maxVolume;
	Boolean					gVolMuteActive;
	Boolean					mCanPollStatus;
	IOInterruptEventSource *headphoneIntEventSource;
	IOInterruptEventSource *lineOutIntEventSource;						//	[2788199]
	IOInterruptEventSource *dallasIntEventSource;
	Boolean					hasVideo;									// TRUE if this hardware has video out its headphone jack
	Boolean					hasSerial;									// TRUE if this hardware has a serial connection on its headphone jack
	Boolean					headphonesActive;
	Boolean					lineOutActive;
	Boolean					headphonesConnected;
	Boolean					lineOutConnected;							//	[2788199]
	Boolean					dallasSpeakersConnected;
	DRCInfo					drc;										// dynamic range compression info
	UInt32					layoutID;									// The ID of the machine we're running on
	UInt32					speakerID;									// The ID of the speakers that are plugged in
	GpioPtr					hwResetGpio;
	GpioPtr					hdpnMuteGpio;
	GpioPtr					ampMuteGpio;
	GpioPtr					lineInExtIntGpio;							//	[叩ew包
	GpioPtr					lineOutExtIntGpio;							//	[叩ew包
	GpioPtr					lineOutMuteGpio;							//	[叩ew包
	GpioPtr					masterMuteGpio;								//	[2933090]
	GpioPtr					headphoneExtIntGpio;
	GpioPtr					dallasExtIntGpio;
	GpioActiveState			hwResetActiveState;							//	indicates asserted state (i.e. '0' or '1')
	GpioActiveState			hdpnActiveState;							//	indicates asserted state (i.e. '0' or '1')
	GpioActiveState			ampActiveState;								//	indicates asserted state (i.e. '0' or '1')
	GpioActiveState			headphoneInsertedActiveState;
	GpioActiveState			dallasInsertedActiveState;
	GpioActiveState			lineInExtIntActiveState;					//	indicates asserted state (i.e. '0' or '1')	[叩ew包
	GpioActiveState			lineOutExtIntActiveState;					//	indicates asserted state (i.e. '0' or '1')	[叩ew包
	GpioActiveState			lineOutMuteActiveState;						//	indicates asserted state (i.e. '0' or '1')	[叩ew包
	GpioActiveState			masterMuteActiveState;						//	indicates asserted state (i.e. '0' or '1')	[2933090]
	UInt32					detectCollection;							//	[2878119]
	UInt8					DEQAddress;									// Address for i2c Texas2
	Texas2_ShadowReg		shadowTexas2Regs;							// write through shadow registers for Texas2
	Boolean					semaphores;
	UInt32					deviceID;
	double					biquadGroupInfo[kNumberOfBiquadCoefficients];
	IOService *				ourProvider;
	IODeviceMemory *		hdpnMuteRegMem;								// Have to free this in free()
	IODeviceMemory *		ampMuteRegMem;								// Have to free this in free()
	IODeviceMemory *		hwResetRegMem;								// Have to free this in free()
	IODeviceMemory *		headphoneExtIntGpioMem;						// Have to free this in free()
	IODeviceMemory *		dallasExtIntGpioMem;						// Have to free this in free()
	IODeviceMemory *		lineInExtIntGpioMem;						// Have to free this in free()	[叩ew包
	IODeviceMemory *		lineOutExtIntGpioMem;						// Have to free this in free()	[叩ew包
	IODeviceMemory *		lineOutMuteGpioMem;							// Have to free this in free()	[叩ew包
	IODeviceMemory *		masterMuteGpioMem;							// Have to free this in free()	[2933090]
	UInt32					i2sSerialFormat;
	IOService *				headphoneIntProvider;
	IOService *				lineOutIntProvider;							//	[2788199]
	IOService *				dallasIntProvider;
	const OSSymbol *		gAppleAudioVideoJackStateKey;
	const OSSymbol *		gAppleAudioSerialJackStateKey;
	AppleDallasDriver *		dallasDriver;
	IONotifier *			dallasDriverNotifier;
	IOTimerEventSource *	dallasHandlerTimer;
	IOTimerEventSource *	notifierHandlerTimer;
	Boolean					doneWaiting;
	UInt64					savedNanos; 
	Boolean					speakerConnectFailed;
    UInt32					activeOutput;								//	[2855519]
	Boolean					useMasterVolumeControl;
	UInt32					lastLeftVol;
	UInt32					lastRightVol;
	Boolean					dallasSpeakersProbed;						// So we only probe the speakers once when they are plugged in
	Boolean					hasANDedReset;								//	[2855519]

	// information specific to the chip
	Boolean					gModemSoundActive;
	Boolean					dontReleaseHPMute;

	// holds the current frame rate settings:
	ClockSource				clockSource;
	UInt32					mclkDivisor;
	UInt32					sclkDivisor;
	SoundFormat				serialFormat;

    // Hardware register manipulation
    virtual void 		sndHWInitialize(IOService *provider) ;
	virtual void		sndHWPostDMAEngineInit (IOService *provider);
    virtual UInt32 		sndHWGetInSenseBits(void) ;
    virtual UInt32 		sndHWGetRegister(UInt32 regNum) ;
    virtual IOReturn   	sndHWSetRegister(UInt32 regNum, UInt32 value) ;

    // IO activation functions
    virtual  UInt32		sndHWGetActiveOutputExclusive(void);
    virtual  IOReturn   sndHWSetActiveOutputExclusive(UInt32 outputPort );
    virtual  UInt32 	sndHWGetActiveInputExclusive(void);
    virtual  IOReturn   sndHWSetActiveInputExclusive(UInt32 input );
    
    // control function
    virtual  bool   	sndHWGetSystemMute(void);
    virtual  IOReturn  	sndHWSetSystemMute(bool mutestate);
    virtual  bool   	sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume);
    virtual  IOReturn   sndHWSetSystemVolume(UInt32 value);
    virtual  IOReturn	sndHWSetPlayThrough(bool playthroughstate);
    virtual  IOReturn   sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain) ;
   

    //Identification
    virtual UInt32 		sndHWGetType( void );
    virtual UInt32		sndHWGetManufacturer( void );

    virtual void setDeviceDetectionActive (void);
    virtual void setDeviceDetectionInActive (void);      

public:
    // Classical Unix driver functions
    virtual bool 		init(OSDictionary *properties);
    virtual void 		free();

    virtual IOService* probe(IOService *provider, SInt32*);

    //IOAudioDevice subclass
    virtual bool 		initHardware(IOService *provider);
            
    //Power Management
    virtual  IOReturn   sndHWSetPowerState(IOAudioDevicePowerState theState);
    
    // 
    virtual  UInt32		sndHWGetConnectedDevices(void);
    virtual  UInt32 	sndHWGetProgOutput();
    virtual  IOReturn   sndHWSetProgOutput(UInt32 outputBits);

private:
    // These will probably change when we have a general method
    // to verify the Detects.  Wait til we figure out how to do 
    // this with interrupts and then make that generic.
    virtual void 		checkStatus(bool force);
    static void 		timerCallback(OSObject *target, IOAudioDevice *device);

	Boolean				IsHeadphoneConnected (void);
	Boolean				IsLineOutConnected (void);
	Boolean				IsSpeakerConnected (void);
	UInt32				ParseDetectCollection ( void );

public:	
	void RealLineOutInterruptHandler (IOInterruptEventSource *source, int count);	//	[2878119]
	void RealHeadphoneInterruptHandler (IOInterruptEventSource *source, int count);
	void RealDallasInterruptHandler (IOInterruptEventSource *source, int count);

	virtual IOReturn performDeviceWake ();
	virtual IOReturn performDeviceSleep ();

protected:
	// activation functions
	IOReturn	AdjustControls (void);
	IOReturn	SetVolumeCoefficients (UInt32 left, UInt32 right);
	IOReturn	SetAmplifierMuteState (UInt32 ampID, Boolean muteState);
    IOReturn	SelectHeadphoneAmplifier( void );
    IOReturn	SelectLineOutAmplifier( void );
    IOReturn	SelectMasterMuteAmplifier( void );
    IOReturn	SelectSpeakerAmplifier( void );
    IOReturn	SetMixerState ( UInt32 mixerState );							//	[2855519]
	IOReturn	InitEQSerialMode (UInt32 mode, Boolean restoreOnNormal);
	Boolean		GpioRead (UInt8* gpioAddress);
	UInt8		GpioGetDDR (UInt8* gpioAddress);
	IOReturn 	AppleTexas2Audio::GetShadowRegisterInfo( UInt8 regAddr, UInt8 ** shadowPtr, UInt8* registerSize );
	IOReturn	Texas2_Initialize ();
	Boolean		IsCodecRESET( Boolean logMessage );
    void		Texas2_Reset ( void );
    void		Texas2_Reset_ASSERT ( void );
    void		Texas2_Reset_NEGATE ( void );
	IOReturn	Texas2_ReadRegister (UInt8 regAddr, UInt8* registerData);
	IOReturn	Texas2_WriteRegister (UInt8 regAddr, UInt8* registerData, UInt8 mode);
	void		GpioWrite (UInt8* gpioAddress, UInt8 data);
	void		SetBiquadInfoToUnityAllPass (void);
	void		SetUnityGainAllPass (void);
	void		ExcludeHPMuteRelease (UInt32 layout);
	IOReturn	SndHWSetDRC( DRCInfoPtr theDRCSettings );
	static IOReturn	DeviceInterruptServiceAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
	void		DeviceInterruptService (void);
	IOReturn	GetCustomEQCoefficients (UInt32 layoutID, UInt32 deviceID, UInt32 speakerID, EQPrefsElementPtr *filterSettings);
	UInt32		GetDeviceMatch (void);
	IOReturn	SndHWSetOutputBiquad( UInt32 streamID, UInt32 biquadRefNum, FourDotTwenty *biquadCoefficients );
	IOReturn	SndHWSetOutputBiquadGroup( UInt32 biquadFilterCount, FourDotTwenty *biquadCoefficients );
	IOReturn	SetOutputBiquadCoefficients (UInt32 streamID, UInt32 biquadRefNum, UInt8 *biquadCoefficients);
	void		SelectOutputAndLoadEQ ( void );
	IOReturn	SetActiveOutput (UInt32 output, Boolean touchBiquad);
	IOReturn	SetAnalogPowerDownMode( UInt8 mode );
	IOReturn	ToggleAnalogPowerDownWake( void );
	IORegistryEntry * FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value);
	IORegistryEntry * FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value);
	UInt32		GetDeviceID (void);
	Boolean		HasANDedReset (void);
	Boolean		HasInput (void);

    static bool interruptFilter (OSObject *, IOFilterInterruptEventSource *);
    static void dallasInterruptHandler (OSObject *owner, IOInterruptEventSource *source, int count);
	static bool	DallasDriverPublished (AppleTexas2Audio * appleTexas2Audio, void * refCon, IOService * newService);
	static void DallasInterruptHandlerTimer (OSObject *owner, IOTimerEventSource *sender);
	static void DisplaySpeakersNotFullyConnected (OSObject *owner, IOTimerEventSource *sender);
	static void headphoneInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count);
	static void lineOutInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count);

	void SetOutputSelectorCurrentSelection (void);

	// This provides access to the Texas2 registers:
	PPCI2CInterface *interface;

	UInt32		getI2CPort ();
	bool		openI2C ();
	void		closeI2C ();

	bool		findAndAttachI2C (IOService *provider);
	bool		detachFromI2C (IOService* /*provider*/);

	// *********************************
	// * I 2 S	DATA & Member Function *
	// *********************************
	void *soundConfigSpace;		   // address of sound config space
	void *ioBaseAddress;		   // base address of our I/O controller
	void *ioClockBaseAddress;	   // base address for the clock

	// Recalls which i2s interface we are attached to:
	UInt8 i2SInterfaceNumber;

	// starts and stops the clock count:
	void   KLSetRegister(void *klRegister, UInt32 value);
	UInt32	 KLGetRegister(void *klRegister);
	bool clockRun(bool start);

	inline UInt32 ReadWordLittleEndian(void *address, UInt32 offset);
	inline void WriteWordLittleEndian(void *address, UInt32 offset, UInt32 value);
	inline void I2SSetSerialFormatReg(UInt32 value);
	inline UInt32 I2SGetSerialFormatReg(void);
	inline void I2SSetDataWordSizeReg(UInt32 value);
	inline UInt32 I2SGetDataWordSizeReg(void);
	inline void Fcr1SetReg(UInt32 value);
	inline UInt32 Fcr1GetReg(void);
	inline void Fcr3SetReg(UInt32 value);
	inline UInt32 Fcr3GetReg(void);

	inline UInt32 I2SGetIntCtlReg();
	bool setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio);
	void setSerialFormatRegister(ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat);
	bool setHWSampleRate(UInt rate);
	UInt32 frameRate(UInt32 index);

	//	The following should probably be implemented in the base class
protected:    
	UInt32				mActiveOutput;		//	set to kSndHWOutputNone at init
	UInt32				mActiveInput;		//	set to kSndHWInputNone at init
	UInt32				gInputNoneAlias;	

	UInt8 *	getGPIOAddress (UInt32 gpioSelector);
	void	GpioWriteByte( UInt8* gpioAddress, UInt8 data );
	UInt8	GpioReadByte( UInt8* gpioAddress );

			// User Client calls
	virtual UInt8	readGPIO (UInt32 selector);
	virtual void	writeGPIO (UInt32 selector, UInt8 data);
	virtual Boolean	getGPIOActiveState (UInt32 gpioSelector);
};

#endif /* _APPLETEXAS2AUDIO_H */
