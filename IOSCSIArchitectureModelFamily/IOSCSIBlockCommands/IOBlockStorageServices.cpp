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

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include "IOBlockStorageServices.h"


#if ( SCSI_BLOCK_SERVICES_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_BLOCK_SERVICES_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_BLOCK_SERVICES_DEBUGGING_LEVEL >= 3 )
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
	IOBlockStorageServices *	owner;

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
OSDefineMetaClassAndStructors ( IOBlockStorageServices, IOBlockStorageDevice );

//---------------------------------------------------------------------------
// attach to provider.

bool
IOBlockStorageServices::attach ( IOService * provider )
{
	
	STATUS_LOG ( ( "IOBlockStorageServices: attach called\n" ) );
	
	if ( !super::attach ( provider ) )
	{
		return false;
	}
	
	fProvider = OSDynamicCast ( IOSCSIBlockCommandsDevice, provider );
	if ( fProvider == NULL )
	{
		
		ERROR_LOG ( ( "IOBlockStorageServices: attach; wrong provider type!\n" ) );
		return false;
		
	}
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey, fProvider->GetProtocolCharacteristicsDictionary ( ) );
	setProperty ( kIOPropertyDeviceCharacteristicsKey, fProvider->GetDeviceCharacteristicsDictionary ( ) );
	
	if ( fProvider->ReportDeviceMediaRemovability ( ) == true )
	{
		
		fMediaChanged			= false;
		fMediaPresent			= false;
		
	}
	
	else
	{
		
		fMediaChanged			= true;
		fMediaPresent			= true;
		
	}
	
	fMaxReadBlocks 	= fProvider->ReportDeviceMaxBlocksReadTransfer ( );
	fMaxWriteBlocks = fProvider->ReportDeviceMaxBlocksWriteTransfer ( );
	
	STATUS_LOG ( ( "IOBlockStorageServices: attach exiting\n" ) );
	
	return true;
	
}


//---------------------------------------------------------------------------
// detach from provider.

void
IOBlockStorageServices::detach ( IOService * provider )
{

	STATUS_LOG ( ( "IOBlockStorageServices: detach called\n" ) );
		
	super::detach ( provider );

	STATUS_LOG ( ( "IOBlockStorageServices: detach exiting\n" ) );
		
}


//---------------------------------------------------------------------------
// message

