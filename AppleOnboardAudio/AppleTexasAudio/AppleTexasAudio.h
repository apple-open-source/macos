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
 * Interface definition for the TAS3001 audio Controller
 *
 * HISTORY
 *
 */

#ifndef _APPLESTEXASAUDIO_H
#define _APPLESTEXASAUDIO_H

#include "AudioHardwareCommon.h"
#include "AudioHardwareConstants.h"
#include "texas_hw.h"

#include <IOKit/i2c/PPCI2CInterface.h>
#include <IOKit/IORegistryEntry.h>

class IOAudioControl;
class IOInterruptEventSource;
class IORegistryEntry;

struct awacs_regmap_t;
struct IODBDMAChannelRegisters;
struct IODBDMADescriptor;


enum 
{
    kMaximumVolume = 140,
    kMinimumVolume = 0,
    kInitialVolume = 100
};

class IOAudioLevelControl;

typedef Boolean	GpioActiveState;
typedef UInt8* GpioPtr;




// Characteristic constants:
typedef enum TicksPerFrame {
	k64TicksPerFrame		= 64,			// 64 ticks per frame
	k32TicksPerFrame		= 32 			// 32 ticks per frame
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

enum {
    kOutMute = 0,
    kOutVolLeft = 1,
    kOutVolRight = 2,
	kMixerInVolLeft = 3,
	kMixerInVolRight = 4,
	kMicrophoneMute = 5,
    kNumControls
};

extern "C" {
	void	headphoneInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count);
	void	dallasInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count);
}

class AppleTexasAudio : public IOAudioDevice
{
    friend class AudioHardwareOutput;
    friend class AudioHardwareInput;
    OSDeclareDefaultStructors(AppleTexasAudio);

protected:
        //IOAudioControl (we assume simple mode)
    IOAudioToggleControl	*outMute;
    IOAudioLevelControl		*outVolLeft;
    IOAudioLevelControl		*outVolRight;

        //globals for the driver
    UInt32      			gVolLeft;
    UInt32      			gVolRight;
	SInt32					minVolume;
	SInt32					maxVolume;
    Boolean					gVolMuteActive;
    IOInterruptEventSource *headphoneIntEventSource;
    IOInterruptEventSource *dallasIntEventSource;
	Boolean					hasVideo;									// TRUE if this hardware has video out its headphone jack

    UInt8					sampleRateReg;
    UInt8					configurationReg;
    UInt16					analogVolumeReg;
//	bool					speakerMuted;
//	bool					headphonesMuted;
	Boolean					headphonesActive;
	Boolean					dallasSpeakersConnected;
    Boolean					mixerOutMuted;
    // Remebers the last value of the status register
    UInt8					lastStatus;

    DRCInfo					drc;										// dynamic range compression info
	UInt32					layoutID;									// The ID of the machine we're running on
    UInt32					outputSampleRate;							// output sample rate						(cached for HERMES)
	UInt32					outputSamplesPerInterrupt;					// number of output samples per interrupt	(cached for HERMES)
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
	UInt32					portSelect;									// for i2c part, from name registry
	UInt8					DEQAddress;									// Address for i2c TAS3001
	UInt32					DEQMixValue[kTumblerMaxSndSystem];			// Mix register value
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

        //information specific to the chip
    Boolean					duringInitialization;
    Boolean					gHasModemSound;
	Boolean					dontReleaseHPMute;

    // holds the current frame rate settings:
    ClockSource				clockSource;
    UInt32					mclkDivisor;
    UInt32					sclkDivisor;
    SoundFormat				serialFormat;

public:
            //Classical Unix funxtions
    virtual bool init(OSDictionary *properties);
    virtual void stop(IOService *provider);

    virtual void free();
    
    virtual IOService* probe(IOService *provider, SInt32*);

    virtual bool start(IOService *provider);

            //IOAudioDevice subclass
    void initHardware();
    
    virtual IOReturn configureDMAEngines(IOService *provider);
    void RealHeadphoneInterruptHandler (IOInterruptEventSource *source, int count);
    void RealDallasInterruptHandler (IOInterruptEventSource *source, int count);
    
    static IOReturn volumeLeftChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
    virtual IOReturn volumeLeftChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
    
