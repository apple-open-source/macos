/*
 *	PlatformInterfaceI2C_PlatformFunction.cpp
 *
 *	
 *
 *  Created by Ray Montagne on Tuesday August 24 2004.
 *  Copyright (c) 2004 AppleComputer. All rights reserved.
 *
 */
#include	"PlatformInterfaceI2C_PlatformFunction.h"
#include	"TAS_hw.h"

#define super PlatformInterfaceI2C



OSDefineMetaClassAndStructors ( PlatformInterfaceI2C_PlatformFunction, PlatformInterfaceI2C )

const char * PlatformInterfaceI2C_PlatformFunction::kPlatformTas3004CodecRef			= "platform-tas-codec-ref";			//	TAS3004
const char * PlatformInterfaceI2C_PlatformFunction::kPlatformTopazCodecRef				= "platform-topaz-codec-ref";		//	CS8406, CS8416, CS8420
const char * PlatformInterfaceI2C_PlatformFunction::kPlatformOnyxCodecRef				= "platform-onyx-codec-ref";		//	PCM3052

const char * PlatformInterfaceI2C_PlatformFunction::kPlatformDoTasCodecRef				=	"platform-do-tas-codec-ref";	//	TAS3004
const char * PlatformInterfaceI2C_PlatformFunction::kPlatformDoTopazCodecRef			=	"platform-do-topaz-codec-ref";	//	CS8406, CS8416, CS8420
const char * PlatformInterfaceI2C_PlatformFunction::kPlatformDoOnyxCodecRef				=	"platform-do-onyx-codec-ref";	//	PCM3052

#pragma mark ¥
#pragma mark ¥ UNIX Like Functions
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
bool	PlatformInterfaceI2C_PlatformFunction::init (IOService* device, AppleOnboardAudio* provider, UInt32 inDBDMADeviceIndex ) 
{
	IORegistryEntry *		sound;
	bool					result;
	OSData *				myPHandle;
	OSData *				osdata;
	IORegistryEntry *		i2s;
	IORegistryEntry *		macIO;
	char					buffer[256];
	const OSSymbol *		fFuncSym;
	IOService *				service;
	mach_timespec_t			timeout = { 10, 0 };		//	10 seconds
	OSData *				theCodecRef = 0;
	UInt32					soundPHandle = 0;
	
	debugIOLog (3,  "+ PlatformInterfaceI2C_PlatformFunction::init ( %p, %p, %d )", device, provider, inDBDMADeviceIndex );
	result = super::init ( device, provider, inDBDMADeviceIndex );
	FailIf ( !result, Exit );

	for ( UInt32 index = kCodec_TAS3004; index < kCodec_NumberOfTypes; index++ )
	{
		saveMAP ( index, 0xFFFFFFFF );
	}
	
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
	
	myPHandle = OSDynamicCast ( OSData, mI2S->getProperty ( "AAPL,phandle" ) );
	mI2SPHandle = *((UInt32*)myPHandle->getBytesNoCopy());
	debugIOLog ( 3,  "  mI2SPHandle 0x%lX", mI2SPHandle );
	
	osdata = OSDynamicCast ( OSData, mI2S->getProperty ( "reg" ) );
	mI2SOffset = *((UInt32*)osdata->getBytesNoCopy());
		
	i2s = mI2S->getParentEntry (gIODTPlane);
	FailWithAction (!i2s, result = false, Exit);

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
	
	//	For each codec reference that is found in our node, append our provider phandle value to the 
	//	'platform-do-xxxxxxx' string and obtain the IOService * for the platform function.  The
	//	IOService * is saved in an array that is indexed by codec reference where the I2C access
	//	methods will obtain the IOService # from the array.
	
	myPHandle = OSDynamicCast ( OSData, sound->getProperty ( "AAPL,phandle" ) );
	soundPHandle = *((UInt32*)myPHandle->getBytesNoCopy());
	debugIOLog ( 3,  "  soundPHandle 0x%lX", soundPHandle );

	result = FALSE;

	for ( UInt32 codecReference = kCodec_TAS3004; codecReference < kCodec_NumberOfTypes; codecReference++ )
	{
		fFuncSym = 0;
		buffer[0] = 0;
		theCodecRef = 0;
		switch ( codecReference )
		{
			case kCodec_CS8406:				theCodecRef = OSDynamicCast ( OSData, sound->getProperty ( kPlatformTopazCodecRef ) );		break;
			case kCodec_CS8416:				mCodecIOServiceArray[codecReference] = mCodecIOServiceArray[kCodec_CS8406];					break;
			case kCodec_CS8420:				mCodecIOServiceArray[codecReference] = mCodecIOServiceArray[kCodec_CS8406];					break;
			case kCodec_PCM3052:			theCodecRef = OSDynamicCast ( OSData, sound->getProperty ( kPlatformOnyxCodecRef ) );		break;
			case kCodec_TAS3004:			theCodecRef = OSDynamicCast ( OSData, sound->getProperty ( kPlatformTas3004CodecRef ) );	break;
		}
		if ( 0 != theCodecRef )
		{
			switch ( codecReference )
			{
				case kCodec_CS8406:			sprintf ( buffer, "%s-%08lx", kPlatformTopazCodecRef, soundPHandle );						break;
				case kCodec_CS8416:			sprintf ( buffer, "%s-%08lx", kPlatformTopazCodecRef, soundPHandle );						break;
				case kCodec_CS8420:			sprintf ( buffer, "%s-%08lx", kPlatformTopazCodecRef, soundPHandle );						break;
				case kCodec_PCM3052:		sprintf ( buffer, "%s-%08lx", kPlatformOnyxCodecRef, soundPHandle );						break;
				case kCodec_TAS3004:		sprintf ( buffer, "%s-%08lx", kPlatformTas3004CodecRef, soundPHandle );						break;
			}
			debugIOLog ( 5, "  codecReference %d, looking for '%s'", codecReference, buffer );
			fFuncSym = OSSymbol::withCString ( buffer );
			if ( fFuncSym )
			{
				if ( service = IOService::waitForService ( IOService::resourceMatching ( fFuncSym ), &timeout ) )
				{
					if ( service = OSDynamicCast ( IOService, service->getProperty ( fFuncSym ) ) )
					{
						mCodecIOServiceArray[codecReference] = service;
						debugIOLog ( 5, "  codecReference %d, looking for '%s' found %p", codecReference, buffer, mCodecIOServiceArray[codecReference] );
						result = TRUE;
					}
					else
					{
						debugIOLog ( 5, "  codecReference %d, looking for '%s' NOT FOUND", codecReference, buffer );
						FailMessage ( TRUE );
					}
				}
				else
				{
					debugIOLog ( 5, "  codecReference %d, looking for '%s' TIMED OUT", codecReference, buffer );
					FailMessage ( TRUE );
				}
				fFuncSym->release ();
				fFuncSym = 0;
			}
			else
			{
				debugIOLog ( 5, "  could not make function symbol for '%s'", buffer );
			}
		}
	}

Exit:
	debugIOLog (3,  "- PlatformInterfaceI2C_PlatformFunction::init ( %p, %p, %d ) returns %d", device, provider, inDBDMADeviceIndex, result );
	return result;
}

