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

const char * 	K2Platform::kAppleGPIO_GetComboInJackType				= "combo-in-type";		

const char * 	K2Platform::kAppleGPIO_GetComboOutJackType				= "combo-out-type";

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
	
	debugIOLog ( "+ K2Platform::init\n" );
	result = super::init (device, provider, inDBDMADeviceIndex);
	FailIf ( !result, Exit );

	debug2IOLog ( "    about to waitForService on mK2Service %p\n", mK2Service );
	mK2Service = IOService::waitForService ( IOService::serviceMatching ( "AppleK2" ) );
	debug2IOLog ( "    mK2Service %p\n", mK2Service );
	
	sound = device;

	FailWithAction (!sound, result = false, Exit);
	debug2IOLog ("K2 - sound's name is %s\n", sound->getName ());

	mI2S = sound->getParentEntry (gIODTPlane);
	FailWithAction (!mI2S, result = false, Exit);
	debug2IOLog ("K2Platform - i2s's name is %s\n", mI2S->getName ());

	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "AAPL,phandle" ) );
	mI2SPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debug2IOLog ( "mI2SPHandle %lX", mI2SPHandle );
	
	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "reg" ) );
	mI2SOffset = *((UInt32*)osdata->getBytesNoCopy());
		
	i2S = mI2S->getParentEntry (gIODTPlane);
	FailWithAction (!i2S, result = false, Exit);
	debug2IOLog ("mI2S - parent name is %s\n", i2S->getName ());

	macIO = i2S->getParentEntry (gIODTPlane);
	FailWithAction (!macIO, result = false, Exit);
	debug2IOLog ("i2S - parent name is %s\n", macIO->getName ());
	
	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "AAPL,phandle" ) );
	mMacIOPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debug2IOLog ( "mMacIOPHandle %lX", mMacIOPHandle );

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

	debugIOLog ( "about to findAndAttachI2C\n" );
	result = findAndAttachI2C();
	if ( !result ) { debugIOLog ( "K2Platform::init COULD NOT FIND I2C\n" ); }
	FailIf ( !result, Exit );
#ifndef kBUILD_FOR_DIRECT_I2S_HW_ACCESS	
	debugIOLog ( "about to findAndAttachI2S\n" );
	result = findAndAttachI2S();
	if ( !result ) { debugIOLog ( "K2Platform::init COULD NOT FIND I2S\n" ); }
	FailIf ( !result, Exit );
#endif
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

	debug2IOLog ( "- K2Platform::init returns %d\n", result );
	return result;
}

//	--------------------------------------------------------------------------------
void K2Platform::free()
{
	debugIOLog ("+ K2Platform::free()\n");

	detachFromI2C();
	//detachFromI2S();

	super::free();

	debugIOLog ("- K2Platform::free()\n");
}

