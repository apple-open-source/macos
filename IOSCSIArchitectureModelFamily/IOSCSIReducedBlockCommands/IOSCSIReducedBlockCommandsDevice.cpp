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
#include <IOKit/scsi-commands/IOSCSIReducedBlockCommandsDevice.h>
#include <IOKit/scsi-commands/IOReducedBlockServices.h>


#if ( SCSI_RBC_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_RBC_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_RBC_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif

#define kMaxRetryCount 8

#define super IOSCSIPrimaryCommandsDevice
OSDefineMetaClass ( IOSCSIReducedBlockCommandsDevice, IOSCSIPrimaryCommandsDevice );
OSDefineAbstractStructors ( IOSCSIReducedBlockCommandsDevice, IOSCSIPrimaryCommandsDevice );

#pragma mark - Public Methods

bool 
IOSCSIReducedBlockCommandsDevice::InitializeDeviceSupport( void )
{
	
	bool setupSuccessful 	= false;
	
	// Initialize the device characteristics flags
	fMediaIsRemovable 		= false;
	
	// Initialize the medium characteristics
	fMediaChanged			= false;
	fMediaPresent			= false;
	fMediaIsRemovable 		= false;
	fMediaIsWriteProtected 	= true;
	
    STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::InitializeDeviceSupport called\n" ) );
	
	// Make sure the drive is ready for us!
	if ( ClearNotReadyStatus ( ) == false )
	{
		goto ERROR_EXIT;
	}
	
	setupSuccessful = DetermineDeviceCharacteristics ( );
	
	if ( setupSuccessful == true ) 
	{		
		
		fPollingThread = thread_call_allocate (
						( thread_call_func_t ) IOSCSIReducedBlockCommandsDevice::sPollForMedia,
						( thread_call_param_t ) this );
		
		if ( fPollingThread == NULL )
		{
			
			ERROR_LOG ( ( "fPollingThread allocation failed.\n" ) );
			setupSuccessful = false;
			goto ERROR_EXIT;
			
		}
		
		InitializePowerManagement ( GetProtocolDriver ( ) );
		
	}
	
	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::InitializeDeviceSupport setupSuccessful = %d\n", setupSuccessful ) );
	
	return setupSuccessful;
	
	
ERROR_EXIT:
	
	
	return setupSuccessful;
	
}

void 
IOSCSIReducedBlockCommandsDevice::StartDeviceSupport( void )
{
	if( fMediaIsRemovable == false )
	{
		UInt32	attempts = 0;
		
		// We have a fixed disk, so make sure we determine its state
		// before we create the layer above us.

		
		do {
			
			PollForMedia();
		
		} while ( ( fMediaPresent == false ) && 
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
IOSCSIReducedBlockCommandsDevice::SuspendDeviceSupport( void )
{
    DisablePolling();
}

void 
IOSCSIReducedBlockCommandsDevice::ResumeDeviceSupport( void )
{
    EnablePolling();
}

void 
IOSCSIReducedBlockCommandsDevice::StopDeviceSupport( void )
{
    DisablePolling();
}

void 
IOSCSIReducedBlockCommandsDevice::TerminateDeviceSupport( void )
{

    STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::cleanUp called.\n" ) );

    if ( fPollingThread != NULL )
    {
        
        thread_call_free ( fPollingThread );
        fPollingThread = NULL;
        
    }

}

bool
IOSCSIReducedBlockCommandsDevice::CreateCommandSetObjects ( void )
{

    STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::CreateCommandSetObjects called\n" ) );
	
    fSCSIReducedBlockCommandObject = SCSIReducedBlockCommands::CreateSCSIReducedBlockCommandObject ( );
    if ( fSCSIReducedBlockCommandObject == NULL )
	{
		ERROR_LOG ( ( "IOSCSIReducedBlockCommandsDevice::start exiting false, RBC object not created\n" ) );
	 	return false;
	}
	
	return true;
}

void
IOSCSIReducedBlockCommandsDevice::FreeCommandSetObjects ( void )
{

    STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::FreeCommandSetObjects called\n" ) );

	if ( fSCSIReducedBlockCommandObject ) 
	{
		
		fSCSIReducedBlockCommandObject->release ( );
		fSCSIReducedBlockCommandObject = NULL;
  	
	}

}

SCSIReducedBlockCommands *
IOSCSIReducedBlockCommandsDevice::GetSCSIReducedBlockCommandObject ( void )
{
	return fSCSIReducedBlockCommandObject;
}


SCSIPrimaryCommands	*
IOSCSIReducedBlockCommandsDevice::GetSCSIPrimaryCommandObject ( void )
{
	return OSDynamicCast ( SCSIPrimaryCommands, GetSCSIReducedBlockCommandObject ( ) );
}


bool
IOSCSIReducedBlockCommandsDevice::ClearNotReadyStatus ( void )
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
		
		if ( TEST_UNIT_READY ( request ) == true )
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
					
					if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize ) == true )
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
						if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01 ) == true )
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


