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
 * IOCompactDiscServices.cpp
 *
 * This subclass implements a relay to a protocol and device-specific
 * provider.
 *
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"
#include "IOCompactDiscServices.h"


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


// The command should be tried 5 times.  The original attempt 
// plus 4 retries.
#define kNumberRetries		4

#define	super IOCDBlockStorageDevice
OSDefineMetaClassAndStructors ( IOCompactDiscServices, IOCDBlockStorageDevice );

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


bool
IOCompactDiscServices::start ( IOService * provider )
{
	
	UInt32			cdFeaturesFlags = 0;
	
	STATUS_LOG ( ( "%s: ::start\n", getName ( ) ) );

	fProvider = OSDynamicCast ( IOSCSIPeripheralDeviceType05, provider );
	if ( fProvider == NULL )
	{
		
		ERROR_LOG ( ( "IOCompactDiscServices: start; wrong provider type!\n" ) );
		return false;
		
	}
	
	fClients = NULL;
	
	if ( !super::start ( fProvider ) )
		return false;
	
	cdFeaturesFlags = ( ( OSNumber * ) fProvider->getProperty ( kIOPropertySupportedCDFeatures ) )->unsigned32BitValue ( );
	
	if ( ( cdFeaturesFlags & kCDFeaturesWriteOnceMask ) ||
		 ( cdFeaturesFlags & kCDFeaturesReWriteableMask ) )
	{
				
		if ( !setProperty ( kIOMatchCategoryKey, kSCSITaskUserClientIniterKey ) )
			goto failure;
		
	}
	
#if (_USE_DATA_CACHING_)
	// Setup the data cache structures
	
	// Allocate a data cache for 5 blocks of CDDA data 
	fDataCacheStorage = ( UInt8 * ) IOMalloc ( _CACHE_BLOCK_COUNT_ * _CACHE_BLOCK_SIZE_ );
	fDataCacheStartBlock = 0;
	fDataCacheBlockCount = 0;

	// Allocate the mutex for accessing the data cache.
	fDataCacheLock = IOSimpleLockAlloc ( );
	if ( fDataCacheLock == NULL )
	{
		PANIC_NOW ( ( "IOCompactDiscServices::start Allocate fDataCacheLock failed." ) );
	}
#endif
	
	setProperty ( kIOPropertyProtocolCharacteristicsKey, fProvider->GetProtocolCharacteristicsDictionary ( ) );
	setProperty ( kIOPropertyDeviceCharacteristicsKey, fProvider->GetDeviceCharacteristicsDictionary ( ) );
	
	registerService ( );
	
	return true;
	

failure:
	
	super::stop ( fProvider );
		
	return false;
	
}


bool
IOCompactDiscServices::open ( IOService * client, IOOptionBits options, IOStorageAccess access )
{
	
	// Same as IOService::open(), but with correct parameter types.
    return super::open ( client, options, ( void * ) access );
    
}


void
IOCompactDiscServices::free ( void )
{
	
#if (_USE_DATA_CACHING_)
	// Release the data cache structures
	if ( fDataCacheStorage != NULL )
	{
		IOFree ( fDataCacheStorage, ( _CACHE_BLOCK_COUNT_ * _CACHE_BLOCK_SIZE_ ) );
	}
	
	fDataCacheStartBlock = 0;
	fDataCacheBlockCount = 0;
	if ( fDataCacheLock != NULL )
	{
		// Free the data cache mutex.
		IOSimpleLockFree( fDataCacheLock );
		fDataCacheLock = NULL;
	}
#endif
	
    super::free ( );

}


//---------------------------------------------------------------------------