#pragma mark ---------------------------
#pragma mark Codec Methods	
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
bool K2Platform::writeCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode) {
	bool success = false;

#ifdef	kVERBOSE_LOG
	debug6IrqIOLog ( "+ K2Platform::writeCodecRegister ( %X, %X, %p, %d, %d )\n", address, subAddress, data, len, mode );
#endif
	FailIf ( NULL == data, Exit );
	if (openI2C()) {
		switch (mode) {
			case kI2C_StandardMode:			mI2CInterface->setStandardMode();			break;
			case kI2C_StandardSubMode:		mI2CInterface->setStandardSubMode();		break;
			case kI2C_CombinedMode:			mI2CInterface->setCombinedMode();			break;
			default:
				debugIrqIOLog ("K2Platform::writeCodecRegister() unknown bus mode!\n");
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
#ifdef kVERBOSE_LOG	
		debug6IrqIOLog ( " mI2CInterface->writeI2CBus ( %X, %X, %p, %d ), data->%X\n", (unsigned int)address, (unsigned int)subAddress, data, (unsigned int)len, *data );
#endif
		success = mI2CInterface->writeI2CBus (address >> 1, subAddress, data, len);
		mI2C_lastTransactionResult = success;
		
		if (!success) { 
			debug5IrqIOLog ( "K2Platform::writeCodecRegister( %X, %X, %p %d), mI2CInterface->writeI2CBus returned false.\n", address, subAddress, data, len );
		}
Exit:
		closeI2C();
	} else {
		debugIrqIOLog ("K2Platform::writeCodecRegister() couldn't open the I2C bus!\n");
	}

#ifdef kVERBOSE_LOG	
	debug7IrqIOLog ( "- K2Platform::writeCodecRegister ( %X, %X, %p, %d, %d ) returns %d\n", address, subAddress, data, len, mode, success );
#endif
	return success;
}

//	--------------------------------------------------------------------------------
bool K2Platform::readCodecRegister(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len, BusMode mode) {
	bool success = false;

#ifdef kVERBOSE_LOG	
	debug6IrqIOLog ( "+ K2Platform::readCodecRegister ( %X, %X, %p, %d, %d )\n", address, subAddress, data, len, mode );
#endif
	if (openI2C()) {
		switch (mode) {
			case kI2C_StandardMode:			mI2CInterface->setStandardMode();			break;
			case kI2C_StandardSubMode:		mI2CInterface->setStandardSubMode();		break;
			case kI2C_CombinedMode:			mI2CInterface->setCombinedMode();			break;
			default:
				debugIrqIOLog ("K2Platform::readCodecRegister() unknown bus mode!\n");
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

		if (!success) debugIrqIOLog ("K2Platform::readCodecRegister(), mI2CInterface->writeI2CBus returned false.\n");
Exit:
		closeI2C();
	} else {
		debugIrqIOLog ("K2Platform::readCodecRegister() couldn't open the I2C bus!\n");
	}
#ifdef kVERBOSE_LOG	
	debug7IrqIOLog ( "- K2Platform::readCodecRegister ( %X, %X, %p, %d, %d ) returns %d\n", address, subAddress, data, len, mode, success );
#endif
	return success;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setCodecReset ( CODEC_RESET target, GpioAttributes reset ) {
	IOReturn				result;

#ifdef kVERBOSE_LOG
	debug3IrqIOLog ( "K2Platform::setCodecReset ( %d, %d )\n", (unsigned int)target, (unsigned int)reset );
#endif
	switch ( target ) {
		case kCODEC_RESET_Analog:	result = writeGpioState ( kGPIO_Selector_AnalogCodecReset, reset );		break;
		case kCODEC_RESET_Digital:	result = writeGpioState ( kGPIO_Selector_DigitalCodecReset, reset );	break;
		default:					result = kIOReturnBadArgument;											break;
	}
	if ( kIOReturnSuccess != result ) {
		debug4IrqIOLog ( "- K2Platform::setCodecReset ( %d, %d ) returns %X\n", (unsigned int)target, (unsigned int)reset, (unsigned int)result );
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
#ifndef kBUILD_FOR_DIRECT_I2SEnable_HW_ACCESS
	const OSSymbol*			funcSymbolName = NULL;
#endif

#ifdef kVERBOSE_LOG
	debug2IrqIOLog( "+ K2Platform::setI2SEnable enable=%d\n", enable );
#endif

	result = kIOReturnError;
#ifdef kBUILD_FOR_DIRECT_I2SEnable_HW_ACCESS
	UInt32			data;
	
	if ( mFcr1 ) {
		if ( enable ) {
			debugIrqIOLog ( "K2Platform::setI2SEnable to 1\n" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data |= ( 1 << 13 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		} else {
			debugIrqIOLog ( "K2Platform::setI2SEnable to 0\n" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data &= ~( 1 << 13 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		}
		result = kIOReturnSuccess;
	}
#else
	if ( mK2Service ) {
		if ( enable ) {
			debugIrqIOLog ( "K2Platform::setI2SEnable to 1\n" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_Enable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)(UInt32)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		} else {
			debugIrqIOLog ( "K2Platform::setI2SEnable to 0\n" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_Disable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)(UInt32)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		LogFCR ();
	}
Exit:
#endif
#ifdef kVERBOSE_LOG
	debug3IrqIOLog( "- K2Platform::setI2SEnable enable=%d returns %d\n", enable, result );
#endif
	return result;
}

//	--------------------------------------------------------------------------------
bool K2Platform::getI2SEnable () {
#ifndef kBUILD_FOR_DIRECT_I2SEnable_HW_ACCESS
	const OSSymbol*			funcSymbolName = NULL;
	IOReturn				result;
#endif
	UInt32					value = 0;

#ifdef kBUILD_FOR_DIRECT_I2SEnable_HW_ACCESS
	if ( NULL != mFcr1 ) {
		value = ( (unsigned int)OSReadSwapInt32 ( mFcr1, 0 ) >> 13 ) & 0x00000001;
	}
#else
	if ( mK2Service ) {
		funcSymbolName = makeFunctionSymbolName( kAppleI2S_GetEnable, mI2SPHandle );
		FailIf ( NULL == funcSymbolName, Exit );
		result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)&value, (void *)(UInt32)0, 0, 0 );
		funcSymbolName->release ();	// [3324205]
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
#endif
	return ( 0 != value );
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setI2SClockEnable (bool enable) {
	IOReturn				result;
#ifndef kBUILD_FOR_DIRECT_I2SClockEnable_HW_ACCESS
	const OSSymbol*			funcSymbolName = NULL;
#endif

#ifdef kVERBOSE_LOG
	debug2IrqIOLog( "+ K2Platform::setI2SClockEnable enable=%d\n", enable );
#endif

	result = kIOReturnError;
#ifdef kBUILD_FOR_DIRECT_I2SClockEnable_HW_ACCESS
	UInt32			data;
	
	if ( mFcr1 ) {
		if ( enable ) {
			debugIrqIOLog ( "K2Platform::setI2SClockEnable to 1\n" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data |= ( 1 << 12 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		} else {
			debugIrqIOLog ( "K2Platform::setI2SClockEnable to 0\n" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data &= ~( 1 << 12 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		}
		result = kIOReturnSuccess;
	}
#else
	if ( mK2Service ) {
		if ( enable ) {
			debugIrqIOLog ( "K2Platform::setI2SClockEnable to 1\n" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_ClockEnable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		} else {
			debugIrqIOLog ( "K2Platform::setI2SClockEnable to 0\n" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_ClockDisable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		LogFCR ();
		if ( kIOReturnSuccess == result ) mAppleI2S_ClockEnable = enable;
	}
Exit:
#endif
#ifdef kVERBOSE_LOG
	debug3IrqIOLog( "- K2Platform::setI2SClockEnable enable=%d returns %d\n", enable, result );
#endif
	return result;
}

//	--------------------------------------------------------------------------------
bool K2Platform::getI2SClockEnable () {
#ifndef kBUILD_FOR_DIRECT_I2SClockEnable_HW_ACCESS
	const OSSymbol*			funcSymbolName = NULL;
	IOReturn				result;
#endif
	UInt32					value = 0;

#ifdef kBUILD_FOR_DIRECT_I2SClockEnable_HW_ACCESS
	if ( NULL != mFcr1 ) {
		value = ( (unsigned int)OSReadSwapInt32 ( mFcr1, 0 ) >> 12 ) & 0x00000001;
	}
#else
	if ( mK2Service ) {
		funcSymbolName = makeFunctionSymbolName( kAppleI2S_GetClockEnable, mI2SPHandle );
		FailIf ( NULL == funcSymbolName, Exit );
		result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)&value, (void *)(UInt32)0, 0, 0 );
		funcSymbolName->release ();	// [3324205]
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
#endif
	return ( 0 != value );
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setI2SCellEnable (bool enable) {
	IOReturn				result;
#ifndef kBUILD_FOR_DIRECT_I2SCellEnable_HW_ACCESS
	const OSSymbol*			funcSymbolName = NULL;
#endif
	
#ifdef kVERBOSE_LOG
	debug2IrqIOLog("+ K2Platform::setI2SCellEnable enable=%d\n",enable);
#endif
	
#ifdef kBUILD_FOR_DIRECT_I2SCellEnable_HW_ACCESS
	UInt32			data;
	
	if ( mFcr1 ) {
		if ( enable ) {
			debugIrqIOLog ( "K2Platform::setI2SCellEnable to 1\n" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data |= ( 1 << 10 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		} else {
			debugIrqIOLog ( "K2Platform::setI2SCellEnable to 0\n" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data &= ~( 1 << 10 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		}
		result = kIOReturnSuccess;
	}
#else
	result = kIOReturnError;
	if ( mK2Service ) {
		if ( enable ) {
			debugIrqIOLog ( "K2Platform::setI2SCellEnable to 1\n" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_CellEnable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		} else {
			debugIrqIOLog ( "K2Platform::setI2SCellEnable to 0\n" );
			funcSymbolName = makeFunctionSymbolName( kAppleI2S_CellDisable, mI2SPHandle );
			FailIf ( NULL == funcSymbolName, Exit );
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)0, (void *)0, 0, 0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		LogFCR ();
		if ( kIOReturnSuccess == result ) mAppleI2S_CellEnable = enable;
	}
Exit:
#endif
#ifdef kVERBOSE_LOG
	debug2IrqIOLog("- K2Platform::setI2SCellEnable result = %x\n",result);
#endif
	return result;
}

//	--------------------------------------------------------------------------------
bool K2Platform::getI2SCellEnable () {
#ifndef kBUILD_FOR_DIRECT_I2SCellEnable_HW_ACCESS
	const OSSymbol*			funcSymbolName;
	IOReturn				result;
#endif
	UInt32					value = 0;

#ifdef kBUILD_FOR_DIRECT_I2SCellEnable_HW_ACCESS
	if ( NULL != mFcr1 ) {
		value = ( (unsigned int)OSReadSwapInt32 ( mFcr1, 0 ) >> 10 ) & 0x00000001;
	}
#else
	if ( mK2Service ) {
		funcSymbolName = makeFunctionSymbolName( kAppleI2S_GetCellEnable, mI2SPHandle );
		FailIf ( NULL == funcSymbolName, Exit );
		result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)&value, (void *)(UInt32)0, 0, 0 );
		funcSymbolName->release ();	// [3324205]
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
#endif
	return ( 0 != value );
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setI2SSWReset(bool enable) {
	IOReturn				result;
#ifndef kBUILD_FOR_DIRECT_I2SswReset_HW_ACCESS
	const OSSymbol*			funcSymbolName = NULL;
#endif
	
#ifdef kVERBOSE_LOG
	debug2IrqIOLog("+ K2Platform::setI2SSWReset enable=%d\n",enable);
#endif
	
#ifdef kBUILD_FOR_DIRECT_I2SswReset_HW_ACCESS
	UInt32			data;
	
	if ( mFcr1 ) {
		if ( enable ) {
			debugIrqIOLog ( "K2Platform::setI2SSWReset to 1\n" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data |= ( 1 << 11 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		} else {
			debugIrqIOLog ( "K2Platform::setI2SSWReset to 0\n" );
			data = (unsigned int)OSReadSwapInt32 ( mFcr1, 0 );
			data &= ~( 1 << 11 );
			OSWriteSwapInt32 ( mFcr1, 0, data );
		}
		result = kIOReturnSuccess;
	}
#else
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
		LogFCR ();
		if ( kIOReturnSuccess == result ) mAppleI2S_CellEnable = enable;
	}
Exit:
#endif
#ifdef kVERBOSE_LOG
	debug2IrqIOLog("- K2Platform::setI2SSWReset result = %x\n",result);
#endif

	return result;
}


//	--------------------------------------------------------------------------------
bool K2Platform::getI2SSWReset() {
#ifndef kBUILD_FOR_DIRECT_I2SswReset_HW_ACCESS
	const OSSymbol*			funcSymbolName = NULL;
	IOReturn				result;
#endif
	UInt32					value = 0;

#ifdef kBUILD_FOR_DIRECT_I2SswReset_HW_ACCESS
	if ( NULL != mFcr1 ) {
		value = ( (unsigned int)OSReadSwapInt32 ( mFcr1, 0 ) >> 11 ) & 0x00000001;
	}
#else
	if ( mK2Service ) {
		funcSymbolName = makeFunctionSymbolName( kAppleI2S_GetReset, mI2SPHandle );
		FailIf ( NULL == funcSymbolName, Exit );
		result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void *)&value, (void *)(UInt32)0, 0, 0 );
		funcSymbolName->release ();	// [3324205]
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
#endif
	return ( 0 != value );
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setSerialFormatRegister (UInt32 serialFormat) {
	IOReturn				result = kIOReturnError;

#ifdef kBUILD_FOR_DIRECT_I2S_HW_ACCESS
	if ( mSerialFormat ) {
		OSWriteSwapInt32 ( mSerialFormat, 0, serialFormat );
		result = kIOReturnSuccess;
	}
#else
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	result = kIOReturnError;
	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SSetSerialFormatReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		result = mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)serialFormat, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
#endif
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2Platform::getSerialFormatRegister () {
	UInt32					serialFormat = 0;

#ifdef kBUILD_FOR_DIRECT_I2S_HW_ACCESS
	if ( NULL != mSerialFormat ) serialFormat = (unsigned int)OSReadSwapInt32 ( mSerialFormat, 0 );
#else
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SGetSerialFormatReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)&serialFormat, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
#endif
	return serialFormat;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setDataWordSizes (UInt32 dataWordSizes) {
	IOReturn				result;

	result = kIOReturnError;
#ifdef kBUILD_FOR_DIRECT_I2S_HW_ACCESS
	if ( mDataWordSize ) {
		OSWriteSwapInt32 ( mDataWordSize, 0, dataWordSizes );
		result = kIOReturnSuccess;
	}
#else
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SSetDataWordSizesReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		result = mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)dataWordSizes, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
#endif
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2Platform::getDataWordSizes () {
	UInt32					dataWordSizes = 0;

#ifdef kBUILD_FOR_DIRECT_I2S_HW_ACCESS
	if ( NULL != mDataWordSize ) dataWordSizes = (unsigned int)OSReadSwapInt32 ( mDataWordSize, 0 );
#else
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SGetDataWordSizesReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)&dataWordSizes, 0, 0 );		//	[3323977]
		funcSymbolName->release ();																//	[3323977]	release OSSymbol references
	}
Exit:
#endif
	return dataWordSizes;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setI2SIOMIntControl (UInt32 intCntrl) {
	IOReturn				result;

	result = kIOReturnError;
#ifdef kBUILD_FOR_DIRECT_I2S_HW_ACCESS
	if ( mI2SIntCtrl ) {
		OSWriteSwapInt32 ( mI2SIntCtrl, 0, intCntrl );
		result = kIOReturnSuccess;
	}
#else
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SSetIntCtlReg );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		result = mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)intCntrl, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
#endif
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2Platform::getI2SIOMIntControl () {
	UInt32					i2sIntCntrl = 0;

#ifdef kBUILD_FOR_DIRECT_I2S_HW_ACCESS
	if ( NULL != mI2SIntCtrl ) i2sIntCntrl = (unsigned int)OSReadSwapInt32 ( mI2SIntCtrl, 0 );
#else
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SGetIntCtlReg );						//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)&i2sIntCntrl, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
#endif
	return i2sIntCntrl;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::setFrameCount (UInt32 value) {
	IOReturn				result;

	//	<<<<<<<<<< WARNING >>>>>>>>>>
	//	Do not debugIOLog in here it screws up the hal timing and causes stuttering audio
	//	<<<<<<<<<< WARNING >>>>>>>>>>
	
	result = kIOReturnError;
#ifdef kBUILD_FOR_DIRECT_I2S_HW_ACCESS
	if ( mFrameCounter ) {
		OSWriteSwapInt32 ( mFrameCounter, 0, value );
		result = kIOReturnSuccess;
	}
#else
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SSetFrameCountReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		result = mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)value, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
#endif
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 K2Platform::getFrameCount () {
	UInt32					frameCount = 0;

#ifdef kBUILD_FOR_DIRECT_I2S_HW_ACCESS
	if ( NULL != mFrameCounter ) frameCount = (unsigned int)OSReadSwapInt32 ( mFrameCounter, 0 );
#else
	const OSSymbol*			funcSymbolName = NULL;											//	[3323977]

	if ( mI2SInterface ) {
		funcSymbolName = OSSymbol::withCString ( kI2SGetFrameCountReg );					//	[3323977]
		FailIf ( NULL == funcSymbolName, Exit );											//	[3323977]
		mI2SInterface->callPlatformFunction ( funcSymbolName, false, (void *)mI2SOffset, (void *)&frameCount, 0, 0 );		//	[3323977]
		funcSymbolName->release ();															//	[3323977]	release OSSymbol references
	}
Exit:
#endif
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
#ifdef kBUILD_FOR_DIRECT_GPIO_HW_ACCESS
#pragma unused ( theDevice, interruptHandler, source )
	IOReturn				result = kIOReturnError;
#else
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
				default:							debugIrqIOLog ( "Attempt to disable unknown interrupt source\n" );								break;
			}
			FailIf ( NULL == enFuncSymbolName, Exit );
			FailIf (NULL == selector, Exit);
			result = mK2Service->callPlatformFunction ( enFuncSymbolName, true, interruptHandler, this, (void*)source, (void*)selector);
			selector->release ();
			enFuncSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
#endif
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
#ifdef kBUILD_FOR_DIRECT_GPIO_HW_ACCESS
#pragma unused ( theDevice, interruptHandler, source )
	IOReturn				result = kIOReturnError;
#else
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
				default:							debugIrqIOLog ( "Attempt to enable unknown interrupt source\n" );								break;
			}
			FailIf ( NULL == enFuncSymbolName, Exit );
			FailIf (NULL == selector, Exit);
			result = mK2Service->callPlatformFunction ( enFuncSymbolName, true, interruptHandler, this, (void*)source, (void*)selector);
			selector->release ();
			enFuncSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
#endif
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
bool K2Platform::interruptUsesTimerPolling( PlatformInterruptSource source ) {
	bool		result = false;
	
	if ( kCodecErrorInterrupt == source ) {
		result = true;
	}
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn K2Platform::registerInterruptHandler ( IOService * theDevice, void * interruptHandler, PlatformInterruptSource source ) {
#ifdef kBUILD_FOR_DIRECT_GPIO_HW_ACCESS
#pragma unused ( theDevice, interruptHandler, source )
	IOReturn				result = kIOReturnError;
#else
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
				default:							debugIOLog ( "Attempt to register unknown interrupt source\n" );								break;
			}
			FailIf ( NULL == funcSymbolName, Exit );
			FailIf (NULL == selector, Exit);
			result = mK2Service->callPlatformFunction ( funcSymbolName, true, interruptHandler, this, (void*)source, (void*)selector);
			selector->release ();
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
#endif
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
#ifdef kBUILD_FOR_DIRECT_GPIO_HW_ACCESS
#pragma unused ( theDevice, interruptHandler, source )
	IOReturn				result = kIOReturnError;
#else
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
			default:							debugIOLog ( "Attempt to register unknown interrupt source\n" );									break;
		}
		FailIf ( NULL == funcSymbolName, Exit );
		FailIf (NULL == selector, Exit);
		result = mK2Service->callPlatformFunction ( funcSymbolName, true, interruptHandler, theDevice, (void*)source, (void*)selector);
		selector->release ();
		funcSymbolName->release ();	// [3324205]
		FailIf ( kIOReturnSuccess != result, Exit );
	}

Exit:
#endif
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
	debug2IOLog ( "K2Platform::setClockMux ( %d )\n", muxState );
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
	return readGpioState ( kGPIO_Selector_DigitalInDetect );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getDigitalOutConnected() {
	return readGpioState ( kGPIO_Selector_DigitalOutDetect );
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
	debug2IOLog ( "K2Platform::setHeadphoneMuteState ( %d )\n", muteState );
	return writeGpioState ( kGPIO_Selector_HeadphoneMute, muteState );
}
	
//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getInputDataMux() {
	return readGpioState ( kGPIO_Selector_InputDataMux );
}

//	--------------------------------------------------------------------------------
IOReturn 	K2Platform::setInputDataMux(GpioAttributes muxState) {
	debug2IOLog ( "K2Platform::setInputDataMux ( %d )\n", muxState );
	return writeGpioState ( kGPIO_Selector_InputDataMux, muxState );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getLineInConnected() {
	return readGpioState ( kGPIO_Selector_LineInDetect );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getLineOutConnected() {
	return readGpioState ( kGPIO_Selector_LineOutDetect );
}

//	--------------------------------------------------------------------------------
GpioAttributes 	K2Platform::getLineOutMuteState() {
	return readGpioState ( kGPIO_Selector_LineOutMute );
}
	
//	--------------------------------------------------------------------------------
IOReturn 	K2Platform::setLineOutMuteState ( GpioAttributes muteState ) {
	debug2IOLog ( "K2Platform::setLineOutMuteState ( %d )\n", muteState );
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
	debug2IOLog ( "K2Platform::setSpeakerMuteState ( %d )\n", muteState );
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
	FailIf ( NULL == i2cServiceDictionary, Exit );
	
#ifdef kVERBOSE_LOG
	debugIOLog ( "about to waitForService on i2cServiceDictionary timeout = 5 seconds\n" );
#endif
	timeout.tv_sec = 5;
	timeout.tv_nsec = 0;
	i2cCandidate = IOService::waitForService ( i2cServiceDictionary, &timeout );
	if ( NULL == i2cCandidate ) {
		iterator = IOService::getMatchingServices ( i2cServiceDictionary );
		if ( NULL != iterator ) {
			do {
				theObject = iterator->getNextObject ();
				if ( theObject ) {
#ifdef kVERBOSE_LOG
					debug2IOLog("found theObject=%p\n",theObject);
#endif
					i2cCandidate = OSDynamicCast(IOService,theObject);
				}
			} while ( !found && NULL != theObject );
		} else {
#ifdef kVERBOSE_LOG
			debugIOLog(" NULL != iterator\n");
#endif
		}
	} else {
#ifdef kVERBOSE_LOG
		debug2IOLog ( "K2Platform::findAndAttachI2C i2cCandidate = %p ",i2cCandidate );
#endif
	}
	
	FailIf(NULL == i2cCandidate,Exit);
	
	mI2CInterface = (PPCI2CInterface*)i2cCandidate->getProperty ( i2cDriverName );
	FailIf ( NULL == mI2CInterface, Exit );

	mI2CInterface->retain();
	result = true;
Exit:
	if (iterator) iterator->release ();
//	if (i2cCandidate) i2cCandidate->release ();
	if (i2cDriverName) i2cDriverName->release ();
//	if (i2cServiceDictionary) i2cServiceDictionary->release ();
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

	i2sDriverName = OSSymbol::withCStringNoCopy ( "AppleI2S" );
	i2sServiceDictionary = IOService::resourceMatching ( i2sDriverName );
	FailIf ( NULL == i2sServiceDictionary, Exit );

	i2sCandidate = IOService::waitForService ( i2sServiceDictionary, &timeout );
	debug2IOLog ( "i2sServiceDictionary %p\n", i2sServiceDictionary );
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
	}
#ifdef kVERBOSE_LOG	//	{
	switch ( selector ) {
		case kGPIO_Selector_AnalogCodecReset:		debug2IOLog ( "... kGPIO_Selector_AnalogCodecReset returns %d from CACHE\n", result );								break;
		case kGPIO_Selector_ClockMux:				debug2IOLog ( "... kGPIO_Selector_ClockMux returns %d from CACHE\n", result );										break;
		case kGPIO_Selector_CodecInterrupt:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_CodecErrorInterrupt:	/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_ComboInJackType:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_ComboOutJackType:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_DigitalCodecReset:		debug2IOLog ( "... kGPIO_Selector_DigitalCodecReset returns %d from CACHE\n", result );								break;
		case kGPIO_Selector_DigitalInDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_DigitalOutDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_HeadphoneDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_HeadphoneMute:			debug2IOLog ( "... kGPIO_Selector_HeadphoneMute returns %d from CACHE\n", result );									break;
		case kGPIO_Selector_InputDataMux:			debug2IOLog ( "... kGPIO_Selector_InputDataMux returns %d from CACHE\n", result );									break;
		case kGPIO_Selector_InternalSpeakerID:		debug2IOLog ( "... kGPIO_Selector_InternalSpeakerID returns %d from CACHE\n", result );								break;
		case kGPIO_Selector_LineInDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_LineOutDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_LineOutMute:			debug2IOLog ( "... kGPIO_Selector_LineOutMute returns %d from CACHE\n", result );									break;
		case kGPIO_Selector_SpeakerDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_SpeakerMute:			debug2IOLog ( "... kGPIO_Selector_SpeakerMute returns %d from CACHE\n", result );									break;
	}
#endif	//	}
	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes K2Platform::readGpioState ( GPIOSelector selector ) {
	GpioAttributes		result;
	
	result = kGPIO_Unknown;
#ifdef kBUILD_FOR_DIRECT_GPIO_HW_ACCESS		//	{
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
#else	//	} else {
	UInt32				value;
	const OSSymbol*		funcSymbolName;
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
			default:										FailIf ( true, Exit );																								break;
		}
		if ( NULL != funcSymbolName ) {
			waitForFunction = kGPIO_Selector_HeadphoneDetect == selector ? true : false ;
			err = mK2Service->callPlatformFunction ( funcSymbolName , waitForFunction, (void*)&value, (void*)0, (void*)0, (void*)0 );
			funcSymbolName->release ();	// [3324205]
			if ( kIOReturnSuccess != err ) {
#ifdef kVERBOSE_LOG	//	{
				switch ( selector ) {
					case kGPIO_Selector_AnalogCodecReset:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetAudioHwReset returned %X NOT FOUND\n", err );			break;
					case kGPIO_Selector_ClockMux:				debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetCodecClockMux returned %X NOT FOUND\n", err );		break;
					case kGPIO_Selector_CodecInterrupt:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetCodecIRQ returned %X NOT FOUND\n", err );				break;
					case kGPIO_Selector_CodecErrorInterrupt:	debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetCodecErrorIRQ returned %X NOT FOUND\n", err );		break;
					case kGPIO_Selector_ComboInJackType:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetComboInJackType returned %X NOT FOUND\n", err );		break;
					case kGPIO_Selector_ComboOutJackType:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetComboOutJackType returned %X NOT FOUND\n", err );		break;
					case kGPIO_Selector_DigitalCodecReset:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetAudioDigHwReset returned %X NOT FOUND\n", err );		break;
					case kGPIO_Selector_DigitalInDetect:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetDigitalInDetect returned %X NOT FOUND\n", err );		break;
					case kGPIO_Selector_DigitalOutDetect:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetDigitalOutDetect returned %X NOT FOUND\n", err );		break;
					case kGPIO_Selector_HeadphoneDetect:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetHeadphoneDetect returned %X NOT FOUND\n", err );		break;
					case kGPIO_Selector_HeadphoneMute:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetHeadphoneMute returned %X NOT FOUND\n", err );		break;
					case kGPIO_Selector_InputDataMux:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetCodecInputDataMux returned %X NOT FOUND\n", err );	break;
					case kGPIO_Selector_InternalSpeakerID:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetInternalSpeakerID returned %X NOT FOUND\n", err );	break;
					case kGPIO_Selector_LineInDetect:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetLineInDetect returned %X NOT FOUND\n", err );			break;
					case kGPIO_Selector_LineOutDetect:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetLineOutDetect returned %X NOT FOUND\n", err );		break;
					case kGPIO_Selector_LineOutMute:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetLineOutMute returned %X NOT FOUND\n", err );			break;
					case kGPIO_Selector_SpeakerDetect:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetSpeakerDetect returned %X NOT FOUND\n", err );		break;
					case kGPIO_Selector_SpeakerMute:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetAmpMute returned %X NOT FOUND\n", err );				break;
					default:									debug3IOLog ( "... callPlatformFunction for UNKNOWN SELECTOR %d returned %X NOT FOUND\n", selector, err );		break;
				}
#endif	//	}
				result = GetCachedAttribute ( selector, result );
			} else {
#ifdef kVERBOSE_LOG	//	{
				switch ( selector ) {
					case kGPIO_Selector_AnalogCodecReset:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_SetAudioHwReset returned %ld\n", value );				break;
					case kGPIO_Selector_ClockMux:				debug2IOLog ( "... callPlatformFunction for kAppleGPIO_SetCodecClockMux returned %ld\n", value );				break;
					case kGPIO_Selector_CodecInterrupt:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetCodecIRQ returned %ld\n", value );					break;
					case kGPIO_Selector_CodecErrorInterrupt:	debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetCodecErrorIRQ returned %ld\n", value );				break;
					case kGPIO_Selector_ComboInJackType:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetComboInJackType returned %ld\n", value );				break;
					case kGPIO_Selector_ComboOutJackType:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetComboOutJackType returned %ld\n", value );			break;
					case kGPIO_Selector_DigitalCodecReset:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_SetAudioDigHwReset returned %ld\n", value );				break;
					case kGPIO_Selector_DigitalInDetect:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetDigitalInDetect returned %ld\n", value );				break;
					case kGPIO_Selector_DigitalOutDetect:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetDigitalOutDetect returned %ld\n", value );			break;
					case kGPIO_Selector_HeadphoneDetect:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetHeadphoneDetect returned %ld\n", value );				break;
					case kGPIO_Selector_HeadphoneMute:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_SetHeadphoneMute returned %ld\n", value );				break;
					case kGPIO_Selector_InputDataMux:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_SetCodecInputDataMux returned %ld\n", value );			break;
					case kGPIO_Selector_InternalSpeakerID:		debug2IOLog ( "... callPlatformFunction for kAppleGPIO_SetInternalSpeakerID returned %ld\n", value );			break;
					case kGPIO_Selector_LineInDetect:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetLineInDetect returned %ld\n", value );				break;
					case kGPIO_Selector_LineOutDetect:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetLineOutDetect returned %ld\n", value );				break;
					case kGPIO_Selector_LineOutMute:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_SetLineOutMute returned %ld\n", value );					break;
					case kGPIO_Selector_SpeakerDetect:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_GetSpeakerDetect returned %ld\n", value );				break;
					case kGPIO_Selector_SpeakerMute:			debug2IOLog ( "... callPlatformFunction for kAppleGPIO_SetAmpMute returned %ld\n", value );						break;
					default:									debug3IOLog ( "... callPlatformFunction for UNKNOWN SELECTOR %d returned %ld\n", selector, value );				break;
				}
#endif	//	}
				//	Translate the GPIO state to a GPIO attribute
				switch ( selector ) {
					case kGPIO_Selector_AnalogCodecReset:		result = value ? kGPIO_Reset : kGPIO_Run;																		break;
					case kGPIO_Selector_ClockMux:				result = value ? kGPIO_MuxSelectAlternate : kGPIO_MuxSelectDefault;												break;
					case kGPIO_Selector_CodecInterrupt:			result = value ? kGPIO_CodecInterruptActive : kGPIO_CodecInterruptInactive;										break;
					case kGPIO_Selector_CodecErrorInterrupt:	result = value ? kGPIO_CodecInterruptActive : kGPIO_CodecInterruptInactive;										break;
					case kGPIO_Selector_ComboInJackType:		result = value ? kGPIO_TypeIsDigital : kGPIO_TypeIsAnalog;														break;
					case kGPIO_Selector_ComboOutJackType:		result = value ? kGPIO_TypeIsDigital : kGPIO_TypeIsAnalog;														break;
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
					case kGPIO_Selector_SpeakerDetect:			result = value ? kGPIO_Muted : kGPIO_Unmuted;																	break;
					case kGPIO_Selector_SpeakerMute:			result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
				}
#ifdef kVERBOSE_LOG	//	{
				switch ( selector ) {
					case kGPIO_Selector_AnalogCodecReset:		debug2IOLog ( "... kGPIO_Selector_AnalogCodecReset returns %d from callPlatformFunction\n", result );				break;
					case kGPIO_Selector_ClockMux:				debug2IOLog ( "... kGPIO_Selector_ClockMux returns %d from callPlatformFunction\n", result );						break;
					case kGPIO_Selector_CodecInterrupt:			debug2IOLog ( "... kGPIO_Selector_CodecInterrupt returns %d from callPlatformFunction\n", result );					break;
					case kGPIO_Selector_CodecErrorInterrupt:	debug2IOLog ( "... kGPIO_Selector_CodecErrorInterrupt returns %d from callPlatformFunction\n", result );			break;
					case kGPIO_Selector_ComboInJackType:		debug2IOLog ( "... kGPIO_Selector_ComboInJackType returns %d from callPlatformFunction\n", result );				break;
					case kGPIO_Selector_ComboOutJackType:		debug2IOLog ( "... kGPIO_Selector_ComboOutJackType returns %d from callPlatformFunction\n", result );				break;
					case kGPIO_Selector_DigitalCodecReset:		debug2IOLog ( "... kGPIO_Selector_DigitalCodecReset returns %d from callPlatformFunction\n", result );				break;
					case kGPIO_Selector_DigitalInDetect:		debug2IOLog ( "... kGPIO_Selector_DigitalInDetect returns %d from callPlatformFunction\n", result );				break;
					case kGPIO_Selector_DigitalOutDetect:		debug2IOLog ( "... kGPIO_Selector_DigitalOutDetect returns %d from callPlatformFunction\n", result );				break;
					case kGPIO_Selector_HeadphoneDetect:		debug2IOLog ( "... kGPIO_Selector_HeadphoneDetect returns %d from callPlatformFunction\n", result );				break;
					case kGPIO_Selector_HeadphoneMute:			debug2IOLog ( "... kGPIO_Selector_HeadphoneMute returns %d from callPlatformFunction\n", result );					break;
					case kGPIO_Selector_InputDataMux:			debug2IOLog ( "... kGPIO_Selector_InputDataMux returns %d from callPlatformFunction\n", result );					break;
					case kGPIO_Selector_InternalSpeakerID:		debug2IOLog ( "... kGPIO_Selector_InternalSpeakerID returns %d from callPlatformFunction\n", result );				break;
					case kGPIO_Selector_LineInDetect:			debug2IOLog ( "... kGPIO_Selector_LineInDetect returns %d from callPlatformFunction\n", result );					break;
					case kGPIO_Selector_LineOutDetect:			debug2IOLog ( "... kGPIO_Selector_LineOutDetect returns %d from callPlatformFunction\n", result );					break;
					case kGPIO_Selector_LineOutMute:			debug2IOLog ( "... kGPIO_Selector_LineOutMute returns %d from callPlatformFunction\n", result );					break;
					case kGPIO_Selector_SpeakerDetect:			debug2IOLog ( "... kGPIO_Selector_SpeakerDetect returns %d from callPlatformFunction\n", result );					break;
					case kGPIO_Selector_SpeakerMute:			debug2IOLog ( "... kGPIO_Selector_SpeakerMute returns %d from callPlatformFunction\n", result );					break;
				}
#endif	//	}
			}
		} else {
			result = GetCachedAttribute ( selector, result );
		}
	}
#endif	//	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn K2Platform::writeGpioState ( GPIOSelector selector, GpioAttributes gpioState ) {
	UInt32				value;
	IOReturn			result;
#ifndef kBUILD_FOR_DIRECT_GPIO_HW_ACCESS
	const OSSymbol*		funcSymbolName;
#endif

	result = kIOReturnError;
	value = 0;
#ifdef kBUILD_FOR_DIRECT_GPIO_HW_ACCESS	//	{
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
#else	//	} else {
	if ( mK2Service ) {
		//	Translate GPIO attribute to gpio state
		switch ( selector ) {
			case kGPIO_Selector_AnalogCodecReset:		FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Reset, gpioState, &value ), Exit );	break;
			case kGPIO_Selector_ClockMux:				FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Mux, gpioState, &value ), Exit );	break;
			case kGPIO_Selector_DigitalCodecReset:		FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Reset, gpioState, &value ), Exit );	break;
			case kGPIO_Selector_HeadphoneMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, gpioState, &value ), Exit );	break;
			case kGPIO_Selector_InputDataMux:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Mux, gpioState, &value ), Exit );	break;
			case kGPIO_Selector_LineOutMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, gpioState, &value ), Exit );	break;
			case kGPIO_Selector_SpeakerMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteL, gpioState, &value ), Exit );	break;
			default:									FailIf ( true, Exit );																							break;
		}
		switch ( selector ) {
			case kGPIO_Selector_AnalogCodecReset:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetAudioHwReset, mI2SPHandle );					break;
			case kGPIO_Selector_ClockMux:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetCodecClockMux, mI2SPHandle );				break;
			case kGPIO_Selector_DigitalCodecReset:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetAudioDigHwReset, mI2SPHandle );				break;
			case kGPIO_Selector_HeadphoneMute:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetHeadphoneMute, mI2SPHandle );				break;
			case kGPIO_Selector_InputDataMux:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetCodecInputDataMux, mI2SPHandle );			break;
			case kGPIO_Selector_LineOutMute:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetLineOutMute, mI2SPHandle );					break;
			case kGPIO_Selector_SpeakerMute:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_SetAmpMute, mI2SPHandle );						break;
			default:																																		break;
		}
		if ( NULL != funcSymbolName ) {
			result = mK2Service->callPlatformFunction ( funcSymbolName, false, (void*)value, (void*)0, (void*)0, (void*)0 );
			funcSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
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
#ifdef kVERBOSE_LOG	//	{
			switch ( selector ) {
				case kGPIO_Selector_AnalogCodecReset:	debug2IOLog ( "... mAppleGPIO_AnalogCodecReset CACHE updated to %d\n", gpioState );					break;
				case kGPIO_Selector_ClockMux:			debug2IOLog ( "... mAppleGPIO_CodecClockMux CACHE updated to %d\n", gpioState );					break;
				case kGPIO_Selector_DigitalCodecReset:	debug2IOLog ( "... mAppleGPIO_DigitalCodecReset CACHE updated to %d\n", gpioState );				break;
				case kGPIO_Selector_HeadphoneMute:		debug2IOLog ( "... mAppleGPIO_HeadphoneMute CACHE updated to %d\n", gpioState );					break;
				case kGPIO_Selector_InputDataMux:		debug2IOLog ( "... mAppleGPIO_CodecInputDataMux CACHE updated to %d\n", gpioState );				break;
				case kGPIO_Selector_LineOutMute:		debug2IOLog ( "... mAppleGPIO_LineOutMute CACHE updated to %d\n", gpioState );						break;
				case kGPIO_Selector_SpeakerMute:		debug2IOLog ( "... mAppleGPIO_AmpMute CACHE updated to %d\n", gpioState );							break;
				default:																																	break;
			}
#endif	//	}
		}
	}
