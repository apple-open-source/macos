/*
 *	AppleTemplateAudio.h
 *	AppleOnboardAudio
 *
 *	Created by nthompso on Thu Jul 05 2001.
 *	Copyright (c) 2001 Apple. All rights reserved.
 *
 */

/*
 * Contains the interface definition for the DAC 3550A audio Controller
 * This is the audio controller used in the original iBook.
 */
 
 
#ifndef _APPLEDACAAUDIO_H
#define _APPLEDACAAUDIO_H

#include <IOKit/i2c/PPCI2CInterface.h>

#include "AppleDallasDriver.h"
#include "AppleOnboardAudio.h"
#include "texas_hw.h"
#include "AppleDBDMAAudioDMAEngine.h"

class IOInterruptEventSource;
class IORegistryEntry;

struct awacs_regmap_t;
struct IODBDMAChannelRegisters;
struct IODBDMADescriptor;

#define kInsertionDelayNanos		4000000000ULL
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

// declare a class for our driver.	This is based from AppleOnboardAudio
class AppleTexasAudio : public AppleOnboardAudio
{
	OSDeclareDefaultStructors(AppleTexasAudio);

	friend class AudioHardwareOutput;
	friend class AudioHardwareInput;
	friend class AudioHardwareMux;

protected:
	SInt32					minVolume;
	SInt32					maxVolume;
	Boolean					gVolMuteActive;
	Boolean					mCanPollStatus;
	IOInterruptEventSource *headphoneIntEventSource;
	IOInterruptEventSource *dallasIntEventSource;
	Boolean					hasVideo;									// TRUE if this hardware has video out its headphone jack
	Boolean					headphonesConnected;
	Boolean					dallasSpeakersConnected;
	DRCInfo					drc;										// dynamic range compression info
	UInt32					layoutID;									// The ID of the machine we're running on
	UInt32					familyID;									// The ID of the speakers that are plugged in (required for rom verification)
	UInt32					speakerID;									// The ID of the speakers that are plugged in
	GpioPtr					hwResetGpio;
	GpioPtr					hdpnMuteGpio;
	GpioPtr					ampMuteGpio;
	UInt8*					headphoneExtIntGpio;
	UInt8*					dallasExtIntGpio;
	GpioActiveState			hwResetActiveState;							//	indicates asserted state (i.e. '0' or '1')
	GpioActiveState			hdpnActiveState;							//	indicates asserted state (i.e. '0' or '1')
	GpioActiveState			ampActiveState;								//	indicates asserted state (i.e. '0' or '1')
	GpioActiveState			headphoneInsertedActiveState;
	GpioActiveState			dallasInsertedActiveState;
	UInt8					DEQAddress;									// Address for i2c TAS3001
	TAS3001C_ShadowReg		shadowRegs;									// write through shadow registers for TAS3001C
	Boolean					semaphores;
	UInt32					deviceID;
	double					biquadGroupInfo[kNumberOfBiquadCoefficients];
	IOService				*ourProvider;
	IODeviceMemory			*hdpnMuteRegMem;							// Have to free this in free()
	IODeviceMemory			*ampMuteRegMem;								// Have to free this in free()
	IODeviceMemory			*hwResetRegMem;								// Have to free this in free()
	IODeviceMemory			*headphoneExtIntGpioMem;					// Have to free this in free()
	IODeviceMemory			*dallasExtIntGpioMem;						// Have to free this in free()
	UInt32					i2sSerialFormat;
	IOService				*headphoneIntProvider;
	IOService				*dallasIntProvider;
	const OSSymbol			*gAppleAudioVideoJackStateKey;
	AppleDallasDriver *		dallasDriver;
	IONotifier *			dallasDriverNotifier;
    IOFilterInterruptEventSource* interruptEventSource;					// interrupt event source
	IOTimerEventSource *	dallasHandlerTimer;
	IOTimerEventSource *	notifierHandlerTimer;
	Boolean					doneWaiting;
	UInt64					savedNanos; 
	Boolean					speakerConnectFailed;
	IOAudioDevicePowerState previousPowerState;
	Boolean					useMasterVolumeControl;
	UInt32					lastLeftVol;
	UInt32					lastRightVol;
	Boolean					dallasSpeakersProbed;						// So we only probe the speakers once when they are plugged in

	// information specific to the chip
	Boolean					gModemSoundActive;
	Boolean					dontReleaseHPMute;

	// holds the current frame rate settings:
	ClockSource				clockSource;
	UInt32					mclkDivisor;
	UInt32					sclkDivisor;
	SoundFormat				serialFormat;

