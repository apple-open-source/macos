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
 * IOReducedBlockServices.cpp
 *
 * This subclass implements a relay to a protocol and device-specific
 * provider.
 *
 */

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include "IOReducedBlockServices.h"


#if ( SCSI_REDUCED_BLOCK_SERVICES_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_REDUCED_BLOCK_SERVICES_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_REDUCED_BLOCK_SERVICES_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif

// The command should be tried 5 times.  The original attempt 
// plus 4 retries.
#define kNumberRetries		4

// Structure for the asynch client data
struct BlockServicesClientData
{
	// The object that owns the copy of this structure.
	IOReducedBlockServices		*owner;

	// The request parameters provided by the client.
	IOStorageCompletion			completionData;
	IOMemoryDescriptor * 		clientBuffer;
	UInt32 						clientStartingBlock;
	UInt32 						clientRequestedBlockCount;
	UInt32 						clientRequestedBlockSize;

	// The internally needed parameters.
	UInt32						retriesLeft;
};

typedef struct BlockServicesClientData	BlockServicesClientData;

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors ( IOReducedBlockServices, IOBlockStorageDevice );

//---------------------------------------------------------------------------
// attach to provider.

bool
IOReducedBlockServices::attach ( IOService * provider )
{

	STATUS_LOG ( ( "IOReducedBlockServices: attach called\n" ) );
	
	if ( !super::attach ( provider ) )
		return false;
	
	fProvider = OSDynamicCast ( IOSCSIPeripheralDeviceType0E, provider );
	if ( fProvider == NULL )
	{
		
		STATUS_LOG ( ( "IOReducedBlockServices: attach; wrong provider type!\n" ) );
		return false;
		
	}
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey, fProvider->GetProtocolCharacteristicsDictionary ( ) );
	setProperty ( kIOPropertyDeviceCharacteristicsKey, fProvider->GetDeviceCharacteristicsDictionary ( ) );
	
	STATUS_LOG ( ( "IOReducedBlockServices: attach exiting\n" ) );
		
	return true;
	
}


//---------------------------------------------------------------------------
// detach from provider.

void
IOReducedBlockServices::detach ( IOService * provider )
{

	STATUS_LOG ( ( "IOReducedBlockServices: detach called\n" ) );
		
	super::detach ( provider );

	STATUS_LOG ( ( "IOReducedBlockServices: detach exiting\n" ) );
		
}


//---------------------------------------------------------------------------
// message

IOReturn
IOReducedBlockServices::message ( UInt32 		type,
								  IOService *	nub,
								  void *		arg )
{
	
	IOReturn 	status = kIOReturnSuccess;
	
	ERROR_LOG ( ( "IOReducedBlockServices::message called\n" ) );
		
	switch ( type )
	{
		
		case kIOMessageMediaStateHasChanged:
		{

			ERROR_LOG ( ( "type = kIOMessageMediaStateHasChanged, nub = %p\n", nub ) );
			status = messageClients ( type, arg );
			ERROR_LOG ( ( "status = %ld\n", ( UInt32 ) status ) );
		
		}
		
		break;
				
		default:
		{
			status = super::message ( type, nub, arg );
		}
		
		break;
		
	}
	
	return status;
	
}


//---------------------------------------------------------------------------
// 