#pragma mark - Protected Methods


void
IOSCSIReducedBlockCommandsDevice::EnablePolling ( void )
{		

    AbsoluteTime	time;
	
    // No reason to start a thread if we've been termintated
    if ( ( isInactive ( ) == false ) && fPollingThread )
    {
        // Retain ourselves so that this object doesn't go away
        // while we are polling
        
        retain ( );
        
        clock_interval_to_deadline ( 1000, kMillisecondScale, &time );
        thread_call_enter_delayed ( fPollingThread, time );
	}

}


void
IOSCSIReducedBlockCommandsDevice::DisablePolling ( void )
{		
	
	// Cancel the thread if it is running
	if ( thread_call_cancel ( fPollingThread ) )
	{
		
		// It was running, so we balance out the retain()
		// with a release()
		release ( );
		
	}
	
}


bool
IOSCSIReducedBlockCommandsDevice::DetermineDeviceCharacteristics ( void )
{
	
	SCSIServiceResponse				serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8							loop;
	UInt8							inquiryBufferCount = sizeof ( SCSICmd_INQUIRY_StandardData );
    SCSICmd_INQUIRY_StandardData * 	inquiryBuffer = NULL;
	IOMemoryDescriptor *			bufferDesc = NULL;
	SCSITaskIdentifier				request = NULL;
	bool							succeeded = false;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );

	inquiryBuffer = ( SCSICmd_INQUIRY_StandardData * ) IOMalloc ( inquiryBufferCount );
	if ( inquiryBuffer == NULL )
	{
		
		STATUS_LOG ( ( "%s: Couldn't allocate Inquiry buffer.\n", getName ( ) ) );
		goto ErrorExit;
	
	}
	
	bufferDesc = IOMemoryDescriptor::withAddress ( 	inquiryBuffer,
													inquiryBufferCount,
													kIODirectionIn );
	
	if ( bufferDesc == NULL )
	{
		
		ERROR_LOG ( ( "%s: Couldn't alloc Inquiry buffer: ", getName() ) );
		goto ErrorExit;
	
	}
			
	request = GetSCSITask( );
	if ( request == NULL )
	{
		
		goto ErrorExit;
	
	}


	for ( loop = 0; ( loop < kMaxRetryCount ) && ( isInactive ( ) == false ) ; loop++ )
	{
		
		if ( INQUIRY( 	request,
						bufferDesc,
						0,
						0,
						0x00,
						inquiryBufferCount) == true )
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
	
	if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
		( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )

	{
		
		goto ErrorExit;
	
	}

	succeeded = true;

	if ( ( inquiryBuffer->RMB & kINQUIRY_PERIPHERAL_RMB_BitMask ) 
				== kINQUIRY_PERIPHERAL_RMB_MediumRemovable )
	{
		
		STATUS_LOG ( ( "Media is removable\n" ) );
		fMediaIsRemovable = true;
	
	}
	else
	{
		
		STATUS_LOG ( ( "Media is NOT removable\n" ) );
		fMediaIsRemovable = false;
	
	}
	
	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::DetermineDeviceCharacteristics exiting\n" ) );

ErrorExit:
	
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
	
		IOFree( ( void *) inquiryBuffer, inquiryBufferCount );
		inquiryBuffer = NULL;
	
	}

	return succeeded;
	
}


void
IOSCSIReducedBlockCommandsDevice::SetMediaCharacteristics ( UInt32 blockSize, UInt32 blockCount )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	STATUS_LOG ( ( "mediaBlockSize = %ld, blockCount = %ld\n", blockSize, blockCount ) );
	
	fMediaBlockSize		= blockSize;
	fMediaBlockCount	= blockCount;
	
	STATUS_LOG ( ( "%s::%s exiting\n", getName ( ), __FUNCTION__ ) );
	
}


