/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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


#include <IOKit/sbp2/IOFireWireSerialBusProtocolTransport.h>
#include <IOKit/firewire/IOConfigDirectory.h>
#include <IOKit/firewire/IOFireWireDevice.h>

#define FIREWIRE_MASS_STORAGE_DEBUG 0

#if (FIREWIRE_MASS_STORAGE_DEBUG == 1)
#define STATUS_LOG(x)	IOLog x
#else
#define STATUS_LOG(x)
#endif

#define super IOSCSIProtocolServices

OSDefineMetaClassAndStructors(	IOFireWireSerialBusProtocolTransport,
								IOSCSIProtocolServices )

// binary compatibility reserved method space
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 1 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 2 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 3 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 4 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 5 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 6 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 7 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 8 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 9 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 10 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 11 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 12 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 13 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 14 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 15 );
OSMetaClassDefineReservedUnused ( IOFireWireSerialBusProtocolTransport, 16 );
	
//------------------------------------------------------------------------------

bool 
IOFireWireSerialBusProtocolTransport::init ( OSDictionary *propTable )
{

	fORB = 0;
	fLogin = 0;
	fLoginRetryCount = 0;
	fReconnectCount = 0;
	fLoggedIn = false;
	fPhysicallyConnected = true;
	fNeedLogin = false;
	fLUNResetORB = NULL;
	fDeferRegisterService = true;
	fObjectIsOpen = false;

	if ( super::init ( propTable ) == false )
	{
		
		return false;
	
	}

	return true;

}

//------------------------------------------------------------------------------

bool 
IOFireWireSerialBusProtocolTransport::start ( IOService *provider )
{

	IOReturn status = kIOReturnSuccess;
	Boolean returnValue = false;
	Boolean openSucceeded = false;

	fSBPTarget = OSDynamicCast( IOFireWireSBP2LUN, provider );
	if ( fSBPTarget == NULL ) goto ErrorExit;

	if ( super::start ( provider ) == false ) goto ErrorExit;

	if ( provider->open ( this ) == false ) goto ErrorExit;

	openSucceeded = true;
	
	fUnit = fSBPTarget->getFireWireUnit ( );
	if ( fUnit == NULL ) goto ErrorExit;

	status = AllocateResources ( );
	if ( status != kIOReturnSuccess ) goto ErrorExit;

	ConnectToDevice ( );

	STATUS_LOG ( ("%s: start complete\n", getName ( ) ) );

	returnValue = true;

	InitializePowerManagement ( provider );

ErrorExit:

	if ( returnValue == false )
	{
		
		STATUS_LOG ( ("%s: start failed.  status = %x\n", getName ( ), status) );

		// call the cleanUp method to clean up any allocated resources.
		cleanUp ( );
		
		// close SBP2 if we have opened it.
		if ( fSBPTarget && openSucceeded )
		{
			
			fSBPTarget->close ( this );
			fSBPTarget = NULL;
		
		}
	
	}

	return returnValue;

}

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::cleanUp ( )
{

	STATUS_LOG ( ("%s: cleanUp called\n", getName ( ) ) );

    if ( fSBPTarget )
    {

        DeallocateResources ( );

    }

}

//------------------------------------------------------------------------------

IOFireWireSBP2ORB *
IOFireWireSerialBusProtocolTransport::CommandORBAccessor ( void )
{

	return fORB;

}

//------------------------------------------------------------------------------

IOFireWireSBP2Login *
IOFireWireSerialBusProtocolTransport::SBP2LoginAccessor ( void )
{

	return fLogin;

}

//------------------------------------------------------------------------------