IOReturn
IOCompactDiscServices::message ( UInt32 		type,
								 IOService *	nub,
								 void *			arg )
{
	
	IOReturn 	status = kIOReturnSuccess;
	
	ERROR_LOG ( ( "IOCompactDiscServices::message called\n" ) );
		
	switch ( type )
	{
		
		case kIOMessageMediaStateHasChanged:
		{
			
			ERROR_LOG ( ( "type = kIOMessageMediaStateHasChanged, nub = %p\n", nub ) );
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


//---------------------------------------------------------------------------
// set registry property

IOReturn 
IOCompactDiscServices::setProperties ( OSObject * properties )
{
	
	IOReturn		status 		= kIOReturnSuccess;
	OSDictionary *	dict 		= OSDynamicCast ( OSDictionary, properties );
	UInt8			trayState	= 0xFF;
	
	STATUS_LOG ( ( "IOCompactDiscServices: setProperties called\n" ) );
	
	if ( dict == NULL )
	{
		return kIOReturnBadArgument;
	}
	
	if ( dict->getObject ( "TrayState" ) != NULL )
	{
		
		STATUS_LOG ( ( "IOCompactDiscServices: setProperties TrayState\n" ) );
		status = fProvider->GetTrayState ( &trayState );
		if ( status == kIOReturnSuccess )
		{
			
			status = fProvider->SetTrayState ( !trayState );
			
		}
		
	}
	
	else
	{
		status = kIOReturnBadArgument;
	}
	
	STATUS_LOG ( ( "IOCompactDiscServices: leave setProperties\n" ) );
    
	return status;
	
}

//---------------------------------------------------------------------------

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
	
	if ( ( ( status != kIOReturnNotAttached ) && ( status != kIOReturnOffline ) &&
		 ( status != kIOReturnUnsupportedMode ) && ( status != kIOReturnSuccess ) ) &&
		 ( bsClientData->retriesLeft > 0 ) )
	{
		
		IOReturn 	requestStatus;
		
		STATUS_LOG ( ( "IOBlockStorageServices: AsyncReadWriteComplete; retry command\n" ) );
		// An error occurred, but it is one on which the command should be retried.  Decrement
		// the retry counter and try again.
		bsClientData->retriesLeft--;
		if ( bsClientData->clientReadCDCall == true )
		{
		
#if (_USE_DATA_CACHING_)
			requestStatus = owner->fProvider->AsyncReadCD ( 
											bsClientData->transferSegDesc,
											bsClientData->transferStart,
											bsClientData->transferCount,
											bsClientData->clientSectorArea,
											bsClientData->clientSectorType,
											clientData );
#else
			requestStatus = owner->fProvider->AsyncReadCD (
											bsClientData->clientBuffer, 
											bsClientData->clientStartingBlock, 
											bsClientData->clientRequestedBlockCount, 
											bsClientData->clientSectorArea,
											bsClientData->clientSectorType,
											clientData );
#endif
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
		
		IOFree ( clientData, sizeof ( BlockServicesClientData ) );
		
		// Release the retain for this command.	
		owner->fProvider->release ( );
		owner->release ( );
		
		IOStorage::complete ( returnData, status, actualByteCount );
		
	}
	
}


//---------------------------------------------------------------------------
// doAsyncReadCD

IOReturn
IOCompactDiscServices::doAsyncReadCD ( 	IOMemoryDescriptor *	buffer,
										UInt32					block,
										UInt32					nblks,
										CDSectorArea			sectorArea,
										CDSectorType			sectorType,
										IOStorageCompletion		completion )
{
	
	BlockServicesClientData	* clientData;
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

#if (_USE_DATA_CACHING_)
	// Only do caching if the requested sector is CDDA
	if ( sectorType == kCDSectorTypeCDDA )
	{
		// Check to see if all requested data is in the cache.  If it is, no need to send a command
		// to the device.
		IOSimpleLockLock( fDataCacheLock );
		if ( fDataCacheBlockCount != 0 )
		{
			// Check to see if this request could possibly fulfilled by the data
			// that is currently in the cache.
			// This is possible if the following conditions appply:
			// 1. The request is the same or smaller than the number of blocks that currently
			// resides in the cache.
			// 2. The starting request block is greater than the startiing block of the cache.
			// 3. The ending request block is the same or less than the end block in the cache.
			if (( nblks <= fDataCacheBlockCount ) && ( block >= fDataCacheStartBlock ) && 
				( block + nblks <= fDataCacheStartBlock + fDataCacheBlockCount))
			{
				UInt32		startByte = (block - fDataCacheStartBlock) * _CACHE_BLOCK_SIZE_;
				
				// All the data for the request is in the cache, complete the request now.
				buffer->writeBytes( 0, &fDataCacheStorage[startByte], (nblks * _CACHE_BLOCK_SIZE_) );
				
				// Release the lock for the Queue
				IOSimpleLockUnlock( fDataCacheLock );
				
				// Call the client's completion
				IOStorage::complete( completion, kIOReturnSuccess, (nblks * _CACHE_BLOCK_SIZE_) );
				
				// Return the status
				return kIOReturnSuccess;
			}
		}

		// Release the lock for the Queue
		IOSimpleLockUnlock( fDataCacheLock );
	}
#endif

	clientData = ( BlockServicesClientData * ) IOMalloc ( sizeof ( BlockServicesClientData ) );
	if ( clientData == NULL )
	{
		
		ERROR_LOG ( ( "IOCompactDiscServices: doAsyncReadCD; clientData malloc failed!\n" ) );
		return kIOReturnNoResources;
		
	}

	// Make sure we don't go away while the command in being executed.
	retain ( );
	fProvider->retain ( );

	STATUS_LOG ( ( "IOCompactDiscServices: doAsyncReadCD; save completion data!\n" ) );

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
					return fProvider->AsyncReadCD ( clientData->transferSegDesc,
													clientData->transferStart,
													clientData->transferCount,
													sectorArea,
													sectorType,
													( void * ) clientData );
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
	clientData->transferStart 		= 0;
	clientData->transferCount		= 0;
#endif

	return fProvider->AsyncReadCD ( buffer,
									block,
									nblks,
									sectorArea,
									sectorType,
									( void * ) clientData );
	
}

//---------------------------------------------------------------------------
// doAsyncReadWrite

IOReturn
IOCompactDiscServices::doAsyncReadWrite (	IOMemoryDescriptor *	buffer,
											UInt32					block,
											UInt32					nblks,
											IOStorageCompletion		completion )
{
	
	BlockServicesClientData	*	clientData;
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	if ( buffer->getDirection ( ) == kIODirectionOut )
	{
		return kIOReturnUnsupported;
	}
	
	clientData = ( BlockServicesClientData * ) IOMalloc ( sizeof ( BlockServicesClientData ) );
	if ( clientData == NULL )
	{
		
		ERROR_LOG ( ( "IOCompactDiscServices: doAsyncReadWrite; clientData malloc failed!\n" ) );
		return false;
		
	}

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
	clientData->transferSegBuffer = NULL;
	clientData->transferSegDesc = NULL;
	clientData->transferStart = 0;
	clientData->transferCount = 0;
#endif

	return fProvider->AsyncReadWrite ( buffer, block, nblks, ( void * ) clientData );
	
}

//---------------------------------------------------------------------------
// doSyncReadWrite

IOReturn
IOCompactDiscServices::doSyncReadWrite ( 	IOMemoryDescriptor *	buffer,
											UInt32					block,
											UInt32					nblks )
{
	
	IOReturn	result;
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}
	
	if ( buffer->getDirection ( ) == kIODirectionOut )
	{
		return kIOReturnUnsupported;
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
// doFormatMedia

IOReturn
IOCompactDiscServices::doFormatMedia ( UInt64 byteCapacity )
{
	
	IOReturn	result;
	
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
	result = fProvider->FormatMedia ( byteCapacity );

	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	return result;
	
}

//---------------------------------------------------------------------------
// doGetFormatCapacities

UInt32
IOCompactDiscServices::doGetFormatCapacities ( 	UInt64 *	capacities,
												UInt32		capacitiesMaxCount ) const
{
	
	IOReturn	result;
	
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
	result = fProvider->GetFormatCapacities ( capacities, capacitiesMaxCount );

	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	return result;
	
}

//---------------------------------------------------------------------------
// doEjectMedia

IOReturn
IOCompactDiscServices::doEjectMedia ( void )
{
	
	IOReturn	result;
	
#if (_USE_DATA_CACHING_)
	// We got an eject call, invalidate the data cache
	fDataCacheStartBlock = 0;
	fDataCacheBlockCount = 0;
#endif

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
	result = fProvider->EjectTheMedia( );

	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	return result;
	
}

//---------------------------------------------------------------------------
// doLockUnlockMedia

IOReturn
IOCompactDiscServices::doLockUnlockMedia ( bool doLock )
{
	
	IOReturn	result;
	
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
	result = fProvider->LockUnlockMedia ( doLock );

	// Release the retain for this command.	
	fProvider->release ( );
	release ( );
	
	return result;
	
}

//---------------------------------------------------------------------------
// getVendorString

char *
IOCompactDiscServices::getVendorString ( void )
{
	
	return fProvider->GetVendorString ( );
	
}

//---------------------------------------------------------------------------
// getProductString

char *
IOCompactDiscServices::getProductString ( void )
{
	
	return fProvider->GetProductString ( );
	
}

//---------------------------------------------------------------------------
// getRevisionString

char *
IOCompactDiscServices::getRevisionString ( void )
{

	return fProvider->GetRevisionString ( );

}

//---------------------------------------------------------------------------
// getAdditionalDeviceInfoString

char *
IOCompactDiscServices::getAdditionalDeviceInfoString ( void )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	return ( "No Additional Device Info" );
	
}

//---------------------------------------------------------------------------
// reportBlockSize

IOReturn
IOCompactDiscServices::reportBlockSize ( UInt64 * blockSize )
{
	
	return fProvider->ReportBlockSize ( blockSize );
	
}

//---------------------------------------------------------------------------
// reportEjectability

IOReturn
IOCompactDiscServices::reportEjectability ( bool * isEjectable )
{
	
	return fProvider->ReportEjectability ( isEjectable );
	
}

//---------------------------------------------------------------------------
// reportLockability

IOReturn
IOCompactDiscServices::reportLockability ( bool * isLockable )
{
	
	return fProvider->ReportLockability ( isLockable );
	
}

//---------------------------------------------------------------------------
// reportMediaState

IOReturn
IOCompactDiscServices::reportMediaState ( 	bool * mediaPresent,
											bool * changed )    
{
	
	return fProvider->ReportMediaState ( mediaPresent, changed );
	
}

//---------------------------------------------------------------------------
// reportPollRequirements

IOReturn
IOCompactDiscServices::reportPollRequirements (	bool * pollIsRequired,
												bool * pollIsExpensive )
{
	
	return fProvider->ReportPollRequirements ( pollIsRequired, pollIsExpensive );
	
}

//---------------------------------------------------------------------------
// reportMaxReadTransfer

IOReturn
IOCompactDiscServices::reportMaxReadTransfer ( 	UInt64   blockSize,
												UInt64 * max )
{
	
	return fProvider->ReportMaxReadTransfer ( blockSize, max );
	
}

//---------------------------------------------------------------------------
// reportMaxValidBlock

IOReturn
IOCompactDiscServices::reportMaxValidBlock ( UInt64 * maxBlock )
{
	
	return fProvider->ReportMaxValidBlock ( maxBlock );
	
}

//---------------------------------------------------------------------------
// reportRemovability

IOReturn
IOCompactDiscServices::reportRemovability ( bool * isRemovable )
{
	
	return fProvider->ReportRemovability ( isRemovable );
	
}

//---------------------------------------------------------------------------
// readISRC

IOReturn
IOCompactDiscServices::readISRC ( UInt8 track, CDISRC isrc )
{

	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );
	
	return fProvider->ReadISRC ( track, isrc );
	
}

//---------------------------------------------------------------------------
// readMCN

IOReturn
IOCompactDiscServices::readMCN ( CDMCN mcn )
{

	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );
	
	return fProvider->ReadMCN ( mcn );
	
}

//---------------------------------------------------------------------------
// readTOC

IOReturn
IOCompactDiscServices::readTOC ( IOMemoryDescriptor *buffer )
{

	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );
	
	return fProvider->ReadTOC ( buffer );
	
}