    static IOReturn volumeRightChangeHandler(IOService *target, IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
    virtual IOReturn volumeRightChanged(IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue);
    
    static IOReturn outputMuteChangeHandler(IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue);
    virtual IOReturn outputMuteChanged(IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue);
    
    virtual IOReturn performPowerStateChange(IOAudioDevicePowerState oldPowerState,
                                                IOAudioDevicePowerState newPowerState,
                                                UInt32 *microsecondsUntilComplete);

	static IOReturn updateControlsAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
	static IOReturn performDeviceWakeAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
	static void	 performDeviceWakeThreadAction (AppleTexasAudio * audioEngine, void *arg);
    virtual IOReturn performDeviceWake ();
    virtual IOReturn performDeviceSleep ();

    virtual IOReturn setModemSound(bool state);
    virtual IOReturn callPlatformFunction( const OSSymbol *functionName , bool waitForFunction, void *param1, void *param2, void *param3, void *param4 );

protected:
            //Do the link to the IOAudioFamily 
    virtual bool createPorts(IOAudioEngine *driverDMAEngine);

            // These should be virtual method in a superclass. All "Get" method
           // could be common.
                
            //hardware registers manipulation

protected:    
            //activation functions
	IOReturn	SetVolumeCoefficients (UInt32 left, UInt32 right);
	UInt32		GetVolumeZeroIndexReference (void);
	IOReturn	SetAmplifierMuteState (UInt32 ampID, Boolean muteState);
	IOReturn	InitEQSerialMode (UInt32 mode, Boolean restoreOnNormal);
	Boolean		GpioRead (UInt8* gpioAddress);
	UInt8		GpioGetDDR (UInt8* gpioAddress);
	IOReturn 	TAS3001C_Initialize (UInt32 resetFlag);
	IOReturn 	TAS3001C_ReadRegister (UInt8 regAddr, UInt8* registerData);
	IOReturn 	TAS3001C_WriteRegister (UInt8 regAddr, UInt8* registerData, UInt8 mode);
	IOReturn 	TAS3001C_Reset (UInt32 resetFlag);
    void		GpioWrite (UInt8* gpioAddress, UInt8 data);
	void		SetBiquadInfoToUnityAllPass (void);
	void		SetUnityGainAllPass (void);
	void		ExcludeHPMuteRelease (UInt32 layout);
	IOReturn	__SndHWSetDRC( DRCInfoPtr theDRCSettings );
	void		DeviceIntService (void);
	IOReturn	GetCustomEQCoefficients (UInt32 layoutID, UInt32 deviceID, UInt32 speakerID, EQPrefsElementPtr *filterSettings);
	UInt32		GetDeviceMatch (void);
	IOReturn	__SndHWSetOutputBiquad( UInt32 streamID, UInt32 biquadRefNum, FourDotTwenty *biquadCoefficients );
	IOReturn	__SndHWSetOutputBiquadGroup( UInt32 biquadFilterCount, FourDotTwenty *biquadCoefficients );
	IOReturn	SetOutputBiquadCoefficients (UInt32 streamID, UInt32 biquadRefNum, UInt8 *biquadCoefficients);

            // control function
    bool		sndHWGetSystemMute (void);
    IOReturn	sndHWSetSystemMute (bool mutestate);
    bool		sndHWSetSystemVolume (UInt32 leftVolume, UInt32 rightVolume);
    IOReturn	sndHWSetSystemVolume (UInt32 value);
	IOReturn	SetActiveOutput (UInt32 system, UInt32 output, Boolean touchBiquad);
        
              //Identification
    UInt32		sndHWGetType (void);
    UInt32		sndHWGetManufacturer (void);

                //PRAM volume - need to move to common class
    UInt8 VolumeToPRAMValue( UInt32 volLeft ,  UInt32 volRight);
    void WritePRAMVol(  UInt32 volLeft ,  UInt32 volRight);

    // This provides access to the Texas registers:
    PPCI2CInterface *interface;

	UInt32		getI2CPort ();
	bool		openI2C ();
	void		closeI2C ();

	bool		findAndAttachI2C (IOService *provider);
	bool		detachFromI2C (IOService* /*provider*/);

    // *********************************
    // * I 2 S  DATA & Member Function *
    // *********************************
    void *soundConfigSpace;        // address of sound config space
    void *ioBaseAddress;           // base address of our I/O controller
    void *ioClockBaseAddress;	   // base address for the clock

    // Recalls which i2s interface we are attached to:
    UInt8 i2SInterfaceNumber;

    // starts and stops the clock count:
    void   KLSetRegister(void *klRegister, UInt32 value);
    UInt32   KLGetRegister(void *klRegister);
    bool clockRun(bool start);

	inline UInt32 ReadWordLittleEndian(void *address, UInt32 offset);
	inline void WriteWordLittleEndian(void *address, UInt32 offset, UInt32 value);
	inline void I2SSetSerialFormatReg(UInt32 value);
	inline void I2SSetDataWordSizeReg(UInt32 value);
	inline UInt32 I2SGetDataWordSizeReg(void);

	inline UInt32 I2SGetIntCtlReg();
	bool setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio);
	void setSerialFormatRegister(ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat);
	bool setHWSampleRate(UInt rate);
	UInt32 frameRate(UInt32 index);

	IORegistryEntry * FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value);
	IORegistryEntry * FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value);
	UInt32			GetDeviceID (void);
	Boolean			HasInput (void);

};

#endif /* _APPLETEXASAUDIO_H */
