/*
 *  KeyLargoPlatform.cpp
 *  
 *  Created by Aram Lindahl on Mon Mar 10 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include "KeyLargoPlatform.h"

#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"
#include "AudioHardwareCommon.h"
#include "AppleOnboardAudio.h"

#define super PlatformInterface

OSDefineMetaClassAndStructors(KeyLargoPlatform, PlatformInterface)

const UInt32 KeyLargoPlatform::kFCR0Offset					= 0x00000038;
const UInt32 KeyLargoPlatform::kFCR1Offset					= 0x0000003C;
const UInt32 KeyLargoPlatform::kFCR2Offset					= 0x00000040;
const UInt32 KeyLargoPlatform::kFCR3Offset					= 0x00000044;
const UInt32 KeyLargoPlatform::kFCR4Offset					= 0x00000048;

const UInt16 KeyLargoPlatform::kAPPLE_IO_CONFIGURATION_SIZE = 256;
const UInt16 KeyLargoPlatform::kI2S_IO_CONFIGURATION_SIZE	= 256;

const UInt32 KeyLargoPlatform::kI2S0BaseOffset				= 0x10000;							/*	mapped by AudioI2SControl	*/
const UInt32 KeyLargoPlatform::kI2S1BaseOffset				= 0x11000;							/*	mapped by AudioI2SControl	*/

const UInt32 KeyLargoPlatform::kI2SIntCtlOffset				= 0x0000;
const UInt32 KeyLargoPlatform::kI2SSerialFormatOffset		= 0x0010;
const UInt32 KeyLargoPlatform::kI2SCodecMsgOutOffset		= 0x0020;
const UInt32 KeyLargoPlatform::kI2SCodecMsgInOffset			= 0x0030;
const UInt32 KeyLargoPlatform::kI2SFrameCountOffset			= 0x0040;
const UInt32 KeyLargoPlatform::kI2SFrameMatchOffset			= 0x0050;
const UInt32 KeyLargoPlatform::kI2SDataWordSizesOffset		= 0x0060;
const UInt32 KeyLargoPlatform::kI2SPeakLevelSelOffset		= 0x0070;
const UInt32 KeyLargoPlatform::kI2SPeakLevelIn0Offset		= 0x0080;
const UInt32 KeyLargoPlatform::kI2SPeakLevelIn1Offset		= 0x0090;

const UInt32 KeyLargoPlatform::kI2SClockOffset				= 0x0003C;							/*	FCR1 offset (not mapped by AudioI2SControl)	*/
const UInt32 KeyLargoPlatform::kI2S0ClockEnable 		    = ( 1 << kI2S0ClkEnBit );
const UInt32 KeyLargoPlatform::kI2S1ClockEnable 			= ( 1 << kI2S1ClkEnBit );
const UInt32 KeyLargoPlatform::kI2S0CellEnable	 		    = ( 1 << kI2S0CellEn );
const UInt32 KeyLargoPlatform::kI2S1CellEnable	 			= ( 1 << kI2S1CellEn );
const UInt32 KeyLargoPlatform::kI2S0InterfaceEnable 		= ( 1 << kI2S0Enable );
const UInt32 KeyLargoPlatform::kI2S1InterfaceEnable 		= ( 1 << kI2S1Enable );

const char*  KeyLargoPlatform::kAmpMuteEntry				= "amp-mute";
const char*  KeyLargoPlatform::kAnalogHWResetEntry			= "audio-hw-reset";

const char*	 KeyLargoPlatform::kCodecErrorIrqTypeEntry		= "codec-error-irq";
const char*	 KeyLargoPlatform::kCodecIrqTypeEntry			= "codec-irq";
const char*	 KeyLargoPlatform::kComboInJackTypeEntry		= "combo-input-type";
const char*	 KeyLargoPlatform::kComboOutJackTypeEntry		= "combo-output-type";
const char*  KeyLargoPlatform::kDigitalHWResetEntry			= "audio-dig-hw-reset";
const char*	 KeyLargoPlatform::kDigitalInDetectEntry		= "digital-input-detect";
const char*	 KeyLargoPlatform::kDigitalOutDetectEntry		= "digital-output-detect";
const char*  KeyLargoPlatform::kHeadphoneDetectInt			= "headphone-detect";
const char*  KeyLargoPlatform::kHeadphoneMuteEntry 			= "headphone-mute";
const char*	 KeyLargoPlatform::kInternalSpeakerIDEntry		= "internal-speaker-id";
const char*  KeyLargoPlatform::kLineInDetectInt				= "line-input-detect";
const char*  KeyLargoPlatform::kLineOutDetectInt			= "line-output-detect";
const char*  KeyLargoPlatform::kLineOutMuteEntry			= "line-output-mute";
const char*  KeyLargoPlatform::kSpeakerDetectEntry			= "speaker-detect";

const char*  KeyLargoPlatform::kNumInputs					= "#-inputs";
const char*  KeyLargoPlatform::kI2CAddress					= "i2c-address";
const char*  KeyLargoPlatform::kAudioGPIO					= "audio-gpio";
const char*  KeyLargoPlatform::kAudioGPIOActiveState		= "audio-gpio-active-state";
const char*  KeyLargoPlatform::kIOInterruptControllers		= "IOInterruptControllers";

#pragma mark ---------------------------
#pragma mark ¥ Platform Interface Methods
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
bool KeyLargoPlatform::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex) {
	Boolean					result;

	IOService* 				theService;

	IORegistryEntry			*sound;
	IORegistryEntry			*macio;
	IORegistryEntry			*gpio;
	IORegistryEntry			*i2s;
	IORegistryEntry			*i2sParent;
	OSData					*tmpData;
	IOMemoryMap				*map;

	OSDictionary*			keyLargoDictionary;

	debugIOLog (3, "+ KeyLargoPlatform::init - device = %p", device);

	result = super::init (device, provider, inDBDMADeviceIndex);
	if (!result)
		return result;

	mKLI2SPowerSymbolName = OSSymbol::withCString ("keyLargo_powerI2S"); // [3324205]
	FailWithAction (NULL == mKLI2SPowerSymbolName, result = false, Exit);
	
	keyLargoDictionary = IOService::serviceMatching ("KeyLargo");
	// use to call platform functions 
	mKeyLargoService = IOService::waitForService (keyLargoDictionary);
	// could retain here...

	debugIOLog (3, "mKeyLargoService = %p", mKeyLargoService);
	FailWithAction (NULL == mKeyLargoService, result = false, Exit);
	
	//  mac-io : is2 : i2s-x : sound : AOA

	sound = device;

	FailWithAction (!sound, result = false, Exit);
	debugIOLog (3, "KeyLargoPlatform - sound's name is %s", sound->getName ());

	i2s = sound->getParentEntry (gIODTPlane);
	FailWithAction (!i2s, result = false, Exit);
	debugIOLog (3, "KeyLargoPlatform - i2s's name is %s", i2s->getName ());

	i2sParent = i2s->getParentEntry (gIODTPlane);
	FailWithAction (!i2sParent, result = false, Exit);
	debugIOLog (3, "KeyLargoPlatform - i2s's name is %s", i2sParent->getName ());

	macio = i2sParent->getParentEntry (gIODTPlane);
	FailWithAction (!macio, result = false, Exit);
	debugIOLog (3, "KeyLargoPlatform - macio's name is %s", macio->getName ());
	
	gpio = macio->childFromPath (kGPIODTEntry, gIODTPlane);
	FailWithAction (!gpio, result = false, Exit);
	debugIOLog (3, "KeyLargoPlatform - gpio's name is %s", gpio->getName ());

	
	FailWithAction (!findAndAttachI2C(), result = false, Exit);

	
	tmpData = OSDynamicCast(OSData, sound->getProperty("AAPL,i2c-port-select"));		
	if (tmpData != NULL) {
		mI2CPort = *((UInt32*)tmpData->getBytesNoCopy());
	}
	debugIOLog (3, "KeyLargoPlatform - mI2CPort = %ld", mI2CPort);

	initAudioGpioPtr ( gpio, kAmpMuteEntry, &mAmplifierMuteGpio, &mAmplifierMuteActiveState, NULL );											//	control - no interrupt
	initAudioGpioPtr ( gpio, kAnalogHWResetEntry, &mAnalogResetGpio, &mAnalogResetActiveState, NULL );											//	control - no interrupt
	initAudioGpioPtr ( gpio, kCodecErrorIrqTypeEntry, &mCodecErrorInterruptGpio, &mCodecErrorInterruptActiveState, &mCodecErrorIntProvider );	//	control - does interrupt
	initAudioGpioPtr ( gpio, kCodecIrqTypeEntry, &mCodecInterruptGpio, &mCodecInterruptActiveState, &mCodecIntProvider );						//	control - does interrupt
	initAudioGpioPtr ( gpio, kComboInJackTypeEntry, &mComboInJackTypeGpio, &mComboInJackTypeActiveState, NULL );								//	control - no interrupt
	initAudioGpioPtr ( gpio, kComboOutJackTypeEntry, &mComboOutJackTypeGpio, &mComboOutJackTypeActiveState, NULL );								//	control - no interrupt
	initAudioGpioPtr ( gpio, kDigitalHWResetEntry, &mDigitalResetGpio, &mDigitalResetActiveState, NULL );										//	control - no interrupt
	initAudioGpioPtr ( gpio, kDigitalInDetectEntry, &mDigitalInDetectGpio, &mDigitalInDetectActiveState, &mDigitalInDetectIntProvider );		//	detect  - does interrupt
	initAudioGpioPtr ( gpio, kDigitalOutDetectEntry, &mDigitalOutDetectGpio, &mDigitalOutDetectActiveState, &mDigitalOutDetectIntProvider );	//	detect  - does interrupt
	initAudioGpioPtr ( gpio, kHeadphoneDetectInt, &mHeadphoneDetectGpio, &mHeadphoneDetectActiveState, &mHeadphoneDetectIntProvider );			//	detect  - does interrupt
	initAudioGpioPtr ( gpio, kHeadphoneMuteEntry, &mHeadphoneMuteGpio, &mHeadphoneMuteActiveState, NULL );										//	control - no interrupt
	initAudioGpioPtr ( gpio, kInternalSpeakerIDEntry, &mInternalSpeakerIDGpio, &mInternalSpeakerIDActiveState, NULL );							//	control - no interrupt
	initAudioGpioPtr ( gpio, kLineInDetectInt, &mLineInDetectGpio, &mLineInDetectActiveState, &mDigitalInDetectIntProvider );					//	detect  - does interrupt
	initAudioGpioPtr ( gpio, kLineOutDetectInt, &mLineOutDetectGpio, &mLineOutDetectActiveState, &mLineOutDetectIntProvider );					//	detect  - does interrupt
	initAudioGpioPtr ( gpio, kLineOutMuteEntry, &mLineOutMuteGpio, &mLineOutMuteActiveState, NULL );											//	control - no interrupt
	initAudioGpioPtr ( gpio, kSpeakerDetectEntry, &mSpeakerDetectGpio, &mSpeakerDetectActiveState, &mSpeakerDetectIntProvider );				//	detect  - does interrupt

	theService = (OSDynamicCast(IOService, i2s));
	FailWithAction (!theService, result = false, Exit);

	map = theService->mapDeviceMemoryWithIndex ( inDBDMADeviceIndex );
	FailWithAction (!map, result = false, Exit);
	FailWithAction (kIOReturnSuccess != initI2S(map), result = false, Exit);
	
	debugIOLog (3, "- KeyLargoPlatform::init");

Exit:
	return result;
}