void
IOSCSIReducedBlockCommandsDevice::ResetMediaCharacteristics ( void )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	fMediaBlockSize		= 0;
	fMediaBlockCount	= 0;
	fMediaPresent		= false;
	
	STATUS_LOG ( ( "%s::%s exiting\n", getName ( ), __FUNCTION__ ) );
	
}


void
IOSCSIReducedBlockCommandsDevice::CreateStorageServiceNub ( void )
{
	
	STATUS_LOG ( ( "%s::%s entering.\n", getName ( ), __FUNCTION__ ) );
	
	IOService * 	nub = new IOReducedBlockServices;
	if ( nub == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIReducedBlockCommandsDevice::createStorageServiceNub failed\n" ) );
	
	}
	
	nub->init ( );
	
	if ( !nub->attach ( this ) )
	{
		
		// panic since the nub can't attach
		PANIC_NOW ( ( "IOSCSIReducedBlockCommandsDevice::createStorageServiceNub unable to attach nub" ) );
		
	}
	
	nub->registerService ( );
	
	STATUS_LOG ( ( "%s::%s exiting.\n", getName ( ), __FUNCTION__ ) );
	
	nub->release();
}


void
IOSCSIReducedBlockCommandsDevice::PollForMedia ( void )
{

	SCSIServiceResponse			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSI_Sense_Data				senseBuffer;
	UInt32						capacityData[2];
	IOMemoryDescriptor *		bufferDesc;
	SCSITaskIdentifier			request;
	bool						mediaFound = false;
	bool						validSense;
	SCSITaskStatus 				taskStatus;
		
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
													kSenseDefaultSize,
													kIODirectionIn );
	
	if ( bufferDesc == NULL )
	{
		return;
	}
	
	request = GetSCSITask ( );
	
	// Do a TEST_UNIT_READY to generate sense data
	if ( TEST_UNIT_READY ( request ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::PollForMedia malformed command" ));
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			validSense = GetAutoSenseData( request, &senseBuffer );
			if ( validSense == false )
			{
				
				// Get the sense data to determine if media is present.
				// This will eventually use the autosense data if the
				// Transport Protocol supports it else issue the REQUEST_SENSE.          
				if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize ) == false )
			    {
			    	// The command was successfully built, now send it
			    	serviceResponse = SendCommand ( request, 0 );
				}
				else
				{
					PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::PollForMedia malformed command" ));
				}
			}
			
			if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x00 ) && 
				( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
			{
				
				mediaFound = true;
			
			}
			
		}
		
		else
		{
			
			mediaFound = true;
		
		}
		
	}
	
	bufferDesc->release ( );

	if ( mediaFound == false )
	{
		ReleaseSCSITask ( request );
		return;
	}
	
	// If we got here, then we have found media
	if ( fMediaIsRemovable == true )
	{
		
		// Lock removable media
		if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, 1 ) == true )
	    {
	    	// The command was successfully built, now send it
	    	serviceResponse = SendCommand ( request, 0 );
		}
		else
		{
			PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::PollForMedia malformed command" ));
		}
	}
		
	bufferDesc = IOMemoryDescriptor::withAddress ( 	capacityData,
													8,
													kIODirectionIn );
		
	// We found media, Get its capacity
	if ( READ_CAPACITY ( request, bufferDesc ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::PollForMedia malformed command" ));
	}
	
	taskStatus = GetTaskStatus ( request );
	ReleaseSCSITask ( request );	
	bufferDesc->release ( );
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( taskStatus == kSCSITaskStatus_GOOD ) )
	{
		
		SetMediaCharacteristics ( OSSwapBigToHostInt32 ( capacityData[1] ), OSSwapBigToHostInt32 ( capacityData[0] ) + 1 );
		STATUS_LOG ( ( "%s: Media capacity: %x and block size: %x\n",
						getName ( ), fMediaBlockCount, fMediaBlockSize ) );
		
	}
		
	else
	{
		ERROR_LOG ( ( "%s: Read Capacity failed\n", getName() ) );
		return;
	}
	
	CheckWriteProtection ( );
	
	fMediaPresent	= true;
	fMediaChanged	= true;
	
	// Message up the chain that we have media
	messageClients ( kIOMessageMediaStateHasChanged,
					 ( void * ) kIOMediaStateOnline,
					 sizeof ( IOMediaState ) );
	
}


