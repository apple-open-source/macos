/*
 *	PlatformInterfaceFCR_PlatformFunction.cpp
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include "PlatformInterfaceFCR_PlatformFunction.h"

#define super PlatformInterfaceFCR

const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_Enable 					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_Enable;
const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_Disable					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_Disable;
const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_ClockEnable				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_ClockEnable;
const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_ClockDisable				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_ClockDisable;
const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_Reset						= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_Reset;
const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_Run						= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_Run;
const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_CellEnable					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_CellEnable;
const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_CellDisable				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_CellDisable;
const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_GetEnable					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_GetEnable;
const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_GetClockEnable				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_GetClockEnable;
const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_GetReset					= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_GetReset;
const char * 	PlatformInterfaceFCR_PlatformFunction::kAppleI2S_GetCellEnable				= kPlatformInterfaceSupportPlatformFunctionCommon_AppleI2S_GetCellEnable;

OSDefineMetaClassAndStructors ( PlatformInterfaceFCR_PlatformFunction, PlatformInterfaceFCR )

#pragma mark ¥
#pragma mark ¥ UNIX Like Functions
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceFCR_PlatformFunction::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex ) 
{
	IORegistryEntry			*sound;
	bool					result;
	OSData					*osdata;
	IORegistryEntry			*i2s;
	IORegistryEntry			*macIO;
	
	debugIOLog ( 3,  "+ PlatformInterfaceFCR_PlatformFunction::init ( %p, %p, %d )", device, provider, inDBDMADeviceIndex );
	result = super::init (device, provider, inDBDMADeviceIndex);
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
		
	i2s = mI2S->getParentEntry (gIODTPlane);
	FailWithAction (!i2s, result = FALSE, Exit);

	macIO = i2s->getParentEntry ( gIODTPlane );
	FailWithAction ( !macIO, result = false, Exit );
	debugIOLog ( 3, "  path = '...:%s:%s:%s:%s:'", macIO->getName (), i2s->getName (), mI2S->getName (), sound->getName () );
	
	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "AAPL,phandle" ) );
	mMacIOPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debugIOLog ( 3,  "  mMacIOPHandle %lX", mMacIOPHandle );

	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "reg" ) );
	mMacIOOffset = *((UInt32*)osdata->getBytesNoCopy());


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
	
Exit:

	debugIOLog (3,  "- PlatformInterfaceFCR_PlatformFunction::init ( %p, %p, %d ) returns %d", device, provider, inDBDMADeviceIndex, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceFCR_PlatformFunction::free() 
{
	super::free();
}

#pragma mark ¥
#pragma mark ¥ Power Management Support
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_PlatformFunction::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	return result;
}


#pragma mark ¥
#pragma mark ¥ I2S Methods: FCR1
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
//	Return only the write-through cache value here as there is no read access platform function.
bool	PlatformInterfaceFCR_PlatformFunction::getI2SCellEnable () 
{
	return mAppleI2S_CellEnable;
}

//	----------------------------------------------------------------------------------------------------
//	Return only the write-through cache value here as there is no read access platform function.
bool	PlatformInterfaceFCR_PlatformFunction::getI2SClockEnable () 
{
	return mAppleI2S_ClockEnable;
}

//	----------------------------------------------------------------------------------------------------
//	Return only the write-through cache value here as there is no read access platform function.
bool	PlatformInterfaceFCR_PlatformFunction::getI2SEnable () 
{
	return mAppleI2S_Enable;
}

//	----------------------------------------------------------------------------------------------------
//	Return only the write-through cache value here as there is no read access platform function.
bool	PlatformInterfaceFCR_PlatformFunction::getI2SSWReset () 
{
	return mAppleI2S_Reset;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_PlatformFunction::setI2SCellEnable ( bool enable ) 
{
	IOReturn				result;
	
	result = kIOReturnError;
	if ( mSystemIOControllerService )
	{
		if ( enable )
		{
			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleI2S_CellEnable, (void *)0, (void *)0, 0, 0 );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		else
		{
			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleI2S_CellDisable, (void *)0, (void *)0, 0, 0 );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		mAppleI2S_CellEnable = enable;
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_PlatformFunction::setI2SEnable ( bool enable ) 
{
	IOReturn				result;

	result = kIOReturnError;
	if ( mSystemIOControllerService )
	{
		if ( enable )
		{
			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleI2S_Enable, (void *)0, (void *)(UInt32)0, 0, 0 );
			FailIf ( kIOReturnSuccess != result, Exit );
		} else {
			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleI2S_Disable, (void *)0, (void *)(UInt32)0, 0, 0 );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		mAppleI2S_Enable = enable;
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_PlatformFunction::setI2SClockEnable ( bool enable ) 
{
	IOReturn				result;

	result = kIOReturnError;

	if ( mSystemIOControllerService )
	{
		if ( enable )
		{
			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleI2S_ClockEnable, (void *)0, (void *)0, 0, 0 );
			FailIf ( kIOReturnSuccess != result, Exit );
		} else {
			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleI2S_ClockDisable, (void *)0, (void *)0, 0, 0 );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		mAppleI2S_ClockEnable = enable;
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_PlatformFunction::setI2SSWReset ( bool enable ) 
{
	IOReturn				result;
	
	result = kIOReturnError;
	if ( mSystemIOControllerService )
	{
		if ( enable )
		{
			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleI2S_Reset, (void *)0, (void *)0, 0, 0 );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		else
		{
			result = makeSymbolAndCallPlatformFunctionNoWait ( kAppleI2S_Run, (void *)0, (void *)0, 0, 0 );
			FailIf ( kIOReturnSuccess != result, Exit );
		}
		mAppleI2S_Reset = enable;
	}
Exit:
	return result;
}


#pragma mark ¥
#pragma mark ¥ I2S Methods: FCR3
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_PlatformFunction::releaseI2SClockSource ( I2SClockFrequency inFrequency ) 
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

    if ( NULL != mSystemIOControllerService ) 
	{
		switch ( inFrequency ) 
		{
			case kI2S_45MHz:		setupI2SClockSource( i2sCellNumber, FALSE, kK2I2SClockSource_45MHz );		break;
			case kI2S_49MHz:		setupI2SClockSource( i2sCellNumber, FALSE, kK2I2SClockSource_49MHz );		break;
			case kI2S_18MHz:		setupI2SClockSource( i2sCellNumber, FALSE, kK2I2SClockSource_18MHz );		break;
		}
		result = kIOReturnSuccess;
	}

	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_PlatformFunction::requestI2SClockSource ( I2SClockFrequency inFrequency )
{
	UInt32						i2sCellNumber;
	IOReturn					result;

	result = kIOReturnError;

	if ( kUseI2SCell0 == mI2SInterfaceNumber )
	{
		i2sCellNumber = kUseI2SCell0;
	} 
	else if ( kUseI2SCell1 == mI2SInterfaceNumber )
	{
		i2sCellNumber = kUseI2SCell1;
	} 
	else if ( kUseI2SCell2 == mI2SInterfaceNumber )
	{
		i2sCellNumber = kUseI2SCell2;
	} 
	else 
	{
		return kIOReturnBadArgument;
	}
	
    if ( NULL != mSystemIOControllerService ) 
	{
		switch ( inFrequency ) 
		{
			case kI2S_45MHz:		setupI2SClockSource( i2sCellNumber, TRUE, kK2I2SClockSource_45MHz );		break;
			case kI2S_49MHz:		setupI2SClockSource( i2sCellNumber, TRUE, kK2I2SClockSource_49MHz );		break;
			case kI2S_18MHz:		setupI2SClockSource( i2sCellNumber, TRUE, kK2I2SClockSource_18MHz );		break;
		}
		result = kIOReturnSuccess;
	}

	return result;
}

#pragma mark ¥
#pragma mark ¥ Utility Methods
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceFCR_PlatformFunction::makeSymbolAndCallPlatformFunctionNoWait ( const char * name, void * param1, void * param2, void * param3, void * param4 )
{
	const OSSymbol*			funcSymbolName;
	IOReturn				result = kIOReturnError;

	if ( ( 0 != mSystemIOControllerService ) && ( 0 != mI2SPHandle ) && ( 0 != name ) )
	{
		funcSymbolName = makeFunctionSymbolName ( name, mI2SPHandle );
		FailIf ( NULL == funcSymbolName, Exit );
		
		result = mSystemIOControllerService->callPlatformFunction ( funcSymbolName, FALSE, param1, param2, param3, param4 );
		funcSymbolName->release ();	// [3324205]
	}
Exit:
	if ( kIOReturnSuccess != result )
	{
		debugIOLog ( 6, "+ PlatformInterfaceFCR_PlatformFunction::makeSymbolAndCallPlatformFunctionNoWait ( %p, %p, %p, %p, %p )", name, param1, param2, param3, param4 );
		debugIOLog ( 6, "- PlatformInterfaceFCR_PlatformFunction::makeSymbolAndCallPlatformFunctionNoWait ( %p, %p, %p, %p, %p ) returns 0x%X", name, param1, param2, param3, param4, result );
	}
	return result;
}

//	--------------------------------------------------------------------------------
const OSSymbol* PlatformInterfaceFCR_PlatformFunction::makeFunctionSymbolName ( const char * name,UInt32 pHandle )
{
	const OSSymbol*			funcSymbolName = NULL;
	char					stringBuf[256];
		
	sprintf ( stringBuf, "%s-%08lx", name, pHandle );
	funcSymbolName = OSSymbol::withCString ( stringBuf );
	
	return funcSymbolName;
}

//	--------------------------------------------------------------------------------
IOReturn PlatformInterfaceFCR_PlatformFunction::setupI2SClockSource ( UInt32 cell, bool requestClock, UInt32 clockSource )	
{
	IOReturn				result;

	result = kIOReturnError;
	if ( mSystemIOControllerService ) 
	{
		result = mSystemIOControllerService->callPlatformFunction ( "keyLargo_powerI2S", FALSE, (void*)requestClock, (void*)cell, (void*)clockSource, 0 );
	}
	return result;
}

