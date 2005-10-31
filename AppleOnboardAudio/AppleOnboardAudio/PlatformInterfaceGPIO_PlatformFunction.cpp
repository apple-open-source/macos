/*
 *	PlatformInterfaceGPIO_PlatformFunction.cpp
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include "PlatformInterfaceGPIO_PlatformFunction.h"

#define super PlatformInterfaceGPIO

OSDefineMetaClassAndStructors ( PlatformInterfaceGPIO_PlatformFunction, PlatformInterfaceGPIO )

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_SetAmpMute						= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetAmpMute;					
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetAmpMute						= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetAmpMute;					

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_SetAudioHwReset					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetAudioHwReset;				
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetAudioHwReset					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetAudioHwReset;					

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_SetCodecClockMux					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetCodecClockMux;				
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetCodecClockMux					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetCodecClockMux;				

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_EnableCodecErrorIRQ				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableCodecErrorIRQ;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_DisableCodecErrorIRQ				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableCodecErrorIRQ;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetCodecErrorIRQ					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetCodecErrorIRQ;				
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_RegisterCodecErrorIRQ			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterCodecErrorIRQ;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_UnregisterCodecErrorIRQ			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterCodecErrorIRQ;		

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_DisableCodecIRQ					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableCodecIRQ;				
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_EnableCodecIRQ					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableCodecIRQ;				
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetCodecIRQ						= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetCodecIRQ;					
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_RegisterCodecIRQ					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterCodecIRQ;				
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_UnregisterCodecIRQ				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterCodecIRQ;			

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_SetCodecInputDataMux				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetCodecInputDataMux;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetCodecInputDataMux				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetCodecInputDataMux;			

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetComboInJackType				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetComboInJackType;		
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_DisableComboInSense				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableComboInSense;
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_EnableComboInSense				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableComboInSense;
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_RegisterComboInSense				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterComboInSense;
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_UnregisterComboInSense			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterComboInSense;

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetComboOutJackType				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetComboOutJackType;
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_DisableComboOutSense				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableComboOutSense;
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_EnableComboOutSense				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableComboOutSense;
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_RegisterComboOutSense			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterComboOutSense;
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_UnregisterComboOutSense			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterComboOutSense;

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_SetAudioDigHwReset				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetAudioDigHwReset;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetAudioDigHwReset				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetAudioDigHwReset;			

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_DisableDigitalInDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableDigitalInDetect;		
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_EnableDigitalInDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableDigitalInDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetDigitalInDetect				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetDigitalInDetect;		
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_RegisterDigitalInDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterDigitalInDetect;		
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_UnregisterDigitalInDetect		= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterDigitalInDetect;		

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_DisableDigitalOutDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableDigitalOutDetect;		
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_EnableDigitalOutDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableDigitalOutDetect;		
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetDigitalOutDetect				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetDigitalOutDetect;	
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_RegisterDigitalOutDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterDigitalOutDetect;		
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_UnregisterDigitalOutDetect		= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterDigitalOutDetect;	
	
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_DisableHeadphoneDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableHeadphoneDetect;		
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_EnableHeadphoneDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableHeadphoneDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetHeadphoneDetect				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetHeadphoneDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_RegisterHeadphoneDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterHeadphoneDetect;		
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_UnregisterHeadphoneDetect		= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterHeadphoneDetect;		

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_SetHeadphoneMute					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetHeadphoneMute;				
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetHeadphoneMute					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetHeadphoneMute;				

const char *	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetInternalMicrophoneID			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetInternalMicrophoneID;

const char *	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetInternalSpeakerID				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetInternalSpeakerID;

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_DisableLineInDetect				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableLineInDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_EnableLineInDetect				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableLineInDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetLineInDetect					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetLineInDetect;				
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_RegisterLineInDetect				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterLineInDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_UnregisterLineInDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterLineInDetect;		

const char *	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_DisableLineOutDetect				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableLineOutDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_EnableLineOutDetect				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableLineOutDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetLineOutDetect					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetLineOutDetect;				
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_RegisterLineOutDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterLineOutDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_UnregisterLineOutDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterLineOutDetect;		

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_SetLineOutMute					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_SetLineOutMute;				
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetLineOutMute					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetLineOutMute;				

const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_DisableSpeakerDetect				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_DisableSpeakerDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_EnableSpeakerDetect				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_EnableSpeakerDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_GetSpeakerDetect					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_GetSpeakerDetect;
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_RegisterSpeakerDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_RegisterSpeakerDetect;			
const char * 	PlatformInterfaceGPIO_PlatformFunction::kAppleGPIO_UnregisterSpeakerDetect			= kPlatformInterfaceSupportPlatformFunctionCommon_AppleGPIO_UnregisterSpeakerDetect;

#pragma mark ¥
#pragma mark ¥ UNIX Like Functions
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceGPIO_PlatformFunction::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex )
{
	IORegistryEntry			*sound;
	bool					result;
	OSData					*osdata;
	IORegistryEntry			*i2s;
	IORegistryEntry			*macIO;
	
	debugIOLog (3,  "+ PlatformInterfaceGPIO_PlatformFunction[%p]::init", this );
	result = super::init ( device, provider, inDBDMADeviceIndex );
	FailIf ( !result, Exit );

	sound = device;
	FailWithAction ( !sound, result = FALSE, Exit );

	mI2S = sound->getParentEntry (gIODTPlane);
	FailWithAction (!mI2S, result = FALSE, Exit);
	if ( 0 == strcmp ( mI2S->getName (), "i2s-a" ) )
	{
		mI2SInterfaceNumber = kUseI2SCell0;
	}
	else if ( 0 == strcmp ( mI2S->getName (), "i2s-b" ) )
	{
		mI2SInterfaceNumber = kUseI2SCell1;
	}
	else if ( 0 == strcmp ( mI2S->getName (), "i2s-c" ) )
	{
		mI2SInterfaceNumber = kUseI2SCell2;
	}
	else if ( 0 == strcmp ( mI2S->getName (), "i2s-d" ) )
	{
		mI2SInterfaceNumber = kUseI2SCell3;
	}
	else if ( 0 == strcmp ( mI2S->getName (), "i2s-e" ) )
	{
		mI2SInterfaceNumber = kUseI2SCell4;
	}
	else if ( 0 == strcmp ( mI2S->getName (), "i2s-f" ) )
	{
		mI2SInterfaceNumber = kUseI2SCell5;
	}
	else if ( 0 == strcmp ( mI2S->getName (), "i2s-g" ) )
	{
		mI2SInterfaceNumber = kUseI2SCell6;
	}
	else if ( 0 == strcmp ( mI2S->getName (), "i2s-h" ) )
	{
		mI2SInterfaceNumber = kUseI2SCell7;
	}
	else
	{
		mI2SInterfaceNumber = kUseI2SCell0;
	}
	
	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "AAPL,phandle" ) );
	mI2SPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debugIOLog ( 3,  "  mI2SPHandle 0x%lX", mI2SPHandle );
	
	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "reg" ) );
	mI2SOffset = *((UInt32*)osdata->getBytesNoCopy());
		
	i2s = mI2S->getParentEntry ( gIODTPlane );
	FailWithAction ( !i2s, result = FALSE, Exit );

	macIO = i2s->getParentEntry ( gIODTPlane );
	FailWithAction ( !macIO, result = false, Exit );
	debugIOLog ( 3, "  path = '...:%s:%s:%s:%s:'", macIO->getName (), i2s->getName (), mI2S->getName (), sound->getName () );
	
	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "AAPL,phandle" ) );
	mMacIOPHandle = *( (UInt32*)osdata->getBytesNoCopy () );
	debugIOLog ( 3,  "  mMacIOPHandle %lX", mMacIOPHandle );

	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "reg" ) );
	mMacIOOffset = *((UInt32*)osdata->getBytesNoCopy
	 () );


	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "compatible" ) );
	FailIf ( 0 == osdata, Exit );
	if ( osdata->isEqualTo ( kParentOfParentCompatible32bitSysIO, strlen ( kParentOfParentCompatible32bitSysIO ) ) )
	{
		debugIOLog ( 3,  "  about to waitForService on mSystemIOControllerService %p for %s", mSystemIOControllerService, kParentOfParentCompatible32bitSysIO );
		mSystemIOControllerService = IOService::waitForService ( IOService::serviceMatching ( "AppleKeyLargo" ) );
	}
	else if ( osdata->isEqualTo ( kParentOfParentCompatible64bitSysIO, strlen ( kParentOfParentCompatible64bitSysIO ) ) )
	{
		debugIOLog ( 3,  "  about to waitForService on mSystemIOControllerService %p for %s", mSystemIOControllerService, kParentOfParentCompatible64bitSysIO );
		mSystemIOControllerService = IOService::waitForService ( IOService::serviceMatching ( "AppleK2" ) );
	}
	else
	{
		FailIf ( TRUE, Exit );
	}
	debugIOLog ( 3,  "  mSystemIOControllerService %p", mSystemIOControllerService );
	
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

Exit:
	debugIOLog (3,  "- PlatformInterfaceGPIO_PlatformFunction[%p]::init returns %d", this, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceGPIO_PlatformFunction::free()
{
	super::free();
}

#pragma mark ¥
#pragma mark ¥ Power Management
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_PlatformFunction::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	return result;
}
	

#pragma mark ¥
#pragma mark ¥ GPIO Methods
#pragma mark ¥

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_PlatformFunction::setCodecReset ( CODEC_RESET target, GpioAttributes reset )
{
	IOReturn				result;

	switch ( target )
	{
		case kCODEC_RESET_Analog:	result = writeGpioState ( kGPIO_Selector_AnalogCodecReset, reset );		break;
		case kCODEC_RESET_Digital:	result = writeGpioState ( kGPIO_Selector_DigitalCodecReset, reset );	break;
		default:					result = kIOReturnBadArgument;											break;
	}
	return result;
}


//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterfaceGPIO_PlatformFunction::getCodecReset ( CODEC_RESET target )
{
	GpioAttributes		result;
	
	switch ( target )
	{
		case kCODEC_RESET_Analog:		result = readGpioState ( kGPIO_Selector_AnalogCodecReset );			break;
		case kCODEC_RESET_Digital:		result = readGpioState ( kGPIO_Selector_DigitalCodecReset );		break;
		default:						result = kGPIO_Unknown;												break;
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getClockMux ()
{
	return readGpioState ( kGPIO_Selector_ClockMux );
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_PlatformFunction::setClockMux ( GpioAttributes muxState )
{
	return writeGpioState ( kGPIO_Selector_ClockMux, muxState );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getCodecErrorInterrupt ()
{
	return readGpioState ( kGPIO_Selector_CodecErrorInterrupt );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getCodecInterrupt ()
{
	return readGpioState ( kGPIO_Selector_CodecInterrupt );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getComboInJackTypeConnected ()
{
	return readGpioState ( kGPIO_Selector_ComboInJackType );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getComboOutJackTypeConnected ()
{
	return readGpioState ( kGPIO_Selector_ComboOutJackType );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getDigitalInConnected ( GPIOSelector association )
{
	GpioAttributes 			gpioState;

	if ( kGPIO_Selector_NotAssociated == association )
	{
		gpioState = readGpioState ( kGPIO_Selector_DigitalInDetect );
	}
	else
	{
		if ( kGPIO_TypeIsDigital == getComboInJackTypeConnected () )
		{
			gpioState = readGpioState ( association );
		}
		else
		{
			gpioState = kGPIO_Disconnected;
		}
	}
	return gpioState;
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getDigitalOutConnected ( GPIOSelector association )
{
	GpioAttributes 			result;
	GpioAttributes 			gpioState;

	if (kGPIO_Selector_NotAssociated == association )
	{
		result = readGpioState ( kGPIO_Selector_DigitalOutDetect );
	}
	else
	{
		gpioState = getComboOutJackTypeConnected ();
		if ( kGPIO_TypeIsDigital == getComboOutJackTypeConnected () )
		{
			result = readGpioState ( association );
		}
		else
		{
			result = kGPIO_Disconnected;
		}
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getHeadphoneConnected ()
{
	GpioAttributes 			result;

	result = readGpioState ( kGPIO_Selector_HeadphoneDetect );
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_PlatformFunction::setHeadphoneMuteState ( GpioAttributes muteState )
{
	IOReturn		result = kIOReturnSuccess;
	
	if ( mEnableAmplifierMuteRelease || ( muteState == kGPIO_Muted ) ) 		//	[3514762]
	{
		result = writeGpioState ( kGPIO_Selector_HeadphoneMute, muteState );
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes 	PlatformInterfaceGPIO_PlatformFunction::getHeadphoneMuteState ()
{
	return readGpioState ( kGPIO_Selector_HeadphoneMute );
}
	
//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_PlatformFunction::setInputDataMux (GpioAttributes muxState)
{
	return writeGpioState ( kGPIO_Selector_InputDataMux, muxState );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getInputDataMux ()
{
	return readGpioState ( kGPIO_Selector_InputDataMux );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction:: getInternalMicrophoneID ()
{
	return readGpioState ( kGPIO_Selector_InternalMicrophoneID );
}


//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction:: getInternalSpeakerID ()
{
	return readGpioState ( kGPIO_Selector_InternalSpeakerID );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getLineInConnected ()
{
	GpioAttributes 			gpioState;

	gpioState = readGpioState ( kGPIO_Selector_LineInDetect );
	return gpioState;
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getLineOutConnected ()
{
	GpioAttributes 			gpioState;

	gpioState = readGpioState ( kGPIO_Selector_LineOutDetect );
	return gpioState;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_PlatformFunction::setLineOutMuteState ( GpioAttributes muteState )
{
	IOReturn		result = kIOReturnSuccess;
	
	if ( mEnableAmplifierMuteRelease || ( muteState == kGPIO_Muted ) ) 		//	[3514762]
	{
		result = writeGpioState ( kGPIO_Selector_LineOutMute, muteState );
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getLineOutMuteState ()
{
	return readGpioState ( kGPIO_Selector_LineOutMute );
}
	
//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getSpeakerConnected ()
{
	return readGpioState ( kGPIO_Selector_SpeakerDetect );
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_PlatformFunction::setSpeakerMuteState ( GpioAttributes muteState )
{
	IOReturn		result = kIOReturnSuccess;
	
	if ( mEnableAmplifierMuteRelease || ( muteState == kGPIO_Muted ) ) 		//	[3514762]
	{
		result = writeGpioState ( kGPIO_Selector_SpeakerMute, muteState );
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_PlatformFunction::getSpeakerMuteState ()
{
	return readGpioState ( kGPIO_Selector_SpeakerMute );
}
	
//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3514762]
void PlatformInterfaceGPIO_PlatformFunction::enableAmplifierMuteRelease ( void )
{
	debugIOLog (3, "± PlatformInterfaceGPIO_Mapped::enableAmplifierMuteRelease enabling amplifier mutes");
	mEnableAmplifierMuteRelease = TRUE;
}


#pragma mark ¥
#pragma mark ¥ Set Interrupt Handler Methods
#pragma mark ¥

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_PlatformFunction::disableInterrupt ( IOService * device, PlatformInterruptSource source )
{
	const OSSymbol * 		selector = OSSymbol::withCString ( kIOPFInterruptDisable );
	const OSSymbol *		enFuncSymbolName = 0;
	void *					interruptHandler = 0;
	IOReturn				result;

	result = kIOReturnError;
	//	NOTE:	Some interrupt sources, although supported by platform functions, do not use the platform functions.
	//			Examples would be where a hardware interrupt status is not latched and is updated at a rate that
	//			exceeds the service latency.  These interrupt sources will be polled through a periodic timer to
	//			prevent interrupts from being queued at a rate that exceeds the ability to service the interrupt.
	if ( interruptUsesTimerPolling ( source ) )
	{
		result = kIOReturnSuccess;
	}
	else
	{
		if ( mSystemIOControllerService )
		{
			switch ( source )
			{
				case kCodecErrorInterrupt:			 interruptHandler = mCodecErrorInterruptHandler;												break;
				case kCodecInterrupt:				 interruptHandler = mCodecInterruptHandler;														break;
				case kComboInDetectInterrupt:		 interruptHandler = mComboInDetectInterruptHandler;												break;
				case kComboOutDetectInterrupt:		 interruptHandler = mComboOutDetectInterruptHandler;											break;
				case kDigitalInDetectInterrupt:		 interruptHandler = mDigitalInDetectInterruptHandler;											break;
				case kDigitalOutDetectInterrupt:	 interruptHandler = mDigitalOutDetectInterruptHandler;											break;
				case kHeadphoneDetectInterrupt:		 interruptHandler = mHeadphoneDetectInterruptHandler;											break;
				case kLineInputDetectInterrupt:		 interruptHandler = mLineInputDetectInterruptHandler;											break;
				case kLineOutputDetectInterrupt:	 interruptHandler = mLineOutputDetectInterruptHandler;											break;
				case kSpeakerDetectInterrupt:		 interruptHandler = mSpeakerDetectInterruptHandler;												break;
				case kUnknownInterrupt:																												break;
				default:																															break;
			}
			FailIf ( 0 == interruptHandler, Exit );
			switch ( source )
			{
				case kCodecErrorInterrupt:			enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableCodecErrorIRQ, mI2SPHandle );		break;
				case kCodecInterrupt:				enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableCodecIRQ, mI2SPHandle );			break;
				case kComboInDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName( kAppleGPIO_DisableComboInSense, mI2SPHandle );		break;
				case kComboOutDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName( kAppleGPIO_DisableComboOutSense, mI2SPHandle );		break;
				case kDigitalOutDetectInterrupt:	enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableDigitalOutDetect, mI2SPHandle );	break;
				case kDigitalInDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableDigitalInDetect, mI2SPHandle );		break;
				case kHeadphoneDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName( kAppleGPIO_DisableHeadphoneDetect, mI2SPHandle );	break;
				case kLineInputDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableLineInDetect, mI2SPHandle );		break;
				case kLineOutputDetectInterrupt:	enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableLineOutDetect, mI2SPHandle );		break;
				case kSpeakerDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_DisableSpeakerDetect, mI2SPHandle );		break;
				case kUnknownInterrupt:
				default:							debugIOLog (7,  "  PlatformInterfaceGPIO_PlatformFunction::disableInterrupt attempt to disable unknown interrupt source" );	break;
			}
			FailIf ( 0 == enFuncSymbolName, Exit );
			FailIf ( 0 == selector, Exit );
			
			result = mSystemIOControllerService->callPlatformFunction ( enFuncSymbolName, true, interruptHandler, device, (void*)source, (void*)selector);
			if ( kIOReturnSuccess != result )
			{
				debugIOLog ( 6, "  mSystemIOControllerService->callPlatformFunction ( %p, true, %p, %p, %p, %p ) returns 0x%lX", enFuncSymbolName, interruptHandler, device, source, selector, result ); 
			}
			selector->release ();
			enFuncSymbolName->release ();	// [3324205]
			
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
Exit:
	if ( kIOReturnSuccess == result )
	{
		switch ( source )
		{
			case kCodecErrorInterrupt:			mCodecErrorInterruptEnable = FALSE;																	break;
			case kCodecInterrupt:				mCodecInterruptEnable = FALSE;																		break;
			case kComboInDetectInterrupt:		mComboInDetectInterruptEnable = FALSE;																break;
			case kComboOutDetectInterrupt:		mComboOutDetectInterruptEnable = FALSE;																break;
			case kDigitalInDetectInterrupt:		mDigitalInDetectInterruptEnable = FALSE;															break;
			case kDigitalOutDetectInterrupt:	mDigitalOutDetectInterruptEnable = FALSE;															break;
			case kHeadphoneDetectInterrupt:		mHeadphoneDetectInterruptEnable = FALSE;															break;
			case kLineInputDetectInterrupt:		mLineInputDetectInterruptEnable = FALSE;															break;
			case kLineOutputDetectInterrupt:	mLineOutputDetectInterruptEnable = FALSE;															break;
			case kSpeakerDetectInterrupt:		mSpeakerDetectInterruptEnable = FALSE;																break;
			case kUnknownInterrupt:																													break;
			default:																																break;
		}
	}
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( IOService * device, PlatformInterruptSource source )
{
	const OSSymbol * 		selector = OSSymbol::withCString ( kIOPFInterruptEnable );
	const OSSymbol *		enFuncSymbolName = 0;
	void *					interruptHandler = 0;
	IOReturn				result;

	result = kIOReturnError;
	switch ( source )
	{
		case kCodecErrorInterrupt:			debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kCodecErrorInterrupt )" );			break;
		case kCodecInterrupt:				debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kCodecInterrupt )" );				break;
		case kComboInDetectInterrupt:		debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kComboInDetectInterrupt )" );		break;
		case kComboOutDetectInterrupt:		debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kComboOutDetectInterrupt )" );		break;
		case kDigitalInDetectInterrupt:		debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kDigitalInDetectInterrupt )" );	break;
		case kDigitalOutDetectInterrupt:	debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kHeadphoneDetectInterrupt )" );	break;
		case kHeadphoneDetectInterrupt:		debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kLineInputDetectInterrupt )" );	break;
		case kLineInputDetectInterrupt:		debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kLineInputDetectInterrupt )" );	break;
		case kLineOutputDetectInterrupt:	debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kLineOutputDetectInterrupt )" );	break;
		case kSpeakerDetectInterrupt:		debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kSpeakerDetectInterrupt )" );		break;
		case kUnknownInterrupt:
		default:							debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( UNKNOWN )" );						break;
	}
	//	NOTE:	Some interrupt sources, although supported by platform functions, do not use the platform functions.
	//			Examples would be where a hardware interrupt status is not latched and is updated at a rate that
	//			exceeds the service latency.  These interrupt sources will be polled through a periodic timer to
	//			prevent interrupts from being queued at a rate that exceeds the ability to service the interrupt.
	if ( interruptUsesTimerPolling ( source ) )
	{
		result = kIOReturnSuccess;
	}
	else
	{
		if ( mSystemIOControllerService )
		{
			switch ( source )
			{
				case kCodecErrorInterrupt:			 interruptHandler = mCodecErrorInterruptHandler;												break;
				case kCodecInterrupt:				 interruptHandler = mCodecInterruptHandler;														break;
				case kComboInDetectInterrupt:		 interruptHandler = mComboInDetectInterruptHandler;												break;
				case kComboOutDetectInterrupt:		 interruptHandler = mComboOutDetectInterruptHandler;											break;
				case kDigitalInDetectInterrupt:		 interruptHandler = mDigitalInDetectInterruptHandler;											break;
				case kDigitalOutDetectInterrupt:	 interruptHandler = mDigitalOutDetectInterruptHandler;											break;
				case kHeadphoneDetectInterrupt:		 interruptHandler = mHeadphoneDetectInterruptHandler;											break;
				case kLineInputDetectInterrupt:		 interruptHandler = mLineInputDetectInterruptHandler;											break;
				case kLineOutputDetectInterrupt:	 interruptHandler = mLineOutputDetectInterruptHandler;											break;
				case kSpeakerDetectInterrupt:		 interruptHandler = mSpeakerDetectInterruptHandler;												break;
				case kUnknownInterrupt:																												break;
				default:																															break;
			}
			FailIf ( 0 == interruptHandler, Exit );
			switch ( source )
			{
				case kCodecErrorInterrupt:			enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableCodecErrorIRQ, mI2SPHandle );		break;
				case kCodecInterrupt:				enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableCodecIRQ, mI2SPHandle );				break;
				case kComboInDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName( kAppleGPIO_EnableComboInSense, mI2SPHandle );		break;
				case kComboOutDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName( kAppleGPIO_EnableComboOutSense, mI2SPHandle );		break;
				case kDigitalOutDetectInterrupt:	enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableDigitalOutDetect, mI2SPHandle );		break;
				case kDigitalInDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableDigitalInDetect, mI2SPHandle );		break;
				case kHeadphoneDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName( kAppleGPIO_EnableHeadphoneDetect, mI2SPHandle );		break;
				case kLineInputDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableLineInDetect, mI2SPHandle );			break;
				case kLineOutputDetectInterrupt:	enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableLineOutDetect, mI2SPHandle );		break;
				case kSpeakerDetectInterrupt:		enFuncSymbolName = makeFunctionSymbolName(kAppleGPIO_EnableSpeakerDetect, mI2SPHandle );		break;
				case kUnknownInterrupt:
				default:							debugIOLog (7,  "  PlatformInterfaceGPIO_PlatformFunction::enableInterrupt attempt to enable unknown interrupt source" ); break;
			}
#if 0
			if ( 0 == enFuncSymbolName )
			{
				switch ( source )
				{
					case kCodecErrorInterrupt:			debugIOLog ( 5, "  ... could not 'makeFunctionSymbolName' for kAppleGPIO_EnableCodecErrorIRQ" );	break;
					case kCodecInterrupt:				debugIOLog ( 5, "  ... could not 'makeFunctionSymbolName' for kAppleGPIO_EnableCodecIRQ" );			break;
					case kComboInDetectInterrupt:		debugIOLog ( 5, "  ... could not 'makeFunctionSymbolName' for kAppleGPIO_EnableComboInSense" );		break;
					case kComboOutDetectInterrupt:		debugIOLog ( 5, "  ... could not 'makeFunctionSymbolName' for kAppleGPIO_EnableComboOutSense" );	break;
					case kDigitalOutDetectInterrupt:	debugIOLog ( 5, "  ... could not 'makeFunctionSymbolName' for kAppleGPIO_EnableDigitalOutDetect" );	break;
					case kDigitalInDetectInterrupt:		debugIOLog ( 5, "  ... could not 'makeFunctionSymbolName' for kAppleGPIO_EnableDigitalInDetect" );	break;
					case kHeadphoneDetectInterrupt:		debugIOLog ( 5, "  ... could not 'makeFunctionSymbolName' for kAppleGPIO_EnableHeadphoneDetect" );	break;
					case kLineInputDetectInterrupt:		debugIOLog ( 5, "  ... could not 'makeFunctionSymbolName' for kAppleGPIO_EnableLineInDetect" );		break;
					case kLineOutputDetectInterrupt:	debugIOLog ( 5, "  ... could not 'makeFunctionSymbolName' for kAppleGPIO_EnableLineOutDetect" );	break;
					case kSpeakerDetectInterrupt:		debugIOLog ( 5, "  ... could not 'makeFunctionSymbolName' for kAppleGPIO_EnableSpeakerDetect" );	break;
					case kUnknownInterrupt:
					default:							debugIOLog ( 5, "  ... could not 'makeFunctionSymbolName' for kUnknownInterrupt" );					break;
				}
			}
#endif
			FailIf ( 0 == enFuncSymbolName, Exit );
			FailIf ( 0 == selector, Exit );
			result = mSystemIOControllerService->callPlatformFunction ( enFuncSymbolName, true, interruptHandler, device, (void*)source, (void*)selector);
			if ( kIOReturnSuccess != result )
			{
				debugIOLog ( 6, "  mSystemIOControllerService->callPlatformFunction ( %p, true, %p, %p, %p, %p ) returns 0x%lX", enFuncSymbolName, interruptHandler, device, source, selector, result ); 
			}
			selector->release ();
			enFuncSymbolName->release ();	// [3324205]
			FailIf ( kIOReturnSuccess != result, Exit );
		}
	}
Exit:
	if ( kIOReturnSuccess == result )
	{
		switch ( source )
		{
			case kCodecErrorInterrupt:			mCodecErrorInterruptEnable = true;																	break;
			case kCodecInterrupt:				mCodecInterruptEnable = true;																		break;
			case kComboInDetectInterrupt:		mComboInDetectInterruptEnable = true;																break;
			case kComboOutDetectInterrupt:		mComboOutDetectInterruptEnable = true;																break;
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
	switch ( source )
	{
		case kCodecErrorInterrupt:			debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kCodecErrorInterrupt ) returns 0x%lX", result );		break;
		case kCodecInterrupt:				debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kCodecInterrupt ) returns 0x%lX", result );			break;
		case kComboInDetectInterrupt:		debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kComboInDetectInterrupt ) returns 0x%lX", result );	break;
		case kComboOutDetectInterrupt:		debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kComboOutDetectInterrupt ) returns 0x%lX", result );	break;
		case kDigitalInDetectInterrupt:		debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kDigitalInDetectInterrupt ) returns 0x%lX", result );	break;
		case kDigitalOutDetectInterrupt:	debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kHeadphoneDetectInterrupt ) returns 0x%lX", result );	break;
		case kHeadphoneDetectInterrupt:		debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kLineInputDetectInterrupt ) returns 0x%lX", result );	break;
		case kLineInputDetectInterrupt:		debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kLineInputDetectInterrupt ) returns 0x%lX", result );	break;
		case kLineOutputDetectInterrupt:	debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kLineOutputDetectInterrupt ) returns 0x%lX", result );	break;
		case kSpeakerDetectInterrupt:		debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( kSpeakerDetectInterrupt ) returns 0x%lX", result );	break;
		case kUnknownInterrupt:
		default:							debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::enableInterrupt ( UNKNOWN ) returns 0x%lX", result );					break;
	}
	return result;
}


//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_PlatformFunction::registerInterruptHandler ( IOService * device, void * interruptHandler, PlatformInterruptSource source )
{
	const OSSymbol * 		selector = OSSymbol::withCString ( kIOPFInterruptRegister );
	const OSSymbol *		funcSymbolName = 0;
	IOReturn				result;

	debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::registerInterruptHandler ( IOService * device %p, void * interruptHandler %p,  PlatformInterruptSource source%ld )", device, interruptHandler, source );
	switch ( source )
	{
		case kCodecErrorInterrupt:			debugIOLog ( 6, "  registering kCodecErrorInterrupt interrupt handler" );								break;
		case kCodecInterrupt:				debugIOLog ( 6, "  registering kCodecInterrupt interrupt handler" );									break;
		case kComboInDetectInterrupt:		debugIOLog ( 6, "  registering kComboInDetectInterrupt interrupt handler" );							break;
		case kComboOutDetectInterrupt:		debugIOLog ( 6, "  registering kComboOutDetectInterrupt interrupt handler" );							break;
		case kDigitalInDetectInterrupt:		debugIOLog ( 6, "  registering kDigitalInDetectInterrupt interrupt handler" );							break;
		case kDigitalOutDetectInterrupt:	debugIOLog ( 6, "  registering kDigitalOutDetectInterrupt interrupt handler" );							break;
		case kHeadphoneDetectInterrupt:		debugIOLog ( 6, "  registering kHeadphoneDetectInterrupt interrupt handler" );							break;
		case kLineInputDetectInterrupt:		debugIOLog ( 6, "  registering kLineInputDetectInterrupt interrupt handler" );							break;
		case kLineOutputDetectInterrupt:	debugIOLog ( 6, "  registering kLineOutputDetectInterrupt interrupt handler" );							break;
		case kSpeakerDetectInterrupt:		debugIOLog ( 6, "  registering kSpeakerDetectInterrupt interrupt handler" );							break;
		case kUnknownInterrupt:
		default:							debugIOLog ( 6,  "  Attempt to register unknown interrupt source" );									break;
	}
	
	result = kIOReturnError;
	//	Don't register interrupt services with platform methods if the platform method is not going to be used.
	//  Note:  For polled interrupt sources, no registration occurs.  Since no registration has occurred, the
	//  interrupt source is not enabled (registration implies enabling the interrupt source due to the behavior
	//  of the Platform Functions used to provide support for GPIO interrupt services).
	if ( interruptUsesTimerPolling ( source ) )
	{
		debugIOLog ( 6, "  ... interrupt will be polled!" );
		result = kIOReturnSuccess;
	}
	else
	{
		if ( mSystemIOControllerService )
		{
			switch ( source )
			{
				case kCodecErrorInterrupt:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterCodecErrorIRQ, mI2SPHandle );		break;
				case kCodecInterrupt:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterCodecIRQ, mI2SPHandle );			break;
				case kComboInDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterComboInSense, mI2SPHandle );		break;
				case kComboOutDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterComboOutSense, mI2SPHandle );		break;
				case kDigitalInDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterDigitalInDetect, mI2SPHandle );		break;
				case kDigitalOutDetectInterrupt:	funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterDigitalOutDetect, mI2SPHandle );	break;
				case kHeadphoneDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterHeadphoneDetect, mI2SPHandle );		break;
				case kLineInputDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterLineInDetect, mI2SPHandle );		break;
				case kLineOutputDetectInterrupt:	funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterLineOutDetect, mI2SPHandle );		break;
				case kSpeakerDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_RegisterSpeakerDetect, mI2SPHandle );		break;
				case kUnknownInterrupt:
				default:							debugIOLog (7,  "  attempt to register unknown interrupt source" );								break;
			}
			FailIf ( 0 == funcSymbolName, Exit );
			FailIf ( 0 == selector, Exit );
			
            
			result = mSystemIOControllerService->callPlatformFunction ( funcSymbolName, true, interruptHandler, device, (void*)source, (void*)selector);
			if ( kIOReturnSuccess != result )
			{
				debugIOLog ( 6, "  mSystemIOControllerService->callPlatformFunction ( %p, true, %p, %p, %p, %p ) returns 0x%lX", funcSymbolName, interruptHandler, device, source, selector, result ); 
			}
			selector->release ();
			funcSymbolName->release ();	// [3324205]
			
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		else
		{
			debugIOLog ( 6, "  mSystemIOControllerService is 0!!!" );
		}
	}
Exit:
	if ( kIOReturnSuccess != result )
	{
		switch ( source )
		{
			case kCodecErrorInterrupt:				mGpioMessageFlag &= (~( 1 << gpioMessage_CodecErrorInterrupt_bitAddress ));						break;
			case kCodecInterrupt:					mGpioMessageFlag &= (~( 1 << gpioMessage_CodecInterrupt_bitAddress ));							break;
			case kComboInDetectInterrupt:			mGpioMessageFlag &= (~( 1 << gpioMessage_ComboInJackType_bitAddress ));							break;
			case kComboOutDetectInterrupt:			mGpioMessageFlag &= (~( 1 << gpioMessage_ComboOutJackType_bitAddress ));						break;
			case kDigitalOutDetectInterrupt:		mGpioMessageFlag &= (~( 1 << gpioMessage_DigitalInDetect_bitAddress ));							break;
			case kDigitalInDetectInterrupt:			mGpioMessageFlag &= (~( 1 << gpioMessage_DigitalOutDetect_bitAddress ));						break;
			case kHeadphoneDetectInterrupt:			mGpioMessageFlag &= (~( 1 << gpioMessage_HeadphoneDetect_bitAddress ));							break;
			case kLineInputDetectInterrupt:			mGpioMessageFlag &= (~( 1 << gpioMessage_LineInDetect_bitAddress ));							break;
			case kLineOutputDetectInterrupt:		mGpioMessageFlag &= (~( 1 << gpioMessage_LineOutDetect_bitAddress ));							break;
			case kSpeakerDetectInterrupt:			mGpioMessageFlag &= (~( 1 << gpioMessage_SpeakerDetect_bitAddress ));							break;
			case kUnknownInterrupt:
				default:							debugIOLog (7,  "  attempt to disable unknown interrupt source" );								break;
		}
	}
	else
	{
		//	Keep a copy of a pointer to the interrupt service routine in this object so that the enable
		//	and disable interrupt methods have access to the pointer since it is possible that
		//	the interrupt might be enabled or disabled by an object that does not have the pointer.
		switch ( source )
		{
			case kCodecErrorInterrupt:				mCodecErrorInterruptHandler = interruptHandler;													break;
			case kCodecInterrupt:					mCodecInterruptHandler = interruptHandler;														break;
			case kComboInDetectInterrupt:			mComboInDetectInterruptHandler = interruptHandler;												break;
			case kComboOutDetectInterrupt:			mComboOutDetectInterruptHandler = interruptHandler;												break;
			case kDigitalInDetectInterrupt:			mDigitalInDetectInterruptHandler = interruptHandler;											break;
			case kDigitalOutDetectInterrupt:		mDigitalOutDetectInterruptHandler = interruptHandler;											break;
			case kHeadphoneDetectInterrupt:			mHeadphoneDetectInterruptHandler = interruptHandler;											break;
			case kLineInputDetectInterrupt:			mLineInputDetectInterruptHandler = interruptHandler;											break;
			case kLineOutputDetectInterrupt:		mLineOutputDetectInterruptHandler = interruptHandler;											break;
			case kSpeakerDetectInterrupt:			mSpeakerDetectInterruptHandler = interruptHandler;												break;
			case kUnknownInterrupt:																													break;
			default:																																break;
		}
		enableInterrupt ( device, source );
	}
	debugIOLog ( 6, "  mGpioMessageFlag = 0x%0.8X", mGpioMessageFlag );
	debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::registerInterruptHandler ( IOService * device %p, void * interruptHandler %p,  PlatformInterruptSource source%ld ) returns %lX", device, interruptHandler, source, result );
	
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_PlatformFunction::unregisterInterruptHandler (IOService * device, void * interruptHandler, PlatformInterruptSource source )
{
	const OSSymbol * 		selector = OSSymbol::withCString ( kIOPFInterruptUnRegister );
	const OSSymbol *		funcSymbolName = 0;
	IOReturn				result;

	result = kIOReturnError;
	FailIf ( 0 == mSystemIOControllerService, Exit );

	disableInterrupt ( device, source );

	if ( interruptUsesTimerPolling ( source ) )
	{
		switch ( source )
		{
			case kCodecErrorInterrupt:			mCodecErrorInterruptHandler = 0;																	break;
			case kCodecInterrupt:				mCodecInterruptHandler = 0;																			break;
			case kComboInDetectInterrupt:		mComboInDetectInterruptHandler = 0;																	break;
			case kComboOutDetectInterrupt:		mComboOutDetectInterruptHandler = 0;																break;
			case kDigitalInDetectInterrupt:		mDigitalInDetectInterruptHandler = 0;																break;
			case kDigitalOutDetectInterrupt:	mDigitalOutDetectInterruptHandler = 0;																break;
			case kHeadphoneDetectInterrupt:		mHeadphoneDetectInterruptHandler = 0;																break;
			case kLineInputDetectInterrupt:		mLineInputDetectInterruptHandler = 0;																break;
			case kLineOutputDetectInterrupt:	mLineOutputDetectInterruptHandler = 0;																break;
			case kSpeakerDetectInterrupt:		mSpeakerDetectInterruptHandler = 0;																	break;
			case kUnknownInterrupt:																													break;
			default:																																break;
		}
		result = kIOReturnSuccess;
	}
	else
	{
		switch ( source )
		{
			case kCodecErrorInterrupt:			funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterCodecErrorIRQ, mI2SPHandle );			break;
			case kCodecInterrupt:				funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterCodecIRQ, mI2SPHandle );				break;
			case kComboInDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterComboInSense, mI2SPHandle );			break;
			case kComboOutDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterComboOutSense, mI2SPHandle );			break;
			case kDigitalInDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterDigitalInDetect, mI2SPHandle );		break;
			case kDigitalOutDetectInterrupt:	funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterDigitalOutDetect, mI2SPHandle );		break;
			case kHeadphoneDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterHeadphoneDetect, mI2SPHandle );		break;
			case kLineInputDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterLineInDetect, mI2SPHandle );			break;
			case kLineOutputDetectInterrupt:	funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterLineOutDetect, mI2SPHandle );			break;
			case kSpeakerDetectInterrupt:		funcSymbolName = makeFunctionSymbolName( kAppleGPIO_UnregisterSpeakerDetect, mI2SPHandle );			break;
			case kUnknownInterrupt:
			default:							debugIOLog (7,  "  attempt to unregister unknown interrupt source" );								break;
		}
		FailIf ( 0 == funcSymbolName, Exit );
		FailIf ( 0 == selector, Exit );
		
		result = mSystemIOControllerService->callPlatformFunction ( funcSymbolName, true, interruptHandler, device, (void*)source, (void*)selector);
		if ( kIOReturnSuccess != result )
		{
			debugIOLog ( 6, "  mSystemIOControllerService->callPlatformFunction ( %p, true, %p, %p, %p, %p ) returns 0x%lX", funcSymbolName, interruptHandler, device, source, selector, result ); 
		}
		selector->release ();
		funcSymbolName->release ();			// [3324205]
		
		FailIf ( kIOReturnSuccess != result && kCodecErrorInterrupt == source, Exit );
		FailIf ( kIOReturnSuccess != result && kCodecInterrupt == source, Exit );
		FailIf ( kIOReturnSuccess != result && kComboInDetectInterrupt == source, Exit );
		FailIf ( kIOReturnSuccess != result && kComboOutDetectInterrupt == source, Exit );
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

#pragma mark ¥
#pragma mark ¥	Polling Support
#pragma mark ¥

//	--------------------------------------------------------------------------------
//	
bool PlatformInterfaceGPIO_PlatformFunction::interruptUsesTimerPolling ( PlatformInterruptSource source )
{
	bool		result = FALSE;
	
	if ( ( kCodecErrorInterrupt == source ) || ( kCodecInterrupt == source ) )
	{
		result = true;
	}
	return result;
}


//	--------------------------------------------------------------------------------
void PlatformInterfaceGPIO_PlatformFunction::poll ( void )
{
	IOCommandGate *			cg;

	debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::poll()" );
	
	FailIf ( 0 == mProvider, Exit );
	cg = mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		if ( interruptUsesTimerPolling ( kCodecErrorInterrupt ) )
		{
			if ( mCodecErrorInterruptEnable )
			{
				debugIOLog ( 6, "  PlatformInterfaceGPIO_PlatformFunction::poll() about to kCodecErrorInterruptStatus" );
				if ( interruptUsesTimerPolling ( kCodecErrorInterrupt ) )
				{
					cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kCodecErrorInterruptStatus, (void *)kGPIO_CodecInterruptActive, (void *)0 );
				}
			}
		}
		if ( interruptUsesTimerPolling ( kCodecInterrupt ) )
		{
			if ( mCodecInterruptEnable )
			{
				debugIOLog ( 6, "  PlatformInterfaceGPIO_PlatformFunction::poll() about to kCodecInterruptStatus" );
				if ( interruptUsesTimerPolling ( kCodecInterrupt ) )
				{
					cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kCodecInterruptStatus, (void *)getCodecInterrupt (), (void *)0 );
				}
			}
		}
	}
Exit:
	debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::poll()" );
	return;
}


#pragma mark ¥
#pragma mark ¥	Utilities
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_PlatformFunction::makeSymbolAndCallPlatformFunctionNoWait ( const char * name, void * param1, void * param2, void * param3, void * param4 )
{
	const OSSymbol*			funcSymbolName;
	IOReturn				result = kIOReturnError;

	if ( ( 0 != mSystemIOControllerService ) && ( 0 != mI2SPHandle ) && ( 0 != name ) )
	{
		funcSymbolName = makeFunctionSymbolName ( name, mI2SPHandle );
		FailIf ( NULL == funcSymbolName, Exit );
		
		result = mSystemIOControllerService->callPlatformFunction ( funcSymbolName, false, param1, param2, param3, param4 );
		funcSymbolName->release ();	// [3324205]
	}
Exit:
	if ( kIOReturnSuccess != result ) 
	{
		debugIOLog ( 6, "+ PlatformInterfaceGPIO_PlatformFunction::makeSymbolAndCallPlatformFunctionNoWait ( %p, %p, %p, %p, %p )", name, param1, param2, param3, param4 );
		debugIOLog ( 6, "  mSystemIOControllerService %p, mI2SPHandle %p", mSystemIOControllerService, mI2SPHandle );
		debugIOLog ( 6, "  name->'%s'", name );
		debugIOLog ( 6, "  mSystemIOControllerService->callPlatformFunction ( %p, FALSE, %p, %p, %p, %p ) returns 0x%lX", funcSymbolName, param1, param2, param3, param4, result );
		debugIOLog ( 6, "- PlatformInterfaceGPIO_PlatformFunction::makeSymbolAndCallPlatformFunctionNoWait ( %p, %p, %p, %p, %p ) returns 0x%X", name, param1, param2, param3, param4, result );
	}
	return result;
}

//	--------------------------------------------------------------------------------
const OSSymbol* PlatformInterfaceGPIO_PlatformFunction::makeFunctionSymbolName(const char * name,UInt32 pHandle)
{
	const OSSymbol* 	funcSymbolName = 0;
	char		stringBuf[256];
		
	sprintf ( stringBuf, "%s-%08lx", name, pHandle );
	funcSymbolName = OSSymbol::withCString ( stringBuf );
	
	return funcSymbolName;
}

//	--------------------------------------------------------------------------------
GpioAttributes  PlatformInterfaceGPIO_PlatformFunction::GetCachedAttribute ( GPIOSelector selector, GpioAttributes defaultResult )
{
	GpioAttributes		result;
	
	result = defaultResult;
	//	If there is no platform function then return the cached GPIO state if a cache state is available
	switch ( selector )
	{
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
		case kGPIO_Selector_InternalMicrophoneID:	result = mAppleGPIO_InternalMicrophoneID;																			break;
		case kGPIO_Selector_InternalSpeakerID:		result = mAppleGPIO_InternalSpeakerID;																				break;
		case kGPIO_Selector_LineInDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_LineOutDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_LineOutMute:			result = mAppleGPIO_LineOutMute;																					break;
		case kGPIO_Selector_SpeakerDetect:			/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_SpeakerMute:			result = mAppleGPIO_AmpMute;																						break;
		case kGPIO_Selector_ExternalMicDetect:		/*	NO CACHE AVAILABLE ON READ ONLY GPIO	*/																		break;
		case kGPIO_Selector_NotAssociated:			/*	NO CACHE AVAILABLE ON UNAVAILABLE ONLY GPIO	*/																	break;
	}
	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterfaceGPIO_PlatformFunction::readGpioState ( GPIOSelector selector )
{
	GpioAttributes		result = kGPIO_Unknown;
	UInt32				value;
	IOReturn			err = kIOReturnNoDevice;

	if ( mSystemIOControllerService )
	{
		switch ( selector )
		{
			case kGPIO_Selector_AnalogCodecReset:		err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetAudioHwReset, (void*)&value, (void*)0, (void*)0, (void*)0 );			break;
			case kGPIO_Selector_ClockMux:				err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetCodecClockMux, (void*)&value, (void*)0, (void*)0, (void*)0 );			break;
			case kGPIO_Selector_CodecInterrupt:			err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetCodecIRQ, (void*)&value, (void*)0, (void*)0, (void*)0 );				break;
			case kGPIO_Selector_CodecErrorInterrupt:	err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetCodecErrorIRQ, (void*)&value, (void*)0, (void*)0, (void*)0 );			break;
			case kGPIO_Selector_ComboInJackType:		err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetComboInJackType, (void*)&value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_ComboOutJackType:		err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetComboOutJackType, (void*)&value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_DigitalCodecReset:		err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetAudioDigHwReset, (void*)&value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_DigitalInDetect:		err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetDigitalInDetect, (void*)&value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_DigitalOutDetect:		err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetDigitalOutDetect, (void*)&value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_HeadphoneDetect:		err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetHeadphoneDetect, (void*)&value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_HeadphoneMute:			err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetHeadphoneMute, (void*)&value, (void*)0, (void*)0, (void*)0 );			break;
			case kGPIO_Selector_InputDataMux:			err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetCodecInputDataMux, (void*)&value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_InternalMicrophoneID:	err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetInternalMicrophoneID, (void*)&value, (void*)0, (void*)0, (void*)0 );	break;
			case kGPIO_Selector_InternalSpeakerID:		err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetInternalSpeakerID, (void*)&value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_LineInDetect:			err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetLineInDetect, (void*)&value, (void*)0, (void*)0, (void*)0 );			break;
			case kGPIO_Selector_LineOutDetect:			err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetLineOutDetect, (void*)&value, (void*)0, (void*)0, (void*)0 );			break;
			case kGPIO_Selector_LineOutMute:			err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetLineOutMute, (void*)&value, (void*)0, (void*)0, (void*)0 );			break;
			case kGPIO_Selector_SpeakerDetect:			err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetSpeakerDetect, (void*)&value, (void*)0, (void*)0, (void*)0 );			break;
			case kGPIO_Selector_SpeakerMute:			err = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_GetAmpMute, (void*)&value, (void*)0, (void*)0, (void*)0 );				break;
			case kGPIO_Selector_ExternalMicDetect:																																			break;
			case kGPIO_Selector_NotAssociated:																																				break;
			default:									FailIf ( true, Exit );																												break;
		}
		if ( kIOReturnSuccess != err )
		{
			result = GetCachedAttribute ( selector, result );
		}
		else
		{
			//	Translate the GPIO state to a GPIO attribute
			switch ( selector )
			{
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
				case kGPIO_Selector_InternalMicrophoneID:	result = value ? kGPIO_IsDefault : kGPIO_IsAlternate;															break;
				case kGPIO_Selector_InternalSpeakerID:		result = value ? kGPIO_IsDefault : kGPIO_IsAlternate;															break;
				case kGPIO_Selector_LineInDetect:			result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
				case kGPIO_Selector_LineOutDetect:			result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
				case kGPIO_Selector_LineOutMute:			result = value ? kGPIO_Muted : kGPIO_Unmuted;																	break;
				case kGPIO_Selector_SpeakerDetect:			result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
				case kGPIO_Selector_SpeakerMute:			result = value ? kGPIO_Muted : kGPIO_Unmuted;																	break;
				case kGPIO_Selector_ExternalMicDetect:		result = value ? kGPIO_Connected : kGPIO_Disconnected;															break;
				case kGPIO_Selector_NotAssociated:			result = kGPIO_Connected;																						break;
			}
		}
	}
	else
	{
		debugIOLog ( 5, "* PlatformInterfaceGPIO_PlatformFunction::readGpioState() blocked due to 0 == mSystemIOControllerService" );
	}
Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_PlatformFunction::writeGpioState ( GPIOSelector selector, GpioAttributes gpioState )
{
	UInt32				value;
	IOReturn			result;

	result = kIOReturnError;
	value = 0;
	if ( mSystemIOControllerService )
	{
		//	Translate GPIO attribute to gpio state
		switch ( selector )
		{
			case kGPIO_Selector_AnalogCodecReset:		FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Reset, gpioState, &value ), FailExit );					break;
			case kGPIO_Selector_ClockMux:				FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Mux, gpioState, &value ), FailExit );					break;
			case kGPIO_Selector_DigitalCodecReset:		FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Reset, gpioState, &value ), FailExit );					break;
			case kGPIO_Selector_HeadphoneMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, gpioState, &value ), FailExit );					break;
			case kGPIO_Selector_InputDataMux:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_Mux, gpioState, &value ), FailExit );					break;
			case kGPIO_Selector_LineOutMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, gpioState, &value ), FailExit );					break;
			case kGPIO_Selector_SpeakerMute:			FailIf ( kIOReturnSuccess != translateGpioAttributeToGpioState ( kGPIO_Type_MuteL, gpioState, &value ), FailExit );					break;
			default:									FailIf ( true, FailExit );																											break;
		}
		switch ( selector )
		{
			case kGPIO_Selector_AnalogCodecReset:		result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_SetAudioHwReset, (void*)value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_ClockMux:				result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_SetCodecClockMux, (void*)value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_DigitalCodecReset:		result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_SetAudioDigHwReset, (void*)value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_HeadphoneMute:			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_SetHeadphoneMute, (void*)value, (void*)0, (void*)0, (void*)0 );		break;
			case kGPIO_Selector_InputDataMux:			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_SetCodecInputDataMux, (void*)value, (void*)0, (void*)0, (void*)0 );	break;
			case kGPIO_Selector_LineOutMute:			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_SetLineOutMute, (void*)value, (void*)0, (void*)0, (void*)0 );			break;
			case kGPIO_Selector_SpeakerMute:			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleGPIO_SetAmpMute, (void*)value, (void*)0, (void*)0, (void*)0 );				break;
			default:																																										break;
		}
		if ( kIOReturnSuccess == result )
		{
#if 0
			//	Cache the gpio state in case the gpio does not have a platform function supporting read access
			if ( kGPIO_Selector_AnalogCodecReset == selector )
			{
				if ( kGPIO_Reset == gpioState )
				{
					debugIOLog ( 6, "  writeGpioState ( kGPIO_Selector_AnalogCodecReset %d, gpioState %d = kGPIO_Reset )", selector, gpioState );
				} 
				else if ( kGPIO_Run == gpioState )
				{
					debugIOLog ( 6, "  writeGpioState ( kGPIO_Selector_AnalogCodecReset %d, gpioState %d = kGPIO_Run )", selector, gpioState );
				}
				else
				{
					debugIOLog ( 6, "  writeGpioState ( kGPIO_Selector_AnalogCodecReset %d, gpioState %d )", selector, gpioState );
				}
			}
			else if ( kGPIO_Selector_DigitalCodecReset == selector )
			{
				if ( kGPIO_Reset == gpioState )
				{
					debugIOLog ( 6, "  writeGpioState ( kGPIO_Selector_DigitalCodecReset %d, gpioState %d = kGPIO_Reset )", selector, gpioState );
				} 
				else if ( kGPIO_Run == gpioState )
				{
					debugIOLog ( 6, "  writeGpioState ( kGPIO_Selector_DigitalCodecReset %d, gpioState %d = kGPIO_Run )", selector, gpioState );
				}
				else
				{
					debugIOLog ( 6, "  writeGpioState ( kGPIO_Selector_DigitalCodecReset %d, gpioState %d )", selector, gpioState );
				}
			}
#endif
			switch ( selector )
			{
				case kGPIO_Selector_AnalogCodecReset:	mAppleGPIO_AnalogCodecReset = gpioState;															break;
				case kGPIO_Selector_ClockMux:			mAppleGPIO_CodecClockMux = gpioState;																break;
				case kGPIO_Selector_DigitalCodecReset:	mAppleGPIO_DigitalCodecReset = gpioState;															break;
				case kGPIO_Selector_HeadphoneMute:		mAppleGPIO_HeadphoneMute = gpioState;																break;
				case kGPIO_Selector_InputDataMux:		mAppleGPIO_CodecInputDataMux = gpioState;															break;
				case kGPIO_Selector_LineOutMute:		mAppleGPIO_LineOutMute = gpioState;																	break;
				case kGPIO_Selector_SpeakerMute:		mAppleGPIO_AmpMute = gpioState;																		break;
				default:																																	break;
			}
		}
	}
	else
	{
		debugIOLog ( 5, "  PlatformInterfaceGPIO_PlatformFunction::writeGpioState() blocked due to 0 == mSystemIOControllerService" );
	}
