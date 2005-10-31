/*
 *	PlatformInterfaceFCR_Mapped.cpp
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include	"PlatformInterfaceFCR_Mapped.h"
#include	"PlatformInterfaceSupportMappedCommon.h"

#define super PlatformInterfaceFCR

#pragma mark ¥
#pragma mark ¥ UNIX Like Functions
#pragma mark ¥

OSDefineMetaClassAndStructors ( PlatformInterfaceFCR_Mapped, PlatformInterfaceFCR )

const UInt16 PlatformInterfaceFCR_Mapped::kAPPLE_IO_CONFIGURATION_SIZE	= kPlatformInterfaceSupportMappedCommon_APPLE_IO_CONFIGURATION_SIZE;
const UInt16 PlatformInterfaceFCR_Mapped::kI2S_IO_CONFIGURATION_SIZE	= kPlatformInterfaceSupportMappedCommon_I2S_IO_CONFIGURATION_SIZE;

const UInt32 PlatformInterfaceFCR_Mapped::kFCR0Offset					= kPlatformInterfaceSupportMappedCommon_FCR0Offset;
const UInt32 PlatformInterfaceFCR_Mapped::kFCR1Offset					= kPlatformInterfaceSupportMappedCommon_FCR1Offset;
const UInt32 PlatformInterfaceFCR_Mapped::kFCR2Offset					= kPlatformInterfaceSupportMappedCommon_FCR2Offset;
const UInt32 PlatformInterfaceFCR_Mapped::kFCR3Offset					= kPlatformInterfaceSupportMappedCommon_FCR3Offset;
const UInt32 PlatformInterfaceFCR_Mapped::kFCR4Offset					= kPlatformInterfaceSupportMappedCommon_FCR4Offset;

const UInt32 PlatformInterfaceFCR_Mapped::kI2S0BaseOffset				= kPlatformInterfaceSupportMappedCommon_I2S0BaseOffset;
const UInt32 PlatformInterfaceFCR_Mapped::kI2S1BaseOffset				= kPlatformInterfaceSupportMappedCommon_I2S1BaseOffset;

const UInt32 PlatformInterfaceFCR_Mapped::kI2SClockOffset				= kPlatformInterfaceSupportMappedCommon_I2SClockOffset;
const UInt32 PlatformInterfaceFCR_Mapped::kI2S0ClockEnable				= kPlatformInterfaceSupportMappedCommon_I2S0ClockEnable;
const UInt32 PlatformInterfaceFCR_Mapped::kI2S1ClockEnable				= kPlatformInterfaceSupportMappedCommon_I2S1ClockEnable;

const UInt32 PlatformInterfaceFCR_Mapped::kI2S0CellEnable	 		    = kPlatformInterfaceSupportMappedCommon_I2S0CellEnable;
const UInt32 PlatformInterfaceFCR_Mapped::kI2S1CellEnable	 			= kPlatformInterfaceSupportMappedCommon_I2S1CellEnable;

const UInt32 PlatformInterfaceFCR_Mapped::kI2S0InterfaceEnable			= kPlatformInterfaceSupportMappedCommon_I2S0InterfaceEnable;
const UInt32 PlatformInterfaceFCR_Mapped::kI2S1InterfaceEnable			= kPlatformInterfaceSupportMappedCommon_I2S1InterfaceEnable;

const UInt32 PlatformInterfaceFCR_Mapped::kI2S0SwReset					= kPlatformInterfaceSupportMappedCommon_I2S0SwReset;
const UInt32 PlatformInterfaceFCR_Mapped::kI2S1SwReset					= kPlatformInterfaceSupportMappedCommon_I2S1SwReset;

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceFCR_Mapped::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex )
{
	bool					result = FALSE;
	IOService* 				theService;
	IORegistryEntry			*macio;
	IORegistryEntry			*gpio;
	IORegistryEntry			*i2s;
	IORegistryEntry			*i2sParent;
	IOMemoryMap				*map;

	debugIOLog ( 3, "+ PlatformInterfaceFCR_Mapped::init ( IOService * device %p, AppleOnboardAudio * provider %p, UInt32 inDBDMADeviceIndex %d )", device, provider, inDBDMADeviceIndex );
	
	FailIf ( NULL == provider, Exit );
	FailIf ( NULL == device, Exit );
	
	mProvider = provider;

	result = super::init (device, provider, inDBDMADeviceIndex);
	if ( result ) 
	{
		mKLI2SPowerSymbolName = OSSymbol::withCString ("keyLargo_powerI2S"); // [3324205]
		FailWithAction (NULL == mKLI2SPowerSymbolName, result = false, Exit);
		
		mKeyLargoService = IOService::waitForService ( IOService::serviceMatching ( "KeyLargo" ) );
		debugIOLog ( 3, "  device name is %s",  ( (IORegistryEntry*)device)->getName () );
		FailWithAction ( 0 == mKeyLargoService, result = FALSE, Exit );
		
		
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
	debugIOLog ( 3, "- PlatformInterfaceFCR_Mapped::init ( IOService * device %p, AppleOnboardAudio * provider %p, UInt32 inDBDMADeviceIndex %d ) returns %lX", device, provider, inDBDMADeviceIndex, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceFCR_Mapped::free() 
{
	debugIOLog (3, "+ PlatformInterfaceFCR_Mapped::free()");

 	if ( NULL != mIOBaseAddressMemory ) 
	{
		mIOBaseAddressMemory->release();
	}
	super::free();
	debugIOLog (3, "- PlatformInterfaceFCR_Mapped::free()");
}

#pragma mark ¥
#pragma mark ¥ Power Management Support
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_Mapped::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	return result;
}


#pragma mark ¥
#pragma mark ¥ I2S Methods: FCR1
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceFCR_Mapped::getI2SCellEnable() 
{
	bool			result = false;
	
	switch ( mI2SInterfaceNumber ) 
	{
		case kUseI2SCell0:		result = getI2SCellBitState ( kI2S0CellEnable );					break;
		case kUseI2SCell1:		result = getI2SCellBitState ( kI2S1CellEnable );					break;
		default:																					break;
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceFCR_Mapped::getI2SClockEnable() 
{
	bool			result = false;
	
	switch ( mI2SInterfaceNumber ) 
	{
		case kUseI2SCell0:		result = getI2SCellBitState ( kI2S0ClockEnable );					break;
		case kUseI2SCell1:		result = getI2SCellBitState ( kI2S1ClockEnable );					break;
		default:																					break;
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceFCR_Mapped::getI2SEnable () 
{
	bool			result = false;
	
	switch ( mI2SInterfaceNumber ) 
	{
		case kUseI2SCell0:		result = getI2SCellBitState ( kI2S0InterfaceEnable );				break;
		case kUseI2SCell1:		result = getI2SCellBitState ( kI2S1InterfaceEnable );				break;
		default:																					break;
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceFCR_Mapped::getI2SSWReset () 
{
	bool			result = false;
	
	switch ( mI2SInterfaceNumber ) 
	{
		case kUseI2SCell0:		result = getI2SCellBitState ( kI2S0SwReset );						break;
		case kUseI2SCell1:		result = getI2SCellBitState ( kI2S1SwReset );						break;
		default:																					break;
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_Mapped::setI2SCellEnable ( bool enable ) 
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 5, "+ PlatformInterfaceFCR_Mapped::setI2SCellEnable ( %d )", enable );
	switch ( mI2SInterfaceNumber ) 
	{
		case kUseI2SCell0:		result = setI2SCellBitState ( enable, kI2S0CellEnable );			break;
		case kUseI2SCell1:		result = setI2SCellBitState ( enable, kI2S1CellEnable );			break;
		default:																					break;
	}
	debugIOLog ( 5, "- PlatformInterfaceFCR_Mapped::setI2SCellEnable ( %d ) returns 0x%lX", enable, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_Mapped::setI2SClockEnable ( bool enable ) 
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 5, "+ PlatformInterfaceFCR_Mapped::setI2SClockEnable ( %d )", enable );
	switch ( mI2SInterfaceNumber ) 
	{
		case kUseI2SCell0:		result = setI2SCellBitState ( enable, kI2S0ClockEnable );			break;
		case kUseI2SCell1:		result = setI2SCellBitState ( enable, kI2S1ClockEnable );			break;
		default:																					break;
	}
	debugIOLog ( 5, "- PlatformInterfaceFCR_Mapped::setI2SClockEnable ( %d ) returns 0x%lX", enable, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_Mapped::setI2SEnable ( bool enable ) 
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 5, "+ PlatformInterfaceFCR_Mapped::setI2SEnable ( %d )", enable );
	switch ( mI2SInterfaceNumber ) 
	{
		case kUseI2SCell0:		result = setI2SCellBitState ( enable, kI2S0InterfaceEnable );		break;
		case kUseI2SCell1:		result = setI2SCellBitState ( enable, kI2S1InterfaceEnable );		break;
		default:																					break;
	}
	debugIOLog ( 5, "- PlatformInterfaceFCR_Mapped::setI2SEnable ( %d ) returns 0x%lX", enable, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_Mapped::setI2SSWReset ( bool enable ) 
{
	IOReturn		result = kIOReturnError;
	
	debugIOLog ( 5, "+ PlatformInterfaceFCR_Mapped::setI2SSWReset ( %d )", enable );
	switch ( mI2SInterfaceNumber ) 
	{
		case kUseI2SCell0:		result = setI2SCellBitState ( enable, kI2S0SwReset );				break;
		case kUseI2SCell1:		result = setI2SCellBitState ( enable, kI2S1SwReset );				break;
		default:																					break;
	}
	debugIOLog ( 5, "- PlatformInterfaceFCR_Mapped::setI2SSWReset ( %d ) returns 0x%lX", enable, result );
	return result;
}


#pragma mark ¥
#pragma mark ¥ I2S Methods: FCR3
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_Mapped::releaseI2SClockSource ( I2SClockFrequency inFrequency ) 
{

	debugIOLog ( 5, "+ PlatformInterfaceFCR_Mapped::releaseI2SClockSource ( 0x%lX )", inFrequency );

	FailIf ( 0 == mKeyLargoService, Exit );
	FailIf ( 0 == mKLI2SPowerSymbolName, Exit );
	
	switch ( mI2SInterfaceNumber )
	{
		case kUseI2SCell0:	
			switch ( inFrequency )
			{
				case kI2S_45MHz:	mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)0, (void *)0, (void *)0);	break;
				case kI2S_49MHz:	mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)0, (void *)1, (void *)0);	break;
				case kI2S_18MHz:	mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)0, (void *)2, (void *)0);	break;
			}
			break;
		case kUseI2SCell1:
			switch ( inFrequency )
			{
				case kI2S_45MHz:	mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)1, (void *)0, (void *)0);	break;
				case kI2S_49MHz:	mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)1, (void *)1, (void *)0);	break;
				case kI2S_18MHz:	mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)false, (void *)1, (void *)2, (void *)0);	break;
			}
			break;
		default:
			break;
	}
Exit:
	debugIOLog ( 5, "- PlatformInterfaceFCR_Mapped::releaseI2SClockSource ( 0x%X )", inFrequency );
	return kIOReturnSuccess;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_Mapped::requestI2SClockSource ( I2SClockFrequency inFrequency ) 
{
	IOReturn 				result;
	
	debugIOLog ( 5, "+ PlatformInterfaceFCR_Mapped::requestI2SClockSource ( 0x%lX )", inFrequency );
	
	result = kIOReturnError;
	
	FailIf ( 0 == mKeyLargoService, Exit );
	FailIf ( 0 == mKLI2SPowerSymbolName, Exit );
	
	switch ( mI2SInterfaceNumber )
	{
		case kUseI2SCell0:	
			switch ( inFrequency )
			{
				case kI2S_45MHz:	result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)0, (void *)0, (void *)0);	break;
				case kI2S_49MHz:	result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)0, (void *)1, (void *)0);	break;
				case kI2S_18MHz:	result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)0, (void *)2, (void *)0);	break;
			}
			break;
		case kUseI2SCell1:	
			switch ( inFrequency )
			{
				case kI2S_45MHz:	result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)1, (void *)0, (void *)0);	break;
				case kI2S_49MHz:	result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)1, (void *)1, (void *)0);	break;
				case kI2S_18MHz:	result = mKeyLargoService->callPlatformFunction (mKLI2SPowerSymbolName, false, (void *)true, (void *)1, (void *)2, (void *)0);	break;
			}
			break;
		default:
			break;
	}
	FailIf ( kIOReturnSuccess != result, Exit );
Exit:
	debugIOLog ( 5, "- PlatformInterfaceFCR_Mapped::requestI2SClockSource ( 0x%lX ) returns 0x%lX", inFrequency, result );
	return result;
}

#pragma mark ¥
#pragma mark ¥ Memory Mapped I/O Access Methods
#pragma mark ¥

//	--------------------------------------------------------------------------------
UInt32 PlatformInterfaceFCR_Mapped::getFCR1( void ) 
{
	UInt32 result = OSReadLittleInt32 ( mIOConfigurationBaseAddress, kFCR1Offset );		
	debugIOLog (3,  "± PlatformInterfaceFCR_Mapped::getFCR1 = %lX", result );
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 PlatformInterfaceFCR_Mapped::getFCR3( void ) 
{
	UInt32 result = OSReadLittleInt32 ( mIOConfigurationBaseAddress, kFCR3Offset );
	debugIOLog (3,  "± PlatformInterfaceFCR_Mapped::getFCR3 = %lX", result );
	return result;
}

//	--------------------------------------------------------------------------------
UInt32 PlatformInterfaceFCR_Mapped::getKeyLargoRegister(void *klRegister) 
{
    return OSReadLittleInt32 ( klRegister, 0 );
}

//	--------------------------------------------------------------------------------
void PlatformInterfaceFCR_Mapped::setFCR1(UInt32 value) 
{
	debugIOLog (3,  "± PlatformInterfaceFCR_Mapped::setFCR1( %lX )", value );
	OSWriteLittleInt32 ( mIOConfigurationBaseAddress, kFCR1Offset, value );
}

//	--------------------------------------------------------------------------------
void PlatformInterfaceFCR_Mapped::setFCR3(UInt32 value) 
{
	debugIOLog (3,  "± PlatformInterfaceFCR_Mapped::setFCR3( %lX )", value );
	OSWriteLittleInt32( mIOConfigurationBaseAddress, kFCR3Offset, value );
}

//	--------------------------------------------------------------------------------
void PlatformInterfaceFCR_Mapped::setKeyLargoRegister(void *klRegister, UInt32 value) 
{
	debugIOLog (3,  "± PlatformInterfaceFCR_Mapped::setKeyLargoRegister( %p = 0x%lX )", klRegister, value );
	OSWriteLittleInt32 ( klRegister, 0, value );
}

#pragma mark ¥
#pragma mark ¥ Utility Functions
#pragma mark ¥

//	--------------------------------------------------------------------------------
bool PlatformInterfaceFCR_Mapped::getI2SCellBitState ( UInt32 bitMask ) 
{
	return ( 0 != ( getKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset ) & bitMask ) );
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceFCR_Mapped::setI2SCellBitState ( bool bitState, UInt32 bitMask ) 
{
	UInt32			regValue;
	IOReturn		result = kIOReturnError;

	FailIf ( 0 == mIOConfigurationBaseAddress, Exit );
	regValue = getKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset );						
	if ( bitState ) 
	{
		regValue |= bitMask;
	} 
	else 
	{
		regValue &= ~bitMask;
	}
	setKeyLargoRegister( ((UInt8*)mIOConfigurationBaseAddress) + kFCR1Offset, regValue );
	result = kIOReturnSuccess;
Exit:
	return result;
}