//	--------------------------------------------------------------------------------
void KeyLargoPlatform::free() {
	debugIOLog (3, "+ KeyLargoPlatform::free()");

 	if (NULL != mIOBaseAddressMemory) {
		mIOBaseAddressMemory->release();
	}

// docs are unclear on whether waitForService adds to the retain count of the service it returns, 
// this panic'd once with the release.
//	CLEAN_RELEASE(mKeyLargoService);

	detachFromI2C();

	super::free();

	debugIOLog (3, "- KeyLargoPlatform::free()");
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::performPlatformSleep ( void ) {
	unregisterInterrupts();
	return kIOReturnSuccess;
}


//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::performPlatformWake ( IOService * device ) {
	registerInterrupts ( (IOService*)this );	//	[3585556]	registerInterrupts requires passing the platform interface object!
	return kIOReturnSuccess;
}


#pragma mark ---------------------------
#pragma mark ¥ Codec Methods	
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
bool KeyLargoPlatform::writeCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode) {
	bool success = false;

	if (openI2C()) {
		switch (mode) {
			case kI2C_StandardMode:			mI2CInterface->setStandardMode();			break;
			case kI2C_StandardSubMode:		mI2CInterface->setStandardSubMode();		break;
			case kI2C_CombinedMode:			mI2CInterface->setCombinedMode();			break;
			default:
				debugIOLog (3, "KeyLargoPlatform::writeCodecRegister() unknown bus mode!");
				FailIf ( true, Exit );
				break;
		}		
		
		mI2CInterface->setPollingMode ( false );

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
		success = mI2CInterface->writeI2CBus (address >> 1, subAddress, data, len);
		mI2C_lastTransactionResult = success;
		
		if (!success) debugIOLog (3,  "KeyLargoPlatform::writeCodecRegister(%X, %X, %p %d), mI2CInterface->writeI2CBus returned false.", address, subAddress, data, len );
Exit:
		closeI2C();
	} else {
		debugIOLog (3, "KeyLargoPlatform::writeCodecRegister() couldn't open the I2C bus!");
	}
	return success;
}

//	--------------------------------------------------------------------------------
bool KeyLargoPlatform::readCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode) {
	bool success = false;

	if (openI2C()) {
		switch (mode) {
			case kI2C_StandardMode:			mI2CInterface->setStandardMode();			break;
			case kI2C_StandardSubMode:		mI2CInterface->setStandardSubMode();		break;
			case kI2C_CombinedMode:			mI2CInterface->setCombinedMode();			break;
			default:
				debugIOLog (3, "KeyLargoPlatform::readCodecRegister() unknown bus mode!");
				FailIf ( true, Exit );
				break;
		}		
		
		mI2CInterface->setPollingMode ( false );

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
		success = mI2CInterface->readI2CBus (address >> 1, subAddress, data, len);
		mI2C_lastTransactionResult = success;

		if (!success) debugIOLog (3, "KeyLargoPlatform::readCodecRegister(), mI2CInterface->writeI2CBus returned false.");
Exit:
		closeI2C();
	} else {
		debugIOLog (3, "KeyLargoPlatform::readCodecRegister() couldn't open the I2C bus!");
	}
	return success;
}