IOReturn
IOFireWireSerialBusProtocolTransport::message (	UInt32 type,
												IOService *nub,
												void *arg )
{

	SBP2ClientOrbData *clientData = NULL;
	IOReturn status;

	switch ( type )
	{
		case kIOMessageServiceIsSuspended:
			STATUS_LOG ( ("%s: kIOMessageServiceIsSuspended\n", getName ( ) ) );

			fLoggedIn = false;
						
			status = kIOReturnSuccess;
			break;

		case kIOMessageServiceIsResumed:
			STATUS_LOG ( ("%s: kIOMessageServiceIsResumed\n", getName ( ) ) );

			fPhysicallyConnected = true;

			if ( fNeedLogin )
			{
				STATUS_LOG ( ("%s: fNeedLogin submitLogin\n", getName ( ) ) );

				fNeedLogin = false;
				
				// in case we are resumed after a terminate - check fLogin
				if ( fLogin ) fLogin->submitLogin ( );
			}
			
			status = kIOReturnSuccess;
			break;

		case kIOMessageFWSBP2ReconnectComplete:
			STATUS_LOG ( ("%s: kIOMessageFWSBP2ReconnectComplete\n", getName ( ) ) );
			
			fLoggedIn = true;

			clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ( );
			if ( clientData != NULL )
			{

				if ( clientData->scsiTask && ( fReconnectCount < kMaxReconnectCount ) )
				{

					STATUS_LOG ( ( "%s: resubmit orb \n", getName ( ) ) );
					fReconnectCount++;
					fLogin->submitORB ( fORB );

				}
				else
				{
					/*
					 	we are unable to recover from bus reset storm
						we have exhausted the fReconnectCount - punt...
					*/
					
					clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus = kSCSITaskStatus_No_Status;
					
					CompleteSCSITask ( fORB );

				}

			}

			status = kIOReturnSuccess;
			break;

		case kIOMessageFWSBP2ReconnectFailed:
			STATUS_LOG ( ("%s: kIOMessageFWSBP2ReconnectFailed\n", getName ( ) ) );
			
			if ( fPhysicallyConnected )
			{

				fLogin->submitLogin( );
			}
			else
			{

				fNeedLogin = true;

			}

			status = kIOReturnSuccess;
			break;

		case kIOFWMessageServiceIsRequestingClose:
			STATUS_LOG ( ("%s: kIOMessageServiceIsRequestingClose\n", getName ( ) ) );

			// tell our super to message it's clients that the device is gone
            SendNotification_DeviceRemoved ( );
        
			clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ( );
			if ( clientData != NULL )
			{

				if ( clientData->scsiTask )
				{
					/* 
						we are unable to recover from bus reset storm
						we have exhausted the fReconnectCount - punt...
					*/
					
					clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus = kSCSITaskStatus_No_Status;
					
					CompleteSCSITask ( fORB );

				}
                
			}

            // let go of memory and what not
            cleanUp ( );

            // close SBP2 and allow termination to continue
            if ( fSBPTarget ) fSBPTarget->close ( this );
            
            // zero out our provider reference
            fSBPTarget = NULL;

			status = kIOReturnSuccess;
			break;

		case kIOMessageServiceIsTerminated:
			STATUS_LOG ( ("%s: kIOMessageServiceIsTerminated\n", getName ( ) ) );

			status = kIOReturnSuccess;
			break;

		default:

			status = IOService::message (type, nub, arg);
			break;
	}

	return status;

}

//------------------------------------------------------------------------------

bool
IOFireWireSerialBusProtocolTransport::SendSCSICommand (	SCSITaskIdentifier request,
														SCSIServiceResponse *serviceResponse,
														SCSITaskStatus *taskStatus )
{
	SBP2ClientOrbData *clientData = NULL;
	SCSICommandDescriptorBlock cdb;
	UInt8 commandLength;
	UInt32 commandFlags;
	UInt32 timeOut;
	bool commandProcessed = true;

	STATUS_LOG ( ("%s: SendSCSICommand called\n", getName ( ) ) );

	*serviceResponse = kSCSIServiceResponse_Request_In_Process;
	*taskStatus = kSCSITaskStatus_No_Status;

	if ( isInactive ( ) )
	{
		// device is disconnected - we can not service command request
		*serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
        commandProcessed = false;
		goto ErrorExit;

	}

	clientData = ( SBP2ClientOrbData * ) fORB->getRefCon( );
	assert ( clientData != NULL );
	
	if ( clientData->scsiTask )
	{
	
		/*
		 	we're busy - return false - command will be resent next time
		 	CommandComplete is called
		 */
		
		commandProcessed = false;
		goto ErrorExit;
	}

	GetCommandDescriptorBlock ( request, &cdb );
	commandLength = GetCommandDescriptorBlockSize ( request );
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

	fReconnectCount = 0;
	commandFlags =	GetDataTransferDirection( request ) == kSCSIDataTransfer_FromTargetToInitiator ? 
					kFWSBP2CommandTransferDataFromTarget : 0L;

	fORB->setCommandFlags (	commandFlags |
							kFWSBP2CommandCompleteNotify |
							kFWSBP2CommandImmediate |
							kFWSBP2CommandNormalORB );


	clientData->scsiTask = request;

	SetCommandBuffers( fORB, request );
	
	fORB->setCommandBlock ( cdb, commandLength );
	
    /*
	 	SBP-2 needs a non-zero timeout to fire completion routines
	 	if timeout is not expressed default to 30 seconds.
	 */
	
	timeOut = GetTimeoutDuration ( request );
    
    if ( timeOut == 0 ) timeOut = 0xFFFFFFFF;
    
	fORB->setCommandTimeout ( timeOut );

	if ( fLoggedIn )
    {
        
        fLogin->submitORB ( fORB );
        
    }

ErrorExit:

	STATUS_LOG ( ("%s: SendSCSICommand exit, Service Response = %x\n", getName ( ), *serviceResponse) );

	return commandProcessed;

}

