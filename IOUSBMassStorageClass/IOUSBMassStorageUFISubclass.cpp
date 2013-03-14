/*
 * Copyright (c) 1998-2012 Apple Inc. All rights reserved.
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


//--------------------------------------------------------------------------------------------------
//	Includes
//--------------------------------------------------------------------------------------------------

// This class' header file
#include "IOUSBMassStorageUFISubclass.h"

#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/usb/IOUFIStorageServices.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>

#include <IOKit/scsi/SCSITask.h>

// IOKit Power Management headers
#include <IOKit/pwr_mgt/RootDomain.h>

#include "Debugging.h"


//--------------------------------------------------------------------------------------------------
//	Macros
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
//	Constants
//--------------------------------------------------------------------------------------------------

#define kKeySwitchProperty			"Keyswitch"
#define kAppleKeySwitchProperty		"AppleKeyswitch"

#define super IOSCSIPrimaryCommandsDevice

enum
{
	kIOUSBMassStorageUFIDevicePowerStateSleep 		= 0,
	kIOUSBMassStorageUFIDevicePowerStateActive		= 1,
	kIOUSBMassStorageUFIDeviceNumPowerStates		= 2
};


static IOPMPowerState sPowerStates[kIOUSBMassStorageUFIDeviceNumPowerStates] =
{
	{ kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, IOPMPowerOn, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};


//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------

IOOptionBits	gRestartShutdownFlag = 0;

OSDefineMetaClassAndStructors( IOUSBMassStorageUFISubclass, IOUSBMassStorageClass )


#pragma mark -
#pragma mark *** IOUSBMassStorageUFIDevice declaration ***
#pragma mark -

OSDefineMetaClassAndStructors( IOUSBMassStorageUFIDevice, IOSCSIPrimaryCommandsDevice )


#pragma mark -
#pragma mark *** Static Class Methods ***
#pragma mark -


//--------------------------------------------------------------------------------------------------
//	sProcessPoll -	Gets scheduled to execute the polls.	   						[STATIC][PUBLIC]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::sProcessPoll ( void * theUFIDriver, void * refCon )
{
	UNUSED( refCon );
	
	IOUSBMassStorageUFIDevice *	driver;
	
	driver = (IOUSBMassStorageUFIDevice *) theUFIDriver;
	require_nonzero ( driver, ErrorExit );

	if( driver->fPollingMode != kPollingMode_Suspended )
	{
	
		driver->ProcessPoll();
		
		if( driver->fPollingMode != kPollingMode_Suspended )
		{
			// schedule the poller again
			driver->EnablePolling();
		}

	}
	
	// drop the retain associated with this poll
	driver->release();
	
	
ErrorExit:

	return;
	
}


//--------------------------------------------------------------------------------------------------
//	AsyncReadWriteComplete - Completion routine for I/O							   [STATIC][PRIVATE]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::AsyncReadWriteComplete ( SCSITaskIdentifier request )
{
	void *							clientData;
	IOReturn						status;
	UInt64							actCount = 0;
	IOUSBMassStorageUFIDevice *		taskOwner;
		
		
	if ( request == NULL )
	{
		PANIC_NOW ( ( "IOUSBMassStorageUFIDevice::AsyncReadWriteComplete request==NULL." ) );
	}

	taskOwner = OSDynamicCast ( IOUSBMassStorageUFIDevice, IOSCSIPrimaryCommandsDevice::sGetOwnerForTask ( request ) );
	if ( taskOwner == NULL )
	{
		PANIC_NOW ( ( "IOUSBMassStorageUFIDevice::AsyncReadWriteComplete taskOwner==NULL." ) );
	}

	// Extract the client data from the SCSITask	
	clientData	= taskOwner->GetApplicationLayerReference( request );
	
	if ( ( taskOwner->GetServiceResponse( request ) == kSCSIServiceResponse_TASK_COMPLETE ) &&
		( taskOwner->GetTaskStatus( request ) == kSCSITaskStatus_GOOD ) ) 
	{
		status = kIOReturnSuccess;
	}
	else
	{
	
		STATUS_LOG ( ( 4, "%s[%p]::Error on read/write", taskOwner->getName(), taskOwner ) );
		status = kIOReturnError;
		
	}

	if ( status == kIOReturnSuccess )
	{
		actCount = taskOwner->GetDataBuffer( request )->getLength();
	}
	

	taskOwner->ReleaseSCSITask( request );

	IOUFIStorageServices::AsyncReadWriteComplete( clientData, status, actCount );
	
}


#pragma mark -
#pragma mark *** Class Methods ***
#pragma mark -


//--------------------------------------------------------------------------------------------------
//	InitializeDeviceSupport - Initializes device support								 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::InitializeDeviceSupport ( void )
{
	bool setupSuccessful 	= false;
	
	
	// Initialize the medium characteristics
	fMediumPresent			= false;
	fMediumIsWriteProtected	= true;

    STATUS_LOG ( ( 6, "%s[%p]::InitializeDeviceSupport called", getName(), this ) );

	ClearNotReadyStatus();
	
	fPollingMode = kPollingMode_NewMedia;

	fIOUSBMassStorageUFIDeviceReserved = ( IOUSBMassStorageUFIDeviceExpansionData * )
			IOMalloc ( sizeof ( IOUSBMassStorageUFIDeviceExpansionData ) );
	require_nonzero ( fIOUSBMassStorageUFIDeviceReserved, ErrorExit );

	require ( ( DetermineDeviceCharacteristics( ) == true ), ErrorExit );
	
	fPollingThread = thread_call_allocate (
					( thread_call_func_t ) IOUSBMassStorageUFIDevice::sProcessPoll,
					( thread_call_param_t ) this );
	require_nonzero ( fPollingThread, ErrorExit );

	bzero ( fIOUSBMassStorageUFIDeviceReserved,
			sizeof ( IOUSBMassStorageUFIDeviceExpansionData ) );	

	InitializePowerManagement ( GetProtocolDriver ( ) );

	STATUS_LOG ( ( 5, "%s[%p]::InitializeDeviceSupport setupSuccessful = %d", getName(), this, setupSuccessful ) );
	
	setupSuccessful = true;
	
ErrorExit:

	if ( setupSuccessful == false )
	{
		TerminateDeviceSupport();
	}

	return setupSuccessful;
}


//--------------------------------------------------------------------------------------------------
//	StartDeviceSupport - Starts device support											 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageUFIDevice::StartDeviceSupport ( void )
{
	OSBoolean *		shouldNotPoll = NULL;
	
		
	shouldNotPoll = OSDynamicCast (	OSBoolean,
									getProperty ( kAppleKeySwitchProperty ) );
	
	if ( shouldNotPoll != NULL )
	{
		
		// See if we should not poll.
		require ( shouldNotPoll->isFalse ( ), Exit );
		
	}
	
	// Start polling
	EnablePolling ( );
	
	
Exit:
	
	CreateStorageServiceNub ( );
	
}


//--------------------------------------------------------------------------------------------------
//	SuspendDeviceSupport - Suspends device support										 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::SuspendDeviceSupport ( void )
{
	if( fPollingMode != kPollingMode_Suspended )
	{
    	DisablePolling();
    }		
	
}


//--------------------------------------------------------------------------------------------------
//	ResumeDeviceSupport - Resumes device support										 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::ResumeDeviceSupport ( void )
{
	// The driver has not found media in the device, restart 
	// the polling for new media.
	if( fMediumPresent == false )
	{
		fPollingMode = kPollingMode_NewMedia;

	    EnablePolling();
	}
	
}


//--------------------------------------------------------------------------------------------------
//	StopDeviceSupport - Stops device support											 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageUFIDevice::StopDeviceSupport ( void )
{
	// This is only here to keep the compiler happy since
	// the method is pure virtual.  We don't need it for UFI.
}


//--------------------------------------------------------------------------------------------------
//	TerminateDeviceSupport - Terminates device support									 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::TerminateDeviceSupport ( void )
{
    STATUS_LOG ( ( 6, "%s[%p]::cleanUp called.", getName(), this ) );

    if ( fPollingThread != NULL )
    {
	
        thread_call_free ( fPollingThread );
        fPollingThread = NULL;
		
    }

	if ( fIOUSBMassStorageUFIDeviceReserved != NULL)
	{
		IODelete ( fIOUSBMassStorageUFIDeviceReserved, IOUSBMassStorageUFIDeviceExpansionData, 1 );
		fIOUSBMassStorageUFIDeviceReserved = NULL;
	}
}


//--------------------------------------------------------------------------------------------------
//	ClearNotReadyStatus - Clears any NOT_READY status on device							 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::ClearNotReadyStatus( void )
{
	SCSI_Sense_Data				senseBuffer;
	IOMemoryDescriptor *		bufferDesc;
	SCSITaskIdentifier			request;
	bool						driveReady = false;
	bool						result = true;
	SCSIServiceResponse 		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	
	STATUS_LOG ( ( 6, "%s[%p]::%s called", getName(), this,  __FUNCTION__ ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress (	( void * ) &senseBuffer,
													kSenseDefaultSize,
													kIODirectionIn );
	
	request = GetSCSITask();
	do
	{
	
		if ( TEST_UNIT_READY ( request ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		else
		{
			PANIC_NOW( ( "IOUSBMassStorageUFIDevice::ClearNotReadyStatus malformed command" ) );
		}
		
		if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
		
			bool validSense = false;
			
			if ( GetTaskStatus( request ) == kSCSITaskStatus_CHECK_CONDITION )
			{
			
				validSense = GetAutoSenseData( request, &senseBuffer );
				if ( validSense == false )
				{
				
					if ( REQUEST_SENSE( request, bufferDesc, kSenseDefaultSize ) == true )
					{
						// The command was successfully built, now send it
						serviceResponse = SendCommand ( request, 0 );
					}
					else
					{
						PANIC_NOW ( ( "IOUSBMassStorageUFIDevice::ClearNotReadyStatus malformed command" ) );
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
						
						STATUS_LOG ( ( 5, "%s[%p]::drive not ready", getName(), this ) );
						driveReady = false;
						IOSleep ( 200 );
						
					}
					else if ( ( ( senseBuffer.SENSE_KEY  & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY  ) && 
							( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) )
					{
						// The drive needs to be spun up. Issue a START_STOP_UNIT to it.
						if ( START_STOP_UNIT( request, 0x00, 0x00, 0x01 ) == true )
						{
							serviceResponse = SendCommand( request, 0 );
						}
						
					}
					else
					{
					
						driveReady = true;
						STATUS_LOG ( (5, "%s[%p]::drive READY", getName(), this ) );
						
					}
					
					STATUS_LOG ( ( 5, "%s[%p]:: sense data: %01x, %02x, %02x", getName(), this,
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
			IOSleep( 200 );
			
		}
	
	// check isInactive in case device was hot unplugged during sleep
	// and we are in an infinite loop here
	} while( ( driveReady == false ) && ( isInactive() == false ) );
	
	bufferDesc->release();
	ReleaseSCSITask ( request );
	
	result = isInactive() ? false : true;
	return result;
	
}


//--------------------------------------------------------------------------------------------------
//	EnablePolling - Schedules the polling thread to run									 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::EnablePolling( void )
{		
    AbsoluteTime	time;
	
    if ( ( fPollingMode != kPollingMode_Suspended ) &&
			fPollingThread &&
			( isInactive() == false ) )
    {
        // Retain ourselves so that this object doesn't go away
        // while we are polling
        retain();
        
        clock_interval_to_deadline( 1000, kMillisecondScale, &time );
        
		// Let's enqueue the polling.
		if (thread_call_enter_delayed( fPollingThread, time ))
		{
			// The call was already enqueued therefore there already is
			// a pending retain and we must release the last retain to
			// maintain the proper balance: if we would not release it,
			// the retain count would keep growing.
			release();
		}
		
	}
}


//--------------------------------------------------------------------------------------------------
//	DisablePolling - Unschedules the polling thread if it hasn't run yet				 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::DisablePolling( void )
{		
	fPollingMode = kPollingMode_Suspended;

	// Cancel the thread if it is scheduled
	if( thread_call_cancel( fPollingThread ) )
	{
		// It was scheduled, so we balance out the retain()
		// with a release()
		release();
		
	}
}


//--------------------------------------------------------------------------------------------------
//	DetermineDeviceCharacteristics - Determines device characteristics					 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageUFIDevice::DetermineDeviceCharacteristics( void )
{
	SCSIServiceResponse 			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier				request = NULL;
	IOMemoryDescriptor *			bufferDesc = NULL;
    SCSICmd_INQUIRY_StandardData * 	inquiryBuffer = NULL;
    UInt8							inquiryBufferCount = sizeof ( SCSICmd_INQUIRY_StandardData );
	bool							succeeded = false;
	int								loopCount;
	char							tempString [ kINQUIRY_PRODUCT_IDENTIFICATION_Length + 1 ]; // Maximum + 1 for null char
	OSString *						string;


	STATUS_LOG ( ( 6,  "%s[%p]::DetermineDeviceCharacteristics called", getName(), this ) );
	
	inquiryBuffer = ( SCSICmd_INQUIRY_StandardData * ) IOMalloc ( inquiryBufferCount );
	if( inquiryBuffer == NULL )
	{
	
		STATUS_LOG ( ( 1, "%s[%p]: Couldn't allocate Inquiry buffer.", getName(), this ) );
		goto ErrorExit;
		
	}

	bufferDesc = IOMemoryDescriptor::withAddress ( inquiryBuffer, inquiryBufferCount, kIODirectionIn );
	if ( bufferDesc == NULL )
	{
	
		STATUS_LOG ( ( 1, "%s[%p]: Couldn't alloc Inquiry buffer: ", getName(), this ) );
		goto ErrorExit;
		
	}

	request = GetSCSITask();
	if ( request == NULL )
	{
		goto ErrorExit;
	}

	if ( INQUIRY ( 	request,
					bufferDesc,
					0,
					inquiryBufferCount ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
		
	}
	else
	{
	
		PANIC_NOW ( ( "IOUSBMassStorageUFIDevice::DetermineDeviceCharacteristics malformed command" ) );
		goto ErrorExit;
		
	}
	
	if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
		( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
	{
	
		STATUS_LOG ( ( 2, "%s[%p]: Inquiry completed with an error: ", getName(), this ) );
		goto ErrorExit;
		
	}
	
   	// Set the Vendor Identification property for the device.
   	for ( loopCount = 0; loopCount < kINQUIRY_VENDOR_IDENTIFICATION_Length; loopCount++ )
   	{
   		tempString[loopCount] = inquiryBuffer->VENDOR_IDENTIFICATION[loopCount];
   	}
	
	tempString[loopCount] = 0;
	
   	for ( loopCount = kINQUIRY_VENDOR_IDENTIFICATION_Length - 1; loopCount >= 0; loopCount-- )
   	{
	
   		if ( tempString[loopCount] != ' ' )
   		{
   			// Found a real character
   			tempString[loopCount+1] = '\0';
   			break;
			
   		}
		
   	}
   	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
	
		fDeviceCharacteristicsDictionary->setObject ( kIOPropertyVendorNameKey, string );
		string->release();
		
	}
	
   	// Set the Product Indentification property for the device.
   	for ( loopCount = 0; loopCount < kINQUIRY_PRODUCT_IDENTIFICATION_Length; loopCount++ )
   	{
   		tempString[loopCount] = inquiryBuffer->PRODUCT_IDENTIFICATION[loopCount];
   	}
   	tempString[loopCount] = 0;
	
   	for ( loopCount = kINQUIRY_PRODUCT_IDENTIFICATION_Length - 1; loopCount >= 0; loopCount-- )
   	{
   		if ( tempString[loopCount] != ' ' )
   		{
   			// Found a real character
   			tempString[loopCount+1] = '\0';
   			break;
			
   		}
   	}
	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
	
		fDeviceCharacteristicsDictionary->setObject ( kIOPropertyProductNameKey, string );
		string->release();
		
	}

   	// Set the Product Revision Level property for the device.
   	for ( loopCount = 0; loopCount < kINQUIRY_PRODUCT_REVISION_LEVEL_Length; loopCount++ )
   	{
   		tempString[loopCount] = inquiryBuffer->PRODUCT_REVISION_LEVEL[loopCount];
   	}
   	
   	tempString[loopCount] = 0;
	
   	for ( loopCount = kINQUIRY_PRODUCT_REVISION_LEVEL_Length - 1; loopCount >= 0; loopCount-- )
   	{
		if ( tempString[loopCount] != ' ' )
		{
			// Found a real character
			tempString[loopCount+1] = '\0';
			break;
			
		}
	}
	
	string = OSString::withCString ( tempString );
	if ( string != NULL )
	{
	
		fDeviceCharacteristicsDictionary->setObject ( kIOPropertyProductRevisionLevelKey, string );
		string->release();
		
	}

	succeeded = true;


ErrorExit:


	STATUS_LOG ( ( 6, "%s[%p]::DetermineDeviceCharacteristics exiting", getName(), this ) );

	if ( request )
	{
		ReleaseSCSITask ( request );
		request = NULL;
	}

	if ( bufferDesc )
	{
		bufferDesc->release();
		bufferDesc = NULL;
	}
	
	if ( inquiryBuffer )	
	{
		IOFree ( ( void * ) inquiryBuffer, inquiryBufferCount );
		inquiryBuffer = NULL;
	}
	
	return succeeded;
	 
}


//--------------------------------------------------------------------------------------------------
//	SetMediumCharacteristics - Sets medium characteristics								 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::SetMediumCharacteristics( UInt32 blockSize, UInt32 blockCount )
{

    STATUS_LOG ( ( 6, "%s[%p]::SetMediumCharacteristics called", getName(), this ) );
	STATUS_LOG ( ( 5, "%s[%p]::mediumBlockSize = %ld, blockCount = %ld", getName(), this, blockSize, blockCount ) );
	
	fMediumBlockSize	= blockSize;
	fMediumBlockCount	= blockCount;
	
	STATUS_LOG ( ( 6, "%s[%p]::SetMediumCharacteristics exiting", getName(), this ) );
	
}


//--------------------------------------------------------------------------------------------------
//	ResetMediumCharacteristics -	Resets medium characteristics to known values.		 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::ResetMediumCharacteristics ( void )
{

    STATUS_LOG ( ( 6, "%s[%p]::ResetMediumCharacteristics called", getName(), this ) );
	
	fMediumBlockSize		= 0;
	fMediumBlockCount		= 0;
	fMediumPresent			= false;
	fMediumIsWriteProtected = true;
	
	STATUS_LOG ( ( 6, "%s[%p]::ResetMediumCharacteristics exiting", getName(), this ) );
	
}


//--------------------------------------------------------------------------------------------------
//	CreateStorageServiceNub -	Creates the linkage object for IOStorageFamily to use.	 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::CreateStorageServiceNub ( void )
{
    STATUS_LOG ( ( 6, "%s[%p]::CreateStorageServiceNub entering.", getName(), this ) );

	IOService * 	nub = OSTypeAlloc ( IOUFIStorageServices );
	if ( nub == NULL )
	{
	
		STATUS_LOG ( ( 1, "%s[%p]::CreateStorageServiceNub failed", getName(), this ) );
		PANIC_NOW ( ( "IOUSBMassStorageUFIDevice::CreateStorageServiceNub failed" ) );
		return;
		
	}
	
	nub->init();
	
	if ( !nub->attach( this ) )
	{
		// panic since the nub can't attach
		PANIC_NOW ( ( "IOUSBMassStorageUFIDevice::CreateStorageServiceNub unable to attach nub" ) );
		return;
		
	}
	
	nub->registerService(kIOServiceAsynchronous);
	STATUS_LOG ( ( 6, "%s[%p]::CreateStorageServiceNub exiting.", getName(), this ) );

	nub->release();
}


//--------------------------------------------------------------------------------------------------
//	ProcessPoll - Processes a poll for media or media removal.							  PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::ProcessPoll ( void )
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
			STATUS_LOG ( ( 1, "%s[%p]:ProcessPoll Unknown polling mode.", getName(), this ) );
		}
		break;
		
	}
}


//--------------------------------------------------------------------------------------------------
//	PollForNewMedia - Polls for new media insertion.									 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageUFIDevice::PollForNewMedia( void )
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
	
	if ( DetermineMediumCapacity ( &blockSize, &blockCount ) == false )
	{
		// Capacity could not be determined, treat it like no media inserted
		// and try again.
		return;
	}
	
	SetMediumCharacteristics ( blockSize, blockCount );
	
	fMediumIsWriteProtected = DetermineMediumWriteProtectState();
	
	fMediumPresent	= true;
	
	// Message up the chain that we have media
	messageClients ( kIOMessageMediaStateHasChanged,
					 ( void * ) kIOMediaStateOnline,
					 sizeof( IOMediaState ) );

	// Media is not locked into the drive, so this is most likely
	// a manually ejectable device, start polling for media removal.
	fPollingMode = kPollingMode_MediaRemoval;
}


//--------------------------------------------------------------------------------------------------
//	DetermineMediaPresence -	Checks if media has been inserted into the
//								device. If medium is detected, this method
//								will return true, else it will return false				 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::DetermineMediaPresence ( void )
{
	SCSIServiceResponse			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier			request = NULL;
	bool						mediaFound = false;
	OSBoolean *					keySwitchLocked = NULL;


	STATUS_LOG ( ( 6, "%s[%p]::DetermineMediaPresence called", getName(), this ) );

	keySwitchLocked = OSDynamicCast ( OSBoolean, getProperty ( kAppleKeySwitchProperty ) );

	if ( keySwitchLocked != NULL )
	{
		// See if we should poll for media.
		if ( keySwitchLocked->isTrue ( ) )
		{
			return false;
		}
	}

	request = GetSCSITask();
	if ( request == NULL )
	{
		return false;
	}
	
	// Do a TEST_UNIT_READY to generate sense data
	if ( TEST_UNIT_READY ( request ) == true )
    {
    	// The command was successfully built, now send it, set timeout to 10 seconds.
    	serviceResponse = SendCommand( request, 10 * 1000 );
		
	}
	else
	{
	
		STATUS_LOG ( ( 1, "%s[%p]::DetermineMediaPresence malformed command", getName(), this ) );
		goto CheckDone;
		
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
	
		bool					validSense;
		SCSI_Sense_Data			senseBuffer;
		
		// Check for valid Autosense data.  If it was not retrieved from the 
		// device, explicitly ask for it by sending a REQUEST SENSE command.		
		validSense = GetAutoSenseData ( request, &senseBuffer );
		if ( validSense == false )
		{
			IOMemoryDescriptor *	bufferDesc;
			
			bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
															kSenseDefaultSize,
															kIODirectionIn );
			if( bufferDesc == NULL )
			{
			
				STATUS_LOG ( ( 1, "%s[%p]: could not allocate sense buffer descriptor.", getName(), this ) );
				goto CheckDone;
				
			}
			
			// Get the sense data to determine if media is present.
			if ( REQUEST_SENSE( request, bufferDesc, kSenseDefaultSize ) == true )
		    {
		    	// The command was successfully built, now send it
		    	serviceResponse = SendCommand( request, 0 );
				
			}
			else
			{
			
				STATUS_LOG ( ( 1, "%s[%p]::PollForMedia malformed command", getName(), this ) );
				bufferDesc->release();
				goto CheckDone;
				
			}
			
			bufferDesc->release();
			
			if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
	 			( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
	 		{
			
				STATUS_LOG ( ( 2, "%s[%p]: REQUEST_SENSE failed", getName(), this ) );
				goto CheckDone;
				
	 		}

		}
		
		if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) && 
			( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) )
		{
			// Since the device has indicated it needs a start, send it one
			// and then reset the polling.
			if ( START_STOP_UNIT( request, 0x00,0x00, 1 ) == true )
		    {
		    	// The command was successfully built, now send it, set timeout to 10 seconds.
		    	serviceResponse = SendCommand( request, 0 );
			}
			
			goto CheckDone;
			
		}
		else if ( ( senseBuffer.ADDITIONAL_SENSE_CODE != 0x00 ) || 
					( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER != 0x00 ) )
		{
		
			STATUS_LOG ( ( 2, "%s[%p]:: ASC = 0x%02x, ASCQ = 0x%02x", 
                            getName(), 
                            this,
							senseBuffer.ADDITIONAL_SENSE_CODE,
							senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
							
			goto CheckDone;
			
		}
		
	}
	else
	{
	
		STATUS_LOG ( ( 2, "%s[%p]:: serviceResponse = %d", getName(), this, serviceResponse ) );
		goto CheckDone;
		
	}


	UInt8					formatBuffer[12];
	IOMemoryDescriptor *	formatDesc;
	
	formatDesc = IOMemoryDescriptor::withAddress ( ( void * ) &formatBuffer[0],
													12,
													kIODirectionIn );
	if ( formatDesc == NULL )
	{
	
		STATUS_LOG ( ( 1, "%s[%p]: could not allocate sense buffer descriptor.", getName(), this ) );
		goto CheckDone;
		
	}
	
	// If the check makes to to this point, then the TUR returned no errors,
	// now send the READ_FORMAT_CAPACITIES to determine is media is truly present.	
	if ( READ_FORMAT_CAPACITIES ( request, formatDesc, 12 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand( request, 0 );
		
	}
	else
	{
	 
		STATUS_LOG ( ( 1, "%s[%p]::PollForMedia malformed command", getName(), this ) ) ;
		formatDesc->release();
		goto CheckDone;
		
	}

	formatDesc->release();
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
	
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
		{
		
			bool					validSense;
			SCSI_Sense_Data			senseBuffer;
			
			
			validSense = GetAutoSenseData ( request, &senseBuffer );
			if ( validSense == false )
			{
			
				IOMemoryDescriptor *	bufferDesc;
				
				
				bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																kSenseDefaultSize,
																kIODirectionIn );
				if( bufferDesc == NULL )
				{
				
					STATUS_LOG ( ( 1, "%s[%p]: could not allocate sense buffer descriptor.", getName(), this ) );
					goto CheckDone;
					
				}
				
				// Get the sense data to determine if media is present.
				if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize ) == true )
			    {
			    	// The command was successfully built, now send it
			    	serviceResponse = SendCommand( request, 0 );
					
				}
				else
				{
				
					STATUS_LOG ( ( 1, "%s[%p]::PollForMedia malformed command", getName(), this ) );
					bufferDesc->release();
					goto CheckDone;
					
				}
				
				bufferDesc->release();
				
				// If the REQUEST SENSE comamnd fails to execute, exit and try the
				// poll again.
				if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
		 			( GetTaskStatus( request ) != kSCSITaskStatus_GOOD ) )
		 		{
				
					STATUS_LOG ( ( 2, "%s[%p]: REQUEST_SENSE failed", getName(), this ) );
					goto CheckDone;
					
		 		}

			}
			
			if ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_ILLEGAL_REQUEST ) 
				&& ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x20 ) )
			{
				// The device indicates that the READ_FORMAT_CAPACITIES command
				// is not supported.  Since the device has already returned a good
				// status to the TEST_UNIT_READY, report that media was found.
				mediaFound = true;
				goto CheckDone;
				
			}
			
		}
		else if ( GetTaskStatus( request ) != kSCSITaskStatus_GOOD )
		{
			goto CheckDone;
		}
	}
	else
	{
	
		STATUS_LOG ( ( 2, "%s[%p]:: serviceResponse = %d", getName(), this, serviceResponse ) );
		goto CheckDone;
		
	}

	STATUS_LOG ( ( 4, "%s[%p]:: Formats data: ", getName(), this ) );
	for ( int i = 0; i < 12; i ++ )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: %X : ", getName(), this, formatBuffer[i] ) );
	}

	if ( formatBuffer[8] == 0x01 )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]: unformatted media was found.", getName(), this ) );
		
		// There is unformatted media in the drive, until format support
		// is added, treat like no media is present.
		goto CheckDone;
		
	}
	else if ( formatBuffer[8] != 0x02 )
	{
	
		STATUS_LOG ( ( 5, "%s[%p]: no media was found.", getName(), this ) );
		
		// There is no media in the drive, reset the poll.
		goto CheckDone;
		
	}
	
	STATUS_LOG ( ( 5, "%s[%p]: media was found.", getName(), this ) );
	// At this point, it has been determined that there is usable media
	// in the device.
	mediaFound = true;
	
	
CheckDone:

	if( request != NULL )
	{
		ReleaseSCSITask( request );
		request = NULL;
	}

	return mediaFound;
	
}


//--------------------------------------------------------------------------------------------------
//	DetermineMediumCapacity -	Determines capacity of the medium. Returns true
//								if the capacity could be determined, else it
//								returns false.											 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageUFIDevice::DetermineMediumCapacity (	UInt64 * blockSize, 
														UInt64 * blockCount )
{
	SCSIServiceResponse			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
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
		goto IsDone;
		
	}
	
	bufferDesc = IOMemoryDescriptor::withAddress ( capacityData, 8, kIODirectionIn );
	if ( bufferDesc == NULL )
	{
	
		result = false;
		goto IsDone;
		
	}
		
	// We found media, get its capacity
	if ( READ_CAPACITY ( request, bufferDesc, 0, 0x00, 0 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand( request, 0 );
		
	}
	else
	{
	
		STATUS_LOG ( ( 1, "%s[%p]::PollForMedia malformed command", getName(), this ) );
    	result = false;
    	goto IsDone;
		
	}
		
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus( request ) == kSCSITaskStatus_GOOD ) )
	{
	
		*blockSize 	= ( UInt64 ) OSSwapBigToHostInt32 ( capacityData[1] );
		*blockCount = ( UInt64 ) ( OSSwapBigToHostInt32 ( capacityData[0] ) + 1 );
		STATUS_LOG ( ( 4, "%s[%p]: Media capacity: %lx and block size: %lx",
						getName(), this, ( UInt32 ) *blockCount, ( UInt32 ) *blockSize ) );
		result = true;
		
	}
	else
	{
	
		STATUS_LOG ( ( 2, "%s[%p]: Read Capacity failed", getName(), this ) );
    	result = false;
		
	}


IsDone:

	if ( request != NULL )
	{
		ReleaseSCSITask ( request );
	}
	
	if ( bufferDesc != NULL )
	{
		bufferDesc->release();
	}
	
	return result;
	
}


//--------------------------------------------------------------------------------------------------
//	DetermineMediumWriteProtectState -	Determines medium write protect state.			 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::DetermineMediumWriteProtectState( void )
{
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					modeBuffer[72];
	IOMemoryDescriptor *	bufferDesc = NULL;
	SCSITaskIdentifier		request = NULL;
	bool					mediumIsProtected = true;


	STATUS_LOG ( ( 6, "%s[%p]::checkWriteProtection called", getName(), this ) );
		
	request = GetSCSITask();
	if ( request == NULL )
	{
		// Since a SCSI Task could not be gotten, do the safe thing and report
		// the medium as write protected.
		return true;
	}

	bufferDesc = IOMemoryDescriptor::withAddress ( 	modeBuffer,
													72,
													kIODirectionIn );

	if ( bufferDesc == NULL )
	{
		// Since the Mode Sense data buffer descriptor could not be allocated,
		// the command cannot be sent to the drive, exit and report the medium
		// as write protected.
		goto WriteProtectCheckDone;
	}
	
	if ( MODE_SENSE_10 ( 	request,
							bufferDesc,
							0,
							0,
							0x3F,
							72 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand ( request, 0 );
		
	}
	else
	{
	
		STATUS_LOG ( ( 1, "%s[%p]::CheckWriteProtection malformed command", getName(), this ) );
		goto WriteProtectCheckDone;
		
	}

	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus( request ) == kSCSITaskStatus_GOOD ) )
	{
	
		if ( ( modeBuffer[3] & 0x80 ) != 0 )
		{
		 	mediumIsProtected = true;
		}
		else
		{
			mediumIsProtected = false;
		}
		
	}

	
WriteProtectCheckDone:

	if ( bufferDesc != NULL )
	{
		bufferDesc->release();
		bufferDesc = NULL;
	}
	
	if ( request != NULL )
	{
		ReleaseSCSITask ( request );
		request = NULL;
	}
	
	return mediumIsProtected;
}


//--------------------------------------------------------------------------------------------------
//	PollForMediaRemoval - Polls for media removal.										 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageUFIDevice::PollForMediaRemoval ( void )
{
	SCSIServiceResponse			serviceResponse= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier			request = NULL;
	bool						mediaRemoved = false;
		
		
	if ( isInactive() == true )
	{
		fPollingMode = kPollingMode_Suspended;
	}
		
	request = GetSCSITask();
	if ( request == NULL )
	{
		// A SCSI Task could not be gotten, return immediately.
		goto Exit;
	}
		
	// Do a TEST_UNIT_READY to generate sense data
	if ( TEST_UNIT_READY ( request ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand( request, 0 );
		
	}
	else
	{
	
		PANIC_NOW ( ( "IOUSBMassStorageUFIDevice::PollForMediaRemoval malformed command" ) );
		goto RemoveCheckDone;
		
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
	
		bool						validSense;
		SCSI_Sense_Data				senseBuffer;
		IOMemoryDescriptor *		bufferDesc;
		

		// Check for valid Autosense data.  If it was not retrieved from the 
		// device, explicitly ask for it by sending a REQUEST SENSE command.		
		validSense = GetAutoSenseData ( request, &senseBuffer );
		if( validSense == false )
		{
		
			bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
															kSenseDefaultSize,
															kIODirectionIn );
			if ( bufferDesc == NULL )
			{
			
				STATUS_LOG ( ( 1, "%s[%p]: could not allocate sense buffer descriptor.", getName(), this ) );
				goto RemoveCheckDone;
				
			}
			
			if ( REQUEST_SENSE( request, bufferDesc, kSenseDefaultSize ) == true )
		    {
		    	// The command was successfully built, now send it
		    	serviceResponse = SendCommand( request, 0 );
				
			}
			else
			{
			
				PANIC_NOW ( ( "IOUSBMassStorageUFIDevice::PollForMediaRemoval malformed command" ) );
				bufferDesc->release();
				goto RemoveCheckDone;
				
			}
			
			bufferDesc->release();
			
			if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
	 			( GetTaskStatus( request ) != kSCSITaskStatus_GOOD ) )
	 		{
			
				STATUS_LOG ( ( 2, "%s[%p]: REQUEST_SENSE failed", getName(), this ) );
				goto RemoveCheckDone;
				
	 		}

		}
		
		if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x28 ) ||
			( senseBuffer.ADDITIONAL_SENSE_CODE == 0x3A ) )
		{
			// It has been determined that media has been removed, clean up and 
			// exit.
			mediaRemoved = true;
			
		}
		
	}
	

RemoveCheckDone:

	if ( request != NULL )
	{
	
		ReleaseSCSITask( request );
		request = NULL;
		
	}
	if ( mediaRemoved == true )
	{ 
		// Media was removed, set the polling to determine when new media has been inserted
 		fPollingMode = kPollingMode_NewMedia;
		
		// Message up the chain that we do not have media
		messageClients( kIOMessageMediaStateHasChanged,
						( void * ) kIOMediaStateOffline );
						
	}

Exit:
	
	return;
}


#pragma mark -
#pragma mark *** Power Management ***
#pragma mark -


//--------------------------------------------------------------------------------------------------
//	GetNumberOfPowerStateTransitions -	Asks the driver for the number of state transitions between
//										sleep state and the highest power state.
//																						 [PROTECTED]
//--------------------------------------------------------------------------------------------------

UInt32
IOUSBMassStorageUFIDevice::GetNumberOfPowerStateTransitions ( void )
{
	// The number of transitions is the number of states - 1
	return ( kIOUSBMassStorageUFIDeviceNumPowerStates - 1 );
}


//--------------------------------------------------------------------------------------------------
// InitializePowerManagement - 		Register the driver with our policy-maker (in the same class).
//																						 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageUFIDevice::InitializePowerManagement (
											IOService * provider )
{
	fCurrentPowerState = kIOUSBMassStorageUFIDevicePowerStateActive;
	fProposedPowerState	= kIOUSBMassStorageUFIDevicePowerStateActive;
	
	// Call our super to get us into the power management tree
	super::InitializePowerManagement ( provider );
	
	// Register ourselves as a "policy maker" for this device. We use
	// the number of default power states defined by RBC.
	registerPowerDriver ( this, sPowerStates, kIOUSBMassStorageUFIDeviceNumPowerStates );
	
	// Make sure we clamp the lowest power setting that we voluntarily go
	// We only enter kIOUSBMassStorageUFIDevicePowerStateSleep if told by the
	// power manager during a system sleep.
    changePowerStateTo ( kIOUSBMassStorageUFIDevicePowerStateActive );
	
}


//--------------------------------------------------------------------------------------------------
//	GetInitialPowerState -	Asks the driver which power state the device is in at startup time. This
//							function is only called	once, right after InitializePowerManagement().
//																						 [PROTECTED]
//--------------------------------------------------------------------------------------------------

UInt32
IOUSBMassStorageUFIDevice::GetInitialPowerState ( void )
{
	return kIOUSBMassStorageUFIDevicePowerStateActive;
}


//--------------------------------------------------------------------------------------------------
//	HandleCheckPowerState - Checks to see if the power state is	"ACTIVE"				 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageUFIDevice::HandleCheckPowerState ( void )
{

	if ( IsDeviceAccessEnabled ( ) )
	{
	
		super::HandleCheckPowerState ( kIOUSBMassStorageUFIDevicePowerStateActive );
		
	}
	
}


//--------------------------------------------------------------------------------------------------
//	TicklePowerManager - Calls activityTickle to tell the power manager we need to be in a certain
//						 state to fulfill I/O.											 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageUFIDevice::TicklePowerManager ( void )
{

	// Tell the power manager we must be in active state to handle requests
	// "active" state means the minimal possible state in which the driver can
	// handle I/O. This may be set to standby, but there is no gain to setting
	// the drive to standby and then issuing an I/O, it just requires more time.
	// Also, if the drive was asleep, it might need a reset which could put it
	// in standby mode anyway, so we usually request the max state from the power
	// manager 
	( void ) super::TicklePowerManager ( kIOUSBMassStorageUFIDevicePowerStateActive );
	
}


//--------------------------------------------------------------------------------------------------
//	HandlePowerChange - Checks to see if the power state is	"ACTIVE"					 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void
IOUSBMassStorageUFIDevice::HandlePowerChange ( void )
{

	STATUS_LOG ( ( 5, "IOUSBMassStorageUFIDevice::HandlePowerChange called\n" ) );
	
	// Avoid changing power state to lower state when a restart is in progress.
	if ( gRestartShutdownFlag != 0 )
	{
	
		if ( fProposedPowerState <= kIOUSBMassStorageUFIDevicePowerStateSleep )
		{
		
			fCurrentPowerState = fProposedPowerState;
		
		}
		
	}
	
	if ( ( fProposedPowerState != fCurrentPowerState ) &&
			( isInactive ( ) == false ) )
	{
	
		switch ( fProposedPowerState )
		{
		
			case kIOUSBMassStorageUFIDevicePowerStateSleep:
			{
			
				STATUS_LOG ( ( 5, "case kIOUSBMassStorageUFIDevicePowerStateSleep\n" ) );
				
				DisablePolling();

				fCurrentPowerState = kIOUSBMassStorageUFIDevicePowerStateSleep;
				
			}
			break;
			
			case kIOUSBMassStorageUFIDevicePowerStateActive:
			{
			
				STATUS_LOG ( ( 5, "case kIOUSBMassStorageUFIDevicePowerStateActive\n" ) );
				
				fCurrentPowerState = kIOUSBMassStorageUFIDevicePowerStateActive;

				if (fMediumPresent == true)
				{
				
					// We preserve the state since we already have the media connected
					fPollingMode = kPollingMode_MediaRemoval;
					
				}
				else
				{
				
					fPollingMode = kPollingMode_NewMedia;
					
				}
				
				EnablePolling();	
				
			}
			break;
			
			default:
			{
			
				PANIC_NOW ( ( "Undefined power state issued\n" ) );
			
			}
			break;
				
		}
		
	}
	
	if ( isInactive ( ) )
	{
	
		fCurrentPowerState = fProposedPowerState;
		
	}
	
}


#pragma mark -
#pragma mark *** Client Requests Support ***
#pragma mark -



//--------------------------------------------------------------------------------------------------
//	IssueRead - Performs the Synchronous Read Request									 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageUFIDevice::IssueRead ( 	IOMemoryDescriptor *	buffer,
										UInt64					startBlock,
										UInt64					blockCount )
{

	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request;
	
	
	STATUS_LOG ( ( 6, "%s[%p]: syncRead Attempted", getName(), this ) );

	request = GetSCSITask ( );
	
	if ( READ_10 ( 	request,
					buffer,
  					fMediumBlockSize,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount ) == false )
    {
    
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand( request, 0 );
		
	}
	else
	{
	
		PANIC_NOW(( "IOUSBMassStorageUFIDevice::IssueRead malformed command" ) );
		
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


//--------------------------------------------------------------------------------------------------
//	IssueRead - Performs the Asynchronous Read Request									 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageUFIDevice::IssueRead ( 	IOMemoryDescriptor *	buffer,
										UInt64					startBlock,
										UInt64					blockCount,
										void *					clientData )
{

	IOReturn 				status = kIOReturnSuccess;
	SCSITaskIdentifier		request;
	

	STATUS_LOG ( ( 6, "%s[%p]: asyncRead Attempted", getName(), this ) );
	
	request = GetSCSITask();
	
	if (READ_10(	request,
					buffer,
      				fMediumBlockSize,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount ) == true )
    {
    
    	// The command was successfully built, now send it
    	SetApplicationLayerReference( request, clientData );
		STATUS_LOG ( ( 6, "%s[%p]::IssueRead send command.", getName(), this ) );
    	SendCommand( request, 0, &this->AsyncReadWriteComplete );
		
	}
	else
	{
	
		PANIC_NOW(( "IOUSBMassStorageUFIDevice::IssueWrite malformed command" ) );
		status = kIOReturnError;
		
	}

	return status;
}


//--------------------------------------------------------------------------------------------------
//	IssueWrite - Performs the Synchronous Write Request									 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageUFIDevice::IssueWrite ( IOMemoryDescriptor *	buffer,
										UInt64					startBlock,
										UInt64					blockCount )
{

	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request;
	
	
	STATUS_LOG ( ( 6, "%s[%p]: syncWrite Attempted", getName(), this ) );
	
	request = GetSCSITask();
	if ( WRITE_10 ( request,
					buffer,
					fMediumBlockSize,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand( request, 0 );
		
	}
	else
	{
		PANIC_NOW ( ( "IOUSBMassStorageUFIDevice::IssueWrite malformed command" ) );
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


//--------------------------------------------------------------------------------------------------
//	IssueWrite - Performs the Asynchronous Write Request								 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageUFIDevice::IssueWrite (	IOMemoryDescriptor *	buffer,
										UInt64					startBlock,
										UInt64					blockCount,
										void *					clientData )
{

	IOReturn				status = kIOReturnSuccess;
	SCSITaskIdentifier		request;
	
	
	STATUS_LOG ( ( 6, "%s[%p]:: asyncWrite Attempted", getName(), this ) );

	request = GetSCSITask();
	
	if ( WRITE_10 ( request, 
					buffer,
   					fMediumBlockSize,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount ) == true )
    {
    	// The command was successfully built, now send it
    	SetApplicationLayerReference ( request, clientData );
		STATUS_LOG ( ( 6, "%s[%p]::IssueWrite send command.", getName(), this ) );
    	SendCommand ( request, 0, &this->AsyncReadWriteComplete );
		
	}
	else
	{
		PANIC_NOW ( ( "IOUSBMassStorageUFIDevice::IssueWrite malformed command" ) );
	}

	return status;
	
}


//--------------------------------------------------------------------------------------------------
//	SyncReadWrite - 	Translates a synchronous I/O request into a	read or a write.		[PUBLIC]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageUFIDevice::SyncReadWrite ( 	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount,
                         					UInt64					blockSize )
{

	UNUSED ( blockSize );
	
	IODirection		direction;
	IOReturn		theErr;
	

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
	
		STATUS_LOG ( ( 1, "%s[%p]: doSyncReadWrite bad direction argument", getName(), this ) );
		theErr = kIOReturnBadArgument;
		
	}
	
	return theErr;
	
}


//--------------------------------------------------------------------------------------------------
//	AsyncReadWrite - 	Translates a asynchronous I/O request into a read or a write.		[PUBLIC]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageUFIDevice::AsyncReadWrite (	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount,
                         					UInt64					blockSize,
											void *					clientData )
{

	UNUSED ( blockSize );
	
	
	IODirection		direction;
	IOReturn		theErr;
	
	
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
	
		STATUS_LOG ( ( 1, "%s[%p]: doAsyncReadWrite bad direction argument", getName(), this ) );
		theErr = kIOReturnBadArgument;
		
	}
	
	return theErr;
	
}


//--------------------------------------------------------------------------------------------------
//	EjectTheMedium - Changes the polling mode to poll for medium removal.					[PUBLIC]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageUFIDevice::EjectTheMedium ( void )
{

    STATUS_LOG ( ( 6, "%s[%p]::EjectTheMedium called", getName(), this ) );
	
	ResetMediumCharacteristics();
	
	// Set the polling to determine when media has been removed
	if ( fPollingMode != kPollingMode_MediaRemoval )
	{	
	
		fPollingMode = kPollingMode_MediaRemoval;
			
		EnablePolling();
		
	}
	
	return kIOReturnSuccess;
	
}


//--------------------------------------------------------------------------------------------------
//	FormatMedium -	Currently does nothing.						   							[PUBLIC]
//--------------------------------------------------------------------------------------------------

IOReturn
IOUSBMassStorageUFIDevice::FormatMedium ( UInt64 blockCount, UInt64 blockSize )
{

	UNUSED ( blockCount );
	UNUSED ( blockSize );
	
	IOReturn	theErr = kIOReturnSuccess;

    STATUS_LOG ( ( 6, "%s[%p]::FormatMedium called", getName(), this ) );

	return theErr;	
	
}


//--------------------------------------------------------------------------------------------------
//	GetFormatCapacities - Currently does nothing.					   [					 PUBLIC]
//--------------------------------------------------------------------------------------------------

UInt32
IOUSBMassStorageUFIDevice::GetFormatCapacities(	UInt64 * capacities,
												UInt32   capacitiesMaxCount ) const
{

	UNUSED ( capacities );
	UNUSED ( capacitiesMaxCount );
	
    STATUS_LOG ( ( 6, "%s[%p]::doGetFormatCapacities called", getName(), this ) );

	return 0;
	
}


#pragma mark -
#pragma mark *** Device Information Retrieval Methods ***
#pragma mark -


//--------------------------------------------------------------------------------------------------
//	GetVendorString - Returns the vendor string.					   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

char *
IOUSBMassStorageUFIDevice::GetVendorString ( void )
{

	OSString *		vendorString;
	
	STATUS_LOG ( ( 6, "%s[%p]::%s", getName(), this, __FUNCTION__ ) );
	
	vendorString = ( OSString * ) fDeviceCharacteristicsDictionary->getObject( kIOPropertyVendorNameKey );
	if ( vendorString != NULL )
	{
		return ( ( char * ) vendorString->getCStringNoCopy ( ) );
	}
	else
	{
		return ( char * ) "NULL STRING";
	}
	
}


//--------------------------------------------------------------------------------------------------
//	GetProductString - Returns the Product String.											[PUBLIC]
//--------------------------------------------------------------------------------------------------

char *
IOUSBMassStorageUFIDevice::GetProductString ( void )
{

	OSString *		productString;
	
	
	STATUS_LOG ( ( 6, "%s[%p]::%s", getName(), this, __FUNCTION__ ) );
	
	productString = ( OSString * ) fDeviceCharacteristicsDictionary->getObject ( kIOPropertyProductNameKey );
	if ( productString != NULL )
	{
		return ( ( char * ) productString->getCStringNoCopy ( ) );
	}
	else
	{
		return ( char * ) "NULL STRING";
	}
	
}


//--------------------------------------------------------------------------------------------------
//	GetRevisionString - Returns the Revision String.				   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

char *
IOUSBMassStorageUFIDevice::GetRevisionString ( void )
{

	OSString *		revisionString;
	
	
	STATUS_LOG ( ( 6, "%s[%p]::%s", getName(), this, __FUNCTION__ ) );
	
	revisionString = ( OSString * ) fDeviceCharacteristicsDictionary->getObject ( kIOPropertyProductRevisionLevelKey );
	if ( revisionString )
	{
		return ( ( char * ) revisionString->getCStringNoCopy ( ) );
	}
	else
	{
		return ( char * ) "NULL STRING";
	}
	
}


//--------------------------------------------------------------------------------------------------
//	GetProtocolCharacteristicsDictionary - Returns the Protocol Characteristics Dictionary.	[PUBLIC]
//--------------------------------------------------------------------------------------------------

OSDictionary *
IOUSBMassStorageUFIDevice::GetProtocolCharacteristicsDictionary ( void )
{

	STATUS_LOG ( ( 6, "%s[%p]::%s", getName(), this, __FUNCTION__ ) );
	return ( OSDictionary * ) getProperty( kIOPropertyProtocolCharacteristicsKey );
	
}


//--------------------------------------------------------------------------------------------------
//	GetDeviceCharacteristicsDictionary - Returns the Device Characteristics Dictionary.		[PUBLIC]
//--------------------------------------------------------------------------------------------------

OSDictionary *
IOUSBMassStorageUFIDevice::GetDeviceCharacteristicsDictionary ( void )
{

	STATUS_LOG ( ( 6, "%s[%p]::%s", getName(), this, __FUNCTION__ ) );
	return fDeviceCharacteristicsDictionary;
	
}


#pragma mark -
#pragma mark *** Query methods to report device characteristics ***
#pragma mark -


//--------------------------------------------------------------------------------------------------
//	ReportDeviceMaxBlocksReadTransfer -	Reports max number of blocks a device can handle per read.
//																	   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

UInt64
IOUSBMassStorageUFIDevice::ReportDeviceMaxBlocksReadTransfer ( void )
{

	UInt64	maxBlockCount;


    STATUS_LOG ( ( 6, "%s[%p]::%s", getName(), this, __FUNCTION__ ) );

	maxBlockCount = 256;

	return maxBlockCount;
	
}


//--------------------------------------------------------------------------------------------------
//	ReportDeviceMaxBlocksWriteTransfer - Reports max number of blocks a device can handle per write.
//																	   						[PUBLIC]
//--------------------------------------------------------------------------------------------------

UInt64
IOUSBMassStorageUFIDevice::ReportDeviceMaxBlocksWriteTransfer ( void )
{

	UInt64	maxBlockCount;
	
	
    STATUS_LOG ( ( 6, "%s[%p]::%s", getName(), this, __FUNCTION__ ) );

	maxBlockCount = 256;

	return maxBlockCount;
	
}


#pragma mark -
#pragma mark *** Query methods to report installed medium characteristics ***
#pragma mark -

//--------------------------------------------------------------------------------------------------
//	ReportMediumBlockSize -	Reports the medium block size.		   							[PUBLIC]
//--------------------------------------------------------------------------------------------------

UInt64 
IOUSBMassStorageUFIDevice::ReportMediumBlockSize ( void )
{

    STATUS_LOG ( ( 5, "%s[%p]::ReportMediumBlockSize blockSize = %ld", getName(), this, ( UInt32 ) fMediumBlockSize ) );
	return fMediumBlockSize;
	
}


//--------------------------------------------------------------------------------------------------
//	ReportMediumTotalBlockCount -	Reports the number of blocks on the medium				[PUBLIC]
//--------------------------------------------------------------------------------------------------

UInt64
IOUSBMassStorageUFIDevice::ReportMediumTotalBlockCount ( void )
{

    STATUS_LOG ( ( 5, "%s[%p]::ReportMediumTotalBlockCount maxBlock = %ld", getName(), this, fMediumBlockCount ) );
	return fMediumBlockCount;
	
}


//--------------------------------------------------------------------------------------------------
//	ReportMediumWriteProtection -	Reports whether the medium is write protected			 PUBLIC]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::ReportMediumWriteProtection ( void )
{

    STATUS_LOG ( ( 5, "%s[%p]::ReportMediumWriteProtection isWriteProtected = %d.", getName(), this, fMediumIsWriteProtected ) );
	return fMediumIsWriteProtected;
	
}


#pragma mark -
#pragma mark *** Command Builders Utility Methods ***
#pragma mark -


// Utility routines used by all SCSI Command Set objects

//--------------------------------------------------------------------------------------------------
//	IsParameterValid -	Validate Parameter used for 1 bit to 1 byte paramaters			 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::IsParameterValid (	SCSICmdField1Byte param,
												SCSICmdField1Byte mask )
{

	STATUS_LOG ( ( 6, "%s[%p]::IsParameterValid called", getName(), this ) );
	
	if ( ( param | mask ) != mask )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]:: param = %x not valid, mask = %x", getName(), this, param, mask ) );
		return false;
		
	}
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	IsParameterValid - Validate Parameter used for 9 bit to 2 byte paramaters			 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::IsParameterValid (	SCSICmdField2Byte param,
												SCSICmdField2Byte mask )
{

	STATUS_LOG ( ( 6, "%s[%p]::IsParameterValid called", getName(), this ) );
	
	if ( ( param | mask ) != mask )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]:: param = %x not valid, mask = %x", getName(), this, param, mask ) );
		return false;
		
	}
	
	return true;
	
}


////--------------------------------------------------------------------------------------------------
//	IsParameterValid - Validate Parameter used for 17 bit to 4 byte paramaters			 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::IsParameterValid (	SCSICmdField4Byte param,
												SCSICmdField4Byte mask )
{

	STATUS_LOG ( ( 6, "%s[%p]::IsParameterValid called", getName(), this ) );
	
	if ( ( param | mask ) != mask )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]:: param = %x not valid, mask = %x", getName(), this, 
						(unsigned int) param, (unsigned int) mask ) );
		return false;
		
	}
	
	return true;
	
}


#pragma mark -
#pragma mark *** Command Builder Methods ***
#pragma mark -


//--------------------------------------------------------------------------------------------------
//	FORMAT_UNIT - Command Builder														 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::FORMAT_UNIT(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer,
			    			IOByteCount					defectListSize,
			    			SCSICmdField1Byte 			TRACK_NUMBER, 
			    			SCSICmdField2Byte 			INTERLEAVE )
{

	UNUSED ( TRACK_NUMBER );
	
	STATUS_LOG ( ( 6, "%s[%p]::FORMAT_UNIT called", getName(), this ) );
	
	if ( ResetForNewTask ( request ) == false )
	{
	
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
		return false;
		
	}
	
	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid ( INTERLEAVE, kSCSICmdFieldMask2Byte ) == false )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]:: INTERLEAVE = %x not valid", getName(), this, INTERLEAVE ) );
		return false;
		
	}

	if ( defectListSize > 0 )
	{
		// We have data to send to the device, 
		// make sure that we were given a valid buffer
		if ( IsMemoryDescriptorValid( dataBuffer, defectListSize  )
				== false )
		{
		
			STATUS_LOG ( ( 4, "%s[%p]:: dataBuffer = %x not valid, defectListSize = %x",
							getName(), this, dataBuffer, defectListSize ) );
			return false;
			
		}
		
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_FORMAT_UNIT,
								0x00,
								0x00,
								( INTERLEAVE >> 8 ) & 0xFF,
								  INTERLEAVE		& 0xFF,
								0x00 );
 	
	// The client has requested a DEFECT LIST be sent to the device
	// to be used with the format command
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_FromInitiatorToTarget );
	SetDataBuffer ( 			request,
								dataBuffer );
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  INQUIRY - Command Builder															 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::INQUIRY (
							SCSITaskIdentifier			request,
    						IOMemoryDescriptor 			*dataBuffer,
    						SCSICmdField1Byte 			PAGE_OR_OPERATION_CODE,
    						SCSICmdField1Byte 			ALLOCATION_LENGTH )
{

	STATUS_LOG ( ( 6, "%s[%p]::INQUIRY called", getName(), this ) );

	if ( ResetForNewTask( request ) == false )
	{
	
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
		return false;
		
	}
	
	if ( IsParameterValid ( PAGE_OR_OPERATION_CODE, kSCSICmdFieldMask1Byte ) == false )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]:: PAGE_OR_OPERATION_CODE = %x not valid",
						getName(), this, PAGE_OR_OPERATION_CODE ) );
		return false;
		
	}
	
	if ( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask1Byte ) == false )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]:: ALLOCATION_LENGTH = %x not valid", getName(), this, ALLOCATION_LENGTH ) );
		return false;
		
	}
	
	if ( IsMemoryDescriptorValid( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: dataBuffer = %x not valid, ALLOCATION_LENGTH = %x",
						getName(), this, dataBuffer, ALLOCATION_LENGTH ) );
		return false;
	}
		
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_INQUIRY,
								0x00,
								PAGE_OR_OPERATION_CODE,
								0x00,
								ALLOCATION_LENGTH,
								0x00 );
	
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );
	SetDataBuffer ( 			request,
								dataBuffer );
	SetRequestedDataTransferCount ( request,
									ALLOCATION_LENGTH );
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  MODE_SELECT_10 - Command Builder													 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageUFIDevice::MODE_SELECT_10 (
							SCSITaskIdentifier			request,
    						IOMemoryDescriptor 			*dataBuffer,
    						SCSICmdField1Bit 			PF,
    						SCSICmdField1Bit 			SP,
    						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH )
{

	STATUS_LOG ( ( 6, "%s[%p]::MODE_SELECT_10 called", getName(), this ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
		return false;
	}
	
	if( IsParameterValid( PF, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: PF = %x not valid", getName(), this, PF ) );
		return false;
	}

	if( IsParameterValid( SP, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: SP = %x not valid", getName(), this, SP ) );
		return false;
	}

	if( IsParameterValid( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: PARAMETER_LIST_LENGTH = %x not valid",
                        getName(), this, PARAMETER_LIST_LENGTH ) );
		return false;
	}

	if( IsMemoryDescriptorValid( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x",
						getName(), this, dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MODE_SELECT_10,
								( PF << 4 ) | SP,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( PARAMETER_LIST_LENGTH >> 8 ) & 0xFF,
								PARAMETER_LIST_LENGTH & 0xFF,
								0x00 );
	
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_FromInitiatorToTarget );
	SetDataBuffer ( 			request,
								dataBuffer );
	SetRequestedDataTransferCount ( request,
									PARAMETER_LIST_LENGTH );
	
	return true;
	
}
  

//--------------------------------------------------------------------------------------------------
//	  MODE_SENSE_10 - Command Builder													 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageUFIDevice::MODE_SENSE_10 (
							SCSITaskIdentifier			request,
    						IOMemoryDescriptor 			*dataBuffer,
    						SCSICmdField1Bit 			DBD,
	   						SCSICmdField2Bit 			PC,
	   						SCSICmdField6Bit 			PAGE_CODE,
	   						SCSICmdField2Byte 			ALLOCATION_LENGTH )
{

	STATUS_LOG ( ( 6, "%s[%p]::MODE_SENSE_10 called", getName(), this ) );
	
	if ( ResetForNewTask ( request ) == false )
	{
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
		return false;
	}
	
	if( IsParameterValid ( DBD, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: DBD = %x not valid", getName(), this, DBD ) );
		return false;
	}

	if( IsParameterValid ( PC, kSCSICmdFieldMask2Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: PC = %x not valid", getName(), this, PC ) );
		return false;
	}

	if( IsParameterValid ( PAGE_CODE, kSCSICmdFieldMask6Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: PAGE_CODE = %x not valid", getName(), this, PAGE_CODE ) );
		return false;
	}

	if( IsParameterValid ( ALLOCATION_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: ALLOCATION_LENGTH = %x not valid", getName(), this, ALLOCATION_LENGTH ) );
		return false;
	}

	if( IsMemoryDescriptorValid ( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		STATUS_LOG ( (4, "%s[%p]:: dataBuffer = %x not valid, ALLOCATION_LENGTH = %x",
						dataBuffer, ALLOCATION_LENGTH, getName(), this ) );
		return false;
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_MODE_SENSE_10,
								( DBD << 3 ),
								( PC << 6 ) | PAGE_CODE,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8 ) & 0xFF,
								  ALLOCATION_LENGTH		   & 0xFF,
								0x00 );
	
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );
	SetDataBuffer ( 			request,
								dataBuffer );
	SetRequestedDataTransferCount ( request,
									ALLOCATION_LENGTH );
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  PREVENT_ALLOW_MEDIUM_REMOVAL - Command Builder									 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::PREVENT_ALLOW_MEDIUM_REMOVAL( 
							SCSITaskIdentifier			request,
	     					SCSICmdField1Bit 			PREVENT )
{

	STATUS_LOG ( ( 6, "%s[%p]::PREVENT_ALLOW_MEDIUM_REMOVAL called", getName(), this ) );

	if ( ResetForNewTask( request ) == false )
	{
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
		return false;
	}
	
	if( IsParameterValid( PREVENT, kSCSICmdFieldMask2Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: PREVENT = %x not valid", getName(), this, PREVENT ) );
		return false;
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL,
								0x00,
								0x00,
								0x00,
								PREVENT,
								0x00 );
	
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  READ_10 - Command Builder															 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageUFIDevice::READ_10 (
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			UInt32						blockSize,
			    			SCSICmdField1Bit 			DPO, 
			    			SCSICmdField1Bit 			FUA,
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH )
{

	UInt32					requestedByteCount;
	
	
	STATUS_LOG ( ( 6, "%s[%p]::READ_10 called", getName(), this ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
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
	if ( dataBuffer == NULL )
	{
		return false;
	}
	else
	{
		// We have a valid buffer object, check that it has the required
		// capcity for the data to be transfered.
		requestedByteCount = TRANSFER_LENGTH * blockSize;
		
		// We know the number of bytes to transfer, now check that the 
		// buffer is large enough to accomodate this request.
		if ( dataBuffer->getLength() < requestedByteCount )
		{
			return false;
		}
		
	}

	// Do the pre-flight check on the passed in parameters
	if ( IsParameterValid( DPO, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: DPO = %x not valid", getName(), this, DPO ) );
		return false;
	}

	if ( IsParameterValid( FUA, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: FUA = %x not valid", getName(), this, FUA ) );
		return false;
	}

	if ( IsParameterValid( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: RELADR = %x not valid", getName(), this, RELADR ) );
		return false;
	}

	if ( IsParameterValid( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: LOGICAL_BLOCK_ADDRESS = %x not valid",
						getName(), this, LOGICAL_BLOCK_ADDRESS ) );
		return false;
	}

	if ( IsParameterValid( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: TRANSFER_LENGTH = %x not valid",
						getName(), this, TRANSFER_LENGTH ) );
		return false;
	}

	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_10,
								( DPO << 4 ) | ( FUA << 3 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								0x00 );
	
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );	
	SetDataBuffer ( 			request,
								dataBuffer );	
	SetRequestedDataTransferCount ( request,
									requestedByteCount );	
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  READ_12 - Command Builder															 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageUFIDevice::READ_12 (
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			UInt32						blockSize,
			    			SCSICmdField1Bit 			DPO, 
			    			SCSICmdField1Bit 			FUA,
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			TRANSFER_LENGTH )
{

	UInt32					requestedByteCount;


	STATUS_LOG ( ( 6, "%s[%p]::READ_12 called", getName(), this ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
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
		// buffer is large enough to accomodate this request.
		if ( dataBuffer->getLength() < requestedByteCount )
		{
			return false;
		}
		
	}

	// Do the pre-flight check on the passed in parameters
	if( IsParameterValid( DPO, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: DPO = %x not valid", getName(), this, DPO ) );
		return false;
	}

	if( IsParameterValid( FUA, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: FUA = %x not valid", getName(), this, FUA ) );
		return false;
	}

	if( IsParameterValid( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: RELADR = %x not valid", getName(), this, RELADR ) );
		return false;
	}

	if( IsParameterValid( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: LOGICAL_BLOCK_ADDRESS = %x not valid",
						getName(), this, LOGICAL_BLOCK_ADDRESS ) );
		return false;
	}

	if( IsParameterValid( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: TRANSFER_LENGTH = %x not valid",
						getName(), this, TRANSFER_LENGTH ) );
		return false;
	}

	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_12,
								( DPO << 4 ) | ( FUA << 3 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								( TRANSFER_LENGTH >> 24 ) 		& 0xFF,
								( TRANSFER_LENGTH >> 16 ) 		& 0xFF,
								( TRANSFER_LENGTH >> 8  ) 		& 0xFF,
								  TRANSFER_LENGTH				& 0xFF,
								0x00,
								0x00 );
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );	
	SetDataBuffer ( 			request,
								dataBuffer );	
	SetRequestedDataTransferCount ( request,
									requestedByteCount );	

	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  READ_CAPACITY - Command Builder													 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::READ_CAPACITY (
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			SCSICmdField1Bit 			RELADR,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField1Bit 			PMI )
{
	STATUS_LOG ( ( 6, "%s[%p]::READ_CAPACITY called", getName(), this ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
		return false;
	}
	
	// Make sure that we were given a buffer
	if ( dataBuffer == NULL )
	{
		return false;
	}

	// Do the pre-flight check on the passed in parameters
	if( IsParameterValid( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: RELADR = %x not valid", getName(), this, RELADR ) );
		return false;
	}
	
	if( IsParameterValid( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: LOGICAL_BLOCK_ADDRESS = %x not valid",
						getName(), this, LOGICAL_BLOCK_ADDRESS ) );
		return false;
	}

	if( IsParameterValid( PMI, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: PMI = %x not valid", getName(), this, PMI ) );
		return false;
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
								kSCSICmd_READ_CAPACITY,
								RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								0x00,
								PMI,
								0x00 );
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );	
	SetDataBuffer( 				request,
								dataBuffer );	
	SetRequestedDataTransferCount( 	request, 8 );	
	
	return true;
}


//--------------------------------------------------------------------------------------------------
//	  READ_FORMAT_CAPACITIES - Command Builder											 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::READ_FORMAT_CAPACITIES(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			SCSICmdField2Byte 			ALLOCATION_LENGTH )
{

	STATUS_LOG ( ( 6, "%s[%p]::READ_CAPACITY called", getName(), this ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
		return false;
	}
	
	// Make sure that we were given a buffer
	if ( dataBuffer == NULL )
	{
		return false;
	}

	// Do the pre-flight check on the passed in parameters
	if( IsParameterValid( ALLOCATION_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		STATUS_LOG ( (4, "%s[%p]:: ALLOCATION_LENGTH = %x not valid", getName(), this, ALLOCATION_LENGTH ) );
		return false;
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_READ_FORMAT_CAPACITIES,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00,
								( ALLOCATION_LENGTH >> 8  ) & 0xFF,
								  ALLOCATION_LENGTH			& 0xFF,
								0x00 );
	
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );	
	SetDataBuffer ( 			request,
								dataBuffer );	
	SetRequestedDataTransferCount( 	request,
									ALLOCATION_LENGTH );	
									
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  REQUEST_SENSE - Command Builder													 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::REQUEST_SENSE (
							SCSITaskIdentifier			request,
   							IOMemoryDescriptor 			*dataBuffer,
			    			SCSICmdField1Byte 			ALLOCATION_LENGTH )
{

	STATUS_LOG ( ( 6, "%s[%p]::REQUEST_SENSE called", getName(), this ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
		return false;
	}
	
	if( IsParameterValid( ALLOCATION_LENGTH, kSCSICmdFieldMask1Byte ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]ALLOCATION_LENGTH = %x not valid",
						getName(), this, ALLOCATION_LENGTH ) );
		return false;
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_REQUEST_SENSE,
								0x00,
								0x00,
								0x00,
								ALLOCATION_LENGTH,
								0x00 );
	
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );
	SetDataBuffer ( 			request,
								dataBuffer );
	SetRequestedDataTransferCount ( request,
									ALLOCATION_LENGTH );
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  REZERO_UNIT - Command Builder														 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::REZERO_UNIT ( SCSITaskIdentifier	request )
{

	UNUSED ( request );
	
	return false;
	
}


//--------------------------------------------------------------------------------------------------
//	  SEEK - Command Builder															 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageUFIDevice::SEEK ( 
							SCSITaskIdentifier			request,
			    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS )
{

	UNUSED ( request );
	UNUSED ( LOGICAL_BLOCK_ADDRESS );
	
	return false;
	
}


//--------------------------------------------------------------------------------------------------
//	  SEND_DIAGNOSTICS - Command Builder												 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageUFIDevice::SEND_DIAGNOSTICS ( 
							SCSITaskIdentifier			request,
							SCSICmdField1Bit 			PF, 
							SCSICmdField1Bit 			SELF_TEST, 
							SCSICmdField1Bit 			DEF_OFL, 
							SCSICmdField1Bit 			UNIT_OFL )
{

	STATUS_LOG ( ( 6, "%s[%p]::SEND_DIAGNOSTICS called", getName(), this ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
		return false;
	}
	
	if( IsParameterValid( PF, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: PF = %x not valid", getName(), this, PF ) );
		return false;
	}
	
	if( IsParameterValid ( SELF_TEST, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: SELF_TEST = %x not valid", getName(), this, SELF_TEST ) );
		return false;
	}
	
	if( IsParameterValid( DEF_OFL, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: DEF_OFL = %x not valid", getName(), this, DEF_OFL ) );
		return false;
	}
	
	if( IsParameterValid( UNIT_OFL, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: UNIT_OFL = %x not valid", getName(), this, UNIT_OFL ) );
		return false;
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_SEND_DIAGNOSTICS,
								( PF << 4 ) |
									( SELF_TEST << 2 ) | ( DEF_OFL << 1 ) | UNIT_OFL,
								0x00,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_NoDataTransfer);
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  START_STOP_UNIT - Command Builder													 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::START_STOP_UNIT ( 
							SCSITaskIdentifier			request,
							SCSICmdField1Bit 			IMMED, 
							SCSICmdField1Bit 			LOEJ, 
							SCSICmdField1Bit 			START )
{

	STATUS_LOG ( ( 6, "%s[%p]::START_STOP_UNIT called", getName(), this ) );

	if ( ResetForNewTask( request ) == false )
	{
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
		return false;
	}
	
	// Do the pre-flight check on the passed in parameters
	if( IsParameterValid( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: IMMED = %x not valid", getName(), this, IMMED ) );
		return false;
	}

	if( IsParameterValid( LOEJ, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: LOEJ = %x not valid", getName(), this, LOEJ ) );
		return false;
	}

	if( IsParameterValid( START, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( 4, "%s[%p]:: START = %x not valid", getName(), this, START ) );
		return false;
	}

	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_START_STOP_UNIT,
								IMMED,
								0x00,
								0x00,
								( LOEJ << 1 ) | START,
								0x00 );
	
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  TEST_UNIT_READY - Command Builder													 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::TEST_UNIT_READY (  
							SCSITaskIdentifier			request )
{

	STATUS_LOG ( ( 6, "%s[%p]::TEST_UNIT_READY called", getName(), this ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
		return false;
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_TEST_UNIT_READY,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  VERIFY - Command Builder															 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::VERIFY( 
							SCSITaskIdentifier			request,
							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			BYTCHK, 
							SCSICmdField1Bit 			RELADR,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			VERIFICATION_LENGTH )
{
	UNUSED( request );
	UNUSED( DPO );
	UNUSED( BYTCHK );
	UNUSED( RELADR );
	UNUSED( LOGICAL_BLOCK_ADDRESS );
	UNUSED( VERIFICATION_LENGTH );
	
	return false;
}


//--------------------------------------------------------------------------------------------------
//	  WRITE_10 - Command Builder														 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFIDevice::WRITE_10 (
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			UInt32						blockSize,
			    			SCSICmdField1Bit 			DPO, 
			    			SCSICmdField1Bit 			FUA,
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH )
{

	UInt32					requestedByteCount;
	
	
	STATUS_LOG ( ( 6, "%s[%p]::WRITE_10 called", getName(), this ) );

	if ( ResetForNewTask( request ) == false )
	{
	
		STATUS_LOG ( ( 1, "%s[%p]:: ResetForNewTask on the request SCSITask failed.", getName(), this ) );
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
		// buffer is large enough to accomodate this request.
		if ( dataBuffer->getLength() < requestedByteCount )
		{
			return false;
		}
		
	}

	// Do the pre-flight check on the passed in parameters
	if( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ) == false )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]:: DPO = %x not valid", getName(), this, DPO ) );
		return false;
		
	}

	if( IsParameterValid ( FUA, kSCSICmdFieldMask1Bit ) == false )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]:: FUA = %x not valid", getName(), this, FUA ) );
		return false;
		
	}

	if( IsParameterValid ( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]:: RELADR = %x not valid", getName(), this, RELADR ) );
		return false;
		
	}

	if( IsParameterValid ( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]:: LOGICAL_BLOCK_ADDRESS = %x not valid",
						getName(), this, LOGICAL_BLOCK_ADDRESS ) );
		return false;
		
	}

	if( IsParameterValid ( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
	
		STATUS_LOG ( ( 4, "%s[%p]:: TRANSFER_LENGTH = %x not valid",
						getName(), this, TRANSFER_LENGTH ) );
		return false;
		
	}

	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock (	request,
								kSCSICmd_WRITE_10,
								( DPO << 4 ) | ( FUA << 3 ) | RELADR,
								( LOGICAL_BLOCK_ADDRESS >> 24 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 16 ) & 0xFF,
								( LOGICAL_BLOCK_ADDRESS >> 8  ) & 0xFF,
								  LOGICAL_BLOCK_ADDRESS			& 0xFF,
								0x00,
								( TRANSFER_LENGTH >> 8 ) & 0xFF,
								  TRANSFER_LENGTH		 & 0xFF,
								0x00 );
	
	SetDataTransferDirection ( 	request,
								kSCSIDataTransfer_FromInitiatorToTarget );
	SetDataBuffer ( 			request,
								dataBuffer );
	SetRequestedDataTransferCount ( request,
									requestedByteCount );
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  WRITE_12 - Command Builder									[PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageUFIDevice::WRITE_12 (
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer, 
			    			UInt32						blockSize,
			    			SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			EBP, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			TRANSFER_LENGTH )
{

	UNUSED ( request );
	UNUSED ( dataBuffer );
	UNUSED ( blockSize );
	UNUSED ( DPO );
	UNUSED ( EBP );
	UNUSED ( RELADR );
	UNUSED ( LOGICAL_BLOCK_ADDRESS );
	UNUSED ( TRANSFER_LENGTH );
	
	return false;
	
}


//--------------------------------------------------------------------------------------------------
//	  WRITE_AND_VERIFY - Command Builder												 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool 
IOUSBMassStorageUFIDevice::WRITE_AND_VERIFY (
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			UInt32						blockSize,
			    			SCSICmdField1Bit 			DPO,
			    			SCSICmdField1Bit 			BYTCHK, 
			    			SCSICmdField1Bit 			RELADR, 
			    			SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS, 
			    			SCSICmdField2Byte 			TRANSFER_LENGTH )
{

	UNUSED ( request );
	UNUSED ( dataBuffer );
	UNUSED ( blockSize );
	UNUSED ( DPO );
	UNUSED ( BYTCHK );
	UNUSED ( RELADR );
	UNUSED ( LOGICAL_BLOCK_ADDRESS );
	UNUSED ( TRANSFER_LENGTH );
	
	return false;
	
}


#pragma mark -
#pragma mark *** IOUSBMassStorageUFISubclass methods ***
#pragma mark -


//--------------------------------------------------------------------------------------------------
//	  BeginProvidedServices																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool
IOUSBMassStorageUFISubclass::BeginProvidedServices ( void )
{
	// Create the IOUSBMassStorageUFIDevice object
	IOUSBMassStorageUFIDevice * 	ufiDevice = OSTypeAlloc ( IOUSBMassStorageUFIDevice );
	if( ufiDevice == NULL )
	{
		STATUS_LOG ( ( 1, "%s[%p]::BeginProvidedServices failed", getName(), this ) );
		PANIC_NOW ( ( "IOUSBMassStorageUFISubclass::BeginProvidedServices failed" ) );
		return false;
	}
	
	ufiDevice->init ( NULL );
	
	if ( !ufiDevice->attach( this ) )
	{
		// panic since the nub can't attach
		PANIC_NOW ( ( "IOUSBMassStorageUFISubclass::BeginProvidedServices unable to attach nub" ) );
		return false;
	}
	
	if ( ufiDevice->start( this ) == false )
	{
		ufiDevice->detach( this );
	}
	
	STATUS_LOG ( ( 4, "%s[%p]::BeginProvidedServices exiting.", getName(), this ) );
	
	ufiDevice->release();
	
	return true;
	
}


//--------------------------------------------------------------------------------------------------
//	  EndProvidedServices																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

bool	
IOUSBMassStorageUFISubclass::EndProvidedServices ( void )
{
	return true;
}
