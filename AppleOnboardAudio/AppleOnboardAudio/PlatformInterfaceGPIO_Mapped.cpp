/*
 *	PlatformInterfaceGPIO_Mapped.cpp
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include "PlatformInterfaceGPIO_Mapped.h"
#include "AppleOnboardAudioUserClient.h"


#define super PlatformInterfaceGPIO

OSDefineMetaClassAndStructors ( PlatformInterfaceGPIO_Mapped, PlatformInterfaceGPIO )

const UInt16 PlatformInterfaceGPIO_Mapped::kAPPLE_IO_CONFIGURATION_SIZE		= kPlatformInterfaceSupportMappedCommon_APPLE_IO_CONFIGURATION_SIZE;
const UInt16 PlatformInterfaceGPIO_Mapped::kI2S_IO_CONFIGURATION_SIZE		= kPlatformInterfaceSupportMappedCommon_I2S_IO_CONFIGURATION_SIZE;

const UInt32 PlatformInterfaceGPIO_Mapped::kI2S0BaseOffset					= kPlatformInterfaceSupportMappedCommon_I2S0BaseOffset;
const UInt32 PlatformInterfaceGPIO_Mapped::kI2S1BaseOffset					= kPlatformInterfaceSupportMappedCommon_I2S1BaseOffset;

const char *	PlatformInterfaceGPIO_Mapped::kAmpMuteEntry					= kPlatformInterfaceSupportMappedCommon_AmpMuteEntry;
const char *	PlatformInterfaceGPIO_Mapped::kAnalogHWResetEntry			= kPlatformInterfaceSupportMappedCommon_AnalogHWResetEntry;
const char *	PlatformInterfaceGPIO_Mapped::kClockMuxEntry				= kPlatformInterfaceSupportMappedCommon_ClockMuxEntry;
const char *	PlatformInterfaceGPIO_Mapped::kCodecErrorIrqTypeEntry		= kPlatformInterfaceSupportMappedCommon_CodecErrorIrqTypeEntry;
const char *	PlatformInterfaceGPIO_Mapped::kCodecIrqTypeEntry			= kPlatformInterfaceSupportMappedCommon_CodecIrqTypeEntry;
const char *	PlatformInterfaceGPIO_Mapped::kComboInJackTypeEntry			= kPlatformInterfaceSupportMappedCommon_ComboInJackTypeEntry;
const char *	PlatformInterfaceGPIO_Mapped::kComboOutJackTypeEntry		= kPlatformInterfaceSupportMappedCommon_ComboOutJackTypeEntry;
const char *	PlatformInterfaceGPIO_Mapped::kDigitalHWResetEntry			= kPlatformInterfaceSupportMappedCommon_DigitalHWResetEntry;
const char *	PlatformInterfaceGPIO_Mapped::kDigitalInDetectEntry			= kPlatformInterfaceSupportMappedCommon_DigitalInDetectEntry;
const char *	PlatformInterfaceGPIO_Mapped::kDigitalOutDetectEntry		= kPlatformInterfaceSupportMappedCommon_DigitalOutDetectEntry;
const char *	PlatformInterfaceGPIO_Mapped::kHeadphoneDetectInt			= kPlatformInterfaceSupportMappedCommon_HeadphoneDetectInt;
const char *	PlatformInterfaceGPIO_Mapped::kHeadphoneMuteEntry 			= kPlatformInterfaceSupportMappedCommon_HeadphoneMuteEntry;
const char *	PlatformInterfaceGPIO_Mapped::kInternalSpeakerIDEntry		= kPlatformInterfaceSupportMappedCommon_InternalSpeakerIDEntry;
const char *	PlatformInterfaceGPIO_Mapped::kLineInDetectInt				= kPlatformInterfaceSupportMappedCommon_LineInDetectInt;
const char *	PlatformInterfaceGPIO_Mapped::kLineOutDetectInt				= kPlatformInterfaceSupportMappedCommon_LineOutDetectInt;
const char *	PlatformInterfaceGPIO_Mapped::kLineOutMuteEntry				= kPlatformInterfaceSupportMappedCommon_LineOutMuteEntry;
const char *	PlatformInterfaceGPIO_Mapped::kSpeakerDetectEntry			= kPlatformInterfaceSupportMappedCommon_SpeakerDetectEntry;

const char *	PlatformInterfaceGPIO_Mapped::kAudioGPIO					= kPlatformInterfaceSupportMappedCommon_AudioGPIO;
const char *	PlatformInterfaceGPIO_Mapped::kAudioGPIOActiveState			= kPlatformInterfaceSupportMappedCommon_AudioGPIOActiveState;

#pragma mark ¥
#pragma mark ¥ UNIX Like Functions
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceGPIO_Mapped::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex )
{
	bool					result = FALSE;
	IOService* 				theService = 0;
	IORegistryEntry *		macio = 0;
	IORegistryEntry *		gpio = 0;
	IORegistryEntry *		i2s = 0;
	IORegistryEntry *		i2sParent = 0;
	IOMemoryMap *			map = 0;

	debugIOLog ( 3, "+ PlatformInterfaceGPIO_Mapped::init ( IOService * device %p, AppleOnboardAudio * provider %p, UInt32 inDBDMADeviceIndex %d )", device, provider, inDBDMADeviceIndex );
	
	FailIf ( 0 == provider, Exit );
	FailIf ( 0 == device, Exit );
	
	mProvider = provider;
	mEnableAmplifierMuteRelease = FALSE;

	result = super::init ( device, provider, inDBDMADeviceIndex );
	if ( result )
	{
		result = FALSE;
		mKeyLargoService = IOService::waitForService ( IOService::serviceMatching ( "KeyLargo" ) );
		FailIf ( 0 == mKeyLargoService, Exit );
		
		debugIOLog ( 3, "  device name is '%s'",  ( (IORegistryEntry*)device)->getName () );
		
		
		i2s =  ( ( IORegistryEntry*)device)->getParentEntry ( gIODTPlane );
		FailWithAction ( 0 == i2s, result = false, Exit );
		debugIOLog ( 3, "  parent name is '%s'", i2s->getName () );

		if ( 0 == strcmp ( "i2s-a", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell0;
		}
		else if ( 0 == strcmp ( "i2s-b", i2s->getName () ) ) 
		{
			mI2SInterfaceNumber = kUseI2SCell1;
		}
		else if ( 0 == strcmp ( "i2s-c", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell2;
		}
		else if ( 0 == strcmp ( "i2s-d", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell3;
		}
		else if ( 0 == strcmp ( "i2s-e", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell4;
		}
		else if ( 0 == strcmp ( "i2s-f", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell5;
		}
		else if ( 0 == strcmp ( "i2s-g", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell6;
		}
		else if ( 0 == strcmp ( "i2s-h", i2s->getName () ) )
		{
			mI2SInterfaceNumber = kUseI2SCell7;
		}
		debugIOLog ( 5, "  mI2SInterfaceNumber = %d", mI2SInterfaceNumber );
		
		i2sParent = i2s->getParentEntry ( gIODTPlane );
		FailWithAction ( 0 == i2sParent, result = false, Exit );
		debugIOLog ( 3, "  parent name of '%s' is %s", i2s->getName (), i2sParent->getName () );

		macio = i2sParent->getParentEntry ( gIODTPlane );
		FailIf ( 0 == macio, Exit );
		debugIOLog ( 3, "  name of parent of '%s' is '%s'", i2sParent->getName (), macio->getName () );
		
		gpio = macio->childFromPath ( kGPIODTEntry, gIODTPlane);
		FailIf ( 0 == gpio, Exit );
		debugIOLog ( 3, "  gpio's name is '%s'", gpio->getName () );

		theService = ( OSDynamicCast ( IOService, i2s ) );
		FailIf ( 0 == theService, Exit );

		map = theService->mapDeviceMemoryWithIndex ( inDBDMADeviceIndex );
		FailIf ( 0 == map, Exit );

		// cache the config space
		mSoundConfigSpace = (UInt8 *)map->getPhysicalAddress();

		// sets the clock base address figuring out which I2S cell we're on
		if ((((UInt32)mSoundConfigSpace ^ kI2S0BaseOffset) & 0x0001FFFF) == 0) 
		{
			//	[3060321]	ioBaseAddress is required by this object in order to enable the target
			//				I2S I/O Module for which this object is to service.  The I2S I/O Module
			//				enable occurs through the configuration registers which reside in the
			//				first block of ioBase.		rbm		2 Oct 2002
			mIOBaseAddress = (void *)( (UInt32)mSoundConfigSpace - kI2S0BaseOffset );
			mIOBaseAddressMemory = IODeviceMemory::withRange ( (IOPhysicalAddress)((UInt8 *)mSoundConfigSpace - kI2S0BaseOffset), 256 );
			mI2SInterfaceNumber = kUseI2SCell0;
		}
		else if ((((UInt32)mSoundConfigSpace ^ kI2S1BaseOffset) & 0x0001FFFF) == 0) 
		{
			//	[3060321]	ioBaseAddress is required by this object in order to enable the target
			//				I2S I/O Module for which this object is to service.  The I2S I/O Module
			//				enable occurs through the configuration registers which reside in the
			//				first block of ioBase.		rbm		2 Oct 2002
			mIOBaseAddress = (void *)( (UInt32)mSoundConfigSpace - kI2S1BaseOffset );
			mIOBaseAddressMemory = IODeviceMemory::withRange ( (IOPhysicalAddress)((UInt8 *)mSoundConfigSpace - kI2S1BaseOffset), 256 );
			mI2SInterfaceNumber = kUseI2SCell1;
		}
		else 
		{
			debugIOLog ( 3, "AudioI2SControl::init ERROR: unable to setup ioBaseAddress and i2SInterfaceNumber" );
			FailIf ( TRUE, Exit );
		}
		FailIf ( 0 == mIOBaseAddressMemory, Exit );

		//	[3060321]	ioConfigurationBaseAddress is required by this object in order to enable the target
		//				I2S I/O Module for which this object is to service.  The I2S I/O Module
		//				enable occurs through the configuration registers which reside in the
		//				first block of ioBase.		rbm		2 Oct 2002
		
		mIOConfigurationBaseAddress = (void *)mIOBaseAddressMemory->map()->getVirtualAddress();
		FailIf ( 0 == mIOConfigurationBaseAddress, Exit );

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
		mIOI2SBaseAddressMemory = IODeviceMemory::withRange ( (IOPhysicalAddress)((UInt8 *)mSoundConfigSpace), kI2S_IO_CONFIGURATION_SIZE );
		FailIf ( 0 == mIOI2SBaseAddressMemory, Exit );
		
		mI2SBaseAddress = (void *)mIOI2SBaseAddressMemory->map()->getVirtualAddress();
		FailIf ( 0 == mI2SBaseAddress, Exit );
		
		debugIOLog ( 3, "  mI2SInterfaceNumber         0x%X", mI2SInterfaceNumber );
		debugIOLog ( 3, "  mIOBaseAddress              %p (Physical Address)", mIOBaseAddress );
		debugIOLog ( 3, "  mIOBaseAddressMemory        %p", mIOBaseAddressMemory );
		debugIOLog ( 3, "  mIOI2SBaseAddressMemory     %p", mIOI2SBaseAddressMemory );
		debugIOLog ( 3, "  mI2SBaseAddress             %p", mI2SBaseAddress );
		debugIOLog ( 3, "  mIOConfigurationBaseAddress %p", mIOConfigurationBaseAddress );
	}

	//	NOTE:	The discovery of the GPIO features is performed where the flat device tree hierarchy has been in use
	//			prior to introduction of platform functions and support of multiple instances of built-in audio devices
	//			so the I2S interface reference number is used here to differentiate between GPIO features associated
	//			with a built in audio device associated with a specific I2S I/O Module.
	
	if ( kUseI2SCell0 == mI2SInterfaceNumber )
	{
		initAudioGpioPtr ( gpio, kAmpMuteEntry, &mAmplifierMuteGpio, &mAmplifierMuteActiveState, 0 );												//	control - no interrupt
		initAudioGpioPtr ( gpio, kAnalogHWResetEntry, &mAnalogResetGpio, &mAnalogResetActiveState, 0 );												//	control - no interrupt
		initAudioGpioPtr ( gpio, kClockMuxEntry, &mClockMuxGpio, &mClockMuxActiveState, 0 );														//	control - no interrupt
		initAudioGpioPtr ( gpio, kComboInJackTypeEntry, &mComboInJackTypeGpio, &mComboInJackTypeActiveState, &mComboInDetectIntProvider );			//	detect  - does interrupt
		initAudioGpioPtr ( gpio, kComboOutJackTypeEntry, &mComboOutJackTypeGpio, &mComboOutJackTypeActiveState, &mComboOutDetectIntProvider );		//	detect  - does interrupt
		initAudioGpioPtr ( gpio, kDigitalInDetectEntry, &mDigitalInDetectGpio, &mDigitalInDetectActiveState, &mDigitalInDetectIntProvider );		//	detect  - does interrupt
		initAudioGpioPtr ( gpio, kDigitalOutDetectEntry, &mDigitalOutDetectGpio, &mDigitalOutDetectActiveState, &mDigitalOutDetectIntProvider );	//	detect  - does interrupt
		initAudioGpioPtr ( gpio, kHeadphoneDetectInt, &mHeadphoneDetectGpio, &mHeadphoneDetectActiveState, &mHeadphoneDetectIntProvider );			//	detect  - does interrupt
		initAudioGpioPtr ( gpio, kHeadphoneMuteEntry, &mHeadphoneMuteGpio, &mHeadphoneMuteActiveState, 0 );											//	control - no interrupt
		initAudioGpioPtr ( gpio, kInternalSpeakerIDEntry, &mInternalSpeakerIDGpio, &mInternalSpeakerIDActiveState, 0 );								//	control - no interrupt
		initAudioGpioPtr ( gpio, kLineInDetectInt, &mLineInDetectGpio, &mLineInDetectActiveState, &mLineInDetectIntProvider );						//	detect  - does interrupt
		initAudioGpioPtr ( gpio, kLineOutDetectInt, &mLineOutDetectGpio, &mLineOutDetectActiveState, &mLineOutDetectIntProvider );					//	detect  - does interrupt
		initAudioGpioPtr ( gpio, kLineOutMuteEntry, &mLineOutMuteGpio, &mLineOutMuteActiveState, 0 );												//	control - no interrupt
		initAudioGpioPtr ( gpio, kSpeakerDetectEntry, &mSpeakerDetectGpio, &mSpeakerDetectActiveState, &mSpeakerDetectIntProvider );				//	detect  - does interrupt
	}
	else
	{
		initAudioGpioPtr ( gpio, kCodecErrorIrqTypeEntry, &mCodecErrorInterruptGpio, &mCodecErrorInterruptActiveState, &mCodecErrorIntProvider );	//	control - does interrupt
		initAudioGpioPtr ( gpio, kCodecIrqTypeEntry, &mCodecInterruptGpio, &mCodecInterruptActiveState, &mCodecIntProvider );						//	control - does interrupt
		initAudioGpioPtr ( gpio, kDigitalHWResetEntry, &mDigitalResetGpio, &mDigitalResetActiveState, 0 );											//	control - no interrupt
	}
	result = TRUE;

Exit:
	debugIOLog ( 3, "- PlatformInterfaceGPIO_Mapped::init ( IOService * device %p, AppleOnboardAudio * provider %p, UInt32 inDBDMADeviceIndex %d ) returns %d", device, provider, inDBDMADeviceIndex, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceGPIO_Mapped::free()
{
	debugIOLog ( 3, "+ PlatformInterfaceGPIO_Mapped::free()" );

 	if ( 0 != mIOBaseAddressMemory )
	{
		mIOBaseAddressMemory->release();
	}
	super::free();
	debugIOLog ( 3, "- PlatformInterfaceGPIO_Mapped::free()" );
}

#pragma mark ¥
#pragma mark ¥ Power Management
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_Mapped::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	return result;
}
	

#pragma mark ¥
#pragma mark ¥ GPIO Methods
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_Mapped::setClockMux ( GpioAttributes muxState )
{
	return setGpioAttributes ( kGPIO_Selector_ClockMux, muxState );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getClockMux ()
{
	return getGpioAttributes ( kGPIO_Selector_ClockMux );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getCodecErrorInterrupt ()
{
	return getGpioAttributes ( kGPIO_Selector_CodecErrorInterrupt );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getCodecInterrupt ()
{
	return getGpioAttributes ( kGPIO_Selector_CodecInterrupt );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getComboInJackTypeConnected ()
{
	return getGpioAttributes ( kGPIO_Selector_ComboInJackType );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getComboOutJackTypeConnected ()
{
	return getGpioAttributes ( kGPIO_Selector_ComboOutJackType );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getDigitalInConnected ( GPIOSelector association )
{
	return getGpioAttributes ( kGPIO_Selector_DigitalInDetect );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getDigitalOutConnected ( GPIOSelector association )
{
	return getGpioAttributes ( kGPIO_Selector_DigitalOutDetect );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getHeadphoneConnected ()
{
	return getGpioAttributes ( kGPIO_Selector_HeadphoneDetect );
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_Mapped::setHeadphoneMuteState ( GpioAttributes muteState )
{
	IOReturn		result = kIOReturnSuccess;
	
	if ( mEnableAmplifierMuteRelease || ( kGPIO_Muted == muteState ) )		//	[3514762]
	{
		result = setGpioAttributes ( kGPIO_Selector_HeadphoneMute, muteState );
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes 	PlatformInterfaceGPIO_Mapped::getHeadphoneMuteState () 
{
	return getGpioAttributes ( kGPIO_Selector_HeadphoneMute );
}
	
//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_Mapped::setInputDataMux (GpioAttributes muxState) 
{
	return setGpioAttributes ( kGPIO_Selector_InputDataMux, muxState );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getInputDataMux () 
{
	return getGpioAttributes ( kGPIO_Selector_InputDataMux );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped:: getInternalSpeakerID () 
{
	return getGpioAttributes ( kGPIO_Selector_InternalSpeakerID );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getLineInConnected () 
{
	return getGpioAttributes ( kGPIO_Selector_LineInDetect );
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getLineOutConnected () 
{
	return getGpioAttributes ( kGPIO_Selector_LineOutDetect );
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_Mapped::setLineOutMuteState ( GpioAttributes muteState ) 
{
	IOReturn		result = kIOReturnSuccess;
	
	if ( mEnableAmplifierMuteRelease || ( kGPIO_Muted == muteState ) )		//	[3514762]
	{
		result = setGpioAttributes ( kGPIO_Selector_LineOutMute, muteState );
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getLineOutMuteState ()
{
	return getGpioAttributes ( kGPIO_Selector_LineOutMute );
}
	
//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getSpeakerConnected ()
{
	return getGpioAttributes ( kGPIO_Selector_SpeakerDetect );
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_Mapped::setSpeakerMuteState ( GpioAttributes muteState )
{
	IOReturn		result = kIOReturnSuccess;
	
	debugIOLog ( 6, "+ PlatformInterfaceGPIO_Mapped::setSpeakerMuteState ( %d ) where mEnableAmplifierMuteRelease = %d", muteState, mEnableAmplifierMuteRelease );
	if ( mEnableAmplifierMuteRelease || ( kGPIO_Muted == muteState ) )		//	[3514762]
	{
		result = setGpioAttributes ( kGPIO_Selector_SpeakerMute, muteState );
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
GpioAttributes	PlatformInterfaceGPIO_Mapped::getSpeakerMuteState ()
{
	return getGpioAttributes ( kGPIO_Selector_SpeakerMute );
}
	
//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_Mapped::setCodecReset ( CODEC_RESET target, GpioAttributes reset )
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterfaceGPIO_Mapped::setCodecReset ( %d, %d )", target, reset );
	if ( kCODEC_RESET_Analog == target )
	{
		if ( 0 != mAnalogResetGpio )
		{
			debugIOLog ( 6, "  about to set analog codec reset to %d", reset );
			result = setGpioAttributes ( kGPIO_Selector_AnalogCodecReset, reset );
		}
	}
	else if ( kCODEC_RESET_Digital == target )
	{
		if ( 0 != mDigitalResetGpio )
		{
			debugIOLog ( 6, "  about to set digital codec reset to %d", reset );
			result = setGpioAttributes ( kGPIO_Selector_DigitalCodecReset, reset );
		}
	}
	debugIOLog ( 6, "- PlatformInterfaceGPIO_Mapped::setCodecReset ( %d, %d ) returns 0x%lX", target, reset, result );
	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterfaceGPIO_Mapped::getCodecReset ( CODEC_RESET target )
{
	GpioAttributes	reset = kGPIO_Unknown;

	if ( kCODEC_RESET_Analog == target )
	{
		if ( 0 != mAnalogResetGpio )
		{
			reset = getGpioAttributes ( kGPIO_Selector_AnalogCodecReset );
		}
	}
	else if ( kCODEC_RESET_Digital == target )
	{
		if ( 0 != mDigitalResetGpio )
		{
			reset = getGpioAttributes ( kGPIO_Selector_DigitalCodecReset );
		}
	}
	return reset;
}

//	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3514762]
void PlatformInterfaceGPIO_Mapped::enableAmplifierMuteRelease ( void )
{
	debugIOLog ( 3, "± PlatformInterfaceGPIO_Mapped::enableAmplifierMuteRelease enabling amplifier mutes" );
	mEnableAmplifierMuteRelease = TRUE;
}


#pragma mark ¥
#pragma mark ¥ Set Interrupt Handler Methods
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_Mapped::disableInterrupt ( IOService * device, PlatformInterruptSource source )
{
	IOReturn		result = kIOReturnError;
	
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_Mapped::enableInterrupt ( IOService * device, PlatformInterruptSource source )
{
	IOReturn		result = kIOReturnSuccess;
	
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_Mapped::registerInterruptHandler ( IOService * device, void * interruptHandler, PlatformInterruptSource source )
{
	IOReturn				result;

	debugIOLog ( 6, "+ PlatformInterfaceGPIO_Mapped::registerInterruptHandler ( IOService * device %p, void * interruptHandler %p, PlatformInterruptSource source %d )", device, interruptHandler, source );
	result = kIOReturnError;
	switch ( source )
	{
		case kCodecInterrupt: 				result = setCodecInterruptHandler ( device, interruptHandler );					break;
		case kCodecErrorInterrupt: 			result = setCodecErrorInterruptHandler ( device, interruptHandler );			break;
		case kDigitalInDetectInterrupt: 	result = setDigitalInDetectInterruptHandler ( device, interruptHandler );		break;
		case kDigitalOutDetectInterrupt: 	result = setDigitalOutDetectInterruptHandler ( device, interruptHandler );		break;
		case kHeadphoneDetectInterrupt: 	result = setHeadphoneDetectInterruptHandler ( device, interruptHandler );		break;
		case kLineInputDetectInterrupt: 	result = setLineInDetectInterruptHandler ( device, interruptHandler );			break;
		case kLineOutputDetectInterrupt: 	result = setLineOutDetectInterruptHandler ( device, interruptHandler );			break;
		case kSpeakerDetectInterrupt: 		result = setSpeakerDetectInterruptHandler ( device, interruptHandler );			break;
		case kUnknownInterrupt:
		default:							debugIOLog ( 3,  "Attempt to register unknown interrupt source %d", source );	break;
	}
	debugIOLog ( 6, "- PlatformInterfaceGPIO_Mapped::registerInterruptHandler ( IOService * device %p, void * interruptHandler %p, PlatformInterruptSource source %d ) returns 0x%lX", device, interruptHandler, source, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceGPIO_Mapped::unregisterInterruptHandler (IOService * device, void * interruptHandler, PlatformInterruptSource source )
{
	IOReturn				result;

	result = kIOReturnError;
	switch ( source ) 
	{
		case kCodecInterrupt: 				result = setCodecInterruptHandler ( device, 0 );								break;
		case kCodecErrorInterrupt: 			result = setCodecErrorInterruptHandler ( device, 0 );							break;
		case kDigitalInDetectInterrupt: 	result = setDigitalInDetectInterruptHandler ( device, 0 );						break;
		case kDigitalOutDetectInterrupt: 	result = setDigitalOutDetectInterruptHandler ( device, 0 );						break;
		case kHeadphoneDetectInterrupt: 	result = setHeadphoneDetectInterruptHandler ( device, 0 );						break;
		case kLineInputDetectInterrupt: 	result = setLineInDetectInterruptHandler ( device, 0 );							break;
		case kLineOutputDetectInterrupt: 	result = setLineOutDetectInterruptHandler ( device, 0 );						break;
		case kSpeakerDetectInterrupt: 		result = setSpeakerDetectInterruptHandler ( device, 0 );						break;
		case kUnknownInterrupt:
		default:							debugIOLog ( 3,  "Attempt to register unknown interrupt source %d", source );	break;
	}
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_Mapped::setHeadphoneDetectInterruptHandler ( IOService* device, void* interruptHandler )
{
	IOReturn					result = kIOReturnError;
	
	debugIOLog ( 3,  "+ PlatformInterfaceGPIO_Mapped::setHeadphoneDetectInterruptHandler ( %p, %p )", device, interruptHandler );
	
	result = kIOReturnError;

	FailIf ( 0 == mWorkLoop, Exit );

	if ( 0 == interruptHandler && 0 != mHeadphoneDetectIntEventSource)
	{
		mHeadphoneDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource ( mHeadphoneDetectIntEventSource );	
		mHeadphoneDetectIntEventSource = 0;
	}
	else
	{
		FailIf ( 0 == mHeadphoneDetectIntProvider, Exit );
		debugIOLog ( 5, "  mHeadphoneDetectIntEventSource = IOInterruptEventSource::interruptEventSource ( device %p, (IOInterruptEventSource::Action)interruptHandler %p, mHeadphoneDetectIntProvider %p, 0", device, (IOInterruptEventSource::Action)interruptHandler, mHeadphoneDetectIntProvider );
		mHeadphoneDetectIntEventSource = IOInterruptEventSource::interruptEventSource ( device,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mHeadphoneDetectIntProvider,
																				0 );
		debugIOLog ( 5, "  mHeadphoneDetectIntEventSource %p", mHeadphoneDetectIntEventSource );
		FailIf ( 0 == mHeadphoneDetectIntEventSource, Exit );
		
		result = mWorkLoop->addEventSource ( mHeadphoneDetectIntEventSource );
		mHeadphoneDetectIntEventSource->enable ();
	}

Exit:
	if ( 0 != mHeadphoneDetectIntEventSource )
	{
		mHeadphoneDetectIntEventSource->release();
	}
	debugIOLog ( 3,  "- PlatformInterfaceGPIO_Mapped::setHeadphoneDetectInterruptHandler ( %p, %p ) returns 0x%lX", device, interruptHandler, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_Mapped::setSpeakerDetectInterruptHandler (IOService* device, void* interruptHandler)
{
	IOReturn					result = kIOReturnError;
	
	debugIOLog ( 3,  "+ PlatformInterfaceGPIO_Mapped::setSpeakerDetectInterruptHandler ( %p, %p )", device, interruptHandler );
	
	result = kIOReturnError;

	if ( 0 == interruptHandler && 0 != mSpeakerDetectIntEventSource)
	{
		mSpeakerDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource ( mSpeakerDetectIntEventSource );	
		mSpeakerDetectIntEventSource = 0;
	}
	else
	{
		FailIf ( 0 == mSpeakerDetectIntProvider, Exit );
		mSpeakerDetectIntEventSource = IOInterruptEventSource::interruptEventSource ( device,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mSpeakerDetectIntProvider,
																				0 );
		FailIf ( 0 == mSpeakerDetectIntEventSource, Exit );
		
		result = mWorkLoop->addEventSource ( mSpeakerDetectIntEventSource );	
		mSpeakerDetectIntEventSource->enable ();
	}

Exit:
	if ( 0 != mSpeakerDetectIntEventSource )
	{
		mSpeakerDetectIntEventSource->release();
	}
	debugIOLog ( 3,  "- PlatformInterfaceGPIO_Mapped::setSpeakerDetectInterruptHandler ( %p, %p ) returns 0x%lX", device, interruptHandler, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_Mapped::setLineOutDetectInterruptHandler (IOService* device, void* interruptHandler)
{
	IOReturn					result = kIOReturnError;
	
	debugIOLog ( 3,  "+ PlatformInterfaceGPIO_Mapped::setLineOutDetectInterruptHandler ( %p, %p )", device, interruptHandler );
	
	result = kIOReturnError;

	if ( 0 == interruptHandler && 0 != mLineOutDetectIntEventSource)
	{
		mLineOutDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource ( mLineOutDetectIntEventSource );
		mLineOutDetectIntEventSource = 0;
	}
	else
	{
		FailIf ( 0 == mLineOutDetectIntProvider, Exit );
		mLineOutDetectIntEventSource = IOInterruptEventSource::interruptEventSource ( device,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mLineOutDetectIntProvider,
																				0 );
		FailIf ( 0 == mLineOutDetectIntEventSource, Exit );
		
		result = mWorkLoop->addEventSource ( mLineOutDetectIntEventSource );	
		mLineOutDetectIntEventSource->enable ();
	}
Exit:
	if ( 0 != mLineOutDetectIntEventSource )
	{
		mLineOutDetectIntEventSource->release();
	}
	debugIOLog ( 3,  "- PlatformInterfaceGPIO_Mapped::setLineOutDetectInterruptHandler ( %p, %p ) returns 0x%lX", device, interruptHandler, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_Mapped::setLineInDetectInterruptHandler (IOService* device, void* interruptHandler)
{
	IOReturn					result = kIOReturnError;
	
	debugIOLog ( 3,  "+ PlatformInterfaceGPIO_Mapped::setLineInDetectInterruptHandler ( %p, %p )", device, interruptHandler );
	
	result = kIOReturnError;

	if ( 0 == interruptHandler && 0 != mLineInDetectIntEventSource)
	{
		mLineInDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource ( mLineInDetectIntEventSource );
		mLineInDetectIntEventSource = 0;
	}
	else
	{
		FailIf ( 0 == mLineInDetectIntProvider, Exit );
		mLineInDetectIntEventSource = IOInterruptEventSource::interruptEventSource ( device,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mLineInDetectIntProvider,
																				0 );
		FailIf ( 0 == mLineInDetectIntEventSource, Exit );
		
		result = mWorkLoop->addEventSource ( mLineInDetectIntEventSource );	
		mLineInDetectIntEventSource->enable ();
	}
Exit:
	if ( 0 != mLineInDetectIntEventSource )
	{
		mLineInDetectIntEventSource->release();
	}
	debugIOLog ( 3,  "- PlatformInterfaceGPIO_Mapped::setLineInDetectInterruptHandler ( %p, %p ) returns 0x%lX", device, interruptHandler, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_Mapped::setDigitalOutDetectInterruptHandler (IOService* device, void* interruptHandler)
{
	IOReturn					result = kIOReturnError;

	debugIOLog ( 3,  "+ PlatformInterfaceGPIO_Mapped::setDigitalOutDetectInterruptHandler ( %p, %p )", device, interruptHandler );
	
	if ( 0 == interruptHandler && 0 != mDigitalOutDetectIntEventSource )
	{
		mDigitalOutDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource ( mDigitalOutDetectIntEventSource );
		mDigitalOutDetectIntEventSource = 0;
	}
	else
	{
		FailIf ( 0 == mDigitalOutDetectIntProvider, Exit );
		mDigitalOutDetectIntEventSource = IOInterruptEventSource::interruptEventSource ( device,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mDigitalOutDetectIntProvider,
																				0 );
		FailIf ( 0 == mDigitalOutDetectIntEventSource, Exit );
		
		result = mWorkLoop->addEventSource ( mDigitalOutDetectIntEventSource );	
		mDigitalOutDetectIntEventSource->enable ();
	}
Exit:
	if ( 0 != mDigitalOutDetectIntEventSource )
	{
		mDigitalOutDetectIntEventSource->release();
	}
	debugIOLog ( 3,  "- PlatformInterfaceGPIO_Mapped::setDigitalOutDetectInterruptHandler ( %p, %p ) returns 0x%lX", device, interruptHandler, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_Mapped::setDigitalInDetectInterruptHandler (IOService* device, void* interruptHandler)
{
	IOReturn					result = kIOReturnError;

	debugIOLog ( 3,  "+ PlatformInterfaceGPIO_Mapped::setDigitalInDetectInterruptHandler ( %p, %p )", device, interruptHandler );
	
	if ( 0 == interruptHandler && 0 != mDigitalInDetectIntEventSource)
	{
		mDigitalInDetectIntEventSource->disable ();
		result = mWorkLoop->removeEventSource ( mDigitalInDetectIntEventSource);
		mDigitalInDetectIntEventSource = 0;
	}
	else
	{
		FailIf ( 0 == mDigitalInDetectIntProvider, Exit);
		mDigitalInDetectIntEventSource = IOInterruptEventSource::interruptEventSource ( device,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mDigitalInDetectIntProvider,
																				0 );
		FailIf ( 0 == mDigitalInDetectIntEventSource, Exit);
		
		result = mWorkLoop->addEventSource ( mDigitalInDetectIntEventSource );	
		mDigitalInDetectIntEventSource->enable ();
	}
Exit:
	if ( 0 != mDigitalInDetectIntEventSource )
	{
		mDigitalInDetectIntEventSource->release();
	}
	debugIOLog ( 3,  "- PlatformInterfaceGPIO_Mapped::setDigitalInDetectInterruptHandler ( %p, %p ) returns 0x%lX", device, interruptHandler, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_Mapped::setCodecInterruptHandler (IOService* device, void* interruptHandler)
{
	IOReturn					result = kIOReturnError;

	debugIOLog ( 3,  "+ PlatformInterfaceGPIO_Mapped::setCodecInterruptHandler ( %p, %p )", device, interruptHandler );
	
	if ( 0 == interruptHandler && 0 != mCodecInterruptEventSource)
	{
		mCodecInterruptEventSource->disable ();
		result = mWorkLoop->removeEventSource ( mCodecInterruptEventSource);
		mCodecInterruptEventSource = 0;
	}
	else
	{
		FailIf ( 0 == mCodecIntProvider, Exit );
		mCodecInterruptEventSource = IOInterruptEventSource::interruptEventSource ( device,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mCodecIntProvider,
																				0 );
		FailIf ( 0 == mDigitalInDetectIntEventSource, Exit );
		
		result = mWorkLoop->addEventSource ( mDigitalInDetectIntEventSource );	
		mDigitalInDetectIntEventSource->enable ();
	}
Exit:
	if ( 0 != mDigitalInDetectIntEventSource )
	{
		mDigitalInDetectIntEventSource->release();
	}
	debugIOLog ( 3,  "- PlatformInterfaceGPIO_Mapped::setCodecInterruptHandler ( %p, %p ) returns 0x%lX", device, interruptHandler, result );
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_Mapped::setCodecErrorInterruptHandler (IOService* device, void* interruptHandler)
{
	IOReturn					result = kIOReturnError;

	debugIOLog ( 3,  "+ PlatformInterfaceGPIO_Mapped::setCodecErrorInterruptHandler ( %p, %p )", device, interruptHandler );
	
	if ( 0 == interruptHandler && 0 != mCodecErrorInterruptEventSource) 
	{
		mCodecErrorInterruptEventSource->disable ();
		result = mWorkLoop->removeEventSource ( mCodecErrorInterruptEventSource );
		mCodecErrorInterruptEventSource = 0;
	}
	else
	{
		FailIf ( 0 == mCodecErrorIntProvider, Exit );
		mCodecErrorInterruptEventSource = IOInterruptEventSource::interruptEventSource ( device,
																				(IOInterruptEventSource::Action)interruptHandler,
																				mCodecErrorIntProvider,
																				0 );
		FailIf ( 0 == mDigitalInDetectIntEventSource, Exit );
		
		result = mWorkLoop->addEventSource ( mDigitalInDetectIntEventSource );	
		mDigitalInDetectIntEventSource->enable ();
	}
Exit:
	if ( 0 != mDigitalInDetectIntEventSource )
	{
		mDigitalInDetectIntEventSource->release();
	}
	debugIOLog ( 3,  "- PlatformInterfaceGPIO_Mapped::setCodecErrorInterruptHandler ( %p, %p ) returns 0x%lX", device, interruptHandler, result );
	return result;
}

#pragma mark ¥
#pragma mark ¥ Polling Support
#pragma mark ¥

//	--------------------------------------------------------------------------------
bool PlatformInterfaceGPIO_Mapped::interruptUsesTimerPolling ( PlatformInterruptSource source )
{
	bool			result = FALSE;
	
	if ( ( kCodecErrorInterrupt == source ) || ( kCodecInterrupt == source ) )
	{
		result = TRUE;
	}
	return result;
}


//	--------------------------------------------------------------------------------
void PlatformInterfaceGPIO_Mapped::poll ( void )
{
	IOCommandGate *			cg;

	debugIOLog ( 6, "+ PlatformInterfaceGPIO_Mapped::poll()" );
	
	FailIf ( 0 == mProvider, Exit );
	cg = mProvider->getCommandGate ();
	if ( 0 != cg )
	{
		if ( interruptUsesTimerPolling ( kCodecErrorInterrupt ) )
		{
			if ( kGPIO_Unknown != getCodecReset ( kCODEC_RESET_Digital ) )
			{
				debugIOLog ( 6, "  PlatformInterfaceGPIO_Mapped::poll() about to kCodecErrorInterruptStatus" );
				if ( interruptUsesTimerPolling ( kCodecErrorInterrupt ) )
				{
					cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kCodecErrorInterruptStatus, (void *)kGPIO_CodecInterruptActive, (void *)0 );
				}
			}
		}
		if ( interruptUsesTimerPolling ( kCodecInterrupt ) )
		{
			if ( mCodecInterruptEventSource )
			{
				debugIOLog ( 6, "  PlatformInterfaceGPIO_Mapped::poll() about to kCodecInterruptStatus" );
				if ( interruptUsesTimerPolling ( kCodecInterrupt ) )
				{
					cg->runAction ( mProvider->interruptEventHandlerAction, (void *)kCodecInterruptStatus, (void *)getCodecInterrupt (), (void *)0 );
				}
			}
		}
	}
Exit:
	debugIOLog ( 6, "- PlatformInterfaceGPIO_Mapped::poll()" );
	return;
}


#pragma mark ¥
#pragma mark ¥ Utility Functions
#pragma mark ¥

//	--------------------------------------------------------------------------------
IORegistryEntry *	PlatformInterfaceGPIO_Mapped::FindEntryByProperty (const IORegistryEntry * start, const char * key, const char * value)
{
	OSIterator				*iterator = 0;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSData					*tmpData;

	theEntry = 0;
	FailIf ( 0 == start, Exit );
	
	iterator = start->getChildIterator ( gIODTPlane );
	FailIf ( 0 == iterator, Exit );

	while ( 0 == theEntry && ( tmpReg = OSDynamicCast ( IORegistryEntry, iterator->getNextObject () ) ) != 0 )
	{
		tmpData = OSDynamicCast ( OSData, tmpReg->getProperty ( key ) );
		if ( 0 != tmpData )
		{
			if ( tmpData->isEqualTo ( value, strlen ( value ) ) )
			{
				theEntry = tmpReg;
			}
		}
	}

Exit:
	if ( 0 != iterator )
	{
		iterator->release ();
	}
	return theEntry;
}


//	--------------------------------------------------------------------------------
//	Finds the gpio in the IODevice plane of the device tree and obtains the address 
//	and active state of the GPIO to be used for later memory mapped I/O accesses.
void PlatformInterfaceGPIO_Mapped::initAudioGpioPtr ( const IORegistryEntry * start, const char * gpioName, GpioPtr * gpioH, GpioActiveState * gpioActiveStatePtr, IOService ** intProvider )
{
    IORegistryEntry			*theRegEntry;
	OSData					*theProperty;
	IOMemoryMap				*map;
	IODeviceMemory			*gpioRegMem;
	UInt32					*theGpioAddr;
	UInt32					*tmpPtr;
	
	debugIOLog ( 6, "+ PlatformInterfaceGPIO_Mapped::initAudioGpioPtr ( const IORegistryEntry * %p, const char * %p, GpioPtr * %p, GpioActiveState * %p, IOService ** %p )", start, gpioName, gpioH, gpioActiveStatePtr, intProvider );
	
	FailIf ( 0 == start, Exit );
	FailIf ( 0 == gpioName, Exit );
	FailIf ( 0 == gpioH, Exit );
	FailIf ( 0 == gpioActiveStatePtr, Exit );
	
	debugIOLog ( 6, "  mProvider           %p", mProvider );
	debugIOLog ( 6, "  this                %p", this );
	debugIOLog ( 6, "  gpioName            '%s'", gpioName );
	
	theRegEntry = FindEntryByProperty ( start, kAudioGPIO, gpioName );
	if ( 0 != theRegEntry )
	{
		if ( 0 != intProvider )
		{
			*intProvider = OSDynamicCast ( IOService, theRegEntry );
			debugIOLog ( 6, "  *intProvider        %p", *intProvider );
		}
		theProperty = OSDynamicCast ( OSData, theRegEntry->getProperty ( kAAPLAddress ) );
		if ( 0 != theProperty )
		{
			theGpioAddr = (UInt32*)theProperty->getBytesNoCopy ();
            if ( 0 != theGpioAddr )
			{
                debugIOLog ( 3, "  PlatformInterfaceGPIO_Mapped - '%s' = %p", gpioName, theGpioAddr );
				if ( 0 != gpioActiveStatePtr )
				{
					theProperty = OSDynamicCast ( OSData, theRegEntry->getProperty ( kAudioGPIOActiveState ) );
					if ( 0 != theProperty )
					{
						tmpPtr = (UInt32*)theProperty->getBytesNoCopy ();
						debugIOLog ( 3, "  PlatformInterfaceGPIO_Mapped - '%s' active state = 0x%X", gpioName, *gpioActiveStatePtr );
						*gpioActiveStatePtr = *tmpPtr;
					}
					else
					{
						*gpioActiveStatePtr = 1;
					}
						debugIOLog ( 6, "  *gpioActiveStatePtr %d", *gpioActiveStatePtr );
				}
				if ( 0 != gpioH )
				{
                    //	Take the hard coded memory address that's in the boot rom and convert it to a virtual address
                    gpioRegMem = IODeviceMemory::withRange ( *theGpioAddr, sizeof ( UInt8 ) );
                    map = gpioRegMem->map ( 0 );
                    *gpioH = (UInt8*)map->getVirtualAddress ();
					debugIOLog ( 6, "  *gpioH              %p", *gpioH );
				}
            }
		}
	}
Exit:
	debugIOLog ( 6, "- PlatformInterfaceGPIO_Mapped::initAudioGpioPtr ( %p, %p, %p, %p, %p )", start, gpioName, gpioH, gpioActiveStatePtr, intProvider );
	return;
}
	
//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceGPIO_Mapped::getGpioPtrAndActiveState ( GPIOSelector theGpio, GpioPtr * gpioPtrPtr, GpioActiveState * activeStatePtr )
{
	IOReturn			result;
	
	result = kIOReturnBadArgument;
	if ( 0 != gpioPtrPtr && 0 != activeStatePtr )
	{
		switch ( theGpio )
		{
			case kGPIO_Selector_AnalogCodecReset:		*gpioPtrPtr = mAnalogResetGpio;				*activeStatePtr = mAnalogResetActiveState;			break;
			case kGPIO_Selector_ClockMux:				*gpioPtrPtr = mClockMuxGpio;				*activeStatePtr = mClockMuxActiveState;				break;
			case kGPIO_Selector_CodecErrorInterrupt:	*gpioPtrPtr = mCodecErrorInterruptGpio;		*activeStatePtr = mCodecErrorInterruptActiveState;	break;
			case kGPIO_Selector_CodecInterrupt:			*gpioPtrPtr = mCodecInterruptGpio;			*activeStatePtr = mCodecInterruptActiveState;		break;
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
		if ( 0 != *gpioPtrPtr )
		{
			result = kIOReturnSuccess;
		}
	}
	return result;
}

//	--------------------------------------------------------------------------------
GpioAttributes PlatformInterfaceGPIO_Mapped::getGpioAttributes ( GPIOSelector theGpio )
{
	UInt8					gpioValue;
	GpioAttributes			result;
	GpioPtr					gpioPtr;
	GpioActiveState			activeState;

	result = kGPIO_Unknown;
	gpioValue = 0;
	gpioPtr = 0;
	activeState = 1;

	getGpioPtrAndActiveState ( theGpio, &gpioPtr, &activeState );
	if ( 0 != gpioPtr )
	{
		gpioValue = *gpioPtr;
	
		switch ( theGpio )
		{
			case kGPIO_Selector_AnalogCodecReset:
			case kGPIO_Selector_DigitalCodecReset:
				if ( ( gpioValue & ( gpioBIT_MASK << gpioPIN_RO ) ) == ( activeState << gpioPIN_RO ) )
				{
					result = kGPIO_Reset;
				} else {
					result = kGPIO_Run;
				}
				break;
			case kGPIO_Selector_ClockMux:
			case kGPIO_Selector_InputDataMux:
				if ( ( gpioValue & ( gpioBIT_MASK << gpioPIN_RO ) ) == ( activeState << gpioPIN_RO ) )
				{
					result = kGPIO_MuxSelectAlternate;
				}
				else
				{
					result = kGPIO_MuxSelectDefault;
				}
				break;
			case kGPIO_Selector_CodecErrorInterrupt:
			case kGPIO_Selector_CodecInterrupt:
				if ( ( gpioValue & ( gpioBIT_MASK << gpioPIN_RO ) ) == ( activeState << gpioPIN_RO ) )
				{
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
				if ( kGPIO_Selector_HeadphoneDetect == theGpio )
				{
					*gpioPtr |= ( dualEdge << intEdgeSEL );		//	This is required for ROMs that do not properly initialize the dual edge interrupt enable
				}
				if ( ( gpioValue & ( gpioBIT_MASK << gpioPIN_RO ) ) == ( activeState << gpioPIN_RO ) )
				{
					result = kGPIO_Connected;
				}
				else
				{
					result = kGPIO_Disconnected;
				}
				break;
			case kGPIO_Selector_HeadphoneMute:
			case kGPIO_Selector_LineOutMute:
			case kGPIO_Selector_SpeakerMute:
				if ( ( gpioValue & ( gpioBIT_MASK << gpioPIN_RO ) ) == ( activeState << gpioPIN_RO ) )
				{
					result = kGPIO_Muted;
				}
				else
				{
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
IOReturn PlatformInterfaceGPIO_Mapped::setGpioAttributes ( GPIOSelector theGpio, GpioAttributes attributes )
{
	UInt8					gpioValue;
	GpioPtr					gpioPtr;
	GpioActiveState			activeState;
	IOReturn				result;

	gpioValue = 0;
	gpioPtr = 0;
	activeState = 1;
	result = kIOReturnBadArgument;

	getGpioPtrAndActiveState ( theGpio, &gpioPtr, &activeState );
	if ( 0 != gpioPtr )
	{
		switch ( theGpio )
		{
			case kGPIO_Selector_AnalogCodecReset:		//	Fall through to kGPIO_Selector_DigitalCodecReset
			case kGPIO_Selector_DigitalCodecReset:
				if ( kGPIO_Reset == attributes ) 
				{
					gpioWrite ( gpioPtr, assertGPIO ( activeState) );
				}
				else if ( kGPIO_Run == attributes )
				{
					gpioWrite ( gpioPtr, negateGPIO ( activeState) );
				}
				break;
			case kGPIO_Selector_ClockMux:				//	Fall through to kGPIO_Selector_InputDataMux
			case kGPIO_Selector_InputDataMux:
				if ( kGPIO_MuxSelectAlternate == attributes )
				{
					gpioWrite ( gpioPtr, assertGPIO ( activeState) );
				}
				else if ( kGPIO_MuxSelectDefault == attributes )
				{
					gpioWrite ( gpioPtr, negateGPIO ( activeState) );
				}
				break;
			case kGPIO_Selector_CodecErrorInterrupt:	//	Fall through to kGPIO_Selector_SpeakerDetect
			case kGPIO_Selector_CodecInterrupt:			//	Fall through to kGPIO_Selector_SpeakerDetect
			case kGPIO_Selector_ComboInJackType:		//	Fall through to kGPIO_Selector_SpeakerDetect
			case kGPIO_Selector_ComboOutJackType:		//	Fall through to kGPIO_Selector_SpeakerDetect
			case kGPIO_Selector_DigitalInDetect:		//	Fall through to kGPIO_Selector_SpeakerDetect
			case kGPIO_Selector_DigitalOutDetect:		//	Fall through to kGPIO_Selector_SpeakerDetect
			case kGPIO_Selector_HeadphoneDetect:		//	Fall through to kGPIO_Selector_SpeakerDetect
			case kGPIO_Selector_InternalSpeakerID:		//	Fall through to kGPIO_Selector_SpeakerDetect
			case kGPIO_Selector_LineInDetect:			//	Fall through to kGPIO_Selector_SpeakerDetect
			case kGPIO_Selector_LineOutDetect:			//	Fall through to kGPIO_Selector_SpeakerDetect
			case kGPIO_Selector_SpeakerDetect:
				break;
			case kGPIO_Selector_HeadphoneMute:			//	Fall through to kGPIO_Selector_SpeakerMute
			case kGPIO_Selector_LineOutMute:			//	Fall through to kGPIO_Selector_SpeakerMute
			case kGPIO_Selector_SpeakerMute:
				if ( kGPIO_Muted == attributes )
				{
					gpioWrite ( gpioPtr, assertGPIO ( activeState) );
				}
				else if ( kGPIO_Unmuted == attributes )
				{
					gpioWrite ( gpioPtr, negateGPIO ( activeState) );
				}
				break;
			case kGPIO_Selector_ExternalMicDetect:		//	Fall through to kGPIO_Selector_SpeakerMute
			case kGPIO_Selector_NotAssociated:
				break;
		}
		if ( 0 != gpioPtr )
		{
			result = kIOReturnSuccess;
		}
	}
	return result;
}

//	--------------------------------------------------------------------------------
//	Sets the 'gpioDDR' to OUTPUT and sets the 'gpioDATA' to the 'data' state.
IOReturn PlatformInterfaceGPIO_Mapped::gpioWrite( UInt8* gpioAddress, UInt8 data )
{
	UInt8		gpioData;
	IOReturn	result = kIOReturnBadArgument;
	
	if( 0 != gpioAddress )
	{
		if( 0 == data )
		{
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 0 << gpioDATA );
		}
		else
		{
			gpioData = ( gpioDDR_OUTPUT << gpioDDR ) | ( 1 << gpioDATA );
		}
		*gpioAddress = gpioData;
		debugIOLog ( 6, "  gpioAddress %p -> 0x%X", gpioAddress, *gpioAddress );
		result = kIOReturnSuccess;
	}
	return result;
}


