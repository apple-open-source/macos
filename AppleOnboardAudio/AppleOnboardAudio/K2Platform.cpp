/*
 *  K2Platform.cpp
 *  
 *  Created by Aram Lindahl on Mon Mar 10 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include "K2Platform.h"

#include "PlatformInterface.h"
#include "AudioHardwareConstants.h"
#include "AudioHardwareUtilities.h"
#include "AudioHardwareCommon.h"
#include "AppleOnboardAudio.h"
#include "IOKit/audio/IOAudioDevice.h"

#define super PlatformInterface

#pragma mark ---------------------------
#pragma mark Platform Interface Methods
#pragma mark ---------------------------

OSDefineMetaClassAndStructors(K2Platform, PlatformInterface)

const char * 	K2Platform::kAppleK2pHandle								= "phK2";
const char * 	K2Platform::kAppleI2S0pHandle							= "phI0";
const char * 	K2Platform::kAppleGPIOpHandle							= "phGn";

//	---------------------------
//	FCR1
//	---------------------------

const char * 	K2Platform::kAppleI2S_Enable 							= "platform-enable";					//	
const char * 	K2Platform::kAppleI2S_Disable							= "platform-disable";					//	
const char * 	K2Platform::kAppleI2S_ClockEnable						= "platform-clock-enable";				//	
const char * 	K2Platform::kAppleI2S_ClockDisable						= "platform-clock-disable";				//	
const char * 	K2Platform::kAppleI2S_Reset								= "platform-sw-reset";					//	
const char * 	K2Platform::kAppleI2S_Run								= "platform-clear-sw-reset";			//	
const char * 	K2Platform::kAppleI2S_CellEnable						= "platform-cell-enable";				//	
const char * 	K2Platform::kAppleI2S_CellDisable						= "platform-cell-disable";				//	
const char * 	K2Platform::kAppleI2S_GetEnable							= "platform-get-enable";				//	
const char * 	K2Platform::kAppleI2S_GetClockEnable					= "platform-get-clock-enable";			//	
const char * 	K2Platform::kAppleI2S_GetReset							= "platform-get-sw-reset";				//	
const char * 	K2Platform::kAppleI2S_GetCellEnable						= "platform-get-cell-enable";			//	

//	---------------------------
//	GPIO
//	---------------------------

const char * 	K2Platform::kAppleGPIO_SetAmpMute						= "platform-amp-mute";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_GetAmpMute						= "platform-rd-amp-mute";				//	

const char * 	K2Platform::kAppleGPIO_SetAudioHwReset					= "platform-hw-reset";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_GetAudioHwReset					= "platform-rd-hw-reset";				//		

const char * 	K2Platform::kAppleGPIO_SetCodecClockMux					= "platform-codec-clock-mux";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_GetCodecClockMux					= "platform-rd-clock-mux";				//	

const char * 	K2Platform::kAppleGPIO_EnableCodecErrorIRQ				= "enable-codec-error-irq";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_DisableCodecErrorIRQ				= "disable-codec-error-irq";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_GetCodecErrorIRQ					= "platform-codec-error-irq";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_RegisterCodecErrorIRQ			= "register-codec-error-irq";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_UnregisterCodecErrorIRQ			= "unregister-codec-error-irq";			//	[IN Q37 IOService:...:IOResources]

const char * 	K2Platform::kAppleGPIO_DisableCodecIRQ					= "disable-codec-irq";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_EnableCodecIRQ					= "enable-codec-irq";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_GetCodecIRQ						= "platform-codec-irq";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_RegisterCodecIRQ					= "register-codec-irq";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_UnregisterCodecIRQ				= "unregister-codec-irq";				//	[IN Q37 IOService:...:IOResources]

const char * 	K2Platform::kAppleGPIO_SetCodecInputDataMux				= "platform-codec-input-data-mu";		//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_GetCodecInputDataMux				= "get-codec-input-data-mux";			//	

const char * 	K2Platform::kAppleGPIO_GetComboInJackType				= "platform-combo-in-sense";		

const char * 	K2Platform::kAppleGPIO_GetComboOutJackType				= "platform-combo-out-sense";

const char * 	K2Platform::kAppleGPIO_SetAudioDigHwReset				= "platform-dig-hw-reset";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_GetAudioDigHwReset				= "get-dig-hw-reset";					//	

const char * 	K2Platform::kAppleGPIO_DisableDigitalInDetect			= "disable-audio-dig-in-det";			//	
const char * 	K2Platform::kAppleGPIO_EnableDigitalInDetect			= "enable-audio-dig-in-det";			//	
const char * 	K2Platform::kAppleGPIO_GetDigitalInDetect				= "platform-audio-dig-in-det";		
const char * 	K2Platform::kAppleGPIO_RegisterDigitalInDetect			= "register-audio-dig-in-det";			//	
const char * 	K2Platform::kAppleGPIO_UnregisterDigitalInDetect		= "unregister-audio-dig-in-det";		//	

const char * 	K2Platform::kAppleGPIO_DisableDigitalOutDetect			= "disable-audio-dig-out-detect";		//	
const char * 	K2Platform::kAppleGPIO_EnableDigitalOutDetect			= "enable-audio-dig-out-detect";		//	
const char * 	K2Platform::kAppleGPIO_GetDigitalOutDetect				= "platform-audio-dig-out-det";	
const char * 	K2Platform::kAppleGPIO_RegisterDigitalOutDetect			= "register-audio-dig-out-detect";		//	
const char * 	K2Platform::kAppleGPIO_UnregisterDigitalOutDetect		= "unregister-audio-dig-out-detect";	//	
	
const char * 	K2Platform::kAppleGPIO_DisableHeadphoneDetect			= "disable-headphone-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_EnableHeadphoneDetect			= "enable-headphone-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_GetHeadphoneDetect				= "platform-headphone-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_RegisterHeadphoneDetect			= "register-headphone-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_UnregisterHeadphoneDetect		= "unregister-headphone-detect";		//	[IN Q37 IOService:...:IOResources]

const char * 	K2Platform::kAppleGPIO_SetHeadphoneMute					= "platform-headphone-mute";			//	
const char * 	K2Platform::kAppleGPIO_GetHeadphoneMute					= "platform-rd-headphone-mute";			//	

const char *	K2Platform::kAppleGPIO_GetInternalSpeakerID				= "internal-speaker-id";

const char * 	K2Platform::kAppleGPIO_DisableLineInDetect				= "disable-linein-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_EnableLineInDetect				= "enable-linein-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_GetLineInDetect					= "platform-linein-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_RegisterLineInDetect				= "register-linein-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_UnregisterLineInDetect			= "unregister-linein-detect";			//	[IN Q37 IOService:...:IOResources]

const char *	K2Platform::kAppleGPIO_DisableLineOutDetect				= "disable-lineout-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_EnableLineOutDetect				= "enable-lineout-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_GetLineOutDetect					= "platform-lineout-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_RegisterLineOutDetect			= "register-lineout-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_UnregisterLineOutDetect			= "unregister-lineout-detect";			//	[IN Q37 IOService:...:IOResources]

const char * 	K2Platform::kAppleGPIO_SetLineOutMute					= "platform-lineout-mute";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2Platform::kAppleGPIO_GetLineOutMute					= "platform-rd-lineout-mute";			//	

const char * 	K2Platform::kAppleGPIO_DisableSpeakerDetect				= "disable-audio-spkr-detect";			//	
const char * 	K2Platform::kAppleGPIO_EnableSpeakerDetect				= "enable-audio-spkr-detect";			//	
const char * 	K2Platform::kAppleGPIO_GetSpeakerDetect					= "platform-audio-spkr-detect";			//
const char * 	K2Platform::kAppleGPIO_RegisterSpeakerDetect			= "register-audio-spkr-detect";			//	
const char * 	K2Platform::kAppleGPIO_UnregisterSpeakerDetect			= "unregister-audio-spkr-detect";		//	

//	--------------------------------------------------------------------------------
bool K2Platform::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex) {
	IORegistryEntry			*sound;
	bool					result;
	OSData					*osdata;
	IORegistryEntry			*i2S;
	IORegistryEntry			*macIO;
	
	debugIOLog (3,  "+ K2Platform[%p]::init", this );
	result = super::init (device, provider, inDBDMADeviceIndex);
	FailIf ( !result, Exit );

	debugIOLog (3,  "    about to waitForService on mK2Service %p", mK2Service );
	mK2Service = IOService::waitForService ( IOService::serviceMatching ( "AppleK2" ) );
	debugIOLog (3,  "    mK2Service %p", mK2Service );
	
	sound = device;

	FailWithAction (!sound, result = false, Exit);
	debugIOLog (3, "K2 - sound's name is %s", sound->getName ());

	mI2S = sound->getParentEntry (gIODTPlane);
	FailWithAction (!mI2S, result = false, Exit);
	debugIOLog (3, "K2Platform - i2s's name is %s", mI2S->getName ());

	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "AAPL,phandle" ) );
	mI2SPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debugIOLog (3,  "mI2SPHandle 0x%lX", mI2SPHandle );
	
	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "reg" ) );
	mI2SOffset = *((UInt32*)osdata->getBytesNoCopy());
		
	i2S = mI2S->getParentEntry (gIODTPlane);
	FailWithAction (!i2S, result = false, Exit);
	debugIOLog (3, "mI2S - parent name is %s", i2S->getName ());

	macIO = i2S->getParentEntry (gIODTPlane);
	FailWithAction (!macIO, result = false, Exit);
	debugIOLog (3, "i2S - parent name is %s", macIO->getName ());
	
	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "AAPL,phandle" ) );
	mMacIOPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debugIOLog (3,  "mMacIOPHandle %lX", mMacIOPHandle );

	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "reg" ) );
	mMacIOOffset = *((UInt32*)osdata->getBytesNoCopy());

	mAppleGPIO_AnalogCodecReset = kGPIO_Unknown;
	mAppleGPIO_DigitalCodecReset = kGPIO_Unknown;
	mAppleGPIO_HeadphoneMute = kGPIO_Unknown;
	mAppleGPIO_AmpMute = kGPIO_Unknown;
	mAppleGPIO_LineOutMute = kGPIO_Unknown;
	mAppleGPIO_CodecClockMux = kGPIO_Unknown;
	mAppleGPIO_CodecInputDataMux = kGPIO_Unknown;

	//	Initialize muxes to default settings
	setInputDataMux ( kGPIO_MuxSelectDefault );
	setClockMux ( kGPIO_MuxSelectDefault );

	debugIOLog (3,  "about to findAndAttachI2C" );
	result = findAndAttachI2C();
	if ( !result ) { debugIOLog (3,  "K2Platform::init COULD NOT FIND I2C" ); }
	FailIf ( !result, Exit );

	debugIOLog (3,  "about to findAndAttachI2S" );
	result = findAndAttachI2S();
	if ( !result ) { debugIOLog (3,  "K2Platform::init COULD NOT FIND I2S" ); }
	FailIf ( !result, Exit );

Exit:

	debugIOLog (3,  "- K2Platform[%p (%ld)]::init returns %d", this, mInstanceIndex, result );
	return result;
}

//	--------------------------------------------------------------------------------
void K2Platform::free()
{
	debugIOLog (3, "+ K2Platform::free()");

	detachFromI2C();
	//detachFromI2S();

	super::free();

	debugIOLog (3, "- K2Platform::free()");
}

//	--------------------------------------------------------------------------------
//	Leave the interrupts running on K2 because it appears that interrupts cannot be
//	re-enabled after being disabled.
IOReturn K2Platform::performPlatformSleep ( void ) {
	return kIOReturnSuccess;
}


//	--------------------------------------------------------------------------------
//	Since K2 is not unregistering interrupts on sleep, re-registration of the interrupts
//	upon wake is avoided.  Registration would have invoked each of the interrupt handlers
//	so that task is performed here.
IOReturn K2Platform::performPlatformWake ( IOService * device ) {
	FailIf (NULL == device, Exit );

	if ( kGPIO_Unknown != getHeadphoneConnected() ) {
		headphoneDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	
	if ( kGPIO_Unknown != getSpeakerConnected() ) {
		speakerDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	
	if ( kGPIO_Unknown != getLineInConnected() ) {
		lineInDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	
	if ( kGPIO_Unknown != getLineOutConnected() ) {
		lineOutDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	
	if ( kGPIO_Unknown != getDigitalInConnected() && kGPIO_Unknown == getComboInJackTypeConnected() ) {
		digitalInDetectInterruptHandler ( device, NULL, 0, 0 );
	}
	
	if ( kGPIO_Unknown != getDigitalOutConnected() && kGPIO_Unknown == getComboOutJackTypeConnected() ) {
		digitalOutDetectInterruptHandler ( device, NULL, 0, 0 );
	}

	if ( kGPIO_Unknown != getCodecInterrupt() ) {
		codecInterruptHandler ( device, NULL, 0, 0 );
	}
	
	if ( kGPIO_Unknown != getCodecErrorInterrupt() ) {
		codecErrorInterruptHandler ( device, NULL, 0, 0 );
	}
Exit:
	return kIOReturnSuccess;
}


#pragma mark ---------------------------
#pragma mark Codec Methods	
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
bool K2Platform::writeCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode) {
	bool success = false;

	debugIOLog (7,  "+ K2Platform::writeCodecRegister ( %X, %X, %p, %d, %d )", address, subAddress, data, len, mode );

	FailIf ( NULL == data, Exit );
	if (openI2C()) {
		switch (mode) {
			case kI2C_StandardMode:			mI2CInterface->setStandardMode();			break;
			case kI2C_StandardSubMode:		mI2CInterface->setStandardSubMode();		break;
			case kI2C_CombinedMode:			mI2CInterface->setCombinedMode();			break;
			default:
				debugIOLog (7, "K2Platform::writeCodecRegister() unknown bus mode!");
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
		debugIOLog (7,  " mI2CInterface->writeI2CBus ( %X, %X, %p, %d ), data->%X", (unsigned int)address, (unsigned int)subAddress, data, (unsigned int)len, *data );

		success = mI2CInterface->writeI2CBus (address >> 1, subAddress, data, len);
		mI2C_lastTransactionResult = success;
		
		if (!success) { 
			debugIOLog (7,  "K2Platform::writeCodecRegister( %X, %X, %p %d), mI2CInterface->writeI2CBus returned false.", address, subAddress, data, len );
		}
		FailIf ( !success, Exit );
Exit:
		closeI2C();
	} else {
		debugIOLog (7, "K2Platform::writeCodecRegister() couldn't open the I2C bus!");
	}

	debugIOLog (7,  "- K2Platform::writeCodecRegister ( %X, %X, %p, %d, %d ) returns %d", address, subAddress, data, len, mode, success );

	return success;
}

//	--------------------------------------------------------------------------------
bool K2Platform::readCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode) {
	bool success = false;

	debugIOLog (7,  "+ K2Platform::readCodecRegister ( %X, %X, %p, %d, %d )", address, subAddress, data, len, mode );

	if (openI2C()) {
		switch (mode) {
			case kI2C_StandardMode:			mI2CInterface->setStandardMode();			break;
			case kI2C_StandardSubMode:		mI2CInterface->setStandardSubMode();		break;
			case kI2C_CombinedMode:			mI2CInterface->setCombinedMode();			break;
			default:
				debugIOLog (7, "K2Platform::readCodecRegister() unknown bus mode!");
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

		if (!success) debugIOLog (7, "K2Platform::readCodecRegister(), mI2CInterface->writeI2CBus returned false.");
Exit:
		closeI2C();
	} else {
		debugIOLog (7, "K2Platform::readCodecRegister() couldn't open the I2C bus!");
	}
	debugIOLog (7,  "- K2Platform::readCodecRegister ( %X, %X, %p, %d, %d ) returns %d", address, subAddress, data, len, mode, success );

	return success;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setCodecReset ( CODEC_RESET target, GpioAttributes reset ) {
	IOReturn				result;

	debugIOLog (7,  "K2Platform::setCodecReset ( %d, %d )", (unsigned int)target, (unsigned int)reset );

	switch ( target ) {
		case kCODEC_RESET_Analog:	
			result = writeGpioState ( kGPIO_Selector_AnalogCodecReset, reset );		
			debugIOLog (5,  "setCodecReset() for kGPIO_Selector_AnalogCodecReset returns %X after reset", getCodecReset(target) );
			break;
		case kCODEC_RESET_Digital:	
			result = writeGpioState ( kGPIO_Selector_DigitalCodecReset, reset );	
			debugIOLog (5,  "setCodecReset() for kGPIO_Selector_DigitalCodecReset returns %X after reset", getCodecReset(target) );
			break;
		default:					result = kIOReturnBadArgument;											break;
	}
	if ( kIOReturnSuccess != result ) {
		debugIOLog (7,  "- K2Platform::setCodecReset ( %d, %d ) returns %X", (unsigned int)target, (unsigned int)reset, (unsigned int)result );
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes K2Platform::getCodecReset ( CODEC_RESET target ) {
	GpioAttributes		result;
	
	switch ( target ) {
		case kCODEC_RESET_Analog:		result = readGpioState ( kGPIO_Selector_AnalogCodecReset );			break;
		case kCODEC_RESET_Digital:		result = readGpioState ( kGPIO_Selector_DigitalCodecReset );		break;
		default:						result = kGPIO_Unknown;												break;
	}
	return result;
}

#pragma mark ---------------------------
#pragma mark I2S
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setI2SEnable (bool enable) {
	IOReturn				result;
	const OSSymbol*			funcSymbolName = NULL;

	debugIOLog (7,  "+ K2Platform::setI2SEnable enable=%d", enable );

	result = kIOReturnError;
	if ( mK2Service ) {
		if ( enable ) {
			debugIOLog (7,  "K2Platform::setI2SEnable to 1" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_Enable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)(UInt32)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		} else {
			debugIOLog (7,  "K2Platform::setI2SEnable to 0" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_Disable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)(UInt32)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( kIOReturnSuccess == result ) { mAppleI2S_Enable = enable; }
	}
Exit:
	debugIOLog (7,  "- K2Platform::setI2SEnable enable=%d returns %d", enable, result );
	return result;
}

//	--------------------------------------------------------------------------------
bool K2Platform::getI2SEnable () {
	const OSSymbol*			funcSymbolName = NULL;
	IOReturn				err;
	UInt32					value = 0;
	bool					result = mAppleI2S_Enable;

	if ( mK2Service ) {
		funcSymbolName = makeFunctionSymbolName( kAppleI2S_GetEnable, mI2SPHandle );
		FailIf ( NULL == funcSymbolName, Exit );
		err = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)&value, (void *)(UInt32)0, 0, 0 );
		funcSymbolName->release ();	// [3324205]
		FailIf ( kIOReturnSuccess != err, Exit );
		result = ( 0 != value );
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setI2SClockEnable (bool enable) {
	IOReturn				result;
	const OSSymbol*			funcSymbolName = NULL;

	debugIOLog (7,  "+ K2Platform::setI2SClockEnable enable=%d", enable );

	result = kIOReturnError;

	if ( mK2Service ) {
		if ( enable ) {
			debugIOLog (7,  "K2Platform::setI2SClockEnable to 1" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_ClockEnable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		} else {
			debugIOLog (7,  "K2Platform::setI2SClockEnable to 0" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_ClockDisable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( kIOReturnSuccess == result ) { mAppleI2S_ClockEnable = enable; }
	}
Exit:
	debugIOLog (7,  "- K2Platform::setI2SClockEnable enable=%d returns %d", enable, result );

	return result;
}

//	--------------------------------------------------------------------------------
bool K2Platform::getI2SClockEnable () {
	const OSSymbol*			funcSymbolName = NULL;
	IOReturn				err;
	UInt32					value = 0;
	bool					result = mAppleI2S_ClockEnable;

	if ( mK2Service ) {
		funcSymbolName = makeFunctionSymbolName( kAppleI2S_GetClockEnable, mI2SPHandle );
		FailIf ( NULL == funcSymbolName, Exit );
		err = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)&value, (void *)(UInt32)0, 0, 0 );
		funcSymbolName->release ();	// [3324205]
		FailIf ( kIOReturnSuccess != err, Exit );
		result = ( 0 != value );
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setI2SCellEnable (bool enable) {
	IOReturn				result;
	const OSSymbol*			funcSymbolName = NULL;
	
	debugIOLog (7, "+ K2Platform::setI2SCellEnable enable=%d",enable);
	
	result = kIOReturnError;
	if ( mK2Service ) {
		if ( enable ) {
			debugIOLog (7,  "K2Platform::setI2SCellEnable to 1" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_CellEnable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		} else {
			debugIOLog (7,  "K2Platform::setI2SCellEnable to 0" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_CellDisable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( kIOReturnSuccess == result ) { mAppleI2S_CellEnable = enable; }
	}
Exit:
	debugIOLog (7, "- K2Platform::setI2SCellEnable result = %x",result);

	return result;
}

//	--------------------------------------------------------------------------------
bool K2Platform::getI2SCellEnable () {
	const OSSymbol*			funcSymbolName;
	IOReturn				err;
	UInt32					value = 0;
	bool					result = mAppleI2S_CellEnable;

	if ( mK2Service ) {
		funcSymbolName = makeFunctionSymbolName( kAppleI2S_GetCellEnable, mI2SPHandle );
		FailIf ( NULL == funcSymbolName, Exit );
		err = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)&value, (void *)(UInt32)0, 0, 0 );
		funcSymbolName->release ();	// [3324205]
		FailIf ( kIOReturnSuccess != err, Exit );
		result = ( 0 != value );
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setI2SSWReset(bool enable) {
	IOReturn				result;
	const OSSymbol*			funcSymbolName = NULL;
	
	debugIOLog (7, "+ K2Platform::setI2SSWReset enable=%d",enable);
	
	result = kIOReturnError;
	if ( mK2Service ) {
		if ( enable ) {
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_Reset, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		} else {
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_Run, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		if ( kIOReturnSuccess == result ) { mAppleI2S_Reset = enable; }
	}
Exit:
	debugIOLog (7, "- K2Platform::setI2SSWReset result = %x",result);

	return result;
}


//	--------------------------------------------------------------------------------
bool K2Platform::getI2SSWReset() {
	const OSSymbol*			funcSymbolName = NULL;
	IOReturn				err;
	UInt32					value = 0;
	bool					result = mAppleI2S_Reset;

	if ( mK2Service ) {
		funcSymbolName = makeFunctionSymbolName( kAppleI2S_GetReset, mI2SPHandle );
		FailIf ( NULL == funcSymbolName, Exit );
		err = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)&value, (void *)(UInt32)0, 0, 0 );
		funcSymbolName->release ();	// [3324205]
		FailIf ( kIOReturnSuccess != err, Exit );
		result = ( 0 != value );
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setSerialFormatRegister (UInt32 serialFormat) {
	IOReturn				result = kIOReturnError;

	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	result = kIOReturnError;
	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SSetSerialFormatReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		result = mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)serialFormat, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2Platform::getSerialFormatRegister () {
	UInt32					serialFormat = 0;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SGetSerialFormatReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)&serialFormat, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
	return serialFormat;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setDataWordSizes (UInt32 dataWordSizes) {
	IOReturn				result;

	result = kIOReturnError;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SSetDataWordSizesReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		result = mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)dataWordSizes, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2Platform::getDataWordSizes () {
	UInt32					dataWordSizes = 0;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SGetDataWordSizesReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)&dataWordSizes, 0, 0 );		//	[3323977]
		funcSymbolName->release ();																//	[3323977]	release OSSymbol references
	}
Exit:
	return dataWordSizes;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setI2SIOMIntControl (UInt32 intCntrl) {
	IOReturn				result;

	result = kIOReturnError;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SSetIntCtlReg );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		result = mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)intCntrl, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2Platform::getI2SIOMIntControl () {
	UInt32					i2sIntCntrl = 0;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SGetIntCtlReg );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)&i2sIntCntrl, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
	return i2sIntCntrl;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setFrameCount (UInt32 value) {
	IOReturn				result;

	//	<<<<<<<<<< WARNING >>>>>>>>>>
	//	Do not debugIOLog in here it screws up the hal timing and causes stuttering audio
	//	<<<<<<<<<< WARNING >>>>>>>>>>
	
	result = kIOReturnError;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SSetFrameCountReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		result = mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)value, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2Platform::getFrameCount () {
	UInt32					frameCount = 0;

	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SGetFrameCountReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)&frameCount, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
	return frameCount;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::releaseI2SClockSource(I2SClockFrequency inFrequency)	
{
	UInt32						i2sCellNumber;
	IOReturn					result;

	result = kIOReturnError;

	if (kUseI2SCell0 == mI2SInterfaceNumber)
		i2sCellNumber = kUseI2SCell0;
	else if (kUseI2SCell1 == mI2SInterfaceNumber)
		i2sCellNumber = kUseI2SCell1;
	else if (kUseI2SCell2 == mI2SInterfaceNumber)
		i2sCellNumber = kUseI2SCell2;
	else
		return kIOReturnBadArgument;

    if (NULL != mK2Service) {
		switch ( inFrequency ) {
			case kI2S_45MHz:		setupI2SClockSource( i2sCellNumber, false, kK2I2SClockSource_45MHz );		break;
			case kI2S_49MHz:		setupI2SClockSource( i2sCellNumber, false, kK2I2SClockSource_49MHz );		break;
			case kI2S_18MHz:		setupI2SClockSource( i2sCellNumber, false, kK2I2SClockSource_18MHz );		break;
		}
		result = kIOReturnSuccess;
	}

	return result;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::requestI2SClockSource(I2SClockFrequency inFrequency)	
{
	UInt32						i2sCellNumber;
	IOReturn					result;

	result = kIOReturnError;

	if (kUseI2SCell0 == mI2SInterfaceNumber)
		i2sCellNumber = kUseI2SCell0;
	else if (kUseI2SCell1 == mI2SInterfaceNumber)
		i2sCellNumber = kUseI2SCell1;
	else if (kUseI2SCell2 == mI2SInterfaceNumber)
		i2sCellNumber = kUseI2SCell2;
	else
		return kIOReturnBadArgument;

    if (NULL != mK2Service) {
		switch ( inFrequency ) {
			case kI2S_45MHz:		setupI2SClockSource( i2sCellNumber, true, kK2I2SClockSource_45MHz );		break;
			case kI2S_49MHz:		setupI2SClockSource( i2sCellNumber, true, kK2I2SClockSource_49MHz );		break;
			case kI2S_18MHz:		setupI2SClockSource( i2sCellNumber, true, kK2I2SClockSource_18MHz );		break;
		}
		result = kIOReturnSuccess;
	}

	return result;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setupI2SClockSource( UInt32 cell, bool requestClock, UInt32 clockSource )	
{
	IOReturn				result;
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	result = kIOReturnError;
	if ( mK2Service ) {
		funcSymbolName = OSSymbol::withCString ( "keyLargo_powerI2S" );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
 		mK2Service->callPlatformFunction (funcSymbolName, false, (void *)requestClock, (void *)cell, (void *)clockSource, (void *)0);		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
		result = kIOReturnSuccess;
	}
Exit:
	return result;
}

#pragma mark ---------------------------
#pragma mark INTERRUPTS
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IOReturn K2Platform::disableInterrupt ( PlatformInterruptSource source ) {
	const OSSymbol * 		selector = OSSymbol::withCString ( kIOPFInterruptRegister );
	const OSSymbol *		enFuncSymbolName = NULL;
	void *					interruptHandler = NULL;
	IOReturn				result;

	result = kIOReturnError;
	//	NOTE:	Some interrupt sources, although supported by platform functions, do not use the platform functions.
	//			Examples would be where a hardware interrupt status is not latched and is updated at a rate that
	//			exceeds the service latency.  These interrupt sources will be polled through a periodic timer to
	//			prevent interrupts from being queued at a rate that exceeds the ability to service the interrupt.
	if ( interruptUsesTimerPolling ( source ) ) {
		result = kIOReturnSuccess;
	} else {
		if ( mK2Service ) {
			switch ( source ) {
				case kCodecErrorInterrupt:			 interruptHandler = mCodecErrorInterruptHandler;												break;
				case kCodecInterrupt:				 interruptHandler = mCodecInterruptHandler;														break;
				case kDigitalInDetectInterrupt:		 interruptHandler = mDigitalInDetectInterruptHandler;											break;
				case kDigitalOutDetectInterrupt:	 interruptHandler = mDigitalOutDetectInterruptHandler;											break;
				case kHeadphoneDetectInterrupt:		 interruptHandler = mHeadphoneDetectInterruptHandler;											break;
				case kLineInputDetectInterrupt:		 interruptHandler = mLineInputDetectInterruptHandler;											break;
				case kLineOutputDetectInterrupt:	 interruptHandler = mLineOutputDetectInterruptHandler;											break;
				case kSpeakerDetectInterrupt:		 interruptHandler = mSpeakerDetectInterruptHandler;												break;
				case kUnknownInterrupt:																												break;
				default:																															break;
			}
			FailIf ( NULL == interruptHandler, Exit );
			switch ( source ) {
				case kCodecErrorInterrupt:			enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableCodecErrorIRQ, mI2SPHandle );		break;
				case kCodecInterrupt:				enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableCodecIRQ, mI2SPHandle );			break;
				case kDigitalOutDetectInterrupt:	enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableDigitalOutDetect, mI2SPHandle );	break;
				case kDigitalInDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableDigitalInDetect, mI2SPHandle );		break;
				case kHeadphoneDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName( kAppleGPIO_DisableHeadphoneDetect, mI2SPHandle );	break;
				case kLineInputDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableLineInDetect, mI2SPHandle );		break;
				case kLineOutputDetectInterrupt:	enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableLineOutDetect, mI2SPHandle );		break;
				case kSpeakerDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableSpeakerDetect, mI2SPHandle );		break;
				case kUnknownInterrupt:
				default:							debugIOLog (7,  "Attempt to disable unknown interrupt source" );								break;
			}
			FailIf ( NULL == enFuncSymbolName, Exit );
			FailIf (NULL == selector, Exit);
			result = mK2Service->callPlatformFunction ( enFuncSymbolName, true, interruptHandler, this, (void*)source, (void*)selector);
			selector->release ();
			enFuncSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
Exit:
	if ( kIOReturnSuccess == result ) {
		switch ( source ) {
			case kCodecErrorInterrupt:			mCodecErrorInterruptEnable = false;																	break;
			case kCodecInterrupt:				mCodecInterruptEnable = false;																		break;
			case kDigitalInDetectInterrupt:		mDigitalInDetectInterruptEnable = false;															break;
			case kDigitalOutDetectInterrupt:	mDigitalOutDetectInterruptEnable = false;															break;
			case kHeadphoneDetectInterrupt:		mHeadphoneDetectInterruptEnable = false;															break;
			case kLineInputDetectInterrupt:		mLineInputDetectInterruptEnable = false;															break;
			case kLineOutputDetectInterrupt:	mLineOutputDetectInterruptEnable = false;															break;
			case kSpeakerDetectInterrupt:		mSpeakerDetectInterruptEnable = false;																break;
			case kUnknownInterrupt:																													break;
			default:																																break;
		}
	}
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn K2Platform::enableInterrupt ( PlatformInterruptSource source ) {
	const OSSymbol * 		selector = OSSymbol::withCString ( kIOPFInterruptRegister );
	const OSSymbol *		enFuncSymbolName = NULL;
	void *					interruptHandler = NULL;
	IOReturn				result;

	result = kIOReturnError;
	//	NOTE:	Some interrupt sources, although supported by platform functions, do not use the platform functions.
	//			Examples would be where a hardware interrupt status is not latched and is updated at a rate that
	//			exceeds the service latency.  These interrupt sources will be polled through a periodic timer to
	//			prevent interrupts from being queued at a rate that exceeds the ability to service the interrupt.
	if ( interruptUsesTimerPolling ( source ) ) {
		result = kIOReturnSuccess;
	} else {
		if ( mK2Service ) {
			switch ( source ) {
				case kCodecErrorInterrupt:			 interruptHandler = mCodecErrorInterruptHandler;												break;
				case kCodecInterrupt:				 interruptHandler = mCodecInterruptHandler;														break;
				case kDigitalInDetectInterrupt:		 interruptHandler = mDigitalInDetectInterruptHandler;											break;
				case kDigitalOutDetectInterrupt:	 interruptHandler = mDigitalOutDetectInterruptHandler;											break;
				case kHeadphoneDetectInterrupt:		 interruptHandler = mHeadphoneDetectInterruptHandler;											break;
				case kLineInputDetectInterrupt:		 interruptHandler = mLineInputDetectInterruptHandler;											break;
				case kLineOutputDetectInterrupt:	 interruptHandler = mLineOutputDetectInterruptHandler;											break;
				case kSpeakerDetectInterrupt:		 interruptHandler = mSpeakerDetectInterruptHandler;												break;
				case kUnknownInterrupt:																												break;
				default:																															break;
			}
			FailIf ( NULL == interruptHandler, Exit );
			switch ( source ) {
				case kCodecErrorInterrupt:			enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableCodecErrorIRQ, mI2SPHandle );		break;
				case kCodecInterrupt:				enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableCodecIRQ, mI2SPHandle );				break;
				case kDigitalOutDetectInterrupt:	enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableDigitalOutDetect, mI2SPHandle );		break;
				case kDigitalInDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableDigitalInDetect, mI2SPHandle );		break;
				case kHeadphoneDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName( kAppleGPIO_EnableHeadphoneDetect, mI2SPHandle );		break;
				case kLineInputDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableLineInDetect, mI2SPHandle );			break;
				case kLineOutputDetectInterrupt:	enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableLineOutDetect, mI2SPHandle );		break;
				case kSpeakerDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableSpeakerDetect, mI2SPHandle );		break;
				case kUnknownInterrupt:
				default:							debugIOLog (7,  "Attempt to enable unknown interrupt source" );								break;
			}
			FailIf ( NULL == enFuncSymbolName, Exit );
			FailIf (NULL == selector, Exit);
			result = mK2Service->callPlatformFunction ( enFuncSymbolName, true, interruptHandler, this, (void*)source, (void*)selector);
			selector->release ();
			enFuncSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
Exit:
	if ( kIOReturnSuccess == result ) {
		switch ( source ) {
			case kCodecErrorInterrupt:			mCodecErrorInterruptEnable = true;																	break;
			case kCodecInterrupt:				mCodecInterruptEnable = true;																		break;
			case kDigitalInDetectInterrupt:		mDigitalInDetectInterruptEnable = true;																break;
			case kDigitalOutDetectInterrupt:	mDigitalOutDetectInterruptEnable = true;															break;
			case kHeadphoneDetectInterrupt:		mHeadphoneDetectInterruptEnable = true;																break;
			case kLineInputDetectInterrupt:		mLineInputDetectInterruptEnable = true;																break;
			case kLineOutputDetectInterrupt:	mLineOutputDetectInterruptEnable = true;															break;
			case kSpeakerDetectInterrupt:		mSpeakerDetectInterruptEnable = true;																break;
			case kUnknownInterrupt:																													break;
			default:																																break;
		}
	}
	return result;
}


//	--------------------------------------------------------------------------------
void K2Platform::poll ( void ) {
	IOCommandGate *			cg;

	FailIf ( NULL == mProvider, Exit );
	cg = mProvider->getCommandGate ();
	if ( NULL != cg ) {
		if ( interruptUsesTimerPolling ( kCodecErrorInterrupt ) ) {
			if ( mCodecErrorInterruptEnable ) {
				cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kCodecErrorInterruptStatus, (void *)getCodecErrorInterrupt (), (void *)0 );
			}
		}
		if ( interruptUsesTimerPolling ( kCodecInterrupt ) ) {
			if ( mCodecInterruptEnable ) {
				cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kCodecInterruptStatus, (void *)getCodecInterrupt (), (void *)0 );
			}
		}
	}
Exit:
	return;
}


//	--------------------------------------------------------------------------------
//	Some interrupt sources, although supported by platform functions, do not use the platform functions.
//	Examples would be where a hardware interrupt status is not latched and is updated at a rate that
//	exceeds the service latency.  These interrupt sources will be polled through a periodic timer to
//	prevent interrupts from being queued at a rate that exceeds the ability to service the interrupt.
//	NOTE:	This check would be better served through indications in the plist so that codec
//			dependencies are avoided in this platform interface object!!!
bool K2Platform::interruptUsesTimerPolling( PlatformInterruptSource source ) {
	bool		result = false;
	
	if ( kCodecErrorInterrupt == source ) {
		result = true;
	}
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn K2Platform::registerInterruptHandler ( IOService * theDevice, void * interruptHandler, PlatformInterruptSource source ) {
	const OSSymbol * 		selector = OSSymbol::withCString ( kIOPFInterruptRegister );
	const OSSymbol *		funcSymbolName = NULL;
	IOReturn				result;

	result = kIOReturnError;
	//	Don't register interrupt services with platform methods if the platform method is not going to be used.
	if ( interruptUsesTimerPolling ( source ) ) {
		result = kIOReturnSuccess;
	} else {
		if ( mK2Service ) {
			switch ( source ) {
				case kCodecErrorInterrupt:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterCodecErrorIRQ, mI2SPHandle );		break;
				case kCodecInterrupt:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterCodecIRQ, mI2SPHandle );			break;
				case kDigitalInDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterDigitalInDetect, mI2SPHandle );		break;
				case kDigitalOutDetectInterrupt:	funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterDigitalOutDetect, mI2SPHandle );	break;
				case kHeadphoneDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterHeadphoneDetect, mI2SPHandle );		break;
				case kLineInputDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterLineInDetect, mI2SPHandle );		break;
				case kLineOutputDetectInterrupt:	funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterLineOutDetect, mI2SPHandle );		break;
				case kSpeakerDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterSpeakerDetect, mI2SPHandle );		break;
				case kUnknownInterrupt:
				default:							debugIOLog (7,  "Attempt to register unknown interrupt source" );								break;
			}
			FailIf ( NULL == funcSymbolName, Exit );
			FailIf (NULL == selector, Exit);
			result = mK2Service->callPlatformFunction ( funcSymbolName, true, interruptHandler, this, (void*)source, (void*)selector);
			selector->release ();
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
Exit:
	if ( kIOReturnSuccess == result ) {
		//	Keep a copy of a pointer to the interrupt service routine in this object so that the enable
		//	and disable interrupt methods have access to the pointer since it is possible that
		//	the interrupt might be enabled or disabled by an object that does not have the pointer.
		switch ( source ) {
			case kCodecErrorInterrupt:			mCodecErrorInterruptHandler = interruptHandler;														break;
			case kCodecInterrupt:				mCodecInterruptHandler = interruptHandler;															break;
			case kDigitalInDetectInterrupt:		mDigitalInDetectInterruptHandler = interruptHandler;												break;
			case kDigitalOutDetectInterrupt:	mDigitalOutDetectInterruptHandler = interruptHandler;												break;
			case kHeadphoneDetectInterrupt:		mHeadphoneDetectInterruptHandler = interruptHandler;												break;
			case kLineInputDetectInterrupt:		mLineInputDetectInterruptHandler = interruptHandler;												break;
			case kLineOutputDetectInterrupt:	mLineOutputDetectInterruptHandler = interruptHandler;												break;
			case kSpeakerDetectInterrupt:		mSpeakerDetectInterruptHandler = interruptHandler;													break;
			case kUnknownInterrupt:																													break;
			default:																																break;
		}
		enableInterrupt ( source );
	}
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::unregisterInterruptHandler ( IOService * theDevice, void * interruptHandler, PlatformInterruptSource source ) {
	const OSSymbol * 		selector = OSSymbol::withCString ( kIOPFInterruptUnRegister );
	const OSSymbol *		funcSymbolName = NULL;
	IOReturn				result;

	result = kIOReturnError;
	FailIf (NULL == mK2Service, Exit);

	disableInterrupt ( source );
	//	No need to unregister interrupt services with platform methods if the platform method was not used.
	if ( !interruptUsesTimerPolling ( source ) ) {
		switch ( source ) {
			case kCodecErrorInterrupt:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterCodecErrorIRQ, mI2SPHandle );			break;
			case kCodecInterrupt:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterCodecIRQ, mI2SPHandle );				break;
			case kDigitalInDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterDigitalInDetect, mI2SPHandle );		break;
			case kDigitalOutDetectInterrupt:	funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterDigitalOutDetect, mI2SPHandle );		break;
			case kHeadphoneDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterHeadphoneDetect, mI2SPHandle );		break;
			case kLineInputDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterLineInDetect, mI2SPHandle );			break;
			case kLineOutputDetectInterrupt:	funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterLineOutDetect, mI2SPHandle );			break;
			case kSpeakerDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterSpeakerDetect, mI2SPHandle );			break;
			case kUnknownInterrupt:
			default:							debugIOLog (7,  "Attempt to register unknown interrupt source" );									break;
		}
		FailIf ( NULL == funcSymbolName, Exit );
		FailIf (NULL == selector, Exit);
		result = mK2Service->callPlatformFunction ( funcSymbolName, true, interruptHandler, theDevice, (void*)source, (void*)selector);
		selector->release ();
		funcSymbolName->release ();	// [3324205]
		FailIf ( kIOReturnSuccess != result && kCodecErrorInterrupt == source, Exit );
		FailIf ( kIOReturnSuccess != result && kCodecInterrupt == source, Exit );
		FailIf ( kIOReturnSuccess != result && kDigitalInDetectInterrupt == source, Exit );
		FailIf ( kIOReturnSuccess != result && kDigitalOutDetectInterrupt == source, Exit );
		FailIf ( kIOReturnSuccess != result && kHeadphoneDetectInterrupt == source, Exit );
		FailIf ( kIOReturnSuccess != result && kLineInputDetectInterrupt == source, Exit );
		FailIf ( kIOReturnSuccess != result && kLineOutputDetectInterrupt == source, Exit );
		FailIf ( kIOReturnSuccess != result && kSpeakerDetectInterrupt == source, Exit );
		FailIf ( kIOReturnSuccess != result, Exit );
	}

Exit:
	return result;
}

#pragma mark ---------------------------
#pragma mark GPIO
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getClockMux() {
	return readGpioState ( kGPIO_Selector_ClockMux );
}

//	--------------------------------------------------------------------------------
IOReturn 	K2Platform::setClockMux ( GpioAttributes muxState ) {
	debugIOLog (7,  "K2Platform::setClockMux ( %d )", muxState );
	return writeGpioState ( kGPIO_Selector_ClockMux, muxState );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getCodecErrorInterrupt() {
	return readGpioState ( kGPIO_Selector_CodecErrorInterrupt );
}


//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getCodecInterrupt() {
	return readGpioState ( kGPIO_Selector_CodecInterrupt );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getComboInJackTypeConnected() {
	return readGpioState ( kGPIO_Selector_ComboInJackType );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getComboOutJackTypeConnected() {
	return readGpioState ( kGPIO_Selector_ComboOutJackType );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getDigitalInConnected() {
	GpioAttributes 			gpioState;

	if (kGPIO_Selector_NotAssociated == mComboInAssociation) {
		debugIOLog (7,  "K2Platform::getDigitalInConnected: no combo jack");
		gpioState = readGpioState ( kGPIO_Selector_DigitalInDetect );
	} else {
		debugIOLog (7,  "K2Platform::getDigitalInConnected: combo jack");
		if (kGPIO_TypeIsDigital == getComboInJackTypeConnected()) {
			gpioState = readGpioState ( mComboInAssociation );
			debugIOLog (7,  "combo in jack connection is digital type, state = %d", gpioState);
		} else {
			gpioState = kGPIO_Disconnected;
			debugIOLog (7,  "combo in jack connection is analog type");
		}
	}
	return gpioState;
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getDigitalOutConnected() {
	GpioAttributes 			gpioState;

	if (kGPIO_Selector_NotAssociated == mComboOutAssociation) {
		debugIOLog (7,  "K2Platform::getDigitalOutConnected: no combo jack");
		gpioState = readGpioState ( kGPIO_Selector_DigitalOutDetect );
	} else {
		debugIOLog (7,  "K2Platform::getDigitalOutConnected: combo jack");
		if (kGPIO_TypeIsDigital == getComboOutJackTypeConnected()) {
			gpioState = readGpioState ( mComboOutAssociation );
			debugIOLog (7,  "combo out jack connection is digital type, state = %d", gpioState);
		} else {
			gpioState = kGPIO_Disconnected;
			debugIOLog (7,  "combo out jack connection is analog type");
		}
	}
	return gpioState;
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getHeadphoneConnected() {
	return readGpioState ( kGPIO_Selector_HeadphoneDetect );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getHeadphoneMuteState() {
	return readGpioState ( kGPIO_Selector_HeadphoneMute );
}
	
//	--------------------------------------------------------------------------------
IOReturn 	K2Platform::setHeadphoneMuteState ( GpioAttributes muteState ) {
	debugIOLog (7,  "K2Platform::setHeadphoneMuteState ( %d )", muteState );
	return writeGpioState ( kGPIO_Selector_HeadphoneMute, muteState );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getInputDataMux() {
	return readGpioState ( kGPIO_Selector_InputDataMux );
}

//	--------------------------------------------------------------------------------
IOReturn 	K2Platform::setInputDataMux(GpioAttributes muxState) {
	debugIOLog (7,  "K2Platform::setInputDataMux ( %d )", muxState );
	return writeGpioState ( kGPIO_Selector_InputDataMux, muxState );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getLineInConnected() {
	GpioAttributes 			gpioState;

	gpioState = readGpioState ( kGPIO_Selector_LineInDetect );

	if (kGPIO_Selector_NotAssociated == mComboInAssociation) {
		debugIOLog (7,  "K2Platform::getLineInConnected: no combo in jack association, line in state = %d", gpioState);
	} else {
		debugIOLog (7,  "K2Platform::getLineInConnected: combo jack is associated with line in");
		if (kGPIO_TypeIsDigital == getComboInJackTypeConnected()) {
			gpioState = kGPIO_Disconnected;
			debugIOLog (7,  "combo in jack connection is digital type, return line out as NOT connected");
		}
	}
	return gpioState;
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getLineOutConnected(bool ignoreCombo) {
	GpioAttributes 			gpioState;

	debugIOLog (7,  "K2Platform::getLineOutConnected (ignoreCombo = %d)", ignoreCombo);

	gpioState = readGpioState ( kGPIO_Selector_LineOutDetect );

	if (false == ignoreCombo) {
		if (kGPIO_Selector_NotAssociated == mComboOutAssociation) {
			debugIOLog (7,  "K2Platform::getLineOutConnected: no combo out jack association, line out state = %d", gpioState);
		} else {
			debugIOLog (7,  "K2Platform::getLineOutConnected: combo jack is associated with line out");
			if (kGPIO_TypeIsDigital == getComboOutJackTypeConnected()) {
				gpioState = kGPIO_Disconnected;
				debugIOLog (7,  "combo out jack connection is digital type, return line out as NOT connected");
			}
		}
	}
	return gpioState;
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getLineOutMuteState() {
	return readGpioState ( kGPIO_Selector_LineOutMute );
}
	
//	--------------------------------------------------------------------------------
IOReturn 	K2Platform::setLineOutMuteState ( GpioAttributes muteState ) {
	debugIOLog (7,  "K2Platform::setLineOutMuteState ( %d )", muteState );
	return writeGpioState ( kGPIO_Selector_LineOutMute, muteState );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getSpeakerConnected() {
	return readGpioState ( kGPIO_Selector_SpeakerDetect );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getSpeakerMuteState() {
	return readGpioState ( kGPIO_Selector_SpeakerMute );
}

//	--------------------------------------------------------------------------------
IOReturn 	K2Platform::setSpeakerMuteState ( GpioAttributes muteState ) {
	debugIOLog (7,  "K2Platform::setSpeakerMuteState ( %d )", muteState );
	return writeGpioState ( kGPIO_Selector_SpeakerMute, muteState );
}
	
#pragma mark ---------------------------
#pragma mark Private I2C
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
bool K2Platform::findAndAttachI2C()
{
	const OSSymbol	*i2cDriverName;
	IOService		*i2cCandidate = NULL;
	OSDictionary	*i2cServiceDictionary;
	mach_timespec_t	timeout;
	bool			result = false;
	OSIterator		*iterator;
	OSObject		*theObject;
	bool			found = false;
	
	// Searches the i2c:
	i2cDriverName = OSSymbol::withCStringNoCopy ( "PPCI2CInterface.i2c-mac-io" );
	i2cServiceDictionary = IOService::resourceMatching ( i2cDriverName );
	iterator = NULL;
	FailIf ( NULL == i2cServiceDictionary, Exit );
	
	debugIOLog (5,  "about to waitForService on i2cServiceDictionary timeout = 5 seconds" );

	timeout.tv_sec = 5;
	timeout.tv_nsec = 0;
	i2cCandidate = IOService::waitForService ( i2cServiceDictionary, &timeout );
	if ( NULL == i2cCandidate ) {
		iterator = IOService::getMatchingServices ( i2cServiceDictionary );
		if ( NULL != iterator ) {
			do {
				theObject = iterator->getNextObject ();
				if ( theObject ) {
					debugIOLog (5, "found theObject=%p",theObject);
					i2cCandidate = OSDynamicCast(IOService,theObject);
				}
			} while ( !found && NULL != theObject );
		} else {
			debugIOLog (5, " NULL != iterator");
		}
	} else {
		debugIOLog (5,  "K2Platform::findAndAttachI2C i2cCandidate = %p ",i2cCandidate );
	}
	
	FailIf(NULL == i2cCandidate,Exit);
	
	mI2CInterface = (PPCI2CInterface*)i2cCandidate->getProperty ( i2cDriverName );
	FailIf ( NULL == mI2CInterface, Exit );

	mI2CInterface->retain();
	result = true;
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
bool K2Platform::openI2C()
{
	bool		result = false;
	
	FailIf ( NULL == mI2CInterface, Exit );
	FailIf ( !mI2CInterface->openI2CBus ( mI2CPort ), Exit );
	result = true;
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
void K2Platform::closeI2C ()
{
	mI2CInterface->closeI2CBus ();
}

//	--------------------------------------------------------------------------------
bool K2Platform::detachFromI2C()
{
	if (mI2CInterface) {
		mI2CInterface->release();
		mI2CInterface = NULL;
	}
	return (true);
}

#pragma mark ---------------------------
#pragma mark Private I2S
#pragma mark ---------------------------

#ifndef kBUILD_FOR_DIRECT_I2S_HW_ACCESS	
//	--------------------------------------------------------------------------------
bool K2Platform::findAndAttachI2S()
{

	const OSSymbol	*i2sDriverName;
	IOService		*i2sCandidate;
	OSDictionary	*i2sServiceDictionary;
	bool			result = false;
	mach_timespec_t	timeout	= {5,0};

	i2sCandidate = NULL;
	i2sDriverName = OSSymbol::withCStringNoCopy ( "AppleI2S" );
	i2sServiceDictionary = IOService::resourceMatching ( i2sDriverName );
	FailIf ( NULL == i2sServiceDictionary, Exit );

	i2sCandidate = IOService::waitForService ( i2sServiceDictionary, &timeout );
	debugIOLog (7,  "i2sServiceDictionary %p", i2sServiceDictionary );
	FailIf(NULL == i2sCandidate,Exit);
	
	mI2SInterface = (AppleI2S*)i2sCandidate->getProperty ( i2sDriverName );
	FailIf ( NULL == mI2SInterface, Exit );
	
	mI2SInterface->retain();
	result = true;
	
Exit:
	if (i2sDriverName) i2sDriverName->release ();
//	if (i2sCandidate) i2sCandidate->release ();
//	if (i2sServiceDictionary) i2sServiceDictionary->release ();
	return result;
}

//	--------------------------------------------------------------------------------
bool K2Platform::openI2S()
{
	return true;			//	No open in K2 I2S driver
}

//	--------------------------------------------------------------------------------
void K2Platform::closeI2S ()
{
	return;					//	No close in K2 I2S driver
}

//	--------------------------------------------------------------------------------
bool K2Platform::detachFromI2S()
{

	if ( NULL != mI2SInterface ) {
		mI2SInterface->release();
		mI2SInterface = NULL;
	}

	return ( true );
}
#endif

#pragma mark ---------------------------
#pragma mark GPIO Utilities
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
const OSSymbol* K2Platform::makeFunctionSymbolName(const char * name,UInt32 pHandle) {
	const OSSymbol* 	funcSymbolName = NULL;
	char		stringBuf[256];
		
	sprintf ( stringBuf, "%s-%08lx", name, pHandle );
	funcSymbolName = OSSymbol::withCString ( stringBuf );
	
	return funcSymbolName;
}

//	--------------------------------------------------------------------------------
GpioAttributes  K2Platform::GetCachedAttribute ( GPIOSelector selector, GpioAttributes defaultResult ) {
	GpioAttributes		result;
	
	result = defaultResult;
	//	If there is no platform function then return the cached GPIO state if a cache state is available
	switch ( selector ) {
		case kGPIO_Selector_AnalogCodecReset:		result = mAppleGPIO_AnalogCodecReset;																				break;
		case kGPIO_Selector_ClockMux:				result = mAppleGPIO_CodecClockMux;																					break;
		case kGPIO_Selector_CodecInterrupt:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_CodecErrorInterrupt:	/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_ComboInJackType:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_ComboOutJackType:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_DigitalCodecReset:		result = mAppleGPIO_DigitalCodecReset;																				break;
		case kGPIO_Selector_DigitalInDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_DigitalOutDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_HeadphoneDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_HeadphoneMute:			result = mAppleGPIO_HeadphoneMute;																					break;
		case kGPIO_Selector_InputDataMux:			result = mAppleGPIO_CodecInputDataMux;																				break;
		case kGPIO_Selector_InternalSpeakerID:		result = mAppleGPIO_InternalSpeakerID;																				break;
		case kGPIO_Selector_LineInDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_LineOutDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_LineOutMute:			result = mAppleGPIO_LineOutMute;																					break;
		case kGPIO_Selector_SpeakerDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_SpeakerMute:			result = mAppleGPIO_AmpMute;																						break;
		case kGPIO_Selector_ExternalMicDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_NotAssociated:			/*	NO CACHE AVAILABLE ON UNAVAILABLE ONLY GPIO	*/																	break;
	}

	switch ( selector ) {
		case kGPIO_Selector_AnalogCodecReset:		debugIOLog (5,  "... kGPIO_Selector_AnalogCodecReset returns %d from CACHE", result );								break;
		case kGPIO_Selector_ClockMux:				debugIOLog (5,  "... kGPIO_Selector_ClockMux returns %d from CACHE", result );										break;
		case kGPIO_Selector_CodecInterrupt:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_CodecErrorInterrupt:	/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_ComboInJackType:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_ComboOutJackType:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_DigitalCodecReset:		debugIOLog (5,  "... kGPIO_Selector_DigitalCodecReset returns %d from CACHE", result );								break;
		case kGPIO_Selector_DigitalInDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_DigitalOutDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_HeadphoneDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_HeadphoneMute:			debugIOLog (5,  "... kGPIO_Selector_HeadphoneMute returns %d from CACHE", result );									break;
		case kGPIO_Selector_InputDataMux:			debugIOLog (5,  "... kGPIO_Selector_InputDataMux returns %d from CACHE", result );									break;
		case kGPIO_Selector_InternalSpeakerID:		debugIOLog (5,  "... kGPIO_Selector_InternalSpeakerID returns %d from CACHE", result );								break;
		case kGPIO_Selector_LineInDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_LineOutDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_LineOutMute:			debugIOLog (5,  "... kGPIO_Selector_LineOutMute returns %d from CACHE", result );									break;
		case kGPIO_Selector_SpeakerDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_SpeakerMute:			debugIOLog (5,  "... kGPIO_Selector_SpeakerMute returns %d from CACHE", result );									break;
		case kGPIO_Selector_ExternalMicDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_NotAssociated:			/*	NO CACHE AVAILABLE ON UNAVAILABLE ONLY GPIO	*/																	break;
	}

	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes K2Platform::readGpioState ( GPIOSelector selector ) {
	GpioAttributes		result;
	
	result = kGPIO_Unknown;
	UInt32				value;
	const OSSymbol*		funcSymbolName = NULL;
	IOReturn			err;
	bool				waitForFunction;

	if ( mK2Service ) {
		switch ( selector ) {
			case kGPIO_Selector_AnalogCodecReset:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetAudioHwReset, mI2SPHandle);									break;
			case kGPIO_Selector_ClockMux:					funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetCodecClockMux, mI2SPHandle);									break;
			case kGPIO_Selector_CodecInterrupt:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetCodecIRQ, mI2SPHandle);										break;
			case kGPIO_Selector_CodecErrorInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetCodecErrorIRQ, mI2SPHandle);									break;
			case kGPIO_Selector_ComboInJackType:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetComboInJackType, mI2SPHandle);								break;
			case kGPIO_Selector_ComboOutJackType:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetComboOutJackType, mI2SPHandle);								break;
			case kGPIO_Selector_DigitalCodecReset:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetAudioDigHwReset, mI2SPHandle);								break;
			case kGPIO_Selector_DigitalInDetect:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetDigitalInDetect, mI2SPHandle);								break;
			case kGPIO_Selector_DigitalOutDetect:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetDigitalOutDetect, mI2SPHandle);								break;
			case kGPIO_Selector_HeadphoneDetect:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetHeadphoneDetect, mI2SPHandle);								break;
			case kGPIO_Selector_HeadphoneMute:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetHeadphoneMute, mI2SPHandle);									break;
			case kGPIO_Selector_InputDataMux:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetCodecInputDataMux, mI2SPHandle);								break;
			case kGPIO_Selector_InternalSpeakerID:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetInternalSpeakerID, mI2SPHandle);								break;
			case kGPIO_Selector_LineInDetect:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetLineInDetect, mI2SPHandle);									break;
			case kGPIO_Selector_LineOutDetect:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetLineOutDetect, mI2SPHandle);									break;
			case kGPIO_Selector_LineOutMute:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetLineOutMute, mI2SPHandle);									break;
			case kGPIO_Selector_SpeakerDetect:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetSpeakerDetect, mI2SPHandle);									break;
			case kGPIO_Selector_SpeakerMute:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_GetAmpMute, mI2SPHandle);										break;
			case kGPIO_Selector_ExternalMicDetect:																																break;
			case kGPIO_Selector_NotAssociated:																																	break;
			default:										FailIf ( true, Exit );																								break;
		}
		if ( NULL != funcSymbolName ) {
			//	This callPlatformFunction with a 'waitForFunction' argument of TRUE was implemented
			//	because the AppleGPIO driver had not loaded when AppleOnboardAudio loaded so the
			//	initialization method, which performs GPIO discovery through this method, would not
			//	perform the required initialization tasks.  A dependency on the AppleGPIO driver 
			//	may not have been put in because only the K2 platform interface object has this
			//	dependency while systems using the Keylargo platform interface object do not have
			//	this dependency and should not expect an AppleGPIO driver to be loaded. (rbm)
#if 0
			waitForFunction = kGPIO_Selector_HeadphoneDetect == selector ? true : false ;
#else
			waitForFunction = false;
#endif
			err = mK2Service->callPlatformFunction ( funcSymbolName , waitForFunction, (void*)&value, (void*)0, (void*)0, (void*)0 );
			funcSymbolName->release ();	// [3324205]

			if ( kIOReturnSuccess != err ) {
				switch ( selector ) {
					case kGPIO_Selector_AnalogCodecReset:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetAudioHwReset returned %X NOT FOUND", err );			break;
					case kGPIO_Selector_ClockMux:				debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetCodecClockMux returned %X NOT FOUND", err );		break;
					case kGPIO_Selector_CodecInterrupt:			debugIOLog (7,  "... callPlatformFunction for kAppleGPIO_GetCodecIRQ returned %X NOT FOUND", err );				break;
					case kGPIO_Selector_CodecErrorInterrupt:	debugIOLog (7,  "... callPlatformFunction for kAppleGPIO_GetCodecErrorIRQ returned %X NOT FOUND", err );		break;
					case kGPIO_Selector_ComboInJackType:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetComboInJackType returned %X NOT FOUND", err );		break;
					case kGPIO_Selector_ComboOutJackType:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetComboOutJackType returned %X NOT FOUND", err );		break;
					case kGPIO_Selector_DigitalCodecReset:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetAudioDigHwReset returned %X NOT FOUND", err );		break;
					case kGPIO_Selector_DigitalInDetect:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetDigitalInDetect returned %X NOT FOUND", err );		break;
					case kGPIO_Selector_DigitalOutDetect:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetDigitalOutDetect returned %X NOT FOUND", err );		break;
					case kGPIO_Selector_HeadphoneDetect:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetHeadphoneDetect returned %X NOT FOUND", err );		break;
					case kGPIO_Selector_HeadphoneMute:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetHeadphoneMute returned %X NOT FOUND", err );		break;
					case kGPIO_Selector_InputDataMux:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetCodecInputDataMux returned %X NOT FOUND", err );	break;
					case kGPIO_Selector_InternalSpeakerID:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetInternalSpeakerID returned %X NOT FOUND", err );	break;
					case kGPIO_Selector_LineInDetect:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetLineInDetect returned %X NOT FOUND", err );			break;
					case kGPIO_Selector_LineOutDetect:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetLineOutDetect returned %X NOT FOUND", err );		break;
					case kGPIO_Selector_LineOutMute:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetLineOutMute returned %X NOT FOUND", err );			break;
					case kGPIO_Selector_SpeakerDetect:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetSpeakerDetect returned %X NOT FOUND", err );		break;
					case kGPIO_Selector_SpeakerMute:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetAmpMute returned %X NOT FOUND", err );				break;
					case kGPIO_Selector_ExternalMicDetect:																														break;
					case kGPIO_Selector_NotAssociated:																															break;
					default:									debugIOLog (5,  "... callPlatformFunction for UNKNOWN SELECTOR %d returned %X NOT FOUND", selector, err );		break;
				}
				result = GetCachedAttribute ( selector, result );
			} else {
				switch ( selector ) {
					case kGPIO_Selector_AnalogCodecReset:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_SetAudioHwReset returned %ld", value );				break;
					case kGPIO_Selector_ClockMux:				debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_SetCodecClockMux returned %ld", value );				break;
					case kGPIO_Selector_CodecInterrupt:			debugIOLog (7,  "... callPlatformFunction for kAppleGPIO_GetCodecIRQ returned %ld", value );					break;
					case kGPIO_Selector_CodecErrorInterrupt:	debugIOLog (7,  "... callPlatformFunction for kAppleGPIO_GetCodecErrorIRQ returned %ld", value );				break;
					case kGPIO_Selector_ComboInJackType:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetComboInJackType returned %ld", value );				break;
					case kGPIO_Selector_ComboOutJackType:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetComboOutJackType returned %ld", value );			break;
					case kGPIO_Selector_DigitalCodecReset:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_SetAudioDigHwReset returned %ld", value );				break;
					case kGPIO_Selector_DigitalInDetect:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetDigitalInDetect returned %ld", value );				break;
					case kGPIO_Selector_DigitalOutDetect:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetDigitalOutDetect returned %ld", value );			break;
					case kGPIO_Selector_HeadphoneDetect:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetHeadphoneDetect returned %ld", value );				break;
					case kGPIO_Selector_HeadphoneMute:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_SetHeadphoneMute returned %ld", value );				break;
					case kGPIO_Selector_InputDataMux:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_SetCodecInputDataMux returned %ld", value );			break;
					case kGPIO_Selector_InternalSpeakerID:		debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_SetInternalSpeakerID returned %ld", value );			break;
					case kGPIO_Selector_LineInDetect:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetLineInDetect returned %ld", value );				break;
					case kGPIO_Selector_LineOutDetect:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetLineOutDetect returned %ld", value );				break;
					case kGPIO_Selector_LineOutMute:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_SetLineOutMute returned %ld", value );					break;
					case kGPIO_Selector_SpeakerDetect:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_GetSpeakerDetect returned %ld", value );				break;
					case kGPIO_Selector_SpeakerMute:			debugIOLog (5,  "... callPlatformFunction for kAppleGPIO_SetAmpMute returned %ld", value );						break;
					case kGPIO_Selector_ExternalMicDetect:																														break;
					case kGPIO_Selector_NotAssociated:																															break;
					default:									debugIOLog (5,  "... callPlatformFunction for UNKNOWN SELECTOR %d returned %ld", selector, value );				break;
				}
				//	Translate the GPIO state to a GPIO attribute
				switch ( selector ) {
					case kGPIO_Selector_AnalogCodecReset:		result = value ? kGPIO_Reset : kGPIO_Run;																		break;
					case kGPIO_Selector_ClockMux:				result = value ? kGPIO_MuxSelectAlternate : kGPIO_MuxSelectDefault;												break;
					case kGPIO_Selector_CodecInterrupt:			result = value ? kGPIO_CodecInterruptActive : kGPIO_CodecInterruptInactive;										break;
					case kGPIO_Selector_CodecErrorInterrupt:	result = value ? kGPIO_CodecInterruptActive : kGPIO_CodecInterruptInactive;										break;
					case kGPIO_Selector_ComboInJackType:		result = value ? kGPIO_TypeIsAnalog : kGPIO_TypeIsDigital;														break;
					case kGPIO_Selector_ComboOutJackType:		result = value ? kGPIO_TypeIsAnalog : kGPIO_TypeIsDigital;														break;
					case kGPIO_Selector_DigitalCodecReset:		result = value ? kGPIO_Reset : kGPIO_Run;																		break;
					case kGPIO_Selector_DigitalInDetect:		result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
					case kGPIO_Selector_DigitalOutDetect:		result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
					case kGPIO_Selector_HeadphoneDetect:		result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
					case kGPIO_Selector_HeadphoneMute:			result = value ? kGPIO_Muted : kGPIO_Unmuted ;																	break;
					case kGPIO_Selector_InputDataMux:			result = value ? kGPIO_MuxSelectAlternate : kGPIO_MuxSelectDefault;												break;
					case kGPIO_Selector_InternalSpeakerID:		result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
					case kGPIO_Selector_LineInDetect:			result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
					case kGPIO_Selector_LineOutDetect:			result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
					case kGPIO_Selector_LineOutMute:			result = value ? kGPIO_Muted : kGPIO_Unmuted;																	break;
					case kGPIO_Selector_SpeakerDetect:			result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
					case kGPIO_Selector_SpeakerMute:			result = value ? kGPIO_Muted : kGPIO_Unmuted;																	break;
					case kGPIO_Selector_ExternalMicDetect:		result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
					case kGPIO_Selector_NotAssociated:			result = kGPIO_Connected;																						break;
				}
				switch ( selector ) {
					case kGPIO_Selector_AnalogCodecReset:		debugIOLog (5,  "... kGPIO_Selector_AnalogCodecReset returns %d from callPlatformFunction", result );				break;
					case kGPIO_Selector_ClockMux:				debugIOLog (5,  "... kGPIO_Selector_ClockMux returns %d from callPlatformFunction", result );						break;
					case kGPIO_Selector_CodecInterrupt:			debugIOLog (7,  "... kGPIO_Selector_CodecInterrupt returns %d from callPlatformFunction", result );					break;
					case kGPIO_Selector_CodecErrorInterrupt:	debugIOLog (7,  "... kGPIO_Selector_CodecErrorInterrupt returns %d from callPlatformFunction", result );			break;
					case kGPIO_Selector_ComboInJackType:		debugIOLog (5,  "... kGPIO_Selector_ComboInJackType returns %d from callPlatformFunction", result );				break;
					case kGPIO_Selector_ComboOutJackType:		debugIOLog (5,  "... kGPIO_Selector_ComboOutJackType returns %d from callPlatformFunction", result );				break;
					case kGPIO_Selector_DigitalCodecReset:		debugIOLog (5,  "... kGPIO_Selector_DigitalCodecReset returns %d from callPlatformFunction", result );				break;
					case kGPIO_Selector_DigitalInDetect:		debugIOLog (5,  "... kGPIO_Selector_DigitalInDetect returns %d from callPlatformFunction", result );				break;
					case kGPIO_Selector_DigitalOutDetect:		debugIOLog (5,  "... kGPIO_Selector_DigitalOutDetect returns %d from callPlatformFunction", result );				break;
					case kGPIO_Selector_HeadphoneDetect:		debugIOLog (5,  "... kGPIO_Selector_HeadphoneDetect returns %d from callPlatformFunction", result );				break;
					case kGPIO_Selector_HeadphoneMute:			debugIOLog (5,  "... kGPIO_Selector_HeadphoneMute returns %d from callPlatformFunction", result );					break;
					case kGPIO_Selector_InputDataMux:			debugIOLog (5,  "... kGPIO_Selector_InputDataMux returns %d from callPlatformFunction", result );					break;
					case kGPIO_Selector_InternalSpeakerID:		debugIOLog (5,  "... kGPIO_Selector_InternalSpeakerID returns %d from callPlatformFunction", result );				break;
					case kGPIO_Selector_LineInDetect:			debugIOLog (5,  "... kGPIO_Selector_LineInDetect returns %d from callPlatformFunction", result );					break;
					case kGPIO_Selector_LineOutDetect:			debugIOLog (5,  "... kGPIO_Selector_LineOutDetect returns %d from callPlatformFunction", result );					break;
					case kGPIO_Selector_LineOutMute:			debugIOLog (5,  "... kGPIO_Selector_LineOutMute returns %d from callPlatformFunction", result );					break;
					case kGPIO_Selector_SpeakerDetect:			debugIOLog (5,  "... kGPIO_Selector_SpeakerDetect returns %d from callPlatformFunction", result );					break;
					case kGPIO_Selector_SpeakerMute:			debugIOLog (5,  "... kGPIO_Selector_SpeakerMute returns %d from callPlatformFunction", result );					break;
					case kGPIO_Selector_ExternalMicDetect:																															break;
					case kGPIO_Selector_NotAssociated:																																break;
				}
			}
		} else {
			result = GetCachedAttribute ( selector, result );
		}
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::writeGpioState ( GPIOSelector selector, GpioAttributes gpioState ) {
	UInt32				value;
	IOReturn			result;
	const OSSymbol*		funcSymbolName;

	result = kIOReturnError;
	value = 0;
	if ( mK2Service ) {
		//	Translate GPIO attribute to gpio state
		switch ( selector ) {
			case kGPIO_Selector_AnalogCodecReset:		FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Reset, gpioState, &value ), FailExit );	break;
			case kGPIO_Selector_ClockMux:				FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Mux, gpioState, &value ), FailExit );	break;
			case kGPIO_Selector_DigitalCodecReset:		FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Reset, gpioState, &value ), FailExit );	break;
			case kGPIO_Selector_HeadphoneMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, gpioState, &value ), FailExit );	break;
			case kGPIO_Selector_InputDataMux:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Mux, gpioState, &value ), FailExit );	break;
			case kGPIO_Selector_LineOutMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, gpioState, &value ), FailExit );	break;
			case kGPIO_Selector_SpeakerMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteL, gpioState, &value ), FailExit );	break;
			default:									FailIf ( true, FailExit );																							break;
		}
		switch ( selector ) {
			case kGPIO_Selector_AnalogCodecReset:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetAudioHwReset, mI2SPHandle );					break;
			case kGPIO_Selector_ClockMux:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetCodecClockMux, mI2SPHandle );				break;
			case kGPIO_Selector_DigitalCodecReset:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetAudioDigHwReset, mI2SPHandle );				break;
			case kGPIO_Selector_HeadphoneMute:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetHeadphoneMute, mI2SPHandle );				break;
			case kGPIO_Selector_InputDataMux:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetCodecInputDataMux, mI2SPHandle );			break;
			case kGPIO_Selector_LineOutMute:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetLineOutMute, mI2SPHandle );					break;
			case kGPIO_Selector_SpeakerMute:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetAmpMute, mI2SPHandle );						break;
			default:												funcSymbolName = NULL;
				break;
		}
		if ( NULL != funcSymbolName ) {
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void*)value, (void*)0, (void*)0, (void*)0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, FailExit );
			//	Cache the gpio state in case the gpio does not have a platform function supporting read access
			switch ( selector ) {
				case kGPIO_Selector_AnalogCodecReset:	mAppleGPIO_AnalogCodecReset = gpioState;															break;
				case kGPIO_Selector_ClockMux:			mAppleGPIO_CodecClockMux = gpioState;																break;
				case kGPIO_Selector_DigitalCodecReset:	mAppleGPIO_DigitalCodecReset = gpioState;															break;
				case kGPIO_Selector_HeadphoneMute:		mAppleGPIO_HeadphoneMute = gpioState;																break;
				case kGPIO_Selector_InputDataMux:		mAppleGPIO_CodecInputDataMux = gpioState;															break;
				case kGPIO_Selector_LineOutMute:		mAppleGPIO_LineOutMute = gpioState;																	break;
				case kGPIO_Selector_SpeakerMute:		mAppleGPIO_AmpMute = gpioState;																		break;
				default:																																	break;
			}
			switch ( selector ) {
				case kGPIO_Selector_AnalogCodecReset:	debugIOLog (5,  "... mAppleGPIO_AnalogCodecReset CACHE updated to %d", gpioState );					break;
				case kGPIO_Selector_ClockMux:			debugIOLog (5,  "... mAppleGPIO_CodecClockMux CACHE updated to %d", gpioState );					break;
				case kGPIO_Selector_DigitalCodecReset:	debugIOLog (5,  "... mAppleGPIO_DigitalCodecReset CACHE updated to %d", gpioState );				break;
				case kGPIO_Selector_HeadphoneMute:		debugIOLog (5,  "... mAppleGPIO_HeadphoneMute CACHE updated to %d", gpioState );					break;
				case kGPIO_Selector_InputDataMux:		debugIOLog (5,  "... mAppleGPIO_CodecInputDataMux CACHE updated to %d", gpioState );				break;
				case kGPIO_Selector_LineOutMute:		debugIOLog (5,  "... mAppleGPIO_LineOutMute CACHE updated to %d", gpioState );						break;
				case kGPIO_Selector_SpeakerMute:		debugIOLog (5,  "... mAppleGPIO_AmpMute CACHE updated to %d", gpioState );							break;
				default:																																	break;
			}
		}
	}
