/*
 *	AppleTexasAudio.cpp
 *	Apple02Audio
 *
 *	Created by nthompso on Tue Jul 03 2001.
 *
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 *
 * This file contains a template for an Apple02Audio based driver.
 * The driver is derived from the Apple02Audio class.
 *
 */
 
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOCommandGate.h>
#include <UserNotification/KUNCUserNotifications.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/audio/IOAudioEngine.h>
#include "AudioI2SHardwareConstants.h"

#include "AppleTexasAudio.h"

#include "AppleTexasEQPrefs.cpp"

#define super Apple02Audio
//#define durationMillisecond 1000	// number of microseconds in a millisecond

OSDefineMetaClassAndStructors(AppleTexasAudio, Apple02Audio)

// Globals in this file
EQPrefsPtr		gEQPrefs = &theEQPrefs;
extern uid_t	console_user;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#pragma mark +UNIX LIKE FUNCTIONS

// init, free, probe are the "Classical Unix driver functions" that you'll like as not find
// in other device drivers.	 Note that there are no start and stop methods.	 The code for start 
// effectively moves to the sndHWInitialize routine.  Also note that the initHardware method 
// effectively does very little other than calling the inherited method, which in turn calls
// sndHWInitialize, so all of the init code should be in the sndHWInitialize routine.

// ::init()
// call into superclass and initialize.
bool AppleTexasAudio::init(OSDictionary *properties)
{
	debugIOLog("+ AppleTexasAudio::init\n");
	if (!super::init(properties))
		return false;

	gVolLeft = 0;
	gVolRight = 0;
	gVolMuteActive = false;
	gModemSoundActive = false;

	debugIOLog("- AppleTexasAudio::init\n");
	return true;
}

// ::free
// call inherited free
void AppleTexasAudio::free()
{
	IOWorkLoop				*workLoop;

	debugIOLog("+ AppleTexasAudio::free\n");

	CLEAN_RELEASE(hdpnMuteRegMem);
	CLEAN_RELEASE(ampMuteRegMem);
	CLEAN_RELEASE(hwResetRegMem);
	CLEAN_RELEASE(headphoneExtIntGpioMem);
	CLEAN_RELEASE(dallasExtIntGpioMem);
	CLEAN_RELEASE(audioI2SControl);
	if (NULL != ioBaseAddressMemory) {
		ioBaseAddressMemory->release();
	}
	
	if (NULL != ioClockBaseAddressMemory) {
		ioClockBaseAddressMemory->release();
	}

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

		if ( NULL != deferHeadphoneHandlerTimer ) { workLoop->removeEventSource ( deferHeadphoneHandlerTimer ); }	//	[3103075]	rbm		11/27/2002
	}

	publishResource (gAppleAudioVideoJackStateKey, NULL);
	CLEAN_RELEASE(headphoneIntEventSource);
	CLEAN_RELEASE(dallasIntEventSource);

	super::free();
	debugIOLog("- AppleTexasAudio::free\n");
}

// ::probe
// called at load time, to see if this driver really does match with a device.	In our
// case we check the registry to ensure we are loading on the appropriate hardware.
IOService* AppleTexasAudio::probe(IOService *provider, SInt32 *score)
{
	// Finds the possible candidate for sound, to be used in
	// reading the caracteristics of this hardware:
	IORegistryEntry *sound = 0;
	debugIOLog("+ AppleTexasAudio::probe\n");

	super::probe(provider, score);
	*score = kIODefaultProbeScore;
	sound = provider->childFromPath("sound", gIODTPlane);
	// we are on a new world : the registry is assumed to be fixed
	if(sound) {
		OSData *tmpData;

		tmpData = OSDynamicCast(OSData, sound->getProperty(kModelPropName));
		if(tmpData) {
			if(tmpData->isEqualTo(kTexasModelName, sizeof(kTexasModelName) -1)) {
				*score = *score+1;
				debugIOLog("++ AppleTexasAudio::probe increasing score\n");
				return(this);
			} else {
				debugIOLog ("++ AppleTexasAudio::probe, didn't find what we were looking for\n");
			}
		}
		sound->release ();
	}

	debugIOLog("- AppleTexasAudio::probe\n");
	return (0);
}

// ::initHardware
// Don't do a whole lot in here, but do call the inherited inithardware method.
// in turn this is going to call sndHWInitialize to perform initialization.	 All
// of the code to initialize and start the device needs to be in that routine, this 
// is kept as simple as possible.
bool AppleTexasAudio::initHardware(IOService *provider)
{
	bool myreturn = true;

	DEBUG_IOLOG("+ AppleTexasAudio::initHardware\n");
	
	// calling the superclasses initHarware will indirectly call our
	// sndHWInitialize() method.  So we don't need to do a whole lot 
	// in this function.
	super::initHardware(provider);
	
	DEBUG_IOLOG("- AppleTexasAudio::initHardware\n");
	return myreturn;
}

// --------------------------------------------------------------------------
// Method: timerCallback
//
// Purpose:
//		  This is a static method that gets called from a timer task at regular intervals
//		  Generally we do not want to do a lot of work here, we simply turn around and call
//		  the appropriate method to perform out periodic tasks.

void AppleTexasAudio::timerCallback(OSObject *target, IOAudioDevice *device)
{
// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("+ AppleTexasAudio::timerCallback\n");
	AppleTexasAudio *			templateDriver;
	
	templateDriver = OSDynamicCast (AppleTexasAudio, target);

	if (templateDriver) {
		templateDriver->checkStatus(false);
	}
// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("- AppleTexasAudio::timerCallback\n");
	return;
}



// --------------------------------------------------------------------------
// Method: checkStatus
//
// Purpose:
//		 poll the detects, note this should prolly be done with interrupts rather
//		 than by polling if interrupts are supported

void AppleTexasAudio::checkStatus(bool force)
{
// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("+ AppleTexasAudio::checkStatus\n");

// probably don't want this on since we will get called a lot...
//	  DEBUG_IOLOG("- AppleTexasAudio::checkStatus\n");
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

// ::sndHWInitialize
// hardware specific initialization needs to be in here, together with the code
// required to start audio on the device.
//
//	There are three sections of memory mapped I/O that are directly accessed by the Apple02Audio.  These
//	include the GPIOs, I2S DMA Channel Registers and I2S control registers.  They fall within the memory map 
//	as follows:
//	~                              ~
//	|______________________________|
//	|                              |
//	|         I2S Control          |
//	|______________________________|	<-	soundConfigSpace = ioBase + i2s0BaseOffset ...OR... ioBase + i2s1BaseOffset
//	|                              |
//	~                              ~
//	~                              ~
//	|______________________________|
//	|                              |
//	|       I2S DMA Channel        |
//	|______________________________|	<-	i2sDMA = ioBase + i2s0_DMA ...OR... ioBase + i2s1_DMA
//	|                              |
//	~                              ~
//	~                              ~
//	|______________________________|
//	|            FCRs              |
//	|            GPIO              |	<-	gpio = ioBase + gpioOffsetAddress
//	|         ExtIntGPIO           |	<-	fcr = ioBase + fcrOffsetAddress
//	|______________________________|	<-	ioConfigurationBaseAddress
//	|                              |
//	~                              ~
//
//	The I2S DMA Channel is mapped in by the Apple02DBDMAAudioDMAEngine.  Only the I2S control registers are 
//	mapped in by the AudioI2SControl.  The Apple I/O Configuration Space (i.e. FCRs, GPIOs and ExtIntGPIOs)
//	are mapped in by the subclass of Apple02Audio.  The FCRs must also be mapped in by the AudioI2SControl
//	object as the init method must enable the I2S I/O Module for which the AudioI2SControl object is
//	being instantiated for.
//
void	AppleTexasAudio::sndHWInitialize(IOService *provider)
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
	IORegistryEntry			*hardwareReset;
	OSData					*tmpData;
	IOMemoryMap				*map;
	UInt32					*hdpnMuteGpioAddr;
	UInt32					*ampMuteGpioAddr;
	UInt32					*hwResetGpioAddr;
	UInt32					*headphoneExtIntGpioAddr;
	UInt32					*dallasExtIntGpioAddr;
	UInt32					*i2cAddrPtr;
	UInt32					*tmpPtr;
	UInt32					loopCnt;
	UInt8					data[kBIQwidth];						// space for biggest register size
	UInt8					curValue;
	bool					hasInput;

	DEBUG_IOLOG("+ AppleTexasAudio::sndHWInitialize\n");

	ourProvider = provider;
	hasInput = (bool) HasInput ();
	savedNanos = 0;
	previousPowerState = kIOAudioDeviceActive;

	i2s = NULL;
	macio = NULL;
	gpio = NULL;
	i2c = NULL;

	i2s = ourProvider->getParentEntry (gIODTPlane);
	FailIf (!i2s, Exit);
	macio = i2s->getParentEntry (gIODTPlane);
	FailIf (!macio, Exit);
	gpio = macio->childFromPath (kGPIODTEntry, gIODTPlane);
	FailIf (!gpio, Exit);
	i2c = macio->childFromPath (kI2CDTEntry, gIODTPlane);
	setProperty (kSpeakerConnectError, speakerConnectFailed);

	//	Determine which systems to exclude from the default behavior of releasing the headphone
	//	mute after 200 milliseconds delay [2660341].
	tmpData = OSDynamicCast (OSData, macio->getProperty (kDeviceIDPropName));
	FailIf (!tmpData, Exit);
	deviceID = (UInt32)tmpData->getBytesNoCopy ();

	// get the physical address of the i2c cell that the sound chip (Digital EQ, "deq") is connected to.
	deq = i2c->childFromPath (kDigitalEQDTEntry, gIODTPlane);
	FailIf (!deq, Exit);
	tmpData = OSDynamicCast (OSData, deq->getProperty (kI2CAddress));
	deq->release ();
	FailIf (!tmpData, Exit);
	i2cAddrPtr = (UInt32*)tmpData->getBytesNoCopy ();
	DEQAddress = *i2cAddrPtr;
	DEQAddress = DEQAddress >> 1;	// have to shift down because I2C driver will shift up on writes

	// get the physical address of the gpio pin for setting the headphone mute
	headphoneMute = FindEntryByProperty (gpio, kAudioGPIO, kHeadphoneAmpEntry);
	FailIf (!headphoneMute, Exit);
	tmpData = OSDynamicCast (OSData, headphoneMute->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	hdpnMuteGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
	tmpData = OSDynamicCast (OSData, headphoneMute->getProperty (kAudioGPIOActiveState));
	tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
	hdpnActiveState = *tmpPtr;

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	hdpnMuteRegMem = IODeviceMemory::withRange (*hdpnMuteGpioAddr, sizeof (UInt8));
	map = hdpnMuteRegMem->map (0);
	hdpnMuteGpio = (UInt8*)map->getVirtualAddress ();

	intSource = 0;

	// get the interrupt provider for the Dallas speaker insertion interrupt
	intSource = FindEntryByProperty (gpio, kOneWireBus, kSpeakerID);

	if (NULL != intSource) {
		dallasIntProvider = OSDynamicCast (IOService, intSource);
		FailIf (!dallasIntProvider, Exit);

		// get the active state of the dallas speaker inserted pin
		tmpData = OSDynamicCast (OSData, intSource->getProperty (kAudioGPIOActiveState));
		if (!tmpData) {
			dallasInsertedActiveState = 1;
		} else {
			tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
			dallasInsertedActiveState = *tmpPtr;
		}

		// get the physical address of the pin for detecting the dallas speaker insertion/removal
		tmpData = OSDynamicCast (OSData, intSource->getProperty (kAAPLAddress));
		FailIf (!tmpData, Exit);
		dallasExtIntGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();

		// take the hard coded memory address that's in the boot rom, and convert it a virtual address
		dallasExtIntGpioMem = IODeviceMemory::withRange (*dallasExtIntGpioAddr, sizeof (UInt8));
		map = dallasExtIntGpioMem->map (0);
		dallasExtIntGpio = (UInt8*)map->getVirtualAddress ();

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

	// get the physical address of the pin for detecting the headphone insertion/removal
	tmpData = OSDynamicCast (OSData, intSource->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	headphoneExtIntGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();

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
	tmpData = OSDynamicCast (OSData, ampMute->getProperty (kAudioGPIOActiveState));
	tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
	ampActiveState = *tmpPtr;

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	ampMuteRegMem = IODeviceMemory::withRange (*ampMuteGpioAddr, sizeof (UInt8));
	map = ampMuteRegMem->map (0);
	ampMuteGpio = (UInt8*)map->getVirtualAddress ();

	// get the physical address of the gpio pin for setting the hardware reset
	hardwareReset = FindEntryByProperty (gpio, kAudioGPIO, kHWResetEntry);
	FailIf (!hardwareReset, Exit);
	tmpData = OSDynamicCast (OSData, hardwareReset->getProperty (kAAPLAddress));
	FailIf (!tmpData, Exit);
	hwResetGpioAddr = (UInt32*)tmpData->getBytesNoCopy ();
	tmpData = OSDynamicCast (OSData, hardwareReset->getProperty (kAudioGPIOActiveState));
	tmpPtr = (UInt32*)tmpData->getBytesNoCopy ();
	hwResetActiveState = *tmpPtr;

	// take the hard coded memory address that's in the boot rom, and convert it a virtual address
	hwResetRegMem = IODeviceMemory::withRange (*hwResetGpioAddr, sizeof (UInt8));
	map = hwResetRegMem->map (0);
	hwResetGpio = (UInt8*)map->getVirtualAddress ();

	FailIf (!findAndAttachI2C (provider), Exit);

	layoutID = GetDeviceID ();
	debug2IOLog ("layoutID = %ld\n", layoutID);
	ExcludeHPMuteRelease (layoutID);

	i2sSerialFormat = ( kClockSource45MHz | ( 1 << kMClkDivisorShift ) | ( 1 << kSClkDivisorShift ) | kSClkMaster | kSerialFormat64x );

	drc.compressionRatioNumerator		= kDrcRatioNumerator;
	drc.compressionRatioDenominator		= kDrcRationDenominator;
	drc.threshold						= kDrcThresholdMax;
	drc.maximumVolume					= kDefaultMaximumVolume;
	drc.enable							= false;

	//	Initialize the TAS3001C as follows:
	//		Mode:						normal
	//		SCLK:						64 fs
	//		input serial mode:			i2s
	//		output serial mode:			i2s
	//		serial word length:			16 bits
	//		Dynamic range control:		disabled
	//		Volume (left & right):		muted
	//		Treble / Bass:				unity
	//		Biquad filters:				unity
	data[0] = ( kNormalLoad << kFL ) | ( k64fs << kSC ) | TAS_I2S_MODE | ( TAS_WORD_LENGTH << kW0 );
	TAS3001C_WriteRegister( kMainCtrlReg, data, kUPDATE_SHADOW );	//	default to normal load mode, 16 bit I2S

	data[0] = ( kDrcDisable << kEN ) | ( kCompression3to1 << kCR );
	data[1] = kDefaultCompThld;
	TAS3001C_WriteRegister( kDynamicRangeCtrlReg, data, kUPDATE_SHADOW );

	for( loopCnt = 0; loopCnt < kVOLwidth; loopCnt++ )				//	init to volume = muted
		data[loopCnt] = 0;
	TAS3001C_WriteRegister( kVolumeCtrlReg, data, kUPDATE_SHADOW );

	data[0] = 0x72;													//	treble = unity 0.0 dB
	TAS3001C_WriteRegister( kTrebleCtrlReg, data, kUPDATE_SHADOW );

	data[0] = 0x3E;													//	bass = unity = 0.0 dB
	TAS3001C_WriteRegister( kBassCtrlReg, data, kUPDATE_SHADOW );

	data[0] = 0x10;													//	output mixer to unity = 0.0 dB
	data[1] = 0x00;
	data[2] = 0x00;
	TAS3001C_WriteRegister( kMixer1CtrlReg, data, kUPDATE_SHADOW );

	data[0] = 0x00;													//	call progress mixer to mute = -70.0 dB
	data[1] = 0x00;
	data[2] = 0x00;
	TAS3001C_WriteRegister( kMixer2CtrlReg, data, kUPDATE_SHADOW );

	for( loopCnt = 1; loopCnt < kBIQwidth; loopCnt++ )				//	all biquads to unity gain all pass mode
		data[loopCnt] = 0x00;
	data[0] = 0x10;

	TAS3001C_WriteRegister( kLeftBiquad0CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kLeftBiquad1CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kLeftBiquad2CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kLeftBiquad3CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kLeftBiquad4CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kLeftBiquad5CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad0CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad1CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad2CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad3CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad4CtrlReg, data, kUPDATE_SHADOW );
	TAS3001C_WriteRegister( kRightBiquad5CtrlReg, data, kUPDATE_SHADOW );

	// All this config should go in a single method: 
	map = provider->mapDeviceMemoryWithIndex (Apple02DBDMAAudioDMAEngine::kDBDMADeviceIndex);
	FailIf (!map, Exit);
    // the i2s stuff is in a separate class.  make an instance of the class and store
    AudioI2SInfo tempInfo;
    tempInfo.map = map;
    tempInfo.i2sSerialFormat = kSerialFormat64x;					//	[3060321]	rbm	2 Oct 2002
    
    // create an object, this will get inited.
    audioI2SControl = AudioI2SControl::create(&tempInfo) ;
    FailIf (NULL == audioI2SControl, Exit);
    audioI2SControl->retain();

	err = TAS3001C_Initialize( kFORCE_RESET_SETUP_TIME );			//	reset the TAS3001C and flush the shadow contents to the HW

	dallasDriver = NULL;
	dallasDriverNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleDallasDriver"), (IOServiceNotificationHandler)&DallasDriverPublished, this);
	if (NULL != dallasDriver)
		dallasDriverNotifier->remove ();

Exit:
	if (NULL != gpio)
		gpio->release ();
	if (NULL != i2c)
		i2c->release ();

	DEBUG_IOLOG("- AppleTexasAudio::sndHWInitialize\n");
	return;
}

void AppleTexasAudio::sndHWPostDMAEngineInit (IOService *provider) {
	IOWorkLoop				*workLoop;

	DEBUG_IOLOG("+ AppleTexasAudio::sndHWPostDMAEngineInit\n");

	if (NULL != driverDMAEngine)
		driverDMAEngine->setSampleLatencies (kTexasOutputSampleLatency, kTexasInputSampleLatency);

	workLoop = getWorkLoop();
	FailIf (NULL == workLoop, Exit);

	if (NULL != headphoneIntProvider) {
		headphoneIntEventSource = IOInterruptEventSource::interruptEventSource (this,
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

 		deferHeadphoneHandlerTimer = IOTimerEventSource::timerEventSource (this, deferHeadphoneHandler);	//	[3103075]	rbm		11/27/2002
		if (NULL != deferHeadphoneHandlerTimer) { workLoop->addEventSource (deferHeadphoneHandlerTimer); }

		dallasIntEventSource = IOFilterInterruptEventSource::filterInterruptEventSource (this,
																			dallasInterruptHandler, 
																			interruptFilter,
																			dallasIntProvider, 
																			0);
		FailIf (NULL == dallasIntEventSource, Exit);
		workLoop->addEventSource (dallasIntEventSource);

		if (NULL != outputSelector) {
			outputSelector->addAvailableSelection(kIOAudioOutputPortSubTypeExternalSpeaker, "ExtSpeakers");
		}
	}

	if ( TRUE == IsSpeakerConnected() ) {
		if (NULL != dallasDriver) {
			UInt8				bEEPROM[32];
			Boolean				result;
		
			debugIOLog ("sndHWPostDMAEngineInit: About to get the speaker ID\n");
			speakerID = 0;
			familyID = 0;
			// dallasIntEventSource isn't enabled yet, so we don't have to disable it before this call
			result = dallasDriver->getSpeakerID (bEEPROM);
			if (TRUE == result) {
				// The speakers have been successfully probed
				dallasSpeakersProbed = TRUE;
				familyID = bEEPROM[0];
				speakerID = bEEPROM[1];
				debug2IOLog ("speakerID = %ld\n", speakerID);
			} else {
				debugIOLog ("speakerID unknown, probe failed\n");
			}
		}
	} else {
		speakerID = 0;
		familyID = 0;
		dallasSpeakersProbed = FALSE;
	}

	if (FALSE == IsHeadphoneConnected ()) {
		if ( NULL == dallasInterruptHandler ) {						//	[3117811]
			SetActiveOutput (kSndHWOutput2, kBiquadUntouched);
		}
		if (TRUE == hasVideo) {
			// Tell the video driver about the jack state change in case a video connector was plugged in
			publishResource (gAppleAudioVideoJackStateKey, kOSBooleanFalse);
		}
		if (NULL != dallasIntProvider) {
			// Set the correct EQ
			DallasInterruptHandlerTimer (this, 0);
		} else {
			DeviceInterruptService ();
		}
	} else {
		if (NULL != headphoneIntProvider) {
			// Set amp mutes accordingly
			deferHeadphoneHandler (this, deferHeadphoneHandlerTimer);
		}
	}

	if (NULL == outVolRight && NULL != outVolLeft) {
		// If they are running mono at boot time, set the right channel's last value to an illegal value
		// so it will come up in stereo and center balanced if they plug in speakers or headphones later.
		lastRightVol = kOUT_OF_BOUNDS_VOLUME_VALUE;
		lastLeftVol = outVolLeft->getIntValue ();
	}

	if (NULL != headphoneIntEventSource)
		headphoneIntEventSource->enable ();
	if (NULL != dallasIntEventSource)
		dallasIntEventSource->enable ();

Exit:
	DEBUG_IOLOG("- AppleTexasAudio::sndHWPostDMAEngineInit\n");
	return;
}

UInt32 AppleTexasAudio::sndHWGetCurrentSampleFrame (void) {
	return audioI2SControl->GetFrameCountReg ();
}

void AppleTexasAudio::sndHWSetCurrentSampleFrame (UInt32 value) {
	audioI2SControl->SetFrameCountReg (value);
}

UInt32	AppleTexasAudio::sndHWGetInSenseBits(void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetInSenseBits\n");
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetInSenseBits\n");
	return 0;	  
}

// we can't read the registers back, so return the value in the shadow reg.
UInt32	AppleTexasAudio::sndHWGetRegister(UInt32 regNum)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetRegister\n");
	
	UInt32	returnValue = 0;
	
	 return returnValue;
}

// set the reg over i2c and make sure the value is cached in the shadow reg so we can "get it back"
IOReturn  AppleTexasAudio::sndHWSetRegister(UInt32 regNum, UInt32 val)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetRegister\n");
	IOReturn myReturn = kIOReturnSuccess;
	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetRegister\n");
	return(myReturn);
}

#pragma mark +HARDWARE IO ACTIVATION
/************************** Manipulation of input and outputs ***********************/
/********(These functions are enough to implement the simple UI policy)**************/

UInt32	AppleTexasAudio::sndHWGetActiveOutputExclusive(void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetActiveOutputExclusive\n");
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetActiveOutputExclusive\n");
	return 0;
}

IOReturn   AppleTexasAudio::sndHWSetActiveOutputExclusive(UInt32 outputPort )
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetActiveOutputExclusive\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetActiveOutputExclusive\n");
	return(myReturn);
}

UInt32	AppleTexasAudio::sndHWGetActiveInputExclusive(void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetActiveInputExclusive\n");
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetActiveInputExclusive\n");
	return 0;
}

IOReturn   AppleTexasAudio::sndHWSetActiveInputExclusive(UInt32 input )
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetActiveInputExclusive\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetActiveInputExclusive\n");
	return(myReturn);
}

