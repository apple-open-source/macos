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

// This class' header file
#include <IOKit/usb/IOUSBMassStorageUFISubclass.h>

#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/usb/IOUFIStorageServices.h>
#include <IOKit/scsi-commands/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi-commands/SCSICommandOperationCodes.h>

#include "Debugging.h"

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

#define kKeySwitchProperty			"Keyswitch"
#define kAppleKeySwitchProperty		"AppleKeyswitch"

#define super IOUSBMassStorageClass
OSDefineMetaClassAndStructors( IOUSBMassStorageUFISubclass, IOUSBMassStorageClass )


void
IOUSBMassStorageUFISubclass::free ( void )
{
	
	// Check first that fIOUSBMassStorageUFISubclassReserved is not NULL
	// so that we don't dereference it when we shouldn't when we
	// check if fKeySwitchNotifier is NULL since fKeySwitchNotifier is
	// defined to be fIOUSBMassStorageUFISubclassReserved->fKeySwitchNotifier
	if ( fIOUSBMassStorageUFISubclassReserved != NULL )
	{
		
		if ( fKeySwitchNotifier != NULL )
		{
			
			fKeySwitchNotifier->remove ( );
			fKeySwitchNotifier = NULL;
			
		}
		
		IOFree ( fIOUSBMassStorageUFISubclassReserved,
				 sizeof ( IOUSBMassStorageUFISubclassExpansionData ) );
		fIOUSBMassStorageUFISubclassReserved = NULL;
		
	}
	
	// Make sure to call our super
	super::free ( );
	
}


IOReturn
IOUSBMassStorageUFISubclass::message( UInt32 type, IOService * provider, void * argument )
{
	
	IOReturn	result;
	
	switch( type )
	{
		
		case kIOMessageServiceIsRequestingClose:
		{
			// Check first that fIOUSBMassStorageUFISubclassReserved is not NULL
			// so that we don't dereference it when we shouldn't when we
			// check if fKeySwitchNotifier is NULL since fKeySwitchNotifier is
			// defined to be fIOUSBMassStorageUFISubclassReserved->fKeySwitchNotifier
			if ( fIOUSBMassStorageUFISubclassReserved != NULL )
			{
				if ( fKeySwitchNotifier != NULL )
				{
					
					fKeySwitchNotifier->remove ( );
					fKeySwitchNotifier = NULL;
					
				}
			}
			result = super::message( type, provider, argument );
		}
		break;
		
		default:
		{
			result = super::message( type, provider, argument );
		}
	}
	
	return result;
	
}


bool
IOUSBMassStorageUFISubclass::BeginProvidedServices( void )
{
	fDeviceCharacteristicsDictionary = OSDictionary::withCapacity( 1 );
	if( fDeviceCharacteristicsDictionary == NULL )
	{
		ERROR_LOG( ( "%s: couldn't device characteristics dictionary.\n", getName() ) );
		return false;
	}
	
	if( InitializeDeviceSupport() == false )
	{
		return false;
	}

	// Create what will be the client.
	CreateStorageServiceNub();

	// Enable the check for media	
	EnablePolling();		

	return true;
}

bool	
IOUSBMassStorageUFISubclass::EndProvidedServices( void )
{
	return true;
}

#pragma mark -
#pragma mark Static Class Methods

void 
IOUSBMassStorageUFISubclass::sProcessPoll( void * theUFIDriver, void * refCon )
{
	IOUSBMassStorageUFISubclass *	driver;
	
	driver = (IOUSBMassStorageUFISubclass *) theUFIDriver;
	driver->ProcessPoll();
	if( driver->fPollingMode != kPollingMode_Suspended )
	{
		// schedule the poller again
		driver->EnablePolling();
	}
	
	// drop the retain associated with this poll
	driver->release();
}


#pragma mark -

bool 
IOUSBMassStorageUFISubclass::InitializeDeviceSupport( void )
{
	bool setupSuccessful 	= false;
	
	// Initialize the medium characteristics
	fMediumPresent			= false;
	fMediumIsWriteProtected	= true;

    STATUS_LOG( ( "IOUSBMassStorageUFISubclass::InitializeDeviceSupport called\n" ) );

	ClearNotReadyStatus();

	require( ( DetermineDeviceCharacteristics( ) == true ), ERROR_EXIT );
	
	fPollingMode = kPollingMode_NewMedia;
	fPollingThread = thread_call_allocate(
					( thread_call_func_t ) IOUSBMassStorageUFISubclass::sProcessPoll,
					( thread_call_param_t ) this );
	
	require_nonzero( fPollingThread, ERROR_EXIT );
	
	fIOUSBMassStorageUFISubclassReserved = ( IOUSBMassStorageUFISubclassExpansionData * )
			IOMalloc ( sizeof ( IOUSBMassStorageUFISubclassExpansionData ) );
	
	require_nonzero ( fIOUSBMassStorageUFISubclassReserved, ERROR_EXIT );
	
	bzero ( fIOUSBMassStorageUFISubclassReserved,
			sizeof ( IOUSBMassStorageUFISubclassExpansionData ) );
	
	// Add a notification for the Apple KeySwitch on the server.
	fKeySwitchNotifier = addNotification (
			gIOMatchedNotification,
			nameMatching ( kAppleKeySwitchProperty ),
			( IOServiceNotificationHandler ) &IOUSBMassStorageUFISubclass::ServerKeyswitchCallback,
			this,
			0 );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::InitializeDeviceSupport setupSuccessful = %d\n", setupSuccessful ) );
	
	setupSuccessful = true;
	
ERROR_EXIT:
	return setupSuccessful;
}

