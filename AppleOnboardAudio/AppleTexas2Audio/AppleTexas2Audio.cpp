/*
 *  AppleTexas2Audio.cpp (definition)
 *  Project : AppleOnboardAudio
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 * 
 *  The contents of this file constitute Original Code as defined in and
 *  are subject to the Apple Public Source License Version 1.1 (the
 *  "License").  You may not use this file except in compliance with the
 *  License.  Please obtain a copy of the License at
 *  http://www.apple.com/publicsource and read it before using this file.
 * 
 *  This Original Code and all software distributed under the License are
 *  distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 *  License for the specific language governing rights and limitations
 *  under the License.
 * 
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  Hardware independent (relatively) code for the Texas Insruments Texas2 Codec
 *  NEW-WORLD MACHINE ONLY!!!
 */

#include "AppleTexas2Audio.h"
#include "Texas2_hw.h"

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOCommandGate.h>
#include <UserNotification/KUNCUserNotifications.h>
#include <IOKit/IOTimerEventSource.h>

#include "AppleTexas2EQPrefs.cpp"

#define durationMillisecond 1000	// number of microseconds in a millisecond

#define super AppleOnboardAudio

OSDefineMetaClassAndStructors(AppleTexas2Audio, AppleOnboardAudio)

EQPrefsPtr		gEQPrefs = &theEQPrefs;
extern uid_t	console_user;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#pragma mark +UNIX LIKE FUNCTIONS

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// init, free, probe are the "Classical Unix driver functions" that you'll like as not find
// in other device drivers.	 Note that there are no start and stop methods.	 The code for start 
// effectively moves to the sndHWInitialize routine.  Also note that the initHardware method 
// effectively does very little other than calling the inherited method, which in turn calls
// sndHWInitialize, so all of the init code should be in the sndHWInitialize routine.

