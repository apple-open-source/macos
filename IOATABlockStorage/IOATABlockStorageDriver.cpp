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
 * Copyright (c) 1999-2001 Apple Computer, Inc.  All rights reserved. 
 *
 * IOATABlockStorageDriver.cpp - Generic ATA disk driver.
 *
 * HISTORY
 * 		
 *		09.28.2000		CJS  	Started IOATABlockStorageDriver.cpp
 *								(ported from IOATAHDDrive.cpp)
 */

#include <IOKit/assert.h>
#include "IOATABlockStorageDriver.h"
#include "IOATABlockStorageDevice.h"
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

#if ( ATA_BLOCK_STORAGE_DRIVER_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)			IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( ATA_BLOCK_STORAGE_DRIVER_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)			IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( ATA_BLOCK_STORAGE_DRIVER_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)			IOLog x
#else
#define STATUS_LOG(x)
#endif

#if ( ATA_BLOCK_STORAGE_DRIVER_PM_DEBUGGING_LEVEL >= 1 )
#define SERIAL_STATUS_LOG(x) 	kprintf x
#else
#define SERIAL_STATUS_LOG(x)
#endif


#define	super IOService
OSDefineMetaClassAndStructors ( IOATABlockStorageDriver, IOService );

#pragma mark Public Methods


//---------------------------------------------------------------------------
// ¥ init - Doesn't do anything, just calls superclass' init.
//---------------------------------------------------------------------------

bool
IOATABlockStorageDriver::init ( OSDictionary * properties )
{
		
	// Run by our superclass
	if ( super::init ( properties ) == false )
	{
		
		return false;
	
	}
		
	return true;
	
}


//---------------------------------------------------------------------------
// ¥ start - Starts up the driver and spawn a nub.
//---------------------------------------------------------------------------

bool
IOATABlockStorageDriver::start ( IOService * provider )
{
	
	IOReturn		theErr				= kIOReturnSuccess;
	IOWorkLoop *	workLoop			= NULL;
	OSNumber *		numCommandObjects 	= NULL;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::start entering.\n" ) );
	
	fATADevice 					= NULL;
	fCommandPool				= NULL;
	fATAUnitID					= kATAInvalidDeviceID;
	fATADeviceType				= kUnknownATADeviceType;
	fATASocketType				= kUnknownSocket;
	fAPMLevel					= 0x80;
	
	// First call start() in our superclass
	if ( super::start ( provider ) == false )
		return false;
	
	// Cache our provider
	fATADevice = OSDynamicCast ( IOATADevice, provider );
	if ( fATADevice == NULL )
		return false;
		
	// Find out if the device type is ATA
	if ( fATADevice->getDeviceType ( ) != reportATADeviceType ( ) )
	{
		
		ERROR_LOG ( ( "IOATABlockStorageDriver::start exiting, not an ATA device.\n" ) );
		return false;
		
	}
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::start opening device.\n" ) );
	
	if ( fATADevice->open ( this ) == false )
		return false;
	
	// Cache the drive unit number (master/slave assignment).
	fATAUnitID 	= fATADevice->getUnitID ( );
	fATADeviceType = fATADevice->getDeviceType ( );

	STATUS_LOG ( ( "IOATABlockStorageDriver::start fATAUnitID = %d.\n", ( UInt8 ) fATAUnitID ) );
	STATUS_LOG ( ( "IOATABlockStorageDriver::start fATADeviceType is %d\n", ( UInt8 ) fATADeviceType ) );
		
	bzero ( fDeviceIdentifyData, 512 );

	fDeviceIdentifyBuffer = IOMemoryDescriptor::withAddress ( ( void * ) fDeviceIdentifyData,
																512,
																kIODirectionIn );
	
	assert ( fDeviceIdentifyBuffer != NULL );
	
	numCommandObjects 	= OSDynamicCast ( OSNumber, getProperty ( "IOCommandPoolSize" ) );
	fNumCommandObjects 	= numCommandObjects->unsigned32BitValue ( );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::start fNumCommandObjects = %ld\n", fNumCommandObjects ) );
	
	// Create an IOCommandGate (for power management support) and attach
	// this event source to the provider's workloop
	fCommandGate = IOCommandGate::commandGate ( this );
	assert ( fCommandGate != NULL );
	
	workLoop = getWorkLoop ( );
	assert ( workLoop != NULL );
	
	theErr = workLoop->addEventSource ( fCommandGate );
	assert ( theErr == kIOReturnSuccess );
	
	fCommandPool = IOCommandPool::commandPool ( this, workLoop, fNumCommandObjects );
	assert ( fCommandPool != NULL );
	
	allocateATACommandObjects ( );
	
	// Inspect the provider
	if ( inspectDevice ( fATADevice ) == false )
		return false;
	
	fCurrentPowerState 			= kIOATAPowerStateActive;
	fProposedPowerState			= kIOATAPowerStateActive;
	fNumCommandsOutstanding		= 0;
	fPowerTransitionInProgress 	= false;
	
	fPowerManagementThread = thread_call_allocate (
					( thread_call_func_t ) IOATABlockStorageDriver::sPowerManagement,
					( thread_call_param_t ) this );
					
	if ( fPowerManagementThread == NULL )
	{
		
		ERROR_LOG ( ( "thread allocation failed.\n" ) );
		return false;
		
	}
		
	// A policy-maker must make these calls to join the PM tree,
	// and to initialize its state
	PMinit ( );								// initialize power management variables
	provider->joinPMtree ( this );  		// join power management tree
	fPowerManagementInitialized = true;
	setIdleTimerPeriod ( k5Minutes );		// 5 minute inactivity timer
	makeUsable ( );
	
	if ( fSupportedFeatures & kIOATAFeaturePowerManagement )
		initForPM ( );
	
	return ( createNub ( provider ) );
	
}


