/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2000-2001 Apple Computer, Inc.  All rights reserved.
 *
 * IOATABlockStorageDevice.cpp
 *
 * This subclass implements a relay to a protocol and device-specific
 * provider.
 *
 *
 * HISTORY
 *
 *		09/28/2000	CJS		Started IOATABlockStorageDevice
 *							(ported IOATAHDDriveNub)
 *
 */

#include <IOKit/IOLib.h>
#include "IOATABlockStorageDriver.h"
#include "IOATABlockStorageDevice.h"
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors ( IOATABlockStorageDevice, IOBlockStorageDevice );

#if ( ATA_BLOCK_STORAGE_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)			IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( ATA_BLOCK_STORAGE_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)			IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( ATA_BLOCK_STORAGE_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)			IOLog x
#else
#define STATUS_LOG(x)
#endif


//---------------------------------------------------------------------------
// attach to provider.

bool
IOATABlockStorageDevice::attach ( IOService * provider )
{
	
	OSDictionary *	dictionary = OSDictionary::withCapacity ( 1 );
	
	if ( !super::attach ( provider ) )
		return false;
	
	fProvider = OSDynamicCast ( IOATABlockStorageDriver, provider );
	if ( fProvider == NULL )
	{
		
		IOLog ( "IOATABlockStorageDevice: attach; wrong provider type!\n" );
		return false;
		
	}
	
	if ( dictionary != NULL )
	{
		
		dictionary->setObject ( kIOPropertyPhysicalInterconnectTypeKey, fProvider->getProperty ( kIOPropertyPhysicalInterconnectTypeKey ) );
		dictionary->setObject ( kIOPropertyPhysicalInterconnectLocationKey, fProvider->getProperty ( kIOPropertyPhysicalInterconnectLocationKey ) );
		
		setProperty ( kIOPropertyProtocolCharacteristicsKey, dictionary );
		
		dictionary->release ( );
		
	}
	
	dictionary = OSDictionary::withCapacity ( 1 );
	if ( dictionary != NULL )
	{
		
		char *		string;
		OSString *	theString;
		
		string = getVendorString ( );
		
		theString = OSString::withCString ( string );
		if ( theString != NULL )
		{
			dictionary->setObject ( kIOPropertyVendorNameKey, theString );
			theString->release ( );
		}

		string = getProductString ( );

		theString = OSString::withCString ( string );
		if ( theString != NULL )
		{
			dictionary->setObject ( kIOPropertyProductNameKey, theString );
			theString->release ( );
		}

		string = getRevisionString ( );

		theString = OSString::withCString ( string );
		if ( theString != NULL )
		{
			dictionary->setObject ( kIOPropertyProductRevisionLevelKey, theString );
			theString->release ( );
		}
		
		setProperty ( kIOPropertyDeviceCharacteristicsKey, dictionary );
		
		dictionary->release ( );
		
	}
	
	return true;
	
}


//---------------------------------------------------------------------------
// detach from provider.

void
IOATABlockStorageDevice::detach ( IOService * provider )
{
	
	if ( fProvider == provider )
		fProvider = 0;
	
	super::detach ( provider );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::setProperties ( OSObject * properties )
{
	
	OSDictionary *	dict 		= NULL;
	OSNumber *		number 		= NULL;
	IOReturn		status 		= kIOReturnSuccess;
	UInt32			features	= 0;
	
	STATUS_LOG ( ( "IOATABlockStorageDevice::setProperties called\n" ) );
	
	number = ( OSNumber * ) fProvider->getProperty ( kIOATASupportedFeaturesKey );
	features = number->unsigned32BitValue ( );
	
	if ( ( features & kIOATAFeatureAdvancedPowerManagement ) == 0 )
	{
		
		ERROR_LOG ( ( "IOATABlockStorageDevice::setProperties called on unsupported drive\n" ) );
		return kIOReturnUnsupported;
		
	}
	
	dict = OSDynamicCast ( OSDictionary, properties );
	if ( dict != NULL )
	{
		number = OSDynamicCast ( OSNumber, dict->getObject ( "APM Level" ) );
	}
	
	if ( number != NULL )
	{
				
		UInt8	apmLevel = number->unsigned8BitValue ( );
		
		if ( ( apmLevel == 0 ) || ( apmLevel == 0xFF ) )
		{
			return kIOReturnBadArgument;
		}

		STATUS_LOG ( ( "apmLevel = %d\n", apmLevel ) );
		
		status = fProvider->setAdvancedPowerManagementLevel ( apmLevel, true );
		
	}
	
	else
		status = kIOReturnBadArgument;
	
	STATUS_LOG ( ( "IOATABlockStorageDevice::leave setProperties\n" ) );
	
	return status;
	
}




//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::doAsyncReadWrite (
											IOMemoryDescriptor *	buffer,
											UInt32					block,
											UInt32					nblks,
											IOStorageCompletion		completion )
{
	
	// Block incoming I/O if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}
	
	fProvider->checkPowerState ( );
	return fProvider->doAsyncReadWrite ( buffer, block, nblks, completion );
	
}