#endif	//	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
//	Convert a GPIOAttribute to a binary value to be applied to a GPIO pin level
IOReturn K2Platform::translateGpioAttributeToGpioState ( GPIOType gpioType, GpioAttributes gpioAttribute, UInt32 * valuePtr ) {
	IOReturn		result;
	
//	debug4IOLog ( "K2Platform::translateGpioAttributeToGpioState ( %d, %d, %p )\n", (unsigned int)gpioType, (unsigned int)gpioAttribute, valuePtr );
#ifndef kBUILD_FOR_DIRECT_GPIO_HW_ACCESS	
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
#else	// use this for direct hardware access (takes into account active high/low of gpio's)
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
#endif
#ifdef kVERBOSE_Log
	if ( kIOReturnSuccess != result ) { 
		switch ( gpioType ) {
			case kGPIO_Type_ConnectorType:	debug2IOLog ( "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_ConnectorType, %p )\n", valuePtr ); 	break;
			case kGPIO_Type_Detect:			debug2IOLog ( "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_Detect, %p )\n", valuePtr ); 			break;
			case kGPIO_Type_Irq:			debug2IOLog ( "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_Irq, %p )\n", valuePtr ); 				break;
			case kGPIO_Type_MuteL:			debug2IOLog ( "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_MuteL, %p )\n", valuePtr ); 			break;
			case kGPIO_Type_MuteH:			debug2IOLog ( "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, %p )\n", valuePtr ); 			break;
			case kGPIO_Type_Mux:			debug2IOLog ( "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_Mux, %p )\n", valuePtr ); 				break;
			case kGPIO_Type_Reset:			debug2IOLog ( "FAIL: K2Platform::translateGpioAttributeToGpioState ( kGPIO_Type_Reset, %p )\n", valuePtr ); 			break;
			default:						debug3IOLog ( "FAIL: K2Platform::translateGpioAttributeToGpioState ( %X, %p )\n", gpioType, valuePtr ); 				break;
		}
	}
#endif
	return result;
}