//---------------------------------------------------------------------------
// ¥ finalize - Terminates all power management
//---------------------------------------------------------------------------

bool
IOATABlockStorageDriver::finalize ( IOOptionBits options )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::finalize this = %p\n", this ) );

    if ( fPowerManagementInitialized )
    {
        
        if ( fPowerTransitionInProgress )
        {
            
            // We have a race here where the thread_call could still fire!!!
            thread_call_cancel ( fPowerManagementThread );
            
            if ( fPowerTransitionInProgress )
            {
            
                acknowledgeSetPowerState ( );
                
            }
            
        }

        PMstop ( );
        fPowerTransitionInProgress 	= false;  
        fPowerManagementInitialized = false;

    }
    
    return super::finalize ( options );
	
}


//---------------------------------------------------------------------------
// ¥ stop - Stop the driver
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver::stop ( IOService * provider )
{
	
	// Call our superclass
	super::stop ( provider );
	
}


#pragma mark -
#pragma mark Protected Methods


//---------------------------------------------------------------------------
// ¥ free - Release allocated resources.
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver::free ( void )
{

	IOWorkLoop *	workLoop;
	
	if ( fCommandPool != NULL )
	{
		
		deallocateATACommandObjects ( );
		fCommandPool->release ( );
		fCommandPool = NULL;
		
	}
		
	if ( fCommandGate != NULL )
	{
		
		workLoop = getWorkLoop ( );
		if ( workLoop != NULL )
		{
		
			workLoop->removeEventSource ( fCommandGate );
		
		}
		
		fCommandGate->release ( );
		fCommandGate = NULL;
		
	}
	
	if ( fPowerManagementThread != NULL )
	{
		
		thread_call_free ( fPowerManagementThread );
		
	}

	super::free ( );
	
}


//---------------------------------------------------------------------------
// ¥ inspectDevice - Fetch information about the ATA device nub.
//---------------------------------------------------------------------------