//------------------------------------------------------------------------------

IOReturn
IOFireWireSerialBusProtocolTransport::SetCommandBuffers (	IOFireWireSBP2ORB *orb,
															SCSITaskIdentifier request )
{

	return orb->setCommandBuffers (	GetDataBuffer( request ),
								GetDataBufferOffset( request ),
								GetRequestedDataTransferCount( request ) );

}

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::CompleteSCSITask ( IOFireWireSBP2ORB *orb )
{
	SBP2ClientOrbData *clientData = NULL;

	STATUS_LOG ( ("%s: CompleteSCSITask called\n", getName ( ) ) );

	IOSimpleLockLock( fQueueLock );

	clientData = ( SBP2ClientOrbData * ) orb->getRefCon( );
	if ( clientData != NULL )
	{

		if ( clientData->scsiTask )
		{
			SCSITaskIdentifier scsiTask;
			IOByteCount	bytesTransfered = 0;

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
			
			// rentrancy protection
			scsiTask = clientData->scsiTask;
			clientData->scsiTask = NULL;

            IOSimpleLockUnlock( fQueueLock );

			CommandCompleted (	scsiTask,
								clientData->serviceResponse,
								clientData->taskStatus );

		}
        else
        {
        
            IOSimpleLockUnlock( fQueueLock );

        }

	}
    else
    {
    
        IOSimpleLockUnlock( fQueueLock );

    }

}

//------------------------------------------------------------------------------

SCSIServiceResponse
IOFireWireSerialBusProtocolTransport::AbortSCSICommand ( SCSITaskIdentifier request )
{

	SCSIServiceResponse status = kSCSIServiceResponse_FUNCTION_REJECTED;

	STATUS_LOG ( ("%s: AbortSCSICommand called\n", getName ( ) ) );
	if ( request == NULL )
	{

		STATUS_LOG ( ("%s: AbortSCSICommand called with a NULL request\n", getName ( ) ) );

	}


	return status;

}

//------------------------------------------------------------------------------

bool
IOFireWireSerialBusProtocolTransport::IsProtocolServiceSupported ( SCSIProtocolFeature feature,
																   void *serviceValue )
{

	bool	isSupported = false;
	
	STATUS_LOG ( ( "IOFireWireSerialBusProtocolTransport::IsProtocolServiceSupported called\n" ) );
	
	switch ( feature )
	{

        case kSCSIProtocolFeature_CPUInDiskMode:
            
			isSupported = IsDeviceCPUInDiskMode ( );
            
			break;
			
		case kSCSIProtocolFeature_GetMaximumLogicalUnitNumber:
			
			* ( UInt32 * ) serviceValue =  kMaxFireWireLUN;
			isSupported = true;
			
			break;
			
		default:

			break;
		
	}
	
	return isSupported;

}

//------------------------------------------------------------------------------

bool
IOFireWireSerialBusProtocolTransport::HandleProtocolServiceFeature ( 
												SCSIProtocolFeature feature, 
												void *serviceValue )
{

	return false;

}

//------------------------------------------------------------------------------

