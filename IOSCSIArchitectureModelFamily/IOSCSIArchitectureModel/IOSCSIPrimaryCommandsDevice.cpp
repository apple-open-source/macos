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
#include <IOKit/IOMessage.h>
#include <IOKit/scsi-commands/SCSICommandOperationCodes.h>
#include "IOSCSIPrimaryCommandsDevice.h"

// For debugging, set SCSI_SPC_DEVICE_DEBUGGING_LEVEL to one
// of the following values:
//		0	No debugging 	(GM release level)
// 		1 	PANIC_NOW only
//		2	PANIC_NOW and ERROR_LOG
//		3	PANIC_NOW, ERROR_LOG and STATUS_LOG

#if ( SCSI_SPC_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_SPC_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_SPC_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


enum
{
	kSecondsInAMinute = 60
};

#define	kPowerConditionsModePage					0x1A
#define	kPowerConditionsModePageSize				12
#define kIOPropertyPowerConditionsSupportedKey		"PowerConditionsSupported"


#define super IOSCSIProtocolInterface
OSDefineMetaClass ( IOSCSIPrimaryCommandsDevice, IOSCSIProtocolInterface );
OSDefineAbstractStructors ( IOSCSIPrimaryCommandsDevice, IOSCSIProtocolInterface );


bool 
IOSCSIPrimaryCommandsDevice::init ( OSDictionary * propTable )
{
	
	STATUS_LOG ( ( "IOSCSIPrimaryCommandsDevice init called.\n" ) );
	
	if ( super::init ( propTable ) == false )
	{
		
		ERROR_LOG ( ( "IOSCSIPrimaryCommandsDevice super didn't like the property table\n" ) );
		return false;
		
	}
	
	return true;
	
}