bool
IOATABlockStorageDriver::inspectDevice ( IOATADevice * ataDevice )
{
	
	OSString *		string			= NULL;
	IOReturn		theErr			= kIOReturnSuccess;
	UInt16			tempWord		= 0;
	
	// Fetch ATA device information from the nub.
	string = OSDynamicCast ( 	OSString,
								ataDevice->getProperty ( kATAVendorPropertyKey ) );
	
	if ( string != NULL )
	{
		
		strncpy ( fModel, string->getCStringNoCopy ( ), kSizeOfATAModelString );
		fModel[kSizeOfATAModelString] = '\0';
		
	}
	
	string = OSDynamicCast ( 	OSString,
								ataDevice->getProperty ( kATARevisionPropertyKey ) );
	
	if ( string != NULL )
	{
		
		strncpy ( fRevision, string->getCStringNoCopy ( ), kSizeOfATARevisionString );
		fRevision[kSizeOfATARevisionString] = '\0';
		
	}
	
	theErr = identifyAndConfigureATADevice ( );
	
	if ( theErr != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "IOATABlockStorageDriver::inspectDevice theErr = %ld\n", ( UInt32 ) theErr ) );
		return false;
		
	}	
	
	tempWord = fDeviceIdentifyData[kATAIdentifyCommandSetSupported];
	
	if ( ( tempWord & kATASupportsPowerManagementMask ) == kATASupportsPowerManagementMask )
		fSupportedFeatures |= kIOATAFeaturePowerManagement;
	
	if ( ( tempWord & kATASupportsWriteCacheMask ) == kATASupportsWriteCacheMask )
		fSupportedFeatures |= kIOATAFeatureWriteCache;
	
	tempWord = fDeviceIdentifyData[kATAIdentifyCommandSetSupported2];
	if ( ( tempWord & kATADataIsValidMask ) == 0x4000 )
	{
		
		if ( ( tempWord & kATASupportsAdvancedPowerManagementMask ) == kATASupportsAdvancedPowerManagementMask )
			fSupportedFeatures |= kIOATAFeatureAdvancedPowerManagement;
		
		if ( ( tempWord & kATASupportsCompactFlashMask ) == kATASupportsCompactFlashMask )
			fSupportedFeatures |= kIOATAFeatureCompactFlash;
		
	}
	
	if ( fDeviceIdentifyData[kATAIdentifyDriveCapabilities] & kLBASupportedMask )
		fUseLBAAddressing = true;
	
	// Add an OSNumber property indicating the supported features.
	setProperty ( 	kIOATASupportedFeaturesKey,
					fSupportedFeatures,
					sizeof ( fSupportedFeatures ) * 8 );
	
	return true;
	
}


//---------------------------------------------------------------------------
// ¥ reportATADeviceType - Report the type of ATA device (ATA vs. ATAPI).
//---------------------------------------------------------------------------

ataDeviceType
IOATABlockStorageDriver::reportATADeviceType ( void ) const
{
	
	return kATADeviceType;
	
}


//---------------------------------------------------------------------------
// ¥ getDeviceTypeName - Returns the device type.
//---------------------------------------------------------------------------

const char *
IOATABlockStorageDriver::getDeviceTypeName ( void )
{
	
	return kIOBlockStorageDeviceTypeGeneric;
	
}


//---------------------------------------------------------------------------
// ¥ instantiateNub - 	Instantiate an ATA specific subclass of
//						IOBlockStorageDevice
//---------------------------------------------------------------------------

IOService *
IOATABlockStorageDriver::instantiateNub ( void )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::instantiateNub entering.\n" ) );
	
	IOService * nub = new IOATABlockStorageDevice;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::instantiateNub exiting nub = %p.\n", nub ) );
	
	return nub;
	
}


//---------------------------------------------------------------------------
// ¥ createNub - Returns an IOATABlockStorageDeviceNub.
//---------------------------------------------------------------------------

bool
IOATABlockStorageDriver::createNub ( IOService * provider )
{
	
	IOService *		nub = NULL;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::createNub entering.\n" ) );
	
	// Instantiate a generic hard disk nub so a generic driver
	// can match above us.
	nub = instantiateNub ( );
	if ( nub == NULL )
	{
		
		ERROR_LOG ( ( "IOATABlockStorageDriver::createNub instantiateNub() failed, returning false\n" ) );
		return false;
		
	}
	
	nub->init ( );
	
	if ( !nub->attach ( this ) )
	{
		
		// panic since the nub can't attach
		PANIC_NOW ( ( "IOATABlockStorageDriver::createNub() unable to attach nub" ) );
		return false;
		
	}
	
	nub->registerService ( );
	
	// The IORegistry retains the nub, so we can release our refcount
	// because we don't need it any more.
	nub->release ( );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::createNub exiting true.\n" ) );
	
	return true;
	
}