//	----------------------------------------------------------------------------------------------------
void	PlatformInterfaceI2C_PlatformFunction::free() 
{
	super::free();
}

#pragma mark ¥
#pragma mark ¥ Power Managment
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2C_PlatformFunction::performPowerStateChange ( IOService * device, UInt32 currentPowerState, UInt32 pendingPowerState )
{
	IOReturn			result = kIOReturnSuccess;
	
	return result;
}
	

#pragma mark ¥
#pragma mark ¥ I2C Access Methods
#pragma mark ¥

//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2C_PlatformFunction::readCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength ) 
{
	IOI2CCommand			command = {0};
	const OSSymbol*			funcSymbolName;
	IOReturn				result = kIOReturnBadArgument;
	
	FailIf ( kCodec_Unknown == codecRef, Exit );
	FailIf ( kCodec_NumberOfTypes <= codecRef, Exit );
	
	result = kIOReturnNoDevice;
	
	FailIf ( 0 == mCodecIOServiceArray[codecRef], Exit );
	
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
			FailIf ( kIOReturnSuccess != setMAP ( codecRef, subAddress ), Exit );
		}

		//	Now that the MAP has been set, perform the read operation to access data pointed to by the MAP.

		command.subAddress = subAddress;
		command.buffer = data;
		command.count = dataLength;
		command.mode = kI2CMode_Standard;
		
		funcSymbolName = OSSymbol::withCString ( "IOI2CReadI2CBus" );
		FailIf ( 0 == funcSymbolName, Exit );
		result = mCodecIOServiceArray[codecRef]->callPlatformFunction ( funcSymbolName, TRUE, (void*)(UInt32)&command, (void*)0, (void*)0, (void*)0 );
		if ( kIOReturnSuccess != result )
		{
			debugIOLog ( 5, "  %p->callPlatformFunction ( %p->'IOI2CReadI2CBus', TRUE, %lX, %lX, %lX, 0 ) result 0x%X, data 0x%X", mCodecIOServiceArray[codecRef], funcSymbolName, 0, data, dataLength, result, *data );
		}
	}
	else
	{
		funcSymbolName = OSSymbol::withCString ( "IOI2CClientRead" );
		FailIf ( 0 == funcSymbolName, Exit );
		result = mCodecIOServiceArray[codecRef]->callPlatformFunction ( funcSymbolName, TRUE, (void*)(UInt32)subAddress, (void*)data, (void*)dataLength, (void*)0 );
		if ( kIOReturnSuccess != result )
		{
			debugIOLog ( 5, "  %p->callPlatformFunction ( %p->'IOI2CClientRead', TRUE, %lX, %lX, %lX, 0 ) result 0x%X, data 0x%X", ((IOService*)mCodecIOServiceArray[codecRef]), funcSymbolName, subAddress, data, dataLength, result, *data );
		}
	}
	
	funcSymbolName->release();
	FailIf ( kIOReturnSuccess != result, Exit );