bool 
IOSCSIPrimaryCommandsDevice::start ( IOService * provider )
{
	
	OSString *	string;
	
	STATUS_LOG ( ( "%s: start called.\n", getName ( ) ) );
	
	// Call our super class' start routine so that all inherited
	// behavior is initialized.	
	if ( !super::start ( provider ) )
	{
		
		ERROR_LOG ( ( "%s: super didn't want to start.\n", getName ( ) ) );
		return false;
		
	}
	
	fDeviceAccessEnabled 			= false;
	fDeviceAccessSuspended			= false;
	fDeviceSupportsPowerConditions 	= false;
	fNumCommandsOutstanding 		= 0;

	fProtocolDriver = OSDynamicCast ( IOSCSIProtocolInterface, provider );
	
	if ( fProtocolDriver == NULL )
	{
		
		ERROR_LOG ( ( "%s: provider is not a IOSCSIProtocolInterface\n", getName ( ) ) );
		return false;
		
	}
	
	fDeviceCharacteristicsDictionary = OSDictionary::withCapacity ( 1 );
	if ( fDeviceCharacteristicsDictionary == NULL )
	{
		
		ERROR_LOG ( ( "%s: couldn't device characteristics dictionary.\n", getName ( ) ) );
		return false;
		
	}
	
	string = ( OSString * ) GetProtocolDriver ( )->getProperty ( kIOPropertySCSIVendorIdentification );	
	fDeviceCharacteristicsDictionary->setObject ( kIOPropertyVendorNameKey, string );
	
	string = ( OSString * ) GetProtocolDriver ( )->getProperty ( kIOPropertySCSIProductIdentification );	
	fDeviceCharacteristicsDictionary->setObject ( kIOPropertyProductNameKey, string );
	
	string = ( OSString * ) GetProtocolDriver ( )->getProperty ( kIOPropertySCSIProductRevisionLevel );	
	fDeviceCharacteristicsDictionary->setObject ( kIOPropertyProductRevisionLevelKey, string );
	
	// Now create the required command sets used the class
	if ( CreateCommandSetObjects ( ) == false )
	{
		
		ERROR_LOG ( ( "%s: couldn't create command set objects.\n", getName ( ) ) );
		return false;
		
	}
	
	// open out provider so it can't slip out from under us
	GetProtocolDriver ( )->open ( this );
	
	STATUS_LOG ( ( "%s: Check for the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
	
	// Check if the personality for this device specifies device characteristics.
	if ( ( GetProtocolDriver ( )->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) == NULL )
	{
		
		STATUS_LOG ( ( "%s: No SCSI Device Characteristics property, set defaults.\n", getName ( ) ) );
		fDefaultInquiryCount = 0;
		
	}
	
	else
	{
		
		OSDictionary * characterDict;
		
		STATUS_LOG ( ( "%s: Get the SCSI Device Characteristics property from provider.\n", getName ( ) ) );
		characterDict = OSDynamicCast ( OSDictionary, ( GetProtocolDriver ( )->getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) ) );
		
		STATUS_LOG ( ( "%s: set this SCSI Device Characteristics property.\n", getName ( ) ) );
		setProperty ( kIOPropertySCSIDeviceCharacteristicsKey, characterDict );

		// Check if the personality for this device specifies a default Inquiry count
		STATUS_LOG ( ( "%s: check for the Inquiry Length property.\n", getName ( ) ) );
		
		if ( characterDict->getObject ( kIOPropertySCSIInquiryLengthKey ) == NULL )
		{
			
			STATUS_LOG ( ( "%s: No Inquiry Length property, use default.\n", getName ( ) ) );
			fDefaultInquiryCount = 0;
			
		}
		
		else
		{
			
			OSNumber *	defaultInquiry;
			
			STATUS_LOG ( ( "%s: Get Inquiry Length property.\n", getName ( ) ) );
			defaultInquiry = OSDynamicCast ( OSNumber, characterDict->getObject ( kIOPropertySCSIInquiryLengthKey ) );
			
			// This device has a default Inquiry length, use that.
			fDefaultInquiryCount = defaultInquiry->unsigned32BitValue ( );
			
		}
		
	}
	
	fProtocolAccessEnabled = true;
		
	if ( InitializeDeviceSupport ( ) == false )
	{
		
		ERROR_LOG ( ( "%s: InitializeDeviceSupport failed\n", getName ( ) ) );
		return false;
		
	}
	
	fDeviceAccessEnabled = true;
	StartDeviceSupport ( );
	
	return true;
	
}


void 
IOSCSIPrimaryCommandsDevice::stop ( IOService * provider )
{
	
	STATUS_LOG ( ( "%s: stop called.\n", getName ( ) ) );
	
	fProtocolAccessEnabled = false;
	
	// Put some protection from having stop called multiple times
	// GetProtocolDriver is called all over the place in this object
	if ( fDeviceAccessEnabled == true )
	{
		
		fDeviceAccessEnabled = false;
		StopDeviceSupport ( );
		
	}
	

	if ( fProtocolDriver && ( fProtocolDriver == provider ) )
	{
		
		fProtocolDriver->close( this );
		fProtocolDriver = NULL;
		
		super::stop ( provider );
		
	}
	
}


void
IOSCSIPrimaryCommandsDevice::free ( void )
{
	
	// Free any command set objects created by
	// CreateCommandSetObjects()
	FreeCommandSetObjects ( );
	
	// Make sure to call our super
	super::free ( );
	
}


IOReturn
IOSCSIPrimaryCommandsDevice::message (	UInt32 		type,
										IOService *	nub,
										void *		arg )
{
	
	IOReturn status;
	
	switch ( type )
	{
		
		// The device is no longer attached to the bus,
		// close provider so message will continue
		// to propagate up the driver stack.
		case kIOMessageServiceIsRequestingClose:
		{
			
			STATUS_LOG ( ( "%s: kIOMessageServiceIsRequestingClose Received\n", getName ( ) ) );
			
			fProtocolAccessEnabled = false;
			
			if ( fDeviceAccessEnabled == true )
			{
				
				fDeviceAccessEnabled = false;
				StopDeviceSupport ( );
				
			}
			
			TerminateDeviceSupport ( );
			
			if ( fProtocolDriver != NULL )
			{
				fProtocolDriver->close ( this );
			}
			
			status = kIOReturnSuccess;
			
		}
		break;

		case kSCSIServicesNotification_Suspend:
		{
			
			ERROR_LOG ( ( "type = kSCSIServicesNotification_Suspend, nub = %p\n", nub ) );
			fDeviceAccessSuspended = true;
			SuspendDeviceSupport ( );
			status = kIOReturnSuccess;
			
		}
		break;
		
		case kSCSIServicesNotification_Resume:
		{
			
			ERROR_LOG ( ( "type = kSCSIServicesNotification_Resume, nub = %p\n", nub ) );
			fDeviceAccessSuspended = false;
			ResumeDeviceSupport ( );
			status = kIOReturnSuccess;
			
		}
		break;

		case kSCSIProtocolNotification_VerifyDeviceState:
		{
			
			STATUS_LOG ( ("%s: kSCSIProtocolNotification_VerifyDeviceState Received\n", getName ( ) ) );
			status = VerifyDeviceState ( );
			
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


IOReturn
IOSCSIPrimaryCommandsDevice::VerifyDeviceState ( void )
{
	return kIOReturnSuccess;
}


bool
IOSCSIPrimaryCommandsDevice::IsProtocolAccessEnabled ( void )
{
	return fProtocolAccessEnabled;
}


bool
IOSCSIPrimaryCommandsDevice::IsDeviceAccessEnabled ( void )
{
	return fDeviceAccessEnabled;
}


bool
IOSCSIPrimaryCommandsDevice::IsDeviceAccessSuspended ( void )
{
	return fDeviceAccessSuspended;
}


#pragma mark -
#pragma mark Power Management Methods


IOReturn
IOSCSIPrimaryCommandsDevice::setAggressiveness ( UInt32 type, UInt32 minutes )
{
	
	UInt32	numStateTransitions = 0;
		
	STATUS_LOG ( ( "IOSCSIPrimaryCommandsDevice::setAggressiveness called\n" ) );
	
	if ( type == kPMMinutesToSpinDown )
	{
		
		STATUS_LOG ( ( "IOSCSIPrimaryCommandsDevice: setting idle timer to %ld min\n", minutes ) );
		
		numStateTransitions = GetNumberOfPowerStateTransitions ( );
		if ( numStateTransitions != 0 )
		{
			
			// Set the idle timer based on number of power state transitions
			setIdleTimerPeriod ( minutes * ( kSecondsInAMinute / numStateTransitions ) );
			
		}
		
		else
		{
			
			// The device has requested no transitions, don't do any
			// (except System Sleep, which is unavoidable).
			setIdleTimerPeriod ( 0 );
			
		}
		
	}
	
	return ( super::setAggressiveness ( type, minutes ) );
	
}


bool
IOSCSIPrimaryCommandsDevice::ClearPowerOnReset ( void )
{
	
	SCSI_Sense_Data				senseBuffer;
	IOMemoryDescriptor *		bufferDesc;
	SCSITaskIdentifier			request;
	bool						driveReady 		= false;
	bool						result 			= true;
	SCSIServiceResponse 		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress( ( void * ) &senseBuffer,
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
			PANIC_NOW ( ( "IOSCSIPrimaryCommandsDevice::ClearPowerOnReset malformed command" ) );
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
						PANIC_NOW ( ( "IOSCSIPrimaryCommandsDevice::ClearPowerOnReset REQUEST_SENSE malformed command" ) );
					}
					
					if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
					{
						
						validSense = true;
						
					}
					
					else
					{
						IOSleep ( 200 );
					}
					
				}
				
				if ( validSense == true )
				{
					
					// Make sure we don't get a Unit Attention with power on reset qualifier or
					// medium changed qualifier
					
					if ( ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_UNIT_ATTENTION  ) && 
							( senseBuffer.ADDITIONAL_SENSE_CODE == 0x29 ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) ) ||
						 ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_UNIT_ATTENTION  ) && 
							( senseBuffer.ADDITIONAL_SENSE_CODE == 0x28 ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) ) )
					{
						
						STATUS_LOG ( ( "%s::drive NOT READY\n", getName ( ) ) );
						driveReady = false;
						IOSleep ( 200 );
						
					}
					
					else
					{
						
						driveReady = true;
						STATUS_LOG ( ( "%s::drive READY\n", getName ( ) ) );
						
					}
					
					STATUS_LOG ( ( "sense data: %01x, %02x, %02x\n",
								( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ),
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


void
IOSCSIPrimaryCommandsDevice::CheckPowerConditionsModePage ( void )
{
	
	SCSI_Sense_Data				senseBuffer;
	IOMemoryDescriptor *		bufferDesc;
	SCSITaskIdentifier			request;
	SCSIServiceResponse 		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8						buffer[kPowerConditionsModePageSize];
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress (  ( void * ) buffer,
													kPowerConditionsModePageSize,
													kIODirectionIn );
	
	request = GetSCSITask ( );
	
	if ( MODE_SENSE_10 ( request,
						 bufferDesc,
						 0x00,
						 0x00,
						 0x00,
						 kPowerConditionsModePage,
						 kPowerConditionsModePageSize,
						 0x00 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, 0 );
		
	}
	
	else
	{
		PANIC_NOW ( ( "IOSCSIPrimaryCommandsDevice::CheckPowerConditionsModePage malformed command" ) );
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		bool validSense = false;
		
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD )
		{
			
			if ( ( buffer[8] & 0x3F ) == kPowerConditionsModePage )
			{
				
				STATUS_LOG ( ( "Device supports power conditions page\n" ) );
				
				// This device supports the power conditions mode page, so set
				// our flag to true
				fDeviceSupportsPowerConditions = true;
				
			}
			
		}
		
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			STATUS_LOG ( ( "Device gave check condition to power conditions page\n" ) );
			
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
					PANIC_NOW ( ( "IOSCSIPrimaryCommandsDevice::CheckPowerConditionsModePage malformed command" ) );
				}
				
				if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
				{
					
					validSense = true;
					
				}
				
			}
			
			if ( validSense == true )
			{
				
				STATUS_LOG ( ( "sense data: %01x, %02x, %02x\n",
						( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ),
						senseBuffer.ADDITIONAL_SENSE_CODE,
						senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
				
			}
			
		}
		
	}
	
	bufferDesc->release ( );
	ReleaseSCSITask ( request );
	
	#if ( SCSI_SPC_DEVICE_DEBUGGING_LEVEL > 0 )
		setProperty ( kIOPropertyPowerConditionsSupportedKey, fDeviceSupportsPowerConditions );
	#endif /* SCSI_SPC_DEVICE_DEBUGGING_LEVEL */
	
}

//---------------------------------------------------------------------------
// ¥ IncrementOutstandingCommandsCount - Call to increment outstanding command								
//---------------------------------------------------------------------------

void
IOSCSIPrimaryCommandsDevice::IncrementOutstandingCommandsCount ( void )
{
	
	fCommandGate->runAction ( ( IOCommandGate::Action )
							&IOSCSIPrimaryCommandsDevice::sIncrementOutstandingCommandsCount );
	
}


//---------------------------------------------------------------------------
// ¥ sIncrementOutstandingCommandsCount - C->C++ glue.
//---------------------------------------------------------------------------

void
IOSCSIPrimaryCommandsDevice::sIncrementOutstandingCommandsCount ( IOSCSIPrimaryCommandsDevice * self )
{
	
	self->HandleIncrementOutstandingCommandsCount ( );
	
}


//---------------------------------------------------------------------------
// ¥ HandleIncrementOutstandingCommandsCount -
//---------------------------------------------------------------------------

void
IOSCSIPrimaryCommandsDevice::HandleIncrementOutstandingCommandsCount ( void )
{
	
	STATUS_LOG ( ( "IOSCSIPrimaryCommandsDevice::%s called\n", __FUNCTION__ ) );	
	fNumCommandsOutstanding++;	
	
}


#pragma mark -
#pragma mark SCSI Task Get and Release


SCSITaskIdentifier 
IOSCSIPrimaryCommandsDevice::GetSCSITask ( void )
{
	
	SCSITask	* newTask = new SCSITask;
	
	newTask->SetTaskOwner ( this );

	// thread safe increment outstanding command count
	IncrementOutstandingCommandsCount ();
	
	// Make sure the object is not removed if there is a pending
	// command.
	retain ( );
	
	return ( SCSITaskIdentifier ) newTask;
	
}


void 
IOSCSIPrimaryCommandsDevice::ReleaseSCSITask ( SCSITaskIdentifier request )
{
	
	if ( request != NULL )
	{
		
		// decrement outstanding command count
		fNumCommandsOutstanding--;
	
		request->release ( );
		
		// Since the command has been released, let go of the retain on this
		// object.
		release ( );
		
	}
	
}


#pragma mark -
#pragma mark Supporting Object Accessor Methods


IOSCSIProtocolInterface *
IOSCSIPrimaryCommandsDevice::GetProtocolDriver ( void )
{
	
	STATUS_LOG ( ( "%s: GetProtocolDriver called.\n", getName ( ) ) );
	return fProtocolDriver;
	
}


SCSIPrimaryCommands	*
IOSCSIPrimaryCommandsDevice::GetSCSIPrimaryCommandObject ( void )
{
	
	STATUS_LOG ( ( "%s: getSCSIPrimaryCommandObject called.\n", getName ( ) ) );
	return fSCSIPrimaryCommandObject;
	
}


bool 
IOSCSIPrimaryCommandsDevice::CreateCommandSetObjects ( void )
{
	
	STATUS_LOG ( ( "%s: CreateCommandSetObjects called.\n", getName ( ) ) );
	
	fSCSIPrimaryCommandObject = SCSIPrimaryCommands::CreateSCSIPrimaryCommandObject ( );
	if ( fSCSIPrimaryCommandObject == NULL )
	{
		
		ERROR_LOG ( ( "%s: Could not allocate an SPC object\n", getName ( ) ) );
		return false;
		
	}
	
	return true;
	
}


void
IOSCSIPrimaryCommandsDevice::FreeCommandSetObjects ( void )
{
	
	STATUS_LOG ( ( "%s: FreeCommandSetObjects called.\n", getName ( ) ) );
	
	if ( fSCSIPrimaryCommandObject )
	{
		
		fSCSIPrimaryCommandObject->release( );
		fSCSIPrimaryCommandObject = NULL;
		
	}
	
}


#pragma mark -
#pragma mark Task Execution Support Methods


void
IOSCSIPrimaryCommandsDevice::TaskCallback ( SCSITaskIdentifier completedTask )
{
	
	SCSIServiceResponse 	serviceResponse;
	SCSITask *				scsiRequest;
	IOSyncer *				fSyncLock;
	
	STATUS_LOG ( ( "IOSCSIPrimaryCommandsDevice::TaskCallback called\n.") );
		
	scsiRequest = OSDynamicCast ( SCSITask, completedTask );
	if ( scsiRequest == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIPrimaryCommandsDevice::TaskCallback scsiRequest==NULL." ) );
		ERROR_LOG ( ( "IOSCSIPrimaryCommandsDevice::TaskCallback scsiRequest==NULL." ) );
		return;
		
	}
	
	fSyncLock = ( IOSyncer * ) scsiRequest->GetApplicationLayerReference ( );
	serviceResponse = scsiRequest->GetServiceResponse ( );
	fSyncLock->signal ( serviceResponse, false );
	
}


SCSIServiceResponse 
IOSCSIPrimaryCommandsDevice::SendCommand ( SCSITaskIdentifier request, UInt32 timeoutDuration )
{
	
	SCSIServiceResponse 	serviceResponse;
	IOSyncer *				fSyncLock;
	
	if ( IsProtocolAccessEnabled ( ) == false )
	{
		
		SetServiceResponse ( request, kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE );
		
		// Save that status into the Task object.
		SetTaskStatus ( request, kSCSITaskStatus_No_Status );
		
		return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		
	}

	fSyncLock = IOSyncer::create ( false );
	if ( fSyncLock == NULL )
	{
		
		PANIC_NOW ( ( "IOSCSIPrimaryCommandsDevice::SendCommand Allocate fSyncLock failed." ) );
		ERROR_LOG ( ( "IOSCSIPrimaryCommandsDevice::SendCommand Allocate fSyncLock failed." ) );
		
		SetServiceResponse ( request, kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE );
		
		// Save that status into the Task object.
		SetTaskStatus ( request, kSCSITaskStatus_No_Status );
		
		return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		
	}
	
	fSyncLock->signal ( kIOReturnSuccess, false );
	
	SetTimeoutDuration ( request, timeoutDuration );
	SetTaskCompletionCallback ( request, &this->TaskCallback );
	SetApplicationLayerReference ( request, ( void * ) fSyncLock );
	
	SetAutosenseCommand ( request, kSCSICmd_REQUEST_SENSE, 0x00, 0x00, 0x00, sizeof ( SCSI_Sense_Data ), 0x00 );
	
	STATUS_LOG ( ( "%s:SendCommand Reinit the syncer.\n", getName ( ) ) );
	fSyncLock->reinit ( );
	
	STATUS_LOG ( ( "%s:SendCommand Execute the command.\n", getName ( ) ) );
	GetProtocolDriver ( )->ExecuteCommand ( request );
	
	// Wait for the completion routine to get called
	serviceResponse = ( SCSIServiceResponse) fSyncLock->wait ( false );
	fSyncLock->release ( );
	
	STATUS_LOG ( ( "%s:SendCommand return the service response.\n", getName ( ) ) );
	return serviceResponse;
	
}


void 
IOSCSIPrimaryCommandsDevice::SendCommand ( 
						SCSITaskIdentifier	request,
						UInt32 				timeoutDuration,
						SCSITaskCompletion 	taskCompletion )
{
	
	STATUS_LOG ( ( "%s: SendCommand called.\n", getName ( ) ) );
	
	if ( IsProtocolAccessEnabled ( ) == false )
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
	
	SetAutosenseCommand ( request, kSCSICmd_REQUEST_SENSE, 0x00, 0x00, 0x00, sizeof ( SCSI_Sense_Data ), 0x00 );
	GetProtocolDriver ( )->ExecuteCommand ( request );
	
}


#pragma mark -
#pragma mark SCSI Task Field Accessors


// ---- Utility methods for accessing SCSITask attributes ----
bool
IOSCSIPrimaryCommandsDevice::SetTaskAttribute ( SCSITaskIdentifier request, SCSITaskAttribute newAttribute )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetTaskAttribute ( newAttribute );
	
}