#pragma mark ---------------------------
#pragma mark ¥ I2S
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setI2SEnable (bool enable) {
	UInt32 regValue;

	debugIOLog (3,  "KeyLargoPlatform::setI2SEnable %s", enable ? "TRUE" : "FALSE");

	regValue = getKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset );						

	switch ( mI2SInterfaceNumber ) {
		case kUseI2SCell0:
			if (enable)
				regValue |= kI2S0InterfaceEnable;
			else
				regValue &= ~kI2S0InterfaceEnable;
			break;
		case kUseI2SCell1:	
			if (enable)
				regValue |= kI2S1InterfaceEnable;
			else
				regValue &= ~kI2S1InterfaceEnable;
			break;
		default:
			enable = false;
			break;
	}
	setKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset, regValue );
	
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
bool KeyLargoPlatform::getI2SEnable () {
	bool enable;
	UInt32 regValue;

	debugIOLog (3,  "KeyLargoPlatform::getI2SEnable");

	regValue = getKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset );						

	switch ( mI2SInterfaceNumber ) {
		case kUseI2SCell0:	
			enable = regValue & kI2S0InterfaceEnable;
			break;
		case kUseI2SCell1:	
			enable = regValue & kI2S1InterfaceEnable;
			break;
		default:
			enable = false;
			break;
	}
	
	return enable;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setI2SClockEnable (bool enable) {
	UInt32 regValue;

	debugIOLog (3,  "+ KeyLargoPlatform::setI2SClockEnable %s", enable ? "TRUE" : "FALSE");

	regValue = getKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset );						

	switch ( mI2SInterfaceNumber ) {
		case kUseI2SCell0:	
			if (enable)
				regValue |= kI2S0ClockEnable;
			else
				regValue &= ~kI2S0ClockEnable;
			break;
		case kUseI2SCell1:	
			if (enable)
				regValue |= kI2S1ClockEnable;
			else
				regValue &= ~kI2S1ClockEnable;
			break;
		default:
			enable = false;
			break;
	}

	setKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset, regValue );
	debugIOLog (3,  "- KeyLargoPlatform::setI2SClockEnable %s returns", enable ? "TRUE" : "FALSE");
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
bool KeyLargoPlatform::getI2SClockEnable () {
	bool enable;
	UInt32 regValue;

	debugIOLog (3,  "KeyLargoPlatform::getI2SClockEnable" );

	regValue = getKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset );						

	switch ( mI2SInterfaceNumber ) {
		case kUseI2SCell0:	
			enable = regValue & kI2S0ClockEnable;
			break;
		case kUseI2SCell1:	
			enable = regValue & kI2S1ClockEnable;
			break;
		default:
			enable = false;
			break;
	}
	
	return enable;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setI2SCellEnable (bool enable) {
	UInt32 regValue;

	debugIOLog (3,  "KeyLargoPlatform::setI2SCellEnable %s", enable ? "TRUE" : "FALSE");

	regValue = getKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset );						

	switch ( mI2SInterfaceNumber ) {
		case kUseI2SCell0:
			if (enable)
				regValue |= kI2S0CellEnable;
			else
				regValue &= ~kI2S0CellEnable;
			break;
		case kUseI2SCell1:	
			if (enable)
				regValue |= kI2S1CellEnable;
			else
				regValue &= ~kI2S1CellEnable;
			break;
		default:
			enable = false;
			break;
	}

	setKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset, regValue );
	
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
bool KeyLargoPlatform::getI2SCellEnable () {
	bool enable;
	UInt32 regValue;

	debugIOLog (3,  "KeyLargoPlatform::getI2SCellEnable" );

	regValue = getKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset );						

	switch ( mI2SInterfaceNumber ) {
		case kUseI2SCell0:	
			enable = regValue & kI2S0CellEnable;
			break;
		case kUseI2SCell1:	
			enable = regValue & kI2S1CellEnable;
			break;
		default:
			enable = false;
			break;
	}
	
	return enable;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setSerialFormatRegister (UInt32 serialFormat) {
	debugIOLog (3,  "KeyLargoPlatform::setSerialFormatRegister (0x%lX)", serialFormat);

	OSWriteLittleInt32(mI2SBaseAddress, kI2SSerialFormatOffset, serialFormat);
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
UInt32 KeyLargoPlatform::getSerialFormatRegister () {
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SSerialFormatOffset);
	debugIOLog (3,  "KeyLargoPlatform::getSerialFormatRegister = 0x%lX", result);
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setDataWordSizes (UInt32 dataWordSizes) {
	debugIOLog (3,  "KeyLargoPlatform::setDataWordSizes (0x%lX)", dataWordSizes);

	OSWriteLittleInt32(mI2SBaseAddress, kI2SDataWordSizesOffset, dataWordSizes);

	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
UInt32 KeyLargoPlatform::getDataWordSizes () {
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SDataWordSizesOffset);
	debugIOLog (3,  "KeyLargoPlatform::getDataWordSizes = 0x%lX", result);
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setI2SIOMIntControl (UInt32 intCntrl) {
	debugIOLog (3,  "KeyLargoPlatform::setI2SIOMIntControl (0x%lX)", intCntrl);

	OSWriteLittleInt32(mI2SBaseAddress, kI2SIntCtlOffset, intCntrl);

	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
UInt32 KeyLargoPlatform::getI2SIOMIntControl () {
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SIntCtlOffset);
	debugIOLog (3,  "KeyLargoPlatform::getI2SIOMIntControl = 0x%lX", result);
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setPeakLevel ( UInt32 channelTarget, UInt32 levelMeterValue ) {
	IOReturn		result = kIOReturnSuccess;
	
	debugIOLog (3,  "+ KeyLargoPlatform::setPeakLevel ( '%4s', %lX )", (char*)&channelTarget, levelMeterValue );
	switch ( channelTarget ) {
		case kStreamFrontLeft:		OSWriteLittleInt32 ( mI2SBaseAddress, kI2SPeakLevelIn0Offset, levelMeterValue );		break;
		case kStreamFrontRight:		OSWriteLittleInt32 ( mI2SBaseAddress, kI2SPeakLevelIn1Offset, levelMeterValue );		break;
		default:					result = kIOReturnBadArgument;															break;
	}
	debugIOLog (3,  "- KeyLargoPlatform::setPeakLevel ( '%4s', %lX ) returns %X", (char*)&channelTarget, levelMeterValue, result );
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 KeyLargoPlatform::getPeakLevel ( UInt32 channelTarget ) {
	UInt32			result;
	
	debugIOLog (3,  "+ KeyLargoPlatform::getPeakLevel ( '%4s' )", (char*)&channelTarget );
	switch ( channelTarget ) {
		case kStreamFrontLeft:		result = OSReadLittleInt32 ( mI2SBaseAddress, kI2SPeakLevelIn0Offset );		break;
		case kStreamFrontRight:		result = OSReadLittleInt32 ( mI2SBaseAddress, kI2SPeakLevelIn1Offset );		break;
		default:					result = 0;																	break;
	}
	debugIOLog (3,  "- KeyLargoPlatform::getPeakLevel ( '%4s' ) returns %lX", (char*)&channelTarget, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::releaseI2SClockSource(I2SClockFrequency inFrequency)	
{
	//	NOTE:	This needs a new KeyLargo driver from Tom LaPerre.  Coordinate for Q37 / Q27.  (see Ray)
    if (NULL != mKeyLargoService && NULL != mKLI2SPowerSymbolName) {
		switch ( mI2SInterfaceNumber ) {
			case kUseI2SCell0:	
				switch ( inFrequency ) {
					case kI2S_45MHz:
						mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)0, (void *)0, (void *)0);
						break;
					case kI2S_49MHz:
						mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)0, (void *)1, (void *)0);
						break;
					case kI2S_18MHz:
						mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)0, (void *)2, (void *)0);
						break;
				}
				break;
			case kUseI2SCell1:
				switch ( inFrequency ) {
					case kI2S_45MHz:
						mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)1, (void *)0, (void *)0);
						break;
					case kI2S_49MHz:
						mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)1, (void *)1, (void *)0);
						break;
					case kI2S_18MHz:
						mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)1, (void *)2, (void *)0);
						break;
				}
				break;
			default:
				break;
		}
	}
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::requestI2SClockSource(I2SClockFrequency inFrequency)	
{
	IOReturn 				result;
	
	result = kIOReturnError;
	//	NOTE:	This needs a new KeyLargo driver from Tom LaPerre.  Coordinate for Q37 / Q27.  (see Ray)
    if (NULL != mKeyLargoService && NULL != mKLI2SPowerSymbolName) {
		switch ( mI2SInterfaceNumber ) {
			case kUseI2SCell0:	
				switch ( inFrequency ) {
					case kI2S_45MHz:
						result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)0, (void *)0, (void *)0);
						break;
					case kI2S_49MHz:
						result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)0, (void *)1, (void *)0);
						break;
					case kI2S_18MHz:
						result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)0, (void *)2, (void *)0);
						break;
				}
				break;
			case kUseI2SCell1:	
				switch ( inFrequency ) {
					case kI2S_45MHz:
						result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)1, (void *)0, (void *)0);
						break;
					case kI2S_49MHz:
						result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)1, (void *)1, (void *)0);
						break;
					case kI2S_18MHz:
						result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)1, (void *)2, (void *)0);
						break;
				}
				break;
			default:
				break;
		}
	}
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setFrameCount ( UInt32 value ) {
	OSWriteLittleInt32 ( mI2SBaseAddress, kI2SFrameCountOffset, value );
	return kIOReturnSuccess;
}

//	--------------------------------------------------------------------------------
UInt32 KeyLargoPlatform::getFrameCount () {
	return OSReadLittleInt32(mI2SBaseAddress, kI2SFrameCountOffset);
}

#pragma mark ---------------------------
#pragma mark ¥ GPIO
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
void KeyLargoPlatform::initAudioGpioPtr ( const IORegistryEntry * start, const char * gpioName, GpioPtr* gpioH, GpioActiveState* gpioActiveStatePtr, IOService ** intProvider ) {
    IORegistryEntry			*theRegEntry;
	OSData					*theProperty;
	IOMemoryMap				*map;
	IODeviceMemory			*gpioRegMem;
	UInt32					*theGpioAddr;
	UInt32					*tmpPtr;
	
	theRegEntry = FindEntryByProperty (start, kAudioGPIO, gpioName);
	if ( NULL != theRegEntry ) {
		if ( NULL != intProvider ) {
			*intProvider = OSDynamicCast ( IOService, theRegEntry );
		}
		theProperty = OSDynamicCast ( OSData, theRegEntry->getProperty ( kAAPLAddress ) );
		if ( NULL != theProperty ) {
			theGpioAddr = (UInt32*)theProperty->getBytesNoCopy();
            if ( NULL != theGpioAddr ) {
                debugIOLog (3, "KeyLargoPlatform - %s = %p", gpioName, theGpioAddr);
				if ( NULL != gpioActiveStatePtr ) {
					theProperty = OSDynamicCast ( OSData, theRegEntry->getProperty ( kAudioGPIOActiveState ) );
					if ( NULL != theProperty ) {
						tmpPtr = (UInt32*)theProperty->getBytesNoCopy();
						debugIOLog (3, "KeyLargoPlatform - %s active state = 0x%X", gpioName, *gpioActiveStatePtr);
						*gpioActiveStatePtr = *tmpPtr;
					} else {
						*gpioActiveStatePtr = 1;
					}
				}
				if ( NULL != gpioH ) {
                    //	Take the hard coded memory address that's in the boot rom and convert it to a virtual address
                    gpioRegMem = IODeviceMemory::withRange ( *theGpioAddr, sizeof ( UInt8 ) );
                    map = gpioRegMem->map ( 0 );
                    *gpioH = (UInt8*)map->getVirtualAddress();
				}
            }
		}
	}	
}
	
