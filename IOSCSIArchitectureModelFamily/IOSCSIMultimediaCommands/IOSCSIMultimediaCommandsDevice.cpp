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
#include <IOKit/scsi-commands/SCSICommandDefinitions.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include "IOSCSIMultimediaCommandsDevice.h"
#include "IODVDServices.h"
#include "IOCompactDiscServices.h"

#include <libkern/OSByteOrder.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/scsi-commands/SCSICommandDefinitions.h>
#include <IOKit/scsi-commands/SCSICmds_INQUIRY_Definitions.h>

// Flag to turn on compiling of APIs marked as obsolete
#define INCLUDE_OBSOLETE_APIS	1

#define SCSI_MMC_DEVICE_DEBUGGING_LEVEL 0

#if ( SCSI_MMC_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_MMC_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_MMC_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define	kMaxProfileSize						56
#define kDiscInformationSize				32
#define kATIPBufferSize						16
#define kTrackInfoBufferSize				8
#define kDVDPhysicalFormatInfoBufferSize	8
#define	kProfileDataLengthFieldSize			4
#define kProfileFeatureHeaderSize			8
#define kProfileDescriptorSize				4
#define kModeSenseParameterHeaderSize		8
#define kSubChannelDataBufferSize			24

#define kMaxRetryCount 8

// Get Configuration Feature Numbers
enum
{
	kGetConfigurationCDROM_Feature		= 0x0008,
	kGetConfigurationCDR_Feature		= 0x0009,
	kGetConfigurationCDRW_Feature		= 0x000A,
	kGetConfigurationDVDROM_Feature		= 0x0010,
	kGetConfigurationDVDR_Feature		= 0x0011,
	kGetConfigurationDVDRAM_Feature		= 0x0012,	// DVD-RAM and DVD+RW
	kGetConfigurationDVDRW_Feature		= 0x0014
};

// Mechanical Capabilities flags
enum
{
	kMechanicalCapabilitiesCDRMask		= 0x01,
	kMechanicalCapabilitiesCDRWMask		= 0x02,
	kMechanicalCapabilitiesDVDROMMask	= 0x08,
	kMechanicalCapabilitiesDVDRMask		= 0x10,
	kMechanicalCapabilitiesDVDRAMMask 	= 0x20,
};

enum
{
	kMechanicalCapabilitiesAnalogAudioMask			= 0x01,
	kMechanicalCapabilitiesCDDAStreamAccurateMask	= 0x02
};


// Random Writable Protection (DVD-RAM DVD+RW protection mask)
enum
{
	kRandomWritableProtectionMask		= 0x01
};

// DiscType mask
enum
{
	kDiscTypeCDRWMask					= 0x40
};

// Media Catalog Number and ISRC masks
enum
{
	kMediaCatalogValueFieldValidMask	= 0x80,
	kTrackCatalogValueFieldValidMask	= 0x80
};


#define super IOSCSIPrimaryCommandsDevice
OSDefineMetaClass ( IOSCSIMultimediaCommandsDevice, IOSCSIPrimaryCommandsDevice );
OSDefineAbstractStructors ( IOSCSIMultimediaCommandsDevice, IOSCSIPrimaryCommandsDevice );


bool
IOSCSIMultimediaCommandsDevice::InitializeDeviceSupport ( void )
{
	
	bool setupSuccessful = false;
	
	// Initialize the device characteristics flags
	fSupportedCDFeatures 			= 0;
	fSupportedDVDFeatures 			= 0;
	fDeviceSupportsLowPowerPolling 	= false;
	fMediaChanged					= false;
	fMediaPresent					= false;
	fMediaIsRemovable 				= false;
	fMediaType						= kCDMediaTypeUnknown;
	fMediaIsWriteProtected 			= true;
	fCurrentDiscSpeed				= 0;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::InitializeDeviceSupport called\n" ) );

	// Make sure the drive is ready for us!
	if ( ClearNotReadyStatus ( ) == false )
	{
		goto ERROR_EXIT;
	}
	
	// Check to see if the device supports power conditions.
	CheckPowerConditionsModePage ( );
		
	setupSuccessful = DetermineDeviceCharacteristics ( );
	
	if ( setupSuccessful == true )
	{		
		
		fPollingMode 	= kPollingMode_NewMedia;
		fPollingThread 	= thread_call_allocate (
						( thread_call_func_t ) IOSCSIMultimediaCommandsDevice::sPollForMedia,
						( thread_call_param_t ) this );
		
		if ( fPollingThread == NULL )
		{
			
			ERROR_LOG ( ( "fPollingThread allocation failed.\n" ) );
			setupSuccessful = false;
			goto ERROR_EXIT;
			
		}
		
		InitializePowerManagement ( GetProtocolDriver ( ) );
		
	}
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::InitializeDeviceSupport setupSuccessful = %d\n", setupSuccessful ) );
	
	
ERROR_EXIT:
	
		
	return setupSuccessful;
	
}


void 
IOSCSIMultimediaCommandsDevice::StartDeviceSupport ( void )
{
	
	if ( fMediaIsRemovable == false )
	{
		
		// We have a fixed disk, so make sure we determine its state
		// before we create the layer above us.
		PollForMedia ( );
		
	}
	
	else
	{
		
		// Removable media - start polling
		EnablePolling ( );
		
	}
	
	CreateStorageServiceNub ( );
	
}


void 
IOSCSIMultimediaCommandsDevice::SuspendDeviceSupport ( void )
{
	
	if ( fPollingMode != kPollingMode_Suspended )
	{
		
		DisablePolling ( );
		
	}
	
	ResetMediaCharacteristics ( );
	
}


void 
IOSCSIMultimediaCommandsDevice::ResumeDeviceSupport ( void )
{
	
	if ( fMediaPresent == false )
	{
		
		fPollingMode = kPollingMode_NewMedia;
		EnablePolling ( );
		
	}
	
}


void 
IOSCSIMultimediaCommandsDevice::StopDeviceSupport ( void )
{
	
	DisablePolling ( );
	
}


void 
IOSCSIMultimediaCommandsDevice::TerminateDeviceSupport ( void )
{
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::cleanUp called.\n" ) );
	
	if ( fPollingThread != NULL )
	{
		
		thread_call_free ( fPollingThread );
		fPollingThread = NULL;
		
	}
   
}