void 
IOUSBMassStorageUFISubclass::SuspendDeviceSupport( void )
{
	if( fPollingMode != kPollingMode_Suspended )
	{
    	DisablePolling();
    }		
}

void 
IOUSBMassStorageUFISubclass::ResumeDeviceSupport( void )
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
IOUSBMassStorageUFISubclass::TerminateDeviceSupport( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::cleanUp called.\n" ) );

    if ( fPollingThread != NULL )
    {
        thread_call_free ( fPollingThread );
        fPollingThread = NULL;
    }
}

bool
IOUSBMassStorageUFISubclass::ClearNotReadyStatus( void )
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
			PANIC_NOW( ( "IOUSBMassStorageUFISubclass::ClearNotReadyStatus malformed command" ) );
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
						PANIC_NOW( ( "IOUSBMassStorageUFISubclass::ClearNotReadyStatus malformed command" ) );
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

void 
IOUSBMassStorageUFISubclass::EnablePolling( void )
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

void 
IOUSBMassStorageUFISubclass::DisablePolling( void )
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
IOUSBMassStorageUFISubclass::DetermineDeviceCharacteristics( void )
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

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::DetermineDeviceCharacteristics called\n" ) );

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
		PANIC_NOW( ( "IOUSBMassStorageUFISubclass::DetermineDeviceCharacteristics malformed command" ) );
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

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::DetermineDeviceCharacteristics exiting\n" ) );

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


void 
IOUSBMassStorageUFISubclass::SetMediumCharacteristics( UInt32 blockSize, UInt32 blockCount )
{
    STATUS_LOG( ( "IOUSBMassStorageUFISubclass::SetMediumCharacteristics called\n" ) );
	STATUS_LOG( ( "mediumBlockSize = %ld, blockCount = %ld\n", blockSize, blockCount ) );
	
	fMediumBlockSize	= blockSize;
	fMediumBlockCount	= blockCount;
	
	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::SetMediumCharacteristics exiting\n" ) );
}


void 
IOUSBMassStorageUFISubclass::ResetMediumCharacteristics( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::ResetMediumCharacteristics called\n" ) );
	fMediumBlockSize		= 0;
	fMediumBlockCount		= 0;
	fMediumPresent			= false;
	fMediumIsWriteProtected = true;
	STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::ResetMediumCharacteristics exiting\n" ) );
}


void 
IOUSBMassStorageUFISubclass::CreateStorageServiceNub( void )
{
    STATUS_LOG( ( "IOUSBMassStorageUFISubclass::CreateStorageServiceNub entering.\n" ) );

	IOService * 	nub = new IOUFIStorageServices;
	if( nub == NULL )
	{
		ERROR_LOG( ( "IOUSBMassStorageUFISubclass::CreateStorageServiceNub failed\n" ) );
		PANIC_NOW(( "IOUSBMassStorageUFISubclass::CreateStorageServiceNub failed\n" ));
		return;
	}
	
	nub->init();
	
	if( !nub->attach( this ) )
	{
		// panic since the nub can't attach
		PANIC_NOW(( "IOUSBMassStorageUFISubclass::CreateStorageServiceNub unable to attach nub" ));
		return;
	}
	
	nub->registerService();
	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::CreateStorageServiceNub exiting.\n" ) );

	nub->release();
}


void 
IOUSBMassStorageUFISubclass::ProcessPoll( void )
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

void 
IOUSBMassStorageUFISubclass::PollForNewMedia( void )
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