//---------------------------------------------------------------------------
// audioPause

IOReturn
IOCompactDiscServices::audioPause ( bool pause )
{

	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );

	return fProvider->AudioPause ( pause );
	
}

//---------------------------------------------------------------------------
// audioPlay

IOReturn
IOCompactDiscServices::audioPlay ( CDMSF timeStart, CDMSF timeStop )
{

	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );
	
	return fProvider->AudioPlay ( timeStart, timeStop );
	
}

//---------------------------------------------------------------------------
// audioScan

IOReturn
IOCompactDiscServices::audioScan ( CDMSF timeStart, bool reverse )
{

	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );
	
	return fProvider->AudioScan ( timeStart, reverse );
	
}

//---------------------------------------------------------------------------
// audioStop

IOReturn
IOCompactDiscServices::audioStop ( void )
{
	

	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );

	return fProvider->AudioStop ( );
	
}

//---------------------------------------------------------------------------
// getAudioStatus

IOReturn
IOCompactDiscServices::getAudioStatus ( CDAudioStatus * status )
{

	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );
	
	return fProvider->GetAudioStatus ( status );
	
}

//---------------------------------------------------------------------------
// getAudioVolume

IOReturn
IOCompactDiscServices::getAudioVolume ( UInt8 * leftVolume,
										UInt8 * rightVolume )
{

	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );
	
	return fProvider->GetAudioVolume ( leftVolume, rightVolume );
	
}

