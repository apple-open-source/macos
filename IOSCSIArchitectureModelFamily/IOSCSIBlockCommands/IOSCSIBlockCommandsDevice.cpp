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

#include <libkern/OSByteOrder.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/scsi-commands/SCSICommandDefinitions.h>
#include <IOKit/scsi-commands/IOBlockStorageServices.h>
#include <IOKit/scsi-commands/IOSCSIBlockCommandsDevice.h>


#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif

#define kMaxRetryCount 8

#define super IOSCSIPrimaryCommandsDevice
OSDefineMetaClass ( IOSCSIBlockCommandsDevice, IOSCSIPrimaryCommandsDevice );
OSDefineAbstractStructors ( IOSCSIBlockCommandsDevice, IOSCSIPrimaryCommandsDevice );

#pragma mark -
#pragma mark Static Class Methods

void 
IOSCSIBlockCommandsDevice::sProcessPoll( void * pdtDriver, void * refCon )
{
	IOSCSIBlockCommandsDevice *		driver;
	
	driver = (IOSCSIBlockCommandsDevice *) pdtDriver;
	
	driver->ProcessPoll();
	
	if ( driver->fPollingMode != kPollingMode_Suspended )
	{
		// schedule the poller again
		driver->EnablePolling();
	}
	
	// drop the retain associated with this poll
	driver->release();
}


#pragma mark -

bool 
IOSCSIBlockCommandsDevice::InitializeDeviceSupport( void )
{
	
	bool setupSuccessful 	= false;
	
	// Initialize the device characteristics flags
	fMediaIsRemovable 		= false;
	
	// Initialize the medium characteristics
	fMediumPresent			= false;
	fMediumIsWriteProtected	= true;
	fMediumRemovalPrevented	= false;
	fKnownManualEject		= false;
	
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::InitializeDeviceSupport called\n" ) );
	
	fIOSCSIBlockCommandsDeviceReserved = ( IOSCSIBlockCommandsDeviceExpansionData * )
			IOMalloc ( sizeof ( IOSCSIBlockCommandsDeviceExpansionData ) );
	
	if ( fIOSCSIBlockCommandsDeviceReserved == NULL )
	{
		goto ERROR_EXIT;
	}
	
	bzero ( fIOSCSIBlockCommandsDeviceReserved, sizeof ( IOSCSIBlockCommandsDeviceExpansionData ) );
	
	// Grab any device information from the IORegistry
	if ( getProperty( kIOPropertySCSIDeviceCharacteristicsKey ) != NULL )
	{
		// There is a characteristics property for this device, check for known entires.
		OSDictionary * characterDict;
		
		STATUS_LOG ( ( "%s: Get the SCSI Device Characteristics.\n", getName() ) );
		characterDict = OSDynamicCast( OSDictionary, getProperty( kIOPropertySCSIDeviceCharacteristicsKey ) );
		
		// Check if the personality for this device specifies that this is known to be manual ejectable.
		STATUS_LOG ( ( "%s: check for the Manual Eject property.\n", getName() ) );
		
		if ( characterDict->getObject( kIOPropertySCSIManualEjectKey ) != NULL )
		{
			STATUS_LOG ( ( "%s: found a Manual Eject property.\n", getName() ) );
			fKnownManualEject = true;
		}
	}
	
	// Make sure the drive is ready for us!
	if ( ClearNotReadyStatus ( ) == false )
	{
		goto ERROR_EXIT;
	}
        
	setupSuccessful = DetermineDeviceCharacteristics ( );
	
	if ( setupSuccessful == true ) 
	{		
		
		fPollingMode = kPollingMode_NewMedia;
		fPollingThread = thread_call_allocate (
						( thread_call_func_t ) IOSCSIBlockCommandsDevice::sProcessPoll,
						( thread_call_param_t ) this );
		
		if ( fPollingThread == NULL )
		{
			
			ERROR_LOG ( ( "fPollingThread allocation failed.\n" ) );
			setupSuccessful = false;
			goto ERROR_EXIT;
			
		}
		
		InitializePowerManagement ( GetProtocolDriver() );
		
	}
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::InitializeDeviceSupport setupSuccessful = %d\n", setupSuccessful ) );
	
	return setupSuccessful;
	
	
ERROR_EXIT:
	
	
	if ( fIOSCSIBlockCommandsDeviceReserved != NULL )
	{
		
		IOFree ( fIOSCSIBlockCommandsDeviceReserved, sizeof ( IOSCSIBlockCommandsDeviceExpansionData ) );
		fIOSCSIBlockCommandsDeviceReserved = NULL;
		
	}
	
	return setupSuccessful;
	
}

void 
IOSCSIBlockCommandsDevice::StartDeviceSupport( void )
{
	if( fMediaIsRemovable == false )
	{
		UInt32	attempts = 0;
		
		// We have a fixed disk, so make sure we determine its state
		// before we create the layer above us.

		
		do {
			
			ProcessPoll();
		
		} while ( ( fMediumPresent == false ) && 
				  ( ++attempts < kMaxRetryCount ) && 
				  ( isInactive ( ) == false ) );
	}
	else
	{
		// Removable media - start polling
		EnablePolling();		
	}

	CreateStorageServiceNub();
}


void 
IOSCSIBlockCommandsDevice::SuspendDeviceSupport( void )
{
	if( fPollingMode != kPollingMode_Suspended )
	{
    	DisablePolling();
    }		
}

void 
IOSCSIBlockCommandsDevice::ResumeDeviceSupport( void )
{
	// The driver has not found media in the device, restart 
	// the polling for new media.
	if( fMediumPresent == false )
	{
		fPollingMode = kPollingMode_NewMedia;
	    EnablePolling();
	}
}

void 
IOSCSIBlockCommandsDevice::StopDeviceSupport( void )
{
    DisablePolling();
}

void 
IOSCSIBlockCommandsDevice::TerminateDeviceSupport( void )
{
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::cleanUp called.\n" ) );

    if ( fPollingThread != NULL )
    {
        
        thread_call_free ( fPollingThread );
        fPollingThread = NULL;
        
    }
	
	// Release all memory/objects associated with the reserved fields.
	if ( fPowerDownNotifier != NULL )
	{
		
		// remove() will also call release() on this object (IONotifier).
		// See IONotifier.h for more info.
		fPowerDownNotifier->remove ( );
		fPowerDownNotifier = NULL;
		
	}
	
	// Release the reserved structure.
	if ( fIOSCSIBlockCommandsDeviceReserved != NULL )
	{
		
		IOFree ( fIOSCSIBlockCommandsDeviceReserved, sizeof ( IOSCSIBlockCommandsDeviceExpansionData ) );
		fIOSCSIBlockCommandsDeviceReserved = NULL;
		
	}
	
}

bool
IOSCSIBlockCommandsDevice::CreateCommandSetObjects ( void )
{

    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::CreateCommandSetObjects called\n" ) );
	
	fSCSIBlockCommandObject = SCSIBlockCommands::CreateSCSIBlockCommandObject ( );
	if ( fSCSIBlockCommandObject == NULL )
	{
		ERROR_LOG ( ( "%s: Could not allocate an SBC object\n", getName ( ) ) );
	 	return false;
	}

	return true;
}

void
IOSCSIBlockCommandsDevice::FreeCommandSetObjects ( void )
{

    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::FreeCommandSetObjects called\n" ) );

	if ( fSCSIBlockCommandObject ) 
	{
		
		fSCSIBlockCommandObject->release ( );
		fSCSIBlockCommandObject = NULL;
  	
	}

}