// Check if media has been inserted into the device.
// if medium is detected, this method will return true, 
// else it will return false
bool
IOUSBMassStorageUFISubclass::DetermineMediaPresence( void )
{
	SCSIServiceResponse			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier			request = NULL;
	bool						mediaFound = false;

	STATUS_LOG(( "IOUSBMassStorageUFISubclass::DetermineMediaPresence called" ));

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
		ERROR_LOG(( "IOUSBMassStorageUFISubclass::DetermineMediaPresence malformed command" ));
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
				ERROR_LOG(( "IOUSBMassStorageUFISubclass::PollForMedia malformed command" ));
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
		ERROR_LOG(( "IOUSBMassStorageUFISubclass::PollForMedia malformed command" ));
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
					ERROR_LOG(( "IOUSBMassStorageUFISubclass::PollForMedia malformed command" ));
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

// Returns true if the capacity could be determined, else it returns false.
bool 
IOUSBMassStorageUFISubclass::DetermineMediumCapacity( UInt64 * blockSize, UInt64 * blockCount )
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
		ERROR_LOG(( "IOUSBMassStorageUFISubclass::PollForMedia malformed command" ));
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

bool 
IOUSBMassStorageUFISubclass::DetermineMediumWriteProtectState( void )
{
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					modeBuffer[8];
	IOMemoryDescriptor *	bufferDesc = NULL;
	SCSITaskIdentifier		request = NULL;
	bool					mediumIsProtected = true;

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::checkWriteProtection called\n" ) );
		
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
		ERROR_LOG(( "IOUSBMassStorageUFISubclass::CheckWriteProtection malformed command" ));
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

void 
IOUSBMassStorageUFISubclass::PollForMediaRemoval( void )
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
		PANIC_NOW(( "IOUSBMassStorageUFISubclass::PollForMedia malformed command" ));
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
				PANIC_NOW(( "IOUSBMassStorageUFISubclass::PollForMedia malformed command" ));
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


bool
IOUSBMassStorageUFISubclass::ServerKeyswitchCallback (
									void *			target,
									void * 			refCon,
									IOService * 	newDevice )
{
	
	OSBoolean *						shouldNotPoll	= NULL;
	IOUSBMassStorageUFISubclass *	device			= NULL;
	
	STATUS_LOG( ( "ServerKeyswitchCallback called.\n" ) );
	
	shouldNotPoll = OSDynamicCast (
							OSBoolean,
							newDevice->getProperty ( kKeySwitchProperty ) );
	
	device = OSDynamicCast ( IOUSBMassStorageUFISubclass, ( OSObject * ) target );
	
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
#pragma mark Client Requests Support

void 
IOUSBMassStorageUFISubclass::AsyncReadWriteComplete( SCSITaskIdentifier request )
{
	void *							clientData;
	IOReturn						status;
	UInt64							actCount = 0;
	IOUSBMassStorageUFISubclass *	taskOwner;
	SCSITask *						scsiRequest;
		
	scsiRequest = OSDynamicCast( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		PANIC_NOW(( "IOUSBMassStorageUFISubclass::AsyncReadWriteComplete scsiRequest==NULL." ));
	}

	// Extract the client data from the SCSITask	
	clientData	= scsiRequest->GetApplicationLayerReference();
	
	if (( scsiRequest->GetServiceResponse() == kSCSIServiceResponse_TASK_COMPLETE ) &&
		( scsiRequest->GetTaskStatus() == kSCSITaskStatus_GOOD )) 
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
	
	taskOwner = OSDynamicCast( IOUSBMassStorageUFISubclass, scsiRequest->GetTaskOwner());
	if ( taskOwner == NULL )
	{
		PANIC_NOW(( "IOUSBMassStorageUFISubclass::AsyncReadWriteComplete taskOwner==NULL." ));
	}

	taskOwner->ReleaseSCSITask( request );

	IOUFIStorageServices::AsyncReadWriteComplete( clientData, status, actCount );
}

// Perform the Synchronous Read Request
IOReturn 
IOUSBMassStorageUFISubclass::IssueRead( 	IOMemoryDescriptor *	buffer,
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
		PANIC_NOW(( "IOUSBMassStorageUFISubclass::IssueRead malformed command" ));
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
IOUSBMassStorageUFISubclass::IssueRead( 	IOMemoryDescriptor *	buffer,
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
		STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::IssueRead send command.\n" ) );
    	SendCommand( request, 0, &this->AsyncReadWriteComplete );
	}
	else
	{
		PANIC_NOW(( "IOUSBMassStorageUFISubclass::IssueWrite malformed command" ));
		status = kIOReturnError;
	}

	return status;
}

// Perform the Synchronous Write Request
IOReturn 
IOUSBMassStorageUFISubclass::IssueWrite( 	IOMemoryDescriptor *	buffer,
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
		PANIC_NOW(( "IOUSBMassStorageUFISubclass::IssueWrite malformed command" ));
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
IOUSBMassStorageUFISubclass::IssueWrite(	IOMemoryDescriptor *	buffer,
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
		STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::IssueWrite send command.\n" ) );
    	SendCommand( request, 0, &this->AsyncReadWriteComplete );
	}
	else
	{
		PANIC_NOW(( "IOUSBMassStorageUFISubclass::IssueWrite malformed command" ));
	}

	return status;
}


IOReturn 
IOUSBMassStorageUFISubclass::SyncReadWrite ( 	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount,
                         					UInt64					blockSize )
{
	IODirection		direction;
	IOReturn		theErr;

	if ( GetInterfaceReference() == NULL )
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
IOUSBMassStorageUFISubclass::AsyncReadWrite (	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount,
                         					UInt64					blockSize,
											void *					clientData )
{
	IODirection		direction;
	IOReturn		theErr;
	
	if ( GetInterfaceReference() == NULL )
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
IOUSBMassStorageUFISubclass::EjectTheMedium( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::EjectTheMedium called\n" ) );
	
	if ( GetInterfaceReference() == NULL )
	{
		return kIOReturnOffline;
	}

	ResetMediumCharacteristics();
	
	// Set the polling to determine when media has been removed
	fPollingMode = kPollingMode_MediaRemoval;
    	
	EnablePolling();
		
	return kIOReturnSuccess;
}


IOReturn 
IOUSBMassStorageUFISubclass::FormatMedium( UInt64 blockCount, UInt64 blockSize )
{
	IOReturn	theErr = kIOReturnSuccess;

    STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::FormatMedium called\n" ) );

	if ( GetInterfaceReference() == NULL )
	{
		return kIOReturnOffline;
	}

	return theErr;	
}


UInt32 
IOUSBMassStorageUFISubclass::GetFormatCapacities(	UInt64 * capacities,
												UInt32   capacitiesMaxCount ) const
{
    STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::doGetFormatCapacities called\n" ) );

	return 0;
}

#pragma mark -
#pragma mark Device Information Retrieval Methods


char *
IOUSBMassStorageUFISubclass::GetVendorString ( void )
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


char *
IOUSBMassStorageUFISubclass::GetProductString ( void )
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


char *
IOUSBMassStorageUFISubclass::GetRevisionString ( void )
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


OSDictionary *
IOUSBMassStorageUFISubclass::GetProtocolCharacteristicsDictionary ( void )
{
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	return ( OSDictionary * ) getProperty( kIOPropertyProtocolCharacteristicsKey );
}


OSDictionary *
IOUSBMassStorageUFISubclass::GetDeviceCharacteristicsDictionary ( void )
{
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );	
	return fDeviceCharacteristicsDictionary;
}


#pragma mark - 
#pragma mark Query methods to report device characteristics 

UInt64 
IOUSBMassStorageUFISubclass::ReportDeviceMaxBlocksReadTransfer( void )
{
	UInt64	maxBlockCount;

    STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::reportMaxReadTransfer\n" ) );

	maxBlockCount = 256;

	return maxBlockCount;
}

UInt64 
IOUSBMassStorageUFISubclass::ReportDeviceMaxBlocksWriteTransfer( void )
{
	UInt64	maxBlockCount;
	
    STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::reportMaxWriteTransfer.\n" ) );

	maxBlockCount = 256;

	return maxBlockCount;
}

#pragma mark - 
#pragma mark Query methods to report installed medium characteristics 

UInt64 
IOUSBMassStorageUFISubclass::ReportMediumBlockSize( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::ReportMediumBlockSize blockSize = %ld\n", ( UInt32 ) fMediumBlockSize ) );
	return fMediumBlockSize;
}


UInt64 
IOUSBMassStorageUFISubclass::ReportMediumTotalBlockCount( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::ReportMediumTotalBlockCount maxBlock = %ld\n", fMediumBlockCount ) );
	return fMediumBlockCount;
}

bool 
IOUSBMassStorageUFISubclass::ReportMediumWriteProtection( void )
{
    STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::ReportMediumWriteProtection isWriteProtected = %d.\n", fMediumIsWriteProtected ) );	
	return fMediumIsWriteProtected;
}

#pragma mark -
#pragma mark SCSI Task Get and Release

SCSITaskIdentifier 
IOUSBMassStorageUFISubclass::GetSCSITask( void )
{
	SCSITask	* newTask = new SCSITask;
	
	newTask->SetTaskOwner( this );

	// Make sure the object is not removed if there is a pending
	// command.
	retain();
	
	return ( SCSITaskIdentifier ) newTask;
}

void 
IOUSBMassStorageUFISubclass::ReleaseSCSITask ( SCSITaskIdentifier request )
{
	if( request != NULL )
	{
		request->release();
		
		// Since the command has been released, let go of the retain on this
		// object.
		release();
	}
}


#pragma mark -
#pragma mark Task Execution Support Methods

void
IOUSBMassStorageUFISubclass::TaskCallback( SCSITaskIdentifier completedTask )
{
	SCSIServiceResponse 	serviceResponse;
	SCSITask *				scsiRequest;
	IOSyncer *				fSyncLock;
	
	STATUS_LOG ( ( "IOUSBMassStorageUFISubclass::TaskCallback called\n.") );
		
	scsiRequest = OSDynamicCast ( SCSITask, completedTask );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOUSBMassStorageUFISubclass::TaskCallback scsiRequest==NULL." ) );
		ERROR_LOG ( ( "IOUSBMassStorageUFISubclass::TaskCallback scsiRequest==NULL." ) );
		return;
		
	}
	
	fSyncLock = ( IOSyncer * ) scsiRequest->GetApplicationLayerReference ( );
	serviceResponse = scsiRequest->GetServiceResponse ( );
	fSyncLock->signal ( serviceResponse, false );
}


SCSIServiceResponse 
IOUSBMassStorageUFISubclass::SendCommand ( SCSITaskIdentifier request, UInt32 timeoutDuration )
{
	SCSIServiceResponse 	serviceResponse;
	IOSyncer *				fSyncLock;
	
	if ( GetInterfaceReference() == NULL )
	{
		SetServiceResponse ( request, kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE );
		
		// Save that status into the Task object.
		SetTaskStatus ( request, kSCSITaskStatus_No_Status );
		
		return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	}

	fSyncLock = IOSyncer::create ( false );
	if ( fSyncLock == NULL )
	{
		PANIC_NOW ( ( "IOUSBMassStorageUFISubclass::SendCommand Allocate fSyncLock failed." ) );
		ERROR_LOG ( ( "IOUSBMassStorageUFISubclass::SendCommand Allocate fSyncLock failed." ) );
		
		SetServiceResponse ( request, kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE );
		
		// Save that status into the Task object.
		SetTaskStatus ( request, kSCSITaskStatus_No_Status );
		
		return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	}
	
	fSyncLock->signal ( kIOReturnSuccess, false );
	
	SetTimeoutDuration ( request, timeoutDuration );
	SetTaskCompletionCallback ( request, &this->TaskCallback );
	SetApplicationLayerReference ( request, ( void * ) fSyncLock );
	
	SetAutosenseCommand ( request, 0x03, 0x00, 0x00, 0x00, sizeof ( SCSI_Sense_Data ), 0x00 );
	
	STATUS_LOG ( ( "%s:SendCommand Reinit the syncer.\n", getName ( ) ) );
	fSyncLock->reinit ( );
	
	STATUS_LOG ( ( "%s:SendCommand Execute the command.\n", getName ( ) ) );
	ExecuteCommand( request );
	
	// Wait for the completion routine to get called
	serviceResponse = ( SCSIServiceResponse) fSyncLock->wait ( false );
	fSyncLock->release ( );
	
	STATUS_LOG ( ( "%s:SendCommand return the service response.\n", getName ( ) ) );
	return serviceResponse;
}


void 
IOUSBMassStorageUFISubclass::SendCommand ( 
						SCSITaskIdentifier	request,
						UInt32 				timeoutDuration,
						SCSITaskCompletion 	taskCompletion )
{
	STATUS_LOG ( ( "%s:  called.\n", getName ( ) ) );
	
	if ( GetInterfaceReference() == NULL )
	{
		
		SetServiceResponse ( request, kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE );
		
		// Save that status into the Task object.
		SetTaskStatus ( request, kSCSITaskStatus_No_Status );
		
		// The task has completed, execute the callback.
		TaskCompletedNotification ( request );
		
		return;
		
	}
	
	SetTimeoutDuration ( request, timeoutDuration );
	
	SetTaskCompletionCallback ( request, taskCompletion );
	
	SetAutosenseCommand ( request, 0x03, 0x00, 0x00, 0x00, sizeof ( SCSI_Sense_Data ), 0x00 );
	ExecuteCommand( request );
	
}


#pragma mark -
#pragma mark SCSI Task Field Accessors


// ---- Utility methods for accessing SCSITask attributes ----
bool
IOUSBMassStorageUFISubclass::SetTaskAttribute( SCSITaskIdentifier request, SCSITaskAttribute newAttribute )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast( SCSITask, request );
	return scsiRequest->SetTaskAttribute( newAttribute );
}


SCSITaskAttribute
IOUSBMassStorageUFISubclass::GetTaskAttribute( SCSITaskIdentifier request )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast( SCSITask, request );
	return scsiRequest->GetTaskAttribute();
}


bool
IOUSBMassStorageUFISubclass::SetTaskState( SCSITaskIdentifier request,
											SCSITaskState newTaskState )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast( SCSITask, request );
	return scsiRequest->SetTaskState( newTaskState );
}