SCSITaskAttribute
IOSCSIPrimaryCommandsDevice::GetTaskAttribute ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetTaskAttribute ( );
	
}


bool
IOSCSIPrimaryCommandsDevice::SetTaskState ( SCSITaskIdentifier request,
											SCSITaskState newTaskState )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetTaskState ( newTaskState );
	
}


SCSITaskState
IOSCSIPrimaryCommandsDevice::GetTaskState ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;

	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetTaskState( );
	
}


bool
IOSCSIPrimaryCommandsDevice::SetTaskStatus ( SCSITaskIdentifier request,
											 SCSITaskStatus newStatus )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetTaskStatus ( newStatus );
	
}


SCSITaskStatus
IOSCSIPrimaryCommandsDevice::GetTaskStatus ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetTaskStatus ( );
	
}


// Get the control information for the transfer, including
// the transfer direction and the number of bytes to transfer.
bool
IOSCSIPrimaryCommandsDevice::SetDataTransferDirection ( SCSITaskIdentifier request,
														UInt8 newDirection )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetDataTransferDirection ( newDirection );
	
}


// Get the control information for the transfer, including
// the transfer direction and the number of bytes to transfer.
UInt8
IOSCSIPrimaryCommandsDevice::GetDataTransferDirection ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetDataTransferDirection ( );
	
}