bool
IOFireWireSerialBusProtocolTransport::IsDeviceCPUInDiskMode ( void )
{

	UInt32 csrModelInfo = 0;
	IOConfigDirectory *directory;
	IOFireWireDevice *device;
	IOReturn status = kIOReturnSuccess;
	IOService *providerService = fUnit->getProvider( );
	bool isCPUDiskMode = false;

	STATUS_LOG ( ("%s: IsDeviceCPUInDiskMode was called\n", getName ( ) ) );

	if ( providerService == NULL )
	{

		status = kIOReturnError;

	}

	if ( status == kIOReturnSuccess )
	{

		device = OSDynamicCast ( IOFireWireDevice, providerService );
		if ( device == NULL )
		{

			status = kIOReturnError;

		}

	}

	if ( status == kIOReturnSuccess )
	{

		status = device->getConfigDirectory ( directory );
	}

	if ( status == kIOReturnSuccess )
	{

		status = directory->getKeyValue ( kCSRModelInfoKey, csrModelInfo );
	
	}

	if ( status == kIOReturnSuccess )
	{

		if ( ( csrModelInfo & 0x00FFFFFF ) == 0x0054444D ) isCPUDiskMode = true;

	}

	STATUS_LOG ( ("%s: CPU Disk Mode = %d\n", getName ( ), isCPUDiskMode ) );

	return isCPUDiskMode;

}

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::StatusNotifyStatic (	void *refCon,
															FWSBP2NotifyParamsPtr params )
{

	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->StatusNotify ( params );

}

//------------------------------------------------------------------------------

void 
IOFireWireSerialBusProtocolTransport::StatusNotify ( FWSBP2NotifyParams *params )
{

	IOFireWireSBP2ORB *orb = 0;
	FWSBP2StatusBlock *statusBlock = 0;
	SBP2ClientOrbData *clientData = 0;
	SCSI_Sense_Data targetData;
				
	if ( ( params->message != NULL ) && params->length )
	{
		orb = ( IOFireWireSBP2ORB * ) params->commandObject;
		statusBlock = ( FWSBP2StatusBlock * ) params->message;
		
		if( orb )
		{
			
			clientData = ( SBP2ClientOrbData * ) orb->getRefCon( );
		
		}
	
	}		

	switch( params->notificationEvent )
	{

		case kFWSBP2NormalCommandStatus:
			
			/*
				read the status block detail bits see SBP-2 spec section 5.3
				check the dead bit ( is 'd' field == 1 )
			*/
			
			if ( clientData && ( statusBlock->details & 0x08 ) ) 	
			{
				SetValidAutoSenseData ( clientData, statusBlock, &targetData );
				
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
				clientData->taskStatus = kSCSITaskStatus_GOOD;

				CompleteSCSITask ( orb );

				STATUS_LOG ( ("%s: StatusNotify normal complete \n", getName ( ) ) );
			
			}
			else if ( clientData )
			{

				SetValidAutoSenseData ( clientData, statusBlock, &targetData );

				CompleteSCSITask ( orb );
				
				STATUS_LOG ( ("%s: StatusNotify unexpected error? \n", getName ( ) ) );

			}

			break;

		case kFWSBP2NormalCommandTimeout:
			STATUS_LOG ( ("%s: kFWSBP2NormalCommandTimeout \n", getName ( ) ) );

			if ( clientData )
			{

				if ( clientData->scsiTask )
				{

					clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus = kSCSITaskStatus_No_Status;

					CompleteSCSITask ( orb );

				}

			}

			// reset LUN as good measure in case device is wedged
			fLUNResetORB->submit ( );

			break;

		case kFWSBP2NormalCommandReset:
			STATUS_LOG ( ("%s: kFWSBP2NormalCommandReset\n", getName ( ) ) );

            /*
				kFWSBP2NormalCommandReset - is a misleading definition
				A pending command has failed so we need notify
				the upper layers to complete failed command.
			*/
			
			if ( clientData )
			{

				if ( clientData->scsiTask )
				{

					clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
					clientData->taskStatus = kSCSITaskStatus_No_Status;

					CompleteSCSITask ( orb );

				}

			}

			break;

		default:
			STATUS_LOG ( ("%s: StatusNotify with unknown notificationEvent\n", getName ( ) ) );
			
			break;
	}

}

//------------------------------------------------------------------------------

void 
IOFireWireSerialBusProtocolTransport::SetValidAutoSenseData (	SBP2ClientOrbData *clientData,
																FWSBP2StatusBlock *statusBlock,
																SCSI_Sense_Data *targetData )
{
	UInt8 quadletCount = 0;

	clientData->serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	clientData->taskStatus = kSCSITaskStatus_No_Status;
	
	quadletCount = ( statusBlock->details & 0x07 ) - 1 ;

	// see if we have any valid sense data
	if ( ( statusBlock->details & 0x30 ) == 0 )
	{

		clientData->serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
		clientData->taskStatus = CoalesceSenseData ( statusBlock, quadletCount, targetData );

	}
	
	if ( clientData->taskStatus ==  kSCSITaskStatus_CHECK_CONDITION )
	{

		if ( clientData->scsiTask )
		{

			SetAutoSenseData( clientData->scsiTask, targetData );
 
		}

	}
}