Exit:
	return result;
}


//	----------------------------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2C_PlatformFunction::WriteCodecRegister ( UInt32 codecRef, UInt8 subAddress, UInt8 *data, UInt32 dataLength ) 
{
	const OSSymbol*			funcSymbolName;
	IOReturn				result = kIOReturnBadArgument;
	
	FailIf ( kCodec_Unknown == codecRef, Exit );
	FailIf ( kCodec_NumberOfTypes <= codecRef, Exit );
	
	result = kIOReturnNoDevice;
	
	FailIf ( 0 == mCodecIOServiceArray[codecRef], Exit );
	funcSymbolName = OSSymbol::withCString ( "IOI2CClientWrite" );
	FailIf ( 0 == funcSymbolName, Exit );
	result = ((IOService*)mCodecIOServiceArray[codecRef])->callPlatformFunction ( funcSymbolName, TRUE, (void*)(UInt32)subAddress, (void*)data, (void*)dataLength, (void*)0 );
	if ( kIOReturnSuccess != result )
	{
		debugIOLog ( 5, "  %p->callPlatformFunction ( %p->'IOI2CClientWrite', TRUE, %lX, %lX, %lX, 0 ) where data->0x%X", ((IOService*)mCodecIOServiceArray[codecRef]), funcSymbolName, subAddress, data, dataLength, *data );
	}

	funcSymbolName->release();
	FailIf ( kIOReturnSuccess != result, Exit );
	if ( codecHasMAP ( codecRef ) )
	{
		saveMAP ( codecRef, (UInt32)subAddress );
	}
Exit:
	return result;
}


//	--------------------------------------------------------------------------------
IOReturn	PlatformInterfaceI2C_PlatformFunction::setMAP ( UInt32 codecRef, UInt8 subAddress )
{
	IOI2CCommand			command = {0};
	const OSSymbol*			funcSymbolName = 0;
	IOReturn				result = kIOReturnError;
		
	FailIf ( !codecHasMAP ( codecRef ), Exit );
	
	command.subAddress = subAddress;
	command.buffer = &subAddress;
	command.count = 1;
	command.mode = kI2CMode_Standard;
	
	funcSymbolName = OSSymbol::withCString ( "IOI2CWriteI2CBus" );
	FailIf ( 0 == funcSymbolName, Exit );
	
	result = mCodecIOServiceArray[codecRef]->callPlatformFunction ( funcSymbolName, TRUE, (void*)(UInt32)&command, (void*)0, (void*)0, (void*)0 );
	if ( kIOReturnSuccess != result )
	{
		debugIOLog ( 5, "  %p->callPlatformFunction ( %p->'IOI2CWriteI2CBus', TRUE, %lX, 0, 0, 0 ) for MAP where command.buffer->0x%X", mCodecIOServiceArray[codecRef], funcSymbolName, &command, subAddress );
	}
	FailIf ( kIOReturnSuccess != result, Exit );
	
	saveMAP ( codecRef, (UInt32)subAddress );
	result = kIOReturnSuccess;
	
Exit:
	if ( funcSymbolName )
	{
		funcSymbolName->release();
		funcSymbolName = 0;
	}
	return result;
}


#pragma mark ¥
#pragma mark ¥	Utilities
#pragma mark ¥

//	--------------------------------------------------------------------------------
const OSSymbol* PlatformInterfaceI2C_PlatformFunction::makeFunctionSymbolName(const char * name,UInt32 pHandle)
{
	const OSSymbol* 	funcSymbolName = 0;
	char		stringBuf[256];
		
	sprintf ( stringBuf, "%s-%08lx", name, pHandle );
	funcSymbolName = OSSymbol::withCString ( stringBuf );
	
	return funcSymbolName;
}

//	--------------------------------------------------------------------------------
bool	PlatformInterfaceI2C_PlatformFunction::codecHasMAP ( UInt32 codecRef )
{
	bool		result;
	
	switch ( codecRef )
	{
		case kCodec_CS8406:
		case kCodec_CS8416:
		case kCodec_CS8420:			result = TRUE;			break;
		default:					result = FALSE;			break;
	}
	return result;
}