bool
IOSCSIPrimaryCommandsDevice::SetRequestedDataTransferCount ( SCSITaskIdentifier request,
															 UInt64 newRequestedCount )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetRequestedDataTransferCount ( newRequestedCount );
	
}


UInt64
IOSCSIPrimaryCommandsDevice::GetRequestedDataTransferCount ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetRequestedDataTransferCount ( );
	
}


bool
IOSCSIPrimaryCommandsDevice::SetRealizedDataTransferCount ( SCSITaskIdentifier request,
															UInt64 newRealizedDataCount )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetRealizedDataTransferCount ( newRealizedDataCount );
	
}


UInt64
IOSCSIPrimaryCommandsDevice::GetRealizedDataTransferCount ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetRealizedDataTransferCount ( );
	
}


bool
IOSCSIPrimaryCommandsDevice::SetDataBuffer ( SCSITaskIdentifier request,
											 IOMemoryDescriptor * newBuffer )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetDataBuffer ( newBuffer );
	
}


IOMemoryDescriptor *
IOSCSIPrimaryCommandsDevice::GetDataBuffer ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetDataBuffer ( );
	
}


bool
IOSCSIPrimaryCommandsDevice::SetTimeoutDuration ( SCSITaskIdentifier request,
												  UInt32 newTimeout )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetTimeoutDuration ( newTimeout );
	
}