//---------------------------------------------------------------------------
// ¥ doAsyncReadWrite - Handles asynchronous read/write requests.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::doAsyncReadWrite (
										IOMemoryDescriptor *	buffer,
										UInt32					block,
										UInt32					nblks,
										IOStorageCompletion		completion )
{
	
	IOReturn			ret;
	IOATACommand *		cmd		= NULL;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doAsyncReadWrite entering.\n" ) );
	
	cmd = ataCommandReadWrite ( buffer, block, nblks );
	if ( cmd == NULL )
	{
		
		return kIOReturnNoMemory;
		
	}
	
	ret = asyncExecute ( cmd, completion );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doAsyncReadWrite exiting ret = %ld.\n", ( UInt32 ) ret ) );
	
	return ret;
	
}



//---------------------------------------------------------------------------
// ¥ doSyncReadWrite - Handles synchronous read/write requests.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::doSyncReadWrite ( 
										IOMemoryDescriptor *	buffer,
										UInt32					block,
										UInt32					nblks )
{

	IOReturn			ret;
	IOATACommand *		cmd 	= NULL;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doSyncReadWrite entering\n" ) );

	cmd = ataCommandReadWrite ( buffer, block, nblks );
	if ( cmd == NULL )
	{
		
		return kIOReturnNoMemory;
		
	}
	
	ret = syncExecute ( cmd );
		
	STATUS_LOG ( ( "IOATABlockStorageDriver::doSyncReadWrite exiting ret = %ld.\n", ( UInt32 ) ret ) );
	
	return ret;
	
}


//---------------------------------------------------------------------------
// ¥ doEjectMedia - Eject the media in the drive.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::doEjectMedia ( void )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doEjectMedia called.\n" ) );
	return kIOReturnUnsupported;	// No support for removable ATA devices.
	
}


//---------------------------------------------------------------------------
// ¥ doFormatMedia -	Format the media in the drive. ATA devices do not
//						support low level formatting.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::doFormatMedia ( UInt64 byteCapacity )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doFormatMedia called.\n" ) );
	return kIOReturnUnsupported;
	
}


//---------------------------------------------------------------------------
// ¥ doGetFormatCapacities - Returns disk capacity.
//---------------------------------------------------------------------------

UInt32
IOATABlockStorageDriver::doGetFormatCapacities ( UInt64 * 	capacities,
												 UInt32		capacitiesMaxCount ) const
{
		
	if ( ( capacities != NULL ) && ( capacitiesMaxCount > 0 ) )
	{
		
		if ( fUseLBAAddressing )
		{
			
			*capacities = ( fDeviceIdentifyData[kATAIdentifyLBACapacity + 1] << 16 ) +
								( UInt32 ) fDeviceIdentifyData[kATAIdentifyLBACapacity];	
			
		}
		
		else
		{
		
			*capacities = 	fDeviceIdentifyData[kATAIdentifySectorsPerTrack] *
							fDeviceIdentifyData[kATAIdentifyLogicalHeadCount] *
							fDeviceIdentifyData[kATAIdentifyLogicalCylinderCount];
		
		}
		
		*capacities *= kATADefaultSectorSize;
				
		return 1;
	
	}
	
	return 0;
	
}


//---------------------------------------------------------------------------
// ¥ doLockUnlockMedia - Lock the media and prevent a user-initiated eject.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::doLockUnlockMedia ( bool doLock )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doLockUnlockMedia called.\n" ) );
	return kIOReturnUnsupported;	// No removable ATA device support.
	
}


//---------------------------------------------------------------------------
// ¥ doSynchronizeCache - Flush the write-cache to the physical media
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::doSynchronizeCache ( void )
{
	
	IOReturn			status 	= kIOReturnSuccess;
	IOATACommand *		cmd 	= NULL;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doSynchronizeCache called.\n" ) );
	
	if ( fATASocketType == kPCCardSocket )
	{
		
		// Device doesnÕt support flush cache. DonÕt send the command.
		fNumCommandsOutstanding--;
		status = kIOReturnSuccess;
		goto Exit;
		
	}
	
	cmd = ataCommandFlushCache ( );
	
	// Do we have a valid command?
	if ( cmd == NULL )
	{
		
		// Return no memory error.
		fNumCommandsOutstanding--;
		status = kIOReturnNoMemory;
		goto Exit;
		
	}
	
	// Send the command to flush the cache.
	status = syncExecute ( cmd, kATATimeout1Minute, 0 );	

Exit:
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doSynchronizeCache returning status = %ld.\n", ( UInt32 ) status ) );
	
	return status;
	
}