SCSITaskState
IOUSBMassStorageUFISubclass::GetTaskState ( SCSITaskIdentifier request )
{
	SCSITask *	scsiRequest;

	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetTaskState( );
}


bool
IOUSBMassStorageUFISubclass::SetTaskStatus ( SCSITaskIdentifier request,
											 SCSITaskStatus newStatus )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetTaskStatus ( newStatus );
}


SCSITaskStatus
IOUSBMassStorageUFISubclass::GetTaskStatus ( SCSITaskIdentifier request )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetTaskStatus ( );
}


// Get the control information for the transfer, including
// the transfer direction and the number of bytes to transfer.
bool
IOUSBMassStorageUFISubclass::SetDataTransferDirection ( SCSITaskIdentifier request,
														UInt8 newDirection )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetDataTransferDirection ( newDirection );
}


// Get the control information for the transfer, including
// the transfer direction and the number of bytes to transfer.
UInt8
IOUSBMassStorageUFISubclass::GetDataTransferDirection ( SCSITaskIdentifier request )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetDataTransferDirection ( );
}


bool
IOUSBMassStorageUFISubclass::SetRequestedDataTransferCount ( SCSITaskIdentifier request,
															 UInt64 newRequestedCount )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast( SCSITask, request );
	return scsiRequest->SetRequestedDataTransferCount( newRequestedCount );
}