//---------------------------------------------------------------------------
// setVolume

IOReturn
IOCompactDiscServices::setAudioVolume ( UInt8 leftVolume, UInt8 rightVolume )
{

	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );
	
	return fProvider->SetAudioVolume ( leftVolume, rightVolume );
	
}

//---------------------------------------------------------------------------
// doSynchronizeCache

IOReturn
IOCompactDiscServices::doSynchronizeCache ( void )
{
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );

	return fProvider->SynchronizeCache ( );
	
}

//---------------------------------------------------------------------------
// reportMaxWriteTransfer

IOReturn
IOCompactDiscServices::reportMaxWriteTransfer ( UInt64   blockSize,
												UInt64 * max )
{

	return fProvider->ReportMaxWriteTransfer ( blockSize, max );

}

//---------------------------------------------------------------------------
// reportMaxWriteTransfer

IOReturn
IOCompactDiscServices::reportWriteProtection ( bool * isWriteProtected )
{
	
	return fProvider->ReportWriteProtection ( isWriteProtected );

}

//---------------------------------------------------------------------------
// getMediaType

UInt32
IOCompactDiscServices::getMediaType ( void )
{
	
	return fProvider->GetMediaType ( );
	
}


//---------------------------------------------------------------------------
// getSpeed

IOReturn
IOCompactDiscServices::getSpeed ( UInt16 * kilobytesPerSecond )
{
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}

	fProvider->CheckPowerState ( );
	
	return fProvider->GetMediaAccessSpeed ( kilobytesPerSecond );
	
}