// ::init()
// call into superclass and initialize.
bool AppleTexas2Audio::init(OSDictionary *properties)
{
	debugIOLog("+ AppleTexas2Audio::init\n");
	if (!super::init(properties))
		return false;

	gVolLeft = 0;
	gVolRight = 0;
	gVolMuteActive = false;
	gModemSoundActive = false;
	gInputNoneAlias = kSndHWInputNone;	//	default assumption is that no unused input is available to alias kSndHWInputNone

	debugIOLog("- AppleTexas2Audio::init\n");
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ::free
// call inherited free
void AppleTexas2Audio::free()
{
	IOWorkLoop				*workLoop;

	debugIOLog("+ AppleTexas2Audio::free\n");

	CLEAN_RELEASE(hdpnMuteRegMem);
	CLEAN_RELEASE(ampMuteRegMem);
	CLEAN_RELEASE(hwResetRegMem);
	CLEAN_RELEASE(headphoneExtIntGpioMem);
	CLEAN_RELEASE(dallasExtIntGpioMem);
 
	workLoop = getWorkLoop();
	if (NULL != workLoop) {
		if (NULL != headphoneIntEventSource && NULL != headphoneIntProvider)
			workLoop->removeEventSource (headphoneIntEventSource);
		if (NULL != dallasIntEventSource && NULL != dallasIntProvider)
			workLoop->removeEventSource (dallasIntEventSource);
		if (NULL != dallasHandlerTimer)
			workLoop->removeEventSource (dallasHandlerTimer);
		if (NULL != notifierHandlerTimer)
			workLoop->removeEventSource (notifierHandlerTimer);
	}

	super::free();
	debugIOLog("- AppleTexas2Audio::free\n");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ::probe
// called at load time, to see if this driver really does match with a device.	In our
// case we check the registry to ensure we are loading on the appropriate hardware.
IOService* AppleTexas2Audio::probe(IOService *provider, SInt32 *score)
{
	// Finds the possible candidate for sound, to be used in
	// reading the caracteristics of this hardware:
	IORegistryEntry		*sound = 0;
	IOService			*result = 0;
	
	debugIOLog("+ AppleTexas2Audio::probe\n");

	super::probe(provider, score);
	*score = kIODefaultProbeScore;
	sound = provider->childFromPath("sound", gIODTPlane);
	//we are on a new world : the registry is assumed to be fixed
	if(sound) {
		OSData *tmpData;

		tmpData = OSDynamicCast(OSData, sound->getProperty(kModelPropName));
		if(tmpData) {
			if(tmpData->isEqualTo(kTexas2ModelName, sizeof(kTexas2ModelName) -1)) {
				*score = *score+1;
				debugIOLog("++ AppleTexas2Audio::probe increasing score\n");
				result = (IOService*)this;
			} else {
				debugIOLog ("++ AppleTexas2Audio::probe, didn't find what we were looking for\n");
			}
		}
		if (!result) {
			sound->release ();
		}
	}

	debugIOLog("- AppleTexas2Audio::probe\n");
	return (result);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/* Don't have a stop in this model, so where does this code go?
void AppleTexas2Audio::stop(IOService *provider)
{
	IOWorkLoop				*workLoop;

	debugIOLog("+ AppleTexas2Audio::stop\n");

	publishResource("setModemSound", NULL);

	if (headphoneIntEventSource) {
		workLoop = getWorkLoop();
		if (workLoop) {
			if (NULL != headphoneIntEventSource && NULL != headphoneIntProvider);
				workLoop->removeEventSource (headphoneIntEventSource);
			if (NULL != dallasIntEventSource && NULL != dallasIntProvider);
				workLoop->removeEventSource (dallasIntEventSource);
		}
	}

	super::stop(provider);

	debugIOLog("- AppleTexas2Audio::stop\n");
}
*/

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ::initHardware
// Don't do a whole lot in here, but do call the inherited inithardware method.
// in turn this is going to call sndHWInitialize to perform initialization.	 All
// of the code to initialize and start the device needs to be in that routine, this 
// is kept as simple as possible.
bool AppleTexas2Audio::initHardware(IOService *provider)
{
	bool myreturn = true;

	DEBUG_IOLOG("+ AppleTexas2Audio::initHardware\n");
	
	// calling the superclasses initHarware will indirectly call our
	// sndHWInitialize() method.  So we don't need to do a whole lot 
	// in this function.
	super::initHardware(provider);
	
	DEBUG_IOLOG("- AppleTexas2Audio::initHardware\n");
	return myreturn;
}

// --------------------------------------------------------------------------
// Method: timerCallback
//
// Purpose:
//		  This is a static method that gets called from a timer task at regular intervals
//		  Generally we do not want to do a lot of work here, we simply turn around and call
//		  the appropriate method to perform out periodic tasks.

void AppleTexas2Audio::timerCallback(OSObject *target, IOAudioDevice *device)
{
// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("+ AppleTexas2Audio::timerCallback\n");
	AppleTexas2Audio *templateDriver;
	
	templateDriver = OSDynamicCast(AppleTexas2Audio, target);

	if (templateDriver) {
		templateDriver->checkStatus(false);
	}
// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("- AppleTexas2Audio::timerCallback\n");
	return;
}



// --------------------------------------------------------------------------
// Method: checkStatus
//
// Purpose:
//		 poll the detects, note this should prolly be done with interrupts rather
//		 than by polling if interrupts are supported

void AppleTexas2Audio::checkStatus(
	bool force)
{
// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("+ AppleTexas2Audio::checkStatus\n");

// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("- AppleTexas2Audio::checkStatus\n");


}

/*************************** sndHWXXXX functions******************************/
/*	 These functions should be common to all Apple hardware drivers and be	 */
/*	 declared as virtual in the superclass. This is the only place where we	 */
/*	 should manipulate the hardware. Order is given by the UI policy system	 */
/*	 The set of functions should be enough to implement the policy			 */
/*****************************************************************************/


#pragma mark +HARDWARE REGISTER MANIPULATION
/************************** Hardware Register Manipulation ********************/
// Hardware specific functions : These are all virtual functions and we have to 
// implement these in the driver class

// --------------------------------------------------------------------------
// ::sndHWInitialize
// hardware specific initialization needs to be in here, together with the code
// required to start audio on the device.
void	AppleTexas2Audio::sndHWInitialize(IOService *provider)
{
	IOReturn				err;
	IORegistryEntry			*i2s;
	IORegistryEntry			*macio;
	IORegistryEntry			*gpio;
	IORegistryEntry			*i2c;
	IORegistryEntry			*deq;
	IORegistryEntry			*intSource;
    IORegistryEntry			*headphoneMute;
    IORegistryEntry			*ampMute;
	OSData					*tmpData;
	IOMemoryMap				*map;
	UInt32					*hdpnMuteGpioAddr;
	UInt32					*ampMuteGpioAddr;
	UInt32					*headphoneExtIntGpioAddr;
	UInt32					*dallasExtIntGpioAddr;
	UInt32					*i2cAddrPtr;
	UInt32					*tmpPtr;
	UInt32					loopCnt;
    UInt32					myFrameRate;
	UInt8					data[kTexas2BIQwidth];						// space for biggest register size
	UInt8					curValue;

	debugIOLog("+ AppleTexas2Audio::sndHWInitialize\n");

	i2s		= NULL;		//	audio sample transport layer
	macio	= NULL;		//	parent entry for gpio & I2C
	gpio	= NULL;		//	detects & mutes
	i2c		= NULL;		//	audio control transport layer
	savedNanos = 0;

    // Sets the frame rate:
	myFrameRate = frameRate(0);
	FailIf (!provider, Exit);
	ourProvider = provider;
    FailIf (!findAndAttachI2C (provider), Exit);

	i2s = ourProvider->getParentEntry (gIODTPlane);
	FailIf (!i2s, Exit);
	macio = i2s->getParentEntry (gIODTPlane);
	FailIf (!macio, Exit);
	gpio = macio->childFromPath (kGPIODTEntry, gIODTPlane);
	FailIf (!gpio, Exit);
	i2c = macio->childFromPath (kI2CDTEntry, gIODTPlane);
	setProperty (kSpeakerConnectError, speakerConnectFailed);

	//	Determine which systems to exclude from the default behavior of releasing the headphone
	//	mute after 200 milliseconds delay [2660341].  Typically this is done for any non-portable
	//	CPU.  Portable CPUs will achieve better battery life by leaving the mute asserted.  Desktop
	//	CPUs have a different amplifier configuration and only want the amplifier quiet during a
	//	detect transition.
	tmpData = OSDynamicCast (OSData, macio->getProperty (kDeviceIDPropName));
	FailIf (!tmpData, Exit);
	deviceID = (UInt32)tmpData->getBytesNoCopy ();
	ExcludeHPMuteRelease (deviceID);

	// get the physical address of the i2c cell that the sound chip (Digital EQ, "deq") is connected to.
    deq = i2c->childFromPath (kDigitalEQDTEntry, gIODTPlane);
	FailIf (!deq, Exit);
	tmpData = OSDynamicCast (OSData, deq->getProperty (kI2CAddress));
	deq->release ();
	FailIf (!tmpData, Exit);
	i2cAddrPtr = (UInt32*)tmpData->getBytesNoCopy ();
	DEQAddress = *i2cAddrPtr;
	
	//
	//	Conventional I2C address nomenclature concatenates a 7 bit address to a 1 bit read/*write bit
	//	 ___ ___ ___ ___ ___ ___ ___ ___
	//	|   |   |   |   |   |   |   |   |
	//	| 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
	//	|___|___|___|___|___|___|___|___|
	//	  |   |   |   |   |   |   |   |____	1 = Read, 0 = Write
	//	  |___|___|___|___|___|___|________	7 bit address
	//
	//	The conventional method of referring to the I2C address is to read the address in
	//	place without any shifting of the address to compensate for the Read/*Write bit.
	//	The I2C driver does not use this standardized method of referring to the address
	//	and instead, requires shifting the address field right 1 bit so that the Read/*Write
	//	bit is not passed to the I2C driver as part of the address field.
	//
	DEQAddress = DEQAddress >> 1;

	// get the physical address of the gpio pin for setting the headphone mute
	headphoneMute = FindEntryByProperty (gpio, kAudioGPIO, kHeadphoneAmpEntry);
	FailIf (!headphoneMute, Exit);
	tmpData = OSDynamicCast (OSData, headphoneMute->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	hdpnMuteGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
	tmpData = OSDynamicCast (OSData, headphoneMute->getProperty (kAudioGPIOActiveState));
	if (tmpData) {
		tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
		hdpnActiveState = *tmpPtr;
		debug2IOLog ("hdpnActiveState = 0x%X\n", hdpnActiveState);
	}

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	if (hdpnMuteGpioAddr) {
		debug2IOLog ("hdpnMuteGpioAddr = 0x%X\n", (unsigned int)hdpnMuteGpioAddr);
		hdpnMuteRegMem = IODeviceMemory::withRange (*hdpnMuteGpioAddr, sizeof (UInt8));
		map = hdpnMuteRegMem->map (0);
		hdpnMuteGpio = (UInt8*)map->getVirtualAddress ();
	}

	intSource = 0;

	//	Get the interrupt provider for the Dallas speaker insertion interrupt.
	//	This can be located by searching for a 'one-wire-bus' property with a
	//	value of 'speaker-id'.  The ExtInt-gpio that is the parent of this
	//	property value is the interrupt provider for the detect.  When a Dallas 
	//	interface is detected by the perferred method, the active state for the 
	//	GPIO is implied as active '1' due to the open collector nature of the 
	//	Dallas interface.  Support the 'audio-gpio' property with a value of 'speaker-detect'.
	intSource = FindEntryByProperty (gpio, kOneWireBusPropName, kSpeakerIDPropValue);
	if (NULL != intSource) {
		debugIOLog ("FOUND: Dallas interrupt by one-wire-bus method\n");
		
		dallasInsertedActiveState = 1;
		dallasIntProvider = OSDynamicCast (IOService, intSource);
		FailIf (!dallasIntProvider, Exit);
		// get the physical address of the pin for detecting the dallas speaker insertion/removal
		tmpData = OSDynamicCast (OSData, intSource->getProperty (kAAPLAddress));
		FailIf (!tmpData, Exit);
		dallasExtIntGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();

		// take the hard coded memory address that's in the boot rom, and convert it a virtual address
		dallasExtIntGpioMem = IODeviceMemory::withRange (*dallasExtIntGpioAddr, sizeof (UInt8));
		debug2IOLog ("dallasExtIntGpioMem = 0x%X\n", (unsigned int)dallasExtIntGpioMem);
		map = dallasExtIntGpioMem->map (0);
		dallasExtIntGpio = (UInt8*)map->getVirtualAddress ();

		//	Setup Dallas Speaker ID detect for dual edge interrupt
		curValue = *dallasExtIntGpio;
		curValue = curValue | (1 << 7);
		*dallasExtIntGpio = curValue;
	} else {
		debugIOLog ("!!!!Couldn't find a dallas speaker interrupt source!!!!\n");
	}

	intSource = 0;

	// get the interrupt provider for the headphone insertion interrupt
	intSource = FindEntryByProperty (gpio, kAudioGPIO, kHeadphoneDetectInt);
	if (!intSource)
		intSource = FindEntryByProperty (gpio, kCompatible, kKWHeadphoneDetectInt);

	FailIf (!intSource, Exit);
	headphoneIntProvider = OSDynamicCast (IOService, intSource);
	FailIf (!headphoneIntProvider, Exit);

	// We only want to publish the jack state if this hardware has video on its headphone connector
	tmpData = OSDynamicCast (OSData, intSource->getProperty (kVideoPropertyEntry));
	debug2IOLog ("kVideoPropertyEntry = %p\n", tmpData);
	if (NULL != tmpData) {
		hasVideo = TRUE;
		gAppleAudioVideoJackStateKey = OSSymbol::withCStringNoCopy ("AppleAudioVideoJackState");
		debugIOLog ("has video in headphone\n");
	}

	// get the active state of the headphone inserted pin
	// This should really be gotten from the sound-objects property, but we're not parsing that yet.
	tmpData = OSDynamicCast (OSData, intSource->getProperty (kAudioGPIOActiveState));
	if (NULL == tmpData) {
		headphoneInsertedActiveState = 1;
	} else {
		tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
		headphoneInsertedActiveState = *tmpPtr;
	}
	debug2IOLog ("headphoneInsertedActiveState = 0x%X\n", headphoneInsertedActiveState);

	// get the physical address of the pin for detecting the headphone insertion/removal
	tmpData = OSDynamicCast (OSData, intSource->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	headphoneExtIntGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
	debug2IOLog ("headphoneExtIntGpioAddr = 0x%X\n", (unsigned int)headphoneExtIntGpioAddr);

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	headphoneExtIntGpioMem = IODeviceMemory::withRange (*headphoneExtIntGpioAddr, sizeof (UInt8));
	map = headphoneExtIntGpioMem->map (0);
	headphoneExtIntGpio = (UInt8*)map->getVirtualAddress ();
	
	curValue = *headphoneExtIntGpio;
	curValue = curValue | (1 << 7);
	*headphoneExtIntGpio = curValue;

	// get the physical address of the gpio pin for setting the amplifier mute
	ampMute = FindEntryByProperty (gpio, kAudioGPIO, kAmpEntry);
	FailIf (!ampMute, Exit);
	tmpData = OSDynamicCast (OSData, ampMute->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	ampMuteGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
	debug2IOLog ("ampMuteGpioAddr = 0x%X\n", (unsigned int)ampMuteGpioAddr);
	tmpData = OSDynamicCast (OSData, ampMute->getProperty (kAudioGPIOActiveState));
	tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
	ampActiveState = *tmpPtr;
	debug2IOLog ("ampActiveState = 0x%X\n", ampActiveState);

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	ampMuteRegMem = IODeviceMemory::withRange (*ampMuteGpioAddr, sizeof (UInt8));
	map = ampMuteRegMem->map (0);
	ampMuteGpio = (UInt8*)map->getVirtualAddress ();

	layoutID = GetDeviceID ();
	debug2IOLog ("layoutID = %ld\n", layoutID);

	i2sSerialFormat = 0x41190000;

	drc.compressionRatioNumerator	= kDrcRatioNumerator;
	drc.compressionRatioDenominator	= kDrcRationDenominator;
	drc.threshold					= kDrcThresholdMax;
	drc.maximumVolume				= kDefaultMaximumVolume;
	drc.enable						= false;

	//	Initialize the Texas2 as follows:
	//		Mode:					normal
	//		SCLK:					64 fs
	//		input serial mode:		i2s
	//		output serial mode:		i2s
	//		serial word length:		16 bits
	//		Dynamic range control:	disabled
	//		Volume (left & right):	muted
	//		Treble / Bass:			unity
	//		Biquad filters:			unity
	//	Initialize the Texas2 registers the same as the Texas2 with the following additions:
	//		AnalogPowerDown:		normal
	//		
	data[0] = ( kNormalLoad << kFL ) | ( k64fs << kSC ) | TAS_I2S_MODE | ( TAS_WORD_LENGTH << kW0 );
	Texas2_WriteRegister( kTexas2MainCtrl1Reg, data, kUPDATE_SHADOW );	//	default to normal load mode, 16 bit I2S

	data[DRC_AboveThreshold]	= kDisableDRC;
	data[DRC_BelowThreshold]	= kDRCBelowThreshold1to1;
	data[DRC_Threshold]			= kDRCUnityThreshold;
	data[DRC_Integration]		= kDRCIntegrationThreshold;
	data[DRC_Attack]			= kDRCAttachThreshold;
	data[DRC_Decay]				= kDRCDecayThreshold;
	Texas2_WriteRegister( kTexas2DynamicRangeCtrlReg, data, kUPDATE_ALL );

	for( loopCnt = 0; loopCnt < kTexas2VOLwidth; loopCnt++ )				//	init to volume = muted
	    data[loopCnt] = 0;
	Texas2_WriteRegister( kTexas2VolumeCtrlReg, data, kUPDATE_SHADOW );

	data[0] = 0x72;									//	treble = bass = unity 0.0 dB
	Texas2_WriteRegister( kTexas2TrebleCtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2BassCtrlReg, data, kUPDATE_SHADOW );

	data[0] = 0x10;								//	output mixer output channel to unity = 0.0 dB
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;								//	output mixer call progress channel to mute = -70.0 dB
	data[4] = 0x00;
	data[5] = 0x00;
	data[6] = 0x00;								//	output mixer analog playthrough channel to mute = -70.0 dB
	data[7] = 0x00;
	data[8] = 0x00;
	Texas2_WriteRegister( kTexas2MixerLeftGainReg, data, kUPDATE_SHADOW );	//	initialize left channel
	Texas2_WriteRegister( kTexas2MixerRightGainReg, data, kUPDATE_SHADOW );	//	initialize right channel

	for( loopCnt = 1; loopCnt < kTexas2BIQwidth; loopCnt++ )				//	all biquads to unity gain all pass mode
		data[loopCnt] = 0x00;
	data[0] = 0x10;

	Texas2_WriteRegister( kTexas2LeftBiquad0CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2LeftBiquad1CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2LeftBiquad2CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2LeftBiquad3CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2LeftBiquad4CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2LeftBiquad5CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2LeftBiquad6CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2LeftLoudnessBiquadReg, data, kUPDATE_SHADOW );

	Texas2_WriteRegister( kTexas2RightBiquad0CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2RightBiquad1CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2RightBiquad2CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2RightBiquad3CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2RightBiquad4CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2RightBiquad5CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2RightBiquad6CtrlReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2RightLoudnessBiquadReg, data, kUPDATE_SHADOW );
	
	data[0] = 0x00;	//	loudness gain to mute
	Texas2_WriteRegister( kTexas2LeftLoudnessBiquadGainReg, data, kUPDATE_SHADOW );
	Texas2_WriteRegister( kTexas2RightLoudnessBiquadGainReg, data, kUPDATE_SHADOW );
	
	data[0] = ( kADMNormal << kADM ) | ( kDeEmphasisOFF << kADM ) | ( kPowerDownAnalog << kAPD );
	Texas2_WriteRegister( kTexas2AnalogControlReg, data, kUPDATE_SHADOW );
	
	data[0] = ( kAllPassFilter << kAP ) | ( kNormalBassTreble << kDL );
	Texas2_WriteRegister( kTexas2MainCtrl2Reg, data, kUPDATE_SHADOW );

	//All this config should go in a single method : 
    map = provider->mapDeviceMemoryWithIndex(AppleDBDMAAudioDMAEngine::kDBDMADeviceIndex);
    FailIf (!map, Exit);
    soundConfigSpace = (UInt8 *)map->getPhysicalAddress();
	FailIf (!soundConfigSpace, Exit);

	// sets the clock base address figuring out which I2S cell we're on
	if ((((UInt32)soundConfigSpace ^ kI2S0BaseOffset) & 0x0001FFFF) == 0) {
		ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S0BaseOffset);
		i2SInterfaceNumber = 0;
	} else if ((((UInt32)soundConfigSpace ^ kI2S1BaseOffset) & 0x0001FFFF) == 0) {
		ioBaseAddress = (void *)((UInt32)soundConfigSpace - kI2S1BaseOffset);
		i2SInterfaceNumber = 1;
	} else {
		FailIf("AppleTexas2Audio::start failed to setup ioBaseAddress and ioClockBaseAddress\n", Exit);
	}

	// This is the keylargo FC1 (Feature configuration 1)
	ioClockBaseAddress = (void *)((UInt32)ioBaseAddress + kI2SClockOffset);

	// Enables the I2S Interface:
	debug2IOLog ("KLGetRegister(ioClockBaseAddress) | kI2S0InterfaceEnable = 0x%8.0lX\n", KLGetRegister(ioClockBaseAddress) | kI2S0InterfaceEnable);
	// I don't think that we should have to do this, it should already be done by OF or the platform expert.
	KLSetRegister(ioClockBaseAddress, KLGetRegister(ioClockBaseAddress) | kI2S0InterfaceEnable);

    // This call will set the next of the frame parameters
    // (clockSource, mclkDivisor,  sclkDivisor)
	FailIf (!setSampleParameters(myFrameRate, 256), Exit);
	FailIf (!setHWSampleRate(myFrameRate), Exit);
	setSerialFormatRegister(clockSource, mclkDivisor, sclkDivisor, serialFormat);

	err = Texas2_Initialize();			//	flush the shadow contents to the HW
	IOSleep (1);
	ToggleAnalogPowerDownWake();

Exit:
	if (NULL != gpio)
		gpio->release ();
	if (NULL != i2c)
		i2c->release ();

	debugIOLog("- AppleTexas2Audio::sndHWInitialize\n");
}

// --------------------------------------------------------------------------
void AppleTexas2Audio::sndHWPostDMAEngineInit (IOService *provider) {
	IOWorkLoop				*workLoop;

	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWPostDMAEngineInit\n");

	dallasDriver = NULL;
	dallasDriverNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleDallasDriver"), (IOServiceNotificationHandler)&DallasDriverPublished, this);
	if (NULL != dallasDriver)
		dallasDriverNotifier->remove ();

	workLoop = getWorkLoop();
	FailIf (NULL == workLoop, Exit);

	if (NULL != headphoneIntProvider) {
		headphoneIntEventSource = IOInterruptEventSource::interruptEventSource ((OSObject *)this,
																			headphoneInterruptHandler,
																			headphoneIntProvider,
																			0);
		FailIf (NULL == headphoneIntEventSource, Exit);
		workLoop->addEventSource (headphoneIntEventSource);
	}

	// Create a (primary) device interrupt source for dallas and attach it to the work loop
	if (NULL != dallasIntProvider) {
		// Create a timer event source
		dallasHandlerTimer = IOTimerEventSource::timerEventSource (this, DallasInterruptHandlerTimer);
		if (NULL != dallasHandlerTimer) {
			workLoop->addEventSource (dallasHandlerTimer);
		}

 		notifierHandlerTimer = IOTimerEventSource::timerEventSource (this, DisplaySpeakersNotFullyConnected);
		if (NULL != notifierHandlerTimer) {
			workLoop->addEventSource (notifierHandlerTimer);
		}

		dallasIntEventSource = IOFilterInterruptEventSource::filterInterruptEventSource (this,
																			dallasInterruptHandler, 
																			interruptFilter,
																			dallasIntProvider, 
																			0);
		FailIf (NULL == dallasIntEventSource, Exit);
		workLoop->addEventSource (dallasIntEventSource);
	}

	if (FALSE == IsHeadphoneConnected ()) {
		SetActiveOutput (kSndHWOutput2, kTouchBiquad);
		if (TRUE == hasVideo) {
			// Tell the video driver about the jack state change in case a video connector was plugged in
			publishResource (gAppleAudioVideoJackStateKey, headphonesConnected ? kOSBooleanTrue : kOSBooleanFalse);
		}
		if (NULL != dallasIntProvider) {
			// Set the correct EQ
			dallasInterruptHandler (this, 0, 0);
		} else {
			DeviceInterruptService ();
		}
	} else {
		if (NULL != headphoneIntProvider) {
			// Set amp mutes accordingly
			RealHeadphoneInterruptHandler (0, 0);
		}
	}

	if (NULL != headphoneIntEventSource)
		headphoneIntEventSource->enable ();
	if (NULL != dallasIntEventSource)
		dallasIntEventSource->enable ();

Exit:
	DEBUG_IOLOG("- AppleTexas2Audio::sndHWPostDMAEngineInit\n");
	return;
}


// --------------------------------------------------------------------------
IOReturn	AppleTexas2Audio::SetAnalogPowerDownMode( UInt8 mode )
{
	IOReturn	err;
	UInt8		dataBuffer[kTexas2ANALOGCTRLREGwidth];
	
	err = kIOReturnSuccess;
	if ( kPowerDownAnalog == mode || kPowerNormalAnalog == mode )
	{
		err = Texas2_ReadRegister( kTexas2AnalogControlReg, dataBuffer );
		if ( kIOReturnSuccess == err )
		{
			dataBuffer[0] &= ~( kAPD_MASK << kAPD );
			dataBuffer[0] |= ( mode << kAPD );
			err = Texas2_WriteRegister( kTexas2AnalogControlReg, dataBuffer, kUPDATE_ALL );
		}
	}
	return err;
}


// --------------------------------------------------------------------------
IOReturn	AppleTexas2Audio::ToggleAnalogPowerDownWake( void )
{
	IOReturn	err;
	
	err = SetAnalogPowerDownMode (kPowerDownAnalog);
	if (kIOReturnSuccess == err) {
		err = SetAnalogPowerDownMode (kPowerNormalAnalog);
	}
	return err;
}


// --------------------------------------------------------------------------
UInt32	AppleTexas2Audio::sndHWGetInSenseBits(
	void)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWGetInSenseBits\n");
	DEBUG_IOLOG("- AppleTexas2Audio::sndHWGetInSenseBits\n");
	return 0;	  
}

// --------------------------------------------------------------------------
// we can't read the registers back, so return the value in the shadow reg.
UInt32	AppleTexas2Audio::sndHWGetRegister(
	UInt32 regNum)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWGetRegister\n");
	
	UInt32	returnValue = 0;
	
	 return returnValue;
}

// --------------------------------------------------------------------------
// set the reg over i2c and make sure the value is cached in the shadow reg so we can "get it back"
IOReturn  AppleTexas2Audio::sndHWSetRegister(
	UInt32 regNum, 
	UInt32 val)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWSetRegister\n");
	IOReturn myReturn = kIOReturnSuccess;
	DEBUG_IOLOG("- AppleTexas2Audio::sndHWSetRegister\n");
	return(myReturn);
}

#pragma mark +HARDWARE IO ACTIVATION
/************************** Manipulation of input and outputs ***********************/
/********(These functions are enough to implement the simple UI policy)**************/

// --------------------------------------------------------------------------
UInt32	AppleTexas2Audio::sndHWGetActiveOutputExclusive(
	void)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWGetActiveOutputExclusive\n");
	DEBUG_IOLOG("- AppleTexas2Audio::sndHWGetActiveOutputExclusive\n");
	return 0;
}

// --------------------------------------------------------------------------
IOReturn   AppleTexas2Audio::sndHWSetActiveOutputExclusive(
	UInt32 outputPort )
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWSetActiveOutputExclusive\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexas2Audio::sndHWSetActiveOutputExclusive\n");
	return(myReturn);
}