//------------------------------------------------------------------------------

SCSITaskStatus
IOFireWireSerialBusProtocolTransport::CoalesceSenseData (	FWSBP2StatusBlock *sourceData,
															UInt8 quadletCount,
															SCSI_Sense_Data *targetData )
{
	SCSITaskStatus returnValue = kSCSITaskStatus_GOOD;
	UInt8 statusBlockFormat;

	/*
		pull bits out of SBP-2 status block ( see SBP-2 Annex B section B.2 )
		and copy them into sense data block ( see SPC-2 section 7.23.2 )
	*/
	
	bzero ( targetData, sizeof ( SCSI_Sense_Data ) );

	if( quadletCount > 0 )
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
			targetData->SENSE_KEY |= (( sourceData->status[0] >> 16 ) & 0x70) << 1;

			if( quadletCount > 1 )
			{
				
				targetData->INFORMATION_1 = ( sourceData->status[1] >> 24 ) & 0xFF;
				targetData->INFORMATION_2 = ( sourceData->status[1] >> 16 ) & 0xFF;
				targetData->INFORMATION_3 = ( sourceData->status[1] >> 8 ) & 0xFF;
				targetData->INFORMATION_4 = sourceData->status[1] & 0xFF;
			
			}
		
			if( quadletCount > 2 )
			{
				
				targetData->COMMAND_SPECIFIC_INFORMATION_1 = ( sourceData->status[2] >> 24 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_2 = ( sourceData->status[2] >> 16 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_3 = ( sourceData->status[2] >> 8 ) & 0xFF;
				targetData->COMMAND_SPECIFIC_INFORMATION_4 = sourceData->status[2] & 0xFF;
			
			}

			if( quadletCount > 3 )
			{

				targetData->FIELD_REPLACEABLE_UNIT_CODE = ( sourceData->status[3] >> 24 ) & 0xFF;
				targetData->SKSV_SENSE_KEY_SPECIFIC_MSB = ( sourceData->status[3] >> 16 ) & 0xFF;
				targetData->SENSE_KEY_SPECIFIC_MID = ( sourceData->status[3] >> 8 ) & 0xFF;
				targetData->SENSE_KEY_SPECIFIC_LSB = sourceData->status[3] & 0xFF;

			}

			// for now truncate additional SCSI_Sense_Data length ?
			targetData->ADDITIONAL_SENSE_LENGTH =  kSenseDefaultSize - 0x07;

		}

	}

	return returnValue;

}

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::LoginCompletionStatic (	void *refCon,
																FWSBP2LoginCompleteParams *params )
{

	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->LoginCompletion( params );

}

//------------------------------------------------------------------------------

