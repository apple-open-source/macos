/*
 * Copyright (c) 1998-2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include "IOUFIStorageServices.h"

//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#if ( USB_MASS_STORAGE_DEBUG == 1 )
#define PANIC_NOW(x)		IOPanic x
// Override the debug level for USBLog to make sure our logs make it out and then import
// the logging header.
#define DEBUG_LEVEL			1
#include <IOKit/usb/IOUSBLog.h>
#define STATUS_LOG(x)		USBLog x
#else
#define PANIC_NOW(x)
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


//-----------------------------------------------------------------------------
//	- attach - attach to provider.									[PROTECTED]
//-----------------------------------------------------------------------------

bool
IOUFIStorageServices::attach( IOService * provider )
{
	STATUS_LOG(( 6, "%s[%p]:: attach called", getName(), this ));
	
	if ( !super::attach ( provider ) )
	{
		return false;
	}
	
	fProvider = OSDynamicCast ( IOUSBMassStorageUFIDevice, provider );
	if ( fProvider == NULL )
	{
		STATUS_LOG(( 1, "%s[%p]:: attach; wrong provider type!", getName(), this ));
		return false;
	}
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey, fProvider->GetProtocolCharacteristicsDictionary ( ) );
	setProperty ( kIOPropertyDeviceCharacteristicsKey, fProvider->GetDeviceCharacteristicsDictionary ( ) );
	
	fMediaChanged			= false;
	fMediaPresent			= false;
	
	fMaxReadBlocks 	= fProvider->ReportDeviceMaxBlocksReadTransfer();
	fMaxWriteBlocks = fProvider->ReportDeviceMaxBlocksWriteTransfer();
	
	STATUS_LOG(( 6, "%s[%p]:: attach exiting", getName(), this ));
	return true;
}


//-----------------------------------------------------------------------------
//	- detach - detach from provider.								[PROTECTED]
//-----------------------------------------------------------------------------

void
IOUFIStorageServices::detach( IOService * provider )
{
	STATUS_LOG(( 6, "%s[%p]: detach called", getName(), this ));
		
	super::detach( provider );

	STATUS_LOG(( 6, "%s[%p]: detach exiting", getName(), this ));
}


//-----------------------------------------------------------------------------
//	- message - handles messages.									   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
IOUFIStorageServices::message( 	UInt32 			type,
								IOService *		nub,
								void *			arg )
{
	IOReturn 	status = kIOReturnSuccess;
	
	STATUS_LOG(( 6, "%s[%p]::message called", getName(), this ));
		
	switch ( type )
	{
		case kIOMessageMediaStateHasChanged:
		{
			STATUS_LOG(( 5, "%s[%p]:: type = kIOMessageMediaStateHasChanged, nub = %p", getName(), this, nub ));
			
			fMediaChanged	= true;
			fMediaPresent	= true;
			status = messageClients ( type, arg, sizeof ( IOMediaState ) );
			STATUS_LOG(( 5, "%s[%p]:: status = %ld", getName(), this, ( UInt32 ) status ));
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


//-----------------------------------------------------------------------------
//	- AsyncReadWriteComplete - Completion routine for I/O	  [STATIC][PRIVATE]
//-----------------------------------------------------------------------------

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

	STATUS_LOG(( 5, "%s[%p]:: AsyncReadWriteComplete; command status %x", owner->getName(), owner, status ));
	// Check to see if an error occurred that on which the request should be retried.
	if ((( status != kIOReturnNotAttached ) && ( status != kIOReturnOffline ) &&
		( status != kIOReturnSuccess )) && ( servicesData->retriesLeft > 0 ))
	{
		IOReturn 	requestStatus;

		STATUS_LOG((5, "%s[%p]:: AsyncReadWriteComplete; retry command", owner->getName(), owner ));
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


//-----------------------------------------------------------------------------
//	- doAsyncReadWrite												   [PUBLIC]
//-----------------------------------------------------------------------------

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
		STATUS_LOG(( 1, "%s[%p]:: doAsyncReadWrite; clientData malloc failed!", getName(), this ));
		return kIOReturnNoResources;
	}

	// Make sure we don't go away while the command is being executed.
	retain();
	fProvider->retain();	

	requestBlockSize = fProvider->ReportMediumBlockSize();
	
	STATUS_LOG(( 5, "%s[%p]:: doAsyncReadWrite; save completion data!", getName(), this ));

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


//-----------------------------------------------------------------------------
//	- doSyncReadWrite												   [PUBLIC]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	- doEjectMedia													   [PUBLIC]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	- doFormatMedia													   [PUBLIC]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	- doGetFormatCapacities											   [PUBLIC]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	- doLockUnlockMedia												   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
IOUFIStorageServices::doLockUnlockMedia ( bool doLock )
{
	UNUSED( doLock );
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive() != false )
	{
		return kIOReturnNotAttached;
	}
	
	return kIOReturnSuccess;
}


//-----------------------------------------------------------------------------
//	- doSynchronizeCache											   [PUBLIC]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	- getVendorString												   [PUBLIC]
//-----------------------------------------------------------------------------

char *
IOUFIStorageServices::getVendorString ( void )
{
	return fProvider->GetVendorString ( );
}


//-----------------------------------------------------------------------------
//	- getProductString												   [PUBLIC]
//-----------------------------------------------------------------------------

char *
IOUFIStorageServices::getProductString ( void )
{
	return fProvider->GetProductString ( );
}


//-----------------------------------------------------------------------------
//	- getRevisionString												   [PUBLIC]
//-----------------------------------------------------------------------------

char *
IOUFIStorageServices::getRevisionString ( void )
{
	return fProvider->GetRevisionString ( );
}


//-----------------------------------------------------------------------------
//	- getAdditionalDeviceInfoString									   [PUBLIC]
//-----------------------------------------------------------------------------

char *
IOUFIStorageServices::getAdditionalDeviceInfoString ( void )
{
	STATUS_LOG((6, "%s::%s called", getName ( ), __FUNCTION__ ) );
	return ( "No Additional Device Info" );
}


//-----------------------------------------------------------------------------
//	- reportBlockSize												   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
IOUFIStorageServices::reportBlockSize ( UInt64 * blockSize )
{
	*blockSize = fProvider->ReportMediumBlockSize ( );
	return kIOReturnSuccess;
}


//-----------------------------------------------------------------------------
//	- reportEjectability											   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
IOUFIStorageServices::reportEjectability ( bool * isEjectable )
{
	*isEjectable = true;
	return kIOReturnSuccess;
}


//-----------------------------------------------------------------------------
//	- reportLockability												   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
IOUFIStorageServices::reportLockability ( bool * isLockable )
{
	*isLockable = true;
	return kIOReturnSuccess;
}


//-----------------------------------------------------------------------------
//	- reportPollRequirements										   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
IOUFIStorageServices::reportPollRequirements ( 	bool * pollIsRequired,
													bool * pollIsExpensive )
{
	*pollIsRequired 	= false;
	*pollIsExpensive 	= false;
	
	return kIOReturnSuccess;
}


//-----------------------------------------------------------------------------
//	- reportMaxReadTransfer											   [PUBLIC]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	- reportMaxValidBlock											   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
IOUFIStorageServices::reportMaxValidBlock ( UInt64 * maxBlock )
{
	*maxBlock = ( fProvider->ReportMediumTotalBlockCount ( ) - 1 );
	return kIOReturnSuccess;
}


//-----------------------------------------------------------------------------
//	- reportMaxWriteTransfer										   [PUBLIC]
//-----------------------------------------------------------------------------

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


//-----------------------------------------------------------------------------
//	- reportMediaState												   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
IOUFIStorageServices::reportMediaState ( 	bool * mediaPresent,
												bool * changed )	
{
    STATUS_LOG(( 6, "%s[%p]::reportMediaState.", getName(), this ));
	
	*mediaPresent 	= fMediaPresent;
	*changed 		= fMediaChanged;
	
	if ( fMediaChanged )
	{
		fMediaChanged = !fMediaChanged;
	}
	
	return kIOReturnSuccess;
}


//-----------------------------------------------------------------------------
//	- reportRemovability											   [PUBLIC]
//-----------------------------------------------------------------------------

IOReturn
IOUFIStorageServices::reportRemovability ( bool * isRemovable )
{
	*isRemovable = true;
	return kIOReturnSuccess;
}


//-----------------------------------------------------------------------------
//	- reportWriteProtection											   [PUBLIC]
//-----------------------------------------------------------------------------

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
