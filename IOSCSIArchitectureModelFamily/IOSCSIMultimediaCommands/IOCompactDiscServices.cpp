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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Libkern includes
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSDictionary.h>

// IOKit includes
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>

// Generic IOKit storage related headers
#include <IOKit/storage/IOBlockStorageDriver.h>

// SCSI Architecture Model Family includes
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"
#include "IOSCSIProtocolInterface.h"
#include "IOCompactDiscServices.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"CD Services"

#if DEBUG
#define SCSI_DVD_SERVICES_DEBUGGING_LEVEL					0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_COMPACT_DISC_SERVICES_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_COMPACT_DISC_SERVICES_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_COMPACT_DISC_SERVICES_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#if (_USE_DATA_CACHING_)
#define	_CACHE_BLOCK_COUNT_	5
#define _CACHE_BLOCK_SIZE_	2352
// Turn cache logging on and off
#if ( 0 )
#define CACHE_LOG(x)		IOLog x
#else
#define CACHE_LOG(x)
#endif
#endif


#define	super IOCDBlockStorageDevice
OSDefineMetaClassAndStructors ( IOCompactDiscServices, IOCDBlockStorageDevice );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// The command should be tried 5 times.  The original attempt 
// plus 4 retries.
#define kNumberRetries		4


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Structures
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Structure for the asynch client data
struct BlockServicesClientData
{
	// The object that owns the copy of this structure.
	IOCompactDiscServices *		owner;
	
	// The request parameters provided by the client.
	IOStorageCompletion			completionData;
	IOMemoryDescriptor * 		clientBuffer;
	UInt32 						clientStartingBlock;
	UInt32 						clientRequestedBlockCount;
	bool						clientReadCDCall;
	CDSectorArea				clientSectorArea;
	CDSectorType				clientSectorType;
	
	// The internally needed parameters.
	UInt32						retriesLeft;
	
#if (_USE_DATA_CACHING_)
	// Strutures used for the Data Cache support
	UInt8 * 					transferSegBuffer;
	IOMemoryDescriptor *		transferSegDesc;
	UInt32						transferStart;
	UInt32						transferCount;
#endif	
};
typedef struct BlockServicesClientData	BlockServicesClientData;


#if 0
#pragma mark -
#pragma mark ¥ Public Methods - API Exported to layers above
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ start - Start our services									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOCompactDiscServices::start ( IOService * provider )
{
	
	UInt32			cdFeaturesFlags = 0;
	bool			result			= false;
	
	fProvider = OSDynamicCast ( IOSCSIPeripheralDeviceType05, provider );
	require_nonzero ( fProvider, ErrorExit );
	require ( super::start ( fProvider ), ErrorExit );
	
	cdFeaturesFlags = ( ( OSNumber * ) fProvider->getProperty (
					kIOPropertySupportedCDFeatures ) )->unsigned32BitValue ( );
	
	if ( ( cdFeaturesFlags & kCDFeaturesWriteOnceMask ) ||
		 ( cdFeaturesFlags & kCDFeaturesReWriteableMask ) )
	{
		
		require ( setProperty ( kIOMatchCategoryKey,
								kSCSITaskUserClientIniterKey ), ErrorExit );
		
	}
	
#if (_USE_DATA_CACHING_)
	// Setup the data cache structures
	
	// Allocate a data cache for 5 blocks of CDDA data 
	fDataCacheStorage = ( UInt8 * ) IOMalloc ( _CACHE_BLOCK_COUNT_ * _CACHE_BLOCK_SIZE_ );
	require_nonzero ( fDataCacheStorage, ErrorExit );
	
	fDataCacheStartBlock = 0;
	fDataCacheBlockCount = 0;
	
	// Allocate the mutex for accessing the data cache.
	fDataCacheLock = IOSimpleLockAlloc ( );
	require_nonzero_action ( fDataCacheLock,
							 ErrorExit,
							 IOFree ( fDataCacheStorage,
							 		  _CACHE_BLOCK_COUNT_ * _CACHE_BLOCK_SIZE_ ) );
	
#endif
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey,
				  fProvider->GetProtocolCharacteristicsDictionary ( ) );
	setProperty ( kIOPropertyDeviceCharacteristicsKey,
				  fProvider->GetDeviceCharacteristicsDictionary ( ) );
	
	registerService ( );
	
	result = true;
	