#pragma mark +CONTROL FUNCTIONS
// control function
bool AppleTexasAudio::sndHWGetSystemMute(void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetSystemMute\n");
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetSystemMute\n");
	return (gVolMuteActive);
}

IOReturn AppleTexasAudio::setModemSound(bool state) {
	UInt8 data[3];
	IOReturn myReturn = kIOReturnSuccess;

	if(gModemSoundActive == state) 
		goto EXIT;

	if(state) {	   // we turned it on
		data[0] = 0x10;
	} else {
		data[0] = 0x00;
	}					  
																							
	data[1] = 0x00;
	data[2] = 0x00;
	myReturn = TAS3001C_WriteRegister( kMixer2CtrlReg, data, kUPDATE_HW );
	gModemSoundActive = state;

EXIT:
	return(myReturn);
}

IOReturn AppleTexasAudio::sndHWSetSystemMute(bool mutestate)
{
	IOReturn						result;

	result = kIOReturnSuccess;

	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetSystemMute\n");

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

	DEBUG_IOLOG ("- AppleTexasAudio::sndHWSetSystemMute\n");
	return (result);
}

bool AppleTexasAudio::sndHWSetSystemVolume(UInt32 leftVolume, UInt32 rightVolume)
{
	bool					result;

	result = false;

	debug3IOLog ("+ AppleTexasAudio::sndHWSetSystemVolume (left: %ld, right %ld)\n", leftVolume, rightVolume);
	gVolLeft = leftVolume;
	gVolRight = rightVolume;

	if (NULL != outVolLeft) {
		lastLeftVol = gVolLeft;
	}
	if (NULL != outVolRight) {
		lastRightVol = gVolRight;
	}

	result = SetVolumeCoefficients (volumeTable[(UInt32)gVolLeft], volumeTable[(UInt32)gVolRight]);

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetSystemVolume\n");
	return (result == kIOReturnSuccess);
}

IOReturn AppleTexasAudio::sndHWSetSystemVolume(UInt32 value)
{
	DEBUG2_IOLOG("+ AppleTexasAudio::sndHWSetSystemVolume (vol: %ld)\n", value);
	
	IOReturn myReturn = kIOReturnError;
		
	// just call the default function in this class with the same val for left and right.
	if( true == sndHWSetSystemVolume( value, value ))
	{
		myReturn = kIOReturnSuccess;
	}

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetSystemVolume\n");
	return(myReturn);
}

IOReturn AppleTexasAudio::sndHWSetPlayThrough(bool playthroughstate)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetPlayThrough\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetPlayThrough\n");
	return(myReturn);
}

IOReturn AppleTexasAudio::sndHWSetSystemInputGain(UInt32 leftGain, UInt32 rightGain) 
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetPlayThrough\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetPlayThrough\n");
	return(myReturn);
}

// You either have only a master volume control, or you have both volume controls.
// This function must be called on the workloop as it will be calling back to IOAudioFamily
IOReturn AppleTexasAudio::AdjustControls (void) {
	IOFixed							mindBVol;
	IOFixed							maxdBVol;
	Boolean							mustUpdate;

	FailIf (NULL == driverDMAEngine, Exit);
	mustUpdate = FALSE;

	mindBVol = volumedBTable[minVolume];
	maxdBVol = volumedBTable[maxVolume];

	//	Must update if any of the following conditions exist:
	//	1.	No master volume control exists AND the master volume is the target
	//	2.	The master volume control exists AND the master volume is not the target
	//	3.	the minimum or maximum dB volume setting for the left volume control changes
	//	4.	the minimum or maximum dB volume setting for the right volume control changes
	
	if ((NULL == outVolMaster && TRUE == useMasterVolumeControl) ||
		(NULL != outVolMaster && FALSE == useMasterVolumeControl) ||
		(NULL != outVolLeft && outVolLeft->getMinValue () != minVolume) ||
		(NULL != outVolLeft && outVolLeft->getMaxValue () != maxVolume) ||
		(NULL != outVolRight && outVolRight->getMinValue () != minVolume) ||
		(NULL != outVolRight && outVolRight->getMaxValue () != maxVolume)) {
		mustUpdate = TRUE;
	}

	if (TRUE == mustUpdate) {
		debug3IOLog ("AdjustControls: mindBVol = %lx, maxdBVol = %lx\n", mindBVol, maxdBVol);
	
		driverDMAEngine->pauseAudioEngine ();
		driverDMAEngine->beginConfigurationChange ();
	
		if (TRUE == useMasterVolumeControl) {
			// We have only the master volume control (possibly not created yet) and have to remove the other volume controls (possibly don't exist)
			if (NULL == outVolMaster) {
				// remove the existing left and right volume controls
				if (NULL != outVolLeft) {
					lastLeftVol = outVolLeft->getIntValue ();
					driverDMAEngine->removeDefaultAudioControl (outVolLeft);
					outVolLeft = NULL;
				}
		
				if (NULL != outVolRight) {
					lastRightVol = outVolRight->getIntValue ();
					driverDMAEngine->removeDefaultAudioControl (outVolRight);
					outVolRight = NULL;
				}
	
				// Create the master control
				outVolMaster = IOAudioLevelControl::createVolumeControl((lastLeftVol + lastRightVol) / 2, minVolume, maxVolume, mindBVol, maxdBVol,
													kIOAudioControlChannelIDAll,
													kIOAudioControlChannelNameAll,
													kOutVolMaster, 
													kIOAudioControlUsageOutput);
	
				if (NULL != outVolMaster) {
					driverDMAEngine->addDefaultAudioControl(outVolMaster);
					outVolMaster->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
					outVolMaster->flushValue ();
				}
			}
		} else {
			// or we have both controls (possibly not created yet) and we have to remove the master volume control (possibly doesn't exist)
			if (NULL == outVolLeft) {
				// Have to create the control again...
				if (lastLeftVol > kMAXIMUM_LEGAL_VOLUME_VALUE && NULL != outVolMaster) {
					lastLeftVol = outVolMaster->getIntValue ();
				}
				outVolLeft = IOAudioLevelControl::createVolumeControl (lastLeftVol, kMinimumVolume, kMaximumVolume, mindBVol, maxdBVol,
													kIOAudioControlChannelIDDefaultLeft,
													kIOAudioControlChannelNameLeft,
													kOutVolLeft,
													kIOAudioControlUsageOutput);
				if (NULL != outVolLeft) {
					driverDMAEngine->addDefaultAudioControl (outVolLeft);
					outVolLeft->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				}
			}
			
			if (NULL == outVolRight) {
				// Have to create the control again...
				if (lastRightVol > kMAXIMUM_LEGAL_VOLUME_VALUE && NULL != outVolMaster) {
					lastRightVol = outVolMaster->getIntValue ();
				}
				outVolRight = IOAudioLevelControl::createVolumeControl (lastRightVol, kMinimumVolume, kMaximumVolume, mindBVol, maxdBVol,
													kIOAudioControlChannelIDDefaultRight,
													kIOAudioControlChannelNameRight,
													kOutVolRight,
													kIOAudioControlUsageOutput);
				if (NULL != outVolRight) {
					driverDMAEngine->addDefaultAudioControl (outVolRight);
					outVolRight->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
				}
			}
	
			if (NULL != outVolMaster) {
				driverDMAEngine->removeDefaultAudioControl (outVolMaster);
				outVolMaster = NULL;
			}
	
			if (NULL != outVolLeft && NULL != outVolRight) {
				outVolLeft->flushValue ();
				outVolRight->flushValue ();
			}
		}
	
		if (NULL != outVolMaster) {
			outVolMaster->setMinValue (minVolume);
			outVolMaster->setMinDB (mindBVol);
			outVolMaster->setMaxValue (maxVolume);
			outVolMaster->setMaxDB (maxdBVol);
			if (outVolMaster->getIntValue () > maxVolume) {
				outVolMaster->setValue (maxVolume);
			}
		}
	
		if (NULL != outVolLeft) {
			outVolLeft->setMinValue (minVolume);
			outVolLeft->setMinDB (mindBVol);
			outVolLeft->setMaxValue (maxVolume);
			outVolLeft->setMaxDB (maxdBVol);
			if (outVolLeft->getIntValue () > maxVolume) {
				outVolLeft->setValue (maxVolume);
			}
		}
	
		if (NULL != outVolRight) {
			outVolRight->setMinValue (minVolume);
			outVolRight->setMinDB (mindBVol);
			outVolRight->setMaxValue (maxVolume);
			outVolRight->setMaxDB (maxdBVol);
			if (outVolRight->getIntValue () > maxVolume) {
				outVolRight->setValue (maxVolume);
			}
		}
	
		driverDMAEngine->completeConfigurationChange ();
		driverDMAEngine->resumeAudioEngine ();
	}

Exit:
	return kIOReturnSuccess;
}

