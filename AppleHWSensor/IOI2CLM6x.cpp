/*
 * Copyright (c) 2004-2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: IOI2CLM6x.cpp,v 1.2 2005/01/12 00:51:36 dirty Exp $
 *
 *  DRI: Cameron Esfahani
 *
 *		$Log: IOI2CLM6x.cpp,v $
 *		Revision 1.2  2005/01/12 00:51:36  dirty
 *		Fix garbage character.
 *		
 *		Revision 1.1  2005/01/12 00:49:16  dirty
 *		First checked in.
 *		
 *		
 */


#include <IOKit/IODeviceTreeSupport.h>
#include "IOI2CLM6x.h"


#define super IOI2CDevice
OSDefineMetaClassAndStructors( IOI2CLM6x, IOI2CDevice )


#if LM6x_DEBUG
static void
IOI2CLM6xDebugAssert	(
							const char * componentNameString,
							const char * assertionString, 
							const char * exceptionLabelString,
							const char * errorString,
							const char * fileName,
							long lineNumber,
							int errorCode
						)
{
	IOLog ( "%s Assert failed: %s ", componentNameString, assertionString );

	if ( exceptionLabelString != NULL )
		IOLog ( "%s ", exceptionLabelString );

	if ( errorString != NULL )
		IOLog ( "%s ", errorString );

	if ( fileName != NULL )
		IOLog ( "file: %s ", fileName );

	if ( lineNumber != 0 )
		IOLog ( "line: %ld ", lineNumber );

	if ( ( long ) errorCode != 0 )
		IOLog ( "error: %ld ", ( long ) errorCode );

	IOLog ( "\n" );
}
#endif	// LM6x_DEBUG


bool
IOI2CLM6x::start	(
						IOService*						provider
					)
{
	IOReturn								status;

	fRegistersAreSaved = false;

	sGetSensorValueSym = OSSymbol::withCString( "getSensorValue" );

	// Start I2CDriver first...
	if ( !( super::start( provider ) ) )
		return( false );

	nrequire( fInitHWFailed, IOI2CLM6x_start_fInitHWFailedErr );

	// Create child nubs.
	require_success( ( status = createChildNubs( fProvider ) ), IOI2CLM6x_start_createChildNubsErr );

	// Register so others can find us with a waitForService().
	registerService();

	return( true );


IOI2CLM6x_start_createChildNubsErr:
IOI2CLM6x_start_fInitHWFailedErr:
	freeI2CResources();
	return( false );
}


void
IOI2CLM6x::free	(
					void
				)
{
	if ( sGetSensorValueSym )
	{
		sGetSensorValueSym->release();
		sGetSensorValueSym = NULL;
	}

	super::free();
}


IOReturn
IOI2CLM6x::initHW	(
						void
					)
{
	IOReturn							status = kIOReturnSuccess;

	return( status );
}


IOReturn
IOI2CLM6x::getLocalTemperature	(
									SInt32*				temperature
								)
{
    IOReturn						status = kIOReturnSuccess;
    UInt8							rawData;

	require_success( ( status = readI2C( kLM6xReg_LocalTemperature, &rawData, 1 ) ), IOI2CLM6x_getLocalTemperature_readI2CErr );

	// Local temperature data is represented by a 8-bit, two's complement word with 
	// an LSB equal to 1.0C.  We need to shift it into a 16.16 format.

	*temperature = ( SInt32 ) ( rawData << 16 );

IOI2CLM6x_getLocalTemperature_readI2CErr:
	return( status );
}