// --------------------------------------------------------------------------
UInt32	AppleTexas2Audio::sndHWGetActiveInputExclusive(
	void)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWGetActiveInputExclusive\n");
	DEBUG_IOLOG("- AppleTexas2Audio::sndHWGetActiveInputExclusive\n");
	return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Set the hardware to select the desired input port after validating
//	that the target input port is available. 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	The Texas2 supports mutually exclusive selection of one of four analog
//	input signals.  There is no provision for selection of a 'none' input.
//	Mapping of input selections is as follows:
//		kSndHWInput1:		Texas2 analog input A, Stereo
//		kSndHWInput2:		Texas2 analog input B, Stereo
//		kSndHWInput3:		Texas2 analog input B, Mono sourced from left
//		kSndHWInput4:		Texas2 analog input B, Mono sourced from right
//	Only a subset of the available inputs are implemented on any given CPU
//	due to the multiple modes that analog input B can operate in.  The
//	'sound-objects' describes the resident hardware implemenation regarding
//	available inputs and the type of input.  It is possible to achieve an
//	equivalent selection of none by collecting the connections to the analog
//	ports.  If the Texas2 stereo input A is unused then kSndHWInput1 may
//	be aliased as kSndHWInputNone.  If neither of the Texas2 input B
//	ports are used then kSndHWInput2 may be aliased as kSndHWInputNone.  If
//	one of the Texas2 input B ports is used as a mono input port but the 
//	other input B mono input port remains unused then the unused mono input
//	port (i.e. kSndHWInput3 for the left channel or kSndHWInput4 for the
//	right channel) may be aliased as kSndHWInputNone.
IOReturn   AppleTexas2Audio::sndHWSetActiveInputExclusive(
    UInt32 input )
{
    UInt8		data[kTexas2MaximumRegisterWidth];
    IOReturn	result = kIOReturnSuccess; 
    
    DEBUG2_IOLOG("+ AppleTexas2Audio::sndHWSetActiveInputExclusive (%ld)\n", input);

	//	Mask off the current input selection and then OR in the new selections
	Texas2_ReadRegister (kTexas2AnalogControlReg, data);
	debug2IOLog ("Texas2_ReadRegister: data = %d\n", data[0]);
	data[0] &= ~((1 << kADM) | (1 << kLRB) | (1 << kINP));
    switch (input) {
        case kSndHWInput1:	data[0] |= ((kADMNormal << kADM) | (kLeftInputForMonaural << kLRB) | (kAnalogInputA << kINP));			break;
        case kSndHWInput2:	data[0] |= ((kADMNormal << kADM) | (kLeftInputForMonaural << kLRB) | (kAnalogInputB << kINP));			break;
        case kSndHWInput3:	data[0] |= ((kADMBInputsMonaural << kADM) | (kLeftInputForMonaural << kLRB) | (kAnalogInputB << kINP));	break;
        case kSndHWInput4:	data[0] |= ((kADMBInputsMonaural << kADM) | (kRightInputForMonaural << kLRB) | (kAnalogInputB << kINP));	break;
        default:			result = kIOReturnError;																					break;
    }
	//	If the new input selection remains valid then flush the
	//	selection setting out to the hardware.
	if (kIOReturnSuccess == result) {
		debug2IOLog ("about to call Texas2_WriteRegister: data = 0x%x\n", data[0]);
		result = Texas2_WriteRegister (kTexas2AnalogControlReg, data, kUPDATE_ALL);
	}
	//	Update history of current input selection for sndHWGetActiveInputExclusive
	if (kIOReturnSuccess == result) {
		mActiveInput = input;
	}

    DEBUG_IOLOG("- AppleTexas2Audio::sndHWSetActiveInputExclusive\n");
    return(result);
}

#pragma mark +CONTROL FUNCTIONS
// control function

// --------------------------------------------------------------------------
bool AppleTexas2Audio::sndHWGetSystemMute(void)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWGetSystemMute\n");
	DEBUG_IOLOG("- AppleTexas2Audio::sndHWGetSystemMute\n");
	return (gVolMuteActive);
}

// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::setModemSound(bool state) {
    UInt8			data[kTexas2MIXERGAINwidth];
    IOReturn		myReturn = kIOReturnSuccess;

    if(gHasModemSound == state) 
        goto EXIT;

	//	The Texas2 SDIN2 can be used for digital call progress.  If the
	//	modem is being enabled then the SDIN2 portion of the mixer is set
	//	to unity gain.  If the modem is being disabled then the SDIN2 
	//	portion of the mixer is set to mute.  All other mixer input gains
	//	remain unchanged.
	Texas2_ReadRegister( kTexas2MixerLeftGainReg, data );
	data[3] = state ? 0x10 : 0x00 ;
	Texas2_WriteRegister( kTexas2MixerLeftGainReg, data, kUPDATE_ALL );

	Texas2_ReadRegister( kTexas2MixerRightGainReg, data );
	data[3] = state ? 0x10 : 0x00 ;
	Texas2_WriteRegister( kTexas2MixerRightGainReg, data, kUPDATE_ALL );
                                                                                            
    gHasModemSound = state;

EXIT:
    return(myReturn);
}

// --------------------------------------------------------------------------
/* I'm not sure if this is still needed or not...
IOReturn AppleTexas2Audio::callPlatformFunction( const OSSymbol * functionName,bool
			waitForFunction,void *param1, void *param2, void *param3, void *param4 ){
			
	if(functionName->isEqualTo("setModemSound")) {
		return(setModemSound((bool)param1));
	}		

	return(super::callPlatformFunction(functionName,
			waitForFunction,param1, param2, param3, param4));
}
*/

// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::sndHWSetSystemMute(bool mutestate)
{
	IOReturn						result;

	result = kIOReturnSuccess;

	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWSetSystemMute\n");

	if (true == mutestate) {
		if (false == gVolMuteActive) {
			// mute the part
			gVolMuteActive = mutestate ;
			result = SetVolumeCoefficients (0, 0);
		}
	} else {
		// unmute the part
		gVolMuteActive = mutestate ;
		result = SetVolumeCoefficients (volumeTable[(UInt32)gVolLeft], volumeTable[(UInt32)gVolRight]);
	}

	DEBUG_IOLOG ("- AppleTexas2Audio::sndHWSetSystemMute\n");
	return (result);
}

// --------------------------------------------------------------------------
bool AppleTexas2Audio::sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume)
{
	bool					result;

	result = false;

	DEBUG3_IOLOG("+ AppleTexas2Audio::sndHWSetSystemVolume (left: %ld, right %ld)\n", leftVolume, rightVolume);
	gVolLeft = leftVolume;
	gVolRight = rightVolume;
	result = SetVolumeCoefficients (volumeTable[(UInt32)gVolLeft], volumeTable[(UInt32)gVolRight]);

	DEBUG_IOLOG("- AppleTexas2Audio::sndHWSetSystemVolume\n");
	return (result == kIOReturnSuccess);
}

// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::sndHWSetSystemVolume(UInt32 value)
{
	DEBUG2_IOLOG("+ AppleTexas2Audio::sndHWSetSystemVolume (vol: %ld)\n", value);
	
	IOReturn myReturn = kIOReturnError;
		
	// just call the default function in this class with the same val for left and right.
	if( true == sndHWSetSystemVolume( value, value ))
	{
		myReturn = kIOReturnSuccess;
	}

	DEBUG_IOLOG("- AppleTexas2Audio::sndHWSetSystemVolume\n");
	return(myReturn);
}

// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::sndHWSetPlayThrough(bool playthroughstate)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWSetPlayThrough\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexas2Audio::sndHWSetPlayThrough\n");
	return(myReturn);
}

// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain) 
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWSetPlayThrough\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexas2Audio::sndHWSetPlayThrough\n");
	return(myReturn);
}


// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::AdjustControls (void) {
	IOAudioEngine *					audioEngine;
	IOFixed							mindBVol;
	IOFixed							maxdBVol;

	audioEngine = OSDynamicCast (IOAudioEngine, audioEngines->getObject (0));

	mindBVol = volumedBTable[minVolume];
	maxdBVol = volumedBTable[maxVolume];

	if (NULL != outVolLeft && NULL != outVolRight) {
		audioEngine->pauseAudioEngine ();
		audioEngine->beginConfigurationChange ();
		outVolLeft->setMinValue (minVolume);
		outVolLeft->setMinDB (mindBVol);
		outVolLeft->setMaxValue (maxVolume);
		outVolLeft->setMaxDB (maxdBVol);
		outVolRight->setMinValue (minVolume);
		outVolRight->setMinDB (mindBVol);
		outVolRight->setMaxValue (maxVolume);
		outVolRight->setMaxDB (maxdBVol);
		audioEngine->completeConfigurationChange ();
		audioEngine->resumeAudioEngine ();
	}

	return kIOReturnSuccess;
}

#pragma mark +INDENTIFICATION

// --------------------------------------------------------------------------
// ::sndHWGetType
UInt32 AppleTexas2Audio::sndHWGetType(void )
{
	UInt32				returnValue;

	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWGetType\n");
 
	// in AudioHardwareConstants.h need to set up a constant for the hardware type.
	returnValue = kSndHWTypeTexas2;

	DEBUG_IOLOG ("- AppleTexas2Audio::sndHWGetType\n");
	return returnValue ;
}

// --------------------------------------------------------------------------
// ::sndHWGetManufactuer
// return the detected part's manufacturer.	 I think Daca is a single sourced part
// from Micronas Intermetall.  Always return just that.
UInt32 AppleTexas2Audio::sndHWGetManufacturer(void )
{
	UInt32				returnValue;

	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWGetManufacturer\n");

	// in AudioHardwareConstants.h need to set up a constant for the part manufacturer.
	returnValue = kSndHWManfTI ;
	
	DEBUG_IOLOG("- AppleTexas2Audio::sndHWGetManufacturer\n");
	return returnValue ;
}

#pragma mark +DETECT ACTIVATION & DEACTIVATION
// --------------------------------------------------------------------------
// ::setDeviceDetectionActive
// turn on detection, TODO move to superclass?? 
void AppleTexas2Audio::setDeviceDetectionActive(void)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::setDeviceDetectionActive\n");
	mCanPollStatus = true ;
	
	DEBUG_IOLOG("- AppleTexas2Audio::setDeviceDetectionActive\n");
	return ;
}

// --------------------------------------------------------------------------
// ::setDeviceDetectionInActive
// turn off detection, TODO move to superclass?? 
void AppleTexas2Audio::setDeviceDetectionInActive(void)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::setDeviceDetectionInActive\n");
	mCanPollStatus = false ;
	
	DEBUG_IOLOG("- AppleTexas2Audio::setDeviceDetectionInActive\n");
	return ;
}

#pragma mark +POWER MANAGEMENT
//Power Management

// --------------------------------------------------------------------------
/*
	Update AudioProj14PowerObject::setHardwarePowerOn() to set *microsecondsUntilComplete = 2000000
	when waking from sleep.
*/

IOReturn AppleTexas2Audio::sndHWSetPowerState(IOAudioDevicePowerState theState)
{
	IOReturn							result;

	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWSetPowerState\n");

	result = kIOReturnSuccess;
	switch (theState) {
		case kIOAudioDeviceActive:
			result = performDeviceWake ();
			completePowerStateChange ();
			break;
		case kIOAudioDeviceIdle:
		case kIOAudioDeviceSleep:
			result = performDeviceSleep ();
			break;
	}

	DEBUG_IOLOG("- AppleTexas2Audio::sndHWSetPowerState\n");
	return result;
}

// --------------------------------------------------------------------------
//	Set the audio hardware to sleep mode by placing the Texas2 into
//	analog power down mode after muting the amplifiers.  The I2S clocks
//	must also be stopped after these two tasks in order to achieve
//	a fully low power state.
IOReturn AppleTexas2Audio::performDeviceSleep () {
    IOService *							keyLargo;

	debugIOLog ("+ AppleTexas2Audio::performDeviceSleep\n");

	keyLargo = NULL;

	//	Mute all of the amplifiers

	SetAmplifierMuteState( kHEADPHONE_AMP, 1 == hdpnActiveState ? 1 : 0 );
	SetAmplifierMuteState( kSPEAKER_AMP, 1 == ampActiveState ? 1 : 0 );

	//	Set the Texas2 analog control register to analog power down mode

	SetAnalogPowerDownMode (kPowerDownAnalog);

    keyLargo = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));
    
    if (NULL != keyLargo) {
		// ...and turn off the i2s clocks...
		keyLargo->callPlatformFunction (OSSymbol::withCString ("keyLargo_powerI2S"), false, (void *)false, (void *)0, 0, 0);
	}

	debugIOLog ("- AppleTexas2Audio::performDeviceSleep\n");
	return kIOReturnSuccess;
}
	