	// Hardware register manipulation
	virtual void		sndHWInitialize (IOService *provider) ;
	virtual void		sndHWPostDMAEngineInit (IOService *provider);
	virtual UInt32		sndHWGetInSenseBits (void) ;
	virtual UInt32		sndHWGetRegister (UInt32 regNum) ;
	virtual IOReturn	sndHWSetRegister (UInt32 regNum, UInt32 value) ;

	// IO activation functions
	virtual	 UInt32		sndHWGetActiveOutputExclusive (void);
	virtual	 IOReturn	sndHWSetActiveOutputExclusive (UInt32 outputPort);
	virtual	 UInt32		sndHWGetActiveInputExclusive (void);
	virtual	 IOReturn	sndHWSetActiveInputExclusive (UInt32 input);

	// control function
	virtual	 bool		sndHWGetSystemMute (void);
	virtual	 IOReturn	sndHWSetSystemMute (bool mutestate);
	virtual	 bool		sndHWSetSystemVolume (UInt32 leftVolume, UInt32 rightVolume);
	virtual	 IOReturn	sndHWSetSystemVolume (UInt32 value);
	virtual	 IOReturn	sndHWSetPlayThrough (bool playthroughstate);
	virtual	 IOReturn	sndHWSetSystemInputGain (UInt32 leftGain, UInt32 rightGain) ;

	// Identification
	virtual UInt32		sndHWGetType (void);
	virtual UInt32		sndHWGetManufacturer (void);

    virtual void setDeviceDetectionActive (void);
    virtual void setDeviceDetectionInActive (void);      

public:
	// Classical Unix driver functions
	virtual bool		init (OSDictionary *properties);
	virtual void		free ();

	virtual IOService*	probe (IOService *provider, SInt32*);

	// IOAudioDevice subclass
	virtual bool		initHardware (IOService *provider);

	// Power Management
	virtual	 IOReturn	sndHWSetPowerState (IOAudioDevicePowerState theState);
	
	// 
	virtual	 UInt32		sndHWGetConnectedDevices (void);
	virtual	 UInt32		sndHWGetProgOutput ();
	virtual	 IOReturn	sndHWSetProgOutput (UInt32 outputBits);

private:
	// These will probably change when we have a general method
	// to verify the Detects.  Wait til we figure out how to do 
	// this with interrupts and then make that generic.
	virtual void		checkStatus (bool force);
	static void			timerCallback (OSObject *target, IOAudioDevice *device);

	Boolean				IsHeadphoneConnected (void);
	Boolean				IsSpeakerConnected (void);

public:	
	static void headphoneInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count);
	void RealHeadphoneInterruptHandler (IOInterruptEventSource *source, int count);
	void RealDallasInterruptHandler (IOInterruptEventSource *source, int count);
    static bool interruptFilter (OSObject *, IOFilterInterruptEventSource *);
    static void dallasInterruptHandler (OSObject *owner, IOInterruptEventSource *source, int count);
	static void DallasInterruptHandlerTimer (OSObject *owner, IOTimerEventSource *sender);
	static bool	DallasDriverPublished (AppleTexasAudio * appleTexasAudio, void * refCon, IOService * newService);
	static void DisplaySpeakersNotFullyConnected (OSObject *owner, IOTimerEventSource *sender);
	static IOReturn DeviceInterruptServiceAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);


	virtual IOReturn performDeviceIdleWake ();
	virtual IOReturn performDeviceIdleSleep ();
	virtual IOReturn performDeviceWake ();
	virtual IOReturn performDeviceSleep ();

	virtual IOReturn setModemSound(bool state);
	// virtual IOReturn callPlatformFunction( const OSSymbol *functionName , bool waitForFunction, void *param1, void *param2, void *param3, void *param4 );