//---------------------------------------------------------------------------
//

IOReturn
IOATABlockStorageDevice::doSyncReadWrite (	IOMemoryDescriptor * buffer,
											UInt32 block, UInt32 nblks )
{
	
	// Block incoming I/O if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}
	
	fProvider->checkPowerState ( );
	return fProvider->doSyncReadWrite ( buffer, block, nblks );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::doEjectMedia ( void )
{
	
	return fProvider->doEjectMedia ( );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::doFormatMedia ( UInt64 byteCapacity )
{
	
	return fProvider->doFormatMedia ( byteCapacity );
	
}


//---------------------------------------------------------------------------
// 

UInt32
IOATABlockStorageDevice::doGetFormatCapacities ( UInt64 * capacities,
												 UInt32   capacitiesMaxCount ) const
{
	
	return fProvider->doGetFormatCapacities ( capacities, capacitiesMaxCount );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::doLockUnlockMedia ( bool doLock )
{
	
	return fProvider->doLockUnlockMedia ( doLock );
	
}


//---------------------------------------------------------------------------
//

IOReturn
IOATABlockStorageDevice::doSynchronizeCache ( void )
{
	
	// Block incoming I/O if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}
	
	fProvider->checkPowerState ( );
	return fProvider->doSynchronizeCache ( );
	
}


//---------------------------------------------------------------------------
// 

char *
IOATABlockStorageDevice::getVendorString ( void )
{

	return fProvider->getVendorString ( );

}


//---------------------------------------------------------------------------
// 

char *
IOATABlockStorageDevice::getProductString ( void )
{
	
	return fProvider->getProductString ( );
	
}


//---------------------------------------------------------------------------
// 

char *
IOATABlockStorageDevice::getRevisionString ( void )
{
	
	return fProvider->getRevisionString ( );
	
}


//---------------------------------------------------------------------------
// 

char *
IOATABlockStorageDevice::getAdditionalDeviceInfoString ( void )
{
	
	return fProvider->getAdditionalDeviceInfoString ( );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::reportBlockSize ( UInt64 * blockSize )
{
	
	return fProvider->reportBlockSize ( blockSize );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::reportEjectability ( bool * isEjectable )
{
	
	return fProvider->reportEjectability ( isEjectable );

}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::reportLockability ( bool * isLockable )
{
	
	return fProvider->reportLockability ( isLockable );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::reportPollRequirements ( 	bool * pollIsRequired,
													bool * pollIsExpensive )
{
	
	return fProvider->reportPollRequirements ( pollIsRequired, pollIsExpensive );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::reportMaxReadTransfer ( UInt64 blockSize,
													UInt64 * max )
{
	
	return fProvider->reportMaxReadTransfer ( blockSize, max );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::reportMaxValidBlock ( UInt64 * maxBlock )
{

	return fProvider->reportMaxValidBlock ( maxBlock );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::reportMaxWriteTransfer ( UInt64	blockSize,
												  UInt64 *	max )
{
	
	return fProvider->reportMaxWriteTransfer ( blockSize, max );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::reportMediaState ( bool * mediaPresent,
											bool * changed )	
{
	
	return fProvider->reportMediaState ( mediaPresent, changed );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::reportRemovability ( bool * isRemovable )
{
	
	return fProvider->reportRemovability ( isRemovable );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOATABlockStorageDevice::reportWriteProtection ( bool * isWriteProtected )
{
	
	return fProvider->reportWriteProtection ( isWriteProtected );
	
}


// binary compatibility reserved method space
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 1 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 2 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 3 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 4 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 5 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 6 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 7 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 8 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 9 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 10 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 11 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 12 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 13 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 14 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 15 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDevice, 16 );

//--------------------------------------------------------------------------------------
//							End				Of				File
//--------------------------------------------------------------------------------------