void 
IOReducedBlockServices::AsyncReadWriteComplete( void * 			clientData,
                                				IOReturn		status,
                                				UInt64 			actualByteCount )
{
	IOReducedBlockServices		*owner;
	BlockServicesClientData * 	servicesData;
	IOStorageCompletion			returnData;
	bool						commandComplete = true;
	
	// Save the IOCompletion information so that it may be returned
	// to the client.
	servicesData 	= ( BlockServicesClientData * ) clientData;
	returnData 		= servicesData->completionData;
	owner 			= servicesData->owner;

	STATUS_LOG( ("IOReducedBlockServices: AsyncReadWriteComplete; command status %x\n", status ));
	// Check to see if an error occurred that the request should be retried on
	if ((( status != kIOReturnNotAttached ) && ( status != kIOReturnOffline ) &&
		( status != kIOReturnSuccess )) && ( servicesData->retriesLeft > 0 ))
	{
		IOReturn 	requestStatus;

		STATUS_LOG(("IOReducedBlockServices: AsyncReadWriteComplete; retry command\n"));
		// An error occurred, but it is one on which the command should be retried.  Decrement
		// the retry counter and try again.
		servicesData->retriesLeft--;
		requestStatus = owner->fProvider->AsyncReadWrite( 
										servicesData->clientBuffer, 
										servicesData->clientStartingBlock, 
										servicesData->clientRequestedBlockCount, 
										clientData );
		if ( requestStatus != kIOReturnSuccess )
		{
			commandComplete = true;
		}
		else
		{
			commandComplete = false;
		}
	}

	if ( commandComplete == true )
	{
		IOFree ( clientData, sizeof ( BlockServicesClientData ) );

		// Release the retains for this command.
		owner->fProvider->release ( );	
		owner->release ( );
		
		IOStorage::complete ( returnData, status, actualByteCount );
	}
}

//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::doAsyncReadWrite (
											IOMemoryDescriptor *	buffer,
											UInt32					block,
											UInt32					nblks,
											IOStorageCompletion		completion )
{
	BlockServicesClientData	*	clientData;
	IODirection					direction;
	IOReturn					requestStatus;
	
	// Return an error for incoming I/O if we have been terminated
	if ( isInactive() != false )
	{
		return kIOReturnNotAttached;
	}

	direction = buffer->getDirection();
	if (( direction != kIODirectionIn ) && ( direction != kIODirectionOut ))
	{
		// This is neither a read or write request (since this is a read/write method,
		// what kind of request is it?) return an error to the client.
		return kIOReturnBadArgument;
	}
	
	clientData = (BlockServicesClientData *) IOMalloc( sizeof(BlockServicesClientData) );
	if ( clientData == NULL )
	{
		ERROR_LOG ( ( "IOReducedBlockServices: doAsyncReadWrite; clientData malloc failed!\n" ) );
		return kIOReturnNoResources;
	}

	// Make sure we don't go away while the command is being executed.
	retain ( );
	fProvider->retain ( );	

	STATUS_LOG ( ( "IOReducedBlockServices: doAsyncReadWrite; save completion data!\n" ) );

	// Set the owner of this request.
	clientData->owner = this;
	
	// Save the client's request parameters.
	clientData->completionData 				= completion;
	clientData->clientBuffer 				= buffer;
	clientData->clientStartingBlock 		= block;
	clientData->clientRequestedBlockCount 	= nblks;
	
	// Set the retry limit to the maximum
	clientData->retriesLeft = kNumberRetries;

	fProvider->CheckPowerState();

	requestStatus = fProvider->AsyncReadWrite ( buffer, block, nblks, (void *) clientData );
	if ( requestStatus != kIOReturnSuccess )
	{
		if ( clientData != NULL )
		{
			IOFree ( clientData, sizeof ( BlockServicesClientData ) );
		}
	}
	
	return requestStatus;
}


//---------------------------------------------------------------------------
//