Exit:
	return result;
FailExit:
	switch ( selector ) {
		case kGPIO_Selector_AnalogCodecReset:		FailIf ( true, Exit );			break;
		case kGPIO_Selector_ClockMux:				FailIf ( true, Exit );			break;
		case kGPIO_Selector_DigitalCodecReset:		FailIf ( true, Exit );			break;
		case kGPIO_Selector_HeadphoneMute:			FailIf ( true, Exit );			break;
		case kGPIO_Selector_InputDataMux:			FailIf ( true, Exit );			break;
		case kGPIO_Selector_LineOutMute:			FailIf ( true, Exit );			break;
		case kGPIO_Selector_SpeakerMute:			FailIf ( true, Exit );			break;
		default:									FailIf ( true, Exit );			break;   
	}
	return result;
}

//	--------------------------------------------------------------------------------
//	Convert a GPIOAttribute to a binary value to be applied to a GPIO pin level
IOReturn K2Platform::translateGpioAttributeToGpioState ( GPIOType gpioType, GpioAttributes gpioAttribute, UInt32 * valuePtr ) {
	IOReturn		result;
	
	result = kIOReturnBadArgument;
	if ( NULL != valuePtr ) {
		result = kIOReturnSuccess;
		switch ( gpioType ) {
			case kGPIO_Type_MuteL:
				if ( kGPIO_Muted == gpioAttribute ) {
					*valuePtr = 1;
				} else if ( kGPIO_Unmuted == gpioAttribute ) {
					*valuePtr = 0;
				} else {
					result = kIOReturnBadArgument;
				}
				break;
			case kGPIO_Type_MuteH:
				if ( kGPIO_Muted == gpioAttribute ) {
					*valuePtr = 1;
				} else if ( kGPIO_Unmuted == gpioAttribute ) {
					*valuePtr = 0;
				} else {
					result = kIOReturnBadArgument;
				}
				break;
			case kGPIO_Type_Mux:
				if ( kGPIO_MuxSelectAlternate == gpioAttribute ) {
					*valuePtr = 1;
				} else if ( kGPIO_MuxSelectDefault == gpioAttribute ) {
					*valuePtr = 0;
				} else {
					result = kIOReturnBadArgument;
				}
				break;
			case kGPIO_Type_Reset:
				if ( kGPIO_Run == gpioAttribute ) {
					*valuePtr = 0;
				} else if ( kGPIO_Reset == gpioAttribute ) {
					*valuePtr = 1;
				} else {
					result = kIOReturnBadArgument;
				}
				break;
			default:
				result = kIOReturnBadArgument;
				break;
		}
	}
	if ( kIOReturnSuccess != result ) { 
		switch ( gpioType ) {
			case kGPIO_Type_ConnectorType:	debugIOLog (5,  "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_ConnectorType, %p )", valuePtr ); 	break;
			case kGPIO_Type_Detect:			debugIOLog (5,  "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_Detect, %p )", valuePtr ); 			break;
			case kGPIO_Type_Irq:			debugIOLog (5,  "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_Irq, %p )", valuePtr ); 				break;
			case kGPIO_Type_MuteL:			debugIOLog (5,  "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_MuteL, %p )", valuePtr ); 			break;
			case kGPIO_Type_MuteH:			debugIOLog (5,  "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, %p )", valuePtr ); 			break;
			case kGPIO_Type_Mux:			debugIOLog (5,  "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_Mux, %p )", valuePtr ); 				break;
			case kGPIO_Type_Reset:			debugIOLog (5,  "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_Reset, %p )", valuePtr ); 			break;
			default:						debugIOLog (5,  "FAIL: K2Platform::translateGpioAttributeToGpioState ( %X, %p )", gpioType, valuePtr ); 				break;
		}
	}
	return result;
}