IOReturn
IOI2CLM6x::getRemoteTemperature	(
									SInt32*				temperature
								)
{
    UInt8							rawDataMsb;
    UInt8							rawDataLsb;
    IOReturn						status = kIOReturnSuccess;

	require_success( ( status = readI2C( kLM6xReg_RemoteTemperatureMSB, &rawDataMsb, 1 ) ), IOI2CLM6x_getRemoteTemperature_readI2CErr1 );
	require_success( ( status = readI2C( kLM6xReg_RemoteTemperatureLSB, &rawDataLsb, 1 ) ), IOI2CLM6x_getRemoteTemperature_readI2CErr2 );

	// Remote temperature data is represented by a 11-bit, two's complement word with 
	// an LSB equal to 0.125C.  The data format is a left justified 16-bit word available in
	// two 8-bit registers.

	*temperature = ( SInt32 ) ( ( rawDataMsb << 16 ) | ( rawDataLsb << 8 ) );

IOI2CLM6x_getRemoteTemperature_readI2CErr2:
IOI2CLM6x_getRemoteTemperature_readI2CErr1:
	return( status );
}


#pragma mark -
#pragma mark *** Platform Functions ***
#pragma mark -


IOReturn
IOI2CLM6x::callPlatformFunction	(
									const OSSymbol*				functionName,
									bool						waitForFunction,
									void*						param1, 
									void*						param2,
									void*						param3, 
									void*						param4
								)
{
	UInt32					id = ( UInt32 ) param1;
	SInt32*					value = ( SInt32 * ) param2;

	if ( functionName->isEqualTo( sGetSensorValueSym ) == TRUE )
	{
		if ( isI2COffline() )
			return( kIOReturnOffline );

		if ( id == kLM6xReg_LocalTemperature )
		{
			return( getLocalTemperature( value ) );
		}

		if ( id == kLM6xReg_RemoteTemperatureMSB )
		{
			return( getRemoteTemperature( value ) );
		}
	}

    return( super::callPlatformFunction(	functionName,
											waitForFunction,
											param1,
											param2,
											param3,
											param4 ) );
}


IOReturn
IOI2CLM6x::createChildNubs	(
								IOService*			nub
							)
{
	OSIterator*							childIterator;
	IORegistryEntry*					childEntry;
	IOService*							childNub;

	require( ( childIterator = nub->getChildIterator( gIODTPlane ) ) != NULL, IOI2CLM6x_createChildNubs_getChildIteratorNull );

	while ( ( childEntry = ( IORegistryEntry * ) childIterator->getNextObject() ) != NULL )
	{
		childNub = OSDynamicCast( IOService, OSMetaClass::allocClassWithName( "IOService" ) );

		if ( childNub )
		{
			childNub->init( childEntry, gIODTPlane );
			childNub->attach( this );
			childNub->registerService();
		}
	}

IOI2CLM6x_createChildNubs_getChildIteratorNull:
	return( kIOReturnSuccess );
}


#pragma mark -
#pragma mark *** Power Management ***
#pragma mark -


void
IOI2CLM6x::processPowerEvent	(
									UInt32			eventType
								)
{
	switch ( eventType )
	{
		case	kI2CPowerEvent_OFF:
		case	kI2CPowerEvent_SLEEP:
		{
			require_success( saveRegisters(), IOI2CLM6x_processPowerEvent_saveRegistersErr );
			fRegistersAreSaved = true;
			break;
		}

		case	kI2CPowerEvent_ON:
		case	kI2CPowerEvent_WAKE:
		{
			if ( fRegistersAreSaved )
			{
				// Full Power State
				require_success( restoreRegisters(), IOI2CLM6x_processPowerEvent_restoreRegistersErr );
				fRegistersAreSaved = false;
			}
			break;
		}

		case	kI2CPowerEvent_STARTUP:
		{
			require_success( initHW(), IOI2CLM6x_processPowerEvent_initHWErr );
			break;
		}
    }

IOI2CLM6x_processPowerEvent_initHWErr:
IOI2CLM6x_processPowerEvent_saveRegistersErr:
	return;

IOI2CLM6x_processPowerEvent_restoreRegistersErr:
	fRegistersAreSaved = false;
	return;
}


IOReturn
IOI2CLM6x::saveRegisters	(
								void
							)
{
	IOReturn						status = kIOReturnSuccess;

	return( status );
}


IOReturn
IOI2CLM6x::restoreRegisters	(
								void
							)
{
	IOReturn						status = kIOReturnSuccess;

	return( status );
}