#pragma mark ---------------------------
#pragma mark DBDMA Memory Address Acquisition Methods
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	K2Platform::GetInputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) {
	IOMemoryMap *				map;
	IOService *					parentOfParent;
	IOPhysicalAddress			ioPhysAddr;
	IOMemoryDescriptor *		theDescriptor;
	IOByteCount					length = 256;
	
	mIOBaseDMAInput = NULL;
	FailIf ( NULL == dbdmaProvider, Exit );
	debug2IOLog ( "K2Platform::GetInputChannelRegistersVirtualAddress i2s-a name is %s\n", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	debug2IOLog ( "   parentOfParent name is %s\n", parentOfParent->getName() );
	map = parentOfParent->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAOutputIndex );
	FailIf ( NULL == map, Exit );
	ioPhysAddr = map->getPhysicalSegment( kIODMAInputOffset, &length );
	FailIf ( NULL == ioPhysAddr, Exit );
	theDescriptor = IOMemoryDescriptor::withPhysicalAddress ( ioPhysAddr, 256, kIODirectionOutIn );
	FailIf ( NULL == theDescriptor, Exit );
	map = theDescriptor->map ();
	FailIf ( NULL == map, Exit );
	mIOBaseDMAInput = (IODBDMAChannelRegisters*)map->getVirtualAddress();
	
	debug2IOLog ( "mIOBaseDMAInput %p\n", mIOBaseDMAInput );
	if ( NULL == mIOBaseDMAInput ) { IOLog ( "K2Platform::GetInputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE\n" ); }
Exit:
	return mIOBaseDMAInput;
}