#pragma mark +INDENTIFICATION

// ::sndHWGetType
// Identification - the only thing this driver supports is the DACA3550 part, return that.
UInt32 AppleTexasAudio::sndHWGetType(void )
{
	UInt32				returnValue;

	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetType\n");
 
	// in AudioHardwareConstants.h need to set up a constant for the hardware type.
	returnValue = kSndHWTypeTumbler;

	DEBUG_IOLOG ("- AppleTexasAudio::sndHWGetType\n");
	return returnValue ;
}

// ::sndHWGetManufactuer
// return the detected part's manufacturer.	 I think Daca is a single sourced part
// from Micronas Intermetall.  Always return just that.
UInt32 AppleTexasAudio::sndHWGetManufacturer(void )
{
	UInt32				returnValue;

	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetManufacturer\n");

	// in AudioHardwareConstants.h need to set up a constant for the part manufacturer.
	returnValue = kSndHWManfTI ;
	
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetManufacturer\n");
	return returnValue ;
}

#pragma mark +DETECT ACTIVATION & DEACTIVATION
// ::setDeviceDetectionActive
// turn on detection, TODO move to superclass?? 
void AppleTexasAudio::setDeviceDetectionActive(void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::setDeviceDetectionActive\n");
	mCanPollStatus = true ;
	
	DEBUG_IOLOG("- AppleTexasAudio::setDeviceDetectionActive\n");
	return ;
}

// ::setDeviceDetectionInActive
// turn off detection, TODO move to superclass?? 
void AppleTexasAudio::setDeviceDetectionInActive(void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::setDeviceDetectionInActive\n");
	mCanPollStatus = false ;
	
	DEBUG_IOLOG("- AppleTexasAudio::setDeviceDetectionInActive\n");
	return ;
}

#pragma mark +POWER MANAGEMENT
// Power Management

IOReturn AppleTexasAudio::sndHWSetPowerState (IOAudioDevicePowerState theState) {
	IOReturn							result;

	debug2IOLog ("+ AppleTexasAudio::sndHWSetPowerState (%d)\n", theState);

	result = kIOReturnSuccess;
	switch (theState) {
		case kIOAudioDeviceActive:
			if (kIOAudioDeviceIdle == previousPowerState) {
				result = performDeviceIdleWake ();
			} else if (kIOAudioDeviceSleep == previousPowerState) {
				result = performDeviceWake ();
			} else {
				debugIOLog ("Trying to wake Texas hardware, but it's already awake.\n");
			}
			break;
		case kIOAudioDeviceIdle:
			result = performDeviceIdleSleep ();
			break;
		case kIOAudioDeviceSleep:
			result = performDeviceSleep ();
			break;
	}
	previousPowerState = theState;

	debugIOLog ("- AppleTexasAudio::sndHWSetPowerState\n");
	return result;
}

IOReturn AppleTexasAudio::performDeviceIdleSleep () {
    IOService *				keyLargo;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	debugIOLog ("+ AppleTexasAudio::performDeviceIdleSleep\n");

	keyLargo = NULL;

	// Mute the amps to avoid pops and clicks...
	SetActiveOutput (kSndHWOutputNone, kBiquadUntouched);

	// ...then hold the RESET line...
	GpioWrite (hwResetGpio, ASSERT_GPIO (hwResetActiveState));

	// ...wait for the part to settle...
	IODelay (100);

    keyLargo = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));
    
    if (NULL != keyLargo) {
		funcSymbolName = OSSymbol::withCString ( "keyLargo_powerI2S" );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		// ...and turn off the i2s clocks...
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)false, (void *)0, 0, 0);		break;
			case kUseI2SCell1:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)false, (void *)1, 0, 0);		break;
		}
		funcSymbolName->release ();															//	[3323977]
	}
Exit:
	debugIOLog ("- AppleTexasAudio::performDeviceIdleSleep\n");
	return kIOReturnSuccess;
}
	
IOReturn AppleTexasAudio::performDeviceIdleWake () {
    IOService *				keyLargo;
	IOReturn				err;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	debugIOLog ("+ AppleTexasAudio::performDeviceIdleWake\n");

	keyLargo = NULL;
    keyLargo = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));
    
    if (NULL != keyLargo) {
		funcSymbolName = OSSymbol::withCString ( "keyLargo_powerI2S" );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		// Turn on the i2s clocks...
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)true, (void *)0, 0, 0);	break;
			case kUseI2SCell1:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)true, (void *)1, 0, 0);	break;
		}
		funcSymbolName->release ();															//	[3323977]
	}

	// ...wait for the clocks to settle...
	IODelay (100);	

	// ...and then release the RESET line...
	GpioWrite (hwResetGpio, NEGATE_GPIO (hwResetActiveState));

	// ...wait for the part to wake up...
	IOSleep (10);

	// ...then bring everything back up the way it should be.
	err = TAS3001C_Initialize (kFORCE_RESET_SETUP_TIME);			//	reset the TAS3001C and flush the shadow contents to the HW

	if ( TRUE == IsSpeakerConnected() ) {
		if (NULL != dallasDriver && FALSE == dallasSpeakersProbed) {
			UInt8				bEEPROM[32];
			Boolean				result;
		
			debugIOLog ("performDeviceIdleWake: About to get the speaker ID\n");
			speakerID = 0;
			familyID = 0;
			dallasIntEventSource->disable ();
			result = dallasDriver->getSpeakerID (bEEPROM);
			dallasIntEventSource->enable ();
			if (TRUE == result) {
				// The speakers have been successfully probed
				dallasSpeakersProbed = TRUE;
				familyID = bEEPROM[0];
				speakerID = bEEPROM[1];
				debug2IOLog ("speakerID = %ld\n", speakerID);
			} else {
				debugIOLog ("speakerID unknown, probe failed\n");
			}
		}
	} else {
		speakerID = 0;
		familyID = 0;
		dallasSpeakersProbed = FALSE;
	}

	if (FALSE == IsHeadphoneConnected ()) {
		if ( NULL == dallasIntProvider ) {			//		[3117811]
			SetActiveOutput (kSndHWOutput2, kBiquadUntouched);
		}
		if (TRUE == hasVideo && FALSE != headphonesConnected) {
			// Tell the video driver about the jack state change in case a video connector was plugged in
			publishResource (gAppleAudioVideoJackStateKey, kOSBooleanFalse);
		}
		if (NULL != dallasIntProvider) {
			// Set the correct EQ
			DallasInterruptHandlerTimer (this, 0);
		} else {
			DeviceInterruptService ();
		}
	} else {
		if (NULL != headphoneIntProvider) {
			// Set amp mutes accordingly
			deferHeadphoneHandler (this, deferHeadphoneHandlerTimer);
		}
	}
Exit:
	debugIOLog ("- AppleTexasAudio::performDeviceIdleWake\n");
	return err;
}

IOReturn AppleTexasAudio::performDeviceSleep () {
    IOService *				keyLargo;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	debugIOLog ("+ AppleTexasAudio::performDeviceSleep\n");

	keyLargo = NULL;

	// Mute the amps to avoid pops and clicks...
	SetActiveOutput (kSndHWOutputNone, kBiquadUntouched);

	// ...then hold the RESET line...
	GpioWrite (hwResetGpio, ASSERT_GPIO (hwResetActiveState));

    keyLargo = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));
    
    if (NULL != keyLargo) {
		funcSymbolName = OSSymbol::withCString ( "keyLargo_powerI2S" );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		// ...and turn off the i2s clocks...
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)false, (void *)0, 0, 0);	break;
			case kUseI2SCell1:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)false, (void *)1, 0, 0);	break;
		}
		funcSymbolName->release ();															//	[3323977]
	}
Exit:
	debugIOLog ("- AppleTexasAudio::performDeviceSleep\n");
	return kIOReturnSuccess;
}
	
IOReturn AppleTexasAudio::performDeviceWake () {
    IOService *				keyLargo;
	IOReturn				err;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	debugIOLog ("+ AppleTexasAudio::performDeviceWake\n");

	// Mute the amps to avoid pops and clicks...
	SetActiveOutput (kSndHWOutputNone, kBiquadUntouched);

	// ...then hold the RESET line...
	GpioWrite (hwResetGpio, ASSERT_GPIO (hwResetActiveState));

	keyLargo = NULL;
    keyLargo = IOService::waitForService (IOService::serviceMatching ("KeyLargo"));
    
    if (NULL != keyLargo) {
		funcSymbolName = OSSymbol::withCString ( "keyLargo_powerI2S" );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		// ...turn on the i2s clocks...
		switch ( i2SInterfaceNumber ) {
			case kUseI2SCell0:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)true, (void *)0, 0, 0);	break;
			case kUseI2SCell1:	keyLargo->callPlatformFunction (funcSymbolName, false, (void *)true, (void *)1, 0, 0);	break;
		}
		funcSymbolName->release ();															//	[3323977]
	}

	// ...wait for the clocks to settle...
	IODelay (100);

	// ...then release the RESET line...
	GpioWrite (hwResetGpio, NEGATE_GPIO (hwResetActiveState));

	// ...wait for the part to wake up...
	IOSleep (150);

	// ...then bring everything back up the way it should be.
	err = TAS3001C_Initialize (kFORCE_RESET_SETUP_TIME);			//	reset the TAS3001C and flush the shadow contents to the HW

	if ( TRUE == IsSpeakerConnected() ) {
		if (NULL != dallasDriver && FALSE == dallasSpeakersProbed && TRUE == IsSpeakerConnected ()) {
			UInt8				bEEPROM[32];
			Boolean				result;
		
			debugIOLog ("performDeviceWake: About to get the speaker ID\n");
			speakerID = 0;
			familyID = 0;
			dallasIntEventSource->disable ();
			result = dallasDriver->getSpeakerID (bEEPROM);
			dallasIntEventSource->enable ();
			if (TRUE == result) {
				// The speakers have been successfully probed
				dallasSpeakersProbed = TRUE;
				familyID = bEEPROM[0];
				speakerID = bEEPROM[1];
				debug2IOLog ("speakerID = %ld\n", speakerID);
			} else {
				debugIOLog ("speakerID unknown, probe failed\n");
			}
		}
	} else {
		speakerID = 0;
		familyID = 0;
		dallasSpeakersProbed = FALSE;
	}

	if (FALSE == IsHeadphoneConnected ()) {
		if ( NULL == dallasIntProvider ) {			//		[3117811]
			SetActiveOutput (kSndHWOutput2, kBiquadUntouched);
		}
		if (TRUE == hasVideo && FALSE != headphonesConnected) {
			// Tell the video driver about the jack state change in case a video connector was plugged in
			publishResource (gAppleAudioVideoJackStateKey, kOSBooleanFalse);
		}
		if (NULL != dallasIntProvider) {
			// Set the correct EQ
			DallasInterruptHandlerTimer (this, 0);
		} else {
			DeviceInterruptService ();
		}
	} else {
		if (NULL != headphoneIntProvider) {
			// Set amp mutes accordingly
			deferHeadphoneHandler (this, deferHeadphoneHandlerTimer);
		}
	}

Exit:
	debugIOLog ("- AppleTexasAudio::performDeviceWake\n");
	return err;
}

// ::sndHWGetConnectedDevices
// TODO: Move to superclass
UInt32 AppleTexasAudio::sndHWGetConnectedDevices(
	void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetConnectedDevices\n");
   UInt32	returnValue = currentDevices;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetConnectedDevices\n");
	return returnValue ;
}

UInt32 AppleTexasAudio::sndHWGetProgOutput(
	void)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWGetProgOutput\n");
	DEBUG_IOLOG("- AppleTexasAudio::sndHWGetProgOutput\n");
	return 0;
}

IOReturn AppleTexasAudio::sndHWSetProgOutput(
	UInt32 outputBits)
{
	DEBUG_IOLOG("+ AppleTexasAudio::sndHWSetProgOutput\n");

	IOReturn myReturn = kIOReturnSuccess;

	DEBUG_IOLOG("- AppleTexasAudio::sndHWSetProgOutput\n");
	return(myReturn);
}

#pragma mark +INTERRUPT HANDLERS
Boolean AppleTexasAudio::IsSpeakerConnected (void) {
	UInt8						dallasSenseContents;
	Boolean						connection;

	connection = FALSE;
	if (NULL != dallasIntProvider) {
		dallasSenseContents = *(dallasExtIntGpio);

		debug3IOLog ("dallasExtIntGpio = %p, dallasSenseContents = 0x%X\n", dallasExtIntGpio, dallasSenseContents);
		if ((dallasSenseContents & (1 << 1)) == (dallasInsertedActiveState << 1)) {
			debugIOLog ("gpio says dallas speakers are connected\n");
			connection = TRUE;
		} else {
			debugIOLog ("gpio says dallas speakers are NOT connected\n");
			connection = FALSE;
		}
	}

	return connection;
}

//	======================================================================================
//	This timer is used to defer a headphone interrupt until the audio system has completed
//	a transition to the wake state.  The timer will re-queue itself if the audio system
//	has not reached the wake state.	[3103075]	rbm		11/27/2002
void AppleTexasAudio::deferHeadphoneHandler (OSObject *owner, IOTimerEventSource *sender) {
    AbsoluteTime				fireTime;
    UInt64						nanos;
    AppleTexasAudio *			device;
	
	device = OSDynamicCast (AppleTexasAudio, owner);
	FailIf (NULL == device, Exit);

	if ( kIOAudioDeviceSleep != device->ourPowerState ) {
		//	Audio system is awake so handle the interrupt normally.	[3103075]
		device->RealHeadphoneInterruptHandler (0, 0);
	} else {
		//	Audio system is not awake so defer the interrupt until audio is awake.	[3103075]
		clock_get_uptime (&fireTime);
		absolutetime_to_nanoseconds (fireTime, &nanos);
		nanos += kDeferInsertionDelayNanos;			// Schedule 250 milliseconds in the future... 
		if (NULL != device->deferHeadphoneHandlerTimer && NULL != sender) {
			nanoseconds_to_absolutetime (nanos, &fireTime);
			sender->cancelTimeout ();
			sender->wakeAtTime (fireTime);
		}
	}
Exit:
	return;
}