void 
IOFireWireSerialBusProtocolTransport::LoginCompletion (	FWSBP2LoginCompleteParams *params )
{

	SBP2ClientOrbData *clientData = NULL;

	STATUS_LOG ( ("%s: LoginCompletion complete \n", getName ( ) ) );

	if	( ( params->status == kIOReturnSuccess ) &&				// ( kIOReturnSuccess )
        ( ( params->statusBlock->details & 0x30 ) == 0 ) &&		// ( is 'resp' field == 0 )
        ( params->statusBlock->sbpStatus == 0 ) )				// ( is 'sbp_status' field == 0 )
	{

		fLoginRetryCount = 0;
		fLoggedIn = true;

		if ( fDeferRegisterService )
		{
		
			registerService ( );
			fDeferRegisterService = false;
		
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
	
		if( fPhysicallyConnected )
		{

			if ( fLoginRetryCount < kMaxLoginRetryCount )
			{

				fLoginRetryCount++;
				STATUS_LOG ( ("%s: resubmitting Login\n", getName ( ) ) );

				fLogin->submitLogin ( );

			}
			else
			{

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

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::LogoutCompletionStatic (	void *refCon,
																FWSBP2LogoutCompleteParams *params )
{

	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->LogoutCompletion ( params );

}

//------------------------------------------------------------------------------

void 
IOFireWireSerialBusProtocolTransport::LogoutCompletion ( FWSBP2LogoutCompleteParams *params )
{
	STATUS_LOG ( ("%s: LogoutCompletion complete \n", getName ( ) ) );

}

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::UnsolicitedStatusNotifyStatic (	void *refCon,
																FWSBP2NotifyParamsPtr params )
{

	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->UnsolicitedStatusNotify ( params );

}

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::UnsolicitedStatusNotify ( FWSBP2NotifyParamsPtr params )
{

	STATUS_LOG ( ("%s: UnsolicitedStatusNotify called\n", getName ( ) ) );

    // parse and handle unsolicited status
	fLogin->enableUnsolicitedStatus ( );

}

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::FetchAgentResetCompleteStatic ( void *refCon,
																	  IOReturn status )
{

	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->FetchAgentResetComplete ( status );

}

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::FetchAgentResetComplete (	IOReturn status )
{

	SBP2ClientOrbData *clientData = NULL;

	STATUS_LOG ( ("%s: FetchAgentResetComplete called\n", getName ( ) ) );

    /*
		When orb chaining is implemented we will notify upper layer
		to reconfigure device state and resubmitting commands
	*/

	clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ( );
	if ( clientData != NULL )
	{

		if ( clientData->scsiTask )
		{

			CompleteSCSITask ( fORB );

		}

	}

}

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::LunResetCompleteStatic ( void *refCon,
															   IOReturn status,
															   IOFireWireSBP2ManagementORB *orb)
{

	( ( IOFireWireSerialBusProtocolTransport * ) refCon )->LunResetComplete (	status,
																				orb );

}

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::LunResetComplete ( IOReturn status,
														 IOFireWireSBP2ManagementORB *orb)
{

	STATUS_LOG ( ("%s: LunResetComplete called\n", getName ( ) ) );

	fLogin->submitFetchAgentReset ( );

}

//------------------------------------------------------------------------------

void 
IOFireWireSerialBusProtocolTransport::ConnectToDevice ( void )
{

	STATUS_LOG ( ("%s: ConnectToDevice called\n", getName ( ) ) );
	
	// avoid double logins during login phase
	fNeedLogin = false;

	fLogin->submitLogin ( );

}

//------------------------------------------------------------------------------

void 
IOFireWireSerialBusProtocolTransport::DisconnectFromDevice ( void )
{

	STATUS_LOG ( ("%s: DisconnectFromDevice called\n", getName ( ) ) );

	fLoggedIn = false;

	// avoid logins during a logout phase
	fNeedLogin = false;
	
	if( fPhysicallyConnected )
	{

		fLogin->submitLogout ( );

	}

}

//------------------------------------------------------------------------------

IOReturn
IOFireWireSerialBusProtocolTransport::AllocateResources ( void )
{

	SBP2ClientOrbData *clientData = NULL;
	IOReturn status = kIOReturnNoMemory;
	
	STATUS_LOG ( ("%s: AllocateResources called\n", getName ( ) ) );

	// allocate mutex for double completion race
	fQueueLock = IOSimpleLockAlloc ( );
	if ( fQueueLock == NULL ) goto ErrorExit;

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

	status = kIOReturnSuccess;

ErrorExit:

	return status;

}

//------------------------------------------------------------------------------

void
IOFireWireSerialBusProtocolTransport::DeallocateResources ( void )
{

	SBP2ClientOrbData *clientData = NULL;

	STATUS_LOG ( ("%s: DeallocateResources called\n", getName ( ) ) );

	/*

		/!\ WARNING - always release orb's before logins

	*/

	if ( fLUNResetORB )
	{
		
		fLUNResetORB->release ( );
		fLUNResetORB = NULL;
	
	}
	
	if ( fORB )
    {

        clientData = ( SBP2ClientOrbData * ) fORB->getRefCon ( );
        if ( clientData != NULL )
        {
    
            IOFree ( clientData, sizeof ( SBP2ClientOrbData ) );
    
        }

        fORB->release ( );
        fORB = NULL;

	}

	if ( fQueueLock != NULL )
	{

		IOSimpleLockFree( fQueueLock );
		fQueueLock = NULL;

	}

	if ( fLogin )
    {
        
        fLogin->release ( );
        fLogin = NULL;
	
    }
    
}

//------------------------------------------------------------------------------