//---------------------------------------------------------------------------
// ¥ doStart - Handle a Start Unit command
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::doStart ( void )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doStart called.\n" ) );
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// ¥ doStop - Handle a Stop Unit command
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::doStop ( void )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doStop called.\n" ) );
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// ¥ getAdditionalDeviceInfoString - Return additional identification strings.
//---------------------------------------------------------------------------

char *
IOATABlockStorageDriver::getAdditionalDeviceInfoString ( void )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::getAdditionalDeviceInfoString called.\n" ) );
	return ( "[ATA]" );
	
}


//---------------------------------------------------------------------------
// ¥ getProductString - Return product identification strings.
//---------------------------------------------------------------------------

char *
IOATABlockStorageDriver::getProductString ( void )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::getProductString called.\n" ) );
	return fModel;
	
}


//---------------------------------------------------------------------------
// ¥ getRevisionString - Return product revision strings.
//---------------------------------------------------------------------------

char *
IOATABlockStorageDriver::getRevisionString ( void )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::getRevisionString called.\n" ) );
	return fRevision;
	
}


//---------------------------------------------------------------------------
// ¥ getVendorString - Return product vendor strings.
//---------------------------------------------------------------------------

char *
IOATABlockStorageDriver::getVendorString ( void )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::getVendorString called.\n" ) );
	return NULL;
	
}


//---------------------------------------------------------------------------
// ¥ reportBlockSize - 	Report the device block size in bytes. This is
//						ALWAYS 512-bytes (//¥¥¥Check if true for compact flash)
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::reportBlockSize ( UInt64 * blockSize )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportBlockSize called.\n" ) );
	
	*blockSize = kATADefaultSectorSize;
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// ¥ reportEjectability - Report the media in the ATA device as non-ejectable.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::reportEjectability ( bool * isEjectable )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportEjectability called.\n" ) );
	
	*isEjectable = false;
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// ¥ reportLockability - Fixed media, locking is invalid.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::reportLockability ( bool * isLockable )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportLockability called.\n" ) );
	
	*isLockable = false;
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// ¥ reportPollRequirements - 	Report the polling requirements for
//								removable media.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::reportPollRequirements ( 	bool * pollRequired,
													bool * pollIsExpensive )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportPollRequirements called.\n" ) );
	
	*pollIsExpensive	= false;
	*pollRequired		= false;
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// ¥ reportMaxReadTransfer - 	Report the max number of bytes transferred for
//								an ATA read command
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::reportMaxReadTransfer ( UInt64 blocksize, UInt64 * max )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMaxReadTransfer called.\n" ) );
	
	*max = blocksize * kIOATAMaxBlocksPerXfer;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMaxReadTransfer max = %ld.\n", *max ) );
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// ¥ reportMaxWriteTransfer - 	Report the max number of bytes transferred for
//								an ATA write command
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::reportMaxWriteTransfer ( UInt64 blocksize, UInt64 * max )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMaxWriteTransfer called.\n" ) );
	
	// Same as read transfer limits.
	return reportMaxReadTransfer ( blocksize, max );
	
}