void
IOSCSIReducedBlockCommandsDevice::CheckWriteProtection ( void )
{
	
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					modeSenseBuffer[17];
	IOMemoryDescriptor *	bufferDesc;
	SCSITaskIdentifier		request;
	SCSITaskStatus			taskStatus;

	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::checkWriteProtection called\n" ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( 	modeSenseBuffer,
													17,
													kIODirectionIn );
	
	request = GetSCSITask ( );

	if ( MODE_SENSE_6 ( request,
						bufferDesc,
						0x01,	/* Disable block descriptors */
						0x00,
						0x06,
						17 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::CheckWriteProtection malformed command" ));
	}

	taskStatus = GetTaskStatus ( request );
	ReleaseSCSITask ( request );
	bufferDesc->release ( );

	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( taskStatus == kSCSITaskStatus_GOOD ) )
	{
		
		STATUS_LOG ( ( "%s: Returned Mode sense data: ", getName ( ) ) );
		
		#if ( SCSI_RBC_DEVICE_DEBUGGING_LEVEL >=3 )
			for ( UInt32 i = 0; i < 17; i++ )
			{
				STATUS_LOG ( ( "%x: ", modeSenseBuffer[i] ) );
			}
	
	        STATUS_LOG ( ( "\n" ) );
		#endif // DEBUG
		
		if ( ( modeSenseBuffer[15] & 0x04 ) != 0 )
		{
			
			fMediaIsWriteProtected = true;
			
		}
		
		else
		{
			
			fMediaIsWriteProtected = false;
		
		}
	
	}
	
	else
	{
		
		STATUS_LOG ( ( "%s: Mode Sense failed with service response = %x\n", getName ( ), serviceResponse ) );
		
		// The mode sense failed, mark as write protected to be safe.
	 	fMediaIsWriteProtected = true;
	
	}
}


void 
IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete ( SCSITaskIdentifier request )
{
	
	void *								clientData;
	IOReturn							status;
	UInt64								actCount = 0;
	IOSCSIReducedBlockCommandsDevice *	taskOwner;
	SCSITask *							scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete scsiRequest==NULL." ));
	}

	// Extract the client data from the SCSITask	
	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete retrieve clientData.\n" ) );
	clientData	= scsiRequest->GetApplicationLayerReference();
	
	if ( ( scsiRequest->GetServiceResponse ( ) == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( scsiRequest->GetTaskStatus ( ) == kSCSITaskStatus_GOOD ) )
	{

		// Our status is good, so return a success
		status = kIOReturnSuccess;
		actCount = scsiRequest->GetRealizedDataTransferCount ( );
	
	}
	
	else
	{
		
		status = kIOReturnError;
		
	}
	
	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete retrieve taskowner.\n" ) );
	taskOwner = OSDynamicCast ( IOSCSIReducedBlockCommandsDevice, scsiRequest->GetTaskOwner ( ) );
	if ( taskOwner == NULL )
	{
		PANIC_NOW ( ( "IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete taskOwner==NULL." ) );
	}
	
	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete release SCSITask.\n" ) );
	taskOwner->ReleaseSCSITask ( request );
	
	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete call IOBlockStorageServices::Complete.\n" ) );
	IOReducedBlockServices::AsyncReadWriteComplete( clientData, status, actCount );
}