//	--------------------------------------------------------------------------------
IODBDMAChannelRegisters *	K2Platform::GetOutputChannelRegistersVirtualAddress ( IOService * dbdmaProvider ) {
	IOMemoryMap *				map;
	IOService *					parentOfParent;
	
	mIOBaseDMAOutput = NULL;
	FailIf ( NULL == dbdmaProvider, Exit );
	debug2IOLog ( "K2Platform::GetOutputChannelRegistersVirtualAddress i2s-a name is %s\n", dbdmaProvider->getName() );
	parentOfParent = (IOService*)dbdmaProvider->getParentEntry ( gIODTPlane );
	FailIf ( NULL == parentOfParent, Exit );
	debug2IOLog ( "   parentOfParent name is %s\n", parentOfParent->getName() );
	map = parentOfParent->mapDeviceMemoryWithIndex ( AppleDBDMAAudio::kDBDMAOutputIndex );
	FailIf ( NULL == map, Exit );
	mIOBaseDMAOutput = (IODBDMAChannelRegisters*)map->getVirtualAddress();
	
	debug3IOLog ( "mIOBaseDMAOutput %p is at physical %p\n", mIOBaseDMAOutput, (void*)map->getPhysicalAddress() );
	if ( NULL == mIOBaseDMAOutput ) { IOLog ( "K2Platform::GetOutputChannelRegistersVirtualAddress IODBDMAChannelRegisters NOT IN VIRTUAL SPACE\n" ); }
Exit:
	return mIOBaseDMAOutput;
}