//---------------------------------------------------------------------------
// ¥ reportMaxValidBlock - Returns the maximum addressable sector number
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::reportMaxValidBlock ( UInt64 * maxBlock )
{
	
	UInt64		diskCapacity = 0;
	
	assert ( fATADevice && maxBlock );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMaxValidBlock called.\n" ) );
	
	doGetFormatCapacities ( &diskCapacity, 1 );
	
	*maxBlock = ( diskCapacity / kATADefaultSectorSize ) - 1;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMaxValidBlock maxBlock = %ld.\n", *maxBlock ) );
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// ¥ reportMediaState - Report whether the media is currently present, and
//						whether a media change has been registered since the
//						last reporting.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::reportMediaState ( bool * mediaPresent, bool * changed )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMediaState called.\n" ) );

	*mediaPresent	= true;
	*changed		= true;
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// ¥ reportRemovability - Report whether the media is removable.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::reportRemovability ( bool * isRemovable )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::reportRemovability called.\n" ) );

	*isRemovable = false;
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// ¥ reportWriteProtection - Report if the media is write-protected.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::reportWriteProtection ( bool * isWriteProtected )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::reportWriteProtection called.\n" ) );

	*isWriteProtected = false;
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// ¥ message - Handles messages from our provider.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::message ( UInt32 type, IOService * provider, void * argument )
{
	
	IOReturn 	status = kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::message %p %lx\n", this, type ) );

	switch ( type )
	{
		
		case kATAResetEvent:					// Someone gave a hard reset to the drive
			SERIAL_STATUS_LOG ( ( "IOATABlockStorageDriver::message reset happened\n" ) );
			// reconfig device here
			fWakeUpResetOccurred = true;
			status = reconfigureATADevice ( );
			break;
		
		case kATANullEvent:						// Just kidding -- nothing happened

		// media bay, PC-Card events the drive has gone away and there's nothing 
		// that can be done about it.	
		case kATAOnlineEvent:					// An ATA device has come online
		case kATAOfflineEvent:					// An ATA device has gone offline
		case kATARemovedEvent:					// An ATA device has been removed from the bus
		
		// on earlier hardware, the system is able to lock the PC Card slot so we can 
		// get a request that the driver can then refuse gracefully.
		case kATAOfflineRequest:				// Someone requesting to offline the drive
		case kATAEjectRequest:					// Someone requesting to eject the drive

		// atapi resets are not relevent to ATA devices, but soft-resets ARE relevant to ATAPI devices.
		case kATAPIResetEvent:					// Someone gave a ATAPI reset to the drive
			break;
		
		case kIOMessageServiceIsRequestingClose:
		{
			
			STATUS_LOG ( ("%s: kIOMessageServiceIsRequestingClose Received\n", getName( ) ) );
            
            // tell clients to to away, terminate myself
            terminate ( kIOServiceRequired );             
			fATADevice->close ( this );
			status = kIOReturnSuccess;
			
		}
			break;
		
		default:
			status = super::message ( type, provider, argument );
			break;
	}

	
	return status;
	
}


#pragma mark -
#pragma mark Static Functions


//--------------------------------------------------------------------------------------
//	¥ sSwapBytes16	-	swaps the buffer for device identify data
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver::sSwapBytes16 ( UInt8 * buffer, IOByteCount numBytesToSwap )
{
	
	IOByteCount		index;
	UInt8			temp;
	UInt8 *			firstBytePtr;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::swapBytes16 called.\n" ) );
	
	for ( index = 0; index < numBytesToSwap; index += 2 )
	{
		
		firstBytePtr 	= buffer;				// save pointer
		temp 			= *buffer++;			// Save Byte0, point to Byte1
		*firstBytePtr 	= *buffer;				// Byte0 = Byte1
		*buffer++		= temp;					// Byte1 = Byte0
		
	}
	
}


//--------------------------------------------------------------------------------------
//	¥ sConvertHighestBitToNumber	-	Finds the higest bit in a number and returns
//--------------------------------------------------------------------------------------

UInt8
IOATABlockStorageDriver::sConvertHighestBitToNumber ( UInt16 bitField )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::convertHighestBitToNumber called.\n" ) );

	UInt16  index, integer;
	
	// Test all bits from left to right, terminating at the first non-zero bit
	for ( index = 0x0080, integer = 7; ( ( index & bitField ) == 0 && index != 0 ) ; index >>= 1, integer-- )
	{ ; }
	
	return integer;
	
}


// binary compatibility reserved method space
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 1 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 2 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 3 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 4 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 5 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 6 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 7 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 8 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 9 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 10 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 11 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 12 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 13 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 14 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 15 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver, 16 );


//--------------------------------------------------------------------------------------
//							End				Of				File
//--------------------------------------------------------------------------------------