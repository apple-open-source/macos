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

#include "IOFireWireSerialBusProtocolTransport.h"

// IOFireWireFamily includes
#include <IOKit/firewire/IOConfigDirectory.h>
#include <IOKit/firewire/IOFireWireDevice.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"FireWire SBP Transport"

#if DEBUG
#define FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL				0
#endif

#if ( FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOSCSIProtocolServices
OSDefineMetaClassAndStructors (	IOFireWireSerialBusProtocolTransport, IOSCSIProtocolServices )


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kSBP2ReceiveBufferByteCountKey		"SBP2ReceiveBufferByteCount"
#define kDefaultIOBlockCount				256
#define kCRSModelInfo_ValidBitsMask			0x00FFFFFF
#define kCRSModelInfo_TargetDiskMode		0x0054444D


#if 0
#pragma mark -
#pragma mark ¥ Public Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ init - Called by IOKit to initialize us.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOFireWireSerialBusProtocolTransport::init ( OSDictionary * propTable )
{
	
	fORB					= 0;
	fLogin					= 0;
	fLoginRetryCount		= 0;
	fReconnectCount			= 0;
	fLoggedIn				= false;
	fPhysicallyConnected	= true;
	fNeedLogin				= false;
	fLUNResetORB			= NULL;
	fDeferRegisterService	= true;
	fObjectIsOpen			= false;
	
	if ( super::init ( propTable ) == false )
	{
		
		return false;
		
	}
	
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ start - Called by IOKit to start our services.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool 
IOFireWireSerialBusProtocolTransport::start ( IOService * provider )
{
	
	IOReturn	status			= kIOReturnSuccess;
	Boolean		returnValue		= false;
	Boolean 	openSucceeded	= false;
	
	fSBPTarget = OSDynamicCast ( IOFireWireSBP2LUN, provider );
	if ( fSBPTarget == NULL ) goto ErrorExit;
	
	if ( super::start ( provider ) == false ) goto ErrorExit;
	
	if ( provider->open ( this ) == false ) goto ErrorExit;
	
	openSucceeded = true;
	
	fUnit = fSBPTarget->getFireWireUnit ( );
	if ( fUnit == NULL ) goto ErrorExit;
	
	// enable retry on ack d flag
	fUnit->setNodeFlags ( kIOFWEnableRetryOnAckD );
	
	status = AllocateResources ( );
	if ( status != kIOReturnSuccess ) goto ErrorExit;
	
	// get us on the workloop so we can sleep the start thread
	fCommandGate->runAction ( ConnectToDeviceStatic );
	
	if ( reserved->fLoginState == kLogginSucceededState )
	{
		
		registerService ( );
		
	}
	
	STATUS_LOG ( ( "%s: start complete\n", getName ( ) ) );
	
	returnValue = true;
	
	InitializePowerManagement ( provider );
	
	
ErrorExit:
	
	
	if ( returnValue == false )
	{
		
		STATUS_LOG ( ( "%s: start failed.  status = %x\n", getName ( ), status) );
		
		// call the cleanUp method to clean up any allocated resources.
		cleanUp ( );
		
		// close SBP2 if we have opened it.
		if ( ( fSBPTarget != NULL ) && openSucceeded )
		{
			
			fSBPTarget->close ( this );
			fSBPTarget = NULL;
			
		}
		
	}
	
	return returnValue;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ cleanUp - Called to deallocate any resources.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::cleanUp ( )
{
	
	STATUS_LOG ( ( "%s: cleanUp called\n", getName ( ) ) );
	
	if ( fSBPTarget != NULL )
	{
		
		DeallocateResources ( );
		
	}
	
}


#if 0
#pragma mark -
#pragma mark ¥ Protected Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CommandORBAccessor - Retrieves command orb.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOFireWireSBP2ORB *
IOFireWireSerialBusProtocolTransport::CommandORBAccessor ( void )
{
	
	return fORB;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SBP2LoginAccessor - Retrieves login orb.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOFireWireSBP2Login *
IOFireWireSerialBusProtocolTransport::SBP2LoginAccessor ( void )
{

	return fLogin;

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ message - Called by IOKit to deliver messages.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOFireWireSerialBusProtocolTransport::message (	UInt32		type,
												IOService *	nub,
												void *		arg )
{
	
	SBP2ClientOrbData *		clientData 	= NULL;
	IOReturn				status		= kIOReturnSuccess;
	
	switch ( type )
	{
		
		case kIOMessageServiceIsSuspended:
			STATUS_LOG ( ( "%s: kIOMessageServiceIsSuspended\n", getName ( ) ) );
			
			fLoggedIn = false;
			
			break;
			
		case kIOMessageServiceIsResumed:
			STATUS_LOG ( ( "%s: kIOMessageServiceIsResumed\n", getName ( ) ) );
			
			fPhysicallyConnected = true;
			
			if ( fNeedLogin )
			{
				
				STATUS_LOG ( ( "%s: fNeedLogin submitLogin\n", getName ( ) ) );
				
				fNeedLogin = false;
				
				// in case we are resumed after a terminate - check fLogin
				if ( fLogin != NULL )
				{
					fLogin->submitLogin ( );
				}
				
			}
			
			break;
			
		case kIOMessageFWSBP2ReconnectComplete:
			STATUS_LOG ( ( "%s: kIOMessageFWSBP2ReconnectComplete\n", getName ( ) ) );
			
			fLoggedIn = true;
			
			clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ( );
			if ( clientData != NULL )
			{
				
				if ( ( clientData->scsiTask != NULL ) && ( fReconnectCount < kMaxReconnectCount ) )
				{
					
					STATUS_LOG ( ( "%s: resubmit orb \n", getName ( ) ) );
					fReconnectCount++;
					fLogin->submitORB ( fORB );
					
				}
				
				else
				{
					
					// We are unable to recover from bus reset storm
					// We have exhausted the fReconnectCount - punt...
					
					clientData->serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus		= kSCSITaskStatus_No_Status;
					
					CompleteSCSITask ( fORB );
					
				}
				
			}
			
			break;
			
		case kIOMessageFWSBP2ReconnectFailed:
			STATUS_LOG ( ( "%s: kIOMessageFWSBP2ReconnectFailed\n", getName ( ) ) );
			
			if ( fPhysicallyConnected )
			{
				
				fLogin->submitLogin( );
				
			}
			
			else
			{
				
				fNeedLogin = true;
				
			}
			
			break;

		case kIOFWMessageServiceIsRequestingClose:
			STATUS_LOG ( ( "%s: kIOMessageServiceIsRequestingClose\n", getName ( ) ) );
			
			// tell our super to message it's clients that the device is gone
			SendNotification_DeviceRemoved ( );
			
			clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ( );
			if ( clientData != NULL )
			{
				
				if ( clientData->scsiTask != NULL )
				{
					
					// We are unable to recover from bus reset storm
					// We have exhausted the fReconnectCount - punt...
					
					clientData->serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus		= kSCSITaskStatus_No_Status;
					
					CompleteSCSITask ( fORB );
					
				}
				
			}
			
			// let go of memory and what not
			cleanUp ( );
			
			// close SBP2 and allow termination to continue
			if ( fSBPTarget != NULL )
			{
				fSBPTarget->close ( this );
			}
			
			// zero out our provider reference
			fSBPTarget = NULL;
			
			break;
			
		case kIOMessageServiceIsTerminated:
			STATUS_LOG ( ( "%s: kIOMessageServiceIsTerminated\n", getName ( ) ) );
			break;
			
		default:
			
			status = IOService::message (type, nub, arg);
			break;
		
	}
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SendSCSICommand - Converts a SCSITask to an ORB.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOFireWireSerialBusProtocolTransport::SendSCSICommand (
							SCSITaskIdentifier		request,
							SCSIServiceResponse * 	serviceResponse,
							SCSITaskStatus *		taskStatus )
{
	
	SBP2ClientOrbData *			clientData 			= NULL;
	IOFireWireSBP2ORB *			orb					= NULL;
	SCSICommandDescriptorBlock	cdb					= { 0 };
	UInt8						commandLength		= 0;
	UInt32						commandFlags		= 0;
	UInt32						timeOut				= 0;
	bool						commandProcessed	= true;
	
	STATUS_LOG ( ( "%s: SendSCSICommand called\n", getName ( ) ) );
	
	*serviceResponse	= kSCSIServiceResponse_Request_In_Process;
	*taskStatus			= kSCSITaskStatus_No_Status;
	
	if ( isInactive ( ) )
	{
		
		// device is disconnected - we can not service command request
		*serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		commandProcessed = false;
		goto ErrorExit;
		
	}
	
	// get an orb from our orb pool without blocking
	orb = ( IOFireWireSBP2ORB * ) reserved->fCommandPool->getCommand ( false );
	
	if ( orb == NULL )
	{

		// We're busy - return false - command will be resent next time CommandComplete is called
		commandProcessed = false;
		goto ErrorExit;
		
	}
	
	clientData = ( SBP2ClientOrbData * ) orb->getRefCon ( );
	if ( clientData == NULL ) goto ErrorExit;
	
	GetCommandDescriptorBlock ( request, &cdb );
	commandLength = GetCommandDescriptorBlockSize ( request );
	
	#if ( FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 3 )
	if ( commandLength == kSCSICDBSize_6Byte )
	{
		
		STATUS_LOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0], cdb[1],
					 cdb[2], cdb[3], cdb[4], cdb[5] ) );
		
	}
	
	else if ( commandLength == kSCSICDBSize_10Byte )
	{
		
		STATUS_LOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0],
					cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
					cdb[9] ) );
		
	}
	
	else if ( commandLength == kSCSICDBSize_12Byte )
	{
		
		STATUS_LOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0],
					cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
					cdb[9], cdb[10], cdb[11] ) );
		
	}
	#endif /* (FIREWIRE_SBP_TRANSPORT_DEBUGGING_LEVEL >= 3) */
	
	fReconnectCount	= 0;
	commandFlags	= GetDataTransferDirection ( request ) == kSCSIDataTransfer_FromTargetToInitiator ? 
															  kFWSBP2CommandTransferDataFromTarget : 0L;
	
	orb->setCommandFlags (	commandFlags |
							kFWSBP2CommandCompleteNotify |
							kFWSBP2CommandImmediate |
							kFWSBP2CommandNormalORB );
	
	
	clientData->scsiTask = request;
	
	SetCommandBuffers ( orb, request );
	
	orb->setCommandBlock ( cdb, commandLength );
	
	// SBP-2 needs a non-zero timeout to fire completion routines if timeout
	// is not expressed default to 0xFFFFFFFF.
	
	timeOut = GetTimeoutDuration ( request );
	if ( timeOut == 0 )
	{
		timeOut = 0xFFFFFFFF;
	}
	
	orb->setCommandTimeout ( timeOut );
	
	if ( fLoggedIn )
	{
		
		fLogin->submitORB ( orb );
		
	}
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "%s: SendSCSICommand exit, Service Response = %x\n", getName ( ), *serviceResponse) );
	
	return commandProcessed;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetCommandBuffers - Sets the command buffers in the ORB.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOFireWireSerialBusProtocolTransport::SetCommandBuffers (
								IOFireWireSBP2ORB *		orb,
								SCSITaskIdentifier		request )
{
	
	return orb->setCommandBuffers (	GetDataBuffer ( request ),
								GetDataBufferOffset ( request ),
								GetRequestedDataTransferCount ( request ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CompleteSCSITask - Complets a task.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::CompleteSCSITask (
								IOFireWireSBP2ORB *		orb )
{
	
	SBP2ClientOrbData *	clientData = NULL;
	
	STATUS_LOG ( ( "%s: CompleteSCSITask called\n", getName ( ) ) );
	
	clientData = ( SBP2ClientOrbData * ) orb->getRefCon ( );
	if ( clientData != NULL )
	{
		
		if ( clientData->scsiTask != NULL )
		{
			
			SCSITaskIdentifier		scsiTask		= NULL;
			SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
			SCSITaskStatus			taskStatus		= kSCSITaskStatus_No_Status;
			IOByteCount				bytesTransfered = 0;
			
			/*
			 	/!\ WARNING - because SBP-2 can send status information at different
			 	stage of an orb's life ( or send none at all ) the caller of this routine
			 	has determined that the orb is indeed done. So we need to explicitly tell
				SBP-2 to left go of the buffer reference by calling releaseCommandBuffers.
			*/
			
			orb->releaseCommandBuffers ( );
			
			
			if ( clientData->taskStatus == kSCSITaskStatus_GOOD )
			{
				
				bytesTransfered = GetRequestedDataTransferCount ( clientData->scsiTask );
				
			}
			
			SetRealizedDataTransferCount ( clientData->scsiTask, bytesTransfered );
			
			// re-entrancy protection
			scsiTask				= clientData->scsiTask;
			serviceResponse 		= clientData->serviceResponse;
			taskStatus				= clientData->taskStatus;
			clientData->scsiTask	= NULL;
			
			reserved->fCommandPool->returnCommand ( orb );
			
			CommandCompleted ( scsiTask, serviceResponse, taskStatus );
			
		}
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortSCSICommand - Aborts an outstanding I/O.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIServiceResponse
IOFireWireSerialBusProtocolTransport::AbortSCSICommand ( SCSITaskIdentifier request )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_FUNCTION_REJECTED;
	
	STATUS_LOG ( ( "%s: AbortSCSICommand called\n", getName ( ) ) );
	
	return serviceResponse;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IsProtocolServiceSupported -	Checks for valid protocol services
//									supported by this device.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOFireWireSerialBusProtocolTransport::IsProtocolServiceSupported (
								SCSIProtocolFeature		feature,
								void *					serviceValue )
{
	
	bool	isSupported = false;
	
	STATUS_LOG ( ( "IOFireWireSerialBusProtocolTransport::IsProtocolServiceSupported called\n" ) );
	
	switch ( feature )
	{
		
		case kSCSIProtocolFeature_CPUInDiskMode:
			
			isSupported = IsDeviceCPUInDiskMode ( );
			
			break;
		
		case kSCSIProtocolFeature_MaximumReadBlockTransferCount:
		case kSCSIProtocolFeature_MaximumWriteBlockTransferCount:
			
			*( UInt32 * ) serviceValue = kDefaultIOBlockCount;
			isSupported = true;
			
			break;
			
		case kSCSIProtocolFeature_MaximumReadTransferByteCount:
			
			OSNumber *		valuePointer;
			
			// If the property SBP2ReceiveBufferByteCount exists we have a FireWire host
			// with the physical unit off and there is a software FIFO. ( i.e. Lynx )
			
			// We should tell clients to deblock on the SBP2ReceiveBufferByteCount.
			// bounds to avoid stalled I/O.
			
			valuePointer = OSDynamicCast ( OSNumber, getProperty ( kSBP2ReceiveBufferByteCountKey, gIOServicePlane ) );
			if ( valuePointer != NULL )
			{
				
				*( UInt64 * ) serviceValue = valuePointer->unsigned32BitValue ( );
				isSupported = true;			
				
			}
			
			break;
			
		case kSCSIProtocolFeature_GetMaximumLogicalUnitNumber:
			
			*( UInt32 * ) serviceValue = kMaxFireWireLUN;
			isSupported = true;
			
			break;
			
		default:
			break;
		
	}
	
	return isSupported;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandleProtocolServiceFeature - Handles protocol service features.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOFireWireSerialBusProtocolTransport::HandleProtocolServiceFeature ( 
								SCSIProtocolFeature		feature, 
								void *					serviceValue )
{
	return false;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IsDeviceCPUInDiskMode - 	Checks if the device is a CPU in
//								FireWire Target Disk Mode.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOFireWireSerialBusProtocolTransport::IsDeviceCPUInDiskMode ( void )
{
	
	UInt32						csrModelInfo 	= 0;
	IOConfigDirectory *			directory		= NULL;
	IOFireWireDevice *			device			= NULL;
	IOReturn					status			= kIOReturnSuccess;
	IOService *					providerService = fUnit->getProvider( );
	bool						isCPUDiskMode	= false;
	
	STATUS_LOG ( ( "%s: IsDeviceCPUInDiskMode was called\n", getName ( ) ) );
	
	if ( providerService == NULL )
	{
		goto ErrorExit;
	}
	
	device = OSDynamicCast ( IOFireWireDevice, providerService );
	if ( device == NULL )
	{
		goto ErrorExit;
	}
	
	status = device->getConfigDirectory ( directory );	
	if ( status == kIOReturnSuccess )
	{
		
		status = directory->getKeyValue ( kCSRModelInfoKey, csrModelInfo );
		
		if ( status == kIOReturnSuccess )
		{
			
			if ( ( csrModelInfo & kCRSModelInfo_ValidBitsMask ) == kCRSModelInfo_TargetDiskMode )
			{
				isCPUDiskMode = true;
			}
			
		}
		
	}
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "%s: CPU Disk Mode = %d\n", getName ( ), isCPUDiskMode ) );
	
	return isCPUDiskMode;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StatusNotifyStatic - 	C->C++ glue method.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::StatusNotifyStatic (
								void *					refCon,
								FWSBP2NotifyParams * 	params )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->StatusNotify ( params );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StatusNotify - Status notify handler.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOFireWireSerialBusProtocolTransport::StatusNotify (
								FWSBP2NotifyParams * params )
{

	IOFireWireSBP2ORB *		orb				= NULL;
	FWSBP2StatusBlock *		statusBlock 	= NULL;
	SBP2ClientOrbData *		clientData		= NULL;
	SCSI_Sense_Data *		targetData		= NULL;
	UInt8			 		senseData[kSenseDefaultSize + 8] = { 0 };
	
	targetData = ( SCSI_Sense_Data * ) &senseData[0];
	
	if ( ( params->message != NULL ) && ( params->length != 0 ) )
	{
		
		orb			= ( IOFireWireSBP2ORB * ) params->commandObject;
		statusBlock = ( FWSBP2StatusBlock * ) params->message;
		
		if ( orb != NULL )
		{
			
			clientData = ( SBP2ClientOrbData * ) orb->getRefCon( );
			
		}
		
	}
	
	switch ( params->notificationEvent )
	{
		
		case kFWSBP2NormalCommandStatus:
			
			/*
				read the status block detail bits see SBP-2 spec section 5.3
				check the dead bit ( is 'd' field == 1 )
			*/
			
			if ( ( clientData != NULL ) && ( statusBlock->details & 0x08 ) ) 	
			{
				
				SetValidAutoSenseData ( clientData, statusBlock, targetData );
				
				/*
					wait for fetch agent to reset before calling CompleteSCSITask
					which will be called in FetchAgentResetComplete
				*/
				
				fLogin->submitFetchAgentReset ( );
				
			}
			
			else if ( clientData &&
					( ( statusBlock->details & 0x30 ) == 0 ) &&		// ( is 'resp' field == 0 )
					( ( statusBlock->details & 0x07 ) == 1 ) && 	// ( is 'len' field == 1 )
					( statusBlock->sbpStatus == 0 ) )				// ( is 'sbp_status' field == 0 )
			{
				
				clientData->serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
				clientData->taskStatus		= kSCSITaskStatus_GOOD;
				
				CompleteSCSITask ( orb );
				
				STATUS_LOG ( ( "%s: StatusNotify normal complete \n", getName ( ) ) );
				
			}
			
			else if ( clientData )
			{
				
				SetValidAutoSenseData ( clientData, statusBlock, targetData );
				
				CompleteSCSITask ( orb );
				
				STATUS_LOG ( ( "%s: StatusNotify unexpected error? \n", getName ( ) ) );
				
			}
			
			break;

		case kFWSBP2NormalCommandTimeout:
			STATUS_LOG ( ( "%s: kFWSBP2NormalCommandTimeout \n", getName ( ) ) );

			if ( clientData )
			{
				
				if ( clientData->scsiTask )
				{
					
					clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus		= kSCSITaskStatus_No_Status;
					
					CompleteSCSITask ( orb );
					
				}
				
			}
			
			// reset LUN as good measure in case device is wedged
			fLUNResetORB->submit ( );
			
			break;
			
		case kFWSBP2NormalCommandReset:
			STATUS_LOG ( ( "%s: kFWSBP2NormalCommandReset\n", getName ( ) ) );
			
			/*
				kFWSBP2NormalCommandReset - is a misleading definition
				A pending command has failed so we need notify
				the upper layers to complete failed command.
			*/
			
			if ( clientData != NULL )
			{
				
				if ( clientData->scsiTask != NULL )
				{
					
					clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus		= kSCSITaskStatus_No_Status;
					
					CompleteSCSITask ( orb );
					
				}
				
			}
			
			break;
			
		default:
			STATUS_LOG ( ( "%s: StatusNotify with unknown notificationEvent\n", getName ( ) ) );
			break;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetValidAutoSenseData - Sets any valid sense data.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOFireWireSerialBusProtocolTransport::SetValidAutoSenseData (
					SBP2ClientOrbData *		clientData,
					FWSBP2StatusBlock *		statusBlock,
					SCSI_Sense_Data *		targetData )
{
	
	UInt8		quadletCount = 0;
	
	clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	clientData->taskStatus		= kSCSITaskStatus_No_Status;
	
	quadletCount = ( statusBlock->details & 0x07 ) - 1 ;
	
	// see if we have any valid sense data
	if ( ( statusBlock->details & 0x30 ) == 0 )
	{
		
		clientData->serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
		clientData->taskStatus		= CoalesceSenseData ( statusBlock, quadletCount, targetData );
		
	}
	
	if ( clientData->taskStatus == kSCSITaskStatus_CHECK_CONDITION )
	{
		
		if ( clientData->scsiTask != NULL )
		{
			
			SetAutoSenseData ( clientData->scsiTask, targetData, kSenseDefaultSize + 8 );
 			
		}
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CoalesceSenseData - Sets sense data in the data buffer.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSITaskStatus
IOFireWireSerialBusProtocolTransport::CoalesceSenseData (
					FWSBP2StatusBlock *		sourceData,
					UInt8					quadletCount,
					SCSI_Sense_Data *		targetData )
{
	
	SCSITaskStatus	returnValue			= kSCSITaskStatus_GOOD;
	UInt8 			statusBlockFormat	= 0;
	
	// Pull bits out of SBP-2 status block ( see SBP-2 Annex B section B.2 )
	// and copy them into sense data block ( see SPC-2 section 7.23.2 )
	
	if ( quadletCount > 0 )
	{
		
		statusBlockFormat = ( sourceData->status[0] >> 30 ) & 0x03;
		returnValue = ( SCSITaskStatus ) ( ( sourceData->status[0] >> 24 ) & 0x3F );
		
		if ( statusBlockFormat == 0 ) 
		{
			
			targetData->VALID_RESPONSE_CODE = kSENSE_RESPONSE_CODE_Current_Errors;
			
		}
		
		else if ( statusBlockFormat == 1 )
		{
			
			targetData->VALID_RESPONSE_CODE = kSENSE_RESPONSE_CODE_Deferred_Errors;
			
		}
		
		if ( statusBlockFormat < 2 )
		{
			
			targetData->VALID_RESPONSE_CODE |= ( sourceData->status[0] >> 16 ) & 0x80;
			targetData->ADDITIONAL_SENSE_CODE = ( sourceData->status[0] >> 8 ) & 0xFF;
			targetData->ADDITIONAL_SENSE_CODE_QUALIFIER = sourceData->status[0] & 0xFF;
			targetData->SENSE_KEY = ( sourceData->status[0] >> 16 ) & 0x0F;
			
			// Set the M, E, I
			// M->FileMark, E->EOM, I->ILI
			targetData->SENSE_KEY |= ( ( sourceData->status[0] >> 16 ) & 0x70 ) << 1;
			
			if ( quadletCount > 1 )
			{
				
				targetData->INFORMATION_1 = ( sourceData->status[1] >> 24 ) & 0xFF;
				targetData->INFORMATION_2 = ( sourceData->status[1] >> 16 ) & 0xFF;
				targetData->INFORMATION_3 = ( sourceData->status[1] >> 8 ) & 0xFF;
				targetData->INFORMATION_4 = sourceData->status[1] & 0xFF;
				targetData->ADDITIONAL_SENSE_LENGTH = 6;
				
			}
			
			if ( quadletCount > 2 )
			{
				
				targetData->COMMAND_SPECIFIC_INFORMATION_1 = ( sourceData->status[2] >> 24 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_2 = ( sourceData->status[2] >> 16 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_3 = ( sourceData->status[2] >> 8 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_4 = sourceData->status[2] & 0xFF;
				targetData->ADDITIONAL_SENSE_LENGTH = 6;
				
			}
			
			if ( quadletCount > 3 )
			{
				
				UInt8	count = ( quadletCount - 3 ) * sizeof ( quadletCount );
				
				bcopy ( &sourceData->status[3],
						&targetData->FIELD_REPLACEABLE_UNIT_CODE,
						count );
				
				targetData->ADDITIONAL_SENSE_LENGTH = count + 6;
				
			}
			
		}
		
	}
	
	return returnValue;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LoginCompletionStatic - 	C->C++ glue method.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::LoginCompletionStatic (
								void *							refCon,
								FWSBP2LoginCompleteParams *		params )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->LoginCompletion ( params );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LoginCompletion - Login completion handler.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOFireWireSerialBusProtocolTransport::LoginCompletion (
								FWSBP2LoginCompleteParams *		params )
{
	
	SBP2ClientOrbData *		clientData = NULL;
	
	STATUS_LOG ( ( "%s: LoginCompletion complete \n", getName ( ) ) );
	
	if	( ( params->status == kIOReturnSuccess ) &&				// ( kIOReturnSuccess )
		( ( params->statusBlock->details & 0x30 ) == 0 ) &&		// ( is 'resp' field == 0 )
		  ( params->statusBlock->sbpStatus == 0 ) )				// ( is 'sbp_status' field == 0 )
	{
		
		fLoginRetryCount	= 0;
		fLoggedIn			= true;
		
		if ( reserved->fLoginState == kFirstTimeLoggingInState )
		{
			
			reserved->fLoginState = kLogginSucceededState;
			fCommandGate->commandWakeup ( ( void * ) &reserved->fLoginState );
			
		}
		
		clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ( );
		if ( clientData != NULL )
		{
			
			if ( clientData->scsiTask )
			{
				
				fLogin->submitORB ( fORB );
				
			}
			
		}
		
	}
	
	else
	{
		
		if ( fPhysicallyConnected )
		{
			
			if ( fLoginRetryCount < kMaxLoginRetryCount )
			{
				
				fLoginRetryCount++;
				STATUS_LOG ( ( "%s: resubmitting Login\n", getName ( ) ) );
				
				fLogin->submitLogin ( );
				
			}
			
			else
			{
				
				if ( reserved->fLoginState == kFirstTimeLoggingInState )
				{
					
					reserved->fLoginState = kLogginFailedState;
					
					// wake up sleeping start thread
					fCommandGate->commandWakeup ( ( void * ) &reserved->fLoginState );
					
				}
				
				/*
					device can not be logged into after kMaxLoginRetryCount
					attemptes let's reset the need login flag in case
					the device was unplugged during login
				*/
				
				fNeedLogin = true;
				
			}
			
		}
		
		else
		{
			
			/*
				login failed because existing device fell off
				the bus set flag to relogin
			*/
			
			fNeedLogin = true;
			
		}
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LogoutCompletionStatic - C->C++ glue.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::LogoutCompletionStatic (
								void *							refCon,
								FWSBP2LogoutCompleteParams *	params )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->LogoutCompletion ( params );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LogoutCompletion - Logout completion handler.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOFireWireSerialBusProtocolTransport::LogoutCompletion (
								FWSBP2LogoutCompleteParams *	params )
{
	STATUS_LOG ( ( "%s: LogoutCompletion complete \n", getName ( ) ) );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ UnsolicitedStatusNotifyStatic - C->C++ glue.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::UnsolicitedStatusNotifyStatic (
								void *					refCon,
								FWSBP2NotifyParamsPtr	params )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->UnsolicitedStatusNotify ( params );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ UnsolicitedStatusNotify - Unsolicited status handler.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::UnsolicitedStatusNotify (
								FWSBP2NotifyParamsPtr	params )
{
	
	STATUS_LOG ( ( "%s: UnsolicitedStatusNotify called\n", getName ( ) ) );
	
	// parse and handle unsolicited status
	fLogin->enableUnsolicitedStatus ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FetchAgentResetCompleteStatic - C->C++ glue.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::FetchAgentResetCompleteStatic (
								void *		refCon,
								IOReturn 	status )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->FetchAgentResetComplete ( status );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FetchAgentResetComplete - Fetch agent reset handler.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::FetchAgentResetComplete ( IOReturn status )
{
	
	SBP2ClientOrbData *		clientData = NULL;
	
	STATUS_LOG ( ( "%s: FetchAgentResetComplete called\n", getName ( ) ) );
	
	// When orb chaining is implemented we will notify upper layer
	// to reconfigure device state and resubmitting commands
	
	clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ( );
	if ( clientData != NULL )
	{
		
		if ( clientData->scsiTask != NULL )
		{
			
			CompleteSCSITask ( fORB );
			
		}
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LunResetCompleteStatic - C->C++ glue.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::LunResetCompleteStatic (
								void *							refCon,
								IOReturn						status,
								IOFireWireSBP2ManagementORB *	orb )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->LunResetComplete ( status, orb );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LunResetComplete - LUN reset completion handler.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::LunResetComplete (
								IOReturn						status,
								IOFireWireSBP2ManagementORB *	orb )
{
	
	STATUS_LOG ( ( "%s: LunResetComplete called\n", getName ( ) ) );
	
	fLogin->submitFetchAgentReset ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ConnectToDeviceStatic - C->C++ glue.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOFireWireSerialBusProtocolTransport::ConnectToDeviceStatic (
								OSObject *		refCon, 
								void *			val1,
								void *			val2,
								void *			val3,
								void * 			val4 )
{
	
	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->ConnectToDevice ( );
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ConnectToDevice - Connects to the device.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOFireWireSerialBusProtocolTransport::ConnectToDevice ( void )
{
	
	STATUS_LOG ( ( "%s: ConnectToDevice called\n", getName ( ) ) );
	
	// avoid double logins during login phase
	fNeedLogin = false;
	
	fLogin->submitLogin ( );
	
	// sleep the start thread - we'll wake it up on login completion
	fCommandGate->commandSleep ( ( void * ) &reserved->fLoginState , THREAD_UNINT );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DisconnectFromDevice - Disconnects from device.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void 
IOFireWireSerialBusProtocolTransport::DisconnectFromDevice ( void )
{
	
	STATUS_LOG ( ( "%s: DisconnectFromDevice called\n", getName ( ) ) );
	
	fLoggedIn = false;
	
	// avoid logins during a logout phase
	fNeedLogin = false;
	
	if ( fPhysicallyConnected )
	{
		
		fLogin->submitLogout ( );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AllocateResources - Allocates resources.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOFireWireSerialBusProtocolTransport::AllocateResources ( void )
{

	SBP2ClientOrbData *		clientData	= NULL;
	IOReturn 				status		= kIOReturnNoMemory;
	IOWorkLoop * 			workLoop	= NULL;
	
	STATUS_LOG ( ( "%s: AllocateResources called\n", getName ( ) ) );
	
	fLogin = fSBPTarget->createLogin ( );
	if ( fLogin == NULL ) goto ErrorExit;
	
	fORB = fLogin->createORB ( );
	if ( fORB == NULL ) goto ErrorExit;
	
	clientData = ( SBP2ClientOrbData * ) IOMalloc ( sizeof ( SBP2ClientOrbData ) );
	if ( clientData == NULL ) goto ErrorExit;
	
	bzero ( clientData, sizeof ( SBP2ClientOrbData ) );
	clientData->orb	= fORB;
	fORB->setRefCon ( ( void * ) clientData );
	
	fLogin->setLoginFlags ( kFWSBP2ExclusiveLogin );
	fLogin->setLoginRetryCountAndDelayTime ( 32, 1000000 );
	fLogin->setMaxPayloadSize ( kMaxFireWirePayload );
	fLogin->setStatusNotifyProc ( this, StatusNotifyStatic );
	fLogin->setUnsolicitedStatusNotifyProc ( this, UnsolicitedStatusNotifyStatic );
	fLogin->setLoginCompletion ( this, LoginCompletionStatic );
	fLogin->setLogoutCompletion ( this, LogoutCompletionStatic );
	fLogin->setFetchAgentResetCompletion ( this, FetchAgentResetCompleteStatic );
	
	/*
		set BUSY_TIMEOUT register value
		see SBP-2 spec section 6.2
		also see IEEE Std 1394-1995 section 8.3.2.3.5 ( no I am not kidding )
	*/
	
	fLogin->setBusyTimeoutRegisterValue ( kDefaultBusyTimeoutValue );
	
	fLUNResetORB = fSBPTarget->createManagementORB ( this, LunResetCompleteStatic );
	if ( fLUNResetORB == NULL ) goto ErrorExit;
	
	fLUNResetORB->setCommandFunction ( kFWSBP2LogicalUnitReset );
	fLUNResetORB->setManageeCommand ( fLogin );
	
	// allocate expansion data
	
	reserved = ( ExpansionData * ) IOMalloc ( sizeof ( ExpansionData ) );
	if ( reserved == NULL ) goto ErrorExit;
	
	bzero ( reserved, sizeof ( ExpansionData ) );
	
	reserved->fLoginState = kFirstTimeLoggingInState;
	
	workLoop = getWorkLoop ( );
	if ( workLoop == NULL ) goto ErrorExit;
	
	reserved->fCommandPool = IOCommandPool::withWorkLoop ( workLoop );
	if ( reserved->fCommandPool == NULL ) goto ErrorExit;
	
	// enqueue the command in the free list
	
	reserved->fCommandPool->returnCommand ( fORB );
	
	status = kIOReturnSuccess;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DeallocateResources - Deallocates resources.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOFireWireSerialBusProtocolTransport::DeallocateResources ( void )
{
	
	SBP2ClientOrbData *		clientData = NULL;
	
	STATUS_LOG ( ( "%s: DeallocateResources called\n", getName ( ) ) );
	
	if ( reserved != NULL )
	{
		
		if ( reserved->fCommandPool != NULL )
		{
			
			reserved->fCommandPool->release ( );
			reserved->fCommandPool = NULL;
			
		}
		
		IOFree ( reserved, sizeof ( ExpansionData ) );
		reserved = NULL;
		
	}
	
	// /!\ WARNING - always release orb's before logins
	
	if ( fLUNResetORB != NULL )
	{
		
		fLUNResetORB->release ( );
		fLUNResetORB = NULL;
		
	}
	
	if ( fORB != NULL )
	{
		
		clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ( );
		if ( clientData != NULL )
		{
			
			IOFree ( clientData, sizeof ( SBP2ClientOrbData ) );
			
		}
		
		fORB->release ( );
		fORB = NULL;
		
	}
	
	if ( fLogin != NULL )
	{
		
		fLogin->release ( );
		fLogin = NULL;
		
	}
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  1 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  2 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  3 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  4 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  5 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  6 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  7 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  8 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport,  9 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 10 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 11 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 12 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 13 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 14 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 15 );