//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::getGpioPtrAndActiveState ( GPIOSelector theGpio, GpioPtr * gpioPtrPtr, GpioActiveState * activeStatePtr ) {
	IOReturn			result;
	
	result = kIOReturnBadArgument;
	if ( NULL != gpioPtrPtr && NULL != activeStatePtr ) {
		switch ( theGpio ) {
			case kGPIO_Selector_AnalogCodecReset:		*gpioPtrPtr = mAnalogResetGpio;				*activeStatePtr = mAnalogResetActiveState;			break;
			case kGPIO_Selector_ClockMux:				*gpioPtrPtr = mClockMuxGpio;				*activeStatePtr = mClockMuxActiveState;				break;
			case kGPIO_Selector_CodecErrorInterrupt:	*gpioPtrPtr = mCodecInterruptGpio;			*activeStatePtr = mCodecInterruptActiveState;		break;
			case kGPIO_Selector_CodecInterrupt:			*gpioPtrPtr = mCodecErrorInterruptGpio;		*activeStatePtr = mCodecErrorInterruptActiveState;	break;
			case kGPIO_Selector_ComboInJackType:		*gpioPtrPtr = mComboInJackTypeGpio;			*activeStatePtr = mDigitalOutDetectActiveState;		break;
			case kGPIO_Selector_ComboOutJackType:		*gpioPtrPtr = mComboOutJackTypeGpio;		*activeStatePtr = mDigitalResetActiveState;			break;
			case kGPIO_Selector_DigitalCodecReset:		*gpioPtrPtr = mDigitalResetGpio;			*activeStatePtr = mDigitalInDetectActiveState;		break;
			case kGPIO_Selector_DigitalInDetect:		*gpioPtrPtr = mDigitalInDetectGpio;			*activeStatePtr = mComboInJackTypeActiveState;		break;
			case kGPIO_Selector_DigitalOutDetect:		*gpioPtrPtr = mDigitalOutDetectGpio;		*activeStatePtr = mComboOutJackTypeActiveState;		break;
			case kGPIO_Selector_HeadphoneDetect:		*gpioPtrPtr = mHeadphoneDetectGpio;			*activeStatePtr = mHeadphoneDetectActiveState;		break;
			case kGPIO_Selector_HeadphoneMute:			*gpioPtrPtr = mHeadphoneMuteGpio;			*activeStatePtr = mHeadphoneMuteActiveState;		break;
			case kGPIO_Selector_InputDataMux:			*gpioPtrPtr = mInputDataMuxGpio;			*activeStatePtr = mInputDataMuxActiveState;			break;
			case kGPIO_Selector_InternalSpeakerID:		*gpioPtrPtr = mInternalSpeakerIDGpio;		*activeStatePtr = mInternalSpeakerIDActiveState;	break;
			case kGPIO_Selector_LineInDetect:			*gpioPtrPtr = mLineInDetectGpio;			*activeStatePtr = mLineInDetectActiveState;			break;
			case kGPIO_Selector_LineOutDetect:			*gpioPtrPtr = mLineOutDetectGpio;			*activeStatePtr = mLineOutDetectActiveState;		break;
			case kGPIO_Selector_LineOutMute:			*gpioPtrPtr = mLineOutMuteGpio;				*activeStatePtr = mLineOutMuteActiveState;			break;
			case kGPIO_Selector_SpeakerDetect:			*gpioPtrPtr = mSpeakerDetectGpio;			*activeStatePtr = mSpeakerDetectActiveState;		break;
			case kGPIO_Selector_SpeakerMute:			*gpioPtrPtr = mAmplifierMuteGpio;			*activeStatePtr = mAmplifierMuteActiveState;		break;
			case kGPIO_Selector_ExternalMicDetect:																										break;
			case kGPIO_Selector_NotAssociated:																											break;
		}
		if ( NULL != *gpioPtrPtr ) {
			result = kIOReturnSuccess;
		}
	}
	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getGpioAttributes ( GPIOSelector theGpio ) {
	UInt8					gpioValue;
	GpioAttributes			result;
	GpioPtr					gpioPtr;
	GpioActiveState			activeState;

	result = kGPIO_Unknown;
	gpioValue = 0;
	gpioPtr = NULL;
	activeState = 1;

	getGpioPtrAndActiveState ( theGpio, &gpioPtr, &activeState );
	if ( NULL != gpioPtr ) {
		gpioValue = *gpioPtr;
	
		switch ( theGpio ) {
			case kGPIO_Selector_AnalogCodecReset:
			case kGPIO_Selector_DigitalCodecReset:
				if ( ( gpioValue & ( gpioBIT_MASK << gpioPIN_RO ) ) == ( activeState << gpioPIN_RO ) ) {
					result = kGPIO_Reset;
				} else {
					result = kGPIO_Run;
				}
				break;
			case kGPIO_Selector_ClockMux:
			case kGPIO_Selector_InputDataMux:
				if ( ( gpioValue & ( gpioBIT_MASK << gpioPIN_RO ) ) == ( activeState << gpioPIN_RO ) ) {
					result = kGPIO_MuxSelectAlternate;
				} else {
					result = kGPIO_MuxSelectDefault;
				}
				break;
			case kGPIO_Selector_CodecErrorInterrupt:
			case kGPIO_Selector_CodecInterrupt:
				if ( ( gpioValue & ( gpioBIT_MASK << gpioPIN_RO ) ) == ( activeState << gpioPIN_RO ) ) {
					result = kGPIO_CodecInterruptActive;
				} else {
					result = kGPIO_CodecInterruptInactive;
				}
				break;
			case kGPIO_Selector_DigitalInDetect:
			case kGPIO_Selector_ComboInJackType:
			case kGPIO_Selector_DigitalOutDetect:
			case kGPIO_Selector_ComboOutJackType:
			case kGPIO_Selector_HeadphoneDetect:
			case kGPIO_Selector_InternalSpeakerID:
			case kGPIO_Selector_LineInDetect:
			case kGPIO_Selector_LineOutDetect:
			case kGPIO_Selector_SpeakerDetect:
				if ( kGPIO_Selector_HeadphoneDetect == theGpio ) { *gpioPtr |= 0x80; }	//	BROWN PATCH - TEMPORARY!!!
				if ( ( gpioValue & ( gpioBIT_MASK << gpioPIN_RO ) ) == ( activeState << gpioPIN_RO ) ) {
					result = kGPIO_Connected;
				} else {
					result = kGPIO_Disconnected;
				}
				break;
			case kGPIO_Selector_HeadphoneMute:
			case kGPIO_Selector_LineOutMute:
			case kGPIO_Selector_SpeakerMute:
				if ( ( gpioValue & ( gpioBIT_MASK << gpioPIN_RO ) ) == ( activeState << gpioPIN_RO ) ) {
					result = kGPIO_Muted;
				} else {
					result = kGPIO_Unmuted;
				}
				break;
			case kGPIO_Selector_ExternalMicDetect:
				break;
			case kGPIO_Selector_NotAssociated:
				break;
		}
	}
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setGpioAttributes ( GPIOSelector theGpio, GpioAttributes attributes ) {
	UInt8					gpioValue;
	GpioPtr					gpioPtr;
	GpioActiveState			activeState;
	IOReturn				result;

	gpioValue = 0;
	gpioPtr = NULL;
	activeState = 1;
	result = kIOReturnBadArgument;

	getGpioPtrAndActiveState ( theGpio, &gpioPtr, &activeState );
	if ( NULL != gpioPtr ) {
		switch ( theGpio ) {
			case kGPIO_Selector_AnalogCodecReset:
			case kGPIO_Selector_DigitalCodecReset:
				if ( kGPIO_Reset == attributes ) {
					gpioWrite ( gpioPtr, assertGPIO ( activeState) );
				} else if ( kGPIO_Run == attributes ) {
					gpioWrite ( gpioPtr, negateGPIO ( activeState) );
				} else {
					gpioPtr = NULL;		//	do not write if there is an invalid argument
				}
				break;
			case kGPIO_Selector_ClockMux:
			case kGPIO_Selector_InputDataMux:
				if ( kGPIO_MuxSelectAlternate == attributes ) {
					gpioWrite ( gpioPtr, assertGPIO ( activeState) );
				} else if ( kGPIO_MuxSelectDefault == attributes ) {
					gpioWrite ( gpioPtr, negateGPIO ( activeState) );
				} else {
					gpioPtr = NULL;		//	do not write if there is an invalid argument
				}
				break;
			case kGPIO_Selector_CodecErrorInterrupt:
			case kGPIO_Selector_CodecInterrupt:
			case kGPIO_Selector_ComboInJackType:
			case kGPIO_Selector_ComboOutJackType:
			case kGPIO_Selector_DigitalInDetect:
			case kGPIO_Selector_DigitalOutDetect:
			case kGPIO_Selector_HeadphoneDetect:
			case kGPIO_Selector_InternalSpeakerID:
			case kGPIO_Selector_LineInDetect:
			case kGPIO_Selector_LineOutDetect:
			case kGPIO_Selector_SpeakerDetect:
				gpioPtr = NULL;		//	do not write if there is an invalid argument
				break;
			case kGPIO_Selector_HeadphoneMute:
			case kGPIO_Selector_LineOutMute:
			case kGPIO_Selector_SpeakerMute:
				if ( kGPIO_Muted == attributes ) {
					gpioWrite ( gpioPtr, assertGPIO ( activeState) );
				} else if ( kGPIO_Unmuted == attributes ) {
					gpioWrite ( gpioPtr, negateGPIO ( activeState) );
				} else {
					gpioPtr = NULL;		//	do not write if there is an invalid argument
				}
				break;
			case kGPIO_Selector_ExternalMicDetect:
				break;
			case kGPIO_Selector_NotAssociated:
				break;
		}
		if ( NULL != gpioPtr ) {
			result = kIOReturnSuccess;
		}
	}
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setClockMux(GpioAttributes muxState) {
	return setGpioAttributes ( kGPIO_Selector_ClockMux, muxState );
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getClockMux() {
	return getGpioAttributes ( kGPIO_Selector_ClockMux );
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getCodecErrorInterrupt() {
	return getGpioAttributes ( kGPIO_Selector_CodecErrorInterrupt );
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getCodecInterrupt() {
	return getGpioAttributes ( kGPIO_Selector_CodecInterrupt );
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getDigitalInConnected() {
	return getGpioAttributes ( kGPIO_Selector_DigitalInDetect );
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getComboInJackTypeConnected() {
	return getGpioAttributes ( kGPIO_Selector_ComboInJackType );
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getDigitalOutConnected() {
	return getGpioAttributes ( kGPIO_Selector_DigitalOutDetect );
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getComboOutJackTypeConnected() {
	return getGpioAttributes ( kGPIO_Selector_ComboOutJackType );
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getHeadphoneConnected() {
	return getGpioAttributes ( kGPIO_Selector_HeadphoneDetect );
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setHeadphoneMuteState ( GpioAttributes muteState ) {
	IOReturn		result = kIOReturnSuccess;
	
	if ( mEnableAmplifierMuteRelease || ( kGPIO_Muted == muteState ) ) {		//	[3514762]
		if ( kGPIO_Muted == muteState ) {
			debugIOLog ( 5, "KeyLargoPlatform::setHeadphoneMuteState ( kGPIO_Muted )" );
		} else {
			debugIOLog ( 5, "KeyLargoPlatform::setHeadphoneMuteState ( kGPIO_Unmuted )" );
		}
		result = setGpioAttributes ( kGPIO_Selector_HeadphoneMute, muteState );
	}
	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getHeadphoneMuteState() {
	return getGpioAttributes ( kGPIO_Selector_HeadphoneMute );
}
	
//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setInputDataMux(GpioAttributes muxState) {
	return setGpioAttributes ( kGPIO_Selector_InputDataMux, muxState );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getInputDataMux() {
	return getGpioAttributes ( kGPIO_Selector_InputDataMux );
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getInternalSpeakerID() {
	return getGpioAttributes ( kGPIO_Selector_InternalSpeakerID );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getLineInConnected() {
	return getGpioAttributes ( kGPIO_Selector_LineInDetect );
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getLineOutConnected() {
	return getGpioAttributes ( kGPIO_Selector_LineOutDetect );
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setLineOutMuteState ( GpioAttributes muteState ) {
	IOReturn		result = kIOReturnSuccess;
	
	if ( mEnableAmplifierMuteRelease || ( kGPIO_Muted == muteState ) ) {		//	[3514762]
		if ( kGPIO_Muted == muteState ) {
			debugIOLog ( 5, "KeyLargoPlatform::setLineOutMuteState ( kGPIO_Muted )" );
		} else {
			debugIOLog ( 5, "KeyLargoPlatform::setLineOutMuteState ( kGPIO_Unmuted )" );
		}
		result = setGpioAttributes ( kGPIO_Selector_LineOutMute, muteState );
	}
	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getLineOutMuteState() {
	return getGpioAttributes ( kGPIO_Selector_LineOutMute );
}

//	--------------------------------------------------------------------------------
GpioAttributes	KeyLargoPlatform::getSpeakerConnected() {
	return getGpioAttributes ( kGPIO_Selector_SpeakerDetect );
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getSpeakerMuteState() {
	return getGpioAttributes ( kGPIO_Selector_SpeakerMute );
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setSpeakerMuteState ( GpioAttributes muteState ) {
	IOReturn		result = kIOReturnSuccess;
	
	if ( mEnableAmplifierMuteRelease || ( kGPIO_Muted == muteState ) ) {		//	[3514762]
		if ( kGPIO_Muted == muteState ) {
			debugIOLog ( 5, "KeyLargoPlatform::setSpeakerMuteState ( kGPIO_Muted )" );
		} else {
			debugIOLog ( 5, "KeyLargoPlatform::setSpeakerMuteState ( kGPIO_Unmuted )" );
		}
		result = setGpioAttributes ( kGPIO_Selector_SpeakerMute, muteState );
	}
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setCodecReset ( CODEC_RESET target, GpioAttributes reset ) {
	IOReturn		result = kIOReturnError;
	
	if ( kCODEC_RESET_Analog == target ) {
		if ( NULL != mAnalogResetGpio ) {
			result = setGpioAttributes ( kGPIO_Selector_AnalogCodecReset, reset );
		}
	} else if ( kCODEC_RESET_Digital == target ) {
		if ( NULL != mDigitalResetGpio ) {
			result = setGpioAttributes ( kGPIO_Selector_DigitalCodecReset, reset );
		}
	}
	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes KeyLargoPlatform::getCodecReset ( CODEC_RESET target ) {
	GpioAttributes	reset = kGPIO_Unknown;

	if ( kCODEC_RESET_Analog == target ) {
		if ( NULL != mAnalogResetGpio ) {
			reset = getGpioAttributes ( kGPIO_Selector_AnalogCodecReset );
		}
	} else if ( kCODEC_RESET_Digital == target ) {
		if ( NULL != mDigitalResetGpio ) {
			reset = getGpioAttributes ( kGPIO_Selector_DigitalCodecReset );
		}
	}
	return reset;
}

#pragma mark ---------------------------
#pragma mark ¥ Interrupts
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::registerInterruptHandler (IOService * theDevice, void * interruptHandler, PlatformInterruptSource source ) {
	IOReturn				result;

	result = kIOReturnError;
	switch ( source ) {
		case kCodecInterrupt: 				result = setCodecInterruptHandler (theDevice, interruptHandler);				break;
		case kCodecErrorInterrupt: 			result = setCodecErrorInterruptHandler (theDevice, interruptHandler);			break;
		case kDigitalInDetectInterrupt: 	result = setDigitalInDetectInterruptHandler (theDevice, interruptHandler);		break;
		case kDigitalOutDetectInterrupt: 	result = setDigitalOutDetectInterruptHandler (theDevice, interruptHandler);		break;
		case kHeadphoneDetectInterrupt: 	result = setHeadphoneDetectInterruptHandler (theDevice, interruptHandler);		break;
		case kLineInputDetectInterrupt: 	result = setLineInDetectInterruptHandler (theDevice, interruptHandler);			break;
		case kLineOutputDetectInterrupt: 	result = setLineOutDetectInterruptHandler (theDevice, interruptHandler);		break;
		case kSpeakerDetectInterrupt: 		result = setSpeakerDetectInterruptHandler (theDevice, interruptHandler);		break;
		case kUnknownInterrupt:
		default:							debugIOLog (3,  "Attempt to register unknown interrupt source" );				break;
	}
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::unregisterInterruptHandler (IOService * theDevice, void * interruptHandler, PlatformInterruptSource source ) {
	IOReturn				result;

	result = kIOReturnError;
	switch ( source ) {
		case kCodecInterrupt: 				result = setCodecInterruptHandler (theDevice, NULL);							break;
		case kCodecErrorInterrupt: 			result = setCodecErrorInterruptHandler (theDevice, NULL);						break;
		case kDigitalInDetectInterrupt: 	result = setDigitalInDetectInterruptHandler (theDevice, NULL);					break;
		case kDigitalOutDetectInterrupt: 	result = setDigitalOutDetectInterruptHandler (theDevice, NULL);					break;
		case kHeadphoneDetectInterrupt: 	result = setHeadphoneDetectInterruptHandler (theDevice, NULL);					break;
		case kLineInputDetectInterrupt: 	result = setLineInDetectInterruptHandler (theDevice, NULL);						break;
		case kLineOutputDetectInterrupt: 	result = setLineOutDetectInterruptHandler (theDevice, NULL);					break;
		case kSpeakerDetectInterrupt: 		result = setSpeakerDetectInterruptHandler (theDevice, NULL);					break;
		case kUnknownInterrupt:
		default:							debugIOLog (3,  "Attempt to register unknown interrupt source" );				break;
	}

	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setHeadphoneDetectInterruptHandler (IOService* theDevice, void* interruptHandler) {
	IOReturn result;
	IOInterruptEventSource * theInterruptEventSource;
	
	theInterruptEventSource = NULL;
	result = kIOReturnError;

	FailIf (NULL == mWorkLoop, Exit);

	if ( NULL == interruptHandler && NULL != mHeadphoneDetectIntEventSource) {
		mHeadphoneDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource (mHeadphoneDetectIntEventSource);	
		mHeadphoneDetectIntEventSource = NULL;
	} else {
		FailIf (NULL == mHeadphoneDetectIntProvider, Exit);
		mHeadphoneDetectIntEventSource = theInterruptEventSource = IOInterruptEventSource::interruptEventSource (this,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mHeadphoneDetectIntProvider,
																				0);
		FailIf (NULL == theInterruptEventSource, Exit);
		
		result = mWorkLoop->addEventSource (theInterruptEventSource);
		theInterruptEventSource->enable ();
	}

Exit:
	if (NULL != theInterruptEventSource) {
		theInterruptEventSource->release();
	}
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setSpeakerDetectInterruptHandler (IOService* theDevice, void* interruptHandler) {
	IOReturn result;
	IOInterruptEventSource * theInterruptEventSource;
	
	theInterruptEventSource = NULL;
	result = kIOReturnError;

	if ( NULL == interruptHandler && NULL != mSpeakerDetectIntEventSource) {
		mSpeakerDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource (mSpeakerDetectIntEventSource);	
		mSpeakerDetectIntEventSource = NULL;
	} else {
		FailIf (NULL == mSpeakerDetectIntProvider, Exit);
		mSpeakerDetectIntEventSource = theInterruptEventSource = IOInterruptEventSource::interruptEventSource (this,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mSpeakerDetectIntProvider,
																				0);
		FailIf (NULL == theInterruptEventSource, Exit);
		
		result = mWorkLoop->addEventSource (theInterruptEventSource);	
		theInterruptEventSource->enable ();
	}

Exit:
	if (NULL != theInterruptEventSource) {
		theInterruptEventSource->release();
	}
	debugIOLog (3,  "KeyLargoPlatform::setSpeakerDetectInterruptHandler() returns %X", result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setLineOutDetectInterruptHandler (IOService* theDevice, void* interruptHandler) {
	IOReturn result;
	IOInterruptEventSource * theInterruptEventSource;

	theInterruptEventSource = NULL;
	result = kIOReturnError;

	if ( NULL == interruptHandler && NULL != mLineOutDetectIntEventSource) {
		mLineOutDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource (mLineOutDetectIntEventSource);		mLineOutDetectIntEventSource = NULL;
	} else {
		FailIf (NULL == mLineOutDetectIntProvider, Exit);
		mLineOutDetectIntEventSource = theInterruptEventSource = IOInterruptEventSource::interruptEventSource (this,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mLineOutDetectIntProvider,
																				0);
		FailIf (NULL == theInterruptEventSource, Exit);
		
		result = mWorkLoop->addEventSource (theInterruptEventSource);	
		theInterruptEventSource->enable ();
	}
Exit:
	if (NULL != theInterruptEventSource) {
		theInterruptEventSource->release();
	}
	debugIOLog (3,  "KeyLargoPlatform::setLineOutDetectInterruptHandler() returns %X", result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setLineInDetectInterruptHandler (IOService* theDevice, void* interruptHandler) {
	IOReturn result;
	IOInterruptEventSource * theInterruptEventSource;

	theInterruptEventSource = NULL;
	result = kIOReturnError;

	if ( NULL == interruptHandler && NULL != mLineInDetectIntEventSource) {
		mLineInDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource (mLineInDetectIntEventSource);		mLineInDetectIntEventSource = NULL;
	} else {
		FailIf (NULL == mLineInDetectIntProvider, Exit);
		mLineInDetectIntEventSource = theInterruptEventSource = IOInterruptEventSource::interruptEventSource (this,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mLineInDetectIntProvider,
																				0);
		FailIf (NULL == theInterruptEventSource, Exit);
		
		result = mWorkLoop->addEventSource (theInterruptEventSource);	
		theInterruptEventSource->enable ();
	}
Exit:
	if (NULL != theInterruptEventSource) {
		theInterruptEventSource->release();
	}
	debugIOLog (3,  "KeyLargoPlatform::setLineInDetectInterruptHandler() returns %X", result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setDigitalOutDetectInterruptHandler (IOService* theDevice, void* interruptHandler) {
	IOReturn result;
	IOInterruptEventSource * theInterruptEventSource;

	theInterruptEventSource = NULL;
	result = kIOReturnError;

	if ( NULL == interruptHandler && NULL != mDigitalOutDetectIntEventSource) {
		mDigitalOutDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource (mDigitalOutDetectIntEventSource);		mDigitalOutDetectIntEventSource = NULL;
	} else {
		FailIf (NULL == mDigitalOutDetectIntProvider, Exit);
		mDigitalOutDetectIntEventSource = theInterruptEventSource = IOInterruptEventSource::interruptEventSource (this,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mDigitalOutDetectIntProvider,
																				0);
		FailIf (NULL == theInterruptEventSource, Exit);
		
		result = mWorkLoop->addEventSource (theInterruptEventSource);	
		theInterruptEventSource->enable ();
	}
Exit:
	if (NULL != theInterruptEventSource) {
		theInterruptEventSource->release();
	}
	debugIOLog (3,  "KeyLargoPlatform::setDigitalOutDetectInterruptHandler() returns %X", result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setDigitalInDetectInterruptHandler (IOService* theDevice, void* interruptHandler) {
	IOReturn result;
	IOInterruptEventSource * theInterruptEventSource;

	theInterruptEventSource = NULL;
	result = kIOReturnError;

	if ( NULL == interruptHandler && NULL != mDigitalInDetectIntEventSource) {
		mDigitalInDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource (mDigitalInDetectIntEventSource);		mDigitalInDetectIntEventSource = NULL;
	} else {
		FailIf (NULL == mDigitalInDetectIntProvider, Exit);
		mDigitalInDetectIntEventSource = theInterruptEventSource = IOInterruptEventSource::interruptEventSource (this,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mDigitalInDetectIntProvider,
																				0);
		FailIf (NULL == theInterruptEventSource, Exit);
		
		result = mWorkLoop->addEventSource (theInterruptEventSource);	
		theInterruptEventSource->enable ();
	}
Exit:
	if (NULL != theInterruptEventSource) {
		theInterruptEventSource->release();
	}
	debugIOLog (3,  "KeyLargoPlatform::setDigitalInDetectInterruptHandler() returns %X", result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setCodecInterruptHandler (IOService* theDevice, void* interruptHandler) {
	IOReturn result;
	IOInterruptEventSource * theInterruptEventSource;

	theInterruptEventSource = NULL;
	result = kIOReturnError;

	if ( NULL == interruptHandler && NULL != mCodecInterruptEventSource) {
		mCodecInterruptEventSource->disable ();
		result = mWorkLoop->removeEventSource (mCodecInterruptEventSource);		mCodecInterruptEventSource = NULL;
	} else {
		FailIf (NULL == mCodecIntProvider, Exit);
		mCodecInterruptEventSource = theInterruptEventSource = IOInterruptEventSource::interruptEventSource (this,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mCodecIntProvider,
																				0);
		FailIf (NULL == theInterruptEventSource, Exit);
		
		result = mWorkLoop->addEventSource (theInterruptEventSource);	
		theInterruptEventSource->enable ();
	}
Exit:
	if (NULL != theInterruptEventSource) {
		theInterruptEventSource->release();
	}
	debugIOLog (3,  "KeyLargoPlatform::setCodecInterruptHandler() returns %X", result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn KeyLargoPlatform::setCodecErrorInterruptHandler (IOService* theDevice, void* interruptHandler) {
	IOReturn result;
	IOInterruptEventSource * theInterruptEventSource;

	theInterruptEventSource = NULL;
	result = kIOReturnError;

	if ( NULL == interruptHandler && NULL != mCodecErrorInterruptEventSource) {
		mCodecErrorInterruptEventSource->disable ();
		result = mWorkLoop->removeEventSource (mCodecErrorInterruptEventSource);		mCodecErrorInterruptEventSource = NULL;
	} else {
		FailIf (NULL == mCodecErrorIntProvider, Exit);
		mCodecErrorInterruptEventSource = theInterruptEventSource = IOInterruptEventSource::interruptEventSource (this,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mCodecErrorIntProvider,
																				0);
		FailIf (NULL == theInterruptEventSource, Exit);
		
		result = mWorkLoop->addEventSource (theInterruptEventSource);	
		theInterruptEventSource->enable ();
	}
Exit:
	if (NULL != theInterruptEventSource) {
		theInterruptEventSource->release();
	}
	debugIOLog (3,  "KeyLargoPlatform::setCodecErrorInterruptHandler() returns %X", result );
	return result;
}

//	--------------------------------------------------------------------------------
void KeyLargoPlatform::logFCR1( void ) {
	debugIOLog (3,  "logFCR1 = %X", OSReadLittleInt32 ( mIOConfigurationBaseAddress, kFCR1Offset ) );
	return;
}

//	--------------------------------------------------------------------------------
void KeyLargoPlatform::logFCR3( void ) {
	debugIOLog (3,  "logFCR3 = %X", OSReadLittleInt32 ( mIOConfigurationBaseAddress, kFCR3Offset ) );
	return;
}

#pragma mark ---------------------------
#pragma mark ¥ Private Direct HW Access
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
//	Sets the 'gpioDDR' to OUTPUT and sets the 'gpioDATA' to the 'data' state.
IOReturn KeyLargoPlatform::gpioWrite( UInt8* gpioAddress, UInt8 data ) {
	UInt8		gpioData;
	IOReturn	result = kIOReturnBadArgument;
	
	if( NULL != gpioAddress ) {
		if( 0 == data )
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 0 << gpioDATA );
		else
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 1 << gpioDATA );

		*gpioAddress = gpioData;
		result = kIOReturnSuccess;
	}
	return result;
}

//	--------------------------------------------------------------------------------
void KeyLargoPlatform::setKeyLargoRegister(void *klRegister, UInt32 value) {
	debugIOLog (3,  "Register %p = %lX", klRegister, value );
	OSWriteLittleInt32(klRegister, 0, value);
}

//	--------------------------------------------------------------------------------
UInt32 KeyLargoPlatform::getKeyLargoRegister(void *klRegister) {
    return (OSReadLittleInt32(klRegister, 0));
}

//	--------------------------------------------------------------------------------
UInt32 KeyLargoPlatform::getFCR1( void ) {
	UInt32 result = OSReadLittleInt32 ( mIOConfigurationBaseAddress, kFCR1Offset );		
	debugIOLog (3,  "getFCR1 = %lX", result );
	return result;
}

//	--------------------------------------------------------------------------------
void KeyLargoPlatform::setFCR1(UInt32 value) {
	debugIOLog (3,  "Register %lX = %lX", (UInt32)(mIOConfigurationBaseAddress) + kFCR1Offset, value );
	OSWriteLittleInt32( mIOConfigurationBaseAddress, kFCR1Offset, value );
}

//	--------------------------------------------------------------------------------
UInt32 KeyLargoPlatform::getFCR3( void ) {
	UInt32 result = OSReadLittleInt32 ( mIOConfigurationBaseAddress, kFCR3Offset );
	debugIOLog (3,  "getFCR3 = %lX", result );
	return result;
}

//	--------------------------------------------------------------------------------
void KeyLargoPlatform::setFCR3(UInt32 value) {
	debugIOLog (3,  "Register %lX = %lX", (UInt32)(mIOConfigurationBaseAddress) + kFCR1Offset, value );
	OSWriteLittleInt32( mIOConfigurationBaseAddress, kFCR3Offset, value );
}

#pragma mark ---------------------------
#pragma mark ¥ Private I2C, I2S
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
// Init the I2S register memory maps
IOReturn KeyLargoPlatform::initI2S(IOMemoryMap* map) {
	IOReturn		result = kIOReturnError;
	
	debugIOLog (3, "KeyLargoPlatform::initI2S - map = 0x%X", (unsigned int)map);

    // cache the config space
	mSoundConfigSpace = (UInt8 *)map->getPhysicalAddress();

    // sets the clock base address figuring out which I2S cell we're on
    if ((((UInt32)mSoundConfigSpace ^ kI2S0BaseOffset) & 0x0001FFFF) == 0) 
    {
		//	[3060321]	ioBaseAddress is required by this object in order to enable the target
		//				I2S I/O Module for which this object is to service.  The I2S I/O Module
		//				enable occurs through the configuration registers which reside in the
		//				first block of ioBase.		rbm		2 Oct 2002
		mIOBaseAddress = (void *)((UInt32)mSoundConfigSpace - kI2S0BaseOffset);
		mIOBaseAddressMemory = IODeviceMemory::withRange ((IOPhysicalAddress)((UInt8 *)mSoundConfigSpace - kI2S0BaseOffset), 256);
        mI2SInterfaceNumber = kUseI2SCell0;
    }
    else if ((((UInt32)mSoundConfigSpace ^ kI2S1BaseOffset) & 0x0001FFFF) == 0) 
    {
		//	[3060321]	ioBaseAddress is required by this object in order to enable the target
		//				I2S I/O Module for which this object is to service.  The I2S I/O Module
		//				enable occurs through the configuration registers which reside in the
		//				first block of ioBase.		rbm		2 Oct 2002
		mIOBaseAddress = (void *)((UInt32)mSoundConfigSpace - kI2S1BaseOffset);
		mIOBaseAddressMemory = IODeviceMemory::withRange ((IOPhysicalAddress)((UInt8 *)mSoundConfigSpace - kI2S1BaseOffset), 256);
        mI2SInterfaceNumber = kUseI2SCell1;
    }
    else 
    {
        debugIOLog (3, "AudioI2SControl::init ERROR: unable to setup ioBaseAddress and i2SInterfaceNumber");
    }
	debugIOLog (3,  "mIOBaseAddress %p, mIOBaseAddressMemory %p, mI2SInterfaceNumber %d", mIOBaseAddress, mIOBaseAddressMemory, mI2SInterfaceNumber );
	FailIf (NULL == mIOBaseAddressMemory, Exit);

	//	[3060321]	ioConfigurationBaseAddress is required by this object in order to enable the target
	//				I2S I/O Module for which this object is to service.  The I2S I/O Module
	//				enable occurs through the configuration registers which reside in the
	//				first block of ioBase.		rbm		2 Oct 2002
	
	mIOConfigurationBaseAddress = (void *)mIOBaseAddressMemory->map()->getVirtualAddress();
	FailIf ( NULL == mIOConfigurationBaseAddress, Exit );

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
	
	//	Map the I2S configuration registers
	mIOI2SBaseAddressMemory = IODeviceMemory::withRange ((IOPhysicalAddress)((UInt8 *)mSoundConfigSpace), kI2S_IO_CONFIGURATION_SIZE);
	FailIf ( NULL == mIOI2SBaseAddressMemory, Exit );
	mI2SBaseAddress = (void *)mIOI2SBaseAddressMemory->map()->getVirtualAddress();
	FailIf (NULL == mI2SBaseAddress, Exit);
	
	debugIOLog (3, "KeyLargoPlatform::initI2S - mI2SInterfaceNumber = 0x%X", mI2SInterfaceNumber);
	debugIOLog (3, "KeyLargoPlatform::initI2S - mIOI2SBaseAddressMemory = %p", mIOI2SBaseAddressMemory);
	debugIOLog (3, "KeyLargoPlatform::initI2S - mI2SBaseAddress = %p", mI2SBaseAddress);
	debugIOLog (3, "KeyLargoPlatform::initI2S - mIOConfigurationBaseAddress = %p", mIOConfigurationBaseAddress);

	//	Enable the I2S interface by setting the enable bit in the feature 
	//	control register.  This one action requires knowledge of the address 
	//	of I/O configuration address space.		[3060321]	rbm		2 Oct 2002
	if (kUseI2SCell0 == mI2SInterfaceNumber) {
		setKeyLargoRegister ( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset, getKeyLargoRegister ( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset ) | kI2S0InterfaceEnable );
	} else {
		setKeyLargoRegister ( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset, getKeyLargoRegister ( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset ) | kI2S1InterfaceEnable );
	}

	result = kIOReturnSuccess;

Exit:
	return result;
}

//	--------------------------------------------------------------------------------
bool KeyLargoPlatform::findAndAttachI2C() {
	const OSSymbol	*i2cDriverName;
	IOService		*i2cCandidate;
	OSDictionary	*i2cServiceDictionary;
	
	// Searches the i2c:
	i2cDriverName = OSSymbol::withCStringNoCopy("PPCI2CInterface.i2c-mac-io");
	i2cServiceDictionary = IOService::resourceMatching(i2cDriverName);
	i2cCandidate = IOService::waitForService(i2cServiceDictionary);
	//i2cDriverName->release();
	//i2cServiceDictionary->release();
	
	mI2CInterface = (PPCI2CInterface*)i2cCandidate->getProperty(i2cDriverName);

	if (NULL == mI2CInterface) {
		debugIOLog (3, "KeyLargoPlatform::findAndAttachI2C can't find the i2c in the registry");
		return false;
	}

	// Make sure that we hold the interface:
	mI2CInterface->retain();

	return true;
}

//	--------------------------------------------------------------------------------
bool KeyLargoPlatform::openI2C() {
	bool		result;
	
	result = false;
	FailIf (NULL == mI2CInterface, Exit);

	// Open the interface and sets it in the wanted mode:
	FailIf (!mI2CInterface->openI2CBus(mI2CPort), Exit);

	result = true;

Exit:
	return result;
}

//	--------------------------------------------------------------------------------
void KeyLargoPlatform::closeI2C () {
	// Closes the bus so other can access to it:
	mI2CInterface->closeI2CBus ();
}

//	--------------------------------------------------------------------------------
bool KeyLargoPlatform::detachFromI2C() {
	if (mI2CInterface) {
		mI2CInterface->release();
		mI2CInterface = NULL;
	}
	return (true);
}

#pragma mark ---------------------------
#pragma mark ¥ DBDMA Memory Address Acquisition Methods
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	KeyLargoPlatform::GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) {
	IOMemoryMap *				map;
	IOService *					parentOfParent;
	IODBDMAChannelRegisters *	ioBaseDMAInput = NULL;
	
	FailIf ( NULL == dbdmaProvider, Exit );
	debugIOLog (3,  "KeyLargoPlatform::GetInputChannelRegistersVirtualAddress i2s-a name is %s", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	debugIOLog (3,  "   parentOfParent name is %s", parentOfParent->getName() );
	map = parentOfParent->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAInputIndex );
	FailIf ( NULL == map, Exit );
	ioBaseDMAInput = (IODBDMAChannelRegisters *) map->getVirtualAddress();
	debugIOLog (3,  "ioBaseDMAInput %p is at physical %p", ioBaseDMAInput, (void*)map->getPhysicalAddress() );
	if ( NULL == ioBaseDMAInput ) { debugIOLog (1,  "KeyLargoPlatform::GetInputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE" ); }
Exit:
	return ioBaseDMAInput;
}

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	KeyLargoPlatform::GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) {
	IOMemoryMap *				map;
	IOService *					parentOfParent;
	
	FailIf ( NULL == dbdmaProvider, Exit );
	debugIOLog (3,  "KeyLargoPlatform::GetOutputChannelRegistersVirtualAddress i2s-a name is %s", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	debugIOLog (3,  "   parentOfParent name is %s", parentOfParent->getName() );
	map = parentOfParent->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAOutputIndex );
	FailIf ( NULL == map, Exit );
	mIOBaseDMAOutput = (IODBDMAChannelRegisters *) map->getVirtualAddress();
	debugIOLog (3,  "mIOBaseDMAOutput %p is at physical %p", mIOBaseDMAOutput, (void*)map->getPhysicalAddress() );
	if ( NULL == mIOBaseDMAOutput ) { debugIOLog (1,  "KeyLargoPlatform::GetOutputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE" ); }
Exit:
	return mIOBaseDMAOutput;
}

#pragma mark ---------------------------
#pragma mark UTILITIES
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IORegistryEntry *KeyLargoPlatform::FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value) {
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


#pragma mark ---------------------------
#pragma mark ¥ USER CLIENT SUPPORT
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IOReturn	KeyLargoPlatform::getPlatformState ( PlatformStateStructPtr outState ) {
	IOReturn			result = kIOReturnBadArgument;
	
	debugIOLog (3,  "+ UC KeyLargoPlatform::getPlatformState ( %p )", outState );
	FailIf ( NULL == outState, Exit );
	outState->platformType = kPlatformInterfaceType_KeyLargo;
	
	outState->i2s.intCtrl = getI2SIOMIntControl ();
	outState->i2s.serialFmt = getSerialFormatRegister ();
	outState->i2s.codecMsgOut = 0;
	outState->i2s.codecMsgIn = 0;
	outState->i2s.frameCount = getFrameCount ();
	outState->i2s.frameCountToMatch = 0;
	outState->i2s.dataWordSizes = getDataWordSizes ();
	outState->i2s.peakLevelSfSel = 0;
	outState->i2s.peakLevelIn0 = getPeakLevel ( kStreamFrontLeft );
	outState->i2s.peakLevelIn1 = getPeakLevel ( kStreamFrontRight );
	outState->i2s.newPeakLevelIn0 = 0;			/*	always 0 - triggers update on setPlatformState!		*/
	outState->i2s.newPeakLevelIn1 = 0;			/*	always 0 - triggers update on setPlatformState!		*/
	
	outState->fcr.i2sEnable = getI2SEnable ();
	outState->fcr.i2sClockEnable = getI2SClockEnable ();
	outState->fcr.i2sReset = 0;
	outState->fcr.i2sCellEnable = getI2SCellEnable ();
	outState->fcr.clock18mHzEnable = 1;
	outState->fcr.clock45mHzEnable = 1;
	outState->fcr.clock49mHzEnable = 1;
	outState->fcr.pll45mHzShutdown = 0;
	outState->fcr.pll49mHzShutdown = 0;
	
	outState->gpio.gpio_AnalogCodecReset = getCodecReset ( kCODEC_RESET_Analog );
	outState->gpio.gpio_ClockMux = getClockMux ();
	outState->gpio.gpio_CodecInterrupt = getCodecInterrupt ();
	outState->gpio.gpio_CodecErrorInterrupt = getCodecErrorInterrupt ();
	outState->gpio.gpio_ComboInJackType = getComboInJackTypeConnected ();
	outState->gpio.gpio_ComboOutJackType = getComboOutJackTypeConnected ();
	outState->gpio.gpio_DigitalCodecReset = getCodecReset ( kCODEC_RESET_Digital );
	outState->gpio.gpio_DigitalInDetect = getDigitalInConnected ();
	outState->gpio.gpio_DigitalOutDetect = getDigitalOutConnected ();
	outState->gpio.gpio_HeadphoneDetect = getHeadphoneConnected ();
	outState->gpio.gpio_HeadphoneMute = getHeadphoneMuteState ();
	outState->gpio.gpio_InputDataMux = getInputDataMux ();
	outState->gpio.gpio_InternalSpeakerID = getInternalSpeakerID ();
	outState->gpio.gpio_LineInDetect = getLineInConnected ();
	outState->gpio.gpio_LineOutDetect = getLineOutConnected ();
	outState->gpio.gpio_LineOutMute = getLineOutMuteState ();
	outState->gpio.gpio_SpeakerDetect = getSpeakerConnected ();
	outState->gpio.gpio_SpeakerMute = getSpeakerMuteState ();
	outState->gpio.gpio_ComboInAssociation = getComboInAssociation ();
	outState->gpio.gpio_ComboOutAssociation = getComboOutAssociation ();
	outState->gpio.reserved_20 = kGPIO_Unknown;
	outState->gpio.reserved_21 = kGPIO_Unknown;
	outState->gpio.reserved_22 = kGPIO_Unknown;
	outState->gpio.reserved_23 = kGPIO_Unknown;
	outState->gpio.reserved_24 = kGPIO_Unknown;
	outState->gpio.reserved_25 = kGPIO_Unknown;
	outState->gpio.reserved_26 = kGPIO_Unknown;
	outState->gpio.reserved_27 = kGPIO_Unknown;
	outState->gpio.reserved_28 = kGPIO_Unknown;
	outState->gpio.reserved_29 = kGPIO_Unknown;
	outState->gpio.reserved_30 = kGPIO_Unknown;
	outState->gpio.reserved_31 = kGPIO_Unknown;

	outState->i2c.i2c_pollingMode = (UInt32)false;
	outState->i2c.i2c_errorStatus = mI2C_lastTransactionResult;

	result = kIOReturnSuccess;
Exit:
	debugIOLog (3,  "- UC KeyLargoPlatform::getPlatformState ( %p ) returns %X", outState, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	KeyLargoPlatform::setPlatformState ( PlatformStateStructPtr inState ) {
	IOReturn			result = kIOReturnBadArgument;
	
	debugIOLog (3,  "+ UC KeyLargoPlatform::setPlatformState ( %p )", inState );
	debugIOLog (3,  "+ UC KeyLargoPlatform::setPlatformState ( %p )", inState );
	FailIf ( NULL == inState, Exit );
	if ( inState->i2s.intCtrl != getI2SIOMIntControl () ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setI2SIOMIntControl ( %lX )", inState->i2s.intCtrl );
		result = setI2SIOMIntControl ( inState->i2s.intCtrl );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->i2s.serialFmt != getSerialFormatRegister () ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setSerialFormatRegister ( %lX )", inState->i2s.serialFmt );
		result = setSerialFormatRegister ( inState->i2s.serialFmt );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( inState->i2s.frameCount != getFrameCount () ) && ( 0 == inState->i2s.frameCount ) ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setFrameCount ( %lX )", inState->i2s.frameCount );
		result = setFrameCount ( inState->i2s.frameCount );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->i2s.dataWordSizes != getDataWordSizes () ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setDataWordSizes ( %lX )", inState->i2s.dataWordSizes );
		result = setDataWordSizes ( inState->i2s.dataWordSizes );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( 0 != inState->i2s.newPeakLevelIn0 ) {
		debugIOLog (3,  "KeyLargoPlatform::setPeakLevel ( kStreamFrontLeft, %lX )", inState->i2s.peakLevelIn0 );
		setPeakLevel ( kStreamFrontLeft, inState->i2s.peakLevelIn0 );
	}
	if ( 0 != inState->i2s.newPeakLevelIn1 ) {
		debugIOLog (3,  "KeyLargoPlatform::setPeakLevel ( kStreamFrontRight, %lX )", inState->i2s.peakLevelIn1 );
		setPeakLevel ( kStreamFrontRight, inState->i2s.peakLevelIn1 );
	}
	if ( inState->fcr.i2sEnable != getI2SEnable () ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setI2SEnable ( %lX )", inState->fcr.i2sEnable );
		result = setI2SEnable ( inState->fcr.i2sEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->fcr.i2sClockEnable != getI2SClockEnable () ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setI2SClockEnable ( %lX )", inState->fcr.i2sClockEnable );
		result = setI2SClockEnable ( inState->fcr.i2sClockEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->fcr.i2sCellEnable != getI2SCellEnable () ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setI2SCellEnable ( %lX )", inState->fcr.i2sCellEnable );
		result = setI2SCellEnable ( inState->fcr.i2sCellEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( kGPIO_Unknown != inState->gpio.gpio_AnalogCodecReset ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setCodecReset ( kCODEC_RESET_Analog, %X )", inState->gpio.gpio_AnalogCodecReset );
		result = setCodecReset ( kCODEC_RESET_Analog, inState->gpio.gpio_AnalogCodecReset );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( kGPIO_Unknown != inState->gpio.gpio_ClockMux ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setClockMux ( %X )", inState->gpio.gpio_ClockMux );
		result = setClockMux ( inState->gpio.gpio_ClockMux );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( kGPIO_Unknown != inState->gpio.gpio_DigitalCodecReset ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setCodecReset ( kCODEC_RESET_Digital, %X )", inState->gpio.gpio_DigitalCodecReset );
		result = setCodecReset ( kCODEC_RESET_Digital, inState->gpio.gpio_DigitalCodecReset );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( kGPIO_Unknown != inState->gpio.gpio_HeadphoneMute ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setHeadphoneMuteState ( %X )", inState->gpio.gpio_HeadphoneMute );
		result = setHeadphoneMuteState ( inState->gpio.gpio_HeadphoneMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( kGPIO_Unknown != inState->gpio.gpio_InputDataMux ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setInputDataMux ( %X )", inState->gpio.gpio_InputDataMux );
		result = setInputDataMux ( inState->gpio.gpio_InputDataMux );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( kGPIO_Unknown != inState->gpio.gpio_LineOutMute ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setLineOutMuteState ( %X )", inState->gpio.gpio_LineOutMute );
		result = setLineOutMuteState ( inState->gpio.gpio_LineOutMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( kGPIO_Unknown != inState->gpio.gpio_SpeakerMute ) {
		debugIOLog (3,  "KeyLargoPlatform::setPlatformState setSpeakerMuteState ( %X )", inState->gpio.gpio_SpeakerMute );
		result = setSpeakerMuteState ( inState->gpio.gpio_SpeakerMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	debugIOLog (3,  "- UC KeyLargoPlatform::setPlatformState ( %p ) returns %X", inState, result );
	debugIOLog (3,  "- UC KeyLargoPlatform::setPlatformState ( %p ) returns %X", inState, result );
	return result;
}