protected:
	// activation functions
	IOReturn	AdjustControls (void);
	IOReturn	SetVolumeCoefficients (UInt32 left, UInt32 right);
	IOReturn	SetAmplifierMuteState (UInt32 ampID, Boolean muteState);
	IOReturn	InitEQSerialMode (UInt32 mode, Boolean restoreOnNormal);
	Boolean		GpioRead (UInt8* gpioAddress);
	UInt8		GpioGetDDR (UInt8* gpioAddress);
	IOReturn	TAS3001C_Initialize (UInt32 resetFlag);
	IOReturn	TAS3001C_ReadRegister (UInt8 regAddr, UInt8* registerData);
	IOReturn	TAS3001C_WriteRegister (UInt8 regAddr, UInt8* registerData, UInt8 mode);
	IOReturn	TAS3001C_Reset (UInt32 resetFlag);
	void		GpioWrite (UInt8* gpioAddress, UInt8 data);
	void		SetBiquadInfoToUnityAllPass (void);
	void		SetUnityGainAllPass (void);
	void		ExcludeHPMuteRelease (UInt32 layout);
	IOReturn	SndHWSetDRC( DRCInfoPtr theDRCSettings );
	void		DeviceInterruptService (void);
	IOReturn	GetCustomEQCoefficients (UInt32 layoutID, UInt32 deviceID, UInt32 speakerID, EQPrefsElementPtr *filterSettings);
	UInt32		GetDeviceMatch (void);
	IOReturn	SndHWSetOutputBiquad( UInt32 streamID, UInt32 biquadRefNum, FourDotTwenty *biquadCoefficients );
	IOReturn	SndHWSetOutputBiquadGroup( UInt32 biquadFilterCount, FourDotTwenty *biquadCoefficients );
	IOReturn	SetOutputBiquadCoefficients (UInt32 streamID, UInt32 biquadRefNum, UInt8 *biquadCoefficients);
	IOReturn	SetActiveOutput (UInt32 output, Boolean touchBiquad);

	IORegistryEntry * FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value);
	IORegistryEntry * FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value);
	UInt32			GetDeviceID (void);
	Boolean			HasInput (void);

	// This provides access to the Texas registers:
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

	inline UInt32	ReadWordLittleEndian(void *address, UInt32 offset);
	inline void		WriteWordLittleEndian(void *address, UInt32 offset, UInt32 value);

	inline void		I2SSetSerialFormatReg(UInt32 value);
	inline UInt32	I2SGetSerialFormatReg(void);
	inline void		I2SSetDataWordSizeReg(UInt32 value);
	inline UInt32	I2SGetDataWordSizeReg(void);

	inline void		I2S1SetSerialFormatReg(UInt32 value);
	inline UInt32	I2S1GetSerialFormatReg(void);
	inline void		I2S1SetDataWordSizeReg(UInt32 value);
	inline UInt32	I2S1GetDataWordSizeReg(void);

	inline UInt32	FCR1GetReg(void);
	inline void		Fcr1SetReg(UInt32 value);
	inline UInt32	FCR3GetReg(void);
	inline void		Fcr3SetReg(UInt32 value);

	inline UInt32 	I2SGetIntCtlReg();
	inline UInt32 	I2S1GetIntCtlReg();
	
	bool			setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio);
	void			setSerialFormatRegister(ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat);
	bool			setHWSampleRate(UInt rate);
	UInt32			frameRate(UInt32 index);

	UInt8 *			getGPIOAddress (UInt32 gpioSelector);
	void			GpioWriteByte( UInt8* gpioAddress, UInt8 data );
	UInt8			GpioReadByte( UInt8* gpioAddress );

	// User Client calls
	virtual UInt8		readGPIO (UInt32 selector);
	virtual void		writeGPIO (UInt32 selector, UInt8 data);
	virtual Boolean		getGPIOActiveState (UInt32 gpioSelector);
	virtual void		setGPIOActiveState ( UInt32 selector, UInt8 gpioActiveState );
	virtual Boolean		checkGpioAvailable ( UInt32 selector );
	virtual IOReturn	readHWReg32 ( UInt32 selector, UInt32 * registerData );
	virtual IOReturn	writeHWReg32 ( UInt32 selector, UInt32 registerData );
	virtual IOReturn	readCodecReg ( UInt32 selector, void * registerData,  UInt32 * registerDataSize );
	virtual IOReturn	writeCodecReg ( UInt32 selector, void * registerData );
	virtual IOReturn	readSpkrID ( UInt32 selector, UInt32 * speakerIDPtr );
	virtual IOReturn	getCodecRegSize ( UInt32 selector, UInt32 * codecRegSizePtr );
	virtual	IOReturn	getVolumePRAM ( UInt32 * pramDataPtr );
	virtual IOReturn	getDmaState ( UInt32 * dmaStatePtr );
	virtual IOReturn	getStreamFormat ( IOAudioStreamFormat * streamFormatPtr );
	virtual IOReturn	readPowerState ( UInt32 selector, IOAudioDevicePowerState * powerState );
	virtual IOReturn	setPowerState ( UInt32 selector, IOAudioDevicePowerState powerState );
	virtual IOReturn	setBiquadCoefficients ( UInt32 selector, void * biquadCoefficients, UInt32 coefficientSize );
	virtual IOReturn	getBiquadInformation ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr );
	virtual IOReturn	getProcessingParameters ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr );
	virtual IOReturn	setProcessingParameters ( UInt32 scalarArg1, void * inStructPtr, UInt32 inStructSize );
	virtual	IOReturn	invokeInternalFunction ( UInt32 functionSelector, void * inData );

};

#endif