//	======================================================================================
//	Defers the invoking of the Dallas one-wireª protocol until the Vdd has stabilized.
//	The timer is also used to defer a dallas interrupt until the audio system has completed
//	a transition to the wake state.  The timer will re-queue itself if the audio system
//	has not reached the wake state.	[3103075]	rbm		11/27/2002
void AppleTexasAudio::DallasInterruptHandlerTimer (OSObject *owner, IOTimerEventSource *sender) {
    AppleTexasAudio *			device;
    IOCommandGate *				cg;
	AbsoluteTime				currTime;
	OSNumber *					activeOutput;
    AbsoluteTime				fireTime;
    UInt64						nanos;

	device = OSDynamicCast (AppleTexasAudio, owner);
	FailIf (NULL == device, Exit);

	if ( kIOAudioDeviceActive == device->ourPowerState ) {
		//	Audio system is awake so handle the interrupt normally.	[3103075]
		device->dallasSpeakersConnected = device->IsSpeakerConnected ();
		debug2IOLog ("dallas speakers connected = %d\n", (unsigned int)device->dallasSpeakersConnected);
	
		if (TRUE == device->dallasSpeakersProbed && FALSE == device->dallasSpeakersConnected) {
			// They've unplugged the dallas speakers, so we'll need to check them out if they get plugged back in.
			device->dallasSpeakersProbed = FALSE;
		}
	
		if (FALSE == device->IsHeadphoneConnected ()) {
			// Set the proper EQ
			cg = device->getCommandGate ();
			if (NULL != cg) {
				cg->runAction (DeviceInterruptServiceAction);
			}
	
			device->SetActiveOutput (kSndHWOutput2, kBiquadUntouched);
			clock_get_uptime (&currTime);
			absolutetime_to_nanoseconds (currTime, &device->savedNanos);
	
			if (TRUE == device->dallasSpeakersConnected) {
				if (NULL != device->outputSelector) {
					activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeExternalSpeaker, 32);
					device->outputSelector->hardwareValueChanged (activeOutput);
				}
			} else {
				if (NULL != device->outputSelector) {
					activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeInternalSpeaker, 32);
					device->outputSelector->hardwareValueChanged (activeOutput);
				}
			}
		}
	
		if (NULL != device->dallasIntEventSource) {
			device->dallasIntEventSource->enable();
		}
	} else {
		//	Audio system is not awake so defer the interrupt until audio is awake.	[3103075]
		if (NULL != device->dallasHandlerTimer) {
			clock_get_uptime (&fireTime);
			absolutetime_to_nanoseconds (fireTime, &nanos);
			nanos += kInsertionDelayNanos;	// Schedule 4s in the future... 
	
			nanoseconds_to_absolutetime (nanos, &fireTime);
			if ( NULL != sender ) {
				sender->cancelTimeout ();
				sender->wakeAtTime (fireTime);
			}
		}
	}

Exit:
	return;
}

//	======================================================================================================
// Set a flag to say if the dallas speakers are plugged in or not so we know which EQ to use.
void AppleTexasAudio::RealDallasInterruptHandler (IOInterruptEventSource *source, int count) {
    AbsoluteTime				fireTime;
    UInt64						nanos;

	// call DallasInterruptHandlerTimer in a bit to check for the dallas rom (and complete speaker insertion).
	if (NULL != dallasHandlerTimer) {
		clock_get_uptime (&fireTime);
		absolutetime_to_nanoseconds (fireTime, &nanos);
		nanos += kInsertionDelayNanos;	// Schedule 4s in the future... 

		nanoseconds_to_absolutetime (nanos, &fireTime);
		dallasHandlerTimer->cancelTimeout ();
		dallasHandlerTimer->wakeAtTime (fireTime);
	}

	return;
}

//	======================================================================================================
//	Put changes in RealDallasInterruptHandler, not in here because this is a static member and the
//	RealDallasInterruptHandler is in virtual space!!!
//	This function is invoked from the hardware interrupt handler:
//		gpio service provider
//			dallasInterruptHandler	<<---------	YOU ARE HERE!
//				RealDallasInterruptHandler
//					DallasInterruptTimerHandler
void AppleTexasAudio::dallasInterruptHandler(OSObject *owner, IOInterruptEventSource *source, int count) {
	AbsoluteTime 			currTime;
	UInt64 					currNanos;
	AppleTexasAudio *		appleTexasAudio;

	appleTexasAudio = OSDynamicCast (AppleTexasAudio, owner);
	FailIf (!appleTexasAudio, Exit);

	// Need this disable for when we call dallasInterruptHandler instead of going through the interrupt filter
	appleTexasAudio->dallasIntEventSource->disable();

	clock_get_uptime (&currTime);
	absolutetime_to_nanoseconds (currTime, &currNanos);

	if ((currNanos - appleTexasAudio->savedNanos) > 10000000) {
		appleTexasAudio->RealDallasInterruptHandler (source, count);
	} else { 
		appleTexasAudio->dallasIntEventSource->enable();
	}

Exit:
	return;
}

// static "action" function to connect to our object
// return TRUE if you want the handler function to be called, or FALSE if you don't want it to be called.
bool AppleTexasAudio::interruptFilter(OSObject *owner, IOFilterInterruptEventSource *src)
{
	AppleTexasAudio* self = (AppleTexasAudio*) owner;

	self->dallasIntEventSource->disable();
	return (true);
}

// This is called to tell the user that they may not have plugged their speakers in all the way.
void AppleTexasAudio::DisplaySpeakersNotFullyConnected (OSObject *owner, IOTimerEventSource *sender) {
	AppleTexasAudio *		appleTexasAudio;
    IOCommandGate *			cg;
	AbsoluteTime			currTime;
	UInt32					deviceID;
	UInt8					bEEPROM[32];
	Boolean					result;

	debugIOLog ("+ DisplaySpeakersNotFullyConnected\n");

	appleTexasAudio = OSDynamicCast (AppleTexasAudio, owner);
	FailIf (!appleTexasAudio, Exit);

	if (0 == console_user) {
		appleTexasAudio->notifierHandlerTimer->setTimeoutMS (kNotifyTimerDelay);	// No one logged in yet (except maybe root) reset the timer to fire later. 
	} else {
		if (appleTexasAudio->doneWaiting == FALSE) { 
			// The next time this function is called we'll check the state and display the dialog as needed
			appleTexasAudio->notifierHandlerTimer->setTimeoutMS (kUserLoginDelay);	// Someone has logged in. Delay the notifier so it does not apear behind the login screen.
			appleTexasAudio->doneWaiting = TRUE;
		} else {
			deviceID = appleTexasAudio->GetDeviceMatch ();
			if (kExternalSpeakersActive == deviceID) {
				if (NULL != appleTexasAudio->dallasDriver) {
					appleTexasAudio->dallasIntEventSource->disable ();
					result = appleTexasAudio->dallasDriver->getSpeakerID (bEEPROM);
					appleTexasAudio->dallasIntEventSource->enable ();
					clock_get_uptime (&currTime);
					absolutetime_to_nanoseconds (currTime, &appleTexasAudio->savedNanos);

					if (FALSE == result && TRUE == appleTexasAudio->IsSpeakerConnected() ) {	// FALSE == failure for DallasDriver	[3103075]	rbm	12/2/2002
						KUNCUserNotificationDisplayNotice (
						0,		// Timeout in seconds
						0,		// Flags (for later usage)
						"",		// iconPath (not supported yet)
						"",		// soundPath (not supported yet)
						"/System/Library/Extensions/Apple02Audio.kext",		// localizationPath
						"HeaderOfDallasPartialInsert",		// the header
						"StringOfDallasPartialInsert",
						"ButtonOfDallasPartialInsert"); 
	
						IOLog ("The device plugged into the Apple speaker mini-jack cannot be recognized.\n");
						IOLog ("Remove the plug from the jack. Then plug it back in and make sure it is fully inserted.\n");
					} else {
						// Speakers are fully plugged in now, so load the proper EQ for them
						appleTexasAudio->dallasIntEventSource->disable ();
						cg = appleTexasAudio->getCommandGate ();
						if (NULL != cg) {
							cg->runAction (DeviceInterruptServiceAction);
						}
						appleTexasAudio->dallasIntEventSource->enable ();
						clock_get_uptime (&currTime);
						absolutetime_to_nanoseconds (currTime, &appleTexasAudio->savedNanos);
					}
				}
			}
		}
	}

Exit:
	debugIOLog ("- DisplaySpeakersNotFullyConnected\n");
    return;
}

Boolean AppleTexasAudio::IsHeadphoneConnected (void) {
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

// make sure we're on the command gate because DeviceInterruptService will make calls to IOAudioFamily
IOReturn AppleTexasAudio::DeviceInterruptServiceAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
	AppleTexasAudio *		appleTexasAudio;

	appleTexasAudio = OSDynamicCast (AppleTexasAudio, owner);
	FailIf (!appleTexasAudio, Exit);
	appleTexasAudio->DeviceInterruptService ();

Exit:
	return kIOReturnSuccess;
}

void AppleTexasAudio::RealHeadphoneInterruptHandler (IOInterruptEventSource *source, int count) {
    IOCommandGate *				cg;
	OSNumber *					activeOutput;

	//	Audio system is awake so handle the interrupt normally.	[3103075]
	SetActiveOutput (kSndHWOutputNone, kBiquadUntouched);

	cg = getCommandGate ();
	if (NULL != cg) {
		cg->runAction (DeviceInterruptServiceAction);
	}

	headphonesConnected = IsHeadphoneConnected ();

	if (TRUE == headphonesConnected) {
		SetActiveOutput (kSndHWOutput1, kBiquadUntouched);
		if (NULL != outputSelector) {
			activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeHeadphones, 32);
			outputSelector->hardwareValueChanged (activeOutput);
		}
	} else {
		SetActiveOutput (kSndHWOutput2, kBiquadUntouched);
		if (NULL != outputSelector) {
			if (FALSE == IsSpeakerConnected()) {
				activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeInternalSpeaker, 32);
			} else {
				activeOutput = OSNumber::withNumber (kIOAudioOutputPortSubTypeExternalSpeaker, 32);
			}
			outputSelector->hardwareValueChanged (activeOutput);
		}
	}

	if (TRUE == hasVideo) {
		// Tell the video driver about the jack state change in case a video connector was plugged in
		publishResource (gAppleAudioVideoJackStateKey, headphonesConnected ? kOSBooleanTrue : kOSBooleanFalse);
	}
}

void AppleTexasAudio::headphoneInterruptHandler (OSObject *owner, IOInterruptEventSource *source, int count) {
	AppleTexasAudio *		appleTexasAudio;

	appleTexasAudio = OSDynamicCast (AppleTexasAudio, owner);
	FailIf (!appleTexasAudio, Exit);

	appleTexasAudio->deferHeadphoneHandler (appleTexasAudio, appleTexasAudio->deferHeadphoneHandlerTimer);

Exit:
	return;
}

#if 0
//	===========================================================================================
//	Override the Apple02Audio implementation here as part of [3103075].  This is necessary
//	because any jack state changes are deferred while the audio system is sleeping.  We need
//	the audio hardware to wake up so that any UI changes resulting from a jack state change
//	can be updated (i.e. removal or construction of the balance control when going from stereo
//	to mono or mono to stereo respectively).	[3103075]	rbm		27 Nov 2002
IOReturn AppleTexasAudio::performPowerStateChange(IOAudioDevicePowerState oldPowerState,
                                                        IOAudioDevicePowerState newPowerState,
                                                        UInt32 *microsecondsUntilComplete)
{
	IOReturn				result;

	debug4IOLog ("+ AppleTexasAudio::performPowerStateChange (%d, %d) -- ourPowerState = %d\n", oldPowerState, newPowerState, ourPowerState);

	if (NULL != theAudioPowerObject && NULL != microsecondsUntilComplete) {
		*microsecondsUntilComplete = theAudioPowerObject->GetTimeToChangePowerState (ourPowerState, newPowerState);
	}

	result = IOAudioDevice::performPowerStateChange (oldPowerState, newPowerState, microsecondsUntilComplete);

	if (NULL != theAudioPowerObject) {
		switch (newPowerState) {
			case kIOAudioDeviceSleep:
				if (ourPowerState == kIOAudioDeviceActive) {
					outputMuteChange (TRUE);			// Mute before turning off power
					theAudioPowerObject->setHardwarePowerOff ();
					ourPowerState = newPowerState;
				}
				break;
			case kIOAudioDeviceIdle:
				if (ourPowerState == kIOAudioDeviceActive) {
					outputMuteChange (TRUE);			// Mute before turning off power
					theAudioPowerObject->setHardwarePowerOff ();
					ourPowerState = kIOAudioDeviceSleep;
				} else if (ourPowerState == kIOAudioDeviceSleep && NULL != dallasDriver) {
					outputMuteChange (TRUE);			// Mute before turning off power
					theAudioPowerObject->setHardwarePowerOn ();
					ourPowerState = kIOAudioDeviceActive;
				}
				break;
			case kIOAudioDeviceActive:
				theAudioPowerObject->setHardwarePowerOn ();
				if (NULL != outMute) {
					outMute->flushValue ();					// Restore hardware to the user's selected state
				}
				ourPowerState = newPowerState;
				break;
			default:
				break;
		}
	}

	debug2IOLog ("- AppleTexasAudio::performPowerStateChange -- ourPowerState = %d\n", ourPowerState);

	return result;
}
#endif

#pragma mark +DIRECT HARDWARE MANIPULATION
// --------------------------------------------------------------------------
UInt8 *	AppleTexasAudio::getGPIOAddress (UInt32 gpioSelector) {
	UInt8 *				gpioAddress;

	gpioAddress = NULL;
	switch (gpioSelector) {
		case kHeadphoneMuteSel:			gpioAddress = hdpnMuteGpio;						break;
		case kHeadphoneDetecteSel:		gpioAddress = headphoneExtIntGpio;				break;
		case kAmplifierMuteSel:			gpioAddress = ampMuteGpio;						break;
		case kSpeakerIDSel:				gpioAddress = dallasExtIntGpio;					break;
		case kCodecResetSel:			gpioAddress = hwResetGpio;						break;
	}
	if ( NULL == gpioAddress ) {
		debug2IOLog ( "AppleTexasAudio::getGPIOAddress ( %d ) returns NULL\n", (unsigned int)gpioSelector );
	}
	return gpioAddress;
}