UInt64
IOUSBMassStorageUFISubclass::GetRequestedDataTransferCount ( SCSITaskIdentifier request )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetRequestedDataTransferCount ( );
}


bool
IOUSBMassStorageUFISubclass::SetRealizedDataTransferCount ( SCSITaskIdentifier request,
															UInt64 newRealizedDataCount )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetRealizedDataTransferCount ( newRealizedDataCount );
}


UInt64
IOUSBMassStorageUFISubclass::GetRealizedDataTransferCount ( SCSITaskIdentifier request )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetRealizedDataTransferCount ( );
}


bool
IOUSBMassStorageUFISubclass::SetDataBuffer ( SCSITaskIdentifier request,
											 IOMemoryDescriptor * newBuffer )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetDataBuffer ( newBuffer );
}


IOMemoryDescriptor *
IOUSBMassStorageUFISubclass::GetDataBuffer ( SCSITaskIdentifier request )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetDataBuffer ( );
}


bool
IOUSBMassStorageUFISubclass::SetTimeoutDuration ( SCSITaskIdentifier request,
												  UInt32 newTimeout )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetTimeoutDuration ( newTimeout );
}


UInt32
IOUSBMassStorageUFISubclass::GetTimeoutDuration ( SCSITaskIdentifier request )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetTimeoutDuration ( );
}


bool
IOUSBMassStorageUFISubclass::SetTaskCompletionCallback ( 
										SCSITaskIdentifier 		request,
										SCSITaskCompletion 		newCallback )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetTaskCompletionCallback ( newCallback );
}


void
IOUSBMassStorageUFISubclass::TaskCompletedNotification (  
										SCSITaskIdentifier 		request )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast( SCSITask, request );
	return scsiRequest->TaskCompletedNotification( );
}


bool
IOUSBMassStorageUFISubclass::SetServiceResponse( 
										SCSITaskIdentifier 		request,
										SCSIServiceResponse 	serviceResponse )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetServiceResponse( serviceResponse );
}


SCSIServiceResponse
IOUSBMassStorageUFISubclass::GetServiceResponse ( 
										SCSITaskIdentifier 		request )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast( SCSITask, request );
	return scsiRequest->GetServiceResponse();
}


bool
IOUSBMassStorageUFISubclass::SetAutosenseCommand (
										SCSITaskIdentifier 		request,
										UInt8					cdbByte0,
										UInt8					cdbByte1,
										UInt8					cdbByte2,
										UInt8					cdbByte3,
										UInt8					cdbByte4,
										UInt8					cdbByte5 )
{
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetAutosenseCommand ( cdbByte0, cdbByte1, cdbByte2,
											  cdbByte3, cdbByte4, cdbByte5 );
}


// Set the auto sense data that was returned for the SCSI Task.
// A return value if true indicates that the data copied to the member 
// sense data structure, false indicates that the data could not be saved.
bool
IOUSBMassStorageUFISubclass::GetAutoSenseData( SCSITaskIdentifier request,
												SCSI_Sense_Data * senseData )
{
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast( SCSITask, request );
	return scsiRequest->GetAutoSenseData( senseData, sizeof ( SCSI_Sense_Data ) );
}


