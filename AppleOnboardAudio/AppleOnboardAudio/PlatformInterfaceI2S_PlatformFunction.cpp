/*
 *	PlatformInterfaceI2S_PlatformFunction.h
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include "PlatformInterfaceI2S_PlatformFunction.h"

#define super PlatformInterfaceI2S

OSDefineMetaClassAndStructors ( PlatformInterfaceI2S_PlatformFunction, PlatformInterfaceI2S )

#pragma mark ¥
#pragma mark ¥ UNIX Like Functions
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceI2S_PlatformFunction::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex )
{
	IORegistryEntry			*sound;
	bool					result;
	OSData					*osdata;
	IORegistryEntry			*i2s;
	IORegistryEntry			*macIO;
	
	debugIOLog ( 3,  "+ PlatformInterfaceI2S_PlatformFunction::init" );
	result = super::init ( device, provider, inDBDMADeviceIndex );
	FailIf ( !result, Exit );

	sound = device;
	FailWithAction ( !sound, result = false, Exit );

	mI2S = sound->getParentEntry (gIODTPlane);
	FailWithAction (!mI2S, result = false, Exit);
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
	
	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "reg" ) );
	mI2SOffset = *((UInt32*)osdata->getBytesNoCopy());
		
	i2s = mI2S->getParentEntry (gIODTPlane);
	FailWithAction ( !i2s, result = false, Exit );

	macIO = i2s->getParentEntry (gIODTPlane);
	FailWithAction ( !macIO, result = false, Exit );
	debugIOLog ( 3, "  path = '...:%s:%s:%s:%s:'", macIO->getName (), i2s->getName (), mI2S->getName (), sound->getName () );
	
	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "AAPL,phandle" ) );
	mMacIOPHandle = *((UInt32*)osdata->getBytesNoCopy());
	debugIOLog ( 3,  "  mMacIOPHandle %lX", mMacIOPHandle );

	osdata = OSDynamicCast ( OSData, macIO->getProperty ( "reg" ) );
	mMacIOOffset = *((UInt32*)osdata->getBytesNoCopy() );


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

	debugIOLog ( 3,  "  about to findAndAttachI2S" );
	result = findAndAttachI2S();
	if ( !result )
	{
		debugIOLog ( 3,  "  COULD NOT FIND I2S" );
	}
	FailIf ( !result, Exit );

Exit:

	debugIOLog (3,  "- PlatformInterfaceI2S_PlatformFunction::init returns %d", result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceI2S_PlatformFunction::free()
{
	super::free();
}

#pragma mark ¥
#pragma mark ¥ POWER MANAGEMENT
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	return result;
}
	

#pragma mark ¥
#pragma mark ¥ I2S IOM ACCESS
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::setSerialFormatRegister ( UInt32 value )
{
	IOReturn				result = kIOReturnError;

	result = kIOReturnError;
	if ( mI2SInterface )
	{
		result = makeSymbolAndCallPlatformFunctionNoWait ( kI2SSetSerialFormatReg, (void *)mI2SOffset, (void *)value, 0, 0 );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_PlatformFunction::getSerialFormatRegister ()
{
	UInt32					result = 0;

	if ( mI2SInterface )
	{
		FailIf ( kIOReturnSuccess != makeSymbolAndCallPlatformFunctionNoWait ( kI2SGetSerialFormatReg, (void *)mI2SOffset, (void *)&result, 0, 0 ), Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::setDataWordSizes ( UInt32 value )
{
	IOReturn				result = kIOReturnError;

	if ( mI2SInterface )
	{
		result = makeSymbolAndCallPlatformFunctionNoWait ( kI2SSetDataWordSizesReg, (void *)mI2SOffset, (void *)value, 0, 0 );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_PlatformFunction::getDataWordSizes()
{
	UInt32					result = 0;

	if ( mI2SInterface )
	{
		FailIf ( kIOReturnSuccess != makeSymbolAndCallPlatformFunctionNoWait ( kI2SGetDataWordSizesReg, (void *)mI2SOffset, (void *)&result, 0, 0 ), Exit );
	}
Exit:
	return result;
}
	
//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::setFrameCount ( UInt32 value )
{
	IOReturn				result;

	//	<<<<<<<<<< WARNING >>>>>>>>>>
	//	Do not debugIOLog in here it screws up the hal timing and causes stuttering audio
	//	<<<<<<<<<< WARNING >>>>>>>>>>
	
	result = kIOReturnError;

	if ( mI2SInterface )
	{
		result = makeSymbolAndCallPlatformFunctionNoWait ( kI2SSetFrameCountReg, (void *)mI2SOffset, (void *)value, 0, 0 );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_PlatformFunction::getFrameCount ()
{
	UInt32					result = 0;

	if ( mI2SInterface )
	{
		FailIf ( kIOReturnSuccess != makeSymbolAndCallPlatformFunctionNoWait ( kI2SGetFrameCountReg, (void *)mI2SOffset, (void *)&result, 0, 0 ), Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::setI2SIOMIntControl ( UInt32 value )
{
	IOReturn				result;

	result = kIOReturnError;

	if ( mI2SInterface )
	{
		result = makeSymbolAndCallPlatformFunctionNoWait ( kI2SSetIntCtlReg, (void *)mI2SOffset, (void *)value, 0, 0 );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_PlatformFunction::getI2SIOMIntControl ()
{
	UInt32					result = 0;

	if ( mI2SInterface )
	{
		FailIf ( kIOReturnSuccess != makeSymbolAndCallPlatformFunctionNoWait ( kI2SGetIntCtlReg, (void *)mI2SOffset, (void *)&result, 0, 0 ), Exit );
	}
Exit:
	return result;
}
	
//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::setPeakLevel ( UInt32 channelTarget, UInt32 value )
{
	IOReturn		result;
	
	switch ( channelTarget )
	{
		case kStreamFrontLeft:		result = setI2SIOM_PeakLevelIn0 (value );				break;
		case kStreamFrontRight:		result = setI2SIOM_PeakLevelIn1 (value );				break;
		default:					result = kIOReturnBadArgument;							break;
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_PlatformFunction::getPeakLevel ( UInt32 channelTarget )
{
	UInt32			result;
	
	switch ( channelTarget )
	{
		case kStreamFrontLeft:		result = getI2SIOM_PeakLevelIn0 ();						break;
		case kStreamFrontRight:		result = getI2SIOM_PeakLevelIn0 ();						break;
		default:					result = 0;												break;
	}
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::setI2SIOM_CodecMsgOut ( UInt32 value )
{
	IOReturn				result = kIOReturnError;

	if ( mI2SInterface )
	{
		result = makeSymbolAndCallPlatformFunctionNoWait ( kI2SGetCodecMsgOutReg, (void *)mI2SOffset, (void *)value, 0, 0 );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_PlatformFunction::getI2SIOM_CodecMsgOut ()
{
	UInt32					result = 0;

	if ( mI2SInterface )
	{
		FailIf ( kIOReturnSuccess != makeSymbolAndCallPlatformFunctionNoWait ( kI2SGetCodecMsgOutReg, (void *)mI2SOffset, (void *)&result, 0, 0 ), Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::setI2SIOM_CodecMsgIn ( UInt32 value )
{
	IOReturn				result = kIOReturnError;

	if ( mI2SInterface )
	{
		result = makeSymbolAndCallPlatformFunctionNoWait ( kI2SSetCodecMsgInReg, (void *)mI2SOffset, (void *)value, 0, 0 );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_PlatformFunction::getI2SIOM_CodecMsgIn ()
{
	UInt32					result = 0;

	if ( mI2SInterface )
	{
		FailIf ( kIOReturnSuccess != makeSymbolAndCallPlatformFunctionNoWait ( kI2SGetCodecMsgInReg, (void *)mI2SOffset, (void *)&result, 0, 0 ), Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::setI2SIOM_FrameMatch ( UInt32 value )
{
	IOReturn				result = kIOReturnError;

	if ( mI2SInterface )
	{
		result = makeSymbolAndCallPlatformFunctionNoWait ( kI2SSetFrameMatchReg, (void *)mI2SOffset, (void *)value, 0, 0 );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_PlatformFunction::getI2SIOM_FrameMatch ()
{
	UInt32					result = 0;

	if ( mI2SInterface )
	{
		FailIf ( kIOReturnSuccess != makeSymbolAndCallPlatformFunctionNoWait ( kI2SGetFrameMatchReg, (void *)mI2SOffset, (void *)&result, 0, 0 ), Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::setI2SIOM_PeakLevelSel ( UInt32 value )
{
	IOReturn				result = kIOReturnError;

	if ( mI2SInterface )
	{
		result = makeSymbolAndCallPlatformFunctionNoWait ( kI2SSetPeakLevelSelReg, (void *)mI2SOffset, (void *)value, 0, 0 );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_PlatformFunction::getI2SIOM_PeakLevelSel ()
{
	UInt32					result = 0;

	if ( mI2SInterface )
	{
		FailIf ( kIOReturnSuccess != makeSymbolAndCallPlatformFunctionNoWait ( kI2SGetPeakLevelSelReg, (void *)mI2SOffset, (void *)&result, 0, 0 ), Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::setI2SIOM_PeakLevelIn0 ( UInt32 value )
{
	IOReturn				result = kIOReturnError;

	if ( mI2SInterface )
	{
		result = makeSymbolAndCallPlatformFunctionNoWait ( kI2SSetPeakLevelIn0Reg, (void *)mI2SOffset, (void *)value, 0, 0 );
		FailIf ( kIOReturnSuccess != result, Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_PlatformFunction::getI2SIOM_PeakLevelIn0 ()
{
	UInt32					result = 0;

	if ( mI2SInterface )
	{
		FailIf ( kIOReturnSuccess != makeSymbolAndCallPlatformFunctionNoWait ( kI2SGetPeakLevelIn0Reg, (void *)mI2SOffset, (void *)&result, 0, 0 ), Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::setI2SIOM_PeakLevelIn1 ( UInt32 value )
{
	IOReturn				result = kIOReturnError;

	if ( mI2SInterface )
	{
		FailIf ( kIOReturnSuccess != makeSymbolAndCallPlatformFunctionNoWait ( kI2SSetPeakLevelIn1Reg, (void *)mI2SOffset, (void *)&result, 0, 0 ), Exit );
	}
Exit:
	return result;
}

//	----------------------------------------------------------------------------------------------------
UInt32	PlatformInterfaceI2S_PlatformFunction::getI2SIOM_PeakLevelIn1 ()
{
	UInt32					result = 0;

	if ( mI2SInterface )
	{
		FailIf ( kIOReturnSuccess != makeSymbolAndCallPlatformFunctionNoWait ( kI2SGetPeakLevelIn1Reg, (void *)mI2SOffset, (void *)&result, 0, 0 ), Exit );
	}
Exit:
	return result;
}


#pragma mark ¥
#pragma mark ¥	Utilities
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2S_PlatformFunction::makeSymbolAndCallPlatformFunctionNoWait ( const char * name, void * param1, void * param2, void * param3, void * param4 )
{
	const OSSymbol*			funcSymbolName;
	IOReturn				result = kIOReturnError;

	if ( ( 0 != mSystemIOControllerService ) && ( 0 != mI2SPHandle ) && ( 0 != name ) )
	{
		funcSymbolName = OSSymbol::withCString ( name );
		FailIf ( NULL == funcSymbolName, Exit );
		
		result = mI2SInterface->callPlatformFunction ( funcSymbolName, false, param1, param2, param3, param4 );
		FailMessage ( kIOReturnSuccess != result );
		funcSymbolName->release ();	// [3324205]
	}
Exit:
	if ( kIOReturnSuccess != result )
	{
		debugIOLog ( 6, "+ PlatformInterfaceI2S_PlatformFunction::mSystemIOControllerService->callPlatformFunction ( %p, FALSE, %p, %p, %p, %p )", funcSymbolName, param1, param2, param3, param4 );
		debugIOLog ( 6, "  mSystemIOControllerService %p, mI2SPHandle %p, mI2SInterface %p, name->'%s'", mSystemIOControllerService, mI2SPHandle, mI2SInterface, name );
		debugIOLog ( 6, "- PlatformInterfaceI2S_PlatformFunction::mSystemIOControllerService->callPlatformFunction ( %p, FALSE, %p, %p, %p, %p ) returns 0x%lX", funcSymbolName, param1, param2, param3, param4, result );
	}
	return result;
}

//	--------------------------------------------------------------------------------
bool PlatformInterfaceI2S_PlatformFunction::findAndAttachI2S()
{

	const OSSymbol *	i2sDriverName;
	IOService *			i2sCandidate;
	OSDictionary *		i2sServiceDictionary;
	bool				result = false;
	mach_timespec_t		timeout	= {5,0};

	i2sCandidate = 0;
	i2sDriverName = OSSymbol::withCStringNoCopy ( "AppleI2S" );
	i2sServiceDictionary = IOService::resourceMatching ( i2sDriverName );
	FailIf ( 0 == i2sServiceDictionary, Exit );

	i2sCandidate = IOService::waitForService ( i2sServiceDictionary, &timeout );
	FailIf ( 0 == i2sCandidate, Exit );
	debugIOLog ( 6,  "  i2sServiceDictionary %p", i2sServiceDictionary );
	
	mI2SInterface = (AppleI2S*)i2sCandidate->getProperty ( i2sDriverName );
	FailIf ( 0 == mI2SInterface, Exit );
	
	mI2SInterface->retain ();
	result = true;
	
Exit:
	if ( 0 != i2sDriverName )
	{
		i2sDriverName->release ();
	}
	return result;
}

//	--------------------------------------------------------------------------------
bool PlatformInterfaceI2S_PlatformFunction::openI2S()
{
	return true;			//	No open in K2 I2S driver
}

//	--------------------------------------------------------------------------------
void PlatformInterfaceI2S_PlatformFunction::closeI2S ()
{
	return;					//	No close in K2 I2S driver
}

//	--------------------------------------------------------------------------------
bool PlatformInterfaceI2S_PlatformFunction::detachFromI2S()
{
	if ( 0 != mI2SInterface ) 
	{
		mI2SInterface->release ();
		mI2SInterface = 0;
	}
	return ( true );
}

	

	