//---------------------------------------------------------------------------
// setSpeed

IOReturn
IOCompactDiscServices::setSpeed ( UInt16 kilobytesPerSecond )
{
	
	// Return errors for incoming activity if we have been terminated
	if ( isInactive ( ) != false )
	{
		return kIOReturnNotAttached;
	}
	
	fProvider->CheckPowerState ( );
	
	return fProvider->SetMediaAccessSpeed ( kilobytesPerSecond );
	
}


#pragma mark -
#pragma mark UserClientSupport

bool
IOCompactDiscServices::handleOpen ( IOService * client, IOOptionBits options, void * access )
{
		
	// If this isn't a user client, pass through to superclass.
	if ( ( options & kIOSCSITaskUserClientAccessMask ) == 0 )
	{
		
		return super::handleOpen ( client, options, access );
		
	}
	
	// It's the user client, so add it to the set
	
	if ( fClients == NULL )
		fClients = OSSet::withCapacity ( 1 );
	
	if ( fClients == NULL )
		return false;
	
	fClients->setObject ( client );
	
	return true;
	
}


void
IOCompactDiscServices::handleClose ( IOService * client, IOOptionBits options )
{
	
	// If this isn't a user client, pass through to superclass.
	if ( ( options & kIOSCSITaskUserClientAccessMask ) == 0 )
		super::handleClose ( client, options );
	
	else
	{
		
		fClients->removeObject ( client );
		
	}

}



bool
IOCompactDiscServices::handleIsOpen ( const IOService * client ) const
{
	
	// General case (is anybody open)
	if ( client == NULL )
	{
		
		STATUS_LOG ( ( "IOCompactDiscServices::handleIsOpen, client is NULL\n" ) );
		
		if ( ( fClients != NULL ) && ( fClients->getCount ( ) > 0 ) )
			return true;
		
		STATUS_LOG ( ( "calling super\n" ) );
		
		return super::handleIsOpen ( client );
		
	}
	
	STATUS_LOG ( ( "IOCompactDiscServices::handleIsOpen, client = %p\n", client ) );
	
	// specific case (is this client open)
	if ( ( fClients != NULL ) && ( fClients->containsObject ( client ) ) )
		return true;
	
	STATUS_LOG ( ( "calling super\n" ) );
	
	return super::handleIsOpen ( client );
	
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOCompactDiscServices, 1 );
OSMetaClassDefineReservedUnused( IOCompactDiscServices, 2 );
OSMetaClassDefineReservedUnused( IOCompactDiscServices, 3 );
OSMetaClassDefineReservedUnused( IOCompactDiscServices, 4 );
OSMetaClassDefineReservedUnused( IOCompactDiscServices, 5 );
OSMetaClassDefineReservedUnused( IOCompactDiscServices, 6 );
OSMetaClassDefineReservedUnused( IOCompactDiscServices, 7 );
OSMetaClassDefineReservedUnused( IOCompactDiscServices, 8 );