#pragma mark ---------------------------
#pragma mark DEBUGGING UTILITIES
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
void K2Platform::LogFCR ( void ) {
	UInt32			data;
	
//#ifdef kVERBOSE_LOG
	data = (unsigned int)OSReadSwapInt32 ( mHwPtr, kAUDIO_MAC_IO_FCR1 );
	debug7IOLog ( "[#] FCR1: %X -> %lX ::: %s %s %s %s\n", 
						kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_MAC_IO_FCR1, 
						data,
						0 == ( data & ( 1 << 13 ) ) ? "i2s0_enable_h" : "I2S0_ENABLE_H" ,
						0 == ( data & ( 1 << 12 ) ) ? "i2s0_clkenbit_h" : "I2S0_CLKENBIT_H" ,
						0 == ( data & ( 1 << 11 ) ) ? "i2s0_swreset_h" : "I2S0_SWRESET_H" ,
						0 == ( data & ( 1 << 10 ) ) ? "i2s0_cell_en_h" : "I2S0_CELL_EN_H"
					);
	data = (unsigned int)OSReadSwapInt32 ( mHwPtr, kAUDIO_MAC_IO_FCR3 );
	debug8IOLog ( "[#] FCR3: %X -> %lX ::: %s %s %s %s %s\n", 
						kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_MAC_IO_FCR3, 
						data,
						0 == ( data & ( 1 << 14 ) ) ? "i2s0_clk18_en_h" : "I2S_CLK18_EN_H" ,
						0 == ( data & ( 1 << 10 ) ) ? "clk45_en_h" : "CLK45_EN_H" ,
						0 == ( data & ( 1 << 9 ) ) ? "clk49_en_h" : "CLK49_EN_H" ,
						0 == ( data & ( 1 << 5 ) ) ? "enablepll2shutdown_h" : "ENABLEPLL2SHUTDOWN_H",
						0 == ( data & ( 1 << 5 ) ) ? "enablepll3shutdown_h" : "ENABLEPLL3SHUTDOWN_H"
					);
	IOSleep ( 10 );
//#endif
	return;
}