ErrorExit:
	
	
	return false;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ open - Open the driver for business							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOCompactDiscServices::open ( IOService *		client,
							  IOOptionBits		options,
							  IOStorageAccess	access )
{
	
	// Same as IOService::open(), but with correct parameter types.
    return super::open ( client, options, ( void * ) access );
    
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ message - Handle and relay any necessary messages				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::message ( UInt32 		type,
								 IOService *	nub,
								 void *			arg )
{
	
	IOReturn 	status = kIOReturnSuccess;
	
	switch ( type )
	{
		
		case kSCSIServicesNotification_ExclusivityChanged:
		case kIOMessageMediaStateHasChanged:
		case kIOMessageTrayStateChange:
		case kIOMessageMediaAccessChange:
		{
			
			status = messageClients ( type, arg );
			
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
//	¥ setProperties - Used by autodiskmount to eject/inject the tray   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn 
IOCompactDiscServices::setProperties ( OSObject * properties )
{
	
	IOReturn		status 				= kIOReturnBadArgument;
	OSDictionary *	dict 				= NULL;
	UInt8			trayState			= 0xFF;
	Boolean			userClientActive	= false;
	
	require_nonzero ( properties, ErrorExit );
	
	dict = OSDynamicCast ( OSDictionary, properties );
	require_nonzero ( dict, ErrorExit );
	
	require_nonzero_action ( fProvider,
							 ErrorExit,
							 status = kIOReturnNotAttached );
	
	fProvider->retain ( );
	
	require_nonzero ( dict->getObject ( "TrayState" ), ReleaseProvider );
	
	// The user client is active, reject this call.
	userClientActive = fProvider->GetUserClientExclusivityState ( );
	require_action (
		( userClientActive == false ),
		ReleaseProvider,
		status = kIOReturnExclusiveAccess;
		messageClients ( kIOMessageTrayStateChange,
						 ( void * ) kMessageTrayStateChangeRequestRejected ) );
	
	fProvider->CheckPowerState ( );
	
	status = fProvider->GetTrayState ( &trayState );
	require_success ( status, ReleaseProvider );
	
	status = fProvider->SetTrayState ( !trayState );
	
	
ReleaseProvider:
	
	
	fProvider->release ( );
	
	
ErrorExit:
	
    
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doAsyncReadCD - Sends READ_CD style commands to the driver 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doAsyncReadCD ( 	IOMemoryDescriptor *	buffer,
										UInt32					block,
										UInt32					nblks,
										CDSectorArea			sectorArea,
										CDSectorType			sectorType,
										IOStorageCompletion		completion )
{
	
	BlockServicesClientData	*	clientData	= NULL;
	IOReturn					status		= kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
#if (_USE_DATA_CACHING_)
	// Only do caching if the requested sector is CDDA
	if ( sectorType == kCDSectorTypeCDDA )
	{
		
		// Check to see if all requested data is in the cache.  If it is, no need to send a command
		// to the device.
		IOSimpleLockLock ( fDataCacheLock );
		if ( fDataCacheBlockCount != 0 )
		{
			
			// Check to see if this request could possibly be fulfilled by the data
			// that is currently in the cache.
			// This is possible if the following conditions apply:
			// 1. The request is the same or smaller than the number of blocks that currently
			// resides in the cache.
			// 2. The starting request block is greater than the starting block of the cache.
			// 3. The ending request block is the same or less than the end block in the cache.
			if ( ( nblks <= fDataCacheBlockCount ) && ( block >= fDataCacheStartBlock ) && 
				( block + nblks <= fDataCacheStartBlock + fDataCacheBlockCount ) )
			{
				
				UInt32		startByte = ( block - fDataCacheStartBlock ) * _CACHE_BLOCK_SIZE_;
				
				// All the data for the request is in the cache, complete the request now.
				buffer->writeBytes ( 0, &fDataCacheStorage[startByte], ( nblks * _CACHE_BLOCK_SIZE_ ) );
				
				// Release the lock for the Queue
				IOSimpleLockUnlock ( fDataCacheLock );
				
				// Call the client's completion
				IOStorage::complete ( completion, kIOReturnSuccess, ( nblks * _CACHE_BLOCK_SIZE_ ) );
				
				// Return the status
				status = kIOReturnSuccess;
				goto Exit;
				
			}
			
		}
		
		// Release the lock for the Queue
		IOSimpleLockUnlock ( fDataCacheLock );
		
	}
#endif
	
	clientData = IONew ( BlockServicesClientData, 1 );
	require_nonzero_action ( clientData, ErrorExit, status = kIOReturnNoResources );
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	// Set the owner of this request.
	clientData->owner 						= this;
	
	// Save the client's request parameters.
	clientData->completionData 				= completion;
	clientData->clientBuffer 				= buffer;
	clientData->clientStartingBlock 		= block;
	clientData->clientRequestedBlockCount 	= nblks;
	clientData->clientReadCDCall			= true;
	clientData->clientSectorArea			= sectorArea;
	clientData->clientSectorType			= sectorType;
	
	// Set the retry limit to the maximum
	clientData->retriesLeft 				= kNumberRetries;
	
	fProvider->CheckPowerState ( );

#if (_USE_DATA_CACHING_)
	// Only do caching if the requested sector is CDDA
	if ( sectorType == kCDSectorTypeCDDA )
	{
		
		// Allocate a buffer before grabbing the lock.  Use the size of the complete request to
		// guarantee that it is large enough.
		clientData->transferSegBuffer = ( UInt8 * ) IOMalloc ( nblks * _CACHE_BLOCK_SIZE_ );
		if ( clientData->transferSegBuffer != NULL )
		{
			
			IOSimpleLockLock ( fDataCacheLock );
			
			// Determine what data can be used from the cache
			if ( ( fDataCacheBlockCount != 0 ) &&			// If the cache has valid data,
				 ( block > fDataCacheStartBlock ) && 		// and the starting block is the same or greater than the cache start
				 ( nblks > fDataCacheBlockCount ) && 		// and the block count is the same or greater than the cache count
				 ( block < ( fDataCacheStartBlock + fDataCacheBlockCount ) ) ) // and the starting block is not beyond the end of the cache
			{
				
				UInt32 offsetBlk;
				
				// There is data in the cache of interest, figure out amount from cache and amount to transfer.
				
				CACHE_LOG ( ( "IOCompactDiscServices::doAsyncReadCD called, block = %ld\n", block ) );
				CACHE_LOG ( ( "IOCompactDiscServices::doAsyncReadCD called, nblks = %ld\n", nblks ) );
				CACHE_LOG ( ( "IOCompactDiscServices::doAsyncReadCD called, fDataCacheStartBlock = %ld\n", fDataCacheStartBlock ) );
				CACHE_LOG ( ( "IOCompactDiscServices::doAsyncReadCD called, fDataCacheBlockCount = %ld\n", fDataCacheBlockCount ) );

				// Calculate the starting position in the cache
				offsetBlk = ( block - fDataCacheStartBlock );
				CACHE_LOG ( ( "IOCompactDiscServices::doAsyncReadCD called, offsetBlk = %ld\n", offsetBlk ) );
				
				// Calculate number of blocks that needs to come from disc
				clientData->transferCount = nblks - ( fDataCacheBlockCount - offsetBlk );
				CACHE_LOG ( ( "IOCompactDiscServices::doAsyncReadCD called, clientData->transferCount = %ld\n", clientData->transferCount ) );
				
				// Calculate starting block to read from disc
				clientData->transferStart = block + ( fDataCacheBlockCount - offsetBlk );
				CACHE_LOG ( ( "IOCompactDiscServices::doAsyncReadCD called, clientData->transferStart = %ld\n", clientData->transferStart ) );
							
				// Create memory descriptor for transfer buffer.
				clientData->transferSegDesc = IOMemoryDescriptor::withAddress ( clientData->transferSegBuffer,
															clientData->transferCount * _CACHE_BLOCK_SIZE_,
															kIODirectionIn );
				
				if ( clientData->transferSegDesc != NULL )
				{
					
					// Copy data from cache into client's buffer
					buffer->writeBytes ( 0, &fDataCacheStorage[offsetBlk * _CACHE_BLOCK_SIZE_], ( ( fDataCacheBlockCount - offsetBlk ) * _CACHE_BLOCK_SIZE_ ) );
					
					// Release the lock for the Queue
					IOSimpleLockUnlock ( fDataCacheLock );
					status = fProvider->AsyncReadCD ( clientData->transferSegDesc,
													clientData->transferStart,
													clientData->transferCount,
													sectorArea,
													sectorType,
													( void * ) clientData );
					goto Exit;
					
				}
				
				CACHE_LOG ( ( "IOCompactDiscServices::doAsyncReadCD called, transferSegDesc = NULL\n" ) );
				
			}
			
			CACHE_LOG ( ( "IOCompactDiscServices::doAsyncReadCD called, transferSegBuffer = NULL\n" ) );
			
			// Release the lock for the data cache
			IOSimpleLockUnlock ( fDataCacheLock );
			
		}
		
		if ( clientData->transferSegBuffer != NULL )
		{
			
			// If memory was allocated for the transfer, release it now since it is not needed.	
			IOFree ( clientData->transferSegBuffer, ( nblks * _CACHE_BLOCK_SIZE_ ) );
			
		}
		
	}
	
	// Make sure that this is cleared out to
	// avoid doing any cache operations in the completion.
	clientData->transferSegBuffer 	= NULL;
	clientData->transferSegDesc 	= NULL;
	clientData->transferStart 		= block;
	clientData->transferCount		= nblks;
#endif
	
	status = fProvider->AsyncReadCD ( buffer,
									  block,
									  nblks,
									  sectorArea,
									  sectorType,
									  ( void * ) clientData );
	
	
ErrorExit:
Exit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doAsyncReadWrite - Sends an asynchronous I/O to the driver	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doAsyncReadWrite ( IOMemoryDescriptor *		buffer,
										  UInt32					block,
										  UInt32					nblks,
										  IOStorageCompletion		completion )
{
	
	BlockServicesClientData	*	clientData 	= NULL;
	IOReturn					status		= kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	clientData = IONew ( BlockServicesClientData, 1 );
	require_nonzero_action ( clientData, ErrorExit, status = kIOReturnNoResources );
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );

	STATUS_LOG ( ( "IOCompactDiscServices: doAsyncReadWrite; save completion data!\n" ) );

	// Set the owner of this request.
	clientData->owner 						= this;
	
	// Save the client's request parameters.
	clientData->completionData 				= completion;
	clientData->clientBuffer 				= buffer;
	clientData->clientStartingBlock 		= block;
	clientData->clientRequestedBlockCount 	= nblks;
	clientData->clientReadCDCall 			= false;
	
	// Set the retry limit to the maximum
	clientData->retriesLeft 				= kNumberRetries;

	fProvider->CheckPowerState ( );

#if (_USE_DATA_CACHING_)
	// Make sure that this is cleared out to
	// avoid doing any cache operations in the completion.
	clientData->transferSegBuffer	= NULL;
	clientData->transferSegDesc 	= NULL;
	clientData->transferStart		= 0;
	clientData->transferCount		= 0;
#endif
	
	status = fProvider->AsyncReadWrite ( buffer, block, nblks, ( void * ) clientData );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doSyncReadWrite - Sends a synchronous I/O to the driver	  	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doSyncReadWrite ( IOMemoryDescriptor *	buffer,
										 UInt32					block,
										 UInt32					nblks )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	fProvider->CheckPowerState ( );		
	
	// Execute the command
	status = fProvider->SyncReadWrite ( buffer, block, nblks );
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doFormatMedia - Sends a format media request to the driver  	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doFormatMedia ( UInt64 byteCapacity )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	status = fProvider->FormatMedia ( byteCapacity );
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
		
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doGetFormatCapacities - 	Sends a get format capacities request to
//								the driver 						 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOCompactDiscServices::doGetFormatCapacities ( UInt64 *	capacities,
											   UInt32	capacitiesMaxCount ) const
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );

	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	fProvider->CheckPowerState ( );

	// Execute the command
	status = fProvider->GetFormatCapacities ( capacities, capacitiesMaxCount );

	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
		
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doEjectMedia - 	Sends an eject media request to the driver 	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doEjectMedia ( void )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
#if (_USE_DATA_CACHING_)
	// We got an eject call, invalidate the data cache
	fDataCacheStartBlock = 0;
	fDataCacheBlockCount = 0;
#endif

	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	status = fProvider->EjectTheMedia ( );
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doLockUnlockMedia - Sends an (un)lock media request to the driver
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doLockUnlockMedia ( bool doLock )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );
	
	fProvider->CheckPowerState ( );
	
	// Execute the command
	status = fProvider->LockUnlockMedia ( doLock );
	
	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
		
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getVendorString - Returns the vendor string					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOCompactDiscServices::getVendorString ( void )
{
	
	return fProvider->GetVendorString ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getProductString - Returns the product string					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOCompactDiscServices::getProductString ( void )
{
	
	return fProvider->GetProductString ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getRevisionString - Returns the product revision level string	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOCompactDiscServices::getRevisionString ( void )
{

	return fProvider->GetRevisionString ( );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getAdditionalDeviceInfoString - Returns nothing				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOCompactDiscServices::getAdditionalDeviceInfoString ( void )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	return ( "No Additional Device Info" );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportBlockSize - Reports media block size					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportBlockSize ( UInt64 * blockSize )
{
	
	return fProvider->ReportBlockSize ( blockSize );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportEjectability - Reports media ejectability characteristic   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportEjectability ( bool * isEjectable )
{
	
	return fProvider->ReportEjectability ( isEjectable );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportLockability - Reports media lockability characteristic	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportLockability ( bool * isLockable )
{
	
	return fProvider->ReportLockability ( isLockable );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMediaState - Reports media state						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportMediaState ( bool * mediaPresent,
										  bool * changed )    
{
	
	return fProvider->ReportMediaState ( mediaPresent, changed );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportPollRequirements - Reports polling requirements			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportPollRequirements (	bool * pollIsRequired,
												bool * pollIsExpensive )
{
	
	return fProvider->ReportPollRequirements ( pollIsRequired, pollIsExpensive );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxReadTransfer - Reports maximum read transfer size *OBSOLETE*
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportMaxReadTransfer ( 	UInt64   blockSize,
												UInt64 * max )
{
	
	return fProvider->ReportMaxReadTransfer ( blockSize, max );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxValidBlock - Reports maximum valid block on media	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportMaxValidBlock ( UInt64 * maxBlock )
{
	
	return fProvider->ReportMaxValidBlock ( maxBlock );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportRemovability -	Reports removability characteristic of the
//							media									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportRemovability ( bool * isRemovable )
{
	
	return fProvider->ReportRemovability ( isRemovable );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readISRC - Reads the ISRC code from the media					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readISRC ( UInt8 track, CDISRC isrc )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadISRC ( track, isrc );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readMCN - Reads the MCN code from the media					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readMCN ( CDMCN mcn )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadMCN ( mcn );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readTOC - Reads the TOC from the media	*OBSOLETE*			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readTOC ( IOMemoryDescriptor * buffer )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadTOC ( buffer );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readTOC - Reads the TOC from the media						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readTOC ( IOMemoryDescriptor * 		buffer,
								 CDTOCFormat				format,
								 UInt8						msf,
								 UInt8						trackSessionNumber,
								 UInt16 *					actualByteCount )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadTOC ( buffer,
								  format,
								  msf,
								  trackSessionNumber,
								  actualByteCount );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readDiscInfo - Reads the disc info from the media				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readDiscInfo ( IOMemoryDescriptor * 	buffer,
									  UInt16 *				actualByteCount )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadDiscInfo ( buffer, actualByteCount );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ readTrackInfo - Reads the track info from the media			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::readTrackInfo ( IOMemoryDescriptor *		buffer,
									   UInt32					address,
									   CDTrackInfoAddressType	addressType,
									   UInt16 *					actualByteCount )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->ReadTrackInfo ( buffer,
										address,
										addressType,
										actualByteCount );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ audioPause - Pauses audio playback				*OBSOLETE*	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::audioPause ( bool pause )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->AudioPause ( pause );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ audioPlay - Starts audio playback				*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::audioPlay ( CDMSF timeStart, CDMSF timeStop )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->AudioPlay ( timeStart, timeStop );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ audioScan - Starts audio scanning				*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::audioScan ( CDMSF timeStart, bool reverse )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->AudioScan ( timeStart, reverse );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ audioStop - Stops audio playback				*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::audioStop ( void )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->AudioStop ( );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getAudioStatus - Gets audio status			*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::getAudioStatus ( CDAudioStatus * cdAudioStatus )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->GetAudioStatus ( cdAudioStatus );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getAudioVolume - Gets audio volume			*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::getAudioVolume ( UInt8 * leftVolume,
										UInt8 * rightVolume )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->GetAudioVolume ( leftVolume, rightVolume );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ setAudioVolume - Sets audio volume			*OBSOLETE*		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::setAudioVolume ( UInt8 leftVolume, UInt8 rightVolume )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->SetAudioVolume ( leftVolume, rightVolume );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ doSynchronizeCache - Synchronizes the write cache				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::doSynchronizeCache ( void )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->SynchronizeCache ( );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportMaxWriteTransfer - Reports the maximum write transfer size [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportMaxWriteTransfer ( UInt64   blockSize,
												UInt64 * max )
{

	return fProvider->ReportMaxWriteTransfer ( blockSize, max );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ reportWriteProtection - 	Reports the write protect characteristic
//								of the media						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::reportWriteProtection ( bool * isWriteProtected )
{
	
	return fProvider->ReportWriteProtection ( isWriteProtected );

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getMediaType - Reports the media type							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOCompactDiscServices::getMediaType ( void )
{
	
	return fProvider->GetMediaType ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getSpeed - Reports the media access speed						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::getSpeed ( UInt16 * kilobytesPerSecond )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->GetMediaAccessSpeed ( kilobytesPerSecond );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ setSpeed - Sets the media access speed						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOCompactDiscServices::setSpeed ( UInt16 kilobytesPerSecond )
{
	
	IOReturn	status = kIOReturnNotAttached;
	
	require ( ( isInactive ( ) == false ), ErrorExit );
	
	retain ( );
	fProvider->retain ( );
	fProvider->CheckPowerState ( );	
	
	status = fProvider->SetMediaAccessSpeed ( kilobytesPerSecond );
	
	fProvider->release ( );
	release ( );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleOpen - Handles opens on the object						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOCompactDiscServices::handleOpen ( IOService *		client,
									IOOptionBits	options,
									void *			access )
{
	
	bool	result = false;
	
	// If this isn't a user client, pass through to superclass.
	if ( ( options & kIOSCSITaskUserClientAccessMask ) == 0 )
	{
		
		result = super::handleOpen ( client, options, access );
		goto Exit;
		
	}
	
	// It's the user client, so add it to the set
	if ( fClients == NULL )
	{
		
		fClients = OSSet::withCapacity ( 1 );
		
	}
	
	require_nonzero ( fClients, ErrorExit );
	fClients->setObject ( client );
	
	result = true;
	
	
Exit:
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleClose - Handles closes on the object					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOCompactDiscServices::handleClose ( IOService * client, IOOptionBits options )
{
	
	// If this isn't a user client, pass through to superclass.
	if ( ( options & kIOSCSITaskUserClientAccessMask ) == 0 )
	{
		
		super::handleClose ( client, options );
		
	}
	
	else
	{
		
		fClients->removeObject ( client );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ handleIsOpen - Figures out if there are any opens on this object
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOCompactDiscServices::handleIsOpen ( const IOService * client ) const
{
	
	bool	result	= false;
	
	// General case (is anybody open)
	if ( client == NULL )
	{
		
		require_nonzero ( fClients, CallSuperClassError );
		require_nonzero ( fClients->getCount ( ), CallSuperClassError );
		result = true;
		
	}
	
	else
	{
		
		// specific case (is this client open)
		require_nonzero ( fClients, CallSuperClassError );
		require ( fClients->containsObject ( client ), CallSuperClassError );
		result = true;
		
	}
	
	
	return result;
	
	
CallSuperClassError:
	
	
	result = super::handleIsOpen ( client );
	return result;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Public Static Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWriteComplete - Static read/write completion routine
//															   [STATIC][PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOCompactDiscServices::AsyncReadWriteComplete ( void * 			clientData,
                                				IOReturn		status,
                                				UInt64 			actualByteCount )
{
	
	IOCompactDiscServices *		owner;
	IOStorageCompletion			returnData;
	BlockServicesClientData *	bsClientData;
	bool						commandComplete = true;

	bsClientData = ( BlockServicesClientData * ) clientData;
	
	// Save the IOCompletion information so that it may be returned
	// to the client.
	returnData 	= bsClientData->completionData;
	owner 		= bsClientData->owner;
	
	if ( ( ( status != kIOReturnNotAttached )		&&
		   ( status != kIOReturnOffline )			&&
		   ( status != kIOReturnUnsupportedMode )	&&
		   ( status != kIOReturnNotPrivileged )		&&
		   ( status != kIOReturnSuccess ) )			&&
		   ( bsClientData->retriesLeft > 0 ) )
	{
		
		IOReturn 	requestStatus;
		
		STATUS_LOG ( ( "IOCompactDiscServices: AsyncReadWriteComplete retry\n" ) );
		// An error occurred, but it is one on which the command
		// should be retried.  Decrement the retry counter and try again.
		bsClientData->retriesLeft--;
		if ( bsClientData->clientReadCDCall == true )
		{
			
#if (_USE_DATA_CACHING_)
			
			if ( ( bsClientData->transferSegDesc != NULL ) &&
				 ( bsClientData->transferCount != 0 ) )
			{
				
				requestStatus = owner->fProvider->AsyncReadCD (
									bsClientData->transferSegDesc,
									bsClientData->transferStart,
									bsClientData->transferCount,
									bsClientData->clientSectorArea,
									bsClientData->clientSectorType,
									clientData );
				
			}
			
			else
			{
				
				requestStatus = owner->fProvider->AsyncReadCD (
									bsClientData->clientBuffer,
									bsClientData->clientStartingBlock,
									bsClientData->clientRequestedBlockCount,
									bsClientData->clientSectorArea,
									bsClientData->clientSectorType,
									clientData );
				
			}

#else	/* !_USE_DATA_CACHING_ */
			
			requestStatus = owner->fProvider->AsyncReadCD (
									bsClientData->clientBuffer,
									bsClientData->clientStartingBlock,
									bsClientData->clientRequestedBlockCount,
									bsClientData->clientSectorArea,
									bsClientData->clientSectorType,
									clientData );
			
#endif	/* _USE_DATA_CACHING_ */
		
		}
		
		else
		{
			
			requestStatus = owner->fProvider->AsyncReadWrite (
									bsClientData->clientBuffer, 
									bsClientData->clientStartingBlock, 
									bsClientData->clientRequestedBlockCount, 
									clientData );
			
		}
		
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

#if (_USE_DATA_CACHING_)
		
		// Check to see if there was a temporary transfer buffer
		if ( bsClientData->transferSegBuffer != NULL )
		{
			
			// Make sure that the transfer completed successfully.
			if ( status == kIOReturnSuccess )
			{
				
				// Copy the data from the temporary buffer into the client's
				// buffer.
				( bsClientData->clientBuffer )->writeBytes ( ( ( bsClientData->clientRequestedBlockCount - bsClientData->transferCount ) * _CACHE_BLOCK_SIZE_ ), 
						bsClientData->transferSegBuffer, ( bsClientData->transferCount * _CACHE_BLOCK_SIZE_ ) );
				
				// Make sure that the actualByteCount is updated to include the amount of data that
				// came from the cache also
				actualByteCount = ( bsClientData->clientRequestedBlockCount * _CACHE_BLOCK_SIZE_ );
				
			}
			
			( bsClientData->transferSegDesc )->release ( );
			
			// Since the buffer is the entire size of the client's requested transfer ( not just the amount transferred), 
			// make sure to release the whole allocation.
			IOFree ( bsClientData->transferSegBuffer, ( bsClientData->clientRequestedBlockCount * _CACHE_BLOCK_SIZE_ ) );
			
		}
		
		// Make sure that the transfer completed successfully.
		if ( status == kIOReturnSuccess )
		{
			
			// Check to see if this was a Read CD call for a CDDA sector and if so,
			// store cache data.
			if ( ( bsClientData->clientReadCDCall == true ) && ( bsClientData->clientSectorType == kCDSectorTypeCDDA ) )
			{
				
				// Save the last blocks into the data cache
				IOSimpleLockLock ( owner->fDataCacheLock );
				
				// Check if the number of blocks tranferred is larger or equal to that
				// of the cache.  If not leave the cache alone.
				if ( bsClientData->clientRequestedBlockCount >= _CACHE_BLOCK_COUNT_ )
				{
					
					UInt32	offset;
					
					// Calculate the beginning of data to copy into cache.
					offset = ( ( bsClientData->clientRequestedBlockCount - _CACHE_BLOCK_COUNT_ ) * _CACHE_BLOCK_SIZE_ );
					( bsClientData->clientBuffer )->readBytes ( offset, owner->fDataCacheStorage, 
								( _CACHE_BLOCK_COUNT_ * _CACHE_BLOCK_SIZE_ ) );
								
					owner->fDataCacheStartBlock = bsClientData->clientStartingBlock + ( bsClientData->clientRequestedBlockCount - _CACHE_BLOCK_COUNT_ );
					owner->fDataCacheBlockCount = _CACHE_BLOCK_COUNT_;
					
				}
				
				IOSimpleLockUnlock ( owner->fDataCacheLock );
			
			}
		
		}
#endif
		
		IODelete ( clientData, BlockServicesClientData, 1 );
		
		// Release the retain for this command.	
		owner->fProvider->release ( );
		owner->release ( );
		
		IOStorage::complete ( returnData, status, actualByteCount );
		
	}
	
}


#if 0
#pragma mark -
#pragma mark ¥ Protected Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ free - Release any memory allocated at start time				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOCompactDiscServices::free ( void )
{
	
#if (_USE_DATA_CACHING_)
	// Release the data cache structures
	if ( fDataCacheStorage != NULL )
	{
		
		IOFree ( fDataCacheStorage,
				( _CACHE_BLOCK_COUNT_ * _CACHE_BLOCK_SIZE_ ) );
		fDataCacheStorage = NULL;
		
	}
	
	fDataCacheStartBlock = 0;
	fDataCacheBlockCount = 0;
	
	if ( fDataCacheLock != NULL )
	{
		
		// Free the data cache mutex.
		IOSimpleLockFree ( fDataCacheLock );
		fDataCacheLock = NULL;
		
	}
#endif
	
    super::free ( );
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 1 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 2 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 3 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 4 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 5 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 6 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 7 );
OSMetaClassDefineReservedUnused ( IOCompactDiscServices, 8 );