IOReturn
IOBlockStorageServices::message ( UInt32 		type,
								  IOService *	nub,
								  void *		arg )
{
	
	IOReturn 	status = kIOReturnSuccess;
	
	ERROR_LOG ( ( "IOBlockStorageServices::message called\n" ) );
		
	switch ( type )
	{
		
		case kIOMessageMediaStateHasChanged:
		{

			ERROR_LOG ( ( "type = kIOMessageMediaStateHasChanged, nub = %p\n", nub ) );
			
			fMediaChanged	= true;
			fMediaPresent	= true;
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
IOBlockStorageServices::AsyncReadWriteComplete (	void * 			clientData,
													IOReturn		status,
													UInt64 			actualByteCount )
{
	IOBlockStorageServices *	owner;
	BlockServicesClientData * 	servicesData;
	IOStorageCompletion			returnData;
	bool						commandComplete = true;
	
	// Save the IOCompletion information so that it may be returned
	// to the client.
	servicesData 	= ( BlockServicesClientData * ) clientData;
	returnData 		= servicesData->completionData;
	owner 			= servicesData->owner;

	STATUS_LOG(("IOBlockStorageServices: AsyncReadWriteComplete; command status %x\n", status ));
	// Check to see if an error occurred that on which the request should be retried.
	if ((( status != kIOReturnNotAttached ) && ( status != kIOReturnOffline ) &&
		( status != kIOReturnSuccess )) && ( servicesData->retriesLeft > 0 ))
	{
		IOReturn 	requestStatus;

		STATUS_LOG(("IOBlockStorageServices: AsyncReadWriteComplete; retry command\n"));
		// An error occurred, but it is one on which the command should be retried.  Decrement
		// the retry counter and try again.
		servicesData->retriesLeft--;
		requestStatus = owner->fProvider->AsyncReadWrite( 
										servicesData->clientBuffer, 
										servicesData->clientStartingBlock, 
										servicesData->clientRequestedBlockCount, 
										servicesData->clientRequestedBlockSize, 
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
		owner->fProvider->release();	
		owner->release();
		
		IOStorage::complete ( returnData, status, actualByteCount );
	}
}

//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::doAsyncReadWrite (	IOMemoryDescriptor *	buffer,
											UInt32					block,
											UInt32					nblks,
											IOStorageCompletion		completion )
{
	BlockServicesClientData	*	clientData;
	IODirection					direction;
	IOReturn					requestStatus;
	UInt32						requestBlockSize;
	
	// Return errors for incoming I/O if we have been terminated.
	if ( isInactive() != false )
	{
		return kIOReturnNotAttached;
	}

	direction = buffer->getDirection();
	if (( direction != kIODirectionIn ) && ( direction != kIODirectionOut ))
	{
		// This is neither a read nor write request (since this is a read/write method,
		// what kind of request is it?) return an error to the client.
		return kIOReturnBadArgument;
	}
	
	clientData = (BlockServicesClientData *) IOMalloc( sizeof(BlockServicesClientData) );
	if ( clientData == NULL )
	{
		ERROR_LOG ( ( "IOBlockStorageServices: doAsyncReadWrite; clientData malloc failed!\n" ) );
		return kIOReturnNoResources;
	}

	// Make sure we don't go away while the command is being executed.
	retain();
	fProvider->retain();	

	requestBlockSize = fProvider->ReportMediumBlockSize();
	
	STATUS_LOG ( ( "IOBlockStorageServices: doAsyncReadWrite; save completion data!\n" ) );

	// Set the owner of this request.
	clientData->owner = this;
	
	// Save the client's request parameters.
	clientData->completionData 				= completion;
	clientData->clientBuffer 				= buffer;
	clientData->clientStartingBlock 		= block;
	clientData->clientRequestedBlockCount 	= nblks;
	clientData->clientRequestedBlockSize 	= requestBlockSize;
	
	// Set the retry limit to the maximum
	clientData->retriesLeft = kNumberRetries;
	
	fProvider->CheckPowerState();

	requestStatus = fProvider->AsyncReadWrite ( buffer, (UInt64) block, (UInt64) nblks, (UInt64) requestBlockSize, (void *) clientData );
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
IOBlockStorageServices::doSyncReadWrite (	IOMemoryDescriptor * 	buffer,
											UInt32 					block,
											UInt32 					nblks )
{
	
	IOReturn	result;
	
	// Return errors for incoming I/O if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	result = fProvider->SyncReadWrite ( buffer, block, nblks, fProvider->ReportMediumBlockSize ( ) );
	
	// Release the retains for this command.	
	fProvider->release ( );
	release ( );
	
	return result;
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::doEjectMedia ( void )
{
	
	IOReturn	result;
	
	fMediaPresent = false;
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	result = fProvider->EjectTheMedium ( );
	
	// Release the retains for this command.	
	fProvider->release ( );
	release ( );
	
	return result;
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::doFormatMedia ( UInt64 byteCapacity )
{
	
	IOReturn result;
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );	
	
	fProvider->CheckPowerState ( );	
	
	// Execute the command
	result = fProvider->FormatMedium ( byteCapacity / ( fProvider->ReportMediumBlockSize ( ) ),
									   fProvider->ReportMediumBlockSize ( ) );
	
	// Release the retains for this command.	
	fProvider->release ( );
	release ( );
	
	return result;
	
}


//---------------------------------------------------------------------------
// 

UInt32
IOBlockStorageServices::doGetFormatCapacities ( 	UInt64 * capacities,
													UInt32   capacitiesMaxCount ) const
{
	
	IOReturn result;
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );	
	
	// Execute the command
	result = fProvider->GetFormatCapacities ( capacities, capacitiesMaxCount );
	
	// Release the retains for this command.	
	fProvider->release ( );
	release ( );
	
	return result;
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::doLockUnlockMedia ( bool doLock )
{
	
	IOReturn result;
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );	
	
	// Execute the command
	result = fProvider->LockUnlockMedium ( doLock );
	
	// Release the retains for this command.	
	fProvider->release ( );
	release ( );
	
	return result;
	
}


//---------------------------------------------------------------------------
//

IOReturn
IOBlockStorageServices::doSynchronizeCache ( void )
{
	
	IOReturn result;
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );	
	
	// Execute the command
	result = fProvider->SynchronizeCache ( );
	
	// Release the retains for this command.	
	fProvider->release ( );
	release ( );
	
	return result;
	
}


//---------------------------------------------------------------------------
// 

char *
IOBlockStorageServices::getVendorString ( void )
{

	return fProvider->GetVendorString ( );

}


//---------------------------------------------------------------------------
// 

char *
IOBlockStorageServices::getProductString ( void )
{
	
	return fProvider->GetProductString ( );
	
}


//---------------------------------------------------------------------------
// 

char *
IOBlockStorageServices::getRevisionString ( void )
{
	
	return fProvider->GetRevisionString ( );
	
}


//---------------------------------------------------------------------------
// 

char *
IOBlockStorageServices::getAdditionalDeviceInfoString ( void )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	return ( "No Additional Device Info" );
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::reportBlockSize ( UInt64 * blockSize )
{
	
	*blockSize = fProvider->ReportMediumBlockSize ( );
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::reportEjectability ( bool * isEjectable )
{
	
	*isEjectable = fProvider->ReportDeviceMediaRemovability ( );
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::reportLockability ( bool * isLockable )
{
	
	*isLockable = true;
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::reportPollRequirements ( 	bool * pollIsRequired,
													bool * pollIsExpensive )
{
	
	*pollIsRequired 	= false;
	*pollIsExpensive 	= false;
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::reportMaxReadTransfer ( UInt64 		blockSize,
												UInt64 * 	max )
{
	
	if ( fMaxReadBlocks == 0 )
	{
		*max = blockSize * 256;
	}
	
	else
	{
		*max = blockSize * fMaxReadBlocks;
	}
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::reportMaxValidBlock ( UInt64 * maxBlock )
{
	
	*maxBlock = ( fProvider->ReportMediumTotalBlockCount ( ) - 1 );
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::reportMaxWriteTransfer ( 	UInt64		blockSize,
													UInt64 * 	max )
{
	
	if ( fMaxWriteBlocks == 0 )
	{
		*max = blockSize * 256;
	}
	
	else
	{
		*max = blockSize * fMaxWriteBlocks;
	}
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::reportMediaState ( 	bool * mediaPresent,
												bool * changed )	
{
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::reportMediaState.\n" ) );
	
	*mediaPresent 	= fMediaPresent;
	*changed 		= fMediaChanged;


	if ( fMediaChanged )
	{
		fMediaChanged = !fMediaChanged;
	}
	
	return kIOReturnSuccess;
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::reportRemovability ( bool * isRemovable )
{
	*isRemovable = fProvider->ReportDeviceMediaRemovability();
	return kIOReturnSuccess;
}


//---------------------------------------------------------------------------
// 

IOReturn
IOBlockStorageServices::reportWriteProtection ( bool * isWriteProtected )
{
	*isWriteProtected = fProvider->ReportMediumWriteProtection();
	return kIOReturnSuccess;
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOBlockStorageServices, 1 );
OSMetaClassDefineReservedUnused( IOBlockStorageServices, 2 );
OSMetaClassDefineReservedUnused( IOBlockStorageServices, 3 );
OSMetaClassDefineReservedUnused( IOBlockStorageServices, 4 );
OSMetaClassDefineReservedUnused( IOBlockStorageServices, 5 );
OSMetaClassDefineReservedUnused( IOBlockStorageServices, 6 );
OSMetaClassDefineReservedUnused( IOBlockStorageServices, 7 );
OSMetaClassDefineReservedUnused( IOBlockStorageServices, 8 );

//--------------------------------------------------------------------------------------
//							End				Of				File