// --------------------------------------------------------------------------
//	The I2S clocks must have been started before performing this method.
//	This method sets the Texas2 analog control register to normal operating
//	mode and unmutes the amplifers.
IOReturn AppleTexas2Audio::performDeviceWake () {
    IOService *							keyLargo;
	IOReturn							err;

	debugIOLog ("+ AppleTexas2Audio::performDeviceWake\n");

	err = kIOReturnSuccess;
	keyLargo = NULL;
    keyLargo = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));
    
    if (NULL != keyLargo) {
		// Turn on the i2s clocks...
		keyLargo->callPlatformFunction (OSSymbol::withCString ("keyLargo_powerI2S"), false, (void *)true, (void *)0, 0, 0);
	}

	//	Set the Texas2 analog control register to analog power up mode
	SetAnalogPowerDownMode (kPowerNormalAnalog);
	
	// ...then bring everything back up the way it should be.
	err = Texas2_Initialize ();			//	reset the TAS3001C and flush the shadow contents to the HW

	//	Mute the amplifiers as needed
	if (FALSE == IsHeadphoneConnected ()) {
		SetActiveOutput (kSndHWOutput2, kTouchBiquad);
		if (TRUE == hasVideo) {
			// Tell the video driver about the jack state change in case a video connector was plugged in
			publishResource (gAppleAudioVideoJackStateKey, headphonesConnected ? kOSBooleanTrue : kOSBooleanFalse);
		}
		if (NULL != dallasIntProvider) {
			// Set the correct EQ
			dallasInterruptHandler (this, 0, 0);
		} else {
			DeviceInterruptService ();
		}
	} else {
		if (NULL != headphoneIntProvider) {
			// Set amp mutes accordingly
			RealHeadphoneInterruptHandler (0, 0);
		}
	}

	debugIOLog ("- AppleTexas2Audio::performDeviceWake\n");
	return err;
}

// --------------------------------------------------------------------------
// ::sndHWGetConnectedDevices
// TODO: Move to superclass
UInt32 AppleTexas2Audio::sndHWGetConnectedDevices(
	void)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWGetConnectedDevices\n");
   UInt32	returnValue = currentDevices;

	DEBUG_IOLOG("- AppleTexas2Audio::sndHWGetConnectedDevices\n");
	return returnValue ;
}

// --------------------------------------------------------------------------
UInt32 AppleTexas2Audio::sndHWGetProgOutput(
	void)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWGetProgOutput\n");
	DEBUG_IOLOG("- AppleTexas2Audio::sndHWGetProgOutput\n");
	return 0;
}

// --------------------------------------------------------------------------
IOReturn AppleTexas2Audio::sndHWSetProgOutput(
	UInt32 outputBits)
{
	DEBUG_IOLOG("+ AppleTexas2Audio::sndHWSetProgOutput\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexas2Audio::sndHWSetProgOutput\n");
	return(myReturn);
}

#pragma mark +INTERRUPT HANDLERS
Boolean AppleTexas2Audio::IsSpeakerConnected (void) {
	UInt8						dallasSenseContents;
	Boolean						connection;

	connection = FALSE;
	if (NULL != dallasIntProvider) {
		dallasSenseContents = *(dallasExtIntGpio);

		debug3IOLog ("dallasExtIntGpio = %p, dallasSenseContents = 0x%X\n", dallasExtIntGpio, dallasSenseContents);
		if ((dallasSenseContents & (1 << 1)) == (dallasInsertedActiveState << 1)) {
			debugIOLog ("dallas speakers are connected\n");
			connection = TRUE;
		} else {
			debugIOLog ("dallas speakers are NOT connected\n");
			connection = FALSE;
		}
	}

	return connection;
}

// --------------------------------------------------------------------------
void AppleTexas2Audio::DallasInterruptHandlerTimer (OSObject *owner, IOTimerEventSource *sender) {
    AppleTexas2Audio *			device;
	AbsoluteTime				currTime;


	device = OSDynamicCast (AppleTexas2Audio, owner);
	FailIf (NULL == device, Exit);

	device->dallasSpeakersConnected = device->IsSpeakerConnected ();
#if DEBUGLOG
	IOLog ("dallas speakers connected = %d\n", device->dallasSpeakersConnected);
#endif

	if (FALSE == device->IsHeadphoneConnected ()) {
		// Set the proper EQ
		device->DeviceInterruptService ();

		clock_get_uptime (&currTime);
		absolutetime_to_nanoseconds (currTime, &device->savedNanos);
	}

	if (NULL != device->dallasIntEventSource) {
		device->dallasIntEventSource->enable();
	}

Exit:
	debugIOLog ("- DallasInterruptHandlerTimer\n");
	return;
}

// --------------------------------------------------------------------------
// Set a flag to say if the dallas speakers are plugged in or not so we know which EQ to use.
void AppleTexas2Audio::RealDallasInterruptHandler (IOInterruptEventSource *source, int count) {
    AbsoluteTime				fireTime;
    UInt64						nanos;

	// call DallasInterruptHandlerTimer in a bit to check for the dallas rom (and complete speaker insertion).
	if (NULL != dallasHandlerTimer) {
		clock_get_uptime (&fireTime);
		absolutetime_to_nanoseconds (fireTime, &nanos);
		nanos += kInsertionDelayNanos;	// Schedule 250ms in the future...

		nanoseconds_to_absolutetime (nanos, &fireTime);
		dallasHandlerTimer->wakeAtTime (fireTime);
	}

	return;
}

// --------------------------------------------------------------------------
// Initial creation of this routine duplicates dallasInterruptHandler to encourage changes 
// to be added to dallasInterruptHandler - not here. 
void AppleTexas2Audio::dallasInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count) {
	AbsoluteTime 	currTime;
	UInt64 			currNanos;

	AppleTexas2Audio *appleTexas2Audio = (AppleTexas2Audio *)owner;
	FailIf (NULL == appleTexas2Audio, Exit);

	// Need this disable for when we call dallasInterruptHandler instead of going through the interrupt filter
	appleTexas2Audio->dallasIntEventSource->disable();

	clock_get_uptime (&currTime);
	absolutetime_to_nanoseconds (currTime, &currNanos);

	if ((currNanos - appleTexas2Audio->savedNanos) > 10000000) {
		appleTexas2Audio->RealDallasInterruptHandler (source, count);
	} else { 
		appleTexas2Audio->dallasIntEventSource->enable();
	}

Exit:
	return;
}

// --------------------------------------------------------------------------
// static "action" function to connect to our object
// return TRUE if you want the handler function to be called, or FALSE if you don't want it to be called.
bool AppleTexas2Audio::interruptFilter(OSObject *owner, IOFilterInterruptEventSource *src)
{
	AppleTexas2Audio* self = (AppleTexas2Audio*) owner;

	self->dallasIntEventSource->disable();
	return (true);
}

// --------------------------------------------------------------------------
// This is called to tell the user that they may not have plugged their speakers in all the way.
void AppleTexas2Audio::DisplaySpeakersNotFullyConnected (OSObject *owner, IOTimerEventSource *sender) {
	AppleTexas2Audio *		appleTexas2Audio;
	AbsoluteTime			currTime;
	UInt32					deviceID;
	UInt8					bROM[8];
	UInt8					bEEPROM[32];
	UInt8					bAppReg[8];
	Boolean					result;

	debugIOLog ("+ DisplaySpeakersNotFullyConnected\n");

	appleTexas2Audio = OSDynamicCast (AppleTexas2Audio, owner);
	FailIf (!appleTexas2Audio, Exit);

	if (0 == console_user) {
		appleTexas2Audio->notifierHandlerTimer->setTimeoutMS (kNotifyTimerDelay);	//No one logged in yet (except maybe root) reset the timer to fire later. 
	} else {
		if (appleTexas2Audio->doneWaiting == FALSE) { 
			// The next time this function is called we'll check the state and display the dialog as needed
			appleTexas2Audio->notifierHandlerTimer->setTimeoutMS (kUserLoginDelay);	// Someone has logged in. Delay the notifier so it does not apear behind the login screen.
			appleTexas2Audio->doneWaiting = TRUE;
		} else {
			deviceID = appleTexas2Audio->GetDeviceMatch ();
			if (kExternalSpeakersActive == deviceID) {
				if (NULL != appleTexas2Audio->dallasDriver) {
					appleTexas2Audio->dallasIntEventSource->disable ();
					result = appleTexas2Audio->dallasDriver->getSpeakerID (bROM, bEEPROM, bAppReg);
					appleTexas2Audio->dallasIntEventSource->enable ();
					clock_get_uptime (&currTime);
					absolutetime_to_nanoseconds (currTime, &appleTexas2Audio->savedNanos);

					if (TRUE == result) {	// TRUE == failure for DallasDriver
						KUNCUserNotificationDisplayNotice (
						0,		// Timeout in seconds
						0,		// Flags (for later usage)
						"",		// iconPath (not supported yet)
						"",		// soundPath (not supported yet)
						"/System/Library/Extensions/AppleOnboardAudio.kext",		// localizationPath
						"HeaderOfDallasPartialInsert",		// the header
						"StringOfDallasPartialInsert",
						"ButtonOfDallasPartialInsert"); 
	
						IOLog ("The device plugged into the Apple speaker mini-jack cannot be recognized.\n");
						IOLog ("Remove the plug from the jack. Then plug it back in and make sure it is fully inserted.\n");
					} else {
						// Speakers are fully plugged in now, so load the proper EQ for them
						appleTexas2Audio->dallasIntEventSource->disable ();
						appleTexas2Audio->DeviceInterruptService ();
						appleTexas2Audio->dallasIntEventSource->enable ();
						clock_get_uptime (&currTime);
						absolutetime_to_nanoseconds (currTime, &appleTexas2Audio->savedNanos);
					}
				}
			}
		}
	}

Exit:
	debugIOLog ("- DisplaySpeakersNotFullyConnected\n");
    return;
}

Boolean AppleTexas2Audio::IsHeadphoneConnected (void) {
	UInt8				headphoneSenseContents;
	Boolean				connection;

	// check the state of the extint-gpio15 pin for the actual state of the headphone jack
	// do this because we get a false interrupt when waking from sleep that makes us think
	// that the headphones were removed during sleep, even if they are still connected.

	connection = FALSE;
	if (NULL != headphoneIntEventSource) {
		headphoneSenseContents = *headphoneExtIntGpio;

		debug3IOLog ("headphoneExtIntGpio = %p, headphoneSenseContents = 0x%X\n", headphoneExtIntGpio, headphoneSenseContents);
		if ((headphoneSenseContents & (1 << 1)) == (headphoneInsertedActiveState << 1)) {
			// headphones are inserted
			debugIOLog ("Headphones are inserted\n");
			connection = TRUE;
		} else {
			// headphones are not inserted
			debugIOLog ("Headphones are not inserted\n");
			connection = FALSE;
		}
	}

	return connection;
}

// --------------------------------------------------------------------------
void AppleTexas2Audio::RealHeadphoneInterruptHandler (IOInterruptEventSource *source, int count) {
	headphonesConnected = IsHeadphoneConnected ();

	if (TRUE == headphonesConnected) {
		SetActiveOutput (kSndHWOutput1, kTouchBiquad);
	} else {
		SetActiveOutput (kSndHWOutput2, kTouchBiquad);
	}

	if (TRUE == hasVideo) {
		// Tell the video driver about the jack state change in case a video connector was plugged in
		publishResource (gAppleAudioVideoJackStateKey, headphonesConnected ? kOSBooleanTrue : kOSBooleanFalse);
	}

	DeviceInterruptService ();
}

// --------------------------------------------------------------------------
void AppleTexas2Audio::headphoneInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count)
{
	AppleTexas2Audio *appleTexas2Audio = (AppleTexas2Audio *)owner;
	FailIf (!appleTexas2Audio, Exit);

	appleTexas2Audio->RealHeadphoneInterruptHandler (source, count);

Exit:
	return;
}

