/*
 *	PlatformInterfaceI2S_Mapped.h
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include "PlatformInterfaceI2S_Mapped.h"

#define super PlatformInterfaceI2S

OSDefineMetaClassAndStructors ( PlatformInterfaceI2S_Mapped, PlatformInterfaceI2S )

const UInt16 PlatformInterfaceI2S_Mapped::kAPPLE_IO_CONFIGURATION_SIZE	= kPlatformInterfaceSupportMappedCommon_APPLE_IO_CONFIGURATION_SIZE;
const UInt16 PlatformInterfaceI2S_Mapped::kI2S_IO_CONFIGURATION_SIZE	= kPlatformInterfaceSupportMappedCommon_I2S_IO_CONFIGURATION_SIZE;

const UInt32 PlatformInterfaceI2S_Mapped::kI2S0BaseOffset				= kPlatformInterfaceSupportMappedCommon_I2S0BaseOffset;
const UInt32 PlatformInterfaceI2S_Mapped::kI2S1BaseOffset				= kPlatformInterfaceSupportMappedCommon_I2S1BaseOffset;

const UInt32 PlatformInterfaceI2S_Mapped::kI2SIntCtlOffset				= kPlatformInterfaceSupportMappedCommon_I2SIntCtlOffset;
const UInt32 PlatformInterfaceI2S_Mapped::kI2SSerialFormatOffset		= kPlatformInterfaceSupportMappedCommon_I2SSerialFormatOffset;
const UInt32 PlatformInterfaceI2S_Mapped::kI2SCodecMsgOutOffset			= kPlatformInterfaceSupportMappedCommon_I2SCodecMsgOutOffset;
const UInt32 PlatformInterfaceI2S_Mapped::kI2SCodecMsgInOffset			= kPlatformInterfaceSupportMappedCommon_I2SCodecMsgInOffset;
const UInt32 PlatformInterfaceI2S_Mapped::kI2SFrameCountOffset			= kPlatformInterfaceSupportMappedCommon_I2SFrameCountOffset;
const UInt32 PlatformInterfaceI2S_Mapped::kI2SFrameMatchOffset			= kPlatformInterfaceSupportMappedCommon_I2SFrameMatchOffset;
const UInt32 PlatformInterfaceI2S_Mapped::kI2SDataWordSizesOffset		= kPlatformInterfaceSupportMappedCommon_I2SDataWordSizesOffset;
const UInt32 PlatformInterfaceI2S_Mapped::kI2SPeakLevelSelOffset		= kPlatformInterfaceSupportMappedCommon_I2SPeakLevelSelOffset;
const UInt32 PlatformInterfaceI2S_Mapped::kI2SPeakLevelIn0Offset		= kPlatformInterfaceSupportMappedCommon_I2SPeakLevelIn0Offset;
const UInt32 PlatformInterfaceI2S_Mapped::kI2SPeakLevelIn1Offset		= kPlatformInterfaceSupportMappedCommon_I2SPeakLevelIn1Offset;

#pragma mark ¥
#pragma mark ¥ UNIX Like Functions
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceI2S_Mapped::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex ) 
{
	bool					result = FALSE;
	IOService* 				theService;
	IORegistryEntry			*macio;
	IORegistryEntry			*gpio;
	IORegistryEntry			*i2s;
	IORegistryEntry			*i2sParent;
	IOMemoryMap				*map;

	debugIOLog ( 3, "+ PlatformInterfaceI2S_Mapped::init ( IOService * device %p, AppleOnboardAudio * provider %p, UInt32 inDBDMADeviceIndex %d )", device, provider, inDBDMADeviceIndex );
	
	FailIf ( NULL == provider, Exit );
	FailIf ( NULL == device, Exit );
	
	mProvider = provider;

	result = super::init ( device, provider, inDBDMADeviceIndex );
	if ( result ) 
	{
		mKeyLargoService = IOService::waitForService ( IOService::serviceMatching ( "KeyLargo" ) );
		debugIOLog ( 3, "  device name is %s",  ( (IORegistryEntry*)device)->getName () );
		
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
		FailWithAction ( 0 == macio, result = false, Exit );
		debugIOLog ( 3, "  macio's name is %s", macio->getName () );
		
		gpio = macio->childFromPath ( kGPIODTEntry, gIODTPlane);
		FailWithAction ( !gpio, result = false, Exit);
		debugIOLog ( 3, "  gpio's name is %s", gpio->getName () );

		theService = ( OSDynamicCast ( IOService, i2s ) );
		FailWithAction ( !theService, result = false, Exit );

		map = theService->mapDeviceMemoryWithIndex ( inDBDMADeviceIndex );
		FailWithAction ( 0 == map, result = false, Exit );

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
			debugIOLog (3, "  AudioI2SControl::init ERROR: unable to setup ioBaseAddress and i2SInterfaceNumber");
		}
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
		
		debugIOLog (3, "  mI2SInterfaceNumber         = %d", mI2SInterfaceNumber);
		debugIOLog (3, "  mIOI2SBaseAddressMemory     = %p", mIOI2SBaseAddressMemory);
		debugIOLog (3, "  mI2SBaseAddress             = %p", mI2SBaseAddress);
		debugIOLog (3, "  mIOBaseAddressMemory        = %p", mIOBaseAddressMemory);
		debugIOLog (3, "  mIOConfigurationBaseAddress = %p", mIOConfigurationBaseAddress);
	}
Exit:
	debugIOLog ( 3, "- PlatformInterfaceI2S_Mapped::init ( IOService * device %p, AppleOnboardAudio * provider%p, UInt32 inDBDMADeviceIndex %d ) returns %lX", device, provider, inDBDMADeviceIndex, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceI2S_Mapped::free() 
{
	debugIOLog (3, "+ PlatformInterfaceI2S_Mapped::free()");

 	if ( NULL != mIOBaseAddressMemory ) 
	{
		mIOBaseAddressMemory->release();
	}
	super::free();
	debugIOLog (3, "- PlatformInterfaceI2S_Mapped::free()");
}

#pragma mark ¥
#pragma mark ¥ POWER MANAGEMENT
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	return result;
}
	

#pragma mark ¥
#pragma mark ¥ I2S IOM ACCESS
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::setSerialFormatRegister ( UInt32 serialFormat ) 
{
	debugIOLog ( 3, "± PlatformInterfaceI2S_Mapped::setSerialFormatRegister ( 0x%0.8X )", serialFormat );
	OSWriteLittleInt32 ( mI2SBaseAddress, kI2SSerialFormatOffset, serialFormat );
	return kIOReturnSuccess;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_Mapped::getSerialFormatRegister () 
{
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SSerialFormatOffset);
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::setDataWordSizes ( UInt32 dataWordSizes ) 
{
	debugIOLog ( 3, "± PlatformInterfaceI2S_Mapped::setDataWordSizes ( 0x%0.8X )", dataWordSizes );
	OSWriteLittleInt32(mI2SBaseAddress, kI2SDataWordSizesOffset, dataWordSizes);
	return kIOReturnSuccess;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_Mapped::getDataWordSizes() 
{
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SDataWordSizesOffset);
	return result;
}
	
//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::setFrameCount ( UInt32 value ) 
{
	OSWriteLittleInt32 ( mI2SBaseAddress, kI2SFrameCountOffset, value );
	return kIOReturnSuccess;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_Mapped::getFrameCount () 
{
	return OSReadLittleInt32(mI2SBaseAddress, kI2SFrameCountOffset);
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::setI2SIOMIntControl ( UInt32 intCntrl ) 
{
	OSWriteLittleInt32(mI2SBaseAddress, kI2SIntCtlOffset, intCntrl);
	return kIOReturnSuccess;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_Mapped::getI2SIOMIntControl () 
{
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SIntCtlOffset);
	return result;
}
	
//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::setPeakLevel ( UInt32 channelTarget, UInt32 levelMeterValue ) 
{
	IOReturn		result = kIOReturnSuccess;
	
	switch ( channelTarget ) 
	{
		case kStreamFrontLeft:		OSWriteLittleInt32 ( mI2SBaseAddress, kI2SPeakLevelIn0Offset, levelMeterValue );		break;
		case kStreamFrontRight:		OSWriteLittleInt32 ( mI2SBaseAddress, kI2SPeakLevelIn1Offset, levelMeterValue );		break;
		default:					result = kIOReturnBadArgument;															break;
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_Mapped::getPeakLevel ( UInt32 channelTarget ) 
{
	UInt32			result;
	
	switch ( channelTarget ) 
	{
		case kStreamFrontLeft:		result = OSReadLittleInt32 ( mI2SBaseAddress, kI2SPeakLevelIn0Offset );					break;
		case kStreamFrontRight:		result = OSReadLittleInt32 ( mI2SBaseAddress, kI2SPeakLevelIn1Offset );					break;
		default:					result = 0;																				break;
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::setI2SIOM_CodecMsgOut ( UInt32 value ) 
{
	OSWriteLittleInt32(mI2SBaseAddress, kI2SCodecMsgOutOffset, value);
	return kIOReturnSuccess;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_Mapped::getI2SIOM_CodecMsgOut () 
{
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SCodecMsgOutOffset);
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::setI2SIOM_CodecMsgIn ( UInt32 value ) 
{
	OSWriteLittleInt32(mI2SBaseAddress, kI2SCodecMsgInOffset, value);
	return kIOReturnSuccess;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_Mapped::getI2SIOM_CodecMsgIn () 
{
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SCodecMsgInOffset);
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::setI2SIOM_FrameMatch ( UInt32 value ) 
{
	OSWriteLittleInt32(mI2SBaseAddress, kI2SFrameMatchOffset, value);
	return kIOReturnSuccess;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_Mapped::getI2SIOM_FrameMatch () 
{
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SFrameMatchOffset);
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::setI2SIOM_PeakLevelSel ( UInt32 value ) 
{
	OSWriteLittleInt32(mI2SBaseAddress, kI2SPeakLevelSelOffset, value);
	return kIOReturnSuccess;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_Mapped::getI2SIOM_PeakLevelSel () 
{
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SPeakLevelSelOffset);
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::setI2SIOM_PeakLevelIn0 ( UInt32 value ) 
{
	OSWriteLittleInt32(mI2SBaseAddress, kI2SPeakLevelIn0Offset, value);
	return kIOReturnSuccess;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_Mapped::getI2SIOM_PeakLevelIn0 () 
{
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SPeakLevelIn0Offset);
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_Mapped::setI2SIOM_PeakLevelIn1 ( UInt32 value ) 
{
	OSWriteLittleInt32(mI2SBaseAddress, kI2SPeakLevelIn1Offset, value);
	return kIOReturnSuccess;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_Mapped::getI2SIOM_PeakLevelIn1 () 
{
	UInt32 result = OSReadLittleInt32(mI2SBaseAddress, kI2SPeakLevelIn1Offset);
	return result;
}


	