// --------------------------------------------------------------------------
Boolean	AppleTexasAudio::getGPIOActiveState (UInt32 gpioSelector) {
	Boolean				activeState;

	activeState = NULL;
	switch (gpioSelector) {
		case kHeadphoneMuteSel:			activeState = hdpnActiveState;					break;
		case kHeadphoneDetecteSel:		activeState = headphoneInsertedActiveState;		break;
		case kAmplifierMuteSel:			activeState = ampActiveState;					break;
		case kSpeakerIDSel:				activeState = dallasInsertedActiveState;		break;
		case kCodecResetSel:			activeState = hwResetActiveState;				break;
		default:
			debug2IOLog ( "AppleTexasAudio::getGPIOActiveState ( %d ) UNKNOWN\n", (unsigned int)gpioSelector );
			break;
	}
	return activeState;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTexasAudio::setGPIOActiveState ( UInt32 selector, UInt8 gpioActiveState ) {

	gpioActiveState = 0 == gpioActiveState ? FALSE : TRUE;
	switch ( selector ) {
		case kHeadphoneMuteSel:			hdpnActiveState = gpioActiveState;					break;
		case kHeadphoneDetecteSel:		headphoneInsertedActiveState = gpioActiveState;		break;
		case kAmplifierMuteSel:			ampActiveState = gpioActiveState;					break;
		case kSpeakerIDSel:				dallasInsertedActiveState = gpioActiveState;		break;
		case kCodecResetSel:			hwResetActiveState = gpioActiveState;				break;
		default:
			debug3IOLog ( "  AppleTexasAudio::setGPIOActiveState ( %d, %d ) UNKNOWN\n", (unsigned int)selector, gpioActiveState );
			break;
	}
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Boolean	AppleTexasAudio::checkGpioAvailable ( UInt32 selector ) {
	Boolean			result = FALSE;
	switch ( selector ) {
		case kHeadphoneMuteSel:			if ( NULL != hdpnMuteGpio ) { result = TRUE; }				break;
		case kHeadphoneDetecteSel:		if ( NULL != headphoneExtIntGpio ) { result = TRUE; }		break;
		case kAmplifierMuteSel:			if ( NULL != ampMuteGpio ) { result = TRUE; }				break;
		case kSpeakerIDSel:				if ( NULL != dallasExtIntGpio ) { result = TRUE; }			break;
		case kCodecResetSel:			if ( NULL != hwResetGpio ) { result = TRUE; }				break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3060321]	Fixed kI2sDataWordFormatRegisterSelector to reference the
//				data word format register.  
IOReturn	AppleTexasAudio::readHWReg32 ( UInt32 selector, UInt32 * registerData ) {
	IOReturn	result = kIOReturnError;
	if ( NULL != registerData ) {
		result = kIOReturnSuccess;
		switch ( selector ) {
			case kI2sSerialFormatRegisterSelector:		*registerData = audioI2SControl->GetSerialFormatReg();	break;
			case kI2sDataWordFormatRegisterSelector:	*registerData = audioI2SControl->GetDataWordSizesReg();	break;
			case kFeatureControlRegister1Selector:		*registerData = audioI2SControl->FCR1GetReg();			break;
			case kFeatureControlRegister3Selector:		*registerData = audioI2SControl->FCR3GetReg();			break;
			case kI2s1SerialFormatRegisterSelector:		*registerData = audioI2SControl->GetSerialFormatReg();	break;
			case kI2s1DataWordFormatRegisterSelector:	*registerData = audioI2SControl->GetDataWordSizesReg();	break;
			default:									result = kIOReturnError;								break;
		}
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3060321]	Fixed kI2sDataWordFormatRegisterSelector to reference the
//				data word format register.  
IOReturn	AppleTexasAudio::writeHWReg32 ( UInt32 selector, UInt32 registerData ) {
	IOReturn	result = kIOReturnError;
	result = kIOReturnSuccess;
	switch ( selector ) {
		case kI2sSerialFormatRegisterSelector:		audioI2SControl->SetSerialFormatReg( registerData );		break;
		case kI2sDataWordFormatRegisterSelector:	audioI2SControl->SetDataWordSizesReg( registerData );		break;
		case kFeatureControlRegister1Selector:		audioI2SControl->Fcr1SetReg( registerData );				break;
		case kFeatureControlRegister3Selector:		audioI2SControl->Fcr3SetReg( registerData );				break;
		case kI2s1SerialFormatRegisterSelector:		audioI2SControl->SetSerialFormatReg( registerData );		break;
		case kI2s1DataWordFormatRegisterSelector:	audioI2SControl->SetFrameMatchReg( registerData );			break;
		default:									result = kIOReturnError;									break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTexasAudio::readCodecReg ( UInt32 selector, void * registerData,  UInt32 * registerDataSize ) {
	UInt32			codecCacheSize;
	IOReturn		err = kIOReturnError;
	
	codecCacheSize = sizeof ( TAS3001C_ShadowReg );
	if ( NULL != registerDataSize && NULL != registerData ) {
		if ( codecCacheSize <= *registerDataSize && 0 != codecCacheSize && 0 == selector ) {
			for ( UInt32 index = 0; index < codecCacheSize; index++ ) {
				((UInt8*)registerData)[index] = ((UInt8*)&shadowRegs)[index];
			}
			err = kIOReturnSuccess;
			if ( kIOReturnSuccess == err ) {
				*registerDataSize = codecCacheSize;
			}
		}
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTexasAudio::writeCodecReg ( UInt32 selector, void * registerData ) {
	UInt32			codecRegSize;
	IOReturn		err = 0;
	err = getCodecRegSize ( selector, &codecRegSize );
	if ( kIOReturnSuccess == err ) {
		err = TAS3001C_WriteRegister( (UInt8)selector, (UInt8*)registerData, kUPDATE_ALL );
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt8	AppleTexasAudio::readGPIO (UInt32 selector) {
	UInt8 *				address;
	UInt8				gpioValue;

	gpioValue = NULL;
	address = getGPIOAddress (selector);
	if (NULL != address)
		gpioValue = GpioReadByte (address);

	return (gpioValue);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTexasAudio::getVolumePRAM ( UInt32 * pramDataPtr )
{
	UInt8					curPRAMVol;
	IODTPlatformExpert * 	platform;
	IOReturn				err = kIOReturnError;

	curPRAMVol = 0;
	if ( NULL != pramDataPtr ) {
		platform = OSDynamicCast(IODTPlatformExpert,getPlatform());
		if (platform) {
			platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
			*pramDataPtr = (UInt32)curPRAMVol;
			err = kIOReturnSuccess;
		}
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTexasAudio::getDmaState ( UInt32 * dmaStatePtr )
{
	IOReturn				err = kIOReturnError;
	if ( NULL != dmaStatePtr && NULL != driverDMAEngine ) {
		*dmaStatePtr = (UInt32)driverDMAEngine->getDmaState();
		err = kIOReturnSuccess;
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTexasAudio::getStreamFormat ( IOAudioStreamFormat * streamFormatPtr )
{
	IOReturn				err = kIOReturnError;
	if ( NULL != streamFormatPtr ) {
		err = driverDMAEngine->getAudioStreamFormat( streamFormatPtr );
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTexasAudio::readPowerState ( UInt32 selector, IOAudioDevicePowerState * powerState )
{
	IOReturn				err = kIOReturnError;
	if ( NULL != powerState ) {
		*powerState = ourPowerState;
		err = kIOReturnSuccess;
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTexasAudio::setPowerState ( UInt32 selector, IOAudioDevicePowerState powerState )
{
	UInt32			microsecondsUntilComplete;
	IOReturn		err = kIOReturnError;
	if ( NULL != powerState ) {
		err = performPowerStateChange ( ourPowerState, powerState, &microsecondsUntilComplete );
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexasAudio::setBiquadCoefficients ( UInt32 selector, void * biquadCoefficients, UInt32 coefficientSize )
{
	IOReturn		err;
	IOReturn		totalErr = kIOReturnError;

	if ( kMaxBiquadWidth >= coefficientSize && NULL != biquadCoefficients && 0 == selector ) {
		totalErr = kIOReturnSuccess;
		for ( UInt32 index = 0; index < ( kTumblerNumBiquads * kTumblerMaxStreamCnt ); index ++ ) {
			if ( kTumblerNumBiquads > index ) {
				err = SndHWSetOutputBiquad ( kStreamFrontLeft, index, (FourDotTwenty*)biquadCoefficients );
			} else {
				err = SndHWSetOutputBiquad ( kStreamFrontRight, index - kTumblerNumBiquads, (FourDotTwenty*)biquadCoefficients );
			}
			(( EQFilterCoefficients*)biquadCoefficients)++;
			if ( err ) { totalErr = err; }
		}
	}

	return totalErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexasAudio::getBiquadInformation ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr )
{
#pragma unused ( scalarArg1 )
	IOReturn		result = kIOReturnError;
	if ( NULL != outStructPtr && NULL != outStructSizePtr ) {
		if ( *outStructSizePtr >= ( sizeof ( BiquadInfoList ) + ( sizeof ( UInt32 ) * ( kTumblerCoefficientsPerBiquad - 1 ) ) ) ) {
			((BiquadInfoListPtr)outStructPtr)->numBiquad = kTumblerNumBiquads;
			((BiquadInfoListPtr)outStructPtr)->numCoefficientsPerBiquad = kTumblerCoefficientsPerBiquad;
			((BiquadInfoListPtr)outStructPtr)->biquadCoefficientBitWidth = kTumblerCoefficientBitWidth;
			((BiquadInfoListPtr)outStructPtr)->coefficientIntegerBitWidth = kTumblerCoefficientIntegerBitWidth;
			((BiquadInfoListPtr)outStructPtr)->coefficientFractionBitWidth = kTumblerCoefficientFractionBitWidth;
			((BiquadInfoListPtr)outStructPtr)->coefficientOrder[0] = 'b0  ';
			((BiquadInfoListPtr)outStructPtr)->coefficientOrder[1] = 'b1  ';
			((BiquadInfoListPtr)outStructPtr)->coefficientOrder[2] = 'b2  ';
			((BiquadInfoListPtr)outStructPtr)->coefficientOrder[3] = 'a1  ';
			((BiquadInfoListPtr)outStructPtr)->coefficientOrder[4] = 'a2  ';
			*outStructSizePtr = ( sizeof ( BiquadInfoList ) + ( sizeof ( UInt32 ) * ( kTumblerCoefficientsPerBiquad - 1 ) ) );
			result = kIOReturnSuccess;
		}
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
IOReturn	AppleTexasAudio::getProcessingParameters ( UInt32 scalarArg1, void * outStructPtr, IOByteCount * outStructSizePtr )
{
	UInt32			index;
	IOReturn		err = kIOReturnNotReadable;
	if ( NULL != outStructPtr && NULL != outStructSizePtr ) {
		if (  kMaxProcessingParamSize >= *outStructSizePtr ) {
			for ( index = 0; index < ( kMaxProcessingParamSize / sizeof(UInt32) ); index++ ) {
				((UInt32*)outStructPtr)[index] = mProcessingParams[index];
			}
			/*	STUB:	Insert code here (see Aram and/or Joe)		*/
			
			/*	END STUB											*/
			err = kIOReturnSuccess;
		}
	}
	return (err);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	User Client Support
//	If loading software EQ, the scalarArg1 will be set to 'true' to indicate
//	that hardware EQ should be disabled.  If re-enabling hardware EQ then
//	scalarArg1 will be set to 'false' and the proper EQ for the current
//	hardware environment (jack state, speaker model, etc.) must be applied.
IOReturn	AppleTexasAudio::setProcessingParameters ( UInt32 scalarArg1, void * inStructPtr, UInt32 inStructSize )
{
	EQPrefsElementPtr	eqPrefs;
	UInt32				index;
	IOReturn			err = kIOReturnNotReadable;
	
	if ( NULL != inStructPtr && kMaxProcessingParamSize >= inStructSize ) {
		disableLoadingEQFromFile = (bool)scalarArg1;
		for ( index = 0; index < ( kMaxProcessingParamSize / sizeof(UInt32) ); index++ ) {
			mProcessingParams[index] = ((UInt32*)inStructPtr)[index];
		}
		if ( disableLoadingEQFromFile ) {
			SetUnityGainAllPass();
		} else {
			err = GetCustomEQCoefficients (layoutID, deviceID, speakerID, &eqPrefs);
			if ( kIOReturnSuccess == err && NULL != eqPrefs ) {
				DRCInfo				localDRC;
		
				//	Set the dynamic range compressor coefficients.
				localDRC.compressionRatioNumerator		= eqPrefs->drcCompressionRatioNumerator;
				localDRC.compressionRatioDenominator	= eqPrefs->drcCompressionRatioDenominator;
				localDRC.threshold						= eqPrefs->drcThreshold;
				localDRC.maximumVolume					= eqPrefs->drcMaximumVolume;
				localDRC.enable							= (Boolean)((UInt32)(eqPrefs->drcEnable));
		
				SndHWSetDRC ((DRCInfoPtr)&localDRC);
		
				SndHWSetOutputBiquadGroup (eqPrefs->filterCount, eqPrefs->filter[0].coefficient);
			} else {
				SetUnityGainAllPass ();
			}
		}
		/*	STUB:	Insert code here (see Aram and/or Joe)		*/
		
		//driverDMAEngine->set_swEQ(inStructPtr);
		
		/*	END STUB											*/
		err = kIOReturnSuccess;
	}
	return (err);
}



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTexasAudio::invokeInternalFunction ( UInt32 functionSelector, void * inData ) {
	switch ( functionSelector ) {
		case kInvokeHeadphoneInterruptHandler:
			RealHeadphoneInterruptHandler ( 0, 0 );
			break;
		case kInvokeSpeakerInterruptHandler:
			RealDallasInterruptHandler ( 0, 0 );
			break;
	}
	return kIOReturnSuccess;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	AppleTexasAudio::writeGPIO (UInt32 selector, UInt8 data) {
	UInt8 *				address;
	UInt32				gpioValue;

	gpioValue = NULL;
	address = getGPIOAddress (selector);
	if (NULL != address)
		GpioWriteByte (address, data);

	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Return 'true' if the 'gpioDDR' bit is non-zero.
UInt8	AppleTexasAudio::GpioGetDDR( UInt8* gpioAddress )
{
	UInt8			gpioData;
	Boolean			result;
	
	result = gpioDDR_INPUT;
	if( NULL != gpioAddress )
	{
		gpioData = *gpioAddress;
		if( 0 != ( gpioData & ( 1 << gpioDDR ) ) ) {
			result = gpioDDR_OUTPUT;
		}
#if DEBUGLOG
		IOLog( "***** GPIO DDR RD 0x%8.0X = 0x%2.0X returns %d\n", (unsigned int)gpioAddress, gpioData, result );
#endif
	}
	return result;
}

UInt8 AppleTexasAudio::GpioReadByte( UInt8* gpioAddress )
{
	UInt8			gpioData;
	Boolean			result;
	
	result = 0;
	gpioData = 0;
	if( NULL != gpioAddress )
	{
		gpioData = *gpioAddress;
#if DEBUGLOG
		IOLog( "GpioReadByte( 0x%8.0X ), *gpioAddress 0x%X\n", (unsigned int)gpioAddress, gpioData );
#endif
	}

	return gpioData;
}

void	AppleTexasAudio::GpioWriteByte( UInt8* gpioAddress, UInt8 gpioData )
{
	if( NULL != gpioAddress )
	{
		*gpioAddress = gpioData;
#if DEBUGLOG
		IOLog( "GpioWrite( 0x%8.0X, 0x%2.0X )\n", (unsigned int)gpioAddress, gpioData);
#endif
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Return 'true' if the 'gpioData' bit is non-zero.  This function does not
//	return the state of the pin.
Boolean AppleTexasAudio::GpioRead( UInt8* gpioAddress )
{
	UInt8			gpioData;
	Boolean			result;
	
	result = 0;
	if( NULL != gpioAddress )
	{
		gpioData = *gpioAddress;
		if( 0 != ( gpioData & ( 1 << gpioDATA ) ) ) {
			result = 1;
		}
#if DEBUGLOG
		IOLog( "GpioRead( 0x%8.0X ) result %d, *gpioAddress 0x%2.0X\n", (unsigned int)gpioAddress, result, *gpioAddress );
#endif
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Sets the 'gpioDDR' to OUTPUT and sets the 'gpioDATA' to the 'data' state.
void	AppleTexasAudio::GpioWrite( UInt8* gpioAddress, UInt8 data )
{
	UInt8		gpioData;
	
	if( NULL != gpioAddress )
	{
		if( 0 == data )
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 0 << gpioDATA );
		else
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 1 << gpioDATA );
		*gpioAddress = gpioData;
#if DEBUGLOG
		IOLog( "GpioWrite( 0x%8.0X, 0x%2.0X ), *gpioAddress 0x%2.0X\n", (unsigned int)gpioAddress, gpioData, *gpioAddress );
#endif
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Returns information regarding the CPU ID, port ID and speaker ID.  The
//	speaker ID is a concatenation of the Dallas ROM 8 bit fields to form a
//	32 bit field where the family ID occupies the least significant byte of
//	the 32 bit field.  The remaining fields appear in ascending order.
//
//	speakerID = 0xCCFFTTSS where:	CC = Connected
//									FF = DallasID.deviceFamily
//									TT = DallasID.deviceType
//									SS = DallasID.deviceSubType (not supported currently, returns 0)
//
IOReturn	AppleTexasAudio::readSpkrID ( UInt32 selector, UInt32 * speakerIDPtr ) {
	UInt8				bEEPROM[32];
	IOReturn			result = kIOReturnError;
	
	if ( NULL != speakerIDPtr ) {
		for ( UInt32 index = 0; index < 4; index++ ) {
			bEEPROM[index] = 0;
		}

		if ( selector ) {
			//	Force a ROM access here.  WARNING:  This may cause corruption the equalizer set
			//	and should only be used with caution for diagnostic purposes		[3053696]
			if ( IsSpeakerConnected () && NULL != dallasDriver ) {
				if ( dallasDriver->getSpeakerID (bEEPROM) ) {
					*speakerIDPtr = ( IsSpeakerConnected () << kSpeakerID_Connected ) | ( bEEPROM[0] << kSpeakerID_Family ) | ( bEEPROM[1] << kSpeakerID_Type ) | ( bEEPROM[2] << kSpeakerID_SubType );
				}
			}
		} else {
			//	Default method for accessing speaker ID information is to use the data that
			//	has been cached by the speaker detect interrupt handler.
			*speakerIDPtr = ( IsHeadphoneConnected () << kHeadphone_Connected ) | \
							( IsSpeakerConnected () << kSpeakerID_Connected ) | \
							( familyID << kSpeakerID_Family ) | \
							( speakerID << kSpeakerID_Type ) | \
							( 0 << kSpeakerID_SubType );
		}
		result = kIOReturnSuccess;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleTexasAudio::getCodecRegSize ( UInt32 selector, UInt32 * codecRegSizePtr )
{
	IOReturn		err = kIOReturnError;
	if ( NULL != codecRegSizePtr ) {
		*codecRegSizePtr = 0;
		err = kIOReturnSuccess;
		switch( selector )
		{
			case kMainCtrlReg:			*codecRegSizePtr = kMCRwidth;	break;
			case kDynamicRangeCtrlReg:	*codecRegSizePtr = kDRCwidth;	break;
			case kVolumeCtrlReg:		*codecRegSizePtr = kVOLwidth;	break;
			case kTrebleCtrlReg:		*codecRegSizePtr = kTREwidth;	break;
			case kBassCtrlReg:			*codecRegSizePtr = kBASwidth;	break;
			case kMixer1CtrlReg:		*codecRegSizePtr = kMIXwidth;	break;
			case kMixer2CtrlReg:		*codecRegSizePtr = kMIXwidth;	break;
			case kLeftBiquad0CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			case kLeftBiquad1CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			case kLeftBiquad2CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			case kLeftBiquad3CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			case kLeftBiquad4CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			case kLeftBiquad5CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			case kRightBiquad0CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			case kRightBiquad1CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			case kRightBiquad2CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			case kRightBiquad3CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			case kRightBiquad4CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			case kRightBiquad5CtrlReg:	*codecRegSizePtr = kBIQwidth;	break;
			default:					err = kIOReturnBadArgument;		break;
		}
		if ( kIOReturnSuccess != err ) {
			debug4IOLog ( "±AppleTexasAudio::getCodecRegSize ( %X, %X ) returns %X\n", (unsigned int)selector, (unsigned int)codecRegSizePtr, (unsigned int)err );
		}
	}
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexasAudio::InitEQSerialMode (UInt32 mode, Boolean restoreOnNormal)
{
	IOReturn		err;
	UInt8			initData;
	UInt8			previousData;
	
	debug3IOLog ("AppleTexasAudio::InitEQSerialMode (%8lX, %d)\n", mode, restoreOnNormal);
	initData = (kNormalLoad << kFL);
	if (kSetFastLoadMode == mode)
		initData = (kFastLoad << kFL);
		
	err = TAS3001C_ReadRegister (kMainCtrlReg, &previousData);
	initData |= (k64fs << kSC) | TAS_I2S_MODE | (TAS_WORD_LENGTH << kW0);
	err = TAS3001C_WriteRegister (kMainCtrlReg, &initData, kUPDATE_ALL);
	
	//	If restoring to normal load mode then restore the settings of all
	//	registers that have been corrupted by entering fast load mode (i.e.
	//	volume, bass, treble, mixer1 and mixer2).  Restoration only occurs
	//	if going from a previous state of Fast Load to a new state of Normal Load.
	if (kRestoreOnNormal == restoreOnNormal && ((kFastLoad << kFL) == (kFastLoad << kFL) & previousData)) {
		if ((kNormalLoad << kFL) == (initData & (kFastLoad << kFL))) {
			TAS3001C_WriteRegister (kVolumeCtrlReg, (UInt8*)shadowRegs.sVOL, kUPDATE_HW);
			TAS3001C_WriteRegister (kMixer1CtrlReg, (UInt8*)shadowRegs.sMX1, kUPDATE_HW);
			TAS3001C_WriteRegister (kMixer2CtrlReg, (UInt8*)shadowRegs.sMX2, kUPDATE_HW);
			TAS3001C_WriteRegister (kTrebleCtrlReg, (UInt8*)shadowRegs.sTRE, kUPDATE_HW);
			TAS3001C_WriteRegister (kBassCtrlReg,	(UInt8*)shadowRegs.sBAS, kUPDATE_HW);
		}
	}
	debug4IOLog ("AppleTexasAudio ... %d = InitEQSerialMode (%8lX, %d)\n", err, mode, restoreOnNormal);
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Updates the amplifier mute state and delays for amplifier settling if
//	the amplifier mute state is not the current mute state.
IOReturn AppleTexasAudio::SetAmplifierMuteState( UInt32 ampID, Boolean muteState )
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
IOReturn AppleTexasAudio::SetVolumeCoefficients( UInt32 left, UInt32 right )
{
	UInt8		volumeData[kVOLwidth];
	IOReturn	err;
	
	debug3IOLog("SetVolumeCoefficients: L=0x%lx R=0x%lx\n", left, right);
	
	volumeData[2] = left;														
	volumeData[1] = left >> 8;												
	volumeData[0] = left >> 16;												
	
	volumeData[5] = right;														
	volumeData[4] = right >> 8;												
	volumeData[3] = right >> 16;
	
	err = TAS3001C_WriteRegister( kVolumeCtrlReg, volumeData, kUPDATE_ALL );

	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	This routine will perform a reset of the TAS3001C and then initialize
//	all registers within the TAS3001C to the values already held within 
//	the shadow registers.  The RESET sequence must not be performed until
//	the I2S clocks are running.	 The TAS3001C may hold the I2C bus signals
//	SDA and SCL low until the reset sequence (high->low->high) has been
//	completed.
IOReturn	AppleTexasAudio::TAS3001C_Initialize(UInt32 resetFlag) {
	IOReturn		err;
	UInt32		retryCount;
	UInt32		initMode;
	UInt8		oldMode;
	Boolean		done;
	
	switch( resetFlag )
	{	
		case kFORCE_RESET_SETUP_TIME:		debug2IOLog( "TAS3001C_Initialize( %s )\n", "kFORCE_RESET_SETUP_TIME" );		break;
		case kNO_FORCE_RESET_SETUP_TIME:	debug2IOLog( "TAS3001C_Initialize( %s )\n", "kNO_FORCE_RESET_SETUP_TIME" ); 	break;
		default:							debug2IOLog( "TAS3001C_Initialize( %s )\n", "UNKNOWN" );						break;
	}
	err = -227; // siDeviceBusyErr
	done = false;
	oldMode = 0;
	initMode = kUPDATE_HW;
	retryCount = 0;
	if (!semaphores)
	{
		semaphores = 1;
		mPollingMode = false;		// try to run with interrupts, will be set to true if we get errors.
		do{
			debug2IOLog( "RESETTING, retryCount %ld\n", retryCount );
			TAS3001C_Reset( resetFlag );											//	cycle reset from 1 through 0 and back to 1
			if( 0 == oldMode )
				TAS3001C_ReadRegister( kMainCtrlReg, &oldMode );					//	save previous load mode

			err = InitEQSerialMode( kSetFastLoadMode, kDontRestoreOnNormal );		//	set fast load mode for biquad initialization
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kLeftBiquad0CtrlReg, (UInt8*)shadowRegs.sLB0, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kLeftBiquad1CtrlReg, (UInt8*)shadowRegs.sLB1, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kLeftBiquad2CtrlReg, (UInt8*)shadowRegs.sLB2, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kLeftBiquad3CtrlReg, (UInt8*)shadowRegs.sLB3, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kLeftBiquad4CtrlReg, (UInt8*)shadowRegs.sLB4, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kLeftBiquad5CtrlReg, (UInt8*)shadowRegs.sLB5, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kRightBiquad0CtrlReg, (UInt8*)shadowRegs.sRB0, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kRightBiquad1CtrlReg, (UInt8*)shadowRegs.sRB1, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kRightBiquad1CtrlReg, (UInt8*)shadowRegs.sRB1, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kRightBiquad2CtrlReg, (UInt8*)shadowRegs.sRB2, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kRightBiquad3CtrlReg, (UInt8*)shadowRegs.sRB3, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kRightBiquad4CtrlReg, (UInt8*)shadowRegs.sRB4, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = InitEQSerialMode( kSetNormalLoadMode, kDontRestoreOnNormal );								//	set normal load mode for most register initialization
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kDynamicRangeCtrlReg, (UInt8*)shadowRegs.sDRC, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
				
			err = TAS3001C_WriteRegister( kVolumeCtrlReg, (UInt8*)shadowRegs.sVOL, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kTrebleCtrlReg, (UInt8*)shadowRegs.sTRE, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kBassCtrlReg, (UInt8*)shadowRegs.sBAS, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kMixer1CtrlReg, (UInt8*)shadowRegs.sMX1, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
	
			err = TAS3001C_WriteRegister( kMixer2CtrlReg, (UInt8*)shadowRegs.sMX2, initMode );
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
			
			err = TAS3001C_WriteRegister( kMainCtrlReg, &oldMode, initMode );			//	restore previous load mode
			FailIf( kIOReturnSuccess != err, AttemptToRetry );
AttemptToRetry:				
			if( kIOReturnSuccess == err ) {		//	terminate when successful
				done = true;
			} else {
				mPollingMode = true;
			}
			retryCount++;
		} while ( !done && ( kRESET_MAX_RETRY_COUNT != retryCount ) );
		semaphores = 0;
		if( kRESET_MAX_RETRY_COUNT == retryCount ) {
			debug2IOLog( "\n\n\n\n			TAS3001 IS DEAD: Check %s\n\n\n\n", "ChooseAudio in fcr1" );
		}
		
	}
	if( kIOReturnSuccess != err )
		debug3IOLog( "TAS3001C_Initialize( %ld ) err = %d\n", resetFlag, err );

	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Reading registers with the TAS3001C is not possible.  A shadow register
//	is maintained for each TAS3001C hardware register.	Whenever a write
//	operation is performed on a hardware register, the data is written to 
//	the shadow register.  Read operations copy data from the shadow register
//	to the client register buffer.
IOReturn	AppleTexasAudio::TAS3001C_ReadRegister(UInt8 regAddr, UInt8* registerData) {
	UInt32			registerSize;
	UInt32			regByteIndex;
	UInt8			*shadowPtr;
	IOReturn		err;
	
	err = kIOReturnSuccess;
	// quiet warnings caused by a complier that can't really figure out if something is going to be used uninitialized or not.
	registerSize = 0;
	shadowPtr = NULL;
	switch( regAddr )
	{
		case kMainCtrlReg:			shadowPtr = (UInt8*)shadowRegs.sMCR;	registerSize = kMCRwidth;	break;
		case kDynamicRangeCtrlReg:	shadowPtr = (UInt8*)shadowRegs.sDRC;	registerSize = kDRCwidth;	break;
		case kVolumeCtrlReg:		shadowPtr = (UInt8*)shadowRegs.sVOL;	registerSize = kVOLwidth;	break;
		case kTrebleCtrlReg:		shadowPtr = (UInt8*)shadowRegs.sTRE;	registerSize = kTREwidth;	break;
		case kBassCtrlReg:			shadowPtr = (UInt8*)shadowRegs.sBAS;	registerSize = kBASwidth;	break;
		case kMixer1CtrlReg:		shadowPtr = (UInt8*)shadowRegs.sMX1;	registerSize = kMIXwidth;	break;
		case kMixer2CtrlReg:		shadowPtr = (UInt8*)shadowRegs.sMX2;	registerSize = kMIXwidth;	break;
		case kLeftBiquad0CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB0;	registerSize = kBIQwidth;	break;
		case kLeftBiquad1CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB1;	registerSize = kBIQwidth;	break;
		case kLeftBiquad2CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB2;	registerSize = kBIQwidth;	break;
		case kLeftBiquad3CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB3;	registerSize = kBIQwidth;	break;
		case kLeftBiquad4CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB4;	registerSize = kBIQwidth;	break;
		case kLeftBiquad5CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB5;	registerSize = kBIQwidth;	break;
		case kRightBiquad0CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB0;	registerSize = kBIQwidth;	break;
		case kRightBiquad1CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB1;	registerSize = kBIQwidth;	break;
		case kRightBiquad2CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB2;	registerSize = kBIQwidth;	break;
		case kRightBiquad3CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB3;	registerSize = kBIQwidth;	break;
		case kRightBiquad4CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB4;	registerSize = kBIQwidth;	break;
		case kRightBiquad5CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB5;	registerSize = kBIQwidth;	break;
		default:					err = -201; /* notEnoughHardware */									break;
	}
	if( kIOReturnSuccess == err )
	{
		for( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ )
		{
			registerData[regByteIndex] = shadowPtr[regByteIndex];
		}
	}
	if( kIOReturnSuccess != err )
		debug4IOLog( "%d notEnoughHardware = TAS3001C_ReadRegister( 0x%2.0X, 0x%8.0X )", err, regAddr, (unsigned int)registerData );

	return err;
}

#if DEBUGLOG
void AppleTexasAudio::LogStuff () {
	IOLog ("gpio reset = 0x%x\n", *hwResetGpio);
	IOLog ("FCR1 = 0x%lx\n", audioI2SControl->FCR1GetReg ());
	IOLog ("FCR3 = 0x%lx\n", audioI2SControl->FCR3GetReg ());
}
#endif

IOReturn 	AppleTexasAudio::TAS3001C_Reset(UInt32 resetFlag){
    IOReturn err = kIOReturnSuccess;

#if DEBUGLOG
	IOLog( "TAS3001C_Reset( %lu )\n", resetFlag );
#endif
	switch( resetFlag )
	{	
		case kFORCE_RESET_SETUP_TIME:		debug2IOLog( "[AppleTexasAudio] TAS3001C_Reset( %s )\n", "kFORCE_RESET_SETUP_TIME" );		break;
		case kNO_FORCE_RESET_SETUP_TIME:	debug2IOLog( "[AppleTexasAudio] TAS3001C_Reset( %s )\n", "kNO_FORCE_RESET_SETUP_TIME" );	break;
		default:							debug2IOLog( "[AppleTexasAudio] TAS3001C_Reset( %s )\n", "UNKNOWN" );						break;
	}

	ClockSource				clockSource;
	UInt32					sclkDivisor;
	UInt32					mclkDivisor;
	UInt32					dataFormat;	//	[3060321]	rbm	2 Oct 2002
	UInt32					myFrameRate;
	dataFormat = ( ( 2 << kNumChannelsInShift ) | kDataIn16 | ( 2 << kNumChannelsOutShift ) | kDataOut16 );	//	[3060321]	rbm	2 Oct 2002

	myFrameRate = frameRate (0);

	FailIf (FALSE == audioI2SControl->setSampleParameters (myFrameRate, 256, &clockSource, &mclkDivisor, &sclkDivisor, kSndIOFormatI2S64x), Exit);
	//	[3060321]	The data word format register and serial format register require that the I2S clocks be stopped and
	//				restarted before the register value is applied to operation of the I2S IOM.  We now pass the data
	//				word format to setSerialFormatRegister as that method stops the clocks when applying the value
	//				to the serial format register.  That method now also sets the data word format register while
	//				the clocks are stopped.		rbm	2 Oct 2002
	audioI2SControl->setSerialFormatRegister (clockSource, mclkDivisor, sclkDivisor, kSndIOFormatI2S64x, dataFormat);

	if( hwResetActiveState == GpioRead( hwResetGpio ) || !GpioGetDDR( hwResetGpio ) || resetFlag )	//	if reset never was performed
	{
		GpioWrite( hwResetGpio, 0 == hwResetActiveState ? 1 : 0 );	//	negate RESET
		// I think we really only have to reset it for a millisecond or two
		IOSleep (200);
	}
	else
	{
		IOSleep (3);
	}

	GpioWrite( hwResetGpio, hwResetActiveState );					//	Assert RESET
	IOSleep (3);

	GpioWrite( hwResetGpio, 0 == hwResetActiveState ? 1 : 0 );		//	negate RESET
	IOSleep (3);

Exit:
#if DEBUGLOG
	IOLog( "%d = TAS3001C_Reset( %lu )\n", err, resetFlag );
#endif

    return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	All TAS3001C write operations pass through this function so that a shadow
//	copy of the registers can be kept in global storage.  This allows a quick
//	method of re-initialization of the TAS3001C register contents when the
//	clock sources have been manipulated, resulting in loss of phase lock and
//	disabling i2c communication until a RESET has been issued to the equalizer.
//	The size of the 'registerData' is implied by the register address and is
//	stipulated within the TAS3001C specification as well as within the
//	TAS3001C_registerWidths enumeration.  This function does enforce the data
//	size of the target register.  No partial register write operations are
//	supported.	IMPORTANT:	There is no enforcement regarding 'load' mode 
//	policy.	 Client routines should properly maintain the 'load' mode by 
//	saving the contents of the master control register, set the appropriate 
//	load mode for the target register and then restore the previous 'load' 
//	mode.  All biquad registers should only be loaded while in 'fast load' 
//	mode.  All other registers should be loaded while in 'normal load' mode.
IOReturn	AppleTexasAudio::TAS3001C_WriteRegister(UInt8 regAddr, UInt8* registerData, UInt8 mode){
//	CntrlParam		pb;
	UInt32			registerSize;
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
	switch( regAddr )
	{
		case kMainCtrlReg:			shadowPtr = (UInt8*)shadowRegs.sMCR;	registerSize = kMCRwidth;	break;
		case kDynamicRangeCtrlReg:	shadowPtr = (UInt8*)shadowRegs.sDRC;	registerSize = kDRCwidth;	break;
		case kVolumeCtrlReg:		shadowPtr = (UInt8*)shadowRegs.sVOL;	registerSize = kVOLwidth;	break;
		case kTrebleCtrlReg:		shadowPtr = (UInt8*)shadowRegs.sTRE;	registerSize = kTREwidth;	break;
		case kBassCtrlReg:			shadowPtr = (UInt8*)shadowRegs.sBAS;	registerSize = kBASwidth;	break;
		case kMixer1CtrlReg:		shadowPtr = (UInt8*)shadowRegs.sMX1;	registerSize = kMIXwidth;	break;
		case kMixer2CtrlReg:		shadowPtr = (UInt8*)shadowRegs.sMX2;	registerSize = kMIXwidth;	break;
		case kLeftBiquad0CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB0;	registerSize = kBIQwidth;	break;
		case kLeftBiquad1CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB1;	registerSize = kBIQwidth;	break;
		case kLeftBiquad2CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB2;	registerSize = kBIQwidth;	break;
		case kLeftBiquad3CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB3;	registerSize = kBIQwidth;	break;
		case kLeftBiquad4CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB4;	registerSize = kBIQwidth;	break;
		case kLeftBiquad5CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sLB5;	registerSize = kBIQwidth;	break;
		case kRightBiquad0CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB0;	registerSize = kBIQwidth;	break;
		case kRightBiquad1CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB1;	registerSize = kBIQwidth;	break;
		case kRightBiquad2CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB2;	registerSize = kBIQwidth;	break;
		case kRightBiquad3CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB3;	registerSize = kBIQwidth;	break;
		case kRightBiquad4CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB4;	registerSize = kBIQwidth;	break;
		case kRightBiquad5CtrlReg:	shadowPtr = (UInt8*)shadowRegs.sRB5;	registerSize = kBIQwidth;	break;
		default:					err = -201; /* notEnoughHardware */									break;
	}
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
#if DEBUGLOG
			IOLog( "TAS3001C_WriteRegister addr: %2.0X subaddr: %2.0X, data = ", DEQAddress, regAddr );
			for( regByteIndex = 0; regByteIndex < registerSize; regByteIndex++ ) {
				IOLog( "%2.0X ", registerData[regByteIndex] );
			}
			IOLog("\n");
#endif
			if (openI2C()) {
#if DEBUGLOG
				IOLog ("interface->writeI2CBus( addr 0x%X, subaddr 0x%X, ..., ... )\n", DEQAddress, regAddr );
#endif
				success = interface->writeI2CBus (DEQAddress, regAddr, registerData, registerSize);
				if ( !success ) {
#if DEBUGLOG
					IOLog ("%d = interface->writeI2CBus( addr 0x%X, subaddr 0x%X, ..., ... )\n",success, DEQAddress, regAddr );
					LogStuff ();
#endif
				}
				closeI2C();
				if ( !success ) {
#if DEBUGLOG
					IOLog("*** Attempting recovery ***\n");
#endif
					success = TAS3001C_Initialize( kNO_FORCE_RESET_SETUP_TIME );
				}
			} else {
				debugIOLog ("couldn't open the I2C bus!\n");
			}
		}
	}

	if( kIOReturnSuccess != err || !success ) {
		debug3IOLog ("err 0x%X returned, success == %d in AppleTexasAudio::TAS3001C_WriteRegister\n", err, success);
		if (kIOReturnSuccess == err)
			err = -1;	// force a retry
	}


	return err;
}

#pragma mark +UTILITY FUNCTIONS
bool AppleTexasAudio::DallasDriverPublished (AppleTexasAudio * appleTexasAudio, void * refCon, IOService * newService) {
	bool						resultCode;

	resultCode = FALSE;

	FailIf (NULL == appleTexasAudio, Exit);
	FailIf (NULL == newService, Exit);

	appleTexasAudio->dallasDriver = (AppleDallasDriver *)newService;

	appleTexasAudio->attach (appleTexasAudio->dallasDriver);
	appleTexasAudio->dallasDriver->open (appleTexasAudio);

	if (NULL != appleTexasAudio->dallasDriverNotifier)
		appleTexasAudio->dallasDriverNotifier->remove ();

	resultCode = TRUE;

Exit:
	return resultCode;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Set state equal to if external speakers are around. If active, restore
// preferred mute state. If inactive, mute in hardware.
// Has to be called on the IOAudioFamily workloop (because we do the config change of the controls)!!!
//
// IMPORTANT!!!  Interrupts must be disabled before calling this function if Dallas speakers has been plugged in
// IMPORTANT!!!  to make sure that when it calls dallasDriver->getSpeakerID you don't get stuck in an infinite
// IMPORTANT!!!  loop of interrupts.
//
void AppleTexasAudio::DeviceInterruptService (void) {
	EQPrefsElementPtr	eqPrefs;
	IOReturn			err;
	UInt32				deviceID;
	Boolean				result;
	UInt8				bEEPROM[32];
	OSNumber *			headphoneState;			// For [2926907]

	debugIOLog ("+ DeviceInterruptService\n");

	err = kIOReturnSuccess;

	deviceID = GetDeviceMatch ();
	// for 2749470
	if (NULL != driverDMAEngine) {
		if (kInternalSpeakerActive == deviceID) {
			// when it's just the internal speaker, figure out if we have to mute the right channel
			switch (layoutID) {
				case layoutTangent:
				case layoutP57:
					driverDMAEngine->setRightChanMixed (FALSE);
					useMasterVolumeControl = TRUE;
					break;
				case layoutP57b:
				case layoutTessera:
				case layoutP79:
					driverDMAEngine->setRightChanMixed (TRUE);
					useMasterVolumeControl = TRUE;
					break;
				default:
					driverDMAEngine->setRightChanMixed (FALSE);
					useMasterVolumeControl = FALSE;
			}
		} else {
			// If it's external speakers or headphones, don't mute the right channel and create it if we've already deleted it
			driverDMAEngine->setRightChanMixed (FALSE);
			useMasterVolumeControl = FALSE;
		}
	}

	debug4IOLog ( "dallasDriver %x, dallasSpeakersConnected %x, dallasSpeakersProbed %x\n", (unsigned int)dallasDriver, (unsigned int)dallasSpeakersConnected, (unsigned int)dallasSpeakersProbed  );
	
	if (NULL != dallasDriver && TRUE == dallasSpeakersConnected && FALSE == dallasSpeakersProbed) {
		// get the layoutID from the IORegistry for the machine we're running on (which is the machine's device-id)
		// deviceMatch is set from sound objects, but we're hard coding it using a table at the moment
		speakerID = 0;
		familyID = 0;
		result = FALSE;						// FALSE == failure from dallasDriver->getSpeakerID
		bEEPROM[0] = 0;
		bEEPROM[1] = 0;
		debugIOLog ("About to get the speaker ID\n");
		result = dallasDriver->getSpeakerID (bEEPROM);
		dallasSpeakersProbed = TRUE;
	
		debug3IOLog ("DallasDriver result = %d speakerID = %ld\n", result, speakerID);

		speakerConnectFailed = TRUE;
		if (TRUE == result) {
			if ( kDallasDeviceFamilySpeaker == bEEPROM[0] ) {
				// If the Dallas speakers are misinserted, set registry up for our MacBuddy buddies no matter what the output device is
				familyID = bEEPROM[0];
				speakerID = bEEPROM[1];
				speakerConnectFailed = FALSE;
			}
		}
		setProperty (kSpeakerConnectError, speakerConnectFailed);
	
		if (kExternalSpeakersActive == deviceID && TRUE == speakerConnectFailed && TRUE == IsSpeakerConnected()) {	//	[3103075]	rbm		12/2/2002
			// Only put up our alert if the Dallas speakers are the output device
			DisplaySpeakersNotFullyConnected (this, NULL);
		}
	}

	if ( !dallasSpeakersConnected ) {		//	begin [3017286]		rbm 08/07/2002
		familyID = 0;
		speakerID = 0;
	}										//	[3017286] end		rbm 08/07/2002

	if ( !disableLoadingEQFromFile ) {
		err = GetCustomEQCoefficients (layoutID, deviceID, speakerID, &eqPrefs);
	} else {
		err = kIOReturnSuccess;
	}
	
	debug6IOLog ("%d = GetCustomEQCoefficients (%lX, %lX, %lX, %p)\n", err, layoutID, deviceID, speakerID, eqPrefs);

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
	} else {
		SetUnityGainAllPass ();
	}

	// Set the level controls to their (possibly) new min and max values
	minVolume = kMinimumVolume;
	maxVolume = kMaximumVolume + drc.maximumVolume;

	debug3IOLog ("DeviceInterruptService: minVolume = %ld, maxVolume = %ld\n", minVolume, maxVolume);
	AdjustControls ();

	// For [2926907]
	if (NULL != headphoneConnection) {
		if (TRUE == IsHeadphoneConnected ()) {
			headphoneState = OSNumber::withNumber (1, 32);
		} else {
			headphoneState = OSNumber::withNumber ((long long unsigned int)0, 32);
		}
		(void)headphoneConnection->hardwareValueChanged (headphoneState);
	}
	// end [2926907]
	debugIOLog ("- DeviceInterruptService\n");
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
void AppleTexasAudio::ExcludeHPMuteRelease (UInt32 layout) {
	switch (layout) {
		case layoutP92:		/*	Fall through to set dontReleaseHPMute = true	*/
		case layoutP54:		/*	Fall through to set dontReleaseHPMute = true	*/
		case layoutP29:		dontReleaseHPMute = true;			break;
		default:			dontReleaseHPMute = false;			break;
	}
}

UInt32 AppleTexasAudio::GetDeviceMatch (void) {
	UInt32			theDeviceMatch;

	if (TRUE == IsHeadphoneConnected ())
		theDeviceMatch = kHeadphonesActive;				// headphones are connected
	else if (TRUE == dallasSpeakersConnected)
		theDeviceMatch = kExternalSpeakersActive;		// headphones aren't connected and external Dallas speakers are connected
	else
		theDeviceMatch = kInternalSpeakerActive;		// headphones aren't connected and external Dallas speakers aren't connected

	return theDeviceMatch;
}

IORegistryEntry *AppleTexasAudio::FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value) {
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

IORegistryEntry *AppleTexasAudio::FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value) {
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
IOReturn AppleTexasAudio::GetCustomEQCoefficients (UInt32 layoutID, UInt32 deviceID, UInt32 speakerID, EQPrefsElementPtr *filterSettings) {
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

		if ((eqElementPtr->layoutID == layoutID) && (eqElementPtr->deviceID == deviceID) && (eqElementPtr->speakerID == speakerID)) {
			found = TRUE;
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
UInt32 AppleTexasAudio::GetDeviceID (void) {
	IORegistryEntry			*sound;
	OSData					*tmpData;
	UInt32					*deviceID;
	UInt32					theDeviceID;

	theDeviceID = 0;

	sound = ourProvider->childFromPath (kSoundEntry, gIODTPlane);
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
Boolean AppleTexasAudio::HasInput (void) {
	IORegistryEntry			*sound;
	OSData					*tmpData;
	UInt32					*numInputs;
	Boolean					hasInput;

	hasInput = false;

	sound = ourProvider->childFromPath (kSoundEntry, gIODTPlane);
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
IOReturn AppleTexasAudio::SetActiveOutput (UInt32 output, Boolean touchBiquad) {
	IOReturn			err;
	
	debug3IOLog ("AppleTexasAudio::SndHWSetActiveOutput (output = %ld, %d)\n", output, touchBiquad);

	err = kIOReturnSuccess;
	if (touchBiquad)
		SetUnityGainAllPass ();
	switch (output) {
		case kSndHWOutputNone:
			SetAmplifierMuteState (kHEADPHONE_AMP, ASSERT_GPIO (hdpnActiveState));	//	mute
			SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));		//	mute
			break;
		case kSndHWOutput1:
			SetAmplifierMuteState (kHEADPHONE_AMP, NEGATE_GPIO (hdpnActiveState));	//	unmute
			SetAmplifierMuteState (kSPEAKER_AMP, ASSERT_GPIO (ampActiveState));		//	mute
			IOSleep (kAmpRecoveryMuteDuration);										// Wait for the amp to become active
			break;
		case kSndHWOutput2:																//	fall through to kSndHWOutput4
		case kSndHWOutput3:																//	fall through to kSndHWOutput4
		case kSndHWOutput4:
			//	The TA1101B amplifier can 'crowbar' when inserting the speaker jack.
			//	Muting the amplifier will release it from the crowbar state.
			SetAmplifierMuteState (kHEADPHONE_AMP, ASSERT_GPIO (hdpnActiveState));	//	mute
			SetAmplifierMuteState (kSPEAKER_AMP, NEGATE_GPIO (ampActiveState));		//	unmute
			IOSleep (kAmpRecoveryMuteDuration);										// Wait for the amp to become active
			if (!dontReleaseHPMute)													//	[2660341] unmute if std hw
				SetAmplifierMuteState (kHEADPHONE_AMP, NEGATE_GPIO (hdpnActiveState));	// unmute
			break;
	}

	debug2IOLog ("AppleTexasAudio::SndHWSetActiveOutput err %d\n", err);
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleTexasAudio::SetBiquadInfoToUnityAllPass (void) {
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
void AppleTexasAudio::SetUnityGainAllPass (void) {
	UInt32			prevLoadMode;
	int				biquadRefnum;
	DRCInfo			localDRC;
	
	//	save previous load mode
	prevLoadMode = 0 == (shadowRegs.sMCR[0] & (kFastLoad << kFL)) ? kSetNormalLoadMode : kSetFastLoadMode;
	debug3IOLog ("AppleTexasAudio::SetUnityGainAllPass (), shadowRegs.sMCR[0] %2X, prevLoadMode %ld\n", shadowRegs.sMCR[0], prevLoadMode);
	//	prepare for proper effect of fast load mode (i.e. unity all pass)
	if (kSetFastLoadMode == prevLoadMode)
		InitEQSerialMode (kSetNormalLoadMode, kRestoreOnNormal);
		
	//	force unity all pass biquad coefficients
	InitEQSerialMode (kSetFastLoadMode, kDontRestoreOnNormal);
	//	Set the biquad coefficients in the shadow registers to 'unity all pass' so that
	//	any future attempt to set the biquads is applied to the hardware registers (i.e.
	//	make sure that the shadow register accurately reflects the current state so that
	//	a data compare in the future does not cause a write operation to be bypassed).
	for (biquadRefnum = 0; biquadRefnum < kNumberOfBiquadsPerChannel; biquadRefnum++) {
		TAS3001C_WriteRegister (kLeftBiquad0CtrlReg	 + biquadRefnum, (UInt8*)kBiquad0db, kUPDATE_ALL);
		TAS3001C_WriteRegister (kRightBiquad0CtrlReg + biquadRefnum, (UInt8*)kBiquad0db, kUPDATE_ALL);
	}
	SetBiquadInfoToUnityAllPass ();
	InitEQSerialMode (kSetNormalLoadMode, kRestoreOnNormal);	//	go to normal load mode and restore registers after default
	
	//	Need to restore volume & mixer control registers after going to fast load mode
	localDRC.compressionRatioNumerator	= kDrcRatioNumerator;
	localDRC.compressionRatioDenominator	= kDrcRationDenominator;
	localDRC.threshold					= kDrcUnityThresholdHW;
	localDRC.maximumVolume				= kDefaultMaximumVolume;
	localDRC.enable						= false;

	SndHWSetDRC (&localDRC);

	//	restore previous load mode
	if (kSetFastLoadMode == prevLoadMode)
		InitEQSerialMode (kSetFastLoadMode, kDontRestoreOnNormal);

	debug3IOLog ("shadowRegs.sMCR[0] %8X, prevLoadMode %ld\n", shadowRegs.sMCR[0], prevLoadMode);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	When disabling Dynamic Range Compression, don't check the other elements
//	of the DRCInfo structure.  When enabling DRC, clip the compression threshold
//	to a valid range for the target hardware & validate that the compression
//	ratio is supported by the target hardware.	The maximumVolume argument
//	will dynamically apply the zero index reference point into the volume
//	gain translation table and will force an update of the volume registers.
IOReturn AppleTexasAudio::SndHWSetDRC( DRCInfoPtr theDRCSettings ) {
	IOReturn		err;
	UInt8			regData[kDRCwidth];
	Boolean			enableUpdated;
	
	err = kIOReturnSuccess;
	enableUpdated = false;
	FailWithAction( NULL == theDRCSettings, err = -50, Exit );
	debug2IOLog( "SndHWSetDRC( theDRCSettings %p )\n", theDRCSettings );
	debug3IOLog( "compressionRatioNumerator %ld, compressionRatioDenominator %ld\n", theDRCSettings->compressionRatioNumerator, theDRCSettings->compressionRatioDenominator );
	debug3IOLog( "threshold %ld, maximumVolume %ld\n", theDRCSettings->threshold, theDRCSettings->maximumVolume );
	debug2IOLog( "enable %d\n", theDRCSettings->enable );
	// FailWithAction( kDrcRatioNumerator != theDRCSettings->compressionRatioNumerator, err = -50, Exit );
	// FailWithAction( kDrcRationDenominator != theDRCSettings->compressionRatioDenominator, err = -50, Exit );
	// FailWithAction( kTumblerAbsMaxVolume < theDRCSettings->maximumVolume, err = -50, Exit );
	// FailWithAction( kTumblerMinVolume > theDRCSettings->maximumVolume, err = -50, Exit );
	
	if( TRUE == theDRCSettings->enable ) {
		debugIOLog( "enable DRC\n" );
		//	Turn off the dynamic range compression and update the globals DRC enable state.
		//	The compression threshold is represented by a 4.4 number with a value of 15.0
		//	representing 0.0 dB and decrementing 0.0625 dB per count as the threshold is
		//	moved down toward -36.0625 dB.	Each count represents 0.375 dB.
		regData[0] = ( kDrcEnable << kEN ) | ( kCompression3to1 << kCR );
		// divide by 1000 to remove the 1000's that the constants were multiplied by
		regData[1] = (UInt8)( kDrcUnityThresholdHW - ((SInt32)( -theDRCSettings->threshold / kDrcThresholdStepSize ) / 1000) );
		err = TAS3001C_WriteRegister( kDynamicRangeCtrlReg, regData, kUPDATE_ALL );
		FailIf( kIOReturnSuccess != err, Exit );
		
		if( drc.enable != theDRCSettings->enable ) {
			enableUpdated = true;
		}

		drc.enable = theDRCSettings->enable;
		debug2IOLog( "drc.compressionRatioNumerator %ld\n", drc.compressionRatioNumerator );
		debug2IOLog( "drc.compressionRatioDenominator %ld\n", drc.compressionRatioDenominator );
		debug2IOLog( "drc.threshold %ld\n", drc.threshold );
		debug2IOLog( "drc.enable %d\n", drc.enable );
		
		//	The current volume setting needs to be scaled against the new range of volume 
		//	control and applied to the hardware.
		if( drc.maximumVolume != theDRCSettings->maximumVolume || enableUpdated ) {
			drc.maximumVolume = theDRCSettings->maximumVolume;
		}
	} else {
		debugIOLog( "disable DRC\n" );
		//	Turn off the dynamic range compression and update the globals DRC enable state
		err = TAS3001C_ReadRegister( kDynamicRangeCtrlReg, regData );
		FailIf( kIOReturnSuccess != err, Exit );
		regData[0] = ( kDrcDisable << kEN ) | ( kCompression3to1 << kCR );	//	[2580249,2667007] Dynamic range control = disabled at 3:1 compression
		regData[1] = kDefaultCompThld;										//	[2580249] Default threshold is 0.0 dB
		err = TAS3001C_WriteRegister( kDynamicRangeCtrlReg, regData, kUPDATE_ALL );
		FailIf( kIOReturnSuccess != err, Exit );
		drc.enable = false;
	}

	drc.compressionRatioNumerator		= theDRCSettings->compressionRatioNumerator;
	drc.compressionRatioDenominator		= theDRCSettings->compressionRatioDenominator;
	drc.threshold						= theDRCSettings->threshold;
	drc.maximumVolume					= theDRCSettings->maximumVolume;

Exit:
	if( kIOReturnSuccess != err ) {
		debug2IOLog( "SndHWSetDRC: err = %d\n", err );
	}

	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	NOTE:	This function does not utilize fast mode loading as to do so would
//	revert all biquad coefficients not addressed by this execution instance to
//	unity all pass.	 Expect DSP processing delay if this function is used.	It
//	is recommended that SndHWSetOutputBiquadGroup be used instead.
IOReturn AppleTexasAudio::SndHWSetOutputBiquad( UInt32 streamID, UInt32 biquadRefNum, FourDotTwenty *biquadCoefficients )
{
	IOReturn		err;
	UInt32			coefficientIndex;
	UInt32			tumblerBiquadIndex;
	UInt32			biquadGroupIndex;
	// FourDotTwenty	coefficients;
	UInt8			tumblerBiquad[kTumblerCoefficientsPerBiquad * kTumblerNumBiquads];
	
#ifdef	kBIQUAD_VERBOSE
	// debug3IOLog( "SndHWSetOutputBiquad( '%0.4s', %0.2d )\n", &streamID, biquadRefNum );
#endif
	err = kIOReturnSuccess;
	FailWithAction( kTumblerMaxBiquadRefNum < biquadRefNum || NULL == biquadCoefficients, err = -50, Exit );
	FailWithAction( kStreamStereo != streamID && kStreamFrontLeft != streamID && kStreamFrontRight != streamID, err = -50, Exit );
	
	tumblerBiquadIndex = 0;
	biquadGroupIndex = biquadRefNum * kTumblerCoefficientsPerBiquad;
	if( kStreamFrontRight == streamID )
		biquadGroupIndex += kNumberOfBiquadCoefficientsPerChannel;
#ifdef	kEQ_VERBOSE
	debug3IOLog( "%0.4s %d : ", &streamID, biquadRefNum );
#endif
	for( coefficientIndex = 0; coefficientIndex < kTumblerCoefficientsPerBiquad; coefficientIndex++ )
	{
		// commented out because in this code biquadCoefficients is a double, not a FourDotTwenty
		// this saved the biquad info so that it could be read back later for verification (because you can't read the values from the hardware -- you have to remember what you wrote)
		// biquadGroupInfo[biquadGroupIndex] = biquadCoefficients[coefficientIndex];
		// if( kStreamStereo == streamID )
		//	biquadGroupInfo[biquadGroupIndex + kNumberOfBiquadCoefficientsPerChannel] = biquadCoefficients[coefficientIndex];
		// biquadGroupIndex++;
		
#if kEQ_VERBOSE
		// if( 0.0 <= biquadCoefficients[coefficientIndex] )
		//	IOLog( " +%3.10f ", biquadCoefficients[coefficientIndex] );
		// else
		//	IOLog( " %3.10f ", biquadCoefficients[coefficientIndex] );
#endif
		// DoubleToFourDotTwenty( biquadCoefficients[coefficientIndex], &coefficients );
		tumblerBiquad[tumblerBiquadIndex++] = biquadCoefficients[coefficientIndex].integerAndFraction1;
		tumblerBiquad[tumblerBiquadIndex++] = biquadCoefficients[coefficientIndex].fraction2;
		tumblerBiquad[tumblerBiquadIndex++] = biquadCoefficients[coefficientIndex].fraction3;
	}
	debugIOLog( "\n" );
	
	err = SetOutputBiquadCoefficients( streamID, biquadRefNum, tumblerBiquad );
Exit:
	if( kIOReturnSuccess != err )
		debug4IOLog( "err %d = SndHWSetOutputBiquad( '%4.4s', %ld )\n", err, (char*)&streamID, biquadRefNum );
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleTexasAudio::SndHWSetOutputBiquadGroup( UInt32 biquadFilterCount, FourDotTwenty *biquadCoefficients )
{
	UInt32			index;
	IOReturn		err;
	
	FailWithAction( 0 == biquadFilterCount || NULL == biquadCoefficients, err = -50, Exit );
	// debug3IOLog( "SndHWSetOutputBiquadGroup( %d, %2.6f )\n", biquadFilterCount, biquadCoefficients );
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
Exit:
	debug2IOLog( "err = %d\n", err );
	return err;
}

IOReturn AppleTexasAudio::SetOutputBiquadCoefficients( UInt32 streamID, UInt32 biquadRefNum, UInt8 *biquadCoefficients )
{
	IOReturn			err;
	
	debug4IOLog ( "SetOutputBiquadCoefficients( '%4.4s', %ld, %p )\n", (char*)&streamID, biquadRefNum, biquadCoefficients );
	err = kIOReturnSuccess;
	FailWithAction ( kTumblerMaxBiquadRefNum < biquadRefNum || NULL == biquadCoefficients, err = -50, Exit );
	FailWithAction ( kStreamStereo != streamID && kStreamFrontLeft != streamID && kStreamFrontRight != streamID, err = -50, Exit );

	switch ( biquadRefNum )
	{
		case kBiquadRefNum_0:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad0CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_1:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad1CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_2:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad2CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_3:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad3CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_4:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad4CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
		case kBiquadRefNum_5:
			switch( streamID )
			{
				case kStreamFrontLeft:	err = TAS3001C_WriteRegister( kLeftBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamFrontRight: err = TAS3001C_WriteRegister( kRightBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
				case kStreamStereo:		err = TAS3001C_WriteRegister( kLeftBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );
										err = TAS3001C_WriteRegister( kRightBiquad5CtrlReg, biquadCoefficients, kUPDATE_ALL );	break;
			}
			break;
	}

Exit:
	if( kIOReturnSuccess != err )
		debug5IOLog( "err %d = SetOutputBiquadCoefficients( '%4.4s', %ld, %p )\n", err, (char*)&streamID, biquadRefNum, biquadCoefficients );
	return err;
}

#pragma mark +I2C FUNCTIONS
// Taken from PPCDACA.cpp

// --------------------------------------------------------------------------
// Method: getI2CPort
//
// Purpose:
//		  returns the i2c port to use for the audio chip.
UInt32 AppleTexasAudio::getI2CPort()
{
	if(ourProvider) {
		OSData *t;
		
		t = OSDynamicCast(OSData, ourProvider->getProperty("AAPL,i2c-port-select"));	// we don't need a port select on Tangent, but look anyway
		if (t != NULL) {
			UInt32 myPort = *((UInt32*)t->getBytesNoCopy());
			return myPort;
		}
		// else
		//	debugIOLog( "AppleTexasAudio::getI2CPort missing property port, but that's not necessarily a problem\n");
	}

	return 0;
}

// --------------------------------------------------------------------------
// Method: openI2C
//
// Purpose:
//		  opens and sets up the i2c bus
bool AppleTexasAudio::openI2C()
{
	FailIf (NULL == interface, Exit);

	// Open the interface and sets it in the wanted mode:
	FailIf (!interface->openI2CBus (getI2CPort()), Exit);
	interface->setStandardSubMode ();

	if (!interface->setPollingMode (mPollingMode)) {
		mPollingMode = true;
		interface->setPollingMode (mPollingMode);
	}

	return true;

Exit:
	return false;
}


// --------------------------------------------------------------------------
// Method: closeI2C
//
// Purpose:
//		  closes the i2c bus
void AppleTexasAudio::closeI2C ()
{
	// Closes the bus so other can access to it:
	interface->closeI2CBus ();
}

// --------------------------------------------------------------------------
// Method: findAndAttachI2C
//
// Purpose:
//	 Attaches to the i2c interface:
bool AppleTexasAudio::findAndAttachI2C(IOService *provider)
{
	const OSSymbol *i2cDriverName;
	IOService *i2cCandidate;

	// Searches the i2c:
	i2cDriverName = OSSymbol::withCStringNoCopy("PPCI2CInterface.i2c-mac-io");
	i2cCandidate = waitForService(resourceMatching(i2cDriverName));
	// interface = OSDynamicCast(PPCI2CInterface, i2cCandidate->getProperty(i2cDriverName));
	interface = (PPCI2CInterface*)i2cCandidate->getProperty(i2cDriverName);

	if (interface == NULL) {
		debugIOLog("AppleTexasAudio::findAndAttachI2C can't find the i2c in the registry\n");
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
bool AppleTexasAudio::detachFromI2C(IOService* /*provider*/)
{
	if (interface) {
		// delete interface;
		interface->release();
		interface = 0;
	}
		
	return (true);
}

// --------------------------------------------------------------------------
// Method: frameRate
//
// Purpose:
//		  returns the frame rate as in the registry, if it is
//		  not found in the registry, it returns the default value.
#define kCommonFrameRate 44100

UInt32 AppleTexasAudio::frameRate(UInt32 index)
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

				debug2IOLog( "AppleTexasAudio::frameRate (%ld)\n",	fR);
				return fR;
			}
		}
	}

	return (UInt32)kCommonFrameRate;
}
