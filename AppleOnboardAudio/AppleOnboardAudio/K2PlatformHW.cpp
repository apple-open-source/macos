/*
 *  K2PlatformHW.cpp
 *  
 *  Created by Aram Lindahl on Mon Mar 10 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include "K2PlatformHW.h"

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

const char * 	K2HWPlatform::kAppleK2pHandle							= "phK2";
const char * 	K2HWPlatform::kAppleI2S0pHandle							= "phI0";
const char * 	K2HWPlatform::kAppleGPIOpHandle							= "phGn";

//	---------------------------
//	FCR1
//	---------------------------

const char * 	K2HWPlatform::kAppleI2S_Enable 							= "platform-enable";					//	
const char * 	K2HWPlatform::kAppleI2S_Disable							= "platform-disable";					//	
const char * 	K2HWPlatform::kAppleI2S_ClockEnable						= "platform-clock-enable";				//	
const char * 	K2HWPlatform::kAppleI2S_ClockDisable					= "platform-clock-disable";				//	
const char * 	K2HWPlatform::kAppleI2S_Reset							= "platform-sw-reset";					//	
const char * 	K2HWPlatform::kAppleI2S_Run								= "platform-clear-sw-reset";			//	
const char * 	K2HWPlatform::kAppleI2S_CellEnable						= "platform-cell-enable";				//	
const char * 	K2HWPlatform::kAppleI2S_CellDisable						= "platform-cell-disable";				//	
const char * 	K2HWPlatform::kAppleI2S_GetEnable						= "platform-get-enable";				//	
const char * 	K2HWPlatform::kAppleI2S_GetClockEnable					= "platform-get-clock-enable";			//	
const char * 	K2HWPlatform::kAppleI2S_GetReset						= "platform-get-sw-reset";				//	
const char * 	K2HWPlatform::kAppleI2S_GetCellEnable					= "platform-get-cell-enable";			//	

//	---------------------------
//	GPIO
//	---------------------------

const char * 	K2HWPlatform::kAppleGPIO_SetAmpMute							= "platform-amp-mute";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_GetAmpMute							= "platform-rd-amp-mute";				//	

const char * 	K2HWPlatform::kAppleGPIO_SetAudioHwReset					= "platform-hw-reset";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_GetAudioHwReset					= "platform-rd-hw-reset";				//		

const char * 	K2HWPlatform::kAppleGPIO_SetCodecClockMux					= "platform-codec-clock-mux";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_GetCodecClockMux					= "platform-rd-clock-mux";				//	

const char * 	K2HWPlatform::kAppleGPIO_EnableCodecErrorIRQ				= "enable-codec-error-irq";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_DisableCodecErrorIRQ				= "disable-codec-error-irq";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_GetCodecErrorIRQ					= "platform-codec-error-irq";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_RegisterCodecErrorIRQ				= "register-codec-error-irq";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_UnregisterCodecErrorIRQ			= "unregister-codec-error-irq";			//	[IN Q37 IOService:...:IOResources]

const char * 	K2HWPlatform::kAppleGPIO_DisableCodecIRQ					= "disable-codec-irq";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_EnableCodecIRQ						= "enable-codec-irq";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_GetCodecIRQ						= "platform-codec-irq";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_RegisterCodecIRQ					= "register-codec-irq";					//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_UnregisterCodecIRQ					= "unregister-codec-irq";				//	[IN Q37 IOService:...:IOResources]

const char * 	K2HWPlatform::kAppleGPIO_SetCodecInputDataMux				= "platform-codec-input-data-mu";		//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_GetCodecInputDataMux				= "get-codec-input-data-mux";			//	

const char * 	K2HWPlatform::kAppleGPIO_GetComboInJackType					= "combo-in-type";		

const char * 	K2HWPlatform::kAppleGPIO_GetComboOutJackType				= "combo-out-type";

const char * 	K2HWPlatform::kAppleGPIO_SetAudioDigHwReset					= "platform-dig-hw-reset";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_GetAudioDigHwReset					= "get-dig-hw-reset";					//	

const char * 	K2HWPlatform::kAppleGPIO_DisableDigitalInDetect				= "disable-audio-dig-in-det";			//	
const char * 	K2HWPlatform::kAppleGPIO_EnableDigitalInDetect				= "enable-audio-dig-in-det";			//	
const char * 	K2HWPlatform::kAppleGPIO_GetDigitalInDetect					= "platform-audio-dig-in-det";		
const char * 	K2HWPlatform::kAppleGPIO_RegisterDigitalInDetect			= "register-audio-dig-in-det";			//	
const char * 	K2HWPlatform::kAppleGPIO_UnregisterDigitalInDetect			= "unregister-audio-dig-in-det";		//	

const char * 	K2HWPlatform::kAppleGPIO_DisableDigitalOutDetect			= "disable-audio-dig-out-detect";		//	
const char * 	K2HWPlatform::kAppleGPIO_EnableDigitalOutDetect				= "enable-audio-dig-out-detect";		//	
const char * 	K2HWPlatform::kAppleGPIO_GetDigitalOutDetect				= "platform-audio-dig-out-det";	
const char * 	K2HWPlatform::kAppleGPIO_RegisterDigitalOutDetect			= "register-audio-dig-out-detect";		//	
const char * 	K2HWPlatform::kAppleGPIO_UnregisterDigitalOutDetect			= "unregister-audio-dig-out-detect";	//	
	
const char * 	K2HWPlatform::kAppleGPIO_DisableHeadphoneDetect				= "disable-headphone-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_EnableHeadphoneDetect				= "enable-headphone-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_GetHeadphoneDetect					= "platform-headphone-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_RegisterHeadphoneDetect			= "register-headphone-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_UnregisterHeadphoneDetect			= "unregister-headphone-detect";		//	[IN Q37 IOService:...:IOResources]

const char * 	K2HWPlatform::kAppleGPIO_SetHeadphoneMute					= "platform-headphone-mute";			//	
const char * 	K2HWPlatform::kAppleGPIO_GetHeadphoneMute					= "platform-rd-headphone-mute";			//	

const char *	K2HWPlatform::kAppleGPIO_GetInternalSpeakerID				= "internal-speaker-id";

const char * 	K2HWPlatform::kAppleGPIO_DisableLineInDetect				= "disable-linein-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_EnableLineInDetect					= "enable-linein-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_GetLineInDetect					= "platform-linein-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_RegisterLineInDetect				= "register-linein-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_UnregisterLineInDetect				= "unregister-linein-detect";			//	[IN Q37 IOService:...:IOResources]

const char *	K2HWPlatform::kAppleGPIO_DisableLineOutDetect				= "disable-lineout-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_EnableLineOutDetect				= "enable-lineout-detect";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_GetLineOutDetect					= "platform-lineout-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_RegisterLineOutDetect				= "register-lineout-detect";			//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_UnregisterLineOutDetect			= "unregister-lineout-detect";			//	[IN Q37 IOService:...:IOResources]

const char * 	K2HWPlatform::kAppleGPIO_SetLineOutMute						= "platform-lineout-mute";				//	[IN Q37 IOService:...:IOResources]
const char * 	K2HWPlatform::kAppleGPIO_GetLineOutMute						= "platform-rd-lineout-mute";			//	

const char * 	K2HWPlatform::kAppleGPIO_DisableSpeakerDetect				= "disable-audio-spkr-detect";			//	
const char * 	K2HWPlatform::kAppleGPIO_EnableSpeakerDetect				= "enable-audio-spkr-detect";			//	
const char * 	K2HWPlatform::kAppleGPIO_GetSpeakerDetect					= "platform-audio-spkr-detect";			//
const char * 	K2HWPlatform::kAppleGPIO_RegisterSpeakerDetect				= "register-audio-spkr-detect";			//	
const char * 	K2HWPlatform::kAppleGPIO_UnregisterSpeakerDetect			= "unregister-audio-spkr-detect";		//	

//	--------------------------------------------------------------------------------
bool K2HWPlatform::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex) {
	IORegistryEntry			*sound;
	bool					result;
	OSData					*osdata;
	IORegistryEntry			*i2S;
	IORegistryEntry			*macIO;
	
	debugIOLog (3,  "+ K2HWPlatform::init" );
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
	debugIOLog (3, "K2PlatformHW - i2s's name is %s", mI2S->getName ());

	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "AAPL,phandle" ) );
	mI2SPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debugIOLog (3,  "mI2SPHandle %lX", mI2SPHandle );
	
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
	if ( !result ) { debugIOLog (3,  "K2HWPlatform::init COULD NOT FIND I2C" ); }
	FailIf ( !result, Exit );

	IOMemoryMap *				map;
	IOMemoryDescriptor *		theDescriptor;

	theDescriptor = IOMemoryDescriptor::withPhysicalAddress ( (IOPhysicalAddress)kAUDIO_MAC_IO_BASE_ADDRESS, kAUDIO_MAC_IO_SIZE, kIODirectionOutIn );
	FailIf ( NULL == theDescriptor, Exit );
	map = theDescriptor->map ();
	FailIf ( NULL == map, Exit );
	mHwPtr = (UInt8*)map->getVirtualAddress();
	FailIf ( NULL == mHwPtr, Exit );
	
	theDescriptor = IOMemoryDescriptor::withPhysicalAddress ( (IOPhysicalAddress)kAUDIO_I2S_BASE_ADDRESS, kAUDIO_I2S_SIZE, kIODirectionOutIn );
	FailIf ( NULL == theDescriptor, Exit );
	map = theDescriptor->map ();
	FailIf ( NULL == map, Exit );
	mHwI2SPtr = (UInt8*)map->getVirtualAddress();
	FailIf ( NULL == mHwI2SPtr, Exit );
	
	mFcr1						= (UInt32*)&mHwPtr[kAUDIO_MAC_IO_FCR1];
	mFcr3						= (UInt32*)&mHwPtr[kAUDIO_MAC_IO_FCR3];
	mSerialFormat				= (UInt32*)&mHwI2SPtr[kAUDIO_I2S_SERIAL_FORMAT];
	mI2SIntCtrl					= (UInt32*)&mHwI2SPtr[kAUDIO_I2S_INTERRUPT_CONTROL];
	mDataWordSize				= (UInt32*)&mHwI2SPtr[kAUDIO_I2S_DATA_WORD_SIZES];
	mFrameCounter				= (UInt32*)&mHwI2SPtr[kAUDIO_I2S_FRAME_COUNTER];
	mGPIO_inputDataMuxSelect	= (UInt8*)&mHwPtr[kAUDIO_GPIO_INPUT_DATA_MUX_SELECT];
	mGPIO_lineInSense			= (UInt8*)&mHwPtr[kAUDIO_GPIO_LINE_IN_SENSE];
	mGPIO_digitalCodecErrorIrq	= (UInt8*)&mHwPtr[kAUDIO_GPIO_CODEC_ERROR_IRQ];
	mGPIO_digitalCodecReset		= (UInt8*)&mHwPtr[kAUDIO_GPIO_DIGITAL_CODEC_RESET];
	mGPIO_lineOutSense			= (UInt8*)&mHwPtr[kAUDIO_GPIO_LINE_OUT_SENSE];
	mGPIO_headphoneSense		= (UInt8*)&mHwPtr[kAUDIO_GPIO_HEADPHONE_SENSE];
	mGPIO_digitalCodecIrq		= (UInt8*)&mHwPtr[kAUDIO_GPIO_CODEC_IRQ];
	mGPIO_headphoneMute			= (UInt8*)&mHwPtr[kAUDIO_GPIO_HEADPHONE_MUTE];
	mGPIO_analogCodecReset		= (UInt8*)&mHwPtr[kAUDIO_GPIO_ANALOG_CODEC_RESET];
	mGPIO_lineOutMute			= (UInt8*)&mHwPtr[kAUDIO_GPIO_LINE_OUT_MUTE];
	mGPIO_mclkMuxSelect			= (UInt8*)&mHwPtr[kAUDIO_GPIO_CLOCK_MUX_SELECT];
	mGPIO_speakerMute			= (UInt8*)&mHwPtr[kAUDIO_GPIO_AMPLIFIER_MUTE];
	
Exit:

	debugIOLog (3,  "- K2HWPlatform::init returns %d", result );
	return result;
}

//	--------------------------------------------------------------------------------
void K2HWPlatform::free()
{
	debugIOLog (3, "+ K2HWPlatform::free()");

	detachFromI2C();
	//detachFromI2S();

	super::free();

	debugIOLog (3, "- K2HWPlatform::free()");
}

#pragma mark ---------------------------
#pragma mark Codec Methods	
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
bool K2HWPlatform::writeCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode) {
	bool success = false;

	debugIOLog (5,  "+ K2HWPlatform::writeCodecRegister ( %X, %X, %p, %d, %d )", address, subAddress, data, len, mode );

	FailIf ( NULL == data, Exit );
	if (openI2C()) {
		switch (mode) {
			case kI2C_StandardMode:			mI2CInterface->setStandardMode();			break;
			case kI2C_StandardSubMode:		mI2CInterface->setStandardSubMode();		break;
			case kI2C_CombinedMode:			mI2CInterface->setCombinedMode();			break;
			default:
				debugIOLog (7, "K2HWPlatform::writeCodecRegister() unknown bus mode!");
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
		debugIOLog (5,  " mI2CInterface->writeI2CBus ( %X, %X, %p, %d ), data->%X", (unsigned int)address, (unsigned int)subAddress, data, (unsigned int)len, *data );

		success = mI2CInterface->writeI2CBus (address >> 1, subAddress, data, len);
		mI2C_lastTransactionResult = success;
		
		if (!success) { 
			debugIOLog (7,  "K2HWPlatform::writeCodecRegister( %X, %X, %p %d), mI2CInterface->writeI2CBus returned false.", address, subAddress, data, len );
		}
Exit:
		closeI2C();
	} else {
		debugIOLog (7, "K2HWPlatform::writeCodecRegister() couldn't open the I2C bus!");
	}

	debugIOLog (5,  "- K2HWPlatform::writeCodecRegister ( %X, %X, %p, %d, %d ) returns %d", address, subAddress, data, len, mode, success );

	return success;
}

//	--------------------------------------------------------------------------------
bool K2HWPlatform::readCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode) {
	bool success = false;

	debugIOLog (5,  "+ K2HWPlatform::readCodecRegister ( %X, %X, %p, %d, %d )", address, subAddress, data, len, mode );

	if (openI2C()) {
		switch (mode) {
			case kI2C_StandardMode:			mI2CInterface->setStandardMode();			break;
			case kI2C_StandardSubMode:		mI2CInterface->setStandardSubMode();		break;
			case kI2C_CombinedMode:			mI2CInterface->setCombinedMode();			break;
			default:
				debugIOLog (7, "K2HWPlatform::readCodecRegister() unknown bus mode!");
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

		if (!success) debugIOLog (7, "K2HWPlatform::readCodecRegister(), mI2CInterface->writeI2CBus returned false.");
Exit:
		closeI2C();
	} else {
		debugIOLog (7, "K2HWPlatform::readCodecRegister() couldn't open the I2C bus!");
	}
	debugIOLog (5,  "- K2HWPlatform::readCodecRegister ( %X, %X, %p, %d, %d ) returns %d", address, subAddress, data, len, mode, success );

	return success;
}

//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::setCodecReset ( CODEC_RESET target, GpioAttributes reset ) {
	IOReturn				result;

	debugIOLog (5,  "K2HWPlatform::setCodecReset ( %d, %d )", (unsigned int)target, (unsigned int)reset );

	switch ( target ) {
		case kCODEC_RESET_Analog:	
			result = writeGpioState ( kGPIO_Selector_AnalogCodecReset, reset );		
			debugIOLog (7,  "setCodecReset() for kGPIO_Selector_AnalogCodecReset returns %X after reset", getCodecReset(target) );
			break;
		case kCODEC_RESET_Digital:	
			result = writeGpioState ( kGPIO_Selector_DigitalCodecReset, reset );	
			debugIOLog (7,  "setCodecReset() for kGPIO_Selector_DigitalCodecReset returns %X after reset", getCodecReset(target) );
			break;
		default:					result = kIOReturnBadArgument;											break;
	}
	if ( kIOReturnSuccess != result ) {
		debugIOLog (7,  "- K2HWPlatform::setCodecReset ( %d, %d ) returns %X", (unsigned int)target, (unsigned int)reset, (unsigned int)result );
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes K2HWPlatform::getCodecReset ( CODEC_RESET target ) {
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
IOReturn K2HWPlatform::setI2SEnable (bool enable) {
	IOReturn				result;

	debugIOLog (5,  "+ K2HWPlatform::setI2SEnable enable=%d", enable );

	result = kIOReturnError;
	UInt32			data;
	
	if ( mFcr1 ) {
		if ( enable ) {
			debugIOLog (7,  "K2HWPlatform::setI2SEnable to 1" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data |= ( 1 << 13 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		} else {
			debugIOLog (7,  "K2HWPlatform::setI2SEnable to 0" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data &= ~( 1 << 13 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		}
		result = kIOReturnSuccess;
	}
	debugIOLog (5,  "- K2HWPlatform::setI2SEnable enable=%d returns %d", enable, result );

	return result;
}

//	--------------------------------------------------------------------------------
bool K2HWPlatform::getI2SEnable () {
	UInt32					value = 0;

	if ( NULL != mFcr1 ) {
		value = ( (unsigned int)OSReadSwapInt32 ( mFcr1, 0 ) >> 13 ) & 0x00000001;
	}
	return ( 0 != value );
}

//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::setI2SClockEnable (bool enable) {
	IOReturn				result;

	debugIOLog (5,  "+ K2HWPlatform::setI2SClockEnable enable=%d", enable );

	result = kIOReturnError;
	UInt32			data;
	
	if ( mFcr1 ) {
		if ( enable ) {
			debugIOLog (7,  "K2HWPlatform::setI2SClockEnable to 1" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data |= ( 1 << 12 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		} else {
			debugIOLog (7,  "K2HWPlatform::setI2SClockEnable to 0" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data &= ~( 1 << 12 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		}
		result = kIOReturnSuccess;
	}
	debugIOLog (5,  "- K2HWPlatform::setI2SClockEnable enable=%d returns %d", enable, result );

	return result;
}

//	--------------------------------------------------------------------------------
bool K2HWPlatform::getI2SClockEnable () {
	UInt32					value = 0;

	if ( NULL != mFcr1 ) {
		value = ( (unsigned int)OSReadSwapInt32 ( mFcr1, 0 ) >> 12 ) & 0x00000001;
	}
	return ( 0 != value );
}

//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::setI2SCellEnable (bool enable) {
	IOReturn				result = kIOReturnError;
	
	debugIOLog (5, "+ K2HWPlatform::setI2SCellEnable enable=%d",enable);
	
	UInt32			data;
	
	if ( mFcr1 ) {
		if ( enable ) {
			debugIOLog (7,  "K2HWPlatform::setI2SCellEnable to 1" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data |= ( 1 << 10 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		} else {
			debugIOLog (7,  "K2HWPlatform::setI2SCellEnable to 0" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data &= ~( 1 << 10 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		}
		result = kIOReturnSuccess;
	}
	debugIOLog (5, "- K2HWPlatform::setI2SCellEnable result = %x",result);

	return result;
}

//	--------------------------------------------------------------------------------
bool K2HWPlatform::getI2SCellEnable () {
	UInt32					value = 0;

	if ( NULL != mFcr1 ) {
		value = ( (unsigned int)OSReadSwapInt32 ( mFcr1, 0 ) >> 10 ) & 0x00000001;
	}
	return ( 0 != value );
}

//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::setI2SSWReset(bool enable) {
	IOReturn				result = kIOReturnError;
	
	debugIOLog (5, "+ K2HWPlatform::setI2SSWReset enable=%d",enable);
	
	UInt32			data;
	
	if ( mFcr1 ) {
		if ( enable ) {
			debugIOLog (7,  "K2HWPlatform::setI2SSWReset to 1" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data |= ( 1 << 11 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		} else {
			debugIOLog (7,  "K2HWPlatform::setI2SSWReset to 0" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data &= ~( 1 << 11 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		}
		result = kIOReturnSuccess;
	}
	debugIOLog (5, "- K2HWPlatform::setI2SSWReset result = %x",result);

	return result;
}


//	--------------------------------------------------------------------------------
bool K2HWPlatform::getI2SSWReset() {
	UInt32					value = 0;

	if ( NULL != mFcr1 ) {
		value = ( (unsigned int)OSReadSwapInt32 ( mFcr1, 0 ) >> 11 ) & 0x00000001;
	}
	return ( 0 != value );
}

//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::setSerialFormatRegister (UInt32 serialFormat) {
	IOReturn				result = kIOReturnError;

	if ( mSerialFormat ) {
		OSWriteSwapInt32 ( mSerialFormat, 0, serialFormat );
		result = kIOReturnSuccess;
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2HWPlatform::getSerialFormatRegister () {
	UInt32					serialFormat = 0;

	if ( NULL != mSerialFormat ) serialFormat = (unsigned int)OSReadSwapInt32 ( mSerialFormat, 0 );
	return serialFormat;
}

//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::setDataWordSizes (UInt32 dataWordSizes) {
	IOReturn				result;

	result = kIOReturnError;
	if ( mDataWordSize ) {
		OSWriteSwapInt32 ( mDataWordSize, 0, dataWordSizes );
		result = kIOReturnSuccess;
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2HWPlatform::getDataWordSizes () {
	UInt32					dataWordSizes = 0;

	if ( NULL != mDataWordSize ) dataWordSizes = (unsigned int)OSReadSwapInt32 ( mDataWordSize, 0 );
	return dataWordSizes;
}

//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::setI2SIOMIntControl (UInt32 intCntrl) {
	IOReturn				result;

	result = kIOReturnError;
	if ( mI2SIntCtrl ) {
		OSWriteSwapInt32 ( mI2SIntCtrl, 0, intCntrl );
		result = kIOReturnSuccess;
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2HWPlatform::getI2SIOMIntControl () {
	UInt32					i2sIntCntrl = 0;

	if ( NULL != mI2SIntCtrl ) i2sIntCntrl = (unsigned int)OSReadSwapInt32 ( mI2SIntCtrl, 0 );
	return i2sIntCntrl;
}

//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::setFrameCount (UInt32 value) {
	IOReturn				result;

	//	<<<<<<<<<< WARNING >>>>>>>>>>
	//	Do not debugIOLog in here it screws up the hal timing and causes stuttering audio
	//	<<<<<<<<<< WARNING >>>>>>>>>>
	
	result = kIOReturnError;
	if ( mFrameCounter ) {
		OSWriteSwapInt32 ( mFrameCounter, 0, value );
		result = kIOReturnSuccess;
	}
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2HWPlatform::getFrameCount () {
	UInt32					frameCount = 0;

	if ( NULL != mFrameCounter ) frameCount = (unsigned int)OSReadSwapInt32 ( mFrameCounter, 0 );
	return frameCount;
}

//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::releaseI2SClockSource(I2SClockFrequency inFrequency)	
{
	UInt32						i2sCellNumber;
	IOReturn					result;

	result = kIOReturnError;

	if (kUseI2SCell0 == mI2SInterfaceNumber)
		i2sCellNumber = kUseI2SCell0;
	else if (kUseI2SCell1 == mI2SInterfaceNumber)
		i2sCellNumber = kUseI2SCell1;
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
IOReturn K2HWPlatform::requestI2SClockSource(I2SClockFrequency inFrequency)	
{
	UInt32						i2sCellNumber;
	IOReturn					result;

	result = kIOReturnError;

	if (kUseI2SCell0 == mI2SInterfaceNumber)
		i2sCellNumber = kUseI2SCell0;
	else if (kUseI2SCell1 == mI2SInterfaceNumber)
		i2sCellNumber = kUseI2SCell1;
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
IOReturn K2HWPlatform::setupI2SClockSource( UInt32 cell, bool requestClock, UInt32 clockSource )	
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
IOReturn K2HWPlatform::disableInterrupt ( PlatformInterruptSource source ) {
#pragma unused ( theDevice, interruptHandler, source )
	IOReturn				result = kIOReturnError;

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
IOReturn K2HWPlatform::enableInterrupt ( PlatformInterruptSource source ) {
#pragma unused ( theDevice, interruptHandler, source )
	IOReturn				result = kIOReturnError;

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
void K2HWPlatform::poll ( void ) {
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
		if ( interruptUsesTimerPolling ( kDigitalInDetectInterrupt ) ) {
			if ( mDigitalInDetectInterruptEnable ) {
				cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kDigitalInStatus, (void *)getDigitalInConnected (), (void *)0 );
			}
		}
		if ( interruptUsesTimerPolling ( kDigitalOutDetectInterrupt ) ) {
			if ( mDigitalOutDetectInterruptEnable ) {
				cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kDigitalOutStatus, (void *)getDigitalOutConnected (), (void *)0 );
			}
		}
		if ( interruptUsesTimerPolling ( kHeadphoneDetectInterrupt ) ) {
			if ( mHeadphoneDetectInterruptEnable ) {
				if ( !mIsComboOutJack ) {
					cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kHeadphoneStatus, (void *)getHeadphoneConnected (), (void *)0 );
				} else {
					if ( kGPIO_Connected == getComboOutJackTypeConnected() ) {
						cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kDigitalOutStatus, (void *)getHeadphoneConnected (), (void *)0 );
					} else {
						cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kHeadphoneStatus, (void *)getHeadphoneConnected (), (void *)0 );
					}
				}
			}
		}
		if ( interruptUsesTimerPolling ( kLineInputDetectInterrupt ) ) {
			if ( mLineInputDetectInterruptEnable ) {
				if ( !mIsComboInJack ) {
					cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kLineInStatus, (void *)getLineInConnected (), (void *)0 );
				} else {
					if ( kGPIO_Connected == getComboInJackTypeConnected() ) {
						cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kDigitalInStatus, (void *)getLineInConnected (), (void *)0 );
					} else {
						cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kLineInStatus, (void *)getLineInConnected (), (void *)0 );
					}
				}
			}
		}
		if ( interruptUsesTimerPolling ( kLineOutputDetectInterrupt ) ) {
			if ( mLineOutputDetectInterruptEnable ) {
				cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kLineOutStatus, (void *)getLineOutConnected (), (void *)0 );
			}
		}
		if ( interruptUsesTimerPolling ( kSpeakerDetectInterrupt ) ) {
			if ( mSpeakerDetectInterruptEnable ) {
				cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kExtSpeakersStatus, (void *)getSpeakerConnected (), (void *)0 );
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
bool K2HWPlatform::interruptUsesTimerPolling( PlatformInterruptSource source ) {
#pragma unused ( source )
	return true;
}


//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::registerInterruptHandler ( IOService * theDevice, void * interruptHandler, PlatformInterruptSource source ) {
#pragma unused ( theDevice, interruptHandler, source )
	return kIOReturnError;
}

//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::unregisterInterruptHandler ( IOService * theDevice, void * interruptHandler, PlatformInterruptSource source ) {
#pragma unused ( theDevice, interruptHandler, source )
	return kIOReturnError;
}

#pragma mark ---------------------------
#pragma mark GPIO
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getClockMux() {
	return readGpioState ( kGPIO_Selector_ClockMux );
}

//	--------------------------------------------------------------------------------
IOReturn 	K2HWPlatform::setClockMux ( GpioAttributes muxState ) {
	return writeGpioState ( kGPIO_Selector_ClockMux, muxState );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getCodecErrorInterrupt() {
	return readGpioState ( kGPIO_Selector_CodecErrorInterrupt );
}


//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getCodecInterrupt() {
	return readGpioState ( kGPIO_Selector_CodecInterrupt );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getComboInJackTypeConnected() {
	return readGpioState ( kGPIO_Selector_ComboInJackType );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getComboOutJackTypeConnected() {
	return readGpioState ( kGPIO_Selector_ComboOutJackType );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getDigitalInConnected() {
	return readGpioState ( kGPIO_Selector_DigitalInDetect );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getDigitalOutConnected() {
	return readGpioState ( kGPIO_Selector_DigitalOutDetect );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getHeadphoneConnected() {
	return readGpioState ( kGPIO_Selector_HeadphoneDetect );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getHeadphoneMuteState() {
	return readGpioState ( kGPIO_Selector_HeadphoneMute );
}
	
//	--------------------------------------------------------------------------------
IOReturn 	K2HWPlatform::setHeadphoneMuteState ( GpioAttributes muteState ) {
	return writeGpioState ( kGPIO_Selector_HeadphoneMute, muteState );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getInputDataMux() {
	return readGpioState ( kGPIO_Selector_InputDataMux );
}

//	--------------------------------------------------------------------------------
IOReturn 	K2HWPlatform::setInputDataMux(GpioAttributes muxState) {
	return writeGpioState ( kGPIO_Selector_InputDataMux, muxState );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getLineInConnected() {
	return readGpioState ( kGPIO_Selector_LineInDetect );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getLineOutConnected() {
	return readGpioState ( kGPIO_Selector_LineOutDetect );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getLineOutMuteState() {
	return readGpioState ( kGPIO_Selector_LineOutMute );
}
	
//	--------------------------------------------------------------------------------
IOReturn 	K2HWPlatform::setLineOutMuteState ( GpioAttributes muteState ) {
	return writeGpioState ( kGPIO_Selector_LineOutMute, muteState );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getSpeakerConnected() {
	return readGpioState ( kGPIO_Selector_SpeakerDetect );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes 	K2HWPlatform::getSpeakerMuteState() {
	return readGpioState ( kGPIO_Selector_SpeakerMute );
}

//	--------------------------------------------------------------------------------
IOReturn 	K2HWPlatform::setSpeakerMuteState ( GpioAttributes muteState ) {
	return writeGpioState ( kGPIO_Selector_SpeakerMute, muteState );
}
	
#pragma mark ---------------------------
#pragma mark Private I2C
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
bool K2HWPlatform::findAndAttachI2C()
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
		debugIOLog (5,  "K2HWPlatform::findAndAttachI2C i2cCandidate = %p ",i2cCandidate );
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
bool K2HWPlatform::openI2C()
{
	bool		result = false;
	
	FailIf ( NULL == mI2CInterface, Exit );
	FailIf ( !mI2CInterface->openI2CBus ( mI2CPort ), Exit );
	result = true;
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
void K2HWPlatform::closeI2C ()
{
	mI2CInterface->closeI2CBus ();
}

//	--------------------------------------------------------------------------------
bool K2HWPlatform::detachFromI2C()
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
bool K2HWPlatform::findAndAttachI2S()
{

	const OSSymbol	*i2sDriverName;
	IOService		*i2sCandidate;
	OSDictionary	*i2sServiceDictionary;
	bool			result = false;
	mach_timespec_t	timeout	= {5,0};

	i2sDriverName = OSSymbol::withCStringNoCopy ( "AppleI2S" );
	i2sServiceDictionary = IOService::resourceMatching ( i2sDriverName );
	FailIf ( NULL == i2sServiceDictionary, Exit );

	i2sCandidate = IOService::waitForService ( i2sServiceDictionary, &timeout );
	debugIOLog (3,  "i2sServiceDictionary %p", i2sServiceDictionary );
	FailIf(NULL == i2sCandidate,Exit);
	
	mI2SInterface = (AppleI2S*)i2sCandidate->getProperty ( i2sDriverName );
	FailIf ( NULL == mI2SInterface, Exit );
	
	mI2SInterface->retain();
	result = true;
	
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
bool K2HWPlatform::openI2S()
{
	return true;			//	No open in K2 I2S driver
}

//	--------------------------------------------------------------------------------
void K2HWPlatform::closeI2S ()
{
	return;					//	No close in K2 I2S driver
}

//	--------------------------------------------------------------------------------
bool K2HWPlatform::detachFromI2S()
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
const OSSymbol* K2HWPlatform::makeFunctionSymbolName(const char * name,UInt32 pHandle) {
	const OSSymbol* 	funcSymbolName = NULL;
	char		stringBuf[256];
		
	sprintf ( stringBuf, "%s-%08lx", name, pHandle );
	funcSymbolName = OSSymbol::withCString ( stringBuf );
	
	return funcSymbolName;
}

//	--------------------------------------------------------------------------------
GpioAttributes  K2HWPlatform::GetCachedAttribute ( GPIOSelector selector, GpioAttributes defaultResult ) {
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
	}

	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes K2HWPlatform::readGpioState ( GPIOSelector selector ) {
	GpioAttributes		result;
	
	result = kGPIO_Unknown;
	switch ( selector ) {
		case kGPIO_Selector_AnalogCodecReset:			result = 0 == ( *mGPIO_analogCodecReset & 0x02 ) ? kGPIO_Reset : kGPIO_Run ;											break;
		case kGPIO_Selector_ClockMux:					result = 0 == ( *mGPIO_mclkMuxSelect & 0x02 ) ? kGPIO_MuxSelectDefault : kGPIO_MuxSelectAlternate ;						break;
		case kGPIO_Selector_CodecInterrupt:				result = 0 == ( *mGPIO_digitalCodecIrq & 0x02 ) ? kGPIO_CodecInterruptInactive : kGPIO_CodecInterruptActive ;			break;
		case kGPIO_Selector_CodecErrorInterrupt:		result = 0 == ( *mGPIO_digitalCodecErrorIrq & 0x02 ) ? kGPIO_CodecInterruptInactive : kGPIO_CodecInterruptActive ;		break;
		case kGPIO_Selector_DigitalCodecReset:			result = 0 == ( *mGPIO_digitalCodecReset & 0x02 ) ? kGPIO_Reset : kGPIO_Run ;											break;
		case kGPIO_Selector_HeadphoneDetect:			result = 0 == ( *mGPIO_headphoneSense & 0x02 ) ? kGPIO_Connected : kGPIO_Disconnected ;									break;
		case kGPIO_Selector_HeadphoneMute:				result = 0 == ( *mGPIO_headphoneMute & 0x02 ) ? kGPIO_Muted : kGPIO_Unmuted ;											break;
		case kGPIO_Selector_InputDataMux:				result = 0 == ( *mGPIO_inputDataMuxSelect & 0x02 ) ? kGPIO_MuxSelectDefault : kGPIO_MuxSelectAlternate ;				break;
		case kGPIO_Selector_InternalSpeakerID:			result = 0 == ( *mGPIO_internalSpeakerID & 0x02 ) ? kGPIO_Disconnected : kGPIO_Connected ;								break;
		case kGPIO_Selector_LineInDetect:				result = 0 == ( *mGPIO_lineInSense & 0x02 ) ? kGPIO_Connected : kGPIO_Disconnected ;									break;
		case kGPIO_Selector_LineOutDetect:				result = 0 == ( *mGPIO_lineOutSense & 0x02 ) ? kGPIO_Connected : kGPIO_Disconnected ;									break;
		case kGPIO_Selector_LineOutMute:				result = 0 == ( *mGPIO_lineOutMute & 0x02 ) ? kGPIO_Muted : kGPIO_Unmuted ;												break;
		case kGPIO_Selector_SpeakerMute:				result = 0 == ( *mGPIO_speakerMute & 0x02 ) ? kGPIO_Unmuted : kGPIO_Muted ;												break;
		default:										goto Exit;																												break;
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn K2HWPlatform::writeGpioState ( GPIOSelector selector, GpioAttributes gpioState ) {
	UInt32				value;
	IOReturn			result;

	result = kIOReturnError;
	value = 0;
	switch ( selector ) {
		case kGPIO_Selector_AnalogCodecReset:		FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Reset, gpioState, &value ), Exit );		break;
		case kGPIO_Selector_ClockMux:				FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Mux, gpioState, &value ), Exit );		break;
		case kGPIO_Selector_DigitalCodecReset:		FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Reset, gpioState, &value ), Exit );		break;
		case kGPIO_Selector_HeadphoneMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, gpioState, &value ), Exit );		break;
		case kGPIO_Selector_InputDataMux:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Mux, gpioState, &value ), Exit );		break;
		case kGPIO_Selector_LineOutMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, gpioState, &value ), Exit );		break;
		case kGPIO_Selector_SpeakerMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteL, gpioState, &value ), Exit );		break;
		default:									FailIf ( true, Exit );																								break;
	}
	switch ( selector ) {
		case kGPIO_Selector_AnalogCodecReset:		*mGPIO_analogCodecReset = value ? kGPIO_DATA_WR_PIN_LEVEL_H : kGPIO_DATA_WR_PIN_LEVEL_L ;							break;
		case kGPIO_Selector_ClockMux:				*mGPIO_mclkMuxSelect = value ? kGPIO_DATA_WR_PIN_LEVEL_H : kGPIO_DATA_WR_PIN_LEVEL_L ;								break;
		case kGPIO_Selector_DigitalCodecReset:		*mGPIO_digitalCodecReset = value ? kGPIO_DATA_WR_PIN_LEVEL_H : kGPIO_DATA_WR_PIN_LEVEL_L ;							break;
		case kGPIO_Selector_HeadphoneMute:			*mGPIO_headphoneMute = value ? kGPIO_DATA_WR_PIN_LEVEL_H : kGPIO_DATA_WR_PIN_LEVEL_L ;								break;
		case kGPIO_Selector_InputDataMux:			*mGPIO_inputDataMuxSelect = value ? kGPIO_DATA_WR_PIN_LEVEL_H : kGPIO_DATA_WR_PIN_LEVEL_L ;							break;
		case kGPIO_Selector_LineOutMute:			*mGPIO_lineOutMute = value ? kGPIO_DATA_WR_PIN_LEVEL_H : kGPIO_DATA_WR_PIN_LEVEL_L ;								break;
		case kGPIO_Selector_SpeakerMute:			*mGPIO_speakerMute = value ? kGPIO_DATA_WR_PIN_LEVEL_H : kGPIO_DATA_WR_PIN_LEVEL_L ;								break;
		default:									FailIf ( true, Exit );																								break;
	}
	//	Update the gpio cache so that read accessors return correct data...
	switch ( selector ) {
		case kGPIO_Selector_AnalogCodecReset:		mAppleGPIO_AnalogCodecReset = gpioState;						break;
		case kGPIO_Selector_ClockMux:				mAppleGPIO_CodecClockMux = gpioState;							break;
		case kGPIO_Selector_DigitalCodecReset:		mAppleGPIO_DigitalCodecReset = gpioState;						break;
		case kGPIO_Selector_HeadphoneMute:			mAppleGPIO_HeadphoneMute = gpioState;							break;
		case kGPIO_Selector_InputDataMux:			mAppleGPIO_CodecInputDataMux = gpioState;						break;
		case kGPIO_Selector_LineOutMute:			mAppleGPIO_LineOutMute = gpioState;								break;
		case kGPIO_Selector_SpeakerMute:			mAppleGPIO_AmpMute = gpioState;									break;
		default:									FailIf ( true, Exit );											break;
	}
	result = kIOReturnSuccess;
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
//	Convert a GPIOAttribute to a binary value to be applied to a GPIO pin level
IOReturn K2HWPlatform::translateGpioAttributeToGpioState ( GPIOType gpioType, GpioAttributes gpioAttribute, UInt32 * valuePtr ) {
	IOReturn				result = kIOReturnError;
	
//	debugIOLog (3,  "K2HWPlatform::translateGpioAttributeToGpioState ( %d, %d, %p )", (unsigned int)gpioType, (unsigned int)gpioAttribute, valuePtr );
	if ( NULL != valuePtr ) {
		result = kIOReturnSuccess;
		switch ( gpioType ) {
			case kGPIO_Type_ConnectorType:
				if ( kGPIO_TypeIsDigital == gpioAttribute ) {
					*valuePtr = 1;
				} else if ( kGPIO_TypeIsAnalog == gpioAttribute ) {
					*valuePtr = 0;
				} else {
					result = kIOReturnBadArgument;
				}
				break;
			case kGPIO_Type_Detect:
				if ( kGPIO_Disconnected == gpioAttribute ) {
					*valuePtr = 0;
				} else if ( kGPIO_Connected == gpioAttribute ) {
					*valuePtr = 1;
				} else {
					result = kIOReturnBadArgument;
				}
				break;
			case kGPIO_Type_Irq:
				if ( kGPIO_CodecInterruptActive == gpioAttribute ) {
					*valuePtr = 1;
				} else if ( kGPIO_CodecInterruptInactive == gpioAttribute ) {
					*valuePtr = 0;
				} else {
					result = kIOReturnBadArgument;
				}
				break;
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
#ifdef kVERBOSE_Log
	if ( kIOReturnSuccess != result ) { 
		switch ( gpioType ) {
			case kGPIO_Type_ConnectorType:	debugIOLog (3,  "FAIL: K2HWPlatform::translateGpioAttributeToGpioState ( kGPIO_Type_ConnectorType, %p )", valuePtr ); 	break;
			case kGPIO_Type_Detect:			debugIOLog (3,  "FAIL: K2HWPlatform::translateGpioAttributeToGpioState ( kGPIO_Type_Detect, %p )", valuePtr ); 			break;
			case kGPIO_Type_Irq:			debugIOLog (3,  "FAIL: K2HWPlatform::translateGpioAttributeToGpioState ( kGPIO_Type_Irq, %p )", valuePtr ); 				break;
			case kGPIO_Type_MuteL:			debugIOLog (3,  "FAIL: K2HWPlatform::translateGpioAttributeToGpioState ( kGPIO_Type_MuteL, %p )", valuePtr ); 			break;
			case kGPIO_Type_MuteH:			debugIOLog (3,  "FAIL: K2HWPlatform::translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, %p )", valuePtr ); 			break;
			case kGPIO_Type_Mux:			debugIOLog (3,  "FAIL: K2HWPlatform::translateGpioAttributeToGpioState ( kGPIO_Type_Mux, %p )", valuePtr ); 				break;
			case kGPIO_Type_Reset:			debugIOLog (3,  "FAIL: K2HWPlatform::translateGpioAttributeToGpioState ( kGPIO_Type_Reset, %p )", valuePtr ); 			break;
			default:						debugIOLog (3,  "FAIL: K2HWPlatform::translateGpioAttributeToGpioState ( %X, %p )", gpioType, valuePtr ); 				break;
		}
	}
#endif
	return result;
}

#pragma mark ---------------------------
#pragma mark DBDMA Memory Address Acquisition Methods
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	K2HWPlatform::GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) {
	IOMemoryMap *				map;
	IOService *					parentOfParent;
	IOPhysicalAddress			ioPhysAddr;
	IOMemoryDescriptor *		theDescriptor;
	IOByteCount					length = 256;
	
	mIOBaseDMAInput = NULL;
	FailIf ( NULL == dbdmaProvider, Exit );
	debugIOLog (3,  "K2HWPlatform::GetInputChannelRegistersVirtualAddress i2s-a name is %s", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	debugIOLog (3,  "   parentOfParent name is %s", parentOfParent->getName() );
	map = parentOfParent->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAOutputIndex );
	FailIf ( NULL == map, Exit );
	ioPhysAddr = map->getPhysicalSegment( kIODMAInputOffset, &length );
	FailIf ( NULL == ioPhysAddr, Exit );
	theDescriptor = IOMemoryDescriptor::withPhysicalAddress ( ioPhysAddr, 256, kIODirectionOutIn );
	FailIf ( NULL == theDescriptor, Exit );
	map = theDescriptor->map ();
	FailIf ( NULL == map, Exit );
	mIOBaseDMAInput = (IODBDMAChannelRegisters*)map->getVirtualAddress();
	
	debugIOLog (3,  "mIOBaseDMAInput %p", mIOBaseDMAInput );
	if ( NULL == mIOBaseDMAInput ) { debugIOLog (1,  "K2HWPlatform::GetInputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE" ); }
Exit:
	return mIOBaseDMAInput;
}

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	K2HWPlatform::GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) {
	IOMemoryMap *				map;
	IOService *					parentOfParent;
	
	mIOBaseDMAOutput = NULL;
	FailIf ( NULL == dbdmaProvider, Exit );
	debugIOLog (3,  "K2HWPlatform::GetOutputChannelRegistersVirtualAddress i2s-a name is %s", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	debugIOLog (3,  "   parentOfParent name is %s", parentOfParent->getName() );
	map = parentOfParent->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAOutputIndex );
	FailIf ( NULL == map, Exit );
	mIOBaseDMAOutput = (IODBDMAChannelRegisters*)map->getVirtualAddress();
	
	debugIOLog (3,  "mIOBaseDMAOutput %p is at physical %p", mIOBaseDMAOutput, (void*)map->getPhysicalAddress() );
	if ( NULL == mIOBaseDMAOutput ) { debugIOLog (1,  "K2HWPlatform::GetOutputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE" ); }
Exit:
	return mIOBaseDMAOutput;
}

#pragma mark ---------------------------
#pragma mark DEBUGGING UTILITIES
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
void K2HWPlatform::LogFCR ( void ) {
	UInt32			data;
	
	data = (unsigned int)OSReadSwapInt32 ( mHwPtr, kAUDIO_MAC_IO_FCR1 );
	debugIOLog (3,  "[#] FCR1: %X -> %lX ::: %s %s %s %s", 
						kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_MAC_IO_FCR1, 
						data,
						0 == ( data & ( 1 << 13 ) ) ? "i2s0_enable_h" : "I2S0_ENABLE_H" ,
						0 == ( data & ( 1 << 12 ) ) ? "i2s0_clkenbit_h" : "I2S0_CLKENBIT_H" ,
						0 == ( data & ( 1 << 11 ) ) ? "i2s0_swreset_h" : "I2S0_SWRESET_H" ,
						0 == ( data & ( 1 << 10 ) ) ? "i2s0_cell_en_h" : "I2S0_CELL_EN_H"
					);
	data = (unsigned int)OSReadSwapInt32 ( mHwPtr, kAUDIO_MAC_IO_FCR3 );
	debugIOLog (3,  "[#] FCR3: %X -> %lX ::: %s %s %s %s %s", 
						kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_MAC_IO_FCR3, 
						data,
						0 == ( data & ( 1 << 14 ) ) ? "i2s0_clk18_en_h" : "I2S_CLK18_EN_H" ,
						0 == ( data & ( 1 << 10 ) ) ? "clk45_en_h" : "CLK45_EN_H" ,
						0 == ( data & ( 1 << 9 ) ) ? "clk49_en_h" : "CLK49_EN_H" ,
						0 == ( data & ( 1 << 5 ) ) ? "enablepll2shutdown_h" : "ENABLEPLL2SHUTDOWN_H",
						0 == ( data & ( 1 << 5 ) ) ? "enablepll3shutdown_h" : "ENABLEPLL3SHUTDOWN_H"
					);
	IOSleep ( 10 );
	return;
}

//	--------------------------------------------------------------------------------
void K2HWPlatform::LogI2S ( void ) {
	debugIOLog (3,  "[*] I2S Serial Format:     %X -> %X", kAUDIO_I2S_BASE_ADDRESS + kAUDIO_I2S_SERIAL_FORMAT, (unsigned int)OSReadSwapInt32 ( mHwI2SPtr, kAUDIO_I2S_SERIAL_FORMAT ) );
	debugIOLog (3,  "[*] I2S Data Word Size:    %X -> %X", kAUDIO_I2S_BASE_ADDRESS + kAUDIO_I2S_DATA_WORD_SIZES, (unsigned int)OSReadSwapInt32 ( mHwI2SPtr, kAUDIO_I2S_DATA_WORD_SIZES ) );
	debugIOLog (3,  "[*] I2S Frame Count:       %X -> %X", kAUDIO_I2S_BASE_ADDRESS + kAUDIO_I2S_FRAME_COUNTER, (unsigned int)OSReadSwapInt32 ( mHwI2SPtr, kAUDIO_I2S_FRAME_COUNTER ) );
	debugIOLog (3,  "[*] I2S Interrupt Control: %X -> %X", kAUDIO_I2S_BASE_ADDRESS + kAUDIO_I2S_INTERRUPT_CONTROL, (unsigned int)OSReadSwapInt32 ( mHwI2SPtr, kAUDIO_I2S_INTERRUPT_CONTROL ) );
	return;
}

//	--------------------------------------------------------------------------------
void K2HWPlatform::LogInterruptGPIO ( void ) {
	debugIOLog (1,  "### CodecErrorIRQ %X, CodecIRQ %X, HeadphoneDet %X, LineInDet %X, LineOutDet %X",
		(unsigned int)mHwPtr[kAUDIO_GPIO_CODEC_ERROR_IRQ], 
		(unsigned int)mHwPtr[kAUDIO_GPIO_CODEC_IRQ], 
		(unsigned int)mHwPtr[kAUDIO_GPIO_HEADPHONE_SENSE], 
		(unsigned int)mHwPtr[kAUDIO_GPIO_LINE_IN_SENSE], 
		(unsigned int)mHwPtr[kAUDIO_GPIO_LINE_OUT_SENSE] 
	);
	return;
}
	
//	--------------------------------------------------------------------------------
void K2HWPlatform::LogGPIO ( void ) {
	debugIOLog (3,  "GPIO28 AnalogReset:         %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_ANALOG_CODEC_RESET, (unsigned int)mHwPtr[kAUDIO_GPIO_ANALOG_CODEC_RESET] );
	debugIOLog (3,  "GPIO30 ClockMuxSelect:      %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_CLOCK_MUX_SELECT, (unsigned int)mHwPtr[kAUDIO_GPIO_CLOCK_MUX_SELECT] );
	debugIOLog (3,  "GPIO5  CodecErrorInterrupt: %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_CODEC_ERROR_IRQ, (unsigned int)mHwPtr[kAUDIO_GPIO_CODEC_ERROR_IRQ] );
	debugIOLog (3,  "GPIO16 CodecInterrupt:      %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_CODEC_IRQ, (unsigned int)mHwPtr[kAUDIO_GPIO_CODEC_IRQ] );
	debugIOLog (3,  "GPIO12 DigitalCodecReset:   %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_DIGITAL_CODEC_RESET, (unsigned int)mHwPtr[kAUDIO_GPIO_DIGITAL_CODEC_RESET] );
	debugIOLog (3,  "GPIO15 HeadphoneDetect:     %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_HEADPHONE_SENSE, (unsigned int)mHwPtr[kAUDIO_GPIO_HEADPHONE_SENSE] );
	debugIOLog (3,  "GPIO23 HeadphoneMute:       %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_HEADPHONE_MUTE, (unsigned int)mHwPtr[kAUDIO_GPIO_HEADPHONE_MUTE] );
	debugIOLog (3,  "GPIO3  InputDataMuxSelect:  %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_INPUT_DATA_MUX_SELECT, (unsigned int)mHwPtr[kAUDIO_GPIO_INPUT_DATA_MUX_SELECT] );
	debugIOLog (3,  "GPIO4  LineInDetect:        %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_LINE_IN_SENSE, (unsigned int)mHwPtr[kAUDIO_GPIO_LINE_IN_SENSE] );
	debugIOLog (3,  "GPIO14 LineOutDetect:       %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_LINE_OUT_SENSE, (unsigned int)mHwPtr[kAUDIO_GPIO_LINE_OUT_SENSE] );
	debugIOLog (3,  "GPIO29 LineOutMute:         %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_LINE_OUT_MUTE, (unsigned int)mHwPtr[kAUDIO_GPIO_LINE_OUT_MUTE] );
	debugIOLog (3,  "GPIO24 SpeakerMute:         %X -> %X", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_AMPLIFIER_MUTE, (unsigned int)mHwPtr[kAUDIO_GPIO_AMPLIFIER_MUTE] );
	return;
}
	
//	--------------------------------------------------------------------------------
void K2HWPlatform::LogDBDMAChannelRegisters ( void ) {
	debugIOLog (3,  "Output ChannelControl:  %lX", mIOBaseDMAOutput->channelControl );
	debugIOLog (3,  "Output channelStatus:   %lX", mIOBaseDMAOutput->channelStatus );
	debugIOLog (3,  "Output commandPtrHi:    %lX", mIOBaseDMAOutput->commandPtrHi );
	debugIOLog (3,  "Output commandPtrLo:    %lX", mIOBaseDMAOutput->commandPtrLo );
	debugIOLog (3,  "Output interruptSelect: %lX", mIOBaseDMAOutput->interruptSelect );
	debugIOLog (3,  "Output branchSelect:    %lX", mIOBaseDMAOutput->branchSelect );
	debugIOLog (3,  "Output waitSelect:      %lX", mIOBaseDMAOutput->waitSelect );
	debugIOLog (3,  "Output transferModes:   %lX", mIOBaseDMAOutput->transferModes );
	debugIOLog (3,  "Output data2PtrHi:      %lX", mIOBaseDMAOutput->data2PtrHi );
	debugIOLog (3,  "Output data2PtrLo:      %lX", mIOBaseDMAOutput->data2PtrLo );
	debugIOLog (3,  "Output reserved1:       %lX", mIOBaseDMAOutput->reserved1 );
	debugIOLog (3,  "Output addressHi:       %lX", mIOBaseDMAOutput->addressHi );
	return;
}


#pragma mark ---------------------------
#pragma mark USER CLIENT SUPPORT
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IOReturn	K2HWPlatform::getPlatformState ( PlatformStateStructPtr outState ) {
	IOReturn			result = kIOReturnBadArgument;
	
	FailIf ( NULL == outState, Exit );
	outState->platformType = kPlatformInterfaceType_K2;
	
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
	
	LogGPIO();
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
	outState->gpio.reserved_18 = kGPIO_Unknown;
	outState->gpio.reserved_19 = kGPIO_Unknown;
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
IOReturn	K2HWPlatform::setPlatformState ( PlatformStateStructPtr inState ) {
	IOReturn			result = kIOReturnBadArgument;
	
	FailIf ( NULL == inState, Exit );
	if ( inState->i2s.intCtrl != getI2SIOMIntControl () ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setI2SIOMIntControl ( %lX )", inState->i2s.intCtrl );
		result = setI2SIOMIntControl ( inState->i2s.intCtrl );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->i2s.serialFmt != getSerialFormatRegister () ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setSerialFormatRegister ( %lX )", inState->i2s.serialFmt );
		result = setSerialFormatRegister ( inState->i2s.serialFmt );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( inState->i2s.frameCount != getFrameCount () ) && ( 0 == inState->i2s.frameCount ) ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setFrameCount ( %lX )", inState->i2s.frameCount );
		result = setFrameCount ( inState->i2s.frameCount );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->i2s.dataWordSizes != getDataWordSizes () ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setDataWordSizes ( %lX )", inState->i2s.dataWordSizes );
		result = setDataWordSizes ( inState->i2s.dataWordSizes );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	
	if ( inState->fcr.i2sEnable != getI2SEnable () ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setI2SEnable ( %lX )", inState->fcr.i2sEnable );
		result = setI2SEnable ( inState->fcr.i2sEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->fcr.i2sClockEnable != getI2SClockEnable () ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setI2SClockEnable ( %lX )", inState->fcr.i2sClockEnable );
		result = setI2SClockEnable ( inState->fcr.i2sClockEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->fcr.i2sCellEnable != getI2SCellEnable () ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setI2SCellEnable ( %lX )", inState->fcr.i2sCellEnable );
		result = setI2SCellEnable ( inState->fcr.i2sCellEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	
	if ( ( kGPIO_Unknown != inState->gpio.gpio_AnalogCodecReset ) && ( inState->gpio.gpio_DigitalCodecReset != getCodecReset ( kCODEC_RESET_Analog ) ) ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setCodecReset ( kCODEC_RESET_Analog, %d )", inState->gpio.gpio_DigitalCodecReset );
		result = setCodecReset ( kCODEC_RESET_Analog, inState->gpio.gpio_AnalogCodecReset );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_ClockMux ) && ( inState->gpio.gpio_ClockMux != getClockMux () ) ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setClockMux ( %d )", inState->gpio.gpio_ClockMux );
		result = setClockMux ( inState->gpio.gpio_ClockMux );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_DigitalCodecReset ) && ( inState->gpio.gpio_DigitalCodecReset != getCodecReset ( kCODEC_RESET_Digital ) ) ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setCodecReset ( kCODEC_RESET_Digital, %d )", inState->gpio.gpio_DigitalCodecReset );
		result = setCodecReset ( kCODEC_RESET_Digital, inState->gpio.gpio_DigitalCodecReset );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_HeadphoneMute ) && ( inState->gpio.gpio_HeadphoneMute != getHeadphoneMuteState () ) ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setHeadphoneMuteState ( %d )", inState->gpio.gpio_HeadphoneMute );
		result = setHeadphoneMuteState ( inState->gpio.gpio_HeadphoneMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_InputDataMux ) && ( inState->gpio.gpio_InputDataMux != getInputDataMux () ) ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setInputDataMux ( %d )", inState->gpio.gpio_InputDataMux );
		result = setInputDataMux ( inState->gpio.gpio_InputDataMux );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_LineOutMute ) && ( inState->gpio.gpio_LineOutMute != getLineOutMuteState () ) ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setLineOutMuteState ( %d )", inState->gpio.gpio_LineOutMute );
		result = setLineOutMuteState ( inState->gpio.gpio_LineOutMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_SpeakerMute ) && ( inState->gpio.gpio_SpeakerMute != getSpeakerMuteState () ) ) {
		debugIOLog (3,  "K2HWPlatform::setPlatformState setSpeakerMuteState ( %d )", inState->gpio.gpio_SpeakerMute );
		result = setSpeakerMuteState ( inState->gpio.gpio_SpeakerMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}