#pragma mark +DIRECT HARDWARE MANIPULATION
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Setup the pointer to the shadow register and the size of the shadow
//	register.  The Texas2 has a different register set and different
//	register sizes from the Texas2 so a shadow register set is maintained
//	for both parts but only the appropriate set is used.
IOReturn 	AppleTexas2Audio::GetShadowRegisterInfo( UInt8 regAddr, UInt8 ** shadowPtr, UInt8* registerSize ) {
	IOReturn		err;
	
	err = kIOReturnSuccess;
	FailWithAction( NULL == shadowPtr, err = -50, Exit );
	FailWithAction( NULL == registerSize, err = -50, Exit );
	
	switch( regAddr )
	{
		case kTexas2MainCtrl1Reg:					*shadowPtr = (UInt8*)shadowTexas2Regs.sMC1R;	*registerSize = kTexas2MC1Rwidth;					break;
		case kTexas2DynamicRangeCtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sDRC;		*registerSize = kTexas2DRCwidth;					break;
		case kTexas2VolumeCtrlReg:					*shadowPtr = (UInt8*)shadowTexas2Regs.sVOL;		*registerSize = kTexas2VOLwidth;					break;
		case kTexas2TrebleCtrlReg:					*shadowPtr = (UInt8*)shadowTexas2Regs.sTRE;		*registerSize = kTexas2TREwidth;					break;
		case kTexas2BassCtrlReg:					*shadowPtr = (UInt8*)shadowTexas2Regs.sBAS;		*registerSize = kTexas2BASwidth;					break;
		case kTexas2MixerLeftGainReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sMXL;		*registerSize = kTexas2MIXERGAINwidth;				break;
		case kTexas2MixerRightGainReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sMXR;		*registerSize = kTexas2MIXERGAINwidth;				break;
		case kTexas2LeftBiquad0CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB0;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad1CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB1;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad2CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB2;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad3CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB3;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad4CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB4;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad5CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB5;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftBiquad6CtrlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sLB6;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad0CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB0;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad1CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB1;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad2CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB2;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad3CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB3;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad4CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB4;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad5CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB5;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightBiquad6CtrlReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRB6;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftLoudnessBiquadReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sLLB;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2RightLoudnessBiquadReg:			*shadowPtr = (UInt8*)shadowTexas2Regs.sRLB;		*registerSize = kTexas2BIQwidth;					break;
		case kTexas2LeftLoudnessBiquadGainReg:		*shadowPtr = (UInt8*)shadowTexas2Regs.sLLBG;	*registerSize = kTexas2LOUDNESSBIQUADGAINwidth;	break;
		case kTexas2RightLoudnessBiquadGainReg:		*shadowPtr = (UInt8*)shadowTexas2Regs.sRLBG;	*registerSize = kTexas2LOUDNESSBIQUADGAINwidth;	break;
		case kTexas2AnalogControlReg:				*shadowPtr = (UInt8*)shadowTexas2Regs.sACR;		*registerSize = kTexas2ANALOGCTRLREGwidth;			break;
		case kTexas2MainCtrl2Reg:					*shadowPtr = (UInt8*)shadowTexas2Regs.sMC2R;	*registerSize = kTexas2MC2Rwidth;					break;
		default:									err = -201;/*notEnoughHardware;*/																	break;
	}
	
Exit:
    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Return 'true' if the 'gpioDDR' bit is non-zero.
UInt8	AppleTexas2Audio::GpioGetDDR( UInt8* gpioAddress )
{
	UInt8			gpioData;
	Boolean			result;
	
	result = gpioDDR_INPUT;
	if( NULL != gpioAddress )
	{
		gpioData = *gpioAddress;
		if( 0 != ( gpioData & ( 1 << gpioDDR ) ) )
			result = gpioDDR_OUTPUT ;
		debug4IOLog( "***** GPIO DDR RD 0x%8.0X = 0x%2.0X returns %d\n", (unsigned int)gpioAddress, gpioData, result );
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Return 'true' if the 'gpioData' bit is non-zero.  This function does not
//	return the state of the pin.
Boolean AppleTexas2Audio::GpioRead( UInt8* gpioAddress )
{
	UInt8			gpioData;
	Boolean			result;
	
	result = 0;
	if( NULL != gpioAddress )
	{
		gpioData = *gpioAddress;
		if( 0 != ( gpioData & ( 1 << gpioDATA ) ) )
			result = 1;
		debug4IOLog( "GpioRead( 0x%8.0X ) result %d, *gpioAddress 0x%2.0X\n", (unsigned int)gpioAddress, result, *gpioAddress );
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Sets the 'gpioDDR' to OUTPUT and sets the 'gpioDATA' to the 'data' state.
void	AppleTexas2Audio::GpioWrite( UInt8* gpioAddress, UInt8 data )
{
	UInt8		gpioData;
	
	if( NULL != gpioAddress )
	{
		if( 0 == data )
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 0 << gpioDATA );
		else
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 1 << gpioDATA );
		*gpioAddress = gpioData;
		debug4IOLog( "GpioWrite( 0x%8.0X, 0x%2.0X ), *gpioAddress 0x%2.0X\n", (unsigned int)gpioAddress, gpioData, *gpioAddress );
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::InitEQSerialMode (UInt32 mode, Boolean restoreOnNormal)
{
	IOReturn		err;
	UInt8			initData;
	UInt8			previousData;
	UInt8*			shadowPtr;
	UInt8			registerSize;
	
	debug3IOLog ("AppleTexas2Audio::InitEQSerialMode (%8lX, %d)\n", mode, restoreOnNormal);
	initData = (kNormalLoad << kFL);
	if (kSetFastLoadMode == mode)
		initData = (kFastLoad << kFL);
		
	err = Texas2_ReadRegister (kTexas2MainCtrl1Reg, &previousData);
	initData |= (k64fs << kSC) | TAS_I2S_MODE | (TAS_WORD_LENGTH << kW0);
	err = Texas2_WriteRegister (kTexas2MainCtrl1Reg, &initData, kUPDATE_ALL);
	
	//	If restoring to normal load mode then restore the settings of all
	//	registers that have been corrupted by entering fast load mode (i.e.
	//	volume, bass, treble, mixer1 and mixer2).  Restoration only occurs
	//	if going from a previous state of Fast Load to a new state of Normal Load.
	if (kRestoreOnNormal == restoreOnNormal && ((kFastLoad << kFL) == (kFastLoad << kFL) & previousData)) {
		if ((kNormalLoad << kFL) == (initData & (kFastLoad << kFL))) {

			GetShadowRegisterInfo( kTexas2VolumeCtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2VolumeCtrlReg, shadowPtr, kUPDATE_HW );
			FailIf( kIOReturnSuccess != err, Exit );
			
			GetShadowRegisterInfo( kTexas2TrebleCtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2TrebleCtrlReg, shadowPtr, kUPDATE_HW );
			FailIf( kIOReturnSuccess != err, Exit );
			
			GetShadowRegisterInfo( kTexas2BassCtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2BassCtrlReg, shadowPtr, kUPDATE_HW );
			FailIf( kIOReturnSuccess != err, Exit );
			
			GetShadowRegisterInfo( kTexas2MixerLeftGainReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2MixerLeftGainReg, shadowPtr, kUPDATE_HW );
			FailIf( kIOReturnSuccess != err, Exit );
	
			GetShadowRegisterInfo( kTexas2MixerRightGainReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2MixerRightGainReg, shadowPtr, kUPDATE_HW );
			FailIf( kIOReturnSuccess != err, Exit );
		}
	}
Exit:
	debug4IOLog ("AppleTexas2Audio ... %d = InitEQSerialMode (%8lX, %d)\n", err, mode, restoreOnNormal);
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Updates the amplifier mute state and delays for amplifier settling if
//	the amplifier mute state is not the current mute state.
IOReturn AppleTexas2Audio::SetAmplifierMuteState( UInt32 ampID, Boolean muteState )
{
	IOReturn			err;
	Boolean				curMuteState;
	
	debug3IOLog( "SetAmplifierMuteState( %ld, %d )\n", ampID, muteState );
	err = kIOReturnSuccess;
	switch( ampID )
	{
		case kHEADPHONE_AMP:
			curMuteState = GpioRead( hdpnMuteGpio );
			if( muteState != curMuteState )
			{
				GpioWrite( hdpnMuteGpio, muteState );
				debug2IOLog( "updated HEADPHONE mute to %d\n", muteState );
			}
			break;
		case kSPEAKER_AMP:
			curMuteState = GpioRead( ampMuteGpio );
			if( muteState != curMuteState )
			{
				GpioWrite( ampMuteGpio, muteState );
				debug2IOLog( "updated AMP mute to %d\n", muteState );
			}
			break;
		default:
			err = -50 /*paramErr */;
			break;
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::SetVolumeCoefficients( UInt32 left, UInt32 right )
{
	UInt8		volumeData[kTexas2VOLwidth];
	IOReturn	err;
	
	debug3IOLog("SetVolumeCoefficients: L=%ld R=%ld\n", left, right);
	
	volumeData[2] = left;														
	volumeData[1] = left >> 8;												
	volumeData[0] = left >> 16;												
	
	volumeData[5] = right;														
	volumeData[4] = right >> 8;												
	volumeData[3] = right >> 16;
	
	err = Texas2_WriteRegister( kTexas2VolumeCtrlReg, volumeData, kUPDATE_ALL );

	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will perform a reset of the TAS3001C and then initialize
//	all registers within the TAS3001C to the values already held within 
//	the shadow registers.  The RESET sequence must not be performed until
//	the I2S clocks are running.	 The TAS3001C may hold the I2C bus signals
//	SDA and SCL low until the reset sequence (high->low->high) has been
//	completed.
IOReturn	AppleTexas2Audio::Texas2_Initialize() {
	IOReturn		err;
	UInt32		retryCount;
	UInt32		initMode;
	UInt8		oldMode;
	UInt8		*shadowPtr;
	UInt8		registerSize;
	Boolean		done;
	
	err = -227;	//siDeviceBusyErr
	done = false;
	oldMode = 0;
	initMode = kUPDATE_HW;
	retryCount = 0;
	if (!semaphores)
	{
		semaphores = 1;
		do{
			debug2IOLog( "[AppleTexas2Audio] ... RETRYING, retryCount %ld\n", retryCount );
			if( 0 == oldMode )
				Texas2_ReadRegister( kTexas2MainCtrl1Reg, &oldMode );					//	save previous load mode

			err = InitEQSerialMode( kSetFastLoadMode, kDontRestoreOnNormal );								//	set fast load mode for biquad initialization
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2LeftBiquad0CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2LeftBiquad0CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo( kTexas2LeftBiquad1CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2LeftBiquad1CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo( kTexas2LeftBiquad2CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2LeftBiquad2CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo( kTexas2LeftBiquad3CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2LeftBiquad3CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo( kTexas2LeftBiquad4CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2LeftBiquad4CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo( kTexas2LeftBiquad5CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2LeftBiquad5CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2LeftBiquad6CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2LeftBiquad6CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2LeftLoudnessBiquadReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2LeftLoudnessBiquadReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2RightBiquad0CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2RightBiquad0CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo( kTexas2RightBiquad1CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2RightBiquad1CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo( kTexas2RightBiquad2CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2RightBiquad2CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo( kTexas2RightBiquad3CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2RightBiquad3CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo( kTexas2RightBiquad4CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2RightBiquad4CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo( kTexas2RightBiquad5CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2RightBiquad5CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2RightBiquad6CtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2RightBiquad6CtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2RightLoudnessBiquadReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2RightLoudnessBiquadReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = InitEQSerialMode( kSetNormalLoadMode, kDontRestoreOnNormal );								//	set normal load mode for most register initialization
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2DynamicRangeCtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2DynamicRangeCtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
			GetShadowRegisterInfo( kTexas2VolumeCtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2VolumeCtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2TrebleCtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2TrebleCtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2BassCtrlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2BassCtrlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2MixerLeftGainReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2MixerLeftGainReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			GetShadowRegisterInfo( kTexas2MixerRightGainReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2MixerRightGainReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );

			GetShadowRegisterInfo( kTexas2LeftLoudnessBiquadGainReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2LeftLoudnessBiquadGainReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2RightLoudnessBiquadGainReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2RightLoudnessBiquadGainReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2AnalogControlReg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2AnalogControlReg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2MainCtrl2Reg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2MainCtrl2Reg, shadowPtr, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			GetShadowRegisterInfo( kTexas2MainCtrl1Reg, &shadowPtr, &registerSize );
			err = Texas2_WriteRegister( kTexas2MainCtrl1Reg, &oldMode, initMode );			//	restore previous load mode
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
AttemptToRetry:				
			if( kIOReturnSuccess == err )		//	terminate when successful
			{
				done = true;
			}
			retryCount++;
		} while ( !done && ( kTexas2_MAX_RETRY_COUNT != retryCount ) );
		semaphores = 0;
		if( kTexas2_MAX_RETRY_COUNT == retryCount )
			debug2IOLog( "\n\n\n\n          Texas2 IS DEAD: Check %s\n\n\n\n", "ChooseAudio in fcr1" );
	}
	if( kIOReturnSuccess != err )
		debug2IOLog( "[AppleTexas2Audio] Texas2_Initialize() err = %d\n", err );

    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Reading registers with the Texas2 is not possible.  A shadow register
//	is maintained for each Texas2 hardware register.  Whenever a write
//	operation is performed on a hardware register, the data is written to 
//	the shadow register.  Read operations copy data from the shadow register
//	to the client register buffer.
IOReturn 	AppleTexas2Audio::Texas2_ReadRegister(UInt8 regAddr, UInt8* registerData) {
	UInt8			registerSize;
	UInt32			regByteIndex;
	UInt8			*shadowPtr;
	IOReturn		err;
	
	err = kIOReturnSuccess;
	// quiet warnings caused by a complier that can't really figure out if something is going to be used uninitialized or not.
	registerSize = 0;
	shadowPtr = NULL;
	err = GetShadowRegisterInfo( regAddr, &shadowPtr, &registerSize );
	if( kIOReturnSuccess == err )
	{
		for( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ )
		{
			registerData[regByteIndex] = shadowPtr[regByteIndex];
		}
	}
	if( kIOReturnSuccess != err )
		debug4IOLog( "[AppleTexas2Audio] %d notEnoughHardware = Texas2_ReadRegister( 0x%2.0X, 0x%8.0X )", err, regAddr, (unsigned int)registerData );

    return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	All Texas2 write operations pass through this function so that a shadow
//	copy of the registers can be kept in global storage.  This function does enforce the data
//	size of the target register.  No partial register write operations are
//	supported.  IMPORTANT:  There is no enforcement regarding 'load' mode 
//	policy.  Client routines should properly maintain the 'load' mode by 
//	saving the contents of the master control register, set the appropriate 
//	load mode for the target register and then restore the previous 'load' 
//	mode.  All biquad registers should only be loaded while in 'fast load' 
//	mode.  All other registers should be loaded while in 'normal load' mode.
IOReturn 	AppleTexas2Audio::Texas2_WriteRegister(UInt8 regAddr, UInt8* registerData, UInt8 mode){
	UInt8			registerSize;
	UInt32			regByteIndex;
	UInt8			*shadowPtr;
	IOReturn		err;
	Boolean			updateRequired;
	Boolean			success;
	
	err = kIOReturnSuccess;
	updateRequired = false;
	success = false;
	// quiet warnings caused by a complier that can't really figure out if something is going to be used uninitialized or not.
	registerSize = 0;
	shadowPtr = NULL;
	err = GetShadowRegisterInfo( regAddr, &shadowPtr, &registerSize );
	if( kIOReturnSuccess == err )
	{
		//	Write through to the shadow register as a 'write through' cache would and
		//	then write the data to the hardware;
		if( kUPDATE_SHADOW == mode || kUPDATE_ALL == mode )
		{
			success = true;
			for( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ )
			{
				if( shadowPtr[regByteIndex] != registerData[regByteIndex] && kUPDATE_ALL == mode )
					updateRequired = true;
				shadowPtr[regByteIndex] = registerData[regByteIndex];
			}
		}
		if( kUPDATE_HW == mode || updateRequired )
		{
			debug3IOLog( "[AppleTexas2Audio] Texas2_WriteRegister addr: %2.0X subaddr: %2.0X, data = ", DEQAddress, regAddr );
			for( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ )
			{
				debug2IOLog( "%2.0X ", registerData[regByteIndex] );
			}
			debug2IOLog( "%s", "\n" );

			if (openI2C()) {
				success = interface->writeI2CBus (DEQAddress, regAddr, registerData, registerSize);
				closeI2C();
			} else {
				debugIOLog ("couldn't open the I2C bus!\n");
			}
		}
	}

	if( kIOReturnSuccess != err || !success )
	{
	debug3IOLog ("error 0x%X returned, success == %d in AppleTexas2Audio::Texas2_WriteRegister\n", err, success);
	if (kIOReturnSuccess == err)
		err = -1;	// force a retry
	/*
		switch( err )
		{
			case notEnoughHardware:	debug3IOLog( "[AppleTexas2Audio] %d notEnoughHardware = PBControlImmed( 0x%8.0X )\n", err, pb );	break;
			case noHardware:		debug3IOLog( "[AppleTexas2Audio] %d noHardware = PBControlImmed( 0x%8.0X )\n", err, pb );		break;
			case controlErr:		debug3IOLog( "[AppleTexas2Audio] %d controlErr = PBControlImmed( 0x%8.0X )\n", err, pb );		break;
			case paramErr:			debug3IOLog( "[AppleTexas2Audio] %d paramErr = PBControlImmed( 0x%8.0X )\n", err, pb );			break;
			case ioErr:				debug3IOLog( "[AppleTexas2Audio] %d ioErr = PBControlImmed( 0x%8.0X )\n", err, pb );				break;
			case siDeviceBusyErr:	debug3IOLog( "[AppleTexas2Audio] %d siDeviceBusyErr = PBControlImmed( 0x%8.0X )\n", err, pb );	break;
			default:				debug3IOLog( "[AppleTexas2Audio] %d = PBControlImmed( 0x%8.0X )\n", err, pb );					break;
		}
	*/
	}


    return err;
}


#pragma mark +UTILITY FUNCTIONS
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleTexas2Audio::DallasDriverPublished (AppleTexas2Audio * AppleTexas2Audio, void * refCon, IOService * newService) {
	bool						resultCode;

	resultCode = FALSE;

	FailIf (NULL == AppleTexas2Audio, Exit);
	FailIf (NULL == newService, Exit);

	AppleTexas2Audio->dallasDriver = (AppleDallasDriver *)newService;

	AppleTexas2Audio->attach (AppleTexas2Audio->dallasDriver);
	AppleTexas2Audio->dallasDriver->open (AppleTexas2Audio);

	if (NULL != AppleTexas2Audio->dallasDriverNotifier)
		AppleTexas2Audio->dallasDriverNotifier->remove ();

	resultCode = TRUE;

Exit:
	return resultCode;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Set state equal to if external speakers are around. If active, restore
// preferred mute state. If inactive, mute in hardware.
// Has to be called on the IOAudioFamily workloop (because we do the config change of the controls)!!!
void AppleTexas2Audio::DeviceInterruptService (void) {
	EQPrefsElementPtr	eqPrefs;
	IOReturn			err;
	UInt32				speakerID;
	UInt32				result;
	UInt8				bROM[8];
	UInt8				bEEPROM[32];
	UInt8				bAppReg[8];

	err = kIOReturnSuccess;

	// get the layoutID from the IORegistry for the machine we're running on
	// deviceMatch is set from sound objects, but we're hard coding it using a table at the moment
	speakerID = 0;
	result = TRUE;
	if (NULL != dallasDriver && 4 == GetDeviceMatch ()) {
		result = dallasDriver->getSpeakerID (bROM, bEEPROM, bAppReg);
		speakerID = bEEPROM[1];
	}

	err = GetCustomEQCoefficients (layoutID, GetDeviceMatch (), speakerID, &eqPrefs);

	if (NULL != dallasDriver && TRUE == dallasSpeakersConnected && TRUE == result) {
		// If the Dallas speakers are misinserted, set registry up for our MacBuddy buddies no matter what the output device is
		speakerConnectFailed = TRUE;
	} else {
		speakerConnectFailed = FALSE;
	}
	setProperty (kSpeakerConnectError, speakerConnectFailed);

	if (NULL != dallasDriver && TRUE == result && kExternalSpeakersActive == deviceID) {
		// Only put up our alert if the Dallas speakers are the output device
		DisplaySpeakersNotFullyConnected (this, NULL);
	}

	debug6IOLog ("%d = GetCustomEQCoefficients (%lX, %lX, %lX, %p)\n", err, layoutID, GetDeviceMatch (), speakerID, eqPrefs);

	if (kIOReturnSuccess == err && NULL != eqPrefs) {
		DRCInfo				localDRC;

		//	Set the dynamic range compressor coefficients.
		localDRC.compressionRatioNumerator	= eqPrefs->drcCompressionRatioNumerator;
		localDRC.compressionRatioDenominator	= eqPrefs->drcCompressionRatioDenominator;
		localDRC.threshold					= eqPrefs->drcThreshold;
		localDRC.maximumVolume				= eqPrefs->drcMaximumVolume;
		localDRC.enable						= (Boolean)((UInt32)(eqPrefs->drcEnable));

		err = SndHWSetDRC ((DRCInfoPtr)&localDRC);

		err = SndHWSetOutputBiquadGroup (eqPrefs->filterCount, eqPrefs->filter[0].coefficient);
	}

	// Set the level controls to their (possibly) new min and max values
	if (drc.maximumVolume < 0) {
		minVolume = kMinimumVolume;
	} else {
		minVolume = kMinimumVolume + drc.maximumVolume;
	}
	maxVolume = kMaximumVolume + drc.maximumVolume;

	AdjustControls ();

	debugIOLog ("OutputPorts::DeviceInterruptService EXIT\n");
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will set the global Boolean dontReleaseHPMute to 'true' if
//	the layout references a system where the hardware implements a headphone
//	mute circuit that deviates from the standard TAS3001 implementation and
//	requires a behavior that the Headphone Mute remain asserted when the 
//	headphone is muted.	 This is a deviation from the standard behavior where
//	the headphone mute is released after 250 milliseconds.	Just add new 
//	'case' for each system to be excluded from the default behavior above
//	the 'case layoutP29' with no 'break' and let the code fall through to
//	the 'case layoutP29' statement.	 Standard hardware implementations that
//	adhere to the default behavior do not require any code change.	[2660341]
void AppleTexas2Audio::ExcludeHPMuteRelease (UInt32 layout) {
	switch (layout) {
		case layoutP72:		/*	Fall through to 'layoutP29' case	*/
		case layoutP29:		dontReleaseHPMute = true;			break;
		default:			dontReleaseHPMute = false;			break;
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleTexas2Audio::GetDeviceMatch (void) {
	UInt32			theDeviceMatch;

	if (TRUE == headphonesActive)
		theDeviceMatch = 2;		// headphones are connected
	else if (TRUE == dallasSpeakersConnected)
		theDeviceMatch = 4;		// headphones aren't connected and external Dallas speakers are connected
	else
		theDeviceMatch = 1;		// headphones aren't connected and external Dallas speakers aren't connected

	return theDeviceMatch;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IORegistryEntry *AppleTexas2Audio::FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSNumber				*tmpNumber;

	theEntry = NULL;
	iterator = NULL;
	FailIf (NULL == start, Exit);

	iterator = start->getChildIterator (gIOServicePlane);
	FailIf (NULL == iterator, Exit);

	while (NULL == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != NULL) {
		if (strcmp (tmpReg->getName (), name) == 0) {
			tmpNumber = OSDynamicCast (OSNumber, tmpReg->getProperty (key));
			if (NULL != tmpNumber && tmpNumber->unsigned32BitValue () == value) {
				theEntry = tmpReg;
								theEntry->retain();
			}
		}
	}

Exit:
	if (NULL != iterator) {
		iterator->release ();
	}
	return theEntry;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IORegistryEntry *AppleTexas2Audio::FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSData					*tmpData;

	theEntry = NULL;
	iterator = start->getChildIterator (gIODTPlane);
	FailIf (NULL == iterator, Exit);

	while (NULL == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != NULL) {
		tmpData = OSDynamicCast (OSData, tmpReg->getProperty (key));
		if (NULL != tmpData && tmpData->isEqualTo (value, strlen (value))) {
			theEntry = tmpReg;
		}
	}

Exit:
	if (NULL != iterator) {
		iterator->release ();
	}
	return theEntry;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::GetCustomEQCoefficients (UInt32 layoutID, UInt32 deviceID, UInt32 speakerID, EQPrefsElementPtr *filterSettings) {
	IOReturn				err;
	Boolean					found;
	UInt32					index;
	EQPrefsElementPtr		eqElementPtr;

	debug5IOLog ("GetCustomEQCoefficients (%lX, %lX, %lX, %p)\n", layoutID, deviceID, speakerID, filterSettings);
	debug2IOLog ("gEQPrefs %p\n", gEQPrefs);

	err = -50;
	FailIf (0 == layoutID, Exit);
	FailIf (NULL == filterSettings, Exit);
	FailIf (NULL == gEQPrefs, Exit);

	found = FALSE;
	eqElementPtr = NULL;
	*filterSettings = NULL;
	for (index = 0; index < gEQPrefs->eqCount && !found; index++) {
		eqElementPtr = &(gEQPrefs->eq[index]);
		debug2IOLog ("eqElementPtr %p\n", eqElementPtr);
		debug3IOLog ("index %ld, eqCount %ld\n", index, gEQPrefs->eqCount);
		debug3IOLog ("layoutID %lX, deviceID %lX, \n", eqElementPtr->layoutID, eqElementPtr->deviceID);
		debug2IOLog ("speakerID %lX\n", eqElementPtr->speakerID);

		if ((eqElementPtr->layoutID == layoutID) && (eqElementPtr->deviceID == deviceID)) {
			if (0 == speakerID) {
				found = TRUE;
			} else if (eqElementPtr->speakerID == speakerID) {
				found = TRUE;
			}
		}
	}

	if (TRUE == found) {
		*filterSettings = eqElementPtr;
		err = kIOReturnSuccess;
	}

Exit:
	if (kIOReturnSuccess != err) {
		debug2IOLog ("err %d\n", err);
	} else {
		debug2IOLog ("filterSettings %p\n", filterSettings);
	}

	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleTexas2Audio::GetDeviceID (void) {
	IORegistryEntry			*i2s;
	IORegistryEntry			*i2sa;
	IORegistryEntry			*sound;
	OSData					*tmpData;
	UInt32					*deviceID;
	UInt32					theDeviceID;

	theDeviceID = 0;

	i2s = ourProvider->getParentEntry (gIODTPlane);
	FailIf (!i2s, Exit);
	i2sa = i2s->childFromPath (ki2saEntry, gIODTPlane);
	FailIf (!i2sa, Exit);
	sound = i2sa->childFromPath (kSoundEntry, gIODTPlane);
	FailIf (!sound, Exit);

	tmpData = OSDynamicCast (OSData, sound->getProperty (kDeviceID));
	FailIf (!tmpData, Exit);
	deviceID = (UInt32*)tmpData->getBytesNoCopy ();
	if (NULL != deviceID) {
		debug2IOLog ("deviceID = %ld\n", *deviceID);
		theDeviceID = *deviceID;
	} else {
		debugIOLog ("deviceID = NULL!\n");
	}

Exit:
	return theDeviceID;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Boolean AppleTexas2Audio::HasInput (void) {
	IORegistryEntry			*i2s;
	IORegistryEntry			*i2sa;
	IORegistryEntry			*sound;
	OSData				*tmpData;
	UInt32				*numInputs;
	Boolean				hasInput;

	hasInput = false;

	i2s = ourProvider->getParentEntry (gIODTPlane);
	FailIf (!i2s, Exit);
	i2sa = i2s->childFromPath (ki2saEntry, gIODTPlane);
	FailIf (!i2sa, Exit);
	sound = i2sa->childFromPath (kSoundEntry, gIODTPlane);
	FailIf (!sound, Exit);

	tmpData = OSDynamicCast (OSData, sound->getProperty (kNumInputs));
	FailIf (!tmpData, Exit);
	numInputs = (UInt32*)tmpData->getBytesNoCopy ();
	debug2IOLog ("numInputs = %ld\n", *numInputs);
	if (*numInputs > 1) {
		hasInput = true;
		debugIOLog ("Has input!\n");
	} else {
		debugIOLog ("Doesn't have input\n");
	}
Exit:
	return hasInput;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::SetActiveOutput (UInt32 output, Boolean touchBiquad) {
	IOReturn			err;
	
	debug3IOLog ("AppleTexas2Audio::SndHWSetActiveOutput (output = %ld, %d)\n", output, touchBiquad);

	err = kIOReturnSuccess;
	if (touchBiquad)
		SetUnityGainAllPass ();
	switch (output) {
		case kSndHWOutputNone:
			SetAmplifierMuteState (kHEADPHONE_AMP, ASSERT_GPIO (hdpnActiveState));	//	mute
			SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));		//	mute
			SetAnalogPowerDownMode (kPowerDownAnalog);
			break;
		case kSndHWOutput1:
			SetAnalogPowerDownMode (kPowerNormalAnalog);
			SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));		//	mute
			SetAmplifierMuteState (kHEADPHONE_AMP, NEGATE_GPIO (hdpnActiveState));	//	unmute
			IODelay (kAmpRecoveryMuteDuration * durationMillisecond);
			break;
		case kSndHWOutput2:																//	fall through to kSndHWOutput4
		case kSndHWOutput3:																//	fall through to kSndHWOutput4
		case kSndHWOutput4:
			SetAnalogPowerDownMode (kPowerNormalAnalog);
			//	The TA1101B amplifier can 'crowbar' when inserting the speaker jack.
			//	Muting the amplifier will release it from the crowbar state.
			SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));		// mute
			IODelay (kAmpRecoveryMuteDuration * durationMillisecond);
			SetAmplifierMuteState (kHEADPHONE_AMP, ASSERT_GPIO (hdpnActiveState));	//	mute
			SetAmplifierMuteState (kSPEAKER_AMP, NEGATE_GPIO (ampActiveState));		//	unmute
			IODelay (kAmpRecoveryMuteDuration * durationMillisecond);
			if (!dontReleaseHPMute)													//	[2660341] unmute if std hw
				SetAmplifierMuteState (kHEADPHONE_AMP, NEGATE_GPIO (hdpnActiveState));	// unmute
			break;
	}

	debug2IOLog ("AppleTexas2Audio::SndHWSetActiveOutput err %d\n", err);
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTexas2Audio::SetBiquadInfoToUnityAllPass (void) {
	UInt32			index;
	
	debugIOLog ("SetBiquadInfoToUnityAllPass ()\n");
	for (index = 0; index < kNumberOfBiquadCoefficients; index++) {
		biquadGroupInfo[index++] = 1.0;				//	b0
		biquadGroupInfo[index++] = 0.0;				//	b1
		biquadGroupInfo[index++] = 0.0;				//	b2
		biquadGroupInfo[index++] = 0.0;				//	a1
		biquadGroupInfo[index] = 0.0;				//	a2
	}
	debugIOLog ("EXIT SetBiquadInfoToUnityAllPass ()\n");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will only restore unity gain all pass coefficients to the
//	biquad registers.  All other coefficients to be passed through exported
//	functions via the sound hardware plug-in manager libaray.
void AppleTexas2Audio::SetUnityGainAllPass (void) {
	UInt32			prevLoadMode;
	int				biquadRefnum;
	DRCInfo			localDRC;
	int				numBiquads;

	//	save previous load mode
	prevLoadMode = 0 == (shadowTexas2Regs.sMC1R[0] & (kFastLoad << kFL)) ? kSetNormalLoadMode : kSetFastLoadMode;
	debug3IOLog ("AppleTexas2Audio::SetUnityGainAllPass (), shadowTexas2Regs.sMC1R[0] %2X, prevLoadMode %ld\n", shadowTexas2Regs.sMC1R[0], prevLoadMode);
	
	//	Set fast load mode to pause the DSP so that as the filter
	//	coefficients are applied, the filter will not become unstable
	//	and result in output instability.
	InitEQSerialMode (kSetFastLoadMode, kDontRestoreOnNormal);

	numBiquads = kNumberOfTexas2BiquadsPerChannel;
	
	//	Set the biquad coefficients in the shadow registers to 'unity all pass' so that
	//	any future attempt to set the biquads is applied to the hardware registers (i.e.
	//	make sure that the shadow register accurately reflects the current state so that
	//	 a data compare in the future does not cause a write operation to be bypassed).
	for (biquadRefnum = 0; biquadRefnum < numBiquads; biquadRefnum++) {
		Texas2_WriteRegister (kTexas2LeftBiquad0CtrlReg  + biquadRefnum, (UInt8*)kBiquad0db, kUPDATE_ALL);
		Texas2_WriteRegister (kTexas2RightBiquad0CtrlReg + biquadRefnum, (UInt8*)kBiquad0db, kUPDATE_ALL);
	}
	SetBiquadInfoToUnityAllPass ();
	
	//	The Texas2 is set to bypass the biquad filters.
	UInt8 mcr2Data[kTexas2MC2Rwidth];
	IOReturn err = Texas2_ReadRegister( kTexas2MainCtrl2Reg, mcr2Data );
	if (kIOReturnSuccess == err) {
		mcr2Data[0] &= ~( kFilter_MASK << kAP );
		mcr2Data[0] |= ( kAllPassFilter << kAP );
		Texas2_WriteRegister( kTexas2MainCtrl2Reg, mcr2Data, kUPDATE_ALL );
	}
	InitEQSerialMode (kSetNormalLoadMode, kRestoreOnNormal);	//	go to normal load mode and restore registers after default
	
	//	Need to restore volume & mixer control registers after going to fast load mode
	localDRC.compressionRatioNumerator		= kDrcRatioNumerator;
	localDRC.compressionRatioDenominator	= kDrcRationDenominator;
	localDRC.threshold						= kDrcUnityThresholdHW;
	localDRC.maximumVolume					= kDefaultMaximumVolume;
	localDRC.enable							= false;

	SndHWSetDRC (&localDRC);
	
	//	restore previous load mode
	if (kSetFastLoadMode == prevLoadMode)
		InitEQSerialMode (kSetFastLoadMode, kDontRestoreOnNormal);

	debug3IOLog ("AppleTexas2Audio ... shadowTexas2Regs.sMC1R[0] %8X, prevLoadMode %ld\n", shadowTexas2Regs.sMC1R[0], prevLoadMode);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	When disabling Dynamic Range Compression, don't check the other elements
//	of the DRCInfo structure.  When enabling DRC, clip the compression threshold
//	to a valid range for the target hardware & validate that the compression
//	ratio is supported by the target hardware.	The maximumVolume argument
//	will dynamically apply the zero index reference point into the volume
//	gain translation table and will force an update of the volume registers.
//	This SPI is primarily in support of the TAS MANIA application and is only
//	accessible through passing a private selector to the output component
//	SoundComponentSetInfo() function.
IOReturn AppleTexas2Audio::SndHWSetDRC( DRCInfoPtr theDRCSettings ) {
	IOReturn		err;
	UInt8			regData[kTexas2DRCwidth];
	Boolean			enableUpdated;

	err = kIOReturnSuccess;
	FailWithAction( NULL == theDRCSettings, err = -50, Exit );
	FailWithAction( kDrcRatioNumerator != theDRCSettings->compressionRatioNumerator, err = -50, Exit );
	FailWithAction( kDrcRationDenominator != theDRCSettings->compressionRatioDenominator, err = -50, Exit );
	
	debug2IOLog( "[AppleTexas2Audio] __SndHWSetDRC( theDRCSettings %p )\n", theDRCSettings );
	debug3IOLog( "[AppleTexas2Audio] ... compressionRatioNumerator %ld, compressionRatioDenominator %ld\n", theDRCSettings->compressionRatioNumerator, theDRCSettings->compressionRatioDenominator );
	debug3IOLog( "[AppleTexas2Audio] ... threshold %ld, maximumVolume %ld\n", theDRCSettings->threshold, theDRCSettings->maximumVolume );
	debug2IOLog( "[AppleTexas2Audio] ... enable %d\n", theDRCSettings->enable );
	
	enableUpdated = drc.enable != theDRCSettings->enable ? true : false ;
	drc.enable = theDRCSettings->enable;

	//	The Texas2 DRC threshold has a range of 0.0 dB through -89.625 dB.  The lowest value
	//	is rounded down to -90.0 dB so that a generalized formula for calculating the hardware
	//	value can be used.  The hardware values decrement two counts for each 0.75 dB of
	//	threshold change toward greater attenuation (i.e. more negative) where a 0.0 dB setting
	//	translates to a hardware setting of #-17 (i.e. kDRCUnityThreshold).  Since the threshold
	//	is passed in as a dB X 1000 value, the threshold is divided by the step size X 1000 or
	//	750, then multiplied by the hardware decrement value of 2 and the total is subtracted
	//	from the unity threshold hardware setting.  Note that the -90.0 dB setting actually
	//	would result in a hardware setting of -89.625 dB as the hardware settings become
	//	non-linear at the very lowest value.
	
	regData[DRC_Threshold]		= (UInt8)(kDRCUnityThreshold + (kDRC_CountsPerStep * (theDRCSettings->threshold / kDRC_ThreholdStepSize)));
	regData[DRC_AboveThreshold]	= theDRCSettings->enable ? kDRCAboveThreshold3to1 : kDisableDRC ;
	regData[DRC_BelowThreshold]	= kDRCBelowThreshold1to1;
	regData[DRC_Integration]	= kDRCIntegrationThreshold;
	regData[DRC_Attack]			= kDRCAttachThreshold;
	regData[DRC_Decay]			= kDRCDecayThreshold;
	debug2IOLog( "[AppleTexas2Audio] ...  drc.compressionRatioNumerator %ld\n", drc.compressionRatioNumerator );
	debug2IOLog( "[AppleTexas2Audio] ...  drc.compressionRatioDenominator %ld\n", drc.compressionRatioDenominator );
	debug2IOLog( "[AppleTexas2Audio] ...  drc.threshold %ld\n", drc.threshold );
	debug2IOLog( "[AppleTexas2Audio] ...  drc.enable %d\n", drc.enable );
	err = Texas2_WriteRegister( kTexas2DynamicRangeCtrlReg, regData, kUPDATE_ALL );

	//	The current volume setting needs to be scaled against the new range of volume 
	//	control and applied to the hardware.
	if( drc.maximumVolume != theDRCSettings->maximumVolume || enableUpdated ) {
		drc.maximumVolume = theDRCSettings->maximumVolume;
	}
	
	drc.compressionRatioNumerator		= theDRCSettings->compressionRatioNumerator;
	drc.compressionRatioDenominator		= theDRCSettings->compressionRatioDenominator;
	drc.threshold						= theDRCSettings->threshold;
	drc.maximumVolume					= theDRCSettings->maximumVolume;

Exit:
	if( kIOReturnSuccess != err ) {
		debug2IOLog( "[AppleTexas2Audio] __SndHWSetDRC: err = %d\n", err );
	}

	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	This function does not utilize fast mode loading as to do so would
//	revert all biquad coefficients not addressed by this execution instance to
//	unity all pass.	 Expect DSP processing delay if this function is used.	It
//	is recommended that SndHWSetOutputBiquadGroup be used instead.
IOReturn AppleTexas2Audio::SndHWSetOutputBiquad( UInt32 streamID, UInt32 biquadRefNum, FourDotTwenty *biquadCoefficients )
{
	IOReturn		err;
	UInt32			coefficientIndex;
	UInt32			Texas2BiquadIndex;
	UInt32			biquadGroupIndex;
//	FourDotTwenty	coefficients;
	UInt8			Texas2Biquad[kTexas2CoefficientsPerBiquad * kTexas2NumBiquads];
	
#ifdef	kBIQUAD_VERBOSE
//	debug3IOLog( "[AppleTexas2Audio] __SndHWSetOutputBiquad( '%0.4s', %0.2d )\n", &streamID, biquadRefNum );
#endif
	err = kIOReturnSuccess;
	FailWithAction( kTexas2MaxBiquadRefNum < biquadRefNum || NULL == biquadCoefficients, err = -50, Exit );
	FailWithAction( kStreamStereo != streamID && kStreamFrontLeft != streamID && kStreamFrontRight != streamID, err = -50, Exit );
	
	Texas2BiquadIndex = 0;
	biquadGroupIndex = biquadRefNum * kTexas2CoefficientsPerBiquad;
	if( kStreamFrontRight == streamID )
		biquadGroupIndex += kNumberOfBiquadCoefficientsPerChannel;
#ifdef	kEQ_VERBOSE
	debug3IOLog( "[AppleTexas2Audio] %0.4s %d : ", &streamID, biquadRefNum );
#endif
	for( coefficientIndex = 0; coefficientIndex < kTexas2CoefficientsPerBiquad; coefficientIndex++ )
	{
		// commented out because in this code biquadCoefficients is a double, not a FourDotTwenty
		// this saved the biquad info so that it could be read back later for verification (because you can't read the values from the hardware -- you have to remember what you wrote)
//		biquadGroupInfo[biquadGroupIndex] = biquadCoefficients[coefficientIndex];
//		if( kStreamStereo == streamID )
//			biquadGroupInfo[biquadGroupIndex + kNumberOfBiquadCoefficientsPerChannel] = biquadCoefficients[coefficientIndex];
//		biquadGroupIndex++;
		
#if	kEQ_VERBOSE
//		if( 0.0 <= biquadCoefficients[coefficientIndex] )
//			IOLog( " +%3.10f ", biquadCoefficients[coefficientIndex] );
//		else
//			IOLog( " %3.10f ", biquadCoefficients[coefficientIndex] );
#endif
//		DoubleToFourDotTwenty( biquadCoefficients[coefficientIndex], &coefficients );
		Texas2Biquad[Texas2BiquadIndex++] = biquadCoefficients[coefficientIndex].integerAndFraction1;
		Texas2Biquad[Texas2BiquadIndex++] = biquadCoefficients[coefficientIndex].fraction2;
		Texas2Biquad[Texas2BiquadIndex++] = biquadCoefficients[coefficientIndex].fraction3;
	}
	debugIOLog( "\n" );
	
	err = SetOutputBiquadCoefficients( streamID, biquadRefNum, Texas2Biquad );
	//	If the Texas2 is not set to use the biquad filter coefficients then enable
	//	the filters.
	UInt8 mcr2Data[kTexas2MC2Rwidth];
	Texas2_ReadRegister( kTexas2MainCtrl2Reg, mcr2Data );
	if ( ( mcr2Data[0] & ( kAllPassFilter << kAP )) == ( kAllPassFilter << kAP )) {
		mcr2Data[0] &= ~( kFilter_MASK << kAP );
		mcr2Data[0] |= ( kNormalFilter << kAP );
		Texas2_WriteRegister( kTexas2MainCtrl2Reg, mcr2Data, kUPDATE_ALL );
	}
Exit:
	if( kIOReturnSuccess != err )
		debug4IOLog( "[AppleTexas2Audio] err %d = __SndHWSetOutputBiquad( '%4.4s', %ld )\n", err, (char*)&streamID, biquadRefNum );
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::SndHWSetOutputBiquadGroup( UInt32 biquadFilterCount, FourDotTwenty *biquadCoefficients )
{
	UInt32			index;
	IOReturn		err;
	
	FailWithAction( 0 == biquadFilterCount || NULL == biquadCoefficients, err = -50, Exit );
//	debug3IOLog( "[AppleTexas2Audio] __SndHWSetOutputBiquadGroup( %d, %2.6f )\n", biquadFilterCount, biquadCoefficients );
	err = kIOReturnSuccess;
	InitEQSerialMode( kSetFastLoadMode, kDontRestoreOnNormal );
	index = 0;
	do {
		if( index >= ( biquadFilterCount / 2 ) ) {
			err = SndHWSetOutputBiquad( kStreamFrontRight, index - ( biquadFilterCount / 2 ), biquadCoefficients );
		} else {
			err = SndHWSetOutputBiquad( kStreamFrontLeft, index, biquadCoefficients );
		}
		index++;
		biquadCoefficients += kNumberOfCoefficientsPerBiquad;
	} while ( ( index < biquadFilterCount ) && ( kIOReturnSuccess == err ) );
	InitEQSerialMode( kSetNormalLoadMode, kRestoreOnNormal );
	//	If the Texas2 is not set to use the biquad filter coefficients then enable
	//	the filters.
	UInt8 mcr2Data[kTexas2MC2Rwidth];
	Texas2_ReadRegister( kTexas2MainCtrl2Reg, mcr2Data );
	if ( ( mcr2Data[0] & ( kAllPassFilter << kAP )) == ( kAllPassFilter << kAP )) {
		mcr2Data[0] &= ~( kFilter_MASK << kAP );
		mcr2Data[0] |= ( kNormalFilter << kAP );
		Texas2_WriteRegister( kTexas2MainCtrl2Reg, mcr2Data, kUPDATE_ALL );
	}
Exit:
	debug2IOLog( "[AppleTexas2Audio] err = %d\n", err );
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexas2Audio::SetOutputBiquadCoefficients( UInt32 streamID, UInt32 biquadRefNum, UInt8 *biquadCoefficients )
{
	IOReturn			err;
	
	debug4IOLog ( "[AppleTexas2Audio] SetOutputBiquadCoefficients( '%4.4s', %ld, %p )\n", (char*)&streamID, biquadRefNum, biquadCoefficients );
	err = kIOReturnSuccess;
	FailWithAction ( kTexas2MaxBiquadRefNum < biquadRefNum || NULL == biquadCoefficients, err = -50, Exit );
	FailWithAction ( kStreamStereo != streamID && kStreamFrontLeft != streamID && kStreamFrontRight != streamID, err = -50, Exit );

	switch ( biquadRefNum )
	{
		case kBiquadRefNum_0:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = Texas2_WriteRegister( kTexas2RightBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_1:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = Texas2_WriteRegister( kTexas2RightBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_2:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = Texas2_WriteRegister( kTexas2RightBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_3:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = Texas2_WriteRegister( kTexas2RightBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_4:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = Texas2_WriteRegister( kTexas2RightBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_5:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = Texas2_WriteRegister( kTexas2RightBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_6:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = Texas2_WriteRegister( kTexas2LeftBiquad6CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight:	err = Texas2_WriteRegister( kTexas2RightBiquad6CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = Texas2_WriteRegister( kTexas2LeftBiquad6CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = Texas2_WriteRegister( kTexas2RightBiquad6CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
	}

Exit:
	if( kIOReturnSuccess != err )
		debug5IOLog( "[AppleTexas2Audio] err %d = SetOutputBiquadCoefficients( '%4.4s', %ld, %p )\n", err, (char*)&streamID, biquadRefNum, biquadCoefficients );
	return err;
}

#pragma mark +I2C FUNCTIONS
// Taken from PPCDACA.cpp

// --------------------------------------------------------------------------
// Method: getI2CPort
//
// Purpose:
//		  returns the i2c port to use for the audio chip.
UInt32 AppleTexas2Audio::getI2CPort()
{
	if(ourProvider) {
		OSData *t;
		
		t = OSDynamicCast(OSData, ourProvider->getProperty("AAPL,i2c-port-select"));	// we don't need a port select on Tangent, but look anyway
		if (t != NULL) {
			UInt32 myPort = *((UInt32*)t->getBytesNoCopy());
			return myPort;
		}
//		else
//			debugIOLog( "AppleTexas2Audio::getI2CPort missing property port, but that's not necessarily a problem\n");
	}

	return 0;
}

// --------------------------------------------------------------------------
// Method: openI2C
//
// Purpose:
//		  opens and sets up the i2c bus
bool AppleTexas2Audio::openI2C()
{
	FailIf (NULL == interface, Exit);

	// Open the interface and sets it in the wanted mode:
	FailIf (!interface->openI2CBus (getI2CPort()), Exit);
	interface->setStandardSubMode ();

	// have to turn on polling or it doesn't work...need to figure out why, but not today.
	interface->setPollingMode (true);

	return true;

Exit:
	return false;
}


// --------------------------------------------------------------------------
// Method: closeI2C
//
// Purpose:
//		  closes the i2c bus
void AppleTexas2Audio::closeI2C ()
{
	// Closes the bus so other can access to it:
	interface->closeI2CBus ();
}

// --------------------------------------------------------------------------
// Method: findAndAttachI2C
//
// Purpose:
//	 Attaches to the i2c interface:
bool AppleTexas2Audio::findAndAttachI2C(IOService *provider)
{
	const OSSymbol	*i2cDriverName;
	IOService		*i2cCandidate;

	// Searches the i2c:
	i2cDriverName = OSSymbol::withCStringNoCopy("PPCI2CInterface.i2c-mac-io");
	i2cCandidate = waitForService(resourceMatching(i2cDriverName));
	//interface = OSDynamicCast(PPCI2CInterface, i2cCandidate->getProperty(i2cDriverName));
	interface = (PPCI2CInterface*)i2cCandidate->getProperty(i2cDriverName);

	if (interface == NULL) {
		debugIOLog("AppleTexas2Audio::findAndAttachI2C can't find the i2c in the registry\n");
		return false;
	}

	// Make sure that we hold the interface:
	interface->retain();

	return true;
}

// --------------------------------------------------------------------------
// Method: detachFromI2C
//
// Purpose:
//	 detaches from the I2C
bool AppleTexas2Audio::detachFromI2C(IOService* /*provider*/)
{
	if (interface) {
		//delete interface;
		interface->release();
		interface = 0;
	}
		
	return (true);
}

// Generic INLINEd methods to access to registers:
// ===============================================
inline UInt32 AppleTexas2Audio::ReadWordLittleEndian(void *address, UInt32 offset)
{
#if 0
	UInt32 *realAddress = (UInt32*)(address) + offset;
	UInt32 value = *realAddress;
	UInt32 newValue =
		((value & 0x000000FF) << 16) |
		((value & 0x0000FF00) << 8) |
		((value & 0x00FF0000) >> 8) |
		((value & 0xFF000000) >> 16);

	return (newValue);
#else
	return OSReadLittleInt32(address, offset);
#endif
}

inline void AppleTexas2Audio::WriteWordLittleEndian(void *address, UInt32 offset, UInt32 value)
{
#if 0
	UInt32 *realAddress = (UInt32*)(address) + offset;
	UInt32 newValue =
		((value & 0x000000FF) << 16) |
		((value & 0x0000FF00) << 8) |
		((value & 0x00FF0000) >> 8) |
		((value & 0xFF000000) >> 16);

	*realAddress = newValue;
#else
	OSWriteLittleInt32(address, offset, value);
#endif	  
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline void AppleTexas2Audio::I2SSetSerialFormatReg(UInt32 value)
{
	WriteWordLittleEndian(soundConfigSpace, kI2SSerialFormatOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline void AppleTexas2Audio::I2SSetDataWordSizeReg(UInt32 value)
{
	WriteWordLittleEndian(soundConfigSpace, kI2SFrameMatchOffset, value);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::I2SGetDataWordSizeReg(void)
{
	return ReadWordLittleEndian(soundConfigSpace, kI2SFrameMatchOffset);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Access to Keylargo registers:
inline void AppleTexas2Audio::KLSetRegister(void *klRegister, UInt32 value)
{
	UInt32 *reg = (UInt32*)klRegister;
	*reg = value;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::KLGetRegister(void *klRegister)
{
	UInt32 *reg = (UInt32*)klRegister;
	return (*reg);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
inline UInt32 AppleTexas2Audio::I2SGetIntCtlReg()
{
	return ReadWordLittleEndian(soundConfigSpace, kI2SIntCtlOffset);
}

// --------------------------------------------------------------------------
// Method: setSampleRate
//
// Purpose:
//		  Sets the sample rate on the I2S bus
bool AppleTexas2Audio::setSampleParameters(UInt32 sampleRate, UInt32 mclkToFsRatio)
{
	UInt32	mclkRatio;
	UInt32	reqMClkRate;

	mclkRatio = mclkToFsRatio;			// remember the MClk ratio required

	if ( mclkRatio == 0 )																				   // or make one up if MClk not required
		mclkRatio = 64;				// use 64 x ratio since that will give us the best characteristics
	
	reqMClkRate = sampleRate * mclkRatio;	// this is the required MClk rate

	// look for a source clock that divides down evenly into the MClk
	if ((kClock18MHz % reqMClkRate) == 0) {
		// preferential source is 18 MHz
		clockSource = kClock18MHz;
	}
	else if ((kClock45MHz % reqMClkRate) == 0) {
		// next check 45 MHz clock
		clockSource = kClock45MHz;
	}
	else if ((kClock49MHz % reqMClkRate) == 0) {
		// last, try 49 Mhz clock
		clockSource = kClock49MHz;
	}
	else {
		debugIOLog("AppleTexas2Audio::setSampleParameters Unable to find a suitable source clock (no globals changes take effect)\n");
		return false;
	}

	// get the MClk divisor
	debug3IOLog("AppleTexas2Audio:setSampleParameters %ld / %ld =", (UInt32)clockSource, (UInt32)reqMClkRate); 
	mclkDivisor = clockSource / reqMClkRate;
	debug2IOLog("%ld\n", mclkDivisor);
	switch (serialFormat)					// SClk depends on format
	{
		case kSndIOFormatI2SSony:
		case kSndIOFormatI2S64x:
			sclkDivisor = mclkRatio / k64TicksPerFrame; // SClk divisor is MClk ratio/64
			break;
		case kSndIOFormatI2S32x:
			sclkDivisor = mclkRatio / k32TicksPerFrame; // SClk divisor is MClk ratio/32
			break;
		default:
			debugIOLog("AppleTexas2Audio::setSampleParameters Invalid serial format\n");
			return false;
			break;
	}

	return true;
 }


// --------------------------------------------------------------------------
// Method: setSerialFormatRegister
//
// Purpose:
//		  Set global values to the serial format register
void AppleTexas2Audio::setSerialFormatRegister(ClockSource clockSource, UInt32 mclkDivisor, UInt32 sclkDivisor, SoundFormat serialFormat)
{
	UInt32	regValue = 0;

	debug5IOLog("AppleTexas2Audio::SetSerialFormatRegister(%d,%d,%d,%d)\n",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);

	switch ((int)clockSource)
	{
		case kClock18MHz:			regValue = kClockSource18MHz;			break;
		case kClock45MHz:			regValue = kClockSource45MHz;			break;
		case kClock49MHz:			regValue = kClockSource49MHz;			break;
		default:
			debug5IOLog("AppleTexas2Audio::SetSerialFormatRegister(%d,%d,%d,%d): Invalid clock source\n",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);
			break;
	}

	switch (mclkDivisor)
	{
		case 1:			regValue |= kMClkDivisor1;																break;
		case 3:			regValue |= kMClkDivisor3;																break;
		case 5:			regValue |= kMClkDivisor5;																break;
		default:		regValue |= (((mclkDivisor / 2) - 1) << kMClkDivisorShift) & kMClkDivisorMask;			break;
	}

	switch ((int)sclkDivisor)
	{
		case 1:			regValue |= kSClkDivisor1;																break;
		case 3:			regValue |= kSClkDivisor3;																break;
		default:		regValue |= (((sclkDivisor / 2) - 1) << kSClkDivisorShift) & kSClkDivisorMask;			break;
	}
	regValue |= kSClkMaster;										// force master mode

	switch (serialFormat)
	{
		case kSndIOFormatI2SSony:			regValue |= kSerialFormatSony;			break;
		case kSndIOFormatI2S64x:			regValue |= kSerialFormat64x;			break;
		case kSndIOFormatI2S32x:			regValue |= kSerialFormat32x;			break;
		default:	
			debug5IOLog("AppleTexas2Audio::SetSerialFormatRegister(%d,%d,%d,%d): Invalid serial format\n",(int)clockSource, (int)mclkDivisor, (int)sclkDivisor, (int)serialFormat);
			break;
	}

	// Set up the data word size register for stereo (input and output)
	I2SSetDataWordSizeReg (0x02000200);

	// Set up the serial format register
	I2SSetSerialFormatReg(i2sSerialFormat);
}

// --------------------------------------------------------------------------
// Method: setHWSampleRate
//
// Purpose:
//		  Gets the sample rate and makes it in a format that is compatible
//		  with the adac register. The funtion returns false if it fails.
bool AppleTexas2Audio::setHWSampleRate(UInt rate)
{
	UInt32 dacRate = 0;

	debug2IOLog("AppleTexas2Audio::setHWSampleRate(%d)\n", rate);

	switch (rate) {
		case 44100:					// 32 kHz - 48 kHz
			dacRate = kSRC_48SR_REG;
			break;

		default:
			debugIOLog("AppleTexas2Audio::setHWSampleRate, supports only 44100 Hz (for now)\n");
			break;
	}

	// manipulate serial format register to change the mclock value for the new sample rate.
	return true;	// 44.1kHz is already set
}

// --------------------------------------------------------------------------
// Method: frameRate
//
// Purpose:
//		  returns the frame rate as in the registry, if it is
//		  not found in the registry, it returns the default value.
#define kCommonFrameRate 44100

UInt32 AppleTexas2Audio::frameRate(UInt32 index)
{
	if(ourProvider) {
		OSData *t;

		t = OSDynamicCast(OSData, ourProvider->getProperty("sample-rates"));
		if (t != NULL) {
			UInt32 *fArray = (UInt32*)(t->getBytesNoCopy());

			if ((fArray != NULL) && (index < fArray[0])){
				// I could do >> 16, but in this way the code is portable and
				// however any decent compiler will recognize this as a shift
				// and do the right thing.
				UInt32 fR = fArray[index + 1] / (UInt32)65536;

				debug2IOLog( "AppleTexas2Audio::frameRate (%ld)\n",	fR);
				return fR;
			}
		}
	}

	return (UInt32)kCommonFrameRate;
}