bool
IOUSBMassStorageUFISubclass::SetApplicationLayerReference( SCSITaskIdentifier request,
															void * newReferenceValue )
{
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast( SCSITask, request );
	return scsiRequest->SetApplicationLayerReference ( newReferenceValue );
}


void *
IOUSBMassStorageUFISubclass::GetApplicationLayerReference ( SCSITaskIdentifier request )
{
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast( SCSITask, request );
	return scsiRequest->GetApplicationLayerReference();
}



#pragma mark -
#pragma mark Command Builders Utility Methods

// Utility routines used by all SCSI Command Set objects

//----------------------------------------------------------------------
//
//		IOUSBMassStorageUFISubclass::IsParameterValid
//
//----------------------------------------------------------------------
//
//		Validate Parameter used for 1 bit to 1 byte paramaters
//
//----------------------------------------------------------------------

bool
IOUSBMassStorageUFISubclass::IsParameterValid( SCSICmdField1Byte param,
										SCSICmdField1Byte mask )
{
	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::IsParameterValid called\n" ) );
	
	if( ( param | mask ) != mask )
	{
		STATUS_LOG( ( "param = %x not valid, mask = %x\n", param, mask ) );
		return false;
	}
	
	return true;
}


//----------------------------------------------------------------------
//
//		IOUSBMassStorageUFISubclass::IsParameterValid
//
//----------------------------------------------------------------------
//
//		Validate Parameter used for 9 bit to 2 byte paramaters
//
//----------------------------------------------------------------------

bool
IOUSBMassStorageUFISubclass::IsParameterValid( SCSICmdField2Byte param,
										SCSICmdField2Byte mask )
{
	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::IsParameterValid called\n" ) );
	
	if( ( param | mask ) != mask )
	{
		STATUS_LOG ( ( "param = %x not valid, mask = %x\n", param, mask ) );
		return false;
	}
	
	return true;
}


//----------------------------------------------------------------------
//
//		IOUSBMassStorageUFISubclass::IsParameterValid
//
//----------------------------------------------------------------------
//
//		Validate Parameter used for 17 bit to 4 byte paramaters
//
//----------------------------------------------------------------------

bool
IOUSBMassStorageUFISubclass::IsParameterValid( SCSICmdField4Byte param,
										SCSICmdField4Byte mask )
{
	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::IsParameterValid called\n" ) );
	
	if( ( param | mask ) != mask )
	{
		STATUS_LOG( ( "param = %x not valid, mask = %x\n", param, mask ) );
		return false;
	}
	
	return true;
}


//----------------------------------------------------------------------
//
//		IOUSBMassStorageUFISubclass::IsBufferAndCapacityValid
//
//----------------------------------------------------------------------
//
//		Check that the buffer is valid and of the required size
//
//----------------------------------------------------------------------

bool
IOUSBMassStorageUFISubclass::IsBufferAndCapacityValid(
				IOMemoryDescriptor *		dataBuffer,
				UInt32						requiredSize )
{
	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::IsBufferAndCapacityValid called\n" ) );
	
	if( dataBuffer == NULL )
	{
		STATUS_LOG ( ( "dataBuffer = %x not valid\n", dataBuffer ) );
		return false;
	}
	
	if( dataBuffer->getLength() < requiredSize )
	{
		STATUS_LOG ( ( "dataBuffer length = %x not valid, requiredSize = %x\n",
						dataBuffer->getLength(), requiredSize ) );
		
		return false;
	}
	
	return true;
}


// ---- Methods for accessing the SCSITask attributes ----
// The setCommandDescriptorBlock methods will populate the CDB of the
// appropriate size.  These methods will returns true if the CDB could 
// be filled out, false if it couldn't.

//----------------------------------------------------------------------
//
//		IOUSBMassStorageUFISubclass::SetCommandDescriptorBlock
//
//----------------------------------------------------------------------
//
//		Populate the 12 Byte Command Descriptor Block
//
//----------------------------------------------------------------------

bool
IOUSBMassStorageUFISubclass::SetCommandDescriptorBlock( 
							SCSITask *		request,
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5,
							UInt8			cdbByte6,
							UInt8			cdbByte7,
							UInt8			cdbByte8,
							UInt8			cdbByte9,
							UInt8			cdbByte10,
							UInt8			cdbByte11)
{
	return request->SetCommandDescriptorBlock(
					cdbByte0,
					cdbByte1,
					cdbByte2,
					cdbByte3,
					cdbByte4,
					cdbByte5,
					cdbByte6,
					cdbByte7,
					cdbByte8,
					cdbByte9,
					cdbByte10,
					cdbByte11 );
}

//----------------------------------------------------------------------
//
//		IOUSBMassStorageUFISubclass::SetDataTransferControl
//
//----------------------------------------------------------------------
//
//		Set up the control information for the transfer, including
//		the transfer direction and the number of bytes to transfer.
//
//----------------------------------------------------------------------