// Perform the Synchronous Read Request
IOReturn
IOSCSIReducedBlockCommandsDevice::IssueRead ( 	IOMemoryDescriptor *	buffer,
												UInt64					startBlock,
												UInt64					blockCount )
{
	
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request;
	IOReturn				status = kIOReturnError;
	
	STATUS_LOG ( ( "%s: syncRead Attempted\n", getName() ) );
	
	request = GetSCSITask ( );
	
	if ( READ_10 ( 	request,
					buffer,
                 	fMediaBlockSize,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::IssueRead malformed command" ));
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		status = kIOReturnSuccess;
	
	}

	ReleaseSCSITask ( request );
	
	return status;
	
}


// Perform the Asynchronous Read Request
IOReturn
IOSCSIReducedBlockCommandsDevice::IssueRead ( 	IOMemoryDescriptor *	buffer,
												UInt64					startBlock,
												UInt64					blockCount,
												void *					clientData )
{
	
	IOReturn 				status = kIOReturnSuccess;
	SCSITaskIdentifier		request;

	STATUS_LOG ( ( "%s: asyncRead Attempted\n", getName() ) );
	
	// For now, let's do the async request synchronously
	request = GetSCSITask ( );
	
	if ( READ_10 (	request,
					buffer,
                    fMediaBlockSize,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount ) == true )
    {
		
    	SetApplicationLayerReference( request, clientData );
    	SendCommand ( request, 0, &IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete );
		
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIReducedBlockCommandsDevice::IssueRead malformed command" ) );
	}
	
	return status;
	
}


// Perform the Synchronous Write Request
IOReturn
IOSCSIReducedBlockCommandsDevice::IssueWrite (	IOMemoryDescriptor *	buffer,
												UInt64					startBlock,
												UInt64					blockCount )
{
	
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request;
	IOReturn				status = kIOReturnError;
	
	STATUS_LOG ( ( "%s: syncWrite Attempted\n", getName ( ) ) );
	
	request = GetSCSITask ( );
	
	if (  WRITE_10 (	request,
						buffer,
						fMediaBlockSize,
						0,
						( SCSICmdField4Byte ) startBlock,
						( SCSICmdField2Byte ) blockCount ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::IssueWrite malformed command" ));
	}

	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		status = kIOReturnSuccess;
	
	}

	ReleaseSCSITask ( request );
	
	return status;
	
}


// Perform the Asynchronous Write Request
IOReturn
IOSCSIReducedBlockCommandsDevice::IssueWrite (	IOMemoryDescriptor *	buffer,
												UInt64					startBlock,
												UInt64					blockCount,
												void *					clientData )
{
	
	IOReturn				status		= kIOReturnSuccess;
	SCSITaskIdentifier		request;
	
	STATUS_LOG ( ( "%s: asyncWrite Attempted\n", getName() ) );

	request = GetSCSITask ( );
	
	if (  WRITE_10 ( 	request, 
						buffer,
	 					fMediaBlockSize,
						0,
						( SCSICmdField4Byte ) startBlock,
						( SCSICmdField2Byte ) blockCount ) == true )
    {

    	SetApplicationLayerReference( request, clientData );
    	SendCommand ( request, 0, &IOSCSIReducedBlockCommandsDevice::AsyncReadWriteComplete );

	}
	else
	{
		PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::IssueWrite malformed command" ));
	}
	
	return status;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::SyncReadWrite (	IOMemoryDescriptor *	buffer,
													UInt64					startBlock,
													UInt64					blockCount )
{
	
	IODirection		direction;
	IOReturn		status;
	
	direction = buffer->getDirection ( );
	
	if ( direction == kIODirectionIn )
	{
		
		status = IssueRead ( buffer, startBlock, blockCount );
	
	}
	
	else if ( direction == kIODirectionOut )
	{
		
		status = IssueWrite ( buffer, startBlock, blockCount );
	
	}
	
	else
	{
		
		ERROR_LOG ( ( "%s: SyncReadWrite bad direction argument\n", getName ( ) ) );
		status = kIOReturnBadArgument;
	
	}
	
	return status;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::AsyncReadWrite ( 	IOMemoryDescriptor *	buffer,
													UInt64					startBlock,
													UInt64					blockCount,
													void *					clientData )
{
	
	IODirection		direction;
	IOReturn		status;
	
	direction = buffer->getDirection ( );
	
	if ( direction == kIODirectionIn )
	{
		
		IssueRead ( buffer, startBlock, blockCount, clientData );
		status = kIOReturnSuccess;
		
	}
	
	else if ( direction == kIODirectionOut )
	{
		
		IssueWrite ( buffer, startBlock, blockCount, clientData );
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		
		ERROR_LOG ( ( "%s: AsyncReadWrite bad direction argument\n", getName ( ) ) );
		status = kIOReturnBadArgument;
		
	}
	
	return status;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::EjectTheMedia ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request;

    STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );

    if ( fMediaIsRemovable == false )
	{
		
		if ( SYNCHRONIZE_CACHE( request ) == true )
	    {
	    	// The command was successfully built, now send it
	    	serviceResponse = SendCommand ( request, 0 );
		}
		else
		{
			PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::EjectTheMedia malformed command" ));
		}
	
		ReleaseSCSITask ( request );
		return kIOReturnSuccess;
		
	}
	
	if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, 0 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::EjectTheMedia malformed command" ));
	}

	if ( START_STOP_UNIT ( request, 0, 0, 1, 0 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::EjectTheMedia malformed command" ));
	}
	
	ReleaseSCSITask ( request );
	
	ResetMediaCharacteristics ( );
	fMediaIsWriteProtected = true;
	
	EnablePolling ( );
		
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::FormatMedia ( UInt64 byteCapacity )
{
	
	IOReturn	status = kIOReturnUnsupported;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	return status;
	
}


UInt32
IOSCSIReducedBlockCommandsDevice::GetFormatCapacities (	UInt64 * capacities,
														UInt32   capacitiesMaxCount ) const
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	return 0;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::LockUnlockMedia ( bool doLock )
{
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::SynchronizeCache ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request;
	IOReturn				status = kIOReturnError;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );
	
	if ( SYNCHRONIZE_CACHE ( request ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOSCSIReducedBlockCommandsDevice::SynchronizeCache malformed command" ));
	}

	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		status = kIOReturnSuccess;
	}

	ReleaseSCSITask ( request );
	
	return status;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::ReportBlockSize ( UInt64 * blockSize )
{
	
	STATUS_LOG ( ( "%s::%s ReportBlockSize blockSize = %ld\n",
				 getName ( ), __FUNCTION__, ( UInt32 ) fMediaBlockSize ) );
	
	*blockSize = fMediaBlockSize;
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::ReportEjectability ( bool * isEjectable )
{
	
	STATUS_LOG ( ( "%s::%s ReportEjectability mediaIsRemovable = %d\n",
					getName ( ), __FUNCTION__, fMediaIsRemovable ) );
	
	*isEjectable = fMediaIsRemovable;
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::ReportLockability ( bool * isLockable )
{
	
	STATUS_LOG ( ( "%s::%s isLockable = %d\n", getName ( ), __FUNCTION__, true ) );
	
	*isLockable = true;
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::ReportPollRequirements ( 	bool * pollIsRequired,
															bool * pollIsExpensive )
{
	
	STATUS_LOG ( ( "%s::%s called \n", getName ( ), __FUNCTION__ ) );
	
//	*pollIsRequired 	= fMediaIsRemovable;
	*pollIsRequired 	= false;
	*pollIsExpensive 	= false;
	
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::ReportMaxReadTransfer (	UInt64		blockSize,
															UInt64 * 	max )
{
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
		
	*max = blockSize * 256;
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::ReportMaxValidBlock ( UInt64 * maxBlock )
{
	
	STATUS_LOG ( ( "%s::%s maxBlock = %ld\n", getName ( ),
					__FUNCTION__, fMediaBlockCount - 1 ) );
	
    *maxBlock = fMediaBlockCount - 1;
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::ReportMaxWriteTransfer ( 	UInt64		blockSize,
															UInt64 *	max )
{
	
	STATUS_LOG ( ( "%s::%s called.\n", getName ( ), __FUNCTION__ ) );
	
	return ( ReportMaxReadTransfer ( blockSize, max ) );
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::ReportMediaState (	bool *	mediaPresent,
														bool *	changed )
{
	
	STATUS_LOG ( ( "%s::%s called.\n", getName ( ), __FUNCTION__ ) );
	
	*mediaPresent 	= fMediaPresent;
	*changed 		= fMediaChanged;
	
	//´´´ HACK - Make sure that the next time they call ReportMediaState,
	// we don't tell them the media changed, else we'll panic.
	if ( fMediaChanged )
	{
		
		fMediaChanged = !fMediaChanged;
		
	}
	
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::ReportRemovability ( bool * isRemovable )
{
	
	STATUS_LOG ( ( "%s::%s isRemovable = %d.\n", getName ( ),
					__FUNCTION__, fMediaIsRemovable ) );
	
	*isRemovable = fMediaIsRemovable;
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIReducedBlockCommandsDevice::ReportWriteProtection ( bool * isWriteProtected )
{

	STATUS_LOG ( ( "%s::%s isWriteProtected = %d.\n", getName ( ),
					__FUNCTION__, fMediaIsWriteProtected ) );	

    *isWriteProtected = fMediaIsWriteProtected;
	return kIOReturnSuccess;
	
}


void
IOSCSIReducedBlockCommandsDevice::sPollForMedia ( void * pdtDriver, void * refCon )
{
	
	IOSCSIReducedBlockCommandsDevice *	driver;
	
	driver = ( IOSCSIReducedBlockCommandsDevice * ) pdtDriver;
	
	driver->PollForMedia ( );
	
	if ( !driver->fMediaPresent )
	{
		
		// schedule the poller again since we didn't find media
		driver->EnablePolling ( );
		
	}
	
	// drop the retain associated with this poll
	driver->release ( );
	
}


#pragma mark -
#pragma mark Reduced Block Commands Builders


bool
IOSCSIReducedBlockCommandsDevice::FORMAT_UNIT(
						SCSITaskIdentifier			request,
						SCSICmdField1Bit			IMMED,
   						SCSICmdField1Bit			PROGRESS,
   						SCSICmdField1Bit			PERCENT_TIME,
   						SCSICmdField1Bit			INCREMENT )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
		
	return GetSCSIReducedBlockCommandObject()->FORMAT_UNIT(
											scsiRequest,
											IMMED,
											PROGRESS,
											PERCENT_TIME,
											INCREMENT );
}


bool
IOSCSIReducedBlockCommandsDevice::INQUIRY(
						SCSITaskIdentifier			request,
    					IOMemoryDescriptor *		dataBuffer,
    					SCSICmdField1Bit			CMDDT, 
    					SCSICmdField1Bit			EVPD, 
    					SCSICmdField1Byte			PAGE_OR_OPERATION_CODE,
    					SCSICmdField1Byte			ALLOCATION_LENGTH )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}

	return GetSCSIPrimaryCommandObject()->INQUIRY( 	
								scsiRequest,
								dataBuffer,
								CMDDT,
								EVPD,
								PAGE_OR_OPERATION_CODE,
								ALLOCATION_LENGTH,
								0x00 );
}


bool
IOSCSIReducedBlockCommandsDevice::MODE_SELECT_6(
						SCSITaskIdentifier			request,
    					IOMemoryDescriptor *		dataBuffer,
    					SCSICmdField1Bit 			PF,
    					SCSICmdField1Bit 			SP,
    					SCSICmdField1Byte 			PARAMETER_LIST_LENGTH )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIPrimaryCommandObject()->MODE_SELECT_6(
									scsiRequest,
									dataBuffer,
									PF,
									SP,
									PARAMETER_LIST_LENGTH,
									0x00 );
}


bool
IOSCSIReducedBlockCommandsDevice::MODE_SENSE_6(
						SCSITaskIdentifier			request,
    					IOMemoryDescriptor *		dataBuffer,
    					SCSICmdField1Bit 			DBD,
	   					SCSICmdField2Bit 			PC,
	   					SCSICmdField6Bit 			PAGE_CODE,
	   					SCSICmdField1Byte 			ALLOCATION_LENGTH )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIPrimaryCommandObject()->MODE_SENSE_6(
									scsiRequest,
									dataBuffer,
									DBD,
									PC,
									PAGE_CODE,
									ALLOCATION_LENGTH,
									0x00 );	
}


bool
IOSCSIReducedBlockCommandsDevice::PERSISTENT_RESERVE_IN(
						SCSITaskIdentifier			request,
   						IOMemoryDescriptor *		dataBuffer,
	   					SCSICmdField5Bit 			SERVICE_ACTION, 
	   					SCSICmdField2Byte 			ALLOCATION_LENGTH )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIPrimaryCommandObject()->PERSISTENT_RESERVE_IN(
											scsiRequest,
											dataBuffer,
											SERVICE_ACTION,
											ALLOCATION_LENGTH,
											0x00 );
}


bool
IOSCSIReducedBlockCommandsDevice::PERSISTENT_RESERVE_OUT (
						SCSITaskIdentifier			request,
    					IOMemoryDescriptor *		dataBuffer,
	   					SCSICmdField5Bit			SERVICE_ACTION,
	   					SCSICmdField4Bit			SCOPE,
	   					SCSICmdField4Bit			TYPE )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIPrimaryCommandObject()->PERSISTENT_RESERVE_OUT(
											scsiRequest,
											dataBuffer,
											SERVICE_ACTION,
											SCOPE,
											TYPE,
											0x00 );
}


bool
IOSCSIReducedBlockCommandsDevice::PREVENT_ALLOW_MEDIUM_REMOVAL(
						SCSITaskIdentifier			request,
    					SCSICmdField2Bit			PREVENT )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIPrimaryCommandObject()->PREVENT_ALLOW_MEDIUM_REMOVAL(
									scsiRequest,
									PREVENT,
									0x00 );
}


bool
IOSCSIReducedBlockCommandsDevice::READ_10(
						SCSITaskIdentifier			request,
    					IOMemoryDescriptor *		dataBuffer,
			    		UInt32						blockSize,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte			TRANSFER_LENGTH )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIReducedBlockCommandObject()->READ_10(
											scsiRequest,
											dataBuffer,
											blockSize,
											LOGICAL_BLOCK_ADDRESS,
											TRANSFER_LENGTH );
}


bool
IOSCSIReducedBlockCommandsDevice::READ_CAPACITY(
						SCSITaskIdentifier			request,
    					IOMemoryDescriptor *		dataBuffer )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIReducedBlockCommandObject()->READ_CAPACITY(
											scsiRequest,
											dataBuffer );
}


bool
IOSCSIReducedBlockCommandsDevice::RELEASE_6( 
						SCSITaskIdentifier			request )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIPrimaryCommandObject()->RELEASE_6( 
												scsiRequest,
												0x00 );
}


bool
IOSCSIReducedBlockCommandsDevice::REQUEST_SENSE( 
						SCSITaskIdentifier			request,
   						IOMemoryDescriptor *		dataBuffer,
			    		SCSICmdField1Byte 			ALLOCATION_LENGTH )
{	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIPrimaryCommandObject()->REQUEST_SENSE(
								scsiRequest,
								dataBuffer,
								ALLOCATION_LENGTH,
								0x00 );
}


bool
IOSCSIReducedBlockCommandsDevice::RESERVE_6(
 						SCSITaskIdentifier			request )
{	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIPrimaryCommandObject()->RESERVE_6( 
								scsiRequest,
								0x00 );
}


bool
IOSCSIReducedBlockCommandsDevice::START_STOP_UNIT(
						SCSITaskIdentifier			request,
    					SCSICmdField1Bit			IMMED,
						SCSICmdField4Bit			POWER_CONDITIONS,
						SCSICmdField1Bit			LOEJ,
						SCSICmdField1Bit			START )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIReducedBlockCommandObject()->START_STOP_UNIT(
											scsiRequest,
											IMMED,
											POWER_CONDITIONS,
											LOEJ,
											START );
}