#pragma mark ---------------------------
#pragma mark DBDMA Memory Address Acquisition Methods
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	K2Platform::GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) {
	IOMemoryMap *				map;
	IOService *				parentOfParent;
	IOPhysicalAddress			ioPhysAddr;
	IOMemoryDescriptor *			theDescriptor;
	IOByteCount				length = 256;
	
	mIOBaseDMA[kDMAInputIndex] = NULL;
	FailIf ( NULL == dbdmaProvider, Exit );
	debugIOLog (7,  "K2Platform::GetInputChannelRegistersVirtualAddress i2s-a name is %s", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	debugIOLog (7,  "   parentOfParent name is %s", parentOfParent->getName() );
	map = parentOfParent->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAOutputIndex );
	FailIf ( NULL == map, Exit );
	ioPhysAddr = map->getPhysicalSegment( kIODMAInputOffset, &length );
	FailIf ( NULL == ioPhysAddr, Exit );
	theDescriptor = IOMemoryDescriptor::withPhysicalAddress ( ioPhysAddr, 256, kIODirectionOutIn );
	FailIf ( NULL == theDescriptor, Exit );
	map = theDescriptor->map ();
	FailIf ( NULL == map, Exit );
	mIOBaseDMA[kDMAInputIndex] = (IODBDMAChannelRegisters*)map->getVirtualAddress();
	
	debugIOLog (7,  "mIOBaseDMA[kDMAInputIndex] %p", mIOBaseDMA[kDMAInputIndex] );
	if ( NULL == mIOBaseDMA[kDMAInputIndex] ) { debugIOLog (1,  "K2Platform::GetInputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE" ); }
Exit:
	return mIOBaseDMA[kDMAInputIndex];
}

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	K2Platform::GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) {
	IOMemoryMap *				map;
	IOService *				parentOfParent;

	mIOBaseDMA[kDMAOutputIndex] = NULL;
	FailIf ( NULL == dbdmaProvider, Exit );
	debugIOLog (7,  "K2Platform::GetOutputChannelRegistersVirtualAddress i2s-a name is %s", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	debugIOLog (7,  "   parentOfParent name is %s", parentOfParent->getName() );
	map = parentOfParent->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAOutputIndex );
	FailIf ( NULL == map, Exit );
	mIOBaseDMA[kDMAOutputIndex] = (IODBDMAChannelRegisters*)map->getVirtualAddress();
	
	debugIOLog (7,  "mIOBaseDMA[kDMAOutputIndex] %p is at physical %p", mIOBaseDMA[kDMAOutputIndex], (void*)map->getPhysicalAddress() );
	if ( NULL == mIOBaseDMA[kDMAOutputIndex] ) { debugIOLog (1,  "K2Platform::GetOutputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE" ); }
Exit:
	return mIOBaseDMA[kDMAOutputIndex];
}