bool
IOSCSIMultimediaCommandsDevice::CreateCommandSetObjects ( void )
{

    STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::CreateCommandSetObjects called\n" ) );
	
	fSCSIMultimediaCommandObject = SCSIMultimediaCommands::CreateSCSIMultimediaCommandObject ( );
	if ( fSCSIMultimediaCommandObject == NULL )
	{
		
		ERROR_LOG ( ( "%s::%s exiting false, MMC object not created\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}
	
	fSCSIBlockCommandObject = SCSIBlockCommands::CreateSCSIBlockCommandObject( );
	if ( fSCSIBlockCommandObject == NULL )
	{
		
		ERROR_LOG ( ( "%s::%s exiting false, SBC object not created\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}
	
	return true;

}


void
IOSCSIMultimediaCommandsDevice::FreeCommandSetObjects ( void )
{

    STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::FreeCommandSetObjects called\n" ) );

	if ( fSCSIMultimediaCommandObject )
	{
	
		fSCSIMultimediaCommandObject->release ( );
		fSCSIMultimediaCommandObject = NULL;
	
	}
	
	if ( fSCSIBlockCommandObject )
	{
	
		fSCSIBlockCommandObject->release ( );
		fSCSIBlockCommandObject = NULL;
	
	}
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::VerifyDeviceState ( void )
{
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::VerifyDeviceState\n" ) );
	
	if ( fLowPowerPollingEnabled == true )
	{
		
		STATUS_LOG ( ( "Low power polling turned off\n" ) );
		fLowPowerPollingEnabled = false;
		
	}
	
	if ( IsPowerManagementIntialized ( ) == true )
	{
		
		STATUS_LOG ( ( "TicklePowerManager\n" ) );
		TicklePowerManager ( );
		
	}
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::VerifyDeviceState exiting\n" ) );
	
	return kIOReturnSuccess;
	
}


bool
IOSCSIMultimediaCommandsDevice::ClearNotReadyStatus ( void )
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


SCSIMultimediaCommands *
IOSCSIMultimediaCommandsDevice::GetSCSIMultimediaCommandObject ( void )
{
	return fSCSIMultimediaCommandObject;
}


SCSIBlockCommands *
IOSCSIMultimediaCommandsDevice::GetSCSIBlockCommandObject ( void )
{
	return fSCSIBlockCommandObject;
}


SCSIPrimaryCommands	*
IOSCSIMultimediaCommandsDevice::GetSCSIPrimaryCommandObject ( void )
{
	return OSDynamicCast ( SCSIPrimaryCommands, GetSCSIMultimediaCommandObject ( ) );
}


#pragma mark - 
#pragma mark Protected Methods


void
IOSCSIMultimediaCommandsDevice::EnablePolling ( void )
{		
	
	AbsoluteTime	time;
	
	// No reason to start a thread if we've been termintated
	
	if ( ( fPollingMode != kPollingMode_Suspended ) && fPollingThread )
	{
		
		// Retain ourselves so that this object doesn't go away
		// while we are polling
		
		retain ( );
		
		clock_interval_to_deadline ( 1000, kMillisecondScale, &time );
		thread_call_enter_delayed ( fPollingThread, time );
		
	}
	
}


void
IOSCSIMultimediaCommandsDevice::DisablePolling ( void )
{		
	
	fPollingMode = kPollingMode_Suspended;
	
	// Cancel the thread if it is running
	if ( thread_call_cancel ( fPollingThread ) )
	{
		
		// It was running, so we balance out the retain ( )
		// with a release ( )
		release ( );
		
	}
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::HandleSetUserClientExclusivityState ( IOService * userClient,
																	  bool state )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::HandleSetUserClientExclusivityState\n" ) );
	
	status = super::HandleSetUserClientExclusivityState ( userClient, state );
	if ( status == kIOReturnSuccess )
	{
		
		status = kIOReturnExclusiveAccess;
		
		if ( state == false )
		{
			status = message ( kSCSIServicesNotification_Resume, NULL, NULL );
		}
		
		else
		{
		
			if ( fMediaPresent )
			{
				
				OSIterator *				childList;
				IOService *					childService;
				OSObject *					childObject;
				IOService *					parent;
				
				STATUS_LOG ( ( "Media is present\n" ) );
	
				childList = getChildIterator ( gIOServicePlane );
				if ( childList != NULL )
				{
	
					STATUS_LOG ( ( "childList != NULL\n" ) );
					
					while ( ( childObject = childList->getNextObject ( ) ) != NULL )
					{
						
						childService = OSDynamicCast ( IOService, childObject );
						if ( childService == NULL )
							continue;
	
						STATUS_LOG ( ( "childService = %s\n", childService->getName ( ) ) );
						
						childService = OSDynamicCast ( IOBlockStorageDevice, childService );
						if ( childService != NULL )
						{
							
							// Keep a pointer to the parent of the block storage driver for
							// the call to messageClient().
							parent = childService;
							parent->retain ( );
							
							childList->release ( );
							childList = childService->getChildIterator ( gIOServicePlane );
							
							while ( ( childObject = childList->getNextObject ( ) ) != NULL )
							{
								
								childService = OSDynamicCast ( IOService, childObject );
								if ( childService == NULL )
									continue;
								
								STATUS_LOG ( ( "childService = %s\n", childService->getName ( ) ) );
								
								childService = OSDynamicCast ( IOBlockStorageDriver, childService );
								if ( childService == NULL )
									continue;
								
								// Ask the child nicely if it can close.  This allows it to say no
								// (if it's busy, has media mounted, etc.) without being destructive
								// to the state of the device.
								status = parent->messageClient ( kIOMessageServiceIsRequestingClose, ( IOBlockStorageDriver * ) childService );
								if ( status == kIOReturnSuccess )
								{
									message ( kSCSIServicesNotification_Suspend, NULL, NULL );
								}
								
								else
								{
									ERROR_LOG ( ( "BlockStorageDriver wouldn't close, status = %d\n", status ) );
									super::HandleSetUserClientExclusivityState ( userClient, !state );
								}
								
								break;
								
							}
							
							// Make sure to drop the retain() from above
							parent->release ( );
							
						}
						
					}
					
					if ( childList != NULL )
						childList->release ( );
					
				}
				
			}
			
			else
			{
				
				// No media is present, so clear the status
				message ( kSCSIServicesNotification_Suspend, NULL, NULL );
				status = kIOReturnSuccess;
				
			}
			
		}
		
	}
	
	ERROR_LOG ( ( "IOSCSIMultimediaCommandsDevice::HandleSetUserClientExclusivityState status = %d\n", status ) );
	
	return status;
	
}


void
IOSCSIMultimediaCommandsDevice::CreateStorageServiceNub ( void )
{
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::CreateStorageServiceNub entering.\n" ) );
	
	IOService * 	nub;
	
	if ( fSupportedDVDFeatures & kDVDFeaturesReadStructuresMask )
	{
		
		// We support DVD structure reads, so create the DVD nub
		nub = new IODVDServices;
	
	}
	
	else
	{
		
		// Create a CD nub instead
		nub = new IOCompactDiscServices;
	
	}

	if ( nub == NULL )
	{
		
		ERROR_LOG ( ( "IOSCSIMultimediaCommandsDevice::CreateStorageServiceNub failed\n" ) );
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::CreateStorageServiceNub failed\n" ) );
		
	}
	
	nub->init ( );
	
	if ( !nub->attach ( this ) )
	{
		
		// panic since the nub can't attach
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::CreateStorageServiceNub unable to attach nub" ) );
		
	}
	
	nub->start ( this );
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::CreateStorageServiceNub exiting.\n" ) );
	
	nub->release ( );
	
}


bool
IOSCSIMultimediaCommandsDevice::DetermineDeviceCharacteristics ( void )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	STATUS_LOG ( ( "%s::%s called.\n", getName ( ), __FUNCTION__ ) );
	
	status = DetermineIfMediaIsRemovable ( );
	if ( status != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "DetermineIfMediaIsRemovable returned error = %ld\n", ( UInt32 ) status ) );
		return false;
		
	}
	
	status = DetermineDeviceFeatures ( );
	if ( status != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "DetermineDeviceFeatures returned error = %ld\n", ( UInt32 ) status ) );
		return false;
		
	}
	
	return true;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::DetermineIfMediaIsRemovable ( void )
{
	
	SCSIServiceResponse				serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8							loop;
	UInt8							inquiryBufferCount = sizeof ( SCSICmd_INQUIRY_StandardData );
    SCSICmd_INQUIRY_StandardData * 	inquiryBuffer = NULL;
	IOMemoryDescriptor *			bufferDesc = NULL;
	SCSITaskIdentifier				request = NULL;
	IOReturn						succeeded = kIOReturnError;
	
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
		succeeded = kIOReturnNoMemory;
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
			
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::DetermineDeviceCharacteristics malformed command" ));
			goto ErrorExit;
			
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
				( GetTaskStatus( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			break;
			
		}
		
	}
	
	if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
		( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
	{
		
		goto ErrorExit;
		
	}
	
	succeeded = kIOReturnSuccess;
	
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
	
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::DetermineDeviceCharacteristics exiting\n" ) );
	
	
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
		
		IOFree ( ( void * ) inquiryBuffer, inquiryBufferCount );
		inquiryBuffer = NULL;
		
	}
	
	return succeeded;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::DetermineDeviceFeatures ( void )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	status = GetDeviceConfiguration ( );
	if ( status != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "GetDeviceConfiguration failed with status = %ld\n", ( UInt32 ) status ) );
		
	}
	
	status = GetMechanicalCapabilities ( );
	if ( status != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "GetMechanicalCapabilities failed with status = %ld\n", ( UInt32 ) status ) );
		
	}
	
	( void ) CheckForLowPowerPollingSupport ( );
	
	// Set Supported CD & DVD features flags
	setProperty ( kIOPropertySupportedCDFeatures, fSupportedCDFeatures, 32 );
	setProperty ( kIOPropertySupportedDVDFeatures, fSupportedDVDFeatures, 32 );
	
	fDeviceCharacteristicsDictionary->setObject ( kIOPropertySupportedCDFeatures, getProperty ( kIOPropertySupportedCDFeatures ) );
	fDeviceCharacteristicsDictionary->setObject ( kIOPropertySupportedDVDFeatures, getProperty ( kIOPropertySupportedDVDFeatures ) );
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::GetDeviceConfigurationSize ( UInt32 * size )
{
	
	IOMemoryDescriptor *		bufferDesc;
	SCSIServiceResponse			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8						featureHeader[kProfileDataLengthFieldSize];
	SCSITaskIdentifier			request;
	IOReturn					status;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	status = kIOReturnError;
	
	bufferDesc = IOMemoryDescriptor::withAddress ( 	featureHeader,
													kProfileDataLengthFieldSize,
													kIODirectionIn );
	
	bzero ( featureHeader, kProfileDataLengthFieldSize );
	
	request = GetSCSITask ( );
	
	if ( GET_CONFIGURATION ( 	request,
								bufferDesc,
								0x02,
								0x00,
								kProfileDataLengthFieldSize,
								0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GetDeviceConfigurationSize malformed command" ) );
	}

	bufferDesc->release ( );
		
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		*size 	= *( UInt32 * ) &featureHeader[0] + kProfileDataLengthFieldSize;
		status 	= kIOReturnSuccess;
		
		STATUS_LOG ( ( "size = %ld\n", *size ) );
		
		// Check if the drive supported the call but returned bad information
		if ( *size <= kProfileDataLengthFieldSize )
		{
			
			*size = 0;
			status = kIOReturnError;
			
		}
		
	}
	
	else
	{
		
		ERROR_LOG ( ( "%s::GET_CONFIGURATION returned serviceResponse = %d\n",
						getName ( ), serviceResponse ) );
		*size = 0;
		
	}
	
	ReleaseSCSITask ( request );
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::GetDeviceConfiguration ( void )
{
	
	IOBufferMemoryDescriptor *		bufferDesc;
	SCSIServiceResponse				serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8							numProfiles;
	UInt8 *							profilePtr;
	SCSITaskIdentifier				request;
	IOReturn						status;
	UInt32							actualProfileSize = 0;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	status = GetDeviceConfigurationSize ( &actualProfileSize );
	if ( status != kIOReturnSuccess )
	{
		return status;
	}
	
	// The number of profiles is the actual size minus the feature header
	// size minus the current profile size divided by the size of a profile
	// descriptor
	numProfiles = ( actualProfileSize - kProfileFeatureHeaderSize -
					kProfileDescriptorSize ) / kProfileDescriptorSize;
	
	if ( numProfiles < 1 )
	{
		status = kIOReturnError;
		return status;
	}
	
	STATUS_LOG ( ( "numProfiles = %d\n", numProfiles ) );
	
	bufferDesc = IOBufferMemoryDescriptor::withCapacity ( 
												actualProfileSize,
												kIODirectionIn,
												true );
	
	profilePtr = ( UInt8 * ) bufferDesc->getBytesNoCopy ( );
	bzero ( profilePtr, actualProfileSize );
	
	request = GetSCSITask ( );
	
	if ( GET_CONFIGURATION ( 	request,
								bufferDesc,
								0x02,
								0x00,
								actualProfileSize,
								0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GetDeviceConfiguration malformed command" ) );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		UInt8		currentProfileSize = kProfileDescriptorSize;
		
		// Adjust the pointer to be beyond the header and the current profile
		// to avoid duplicates
		profilePtr	= &profilePtr[kProfileFeatureHeaderSize + currentProfileSize];		
		status 		= ParseFeatureList ( numProfiles, profilePtr );
		
	}
	
	ReleaseSCSITask ( request );
	bufferDesc->release ( );
	
	// Check for Analog Audio Play Support
	if ( status == kIOReturnSuccess )
	{
		
		bufferDesc = IOBufferMemoryDescriptor::withCapacity ( 
												4,
												kIODirectionIn,
												true );
		
		request = GetSCSITask ( );
		
		if ( GET_CONFIGURATION ( 	request,
									bufferDesc,
									0x02,
									0x0103, /* Analog Audio Profile */
									4,
									0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GetDeviceConfiguration malformed command" ) );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			STATUS_LOG ( ( "device supports Analog Audio \n" ) );
			fSupportedCDFeatures |= kCDFeaturesAnalogAudioMask;
			
		}
		
		ReleaseSCSITask ( request );	
		bufferDesc->release ( );
		
	}
	
	// Check for DVD-CSS Support (on DVD-ROM drives only)
	if ( ( status == kIOReturnSuccess ) && ( fSupportedDVDFeatures & kDVDFeaturesReadStructuresMask ) )
	{
		
		bufferDesc = IOBufferMemoryDescriptor::withCapacity ( 
												4,
												kIODirectionIn,
												true );
		
		request = GetSCSITask ( );
	
		if ( GET_CONFIGURATION ( 	request,
									bufferDesc,
									0x02,
									0x0106, /* DVD-CSS Profile */
									4,
									0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GetDeviceConfiguration malformed command" ) );
		}
	
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			STATUS_LOG ( ( "device supports DVD-CSS \n" ) );
			fSupportedDVDFeatures |= kDVDFeaturesCSSMask;
			
		}
		
		ReleaseSCSITask ( request );	
		bufferDesc->release ( );
		
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ParseFeatureList ( UInt32 numProfiles, UInt8 * firstFeaturePtr )
{
	
	UInt16		profileNumber;
	UInt8 *		profilePtr;
	
	if ( ( numProfiles < 1 ) || ( firstFeaturePtr == NULL ) )
	{
		return kIOReturnBadArgument;
	}
	
	profilePtr = firstFeaturePtr;
	
	while ( numProfiles-- )
	{
		
		profileNumber = *( UInt16 * ) profilePtr;
		
		switch ( profileNumber )
		{
			
			case kGetConfigurationCDROM_Feature:
				STATUS_LOG ( ( "device supports CD-ROM\n" ) );
				fSupportedCDFeatures |= kCDFeaturesReadStructuresMask;
				break;
			
			case kGetConfigurationCDR_Feature:
				STATUS_LOG ( ( "device supports CD-R\n" ) );
				fSupportedCDFeatures |= kCDFeaturesWriteOnceMask;
				break;
			
			case kGetConfigurationCDRW_Feature:
				STATUS_LOG ( ( "device supports CD-RW\n" ) );
				fSupportedCDFeatures |= kCDFeaturesReWriteableMask;
				break;
			
			case kGetConfigurationDVDROM_Feature:
				STATUS_LOG ( ( "device supports DVD-ROM\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesReadStructuresMask;
				break;
			
			case kGetConfigurationDVDR_Feature:
				STATUS_LOG ( ( "device supports DVD-R\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesWriteOnceMask;
				break;
			
			case kGetConfigurationDVDRAM_Feature:
				STATUS_LOG ( ( "device supports DVD-RAM/DVD+RW\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesRandomWriteableMask;
				break;
			
			case kGetConfigurationDVDRW_Feature:
				STATUS_LOG ( ( "device supports DVD-RW\n" ) );
				fSupportedDVDFeatures |= kDVDFeaturesReWriteableMask;
			
			default:
				STATUS_LOG ( ( "%s::%s unknown drive type\n", getName ( ), __FUNCTION__ ) );
				break;
			
		}
		
		profilePtr += kProfileDescriptorSize;
		
	}
	
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::GetMechanicalCapabilitiesSize ( UInt32 * size )
{
	
	IOMemoryDescriptor *			bufferDesc;
	UInt8							parameterHeader[kModeSenseParameterHeaderSize];
	SCSIServiceResponse				serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier				request;
	IOReturn						status;
	
	STATUS_LOG ( ( "%s::%s called.\n", getName ( ), __FUNCTION__ ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( 	parameterHeader,
													kModeSenseParameterHeaderSize,
													kIODirectionIn );
	
	bzero ( parameterHeader, kModeSenseParameterHeaderSize );
	
	request = GetSCSITask ( );
	
	if ( MODE_SENSE_10 ( 	request,
							bufferDesc,
							0x00,
							0x00,
							0x00,
							0x2A,
							kModeSenseParameterHeaderSize,
							0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GetMechanicalCapabilitiesSize malformed command" ) );
	}
	
	bufferDesc->release ( );
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		*size 	= ( *( UInt16 * ) parameterHeader ) + sizeof ( UInt16 );
		status 	= kIOReturnSuccess;
		
		STATUS_LOG ( ( "size = %ld\n", *size ) );
		
		if ( *size <= kModeSenseParameterHeaderSize )
		{
			
			ERROR_LOG ( ( "Modes sense size wrong, size = %ld\n", *size ) );
			status = kIOReturnError;
			
		}
		
	}
	
	else
	{
		
		ERROR_LOG ( ( "Modes sense returned error\n" ) );
		*size 	= 0;
		status 	= kIOReturnError;
		
	}
	
	ReleaseSCSITask ( request );
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::GetMechanicalCapabilities ( void )
{
	
	UInt8 *							mechanicalCapabilitiesPtr;
	IOBufferMemoryDescriptor *		bufferDesc;
	SCSIServiceResponse				serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier				request;
	UInt32							actualSize = 0;
	IOReturn						status;
	
	STATUS_LOG ( ( "%s::%s called.\n", getName ( ), __FUNCTION__ ) );
	
	status = GetMechanicalCapabilitiesSize ( &actualSize );
	if ( status != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "GetMechanicalCapabilitiesSize returned error\n" ) );
		return status;
		
	}
	
	bufferDesc = IOBufferMemoryDescriptor::withCapacity ( 	actualSize,
															kIODirectionIn,
															true );
	
	mechanicalCapabilitiesPtr = ( UInt8 * ) bufferDesc->getBytesNoCopy ( );
	bzero ( mechanicalCapabilitiesPtr, actualSize );
	
	request = GetSCSITask ( );
	
	if ( MODE_SENSE_10 ( 	request,
							bufferDesc,
							0x00,
							0x00,
							0x00,
							0x2A,
							actualSize,
							0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GetMechanicalCapabilities malformed command" ) );
	}
	
	ReleaseSCSITask ( request );
	
	if ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		status = kIOReturnError;
		
	}
	
	else
	{
		
		mechanicalCapabilitiesPtr = &mechanicalCapabilitiesPtr[kModeSenseParameterHeaderSize + 2];
		status = ParseMechanicalCapabilities ( mechanicalCapabilitiesPtr );
				
	}
	
	bufferDesc->release ( );
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ParseMechanicalCapabilities ( UInt8 * mechanicalCapabilitiesPtr )
{
	
	if ( mechanicalCapabilitiesPtr == NULL )
	{
		return kIOReturnBadArgument;
	}
	
	if ( ( *mechanicalCapabilitiesPtr & kMechanicalCapabilitiesDVDROMMask ) != 0 )
	{
		
		STATUS_LOG ( ( "device supports DVD-ROM\n" ) );
		fSupportedDVDFeatures |= ( kDVDFeaturesReadStructuresMask | kDVDFeaturesCSSMask );
		
	}
	
	// Hop to the next byte so we can check more capabilities
	mechanicalCapabilitiesPtr++;		
	
	if ( ( *mechanicalCapabilitiesPtr & kMechanicalCapabilitiesDVDRAMMask ) != 0 )
	{
		
		STATUS_LOG ( ( "device supports DVD-RAM\n" ) );
		fSupportedDVDFeatures |= kDVDFeaturesRandomWriteableMask;
	
	}
	
	if ( ( *mechanicalCapabilitiesPtr & kMechanicalCapabilitiesDVDRMask ) != 0 )
	{
		
		STATUS_LOG ( ( "device supports DVD-R\n" ) );
		fSupportedDVDFeatures |= kDVDFeaturesWriteOnceMask;
	
	}
	
	if ( ( *mechanicalCapabilitiesPtr & kMechanicalCapabilitiesCDRWMask ) != 0 )
	{
		
		STATUS_LOG ( ( "device supports CD-RW\n" ) );
		fSupportedCDFeatures |= kCDFeaturesReWriteableMask;
	
	}
	
	if ( ( *mechanicalCapabilitiesPtr & kMechanicalCapabilitiesCDRMask ) != 0 )
	{
		
		STATUS_LOG ( ( "device supports CD-R\n" ) );
		fSupportedCDFeatures |= kCDFeaturesWriteOnceMask;
	
	}
	
	// Hop to the next byte so we can check more capabilities
	mechanicalCapabilitiesPtr++;		
	
	if ( ( *mechanicalCapabilitiesPtr & kMechanicalCapabilitiesAnalogAudioMask ) != 0 )
	{
		
		STATUS_LOG ( ( "device supports Analog Audio \n" ) );
		fSupportedCDFeatures |= kCDFeaturesAnalogAudioMask;
	
	}
	
	// Hop to the next byte so we can check more capabilities
	mechanicalCapabilitiesPtr++;		
	
	if ( ( *mechanicalCapabilitiesPtr & kMechanicalCapabilitiesCDDAStreamAccurateMask ) != 0 )
	{
		
		STATUS_LOG ( ( "device supports CD-DA stream accurate reads\n" ) );
		fSupportedCDFeatures |= kCDFeaturesCDDAStreamAccurateMask;
	
	}
	
	// Since it responded to the CD Mechanical Capabilities Mode Page, it must at
	// least be a CD-ROM...
	STATUS_LOG ( ( "device supports CD-ROM\n" ) );
	fSupportedCDFeatures |= kCDFeaturesReadStructuresMask;
	
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::GetMediaAccessSpeed ( UInt16 * kilobytesPerSecond )
{
	
	UInt32					mechanicalCapabilitiesSize 	= 0;
	IOReturn				status						= kIOReturnError;
	UInt8 *					mechanicalCapabilitiesPtr 	= NULL;
	SCSITaskIdentifier		request						= NULL;
	SCSIServiceResponse		serviceResponse 			= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	status = GetMechanicalCapabilitiesSize ( &mechanicalCapabilitiesSize );
	if ( status == kIOReturnSuccess )
	{
		
		IOBufferMemoryDescriptor *	bufferDesc = NULL;
		
		bufferDesc = IOBufferMemoryDescriptor::withCapacity ( 	mechanicalCapabilitiesSize,
																kIODirectionIn,
																true );
		
		mechanicalCapabilitiesPtr = ( UInt8 * ) bufferDesc->getBytesNoCopy ( );
		bzero ( mechanicalCapabilitiesPtr, mechanicalCapabilitiesSize );
		
		request = GetSCSITask ( );
		
		if ( MODE_SENSE_10 ( 	request,
								bufferDesc,
								0x00,
								0x00,
								0x00,
								0x2A,
								mechanicalCapabilitiesSize,
								0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GetMechanicalCapabilities malformed command" ) );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			mechanicalCapabilitiesPtr = &mechanicalCapabilitiesPtr[kModeSenseParameterHeaderSize + 14];
			*kilobytesPerSecond = *( UInt16 * ) mechanicalCapabilitiesPtr;
			status = kIOReturnSuccess;
			
		}
		
		else
		{
			status = kIOReturnError;
		}
		
		ReleaseSCSITask ( request );		
		bufferDesc->release ( );
		
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::SetMediaAccessSpeed ( UInt16 kilobytesPerSecond )
{

	IOReturn				status 			= kIOReturnError;
	SCSITaskIdentifier 		request			= NULL;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::SetMediaAccessSpeed called\n" ) );
			
	request = GetSCSITask ( );
	
	if ( request != NULL )
	{
		
		switch ( fMediaType )
		{
			
			case kCDMediaTypeROM:
			case kCDMediaTypeR:
			case kCDMediaTypeRW:
				if ( SET_CD_SPEED ( request, kilobytesPerSecond, 0, 0 ) == true )
				{
					serviceResponse = SendCommand ( request, 0 );
				}
				else
				{
					PANIC_NOW ( ( "Invalid SET_CD_SPEED command in SetMediaAccessSpeed\n" ) );
				}
				break;

			default:
				break;
			
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			fCurrentDiscSpeed = kilobytesPerSecond;
			status = kIOReturnSuccess;
			
		}
		
		ReleaseSCSITask ( request );
		
	}
	
	return status;
	
}


void
IOSCSIMultimediaCommandsDevice::SetMediaCharacteristics ( UInt32 blockSize, UInt32 blockCount )
{
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::SetMediaCharacteristics called\n" ) );
	
	STATUS_LOG ( ( "mediaBlockSize = %ld, blockCount = %ld\n", blockSize, blockCount ) );
	
	fMediaBlockSize		= blockSize;
	fMediaBlockCount	= blockCount;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::SetMediaCharacteristics exiting\n" ) );
	
}


void
IOSCSIMultimediaCommandsDevice::ResetMediaCharacteristics ( void )
{
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ResetMediaCharacteristics called\n" ) );
	
	fMediaBlockSize		= 0;
	fMediaBlockCount	= 0;
	fMediaPresent		= false;
	fMediaType			= kCDMediaTypeUnknown;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ResetMediaCharacteristics exiting\n" ) );
	
}


void
IOSCSIMultimediaCommandsDevice::PollForMedia ( void )
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
	if ( TEST_UNIT_READY ( request, 0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::PollForMedia malformed command" ) );
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			validSense = GetAutoSenseData ( request, &senseBuffer );
			if ( validSense == false )
			{
				
				// Get the sense data to determine if media is present.
				// This will eventually use the autosense data if the
				// Transport Protocol supports it else issue the REQUEST_SENSE.		  
				if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
				{
					// The command was successfully built, now send it
					serviceResponse = SendCommand ( request, 0 );
				}
				
				else
				{
					PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::PollForMedia malformed command" ) );
				}
				
			}
			
			if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x00 ) && 
				( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
			{
				
				mediaFound = true;
				
			}
			
			// Check other types of mechanical errors -> eject medium
			else if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x09 ) && 
				( ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) ||
				  ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x01 ) ||
				  ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) ||
				  ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x03 ) ) )
			{
				
				// We got an error from the drive and will not be able to access
				// this medium at all. Just eject it...
				
				EjectTheMedia ( );
				
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
		if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, 1, 0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::PollForMedia malformed command" ) );
		}
		
	}
	
	bufferDesc = IOMemoryDescriptor::withAddress (	capacityData,
													8,
													kIODirectionIn );
	
	// We found media, get its capacity
	if ( READ_CAPACITY ( 	request,
							bufferDesc,
							0,
							0x00,
							0,
							0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::PollForMedia malformed command" ) );
	}
	
	taskStatus = GetTaskStatus ( request );
	ReleaseSCSITask ( request );
	bufferDesc->release ( );
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( taskStatus == kSCSITaskStatus_GOOD ) )
	{
				
		if ( capacityData[0] == 0 )
		{
			
			// If the last block address is zero, set the characteristics with
			// the returned data.
			SetMediaCharacteristics ( capacityData[1], capacityData[0] );
			
		}
		
		else
		{
			
			// If the last block address is not zero, increment it by one to 
			// get the total number of blocks on the media.
			SetMediaCharacteristics ( OSSwapBigToHostInt32 ( capacityData[1] ),
									  OSSwapBigToHostInt32 ( capacityData[0] ) + 1 );
			
		}
		
		STATUS_LOG ( ( "%s: Media capacity: 0x%x and block size: 0x%x\n",
						getName ( ), fMediaBlockCount, fMediaBlockSize ) );
		
	}
	
	else
	{
		
		ERROR_LOG ( ( "%s: Read Capacity failed\n", getName ( ) ) );
		return;
		
	}
	
	DetermineMediaType ( );
	
	CheckWriteProtection ( );
	
	fMediaPresent	= true;
	fMediaChanged	= true;
	fPollingMode 	= kPollingMode_Suspended;

	// Message up the chain that we have media
	messageClients ( kIOMessageMediaStateHasChanged,
					 ( void * ) kIOMediaStateOnline,
					 sizeof ( IOMediaState ) );
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::CheckForLowPowerPollingSupport ( void )
{
	
	IOReturn			status 		= kIOReturnSuccess;
	OSBoolean *			boolValue 	= NULL;
	const OSSymbol * 	key			= OSSymbol::withCString ( kIOPropertyPhysicalInterconnectLocationKey );
	OSDictionary *		dict		= NULL;
	
	
	if ( fDeviceSupportsLowPowerPolling )
		fDeviceSupportsLowPowerPolling = IsProtocolServiceSupported ( 
												kSCSIProtocolFeature_ProtocolSpecificPolling,
												NULL );
	
	dict = GetProtocolCharacteristicsDictionary ( );
	if ( dict != NULL )
	{
		
		internalString = OSDynamicCast ( OSString, dict->getObject ( key ) );
		
	}
	
    if ( ( internalString == NULL ) || ( !internalString->isEqualTo ( kIOPropertyInternalKey ) ) )
	{
		
		// Not an internal drive, let's not use the power conditions mode page
		// info or low power polling
		fDeviceSupportsPowerConditions = false;
		fDeviceSupportsLowPowerPolling = false;
		
	}
	
	// If the drive is not a DVD drive, we won't use power conditions,
	// we'll either use ATA style sleep commands for ATAPI drives or just
	// spin down the drives if they are external.
	if ( fSupportedDVDFeatures == 0 )
	{
		fDeviceSupportsPowerConditions = false;
	}
		
	if ( key != NULL )
	{
		key->release ( );
	}
	
	boolValue = OSBoolean::withBoolean ( fDeviceSupportsLowPowerPolling );
	if ( boolValue != NULL )
	{
		
		fDeviceCharacteristicsDictionary->setObject ( kIOPropertyLowPowerPolling, boolValue );
		boolValue->release ( );
		boolValue = NULL;
		
	}
	
	return status;
	
}


void
IOSCSIMultimediaCommandsDevice::DetermineMediaType ( void )
{

	bool	mediaTypeFound = false;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	mediaTypeFound = CheckForDVDMediaType ( );
	if ( mediaTypeFound == false )
	{
		
		mediaTypeFound = CheckForCDMediaType ( );
		
	}
	
	// Set to maximum speed
	SetMediaAccessSpeed ( 0xFFFF );
	
	STATUS_LOG ( ( "mediaTypeFound = %d\n", mediaTypeFound ) );
	
}


bool
IOSCSIMultimediaCommandsDevice::CheckForDVDMediaType ( void )
{
	
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskStatus			taskStatus;
	DVDPhysicalFormatInfo	physicalFormatInfo;
	IOMemoryDescriptor *	bufferDesc;
	bool					mediaTypeFound = false;

	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	// If device supports READ_DVD_STRUCTURE, issue one to find
	// out if the media is a DVD media type
	if ( fSupportedDVDFeatures & kDVDFeaturesReadStructuresMask )
	{
				
		bufferDesc = IOMemoryDescriptor::withAddress ( 	( void * ) &physicalFormatInfo,
														kDVDPhysicalFormatInfoBufferSize,
														kIODirectionIn );
		
		request = GetSCSITask ( );
		
		if ( READ_DVD_STRUCTURE ( 	request,
									bufferDesc,
									0x00,
									0x00,
									0x00, /* kDVDStructureFormatPhysicalFormatInfo */
									kDVDPhysicalFormatInfoBufferSize,
									0x00,
									0x00 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::CheckForDVDMediaType malformed command" ) );
		}

		taskStatus = GetTaskStatus ( request );
		ReleaseSCSITask ( request );
		bufferDesc->release ( );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( taskStatus == kSCSITaskStatus_GOOD ) )
		{
			
			switch ( physicalFormatInfo.bookType )
			{
				
				case 0:
					STATUS_LOG ( ( "fMediaType = DVD-ROM\n" ) );
					fMediaType = kDVDMediaTypeROM;
					break;
					
				case 1:
					STATUS_LOG ( ( "fMediaType = DVD-RAM\n" ) );
					fMediaType = kDVDMediaTypeRAM;
					break;
					
				case 2:
					STATUS_LOG ( ( "fMediaType = DVD-R\n" ) );
					fMediaType = kDVDMediaTypeR;
					break;
					
				case 3:
					STATUS_LOG ( ( "fMediaType = DVD-RW\n" ) );
					fMediaType = kDVDMediaTypeRW;
					break;
					
				case 9:
					STATUS_LOG ( ( "fMediaType = DVD+RW\n" ) );
					fMediaType = kDVDMediaTypePlusRW;
					break;
					
			}
				
		}
	
	}

	if ( fMediaType != kCDMediaTypeUnknown )
	{
		mediaTypeFound = true;
	}
	
	return mediaTypeFound;
	
}
		

bool
IOSCSIMultimediaCommandsDevice::CheckForCDMediaType ( void )
{
	
	UInt8					tocBuffer[4];
	IOMemoryDescriptor * 	bufferDesc;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	bool					mediaTypeFound = false;
	SCSITaskStatus			taskStatus;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( 	tocBuffer,
													sizeof ( tocBuffer ),
													kIODirectionIn );
	if ( bufferDesc == NULL )
	{
		
		ERROR_LOG ( ( "Could not allocate bufferDesc\n" ) );
		return false;
		
	}
	
	// Issue a READ_TOC_PMA_ATIP to find out if the media is
	// finalized or not
	if ( READ_TOC_PMA_ATIP ( 	request,
								bufferDesc,
								0x00,
								0x00,
								0x00,
								sizeof ( tocBuffer ),
								0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::CheckForCDMediaType malformed command" ) );
	}
	
	STATUS_LOG ( ( "%s::%s serviceResponse = %x\n", getName ( ), __FUNCTION__, serviceResponse ) );
	
	bufferDesc->release ( );
	bufferDesc = NULL;
	taskStatus = GetTaskStatus ( request );
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( taskStatus == kSCSITaskStatus_GOOD ) )
	{
		
		UInt8	discInfoBuffer[4];
		
		bufferDesc = IOMemoryDescriptor::withAddress ( 	discInfoBuffer,
														sizeof ( discInfoBuffer ),
														kIODirectionIn );
		
		if ( bufferDesc == NULL )
		{
			
			ERROR_LOG ( ( "Could not allocate bufferDesc\n" ) );
			return false;
			
		}

		
		if ( fSupportedCDFeatures & kCDFeaturesWriteOnceMask )
		{
			
			if ( READ_DISC_INFORMATION ( 	request,
											bufferDesc,
											sizeof ( discInfoBuffer ),
											0 ) == true )
			{
				// The command was successfully built, now send it
				serviceResponse = SendCommand ( request, 0 );
			}
			
			else
			{
				PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::CheckForCDMediaType malformed command" ) );
			}
			
			bufferDesc->release ( );
			bufferDesc = NULL;
			taskStatus = GetTaskStatus ( request );
			
			if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
				( taskStatus == kSCSITaskStatus_GOOD ) )
			{

				switch ( discInfoBuffer[2] & kDiscStatusMask )
				{
					
					case kDiscStatusEmpty:
						PANIC_NOW ( ( "A disc with a valid TOC should never be empty" ) );
						break;
					
					case kDiscStatusOther:
					case kDiscStatusIncomplete:
						break;
					
					case kDiscStatusComplete:
						STATUS_LOG ( ( "fMediaType = CD-ROM\n" ) );
						fMediaType = kCDMediaTypeROM;
						mediaTypeFound = true;
						break;
					

				}
				
			}
		
		}
		
		else
		{
			
			// The drive is not a CD-R/W drive, so we mark the media as
			// finalized since it can't be written to.
			STATUS_LOG ( ( "fMediaType = CD-ROM\n" ) );
			fMediaType = kCDMediaTypeROM;
			mediaTypeFound = true;
			
		}
		
	}
	
	if ( mediaTypeFound == false )
	{
		
		if ( fSupportedCDFeatures & kCDFeaturesWriteOnceMask )
		{
			
			UInt8	atipBuffer[kATIPBufferSize];
			UInt8	trackInfoBuffer[kTrackInfoBufferSize];
			
			bufferDesc = IOMemoryDescriptor::withAddress ( 	atipBuffer,
															kATIPBufferSize,
															kIODirectionIn );
			
			if ( READ_TOC_PMA_ATIP ( 	request,
										bufferDesc,
										0x00,
										0x04,
										0x00,
										sizeof ( atipBuffer ),
										0 ) == true )
			{
				// The command was successfully built, now send it
				serviceResponse = SendCommand ( request, 0 );
			}
			
			else
			{
				PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::CheckForCDMediaType malformed command" ) );
			}
	
			bufferDesc->release ( );
			taskStatus = GetTaskStatus ( request );
			
			if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
				 ( taskStatus == kSCSITaskStatus_GOOD ) )
			{
				
				// Check the DiscType field in byte 7 of the READ_TOC_PMA_ATIP
				// format 0x04 to see if the disc is CD-RW or CD-R
				if ( atipBuffer[6] & kDiscTypeCDRWMask )
				{
					
					STATUS_LOG ( ( "fMediaType = CD-RW\n" ) );
					fMediaType = kCDMediaTypeRW;
					
				}
				
				else
				{
					
					STATUS_LOG ( ( "fMediaType = CD-R\n" ) );
					fMediaType = kCDMediaTypeR;
				
				}

				bufferDesc = IOMemoryDescriptor::withAddress ( 	trackInfoBuffer,
																kTrackInfoBufferSize,
																kIODirectionIn );

				// Check to see if the medium is blank
				if ( READ_TRACK_INFORMATION ( 	request,
												bufferDesc,
												0x00,
												0x01,
												kTrackInfoBufferSize,
												0 ) == true )
				{
					// The command was successfully built, now send it
					serviceResponse = SendCommand ( request, 0 );
				}
				
				else
				{
					PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::CheckForCDMediaType malformed command" ) );
				}
				
				bufferDesc->release ( );
				taskStatus = GetTaskStatus ( request );
				
				if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
					 ( taskStatus == kSCSITaskStatus_GOOD ) )
				{
					
					if ( trackInfoBuffer[6] & 0x40 )
					{
						
						STATUS_LOG ( ( "media is blank\n" ) );
						// Yes it's blank, make sure the blockCount zero.
						fMediaBlockCount = 0;
						
					}
					
				}
				
				mediaTypeFound = true;
				
			}
			
		}
		
	}
	
	ReleaseSCSITask ( request );
	
	return mediaTypeFound;
	
}


void
IOSCSIMultimediaCommandsDevice::CheckWriteProtection ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *	bufferDesc;
	UInt8					buffer[16];
	SCSITaskIdentifier		request;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	// Assume it is write protected
	fMediaIsWriteProtected = true;
	
	if ( fMediaType != kDVDMediaTypeRAM )
	{
		return;
		
	}
	
	bufferDesc = IOMemoryDescriptor::withAddress (	buffer,
													sizeof ( buffer ),
													kIODirectionIn );
	
	request = GetSCSITask ( );
	
	if ( GET_CONFIGURATION ( 	request,
								bufferDesc,
								0x02,
								0x20,
								sizeof ( buffer ),
								0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::CheckWriteProtection malformed command" ) );
	}

	bufferDesc->release ( );
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		// The current bit in the Random Writable Descriptor
		// tells us whether the disc is write protected. It is located
		// at byte 2 of the Random Writable Descriptor Feature page
		if ( buffer[kProfileFeatureHeaderSize + 2] & kRandomWritableProtectionMask )
		{
			
			fMediaIsWriteProtected = false;
			
		}
	
	}

	ReleaseSCSITask ( request );
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::SyncReadWrite ( IOMemoryDescriptor *	buffer,
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


void 
IOSCSIMultimediaCommandsDevice::AsyncReadWriteComplete ( SCSITaskIdentifier request )
{
	
	IOReturn							status;
	UInt64								actCount = 0;
	IOSCSIMultimediaCommandsDevice	*	taskOwner;
	void *								clientData;
	SCSITask *							scsiRequest;
		
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		PANIC_NOW ( ( "IOSCSIBlockCommandsDevice::AsyncReadWriteComplete scsiRequest==NULL." ) );
	}

	taskOwner = OSDynamicCast ( IOSCSIMultimediaCommandsDevice, scsiRequest->GetTaskOwner ( ) );
	if ( taskOwner == NULL )
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::AsyncReadWriteComplete taskOwner==NULL." ) );
	}
	
	// Extract the client data from the SCSITask	
	clientData	= scsiRequest->GetApplicationLayerReference ( );
	
	if ( ( scsiRequest->GetServiceResponse ( ) == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( scsiRequest->GetTaskStatus ( ) == kSCSITaskStatus_GOOD ) )
	{
		
		// Our status is good, so return a success
		status = kIOReturnSuccess;
		actCount = scsiRequest->GetRealizedDataTransferCount ( );
		
	}
	
	else
	{
		// Either the task never completed or we have a status other than GOOD,
		// return an error.		
		if ( scsiRequest->GetTaskStatus ( ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			SCSI_Sense_Data		senseDataBuffer;
			bool				senseIsValid;
			
			senseIsValid = scsiRequest->GetAutoSenseData ( &senseDataBuffer );
			if ( senseIsValid )
			{
				
				ERROR_LOG ( ( "ASC = 0x%02x, ASCQ = 0x%02x\n", 
				senseDataBuffer.ADDITIONAL_SENSE_CODE,
				senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
				
				if ( ( senseDataBuffer.ADDITIONAL_SENSE_CODE == 0x3A ) ||
					 ( ( senseDataBuffer.ADDITIONAL_SENSE_CODE == 0x28 ) &&
					   ( senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) ) )
				{
					
					// Message up the chain that we do not have media
					taskOwner->messageClients ( kIOMessageMediaStateHasChanged,
												( void * ) kIOMediaStateOffline,
												sizeof ( IOMediaState ) );
					
					taskOwner->ResetMediaCharacteristics ( );
					taskOwner->EnablePolling ( );
					
				}
				
			}
			
		}
		
		status = kIOReturnError;
		
	}
		
	taskOwner->ReleaseSCSITask ( request );
		
	if ( taskOwner->fSupportedDVDFeatures & kDVDFeaturesReadStructuresMask )
	{
		
		IODVDServices::AsyncReadWriteComplete ( clientData, status, actCount );
	
	}
	
	else
	{
		
		IOCompactDiscServices::AsyncReadWriteComplete ( clientData, status, actCount );
	
	}
	
}



IOReturn
IOSCSIMultimediaCommandsDevice::IssueRead ( IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount )
{
	
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOReturn				status = kIOReturnSuccess;
	SCSITaskIdentifier		request;
	
	STATUS_LOG ( ( "%s::%s attempted\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );
	
	if ( READ_10 ( 	request,
					buffer,
					fMediaBlockSize,
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
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::IssueRead malformed command" ) );
	}

	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		status = kIOReturnError;
	}

	ReleaseSCSITask ( request );
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::syncRead status = %ld\n", ( UInt32 ) status ) );
			
	return status;
		
}


IOReturn
IOSCSIMultimediaCommandsDevice::IssueWrite ( IOMemoryDescriptor *	buffer,
											 UInt64					startBlock,
											 UInt64					blockCount )
{

	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOReturn				status = kIOReturnSuccess;
	SCSITaskIdentifier		request;
	
	STATUS_LOG ( ( "%s::%s Attempted\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );
	if ( WRITE_10 ( request,
					buffer,
					fMediaBlockSize,
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
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::IssueWrite malformed command" ) );
	}

	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		status = kIOReturnError;
	}

	ReleaseSCSITask ( request );
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::AsyncReadWrite ( 	IOMemoryDescriptor *	buffer,
													UInt64					startBlock,
													UInt64					blockCount,
													void *					clientData )
{
	
	IODirection		direction;
	IOReturn		status;
	
	direction = buffer->getDirection ( );
	
	if ( direction == kIODirectionIn )
	{
		
		IssueRead ( buffer, clientData, startBlock, blockCount );
		status = kIOReturnSuccess;
	
	}
	
	else if ( direction == kIODirectionOut )
	{
		
		IssueWrite ( buffer, clientData, startBlock, blockCount );
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
IOSCSIMultimediaCommandsDevice::IssueRead ( IOMemoryDescriptor *	buffer,
											void *					clientData,
											UInt64					startBlock,
											UInt64					blockCount )
{
	
	IOReturn 				status		= kIOReturnSuccess;
	SCSITaskIdentifier		request;

	STATUS_LOG ( ( "%s::%s Attempted\n", getName ( ), __FUNCTION__ ) );

	request = GetSCSITask ( );
	
	if ( READ_10 ( 	request,
					buffer,
					fMediaBlockSize,
					0,
					0,
					0,
					startBlock,
					blockCount,
					0 ) == true )
	{
		
		SetApplicationLayerReference( request, clientData );
		
		// The command was successfully built, now send it
		SendCommand ( request, 0, &IOSCSIMultimediaCommandsDevice::AsyncReadWriteComplete );
		
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::IssueRead malformed command" ) );
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::IssueWrite ( IOMemoryDescriptor *	buffer,
											 void *					clientData,
											 UInt64					startBlock,
											 UInt64					blockCount )
{
	
	IOReturn 				status		= kIOReturnSuccess;
	SCSITaskIdentifier		request;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );
	
	if ( WRITE_10 ( request, 
					buffer,
					fMediaBlockSize,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 ) == true )
	{
		
		SetApplicationLayerReference( request, clientData );
		
		// The command was successfully built, now send it
		SendCommand ( request, 0, &IOSCSIMultimediaCommandsDevice::AsyncReadWriteComplete );
		
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::IssueRead malformed command" ) );
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::GetTrayState ( UInt8 * trayState )
{
	
	IOReturn				status = kIOReturnError;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					statusBuffer[8];
	IOMemoryDescriptor *	buffer;
	
	CheckPowerState ( );
	
	request = GetSCSITask ( );
	
	buffer = IOMemoryDescriptor::withAddress ( 	statusBuffer,
												8,
												kIODirectionIn );
		
	if ( GET_EVENT_STATUS_NOTIFICATION ( request,
										 buffer,
										 1,
										 1 << 4, /* media status notification event */
										 8,
										 0x00 ) == true )
	{
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GetTrayState malformed command" ) );
	}
	
	buffer->release ( );
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		STATUS_LOG ( ( "GET_EVENT_STATUS_NOTIFICATION succeeded.\n" ) );
		*trayState = statusBuffer[5] & 0x01;
		STATUS_LOG ( ( "trayState = %d.\n", *trayState ) );
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		// The device doesn't support the GET_EVENT_STATUS_NOTIFICATION.
		// Assume the tray is shut.
		ERROR_LOG ( ( "GET_EVENT_STATUS_NOTIFICATION failed.\n" ) );
		*trayState = 0;
		STATUS_LOG ( ( "trayState = %d.\n", *trayState ) );
		status = kIOReturnSuccess;
		
	}
	
	ReleaseSCSITask ( request );
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::SetTrayState ( UInt8 trayState )
{
	
	IOReturn				status = kIOReturnError;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	CheckPowerState ( );
	
	if ( fMediaPresent )
	{
		ERROR_LOG ( ( "Media present, not permitted to send SetTrayState\n" ) );
		return kIOReturnNotPermitted;
	}
	
	request = GetSCSITask ( );
	
	// Set to desired tray state.
	if ( START_STOP_UNIT ( request, 0, 0, 1, !trayState, 0 ) == true )
	{
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SetTrayState malformed command" ) );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		STATUS_LOG ( ( "START_STOP_UNIT succeeded.\n" ) );
		status = kIOReturnSuccess;
	}
	
	ReleaseSCSITask ( request );
	
	return status;
	
}

IOReturn
IOSCSIMultimediaCommandsDevice::EjectTheMedia ( void )
{
	
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );
	
	if ( fMediaIsRemovable == false )
	{
		
		if ( SYNCHRONIZE_CACHE ( request, 0, 0, 0, 0, 0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::EjectTheMedia malformed command" ) );
		}

		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			ReleaseSCSITask ( request );
			return kIOReturnSuccess;
		
		}
		
		else
		{

			ReleaseSCSITask ( request );
			return kIOReturnError;
		
		}
		
	}

	if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, 0, 0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::EjectTheMedia malformed command" ) );
	}

	if ( START_STOP_UNIT ( request, 0, 0, 1, 0, 0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::EjectTheMedia malformed command" ) );
	}

	ReleaseSCSITask ( request );
	
	ResetMediaCharacteristics ( );
	
	fMediaIsWriteProtected = true;

	fCurrentDiscSpeed = 0;
	
	if ( fLowPowerPollingEnabled == false )
	{
		
		// Set the polling to determine when new media has been inserted
		fPollingMode = kPollingMode_NewMedia;
		TicklePowerManager ( );
		EnablePolling ( );
		
	}
	
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::FormatMedia ( UInt64 byteCapacity )
{
	
	IOReturn	status = kIOReturnError;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	return status;
	
}


UInt32
IOSCSIMultimediaCommandsDevice::GetFormatCapacities ( 	UInt64 * capacities,
														UInt32   capacitiesMaxCount ) const
{

	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	return 0;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::LockUnlockMedia ( bool doLock )
{
	
	IOReturn				status = kIOReturnSuccess;
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::SynchronizeCache ( void )
{
	
	IOReturn				status = kIOReturnError;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	request = GetSCSITask ( );
	
	if ( SYNCHRONIZE_CACHE ( request, 0, 0, 0, 0, 0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SynchronizeCache malformed command" ) );
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
IOSCSIMultimediaCommandsDevice::ReportBlockSize ( UInt64 * blockSize )
{
	
	STATUS_LOG ( ( "%s::%s blockSize = %ld\n", getName ( ),
					__FUNCTION__, fMediaBlockSize ) );
	
	*blockSize = fMediaBlockSize;
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReportEjectability ( bool * isEjectable )
{

	STATUS_LOG ( ( "%s::%s mediaIsRemovable = %d\n", getName ( ),
					__FUNCTION__, ( int ) fMediaIsRemovable ) );
	
	*isEjectable = fMediaIsRemovable;
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReportLockability ( bool * isLockable )
{

	STATUS_LOG ( ( "%s::%s isLockable = %d\n", getName ( ),
					__FUNCTION__, ( int ) true ) );
	
	*isLockable = true;
	return kIOReturnSuccess;

}


IOReturn
IOSCSIMultimediaCommandsDevice::ReportPollRequirements ( bool * pollIsRequired,
														 bool * pollIsExpensive )
{

	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	
	*pollIsRequired 	= false;
	*pollIsExpensive 	= false;
	
	return kIOReturnSuccess;

}


IOReturn
IOSCSIMultimediaCommandsDevice::ReportMaxReadTransfer ( UInt64 		blockSize,
														UInt64 * 	max )
{

	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	
	*max = blockSize * 256;
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReportMaxValidBlock ( UInt64 * maxBlock )
{
	
	if ( fMediaBlockCount == 0 )
	{
		// If the capacity is zero, return that for
		// the max valid block.
		*maxBlock = 0;
	}
	
	else
	{
		// Since the driver stores the number of blocks, and
		// blocks are addressed starting at zero, subtract one
		// to get the maximum valid block.
		*maxBlock = fMediaBlockCount - 1;
	}

	STATUS_LOG ( ( "%s::%s maxBlockHi = 0x%x, maxBlockLo = 0x%x\n", getName ( ),
					__FUNCTION__, ( *maxBlock ), ( *maxBlock ) >> 32 ) );
	
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReportMaxWriteTransfer ( UInt64		blockSize,
														 UInt64 *	max )
{

	STATUS_LOG ( ( "%s::%s.\n", getName ( ), __FUNCTION__ ) );

	return ReportMaxReadTransfer ( blockSize, max );
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReportMediaState ( 	bool * mediaPresent,
													bool * changed )
{

	STATUS_LOG ( ( "%s::%s.\n", getName ( ), __FUNCTION__ ) );

	STATUS_LOG ( ( "fMediaPresent = %d.\n", fMediaPresent ) );
	STATUS_LOG ( ( "fMediaChanged = %d.\n", fMediaChanged ) );
	
	*mediaPresent 	= fMediaPresent;
	*changed 		= fMediaChanged;
	
	if ( fMediaChanged )
	{
		
		fMediaChanged = !fMediaChanged;
	
	}
	
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReportRemovability ( bool * isRemovable )
{

	STATUS_LOG ( ( "%s::%s isRemovable = %d.\n", getName ( ),
					__FUNCTION__, fMediaIsRemovable ) );
	
	*isRemovable = fMediaIsRemovable;
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReportWriteProtection ( bool * isWriteProtected )
{
	
	STATUS_LOG ( ( "%s::%s isWriteProtected = %d.\n",
					getName ( ), __FUNCTION__, fMediaIsWriteProtected ) );	
	
	*isWriteProtected = fMediaIsWriteProtected;
	return kIOReturnSuccess;
	
}


void
IOSCSIMultimediaCommandsDevice::sPollForMedia ( void * pdtDriver, void * refCon )
{
	
	IOSCSIMultimediaCommandsDevice *	driver;
	
	driver = ( IOSCSIMultimediaCommandsDevice * ) pdtDriver;
	
	driver->PollForMedia ( );
	
	if ( driver->fPollingMode != kPollingMode_Suspended )
	{
		
		// schedule the poller again
		driver->EnablePolling ( );
		
	}
	
	// drop the retain associated with this poll
	driver->release ( );
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::AsyncReadCD ( 	IOMemoryDescriptor *	buffer,
												UInt32					startBlock,
												UInt32					blockCount,
												CDSectorArea			sectorArea,
												CDSectorType			sectorType,
												void *					clientData )
{
	
	IOReturn				status = kIOReturnSuccess;
	SCSITaskIdentifier		request;

	STATUS_LOG ( ( "%s::%s Attempted\n", getName ( ), __FUNCTION__ ) );
	request = GetSCSITask ( );
	
	if ( READ_CD (	request,
					buffer,
					sectorType,
					0,
					startBlock, 
					blockCount, 
					( ( sectorArea & ~kCDSectorAreaSubChannel ) >> 7 ) & 0x1,
					( ( sectorArea & ~kCDSectorAreaSubChannel ) >> 5 ) & 0x3,
					( ( sectorArea & ~kCDSectorAreaSubChannel ) >> 4 ) & 0x1,
					( ( sectorArea & ~kCDSectorAreaSubChannel ) >> 3 ) & 0x1,
					( ( sectorArea & ~kCDSectorAreaSubChannel ) >> 1 ) & 0x3,
					sectorArea & kCDSectorAreaSubChannel,
					0 ) == true )
	{
		
		SetApplicationLayerReference ( request, clientData );
		
		// The command was successfully built, now send it
		SendCommand ( request, 0, &IOSCSIMultimediaCommandsDevice::AsyncReadWriteComplete );
		
	}
	
	else
	{
		status = kIOReturnUnsupported;
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::IssueRead malformed command" ) );
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReadISRC ( UInt8 track, CDISRC isrc )
{
	
	IOReturn				status = kIOReturnError;
	IOMemoryDescriptor *	desc;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					isrcData[kSubChannelDataBufferSize];
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReadISRC called\n" ) );
	
	desc = IOMemoryDescriptor::withAddress ( isrcData, kSubChannelDataBufferSize, kIODirectionIn );
	
	request = GetSCSITask ( );
	
	if ( READ_SUB_CHANNEL ( request,
							desc,
							1,
							1,
							0x03,
							track,
							kSubChannelDataBufferSize,
							0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::ReadISRC malformed command" ) );
	}
	
	desc->release ( );

	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		// Check if we found good data.
		if ( isrcData[8] & kTrackCatalogValueFieldValidMask )
		{
			
			bcopy ( &isrcData[9], isrc, kCDISRCMaxLength );
			isrc[kCDISRCMaxLength] = 0;
			
			status = kIOReturnSuccess;
			
		}
		
		else
		{
			status = kIOReturnNotFound;
		}
		
	}
	
	else
	{
		status = kIOReturnNotFound;
	}
	
	ReleaseSCSITask ( request );

	STATUS_LOG ( ( "%s::%s status = %ld\n", getName ( ),
					__FUNCTION__, ( UInt32 ) status ) );
		
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReadMCN ( CDMCN mcn )
{
	
	IOReturn				status = kIOReturnError;
	IOMemoryDescriptor *	desc;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8					mcnData[kSubChannelDataBufferSize];
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReadMCN called\n" ) );
	
	desc = IOMemoryDescriptor::withAddress ( mcnData, kSubChannelDataBufferSize, kIODirectionIn );
	
	request = GetSCSITask ( );
	
	if ( READ_SUB_CHANNEL (	request,
							desc,
							1,
							1,
							0x02,
							0,
							kSubChannelDataBufferSize,
							0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::ReadMCN malformed command" ) );
	}
	
	desc->release ( );

	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		// Check if we found good data.
		if ( mcnData[8] & kMediaCatalogValueFieldValidMask )
		{
			
			bcopy ( &mcnData[9], mcn, kCDMCNMaxLength );
			mcn[kCDMCNMaxLength] = 0;
			
			status = kIOReturnSuccess;
			
		}
		
		else
		{
			status = kIOReturnNotFound;
		}
		
	}
	
	else
	{
		status = kIOReturnNotFound;
	}
	
	ReleaseSCSITask ( request );
	
	STATUS_LOG ( ( "%s::%s status = %ld\n", getName ( ),
					__FUNCTION__, ( UInt32 ) status ) );
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReadTOC ( IOMemoryDescriptor * buffer )
{
	
	IOReturn				status = kIOReturnError;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReadTOC called, tocMaxSize = %ld\n",
					buffer->getLength ( ) ) );
	
	request = GetSCSITask ( );
	
	if ( READ_TOC_PMA_ATIP (	request,
								buffer,
								1,
								0x02,
								0,
								buffer->getLength ( ),
								0 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::ReadTOC malformed command" ) );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		status = kIOReturnSuccess;
		
		// We have successfully gotten a TOC, check if the driver was able to 
		// determine a block count from READ CAPACITY.  If the media has a reported
		// block count of zero, then this is most likely one of the devices that does
		// not correctly update the capacity information after writing a disc.  Since the
		// BCD conversion check cannot be successfully done without valid capacity
		// data, do neither the check nor the conversion. 
		if ( fMediaBlockCount != 0 )
		{
			
			UInt8			lastSessionNum;
			UInt16			sizeOfTOC;
			UInt32 			index;
			UInt8 *			ptr;
			IOByteCount		bufLength;
			bool			needsBCDtoHexConv = false;
			UInt32			numLBAfromMSF;
			
			ptr = ( UInt8 * ) buffer->getVirtualSegment ( 0, &bufLength );
			
			STATUS_LOG ( ( "bufLength = %ld\n", bufLength ) );
			
			// Make sure the buffer is greater than 4 bytes. If it is not,
			// don't touch the buffer (could cause a kernel panic).
			if ( bufLength > 4 )
			{
				
				// Get the size of the TOC data returned
				sizeOfTOC = *( ( UInt16 * ) &ptr[0] );
				
				// Get the number of the last session on the disc.
				// Since this number will be match to the appropriate
				// data in the returned TOC, the format of this (Hex or BCD)
				// has no impact on its use.		
				lastSessionNum = ptr[3];
				
				// Find the A2 point for the last session
				for ( index = 4; index < buffer->getLength ( ) - 4; index += 11 )
				{
					
					// Check if this Track Descriptor is for the last session
					if ( ptr[index] != lastSessionNum )
					{
						// Not for the last session, go on to the next descriptor.
						continue;
					}
					
					// If we got here, then this Track Descriptor is for the last session,
					// now check to see if it is the A2 point.
					if ( ptr[index + 3] != 0xA2 )
					{
						continue;
					}
					
					// If we got here, then this Track Descriptor is for the last session,
					// and is the A2 point.
					// Now check if the beginning of the lead out is greater than
					// the disc capacity (plus the 2 second leadin or 150 blocks) plus 75 sector
					// tolerance.
					numLBAfromMSF = ( ( ( ptr[index + 8] * 60 ) + ( ptr[index + 9] ) ) * 75 ) + ( ptr[index + 10] );
					
					if ( numLBAfromMSF > ( ( fMediaBlockCount + 150 ) + 75 ) )
					{
						
						needsBCDtoHexConv = true;
						break;
						
					}
					
				}
				
			}
			
			if ( needsBCDtoHexConv == true )
			{
				
				// Convert First/Last session info
				ptr[2] = ConvertBCDToHex ( ptr[2] );
				ptr[3] = ConvertBCDToHex ( ptr[3] );
				ptr = &ptr[4];
				
				// Loop over track descriptors finding the BCD values and change them to hex.
				for ( index = 0; index < buffer->getLength ( ) - 4; index += 11 )
				{
					
					if ( ( ptr[index + 3] == 0xA0 ) || ( ptr[index + 3] == 0xA1 ) )
					{
						
						// Fix the A0 and A1 PMIN values.
						ptr[index + 8] = ConvertBCDToHex ( ptr[index + 8] );
						
					}
					
					else
					{
						
						// Fix the Point value field
						ptr[index + 3] = ConvertBCDToHex ( ptr[index + 3] );
						
						// Fix the Minutes value field
						ptr[index + 8] = ConvertBCDToHex ( ptr[index + 8] );
						
						// Fix the Seconds value field
						ptr[index + 9] = ConvertBCDToHex ( ptr[index + 9] );
						
						// Fix the Frames value field
						ptr[index + 10] = ConvertBCDToHex ( ptr[index + 10] );
						
					}
					
				}
				
			}
			
		}
		
	}
	
	ReleaseSCSITask ( request );
		
	STATUS_LOG ( ( "IOSCSIMultimediaCommandsDevice::ReadTOC status = %d\n", status ) );
	
	return status;
	
}


UInt8
IOSCSIMultimediaCommandsDevice::ConvertBCDToHex ( UInt8 binaryCodedDigit )
{
	
	UInt8	accumulator;
	UInt8	x;
	
	// Divide by 16 (equivalent to >> 4)
	x = ( binaryCodedDigit >> 4 ) & 0x0F;
	if ( x > 9 )
	{
		
		return binaryCodedDigit;
		
	}
	
	accumulator = 10 * x;
	x = binaryCodedDigit & 0x0F;
	if ( x > 9 )
	{
		
		return binaryCodedDigit;
		
	}
	
	accumulator += x;
	
	return accumulator;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::AudioPause ( bool pause )
{
	
	IOReturn				status = kIOReturnUnsupported;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	if ( fSupportedCDFeatures & kCDFeaturesAnalogAudioMask )
	{
		
		request = GetSCSITask ( );
		
		if ( PAUSE_RESUME ( request, !pause, 0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::AudioPause malformed command" ) );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			status = kIOReturnSuccess;
		}
		
		else
		{
			status = kIOReturnError;
		}
		
		ReleaseSCSITask ( request );
				
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::AudioPlay ( CDMSF timeStart, CDMSF timeStop )
{
	
	IOReturn				status = kIOReturnUnsupported;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;

	if ( fSupportedCDFeatures & kCDFeaturesAnalogAudioMask )
	{
		
		request = GetSCSITask ( );
		
		if ( PLAY_AUDIO_MSF ( 	request,
								*( UInt32 * ) &timeStart,
								*( UInt32 * ) &timeStop,
								0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::AudioPlay malformed command" ) );
		}

		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			status = kIOReturnSuccess;
		}
		else
		{
			status = kIOReturnError;
		}

		ReleaseSCSITask ( request );
				
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::AudioScan ( CDMSF timeStart, bool reverse )
{
	
	IOReturn				status = kIOReturnUnsupported;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	if ( fSupportedCDFeatures & kCDFeaturesAnalogAudioMask )
	{
		
		request = GetSCSITask ( );
		
		if ( SCAN ( request,
					reverse,
					0,
					*( UInt32 * ) &timeStart,
					0x01,
					0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::AudioScan malformed command" ) );
		}

		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			status = kIOReturnSuccess;
		}
		
		else
		{
			status = kIOReturnError;
		}

		ReleaseSCSITask ( request );
				
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::AudioStop ( void )
{
	
	IOReturn				status = kIOReturnUnsupported;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	if ( fSupportedCDFeatures & kCDFeaturesAnalogAudioMask )
	{
		
		request = GetSCSITask ( );
		
		if ( STOP_PLAY_SCAN ( request, 0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::AudioStop malformed command" ) );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			status = kIOReturnSuccess;
		}
		
		else
		{
			status = kIOReturnError;
		}
		
		ReleaseSCSITask ( request );
		
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::GetAudioStatus ( CDAudioStatus * status )
{
	
	return kIOReturnUnsupported;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::GetAudioVolume ( UInt8 * leftVolume,
												 UInt8 * rightVolume )
{
	
	IOReturn				status = kIOReturnUnsupported;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	if ( fSupportedCDFeatures & kCDFeaturesAnalogAudioMask )
	{
		
		IOMemoryDescriptor *	bufferDesc;
		UInt8					cdAudioModePageBuffer[24];
		
		bufferDesc = IOMemoryDescriptor::withAddress ( 	cdAudioModePageBuffer,
														24,
														kIODirectionIn );
		
		request = GetSCSITask ( );
		
		if ( MODE_SENSE_10 ( 	request,
								bufferDesc,
								0,
								0,
								0,
								0x0E,
								24,
								0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GetAudioVolume malformed command" ) );
		}
		
		bufferDesc->release ( );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			status = kIOReturnSuccess;
		}
		
		else
		{
			status = kIOReturnError;
		}
		
		ReleaseSCSITask ( request );
		
		if ( status == kIOReturnSuccess )
		{
			
			*leftVolume 	= cdAudioModePageBuffer[17];
			*rightVolume 	= cdAudioModePageBuffer[19];
			
		}
		
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::SetAudioVolume ( UInt8 leftVolume,
												 UInt8 rightVolume )
{
	
	IOReturn				status = kIOReturnUnsupported;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	if ( fSupportedCDFeatures & kCDFeaturesAnalogAudioMask )
	{
		
		IOMemoryDescriptor *	bufferDesc;
		UInt8					cdAudioModePageBuffer[24];
		
		bzero ( cdAudioModePageBuffer, 24 );
		
		bufferDesc = IOMemoryDescriptor::withAddress ( 	cdAudioModePageBuffer,
														24,
														kIODirectionIn );
		
		request = GetSCSITask ( );
		
		if ( MODE_SENSE_10 ( 	request,
								bufferDesc,
								0,
								0,
								0,
								0x0E,
								24,
								0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SetAudioVolume malformed command" ) );
		}
		
		bufferDesc->release ( );
		
		if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
			status = kIOReturnSuccess;
		}
		
		else
		{
			status = kIOReturnError;
		}
		
		if ( status != kIOReturnSuccess )
		{
			goto Exit;
		}
		
		bufferDesc = IOMemoryDescriptor::withAddress ( 	cdAudioModePageBuffer,
														24,
														kIODirectionOut );
		
		cdAudioModePageBuffer[9]	= 0x0E;
		cdAudioModePageBuffer[17] 	= leftVolume;
		cdAudioModePageBuffer[19]	= rightVolume;
		
		if ( MODE_SELECT_10 ( 	request,
								bufferDesc,
								0x01,
								0x00,
								24,
								0 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SetAudioVolume malformed command" ) );
		}
		
		bufferDesc->release ( );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			status = kIOReturnSuccess;
		}
		
		else
		{
			status = kIOReturnError;
		}
		
Exit:
		
		ReleaseSCSITask ( request );
		
	}
	
	return status;
	
}


UInt32
IOSCSIMultimediaCommandsDevice::GetMediaType ( void )
{
	
	return fMediaType;
		
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReportKey ( IOMemoryDescriptor * 	buffer,
											const DVDKeyClass		keyClass,
											const UInt32			lba,
											const UInt8				agid,
											const DVDKeyFormat		keyFormat )
{
	
	IOReturn				status = kIOReturnUnsupported;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	if ( fSupportedDVDFeatures & kDVDFeaturesCSSMask )
	{
		
		request = GetSCSITask ( );
		
		if ( REPORT_KEY ( 	request,
							buffer,
							( keyFormat == 0x04 ) ? lba : 0,
							( buffer != NULL ) ? buffer->getLength ( ) : 0,
							agid,
							keyFormat,
							0x00 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::ReportKey malformed command" ) );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			status = kIOReturnSuccess;
		}
		
		else
		{
			status = kIOReturnError;
		}
		
		ReleaseSCSITask ( request );
		
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::SendKey ( 	IOMemoryDescriptor * 	buffer,
											const DVDKeyClass		keyClass,
											const UInt8				agid,
											const DVDKeyFormat		keyFormat )
{
	
	IOReturn				status = kIOReturnUnsupported;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	if ( fSupportedDVDFeatures & kDVDFeaturesCSSMask )
	{
		
		request = GetSCSITask ( );
		
		if ( SEND_KEY (	request,
						buffer,
						( buffer != NULL ) ? buffer->getLength ( ) : 0,
						agid,
						keyFormat,
						0x00 ) == true )
		{
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, 0 );
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SendKey malformed command" ) );
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			status = kIOReturnSuccess;
		}
		
		else
		{
			status = kIOReturnError;
		}
		
		ReleaseSCSITask ( request );
			
	}
	
	return status;
	
}


IOReturn
IOSCSIMultimediaCommandsDevice::ReadDVDStructure (	IOMemoryDescriptor * 		buffer,
													const UInt32 				length,
													const UInt8					structureFormat,
													const UInt32				logicalBlockAddress,
													const UInt8					layer,
													const UInt8 				agid )
{
	
	IOReturn				status = kIOReturnUnsupported;
	SCSITaskIdentifier		request;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	request = GetSCSITask ( );
	
	if ( READ_DVD_STRUCTURE (	request,
								buffer,
								logicalBlockAddress,
								layer,
								structureFormat,
								length,
								agid,
								0x00 ) == true )
	{
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::ReadDVDStructure malformed command" ) );
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		status = kIOReturnSuccess;
	}
	
	else
	{
		status = kIOReturnError;
	}
	
	ReleaseSCSITask ( request );
	
	return status;
	
}


#pragma mark - 
#pragma mark SCSI MultiMedia Commands Builders


bool
IOSCSIMultimediaCommandsDevice::BLANK (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED, 
						SCSICmdField3Bit 			BLANKING_TYPE, 
						SCSICmdField4Byte 			START_ADDRESS_TRACK_NUMBER, 
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::BLANK invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->BLANK (
													scsiRequest,
													IMMED,
													BLANKING_TYPE,
													START_ADDRESS_TRACK_NUMBER,
													CONTROL );

}


bool
IOSCSIMultimediaCommandsDevice::CLOSE_TRACK_SESSION (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			SESSION,
						SCSICmdField1Bit 			TRACK,
						SCSICmdField2Byte 			TRACK_NUMBER,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::CLOSE_TRACK_SESSION invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->CLOSE_TRACK_SESSION (
								scsiRequest,
								IMMED,
								SESSION,
								TRACK,
								TRACK_NUMBER,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::FORMAT_UNIT (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						IOByteCount					parameterListSize,
						SCSICmdField1Bit 			FMT_DATA,
						SCSICmdField1Bit 			CMP_LIST,
						SCSICmdField3Bit 			FORMAT_CODE,
						SCSICmdField2Byte 			INTERLEAVE_VALUE,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::FORMAT_UNIT invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->FORMAT_UNIT (
								scsiRequest,
								dataBuffer,
								parameterListSize,
								FMT_DATA,
								CMP_LIST,
								FORMAT_CODE,
								INTERLEAVE_VALUE,
								CONTROL );

}


bool
IOSCSIMultimediaCommandsDevice::GET_CONFIGURATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Bit 			RT,
						SCSICmdField2Byte 			STARTING_FEATURE_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GET_CONFIGURATION invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->GET_CONFIGURATION (
								scsiRequest,
								dataBuffer,
								RT,
								STARTING_FEATURE_NUMBER,
								ALLOCATION_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::GET_EVENT_STATUS_NOTIFICATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Byte 			NOTIFICATION_CLASS_REQUEST,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GET_EVENT_STATUS_NOTIFICATION invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->GET_EVENT_STATUS_NOTIFICATION (
								scsiRequest,
								dataBuffer,
								IMMED,
								NOTIFICATION_CLASS_REQUEST,
								ALLOCATION_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::GET_PERFORMANCE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Bit 			TOLERANCE,
						SCSICmdField1Bit 			WRITE,
						SCSICmdField2Bit 			EXCEPT,
						SCSICmdField4Byte 			STARTING_LBA,
						SCSICmdField2Byte 			MAXIMUM_NUMBER_OF_DESCRIPTORS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::GET_PERFORMANCE invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->GET_PERFORMANCE (
								scsiRequest,
								dataBuffer,
								TOLERANCE,
								WRITE,
								EXCEPT,
								STARTING_LBA,
								MAXIMUM_NUMBER_OF_DESCRIPTORS,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::LOAD_UNLOAD_MEDIUM (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			LO_UNLO,
						SCSICmdField1Bit 			START,
						SCSICmdField1Byte 			SLOT,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::LOAD_UNLOAD_MEDIUM invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->LOAD_UNLOAD_MEDIUM (
								scsiRequest,
								IMMED,
								LO_UNLO,
								START,
								SLOT,
								CONTROL );	
}


bool
IOSCSIMultimediaCommandsDevice::MECHANISM_STATUS (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::MECHANISM_STATUS invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->MECHANISM_STATUS (
								scsiRequest,
								dataBuffer,
								ALLOCATION_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::PAUSE_RESUME (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			RESUME,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::PAUSE_RESUME invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->PAUSE_RESUME (
								scsiRequest,
								RESUME,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::PLAY_AUDIO_10 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			PLAY_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::PLAY_AUDIO_10 invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->PLAY_AUDIO_10 (
								scsiRequest,
								RELADR,
								STARTING_LOGICAL_BLOCK_ADDRESS,
								PLAY_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::PLAY_AUDIO_12 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			PLAY_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::PLAY_AUDIO_12 invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->PLAY_AUDIO_12 (
								scsiRequest,
								RELADR,
								STARTING_LOGICAL_BLOCK_ADDRESS,
								PLAY_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::PLAY_AUDIO_MSF (
						SCSITaskIdentifier			request,
						SCSICmdField3Byte 			STARTING_MSF,
						SCSICmdField3Byte 			ENDING_MSF,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::PLAY_AUDIO_MSF invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->PLAY_AUDIO_MSF (
								scsiRequest,
								STARTING_MSF,
								ENDING_MSF,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::PLAY_CD (
						SCSITaskIdentifier			request,
						SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
						SCSICmdField1Bit 			CMSF,
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			PLAY_LENGTH_IN_BLOCKS,
						SCSICmdField1Bit 			SPEED,
						SCSICmdField1Bit 			PORT2,
						SCSICmdField1Bit 			PORT1,
						SCSICmdField1Bit 			COMPOSITE,
						SCSICmdField1Bit 			AUDIO,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::PLAY_CD invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->PLAY_CD (
								scsiRequest,
								EXPECTED_SECTOR_TYPE,
								CMSF,
								STARTING_LOGICAL_BLOCK_ADDRESS,
								PLAY_LENGTH_IN_BLOCKS,
								SPEED,
								PORT2,
								PORT1,
								COMPOSITE,
								AUDIO,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::READ_BUFFER_CAPACITY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_BUFFER_CAPACITY invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_BUFFER_CAPACITY (
								scsiRequest,
								dataBuffer,
								ALLOCATION_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::READ_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	UInt64 		requestedByteCount;
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_10 invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
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
		// buffer is large ebnough to accomodate this request.
		if ( dataBuffer->getLength( ) < requestedByteCount )
		{
			
			return false;
		
		}
	
	}
	
	return GetSCSIBlockCommandObject ( )->READ_10 (
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
IOSCSIMultimediaCommandsDevice::READ_CD (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField3Byte 			TRANSFER_LENGTH,
						SCSICmdField1Bit 			SYNC,
						SCSICmdField2Bit 			HEADER_CODES,
						SCSICmdField1Bit 			USER_DATA,
						SCSICmdField1Bit 			EDC_ECC,
						SCSICmdField2Bit 			ERROR_FIELD,
						SCSICmdField3Bit 			SUBCHANNEL_SELECTION_BITS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_CD invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_CD (
								scsiRequest,
								dataBuffer,
								EXPECTED_SECTOR_TYPE,
								RELADR,
								STARTING_LOGICAL_BLOCK_ADDRESS,
								TRANSFER_LENGTH,
								SYNC,
								HEADER_CODES,
								USER_DATA,
								EDC_ECC,
								ERROR_FIELD,
								SUBCHANNEL_SELECTION_BITS,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::READ_CD_MSF (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
						SCSICmdField3Byte 			STARTING_MSF,
						SCSICmdField3Byte 			ENDING_MSF,
						SCSICmdField1Bit 			SYNC,
						SCSICmdField2Bit 			HEADER_CODES,
						SCSICmdField1Bit 			USER_DATA,
						SCSICmdField1Bit 			EDC_ECC,
						SCSICmdField2Bit 			ERROR_FIELD,
						SCSICmdField3Bit 			SUBCHANNEL_SELECTION_BITS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_CD_MSF invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_CD_MSF (
								scsiRequest,
								dataBuffer,
								EXPECTED_SECTOR_TYPE,
								STARTING_MSF,
								ENDING_MSF,
								SYNC,
								HEADER_CODES,
								USER_DATA,
								EDC_ECC,
								ERROR_FIELD,
								SUBCHANNEL_SELECTION_BITS,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::READ_CAPACITY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Bit 			PMI,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_CAPACITY invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_CAPACITY (
								scsiRequest,
								dataBuffer,
								RELADR,
								LOGICAL_BLOCK_ADDRESS,
								PMI,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::READ_DISC_INFORMATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_DISC_INFORMATION invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_DISC_INFORMATION (
								scsiRequest,
								dataBuffer,
								ALLOCATION_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::READ_DVD_STRUCTURE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Byte 			ADDRESS,
						SCSICmdField1Byte 			LAYER_NUMBER,
						SCSICmdField1Byte 			FORMAT,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField2Bit 			AGID,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_DVD_STRUCTURE invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_DVD_STRUCTURE (
								scsiRequest,
								dataBuffer,
								ADDRESS,
								LAYER_NUMBER,
								FORMAT,
								ALLOCATION_LENGTH,
								AGID,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::READ_FORMAT_CAPACITIES (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_FORMAT_CAPACITIES invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_FORMAT_CAPACITIES (
								scsiRequest,
								dataBuffer,
								ALLOCATION_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::READ_HEADER (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			MSF,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_HEADER invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_HEADER (
								scsiRequest,
								dataBuffer,
								MSF,
								LOGICAL_BLOCK_ADDRESS,
								ALLOCATION_LENGTH,
								CONTROL );
	
}

#ifdef INCLUDE_OBSOLETE_APIS
bool
IOSCSIMultimediaCommandsDevice::READ_MASTER_CUE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte 			SHEET_NUMBER,
						SCSICmdField3Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_MASTER_CUE invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_MASTER_CUE (
								scsiRequest,
								dataBuffer,
								SHEET_NUMBER,
								ALLOCATION_LENGTH,
								CONTROL );

}
#endif	/* INCLUDE_OBSOLETE_APIS */


bool
IOSCSIMultimediaCommandsDevice::READ_SUB_CHANNEL (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			MSF,
						SCSICmdField1Bit 			SUBQ,
						SCSICmdField1Byte 			SUB_CHANNEL_PARAMETER_LIST,
						SCSICmdField1Byte 			TRACK_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_SUB_CHANNEL invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_SUB_CHANNEL (
								scsiRequest,
								dataBuffer,
								MSF,
								SUBQ,
								SUB_CHANNEL_PARAMETER_LIST,
								TRACK_NUMBER,
								ALLOCATION_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::READ_TOC_PMA_ATIP (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			MSF,
						SCSICmdField4Bit 			FORMAT,
						SCSICmdField1Byte			TRACK_SESSION_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_TOC_PMA_ATIP invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_TOC_PMA_ATIP (
								scsiRequest,
								dataBuffer,
								MSF,
								FORMAT,
								TRACK_SESSION_NUMBER,
								ALLOCATION_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::READ_TRACK_INFORMATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Bit 			ADDRESS_NUMBER_TYPE,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::READ_TRACK_INFORMATION invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->READ_TRACK_INFORMATION (
								scsiRequest,
								dataBuffer,
								ADDRESS_NUMBER_TYPE,
								LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
								ALLOCATION_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::REPAIR_TRACK (
						SCSITaskIdentifier			request,
						SCSICmdField2Byte 			TRACK_NUMBER,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::REPAIR_TRACK invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->REPAIR_TRACK (
								scsiRequest,
								TRACK_NUMBER,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::REPORT_KEY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField2Bit 			AGID,
						SCSICmdField6Bit 			KEY_FORMAT,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::REPORT_KEY invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->REPORT_KEY (
								scsiRequest,
								dataBuffer,
								LOGICAL_BLOCK_ADDRESS,
								ALLOCATION_LENGTH,
								AGID,
								KEY_FORMAT,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::RESERVE_TRACK (
						SCSITaskIdentifier			request,
						SCSICmdField4Byte			RESERVATION_SIZE,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::RESERVE_TRACK invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->RESERVE_TRACK (
								scsiRequest,
								RESERVATION_SIZE,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::SCAN (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			DIRECT,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte			SCAN_STARTING_ADDRESS_FIELD,
						SCSICmdField2Bit 			TYPE,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SCAN invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->SCAN (
								scsiRequest,
								DIRECT,
								RELADR,
								SCAN_STARTING_ADDRESS_FIELD,
								TYPE,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::SEND_CUE_SHEET (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Byte			CUE_SHEET_SIZE,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SEND_CUE_SHEET invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->SEND_CUE_SHEET (
								scsiRequest,
								dataBuffer,
								CUE_SHEET_SIZE,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::SEND_DVD_STRUCTURE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte			FORMAT,
						SCSICmdField2Byte 			STRUCTURE_DATA_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SEND_DVD_STRUCTURE invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->SEND_DVD_STRUCTURE (
								scsiRequest,
								dataBuffer,
								FORMAT,
								STRUCTURE_DATA_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::SEND_EVENT (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SEND_EVENT invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->SEND_EVENT (
								scsiRequest,
								dataBuffer,
								IMMED,
								PARAMETER_LIST_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::SEND_KEY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField2Bit 			AGID,
						SCSICmdField6Bit 			KEY_FORMAT,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SEND_KEY invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->SEND_KEY (
								scsiRequest,
								dataBuffer,
								PARAMETER_LIST_LENGTH,
								AGID,
								KEY_FORMAT,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::SEND_OPC_INFORMATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			DO_OPC,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SEND_OPC_INFORMATION invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->SEND_OPC_INFORMATION (
								scsiRequest,
								dataBuffer,
								DO_OPC,
								PARAMETER_LIST_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::SET_CD_SPEED (
						SCSITaskIdentifier			request,
						SCSICmdField2Byte 			LOGICAL_UNIT_READ_SPEED,
						SCSICmdField2Byte 			LOGICAL_UNIT_WRITE_SPEED,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SET_CD_SPEED invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->SET_CD_SPEED (
								scsiRequest,
								LOGICAL_UNIT_READ_SPEED,
								LOGICAL_UNIT_WRITE_SPEED,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::SET_READ_AHEAD (
						SCSITaskIdentifier			request,
						SCSICmdField4Byte 			TRIGGER_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			READ_AHEAD_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SET_READ_AHEAD invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->SET_READ_AHEAD (
								scsiRequest,
								TRIGGER_LOGICAL_BLOCK_ADDRESS,
								READ_AHEAD_LOGICAL_BLOCK_ADDRESS,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::SET_STREAMING (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SET_STREAMING invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->SET_STREAMING (
								scsiRequest,
								dataBuffer,
								PARAMETER_LIST_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::START_STOP_UNIT ( 
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField4Bit 			POWER_CONDITIONS,
						SCSICmdField1Bit 			LOEJ,
						SCSICmdField1Bit 			START,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::START_STOP_UNIT invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIBlockCommandObject ( )->START_STOP_UNIT (
											scsiRequest,
											IMMED,
											POWER_CONDITIONS,
											LOEJ,
											START,
											CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::STOP_PLAY_SCAN (
						SCSITaskIdentifier			request,
						SCSICmdField1Byte 		CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::STOP_PLAY_SCAN invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->STOP_PLAY_SCAN (
								scsiRequest,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::SYNCHRONIZE_CACHE (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::SYNCHRONIZE_CACHE invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->SYNCHRONIZE_CACHE (
													scsiRequest,
													IMMED,
													RELADR,
													LOGICAL_BLOCK_ADDRESS,
													NUMBER_OF_BLOCKS,
													CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::WRITE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::WRITE_10 invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->WRITE_10 (
								scsiRequest,
								dataBuffer,
								blockSize,
								DPO,
								FUA,
								RELADR,
								LOGICAL_BLOCK_ADDRESS,
								TRANSFER_LENGTH,
								CONTROL );
	
}


bool
IOSCSIMultimediaCommandsDevice::WRITE_AND_VERIFY_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			BYT_CHK,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
		
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIMultimediaCommandsDevice::WRITE_AND_VERIFY_10 invalid SCSITaskIdentifier.\n" ) );
		ERROR_LOG ( ( "%s::%s invalid SCSITaskIdentifier.\n", getName ( ), __FUNCTION__ ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIMultimediaCommandObject ( )->WRITE_AND_VERIFY_10 (
								scsiRequest,
								dataBuffer,
								blockSize,
								DPO,
								BYT_CHK,
								RELADR,
								LOGICAL_BLOCK_ADDRESS,
								TRANSFER_LENGTH,
								CONTROL );
	
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 1 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 2 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 3 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 4 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 5 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 6 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 7 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 8 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 9 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 10 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 11 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 12 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 13 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 14 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 15 );
OSMetaClassDefineReservedUnused( IOSCSIMultimediaCommandsDevice, 16 );