//	--------------------------------------------------------------------------------
void K2Platform::LogI2S ( void ) {
//#ifdef kVERBOSE_LOG
	debug3IOLog ( "[*] I2S Serial Format:     %X -> %X\n", kAUDIO_I2S_BASE_ADDRESS + kAUDIO_I2S_SERIAL_FORMAT, (unsigned int)OSReadSwapInt32 ( mHwI2SPtr, kAUDIO_I2S_SERIAL_FORMAT ) );
	debug3IOLog ( "[*] I2S Data Word Size:    %X -> %X\n", kAUDIO_I2S_BASE_ADDRESS + kAUDIO_I2S_DATA_WORD_SIZES, (unsigned int)OSReadSwapInt32 ( mHwI2SPtr, kAUDIO_I2S_DATA_WORD_SIZES ) );
	debug3IOLog ( "[*] I2S Frame Count:       %X -> %X\n", kAUDIO_I2S_BASE_ADDRESS + kAUDIO_I2S_FRAME_COUNTER, (unsigned int)OSReadSwapInt32 ( mHwI2SPtr, kAUDIO_I2S_FRAME_COUNTER ) );
	debug3IOLog ( "[*] I2S Interrupt Control: %X -> %X\n", kAUDIO_I2S_BASE_ADDRESS + kAUDIO_I2S_INTERRUPT_CONTROL, (unsigned int)OSReadSwapInt32 ( mHwI2SPtr, kAUDIO_I2S_INTERRUPT_CONTROL ) );
//#endif
	return;
}

//	--------------------------------------------------------------------------------
void K2Platform::LogInterruptGPIO ( void ) {
#ifdef kVERBOSE_LOG
	IOLog ( "### CodecErrorIRQ %X, CodecIRQ %X, HeadphoneDet %X, LineInDet %X, LineOutDet %X\n",
		(unsigned int)mHwPtr[kAUDIO_GPIO_CODEC_ERROR_IRQ], 
		(unsigned int)mHwPtr[kAUDIO_GPIO_CODEC_IRQ], 
		(unsigned int)mHwPtr[kAUDIO_GPIO_HEADPHONE_SENSE], 
		(unsigned int)mHwPtr[kAUDIO_GPIO_LINE_IN_SENSE], 
		(unsigned int)mHwPtr[kAUDIO_GPIO_LINE_OUT_SENSE] 
	);
#endif
	return;
}
	