#pragma mark ---------------------------
#pragma mark USER CLIENT SUPPORT
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IOReturn	K2Platform::getPlatformState ( PlatformStateStructPtr outState ) {
	IOReturn			result = kIOReturnBadArgument;
	
	FailIf ( NULL == outState, Exit );
	outState->platformType = getPlatformInterfaceType ();
	
	outState->i2s.intCtrl = getI2SIOMIntControl ();
	outState->i2s.serialFmt = getSerialFormatRegister ();
	outState->i2s.codecMsgOut = 0;
	outState->i2s.codecMsgIn = 0;
	outState->i2s.frameCount = getFrameCount ();
	outState->i2s.frameCountToMatch = 0;
	outState->i2s.dataWordSizes = getDataWordSizes ();
	outState->i2s.peakLevelSfSel = 0;
	outState->i2s.peakLevelIn0 = 0;
	outState->i2s.peakLevelIn1 = 0;
	
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
	outState->gpio.gpio_LineOutDetect = getLineOutConnected (true);
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

	debugIOLog (5,  "outState->gpio: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d ",
				outState->gpio.gpio_AnalogCodecReset,
				outState->gpio.gpio_ClockMux,
				outState->gpio.gpio_CodecInterrupt,
				outState->gpio.gpio_CodecErrorInterrupt,
				outState->gpio.gpio_ComboInJackType,
				outState->gpio.gpio_ComboOutJackType,
				outState->gpio.gpio_DigitalCodecReset,
				outState->gpio.gpio_DigitalInDetect,
				outState->gpio.gpio_DigitalOutDetect,
				outState->gpio.gpio_HeadphoneDetect,
				outState->gpio.gpio_HeadphoneMute,
				outState->gpio.gpio_InputDataMux,
				outState->gpio.gpio_InternalSpeakerID,
				outState->gpio.gpio_LineInDetect,
				outState->gpio.gpio_LineOutDetect,
				outState->gpio.gpio_LineOutMute,
				outState->gpio.gpio_SpeakerMute
			);

	outState->i2c.i2c_pollingMode = (UInt32)false;
	outState->i2c.i2c_errorStatus = mI2C_lastTransactionResult;
	
	result = kIOReturnSuccess;
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn	K2Platform::setPlatformState ( PlatformStateStructPtr inState ) {
	IOReturn			result = kIOReturnBadArgument;
	
	FailIf ( NULL == inState, Exit );
	if ( inState->i2s.intCtrl != getI2SIOMIntControl () ) {
		debugIOLog (7,  "K2Platform::setPlatformState setI2SIOMIntControl ( %lX )", inState->i2s.intCtrl );
		result = setI2SIOMIntControl ( inState->i2s.intCtrl );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->i2s.serialFmt != getSerialFormatRegister () ) {
		debugIOLog (7,  "K2Platform::setPlatformState setSerialFormatRegister ( %lX )", inState->i2s.serialFmt );
		result = setSerialFormatRegister ( inState->i2s.serialFmt );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( inState->i2s.frameCount != getFrameCount () ) && ( 0 == inState->i2s.frameCount ) ) {
		debugIOLog (7,  "K2Platform::setPlatformState setFrameCount ( %lX )", inState->i2s.frameCount );
		result = setFrameCount ( inState->i2s.frameCount );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->i2s.dataWordSizes != getDataWordSizes () ) {
		debugIOLog (7,  "K2Platform::setPlatformState setDataWordSizes ( %lX )", inState->i2s.dataWordSizes );
		result = setDataWordSizes ( inState->i2s.dataWordSizes );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	
	if ( inState->fcr.i2sEnable != getI2SEnable () ) {
		debugIOLog (7,  "K2Platform::setPlatformState setI2SEnable ( %lX )", inState->fcr.i2sEnable );
		result = setI2SEnable ( inState->fcr.i2sEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->fcr.i2sClockEnable != getI2SClockEnable () ) {
		debugIOLog (7,  "K2Platform::setPlatformState setI2SClockEnable ( %lX )", inState->fcr.i2sClockEnable );
		result = setI2SClockEnable ( inState->fcr.i2sClockEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->fcr.i2sCellEnable != getI2SCellEnable () ) {
		debugIOLog (7,  "K2Platform::setPlatformState setI2SCellEnable ( %lX )", inState->fcr.i2sCellEnable );
		result = setI2SCellEnable ( inState->fcr.i2sCellEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	
	if ( ( kGPIO_Unknown != inState->gpio.gpio_AnalogCodecReset ) && ( inState->gpio.gpio_DigitalCodecReset != getCodecReset ( kCODEC_RESET_Analog ) ) ) {
		debugIOLog (7,  "K2Platform::setPlatformState setCodecReset ( kCODEC_RESET_Analog, %d )", inState->gpio.gpio_DigitalCodecReset );
		result = setCodecReset ( kCODEC_RESET_Analog, inState->gpio.gpio_AnalogCodecReset );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_ClockMux ) && ( inState->gpio.gpio_ClockMux != getClockMux () ) ) {
		debugIOLog (7,  "K2Platform::setPlatformState setClockMux ( %d )", inState->gpio.gpio_ClockMux );
		result = setClockMux ( inState->gpio.gpio_ClockMux );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_DigitalCodecReset ) && ( inState->gpio.gpio_DigitalCodecReset != getCodecReset ( kCODEC_RESET_Digital ) ) ) {
		debugIOLog (7,  "K2Platform::setPlatformState setCodecReset ( kCODEC_RESET_Digital, %d )", inState->gpio.gpio_DigitalCodecReset );
		result = setCodecReset ( kCODEC_RESET_Digital, inState->gpio.gpio_DigitalCodecReset );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_HeadphoneMute ) && ( inState->gpio.gpio_HeadphoneMute != getHeadphoneMuteState () ) ) {
		debugIOLog (7,  "K2Platform::setPlatformState setHeadphoneMuteState ( %d )", inState->gpio.gpio_HeadphoneMute );
		result = setHeadphoneMuteState ( inState->gpio.gpio_HeadphoneMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_InputDataMux ) && ( inState->gpio.gpio_InputDataMux != getInputDataMux () ) ) {
		debugIOLog (7,  "K2Platform::setPlatformState setInputDataMux ( %d )", inState->gpio.gpio_InputDataMux );
		result = setInputDataMux ( inState->gpio.gpio_InputDataMux );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_LineInDetect ) && ( inState->gpio.gpio_LineInDetect != getLineInConnected () ) ) {
		debugIOLog (2,  "K2Platform::setPlatformState TRIGGER LINE IN INTERRUPT ( %d )", inState->gpio.gpio_LineOutMute );
		lineInDetectInterruptHandler ( this, 0, 0, 0 );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_LineOutMute ) && ( inState->gpio.gpio_LineOutMute != getLineOutMuteState () ) ) {
		debugIOLog (7,  "K2Platform::setPlatformState setLineOutMuteState ( %d )", inState->gpio.gpio_LineOutMute );
		result = setLineOutMuteState ( inState->gpio.gpio_LineOutMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_SpeakerMute ) && ( inState->gpio.gpio_SpeakerMute != getSpeakerMuteState () ) ) {
		debugIOLog (7,  "K2Platform::setPlatformState setSpeakerMuteState ( %d )", inState->gpio.gpio_SpeakerMute );
		result = setSpeakerMuteState ( inState->gpio.gpio_SpeakerMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}







