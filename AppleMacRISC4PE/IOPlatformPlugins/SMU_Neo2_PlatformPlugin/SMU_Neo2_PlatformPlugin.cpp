/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: SMU_Neo2_PlatformPlugin.cpp,v 1.12 2005/09/13 02:29:34 larson Exp $
 */


#include <IOKit/pwr_mgt/IOPM.h>
#include "IOPlatformPluginSymbols.h"

#include "SMU_Neo2_PlatformPlugin.h"


OSDefineMetaClassAndStructors( SMU_Neo2_PlatformPlugin, IOPlatformPlugin )


SMU_Neo2_PlatformPlugin*		gPlatformPlugin;


bool SMU_Neo2_PlatformPlugin::start( IOService* provider )
    {
	gPlatformPlugin = this;

	if ( !IOPlatformPlugin::start( provider ) )
		{
		return( false );
		}

	// Set flags to tell the system we support dynamic power step.

	if ( pmRootDomain != 0 )
		{
		pmRootDomain->publishFeature( "Reduce Processor Speed" );
		pmRootDomain->publishFeature( "Dynamic Power Step" );
		}

	// Set the platform ID.

	if ( gIOPPluginPlatformID )
		gIOPPluginPlatformID->release();

	gIOPPluginPlatformID = OSSymbol::withCString( "SMU_Neo2" );


	return( true );
	}


UInt8 SMU_Neo2_PlatformPlugin::probeConfig( void )
	{
	char											thermalProfilePrefix[ 128 ] = "ThermalProfile_";
	OSDictionary*									thermalNubDict;
	OSString*										modelString;
	OSString*										name;
	UInt8											config = 0;

	if ( ( thermalNubDict = OSDictionary::withCapacity( 1 ) ) == NULL )
		return( 0 );

	if ( ( modelString = OSDynamicCast( OSString, getProvider()->getProperty( "model" ) ) ) == NULL )
		return( 0 );

	strcat( thermalProfilePrefix, modelString->getCStringNoCopy() );
	name = OSString::withCString( thermalProfilePrefix );

	// By using OSString or OSSymbol and setting the name as IOName, we avoid
	// overriding compareName.

	thermalNubDict->setObject( "IOName", name );

	if ( ( ( thermalNub = new IOService ) == NULL ) || ( !thermalNub->init( thermalNubDict ) ) )
		return( 0 );

	thermalNub->attach( this );
	thermalNub->start( this );
	thermalNub->registerService( kIOServiceSynchronous );

	// Get the dictionary from the nub.  What do we do if we couldn't find the thermal profile?

	if ( ( thermalProfile = OSDynamicCast( IOPlatformPluginThermalProfile, thermalNub->getClient() ) ) != NULL )
		{
		config = thermalProfile->getThermalConfig();
		removeProperty( kIOPPluginThermalProfileKey );
		setProperty( kIOPPluginThermalProfileKey, thermalProfile->copyProperty( kIOPPluginThermalProfileKey ) );
		}


	thermalNubDict->release();
	name->release();

	return( config );
	}

bool SMU_Neo2_PlatformPlugin::initThermalProfile(IOService *nub)
{
	IOLog("SMU_Neo2_PlatformPlugin::initThermalProfile - entry\n");
	if(!(IOPlatformPlugin::initThermalProfile(nub)))
		return false;

	// Give the ThermalProfile a chance to override the environment.

	if ( thermalProfile != NULL )
		{
		IOLog("SMU_Neo2_PlatformPlugin::initThermalProfile - calling adjust\n");
		thermalProfile->adjustThermalProfile();

		// Tear down the nub (which also terminates thermalProfile)
		thermalNub->terminate();
		thermalNub = NULL;
		thermalProfile = NULL;
		}
	return true;
}
bool SMU_Neo2_PlatformPlugin::getSDBPartitionData( UInt8 partitionID, UInt16 offset, UInt16 length, UInt8* outBuffer )
	{
	char								partitionName[ 20 ];
	bool								success = false;

	sprintf( partitionName, "sdb-partition-%02X", partitionID );

	OSData*								partitionData;

	if ( ( partitionData = OSDynamicCast( OSData, getAppleSMU()->getProperty( partitionName ) ) ) != NULL )
		{
		const void*						partitionBytes;

		if ( ( partitionBytes = partitionData->getBytesNoCopy( offset, length ) ) != NULL )
			{
			bcopy( partitionBytes, outBuffer, length );
			success = true;
			}
		}

	return( success );
	}


IOService* SMU_Neo2_PlatformPlugin::getAppleSMU( void )
	{
	if ( !appleSMU )
		{
		IOService*						service;

		// Find "AppleSMU" for other clients...

		if ( ( service = waitForService( resourceMatching( "IOPMU" ) ) ) != NULL )
			{
			appleSMU = OSDynamicCast( IOService, service->getProperty( "IOPMU" ) );
			}
		}

	return( appleSMU );
	}

/*******************************************************************************
 * Method:
 *	registerChassisSwitchNotifier
 *
 * Purpose:
 *	Register to receive AppleSMU environmental interrupt notifications.
 ******************************************************************************/
