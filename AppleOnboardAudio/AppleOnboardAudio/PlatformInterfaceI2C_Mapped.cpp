/*
 *	PlatformInterfaceI2C_Mapped.cpp
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include	"PlatformInterfaceI2C_Mapped.h"
#include	"TAS_hw.h"

#define super PlatformInterfaceI2C

OSDefineMetaClassAndStructors ( PlatformInterfaceI2C_Mapped, PlatformInterfaceI2C )

const UInt16 PlatformInterfaceI2C_Mapped::kAPPLE_IO_CONFIGURATION_SIZE	= kPlatformInterfaceSupportMappedCommon_APPLE_IO_CONFIGURATION_SIZE;
const UInt16 PlatformInterfaceI2C_Mapped::kI2S_IO_CONFIGURATION_SIZE	= kPlatformInterfaceSupportMappedCommon_I2S_IO_CONFIGURATION_SIZE;

const UInt32 PlatformInterfaceI2C_Mapped::kI2S0BaseOffset				= kPlatformInterfaceSupportMappedCommon_I2S0BaseOffset;
const UInt32 PlatformInterfaceI2C_Mapped::kI2S1BaseOffset				= kPlatformInterfaceSupportMappedCommon_I2S1BaseOffset;

#pragma mark ¥
#pragma mark ¥ UNIX Like Functions
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceI2C_Mapped::init ( IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex ) 
{
	bool					result = FALSE;
	IORegistryEntry			*i2s;
	OSData					*tmpData;

	debugIOLog ( 3, "+ PlatformInterfaceI2C_Mapped::init ( IOService * device %p, AppleOnboardAudio * provider %p, UInt32 inDBDMADeviceIndex %d )", device, provider, inDBDMADeviceIndex );
	
	FailIf ( NULL == provider, Exit );
	FailIf ( NULL == device, Exit );
	
	mProvider = provider;

	result = super::init ( device, provider, inDBDMADeviceIndex );
	if ( result )
	{
		for ( UInt32 index = kCodec_TAS3004; index < kCodec_NumberOfTypes; index++ )
		{
			saveMAP ( index, 0xFFFFFFFF );
		}
		
		mKeyLargoService = IOService::waitForService ( IOService::serviceMatching ( "KeyLargo" ) );
		debugIOLog ( 3, "  device name is '%s'", ( (IORegistryEntry*)device)->getName () );

		i2s =  ( ( IORegistryEntry*)device)->getParentEntry ( gIODTPlane );
		FailWithAction ( 0 == i2s, result = false, Exit );
		debugIOLog ( 3, "  parent name of '%s' is '%s'", ( (IORegistryEntry*)device)->getName (), i2s->getName () );

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
		
		result = findAndAttachI2C();
		FailIf ( !result, Exit );
	
		tmpData = OSDynamicCast ( OSData, device->getProperty ( "AAPL,i2c-port-select" ) );		
		if ( 0 != tmpData )
		{
			mI2CPort = *( (UInt32*)tmpData->getBytesNoCopy() );
		}
		debugIOLog (3, "  mI2CPort = %ld", mI2CPort);

	}
Exit:
	debugIOLog ( 3, "- PlatformInterfaceI2C_Mapped::init ( IOService * device %p, AppleOnboardAudio * provider %p, UInt32 inDBDMADeviceIndex %d ) returns %lX", device, provider, inDBDMADeviceIndex, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceI2C_Mapped::free()
{
	debugIOLog (3, "+ PlatformInterfaceI2C_Mapped::free()");

	detachFromI2C ();
 	if ( NULL != mIOBaseAddressMemory )
	{
		mIOBaseAddressMemory->release();
	}
	super::free();
	debugIOLog (3, "- PlatformInterfaceI2C_Mapped::free()");
}

#pragma mark ¥
#pragma mark ¥ Power Managment
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2C_Mapped::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	return result;
}
	

#pragma mark ¥
#pragma mark ¥ I2C Access Methods
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2C_Mapped::readCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength )
{
	UInt8		i2cDeviceAddress;
	UInt8		i2cBusMode = kI2C_CombinedMode;
	IOReturn	result = kIOReturnError;
	
	debugIOLog ( 6, "+ PlatformInterfaceI2C_Mapped::readCodecRegister ( %d, %X, %p, %d )", codecRef, subAddress, data, dataLength );
	
	FailIf ( 0 == mI2CInterface, Exit );
	FailIf ( 0 == data, Exit );
	
	//	If a legal I2C device address was found then perform the transaction
	result = setupForI2CTransaction ( codecRef, &i2cDeviceAddress, &i2cBusMode, TRUE );
	
	FailIf ( kIOReturnSuccess != result, Exit );

	//	Codec devices that use a Memory Address Pointer (i.e. MAP) will require performing a series of 
	//	transactions using standard 7 bit addressing with sub-address for write operations to set the 
	//	MAP and standard 7 bit addressing for a read operation to access the data pointed to by the MAP 
	//	in lieu of assumed support for combined addressing.  If the codec device does not have a MAP then
	//	combined format is assumed (available as the default read transaction protocol using the IOI2CClientRead
	//	function.
	
	if ( codecHasMAP ( codecRef ) )
	{
		if ( getSavedMAP ( codecRef ) != subAddress )
		{
			setMAP ( codecRef, subAddress );
		}

		FailIf ( !openI2C (), Exit );
		
		switch ( i2cBusMode )
		{
			case kI2C_StandardMode:			debugIOLog ( 6, "  mI2CInterface->setStandardMode ()" );		mI2CInterface->setStandardMode ();		break;
			case kI2C_StandardSubMode:		debugIOLog ( 6, "  mI2CInterface->setStandardSubMode ()" );		mI2CInterface->setStandardSubMode ();	break;
			case kI2C_CombinedMode:			debugIOLog ( 6, "  mI2CInterface->setCombinedMode ()" );		mI2CInterface->setCombinedMode ();		break;
			default:						FailIf ( TRUE, CloseAndExit );																			break;
		}		
		mI2CInterface->setPollingMode ( FALSE );

#if 1
		debugIOLog ( 6, "  mI2CInterface->readI2CBus ( 0x%X, 0x%X, %p, %d ) where i2cDeviceAddress = 0x%0.2X", i2cDeviceAddress >> 1, subAddress, data, dataLength, i2cDeviceAddress );
#endif
		FailWithAction ( !mI2CInterface->readI2CBus ( i2cDeviceAddress >> 1, subAddress, data, dataLength ), result = kIOReturnNoDevice, CloseAndExit );
		
		result = kIOReturnSuccess;

		
	}
	else
	{
		FailIf ( !openI2C (), Exit );
		
		switch ( i2cBusMode )
		{
			case kI2C_StandardMode:			debugIOLog ( 6, "  mI2CInterface->setStandardMode ()" );		mI2CInterface->setStandardMode ();		break;
			case kI2C_StandardSubMode:		debugIOLog ( 6, "  mI2CInterface->setStandardSubMode ()" );		mI2CInterface->setStandardSubMode ();	break;
			case kI2C_CombinedMode:			debugIOLog ( 6, "  mI2CInterface->setCombinedMode ()" );		mI2CInterface->setCombinedMode ();		break;
			default:						FailIf ( TRUE, CloseAndExit );																			break;
		}		
		mI2CInterface->setPollingMode ( FALSE );

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
#if 1
		debugIOLog ( 6, "  mI2CInterface->readI2CBus ( 0x%X, 0x%X, %p, %d ) where i2cDeviceAddress = 0x%0.2X", i2cDeviceAddress >> 1, subAddress, data, dataLength, i2cDeviceAddress );
#endif
		FailWithAction ( !mI2CInterface->readI2CBus ( i2cDeviceAddress >> 1, subAddress, data, dataLength ), result = kIOReturnNoDevice, CloseAndExit );
		
		result = kIOReturnSuccess;
	}

CloseAndExit:
	closeI2C ();
Exit:
	debugIOLog ( 6, "- PlatformInterfaceI2C_Mapped::readCodecRegister ( %d, %X, %p, %d ) returns %lX", codecRef, subAddress, data, dataLength, result );
	return result;
}


//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2C_Mapped::WriteCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength )
{
	UInt8		i2cDeviceAddress = 0;
	UInt8		i2cBusMode;
	IOReturn	result = kIOReturnError;
	
	FailIf ( 0 == mI2CInterface, Exit );
	FailIf ( 0 == data, Exit );
	
	//	If a legal I2C device address was found then perform the transaction
	result = setupForI2CTransaction ( codecRef, &i2cDeviceAddress, &i2cBusMode, FALSE );
	
	FailIf ( kIOReturnSuccess != result, Exit );

	FailIf ( !openI2C (), Exit );
	switch ( i2cBusMode )
	{
		case kI2C_StandardMode:			mI2CInterface->setStandardMode ();		break;	//	0
		case kI2C_StandardSubMode:		mI2CInterface->setStandardSubMode ();	break;	//	1
		case kI2C_CombinedMode:			mI2CInterface->setCombinedMode ();		break;	//	2
		default:						FailIf ( TRUE, CloseAndExit );			break;
	}		
	
	mI2CInterface->setPollingMode ( FALSE );

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
#if 1
	debugIOLog ( 6, "  mI2CInterface->writeI2CBus ( 0x%X, %d, %p, %d ) where i2cDeviceAddress = 0x%0.2X", i2cDeviceAddress >> 1, subAddress, data, dataLength, i2cDeviceAddress );
#endif
	FailWithAction ( !mI2CInterface->writeI2CBus ( i2cDeviceAddress >> 1, subAddress, data, dataLength ), result = kIOReturnNoDevice, CloseAndExit );
	saveMAP ( codecRef, (UInt32)subAddress );
	result = kIOReturnSuccess;

CloseAndExit:
	closeI2C ();
Exit:
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2C_Mapped::setMAP ( UInt32 codecRef, UInt8 subAddress )
{
	UInt8				i2cDeviceAddress;
	UInt8				i2cBusMode = kI2C_CombinedMode;
	IOReturn			result = kIOReturnError;
	bool				appleI2CResult = FALSE;
	
	debugIOLog ( 6, "+ PlatformInterfaceI2C_Mapped::setMAP ( %d, %d )", codecRef, subAddress );
	
	FailIf ( !codecHasMAP ( codecRef ), Exit );
	
	result = setupForI2CTransaction ( codecRef, &i2cDeviceAddress, &i2cBusMode, FALSE );
	FailIf ( kIOReturnSuccess != result, Exit );
	
	FailIf ( !openI2C (), Exit );
	
	mI2CInterface->setStandardMode ();
	mI2CInterface->setPollingMode ( FALSE );
	
	debugIOLog ( 6, "  mI2CInterface->writeI2CBus ( 0x%X, 0, %p, 1 ) where i2cDeviceAddress = 0x%0.2X to set MAP", i2cDeviceAddress >> 1, &subAddress, i2cDeviceAddress );
	appleI2CResult = mI2CInterface->writeI2CBus ( i2cDeviceAddress >> 1, 0, &subAddress, 1 );
	if ( !appleI2CResult )
	{
		debugIOLog ( 6, "  FAIL: mI2CInterface->writeI2CBus ( 0x%X, 0, %p, 1 ) where i2cDeviceAddress = 0x%0.2X to set MAP", i2cDeviceAddress >> 1, &subAddress, i2cDeviceAddress );
	}

	FailWithAction ( !appleI2CResult, result = kIOReturnNoDevice, CloseAndExit );
	saveMAP ( codecRef, (UInt32)subAddress );
	result = kIOReturnSuccess;
	
CloseAndExit:
	closeI2C ();
Exit:
	debugIOLog ( 6, "- PlatformInterfaceI2C_Mapped::setMAP ( %d, %d ) returns 0x%lX", codecRef, subAddress, result );
	return result;
}


#pragma mark ¥
#pragma mark ¥ Utility Methods
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2C_Mapped::setupForI2CTransaction ( UInt32 codecRef, UInt8 * i2cDeviceAddress, UInt8 * i2cBusProtocol, bool direction )
{
	IOReturn			result = kIOReturnSuccess;
	
	debugIOLog ( 6, "+ PlatformInterfaceI2C_Mapped::setupForI2CTransaction ( %x, %p, %p, %d )", codecRef, i2cDeviceAddress, i2cBusProtocol, direction );
	FailWithAction ( 0 == i2cDeviceAddress, result = kIOReturnBadArgument, Exit );
	FailWithAction ( 0 == i2cBusProtocol, result = kIOReturnBadArgument, Exit );
	
	switch ( codecRef )
	{
		case kCodec_CS8406:
			*i2cBusProtocol = direction ? kI2C_StandardMode : kI2C_StandardSubMode ;
			switch ( mI2SInterfaceNumber )
			{
				case kUseI2SCell0:	*i2cDeviceAddress = kCS8406_i2cAddress_i2sA;	break;
				case kUseI2SCell1:	*i2cDeviceAddress = kCS8406_i2cAddress_i2sB;	break;
				case kUseI2SCell2:	*i2cDeviceAddress = kCS8406_i2cAddress_i2sC;	break;
				default:			result = kIOReturnBadArgument;					break;
			}
			break;
		case kCodec_CS8416:
			*i2cBusProtocol = direction ? kI2C_StandardMode : kI2C_StandardSubMode ;
			switch ( mI2SInterfaceNumber )
			{
				case kUseI2SCell0:	*i2cDeviceAddress = kCS8416_i2cAddress_i2sA;	break;
				case kUseI2SCell1:	*i2cDeviceAddress = kCS8416_i2cAddress_i2sB;	break;
				case kUseI2SCell2:	*i2cDeviceAddress = kCS8416_i2cAddress_i2sC;	break;
				default:			result = kIOReturnBadArgument;					break;
			}
			break;
		case kCodec_CS8420:
			*i2cBusProtocol = direction ? kI2C_StandardMode : kI2C_StandardSubMode ;
			switch ( mI2SInterfaceNumber )
			{
				case kUseI2SCell0:	*i2cDeviceAddress = kCS8420_i2cAddress_i2sA;	break;
				case kUseI2SCell1:	*i2cDeviceAddress = kCS8420_i2cAddress_i2sB;	break;
				case kUseI2SCell2:	*i2cDeviceAddress = kCS8420_i2cAddress_i2sC;	break;
				default:			result = kIOReturnBadArgument;					break;
			}
			break;
		case kCodec_PCM3052:
			*i2cBusProtocol = direction ? kI2C_CombinedMode : kI2C_StandardSubMode ;
			switch ( mI2SInterfaceNumber )
			{
				case kUseI2SCell0:	*i2cDeviceAddress = kPCM3052_i2cAddress_i2sA;	break;
				case kUseI2SCell1:	*i2cDeviceAddress = kPCM3052_i2cAddress_i2sB;	break;
				case kUseI2SCell2:	*i2cDeviceAddress = kPCM3052_i2cAddress_i2sC;	break;
				default:			result = kIOReturnBadArgument;					break;
			}
			break;
		case kCodec_TAS3004:
			*i2cBusProtocol = direction ? kI2C_CombinedMode : kI2C_StandardSubMode ;
			if ( kUseI2SCell0 == mI2SInterfaceNumber )
			{
				*i2cDeviceAddress = kTAS3004_i2cAddress_i2sA;
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
	debugIOLog ( 6, "  *i2cDeviceAddress %X, *i2cBusProtocol %X", *i2cDeviceAddress, *i2cBusProtocol );
Exit:
	debugIOLog ( 6, "- PlatformInterfaceI2C_Mapped::setupForI2CTransaction ( %x, %p, %p, %d ) returns %lX", codecRef, i2cDeviceAddress, i2cBusProtocol, direction, result );
	return result;
}


//	--------------------------------------------------------------------------------
bool PlatformInterfaceI2C_Mapped::findAndAttachI2C ()
{
	const OSSymbol	*i2cDriverName;
	IOService		*i2cCandidate;
	OSDictionary	*i2cServiceDictionary;
	bool			result = FALSE;
	
	debugIOLog ( 6, "+ PlatformInterfaceI2C_Mapped::findAndAttachI2C ()" );
	i2cDriverName = OSSymbol::withCStringNoCopy ( "PPCI2CInterface.i2c-mac-io" );
	i2cServiceDictionary = IOService::resourceMatching ( i2cDriverName );
	i2cCandidate = IOService::waitForService ( i2cServiceDictionary );
	
	mI2CInterface = (PPCI2CInterface*)i2cCandidate->getProperty ( i2cDriverName );
	FailIf ( 0 == mI2CInterface, Exit );
	
	result = TRUE;
Exit:
	debugIOLog ( 6, "- PlatformInterfaceI2C_Mapped::findAndAttachI2C () returns %d", result );
	return result;
}

//	--------------------------------------------------------------------------------
bool PlatformInterfaceI2C_Mapped::openI2C()
{
	bool		result;
	
	result = FALSE;
	FailIf ( NULL == mI2CInterface, Exit );

	// Open the interface and sets it in the wanted mode:
	FailIf ( !mI2CInterface->openI2CBus ( mI2CPort ), Exit );

	result = TRUE;

Exit:
	return result;
}

//	--------------------------------------------------------------------------------
void PlatformInterfaceI2C_Mapped::closeI2C ()
{
	// Closes the bus so other can access to it:
	mI2CInterface->closeI2CBus ();
}

//	--------------------------------------------------------------------------------
bool PlatformInterfaceI2C_Mapped::detachFromI2C()
{
	if ( mI2CInterface && 0 )
	{
		mI2CInterface->release ();
		mI2CInterface = NULL;
	}
	return TRUE;
}

//	--------------------------------------------------------------------------------
bool	PlatformInterfaceI2C_Mapped::codecHasMAP ( UInt32 codecRef )
{
	bool		result;
	
	switch ( codecRef )
	{
		case kCodec_CS8406:			//	Fall through to kCodec_CS8420
		case kCodec_CS8416:			//	Fall through to kCodec_CS8420
		case kCodec_CS8420:			result = TRUE;			break;
		default:					result = FALSE;			break;
	}
	return result;
}