Exit:
	return result;
FailExit:
#if DEBUGLOG
	switch ( selector )
	{
		case kGPIO_Selector_AnalogCodecReset:		FailIf ( true, FailExit );			break;
		case kGPIO_Selector_ClockMux:				FailIf ( true, FailExit );			break;
		case kGPIO_Selector_DigitalCodecReset:		FailIf ( true, FailExit );			break;
		case kGPIO_Selector_HeadphoneMute:			FailIf ( true, FailExit );			break;
		case kGPIO_Selector_InputDataMux:			FailIf ( true, FailExit );			break;
		case kGPIO_Selector_LineOutMute:			FailIf ( true, FailExit );			break;
		case kGPIO_Selector_SpeakerMute:			FailIf ( true, FailExit );			break;
		default:									FailIf ( true, FailExit );			break;   
	}
#endif
	goto Exit;
}

//	--------------------------------------------------------------------------------
//	Convert a GPIOAttribute to a binary value to be applied to a GPIO pin level
IOReturn PlatformInterfaceGPIO_PlatformFunction::translateGpioAttributeToGpioState ( GPIOType gpioType, GpioAttributes gpioAttribute, UInt32 * valuePtr )
{
	IOReturn		result;
	
	result = kIOReturnBadArgument;
	if ( 0 != valuePtr )
	{
		result = kIOReturnSuccess;
		switch ( gpioType )
		{
			case kGPIO_Type_MuteL:
				if ( kGPIO_Muted == gpioAttribute )
				{
					*valuePtr = 1;
				}
				else if ( kGPIO_Unmuted == gpioAttribute )
				{
					*valuePtr = 0;
				}
				else
				{
					result = kIOReturnBadArgument;
				}
				break;
			case kGPIO_Type_MuteH:
				if ( kGPIO_Muted == gpioAttribute )
				{
					*valuePtr = 1;
				}
				else if ( kGPIO_Unmuted == gpioAttribute )
				{
					*valuePtr = 0;
				}
				else
				{
					result = kIOReturnBadArgument;
				}
				break;
			case kGPIO_Type_Mux:
				if ( kGPIO_MuxSelectAlternate == gpioAttribute )
				{
					*valuePtr = 1;
				}
				else if ( kGPIO_MuxSelectDefault == gpioAttribute )
				{
					*valuePtr = 0;
				} else {
					result = kIOReturnBadArgument;
				}
				break;
			case kGPIO_Type_Reset:
				if ( kGPIO_Run == gpioAttribute )
				{
					*valuePtr = 0;
				}
				else if ( kGPIO_Reset == gpioAttribute )
				{
					*valuePtr = 1;
				}
				else
				{
					result = kIOReturnBadArgument;
				}
				break;
			default:
				result = kIOReturnBadArgument;
				break;
		}
	}
	if ( kIOReturnSuccess != result ) 
	{ 
		switch ( gpioType )
		{
			case kGPIO_Type_ConnectorType:	debugIOLog (5,  "FAIL: PlatformInterfaceGPIO_PlatformFunction::translateGpioAttributeToGpioState ( kGPIO_Type_ConnectorType, %p )", valuePtr ); 	break;
			case kGPIO_Type_Detect:			debugIOLog (5,  "FAIL: PlatformInterfaceGPIO_PlatformFunction::translateGpioAttributeToGpioState ( kGPIO_Type_Detect, %p )", valuePtr ); 			break;
			case kGPIO_Type_Irq:			debugIOLog (5,  "FAIL: PlatformInterfaceGPIO_PlatformFunction::translateGpioAttributeToGpioState ( kGPIO_Type_Irq, %p )", valuePtr ); 				break;
			case kGPIO_Type_MuteL:			debugIOLog (5,  "FAIL: PlatformInterfaceGPIO_PlatformFunction::translateGpioAttributeToGpioState ( kGPIO_Type_MuteL, %p )", valuePtr ); 			break;
			case kGPIO_Type_MuteH:			debugIOLog (5,  "FAIL: PlatformInterfaceGPIO_PlatformFunction::translateGpioAttributeToGpioState ( kGPIO_Type_MuteH, %p )", valuePtr ); 			break;
			case kGPIO_Type_Mux:			debugIOLog (5,  "FAIL: PlatformInterfaceGPIO_PlatformFunction::translateGpioAttributeToGpioState ( kGPIO_Type_Mux, %p )", valuePtr ); 				break;
			case kGPIO_Type_Reset:			debugIOLog (5,  "FAIL: PlatformInterfaceGPIO_PlatformFunction::translateGpioAttributeToGpioState ( kGPIO_Type_Reset, %p )", valuePtr ); 			break;
			default:						debugIOLog (5,  "FAIL: PlatformInterfaceGPIO_PlatformFunction::translateGpioAttributeToGpioState ( %X, %p )", gpioType, valuePtr ); 				break;
		}
	}
	return result;
}