bool
IOSCSIBlockCommandsDevice::ClearNotReadyStatus ( void )
{
	
	SCSI_Sense_Data				senseBuffer;
	IOMemoryDescriptor *		bufferDesc;
	SCSITaskIdentifier			request;
	bool						driveReady = false;
	bool						result = true;
	SCSIServiceResponse 		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
													kSenseDefaultSize,
													kIODirectionIn );
	
	request = GetSCSITask ( );
	
	do
	{
		
		if ( TEST_UNIT_READY ( request, 0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIBlockCommandsDevice::ClearNotReadyStatus malformed command" ) );
		}
		
		if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
			
			bool validSense = false;
			
			if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
			{
				
				validSense = GetAutoSenseData ( request, &senseBuffer );
				if ( validSense == false )
				{
					
					if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0  ) == true )
					{
						// The command was successfully built, now send it
						serviceResponse = SendCommand ( request, 0 );
					}
					
					else
					{
						PANIC_NOW ( ( "IOSCSIBlockCommandsDevice::ClearNotReadyStatus malformed command" ) );
					}
					
					if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
					{
						
						validSense = true;
						
					}
					
				}
				
				if ( validSense == true )
				{
					
					if ( ( ( senseBuffer.SENSE_KEY  & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY  ) && 
							( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x01 ) )
					{
						
						STATUS_LOG ( ( "%s::drive not ready\n", getName ( ) ) );
						driveReady = false;
						IOSleep ( 200 );
						
					}
					
					else if ( ( ( senseBuffer.SENSE_KEY  & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY  ) && 
							( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) )
					{
						
						// The drive needs to be spun up. Issue a START_STOP_UNIT to it.
						if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01, 0x00 ) == true )
						{
							
							serviceResponse = SendCommand ( request, 0 );
							
						}
						
					}
					
					else
					{
						
						driveReady = true;
						STATUS_LOG ( ( "%s::drive READY\n", getName ( ) ) );
						
					}
					
					STATUS_LOG ( ( "sense data: %01x, %02x, %02x\n",
								( senseBuffer.SENSE_KEY  & kSENSE_KEY_Mask ),
								senseBuffer.ADDITIONAL_SENSE_CODE,
								senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
					
				}
				
			}
			
			else
			{
				
				driveReady = true;
				
			}
			
		}
		
		else
		{
			
			// the command failed - perhaps the device was hot unplugged
			// give other threads some time to run.
			IOSleep ( 200 );
			
		}
	
	// check isInactive in case device was hot unplugged during sleep
	// and we are in an infinite loop here
	} while ( ( driveReady == false ) && ( isInactive ( ) == false ) );
	
	bufferDesc->release ( );
	ReleaseSCSITask ( request );
	
	result = isInactive ( ) ? false : true;
	
	return result;
	
}


SCSIBlockCommands * 
IOSCSIBlockCommandsDevice::GetSCSIBlockCommandObject( void )
{
	return fSCSIBlockCommandObject;
}

SCSIPrimaryCommands	*
IOSCSIBlockCommandsDevice::GetSCSIPrimaryCommandObject( void )
{
    return  OSDynamicCast(SCSIPrimaryCommands, GetSCSIBlockCommandObject());
}

void 
IOSCSIBlockCommandsDevice::EnablePolling( void )
{		
    AbsoluteTime	time;
	
    if (( fPollingMode != kPollingMode_Suspended ) && fPollingThread )
    {
        // Retain ourselves so that this object doesn't go away
        // while we are polling
        
        retain ( );
        
        clock_interval_to_deadline ( 1000, kMillisecondScale, &time );
        thread_call_enter_delayed ( fPollingThread, time );
	}
}

void 
IOSCSIBlockCommandsDevice::DisablePolling( void )
{		
	fPollingMode = kPollingMode_Suspended;

	// Cancel the thread if it is running
	if( thread_call_cancel( fPollingThread ) )
	{
		// It was running, so we balance out the retain()
		// with a release()
		release();
	}
}