//	--------------------------------------------------------------------------------
void K2Platform::LogGPIO ( void ) {
#ifdef kVERBOSE_LOG
	debug3IOLog ( "GPIO28 AnalogReset:         %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_ANALOG_CODEC_RESET, (unsigned int)mHwPtr[kAUDIO_GPIO_ANALOG_CODEC_RESET] );
	debug3IOLog ( "GPIO30 ClockMuxSelect:      %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_CLOCK_MUX_SELECT, (unsigned int)mHwPtr[kAUDIO_GPIO_CLOCK_MUX_SELECT] );
	debug3IOLog ( "GPIO5  CodecErrorInterrupt: %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_CODEC_ERROR_IRQ, (unsigned int)mHwPtr[kAUDIO_GPIO_CODEC_ERROR_IRQ] );
	debug3IOLog ( "GPIO16 CodecInterrupt:      %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_CODEC_IRQ, (unsigned int)mHwPtr[kAUDIO_GPIO_CODEC_IRQ] );
	debug3IOLog ( "GPIO12 DigitalCodecReset:   %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_DIGITAL_CODEC_RESET, (unsigned int)mHwPtr[kAUDIO_GPIO_DIGITAL_CODEC_RESET] );
	debug3IOLog ( "GPIO15 HeadphoneDetect:     %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_HEADPHONE_SENSE, (unsigned int)mHwPtr[kAUDIO_GPIO_HEADPHONE_SENSE] );
	debug3IOLog ( "GPIO23 HeadphoneMute:       %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_HEADPHONE_MUTE, (unsigned int)mHwPtr[kAUDIO_GPIO_HEADPHONE_MUTE] );
	debug3IOLog ( "GPIO3  InputDataMuxSelect:  %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_INPUT_DATA_MUX_SELECT, (unsigned int)mHwPtr[kAUDIO_GPIO_INPUT_DATA_MUX_SELECT] );
	debug3IOLog ( "GPIO4  LineInDetect:        %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_LINE_IN_SENSE, (unsigned int)mHwPtr[kAUDIO_GPIO_LINE_IN_SENSE] );
	debug3IOLog ( "GPIO14 LineOutDetect:       %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_LINE_OUT_SENSE, (unsigned int)mHwPtr[kAUDIO_GPIO_LINE_OUT_SENSE] );
	debug3IOLog ( "GPIO29 LineOutMute:         %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_LINE_OUT_MUTE, (unsigned int)mHwPtr[kAUDIO_GPIO_LINE_OUT_MUTE] );
	debug3IOLog ( "GPIO24 SpeakerMute:         %X -> %X\n", kAUDIO_MAC_IO_BASE_ADDRESS + kAUDIO_GPIO_AMPLIFIER_MUTE, (unsigned int)mHwPtr[kAUDIO_GPIO_AMPLIFIER_MUTE] );
#endif
	return;
}
	
//	--------------------------------------------------------------------------------
void K2Platform::LogDBDMAChannelRegisters ( void ) {
#ifdef kVERBOSE_LOG
	debug2IOLog ( "Output ChannelControl:  %lX\n", mIOBaseDMAOutput->channelControl );
	debug2IOLog ( "Output channelStatus:   %lX\n", mIOBaseDMAOutput->channelStatus );
	debug2IOLog ( "Output commandPtrHi:    %lX\n", mIOBaseDMAOutput->commandPtrHi );
	debug2IOLog ( "Output commandPtrLo:    %lX\n", mIOBaseDMAOutput->commandPtrLo );
	debug2IOLog ( "Output interruptSelect: %lX\n", mIOBaseDMAOutput->interruptSelect );
	debug2IOLog ( "Output branchSelect:    %lX\n", mIOBaseDMAOutput->branchSelect );
	debug2IOLog ( "Output waitSelect:      %lX\n", mIOBaseDMAOutput->waitSelect );
	debug2IOLog ( "Output transferModes:   %lX\n", mIOBaseDMAOutput->transferModes );
	debug2IOLog ( "Output data2PtrHi:      %lX\n", mIOBaseDMAOutput->data2PtrHi );
	debug2IOLog ( "Output data2PtrLo:      %lX\n", mIOBaseDMAOutput->data2PtrLo );
	debug2IOLog ( "Output reserved1:       %lX\n", mIOBaseDMAOutput->reserved1 );
	debug2IOLog ( "Output addressHi:       %lX\n", mIOBaseDMAOutput->addressHi );
#endif
	return;
}


#pragma mark ---------------------------
#pragma mark USER CLIENT SUPPORT
#pragma mark ---------------------------

//	--------------------------------------------------------------------------------
IOReturn	K2Platform::getPlatformState ( PlatformStateStructPtr outState ) {
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
#ifdef kVERBOSE_LOG
	IOLog ( "outState->gpio: %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d \n",
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
#endif	
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
		debug2IOLog ( "K2Platform::setPlatformState setI2SIOMIntControl ( %lX )\n", inState->i2s.intCtrl );
		result = setI2SIOMIntControl ( inState->i2s.intCtrl );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->i2s.serialFmt != getSerialFormatRegister () ) {
		debug2IOLog ( "K2Platform::setPlatformState setSerialFormatRegister ( %lX )\n", inState->i2s.serialFmt );
		result = setSerialFormatRegister ( inState->i2s.serialFmt );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( inState->i2s.frameCount != getFrameCount () ) && ( 0 == inState->i2s.frameCount ) ) {
		debug2IOLog ( "K2Platform::setPlatformState setFrameCount ( %lX )\n", inState->i2s.frameCount );
		result = setFrameCount ( inState->i2s.frameCount );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->i2s.dataWordSizes != getDataWordSizes () ) {
		debug2IOLog ( "K2Platform::setPlatformState setDataWordSizes ( %lX )\n", inState->i2s.dataWordSizes );
		result = setDataWordSizes ( inState->i2s.dataWordSizes );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	
	if ( inState->fcr.i2sEnable != getI2SEnable () ) {
		debug2IOLog ( "K2Platform::setPlatformState setI2SEnable ( %lX )\n", inState->fcr.i2sEnable );
		result = setI2SEnable ( inState->fcr.i2sEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->fcr.i2sClockEnable != getI2SClockEnable () ) {
		debug2IOLog ( "K2Platform::setPlatformState setI2SClockEnable ( %lX )\n", inState->fcr.i2sClockEnable );
		result = setI2SClockEnable ( inState->fcr.i2sClockEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( inState->fcr.i2sCellEnable != getI2SCellEnable () ) {
		debug2IOLog ( "K2Platform::setPlatformState setI2SCellEnable ( %lX )\n", inState->fcr.i2sCellEnable );
		result = setI2SCellEnable ( inState->fcr.i2sCellEnable );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	
	if ( ( kGPIO_Unknown != inState->gpio.gpio_AnalogCodecReset ) && ( inState->gpio.gpio_DigitalCodecReset != getCodecReset ( kCODEC_RESET_Analog ) ) ) {
		debug2IOLog ( "K2Platform::setPlatformState setCodecReset ( kCODEC_RESET_Analog, %d )\n", inState->gpio.gpio_DigitalCodecReset );
		result = setCodecReset ( kCODEC_RESET_Analog, inState->gpio.gpio_AnalogCodecReset );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_ClockMux ) && ( inState->gpio.gpio_ClockMux != getClockMux () ) ) {
		debug2IOLog ( "K2Platform::setPlatformState setClockMux ( %d )\n", inState->gpio.gpio_ClockMux );
		result = setClockMux ( inState->gpio.gpio_ClockMux );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_DigitalCodecReset ) && ( inState->gpio.gpio_DigitalCodecReset != getCodecReset ( kCODEC_RESET_Digital ) ) ) {
		debug2IOLog ( "K2Platform::setPlatformState setCodecReset ( kCODEC_RESET_Digital, %d )\n", inState->gpio.gpio_DigitalCodecReset );
		result = setCodecReset ( kCODEC_RESET_Digital, inState->gpio.gpio_DigitalCodecReset );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_HeadphoneMute ) && ( inState->gpio.gpio_HeadphoneMute != getHeadphoneMuteState () ) ) {
		debug2IOLog ( "K2Platform::setPlatformState setHeadphoneMuteState ( %d )\n", inState->gpio.gpio_HeadphoneMute );
		result = setHeadphoneMuteState ( inState->gpio.gpio_HeadphoneMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_InputDataMux ) && ( inState->gpio.gpio_InputDataMux != getInputDataMux () ) ) {
		debug2IOLog ( "K2Platform::setPlatformState setInputDataMux ( %d )\n", inState->gpio.gpio_InputDataMux );
		result = setInputDataMux ( inState->gpio.gpio_InputDataMux );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_LineOutMute ) && ( inState->gpio.gpio_LineOutMute != getLineOutMuteState () ) ) {
		debug2IOLog ( "K2Platform::setPlatformState setLineOutMuteState ( %d )\n", inState->gpio.gpio_LineOutMute );
		result = setLineOutMuteState ( inState->gpio.gpio_LineOutMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	if ( ( kGPIO_Unknown != inState->gpio.gpio_SpeakerMute ) && ( inState->gpio.gpio_SpeakerMute != getSpeakerMuteState () ) ) {
		debug2IOLog ( "K2Platform::setPlatformState setSpeakerMuteState ( %d )\n", inState->gpio.gpio_SpeakerMute );
		result = setSpeakerMuteState ( inState->gpio.gpio_SpeakerMute );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}