bool
IOUSBMassStorageUFISubclass::SetDataTransferControl( 
							SCSITask *				request,
							UInt8					dataTransferDirection,
							IOMemoryDescriptor *	dataBuffer,
							UInt64					transferCountInBytes )
{
	bool	result = false;
		
	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::SetDataTransferControl called\n" ) );
	
	// Needs to do more extensive checking based on buffer count and control values
	if( ( transferCountInBytes != 0 ) && ( dataBuffer == NULL ) )
	{
		STATUS_LOG( ( "transferCountInBytes = %x not valid, dataBuffer = %x\n",
						transferCountInBytes, dataBuffer ) );
		return result;
	}
	
	result = request->SetDataTransferDirection( dataTransferDirection );
	if( result == false )
	{
		STATUS_LOG( ( "SetDataTransferDirection failed, dataTransferDirection = %x\n",
						dataTransferDirection ) );
		return result;
	}
	
	result = request->SetDataBuffer( dataBuffer );
	if( result == false )
	{
		STATUS_LOG( ( "SetDataBuffer failed, dataBuffer = %x\n",
						dataBuffer ) );
		return result;
	}
	
	result = request->SetRequestedDataTransferCount( transferCountInBytes );
	if( result == false )
	{
		STATUS_LOG( ( "SetRequestedDataTransferCount failed, transferCountInBytes = %x\n",
						transferCountInBytes ) );
		return result;
	}
	
	return true;
}


#pragma mark -
#pragma mark Command Builder Methods

bool 
IOUSBMassStorageUFISubclass::FORMAT_UNIT(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer,
			    			IOByteCount					defectListSize,
			    			SCSICmdField1Byte 			TRACK_NUMBER, 
			    			SCSICmdField2Byte 			INTERLEAVE )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::FORMAT_UNIT called\n" ) );
	
	if ( scsiRequest->ResetForNewTask() == false )
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
		if( IsBufferAndCapacityValid( dataBuffer, defectListSize  )
				== false )
		{
			STATUS_LOG( ( "dataBuffer = %x not valid, defectListSize = %x\n",
							dataBuffer, defectListSize ) );
			return false;
		}
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	scsiRequest,
								kSCSICmd_FORMAT_UNIT,
								0x00,
								0x00,
								( INTERLEAVE >> 8 ) & 0xFF,
								  INTERLEAVE		& 0xFF,
								0x00 );
 	
	// The client has requested a DEFECT LIST be sent to the device
	// to be used with the format command
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer );
	
	return true;
}

bool 
IOUSBMassStorageUFISubclass::INQUIRY(
							SCSITaskIdentifier			request,
    						IOMemoryDescriptor 			*dataBuffer,
    						SCSICmdField1Byte 			PAGE_OR_OPERATION_CODE,
    						SCSICmdField1Byte 			ALLOCATION_LENGTH )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::INQUIRY called\n" ) );

	if ( scsiRequest->ResetForNewTask() == false )
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
	
	if ( IsBufferAndCapacityValid( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		STATUS_LOG ( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
	}
		
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	scsiRequest,
								kSCSICmd_INQUIRY,
								0x00,
								PAGE_OR_OPERATION_CODE,
								0x00,
								ALLOCATION_LENGTH,
								0x00 );
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
}


bool 
IOUSBMassStorageUFISubclass::MODE_SELECT_10(
							SCSITaskIdentifier			request,
    						IOMemoryDescriptor 			*dataBuffer,
    						SCSICmdField1Bit 			PF,
    						SCSICmdField1Bit 			SP,
    						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::MODE_SELECT_10 called\n" ) );
	
	if ( scsiRequest->ResetForNewTask() == false )
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

	if( IsBufferAndCapacityValid( dataBuffer, PARAMETER_LIST_LENGTH ) == false )
	{
		STATUS_LOG( ( "dataBuffer = %x not valid, PARAMETER_LIST_LENGTH = %x\n",
						dataBuffer, PARAMETER_LIST_LENGTH ) );
		return false;
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	scsiRequest,
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
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								PARAMETER_LIST_LENGTH );
	
	return true;
}
  

bool 
IOUSBMassStorageUFISubclass::MODE_SENSE_10(
							SCSITaskIdentifier			request,
    						IOMemoryDescriptor 			*dataBuffer,
    						SCSICmdField1Bit 			DBD,
	   						SCSICmdField2Bit 			PC,
	   						SCSICmdField6Bit 			PAGE_CODE,
	   						SCSICmdField2Byte 			ALLOCATION_LENGTH )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::MODE_SENSE_10 called\n" ) );
	
	if ( scsiRequest->ResetForNewTask() == false )
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

	if( IsBufferAndCapacityValid( dataBuffer, ALLOCATION_LENGTH ) == false )
	{
		STATUS_LOG( ( "dataBuffer = %x not valid, ALLOCATION_LENGTH = %x\n",
						dataBuffer, ALLOCATION_LENGTH ) );
		return false;
	}
	
	// This is a 10-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	scsiRequest,
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
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
}


bool 
IOUSBMassStorageUFISubclass::PREVENT_ALLOW_MEDIUM_REMOVAL( 
							SCSITaskIdentifier			request,
	     					SCSICmdField1Bit 			PREVENT )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::PREVENT_ALLOW_MEDIUM_REMOVAL called\n" ) );

	if ( scsiRequest->ResetForNewTask() == false )
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
	SetCommandDescriptorBlock(	scsiRequest,
								kSCSICmd_PREVENT_ALLOW_MEDIUM_REMOVAL,
								0x00,
								0x00,
								0x00,
								PREVENT,
								0x00 );
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
}


bool 
IOUSBMassStorageUFISubclass::READ_10(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			UInt32						blockSize,
			    			SCSICmdField1Bit 			DPO, 
			    			SCSICmdField1Bit 			FUA,
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH )
{
	SCSITask *		scsiRequest;
	UInt32			requestedByteCount;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::READ_10 called\n" ) );
	
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
	SetCommandDescriptorBlock(	scsiRequest,
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
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								requestedByteCount );	
	
	return true;
}