bool 
IOSCSIBlockCommandsDevice::DetermineDeviceCharacteristics( void )
{
	SCSIServiceResponse 			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier				request = NULL;
	IOMemoryDescriptor 			*	bufferDesc = NULL;
    SCSICmd_INQUIRY_StandardData * 	inquiryBuffer = NULL;
    UInt8							inquiryBufferCount;
	UInt8							loop;
	bool							succeeded = false;

	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::DetermineDeviceCharacteristics called\n" ) );
		
	if ( fDefaultInquiryCount == 0 )
	{
	
		// There is no default Inquiry count for this device, use the standard
		// structure size.
		STATUS_LOG ( ( "%s: use sizeof(SCSICmd_INQUIRY_StandardData) for Inquiry.\n", getName ( ) ) );
		inquiryBufferCount = sizeof ( SCSICmd_INQUIRY_StandardData );
	
	}
	else
	{
	
		// This device has a default inquiry count, use it.
		STATUS_LOG ( ( "%s: use fDefaultInquiryCount for Inquiry.\n", getName ( ) ) );
		inquiryBufferCount = fDefaultInquiryCount;
	
	}
	
	inquiryBuffer = ( SCSICmd_INQUIRY_StandardData * ) IOMalloc ( inquiryBufferCount );
	if ( inquiryBuffer == NULL )
	{
		
		STATUS_LOG ( ( "%s: Couldn't allocate Inquiry buffer.\n", getName ( ) ) );
		goto ErrorExit;
	
	}

	bufferDesc = IOMemoryDescriptor::withAddress ( inquiryBuffer, inquiryBufferCount, kIODirectionIn );
	if ( bufferDesc == NULL )
	{
		
		ERROR_LOG ( ( "%s: Couldn't alloc Inquiry buffer: ", getName ( ) ) );
		goto ErrorExit;
	
	}

	request = GetSCSITask ( );
	if ( request == NULL )
	{
		
		goto ErrorExit;
		
	}


	for ( loop = 0; ( loop < kMaxRetryCount ) && ( isInactive ( ) == false ) ; loop++ )
	{
		
		if ( INQUIRY ( 	request,
						bufferDesc,
						0,
						0,
						0x00,
						inquiryBufferCount,
						0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		
		}
		else
		{
			
			PANIC_NOW ( ( "IOSCSIBlockCommandsDevice::DetermineDeviceCharacteristics malformed command" ) );
			goto ErrorExit;
		
		}

		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
				( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			break;
			
		}
			
	}
	
	if( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
		( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
	{
		
		ERROR_LOG ( ( "%s: Inquiry completed with an error: ", getName ( ) ) );
		goto ErrorExit;
	
	}

	succeeded = true;

	// Save ANSI version of the device
	fANSIVersion = inquiryBuffer->VERSION & kINQUIRY_ANSI_VERSION_Mask;
	
	if ( ( inquiryBuffer->RMB & kINQUIRY_PERIPHERAL_RMB_BitMask ) 
			== kINQUIRY_PERIPHERAL_RMB_MediumRemovable )
	{
		
		fMediaIsRemovable = true;
	
	}
	else
	{
		
		fMediaIsRemovable = false;
	
	}

ErrorExit:

	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::DetermineDeviceCharacteristics exiting\n" ) );

	if ( request )
	{
	
		ReleaseSCSITask ( request );
		request = NULL;
	
	}

	if ( bufferDesc )
	{
	
		bufferDesc->release ( );
		bufferDesc = NULL;
	
	}
	
	if ( inquiryBuffer )	
	{
		IOFree( ( void * ) inquiryBuffer, inquiryBufferCount );
		inquiryBuffer = NULL;
	}
	
	return succeeded;
}


void 
IOSCSIBlockCommandsDevice::SetMediumCharacteristics( UInt32 blockSize, UInt32 blockCount )
{
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::SetMediumCharacteristics called\n" ) );
	STATUS_LOG ( ( "mediumBlockSize = %ld, blockCount = %ld\n", blockSize, blockCount ) );
	
	fMediumBlockSize	= blockSize;
	fMediumBlockCount	= blockCount;
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::SetMediumCharacteristics exiting\n" ) );
}


void 
IOSCSIBlockCommandsDevice::ResetMediumCharacteristics( void )
{
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ResetMediumCharacteristics called\n" ) );
	fMediumBlockSize	= 0;
	fMediumBlockCount	= 0;
	fMediumPresent		= false;
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ResetMediumCharacteristics exiting\n" ) );
}


void 
IOSCSIBlockCommandsDevice::CreateStorageServiceNub( void )
{
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::CreateStorageServiceNub entering.\n" ) );

	IOService * 	nub = new IOBlockStorageServices;
	if ( nub == NULL )
	{
		ERROR_LOG ( ( "IOSCSIBlockCommandsDevice::CreateStorageServiceNub failed\n" ) );
		PANIC_NOW(( "IOSCSIBlockCommandsDevice::CreateStorageServiceNub failed\n" ));
		return;
	}
	
	nub->init();
	
	if ( !nub->attach( this ) )
	{
		// panic since the nub can't attach
		PANIC_NOW(( "IOSCSIBlockCommandsDevice::CreateStorageServiceNub unable to attach nub" ));
		return;
	}
	
	nub->registerService();
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::CreateStorageServiceNub exiting.\n" ) );

	nub->release();
}


void 
IOSCSIBlockCommandsDevice::ProcessPoll( void )
{
	switch ( fPollingMode )
	{
		case kPollingMode_NewMedia:
		{
			PollForNewMedia();
		}
		break;
		
		case kPollingMode_MediaRemoval:
		{
			PollForMediaRemoval();
		}
		break;
		
		default:
		{
			// This is an unknown polling mode -- do nothing.
			ERROR_LOG ( ( "%s:ProcessPoll Unknown polling mode.\n", getName() ) );
		}
		break;
	}
}

void 
IOSCSIBlockCommandsDevice::PollForNewMedia( void )
{
	bool						mediaFound = false;
	UInt64						blockCount;
	UInt64						blockSize;

	// Since this is a poll for new media, 	
	fMediumPresent	= false;

	mediaFound = DetermineMediaPresence();
	if ( mediaFound == false )
	{
		return;
	}
	
	// If we got here, then we have found media
	if( fMediaIsRemovable == true )
	{
		fMediumRemovalPrevented = PreventMediumRemoval();
	}
	else
	{
		fMediumRemovalPrevented = true;
	}
	
	if ( DetermineMediumCapacity( &blockSize, &blockCount ) == false )
	{
		// Capacity could not be determined, treat it like no media inserted
		// and try again.
		return;
	}
	
	// What happens if the medium is unformatted? 
	// A check should be added to handle this case.
	
	SetMediumCharacteristics( blockSize, blockCount );
	
	fMediumIsWriteProtected = DetermineMediumWriteProtectState();
	
	fMediumPresent	= true;
	fPollingMode	= kPollingMode_Suspended;
	
	// Message up the chain that we have media
	messageClients ( kIOMessageMediaStateHasChanged, ( void * ) kIOMediaStateOnline );
	
}

// Check if media has been inserted into the device.
// if medium is detected, this method will return true, 
// else it will return false
bool
IOSCSIBlockCommandsDevice::DetermineMediaPresence( void )
{
	SCSIServiceResponse			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier			request;
	bool						mediaFound = false;

	STATUS_LOG(( "IOSCSIBlockCommandsDevice::DetermineMediaPresence called" ));

	request = GetSCSITask();
	
	// Do a TEST_UNIT_READY to generate sense data
	if( TEST_UNIT_READY( request, 0 ) == true )
    {
    	// The command was successfully built, now send it, set timeout to 10 seconds.
    	serviceResponse = SendCommand ( request, 10 * 1000 );
	}
	else
	{
		ERROR_LOG(( "IOSCSIBlockCommandsDevice::DetermineMediaPresence malformed command" ));
		ReleaseSCSITask( request );
		return false;
	}
	
	if( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		if( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			bool					validSense;
			SCSI_Sense_Data			senseBuffer;
			IOMemoryDescriptor *	bufferDesc;
			
			validSense = GetAutoSenseData( request, &senseBuffer );
			if( validSense == false )
			{
				bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																kSenseDefaultSize,
																kIODirectionIn );
				if( bufferDesc == NULL )
				{
					ERROR_LOG ( ( "%s: could not allocate sense buffer descriptor.\n", getName() ) );
					ReleaseSCSITask( request );
					return false;
				}
				
				// Get the sense data to determine if media is present.
				if( REQUEST_SENSE( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
			    {
			    	// The command was successfully built, now send it
			    	serviceResponse = SendCommand ( request, 0 );
				}
				else
				{
					ERROR_LOG(( "IOSCSIBlockCommandsDevice::PollForMedia malformed command" ));
					bufferDesc->release();
					ReleaseSCSITask( request );
					return false;
				}
				
				bufferDesc->release();
				
				if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
		 			( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
		 		{
					ERROR_LOG ( ( "%s: REQUEST_SENSE failed\n", getName() ) );
					ReleaseSCSITask( request );
					return false;
		 		}

			}
			
			if( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x00 ) && 
				( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
			{
				STATUS_LOG ( ( "Media found\n" ) );
                                mediaFound = true;
			}
			
			else
			{
				ERROR_LOG ( ( "ASC = 0x%02x, ASCQ = 0x%02x\n",
								senseBuffer.ADDITIONAL_SENSE_CODE,
								senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
			}
			
		}
		else
		{
			STATUS_LOG ( ( "Media found\n" ) );
			mediaFound = true;
		}
	}
	else
	{
		ERROR_LOG ( ( "serviceResponse = %d\n", serviceResponse ) );
	}
	
	ReleaseSCSITask( request );
	
	return mediaFound;
}

bool
IOSCSIBlockCommandsDevice::PreventMediumRemoval( void )
{
	SCSIServiceResponse		serviceResponse= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	bool  					mediumLocked;
	SCSITaskIdentifier		request;

	// Before forcing work to be done, verify that it is necessary by checking if this is a known
	// manual eject device.
	if( fKnownManualEject == true )
	{
		// This device is known to be manual eject so it is not possible to 
		// lock the media.
		return false;
	}
		
	request = GetSCSITask();

	if ( PREVENT_ALLOW_MEDIUM_REMOVAL( request, 1, 0 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		ERROR_LOG(( "IOSCSIBlockCommandsDevice::PollForMedia malformed command" ));
		ReleaseSCSITask( request );
		return false;
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
	 	( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		mediumLocked = true;
	}
	else
	{
		ERROR_LOG ( ( "%s: PREVENT_ALLOW_MEDIUM_REMOVAL failed\n", getName() ) );

		mediumLocked = false;
	}
	
	ReleaseSCSITask( request );

	return mediumLocked;
}

// Returns true if the capacity could be determined, else it returns false.
bool 
IOSCSIBlockCommandsDevice::DetermineMediumCapacity( UInt64 * blockSize, UInt64 * blockCount )
{
	SCSIServiceResponse			serviceResponse= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt32						capacityData[2];
	IOMemoryDescriptor *		bufferDesc		= NULL;
	SCSITaskIdentifier			request			= NULL;
	bool						result;

	*blockSize 	= 0;
	*blockCount = 0;

	request = GetSCSITask();
	if ( request == NULL )
	{
		result = false;
		goto isDone;
	}
	
	bufferDesc = IOMemoryDescriptor::withAddress( capacityData, 8, kIODirectionIn );
	if ( bufferDesc == NULL )
	{
		result = false;
		goto isDone;
	}
		
	// We found media, get its capacity
	if ( READ_CAPACITY( request, bufferDesc, 0, 0x00, 0, 0 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		ERROR_LOG(( "IOSCSIBlockCommandsDevice::PollForMedia malformed command" ));
    	result = false;
    	goto isDone;
	}
		
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		*blockSize 	= ( UInt64 ) OSSwapBigToHostInt32 ( capacityData[1] );
		*blockCount = ( UInt64 ) ( OSSwapBigToHostInt32 ( capacityData[0] ) + 1 );
		STATUS_LOG ( ( "%s: Media capacity: %x and block size: %x\n",
						getName(), (UInt32) *blockCount, (UInt32) *blockSize ) );
		result = true;
	}
	else
	{
		ERROR_LOG ( ( "%s: Read Capacity failed\n", getName() ) );
    	result = false;
	}

isDone:
	if ( request != NULL )
	{
		ReleaseSCSITask( request );
	}
	
	if ( bufferDesc != NULL )
	{
		bufferDesc->release();
	}
	
	return result;
}

bool 
IOSCSIBlockCommandsDevice::DetermineMediumWriteProtectState( void )
{
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					modeBuffer[16];
	IOMemoryDescriptor *	bufferDesc;
	SCSITaskIdentifier		request;
	bool					writeProtectDetermined = false;
	bool					mediumIsProtected = true;
	SCSI_Sense_Data			senseBuffer;

	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::checkWriteProtection called\n" ) );

	// For now, report all fixed disks as writable since most have no way of changing this.
	if( fMediaIsRemovable == false )
	{
		return false;
	}
		
	request = GetSCSITask();


	// Send a Request Sense to the device, this seems to make some happy again.
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
													kSenseDefaultSize,
													kIODirectionIn );
	if( bufferDesc != NULL )
	{
		// Issue a Request Sense
		// Whether the command completes successfully or not is irrelevent.
		if( REQUEST_SENSE( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
	    {
	    	// The command was successfully built, now send it
	    	serviceResponse = SendCommand ( request, 0 );
		}

		// release the sense data buffer;
		bufferDesc->release();
	}
	
	// Now back to normal programming.
	bufferDesc = IOMemoryDescriptor::withAddress( 	modeBuffer,
													8,
													kIODirectionIn );

	// The device does not claim compliance with any ANSI version, so it 
	// is most likely an ATAPI device, try the 10 byte command first.
	if ( fANSIVersion == kINQUIRY_ANSI_VERSION_NoClaimedConformance )
	{
		if ( MODE_SENSE_10( 	request,
								bufferDesc,
								0x00,
								0x00,
								0x00,	// Normally, we set DBD=1, but since we're only
								0x3F,	// interested in modeBuffer[3], we set DBD=0 since
								8,		// it makes legacy devices happier
								0 ) == true )
	    {
	    	// The command was successfully built, now send it
	    	serviceResponse = SendCommand ( request, 0 );
		}
		else
		{
			ERROR_LOG(( "IOSCSIBlockCommandsDevice::CheckWriteProtection malformed command" ));
			return true;
		}

		if( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			STATUS_LOG ( ( "%s: Returned Mode sense data: ", getName() ) );
			
#if SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 3

				for ( UInt32 i = 0;i < 8; i++ )
				{
					STATUS_LOG ( ( "%x: ", modeBuffer[i] ) );
				}
		
		        STATUS_LOG ( ( "\n" ) );

#endif // SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 3
			
			if ( ( modeBuffer[3] & 0x80 ) != 0 )
			{
			 	mediumIsProtected = true;
			}
			else
			{
				mediumIsProtected = false;
			}
			
			writeProtectDetermined = true;
		}
	}

	// Check if the write protect status has been successfully determined.
	if ( writeProtectDetermined == false )
	{
		// Either this device reports an ANSI version, or the 10 Byte command failed.
		// Try the six byte mode sense.	
		if ( MODE_SENSE_6( 	request, 
							bufferDesc,
							0x00,
							0x00,	// Normally, we set DBD=1, but since we're only
							0x3F,	// interested in modeBuffer[3], we set DBD=0 since
							8,		// it makes legacy devices happier
							0 ) == true )
	    {
	    	// The command was successfully built, now send it
	    	serviceResponse = SendCommand ( request, 0 );
		}
		else
		{
			ERROR_LOG(( "IOSCSIBlockCommandsDevice::CheckWriteProtection malformed command" ));
			return true;
		}
		
		if( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{

			STATUS_LOG ( ( "%s: Returned Mode sense data: ", getName() ) );
#if DEBUG
			for ( UInt32 i = 0;i < 8; i++ )
			{
				STATUS_LOG ( ( "%x: ", modeBuffer[i] ) );
			}
		
			STATUS_LOG ( ( "\n" ) );
#endif // DEBUG
			
			if ( ( modeBuffer[2] & 0x80 ) != 0 )
			{
				mediumIsProtected = true;
			}
			else
			{
				mediumIsProtected = false;
			}
		}
		else
		{
			ERROR_LOG ( ( "%s: Mode Sense failed\n", getName() ) );
			
			// The mode sense failed, mark as write protected to be safe.
			mediumIsProtected = true;
		}
	}
	
	bufferDesc->release();
	ReleaseSCSITask ( request );
	
	return mediumIsProtected;
}

void 
IOSCSIBlockCommandsDevice::PollForMediaRemoval( void )
{
	SCSIServiceResponse			serviceResponse= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier			request;
	bool						mediaRemoved = false;
		
	request = GetSCSITask();
	
	// Do a TEST_UNIT_READY to generate sense data
	if( TEST_UNIT_READY( request, 0 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOSCSIBlockCommandsDevice::PollForMedia malformed command" ));
	}
	
	if( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		if( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			bool						validSense;
			SCSI_Sense_Data				senseBuffer;
			IOMemoryDescriptor *		bufferDesc;
			
			validSense = GetAutoSenseData( request, &senseBuffer );
			if( validSense == false )
			{
				bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																kSenseDefaultSize,
																kIODirectionIn );
				if( bufferDesc == NULL )
				{
					ERROR_LOG ( ( "%s: could not allocate sense buffer descriptor.\n", getName() ) );
					ReleaseSCSITask( request );
					return;
				}
				
				if( REQUEST_SENSE( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
			    {
			    	// The command was successfully built, now send it
			    	serviceResponse = SendCommand ( request, 0 );
				}
				else
				{
					PANIC_NOW(( "IOSCSIBlockCommandsDevice::PollForMedia malformed command" ));
				}
				
				bufferDesc->release();
				
				if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
		 			( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
		 		{
					ERROR_LOG ( ( "%s: REQUEST_SENSE failed\n", getName() ) );
					ReleaseSCSITask( request );
					return;
		 		}

			}
			
			if( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x28 ) ||
				( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x3A ) )
			{
				mediaRemoved = true;
			}
		}
		else
		{
			mediaRemoved = false;
		}
	}
	
	ReleaseSCSITask( request );
	
	if ( mediaRemoved == true )
	{
		// Media was removed, set the polling to determine when new media has been inserted
 		fPollingMode = kPollingMode_NewMedia;
	}
}


#pragma mark -
#pragma mark Client Requests Support

void 
IOSCSIBlockCommandsDevice::AsyncReadWriteComplete( SCSITaskIdentifier request )
{
	void *						clientData;
	IOReturn					status;
	UInt64						actCount = 0;
	IOSCSIBlockCommandsDevice *	taskOwner;
	SCSITask *					scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		PANIC_NOW(( "IOSCSIBlockCommandsDevice::AsyncReadWriteComplete scsiRequest==NULL." ));
	}

	// Extract the client data from the SCSITask	
	clientData	= scsiRequest->GetApplicationLayerReference();
	
	if (( scsiRequest->GetServiceResponse() == kSCSIServiceResponse_TASK_COMPLETE ) &&
		( scsiRequest->GetTaskStatus () == kSCSITaskStatus_GOOD )) 
	{
		status = kIOReturnSuccess;
	}
	else
	{
		ERROR_LOG ( ( "Error on read/write\n" ) );
		status = kIOReturnError;
	}

	if ( status == kIOReturnSuccess )
	{
		actCount = scsiRequest->GetDataBuffer()->getLength();
	}
	
	taskOwner = OSDynamicCast( IOSCSIBlockCommandsDevice, scsiRequest->GetTaskOwner());
	if ( taskOwner == NULL )
	{
		PANIC_NOW(( "IOSCSIBlockCommandsDevice::AsyncReadWriteComplete taskOwner==NULL." ));
	}

	taskOwner->ReleaseSCSITask( request );

	IOBlockStorageServices::AsyncReadWriteComplete( clientData, status, actCount );
}

// Perform the Synchronous Read Request
IOReturn 
IOSCSIBlockCommandsDevice::IssueRead( 	IOMemoryDescriptor *	buffer,
										UInt64					startBlock,
										UInt64					blockCount )
{
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request;
	
	STATUS_LOG ( ( "%s: syncRead Attempted\n", getName() ) );

	request = GetSCSITask ( );
	
	if ( READ_10( 	request,
					buffer,
  					fMediumBlockSize,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 ) == false )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOSCSIBlockCommandsDevice::IssueRead malformed command" ));
	}

	ReleaseSCSITask ( request );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		return kIOReturnSuccess;
	}
	else
	{
		return kIOReturnError;
	}
}


// Perform the Asynchronous Read Request
IOReturn 
IOSCSIBlockCommandsDevice::IssueRead( 	IOMemoryDescriptor *	buffer,
										UInt64					startBlock,
										UInt64					blockCount,
										void *					clientData )
{
	IOReturn 				status = kIOReturnSuccess;
	SCSITaskIdentifier		request;

	STATUS_LOG ( ( "%s: asyncRead Attempted\n", getName() ) );
	
	request = GetSCSITask();
	
	if (READ_10(	request,
					buffer,
      				fMediumBlockSize,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 ) == true )
    {
    	// The command was successfully built, now send it
    	SetApplicationLayerReference( request, clientData );
		STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::IssueRead send command.\n" ) );
    	SendCommand ( request, 0, &this->AsyncReadWriteComplete );
	}
	else
	{
		PANIC_NOW(( "IOSCSIBlockCommandsDevice::IssueWrite malformed command" ));
		status = kIOReturnError;
	}

	return status;
}

// Perform the Synchronous Write Request
IOReturn 
IOSCSIBlockCommandsDevice::IssueWrite( 	IOMemoryDescriptor *	buffer,
										UInt64					startBlock,
										UInt64					blockCount )
{
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request;
	
	STATUS_LOG ( ( "%s: syncWrite Attempted\n", getName() ) );
	
	request = GetSCSITask();
	if ( WRITE_10( 	request,
					buffer,
					fMediumBlockSize,
					0,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOSCSIBlockCommandsDevice::IssueWrite malformed command" ));
	}

	ReleaseSCSITask ( request );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		return kIOReturnSuccess;
	}
	else
	{
		return kIOReturnError;
	}
}


// Perform the Asynchronous Write Request
IOReturn 
IOSCSIBlockCommandsDevice::IssueWrite(	IOMemoryDescriptor *	buffer,
										UInt64					startBlock,
										UInt64					blockCount,
										void *					clientData )
{
	IOReturn				status = kIOReturnSuccess;
	SCSITaskIdentifier		request;
	
	STATUS_LOG ( ( "%s: asyncWrite Attempted\n", getName() ) );

	request = GetSCSITask();
	
	if ( WRITE_10( 	request, 
					buffer,
   					fMediumBlockSize,
					0,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 ) == true )
    {
    	// The command was successfully built, now send it
    	SetApplicationLayerReference( request, clientData );
		STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::IssueWrite send command.\n" ) );
    	SendCommand ( request, 0, &this->AsyncReadWriteComplete );
	}
	else
	{
		PANIC_NOW(( "IOSCSIBlockCommandsDevice::IssueWrite malformed command" ));
	}

	return status;
}


IOReturn 
IOSCSIBlockCommandsDevice::SyncReadWrite ( 	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount,
                         					UInt64					blockSize )
{
	IODirection		direction;
	IOReturn		theErr;

	if ( IsProtocolAccessEnabled() == false )
	{
		return kIOReturnNotAttached;
	}
	
	if ( IsDeviceAccessEnabled() == false )
	{
		return kIOReturnOffline;
	}
	
	direction = buffer->getDirection();
	
	if ( direction == kIODirectionIn )
	{
		theErr = IssueRead( buffer, startBlock, blockCount );
	}
	else if ( direction == kIODirectionOut )
	{
		theErr = IssueWrite( buffer, startBlock, blockCount );
	}
	else
	{
		ERROR_LOG ( ( "%s: doSyncReadWrite bad direction argument\n", getName() ) );
		theErr = kIOReturnBadArgument;
	}
	
	return theErr;
}

IOReturn 
IOSCSIBlockCommandsDevice::AsyncReadWrite (	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount,
                         					UInt64					blockSize,
											void *					clientData )
{
	IODirection		direction;
	IOReturn		theErr;
	
	if ( IsProtocolAccessEnabled() == false )
	{
		return kIOReturnNotAttached;
	}
	
	if ( IsDeviceAccessEnabled() == false )
	{
		return kIOReturnOffline;
	}

	direction = buffer->getDirection();
	if ( direction == kIODirectionIn )
	{
		IssueRead( buffer, startBlock, blockCount, clientData );
		theErr = kIOReturnSuccess;
	}
	else if ( direction == kIODirectionOut )
	{
		IssueWrite( buffer, startBlock, blockCount, clientData );
		theErr = kIOReturnSuccess;
	}
	else
	{
		ERROR_LOG ( ( "%s: doAsyncReadWrite bad direction argument\n", getName() ) );
		theErr = kIOReturnBadArgument;
	}
	
	return theErr;
}

IOReturn 
IOSCSIBlockCommandsDevice::EjectTheMedium( void )
{
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request;
	bool					doPollForRemoval = false;
	
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::EjectTheMedium called\n" ) );
	
	if ( IsProtocolAccessEnabled() == false )
	{
		return kIOReturnNotAttached;
	}
	
	if ( IsDeviceAccessEnabled() == false )
	{
		return kIOReturnOffline;
	}

	request = GetSCSITask ( );
	
    if ( fMediaIsRemovable == true )
	{
		if( fKnownManualEject == false )
		{
			if ( PREVENT_ALLOW_MEDIUM_REMOVAL( request, 0, 0 ) == true )
		    {
		    	// The command was successfully built, now send it
		    	serviceResponse = SendCommand ( request, 0 );
			}
			else
			{
				PANIC_NOW(( "IOSCSIBlockCommandsDevice::EjectTheMedium malformed command" ));
			}

			if ( START_STOP_UNIT( request, 0, 0, 1, 0, 0 ) == true )
		    {
		    	// The command was successfully built, now send it
		    	serviceResponse = SendCommand ( request, 0 );
			}
			else
			{
				PANIC_NOW(( "IOSCSIBlockCommandsDevice::EjectTheMedium malformed command" ));
			}

			if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 	( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
			{
				// The eject command failed.  This is most likely a manually ejectable
				// device, start the polling to determine when the media has been removed.
				doPollForRemoval = true;
			}
		}
	}
	else
	{
		if ( SYNCHRONIZE_CACHE( request, 0, 0, 0, 0, 0 ) == true )
	    {
	    	// The command was successfully built, now send it
	    	serviceResponse = SendCommand ( request, 0 );
		}
		else
		{
			PANIC_NOW(( "IOSCSIBlockCommandsDevice::EjectTheMedium malformed command" ));
		}

		ReleaseSCSITask( request );
		return kIOReturnSuccess;
	}
	
	ReleaseSCSITask( request );

	ResetMediumCharacteristics();
	
	fMediumIsWriteProtected = true;
	
    if ( fMediaIsRemovable == true )
    {
    	if (( doPollForRemoval == true ) || ( fMediumRemovalPrevented == false ) || ( fKnownManualEject == true ))
    	{
    		// Set the polling to determine when media has been removed
 			fPollingMode = kPollingMode_MediaRemoval;
   		}
    	else
    	{
    		// Set the polling to determine when new media has been inserted
 			fPollingMode = kPollingMode_NewMedia;
    	}
    	
		EnablePolling();
	}
		
	return kIOReturnSuccess;
}


IOReturn 
IOSCSIBlockCommandsDevice::FormatMedium( UInt64 blockCount, UInt64 blockSize )
{
	IOReturn	theErr = kIOReturnSuccess;

    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::FormatMedium called\n" ) );

	if ( IsProtocolAccessEnabled() == false )
	{
		return kIOReturnNotAttached;
	}
	
	if ( IsDeviceAccessEnabled() == false )
	{
		return kIOReturnOffline;
	}

	return theErr;	
}


UInt32 
IOSCSIBlockCommandsDevice::GetFormatCapacities(	UInt64 * capacities,
												UInt32   capacitiesMaxCount ) const
{
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::doGetFormatCapacities called\n" ) );

	return 0;
}


IOReturn 
IOSCSIBlockCommandsDevice::LockUnlockMedium( bool doLock )
{
	IOReturn	theErr = kIOReturnSuccess;

	if ( IsProtocolAccessEnabled() == false )
	{
		return kIOReturnNotAttached;
	}
	
	if ( IsDeviceAccessEnabled() == false )
	{
		return kIOReturnOffline;
	}


	return theErr;
}


IOReturn 
IOSCSIBlockCommandsDevice::SynchronizeCache( void )
{
	IOReturn				theErr = kIOReturnSuccess;

	if ( IsProtocolAccessEnabled() == false )
	{
		return kIOReturnNotAttached;
	}
	
	if ( IsDeviceAccessEnabled() == false )
	{
		return kIOReturnOffline;
	}


	return theErr;
}

#pragma mark - 
#pragma mark Query methods to report device characteristics 

UInt64 
IOSCSIBlockCommandsDevice::ReportDeviceMaxBlocksReadTransfer( void )
{
	UInt64	maxBlockCount;
	bool	supported;
	
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::reportMaxReadTransfer\n" ) );

	supported = GetProtocolDriver()->
				IsProtocolServiceSupported( kSCSIProtocolFeature_MaximumReadBlockTransferCount, &maxBlockCount );	
	if ( supported == false )
	{
		maxBlockCount = 256;
	}

	return maxBlockCount;
}

UInt64 
IOSCSIBlockCommandsDevice::ReportDeviceMaxBlocksWriteTransfer( void )
{
	UInt64	maxBlockCount;
	bool	supported;
	
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::reportMaxWriteTransfer.\n" ) );

	supported = GetProtocolDriver()->
				IsProtocolServiceSupported( kSCSIProtocolFeature_MaximumWriteBlockTransferCount, &maxBlockCount );	
	if ( supported == false )
	{
		maxBlockCount = 256;
	}

	return maxBlockCount;
}

bool 
IOSCSIBlockCommandsDevice::ReportDeviceMediaRemovability( void )
{
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ReportMediaRemovability fMediaIsRemovable = %d\n", ( int ) fMediaIsRemovable ) );

	return fMediaIsRemovable;
}


#pragma mark - 
#pragma mark Query methods to report installed medium characteristics 

UInt64 
IOSCSIBlockCommandsDevice::ReportMediumBlockSize( void )
{
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ReportMediumBlockSize blockSize = %ld\n", ( UInt32 ) fMediumBlockSize ) );

	return fMediumBlockSize;
}


UInt64 
IOSCSIBlockCommandsDevice::ReportMediumTotalBlockCount( void )
{
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ReportMediumTotalBlockCount maxBlock = %ld\n", fMediumBlockCount ) );

	return fMediumBlockCount;
}

bool 
IOSCSIBlockCommandsDevice::ReportMediumWriteProtection( void )
{
    STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ReportMediumWriteProtection isWriteProtected = %d.\n", fMediumIsWriteProtected ) );	

	return fMediumIsWriteProtected;
}

#pragma mark -
#pragma mark SCSI Block Commands Builders
bool	
IOSCSIBlockCommandsDevice::ERASE_10(
						SCSITaskIdentifier			request,
	    				SCSICmdField1Bit 			ERA, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *	scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->ERASE_10(
				scsiRequest,
    			ERA, 
    			RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			TRANSFER_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::ERASE_12(
						SCSITaskIdentifier			request,
		    			SCSICmdField1Bit 			ERA, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField4Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *	scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->ERASE_12(
				scsiRequest,
    			ERA, 
    			RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			TRANSFER_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::FORMAT_UNIT(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer,
		    			IOByteCount					defectListSize,
		    			SCSICmdField1Bit 			FMTDATA, 
		    			SCSICmdField1Bit 			CMPLST, 
		    			SCSICmdField3Bit 			DEFECT_LIST_FORMAT, 
		    			SCSICmdField1Byte 			VENDOR_SPECIFIC, 
		    			SCSICmdField2Byte 			INTERLEAVE, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *	scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->FORMAT_UNIT(
				scsiRequest,
				dataBuffer,
				defectListSize,
    			FMTDATA, 
    			CMPLST, 
    			DEFECT_LIST_FORMAT, 
    			VENDOR_SPECIFIC, 
    			INTERLEAVE, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::LOCK_UNLOCK_CACHE(
						SCSITaskIdentifier			request,
		    			SCSICmdField1Bit 			LOCK, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			NUMBER_OF_BLOCKS, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *	scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->LOCK_UNLOCK_CACHE(
				scsiRequest,
    			LOCK, 
    			RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			NUMBER_OF_BLOCKS, 
    			CONTROL );
}


bool	
IOSCSIBlockCommandsDevice::MEDIUM_SCAN(
						SCSITaskIdentifier			request,
			     		IOMemoryDescriptor 		 	*dataBuffer,
			   			SCSICmdField1Bit 			WBS, 
			   			SCSICmdField1Bit 			ASA, 
			   			SCSICmdField1Bit 			RSD, 
			   			SCSICmdField1Bit 			PRA, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			PARAMETER_LIST_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *	scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->MEDIUM_SCAN(
				scsiRequest,
				dataBuffer,
   				WBS, 
   				ASA, 
   				RSD, 
   				PRA, 
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				PARAMETER_LIST_LENGTH, 
				CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::PREFETCH(
						SCSITaskIdentifier			request,
		    			SCSICmdField1Bit 			IMMED, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *	scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->PREFETCH(
				scsiRequest,
    			IMMED, 
    			RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			TRANSFER_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::READ_6(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			UInt32						blockSize,
		    			SCSICmdField21Bit 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField1Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	UInt32 			requestedByteCount;
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	// Do the pre-flight check on the passed in parameters
	// Check the validity of the media
	if ( blockSize == 0 )
	{
		// There is no media in the device, or it has an undetermined
		// blocksize (could be unformatted).
		return false;
	}

	// Make sure that we were given a valid buffer
	if (dataBuffer == NULL )
	{
		return false;
	}
	else
	{
		// We have a valid buffer object, check that it has the required
		// capcity for the data to be transfered.
		if ( TRANSFER_LENGTH == 0 )
		{
			// The TRANSFER_LENGTH is zero, this indicates that 256 blocks
			// should be transfer from the device
			requestedByteCount = 256 * blockSize;
		}
		else
		{
			requestedByteCount = TRANSFER_LENGTH * blockSize;
		}
		
		// We know the number of bytes to transfer, now check that the 
		// buffer is large ebnough to accomodate thuis request.
		if ( dataBuffer->getLength() < requestedByteCount )
		{
			return false;
		}
	}

	return GetSCSIBlockCommandObject()->READ_6(
				scsiRequest,
				dataBuffer,
				requestedByteCount,
    			LOGICAL_BLOCK_ADDRESS, 
    			TRANSFER_LENGTH, 
    			CONTROL );
}

bool 	
IOSCSIBlockCommandsDevice::READ_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit			FUA,
						SCSICmdField1Bit			RELADR,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte			TRANSFER_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	UInt32			requestedByteCount;
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	// Check the validity of the media
	if ( blockSize == 0 )
	{
		// There is no media in the device, or it has an undetermined
		// blocksize (could be unformatted).
		return false;
	}

	// Make sure that we were given a valid buffer
	if (dataBuffer == NULL )
	{
		return false;
	}
	else
	{
		// We have a valid buffer object, check that it has the required
		// capcity for the data to be transfered.
		requestedByteCount = TRANSFER_LENGTH * blockSize;
		
		// We know the number of bytes to transfer, now check that the 
		// buffer is large ebnough to accomodate thuis request.
		if ( dataBuffer->getLength() < requestedByteCount )
		{
			return false;
		}
	}

	return GetSCSIBlockCommandObject ( )->READ_10(
				scsiRequest,
				dataBuffer,
				requestedByteCount,
    			DPO, 
    			FUA,
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				TRANSFER_LENGTH, 
				CONTROL );
}

bool 	
IOSCSIBlockCommandsDevice::READ_12(
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 		 	*dataBuffer, 
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO, 
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField4Byte 			TRANSFER_LENGTH, 
						SCSICmdField1Byte 			CONTROL )
{
	UInt64 			requestedByteCount;
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	// Check the validity of the media
	if ( blockSize == 0 )
	{
		// There is no media in the device, or it has an undetermined
		// blocksize (could be unformatted).
		return false;
	}

	// Make sure that we were given a valid buffer
	if (dataBuffer == NULL )
	{
		return false;
	}
	else
	{
		// We have a valid buffer object, check that it has the required
		// capcity for the data to be transfered.
		requestedByteCount = TRANSFER_LENGTH * blockSize;
		
		// We know the number of bytes to transfer, now check that the 
		// buffer is large ebnough to accomodate thuis request.
		if ( dataBuffer->getLength() < requestedByteCount )
		{
			return false;
		}
	}

	return GetSCSIBlockCommandObject()->READ_12(
				scsiRequest,
				dataBuffer,
				requestedByteCount,
    			DPO, 
    			FUA,
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				TRANSFER_LENGTH, 
				CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::READ_CAPACITY(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField1Bit 			PMI, 
						SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	// Make sure that we were given a buffer
	if ( dataBuffer == NULL )
	{
		// whatever would be similar to paramErr
		return false;
	}

	return GetSCSIBlockCommandObject()->READ_CAPACITY(
				scsiRequest,
				dataBuffer,
    			RELADR,
				LOGICAL_BLOCK_ADDRESS, 
				PMI, 
				CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::READ_DEFECT_DATA_10(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			PLIST, 
		    			SCSICmdField1Bit 			GLIST, 
		    			SCSICmdField3Bit 			DEFECT_LIST_FORMAT, 
		    			SCSICmdField2Byte 			ALLOCATION_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->READ_DEFECT_DATA_10(
				scsiRequest,
				dataBuffer,
    			PLIST, 
    			GLIST, 
    			DEFECT_LIST_FORMAT, 
    			ALLOCATION_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::READ_DEFECT_DATA_12(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			PLIST, 
		    			SCSICmdField1Bit 			GLIST, 
		    			SCSICmdField3Bit 			DEFECT_LIST_FORMAT, 
		    			SCSICmdField4Byte 			ALLOCATION_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->READ_DEFECT_DATA_12(
				scsiRequest,
				dataBuffer,
    			PLIST, 
    			GLIST, 
    			DEFECT_LIST_FORMAT, 
    			ALLOCATION_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::READ_GENERATION(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField1Byte 			ALLOCATION_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->READ_GENERATION(
				scsiRequest,
				dataBuffer,
    			RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			ALLOCATION_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::READ_LONG(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			CORRCT, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			BYTE_TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->READ_LONG(
				scsiRequest,
				dataBuffer,
    			CORRCT, 
    			RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			BYTE_TRANSFER_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::READ_UPDATED_BLOCK_10(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			DPO, 
		    			SCSICmdField1Bit 			FUA,
		    		 	SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField1Bit 			LATEST, 
		    		 	SCSICmdField15Bit 			GENERATION_ADDRESS, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->READ_UPDATED_BLOCK_10(
				scsiRequest,
				dataBuffer,
    			DPO, 
    			FUA,
    		 	RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			LATEST, 
    		 	GENERATION_ADDRESS, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::REASSIGN_BLOCKS(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->REASSIGN_BLOCKS(
				scsiRequest,
				dataBuffer,
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::REBUILD(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			DPO, 
		    			SCSICmdField1Bit 			FUA,
		    		 	SCSICmdField1Bit 			INTDATA, 
		    			SCSICmdField2Bit 			PORT_CONTROL, 
		    		 	SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField4Byte 			REBUILD_LENGTH, 
		    			SCSICmdField4Byte 			PARAMETER_LIST_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->REBUILD(
				scsiRequest,
				dataBuffer,
    			DPO, 
    			FUA,
    		 	INTDATA, 
    			PORT_CONTROL, 
    		 	LOGICAL_BLOCK_ADDRESS, 
    			REBUILD_LENGTH, 
    			PARAMETER_LIST_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::REGENERATE(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit			DPO, 
		    			SCSICmdField1Bit 			FUA,
		    		 	SCSICmdField1Bit 			INTDATA, 
		    		 	SCSICmdField2Bit 			PORT_CONTROL, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField4Byte 			REBUILD_LENGTH, 
		    			SCSICmdField4Byte 			PARAMETER_LIST_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->REGENERATE(
				scsiRequest,
				dataBuffer,
    			DPO, 
    			FUA,
    		 	INTDATA, 
    		 	PORT_CONTROL, 
    			LOGICAL_BLOCK_ADDRESS, 
    			REBUILD_LENGTH, 
    			PARAMETER_LIST_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::REZERO_UNIT( 
						SCSITaskIdentifier			request,
    					SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->REZERO_UNIT( 
					scsiRequest,
					CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::SEARCH_DATA_EQUAL_10(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			INVERT, 
		    			SCSICmdField1Bit 			SPNDAT, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->SEARCH_DATA_EQUAL_10(
				scsiRequest,
				dataBuffer,
    			INVERT, 
    			SPNDAT, 
    			RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			NUMBER_OF_BLOCKS_TO_SEARCH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::SEARCH_DATA_HIGH_10(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			INVERT, 
		    			SCSICmdField1Bit 			SPNDAT, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->SEARCH_DATA_HIGH_10(
				scsiRequest,
				dataBuffer,
    			INVERT, 
    			SPNDAT, 
    			RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			NUMBER_OF_BLOCKS_TO_SEARCH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::SEARCH_DATA_LOW_10(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			INVERT, 
		    			SCSICmdField1Bit 			SPNDAT, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			NUMBER_OF_BLOCKS_TO_SEARCH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->SEARCH_DATA_LOW_10(
				scsiRequest,
				dataBuffer,
    			INVERT, 
    			SPNDAT, 
    			RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			NUMBER_OF_BLOCKS_TO_SEARCH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::SEEK_6( 
						SCSITaskIdentifier			request,
		    			SCSICmdField21Bit 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->SEEK_6(
				scsiRequest,
    			LOGICAL_BLOCK_ADDRESS, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::SEEK_10( 
						SCSITaskIdentifier			request,
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->SEEK_10(
				scsiRequest,
    			LOGICAL_BLOCK_ADDRESS, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::SET_LIMITS_10( 
						SCSITaskIdentifier			request,
		    			SCSICmdField1Bit 			RDINH, 
		    			SCSICmdField1Bit 			WRINH, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->SET_LIMITS_10(
				scsiRequest,
    			RDINH, 
    			WRINH, 
    			LOGICAL_BLOCK_ADDRESS, 
    			NUMBER_OF_BLOCKS,
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::SET_LIMITS_12( 
						SCSITaskIdentifier			request,
		    			SCSICmdField1Bit 			RDINH, 
		    			SCSICmdField1Bit 			WRINH, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField4Byte 			NUMBER_OF_BLOCKS,
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->SET_LIMITS_12(
				scsiRequest,
    			RDINH, 
    			WRINH, 
    			LOGICAL_BLOCK_ADDRESS, 
    			NUMBER_OF_BLOCKS,
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::START_STOP_UNIT( 
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED, 
						SCSICmdField4Bit 			POWER_CONDITIONS, 
						SCSICmdField1Bit 			LOEJ, 
						SCSICmdField1Bit 			START, 
						SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->START_STOP_UNIT(
				scsiRequest,
				IMMED, 
				POWER_CONDITIONS, 
				LOEJ, 
				START, 
				CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::SYNCHRONIZE_CACHE( 
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED, 
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS, 
						SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->SYNCHRONIZE_CACHE(
				scsiRequest,
				IMMED, 
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				NUMBER_OF_BLOCKS, 
				CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::UPDATE_BLOCK( 
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->UPDATE_BLOCK(
				scsiRequest,
				dataBuffer,
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::VERIFY_10( 
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			DPO, 
						SCSICmdField1Bit 			BLKVFY, 
						SCSICmdField1Bit 			BYTCHK, 
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField2Byte 			VERIFICATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->VERIFY_10(
				scsiRequest,
				DPO, 
				BLKVFY, 
				BYTCHK, 
				RELADR,
				LOGICAL_BLOCK_ADDRESS, 
				VERIFICATION_LENGTH, 
				CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::VERIFY_12( 
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			DPO, 
						SCSICmdField1Bit 			BLKVFY, 
						SCSICmdField1Bit 			BYTCHK, 
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField4Byte 			VERIFICATION_LENGTH, 
						SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->VERIFY_12(
				scsiRequest,
				DPO, 
				BLKVFY, 
				BYTCHK, 
				RELADR,
				LOGICAL_BLOCK_ADDRESS, 
				VERIFICATION_LENGTH, 
				CONTROL );
}

// The WRITE(6) command as defined in section 6.1.17
bool	
IOSCSIBlockCommandsDevice::WRITE_6(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			UInt32						blockSize,
		    			SCSICmdField2Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField1Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->WRITE_6(
				scsiRequest,
				dataBuffer,
    			LOGICAL_BLOCK_ADDRESS, 
    			TRANSFER_LENGTH, 
    			CONTROL );
}


bool	
IOSCSIBlockCommandsDevice::WRITE_10(
						SCSITaskIdentifier			request,
		   				IOMemoryDescriptor 		 	*dataBuffer, 
		    			UInt32						blockSize,
		    			SCSICmdField1Bit 			DPO, 
		    			SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			EBP, 
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField2Byte 			TRANSFER_LENGTH, 
						SCSICmdField1Byte 			CONTROL )
{
	UInt32 			requestedByteCount;
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	// Check the validity of the media
	if ( blockSize == 0 )
	{
		// There is no media in the device, or it has an undetermined
		// blocksize (could be unformatted).
		return false;
	}

	// Make sure that we were given a valid buffer
	if (dataBuffer == NULL )
	{
		return false;
	}
	else
	{
		// We have a valid buffer object, check that it has the required
		// capcity for the data to be transfered.
		requestedByteCount = TRANSFER_LENGTH * blockSize;
		
		// We know the number of bytes to transfer, now check that the 
		// buffer is large ebnough to accomodate thuis request.
		if ( dataBuffer->getLength() < requestedByteCount )
		{
			return false;
		}
	}

	return GetSCSIBlockCommandObject()->WRITE_10(
				scsiRequest,
				dataBuffer,
				requestedByteCount,
    			DPO, 
    			FUA,
				EBP, 
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				TRANSFER_LENGTH, 
				CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::WRITE_12(
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 		 	*dataBuffer, 
			    		UInt32						blockSize,
		    			SCSICmdField1Bit 			DPO, 
		    			SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			EBP, 
						SCSICmdField1Bit 			RELADR, 
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
						SCSICmdField4Byte 			TRANSFER_LENGTH, 
						SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->WRITE_12(
				scsiRequest,
				dataBuffer,
    			DPO, 
    			FUA,
				EBP, 
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				TRANSFER_LENGTH, 
				CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::WRITE_AND_VERIFY_10(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			UInt32						blockSize,
		    			SCSICmdField1Bit 			DPO,
		    			SCSICmdField1Bit 			EBP, 
		    			SCSICmdField1Bit 			BYTCHK, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->WRITE_AND_VERIFY_10(
				scsiRequest,
				dataBuffer,
    			DPO, 
				EBP, 
	    		BYTCHK, 
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				TRANSFER_LENGTH, 
				CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::WRITE_AND_VERIFY_12(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			UInt32						blockSize,
		    			SCSICmdField1Bit 			DPO,
		    			SCSICmdField1Bit 			EBP, 
		    			SCSICmdField1Bit 			BYTCHK, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField4Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->WRITE_AND_VERIFY_12(
				scsiRequest,
				dataBuffer,
    			DPO, 
				EBP,
				BYTCHK, 
				RELADR, 
				LOGICAL_BLOCK_ADDRESS, 
				TRANSFER_LENGTH, 
				CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::WRITE_LONG(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->WRITE_LONG(
				scsiRequest,
				dataBuffer,
    			RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			TRANSFER_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::WRITE_SAME(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			PBDATA, 
		    			SCSICmdField1Bit 			LBDATA, 
		    			SCSICmdField1Bit 			RELADR, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->WRITE_SAME(
				scsiRequest,
				dataBuffer,
    			PBDATA, 
    			LBDATA, 
    			RELADR, 
    			LOGICAL_BLOCK_ADDRESS, 
    			TRANSFER_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::XDREAD(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->XDREAD(
				scsiRequest,
				dataBuffer,
    			LOGICAL_BLOCK_ADDRESS, 
    			TRANSFER_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::XDWRITE(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			DPO, 
		    			SCSICmdField1Bit 			FUA, 
		    			SCSICmdField1Bit 			DISABLE_WRITE, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->XDWRITE(
				scsiRequest,
				dataBuffer,
    			DPO, 
    			FUA, 
    			DISABLE_WRITE, 
    			LOGICAL_BLOCK_ADDRESS, 
    			TRANSFER_LENGTH, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::XDWRITE_EXTENDED(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer,
		    			SCSICmdField1Bit 			TABLE_ADDRESS, 
		    			SCSICmdField1Bit 			DPO, 
		    			SCSICmdField1Bit 			FUA, 
		    			SCSICmdField1Bit 			DISABLE_WRITE,
		    			SCSICmdField2Bit 			PORT_CONTROL, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField4Byte 			SECONDARY_BLOCK_ADDRESS, 
		    			SCSICmdField4Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			SECONDARY_ADDRESS, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->XDWRITE_EXTENDED(
				scsiRequest,
				dataBuffer,
    			TABLE_ADDRESS, 
    			DPO, 
    			FUA, 
    			DISABLE_WRITE,
    			PORT_CONTROL, 
    			LOGICAL_BLOCK_ADDRESS, 
    			SECONDARY_BLOCK_ADDRESS, 
    			TRANSFER_LENGTH, 
    			SECONDARY_ADDRESS, 
    			CONTROL );
}

bool	
IOSCSIBlockCommandsDevice::XPWRITE(
						SCSITaskIdentifier			request,
		    			IOMemoryDescriptor 		 	*dataBuffer, 
		    			SCSICmdField1Bit 			DPO, 
		    			SCSICmdField1Bit 			FUA, 
		    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
		    			SCSICmdField2Byte 			TRANSFER_LENGTH, 
		    			SCSICmdField1Byte 			CONTROL )
{
	SCSITask *		scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIBlockCommandObject()->XPWRITE(
				scsiRequest,
				dataBuffer,
    			DPO, 
    			FUA, 
    			LOGICAL_BLOCK_ADDRESS, 
    			TRANSFER_LENGTH, 
    			CONTROL );
}


OSMetaClassDefineReservedUsed( IOSCSIBlockCommandsDevice, 1 );	/* PowerDownHandler */

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 2 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 3 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 4 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 5 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 6 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 7 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 8 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 9 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 10 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 11 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 12 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 13 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 14 );
OSMetaClassDefineReservedUnused( IOSCSIBlockCommandsDevice, 15 );
