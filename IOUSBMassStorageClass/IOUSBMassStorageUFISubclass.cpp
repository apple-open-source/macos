/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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

// This class' header file
#include <IOKit/usb/IOUSBMassStorageUFISubclass.h>

#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/usb/IOUFIStorageServices.h>
#include <IOKit/scsi-commands/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi-commands/SCSICommandOperationCodes.h>

#include <IOKit/scsi-commands/SCSITask.h>

#include "Debugging.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#if (USB_MASS_STORAGE_DEBUG == 0)
#define USB_MSC_UFI_DEBUGGING_LEVEL 0
#else
#define USB_MSC_UFI_DEBUGGING_LEVEL 3
#endif

// For debugging, set USB_MSC_UFI_DEBUGGING_LEVEL to one
// of the following values:
//		0	No debugging 	(GM release level)
// 		1 	PANIC_NOW only
//		2	PANIC_NOW and ERROR_LOG
//		3	PANIC_NOW, ERROR_LOG and STATUS_LOG

#if ( USB_MSC_UFI_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( USB_MSC_UFI_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( USB_MSC_UFI_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kKeySwitchProperty			"Keyswitch"
#define kAppleKeySwitchProperty		"AppleKeyswitch"

OSDefineMetaClassAndStructors( IOUSBMassStorageUFISubclass, IOUSBMassStorageClass )


#pragma mark -
#pragma mark *** IOUSBMassStorageUFIDevice declaration ***
#pragma mark -