bool 
IOUSBMassStorageUFISubclass::READ_12(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			UInt32						blockSize,
			    			SCSICmdField1Bit 			DPO, 
			    			SCSICmdField1Bit 			FUA,
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField4Byte 			TRANSFER_LENGTH )
{
	SCSITask *		scsiRequest;
	UInt32			requestedByteCount;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::READ_12 called\n" ) );
	
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
	SetCommandDescriptorBlock(	scsiRequest,
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
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								requestedByteCount );	

	return true;
}


bool 
IOUSBMassStorageUFISubclass::READ_CAPACITY(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			SCSICmdField1Bit 			RELADR,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField1Bit 			PMI )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::READ_CAPACITY called\n" ) );
	
	if ( scsiRequest->ResetForNewTask() == false )
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
	SetCommandDescriptorBlock(	scsiRequest,
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
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								8 );	
	return true;
}


bool 
IOUSBMassStorageUFISubclass::READ_FORMAT_CAPACITIES(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			SCSICmdField2Byte 			ALLOCATION_LENGTH )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::READ_CAPACITY called\n" ) );
	
	if ( scsiRequest->ResetForNewTask() == false )
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
	SetCommandDescriptorBlock(	scsiRequest,
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
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );	
	return true;
}


bool 
IOUSBMassStorageUFISubclass::REQUEST_SENSE(
							SCSITaskIdentifier			request,
   							IOMemoryDescriptor 			*dataBuffer,
			    			SCSICmdField1Byte 			ALLOCATION_LENGTH )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::REQUEST_SENSE called\n" ) );
	
	if ( scsiRequest->ResetForNewTask() == false )
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
	SetCommandDescriptorBlock(	scsiRequest,
								kSCSICmd_REQUEST_SENSE,
								0x00,
								0x00,
								0x00,
								ALLOCATION_LENGTH,
								0x00 );
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_FromTargetToInitiator,
								dataBuffer,
								ALLOCATION_LENGTH );
	
	return true;
}

 	
bool 
IOUSBMassStorageUFISubclass::REZERO_UNIT( 
							SCSITaskIdentifier			request )
{
	return false;
}


bool 
IOUSBMassStorageUFISubclass::SEEK( 
							SCSITaskIdentifier			request,
			    			SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS )
{
	return false;
}


bool 
IOUSBMassStorageUFISubclass::SEND_DIAGNOSTICS( 
							SCSITaskIdentifier			request,
							SCSICmdField1Bit 			PF, 
							SCSICmdField1Bit 			SELF_TEST, 
							SCSICmdField1Bit 			DEF_OFL, 
							SCSICmdField1Bit 			UNIT_OFL )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::SEND_DIAGNOSTICS called\n" ) );
	
	if ( scsiRequest->ResetForNewTask() == false )
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
	SetCommandDescriptorBlock(	scsiRequest,
								kSCSICmd_SEND_DIAGNOSTICS,
								( PF << 4 ) |
									( SELF_TEST << 2 ) | ( DEF_OFL << 1 ) | UNIT_OFL,
								0x00,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_NoDataTransfer);
	
	return true;
}


bool 
IOUSBMassStorageUFISubclass::START_STOP_UNIT( 
							SCSITaskIdentifier			request,
							SCSICmdField1Bit 			IMMED, 
							SCSICmdField1Bit 			LOEJ, 
							SCSICmdField1Bit 			START )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::START_STOP_UNIT called\n" ) );

	if ( scsiRequest->ResetForNewTask() == false )
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
	SetCommandDescriptorBlock(	scsiRequest,
								kSCSICmd_START_STOP_UNIT,
								IMMED,
								0x00,
								0x00,
								( LOEJ << 1 ) | START,
								0x00 );
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
}


bool 
IOUSBMassStorageUFISubclass::TEST_UNIT_READY(  
							SCSITaskIdentifier			request )
{
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::TEST_UNIT_READY called\n" ) );
	
	if ( scsiRequest->ResetForNewTask() == false )
	{
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
	}
	
	// This is a 6-Byte command, fill out the cdb appropriately  
	SetCommandDescriptorBlock(	scsiRequest,
								kSCSICmd_TEST_UNIT_READY,
								0x00,
								0x00,
								0x00,
								0x00,
								0x00 );
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_NoDataTransfer );
	
	return true;
}

 
bool 
IOUSBMassStorageUFISubclass::VERIFY( 
							SCSITaskIdentifier			request,
							SCSICmdField1Bit 			DPO, 
							SCSICmdField1Bit 			BYTCHK, 
							SCSICmdField1Bit 			RELADR,
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			VERIFICATION_LENGTH )
{
	return false;
}


bool 
IOUSBMassStorageUFISubclass::WRITE_10(
							SCSITaskIdentifier			request,
			    			IOMemoryDescriptor *		dataBuffer, 
			    			UInt32						blockSize,
			    			SCSICmdField1Bit 			DPO, 
			    			SCSICmdField1Bit 			FUA,
							SCSICmdField1Bit 			RELADR, 
							SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS, 
							SCSICmdField2Byte 			TRANSFER_LENGTH )
{
	SCSITask *		scsiRequest;
	UInt32			requestedByteCount;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	STATUS_LOG( ( "IOUSBMassStorageUFISubclass::WRITE_10 called\n" ) );

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
	SetCommandDescriptorBlock(	scsiRequest,
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
	
	SetDataTransferControl( 	scsiRequest,
								kSCSIDataTransfer_FromInitiatorToTarget,
								dataBuffer,
								requestedByteCount );
	
	return true;
}


bool 
IOUSBMassStorageUFISubclass::WRITE_12(
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


bool 
IOUSBMassStorageUFISubclass::WRITE_AND_VERIFY(
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

