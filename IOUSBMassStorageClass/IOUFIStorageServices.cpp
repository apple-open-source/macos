/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include "IOUFIStorageServices.h"

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


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
	IOUFIStorageServices *		owner;

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
OSDefineMetaClassAndStructors ( IOUFIStorageServices, IOBlockStorageDevice );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ attach - attach to provider.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUFIStorageServices::attach( IOService * provider )
{
	STATUS_LOG ( ( "IOUFIStorageServices: attach called\n" ) );
	
	if ( !super::attach ( provider ) )
	{
		return false;
	}
	
	fProvider = OSDynamicCast ( IOUSBMassStorageUFIDevice, provider );
	if ( fProvider == NULL )
	{
		ERROR_LOG ( ( "IOUFIStorageServices: attach; wrong provider type!\n" ) );
		return false;
	}
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey, fProvider->GetProtocolCharacteristicsDictionary ( ) );
	setProperty ( kIOPropertyDeviceCharacteristicsKey, fProvider->GetDeviceCharacteristicsDictionary ( ) );
	
	fMediaChanged			= false;
	fMediaPresent			= false;
	
	fMaxReadBlocks 	= fProvider->ReportDeviceMaxBlocksReadTransfer();
	fMaxWriteBlocks = fProvider->ReportDeviceMaxBlocksWriteTransfer();
	
	STATUS_LOG ( ( "IOUFIStorageServices: attach exiting\n" ) );
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ detach - detach from provider.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOUFIStorageServices::detach( IOService * provider )
{
	STATUS_LOG ( ( "IOUFIStorageServices: detach called\n" ) );
		
	super::detach( provider );

	STATUS_LOG ( ( "IOUFIStorageServices: detach exiting\n" ) );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ message - handles messages.									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::message( 	UInt32 			type,
								IOService *		nub,
								void *			arg )
{
	IOReturn 	status = kIOReturnSuccess;
	
	ERROR_LOG ( ( "IOUFIStorageServices::message called\n" ) );
		
	switch ( type )
	{
		case kIOMessageMediaStateHasChanged:
		{
			ERROR_LOG ( ( "type = kIOMessageMediaStateHasChanged, nub = %p\n", nub ) );
			
			fMediaChanged	= true;
			fMediaPresent	= true;
			status = messageClients ( type, arg, sizeof ( IOMediaState ) );
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWriteComplete - Completion routine for I/O	  [STATIC][PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUFIStorageServices::AsyncReadWriteComplete(	void * 			clientData,
												IOReturn		status,
												UInt64 			actualByteCount )
{
	IOUFIStorageServices *		owner;
	BlockServicesClientData * 	servicesData;
	IOStorageCompletion			returnData;
	bool						commandComplete = true;
	
	// Save the IOCompletion information so that it may be returned
	// to the client.
	servicesData 	= ( BlockServicesClientData * ) clientData;
	returnData 		= servicesData->completionData;
	owner 			= servicesData->owner;

	STATUS_LOG(("IOUFIStorageServices: AsyncReadWriteComplete; command status %x\n", status ));
	// Check to see if an error occurred that on which the request should be retried.
	if ((( status != kIOReturnNotAttached ) && ( status != kIOReturnOffline ) &&
		( status != kIOReturnSuccess )) && ( servicesData->retriesLeft > 0 ))
	{
		IOReturn 	requestStatus;

		STATUS_LOG(("IOUFIStorageServices: AsyncReadWriteComplete; retry command\n"));
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doAsyncReadWrite												   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::doAsyncReadWrite (	IOMemoryDescriptor *	buffer,
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
	
	clientData = (BlockServicesClientData *) IOMalloc( sizeof( BlockServicesClientData ) );
	if ( clientData == NULL )
	{
		ERROR_LOG ( ( "IOUFIStorageServices: doAsyncReadWrite; clientData malloc failed!\n" ) );
		return kIOReturnNoResources;
	}

	// Make sure we don't go away while the command is being executed.
	retain();
	fProvider->retain();	

	requestBlockSize = fProvider->ReportMediumBlockSize();
	
	STATUS_LOG ( ( "IOUFIStorageServices: doAsyncReadWrite; save completion data!\n" ) );

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
	
	requestStatus = fProvider->AsyncReadWrite( buffer, (UInt64) block, (UInt64) nblks, (UInt64) requestBlockSize, (void *) clientData );
	if( requestStatus != kIOReturnSuccess )
	{
		if( clientData != NULL )
		{
			IOFree( clientData, sizeof( BlockServicesClientData ) );
		}
	}
	
	return requestStatus;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doSyncReadWrite												   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::doSyncReadWrite(		IOMemoryDescriptor * 	buffer,
											UInt32 					block,
											UInt32 					nblks )
{
	IOReturn	result;
	
	// Return errors for incoming I/O if we have been terminated
	if ( isInactive() != false )
	{
		return kIOReturnNotAttached;
	}
	
	// Make sure we don't go away while the command in being executed.
	retain();
	fProvider->retain();

	// Execute the command
	result = fProvider->SyncReadWrite( buffer, block, nblks, fProvider->ReportMediumBlockSize() );
	
	// Release the retains for this command.	
	fProvider->release();
	release();
	
	return result;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doEjectMedia													   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::doEjectMedia( void )
{
	IOReturn	result;
	
	fMediaPresent = false;
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive() != false )
	{
		return kIOReturnNotAttached;
	}
	
	// Make sure we don't go away while the command in being executed.
	retain();
	fProvider->retain();
	
	// Execute the command
	result = fProvider->EjectTheMedium();
	
	// Release the retains for this command.	
	fProvider->release();
	release();
	
	return result;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doFormatMedia													   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::doFormatMedia( UInt64 byteCapacity )
{
	IOReturn result;
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive() != false )
	{
		return kIOReturnNotAttached;
	}
	
	// Make sure we don't go away while the command in being executed.
	retain();
	fProvider->retain();	

	// Execute the command
	result = fProvider->FormatMedium( byteCapacity / ( fProvider->ReportMediumBlockSize() ),
									   fProvider->ReportMediumBlockSize() );
	
	// Release the retains for this command.	
	fProvider->release();
	release();
	
	return result;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doGetFormatCapacities											   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOUFIStorageServices::doGetFormatCapacities(	UInt64 * capacities,
												UInt32   capacitiesMaxCount ) const
{
	IOReturn result;
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive() != false )
	{
		return kIOReturnNotAttached;
	}
	
	// Make sure we don't go away while the command in being executed.
	retain();
	fProvider->retain();	
	
	// Execute the command
	result = fProvider->GetFormatCapacities ( capacities, capacitiesMaxCount );
	
	// Release the retains for this command.	
	fProvider->release();
	release();
	
	return result;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doLockUnlockMedia												   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::doLockUnlockMedia ( bool doLock )
{
	// Return errors for incoming activity if we have been terminated
	if ( isInactive() != false )
	{
		return kIOReturnNotAttached;
	}
	
	return kIOReturnSuccess;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doSynchronizeCache											   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::doSynchronizeCache ( void )
{
	// Return errors for incoming activity if we have been terminated
	if ( isInactive() != false )
	{
		return kIOReturnNotAttached;
	}
	
	return kIOReturnSuccess;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getVendorString												   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOUFIStorageServices::getVendorString ( void )
{
	return fProvider->GetVendorString ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getProductString												   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOUFIStorageServices::getProductString ( void )
{
	return fProvider->GetProductString ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getRevisionString												   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOUFIStorageServices::getRevisionString ( void )
{
	return fProvider->GetRevisionString ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getAdditionalDeviceInfoString									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOUFIStorageServices::getAdditionalDeviceInfoString ( void )
{
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	return ( "No Additional Device Info" );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportBlockSize												   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::reportBlockSize ( UInt64 * blockSize )
{
	*blockSize = fProvider->ReportMediumBlockSize ( );
	return kIOReturnSuccess;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportEjectability											   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::reportEjectability ( bool * isEjectable )
{
	*isEjectable = true;
	return kIOReturnSuccess;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportLockability												   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::reportLockability ( bool * isLockable )
{
	*isLockable = true;
	return kIOReturnSuccess;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportPollRequirements										   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::reportPollRequirements ( 	bool * pollIsRequired,
													bool * pollIsExpensive )
{
	*pollIsRequired 	= false;
	*pollIsExpensive 	= false;
	
	return kIOReturnSuccess;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxReadTransfer											   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::reportMaxReadTransfer ( UInt64 		blockSize,
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxValidBlock											   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::reportMaxValidBlock ( UInt64 * maxBlock )
{
	*maxBlock = ( fProvider->ReportMediumTotalBlockCount ( ) - 1 );
	return kIOReturnSuccess;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxWriteTransfer										   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::reportMaxWriteTransfer ( 	UInt64		blockSize,
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMediaState												   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::reportMediaState ( 	bool * mediaPresent,
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportRemovability											   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::reportRemovability ( bool * isRemovable )
{
	*isRemovable = true;
	return kIOReturnSuccess;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportWriteProtection											   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUFIStorageServices::reportWriteProtection ( bool * isWriteProtected )
{
	*isWriteProtected = fProvider->ReportMediumWriteProtection();
	return kIOReturnSuccess;
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOUFIStorageServices, 1 );
OSMetaClassDefineReservedUnused( IOUFIStorageServices, 2 );
OSMetaClassDefineReservedUnused( IOUFIStorageServices, 3 );
OSMetaClassDefineReservedUnused( IOUFIStorageServices, 4 );
OSMetaClassDefineReservedUnused( IOUFIStorageServices, 5 );
OSMetaClassDefineReservedUnused( IOUFIStorageServices, 6 );
OSMetaClassDefineReservedUnused( IOUFIStorageServices, 7 );
OSMetaClassDefineReservedUnused( IOUFIStorageServices, 8 );

//--------------------------------------------------------------------------------------
//							End				Of				File
//--------------------------------------------------------------------------------------