IOReturn
IOReducedBlockServices::doSyncReadWrite (	IOMemoryDescriptor * 	buffer,
											UInt32 					block,
											UInt32 					nblks )
{
	IOReturn result;
	
	// Return an error for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	fProvider->CheckPowerState ( );	

	// Execute the command
	result = fProvider->SyncReadWrite ( buffer, block, nblks );

	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	return result;
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::doEjectMedia ( void )
{
	IOReturn result;
	
	// Return an error for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	result = fProvider->EjectTheMedia ( );

	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	return result;
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::doFormatMedia ( UInt64 byteCapacity )
{
	IOReturn result;
	
	// Return an error for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	fProvider->CheckPowerState ( );	

	// Execute the command
	result = fProvider->FormatMedia ( byteCapacity );

	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	return result;
}


//---------------------------------------------------------------------------
// 

UInt32
IOReducedBlockServices::doGetFormatCapacities ( 	UInt64 * capacities,
													UInt32   capacitiesMaxCount ) const
{
	IOReturn result;
	
	// Return an error for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Execute the command
	result = fProvider->GetFormatCapacities ( capacities, capacitiesMaxCount );

	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	return result;
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::doLockUnlockMedia ( bool doLock )
{
	IOReturn result;
	
	// Return an error for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Execute the command
	result = fProvider->LockUnlockMedia ( doLock );

	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	return result;
}


//---------------------------------------------------------------------------
//

IOReturn
IOReducedBlockServices::doSynchronizeCache ( void )
{
	IOReturn result;
	
	// Return an error for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	// Make sure we don't away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Execute the command
	result = fProvider->SynchronizeCache ( );

	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	return result;
}


//---------------------------------------------------------------------------
// 

char *
IOReducedBlockServices::getVendorString ( void )
{

	return fProvider->GetVendorString ( );

}


//---------------------------------------------------------------------------
// 

char *
IOReducedBlockServices::getProductString ( void )
{
	
	return fProvider->GetProductString ( );
	
}


//---------------------------------------------------------------------------
// 

char *
IOReducedBlockServices::getRevisionString ( void )
{
	
	return fProvider->GetRevisionString ( );
	
}


//---------------------------------------------------------------------------
// 

char *
IOReducedBlockServices::getAdditionalDeviceInfoString ( void )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	return ( "No Additional Device Info" );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::reportBlockSize ( UInt64 * blockSize )
{
	
	return fProvider->ReportBlockSize ( blockSize );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::reportEjectability ( bool * isEjectable )
{
	
	return fProvider->ReportEjectability ( isEjectable );

}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::reportLockability ( bool * isLockable )
{
	
	return fProvider->ReportLockability ( isLockable );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::reportPollRequirements ( 	bool * pollIsRequired,
													bool * pollIsExpensive )
{
	
	return fProvider->ReportPollRequirements ( pollIsRequired, pollIsExpensive );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::reportMaxReadTransfer ( UInt64 blockSize,
												 UInt64 * max )
{
	
	return fProvider->ReportMaxReadTransfer ( blockSize, max );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::reportMaxValidBlock ( UInt64 * maxBlock )
{

	return fProvider->ReportMaxValidBlock ( maxBlock );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::reportMaxWriteTransfer ( UInt64	blockSize,
													 UInt64 * max )
{
	
	return fProvider->ReportMaxWriteTransfer ( blockSize, max );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::reportMediaState ( 	bool * mediaPresent,
												bool * changed )	
{
	
	return fProvider->ReportMediaState ( mediaPresent, changed );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::reportRemovability ( bool * isRemovable )
{
	
	return fProvider->ReportRemovability ( isRemovable );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOReducedBlockServices::reportWriteProtection ( bool * isWriteProtected )
{
	
	return fProvider->ReportWriteProtection ( isWriteProtected );
	
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOReducedBlockServices, 1 );
OSMetaClassDefineReservedUnused( IOReducedBlockServices, 2 );
OSMetaClassDefineReservedUnused( IOReducedBlockServices, 3 );
OSMetaClassDefineReservedUnused( IOReducedBlockServices, 4 );
OSMetaClassDefineReservedUnused( IOReducedBlockServices, 5 );
OSMetaClassDefineReservedUnused( IOReducedBlockServices, 6 );
OSMetaClassDefineReservedUnused( IOReducedBlockServices, 7 );
OSMetaClassDefineReservedUnused( IOReducedBlockServices, 8 );



//--------------------------------------------------------------------------------------
//							End				Of				File
//--------------------------------------------------------------------------------------