// virtual
void SMU_Neo2_PlatformPlugin::registerChassisSwitchNotifier( void )
{
	IOReturn status;

	if (!appleSMU && !getAppleSMU()) return;

	status = appleSMU->callPlatformFunction( kSymRegisterForInts, false,
			(void *) kPMUenvironmentInt /* mask */,
			(void *) this /* caller */,
			(void *) &SMU_Neo2_PlatformPlugin::chassisSwitchHandler /* function */,
			NULL );

	if (status != kIOReturnSuccess)
	{
		CTRLLOOP_DLOG( "SMU_Neo2_PlatformPlugin::registerChassisSwitchNotifier failed: 0x%08lx\n", status );
	}
}

/*******************************************************************************
 * Method:
 *	pollChassisSwitch
 *
 * Purpose:
 *	Call AppleSMU to get the state of the clamshell switch.
 ******************************************************************************/
// virtual
OSBoolean *SMU_Neo2_PlatformPlugin::pollChassisSwitch( void )
{
	IOReturn	status;
	UInt8		switchState;

	if (!appleSMU && !getAppleSMU()) return( kOSBooleanFalse );

	status = appleSMU->callPlatformFunction( kSymGetExtSwitches, false,
			(void *) &switchState /* UInt8 * byte */,
			NULL,
			NULL,
			NULL );

	if (status == kIOReturnSuccess)
	{
		return ( switchState & kClamshellClosedEventMask ) ? kOSBooleanTrue : kOSBooleanFalse;
	}
	else
	{
		CTRLLOOP_DLOG( "SMU_Neo2_PlatformPlugin::pollChassisSwitch failed: 0x%08lx\n", status );
		return( kOSBooleanFalse );
	}
}

/*******************************************************************************
 * Method:
 *	chassisSwitchHandler
 *
 * Purpose:
 *	Static, unsynchronized SMU interrupt handler. When an interrupt notification
 *	comes in, we generate a Plugin event so it can be handled synchronously.
 ******************************************************************************/
// static -- conforms to AppleSMUClient function prototype
void SMU_Neo2_PlatformPlugin::chassisSwitchHandler( IOService * client,
		UInt8 matchingMask, UInt32 length, UInt8 * buffer )
{
	SMU_Neo2_PlatformPlugin		*me;
	IOReturn					result;
	IOPPluginEventData			switchEvent;

	CTRLLOOP_DLOG( "SMU_Neo2_PlatformPlugin::chassisSwitchHandler entered\n" );

	if ((me = OSDynamicCast( SMU_Neo2_PlatformPlugin, client )) == NULL) 
	{
		CTRLLOOP_DLOG( "SMU_Neo2_PlatformPlugin::chassisSwitchHandler client is invalid\n" );
		return;
	}

	// Construct an event for the plugin dispatch mechanism. This will call the synchronized
	// chassis switch handler after obtaining the plugin gate lock.
	switchEvent.eventType	= IOPPluginEventMisc;
	switchEvent.param1		= (void *) me;
	switchEvent.param2		= (void *) length;
	switchEvent.param3		= (void *) buffer;
	switchEvent.param4		= (void *) &SMU_Neo2_PlatformPlugin::chassisSwitchSyncHandler;

	// Send off the event
	result = me->dispatchEvent( &switchEvent );

	// Report errors
	if (result != kIOReturnSuccess)
	{
		CTRLLOOP_DLOG( "SMU_Neo2_PlatformPlugin::chassisSwitchHandler got error: 0x%08lx\n", result );
	}
}

/*******************************************************************************
 * Method:
 *	chassisSwitchSyncHandler
 *
 * Purpose:
 *	Synchronized instance method for handling chassis switch events from SMU.
 *	This method parses the environmental interrupt data and sets the plugin's
 *	environment dictionary appropriately. Control loops will be notified
 *	through the normal mechanism (IOPlatformCtrlLoop::updateMetaState()).
 ******************************************************************************/
// static
IOReturn SMU_Neo2_PlatformPlugin::chassisSwitchSyncHandler( void * p1 /* SMU_Neo2_PlatformPlugin* me */,
															void * p2 /* UInt32 length */,
															void * p3 /* UInt8* buffer */ )
{
	SMU_Neo2_PlatformPlugin* me = OSDynamicCast( SMU_Neo2_PlatformPlugin, (OSMetaClassBase *) p1 );
	UInt32 length = (UInt32) p2;
	UInt8 * buffer = (UInt8 *) p3;

	CTRLLOOP_DLOG( "SMU_Neo2_PlatformPlugin::chassisSwitchSyncHandler 0x%08lX 0x%08lX 0x%08lX\n",
			(UInt32)p1, (UInt32)p2, (UInt32)p3);

	if (!me)
	{
		CTRLLOOP_DLOG( "AppleSMU::chassisSwitchSyncHandler I've lost myself ?!!?!! \n" );
		return( kIOReturnBadArgument );
	}

	if (length == 0)
	{
		CTRLLOOP_DLOG( "AppleSMU::chassisSwitchSyncHandler invalid length\n" );
		return( kIOReturnBadArgument );
	}

	if (buffer == NULL)
	{
		CTRLLOOP_DLOG( "AppleSMU::chassisSwitchSyncHandler invalid buffer pointer\n" );
		return( kIOReturnBadArgument );
	}

	me->setEnv( gIOPPluginEnvChassisSwitch, ( *buffer & kClamshellClosedEventMask ) ? kOSBooleanTrue : kOSBooleanFalse );

	return( kIOReturnSuccess );
}