bool
IOSCSIReducedBlockCommandsDevice::SYNCHRONIZE_CACHE(
 						SCSITaskIdentifier			request )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIReducedBlockCommandObject()->SYNCHRONIZE_CACHE( scsiRequest );
}


bool
IOSCSIReducedBlockCommandsDevice::TEST_UNIT_READY( 
						SCSITaskIdentifier			request )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIPrimaryCommandObject()->TEST_UNIT_READY( scsiRequest, 0x00 );
}


bool
IOSCSIReducedBlockCommandsDevice::VERIFY(
						SCSITaskIdentifier			request,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			VERIFICATION_LENGTH )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIReducedBlockCommandObject()->VERIFY(
										scsiRequest,
										LOGICAL_BLOCK_ADDRESS,
										VERIFICATION_LENGTH );
}


bool
IOSCSIReducedBlockCommandsDevice::WRITE_10(
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
			    		UInt32						blockSize,
						SCSICmdField1Bit       		FUA,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIReducedBlockCommandObject()->WRITE_10(
										scsiRequest,
										dataBuffer,
										blockSize,
										FUA,
										LOGICAL_BLOCK_ADDRESS,
										TRANSFER_LENGTH );
}


bool
IOSCSIReducedBlockCommandsDevice::WRITE_BUFFER( 
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Bit 			MODE,
						SCSICmdField1Byte 			BUFFER_ID,
						SCSICmdField3Byte 			BUFFER_OFFSET,
						SCSICmdField3Byte 			PARAMETER_LIST_LENGTH )
{
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName(), __FUNCTION__ ) );

	scsiRequest = OSDynamicCast( SCSITask, request );

	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	return GetSCSIPrimaryCommandObject()->WRITE_BUFFER(
								scsiRequest,
								dataBuffer,
								MODE,
								BUFFER_ID,
								BUFFER_OFFSET,
								PARAMETER_LIST_LENGTH,
								0x00 );
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 1 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 2 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 3 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 4 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 5 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 6 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 7 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 8 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 9 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 10 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 11 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 12 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 13 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 14 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 15 );
OSMetaClassDefineReservedUnused( IOSCSIReducedBlockCommandsDevice, 16 );