OSDefineMetaClassAndStructors( IOUSBMassStorageUFIDevice, IOSCSIPrimaryCommandsDevice )


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ free - Called by IOKit to free any resources.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOUSBMassStorageUFIDevice::free ( void )
{
	
	// Check first that fIOUSBMassStorageUFIDeviceReserved is not NULL
	// so that we don't dereference it when we shouldn't when we
	// check if fKeySwitchNotifier is NULL since fKeySwitchNotifier is
	// defined to be fIOUSBMassStorageUFIDeviceReserved->fKeySwitchNotifier
	if ( fIOUSBMassStorageUFIDeviceReserved != NULL )
	{
		
		if ( fKeySwitchNotifier != NULL )
		{
			
			fKeySwitchNotifier->remove ( );
			fKeySwitchNotifier = NULL;
			
		}
		
		IOFree ( fIOUSBMassStorageUFIDeviceReserved,
				 sizeof ( IOUSBMassStorageUFIDeviceExpansionData ) );
		fIOUSBMassStorageUFIDeviceReserved = NULL;
		
	}
	
	// Make sure to call our super
	IOSCSIPrimaryCommandsDevice::free ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ message - Called by IOKit to deliver messages.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUSBMassStorageUFIDevice::message( UInt32 type, IOService * provider, void * argument )
{
	
	IOReturn	result;
	
	switch( type )
	{
		
		case kIOMessageServiceIsRequestingClose:
		{
			// Check first that fIOUSBMassStorageUFIDeviceReserved is not NULL
			// so that we don't dereference it when we shouldn't when we
			// check if fKeySwitchNotifier is NULL since fKeySwitchNotifier is
			// defined to be fIOUSBMassStorageUFIDeviceReserved->fKeySwitchNotifier
			if ( fIOUSBMassStorageUFIDeviceReserved != NULL )
			{
				if ( fKeySwitchNotifier != NULL )
				{
					
					fKeySwitchNotifier->remove ( );
					fKeySwitchNotifier = NULL;
					
				}
			}
			result = IOSCSIPrimaryCommandsDevice::message( type, provider, argument );
		}
		break;
		
		default:
		{
			result = IOSCSIPrimaryCommandsDevice::message( type, provider, argument );
		}
	}
	
	return result;
	
}


#pragma mark -
#pragma mark *** Static Class Methods ***
#pragma mark -

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sProcessPoll - Gets scheduled to execute the polls.	   [STATIC][PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::sProcessPoll( void * theUFIDriver, void * refCon )
{
	IOUSBMassStorageUFIDevice *	driver;
	
	driver = (IOUSBMassStorageUFIDevice *) theUFIDriver;
	driver->ProcessPoll();
	if( driver->fPollingMode != kPollingMode_Suspended )
	{
		// schedule the poller again
		driver->EnablePolling();
	}
	
	// drop the retain associated with this poll
	driver->release();
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWriteComplete - Completion routine for I/O	  [STATIC][PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::AsyncReadWriteComplete( SCSITaskIdentifier request )
{
	void *							clientData;
	IOReturn						status;
	UInt64							actCount = 0;
	IOUSBMassStorageUFIDevice *		taskOwner;
		
	if ( request == NULL )
	{
		PANIC_NOW(( "IOUSBMassStorageUFIDevice::AsyncReadWriteComplete request==NULL." ));
	}

	taskOwner = OSDynamicCast( IOUSBMassStorageUFIDevice, IOSCSIPrimaryCommandsDevice::sGetOwnerForTask( request ));
	if ( taskOwner == NULL )
	{
		PANIC_NOW(( "IOUSBMassStorageUFIDevice::AsyncReadWriteComplete taskOwner==NULL." ));
	}

	// Extract the client data from the SCSITask	
	clientData	= taskOwner->GetApplicationLayerReference( request );
	
	if (( taskOwner->GetServiceResponse( request ) == kSCSIServiceResponse_TASK_COMPLETE ) &&
		( taskOwner->GetTaskStatus( request ) == kSCSITaskStatus_GOOD )) 
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
		actCount = taskOwner->GetDataBuffer( request )->getLength();
	}
	

	taskOwner->ReleaseSCSITask( request );

	IOUFIStorageServices::AsyncReadWriteComplete( clientData, status, actCount );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// ¥ ServerKeyswitchCallback -	Called when notifications about the server
//								keyswitch are broadcast.	  [STATIC][PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::ServerKeyswitchCallback (
									void *			target,
									void * 			refCon,
									IOService * 	newDevice )
{
	
	OSBoolean *						shouldNotPoll	= NULL;
	IOUSBMassStorageUFIDevice *	device			= NULL;
	
	STATUS_LOG( ( "ServerKeyswitchCallback called.\n" ) );
	
	shouldNotPoll = OSDynamicCast (
							OSBoolean,
							newDevice->getProperty ( kKeySwitchProperty ) );
	
	device = OSDynamicCast ( IOUSBMassStorageUFIDevice, ( OSObject * ) target );
	
	if ( ( shouldNotPoll != NULL ) && ( device != NULL ) )
	{
		
		// Is the key unlocked?
		if ( shouldNotPoll->isFalse ( ) )
		{
			
			// Key is unlocked, start resuming device support
			device->ResumeDeviceSupport ( );
			
		}
		
		else if ( shouldNotPoll->isTrue ( ) )
		{
			
			// Key is locked, suspend device support
			device->SuspendDeviceSupport ( );
			
		}
		
	}
	
	return true;
	
}


#pragma mark -
#pragma mark *** Class Methods ***
#pragma mark -


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ InitializeDeviceSupport - Initializes device support			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::InitializeDeviceSupport( void )
{
	bool setupSuccessful 	= false;
	
	// Initialize the medium characteristics
	fMediumPresent			= false;
	fMediumIsWriteProtected	= true;

    STATUS_LOG( ( "IOUSBMassStorageUFIDevice::InitializeDeviceSupport called\n" ) );

	ClearNotReadyStatus();

	require( ( DetermineDeviceCharacteristics( ) == true ), ERROR_EXIT );
	
	fPollingMode = kPollingMode_NewMedia;
	fPollingThread = thread_call_allocate(
					( thread_call_func_t ) IOUSBMassStorageUFIDevice::sProcessPoll,
					( thread_call_param_t ) this );
	
	require_nonzero( fPollingThread, ERROR_EXIT );
	
	fIOUSBMassStorageUFIDeviceReserved = ( IOUSBMassStorageUFIDeviceExpansionData * )
			IOMalloc ( sizeof ( IOUSBMassStorageUFIDeviceExpansionData ) );
	
	require_nonzero ( fIOUSBMassStorageUFIDeviceReserved, ERROR_EXIT );
	
	bzero ( fIOUSBMassStorageUFIDeviceReserved,
			sizeof ( IOUSBMassStorageUFIDeviceExpansionData ) );
	
	// Add a notification for the Apple KeySwitch on the server.
	fKeySwitchNotifier = addNotification (
			gIOMatchedNotification,
			nameMatching ( kAppleKeySwitchProperty ),
			( IOServiceNotificationHandler ) &IOUSBMassStorageUFIDevice::ServerKeyswitchCallback,
			this,
			0 );

	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::InitializeDeviceSupport setupSuccessful = %d\n", setupSuccessful ) );
	
	setupSuccessful = true;
	
ERROR_EXIT:
	return setupSuccessful;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StartDeviceSupport - Starts device support					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOUSBMassStorageUFIDevice::StartDeviceSupport( void )
{
	// This is only here to keep the compiler happy since
	// the method is pure virtual.  We don't need it for UFI.
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SuspendDeviceSupport - Suspends device support				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::SuspendDeviceSupport( void )
{
	if( fPollingMode != kPollingMode_Suspended )
	{
    	DisablePolling();
    }		
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ResumeDeviceSupport - Resumes device support					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::ResumeDeviceSupport( void )
{
	// The driver has not found media in the device, restart 
	// the polling for new media.
	if( fMediumPresent == false )
	{
		fPollingMode = kPollingMode_NewMedia;
	    EnablePolling();
	}
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StopDeviceSupport - Stops device support						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOUSBMassStorageUFIDevice::StopDeviceSupport( void )
{
	// This is only here to keep the compiler happy since
	// the method is pure virtual.  We don't need it for UFI.
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TerminateDeviceSupport - Terminates device support			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::TerminateDeviceSupport( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::cleanUp called.\n" ) );

    if ( fPollingThread != NULL )
    {
        thread_call_free ( fPollingThread );
        fPollingThread = NULL;
    }
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ClearNotReadyStatus - Clears any NOT_READY status on device	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::ClearNotReadyStatus( void )
{
	SCSI_Sense_Data				senseBuffer;
	IOMemoryDescriptor *		bufferDesc;
	SCSITaskIdentifier			request;
	bool						driveReady = false;
	bool						result = true;
	SCSIServiceResponse 		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	STATUS_LOG( ( "%s::%s called\n", getName(), __FUNCTION__ ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress( ( void * ) &senseBuffer,
													kSenseDefaultSize,
													kIODirectionIn );
	
	request = GetSCSITask();
	do
	{
		if( TEST_UNIT_READY( request ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand( request, 0 );
		}
		else
		{
			PANIC_NOW( ( "IOUSBMassStorageUFIDevice::ClearNotReadyStatus malformed command" ) );
		}
		
		if( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
			bool validSense = false;
			
			if ( GetTaskStatus( request ) == kSCSITaskStatus_CHECK_CONDITION )
			{
				validSense = GetAutoSenseData( request, &senseBuffer );
				if( validSense == false )
				{
					if( REQUEST_SENSE( request, bufferDesc, kSenseDefaultSize ) == true )
					{
						// The command was successfully built, now send it
						serviceResponse = SendCommand( request, 0 );
					}
					else
					{
						PANIC_NOW( ( "IOUSBMassStorageUFIDevice::ClearNotReadyStatus malformed command" ) );
					}
					
					if( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
					{
						validSense = true;
					}
				}
				
				if( validSense == true )
				{
					if( ( ( senseBuffer.SENSE_KEY  & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY  ) && 
							( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x01 ) )
					{
						
						STATUS_LOG( ( "%s::drive not ready\n", getName ( ) ) );
						driveReady = false;
						IOSleep( 200 );
						
					}
					else if( ( ( senseBuffer.SENSE_KEY  & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY  ) && 
							( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) )
					{
						// The drive needs to be spun up. Issue a START_STOP_UNIT to it.
						if( START_STOP_UNIT( request, 0x00, 0x00, 0x01 ) == true )
						{
							serviceResponse = SendCommand( request, 0 );
						}
					}
					else
					{
						driveReady = true;
						STATUS_LOG( ( "%s::drive READY\n", getName ( ) ) );
					}
					
					STATUS_LOG( ( "sense data: %01x, %02x, %02x\n",
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
	ReleaseSCSITask( request );
	
	result = isInactive() ? false : true;
	return result;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EnablePolling - Schedules the polling thread to run			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::EnablePolling( void )
{		
    AbsoluteTime	time;
	
    if( ( fPollingMode != kPollingMode_Suspended ) &&
		fPollingThread &&
		( isInactive() == false ) )
    {
        // Retain ourselves so that this object doesn't go away
        // while we are polling
        retain();
        
        clock_interval_to_deadline( 1000, kMillisecondScale, &time );
        thread_call_enter_delayed( fPollingThread, time );
	}
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DisablePolling - Unschedules the polling thread if it hasn't run yet
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::DisablePolling( void )
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineDeviceCharacteristics - Determines device characteristics
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOUSBMassStorageUFIDevice::DetermineDeviceCharacteristics( void )
{
	SCSIServiceResponse 			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier				request = NULL;
	IOMemoryDescriptor *			bufferDesc = NULL;
    SCSICmd_INQUIRY_StandardData * 	inquiryBuffer = NULL;
    UInt8							inquiryBufferCount = sizeof( SCSICmd_INQUIRY_StandardData );
	bool							succeeded = false;
	int								loopCount;
	char							tempString[17]; // Maximum + 1 for null char
	OSString *						string;

	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::DetermineDeviceCharacteristics called\n" ) );

	inquiryBuffer = ( SCSICmd_INQUIRY_StandardData * ) IOMalloc( inquiryBufferCount );
	if( inquiryBuffer == NULL )
	{
		STATUS_LOG( ( "%s: Couldn't allocate Inquiry buffer.\n", getName ( ) ) );
		goto ErrorExit;
	}

	bufferDesc = IOMemoryDescriptor::withAddress( inquiryBuffer, inquiryBufferCount, kIODirectionIn );
	if( bufferDesc == NULL )
	{
		ERROR_LOG ( ( "%s: Couldn't alloc Inquiry buffer: ", getName ( ) ) );
		goto ErrorExit;
	}

	request = GetSCSITask();
	if( request == NULL )
	{
		goto ErrorExit;
	}

	if( INQUIRY( 	request,
					bufferDesc,
					0,
					inquiryBufferCount ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand( request, 0 );
	}
	else
	{
		PANIC_NOW( ( "IOUSBMassStorageUFIDevice::DetermineDeviceCharacteristics malformed command" ) );
		goto ErrorExit;
	}
	
	if( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
		( GetTaskStatus( request ) != kSCSITaskStatus_GOOD ) )
	{
		ERROR_LOG ( ( "%s: Inquiry completed with an error: ", getName ( ) ) );
		goto ErrorExit;
	}

   	// Set the Vendor Identification property for the device.
   	for( loopCount = 0; loopCount < kINQUIRY_VENDOR_IDENTIFICATION_Length; loopCount++ )
   	{
   		tempString[loopCount] = inquiryBuffer->VENDOR_IDENTIFICATION[loopCount];
   	}
	
	tempString[loopCount] = 0;
	
   	for( loopCount = kINQUIRY_VENDOR_IDENTIFICATION_Length - 1; loopCount >= 0; loopCount-- )
   	{
   		if( tempString[loopCount] != ' ' )
   		{
   			// Found a real character
   			tempString[loopCount+1] = '\0';
   			break;
   		}
   	}
   	
	string = OSString::withCString( tempString );
	if( string != NULL )
	{
		fDeviceCharacteristicsDictionary->setObject( kIOPropertyVendorNameKey, string );
		string->release();
	}
	
   	// Set the Product Indentification property for the device.
   	for( loopCount = 0; loopCount < kINQUIRY_PRODUCT_IDENTIFICATION_Length; loopCount++ )
   	{
   		tempString[loopCount] = inquiryBuffer->PRODUCT_INDENTIFICATION[loopCount];
   	}
   	tempString[loopCount] = 0;
	
   	for( loopCount = kINQUIRY_PRODUCT_IDENTIFICATION_Length - 1; loopCount >= 0; loopCount-- )
   	{
   		if( tempString[loopCount] != ' ' )
   		{
   			// Found a real character
   			tempString[loopCount+1] = '\0';
   			break;
   		}
   	}
	
	string = OSString::withCString( tempString );
	if( string != NULL )
	{
		fDeviceCharacteristicsDictionary->setObject( kIOPropertyProductNameKey, string );
		string->release();
	}

   	// Set the Product Revision Level property for the device.
   	for( loopCount = 0; loopCount < kINQUIRY_PRODUCT_REVISION_LEVEL_Length; loopCount++ )
   	{
   		tempString[loopCount] = inquiryBuffer->PRODUCT_REVISION_LEVEL[loopCount];
   	}
   	
   	tempString[loopCount] = 0;
	
   	for( loopCount = kINQUIRY_PRODUCT_REVISION_LEVEL_Length - 1; loopCount >= 0; loopCount-- )
   	{
		if ( tempString[loopCount] != ' ' )
		{
			// Found a real character
			tempString[loopCount+1] = '\0';
			break;
		}
	}
	
	string = OSString::withCString( tempString );
	if( string != NULL )
	{
		fDeviceCharacteristicsDictionary->setObject( kIOPropertyProductRevisionLevelKey, string );
		string->release();
	}

	succeeded = true;

ErrorExit:

	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::DetermineDeviceCharacteristics exiting\n" ) );

	if( request )
	{
		ReleaseSCSITask( request );
		request = NULL;
	}

	if( bufferDesc )
	{
		bufferDesc->release();
		bufferDesc = NULL;
	}
	
	if( inquiryBuffer )	
	{
		IOFree( ( void * ) inquiryBuffer, inquiryBufferCount );
		inquiryBuffer = NULL;
	}
	
	return succeeded;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetMediumCharacteristics - Sets medium characteristics		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::SetMediumCharacteristics( UInt32 blockSize, UInt32 blockCount )
{
    STATUS_LOG( ( "IOUSBMassStorageUFIDevice::SetMediumCharacteristics called\n" ) );
	STATUS_LOG( ( "mediumBlockSize = %ld, blockCount = %ld\n", blockSize, blockCount ) );
	
	fMediumBlockSize	= blockSize;
	fMediumBlockCount	= blockCount;
	
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::SetMediumCharacteristics exiting\n" ) );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ResetMediumCharacteristics -	Resets medium characteristics to known
//									values							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::ResetMediumCharacteristics( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::ResetMediumCharacteristics called\n" ) );
	fMediumBlockSize		= 0;
	fMediumBlockCount		= 0;
	fMediumPresent			= false;
	fMediumIsWriteProtected = true;
	STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::ResetMediumCharacteristics exiting\n" ) );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CreateStorageServiceNub - Creates the linkage object for IOStorageFamily
//								to use.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::CreateStorageServiceNub( void )
{
    STATUS_LOG( ( "IOUSBMassStorageUFIDevice::CreateStorageServiceNub entering.\n" ) );

	IOService * 	nub = new IOUFIStorageServices;
	if( nub == NULL )
	{
		ERROR_LOG( ( "IOUSBMassStorageUFIDevice::CreateStorageServiceNub failed\n" ) );
		PANIC_NOW(( "IOUSBMassStorageUFIDevice::CreateStorageServiceNub failed\n" ));
		return;
	}
	
	nub->init();
	
	if( !nub->attach( this ) )
	{
		// panic since the nub can't attach
		PANIC_NOW(( "IOUSBMassStorageUFIDevice::CreateStorageServiceNub unable to attach nub" ));
		return;
	}
	
	nub->registerService();
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::CreateStorageServiceNub exiting.\n" ) );

	nub->release();
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ProcessPoll - Processes a poll for media or media removal.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::ProcessPoll( void )
{
	switch( fPollingMode )
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PollForNewMedia - Polls for new media insertion.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOUSBMassStorageUFIDevice::PollForNewMedia( void )
{
	bool						mediaFound = false;
	UInt64						blockCount;
	UInt64						blockSize;

	// Since this is a poll for new media, 	
	fMediumPresent	= false;

	mediaFound = DetermineMediaPresence();
	if( mediaFound == false )
	{
		return;
	}
	
	if( DetermineMediumCapacity( &blockSize, &blockCount ) == false )
	{
		// Capacity could not be determined, treat it like no media inserted
		// and try again.
		return;
	}
	
	SetMediumCharacteristics( blockSize, blockCount );
	
	fMediumIsWriteProtected = DetermineMediumWriteProtectState();
	
	fMediumPresent	= true;
	
	// Message up the chain that we have media
	messageClients( kIOMessageMediaStateHasChanged,
					 ( void * ) kIOMediaStateOnline,
					 sizeof( IOMediaState ) );
	
	// Media is not locked into the drive, so this is most likely
	// a manually ejectable device, start polling for media removal.
	fPollingMode = kPollingMode_MediaRemoval;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMediaPresence -	Checks if media has been inserted into the
//								device. If medium is detected, this method
//								will return true, else it will return false
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::DetermineMediaPresence( void )
{
	SCSIServiceResponse			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier			request = NULL;
	bool						mediaFound = false;

	STATUS_LOG(( "IOUSBMassStorageUFIDevice::DetermineMediaPresence called" ));

	request = GetSCSITask();
	if( request == NULL )
	{
		return false;
	}
	
	// Do a TEST_UNIT_READY to generate sense data
	if( TEST_UNIT_READY( request ) == true )
    {
    	// The command was successfully built, now send it, set timeout to 10 seconds.
    	serviceResponse = SendCommand( request, 10 * 1000 );
	}
	else
	{
		ERROR_LOG(( "IOUSBMassStorageUFIDevice::DetermineMediaPresence malformed command" ));
		goto CHECK_DONE;
	}
	
	if( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		bool					validSense;
		SCSI_Sense_Data			senseBuffer;
		
		// Check for valid Autosense data.  If it was not retrieved from the 
		// device, explicitly ask for it by sending a REQUEST SENSE command.		
		validSense = GetAutoSenseData( request, &senseBuffer );
		if( validSense == false )
		{
			IOMemoryDescriptor *	bufferDesc;
			
			bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
															kSenseDefaultSize,
															kIODirectionIn );
			if( bufferDesc == NULL )
			{
				ERROR_LOG ( ( "%s: could not allocate sense buffer descriptor.\n", getName() ) );
				goto CHECK_DONE;
			}
			
			// Get the sense data to determine if media is present.
			if( REQUEST_SENSE( request, bufferDesc, kSenseDefaultSize ) == true )
		    {
		    	// The command was successfully built, now send it
		    	serviceResponse = SendCommand( request, 0 );
			}
			else
			{
				ERROR_LOG(( "IOUSBMassStorageUFIDevice::PollForMedia malformed command" ));
				bufferDesc->release();
				goto CHECK_DONE;
			}
			
			bufferDesc->release();
			
			if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
	 			( GetTaskStatus( request ) != kSCSITaskStatus_GOOD ) )
	 		{
				ERROR_LOG ( ( "%s: REQUEST_SENSE failed\n", getName() ) );
				goto CHECK_DONE;
	 		}

		}
		
		if( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) && 
			( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) )
		{
			// Since the device has indicated it needs a start, send it one
			// and then reset the polling.
			if( START_STOP_UNIT( request, 0x00,0x00, 1 ) == true )
		    {
		    	// The command was successfully built, now send it, set timeout to 10 seconds.
		    	serviceResponse = SendCommand( request, 0 );
			}
			
			goto CHECK_DONE;
		}
		else if( ( senseBuffer.ADDITIONAL_SENSE_CODE != 0x00 ) || 
			( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER != 0x00 ) )
		{
			ERROR_LOG ( ( "ASC = 0x%02x, ASCQ = 0x%02x\n",
							senseBuffer.ADDITIONAL_SENSE_CODE,
							senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
			goto CHECK_DONE;
		}
	}
	else
	{
		ERROR_LOG ( ( "serviceResponse = %d\n", serviceResponse ) );
		goto CHECK_DONE;
	}

	UInt8					formatBuffer[12];
	IOMemoryDescriptor *	formatDesc;
	
	formatDesc = IOMemoryDescriptor::withAddress ( ( void * ) &formatBuffer[0],
													12,
													kIODirectionIn );
	if( formatDesc == NULL )
	{
		ERROR_LOG ( ( "%s: could not allocate sense buffer descriptor.\n", getName() ) );
		goto CHECK_DONE;
	}
	
	// If the check makes to to this point, then the TUR returned no errors,
	// now send the READ_FORMAT_CAPACITIES to determine is media is truly present.	
	if ( READ_FORMAT_CAPACITIES( request, formatDesc, 12 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand( request, 0 );
	}
	else
	{
		ERROR_LOG(( "IOUSBMassStorageUFIDevice::PollForMedia malformed command" ));
		formatDesc->release();
		goto CHECK_DONE;
	}

	formatDesc->release();
	
	if( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		if( GetTaskStatus( request ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			bool					validSense;
			SCSI_Sense_Data			senseBuffer;
			
			validSense = GetAutoSenseData( request, &senseBuffer );
			if( validSense == false )
			{
				IOMemoryDescriptor *	bufferDesc;
				
				bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																kSenseDefaultSize,
																kIODirectionIn );
				if( bufferDesc == NULL )
				{
					ERROR_LOG ( ( "%s: could not allocate sense buffer descriptor.\n", getName() ) );
					goto CHECK_DONE;
				}
				
				// Get the sense data to determine if media is present.
				if( REQUEST_SENSE( request, bufferDesc, kSenseDefaultSize ) == true )
			    {
			    	// The command was successfully built, now send it
			    	serviceResponse = SendCommand( request, 0 );
				}
				else
				{
					ERROR_LOG(( "IOUSBMassStorageUFIDevice::PollForMedia malformed command" ));
					bufferDesc->release();
					goto CHECK_DONE;
				}
				
				bufferDesc->release();
				
				// If the REQUEST SENSE comamnd fails to execute, exit and try the
				// poll again.
				if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
		 			( GetTaskStatus( request ) != kSCSITaskStatus_GOOD ) )
		 		{
					ERROR_LOG ( ( "%s: REQUEST_SENSE failed\n", getName() ) );
					goto CHECK_DONE;
		 		}

			}
			
			if( (( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_ILLEGAL_REQUEST ) 
				&& ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x20 ) )
			{
				// The device indicates that the READ_FORMAT_CAPACITIES command
				// is not supported.  Since the device has already returned a good
				// status to the TEST_UNIT_READY, report that media was found.
				mediaFound = true;
				goto CHECK_DONE;
			}
		}
		else if( GetTaskStatus( request ) != kSCSITaskStatus_GOOD )
		{
			goto CHECK_DONE;
		}
	}
	else
	{
		ERROR_LOG ( ( "serviceResponse = %d\n", serviceResponse ) );
		goto CHECK_DONE;
	}

	STATUS_LOG ( ( "%s: Formats data: ", getName() ) );
	for ( int i=0; i < 12; i ++ )
	{
		STATUS_LOG(("%X : ", formatBuffer[i]));
	}
	STATUS_LOG(( "\n" ));

	if( formatBuffer[8] == 0x01 )
	{
		STATUS_LOG ( ( "%s: unformatted media was found.\n", getName() ) );
		// There is unformatted media in the drive, until format support
		// is added, treat like no media is present.
		goto CHECK_DONE;
	}
	else if ( formatBuffer[8] != 0x02 )
	{
		STATUS_LOG ( ( "%s: no media was found.\n", getName() ) );
		// There is no media in the drive, reset the poll.
		goto CHECK_DONE;
	}
	
	STATUS_LOG ( ( "%s: media was found.\n", getName() ) );
	// At this point, it has been determined that there is usable media
	// in the device.
	mediaFound = true;
	
CHECK_DONE:
	if( request != NULL )
	{
		ReleaseSCSITask( request );
		request = NULL;
	}

	return mediaFound;
}

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMediumCapacity - Determines capacity of the medium. Returns true
//								if the capacity could be determined, else it
//								returns false.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOUSBMassStorageUFIDevice::DetermineMediumCapacity( UInt64 * blockSize, UInt64 * blockCount )
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
	if ( READ_CAPACITY( request, bufferDesc, 0, 0x00, 0 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand( request, 0 );
	}
	else
	{
		ERROR_LOG(( "IOUSBMassStorageUFIDevice::PollForMedia malformed command" ));
    	result = false;
    	goto isDone;
	}
		
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus( request ) == kSCSITaskStatus_GOOD ) )
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMediumWriteProtectState - Determines medium write protect state.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::DetermineMediumWriteProtectState( void )
{
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					modeBuffer[8];
	IOMemoryDescriptor *	bufferDesc = NULL;
	SCSITaskIdentifier		request = NULL;
	bool					mediumIsProtected = true;

	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::checkWriteProtection called\n" ) );
		
	request = GetSCSITask();
	if( request == NULL )
	{
		// Since a SCSI Task could not be gotten, do the safe thing and report
		// the medium as write protected.
		return true;
	}

	bufferDesc = IOMemoryDescriptor::withAddress( 	modeBuffer,
													8,
													kIODirectionIn );

	if ( bufferDesc == NULL )
	{
		// Since the Mode Sense data buffer descriptor could not be allocated,
		// the command cannot be sent to the drive, exit and report the medium
		// as write protected.
		goto WP_CHECK_DONE;
	}
	
	if ( MODE_SENSE_10( 	request,
							bufferDesc,
							0,
							0,
							0x3F,
							8 ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand( request, 0 );
	}
	else
	{
		ERROR_LOG(( "IOUSBMassStorageUFIDevice::CheckWriteProtection malformed command" ));
		goto WP_CHECK_DONE;
	}

	if( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus( request ) == kSCSITaskStatus_GOOD ) )
	{
		if( ( modeBuffer[3] & 0x80 ) != 0 )
		{
		 	mediumIsProtected = true;
		}
		else
		{
			mediumIsProtected = false;
		}
	}
	
WP_CHECK_DONE:
	if( bufferDesc != NULL )
	{
		bufferDesc->release();
		bufferDesc = NULL;
	}
	
	if( request != NULL )
	{
		ReleaseSCSITask( request );
		request = NULL;
	}
	
	return mediumIsProtected;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PollForMediaRemoval - Polls for media removal.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOUSBMassStorageUFIDevice::PollForMediaRemoval( void )
{
	SCSIServiceResponse			serviceResponse= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier			request = NULL;
	bool						mediaRemoved = false;
		
	request = GetSCSITask();
	if( request == NULL )
	{
		// A SCSI Task could not be gotten, return immediately.
		return;
	}
		
	// Do a TEST_UNIT_READY to generate sense data
	if( TEST_UNIT_READY( request ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOUSBMassStorageUFIDevice::PollForMedia malformed command" ));
		goto REMOVE_CHECK_DONE;
	}
	
	if( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		bool						validSense;
		SCSI_Sense_Data				senseBuffer;
		IOMemoryDescriptor *		bufferDesc;

		// Check for valid Autosense data.  If it was not retrieved from the 
		// device, explicitly ask for it by sending a REQUEST SENSE command.		
		validSense = GetAutoSenseData( request, &senseBuffer );
		if( validSense == false )
		{
			bufferDesc = IOMemoryDescriptor::withAddress( ( void * ) &senseBuffer,
															kSenseDefaultSize,
															kIODirectionIn );
			if( bufferDesc == NULL )
			{
				ERROR_LOG( ( "%s: could not allocate sense buffer descriptor.\n", getName() ) );
				goto REMOVE_CHECK_DONE;
			}
			
			if( REQUEST_SENSE( request, bufferDesc, kSenseDefaultSize ) == true )
		    {
		    	// The command was successfully built, now send it
		    	serviceResponse = SendCommand( request, 0 );
			}
			else
			{
				PANIC_NOW(( "IOUSBMassStorageUFIDevice::PollForMedia malformed command" ));
				bufferDesc->release();
				goto REMOVE_CHECK_DONE;
			}
			
			bufferDesc->release();
			
			if( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
	 			( GetTaskStatus( request ) != kSCSITaskStatus_GOOD ) )
	 		{
				ERROR_LOG ( ( "%s: REQUEST_SENSE failed\n", getName() ) );
				goto REMOVE_CHECK_DONE;
	 		}

		}
		
		if( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x28 ) ||
			( senseBuffer.ADDITIONAL_SENSE_CODE == 0x3A ) )
		{
			// It has been determined that media has been removed, clean up and 
			// exit.
			mediaRemoved = true;
		}
	}

REMOVE_CHECK_DONE:
	if( request != NULL )
	{
		ReleaseSCSITask( request );
		request = NULL;
	}
			
	if( mediaRemoved == true )
	{
		// Media was removed, set the polling to determine when new media has been inserted
 		fPollingMode = kPollingMode_NewMedia;
		
		// Message up the chain that we do not have media
		messageClients( kIOMessageMediaStateHasChanged,
						( void * ) kIOMediaStateOffline );
	}
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetInitialPowerState - Currently does nothing.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOUSBMassStorageUFIDevice::GetInitialPowerState ( void )
{
	return 0;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandlePowerChange - Currently does nothing.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOUSBMassStorageUFIDevice::HandlePowerChange ( void )
{
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandleCheckPowerState - Currently does nothing.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOUSBMassStorageUFIDevice::HandleCheckPowerState ( void )
{
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TicklePowerManager - Currently does nothing.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOUSBMassStorageUFIDevice::TicklePowerManager ( void )
{
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetNumberOfPowerStateTransitions - Currently does nothing.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOUSBMassStorageUFIDevice::GetNumberOfPowerStateTransitions ( void )
{
	return 0;
}


#pragma mark -
#pragma mark *** Client Requests Support ***
#pragma mark -



//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueRead - Performs the Synchronous Read Request				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn 
IOUSBMassStorageUFIDevice::IssueRead( 	IOMemoryDescriptor *	buffer,
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
					( SCSICmdField2Byte ) blockCount ) == false )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOUSBMassStorageUFIDevice::IssueRead malformed command" ));
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueRead - Performs the Asynchronous Read Request			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn 
IOUSBMassStorageUFIDevice::IssueRead( 	IOMemoryDescriptor *	buffer,
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
					( SCSICmdField2Byte ) blockCount ) == true )
    {
    	// The command was successfully built, now send it
    	SetApplicationLayerReference( request, clientData );
		STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::IssueRead send command.\n" ) );
    	SendCommand( request, 0, &this->AsyncReadWriteComplete );
	}
	else
	{
		PANIC_NOW(( "IOUSBMassStorageUFIDevice::IssueWrite malformed command" ));
		status = kIOReturnError;
	}

	return status;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueWrite - Performs the Synchronous Write Request			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn 
IOUSBMassStorageUFIDevice::IssueWrite( 	IOMemoryDescriptor *	buffer,
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
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount ) == true )
    {
    	// The command was successfully built, now send it
    	serviceResponse = SendCommand( request, 0 );
	}
	else
	{
		PANIC_NOW(( "IOUSBMassStorageUFIDevice::IssueWrite malformed command" ));
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueWrite - Performs the Asynchronous Write Request			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn 
IOUSBMassStorageUFIDevice::IssueWrite(	IOMemoryDescriptor *	buffer,
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
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount ) == true )
    {
    	// The command was successfully built, now send it
    	SetApplicationLayerReference( request, clientData );
		STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::IssueWrite send command.\n" ) );
    	SendCommand( request, 0, &this->AsyncReadWriteComplete );
	}
	else
	{
		PANIC_NOW(( "IOUSBMassStorageUFIDevice::IssueWrite malformed command" ));
	}

	return status;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SyncReadWrite - 	Translates a synchronous I/O request into a
//						read or a write.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn 
IOUSBMassStorageUFIDevice::SyncReadWrite ( 	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount,
                         					UInt64					blockSize )
{
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
		ERROR_LOG ( ( "%s: doSyncReadWrite bad direction argument\n", getName() ) );
		theErr = kIOReturnBadArgument;
	}
	
	return theErr;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWrite - 	Translates a asynchronous I/O request into a
//						read or a write.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn 
IOUSBMassStorageUFIDevice::AsyncReadWrite (	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount,
                         					UInt64					blockSize,
											void *					clientData )
{
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
		ERROR_LOG ( ( "%s: doAsyncReadWrite bad direction argument\n", getName() ) );
		theErr = kIOReturnBadArgument;
	}
	
	return theErr;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EjectTheMedium - Changes the polling mode to poll for medium removal.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn 
IOUSBMassStorageUFIDevice::EjectTheMedium( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::EjectTheMedium called\n" ) );
	
	ResetMediumCharacteristics();
	
	// Set the polling to determine when media has been removed
	fPollingMode = kPollingMode_MediaRemoval;
    	
	EnablePolling();
		
	return kIOReturnSuccess;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FormatMedium - 	Currently does nothing.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOUSBMassStorageUFIDevice::FormatMedium( UInt64 blockCount, UInt64 blockSize )
{
	IOReturn	theErr = kIOReturnSuccess;

    STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::FormatMedium called\n" ) );

	return theErr;	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetFormatCapacities - Currently does nothing.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOUSBMassStorageUFIDevice::GetFormatCapacities(	UInt64 * capacities,
												UInt32   capacitiesMaxCount ) const
{
    STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::doGetFormatCapacities called\n" ) );

	return 0;
}


#pragma mark -
#pragma mark *** Device Information Retrieval Methods ***
#pragma mark -

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetVendorString - Returns the vendor string.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOUSBMassStorageUFIDevice::GetVendorString ( void )
{
	OSString *		vendorString;
	
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	
	vendorString = ( OSString * ) fDeviceCharacteristicsDictionary->getObject( kIOPropertyVendorNameKey );
	if ( vendorString != NULL )
	{
		return ( ( char * ) vendorString->getCStringNoCopy ( ) );
	}
	else
	{
		return "NULL STRING";
	}
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetProductString - Returns the Product String.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOUSBMassStorageUFIDevice::GetProductString ( void )
{
	OSString *		productString;
	
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	
	productString = ( OSString * ) fDeviceCharacteristicsDictionary->getObject( kIOPropertyProductNameKey );
	if ( productString != NULL )
	{
		return ( ( char * ) productString->getCStringNoCopy ( ) );
	}
	else
	{
		return "NULL STRING";
	}
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetRevisionString - Returns the Revision String.			   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

char *
IOUSBMassStorageUFIDevice::GetRevisionString ( void )
{
	OSString *		revisionString;
	
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	
	revisionString = ( OSString * ) fDeviceCharacteristicsDictionary->getObject( kIOPropertyProductRevisionLevelKey );
	if ( revisionString )
	{
		return ( ( char * ) revisionString->getCStringNoCopy ( ) );
	}
	else
	{
		return "NULL STRING";
	}
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetProtocolCharacteristicsDictionary - 	Returns the Protocol
//												Characteristics Dictionary.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

OSDictionary *
IOUSBMassStorageUFIDevice::GetProtocolCharacteristicsDictionary ( void )
{
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	return ( OSDictionary * ) getProperty( kIOPropertyProtocolCharacteristicsKey );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetDeviceCharacteristicsDictionary - 	Returns the Device
//												Characteristics Dictionary.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

OSDictionary *
IOUSBMassStorageUFIDevice::GetDeviceCharacteristicsDictionary ( void )
{
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );	
	return fDeviceCharacteristicsDictionary;
}


#pragma mark -
#pragma mark *** Query methods to report device characteristics ***
#pragma mark -

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportDeviceMaxBlocksReadTransfer -	Reports the max number of blocks
//											a device can handle per read.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOUSBMassStorageUFIDevice::ReportDeviceMaxBlocksReadTransfer( void )
{
	UInt64	maxBlockCount;

    STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::reportMaxReadTransfer\n" ) );

	maxBlockCount = 256;

	return maxBlockCount;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportDeviceMaxBlocksWriteTransfer -	Reports the max number of blocks
//											a device can handle per write.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOUSBMassStorageUFIDevice::ReportDeviceMaxBlocksWriteTransfer( void )
{
	UInt64	maxBlockCount;
	
    STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::reportMaxWriteTransfer.\n" ) );

	maxBlockCount = 256;

	return maxBlockCount;
}


#pragma mark -
#pragma mark *** Query methods to report installed medium characteristics ***
#pragma mark -

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMediumBlockSize -	Reports the medium block size.		   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64 
IOUSBMassStorageUFIDevice::ReportMediumBlockSize( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::ReportMediumBlockSize blockSize = %ld\n", ( UInt32 ) fMediumBlockSize ) );
	return fMediumBlockSize;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMediumTotalBlockCount -	Reports the number of blocks on the medium
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOUSBMassStorageUFIDevice::ReportMediumTotalBlockCount( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::ReportMediumTotalBlockCount maxBlock = %ld\n", fMediumBlockCount ) );
	return fMediumBlockCount;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMediumWriteProtection -	Reports whether the medium is write 
//									protected						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::ReportMediumWriteProtection( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFIDevice::ReportMediumWriteProtection isWriteProtected = %d.\n", fMediumIsWriteProtected ) );	
	return fMediumIsWriteProtected;
}


#pragma mark -
#pragma mark *** Command Builders Utility Methods ***
#pragma mark -


// Utility routines used by all SCSI Command Set objects

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IsParameterValid - Validate Parameter used for 1 bit to 1 byte paramaters
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::IsParameterValid( SCSICmdField1Byte param,
										SCSICmdField1Byte mask )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::IsParameterValid called\n" ) );
	
	if( ( param | mask ) != mask )
	{
		STATUS_LOG( ( "param = %x not valid, mask = %x\n", param, mask ) );
		return false;
	}
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IsParameterValid - Validate Parameter used for 9 bit to 2 byte paramaters
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::IsParameterValid( SCSICmdField2Byte param,
										SCSICmdField2Byte mask )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::IsParameterValid called\n" ) );
	
	if( ( param | mask ) != mask )
	{
		STATUS_LOG ( ( "param = %x not valid, mask = %x\n", param, mask ) );
		return false;
	}
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IsParameterValid -	Validate Parameter used for 17 bit to 4 byte
//							paramaters								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::IsParameterValid( SCSICmdField4Byte param,
										SCSICmdField4Byte mask )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::IsParameterValid called\n" ) );
	
	if( ( param | mask ) != mask )
	{
		STATUS_LOG( ( "param = %x not valid, mask = %x\n", param, mask ) );
		return false;
	}
	
	return true;
}


#pragma mark -
#pragma mark *** Command Builder Methods ***
#pragma mark -

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FORMAT_UNIT - Command Builder									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::FORMAT_UNIT(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer,
			    			IOByteCount					defectListSize,
			    			SCSICmdField1Byte 			TRACK_NUMBER, 
			    			SCSICmdField2Byte 			INTERLEAVE )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::FORMAT_UNIT called\n" ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	// Do the pre-flight check on the passed in parameters
	if( IsParameterValid( INTERLEAVE, kSCSICmdFieldMask2Byte ) == false )
	{
		STATUS_LOG( ( "INTERLEAVE = %x not valid\n", INTERLEAVE ) );
		return false;
	}

	if( defectListSize > 0 )
	{
		// We have data to send to the device, 
		// make sure that we were given a valid buffer
		if( IsMemoryDescriptorValid( dataBuffer, defectListSize  )
				== false )
		{
			STATUS_LOG( ( "dataBuffer = %x not valid, defectListSize = %x\n",
							dataBuffer, defectListSize ) );
			return false;
		}
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
								kSCSICmd_FORMAT_UNIT,
								0x00,
								0x00,
								( INTERLEAVE >> 8 ) & 0xFF,
								  INTERLEAVE		& 0xFF,
								0x00 );
 	
	// The client has requested a DEFECT LIST be sent to the device
	// to be used with the format command
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_FromInitiatorToTarget );
	SetDataBuffer( 				request,
								dataBuffer );
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ INQUIRY - Command Builder										[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::INQUIRY(
							SCSITaskIdentifier			request,
    						IOMemoryDescriptor 			*dataBuffer,
    						SCSICmdField1Byte 			PAGE_OR_OPERATION_CODE,
    						SCSICmdField1Byte 			ALLOCATION_LENGTH )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::INQUIRY called\n" ) );

	if ( ResetForNewTask( request ) == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	if ( IsParameterValid( PAGE_OR_OPERATION_CODE, kSCSICmdFieldMask1Byte ) == false )
	{
		STATUS_LOG( ( "PAGE_OR_OPERATION_CODE = %x not valid\n",
						PAGE_OR_OPERATION_CODE ) );
		return false;
	}
	
	if ( IsParameterValid( ALLOCATION_LENGTH, kSCSICmdFieldMask1Byte ) == false )
	{
		STATUS_LOG( ( "ALLOCATION_LENGTH = %x not valid\n", ALLOCATION_LENGTH ) );
		return false;
	}
	
	if ( IsMemoryDescriptorValid( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		STATUS_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
	}
		
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
								kSCSICmd_INQUIRY,
								0x00,
								PAGE_OR_OPERATION_CODE,
								0x00,
								ALLOCATION_LENGTH,
								0x00 );
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );
	SetDataBuffer( 				request,
								dataBuffer );
	SetRequestedDataTransferCount( 	request,
								ALLOCATION_LENGTH );
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ MODE_SELECT_10 - Command Builder								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOUSBMassStorageUFIDevice::MODE_SELECT_10(
							SCSITaskIdentifier			request,
    						IOMemoryDescriptor 			*dataBuffer,
    						SCSICmdField1Bit 			PF,
    						SCSICmdField1Bit 			SP,
    						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::MODE_SELECT_10 called\n" ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	if( IsParameterValid( PF, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "PF = %x not valid\n", PF ) );
		return false;
	}

	if( IsParameterValid( SP, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( "SP = %x not valid\n", SP ) );
		return false;
	}

	if( IsParameterValid( PARAMETER_LIST_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		STATUS_LOG( ( "PARAMETER_LIST_LENGTH = %x not valid\n",
						PARAMETER_LIST_LENGTH ) );
		return false;
	}

	if( IsMemoryDescriptorValid( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{
		STATUS_LOG( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
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
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_FromInitiatorToTarget );
	SetDataBuffer( 				request,
								dataBuffer );
	SetRequestedDataTransferCount( 	request,
								PARAMETER_LIST_LENGTH );
	
	return true;
}
  

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ MODE_SENSE_10 - Command Builder								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOUSBMassStorageUFIDevice::MODE_SENSE_10(
							SCSITaskIdentifier			request,
    						IOMemoryDescriptor 			*dataBuffer,
    						SCSICmdField1Bit 			DBD,
	   						SCSICmdField2Bit 			PC,
	   						SCSICmdField6Bit 			PAGE_CODE,
	   						SCSICmdField2Byte 			ALLOCATION_LENGTH )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::MODE_SENSE_10 called\n" ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	if( IsParameterValid( DBD, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "DBD = %x not valid\n", DBD ) );
		return false;
	}

	if( IsParameterValid( PC, kSCSICmdFieldMask2Bit ) == false )
	{
		STATUS_LOG( ( "PC = %x not valid\n", PC ) );
		return false;
	}

	if( IsParameterValid( PAGE_CODE, kSCSICmdFieldMask6Bit ) == false )
	{
		STATUS_LOG( ( "PAGE_CODE = %x not valid\n", PAGE_CODE ) );
		return false;
	}

	if( IsParameterValid( ALLOCATION_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		STATUS_LOG( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
	}

	if( IsMemoryDescriptorValid( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		STATUS_LOG( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
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
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );
	SetDataBuffer( 				request,
								dataBuffer );
	SetRequestedDataTransferCount( 	request,
								ALLOCATION_LENGTH );
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PREVENT_ALLOW_MEDIUM_REMOVAL - Command Builder				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::PREVENT_ALLOW_MEDIUM_REMOVAL( 
							SCSITaskIdentifier			request,
	     					SCSICmdField1Bit 			PREVENT )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::PREVENT_ALLOW_MEDIUM_REMOVAL called\n" ) );

	if ( ResetForNewTask( request ) == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	if( IsParameterValid( PREVENT, kSCSICmdFieldMask2Bit ) == false )
	{
		STATUS_LOG( ( "PREVENT = %x not valid\n", PREVENT ) );
		return false;
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
								kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL,
								0x00,
								0x00,
								0x00,
								PREVENT,
								0x00 );
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_10 - Command Builder										[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOUSBMassStorageUFIDevice::READ_10(
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
	
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::READ_10 called\n" ) );
	
	if ( ResetForNewTask( request ) == false )
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
		// buffer is large enough to accomodate this request.
		if ( dataBuffer->getLength() < requestedByteCount )
		{
			return false;
		}
	}

	// Do the pre-flight check on the passed in parameters
	if( IsParameterValid( DPO, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "DPO = %x not valid\n", DPO ) );
		return false;
	}

	if( IsParameterValid( FUA, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "FUA = %x not valid\n", FUA ) );
		return false;
	}

	if( IsParameterValid( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
	}

	if( IsParameterValid( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		STATUS_LOG( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
	}

	if( IsParameterValid( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		STATUS_LOG( ( "TRANSFER_LENGTH = %x not valid\n",
						TRANSFER_LENGTH ) );
		return false;
	}

	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
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
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );	
	SetDataBuffer( 				request,
								dataBuffer );	
	SetRequestedDataTransferCount( 	request,
								requestedByteCount );	
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_12 - Command Builder										[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOUSBMassStorageUFIDevice::READ_12(
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

	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::READ_12 called\n" ) );
	
	if ( ResetForNewTask( request ) == false )
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
		// buffer is large enough to accomodate this request.
		if ( dataBuffer->getLength() < requestedByteCount )
		{
			return false;
		}
	}

	// Do the pre-flight check on the passed in parameters
	if( IsParameterValid( DPO, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "DPO = %x not valid\n", DPO ) );
		return false;
	}

	if( IsParameterValid( FUA, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "FUA = %x not valid\n", FUA ) );
		return false;
	}

	if( IsParameterValid( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
	}

	if( IsParameterValid( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		STATUS_LOG( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
	}

	if( IsParameterValid( TRANSFER_LENGTH, kSCSICmdFieldMask4Byte ) == false )
	{
		STATUS_LOG( ( "TRANSFER_LENGTH = %x not valid\n",
						TRANSFER_LENGTH ) );
		return false;
	}

	// This is a 12-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
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
	SetDataBuffer( 				request,
								dataBuffer );	
	SetRequestedDataTransferCount( 	request,
								requestedByteCount );	

	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_CAPACITY - Command Builder								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::READ_CAPACITY(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			SCSICmdField1Bit 			RELADR,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField1Bit 			PMI )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::READ_CAPACITY called\n" ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
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
		STATUS_LOG( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
	}
	
	if( IsParameterValid( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		STATUS_LOG( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
	}

	if( IsParameterValid( PMI, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "PMI = %x not valid\n", PMI ) );
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_FORMAT_CAPACITIES - Command Builder						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::READ_FORMAT_CAPACITIES(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			SCSICmdField2Byte 			ALLOCATION_LENGTH )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::READ_CAPACITY called\n" ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
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
		STATUS_LOG( ( "ALLOCATION_LENGTH = %x not valid\n", ALLOCATION_LENGTH ) );
		return false;
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
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
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );	
	SetDataBuffer( 				request,
								dataBuffer );	
	SetRequestedDataTransferCount( 	request,
								ALLOCATION_LENGTH );	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REQUEST_SENSE - Command Builder								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::REQUEST_SENSE(
							SCSITaskIdentifier			request,
   							IOMemoryDescriptor 			*dataBuffer,
			    			SCSICmdField1Byte 			ALLOCATION_LENGTH )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::REQUEST_SENSE called\n" ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	if( IsParameterValid( ALLOCATION_LENGTH, kSCSICmdFieldMask1Byte ) == false )
	{
		STATUS_LOG( ( "ALLOCATION_LENGTH = %x not valid\n",
						ALLOCATION_LENGTH ) );
		return false;
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
								kSCSICmd_REQUEST_SENSE,
								0x00,
								0x00,
								0x00,
								ALLOCATION_LENGTH,
								0x00 );
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_FromTargetToInitiator );
	SetDataBuffer( 				request,
								dataBuffer );
	SetRequestedDataTransferCount( 	request,
								ALLOCATION_LENGTH );
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REZERO_UNIT - Command Builder									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::REZERO_UNIT( 
							SCSITaskIdentifier			request )
{
	return false;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEEK - Command Builder										[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOUSBMassStorageUFIDevice::SEEK( 
							SCSITaskIdentifier			request,
			    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS )
{
	return false;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEND_DIAGNOSTICS - Command Builder							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOUSBMassStorageUFIDevice::SEND_DIAGNOSTICS( 
							SCSITaskIdentifier			request,
							SCSICmdField1Bit 			PF, 
							SCSICmdField1Bit 			SELF_TEST, 
							SCSICmdField1Bit 			DEF_OFL, 
							SCSICmdField1Bit 			UNIT_OFL )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::SEND_DIAGNOSTICS called\n" ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	if( IsParameterValid( PF, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "PF = %x not valid\n", PF ) );
		return false;
	}
	
	if( IsParameterValid ( SELF_TEST, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG ( ( "SELF_TEST = %x not valid\n", SELF_TEST ) );
		return false;
	}
	
	if( IsParameterValid( DEF_OFL, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "DEF_OFL = %x not valid\n", DEF_OFL ) );
		return false;
	}
	
	if( IsParameterValid( UNIT_OFL, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "UNIT_OFL = %x not valid\n", UNIT_OFL ) );
		return false;
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
								kSCSICmd_SEND_DIAGNOSTICS,
								( PF << 4 ) |
									( SELF_TEST << 2 ) | ( DEF_OFL << 1 ) | UNIT_OFL,
								0x00,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_NoDataTransfer);
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ START_STOP_UNIT - Command Builder								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::START_STOP_UNIT( 
							SCSITaskIdentifier			request,
							SCSICmdField1Bit 			IMMED, 
							SCSICmdField1Bit 			LOEJ, 
							SCSICmdField1Bit 			START )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::START_STOP_UNIT called\n" ) );

	if ( ResetForNewTask( request ) == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	// Do the pre-flight check on the passed in parameters
	if( IsParameterValid( IMMED, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "IMMED = %x not valid\n", IMMED ) );
		return false;
	}

	if( IsParameterValid( LOEJ, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "LOEJ = %x not valid\n", LOEJ ) );
		return false;
	}

	if( IsParameterValid( START, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "START = %x not valid\n", START ) );
		return false;
	}

	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
								kSCSICmd_START_STOP_UNIT,
								IMMED,
								0x00,
								0x00,
								( LOEJ << 1 ) | START,
								0x00 );
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TEST_UNIT_READY - Command Builder								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::TEST_UNIT_READY(  
							SCSITaskIdentifier			request )
{
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::TEST_UNIT_READY called\n" ) );
	
	if ( ResetForNewTask( request ) == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
								kSCSICmd_TEST_UNIT_READY,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ VERIFY - Command Builder										[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::VERIFY( 
							SCSITaskIdentifier			request,
							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			BYTCHK, 
							SCSICmdField1Bit 			RELADR,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			VERIFICATION_LENGTH )
{
	return false;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_10 - Command Builder									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFIDevice::WRITE_10(
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
	
	STATUS_LOG( ( "IOUSBMassStorageUFIDevice::WRITE_10 called\n" ) );

	if ( ResetForNewTask( request ) == false )
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
		// buffer is large enough to accomodate this request.
		if ( dataBuffer->getLength() < requestedByteCount )
		{
			return false;
		}
	}

	// Do the pre-flight check on the passed in parameters
	if( IsParameterValid ( DPO, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "DPO = %x not valid\n", DPO ) );
		return false;
	}

	if( IsParameterValid( FUA, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "FUA = %x not valid\n", FUA ) );
		return false;
	}

	if( IsParameterValid( RELADR, kSCSICmdFieldMask1Bit ) == false )
	{
		STATUS_LOG( ( "RELADR = %x not valid\n", RELADR ) );
		return false;
	}

	if( IsParameterValid( LOGICAL_BLOCK_ADDRESS, kSCSICmdFieldMask4Byte ) == false )
	{
		STATUS_LOG( ( "LOGICAL_BLOCK_ADDRESS = %x not valid\n",
						LOGICAL_BLOCK_ADDRESS ) );
		return false;
	}

	if( IsParameterValid( TRANSFER_LENGTH, kSCSICmdFieldMask2Byte ) == false )
	{
		STATUS_LOG( ( "TRANSFER_LENGTH = %x not valid\n",
						TRANSFER_LENGTH ) );
		return false;
	}

	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	request,
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
	
	SetDataTransferDirection( 	request,
								kSCSIDataTransfer_FromInitiatorToTarget );
	SetDataBuffer( 				request,
								dataBuffer );
	SetRequestedDataTransferCount( 	request,
								requestedByteCount );
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_12 - Command Builder									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOUSBMassStorageUFIDevice::WRITE_12(
							SCSITaskIdentifier			request,
							IOMemoryDescriptor *		dataBuffer, 
			    			UInt32						blockSize,
			    			SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			EBP, 
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			TRANSFER_LENGTH )
{
	return false;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_AND_VERIFY - Command Builder							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOUSBMassStorageUFIDevice::WRITE_AND_VERIFY(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			UInt32						blockSize,
			    			SCSICmdField1Bit 			DPO,
			    			SCSICmdField1Bit 			BYTCHK, 
			    			SCSICmdField1Bit 			RELADR, 
			    			SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS, 
			    			SCSICmdField2Byte 			TRANSFER_LENGTH )
{
	return false;
}


#pragma mark -
#pragma mark *** IOUSBMassStorageUFISubclass methods ***
#pragma mark -

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ BeginProvidedServices											[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOUSBMassStorageUFISubclass::BeginProvidedServices( void )
{
	// Call our superclass
	IOUSBMassStorageClass::BeginProvidedServices( );
	
	// Create the IOUSBMassStorageUFIDevice object
	IOUSBMassStorageUFIDevice * 	ufiDevice = new IOUSBMassStorageUFIDevice;
	if( ufiDevice == NULL )
	{
		ERROR_LOG( ( "IOUSBMassStorageUFISubclass::BeginProvidedServices failed\n" ) );
		PANIC_NOW( ( "IOUSBMassStorageUFISubclass::BeginProvidedServices failed\n" ) );
		return false;
	}
	
	ufiDevice->init( NULL );
	
	if( !ufiDevice->attach( this ) )
	{
		// panic since the nub can't attach
		PANIC_NOW(( "IOUSBMassStorageUFISubclass::BeginProvidedServices unable to attach nub" ));
		return false;
	}
	
	ufiDevice->registerService();
	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::BeginProvidedServices exiting.\n" ) );
	
	ufiDevice->release();
	
	return true;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EndProvidedServices											[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool	
IOUSBMassStorageUFISubclass::EndProvidedServices( void )
{
	return true;
}