UInt32
IOSCSIPrimaryCommandsDevice::GetTimeoutDuration ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetTimeoutDuration ( );
	
}


bool
IOSCSIPrimaryCommandsDevice::SetTaskCompletionCallback ( 
										SCSITaskIdentifier 		request,
										SCSITaskCompletion 		newCallback )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetTaskCompletionCallback ( newCallback );
	
}


void
IOSCSIPrimaryCommandsDevice::TaskCompletedNotification (  
										SCSITaskIdentifier 		request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->TaskCompletedNotification ( );
	
}


bool
IOSCSIPrimaryCommandsDevice::SetServiceResponse ( 
										SCSITaskIdentifier 		request,
										SCSIServiceResponse 	serviceResponse )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetServiceResponse ( serviceResponse );
	
}


SCSIServiceResponse
IOSCSIPrimaryCommandsDevice::GetServiceResponse ( 
										SCSITaskIdentifier 		request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetServiceResponse ( );
	
}


bool
IOSCSIPrimaryCommandsDevice::SetAutosenseCommand (
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
IOSCSIPrimaryCommandsDevice::GetAutoSenseData ( SCSITaskIdentifier request,
												SCSI_Sense_Data * senseData )
{
	
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetAutoSenseData ( senseData );
	
}


bool
IOSCSIPrimaryCommandsDevice::SetApplicationLayerReference ( SCSITaskIdentifier request,
															void * newReferenceValue )
{
	
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetApplicationLayerReference ( newReferenceValue );
	
}


void *
IOSCSIPrimaryCommandsDevice::GetApplicationLayerReference ( SCSITaskIdentifier request )
{
	
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetApplicationLayerReference ( );
	
}


#pragma mark -
#pragma mark Device Information Retrieval Methods


char *
IOSCSIPrimaryCommandsDevice::GetVendorString ( void )
{
	
	OSString *		vendorString;
	
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	
	vendorString = ( OSString * ) GetProtocolDriver ( )->getProperty ( kIOPropertySCSIVendorIdentification );
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
IOSCSIPrimaryCommandsDevice::GetProductString ( void )
{
	
	OSString *		productString;
	
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	
	productString = ( OSString * ) GetProtocolDriver ( )->getProperty ( kIOPropertySCSIProductIdentification );
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
IOSCSIPrimaryCommandsDevice::GetRevisionString ( void )
{
	
	OSString *		revisionString;
	
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	
	revisionString = ( OSString * ) GetProtocolDriver ( )->getProperty ( kIOPropertySCSIProductRevisionLevel );
	
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
IOSCSIPrimaryCommandsDevice::GetProtocolCharacteristicsDictionary ( void )
{
	
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	return ( OSDictionary * ) GetProtocolDriver ( )->getProperty ( kIOPropertyProtocolCharacteristicsKey );
	
}


OSDictionary *
IOSCSIPrimaryCommandsDevice::GetDeviceCharacteristicsDictionary ( void )
{

	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );	
	return fDeviceCharacteristicsDictionary;
	
}


#pragma mark -
#pragma mark SCSI Protocol Interface Implementations


// The ExecuteCommand method will take a SCSI Task and transport
// it across the physical wire(s) to the device
void
IOSCSIPrimaryCommandsDevice::ExecuteCommand ( SCSITaskIdentifier request )
{
	GetProtocolDriver ( )->ExecuteCommand ( request );
}
	
// The AbortCommand method will abort the indicated SCSI Task,
// if it is possible and the task has not already completed.
SCSIServiceResponse
IOSCSIPrimaryCommandsDevice::AbortCommand ( SCSITaskIdentifier request )
{
	return GetProtocolDriver ( )->AbortCommand ( request );
}
	
// The IsProtocolServiceSupported will return true if the specified
// feature is supported by the protocol layer.  If the service has a value that must be
// returned, it will be returned in the serviceValue output parameter.
bool
IOSCSIPrimaryCommandsDevice::IsProtocolServiceSupported ( SCSIProtocolFeature feature,
														  void * serviceValue )
{
	return GetProtocolDriver ( )->IsProtocolServiceSupported ( feature, serviceValue );
}


// The HandleProtocolServiceFeature will return true if the specified feature could
// be done by the protocol layer.  If the service has a value that must be
// sent, it will be sent in the serviceValue input parameter.
bool
IOSCSIPrimaryCommandsDevice::HandleProtocolServiceFeature ( SCSIProtocolFeature feature,
															void * serviceValue )
{
	return GetProtocolDriver ( )->HandleProtocolServiceFeature ( feature, serviceValue );
}


#pragma mark -
#pragma mark SCSI Primary Commands Builders


// SCSI Primary command implementations
bool 	
IOSCSIPrimaryCommandsDevice::CHANGE_DEFINITION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			SAVE,
						SCSICmdField7Bit 			DEFINITION_PARAMETER,
						SCSICmdField1Byte 			PARAMETER_DATA_LENGTH,
		 				SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: CHANGE_DEFINITION called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}
	
	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->CHANGE_DEFINITION (
											scsiRequest,
											dataBuffer,
											SAVE,
											DEFINITION_PARAMETER,
											PARAMETER_DATA_LENGTH,
											CONTROL );
	
}


bool	
IOSCSIPrimaryCommandsDevice::COMPARE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
 						SCSICmdField1Bit 			PAD,
						SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: COMPARE called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->COMPARE (
											scsiRequest,
											dataBuffer,
											PAD,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::COPY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit 			PAD, 
						SCSICmdField3Byte 			PARAMETER_LIST_LENGTH,  
						SCSICmdField1Byte 			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: COPY called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->COPY (
											scsiRequest,
											dataBuffer,
											PAD,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::COPY_AND_VERIFY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit 			BYTCHK, 
						SCSICmdField1Bit 			PAD, 
						SCSICmdField3Byte 			PARAMETER_LIST_LENGTH, 
						SCSICmdField1Byte 			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: COPY_AND_VERIFY called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->COPY_AND_VERIFY (
											scsiRequest,
											dataBuffer,
											BYTCHK,
											PAD,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}


bool	
IOSCSIPrimaryCommandsDevice::EXTENDED_COPY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField3Byte			PARAMETER_LIST_LENGTH, 
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: EXTENDED_COPY called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->EXTENDED_COPY (
											scsiRequest,
											dataBuffer,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::INQUIRY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor			*dataBuffer,
						SCSICmdField1Bit			CMDDT,
						SCSICmdField1Bit			EVPD,
						SCSICmdField1Byte			PAGE_OR_OPERATION_CODE,
						SCSICmdField1Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{	
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: INQUIRY called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->INQUIRY (
											scsiRequest,
											dataBuffer,
											CMDDT,
											EVPD,
											PAGE_OR_OPERATION_CODE,
											ALLOCATION_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::LOG_SELECT (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit			PCR,
						SCSICmdField1Bit			SP,
						SCSICmdField2Bit			PC,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: LOG_SELECT called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->LOG_SELECT (
											scsiRequest,
											dataBuffer,
											PCR,
											SP,
											PC,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::LOG_SENSE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit			PPC,
						SCSICmdField1Bit			SP,
						SCSICmdField2Bit 			PC,
						SCSICmdField6Bit			PAGE_CODE,
						SCSICmdField2Byte			PARAMETER_POINTER,
						SCSICmdField2Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: LOG_SENSE called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->LOG_SENSE (
											scsiRequest,
											dataBuffer,
											PPC,
											SP,
											PC,
											PAGE_CODE,
											PARAMETER_POINTER,
											ALLOCATION_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::MODE_SELECT_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit 			PF,
						SCSICmdField1Bit			SP,
						SCSICmdField1Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: MODE_SELECT_6 called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->MODE_SELECT_6 (
											scsiRequest,
											dataBuffer,
											PF,
											SP,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::MODE_SELECT_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit			PF,
						SCSICmdField1Bit			SP,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: MODE_SELECT_10 called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->MODE_SELECT_10 (
											scsiRequest,
											dataBuffer,
											PF,
											SP,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::MODE_SENSE_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit			DBD,
						SCSICmdField2Bit			PC,
						SCSICmdField6Bit			PAGE_CODE,
						SCSICmdField1Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: MODE_SENSE_6 called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->MODE_SENSE_6 (
											scsiRequest,
											dataBuffer,
											DBD,
											PC,
											PAGE_CODE,
											ALLOCATION_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::MODE_SENSE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit			LLBAA,
						SCSICmdField1Bit 			DBD,
						SCSICmdField2Bit 			PC,
						SCSICmdField6Bit 			PAGE_CODE,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: MODE_SENSE_10 called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->MODE_SENSE_10 (
											scsiRequest,
											dataBuffer,
											LLBAA,
											DBD,
											PC,
											PAGE_CODE,
											ALLOCATION_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::PERSISTENT_RESERVE_IN (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField5Bit			SERVICE_ACTION,
						SCSICmdField2Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: PERSISTENT_RESERVE_IN called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->PERSISTENT_RESERVE_IN (
											scsiRequest,
											dataBuffer,
											SERVICE_ACTION,
											ALLOCATION_LENGTH,
											CONTROL );
	
}


bool	
IOSCSIPrimaryCommandsDevice::PERSISTENT_RESERVE_OUT (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField5Bit			SERVICE_ACTION, 
   						SCSICmdField4Bit			SCOPE, 
   						SCSICmdField4Bit			TYPE,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: PERSISTENT_RESERVE_OUT called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->PERSISTENT_RESERVE_OUT (
											scsiRequest,
											dataBuffer,
											SERVICE_ACTION,
											SCOPE,
											TYPE,
											CONTROL );
	
}


bool	
IOSCSIPrimaryCommandsDevice::PREVENT_ALLOW_MEDIUM_REMOVAL (
						SCSITaskIdentifier			request,
						SCSICmdField2Bit			PREVENT,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: PREVENT_ALLOW_MEDIUM_REMOVAL called.\n", getName ( ) ) );

	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->PREVENT_ALLOW_MEDIUM_REMOVAL (
											scsiRequest,
											PREVENT,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::READ_BUFFER (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField4Bit			MODE,
						SCSICmdField1Byte			BUFFER_ID,
						SCSICmdField3Byte			BUFFER_OFFSET,
						SCSICmdField3Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: READ_BUFFER called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->READ_BUFFER (
											scsiRequest,
											dataBuffer,
											MODE,
											BUFFER_ID,
											BUFFER_OFFSET,
											ALLOCATION_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::RECEIVE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField3Byte			TRANSFER_LENGTH, 
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: RECEIVE called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->RECEIVE(
											scsiRequest,
											dataBuffer,
											TRANSFER_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::RECEIVE_DIAGNOSTICS_RESULTS (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit			PCV,
						SCSICmdField1Byte			PAGE_CODE,
						SCSICmdField2Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: RECEIVE_DIAGNOSTICS_RESULTS called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->RECEIVE_DIAGNOSTICS_RESULTS (
											scsiRequest,
											dataBuffer,
											PCV,
											PAGE_CODE,
											ALLOCATION_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::RELEASE_6 (
						SCSITaskIdentifier			request,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: RELEASE_6 called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->RELEASE_6 (
										scsiRequest,
										CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::RELEASE_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor			*dataBuffer,
						SCSICmdField1Bit			EXTENT,
						SCSICmdField1Byte			RESERVATION_IDENTIFICATION,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: RELEASE_6 *OBSOLETE* called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->RELEASE_6 (
										scsiRequest,
										EXTENT,
										RESERVATION_IDENTIFICATION,
										CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::RELEASE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit			THRDPTY,
						SCSICmdField1Bit			LONGID,
						SCSICmdField1Byte			THIRD_PARTY_DEVICE_ID,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: RELEASE_10 called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->RELEASE_10 (
											scsiRequest,
											dataBuffer,
											THRDPTY,
											LONGID,
											THIRD_PARTY_DEVICE_ID,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::RELEASE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit			THRDPTY,
						SCSICmdField1Bit			LONGID,
						SCSICmdField1Bit			EXTENT,
						SCSICmdField1Byte			RESERVATION_IDENTIFICATION,
						SCSICmdField1Byte			THIRD_PARTY_DEVICE_ID,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: RELEASE_10 *OBSOLETE* called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->RELEASE_10 (
											scsiRequest,
											dataBuffer,
											THRDPTY,
											LONGID,
											EXTENT,
											RESERVATION_IDENTIFICATION,
											THIRD_PARTY_DEVICE_ID,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::REPORT_DEVICE_IDENTIFIER (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField4Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: REPORT_DEVICE_IDENTIFIER called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->REPORT_DEVICE_IDENTIFIER (
											scsiRequest,
											dataBuffer,
											ALLOCATION_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::REPORT_LUNS (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField4Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: REPORT_LUNS called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->REPORT_LUNS (
											scsiRequest,
											dataBuffer,
											ALLOCATION_LENGTH,
											CONTROL );
	
}


bool	
IOSCSIPrimaryCommandsDevice::REQUEST_SENSE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Byte			ALLOCATION_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: REQUEST_SENSE called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->REQUEST_SENSE (
											scsiRequest,
											dataBuffer,
											ALLOCATION_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::RESERVE_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: RESERVE_6 called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->RESERVE_6 (
											scsiRequest,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::RESERVE_6 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor	 		*dataBuffer, 
						SCSICmdField1Bit			EXTENT, 
						SCSICmdField1Byte			RESERVATION_IDENTIFICATION,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: RESERVE_6 *OBSOLETE* called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->RESERVE_6 (
											scsiRequest,
											dataBuffer,
											EXTENT,
											RESERVATION_IDENTIFICATION,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::RESERVE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit			THRDPTY,
						SCSICmdField1Bit			LONGID,
						SCSICmdField1Byte			THIRD_PARTY_DEVICE_ID,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: RESERVE_10 called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->RESERVE_10 (
											scsiRequest,
											dataBuffer,
											THRDPTY,
											LONGID,
											THIRD_PARTY_DEVICE_ID,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::RESERVE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit			THRDPTY,
						SCSICmdField1Bit			LONGID,
						SCSICmdField1Bit			EXTENT,
						SCSICmdField1Byte			RESERVATION_IDENTIFICATION,
						SCSICmdField1Byte			THIRD_PARTY_DEVICE_ID,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: RESERVE_10 *OBSOLETE* called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->RESERVE_10 (
											scsiRequest,
											dataBuffer,
											THRDPTY,
											LONGID,
											EXTENT,
											RESERVATION_IDENTIFICATION,
											THIRD_PARTY_DEVICE_ID,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::SEND (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField1Bit			AER,
						SCSICmdField3Byte			TRANSFER_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: SEND called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->SEND (
											scsiRequest,
											dataBuffer,
											AER,
											TRANSFER_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::SEND_DIAGNOSTICS (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField3Bit			SELF_TEST_CODE,
						SCSICmdField1Bit			PF,
						SCSICmdField1Bit			SELF_TEST,
						SCSICmdField1Bit			DEVOFFL,
						SCSICmdField1Bit			UNITOFFL,
						SCSICmdField2Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: SEND_DIAGNOSTICS called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->SEND_DIAGNOSTICS (
											scsiRequest,
											dataBuffer,
											SELF_TEST_CODE,
											PF,
											SELF_TEST,
											DEVOFFL,
											UNITOFFL,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::SET_DEVICE_IDENTIFIER (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField5Bit			SERVICE_ACTION,
						SCSICmdField4Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: SET_DEVICE_IDENTIFIER called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->SET_DEVICE_IDENTIFIER (
											scsiRequest,
											dataBuffer,
											SERVICE_ACTION,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}



bool	
IOSCSIPrimaryCommandsDevice::TEST_UNIT_READY (
						SCSITaskIdentifier			request,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: TEST_UNIT_READY called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}

	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->TEST_UNIT_READY (
											scsiRequest,
											CONTROL );
	
}


bool	
IOSCSIPrimaryCommandsDevice::WRITE_BUFFER (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor 			*dataBuffer,
						SCSICmdField4Bit			MODE,
						SCSICmdField1Byte			BUFFER_ID,
						SCSICmdField3Byte			BUFFER_OFFSET,
						SCSICmdField3Byte			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte			CONTROL )
{
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: WRITE_BUFFER called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest == NULL )
	{
		
		ERROR_LOG ( ( "OSDynamicCast on the request SCSITaskIdentifier failed.\n" ) );
		return false;
		
	}
	
	if ( scsiRequest->ResetForNewTask ( ) == false )
	{
		
		ERROR_LOG ( ( "ResetForNewTask on the request SCSITask failed.\n" ) );
		return false;
		
	}
	
	return GetSCSIPrimaryCommandObject ( )->WRITE_BUFFER (
											scsiRequest,
											dataBuffer,
											MODE,
											BUFFER_ID,
											BUFFER_OFFSET,
											PARAMETER_LIST_LENGTH,
											CONTROL );
	
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 1 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 2 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 3 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 4 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 5 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 6 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 7 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 8 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 9 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 10 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 11 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 12 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 13 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 14 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 15 );
OSMetaClassDefineReservedUnused( IOSCSIPrimaryCommandsDevice, 16 );
